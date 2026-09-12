# =============================================================================
# J-Link GDB Server Start Script
# =============================================================================
# Starts J-Link GDB Server with RTT support for nRF52840 debugging
# =============================================================================

param(
    [int]$GDBPort = 2331,
    [int]$RTTPort = 19021,
    [switch]$Force
)

$ErrorActionPreference = "Continue"

# Check if GDB Server is already running
$process = Get-Process -Name "JLinkGDBServerCL" -ErrorAction SilentlyContinue

if ($process -and -not $Force) {
    Write-Host "GDB Server already running on PID: $($process.Id)" -ForegroundColor Green
    # Try to get the RTT port from command line
    try {
        $cmdLine = (Get-CimInstance Win32_Process -Filter "ProcessId = $($process.Id)").CommandLine
        if ($cmdLine -match "-RTTTelnetPort\s+(\d+)") {
            Write-Host "RTT Telnet Port: $($matches[1])" -ForegroundColor Cyan
        }
        if ($cmdLine -match "-port\s+(\d+)") {
            Write-Host "GDB Port: $($matches[1])" -ForegroundColor Cyan
        }
    } catch {
        # Ignore errors getting command line
    }
    exit 0
}

if ($process -and $Force) {
    Write-Host "Force flag set, stopping existing GDB Server..." -ForegroundColor Yellow
    $process | Stop-Process -Force
    Start-Sleep -Seconds 1
}

# Load configuration for JLink path
$ConfigFile = Join-Path $PSScriptRoot "nrf_sdk_config.json"
$JLinkPath = "C:\Program Files\SEGGER\JLink"
if (Test-Path $ConfigFile) {
    try {
        $config = Get-Content $ConfigFile -Raw | ConvertFrom-Json
        if ($config.JLinkPath) {
            $JLinkPath = $config.JLinkPath
        }
    } catch {
        # Use default if config parse fails
    }
}

Write-Host "Starting J-Link GDB Server..." -ForegroundColor Cyan
Write-Host "  Device: nRF52840_xxAA" -ForegroundColor Gray
Write-Host "  Interface: SWD @ 4000 kHz" -ForegroundColor Gray
Write-Host "  GDB Port: $GDBPort" -ForegroundColor Gray
Write-Host "  RTT Port: $RTTPort" -ForegroundColor Gray

# Find JLink installation (use config path first, then fallbacks)
$gdbServerPath = Join-Path $JLinkPath "JLinkGDBServerCL.exe"
if (-not (Test-Path $gdbServerPath)) {
    # Try alternate locations
    $altPaths = @(
        "C:\Program Files\SEGGER\JLink\JLinkGDBServerCL.exe",
        "C:\Program Files (x86)\SEGGER\JLink\JLinkGDBServerCL.exe",
        "$env:ProgramFiles\SEGGER\JLink\JLinkGDBServerCL.exe"
    )
    foreach ($path in $altPaths) {
        if (Test-Path $path) {
            $gdbServerPath = $path
            break
        }
    }
}

if (-not (Test-Path $gdbServerPath)) {
    Write-Host "Error: J-Link GDB Server not found!" -ForegroundColor Red
    Write-Host "Please install SEGGER J-Link software from https://www.segger.com/downloads/jlink/" -ForegroundColor Yellow
    exit 1
}

$arguments = @(
    "-select", "USB",
    "-device", "nRF52840_xxAA", 
    "-if", "swd",
    "-speed", "4000",
    "-port", $GDBPort,
    "-RTTTelnetPort", $RTTPort,
    "-singlerun",
    "-nogui",
    "-localhostonly"
)

Start-Process -FilePath $gdbServerPath -ArgumentList $arguments -WindowStyle Hidden
Start-Sleep -Seconds 2

# Verify it started
$newProcess = Get-Process -Name "JLinkGDBServerCL" -ErrorAction SilentlyContinue
if ($newProcess) {
    Write-Host "GDB Server started successfully (PID: $($newProcess.Id))" -ForegroundColor Green
    Write-Host ""
    Write-Host "Connect with:" -ForegroundColor Cyan
    Write-Host "  GDB: target remote localhost:$GDBPort" -ForegroundColor White
    Write-Host "  RTT: telnet localhost $RTTPort" -ForegroundColor White
} else {
    Write-Host "Warning: GDB Server may not have started correctly" -ForegroundColor Yellow
}
