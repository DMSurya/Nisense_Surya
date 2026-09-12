# =============================================================================
# Build Project Script
# =============================================================================
# Builds the project using nRF Connect SDK
# Uses the same environment setup as the VS Code nRF Connect extension:
#   nrfutil-sdk-manager.exe toolchain env --toolchain-path <path>
# =============================================================================

param(
    [switch]$Clean,
    [switch]$Verbose,
    [switch]$EnableGlucose,
    [switch]$EnableWifi,
    [switch]$EnableBle,
    [switch]$EnableFilesystem,
    [switch]$EnableConfigPersist,
    [switch]$EnableUsb,
    [switch]$EnableSystemView,
    [switch]$EnableFaultDebug,
    [switch]$EnableShell,
    [switch]$EnableMax32664,
    [switch]$EnableMax32664VerboseDebug,
    [switch]$EnableMax3010x,
    [switch]$EnablePpg,
    [switch]$EnableMax32664Update,
    [switch]$EnableDiagMonitor,
    [switch]$EnableRttLog,
    [switch]$EnableBleDfu,
    [switch]$HeadlessOnly,
    [string]$Board = "",
    [string]$BuildDir = ""
)

$ErrorActionPreference = "Continue"

# Load configuration (same data as setup_nrf_sdk.ps1 — usually under scripts/setup/)
$ProjectRoot = (Get-Item $PSScriptRoot).Parent.Parent.FullName
$ConfigCandidates = @(
    (Join-Path $ProjectRoot ".vscode\nrf_sdk_config.json"),
    (Join-Path (Split-Path $PSScriptRoot -Parent) "..\\.vscode\\nrf_sdk_config.json"),
    (Join-Path $PSScriptRoot "nrf_sdk_config.json")
)
$ConfigFile = $null
foreach ($candidate in $ConfigCandidates) {
    if (Test-Path $candidate) {
        $ConfigFile = $candidate
        break
    }
}
if (-not $ConfigFile) {
    Write-Host "Error: nRF SDK configuration not found. Run scripts/setup/setup_nrf_sdk.ps1 first." -ForegroundColor Red
    exit 1
}

$config = Get-Content $ConfigFile -Raw | ConvertFrom-Json

# Set build directory (single app config: prj.conf — RTT / SystemView / debug live there)
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = if ($config.BuildDirName) { $config.BuildDirName } else { "build_sdk_v330" }
}
$BuildDir = Join-Path $ProjectRoot $BuildDir

# Set board
if ([string]::IsNullOrWhiteSpace($Board)) {
    $Board = "raytac_mdbt50q_db_40/nrf52840"
}

# Find the nrfutil-sdk-manager.exe bundled with the VS Code extension
# This is the same tool the extension uses to get proper environment
$nrfutilSdkManager = $null
$vsCodeExtensionsPath = Join-Path $env:USERPROFILE ".vscode\extensions"
if (Test-Path $vsCodeExtensionsPath) {
    # Look for nrf-connect extension (not extension-pack), must start with 2 for version year
    $nrfExtension = Get-ChildItem -Path $vsCodeExtensionsPath -Directory | 
                    Where-Object { $_.Name -like "nordic-semiconductor.nrf-connect-2*" } |
                    Sort-Object Name -Descending | 
                    Select-Object -First 1
    if ($nrfExtension) {
        $nrfutilSdkManager = Join-Path $nrfExtension.FullName "platform\nrfutil\bin\nrfutil-sdk-manager.exe"
    }
}

# Get environment from nrfutil-sdk-manager (same approach as VS Code extension)
if ($nrfutilSdkManager -and (Test-Path $nrfutilSdkManager) -and $config.ToolchainPath) {
    Write-Host "Getting toolchain environment from nrfutil-sdk-manager..." -ForegroundColor Cyan
    try {
        $envJson = & $nrfutilSdkManager toolchain env --skip-overhead --toolchain-path $config.ToolchainPath --json 2>&1
        $envData = $envJson | ConvertFrom-Json
        
        foreach ($var in $envData.env_variables) {
            Set-Item -Path "env:$($var.key)" -Value $var.value
        }
        
        # Also set ZEPHYR_BASE for west
        $env:ZEPHYR_BASE = Join-Path $config.SDKPath "zephyr"
        
        Write-Host "Environment configured via nrfutil-sdk-manager" -ForegroundColor Green
    }
    catch {
        Write-Host "Warning: nrfutil-sdk-manager failed, falling back to manual environment setup" -ForegroundColor Yellow
        Write-Host "Error: $_" -ForegroundColor Yellow
        
        # Fallback to manual setup
        $env:NCS_ROOT = $config.SDKPath
        $env:ZEPHYR_BASE = Join-Path $config.SDKPath "zephyr"
        $env:ZEPHYR_TOOLCHAIN_VARIANT = "zephyr"
        
        $zephyrSdk = Join-Path $config.ToolchainPath "opt\zephyr-sdk"
        $env:ZEPHYR_SDK_INSTALL_DIR = $zephyrSdk
        
        $toolchainPaths = @(
            $config.ToolchainPath,
            (Join-Path $config.ToolchainPath "mingw64\bin"),
            (Join-Path $config.ToolchainPath "bin"),
            (Join-Path $config.ToolchainPath "opt\bin"),
            (Join-Path $config.ToolchainPath "opt\bin\Scripts"),
            (Join-Path $config.ToolchainPath "opt\nanopb\generator-bin"),
            (Join-Path $zephyrSdk "arm-zephyr-eabi\bin"),
            (Join-Path $zephyrSdk "riscv64-zephyr-elf\bin")
        )
        
        # Filter out Python from system PATH
        $filteredPath = ($env:PATH -split ";") | Where-Object { 
            $_ -notmatch "Python\d|Python\\|\\python|Launcher" -and $_ -ne ""
        }
        
        $env:PATH = ($toolchainPaths -join ";") + ";" + ($filteredPath -join ";")
        $env:PYTHONPATH = (Join-Path $config.ToolchainPath "opt\bin") + ";" + 
                          (Join-Path $config.ToolchainPath "opt\bin\Lib") + ";" + 
                          (Join-Path $config.ToolchainPath "opt\bin\Lib\site-packages")
    }
}
else {
    Write-Host "Warning: nrfutil-sdk-manager not found, using manual environment setup" -ForegroundColor Yellow
    
    # Fallback to manual setup
    $env:NCS_ROOT = $config.SDKPath
    $env:ZEPHYR_BASE = Join-Path $config.SDKPath "zephyr"
    $env:ZEPHYR_TOOLCHAIN_VARIANT = "zephyr"
    
    if ($config.ToolchainPath) {
        $zephyrSdk = Join-Path $config.ToolchainPath "opt\zephyr-sdk"
        $env:ZEPHYR_SDK_INSTALL_DIR = $zephyrSdk
        
        $toolchainPaths = @(
            $config.ToolchainPath,
            (Join-Path $config.ToolchainPath "mingw64\bin"),
            (Join-Path $config.ToolchainPath "bin"),
            (Join-Path $config.ToolchainPath "opt\bin"),
            (Join-Path $config.ToolchainPath "opt\bin\Scripts"),
            (Join-Path $config.ToolchainPath "opt\nanopb\generator-bin"),
            (Join-Path $zephyrSdk "arm-zephyr-eabi\bin"),
            (Join-Path $zephyrSdk "riscv64-zephyr-elf\bin")
        )
        
        # Filter out Python from system PATH
        $filteredPath = ($env:PATH -split ";") | Where-Object { 
            $_ -notmatch "Python\d|Python\\|\\python|Launcher" -and $_ -ne ""
        }
        
        $env:PATH = ($toolchainPaths -join ";") + ";" + ($filteredPath -join ";")
        $env:PYTHONPATH = (Join-Path $config.ToolchainPath "opt\bin") + ";" + 
                          (Join-Path $config.ToolchainPath "opt\bin\Lib") + ";" + 
                          (Join-Path $config.ToolchainPath "opt\bin\Lib\site-packages")
    }
}

# nrfutil "toolchain env" often replaces PATH with a toolchain-only list and
# may leave ComSpec unset/relative. CMake/Ninja link rules invoke cmd.exe;
# a bloated PATH (8k+) or missing System32 makes nested `cmd.exe /C` fail with:
#   'cmd.exe' is not recognized as an internal or external command.
$system32 = Join-Path $env:SystemRoot "System32"
$cmdPath = Join-Path $system32 "cmd.exe"
if (-not (Test-Path -LiteralPath $cmdPath)) {
    Write-Host "Error: Windows cmd.exe not found at $cmdPath" -ForegroundColor Red
    exit 1
}
$env:ComSpec = $cmdPath

# Remove accidental shim beside ninja.exe (CreateProcess searches the app dir
# first). A copied cmd.exe there is unnecessary and confusing.
$toolchainCmd = Join-Path $config.ToolchainPath "opt\bin\cmd.exe"
if (Test-Path -LiteralPath $toolchainCmd) {
    Remove-Item -LiteralPath $toolchainCmd -Force -ErrorAction SilentlyContinue
}

# Keep PATH short and deterministic: System32 + Windows + unique toolchain/env
# entries. Re-merging the full user PATH ballooned to ~10k chars and broke
# nested cmd.exe resolution during the zephyr_pre0 link step.
$ordered = New-Object System.Collections.Generic.List[string]
$seen = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
foreach ($p in @($system32, $env:SystemRoot) + @($env:PATH -split ';')) {
    if ([string]::IsNullOrWhiteSpace($p)) { continue }
    $trim = $p.Trim().TrimEnd('\')
    if (-not $seen.Add($trim)) { continue }
    # Drop empty/quote-corrupted entries that break cmd.exe PATH parsing.
    if ($trim.Contains('"')) { continue }
    $ordered.Add($trim)
}
$env:PATH = ($ordered -join ';')

if (-not (Get-Command cmd.exe -ErrorAction SilentlyContinue)) {
    Write-Host "Error: cmd.exe still not on PATH after fix (ComSpec=$env:ComSpec)" -ForegroundColor Red
    exit 1
}
Write-Host "Windows shell: ComSpec=$env:ComSpec  PATH length=$($env:PATH.Length)" -ForegroundColor Gray

# Clear potentially conflicting Python environment variables
$env:PYTHONHOME = ""
$env:PYTHONSTARTUP = ""
$env:PYTHONUSERBASE = ""

# Find Python path
$pythonPath = Join-Path $config.ToolchainPath "opt\bin\python.exe"

if (-not (Test-Path $pythonPath)) {
    Write-Host "Error: Python not found at $pythonPath" -ForegroundColor Red
    exit 1
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Building Project" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "SDK: $($config.SDKPath)" -ForegroundColor Gray
Write-Host "Version: $($config.Version)" -ForegroundColor Gray
Write-Host "Board: $Board" -ForegroundColor Gray
Write-Host "Build Directory: $BuildDir" -ForegroundColor Gray
Write-Host ""

function Test-PrjConfigEnabled {
    param([string]$Symbol)

    $prjConfPath = Join-Path $ProjectRoot "prj.conf"
    if (-not (Test-Path $prjConfPath)) {
        return $false
    }

    $match = Select-String -Path $prjConfPath -Pattern "^$([regex]::Escape($Symbol))=y$" -SimpleMatch:$false
    return $null -ne $match
}

# Only emit -DCONFIG_*=y when prj.conf does not already set it. Re-passing
# -D on every build sticks in CMakeCache / build_info and later ignores
# prj.conf =n (shell/USB "disabled" but still linked).
function Add-FeatureKconfigFlag {
    param(
        [string]$Symbol,
        [bool]$Enabled
    )
    if ($Enabled -and -not (Test-PrjConfigEnabled $Symbol)) {
        $script:featureKconfigFlags += "-D${Symbol}=y"
    }
}

$EnableGlucose = $EnableGlucose -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_GLUCOSE")
$EnableWifi = $EnableWifi -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_WIFI")
$EnableBle = $EnableBle -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_BLE")
$EnableFilesystem = $EnableFilesystem -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_FILE_SYSTEM")
$EnableConfigPersist = $EnableConfigPersist -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_CONFIG_PERSIST")
$EnableUsb = $EnableUsb -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_USB")
$EnableSystemView = $EnableSystemView -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_SYSTEMVIEW")
$EnableFaultDebug = $EnableFaultDebug -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_FAULT_DEBUG")
$EnableShell = $EnableShell -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_SHELL")
$EnableMax32664 = $EnableMax32664 -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_MAX32664")
$EnableMax3010x = $EnableMax3010x -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_MAX3010X")
$EnablePpg = $EnablePpg -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_PPG")
$EnableMax32664Update = $EnableMax32664Update -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_MAX32664_UPDATE")
$EnableDiagMonitor = $EnableDiagMonitor -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_DIAG_MONITOR")
$EnableRttLog = $EnableRttLog -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_RTT_LOG")
$EnableBleDfu = $EnableBleDfu -or (Test-PrjConfigEnabled "CONFIG_APP_FEATURE_BLE_DFU")
# BLE DFU requires BLE — auto-enable the BLE feature overlay when DFU is requested
if ($EnableBleDfu -and -not $EnableBle) {
    $EnableBle = $true
}

# Clean if requested
if ($Clean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    if (Test-Path $BuildDir) {
        Remove-Item -Path $BuildDir -Recurse -Force
        Write-Host "Build directory cleaned." -ForegroundColor Green
    }
}

# Prepare west arguments
$westArgs = @(
    "build",
    "--build-dir", $BuildDir,
    "--board", $Board,
    "--sysbuild"
)

# Core overlays are always included regardless of feature flags.
# conf/core/ holds mandatory infrastructure (bootloader, partition management).
$coreOverlays = @(
    "conf/core/bootloader.conf",
    "conf/core/memory.conf",
    "conf/core/hardware.conf",
    "conf/core/storage.conf",
    "conf/core/debug.conf",
    "conf/core/pmic.conf",
    "conf/core/sensors.conf",
    "conf/core/time.conf"
)

# Display is always required on this hardware (ST7789V + FT5336 touch).
# display.conf is always included; pass -HeadlessOnly to omit it.
if ($HeadlessOnly) {
    $featureOverlays = @()
    $featureKconfigFlags = @("-DCONFIG_APP_FEATURE_DISPLAY=n")
} else {
    $featureOverlays = @("conf/features/display.conf")
    $featureKconfigFlags = @()
}

# FatFS stack is removed. Only enable the (empty/deprecated) filesystem
# overlay when explicitly requested, or when USB MSC is on (still requires FS).
$EnableFilesystemEffective = $EnableFilesystem -or $EnableUsb
# Shell selects USB in Kconfig — keep overlay parity.
if ($EnableShell) {
    $EnableUsb = $true
}
if ($EnableFilesystemEffective) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_FILE_SYSTEM" $true
    $featureOverlays += "conf/features/filesystem.conf"
}
if ($EnableConfigPersist) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_CONFIG_PERSIST" $true
    $featureOverlays += "conf/features/config_persist.conf"
}

if ($EnableGlucose) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_GLUCOSE" $true
    $featureOverlays += "conf/features/glucose.conf"
}
if ($EnableWifi) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_WIFI" $true
    $featureOverlays += "conf/features/wifi.conf"
}
if ($EnableBle) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_BLE" $true
    $featureOverlays += "conf/features/ble.conf"
}
if ($EnableUsb) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_USB" $true
    $featureOverlays += "conf/features/usb.conf"
}
if ($EnableSystemView) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_SYSTEMVIEW" $true
    $featureOverlays += "conf/features/systemview.conf"
}
if ($EnableFaultDebug) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_FAULT_DEBUG" $true
    $featureOverlays += "conf/features/fault_debug.conf"
}
if ($EnableShell) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_SHELL" $true
    $featureOverlays += "conf/features/shell.conf"
}
# PPG pipeline — auto-select MAX32664 hub as default sensor when no sensor chosen
if ($EnablePpg -and -not $EnableMax3010x -and -not $EnableMax32664) {
    $EnableMax32664 = $true
}
if ($EnableMax32664) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_MAX32664" $true
    $featureOverlays += "conf/features/max32664.conf"
}
if ($EnableMax32664VerboseDebug) {
    # Verbose debug overlay targets MAX32664 RAW FIFO diagnostics.
    # Ensure base MAX32664 feature is active when requested.
    if (-not $EnableMax32664) {
        Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_MAX32664" $true
        $featureOverlays += "conf/features/max32664.conf"
        $EnableMax32664 = $true
    }
    $featureOverlays += "conf/features/max32664_debug_verbose.conf"
}
if ($EnableMax3010x) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_MAX3010X" $true
    $featureOverlays += "conf/features/max3010x.conf"
}
if ($EnablePpg) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_PPG" $true
    $featureOverlays += "conf/features/ppg.conf"
}
if ($EnableMax32664Update) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_MAX32664_UPDATE" $true
    $featureOverlays += "conf/features/max32664_update.conf"
}
if ($EnableDiagMonitor) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_DIAG_MONITOR" $true
    $featureOverlays += "conf/features/diag_monitor.conf"
}
if ($EnableRttLog) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_RTT_LOG" $true
    $featureOverlays += "conf/features/rtt_log.conf"
}
if ($EnableBleDfu) {
    Add-FeatureKconfigFlag "CONFIG_APP_FEATURE_BLE_DFU" $true
    $featureOverlays += "conf/features/ble_dfu.conf"
}

Write-Host "Core Overlays:" -ForegroundColor DarkGray
foreach ($overlay in $coreOverlays) {
    Write-Host "  - $overlay" -ForegroundColor DarkGray
}
if ($featureOverlays.Count -gt 0) {
    Write-Host "Feature Overlays:" -ForegroundColor Gray
    foreach ($overlay in $featureOverlays) {
        Write-Host "  - $overlay" -ForegroundColor Gray
    }
}
Write-Host ""

$cmakeArgs = @()
if ($featureKconfigFlags.Count -gt 0) {
    $cmakeArgs += $featureKconfigFlags
}
# Merge core overlays (always) + feature overlays (optional)
$allOverlays = $coreOverlays + $featureOverlays
if ($allOverlays.Count -gt 0) {
    # Sysbuild image-scoped overlay assignment: apply overlays to the main app image only.
    # This avoids leaking app-only driver config into MCUboot.
    $cmakeArgs += "-DNiSense_EXTRA_CONF_FILE=$($allOverlays -join ';')"
}

if ($cmakeArgs.Count -gt 0) {
    $westArgs += "--"
    $westArgs += $cmakeArgs
}

if ($Verbose) {
    $env:WEST_VERBOSE = "1"
}

Push-Location $ProjectRoot

try {
    Write-Host "Running west build..." -ForegroundColor Cyan
    # Run west using the toolchain's Python with proper environment from nrfutil-sdk-manager
    $westCommand = @("-m", "west") + $westArgs
    Write-Host "Command: python -m west $($westArgs -join ' ')" -ForegroundColor Gray
    Write-Host ""
    
    & $pythonPath $westCommand
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`n========================================" -ForegroundColor Green
        Write-Host "Build Successful!" -ForegroundColor Green
        Write-Host "========================================`n" -ForegroundColor Green
        
        $elfPath = Join-Path $BuildDir "NiSense\zephyr\zephyr.elf"
        $hexPath = Join-Path $BuildDir "NiSense\zephyr\zephyr.hex"
        $mergedHex = Join-Path $BuildDir "merged.hex"
        $mergedHexGenerated = Get-ChildItem -Path $BuildDir -File -Filter "merged_*.hex" -ErrorAction SilentlyContinue |
            Sort-Object Name |
            Select-Object -First 1
        $mapPath = Join-Path $BuildDir "NiSense\zephyr\zephyr.map"

        # Sysbuild now emits merged_<board_target>.hex. Keep merged.hex as a
        # compatibility alias for existing flash/debug scripts.
        if (-not (Test-Path $mergedHex) -and $mergedHexGenerated) {
            Copy-Item -Path $mergedHexGenerated.FullName -Destination $mergedHex -Force
            Write-Host "Created compatibility merged image alias: $mergedHex" -ForegroundColor DarkYellow
        }
        
        if (Test-Path $elfPath) {
            Write-Host "ELF: $elfPath" -ForegroundColor Cyan
        }
        if (Test-Path $hexPath) {
            Write-Host "HEX: $hexPath" -ForegroundColor Cyan
        }
        if ($mergedHexGenerated) {
            Write-Host "MERGED (sysbuild): $($mergedHexGenerated.FullName)" -ForegroundColor Green
        }
        if (Test-Path $mergedHex) {
            Write-Host "MERGED (compat alias): $mergedHex" -ForegroundColor Green
        }
        
        # Display memory usage from map file
        if (Test-Path $mapPath) {
            Write-Host "`n--- Memory Usage ---" -ForegroundColor Cyan
            $mapContent = Get-Content $mapPath -Raw
            if ($mapContent -match '__kernel_ram_size = [^\r\n]+') {
                # Extract RAM info
                $ramSizeMatch = [regex]::Match($mapContent, '__kernel_ram_size = 0x([0-9a-fA-F]+)')
                $imageRamMatch = [regex]::Match($mapContent, '_image_ram_size = 0x([0-9a-fA-F]+)')
                if ($ramSizeMatch.Success -and $imageRamMatch.Success) {
                    $kernelRam = [Convert]::ToInt32($ramSizeMatch.Groups[1].Value, 16)
                    $imageRam = [Convert]::ToInt32($imageRamMatch.Groups[1].Value, 16)
                    $usedKB = [math]::Round($imageRam / 1024, 1)
                    $totalKB = [math]::Round($kernelRam / 1024, 1)
                    $pct = [math]::Round(($imageRam / $kernelRam) * 100, 1)
                    Write-Host "RAM: $usedKB KB / $totalKB KB ($pct%)" -ForegroundColor $(if ($pct -gt 90) { "Yellow" } else { "Green" })
                    $freeKB = [math]::Round(($kernelRam - $imageRam) / 1024, 1)
                    Write-Host "RAM Free: $freeKB KB" -ForegroundColor $(if ($freeKB -lt 20) { "Yellow" } else { "Green" })
                }
            }
            # Get FLASH usage from ELF size
            if (Test-Path $elfPath) {
                $elfSize = (Get-Item $elfPath).Length
                Write-Host "ELF Size: $([math]::Round($elfSize / 1024, 1)) KB" -ForegroundColor Gray
            }
        }
        
        exit 0
    } else {
        Write-Host "`n========================================" -ForegroundColor Red
        Write-Host "Build Failed!" -ForegroundColor Red
        Write-Host "========================================`n" -ForegroundColor Red
        exit $LASTEXITCODE
    }
} finally {
    Pop-Location
}


