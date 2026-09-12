library hcm_protocol;

import 'dart:convert';
import 'dart:math' as math;
import 'dart:typed_data';

const int protocolVersionMajor = 3;
const int protocolVersionMinor = 2;

const int bleSecurityProfileOpen = 0;
const int bleSecurityProfileSecure = 1;

const int blePairingIdle = 0;
const int blePairingWaitingPasskey = 1;
const int blePairingConfirmPasskey = 2;
const int blePairingBonding = 3;
const int blePairingComplete = 4;
const int blePairingFailed = 5;

const String svcWearableConfig = '12345678-1234-5678-1234-56789abcdef0';
const String svcSensorData = '12345678-1234-5678-1234-56789abcdef1';
const String svcWifiConfig = '12345678-1234-5678-1234-56789abcdef2';

const String chrcDeviceName = '12345678-1234-5678-1234-56789abcf001';
const String chrcRtcTrim = '12345678-1234-5678-1234-56789abcf003';
const String chrcBatteryLow = '12345678-1234-5678-1234-56789abcf004';

/** SIG Current Time Service (0x1805) — calendar date/time/day-of-week. */
const String svcCurrentTime = '00001805-0000-1000-8000-00805f9b34fb';
const String chrcCurrentTime = '00002a2b-0000-1000-8000-00805f9b34fb';

/** SIG Battery Service (0x180F) — standard battery level for generic apps. */
const String svcBattery = '0000180f-0000-1000-8000-00805f9b34fb';
const String chrcBatteryLevel = '00002a19-0000-1000-8000-00805f9b34fb';
const String chrcBrightness = '12345678-1234-5678-1234-56789abcf005';
const String chrcVolume = '12345678-1234-5678-1234-56789abcf006';
const String chrcPpgPref = '12345678-1234-5678-1234-56789abcf007';
const String chrcSecurityProfile = '12345678-1234-5678-1234-56789abcf008';
const String chrcMeasCtrl = '12345678-1234-5678-1234-56789abcf010';
const String chrcMeasStatus = '12345678-1234-5678-1234-56789abcf011';
const String chrcPpgDecimate = '12345678-1234-5678-1234-56789abcf012';
const String chrcPmicCtrl = '12345678-1234-5678-1234-56789abcf013';
const String chrcAdminCtrl = '12345678-1234-5678-1234-56789abcf014';
const String chrcDeviceId = '12345678-1234-5678-1234-56789abcf015';
const String chrcPairingStatus = '12345678-1234-5678-1234-56789abcf016';
const String chrcScreenTimeout = '12345678-1234-5678-1234-56789abcf017';
const String chrcDeviceBuild = '12345678-1234-5678-1234-56789abcf01a';
const String chrcSamplingConfig = '12345678-1234-5678-1234-56789abcf01b';

const String chrcPmic = '12345678-1234-5678-1234-56789abcf101';
const String chrcTemperature = '12345678-1234-5678-1234-56789abcf102';
const String chrcGlucose = '12345678-1234-5678-1234-56789abcf105';
const String chrcPpgStream = '12345678-1234-5678-1234-56789abcf106';
const String chrcGlucoseSample = '12345678-1234-5678-1234-56789abcf107';
const String chrcGlucoseAlgo = '12345678-1234-5678-1234-56789abcf108';
const String chrcPmicExt = '12345678-1234-5678-1234-56789abcf109';
const String chrcProximity = '12345678-1234-5678-1234-56789abcf10a';
const String chrcVitals = '12345678-1234-5678-1234-56789abcf10b';
const String chrcAccelStream = '12345678-1234-5678-1234-56789abcf10c';
const String chrcSensorAll = '12345678-1234-5678-1234-56789abcf1ff';

const String chrcWifiEnable = '12345678-1234-5678-1234-56789abcf201';
const String chrcWifiSsid = '12345678-1234-5678-1234-56789abcf202';
const String chrcWifiPassword = '12345678-1234-5678-1234-56789abcf203';
const String chrcWifiStatus = '12345678-1234-5678-1234-56789abcf204';
const String chrcWifiConnect = '12345678-1234-5678-1234-56789abcf205';
const String chrcBulkSessionCtrl = '12345678-1234-5678-1234-56789abcf206';
const String chrcBulkSessionStatus = '12345678-1234-5678-1234-56789abcf207';

// Glucose model transfer service (...def3) — A/B model update over BLE.
const String svcModelTransfer = '12345678-1234-5678-1234-56789abcdef3';
const String chrcModelCtrl = '12345678-1234-5678-1234-56789abcf301';
const String chrcModelData = '12345678-1234-5678-1234-56789abcf302';
const String chrcModelStatus = '12345678-1234-5678-1234-56789abcf303';

// Model-transfer control commands (first byte of a write to chrcModelCtrl).
const int modelCmdBegin = 0x01;
const int modelCmdCommit = 0x02;
const int modelCmdAbort = 0x03;

// Resource store transfer service (...def5) — A/B fonts/icons/logos over BLE.
const String svcResourceTransfer = '12345678-1234-5678-1234-56789abcdef5';
const String chrcResourceCtrl = '12345678-1234-5678-1234-56789abcf501';
const String chrcResourceData = '12345678-1234-5678-1234-56789abcf502';
const String chrcResourceStatus = '12345678-1234-5678-1234-56789abcf503';

const int resourceCmdBegin = 0x01;
const int resourceCmdCommit = 0x02;
const int resourceCmdAbort = 0x03;

// NOR record sync service (...def4) — device → phone bulk pull.
const String svcRecordSync = '12345678-1234-5678-1234-56789abcdef4';
const String chrcRecordCtrl = '12345678-1234-5678-1234-56789abcf401';
const String chrcRecordData = '12345678-1234-5678-1234-56789abcf402';
const String chrcRecordStatus = '12345678-1234-5678-1234-56789abcf403';

const int recCmdStart = 0x01;
const int recCmdAck = 0x02;
const int recCmdAbort = 0x03;
const int recModeSummaryOnly = 0x01;
const int recModeFull = 0x02;

const int bulkSessionCmdStart = 0x01;
const int bulkSessionCmdAck = 0x02;
const int bulkSessionCmdAbort = 0x03;
const int bulkSessionFlagOtaModel = 0x01;
const int bulkSessionFlagOtaResource = 0x02;
const int bulkSessionFlagOtaFirmware = 0x04;
const int bulkSessionStatusStructSize = 18;

/// Wi-Fi bulk session states (device `wifi_bulk_status.state`).
const int bulkSessionStateIdle = 0;
const int bulkSessionStateProvisioning = 1;
const int bulkSessionStateAssociating = 2;
const int bulkSessionStateSessionReady = 3;
const int bulkSessionStateTransferring = 4;
const int bulkSessionStateCompleting = 5;
const int bulkSessionStateTeardown = 6;
const int bulkSessionStateError = 7;

const int recTypeGlucose = 1;
const int recTypeVitals = 2;
const int recTypeTemp = 3;
const int recTypeHome = 4;
const int recTypePpgRaw = 5;
const int recTypeGlucoseRaw = 6;

const String svcSmp = '8d53dc1d-1db7-4cd3-868b-8a527460aa84';
const String chrcSmp = 'da2e7828-fbce-4e01-ae9e-261174997c48';

final Set<String> encryptedBleCharacteristicsOpen = Set.unmodifiable({
  chrcWifiSsid,
  chrcWifiPassword,
  chrcPmicCtrl,
  chrcAdminCtrl,
  chrcSmp,
});

final Set<String> encryptedBleCharacteristicsSecureExtra = Set.unmodifiable({
  chrcDeviceName,
  chrcRtcTrim,
  chrcBatteryLow,
  chrcPpgPref,
  chrcPpgDecimate,
  chrcSamplingConfig,
  chrcMeasCtrl,
  chrcWifiEnable,
  chrcWifiConnect,
  chrcWifiStatus,
  chrcBulkSessionCtrl,
  chrcBulkSessionStatus,
  chrcPmic,
  chrcTemperature,
  chrcPmicExt,
  chrcProximity,
  chrcAccelStream,
  chrcRecordCtrl,
  chrcRecordStatus,
  chrcModelCtrl,
  chrcModelStatus,
  chrcResourceCtrl,
  chrcResourceStatus,
});

final Set<String> authenticatedBleCharacteristics = Set.unmodifiable({
  chrcWifiSsid,
  chrcWifiPassword,
  chrcPmicCtrl,
  chrcAdminCtrl,
  chrcSmp,
  chrcVitals,
  chrcGlucose,
  chrcGlucoseSample,
  chrcGlucoseAlgo,
  chrcPpgStream,
  chrcSensorAll,
  chrcBulkSessionCtrl,
  chrcBulkSessionStatus,
});

final Set<String> encryptedBleCharacteristics = encryptedBleCharacteristicsOpen;

final Set<String> _encryptedBleCharacteristicsSecure = Set.unmodifiable({
  ...encryptedBleCharacteristicsOpen,
  ...encryptedBleCharacteristicsSecureExtra,
});

Set<String> encryptedCharacteristicsForProfile(int profile) {
  if (profile == bleSecurityProfileSecure) {
    return _encryptedBleCharacteristicsSecure;
  }
  return encryptedBleCharacteristicsOpen;
}

final List<String> healthNotifyCharacteristics = List.unmodifiable([
  chrcVitals,
  chrcGlucose,
  chrcGlucoseSample,
  chrcGlucoseAlgo,
  chrcPpgStream,
  chrcSensorAll,
]);

final List<String> publicNotifyCharacteristics = List.unmodifiable([
  chrcMeasStatus,
]);

const int measCmdStart = 0x01;
const int measCmdStop = 0x02;
const int measCmdResetSync = 0x03;
const int measTypeHr = 0x01;
const int measTypeSpo2 = 0x02;
const int measTypeGlucose = 0x03;
const int measTypeVitals = 0x04;
const int measTypeAccel = 0x05;

/// Optional 3rd byte on Measurement Control writes (matches firmware BLE_MEAS_FLAG_*).
const int measFlagSkipProx = 0x01;

/// Matches device `CONFIG_UI_MEASURE_LONG_PRESS_MS` (display.conf).
const int measureLongPressMs = 5000;

const int pmicCmdEnable = 0x01;
const int pmicCmdDisable = 0x02;
const int pmicCmdSetVoltage = 0x03;
const int pmicTargetBk1 = 0x01;
const int pmicTargetBk2 = 0x02;
const int pmicTargetBk3 = 0x03;
const int pmicTargetBbout = 0x04;

const int adminCmdDeleteBonds = 0x01;

const Map<String, String> gattNames = {
  svcWearableConfig: 'Wearable Config Service',
  svcSensorData: 'Sensor Data Service',
  svcWifiConfig: 'WiFi Config Service',
  chrcDeviceName: 'Device Name',
  chrcRtcTrim: 'RTC Trim (PPM)',
  chrcBatteryLow: 'Battery Low Threshold',
  chrcBrightness: 'Display Backlight',
  chrcVolume: 'Buzzer Volume',
  chrcPpgPref: 'PPG Sensor Preference',
  chrcSecurityProfile: 'Security Profile',
  chrcDeviceId: 'Hardware Device ID',
  chrcPairingStatus: 'Pairing Status',
  chrcScreenTimeout: 'Screen Sleep Timeout',
  chrcDeviceBuild: 'Device Build',
  chrcMeasCtrl: 'Measurement Control',
  chrcMeasStatus: 'Measurement Status',
  chrcPpgDecimate: 'PPG Stream Decimation',
  chrcSamplingConfig: 'Sampling Config',
  chrcPmicCtrl: 'PMIC Control',
  chrcAdminCtrl: 'Admin Control',
  chrcPmic: 'PMIC Telemetry',
  chrcTemperature: 'Body Temperature',
  chrcVitals: 'Vitals (HR+SpO2+Hb+Resp)',
  chrcGlucose: 'Glucose',
  chrcGlucoseSample: 'Glucose Raw Sample',
  chrcGlucoseAlgo: 'Glucose Algorithm',
  chrcPmicExt: 'PMIC Extended',
  chrcProximity: 'Proximity / Wear State',
  chrcAccelStream: 'Accelerometer Stream',
  chrcPpgStream: 'PPG Raw Stream',
  chrcSensorAll: 'Combined Sensor Packet',
  chrcWifiEnable: 'WiFi Enable',
  chrcWifiSsid: 'WiFi SSID',
  chrcWifiPassword: 'WiFi Password',
  chrcWifiStatus: 'WiFi Status',
  chrcWifiConnect: 'WiFi Connect',
  chrcBulkSessionCtrl: 'Bulk Session Control',
  chrcBulkSessionStatus: 'Bulk Session Status',
  svcSmp: 'SMP Service (BLE DFU)',
  chrcSmp: 'SMP Characteristic',
  '00001800-0000-1000-8000-00805f9b34fb': 'Generic Access',
  '00001801-0000-1000-8000-00805f9b34fb': 'Generic Attribute',
  '00001805-0000-1000-8000-00805f9b34fb': 'Current Time Service',
  '0000180a-0000-1000-8000-00805f9b34fb': 'Device Information',
  '0000180f-0000-1000-8000-00805f9b34fb': 'Battery Service',
  '00002a00-0000-1000-8000-00805f9b34fb': 'Device Name',
  '00002a01-0000-1000-8000-00805f9b34fb': 'Appearance',
  '00002a05-0000-1000-8000-00805f9b34fb': 'Service Changed',
  '00002a19-0000-1000-8000-00805f9b34fb': 'Battery Level',
  '00002a2b-0000-1000-8000-00805f9b34fb': 'Current Time',
  '00002a29-0000-1000-8000-00805f9b34fb': 'Manufacturer Name',
  '00002a24-0000-1000-8000-00805f9b34fb': 'Model Number',
  '00002a26-0000-1000-8000-00805f9b34fb': 'Firmware Revision',
  '00002a28-0000-1000-8000-00805f9b34fb': 'Software Revision',
  '00002902-0000-1000-8000-00805f9b34fb': 'Client Characteristic Config (CCCD)',
  '00002901-0000-1000-8000-00805f9b34fb': 'Characteristic User Description',
};

String gattName(String uuid, {String fallback = ''}) {
  final normalized = uuid.toLowerCase();
  final name = gattNames[normalized];
  if (name != null) {
    return name;
  }
  if (fallback.isNotEmpty &&
      fallback != 'Unknown' &&
      fallback != 'Vendor specific') {
    return fallback;
  }
  if (normalized.endsWith('-0000-1000-8000-00805f9b34fb') &&
      normalized.startsWith('0000')) {
    return '0x${normalized.substring(4, 8).toUpperCase()}';
  }
  return 'Custom';
}

const int pairingStatusStructSize = 5;
const int pmicStructSize = 10;

/// v1 payload is 28 bytes; v2 appends VBAT/VCELL/flags (34 bytes total).
const int pmicExtStructSize = 28;
const int pmicExtStructSizeV2 = 34;
const int deviceBuildStructSize = 44;
const int deviceBuildStructSizeV2 = 64; // +5x uint32 per-type pending counts
const int deviceBuildStructSizeV3 = 72; // +dropped + crc_fail_count
const int temperatureStructSize = 6;
const int spo2StructSize = 7;
const int hrStructSize = 8;
const int vitalsStructSize = 26;
const int glucoseStructSize = 7;
const int glucoseAlgoStructSize = 124;
const int glucoseSampleStructSize = 12;
const int wifiStatusStructSize = 38;
const int measStatusStructSize = 8;
const int proximityStructSize = 10;
const int ppgSampleStructSize = 24;
const int accelSampleStructSize = 12;
const int sensorAllStructSize =
    pmicStructSize +
    temperatureStructSize +
    vitalsStructSize +
    glucoseStructSize +
    proximityStructSize;

class PairingStatus {
  const PairingStatus({required this.state, required this.passkey});

  final int state;
  final int passkey;
}

class PmicData {
  const PmicData({
    required this.battery_mv,
    required this.current_ma,
    required this.soc_percent,
    required this.charger_status,
    required this.buck1_mv,
    required this.buck2_mv,
  });

  final int battery_mv;
  final int current_ma;
  final int soc_percent;
  final int charger_status;
  final int buck1_mv;
  final int buck2_mv;
}

/// PMIC charger_status values (f101 byte 5, matches firmware ble_pmic_data).
const int pmicChargerIdle = 0;
const int pmicChargerCharging = 1;
const int pmicChargerFull = 2;

/// Li-ion VBAT floor (mV): below this with USB present ⇒ no cell / bench power.
const int pmicVbatExternalPowerMaxMv = 3000;

/// True when firmware reports no Li+ pack (PMIC+gauge detect or low VBAT).
bool isPmicExternalPowerOnly(int batteryMv, {int socPercent = -1}) {
  if (batteryMv <= 0) {
    return false;
  }
  /* Firmware clamps SOC to 0 when BatRegDone+idle or sub-UVLO (power_batt). */
  if (socPercent == 0) {
    return true;
  }
  return batteryMv < pmicVbatExternalPowerMaxMv;
}

String formatPmicChargerStatus(int status, {bool externalPowerOnly = false}) {
  if (externalPowerOnly) {
    return 'USB powered (no pack)';
  }
  switch (status) {
    case pmicChargerCharging:
      return 'Charging';
    case pmicChargerFull:
      return 'Full';
    case pmicChargerIdle:
    default:
      return 'Not charging';
  }
}

/// Short label for dashboard / GATT decode (VBAT sourced from PMIC internal ADC).
String describePmicBattery(PmicData p) {
  final external = isPmicExternalPowerOnly(p.battery_mv, socPercent: p.soc_percent);
  final vbat = 'VBAT ${p.battery_mv} mV (PMIC ADC)';
  final soc = external ? 'SOC clamped 0% (no pack)' : 'SOC ${p.soc_percent}%';
  return '$vbat · ${p.current_ma} mA · $soc · '
      '${formatPmicChargerStatus(p.charger_status, externalPowerOnly: external)}';
}

class PmicExtData {
  const PmicExtData({
    required this.buck3_mv,
    required this.bbout_mv,
    required this.buck1_enabled,
    required this.buck2_enabled,
    required this.buck3_enabled,
    required this.bbout_enabled,
    required this.charge_voltage_mv,
    required this.charge_current_ma,
    required this.battery_temp_c,
    required this.cycle_count,
    required this.remaining_mah,
    required this.full_mah,
    required this.design_mah,
    required this.time_to_empty_min,
    required this.time_to_full_min,
    required this.avg_current_ma,
    this.vbat_mv,
    this.vcell_mv,
    this.flags,
  });

  final int buck3_mv;
  final int bbout_mv;
  final int buck1_enabled;
  final int buck2_enabled;
  final int buck3_enabled;
  final int bbout_enabled;
  final int charge_voltage_mv;
  final int charge_current_ma;
  final int battery_temp_c;
  final int cycle_count;
  final int remaining_mah;
  final int full_mah;
  final int design_mah;
  final int time_to_empty_min;
  final int time_to_full_min;
  final int avg_current_ma;

  /// PMIC VBAT mV (f109 v2); null when payload is v1-only.
  final int? vbat_mv;

  /// Fuel-gauge VCELL mV (f109 v2).
  final int? vcell_mv;

  /// bit0 usb, bit1 bat_good, bit2 bat_reg_done, bit3 sys_bat_lim, bits4–5 cell.
  final int? flags;

  bool get usbOnline => ((flags ?? 0) & 0x01) != 0;
  bool get batGood => ((flags ?? 0) & 0x02) != 0;
  bool get batRegDone => ((flags ?? 0) & 0x04) != 0;
  bool get sysBatLim => ((flags ?? 0) & 0x08) != 0;

  /// 0=absent, 1=present, 2=unknown (v2); null if v1.
  int? get cellState => flags == null ? null : ((flags! >> 4) & 0x3);
}

class DeviceBuildData {
  const DeviceBuildData({
    required this.fwVersion,
    required this.gitHash,
    required this.uptimeS,
    required this.pendingRecords,
    required this.deviceId,
    this.pendingGlucose = 0,
    this.pendingVitals = 0,
    this.pendingTemp = 0,
    this.pendingPpgRaw = 0,
    this.pendingGlucoseRaw = 0,
    this.dropped = 0,
    this.crcFailCount = 0,
  });

  final String fwVersion;
  final String gitHash;
  final int uptimeS;
  final int pendingRecords;
  final List<int> deviceId;

  /// Per-type breakdown of [pendingRecords]. Zero on firmware builds older
  /// than the 64-byte device-build struct (only the 44-byte base is read).
  final int pendingGlucose;
  final int pendingVitals;
  final int pendingTemp;
  final int pendingPpgRaw;
  final int pendingGlucoseRaw;

  /// v3: unsynced records dropped when the NOR ring was full.
  final int dropped;

  /// v3: CRC mismatches skipped while reading the record store.
  final int crcFailCount;

  String get deviceIdHex =>
      deviceId.map((b) => b.toRadixString(16).padLeft(2, '0')).join();
}

/// BLE f01b sampling / schedule knobs (14 bytes LE; legacy 10/12 accepted on decode).
class SamplingConfigData {
  const SamplingConfigData({
    required this.ppgSampleCount,
    required this.glucoseNumSamples,
    required this.glucoseDelayMs,
    this.disableProximity = false,
    this.autoEnabled = true,
    this.scheduleIntervalSec = 0,
    this.currentIntervalSec = 600,
    this.tempIdleIntervalSec = 60,
  });

  final int ppgSampleCount;
  final int glucoseNumSamples;
  final int glucoseDelayMs;

  /// When true, firmware ignores VCNL3040 wear gating (bench / sched test).
  final bool disableProximity;

  /// Health auto-sched on/off.
  final bool autoEnabled;

  /// Fixed interval seconds (0 = adaptive ladder).
  final int scheduleIntervalSec;

  /// Effective interval now (ladder step or fixed). Always set on device read.
  final int currentIntervalSec;

  /// Idle wrist/SoC temp log period (seconds). Part of measure scheduling UX.
  final int tempIdleIntervalSec;

  bool get isAdaptive => scheduleIntervalSec == 0;

  /// Approximate window at the usual 25 Hz PPG rate.
  double get ppgSecondsApprox => ppgSampleCount / 25.0;
}

/// f01b flags bit0 — persistent PPG proximity disable.
const int sampFlagDisableProx = 0x01;

class TemperatureData {
  const TemperatureData({required this.temp_c, required this.timestamp});

  final double temp_c;
  final int timestamp;
}

class SpO2Data {
  const SpO2Data({
    required this.spo2_percent,
    required this.confidence,
    required this.finger_detected,
    required this.timestamp,
  });

  final int spo2_percent;
  final int confidence;
  final bool finger_detected;
  final int timestamp;
}

class HrData {
  const HrData({
    required this.hr_bpm,
    required this.confidence,
    required this.finger_detected,
    required this.timestamp,
  });

  final int hr_bpm;
  final int confidence;
  final bool finger_detected;
  final int timestamp;
}

class VitalsData {
  const VitalsData({
    required this.hr_bpm,
    required this.hr_confidence,
    required this.spo2_percent,
    required this.spo2_confidence,
    required this.hb_g_dl_x10,
    required this.hb_confidence,
    required this.resp_rate_bpm,
    required this.resp_confidence,
    required this.r_value_x1000,
    required this.quality,
    required this.flags,
    required this.timestamp,
    this.sdnn_ms = 0,
    this.rmssd_ms = 0,
    this.systolic_mmhg = 0,
    this.diastolic_mmhg = 0,
  });

  final int hr_bpm;
  final int hr_confidence;
  final int spo2_percent;
  final int spo2_confidence;
  final int hb_g_dl_x10;
  final int hb_confidence;
  final int resp_rate_bpm;
  final int resp_confidence;
  final int r_value_x1000;
  final int quality;
  final int flags;
  final int timestamp;
  final int sdnn_ms;
  final int rmssd_ms;
  final int systolic_mmhg;
  final int diastolic_mmhg;

  bool get hr_valid => (flags & 0x01) != 0;
  bool get spo2_valid => (flags & 0x02) != 0;
  bool get hb_valid => (flags & 0x04) != 0;
  bool get resp_valid => (flags & 0x08) != 0;
  bool get finger_on => (flags & 0x10) != 0;
  bool get hrv_valid => (flags & 0x20) != 0;
  bool get bp_valid => (flags & 0x40) != 0;
  double get hb_g_dl => _roundTo(hb_g_dl_x10 / 10.0, 1);

  int get hrBpm => hr_bpm;
  int get hrConf => hr_confidence;
  int get spo2Pct => spo2_percent;
  int get spo2Conf => spo2_confidence;
  int get hbX10 => hb_g_dl_x10;
  int get hbConf => hb_confidence;
  int get respBpm => resp_rate_bpm;
  int get respConf => resp_confidence;
  int get rVal => r_value_x1000;
  double get hbGdl => hb_g_dl;
}

class GlucoseData {
  const GlucoseData({
    required this.glucose_mg_dl,
    required this.quality,
    required this.timestamp,
  });

  final int glucose_mg_dl;
  final int quality;
  final int timestamp;

  double get glucose_mmol_l => _roundTo(glucose_mg_dl / 18.0182, 2);
}

class GlucoseSampleData {
  const GlucoseSampleData({
    required this.sample_number,
    required this.total_samples,
    required this.raw_adc_value,
    required this.voltage_mv,
    required this.timestamp,
  });

  final int sample_number;
  final int total_samples;
  final int raw_adc_value;
  final double voltage_mv;
  final int timestamp;
}

class GlucoseAlgoData {
  const GlucoseAlgoData({
    required this.tot_coeff,
    required this.intercept,
    required this.y1_value,
    required this.avg_val,
    required this.std_dev,
    required this.up_lim,
    required this.ll_lim,
    required this.p_count,
    required this.n_count,
    required this.p_val,
    required this.n_val,
    required this.p_plus_n,
    required this.y2_val,
    required this.y2_percent,
    required this.group_cd,
    required this.y2_factor,
    required this.y2_factor_val,
    required this.const_val,
    required this.y3_value,
    required this.y3_row_no,
    required this.elim_per,
    required this.elim_val,
    required this.y_value,
    required this.calibration_factor,
    required this.ag_adjusted,
    required this.normalized_glucose,
    required this.actual_insulin,
    required this.insulin_correction,
    required this.insulin_ratio,
    required this.inverse_ratio,
    required this.homa_ir_index,
  });

  final double tot_coeff;
  final double intercept;
  final double y1_value;
  final double avg_val;
  final double std_dev;
  final double up_lim;
  final double ll_lim;
  final int p_count;
  final int n_count;
  final double p_val;
  final double n_val;
  final double p_plus_n;
  final double y2_val;
  final double y2_percent;
  final int group_cd;
  final double y2_factor;
  final double y2_factor_val;
  final double const_val;
  final double y3_value;
  final int y3_row_no;
  final double elim_per;
  final double elim_val;
  final int y_value;
  final double calibration_factor;
  final double ag_adjusted;
  final double normalized_glucose;
  final double actual_insulin;
  final double insulin_correction;
  final double insulin_ratio;
  final double inverse_ratio;
  final double homa_ir_index;
}

class WifiStatus {
  const WifiStatus({
    required this.connected,
    required this.rssi_dbm,
    required this.ip_addr,
    required this.ssid,
  });

  final int connected;
  final int rssi_dbm;
  final String ip_addr;
  final String ssid;
}

class BulkSessionStart {
  const BulkSessionStart({
    required this.host,
    required this.port,
    required this.token,
    required this.afterId,
    required this.mode,
    this.flags = 0,
  });

  final String host;
  final int port;
  final String token;
  final int afterId;
  final int mode;
  final int flags;
}

class BulkSessionStatus {
  const BulkSessionStatus({
    required this.state,
    required this.error,
    required this.pending,
    required this.cursorId,
    required this.ackId,
    required this.sentCount,
  });

  final int state;
  final int error;
  final int pending;
  final int cursorId;
  final int ackId;
  final int sentCount;

  int get upToId => ackId != 0 ? ackId : cursorId;
}

class MeasStatus {
  const MeasStatus({
    required this.active,
    required this.meas_type,
    required this.percent_complete,
    required this.quality,
    required this.samples_taken,
    required this.samples_target,
  });

  final bool active;
  final int meas_type;
  final int percent_complete;
  final int quality;
  final int samples_taken;
  final int samples_target;
}

class ProximityStatus {
  const ProximityStatus({
    required this.contact,
    required this.wear_state,
    required this.proximity_raw,
    required this.proximity_filt,
    required this.timestamp,
  });

  final bool contact;
  final int wear_state;
  final int proximity_raw;
  final int proximity_filt;
  final int timestamp;
}

class PpgSample {
  const PpgSample({
    required this.sample_num,
    required this.raw_ir,
    required this.raw_red,
    required this.raw_green,
    required this.accel_x,
    required this.accel_y,
    required this.accel_z,
    required this.timestamp_ms,
  });

  final int sample_num;
  final int raw_ir;
  final int raw_red;
  final int raw_green;
  final int accel_x;
  final int accel_y;
  final int accel_z;
  final int timestamp_ms;
}

class AccelSample {
  const AccelSample({
    required this.seq,
    required this.x_mg,
    required this.y_mg,
    required this.z_mg,
    required this.timestamp_ms,
  });

  final int seq;
  final int x_mg;
  final int y_mg;
  final int z_mg;
  final int timestamp_ms;

  double get magnitude_mg =>
      _roundTo(math.sqrt(x_mg * x_mg + y_mg * y_mg + z_mg * z_mg), 1);
}

class SensorAllData {
  const SensorAllData({
    required this.pmic,
    required this.temperature,
    required this.vitals,
    required this.glucose,
    required this.proximity,
  });

  final PmicData pmic;
  final TemperatureData temperature;
  final VitalsData vitals;
  final GlucoseData glucose;
  final ProximityStatus proximity;
}

PmicData? decodePmic(List<int> data) {
  if (data.length < pmicStructSize) {
    return null;
  }
  final view = _asByteData(data);
  return PmicData(
    battery_mv: view.getUint16(0, Endian.little),
    current_ma: view.getInt16(2, Endian.little),
    soc_percent: view.getUint8(4),
    charger_status: view.getUint8(5),
    buck1_mv: view.getUint16(6, Endian.little),
    buck2_mv: view.getUint16(8, Endian.little),
  );
}

PmicExtData? decodePmicExt(List<int> data) {
  if (data.length < pmicExtStructSize) {
    return null;
  }
  final view = _asByteData(data);
  final hasV2 = data.length >= pmicExtStructSizeV2;
  return PmicExtData(
    buck3_mv: view.getUint16(0, Endian.little),
    bbout_mv: view.getUint16(2, Endian.little),
    buck1_enabled: view.getUint8(4),
    buck2_enabled: view.getUint8(5),
    buck3_enabled: view.getUint8(6),
    bbout_enabled: view.getUint8(7),
    charge_voltage_mv: view.getUint16(8, Endian.little),
    charge_current_ma: view.getUint16(10, Endian.little),
    battery_temp_c: view.getInt16(12, Endian.little),
    cycle_count: view.getUint16(14, Endian.little),
    remaining_mah: view.getUint16(16, Endian.little),
    full_mah: view.getUint16(18, Endian.little),
    design_mah: view.getUint16(20, Endian.little),
    time_to_empty_min: view.getUint16(22, Endian.little),
    time_to_full_min: view.getUint16(24, Endian.little),
    avg_current_ma: view.getInt16(26, Endian.little),
    vbat_mv: hasV2 ? view.getUint16(28, Endian.little) : null,
    vcell_mv: hasV2 ? view.getUint16(30, Endian.little) : null,
    flags: hasV2 ? view.getUint8(32) : null,
  );
}

String _cStringAt(ByteData view, int offset, int maxLen) {
  final bytes = <int>[];
  for (var i = 0; i < maxLen; i++) {
    final b = view.getUint8(offset + i);
    if (b == 0) break;
    bytes.add(b);
  }
  return String.fromCharCodes(bytes);
}

DeviceBuildData? decodeDeviceBuild(List<int> data) {
  if (data.length < deviceBuildStructSize) {
    return null;
  }
  final view = _asByteData(data);
  final hasPerType = data.length >= deviceBuildStructSizeV2;
  final hasV3 = data.length >= deviceBuildStructSizeV3;
  return DeviceBuildData(
    fwVersion: _cStringAt(view, 0, 16),
    gitHash: _cStringAt(view, 16, 12),
    uptimeS: view.getUint32(28, Endian.little),
    pendingRecords: view.getUint32(32, Endian.little),
    deviceId: List<int>.generate(8, (i) => view.getUint8(36 + i)),
    pendingGlucose: hasPerType ? view.getUint32(44, Endian.little) : 0,
    pendingVitals: hasPerType ? view.getUint32(48, Endian.little) : 0,
    pendingTemp: hasPerType ? view.getUint32(52, Endian.little) : 0,
    pendingPpgRaw: hasPerType ? view.getUint32(56, Endian.little) : 0,
    pendingGlucoseRaw: hasPerType ? view.getUint32(60, Endian.little) : 0,
    dropped: hasV3 ? view.getUint32(64, Endian.little) : 0,
    crcFailCount: hasV3 ? view.getUint32(68, Endian.little) : 0,
  );
}

const int samplingConfigStructSize = 14;

SamplingConfigData? decodeSamplingConfig(List<int> data) {
  if (data.length < 8) {
    return null;
  }
  final view = _asByteData(data);
  final flags = view.getUint8(6);
  final autoEnabled = data.length < 10 ? true : view.getUint8(7) != 0;
  final scheduleIntervalSec = data.length < 10
      ? 0
      : view.getUint16(8, Endian.little);
  final currentIntervalSec = data.length >= 12
      ? view.getUint16(10, Endian.little)
      : (scheduleIntervalSec == 0 ? 600 : scheduleIntervalSec);
  final tempIdleIntervalSec = data.length >= 14
      ? view.getUint16(12, Endian.little)
      : 60;
  return SamplingConfigData(
    ppgSampleCount: view.getUint16(0, Endian.little),
    glucoseNumSamples: view.getUint16(2, Endian.little),
    glucoseDelayMs: view.getUint16(4, Endian.little),
    disableProximity: (flags & sampFlagDisableProx) != 0,
    autoEnabled: autoEnabled,
    scheduleIntervalSec: scheduleIntervalSec,
    currentIntervalSec: currentIntervalSec == 0 ? 600 : currentIntervalSec,
    tempIdleIntervalSec: tempIdleIntervalSec == 0 ? 60 : tempIdleIntervalSec,
  );
}

Uint8List encodeSamplingConfig(SamplingConfigData cfg) {
  final bytes = Uint8List(samplingConfigStructSize);
  final view = ByteData.sublistView(bytes);
  view.setUint16(0, cfg.ppgSampleCount, Endian.little);
  view.setUint16(2, cfg.glucoseNumSamples, Endian.little);
  view.setUint16(4, cfg.glucoseDelayMs, Endian.little);
  view.setUint8(6, cfg.disableProximity ? sampFlagDisableProx : 0);
  view.setUint8(7, cfg.autoEnabled ? 1 : 0);
  view.setUint16(8, cfg.scheduleIntervalSec, Endian.little);
  // Device ignores this on write; send current for struct size alignment.
  view.setUint16(10, cfg.currentIntervalSec, Endian.little);
  view.setUint16(12, cfg.tempIdleIntervalSec, Endian.little);
  return bytes;
}

TemperatureData? decodeTemperature(List<int> data) {
  if (data.length < temperatureStructSize) {
    return null;
  }
  final view = _asByteData(data);
  final raw = view.getInt16(0, Endian.little);
  final timestamp = view.getUint32(2, Endian.little);
  return TemperatureData(
    temp_c: _roundTo(raw / 100.0, 2),
    timestamp: timestamp,
  );
}

SpO2Data? decodeSpo2(List<int> data) {
  if (data.length < spo2StructSize) {
    return null;
  }
  final view = _asByteData(data);
  return SpO2Data(
    spo2_percent: view.getUint8(0),
    confidence: view.getUint8(1),
    finger_detected: view.getUint8(2) != 0,
    timestamp: view.getUint32(3, Endian.little),
  );
}

HrData? decodeHr(List<int> data) {
  if (data.length < hrStructSize) {
    return null;
  }
  final view = _asByteData(data);
  return HrData(
    hr_bpm: view.getUint16(0, Endian.little),
    confidence: view.getUint8(2),
    finger_detected: view.getUint8(3) != 0,
    timestamp: view.getUint32(4, Endian.little),
  );
}

VitalsData? decodeVitals(List<int> data) {
  // Accept legacy 18-byte payloads and current 26-byte (with HRV/BP).
  if (data.length < 18) {
    return null;
  }
  final view = _asByteData(data);
  var offset = 0;
  final hrBpm = view.getUint16(offset, Endian.little);
  offset += 2;
  final hrConf = view.getUint8(offset++);
  final spo2Pct = view.getUint8(offset++);
  final spo2Conf = view.getUint8(offset++);
  final hbX10 = view.getUint16(offset, Endian.little);
  offset += 2;
  final hbConf = view.getUint8(offset++);
  final respBpm = view.getUint8(offset++);
  final respConf = view.getUint8(offset++);
  final rVal = view.getUint16(offset, Endian.little);
  offset += 2;
  final quality = view.getUint8(offset++);
  final flags = view.getUint8(offset++);
  final timestamp = view.getUint32(offset, Endian.little);
  offset += 4;
  var sdnn = 0;
  var rmssd = 0;
  var sys = 0;
  var dia = 0;
  if (data.length >= vitalsStructSize) {
    sdnn = view.getUint16(offset, Endian.little);
    offset += 2;
    rmssd = view.getUint16(offset, Endian.little);
    offset += 2;
    sys = view.getUint16(offset, Endian.little);
    offset += 2;
    dia = view.getUint16(offset, Endian.little);
  }
  return VitalsData(
    hr_bpm: hrBpm,
    hr_confidence: hrConf,
    spo2_percent: spo2Pct,
    spo2_confidence: spo2Conf,
    hb_g_dl_x10: hbX10,
    hb_confidence: hbConf,
    resp_rate_bpm: respBpm,
    resp_confidence: respConf,
    r_value_x1000: rVal,
    quality: quality,
    flags: flags,
    timestamp: timestamp,
    sdnn_ms: sdnn,
    rmssd_ms: rmssd,
    systolic_mmhg: sys,
    diastolic_mmhg: dia,
  );
}

GlucoseData? decodeGlucose(List<int> data) {
  if (data.length < glucoseStructSize) {
    return null;
  }
  final view = _asByteData(data);
  return GlucoseData(
    glucose_mg_dl: view.getUint16(0, Endian.little),
    quality: view.getUint8(2),
    timestamp: view.getUint32(3, Endian.little),
  );
}

GlucoseSampleData? decodeGlucoseSample(List<int> data) {
  if (data.length < glucoseSampleStructSize) {
    return null;
  }
  final view = _asByteData(data);
  final voltageX100 = view.getInt16(6, Endian.little);
  return GlucoseSampleData(
    sample_number: view.getUint16(0, Endian.little),
    total_samples: view.getUint16(2, Endian.little),
    raw_adc_value: view.getUint16(4, Endian.little),
    voltage_mv: voltageX100 / 100.0,
    timestamp: view.getUint32(8, Endian.little),
  );
}

GlucoseAlgoData? decodeGlucoseAlgo(List<int> data) {
  if (data.length < glucoseAlgoStructSize) {
    return null;
  }
  final view = _asByteData(data);
  var offset = 0;

  double readF32() {
    final value = view.getFloat32(offset, Endian.little);
    offset += 4;
    return value;
  }

  int readI32() {
    final value = view.getInt32(offset, Endian.little);
    offset += 4;
    return value;
  }

  return GlucoseAlgoData(
    tot_coeff: readF32(),
    intercept: readF32(),
    y1_value: readF32(),
    avg_val: readF32(),
    std_dev: readF32(),
    up_lim: readF32(),
    ll_lim: readF32(),
    p_count: readI32(),
    n_count: readI32(),
    p_val: readF32(),
    n_val: readF32(),
    p_plus_n: readF32(),
    y2_val: readF32(),
    y2_percent: readF32(),
    group_cd: readI32(),
    y2_factor: readF32(),
    y2_factor_val: readF32(),
    const_val: readF32(),
    y3_value: readF32(),
    y3_row_no: readI32(),
    elim_per: readF32(),
    elim_val: readF32(),
    y_value: readI32(),
    calibration_factor: readF32(),
    ag_adjusted: readF32(),
    normalized_glucose: readF32(),
    actual_insulin: readF32(),
    insulin_correction: readF32(),
    insulin_ratio: readF32(),
    inverse_ratio: readF32(),
    homa_ir_index: readF32(),
  );
}

WifiStatus? decodeWifiStatus(List<int> data) {
  if (data.length < wifiStatusStructSize) {
    return null;
  }
  final bytes = _asUint8List(data);
  final view = ByteData.sublistView(bytes);
  final ip = '${bytes[2]}.${bytes[3]}.${bytes[4]}.${bytes[5]}';
  var end = 38;
  while (end > 6 && bytes[end - 1] == 0) {
    end--;
  }
  final ssid = utf8.decode(bytes.sublist(6, end), allowMalformed: true);
  return WifiStatus(
    connected: view.getUint8(0),
    rssi_dbm: view.getInt8(1),
    ip_addr: ip,
    ssid: ssid,
  );
}

MeasStatus? decodeMeasStatus(List<int> data) {
  if (data.length < measStatusStructSize) {
    return null;
  }
  final view = _asByteData(data);
  return MeasStatus(
    active: view.getUint8(0) != 0,
    meas_type: view.getUint8(1),
    percent_complete: view.getUint8(2),
    quality: view.getUint8(3),
    samples_taken: view.getUint16(4, Endian.little),
    samples_target: view.getUint16(6, Endian.little),
  );
}

ProximityStatus? decodeProximityStatus(List<int> data) {
  if (data.length < proximityStructSize) {
    return null;
  }
  final view = _asByteData(data);
  return ProximityStatus(
    contact: view.getUint8(0) != 0,
    wear_state: view.getUint8(1),
    proximity_raw: view.getUint16(2, Endian.little),
    proximity_filt: view.getUint16(4, Endian.little),
    timestamp: view.getUint32(6, Endian.little),
  );
}

PpgSample? decodePpgSample(List<int> data) {
  if (data.length < ppgSampleStructSize) {
    return null;
  }
  final view = _asByteData(data);
  return PpgSample(
    sample_num: view.getUint16(0, Endian.little),
    raw_ir: view.getUint32(2, Endian.little),
    raw_red: view.getUint32(6, Endian.little),
    raw_green: view.getUint32(10, Endian.little),
    accel_x: view.getInt16(14, Endian.little),
    accel_y: view.getInt16(16, Endian.little),
    accel_z: view.getInt16(18, Endian.little),
    timestamp_ms: view.getUint32(20, Endian.little),
  );
}

AccelSample? decodeAccelSample(List<int> data) {
  if (data.length < accelSampleStructSize) {
    return null;
  }
  final view = _asByteData(data);
  return AccelSample(
    seq: view.getUint16(0, Endian.little),
    x_mg: view.getInt16(2, Endian.little),
    y_mg: view.getInt16(4, Endian.little),
    z_mg: view.getInt16(6, Endian.little),
    timestamp_ms: view.getUint32(8, Endian.little),
  );
}

int? decodeSecurityProfile(List<int> data) {
  if (data.isEmpty) {
    return null;
  }
  final value = data[0];
  if (value != bleSecurityProfileOpen && value != bleSecurityProfileSecure) {
    return null;
  }
  return value;
}

/** SIG Battery Level (0x2A19): single uint8 0–100%. */
int? decodeBatteryLevel(List<int> data) {
  if (data.isEmpty) {
    return null;
  }
  return data[0].clamp(0, 100);
}

PairingStatus? decodePairingStatus(List<int> data) {
  if (data.length < pairingStatusStructSize) {
    return null;
  }
  final view = _asByteData(data);
  final state = view.getUint8(0);
  if (state > blePairingFailed) {
    return null;
  }
  return PairingStatus(state: state, passkey: view.getUint32(1, Endian.little));
}

SensorAllData? decodeSensorAll(List<int> data) {
  if (data.length < sensorAllStructSize) {
    return null;
  }
  var offset = 0;
  final pmic = decodePmic(data.sublist(offset, offset + pmicStructSize));
  offset += pmicStructSize;
  final temperature = decodeTemperature(
    data.sublist(offset, offset + temperatureStructSize),
  );
  offset += temperatureStructSize;
  final vitals = decodeVitals(data.sublist(offset, offset + vitalsStructSize));
  offset += vitalsStructSize;
  final glucose = decodeGlucose(
    data.sublist(offset, offset + glucoseStructSize),
  );
  offset += glucoseStructSize;
  final proximity = decodeProximityStatus(
    data.sublist(offset, offset + proximityStructSize),
  );
  if (pmic == null ||
      temperature == null ||
      vitals == null ||
      glucose == null ||
      proximity == null) {
    return null;
  }
  return SensorAllData(
    pmic: pmic,
    temperature: temperature,
    vitals: vitals,
    glucose: glucose,
    proximity: proximity,
  );
}

Uint8List encodeMeasCtrl(int cmd, [int measType = 0, int flags = 0]) {
  if (flags == 0) {
    return Uint8List.fromList([cmd & 0xFF, measType & 0xFF]);
  }
  return Uint8List.fromList([cmd & 0xFF, measType & 0xFF, flags & 0xFF]);
}

/// Protocol 3.2 f206 session start:
/// cmd,u8 mode,u16 flags,u16 port,u32 after_id,u8 token_len,u8 host_len,token,host.
Uint8List encodeBulkSessionStart(BulkSessionStart session) {
  final token = utf8.encode(session.token);
  final host = utf8.encode(session.host);
  if (token.length > 255) {
    throw ArgumentError.value(session.token, 'token', 'token too long');
  }
  if (host.length > 255) {
    throw ArgumentError.value(session.host, 'host', 'host too long');
  }
  final bytes = Uint8List(12 + token.length + host.length);
  final view = ByteData.sublistView(bytes);
  view.setUint8(0, bulkSessionCmdStart);
  view.setUint8(1, session.mode & 0xFF);
  view.setUint16(2, session.flags & 0xFFFF, Endian.little);
  view.setUint16(4, session.port & 0xFFFF, Endian.little);
  view.setUint32(6, session.afterId, Endian.little);
  view.setUint8(10, token.length);
  view.setUint8(11, host.length);
  bytes.setRange(12, 12 + token.length, token);
  bytes.setRange(12 + token.length, bytes.length, host);
  return bytes;
}

BulkSessionStart? decodeBulkSessionStart(List<int> data) {
  if (data.length < 12 || data[0] != bulkSessionCmdStart) {
    return null;
  }
  final bytes = _asUint8List(data);
  final view = ByteData.sublistView(bytes);
  final tokenLen = view.getUint8(10);
  final hostLen = view.getUint8(11);
  if (bytes.length < 12 + tokenLen + hostLen) {
    return null;
  }
  final token = utf8.decode(bytes.sublist(12, 12 + tokenLen));
  final host = utf8.decode(
    bytes.sublist(12 + tokenLen, 12 + tokenLen + hostLen),
  );
  return BulkSessionStart(
    host: host,
    port: view.getUint16(4, Endian.little),
    token: token,
    afterId: view.getUint32(6, Endian.little),
    mode: view.getUint8(1),
    flags: view.getUint16(2, Endian.little),
  );
}

/// Wi-Fi bulk ACK: `[cmd][up_to:u32][mode:u8]`. Mode defaults to FULL.
Uint8List encodeBulkSessionAck(int upToId, {int mode = recModeFull}) {
  final bytes = Uint8List(6);
  final view = ByteData.sublistView(bytes);
  view.setUint8(0, bulkSessionCmdAck);
  view.setUint32(1, upToId, Endian.little);
  view.setUint8(5, mode);
  return bytes;
}

Uint8List encodeBulkSessionAbort() {
  return Uint8List.fromList([bulkSessionCmdAbort]);
}

BulkSessionStatus? decodeBulkSessionStatus(List<int> data) {
  if (data.length < bulkSessionStatusStructSize) {
    return null;
  }
  final view = _asByteData(data);
  return BulkSessionStatus(
    state: view.getUint8(0),
    error: view.getInt8(1),
    pending: view.getUint32(2, Endian.little),
    cursorId: view.getUint32(6, Endian.little),
    ackId: view.getUint32(10, Endian.little),
    sentCount: view.getUint32(14, Endian.little),
  );
}

Uint8List encodePmicCtrl(int cmd, int target, [int? valueMv]) {
  if (valueMv == null) {
    return Uint8List.fromList([cmd & 0xFF, target & 0xFF]);
  }
  final bytes = Uint8List(4);
  final view = ByteData.sublistView(bytes);
  view.setUint8(0, cmd & 0xFF);
  view.setUint8(1, target & 0xFF);
  view.setUint16(2, valueMv & 0xFFFF, Endian.little);
  return bytes;
}

/// Encode Bluetooth SIG Current Time (0x2A2B), 10 bytes.
/// [local] wall-clock fields; weekday uses Dart 1=Mon…7=Sun (same as CTS).
/// [reason] adjust-reason bits (default: external reference).
Uint8List encodeCurrentTime(DateTime local, {int reason = 0x02}) {
  final bytes = Uint8List(10);
  final view = ByteData.sublistView(bytes);
  view.setUint16(0, local.year, Endian.little);
  view.setUint8(2, local.month);
  view.setUint8(3, local.day);
  view.setUint8(4, local.hour);
  view.setUint8(5, local.minute);
  view.setUint8(6, local.second);
  view.setUint8(7, local.weekday); // 1=Mon … 7=Sun
  view.setUint8(8, 0); // fractions256
  view.setUint8(9, reason & 0xFF);
  return bytes;
}

Uint8List encodeRtcTrim(int ppm) {
  final bytes = Uint8List(4);
  ByteData.sublistView(bytes).setInt32(0, ppm, Endian.little);
  return bytes;
}

Uint8List encodeBatteryLow(int thresholdMv) {
  final bytes = Uint8List(2);
  ByteData.sublistView(bytes).setUint16(0, thresholdMv, Endian.little);
  return bytes;
}

Uint8List encodeUint8(int value) {
  return Uint8List.fromList([value & 0xFF]);
}

Uint8List encodeString(String s, int maxLen) {
  final encoded = utf8.encode(s);
  final limit = math.min(encoded.length, maxLen);
  return Uint8List.fromList(encoded.sublist(0, limit));
}

const Map<int, String> measTypeNames = {
  measTypeHr: 'HR',
  measTypeSpo2: 'SpO2',
  measTypeGlucose: 'Glucose',
  measTypeVitals: 'Vitals',
  measTypeAccel: 'Accel (deprecated)',
};

/// nRF Connect-style dashed uppercase hex (e.g. `F6-09-00-00`).
String formatGattHex(List<int> data) {
  if (data.isEmpty) {
    return '(empty)';
  }
  return data
      .map((b) => b.toRadixString(16).padLeft(2, '0').toUpperCase())
      .join('-');
}

/// Human-readable decode for GATT explorer (empty when unknown).
String describeGattPayload(String uuid, List<int> data) {
  final u = uuid.toLowerCase();
  if (u == chrcPmic.toLowerCase()) {
    final p = decodePmic(data);
    if (p == null) return '';
    return '${describePmicBattery(p)}  '
        'BK1=${p.buck1_mv} mV  BK2=${p.buck2_mv} mV';
  }
  if (u == chrcPmicExt.toLowerCase()) {
    final p = decodePmicExt(data);
    if (p == null) return '';
    final v2 = (p.vbat_mv != null)
        ? '  VBAT=${p.vbat_mv} VCELL=${p.vcell_mv} flags=0x${(p.flags ?? 0).toRadixString(16)}'
        : '';
    return 'BK3=${p.buck3_mv} mV  BBOUT=${p.bbout_mv} mV  '
        'en BK1/2/3/BB=${p.buck1_enabled}/${p.buck2_enabled}/${p.buck3_enabled}/${p.bbout_enabled}  '
        'chg ${p.charge_voltage_mv} mV / ${p.charge_current_ma} mA  '
        'Tbat=${p.battery_temp_c} °C  rem=${p.remaining_mah}/${p.full_mah} mAh$v2';
  }
  if (u == chrcDeviceBuild.toLowerCase()) {
    final b = decodeDeviceBuild(data);
    if (b == null) return '';
    return 'FW ${b.fwVersion} #${b.gitHash}  up=${b.uptimeS}s  '
        'pend=${b.pendingRecords}  id=${b.deviceIdHex}';
  }
  if (u == chrcVitals.toLowerCase()) {
    final v = decodeVitals(data);
    if (v == null) return '';
    return 'HR=${v.hrBpm} SpO2=${v.spo2Pct}% temp path N/A';
  }
  if (u == chrcTemperature.toLowerCase()) {
    final t = decodeTemperature(data);
    if (t == null) return '';
    return '${t.temp_c} °C  ts=${t.timestamp}';
  }
  if (u == chrcGlucose.toLowerCase()) {
    final g = decodeGlucose(data);
    if (g == null) return '';
    return '${g.glucose_mg_dl} mg/dL  (${g.glucose_mmol_l.toStringAsFixed(2)} mmol/L)';
  }
  if (u == chrcSecurityProfile.toLowerCase()) {
    return decodeSecurityProfile(data) == bleSecurityProfileSecure
        ? 'SECURE'
        : 'OPEN';
  }
  if (u == chrcPairingStatus.toLowerCase()) {
    final s = decodePairingStatus(data);
    if (s == null) return '';
    return 'state=${s.state} passkey=${s.passkey.toString().padLeft(6, '0')}';
  }
  return '';
}

Uint8List _asUint8List(List<int> data) {
  if (data is Uint8List) {
    return data;
  }
  return Uint8List.fromList(data);
}

ByteData _asByteData(List<int> data) {
  return ByteData.sublistView(_asUint8List(data));
}

double _roundTo(double value, int places) {
  final factor = math.pow(10, places).toDouble();
  return (value * factor).round() / factor;
}

// =============================================================================
// NOR record-store payload decoders (see include/record_store.h on firmware).
//
// Single source of truth for interpreting `SyncedRecord.payload` bytes —
// used by the local xlsx exporter, the "All Records" browser, per-metric
// history, and the server push mapper. Byte offsets mirror the packed C
// structs exactly; keep in sync if the firmware structs change.
// =============================================================================

/// RECORD_TYPE_GLUCOSE summary (`struct rec_glucose`, 148 bytes schema v2).
Map<String, dynamic> decodeGlucoseRecordFields(Uint8List payload) {
  if (payload.length < 16) return const {};
  final d = ByteData.sublistView(payload);
  double f(int off) =>
      payload.length >= off + 4 ? d.getFloat32(off, Endian.little) : 0.0;
  int i32(int off) =>
      payload.length >= off + 4 ? d.getInt32(off, Endian.little) : 0;
  return {
    'timestamp_unix': d.getUint32(0, Endian.little),
    'device_id': d.getUint32(4, Endian.little),
    'glucose_mg_dl': d.getUint16(8, Endian.little),
    'quality': d.getUint8(10),
    'variant': d.getUint8(11),
    'model_version': payload.length >= 16 ? d.getUint32(12, Endian.little) : 0,
    'intercept': f(16),
    'outlier_k': f(20),
    'tot_coeff': f(24),
    'y1_value': f(28),
    'avg_val': f(32),
    'std_dev': f(36),
    'up_lim': f(40),
    'll_lim': f(44),
    'p_count': i32(48),
    'n_count': i32(52),
    'p_val': f(56),
    'n_val': f(60),
    'p_plus_n': f(64),
    'y2_val': f(68),
    'y2_percent': f(72),
    'group_cd': i32(76),
    'y2_factor': f(80),
    'y2_factor_val': f(84),
    'const_val': f(88),
    'y3_value': f(92),
    'y3_row_no': i32(96),
    'elim_per': f(100),
    'elim_val': f(104),
    'y_value': i32(108),
    'calibration_factor': f(112),
    'ag_adjusted': f(116),
    'normalized_glucose': f(120),
    'actual_insulin': f(124),
    'insulin_correction': f(128),
    'insulin_ratio': f(132),
    'inverse_ratio': f(136),
    'homa_ir_index': f(140),
    'measurement_id': payload.length >= 148
        ? d.getUint32(144, Endian.little)
        : 0,
  };
}

/// RECORD_TYPE_VITALS summary (`struct rec_vitals`, 40 bytes schema v2).
Map<String, dynamic> decodeVitalsRecordFields(Uint8List payload) {
  if (payload.length < 12) return const {};
  final d = ByteData.sublistView(payload);
  int u16(int off) =>
      payload.length >= off + 2 ? d.getUint16(off, Endian.little) : 0;
  int u8(int off) => payload.length >= off + 1 ? d.getUint8(off) : 0;
  return {
    'timestamp_unix': d.getUint32(0, Endian.little),
    'device_id': d.getUint32(4, Endian.little),
    'hr_bpm': u16(8),
    'hr_conf': u8(10),
    'spo2_percent': u8(11),
    'spo2_conf': u8(12),
    'hb_conf': u8(13),
    'hb_g_dl': u16(14) / 10.0,
    'resp_rate_bpm': u8(16),
    'resp_conf': u8(17),
    'sdnn_ms': u16(18),
    'rmssd_ms': u16(20),
    'systolic_mmhg': u16(22),
    'diastolic_mmhg': u16(24),
    'quality': u8(26),
    'flags': u8(27),
    'snr_db_x10': u16(28),
    'perfusion_index_x10': u8(30),
    'sample_rate_hz': u16(32),
    'sample_count': u16(34),
    'measurement_id': payload.length >= 40 ? d.getUint32(36, Endian.little) : 0,
  };
}

/// RECORD_TYPE_TEMP summary (`struct rec_temp`, 18 bytes schema v2).
Map<String, dynamic> decodeTempRecordFields(Uint8List payload) {
  if (payload.length < 12) return const {};
  final d = ByteData.sublistView(payload);
  return {
    'timestamp_unix': d.getUint32(0, Endian.little),
    'device_id': d.getUint32(4, Endian.little),
    'soc_temp_c': d.getInt16(8, Endian.little) / 100.0,
    'skin_temp_c': d.getInt16(10, Endian.little) / 100.0,
    'skin_band': payload.length >= 13 ? d.getUint8(12) : 0,
    'source': payload.length >= 14 ? d.getUint8(13) : 0,
    'measurement_id': payload.length >= 18 ? d.getUint32(14, Endian.little) : 0,
  };
}

/// One PPG optics sample at [offset].
/// Schema v3 = 48 B (includes capture timestamp); legacy v2 = 42 B.
Map<String, dynamic> _decodePpgSampleAt(
  ByteData d,
  int offset,
  int sampleSize,
) {
  final m = <String, dynamic>{
    'ir': d.getUint32(offset, Endian.little),
    'red': d.getUint32(offset + 4, Endian.little),
    'green': d.getUint32(offset + 8, Endian.little),
    'ir_dc': d.getInt32(offset + 12, Endian.little),
    'red_dc': d.getInt32(offset + 16, Endian.little),
    'green_dc': d.getInt32(offset + 20, Endian.little),
    'ir_ac': d.getInt32(offset + 24, Endian.little),
    'red_ac': d.getInt32(offset + 28, Endian.little),
    'green_ac': d.getInt32(offset + 32, Endian.little),
    'accel_x': d.getInt16(offset + 36, Endian.little),
    'accel_y': d.getInt16(offset + 38, Endian.little),
    'accel_z': d.getInt16(offset + 40, Endian.little),
  };
  if (sampleSize >= recPpgRawSampleSizeV3) {
    final unix = d.getUint32(offset + 42, Endian.little);
    final ms = d.getUint16(offset + 46, Endian.little);
    m['timestamp_unix'] = unix;
    m['timestamp_ms'] = ms;
    m['timestamp_unix_ms'] = unix * 1000 + ms;
  }
  return m;
}

const int recPpgRawSampleSizeV2 = 42;
const int recPpgRawSampleSizeV3 = 48;
const int recPpgRawSampleSize = recPpgRawSampleSizeV3;
const int recPpgRawHdrSizeV1 = 16;
const int recPpgRawHdrSizeV2 = 20;
const int recPpgRawFlagTimestamp = 0x0004;

int _ppgRawSampleSizeFor(int payloadLen, int hdrSize, int n) {
  if (n <= 0) return recPpgRawSampleSizeV3;
  final body = payloadLen - hdrSize;
  if (body >= n * recPpgRawSampleSizeV3 && body % recPpgRawSampleSizeV3 == 0) {
    return recPpgRawSampleSizeV3;
  }
  if (body >= n * recPpgRawSampleSizeV2 && body % recPpgRawSampleSizeV2 == 0) {
    return recPpgRawSampleSizeV2;
  }
  return recPpgRawSampleSizeV3;
}

/// RECORD_TYPE_PPG_RAW chunk (`struct rec_ppg_raw_hdr` + N x `rec_ppg_sample`).
/// Returns `{'header': {...}, 'samples': [...]}`; empty map if too short.
Map<String, dynamic> decodePpgRawChunkFields(Uint8List payload) {
  if (payload.length < recPpgRawHdrSizeV1) return const {};
  final d = ByteData.sublistView(payload);
  final n = d.getUint16(12, Endian.little);
  var hdrSize = recPpgRawHdrSizeV1;
  var measurementId = 0;
  if (payload.length >= recPpgRawHdrSizeV2) {
    final flags = d.getUint16(14, Endian.little);
    final tryV3 = recPpgRawHdrSizeV2 + n * recPpgRawSampleSizeV3;
    final tryV2 = recPpgRawHdrSizeV2 + n * recPpgRawSampleSizeV2;
    if (payload.length == tryV3 ||
        payload.length == tryV2 ||
        (flags & recPpgRawFlagTimestamp) != 0 ||
        payload.length > tryV2) {
      hdrSize = recPpgRawHdrSizeV2;
      measurementId = d.getUint32(16, Endian.little);
    } else if (payload.length !=
        recPpgRawHdrSizeV1 + n * recPpgRawSampleSizeV2) {
      hdrSize = recPpgRawHdrSizeV2;
      measurementId = d.getUint32(16, Endian.little);
    }
  }
  final sampleSize = _ppgRawSampleSizeFor(payload.length, hdrSize, n);
  final header = {
    'parent_id': d.getUint32(0, Endian.little),
    'sample_rate_hz': d.getUint16(4, Endian.little),
    'total_samples': d.getUint16(6, Endian.little),
    'chunk_index': d.getUint16(8, Endian.little),
    'chunk_count': d.getUint16(10, Endian.little),
    'n_in_chunk': n,
    'flags': d.getUint16(14, Endian.little),
    'measurement_id': measurementId,
    'sample_size': sampleSize,
  };
  final samples = <Map<String, dynamic>>[];
  var off = hdrSize;
  for (var i = 0; i < n && off + sampleSize <= payload.length; i++) {
    samples.add(_decodePpgSampleAt(d, off, sampleSize));
    off += sampleSize;
  }
  return {'header': header, 'samples': samples};
}

const int recGlucoseRawSampleSizeV2 = 4;
const int recGlucoseRawSampleSizeV3 = 8;
const int recGlucoseRawSampleSize = recGlucoseRawSampleSizeV3;
const int recGlucoseRawHdrSizeV1 = 14;
const int recGlucoseRawHdrSizeV2 = 18;

int _glucoseRawSampleSizeFor(int payloadLen, int hdrSize, int n) {
  if (n <= 0) return recGlucoseRawSampleSizeV3;
  final body = payloadLen - hdrSize;
  if (body >= n * recGlucoseRawSampleSizeV3 &&
      body % recGlucoseRawSampleSizeV3 == 0) {
    return recGlucoseRawSampleSizeV3;
  }
  if (body >= n * recGlucoseRawSampleSizeV2 &&
      body % recGlucoseRawSampleSizeV2 == 0) {
    return recGlucoseRawSampleSizeV2;
  }
  return recGlucoseRawSampleSizeV3;
}

/// RECORD_TYPE_GLUCOSE_RAW chunk (`struct rec_glucose_raw_hdr` + N samples).
/// Schema v3 sample = adc + mv_x10 + timestamp_unix (8 B); legacy = 4 B.
Map<String, dynamic> decodeGlucoseRawChunkFields(Uint8List payload) {
  if (payload.length < recGlucoseRawHdrSizeV1) return const {};
  final d = ByteData.sublistView(payload);
  final n = d.getUint16(10, Endian.little);
  var hdrSize = recGlucoseRawHdrSizeV1;
  var measurementId = 0;
  if (payload.length >= recGlucoseRawHdrSizeV2) {
    final tryV3 = recGlucoseRawHdrSizeV2 + n * recGlucoseRawSampleSizeV3;
    final tryV2 = recGlucoseRawHdrSizeV2 + n * recGlucoseRawSampleSizeV2;
    if (payload.length == tryV3 ||
        payload.length == tryV2 ||
        payload.length > tryV2) {
      hdrSize = recGlucoseRawHdrSizeV2;
      measurementId = d.getUint32(14, Endian.little);
    } else if (payload.length !=
        recGlucoseRawHdrSizeV1 + n * recGlucoseRawSampleSizeV2) {
      hdrSize = recGlucoseRawHdrSizeV2;
      measurementId = d.getUint32(14, Endian.little);
    }
  }
  final sampleSize = _glucoseRawSampleSizeFor(payload.length, hdrSize, n);
  final header = {
    'parent_id': d.getUint32(0, Endian.little),
    'total_samples': d.getUint16(4, Endian.little),
    'chunk_index': d.getUint16(6, Endian.little),
    'chunk_count': d.getUint16(8, Endian.little),
    'n_in_chunk': n,
    'measurement_id': measurementId,
    'sample_size': sampleSize,
  };
  final samples = <Map<String, dynamic>>[];
  var off = hdrSize;
  for (var i = 0; i < n && off + sampleSize <= payload.length; i++) {
    final sample = <String, dynamic>{
      'adc': d.getUint16(off, Endian.little),
      'voltage_mv': d.getInt16(off + 2, Endian.little) / 10.0,
    };
    if (sampleSize >= recGlucoseRawSampleSizeV3) {
      sample['timestamp_unix'] = d.getUint32(off + 4, Endian.little);
    }
    samples.add(sample);
    off += sampleSize;
  }
  return {'header': header, 'samples': samples};
}
