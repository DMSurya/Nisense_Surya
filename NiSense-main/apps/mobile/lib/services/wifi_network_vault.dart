import 'dart:convert';

import 'package:flutter_secure_storage/flutter_secure_storage.dart';

class WifiNetworkCredentials {
  const WifiNetworkCredentials({required this.ssid, required this.psk});

  final String ssid;
  final String psk;

  Map<String, dynamic> toJson() => {'ssid': ssid, 'psk': psk};

  factory WifiNetworkCredentials.fromJson(Map<String, dynamic> json) {
    return WifiNetworkCredentials(
      ssid: (json['ssid'] as String?) ?? '',
      psk: (json['psk'] as String?) ?? '',
    );
  }
}

/// Encrypted vault for premises Wi-Fi and the phone hotspot profile.
class WifiNetworkVault {
  WifiNetworkVault({FlutterSecureStorage? storage})
    : _storage = storage ?? const FlutterSecureStorage();

  static const _networksKey = 'nisense_wifi_network_vault_v1';
  static const _hotspotKey = 'nisense_wifi_hotspot_profile_v1';

  final FlutterSecureStorage _storage;

  Future<Map<String, String>> loadNetworks() async {
    final raw = await _storage.read(key: _networksKey);
    if (raw == null || raw.isEmpty) return const {};
    final decoded = jsonDecode(raw) as Map<String, dynamic>;
    return decoded.map((key, value) => MapEntry(key, value as String));
  }

  Future<String?> loadPsk(String ssid) async {
    final networks = await loadNetworks();
    return networks[_normalizeSsid(ssid)];
  }

  Future<void> saveNetwork(String ssid, String psk) async {
    final normalized = _normalizeSsid(ssid);
    if (normalized.isEmpty) {
      throw ArgumentError.value(ssid, 'ssid', 'SSID must not be empty');
    }
    final networks = Map<String, String>.from(await loadNetworks());
    networks[normalized] = psk;
    await _storage.write(key: _networksKey, value: jsonEncode(networks));
  }

  Future<void> deleteNetwork(String ssid) async {
    final networks = Map<String, String>.from(await loadNetworks());
    networks.remove(_normalizeSsid(ssid));
    await _storage.write(key: _networksKey, value: jsonEncode(networks));
  }

  Future<WifiNetworkCredentials?> loadHotspotProfile() async {
    final raw = await _storage.read(key: _hotspotKey);
    if (raw == null || raw.isEmpty) return null;
    final decoded = jsonDecode(raw) as Map<String, dynamic>;
    final creds = WifiNetworkCredentials.fromJson(decoded);
    if (creds.ssid.isEmpty) return null;
    return creds;
  }

  Future<void> saveHotspotProfile(String ssid, String psk) async {
    final normalized = _normalizeSsid(ssid);
    if (normalized.isEmpty) {
      throw ArgumentError.value(ssid, 'ssid', 'SSID must not be empty');
    }
    final creds = WifiNetworkCredentials(ssid: normalized, psk: psk);
    await _storage.write(key: _hotspotKey, value: jsonEncode(creds.toJson()));
  }

  String _normalizeSsid(String ssid) {
    var s = ssid.trim();
    if (s.length >= 2 && s.startsWith('"') && s.endsWith('"')) {
      s = s.substring(1, s.length - 1);
    }
    return s;
  }
}
