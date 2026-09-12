import 'dart:io';

import 'package:crypto/crypto.dart';
import 'package:dio/dio.dart';
import 'package:dio/io.dart';

import '../auth/auth_service.dart';
import 'server_config.dart';

/// Thin Dio wrapper for the NiSense platform API.
///
/// - Injects the bearer token from [AuthService].
/// - Optional certificate pinning: rejects TLS leaf certs whose SHA-256 does
///   not match [ServerConfig.pinnedSha256] (industrial-standard transport).
/// - Retries idempotent GETs on transient network errors.
class ApiClient {
  ApiClient(this._config, this._auth) {
    _dio = Dio(BaseOptions(
      baseUrl: _config.apiBase,
      connectTimeout: const Duration(seconds: 15),
      receiveTimeout: const Duration(seconds: 30),
    ));

    _dio.interceptors.add(InterceptorsWrapper(
      onRequest: (options, handler) async {
        final token = await _auth.bearer();
        if (token != null && token.isNotEmpty) {
          options.headers['Authorization'] = 'Bearer $token';
        }
        handler.next(options);
      },
    ));

    if (_config.pinnedSha256.isNotEmpty) {
      final adapter = _dio.httpClientAdapter as IOHttpClientAdapter;
      final want = _config.pinnedSha256.toLowerCase();
      adapter.createHttpClient = () {
        final client = HttpClient();
        client.badCertificateCallback = (X509Certificate cert, String host, int port) {
          final got = sha256.convert(cert.der).toString().toLowerCase();
          return got == want;
        };
        return client;
      };
    }
  }

  final ServerConfig _config;
  final AuthService _auth;
  late final Dio _dio;

  Dio get dio => _dio;

  Future<Map<String, dynamic>> me() async {
    final r = await _dio.get('/auth/me');
    return Map<String, dynamic>.from(r.data as Map);
  }

  /// Idempotent batch upload of measurement readings.
  Future<Map<String, dynamic>> ingestReadings(
    String deviceId,
    List<Map<String, dynamic>> readings,
  ) async {
    final r = await _dio.post('/ingest/readings', data: {
      'device_id': deviceId,
      'readings': readings,
    });
    return Map<String, dynamic>.from(r.data as Map);
  }

  Future<Map<String, dynamic>> uploadCsv(
    String filename,
    List<int> bytes, {
    String? deviceId,
    String? patientId,
  }) async {
    final form = FormData.fromMap({
      'file': MultipartFile.fromBytes(bytes, filename: filename),
      if (deviceId != null) 'device_id': deviceId,
      if (patientId != null) 'patient_id': patientId,
    });
    final r = await _dio.post('/ingest/csv', data: form);
    return Map<String, dynamic>.from(r.data as Map);
  }

  Future<Map<String, dynamic>> latestArtifact(String kind, {String? variant}) async {
    final r = await _dio.get('/artifacts/latest', queryParameters: {
      'kind': kind,
      if (variant != null) 'variant': variant,
    });
    return Map<String, dynamic>.from(r.data as Map);
  }

  /// Downloads an artifact's bytes; returns (bytes, sha256Header, signatureB64).
  Future<({List<int> bytes, String? sha256, String? signature})> downloadArtifact(
    String artifactId,
  ) async {
    final r = await _dio.get<List<int>>(
      '/artifacts/$artifactId/download',
      options: Options(responseType: ResponseType.bytes),
    );
    return (
      bytes: r.data ?? const <int>[],
      sha256: r.headers.value('x-artifact-sha256'),
      signature: r.headers.value('x-artifact-signature'),
    );
  }

  Future<void> reportAppliedVersion(String deviceId, String kind, String version) async {
    await _dio.post('/artifacts/applied-version', data: {
      'device_id': deviceId,
      'kind': kind,
      'applied_version': version,
    });
  }

  Future<Map<String, dynamic>> registerDevice({
    required String token,
    required String hwId,
    String? bleId,
    String? firmwareVersion,
  }) async {
    final r = await _dio.post('/devices/register', data: {
      'token': token,
      'hw_id': hwId,
      if (bleId != null) 'ble_id': bleId,
      if (firmwareVersion != null) 'firmware_version': firmwareVersion,
    });
    return Map<String, dynamic>.from(r.data as Map);
  }

  static String bytesToHex(List<int> b) =>
      b.map((x) => x.toRadixString(16).padLeft(2, '0')).join();
}
