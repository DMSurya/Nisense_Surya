import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../state/hcm_backend.dart';

class LogsPage extends StatelessWidget {
  const LogsPage({super.key});

  @override
  Widget build(BuildContext context) {
    final b = context.watch<HcmBackend>();
    final paths = b.logPaths;
    return ListView(
      padding: const EdgeInsets.all(12),
      children: [
        Text('Session CSV logs', style: Theme.of(context).textTheme.titleMedium),
        const SizedBox(height: 8),
        if (paths.isEmpty) const Text('No sessions yet — connect and receive vitals.'),
        ...paths.map((p) => ListTile(
              title: Text(p.split('/').last.split('\\').last),
              subtitle: Text(p),
              leading: const Icon(Icons.description),
            )),
      ],
    );
  }
}
