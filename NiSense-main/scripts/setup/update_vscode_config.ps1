# =============================================================================
# Update VS Code Configuration Script
# =============================================================================
# Updates launch.json and tasks.json with paths from nRF SDK configuration
# =============================================================================

param()

$ErrorActionPreference = "Continue"

$ConfigFile = Join-Path $PSScriptRoot "nrf_sdk_config.json"
$LaunchJson = Join-Path $PSScriptRoot "launch.json"

if (-not (Test-Path $ConfigFile)) {
    Write-Host "Warning: nRF SDK configuration not found. Using defaults." -ForegroundColor Yellow
    Write-Host "Run 'Setup nRF SDK' task to configure paths." -ForegroundColor Yellow
    exit 0
}

$config = Get-Content $ConfigFile -Raw | ConvertFrom-Json
$buildDir = if ($config.BuildDirName) { $config.BuildDirName } else { "build_sdk_v330" }

if (-not (Test-Path $LaunchJson)) {
    Write-Host "Error: launch.json not found" -ForegroundColor Red
    exit 1
}

# Read launch.json
$launchContent = Get-Content $LaunchJson -Raw | ConvertFrom-Json

# Update paths in each configuration
foreach ($configItem in $launchContent.configurations) {
    # Update program path
    if ($configItem.program) {
        $configItem.program = $configItem.program -replace "build_sdk_v300", $buildDir
        $configItem.program = $configItem.program -replace "build_sdk_v322", $buildDir
        $configItem.program = $configItem.program -replace "build_sdk_v330", $buildDir
        $configItem.program = $configItem.program -replace '\$\{config:nrfConnect\.buildDirectory\}', $buildDir
    }
    
    # Update GDB path
    if ($configItem.miDebuggerPath -and $config.ToolchainPath) {
        $gdbPath = Join-Path $config.ToolchainPath "opt\zephyr-sdk\arm-zephyr-eabi\bin\arm-zephyr-eabi-gdb.exe"
        $gdbPath = $gdbPath -replace '\\', '/'
        
        # Replace config variable or default path
        if ($configItem.miDebuggerPath -match '\$\{config:') {
            $configItem.miDebuggerPath = $gdbPath
        } elseif ($configItem.miDebuggerPath -match 'C:/ncs/toolchains/') {
            $configItem.miDebuggerPath = $gdbPath
        }
    }
}

# Convert back to JSON with proper formatting
$jsonContent = $launchContent | ConvertTo-Json -Depth 10

# Pretty-print JSON (basic formatting)
$jsonContent = $jsonContent -replace '([{,])"', '$1"'
$jsonContent = $jsonContent -replace '"([,}])', '"$1'
$jsonContent = $jsonContent -replace '":', '": '
$jsonContent = $jsonContent -replace ',\s*', ",`n"

# Write back
$jsonContent | Out-File -FilePath $LaunchJson -Encoding UTF8 -NoNewline
Write-Host "Updated launch.json with paths from configuration" -ForegroundColor Green

# =============================================================================
# Update tasks.json with JLink paths
# =============================================================================
$TasksJson = Join-Path $PSScriptRoot "tasks.json"
if (Test-Path $TasksJson) {
    $JLinkPath = if ($config.JLinkPath) { $config.JLinkPath -replace '\\', '/' } else { "C:/Program Files/SEGGER/JLink" }
    
    # Read as text and replace paths
    $tasksContent = Get-Content $TasksJson -Raw
    
    # Replace JLink paths (handle both forward and backslash variants)
    $tasksContent = $tasksContent -replace 'C:/Program Files/SEGGER/JLink', $JLinkPath
    $tasksContent = $tasksContent -replace 'C:/Program Files \(x86\)/SEGGER/JLink', $JLinkPath
    $tasksContent = $tasksContent -replace 'C:\\Program Files\\SEGGER\\JLink', ($JLinkPath -replace '/', '\\')
    
    # Write back
    $tasksContent | Out-File -FilePath $TasksJson -Encoding UTF8 -NoNewline
    Write-Host "Updated tasks.json with JLink paths from configuration" -ForegroundColor Green
}


