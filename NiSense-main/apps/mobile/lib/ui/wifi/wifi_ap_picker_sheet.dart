import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';

import '../../services/wifi_ap_scanner.dart';
import '../../services/wifi_network_vault.dart';
import '../../theme/nisense_colors.dart';

/// Result of the phone AP scan → select → password flow.
class WifiApPickResult {
  const WifiApPickResult({
    required this.ssid,
    required this.password,
    required this.security,
    required this.isSecure,
  });

  final String ssid;
  final String password;
  final String security;
  final bool isSecure;
}

/// Bottom sheet: scan nearby APs → user picks SSID → enters password.
Future<WifiApPickResult?> showWifiApPickerSheet(
  BuildContext context, {
  WifiNetworkVault? vault,
  String title = 'Choose Wi-Fi network',
}) {
  return showModalBottomSheet<WifiApPickResult>(
    context: context,
    isScrollControlled: true,
    useSafeArea: true,
    showDragHandle: true,
    builder: (ctx) => _WifiApPickerSheet(
      vault: vault ?? WifiNetworkVault(),
      title: title,
    ),
  );
}

class _WifiApPickerSheet extends StatefulWidget {
  const _WifiApPickerSheet({required this.vault, required this.title});

  final WifiNetworkVault vault;
  final String title;

  @override
  State<_WifiApPickerSheet> createState() => _WifiApPickerSheetState();
}

class _WifiApPickerSheetState extends State<_WifiApPickerSheet> {
  final _scanner = WifiApScanner();
  final _manualSsidCtrl = TextEditingController();
  List<WifiApInfo> _aps = const [];
  String? _error;
  bool _scanning = false;
  bool _manualMode = false;

  @override
  void initState() {
    super.initState();
    if (_scanner.scanSupported) {
      _rescan();
    } else {
      _manualMode = true;
      _error =
          'This phone OS cannot list nearby Wi-Fi networks. Enter the SSID '
          'manually, then continue.';
    }
  }

  @override
  void dispose() {
    _manualSsidCtrl.dispose();
    super.dispose();
  }

  Future<void> _rescan() async {
    setState(() {
      _scanning = true;
      _error = null;
    });
    try {
      final list = await _scanner.scan();
      if (!mounted) return;
      setState(() {
        _aps = list;
        _scanning = false;
        if (list.isEmpty) {
          _error = 'No networks found. Move closer to the AP and retry.';
        }
      });
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _scanning = false;
        _error = e.toString();
        _manualMode = true;
      });
    }
  }

  Future<void> _onSelect(WifiApInfo ap) async {
    final vaulted = await widget.vault.loadPsk(ap.ssid);
    if (!mounted) return;

    if (!ap.isSecure) {
      final ok = await showDialog<bool>(
        context: context,
        builder: (ctx) => AlertDialog(
          title: Text(ap.ssid),
          content: const Text(
            'This network appears open (no password). Provision the device '
            'with an empty password?',
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(ctx).pop(false),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.of(ctx).pop(true),
              child: const Text('Use open network'),
            ),
          ],
        ),
      );
      if (ok == true && mounted) {
        Navigator.of(context).pop(
          WifiApPickResult(
            ssid: ap.ssid,
            password: '',
            security: ap.security,
            isSecure: false,
          ),
        );
      }
      return;
    }

    final passCtrl = TextEditingController(text: vaulted ?? '');
    try {
      final password = await showDialog<String>(
        context: context,
        builder: (ctx) => AlertDialog(
          title: Text(ap.ssid),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                'Security: ${ap.security}',
                style: Theme.of(ctx).textTheme.bodySmall,
              ),
              const SizedBox(height: 8),
              Text(
                vaulted != null
                    ? 'Password found in vault — confirm or edit.'
                    : 'Enter the Wi-Fi password. It is written to the device '
                          'over BLE and saved in the vault.',
              ),
              const SizedBox(height: 12),
              TextField(
                controller: passCtrl,
                obscureText: true,
                autofocus: vaulted == null,
                decoration: const InputDecoration(
                  labelText: 'Password',
                  border: OutlineInputBorder(),
                ),
                onSubmitted: (v) => Navigator.of(ctx).pop(v),
              ),
            ],
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(ctx).pop(),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.of(ctx).pop(passCtrl.text),
              child: const Text('Provision'),
            ),
          ],
        ),
      );
      if (password == null || !mounted) return;
      if (password.isEmpty) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Password required for this network')),
        );
        return;
      }
      await widget.vault.saveNetwork(ap.ssid, password);
      if (!mounted) return;
      Navigator.of(context).pop(
        WifiApPickResult(
          ssid: ap.ssid,
          password: password,
          security: ap.security,
          isSecure: true,
        ),
      );
    } finally {
      passCtrl.dispose();
    }
  }

  Future<void> _submitManual() async {
    final ssid = _manualSsidCtrl.text.trim();
    if (ssid.isEmpty) return;
    final vaulted = await widget.vault.loadPsk(ssid);
    if (!mounted) return;
    final passCtrl = TextEditingController(text: vaulted ?? '');
    try {
      final password = await showDialog<String>(
        context: context,
        builder: (ctx) => AlertDialog(
          title: Text(ssid),
          content: TextField(
            controller: passCtrl,
            obscureText: true,
            autofocus: true,
            decoration: const InputDecoration(
              labelText: 'Password',
              border: OutlineInputBorder(),
            ),
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(ctx).pop(),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.of(ctx).pop(passCtrl.text),
              child: const Text('Provision'),
            ),
          ],
        ),
      );
      if (password == null || !mounted) return;
      if (password.isNotEmpty) {
        await widget.vault.saveNetwork(ssid, password);
      }
      if (!mounted) return;
      Navigator.of(context).pop(
        WifiApPickResult(
          ssid: ssid,
          password: password,
          security: 'Manual',
          isSecure: password.isNotEmpty,
        ),
      );
    } finally {
      passCtrl.dispose();
    }
  }

  IconData _signalIcon(int dbm) {
    if (dbm >= -55) return Icons.signal_wifi_4_bar;
    if (dbm >= -70) return Icons.network_wifi_3_bar;
    if (dbm >= -80) return Icons.network_wifi_2_bar;
    return Icons.network_wifi_1_bar;
  }

  @override
  Widget build(BuildContext context) {
    final height = MediaQuery.sizeOf(context).height * 0.75;
    return SizedBox(
      height: height,
      child: Padding(
        padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Text(widget.title, style: Theme.of(context).textTheme.titleLarge),
            const SizedBox(height: 4),
            Text(
              'Scan nearby access points, pick one, enter the password, then '
              'NiSense writes it to the device over BLE.',
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: NiSenseColors.textMuted,
              ),
            ),
            const SizedBox(height: 12),
            Row(
              children: [
                FilledButton.icon(
                  onPressed: (_scanning || !_scanner.scanSupported)
                      ? null
                      : _rescan,
                  icon: _scanning
                      ? const SizedBox(
                          width: 16,
                          height: 16,
                          child: CircularProgressIndicator(strokeWidth: 2),
                        )
                      : const Icon(Icons.wifi_find, size: 18),
                  label: Text(_scanning ? 'Scanning…' : 'Scan'),
                ),
                const SizedBox(width: 8),
                TextButton(
                  onPressed: () => setState(() => _manualMode = !_manualMode),
                  child: Text(_manualMode ? 'Show scan list' : 'Enter manually'),
                ),
                const Spacer(),
                if (_error != null &&
                    (_error!.contains('permission') ||
                        _error!.contains('Location')))
                  const TextButton(
                    onPressed: openAppSettings,
                    child: Text('Settings'),
                  ),
              ],
            ),
            if (_error != null) ...[
              const SizedBox(height: 8),
              Text(
                _error!,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: NiSenseColors.accentRed,
                ),
              ),
            ],
            const SizedBox(height: 8),
            Expanded(
              child: _manualMode
                  ? Column(
                      children: [
                        TextField(
                          controller: _manualSsidCtrl,
                          decoration: const InputDecoration(
                            labelText: 'SSID',
                            border: OutlineInputBorder(),
                          ),
                          textInputAction: TextInputAction.done,
                          onSubmitted: (_) => _submitManual(),
                        ),
                        const SizedBox(height: 12),
                        FilledButton(
                          onPressed: _submitManual,
                          child: const Text('Continue'),
                        ),
                      ],
                    )
                  : _aps.isEmpty && !_scanning
                  ? const Center(child: Text('No networks yet — tap Scan.'))
                  : ListView.separated(
                      itemCount: _aps.length,
                      separatorBuilder: (_, _) => const Divider(height: 1),
                      itemBuilder: (ctx, i) {
                        final ap = _aps[i];
                        return ListTile(
                          leading: Icon(_signalIcon(ap.levelDbm)),
                          title: Text(ap.ssid),
                          subtitle: Text(
                            '${ap.security} · ${ap.levelDbm} dBm'
                            '${ap.frequencyMhz > 0 ? ' · ${ap.frequencyMhz} MHz' : ''}',
                          ),
                          trailing: Icon(
                            ap.isSecure ? Icons.lock_outline : Icons.lock_open,
                            size: 18,
                          ),
                          onTap: () => _onSelect(ap),
                        );
                      },
                    ),
            ),
          ],
        ),
      ),
    );
  }
}
