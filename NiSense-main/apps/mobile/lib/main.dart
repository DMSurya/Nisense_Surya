import 'package:flutter/foundation.dart';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:provider/provider.dart';

import 'state/hcm_backend.dart';
import 'theme/nisense_theme.dart';
import 'ui/splash_gate.dart';
import 'util/app_log.dart';

Future<void> _requestBlePermissions() async {
  if (defaultTargetPlatform != TargetPlatform.android) {
    return;
  }
  // Android 12+ (API 31) uses the split BLUETOOTH_SCAN/CONNECT runtime perms.
  // Requesting them on Android 10-11 is a no-op, and location is what actually
  // gates BLE scanning there — so request all and let the OS grant what applies.
  final statuses = await [
    Permission.bluetoothScan,
    Permission.bluetoothConnect,
    Permission.locationWhenInUse,
  ].request();
  AppLog.i(
    'perm',
    'scan=${statuses[Permission.bluetoothScan]} '
    'connect=${statuses[Permission.bluetoothConnect]} '
    'location=${statuses[Permission.locationWhenInUse]}',
  );
  // On Android 10-11, scanning silently returns no results without fine
  // location; surface a retry if the user denied it.
  final location = statuses[Permission.locationWhenInUse];
  if (location != null && location.isPermanentlyDenied) {
    AppLog.w('perm', 'location permanently denied — scan may return empty on Android 10-11');
  }
}

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  if (kDebugMode) {
    await FlutterBluePlus.setLogLevel(LogLevel.verbose, color: false);
  }
  AppLog.i('app', 'NiSense Link starting (debug=$kDebugMode)');
  await _requestBlePermissions();
  runApp(
    ChangeNotifierProvider(
      create: (_) => HcmBackend(),
      child: const HcmApp(),
    ),
  );
}

class HcmApp extends StatelessWidget {
  const HcmApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'NiSense Link',
      theme: NiSenseTheme.dark(),
      home: const SplashGate(),
    );
  }
}
