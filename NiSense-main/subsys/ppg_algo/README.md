# PPG Algorithm Subsystem

**Zephyr Out-of-Tree Subsystem for Heart Rate, SpO2, and experimental vascular metrics**

**Last Updated**: 2026-08-05  
**Version**: 2.4.0

> **Wellness-grade disclaimer**: This subsystem implements engineering heuristics for
> wearable HR/SpO2/Hb/BP/HRV. It is **not** FDA/CE cleared. Accuracy claims require a
> device-specific R-curve and ISO 80601-2-61 Arms evidence. Design references:
> [PPG_ALGO_DESIGN_REFERENCES.md](../../docs/architecture/PPG_ALGO_DESIGN_REFERENCES.md).

## Overview

The PPG Algorithm Subsystem (`subsys/ppg_algo/`) processes PPG (and optional accel) to calculate:

| Metric | Status | Notes |
|--------|--------|--------|
| Heart Rate (HR) | Product | Confidence + quality gate |
| SpO2 | Product | R-value; factory R-poly when present |
| Signal quality / PI / SNR | Product | Motion flag; Maxim-style AC SNR |
| Respiration | Product (RESP on) | Needs ≥20 s IR AC |
| HRV (SDNN/RMSSD) | Experimental PRV | ≥20 RR; not ECG HRV |
| Hemoglobin (tHb) | Experimental | Green/IR model; 8–18 g/dL |
| Cuffless BP | Experimental | ≥10 RR + PAR/HR empiric model |

**Windows, staged live, BLE `f01b`, SFH LED stop, and health schedule**:

→ [docs/architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md](../../docs/architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md)

**Medical-grade design references** (standards, papers, vendor ANs):

→ [docs/architecture/PPG_ALGO_DESIGN_REFERENCES.md](../../docs/architecture/PPG_ALGO_DESIGN_REFERENCES.md)

## MAX32664 RAW vs HUB (critical)

| Mode | Kconfig | PPG source | On-chip DSP |
|------|---------|------------|-------------|
| **RAW** (default wearable) | `CONFIG_MAX32664_MODE_RAW=y` | MAX86141 FIFO via hub (SFH7074 LEDs) | **Yes** — preproc → peaks → HR/SpO2/Hb/… |
| **HUB** | `CONFIG_MAX32664_MODE_HUB=y` | MAX32664 WHRM+WSpO2 | **No** — hub provides HR/SpO2 |
| **Standalone** | MAX3010x | `PPG_SOURCE_MAX3010X` | **Yes** — same DSP (pulse path) |
| **PASSTHROUGH** | diagnostic | AFE FIFO bridge | Host DSP / tests |

**Project default** (`conf/features/ppg.conf`): RAW, 25 Hz, RESP+HB+BP+HRV, motion rejection on, **staged live**, **live tail 0**, buffer **20 s / 500**.

Source selection (`ppg_algo.c` + `product_hw_detect()`):

- **Watch** (MAX32664C ready) → `ncs,ppg-primary` (MAX86141 RAW)
- **Pulse** (else) → `ncs,ppg-secondary` (MAX3010x) — separate finger path; not the SFH watch LEDs

## Staged live (watch UX)

When `CONFIG_PPG_ALGO_STAGED_LIVE=y`:

1. **n ≈ 300** — `PARTIAL_VITALS`: compute HR/SpO2; UI → Vital; BLE notify  
2. **Refine ~1 Hz** — recompute while buffer fills toward 500  
3. **Hb/BP/HRV/Resp ready or n = 500** — `PARTIAL_VASCULAR`; UI → Vascular  
4. **n = 500** — `RECORD_READY`: NOR vitals write (authoritative)  
5. **n = session_target** — with product `LIVE_TAIL=0`, same as 500: SHDN SFH LEDs, `MEASUREMENT_COMPLETE` → glucose  

Sample RAM holds **record_target** only; after full, peaks still run but arrays do not grow.

## Application integration

| Layer | Files |
|-------|-------|
| Facade | `src/sensors/ppg.c` — prox guard, staged triggers, BLE notify, health glue |
| Logging | `src/sensors/ppg_logger.c` — NOR record store |
| UI | `vitals_ui.c`, `vascular_ui.c`, `ui_agent.c` — live cards + auto-nav |
| Config | `conf/features/ppg.conf` |

## Architecture

```
Application (UI, logging, BLE)
        ↓ Triggers (SAMPLE / PARTIAL_* / RECORD_READY / COMPLETE)
ppg_algo: buffer → preproc → peaks → HR / SpO2 / quality / Hb / Resp / HRV / BP
        ↓ Sensor APIs
Drivers (MAX86141_PPG + SFH7074 / MAX3010x / LIS2DS12)
```

## Algorithm pipeline (RAW)

### Preprocessing
1. Ambient subtract when DIRECT_AMBIENT SEQ slot is present  
2. DC: recursive MA (`CONFIG_PPG_ALGO_DC_WINDOW_SAMPLES`, default 32 ≈ 1.28 s @ 25 Hz)  
3. AC: raw − DC; SpO2 uses window RMS / mean peak-to-peak amplitude  

### HR
1. Local-maxima peak detect + 300–2000 ms refractory (green AC on MAX86141)  
2. Ectopic filter (±30% of mean RR)  
3. `HR = 60000 / avg(filtered R-R)`  

### SpO2
1. Window AC amplitudes (TI SLAA655 / Maxim PAC)  
2. `R = (AC_red/DC_red)/(AC_ir/DC_ir)`  
3. Factory R-poly when calibration present; else `110 − 25·R`  

### Hemoglobin (experimental)
`R_gr = (AC_g/DC_g)/(AC_ir/DC_ir)`, `SpO2R = DC_ir/(DC_ir+DC_red)`,  
`tHb = K1·R_gr + K2·SpO2R + K3` (Kconfig coeffs ×10). Valid only if green DC/AC plausible and result in 8.0–18.0 g/dL.

### Respiration
IR AC envelope peak-count in 6–30 brpm; requires ≥20 s buffer.

### HRV / BP (experimental)
- HRV: SDNN/RMSSD after ≥20 RR  
- BP: empiric MAP/PP from HR + IR(/Red) perfusion ratio; `bp_valid` needs estimate OK and ≥10 RR  

### Quality gate
Low PI, motion (when enabled), poor SNR, &lt;3 RR, physiologic range, or confidence &lt; threshold clear `hr_valid` / `spo2_valid`. No green on MAX86141 → HR/Hb fail closed.

## Configuration (product overlay)

```kconfig
CONFIG_PPG_ALGO=y
CONFIG_PPG_ALGO_SAMPLE_RATE=25
CONFIG_PPG_ALGO_BUFFER_SECONDS=20
CONFIG_APP_HEALTH_PPG_SECONDS=20
CONFIG_PPG_ALGO_DC_WINDOW_SAMPLES=32
CONFIG_PPG_ALGO_QUALITY_THRESHOLD=50
CONFIG_PPG_ALGO_MOTION_REJECTION=y
CONFIG_PPG_ALGO_HB_ENABLED=y
CONFIG_PPG_ALGO_RESP_ENABLED=y
CONFIG_PPG_ALGO_BP_ENABLED=y
CONFIG_PPG_ALGO_HRV_ENABLED=y
CONFIG_PPG_ALGO_STAGED_LIVE=y
CONFIG_PPG_ALGO_PARTIAL_VITALS_SAMPLES=300
CONFIG_PPG_ALGO_LIVE_TAIL_SAMPLES=0
```

## File structure

```
subsys/ppg_algo/
├── Kconfig / CMakeLists.txt
├── ppg_algo.c / ppg_algo_priv.h   # lifecycle, staged live, finalize, SHDN
├── ppg_samples.c
├── ppg_preprocessing.c
├── ppg_peak_detect.c
├── ppg_hr_calc.c / ppg_spo2_calc.c / ppg_quality.c
├── ppg_hb_calc.c / ppg_resp_calc.c / ppg_hrv_bp_calc.c
├── ppg_triggers.c / ppg_shell.c
└── README.md
```

## Performance (engineering estimates — not clinical claims)

| Metric | Value |
|--------|-------|
| Sample Rate | 10–100 Hz (default 25 Hz) |
| Record window | 500 samples @ 25 Hz (20 s) with RESP |
| Buffer RAM | sized to `record_target` (not session+tail) |
| Max samples ceiling | 800 (`PPG_MAX_SAMPLES`) |
| CPU Load | ~2–10% @ 25 Hz |

Validated Arms accuracy figures are intentionally omitted until a device-specific study exists.

## License

SPDX-License-Identifier: Apache-2.0
