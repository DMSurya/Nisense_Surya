import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:hcm/ble/connection_flow.dart';
import 'package:hcm/ble/hcm_ble_client.dart';
import 'package:hcm/ble/nisense_scan.dart';
import 'package:hcm/protocol/gatt_registry.dart';
import 'package:hcm/protocol/hcm_protocol.dart';

void main() {
  test('vitals struct size matches firmware', () {
    expect(vitalsStructSize, 26);
    expect(decodeVitals(List<int>.filled(18, 0))?.hrBpm, 0);
    expect(decodeVitals(List<int>.filled(26, 0))?.sdnn_ms, 0);
  });

  test('sensor_all size includes vascular vitals fields', () {
    expect(sensorAllStructSize, 59);
    final parsed = decodeSensorAll(List<int>.filled(sensorAllStructSize, 0));
    expect(parsed, isNotNull);
    expect(parsed!.pmic.battery_mv, 0);
  });

  test('chrc UUIDs use HCM base', () {
    expect(chrcVitals.toLowerCase(), contains('12345678-1234-5678-1234-56789abcf10b'));
  });

  test('gattName resolves HCM vitals', () {
    expect(resolveGattName(chrcVitals), contains('Vitals'));
  });

  test('gattName resolves SIG battery level', () {
    expect(
      resolveGattName('00002a19-0000-1000-8000-00805f9b34fb'),
      isNotEmpty,
    );
  });

  test('decode PPG and accel samples', () {
    final ppg = decodePpgSample(List<int>.filled(ppgSampleStructSize, 0));
    expect(ppg, isNotNull);
    expect(ppg!.sample_num, 0);

    final accel = decodeAccelSample(List<int>.filled(accelSampleStructSize, 0));
    expect(accel, isNotNull);
    expect(accel!.seq, 0);
  });

  test('decode glucose and mmol conversion', () {
    final data = Uint8List(glucoseStructSize);
    final view = ByteData.sublistView(data);
    view.setUint16(0, 120, Endian.little);
    view.setUint8(2, 90);
    view.setUint32(3, 1700000000, Endian.little);
    final g = decodeGlucose(data);
    expect(g, isNotNull);
    expect(g!.glucose_mg_dl, 120);
    expect(g.glucose_mmol_l, closeTo(6.66, 0.1));
  });

  test('ConnectionResult flags needsPair for secure unpaired', () {
    final result = ConnectionResult.success(
      profile: bleSecurityProfileSecure,
      paired: false,
    );
    expect(result.ok, isTrue);
    expect(result.needsPair, isTrue);
  });

  test('resolveNiSenseProductLabel detects Watch and Pulse names', () {
    ScanResult fake(String name) => ScanResult(
          device: BluetoothDevice.fromId('AA:BB:CC:DD:EE:FF'),
          advertisementData: AdvertisementData(
            advName: name,
            txPowerLevel: null,
            appearance: null,
            connectable: true,
            manufacturerData: {},
            serviceData: {},
            serviceUuids: const [],
          ),
          rssi: -50,
          timeStamp: DateTime.now(),
        );

    expect(resolveNiSenseProductLabel(fake('NiSense Watch')), nisenseWatchLabel);
    expect(resolveNiSenseProductLabel(fake('NiSense Pulse')), nisensePulseLabel);
    expect(resolveNiSenseProductLabel(fake('HCM')), nisenseWatchLabel);
    expect(resolveNiSenseProductLabel(fake('NiSense Pulse Clip')), nisensePulseLabel);
  });

  test('PMIC external power and charger labels', () {
    expect(isPmicExternalPowerOnly(2550), isTrue);
    expect(isPmicExternalPowerOnly(0), isFalse);
    expect(isPmicExternalPowerOnly(3700), isFalse);
    expect(isPmicExternalPowerOnly(3910, socPercent: 0), isTrue);
    expect(isPmicExternalPowerOnly(3910, socPercent: 76), isFalse);
    expect(formatPmicChargerStatus(pmicChargerCharging), 'Charging');
    expect(formatPmicChargerStatus(pmicChargerFull), 'Full');
    expect(
      formatPmicChargerStatus(pmicChargerCharging, externalPowerOnly: true),
      'USB powered (no pack)',
    );

    final data = Uint8List(pmicStructSize);
    final view = ByteData.sublistView(data);
    view.setUint16(0, 2550, Endian.little);
    view.setInt16(2, 0, Endian.little);
    view.setUint8(4, 0);
    view.setUint8(5, pmicChargerIdle);
    final p = decodePmic(data);
    expect(p, isNotNull);
    expect(describePmicBattery(p!), contains('PMIC ADC'));
    expect(describePmicBattery(p), contains('no pack'));
  });

  test('decodeBatteryLevel clamps SIG BAS payload', () {
    expect(decodeBatteryLevel([105]), 100);
    expect(decodeBatteryLevel([42]), 42);
    expect(decodeBatteryLevel([]), isNull);
  });

  test('isNiSenseScanResult accepts NiSense product names', () {
    expect(deviceNameAliases, contains('NiSense Watch'));
    expect(deviceNameAliases, contains('NiSense Pulse'));
  });
}
