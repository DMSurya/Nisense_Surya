import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../protocol/hcm_protocol.dart';

/// BLE client for the glucose-model transfer service (...def3).
///
/// Streams a packed model image into the device's *inactive* A/B slot, then
/// commits (with optional Ed25519 signature). Mirrors the firmware state machine
/// in src/ble/ble_model_transfer.c.
class ModelTransferClient {
  ModelTransferClient(this.device);

  final BluetoothDevice device;

  BluetoothCharacteristic? _ctrl;
  BluetoothCharacteristic? _data;
  BluetoothCharacteristic? _status;

  Future<void> _resolve() async {
    if (_ctrl != null && _data != null && _status != null) return;
    final services = await device.discoverServices();
    for (final s in services) {
      if (s.uuid.toString().toLowerCase() != svcModelTransfer.toLowerCase()) continue;
      for (final c in s.characteristics) {
        final u = c.uuid.toString().toLowerCase();
        if (u == chrcModelCtrl.toLowerCase()) _ctrl = c;
        if (u == chrcModelData.toLowerCase()) _data = c;
        if (u == chrcModelStatus.toLowerCase()) _status = c;
      }
    }
    if (_ctrl == null || _data == null || _status == null) {
      throw StateError('Model transfer service not found on device');
    }
  }

  Future<ModelStatus> readStatus() async {
    final bytes = await _status!.read();
    return ModelStatus.parse(Uint8List.fromList(bytes));
  }

  Future<ModelStatus> _waitFor(int state, {Duration timeout = const Duration(seconds: 20)}) async {
    final deadline = DateTime.now().add(timeout);
    while (DateTime.now().isBefore(deadline)) {
      final st = await readStatus();
      if (st.state == ModelStatus.stateError) {
        throw StateError('Device reported model error (${st.error})');
      }
      if (st.state == state) return st;
      await Future.delayed(const Duration(milliseconds: 150));
    }
    throw TimeoutException('Timed out waiting for model state $state');
  }

  /// Upload [image] (packed glucose_model.bin) and commit it.
  Future<void> upload(
    List<int> image, {
    required int version,
    required int variant, // 0=wearable, 1=pulse
    String? signatureB64,
    void Function(double progress)? onProgress,
  }) async {
    await _resolve();

    // BEGIN: [cmd][total u32][version u32][variant u8]
    final begin = ByteData(10);
    begin.setUint8(0, modelCmdBegin);
    begin.setUint32(1, image.length, Endian.little);
    begin.setUint32(5, version, Endian.little);
    begin.setUint8(9, variant);
    await _ctrl!.write(begin.buffer.asUint8List(), withoutResponse: false);
    await _waitFor(ModelStatus.stateReceiving);

    // DATA: chunk by negotiated MTU (leave 3 bytes ATT overhead).
    final mtu = device.mtuNow;
    final chunk = (mtu > 23 ? mtu - 3 : 180).clamp(20, 244);
    for (var off = 0; off < image.length; off += chunk) {
      final end = (off + chunk < image.length) ? off + chunk : image.length;
      await _data!.write(image.sublist(off, end), withoutResponse: true);
      onProgress?.call(end / image.length);
    }
    await _waitFor(ModelStatus.stateReady);

    // COMMIT: [cmd][sig_len u8][sig...]
    final sig = (signatureB64 != null && signatureB64.isNotEmpty)
        ? base64Decode(signatureB64)
        : Uint8List(0);
    final commit = BytesBuilder()
      ..addByte(modelCmdCommit)
      ..addByte(sig.length)
      ..add(sig);
    await _ctrl!.write(commit.toBytes(), withoutResponse: false);
    await _waitFor(ModelStatus.stateCommitted, timeout: const Duration(seconds: 15));
  }

  Future<void> abort() async {
    await _resolve();
    await _ctrl!.write([modelCmdAbort], withoutResponse: false);
  }
}

/// Parsed status characteristic (matches struct ble_model_status).
class ModelStatus {
  ModelStatus(this.state, this.error, this.received, this.total,
      this.activeVersion, this.activeSlot);

  static const stateIdle = 0;
  static const stateErasing = 1;
  static const stateReceiving = 2;
  static const stateReady = 3;
  static const stateCommitted = 4;
  static const stateError = 5;

  final int state;
  final int error;
  final int received;
  final int total;
  final int activeVersion;
  final int activeSlot;

  static ModelStatus parse(Uint8List b) {
    final d = ByteData.sublistView(b);
    if (b.length < 15) {
      return ModelStatus(stateError, -1, 0, 0, 0, 0);
    }
    return ModelStatus(
      d.getUint8(0),
      d.getInt8(1),
      d.getUint32(2, Endian.little),
      d.getUint32(6, Endian.little),
      d.getUint32(10, Endian.little),
      d.getUint8(14),
    );
  }
}
