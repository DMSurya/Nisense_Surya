# =============================================================================
# Debug Process Killer Script
# =============================================================================
# Terminates all J-Link and Ozone debug processes for clean restart
# =============================================================================

param(
    [switch]$JLinkOnly,
    [switch]$OzoneOnly,
    [switch]$All,
    [switch]$Quiet
)

$ErrorActionPreference = "Continue"

function Write-Status {
    param([string]$Message, [string]$Color = "White")
    if (-not $Quiet) {
        Write-Host $Message -ForegroundColor $Color
    }
}

Write-Status "`n========================================" "Cyan"
Write-Status "Debug Process Terminator" "Cyan"
Write-Status "========================================`n" "Cyan"

$totalKilled = 0

# J-Link processes
if (-not $OzoneOnly) {
    $jlinkProcessNames = @(
        "JLinkGDBServerCL",
        "JLinkGDBServer",
        "JLinkRTTViewer",
        "JLinkRTTClient", 
        "JLink"
    )
    
    $jlinkProcesses = Get-Process -Name $jlinkProcessNames -ErrorAction SilentlyContinue
    
    if ($jlinkProcesses) {
        Write-Status "Terminating J-Link processes..." "Yellow"
        foreach ($proc in $jlinkProcesses) {
            Write-Status "  Killing $($proc.Name) (PID: $($proc.Id))..." "Gray"
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
            $totalKilled++
        }
        Write-Status "J-Link processes terminated." "Green"
    } else {
        Write-Status "No J-Link processes found." "Gray"
    }
}

# Ozone debugger
if (-not $JLinkOnly) {
    $ozoneProcesses = Get-Process -Name "Ozone" -ErrorAction SilentlyContinue
    
    if ($ozoneProcesses) {
        Write-Status "Terminating Ozone debugger..." "Yellow"
        foreach ($proc in $ozoneProcesses) {
            Write-Status "  Killing Ozone (PID: $($proc.Id))..." "Gray"
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
            $totalKilled++
        }
        Write-Status "Ozone terminated." "Green"
    } else {
        Write-Status "No Ozone processes found." "Gray"
    }
}

# RTT terminal windows (PowerShell windows running RTT scripts)
if ($All) {
    # Find PowerShell windows with RTT in the title
    $rttTerminals = Get-Process -Name "pwsh", "powershell" -ErrorAction SilentlyContinue | 
        Where-Object { $_.MainWindowTitle -like "*RTT*" }
    
    if ($rttTerminals) {
        Write-Status "Terminating RTT terminal windows..." "Yellow"
        foreach ($proc in $rttTerminals) {
            Write-Status "  Killing RTT terminal (PID: $($proc.Id))..." "Gray"
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
            $totalKilled++
        }
        Write-Status "RTT terminals terminated." "Green"
    }
}

# Summary
Write-Status "`n----------------------------------------" "Cyan"
if ($totalKilled -gt 0) {
    Write-Status "Total processes terminated: $totalKilled" "Green"
} else {
    Write-Status "No debug processes were running." "Gray"
}
Write-Status "Ready for fresh debug session." "Cyan"
Write-Status "----------------------------------------`n" "Cyan"
