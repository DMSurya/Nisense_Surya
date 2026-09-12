# Build Status — Link Snapshot

**Last updated**: 2026-07-30 · **SDK**: nRF Connect SDK v3.3.0  
**Board**: `raytac_mdbt50q_db_40/nrf52840` · **Build dir**: `build_sdk_v330`

Re-measure after any `APP_FEATURE_*` or overlay change:

```powershell
arm-zephyr-eabi-size build_sdk_v330\NiSense\zephyr\zephyr.elf
# or read the "Memory region" block at end of west build
```

Authoritative process: [BUILD_FLASH_DEPLOY.md](BUILD_FLASH_DEPLOY.md).  
Tuning: [MEMORY_TUNING.md](../debugging/MEMORY_TUNING.md).

---

## Current baselines (2026-08-01)

Feature set matches `prj.conf` production intent: BLE + DFU + glucose + PPG
(MAX32664 + MAX3010x) + display (compiled fonts) + RTT + fault debug +
**Wi-Fi STA/HTTP bulk sync**; **`APP_FEATURE_SHELL=n`**, **`APP_FEATURE_USB=n`**;
BLE PPG/accel live streams **off** (NOR sync over Wi-Fi / BLE SUMMARY).

| Image | FLASH used | FLASH region | % | RAM used | RAM % | Free |
|-------|------------|--------------|---|----------|-------|------|
| **NiSense app (Wi-Fi on)** | **954,372 B** | 982,704 B | **97.12%** | **240,356 B** | **91.69%** | ~28 KB FLASH · ~21.8 KB RAM |
| MCUboot | 35,360 B | 40 KB | 86.33% | — | — | — |

### Reclaim package that made Wi-Fi production-default viable

| Change | Approx. benefit |
|--------|-----------------|
| WExx RX/TX/HTTP/line/stacks/scan lean | ~3–4 KB RAM |
| Bulk session stack + buffer union | ~1.5 KB RAM |
| Heap 32.5→28.7 KB, LVGL 31.7→28.7 KB | ~6 KB RAM |
| RTT up-buffer 4→2 KB | ~2 KB RAM |
| BLE PPG/accel streaming off | flash + some RAM |
| Log default level 4→3, I2C DBG→WRN | flash |

Pre-reclaim Wi-Fi-on was ~964 KB FLASH / ~254 KB RAM (~8 KB RAM free) — below gate.
Gate met: ≥20 KB FLASH and ≥12 KB static RAM free with Wi-Fi on.

See [BLE_WIFI_DUAL_TRANSPORT.md](../architecture/BLE_WIFI_DUAL_TRANSPORT.md).

### With USB + shell (budget stress)

When `APP_FEATURE_SHELL=y` (selects USB + `SHELL_MINIMAL`):

| Metric | Approx. |
|--------|---------|
| FLASH | **~98%+** — do not combine shell with Wi-Fi without extra reclaim |
| Headroom | **Very tight** |

## Wi-Fi lean profile (dual transport)

**`CONFIG_APP_FEATURE_WIFI=y`** is the production default. Overlay:
[`conf/features/wifi.conf`](../../conf/features/wifi.conf) (STA + HTTP only).

Authoritative numbers: table above (2026-08-01 reclaim build).

Do **not** enable shell/USB together with Wi-Fi without measuring again.

See [BLE_WIFI_DUAL_TRANSPORT.md](../architecture/BLE_WIFI_DUAL_TRANSPORT.md).

---

## Historical (obsolete as “current”)

| Date | Note | FLASH |
|------|------|-------|
| 2026-07-21 | record_store + CDC shell (pre-minimal / sticky era) | ~966,544 B (98.36%) |
| Older | USB MSC + FatFS era | ignore |

---

## Slot / region sizes

| Region | Size | Source |
|--------|------|--------|
| App `FLASH` (slot0) | 982,704 B (`0xf2000` − load metadata) | linker / PM-less DTS |
| SRAM | 256 KB | nRF52840 |

Partitions: [PARTITION_LAYOUT.md](PARTITION_LAYOUT.md).

---

## Verify features actually off / on

```text
# build_sdk_v330/NiSense/zephyr/.config
# CONFIG_APP_FEATURE_SHELL is not set
# CONFIG_APP_FEATURE_USB is not set
# CONFIG_USB_DEVICE_STACK_NEXT is not set
# CONFIG_SHELL is not set
```

If `prj.conf` says `=n` but `.config` still has `=y`, sticky CLI overrides —
see [BUILD_FLASH_DEPLOY.md § Sticky -D](BUILD_FLASH_DEPLOY.md#sticky--dconfig--common-footgun).
