# =============================================================================
# J-Link Flash Script for nRF52840
# =============================================================================
# Usage: .\flash_jlink.ps1 [-HexFile <path>] [-BuildDir <path>] [-Erase] [-NoRun] [-Verify]
# =============================================================================

param(
    [string]$HexFile = "",
    [string]$BuildDir = "",
    [switch]$Erase,
    [switch]$Reset,
    [switch]$NoRun,
    [switch]$Verify
)

# Get project root (two levels up from scripts/flash)
$ProjectRoot = (Get-Item $PSScriptRoot).Parent.Parent.FullName

function Get-BuildDirName {
    param([string]$ProjectRoot)
    $configFile = Join-Path $ProjectRoot "scripts\setup\nrf_sdk_config.json"
    if (Test-Path $configFile) {
        try {
            $cfg = Get-Content $configFile -Raw | ConvertFrom-Json
            if ($cfg.BuildDirName) { return $cfg.BuildDirName }
        } catch {}
    }
    return "build_sdk_v330"
}

$ResolvedBuildDir = if ($BuildDir) { $BuildDir } else { (Get-BuildDirName -ProjectRoot $ProjectRoot) }

$JLinkExe = "C:\Program Files\SEGGER\JLink\JLink.exe"
$Device = "nRF52840_xxAA"
$Interface = "swd"
$Speed = "4000"

function Stop-DebugProcesses {
    $names = @("JLinkGDBServerCL", "JLinkGDBServer", "JLinkRTTClient", "JLinkGUIServer", "Ozone")
    foreach ($name in $names) {
        $procs = Get-Process -Name $name -ErrorAction SilentlyContinue
        foreach ($p in $procs) {
            Write-Host "Stopping stale debug process: $($p.ProcessName) (PID: $($p.Id))" -ForegroundColor Yellow
            Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        }
    }
}

# Auto-select a firmware hex when not explicitly provided.
# Some sysbuild layouts do not emit merged.hex; fall back to app hex artifacts.
if (-not $HexFile) {
    $mergedCandidates = @(Get-ChildItem -Path $ProjectRoot -Filter "merged_*.hex" -File -ErrorAction SilentlyContinue |
        Where-Object { $_.DirectoryName -ieq (Join-Path $ProjectRoot $ResolvedBuildDir) } |
        Sort-Object Name |
        ForEach-Object { $_.FullName })

    $candidates = @()
    $candidates += $mergedCandidates
    $candidates += (Join-Path $ProjectRoot "$ResolvedBuildDir\merged.hex")
    $candidates += (Join-Path $ProjectRoot "$ResolvedBuildDir\NiSense\zephyr\zephyr.signed.hex")
    $candidates += (Join-Path $ProjectRoot "$ResolvedBuildDir\NiSense\zephyr\zephyr.hex")

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            $HexFile = $candidate
            break
        }
    }

    if (-not $HexFile) {
        # Preserve first candidate in status output for troubleshooting.
        $HexFile = $candidates[0]
    }
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "J-Link Flash Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Device: $Device"
Write-Host "Interface: $Interface @ ${Speed}kHz"
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

# Check if J-Link exists
if (-not (Test-Path $JLinkExe)) {
    Write-Host "ERROR: J-Link not found at: $JLinkExe" -ForegroundColor Red
    exit 1
}

# Create temporary command file
$TempCmdFile = [System.IO.Path]::GetTempFileName()
$Commands = @()

# Build command sequence
$Commands += "r"  # Reset and halt
if ($Erase) {
    Write-Host "Performing chip erase..." -ForegroundColor Yellow
    $Commands += "erase"
}
$Commands += "loadfile `"$HexFile`""
if ($Verify) {
    $Commands += "verifyfile `"$HexFile`""
}
$Commands += "r"  # Reset
if (-not $NoRun) {
    $Commands += "g"  # Go (run)
}
$Commands += "exit"

# Write commands to temp file
$Commands | Out-File -FilePath $TempCmdFile -Encoding ASCII

Write-Host "Flashing..." -ForegroundColor Green

# Ensure no stale J-Link/Ozone sessions are holding the probe.
Stop-DebugProcesses

# Execute J-Link
$Process = Start-Process -FilePath $JLinkExe -ArgumentList @(
    "-device", $Device,
    "-if", $Interface,
    "-speed", $Speed,
    "-autoconnect", "1",
    "-CommandFile", $TempCmdFile
) -NoNewWindow -Wait -PassThru

if ($Process.ExitCode -ne 0) {
    Write-Host "Initial flash attempt failed, retrying at lower SWD speed (1000 kHz)..." -ForegroundColor Yellow
    Stop-DebugProcesses
    $Process = Start-Process -FilePath $JLinkExe -ArgumentList @(
        "-device", $Device,
        "-if", $Interface,
        "-speed", "1000",
        "-autoconnect", "1",
        "-CommandFile", $TempCmdFile
    ) -NoNewWindow -Wait -PassThru
}

# Cleanup
Remove-Item $TempCmdFile -Force -ErrorAction SilentlyContinue

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

