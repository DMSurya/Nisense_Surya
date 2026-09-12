# =============================================================================
# RETIRED — Do not use for current firmware.
# =============================================================================
# FatFS / USB MSC (/NAND:) are removed. Glucose matrices are packed XIP models.
# Use instead:
#   .\scripts\flash\flash_glucose_model.ps1
# See: docs/architecture/STORAGE_NOR_RECORD_STORE.md , model/glucose/README.md
# =============================================================================
# (Historical) Copy glucose Svasth matrix CSVs to NiSense USB MSC (FAT /NAND:)
# Source (repo):  model/glucose/{wearable,pulse}/group_*.csv
# On-device path:  /NAND:/model/glucose/{wearable,pulse}/group_*.csv
# Windows mount:   <NiSense volume>\model\glucose\...
#
# Prerequisites (obsolete):
#   - Device USB MSC enabled (USB screen or boot with CONFIG_USBD_MSC_CLASS)
#   - Windows shows a removable drive with volume label "NiSense"
#
# Usage (obsolete):
#   .\scripts\deploy\copy_glucose_model.ps1
#   .\scripts\deploy\copy_glucose_model.ps1 -DrivePath E:\
#   .\scripts\deploy\copy_glucose_model.ps1 -Variant wearable
#   .\scripts\deploy\copy_glucose_model.ps1 -ListDrives
# =============================================================================

param(
    [string]$DrivePath = "",
    [ValidateSet("both", "wearable", "pulse")]
    [string]$Variant = "both",
    [switch]$ListDrives,
    [switch]$Force
)

$ErrorActionPreference = "Stop"

throw @"
RETIRED: copy_glucose_model.ps1 (FatFS / USB MSC /NAND:).

Glucose matrices are packed XIP models. Use:
  .\scripts\flash\flash_glucose_model.ps1

See: docs/architecture/STORAGE_NOR_RECORD_STORE.md , model/glucose/README.md
"@

$ProjectRoot = (Get-Item $PSScriptRoot).Parent.Parent.FullName
$SourceRoot = Join-Path $ProjectRoot "model\glucose"
$VolumeLabel = "NiSense"
$WearableGroups = 11
$PulseGroups = 10

function Write-Step([string]$Message) {
    Write-Host $Message -ForegroundColor Cyan
}

function Write-Ok([string]$Message) {
    Write-Host $Message -ForegroundColor Green
}

function Write-Warn([string]$Message) {
    Write-Host $Message -ForegroundColor Yellow
}

function Get-HcmUsbDrives {
    Get-CimInstance -ClassName Win32_LogicalDisk -Filter "DriveType=2" |
        Where-Object {
            $_.VolumeName -eq $VolumeLabel -or
            ($_.VolumeName -eq $null -and $_.FileSystem -eq "FAT")
        } |
        ForEach-Object {
            [PSCustomObject]@{
                DeviceId = $_.DeviceID
                VolumeName = $_.VolumeName
                FileSystem = $_.FileSystem
                FreeGB = [math]::Round($_.FreeSpace / 1GB, 2)
                SizeGB = [math]::Round($_.Size / 1GB, 2)
            }
        }
}

function Resolve-TargetDrive {
    param([string]$ExplicitPath)

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        $normalized = $ExplicitPath.Trim().TrimEnd('\')
        if (-not (Test-Path -LiteralPath $normalized)) {
            throw "Drive path not found: $normalized"
        }
        return $normalized
    }

    $candidates = @(Get-HcmUsbDrives)
    if ($candidates.Count -eq 0) {
        throw @"
No USB MSC drive found (volume label '$VolumeLabel').

Enable USB Mass Storage on the device, wait for Windows to mount the drive,
then re-run. Or pass an explicit path:

  .\scripts\deploy\copy_glucose_model.ps1 -DrivePath E:\
"@
    }

    if ($candidates.Count -gt 1) {
        Write-Warn "Multiple removable drives match; using $($candidates[0].DeviceId)"
        foreach ($d in $candidates) {
            Write-Host "  $($d.DeviceId)  label='$($d.VolumeName)'  fs=$($d.FileSystem)  free=$($d.FreeGB) GB"
        }
    }

    return $candidates[0].DeviceId
}

function Test-VariantSource([string]$Name, [int]$GroupCount) {
    $dir = Join-Path $SourceRoot $Name
    if (-not (Test-Path -LiteralPath $dir)) {
        throw "Missing source folder: $dir"
    }

    $missing = @()
    for ($i = 1; $i -le $GroupCount; $i++) {
        $file = Join-Path $dir ("group_{0}.csv" -f $i)
        if (-not (Test-Path -LiteralPath $file)) {
            $missing += "group_$i.csv"
        }
    }

    if ($missing.Count -gt 0) {
        throw "Incomplete $Name matrices in repo - missing: $($missing -join ', ')"
    }
}

function Copy-Variant([string]$DriveRoot, [string]$Name) {
    $src = Join-Path $SourceRoot $Name
    $dst = Join-Path $DriveRoot "model\glucose\$Name"

    if (-not (Test-Path -LiteralPath $dst)) {
        New-Item -ItemType Directory -Path $dst -Force | Out-Null
    }

    $robocopyArgs = @(
        $src,
        $dst,
        "*.csv",
        "/NFL", "/NDL", "/NJH", "/NJS", "/NC", "/NS"
    )
    if ($Force) {
        $robocopyArgs += "/IS"
    }

    $output = & robocopy @robocopyArgs 2>&1
    $exitCode = $LASTEXITCODE

    # robocopy: 0-7 = success (with various copy stats), >=8 = error
    if ($exitCode -ge 8) {
        throw "robocopy failed for $Name (exit $exitCode): $output"
    }

    Write-Ok "  $Name -> $dst"
}

function Test-VariantDest([string]$DriveRoot, [string]$Name, [int]$GroupCount) {
    $dir = Join-Path $DriveRoot "model\glucose\$Name"
    $missing = @()

    for ($i = 1; $i -le $GroupCount; $i++) {
        $file = Join-Path $dir ("group_{0}.csv" -f $i)
        if (-not (Test-Path -LiteralPath $file)) {
            $missing += "group_$i.csv"
        }
    }

    if ($missing.Count -gt 0) {
        throw "Verification failed on device - $Name missing: $($missing -join ', ')"
    }

    $count = (Get-ChildItem -LiteralPath $dir -Filter "group_*.csv" -File).Count
    Write-Ok "  verified $Name ($count CSV files)"
}

Write-Host ""
Write-Step "Glucose model deploy -> USB MSC (FAT /NAND:)"
Write-Host "Repo source: $SourceRoot"
Write-Host ""

if ($ListDrives) {
    Write-Step "Removable drives:"
    Get-CimInstance -ClassName Win32_LogicalDisk -Filter "DriveType=2" |
        ForEach-Object {
            $label = if ($_.VolumeName) { $_.VolumeName } else { "(no label)" }
            Write-Host "  $($_.DeviceId)  label=$label  fs=$($_.FileSystem)"
        }
    exit 0
}

if (-not (Test-Path -LiteralPath $SourceRoot)) {
    throw "Glucose model folder not found: $SourceRoot"
}

$variants = switch ($Variant) {
    "wearable" { @(@{ Name = "wearable"; Groups = $WearableGroups }) }
    "pulse"    { @(@{ Name = "pulse"; Groups = $PulseGroups }) }
    default    {
        @(
            @{ Name = "wearable"; Groups = $WearableGroups },
            @{ Name = "pulse"; Groups = $PulseGroups }
        )
    }
}

foreach ($v in $variants) {
    Test-VariantSource -Name $v.Name -GroupCount $v.Groups
}

$drive = Resolve-TargetDrive -ExplicitPath $DrivePath
Write-Ok "Target drive: $drive"

Write-Step "Copying matrices..."
foreach ($v in $variants) {
    Copy-Variant -DriveRoot $drive -Name $v.Name
}

Write-Step "Verifying on device..."
foreach ($v in $variants) {
    Test-VariantDest -DriveRoot $drive -Name $v.Name -GroupCount $v.Groups
}

Write-Host ""
Write-Ok "Done. Device path: /NAND:/model/glucose/"
Write-Warn "Safely eject the USB drive in Windows before disconnecting USB."
Write-Host ""
