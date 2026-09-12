# =============================================================================
# nRF Connect SDK Setup Script
# =============================================================================
# This script detects existing nRF SDK installations, allows installation
# if needed, and configures the build environment for VS Code
# =============================================================================

param(
    [string]$InstallPath = "",
    [string]$BuildDirName = ""
)

# Load Windows Forms for GUI dialogs
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$ErrorActionPreference = "Stop"

# Configuration file location
$ConfigFile = Join-Path $PSScriptRoot "nrf_sdk_config.json"

# =============================================================================
# Helper Functions
# =============================================================================

function Show-InfoDialog {
    param([string]$Message, [string]$Title = "Information")
    [System.Windows.Forms.MessageBox]::Show($Message, $Title, [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information)
}

function Show-ErrorDialog {
    param([string]$Message, [string]$Title = "Error")
    [System.Windows.Forms.MessageBox]::Show($Message, $Title, [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Error)
}

function Show-FolderBrowserDialog {
    param(
        [string]$Description = "Select Folder",
        [string]$SelectedPath = ""
    )
    
    $dialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $dialog.Description = $Description
    $dialog.ShowNewFolderButton = $true
    
    if ($SelectedPath -and (Test-Path $SelectedPath)) {
        $dialog.SelectedPath = $SelectedPath
    }
    
    if ($dialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        return $dialog.SelectedPath
    }
    return $null
}

function Get-NrfSdkInstallations {
    $installations = @()
    
    # Common installation locations
    $commonPaths = @(
        "${env:ProgramFiles}\nrf\ncs",
        "${env:ProgramFiles(x86)}\nrf\ncs",
        "$env:LOCALAPPDATA\nrf\ncs",
        "C:\ncs",
        "C:\Nordic Semiconductor\nrfconnect-sdk",
        "D:\ncs",
        "E:\ncs"
    )
    
    # Check common paths
    foreach ($basePath in $commonPaths) {
        if (Test-Path $basePath) {
            # Look for version directories (v3.0.0, v2.6.0, etc.)
            $versionDirs = Get-ChildItem -Path $basePath -Directory -Filter "v*" -ErrorAction SilentlyContinue
            foreach ($versionDir in $versionDirs) {
                $sdkPath = $versionDir.FullName
                $version = $versionDir.Name
                
                # Check if this is a valid SDK directory
                if ((Test-Path (Join-Path $sdkPath "zephyr")) -and (Test-Path (Join-Path $sdkPath ".west"))) {
                    # Find toolchain
                    $toolchainPath = $null
                    $toolchainBase = Split-Path $sdkPath -Parent
                    if (Test-Path (Join-Path $toolchainBase "toolchains")) {
                        $toolchainDirs = Get-ChildItem -Path (Join-Path $toolchainBase "toolchains") -Directory -ErrorAction SilentlyContinue
                        if ($toolchainDirs) {
                            $toolchainPath = $toolchainDirs[0].FullName
                        }
                    }
                    
                    $installations += @{
                        SDKPath = $sdkPath
                        Version = $version
                        ToolchainPath = $toolchainPath
                        BasePath = $toolchainBase
                    }
                }
            }
        }
    }
    
    # Check environment variables
    if ($env:NCS_ROOT) {
        $sdkPath = $env:NCS_ROOT
        if (Test-Path $sdkPath) {
            $version = (Get-ChildItem -Path (Split-Path $sdkPath -Parent) -Directory -Filter "v*" | Where-Object { $_.FullName -eq $sdkPath } | Select-Object -First 1).Name
            if (-not $version) {
                $version = "Unknown"
            }
            $toolchainBase = Split-Path $sdkPath -Parent
            $toolchainPath = $null
            if (Test-Path (Join-Path $toolchainBase "toolchains")) {
                $toolchainDirs = Get-ChildItem -Path (Join-Path $toolchainBase "toolchains") -Directory -ErrorAction SilentlyContinue
                if ($toolchainDirs) {
                    $toolchainPath = $toolchainDirs[0].FullName
                }
            }
            
            $installations += @{
                SDKPath = $sdkPath
                Version = $version
                ToolchainPath = $toolchainPath
                BasePath = $toolchainBase
            }
        }
    }
    
    return $installations
}

function Get-SdkVersion {
    param([string]$SDKPath)
    
    # Try to read version from various sources
    $versionFile = Join-Path $SDKPath "VERSION"
    if (Test-Path $versionFile) {
        $content = Get-Content $versionFile -Raw
        if ($content -match 'v(\d+\.\d+\.\d+)') {
            return $matches[1]
        }
    }
    
    # Try to parse from path
    if ($SDKPath -match 'v(\d+\.\d+\.\d+)') {
        return $matches[1]
    }
    
    return "Unknown"
}

function Select-NrfSdkInstallation {
    $installations = Get-NrfSdkInstallations
    
    if ($installations.Count -eq 0) {
        Write-Host "No nRF Connect SDK installations found." -ForegroundColor Yellow
        return $null
    }
    
    if ($installations.Count -eq 1) {
        Write-Host "Found single installation: $($installations[0].SDKPath) (v$($installations[0].Version))" -ForegroundColor Green
        return $installations[0]
    }
    
    # Multiple installations - show selection dialog
    Write-Host "`nFound multiple nRF Connect SDK installations:" -ForegroundColor Cyan
    for ($i = 0; $i -lt $installations.Count; $i++) {
        Write-Host "  [$i] $($installations[$i].SDKPath) (v$($installations[$i].Version))" -ForegroundColor Gray
    }
    
    # Simple selection (can be enhanced with GUI)
    $selection = Read-Host "`nSelect installation (0-$($installations.Count-1)) or press Enter for first"
    
    if ([string]::IsNullOrWhiteSpace($selection)) {
        return $installations[0]
    }
    
    $index = [int]$selection
    if ($index -ge 0 -and $index -lt $installations.Count) {
        return $installations[$index]
    }
    
    return $installations[0]
}

function Install-NrfSdk {
    param([string]$InstallPath)
    
    Write-Host "`nTo install nRF Connect SDK, please:" -ForegroundColor Yellow
    Write-Host "1. Download nRF Connect SDK from https://www.nordicsemi.com/Software-and-tools/Software/nRF-Connect-SDK" -ForegroundColor Cyan
    Write-Host "2. Run the installer or use nRF Connect for Desktop" -ForegroundColor Cyan
    Write-Host "3. Run this script again after installation" -ForegroundColor Cyan
    
    $result = [System.Windows.Forms.MessageBox]::Show(
        "nRF Connect SDK is not installed.`n`nWould you like to open the download page?",
        "nRF SDK Not Found",
        [System.Windows.Forms.MessageBoxButtons]::YesNo,
        [System.Windows.Forms.MessageBoxIcon]::Question
    )
    
    if ($result -eq [System.Windows.Forms.DialogResult]::Yes) {
        Start-Process "https://www.nordicsemi.com/Software-and-tools/Software/nRF-Connect-SDK"
    }
    
    return $null
}

function Save-Configuration {
    param(
        [hashtable]$Config
    )
    
    $json = $Config | ConvertTo-Json -Depth 10
    $json | Out-File -FilePath $ConfigFile -Encoding UTF8
    Write-Host "Configuration saved to: $ConfigFile" -ForegroundColor Green
}

function Load-Configuration {
    if (Test-Path $ConfigFile) {
        $json = Get-Content $ConfigFile -Raw | ConvertFrom-Json
        return @{
            SDKPath = $json.SDKPath
            ToolchainPath = $json.ToolchainPath
            Version = $json.Version
            BuildDirName = $json.BuildDirName
        }
    }
    return $null
}

function Update-VSCodeConfig {
    param(
        [hashtable]$Config,
        [string]$BuildDirName
    )
    
    $buildDir = Join-Path (Split-Path $PSScriptRoot -Parent) $BuildDirName
    $nrfConfigFile = Join-Path $buildDir ".vscode-nrf-connect.json"
    
    # Ensure build directory exists
    if (-not (Test-Path $buildDir)) {
        New-Item -ItemType Directory -Path $buildDir -Force | Out-Null
    }
    
    # Create or update .vscode-nrf-connect.json
    $nrfConfig = @{
        toolchainPath = $Config.ToolchainPath
        sdkPath = $Config.SDKPath
        board = "raytac_mdbt50q_db_40/nrf52840"
        boardDir = Join-Path $Config.SDKPath "zephyr\boards\raytac\mdbt50q_db_40"
        boardDirs = @(
            Join-Path $Config.SDKPath "zephyr\boards\raytac\mdbt50q_db_40"
        )
        optimizations = "none"
        cmakeArgs = @()
        confFiles = @()
        buildDirectory = $BuildDirName
        kconfigFragments = @()
        snippets = @()
        dtcOverlays = @()
        extraDtcOverlays = @()
    }
    
    $nrfConfig | ConvertTo-Json -Depth 10 | Out-File -FilePath $nrfConfigFile -Encoding UTF8
    Write-Host "Updated nRF Connect extension config: $nrfConfigFile" -ForegroundColor Green
}

function Update-EnvironmentVariables {
    param([hashtable]$Config)
    
    # Set environment variables for current session
    $env:NCS_ROOT = $Config.SDKPath
    $env:ZEPHYR_BASE = Join-Path $Config.SDKPath "zephyr"
    $env:ZEPHYR_TOOLCHAIN_VARIANT = "zephyr"
    
    if ($Config.ToolchainPath) {
        $env:ZEPHYR_SDK_INSTALL_DIR = Join-Path $Config.ToolchainPath "opt\zephyr-sdk"
    }
    
    Write-Host "`nEnvironment variables for current session:" -ForegroundColor Cyan
    Write-Host "  NCS_ROOT = $env:NCS_ROOT" -ForegroundColor Gray
    Write-Host "  ZEPHYR_BASE = $env:ZEPHYR_BASE" -ForegroundColor Gray
}

# =============================================================================
# Main Script
# =============================================================================

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "nRF Connect SDK Setup" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# Try to load existing configuration
$existingConfig = Load-Configuration

# Get or select SDK installation
$sdkConfig = $null

if ($existingConfig -and (Test-Path $existingConfig.SDKPath)) {
    Write-Host "Found existing configuration:" -ForegroundColor Green
    Write-Host "  SDK: $($existingConfig.SDKPath)" -ForegroundColor Gray
    Write-Host "  Version: $($existingConfig.Version)" -ForegroundColor Gray
    
    $result = [System.Windows.Forms.MessageBox]::Show(
        "Use existing configuration?`n`nSDK: $($existingConfig.SDKPath)`nVersion: $($existingConfig.Version)",
        "Use Existing Configuration?",
        [System.Windows.Forms.MessageBoxButtons]::YesNo,
        [System.Windows.Forms.MessageBoxIcon]::Question
    )
    
    if ($result -eq [System.Windows.Forms.DialogResult]::Yes) {
        $sdkConfig = $existingConfig
    }
}

if (-not $sdkConfig) {
    $installation = Select-NrfSdkInstallation
    
    if (-not $installation) {
        $installation = Install-NrfSdk
    }
    
    if ($installation) {
        $sdkConfig = @{
            SDKPath = $installation.SDKPath
            ToolchainPath = $installation.ToolchainPath
            Version = $installation.Version
        }
    } else {
        Show-ErrorDialog "Could not find or configure nRF Connect SDK. Please install it first."
        exit 1
    }
}

# Get build directory name
if ([string]::IsNullOrWhiteSpace($BuildDirName)) {
    if ($existingConfig -and $existingConfig.BuildDirName) {
        $BuildDirName = $existingConfig.BuildDirName
    } else {
        $BuildDirName = Read-Host "`nBuild directory name (default: build_sdk_v330)"
        if ([string]::IsNullOrWhiteSpace($BuildDirName)) {
            $BuildDirName = "build_sdk_v330"
        }
    }
}

# Detect SEGGER paths (preserve existing if set, otherwise auto-detect)
$JLinkPath = if ($existingConfig -and $existingConfig.JLinkPath) { $existingConfig.JLinkPath } else {
    # Auto-detect JLink installation
    $defaultPaths = @("C:\Program Files\SEGGER\JLink", "C:\Program Files (x86)\SEGGER\JLink")
    $detected = $defaultPaths | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($detected) { $detected } else { "C:\Program Files\SEGGER\JLink" }
}
$OzonePath = if ($existingConfig -and $existingConfig.OzonePath) { $existingConfig.OzonePath } else {
    # Auto-detect Ozone installation
    $defaultPaths = @("C:\Program Files\SEGGER\Ozone", "C:\Program Files (x86)\SEGGER\Ozone")
    $detected = $defaultPaths | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($detected) { $detected } else { "C:\Program Files\SEGGER\Ozone" }
}

# Save configuration
$configToSave = @{
    SDKPath = $sdkConfig.SDKPath
    ToolchainPath = $sdkConfig.ToolchainPath
    Version = $sdkConfig.Version
    BuildDirName = $BuildDirName
    JLinkPath = $JLinkPath
    OzonePath = $OzonePath
}

Save-Configuration -Config $configToSave
Update-VSCodeConfig -Config $sdkConfig -BuildDirName $BuildDirName
Update-EnvironmentVariables -Config $sdkConfig

# Update VS Code launch.json and tasks.json
Write-Host "Updating VS Code configuration files..." -ForegroundColor Cyan
$updateScript = Join-Path $PSScriptRoot "update_vscode_config.ps1"
if (Test-Path $updateScript) {
    & $updateScript
}

Write-Host "`n========================================" -ForegroundColor Green
Write-Host "Setup Complete!" -ForegroundColor Green
Write-Host "========================================`n" -ForegroundColor Green
Write-Host "SDK Path: $($sdkConfig.SDKPath)" -ForegroundColor Cyan
Write-Host "Toolchain Path: $($sdkConfig.ToolchainPath)" -ForegroundColor Cyan
Write-Host "Version: $($sdkConfig.Version)" -ForegroundColor Cyan
Write-Host "Build Directory: $BuildDirName" -ForegroundColor Cyan
Write-Host "JLink Path: $JLinkPath" -ForegroundColor Cyan
Write-Host "Ozone Path: $OzonePath" -ForegroundColor Cyan
Write-Host "Build Directory: $BuildDirName" -ForegroundColor Cyan

Show-InfoDialog "nRF Connect SDK setup complete!`n`nSDK: $($sdkConfig.SDKPath)`nVersion: $($sdkConfig.Version)`n`nVS Code tasks and launch configurations will be updated automatically."


