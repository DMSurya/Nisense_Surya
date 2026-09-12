import 'dart:io';

import 'package:intl/intl.dart';
import 'package:path_provider/path_provider.dart';

import '../protocol/hcm_protocol.dart';

class SessionLogger {
  SessionLogger({required this.patientName, required this.deviceId}) {
    _openVitals();
    _openGlucose();
    _openPpgStream();
    _openAccelStream();
  }

  String patientName;
  final String deviceId;
  final List<String> sessionPaths = [];
  final _fmt = DateFormat('yyyy_MM_dd-HH_mm_ss');

  IOSink? _vitalsSink;
  IOSink? _glucoseSink;
  IOSink? _ppgSink;
  IOSink? _accelSink;

  Future<Directory> _logDir() async {
    final base = await getApplicationDocumentsDirectory();
    final dir = Directory('${base.path}/HCM_Logs');
    if (!dir.existsSync()) dir.createSync(recursive: true);
    return dir;
  }

  void setPatientName(String name) {
    patientName = name.isNotEmpty ? name : 'Patient';
  }

  Future<void> _openVitals() async {
    final dir = await _logDir();
    final path = '${dir.path}/${_fmt.format(DateTime.now())}_Vitals.csv';
    sessionPaths.add(path);
    _vitalsSink = File(path).openWrite(mode: FileMode.write);
    _vitalsSink!.writeln(
      'Timestamp_unix,Patient,Device_ID,HR_BPM,HR_Conf,SpO2_Pct,SpO2_Conf,Hb_g_dL,Hb_Conf,Resp_BPM,Resp_Conf,SDNN_ms,RMSSD_ms,Sys_mmHg,Dia_mmHg',
    );
  }

  Future<void> _openGlucose() async {
    final dir = await _logDir();
    final path = '${dir.path}/${_fmt.format(DateTime.now())}_Glucose.csv';
    sessionPaths.add(path);
    _glucoseSink = File(path).openWrite(mode: FileMode.write);
    _glucoseSink!.writeln(
      'Timestamp_unix,Patient,Device_ID,Glucose_mg_dL,Glucose_mmol_L,Quality',
    );
  }

  Future<void> _openPpgStream() async {
    final dir = await _logDir();
    final path = '${dir.path}/${_fmt.format(DateTime.now())}_PPG_Stream.csv';
    sessionPaths.add(path);
    _ppgSink = File(path).openWrite(mode: FileMode.write);
    _ppgSink!.writeln(
      'Timestamp_unix_ms,Sample_Num,Raw_IR,Raw_Red,Raw_Green,Accel_X_mg,Accel_Y_mg,Accel_Z_mg',
    );
  }

  Future<void> _openAccelStream() async {
    final dir = await _logDir();
    final path = '${dir.path}/${_fmt.format(DateTime.now())}_Accel_Stream.csv';
    sessionPaths.add(path);
    _accelSink = File(path).openWrite(mode: FileMode.write);
    _accelSink!.writeln('Timestamp_unix_ms,Seq,X_mg,Y_mg,Z_mg');
  }

  void logVitals(VitalsData v) {
    _vitalsSink?.writeln(
      '${v.timestamp},$patientName,$deviceId,'
      '${v.hrBpm},${v.hrConf},${v.spo2Pct},${v.spo2Conf},'
      '${v.hbGdl},${v.hbConf},${v.respBpm},${v.respConf},'
      '${v.sdnn_ms},${v.rmssd_ms},${v.systolic_mmhg},${v.diastolic_mmhg}',
    );
  }

  void logGlucose(GlucoseData g) {
    _glucoseSink?.writeln(
      '${g.timestamp},$patientName,$deviceId,'
      '${g.glucose_mg_dl},${g.glucose_mmol_l},${g.quality}',
    );
  }

  void logPpg(PpgSample sample) {
    _ppgSink?.writeln(
      '${sample.timestamp_ms},${sample.sample_num},'
      '${sample.raw_ir},${sample.raw_red},${sample.raw_green},'
      '${sample.accel_x},${sample.accel_y},${sample.accel_z}',
    );
  }

  void logAccel(AccelSample sample) {
    _accelSink?.writeln(
      '${sample.timestamp_ms},${sample.seq},'
      '${sample.x_mg},${sample.y_mg},${sample.z_mg}',
    );
  }

  Future<void> close() async {
    await _vitalsSink?.flush();
    await _vitalsSink?.close();
    await _glucoseSink?.flush();
    await _glucoseSink?.close();
    await _ppgSink?.flush();
    await _ppgSink?.close();
    await _accelSink?.flush();
    await _accelSink?.close();
    _vitalsSink = null;
    _glucoseSink = null;
    _ppgSink = null;
    _accelSink = null;
  }
}
