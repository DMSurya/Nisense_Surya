import 'dart:async';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../state/hcm_backend.dart';
import '../../theme/nisense_colors.dart';

class DeviceInfoPage extends StatefulWidget {
  const DeviceInfoPage({super.key});

  @override
  State<DeviceInfoPage> createState() => _DeviceInfoPageState();
}

class _DeviceInfoPageState extends State<DeviceInfoPage> {
  Timer? _refreshTimer;

  @override
  void initState() {
    super.initState();
    _refresh();
    // Firmware recomputes uptime/pending on every read; keep this page live
    // while it's open instead of showing a one-time snapshot from connect.
    _refreshTimer = Timer.periodic(const Duration(seconds: 3), (_) => _refresh());
  }

  @override
  void dispose() {
    _refreshTimer?.cancel();
    super.dispose();
  }

  Future<void> _refresh() async {
    await context.read<HcmBackend>().refreshDeviceInfo();
  }

  String _formatUptime(int s) {
    final h = s ~/ 3600;
    final m = (s % 3600) ~/ 60;
    final sec = s % 60;
    return '${h}h ${m.toString().padLeft(2, '0')}m ${sec.toString().padLeft(2, '0')}s';
  }

  @override
  Widget build(BuildContext context) {
    final b = context.watch<HcmBackend>();
    final build = b.deviceBuild;
    final theme = Theme.of(context);

    return Scaffold(
      appBar: AppBar(
        title: const Text('Device Info'),
        backgroundColor: NiSenseColors.bgHeader,
        actions: [
          IconButton(
            tooltip: 'Refresh',
            icon: const Icon(Icons.refresh),
            onPressed: b.connected ? _refresh : null,
          ),
        ],
      ),
      body: RefreshIndicator(
        onRefresh: _refresh,
        child: ListView(
          padding: const EdgeInsets.all(16),
          children: [
            _InfoSection(
              title: 'Identity',
              rows: [
                _InfoRow(label: 'Hardware ID', value: b.hardwareDeviceId.isEmpty ? '—' : b.hardwareDeviceId),
                _InfoRow(
                  label: 'Security profile',
                  value: b.securityProfile == 1 ? 'SECURE' : 'OPEN',
                  valueColor: b.securityProfile == 1
                      ? NiSenseColors.statusNormal
                      : NiSenseColors.accentAmber,
                ),
              ],
            ),
            const SizedBox(height: 12),
            _InfoSection(
              title: 'Firmware',
              rows: [
                _InfoRow(label: 'Version', value: build?.fwVersion ?? '—'),
                _InfoRow(label: 'Git hash', value: build != null ? '#${build.gitHash}' : '—'),
                _InfoRow(
                  label: 'Uptime',
                  value: build != null ? _formatUptime(build.uptimeS) : '—',
                ),
              ],
            ),
            const SizedBox(height: 12),
            _InfoSection(
              title: 'Records',
              rows: [
                _InfoRow(
                  label: 'Pending on device',
                  value: build != null ? '${build.pendingRecords}' : '—',
                  valueColor: (build?.pendingRecords ?? 0) > 0
                      ? NiSenseColors.accentAmber
                      : NiSenseColors.statusNormal,
                ),
                if (build != null) ...[
                  _InfoRow(label: '  Glucose', value: '${build.pendingGlucose}'),
                  _InfoRow(label: '  Vitals', value: '${build.pendingVitals}'),
                  _InfoRow(label: '  Temp', value: '${build.pendingTemp}'),
                  _InfoRow(label: '  PPG Raw', value: '${build.pendingPpgRaw}'),
                  _InfoRow(label: '  Glucose Raw', value: '${build.pendingGlucoseRaw}'),
                ],
              ],
            ),
            const SizedBox(height: 12),
            if (!b.connected)
              Padding(
                padding: const EdgeInsets.all(8),
                child: Text(
                  'Connect to device to refresh info.',
                  style: theme.textTheme.bodySmall?.copyWith(
                    color: NiSenseColors.textMuted,
                  ),
                  textAlign: TextAlign.center,
                ),
              ),
          ],
        ),
      ),
    );
  }
}

class _InfoSection extends StatelessWidget {
  const _InfoSection({required this.title, required this.rows});

  final String title;
  final List<_InfoRow> rows;

  @override
  Widget build(BuildContext context) {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(12),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(title, style: Theme.of(context).textTheme.titleSmall),
            const Divider(height: 12),
            ...rows,
          ],
        ),
      ),
    );
  }
}

class _InfoRow extends StatelessWidget {
  const _InfoRow({required this.label, required this.value, this.valueColor});

  final String label;
  final String value;
  final Color? valueColor;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        children: [
          SizedBox(
            width: 140,
            child: Text(
              label,
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                    color: NiSenseColors.textMuted,
                  ),
            ),
          ),
          Expanded(
            child: Text(
              value,
              style: Theme.of(context).textTheme.bodyMedium?.copyWith(
                    color: valueColor,
                    fontFamily: 'monospace',
                  ),
            ),
          ),
        ],
      ),
    );
  }
}
