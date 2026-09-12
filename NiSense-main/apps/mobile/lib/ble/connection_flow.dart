import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'hcm_ble_client.dart';
import '../protocol/hcm_protocol.dart';
import '../util/app_log.dart';

/// Android connect flow: link → f008 → discover → bond (optional) → subscribe.
class ConnectionFlow {
  ConnectionFlow(this.client);

  final HcmBleClient client;

  Future<ConnectionResult> connectAndSetup(
    BluetoothDevice device, {
    void Function(PairingStatus?)? onPairingStatus,
    bool attemptBond = true,
  }) async {
    final id = device.remoteId.str;
    AppLog.i('flow', '=== connectAndSetup begin id=$id attemptBond=$attemptBond ===');

    final ok = await client.connect(device);
    if (!ok) {
      AppLog.e('flow', 'link connect failed id=$id');
      return ConnectionResult.failed(
        'Connect failed (timeout or link dropped). '
        'Check phone BT is on, device advertising, and not bonded to another phone.',
      );
    }
    AppLog.i('flow', 'link up');

    // Discover first so later reads reuse the cache and do not re-enter
    // discoverServices (which previously raced on 0x2A05 CCCD writes).
    try {
      await client.discoverServices();
    } catch (e, st) {
      AppLog.e('flow', 'discoverServices failed', e, st);
      return ConnectionResult.failed('GATT discovery failed: $e');
    }

    var profile = bleSecurityProfileSecure;
    try {
      profile = await client.readSecurityProfile();
      AppLog.i('flow', 'security profile=$profile');
    } catch (e) {
      AppLog.w('flow', 'readSecurityProfile failed — assuming secure', e);
    }

    final mtu = await client.requestMtuNegotiation();
    var paired = await client.refreshBondState();
    AppLog.i('flow', 'bondState bonded=$paired');

    if (attemptBond && profile == bleSecurityProfileSecure && !paired) {
      AppLog.i('flow', 'createBond…');
      try {
        paired = await client.bond(onPairingStatus: onPairingStatus);
        AppLog.i('flow', 'createBond result bonded=$paired');
      } catch (e, st) {
        AppLog.e('flow', 'createBond failed', e, st);
        paired = false;
      }
    }

    try {
      await client.syncRtcFromHost();
    } catch (e, st) {
      AppLog.w('flow', 'CTS RTC sync failed (non-fatal)', e);
      AppLog.d('flow', '$st');
    }

    try {
      // Minimal CCC only — screens subscribe on demand (see BleNotifyProfile).
      await client.setNotifyProfile(
        BleNotifyProfile.minimal,
        pairedForHealth: paired,
      );
    } catch (e, st) {
      AppLog.e('flow', 'minimal notify subscribe failed', e, st);
      return ConnectionResult.failed('Notify subscribe failed: $e');
    }

    AppLog.i(
      'flow',
      '=== connectAndSetup OK profile=$profile paired=$paired mtu=$mtu ===',
    );
    return ConnectionResult.success(
      profile: profile,
      paired: paired,
      mtu: mtu,
    );
  }
}

class ConnectionResult {
  const ConnectionResult._({
    required this.ok,
    this.error,
    this.profile = bleSecurityProfileSecure,
    this.paired = false,
    this.needsPair = false,
    this.mtu = 0,
  });

  factory ConnectionResult.success({
    required int profile,
    required bool paired,
    int mtu = 0,
  }) =>
      ConnectionResult._(
        ok: true,
        profile: profile,
        paired: paired,
        needsPair: profile == bleSecurityProfileSecure && !paired,
        mtu: mtu,
      );

  factory ConnectionResult.failed(String error) =>
      ConnectionResult._(ok: false, error: error);

  factory ConnectionResult.needsPairing(String error) =>
      ConnectionResult._(ok: false, error: error, needsPair: true);

  final bool ok;
  final String? error;
  final int profile;
  final bool paired;
  final bool needsPair;
  final int mtu;
}
