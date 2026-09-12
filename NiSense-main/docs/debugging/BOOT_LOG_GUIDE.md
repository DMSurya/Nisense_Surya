# Boot Log Guide — Errors and Warnings

**Last Updated**: 2026-07-20  
**Platform**: nRF52840 · nRF Connect SDK v3.3.0

Use this guide when reviewing RTT boot logs (interactive shell is USB CDC when
`APP_FEATURE_SHELL=y`, not RTT). Most `<wrn>` and `<err>` lines during **sensor init**
are expected on the WEARABLE (GWEC) hardware profile.

See also: [RTT_DEBUG.md](RTT_DEBUG.md) · [SENSOR_STATUS.md](../sensors/SENSOR_STATUS.md) · [FACTORY_CALIBRATION.md](../hardware/FACTORY_CALIBRATION.md)

---

## Quick verdict

A healthy boot ends with early lines similar to:

- `Product HW: WATCH` (or `PULSE`) / `Product SKU: WATCH`
- `Verifying Resource store` → Resource init OK / CRC
- `Verifying Model store` → model pack CRC + SKU glucose entry OK
- `MAX32664 RAW Mode initialization complete` (Watch RAW builds)
- `Wrist body temperature sensor ready (MAX30208)` (Watch)
- `Glucose profile: WEARABLE` or `PULSE` with model validated
- `record_store` init / ready (when persistence enabled)
- `Screen 0 (Home) created successfully`
- `BLE advertising started` (when `CONFIG_BT=y`)

If those appear, later sensor-probe warnings below are usually **not** actionable.

---

## Expected warnings (WEARABLE / GWEC)

### `max3010x_ppg: MAX3010x not present (Part ID read failed: -5)`

| Field | Detail |
|-------|--------|
| **Cause** | Standalone MAX30102 is not on I2C0; PPG is **MAX86141 via MAX32664 hub** on I2C1. |
| **Action** | None. MAX3010x driver probes at boot; `-5` is `-EIO` / not responding. |

### `max302xx: Part ID 0x00 doesn't match...` / `Failed to read config register`

| Field | Detail |
|-------|--------|
| **Cause** | Unified MAX302xx driver probes Part ID register `0xFF`, then tries MAX30205 fallback before succeeding on retry. |
| **Typical resolution** | `MAX30208 detected (Part ID: 0x30)` — alternate Part ID for some MAX30208 variants. |
| **Action** | None if final line shows detection + `temp: Wrist body temperature sensor ready`. |

### `max32664: Hub command error, status: 0x01`  
`RAW: report period cmd returned ret=-22 status=0x01 (continuing)`

| Field | Detail |
|-------|--------|
| **Cause** | Hub firmware **30.2.2** returns `ERR_UNAVAIL_FUNC (0x01)` for AN6924 “samples report period” (`0x10 0x02`). |
| **Driver behaviour** | Logs and **continues** — see `drivers/sensor/max32664/max32664_raw.c`. |
| **Action** | None unless PPG FIFO is empty at runtime. |

### `calibration: Calibration magic invalid: 0xffffffff`  
`main: Factory calibration unavailable: -2`

| Field | Detail |
|-------|--------|
| **Cause** | External `calibration_model` partition is erased (`0xFF`) or never programmed. Expected magic: `CALB` (`0x43414c42`). |
| **Runtime effect** | App uses device-tree defaults and algorithm profiles; PPG/glucose factory trim sections are **not** applied yet. |
| **Action** | Program factory cal in production only — see [FACTORY_CALIBRATION.md](../hardware/FACTORY_CALIBRATION.md). |

---

## Informational (not faults)

| Log | Meaning |
|-----|---------|
| `Reset cause: 0x00000002` | Bit 1 = pin reset (debugger, button, PMIC). Normal after flash. |
| `RTC time valid (preserving stored clock)` | RTC not overwritten with build time. |
| `health_sched: Waiting for wear detection` | Auto measurement cycle arms when VCNL3040 reports worn. |
| `temp: Wear state changed: 0` | Not on wrist at boot. |
| `ui_obj_track: LVGL CREATE screen home` | UI lifecycle debug enabled (`CONFIG_APP_UI_OBJ_LIFECYCLE_DEBUG`). |

---

## Boot timeline gaps

```
[00:00:07.xxx] *** Booting Zephyr OS build ***
[00:00:30.xxx] === APP MAIN START ===
```

A multi-second gap between Zephyr banner and `APP MAIN START` is common:

1. **RTT not connected** early — logs appear only after the viewer attaches.
2. **Startup logo** — `CONFIG_UI_STARTUP_LOGO=y` shows XIP logo before `main()` (typically 1–3 s once `main` runs).
3. **Target reset** — J-Link “TARGET RESET” between capture segments merges two boot phases.

---

## When to investigate

| Symptom | Where to look |
|---------|----------------|
| No `MAX32664` / `MAX86141 detected` | I2C1 wiring, hub power, `0x55` probe in `i2c_probe` |
| Repeated MAX302xx errors **after** init | I2C bus, sensor power, address `0x53` |
| Boot loop / no `APP MAIN START` | MCUboot, hardfault — use Ozone ([OZONE_MCUBOOT_GUIDE.md](OZONE_MCUBOOT_GUIDE.md)) |
| PPG measurements fail at runtime | [subsys/ppg_algo/README.md](../../subsys/ppg_algo/README.md), hub RAW FIFO |
| Glucose always invalid | Wearable vs pulse profile; XIP glucose model flashed (`flash_glucose_model.ps1`) |
| UI hardfault on screen change | [UI_GUIDE.md](../ui/UI_GUIDE.md) lifecycle debug |

---

## Reducing log noise

In `conf/features/display.conf` (or menuconfig):

```text
CONFIG_APP_UI_OBJ_LIFECYCLE_DEBUG=n
CONFIG_UI_OBJ_TRACK_LOG_LEVEL_INF=y
CONFIG_UI_LOG_LEVEL_INF=y
```

Lower hub/charger modules to `INF` or `WRN` in menuconfig under **Logging**.

---

## Reference: hub status `0x01`

| Status | Name | Typical meaning |
|--------|------|-----------------|
| `0x00` | SUCCESS | Command accepted |
| `0x01` | ERR_UNAVAIL_FUNC | Not supported on this FW variant |
| `-22` (`EINVAL`) | Host | Command rejected or bad state; driver may retry or continue |
