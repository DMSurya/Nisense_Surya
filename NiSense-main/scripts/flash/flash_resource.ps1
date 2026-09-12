# =============================================================================
# Flash Resource store (wrapper → flash_qspi_stores.ps1)
# =============================================================================
# Prefer the common script:
#   .\flash_qspi_stores.ps1 -Targets resource
#   .\flash_qspi_stores.ps1                    # resource+model+calibration
# =============================================================================

param(
    [string]$BuildDir = "build_sdk_v330",
    [string]$JLinkSerialNumber = "",
    [switch]$Verify,
    [switch]$EraseOnly
)

$ErrorActionPreference = "Stop"
$common = Join-Path $PSScriptRoot "flash_qspi_stores.ps1"
$splat = @{
    Targets  = "resource"
    BuildDir = $BuildDir
}
if ($JLinkSerialNumber) { $splat.JLinkSerialNumber = $JLinkSerialNumber }
if ($EraseOnly) { $splat.EraseOnly = $true }

& $common @splat
exit $LASTEXITCODE
