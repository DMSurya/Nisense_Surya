import 'package:file_selector/file_selector.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../state/hcm_backend.dart';
import '../widgets/common_widgets.dart';

class FirmwarePage extends StatelessWidget {
  const FirmwarePage({super.key});

  static const _firmwareTypes = XTypeGroup(
    label: 'Firmware images',
    extensions: ['bin', 'hex'],
  );

  @override
  Widget build(BuildContext context) {
    final b = context.watch<HcmBackend>();
    return ListView(
      padding: const EdgeInsets.all(12),
      children: [
        GlassCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('Connection', style: Theme.of(context).textTheme.titleMedium),
              Text('MTU: ${b.connMtu > 0 ? b.connMtu : '—'}'),
              Text('Security: ${b.securityProfile == 1 ? 'SECURE' : 'OPEN'}'),
              Text('Paired: ${b.isPaired}'),
              Row(
                children: [
                  FilledButton(onPressed: b.connected ? () => b.pairDevice() : null, child: const Text('Pair')),
                  const SizedBox(width: 8),
                  OutlinedButton(onPressed: b.connected ? () => b.forgetPairing() : null, child: const Text('Forget bond')),
                ],
              ),
            ],
          ),
        ),
        const SizedBox(height: 12),
        GlassCard(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text('DFU (SMP)', style: Theme.of(context).textTheme.titleMedium),
              if (b.dfuActive) LinearProgressIndicator(value: b.dfuProgress),
              if (b.dfuActive) ...[
                const SizedBox(height: 8),
                Text(
                  'Stay on Firmware until the update finishes.',
                  style: Theme.of(context).textTheme.bodySmall,
                ),
              ],
              const SizedBox(height: 8),
              FilledButton(
                onPressed: b.connected && !b.dfuActive && !b.navigationLocked
                    ? () async {
                        final file = await openFile(acceptedTypeGroups: const [_firmwareTypes]);
                        final path = file?.path;
                        if (path != null && context.mounted) {
                          await b.startDfu(path);
                        }
                      }
                    : null,
                child: const Text('Select image & flash'),
              ),
            ],
          ),
        ),
      ],
    );
  }
}
