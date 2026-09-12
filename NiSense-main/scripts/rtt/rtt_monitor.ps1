<#
.SYNOPSIS
    RTT Monitor for nRF52840 Wearable Health Monitor
    
.DESCRIPTION
    Connects to the device via J-Link and displays RTT output in real-time.
    Supports log file saving and color-coded output.
    Handles power cycles and reconnects automatically.

.PARAMETER LogFile
    Optional. Save RTT output to a log file.

.PARAMETER Duration
    Optional. Stop monitoring after specified seconds (0 = continuous until Ctrl+C).

.PARAMETER AutoReconnect
    Auto-reconnect if connection is lost (e.g., after power cycle). Default: true

.EXAMPLE
    .\rtt_monitor.ps1
    # Continuous monitoring with auto-reconnect

.EXAMPLE
    .\rtt_monitor.ps1 -LogFile "session.log" -Duration 60
    # Monitor for 60 seconds and save to file

.NOTES
    Press Ctrl+C to stop monitoring.
    Handles power cycles - will reconnect automatically.
    RTT control block is searched in RAM range 0x20000000-0x20040000.
#>

param(
    [string]$LogFile = "",
    [int]$Duration = 0,
    [int]$GdbPort = 2331,
    [int]$RTTPort = 19021,
    [bool]$AutoReconnect = $true,
    [switch]$Reset
)

$JLinkPath = "C:\Program Files\SEGGER\JLink"
$Device = "nRF52840_xxAA"
# Use fixed RTT control block location to avoid attaching to stale blocks.
# Both MCUboot and app place .rtt_buff_data at 0x20000000 with _SEGGER_RTT
# inside that section (currently 0x20000410 in both images).
$RTTSearchRange = "0x20000000 0x800"

# Cleanup function
function Cleanup {
    Write-Host "`n[*] Cleaning up..." -ForegroundColor Yellow
    Stop-Process -Name "JLinkGDBServer*" -Force -ErrorAction SilentlyContinue
    if ($script:logWriter) {
        $script:logWriter.Flush()
        $script:logWriter.Dispose()
    }
    if ($script:tcp) { 
        $script:tcp.Close() 
    }
    Write-Host "[+] Done." -ForegroundColor Green
}

function Start-GdbServerWithRetry {
    param(
        [string]$ExePath,
        [object[]]$Arguments,
        [int]$Attempts = 3,
        [int]$DelayMs = 1500
    )

    for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
        $proc = Start-Process -FilePath $ExePath -ArgumentList $Arguments -PassThru -WindowStyle Hidden
        Start-Sleep -Milliseconds $DelayMs

        if (-not $proc.HasExited) {
            return $proc
        }

        Write-Host "[!] GDB Server start attempt $attempt/$Attempts failed" -ForegroundColor Yellow
        Start-Sleep -Milliseconds 500
    }

    return $null
}

function Test-TcpPortInUse {
    param([int]$Port)

    $active = [System.Net.NetworkInformation.IPGlobalProperties]::GetIPGlobalProperties().GetActiveTcpListeners()
    return $null -ne ($active | Where-Object { $_.Port -eq $Port } | Select-Object -First 1)
}

function Get-FreeTcpPort {
    param([int]$StartPort)

    for ($port = $StartPort; $port -lt ($StartPort + 200); $port++) {
        if (-not (Test-TcpPortInUse -Port $port)) {
            return $port
        }
    }

    throw "No free TCP port found from $StartPort to $($StartPort + 199)"
}

# Register Ctrl+C handler
$null = Register-EngineEvent -SourceIdentifier PowerShell.Exiting -Action { Cleanup }

try {
    # Header
    Write-Host @"

================================================================================
                    nRF52840 RTT Monitor - Health Monitor
================================================================================
    Device: $Device | RTT Port: $RTTPort
  Press Ctrl+C to stop | Reset device to see boot messages
================================================================================

"@ -ForegroundColor Cyan

        if (Test-TcpPortInUse -Port $GdbPort) {
                $newGdbPort = Get-FreeTcpPort -StartPort ($GdbPort + 1)
                Write-Host "[!] GDB port $GdbPort is busy, using $newGdbPort" -ForegroundColor Yellow
                $GdbPort = $newGdbPort
        }

        if (Test-TcpPortInUse -Port $RTTPort) {
                $newRttPort = Get-FreeTcpPort -StartPort ($RTTPort + 1)
                Write-Host "[!] RTT telnet port $RTTPort is busy, using $newRttPort" -ForegroundColor Yellow
                $RTTPort = $newRttPort
        }

    # Kill existing J-Link processes
    Write-Host "[*] Stopping existing J-Link processes..." -ForegroundColor Yellow
    Stop-Process -Name "JLinkGDBServer*" -Force -ErrorAction SilentlyContinue
    Stop-Process -Name "JLinkRTTClient*" -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500

    # Start GDB Server with RTT search range for auto-detection after power cycle
    Write-Host "[*] Starting J-Link GDB Server..." -ForegroundColor Yellow
    $gdbArgs = @(
        "-select", "USB",
        "-device", $Device,
        "-if", "SWD",
        "-speed", "4000",
        "-port", [string]$GdbPort,
        "-RTTTelnetPort", $RTTPort,
        "-RTTSearchRanges", $RTTSearchRange,  # Search for RTT control block in RAM
        "-localhostonly"
        # Note: removed -noir so GDB server can detect target after power cycle
    )
    $gdb = Start-GdbServerWithRetry -ExePath "$JLinkPath\JLinkGDBServerCL.exe" -Arguments $gdbArgs

    if (-not $gdb) {
        Write-Host "[-] Failed to start GDB Server after retries." -ForegroundColor Red
        Write-Host "    Hint: close Ozone/J-Link Commander and try again." -ForegroundColor DarkYellow
        exit 1
    }
    Write-Host "[+] GDB Server started (PID: $($gdb.Id))" -ForegroundColor Green

    # Setup log file if specified
    if ($LogFile) {
        $logPath = if ([System.IO.Path]::IsPathRooted($LogFile)) { $LogFile } else { Join-Path (Get-Location) $LogFile }
        $script:logWriter = [System.IO.StreamWriter]::new($logPath, $true)
        $script:logWriter.AutoFlush = $true
        Write-Host "[*] Logging to: $logPath" -ForegroundColor Yellow
    }

    # Connect to RTT
    Write-Host "[*] Connecting to RTT..." -ForegroundColor Yellow
    $script:tcp = New-Object System.Net.Sockets.TcpClient
    $script:tcp.Connect("localhost", $RTTPort)
    $stream = $script:tcp.GetStream()
    
    Write-Host "[+] Connected! Monitoring RTT output..." -ForegroundColor Green
    Write-Host "[*] Power cycle OK - RTT will auto-reconnect (may take 1-2 seconds)" -ForegroundColor DarkYellow
    Write-Host ("-" * 80) -ForegroundColor DarkGray

    # -Reset: trigger a device reset via J-Link so we capture boot from the start
    if ($Reset) {
        Write-Host "[*] Resetting device to capture boot log..." -ForegroundColor Yellow
        $resetScript = [System.IO.Path]::GetTempFileName() + ".jlink"
        @"
r
g
exit
"@ | Out-File -FilePath $resetScript -Encoding ASCII -Force
        $resetArgs = @("-device", "nRF52840_xxAA", "-if", "SWD", "-speed", "4000",
                       "-autoconnect", "1", "-CommandFile", $resetScript)
        $resetProc = Start-Process -FilePath "$JLinkPath\JLink.exe" -ArgumentList $resetArgs `
            -NoNewWindow -PassThru -RedirectStandardOutput "$env:TEMP\jlink_reset_out.txt"
        $resetProc.WaitForExit(5000) | Out-Null
        Remove-Item $resetScript -Force -ErrorAction SilentlyContinue
        Write-Host "[+] Device reset sent" -ForegroundColor Green
    }
    
    $buffer = New-Object byte[] 4096
    $startTime = Get-Date
    $skipHeader = $true
    $headerLines = 0
    $appLogDetected = $false
    $reconnectAttempts = 0
    $maxReconnectAttempts = 10

    while ($true) {
        # Check duration
        if ($Duration -gt 0 -and ((Get-Date) - $startTime).TotalSeconds -ge $Duration) {
            Write-Host "`n[*] Duration limit reached ($Duration seconds)" -ForegroundColor Yellow
            break
        }

        # Check GDB server - restart if needed
        if ($gdb.HasExited) {
            if ($AutoReconnect -and $reconnectAttempts -lt $maxReconnectAttempts) {
                $reconnectAttempts++
                Write-Host "`n[!] GDB Server exited - reconnecting ($reconnectAttempts/$maxReconnectAttempts)..." -ForegroundColor Yellow
                
                # Restart GDB server
                $gdb = Start-GdbServerWithRetry -ExePath "$JLinkPath\JLinkGDBServerCL.exe" -Arguments $gdbArgs
                
                if ($gdb) {
                    # Reconnect TCP
                    try {
                        if ($script:tcp) { $script:tcp.Close() }
                        $script:tcp = New-Object System.Net.Sockets.TcpClient
                        $script:tcp.Connect("localhost", $RTTPort)
                        $stream = $script:tcp.GetStream()
                        Write-Host "[+] Reconnected!" -ForegroundColor Green
                        $reconnectAttempts = 0
                        $skipHeader = $true
                        $headerLines = 0
                    } catch {
                        Write-Host "[-] TCP reconnect failed: $_" -ForegroundColor Red
                    }
                } else {
                    Write-Host "[-] GDB server restart failed" -ForegroundColor Red
                }
                continue
            } else {
                Write-Host "`n[-] GDB Server exited - giving up after $reconnectAttempts attempts" -ForegroundColor Red
                break
            }
        }

        # Check for stale connection (no data for 30 seconds while target may have reset)
        # This helps detect power cycles when TCP stays connected but RTT stops
        
        # Read RTT data
        try {
            if ($stream.DataAvailable) {
                $bytesRead = $stream.Read($buffer, 0, $buffer.Length)
                if ($bytesRead -gt 0) {
                    $text = [System.Text.Encoding]::UTF8.GetString($buffer, 0, $bytesRead)
                    
                    $lines = $text -split "`n"
                    foreach ($line in $lines) {
                        # Skip SEGGER RTT header lines
                        if ($skipHeader -and $headerLines -lt 3) {
                            if ($line -match "SEGGER|Process:") {
                                $headerLines++
                                continue
                            }
                            $skipHeader = $false
                        }
                        
                        if ($line -match "SEGGER|Process:") { continue }
                        
                        # Output line with color coding
                        $color = "White"
                        if ($line -match "<err>|ERROR|FATAL|HardFault|BusFault") { $color = "Red" }
                        elseif ($line -match "<wrn>|WARNING|WARN") { $color = "Yellow" }
                        elseif ($line -match "<inf>|Booting|Using|initialized") { $color = "Cyan" }
                        elseif ($line -match "<dbg>|DEBUG") { $color = "DarkGray" }
                        
                        if ($line.Trim()) {
                            Write-Host $line -ForegroundColor $color
                            if (-not $appLogDetected -and ($line -match "APP MAIN START|<inf> main:")) {
                                Write-Host "[+] APP log detected - runtime logging is active" -ForegroundColor Green
                                $appLogDetected = $true
                            }
                            if ($script:logWriter) {
                                $ts = (Get-Date).ToString("HH:mm:ss.fff")
                                $script:logWriter.WriteLine("[$ts] $line")
                            }
                        }
                    }
                }
            }
        } catch {
            if ($AutoReconnect) {
                Write-Host "`n[!] Stream error - will reconnect..." -ForegroundColor Yellow
                Start-Sleep -Seconds 1
            } else {
                throw $_
            }
        }
        
        Start-Sleep -Milliseconds 10
    }

} catch {
    Write-Host "[-] Error: $_" -ForegroundColor Red
} finally {
    Cleanup
}

