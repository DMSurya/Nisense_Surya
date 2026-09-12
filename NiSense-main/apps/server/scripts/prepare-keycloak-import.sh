#!/usr/bin/env bash
# Build keycloak/import/nisense-realm.json for compose mount.
# Runs render-realm.sh when IdP secrets are present; otherwise copies the
# template (Google/Microsoft IdPs disabled).
set -euo pipefail
cd "$(dirname "$0")/.."

mkdir -p keycloak/import
./scripts/render-realm.sh

if [ -f keycloak/nisense-realm.rendered.json ]; then
  cp keycloak/nisense-realm.rendered.json keycloak/import/nisense-realm.json
else
  cp keycloak/nisense-realm.json keycloak/import/nisense-realm.json
fi
echo "Keycloak import ready: keycloak/import/nisense-realm.json"
