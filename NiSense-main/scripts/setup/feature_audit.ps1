# =============================================================================
# Feature Memory Audit Script
# =============================================================================
# Builds each APP_FEATURE_* one at a time on top of the core+display baseline,
# measures flash and RAM delta, and outputs a summary table.
#
# Usage:
#   .\scripts\setup\feature_audit.ps1
#   .\scripts\setup\feature_audit.ps1 -SkipClean   (faster; reuses CMake cache)
#   .\scripts\setup\feature_audit.ps1 -Features RttLog,BLE,Wifi
#
# Output columns:
#   Feature | Flash (KB) | RAM (KB) | Flash Delta | RAM Delta | Status
# =============================================================================

param(
    [switch]$SkipClean,
    [string[]]$Features = @()   # Leave empty to run all features
)

$ErrorActionPreference = "Continue"
$ProjectRoot = (Get-Item $PSScriptRoot).Parent.Parent.FullName
$ConfigFile  = Join-Path $ProjectRoot ".vscode\nrf_sdk_config.json"
$config      = Get-Content $ConfigFile -Raw | ConvertFrom-Json

$BuildRoot  = Join-Path $ProjectRoot ($config.BuildDirName + "_feature_audit")
$pythonPath  = Join-Path $config.ToolchainPath "opt\bin\python.exe"
$sizeTool    = Join-Path $config.ToolchainPath "opt\zephyr-sdk\arm-zephyr-eabi\bin\arm-zephyr-eabi-size.exe"
$Board       = "raytac_mdbt50q_db_40/nrf52840"

# ---------------------------------------------------------------------------
# Feature definitions
#   Key   = display name
#   Flags = west CMake -D flags
#   Conf  = conf overlay paths (relative to project root)
#   Deps  = list of feature keys that must also be enabled
# ---------------------------------------------------------------------------
$AllFeatures = [ordered]@{
    "RttLog" = @{
        Flags = @("-DCONFIG_APP_FEATURE_RTT_LOG=y")
        Conf  = @("conf/features/rtt_log.conf")
        Deps  = @()
    }
    "FaultDebug" = @{
        Flags = @("-DCONFIG_APP_FEATURE_FAULT_DEBUG=y")
        Conf  = @("conf/features/fault_debug.conf")
        Deps  = @()
    }
    "DiagMonitor" = @{
        Flags = @("-DCONFIG_APP_FEATURE_DIAG_MONITOR=y")
        Conf  = @("conf/features/diag_monitor.conf")
        Deps  = @()
    }
    "Shell" = @{
        Flags = @("-DCONFIG_APP_FEATURE_USB=y", "-DCONFIG_APP_FEATURE_SHELL=y")
        Conf  = @("conf/features/usb.conf", "conf/features/shell.conf")
        Deps  = @("USB")
    }
    "FileSystem" = @{
        Flags = @("-DCONFIG_APP_FEATURE_FILE_SYSTEM=y")
        Conf  = @("conf/features/filesystem.conf")
        Deps  = @()
    }
    "ConfigPersist" = @{
        Flags = @("-DCONFIG_APP_FEATURE_FILE_SYSTEM=y", "-DCONFIG_APP_FEATURE_CONFIG_PERSIST=y")
        Conf  = @("conf/features/filesystem.conf", "conf/features/config_persist.conf")
        Deps  = @("FileSystem")
    }
    "USB" = @{
        Flags = @("-DCONFIG_APP_FEATURE_USB=y")
        Conf  = @("conf/features/usb.conf")
        Deps  = @()
    }
    "Wifi" = @{
        Flags = @("-DCONFIG_APP_FEATURE_WIFI=y")
        Conf  = @("conf/features/wifi.conf")
        Deps  = @()
    }
    "BLE" = @{
        Flags = @("-DCONFIG_APP_FEATURE_BLE=y")
        Conf  = @("conf/features/ble.conf")
        Deps  = @()
    }
    "MAX32664" = @{
        Flags = @("-DCONFIG_APP_FEATURE_MAX32664=y")
        Conf  = @("conf/features/max32664.conf")
        Deps  = @()
    }
    "MAX3010x" = @{
        Flags = @("-DCONFIG_APP_FEATURE_MAX3010X=y")
        Conf  = @("conf/features/max3010x.conf")
        Deps  = @()
    }
    "HR_via_MAX32664" = @{
        Flags = @("-DCONFIG_APP_FEATURE_MAX32664=y", "-DCONFIG_APP_FEATURE_PPG=y", "-DCONFIG_APP_FEATURE_HR=y")
        Conf  = @("conf/features/max32664.conf", "conf/features/ppg.conf", "conf/features/hr.conf")
        Deps  = @("MAX32664")
    }
    "SpO2_via_MAX32664" = @{
        Flags = @("-DCONFIG_APP_FEATURE_MAX32664=y", "-DCONFIG_APP_FEATURE_PPG=y", "-DCONFIG_APP_FEATURE_SPO2=y")
        Conf  = @("conf/features/max32664.conf", "conf/features/ppg.conf", "conf/features/spo2.conf")
        Deps  = @("MAX32664")
    }
    "HR_SpO2_MAX32664" = @{
        Flags = @("-DCONFIG_APP_FEATURE_MAX32664=y", "-DCONFIG_APP_FEATURE_PPG=y", "-DCONFIG_APP_FEATURE_HR=y", "-DCONFIG_APP_FEATURE_SPO2=y")
        Conf  = @("conf/features/max32664.conf", "conf/features/ppg.conf", "conf/features/hr.conf", "conf/features/spo2.conf")
        Deps  = @("MAX32664")
    }
    "MAX32664Update" = @{
        Flags = @("-DCONFIG_APP_FEATURE_FILE_SYSTEM=y", "-DCONFIG_APP_FEATURE_MAX32664_UPDATE=y")
        Conf  = @("conf/features/filesystem.conf", "conf/features/max32664_update.conf")
        Deps  = @("FileSystem")
    }
    "Glucose" = @{
        Flags = @("-DCONFIG_APP_FEATURE_GLUCOSE=y")
        Conf  = @("conf/features/glucose.conf")
        Deps  = @("FileSystem")
    }
    "SystemView" = @{
        Flags = @("-DCONFIG_APP_FEATURE_SYSTEMVIEW=y")
        Conf  = @("conf/features/systemview.conf")
        Deps  = @()
    }
}

# Preserve full feature catalog so dependency checks remain valid even when
# running a filtered subset via -Features.
$FeatureCatalog = [ordered]@{}
foreach ($k in $AllFeatures.Keys) {
    $FeatureCatalog[$k] = $AllFeatures[$k]
}

# Filter features if -Features was specified
if ($Features.Count -gt 0) {
    $normalizedFeatures = @()
    foreach ($f in $Features) {
        if ([string]::IsNullOrWhiteSpace($f)) { continue }
        $normalizedFeatures += ($f -split ',') | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' }
    }

    $filtered = [ordered]@{}
    foreach ($name in $normalizedFeatures) {
        if ($AllFeatures.Contains($name)) {
            $filtered[$name] = $AllFeatures[$name]
        } else {
            Write-Host "Warning: Unknown feature '$name' (ignored)" -ForegroundColor Yellow
        }
    }
    $AllFeatures = $filtered
}

# ---------------------------------------------------------------------------
# Environment setup (mirrors build_project.ps1)
# ---------------------------------------------------------------------------
$nrfutilSdkManager = $null
$vsCodeExtensionsPath = Join-Path $env:USERPROFILE ".vscode\extensions"
if (Test-Path $vsCodeExtensionsPath) {
    $nrfExtension = Get-ChildItem -Path $vsCodeExtensionsPath -Directory |
        Where-Object { $_.Name -like "nordic-semiconductor.nrf-connect-2*" } |
        Sort-Object Name -Descending | Select-Object -First 1
    if ($nrfExtension) {
        $nrfutilSdkManager = Join-Path $nrfExtension.FullName "platform\nrfutil\bin\nrfutil-sdk-manager.exe"
    }
}

if ($nrfutilSdkManager -and (Test-Path $nrfutilSdkManager) -and $config.ToolchainPath) {
    try {
        $envJson = & $nrfutilSdkManager toolchain env --skip-overhead --toolchain-path $config.ToolchainPath --json 2>&1
        $envData = $envJson | ConvertFrom-Json
        foreach ($var in $envData.env_variables) {
            Set-Item -Path "env:$($var.key)" -Value $var.value
        }
        $env:ZEPHYR_BASE = Join-Path $config.SDKPath "zephyr"
        Write-Host "Environment: nrfutil-sdk-manager" -ForegroundColor Green
    } catch {
        Write-Host "Warning: nrfutil fallback to manual env" -ForegroundColor Yellow
    }
}
$env:PYTHONHOME = ""; $env:PYTHONSTARTUP = ""; $env:PYTHONUSERBASE = ""

# ---------------------------------------------------------------------------
# Helper: run arm-zephyr-eabi-size and return [flash_bytes, ram_bytes]
# ---------------------------------------------------------------------------
function Get-ElfSize {
    param([string]$ElfPath)
    if (-not (Test-Path $ElfPath)) { return @(0, 0) }
    if (-not (Test-Path $sizeTool)) {
        # Fallback: parse map file for _image_ram_size / _flash_used
        return @(0, 0)
    }
    $out = & $sizeTool $ElfPath 2>$null
    # Output format: text    data     bss     dec     hex filename
    if ($out -and $out.Count -ge 2) {
        $parts = ($out[1] -split '\s+' | Where-Object { $_ -ne "" })
        if ($parts.Count -ge 3) {
            $text = [int64]$parts[0]
            $data = [int64]$parts[1]
            $bss  = [int64]$parts[2]
            $flash = $text + $data
            $ram   = $data + $bss
            return @($flash, $ram)
        }
    }
    return @(0, 0)
}

# ---------------------------------------------------------------------------
# Core overlays (always)
# ---------------------------------------------------------------------------
$coreOverlays = @(
    "conf/core/bootloader.conf",
    "conf/core/memory.conf",
    "conf/core/hardware.conf",
    "conf/core/storage.conf",
    "conf/core/debug.conf",
    "conf/core/pmic.conf",
    "conf/core/sensors.conf",
    "conf/core/time.conf",
    "conf/features/display.conf"   # display is always on
)

# Force a deterministic APP_FEATURE_* baseline for audit runs so current
# prj.conf defaults cannot leak into single-feature measurements.
$baseFeatureFlags = @(
    "-DCONFIG_APP_FEATURE_DISPLAY=y",
    "-DCONFIG_APP_FEATURE_RTT_LOG=n",
    "-DCONFIG_APP_FEATURE_FAULT_DEBUG=n",
    "-DCONFIG_APP_FEATURE_SHELL=n",
    "-DCONFIG_APP_FEATURE_FILE_SYSTEM=n",
    "-DCONFIG_APP_FEATURE_CONFIG_PERSIST=n",
    "-DCONFIG_APP_FEATURE_GLUCOSE=n",
    "-DCONFIG_APP_FEATURE_HR=n",
    "-DCONFIG_APP_FEATURE_SPO2=n",
    "-DCONFIG_APP_FEATURE_PPG=n",
    "-DCONFIG_APP_FEATURE_WIFI=n",
    "-DCONFIG_APP_FEATURE_BLE=n",
    "-DCONFIG_APP_FEATURE_USB=n",
    "-DCONFIG_APP_FEATURE_SYSTEMVIEW=n",
    "-DCONFIG_APP_FEATURE_MAX32664=n",
    "-DCONFIG_APP_FEATURE_MAX3010X=n",
    "-DCONFIG_APP_FEATURE_MAX32664_UPDATE=n",
    "-DCONFIG_APP_FEATURE_DIAG_MONITOR=n"
)

# ---------------------------------------------------------------------------
# Helper: build with given feature overlays and flags
# ---------------------------------------------------------------------------
function Invoke-FeatureBuild {
    param(
        [string]   $Label,
        [string[]] $ExtraFlags,
        [string[]] $ExtraConf,
        [string]   $BuildDir
    )

    $allConf   = $coreOverlays + $ExtraConf
    $allFlags  = $baseFeatureFlags + $ExtraFlags + @("-DNiSense_EXTRA_CONF_FILE=$($allConf -join ';')")

    $westArgs = @("build", "--build-dir", $BuildDir, "--board", $Board, "--sysbuild", "--") + $allFlags

    Write-Host ""
    Write-Host ("=" * 60) -ForegroundColor Cyan
    Write-Host "  Building: $Label" -ForegroundColor Cyan
    Write-Host ("=" * 60) -ForegroundColor Cyan

    # On Windows, ninja may fail with "failed recompaction: Permission denied"
    # when build.ninja is locked by a lingering prior process. Retry once with delay.
    Push-Location $ProjectRoot
    $succeeded = $false
    try {
        for ($attempt = 0; $attempt -lt 2; $attempt++) {
            if ($attempt -gt 0) {
                Write-Host "  [retry $attempt] Waiting 3s for file handles to release..." -ForegroundColor Yellow
                Start-Sleep -Seconds 3
                
            }
            $output = & $pythonPath "-m" "west" $westArgs 2>&1
            $succeeded = $LASTEXITCODE -eq 0
            $output | Where-Object {
                $_ -match "error:|warning:|Building|Linking|FAILED" -and
                $_ -notmatch "PARTITION_MANAGER|MBEDTLS_MD|MIPI_DBI|pixel array|deprecated"
            } | ForEach-Object { Write-Host $_ -ForegroundColor Gray }

            $permDenied = $output | Where-Object { $_ -match "failed recompaction: Permission denied" }
            if (-not $succeeded -and $permDenied) {
                Write-Host "  [ninja recompaction Permission Denied -- will retry]" -ForegroundColor Yellow
            } else {
                break
            }
        }
    } finally {
        Pop-Location
    }
    return $succeeded
}

# ---------------------------------------------------------------------------
# Main audit loop
# ---------------------------------------------------------------------------
$results = [System.Collections.ArrayList]::new()

# 0. BASELINE: core + display
Write-Host "`n>>> Baseline build (core + display)..." -ForegroundColor Yellow

$baselineBuildDir = Join-Path $BuildRoot "BASELINE"

# Clean isolated audit root only at start
if (-not $SkipClean -and (Test-Path $BuildRoot)) {
    Remove-Item $BuildRoot -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "Cleaned $BuildRoot" -ForegroundColor Yellow
}

$baselineOk = Invoke-FeatureBuild "BASELINE (core+display)" @() @() $baselineBuildDir

$elfPath = Join-Path $baselineBuildDir "NiSense\zephyr\zephyr.elf"
$baseSizes = Get-ElfSize $elfPath
$baseFlash = $baseSizes[0]
$baseRam   = $baseSizes[1]

$null = $results.Add([PSCustomObject]@{
    Feature      = "BASELINE"
    Status       = if ($baselineOk) { "OK" } else { "FAIL" }
    Flash_KB     = [math]::Round($baseFlash / 1024, 1)
    RAM_KB       = [math]::Round($baseRam   / 1024, 1)
    FlashDelta   = 0
    RAMDelta     = 0
    Dependencies = "(none)"
})

if (-not $baselineOk) {
    Write-Host "Baseline failed. Stopping feature loop." -ForegroundColor Red
}

# 1-N. Each feature on top of baseline
foreach ($featureName in $AllFeatures.Keys) {
    if (-not $baselineOk) { break }
    $feat = $AllFeatures[$featureName]

    Write-Host "`n>>> Feature: $featureName" -ForegroundColor Yellow

    $featureBuildDir = Join-Path $BuildRoot $featureName
    $featureOk = Invoke-FeatureBuild $featureName $feat.Flags $feat.Conf $featureBuildDir

    $elfPath = Join-Path $featureBuildDir "NiSense\zephyr\zephyr.elf"
    $sizes = Get-ElfSize $elfPath
    $flash = $sizes[0]; $ram = $sizes[1]
    $dFlash = [math]::Round(($flash - $baseFlash) / 1024, 1)
    $dRam   = [math]::Round(($ram   - $baseRam)   / 1024, 1)

    $deps = if ($feat.Deps.Count -gt 0) { $feat.Deps -join ", " } else { "(none)" }

    $null = $results.Add([PSCustomObject]@{
        Feature      = $featureName
        Status       = if ($featureOk) { "OK" } else { "FAIL" }
        Flash_KB     = [math]::Round($flash / 1024, 1)
        RAM_KB       = [math]::Round($ram   / 1024, 1)
        FlashDelta   = $dFlash
        RAMDelta     = $dRam
        Dependencies = $deps
    })
}

# ---------------------------------------------------------------------------
# Print summary table
# ---------------------------------------------------------------------------
Write-Host ("=" * 90) -ForegroundColor Cyan
Write-Host (" FEATURE MEMORY AUDIT RESULTS - NCS v3.3.0 - $(Get-Date -Format 'yyyy-MM-dd')") -ForegroundColor Cyan
Write-Host ("=" * 90) -ForegroundColor Cyan
Write-Host ("{0,-22} {1,-6} {2,10} {3,9} {4,12} {5,10} {6}" -f "Feature","Status","Flash(KB)","RAM(KB)","Flash+D(KB)","RAM+D(KB)","Dependencies") -ForegroundColor White
Write-Host ("-" * 90) -ForegroundColor DarkGray

foreach ($r in $results) {
    $color = if ($r.Status -eq "OK") { "Green" } else { "Red" }
    $dFlashStr = if ($r.FlashDelta -gt 0) { "+$($r.FlashDelta)" } elseif ($r.FlashDelta -lt 0) { "$($r.FlashDelta)" } else { "-" }
    $dRamStr   = if ($r.RAMDelta   -gt 0) { "+$($r.RAMDelta)"   } elseif ($r.RAMDelta   -lt 0) { "$($r.RAMDelta)"   } else { "-" }
    Write-Host ("{0,-22} {1,-6} {2,10} {3,9} {4,12} {5,10} {6}" -f `
        $r.Feature, $r.Status, $r.Flash_KB, $r.RAM_KB, $dFlashStr, $dRamStr, $r.Dependencies) -ForegroundColor $color
}
Write-Host ("=" * 90) -ForegroundColor Cyan
Write-Host ""

# ---------------------------------------------------------------------------
# Save results to CSV
# ---------------------------------------------------------------------------
$csvPath = Join-Path $ProjectRoot "scripts\setup\feature_audit_results.csv"
$results | Export-Csv -Path $csvPath -NoTypeInformation
Write-Host "Results saved to: $csvPath" -ForegroundColor Green

# Dependency verification
Write-Host ""
Write-Host "DEPENDENCY VERIFICATION" -ForegroundColor Cyan
Write-Host ("-" * 50) -ForegroundColor DarkGray
foreach ($featureName in $AllFeatures.Keys) {
    $feat = $AllFeatures[$featureName]
    if ($feat.Deps.Count -gt 0) {
        $depsOk = $feat.Deps | ForEach-Object {
            $depName = $_
            if (-not $FeatureCatalog.Contains($depName)) {
                $false
            } else {
                $FeatureCatalog[$depName].Conf | ForEach-Object { $feat.Conf -contains $_ }
            }
        }
        $allOk = $depsOk -notcontains $false
        $symbol = if ($allOk) { "[OK]" } else { "[MISSING]" }
        $col    = if ($allOk) { "Green" } else { "Red" }
        Write-Host "  $featureName requires $($feat.Deps -join ', ') -- $symbol" -ForegroundColor $col
    } else {
        Write-Host "  $featureName -- no dependencies" -ForegroundColor DarkGray
    }
}
Write-Host ""


