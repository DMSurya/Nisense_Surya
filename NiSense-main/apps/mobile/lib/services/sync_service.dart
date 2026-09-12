import 'dart:io';
import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:path_provider/path_provider.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../ble/record_sync_client.dart';
import '../net/api_client.dart';
import '../protocol/hcm_protocol.dart';

/// Pushes locally-logged measurement data to the server with an offline-tolerant
/// queue. CSV session logs (from SessionLogger) are uploaded to /ingest/csv;
/// the server dedupes by checksum so retries/backfill are safe. Structured
/// readings can also be pushed directly via [pushReadings].
class SyncService extends ChangeNotifier {
  SyncService(this._api);

  final ApiClient _api;

  bool _busy = false;
  String _status = 'idle';
  int _uploaded = 0;

  bool get busy => _busy;
  String get status => _status;
  int get uploadedCount => _uploaded;

  static const _kUploaded = 'sync_uploaded_files';

  Future<Directory> _logsDir() async {
    final docs = await getApplicationDocumentsDirectory();
    final dir = Directory('${docs.path}/HCM_Logs');
    if (!await dir.exists()) {
      await dir.create(recursive: true);
    }
    return dir;
  }

  /// Upload any CSV session logs not yet acknowledged as uploaded.
  Future<void> syncCsvLogs({String? deviceId, String? patientId}) async {
    if (_busy) return;
    _busy = true;
    _status = 'scanning logs…';
    notifyListeners();

    try {
      final prefs = await SharedPreferences.getInstance();
      final done = prefs.getStringList(_kUploaded)?.toSet() ?? <String>{};
      final dir = await _logsDir();
      final files = dir
          .listSync()
          .whereType<File>()
          .where((f) => f.path.toLowerCase().endsWith('.csv'))
          .toList();

      for (final f in files) {
        final name = f.uri.pathSegments.last;
        if (done.contains(name)) continue;
        _status = 'uploading $name';
        notifyListeners();
        try {
          final bytes = await f.readAsBytes();
          final res = await _api.uploadCsv(name, bytes,
              deviceId: deviceId, patientId: patientId);
          if (res['status'] == 'uploaded' || res['status'] == 'duplicate') {
            done.add(name);
            _uploaded++;
          }
        } catch (e) {
          // Leave for the next sync attempt (offline-tolerant).
          _status = 'deferred $name ($e)';
          notifyListeners();
        }
      }
      await prefs.setStringList(_kUploaded, done.toList());
      _status = 'done ($_uploaded uploaded)';
    } catch (e) {
      _status = 'error: $e';
    } finally {
      _busy = false;
      notifyListeners();
    }
  }

  /// Push structured readings (e.g. from live history) — idempotent server-side.
  Future<Map<String, dynamic>> pushReadings(
    String deviceId,
    List<Map<String, dynamic>> readings,
  ) async {
    return _api.ingestReadings(deviceId, readings);
  }

  /// Push the latest Metabolic / Vital / Vascular snapshot for the Link dashboard.
  Future<Map<String, dynamic>> pushClinicalSnapshot({
    required String deviceId,
    required int glucoseMgDl,
    required double insulin,
    required double homaIr,
    required int hrBpm,
    required int spo2Pct,
    required double hbGdl,
    required int respBpm,
    required int sdnnMs,
    required int rmssdMs,
    required int systolicMmhg,
    required int diastolicMmhg,
    int? quality,
  }) async {
    final ts = DateTime.now().toUtc().toIso8601String();
    final readings = <Map<String, dynamic>>[
      if (glucoseMgDl > 0)
        {
          'ts': ts,
          'type': 'glucose',
          'value': glucoseMgDl.toDouble(),
          'quality': quality,
          'extra': {
            'glucose_mg_dl': glucoseMgDl,
            'insulin': insulin,
            'homa_ir': homaIr,
          },
        },
      if (hrBpm > 0 || spo2Pct > 0 || sdnnMs > 0 || systolicMmhg > 0)
        {
          'ts': ts,
          'type': 'vitals',
          'value': hrBpm > 0 ? hrBpm.toDouble() : spo2Pct.toDouble(),
          'quality': quality,
          'extra': {
            'hr_bpm': hrBpm,
            'spo2_percent': spo2Pct,
            'hb_g_dl': hbGdl,
            'resp_rate_bpm': respBpm,
            'sdnn_ms': sdnnMs,
            'rmssd_ms': rmssdMs,
            'systolic_mmhg': systolicMmhg,
            'diastolic_mmhg': diastolicMmhg,
          },
        },
    ];
    if (readings.isEmpty) {
      return {'accepted': 0, 'duplicates': 0};
    }
    return pushReadings(deviceId, readings);
  }

  /// Maps one synced NOR record to a `/ingest/readings` row, or null if the
  /// type is unsupported / payload too short to decode. Reuses the same
  /// decoders as the local xlsx export so server and phone agree on fields.
  static Map<String, dynamic>? _recordToReading(SyncedRecord r) {
    String tsFrom(int unixSeconds) => unixSeconds > 0
        ? DateTime.fromMillisecondsSinceEpoch(unixSeconds * 1000, isUtc: true).toIso8601String()
        : DateTime.now().toUtc().toIso8601String();

    Map<String, dynamic> withMeas(Map<String, dynamic> extra) => {
          'measurement_id': r.measurementId,
          ...extra,
        };

    switch (r.type) {
      case recTypeGlucose:
        final f = decodeGlucoseRecordFields(r.payload);
        if (f.isEmpty) return null;
        return {
          'ts': tsFrom(f['timestamp_unix'] as int),
          'type': 'glucose',
          'record_id': r.recordId,
          'value': (f['glucose_mg_dl'] as int).toDouble(),
          'quality': f['quality'] as int,
          'extra': withMeas(f),
        };
      case recTypeVitals:
        final f = decodeVitalsRecordFields(r.payload);
        if (f.isEmpty) return null;
        return {
          'ts': tsFrom(f['timestamp_unix'] as int),
          'type': 'vitals',
          'record_id': r.recordId,
          'value': (f['hr_bpm'] as int).toDouble(),
          'quality': f['quality'] as int,
          'extra': withMeas(f),
        };
      case recTypeTemp:
        final f = decodeTempRecordFields(r.payload);
        if (f.isEmpty) return null;
        return {
          'ts': tsFrom(f['timestamp_unix'] as int),
          'type': 'temp',
          'record_id': r.recordId,
          'value': f['skin_temp_c'] as double,
          'extra': withMeas(f),
        };
      case recTypePpgRaw:
        final f = decodePpgRawChunkFields(r.payload);
        if (f.isEmpty) return null;
        return {
          'ts': DateTime.now().toUtc().toIso8601String(),
          'type': 'ppg_raw',
          'record_id': r.recordId,
          'extra': withMeas({'parent_id': r.parentId, ...f}),
        };
      case recTypeGlucoseRaw:
        final f = decodeGlucoseRawChunkFields(r.payload);
        if (f.isEmpty) return null;
        return {
          'ts': DateTime.now().toUtc().toIso8601String(),
          'type': 'glucose_raw',
          'record_id': r.recordId,
          'extra': withMeas({'parent_id': r.parentId, ...f}),
        };
      default:
        return null;
    }
  }

  /// Pushes synced NOR records to `/ingest/readings`, batching requests and
  /// tolerating partial failure (idempotent server-side, so failed batches
  /// are safe to retry on the next sync). Never throws — callers should
  /// treat this as best-effort and not block the local sync on it.
  ///
  /// [pushedIds] lists record IDs from batches that the server accepted or
  /// reported as duplicates (safe to mark locally as pushed).
  Future<({int accepted, int duplicates, int failed, List<int> pushedIds})>
      pushSyncedRecords(
    String deviceId,
    List<SyncedRecord> records, {
    int batchSize = 200,
  }) async {
    final mapped = <({int recordId, Map<String, dynamic> reading})>[];
    var undecodable = 0;
    for (final r in records) {
      final m = _recordToReading(r);
      if (m != null) {
        mapped.add((recordId: r.recordId, reading: m));
      } else {
        undecodable++;
      }
    }

    var accepted = 0;
    var duplicates = 0;
    var failed = undecodable;
    final pushedIds = <int>[];
    for (var i = 0; i < mapped.length; i += batchSize) {
      final batch = mapped.sublist(i, math.min(i + batchSize, mapped.length));
      try {
        final res = await pushReadings(
          deviceId,
          batch.map((e) => e.reading).toList(),
        );
        accepted += (res['accepted'] as int?) ?? 0;
        duplicates += (res['duplicates'] as int?) ?? 0;
        pushedIds.addAll(batch.map((e) => e.recordId));
      } catch (_) {
        failed += batch.length;
      }
    }
    return (
      accepted: accepted,
      duplicates: duplicates,
      failed: failed,
      pushedIds: pushedIds,
    );
  }
}
