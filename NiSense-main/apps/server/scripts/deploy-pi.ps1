# Sync apps/server to the Raspberry Pi and rebuild the prod stack.
#
#   .\scripts\deploy-pi.ps1
#   .\scripts\deploy-pi.ps1 -WithWeb
#   .\scripts\deploy-pi.ps1 -Migrate
#   .\scripts\deploy-pi.ps1 -DryRun
#
# Overrides (env): NISENSE_PI_HOST, NISENSE_PI_PORT, NISENSE_PI_USER,
#                  NISENSE_PI_KEY, NISENSE_PI_DIR
param(
    [switch]$WithWeb,
    [switch]$Migrate,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$ServerRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$RepoRoot = (Resolve-Path (Join-Path $ServerRoot "../..")).Path

$PiHost = if ($env:NISENSE_PI_HOST) { $env:NISENSE_PI_HOST } else { "192.168.1.115" }
$Port = if ($env:NISENSE_PI_PORT) { $env:NISENSE_PI_PORT } else { "42022" }
$User = if ($env:NISENSE_PI_USER) { $env:NISENSE_PI_USER } else { "ponmadasamy" }
$Key = if ($env:NISENSE_PI_KEY) { $env:NISENSE_PI_KEY } else { Join-Path $env:USERPROFILE ".ssh\id_ecdsa_rasp_3b" }
$RemoteRoot = if ($env:NISENSE_PI_DIR) { $env:NISENSE_PI_DIR } else { "~/NiSense" }
$Target = "${User}@${PiHost}"

function Invoke-PiSsh([string]$RemoteCmd) {
    & ssh -i $Key -p $Port -o BatchMode=yes -o StrictHostKeyChecking=accept-new $Target $RemoteCmd
    if ($LASTEXITCODE -ne 0) { throw "ssh failed ($LASTEXITCODE): $RemoteCmd" }
}

function Sync-TarDir([string]$LocalDir, [string]$RemoteDir, [string[]]$ExtraExclude = @()) {
    $archive = Join-Path $env:TEMP ("nisense-deploy-" + [guid]::NewGuid().ToString("n") + ".tgz")
    $excludes = @(
        "--exclude=.env",
        "--exclude=.venv",
        "--exclude=__pycache__",
        "--exclude=.pytest_cache",
        "--exclude=.mypy_cache",
        "--exclude=web-dist",
        "--exclude=data",
        "--exclude=.git"
    ) + $ExtraExclude
    try {
        Push-Location $LocalDir
        & tar -czf $archive @excludes .
        if ($LASTEXITCODE -ne 0) { throw "tar create failed ($LASTEXITCODE)" }
        Pop-Location
        & scp -i $Key -P $Port -o BatchMode=yes -o StrictHostKeyChecking=accept-new $archive "${Target}:/tmp/nisense-deploy.tgz"
        if ($LASTEXITCODE -ne 0) { throw "scp failed ($LASTEXITCODE)" }
        Invoke-PiSsh "mkdir -p $RemoteDir && tar -xzf /tmp/nisense-deploy.tgz -C $RemoteDir && rm -f /tmp/nisense-deploy.tgz"
    } finally {
        if (Test-Path $archive) { Remove-Item -Force $archive -ErrorAction SilentlyContinue }
        if ((Get-Location).Path -ne $ServerRoot -and (Get-Location).Path -ne $RepoRoot) {
            # no-op; Pop-Location may already have run
        }
    }
}

Write-Host "== Deploy NiSense server → ${Target}:${RemoteRoot}/apps/server =="

if (-not (Test-Path $Key)) { throw "SSH key not found: $Key" }

Invoke-PiSsh "test -d ${RemoteRoot}/apps/server"

if ($DryRun) {
    Write-Host "(dry-run) would sync $ServerRoot → ${RemoteRoot}/apps/server"
} else {
    Write-Host "Syncing apps/server..."
    Sync-TarDir -LocalDir $ServerRoot -RemoteDir "${RemoteRoot}/apps/server"
}

if ($WithWeb) {
    $web = Join-Path $RepoRoot "apps\web"
    $dist = Join-Path $web "dist"
    if (-not (Test-Path $dist) -or -not (Get-ChildItem $dist -ErrorAction SilentlyContinue)) {
        Write-Host "Building apps/web..."
        if ($DryRun) {
            Write-Host "(dry-run) would npm ci && npm run build"
        } else {
            Push-Location $web
            try {
                npm ci
                if ($LASTEXITCODE -ne 0) { throw "npm ci failed" }
                npm run build
                if ($LASTEXITCODE -ne 0) { throw "npm run build failed" }
            } finally { Pop-Location }
        }
    }
    if ($DryRun) {
        Write-Host "(dry-run) would sync web/dist → remote web-dist"
    } else {
        Write-Host "Syncing web-dist..."
        Invoke-PiSsh "rm -rf ${RemoteRoot}/apps/server/web-dist && mkdir -p ${RemoteRoot}/apps/server/web-dist"
        # web-dist sync: no .env excludes needed; still ok
        $archive = Join-Path $env:TEMP ("nisense-web-" + [guid]::NewGuid().ToString("n") + ".tgz")
        try {
            Push-Location $dist
            & tar -czf $archive .
            if ($LASTEXITCODE -ne 0) { throw "web tar failed" }
            Pop-Location
            & scp -i $Key -P $Port -o BatchMode=yes $archive "${Target}:/tmp/nisense-web.tgz"
            if ($LASTEXITCODE -ne 0) { throw "web scp failed" }
            Invoke-PiSsh "tar -xzf /tmp/nisense-web.tgz -C ${RemoteRoot}/apps/server/web-dist && rm -f /tmp/nisense-web.tgz"
        } finally {
            if (Test-Path $archive) { Remove-Item -Force $archive -ErrorAction SilentlyContinue }
        }
    }
}

# Strip CRLF so shebangs work after a Windows tar sync.
$remoteCmds = "cd ${RemoteRoot}/apps/server && sed -i 's/\r`$//' scripts/*.sh scripts/nisense-sign 2>/dev/null || true && chmod +x scripts/*.sh scripts/nisense-sign 2>/dev/null || true && ./scripts/dev.sh prod"
if ($Migrate) { $remoteCmds += " && ./scripts/dev.sh migrate" }

Write-Host "== Rebuild / restart prod stack on Pi =="
if ($DryRun) {
    Write-Host "(dry-run) ssh … '$remoteCmds'"
    exit 0
}
Invoke-PiSsh $remoteCmds

Write-Host "== Health check (via SSH) =="
try {
    Invoke-PiSsh 'curl -sf http://127.0.0.1:8000/healthz && echo'
} catch {
    Write-Host "WARN: healthz not ready yet — check logs on the Pi"
}

Write-Host "Done. Tunnel if needed:"
Write-Host "  ssh -i $Key -p $Port $Target -L 8000:localhost:8000"
