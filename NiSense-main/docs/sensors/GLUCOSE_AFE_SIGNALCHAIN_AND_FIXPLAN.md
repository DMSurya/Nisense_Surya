# Glucose NIRS — Step 0 Resolutions, Signal-Chain Math, FW Findings, Calibration Fix Plan

**Date**: 2026-08-02  
**Depends on**: [`GLUCOSE_ANALOG_FRONTEND.md`](GLUCOSE_ANALOG_FRONTEND.md), [`GLUCOSE_AFE_REVIEW.md`](GLUCOSE_AFE_REVIEW.md)  
**Contract**: Analysis + phased fix **plan** only (no implementation in this pass).

---

## 1. Step 0 resolutions

### 0a. INA122 RG — definitive status

| Item | Result |
|------|--------|
| Bench DMM | **Not available** in this environment (no continuity measurement performed) |
| Schematic re-derive | PyMuPDF word-coord extract of `wearable-SNR-V2 Schematic.pdf`: `INA122UA` / `RG` labels at ~`(60–68, 540–554)`; ground text at ~`(87.5, 566)` adjacent; **R11/R12 = NC,0603**; **R10 = 1M,0603** nearby at ~`(60, 582)`. PDF text alone **cannot** prove copper net identity (ground symbol vs floating net ambiguity remains). High-res pixmap crop of that region did not yield a reliable visual in automation. |
| Owner prior visual (2026-08-01) | Pins 1 & 8 on same net as GND; no Rg between 1–8; R10 attributed to OPA358-side netting, not RG |
| TI guidance | Datasheet Rev. B: “Place a gain resistor between pin 1 and pin 8”; table **G = 5 … 10000** with **NC → G = 5**. No errata/app-note/E2E hit documenting intentional **RG shorted to GND** as a valid mode. Shorted RG is **outside** recommended operation → treat as **probable design/assembly defect**, not a feature. |

**Definitive statement (with honesty bound):**

> **RG is treated as shorted to GND → gain is non-standard / indeterminate (probable fault condition per TI datasheet).**  
> Evidence: owner visual + schematic GND adjacency.  
> **Still open for electrical closure:** DMM continuity pin1–pin8 and pin1/8–GND. If that measures ~1 MΩ between 1–8, then R10 is Rg and **G ≈ 5.2** — correct the AFE doc and this file immediately.

**Working assumptions for Step 1:**

- **Case Short (primary):** IA gain indeterminate / effectively “max”; headroom **at-risk**.  
- **Case G=5 (alternate if DMM shows open/NC):** datasheet-compliant; headroom evaluated below.

### 0b. Sample rate — 2 Hz or 100 Hz?

| Source | Value | Notes |
|--------|-------|-------|
| Active DT [`40_adc_glucose.overlayinc`](../../boards/raytac_overlay/40_adc_glucose.overlayinc) | **`sample-rate-hz = <2>`** | Shipping product config |
| Git history | Was `<100>` from initial glucose DT; changed to `<2>` in **`9400f6de` (2025-11-06)** in the **same commit that added `glucose_algorithm_matrices.*`** | Comment left stale: still said “100 Hz = 10ms” |
| Kconfig `GLUCOSE_SENSOR_DEFAULT_SAMPLE_RATE_HZ` | default **100** since `c8bd98e` (2025-10-23) | Fallback only if DT prop missing; **stale vs board** |
| `model/glucose/**` metadata | **No** sample-spacing / Hz recorded | — |
| PPG path | Uses its own 25/100 Hz configs | Not the origin of glucose DT=2; glucose Kconfig 100 is original glucose default |

**Definitive statement:**

> **Use 2 Hz as the firmware/product sample rate** (active overlay; intentional change co-committed with clinical matrices).  
> **100 Hz is a stale Kconfig default + stale comments** from the original Oct 2025 driver, not the shipping DT.  
> **Clinical training spacing is still not explicitly documented** in `model/glucose/`; strongest circumstantial evidence that training matches 2 Hz is the Nov 2025 matrix+DT co-change.  
> Fastest closure: ask matrix owner, or inspect raw capture logs for inter-sample timestamps if any exist.

---

## 2. Step 1 — Signal-chain margin report

### 2.1 TIA bandwidth (independent of RG)

\[
f_{-3\mathrm{dB}} \approx \frac{1}{2\pi R_f C_f} = \frac{1}{2\pi \cdot 10\,\mathrm{M}\Omega \cdot 0.8\,\mathrm{pF}} \approx \mathbf{19.9\,\mathrm{kHz}}
\]

Single-pole noise bandwidth \(B_n = (\pi/2) f_{-3\mathrm{dB}} \approx \mathbf{31.3\,\mathrm{kHz}}\).

Vs 2 Hz sampling: electrical BW is **not** limiting. LED 2 s settle ≫ \(R_f C_f \approx 8\,\mu\mathrm{s}\) — settle is for LED/tissue/thermal, not TIA poles.

### 2.2 Johnson noise (Rf) vs OPA358

| Contributor | Approx. output-referred (TIA out) | Notes |
|-------------|-----------------------------------|-------|
| Rf Johnson | **~72 µV rms** → **~7.2 pA rms** input | \(v_n=\sqrt{4kTR_f B_n}\), \(T=300\,\mathrm{K}\) |
| OPA358 \(e_n\) (use ~6.5 nV/√Hz, TI OPA358 class) | **~1.1 µV rms** | Dominated by Johnson |
| OPA358 \(i_n\) (~0.5 fA/√Hz FET-class estimate) | **~0.9 µV rms** | Secondary |

**Verdict:** **Rf Johnson noise dominates** OPA358 voltage/current noise at this gain/BW.

### 2.3 Photodiode dark / signal scale (Hamamatsu G11193-03R, KIRD1111E)

| Parameter | Value | Source |
|-----------|-------|--------|
| Responsivity @ 1.55 µm | typ **1.0 A/W** | Hamamatsu table |
| Dark current | typ **100 pA**, max **1.2 nA** @ VR=5 V | Same; **zero-bias PV mode → typically lower** |
| NEP @ λp | typ **4×10⁻¹⁵ W/√Hz** | Same |
| Shunt R | typ **1 GΩ** @ 10 mV | Same |

TIA output for photocurrent \(I\): \(V_\mathrm{TIA}=I\cdot 10\,\mathrm{M}\Omega\).

| \(I_{pd}\) | \(V_\mathrm{TIA}\) | vs Johnson 72 µV |
|------------|--------------------|------------------|
| 100 pA (dark order @ bias) | 1.0 mV | SNR ~23 dB (voltage) |
| 1 nA | 10 mV | comfortable |
| 10 nA | 100 mV | comfortable |
| 330 nA | 3.3 V | **TIA rail** |

Tissue NIRS returns are often sub-nA to tens of nA (geometry-dependent) — **SNR vs Johnson can be marginal at the low end**; ambient/LED mix (1050∥1550) dominates uncertainty more than op-amp noise.

### 2.4 Chain responsivity + SAADC headroom (conditional on 0a)

SAADC (nRF52840): `ADC_GAIN_1_6` + internal **0.6 V** ref → full-scale input **0 … 3.6 V** (Nordic DevZone / PS). 12-bit LSB ≈ **0.88 mV** at pin.

Simplifying assumption for IA: single-ended \(V_\mathrm{ADC} \approx G_\mathrm{INA}\cdot V_\mathrm{TIA}\) with REF=GND (exact differential wiring not re-netlisted here).

| Case | \(G_\mathrm{INA}\) | \(I_{pd}\) for ADC FS 3.6 V | \(I_{pd}\) for TIA 3.3 V rail | Verdict |
|------|-------------------|-----------------------------|-------------------------------|---------|
| **G=5** (datasheet NC) | 5 | ~72 nA (ADC) / 330 nA (TIA first) | TIA rails first ~330 nA | **Safe–marginal** for ~1–50 nA tissue; **at-risk** ≥~70 nA at ADC |
| **Short / max** | ≫5 (undefined; table max 10000) | **≪1 nA** can rail ADC | Extreme | **At-risk** for any plausible lit tissue return |

**Saturation / SNR summary**

- TIA noise floor: OK for multi-nA signals; weak for ~100 pA.  
- **If RG short (primary assumption): headroom at-risk — flag Critical until DMM or Rg fix.**  
- **If G=5: headroom generally usable** if tissue photocurrent stays below ~50–70 nA; still watch ambient + dual-LED sum.

---

## 3. Step 2 — Firmware correctness findings

### F2-1 — `ADC_GAIN_MULTIPLIER` only on voltage path (confirmed)

- **Evidence**: [`glucose_sensor.c`](../../drivers/sensor/glucose/glucose_sensor.c) uses `ADC_GAIN_MULTIPLIER` only when building `voltage_mv` / dark average mV (approx. lines 46–48, 217–218, 276–297, 812).  
- **Algo**: `glucose_intercept(raw_samples, …)` at ~341 — **raw `uint16_t` only**.  
- **Verdict**: Display/logging gain does **not** leak into clinical path. (Stale comment at ~764 still says “INA122 gain” — doc debt.)

### F2-2 — 2 s LED settle vs TIA BW

- Electrical \(\tau = R_f C_f \approx 8\,\mu\mathrm{s}\); 2 s ≈ \(2.5\times10^5\,\tau\).  
- **Verdict**: 2 s is **not** justified by TIA bandwidth; it is a **product/thermal/tissue** settle choice (or round number). Not wrong electrically; not proven optically.

### F2-3 — Dark offset across power cycle

- Init loads `calibration.offset_mv` from DT (`calibration-offset-mv`, default **0**) ~1077–1080.  
- Runtime dark cal writes RAM only; **no NVS/settings save**.  
- Auto-cal default **n**; dark cal path also **broken** (IDLE vs SAMPLING — F-FW-01 in review).  
- **Verdict**: After reboot, offset is **0** (or DT constant). No silent stale RAM — but also **no persistence**. Voltage path subtracts 0; algo never saw offset anyway.

### F2-4 — Saturation detection does not block algo

- Flag `GLUCOSE_SENSOR_FLAG_SATURATION` if `raw >= 4085` (~309–318).  
- **Still calls** `glucose_intercept(raw_samples, …)` afterward.  
- **Verdict**: Saturation is **logged/flagged only**; clipped codes can still produce a mg/dL. Given Step 1 at-risk if RG short, this is **High**.

### F2-5 — Timing vs `health_sched` guard

| Quantity | Value |
|----------|-------|
| LED delay | 2000 ms |
| 80 × (1/2 Hz) | 40 000 ms (79 gaps ≈ 39.5 s if exclusive) |
| Wall ≈ | **~42 s** lit+settle |
| `CONFIG_APP_HEALTH_STEP_GUARD_SEC` default | **90 s** ([`Kconfig`](../../Kconfig) L315–318) |

**Verdict**: Guard **fits** 2 Hz capture with margin. Would also fit 100 Hz (~2.8 s). No truncation mismatch at defaults. (Busy cycle still long vs UX — performance finding stands.)

---

## 4. Step 3 — Calibration fix plan (phased; not code yet)

### Phase A — Unblock dark capture (firmware must-fix)

1. Add `GLUCOSE_SENSOR_STATE_CALIBRATING` (or bypass abort check in `sample_adc` when calibrating).  
2. Ensure LED off; collect N dark samples; compute mean **raw** and **mV**.  
3. Unit/bench test: calibrate returns 0 and non-zero offset on real HW.

### Phase B — Persist offset (follow existing pattern)

**Prefer `config_manager` / Zephyr settings (`cm/app` NVS)** already used for `glucose_num_samples` / `glucose_delay_ms` — extend `struct config_ppg` or add `struct config_glucose_cal { float dark_offset_mv; uint16_t dark_offset_raw; uint32_t timestamp; }`.

Do **not** invent a new storage stack. Factory `calibration` partition is **read-only / flashed offline** — use it only if product wants factory-locked dark pedestals, not field recal.

### Phase C — Product decision gate (algorithm owner)

| Option | Meaning |
|--------|---------|
| **C1 Raw pedestal** | Matrices trained on absolute ADC → **do not** subtract dark from algo input; dark is diagnostic only |
| **C2 Dark-subtracted** | Subtract `dark_raw` from each sample (saturate at 0) **before** `glucose_intercept` |

**Firmware must not silently choose.** Until answered, persist offset but apply only to voltage/BLE path (current semantics), with clear logs.

### Phase D — Drift check at boot

1. Optional quick dark sample (if worn/dark enclosure policy allows).  
2. Compare to NVS value; if \(|\Delta| > N\cdot\sigma\) with \(\sigma\) from Step 1 Johnson≈7 pA → at G=5 ≈ **~0.35 mV** at TIA, ×G at ADC… use **ADC-count threshold** e.g. **4–8 LSB** (~3.5–7 mV at pin) as starting point pending bench.  
3. If drift high: prefer fresh capture + rewrite NVS; flag UI “recalibrate”.

### Phase E — Minimal “wired properly” bench test

1. Power on → dark cal → note `offset_raw` / `offset_mv`.  
2. Power cycle → confirm persisted values reload.  
3. Known reflectance / finger capture → confirm voltage path = raw→mV − offset; confirm algo path matches chosen C1/C2 policy.  
4. Force ADC near 4095 (bright) → confirm `SATURATION` flag **and** (after fix) algo gated or marked invalid.

---

## 5. Doc corrections required

| Doc | Change |
|-----|--------|
| [`GLUCOSE_ANALOG_FRONTEND.md`](GLUCOSE_ANALOG_FRONTEND.md) | Mark RG as **shorted (owner visual; DMM pending)**; sample rate **2 Hz definitive for FW**, 100 Hz stale Kconfig; link this file |
| [`GLUCOSE_AFE_REVIEW.md`](GLUCOSE_AFE_REVIEW.md) | Point F-AFE-01 / sample-rate findings at Step 0; note 9400f6de matrix co-change |
| Kconfig help / overlay comments | Align defaults/comments to **2 Hz** (fix-plan Batch 1) |
| `glucose_sensor.c` ~764 | Remove “INA122 gain factor” wording on `ADC_GAIN_MULTIPLIER` |

---

## 6. Open product / algorithm-owner / hardware decisions

1. **DMM RG**: pin1–pin8 and to GND (closes 0a).  
2. **Dark into algo?** C1 vs C2 above.  
3. **Confirm training Hz** with whoever ran Svasth capture (closes residual 0b clinical).  
4. **If RG short confirmed:** PCB rework — fit Rg for target G (bench) or open for G=5; **re-validate matrices**.  
5. **1050∥1550**: confirm training used both LEDs; whether next rev splits drive for true dual-λ.  
6. **HOMA / glucose claim labeling** (from prior review).

---

## Suggested fix batches (for approval before coding)

| Batch | Contents | Depends |
|-------|----------|---------|
| **1** FW must-fix | Dark-cal state; clamp `num_samples`=80; Kconfig default 2 Hz; sat flag blocks or clears VALID; comment cleanup | None |
| **2** Cal persist | NVS via config_manager; boot drift check; bench procedure | Owner C1/C2 |
| **3** Hardware | Rg fix / DMM; optional LED GPIO split | Bench + PCB |
| **4** Claims/UX | Glucose/HOMA labeling | Product |

---

## References

- TI INA122 Rev. B — gain equation / NC = G=5  
- TI OPA358 — TIA notes; \(e_n\) class ~nV/√Hz  
- Hamamatsu G11193 series KIRD1111E — responsivity, Id, NEP  
- Nordic nRF52840 SAADC — 0.6 V × 6 = 3.6 V FS  
- Git `9400f6de` — DT 100→2 Hz with matrix integration  
