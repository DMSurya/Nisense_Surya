import 'dart:async';
import 'dart:io';
import 'dart:math' as math;

import 'package:cbor/cbor.dart';
import 'package:crypto/crypto.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../protocol/hcm_protocol.dart';
import '../util/app_log.dart';

/// Must stay under firmware `CONFIG_MCUMGR_TRANSPORT_NETBUF_SIZE` (1536)
/// after the 8-byte SMP header and CBOR map overhead.
///
/// First upload frame also carries `len` + SHA-256 → leave more headroom.
const _maxImageDataFirst = 1200;
const _maxImageDataNext = 1400;

/// SMP-over-BLE DFU client (MCUmgr image + OS groups).
///
/// Throughput path:
/// 1. Request high connection priority + 2M PHY (Android).
/// 2. Build large SMP frames (~1.2–1.4 KB image data).
/// 3. Fragment across ATT Write-Without-Response packets (mtu−3).
/// 4. Wait once for the SMP Notification reply.
class SmpClient {
  SmpClient(this._device);

  final BluetoothDevice _device;
  int _seq = 0;
  final List<int> _rxBuf = [];
  Completer<Uint8List>? _pending;
  StreamSubscription<List<int>>? _sub;
  BluetoothCharacteristic? _char;

  Future<void> _setup() async {
    final services = await _device.discoverServices();
    for (final svc in services) {
      if (svc.uuid.toString().toLowerCase() != svcSmp) continue;
      for (final ch in svc.characteristics) {
        if (ch.uuid.toString().toLowerCase() == chrcSmp) {
          _char = ch;
          await ch.setNotifyValue(true);
          _sub = ch.onValueReceived.listen(_onNotify);
          _device.cancelWhenDisconnected(_sub!);
          return;
        }
      }
    }
    throw StateError('SMP characteristic not found');
  }

  Future<void> _teardown() async {
    await _sub?.cancel();
    _sub = null;
    final ch = _char;
    if (ch != null) {
      try {
        await ch.setNotifyValue(false);
      } catch (_) {}
    }
  }

  /// Best-effort radio speedups for the DFU session (Android only).
  Future<void> _boostLink() async {
    if (kIsWeb || !Platform.isAndroid) return;
    try {
      await _device.requestConnectionPriority(
        connectionPriorityRequest: ConnectionPriority.high,
      );
      AppLog.i('dfu', 'connection priority → high');
    } catch (e) {
      AppLog.w('dfu', 'requestConnectionPriority failed', e);
    }
    try {
      await _device.setPreferredPhy(
        txPhy: Phy.le1m.mask | Phy.le2m.mask,
        rxPhy: Phy.le1m.mask | Phy.le2m.mask,
        option: PhyCoding.noPreferred,
      );
      AppLog.i('dfu', 'preferred PHY → 1M|2M');
    } catch (e) {
      AppLog.w('dfu', 'setPreferredPhy failed', e);
    }
  }

  void _onNotify(List<int> chunk) {
    _rxBuf.addAll(chunk);
    if (_rxBuf.length < 8) return;
    final len = (_rxBuf[2] << 8) | _rxBuf[3];
    if (_rxBuf.length < 8 + len) return;
    final frame = Uint8List.fromList(_rxBuf.sublist(0, 8 + len));
    _rxBuf.removeRange(0, 8 + len);
    _pending?.complete(frame);
    _pending = null;
  }

  int get _attPayload {
    final mtu = _device.mtuNow;
    return (mtu > 23 ? mtu - 3 : 20).clamp(20, 244);
  }

  /// Write a full SMP frame, fragmenting across Write-Without-Response ATT PDUs.
  Future<void> _writeSmpFrame(Uint8List frame) async {
    final att = _attPayload;
    for (var off = 0; off < frame.length; off += att) {
      final end = math.min(off + att, frame.length);
      await _char!.write(frame.sublist(off, end), withoutResponse: true);
    }
  }

  Future<Uint8List> _request(int op, int group, int id, CborValue? payload) async {
    final cborBytes = payload != null ? cbor.encode(payload) : <int>[];
    final header = Uint8List(8);
    header[0] = op;
    header[1] = 0;
    header[2] = (cborBytes.length >> 8) & 0xFF;
    header[3] = cborBytes.length & 0xFF;
    header[4] = (group >> 8) & 0xFF;
    header[5] = group & 0xFF;
    header[6] = _seq & 0xFF;
    header[7] = id & 0xFF;
    _seq = (_seq + 1) & 0xFF;

    final frame = Uint8List.fromList([...header, ...cborBytes]);
    _rxBuf.clear();
    _pending = Completer<Uint8List>();
    await _writeSmpFrame(frame);
    return _pending!.future.timeout(const Duration(seconds: 30));
  }

  Future<void> uploadImage(
    String path, {
    void Function(int sent, int total)? onProgress,
  }) async {
    await _setup();
    try {
      await _boostLink();

      // Ensure MTU is negotiated — larger ATT PDUs → fewer fragments per SMP frame.
      if (_device.mtuNow < 200) {
        try {
          await _device.requestMtu(247, predelay: 0.2, timeout: 10);
          AppLog.i('dfu', 'mtuNow=${_device.mtuNow}');
        } catch (e) {
          AppLog.w('dfu', 'requestMtu failed (mtuNow=${_device.mtuNow})', e);
        }
      }

      final bytes = await File(path).readAsBytes();
      final hash = Uint8List.fromList(sha256.convert(bytes).bytes);
      AppLog.i(
        'dfu',
        'upload start size=${bytes.length} att=$_attPayload '
        'chunkFirst=$_maxImageDataFirst chunkNext=$_maxImageDataNext',
      );

      var offset = 0;
      while (offset < bytes.length) {
        final maxData = offset == 0 ? _maxImageDataFirst : _maxImageDataNext;
        final end = math.min(offset + maxData, bytes.length);
        final chunk = bytes.sublist(offset, end);
        final payload = CborMap({
          CborString('data'): CborBytes(chunk),
          CborString('off'): CborSmallInt(offset),
          if (offset == 0) ...{
            CborString('len'): CborSmallInt(bytes.length),
            CborString('sha'): CborBytes(hash),
          },
        });
        await _request(2, 1, 1, payload);
        offset = end;
        onProgress?.call(offset, bytes.length);
      }
      AppLog.i('dfu', 'upload complete ${bytes.length} bytes');
    } finally {
      await _teardown();
    }
  }

  Future<void> setImagePending() async {
    await _setup();
    try {
      await _request(
        2,
        1,
        0,
        CborMap({CborString('confirm'): const CborBool(false)}),
      );
    } finally {
      await _teardown();
    }
  }

  Future<void> resetDevice() async {
    await _setup();
    try {
      await _request(2, 0, 5, null);
    } finally {
      await _teardown();
    }
  }
}
