import 'dart:convert';

import 'package:flutter/foundation.dart';
import 'package:flutter_appauth/flutter_appauth.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';

import '../net/server_config.dart';

/// OIDC (Authorization Code + PKCE) login against Keycloak/Cognito/Azure AD B2C,
/// with a dev-token fallback for the Raspberry Pi `dev` provider.
///
/// Tokens are kept in secure storage; roles are decoded from the access token so
/// the UI can gate features (device_engineer, doctor, ...). Exposes a plain
/// bearer accessor for the API client.
class AuthService extends ChangeNotifier {
  AuthService(this._config);

  final ServerConfig _config;
  final _appAuth = const FlutterAppAuth();
  final _storage = const FlutterSecureStorage();

  String? _accessToken;
  String? _refreshToken;
  DateTime? _expiry;
  List<String> _roles = const [];
  String? _username;

  bool get isAuthenticated => (_accessToken != null && _accessToken!.isNotEmpty) ||
      _config.devToken.isNotEmpty;
  List<String> get roles => _roles;
  String? get username => _username;

  bool hasAnyRole(List<String> want) =>
      _roles.contains('super_admin') || want.any(_roles.contains);

  /// Current bearer token (dev token wins when configured for the Pi test box).
  Future<String?> bearer() async {
    if (_config.devToken.isNotEmpty) return _config.devToken;
    if (_accessToken == null) {
      await _restore();
    }
    if (_expiry != null && DateTime.now().isAfter(_expiry!)) {
      await _refresh();
    }
    return _accessToken;
  }

  Future<void> _restore() async {
    _accessToken = await _storage.read(key: 'access_token');
    _refreshToken = await _storage.read(key: 'refresh_token');
    if (_accessToken != null) _applyClaims(_accessToken!);
  }

  Future<void> login() async {
    final result = await _appAuth.authorizeAndExchangeCode(
      AuthorizationTokenRequest(
        _config.oidcClientId,
        _config.oidcRedirectUri,
        issuer: _config.oidcIssuer,
        scopes: const ['openid', 'profile', 'email', 'offline_access'],
        promptValues: const ['login'],
      ),
    );
    await _store(result.accessToken, result.refreshToken,
        result.accessTokenExpirationDateTime);
    notifyListeners();
  }

  Future<void> _refresh() async {
    if (_refreshToken == null) return;
    try {
      final result = await _appAuth.token(TokenRequest(
        _config.oidcClientId,
        _config.oidcRedirectUri,
        issuer: _config.oidcIssuer,
        refreshToken: _refreshToken,
        scopes: const ['openid', 'profile', 'email', 'offline_access'],
      ));
      await _store(result.accessToken, result.refreshToken,
          result.accessTokenExpirationDateTime);
    } catch (_) {
      await logout();
    }
  }

  Future<void> _store(String? access, String? refresh, DateTime? exp) async {
    _accessToken = access;
    _refreshToken = refresh;
    _expiry = exp;
    if (access != null) {
      await _storage.write(key: 'access_token', value: access);
      _applyClaims(access);
    }
    if (refresh != null) {
      await _storage.write(key: 'refresh_token', value: refresh);
    }
  }

  void _applyClaims(String jwt) {
    try {
      final parts = jwt.split('.');
      if (parts.length < 2) return;
      final payload = jsonDecode(
        utf8.decode(base64Url.decode(base64Url.normalize(parts[1]))),
      ) as Map<String, dynamic>;
      _username = (payload['preferred_username'] ?? payload['email'] ?? payload['sub'])
          ?.toString();
      final ra = payload['realm_access'];
      final collected = <String>{};
      if (ra is Map && ra['roles'] is List) {
        collected.addAll((ra['roles'] as List).map((e) => e.toString()));
      }
      if (payload['roles'] is List) {
        collected.addAll((payload['roles'] as List).map((e) => e.toString()));
      }
      _roles = collected.toList();
    } catch (_) {
      _roles = const [];
    }
  }

  Future<void> logout() async {
    _accessToken = null;
    _refreshToken = null;
    _expiry = null;
    _roles = const [];
    _username = null;
    await _storage.deleteAll();
    notifyListeners();
  }
}
