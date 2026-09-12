# =============================================================================
# New Project Setup Script
# =============================================================================
# Creates a new nRF Connect SDK project with drivers and required files
# =============================================================================

param(
    [string]$ProjectPath = "",
    [string]$ProjectName = "",
    [string]$Board = "raytac_mdbt50q_db_40/nrf52840"
)

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$ErrorActionPreference = "Stop"

# Note: Template structure is copied directly from source directory
# No need to define structure separately

# =============================================================================
# Helper Functions
# =============================================================================

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

function Get-InputDialog {
    param(
        [string]$Title,
        [string]$Prompt,
        [string]$DefaultValue = ""
    )
    
    $form = New-Object System.Windows.Forms.Form
    $form.Text = $Title
    $form.Size = New-Object System.Drawing.Size(400, 150)
    $form.StartPosition = "CenterScreen"
    $form.FormBorderStyle = "FixedDialog"
    $form.MaximizeBox = $false
    $form.MinimizeBox = $false
    
    $label = New-Object System.Windows.Forms.Label
    $label.Location = New-Object System.Drawing.Point(10, 10)
    $label.Size = New-Object System.Drawing.Size(360, 20)
    $label.Text = $Prompt
    $form.Controls.Add($label)
    
    $textBox = New-Object System.Windows.Forms.TextBox
    $textBox.Location = New-Object System.Drawing.Point(10, 35)
    $textBox.Size = New-Object System.Drawing.Size(360, 20)
    $textBox.Text = $DefaultValue
    $form.Controls.Add($textBox)
    
    $okButton = New-Object System.Windows.Forms.Button
    $okButton.Location = New-Object System.Drawing.Point(200, 70)
    $okButton.Size = New-Object System.Drawing.Size(75, 23)
    $okButton.Text = "OK"
    $okButton.DialogResult = [System.Windows.Forms.DialogResult]::OK
    $form.AcceptButton = $okButton
    $form.Controls.Add($okButton)
    
    $cancelButton = New-Object System.Windows.Forms.Button
    $cancelButton.Location = New-Object System.Drawing.Point(285, 70)
    $cancelButton.Size = New-Object System.Drawing.Size(75, 23)
    $cancelButton.Text = "Cancel"
    $cancelButton.DialogResult = [System.Windows.Forms.DialogResult]::Cancel
    $form.CancelButton = $cancelButton
    $form.Controls.Add($cancelButton)
    
    if ($form.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        return $textBox.Text
    }
    return $null
}

function Copy-TemplateFiles {
    param(
        [string]$SourceDir,
        [string]$DestDir
    )
    
    if (-not (Test-Path $SourceDir)) {
        Write-Warning "Template directory not found: $SourceDir"
        return $false
    }
    
    # Copy directory structure
    $items = Get-ChildItem -Path $SourceDir -Recurse -File
    
    foreach ($item in $items) {
        $relativePath = $item.FullName.Substring($SourceDir.Length + 1)
        $destPath = Join-Path $DestDir $relativePath
        $destParent = Split-Path $destPath -Parent
        
        if (-not (Test-Path $destParent)) {
            New-Item -ItemType Directory -Path $destParent -Force | Out-Null
        }
        
        Copy-Item -Path $item.FullName -Destination $destPath -Force
        Write-Host "  Copied: $relativePath" -ForegroundColor Gray
    }
    
    return $true
}

function Create-CMakeLists {
    param([string]$ProjectPath, [string]$ProjectName)
    
    $cmakeContent = @"
cmake_minimum_required(VERSION 3.20.0)

# Acknowledge NCS_TOOLCHAIN_VERSION if set (from nRF Connect SDK)
if(NCS_TOOLCHAIN_VERSION)
  # Variable acknowledged, no action needed
endif()

# CRITICAL: Tell Zephyr device tree compiler where to find custom bindings
# This must be set BEFORE find_package(Zephyr)
list(APPEND DTS_ROOT "`${CMAKE_CURRENT_SOURCE_DIR}/dts")

# Add custom drivers to Zephyr's driver path
list(APPEND ZEPHYR_EXTRA_MODULES "`${CMAKE_CURRENT_SOURCE_DIR}/drivers")

find_package(Zephyr REQUIRED HINTS `$ENV{ZEPHYR_BASE})

project($ProjectName)

target_sources(app PRIVATE 
    src/main.c
)

# Add include directory for custom headers
target_include_directories(app PRIVATE `${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add RTT debug definitions
target_compile_definitions(app PRIVATE
    -DRTT_USE_ASM=0
    -DSEGGER_RTT_SECTION=".rtt_buff_data"
)
"@
    
    $cmakePath = Join-Path $ProjectPath "CMakeLists.txt"
    $cmakeContent | Out-File -FilePath $cmakePath -Encoding UTF8
}

function Create-PrjConf {
    param([string]$ProjectPath)
    
    $prjConfContent = @"
# Basic project configuration
CONFIG_DEBUG=y
CONFIG_CONSOLE=y
CONFIG_RTT_CONSOLE=y
CONFIG_UART_CONSOLE=n

# Logging
CONFIG_LOG=y
CONFIG_LOG_MODE_IMMEDIATE=y
CONFIG_LOG_DEFAULT_LEVEL=4
CONFIG_LOG_BACKEND_RTT=y
CONFIG_LOG_BACKEND_UART=n

# RTT Configuration
CONFIG_USE_SEGGER_RTT=y
CONFIG_SEGGER_RTT_MAX_NUM_UP_BUFFERS=4
CONFIG_SEGGER_RTT_MAX_NUM_DOWN_BUFFERS=4
CONFIG_SEGGER_RTT_BUFFER_SIZE_UP=8192
CONFIG_SEGGER_RTT_BUFFER_SIZE_DOWN=64
CONFIG_LOG_BACKEND_RTT_BUFFER=0

# Core subsystems
CONFIG_I2C=y
CONFIG_GPIO=y
CONFIG_SPI=y
CONFIG_ADC=y
"@
    
    $prjConfPath = Join-Path $ProjectPath "prj.conf"
    $prjConfContent | Out-File -FilePath $prjConfPath -Encoding UTF8
}

function Create-MainC {
    param([string]$ProjectPath)
    
    $mainContent = @"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

int main(void)
{
    LOG_INF("Application started");
    printk("Hello from $ProjectName!`n");
    
    while (1) {
        k_sleep(K_SECONDS(1));
        LOG_DBG("Heartbeat");
    }
    
    return 0;
}
"@
    
    $mainPath = Join-Path $ProjectPath "src\main.c"
    if (-not (Test-Path (Split-Path $mainPath -Parent))) {
        New-Item -ItemType Directory -Path (Split-Path $mainPath -Parent) -Force | Out-Null
    }
    $mainContent | Out-File -FilePath $mainPath -Encoding UTF8
}

function Setup-VSCodeConfig {
    param(
        [string]$ProjectPath,
        [string]$ProjectName,
        [string]$Board,
        [string]$SdkPath,
        [string]$ToolchainPath
    )
    
    $vscodeDir = Join-Path $ProjectPath ".vscode"
    if (-not (Test-Path $vscodeDir)) {
        New-Item -ItemType Directory -Path $vscodeDir -Force | Out-Null
    }
    
    # Copy .vscode files from template (current project)
    $templateVscode = Join-Path $PSScriptRoot ".."
    if (Test-Path (Join-Path $templateVscode ".vscode")) {
        Write-Host "Copying VS Code configuration..." -ForegroundColor Cyan
        Copy-Item -Path (Join-Path $templateVscode ".vscode\*") -Destination $vscodeDir -Recurse -Force
    }
    
    # Update paths in tasks.json and launch.json (if they exist)
    # This would require parsing and updating JSON, simplified here
    Write-Host "  VS Code configuration copied" -ForegroundColor Gray
}

# =============================================================================
# Main Script
# =============================================================================

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "New nRF Connect SDK Project Setup" -ForegroundColor Cyan
Write-Host "========================================`n" -ForegroundColor Cyan

# Get project name
if ([string]::IsNullOrWhiteSpace($ProjectName)) {
    $ProjectName = Get-InputDialog -Title "New Project" -Prompt "Enter project name:" -DefaultValue "MyProject"
    if ([string]::IsNullOrWhiteSpace($ProjectName)) {
        Write-Host "Project creation cancelled." -ForegroundColor Yellow
        exit 0
    }
}

# Get project path
if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = Show-FolderBrowserDialog -Description "Select parent directory for new project" -SelectedPath $PWD.Path
    if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
        Write-Host "Project creation cancelled." -ForegroundColor Yellow
        exit 0
    }
    $ProjectPath = Join-Path $ProjectPath $ProjectName
}

# Confirm creation
$result = [System.Windows.Forms.MessageBox]::Show(
    "Create project:`n`nName: $ProjectName`nPath: $ProjectPath`nBoard: $Board",
    "Confirm Project Creation",
    [System.Windows.Forms.MessageBoxButtons]::YesNo,
    [System.Windows.Forms.MessageBoxIcon]::Question
)

if ($result -ne [System.Windows.Forms.DialogResult]::Yes) {
    Write-Host "Project creation cancelled." -ForegroundColor Yellow
    exit 0
}

# Create project directory
Write-Host "Creating project structure..." -ForegroundColor Cyan
if (Test-Path $ProjectPath) {
    $overwrite = [System.Windows.Forms.MessageBox]::Show(
        "Directory already exists. Overwrite?",
        "Directory Exists",
        [System.Windows.Forms.MessageBoxButtons]::YesNo,
        [System.Windows.Forms.MessageBoxIcon]::Warning
    )
    if ($overwrite -eq [System.Windows.Forms.DialogResult]::No) {
        exit 0
    }
    Remove-Item -Path $ProjectPath -Recurse -Force
}

New-Item -ItemType Directory -Path $ProjectPath -Force | Out-Null

# Load nRF SDK config
$nrfConfigFile = Join-Path $PSScriptRoot "nrf_sdk_config.json"
$nrfConfig = $null
if (Test-Path $nrfConfigFile) {
    $nrfConfig = Get-Content $nrfConfigFile -Raw | ConvertFrom-Json
}

# Copy template files from current project
$templateDir = Split-Path $PSScriptRoot -Parent
Write-Host "Copying template files..." -ForegroundColor Cyan

# Copy drivers
if (Test-Path (Join-Path $templateDir "drivers")) {
    Copy-Item -Path (Join-Path $templateDir "drivers") -Destination (Join-Path $ProjectPath "drivers") -Recurse -Force
    Write-Host "  Copied drivers" -ForegroundColor Gray
}

# Copy dts bindings
if (Test-Path (Join-Path $templateDir "dts")) {
    Copy-Item -Path (Join-Path $templateDir "dts") -Destination (Join-Path $ProjectPath "dts") -Recurse -Force
    Write-Host "  Copied DTS bindings" -ForegroundColor Gray
}

# Copy include directory
if (Test-Path (Join-Path $templateDir "include")) {
    Copy-Item -Path (Join-Path $templateDir "include") -Destination (Join-Path $ProjectPath "include") -Recurse -Force
    Write-Host "  Copied includes" -ForegroundColor Gray
}

# Copy boards if exists
if (Test-Path (Join-Path $templateDir "boards")) {
    Copy-Item -Path (Join-Path $templateDir "boards") -Destination (Join-Path $ProjectPath "boards") -Recurse -Force
    Write-Host "  Copied boards" -ForegroundColor Gray
}

# Create basic project files
Write-Host "Creating project files..." -ForegroundColor Cyan
Create-CMakeLists -ProjectPath $ProjectPath -ProjectName $ProjectName
Create-PrjConf -ProjectPath $ProjectPath
Create-MainC -ProjectPath $ProjectPath

# Create Kconfig
$kconfigContent = @"
source "Kconfig.zephyr"
"@
$kconfigContent | Out-File -FilePath (Join-Path $ProjectPath "Kconfig") -Encoding UTF8

# Setup VS Code configuration
if ($nrfConfig) {
    Setup-VSCodeConfig -ProjectPath $ProjectPath -ProjectName $ProjectName -Board $Board -SdkPath $nrfConfig.SDKPath -ToolchainPath $nrfConfig.ToolchainPath
} else {
    Write-Host "Warning: nRF SDK configuration not found. Run setup_nrf_sdk.ps1 first." -ForegroundColor Yellow
}

Write-Host "`n========================================" -ForegroundColor Green
Write-Host "Project Created Successfully!" -ForegroundColor Green
Write-Host "========================================`n" -ForegroundColor Green
Write-Host "Project: $ProjectName" -ForegroundColor Cyan
Write-Host "Path: $ProjectPath" -ForegroundColor Cyan
Write-Host "Board: $Board" -ForegroundColor Cyan

[System.Windows.Forms.MessageBox]::Show(
    "Project created successfully!`n`nPath: $ProjectPath`n`nNext steps:`n1. Open the project in VS Code`n2. Run 'Setup nRF SDK' task if needed`n3. Build the project",
    "Project Created",
    [System.Windows.Forms.MessageBoxButtons]::OK,
    [System.Windows.Forms.MessageBoxIcon]::Information
)


