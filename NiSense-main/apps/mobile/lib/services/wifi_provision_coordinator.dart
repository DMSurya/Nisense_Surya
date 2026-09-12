import 'dart:async';
import 'dart:io';

import 'package:flutter/services.dart';
import 'package:network_info_plus/network_info_plus.dart';
import 'package:permission_handler/permission_handler.dart';

import '../state/hcm_backend.dart';
import '../util/app_log.dart';
import 'wifi_network_vault.dart';

typedef WifiPasswordPrompt = Future<String?> Function(String ssid);
typedef WifiInstructionSink = void Function(String message);
typedef WifiHotspotReadyConfirm = Future<bool> Function(String ssid);
typedef WifiHotspotCredentialsPrompt =
    Future<WifiNetworkCredentials?> Function();

class WifiProvisionResult {
  const WifiProvisionResult({
    required this.success,
    required this.ssid,
    this.message = '',
    this.usedHotspot = false,
  });

  final bool success;
  final String ssid;
  final String message;
  final bool usedHotspot;
}

class WifiSsidDetectResult {
  const WifiSsidDetectResult({
    this.ssid,
    this.error,
    this.locationEnabled = true,
    this.locationGranted = true,
  });

  final String? ssid;
  final String? error;
  final bool locationEnabled;
  final bool locationGranted;

  bool get ok => ssid != null && ssid!.isNotEmpty;

  String get userMessage {
    switch (error) {
      case 'location_permission':
        return 'Allow Location permission so NiSense can read the current Wi-Fi name.';
      case 'location_disabled':
        return 'Turn on Location (GPS) in system Settings — Android requires it to read the Wi-Fi name.';
      case 'unavailable':
        return 'Could not read the current Wi-Fi name. Confirm the phone is on Wi-Fi, then retry.';
      default:
        return ok
            ? 'Detected phone Wi-Fi: $ssid'
            : 'Could not auto-detect SSID — enter it manually (same network as this phone).';
    }
  }
}

/// Coordinates BLE Wi-Fi provisioning before local bulk transfer.
class WifiProvisionCoordinator {
  WifiProvisionCoordinator({required this.backend, WifiNetworkVault? vault})
    : _vault = vault ?? WifiNetworkVault();

  static const _wifiChannel = MethodChannel('com.aarms.hcm/wifi_info');

  final HcmBackend backend;
  final WifiNetworkVault _vault;
  final NetworkInfo _networkInfo = NetworkInfo();

  Future<bool> ensureWifiSsidPermissions() async {
    var loc = await Permission.locationWhenInUse.status;
    if (!loc.isGranted) {
      loc = await Permission.locationWhenInUse.request();
    }
    if (loc.isPermanentlyDenied) {
      AppLog.w('wifi-sync', 'location permanently denied for SSID detect');
      return false;
    }
    if (!loc.isGranted && !loc.isLimited) {
      AppLog.w('wifi-sync', 'location permission denied; SSID detect may fail');
      return false;
    }
    return true;
  }

  /// Best-effort current phone Wi-Fi SSID with a concrete failure reason.
  Future<WifiSsidDetectResult> detectCurrentSsid() async {
    try {
      final granted = await ensureWifiSsidPermissions();
      if (!granted) {
        return const WifiSsidDetectResult(
          error: 'location_permission',
          locationGranted: false,
        );
      }

      if (Platform.isAndroid) {
        final raw = await _wifiChannel.invokeMethod<dynamic>('getWifiSsid');
        if (raw is Map) {
          final map = Map<String, dynamic>.from(raw);
          final ssid = _normalizeSsid((map['ssid'] as String?) ?? '');
          final error = map['error'] as String?;
          final locationEnabled = map['locationEnabled'] as bool? ?? true;
          final locationGranted = map['locationGranted'] as bool? ?? true;
          if (ssid.isNotEmpty) {
            return WifiSsidDetectResult(
              ssid: ssid,
              locationEnabled: locationEnabled,
              locationGranted: locationGranted,
            );
          }
          return WifiSsidDetectResult(
            error: error ?? 'unavailable',
            locationEnabled: locationEnabled,
            locationGranted: locationGranted,
          );
        }
      }

      final service = await Permission.locationWhenInUse.serviceStatus;
      if (!service.isEnabled) {
        return const WifiSsidDetectResult(
          error: 'location_disabled',
          locationEnabled: false,
        );
      }

      final raw = await _networkInfo.getWifiName();
      final ssid = _normalizeSsid(raw ?? '');
      if (ssid.isEmpty || ssid.toLowerCase() == '<unknown ssid>') {
        return const WifiSsidDetectResult(error: 'unavailable');
      }
      return WifiSsidDetectResult(ssid: ssid);
    } catch (e) {
      AppLog.w('wifi-sync', 'SSID auto-detect failed', e);
      return const WifiSsidDetectResult(error: 'unavailable');
    }
  }

  /// Convenience wrapper used by provisioning (null when undetectable).
  Future<String?> readCurrentSsid() async {
    final result = await detectCurrentSsid();
    return result.ssid;
  }

  Future<bool> hasVaultedPsk(String ssid) async {
    final psk = await _vault.loadPsk(ssid);
    return psk != null && psk.isNotEmpty;
  }

  Future<WifiNetworkCredentials?> loadHotspotProfile() =>
      _vault.loadHotspotProfile();

  Future<void> saveHotspotProfile(String ssid, String psk) =>
      _vault.saveHotspotProfile(ssid, psk);

  /// Phone AP pick → BLE GATT write SSID/PSK → connect → wait for status notify.
  Future<WifiProvisionResult> provisionSelectedNetwork({
    required String ssid,
    required String password,
    bool saveToVault = true,
    Duration connectWait = const Duration(seconds: 20),
    WifiInstructionSink? instructions,
  }) async {
    final normalized = _normalizeSsid(ssid);
    if (normalized.isEmpty) {
      return const WifiProvisionResult(
        success: false,
        ssid: '',
        message: 'SSID is empty.',
      );
    }

    if (saveToVault && password.isNotEmpty) {
      await _vault.saveNetwork(normalized, password);
    }

    instructions?.call('Writing Wi-Fi credentials over BLE…');
    await backend.setWifiCredentials(normalized, password);
    instructions?.call('Asking device to join $normalized…');
    await backend.wifiConnect();
    final ok = await _waitForDeviceWifi(normalized, connectWait);
    return WifiProvisionResult(
      success: ok,
      ssid: normalized,
      message: ok
          ? 'Device joined $normalized.'
          : 'Device did not report Wi-Fi ready for $normalized.',
    );
  }

  Future<WifiProvisionResult> provisionPremisesWifi({
    String? manualSsid,
    String? password,
    WifiPasswordPrompt? askPassword,
    Duration connectWait = const Duration(seconds: 12),
  }) async {
    final ssid = _normalizeSsid(manualSsid ?? '');
    if (ssid.isEmpty) {
      return const WifiProvisionResult(
        success: false,
        ssid: '',
        message: 'Select a Wi-Fi network (scan) or enter an SSID.',
      );
    }

    var psk = password;
    psk ??= await _vault.loadPsk(ssid);
    if (psk == null && askPassword != null) {
      psk = await askPassword(ssid);
    }
    if (psk == null) {
      return WifiProvisionResult(
        success: false,
        ssid: ssid,
        message: 'Password required for $ssid.',
      );
    }

    return provisionSelectedNetwork(
      ssid: ssid,
      password: psk,
      connectWait: connectWait,
    );
  }

  Future<WifiProvisionResult> provisionHotspotFallback({
    WifiInstructionSink? instructions,
    WifiHotspotReadyConfirm? confirmHotspotReady,
    WifiHotspotCredentialsPrompt? askHotspotCredentials,
    Duration connectWait = const Duration(seconds: 18),
  }) async {
    var hotspot = await _vault.loadHotspotProfile();
    if (hotspot == null && askHotspotCredentials != null) {
      hotspot = await askHotspotCredentials();
      if (hotspot != null &&
          hotspot.ssid.isNotEmpty &&
          hotspot.psk.isNotEmpty) {
        await _vault.saveHotspotProfile(hotspot.ssid, hotspot.psk);
      }
    }
    if (hotspot == null || hotspot.ssid.isEmpty || hotspot.psk.isEmpty) {
      return const WifiProvisionResult(
        success: false,
        ssid: '',
        message:
            'Save a hotspot SSID/password before using hotspot fallback.',
        usedHotspot: true,
      );
    }

    instructions?.call(
      'Turn on Personal Hotspot named "${hotspot.ssid}", then return here.',
    );

    final opened = await openAppSettings();
    if (!opened) {
      instructions?.call(
        'Open Settings → Personal Hotspot, enable "${hotspot.ssid}".',
      );
    }

    if (confirmHotspotReady != null) {
      final ready = await confirmHotspotReady(hotspot.ssid);
      if (!ready) {
        return WifiProvisionResult(
          success: false,
          ssid: hotspot.ssid,
          message: 'Hotspot setup cancelled.',
          usedHotspot: true,
        );
      }
    } else {
      await Future<void>.delayed(const Duration(seconds: 3));
    }

    await backend.setWifiCredentials(hotspot.ssid, hotspot.psk);
    await backend.wifiConnect();
    final ok = await _waitForDeviceWifi(hotspot.ssid, connectWait);
    return WifiProvisionResult(
      success: ok,
      ssid: hotspot.ssid,
      usedHotspot: true,
      message: ok
          ? 'Device joined hotspot ${hotspot.ssid}.'
          : 'Device did not report hotspot Wi-Fi ready.',
    );
  }

  Future<bool> _waitForDeviceWifi(String ssid, Duration timeout) async {
    final deadline = DateTime.now().add(timeout);
    while (DateTime.now().isBefore(deadline)) {
      if (backend.wifiConnected > 0 &&
          (backend.wifiSsid.isEmpty || backend.wifiSsid == ssid) &&
          backend.wifiIp.isNotEmpty &&
          backend.wifiIp != '0.0.0.0') {
        return true;
      }
      await Future<void>.delayed(const Duration(milliseconds: 500));
    }
    return backend.wifiConnected > 0 &&
        (backend.wifiSsid.isEmpty || backend.wifiSsid == ssid);
  }

  String _normalizeSsid(String ssid) {
    var s = ssid.trim();
    if (s.length >= 2 && s.startsWith('"') && s.endsWith('"')) {
      s = s.substring(1, s.length - 1);
    }
    return s;
  }
}
