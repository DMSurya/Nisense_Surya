# NOR Record Store & Persistence

**Last updated**: 2026-07-27  
**Status**: Current architecture (FatFS / USB MSC removed)

Wearable PPG write path (hub → algo → VITALS/`PPG_RAW`):  
[`MAX32664C_PPG_NOR_PIPELINE.md`](MAX32664C_PPG_NOR_PIPELINE.md) ·  
[`../diagrams/05-data/max32664c_to_record_store_pipeline.mmd`](../diagrams/05-data/max32664c_to_record_store_pipeline.mmd)

NiSense no longer mounts a FAT volume on QSPI or exposes measurement logs over
USB MSC. Persistence is:

| Store | Medium | Purpose |
|-------|--------|---------|
| **NVS / Zephyr settings** | Internal flash `storage` + external `storage_nvs` | App config, BLE bonds, Resource/Model slot keys |
| **Resource A/B** | External QSPI XIP | UI fonts/logos/icons (`resource-primary/secondary`) |
| **Model A/B** | External QSPI XIP | Clinical pack (glucose wearable+pulse; future PPG/HR/SpO2) |
| **Record store** | External QSPI `record_store` | Append-only measurement summaries + raw chunks |
| **App** | External QSPI `app` | App-owned blobs (e.g. hub `.msbl`) |

Former FatFS/`userdata_storage` space was absorbed into `record_store`, then
trimmed when Model grew to 512 KB A/B and `app` was sized for hub MSBL
(~872 KB at `0x726000`) after rounding `record_store` to **4 MB**. See
[`PARTITION_LAYOUT.md`](../build/PARTITION_LAYOUT.md).

---

## Hybrid logging model

Fixed 256 B slots (16 B frame + 240 B payload) cannot hold a full PPG window.
One measurement therefore maps to:

| Piece | Record type | Role |
|-------|-------------|------|
| Clinical + algo scalars | `GLUCOSE` / `VITALS` / `TEMP` | One **summary** per measurement |
| Waveforms | `PPG_RAW` / `GLUCOSE_RAW` | **N chunk** records linked by `parent_id` |

```
id=100  VITALS       ts=T_calc_done  …     ← summary (calculation complete)
id=101  PPG_RAW      parent=100 chunk 0/K   ← each sample has capture ts
id=200  GLUCOSE      ts=T_calc_done  …
id=201  GLUCOSE_RAW  parent=200 …          ← each sample has capture ts
```

**Timestamps (schema v3)**

| Record | `timestamp` meaning |
|--------|---------------------|
| `VITALS` / `GLUCOSE` summary | Wall-clock when **calculation completed** (RTC unix seconds) |
| `PPG_RAW` sample | Wall-clock when that sample was **captured** (`timestamp_unix` + `timestamp_ms`) |
| `GLUCOSE_RAW` sample | Wall-clock when that ADC sample was **captured** (`timestamp_unix`) |

**Join key is still `record_id` / `parent_id`**, not timestamp. Mobile export prefers
per-sample capture times from the payload; legacy schema v2 chunks (no sample ts)
fall back to parent summary time ± rate.

Sources: [`include/record_store.h`](../../include/record_store.h),
[`src/sensors/ppg_logger.c`](../../src/sensors/ppg_logger.c),
[`src/sensors/glucose_logger.c`](../../src/sensors/glucose_logger.c).

### PPG raw sample packing

When `CONFIG_PPG_LOG_DC_AC=y` (default in `conf/features/ppg.conf`), each
`PPG_RAW` sample stores IR/Red/Green + DC/AC all channels + accel + capture
time (`struct rec_ppg_sample`, **48 B**, flag `REC_PPG_RAW_FLAG_TIMESTAMP`).
On the MAX86141 wearable path, Green is always driven (LED1) with Red/IR;
export columns `Green` / `Green_DC` / `Green_AC` are filled every sample (may
be 0 only when hub fillers are sanitized or contact is poor). Legacy records
used 42 B samples without timestamps. Chunks carry `parent_id`,
`sample_rate_hz`, `chunk_index` / `chunk_count`.

### Glucose raw packing

`GLUCOSE_RAW` samples are **8 B**: `uint16` ADC + `int16` mV×10 + `uint32`
capture unix seconds. Legacy was 4 B (adc+mv only). Summary still holds full
`glucose_algorithm_details` + profile constants.

---

## APIs

| API | Role |
|-----|------|
| `record_store_init()` | Open partition, load meta from settings |
| `record_store_append(type, payload, len, &id)` | Write one slot; returns monotonic `record_id` |
| `record_store_read_next(after_id, …)` | Next pending record with id > after_id |
| `record_store_read_next_type(after_id, type, …)` | Next pending record of a given type |
| `record_store_read_latest_type(type, …)` | Newest record of a type (scan head→tail) |
| `record_store_read_id(id, …)` | Exact id (shell get/export) |
| `record_store_ack_synced(up_to_id, mode)` | SUMMARY: watermark only; FULL: advance ring tail |
| `record_store_get_stats()` | Pending total + summary/raw + dropped + crc_fail |

### ACK modes (space reclaim)

| Mode | Wire value | Effect |
|------|------------|--------|
| SUMMARY | `0x01` | Updates `summary_ack_upto` in NVS meta; **does not** advance `tail` — raw stays until FULL ACK |
| FULL | `0x02` | Advances `tail` past all records with `id <= up_to` (reclaims space) |

Legacy 5-byte BLE ACK (no mode byte) is treated as FULL.

### Overflow / summary reserve

When the ring is full, oldest slots are dropped unless the oldest is a
clinical summary and dropping it would leave fewer than
`CONFIG_RECORD_STORE_SUMMARY_RESERVE` (default 128) pending summaries.
In that case a **raw** append returns `-ENOSPC` instead of evicting the
summary. Summary appends may still force-drop when no other option exists.
`meas_precheck` refuses a health cycle when free slots &lt; one PPG footprint
(`RECORD_STORE_PPG_FOOTPRINT_SLOTS` = 126).

### BLE last-reading seed

On connect, `ble_gatt_seed_from_last_results()` (deferred ~3 s) loads NVS
`home/last`, then fills missing Hb/BP/insulin/etc. from
`record_store_read_latest_type(VITALS|GLUCOSE)`, and re-notifies subscribed
GATT clients. See [`guides/PLATFORM_GUIDE.md`](../guides/PLATFORM_GUIDE.md).

---

## Export paths

### USB CDC shell (`CONFIG_APP_FEATURE_SHELL=y`)

Transport: CDC ACM VCOM. **`APP_FEATURE_SHELL` selects `APP_FEATURE_USB`**
(USBD-next + CDC). Overlays: `conf/features/usb.conf` + `conf/features/shell.conf`
(`SHELL_MINIMAL`). Logging stays on RTT. Production builds often leave shell/USB
**off** for FLASH headroom — see [BUILD_FLASH_DEPLOY.md](../build/BUILD_FLASH_DEPLOY.md).

| Command | Behavior |
|---------|----------|
| `nisense rec stats` | Slots, pending summary/raw, sizes |
| `nisense rec summary [N]` | Dump up to N summary records (skip raw) |
| `nisense rec get <id>` | Summary + all raw chunks for parent |
| `nisense rec raw <id>` | Raw chunks only for parent |
| `nisense rec export <id>` | CSV text for one measurement (same columns as mobile) |
| `nisense rec dump [N]` | Mixed pending stream |
| `nisense rec ack <id>` | Mark synced through id |
| `nisense rec reset` | Factory-erase store |
| `nisense cfg show` | Persisted app config |

Implementation: [`src/diag/diag_shell.c`](../../src/diag/diag_shell.c).

### BLE record sync (`…def4`)

Service UUID `12345678-1234-5678-1234-56789abcdef4`:

| Char | Role |
|------|------|
| `f401` Control (Write encrypt) | `START(after_id, mode)`, `ACK(up_to_id[, mode])`, `ABORT` |
| `f402` Data (Notify) | Framed records, MTU-fragmented |
| `f403` Status (Read/Notify) | `state`, `pending`, `cursor_id`, `sent_count`, `error` |

Modes: `SUMMARY_ONLY` (0x01) or `FULL` (0x02). After pull, host ACK is
mode-aware: SUMMARY updates watermark only; FULL advances the ring tail
(see ACK modes above).

Firmware: [`include/ble_record_sync.h`](../../include/ble_record_sync.h),
[`src/ble/ble_record_sync.c`](../../src/ble/ble_record_sync.c).  
Mobile: `apps/mobile/lib/ble/record_sync_client.dart` +
`apps/mobile/lib/services/record_csv_exporter.dart` → app docs `HCM_Logs/`.  
Verify: [`RECORD_STORE_HARDENING_VERIFY.md`](RECORD_STORE_HARDENING_VERIFY.md).

### CSV layouts (phone / shell export)

| File | Contents |
|------|----------|
| `*_Glucose.csv` | One row per glucose summary (algo + profile fields) |
| `*_Glucose_Raw.csv` | Per-sample ADC + voltage_mV |
| `*_Vitals.csv` | One row per vitals summary (+ quality / sample meta) |
| `*_PPG_Raw.csv` | Per-sample optics + DC/AC + accel + `parent_id` |

Live BLE session logging (`SessionLogger`) still writes host-side CSVs during
a connected session; bulk NOR pull is the offline / AI path.

---

## What was removed

| Legacy | Status |
|--------|--------|
| FatFS `/NAND:` userdata | Removed (`APP_FEATURE_FILE_SYSTEM=n`) |
| USB MSC mass storage | Removed |
| USB CDC shell | Optional (`APP_FEATURE_SHELL` → USB); not required for product export |
| `fat_access.c` / `usb_msc.c` | Gone |
| On-device `glucose.csv` / `ppg.csv` | Replaced by record_store |
| Streaming matrix CSV from FAT | Replaced by XIP packed model |

---

## Related

- [`PARTITION_LAYOUT.md`](../build/PARTITION_LAYOUT.md) — QSPI map  
- [`GLUCOSE_DATA_LOGGING.md`](../sensors/GLUCOSE_DATA_LOGGING.md) — glucose path detail  
- [`model/glucose/README.md`](../../model/glucose/README.md) — matrix pack/flash  
- [`OTA_BOOTLOADER.md`](../build/OTA_BOOTLOADER.md) — app slots (unchanged by this redesign)
