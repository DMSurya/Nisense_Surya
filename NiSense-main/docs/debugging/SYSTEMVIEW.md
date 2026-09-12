# SEGGER SystemView Guide

**Platform**: nRF52840 · NCS v3.3.0  
**Feature flag**: `CONFIG_APP_FEATURE_SYSTEMVIEW=y` (`conf/features/systemview.conf`)

## What you get

SystemView records a **real-time timeline** over SWD/RTT:

- Thread scheduling (who runs, when, for how long)
- ISR entry/exit and interrupt nesting
- Idle vs active CPU time

NiSense uses **RTT channel 1** for SystemView and **channel 0** for `LOG_*` output.

| Channel | Purpose |
|---------|---------|
| 0 | `LOG_*` / RTT console (`rtt_monitor.ps1`) |
| 1 | SystemView event stream |

**Important:** Only **one** tool may own J-Link RTT at a time. Close `rtt_monitor.ps1`,
Ozone, and other J-Link clients before opening SystemView. Use SystemView for timing,
or RTT monitor for text logs — alternate sessions, not both at once.

Tracing does **not** start at boot (`CONFIG_SEGGER_SYSTEMVIEW_BOOT_ENABLE=n`). Press
**Start / F5** in SystemView after connect — that sends the start command to the
firmware. Until then the target buffer stays empty.

## Quick start (Windows)

1. Build with SystemView on (default in `prj.conf`) and flash.
2. **Close** any running `rtt_monitor.ps1` or debugger holding J-Link.
3. Let the device run (not halted).
4. Launch SystemView:

```powershell
.\scripts\debug\start_systemview.ps1
```

Optional clean capture from reset:

```powershell
.\scripts\debug\start_systemview.ps1 -ResetTarget
```

5. In SystemView:
   - **File → Open** → `scripts/debug/systemview/HCM.SVPrj` (if not already loaded)
   - **Target → Recorder Configuration** (Alt+Enter) — set **SWD speed 12000 kHz**
   - Connect J-Link (target toolbar)
   - **Target → Start Recording (F5)** — required; wait for live timeline
6. Reproduce (BLE screen, measure cycle, UI navigation).
7. **Stop** recording and inspect the timeline.

Project bundles `SYSVIEW_Zephyr.txt` so thread/API names decode as Zephyr, not embOS.

## NiSense workflows

### BLE / stack sizing

1. Start SystemView recording **before** opening the BLE screen / pairing.
2. Filter for **`BT RX WQ`**, `bt_tx_processor`, `sysworkq`, `main`.
3. Cross-check stack peaks with `CONFIG_APP_FEATURE_DIAG_MONITOR=y` (tuning build).

### Glucose / health measurement pipeline

Follow `glucose_sensor`, `ppg_wq`, `main`, `sysworkq` through a measure cycle.

### UI / LVGL responsiveness

Swipe carousel screens; long `main` bars during screen create point at LVGL/init cost.

## Reading the timeline

- **Colored bars** — thread execution windows.
- **ISR markers** — interrupt handlers.
- **Gaps** — idle or blocked on sync primitive.
- **Thread names** — `CONFIG_THREAD_NAME=y` in `systemview.conf`.

## Configuration reference

`conf/features/systemview.conf`:

```conf
CONFIG_TRACING=y
CONFIG_SEGGER_SYSTEMVIEW=y
CONFIG_SEGGER_SYSTEMVIEW_BOOT_ENABLE=n
CONFIG_SEGGER_SYSVIEW_RTT_CHANNEL=1
CONFIG_SEGGER_SYSVIEW_RTT_BUFFER_SIZE=4096
CONFIG_SEGGER_SYSVIEW_APP_NAME="HCM"
CONFIG_THREAD_NAME=y

# Tracing trimmed to thread + idle scheduling (see systemview.conf for full list).
```

`HCM.SVPrj` sets **SWD 12000 kHz** so J-Link can drain RTT channel 1 faster than the
default 4000 kHz.

SystemView adds call depth on hot paths. BT RX WQ needed `CONFIG_BT_RX_STACK_SIZE=4096`
with SystemView + SMP + notify burst.

**Production:** set `CONFIG_APP_FEATURE_SYSTEMVIEW=n` to save RAM and tracing overhead.

## Troubleshooting (your log messages)

### `License Manager: No license servers discovered`

Informational on a local install. SystemView still runs without a network license
server.

### `Loading Sample_Multicore.SVDat` / embOS description files

SystemView opened a **sample** recording on startup. Ignore it. Use
**File → Open → `HCM.SVPrj`** for NiSense.

### `RTT host buffer overflow on channel 1` → recording stopped

J-Link could not read RTT channel 1 fast enough. SystemView 3.x has **no Options → J-Link**
menu; use **Target → Recorder Configuration** (Alt+Enter) for SWD speed and device.

1. Close `rtt_monitor.ps1`, Ozone, VS Code debug, and any other J-Link tool (most common cause).
2. **Target → Recorder Configuration** → set **SWD speed to 12000 kHz** (bundled in `HCM.SVPrj`).
3. Rebuild and flash after `systemview.conf` tracing trim (thread/idle timeline only by default).
4. `.\scripts\debug\start_systemview.ps1 -ResetTarget` → connect → **F5** quickly → reproduce.
5. Do not leave the device running for minutes before starting capture when event rate is high.

To trace ISRs again, set `CONFIG_TRACING_ISR=y` in `conf/features/systemview.conf` and
rebuild — expect higher RTT load.

### `RTT Control Block found` + `SystemView Buffer is empty`

RTT works, but **no trace was recorded**:

1. You did not press **Start / F5** (live record) — `BOOT_ENABLE=n` requires this.
2. You tried to read an offline/post-mortem buffer without a prior live session.
3. The core was halted in a debugger when you pressed Start.

**Fix:** Reset → run → connect → **F5 Start** → reproduce → Stop.

### `Failed to read recorded data`

Usually follows an empty buffer or a session that stopped on overflow. Start a fresh
live recording (steps above).

### Garbled timeline / wrong OS names

Confirm `HCM.SVPrj` lists `SYSVIEW_Zephyr.txt` under Description files (bundled in
`scripts/debug/systemview/`).

## Related

- [RTT_DEBUG.md](RTT_DEBUG.md) — log channel 0, shell, fault scripts
- [MEMORY_TUNING.md](MEMORY_TUNING.md) — stack/heap sizing with `diag_monitor`
- [OZONE_MCUBOOT_GUIDE.md](OZONE_MCUBOOT_GUIDE.md) — source-level debug with MCUboot
