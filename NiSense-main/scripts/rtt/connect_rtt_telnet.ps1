# =============================================================================
# RTT Telnet Connection Helper
# =============================================================================
# Alternative to RTT Client - uses raw TCP/telnet for RTT channels
# Useful when JLinkRTTClient doesn't work properly
# =============================================================================

param(
    [ValidateSet("logs", "shell", "both", "0", "1", "b")]
    [string]$Channel = "",
    [int]$LogPort = 19020,
    [int]$ShellPort = 19021
)

$ErrorActionPreference = "Continue"

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "RTT Telnet Connection Helper" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# Auto-detect RTT ports from running GDB Server
$gdbServerProcess = Get-Process -Name "JLinkGDBServerCL" -ErrorAction SilentlyContinue
if ($gdbServerProcess) {
    try {
        $cmdLine = (Get-CimInstance Win32_Process -Filter "ProcessId = $($gdbServerProcess.Id)").CommandLine
        if ($cmdLine -match "-RTTTelnetPort\s+(\d+)") {
            $ShellPort = [int]$matches[1]
            $LogPort = $ShellPort - 1  # Logs typically on port-1
            Write-Host "Auto-detected RTT ports from GDB Server" -ForegroundColor Green
        }
    } catch {
        # Use defaults
    }
}

Write-Host "Available RTT Channels:" -ForegroundColor Yellow
Write-Host "  [0] Channel 0 (Logs):  localhost:$LogPort" -ForegroundColor Cyan
Write-Host "  [1] Channel 1 (Shell): localhost:$ShellPort" -ForegroundColor Green
Write-Host "  [b] Both channels" -ForegroundColor White
Write-Host ""

# Get user choice if not provided via parameter
if ([string]::IsNullOrEmpty($Channel)) {
    $Channel = Read-Host "Which channel? (0=logs, 1=shell, b=both)"
}

# Normalize channel selection
$Channel = $Channel.ToLower()
if ($Channel -eq "logs") { $Channel = "0" }
if ($Channel -eq "shell") { $Channel = "1" }
if ($Channel -eq "both") { $Channel = "b" }

# Function to create RTT connection script
function Start-RTTConnection {
    param([string]$Title, [int]$Port, [string]$Color)
    
    $script = @"
`$Host.UI.RawUI.WindowTitle = '$Title'
Write-Host '=== $Title ===' -ForegroundColor $Color
Write-Host 'Connecting to localhost:$Port...' -ForegroundColor Gray
Write-Host 'Press Ctrl+C to exit' -ForegroundColor Gray
Write-Host ''
try {
    `$tcp = New-Object System.Net.Sockets.TcpClient('localhost', $Port)
    `$stream = `$tcp.GetStream()
    `$buffer = New-Object byte[] 1024
    while (`$true) {
        if (`$stream.DataAvailable) {
            `$n = `$stream.Read(`$buffer, 0, 1024)
            if (`$n -gt 0) {
                Write-Host ([System.Text.Encoding]::UTF8.GetString(`$buffer, 0, `$n)) -NoNewline
            }
        }
        if ([Console]::KeyAvailable) {
            `$key = [Console]::ReadKey(`$true)
            if (`$key.Key -eq 'C' -and `$key.Modifiers -eq 'Control') { break }
            `$stream.Write([System.Text.Encoding]::UTF8.GetBytes(`$key.KeyChar), 0, 1)
        }
        Start-Sleep -Milliseconds 10
    }
} catch { Write-Host "Error: `$_" -ForegroundColor Red }
finally { if (`$tcp) { `$tcp.Close() } }
Write-Host "`nDisconnected."
"@
    
    $tempFile = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), "rtt_$Port.ps1")
    $script | Out-File -FilePath $tempFile -Encoding UTF8 -Force
    
    # Try pwsh first, fallback to powershell
    $shell = if (Get-Command "pwsh.exe" -ErrorAction SilentlyContinue) { "pwsh.exe" } else { "powershell.exe" }
    Start-Process -FilePath $shell -ArgumentList "-ExecutionPolicy", "Bypass", "-NoExit", "-File", "`"$tempFile`""
    Write-Host "Started $Title" -ForegroundColor $Color
}

switch ($Channel) {
    "0" {
        Write-Host "Connecting to Channel 0 (Logs)..." -ForegroundColor Cyan
        Start-RTTConnection -Title "RTT Logs (Ch0)" -Port $LogPort -Color "Cyan"
    }
    "1" {
        Write-Host "Connecting to Channel 1 (Shell)..." -ForegroundColor Green  
        Start-RTTConnection -Title "RTT Shell (Ch1)" -Port $ShellPort -Color "Green"
    }
    "b" {
        Write-Host "Opening both channels..." -ForegroundColor Yellow
        Start-RTTConnection -Title "RTT Logs (Ch0)" -Port $LogPort -Color "Cyan"
        Start-Sleep -Milliseconds 500
        Start-RTTConnection -Title "RTT Shell (Ch1)" -Port $ShellPort -Color "Green"
    }
    default {
        Write-Host "Opening Shell (Channel 1) by default..." -ForegroundColor Green
        Start-RTTConnection -Title "RTT Shell (Ch1)" -Port $ShellPort -Color "Green"
    }
}

Write-Host "`nConnections started. Close terminal windows when done." -ForegroundColor Gray
