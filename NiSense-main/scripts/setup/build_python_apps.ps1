# =============================================================================
# Build Python Applications (Standalone EXE)
# =============================================================================
# Packages runnable Python applications in this repo as standalone executables
# using PyInstaller.
#
# Examples:
#   .\scripts\setup\build_python_apps.ps1 -InstallDeps
#   .\scripts\setup\build_python_apps.ps1 -InstallDeps -Clean
#   .\scripts\setup\build_python_apps.ps1 -OnlyApps hcm_monitor,nisense_analyzer
#   .\scripts\setup\build_python_apps.ps1 -ListApps
# =============================================================================

param(
    [switch]$InstallDeps,
    [switch]$Clean,
    [switch]$ListApps,
    [string[]]$OnlyApps,
    [string]$OutputDir = "dist_python"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = (Get-Item $PSScriptRoot).Parent.Parent.FullName
$DefaultPython = Join-Path $ProjectRoot ".venv\Scripts\python.exe"

if (Test-Path $DefaultPython) {
    $PythonExe = $DefaultPython
} else {
    $PythonCmd = Get-Command python -ErrorAction SilentlyContinue
    if (-not $PythonCmd) {
        Write-Host "Error: Python not found. Activate a venv or install Python first." -ForegroundColor Red
        exit 1
    }
    $PythonExe = $PythonCmd.Source
}

$DistRoot = Join-Path $ProjectRoot $OutputDir
$BuildRoot = Join-Path $ProjectRoot "build_python_apps"
$SpecRoot = Join-Path $ProjectRoot "build_python_specs"

function Test-PythonModule {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ModuleName
    )

    & $PythonExe -c "import importlib.util,sys; sys.exit(0 if importlib.util.find_spec('$ModuleName') else 1)"
    return ($LASTEXITCODE -eq 0)
}

$apps = @(
    @{
        Name = "hcm_monitor"
        Entry = "apps/pc/hcm_monitor.py"
        Windowed = $true
        OneFile = $true
        AddData = @("apps/pc/qml;qml")
        HiddenImports = @(
            "qasync",
            "bleak.backends.winrt.client",
            "bleak.backends.winrt.scanner"
        )
        CollectAll = @("PySide6", "shiboken6")
    },
    @{
        Name = "nisense_analyzer"
        Entry = "scripts/tools/nisense_analyzer.py"
        Windowed = $true
        OneFile = $true
        AddData = @()
        HiddenImports = @()
        CollectAll = @("PySide6", "shiboken6", "matplotlib")
    },
    @{
        Name = "plot_ppg_runs"
        Entry = "scripts/tools/plot_ppg_runs.py"
        Windowed = $false
        OneFile = $true
        AddData = @()
        HiddenImports = @()
        CollectAll = @("matplotlib")
    },
    @{
        Name = "rom_breakdown_lvgl"
        Entry = "scripts/tools/rom_breakdown_lvgl.py"
        Windowed = $false
        OneFile = $true
        AddData = @()
        HiddenImports = @()
        CollectAll = @()
    },
    @{
        Name = "summarize_rom_json"
        Entry = "scripts/tools/summarize_rom_json.py"
        Windowed = $false
        OneFile = $true
        AddData = @()
        HiddenImports = @()
        CollectAll = @()
    },
    @{
        Name = "extract_matrices"
        Entry = "scripts/extract_matrices.py"
        Windowed = $false
        OneFile = $true
        AddData = @()
        HiddenImports = @()
        CollectAll = @()
    }
)

if ($ListApps) {
    Write-Host "Available Python apps:" -ForegroundColor Cyan
    foreach ($app in $apps) {
        Write-Host (" - {0} ({1})" -f $app.Name, $app.Entry)
    }
    exit 0
}

if ($OnlyApps -and $OnlyApps.Count -gt 0) {
    $selectedNames = @{}
    foreach ($name in $OnlyApps) {
        if (-not [string]::IsNullOrWhiteSpace($name)) {
            $selectedNames[$name.Trim().ToLowerInvariant()] = $true
        }
    }

    $apps = @($apps | Where-Object {
        $selectedNames.ContainsKey($_.Name.ToLowerInvariant())
    })

    if ($apps.Count -eq 0) {
        Write-Host "Error: none of the -OnlyApps names matched." -ForegroundColor Red
        Write-Host "Tip: run with -ListApps to see valid app names." -ForegroundColor Yellow
        exit 1
    }
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build Python Apps (Standalone EXE)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ("Project Root : {0}" -f $ProjectRoot) -ForegroundColor Gray
Write-Host ("Python       : {0}" -f $PythonExe) -ForegroundColor Gray
Write-Host ("Output       : {0}" -f $DistRoot) -ForegroundColor Gray
Write-Host ("Apps         : {0}" -f (($apps | ForEach-Object { $_.Name }) -join ", ")) -ForegroundColor Gray
Write-Host ""

if ($InstallDeps) {
    Write-Host "Installing Python dependencies..." -ForegroundColor Cyan

    # Check pip availability by inspecting output string (LASTEXITCODE can be unreliable
    # when 2>&1 captures ErrorRecord objects on some PS versions).
    $pipVersion = (& $PythonExe -m pip --version 2>&1) | Out-String
    if ($pipVersion -notmatch "pip \d") {
        Write-Host "pip is not available in this Python environment." -ForegroundColor Red
        Write-Host ("Interpreter: {0}" -f $PythonExe) -ForegroundColor Red
        Write-Host ""
        Write-Host "To fix: activate your venv and run:" -ForegroundColor Yellow
        Write-Host "  python -m ensurepip --upgrade" -ForegroundColor Yellow
        Write-Host "  python -m pip install --upgrade pip" -ForegroundColor Yellow
        Write-Host "Then retry this script with -InstallDeps." -ForegroundColor Yellow
        exit 1
    }

    & $PythonExe -m pip install --upgrade pip setuptools wheel pyinstaller
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $BleReq = Join-Path $ProjectRoot "scripts\ble\requirements.txt"
    if (Test-Path $BleReq) {
        & $PythonExe -m pip install -r $BleReq
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }

    # Analyzer/plotter dependencies
    & $PythonExe -m pip install matplotlib pandas
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host "Dependency install completed." -ForegroundColor Green
    Write-Host ""
}

if (-not (Test-PythonModule -ModuleName "PyInstaller")) {
    Write-Host "Error: PyInstaller is not installed for this interpreter:" -ForegroundColor Red
    Write-Host ("  {0}" -f $PythonExe) -ForegroundColor Red
    Write-Host "Run again with -InstallDeps to install required packaging dependencies." -ForegroundColor Yellow
    Write-Host "Example:" -ForegroundColor Yellow
    Write-Host "  .\scripts\setup\build_python_apps.ps1 -InstallDeps" -ForegroundColor Yellow
    exit 1
}

if (-not (Test-Path $DistRoot)) { New-Item -ItemType Directory -Path $DistRoot | Out-Null }
if (-not (Test-Path $BuildRoot)) { New-Item -ItemType Directory -Path $BuildRoot | Out-Null }
if (-not (Test-Path $SpecRoot)) { New-Item -ItemType Directory -Path $SpecRoot | Out-Null }

$failures = @()

foreach ($app in $apps) {
    $entryAbs = Join-Path $ProjectRoot $app.Entry
    if (-not (Test-Path $entryAbs)) {
        Write-Host ("Skipping {0}: missing entry file {1}" -f $app.Name, $app.Entry) -ForegroundColor Yellow
        continue
    }

    Write-Host ("[BUILD] {0}" -f $app.Name) -ForegroundColor Cyan

    if ($Clean) {
        $oldExe = Join-Path $DistRoot ($app.Name + ".exe")
        $oldFolder = Join-Path $DistRoot $app.Name
        $oldSpec = Join-Path $SpecRoot ($app.Name + ".spec")
        if (Test-Path $oldExe) { Remove-Item -Force $oldExe }
        if (Test-Path $oldFolder) { Remove-Item -Recurse -Force $oldFolder }
        if (Test-Path $oldSpec) { Remove-Item -Force $oldSpec }
    }

    $pyArgs = @(
        "-m", "PyInstaller",
        "--noconfirm",
        "--name", $app.Name,
        "--distpath", $DistRoot,
        "--workpath", (Join-Path $BuildRoot $app.Name),
        "--specpath", $SpecRoot
    )

    if ($Clean) { $pyArgs += "--clean" }
    if ($app.OneFile) { $pyArgs += "--onefile" }
    if ($app.Windowed) { $pyArgs += "--windowed" } else { $pyArgs += "--console" }

    foreach ($item in $app.AddData) {
        $parts = $item -split ";", 2
        if ($parts.Count -eq 2) {
            $srcAbs = Join-Path $ProjectRoot $parts[0]
            $dstRel = $parts[1]
            $pyArgs += @("--add-data", ("{0};{1}" -f $srcAbs, $dstRel))
        }
    }

    foreach ($hidden in $app.HiddenImports) {
        $pyArgs += @("--hidden-import", $hidden)
    }

    foreach ($pkg in $app.CollectAll) {
        $pyArgs += @("--collect-all", $pkg)
    }

    $pyArgs += $entryAbs

    & $PythonExe @pyArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host ("Failed: {0}" -f $app.Name) -ForegroundColor Red
        $failures += $app.Name
        continue
    }

    $outExe = Join-Path $DistRoot ($app.Name + ".exe")
    if (Test-Path $outExe) {
        Write-Host ("Created: {0}" -f $outExe) -ForegroundColor Green
    } else {
        Write-Host ("Built {0}, but executable path not found where expected." -f $app.Name) -ForegroundColor Yellow
    }

    Write-Host ""
}

Write-Host "========================================" -ForegroundColor Cyan
if ($failures.Count -eq 0) {
    Write-Host "All selected Python apps built successfully." -ForegroundColor Green
    Write-Host ("Output folder: {0}" -f $DistRoot) -ForegroundColor Gray
    exit 0
}

Write-Host "Some builds failed:" -ForegroundColor Red
foreach ($name in $failures) {
    Write-Host (" - {0}" -f $name) -ForegroundColor Red
}
exit 1
