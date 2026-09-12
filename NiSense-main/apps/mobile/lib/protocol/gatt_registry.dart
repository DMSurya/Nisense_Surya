import 'generated/sig_uuids.g.dart';
import 'hcm_protocol.dart';

export 'hcm_protocol.dart';

/// Resolve GATT UUID to human-readable name (HCM registry → Nordic SIG → fallback).
String resolveGattName(String uuid, {String fallback = ''}) {
  final u = uuid.toLowerCase();
  final hcm = gattNames[u];
  if (hcm != null) {
    return hcm;
  }
  final sig = sigUuidNames[u];
  if (sig != null) {
    return sig;
  }
  if (fallback.isNotEmpty &&
      fallback != 'Unknown' &&
      fallback != 'Vendor specific') {
    return fallback;
  }
  if (u.endsWith('-0000-1000-8000-00805f9b34fb') && u.startsWith('0000')) {
    return '0x${u.substring(4, 8).toUpperCase()}';
  }
  if (u.contains('56789abc')) {
    return 'HCM …${u.substring(u.length - 4)}';
  }
  return 'Custom';
}

/// Security level hint for GATT explorer badges.
String securityLevelForUuid(String uuid, int profile) {
  final u = uuid.toLowerCase();
  if (authenticatedBleCharacteristics.contains(u)) {
    return 'L3';
  }
  if (encryptedCharacteristicsForProfile(profile).contains(u)) {
    return 'L2';
  }
  return 'L1';
}
