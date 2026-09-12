# OTA & MCUboot

**Last updated**: 2026-07-30

NiSense uses **MCUboot** (RSA-2048 signed images) with sysbuild. The application
runs from **internal flash slot0**; OTA staging uses **external QSPI slot1**.

Flash **addresses and sizes**: [`PARTITION_LAYOUT.md`](PARTITION_LAYOUT.md).  
**Image version** (imgtool header): repo [`VERSION`](../../VERSION) — see
[BUILD_FLASH_DEPLOY.md § Firmware versioning](BUILD_FLASH_DEPLOY.md#firmware-versioning).
`CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION` must track `APP_VERSION` (e.g. `1.0.0+0`),
not the legacy default `0.0.0+0`.

---

## Hardware roles (why firmware ≠ assets)

```text
Internal Flash                         External QSPI NOR (MX25R6435F)
+------------------+                   +---------------------------+
| MCUboot (40 KB)  |                   | Slot1 — OTA staging       |
+------------------+                   +---------------------------+
| Slot0 (running)  | ◄── MCUboot ──►  | Scratch (swap only)       |
| app executes here|     upgrade       +---------------------------+
+------------------+                   | Resource store (fonts/icons)  |
| Settings NVS     |                   | … calibration, NVS, models, record_store … |
+------------------+                   +---------------------------+
```

| Content | Where it lives | How CPU uses it |
|---------|----------------|-----------------|
| **Application firmware** | Slot0 (internal flash) | **Execute** from internal flash |
| **OTA download** | Slot1 (QSPI) | Staging only until MCUboot upgrades |
| **LVGL fonts / icons / logos** | `resource-*` (QSPI) | Memory-mapped **data** reads (`0x12000000 + offset`) |

Nordic’s intended external-secondary pattern for nRF52840:

```text
BLE OTA / SMP  →  Slot1 (QSPI)  →  reboot  →  MCUboot copy/upgrade  →  Slot0 (internal)  →  boot
```

---

## Upgrade modes (MCUboot choices)

MCUboot can apply a pending image in slot1 in several ways. Only two are
realistic for NiSense firmware; the third is often confused with **asset XIP**.

### 1. Swap (using scratch) — what the layout still assumes today

Classic mechanism: MCUboot exchanges slot0 ↔ slot1 in blocks, using a scratch
area so a power loss mid-update can resume safely and the previous image can
be restored.

```text
Slot0 = v1.0 (internal)     Slot1 = v1.1 (QSPI)

  for each block:
    Slot0 block → Scratch
    Slot1 block → Slot0
    Scratch     → Slot1

Slot0 = v1.1                Slot1 = v1.0 (old image preserved)
```

| | |
|--|--|
| **Pros** | Strong rollback; power-fail resilient; old firmware kept in the other slot |
| **Cons** | Many erase/write cycles; slow (internal ↔ QSPI ↔ scratch); needs `image-scratch` (16 KB on NOR today) |
| **NiSense today** | DTS still reserves `scratch_partition` @ `0x0f2000`; docs/boot flow historically described swap-move |

### 2. Overwrite-only — **chosen direction**

No swap of the old image back to slot1. MCUboot erases slot0 and copies
slot1 → slot0 in one direction.

```text
Before:  Slot0 = v1.0 (internal)     Slot1 = v1.1 (QSPI)
After:   Slot0 = v1.1 (internal)     Slot1 = still v1.1 (not “old” firmware)
```

| | |
|--|--|
| **Pros** | Simpler; faster; **no scratch partition**; fewer NOR cycles; natural fit when secondary is external |
| **Cons** | Weaker rollback than full swap; if power dies mid-copy, recovery depends on a still-valid slot1 image and MCUboot’s ability to retry |
| **NiSense** | **Target configuration** — reclaim 16 KB scratch for other use (or leave unused until layout cleaned up); keep app execution on internal flash |

### 3. Direct-XIP (firmware) — **not for nRF52840 app**

In generic MCUboot terms, Direct-XIP means **no copy**: the bootloader jumps
to whichever slot holds the active image, and the CPU executes that image in
place (including from external flash on platforms that support instruction XIP).

| | |
|--|--|
| **Pros** | Fastest OTA (mark + reboot); no erase/copy of the full image |
| **Cons for NiSense** | nRF52840 QSPI is excellent for **memory-mapped data** (assets), but the Cortex-M4 does **not** run application firmware from external QSPI the way STM32H7 OCTOSPI / NXP RT true XIP targets do |
| **Verdict** | **Do not** use Direct-XIP as the **application** upgrade strategy |

**Do not confuse** with NiSense **asset XIP**: fonts/icons stay memory-mapped from
the `resource` partitions. That is data fetch, not “run firmware from NOR.”

---

## Decision matrix

| Mode | Good for NiSense? | Notes |
|------|-------------------|--------|
| **Swap + scratch** | Works | Safest rollback; slow; burns NOR; needs scratch |
| **Overwrite-only** | **Recommended / direction** | Best fit: primary internal, secondary QSPI, wearable simplicity |
| **Direct-XIP (app)** | **No** | Wrong execution model on nRF52840; assets XIP is unrelated |

### Chosen direction

**Configure MCUboot for overwrite-only upgrades.**

Rationale:

- MCU: nRF52840 — app must execute from **internal** flash  
- Secondary: MX25R64 QSPI — ideal as **download/staging** only  
- Wearable priorities: simpler OTA, less NOR wear, reclaim scratch space  
- Assets already use memory-mapped QSPI; that path stays separate from firmware upgrade  

Intended end state:

```text
BLE OTA → Slot1 (QSPI) → reboot → MCUboot overwrite Slot1 → Slot0 → boot from internal
```

Scratch partition becomes unused once overwrite-only is enabled and validated;
layout cleanup (remove or reassign `0x0f2000` / 16 KB) is a follow-up after
Kconfig + smoke test.

---

## Current vs target (status)

| Item | Current (as of this doc) | Target |
|------|--------------------------|--------|
| Slot0 / Slot1 sizes equal (`0xf2000`) | Required (and kept) | Still required for MCUboot dual-slot |
| `image-scratch` in DTS | Present (16 KB) | Removable after overwrite-only lands |
| Documented upgrade story | Historically swap | **Overwrite-only** |
| App execution | Internal slot0 | Unchanged |
| Asset XIP | QSPI memory-map | Unchanged |

Implementation checklist (when migrating):

1. Set MCUboot upgrade mode to overwrite-only in `sysbuild/mcuboot/prj.conf`  
   (NCS: `CONFIG_BOOT_UPGRADE_ONLY` / equivalent — confirm against NCS v3.3.0 Kconfig help).  
2. Rebuild MCUboot + app; OTA smoke test (A → B, power-cycle mid-copy lab).  
3. Update DTS comments; optionally drop `scratch_partition` and shift following
   regions only after confirming nothing still references scratch.  
4. Keep RSA signing and slot size equality.

---

## Boot sequence (runtime)

1. CPU resets to `0x0` → MCUboot (`boot_partition`, 40 KB).  
2. If a valid upgrade is pending in slot1, MCUboot applies it (**overwrite-only**
   once configured; historically swap via scratch).  
3. Validate signed image in slot0 (`image-0` @ `0xa000`).  
4. Jump to application at **`0xa200`** (header + vector table).

`CONFIG_BOOT_BOOTSTRAP=y` allows the first factory flash without a confirmed
image trailer.

## OTA update flow

1. Host writes a **signed** firmware image to slot1 (`image-1` on external flash).  
   - BLE SMP DFU when `APP_FEATURE_BLE_DFU` is enabled  
   - Or programmer / `nrfutil` / USB tooling in development  
2. Device reboots → MCUboot verifies RSA signature on slot1.  
3. Upgrade into slot0 (**swap using move** today; overwrite-only is the documented target).  
4. Application runs from updated slot0 and calls `boot_write_img_confirmed()` when boot succeeds.

### Multi-image OTA (app + Resource store + glucose model)

MCUboot does **not** flash Resource store or the glucose model. Those live in separate
QSPI A/B partitions and are updated by the running application:

| Payload | BLE channel | Staging | Activate |
|---------|-------------|---------|----------|
| App firmware | MCUmgr SMP | `image-1` | MCUboot on reboot |
| Resource store | GATT `…def5` | inactive `resource-*` | NVS `resource/slot` after CRC + `ota_compat` |
| Glucose model | GATT `…def3` | inactive `glucose-model-*` | NVS `glucose/slot` after CRC + `ota_compat` |

**Companion packaging:** ZIP + `manifest.json` (`nisense-ota-v1`) built by
[`scripts/ota/pack_ota_bundle.py`](../../scripts/ota/pack_ota_bundle.py). Mobile
extracts and transfers **resource → model → firmware** (firmware last; causes reboot).

**Progress UI:** phone and app LVGL show transfer progress while the app is still
running. MCUboot apply has **no** LCD progress (40 KB boot partition).

**Compatibility:** `include/ota_compat.h` — device COMMIT is authoritative.

Slot sizes must match — see [`PARTITION_LAYOUT.md`](PARTITION_LAYOUT.md).

---

## Build configuration

### Sysbuild (`sysbuild.conf`)

```conf
SB_CONFIG_BOOTLOADER_MCUBOOT=y
SB_CONFIG_PARTITION_MANAGER=n
SB_CONFIG_BOOT_SIGNATURE_KEY_FILE=".../mcuboot/root-rsa-2048.pem"
SB_CONFIG_BOOT_SIGNATURE_TYPE_RSA=y
SB_CONFIG_MERGED_HEX_FILES=y
```

### Application (`conf/core/bootloader.conf`)

```conf
CONFIG_BOOTLOADER_MCUBOOT=y
# FLASH_LOAD_OFFSET / FLASH_LOAD_SIZE derived from slot0_partition DTS
```

**Do not** set `CONFIG_FLASH_LOAD_OFFSET=0x0` — that links the app over the
bootloader and bricks the device.

Image management (`CONFIG_IMG_MANAGER`, SMP) is off by default; enable via
`conf/features/ble_dfu.conf` when BLE DFU is needed.

### MCUboot image (`sysbuild/mcuboot/prj.conf`)

| Option | Purpose |
|--------|---------|
| `CONFIG_NORDIC_QSPI_NOR=y` | Access external slot1 for upgrade |
| `CONFIG_BOOT_BOOTSTRAP=y` | Boot freshly flashed images |
| `CONFIG_BOOT_VALIDATE_SLOT0=y` | Verify primary on every boot |
| `CONFIG_SIZE_OPTIMIZATIONS=y` | Stay within 40 KB bootloader budget |
| `CONFIG_LOG=n` | Save flash in bootloader |
| Overwrite-only (target) | Prefer upgrade-only; drop dependency on scratch |

MCUboot board overlay:
`sysbuild/mcuboot/boards/raytac_mdbt50q_db_40_nrf52840.overlay` — internal
partitions + QSPI slot1/scratch for the bootloader’s flash map.

---

## Verification

### Build

```powershell
.\scripts\setup\build_project.ps1 -Clean
```

Check app `zephyr/.config`:

- `CONFIG_FLASH_LOAD_OFFSET=0xa000`
- `CONFIG_FLASH_LOAD_SIZE=0xf1e00` (approx.)
- `CONFIG_PARTITION_MANAGER` disabled / unset

### J-Link memory spot-check

```
mem 0x0 0x20           # MCUboot
mem 0xa000 0x20        # Image header (magic 0x96f3b83d)
mem 0xa200 0x20        # App vectors (SP in 0x2xxxxxxx range)
```

### OTA smoke test

1. Flash signed build A to device (merged hex via sysbuild).  
2. Build and sign build B; write to slot1.  
3. Reboot → confirm build B runs.  
4. Power-cycle → image should remain (confirmed boot).  
5. (Overwrite-only) Interrupt mid-copy in lab → confirm recovery from slot1 retry.

---

## Troubleshooting

| Symptom | Likely cause | Mitigation |
|---------|--------------|------------|
| MCUboot `FLASH` overflow | Bootloader > 40 KB | Size opts; `CONFIG_LOG=n` in mcuboot |
| App `FLASH` overflow | Image > slot | Trim features; see `BUILD_STATUS.md` |
| OTA upgrade fails | slot1 ≠ slot0 size | Fix DTS — `PARTITION_LAYOUT.md` |
| MCUBoot cannot read slot1 | QSPI off in mcuboot | `CONFIG_NORDIC_QSPI_NOR=y` |
| Boots old image after OTA | Bad signature / incomplete upgrade | Check signing key; RTT mcuboot banner |
| Brick after flash | App at `0x0` | Use sysbuild; never manual load offset |
| QSPI init failure | Wiring / JEDEC / pins | `docs/hardware/HARDWARE_REFERENCE.md` |
| Expect “run app from QSPI” | Direct-XIP misconception | App stays internal; only assets are XIP |

---

## Debugging

- **RTT**: MCUboot prints via SEGGER RTT (channel 0) when enabled in mcuboot
  `prj.conf`.
- **Ozone**: [`docs/debugging/OZONE_MCUBOOT_GUIDE.md`](../debugging/OZONE_MCUBOOT_GUIDE.md) —
  dual-ELF (bootloader + application) debugging.

---

## Related documentation

- [`PARTITION_LAYOUT.md`](PARTITION_LAYOUT.md) — flash map, scratch note, layout changes  
- [`BUILD_STATUS.md`](BUILD_STATUS.md) — link-time flash/RAM budget  
- [`docs/architecture/STORAGE_NOR_RECORD_STORE.md`](../architecture/STORAGE_NOR_RECORD_STORE.md) — measurement NOR store (not OTA)  
- [`docs/architecture/STORAGE_NOR_RECORD_STORE.md`](../architecture/STORAGE_NOR_RECORD_STORE.md) — NOR measurements + export
- [`docs/architecture/STORAGE_NOR_FAT_MSC.md`](../architecture/STORAGE_NOR_FAT_MSC.md) — retired FatFS/MSC stub  

- [`docs/resource/`](../resource/) — asset memory-map (not firmware Direct-XIP)  
- [MCUboot design](https://docs.mcuboot.com/design.html)  
- [`docs/hardware/HARDWARE_FLASH_CAVEATS.md`](../hardware/HARDWARE_FLASH_CAVEATS.md)  
- [`docs/debugging/OZONE_MCUBOOT_GUIDE.md`](../debugging/OZONE_MCUBOOT_GUIDE.md)
