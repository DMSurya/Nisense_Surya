import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../state/hcm_backend.dart';
import '../../theme/nisense_colors.dart';

class RawPage extends StatelessWidget {
  const RawPage({super.key});

  @override
  Widget build(BuildContext context) {
    final b = context.watch<HcmBackend>();
    final ppg = b.ppgSamples;
    final glucose = b.glucoseSamples;
    final idle = ppg.isEmpty && glucose.isEmpty;

    return ListView(
      padding: const EdgeInsets.all(12),
      children: [
        if (idle)
          const _IdleCard()
        else ...[
          if (ppg.isNotEmpty) _PpgChart(samples: ppg),
          if (ppg.isNotEmpty) const SizedBox(height: 12),
          if (glucose.isNotEmpty) _GlucoseChart(samples: glucose),
        ],
      ],
    );
  }
}

class _IdleCard extends StatelessWidget {
  const _IdleCard();

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.timeline, size: 48, color: NiSenseColors.textMuted),
            const SizedBox(height: 12),
            Text(
              'Start a measurement to see raw samples',
              textAlign: TextAlign.center,
              style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                    color: NiSenseColors.textMuted,
                  ),
            ),
          ],
        ),
      ),
    );
  }
}

class _PpgChart extends StatelessWidget {
  const _PpgChart({required this.samples});

  final List<dynamic> samples;

  @override
  Widget build(BuildContext context) {
    final irSpots = <FlSpot>[];
    final redSpots = <FlSpot>[];
    final greenSpots = <FlSpot>[];
    for (var i = 0; i < samples.length; i++) {
      final s = samples[i];
      irSpots.add(FlSpot(i.toDouble(), s.raw_ir.toDouble()));
      redSpots.add(FlSpot(i.toDouble(), s.raw_red.toDouble()));
      greenSpots.add(FlSpot(i.toDouble(), s.raw_green.toDouble()));
    }

    return _RawChartCard(
      title: 'PPG Raw — IR / Red / Green',
      bars: [
        _BarSpec(spots: irSpots, color: NiSenseColors.secondary, label: 'IR'),
        _BarSpec(spots: redSpots, color: NiSenseColors.accentRed, label: 'Red'),
        _BarSpec(spots: greenSpots, color: NiSenseColors.statusNormal, label: 'Green'),
      ],
    );
  }
}

class _GlucoseChart extends StatelessWidget {
  const _GlucoseChart({required this.samples});

  final List<dynamic> samples;

  @override
  Widget build(BuildContext context) {
    final adcSpots = <FlSpot>[];
    final mvSpots = <FlSpot>[];
    for (var i = 0; i < samples.length; i++) {
      final s = samples[i];
      adcSpots.add(FlSpot(i.toDouble(), s.raw_adc_value.toDouble()));
      mvSpots.add(FlSpot(i.toDouble(), (s.voltage_mv * 100).roundToDouble()));
    }

    return _RawChartCard(
      title: 'Glucose Raw — ADC / mV×100',
      bars: [
        _BarSpec(spots: adcSpots, color: NiSenseColors.paramGlucose, label: 'ADC'),
        _BarSpec(spots: mvSpots, color: NiSenseColors.paramHb, label: 'mV×100'),
      ],
    );
  }
}

class _BarSpec {
  const _BarSpec({required this.spots, required this.color, required this.label});
  final List<FlSpot> spots;
  final Color color;
  final String label;
}

class _RawChartCard extends StatelessWidget {
  const _RawChartCard({required this.title, required this.bars});

  final String title;
  final List<_BarSpec> bars;

  @override
  Widget build(BuildContext context) {
    final hasData = bars.any((b) => b.spots.length >= 2);
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Expanded(
                  child: Text(title, style: Theme.of(context).textTheme.titleSmall),
                ),
                // Legend
                for (final bar in bars) ...[
                  Container(width: 10, height: 10, color: bar.color),
                  const SizedBox(width: 4),
                  Text(bar.label, style: Theme.of(context).textTheme.bodySmall),
                  const SizedBox(width: 8),
                ],
              ],
            ),
            const SizedBox(height: 8),
            SizedBox(
              height: 180,
              child: !hasData
                  ? const Center(child: Text('Collecting samples…'))
                  : LineChart(
                      LineChartData(
                        gridData: const FlGridData(show: false),
                        titlesData: const FlTitlesData(show: false),
                        borderData: FlBorderData(show: false),
                        lineBarsData: [
                          for (final bar in bars)
                            if (bar.spots.length >= 2)
                              LineChartBarData(
                                spots: bar.spots,
                                color: bar.color,
                                dotData: const FlDotData(show: false),
                                barWidth: 1.2,
                                belowBarData: BarAreaData(
                                  show: true,
                                  color: bar.color.withValues(alpha: 0.08),
                                ),
                              ),
                        ],
                      ),
                    ),
            ),
          ],
        ),
      ),
    );
  }
}
