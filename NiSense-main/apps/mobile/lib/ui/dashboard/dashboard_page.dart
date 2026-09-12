import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../protocol/hcm_protocol.dart';
import '../../state/hcm_backend.dart';
import '../../theme/nisense_icons.dart';
import '../device/device_info_page.dart';
import '../widgets/common_widgets.dart';
import 'metric_history_page.dart';

class DashboardPage extends StatelessWidget {
  const DashboardPage({super.key});

  void _pushHistory(BuildContext ctx, String key, String label) {
    Navigator.of(ctx).push(
      MaterialPageRoute<void>(
        builder: (_) => MetricHistoryPage(metricKey: key, metricLabel: label),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final b = context.watch<HcmBackend>();
    return ListView(
      padding: const EdgeInsets.all(12),
      children: [
        // Patient name + info icon row
        Row(
          children: [
            Expanded(
              child: Text(
                b.patientName,
                style: Theme.of(context).textTheme.titleMedium,
                overflow: TextOverflow.ellipsis,
              ),
            ),
            IconButton(
              tooltip: 'Device Info',
              icon: const Icon(Icons.info_outline),
              onPressed: () => Navigator.of(context).push(
                MaterialPageRoute<void>(builder: (_) => const DeviceInfoPage()),
              ),
            ),
          ],
        ),
        Row(
          children: [
            ConnectionPill(connected: b.connected, label: b.deviceName),
            const Spacer(),
            if (b.needsPair)
              TextButton(onPressed: () => b.pairDevice(), child: const Text('Pair')),
          ],
        ),
        if (b.needsPair)
          GlassCard(
            child: const Text(
              'SECURE profile — pair to receive vitals, glucose, and streams. '
              'Measurement status is live.',
            ),
          ),
        if (b.pairingStateLabel == 'confirm_passkey' ||
            b.pairingStateLabel == 'bonding' ||
            b.pairingPasskey > 0)
          GlassCard(
            child: Text('Passkey: ${b.pairingPasskey.toString().padLeft(6, '0')}'),
          ),
        const SizedBox(height: 8),
        GridView.count(
          crossAxisCount: 2,
          shrinkWrap: true,
          physics: const NeverScrollableScrollPhysics(),
          mainAxisSpacing: 8,
          crossAxisSpacing: 8,
          childAspectRatio: 1.4,
          children: [
            MetricBadge(
              label: 'Heart Rate',
              value: b.hrBpm > 0 ? '${b.hrBpm}' : '--',
              unit: 'bpm',
              paramKey: 'hr',
              confidence: b.hrConf > 0 ? b.hrConf : null,
              onTap: () => _pushHistory(context, 'hr', 'Heart Rate'),
            ),
            MetricBadge(
              label: 'SpO₂',
              value: b.spo2Pct > 0 ? '${b.spo2Pct}' : '--',
              unit: '%',
              paramKey: 'spo2',
              confidence: b.spo2Conf > 0 ? b.spo2Conf : null,
              onTap: () => _pushHistory(context, 'spo2', 'SpO₂'),
            ),
            MetricBadge(
              label: 'Hemoglobin',
              value: b.hbGdl > 0 ? b.hbGdl.toStringAsFixed(1) : '--',
              unit: 'g/dL',
              paramKey: 'hb',
              onTap: () => _pushHistory(context, 'hb', 'Hemoglobin'),
            ),
            MetricBadge(
              label: 'Resp Rate',
              value: b.respBpm > 0 ? '${b.respBpm}' : '--',
              unit: 'bpm',
              paramKey: 'resp',
              onTap: () => _pushHistory(context, 'resp', 'Resp Rate'),
            ),
            MetricBadge(
              label: 'Glucose',
              value: b.glucoseMgDl > 0 ? '${b.glucoseMgDl}' : '--',
              unit: 'mg/dL',
              paramKey: 'glucose',
              onTap: () => _pushHistory(context, 'glucose', 'Glucose'),
            ),
            MetricBadge(
              label: 'Body Temp',
              value: b.tempC > 0 ? b.tempC.toStringAsFixed(1) : '--',
              unit: '°C',
              paramKey: 'temp',
              onTap: () => _pushHistory(context, 'temp', 'Body Temperature'),
            ),
            MetricBadge(
              label: 'Insulin',
              value: b.insulinUiUml > 0 ? b.insulinUiUml.toStringAsFixed(1) : '--',
              unit: 'µIU/mL',
              paramKey: 'insulin',
              onTap: () => _pushHistory(context, 'insulin', 'Insulin'),
            ),
            MetricBadge(
              label: 'HOMA-IR',
              value: b.homaIr > 0 ? b.homaIr.toStringAsFixed(2) : '--',
              unit: '',
              paramKey: 'homa',
              onTap: () => _pushHistory(context, 'homa', 'HOMA-IR'),
            ),
            MetricBadge(
              label: 'SDNN',
              value: b.sdnnMs > 0 ? '${b.sdnnMs}' : '--',
              unit: 'ms',
              paramKey: 'hrv',
              onTap: () => _pushHistory(context, 'hrv', 'HRV (SDNN)'),
            ),
            MetricBadge(
              label: 'Blood Pressure',
              value: b.systolicMmhg > 0
                  ? '${b.systolicMmhg}/${b.diastolicMmhg}'
                  : '--',
              unit: 'mmHg',
              paramKey: 'bp',
              onTap: () => _pushHistory(context, 'bp', 'Blood Pressure'),
            ),
          ],
        ),
        const SizedBox(height: 12),
        GlassCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  NiSenseIcons.paramIcon('battery', size: 20),
                  const SizedBox(width: 8),
                  Expanded(
                    child: Text(
                      b.externalPowerOnly
                          ? 'External power (no battery)'
                          : 'Battery ${b.battSoc > 0 ? '${b.battSoc}%' : '--'}',
                      style: Theme.of(context).textTheme.titleSmall,
                    ),
                  ),
                ],
              ),
              if (b.battMv > 0)
                Text('VBAT ${b.battMv} mV · PMIC ADC · ${b.battMa} mA'),
              if (b.connected)
                Text('Charger: ${b.chargerStatusLabel}'),
              if (b.externalPowerOnly)
                Text(
                  'SOC clamped to 0% — no Li-ion cell detected on BAT pin',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              if (!b.externalPowerOnly &&
                  b.remainingMah > 0 &&
                  (b.designMah > 0 || b.fullMah > 0))
                Text(
                  'Capacity: ${b.remainingMah} / ${b.designMah > 0 ? b.designMah : b.fullMah} mAh (fuel gauge)',
                ),
              const SizedBox(height: 4),
              Text('Wear: ${b.wearContact ? 'contact' : 'off-wrist'}'),
              if (b.accelX != 0 || b.accelY != 0 || b.accelZ != 0)
                Text('Accel: ${b.accelX}, ${b.accelY}, ${b.accelZ} mg'),
              if (b.measActive) Text('Measurement ${b.measPct}%'),
            ],
          ),
        ),
        const SizedBox(height: 12),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            SizedBox(
              width: 148,
              child: HoldToStartButton(
                label: 'Start Vitals',
                enabled: b.connected && !b.measActive,
                holdDuration: const Duration(milliseconds: measureLongPressMs),
                onHoldComplete: () {
                  b.startVitals(skipProximity: true);
                },
              ),
            ),
            SizedBox(
              width: 156,
              child: HoldToStartButton(
                label: 'Start Glucose',
                enabled: b.connected && !b.measActive,
                holdDuration: const Duration(milliseconds: measureLongPressMs),
                onHoldComplete: () {
                  b.startGlucose(skipProximity: true);
                },
              ),
            ),
            OutlinedButton(
              onPressed: b.connected ? () => b.stopMeasurement() : null,
              child: const Text('Stop'),
            ),
          ],
        ),
        if (b.connected)
          Padding(
            padding: const EdgeInsets.only(top: 6),
            child: Text(
              'Hold ${measureLongPressMs ~/ 1000}s to start (proximity bypass)',
              style: Theme.of(context).textTheme.bodySmall,
            ),
          ),
      ],
    );
  }
}
