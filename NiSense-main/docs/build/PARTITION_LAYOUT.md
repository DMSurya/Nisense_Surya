# Flash Partition Layout

**Last updated**: 2026-07-30

NiSense stores the bootloader, application images, **Resource** (UI), **Model**
(clinical), calibration, config, measurement records, and app-owned blobs across
internal flash (1 MB) and external QSPI NOR (8 MB). Layout is defined in
**device tree overlays** — not Partition Manager.

**Flash / build / QSPI programming:** [BUILD_FLASH_DEPLOY.md](BUILD_FLASH_DEPLOY.md).  
**Link size:** [BUILD_STATUS.md](BUILD_STATUS.md).

## Source of truth

| File | Role |
|------|------|
| `boards/raytac_overlay/50_memory.overlayinc` | Internal flash: mcuboot, slot0, settings NVS |
| `boards/raytac_overlay/51_qspi.overlayinc` | External QSPI map (this document must match) |
| `sysbuild/mcuboot/boards/raytac_mdbt50q_db_40_nrf52840.overlay` | MCUboot view of boot + slot0 + slot1 + scratch |
| `sysbuild.conf` | `SB_CONFIG_PARTITION_MANAGER=n` |
| `include/resource.h` | Resource A/B offsets |
| `include/glucose_model_xip.h` | Model A/B offsets + pack format |
| `include/calibration.h` | Sub-layout inside `calibration` |

**Migration:** Offsets changed 2026-07 (Resource/Model rename + dual glucose pack +
`app` partition). Field devices need a **full reflash** of Resource, Model,
calibration (if used), and any data that lived at old addresses. Do not mix old
and new maps.

## Glossary

| Term | Meaning |
|------|---------|
| **Resource** | UI fonts/logos/icons A/B store (`resource-primary/secondary`). |
| **Model** | Clinical algorithm pack A/B (`model-primary/secondary`). Glucose wearable+pulse now; future PPG/HR/SpO2 entries. |
| **XIP** | Memory-map technique (`0x12000000 + offset`) — not a product name. |

## Internal flash (`flash0`, 1 MB)

| Address | Size | DT label | Description |
|---------|------|----------|-------------|
| `0x00000` | 40 KB (`0xa000`) | `mcuboot` | MCUboot bootloader |
| `0x0a000` | 968 KB (`0xf2000`) | `image-0` | Primary application (slot0) |
| `0xfc000` | 16 KB (`0x4000`) | `storage` | Zephyr settings / BLE bond NVS |

## External flash (`mx25r64`, 8 MB QSPI)

| Address | Size | DT label | Description |
|---------|------|----------|-------------|
| `0x000000` | 968 KB (`0xf2000`) | `image-1` | Secondary app (OTA staging) |
| `0x0f2000` | 16 KB (`0x4000`) | `image-scratch` | MCUboot swap scratch |
| `0x0f6000` | 512 KB (`0x80000`) | `resource-primary` | UI Resource A/B slot 0 (NVS `resource/slot`) |
| `0x176000` | 512 KB (`0x80000`) | `resource-secondary` | UI Resource A/B slot 1 (BLE `…def5`) |
| `0x1f6000` | 512 KB (`0x80000`) | `model-primary` | Clinical model pack A/B slot 0 (NVS `model/slot`) |
| `0x276000` | 512 KB (`0x80000`) | `model-secondary` | Clinical model pack A/B slot 1 (BLE `…def3`) |
| `0x2f6000` | 64 KB (`0x10000`) | `calibration` | Factory calibration (RO) |
| `0x306000` | 128 KB (`0x20000`) | `storage_nvs` / `nvs_partition` | External NVS / config |
| `0x326000` | 4 MB (`0x400000`) | `record_store` | Append-only measurement records |
| `0x726000` | ~872 KB (`0xda000`) | `app` | App-owned blobs (MAX32664 `.msbl` ~300 KB) |

XIP mmap base: `0x12000000 + partition_offset`.

**A/B OTA:** BLE always writes the **inactive** Resource or Model slot, validates
magic + CRC, then flips NVS. Prevents a partial write from corrupting the live UI
or live clinical model.

**Model pack:** each Model slot holds an `MDLP` catalog with glucose wearable +
pulse `GMDL` entries (and room for future algo types). `product_hw_detect()`
selects the active glucose entry at boot after pack CRC verify.

## Boot order (application)

1. Power init  
2. `product_hw_detect()`  
3. Hardware / build / firmware info (includes SKU)  
4. I2C scan  
5. Enable QSPI mmap → **Resource CRC** → **Model CRC**  
6. Calibration / UI / sensors / BLE / …

## Manufacturing flash (QSPI stores)

One script programs Resource / Model / Calibration via direct QSPI registers:

```powershell
.\scripts\flash\flash_qspi_stores.ps1
.\scripts\flash\flash_qspi_stores.ps1 -Targets resource,model
.\scripts\flash\flash_qspi_stores.ps1 -Targets calibration -CalibrationFile cal.bin
# Wipe entire 8 MB QSPI (slot1, Resource/Model, calib, NVS, records, app):
.\scripts\flash\flash_qspi_stores.ps1 -EraseChip
# Then re-flash stores:
.\scripts\flash\flash_qspi_stores.ps1
```

`flash_resource.ps1` and `flash_glucose_model.ps1` are thin wrappers around the same script.

## Constraints

1. `slot0` and `slot1` must be the same size (`0xf2000`).
2. Partitions must not overlap; prefer 4 KB sector alignment.
3. Changing any address in `51_qspi.overlayinc` requires updating headers,
   flash scripts, Ozone maps, and this document together.
