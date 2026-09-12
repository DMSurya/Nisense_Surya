# =============================================================================
# RTT Terminal Manager Script
# =============================================================================
# Manages RTT terminals (both log and shell channels)
# Auto-opens if not running, refreshes if running
# =============================================================================

param(
    [switch]$LogsOnly,
    [switch]$ShellOnly,
    [switch]$RefreshOnly,
    [int]$RTTTelnetPort = 19021,
    [int]$RTTLogPort = 19020
)

$ErrorActionPreference = "Continue"

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

# JLink tool paths (derived from configured JLinkPath)
$JLinkRTTClientPath = Join-Path $JLinkPath "JLinkRTTClient.exe"
$JLinkRTTViewerPath = Join-Path $JLinkPath "JLinkRTTViewer.exe"

# Check if GDB Server is running to get RTT port
$gdbServerProcess = Get-Process -Name "JLinkGDBServerCL" -ErrorAction SilentlyContinue
if ($gdbServerProcess) {
    $gdbServerCmdLine = (Get-CimInstance Win32_Process -Filter "ProcessId = $($gdbServerProcess.Id)").CommandLine
    if ($gdbServerCmdLine -match "-RTTTelnetPort\s+(\d+)") {
        $RTTTelnetPort = [int]$matches[1]
        Write-Host "Detected RTT port from GDB server: $RTTTelnetPort" -ForegroundColor Green
    }
}

# Function to create RTT terminal window
function Start-RTTTerminal {
    param(
        [string]$Title,
        [int]$Port,
        [string]$Color
    )
    
    # Create a PowerShell window with telnet to RTT
    $scriptBlock = @"
`$Host.UI.RawUI.WindowTitle = '$Title'
`$Host.UI.RawUI.ForegroundColor = '$Color'
Write-Host '=== $Title ===' -ForegroundColor $Color
Write-Host 'Connecting to RTT on port $Port...' -ForegroundColor Gray
Write-Host 'Press Ctrl+C to exit' -ForegroundColor Gray
Write-Host ''
try {
    `$tcpClient = New-Object System.Net.Sockets.TcpClient('localhost', $Port)
    `$stream = `$tcpClient.GetStream()
    `$buffer = New-Object byte[] 1024
    
    while (`$true) {
        if (`$stream.DataAvailable) {
            `$bytesRead = `$stream.Read(`$buffer, 0, 1024)
            if (`$bytesRead -gt 0) {
                `$text = [System.Text.Encoding]::UTF8.GetString(`$buffer, 0, `$bytesRead)
                Write-Host `$text -NoNewline
            }
        }
        
        # Check for user input (non-blocking)
        if ([Console]::KeyAvailable) {
            `$key = [Console]::ReadKey(`$true)
            if (`$key.Key -eq 'C' -and `$key.Modifiers -eq 'Control') {
                break
            }
            `$bytes = [System.Text.Encoding]::UTF8.GetBytes(`$key.KeyChar)
            `$stream.Write(`$bytes, 0, `$bytes.Length)
        }
        
        Start-Sleep -Milliseconds 10
    }
} catch {
    Write-Host "Error: `$_" -ForegroundColor Red
    Write-Host "Make sure J-Link GDB Server is running with RTT enabled." -ForegroundColor Yellow
} finally {
    if (`$tcpClient) { `$tcpClient.Close() }
}
Write-Host "`nConnection closed. Press any key to exit..."
`$null = `$Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')
"@
    
    # Create a temporary script file to avoid command-line length and escaping issues
    $tempScript = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), "rtt_terminal_$Port.ps1")
    
    try {
        # Write the script to a temporary file
        $scriptBlock | Out-File -FilePath $tempScript -Encoding UTF8 -Force
        
        # Try multiple methods to start the process
        $processStarted = $false
        
        # Method 1: Try pwsh.exe with ExecutionPolicy Bypass
        try {
            Start-Process -FilePath "pwsh.exe" -ArgumentList "-ExecutionPolicy", "Bypass", "-NoExit", "-File", "`"$tempScript`"" -ErrorAction Stop
            $processStarted = $true
            Write-Host "Started RTT terminal using pwsh.exe" -ForegroundColor Gray
        } catch {
            # Method 2: Try powershell.exe (Windows PowerShell 5.1)
            try {
                Start-Process -FilePath "powershell.exe" -ArgumentList "-ExecutionPolicy", "Bypass", "-NoExit", "-File", "`"$tempScript`"" -ErrorAction Stop
                $processStarted = $true
                Write-Host "Started RTT terminal using powershell.exe" -ForegroundColor Gray
            } catch {
                # Method 3: Use cmd.exe to start PowerShell
                try {
                    $cmdLine = "powershell.exe -ExecutionPolicy Bypass -NoExit -File `"$tempScript`""
                    Start-Process -FilePath "cmd.exe" -ArgumentList "/c", "start", "`"$Title`"", $cmdLine -ErrorAction Stop
                    $processStarted = $true
                    Write-Host "Started RTT terminal using cmd.exe wrapper" -ForegroundColor Gray
                } catch {
                    Write-Host "Warning: Could not start RTT terminal automatically. Error: $_" -ForegroundColor Yellow
                    Write-Host "Manual connection: Run this in a new PowerShell window:" -ForegroundColor Cyan
                    Write-Host "  pwsh.exe -ExecutionPolicy Bypass -File `"$tempScript`"" -ForegroundColor White
                }
            }
        }
        
        # Clean up temp file after a short delay (give process time to start)
        if ($processStarted) {
            Start-Sleep -Milliseconds 1000
            Remove-Item -Path $tempScript -ErrorAction SilentlyContinue
        } else {
            Write-Host "Temp script saved to: $tempScript" -ForegroundColor Gray
            Write-Host "You can manually run it to start the RTT terminal." -ForegroundColor Gray
        }
    } catch {
        Write-Host "Error creating RTT terminal script: $_" -ForegroundColor Red
        if (Test-Path $tempScript) {
            Remove-Item -Path $tempScript -ErrorAction SilentlyContinue
        }
    }
}

# Function to check if RTT terminal is already running (by checking for telnet connections)
function Test-RTTTerminalRunning {
    param([int]$Port)
    
    # Check for processes listening on the port or telnet connections
    $connections = Get-NetTCPConnection -LocalPort $Port -ErrorAction SilentlyContinue
    return ($connections.Count -gt 0)
}

# Check if JLinkRTTClient/RTTViewer is running
$rttClientProcess = Get-Process -Name "JLinkRTTClient", "JLinkRTTViewer" -ErrorAction SilentlyContinue

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "RTT Terminal Manager" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# Determine which terminals to open
$openLogs = -not $ShellOnly
$openShell = -not $LogsOnly

if (-not $LogsOnly -and -not $ShellOnly) {
    $openLogs = $true
    $openShell = $true
}

# Check if GDB Server is running
if (-not $gdbServerProcess) {
    Write-Host "Warning: J-Link GDB Server is not running." -ForegroundColor Yellow
    Write-Host "RTT terminals may not connect. Start the debugger first." -ForegroundColor Yellow
    Write-Host ""
}

# Start RTT Logs terminal (Channel 0)
if ($openLogs) {
    $logsRunning = Test-RTTTerminalRunning -Port $RTTLogPort
    
    if ($logsRunning -and $RefreshOnly) {
        Write-Host "RTT Logs terminal already running. Refreshing..." -ForegroundColor Yellow
        # Close and reopen
        Get-Process | Where-Object { $_.MainWindowTitle -like "*RTT*Logs*" } | Stop-Process -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 500
    }
    
    if (-not $logsRunning -or $RefreshOnly) {
        Write-Host "Starting RTT Logs terminal (Channel 0, Port $RTTLogPort)..." -ForegroundColor Cyan
        Start-RTTTerminal -Title "RTT Logs (Channel 0)" -Port $RTTLogPort -Color "Cyan"
        Start-Sleep -Milliseconds 300
    } else {
        Write-Host "RTT Logs terminal already running." -ForegroundColor Green
    }
}

# Start RTT Shell terminal (Channel 1)
if ($openShell) {
    $shellRunning = Test-RTTTerminalRunning -Port $RTTTelnetPort
    
    if ($shellRunning -and $RefreshOnly) {
        Write-Host "RTT Shell terminal already running. Refreshing..." -ForegroundColor Yellow
        Get-Process | Where-Object { $_.MainWindowTitle -like "*RTT*Shell*" } | Stop-Process -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 500
    }
    
    if (-not $shellRunning -or $RefreshOnly) {
        Write-Host "Starting RTT Shell terminal (Channel 1, Port $RTTTelnetPort)..." -ForegroundColor Cyan
        Start-RTTTerminal -Title "RTT Shell (Channel 1)" -Port $RTTTelnetPort -Color "Green"
        Start-Sleep -Milliseconds 300
    } else {
        Write-Host "RTT Shell terminal already running." -ForegroundColor Green
    }
}

# Also offer to start JLinkRTTClient as alternative
if ($rttClientProcess) {
    Write-Host "`nJLinkRTTClient/RTTViewer is also running (PID: $($rttClientProcess.Id -join ', '))" -ForegroundColor Gray
} else {
    Write-Host "`nNote: You can also use JLinkRTTClient for a different RTT interface." -ForegroundColor Gray
}

Write-Host "`nRTT terminals ready!" -ForegroundColor Green
Write-Host "Logs channel: localhost:$RTTLogPort" -ForegroundColor Cyan
Write-Host "Shell channel: localhost:$RTTTelnetPort" -ForegroundColor Green


