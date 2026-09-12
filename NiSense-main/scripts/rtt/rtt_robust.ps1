<#
.SYNOPSIS
    Robust RTT Monitor - Survives Power Cycles
    
.DESCRIPTION
    Starts J-Link GDB Server and RTT Client with auto-reconnect.
    When you power cycle, the GDB server maintains connection and RTT resumes.

.NOTES
    - Keep J-Link USB connected during power cycle
    - RTT output will pause briefly during power cycle (~1-2 sec)
    - Boot messages will appear after device powers back on
#>

$JLinkPath = "C:\Program Files\SEGGER\JLink"
$Device = "nRF52840_xxAA"
$GDBPort = 2331
$RTTPort = 19021

Write-Host @"

================================================================================
           nRF52840 RTT Monitor - Power Cycle Resilient
================================================================================
  Keep J-Link USB connected during power cycles
  RTT will resume automatically after device boots
  Press Ctrl+C to stop
================================================================================

"@ -ForegroundColor Cyan

# Cleanup function
function Cleanup {
    Write-Host "`n[*] Stopping..." -ForegroundColor Yellow
    Stop-Process -Name "JLinkGDBServer*" -Force -ErrorAction SilentlyContinue
    Stop-Process -Name "JLinkRTTClient*" -Force -ErrorAction SilentlyContinue
    Write-Host "[+] Stopped." -ForegroundColor Green
}

# Register cleanup on exit
trap { Cleanup; break }

try {
    # Kill any existing processes
    Stop-Process -Name "JLinkGDBServer*" -Force -ErrorAction SilentlyContinue
    Stop-Process -Name "JLinkRTTClient*" -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500

    # Start GDB Server with RTT search range
    # Key: -RTTSearchRanges tells it where to find RTT control block after reset
    Write-Host "[*] Starting J-Link GDB Server..." -ForegroundColor Yellow
    $gdbArgs = @(
        "-select", "USB",
        "-device", $Device,
        "-if", "SWD", 
        "-speed", "4000",
        "-port", $GDBPort,
        "-RTTTelnetPort", $RTTPort,
        "-RTTSearchRanges", "0x20000000 0x40000",
        "-localhostonly",
        "-singlerun"  # Don't exit when target disconnects
    )
    
    # Start GDB server in visible window so you can see status
    Start-Process -FilePath "$JLinkPath\JLinkGDBServerCL.exe" -ArgumentList $gdbArgs -WindowStyle Minimized
    Start-Sleep -Seconds 2
    Write-Host "[+] GDB Server started" -ForegroundColor Green

    # Start RTT Client - this shows the actual RTT output
    Write-Host "[*] Starting RTT Client..." -ForegroundColor Yellow
    Write-Host "[*] RTT output will appear below. Power cycle will auto-reconnect." -ForegroundColor DarkYellow
    Write-Host ("-" * 80) -ForegroundColor DarkGray
    
    # Run RTT Client in foreground - it will display output directly
    & "$JLinkPath\JLinkRTTClient.exe" -RTTTelnetPort $RTTPort

} finally {
    Cleanup
}

