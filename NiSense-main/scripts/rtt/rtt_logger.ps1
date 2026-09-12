<#
.SYNOPSIS
    RTT Logger for nRF52840 - Handles Power Cycles
    
.DESCRIPTION
    Uses JLinkRTTLogger which is specifically designed to handle target resets
    and power cycles. Automatically reconnects when the target comes back.

.PARAMETER LogFile
    Optional. Save RTT output to a file in addition to console.

.EXAMPLE
    .\rtt_logger.ps1
    # Monitor with auto-reconnect on power cycle

.EXAMPLE
    .\rtt_logger.ps1 -LogFile "session.log"
    # Monitor and save to file

.NOTES
    This script survives power cycles better than the telnet-based approach.
    Press Ctrl+C to stop.
#>

param(
    [string]$LogFile = ""
)

$JLinkPath = "C:\Program Files\SEGGER\JLink"
$Device = "nRF52840_xxAA"
$RTTChannel = 0

# Header
Write-Host @"

================================================================================
              nRF52840 RTT Logger - Power Cycle Resilient
================================================================================
  Device: $Device | RTT Channel: $RTTChannel
  This logger handles power cycles automatically
  Press Ctrl+C to stop
================================================================================

"@ -ForegroundColor Cyan

# Build arguments for JLinkRTTLogger
$args = @(
    "-Device", $Device,
    "-if", "SWD",
    "-Speed", "4000",
    "-RTTChannel", $RTTChannel,
    "-RTTSearchRanges", "0x20000000 0x40000"  # nRF52840 RAM range
)

# If log file specified, use it; otherwise output to console
if ($LogFile) {
    $logPath = if ([System.IO.Path]::IsPathRooted($LogFile)) { $LogFile } else { Join-Path (Get-Location) $LogFile }
    Write-Host "[*] Logging to: $logPath" -ForegroundColor Yellow
    $args += $logPath
} else {
    # Use a temp file and tail it for console output
    $tempLog = [System.IO.Path]::GetTempFileName()
    $args += $tempLog
}

Write-Host "[*] Starting J-Link RTT Logger..." -ForegroundColor Yellow
Write-Host "[*] Power cycle OK - will auto-reconnect" -ForegroundColor DarkYellow
Write-Host ("-" * 80) -ForegroundColor DarkGray

try {
    if ($LogFile) {
        # Direct logging mode - just run the logger
        & "$JLinkPath\JLinkRTTLogger.exe" @args
    } else {
        # Console mode - run logger in background and tail the output
        $logger = Start-Process -FilePath "$JLinkPath\JLinkRTTLogger.exe" -ArgumentList $args -PassThru -WindowStyle Hidden
        
        # Wait for temp file to be created
        Start-Sleep -Seconds 2
        
        # Tail the temp file
        $lastPos = 0
        while (-not $logger.HasExited) {
            if (Test-Path $tempLog) {
                $content = Get-Content $tempLog -Raw -ErrorAction SilentlyContinue
                if ($content -and $content.Length -gt $lastPos) {
                    $newContent = $content.Substring($lastPos)
                    $lastPos = $content.Length
                    
                    # Color-code output
                    $lines = $newContent -split "`n"
                    foreach ($line in $lines) {
                        if (-not $line.Trim()) { continue }
                        
                        $color = "White"
                        if ($line -match "<err>|ERROR|FATAL") { $color = "Red" }
                        elseif ($line -match "<wrn>|WARNING") { $color = "Yellow" }
                        elseif ($line -match "<inf>|Booting|initialized") { $color = "Cyan" }
                        elseif ($line -match "<dbg>") { $color = "DarkGray" }
                        
                        Write-Host $line -ForegroundColor $color
                    }
                }
            }
            Start-Sleep -Milliseconds 50
        }
    }
} catch {
    Write-Host "[-] Error: $_" -ForegroundColor Red
} finally {
    # Cleanup
    if ($logger -and -not $logger.HasExited) {
        $logger.Kill()
    }
    if ($tempLog -and (Test-Path $tempLog)) {
        Remove-Item $tempLog -Force -ErrorAction SilentlyContinue
    }
    Stop-Process -Name "JLinkRTTLogger*" -Force -ErrorAction SilentlyContinue
    Write-Host "`n[+] Stopped." -ForegroundColor Green
}

