import 'dart:async';
import 'dart:typed_data';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../protocol/hcm_protocol.dart';

/// BLE client for the Resource store transfer service (...def5).
///
/// Mirrors firmware `src/ble/ble_resource_transfer.c`: BEGIN → DATA chunks → COMMIT.
class ResourceTransferClient {
  ResourceTransferClient(this.device);

  final BluetoothDevice device;

  BluetoothCharacteristic? _ctrl;
  BluetoothCharacteristic? _data;
  BluetoothCharacteristic? _status;

  Future<void> _resolve() async {
    if (_ctrl != null && _data != null && _status != null) return;
    final services = await device.discoverServices();
    for (final s in services) {
      if (s.uuid.toString().toLowerCase() != svcResourceTransfer.toLowerCase()) continue;
      for (final c in s.characteristics) {
        final u = c.uuid.toString().toLowerCase();
        if (u == chrcResourceCtrl.toLowerCase()) _ctrl = c;
        if (u == chrcResourceData.toLowerCase()) _data = c;
        if (u == chrcResourceStatus.toLowerCase()) _status = c;
      }
    }
    if (_ctrl == null || _data == null || _status == null) {
      throw StateError('Resource transfer service not found on device');
    }
  }

  Future<ResourceTransferStatus> readStatus() async {
    final bytes = await _status!.read();
    return ResourceTransferStatus.parse(Uint8List.fromList(bytes));
  }

  Future<ResourceTransferStatus> _waitFor(int state, {Duration timeout = const Duration(seconds: 30)}) async {
    final deadline = DateTime.now().add(timeout);
    while (DateTime.now().isBefore(deadline)) {
      final st = await readStatus();
      if (st.state == ResourceTransferStatus.stateError) {
        throw StateError('Device reported Resource error (${st.error})');
      }
      if (st.state == state) return st;
      await Future.delayed(const Duration(milliseconds: 150));
    }
    throw TimeoutException('Timed out waiting for Resource state $state');
  }

  Future<void> upload(
    List<int> image, {
    void Function(double progress)? onProgress,
  }) async {
    await _resolve();

    final begin = ByteData(5);
    begin.setUint8(0, resourceCmdBegin);
    begin.setUint32(1, image.length, Endian.little);
    await _ctrl!.write(begin.buffer.asUint8List(), withoutResponse: false);
    await _waitFor(ResourceTransferStatus.stateReceiving);

    final mtu = device.mtuNow;
    final chunk = (mtu > 23 ? mtu - 3 : 180).clamp(20, 244);
    for (var off = 0; off < image.length; off += chunk) {
      final end = (off + chunk < image.length) ? off + chunk : image.length;
      await _data!.write(image.sublist(off, end), withoutResponse: true);
      onProgress?.call(end / image.length);
    }
    await _waitFor(ResourceTransferStatus.stateReady, timeout: const Duration(seconds: 60));

    await _ctrl!.write([resourceCmdCommit], withoutResponse: false);
    await _waitFor(ResourceTransferStatus.stateCommitted, timeout: const Duration(seconds: 30));
  }

  Future<void> abort() async {
    await _resolve();
    await _ctrl!.write([resourceCmdAbort], withoutResponse: false);
  }
}

class ResourceTransferStatus {
  ResourceTransferStatus(this.state, this.error, this.received, this.total,
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

  static ResourceTransferStatus parse(Uint8List b) {
    final d = ByteData.sublistView(b);
    if (b.length < 15) {
      return ResourceTransferStatus(stateError, -1, 0, 0, 0, 0);
    }
    return ResourceTransferStatus(
      d.getUint8(0),
      d.getInt8(1),
      d.getUint32(2, Endian.little),
      d.getUint32(6, Endian.little),
      d.getUint32(10, Endian.little),
      d.getUint8(14),
    );
  }
}
