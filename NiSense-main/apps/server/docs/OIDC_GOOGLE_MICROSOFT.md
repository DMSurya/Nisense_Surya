# Google & Microsoft IdP setup (Keycloak)

Keycloak sits at `https://mpsaami.ddns.net/auth` (Caddy → Keycloak with
`KC_HTTP_RELATIVE_PATH=/auth`). Realm: `nisense`.

## Google

1. [Google Cloud Console](https://console.cloud.google.com/) → APIs & Services → Credentials.
2. Create **OAuth 2.0 Client ID** (Web application).
3. Authorized redirect URI:
   ```
   https://mpsaami.ddns.net/auth/realms/nisense/broker/google/endpoint
   ```
4. Put values in `apps/server/.env`:
   ```
   GOOGLE_OIDC_CLIENT_ID=....apps.googleusercontent.com
   GOOGLE_OIDC_CLIENT_SECRET=....
   ```
5. `./scripts/dev.sh realm` then recreate Keycloak (`docker compose ... up -d --force-recreate keycloak`).

## Microsoft (Entra ID / Azure AD)

1. Azure Portal → App registrations → New registration.
2. Redirect URI (Web):
   ```
   https://mpsaami.ddns.net/auth/realms/nisense/broker/microsoft/endpoint
   ```
3. Create a client secret under Certificates & secrets.
4. In `.env`:
   ```
   MICROSOFT_OIDC_CLIENT_ID=<application (client) id>
   MICROSOFT_OIDC_CLIENT_SECRET=<secret value>
   MICROSOFT_OIDC_TENANT_ID=common   # or your tenant GUID
   ```
5. Render + recreate Keycloak as above.

## After IdPs are enabled

1. Assign realm roles to federated users (Keycloak Admin → Users → Role mapping).
2. Set API: `NISENSE_OIDC_PROVIDER=keycloak`.
3. Web SSO uses client `nisense-web` (PKCE); mobile uses `nisense-mobile`.

## OTP

TOTP is registered as required action `CONFIGURE_TOTP`. Enable per-user or as a
default required action under Authentication → Required actions.
