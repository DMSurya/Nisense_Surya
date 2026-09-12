# =============================================================================
# nrfjprog Flash Script for nRF52840
# =============================================================================
# Usage: .\flash_nrfjprog.ps1 [-HexFile <path>] [-Erase] [-SoftReset]
# =============================================================================

param(
    [string]$HexFile = "",
    [string]$BuildDir = "",
    [switch]$Erase,
    [switch]$SoftReset,
    [switch]$Verify
)

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

$ProjectRoot = (Get-Item $PSScriptRoot).Parent.Parent.FullName
$ResolvedBuildDir = if ($BuildDir) { $BuildDir } else { (Get-BuildDirName) }

# Try to find nrfjprog
$nrfjprog = $null
$searchPaths = @(
    "C:\Program Files\Nordic Semiconductor\nrf-command-line-tools\bin\nrfjprog.exe",
    "C:\Program Files (x86)\Nordic Semiconductor\nrf-command-line-tools\bin\nrfjprog.exe",
    "$env:USERPROFILE\nrf-command-line-tools\bin\nrfjprog.exe"
)

foreach ($path in $searchPaths) {
    if (Test-Path $path) {
        $nrfjprog = $path
        break
    }
}

# Also try PATH
if (-not $nrfjprog) {
    $nrfjprog = Get-Command nrfjprog -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
}

# Resolve full path for hex file
if (-not $HexFile) {
    $candidateList = @()
    $candidateList += @(Get-ChildItem -Path (Join-Path $ProjectRoot $ResolvedBuildDir) -Filter "merged_*.hex" -File -ErrorAction SilentlyContinue |
        Sort-Object Name |
        ForEach-Object { $_.FullName })
    $candidateList += Join-Path $ProjectRoot "$ResolvedBuildDir\merged.hex"
    $candidateList += Join-Path $ProjectRoot "$ResolvedBuildDir\NiSense\zephyr\zephyr.signed.hex"
    $candidateList += Join-Path $ProjectRoot "$ResolvedBuildDir\NiSense\zephyr\zephyr.hex"

    foreach ($candidate in $candidateList) {
        if (Test-Path $candidate) {
            $HexFile = $candidate
            break
        }
    }

    if (-not $HexFile -and $candidateList.Count -gt 0) {
        $HexFile = $candidateList[0]
    }
}
$HexFile = (Resolve-Path $HexFile -ErrorAction SilentlyContinue).Path
if (-not $HexFile) {
    $HexFile = Join-Path $ProjectRoot "$ResolvedBuildDir\merged.hex"
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "nrfjprog Flash Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "HEX File: $HexFile"
Write-Host ""

# Check if hex file exists
if (-not (Test-Path $HexFile)) {
    Write-Host "ERROR: HEX file not found: $HexFile" -ForegroundColor Red
    Write-Host "Searched default paths:" -ForegroundColor Yellow
    Write-Host "  - $ProjectRoot\$ResolvedBuildDir\merged_*.hex" -ForegroundColor Yellow
    Write-Host "  - $ProjectRoot\$ResolvedBuildDir\merged.hex" -ForegroundColor Yellow
    Write-Host "  - $ProjectRoot\$ResolvedBuildDir\NiSense\zephyr\zephyr.signed.hex" -ForegroundColor Yellow
    Write-Host "  - $ProjectRoot\$ResolvedBuildDir\NiSense\zephyr\zephyr.hex" -ForegroundColor Yellow
    Write-Host "Run build first (preferred): .\\scripts\\setup\\build_project.ps1" -ForegroundColor Yellow
    exit 1
}

# Check if nrfjprog exists
if (-not $nrfjprog) {
    Write-Host "ERROR: nrfjprog not found!" -ForegroundColor Red
    Write-Host "Install nRF Command Line Tools from:" -ForegroundColor Yellow
    Write-Host "https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools/Download" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Or use J-Link script instead: .\flash_jlink.ps1" -ForegroundColor Cyan
    exit 1
}

Write-Host "Using: $nrfjprog"
Write-Host ""

# Build arguments
$args = @()

if ($Erase) {
    Write-Host "Performing chip erase..." -ForegroundColor Yellow
    $args += "--chiperase"
} else {
    $args += "--sectorerase"
}

$args += "--program"
$args += "`"$HexFile`""

if ($Verify) {
    $args += "--verify"
}

$args += "--reset"

Write-Host "Flashing..." -ForegroundColor Green
Write-Host "Command: nrfjprog $($args -join ' ')" -ForegroundColor DarkGray

# Execute nrfjprog
$argString = $args -join " "
$Process = Start-Process -FilePath $nrfjprog -ArgumentList $argString -NoNewWindow -Wait -PassThru

if ($Process.ExitCode -eq 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Green
    Write-Host "Flash Complete!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Green
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "1. Power cycle the device (unplug/replug USB)"
    Write-Host "2. Run RTT viewer to see boot logs:"
    Write-Host "   .\start_rtt_viewer.ps1" -ForegroundColor Yellow
} else {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "Flash Failed! (Exit code: $($Process.ExitCode))" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    exit 1
}

