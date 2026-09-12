#!/usr/bin/env bash
# Single entrypoint wrapping docker compose for the NiSense server.
#
#   ./scripts/dev.sh up        # start dev stack (hot-reload + debugpy + mailhog)
#   ./scripts/dev.sh down       # stop
#   ./scripts/dev.sh logs [svc] # follow logs
#   ./scripts/dev.sh migrate    # alembic upgrade head (inside api container)
#   ./scripts/dev.sh seed        # seed baseline data
#   ./scripts/dev.sh test        # run pytest
#   ./scripts/dev.sh shell       # shell into api container
#   ./scripts/dev.sh prod        # start prod stack (gunicorn + caddy + keycloak)
#   ./scripts/dev.sh deploy      # sync this tree to the Pi + rebuild prod there
#                                #   flags: --with-web --migrate --dry-run
set -euo pipefail
cd "$(dirname "$0")/.."

DEV="-f docker-compose.yml -f docker-compose.dev.yml --profile dev"
PROD="-f docker-compose.yml --profile prod"

[ -f .env ] || cp .env.example .env

cmd="${1:-up}"; shift || true
case "$cmd" in
  up)      docker compose $DEV up --build "$@" ;;
  down)    docker compose $DEV down "$@" ;;
  logs)    docker compose $DEV logs -f "$@" ;;
  migrate) docker compose $DEV exec api alembic upgrade head ;;
  seed)    docker compose $DEV exec api python -c "from app.seed import seed_defaults; seed_defaults()" ;;
  test)    docker compose $DEV exec api pytest -q "$@" ;;
  shell)   docker compose $DEV exec api /bin/bash ;;
  prod)
    ./scripts/prepare-keycloak-import.sh
    if [ ! -d web-dist ] || [ -z "$(ls -A web-dist 2>/dev/null || true)" ]; then
      echo "WARN: web-dist/ empty — build apps/web and copy dist here first:"
      echo "  (cd ../web && npm ci && npm run build && rm -rf ../server/web-dist && cp -r dist ../server/web-dist)"
    fi
    docker compose $PROD up -d --build "$@"
    ;;
  deploy)  ./scripts/deploy-pi.sh "$@" ;;
  realm)   ./scripts/prepare-keycloak-import.sh ;;
  *) echo "unknown: $cmd (up|down|logs|migrate|seed|test|shell|prod|deploy|realm)"; exit 1 ;;
esac
