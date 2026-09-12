#!/usr/bin/env bash
# Substitute IdP client secrets into the Keycloak realm import file.
# Usage (from apps/server):
#   ./scripts/render-realm.sh
# Reads .env for GOOGLE_* / MICROSOFT_* vars and writes
# keycloak/nisense-realm.rendered.json. Compose prefers the rendered file when
# present; otherwise the template (IdPs disabled) is imported.
set -euo pipefail
cd "$(dirname "$0")/.."

# Load only IdP-related vars from .env (avoid sourcing values with spaces).
if [ -f .env ]; then
  while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
      GOOGLE_OIDC_*|MICROSOFT_OIDC_*)
        key="${line%%=*}"
        val="${line#*=}"
        val="${val%\"}"; val="${val#\"}"
        export "$key=$val"
        ;;
    esac
  done < .env
fi

export GOOGLE_OIDC_CLIENT_ID="${GOOGLE_OIDC_CLIENT_ID:-}"
export GOOGLE_OIDC_CLIENT_SECRET="${GOOGLE_OIDC_CLIENT_SECRET:-}"
export MICROSOFT_OIDC_CLIENT_ID="${MICROSOFT_OIDC_CLIENT_ID:-}"
export MICROSOFT_OIDC_CLIENT_SECRET="${MICROSOFT_OIDC_CLIENT_SECRET:-}"
export MICROSOFT_OIDC_TENANT_ID="${MICROSOFT_OIDC_TENANT_ID:-common}"

export ENABLE_GOOGLE=false
export ENABLE_MICROSOFT=false
[ -n "$GOOGLE_OIDC_CLIENT_ID" ] && [ -n "$GOOGLE_OIDC_CLIENT_SECRET" ] && ENABLE_GOOGLE=true
[ -n "$MICROSOFT_OIDC_CLIENT_ID" ] && [ -n "$MICROSOFT_OIDC_CLIENT_SECRET" ] && ENABLE_MICROSOFT=true
export ENABLE_GOOGLE ENABLE_MICROSOFT

python3 - keycloak/nisense-realm.json keycloak/nisense-realm.rendered.json <<'PY'
import json, os, sys

src, dst = sys.argv[1], sys.argv[2]
with open(src, encoding="utf-8") as f:
    realm = json.load(f)

subs = {
    "${GOOGLE_OIDC_CLIENT_ID}": os.environ.get("GOOGLE_OIDC_CLIENT_ID", ""),
    "${GOOGLE_OIDC_CLIENT_SECRET}": os.environ.get("GOOGLE_OIDC_CLIENT_SECRET", ""),
    "${MICROSOFT_OIDC_CLIENT_ID}": os.environ.get("MICROSOFT_OIDC_CLIENT_ID", ""),
    "${MICROSOFT_OIDC_CLIENT_SECRET}": os.environ.get("MICROSOFT_OIDC_CLIENT_SECRET", ""),
    "${MICROSOFT_OIDC_TENANT_ID}": os.environ.get("MICROSOFT_OIDC_TENANT_ID", "common"),
}

def walk(obj):
    if isinstance(obj, dict):
        return {k: walk(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [walk(v) for v in obj]
    if isinstance(obj, str):
        for k, v in subs.items():
            obj = obj.replace(k, v)
        return obj
    return obj

realm = walk(realm)
enable_g = os.environ.get("ENABLE_GOOGLE", "false") == "true"
enable_m = os.environ.get("ENABLE_MICROSOFT", "false") == "true"
for idp in realm.get("identityProviders", []):
    if idp.get("alias") == "google":
        idp["enabled"] = enable_g
    if idp.get("alias") == "microsoft":
        idp["enabled"] = enable_m

with open(dst, "w", encoding="utf-8") as f:
    json.dump(realm, f, indent=2)
    f.write("\n")
print(f"Wrote {dst} (google={enable_g}, microsoft={enable_m})")
PY
