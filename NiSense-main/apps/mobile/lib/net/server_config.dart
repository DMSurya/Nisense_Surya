import 'package:shared_preferences/shared_preferences.dart';

/// Persisted connection settings for the NiSense platform API.
///
/// Defaults target the DDNS domain over HTTPS. `pinnedSha256` optionally holds
/// the lowercase hex SHA-256 of the server leaf certificate's DER bytes for
/// certificate pinning (leave empty to trust the system trust store).
class ServerConfig {
  ServerConfig({
    required this.baseUrl,
    required this.oidcIssuer,
    required this.oidcClientId,
    required this.oidcRedirectUri,
    this.pinnedSha256 = '',
    this.devToken = '',
  });

  String baseUrl;
  String oidcIssuer;
  String oidcClientId;
  String oidcRedirectUri;
  String pinnedSha256;

  /// Optional dev bearer token (Pi `dev` provider) used when OIDC is not set up.
  String devToken;

  static const _kBaseUrl = 'srv_base_url';
  static const _kIssuer = 'srv_oidc_issuer';
  static const _kClientId = 'srv_oidc_client_id';
  static const _kRedirect = 'srv_oidc_redirect';
  static const _kPin = 'srv_pin_sha256';
  static const _kDevToken = 'srv_dev_token';

  static ServerConfig defaults() => ServerConfig(
        baseUrl: 'https://mpsaami.ddns.net',
        oidcIssuer: 'https://mpsaami.ddns.net/auth/realms/nisense',
        oidcClientId: 'nisense-mobile',
        oidcRedirectUri: 'com.aarms.hcm://oauthredirect',
      );

  static Future<ServerConfig> load() async {
    final p = await SharedPreferences.getInstance();
    final d = defaults();
    return ServerConfig(
      baseUrl: p.getString(_kBaseUrl) ?? d.baseUrl,
      oidcIssuer: p.getString(_kIssuer) ?? d.oidcIssuer,
      oidcClientId: p.getString(_kClientId) ?? d.oidcClientId,
      oidcRedirectUri: p.getString(_kRedirect) ?? d.oidcRedirectUri,
      pinnedSha256: p.getString(_kPin) ?? '',
      devToken: p.getString(_kDevToken) ?? '',
    );
  }

  Future<void> save() async {
    final p = await SharedPreferences.getInstance();
    await p.setString(_kBaseUrl, baseUrl);
    await p.setString(_kIssuer, oidcIssuer);
    await p.setString(_kClientId, oidcClientId);
    await p.setString(_kRedirect, oidcRedirectUri);
    await p.setString(_kPin, pinnedSha256);
    await p.setString(_kDevToken, devToken);
  }

  String get apiBase => '$baseUrl/api/v1';
}
