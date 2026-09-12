<#
.SYNOPSIS
    Fault Analysis Script - Reads ARM Cortex-M4 fault registers via J-Link GDB
.DESCRIPTION
    Connects to target via J-Link GDB Server and reads/decodes fault status registers.
    Use this when a HardFault/BusFault occurs to diagnose the cause.
.EXAMPLE
    .\fault_analyze.ps1
    .\fault_analyze.ps1 -StartGdbServer
#>

param(
    [switch]$StartGdbServer,
    [int]$GdbPort = 2331,
    [string]$Device = "nRF52840_xxAA"
)

$ErrorActionPreference = "Stop"

# Paths
$JLinkPath = "C:\Program Files\SEGGER\JLink"
$GdbServerExe = "$JLinkPath\JLinkGDBServerCL.exe"

function Resolve-GdbExePath {
  $projectRoot = (Get-Item $PSScriptRoot).Parent.Parent.FullName
  $configFile = Join-Path $projectRoot ".vscode\nrf_sdk_config.json"

  if (Test-Path $configFile) {
    try {
      $cfg = Get-Content $configFile -Raw | ConvertFrom-Json
      if ($cfg.ToolchainPath) {
        $candidate = Join-Path $cfg.ToolchainPath "opt\zephyr-sdk\arm-zephyr-eabi\bin\arm-zephyr-eabi-gdb.exe"
        if (Test-Path $candidate) {
          return $candidate
        }
      }
    } catch {
      Write-Host "Warning: Failed to parse nRF SDK config for GDB path: $_" -ForegroundColor Yellow
    }
  }

  $cmd = Get-Command "arm-zephyr-eabi-gdb.exe" -ErrorAction SilentlyContinue
  if ($cmd -and $cmd.Source) {
    return $cmd.Source
  }

  return $null
}

$GdbExe = Resolve-GdbExePath

if (-not $GdbExe) {
  Write-Host "Error: Could not find arm-zephyr-eabi-gdb.exe" -ForegroundColor Red
  Write-Host "Expected via .vscode/nrf_sdk_config.json ToolchainPath or in PATH." -ForegroundColor Yellow
  exit 1
}

function Get-BuildDirName {
    $projectRoot = (Get-Item $PSScriptRoot).Parent.Parent.FullName
    $configFile = Join-Path $projectRoot "scripts\setup\nrf_sdk_config.json"
    if (Test-Path $configFile) {
        try {
            $cfg = Get-Content $configFile -Raw | ConvertFrom-Json
            if ($cfg.BuildDirName) { return $cfg.BuildDirName }
        } catch {}
    }
    return "build_sdk_v330"
}

$ProjectRoot = (Get-Item $PSScriptRoot).Parent.Parent.FullName
$BuildDirName = Get-BuildDirName
$ElfFile = Join-Path $ProjectRoot "$BuildDirName\NiSense\zephyr\zephyr.elf"
$McubootElf = Join-Path $ProjectRoot "$BuildDirName\mcuboot\zephyr\zephyr.elf"

# GDB commands file
$GdbCommandsFile = "$env:TEMP\fault_analyze_gdb.txt"

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  ARM Cortex-M4 Fault Analysis Tool" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Check if GDB server is running
$gdbServerRunning = Get-Process -Name "JLinkGDBServerCL" -ErrorAction SilentlyContinue

if ($StartGdbServer -or -not $gdbServerRunning) {
    if ($gdbServerRunning) {
        Write-Host "Stopping existing GDB Server..." -ForegroundColor Yellow
        Stop-Process -Name "JLinkGDBServerCL" -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 1
    }
    
    Write-Host "Starting J-Link GDB Server..." -ForegroundColor Yellow
    $gdbServerProc = Start-Process -FilePath $GdbServerExe -ArgumentList @(
        "-select", "USB",
        "-device", $Device,
        "-if", "swd",
        "-speed", "4000",
        "-port", $GdbPort,
        "-noir",
        "-LocalhostOnly"
    ) -PassThru -WindowStyle Hidden
    
    Start-Sleep -Seconds 2
    Write-Host "GDB Server started on port $GdbPort" -ForegroundColor Green
}

# Create GDB commands
$gdbCommands = @'
# Connect to target
target remote localhost:__GDB_PORT__

# Don't ask for confirmations
set confirm off
set pagination off

# Define helper to print hex
define phex
  printf "0x%08X", \$arg0
end

# Read and display fault registers
printf "\n"
printf "=============== FAULT ANALYSIS ===============\n"
printf "\n"

# Core registers
printf "CORE REGISTERS:\n"
info registers
printf "\n"

# Read fault status registers
set $cfsr = *((unsigned int *)0xE000ED28)
set $hfsr = *((unsigned int *)0xE000ED2C)
set $dfsr = *((unsigned int *)0xE000ED30)
set $mmfar = *((unsigned int *)0xE000ED34)
set $bfar = *((unsigned int *)0xE000ED38)
set $vtor = *((unsigned int *)0xE000ED08)

printf "FAULT STATUS REGISTERS:\n"
printf "  VTOR:   0x%08X", \$vtor
if \$vtor == 0x00000000
  printf " (MCUboot - WRONG if app running!)\n"
else
  if \$vtor == 0x0000A200
    printf " (Application - OK)\n"
  else
    printf " (Unknown!)\n"
  end
end
printf "  CFSR:   0x%08X\n", \$cfsr
printf "  HFSR:   0x%08X\n", \$hfsr
printf "  DFSR:   0x%08X\n", \$dfsr
printf "  BFAR:   0x%08X\n", \$bfar
printf "  MMFAR:  0x%08X\n", \$mmfar
printf "\n"

# Decode HFSR
printf "HFSR DECODE:\n"
if \$hfsr & 0x40000000
  printf "  [FORCED] - Escalated from configurable fault\n"
end
if \$hfsr & 0x00000002
  printf "  [VECTTBL] - Vector table read error\n"
end
if \$hfsr & 0x80000000
  printf "  [DEBUGEVT] - Debug event\n"
end
if \$hfsr == 0
  printf "  (No HardFault flags set)\n"
end
printf "\n"

# Decode CFSR - BusFault (bits 15:8)
printf "BUSFAULT STATUS (BFSR - bits 15:8 of CFSR):\n"
set \$bfsr_val = (\$cfsr >> 8) & 0xFF
if \$bfsr_val == 0
  printf "  (No BusFault)\n"
else
  if \$cfsr & 0x8000
    printf "  [BFARVALID] - BFAR contains valid address: 0x%08X\n", \$bfar
  end
  if \$cfsr & 0x1000
    printf "  [LSPERR] - Fault during lazy FP state preservation\n"
  end
  if \$cfsr & 0x0800
    printf "  [STKERR] - Stack error during exception entry\n"
  end
  if \$cfsr & 0x0400
    printf "  [UNSTKERR] - Stack error during exception return\n"
  end
  if \$cfsr & 0x0200
    printf "  [IMPRECISERR] - Imprecise data access (address unknown)\n"
  end
  if \$cfsr & 0x0100
    printf "  [PRECISERR] - Precise data access violation\n"
  end
  if \$cfsr & 0x0080
    printf "  [IBUSERR] - Instruction bus error\n"
  end
end
printf "\n"

# Decode CFSR - UsageFault (bits 31:16)
printf "USAGEFAULT STATUS (UFSR - bits 31:16 of CFSR):\n"
set \$ufsr_val = (\$cfsr >> 16) & 0xFFFF
if \$ufsr_val == 0
  printf "  (No UsageFault)\n"
else
  if \$cfsr & 0x02000000
    printf "  [DIVBYZERO] - Division by zero\n"
  end
  if \$cfsr & 0x01000000
    printf "  [UNALIGNED] - Unaligned memory access\n"
  end
  if \$cfsr & 0x00080000
    printf "  [NOCP] - No coprocessor\n"
  end
  if \$cfsr & 0x00040000
    printf "  [INVPC] - Invalid PC load (bad EXC_RETURN)\n"
  end
  if \$cfsr & 0x00020000
    printf "  [INVSTATE] - Invalid EPSR.T bit (ARM vs Thumb)\n"
  end
  if \$cfsr & 0x00010000
    printf "  [UNDEFINSTR] - Undefined instruction\n"
  end
end
printf "\n"

# Decode CFSR - MemManage (bits 7:0)
printf "MEMMANAGE STATUS (MMFSR - bits 7:0 of CFSR):\n"
set \$mmfsr_val = \$cfsr & 0xFF
if \$mmfsr_val == 0
  printf "  (No MemManage fault)\n"
else
  if \$cfsr & 0x80
    printf "  [MMARVALID] - MMFAR contains valid address: 0x%08X\n", \$mmfar
  end
  if \$cfsr & 0x20
    printf "  [MLSPERR] - Lazy FP state preservation fault\n"
  end
  if \$cfsr & 0x10
    printf "  [MSTKERR] - Stack error on exception entry\n"
  end
  if \$cfsr & 0x08
    printf "  [MUNSTKERR] - Stack error on exception return\n"
  end
  if \$cfsr & 0x02
    printf "  [DACCVIOL] - Data access violation\n"
  end
  if \$cfsr & 0x01
    printf "  [IACCVIOL] - Instruction access violation\n"
  end
end
printf "\n"

# Exception stack frame
printf "EXCEPTION STACK FRAME (at SP=0x%08X):\n", \$sp
set $sp_u32 = (unsigned int *)$sp
set $stacked_r0 = *($sp_u32 + 0)
set $stacked_r1 = *($sp_u32 + 1)
set $stacked_r2 = *($sp_u32 + 2)
set $stacked_r3 = *($sp_u32 + 3)
set $stacked_r12 = *($sp_u32 + 4)
set $stacked_lr = *($sp_u32 + 5)
set $stacked_pc = *($sp_u32 + 6)
set $stacked_xpsr = *($sp_u32 + 7)

printf "  R0:   0x%08X\n", \$stacked_r0
printf "  R1:   0x%08X\n", \$stacked_r1
printf "  R2:   0x%08X\n", \$stacked_r2
printf "  R3:   0x%08X\n", \$stacked_r3
printf "  R12:  0x%08X\n", \$stacked_r12
printf "  LR:   0x%08X  <-- Return address\n", \$stacked_lr
printf "  PC:   0x%08X  <-- FAULTING INSTRUCTION\n", \$stacked_pc
printf "  xPSR: 0x%08X\n", \$stacked_xpsr
printf "\n"

# Analyze faulting PC location
printf "FAULTING PC ANALYSIS:\n"
if \$stacked_pc < 0x0000A000
  printf "  *** PC 0x%08X is in MCUboot region (0x0 - 0xA000) ***\n", \$stacked_pc
  printf "  This is UNEXPECTED if app should be running!\n"
  printf "  Possible causes:\n"
  printf "    - Stack corruption with MCUboot return address\n"
  printf "    - Function pointer corruption\n"
  printf "    - VTOR not set correctly\n"
else
  if \$stacked_pc >= 0x0000A200 && \$stacked_pc < 0x000FA000
    printf "  PC 0x%08X is in Application region (OK)\n", \$stacked_pc
  else
    printf "  PC 0x%08X is in UNKNOWN region\n", \$stacked_pc
  end
end
printf "\n"

# Show symbol at faulting PC (if available)
printf "SYMBOL AT FAULTING PC:\n"
info symbol \$stacked_pc

printf "\nSYMBOL AT RETURN ADDRESS (LR):\n"
info symbol \$stacked_lr

printf "\n"
printf "==============================================\n"
printf "\n"

# Show backtrace
printf "BACKTRACE:\n"
bt

printf "\n"
printf "==============================================\n"

# Disconnect cleanly
disconnect
quit
'@

$gdbCommands = $gdbCommands.Replace("__GDB_PORT__", [string]$GdbPort)
$gdbCommands = $gdbCommands.Replace('\$', '$')

# Write GDB commands to file
$gdbCommands | Out-File -FilePath $GdbCommandsFile -Encoding ASCII

Write-Host "Connecting to target and reading fault registers..." -ForegroundColor Yellow
Write-Host ""

# Run GDB with commands
try {
    # Check if ELF exists
    if (Test-Path $ElfFile) {
        & $GdbExe -batch -x $GdbCommandsFile $ElfFile 2>&1 | ForEach-Object {
            $line = $_
            # Color code the output
            if ($line -match "FAULT|ERROR|WRONG|UNEXPECTED") {
                Write-Host $line -ForegroundColor Red
            } elseif ($line -match "\[.*\]") {
                Write-Host $line -ForegroundColor Yellow
            } elseif ($line -match "0x[0-9A-Fa-f]+") {
                Write-Host $line -ForegroundColor Cyan
            } elseif ($line -match "===") {
                Write-Host $line -ForegroundColor Green
            } else {
                Write-Host $line
            }
        }
    } else {
        Write-Host "ELF file not found: $ElfFile" -ForegroundColor Red
        Write-Host "Running without symbols..." -ForegroundColor Yellow
        & $GdbExe -batch -x $GdbCommandsFile 2>&1
    }
} catch {
    Write-Host "Error running GDB: $_" -ForegroundColor Red
}

# Cleanup
Remove-Item -Path $GdbCommandsFile -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Analysis Complete" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Tips:" -ForegroundColor Yellow
Write-Host "  - If VTOR shows 0x00000000, the app hasn't set it yet"
Write-Host "  - If stacked PC is in MCUboot (< 0xA000), there's corruption"
Write-Host "  - BFAR shows the address that caused a BusFault"
Write-Host "  - Check the backtrace for the call sequence"
Write-Host ""

