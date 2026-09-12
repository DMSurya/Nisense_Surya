# RTT Debug Guide

**Last Updated**: 2026-07-30  
**Platform**: nRF52840 (Raytac MDBT50Q-DB-40) | nRF Connect SDK v3.3.0

> Build / flash / deploy: [`build/BUILD_FLASH_DEPLOY.md`](../build/BUILD_FLASH_DEPLOY.md).  
> Operator checklist: [`guides/PLATFORM_GUIDE.md`](../guides/PLATFORM_GUIDE.md).

## Overview

SEGGER RTT (Real-Time Transfer) carries **firmware logs** over SWD without a UART.
Interactive shell commands are **not** on RTT — they use **USB CDC ACM** when
`CONFIG_APP_FEATURE_SHELL=y` (selects `APP_FEATURE_USB`; see below).

## RTT Features Enabled

### 1. RTT Logging Backend
- All `LOG_*` messages are output via RTT Channel 0
- Supports multiple log levels: DBG, INF, WRN, ERR
- 2048-byte buffer for log output

### 2. RTT Channels
- Channel 0: Logging (`LOG_*`, `rtt_monitor.ps1`)
- Channel 1: **SEGGER SystemView** when `CONFIG_APP_FEATURE_SYSTEMVIEW=y` — see [SYSTEMVIEW.md](SYSTEMVIEW.md)

### 3. Diagnostics without shell
For **heap + LVGL pool + stack high-watermarks**, see [MEMORY_TUNING.md](MEMORY_TUNING.md)
(`CONFIG_APP_FEATURE_DIAG_MONITOR`).

### 4. UI lifecycle debug (optional)

When `CONFIG_APP_UI_OBJ_LIFECYCLE_DEBUG=y` in `conf/features/display.conf`:

- Module `ui_obj_track` logs `LVGL CREATE` / `LVGL DELETE` per widget
- Requires `CONFIG_LV_USE_OBJ_NAME=y`
- See [UI_GUIDE.md](../ui/UI_GUIDE.md) and [BOOT_LOG_GUIDE.md](BOOT_LOG_GUIDE.md)

Disable for production builds to reduce RTT volume.

---

## Shell (USB CDC — not RTT)

`CONFIG_APP_FEATURE_SHELL=y` enables Zephyr shell on **CDC ACM** VCOM. It
**selects** `APP_FEATURE_USB` (USBD-next + CDC). Overlays:

| Feature | Overlay |
|---------|---------|
| USB | `conf/features/usb.conf` — stack, CDC boot init, UDC/USBD sizes |
| Shell | `conf/features/shell.conf` — `SHELL_MINIMAL`, DTR, prompt |

Board DT always binds `zephyr,shell-uart` to the CDC node; drivers compile only
when the features are on. RTT remains the log backend.

**FLASH:** shell+USB on a full product build is ~**98%** of the app slot — use for
lab/diagnostics. Production often leaves both **off** (~**91%**). Pristine rebuild
after toggling; see [BUILD_FLASH_DEPLOY.md](../build/BUILD_FLASH_DEPLOY.md).

Connect a serial terminal to the device CDC port, then use commands from
`src/diag/diag_shell.c`:

| Command | Behavior |
|---------|----------|
| `nisense rec stats` | Slots, pending summary/raw counts |
| `nisense rec summary [N]` | Dump up to N summary records |
| `nisense rec get <id>` | Summary + raw chunks for parent |
| `nisense rec export <id>` | CSV text for one measurement |
| `nisense rec ack <id>` | Mark synced through id |
| `nisense cfg show` | Persisted app config |

Full list: [STORAGE_NOR_RECORD_STORE.md](../architecture/STORAGE_NOR_RECORD_STORE.md).

With `SHELL_MINIMAL=y`, built-in Zephyr help/history/tab/kernel cmds are reduced.
Legacy note: `CONFIG_SHELL_BACKEND_RTT` is **not** the product path.

---

## Status Monitoring

### Memory Locations
- `0x20003F00`: Status code byte
  - `0x00`: Initialization
  - `0x10-0x13`: Regulator config
  - `0x20`: All enabled
  - `0x30`: RGB init
  - `0x40`: Display init
  - `0xFF`: Success (lower 4 bits = LED pattern)
  - `0xE0-0xE2`: Error codes

### Debug Monitor Script
```bash
python debug_monitor.py           # Monitor status
python debug_monitor.py commands  # Show available commands
```

## Boot log interpretation

Common init warnings (`max3010x not present`, hub `status 0x01`, erased calibration) are documented in **[BOOT_LOG_GUIDE.md](BOOT_LOG_GUIDE.md)**.

---

## Troubleshooting

### No RTT Output
1. Check SWD connections
2. Verify target is running (not in reset/halt)
3. Ensure RTT buffer address matches linker script
4. Try different RTT detection methods

### Shell Not Responsive
1. Confirm `CONFIG_APP_FEATURE_SHELL=y` and a CDC COM port appears
2. Open that serial port (not RTT Channel 1)
3. Type `nisense rec stats` — if no reply, check USB cable / CDC overlay

### Missing Debug Symbols
1. Ensure `CONFIG_DEBUG_INFO=y`
2. Build with `-O0` optimization
3. Load ELF file with debug symbols

## Advanced Features

### RTT Channels
- Channel 0: Log output
- Channel 1: SystemView (when enabled)
- Channel 2+: reserved

### Memory Sections
RTT buffers are placed in custom section `.rtt_buff_data` in RAM for easy debugger detection.

## Integration with External Tools

### Python Scripts
Use the provided `debug_monitor.py` for automated monitoring and analysis.

### GDB Integration
```gdb
# Monitor status in GDB
define monitor_status
    while 1
        x/xb 0x20003F00
        sleep 1
    end
end
```

RTT provides runtime logging without UART; use USB CDC for interactive `nisense` commands.
