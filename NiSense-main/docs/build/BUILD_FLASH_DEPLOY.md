# Build, Flash, Deploy, Debug

**Last updated**: 2026-07-30 · **SDK**: nRF Connect SDK v3.3.0  
**Build dir**: `build_sdk_v330/` (sysbuild: app + MCUboot)

This is the **canonical** firmware lifecycle doc. Operator smoke tests stay in
[guides/PLATFORM_GUIDE.md](../guides/PLATFORM_GUIDE.md). Memory numbers:
[BUILD_STATUS.md](BUILD_STATUS.md) · [MEMORY_TUNING.md](../debugging/MEMORY_TUNING.md).

```
prj.conf (APP_FEATURE_*)
    │
    ├─ Kconfig select ──► stack symbols (USB/shell/…)
    └─ CMakeLists.txt ──► conf/core/* + conf/features/*.conf  (EXTRA_CONF)
              │
              ▼
     west / build_project.ps1  →  build_sdk_v330/
              │
              ├─ flash_jlink.ps1          (merged app + MCUboot)
              ├─ flash_qspi_stores.ps1    (Resource / Model / calib)
              └─ rtt_monitor.ps1          (logs; shell is USB CDC)
```

---

## 1. Prerequisites

1. nRF Connect SDK **v3.3.0** + toolchain (same as VS Code nRF Connect extension).
2. One-time setup: [`scripts/setup/setup_nrf_sdk.ps1`](../../scripts/setup/setup_nrf_sdk.ps1)  
   → writes `.vscode/nrf_sdk_config.json`.
3. J-Link (or nrfjprog) for flash; RTT for logs.

---

## 2. Feature model (`prj.conf` is source of truth)

All product switches live at the top of [`prj.conf`](../../prj.conf) as
`CONFIG_APP_FEATURE_*=y|n`. CMake merges matching overlays from
`conf/features/`. Kconfig may **select** downstream symbols so stacks stay
coherent even before EXTRA_CONF is refreshed.

### USB & shell features

| Feature | Overlay | What it enables |
|---------|---------|-----------------|
| `APP_FEATURE_USB` | `conf/features/usb.conf` | USBD-next, CDC ACM init, UDC/USBD stacks, strings |
| `APP_FEATURE_SHELL` | `conf/features/shell.conf` | Zephyr shell (minimal), DTR, prompt |

**Rules:**

1. **`APP_FEATURE_SHELL` selects `APP_FEATURE_USB`** (and `SHELL`, `SHELL_MINIMAL`).
   You cannot have shell without CDC VCOM.
2. Prefer setting **both** explicitly when shell is on (`USB=y` documents the dependency).
3. Board DT **always** includes USBD + CDC node + `zephyr,shell-uart`
   (`boards/raytac_overlay/63_usb`, `64_usb_cdc`, `65_shell_uart`). Drivers bind
   only when the feature Kconfig is on.
4. FatFS / **USB MSC are removed**. `APP_FEATURE_USB` is CDC VCOM only.
5. Logging stays on **RTT**; shell is never RTT.

Production default (tight FLASH): `SHELL=n`, `USB=n` → ~91% FLASH.  
With shell+USB (minimal shell): ~**98%** FLASH — leave headroom before adding code.

### Sticky `-DCONFIG_*` (common footgun)

nRF Connect / an old `west build -- -DCONFIG_APP_FEATURE_SHELL=y` stores
`CLI_CONFIG_APP_FEATURE_SHELL=y` in `CMakeCache.txt` and **`build_info.yml`**.
That **overrides** a later `prj.conf=n`, so USB/shell keep linking, FLASH stays
~98%, and you may see:

```text
#warning "USBD_CDC_ACM_LOG_LEVEL forced to LOG_LEVEL_NONE"
```

**Fix:**

1. Set `CONFIG_APP_FEATURE_SHELL=n` and `CONFIG_APP_FEATURE_USB=n` in `prj.conf`.
2. CMake clears sticky CLI for those two when prj says `n` (see `CMakeLists.txt`).
3. Prefer a **pristine** rebuild after toggling shell/USB:
   ```powershell
   .\scripts\setup\build_project.ps1 -Clean
   ```
4. In the IDE, remove Extra CMake args / Kconfig fragments that force
   `-DCONFIG_APP_FEATURE_SHELL=y` or `USB=y`.
5. Confirm after configure:
   ```text
   # CONFIG_APP_FEATURE_SHELL is not set
   # CONFIG_USB_DEVICE_STACK_NEXT is not set
   ```
   in `build_sdk_v330/NiSense/zephyr/.config`.

`build_project.ps1` only emits `-DCONFIG_APP_FEATURE_*=y` when the feature is
**not** already `=y` in `prj.conf` (avoids re-sticking).

### EXTRA_CONF refresh

CMake recomputes `NiSense_EXTRA_CONF_FILE` from `prj.conf` on **reconfigure**
(not on every incremental compile). After toggling features, use `-Clean`,
`-p auto`, or touch `CMakeLists.txt` / run Configure in the IDE. Look for:

```text
-- NiSense EXTRA_CONF (FALSE USB / FALSE shell): …display.conf;…ble.conf;…
```

---

## 3. Build

### Recommended: script

```powershell
.\scripts\setup\build_project.ps1
.\scripts\setup\build_project.ps1 -Clean          # pristine
.\scripts\setup\build_project.ps1 -EnableShell    # only if not already in prj
```

Script setup matches the VS Code extension toolchain env. Overlays are applied
via `-DNiSense_EXTRA_CONF_FILE=…` for the **app** image only (not MCUboot).

### VS Code / nRF Connect

- Task: **Ctrl+Shift+B** → Build (+ Flash + Monitor as configured).
- Board: `raytac_mdbt50q_db_40/nrf52840`, sysbuild on, build dir `build_sdk_v330`.

### Outputs

| Artifact | Path |
|----------|------|
| App ELF | `build_sdk_v330/NiSense/zephyr/zephyr.elf` |
| Merged hex | `build_sdk_v330/merged_raytac_mdbt50q_db_40_nrf52840.hex` (+ `merged.hex` alias) |
| Map | `build_sdk_v330/NiSense/zephyr/zephyr.map` |

Link snapshot: [BUILD_STATUS.md](BUILD_STATUS.md).

---

## 4. Flash (internal + QSPI)

### Application + MCUboot

```powershell
.\scripts\flash\flash_jlink.ps1
.\scripts\flash\flash_and_monitor.ps1    # flash + RTT session
.\scripts\flash\flash_full.ps1           # erase + flash
```

### QSPI stores (Resource / Model / calibration)

Partitions: [PARTITION_LAYOUT.md](PARTITION_LAYOUT.md).

```powershell
.\scripts\flash\flash_qspi_stores.ps1
.\scripts\flash\flash_qspi_stores.ps1 -Targets resource,model
.\scripts\flash\flash_resource.ps1              # wrapper → Resource
.\scripts\flash\flash_glucose_model.ps1         # wrapper → Model pack
```

After a QSPI map change (2026-07 Resource/Model rename), field devices need a
**full reflash** of those stores — do not mix old offsets with new firmware.

### OTA

- Modes: [OTA_BOOTLOADER.md](OTA_BOOTLOADER.md)
- Multi-image ZIP (app + Resource + Model): [MULTI_IMAGE_OTA.md](MULTI_IMAGE_OTA.md)

---

## 5. Deploy / verify

| Step | Action |
|------|--------|
| 1 | Flash merged hex; confirm boot on RTT (`.\scripts\rtt\rtt_monitor.ps1`) |
| 2 | Flash Resource + Model if UI fonts/icons or glucose matrices changed |
| 3 | SKU: RTT should show Watch vs Pulse after `product_hw_detect()` |
| 4 | BLE: pair per [BLE_MONITOR_FLOW_GATES.md](../ble/BLE_MONITOR_FLOW_GATES.md) |
| 5 | Optional CDC shell: only if `APP_FEATURE_SHELL=y` — open VCOM, `nisense rec stats` |
| 6 | Host smoke: [PLATFORM_GUIDE.md](../guides/PLATFORM_GUIDE.md) |

Manufacturing caveats: [HARDWARE_FLASH_CAVEATS.md](../hardware/HARDWARE_FLASH_CAVEATS.md),
[FACTORY_CALIBRATION.md](../hardware/FACTORY_CALIBRATION.md).

---

## 6. Debug

| Need | Path |
|------|------|
| RTT logs | [RTT_DEBUG.md](../debugging/RTT_DEBUG.md) · `scripts/rtt/rtt_monitor.ps1` |
| Boot `<wrn>` / `<err>` | [BOOT_LOG_GUIDE.md](../debugging/BOOT_LOG_GUIDE.md) |
| RAM / LVGL / stacks | [MEMORY_TUNING.md](../debugging/MEMORY_TUNING.md) |
| SystemView | [SYSTEMVIEW.md](../debugging/SYSTEMVIEW.md) |
| Ozone + MCUboot | [OZONE_MCUBOOT_GUIDE.md](../debugging/OZONE_MCUBOOT_GUIDE.md) |
| CDC `nisense` commands | [STORAGE_NOR_RECORD_STORE.md](../architecture/STORAGE_NOR_RECORD_STORE.md) |

**Shell vs logs:** RTT = logs. USB CDC = interactive shell (when feature on).

---

## Firmware versioning

| Piece | Role |
|-------|------|
| Repo-root [`VERSION`](../../VERSION) | Zephyr APP format (`VERSION_MAJOR` / `MINOR` / `PATCHLEVEL`) — **source of truth** |
| Zephyr `APP_VERSION_*` | Parsed from `VERSION` at CMake configure |
| `CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION` | Defaults to `APP_VERSION_TWEAK_STRING` (e.g. `1.2.3+0`) — **OTA image header** |
| `cmake/git_version.cmake` | Same `VERSION` → `FW_VERSION_*` in `git_version.h` |
| `FW_VERSION_STRING` | Boot RTT, BLE Device Info, UI, shell (`vMAJOR.MINOR.PATCH`) |
| `GIT_VERSION_STRING` | Short hash (`-dirty` if tree dirty) |

Bump `VERSION` when committing (Cursor global rule). After configure, confirm:

```text
CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION="1.0.0+0"
```

in `build_*/NiSense/zephyr/.config` (not the old default `0.0.0+0`).

```powershell
Get-Content .\VERSION
# RTT: Firmware: vX.Y.Z (<git>)
# imgtool sign --version X.Y.Z+TWEAK
```

---

## 7. Related scripts

Full index: [scripts/README.md](../../scripts/README.md).

| Script | Role |
|--------|------|
| `scripts/setup/build_project.ps1` | Build with overlays |
| `scripts/setup/feature_audit.ps1` | Per-feature RAM/FLASH deltas |
| `scripts/flash/*.ps1` | Program internal / QSPI |
| `scripts/rtt/*.ps1` | Log monitor |
| `scripts/debug/*` | Fault / Ozone helpers |
