#!/usr/bin/env bash
# Sync apps/server to the Raspberry Pi and rebuild the prod stack.
#
#   ./scripts/deploy-pi.sh              # sync + ./scripts/dev.sh prod
#   ./scripts/deploy-pi.sh --with-web   # also sync apps/web → web-dist (needs local build)
#   ./scripts/deploy-pi.sh --migrate    # run alembic after restart
#   ./scripts/deploy-pi.sh --dry-run    # print what would sync / run
#
# Overrides (env):
#   NISENSE_PI_HOST  (default 192.168.1.115)
#   NISENSE_PI_PORT  (default 42022)
#   NISENSE_PI_USER  (default ponmadasamy)
#   NISENSE_PI_KEY   (default ~/.ssh/id_ecdsa_rasp_3b)
#   NISENSE_PI_DIR   (default ~/NiSense)  — repo root on the Pi
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"          # apps/server
REPO="$(cd "$ROOT/../.." && pwd)"               # NiSense root
HOST="${NISENSE_PI_HOST:-192.168.1.115}"
PORT="${NISENSE_PI_PORT:-42022}"
USER="${NISENSE_PI_USER:-ponmadasamy}"
KEY="${NISENSE_PI_KEY:-$HOME/.ssh/id_ecdsa_rasp_3b}"
REMOTE_ROOT="${NISENSE_PI_DIR:-~/NiSense}"

WITH_WEB=0
MIGRATE=0
DRY_RUN=0
for arg in "$@"; do
  case "$arg" in
    --with-web) WITH_WEB=1 ;;
    --migrate)  MIGRATE=1 ;;
    --dry-run)  DRY_RUN=1 ;;
    -h|--help)
      sed -n '2,16p' "$0"
      exit 0
      ;;
    *) echo "unknown arg: $arg"; exit 1 ;;
  esac
done

SSH=(ssh -i "$KEY" -p "$PORT" -o BatchMode=yes -o StrictHostKeyChecking=accept-new)
RSH="ssh -i $KEY -p $PORT -o BatchMode=yes -o StrictHostKeyChecking=accept-new"
TARGET="${USER}@${HOST}"

echo "== Deploy NiSense server → ${TARGET}:${REMOTE_ROOT}/apps/server =="

if [ ! -f "$KEY" ]; then
  echo "SSH key not found: $KEY"
  exit 1
fi

"${SSH[@]}" "$TARGET" "test -d ${REMOTE_ROOT}/apps/server" \
  || { echo "Remote path missing: ${REMOTE_ROOT}/apps/server"; exit 1; }

EXCLUDES=(
  --exclude '.env'
  --exclude '.venv'
  --exclude '__pycache__'
  --exclude '*.pyc'
  --exclude '.pytest_cache'
  --exclude '.mypy_cache'
  --exclude 'web-dist'
  --exclude 'data'
  --exclude '.git'
)

sync_server() {
  if command -v rsync >/dev/null 2>&1; then
    if [ "$DRY_RUN" = 1 ]; then
      rsync -avzn -e "$RSH" "${EXCLUDES[@]}" "$ROOT/" "$TARGET:${REMOTE_ROOT}/apps/server/"
    else
      rsync -avz -e "$RSH" "${EXCLUDES[@]}" "$ROOT/" "$TARGET:${REMOTE_ROOT}/apps/server/"
    fi
  else
    echo "rsync not found; using tar+ssh"
    if [ "$DRY_RUN" = 1 ]; then
      echo "(dry-run) would tar $ROOT → remote apps/server"
      return
    fi
    # Preserve remote .env; replace tree contents (except excluded).
    (
      cd "$ROOT"
      tar --exclude='.env' --exclude='.venv' --exclude='__pycache__' \
          --exclude='*.pyc' --exclude='.pytest_cache' --exclude='.mypy_cache' \
          --exclude='web-dist' --exclude='data' --exclude='.git' \
          -czf - .
    ) | "${SSH[@]}" "$TARGET" "mkdir -p ${REMOTE_ROOT}/apps/server && tar -xzf - -C ${REMOTE_ROOT}/apps/server"
  fi
}

sync_web() {
  local web="$REPO/apps/web"
  local dist="$web/dist"
  if [ ! -d "$dist" ] || [ -z "$(ls -A "$dist" 2>/dev/null || true)" ]; then
    echo "Building apps/web (web-dist empty)..."
    if [ "$DRY_RUN" = 1 ]; then
      echo "(dry-run) would npm ci && npm run build in apps/web"
      return
    fi
    (cd "$web" && npm ci && npm run build)
  fi
  if [ "$DRY_RUN" = 1 ]; then
    echo "(dry-run) would sync $dist → remote web-dist"
    return
  fi
  if command -v rsync >/dev/null 2>&1; then
    rsync -avz -e "$RSH" --delete "$dist/" "$TARGET:${REMOTE_ROOT}/apps/server/web-dist/"
  else
    (
      cd "$dist"
      tar -czf - .
    ) | "${SSH[@]}" "$TARGET" "rm -rf ${REMOTE_ROOT}/apps/server/web-dist && mkdir -p ${REMOTE_ROOT}/apps/server/web-dist && tar -xzf - -C ${REMOTE_ROOT}/apps/server/web-dist"
  fi
}

sync_server
if [ "$WITH_WEB" = 1 ]; then
  sync_web
fi

# Strip CRLF if a Windows checkout synced shebang files with \r.
REMOTE_CMDS="cd ${REMOTE_ROOT}/apps/server && sed -i 's/\r$//' scripts/*.sh scripts/nisense-sign 2>/dev/null || true && chmod +x scripts/*.sh scripts/nisense-sign 2>/dev/null || true && ./scripts/dev.sh prod"
if [ "$MIGRATE" = 1 ]; then
  REMOTE_CMDS="$REMOTE_CMDS && ./scripts/dev.sh migrate"
fi

echo "== Rebuild / restart prod stack on Pi =="
if [ "$DRY_RUN" = 1 ]; then
  echo "(dry-run) ssh … '$REMOTE_CMDS'"
  exit 0
fi
"${SSH[@]}" "$TARGET" "$REMOTE_CMDS"

echo "== Health check (via SSH) =="
"${SSH[@]}" "$TARGET" 'curl -sf http://127.0.0.1:8000/healthz && echo' \
  || echo "WARN: healthz not ready yet — check: ssh … 'cd ~/NiSense/apps/server && ./scripts/dev.sh logs api'"

echo "Done. Tunnel if needed:"
echo "  ssh -i $KEY -p $PORT $TARGET -L 8000:localhost:8000"
