import 'dart:typed_data';

import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';

import '../../protocol/hcm_protocol.dart';
import '../../services/record_local_store.dart';
import '../../theme/nisense_colors.dart';

/// One point in a metric's history, decoded from a persisted record.
class _HistoryPoint {
  _HistoryPoint(this.time, this.value, {this.value2});
  final DateTime time;
  final double value;
  final double? value2; // e.g. diastolic for blood pressure
}

/// Static description of how to derive a dashboard metric's history from
/// the locally persisted NOR records (see record_local_store.dart).
class _MetricSpec {
  const _MetricSpec({
    required this.recordType,
    required this.unit,
    required this.color,
    this.color2,
    this.label2,
    required this.extract,
  });

  final int recordType;
  final String unit;
  final Color color;
  final Color? color2;
  final String? label2;

  /// Decodes one record's payload into a history point, or null if the
  /// record is too short / not applicable.
  final _HistoryPoint? Function(int recordId, DateTime receivedAt, Uint8List payload) extract;
}

_MetricSpec? _specFor(String metricKey) {
  DateTime tsOf(Map<String, dynamic> f, DateTime fallback) {
    final unix = f['timestamp_unix'] as int?;
    if (unix == null || unix == 0) return fallback;
    return DateTime.fromMillisecondsSinceEpoch(unix * 1000);
  }

  switch (metricKey) {
    case 'hr':
      return _MetricSpec(
        recordType: recTypeVitals,
        unit: 'bpm',
        color: NiSenseColors.paramHr,
        extract: (id, at, p) {
          final f = decodeVitalsRecordFields(p);
          if (f.isEmpty || (f['hr_bpm'] as int) == 0) return null;
          return _HistoryPoint(tsOf(f, at), (f['hr_bpm'] as int).toDouble());
        },
      );
    case 'spo2':
      return _MetricSpec(
        recordType: recTypeVitals,
        unit: '%',
        color: NiSenseColors.paramSpo2,
        extract: (id, at, p) {
          final f = decodeVitalsRecordFields(p);
          if (f.isEmpty || (f['spo2_percent'] as int) == 0) return null;
          return _HistoryPoint(tsOf(f, at), (f['spo2_percent'] as int).toDouble());
        },
      );
    case 'hb':
      return _MetricSpec(
        recordType: recTypeVitals,
        unit: 'g/dL',
        color: NiSenseColors.paramHb,
        extract: (id, at, p) {
          final f = decodeVitalsRecordFields(p);
          if (f.isEmpty || (f['hb_g_dl'] as double) <= 0) return null;
          return _HistoryPoint(tsOf(f, at), f['hb_g_dl'] as double);
        },
      );
    case 'resp':
      return _MetricSpec(
        recordType: recTypeVitals,
        unit: 'bpm',
        color: NiSenseColors.paramResp,
        extract: (id, at, p) {
          final f = decodeVitalsRecordFields(p);
          if (f.isEmpty || (f['resp_rate_bpm'] as int) == 0) return null;
          return _HistoryPoint(tsOf(f, at), (f['resp_rate_bpm'] as int).toDouble());
        },
      );
    case 'hrv':
      return _MetricSpec(
        recordType: recTypeVitals,
        unit: 'ms',
        color: NiSenseColors.secondary,
        extract: (id, at, p) {
          final f = decodeVitalsRecordFields(p);
          if (f.isEmpty || (f['sdnn_ms'] as int) == 0) return null;
          return _HistoryPoint(tsOf(f, at), (f['sdnn_ms'] as int).toDouble());
        },
      );
    case 'bp':
      return _MetricSpec(
        recordType: recTypeVitals,
        unit: 'mmHg',
        color: NiSenseColors.accentRed,
        color2: NiSenseColors.secondary,
        label2: 'Diastolic',
        extract: (id, at, p) {
          final f = decodeVitalsRecordFields(p);
          if (f.isEmpty || (f['systolic_mmhg'] as int) == 0) return null;
          return _HistoryPoint(
            tsOf(f, at),
            (f['systolic_mmhg'] as int).toDouble(),
            value2: (f['diastolic_mmhg'] as int).toDouble(),
          );
        },
      );
    case 'glucose':
      return _MetricSpec(
        recordType: recTypeGlucose,
        unit: 'mg/dL',
        color: NiSenseColors.paramGlucose,
        extract: (id, at, p) {
          final f = decodeGlucoseRecordFields(p);
          if (f.isEmpty || (f['glucose_mg_dl'] as int) == 0) return null;
          return _HistoryPoint(tsOf(f, at), (f['glucose_mg_dl'] as int).toDouble());
        },
      );
    case 'insulin':
      return _MetricSpec(
        recordType: recTypeGlucose,
        unit: 'µIU/mL',
        color: NiSenseColors.paramInsulin,
        extract: (id, at, p) {
          final f = decodeGlucoseRecordFields(p);
          if (f.isEmpty) return null;
          return _HistoryPoint(tsOf(f, at), f['actual_insulin'] as double);
        },
      );
    case 'homa':
      return _MetricSpec(
        recordType: recTypeGlucose,
        unit: '',
        color: NiSenseColors.paramHoma,
        extract: (id, at, p) {
          final f = decodeGlucoseRecordFields(p);
          if (f.isEmpty) return null;
          return _HistoryPoint(tsOf(f, at), f['homa_ir_index'] as double);
        },
      );
    case 'temp':
      return _MetricSpec(
        recordType: recTypeTemp,
        unit: '°C',
        color: NiSenseColors.paramTemp,
        extract: (id, at, p) {
          final f = decodeTempRecordFields(p);
          if (f.isEmpty) return null;
          return _HistoryPoint(tsOf(f, at), f['skin_temp_c'] as double);
        },
      );
    default:
      return null;
  }
}

/// Full history for a metric card tapped on the Dashboard, backed by every
/// record ever synced and persisted locally (see record_local_store.dart).
class MetricHistoryPage extends StatefulWidget {
  const MetricHistoryPage({super.key, required this.metricKey, required this.metricLabel});

  final String metricKey;
  final String metricLabel;

  @override
  State<MetricHistoryPage> createState() => _MetricHistoryPageState();
}

class _MetricHistoryPageState extends State<MetricHistoryPage> {
  bool _loading = true;
  List<_HistoryPoint> _points = [];

  @override
  void initState() {
    super.initState();
    _load();
  }

  Future<void> _load() async {
    setState(() => _loading = true);
    final spec = _specFor(widget.metricKey);
    if (spec == null) {
      setState(() => _loading = false);
      return;
    }
    final rows = await RecordLocalStore.instance.getAll(type: spec.recordType);
    final pts = <_HistoryPoint>[];
    for (final r in rows) {
      final pt = spec.extract(r.recordId, r.receivedAt, r.payload);
      if (pt != null) pts.add(pt);
    }
    pts.sort((a, b) => a.time.compareTo(b.time));
    if (!mounted) return;
    setState(() {
      _points = pts;
      _loading = false;
    });
  }

  @override
  Widget build(BuildContext context) {
    final spec = _specFor(widget.metricKey);
    return Scaffold(
      appBar: AppBar(
        title: Text('${widget.metricLabel} History'),
        backgroundColor: NiSenseColors.bgHeader,
        actions: [
          IconButton(
            tooltip: 'Refresh',
            icon: const Icon(Icons.refresh),
            onPressed: _loading ? null : _load,
          ),
        ],
      ),
      body: _loading
          ? const Center(child: CircularProgressIndicator())
          : (spec == null || _points.isEmpty)
              ? _EmptyState(metricLabel: widget.metricLabel)
              : RefreshIndicator(
                  onRefresh: _load,
                  child: ListView(
                    padding: const EdgeInsets.all(12),
                    children: [
                      _HistoryChart(points: _points, spec: spec),
                      const SizedBox(height: 12),
                      Text('${_points.length} readings',
                          style: Theme.of(context).textTheme.titleSmall),
                      const SizedBox(height: 4),
                      for (final pt in _points.reversed.take(200))
                        ListTile(
                          dense: true,
                          leading: const Icon(Icons.circle, size: 8),
                          title: Text(
                            spec.label2 == null
                                ? '${pt.value.toStringAsFixed(pt.value == pt.value.roundToDouble() ? 0 : 1)} ${spec.unit}'
                                : '${pt.value.toStringAsFixed(0)}/${pt.value2?.toStringAsFixed(0)} ${spec.unit}',
                          ),
                          subtitle: Text(_fmtTime(pt.time)),
                        ),
                    ],
                  ),
                ),
    );
  }

  String _fmtTime(DateTime t) {
    final l = t.toLocal();
    return '${l.day.toString().padLeft(2, '0')}/${l.month.toString().padLeft(2, '0')}/${l.year} '
        '${l.hour.toString().padLeft(2, '0')}:${l.minute.toString().padLeft(2, '0')}';
  }
}

class _HistoryChart extends StatelessWidget {
  const _HistoryChart({required this.points, required this.spec});

  final List<_HistoryPoint> points;
  final _MetricSpec spec;

  @override
  Widget build(BuildContext context) {
    final spots = <FlSpot>[];
    final spots2 = <FlSpot>[];
    for (var i = 0; i < points.length; i++) {
      spots.add(FlSpot(i.toDouble(), points[i].value));
      if (points[i].value2 != null) spots2.add(FlSpot(i.toDouble(), points[i].value2!));
    }
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: SizedBox(
          height: 220,
          child: spots.length < 2
              ? const Center(child: Text('Not enough readings to chart yet'))
              : LineChart(
                  LineChartData(
                    gridData: const FlGridData(show: true, drawVerticalLine: false),
                    titlesData: const FlTitlesData(
                      topTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                      rightTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                      bottomTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
                    ),
                    borderData: FlBorderData(show: false),
                    lineBarsData: [
                      LineChartBarData(
                        spots: spots,
                        color: spec.color,
                        barWidth: 2,
                        dotData: const FlDotData(show: false),
                        belowBarData: BarAreaData(show: true, color: spec.color.withValues(alpha: 0.1)),
                      ),
                      if (spots2.isNotEmpty)
                        LineChartBarData(
                          spots: spots2,
                          color: spec.color2 ?? NiSenseColors.secondary,
                          barWidth: 2,
                          dotData: const FlDotData(show: false),
                        ),
                    ],
                  ),
                ),
        ),
      ),
    );
  }
}

class _EmptyState extends StatelessWidget {
  const _EmptyState({required this.metricLabel});
  final String metricLabel;

  @override
  Widget build(BuildContext context) {
    return Center(
      child: Padding(
        padding: const EdgeInsets.all(32),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.bar_chart_outlined, size: 64, color: NiSenseColors.textMuted),
            const SizedBox(height: 16),
            Text('No data yet', style: Theme.of(context).textTheme.titleMedium),
            const SizedBox(height: 8),
            Text(
              'Tap Sync to pull records from the device, then come back here to see your $metricLabel history.',
              textAlign: TextAlign.center,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(color: NiSenseColors.textMuted),
            ),
          ],
        ),
      ),
    );
  }
}
