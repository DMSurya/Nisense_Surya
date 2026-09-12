import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../../protocol/gatt_registry.dart';
import '../../protocol/hcm_protocol.dart';

/// Bottom sheet showing raw GATT bytes (nRF Connect style) with copy.
void showGattRawSheet(
  BuildContext context, {
  required String uuid,
  required List<int> data,
  required String source,
}) {
  final hex = formatGattHex(data);
  final decoded = describeGattPayload(uuid, data);
  final name = resolveGattName(uuid, fallback: uuid);

  showModalBottomSheet<void>(
    context: context,
    showDragHandle: true,
    isScrollControlled: true,
    builder: (ctx) {
      return Padding(
        padding: EdgeInsets.fromLTRB(16, 0, 16, 16 + MediaQuery.paddingOf(ctx).bottom),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(name, style: Theme.of(ctx).textTheme.titleMedium),
            Text('$source · ${data.length} bytes', style: Theme.of(ctx).textTheme.bodySmall),
            Text(uuid, style: Theme.of(ctx).textTheme.bodySmall),
            const SizedBox(height: 12),
            SelectableText(
              hex,
              style: Theme.of(ctx).textTheme.bodyLarge?.copyWith(
                    fontFamily: 'monospace',
                    letterSpacing: 0.5,
                  ),
            ),
            if (decoded.isNotEmpty) ...[
              const SizedBox(height: 12),
              Text(decoded, style: Theme.of(ctx).textTheme.bodyMedium),
            ],
            const SizedBox(height: 16),
            FilledButton.icon(
              onPressed: () {
                Clipboard.setData(ClipboardData(text: hex));
                ScaffoldMessenger.of(ctx).showSnackBar(
                  const SnackBar(content: Text('Hex copied')),
                );
              },
              icon: const Icon(Icons.copy),
              label: const Text('Copy hex'),
            ),
          ],
        ),
      );
    },
  );
}
