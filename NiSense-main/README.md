# NiSense

Dual-mode health monitor firmware and companion apps on **nRF52840** (Raytac MDBT50Q-DB-40) with **Zephyr / nRF Connect SDK v3.3.0**.

| Profile | Hardware | Primary measures |
|---------|----------|------------------|
| **Watch** (wearable) | MAX32664C + MAX86141 + wrist temp | HR, SpO₂, Hb, respiration, NIRS glucose |
| **Pulse** (finger) | MAX3010x + NIRS + spot temp | HR, SpO₂, glucose / insulin / HOMA-IR |

SKU is selected at boot by `product_hw_detect()` (Watch = MAX32664C present; else Pulse). One firmware image serves both.

**Persistence:** NOR `record_store` + NVS. FatFS / USB MSC are removed. Export is BLE `…def4` and optional USB CDC shell.

---

## Start here

| Goal | Doc |
|------|-----|
| **Documentation hub** | [docs/README.md](docs/README.md) |
| **Build · flash · deploy · debug** | [docs/build/BUILD_FLASH_DEPLOY.md](docs/build/BUILD_FLASH_DEPLOY.md) |
| **Operator checklist** | [docs/guides/PLATFORM_GUIDE.md](docs/guides/PLATFORM_GUIDE.md) |
| **Architecture** | [docs/architecture/ARCHITECTURE_OVERVIEW.md](docs/architecture/ARCHITECTURE_OVERVIEW.md) |
| **Partitions / QSPI** | [docs/build/PARTITION_LAYOUT.md](docs/build/PARTITION_LAYOUT.md) |
| **Firmware version** (`VERSION` → RTT/BLE + MCUboot imgtool) | [build/BUILD_FLASH_DEPLOY.md](docs/build/BUILD_FLASH_DEPLOY.md#firmware-versioning) |

```powershell
# Typical firmware loop (from repo root, NCS toolchain env):
.\scripts\setup\build_project.ps1
.\scripts\flash\flash_jlink.ps1
.\scripts\rtt\rtt_monitor.ps1
```

QSPI Resource / Model / calibration: `.\scripts\flash\flash_qspi_stores.ps1`  
Scripts index: [scripts/README.md](scripts/README.md)

---

## Repository map

```
NiSense/
├── README.md                 ← you are here
├── prj.conf                  ← APP_FEATURE_* master switches
├── Kconfig                   ← feature → stack selects (USB/shell, …)
├── CMakeLists.txt            ← EXTRA_CONF from prj.conf; prj wins over sticky -D
├── conf/                     ← core + feature Kconfig overlays
├── boards/                   ← DTS overlays (USB CDC DT always present; drivers gated)
├── src/                      ← application (see src/README.md)
├── include/                  ← public headers
├── drivers/                  ← out-of-tree Zephyr drivers
├── subsys/                   ← PPG algorithm, …
├── docs/                     ← documentation hub
├── scripts/                  ← build / flash / RTT / tools
├── model/                    ← clinical glucose matrices
├── assets/ · fonts/          ← UI assets / fonts for Resource pack
├── apps/                     ← mobile / web / server / PC hosts
├── sysbuild/ · mcuboot/      ← multi-image / bootloader
└── build_sdk_v330/           ← default build dir (gitignored)
```

| Path | What lives there |
|------|------------------|
| [src/](src/README.md) | App: UI, BLE, sensors, `record_store`, `diag_shell` |
| [include/](include/) | Shared APIs (`record_store.h`, `resource.h`, …) |
| [drivers/](drivers/) | MAX20360, MAX32664, MAX3010x, glucose, Wi‑Fi WExx, … |
| [subsys/ppg_algo/](subsys/ppg_algo/README.md) | On-chip / hub PPG algorithm |
| [conf/](conf/) | `conf/core/*` always; `conf/features/*` per `APP_FEATURE_*` |
| [boards/](boards/) | Raytac overlay fragments (`64_usb_cdc`, `65_shell_uart`, QSPI, …) |
| [docs/](docs/README.md) | Architecture, build, sensors, BLE, debug, UI |
| [scripts/](scripts/README.md) | Build, flash, RTT, packing tools |
| [model/glucose/](model/glucose/README.md) | Wearable + pulse matrix packs |
| [apps/](apps/README.md) | Flutter mobile, server, web, PC monitor |
| [docs/resource/](docs/resource/quick-reference.md) | UI Resource store (fonts/icons) |

---

## Features that matter for size

Toggle in **`prj.conf` only** (do not leave sticky `-DCONFIG_APP_FEATURE_*=y` in the IDE build args):

| Feature | Role | Rough FLASH impact |
|---------|------|--------------------|
| `APP_FEATURE_USB` | USBD-next + CDC VCOM (`usb.conf`) | ~tens of KB |
| `APP_FEATURE_SHELL` | Zephyr shell on CDC; **selects USB** + `SHELL_MINIMAL` | USB + shell (~full set ~98% FLASH) |
| Default product path | Shell/USB **off** | ~**91–92%** FLASH (see BUILD_STATUS) |

Hierarchy: **Shell → USB → drivers**. Board DT always describes CDC + `zephyr,shell-uart`; software is gated by the features. Details: [BUILD_FLASH_DEPLOY.md](docs/build/BUILD_FLASH_DEPLOY.md#usb--shell-features).

---

## Host applications

| App | Path |
|-----|------|
| Mobile (Flutter) | [apps/mobile/README.md](apps/mobile/README.md) |
| Server API | [apps/server/README.md](apps/server/README.md) |
| Web browser | [apps/web/README.md](apps/web/README.md) |
| PC HCM monitor | [apps/pc/README.md](apps/pc/README.md) |

---

## License / ownership

Internal AARMS / HCM project. Treat hardware docs and calibration as controlled manufacturing material.
