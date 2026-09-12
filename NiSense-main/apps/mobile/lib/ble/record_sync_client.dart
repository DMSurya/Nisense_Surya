import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../protocol/hcm_protocol.dart';
import '../util/app_log.dart';

/// One reassembled NOR record from the device record-sync service.
class SyncedRecord {
  SyncedRecord({
    required this.recordId,
    required this.type,
    required this.parentId,
    required this.measurementId,
    required this.payload,
  });

  final int recordId;
  final int type;
  final int parentId;

  /// Shared health Measure cycle id (0 = idle / ungrouped / schema v1).
  final int measurementId;
  final Uint8List payload;
}

class RecordSyncStatus {
  RecordSyncStatus({
    required this.state,
    required this.error,
    required this.pending,
    required this.cursorId,
    required this.sentCount,
  });

  final int state;
  final int error;

  /// Total un-ACKed records still in device NOR (not "records in this transfer").
  final int pending;
  final int cursorId;

  /// Records the device has finished sending in the current START session.
  final int sentCount;

  static const stateIdle = 0;
  static const stateSending = 1;
  static const stateDone = 2;
  static const stateError = 3;

  static RecordSyncStatus parse(Uint8List b) {
    if (b.length < 14) {
      throw FormatException('record sync status too short (${b.length})');
    }
    final bd = ByteData.sublistView(b);
    return RecordSyncStatus(
      state: b[0],
      error: bd.getInt8(1),
      pending: bd.getUint32(2, Endian.little),
      cursorId: bd.getUint32(6, Endian.little),
      sentCount: bd.getUint32(10, Endian.little),
    );
  }
}

/// BLE client for NOR record bulk pull (...def4).
class RecordSyncClient {
  RecordSyncClient(this.device);

  final BluetoothDevice device;

  BluetoothCharacteristic? _ctrl;
  BluetoothCharacteristic? _data;
  BluetoothCharacteristic? _status;

  StreamSubscription<List<int>>? _dataSub;
  StreamSubscription<List<int>>? _statusSub;
  final _frames = <SyncedRecord>[];
  final _seenIds = <int>{};
  final _frag = BytesBuilder(copy: false);
  int _fragTotal = 0;

  /// Status pushed by the device's notify (cheaper than an ATT read, which
  /// costs a request/response round trip that stalls the bulk notify stream).
  RecordSyncStatus? _pushedStatus;
  DateTime? _pushedStatusAt;

  Future<void> _resolve() async {
    if (_ctrl != null && _data != null && _status != null) return;
    final services = await device.discoverServices();
    for (final s in services) {
      if (s.uuid.toString().toLowerCase() != svcRecordSync.toLowerCase()) continue;
      for (final c in s.characteristics) {
        final u = c.uuid.toString().toLowerCase();
        if (u == chrcRecordCtrl.toLowerCase()) _ctrl = c;
        if (u == chrcRecordData.toLowerCase()) _data = c;
        if (u == chrcRecordStatus.toLowerCase()) _status = c;
      }
    }
    if (_ctrl == null || _data == null || _status == null) {
      throw StateError('Record sync service not found on device');
    }
  }

  Future<RecordSyncStatus> readStatus() async {
    await _resolve();
    final bytes = await _status!.read();
    return RecordSyncStatus.parse(Uint8List.fromList(bytes));
  }

  /// Device-pushed status if it is recent enough to trust, else null.
  RecordSyncStatus? _freshPushedStatus() {
    final at = _pushedStatusAt;
    if (at == null) return null;
    if (DateTime.now().difference(at) > const Duration(seconds: 2)) return null;
    return _pushedStatus;
  }

  /// Prefer the pushed status; fall back to a read when notifies go quiet.
  Future<RecordSyncStatus> _currentStatus() async =>
      _freshPushedStatus() ?? await readStatus();

  void _onStatusNotify(List<int> raw) {
    try {
      _pushedStatus = RecordSyncStatus.parse(Uint8List.fromList(raw));
      _pushedStatusAt = DateTime.now();
    } catch (_) {}
  }

  /// Best-effort radio speedups for the pull session (Android only; the
  /// firmware asks for 2M PHY and a tighter interval from its side too).
  Future<void> _boostLink() async {
    if (kIsWeb || !Platform.isAndroid) return;
    try {
      await device.requestConnectionPriority(
        connectionPriorityRequest: ConnectionPriority.high,
      );
    } catch (e) {
      AppLog.w('sync', 'requestConnectionPriority failed', e);
    }
    try {
      await device.setPreferredPhy(
        txPhy: Phy.le1m.mask | Phy.le2m.mask,
        rxPhy: Phy.le1m.mask | Phy.le2m.mask,
        option: PhyCoding.noPreferred,
      );
    } catch (e) {
      AppLog.w('sync', 'setPreferredPhy failed', e);
    }
  }

  /// Restore power-friendly link settings once the transfer is over.
  Future<void> _relaxLink() async {
    if (kIsWeb || !Platform.isAndroid) return;
    try {
      await device.requestConnectionPriority(
        connectionPriorityRequest: ConnectionPriority.balanced,
      );
    } catch (_) {}
  }

  /// Live count of unique fully assembled records during an active pull.
  int get receivedCount => _frames.length;

  void _onData(List<int> raw) {
    if (raw.length < 5) return;
    final bd = ByteData.sublistView(Uint8List.fromList(raw));
    final flags = raw[0];
    final offset = bd.getUint16(1, Endian.little);
    final total = bd.getUint16(3, Endian.little);
    final chunk = raw.sublist(5);

    if (offset == 0) {
      _frag.clear();
      _fragTotal = total;
    }
    if (offset != _frag.length) {
      // Drop out-of-order fragment; wait for a new frame start.
      if (offset == 0) {
        _frag.add(chunk);
      }
      return;
    }
    _frag.add(chunk);

    final more = (flags & 0x01) != 0;
    if (more) return;
    if (_frag.length != _fragTotal) return;

    final frame = _frag.takeBytes();
    if (frame.length < 12) return;
    final fbd = ByteData.sublistView(frame);
    final recordId = fbd.getUint32(0, Endian.little);
    final type = fbd.getUint16(4, Endian.little);
    final parentId = fbd.getUint32(6, Endian.little);

    // v2 hdr (16 B): … measurement_id @10, payload_len @14
    // v1 hdr (12 B): … payload_len @10
    int hdrSize;
    int measurementId;
    int payloadLen;
    final plenV2 = frame.length >= 16 ? fbd.getUint16(14, Endian.little) : -1;
    final plenV1 = fbd.getUint16(10, Endian.little);
    if (plenV2 >= 0 && frame.length == 16 + plenV2) {
      hdrSize = 16;
      measurementId = fbd.getUint32(10, Endian.little);
      payloadLen = plenV2;
    } else if (frame.length == 12 + plenV1) {
      hdrSize = 12;
      measurementId = 0;
      payloadLen = plenV1;
    } else {
      return;
    }

    // Deduplicate — BLE can deliver the same frame twice under load.
    if (!_seenIds.add(recordId)) return;

    _frames.add(SyncedRecord(
      recordId: recordId,
      type: type,
      parentId: parentId,
      measurementId: measurementId,
      payload: Uint8List.sublistView(frame, hdrSize, hdrSize + payloadLen),
    ));
  }

  Future<List<SyncedRecord>> pull({
    int afterId = 0,
    /// Default: clinical summaries only (fast). Use [recModeFull] for PPG/glucose raw.
    int mode = recModeSummaryOnly,
    /// Wall-clock ceiling for a single pull. Activity (rising sent/received)
    /// keeps the transfer alive until this absolute limit.
    Duration timeout = const Duration(minutes: 30),
    /// If neither sent_count nor assembled frames advance for this long while
    /// still SENDING, treat as stalled.
    Duration stallTimeout = const Duration(minutes: 2),
    void Function(RecordSyncStatus st)? onStatus,
  }) async {
    await _resolve();
    _frames.clear();
    _seenIds.clear();
    _frag.clear();
    _fragTotal = 0;
    _pushedStatus = null;
    _pushedStatusAt = null;

    await _boostLink();
    await _data!.setNotifyValue(true);
    await _status!.setNotifyValue(true);
    _dataSub?.cancel();
    _dataSub = _data!.onValueReceived.listen(_onData);
    device.cancelWhenDisconnected(_dataSub!);
    _statusSub?.cancel();
    _statusSub = _status!.onValueReceived.listen(_onStatusNotify);
    device.cancelWhenDisconnected(_statusSub!);

    final start = ByteData(6);
    start.setUint8(0, recCmdStart);
    start.setUint32(1, afterId, Endian.little);
    start.setUint8(5, mode);
    await _ctrl!.write(start.buffer.asUint8List(), withoutResponse: false);

    final absoluteDeadline = DateTime.now().add(timeout);
    var lastProgressAt = DateTime.now();
    var lastSent = 0;
    var lastReceived = 0;
    RecordSyncStatus? last;

    try {
      while (DateTime.now().isBefore(absoluteDeadline)) {
        final st = await _currentStatus();
        last = st;
        onStatus?.call(st);
        if (st.state == RecordSyncStatus.stateError) {
          throw StateError('Record sync error (${st.error})');
        }
        if (st.state == RecordSyncStatus.stateDone ||
            (st.state == RecordSyncStatus.stateIdle && st.sentCount > 0)) {
          // Status can go DONE while ATT notifies are still in flight — drain
          // until assembled unique frames catch up to the device's sent_count.
          await _drainUntilCaughtUp(st.sentCount, onStatus);
          break;
        }

        final received = _frames.length;
        if (st.sentCount > lastSent || received > lastReceived) {
          lastSent = st.sentCount;
          lastReceived = received;
          lastProgressAt = DateTime.now();
        } else if (st.state == RecordSyncStatus.stateSending &&
            DateTime.now().difference(lastProgressAt) > stallTimeout) {
          if (_frames.isNotEmpty) {
            // Partial pull — persist what we have; next Sync continues.
            break;
          }
          throw TimeoutException(
            'Record sync stalled after ${stallTimeout.inSeconds}s with no progress '
            '(sent=$lastSent received=$lastReceived)',
          );
        }

        await Future.delayed(const Duration(milliseconds: 200));
      }

      if (last != null &&
          last.state == RecordSyncStatus.stateSending &&
          DateTime.now().isAfter(absoluteDeadline)) {
        // Keep whatever arrived so the caller can persist + advance last_pulled;
        // next Sync resumes after that cursor instead of re-pulling from scratch.
        if (_frames.isNotEmpty) {
          return List<SyncedRecord>.from(_frames);
        }
        throw TimeoutException(
          'Record sync timed out after $timeout '
          '(sent=${last.sentCount} received=0)',
        );
      }

      return List<SyncedRecord>.from(_frames);
    } finally {
      await _relaxLink();
    }
  }

  /// Wait for late notifies after the device reports DONE.
  /// Extends while frames are still arriving; fails soft if still short.
  Future<void> _drainUntilCaughtUp(
    int sentCount,
    void Function(RecordSyncStatus st)? onStatus,
  ) async {
    if (sentCount <= 0) return;
    var lastCount = _frames.length;
    var idleRounds = 0;
    // Large FULL syncs can have many in-flight fragments after DONE.
    final drainDeadline = DateTime.now().add(const Duration(minutes: 5));
    var nextStatusAt = DateTime.now();
    while (DateTime.now().isBefore(drainDeadline)) {
      if (_frames.length >= sentCount) return;
      await Future.delayed(const Duration(milliseconds: 50));
      if (_frames.length > lastCount) {
        lastCount = _frames.length;
        idleRounds = 0;
      } else {
        idleRounds++;
        // No new frames for ~3s after DONE — stop waiting.
        if (idleRounds >= 60) return;
      }
      // Reading status every round would compete with the notifies we are
      // waiting on; poll sparingly and prefer whatever the device pushed.
      if (DateTime.now().isBefore(nextStatusAt)) continue;
      nextStatusAt = DateTime.now().add(const Duration(milliseconds: 500));
      try {
        final st = await _currentStatus();
        onStatus?.call(st);
        if (st.sentCount > sentCount) {
          sentCount = st.sentCount;
        }
      } catch (_) {}
    }
  }

  /// ACK records through [upToId].
  ///
  /// [mode] `recModeSummaryOnly` updates the device watermark only (keeps raw).
  /// `recModeFull` reclaims NOR space (advances ring tail).
  Future<void> ack(int upToId, {int mode = recModeFull}) async {
    await _resolve();
    final pkt = ByteData(6);
    pkt.setUint8(0, recCmdAck);
    pkt.setUint32(1, upToId, Endian.little);
    pkt.setUint8(5, mode);
    await _ctrl!.write(pkt.buffer.asUint8List(), withoutResponse: false);
  }

  Future<void> abort() async {
    await _resolve();
    await _ctrl!.write(Uint8List.fromList([recCmdAbort]), withoutResponse: false);
  }

  Future<void> dispose() async {
    await _dataSub?.cancel();
    _dataSub = null;
    await _statusSub?.cancel();
    _statusSub = null;
    try {
      if (_data != null) await _data!.setNotifyValue(false);
    } catch (_) {}
    try {
      if (_status != null) await _status!.setNotifyValue(false);
    } catch (_) {}
  }
}
