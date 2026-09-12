import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../../auth/auth_service.dart';
import '../../ble/record_sync_client.dart';
import '../../net/api_client.dart';
import '../../net/server_config.dart';
import '../../protocol/hcm_protocol.dart';
import '../../services/bulk_session_controller.dart';
import '../../services/record_local_store.dart';
import '../../services/record_xlsx_exporter.dart';
import '../../services/sync_service.dart';
import '../../services/wifi_network_vault.dart';
import '../../state/hcm_backend.dart';
import '../../theme/nisense_colors.dart';
import '../records/all_records_page.dart';
import '../wifi/wifi_ap_picker_sheet.dart';

const _kLastPulled = 'last_pulled_record_id';
const _kLastAcked = 'last_acked_record_id';
const _kLastSyncMode = 'last_sync_mode'; // recModeSummaryOnly or recModeFull

/// Persistent metadata entry for a completed sync session.
class _SyncSession {
  _SyncSession({
    required this.timestamp,
    required this.recordCount,
    required this.perType,
  });

  final DateTime timestamp;
  final int recordCount;
  final Map<int, int> perType;

  Map<String, dynamic> toJson() => {
    'ts': timestamp.millisecondsSinceEpoch,
    'count': recordCount,
    'xlsx': '',
    'perType': perType.map((k, v) => MapEntry(k.toString(), v)),
  };

  factory _SyncSession.fromJson(Map<String, dynamic> j) => _SyncSession(
    timestamp: DateTime.fromMillisecondsSinceEpoch(j['ts'] as int),
    recordCount: j['count'] as int,
    perType: (j['perType'] as Map<String, dynamic>).map(
      (k, v) => MapEntry(int.parse(k), v as int),
    ),
  );
}

class _WifiSyncPrompt {
  const _WifiSyncPrompt({
    required this.ssid,
    required this.password,
    this.preferHotspot = false,
  });

  final String ssid;
  final String password;
  final bool preferHotspot;
}

class SyncPage extends StatefulWidget {
  const SyncPage({super.key});

  @override
  State<SyncPage> createState() => _SyncPageState();
}

class _SyncPageState extends State<SyncPage>
    with AutomaticKeepAliveClientMixin {
  bool _syncing = false;
  bool _exporting = false;
  bool _clearingPhone = false;
  bool _clearingDevice = false;
  String? _error;

  // Progress — [_received] is unique frames assembled; [_deviceSent] is what
  // firmware claims it transmitted (can lead received while notifies drain).
  int _deviceSent = 0;
  int _devicePending = 0;
  int _received = 0;
  bool _incompleteTransfer = false;

  /// True when the most recent pull used FULL mode (includes raw).
  bool _lastSyncIncludedRaw = false;
  bool _lastSyncUsedWifi = false;
  final Map<int, int> _perType = {};
  int _errorCount = 0;
  String _wifiSyncStatus = '';

  List<_SyncSession> _sessions = [];
  int _localRecordCount = 0;
  int _lastPulledId = 0;
  int _lastAckedId = 0;

  // Best-effort cloud push, mirrors the server's existing pull-device/upload-csv
  // flow but goes record-by-record via /ingest/readings so the server can
  // export by range/device without needing a whole-file CSV round trip.
  AuthService? _auth;
  SyncService? _pushSvc;
  String _serverPushStatus = '';

  @override
  bool get wantKeepAlive => true;

  @override
  void initState() {
    super.initState();
    _loadSessions();
    _loadCursors();
    _refreshLocalCount();
    _bootstrapServer();
  }

  Future<void> _bootstrapServer() async {
    final cfg = await ServerConfig.load();
    final auth = AuthService(cfg);
    final api = ApiClient(cfg, auth);
    if (!mounted) return;
    setState(() {
      _auth = auth;
      _pushSvc = SyncService(api);
    });
  }

  /// Silently pushes newly-synced records to the server if signed in.
  /// Never blocks or fails the local sync — the ingest API is idempotent so
  /// a failed push here is simply retried in full on the next sync.
  Future<void> _pushToServer(
    List<SyncedRecord> records,
    String deviceId,
  ) async {
    final svc = _pushSvc;
    final auth = _auth;
    if (svc == null || auth == null || !auth.isAuthenticated) return;
    if (mounted) {
      setState(() => _serverPushStatus = 'Server: pushing ${records.length}…');
    }
    try {
      final r = await svc.pushSyncedRecords(deviceId, records);
      if (r.pushedIds.isNotEmpty) {
        await RecordLocalStore.instance.markPushed(deviceId, r.pushedIds);
      }
      if (!mounted) return;
      setState(() {
        _serverPushStatus =
            'Server: ${r.accepted} pushed'
            '${r.duplicates > 0 ? ', ${r.duplicates} dup' : ''}'
            '${r.failed > 0 ? ', ${r.failed} failed' : ''}';
      });
    } catch (e) {
      if (mounted) setState(() => _serverPushStatus = 'Server push error: $e');
    }
  }

  Future<void> _loadSessions() async {
    final prefs = await SharedPreferences.getInstance();
    final raw = prefs.getStringList('sync_sessions') ?? [];
    if (!mounted) return;
    setState(() {
      _sessions = raw
          .map(
            (s) => _SyncSession.fromJson(jsonDecode(s) as Map<String, dynamic>),
          )
          .toList();
    });
  }

  Future<void> _saveSessions() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setStringList(
      'sync_sessions',
      _sessions.map((s) => jsonEncode(s.toJson())).toList(),
    );
  }

  /// Seeds last_pulled from last_acked once so existing installs keep
  /// incremental Sync behavior after the cursor split.
  Future<void> _loadCursors() async {
    final prefs = await SharedPreferences.getInstance();
    var pulled = prefs.getInt(_kLastPulled);
    final acked = prefs.getInt(_kLastAcked) ?? 0;
    if (pulled == null) {
      pulled = acked;
      await prefs.setInt(_kLastPulled, pulled);
    }
    if (!mounted) return;
    setState(() {
      _lastPulledId = pulled!;
      _lastAckedId = acked;
    });
  }

  Future<void> _refreshLocalCount() async {
    final n = await RecordLocalStore.instance.totalCount();
    if (!mounted) return;
    setState(() => _localRecordCount = n);
  }

  String _typeName(int t) {
    switch (t) {
      case recTypeGlucose:
        return 'Glucose';
      case recTypeVitals:
        return 'Vitals';
      case recTypeTemp:
        return 'Temp';
      case recTypePpgRaw:
        return 'PPG Raw';
      case recTypeGlucoseRaw:
        return 'Glucose Raw';
      default:
        return 'Type $t';
    }
  }

  String _deviceKey(HcmBackend b) {
    return b.hardwareDeviceId.isNotEmpty
        ? b.hardwareDeviceId
        : (b.deviceName.isNotEmpty ? b.deviceName : b.connectedAddress);
  }

  Future<void> _startSync({
    bool resetCursor = false,
    bool includeRaw = false,
  }) async {
    final b = context.read<HcmBackend>();
    if (!b.connected) {
      setState(() => _error = 'Not connected to device');
      return;
    }
    final device = b.client.device;
    if (device == null) {
      setState(() => _error = 'Device handle unavailable');
      return;
    }

    setState(() {
      _syncing = true;
      _error = null;
      _deviceSent = 0;
      _devicePending = 0;
      _received = 0;
      _incompleteTransfer = false;
      _lastSyncIncludedRaw = includeRaw;
      _lastSyncUsedWifi = false;
      _perType.clear();
      _errorCount = 0;
      _wifiSyncStatus = '';
    });

    int? wipeMaxId;

    try {
      await b.runWithExclusiveNotifies('Record sync in progress', () async {
        final prefs = await SharedPreferences.getInstance();
        await _loadCursors();
        final afterId = resetCursor ? 0 : (prefs.getInt(_kLastPulled) ?? 0);

        final client = RecordSyncClient(device);
        try {
          final records = await client.pull(
            afterId: afterId,
            mode: includeRaw ? recModeFull : recModeSummaryOnly,
            onStatus: (st) {
              if (mounted) {
                setState(() {
                  _devicePending = st.pending;
                  _deviceSent = st.sentCount;
                  _received = client.receivedCount;
                });
              }
            },
          );

          final perType = <int, int>{};
          for (final r in records) {
            perType[r.type] = (perType[r.type] ?? 0) + 1;
          }

          final deviceSent = _deviceSent;
          final incomplete = deviceSent > 0 && records.length < deviceSent;

          // Prefer stable hardware id so re-syncs upsert instead of duplicating.
          final deviceKey = _deviceKey(b);
          if (records.isNotEmpty && deviceKey.isNotEmpty) {
            await RecordLocalStore.instance.insertAll(deviceKey, records);
            // FULL wipe path awaits cloud; SUMMARY can push in background.
            if (includeRaw) {
              await _pushToServer(records, deviceKey);
            } else {
              unawaited(_pushToServer(records, deviceKey));
            }
          }

          if (records.isNotEmpty) {
            final maxId = records
                .map((r) => r.recordId)
                .reduce((a, b) => a > b ? a : b);
            final prevPulled = prefs.getInt(_kLastPulled) ?? 0;
            final nextPulled = maxId > prevPulled ? maxId : prevPulled;
            await prefs.setInt(_kLastPulled, nextPulled);
            if (mounted) setState(() => _lastPulledId = nextPulled);
            wipeMaxId = nextPulled;
          }

          final session = _SyncSession(
            timestamp: DateTime.now(),
            recordCount: records.length,
            perType: perType,
          );
          _sessions.insert(0, session);
          await _saveSessions();

          if (mounted) {
            setState(() {
              _received = records.length;
              _incompleteTransfer = incomplete;
              _perType
                ..clear()
                ..addAll(perType);
              if (incomplete) {
                _error =
                    'Transfer incomplete: saved ${records.length} of $deviceSent '
                    'the device reported sending. Tap Sync again to pull the rest.';
              }
            });
          }
        } finally {
          await client.dispose();
        }
      });

      if (mounted) {
        setState(() => _syncing = false);
        await _refreshLocalCount();
      }

      // Post-pull reclaim: SUMMARY = watermark only; FULL = optional space wipe.
      // Skip if transfer incomplete — cursor may need another Sync.
      if (wipeMaxId != null && mounted && !_incompleteTransfer) {
        final prefs = await SharedPreferences.getInstance();
        await prefs.setInt(
          _kLastSyncMode,
          includeRaw ? recModeFull : recModeSummaryOnly,
        );
        if (includeRaw) {
          final wipe = await _confirmFullWipe(wipeMaxId!);
          if (wipe == true && mounted) {
            await _wipeDeviceUpTo(wipeMaxId!, mode: recModeFull);
          }
        } else {
          final mark = await _confirmMarkSummaries(wipeMaxId!);
          if (mark == true && mounted) {
            await _wipeDeviceUpTo(wipeMaxId!, mode: recModeSummaryOnly);
          }
        }
      }
    } catch (e) {
      if (mounted) {
        setState(() {
          _syncing = false;
          _error = e.toString();
        });
      }
    }
  }

  Future<void> _startWifiSync() async {
    final b = context.read<HcmBackend>();
    if (!b.connected) {
      setState(() => _error = 'Not connected to device');
      return;
    }
    final deviceKey = _deviceKey(b);
    if (deviceKey.isEmpty) {
      setState(() => _error = 'Device identity unavailable');
      return;
    }

    // Scan nearby APs on the phone → user picks SSID + password → BLE provision.
    final mode = await showDialog<String>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Sync over Wi-Fi'),
        content: const Text(
          'Scan nearby Wi-Fi networks, pick the AP the device should join, '
          'enter the password, then NiSense provisions the device over BLE '
          'and pulls records over Wi-Fi.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.of(ctx).pop('hotspot'),
            child: const Text('Use hotspot'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(ctx).pop('scan'),
            child: const Text('Scan networks'),
          ),
        ],
      ),
    );
    if (!mounted || mode == null) return;

    late final _WifiSyncPrompt wifi;
    if (mode == 'hotspot') {
      wifi = const _WifiSyncPrompt(
        ssid: '',
        password: '',
        preferHotspot: true,
      );
    } else {
      final pick = await showWifiApPickerSheet(
        context,
        title: 'Choose Wi-Fi for device',
      );
      if (!mounted || pick == null) return;
      wifi = _WifiSyncPrompt(ssid: pick.ssid, password: pick.password);
    }

    setState(() {
      _syncing = true;
      _error = null;
      _deviceSent = 0;
      _devicePending = 0;
      _received = 0;
      _incompleteTransfer = false;
      _lastSyncIncludedRaw = true;
      _lastSyncUsedWifi = true;
      _perType.clear();
      _errorCount = 0;
      _wifiSyncStatus = wifi.preferHotspot
          ? 'Preparing hotspot provision…'
          : 'Provisioning ${wifi.ssid} over BLE…';
    });

    try {
      final prefs = await SharedPreferences.getInstance();
      await _loadCursors();
      // FULL pull always from afterId=0 so SUMMARY-skipped raw ids are fetched.
      const afterId = 0;
      late final BulkSessionResult result;
      await b.runWithExclusiveNotifies('Wi-Fi sync in progress', () async {
        final controller = BulkSessionController(backend: b);
        result = await controller.run(
          deviceId: deviceKey,
          afterId: afterId,
          mode: recModeFull,
          forceHotspot: wifi.preferHotspot,
          manualSsid: wifi.preferHotspot ? null : wifi.ssid,
          password: wifi.preferHotspot ? null : wifi.password,
          instructions: (message) {
            if (!mounted) return;
            setState(() => _wifiSyncStatus = message);
          },
          confirmHotspotReady: _confirmHotspotReady,
          askHotspotCredentials: _askHotspotCredentials,
          onProgress: (p) {
            if (!mounted) return;
            setState(() {
              _wifiSyncStatus = p.message.isNotEmpty
                  ? p.message
                  : _phaseLabel(p.phase);
              _received = p.receivedRecords;
              _deviceSent = p.deviceSent;
              _devicePending = p.devicePending;
            });
          },
        );
      });

      await _loadCursors();
      await _refreshLocalCount();

      // Persist + await cloud, then offer FULL wipe (reclaim).
      if (deviceKey.isNotEmpty && result.receivedRecords > 0) {
        final stored = await RecordLocalStore.instance.getUnpushed(
          deviceId: deviceKey,
          limit: 2000,
        );
        if (stored.isNotEmpty) {
          await _pushToServer(
            stored.map((r) => r.toSyncedRecord()).toList(),
            deviceKey,
          );
        }
      }

      final perType = <int, int>{};
      final session = _SyncSession(
        timestamp: DateTime.now(),
        recordCount: result.receivedRecords,
        perType: perType,
      );
      _sessions.insert(0, session);
      await _saveSessions();
      await prefs.setInt(_kLastSyncMode, recModeFull);
      unawaited(b.refreshDeviceInfo());
      if (!mounted) return;
      setState(() {
        _syncing = false;
        _received = result.receivedRecords;
        _lastSyncIncludedRaw = true;
        _perType
          ..clear()
          ..addAll(perType);
        _wifiSyncStatus = result.message;
        if (result.upToId > _lastPulledId) {
          _lastPulledId = result.upToId;
        }
      });

      if (result.upToId > 0 && mounted) {
        final wipe = await _confirmFullWipe(result.upToId);
        if (wipe == true && mounted) {
          await _wipeDeviceUpTo(result.upToId, mode: recModeFull);
        }
      }
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _syncing = false;
        _error = e.toString();
      });
    }
  }

  Future<bool> _confirmHotspotReady(String ssid) async {
    if (!mounted) return false;
    final ok = await showDialog<bool>(
      context: context,
      barrierDismissible: false,
      builder: (ctx) => AlertDialog(
        title: const Text('Enable Personal Hotspot'),
        content: Text(
          'In Settings, turn on Personal Hotspot named "$ssid", then return '
          'and tap Continue. The device will join that hotspot.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(ctx).pop(true),
            child: const Text('Continue'),
          ),
        ],
      ),
    );
    return ok == true;
  }

  Future<WifiNetworkCredentials?> _askHotspotCredentials() async {
    if (!mounted) return null;
    final ssidCtrl = TextEditingController();
    final passCtrl = TextEditingController();
    try {
      return showDialog<WifiNetworkCredentials>(
        context: context,
        builder: (ctx) => AlertDialog(
          title: const Text('Save phone hotspot'),
          content: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const Text(
                'Enter the SSID and password of this phone’s Personal Hotspot. '
                'Saved encrypted for future Wi-Fi sync fallback.',
              ),
              const SizedBox(height: 12),
              TextField(
                controller: ssidCtrl,
                decoration: const InputDecoration(labelText: 'Hotspot SSID'),
                textInputAction: TextInputAction.next,
              ),
              TextField(
                controller: passCtrl,
                decoration: const InputDecoration(labelText: 'Hotspot password'),
                obscureText: true,
              ),
            ],
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(ctx).pop(),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () {
                final ssid = ssidCtrl.text.trim();
                final psk = passCtrl.text;
                if (ssid.isEmpty || psk.isEmpty) return;
                Navigator.of(ctx).pop(
                  WifiNetworkCredentials(ssid: ssid, psk: psk),
                );
              },
              child: const Text('Save'),
            ),
          ],
        ),
      );
    } finally {
      ssidCtrl.dispose();
      passCtrl.dispose();
    }
  }

  String _phaseLabel(String phase) {
    switch (phase) {
      case 'provisioning_wifi':
        return 'Provisioning Wi-Fi…';
      case 'starting_wifi_session':
        return 'Starting Wi-Fi session…';
      case 'receiving_wifi':
        return 'Receiving over Wi-Fi…';
      case 'fallback_ble':
        return 'Wi-Fi failed; using BLE…';
      case 'receiving_ble':
        return 'Receiving over BLE…';
      case 'ota_firmware_ble':
        return 'Firmware over BLE SMP…';
      case 'ota_model_ble':
        return 'Model over BLE…';
      case 'ota_resource_ble':
        return 'Resources over BLE…';
      case 'done':
        return 'Sync complete';
      default:
        return 'Preparing sync…';
    }
  }

  Future<bool?> _confirmMarkSummaries(int maxId) {
    return showDialog<bool>(
      context: context,
      barrierDismissible: false,
      builder: (ctx) => AlertDialog(
        title: Text('Synced $_received summaries'),
        content: Text(
          'Records are saved on this phone through #$maxId.\n\n'
          'Marking summaries received does not free device storage and does '
          'not delete PPG/glucose raw waveforms. Run Sync raw, then wipe, '
          'to reclaim NOR space.',
        ),
        actions: [
          FilledButton(
            onPressed: () => Navigator.of(ctx).pop(false),
            child: const Text('Skip'),
          ),
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(true),
            child: const Text('Mark summaries received'),
          ),
        ],
      ),
    );
  }

  Future<bool?> _confirmFullWipe(int maxId) async {
    // Two-phase durability: await cloud push when signed in before offering wipe.
    var cloudOk = true;
    if (_auth?.isAuthenticated ?? false) {
      if (mounted) {
        setState(() => _serverPushStatus = 'Server: waiting before wipe…');
      }
      // Give in-flight push from _startSync a moment; then require success or override.
      await Future<void>.delayed(const Duration(milliseconds: 100));
      if (_serverPushStatus.contains('error') ||
          _serverPushStatus.contains('failed')) {
        cloudOk = false;
      }
    }

    if (!mounted) return false;

    final cloudNote = !(_auth?.isAuthenticated ?? false)
        ? '\n\nNot signed in — wipe will free the device using phone-local copies only.'
        : (cloudOk
              ? '\n\nCloud push finished (or was not needed).'
              : '\n\nCloud push may have failed. You can wipe anyway (phone keeps a copy) '
                    'or cancel and retry from the Server tab.');

    return showDialog<bool>(
      context: context,
      barrierDismissible: false,
      builder: (ctx) => AlertDialog(
        title: Text('Synced $_received records (full)'),
        content: Text(
          'Records including raw waveforms are saved on this phone through #$maxId. '
          'Wipe frees device NOR storage and cannot be undone on the device.'
          '$cloudNote',
        ),
        actions: [
          FilledButton(
            onPressed: () => Navigator.of(ctx).pop(false),
            child: const Text('Keep on device'),
          ),
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(true),
            child: Text(
              cloudOk ? 'Sync and wipe' : 'Wipe without cloud',
              style: const TextStyle(color: NiSenseColors.accentRed),
            ),
          ),
        ],
      ),
    );
  }

  Future<void> _wipeDeviceUpTo(int upToId, {required int mode}) async {
    final b = context.read<HcmBackend>();
    if (!b.connected) {
      setState(() => _error = 'Not connected — could not update device');
      return;
    }
    final device = b.client.device;
    if (device == null) {
      setState(() => _error = 'Device handle unavailable');
      return;
    }

    setState(() {
      _clearingDevice = true;
      _error = null;
    });
    try {
      await b.runWithExclusiveNotifies(
        mode == recModeFull
            ? 'Clearing device records'
            : 'Marking summaries received',
        () async {
          final client = RecordSyncClient(device);
          try {
            await client.ack(upToId, mode: mode);
          } finally {
            await client.dispose();
          }
        },
      );
      final prefs = await SharedPreferences.getInstance();
      if (mode == recModeFull) {
        await prefs.setInt(_kLastAcked, upToId);
      }
      final pulled = prefs.getInt(_kLastPulled) ?? 0;
      if (pulled < upToId) {
        await prefs.setInt(_kLastPulled, upToId);
      }
      unawaited(b.refreshDeviceInfo());
      if (!mounted) return;
      setState(() {
        if (mode == recModeFull) {
          _lastAckedId = upToId;
        }
        if (_lastPulledId < upToId) _lastPulledId = upToId;
        _clearingDevice = false;
      });
    } catch (e) {
      if (mounted) {
        setState(() {
          _clearingDevice = false;
          _error = e.toString();
        });
      }
    }
  }

  Future<void> _clearDevice() async {
    final b = context.read<HcmBackend>();
    if (!b.connected) {
      setState(() => _error = 'Not connected to device');
      return;
    }
    if (_lastPulledId <= 0) {
      setState(
        () => _error = 'Nothing to wipe yet — Sync from the device first.',
      );
      return;
    }

    final prefs = await SharedPreferences.getInstance();
    final lastMode = prefs.getInt(_kLastSyncMode) ?? recModeSummaryOnly;
    final canFullWipe = lastMode == recModeFull || _lastSyncIncludedRaw;

    if (!canFullWipe) {
      if (!mounted) return;
      final mark = await showDialog<bool>(
        context: context,
        builder: (ctx) => AlertDialog(
          title: const Text('Mark summaries only?'),
          content: const Text(
            'The last pull was summaries-only. Device wipe that frees NOR '
            'space requires Sync raw (full) first so raw waveforms are on '
            'this phone.\n\n'
            'You can mark summaries as received (no space freed, raw kept).',
          ),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(ctx).pop(false),
              child: const Text('Cancel'),
            ),
            FilledButton(
              onPressed: () => Navigator.of(ctx).pop(true),
              child: const Text('Mark summaries'),
            ),
          ],
        ),
      );
      if (mark == true && mounted) {
        await _wipeDeviceUpTo(_lastPulledId, mode: recModeSummaryOnly);
      }
      return;
    }

    final confirm = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Clear device records?'),
        content: Text(
          'Permanently delete pending records on the device up to '
          'record #$_lastPulledId (FULL reclaim). This cannot be undone '
          'on the device.\n\n'
          'Phone copies are kept unless you clear phone records separately.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(false),
            child: const Text('Cancel'),
          ),
          FilledButton(
            style: FilledButton.styleFrom(
              backgroundColor: NiSenseColors.accentRed,
            ),
            onPressed: () => Navigator.of(ctx).pop(true),
            child: const Text('Clear device'),
          ),
        ],
      ),
    );
    if (confirm != true || !mounted) return;
    await _wipeDeviceUpTo(_lastPulledId, mode: recModeFull);
  }

  Future<void> _exportAll() async {
    setState(() {
      _exporting = true;
      _error = null;
    });
    try {
      final patient = context.read<HcmBackend>().patientName;
      final path = await RecordXlsxExporter.exportAllFromStore(
        patientName: patient.isNotEmpty ? patient : 'Patient',
      );
      if (!mounted) return;
      if (path == null) {
        setState(() => _error = 'No records on this phone to export');
      }
    } catch (e) {
      if (mounted) setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() => _exporting = false);
    }
  }

  Future<void> _clearPhoneRecords() async {
    final total = await RecordLocalStore.instance.totalCount();
    if (total == 0) {
      setState(() => _error = 'No records on this phone');
      return;
    }
    final unpushed = await RecordLocalStore.instance.countUnpushed();
    final signedIn = _auth?.isAuthenticated ?? false;

    if (!mounted) return;
    final confirm = await showDialog<bool>(
      context: context,
      builder: (ctx) {
        final warning = !signedIn
            ? 'You are not signed in — none of these $total records are backed '
                  'up to the server. Clearing will permanently delete them from '
                  'this phone.'
            : unpushed > 0
            ? '$unpushed of $total records have not been synced to the '
                  'server and will be permanently lost.'
            : 'Delete all $total records stored on this phone? '
                  'They appear to be already on the server.';
        return AlertDialog(
          title: const Text('Clear phone records?'),
          content: Text(warning),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(ctx).pop(false),
              child: const Text('Cancel'),
            ),
            FilledButton(
              style: FilledButton.styleFrom(
                backgroundColor: NiSenseColors.accentRed,
              ),
              onPressed: () => Navigator.of(ctx).pop(true),
              child: const Text('Clear phone'),
            ),
          ],
        );
      },
    );
    if (confirm != true || !mounted) return;

    setState(() {
      _clearingPhone = true;
      _error = null;
    });
    try {
      await RecordLocalStore.instance.clearAll();
      await _refreshLocalCount();
    } catch (e) {
      if (mounted) setState(() => _error = e.toString());
    } finally {
      if (mounted) setState(() => _clearingPhone = false);
    }
  }

  double get _progress {
    // Only meaningful after a finished transfer: received / received (= 100%).
    // While syncing we use an indeterminate bar — device does not advertise a
    // fixed "will send N" up front (sentCount climbs as it transmits).
    if (_syncing || _received == 0) return 0;
    return 1.0;
  }

  bool get _busy => _syncing || _exporting || _clearingPhone || _clearingDevice;

  bool get _deviceMayHavePending => _lastPulledId > _lastAckedId;

  @override
  Widget build(BuildContext context) {
    super.build(context); // required by AutomaticKeepAliveClientMixin
    final b = context.watch<HcmBackend>();
    final lastSync = _sessions.isNotEmpty ? _sessions.first.timestamp : null;
    final lastSyncStr = lastSync != null
        ? '${lastSync.day} ${_monthName(lastSync.month)} ${lastSync.year} '
              '${lastSync.hour.toString().padLeft(2, '0')}:${lastSync.minute.toString().padLeft(2, '0')}'
        : 'Never';

    return ListView(
      padding: const EdgeInsets.all(12),
      children: [
        Row(
          children: [
            Expanded(
              child: Tooltip(
                message:
                    'Pull new clinical summaries only (temp / vitals / glucose). '
                    'Fast day-to-day sync — skips PPG and glucose raw waveforms.',
                child: FilledButton.icon(
                  icon: const Icon(Icons.sync),
                  label: Text(_syncing ? 'Syncing…' : 'Sync'),
                  onPressed: (_busy || !b.connected)
                      ? null
                      : () => _startSync(),
                ),
              ),
            ),
            const SizedBox(width: 8),
            Tooltip(
              message:
                  'Re-pull every summary still on the device (afterId=0). '
                  'Does not include raw waveforms.',
              child: OutlinedButton(
                onPressed: (_busy || !b.connected)
                    ? null
                    : () async {
                        final ok = await showDialog<bool>(
                          context: context,
                          builder: (ctx) => AlertDialog(
                            title: const Text('Sync all summaries?'),
                            content: const Text(
                              'Re-pulls every clinical summary still on the device '
                              '(no PPG/glucose raw). Prefer Sync for new records only.',
                            ),
                            actions: [
                              TextButton(
                                onPressed: () => Navigator.of(ctx).pop(false),
                                child: const Text('Cancel'),
                              ),
                              FilledButton(
                                onPressed: () => Navigator.of(ctx).pop(true),
                                child: const Text('Sync all'),
                              ),
                            ],
                          ),
                        );
                        if (ok == true && mounted) {
                          await _startSync(resetCursor: true);
                        }
                      },
                child: const Text('Sync all'),
              ),
            ),
          ],
        ),
        const SizedBox(height: 8),
        Tooltip(
          message:
              'Full pull including PPG and glucose raw waveforms. '
              'Much slower — use for AI export / archive, then wipe if needed.',
          child: OutlinedButton.icon(
            icon: const Icon(Icons.waves, size: 18),
            label: const Text('Sync raw (full)'),
            onPressed: (_busy || !b.connected)
                ? null
                : () async {
                    final ok = await showDialog<bool>(
                      context: context,
                      builder: (ctx) => AlertDialog(
                        title: const Text('Sync raw waveforms?'),
                        content: const Text(
                          'Downloads every pending record including PPG and glucose '
                          'raw chunks. This can take many minutes on a full device. '
                          'Day-to-day use: prefer Sync (summaries only).',
                        ),
                        actions: [
                          TextButton(
                            onPressed: () => Navigator.of(ctx).pop(false),
                            child: const Text('Cancel'),
                          ),
                          FilledButton(
                            onPressed: () => Navigator.of(ctx).pop(true),
                            child: const Text('Sync raw'),
                          ),
                        ],
                      ),
                    );
                    if (ok == true && mounted) {
                      // Full history from device: afterId=0 so skipped raws
                      // behind the summary cursor are still fetched.
                      await _startSync(resetCursor: true, includeRaw: true);
                    }
                  },
          ),
        ),
        const SizedBox(height: 8),
        Tooltip(
          message:
              'Provision device Wi-Fi over BLE, then receive full records over LAN. '
              'Falls back to BLE if Wi-Fi cannot start.',
          child: FilledButton.icon(
            icon: const Icon(Icons.wifi, size: 18),
            label: Text(
              _syncing && _lastSyncUsedWifi
                  ? 'Wi-Fi syncing…'
                  : 'Sync over Wi-Fi',
            ),
            onPressed: (_busy || !b.connected) ? null : _startWifiSync,
          ),
        ),
        const SizedBox(height: 4),
        if (_syncing)
          Padding(
            padding: const EdgeInsets.only(bottom: 4),
            child: Text(
              _lastSyncUsedWifi
                  ? 'Wi-Fi sync — keep this phone on the same network until finished.'
                  : _lastSyncIncludedRaw
                  ? 'Full sync (summaries + raw) — stay on Sync until finished.'
                  : 'Summary sync — stay on Sync until finished.',
              style: Theme.of(
                context,
              ).textTheme.bodySmall?.copyWith(color: NiSenseColors.textMuted),
            ),
          ),
        Row(
          children: [
            Expanded(
              child: Text(
                'Last sync: $lastSyncStr',
                style: Theme.of(
                  context,
                ).textTheme.bodySmall?.copyWith(color: NiSenseColors.textMuted),
              ),
            ),
            TextButton.icon(
              icon: const Icon(Icons.storage_outlined, size: 18),
              label: const Text('All Records'),
              onPressed: _busy
                  ? null
                  : () => Navigator.of(context).push(
                      MaterialPageRoute<void>(
                        builder: (_) => const AllRecordsPage(),
                      ),
                    ),
            ),
          ],
        ),
        Text(
          '$_localRecordCount records on this phone'
          '${_deviceMayHavePending ? ' · device may still hold unsynced wipe data' : ''}',
          style: Theme.of(
            context,
          ).textTheme.bodySmall?.copyWith(color: NiSenseColors.textMuted),
        ),
        if ((b.deviceBuild?.dropped ?? 0) > 0 ||
            (b.deviceBuild?.crcFailCount ?? 0) > 0)
          Padding(
            padding: const EdgeInsets.only(top: 6),
            child: Text(
              'Device storage health: '
              '${b.deviceBuild!.dropped > 0 ? 'overwrote ${b.deviceBuild!.dropped} unsynced record(s)' : ''}'
              '${b.deviceBuild!.dropped > 0 && b.deviceBuild!.crcFailCount > 0 ? '; ' : ''}'
              '${b.deviceBuild!.crcFailCount > 0 ? '${b.deviceBuild!.crcFailCount} CRC error(s) on read' : ''}. '
              'Sync soon to avoid further loss.',
              style: Theme.of(context).textTheme.bodySmall?.copyWith(
                color: NiSenseColors.accentRed,
              ),
            ),
          ),
        if (_serverPushStatus.isNotEmpty)
          Text(
            _serverPushStatus,
            style: Theme.of(
              context,
            ).textTheme.bodySmall?.copyWith(color: NiSenseColors.textMuted),
          )
        else if (!(_auth?.isAuthenticated ?? false))
          Text(
            'Not signed in — records stay on this phone only (see Server tab to sign in).',
            style: Theme.of(
              context,
            ).textTheme.bodySmall?.copyWith(color: NiSenseColors.textMuted),
          ),
        if (_wifiSyncStatus.isNotEmpty)
          Text(
            'Wi-Fi: $_wifiSyncStatus',
            style: Theme.of(
              context,
            ).textTheme.bodySmall?.copyWith(color: NiSenseColors.textMuted),
          ),

        const SizedBox(height: 8),
        Row(
          children: [
            Expanded(
              child: OutlinedButton.icon(
                icon: _exporting
                    ? const SizedBox(
                        width: 16,
                        height: 16,
                        child: CircularProgressIndicator(strokeWidth: 2),
                      )
                    : const Icon(Icons.ios_share, size: 18),
                label: Text(_exporting ? 'Exporting…' : 'Export all'),
                onPressed: (_busy || _localRecordCount == 0)
                    ? null
                    : _exportAll,
              ),
            ),
            const SizedBox(width: 8),
            Expanded(
              child: OutlinedButton.icon(
                icon: const Icon(Icons.delete_outline, size: 18),
                label: Text(_clearingPhone ? 'Clearing…' : 'Clear phone'),
                style: OutlinedButton.styleFrom(
                  foregroundColor: NiSenseColors.accentRed,
                ),
                onPressed: (_busy || _localRecordCount == 0)
                    ? null
                    : _clearPhoneRecords,
              ),
            ),
          ],
        ),
        const SizedBox(height: 8),
        OutlinedButton.icon(
          icon: const Icon(Icons.phonelink_erase, size: 18),
          label: Text(_clearingDevice ? 'Clearing device…' : 'Clear device'),
          style: OutlinedButton.styleFrom(
            foregroundColor: NiSenseColors.accentRed,
          ),
          onPressed: (_busy || !b.connected) ? null : _clearDevice,
        ),

        if (_error != null) ...[
          const SizedBox(height: 8),
          Card(
            color: NiSenseColors.accentRed.withValues(alpha: 0.12),
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: Text(
                _error!,
                style: const TextStyle(color: NiSenseColors.accentRed),
              ),
            ),
          ),
        ],

        if (_syncing || _received > 0) ...[
          const SizedBox(height: 12),
          Card(
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    children: [
                      Expanded(
                        child: LinearProgressIndicator(
                          value: _syncing ? null : _progress.clamp(0.0, 1.0),
                          backgroundColor: NiSenseColors.borderMid,
                        ),
                      ),
                      const SizedBox(width: 8),
                      Text(
                        _syncing
                            ? '…'
                            : (_incompleteTransfer ? 'incomplete' : 'done'),
                        style: Theme.of(context).textTheme.bodySmall,
                      ),
                    ],
                  ),
                  const SizedBox(height: 4),
                  Text(
                    _syncing
                        ? 'Receiving $_received'
                              '${_deviceSent > _received ? ' (device sent $_deviceSent so far)' : ''}…'
                        : 'Saved $_received record${_received == 1 ? '' : 's'} this sync',
                    style: Theme.of(context).textTheme.bodySmall,
                  ),
                  if (!_syncing && _devicePending > 0)
                    Text(
                      'Device still has $_devicePending un-wiped record(s) in NOR '
                      '(not the same as this transfer count).',
                      style: Theme.of(context).textTheme.bodySmall?.copyWith(
                        color: NiSenseColors.textMuted,
                      ),
                    ),
                  if (_syncing && _devicePending > 0)
                    Text(
                      'NOR pending (un-wiped on device): $_devicePending',
                      style: Theme.of(context).textTheme.bodySmall?.copyWith(
                        color: NiSenseColors.textMuted,
                      ),
                    ),
                  if (_perType.isNotEmpty) ...[
                    const SizedBox(height: 4),
                    Wrap(
                      spacing: 12,
                      children: [
                        for (final e in _perType.entries)
                          Text(
                            '${_typeName(e.key)}: ${e.value}',
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                      ],
                    ),
                  ],
                  if (_errorCount > 0)
                    Text(
                      'Errors: $_errorCount',
                      style: const TextStyle(color: NiSenseColors.accentRed),
                    ),
                ],
              ),
            ),
          ),
        ],

        const SizedBox(height: 12),
        const Divider(),
        const SizedBox(height: 4),
        Text('Previous syncs', style: Theme.of(context).textTheme.titleSmall),
        const SizedBox(height: 8),

        if (_sessions.isEmpty)
          Text(
            'No syncs yet.',
            style: Theme.of(
              context,
            ).textTheme.bodySmall?.copyWith(color: NiSenseColors.textMuted),
          )
        else
          for (final s in _sessions)
            _SessionTile(session: s, typeName: _typeName),
      ],
    );
  }

  String _monthName(int m) {
    const months = [
      '',
      'Jan',
      'Feb',
      'Mar',
      'Apr',
      'May',
      'Jun',
      'Jul',
      'Aug',
      'Sep',
      'Oct',
      'Nov',
      'Dec',
    ];
    return months[m.clamp(1, 12)];
  }
}

class _SessionTile extends StatelessWidget {
  const _SessionTile({required this.session, required this.typeName});

  final _SyncSession session;
  final String Function(int) typeName;

  @override
  Widget build(BuildContext context) {
    final ts = session.timestamp;
    final label =
        '${ts.day} ${_monthName(ts.month)} ${ts.year} '
        '${ts.hour.toString().padLeft(2, '0')}:${ts.minute.toString().padLeft(2, '0')}';
    final counts = session.perType.entries
        .map((e) => '${typeName(e.key)}: ${e.value}')
        .join('  ');

    return Card(
      margin: const EdgeInsets.only(bottom: 8),
      child: ListTile(
        title: Text('$label  ·  ${session.recordCount} records'),
        subtitle: Text(counts.isEmpty ? '—' : counts),
        leading: const Icon(Icons.history),
      ),
    );
  }

  String _monthName(int m) {
    const months = [
      '',
      'Jan',
      'Feb',
      'Mar',
      'Apr',
      'May',
      'Jun',
      'Jul',
      'Aug',
      'Sep',
      'Oct',
      'Nov',
      'Dec',
    ];
    return months[m.clamp(1, 12)];
  }
}
