# Bootstrap a Windows dev host for the NiSense server stack.
# Ensures Docker Desktop is installed (via winget) and .env exists.
$ErrorActionPreference = "Stop"

Write-Host "== NiSense host bootstrap (Windows) =="

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Write-Host "Installing Docker Desktop via winget..."
    winget install -e --id Docker.DockerDesktop
    Write-Host "Docker Desktop installed. Start it, then re-run this script."
    exit 0
} else {
    Write-Host "Docker present: $(docker --version)"
}

$envPath = Join-Path $PSScriptRoot "..\.env"
if (-not (Test-Path $envPath)) {
    Copy-Item (Join-Path $PSScriptRoot "..\.env.example") $envPath
    Write-Host "Created .env from .env.example — edit secrets before 'up'."
}

Write-Host "Done. Next: .\scripts\dev.ps1 up"
