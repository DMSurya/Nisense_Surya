# Create J-Link flash commands file
# =============================================================================
# Generates flash_commands.jlink for J-Link Commander
# Uses merged.hex (MCUboot+app) when available, falls back to app-only
# =============================================================================

Write-Host "Creating flash commands file..."

# Get the current working directory (should be the workspace folder)
$workspaceFolder = Get-Location

# Load configuration for build directory name
$ConfigFile = Join-Path $workspaceFolder ".vscode\nrf_sdk_config.json"
function Get-BuildDirName {
    $projectRoot = (Get-Item $PSScriptRoot).Parent.Parent.FullName
    $configFile = Join-Path $projectRoot "scripts\setup\nrf_sdk_config.json"
    if (Test-Path $configFile) {
        try {
            $cfg = Get-Content $configFile -Raw | ConvertFrom-Json
            if ($cfg.BuildDirName) { return $cfg.BuildDirName }
        } catch {}
    }
    return "build_sdk_v330"
}

$buildDir = Get-BuildDirName
if (Test-Path $ConfigFile) {
    try {
        $config = Get-Content $ConfigFile -Raw | ConvertFrom-Json
        if ($config.BuildDirName) {
            $buildDir = $config.BuildDirName
        }
    } catch {
        Write-Host "Warning: Could not parse config file, using default build dir" -ForegroundColor Yellow
    }
}

# CRITICAL: Use merged image which contains MCUboot bootloader + signed app
# Using zephyr.hex alone will cause boot failure because MCUboot won't be present
$buildPath = Join-Path $workspaceFolder $buildDir
$mergedHexGenerated = @(Get-ChildItem -Path $buildPath -Filter "merged_*.hex" -File -ErrorAction SilentlyContinue |
    Sort-Object Name |
    Select-Object -First 1)
$mergedHexFile = Join-Path $workspaceFolder "$buildDir\merged.hex"
$appHexFile = Join-Path $workspaceFolder "$buildDir\NiSense\zephyr\zephyr.hex"
$flashCommandsFile = Join-Path $workspaceFolder ".vscode\flash_commands.jlink"

# Prefer merged image for full system (bootloader + app), fallback to app-only
if ($mergedHexGenerated.Count -gt 0) {
    $hexFile = $mergedHexGenerated[0].FullName
    Write-Host "Using merged image (MCUboot + signed app): $hexFile" -ForegroundColor Green
} elseif (Test-Path $mergedHexFile) {
    $hexFile = $mergedHexFile
    Write-Host "Using merged image (MCUboot + signed app): $hexFile" -ForegroundColor Green
} else {
    $hexFile = $appHexFile
    Write-Host "WARNING: merged image not found, using app-only image (will not boot without MCUboot!)" -ForegroundColor Yellow
    Write-Host "         Run 'Build NiSense' task to generate merged image with bootloader" -ForegroundColor Yellow
}

Write-Host "Workspace folder: $workspaceFolder"
Write-Host "Hex file: $hexFile"
Write-Host "Flash commands file: $flashCommandsFile"

$content = @"
r
erase
h
loadfile $hexFile
r
g
qc
"@

# Ensure the .vscode directory exists
$vsCodeDir = Join-Path $workspaceFolder ".vscode"
if (-not (Test-Path $vsCodeDir)) {
    New-Item -ItemType Directory -Path $vsCodeDir -Force
}

$content | Out-File -FilePath $flashCommandsFile -Encoding ASCII
Write-Host "Flash commands file created: $flashCommandsFile"
