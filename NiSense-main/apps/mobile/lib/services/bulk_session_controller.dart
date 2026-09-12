import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:shared_preferences/shared_preferences.dart';

import '../ble/model_transfer_client.dart';
import '../ble/record_sync_client.dart';
import '../ble/resource_transfer_client.dart';
import '../dfu/smp_client.dart';
import '../net/local_transfer_server.dart';
import '../protocol/hcm_protocol.dart';
import '../state/hcm_backend.dart';
import '../util/app_log.dart';
import 'record_local_store.dart';
import 'wifi_network_vault.dart';
import 'wifi_provision_coordinator.dart';

class BulkSessionProgress {
  const BulkSessionProgress({
    required this.phase,
    this.receivedRecords = 0,
    this.deviceSent = 0,
    this.devicePending = 0,
    this.upToId = 0,
    this.message = '',
  });

  final String phase;
  final int receivedRecords;
  final int deviceSent;
  final int devicePending;
  final int upToId;
  final String message;
}

class BulkSessionResult {
  const BulkSessionResult({
    required this.usedWifi,
    required this.receivedRecords,
    required this.upToId,
    this.message = '',
  });

  final bool usedWifi;
  final int receivedRecords;
  final int upToId;
  final String message;
}

class BulkSessionController {
  BulkSessionController({
    required HcmBackend backend,
    LocalTransferServer? server,
    WifiProvisionCoordinator? wifi,
    WifiNetworkVault? vault,
  }) : _backend = backend,
       _server = server ?? LocalTransferServer(),
       _wifi =
           wifi ??
           WifiProvisionCoordinator(
             backend: backend,
             vault: vault ?? WifiNetworkVault(),
           );

  static const _kLastPulled = 'last_pulled_record_id';

  final HcmBackend _backend;
  final LocalTransferServer _server;
  final WifiProvisionCoordinator _wifi;

  Future<BulkSessionResult> run({
    required String deviceId,
    required int afterId,
    int mode = recModeFull,
    String? manualSsid,
    String? password,
    bool forceHotspot = false,
    WifiPasswordPrompt? askPassword,
    WifiInstructionSink? instructions,
    WifiHotspotReadyConfirm? confirmHotspotReady,
    WifiHotspotCredentialsPrompt? askHotspotCredentials,
    void Function(BulkSessionProgress progress)? onProgress,
    String? modelPath,
    String? resourcePath,
    String? firmwarePath,
  }) async {
    onProgress?.call(const BulkSessionProgress(phase: 'estimating'));
    final estimate = _estimateBytes();
    onProgress?.call(
      BulkSessionProgress(
        phase: 'provisioning_wifi',
        devicePending: _backend.deviceBuild?.pendingRecords ?? 0,
        message: estimate > 0 ? 'Estimated $estimate bytes pending.' : '',
      ),
    );

    try {
      WifiProvisionResult provision;
      if (forceHotspot) {
        provision = await _wifi.provisionHotspotFallback(
          instructions: instructions,
          confirmHotspotReady: confirmHotspotReady,
          askHotspotCredentials: askHotspotCredentials,
        );
      } else {
        provision = await _wifi.provisionPremisesWifi(
          manualSsid: manualSsid,
          password: password,
          askPassword: askPassword,
        );
        if (!provision.success) {
          instructions?.call(provision.message);
          provision = await _wifi.provisionHotspotFallback(
            instructions: instructions,
            confirmHotspotReady: confirmHotspotReady,
            askHotspotCredentials: askHotspotCredentials,
          );
        }
      }
      if (!provision.success) {
        throw StateError(provision.message);
      }

      final result = await _runWifiSession(
        deviceId: deviceId,
        afterId: afterId,
        mode: mode,
        onProgress: onProgress,
        modelPath: modelPath,
        resourcePath: resourcePath,
        firmwarePath: firmwarePath,
      );

      // Firmware stays on BLE SMP; model/resource are applied in-session over Wi-Fi.
      await _pushBleFirmwareIfNeeded(
        firmwarePath: firmwarePath,
        onProgress: onProgress,
      );

      return result;
    } catch (e) {
      AppLog.w('wifi-sync', 'Wi-Fi session failed; falling back to BLE', e);
      onProgress?.call(
        BulkSessionProgress(phase: 'fallback_ble', message: 'Wi-Fi failed: $e'),
      );
      final ble = await _runBleFallback(
        deviceId: deviceId,
        afterId: afterId,
        mode: mode,
        onProgress: onProgress,
      );
      try {
        await _pushBleOtaFallback(
          modelPath: modelPath,
          resourcePath: resourcePath,
          firmwarePath: firmwarePath,
          onProgress: onProgress,
        );
      } catch (otaErr) {
        AppLog.w('wifi-sync', 'BLE OTA after fallback failed', otaErr);
      }
      return ble;
    } finally {
      await _server.stop();
    }
  }

  Future<BulkSessionResult> _runWifiSession({
    required String deviceId,
    required int afterId,
    required int mode,
    required void Function(BulkSessionProgress progress)? onProgress,
    String? modelPath,
    String? resourcePath,
    String? firmwarePath,
  }) async {
    await _server.start(
      deviceId: deviceId,
      modelPath: modelPath,
      resourcePath: resourcePath,
      firmwarePath: firmwarePath,
    );
    final host = await _hostForDevice();
    final flags = _otaFlags(
      modelPath: modelPath,
      resourcePath: resourcePath,
      firmwarePath: firmwarePath,
    );
    final session = BulkSessionStart(
      host: host,
      port: _server.port,
      token: _server.sessionToken,
      afterId: afterId,
      mode: mode,
      flags: flags,
    );

    onProgress?.call(
      BulkSessionProgress(
        phase: 'starting_wifi_session',
        message: '$host:${_server.port}',
      ),
    );
    await _backend.client.writeUuid(
      chrcBulkSessionCtrl,
      encodeBulkSessionStart(session),
    );

    final finished = await _monitorWifiSession(onProgress);
    final stats = _server.stats;
    final upToId = stats.upToId > 0 ? stats.upToId : finished?.upToId ?? 0;
    // Do not ACK here — host completes two-phase durability (SQLite + optional
    // cloud) then sends mode-aware ACK from the Sync UI.
    if (upToId > 0) {
      final prefs = await SharedPreferences.getInstance();
      final prev = prefs.getInt(_kLastPulled) ?? 0;
      if (upToId > prev) {
        await prefs.setInt(_kLastPulled, upToId);
      }
      await prefs.setInt('last_sync_mode', mode);
    }
    onProgress?.call(
      BulkSessionProgress(
        phase: 'done',
        receivedRecords: stats.receivedRecords,
        deviceSent: finished?.sentCount ?? 0,
        devicePending: finished?.pending ?? 0,
        upToId: upToId,
      ),
    );
    return BulkSessionResult(
      usedWifi: true,
      receivedRecords: stats.receivedRecords,
      upToId: upToId,
      message: 'Wi-Fi sync complete',
    );
  }

  Future<void> _pushBleFirmwareIfNeeded({
    String? firmwarePath,
    void Function(BulkSessionProgress progress)? onProgress,
  }) async {
    if (firmwarePath == null || firmwarePath.isEmpty) return;
    final device = _backend.client.device;
    if (device == null) return;

    onProgress?.call(
      const BulkSessionProgress(
        phase: 'ota_firmware_ble',
        message: 'Pushing firmware over BLE SMP…',
      ),
    );
    final smp = SmpClient(device);
    await smp.uploadImage(firmwarePath);
    await smp.setImagePending();
    await smp.resetDevice();
  }

  Future<void> _pushBleOtaFallback({
    String? modelPath,
    String? resourcePath,
    String? firmwarePath,
    void Function(BulkSessionProgress progress)? onProgress,
  }) async {
    await _pushBleFirmwareIfNeeded(
      firmwarePath: firmwarePath,
      onProgress: onProgress,
    );

    final device = _backend.client.device;
    if (device == null) return;

    if (modelPath != null && modelPath.isNotEmpty) {
      try {
        onProgress?.call(
          const BulkSessionProgress(
            phase: 'ota_model_ble',
            message: 'Pushing model over BLE…',
          ),
        );
        final bytes = await File(modelPath).readAsBytes();
        await ModelTransferClient(device).upload(
          bytes,
          version: 0,
          variant: 0,
        );
      } catch (e) {
        AppLog.w('wifi-sync', 'BLE model fallback failed', e);
      }
    }

    if (resourcePath != null && resourcePath.isNotEmpty) {
      try {
        onProgress?.call(
          const BulkSessionProgress(
            phase: 'ota_resource_ble',
            message: 'Pushing resources over BLE…',
          ),
        );
        final bytes = await File(resourcePath).readAsBytes();
        await ResourceTransferClient(device).upload(Uint8List.fromList(bytes));
      } catch (e) {
        AppLog.w('wifi-sync', 'BLE resource fallback failed', e);
      }
    }
  }

  Future<BulkSessionStatus?> _monitorWifiSession(
    void Function(BulkSessionProgress progress)? onProgress,
  ) async {
    final deadline = DateTime.now().add(const Duration(minutes: 20));
    var lastProgressAt = DateTime.now();
    var lastReceived = 0;
    var lastSent = 0;
    BulkSessionStatus? last;
    while (DateTime.now().isBefore(deadline)) {
      try {
        final raw = await _backend.client.readUuid(chrcBulkSessionStatus);
        last = decodeBulkSessionStatus(raw);
      } catch (e) {
        AppLog.w('wifi-sync', 'status read failed', e);
      }
      final stats = _server.stats;
      if (stats.receivedRecords > lastReceived ||
          (last != null && last.sentCount > lastSent)) {
        lastReceived = stats.receivedRecords;
        lastSent = last?.sentCount ?? lastSent;
        lastProgressAt = DateTime.now();
      } else if (last != null &&
          last.state == bulkSessionStateTransferring) {
        // In-session OTA keeps TRANSFERRING without new records.
        lastProgressAt = DateTime.now();
      }
      onProgress?.call(
        BulkSessionProgress(
          phase: 'receiving_wifi',
          receivedRecords: stats.receivedRecords,
          deviceSent: last?.sentCount ?? 0,
          devicePending: last?.pending ?? 0,
          upToId: stats.upToId,
        ),
      );
      if (last != null) {
        if (last.error != 0 || last.state == bulkSessionStateError) {
          throw StateError('Bulk session error ${last.error}');
        }
        final sawTransfer = stats.receivedRecords > 0 || last.sentCount > 0;
        final idleAfterWork =
            last.state == bulkSessionStateIdle ||
            last.state == bulkSessionStateTeardown;
        final completing =
            last.state == bulkSessionStateCompleting;
        // Prefer IDLE so in-session Wi-Fi OTA finishes before the phone
        // tears down LocalTransferServer.
        if ((sawTransfer || last.pending == 0) && idleAfterWork) {
          await Future<void>.delayed(const Duration(seconds: 1));
          return last;
        }
        if ((sawTransfer || last.pending == 0) && completing) {
          // Hold briefly; OTA may still be running under TRANSFERRING.
          lastProgressAt = DateTime.now();
        }
      }
      if (DateTime.now().difference(lastProgressAt) >
              const Duration(minutes: 2) &&
          stats.receivedRecords == 0) {
        throw TimeoutException('Wi-Fi bulk session made no progress');
      }
      await Future<void>.delayed(const Duration(milliseconds: 750));
    }
    throw TimeoutException('Wi-Fi bulk session timed out');
  }

  Future<BulkSessionResult> _runBleFallback({
    required String deviceId,
    required int afterId,
    required int mode,
    required void Function(BulkSessionProgress progress)? onProgress,
  }) async {
    final device = _backend.client.device;
    if (device == null) {
      throw StateError('Device handle unavailable for BLE fallback');
    }
    final client = RecordSyncClient(device);
    try {
      final records = await client.pull(
        afterId: afterId,
        mode: mode,
        onStatus: (st) => onProgress?.call(
          BulkSessionProgress(
            phase: 'receiving_ble',
            receivedRecords: client.receivedCount,
            deviceSent: st.sentCount,
            devicePending: st.pending,
            upToId: st.cursorId,
          ),
        ),
      );
      await RecordLocalStore.instance.insertAll(deviceId, records);
      final upToId = records.isEmpty
          ? afterId
          : records.map((r) => r.recordId).reduce((a, b) => a > b ? a : b);
      final prefs = await SharedPreferences.getInstance();
      final prev = prefs.getInt(_kLastPulled) ?? 0;
      if (upToId > prev) {
        await prefs.setInt(_kLastPulled, upToId);
      }
      return BulkSessionResult(
        usedWifi: false,
        receivedRecords: records.length,
        upToId: upToId,
        message: 'BLE fallback sync complete',
      );
    } finally {
      await client.dispose();
    }
  }

  int _estimateBytes() {
    final pending = _backend.deviceBuild?.pendingRecords ?? 0;
    if (pending <= 0) return 0;
    return pending * 256;
  }

  Future<String> _hostForDevice() async {
    final interfaces = await NetworkInterface.list(
      includeLoopback: false,
      type: InternetAddressType.IPv4,
    );
    for (final interface in interfaces) {
      final name = interface.name.toLowerCase();
      if (!name.contains('wlan') &&
          !name.contains('wifi') &&
          !name.contains('ap') &&
          !name.contains('swlan') &&
          !name.contains('softap')) {
        continue;
      }
      if (interface.addresses.isNotEmpty) {
        return interface.addresses.first.address;
      }
    }
    for (final interface in interfaces) {
      if (interface.addresses.isNotEmpty) {
        return interface.addresses.first.address;
      }
    }
    throw StateError('No IPv4 address available for Wi-Fi transfer');
  }

  int _otaFlags({
    String? modelPath,
    String? resourcePath,
    String? firmwarePath,
  }) {
    var flags = 0;
    if (modelPath != null && modelPath.isNotEmpty) {
      flags |= bulkSessionFlagOtaModel;
    }
    if (resourcePath != null && resourcePath.isNotEmpty) {
      flags |= bulkSessionFlagOtaResource;
    }
    if (firmwarePath != null && firmwarePath.isNotEmpty) {
      flags |= bulkSessionFlagOtaFirmware;
    }
    return flags;
  }
}
