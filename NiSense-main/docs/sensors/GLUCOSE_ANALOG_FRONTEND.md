# Glucose NIRS Analog Front-End (AFE)

**Last Updated**: 2026-08-02 (SMC1050 parallel emitters documented)  
**Schematics**: `docs/schematics/wearable-SNR-V2 Schematic.pdf`, `docs/schematics/WEARABLE_MB-V2-R6 Schematic.pdf`  
**Firmware**: `drivers/sensor/glucose/`, overlay `boards/raytac_overlay/40_adc_glucose.overlayinc`  
**Review (findings)**: [`GLUCOSE_AFE_REVIEW.md`](GLUCOSE_AFE_REVIEW.md)  
**Signal-chain math + fix plan**: [`GLUCOSE_AFE_SIGNALCHAIN_AND_FIXPLAN.md`](GLUCOSE_AFE_SIGNALCHAIN_AND_FIXPLAN.md)

This document is the ground-truth inventory for the discrete optical glucose path
(SMC1550 + SMC1050 → G11193 → OPA358 → INA122 → nRF SAADC). It is **not** the
MAX86141 / MAX3010x PPG chain.

**Board note (2026-08 HW tweak):** in addition to the 1550 nm SMC1550 emitters,
a smaller **SMC1050 (1050 nm)** is fitted on the opposite side of the
photodiode. **1550 nm and 1050 nm LEDs are wired in parallel** (both sides of
the PD share the same `IRLED_POWER` drive). The SNR schematic PDF silkscreen
text may still show only SMC1550 — trust the stuffed BOM / visual inspection.

---

## Framing

There is **no** discrete glucose sensor IC with a register map or bus. The
“sensor” is a passive/analog chain whose only digital touchpoints are:

- GPIO **P0.24** (`IRLED_POWER`) — LED on/off  
- nRF52840 **SAADC AIN1 / P0.03** (`GLUC_SIGNAL`)

Firmware under `drivers/sensor/glucose/` registers a **custom**
`glucose_sensor_driver_api` (async measure, calibrate, triggers). It does **not**
implement Zephyr `sensor_driver_api` (`sample_fetch` / `channel_get`).

Product naming is **literal NIRS glucose** (mg/dL + HOMA-IR), not a PPG
stand-in. PPG (HR/SpO2) is a separate optical path (MAX86141 / MAX3010x).

---

## Signal chain

```
SMC1550 (1550 nm) ──┐
                    ├── parallel, both sides of PD ──► tissue ──► G11193-03
SMC1050 (1050 nm) ──┘         ▲
                              │ GPIO P0.24 IRLED_POWER
                              │ → BC847 → SI2333CDS (high-side)
G11193_OUT → OPA358 TIA (Rf=10 MΩ, Cf≈0.8 pF; EN→3V3)
           → INA122UA (RG shorted to GND; REF=GND)
           → GLUC_SIGNAL → P0.03/AIN1 SAADC → glucose_intercept(80 raw)
```

| Stage | Part | Software | Hardware (confirmed) |
|-------|------|----------|----------------------|
| Emitters | **SMC1550 (1550 nm) + SMC1050 (1050 nm)** | One GPIO on/off for **both** (no PWM, no per-λ mux) | Parallel drive; LEDs on **both sides** of the PD; optional DNP stuffing variants may still apply |
| Drive | BC847 + SI2333CDS | — | Discrete switch from `IRLED_POWER` |
| Detector | G11193-03 (0.9–1.7 µm) | — | Responds to **both** λ; single `G11193_OUT`; zero external reverse bias |
| TIA | OPA358 | — | **Rf = R7 = 10 MΩ**, **Cf = C2 ≈ 0.8 pF** (GJM1555C1HR80WB01D); **EN hard-tied to 3V3** |
| IA | INA122UA | — | **RG pins 1 & 8 shorted together and to GND** (not open → not datasheet G=5); **REF = GND** |
| ADC | nRF SAADC | Channel setup + polled `adc_read` | Overlay: gain `ADC_GAIN_1_6`, `ADC_REF_INTERNAL`, `ADC_ACQ_TIME_DEFAULT`, 12-bit, AIN1 |

**Wavelength note:** Hardware is **dual-λ emitters**, but firmware has a **single**
drive net and a **single** ADC channel. 1050 nm and 1550 nm illuminate
together; the photodiode current is a **mixed** return, not a separable
signal/reference pair. That is not the same as dual-λ differential NIRS.

### INA122 gain note

TI: `G = 5 + 200kΩ / Rg`, recommended **5 … 10000** (Rg ≥ ~20 Ω for G=10000).  
**Open RG → G = 5.** Working assumption (owner visual 2026-08-01 + schematic
GND adjacency): this board **shorts RG to GND** (`Rg ≈ 0`), which is
**outside** the published gain table — treat as **probable defect**, not a
feature. TI has no documented “RG shorted to ground” mode.

**Electrical closure still open:** DMM continuity pin1–pin8 and each to GND.
If that measures ~1 MΩ (R10 nearby on the schematic), then Rg is present and
**G ≈ 5.2** — correct this section immediately. Until then, IA gain is
**non-standard / indeterminate**; headroom math is conditional (see signal-chain
doc).

Firmware voltage display uses `ADC_GAIN_MULTIPLIER = 6` only to undo SAADC
`ADC_GAIN_1_6`. That constant is **not** INA122 gain (some comments wrongly
say it is).

---

## MCU pins and DT

| Net / function | Pin | DT / code |
|----------------|-----|-----------|
| `IRLED_POWER` | P0.24 | `ir-led-gpios = <&gpio0 24 GPIO_ACTIVE_HIGH>` |
| `GLUC_SIGNAL` | P0.03 / AIN1 | `io-channels = <&adc 1>`, `NRF_SAADC_AIN1` |

No firmware control of OPA358 EN or INA122. Amp enable sequencing is N/A
(always on when 3V3 is up).

Overlay measurement defaults (`40_adc_glucose.overlayinc`):

| Property | Value | Notes |
|----------|-------|-------|
| `num-samples` | 80 | Algorithm requires exactly 80 |
| `led-delay-ms` | 2000 | LED settle before sampling |
| `sample-rate-hz` | **2** | 500 ms between samples (~40 s lit capture) |
| `calibration-offset-mv` | 0 | Dark offset unused unless calibrated |
| `calibration-gain` | 1000 (= 1.0) | |

**Product sample rate:** **2 Hz** is definitive for shipping firmware (active
overlay). Changed from 100 → 2 in commit `9400f6de` (2025-11-06) together with
`glucose_algorithm_matrices.*`. Kconfig default 100 and some comments are
**stale**. Clinical training spacing is not written in `model/glucose/`
metadata — confirm with matrix owner / capture logs if absolute certainty needed.

---

## LED timing (firmware)

1. LED ON  
2. Wait `led-delay-ms` (default 2 s)  
3. Take `num-samples` single-shot SAADC reads at `sample-rate-hz`  
4. LED OFF  
5. Run algorithm on dedicated workqueue  

No LED chopping, no dark/lit interleave during a measurement, no SAADC
hardware trigger on LED edge. **No per-wavelength sequencing** — SMC1050 and
SMC1550 share `IRLED_POWER` and turn on/off together.

---

## Data path

| Path | Uses |
|------|------|
| Raw ADC → `glucose_intercept()` | **Glucose mg/dL** (primary) |
| Raw → mV (`(raw/4095)*600*6` − offset × gain) | Logging, BLE live waveform, quality metrics |
| Dark calibrate (`calibrate(0)`) | Fills RAM `offset_mv`; applied to **voltage only** today — **not** to algorithm input |
| Factory `calibration` partition glucose section | Clamp / temp-comp hooks; matrix-factor replace still incomplete |

Dark offset is **not** persisted to NVS. `CONFIG_GLUCOSE_SENSOR_AUTO_CALIBRATE`
defaults **n**.

---

## Related optical paths (do not confuse)

| Path | Parts | Role |
|------|-------|------|
| **Glucose NIRS** | SMC1550 + SMC1050 (parallel) + G11193 + OPA358 + INA122 | This document |
| **PPG (wearable)** | MAX86141 + VEMD5080 / SFH optics | HR/SpO2 via hub; `PD1_IN` / `PD_GND` |
| **PPG (pulse)** | MAX30102 | Standalone PPG |

`health_sched` runs PPG then glucose **sequentially** (shared optical path /
BBOUT current).

---

## Datasheets / references

| Part | Source |
|------|--------|
| SMC1550 (1550 nm) | Roithner / Tech-LED (not vendored in-repo) |
| SMC1050 (1050 nm) | Roithner / Tech-LED (tiny SMD; board stuffing tweak) |
| G11193-03R | Hamamatsu (covers 0.9–1.7 µm → both LEDs) |
| OPA358 | [TI OPA358](https://www.ti.com/lit/ds/symlink/opa358.pdf) (includes TIA design notes) |
| INA122 | [TI INA122](https://www.ti.com/lit/ds/symlink/ina122.pdf) — `G = 5 + 200k/Rg` |
| nRF52840 SAADC | Nordic product specification |
| Schematics | `docs/schematics/` |

PPG AFE reference designs (e.g. TI pulse-ox / AFE440x) are useful topology
sanity checks only; this board uses a continuous LED-on NIRS snapshot, not
pulsed multi-λ ambient cancellation.

---

## Software map

| Layer | Location |
|-------|----------|
| AFE device + sampling | `drivers/sensor/glucose/glucose_sensor.c` |
| Clinical algorithm | `drivers/sensor/glucose/glucose_algorithm*.c` |
| Profiles / XIP model | `glucose_algorithm_profiles.c`, `glucose_model_xip.c`, `model/glucose/` |
| App facade / logger / UI | `src/sensors/glucose.c`, `glucose_logger.c`, `src/ui/glucose_ui.c` |
| Scheduling | `src/sensors/health_sched.c` |
| DT binding | `dts/bindings/sensor/glucose-sensor.yaml` |

---

## Architecture placement (recommendation)

**Do not** fold this into the PPG/`ppg_algo` abstraction — different emitter,
detector, timing, and clinical output.

Preferred end state (incremental is fine):

1. **Keep a thin AFE/acquisition driver** (GPIO LED + SAADC + measure state
   machine) as a Zephyr `DEVICE` with the existing custom API — optionally
   rename/move later to something like `drivers/afe/glucose_nirs/` so it is
   not mistaken for a `sensor_driver_api` IC driver.  
2. **Keep clinical math + matrices** clearly layered (already partly separate
   files); long-term peer of `subsys/ppg_algo/` is reasonable
   (`subsys/glucose_algo/` or stay beside the AFE under a `glucose/` module).  
3. **Short term:** remaining under `drivers/sensor/glucose/` is acceptable if
   this document stays the source of truth and new code does not pretend the
   part is a register-mapped sensor IC.

---

## Open follow-ups

See severity-ranked findings in [`GLUCOSE_AFE_REVIEW.md`](GLUCOSE_AFE_REVIEW.md)
and Step 0–3 plan in
[`GLUCOSE_AFE_SIGNALCHAIN_AND_FIXPLAN.md`](GLUCOSE_AFE_SIGNALCHAIN_AND_FIXPLAN.md).
Remaining product decisions:

- Confirm with matrix owners that training used **2 Hz / 500 ms** spacing.  
- Decide whether dark subtraction should affect **raw** algo input, then
  persist offset (NVS or factory calibration section).  
- Approve a follow-up **fix plan** (firmware / PCB / claims batches).
