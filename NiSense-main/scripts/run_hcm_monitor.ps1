# Launch HCM Monitor (apps/pc). Wrapper for backward compatibility after monorepo move.
$ErrorActionPreference = "Stop"
$Root = (Get-Item $PSScriptRoot).Parent.FullName
$AppDir = Join-Path $Root "apps\pc"
Set-Location $AppDir
$Python = Join-Path $Root ".venv\Scripts\python.exe"
if (-not (Test-Path $Python)) {
    $Python = "python"
}
& $Python hcm_monitor.py @args
