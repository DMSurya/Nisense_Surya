import 'dart:io';

import 'package:permission_handler/permission_handler.dart';
import 'package:wifi_scan/wifi_scan.dart';

import '../util/app_log.dart';

/// Visible Wi-Fi AP discovered by the phone (not the wearable).
class WifiApInfo {
  const WifiApInfo({
    required this.ssid,
    required this.bssid,
    required this.capabilities,
    required this.security,
    required this.isSecure,
    required this.levelDbm,
    required this.frequencyMhz,
  });

  final String ssid;
  final String bssid;
  final String capabilities;
  final String security;
  final bool isSecure;
  final int levelDbm;
  final int frequencyMhz;

  factory WifiApInfo.fromAccessPoint(WiFiAccessPoint ap) {
    final security = securityLabel(ap.capabilities);
    return WifiApInfo(
      ssid: ap.ssid.trim(),
      bssid: ap.bssid,
      capabilities: ap.capabilities,
      security: security,
      isSecure: isSecureCapabilities(ap.capabilities),
      levelDbm: ap.level,
      frequencyMhz: ap.frequency,
    );
  }
}

String securityLabel(String capabilities) {
  final c = capabilities.toUpperCase();
  if (c.contains('WPA3') || c.contains('SAE')) return 'WPA3';
  if (c.contains('WPA2')) return 'WPA2';
  if (c.contains('WPA')) return 'WPA';
  if (c.contains('WEP')) return 'WEP';
  if (c.contains('ESS') &&
      !c.contains('WPA') &&
      !c.contains('WEP') &&
      !c.contains('SAE')) {
    return 'Open';
  }
  if (capabilities.trim().isEmpty) return 'Unknown';
  return capabilities;
}

bool isSecureCapabilities(String capabilities) {
  final c = capabilities.toUpperCase();
  return c.contains('WPA') ||
      c.contains('WEP') ||
      c.contains('SAE') ||
      c.contains('EAP');
}

/// Scans nearby Wi-Fi APs on the phone for BLE provisioning to the device.
class WifiApScanner {
  /// iOS has no public AP-scan API without Apple hotspot entitlements.
  bool get scanSupported => !Platform.isIOS;

  Future<bool> ensurePermissions() async {
    if (Platform.isIOS) return false;

    var loc = await Permission.locationWhenInUse.status;
    if (!loc.isGranted) {
      loc = await Permission.locationWhenInUse.request();
    }
    if (!loc.isGranted && !loc.isLimited) {
      AppLog.w('wifi-scan', 'location permission denied');
      return false;
    }

    if (await Permission.nearbyWifiDevices.isDenied) {
      await Permission.nearbyWifiDevices.request();
    }

    final service = await Permission.locationWhenInUse.serviceStatus;
    if (!service.isEnabled) {
      AppLog.w('wifi-scan', 'location services disabled');
      return false;
    }
    return true;
  }

  /// Trigger a scan and return unique SSIDs (best RSSI wins), strongest first.
  Future<List<WifiApInfo>> scan({
    Duration settle = const Duration(milliseconds: 1500),
  }) async {
    if (!scanSupported) {
      throw UnsupportedError(
        'iOS cannot scan Wi-Fi networks without Apple entitlements. '
        'Enter the SSID manually.',
      );
    }

    final okPerms = await ensurePermissions();
    if (!okPerms) {
      throw StateError(
        'Location permission (and Location toggle) are required to scan Wi-Fi.',
      );
    }

    final canStart = await WiFiScan.instance.canStartScan(askPermissions: true);
    if (canStart != CanStartScan.yes) {
      throw StateError('Cannot start Wi-Fi scan ($canStart).');
    }

    final started = await WiFiScan.instance.startScan();
    if (!started) {
      AppLog.w('wifi-scan', 'startScan returned false; using cached results');
    } else {
      await Future<void>.delayed(settle);
    }

    final canGet = await WiFiScan.instance.canGetScannedResults(
      askPermissions: true,
    );
    if (canGet != CanGetScannedResults.yes) {
      throw StateError('Cannot read Wi-Fi scan results ($canGet).');
    }

    final raw = await WiFiScan.instance.getScannedResults();
    final bySsid = <String, WifiApInfo>{};
    for (final ap in raw) {
      final ssid = ap.ssid.trim();
      if (ssid.isEmpty) continue;
      final info = WifiApInfo.fromAccessPoint(ap);
      final prev = bySsid[ssid];
      if (prev == null || info.levelDbm > prev.levelDbm) {
        bySsid[ssid] = info;
      }
    }

    final list = bySsid.values.toList()
      ..sort((a, b) => b.levelDbm.compareTo(a.levelDbm));
    AppLog.i('wifi-scan', 'found ${list.length} unique SSIDs');
    return list;
  }
}
