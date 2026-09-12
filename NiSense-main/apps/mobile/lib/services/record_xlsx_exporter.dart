import 'dart:io';

import 'package:excel/excel.dart';
import 'package:path_provider/path_provider.dart';
import 'package:share_plus/share_plus.dart';
import '../ble/record_sync_client.dart';
import '../protocol/hcm_protocol.dart';
import '../util/record_timestamp.dart';
import 'record_local_store.dart';

/// Multi-sheet xlsx exporter for NOR sync records.
///
/// Sheet order: Summary, Glucose, Glucose_Raw, PPG_Raw, Vitals, Temp
///
/// Byte-level payload decoding lives in `hcm_protocol.dart`
/// (`decode*RecordFields`/`decode*ChunkFields`) so the local export, the
/// "All Records" browser, and the server push all agree on field meaning.
class RecordXlsxExporter {
  RecordXlsxExporter({required this.records, required this.patientName});

  final List<SyncedRecord> records;
  final String patientName;

  /// Export every record in the local store to a single FULL xlsx and share it.
  /// Returns the written path, or null when the store is empty.
  static Future<String?> exportAllFromStore({
    required String patientName,
    bool share = true,
  }) async {
    final stored = await RecordLocalStore.instance.getAll();
    if (stored.isEmpty) return null;

    final dir = await getApplicationDocumentsDirectory();
    final now = DateTime.now();
    final ts = '${now.year.toString().padLeft(4, '0')}_'
        '${now.month.toString().padLeft(2, '0')}_'
        '${now.day.toString().padLeft(2, '0')}-'
        '${now.hour.toString().padLeft(2, '0')}_'
        '${now.minute.toString().padLeft(2, '0')}_'
        '${now.second.toString().padLeft(2, '0')}';
    final safe = patientName.replaceAll(RegExp(r'[^A-Za-z0-9_-]'), '_');
    final path = '${dir.path}/${ts}_${safe}_NiSense_FULL.xlsx';

    final exporter = RecordXlsxExporter(
      records: stored.map((r) => r.toSyncedRecord()).toList(),
      patientName: patientName,
    );
    await exporter.export(path);
    if (share) await shareFile(path);
    return path;
  }

  /// Write workbook to [path]. Creates parent dirs if needed.
  Future<void> export(String path) async {
    final dir = File(path).parent;
    if (!dir.existsSync()) dir.createSync(recursive: true);

    final excel = Excel.createExcel();

    final glucose = records.where((r) => r.type == recTypeGlucose).toList();
    final glucRaw = records.where((r) => r.type == recTypeGlucoseRaw).toList();
    final vitals  = records.where((r) => r.type == recTypeVitals).toList();
    final ppgRaw  = records.where((r) => r.type == recTypePpgRaw).toList();
    final temp    = records.where((r) => r.type == recTypeTemp).toList();

    // Rename the default Sheet1 to Summary so it is always first in the workbook.
    excel.rename('Sheet1', 'Summary');
    _writeSummary(excel['Summary'], glucose, glucRaw, vitals, ppgRaw, temp);

    if (glucose.isNotEmpty) _writeGlucose(excel, glucose);
    if (glucRaw.isNotEmpty) _writeGlucoseRaw(excel, glucRaw, glucose);
    if (ppgRaw.isNotEmpty) _writePpgRaw(excel, ppgRaw, vitals);
    if (vitals.isNotEmpty) _writeVitals(excel, vitals);
    if (temp.isNotEmpty)   _writeTemp(excel, temp);

    final bytes = excel.save();
    if (bytes == null) throw StateError('Excel encode failed');
    await File(path).writeAsBytes(bytes);
  }

  /// Share the file using share_plus.
  static Future<void> shareFile(String path) async {
    await SharePlus.instance.share(
      ShareParams(files: [XFile(path)], text: 'NiSense records'),
    );
  }

  void _header(Sheet sheet, List<String> cols) {
    sheet.appendRow(cols.map<CellValue>((c) => TextCellValue(c)).toList());
  }

  /// Carry-forward tracker: for headline outputs, a 0 usually means "sensor
  /// gave nothing this time", not a genuine reading. Rather than write a raw
  /// 0 into the sheet, hold the last genuine (non-zero) value seen so far
  /// for that key until a new non-zero value arrives. If nothing genuine has
  /// been seen yet, the original (zero) value is returned as-is — there is
  /// nothing to carry forward from.
  final Map<String, num> _lastNonZero = {};

  int _carryInt(String key, int value) {
    if (value != 0) {
      _lastNonZero[key] = value;
      return value;
    }
    final prev = _lastNonZero[key];
    return prev != null ? prev.toInt() : value;
  }

  double _carryDouble(String key, double value) {
    if (value != 0.0) {
      _lastNonZero[key] = value;
      return value;
    }
    final prev = _lastNonZero[key];
    return prev != null ? prev.toDouble() : value;
  }

  // ---------------------------------------------------------------------------
  // Summary
  // ---------------------------------------------------------------------------

  void _writeSummary(
    Sheet sheet,
    List<SyncedRecord> glucose,
    List<SyncedRecord> glucRaw,
    List<SyncedRecord> vitals,
    List<SyncedRecord> ppgRaw,
    List<SyncedRecord> temp,
  ) {
    _header(sheet, ['Record_Type', 'Count']);
    sheet.appendRow([TextCellValue('Patient'), TextCellValue(patientName)]);
    sheet.appendRow([TextCellValue('Export_Date'), TextCellValue(DateTime.now().toIso8601String())]);
    sheet.appendRow([TextCellValue(''), TextCellValue('')]);
    final rows = [
      ('Glucose', glucose.length),
      ('Glucose_Raw chunks', glucRaw.length),
      ('Vitals', vitals.length),
      ('PPG_Raw chunks', ppgRaw.length),
      ('Temp', temp.length),
      ('Total', records.length),
    ];
    for (final (label, count) in rows) {
      sheet.appendRow([TextCellValue(label), IntCellValue(count)]);
    }
  }

  // ---------------------------------------------------------------------------
  // Glucose — full algo record
  // ---------------------------------------------------------------------------

  void _writeGlucose(Excel excel, List<SyncedRecord> rows) {
    final sheet = excel['Glucose'];
    _header(sheet, [
      'Record_ID', 'Measurement_ID', 'Timestamp_unix', 'Date', 'Time',
      'Patient', 'Device_ID',
      'Glucose_mg_dL', 'Quality', 'Variant', 'Model_Version',
      'Intercept', 'Outlier_K', 'Tot_Coeff', 'Y1', 'Avg', 'StdDev', 'UpLim', 'LlLim',
      'P_Count', 'N_Count', 'P_Val', 'N_Val', 'P_Plus_N',
      'Y2_Val', 'Y2_Percent', 'Group_CD', 'Y2_Factor', 'Y2_Factor_Val',
      'Const_Val', 'Y3_Value', 'Y3_Row', 'Elim_Per', 'Elim_Val', 'Y_Value',
      'Cal_Factor', 'AG_Adj', 'Norm_Glucose', 'Insulin', 'Insulin_Corr',
      'Insulin_Ratio', 'Inv_Ratio', 'HOMA_IR',
    ]);
    for (final r in rows) {
      final f = decodeGlucoseRecordFields(r.payload);
      if (f.isEmpty) continue;
      final ts = f['timestamp_unix'] as int;
      sheet.appendRow([
        IntCellValue(r.recordId),
        IntCellValue(r.measurementId != 0
            ? r.measurementId
            : (f['measurement_id'] as int? ?? 0)),
        IntCellValue(ts),
        TextCellValue(RecordTimestamp.date(ts)),
        TextCellValue(RecordTimestamp.time(ts)),
        TextCellValue(patientName),
        IntCellValue(f['device_id'] as int),
        IntCellValue(_carryInt('glucose_mg_dl', f['glucose_mg_dl'] as int)),
        IntCellValue(f['quality'] as int),
        IntCellValue(f['variant'] as int),
        IntCellValue(f['model_version'] as int),
        DoubleCellValue(f['intercept'] as double), DoubleCellValue(f['outlier_k'] as double),
        DoubleCellValue(f['tot_coeff'] as double), DoubleCellValue(f['y1_value'] as double),
        DoubleCellValue(f['avg_val'] as double), DoubleCellValue(f['std_dev'] as double),
        DoubleCellValue(f['up_lim'] as double), DoubleCellValue(f['ll_lim'] as double),
        IntCellValue(f['p_count'] as int), IntCellValue(f['n_count'] as int),
        DoubleCellValue(f['p_val'] as double), DoubleCellValue(f['n_val'] as double),
        DoubleCellValue(f['p_plus_n'] as double),
        DoubleCellValue(f['y2_val'] as double), DoubleCellValue(f['y2_percent'] as double),
        IntCellValue(f['group_cd'] as int),
        DoubleCellValue(f['y2_factor'] as double), DoubleCellValue(f['y2_factor_val'] as double),
        DoubleCellValue(f['const_val'] as double), DoubleCellValue(f['y3_value'] as double),
        IntCellValue(f['y3_row_no'] as int),
        DoubleCellValue(f['elim_per'] as double), DoubleCellValue(f['elim_val'] as double),
        IntCellValue(f['y_value'] as int),
        DoubleCellValue(f['calibration_factor'] as double), DoubleCellValue(f['ag_adjusted'] as double),
        DoubleCellValue(f['normalized_glucose'] as double),
        DoubleCellValue(_carryDouble('actual_insulin', f['actual_insulin'] as double)), DoubleCellValue(f['insulin_correction'] as double),
        DoubleCellValue(f['insulin_ratio'] as double), DoubleCellValue(f['inverse_ratio'] as double),
        DoubleCellValue(_carryDouble('homa_ir_index', f['homa_ir_index'] as double)),
      ]);
    }
  }

  // ---------------------------------------------------------------------------
  // Glucose Raw chunks
  // ---------------------------------------------------------------------------

  void _writeGlucoseRaw(
    Excel excel,
    List<SyncedRecord> chunks,
    List<SyncedRecord> glucose,
  ) {
    final sheet = excel['Glucose_Raw'];
    _header(sheet, [
      'Measurement_ID', 'Parent_ID', 'Timestamp_unix', 'Date', 'Time',
      'Chunk_ID', 'Sample_Index', 'ADC', 'Voltage_mV',
    ]);

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

    for (final entry in byParent.entries) {
      final decoded = entry.value
          .map((c) => (
                mid: c.measurementId,
                fields: decodeGlucoseRawChunkFields(c.payload),
              ))
          .where((x) => x.fields.isNotEmpty)
          .toList()
        ..sort((a, b) => ((a.fields['header'] as Map)['chunk_index'] as int)
            .compareTo((b.fields['header'] as Map)['chunk_index'] as int));
      final parentFallbackTs = tsByParent[entry.key] ?? 0;
      var idx = 0;
      for (final item in decoded) {
        final f = item.fields;
        final header = f['header'] as Map<String, dynamic>;
        final samples = f['samples'] as List<Map<String, dynamic>>;
        final mid = item.mid != 0
            ? item.mid
            : (header['measurement_id'] as int? ?? 0);
        for (final s in samples) {
          final ts = (s['timestamp_unix'] as int?) ?? parentFallbackTs;
          final date = ts > 0 ? RecordTimestamp.date(ts) : '';
          final time = ts > 0 ? RecordTimestamp.time(ts) : '';
          sheet.appendRow([
            IntCellValue(mid),
            IntCellValue(entry.key),
            IntCellValue(ts),
            TextCellValue(date),
            TextCellValue(time),
            IntCellValue(header['chunk_index'] as int),
            IntCellValue(idx),
            IntCellValue(s['adc'] as int),
            DoubleCellValue(s['voltage_mv'] as double),
          ]);
          idx++;
        }
      }
    }
  }

  // ---------------------------------------------------------------------------
  // PPG Raw chunks
  // ---------------------------------------------------------------------------

  void _writePpgRaw(Excel excel, List<SyncedRecord> chunks, List<SyncedRecord> vitals) {
    final sheet = excel['PPG_Raw'];
    _header(sheet, [
      'Measurement_ID', 'Parent_ID', 'Timestamp_ms', 'Date', 'Time',
      'Sample_Index',
      'IR', 'Red', 'Green',
      'IR_DC', 'Red_DC', 'Green_DC',
      'IR_AC', 'Red_AC', 'Green_AC',
      'Accel_X', 'Accel_Y', 'Accel_Z',
    ]);

    final tsByParent   = <int, int>{};
    final rateByParent = <int, int>{};
    for (final v in vitals) {
      final f = decodeVitalsRecordFields(v.payload);
      if (f.isEmpty) continue;
      tsByParent[v.recordId]   = f['timestamp_unix'] as int;
      rateByParent[v.recordId] = f['sample_rate_hz'] as int;
    }

    final byParent = <int, List<SyncedRecord>>{};
    for (final c in chunks) {
      byParent.putIfAbsent(c.parentId, () => []).add(c);
    }

    for (final entry in byParent.entries) {
      final parent = entry.key;
      final decoded = entry.value
          .map((c) => (
                mid: c.measurementId,
                fields: decodePpgRawChunkFields(c.payload),
              ))
          .where((x) => x.fields.isNotEmpty)
          .toList()
        ..sort((a, b) => ((a.fields['header'] as Map)['chunk_index'] as int)
            .compareTo((b.fields['header'] as Map)['chunk_index'] as int));
      final tsSec = tsByParent[parent] ?? 0;
      final rate  = rateByParent[parent] ?? 0;
      var idx = 0;
      for (final item in decoded) {
        final f = item.fields;
        final header = f['header'] as Map<String, dynamic>;
        final samples = f['samples'] as List<Map<String, dynamic>>;
        final mid = item.mid != 0
            ? item.mid
            : (header['measurement_id'] as int? ?? 0);
        for (final s in samples) {
          var tsMs = (s['timestamp_unix_ms'] as int?) ?? 0;
          if (tsMs == 0) {
            tsMs = tsSec * 1000;
            if (rate > 0) tsMs += (idx * 1000) ~/ rate;
          }
          final date = tsMs > 0 ? RecordTimestamp.dateMs(tsMs) : '';
          final time = tsMs > 0 ? RecordTimestamp.timeMs(tsMs) : '';
          sheet.appendRow([
            IntCellValue(mid),
            IntCellValue(parent),
            IntCellValue(tsMs),
            TextCellValue(date),
            TextCellValue(time),
            IntCellValue(idx),
            IntCellValue(s['ir'] as int),
            IntCellValue(s['red'] as int),
            IntCellValue(_carryInt('green_$parent', s['green'] as int)),
            IntCellValue(s['ir_dc'] as int),
            IntCellValue(s['red_dc'] as int),
            IntCellValue(s['green_dc'] as int),
            IntCellValue(s['ir_ac'] as int),
            IntCellValue(s['red_ac'] as int),
            IntCellValue(s['green_ac'] as int),
            IntCellValue(s['accel_x'] as int),
            IntCellValue(s['accel_y'] as int),
            IntCellValue(s['accel_z'] as int),
          ]);
          idx++;
        }
      }
    }
  }

  // ---------------------------------------------------------------------------
  // Vitals
  // ---------------------------------------------------------------------------

  void _writeVitals(Excel excel, List<SyncedRecord> rows) {
    final sheet = excel['Vitals'];
    _header(sheet, [
      'Record_ID', 'Measurement_ID', 'Timestamp_unix', 'Date', 'Time',
      'Patient', 'Device_ID',
      'HR_BPM', 'HR_Conf', 'SpO2_Pct', 'SpO2_Conf',
      'Hb_g_dL', 'Hb_Conf', 'Resp_BPM', 'Resp_Conf',
      'SDNN_ms', 'RMSSD_ms', 'Sys_mmHg', 'Dia_mmHg',
      'Quality', 'SNR_dB_x10', 'Perf_x10', 'Sample_Rate_Hz', 'Sample_Count',
    ]);
    for (final r in rows) {
      final f = decodeVitalsRecordFields(r.payload);
      if (f.isEmpty) continue;
      final ts = f['timestamp_unix'] as int;
      sheet.appendRow([
        IntCellValue(r.recordId),
        IntCellValue(r.measurementId != 0
            ? r.measurementId
            : (f['measurement_id'] as int? ?? 0)),
        IntCellValue(ts),
        TextCellValue(RecordTimestamp.date(ts)),
        TextCellValue(RecordTimestamp.time(ts)),
        TextCellValue(patientName),
        IntCellValue(f['device_id'] as int),
        IntCellValue(_carryInt('hr_bpm', f['hr_bpm'] as int)),
        IntCellValue(f['hr_conf'] as int),
        IntCellValue(_carryInt('spo2_percent', f['spo2_percent'] as int)),
        IntCellValue(f['spo2_conf'] as int),
        DoubleCellValue(_carryDouble('hb_g_dl', f['hb_g_dl'] as double)),
        IntCellValue(f['hb_conf'] as int),
        IntCellValue(_carryInt('resp_rate_bpm', f['resp_rate_bpm'] as int)),
        IntCellValue(f['resp_conf'] as int),
        IntCellValue(f['sdnn_ms'] as int),
        IntCellValue(f['rmssd_ms'] as int),
        IntCellValue(f['systolic_mmhg'] as int),
        IntCellValue(f['diastolic_mmhg'] as int),
        IntCellValue(f['quality'] as int),
        IntCellValue(f['snr_db_x10'] as int),
        IntCellValue(f['perfusion_index_x10'] as int),
        IntCellValue(f['sample_rate_hz'] as int),
        IntCellValue(f['sample_count'] as int),
      ]);
    }
  }

  // ---------------------------------------------------------------------------
  // Temperature
  // ---------------------------------------------------------------------------

  void _writeTemp(Excel excel, List<SyncedRecord> rows) {
    final sheet = excel['Temp'];
    _header(sheet, [
      'Record_ID', 'Measurement_ID', 'Timestamp_unix', 'Date', 'Time',
      'Patient', 'Device_ID',
      'SoC_Temp_C', 'Skin_Temp_C', 'Skin_Band', 'Source',
    ]);
    for (final r in rows) {
      final f = decodeTempRecordFields(r.payload);
      if (f.isEmpty) continue;
      final ts = f['timestamp_unix'] as int;
      sheet.appendRow([
        IntCellValue(r.recordId),
        IntCellValue(r.measurementId != 0
            ? r.measurementId
            : (f['measurement_id'] as int? ?? 0)),
        IntCellValue(ts),
        TextCellValue(RecordTimestamp.date(ts)),
        TextCellValue(RecordTimestamp.time(ts)),
        TextCellValue(patientName),
        IntCellValue(f['device_id'] as int),
        DoubleCellValue(_carryDouble('soc_temp_c', f['soc_temp_c'] as double)),
        DoubleCellValue(_carryDouble('skin_temp_c', f['skin_temp_c'] as double)),
        IntCellValue(f['skin_band'] as int),
        IntCellValue(f['source'] as int),
      ]);
    }
  }
}
