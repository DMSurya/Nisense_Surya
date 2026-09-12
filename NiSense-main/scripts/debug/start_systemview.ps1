# =============================================================================
# Launch SEGGER SystemView for NiSense (nRF52840)
# =============================================================================
# SystemView needs exclusive J-Link access. Close rtt_monitor.ps1, Ozone, and
# other J-Link tools before starting.
#
# Usage: .\scripts\debug\start_systemview.ps1 [-ResetTarget]
# =============================================================================

param(
    [switch]$ResetTarget
)

$ErrorActionPreference = "Stop"

$ProjectRoot = (Get-Item $PSScriptRoot).Parent.Parent.FullName
$SvDir = Join-Path $ProjectRoot "scripts\debug\systemview"
$SvProject = Join-Path $SvDir "HCM.SVPrj"
$DescFile = Join-Path $SvDir "SYSVIEW_Zephyr.txt"
$NcsDesc = "C:\ncs\v3.3.0\zephyr\subsys\tracing\sysview\SYSVIEW_Zephyr.txt"

function Test-JLinkExclusive {
    $blockers = @(
        @{ Name = "JLinkRTTClient"; Hint = "Close rtt_monitor.ps1 / RTT Viewer" },
        @{ Name = "JLinkGDBServerCL"; Hint = "Close VS Code / Ozone debug session" },
        @{ Name = "JLinkGDBServer"; Hint = "Close active GDB server" },
        @{ Name = "JLinkSWOViewerCL"; Hint = "Close SWO Viewer" }
    )
    $found = @()
    foreach ($entry in $blockers) {
        if (Get-Process -Name $entry.Name -ErrorAction SilentlyContinue) {
            $found += "$($entry.Name) ($($entry.Hint))"
        }
    }
    return $found
}

function Ensure-SystemViewAssets {
    if (-not (Test-Path $SvDir)) {
        New-Item -ItemType Directory -Path $SvDir -Force | Out-Null
    }

    if (-not (Test-Path $DescFile)) {
        if (Test-Path $NcsDesc) {
            Copy-Item -Force $NcsDesc $DescFile
            Write-Host "Restored $DescFile from NCS SDK" -ForegroundColor DarkGray
        } else {
            Write-Host "Missing $DescFile and NCS source not found at:" -ForegroundColor Red
            Write-Host "  $NcsDesc" -ForegroundColor Red
            exit 1
        }
    }

    if (-not (Test-Path $SvProject)) {
        @"
<SystemViewProject Name="HCM">
  <Recorder Type="J-Link">
    <J-Link CMB_HOST_IF="USB" TXT_USB_SN="" TXT_IP="" TXT_TUNNEL="" TXT_TARGET_DEVICE="NRF52840_XXAA" CMB_TARGET_IF="SWD" SPN_TIF_SPEED="12000" SPN_TIF_IRPRE="-1" SPN_TIF_DRPRE="-1" SPN_NUM_CORES="1" RDG_RTT_MODE="Auto" TXT_RTT_CB_LOCATION="" TXT_RTT_CB_RANGE="" CHK_ADVANCED="0" FLE_SETTINGS="" FLE_SCRIPT="" FLE_LOG=""/>
    <IP TXT_IP="127.0.0.1" TXT_PORT="19111"/>
    <UART CMB_COM_PORT="" TXT_BAUDRATE="700000"/>
  </Recorder>
  <RecordingFiles/>
  <DescriptionsFiles>
    <File Path="SYSVIEW_Zephyr.txt"/>
  </DescriptionsFiles>
</SystemViewProject>
"@ | Set-Content -Path $SvProject -Encoding UTF8
        Write-Host "Restored $SvProject" -ForegroundColor DarkGray
    }
}

$SystemViewCandidates = @(
    "C:\Program Files\SEGGER\SystemView\SystemView.exe",
    "C:\Program Files (x86)\SEGGER\SystemView\SystemView.exe",
    "$env:ProgramFiles\SEGGER\SystemView\SystemView.exe"
)

$SystemViewExe = $null
foreach ($candidate in $SystemViewCandidates) {
    if (Test-Path $candidate) {
        $SystemViewExe = $candidate
        break
    }
}

if (-not $SystemViewExe) {
    Write-Host "SEGGER SystemView not found." -ForegroundColor Red
    Write-Host "Install from: https://www.segger.com/downloads/systemview/" -ForegroundColor Yellow
    exit 1
}

Ensure-SystemViewAssets

$jlinkBlockers = Test-JLinkExclusive
if ($jlinkBlockers.Count -gt 0) {
    Write-Host "WARNING: Another J-Link client is running:" -ForegroundColor Red
    foreach ($blocker in $jlinkBlockers) {
        Write-Host "  - $blocker" -ForegroundColor Red
    }
    Write-Host "Close it before recording or you will see RTT host buffer overflow." -ForegroundColor Yellow
    Write-Host ""
}

$jlink = "C:\Program Files\SEGGER\JLink\JLink.exe"
if (-not (Test-Path $jlink)) {
    $jlink = "C:\Program Files (x86)\SEGGER\JLink\JLink.exe"
}

if ($ResetTarget -and (Test-Path $jlink)) {
    Write-Host "Resetting nRF52840..." -ForegroundColor DarkGray
    $jlinkCmd = Join-Path $env:TEMP "nisense_sysview_reset.jlink"
    @(
        "r",
        "g",
        "exit"
    ) | Set-Content -Path $jlinkCmd -Encoding ASCII
    & $jlink -device nRF52840_xxAA -if SWD -speed 12000 -autoconnect 1 -CommandFile $jlinkCmd 2>$null | Out-Null
    Remove-Item $jlinkCmd -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "SEGGER SystemView - NiSense (HCM)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Project: $SvProject" -ForegroundColor DarkGray
Write-Host ""
Write-Host "Before recording:" -ForegroundColor Yellow
Write-Host "  1. Close rtt_monitor.ps1, Ozone, and any other J-Link tool" -ForegroundColor Yellow
Write-Host "  2. Target must be RUNNING (not halted in debugger)" -ForegroundColor Yellow
Write-Host "  3. Target -> Recorder Configuration (Alt+Enter): SWD speed 12000 kHz" -ForegroundColor Yellow
Write-Host "  4. Target -> Start Recording (F5) AFTER connect" -ForegroundColor Yellow
Write-Host "     (firmware uses BOOT_ENABLE=n - trace starts when you press Start)" -ForegroundColor Yellow
Write-Host ""
Write-Host "If you see 'RTT host buffer overflow':" -ForegroundColor Yellow
Write-Host "  - Close rtt_monitor.ps1 / Ozone / any other J-Link tool (most common)" -ForegroundColor Yellow
Write-Host "  - Target -> Recorder Configuration: set SWD speed to 12000 kHz" -ForegroundColor Yellow
Write-Host "  - Rebuild after systemview.conf tracing trim (thread timeline only)" -ForegroundColor Yellow
Write-Host "  - Use -ResetTarget, connect, press F5 quickly, then reproduce" -ForegroundColor Yellow
Write-Host ""
Write-Host "If you see 'SystemView Buffer is empty':" -ForegroundColor Yellow
Write-Host "  You stopped without a live session - use F5 to record, not offline read" -ForegroundColor Yellow
Write-Host ""

# Launch with absolute path; working directory = SvDir so relative Description path resolves.
Start-Process -FilePath $SystemViewExe -WorkingDirectory $SvDir -ArgumentList @(
    "`"$SvProject`""
)
