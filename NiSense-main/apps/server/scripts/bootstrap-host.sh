#!/usr/bin/env bash
# Bootstrap a Linux host (Raspberry Pi / Debian / Ubuntu) to run the NiSense
# server stack. Installs Docker Engine + compose plugin + git. Docker-first:
# no apt/pip/npm packages are installed on the host beyond these.
set -euo pipefail

echo "== NiSense host bootstrap =="

if ! command -v docker >/dev/null 2>&1; then
    echo "Installing Docker Engine..."
    curl -fsSL https://get.docker.com | sh
    sudo usermod -aG docker "$USER" || true
    echo "Docker installed. Log out/in for group membership to take effect."
else
    echo "Docker already installed: $(docker --version)"
fi

if ! docker compose version >/dev/null 2>&1; then
    echo "Installing docker compose plugin..."
    sudo apt-get update
    sudo apt-get install -y docker-compose-plugin
fi

command -v git >/dev/null 2>&1 || sudo apt-get install -y git

echo "Done. Next: cp .env.example .env && ./scripts/dev.sh up"
