import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import '../protocol/gatt_registry.dart';
import '../util/app_log.dart';
import 'nisense_scan.dart';

const _fbpLicense = License.commercial;

const deviceNameAliases = [
  'NiSense Watch',
  'NiSense Pulse',
  'NiSense',
  'HCM',
  'HealthMonitor',
];

const notifyCharacteristics = [
  chrcBatteryLevel,
  chrcPmic,
  chrcTemperature,
  chrcVitals,
  chrcGlucose,
  chrcGlucoseSample,
  chrcGlucoseAlgo,
  chrcPmicExt,
  chrcPpgStream,
  chrcAccelStream,
  chrcSensorAll,
  chrcMeasStatus,
  chrcProximity,
  chrcWifiStatus,
];

/// Screen-scoped GATT notify sets. Keeps CCC count low so Android 10 does not
/// hit GATT_INSUFFICIENT_RESOURCES when Sync/DFU need their own notifies.
enum BleNotifyProfile {
  /// Meas status + battery only (Bluetooth / Settings / Sync idle / Firmware idle).
  minimal,

  /// Live dashboard metrics (no raw PPG/accel streams).
  dashboard,

  /// Dashboard + high-rate raw streams for the Raw charts tab.
  raw,

  /// No live health notifies — caller owns Sync (f402/f403) or SMP CCC.
  exclusive,
}

/// UUID list for [profile]. Health-encrypted chars are still filtered at
/// subscribe time when unpaired.
List<String> notifyUuidsForProfile(BleNotifyProfile profile) {
  switch (profile) {
    case BleNotifyProfile.minimal:
      return const [chrcMeasStatus, chrcBatteryLevel];
    case BleNotifyProfile.dashboard:
      return const [
        chrcMeasStatus,
        chrcBatteryLevel,
        chrcPmic,
        chrcPmicExt,
        chrcTemperature,
        chrcVitals,
        chrcGlucose,
        chrcGlucoseAlgo,
        chrcProximity,
        chrcWifiStatus,
      ];
    case BleNotifyProfile.raw:
      return const [
        chrcMeasStatus,
        chrcBatteryLevel,
        chrcPmic,
        chrcPmicExt,
        chrcTemperature,
        chrcVitals,
        chrcGlucose,
        chrcGlucoseAlgo,
        chrcGlucoseSample,
        chrcPpgStream,
        chrcAccelStream,
        chrcSensorAll,
        chrcProximity,
        chrcWifiStatus,
      ];
    case BleNotifyProfile.exclusive:
      return const [];
  }
}

const _pairPollInterval = Duration(milliseconds: 350);
const _preferredMtu = 247;

// Scan filtering and product labels: see nisense_scan.dart (isNiSenseScanResult).

class GattDescriptorNode {
  GattDescriptorNode({required this.uuid, required this.name});
  final String uuid;
  final String name;
}

class GattCharacteristicNode {
  GattCharacteristicNode({
    required this.uuid,
    required this.name,
    required this.properties,
    required this.characteristic,
    this.descriptors = const [],
  });
  final String uuid;
  final String name;
  final List<String> properties;
  final BluetoothCharacteristic characteristic;
  final List<GattDescriptorNode> descriptors;
}

class GattServiceNode {
  GattServiceNode({
    required this.uuid,
    required this.name,
    required this.characteristics,
  });
  final String uuid;
  final String name;
  final List<GattCharacteristicNode> characteristics;
}

typedef NotifyCallback = void Function(String uuid, List<int> data);
typedef PairingStatusCallback = void Function(PairingStatus? status);

class HcmBleClient {
  BluetoothDevice? _device;
  final Map<String, StreamSubscription<List<int>>> _notifySubs = {};
  NotifyCallback? _notifyCallback;
  int _securityProfile = bleSecurityProfileSecure;
  bool _bonded = false;
  List<BluetoothService>? _cachedServices;

  BluetoothDevice? get device => _device;
  bool get isConnected => _device?.isConnected ?? false;
  int get securityProfile => _securityProfile;
  bool get isBonded => _bonded;

  Future<void> ensurePermissions() async {
    if (defaultTargetPlatform == TargetPlatform.android) {
      // permission_handler used at app level
    }
  }

  Future<List<ScanResult>> scan({Duration timeout = const Duration(seconds: 10)}) async {
    AppLog.i('scan', 'start (${timeout.inSeconds}s)');
    await FlutterBluePlus.startScan(timeout: timeout);
    await Future<void>.delayed(timeout);
    await FlutterBluePlus.stopScan();
    final hits = FlutterBluePlus.lastScanResults.where(isNiSenseScanResult).toList();
    AppLog.i('scan', 'done — ${hits.length} NiSense hit(s) '
        '(raw=${FlutterBluePlus.lastScanResults.length})');
    for (final r in hits) {
      AppLog.d(
        'scan',
        '  ${r.device.remoteId.str} name="${r.advertisementData.advName}" '
        'rssi=${r.rssi}',
      );
    }
    return hits;
  }

  Future<bool> connect(BluetoothDevice dev, {Duration timeout = const Duration(seconds: 20)}) async {
    final id = dev.remoteId.str;
    final name = dev.platformName.isNotEmpty ? dev.platformName : '(no name)';
    AppLog.i(
      'ble',
      'connect begin id=$id name="$name" timeout=${timeout.inSeconds}s '
      'alreadyConnected=${dev.isConnected}',
    );
    _device = dev;
    _cachedServices = null;
    try {
      await FlutterBluePlus.adapterState
          .where((s) => s == BluetoothAdapterState.on)
          .first
          .timeout(const Duration(seconds: 3));
    } catch (e) {
      AppLog.w('ble', 'adapter not confirmed ON before connect', e);
    }
    try {
      // mtu: null — do NOT request MTU inside connect(). FBP defaults to
      // mtu:512 and awaits requestMtu; on some phones that times out even
      // though the link + bonding already succeeded (firmware shows level 4),
      // then FBP disconnects (CONNECTION_TERMINATED_BY_LOCAL_HOST).
      // We negotiate MTU later in requestMtuNegotiation() as best-effort.
      await dev.connect(
        license: _fbpLicense,
        autoConnect: false,
        timeout: timeout,
        mtu: null,
      );
    } catch (e, st) {
      // If the link came up anyway, treat as success (MTU/timeout races).
      if (dev.isConnected) {
        AppLog.w(
          'ble',
          'connect threw but isConnected=true — continuing id=$id',
          e,
        );
        return true;
      }
      AppLog.e(
        'ble',
        'connect threw id=$id isConnected=${dev.isConnected}',
        e,
        st,
      );
      return false;
    }
    final ok = dev.isConnected;
    AppLog.i('ble', 'connect end id=$id isConnected=$ok mtu=${dev.mtuNow}');
    return ok;
  }

  Future<void> disconnect() async {
    AppLog.i('ble', 'disconnect begin');
    await _cancelAllNotifications();
    final d = _device;
    if (d != null && d.isConnected) {
      try {
        await d.disconnect();
      } catch (e) {
        AppLog.w('ble', 'disconnect threw', e);
      }
    }
    _device = null;
    _bonded = false;
    _cachedServices = null;
    AppLog.i('ble', 'disconnect end');
  }

  Future<bool> refreshBondState() async {
    final d = _device;
    if (d == null) {
      _bonded = false;
      return false;
    }
    _bonded = await d.bondState.first == BluetoothBondState.bonded;
    return _bonded;
  }

  Future<bool> bond({PairingStatusCallback? onPairingStatus}) async {
    final d = _device;
    if (d == null) return false;

    Timer? pollTimer;
    pollTimer = Timer.periodic(_pairPollInterval, (_) async {
      try {
        onPairingStatus?.call(await readPairingStatus());
      } catch (_) {}
    });

    try {
      AppLog.i('ble', 'createBond…');
      await d.createBond();
      await refreshBondState();
      onPairingStatus?.call(await readPairingStatus());
      AppLog.i('ble', 'createBond done bonded=$_bonded');
      return _bonded;
    } catch (e, st) {
      AppLog.e('ble', 'createBond threw', e, st);
      rethrow;
    } finally {
      pollTimer.cancel();
    }
  }

  Future<void> removeBond() async {
    final d = _device;
    if (d != null) {
      await d.removeBond();
    }
    _bonded = false;
  }

  Future<List<BluetoothService>> discoverServices({bool refresh = false}) async {
    final d = _device;
    if (d == null) throw StateError('Not connected');
    if (!refresh && _cachedServices != null) {
      return _cachedServices!;
    }
    AppLog.i('gatt', 'discoverServices… (subscribeToServicesChanged=false)');
    final sw = Stopwatch()..start();
    // Android: FBP defaults to enabling 0x2A05 (Service Changed) indications
    // after discovery. On some phones that CCCD write times out / returns
    // ERROR_GATT_WRITE_REQUEST_BUSY and aborts the whole connect path.
    _cachedServices = await d.discoverServices(
      subscribeToServicesChanged: false,
      timeout: 20,
    );
    AppLog.i(
      'gatt',
      'discoverServices done in ${sw.elapsedMilliseconds}ms — '
      '${_cachedServices!.length} service(s)',
    );
    return _cachedServices!;
  }

  Future<int> requestMtuNegotiation({int preferred = _preferredMtu}) async {
    final d = _device;
    if (d == null) return 23;
    final current = d.mtuNow;
    if (current >= preferred) {
      AppLog.i('gatt', 'mtu already $current — skip request');
      return current;
    }
    try {
      // Small delay so Android's automatic/post-connect MTU settle first
      // (FBP documents a race if requestMtu runs too early).
      AppLog.i('gatt', 'requestMtu($preferred) after settle…');
      await Future<void>.delayed(const Duration(milliseconds: 400));
      await d.requestMtu(preferred, predelay: 0.35, timeout: 10);
    } catch (e) {
      AppLog.w('gatt', 'requestMtu failed (using mtuNow=${d.mtuNow})', e);
    }
    AppLog.i('gatt', 'mtuNow=${d.mtuNow}');
    return d.mtuNow;
  }

  Future<List<int>> readUuid(String uuid) async {
    final ch = await _findCharacteristic(uuid);
    return ch.read();
  }

  Future<void> writeUuid(String uuid, List<int> value, {bool withoutResponse = false}) async {
    final ch = await _findCharacteristic(uuid);
    await ch.write(value, withoutResponse: withoutResponse);
  }

  Future<int> getPpgDecimate() async {
    final raw = await readUuid(chrcPpgDecimate);
    return raw.isEmpty ? 3 : raw[0];
  }

  Future<void> setPpgDecimate(int factor) async {
    await writeUuid(chrcPpgDecimate, encodeUint8(factor.clamp(1, 33)));
  }

  Future<SamplingConfigData?> getSamplingConfig() async {
    final raw = await readUuid(chrcSamplingConfig);
    return decodeSamplingConfig(raw);
  }

  Future<void> setSamplingConfig(SamplingConfigData cfg) async {
    await writeUuid(chrcSamplingConfig, encodeSamplingConfig(cfg));
  }

  Future<int> getPpgPreference() async {
    final raw = await readUuid(chrcPpgPref);
    return raw.isEmpty ? 0 : raw[0];
  }

  Future<void> setPpgPreference(int pref) async {
    await writeUuid(chrcPpgPref, encodeUint8(pref));
  }

  /// Normalize BLE UUIDs so 16-bit (`2a2b`) matches 128-bit SIG form.
  static String _normalizeUuid(String uuid) {
    final compact = uuid.toLowerCase().replaceAll('-', '');
    if (compact.length == 4) {
      return '0000$compact-0000-1000-8000-00805f9b34fb';
    }
    if (compact.length == 8) {
      return '$compact-0000-1000-8000-00805f9b34fb';
    }
    if (compact.length == 32) {
      return '${compact.substring(0, 8)}-${compact.substring(8, 12)}-'
          '${compact.substring(12, 16)}-${compact.substring(16, 20)}-'
          '${compact.substring(20)}';
    }
    return uuid.toLowerCase();
  }

  static bool _uuidEquals(String a, String b) =>
      _normalizeUuid(a) == _normalizeUuid(b);

  Future<BluetoothCharacteristic> _findCharacteristic(String uuid) async {
    final services = await discoverServices();
    for (final svc in services) {
      for (final ch in svc.characteristics) {
        if (_uuidEquals(ch.uuid.toString(), uuid)) {
          return ch;
        }
      }
    }
    throw StateError('Characteristic not found: $uuid');
  }

  Future<int> readSecurityProfile() async {
    final raw = await readUuid(chrcSecurityProfile);
    final profile = decodeSecurityProfile(raw);
    if (profile != null) {
      _securityProfile = profile;
    }
    return _securityProfile;
  }

  Future<PairingStatus?> readPairingStatus() async {
    final raw = await readUuid(chrcPairingStatus);
    return decodePairingStatus(raw);
  }

  bool shouldSkipRead(String uuid) {
    if (_bonded) return false;
    return encryptedCharacteristicsForProfile(_securityProfile).contains(uuid.toLowerCase());
  }

  void setNotifyCallback(NotifyCallback? cb) => _notifyCallback = cb;

  /// Diff-update CCC subscriptions to exactly [desired] (after pairing/security
  /// filters). Disables notifies that are no longer needed so Android frees
  /// GATT notify slots (critical on API 29).
  Future<void> setNotifySubscriptions(
    Iterable<String> desired, {
    required bool pairedForHealth,
  }) async {
    final want = <String>{};
    for (final uuid in desired) {
      final key = uuid.toLowerCase();
      final isHealth = healthNotifyCharacteristics
          .map((u) => u.toLowerCase())
          .contains(key);
      if (isHealth && !pairedForHealth) continue;
      if (shouldSkipRead(uuid) && key != chrcMeasStatus.toLowerCase()) {
        continue;
      }
      want.add(key);
    }

    final active = _notifySubs.keys.toList();
    for (final uuid in active) {
      if (!want.contains(uuid)) {
        await _unsubscribeOne(uuid);
      }
    }

    var ok = 0;
    var fail = 0;
    for (final uuid in want) {
      if (_notifySubs.containsKey(uuid)) {
        ok++;
        continue;
      }
      if (await _subscribeOne(uuid)) {
        ok++;
      } else {
        fail++;
      }
    }
    AppLog.i(
      'gatt',
      'setNotifySubscriptions want=${want.length} ok=$ok fail/skip=$fail '
      'paired=$pairedForHealth',
    );
  }

  Future<void> setNotifyProfile(
    BleNotifyProfile profile, {
    required bool pairedForHealth,
  }) async {
    AppLog.i('gatt', 'setNotifyProfile $profile');
    await setNotifySubscriptions(
      notifyUuidsForProfile(profile),
      pairedForHealth: pairedForHealth,
    );
  }

  /// Legacy: enable the full live set (prefer [setNotifyProfile] / tab scoping).
  Future<void> subscribeAll({required bool pairedForHealth}) async {
    await setNotifyProfile(
      BleNotifyProfile.raw,
      pairedForHealth: pairedForHealth,
    );
  }

  Future<bool> _subscribeOne(String uuid) async {
    final key = uuid.toLowerCase();
    try {
      final ch = await _findCharacteristic(uuid);
      if (!ch.properties.notify && !ch.properties.indicate) {
        AppLog.d('gatt', 'skip $uuid (no notify/indicate)');
        return false;
      }
      await ch.setNotifyValue(true);
      final sub = ch.onValueReceived.listen((data) {
        _notifyCallback?.call(key, data);
      });
      _notifySubs[key] = sub;
      final d = _device;
      if (d != null) {
        d.cancelWhenDisconnected(sub);
      }
      AppLog.d('gatt', 'notify ON $uuid');
      return true;
    } catch (e) {
      AppLog.w('gatt', 'notify fail $uuid', e);
      return false;
    }
  }

  Future<void> _unsubscribeOne(String uuid) async {
    final key = uuid.toLowerCase();
    final sub = _notifySubs.remove(key);
    await sub?.cancel();
    try {
      final ch = await _findCharacteristic(uuid);
      await ch.setNotifyValue(false);
      AppLog.d('gatt', 'notify OFF $uuid');
    } catch (e) {
      AppLog.w('gatt', 'notify OFF fail $uuid', e);
    }
  }

  Future<void> _cancelAllNotifications() async {
    for (final uuid in _notifySubs.keys.toList()) {
      await _unsubscribeOne(uuid);
    }
  }

  Future<List<GattServiceNode>> buildGattTree() async {
    final services = await discoverServices(refresh: true);
    return services.map((svc) {
      final svcUuid = svc.uuid.toString().toLowerCase();
      return GattServiceNode(
        uuid: svcUuid,
        name: resolveGattName(svcUuid),
        characteristics: svc.characteristics.map((ch) {
          final chUuid = ch.uuid.toString().toLowerCase();
          final props = <String>[];
          if (ch.properties.read) props.add('read');
          if (ch.properties.write) props.add('write');
          if (ch.properties.writeWithoutResponse) props.add('write-without-response');
          if (ch.properties.notify) props.add('notify');
          if (ch.properties.indicate) props.add('indicate');
          return GattCharacteristicNode(
            uuid: chUuid,
            name: resolveGattName(chUuid),
            properties: props,
            characteristic: ch,
            descriptors: ch.descriptors.map((d) {
              final du = d.uuid.toString().toLowerCase();
              return GattDescriptorNode(uuid: du, name: resolveGattName(du));
            }).toList(),
          );
        }).toList(),
      );
    }).toList();
  }

  Future<void> syncRtcFromHost() async {
    final now = DateTime.now();
    final payload = encodeCurrentTime(now);
    AppLog.i(
      'cts',
      'write 0x2A2B ${now.toIso8601String()} '
      '(${payload.length} bytes, wday=${now.weekday})',
    );
    await writeUuid(chrcCurrentTime, payload);
    AppLog.i('cts', 'write complete');
  }
}
