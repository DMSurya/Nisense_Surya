import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../ble/nisense_scan.dart';
import '../../state/hcm_backend.dart';
import '../../theme/nisense_colors.dart';
import '../widgets/common_widgets.dart';

class ScanPage extends StatelessWidget {
  const ScanPage({super.key});

  @override
  Widget build(BuildContext context) {
    final backend = context.watch<HcmBackend>();
    return Padding(
      padding: const EdgeInsets.all(12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Row(
            children: [
              Expanded(child: Text(backend.status)),
              ConnectionPill(connected: backend.connected),
            ],
          ),
          const SizedBox(height: 8),
          FilledButton.icon(
            onPressed: backend.scanning ? null : () => backend.scan(),
            icon: const Icon(Icons.bluetooth_searching),
            label: Text(backend.scanning ? 'Scanning…' : 'Scan'),
          ),
          if (backend.knownDevices.isNotEmpty) ...[
            const SizedBox(height: 12),
            Text('Known devices', style: Theme.of(context).textTheme.titleMedium),
            ...backend.knownDevices.map((d) => ListTile(
                  title: Text(d.name.isNotEmpty ? d.name : d.address),
                  subtitle: Text(d.address),
                  trailing: Row(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      IconButton(
                        icon: const Icon(Icons.link),
                        onPressed: backend.connecting
                            ? null
                            : () => backend.connectToAddress(d.address),
                      ),
                      IconButton(
                        icon: const Icon(Icons.delete_outline),
                        onPressed: () => backend.forgetDevice(d.address),
                      ),
                    ],
                  ),
                )),
          ],
          const SizedBox(height: 8),
          Text('Results (${backend.scanResults.length})',
              style: Theme.of(context).textTheme.titleMedium),
          Expanded(
            child: ListView.builder(
              itemCount: backend.scanResults.length,
              itemBuilder: (context, i) {
                final r = backend.scanResults[i];
                final label = resolveNiSenseProductLabel(r);
                final raw = rawBleAdvertisedName(r);
                return ListTile(
                  title: Text(label),
                  subtitle: Text(
                    raw.isNotEmpty && raw != label
                        ? '$raw · ${r.device.remoteId.str}'
                        : r.device.remoteId.str,
                  ),
                  trailing: Text('${r.rssi} dBm'),
                  onTap: backend.connecting
                      ? null
                      : () => backend.connectToAddress(r.device.remoteId.str),
                );
              },
            ),
          ),
          if (backend.connected)
            OutlinedButton(
              onPressed: () => backend.disconnect(),
              child: const Text('Disconnect'),
            ),
        ],
      ),
    );
  }
}
