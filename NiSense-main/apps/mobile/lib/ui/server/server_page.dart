import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:provider/provider.dart';

import '../../auth/auth_service.dart';
import '../../ble/record_sync_client.dart';
import '../../net/api_client.dart';
import '../../net/server_config.dart';
import '../../protocol/hcm_protocol.dart';
import '../../services/ota_service.dart';
import '../../services/record_csv_exporter.dart';
import '../../services/sync_service.dart';
import '../../state/hcm_backend.dart';

/// Server connectivity screen: OIDC login, sync, and role-gated OTA (firmware +
/// glucose model). Instantiates its own API stack from the persisted
/// [ServerConfig] so it does not disturb the BLE-only app state.
class ServerPage extends StatefulWidget {
  const ServerPage({super.key});

  @override
  State<ServerPage> createState() => _ServerPageState();
}

class _ServerPageState extends State<ServerPage> {
  ServerConfig? _config;
  AuthService? _auth;
  SyncService? _sync;
  OtaService? _ota;

  String _variant = 'wearable';
  String _message = '';
  bool _pullBusy = false;
  String _pullStatus = '';
  final _urlCtrl = TextEditingController();
  final _tokenCtrl = TextEditingController();

  Future<void> _pullDeviceRecords(HcmBackend backend, {required bool full}) async {
    if (!backend.connected || backend.connectedAddress.isEmpty) {
      throw Exception('Connect to a device first');
    }
    if (_pullBusy) return;
    setState(() {
      _pullBusy = true;
      _pullStatus = 'Pulling ${full ? 'full' : 'summary'} records…';
      _message = '';
    });
    try {
      await backend.runWithExclusiveNotifies('Record sync in progress', () async {
        final client =
            RecordSyncClient(BluetoothDevice.fromId(backend.connectedAddress));
        try {
          final records = await client.pull(
            afterId: 0,
            mode: full ? recModeFull : recModeSummaryOnly,
            onStatus: (st) {
              if (!mounted) return;
              setState(() {
                _pullStatus =
                    'state=${st.state} sent=${st.sentCount} cursor=${st.cursorId} pending=${st.pending}';
              });
            },
          );
          final exporter = RecordCsvExporter(
            patientName:
                backend.patientName.isNotEmpty ? backend.patientName : 'Patient',
          );
          final paths = await exporter.exportAll(records);
          // Copy-only: do not ACK/wipe. Use Sync tab "Sync and wipe" or
          // "Clear device" to free NOR on the wearable.
          if (!mounted) return;
          setState(() {
            _pullStatus =
                'Saved ${paths.length} CSV file(s) from ${records.length} records '
                '(device not wiped — use Sync tab to clear device)';
            _message =
                paths.isEmpty ? 'No records pending on device' : paths.join('\n');
          });
        } finally {
          await client.dispose();
        }
      });
    } finally {
      if (mounted) setState(() => _pullBusy = false);
    }
  }

  @override
  void initState() {
    super.initState();
    _bootstrap();
  }

  Future<void> _bootstrap() async {
    final cfg = await ServerConfig.load();
    final auth = AuthService(cfg);
    final api = ApiClient(cfg, auth);
    setState(() {
      _config = cfg;
      _auth = auth;
      _sync = SyncService(api);
      _ota = OtaService(api);
      _urlCtrl.text = cfg.baseUrl;
      _tokenCtrl.text = cfg.devToken;
    });
  }

  Future<void> _saveConfig() async {
    final cfg = _config;
    if (cfg == null) return;
    cfg.baseUrl = _urlCtrl.text.trim();
    cfg.devToken = _tokenCtrl.text.trim();
    await cfg.save();
    setState(() => _message = 'Saved server settings');
  }

  Future<void> _guard(Future<void> Function() action) async {
    try {
      await action();
    } catch (e) {
      setState(() => _message = 'Error: $e');
    }
  }

  @override
  void dispose() {
    _urlCtrl.dispose();
    _tokenCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final backend = context.watch<HcmBackend>();
    final auth = _auth;
    if (auth == null) {
      return const Center(child: CircularProgressIndicator());
    }

    return AnimatedBuilder(
      animation: Listenable.merge([auth, _sync!, _ota!]),
      builder: (context, _) {
        final canManageDevices = auth.hasAnyRole(['device_engineer', 'super_admin']);
        return ListView(
          padding: const EdgeInsets.all(16),
          children: [
            const Text('Server', style: TextStyle(fontSize: 22, fontWeight: FontWeight.bold)),
            const SizedBox(height: 12),
            TextField(
              controller: _urlCtrl,
              decoration: const InputDecoration(labelText: 'API base URL'),
            ),
            TextField(
              controller: _tokenCtrl,
              decoration: const InputDecoration(
                labelText: 'Dev bearer token (Pi dev provider, optional)',
              ),
            ),
            const SizedBox(height: 8),
            Wrap(spacing: 8, children: [
              OutlinedButton(onPressed: _saveConfig, child: const Text('Save')),
              if (!auth.isAuthenticated)
                FilledButton(
                  onPressed: () => _guard(auth.login),
                  child: const Text('Sign in (OIDC)'),
                )
              else
                OutlinedButton(
                  onPressed: () => _guard(auth.logout),
                  child: const Text('Sign out'),
                ),
            ]),
            const SizedBox(height: 12),
            if (auth.isAuthenticated) ...[
              Text('Signed in as ${auth.username ?? '(dev token)'}'),
              Wrap(
                spacing: 6,
                children: auth.roles.map((r) => Chip(label: Text(r))).toList(),
              ),
            ],
            const Divider(height: 32),

            // Sync (any authenticated user)
            const Text('Data Sync', style: TextStyle(fontWeight: FontWeight.bold)),
            const SizedBox(height: 6),
            FilledButton.icon(
              icon: const Icon(Icons.cloud_upload),
              label: Text(_sync!.busy ? _sync!.status : 'Upload CSV logs'),
              onPressed: _sync!.busy
                  ? null
                  : () => _guard(() => _sync!.syncCsvLogs(
                        deviceId: backend.hardwareDeviceId,
                        patientId: backend.patientName,
                      )),
            ),
            const SizedBox(height: 8),
            OutlinedButton.icon(
              icon: const Icon(Icons.monitor_heart_outlined),
              label: const Text('Push latest clinical snapshot'),
              onPressed: _sync!.busy
                  ? null
                  : () => _guard(() async {
                        final id = backend.hardwareDeviceId.isNotEmpty
                            ? backend.hardwareDeviceId
                            : backend.connectedAddress;
                        if (id.isEmpty) {
                          throw Exception('No device id — connect first');
                        }
                        await _sync!.pushClinicalSnapshot(
                          deviceId: id,
                          glucoseMgDl: backend.glucoseMgDl,
                          insulin: backend.insulinUiUml,
                          homaIr: backend.homaIr,
                          hrBpm: backend.hrBpm,
                          spo2Pct: backend.spo2Pct,
                          hbGdl: backend.hbGdl,
                          respBpm: backend.respBpm,
                          sdnnMs: backend.sdnnMs,
                          rmssdMs: backend.rmssdMs,
                          systolicMmhg: backend.systolicMmhg,
                          diastolicMmhg: backend.diastolicMmhg,
                        );
                      }),
            ),
            Text('Uploaded: ${_sync!.uploadedCount} • ${_sync!.status}'),
            const SizedBox(height: 16),
            const Text('Device records (BLE → CSV)',
                style: TextStyle(fontWeight: FontWeight.bold)),
            const SizedBox(height: 6),
            if (!backend.connected)
              const Text('Connect to a device to pull NOR records.')
            else
              Wrap(spacing: 8, runSpacing: 8, children: [
                FilledButton.icon(
                  icon: const Icon(Icons.download),
                  label: Text(_pullBusy ? 'Pulling…' : 'Pull device records (full)'),
                  onPressed: _pullBusy
                      ? null
                      : () => _guard(() => _pullDeviceRecords(backend, full: true)),
                ),
                OutlinedButton.icon(
                  icon: const Icon(Icons.summarize_outlined),
                  label: const Text('Pull summaries only'),
                  onPressed: _pullBusy
                      ? null
                      : () => _guard(() => _pullDeviceRecords(backend, full: false)),
                ),
              ]),
            if (_pullStatus.isNotEmpty) ...[
              const SizedBox(height: 6),
              Text(_pullStatus),
            ],
            const Divider(height: 32),

            // OTA (role-gated: device engineers / admins only)
            const Text('Over-the-air Updates', style: TextStyle(fontWeight: FontWeight.bold)),
            if (!canManageDevices)
              const Padding(
                padding: EdgeInsets.symmetric(vertical: 8),
                child: Text('Requires device_engineer or super_admin role.'),
              )
            else if (!backend.connected)
              const Padding(
                padding: EdgeInsets.symmetric(vertical: 8),
                child: Text('Connect to a device first.'),
              )
            else ...[
              const SizedBox(height: 6),
              Row(children: [
                const Text('Model variant: '),
                DropdownButton<String>(
                  value: _variant,
                  items: const [
                    DropdownMenuItem(value: 'wearable', child: Text('wearable')),
                    DropdownMenuItem(value: 'pulse', child: Text('pulse')),
                  ],
                  onChanged: (v) => setState(() => _variant = v ?? 'wearable'),
                ),
              ]),
              Wrap(spacing: 8, children: [
                FilledButton.icon(
                  icon: const Icon(Icons.inventory_2),
                  label: const Text('Update bundle'),
                  onPressed: () => _guard(() => _ota!.updateBundle(
                        BluetoothDevice.fromId(backend.connectedAddress),
                        backend.hardwareDeviceId,
                      )),
                ),
                FilledButton.icon(
                  icon: const Icon(Icons.system_update),
                  label: const Text('Update firmware'),
                  onPressed: () => _guard(() => _ota!.updateFirmware(
                        BluetoothDevice.fromId(backend.connectedAddress),
                        backend.hardwareDeviceId,
                      )),
                ),
                FilledButton.icon(
                  icon: const Icon(Icons.memory),
                  label: const Text('Update model'),
                  onPressed: () => _guard(() => _ota!.updateModel(
                        BluetoothDevice.fromId(backend.connectedAddress),
                        backend.hardwareDeviceId,
                        _variant,
                      )),
                ),
              ]),
              if (_ota!.phase.isNotEmpty)
                Text('Phase: ${_ota!.phase}', style: const TextStyle(fontSize: 12)),
              if (_ota!.progress > 0 && _ota!.progress < 1)
                Padding(
                  padding: const EdgeInsets.symmetric(vertical: 8),
                  child: LinearProgressIndicator(value: _ota!.progress),
                ),
              Text(_ota!.status),
            ],
            const SizedBox(height: 16),
            if (_message.isNotEmpty)
              Text(_message, style: const TextStyle(color: Colors.redAccent)),
          ],
        );
      },
    );
  }
}
