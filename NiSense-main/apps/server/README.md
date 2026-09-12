# NiSense Platform API (`apps/server`)

FastAPI + PostgreSQL/TimescaleDB backend for the NiSense Digital Health
Ecosystem. Phase 1 delivers the core foundation: auth/RBAC, device registration
& provisioning, measurement ingestion & storage, and firmware + glucose-model
distribution. The mobile app is the BLE gateway (server ⇄ mobile ⇄ device); a
WiFi/MQTT direct path is designed-for but not wired yet.

Cloud-portable by design (the Raspberry Pi is a test box, ≤10 units): DB,
object storage and identity are behind adapters selected by env, so the same
images run on the Pi or on AWS/Azure.

## Quick start (Docker-first)

```bash
# One-time host prep (installs Docker + compose)
./scripts/bootstrap-host.sh        # Linux/Pi
# .\scripts\bootstrap-host.ps1      # Windows dev

cp .env.example .env               # edit secrets
./scripts/dev.sh up                # dev stack: API (hot-reload+debugpy) + db + mailhog
```

- API docs: http://localhost:8000/docs
- Get a dev token (dev provider only):
  ```bash
  curl -s -XPOST localhost:8000/api/v1/auth/dev-token \
    -H 'content-type: application/json' \
    -d '{"username":"admin","roles":["super_admin"]}'
  ```
  Use it as `Authorization: Bearer <token>`.

Prod stack (gunicorn + Caddy TLS + Keycloak):
```bash
./scripts/dev.sh prod
```

## Deploy to the Pi (dev → 192.168.1.115)

The Pi keeps a non-git tree at `~/NiSense` and runs the **prod** compose profile.
Push local `apps/server` changes and rebuild there:

```bash
# From Linux/mac (or WSL) in apps/server:
./scripts/dev.sh deploy              # sync + docker compose --profile prod up -d --build
./scripts/dev.sh deploy --with-web   # also build/sync apps/web → web-dist
./scripts/dev.sh deploy --migrate    # alembic upgrade after restart
./scripts/dev.sh deploy --dry-run

# From Windows PowerShell:
.\scripts\dev.ps1 deploy
.\scripts\dev.ps1 deploy -WithWeb -Migrate
```

Defaults: user `ponmadasamy`, port `42022`, key `~/.ssh/id_ecdsa_rasp_3b`,
remote `~/NiSense`. Override with `NISENSE_PI_HOST`, `NISENSE_PI_PORT`,
`NISENSE_PI_USER`, `NISENSE_PI_KEY`, `NISENSE_PI_DIR`. Remote `.env`,
`web-dist/` (unless `--with-web`), and Docker volumes are preserved.

After deploy, reach the API over the SSH tunnel (port 8000 is not LAN-exposed):

```bash
ssh -i ~/.ssh/id_ecdsa_rasp_3b -p 42022 ponmadasamy@192.168.1.115 -L 8000:localhost:8000
```

## Ports & router port-forwarding (static IP + DDNS `mpsaami.ddns.net`)

Forward **only** these on the home router (router → Pi):

| Port | Service | Why |
|------|---------|-----|
| 443  | Caddy HTTPS | API + web + Keycloak + WSS + OAuth callbacks |
| 80   | Caddy HTTP | Let's Encrypt ACME (skip if using DNS-01) |
| 8883 | MQTTS | **future** (direct-WiFi); keep closed now |

Everything else is internal — reach it over the existing SSH tunnel, never the
router:

```bash
ssh -i ~/.ssh/id_ecdsa_rasp_3b -p 42022 ponmadasamy@192.168.1.115 \
    -L 8000:localhost:8000 -L 5678:localhost:5678 -L 5432:localhost:5432 -L 8025:localhost:8025
```

`8000` API · `8080` Keycloak · `5432` Postgres · `5678` debugpy · `8025` Mailhog.

## Remote debugging

1. Cursor/VS Code **Remote-SSH** into the Pi (or use local Docker Desktop).
2. `./scripts/dev.sh up` (API runs under debugpy on 5678, hot-reload on).
3. Tunnel `5678` (see above) and run the **"Attach: NiSense API"** launch config
   (`.vscode/launch.json`). Breakpoints map `apps/server` ⇄ `/app`.

## Auth / OIDC

`NISENSE_OIDC_PROVIDER` selects the identity source:

- `dev` — local HS256 tokens via `/api/v1/auth/dev-token` (Pi smoke test).
- `keycloak` — self-hosted at `https://mpsaami.ddns.net/auth` (realm `nisense`).
- `cognito` / `azuread` — managed IdPs (RS256 via JWKS); no code change.

### Keycloak + Google / Microsoft

1. Register OAuth apps and set redirect URIs:
   - Google: `https://mpsaami.ddns.net/auth/realms/nisense/broker/google/endpoint`
   - Microsoft: `https://mpsaami.ddns.net/auth/realms/nisense/broker/microsoft/endpoint`
2. Put client IDs/secrets in `.env` (`GOOGLE_OIDC_*`, `MICROSOFT_OIDC_*`).
3. `./scripts/dev.sh realm` (or `prod`) renders `keycloak/import/nisense-realm.json`
   and enables IdPs that have secrets.
4. OTP (TOTP) is available as a required action (`CONFIGURE_TOTP`); enable it for
   specific users or as a default action in the admin console.
5. Bootstrap admin user (temporary password): `admin` / `ChangeMeOnFirstLogin!`
6. Switch API: `NISENSE_OIDC_PROVIDER=keycloak` and restart.

Roles: `super_admin, clinical_trial_admin, doctor, investigator, patient,
device_engineer, ai_analyst` (from `realm_access.roles`). `super_admin` implies all.

Pi memory note: Keycloak heap defaults to `-Xms64m -Xmx256m` (`JAVA_OPTS_KC_HEAP`).
Smoke-test with `NISENSE_OIDC_PROVIDER=dev` if the Pi OOMs; enable Keycloak after.

## Model signing (Ed25519, key stays on the Pi)

```bash
./scripts/gen_model_key.sh /data/keys ../../include/glucose_model_pubkey.h you@example.com
# sign a packed model over SSH from any authorized machine:
ssh -p 42022 ponmadasamy@192.168.1.115 'apps/server/scripts/nisense-sign' \
    < glucose_model_wearable.bin > model.sig.b64
```
The private key never leaves the Pi. The API verifies the signature on upload
(`NISENSE_REQUIRE_SIGNED_MODELS=true`); the device verifies again before flipping
its A/B slot.

## Migrations

Schema is managed by Alembic in prod:
```bash
./scripts/dev.sh migrate       # alembic upgrade head
```
In `dev` the app also auto-creates tables + the TimescaleDB hypertable at startup.

## API surface (Phase 1)

- `POST /api/v1/auth/dev-token`, `GET /auth/me`
- `GET/POST/PATCH/DELETE /api/v1/users` (RBAC)
- `GET/POST/PATCH /api/v1/devices`, `POST /devices/{id}/provision-token`, `POST /devices/register`
- `GET/POST/PATCH/DELETE /api/v1/sensors`
- `GET/POST /api/v1/patients`, `POST /api/v1/mappings/device-patient`, `/device-algorithm`
- `POST /api/v1/ingest/readings` (idempotent batch on `(device_id, type, record_id)` via `reading_idempotency`), `POST /api/v1/ingest/csv`, `GET /api/v1/readings`
- `GET /api/v1/export/readings.xlsx` — multi-sheet Excel (Summary / Glucose / Vitals / Temp / PPG_Raw / Glucose_Raw);
  query filters: `device_id`, `patient_id`, `type`, `since`, `until` (auth required)
- `POST /api/v1/artifacts` (firmware | model | **bundle** ZIP upload),
  `GET /artifacts`, `/artifacts/latest`,
  `GET /artifacts/{id}/download`, `DELETE /artifacts/{id}`,
  `POST /artifacts/applied-version`

Bundle upload (dev):

```bash
TOKEN=$(curl -s -XPOST localhost:8000/api/v1/auth/dev-token \
  -H 'content-type: application/json' \
  -d '{"username":"admin","roles":["device_engineer"]}' | jq -r .access_token)

# Or: python scripts/ota/upload_artifact.py --dev-token --kind bundle ...
curl -s -XPOST localhost:8000/api/v1/artifacts \
  -H "Authorization: Bearer $TOKEN" \
  -F kind=bundle -F version=1.2.3 \
  -F file=@dist/nisense_ota_1.2.3.zip
```

Multi-image OTA guide: [`docs/build/MULTI_IMAGE_OTA.md`](../../docs/build/MULTI_IMAGE_OTA.md).

Example export:

```bash
TOKEN=…   # from /api/v1/auth/dev-token or OIDC
curl -OJ -H "Authorization: Bearer $TOKEN" \
  "http://localhost:8000/api/v1/export/readings.xlsx?since=2026-01-01T00:00:00Z"
```

End-to-end operator checklist: [`docs/guides/PLATFORM_GUIDE.md`](../../docs/guides/PLATFORM_GUIDE.md).
Web Data Browser (filters + Export button): [`apps/web/README.md`](../web/README.md).
Phased server backlog (ACL, OTA campaigns, Wi‑Fi→cloud push, MQTT decision):
[`docs/guides/SERVER_ROADMAP.md`](../../docs/guides/SERVER_ROADMAP.md).
