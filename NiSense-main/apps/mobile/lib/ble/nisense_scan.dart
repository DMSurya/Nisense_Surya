import 'package:flutter_blue_plus/flutter_blue_plus.dart';

/// User-facing product labels in scan UI and known-device list.
const String nisenseWatchLabel = 'NiSense Watch';
const String nisensePulseLabel = 'NiSense Pulse';

/// Legacy / engineering names still seen in the field before firmware refresh.
const legacyBleNameTokens = [
  'hcm',
  'healthmonitor',
  'wearabledevice',
  'nisense',
];

const _watchTokens = ['watch', 'wearable', 'wrist', 'gwec'];
const _pulseTokens = ['pulse', 'finger', 'clip', 'nirs', 'max3010'];

/// Raw name from advertisement (prefers AD complete/local name over cached platform name).
String rawBleAdvertisedName(ScanResult r) {
  final adv = r.advertisementData.advName.trim();
  if (adv.isNotEmpty) return adv;
  return r.device.platformName.trim();
}

/// Maps advertisement text to NiSense Watch / NiSense Pulse.
String resolveNiSenseProductLabel(ScanResult r) {
  final raw = rawBleAdvertisedName(r);
  final lower = raw.toLowerCase();

  if (lower.contains('nisense pulse') || _containsAny(lower, _pulseTokens)) {
    return nisensePulseLabel;
  }
  if (lower.contains('nisense watch') || _containsAny(lower, _watchTokens)) {
    return nisenseWatchLabel;
  }

  if (isNiSenseScanResult(r)) {
    // Vendor service without profile hint — default Watch (primary SKU).
    return nisenseWatchLabel;
  }

  return raw.isNotEmpty ? raw : 'NiSense device';
}

/// Maps any BLE/GATT name string to a product label (post-connect f001 read).
String resolveNiSenseProductLabelFromName(String raw) {
  final lower = raw.trim().toLowerCase();
  if (lower.isEmpty) return nisenseWatchLabel;
  if (lower.contains('nisense pulse') || _containsAny(lower, _pulseTokens)) {
    return nisensePulseLabel;
  }
  if (lower.contains('nisense watch') || _containsAny(lower, _watchTokens)) {
    return nisenseWatchLabel;
  }
  if (_containsAny(lower, legacyBleNameTokens)) {
    return nisenseWatchLabel;
  }
  if (lower.contains('nisense')) return nisenseWatchLabel;
  return raw.trim();
}

bool _containsAny(String haystack, List<String> needles) {
  for (final n in needles) {
    if (haystack.contains(n)) return true;
  }
  return false;
}

bool _hasVendorService(ScanResult r) {
  return r.advertisementData.serviceUuids.any((u) {
    final s = u.toString().toLowerCase();
    return s.contains('56789abc') || s.contains('56789abcdef');
  });
}

/// True when a scan result looks like a NiSense/HCM wearable advertisement.
bool isNiSenseScanResult(ScanResult r) {
  final name = rawBleAdvertisedName(r).toLowerCase();
  final nameMatch = legacyBleNameTokens.any((a) => name.contains(a)) ||
      _containsAny(name, _watchTokens) ||
      _containsAny(name, _pulseTokens) ||
      name.contains('nisense watch') ||
      name.contains('nisense pulse');
  return nameMatch || _hasVendorService(r);
}
