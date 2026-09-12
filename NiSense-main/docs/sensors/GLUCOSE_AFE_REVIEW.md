# Glucose NIRS AFE — Correctness / Performance Review

**Date**: 2026-08-02 (emitters amended same day: SMC1050 + SMC1550 parallel)  
**Contract**: Findings only (A3). No code or PCB changes in this pass.  
**Scope**: AFE + firmware + algorithm plumbing + clinical/NIRS plausibility (B3).  
**Hardware baseline**: [`GLUCOSE_ANALOG_FRONTEND.md`](GLUCOSE_ANALOG_FRONTEND.md)  
**Step 0 / signal-chain / calibration fix plan**:
[`GLUCOSE_AFE_SIGNALCHAIN_AND_FIXPLAN.md`](GLUCOSE_AFE_SIGNALCHAIN_AND_FIXPLAN.md)

This is not a regulatory or clinical-validation report. Severities describe
engineering risk to correctness, reproducibility, power, or product claims.

---

## Executive summary

| Severity | Count | Themes |
|----------|------:|--------|
| Critical | 3 | INA122 RG short; dark-cal state machine broken; clinical claim vs ganged dual-λ (no separable spectra) |
| High | 7 | Dark offset unused by algo; BLE sample-count vs 80; absolute ADC; HOMA/insulin UX; pulse interim coeffs; no ambient reject; ~41 s LED-on |
| Medium | 5 | Sample-rate Kconfig drift; factory temp-comp unwired; HOMA formula opacity; TIA dynamic range; always-on amps |
| Low / Info | 4 | Oversampling; profile default; TIA BW OK; BLE rate OK |

**Top actions for a later fix plan (not done here):**

1. Hardware: give INA122 a real `Rg` (or prove intentional max-gain design on bench).  
2. Firmware: fix dark-cal state gate; decide dark policy for algo; clamp `num_samples` to 80 when algo requires it.  
3. Product: qualify UI/BLE “glucose mg/dL” / HOMA claims against **ganged 1050+1550** continuous-on limits (not separable dual-λ); get matrix-owner answers below.

---

## Pass 1 — Analog front-end

### Bounds (computed)

| Quantity | Value | Notes |
|----------|-------|-------|
| TIA `f≈1/(2π Rf Cf)` | **~20 kHz** | Rf=10 MΩ, Cf≈0.8 pF — adequate vs 2 Hz sampling |
| TIA `Ipd` at Vout=3.3 V | **~330 nA** | Upper bound before TIA rails (single-supply) |
| Optical power @ 1 A/W | **~0.33 µW** | At that Ipd (order-of-magnitude) |
| SAADC full-scale | **0 … 3.6 V** | `ADC_GAIN_1_6` + 0.6 V internal ref |
| SAADC LSB at pin | **~0.88 mV** | 12-bit |
| Lit capture | **~41.5 s** | 2 s settle + 79×500 ms |
| LED energy @ 50 mA | **~0.58 mAh** | Order-of-magnitude; actual IF from discrete path unknown |

### F-AFE-01 — Critical — INA122 RG shorted to GND

- **Layer**: AFE  
- **Evidence**: Owner visual + schematic GND adjacency (see Step 0a in signal-chain doc). TI INA122: `G = 5 + 200k/Rg`, table **G=5…10000** (Rg ≥ ~20 Ω); open RG → G=5. No TI guidance for RG shorted to GND. **DMM still pending** to close residual R10=1M ambiguity.  
- **Impact**: Gain is outside the published operating table (`Rg≈0`). Risk of extreme noise gain, oscillation, output rail, or undefined transfer function. Algorithm trained on raw ADC then inherits that undefined frontend.  
- **Direction**: DMM first; then fit discrete `Rg` for intended G (bench-choose), or open RG for G=5 if short was fab error. Re-collect or re-validate matrices after change.

### F-AFE-02 — Medium — TIA optical dynamic range is tiny in current

- **Layer**: AFE  
- **Evidence**: Rf=10 MΩ → ~330 nA to rail; G11193 ~1 A/W @ 1.55 µm.  
- **Impact**: Easy saturation with strong return; or buried signal if LED/tissue coupling is weak. Continuous 41 s LED does not expand dynamic range.  
- **Direction**: Bench map ADC codes LED-off / finger / open-air; adjust Rf and/or LED current; document target code window.

### F-AFE-03 — High — No optical ambient rejection in AFE

- **Layer**: AFE / Clinical  
- **Evidence**: Continuous LED-on; no dark/lit chop; no second wavelength; contrast PPG AFE440x-style ambient sample.  
- **Impact**: Room IR / stray light / motion / perfusion walk into the same absolute ADC the algo uses.  
- **Direction**: Firmware interleaved dark samples and/or hardware chop. Board already has **1050 + 1550** LEDs, but they are **paralleled** — to use 1050 as a reference λ, drive must be sequenced (separate GPIOs or mux) and firmware must sample per-λ.

### F-AFE-04 — Low — Amps always powered

- **Layer**: AFE / Perf  
- **Evidence**: OPA358 EN→3V3; no INA shutdown pin in DT.  
- **Impact**: Quiescent draw whenever 3V3 is up; secondary to LED energy during measure.  
- **Direction**: Next PCB: MCU-controlled EN if battery budget requires.

### F-AFE-05 — Info — TIA bandwidth vs sample rate is fine

- **Layer**: AFE  
- **Evidence**: ~20 kHz ≫ 2 Hz Nyquist needs.  
- **Impact**: Bandwidth is not the limiting correctness issue; gain/offset/ambient are.

---

## Pass 2 — Firmware acquisition

### F-FW-01 — Critical — Dark calibration cannot sample (state gate)

- **Layer**: FW  
- **Evidence**: [`glucose_sensor.c`](../../drivers/sensor/glucose/glucose_sensor.c) `glucose_sensor_sample_adc()` requires `state == SAMPLING` or returns `-ECANCELED`. `glucose_sensor_impl_calibrate(0)` runs only when `IDLE` and never sets `SAMPLING` before calling `sample_adc`.  
- **Impact**: Dark cal fails (or aborts at sample 0). `CONFIG_GLUCOSE_SENSOR_AUTO_CALIBRATE` path is broken. Offset stays DT/0.  
- **Direction**: Allow a CALIBRATING state, or bypass the abort check for dark cal; then persist offset.

### F-FW-02 — High — Dark offset never reaches `glucose_intercept()`

- **Layer**: FW / Algo  
- **Evidence**: Offset applied only when building `voltage_mv`; algo called with `raw_samples`. DT default offset 0; auto-cal default n.  
- **Impact**: Even a working dark cal would not change mg/dL unless matrices expect absolute ADC including dark. Voltage/BLE plots diverge from algo input.  
- **Direction**: Matrix owner decides absolute vs delta; if delta, subtract dark from raw (or float copy) before intercept; persist offset.

### F-FW-03 — High — BLE/NVS can set `num_samples ≠ 80` → algo hard-fail

- **Layer**: FW / Algo  
- **Evidence**: `glucose_sensor_configure` allows 10–500; `glucose_intercept` requires exactly 80; BLE `f01b` / `config_manager` can change count.  
- **Impact**: Measurement completes with `GLUCOSE_SENSOR_FLAG_ALGO_ERROR`, glucose 0 — easy to trigger from app Settings.  
- **Direction**: Clamp configure to 80 while algo is fixed-length, or make algo length configurable with matching coeff tables.

### F-FW-04 — Medium — Sample-rate source-of-truth drift

- **Layer**: FW  
- **Evidence**: Overlay `sample-rate-hz = <2>` (definitive for shipping FW — Step 0b). Was 100 until `9400f6de` (co-changed with matrices). Kconfig `GLUCOSE_SENSOR_DEFAULT_SAMPLE_RATE_HZ` default **100** remains stale. Coeffs are per-index, so spacing is load-bearing.  
- **Impact**: Any path that builds without the overlay default (or future DT miss) trains the wrong time base into the same coeffs.  
- **Direction**: Align Kconfig default to 2; confirm training rate with matrix owner; fail boot if DT ≠ expected.

### F-FW-05 — Low — No SAADC oversampling / long acq time

- **Layer**: FW / Perf  
- **Evidence**: Overlay `ADC_ACQ_TIME_DEFAULT`; no oversampling; polled `adc_read`.  
- **Impact**: Extra noise at 2 Hz; jitter of software sleep vs true 500 ms. Secondary to ambient/gain issues.  
- **Direction**: Enable oversampling; optionally timer-driven sampling.

### F-FW-06 — Info — Optical serialization with PPG is correct intent

- **Layer**: FW  
- **Evidence**: `health_sched` glucose after PPG; shared path / BBOUT.  
- **Impact**: Avoids concurrent LED fight; does not fix ambient during the glucose window itself.

---

## Pass 3 — Algorithm plumbing

### F-ALG-01 — High — Prediction is a linear combination of absolute ADC codes

- **Layer**: Algo  
- **Evidence**: `weighted_coefficients[i] = data[i] * svasth_coeff[i]` then intercept; no dark/lit ratio, no AC extraction.  
- **Impact**: Any DC shift (temp, contact, ambient, amp offset, LED aging) moves Y1 and final mg/dL.  
- **Direction**: Confirm training used the same absolute scale; prefer normalized or differential features if re-training.

### F-ALG-02 — High — Pulse profile uses interim wearable regression coefficients

- **Layer**: Algo  
- **Evidence**: Comment in [`glucose_algorithm_profiles.c`](../../drivers/sensor/glucose/glucose_algorithm_profiles.c): pulse `regression_coefficient` / `regression_constant` are wearable interim until customer supplies pulse table. Pulse has its own `svasth_coeff` and %-bands.  
- **Impact**: Pulse SKU group regression stage is not fully pulse-calibrated.  
- **Direction**: Obtain pulse-specific regression table; gate pulse glucose on model readiness.

### F-ALG-03 — High — HOMA-IR often uses default fasting insulin, not a measured value

- **Layer**: Algo / Clinical  
- **Evidence**: Default `DEFAULT_FASTING_INSULIN_UIU_ML` (Kconfig, typically 10.0); device has no insulin assay; HOMA still computed and BLE-notified.  
- **Impact**: HOMA-IR on UI/BLE can look like a measured metabolic index while insulin is assumed.  
- **Direction**: Require explicit user insulin entry before HOMA; label as “estimated with entered insulin”; hide otherwise.

### F-ALG-04 — Medium — HOMA implementation is opaque vs classic formula

- **Layer**: Algo  
- **Evidence**: Multi-step path with +5% glucose calibration then ratios; algebraically inverse ratio collapses toward fasting insulin, then `(inverse × calibrated_glucose)/405` ≈ classic HOMA with inflated glucose. BLE fallback elsewhere uses classic `(g×i)/405` directly.  
- **Impact**: Driver HOMA and BLE seed fallback can disagree; documentation claims Matthews formula while applying +5% and odd intermediates.  
- **Direction**: One shared helper implementing classic HOMA; drop unused intermediates or document intentional calibration factor.

### F-ALG-05 — Medium — Factory glucose temp-comp never gets skin temperature in-driver

- **Layer**: Algo / Cal  
- **Evidence**: `calibration_apply_glucose_mg_dl(&v, NAN)` in sensor path; factory section supports slope/reference.  
- **Impact**: Temp compensation in calibration partition is inert for live measures.  
- **Direction**: Pass wrist/finger temp from health_sched into apply path.

### F-ALG-06 — Low — Active profile defaults to pulse until `glucose_init`

- **Layer**: Algo  
- **Evidence**: `active_profile = &pulse_profile` static; `glucose_algorithm_select_profile()` from `glucose_init`.  
- **Impact**: Harmless if no measure before init; footgun for early tests.  
- **Direction**: Default null / require select; assert profile matches SKU at measure start.

---

## Pass 4 — Clinical / NIRS plausibility

Literature consensus used for sanity check (not exhaustive):

- Glucose NIR signal is weak and overlapped; **multivariate** methods (PLS etc.) and **multi-λ** or broad spectra are standard ([Goodarzi et al. / NIR CGM reviews](https://lirias.kuleuven.be/retrieve/306371); biosensors ML/NIR studies).  
- **1550 nm** sits in a useful overtone region; **1050 nm** is a shorter NIR band often used for deeper penetration / scatter context. Published feasibility work typically needs **separable** multi-λ or spectra + chemometrics ([SPIE dual-λ designs](https://doi.org/10.1117/12.2503852); NIR CGM reviews).  
- **Hardware update (2026-08):** board stuffs **SMC1050 + SMC1550 in parallel** on both sides of the G11193 — dual emitters, **one** `IRLED_POWER`, **one** ADC channel → mixed photocurrent, not differential dual-λ.  
- Ambient / perfusion / temperature remain first-class confounders; pulsed AFEs with ambient subtraction are normal in PPG, rare here.

### F-CLN-01 — Critical — Product presents mg/dL without separable dual-λ spectroscopy

- **Layer**: Clinical / Product  
- **Evidence**: Emitters are **1050 nm + 1550 nm** but ganged on one GPIO into one PD/ADC; algo is still a univariate 80-sample ADC → `glucose_mg_dl` path; literature dual-λ designs sequence or spectrally separate signal vs reference.  
- **Impact**: Having two LEDs does **not** by itself implement reference-wavelength correction. Users may still treat readings as capillary-equivalent glucose.  
- **Direction**: Label as investigational NIRS index unless validated; or sequence λ (HW + FW) and retrain; document that current optical return is a **1050∥1550 mix**.

### F-CLN-02 — High — No subject-specific or session calibration in the live path

- **Layer**: Clinical  
- **Evidence**: Global Svasth coeffs + group matrices; factory clamp only; dark/gain cal incomplete.  
- **Impact**: Inter-person tissue differences dominate weak glucose optical contrast.  
- **Direction**: Document required calibration protocol; support per-user affine if clinically required.

### F-CLN-03 — High — HOMA-IR surfaced without measured insulin (see F-ALG-03)

- **Layer**: Clinical  
- **Evidence**: Same as F-ALG-03; HOMA definition requires fasting glucose **and** fasting insulin.  
- **Impact**: Misleading metabolic resistance index.  
- **Direction**: Same as F-ALG-03.

### F-CLN-04 — Info — Emitter wavelengths (1050 + 1550 nm) are individually plausible

- **Layer**: Clinical  
- **Evidence**: 1550 nm in first-overtone / glucose-discussed band; 1050 nm common shorter NIR; G11193 covers both. Dual emitters are a sensible *parts* choice.  
- **Impact**: Plausible components ≠ separable dual-λ method while drive/ADC remain single-channel.

### F-AFE-06 — High — Dual-λ LEDs paralleled (firmware cannot isolate bands)

- **Layer**: AFE / FW  
- **Evidence**: HW tweak: SMC1050 opposite side of PD from SMC1550; both on `IRLED_POWER`. Docs: [`GLUCOSE_ANALOG_FRONTEND.md`](GLUCOSE_ANALOG_FRONTEND.md).  
- **Impact**: Training/optics assume a mixed spectrum; current draw is sum of both LEDs; cannot implement ambient or water-reference subtraction by λ without a respin or drive split.  
- **Direction**: Confirm stuffing map (which D# is 1050 vs 1550); decide if next rev splits GPIO; update BOM vs schematic silkscreen.

---

## Pass 5 — Performance

### F-PRF-01 — High — ~41.5 s IR LED on per measurement

- **Layer**: Perf / UX / Power  
- **Evidence**: DT 2 s + 80 @ 2 Hz; health cadence can stack after 20 s PPG.  
- **Impact**: Battery, thermal, user motion during capture, long UI wait; optical exposure.  
- **Direction**: Confirm clinical need for 40 s; if training allows, higher rate / shorter window with new coeffs.

### F-PRF-02 — Medium — End-to-end cycle time vs adaptive health ladder

- **Layer**: Perf  
- **Evidence**: PPG (~20 s) + glucose (~42 s) + temps → minute-scale busy; adaptive intervals 5–30 min.  
- **Impact**: Manual/auto overlap risk mitigated by busy flags; still heavy for frequent critical cadence.  
- **Direction**: Revisit ladder mins if glucose stays 40 s.

### F-PRF-03 — Info — Algo CPU / 2 Hz BLE stream

- **Layer**: Perf  
- **Evidence**: Dedicated workq stack 5120; matrix lookup once per measure; BLE sample notify at 2 Hz.  
- **Impact**: Unlikely to be the primary bottleneck vs LED-on time.

---

## Questions for matrix / clinical owner

1. Exact **inter-sample delay** used when collecting the 80-point training sets (confirm 500 ms)?  
2. Were features **absolute ADC**, dark-subtracted, or otherwise normalized?  
3. Was INA122 intended as **G=5 (open RG)** or max gain (short)? Any bench gain measurement?  
4. Validation metrics (Clarke/Parkes, MARD, subject count) for wearable vs pulse packs?  
5. Is pulse **regression** table still interim wearable values?  
6. Should HOMA appear in product UX without lab insulin?  
7. Were training sets collected with **1050+1550 both on**, 1550-only, or sequenced? Does the model assume the mixed spectrum?

---

## Suggested fix-plan batches (for next approval pass)

| Batch | Items | Risk |
|-------|-------|------|
| 1 Firmware must-fix | F-FW-01, F-FW-03, F-FW-04 align defaults | Low HW risk |
| 2 Calibration policy | F-FW-02 + persist; F-ALG-05 temp wire | Needs owner answer on absolute vs delta |
| 3 Hardware / PCB | F-AFE-01 Rg; F-AFE-02 Rf/LED; optional EN | Respin / bench |
| 4 Claims / UX | F-CLN-01…03, F-ALG-03/04 | Product copy + BLE |
| 5 Perf / ambient | F-PRF-01, F-AFE-03 | May need re-train |

---

## References (review)

- Hardware inventory: [`GLUCOSE_ANALOG_FRONTEND.md`](GLUCOSE_ANALOG_FRONTEND.md)  
- TI INA122, OPA358 datasheets; Hamamatsu G11193; Nordic SAADC (0.6 V × 6 = 3.6 V FS)  
- NIR glucose chemometrics review (Leuven / related NIR CGM literature)  
- Dual-λ / 1550 nm feasibility discussions (SPIE and related)  
- Blood-flow confounding at 1550 nm (Sensors 2025)  
