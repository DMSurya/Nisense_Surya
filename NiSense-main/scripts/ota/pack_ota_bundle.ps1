<#
.SYNOPSIS
  Pack a nisense-ota-v1 ZIP (wrapper around pack_ota_bundle.py).
#>
param(
  [Parameter(Mandatory = $true)][string]$Version,
  [string]$Firmware,
  [string]$Xip,
  [string]$Model,
  [ValidateSet("wearable", "pulse")][string]$ModelVariant = "wearable",
  [int]$ModelVersion = 0,
  [string]$ModelSignature,
  [string]$Out = "dist/nisense_ota_$Version.zip"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$py = Join-Path $PSScriptRoot "pack_ota_bundle.py"

$argsList = @($py, "--version", $Version, "--out", (Join-Path $root $Out))
if ($Firmware) { $argsList += @("--firmware", $Firmware) }
if ($Xip) { $argsList += @("--xip", $Xip) }
if ($Model) {
  $argsList += @("--model", $Model, "--model-variant", $ModelVariant, "--model-version", "$ModelVersion")
}
if ($ModelSignature) { $argsList += @("--model-signature", $ModelSignature) }

python @argsList
