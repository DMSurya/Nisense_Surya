# Scripts Directory

Organized scripts for the nRF52840 Wearable Health Monitor project.

**Host applications** (HCM Monitor, mobile app) live under [`apps/`](../apps/) — not in `scripts/`.

**Canonical lifecycle:** [`docs/build/BUILD_FLASH_DEPLOY.md`](../docs/build/BUILD_FLASH_DEPLOY.md)  
**Docs hub:** [`docs/README.md`](../docs/README.md) · **Root:** [`README.md`](../README.md)

## Directory Structure

```
scripts/
├── deploy/          # RETIRED MSC copy helpers (see flash_qspi_stores.ps1)
├── debug/           # Debugging and analysis tools
├── flash/           # Firmware + QSPI Resource/Model flash scripts
├── jlink/           # J-Link command files (.jlink)
├── rtt/             # RTT monitoring scripts
├── setup/           # Build and project setup scripts
├── tools/           # Analysis, icons, pack_glucose_model, ROM tools
├── boards/          # Board-specific configurations
└── run_hcm_monitor.ps1   # Wrapper → apps/pc/hcm_monitor.py
```

## Host apps (not in scripts/)

| App | Path |
|-----|------|
| HCM Monitor (PC) | [`apps/pc/`](../apps/pc/) |
| HCM Mobile | [`apps/mobile/`](../apps/mobile/) |

See [`apps/README.md`](../apps/README.md).

## Quick Start

### Build, Flash, and Monitor
```powershell
# From project root:
.\scripts\setup\build_project.ps1      # Build firmware
.\scripts\setup\build_project.ps1 -Clean  # After toggling APP_FEATURE_SHELL/USB
.\scripts\flash\flash_jlink.ps1        # Flash merged app+MCUboot
.\scripts\rtt\rtt_monitor.ps1          # Monitor RTT output (logs, not shell)
```

Or use VS Code tasks: `Ctrl+Shift+B` → "Build + Flash + Monitor"

**Features:** edit `prj.conf` only. Sticky IDE `-DCONFIG_APP_FEATURE_SHELL=y`
overrides `=n` — see BUILD_FLASH_DEPLOY.

End-to-end operator guide:  
[`docs/guides/PLATFORM_GUIDE.md`](../docs/guides/PLATFORM_GUIDE.md).

### Flash QSPI stores (`scripts/flash/`)

Resource (UI), Model (clinical), and calibration:

| Script | Description |
|--------|-------------|
| `flash_qspi_stores.ps1` | Pack/program Resource / Model / calibration |
| `flash_resource.ps1` | Wrapper → Resource slot |
| `flash_glucose_model.ps1` | Wrapper → Model pack |
| `tools/pack_glucose_model.py` | Build packed binary from `model/glucose/` CSVs |

```powershell
.\scripts\flash\flash_qspi_stores.ps1
.\scripts\flash\flash_qspi_stores.ps1 -Targets resource,model
.\scripts\flash\flash_glucose_model.ps1 -Variant wearable
```

### Retired: MSC copy (`scripts/deploy/`)

| Script | Status |
|--------|--------|
| `copy_glucose_model.ps1` | **Retired** — USB MSC / `/NAND:` removed; use `flash_qspi_stores.ps1` |

## Script Categories

### RTT Monitoring (`scripts/rtt/`)

| Script | Description |
|--------|-------------|
| `rtt_monitor.ps1` | Main RTT monitor with color output and auto-reconnect |
| `rtt_logger.ps1` | RTT output to file |
| `start_rtt_viewer.ps1` | Launch SEGGER RTT Viewer GUI |

**Usage:**
```powershell
# Basic monitoring
.\scripts\rtt\rtt_monitor.ps1

# With logging
.\scripts\rtt\rtt_monitor.ps1 -LogFile "session.log"

# Reset device first
.\scripts\rtt\rtt_monitor.ps1 -Reset
```

### Flashing (`scripts/flash/`)

| Script | Description |
|--------|-------------|
| `flash_and_monitor.ps1` | **Interactive flash + RTT monitor with keyboard controls** |
| `flash_jlink.ps1` | Flash via J-Link (recommended) |
| `flash_nrfjprog.ps1` | Flash via nrfjprog |
| `flash_full.ps1` | Full erase + flash |
| `flash_glucose_model.ps1` | Pack + flash glucose XIP model to QSPI |

**flash_and_monitor.ps1 Key Controls:**
- `Ctrl+R` = Reflash and restart
- `Ctrl+T` = Reset target only (no reflash)
- `Ctrl+Q` = Quit
- `Ctrl+L` = Clear screen

**Usage:**
```powershell
# Interactive flash + monitor (recommended)
.\scripts\flash\flash_and_monitor.ps1

# Flash merged.hex (default)
.\scripts\flash\flash_jlink.ps1

# Flash specific file with erase
.\scripts\flash\flash_jlink.ps1 -HexFile "custom.hex" -Erase

# Flash and run
.\scripts\flash\flash_jlink.ps1 -Reset
```

### Debugging (`scripts/debug/`)

| Script | Description |
|--------|-------------|
| `start_gdb_server.ps1` | Start J-Link GDB Server |
| `fault_analyze.ps1` | Analyze ARM fault registers |
| `fault_summary.ps1` | Quick fault summary |
| `kill_debug_processes.ps1` | Kill all debug processes |

### Setup (`scripts/setup/`)

| Script | Description |
|--------|-------------|
| `build_project.ps1` | Build with west/CMake |
| `launch_ozone.ps1` | Launch Ozone debugger |
| `setup_nrf_sdk.ps1` | Initialize nRF SDK environment |
| `setup_new_project.ps1` | Create new project from template |

### J-Link Commands (`scripts/jlink/`)

Pre-configured J-Link Commander scripts for common operations:

| File | Description |
|------|-------------|
| `reset_go.jlink` | Reset and run |
| `flash_commands.jlink` | Flash merged.hex |
| `check_boot.jlink` | Check MCUboot status |
| `read_stack.jlink` | Dump stack memory |
| `fault_check.jlink` | Read fault registers |

**Usage:**
```powershell
& "C:\Program Files\SEGGER\JLink\JLink.exe" -device nRF52840_xxAA -if swd -speed 4000 -CommandFile scripts\jlink\reset_go.jlink
```

## VS Code Integration

All scripts are accessible via VS Code tasks (`Ctrl+Shift+P` → "Tasks: Run Task"):

- **Build + Flash + Monitor** - Complete workflow
- **Build NiSense** - Build only
- **Flash Program** - Flash only
- **RTT Monitor** - Monitor RTT output
- **Launch Ozone** - Open debugger

## Environment Requirements

- **J-Link Software** (v7.80+): `C:\Program Files\SEGGER\JLink\`
- **nRF Connect SDK** (v3.0.0): Set in `NCS_BASE` environment variable
- **PowerShell** 5.1+ or PowerShell Core 7+
- **Python 3.8+** (for some scripts)

## Troubleshooting

### J-Link not found
```powershell
# Check J-Link installation
Test-Path "C:\Program Files\SEGGER\JLink\JLink.exe"
```

### RTT not connecting
```powershell
# Kill stale processes
.\scripts\debug\kill_debug_processes.ps1

# Then retry
.\scripts\rtt\rtt_monitor.ps1
```

### Build fails
```powershell
# Clean and rebuild
.\scripts\setup\build_project.ps1 -Clean
```

