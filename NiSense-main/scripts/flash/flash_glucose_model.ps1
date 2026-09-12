# =============================================================================
# Flash Model store (wrapper → flash_qspi_stores.ps1)
# =============================================================================
# Prefer the common script:
#   .\flash_qspi_stores.ps1 -Targets model -ModelVariant pack
#   .\flash_qspi_stores.ps1                    # resource+model+calibration
# =============================================================================

param(
    [ValidateSet("wearable", "pulse", "pack")]
    [string]$Variant = "pack",
    [ValidateSet("primary", "secondary")]
    [string]$Slot = "primary",
    [string]$BuildDir = "build_sdk_v330",
    [string]$JLinkSerialNumber = "",
    [switch]$EraseOnly
)

$ErrorActionPreference = "Stop"
$common = Join-Path $PSScriptRoot "flash_qspi_stores.ps1"
$splat = @{
    Targets      = "model"
    BuildDir     = $BuildDir
    ModelVariant = $Variant
    ModelSlot    = $Slot
}
if ($JLinkSerialNumber) { $splat.JLinkSerialNumber = $JLinkSerialNumber }
if ($EraseOnly) { $splat.EraseOnly = $true }

& $common @splat
exit $LASTEXITCODE
