# BLE Test Checklist — NiSense Link / Monitor

Manual verification checklist for NiSense GATT over BLE. Start with **Session 1 (PMIC)**; later sessions cover vitals, glucose, pairing, and WiFi.

**Related docs**

| Doc | Use |
| --- | --- |
| [BLE_DATA_SECURITY_DATABASE.md](BLE_DATA_SECURITY_DATABASE.md) | Per-characteristic security levels |
| [BLE_MONITOR_FLOW_GATES.md](BLE_MONITOR_FLOW_GATES.md) | Connect / pair / subscribe gating |
| [apps/mobile/README.md](../../apps/mobile/README.md) | NiSense Link build and permissions |
| [PPG_ALGO_PARAMS_AND_SCHEDULING.md](../architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md) | `f01b` sampling/schedule, RESP windows |
| SIG service UUID reference | [Bluetooth SIG `service_uuids.yaml`](https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/uuids/service_uuids.yaml) |

**Protocol:** v2.1 (`BLE_GATT_PROTOCOL_VERSION_*` in `src/ble/ble_gatt.h`)

---

## Test record (fill per session)

| Field | Value |
| --- | --- |
| Date | 2026-07-05 |
| Tester | |
| Phone / PC | OnePlus CPH2649, Android 16 |
| App | NiSense Link |
| Firmware build | |
| Security profile (`f008`) | SECURE (1) — MITM passkey required |
| Product | NiSense Watch |
| Device MAC | |
| Notes | Dashboard: 100 % SOC, 2550 mV, 0–2 mA idle; temp 36.3 °C; wear off-wrist; accel −53, −50, −982 mg. **Follow-up:** SOC vs voltage mismatch (see P8). |

---

## Pre-flight (all sessions)

| # | Step | Pass | Fail | Notes |
| --- | --- | :---: | :---: | --- |
| P1 | Flash firmware with BLE enabled; device advertises **NiSense** (default) or product label after rename / HW detect | ☑ | ☐ | `CONFIG_BT_DEVICE_NAME="NiSense"` |
| P2 | Grant BLE permissions (Android 12+: Nearby devices; API 29–30: Location) | ☑ | ☐ | Android 16 — connect OK |
| P3 | **Scan** tab lists device (name **NiSense** or Watch/Pulse if set) | ☑ | ☐ | |
| P4 | Connect succeeds; GATT shows `…def0`, `…def1`, `…def4` record sync (`f401`–`f403`), plus `…def2` if WiFi | ☑ | ☐ | Also `1800`, `1801`, SMP `8d53dc1d-…` |
| P5 | Read `f008` Security Profile — note OPEN vs SECURE | ☑ | ☐ | SECURE — pair code required |
| P6 | **SECURE only:** complete MITM pair (`f016` passkey + system dialog) | ☑ | ☐ | Paired with passkey |
| P7 | **SECURE only:** after pair, PMIC notify (`f101`/`f109`) and `f013` write succeed | ☐ | ☐ | **Partial (2026-07-05):** `f101` live on dashboard. **`f013` still untested** — clear Pass only after BK1 toggle + `ble_publish_pmic_snapshot` refresh. See §1.3. |
| P8 | Dashboard shows battery mV / mA / SOC updating (~1 Hz) | ☑ | ☐ | USB-only: **External power (no battery)** + 2550 mV; SOC clamped 0 % after firmware fix. **Retest:** `battery_mv` now sourced from PMIC internal ADC (IVMON BAT via haptic/ADC slave 0x50), fuel gauge VCELL only as fallback |

---

## Session 1 — PMIC (power rails & fuel gauge)

**Service:** Sensor Data `12345678-1234-5678-1234-56789abcdef1`  
**Control service:** Wearable Config `…def0` (characteristic `f013`)

**Notify rate:** ~1 Hz while connected (`app_ble_tick()` in `src/ble/ble.c`, 1000 ms).  
**Immediate refresh:** after any successful `f013` write, firmware calls `ble_publish_pmic_snapshot()`.

**SIG note:** NiSense exposes **Battery Service (`0x180F`)** / **Battery Level (`0x2A19`)** for SOC %. PMIC voltage, current, and rails remain vendor `f101` / `f109`.

### Tooling matrix

| Capability | NiSense Link (Android) | PC monitor | nRF Connect |
| --- | --- | --- | --- |
| `f101` decode + dashboard | Yes (mV, mA, SOC) | Yes | Raw hex only |
| `f109` decode + UI | Subscribed, **not shown in UI** | Yes (Settings PMIC panel) | Raw hex only |
| `f013` BK1 on/off | Settings buttons | Full rail + voltage | Manual write |
| `f013` BK2/BK3/BBOUT | Write via GATT explorer only | Full | Manual write |
| `f004` battery low threshold | Settings (if exposed) | Yes | Manual read/write |

---

### 1.1 `f101` — PMIC Status (NOTIFY, 10 bytes)

**UUID:** `12345678-1234-5678-1234-56789abcf101`  
**Struct:** `struct ble_pmic_data` (`src/ble/ble_gatt.h`)  
**Endian:** little-endian

| Offset | Size | Field | Type | Unit / values |
| ---: | ---: | --- | --- | --- |
| 0 | 2 | `battery_mv` | u16 | mV (fuel gauge) |
| 2 | 2 | `current_ma` | i16 | mA (+ charge, − discharge) |
| 4 | 1 | `soc_percent` | u8 | 0–100 % |
| 5 | 1 | `charger_status` | u8 | 0=not charging, 1=charging, 2=full |
| 6 | 2 | `buck1_mv` | u16 | mV |
| 8 | 2 | `buck2_mv` | u16 | mV |

**Example:** SOC 87 %, 3850 mV, −45 mA, not charging, BK1=1800 mV, BK2=3300 mV:

```text
02 0F  D3 FF  57  00  08 07  0C 0D
```

| # | Test | Expected | Pass | Fail |
| --- | --- | --- | :---: | :---: |
| 1.1.1 | Subscribe `f101` (SECURE: after pair) | Notifications ~1/s | ☐ | ☐ |
| 1.1.2 | `battery_mv` in range | Typically 3000–4200 mV (cell dependent) | ☐ | ☐ |
| 1.1.3 | `soc_percent` stable | 0–100; slow drift only | ☐ | ☐ |
| 1.1.4 | Plug USB charger | `charger_status` → 1; `current_ma` often positive | ☐ | ☐ |
| 1.1.5 | Full charge | `charger_status` → 2 when full | ☐ | ☐ |
| 1.1.6 | Unplug | `charger_status` → 0 | ☐ | ☐ |
| 1.1.7 | `buck1_mv` / `buck2_mv` | Match enabled rails (0 mV if rail off) | ☐ | ☐ |
| 1.1.8 | NiSense Link dashboard | **Battery %**, **mV · mA** match `f101` | ☐ | ☐ |

---

### 1.2 `f109` — PMIC Extended (NOTIFY, 28 bytes v1 / 34 bytes v2)

**UUID:** `12345678-1234-5678-1234-56789abcf109`  
**Struct:** `struct ble_pmic_ext_data` (append-only v2)

| Offset | Size | Field | Type | Unit / values |
| ---: | ---: | --- | --- | --- |
| 0 | 2 | `buck3_mv` | u16 | mV |
| 2 | 2 | `bbout_mv` | u16 | mV |
| 4 | 1 | `buck1_enabled` | u8 | 0/1 |
| 5 | 1 | `buck2_enabled` | u8 | 0/1 |
| 6 | 1 | `buck3_enabled` | u8 | 0/1 |
| 7 | 1 | `bbout_enabled` | u8 | 0/1 |
| 8 | 2 | `charge_voltage_mv` | u16 | Charger CV setpoint mV |
| 10 | 2 | `charge_current_ma` | u16 | Charger CC setpoint mA |
| 12 | 2 | `battery_temp_c` | i16 | °C (fuel gauge) |
| 14 | 2 | `cycle_count` | u16 | Charge cycles |
| 16 | 2 | `remaining_mah` | u16 | mAh |
| 18 | 2 | `full_mah` | u16 | mAh |
| 20 | 2 | `design_mah` | u16 | mAh |
| 22 | 2 | `time_to_empty_min` | u16 | minutes |
| 24 | 2 | `time_to_full_min` | u16 | minutes |
| 26 | 2 | `avg_current_ma` | i16 | mA |
| 28 | 2 | `vbat_mv` | u16 | PMIC VBAT mV (**v2**) |
| 30 | 2 | `vcell_mv` | u16 | Fuel-gauge VCELL mV (**v2**) |
| 32 | 1 | `flags` | u8 | bit0 usb, bit1 bat_good, bit2 bat_reg_done, bit3 sys_bat_lim, bits4–5 cell (0=absent,1=present,2=unknown) (**v2**) |
| 33 | 1 | `reserved` | u8 | pad (**v2**) |

| # | Test | Expected | Pass | Fail |
| --- | --- | --- | :---: | :---: |
| 1.2.1 | Subscribe `f109` | Notifications ~1/s with `f101` | ☐ | ☐ |
| 1.2.2 | Enable flags match hardware | `buckN_enabled` aligns with rail state | ☐ | ☐ |
| 1.2.3 | `charge_voltage_mv` / `charge_current_ma` | Non-zero when charger present | ☐ | ☐ |
| 1.2.4 | Fuel gauge fields | `remaining_mah` ≤ `full_mah` ≤ `design_mah` (typical) | ☐ | ☐ |
| 1.2.5 | Decode with PC monitor or `decodePmicExt()` | All fields parse without error | ☐ | ☐ |
| 1.2.6 | v2 dual voltage + flags | `vbat_mv` / `vcell_mv` / pack flags present (34 B) | ☐ | ☐ |
| 1.2.7 | **Known gap:** NiSense Link UI | No dashboard row yet — verify via GATT log / PC | ☐ | ☐ |

---

### 1.2b `f01a` — Device Build (READ, 64 bytes)

**UUID:** `12345678-1234-5678-1234-56789abcf01a`  
**Struct:** `struct ble_device_build_data`

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 16 | `fw_version` (NUL-terminated) |
| 16 | 12 | `git_hash` (NUL-terminated) |
| 28 | 4 | `uptime_s` |
| 32 | 4 | `pending_records` (all types) |
| 36 | 8 | `device_id` (hwinfo first 8 bytes) |
| 44 | 4 | `pending_glucose` |
| 48 | 4 | `pending_vitals` |
| 52 | 4 | `pending_temp` |
| 56 | 4 | `pending_ppg_raw` |
| 60 | 4 | `pending_glucose_raw` |

Recomputed fresh on every GATT read (not cached) — safe to poll for a live "records pending" view. Older app builds only read the first 44 bytes and stay backward compatible; older firmware only returning 44 bytes will leave the 5 per-type fields decoding to 0 on newer apps.

---

### 1.3 `f013` — PMIC Control (WRITE, 2 or 4 bytes)

**UUID:** `12345678-1234-5678-1234-56789abcf013`  
**Security:** OPEN profile = encrypted write (L2); SECURE = authenticated write (L3, MITM pair required).

**Payload**

| Bytes | Content |
| --- | --- |
| 0 | `cmd` |
| 1 | `target` |
| 2–3 | `value_mv` u16 LE — **only for** `SET_VOLTAGE` (cmd `0x03`) |

**Commands (`cmd`)**

| Value | Name |
| ---: | --- |
| `0x01` | ENABLE |
| `0x02` | DISABLE |
| `0x03` | SET_VOLTAGE (requires 4-byte payload, 600–5500 mV) |

**Targets (`target`)**

| Value | Name | Devicetree alias |
| ---: | --- | --- |
| `0x01` | BK1 | `bk1` |
| `0x02` | BK2 | `bk2` |
| `0x03` | BK3 | `bk3` |
| `0x04` | BBOUT | `bbout` |

**Encode helpers:** `encodePmicCtrl()` in `apps/mobile/lib/protocol/hcm_protocol.dart` / `encode_pmic_ctrl()` in `apps/pc/hcm_protocol.py`.

**Examples**

| Action | Hex write |
| --- | --- |
| Enable BK1 | `01 01` |
| Disable BK1 | `02 01` |
| Set BK3 to 1800 mV | `03 03 08 07` |

| # | Test | Expected | Pass | Fail |
| --- | --- | --- | :---: | :---: |
| 1.3.1 | Write enable BK1 (`01 01`) | Success; `f109.buck1_enabled` → 1 within ~1 s | ☐ | ☐ |
| 1.3.2 | Write disable BK1 (`02 01`) | Success; `buck1_enabled` → 0; `buck1_mv` → 0 | ☐ | ☐ |
| 1.3.3 | NiSense Link **Settings → BK1 on/off** | Same as 1.3.1 / 1.3.2 | ☐ | ☐ |
| 1.3.4 | Enable BK2 (`01 02`) | `buck2_enabled` → 1; `f101.buck2_mv` updates | ☐ | ☐ |
| 1.3.5 | SET_VOLTAGE BK3 1800 mV (`03 03 08 07`) | `f109.buck3_mv` ≈ 1800 | ☐ | ☐ |
| 1.3.6 | SET_VOLTAGE out of range (e.g. 100 mV) | Firmware rejects; rail unchanged | ☐ | ☐ |
| 1.3.7 | Invalid length (1 byte) | ATT error `Invalid Attribute Length` | ☐ | ☐ |
| 1.3.8 | **SECURE:** write without pair | Write rejected or insufficient auth | ☐ | ☐ |
| 1.3.9 | **SECURE:** write after MITM pair | Success | ☐ | ☐ |
| 1.3.10 | BBOUT enable/disable (if hardware present) | `bbout_enabled` / `bbout_mv` follow | ☐ | ☐ |

---

### 1.4 `f004` — Battery low threshold (READ/WRITE, 2 bytes)

**UUID:** `12345678-1234-5678-1234-56789abcf004`  
**Payload:** u16 LE, millivolts, typical range 2500–3500 (config, not live telemetry).

| # | Test | Expected | Pass | Fail |
| --- | --- | --- | :---: | :---: |
| 1.4.1 | Read default | ~3000 mV (factory default in `ble_gatt.c`) | ☐ | ☐ |
| 1.4.2 | Write 3200 mV | Read back 3200; persists across reconnect | ☐ | ☐ |

---

### 1.5 `f1ff` — Sensor-all bundle (PMIC block, ~5 s)

**UUID:** `12345678-1234-5678-1234-56789abcf1ff`  
**Layout:** 51-byte aggregate; first 10 bytes = same `ble_pmic_data` as `f101`.

| # | Test | Expected | Pass | Fail |
| --- | --- | --- | :---: | :---: |
| 1.5.1 | Subscribe `f1ff` | Notify ~every 5 s (with temperature path) | ☐ | ☐ |
| 1.5.2 | Bytes 0–9 of notify | Match latest `f101` PMIC snapshot (±1 tick) | ☐ | ☐ |
| 1.5.3 | NiSense Link on `f1ff` | Dashboard battery mV updates from embedded PMIC | ☐ | ☐ |

---

## Session 1 sign-off

| Item | Result |
| --- | --- |
| All P pre-flight steps | ☐ Pass ☐ Fail |
| `f101` telemetry | ☐ Pass ☐ Fail |
| `f109` telemetry | ☐ Pass ☐ Fail |
| `f013` control | ☐ Pass ☐ Fail |
| `f004` threshold | ☐ Pass ☐ Fail / ☐ N/A |
| `f1ff` PMIC slice | ☐ Pass ☐ Fail / ☐ N/A |
| Blockers / follow-ups | |

---

## Planned sessions (outline)

Use the same pre-flight block before each session.

| Session | Focus | Key UUIDs |
| --- | --- | --- |
| **2** | Connection & pairing | `f008`, `f016`, `f011` |
| **3** | Vitals & temperature | `f10b`, `f102`, `f010`/`f011` |
| **3b** | Last-reading seed + hold start | `f10b`/`f105`/`f108`/`f1ff`, `f010` len=3 |
| **4** | Glucose stream | `f105`, `f107`, `f108` |
| **5** | PPG / accel raw | `f106`, `f10c`, `f012` |
| **6** | Proximity & wear | `f10a` |
| **7** | WiFi config | `f201`–`f205` |
| **8** | Record sync pull | `…def4` `f401`–`f403` |
| **9** | DFU / SMP | `8d53dc1d-…` |

---

## Session — Last-reading seed & hold-to-start

**Docs:** [PLATFORM_GUIDE.md](../guides/PLATFORM_GUIDE.md), [ANDROID_MONITOR.md](../clients/ANDROID_MONITOR.md)

### Runbook

1. Complete **P1–P6** (SECURE pair). Ensure device has prior vitals/glucose (or run Measure).
2. Disconnect and reconnect; open **Dashboard**; wait ~5 s.
3. Confirm cards fill from last readings (not only live streams).
4. **Hold** Start Vitals 5 s (countdown bar); confirm measure starts with proximity bypass (`f010` write len=3, flags=`0x01`).
5. Optional: Hold Start Glucose; Stop cancels.

### Verification log

| # | Step | Pass | Fail | Tester / date | Notes |
| --- | --- | :---: | :---: | --- | --- |
| L1 | Reconnect → Dashboard last readings within ~5 s | ☐ | ☐ | | Seed + re-push |
| L2 | Hold Start Vitals 5 s starts measure (prox bypass) | ☐ | ☐ | | `f010` + `0x01` |
| L3 | Early release cancels hold (no start) | ☐ | ☐ | | |

---

## Session — Record sync (`…def4`)

**Service UUID:** `12345678-1234-5678-1234-56789abcdef4`  
**Chars:** `f401` Control (WRITE), `f402` Data (NOTIFY), `f403` Status (READ/NOTIFY)  
**Code:** `src/ble/ble_record_sync.c`, `apps/mobile/lib/ble/record_sync_client.dart`  
**Docs:** [STORAGE_NOR_RECORD_STORE.md](../architecture/STORAGE_NOR_RECORD_STORE.md)

### Runbook

1. Flash firmware with BLE + record_store; complete **P1–P6** (SECURE pair).
2. On device: run **measure-all** (vitals + glucose + temp) so summaries exist.
3. Phone: open NiSense Link → **Sync** tab (or Server / cloud helpers, or nRF Connect GATT).
4. Confirm service `…def4` and chars `f401`–`f403`.
5. Pull **SUMMARY_ONLY** from `after_id=0`; watch `f402` frames and `f403` pending.
6. Optional: pull **FULL**; verify raw chunks share `parent_id` with summaries.
7. Send ACK `up_to_id`; confirm pending decreases; CSV/XLSX under app `HCM_Logs/`; local SQLite updated.
8. Optional: cloud push → web Data Browser → **Export to Excel**.
9. Cross-check USB CDC (only if `APP_FEATURE_SHELL=y`): `nisense rec stats` / `nisense rec export <id>` (not RTT).

### Verification log

| # | Step | Pass | Fail | Tester / date | Notes |
| --- | --- | :---: | :---: | --- | --- |
| R1 | After measure-all, bonded phone discovers `…def4` (`f401`–`f403`) | ☐ | ☐ | | |
| R2 | `SUMMARY_ONLY` pull from `after_id=0`; framed summaries on `f402` | ☐ | ☐ | | Status `pending` decreases |
| R3 | Optional `FULL` — raw chunks with matching `parent_id` | ☐ | ☐ | | |
| R4 | ACK `up_to_id`; pending advances; CSV under `HCM_Logs/` | ☐ | ☐ | | `record_csv_exporter.dart` |
| R5 | CDC `nisense rec stats` / `export <id>` matches pull | ☐ | ☐ | | Only if shell/USB features on |
| R6 | Optional: ingest + `GET /api/v1/export/readings.xlsx` | ☐ | ☐ | | Server/web |

Do **not** mark Pass without hardware evidence in the Tester/date column.

---

## Session S — Sampling / schedule (`f01b`)

Characteristic: `…f01b` (12 B LE). Details: [PPG_ALGO_PARAMS_AND_SCHEDULING.md](../architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md).

| # | Check | Pass | Fail | Notes |
| --- | --- | :---: | :---: | --- |
| S1 | Read default PPG count | ☐ | ☐ | RESP on → **500**; write `0` restores Kconfig default |
| S2 | Write PPG count 300; start vitals/BLE measure | ☐ | ☐ | Completes ~12 s @ 25 Hz |
| S3 | Toggle auto schedule off/on | ☐ | ☐ | Persists across reboot (`nisense cfg show`) |
| S4 | Set fixed interval 120 s; confirm log “fixed” | ☐ | ☐ | Adaptive ladder skipped |
| S5 | Set interval 0; confirm adaptive ladder | ☐ | ☐ | 5/10/15/20/30 min |
| S6 | Cycle complete wakes display if asleep | ☐ | ☐ | `power_mgr_activity_notify` on DONE |

---

## Known gaps (track during testing)

| Gap | Impact | Workaround |
| --- | --- | --- |
| NiSense Link: `f109` not decoded in UI | Extended PMIC invisible on phone | PC monitor Settings PMIC panel; nRF Connect hex |
| NiSense Link: no PMIC CSV log | No session log for power data | Use PC monitor or add logger |
| SIG BAS `0x180F` / `0x2A19` | Generic apps read battery % | Also mirrored in vendor `f101` SOC byte |
| Mobile Settings: BK1 only | BK2/BK3/BBOUT need GATT manual write | PC monitor or nRF Connect |

---

## Quick reference — NiSense vs SIG (PMIC domain)

| NiSense | SIG standard | We use |
| --- | --- | --- |
| `f101` SOC byte | Battery Service `0x180F` + `0x2A19` | **Both** — BAS is primary for generic apps |
| `f101` VBAT / `f101` current | — (no SIG battery V/I service) | Vendor PMIC structs |
| `f004` | — | Config threshold only |
| `f013` | — | Vendor rail control |

See full SIG mapping discussion in project chat / architecture notes; authoritative assigned UUIDs: [Bluetooth SIG repository](https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/uuids/service_uuids.yaml).
