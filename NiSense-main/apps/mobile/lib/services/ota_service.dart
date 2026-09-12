import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:path_provider/path_provider.dart';

import '../ble/model_transfer_client.dart';
import '../ble/resource_transfer_client.dart';
import '../dfu/smp_client.dart';
import '../net/api_client.dart';
import 'ota_bundle.dart';

/// Server-driven OTA: single-component artifacts or nisense-ota-v1 ZIP bundles.
class OtaService extends ChangeNotifier {
  OtaService(this._api);

  final ApiClient _api;

  double _progress = 0;
  String _status = 'idle';
  String _phase = '';

  double get progress => _progress;
  String get status => _status;
  String get phase => _phase;

  void _set(String s, [double? p, String? phase]) {
    _status = s;
    if (p != null) _progress = p;
    if (phase != null) _phase = phase;
    notifyListeners();
  }

  /// Firmware update: pull latest firmware artifact and flash via SMP DFU.
  Future<void> updateFirmware(BluetoothDevice device, String deviceId) async {
    _set('checking firmware…', 0, 'firmware');
    final meta = await _api.latestArtifact('firmware');
    final dl = await _api.downloadArtifact(meta['id'] as String);
    _verifySha(dl.bytes, dl.sha256);

    final tmp = await _writeTemp('fw_${meta['version']}.bin', dl.bytes);
    _set('flashing firmware ${meta['version']}…', 0.1, 'firmware');
    final smp = SmpClient(device);
    await smp.uploadImage(tmp.path, onProgress: (sent, total) {
      _set('flashing firmware…', total == 0 ? 0 : sent / total, 'firmware');
    });
    await smp.setImagePending();
    await smp.resetDevice();
    await _api.reportAppliedVersion(deviceId, 'firmware', meta['version'] as String);
    _set('firmware update complete — device rebooting', 1.0, 'firmware');
  }

  /// Glucose model update: pull latest model artifact for [variant].
  Future<void> updateModel(
    BluetoothDevice device,
    String deviceId,
    String variant, // "wearable" | "pulse"
  ) async {
    _set('checking model…', 0, 'model');
    final meta = await _api.latestArtifact('model', variant: variant);
    final dl = await _api.downloadArtifact(meta['id'] as String);
    _verifySha(dl.bytes, dl.sha256);

    final variantCode = variant == 'pulse' ? 1 : 0;
    final version = int.tryParse('${meta['version']}'.replaceAll(RegExp(r'[^0-9]'), '')) ?? 0;

    _set('uploading model ${meta['version']}…', 0.1, 'model');
    final client = ModelTransferClient(device);
    await client.upload(
      dl.bytes,
      version: version,
      variant: variantCode,
      signatureB64: dl.signature,
      onProgress: (p) => _set('uploading model…', p, 'model'),
    );
    await _api.reportAppliedVersion(deviceId, 'model', meta['version'] as String);
    _set('model update complete', 1.0, 'model');
  }

  /// Multi-image bundle: download latest `bundle` ZIP, extract, transfer
  /// resource → model → firmware (reboot last).
  Future<void> updateBundle(BluetoothDevice device, String deviceId) async {
    _set('checking bundle…', 0, 'bundle');
    final meta = await _api.latestArtifact('bundle');
    final dl = await _api.downloadArtifact(meta['id'] as String);
    _verifySha(dl.bytes, dl.sha256);
    final bundle = await OtaBundle.fromZipBytes(dl.bytes);
    await applyBundle(device, deviceId, bundle, reportVersion: meta['version'] as String?);
  }

  /// Apply an already-parsed or local ZIP bundle.
  Future<void> applyBundle(
    BluetoothDevice device,
    String deviceId,
    OtaBundle bundle, {
    String? reportVersion,
  }) async {
    final order = bundle.transferOrder
        .where((k) => bundle.components.containsKey(k))
        .toList();
    if (order.isEmpty) {
      throw StateError('bundle has no transferable components');
    }

    final weights = <String, double>{};
    for (final k in order) {
      weights[k] = (bundle.components[k]!.length).toDouble();
    }
    final totalWeight = weights.values.fold<double>(0, (a, b) => a + b);
    var doneWeight = 0.0;

    double overall(String kind, double local) {
      final w = weights[kind] ?? 1;
      return ((doneWeight + w * local) / totalWeight).clamp(0.0, 1.0);
    }

    for (final kind in order) {
      final bytes = bundle.components[kind]!;
      switch (kind) {
        case 'resource':
          _set('uploading Resource store…', overall(kind, 0), 'resource');
          await ResourceTransferClient(device).upload(
            bytes,
            onProgress: (p) => _set('uploading Resource store…', overall(kind, p), 'resource'),
          );
          break;
        case 'model':
          final comps = bundle.componentList.firstWhere((c) => c['kind'] == 'model');
          final variantName = '${comps['variant'] ?? 'wearable'}';
          final variantCode = variantName == 'pulse' ? 1 : 0;
          final version = (comps['version'] as num?)?.toInt() ??
              int.tryParse('${bundle.version}'.replaceAll(RegExp(r'[^0-9]'), '')) ??
              0;
          final sig = comps['signature'] as String?;
          _set('uploading model…', overall(kind, 0), 'model');
          await ModelTransferClient(device).upload(
            bytes,
            version: version,
            variant: variantCode,
            signatureB64: sig,
            onProgress: (p) => _set('uploading model…', overall(kind, p), 'model'),
          );
          break;
        case 'firmware':
          final tmp = await bundle.writeTempFirmware();
          _set('flashing firmware…', overall(kind, 0), 'firmware');
          final smp = SmpClient(device);
          await smp.uploadImage(tmp.path, onProgress: (sent, total) {
            final p = total == 0 ? 0.0 : sent / total;
            _set('flashing firmware…', overall(kind, p), 'firmware');
          });
          await smp.setImagePending();
          await smp.resetDevice();
          break;
        default:
          throw StateError('unknown component kind $kind');
      }
      doneWeight += weights[kind]!;
    }

    final ver = reportVersion ?? bundle.version;
    await _api.reportAppliedVersion(deviceId, 'bundle', ver);
    _set('bundle update complete — device rebooting if firmware applied', 1.0, 'bundle');
  }

  void _verifySha(List<int> bytes, String? expectedHex) {
    if (expectedHex == null || expectedHex.isEmpty) return;
    final got = sha256.convert(bytes).toString().toLowerCase();
    if (got != expectedHex.toLowerCase()) {
      throw StateError('artifact checksum mismatch');
    }
  }

  Future<File> _writeTemp(String name, List<int> bytes) async {
    final dir = await getTemporaryDirectory();
    final f = File('${dir.path}/$name');
    await f.writeAsBytes(bytes, flush: true);
    return f;
  }
}
