# Documentation Hub

**Project**: NiSense — Dual-Mode Health Monitor (Wearable + Finger-Pulse)  
**Platform**: nRF52840 · Zephyr / nRF Connect SDK v3.3.0  
**Last Updated**: 2026-07-30 · **Version**: 5.13

> **Root entry:** [../README.md](../README.md) — repo map and quick links.  
> **Build / flash / deploy / debug (canonical):** [build/BUILD_FLASH_DEPLOY.md](build/BUILD_FLASH_DEPLOY.md)

NiSense runs two hardware profiles on one firmware image:

- **Watch (wearable)** — MAX32664C + MAX86141 + wrist MAX30208; HR, SpO₂, Hb, respiration; NIRS glucose.
- **Pulse** — MAX3010x + NIRS + MAX30205; HR, SpO₂, glucose / insulin / HOMA-IR.

SKU: `product_hw_detect()` at boot. UI assets live in the **Resource** A/B QSPI store; clinical matrices in the **Model** A/B pack. FatFS / USB MSC are **removed**. Export: BLE `…def4` and optional USB CDC shell (`APP_FEATURE_SHELL`).

Glossary: **Resource** = UI · **Model** = clinical packs · **XIP** = QSPI mmap technique. See [PARTITION_LAYOUT.md](build/PARTITION_LAYOUT.md).

---

## Primary paths (start here)

| Need | Doc |
|------|-----|
| **Build · flash · QSPI deploy · sticky CLI / USB·shell** | [build/BUILD_FLASH_DEPLOY.md](build/BUILD_FLASH_DEPLOY.md) |
| **Operator checklist** (build → mobile → Excel) | [guides/PLATFORM_GUIDE.md](guides/PLATFORM_GUIDE.md) |
| **Server / platform API roadmap** | [guides/SERVER_ROADMAP.md](guides/SERVER_ROADMAP.md) |
| **Link FLASH/RAM snapshot** | [build/BUILD_STATUS.md](build/BUILD_STATUS.md) |
| **Firmware `VERSION` (semver)** | [build/BUILD_FLASH_DEPLOY.md](build/BUILD_FLASH_DEPLOY.md#firmware-versioning) |
| **Memory / stack tuning** | [debugging/MEMORY_TUNING.md](debugging/MEMORY_TUNING.md) |
| **RTT logs** | [debugging/RTT_DEBUG.md](debugging/RTT_DEBUG.md) |
| **Architecture** | [architecture/ARCHITECTURE_OVERVIEW.md](architecture/ARCHITECTURE_OVERVIEW.md) |
| **Partitions** | [build/PARTITION_LAYOUT.md](build/PARTITION_LAYOUT.md) |
| **Scripts** | [../scripts/README.md](../scripts/README.md) |

```powershell
.\scripts\setup\build_project.ps1
.\scripts\flash\flash_jlink.ps1
.\scripts\flash\flash_qspi_stores.ps1   # Resource + Model
.\scripts\rtt\rtt_monitor.ps1
```

---

## Build status (summary)

| Config | FLASH (app slot) | Notes |
|--------|------------------|-------|
| **Production default** (`SHELL=n`, `USB=n`) | **~901 KB / 91.7%** | 2026-07-30 — [BUILD_STATUS.md](build/BUILD_STATUS.md) |
| Shell + USB (`SHELL_MINIMAL`) | **~98%** | Diagnostic only; very tight |

Numbers move with features — always re-check after toggles. Sticky IDE `-DCONFIG_APP_FEATURE_SHELL=y` can leave USB linked while `prj.conf` says `n`; see BUILD_FLASH_DEPLOY.

---

## Directory structure

```
docs/
├── README.md                 # This hub
├── guides/                   # Operator checklists
├── build/                    # Build, flash, partitions, OTA, status
├── architecture/             # System + storage design
├── debugging/                # RTT, memory, Ozone, boot logs
├── hardware/                 # Pins, calibration, flash caveats
├── sensors/ · ble/ · ui/ · resource/ · drivers/ · clients/ · diagrams/
```

---

## Quick navigation

| Need | Start here |
|------|------------|
| Platform guide | [guides/PLATFORM_GUIDE.md](guides/PLATFORM_GUIDE.md) |
| Hardware pinout | [hardware/HARDWARE_REFERENCE.md](hardware/HARDWARE_REFERENCE.md) |
| BLE security / gates / tests | [ble/](ble/) |
| Android connect | [clients/ANDROID_MONITOR.md](clients/ANDROID_MONITOR.md) |
| Mobile / server / web | [../apps/](../apps/README.md) |
| Sensor status | [sensors/SENSOR_STATUS.md](sensors/SENSOR_STATUS.md) |
| SystemView | [debugging/SYSTEMVIEW.md](debugging/SYSTEMVIEW.md) |
| Boot log decode | [debugging/BOOT_LOG_GUIDE.md](debugging/BOOT_LOG_GUIDE.md) |
| UI screens | [ui/UI_GUIDE.md](ui/UI_GUIDE.md) |
| NOR record store | [architecture/STORAGE_NOR_RECORD_STORE.md](architecture/STORAGE_NOR_RECORD_STORE.md) |
| BLE + Wi-Fi dual transport | [architecture/BLE_WIFI_DUAL_TRANSPORT.md](architecture/BLE_WIFI_DUAL_TRANSPORT.md) |
| PPG schedule / RESP / staged live | [architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md](architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md) |
| PPG design references | [architecture/PPG_ALGO_DESIGN_REFERENCES.md](architecture/PPG_ALGO_DESIGN_REFERENCES.md) |
| OTA / multi-image | [build/OTA_BOOTLOADER.md](build/OTA_BOOTLOADER.md) · [build/MULTI_IMAGE_OTA.md](build/MULTI_IMAGE_OTA.md) |
| Resource store | [resource/quick-reference.md](resource/quick-reference.md) |
| Glucose AFE (NIRS chain) | [sensors/GLUCOSE_ANALOG_FRONTEND.md](sensors/GLUCOSE_ANALOG_FRONTEND.md) |
| Glucose AFE review (findings) | [sensors/GLUCOSE_AFE_REVIEW.md](sensors/GLUCOSE_AFE_REVIEW.md) |
| Glucose AFE signal-chain + fix plan | [sensors/GLUCOSE_AFE_SIGNALCHAIN_AND_FIXPLAN.md](sensors/GLUCOSE_AFE_SIGNALCHAIN_AND_FIXPLAN.md) |
| Glucose matrices | [sensors/GLUCOSE_MATRIX_FILE_LOADING.md](sensors/GLUCOSE_MATRIX_FILE_LOADING.md) · [../model/glucose/README.md](../model/glucose/README.md) |

---

## build/ — Build, flash, OTA

| File | Description |
|------|-------------|
| [BUILD_FLASH_DEPLOY.md](build/BUILD_FLASH_DEPLOY.md) | **Canonical** build / flash / QSPI / debug / USB·shell rules |
| [BUILD_STATUS.md](build/BUILD_STATUS.md) | Current FLASH/RAM link snapshot |
| [PARTITION_LAYOUT.md](build/PARTITION_LAYOUT.md) | Internal + QSPI map (Resource / Model / records) |
| [OTA_BOOTLOADER.md](build/OTA_BOOTLOADER.md) | MCUboot modes |
| [MULTI_IMAGE_OTA.md](build/MULTI_IMAGE_OTA.md) | ZIP: app + Resource + Model |

---

## architecture/

| File | Description |
|------|-------------|
| [ARCHITECTURE_OVERVIEW.md](architecture/ARCHITECTURE_OVERVIEW.md) | Layers, power, UI, export |
| [BLE_WIFI_DUAL_TRANSPORT.md](architecture/BLE_WIFI_DUAL_TRANSPORT.md) | BLE control + Wi-Fi STA bulk |
| [STORAGE_NOR_RECORD_STORE.md](architecture/STORAGE_NOR_RECORD_STORE.md) | Records + BLE/CDC export |
| [STORAGE_NOR_FAT_MSC.md](architecture/STORAGE_NOR_FAT_MSC.md) | **Retired** FatFS/MSC |
| [PPG_ALGO_PARAMS_AND_SCHEDULING.md](architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md) | Windows, RESP, staged live, SFH stop, BLE schedule |
| [PPG_ALGO_DESIGN_REFERENCES.md](architecture/PPG_ALGO_DESIGN_REFERENCES.md) | Medical-grade design refs + gap matrix |
| [MAX32664C_PPG_NOR_PIPELINE.md](architecture/MAX32664C_PPG_NOR_PIPELINE.md) | Hub → NOR |

---

## debugging/

| File | Description |
|------|-------------|
| [RTT_DEBUG.md](debugging/RTT_DEBUG.md) | RTT logs; CDC shell when enabled |
| [MEMORY_TUNING.md](debugging/MEMORY_TUNING.md) | Heap / LVGL / USB stacks |
| [BOOT_LOG_GUIDE.md](debugging/BOOT_LOG_GUIDE.md) | Boot `<wrn>` / `<err>` |
| [SYSTEMVIEW.md](debugging/SYSTEMVIEW.md) | Timeline tracing |
| [OZONE_MCUBOOT_GUIDE.md](debugging/OZONE_MCUBOOT_GUIDE.md) | Dual-ELF Ozone |

---

## sensors / drivers / hardware / ui / ble

See section tables in prior hub revisions; key entries:

- Sensors: [sensors/SENSOR_STATUS.md](sensors/SENSOR_STATUS.md), MAX32664 / glucose / Wi‑Fi docs under `sensors/`
- Drivers notes: [drivers/MAX20360.md](drivers/MAX20360.md); tree: [../drivers/README.md](../drivers/README.md)
- Hardware: [HARDWARE_REFERENCE.md](hardware/HARDWARE_REFERENCE.md)
- UI: [ui/UI_GUIDE.md](ui/UI_GUIDE.md)
- BLE: [ble/BLE_DATA_SECURITY_DATABASE.md](ble/BLE_DATA_SECURITY_DATABASE.md), [BLE_TEST_CHECKLIST.md](ble/BLE_TEST_CHECKLIST.md), [WIFI_BULK_FAILURE_MATRIX.md](ble/WIFI_BULK_FAILURE_MATRIX.md)

CDC shell implementation: [`src/diag/diag_shell.c`](../src/diag/diag_shell.c) when `CONFIG_APP_FEATURE_SHELL=y` (pulls USB).

---

## guides/

| File | Description |
|------|-------------|
| [PLATFORM_GUIDE.md](guides/PLATFORM_GUIDE.md) | Build / flash / RTT / CDC / mobile / Excel smoke test |

---

## Related trees (repo root)

| Directory | Description |
|-----------|-------------|
| [../README.md](../README.md) | **Repo root** map |
| [../src/](../src/README.md) | Application sources |
| [../drivers/](../drivers/README.md) | Out-of-tree Zephyr drivers |
| [../subsys/ppg_algo/](../subsys/ppg_algo/README.md) | PPG algorithm |
| [../conf/](../conf/) | Core + feature overlays |
| [../boards/](../boards/) | Device tree overlays |
| [../scripts/](../scripts/README.md) | Build / flash / RTT |
| [../apps/](../apps/README.md) | Host applications |
| [../model/glucose/](../model/glucose/README.md) | Clinical matrices |
