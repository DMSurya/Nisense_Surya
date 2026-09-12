import 'dart:async';
import 'dart:collection';
import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../ble/connection_flow.dart';
import '../ble/nisense_scan.dart';
import '../ble/hcm_ble_client.dart';
import '../dfu/smp_client.dart';
import '../protocol/gatt_registry.dart';
import '../protocol/hcm_protocol.dart';
import '../services/session_logger.dart';
import '../util/app_log.dart';

const _autoReconnectBaseDelayS = 2;
const _autoReconnectMaxDelayS = 20;

class KnownDevice {
  KnownDevice({required this.address, required this.name});
  final String address;
  final String name;
}

class HcmBackend extends ChangeNotifier {
  HcmBackend() {
    _client.setNotifyCallback(_onNotify);
    _initKnownDevices();
  }

  final HcmBleClient _client = HcmBleClient();
  late final ConnectionFlow _connectionFlow = ConnectionFlow(_client);

  SessionLogger? _logger;
  final List<ScanResult> _scanResults = [];
  List<KnownDevice> _knownDevices = [];
  List<GattServiceNode> _gattTree = [];

  bool _scanning = false;
  bool _connecting = false;
  bool _connected = false;
  bool _manualDisconnect = false;
  bool _suppressAutoReconnect = false;
  String _status = 'Idle';
  String _connectedAddress = '';
  String _deviceName = '';
  String _hardwareDeviceId = '';
  int _securityProfile = bleSecurityProfileSecure;
  bool _isPaired = false;
  bool _needsPair = false;
  int _pairingPasskey = 0;
  String _pairingStateLabel = 'idle';
  int _connMtu = 0;
  int _reconnectAttempt = 0;

  StreamSubscription<BluetoothConnectionState>? _connStateSub;
  Timer? _reconnectTimer;

  // Vitals / dashboard
  int _hrBpm = 0;
  int _hrConf = 0;
  int _spo2Pct = 0;
  int _spo2Conf = 0;
  double _hbGdl = 0;
  int _hbConf = 0;
  int _respBpm = 0;
  int _respConf = 0;
  int _glucoseMgDl = 0;
  double _tempC = 0;
  double _insulinUiUml = 0;
  double _homaIr = 0;
  int _sdnnMs = 0;
  int _rmssdMs = 0;
  int _systolicMmhg = 0;
  int _diastolicMmhg = 0;
  int _battMv = 0;
  int _battMa = 0;
  int _battSoc = 0;
  int _chargerSt = 0;
  int _buck1Mv = 0;
  int _buck2Mv = 0;
  int _buck3Mv = 0;
  int _bboutMv = 0;
  int _chargeVoltageMv = 0;
  int _chargeCurrentMa = 0;
  int _battTempC = 0;
  int _remainingMah = 0;
  int _fullMah = 0;
  int _designMah = 0;
  bool _measActive = false;
  int _measPct = 0;
  bool _wearContact = false;
  int _accelX = 0;
  int _accelY = 0;
  int _accelZ = 0;

  // Config
  int _screenTimeoutS = 60;
  String _wifiSsid = '';
  int _wifiConnected = 0;
  String _wifiIp = '';
  String _patientName = 'Patient';

  // Device build info (from f01a)
  DeviceBuildData? _deviceBuild;

  // DFU
  bool _dfuActive = false;
  double _dfuProgress = 0;

  // Screen-scoped GATT notifies (see BleNotifyProfile).
  BleNotifyProfile _shellNotifyProfile = BleNotifyProfile.minimal;
  String? _exclusiveLock;

  int _ppgDecimate = 1;
  SamplingConfigData _samplingConfig = const SamplingConfigData(
    // Placeholder until f01b read; device default is rate × buffer (25×20=500 with RESP).
    ppgSampleCount: 500,
    glucoseNumSamples: 80,
    glucoseDelayMs: 2000,
  );

  // Charts ring buffers — PPG and glucose raw samples
  static const _rawSampleMax = 200;
  final _ppgSamples = ListQueue<PpgSample>();
  final _glucoseSamples = ListQueue<GlucoseSampleData>();

  /// Last raw notify payload per characteristic UUID (for GATT explorer).
  final Map<String, List<int>> _lastNotifyRaw = {};

  bool get scanning => _scanning;
  bool get connecting => _connecting;
  bool get connected => _connected;
  String get status => _status;
  List<ScanResult> get scanResults => List.unmodifiable(_scanResults);
  List<KnownDevice> get knownDevices => List.unmodifiable(_knownDevices);
  List<GattServiceNode> get gattTree => List.unmodifiable(_gattTree);
  HcmBleClient get client => _client;

  String get connectedAddress => _connectedAddress;
  String get deviceName => _deviceName;
  String get hardwareDeviceId => _hardwareDeviceId;
  int get securityProfile => _securityProfile;
  bool get isPaired => _isPaired;
  bool get needsPair => _needsPair;
  int get pairingPasskey => _pairingPasskey;
  String get pairingStateLabel => _pairingStateLabel;
  int get connMtu => _connMtu;

  int get hrBpm => _hrBpm;
  int get hrConf => _hrConf;
  int get spo2Pct => _spo2Pct;
  int get spo2Conf => _spo2Conf;
  double get hbGdl => _hbGdl;
  int get hbConf => _hbConf;
  int get respBpm => _respBpm;
  int get respConf => _respConf;
  int get glucoseMgDl => _glucoseMgDl;
  double get tempC => _tempC;
  double get insulinUiUml => _insulinUiUml;
  double get homaIr => _homaIr;
  int get sdnnMs => _sdnnMs;
  int get rmssdMs => _rmssdMs;
  int get systolicMmhg => _systolicMmhg;
  int get diastolicMmhg => _diastolicMmhg;
  int get battMv => _battMv;
  int get battMa => _battMa;
  int get battSoc => _battSoc;
  int get chargerSt => _chargerSt;
  int get buck1Mv => _buck1Mv;
  int get buck2Mv => _buck2Mv;
  int get buck3Mv => _buck3Mv;
  int get bboutMv => _bboutMv;
  int get chargeVoltageMv => _chargeVoltageMv;
  int get chargeCurrentMa => _chargeCurrentMa;
  int get battTempC => _battTempC;
  int get remainingMah => _remainingMah;
  int get fullMah => _fullMah;
  int get designMah => _designMah;

  String get chargerStatusLabel =>
      formatPmicChargerStatus(_chargerSt, externalPowerOnly: externalPowerOnly);

  /// True when PMIC ADC VBAT is below Li-ion range (USB bench power, no cell).
  bool get externalPowerOnly =>
      isPmicExternalPowerOnly(_battMv, socPercent: _battSoc);

  Map<String, List<int>> get lastNotifyRaw {
    return Map.unmodifiable(
      _lastNotifyRaw.map((k, v) => MapEntry(k, List<int>.from(v))),
    );
  }
  bool get measActive => _measActive;
  int get measPct => _measPct;
  bool get wearContact => _wearContact;
  int get accelX => _accelX;
  int get accelY => _accelY;
  int get accelZ => _accelZ;

  bool get backlightOn => false;
  int get screenTimeoutS => _screenTimeoutS;
  String get wifiSsid => _wifiSsid;
  int get wifiConnected => _wifiConnected;
  String get wifiIp => _wifiIp;
  String get patientName => _patientName;
  DeviceBuildData? get deviceBuild => _deviceBuild;

  bool get dfuActive => _dfuActive;
  double get dfuProgress => _dfuProgress;

  /// Tab-driven notify set; ignored while an exclusive Sync/DFU op holds the lock.
  BleNotifyProfile get shellNotifyProfile => _shellNotifyProfile;

  /// True while record sync or DFU owns GATT notify slots — block tab changes.
  bool get navigationLocked => _exclusiveLock != null;
  String? get navigationLockReason => _exclusiveLock;

  int get ppgDecimate => _ppgDecimate;
  SamplingConfigData get samplingConfig => _samplingConfig;

  List<PpgSample> get ppgSamples => _ppgSamples.toList();
  List<GlucoseSampleData> get glucoseSamples => _glucoseSamples.toList();

  Future<void> _initKnownDevices() async {
    final prefs = await SharedPreferences.getInstance();
    final saved = prefs.getString('patient_name');
    if (saved != null && saved.isNotEmpty) _patientName = saved;
    await _loadKnownDevices();
    await autoConnectKnown();
  }

  Future<void> _loadKnownDevices() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getStringList('known_devices') ?? [];
    _knownDevices = raw.map((e) {
      final parts = e.split('|');
      return KnownDevice(address: parts[0], name: parts.length > 1 ? parts[1] : '');
    }).toList();
    notifyListeners();
  }

  Future<void> _saveKnownDevice(String address, String name) async {
    final prefs = await SharedPreferences.getInstance();
    _knownDevices.removeWhere((d) => d.address == address);
    _knownDevices.insert(0, KnownDevice(address: address, name: name));
    await prefs.setStringList(
      'known_devices',
      _knownDevices.map((d) => '${d.address}|${d.name}').toList(),
    );
    notifyListeners();
  }

  Future<void> autoConnectKnown() async {
    if (_connected || _connecting) return;
    if (_knownDevices.isEmpty) {
      _status = 'No known device — scan to connect';
      notifyListeners();
      return;
    }
    await connectToAddress(_knownDevices.first.address, attemptBond: true);
  }

  Future<void> scan() async {
    _scanning = true;
    _status = 'Scanning…';
    notifyListeners();
    AppLog.i('ui', 'scan requested');
    try {
      _scanResults
        ..clear()
        ..addAll(await _client.scan());
      _status = 'Found ${_scanResults.length} device(s)';
      AppLog.i('ui', _status);
    } catch (e, st) {
      _status = 'Scan failed: $e';
      AppLog.e('ui', 'scan failed', e, st);
    }
    _scanning = false;
    notifyListeners();
  }

  Future<void> connectToAddress(String address, {bool attemptBond = true}) async {
    _cancelReconnectTimer();
    _manualDisconnect = false;
    _connecting = true;
    _status = 'Connecting…';
    notifyListeners();
    AppLog.i('ui', 'connectToAddress $address attemptBond=$attemptBond');
    try {
      final adapter = await FlutterBluePlus.adapterState.first;
      AppLog.i('ui', 'adapterState=$adapter');
      if (adapter != BluetoothAdapterState.on) {
        _status = 'Bluetooth is off';
        AppLog.e('ui', 'abort connect — adapter not on');
        _connecting = false;
        notifyListeners();
        return;
      }

      final dev = BluetoothDevice.fromId(address);
      final result = await _connectionFlow.connectAndSetup(
        dev,
        attemptBond: attemptBond,
        onPairingStatus: _updatePairingStatus,
      );
      if (!result.ok) {
        _status = result.error ?? 'Connect failed';
        AppLog.e('ui', 'connect failed: $_status');
        _connecting = false;
        notifyListeners();
        return;
      }
      _connected = true;
      _connectedAddress = address;
      _securityProfile = result.profile;
      _isPaired = result.paired;
      _needsPair = result.needsPair;
      _connMtu = result.mtu;
      _reconnectAttempt = 0;
      final matches = _scanResults.where((r) => r.device.remoteId.str == address);
      final scanHit = matches.isEmpty ? null : matches.first;
      if (scanHit != null) {
        _deviceName = resolveNiSenseProductLabel(scanHit);
      } else {
        _deviceName = resolveNiSenseProductLabelFromName(
          dev.platformName.isNotEmpty ? dev.platformName : 'NiSense',
        );
      }
      _attachConnectionListener(dev);
      await _readConfig();
      _deviceName = resolveNiSenseProductLabelFromName(_deviceName);
      await _saveKnownDevice(address, _deviceName);
      _logger = SessionLogger(patientName: _patientName, deviceId: _hardwareDeviceId);
      // ConnectionFlow installed minimal CCC; restore the active tab profile.
      try {
        await _client.setNotifyProfile(
          _shellNotifyProfile,
          pairedForHealth: _isPaired,
        );
      } catch (e) {
        AppLog.w('gatt', 'post-connect notify profile failed', e);
      }
      _status = _needsPair ? 'Connected — pair for health data' : 'Connected';
      AppLog.i('ui', '$_status mtu=$_connMtu paired=$_isPaired');
    } catch (e, st) {
      _status = 'Connect error: $e';
      _connected = false;
      _needsPair = false;
      AppLog.e('ui', 'connectToAddress exception', e, st);
    }
    _connecting = false;
    notifyListeners();
  }

  void _attachConnectionListener(BluetoothDevice dev) {
    _connStateSub?.cancel();
    _connStateSub = dev.connectionState.listen((state) {
      if (state == BluetoothConnectionState.disconnected) {
        _onLinkLost();
      }
    });
  }

  void _detachConnectionListener() {
    _connStateSub?.cancel();
    _connStateSub = null;
  }

  void _onLinkLost() {
    if (!_connected && !_connecting) return;
    AppLog.w('ui', 'link lost address=$_connectedAddress');
    _connected = false;
    _connMtu = 0;
    _detachConnectionListener();
    _status = 'Link lost';
    notifyListeners();
    if (!_manualDisconnect && !_suppressAutoReconnect && _connectedAddress.isNotEmpty) {
      _scheduleReconnect();
    }
  }

  void _scheduleReconnect() {
    if (_manualDisconnect || _suppressAutoReconnect || _connectedAddress.isEmpty) {
      return;
    }
    _reconnectAttempt++;
    final delay = math.min(
      _autoReconnectBaseDelayS * math.pow(2, math.min(_reconnectAttempt - 1, 4)).toInt(),
      _autoReconnectMaxDelayS,
    );
    _status = 'Reconnecting in ${delay}s (attempt $_reconnectAttempt)…';
    notifyListeners();
    _reconnectTimer?.cancel();
    _reconnectTimer = Timer(Duration(seconds: delay), () async {
      if (_connected || _manualDisconnect || _connecting) return;
      await connectToAddress(_connectedAddress, attemptBond: false);
      if (_connected) {
        _reconnectAttempt = 0;
        if (_needsPair) {
          _status = 'Reconnected — pair for health data';
          notifyListeners();
        }
      } else if (!_manualDisconnect) {
        _scheduleReconnect();
      }
    });
  }

  void _cancelReconnectTimer() {
    _reconnectTimer?.cancel();
    _reconnectTimer = null;
  }

  Future<void> disconnect() async {
    _manualDisconnect = true;
    _cancelReconnectTimer();
    _detachConnectionListener();
    await _client.disconnect();
    _connected = false;
    _connectedAddress = '';
    _needsPair = false;
    _connMtu = 0;
    _status = 'Disconnected';
    notifyListeners();
  }

  Future<void> forgetDevice(String address) async {
    _manualDisconnect = true;
    _cancelReconnectTimer();
    if (_connected && _connectedAddress == address) {
      await disconnect();
    }
    _manualDisconnect = false;
    _knownDevices.removeWhere((d) => d.address == address);
    final prefs = await SharedPreferences.getInstance();
    await prefs.setStringList(
      'known_devices',
      _knownDevices.map((d) => '${d.address}|${d.name}').toList(),
    );
    if (_connectedAddress == address) {
      _connectedAddress = '';
    }
    _status = 'Removed known device';
    notifyListeners();
  }

  void _updatePairingStatus(PairingStatus? status) {
    if (status == null) return;
    _pairingPasskey = status.passkey;
    _pairingStateLabel = _pairingLabel(status.state);
    notifyListeners();
  }

  /// Apply the notify set for the active bottom-nav tab.
  Future<void> setShellNotifyProfile(BleNotifyProfile profile) async {
    _shellNotifyProfile = profile;
    if (!_connected || _exclusiveLock != null) {
      notifyListeners();
      return;
    }
    try {
      await _client.setNotifyProfile(
        profile,
        pairedForHealth: _isPaired,
      );
    } catch (e) {
      AppLog.w('gatt', 'setShellNotifyProfile failed', e);
    }
    notifyListeners();
  }

  /// Clear live health CCCDs, run [op] (Sync / DFU), then restore the tab profile.
  Future<T> runWithExclusiveNotifies<T>(
    String reason,
    Future<T> Function() op,
  ) async {
    if (_exclusiveLock != null) {
      throw StateError('Busy: $_exclusiveLock');
    }
    if (!_connected) {
      throw StateError('Not connected');
    }
    _exclusiveLock = reason;
    notifyListeners();
    try {
      await _client.setNotifyProfile(
        BleNotifyProfile.exclusive,
        pairedForHealth: _isPaired,
      );
      // Give Android a beat to free notify slots before Sync/SMP CCC writes.
      await Future<void>.delayed(const Duration(milliseconds: 150));
      return await op();
    } finally {
      _exclusiveLock = null;
      if (_connected) {
        try {
          await _client.setNotifyProfile(
            _shellNotifyProfile,
            pairedForHealth: _isPaired,
          );
        } catch (e) {
          AppLog.w('gatt', 'restore notify profile failed', e);
        }
      }
      notifyListeners();
    }
  }

  Future<void> pairDevice() async {
    if (!_connected) return;
    _status = 'Bonding…';
    _pairingStateLabel = 'bonding';
    notifyListeners();
    try {
      final ok = await _client.bond(onPairingStatus: _updatePairingStatus);
      if (ok) {
        _isPaired = true;
        _needsPair = false;
        if (_exclusiveLock == null) {
          await _client.setNotifyProfile(
            _shellNotifyProfile,
            pairedForHealth: true,
          );
        }
        await _readConfig();
        try {
          await _client.syncRtcFromHost();
        } catch (e) {
          AppLog.w('cts', 'CTS RTC sync after bond failed', e);
        }
        _status = 'Bonded';
        _pairingStateLabel = 'complete';
      } else {
        _status = 'Pair failed — measurement status only';
        _pairingStateLabel = 'failed';
      }
    } catch (e) {
      _status = 'Pair failed: $e';
      _pairingStateLabel = 'failed';
    }
    notifyListeners();
  }

  Future<void> forgetPairing() async {
    await _client.removeBond();
    _isPaired = false;
    _needsPair = _securityProfile == bleSecurityProfileSecure;
    if (_exclusiveLock == null) {
      await _client.setNotifyProfile(
        _shellNotifyProfile,
        pairedForHealth: false,
      );
    }
    _status = 'Bond removed';
    notifyListeners();
  }

  Future<void> unpairDevice(String address) async {
    _manualDisconnect = true;
    _cancelReconnectTimer();
    if (_connected && _connectedAddress == address) {
      try {
        await _client.writeUuid(chrcAdminCtrl, [adminCmdDeleteBonds]);
      } catch (_) {}
      await forgetPairing();
      await disconnect();
    }
    await forgetDevice(address);
    _manualDisconnect = false;
  }

  Future<void> _readConfig() async {
    try {
      if (!_client.shouldSkipRead(chrcDeviceName)) {
        final name = await _client.readUuid(chrcDeviceName);
        _deviceName = String.fromCharCodes(name.where((b) => b != 0));
      }
      if (!_client.shouldSkipRead(chrcDeviceId)) {
        final id = await _client.readUuid(chrcDeviceId);
        _hardwareDeviceId = String.fromCharCodes(id.where((b) => b != 0));
      }
      if (!_client.shouldSkipRead(chrcDeviceBuild)) {
        final raw = await _client.readUuid(chrcDeviceBuild);
        _deviceBuild = decodeDeviceBuild(raw);
      }
      if (!_client.shouldSkipRead(chrcScreenTimeout)) {
        final v = await _client.readUuid(chrcScreenTimeout);
        if (v.length >= 2) _screenTimeoutS = v[0] | (v[1] << 8);
      }
      if (_isPaired && !_client.shouldSkipRead(chrcPpgDecimate)) {
        try {
          _ppgDecimate = await _client.getPpgDecimate();
        } catch (_) {}
      }
      if (_isPaired && !_client.shouldSkipRead(chrcSamplingConfig)) {
        try {
          final cfg = await _client.getSamplingConfig();
          if (cfg != null) _samplingConfig = cfg;
        } catch (_) {}
      }
      if (_isPaired && !_client.shouldSkipRead(chrcWifiSsid)) {
        final ssid = await _client.readUuid(chrcWifiSsid);
        _wifiSsid = String.fromCharCodes(ssid.where((b) => b != 0));
      }
      final pairing = await _client.readPairingStatus();
      if (pairing != null) {
        _updatePairingStatus(pairing);
      }
    } catch (_) {}
    notifyListeners();
  }

  /// Re-reads the device build/status characteristic (FW, uptime, pending
  /// records). Firmware fills this fresh on every GATT read, but the app
  /// only reads it once at connect — call this explicitly (e.g. on Device
  /// Info page open/refresh, or right after a sync ACK) to avoid a stale
  /// snapshot.
  Future<void> refreshDeviceInfo() async {
    if (!_connected) return;
    try {
      final raw = await _client.readUuid(chrcDeviceBuild);
      _deviceBuild = decodeDeviceBuild(raw);
      notifyListeners();
    } catch (_) {}
  }

  String _pairingLabel(int state) {
    switch (state) {
      case blePairingWaitingPasskey:
        return 'waiting_passkey';
      case blePairingConfirmPasskey:
        return 'confirm_passkey';
      case blePairingBonding:
        return 'bonding';
      case blePairingComplete:
        return 'complete';
      case blePairingFailed:
        return 'failed';
      default:
        return 'idle';
    }
  }

  Future<void> refreshGatt() async {
    _gattTree = await _client.buildGattTree();
    notifyListeners();
  }

  Future<List<int>?> readCharacteristic(String uuid) async {
    if (_client.shouldSkipRead(uuid)) {
      _status = 'Read blocked until paired';
      notifyListeners();
      return null;
    }
    try {
      final data = await _client.readUuid(uuid);
      _status = 'Read ${resolveGattName(uuid)} (${data.length} B)';
      notifyListeners();
      return data;
    } catch (e) {
      _status = 'Read failed: $e';
      notifyListeners();
      return null;
    }
  }

  List<int>? lastNotifyFor(String uuid) {
    final raw = _lastNotifyRaw[uuid.toLowerCase()];
    if (raw == null) return null;
    return List<int>.from(raw);
  }

  Future<void> writeCharacteristic(String uuid, List<int> value) async {
    await _client.writeUuid(uuid, value);
    _status = 'Wrote ${value.length} bytes';
    notifyListeners();
  }

  Future<void> setScreenTimeout(int seconds) async {
    await _client.writeUuid(chrcScreenTimeout, [seconds & 0xff, (seconds >> 8) & 0xff]);
    _screenTimeoutS = seconds;
    notifyListeners();
  }

  Future<void> refreshSamplingConfig() async {
    if (!_connected) return;
    try {
      if (!_client.shouldSkipRead(chrcPpgDecimate)) {
        _ppgDecimate = await _client.getPpgDecimate();
      }
      if (!_client.shouldSkipRead(chrcSamplingConfig)) {
        final cfg = await _client.getSamplingConfig();
        if (cfg != null) _samplingConfig = cfg;
      }
    } catch (e) {
      AppLog.w('gatt', 'refreshSamplingConfig failed', e);
      _status = 'Sampling read failed: $e';
    }
    notifyListeners();
  }

  Future<void> setPpgDecimate(int factor) async {
    final v = factor.clamp(1, 33);
    await _client.setPpgDecimate(v);
    _ppgDecimate = v;
    _status = 'PPG stream decimate set to 1/$v';
    notifyListeners();
  }

  Future<void> setSamplingConfig({
    int? ppgSampleCount,
    int? glucoseNumSamples,
    int? glucoseDelayMs,
    bool? disableProximity,
    bool? autoEnabled,
    int? scheduleIntervalSec,
    int? tempIdleIntervalSec,
  }) async {
    final next = SamplingConfigData(
      ppgSampleCount: (ppgSampleCount ?? _samplingConfig.ppgSampleCount).clamp(50, 2000),
      glucoseNumSamples:
          (glucoseNumSamples ?? _samplingConfig.glucoseNumSamples).clamp(10, 500),
      glucoseDelayMs: (glucoseDelayMs ?? _samplingConfig.glucoseDelayMs).clamp(100, 10000),
      disableProximity: disableProximity ?? _samplingConfig.disableProximity,
      autoEnabled: autoEnabled ?? _samplingConfig.autoEnabled,
      scheduleIntervalSec: (scheduleIntervalSec ?? _samplingConfig.scheduleIntervalSec)
          .clamp(0, 3600),
      currentIntervalSec: _samplingConfig.currentIntervalSec,
      tempIdleIntervalSec:
          (tempIdleIntervalSec ?? _samplingConfig.tempIdleIntervalSec).clamp(10, 3600),
    );
    await _client.setSamplingConfig(next);
    // Re-read so schedule_interval persists check + current ladder step update.
    if (!_client.shouldSkipRead(chrcSamplingConfig)) {
      final confirmed = await _client.getSamplingConfig();
      if (confirmed != null) {
        _samplingConfig = confirmed;
      } else {
        _samplingConfig = next;
      }
    } else {
      _samplingConfig = next;
    }
    final cfg = _samplingConfig;
    final intervalLabel = cfg.isAdaptive
        ? 'adaptive · now ${_formatSched(cfg.currentIntervalSec)}'
        : _formatSched(cfg.scheduleIntervalSec);
    _status =
        'Sampling: PPG ${cfg.ppgSampleCount}, glucose ${cfg.glucoseNumSamples}, '
        'sched $intervalLabel${cfg.autoEnabled ? '' : ' (auto off)'}'
        '${cfg.disableProximity ? ', prox OFF' : ''}';
    notifyListeners();
  }

  static String _formatSched(int sec) {
    if (sec <= 0) return '?';
    if (sec % 60 == 0) return '${sec ~/ 60} min';
    return '${(sec / 60).toStringAsFixed(1)} min';
  }

  Future<void> setWifiCredentials(String ssid, String password) async {
    await _client.writeUuid(chrcWifiSsid, encodeString(ssid, 32));
    await _client.writeUuid(chrcWifiPassword, encodeString(password, 64));
    _wifiSsid = ssid;
    notifyListeners();
  }

  Future<void> wifiConnect() async {
    await _client.writeUuid(chrcWifiConnect, encodeUint8(1));
    _status = 'WiFi connect requested';
    notifyListeners();
  }

  Future<void> wifiDisconnect() async {
    await _client.writeUuid(chrcWifiConnect, encodeUint8(0));
    notifyListeners();
  }

  Future<void> startVitals({bool skipProximity = false}) async {
    await _client.writeUuid(
      chrcMeasCtrl,
      encodeMeasCtrl(
        measCmdStart,
        measTypeVitals,
        skipProximity ? measFlagSkipProx : 0,
      ),
    );
  }

  Future<void> startGlucose({bool skipProximity = false}) async {
    await _client.writeUuid(
      chrcMeasCtrl,
      encodeMeasCtrl(
        measCmdStart,
        measTypeGlucose,
        skipProximity ? measFlagSkipProx : 0,
      ),
    );
  }

  Future<void> stopMeasurement() async =>
      _client.writeUuid(chrcMeasCtrl, encodeMeasCtrl(measCmdStop, 0));

  Future<void> pmicEnable(int target) async {
    await _client.writeUuid(chrcPmicCtrl, encodePmicCtrl(pmicCmdEnable, target));
  }

  Future<void> pmicDisable(int target) async {
    await _client.writeUuid(chrcPmicCtrl, encodePmicCtrl(pmicCmdDisable, target));
  }

  void setPatientName(String name) async {
    _patientName = name;
    _logger?.setPatientName(name);
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('patient_name', name);
    notifyListeners();
  }

  Future<void> startDfu(String imagePath) async {
    if (!_isPaired) {
      _status = 'DFU requires bond';
      notifyListeners();
      return;
    }
    try {
      await runWithExclusiveNotifies('Firmware update in progress', () async {
        _dfuActive = true;
        _dfuProgress = 0;
        notifyListeners();
        try {
          final dev = _client.device;
          if (dev == null) throw StateError('Not connected');
          final smp = SmpClient(dev);
          await smp.uploadImage(
            imagePath,
            onProgress: (sent, total) {
              _dfuProgress = total > 0 ? sent / total : 0;
              notifyListeners();
            },
          );
          await smp.setImagePending();
          await smp.resetDevice();
          _status = 'DFU complete — reconnecting…';
        } finally {
          _dfuActive = false;
        }
      });
    } catch (e) {
      _status = 'DFU failed: $e';
      _dfuActive = false;
      notifyListeners();
    }
  }

  List<String> get logPaths => _logger?.sessionPaths ?? [];

  void _applyVitals(VitalsData v) {
    // Keep last non-zero readings when a later snapshot omits fields (sensorAll
    // often carries resp/temp while HR/SpO2/Hb are still 0 in cache).
    if (v.hrBpm > 0) {
      _hrBpm = v.hrBpm;
      _hrConf = v.hrConf;
    }
    if (v.spo2Pct > 0) {
      _spo2Pct = v.spo2Pct;
      _spo2Conf = v.spo2Conf;
    }
    if (v.hbGdl > 0) {
      _hbGdl = v.hbGdl;
      _hbConf = v.hbConf;
    }
    if (v.respBpm > 0) {
      _respBpm = v.respBpm;
      _respConf = v.respConf;
    }
    if (v.hrv_valid || v.sdnn_ms > 0) {
      _sdnnMs = v.sdnn_ms;
      _rmssdMs = v.rmssd_ms;
    }
    if (v.bp_valid || v.systolic_mmhg > 0) {
      _systolicMmhg = v.systolic_mmhg;
      _diastolicMmhg = v.diastolic_mmhg;
    }
  }

  void _applyGlucoseAlgo(GlucoseAlgoData a) {
    if (a.actual_insulin > 0) {
      _insulinUiUml = a.actual_insulin;
    }
    if (a.homa_ir_index > 0) {
      _homaIr = a.homa_ir_index;
    }
  }

  void _onNotify(String uuid, List<int> data) {
    _lastNotifyRaw[uuid.toLowerCase()] = List<int>.from(data);
    switch (uuid) {
      case chrcVitals:
        final v = decodeVitals(data);
        if (v == null) return;
        _applyVitals(v);
        _logger?.logVitals(v);
      case chrcGlucose:
        final g = decodeGlucose(data);
        if (g == null) return;
        if (g.glucose_mg_dl > 0) {
          _glucoseMgDl = g.glucose_mg_dl;
        }
        _logger?.logGlucose(g);
      case chrcGlucoseAlgo:
        final algo = decodeGlucoseAlgo(data);
        if (algo != null) _applyGlucoseAlgo(algo);
      case chrcTemperature:
        final t = decodeTemperature(data);
        if (t != null && t.temp_c > 0) {
          _tempC = t.temp_c;
        }
      case chrcBatteryLevel:
        final level = decodeBatteryLevel(data);
        if (level != null) _battSoc = level;
      case chrcPmic:
        final p = decodePmic(data);
        if (p != null) {
          _battMv = p.battery_mv;
          _battMa = p.current_ma;
          _battSoc = p.soc_percent;
          _chargerSt = p.charger_status;
          _buck1Mv = p.buck1_mv;
          _buck2Mv = p.buck2_mv;
        }
      case chrcPmicExt:
        final ext = decodePmicExt(data);
        if (ext != null) {
          _buck3Mv = ext.buck3_mv;
          _bboutMv = ext.bbout_mv;
          _chargeVoltageMv = ext.charge_voltage_mv;
          _chargeCurrentMa = ext.charge_current_ma;
          _battTempC = ext.battery_temp_c;
          _remainingMah = ext.remaining_mah;
          _fullMah = ext.full_mah;
          _designMah = ext.design_mah;
        }
      case chrcMeasStatus:
        final m = decodeMeasStatus(data);
        if (m != null) {
          _measActive = m.active;
          _measPct = m.percent_complete;
        }
      case chrcProximity:
        final prox = decodeProximityStatus(data);
        if (prox != null) _wearContact = prox.contact;
      case chrcWifiStatus:
        final w = decodeWifiStatus(data);
        if (w != null) {
          _wifiConnected = w.connected;
          _wifiIp = w.ip_addr;
          _wifiSsid = w.ssid;
        }
      case chrcSensorAll:
        final s = decodeSensorAll(data);
        if (s == null) return;
        _applyVitals(s.vitals);
        if (s.glucose.glucose_mg_dl > 0) {
          _glucoseMgDl = s.glucose.glucose_mg_dl;
        }
        if (s.temperature.temp_c > 0) {
          _tempC = s.temperature.temp_c;
        }
        _battMv = s.pmic.battery_mv;
        _wearContact = s.proximity.contact;
      case chrcPpgStream:
        final sample = decodePpgSample(data);
        if (sample != null) {
          _ppgSamples.addLast(sample);
          while (_ppgSamples.length > _rawSampleMax) {
            _ppgSamples.removeFirst();
          }
          _logger?.logPpg(sample);
        }
      case chrcGlucoseSample:
        final gs = decodeGlucoseSample(data);
        if (gs != null) {
          _glucoseSamples.addLast(gs);
          while (_glucoseSamples.length > _rawSampleMax) {
            _glucoseSamples.removeFirst();
          }
        }
      case chrcAccelStream:
        final sample = decodeAccelSample(data);
        if (sample != null) {
          _accelX = sample.x_mg;
          _accelY = sample.y_mg;
          _accelZ = sample.z_mg;
          _logger?.logAccel(sample);
        }
      default:
        break;
    }
    notifyListeners();
  }

  @override
  void dispose() {
    _manualDisconnect = true;
    _cancelReconnectTimer();
    _detachConnectionListener();
    _client.disconnect();
    _logger?.close();
    super.dispose();
  }
}
