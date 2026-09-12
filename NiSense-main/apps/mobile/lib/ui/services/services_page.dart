import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../../protocol/gatt_registry.dart';
import '../../state/hcm_backend.dart';
import '../widgets/gatt_raw_sheet.dart';

class ServicesPage extends StatefulWidget {
  const ServicesPage({super.key});

  @override
  State<ServicesPage> createState() => _ServicesPageState();
}

class _ServicesPageState extends State<ServicesPage> {
  final _expanded = <String, bool>{};

  bool _canRead(List<String> properties) {
    return properties.any((p) => p.toLowerCase().contains('read'));
  }

  bool _canNotify(List<String> properties) {
    return properties.any((p) => p.toLowerCase().contains('notify'));
  }

  @override
  Widget build(BuildContext context) {
    final b = context.watch<HcmBackend>();
    return Column(
      children: [
        Padding(
          padding: const EdgeInsets.all(12),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              FilledButton(
                onPressed: b.connected ? () => b.refreshGatt() : null,
                child: const Text('Discover GATT'),
              ),
              const SizedBox(height: 8),
              Text(
                'Tap Read for readable chars, or the notify icon for last notify hex (like nRF Connect).',
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ],
          ),
        ),
        Expanded(
          child: ListView.builder(
            itemCount: b.gattTree.length,
            itemBuilder: (context, si) {
              final svc = b.gattTree[si];
              final open = _expanded[svc.uuid] ?? false;
              return ExpansionTile(
                title: Text(svc.name),
                subtitle: Text(svc.uuid),
                initiallyExpanded: open,
                onExpansionChanged: (v) => setState(() => _expanded[svc.uuid] = v),
                children: svc.characteristics.map((ch) {
                  final sec = securityLevelForUuid(ch.uuid, b.securityProfile);
                  final readable = _canRead(ch.properties);
                  final notifiable = _canNotify(ch.properties);
                  final hasNotify = b.lastNotifyFor(ch.uuid) != null;
                  return ListTile(
                    title: Text(ch.name),
                    subtitle: Text('${ch.properties.join(', ')} · $sec'),
                    trailing: Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        if (notifiable)
                          IconButton(
                            tooltip: 'Last notify (hex)',
                            icon: Icon(
                              Icons.notifications_active_outlined,
                              color: hasNotify ? null : Theme.of(context).disabledColor,
                            ),
                            onPressed: b.connected && hasNotify
                                ? () {
                                    showGattRawSheet(
                                      context,
                                      uuid: ch.uuid,
                                      data: b.lastNotifyFor(ch.uuid)!,
                                      source: 'Last notify',
                                    );
                                  }
                                : null,
                          ),
                        if (readable)
                          IconButton(
                            tooltip: 'Read (hex)',
                            icon: const Icon(Icons.download),
                            onPressed: b.connected
                                ? () async {
                                    final data = await b.readCharacteristic(ch.uuid);
                                    if (!context.mounted || data == null) return;
                                    showGattRawSheet(
                                      context,
                                      uuid: ch.uuid,
                                      data: data,
                                      source: 'GATT read',
                                    );
                                  }
                                : null,
                          ),
                      ],
                    ),
                    onTap: readable && b.connected
                        ? () async {
                            final data = await b.readCharacteristic(ch.uuid);
                            if (!context.mounted || data == null) return;
                            showGattRawSheet(
                              context,
                              uuid: ch.uuid,
                              data: data,
                              source: 'GATT read',
                            );
                          }
                        : notifiable && b.connected && hasNotify
                            ? () {
                                showGattRawSheet(
                                  context,
                                  uuid: ch.uuid,
                                  data: b.lastNotifyFor(ch.uuid)!,
                                  source: 'Last notify',
                                );
                              }
                            : null,
                  );
                }).toList(),
              );
            },
          ),
        ),
      ],
    );
  }
}
