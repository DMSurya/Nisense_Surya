# =============================================================================
# Ozone Launcher Script
# =============================================================================
# Launches Ozone debugger, auto-opens project if not running,
# refreshes if already running
# =============================================================================

param(
    [switch]$RefreshOnly,
    [string]$ProjectFile = ""
)

$ErrorActionPreference = "Continue"

# Get project root - go up 2 levels from scripts/setup to project root
$ProjectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$ConfigFile = Join-Path $PSScriptRoot "nrf_sdk_config.json"

# Load configuration
$buildDir = "build_sdk_v330"
$OzonePath = "C:\Program Files\SEGGER\Ozone"
if (Test-Path $ConfigFile) {
    $config = Get-Content $ConfigFile -Raw | ConvertFrom-Json
    if ($config.BuildDirName) {
        $buildDir = $config.BuildDirName
    }
    if ($config.OzonePath) {
        $OzonePath = $config.OzonePath
    }
}
$OzoneExe = Join-Path $OzonePath "Ozone.exe"

# Default project file location
if ([string]::IsNullOrWhiteSpace($ProjectFile)) {
    $ProjectFile = Join-Path $ProjectRoot ".vscode\ozone_project.jdebug"
}

# Check if Ozone is installed
if (-not (Test-Path $OzoneExe)) {
    Write-Host "Error: Ozone not found at $OzoneExe" -ForegroundColor Red
    Write-Host "Please install SEGGER Ozone or update the path in this script." -ForegroundColor Yellow
    exit 1
}

# Find Ozone process
$ozoneProcess = Get-Process -Name "Ozone" -ErrorAction SilentlyContinue

function Create-OzoneProject {
    param([string]$OutFile)
    
    $elfFile = Join-Path $ProjectRoot "$buildDir\NiSense\zephyr\zephyr.elf"
    
    # Path to merged.hex (MCUboot + Application) for flashing
    $mergedHex = Join-Path $ProjectRoot "$buildDir\merged.hex"
    
    if (-not (Test-Path $elfFile)) {
        Write-Host "Error: ELF file not found at $elfFile" -ForegroundColor Red
        Write-Host "Please build the project first." -ForegroundColor Yellow
        exit 1
    }
    
    if (-not (Test-Path $mergedHex)) {
        Write-Host "Error: merged.hex not found at $mergedHex" -ForegroundColor Red
        Write-Host "Please build the project first." -ForegroundColor Yellow
        exit 1
    }
    
    $projectContent = @"
<?xml version="1.0" encoding="UTF-8"?>
<project version="2.0">
    <Session>
        <Target>
            <TargetInterface>J-Link</TargetInterface>
            <TargetDevice>nRF52840_xxAA</TargetDevice>
            <TargetInterfaceSettings>
                <TargetInterfaceSettings>
                    <Connector>SWD</Connector>
                    <Speed>4000</Speed>
                    <ResetMode>Normal</ResetMode>
                </TargetInterfaceSettings>
            </TargetInterfaceSettings>
            <Flash>
                <FlashDevice>
                    <DeviceName>nRF52840_xxAA</DeviceName>
                </FlashDevice>
            </Flash>
            <RegisterFile>
                <RegisterFile>
                    <Name>Cortex-M4</Name>
                    <FileName>$(Join-Path $PSScriptRoot "Cortex-M4.svd")</FileName>
                </RegisterFile>
            </RegisterFile>
        </Target>
        <Project>
            <ProjectName>$($buildDir)</ProjectName>
            <Files>
                <File>
                    <FileName>$elfFile</FileName>
                    <FileType>3</FileType>
                </File>
            </Files>
            <Breakpoints>
                <BreakpointList />
            </Breakpoints>
            <WatchExpressions>
                <WatchExpressionList />
            </WatchExpressions>
        </Project>
        <Program>
            <ProgramFile>$mergedHex</ProgramFile>
            <FlashDownload>
                <FlashDownload>
                    <EraseChip>true</EraseChip>
                    <EraseSectors>false</EraseSectors>
                </FlashDownload>
            </FlashDownload>
            <ResetAndRun>true</ResetAndRun>
            <StopOnReset>true</StopOnReset>
            <VerifyDownload>true</VerifyDownload>
        </Program>
        <RTT>
            <RTT>
                <RTTEnable>true</RTTEnable>
                <RTTSearchRanges>
                    <RTTSearchRanges>
                        <StartAddress>0x20000000</StartAddress>
                        <Size>0x40000</Size>
                    </RTTSearchRanges>
                </RTTSearchRanges>
                <RTTBufferSizeUp>8192</RTTBufferSizeUp>
                <RTTBufferSizeDown>64</RTTBufferSizeDown>
                <RTTChannels>
                    <RTTChannel>
                        <ChannelNumber>0</ChannelNumber>
                        <Name>Logs</Name>
                        <Color>0xFF00FF00</Color>
                    </RTTChannel>
                    <RTTChannel>
                        <ChannelNumber>1</ChannelNumber>
                        <Name>Shell</Name>
                        <Color>0xFFFF0000</Color>
                    </RTTChannel>
                </RTTChannels>
            </RTT>
        </RTT>
    </Session>
</project>
"@
    
    $projectDir = Split-Path $OutFile -Parent
    if (-not (Test-Path $projectDir)) {
        New-Item -ItemType Directory -Path $projectDir -Force | Out-Null
    }
    
    $projectContent | Out-File -FilePath $OutFile -Encoding UTF8
    Write-Host "Created Ozone project file: $OutFile" -ForegroundColor Green
}

# Create project file if it doesn't exist
if (-not (Test-Path $ProjectFile)) {
    Write-Host "Creating Ozone project file..." -ForegroundColor Cyan
    Create-OzoneProject -OutFile $ProjectFile
}

# Update project file if ELF changed
$elfFile = Join-Path $ProjectRoot "$buildDir\NiSense\zephyr\zephyr.elf"
if (Test-Path $elfFile) {
    $elfTime = (Get-Item $elfFile).LastWriteTime
    $projTime = if (Test-Path $ProjectFile) { (Get-Item $ProjectFile).LastWriteTime } else { [DateTime]::MinValue }
    
    if ($elfTime -gt $projTime) {
        Write-Host "Updating Ozone project file (ELF changed)..." -ForegroundColor Cyan
        Create-OzoneProject -OutFile $ProjectFile
    }
}

# Launch or refresh Ozone
if ($ozoneProcess) {
    Write-Host "Ozone is already running (PID: $($ozoneProcess.Id))" -ForegroundColor Yellow
    
    if ($RefreshOnly) {
        Write-Host "Refreshing Ozone project..." -ForegroundColor Cyan
        # Ozone can be refreshed by opening the project file again
        # We'll launch it with the project file, which should reload it
        Start-Process -FilePath $OzoneExe -ArgumentList "`"$ProjectFile`"" -ErrorAction SilentlyContinue
    } else {
        # Bring Ozone window to front
        Add-Type @"
            using System;
            using System.Runtime.InteropServices;
            public class Win32 {
                [DllImport("user32.dll")]
                public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
                [DllImport("user32.dll")]
                public static extern bool SetForegroundWindow(IntPtr hWnd);
                public static int SW_RESTORE = 9;
            }
"@
        [Win32]::ShowWindow($ozoneProcess.MainWindowHandle, [Win32]::SW_RESTORE)
        [Win32]::SetForegroundWindow($ozoneProcess.MainWindowHandle)
        Write-Host "Brought Ozone window to front." -ForegroundColor Green
        
        # Also refresh project by opening it again
        Start-Sleep -Milliseconds 500
        Start-Process -FilePath $OzoneExe -ArgumentList "`"$ProjectFile`"" -ErrorAction SilentlyContinue
    }
} else {
    Write-Host "Launching Ozone..." -ForegroundColor Cyan
    Start-Process -FilePath $OzoneExe -ArgumentList "`"$ProjectFile`""
    Write-Host "Ozone launched with project: $ProjectFile" -ForegroundColor Green
}

Write-Host "`nOzone ready for debugging!" -ForegroundColor Green
Write-Host "Project file: $ProjectFile" -ForegroundColor Gray
Write-Host "ELF file: $elfFile" -ForegroundColor Gray


