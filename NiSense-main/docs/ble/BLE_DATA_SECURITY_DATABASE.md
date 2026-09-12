# BLE Data and Security Database (Current vs Required)

Last updated: 2026-07-20
Target: NiSense (nRF52840, Zephyr/NCS)

## 1. Security Level Model

| Level | Meaning | MITM |
| --- | --- | --- |
| L1 | No security | No |
| L2 | Encrypted | No |
| L3 | Encrypted + Authenticated | Yes |
| L4 | Authenticated LE Secure Connections | Yes |

Operational mapping used in this document:
- L1: `BT_GATT_PERM_READ` / `BT_GATT_PERM_WRITE` (or notify with open CCC).
- L2: `BT_GATT_PERM_*_ENCRYPT`.
- L3: `BT_GATT_PERM_*_AUTHEN` (or subsystem-level auth gate such as MCUmgr `RW_AUTHEN`).
- L4: L3 + LE Secure Connections policy enforced (`BT_SMP_SC`/equivalent policy and pairing constraints).

## 2. Evidence Sources (Current Implementation)

Primary sources analyzed:
- `src/ble/ble_gatt.h`
- `src/ble/ble_gatt.c`
- `src/ble/ble.c`
- `conf/features/ble.conf`
- `conf/features/ble_dfu.conf`
- `apps/pc/hcm_protocol.py`
- `src/ble/ble_record_sync.c` / `include/ble_record_sync.h`
- `docs/ble/BLE_MONITOR_FLOW_GATES.md`

Current baseline findings:
- `CONFIG_BT_SMP=y`, `CONFIG_BT_BONDABLE=y` in BLE feature overlay.
- Default advertise name: `CONFIG_BT_DEVICE_NAME="NiSense"`.
- Most custom GATT controls are L1; a small subset is L2 (`*_ENCRYPT`).
- SMP DFU is explicitly gated to authenticated access via `CONFIG_MCUMGR_TRANSPORT_BT_PERM_RW_AUTHEN=y` (L3).
- No explicit global L4-only policy found in active BLE overlay.
- Custom services: `...def0`, `...def1`, `...def2` (WiFi) in `ble_gatt.c`, plus record sync `...def4` in `ble_record_sync.c`.

## 3. Complete BLE Data Inventory and Security Matrix

Legend:
- Current level: what firmware enforces today.
- Required level: recommended target for production hardening.
- Gap: `OK` or `Upgrade`.

### 3.1 Wearable Config Service (`...def0`)

| Characteristic | UUID suffix | Access | Data/Purpose | Current level | Required level | Gap | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Device Name | `f001` | R/W | User-visible device name | L1 | L2 | Upgrade | Prevent spoof/tamper in open links. |
| RTC Trim | `f003` | R/W | RTC calibration ppm | L1 | L2 | Upgrade | Prevent remote drift/timing sabotage. |
| Battery Low Threshold | `f004` | R/W | mV threshold | L1 | L2 | Upgrade | Influences low-battery behavior. |
| Brightness | `f005` | R/W | UI brightness | L1 | L1 | OK | Low risk user preference. |
| Volume | `f006` | R/W | Buzzer volume | L1 | L1 | OK | Low risk user preference. |
| PPG Preference | `f007` | R/W | Sensor source preference | L1 | L2 | Upgrade | Measurement pipeline control. |
| Security Profile | `f008` | R | 0=open, 1=secure (read-only) | L1 | L1 | OK | Monitor uses this to choose pair-first UX; value set by Kconfig profile. |
| Measurement Control | `f010` | W | Start/stop/reset measurement (len 1–3; optional flags) | L1 | L2 | Upgrade | Prevent unauthorized battery/data abuse. Len 3: byte2 flags — `0x01` = skip PPG proximity (`BLE_MEAS_FLAG_SKIP_PROX`). Mobile hold-to-start uses this. |
| PPG Decimate | `f012` | R/W | Stream rate decimation | L1 | L2 | Upgrade | Affects transport/perf behavior. |
| PMIC Control | `f013` | W | Rail enable/disable/set voltage | L2 | L3 | Upgrade | Safety/power-critical; require authenticated writer. |
| Admin Control | `f014` | W | Delete bonds and disconnect | L2 | L3 | Upgrade | Identity/security state mutation requires MITM-auth peer. |

### 3.1b Current Time Service (`0x1805`) — SIG

| Characteristic | UUID | Access | Data/Purpose | Current level | Required level | Gap | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Current Time | `0x2A2B` | R/W/N | Date, time, day-of-week (10-byte CTS format) | L1 | L1 | OK | Zephyr `CONFIG_BT_CTS`; replaces vendor `f002`. Open R/W per Zephyr CTS implementation. |

### 3.2 Sensor Data Service (`...def1`)

| Characteristic | UUID suffix | Access | Data/Purpose | Current level | Required level | Gap | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| PMIC Status | `f101` | N | Battery/current/rails | L1 | L2 | Upgrade | Device telemetry leakage. |
| Temperature | `f102` | N | Wearable temperature | L1 | L2 | Upgrade | Health-adjacent telemetry. |
| Glucose | `f105` | N | Final glucose estimate | L1 | L3 | Upgrade | Sensitive health data. |
| PPG Stream | `f106` | N | Raw biometric waveform + accel | L1 | L3 | Upgrade | Biometric signal; strongest privacy requirement. |
| Glucose Sample | `f107` | N | Per-sample glucose stream | L1 | L3 | Upgrade | Sensitive health stream. |
| Glucose Algorithm | `f108` | N | Intermediate model outputs (HOMA-IR etc.) | L1 | L3 | Upgrade | High sensitivity and reverse-engineering risk. |
| PMIC Extended | `f109` | N | Extended fuel/charger diagnostics (v2: VBAT/VCELL/pack flags) | L1 | L2 | Upgrade | Operational telemetry. |
| Device Build | `f01a` | R | FW version, git hash, uptime, pending records, device ID | L1 | L1 | — | One-shot Link / factory identity. |
| Proximity Status | `f10a` | R/N | Contact/wear state | L1 | L2 | Upgrade | Behavioral/privacy signal. |
| Vitals | `f10b` | N | HR + SpO2 + Hb + RespRate | L1 | L3 | Upgrade | Core health payload. |
| Accel Stream | `f10c` | N | High-rate motion stream | L1 | L2 | Upgrade | Motion/behavioral telemetry. |
| Sensor All | `f1ff` | N | Combined all-sensor packet | L1 | L3 | Upgrade | Aggregates health + system state. |
| Measurement Status | `f011` | N | Progress telemetry | L1 | L1 | OK | Implemented under Sensor Data service in current GATT table. |

Notes for this section:
- `f106` (PPG Stream) and `f10c` (Accel Stream) are build-gated by `CONFIG_BLE_PPG_STREAMING` and `CONFIG_BLE_ACCEL_STREAMING`.
- `f103`/`f104` (legacy standalone SpO2/HR) are removed from active firmware and replaced by `f10b` (Vitals).

### 3.3 WiFi Config Service (`...def2`)

| Characteristic | UUID suffix | Access | Data/Purpose | Current level | Required level | Gap | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| WiFi Enable | `f201` | R/W | Global WiFi enable | L1 | L2 | Upgrade | Prevent unauthorized network state changes. |
| WiFi SSID | `f202` | R/W | SSID text | L2 | L3 | Upgrade | Location-sensitive identifier; require MITM-auth. |
| WiFi Password | `f203` | W | PSK/passphrase | L2 | L3 | Upgrade | Secrets must be MITM-protected. |
| WiFi Status | `f204` | R/N | Connected state, RSSI, IP, SSID | L1 | L2 | Upgrade | Network metadata exposure. |
| WiFi Connect Trigger | `f205` | W | Connect/disconnect command | L1 | L2 | Upgrade | Unauthorized network control risk. |
| Bulk Session Control | `f206` | W | START/ACK/ABORT Wi-Fi bulk session (when `APP_FEATURE_WIFI`) | L2/L3 | L3 | Keep AUTH in SECURE | Session token + endpoint; gated on Wi-Fi build. |
| Bulk Session Status | `f207` | R/N | Bulk session state/progress (18 B) | L2 | L2 | Keep ENC | Progress only; no secrets. |

### 3.4 Record Sync Service (`...def4`)

Bulk pull of NOR `record_store` pending records (summaries ± raw chunks). See
[`STORAGE_NOR_RECORD_STORE.md`](../architecture/STORAGE_NOR_RECORD_STORE.md).

| Characteristic | UUID suffix | Access | Data/Purpose | Current level | Required level | Gap | Notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Control | `f401` | W | `START(after_id, mode)`, `ACK(up_to_id[, mode])`, `ABORT` | L2 | L3 | Upgrade | `BT_GATT_PERM_WRITE_ENCRYPT` in `ble_record_sync.c`. |
| Data | `f402` | N | Framed records (MTU-fragmented) | L1 CCC / open char | L3 | Upgrade | CCC write encrypt; health history export. |
| Status | `f403` | R/N | `state`, `pending`, `cursor_id`, `sent_count`, `error` | L2 | L2 | OK | Read encrypt; CCC write encrypt. |

Modes: `SUMMARY_ONLY` (0x01) or `FULL` (0x02). ACK is
`[cmd][up_to:u32][mode:u8?]`; legacy 5-byte ACK = FULL reclaim.
SUMMARY ACK updates `summary_ack_upto` only (does not reclaim raw);
FULL ACK advances the ring tail via `record_store_ack_synced`.
Mobile: `record_sync_client.dart` → `HCM_Logs/` CSV.

### 3.5 BLE DFU / MCUmgr (SMP)

| Endpoint | UUID | Access | Current level | Required level | Gap | Notes |
| --- | --- | --- | --- | --- | --- | --- |
| SMP Service | `8d53dc1d-1db7-4cd3-868b-8a527460aa84` | Service | L3 | L3 | OK | Gated by `CONFIG_MCUMGR_TRANSPORT_BT_PERM_RW_AUTHEN=y`. |
| SMP Characteristic | `da2e7828-fbce-4e01-ae9e-261174997c48` | R/W | L3 | L4 (target), L3 (minimum) | Upgrade (optional hardening) | Production-grade DFU should move to L4 policy where feasible. |

### 3.6 CCC Descriptor Security Coverage (Missing in Earlier Revisions)

All notify-capable characteristics in `src/ble/ble_gatt.c` currently use:
- CCC permissions: `BT_GATT_PERM_READ | BT_GATT_PERM_WRITE` (L1)

Implication:
- Notification subscription toggling is currently open-link (L1) unless the characteristic itself has stronger read/write permissions.

Recommended target:
- For sensitive notify streams (Vitals, Glucose, Glucose Sample/Algo, PPG Stream, Sensor All), raise CCC permissions to encrypted/authenticated level aligned with characteristic target policy.

## 4. Security Gap Summary

Current profile shape:
- L1: Majority of data and control endpoints.
- L2: WiFi SSID/PW, PMIC ctrl, Admin ctrl.
- L3: SMP DFU characteristic only.
- L4: None enforced by explicit policy.

Risk hotspots:
- Health payloads (Vitals/Glucose/Raw streams) are currently readable on L1 links.
- Critical control writes (measurement, WiFi connect/enable) are currently L1.
- PMIC/Admin are encrypted but not authenticated (L2 only).

## 5. Recommended Target Security Policy (Production)

Policy profile:
- Public low-risk UX settings: keep at L1 only if product explicitly supports open control.
- Operational controls: minimum L2.
- Identity/security/power-critical controls: L3.
- DFU/control-plane updates: L3 minimum, L4 preferred.
- Health telemetry and biometric streams: L3 (or at least L2 if onboarding constraints require).

Suggested phased rollout:
1. Phase A (quick hardening): move control writes from L1 -> L2.
2. Phase B (privacy): move health data characteristics from L1 -> L2/L3 per table.
3. Phase C (auth hardening): move PMIC/Admin/WiFi credentials to L3 auth permissions.
4. Phase D (highest assurance): enforce L4 for DFU and optionally for full secure-mode profile.

## 6. Implementation Checklist (Firmware)

- Characteristic permission changes:
  - Replace open perms with `*_ENCRYPT` or `*_AUTHEN` per target table.
- Security level requests:
  - On secure-path entry points, request stronger security where needed.
- Pairing mode consistency:
  - Keep numeric comparison/passkey UX aligned between firmware and desktop monitor.
- LE Secure Connections (L4 path):
  - Add and validate SC-only policy symbols/settings for platform support.
- Regression validation:
  - Verify read/write/notify behavior from unpaired, bonded-L2, bonded-L3, and SC paths.

## 7. Final Recommended Security Targets Snapshot

- Keep at L1: Brightness, Volume, optional Measurement Status, CTS Current Time (Zephyr SIG service open R/W).
- Move to L2: Device Name, RTC Trim, Battery threshold, PPG preference, measurement control, decimation, PMIC/temperature/proximity/wifi status streams, WiFi enable/connect.
- Move to L3: PMIC control, Admin control, WiFi SSID/password, Vitals, Glucose, Glucose sample/algo, Sensor-all, optional PPG raw.
- Keep L3 min / move to L4 target: SMP DFU characteristic.

## 8. Machine-Verifiable Implementation Matrix (UUID -> Perm Bits -> Source)

This appendix mirrors active GATT declarations in `src/ble/ble_gatt.c` via macros in `src/ble/ble_gatt_security.h` and BLE DFU overlay in `conf/features/ble_dfu.conf`.

**Production default:** `CONFIG_BLE_SECURITY_PROFILE_SECURE=y` (`conf/features/ble.conf`). Tables below list **SECURE profile** permission macros. For OPEN profile (dev overlay `conf/features/ble_open.conf`), see the `#else` branch in `ble_gatt_security.h` — most characteristics use `BT_GATT_PERM_READ | BT_GATT_PERM_WRITE`.

Legend:
- Access: GATT property bits (`READ`, `WRITE`, `NOTIFY`, etc.)
- Perm bits: Zephyr permission macros as implemented (SECURE profile)
- Derived level: L1/L2/L3 mapping used in this document

### 8.1 Wearable Config Service (`...def0`) — SECURE profile

| UUID | Characteristic | Access | Perm bits (SECURE) | Derived level | Source |
| --- | --- | --- | --- | --- | --- |
| `f001` | Device Name | READ/WRITE | `READ_ENCRYPT \| WRITE_ENCRYPT` | L2 | `ble_gatt_security.h` |
| `f003` | RTC Trim | READ/WRITE | `READ_ENCRYPT \| WRITE_ENCRYPT` | L2 | `ble_gatt_security.h` |
| `f004` | Battery Low Threshold | READ/WRITE | `READ_ENCRYPT \| WRITE_ENCRYPT` | L2 | `ble_gatt_security.h` |
| `f005` | Brightness | READ/WRITE | `BT_GATT_PERM_READ \| BT_GATT_PERM_WRITE` | L1 | `ble_gatt_security.h` |
| `f006` | Volume | READ/WRITE | `BT_GATT_PERM_READ \| BT_GATT_PERM_WRITE` | L1 | `ble_gatt_security.h` |
| `f007` | PPG Preference | READ/WRITE | `READ_ENCRYPT \| WRITE_ENCRYPT` | L2 | `ble_gatt_security.h` |
| `f008` | Security Profile | READ | `BT_GATT_PERM_READ` | L1 | `ble_gatt_security.h` |
| `f010` | Measurement Control | WRITE/WRITE_NO_RSP | `BT_GATT_PERM_WRITE_ENCRYPT` | L2 | `ble_gatt_security.h` |
| `f012` | PPG Decimate | READ/WRITE | `READ_ENCRYPT \| WRITE_ENCRYPT` | L2 | `ble_gatt_security.h` |
| `f013` | PMIC Control | WRITE/WRITE_NO_RSP | `BT_GATT_PERM_WRITE_AUTHEN` | L3 | `ble_gatt_security.h` |
| `f014` | Admin Control | WRITE/WRITE_NO_RSP | `BT_GATT_PERM_WRITE_AUTHEN` | L3 | `ble_gatt_security.h` |

### 8.1b Current Time Service (`0x1805`) — Zephyr CTS

| UUID | Characteristic | Access | Perm bits | Derived level | Source |
| --- | --- | --- | --- | --- | --- |
| `0x2A2B` | Current Time | READ/WRITE/NOTIFY | `BT_GATT_PERM_READ \| BT_GATT_PERM_WRITE` | L1 | Zephyr `cts.c` (`CONFIG_BT_CTS`) |

### 8.2 Sensor Data Service (`...def1`) — SECURE profile

| UUID | Characteristic | Access | Perm bits (SECURE) | CCC perm bits (SECURE) | Derived level | Source |
| --- | --- | --- | --- | --- | --- | --- |
| `f101` | PMIC Status | NOTIFY | `BT_GATT_PERM_NONE` | `READ_ENCRYPT \| WRITE_ENCRYPT` | L2 (CCC) | `ble_gatt_security.h` |
| `f102` | Temperature | NOTIFY | `BT_GATT_PERM_NONE` | `READ_ENCRYPT \| WRITE_ENCRYPT` | L2 (CCC) | `ble_gatt_security.h` |
| `f105` | Glucose | NOTIFY | `BT_GATT_PERM_NONE` | `READ_AUTHEN \| WRITE_AUTHEN` | L3 (CCC) | `ble_gatt_security.h` |
| `f106` | PPG Stream* | NOTIFY | `BT_GATT_PERM_NONE` | `READ_AUTHEN \| WRITE_AUTHEN` | L3 (CCC) | `ble_gatt_security.h` |
| `f107` | Glucose Sample | NOTIFY | `BT_GATT_PERM_NONE` | `READ_AUTHEN \| WRITE_AUTHEN` | L3 (CCC) | `ble_gatt_security.h` |
| `f108` | Glucose Algorithm | NOTIFY | `BT_GATT_PERM_NONE` | `READ_AUTHEN \| WRITE_AUTHEN` | L3 (CCC) | `ble_gatt_security.h` |
| `f109` | PMIC Extended | NOTIFY | `BT_GATT_PERM_NONE` | `READ_ENCRYPT \| WRITE_ENCRYPT` | L2 (CCC) | `ble_gatt_security.h` |
| `f10a` | Proximity Status | READ/NOTIFY | `BT_GATT_PERM_READ_ENCRYPT` | `READ_ENCRYPT \| WRITE_ENCRYPT` | L2 | `ble_gatt_security.h` |
| `f10b` | Vitals | NOTIFY | `BT_GATT_PERM_NONE` | `READ_AUTHEN \| WRITE_AUTHEN` | L3 (CCC) | `ble_gatt_security.h` |
| `f10c` | Accel Stream* | NOTIFY | `BT_GATT_PERM_NONE` | `READ_ENCRYPT \| WRITE_ENCRYPT` | L2 (CCC) | `ble_gatt_security.h` |
| `f1ff` | Sensor All | NOTIFY | `BT_GATT_PERM_NONE` | `READ_AUTHEN \| WRITE_AUTHEN` | L3 (CCC) | `ble_gatt_security.h` |
| `f011` | Measurement Status | NOTIFY | `BT_GATT_PERM_NONE` | `BT_GATT_PERM_READ \| BT_GATT_PERM_WRITE` | L1 (CCC) | `ble_gatt_security.h` |

*`f106` and `f10c` are present only when feature flags are enabled (`CONFIG_BLE_PPG_STREAMING`, `CONFIG_BLE_ACCEL_STREAMING`).

### 8.3 WiFi Config Service (`...def2`) — SECURE profile

| UUID | Characteristic | Access | Perm bits (SECURE) | Derived level | Source |
| --- | --- | --- | --- | --- | --- |
| `f201` | WiFi Enable | READ/WRITE | `READ_ENCRYPT \| WRITE_ENCRYPT` | L2 | `ble_gatt_security.h` |
| `f202` | WiFi SSID | READ/WRITE | `READ_AUTHEN \| WRITE_AUTHEN` | L3 | `ble_gatt_security.h` |
| `f203` | WiFi Password | WRITE | `BT_GATT_PERM_WRITE_AUTHEN` | L3 | `ble_gatt_security.h` |
| `f204` | WiFi Status | READ/NOTIFY | `BT_GATT_PERM_READ_ENCRYPT` | L2 (char), L2 (CCC) | `ble_gatt_security.h` |
| `f205` | WiFi Connect Trigger | WRITE | `BT_GATT_PERM_WRITE_ENCRYPT` | L2 | `ble_gatt_security.h` |
| `f206` | Bulk Session Control | WRITE | AUTH (SECURE) / ENC (OPEN) | L3/L2 | `ble_gatt_security.h` + `wifi_bulk_session` |
| `f207` | Bulk Session Status | READ/NOTIFY | ENC | L2 | `wifi_bulk_status` |

### 8.4 Record Sync Service (`...def4`)

| UUID | Characteristic | Access | Perm bits | Derived level | Source |
| --- | --- | --- | --- | --- | --- |
| `f401` | Control | WRITE | `BT_GATT_PERM_WRITE_ENCRYPT` | L2 | `ble_record_sync.c` |
| `f402` | Data | NOTIFY | char `NONE`; CCC `READ \| WRITE_ENCRYPT` | L2 (CCC) | `ble_record_sync.c` |
| `f403` | Status | READ/NOTIFY | `READ_ENCRYPT`; CCC `READ \| WRITE_ENCRYPT` | L2 | `ble_record_sync.c` |

### 8.5 SMP / DFU Security Gate

| Endpoint | UUID | Effective gate | Derived level | Source |
| --- | --- | --- | --- | --- |
| SMP Service | `8d53dc1d-1db7-4cd3-868b-8a527460aa84` | MCUmgr transport permission choice `RW_AUTHEN` | L3 | `conf/features/ble_dfu.conf:41` |
| SMP Characteristic | `da2e7828-fbce-4e01-ae9e-261174997c48` | MCUmgr transport permission choice `RW_AUTHEN` | L3 | `conf/features/ble_dfu.conf:41` |

### 8.6 Security Baseline Flags Referenced by This Matrix

| Symbol | Value | Source |
| --- | --- | --- |
| `CONFIG_BT_SMP` | `y` | `conf/features/ble.conf:22` |
| `CONFIG_BT_BONDABLE` | `y` | `conf/features/ble.conf:23` |
| `CONFIG_MCUMGR_TRANSPORT_BT_PERM_RW_AUTHEN` | `y` | `conf/features/ble_dfu.conf:41` |

This file is intended to be the authoritative BLE security gap database for current firmware behavior versus production-required hardening.
