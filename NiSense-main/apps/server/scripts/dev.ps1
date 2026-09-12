# Windows equivalent of dev.sh (wraps docker compose).
#   .\scripts\dev.ps1 up|down|logs|migrate|seed|test|shell|prod|deploy
#   deploy flags: -WithWeb -Migrate -DryRun
param(
    [string]$Cmd = "up",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Rest
)
$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$dev = @("-f", "docker-compose.yml", "-f", "docker-compose.dev.yml", "--profile", "dev")
$prod = @("-f", "docker-compose.yml", "--profile", "prod")

if (-not (Test-Path ".env")) { Copy-Item ".env.example" ".env" }

switch ($Cmd) {
    "up"      { docker compose @dev up --build }
    "down"    { docker compose @dev down }
    "logs"    { docker compose @dev logs -f }
    "migrate" { docker compose @dev exec api alembic upgrade head }
    "seed"    { docker compose @dev exec api python -c "from app.seed import seed_defaults; seed_defaults()" }
    "test"    { docker compose @dev exec api pytest -q }
    "shell"   { docker compose @dev exec api /bin/bash }
    "prod"    { docker compose @prod up -d --build }
    "deploy"  {
        $deploy = Join-Path $PSScriptRoot "deploy-pi.ps1"
        $flags = @{}
        foreach ($a in $Rest) {
            switch -Regex ($a) {
                '^--with-web$|^-WithWeb$' { $flags.WithWeb = $true }
                '^--migrate$|^-Migrate$'  { $flags.Migrate = $true }
                '^--dry-run$|^-DryRun$'   { $flags.DryRun = $true }
                default { Write-Host "unknown deploy flag: $a"; exit 1 }
            }
        }
        & $deploy @flags
    }
    default   { Write-Host "unknown: $Cmd (up|down|logs|migrate|seed|test|shell|prod|deploy)"; exit 1 }
}
