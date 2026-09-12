import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:archive/archive.dart';
import 'package:crypto/crypto.dart';
import 'package:path/path.dart' as p;
import 'package:path_provider/path_provider.dart';

/// Parsed nisense-ota-v1 ZIP (manifest + component bytes).
class OtaBundle {
  OtaBundle({
    required this.version,
    required this.manifest,
    required this.components,
  });

  final String version;
  final Map<String, dynamic> manifest;
  final Map<String, Uint8List> components; // kind -> bytes

  Map<String, dynamic>? get requires =>
      manifest['requires'] is Map ? Map<String, dynamic>.from(manifest['requires'] as Map) : null;

  Map<String, dynamic>? get provides =>
      manifest['provides'] is Map ? Map<String, dynamic>.from(manifest['provides'] as Map) : null;

  List<Map<String, dynamic>> get componentList {
    final raw = manifest['components'];
    if (raw is! List) return const [];
    return raw.map((e) => Map<String, dynamic>.from(e as Map)).toList();
  }

  /// Preferred transfer order from manifest, falling back to resource → model → firmware.
  List<String> get transferOrder {
    final raw = manifest['transfer_order'];
    if (raw is List && raw.isNotEmpty) {
      return raw.map((e) => '$e').toList();
    }
    return const ['resource', 'model', 'firmware'];
  }

  static Future<OtaBundle> fromZipBytes(List<int> zipBytes) async {
    final archive = ZipDecoder().decodeBytes(zipBytes);
    ArchiveFile? manifestFile;
    final byName = <String, ArchiveFile>{};
    for (final f in archive.files) {
      if (!f.isFile) continue;
      final name = f.name.replaceAll('\\', '/').split('/').last;
      byName[name] = f;
      if (name == 'manifest.json') manifestFile = f;
    }
    if (manifestFile == null) {
      throw StateError('OTA ZIP missing manifest.json');
    }
    final manifest = jsonDecode(utf8.decode(manifestFile.content as List<int>))
        as Map<String, dynamic>;
    if (manifest['format'] != 'nisense-ota-v1') {
      throw StateError('Unsupported OTA format: ${manifest['format']}');
    }

    final components = <String, Uint8List>{};
    final list = manifest['components'];
    if (list is! List || list.isEmpty) {
      throw StateError('OTA manifest has no components');
    }
    for (final raw in list) {
      final c = Map<String, dynamic>.from(raw as Map);
      final kind = c['kind'] as String?;
      final file = c['file'] as String?;
      final wantSha = (c['sha256'] as String?)?.toLowerCase();
      if (kind == null || file == null) {
        throw StateError('component missing kind/file');
      }
      final af = byName[file.split('/').last];
      if (af == null) {
        throw StateError('OTA ZIP missing file $file');
      }
      final bytes = Uint8List.fromList(af.content as List<int>);
      if (wantSha != null && wantSha.isNotEmpty) {
        final got = sha256.convert(bytes).toString().toLowerCase();
        if (got != wantSha) {
          throw StateError('SHA mismatch for $file');
        }
      }
      components[kind] = bytes;
    }

    return OtaBundle(
      version: '${manifest['version'] ?? ''}',
      manifest: manifest,
      components: components,
    );
  }

  static Future<OtaBundle> fromZipFile(File zip) async =>
      fromZipBytes(await zip.readAsBytes());

  Future<File> writeTempFirmware() async {
    final bytes = components['firmware'];
    if (bytes == null) throw StateError('bundle has no firmware');
    final dir = await getTemporaryDirectory();
    final f = File(p.join(dir.path, 'fw_${version.replaceAll(RegExp(r"[^A-Za-z0-9._-]"), "_")}.bin'));
    await f.writeAsBytes(bytes, flush: true);
    return f;
  }
}
