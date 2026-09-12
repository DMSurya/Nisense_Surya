import 'dart:convert';
import 'dart:io';
import 'dart:math';
import 'dart:typed_data';

import '../ble/record_sync_client.dart';
import '../services/record_local_store.dart';
import '../util/app_log.dart';

class LocalTransferServerStats {
  const LocalTransferServerStats({
    required this.receivedRecords,
    required this.upToId,
    required this.receivedBytes,
  });

  final int receivedRecords;
  final int upToId;
  final int receivedBytes;
}

/// Phone-side LAN endpoint used by firmware during a Wi-Fi bulk session.
class LocalTransferServer {
  LocalTransferServer({RecordLocalStore? store})
    : _store = store ?? RecordLocalStore.instance;

  static const recordFrameHeaderSize = 16;

  final RecordLocalStore _store;
  final _random = Random.secure();

  HttpServer? _server;
  String? _deviceId;
  String? _modelPath;
  String? _resourcePath;
  String? _firmwarePath;
  String _sessionToken = '';
  int _receivedRecords = 0;
  int _upToId = 0;
  int _receivedBytes = 0;

  bool get isRunning => _server != null;
  int get port => _server?.port ?? 0;
  String get sessionToken => _sessionToken;
  LocalTransferServerStats get stats => LocalTransferServerStats(
    receivedRecords: _receivedRecords,
    upToId: _upToId,
    receivedBytes: _receivedBytes,
  );

  Future<void> start({
    required String deviceId,
    String? modelPath,
    String? resourcePath,
    String? firmwarePath,
  }) async {
    await stop();
    _deviceId = deviceId;
    _modelPath = modelPath;
    _resourcePath = resourcePath;
    _firmwarePath = firmwarePath;
    _sessionToken = _newSessionToken();
    _receivedRecords = 0;
    _upToId = 0;
    _receivedBytes = 0;
    _server = await HttpServer.bind(InternetAddress.anyIPv4, 0);
    _server!.listen(
      _handleRequest,
      onError: (Object e, StackTrace st) {
        AppLog.e('wifi-sync', 'local transfer server error', e, st);
      },
    );
    AppLog.i('wifi-sync', 'local transfer server listening on port $port');
  }

  Future<void> stop() async {
    final server = _server;
    _server = null;
    if (server != null) {
      await server.close(force: true);
    }
  }

  Future<void> _handleRequest(HttpRequest request) async {
    try {
      final path = request.uri.path;
      if (request.method == 'POST' && path == '/sync/records') {
        // Form posts carry token= in the body; Bearer is optional.
        await _handleRecordPost(request);
        return;
      }
      if (!_authorized(request)) {
        await _json(request.response, HttpStatus.unauthorized, {
          'error': 'missing_or_invalid_token',
        });
        return;
      }
      if (request.method == 'GET' && path.startsWith('/ota/')) {
        await _serveOta(request);
        return;
      }
      await _json(request.response, HttpStatus.notFound, {
        'error': 'not_found',
      });
    } catch (e, st) {
      AppLog.e('wifi-sync', 'request failed', e, st);
      try {
        await _json(request.response, HttpStatus.internalServerError, {
          'error': e.toString(),
        });
      } catch (_) {
        // Response may already be closed by a file pipe.
      }
    }
  }

  Future<void> _handleRecordPost(HttpRequest request) async {
    final raw = await _readRequestBytes(request);
    Uint8List bytes;
    try {
      bytes = _coerceRecordBytes(raw, request);
    } on FormatException catch (e) {
      await _json(request.response, HttpStatus.unauthorized, {
        'error': e.message,
      });
      return;
    }
    final records = _parseRecordFrames(bytes);
    final deviceId = _deviceId;
    if (deviceId == null || deviceId.isEmpty) {
      await _json(request.response, HttpStatus.badRequest, {
        'error': 'missing_device_id',
      });
      return;
    }
    await _store.insertAll(deviceId, records);
    for (final r in records) {
      if (r.recordId > _upToId) {
        _upToId = r.recordId;
      }
    }
    _receivedRecords += records.length;
    _receivedBytes += bytes.length;
    await _json(request.response, HttpStatus.ok, {
      'ack': _upToId,
      'up_to_id': _upToId,
      'count': records.length,
    });
  }

  Uint8List _coerceRecordBytes(Uint8List raw, HttpRequest request) {
    final contentType = request.headers.contentType?.mimeType ?? '';
    if (contentType.contains('octet-stream') ||
        (raw.isNotEmpty && raw[0] != 0x74 /* 't' of token= */)) {
      // Prefer binary when clearly not form-urlencoded.
      if (!utf8.decode(raw, allowMalformed: true).startsWith('token=')) {
        return raw;
      }
    }
    final body = utf8.decode(raw);
    if (!_authorizedForm(body)) {
      throw const FormatException('missing_or_invalid_token');
    }
    final idx = body.indexOf('frame=');
    if (idx < 0) {
      throw const FormatException('missing frame field');
    }
    final hex = body.substring(idx + 6).split('&').first.trim();
    return _hexDecode(hex);
  }

  bool _authorizedForm(String body) {
    final match = RegExp(r'(?:^|&)token=([^&]*)').firstMatch(body);
    if (match == null) {
      return false;
    }
    return Uri.decodeQueryComponent(match.group(1)!) == _sessionToken;
  }

  Uint8List _hexDecode(String hex) {
    final cleaned = hex.replaceAll(RegExp(r'[^0-9a-fA-F]'), '');
    if (cleaned.length.isOdd) {
      throw const FormatException('odd hex length');
    }
    final out = Uint8List(cleaned.length ~/ 2);
    for (var i = 0; i < out.length; i++) {
      out[i] = int.parse(cleaned.substring(i * 2, i * 2 + 2), radix: 16);
    }
    return out;
  }

  Future<void> _serveOta(HttpRequest request) async {
    final path = switch (request.uri.path) {
      '/ota/model' => _modelPath,
      '/ota/resource' => _resourcePath,
      '/ota/firmware' => _firmwarePath,
      _ => null,
    };
    if (path == null || path.isEmpty) {
      await _json(request.response, HttpStatus.notFound, {
        'error': 'not_configured',
      });
      return;
    }
    final file = File(path);
    if (!await file.exists()) {
      await _json(request.response, HttpStatus.notFound, {
        'error': 'missing_file',
      });
      return;
    }
    final length = await file.length();
    final offset =
        int.tryParse(request.uri.queryParameters['offset'] ?? '0') ?? 0;
    final want =
        int.tryParse(request.uri.queryParameters['len'] ?? '$length') ?? length;
    if (offset < 0 || offset > length) {
      await _json(request.response, HttpStatus.badRequest, {
        'error': 'bad_offset',
      });
      return;
    }
    final end = (offset + want > length) ? length : offset + want;
    final chunkLen = end - offset;
    final raf = await file.open();
    try {
      await raf.setPosition(offset);
      final chunk = await raf.read(chunkLen);
      final asHex = request.uri.queryParameters['encoding'] == 'hex';
      request.response.statusCode = HttpStatus.ok;
      if (asHex) {
        request.response.headers.contentType = ContentType.text;
        final hex = chunk
            .map((b) => b.toRadixString(16).padLeft(2, '0'))
            .join();
        request.response.write(hex);
        await request.response.close();
      } else {
        request.response.headers.contentType = ContentType.binary;
        request.response.headers.set(
          HttpHeaders.contentLengthHeader,
          chunk.length,
        );
        request.response.add(chunk);
        await request.response.close();
      }
    } finally {
      await raf.close();
    }
  }

  bool _authorized(HttpRequest request) {
    final header = request.headers.value(HttpHeaders.authorizationHeader);
    if (header == 'Bearer $_sessionToken') {
      return true;
    }
    final q = request.uri.queryParameters['token'];
    return q != null && q == _sessionToken;
  }

  Future<Uint8List> _readRequestBytes(HttpRequest request) async {
    final builder = BytesBuilder(copy: false);
    await for (final chunk in request) {
      builder.add(chunk);
    }
    return builder.takeBytes();
  }

  List<SyncedRecord> _parseRecordFrames(Uint8List bytes) {
    final records = <SyncedRecord>[];
    var offset = 0;
    while (offset < bytes.length) {
      final remaining = bytes.length - offset;
      if (remaining < recordFrameHeaderSize) {
        throw const FormatException(
          'trailing bytes shorter than record header',
        );
      }
      final view = ByteData.sublistView(bytes, offset, bytes.length);
      final recordId = view.getUint32(0, Endian.little);
      final type = view.getUint16(4, Endian.little);
      final parentId = view.getUint32(6, Endian.little);
      final measurementId = view.getUint32(10, Endian.little);
      final payloadLen = view.getUint16(14, Endian.little);
      final frameLen = recordFrameHeaderSize + payloadLen;
      if (remaining < frameLen) {
        throw FormatException('incomplete record frame $recordId');
      }
      records.add(
        SyncedRecord(
          recordId: recordId,
          type: type,
          parentId: parentId,
          measurementId: measurementId,
          payload: Uint8List.sublistView(
            bytes,
            offset + recordFrameHeaderSize,
            offset + frameLen,
          ),
        ),
      );
      offset += frameLen;
    }
    return records;
  }

  Future<void> _json(
    HttpResponse response,
    int status,
    Map<String, dynamic> body,
  ) async {
    response.statusCode = status;
    response.headers.contentType = ContentType.json;
    response.write(jsonEncode(body));
    await response.close();
  }

  String _newSessionToken() {
    final bytes = List<int>.generate(32, (_) => _random.nextInt(256));
    return base64Url.encode(bytes).replaceAll('=', '');
  }
}
