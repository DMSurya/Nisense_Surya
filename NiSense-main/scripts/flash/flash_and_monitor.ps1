<#
.SYNOPSIS
    Flash firmware and monitor RTT output with keyboard controls.
    
.DESCRIPTION
    Flashes the nRF52840 firmware using J-Link and monitors RTT output.
    
    Key Controls:
    - Ctrl+R = Reflash and restart
    - Ctrl+T = Reset target only (no reflash)
    - Ctrl+Q = Quit
    - Ctrl+L = Clear screen
    
.EXAMPLE
    .\flash_and_monitor.ps1
#>

param(
    [string]$BuildDir = "",
    [string]$JLinkPath = "C:\Program Files\SEGGER\JLink"
)

$ErrorActionPreference = "Stop"

# Load configuration if not provided
if ([string]::IsNullOrEmpty($BuildDir)) {
    $ConfigFile = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) "scripts\setup\nrf_sdk_config.json"
    $buildDirName = "build_sdk_v330"
    if (Test-Path $ConfigFile) {
        $config = Get-Content $ConfigFile -Raw | ConvertFrom-Json
        if ($config.BuildDirName) {
            $buildDirName = $config.BuildDirName
        }
    }
    $ProjectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
    $BuildDir = Join-Path $ProjectRoot $buildDirName
}

# === Color Helpers ===
function Write-Status { param($msg) Write-Host "[*] $msg" -ForegroundColor Cyan }
function Write-OK { param($msg) Write-Host "[+] $msg" -ForegroundColor Green }
function Write-Err { param($msg) Write-Host "[-] $msg" -ForegroundColor Red }
function Write-RTT { param($msg) Write-Host $msg -ForegroundColor White }
function Write-KeyHelp { 
    Write-Host ""
    Write-Host "=== Key Controls ===" -ForegroundColor Yellow
    Write-Host "  Ctrl+R = Reflash and restart" -ForegroundColor Gray
    Write-Host "  Ctrl+T = Reset target only" -ForegroundColor Gray
    Write-Host "  Ctrl+Q = Quit" -ForegroundColor Gray
    Write-Host "  Ctrl+L = Clear screen" -ForegroundColor Gray
    Write-Host "===================" -ForegroundColor Yellow
    Write-Host ""
}

# === Kill Existing Processes ===
function Stop-JLinkProcesses {
    Get-Process -Name "JLink*" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 300
}

# === Flash Firmware ===
function Invoke-Flash {
    # Try multiple possible hex file locations (sysbuild vs non-sysbuild)
    $hexCandidates = @()
    $hexCandidates += @(Get-ChildItem -Path $BuildDir -Filter "merged_*.hex" -File -ErrorAction SilentlyContinue |
        Sort-Object Name |
        ForEach-Object { $_.FullName })
    $hexCandidates += @(
        (Join-Path $BuildDir "merged.hex"),
        (Join-Path $BuildDir "NiSense\zephyr\zephyr.signed.hex"),
        (Join-Path $BuildDir "NiSense\zephyr\zephyr.hex"),
        (Join-Path $BuildDir "zephyr\zephyr.signed.hex"),
        (Join-Path $BuildDir "zephyr\zephyr.hex")
    )
    
    $hexFile = $null
    foreach ($candidate in $hexCandidates) {
        if (Test-Path $candidate) {
            $hexFile = $candidate
            break
        }
    }
    
    if (-not $hexFile) {
        Write-Err "Hex file not found. Searched:"
        foreach ($candidate in $hexCandidates) {
            Write-Err "  - $candidate"
        }
        return $false
    }
    
    Write-Status "Using hex file: $hexFile"
    
    # Check for Resource store (logos stored in external QSPI flash)
    $resourceBin = Join-Path $BuildDir "resource.bin"
    $hasResource = Test-Path $resourceBin
    if ($hasResource) {
        $resourceSizeKB = [math]::Round((Get-Item $resourceBin).Length / 1024, 1)
        Write-Status "Resource store found: $resourceSizeKB KB -> external QSPI @ 0x120F6000"
    }
    
    Write-Status "Flashing firmware..."
    
    # Create temp J-Link script for internal flash only
    # Resource store are programmed separately via direct QSPI register writes
    # (J-Link RAMCode can't handle this board's non-standard QSPI pins)
    $jlinkLines = @(
        "r",
        "h",
        "loadfile `"$hexFile`"",
        "r",
        "g",
        "exit"
    )
    $jlinkScript = $jlinkLines -join "`n"
    
    $tempScript = [System.IO.Path]::GetTempFileName()
    $jlinkScript | Out-File -FilePath $tempScript -Encoding ASCII
    
    $jlinkExe = Join-Path $JLinkPath "JLink.exe"
    $args = @("-device", "nRF52840_xxAA", "-if", "swd", "-speed", "4000", "-autoconnect", "1", "-CommandFile", $tempScript)
    
    $proc = Start-Process -FilePath $jlinkExe -ArgumentList $args -NoNewWindow -Wait -PassThru
    
    Remove-Item $tempScript -Force -ErrorAction SilentlyContinue
    
    if ($proc.ExitCode -ne 0) {
        Write-Err "Internal flash failed with exit code: $($proc.ExitCode)"
        return $false
    }
    
    Write-OK "Internal flash complete!"
    
    # Program Resource store to external QSPI via direct register writes
    if ($hasResource) {
        Write-Status "Programming Resource store to external QSPI..."
        $resourceScript = Join-Path $PSScriptRoot "flash_resource.ps1"
        if (Test-Path $resourceScript) {
            $resourceResult = & powershell.exe -ExecutionPolicy Bypass -File $resourceScript -BuildDir (Split-Path $BuildDir -Leaf) 2>&1
            $resourceResult | ForEach-Object { Write-Host $_ }
            if ($LASTEXITCODE -eq 0) {
                Write-OK "Resource store programmed to external QSPI"
            } else {
                Write-Err "Resource flash failed (non-critical - logos may use cached data)"
            }
        } else {
            Write-Err "flash_resource.ps1 not found at $resourceScript"
        }
    }
    
    return $true
}

# === Reset Target Only ===
function Invoke-Reset {
    Write-Status "Resetting target..."
    
    $jlinkScript = @"
r
g
exit
"@
    
    $tempScript = [System.IO.Path]::GetTempFileName()
    $jlinkScript | Out-File -FilePath $tempScript -Encoding ASCII
    
    $jlinkExe = Join-Path $JLinkPath "JLink.exe"
    $args = @("-device", "nRF52840_xxAA", "-if", "swd", "-speed", "4000", "-autoconnect", "1", "-CommandFile", $tempScript)
    
    $proc = Start-Process -FilePath $jlinkExe -ArgumentList $args -NoNewWindow -Wait -PassThru
    
    Remove-Item $tempScript -Force -ErrorAction SilentlyContinue
    
    if ($proc.ExitCode -eq 0) {
        Write-OK "Reset complete!"
        return $true
    } else {
        Write-Err "Reset failed"
        return $false
    }
}

# === Start GDB Server (for RTT) and Connect ===
function Start-RTTMonitor {
    param([ref]$rttProcess, [ref]$tcpClient, [ref]$netStream)
    
    Write-Status "Starting J-Link GDB Server for RTT..."
    
    $gdbServerExe = Join-Path $JLinkPath "JLinkGDBServerCL.exe"
    
    if (-not (Test-Path $gdbServerExe)) {
        Write-Err "JLinkGDBServerCL.exe not found at: $gdbServerExe"
        return $false
    }
    
    # Start GDB Server with RTT Telnet enabled
    $gdbArgs = @(
        "-select", "USB",
        "-device", "nRF52840_xxAA",
        "-if", "swd",
        "-speed", "4000",
        "-port", "2331",
        "-RTTTelnetPort", "19021",
        "-RTTSearchRanges", "0x20000000 0x800",
        "-nogui",
        "-localhostonly"
    )
    
    $rttProcess.Value = Start-Process -FilePath $gdbServerExe -ArgumentList $gdbArgs -WindowStyle Hidden -PassThru
    
    # Wait for GDB server to start
    Write-Status "Waiting for RTT connection..."
    Start-Sleep -Seconds 2
    
    # Connect to RTT telnet port
    $maxRetries = 15
    $connected = $false
    
    for ($i = 0; $i -lt $maxRetries; $i++) {
        try {
            $tcpClient.Value = New-Object System.Net.Sockets.TcpClient
            $tcpClient.Value.Connect("127.0.0.1", 19021)
            $netStream.Value = $tcpClient.Value.GetStream()
            $netStream.Value.ReadTimeout = 100
            $connected = $true
            Write-OK "RTT connected!"
            break
        } catch {
            Write-Status "Retrying RTT connection... ($($i+1)/$maxRetries)"
            Start-Sleep -Milliseconds 500
        }
    }
    
    return $connected
}

# === Stop RTT Monitor ===
function Stop-RTTMonitor {
    param($rttProcess, $tcpClient, $netStream)
    
    if ($netStream) {
        try { $netStream.Close() } catch {}
    }
    if ($tcpClient) {
        try { $tcpClient.Close() } catch {}
    }
    if ($rttProcess -and -not $rttProcess.HasExited) {
        try { $rttProcess.Kill() } catch {}
    }
}

# === Main ===
function Main {
    Clear-Host
    Write-Host "========================================" -ForegroundColor Magenta
    Write-Host "  nRF52840 Flash & RTT Monitor" -ForegroundColor Magenta
    Write-Host "========================================" -ForegroundColor Magenta
    Write-KeyHelp
    
    # Kill any existing J-Link processes
    Stop-JLinkProcesses
    
    # Initial flash
    if (-not (Invoke-Flash)) {
        Write-Err "Initial flash failed. Press any key to exit."
        $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        return
    }
    
    # Start RTT monitor
    $rttProcess = $null
    $tcpClient = $null
    $netStream = $null
    
    if (-not (Start-RTTMonitor -rttProcess ([ref]$rttProcess) -tcpClient ([ref]$tcpClient) -netStream ([ref]$netStream))) {
        Write-Err "Failed to connect RTT. Press any key to exit."
        $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
        return
    }
    
    Write-Host ""
    Write-Host "=== RTT Output ===" -ForegroundColor Green
    Write-Host ""
    
    # Buffer for reading
    $buffer = New-Object byte[] 4096
    $partialLine = ""
    
    # Main loop
    $running = $true
    while ($running) {
        # Check for key press (non-blocking)
        if ([Console]::KeyAvailable) {
            $key = [Console]::ReadKey($true)
            
            # Ctrl+Q = Quit
            if (($key.Modifiers -band [ConsoleModifiers]::Control) -and $key.Key -eq [ConsoleKey]::Q) {
                Write-Host ""
                Write-Status "Quitting..."
                $running = $false
                continue
            }
            
            # Ctrl+L = Clear screen
            if (($key.Modifiers -band [ConsoleModifiers]::Control) -and $key.Key -eq [ConsoleKey]::L) {
                Clear-Host
                Write-Host "=== RTT Output (cleared) ===" -ForegroundColor Green
                Write-Host ""
                continue
            }
            
            # Ctrl+R = Reflash
            if (($key.Modifiers -band [ConsoleModifiers]::Control) -and $key.Key -eq [ConsoleKey]::R) {
                Write-Host ""
                Write-Status "Reflashing..."
                
                # Stop RTT
                Stop-RTTMonitor $rttProcess $tcpClient $netStream
                Stop-JLinkProcesses
                
                # Flash
                if (-not (Invoke-Flash)) {
                    Write-Err "Reflash failed!"
                    continue
                }
                
                # Restart RTT
                if (-not (Start-RTTMonitor -rttProcess ([ref]$rttProcess) -tcpClient ([ref]$tcpClient) -netStream ([ref]$netStream))) {
                    Write-Err "Failed to reconnect RTT!"
                    $running = $false
                    continue
                }
                
                Write-Host ""
                Write-Host "=== RTT Output ===" -ForegroundColor Green
                Write-Host ""
                continue
            }
            
            # Ctrl+T = Reset target only
            if (($key.Modifiers -band [ConsoleModifiers]::Control) -and $key.Key -eq [ConsoleKey]::T) {
                Write-Host ""
                
                # Stop RTT first
                Stop-RTTMonitor $rttProcess $tcpClient $netStream
                Stop-JLinkProcesses
                
                # Reset target
                Invoke-Reset
                
                # Restart RTT
                if (-not (Start-RTTMonitor -rttProcess ([ref]$rttProcess) -tcpClient ([ref]$tcpClient) -netStream ([ref]$netStream))) {
                    Write-Err "Failed to reconnect RTT after reset!"
                    $running = $false
                    continue
                }
                
                Write-Host ""
                Write-Host "=== RTT Output (after reset) ===" -ForegroundColor Green
                Write-Host ""
                continue
            }
        }
        
        # Read RTT data (non-blocking with timeout)
        try {
            if ($tcpClient -and $tcpClient.Connected -and $netStream -and $netStream.DataAvailable) {
                $bytesRead = $netStream.Read($buffer, 0, $buffer.Length)
                if ($bytesRead -gt 0) {
                    $text = [System.Text.Encoding]::ASCII.GetString($buffer, 0, $bytesRead)
                    
                    # Process and print line by line with colors
                    $fullText = $partialLine + $text
                    $lines = $fullText -split "`n"
                    
                    # Last element might be partial
                    for ($i = 0; $i -lt $lines.Count - 1; $i++) {
                        $line = $lines[$i].TrimEnd("`r")
                        
                        # Color coding based on content
                        if ($line -match "\[ERR\]|error|ERROR|Error|fault|FAULT") {
                            Write-Host $line -ForegroundColor Red
                        } elseif ($line -match "\[WRN\]|warning|WARNING|Warning") {
                            Write-Host $line -ForegroundColor Yellow
                        } elseif ($line -match "\[INF\]") {
                            Write-Host $line -ForegroundColor White
                        } elseif ($line -match "\[DBG\]") {
                            Write-Host $line -ForegroundColor DarkGray
                        } elseif ($line -match "Booting|BOOT|Starting") {
                            Write-Host $line -ForegroundColor Cyan
                        } else {
                            Write-Host $line -ForegroundColor Gray
                        }
                    }
                    
                    # Keep partial line for next iteration
                    $partialLine = $lines[-1]
                }
            }
        } catch [System.IO.IOException] {
            # Timeout - normal when no data
        } catch {
            # Connection lost?
            if (-not $tcpClient.Connected) {
                Write-Err "RTT connection lost!"
                $running = $false
            }
        }
        
        # Small sleep to reduce CPU usage
        Start-Sleep -Milliseconds 10
    }
    
    # Cleanup
    Stop-RTTMonitor $rttProcess $tcpClient $netStream
    Stop-JLinkProcesses
    
    Write-OK "Goodbye!"
}

# Run main
Main

