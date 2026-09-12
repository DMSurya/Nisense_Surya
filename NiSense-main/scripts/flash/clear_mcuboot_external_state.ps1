# =============================================================================
# Clear MCUboot External State (QSPI)
# =============================================================================
# Erases ONLY external flash regions used by MCUboot image-0 swap state:
#   - mcuboot_secondary: 0x000000 - 0x0F2000
#   - mcuboot_scratch:   0x0F2000 - 0x0F6000
# Total erased: 0x0F6000 (984 KB)
#
# This is a recovery tool for cases where MCUboot is stuck due to stale
# secondary/scratch metadata. It does NOT erase resource, calibration,
# storage_nvs, or userdata_storage.
# =============================================================================

param(
    [string]$JLinkPath = "C:\Program Files\SEGGER\JLink"
)

$ErrorActionPreference = "Stop"

$JLinkExe = Join-Path $JLinkPath "JLink.exe"
if (-not (Test-Path $JLinkExe)) {
    Write-Host "ERROR: J-Link not found at $JLinkExe" -ForegroundColor Red
    exit 1
}

# nRF52840 QSPI registers
$QSPI = @{
    TASKS_ACTIVATE = "0x40029000"
    TASKS_ERASESTART = "0x4002900C"
    EVENTS_READY = "0x40029100"
    ENABLE = "0x40029500"
    ERASE_PTR = "0x4002951C"
    ERASE_LEN = "0x40029520"
    PSEL_SCK = "0x40029524"
    PSEL_CSN = "0x40029528"
    PSEL_IO0 = "0x40029530"
    PSEL_IO1 = "0x40029534"
    PSEL_IO2 = "0x40029538"
    PSEL_IO3 = "0x4002953C"
    IFCONFIG0 = "0x40029544"
    IFCONFIG1 = "0x40029600"
}

# Board-specific pins from overlay
$PIN = @{
    SCK = "0x00000013"  # P0.19
    CSN = "0x00000011"  # P0.17
    IO0 = "0x00000016"  # P0.22
    IO1 = "0x00000017"  # P0.23
    IO2 = "0x00000015"  # P0.21
    IO3 = "0x00000020"  # P1.0
}

$EraseStart = 0x000000
$EraseSize = 0x0F6000
$Block64K = 0x10000
$Sector4K = 0x1000

$blocks64 = [int][math]::Floor($EraseSize / $Block64K)
$remain = [int]($EraseSize - ($blocks64 * $Block64K))
$sectors4 = if ($remain -gt 0) { [int][math]::Ceiling($remain / $Sector4K) } else { 0 }

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Clear MCUboot External State" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Target external range: 0x000000-0x0F5FFF ($([math]::Round($EraseSize/1024,1)) KB)"
Write-Host "64KB blocks: $blocks64, tail 4KB sectors: $sectors4"
Write-Host ""

$temp = [System.IO.Path]::GetTempFileName() + ".jlink"
$sb = [System.Text.StringBuilder]::new()

[void]$sb.AppendLine("r")
[void]$sb.AppendLine("h")
[void]$sb.AppendLine("w4 $($QSPI.PSEL_SCK) $($PIN.SCK)")
[void]$sb.AppendLine("w4 $($QSPI.PSEL_CSN) $($PIN.CSN)")
[void]$sb.AppendLine("w4 $($QSPI.PSEL_IO0) $($PIN.IO0)")
[void]$sb.AppendLine("w4 $($QSPI.PSEL_IO1) $($PIN.IO1)")
[void]$sb.AppendLine("w4 $($QSPI.PSEL_IO2) $($PIN.IO2)")
[void]$sb.AppendLine("w4 $($QSPI.PSEL_IO3) $($PIN.IO3)")
[void]$sb.AppendLine("w4 $($QSPI.IFCONFIG0) 0x00000000")
[void]$sb.AppendLine("w4 $($QSPI.IFCONFIG1) 0xF0000080")
[void]$sb.AppendLine("w4 $($QSPI.ENABLE) 0x00000001")
[void]$sb.AppendLine("w4 $($QSPI.EVENTS_READY) 0x00000000")
[void]$sb.AppendLine("w4 $($QSPI.TASKS_ACTIVATE) 0x00000001")
[void]$sb.AppendLine("sleep 200")

for ($i = 0; $i -lt $blocks64; $i++) {
    $addr = $EraseStart + ($i * $Block64K)
    $hex = "0x" + $addr.ToString("X8")
    [void]$sb.AppendLine("w4 $($QSPI.ERASE_PTR) $hex")
    [void]$sb.AppendLine("w4 $($QSPI.ERASE_LEN) 0x00000001")
    [void]$sb.AppendLine("w4 $($QSPI.EVENTS_READY) 0x00000000")
    [void]$sb.AppendLine("w4 $($QSPI.TASKS_ERASESTART) 0x00000001")
    [void]$sb.AppendLine("sleep 1500")
}

if ($sectors4 -gt 0) {
    $base = $EraseStart + ($blocks64 * $Block64K)
    for ($s = 0; $s -lt $sectors4; $s++) {
        $addr = $base + ($s * $Sector4K)
        $hex = "0x" + $addr.ToString("X8")
        [void]$sb.AppendLine("w4 $($QSPI.ERASE_PTR) $hex")
        [void]$sb.AppendLine("w4 $($QSPI.ERASE_LEN) 0x00000000")
        [void]$sb.AppendLine("w4 $($QSPI.EVENTS_READY) 0x00000000")
        [void]$sb.AppendLine("w4 $($QSPI.TASKS_ERASESTART) 0x00000001")
        [void]$sb.AppendLine("sleep 500")
    }
}

[void]$sb.AppendLine("r")
[void]$sb.AppendLine("g")
[void]$sb.AppendLine("exit")

$sb.ToString() | Out-File -FilePath $temp -Encoding ASCII -Force

try {
    $proc = Start-Process -FilePath $JLinkExe -ArgumentList @(
        "-device", "nRF52840_xxAA",
        "-if", "swd",
        "-speed", "4000",
        "-autoconnect", "1",
        "-CommandFile", $temp
    ) -NoNewWindow -Wait -PassThru

    if ($proc.ExitCode -ne 0) {
        Write-Host "ERROR: J-Link erase failed (exit $($proc.ExitCode))" -ForegroundColor Red
        exit 1
    }
} finally {
    Remove-Item $temp -Force -ErrorAction SilentlyContinue
}

Write-Host ""
Write-Host "External MCUboot secondary+scratch erased successfully." -ForegroundColor Green
Write-Host "Next: flash merged image, then run RTT monitor with reset." -ForegroundColor Cyan
