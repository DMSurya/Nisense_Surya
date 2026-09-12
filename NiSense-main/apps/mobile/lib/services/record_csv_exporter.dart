import 'dart:io';
import 'dart:typed_data';

import 'package:intl/intl.dart';
import 'package:path_provider/path_provider.dart';

import '../ble/record_sync_client.dart';
import '../protocol/hcm_protocol.dart';
import '../util/record_timestamp.dart';

/// Converts BLE-pulled NOR records into CSV files matching prior FatFS layouts
/// (plus full glucose analytics / PPG DC-AC columns from the hybrid store).
class RecordCsvExporter {
  RecordCsvExporter({required this.patientName});

  final String patientName;
  final _fmt = DateFormat('yyyy_MM_dd-HH_mm_ss');

  /// Parents whose raw chunks are missing indexes vs declared [chunk_count].
  final List<String> incompleteParents = [];

  /// Returns true when decoded chunk headers cover `0 .. chunk_count-1`.
  static bool chunksComplete(List<Map<String, dynamic>> decoded) {
    if (decoded.isEmpty) return true;
    final expect =
        (decoded.first['header'] as Map)['chunk_count'] as int? ?? 0;
    if (expect <= 0) return true;
    final found = <int>{};
    for (final f in decoded) {
      final idx = (f['header'] as Map)['chunk_index'] as int?;
      if (idx != null) found.add(idx);
    }
    for (var i = 0; i < expect; i++) {
      if (!found.contains(i)) return false;
    }
    return true;
  }

  Future<Directory> _logDir() async {
    final base = await getApplicationDocumentsDirectory();
    final dir = Directory('${base.path}/HCM_Logs');
    if (!dir.existsSync()) dir.createSync(recursive: true);
    return dir;
  }

  /// Write CSV files for [records]. Returns paths created.
  Future<List<String>> exportAll(List<SyncedRecord> records) async {
    final dir = await _logDir();
    final stamp = _fmt.format(DateTime.now());
    final paths = <String>[];

    final glucose = records.where((r) => r.type == recTypeGlucose).toList();
    final vitals = records.where((r) => r.type == recTypeVitals).toList();
    final ppgRaw = records.where((r) => r.type == recTypePpgRaw).toList();
    final glucRaw = records.where((r) => r.type == recTypeGlucoseRaw).toList();

    if (glucose.isNotEmpty) {
      final p = '${dir.path}/${stamp}_Glucose.csv';
      await File(p).writeAsString(_glucoseCsv(glucose));
      paths.add(p);
    }
    if (glucRaw.isNotEmpty) {
      final p = '${dir.path}/${stamp}_Glucose_Raw.csv';
      await File(p).writeAsString(_glucoseRawCsv(glucRaw, glucose));
      paths.add(p);
    }
    if (vitals.isNotEmpty) {
      final p = '${dir.path}/${stamp}_Vitals.csv';
      await File(p).writeAsString(_vitalsCsv(vitals));
      paths.add(p);
    }
    if (ppgRaw.isNotEmpty) {
      final p = '${dir.path}/${stamp}_PPG_Raw.csv';
      await File(p).writeAsString(_ppgRawCsv(ppgRaw, vitals));
      paths.add(p);
    }
    return paths;
  }

  String _glucoseCsv(List<SyncedRecord> rows) {
    final buf = StringBuffer();
    buf.writeln(
      'Timestamp_unix,Date,Time,Patient,Device_ID,Glucose_mg_dL,Quality,Variant,Model_Version,'
      'Intercept,Outlier_K,Tot_Coeff,Y1,Avg,StdDev,UpLim,LlLim,P_Count,N_Count,'
      'P_Val,N_Val,P_Plus_N,Y2_Val,Y2_Percent,Group_CD,Y2_Factor,Y2_Factor_Val,'
      'Const_Val,Y3_Value,Y3_Row,Elim_Per,Elim_Val,Y_Value,Cal_Factor,AG_Adj,'
      'Norm_Glucose,Insulin,Insulin_Corr,Insulin_Ratio,Inv_Ratio,HOMA_IR',
    );
    for (final r in rows) {
      final d = ByteData.sublistView(r.payload);
      if (r.payload.length < 144) continue;
      final ts = d.getUint32(0, Endian.little);
      final deviceId = d.getUint32(4, Endian.little);
      final glucose = d.getUint16(8, Endian.little);
      final quality = d.getUint8(10);
      final variant = d.getUint8(11);
      final modelVer = d.getUint32(12, Endian.little);
      double f(int off) => d.getFloat32(off, Endian.little);
      int i32(int off) => d.getInt32(off, Endian.little);

      buf.writeln(
        '$ts,${RecordTimestamp.date(ts)},${RecordTimestamp.time(ts)},'
        '$patientName,$deviceId,$glucose,$quality,$variant,$modelVer,'
        '${f(16)},${f(20)},${f(24)},${f(28)},${f(32)},${f(36)},${f(40)},${f(44)},'
        '${i32(48)},${i32(52)},'
        '${f(56)},${f(60)},${f(64)},${f(68)},${f(72)},${i32(76)},${f(80)},${f(84)},'
        '${f(88)},${f(92)},${i32(96)},${f(100)},${f(104)},${i32(108)},'
        '${f(112)},${f(116)},${f(120)},${f(124)},${f(128)},${f(132)},${f(136)},${f(140)}',
      );
    }
    return buf.toString();
  }

  String _vitalsCsv(List<SyncedRecord> rows) {
    final buf = StringBuffer();
    buf.writeln(
      'Timestamp_unix,Date,Time,Patient,Device_ID,HR_BPM,HR_Conf,SpO2_Pct,SpO2_Conf,'
      'Hb_g_dL,Hb_Conf,Resp_BPM,Resp_Conf,SDNN_ms,RMSSD_ms,Sys_mmHg,Dia_mmHg,'
      'Quality,SNR_dB_x10,Perf_x10,Sample_Rate_Hz,Sample_Count',
    );
    for (final r in rows) {
      final d = ByteData.sublistView(r.payload);
      if (r.payload.length < 32) continue;
      final ts = d.getUint32(0, Endian.little);
      final deviceId = d.getUint32(4, Endian.little);
      final hr = d.getUint16(8, Endian.little);
      final hrConf = d.getUint8(10);
      final spo2 = d.getUint8(11);
      final spo2Conf = d.getUint8(12);
      final hbConf = d.getUint8(13);
      final hbX10 = d.getUint16(14, Endian.little);
      final resp = d.getUint8(16);
      final respConf = d.getUint8(17);
      final sdnn = d.getUint16(18, Endian.little);
      final rmssd = d.getUint16(20, Endian.little);
      final sys = d.getUint16(22, Endian.little);
      final dia = d.getUint16(24, Endian.little);
      final quality = d.getUint8(26);
      // flags at 27
      final snr = r.payload.length >= 30 ? d.getUint16(28, Endian.little) : 0;
      final perf = r.payload.length >= 31 ? d.getUint8(30) : 0;
      final rate = r.payload.length >= 34 ? d.getUint16(32, Endian.little) : 0;
      final count = r.payload.length >= 36 ? d.getUint16(34, Endian.little) : 0;
      final hb = hbX10 / 10.0;
      buf.writeln(
        '$ts,${RecordTimestamp.date(ts)},${RecordTimestamp.time(ts)},'
        '$patientName,$deviceId,$hr,$hrConf,$spo2,$spo2Conf,'
        '$hb,$hbConf,$resp,$respConf,$sdnn,$rmssd,$sys,$dia,'
        '$quality,$snr,$perf,$rate,$count',
      );
    }
    return buf.toString();
  }

  String _glucoseRawCsv(List<SyncedRecord> chunks, List<SyncedRecord> glucose) {
    final tsByParent = <int, int>{};
    for (final g in glucose) {
      final f = decodeGlucoseRecordFields(g.payload);
      if (f.isEmpty) continue;
      tsByParent[g.recordId] = f['timestamp_unix'] as int;
    }

    final byParent = <int, List<SyncedRecord>>{};
    for (final c in chunks) {
      byParent.putIfAbsent(c.parentId, () => []).add(c);
    }
    final buf = StringBuffer();
    buf.writeln(
      'Timestamp_unix,Date,Time,Sample_Index,ADC,Voltage_mV,parent_id',
    );
    for (final entry in byParent.entries) {
      final decoded = entry.value
          .map((c) => decodeGlucoseRawChunkFields(c.payload))
          .where((f) => f.isNotEmpty)
          .toList()
        ..sort((a, b) => ((a['header'] as Map)['chunk_index'] as int)
            .compareTo((b['header'] as Map)['chunk_index'] as int));
      if (!chunksComplete(decoded)) {
        incompleteParents.add('glucose_raw parent=${entry.key}');
        buf.writeln(
          '# INCOMPLETE: parent=${entry.key} missing chunk index(es)',
        );
      }
      final parentFallbackTs = tsByParent[entry.key] ?? 0;
      var idx = 0;
      for (final f in decoded) {
        final samples = f['samples'] as List<Map<String, dynamic>>;
        for (final s in samples) {
          final ts = (s['timestamp_unix'] as int?) ?? parentFallbackTs;
          final date = ts > 0 ? RecordTimestamp.date(ts) : '';
          final time = ts > 0 ? RecordTimestamp.time(ts) : '';
          buf.writeln(
            '$ts,$date,$time,$idx,${s['adc']},${s['voltage_mv']},${entry.key}',
          );
          idx++;
        }
      }
    }
    return buf.toString();
  }

  String _ppgRawCsv(List<SyncedRecord> chunks, List<SyncedRecord> vitals) {
    final tsByParent = <int, int>{};
    final rateByParent = <int, int>{};
    for (final v in vitals) {
      final f = decodeVitalsRecordFields(v.payload);
      if (f.isEmpty) continue;
      tsByParent[v.recordId] = f['timestamp_unix'] as int;
      rateByParent[v.recordId] = f['sample_rate_hz'] as int;
    }

    final byParent = <int, List<SyncedRecord>>{};
    for (final c in chunks) {
      byParent.putIfAbsent(c.parentId, () => []).add(c);
    }

    final buf = StringBuffer();
    buf.writeln(
      'Timestamp_unix_ms,Date,Time,Sample_Index,IR,Red,Green,IR_DC,Red_DC,Green_DC,'
      'IR_AC,Red_AC,Green_AC,Accel_X,Accel_Y,Accel_Z,parent_id',
    );

    for (final entry in byParent.entries) {
      final parent = entry.key;
      final decoded = entry.value
          .map((c) => decodePpgRawChunkFields(c.payload))
          .where((f) => f.isNotEmpty)
          .toList()
        ..sort((a, b) => ((a['header'] as Map)['chunk_index'] as int)
            .compareTo((b['header'] as Map)['chunk_index'] as int));
      if (!chunksComplete(decoded)) {
        incompleteParents.add('ppg_raw parent=$parent');
        buf.writeln('# INCOMPLETE: parent=$parent missing chunk index(es)');
      }
      final tsSec = tsByParent[parent] ?? 0;
      final rate = rateByParent[parent] ?? 0;
      var idx = 0;
      for (final f in decoded) {
        final samples = f['samples'] as List<Map<String, dynamic>>;
        for (final s in samples) {
          var tsMs = (s['timestamp_unix_ms'] as int?) ?? 0;
          if (tsMs == 0) {
            tsMs = tsSec * 1000;
            if (rate > 0) tsMs += (idx * 1000) ~/ rate;
          }
          final date = tsMs > 0 ? RecordTimestamp.dateMs(tsMs) : '';
          final time = tsMs > 0 ? RecordTimestamp.timeMs(tsMs) : '';
          buf.writeln(
            '$tsMs,$date,$time,$idx,${s['ir']},${s['red']},${s['green']},'
            '${s['ir_dc']},${s['red_dc']},${s['green_dc']},'
            '${s['ir_ac']},${s['red_ac']},${s['green_ac']},'
            '${s['accel_x']},${s['accel_y']},${s['accel_z']},$parent',
          );
          idx++;
        }
      }
    }
    return buf.toString();
  }
}
