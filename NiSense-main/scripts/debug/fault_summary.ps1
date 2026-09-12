# =============================================================================
# Fault Diagnostic Summary Script
# =============================================================================
# Run this after a crash to get a clear picture of what happened
#
# Usage: .\fault_summary.ps1
# =============================================================================

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  nRF52840 Fault Diagnostic Summary" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Create JLink command file
$jlinkCommands = @"
connect
halt
sleep 100

// Core registers
regs

// Fault registers
mem32 0xE000ED28 1
mem32 0xE000ED2C 1
mem32 0xE000ED30 1
mem32 0xE000ED34 1
mem32 0xE000ED38 1
mem32 0xE000ED08 1

// Vector tables
mem32 0x0 4
mem32 0xA200 4

// PSP stack frame (if in thread mode fault)
mem32 0x20003670 8

exit
"@

$jlinkFile = "$env:TEMP\fault_summary.jlink"
$jlinkCommands | Out-File -FilePath $jlinkFile -Encoding ASCII

# Run JLink and capture output
$output = & "C:\Program Files\SEGGER\JLink\JLink.exe" -device nRF52840_xxAA -if swd -speed 4000 -autoconnect 1 -CommandFile $jlinkFile 2>&1

# Parse the output
Write-Host "Raw J-Link Output:" -ForegroundColor Yellow
Write-Host "-------------------" -ForegroundColor Yellow
$output | ForEach-Object { Write-Host $_ }

Write-Host ""
Write-Host "============================================" -ForegroundColor Green
Write-Host "  DIAGNOSIS SUMMARY" -ForegroundColor Green
Write-Host "============================================" -ForegroundColor Green
Write-Host ""

# Extract key values from output
$vtor = ($output | Where-Object { $_ -match "E000ED08\s*=\s*([0-9A-Fa-f]+)" }) -replace ".*=\s*", ""
$ipsr = ($output | Where-Object { $_ -match "IPSR\s*=\s*(\d+)" }) -replace ".*IPSR\s*=\s*(\d+).*", '$1'
$pc = ($output | Where-Object { $_ -match "PC\s*=\s*([0-9A-Fa-f]+)" }) -replace ".*PC\s*=\s*", ""

Write-Host "Key Findings:" -ForegroundColor White
Write-Host "  VTOR     = $vtor" -ForegroundColor $(if ($vtor -eq "00000000") { "Red" } else { "Green" })
Write-Host "  PC       = $pc"
Write-Host "  IPSR     = $ipsr (5=BusFault, 4=MemManage, 6=UsageFault)"
Write-Host ""

if ($vtor -eq "00000000") {
    Write-Host "[!] VTOR = 0 means MCUboot's vector table is active!" -ForegroundColor Red
    Write-Host "    This crash is in MCUboot, NOT the application." -ForegroundColor Red
    Write-Host ""
    Write-Host "    The crash likely occurs because:" -ForegroundColor Yellow
    Write-Host "    1. MCUboot is trying to read the app image header" -ForegroundColor Yellow
    Write-Host "    2. flash_area_read() is failing with a bus error" -ForegroundColor Yellow
    Write-Host "    3. This could be a flash access issue or corruption" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Recommendations:" -ForegroundColor Cyan
    Write-Host "    1. Check if the app image is properly signed" -ForegroundColor Cyan
    Write-Host "    2. Verify flash partition configuration matches actual layout" -ForegroundColor Cyan
    Write-Host "    3. Try erasing all flash and re-flashing both MCUboot and app" -ForegroundColor Cyan
    Write-Host "    4. Check prj.conf for CONFIG_FLASH_MAP settings" -ForegroundColor Cyan
} else {
    Write-Host "[OK] VTOR = 0x$vtor - Application vector table is active" -ForegroundColor Green
    Write-Host "     The crash is in the application code." -ForegroundColor Green
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  End of Diagnostic Summary" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan

# Cleanup
Remove-Item $jlinkFile -ErrorAction SilentlyContinue

