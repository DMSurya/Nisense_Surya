# =============================================================================
# RTT Viewer Script for nRF52840 Boot Debug
# =============================================================================
# This script starts J-Link RTT Viewer to capture MCUboot and application logs
# Usage: .\start_rtt_viewer.ps1 [-Channel <0-2>] [-LogFile <path>]
# =============================================================================

param(
    [int]$Channel = 0,
    [string]$LogFile = "",
    [switch]$GDBServer
)

$JLinkRTTViewerExe = "C:\Program Files\SEGGER\JLink\JLinkRTTViewer.exe"
$JLinkRTTClientExe = "C:\Program Files\SEGGER\JLink\JLinkRTTClient.exe"
$JLinkGDBServerExe = "C:\Program Files\SEGGER\JLink\JLinkGDBServerCL.exe"
$Device = "nRF52840_xxAA"
$Interface = "SWD"
$Speed = "4000"
$RTTAddress = "auto"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RTT Viewer for MCUboot Debug" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Device: $Device"
Write-Host "Channel: $Channel"
Write-Host ""

# Check if RTT Viewer exists
if (-not (Test-Path $JLinkRTTViewerExe)) {
    Write-Host "WARNING: JLinkRTTViewer not found, trying JLinkRTTClient..." -ForegroundColor Yellow
    
    if (-not (Test-Path $JLinkRTTClientExe)) {
        Write-Host "ERROR: Neither JLinkRTTViewer nor JLinkRTTClient found!" -ForegroundColor Red
        Write-Host "Install J-Link Software from: https://www.segger.com/downloads/jlink/" -ForegroundColor Yellow
        exit 1
    }
}

Write-Host "Starting RTT connection..." -ForegroundColor Green
Write-Host "Press Ctrl+C to stop" -ForegroundColor Yellow
Write-Host ""
Write-Host "Expected MCUboot output:" -ForegroundColor Cyan
Write-Host "  [INF] Starting bootloader"
Write-Host "  [INF] Primary image: magic=unset, swap_type=0x1, copy_done=0x3, image_ok=0x3"
Write-Host "  [INF] Scratch: magic=unset, swap_type=0x1, copy_done=0x3, image_ok=0x3"
Write-Host "  [INF] Jumping to the first image slot"
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan

if ($GDBServer) {
    # Start GDB Server with RTT enabled, then connect RTT Client
    Write-Host "Starting GDB Server with RTT..." -ForegroundColor Green
    
    # Start GDB Server in background
    $gdbProcess = Start-Process -FilePath $JLinkGDBServerExe -ArgumentList @(
        "-select", "USB",
        "-device", $Device,
        "-if", $Interface,
        "-speed", $Speed,
        "-port", "2331",
        "-RTTTelnetPort", "19021",
        "-nogui",
        "-localhostonly"
    ) -PassThru -WindowStyle Minimized
    
    Write-Host "GDB Server started (PID: $($gdbProcess.Id))" -ForegroundColor DarkGray
    Write-Host "Waiting for GDB Server to initialize..." -ForegroundColor DarkGray
    Start-Sleep -Seconds 2
    
    # Connect RTT Client
    Write-Host "Connecting RTT Client..." -ForegroundColor Green
    & $JLinkRTTClientExe
    
    # Cleanup GDB Server when done
    if (-not $gdbProcess.HasExited) {
        Stop-Process -Id $gdbProcess.Id -Force -ErrorAction SilentlyContinue
    }
} else {
    # Use RTT Viewer directly (preferred - has GUI)
    if (Test-Path $JLinkRTTViewerExe) {
        $args = @(
            "--device", $Device,
            "--if", $Interface,
            "--speed", $Speed,
            "--rttaddr", $RTTAddress
        )
        
        if ($LogFile) {
            $args += "--logfile"
            $args += $LogFile
        }
        
        & $JLinkRTTViewerExe @args
    } else {
        # Fallback to RTT Client with telnet
        Write-Host "Using RTT Client (text mode)..." -ForegroundColor Yellow
        Write-Host "Note: This requires a GDB Server or debug session running" -ForegroundColor Yellow
        Write-Host ""
        
        # Try direct connection first
        & $JLinkRTTClientExe -RTTTelnetPort 19021
    }
}

