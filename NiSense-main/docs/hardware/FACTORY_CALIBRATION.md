# Factory Calibration Partition

**Last Updated**: 2026-07-21

Production calibration data is stored in external QSPI flash and is **read-only at runtime**. OTA does not update this partition.

---

## Partition layout

| Property | Value |
|----------|-------|
| DT label | `calibration_model` |
| Flash offset | **`0x2f6000`** (8 MB external) |
| XIP / runtime address | `0x122f6000` (`0x12000000 + 0x2f6000`) |
| Size | 64 KB |
| Magic | `CALB` (`0x43414c42`) |

See also: [HARDWARE_REFERENCE.md](HARDWARE_REFERENCE.md) · [PARTITION_LAYOUT.md](../build/PARTITION_LAYOUT.md) · `include/calibration.h`

---

## Boot behaviour

At startup `calibration_init()` in `src/ota/calibration.c` maps the partition via XIP and validates magics:

| Log | Meaning |
|-----|---------|
| `Calibration initialized: serial=…` | Header magic OK; getters may return sections |
| `cal applied: ppg=y/n glucose=y/n temp=y/n` | Which sections have valid section magic for apply |
| `Calibration magic invalid: 0xffffffff` | Partition erased — **normal on dev boards** |
| `Factory calibration unavailable: -2` | App continues with algorithm defaults |

Erased flash → no apply paths change measured values.

---

## Runtime apply (2026-07-30)

| Section | Applied fields | Where | Still skipped |
|---------|----------------|-------|----------------|
| PPG | `spo2_offset/scale`, `hr_offset_bpm/scale`, **`r_ratio_coefficients[]`** (AN6845-style poly when non-zero), **LED currents → RAW PA seed** | `ppg_spo2_calculate` / `max32664_raw_seed_led_pa_from_cal` | Melanin correction |
| Temp | `offset_celsius`, `scale_factor`, optional min/max clamp | `temp_read_wrist` / `temp_read_finger` | Polynomial / ambient slope; **not** SoC die temp |
| Glucose | min/max clamp; temp-comp if skin temp passed | `glucose_sensor` after intercept | `calibration_factors[]` matrix replace; in-driver skin temp is NaN (temp-comp inactive until wired) |

API: `calibration_apply_ppg_hr_spo2()`, R-poly inside `ppg_spo2_calculate()`, `max32664_raw_seed_led_pa_from_cal()`, `calibration_apply_temp_c()`, `calibration_apply_glucose_mg_dl()` in `include/calibration.h`.

Host AGC (`max32664_raw_host_agc`) steps PA between measurements toward mid IR DC after the factory seed.
---

## Data sections (header + payloads)

| Section | Offset | Purpose |
|---------|--------|---------|
| Header | `0x0000` | Magic, version, flags, CRC |
| PPG | `0x0100` | LED currents, SpO2/HR offsets |
| Temperature | `0x1100` | Temp trim |
| Glucose | `0x1900` | Clamp / temp-comp / factors |
| Accelerometer | `0x3900` | Accel trim |
| MAX32664 | `0x4100` | Hub-specific |
| Display | `0x5100` | Panel trim |

---

## Programming (production)

Use the common QSPI stores script (Resource + Model + Calibration in one pass):

```powershell
.\scripts\flash\flash_qspi_stores.ps1
# or calibration only:
.\scripts\flash\flash_qspi_stores.ps1 -Targets calibration -CalibrationFile path\to\calibration.bin
```

Binaries default under `$BuildDir`:
- `resource.bin` → `0x0F6000`
- `model_pack.bin` → `0x1F6000`
- `calibration.bin` → `0x2F6000` (skipped if missing unless `-RequireAll`)

Never write calibration from the application at runtime.


---

## Related

- [SENSOR_STATUS.md](../sensors/SENSOR_STATUS.md)  
- [BOOT_LOG_GUIDE.md](../debugging/BOOT_LOG_GUIDE.md) — *calibration magic invalid*  
