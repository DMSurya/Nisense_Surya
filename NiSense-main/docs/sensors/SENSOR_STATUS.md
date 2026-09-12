# Current Sensor Implementation Status

**Last Updated**: 2026-07-21

> **Hardware profiles** — the same firmware binary supports two configurations:
> | Profile | Form factor | Primary PPG | Glucose path |
> |---------|-------------|-------------|--------------|
> | **WEARABLE** | Wrist band | MAX32664 hub + MAX86141 | Not primary (optical crosstalk avoidance) |
> | **PULSE** | Finger-clip probe | MAX30102 standalone | 1550 nm NIRS + INA122 → SAADC |
>
> Auto-detected at boot by `product_hw_detect()` (MAX32664 Variant C → WATCH /
> wearable profile; else PULSE).

## ✓ Implemented Sensors

### 1. MAX32664 Sensor Hub (All 4 Variants - Modular Architecture)
- **Status**: ✓ **IMPLEMENTED** - Recently refactored into variant-specific modular architecture
- **Location**: `drivers/sensor/max32664/`
- **Architecture**: Function pointer dispatch with variant-specific files
- **Files**: 
  - `max32664.c` (1959 lines) - Common core: I2C, detection, FIFO, dispatch
  - `max32664_priv.h` - Private API with `max32664_variant_ops` structure
  - `max32664a.c` (110 lines) - Variant A: MaximFast (HR+SpO2, MAX30101, AGC)
  - `max32664b.c` (120 lines) - Variant B: WHRM (HR, MAX86141, KX-122 accel)
  - `max32664c.c` (252 lines) - Variant C: WHRM+WSpO2 (HR+SpO2, MAX86141, KX-122, FIFO parsing)
  - `max32664d.c` (130 lines) - Variant D: BPT (Blood Pressure, MAX30101, calibration)
- **Naming**: Primary: Variant A/B/C/D | Hardware aliases: GWEC (Variant C), GWED (Variant D)
- **Device Tree**: 
  - Compatible strings: `maxim,max32664-gwec` (Variant C) @ 0x55
  - Compatible strings: `maxim,max32664-gwed` (Variant D) @ 0x55
  - Runtime variant detection via AFE WHOAMI register
- **Configuration**: `CONFIG_MAX32664=y` in `prj.conf`
- **Key Features**:
  - Automatic variant detection (A/B/C/D) via AFE WHOAMI + firmware version
  - **Active operating mode: RAW** (`CONFIG_MAX32664_MODE_RAW=y`) — hub outputs raw sensor FIFO, HR/SpO2 algorithms run on nRF52840
  - Variant-specific initialization: ops->init() dispatches to appropriate variant file
  - AGC configuration for Variants A/D (Automatic Gain Control)
  - WHRM mode selection for Variants B/C (motion artifact rejection)
  - Accelerometer validation (MANDATORY for Variants B/C)
  - BPT calibration tracking (MANDATORY for Variant D)
- **Reference**: See `docs/sensors/MAX32664_VARIANT_ARCHITECTURE.md` for complete architecture details

### 2. Glucose Sensor (Non-Invasive NIRS)
- **Status**: ✓ **IMPLEMENTED**
- **Location**: `drivers/sensor/glucose/` (custom `glucose_sensor_driver_api`, not Zephyr `sensor_driver_api`)
- **Hardware path**: **SMC1550 (1550 nm) + SMC1050 (1050 nm) in parallel** (both sides of PD, P0.24 → BC847/SI2333) → G11193-03 → OPA358 TIA (**Rf=10 MΩ**, **Cf≈0.8 pF**) → INA122UA (**RG shorted to GND**, REF=GND) → SAADC AIN1 (P0.03); 80 raw ADC samples @ **2 Hz** after 2 s LED settle (single drive → mixed λ, not sequenced dual-λ)
- **AFE reference**: [`GLUCOSE_ANALOG_FRONTEND.md`](GLUCOSE_ANALOG_FRONTEND.md) (schematic-confirmed passives, timing, data path)
- **AFE review**: [`GLUCOSE_AFE_REVIEW.md`](GLUCOSE_AFE_REVIEW.md) (correctness/performance findings, 2026-08-02)
- **Files**:
  - `glucose_sensor.c` - Sensor driver (LED control, ADC sampling, state machine, dark-offset calibration)
  - `glucose_algorithm.c` - Svasth 6-stage `glucose_intercept()` regression + HOMA-IR (`insulin_resistance_calculate()`)
  - `glucose_algorithm_profiles.c` - Hardware profiles: WEARABLE (11 matrix groups) / PULSE (10 matrix groups)
  - `glucose_algorithm_matrices.c` - Embedded elimination matrices (fallback if XIP model unused)
  - `glucose_model_xip` / related — packed XIP model on `glucose-model-primary` / secondary
  - `product_hw_detect.c` - Product SKU (Watch vs Pulse); glucose profile maps 1:1
  - `Kconfig` - Configuration options
- **Application**: `src/sensors/glucose.c` (facade), `src/sensors/glucose_logger.c` (NOR `record_store` GLUCOSE + GLUCOSE_RAW), `src/ui/glucose_ui.c` (long-press card UX), `src/sensors/health_sched.c` (auto cycle), `src/ble/ble_gatt.c` (result f105, per-sample stream f107, algorithm details f108)
- **Algorithm input**: 80 raw ADC samples only — PPG features are **not** an input; PPG runs sequentially before glucose in the health cycle to avoid optical crosstalk
- **Features**: Wearable/pulse profile auto-selection, XIP packed matrices (production default), dark-current offset calibration (voltage path only today), optional filtering + signal quality metrics, HOMA-IR secondary metrics, record_store logging, BLE notify / `…def4` sync, MQTT publish (optional)

### 3. MAX3010x PPG Sensor (MAX30102, Standalone)
- **Status**: ✓ **IMPLEMENTED** (561 lines, full Zephyr sensor API)
- **Location**: `drivers/sensor/max3010x/`
- **Files**:
  - `max3010x.c` (561 lines) - Complete driver with DEVICE_DT_INST_DEFINE
  - `Kconfig` - CONFIG_MAX3010X_PPG option
- **Fitted hardware**: MAX30102 @ 0x57 on I2C0
- **Features**:
  - 3-LED configuration (Red, IR, Green) with 0-51mA current range
  - 32-sample FIFO with rollover support
  - 8192nA ADC range, configurable sample rate
  - Interrupt support (A_FULL, DATA_RDY)
  - Part ID verification (0x15) at init
- **API**: Standard Zephyr sensor API (`sensor_sample_fetch()`, `sensor_channel_get()`)
- **Channels**: `SENSOR_CHAN_RED`, `SENSOR_CHAN_IR`, `SENSOR_CHAN_GREEN`, `SENSOR_CHAN_HEART_RATE`, `SENSOR_CHAN_SPO2`
- **Configuration**: `CONFIG_MAX3010X_PPG=y` (disabled by default in prj.conf; MAX32664 hub is primary)
- **Note**: Standalone fallback path if MAX32664 hub is not present. On **WEARABLE** builds, boot log `not present (-5)` is **expected** — see [BOOT_LOG_GUIDE.md](../debugging/BOOT_LOG_GUIDE.md).

### 4. MAX302xx Temperature Sensor (Unified Driver)
- **Status**: ✓ **IMPLEMENTED**
- **Location**: `drivers/sensor/max302xx/`
- **Files**: `max302xx.c` - Unified driver with auto-detection
- **Supported Variants** (auto-detected via Part ID register 0xFF):
  - MAX30205 (fallback) - 13-bit resolution, continuous conversion
  - MAX30208 (0x08) - 16-bit resolution, 32-sample FIFO, GPIO interrupt
  - MAX30210 (0x45) - 16-bit resolution, 64-sample FIFO, slope detection
- **I2C Addresses**: 0x50-0x53 (address latched at POR from GPIO pins)
- **Current config**: MAX30205 @ 0x48 (pulse), MAX30208 @ 0x53 (wearable)
- **Configuration**: `CONFIG_MAX302XX=y` in `prj.conf`

### 5. VCNL3040 Proximity/ALS Sensor
- **Status**: ✓ **IMPLEMENTED** (572 lines)
- **Location**: `drivers/sensor/vcnl3040/`
- **Files**: `vcnl3040.c` (572 lines) - Full driver with wear detection
- **I2C Address**: 0x60
- **Features**:
  - Active Force (one-shot) mode for low power
  - Dark offset calibration at init (16-sample average)
  - IIR low-pass filter (╬▒=0.25, integer math)
  - Multi-pulse averaging (4 pulses default)
  - Sunlight cancellation, 1/40 duty cycle
- **Wear State Classification** (see `vcnl3040.c`; hardware-tuned):
  - NOT_WORN: proximity < 500
  - WORN_LOOSE: 500 – &lt; 30000
  - WORN_GOOD: proximity ≥ **30000** (skin typically ~30k–40k; table/air stay below)
- **Configuration**: `CONFIG_VCNL3040=y` in `prj.conf`

### 6. PPG Algorithm Subsystem (On-Chip HR/SpO2)
- **Status**: ✓ **IMPLEMENTED** (updated 2026-08-05 — staged live)
- **Location**: `subsys/ppg_algo/` (Zephyr out-of-tree subsystem)
- **Files**:
  - `ppg_algo.c` - Main coordinator (source probe, workqueues, staged live, finalize/SHDN)
  - `ppg_preprocessing.c` - DC removal (MA ≥32), ambient subtract
  - `ppg_peak_detect.c` - Local-maxima + refractory peaks
  - `ppg_hr_calc.c` - Heart rate (R-R intervals)
  - `ppg_spo2_calc.c` - SpO2 (window AC R-value)
  - `ppg_quality.c` - Signal quality (SNR, PI, motion)
  - `ppg_samples.c` - Sample buffer (sized to **record** window)
  - `ppg_triggers.c` - Trigger handling
  - `ppg_shell.c` - Shell commands (optional, `CONFIG_PPG_ALGO_SHELL=n` by default)
  - `ppg_hb_calc.c` / `ppg_resp_calc.c` / `ppg_hrv_bp_calc.c` - Hb / Resp / HRV·BP (product enables all; **RESP requires buffer ≥20 s**)
  - `README.md` - Subsystem overview
- **Purpose**: Software HR/SpO2 (+ experimental Hb/BP/HRV) for RAW mode on nRF52840
- **PPG sources**: Watch MAX86141 via MAX32664 RAW (SFH7074); Pulse MAX3010x secondary
- **Staged live**: partial vitals @ ~300, vascular by record window, NOR @ 500, **live tail 0** (SFH off at required count)
- **Configuration**: see `conf/features/ppg.conf` — `STAGED_LIVE`, `MOTION_REJECTION=y`, HB/BP/HRV/RESP=y
- **Buffer / acquisition**: **RESP-gated** — **20 s / 500 @ 25 Hz**. Full why/how + algo math: [PPG_ALGO_PARAMS_AND_SCHEDULING.md](../architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md)
- **Integration**: `src/sensors/ppg.c` feeds RAW FIFO to ppg_algo; UI agent auto-navs Vital → Vascular → Metabolic
- **Accuracy**: Research/wellness grade (~±3-5% SpO2, ±5 BPM HR) — not clinical claims
- **Reference**: `subsys/ppg_algo/README.md`

### 7. MCP79xx RTC (Out-of-Tree RTC Driver)
- **Status**: ✓ **IMPLEMENTED**
- **Location**: `drivers/counter/mcp79xx.c` (Zephyr RTC API only)
- **Supported Chips**: MCP7940M/N register map (product: MCP7940N)
- **Features**:
  - RTC API: calendar set/get, optional alarms + calibration
  - Year encoding fix (offset 100 per datasheet)
  - 24h mode; errata #3/#4 workarounds on `set_time`
  - Calibration: ±127 ppm, Zephyr polarity (positive = speed up)
  - Periodic HW alarm off by default (`CONFIG_APP_RTC_PERIODIC_ALARM=n`)
- **I2C Address**: 0x6F
- **Configuration**: `CONFIG_COUNTER_MCP79XX=y`, `CONFIG_RTC=y`
- **Device Tree**: `compatible = "microchip,mcp79xx"`
- **Docs**: `drivers/counter/README.md` (DS20005010J, Track B deferred alarms)

## Hub Child Devices (Accessed Via MAX32664)

### 1. MAX86141 PPG AFE
- **Status**: ✓ **IMPLEMENTED as hub child sensor device** (`drivers/sensor/max32664/max86141_ppg.c`)
- **Note**: Exposed as Zephyr sensor device `max86141_ppg` (DT `maxim,max86141-ppg`, child of the hub node). In RAW mode, `max86141_ppg` sample fetch pops the hub output FIFO; the nRF52840 still never talks to the AFE directly over I2C
- **Used by**: `subsys/ppg_algo` as the primary PPG source (`ncs,ppg-primary`)

### 2. LIS2DS12 Accelerometer
- **Status**: ✓ **IMPLEMENTED as hub child sensor device** (`drivers/sensor/max32664/lis2ds12_accel.c`, hub passthrough)
- **Note**: Read by `ppg_algo` for per-sample motion pairing; also fed back to the hub input FIFO by `max32664_accel_feeder.c` (host-fed accel @ 25 Hz) for hub-side motion rejection

## Known Gaps / Incomplete Items (Verified 2026-07-21)

### Product backlog (major)

| Gap | Evidence |
|-----|----------|
| **Variant B FIFO parsing is a stub** (returns `-ENOTSUP`); runtime hub mode switching (`max32664_set_mode`) unimplemented — no SKU today | `drivers/sensor/max32664/max32664b.c`, `max32664.c` |

### PPG
| Gap | Evidence |
|-----|----------|
| **Hemoglobin / BP / HRV** — enabled in wearable `ppg.conf` but experimental; coeffs need clinical tuning; HRV ≥20 RR is borderline at 20 s | `conf/features/ppg.conf`, `ppg_hb_calc.c`, `ppg_hrv_bp_calc.c`, [PPG_ALGO_PARAMS_AND_SCHEDULING.md](../architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md) |
| **Factory PPG LED currents / R-ratio** — SpO2/HR linear trim **is applied**; LED current + polynomial not applied (v1 skip) | `calibration_apply_ppg_hr_spo2()`, [FACTORY_CALIBRATION.md](../hardware/FACTORY_CALIBRATION.md) |
| **Blood pressure (hub BPT, Variant D)** — driver channels defined; product BP is **host empiric**, not hub BPT | `include/zephyr/drivers/sensor/max32664.h`, `ppg_hrv_bp_calc.c` |

### Glucose
| Gap | Evidence |
|-----|----------|
| **Factory glucose matrix replace skipped** — clamp applied; `calibration_factors[]` does not replace XIP matrices | `calibration_apply_glucose_mg_dl()`, driver uses `NAN` skin temp (temp-comp inactive until wired) |
| **Gain calibration partially implemented** — calibrating against a known standard just resets gain to 1.0 | `drivers/sensor/glucose/glucose_sensor.c` |
| **Pulse profile regression coefficients are interim** | `glucose_algorithm_profiles.c` |
| **Fasting insulin defaults to 10.0 μIU/mL** with no BLE/GATT provisioning path (HOMA-IR only) | `glucose_algorithm_config.h` |
| **Matrix lookup miss degrades silently** | `glucose_algorithm.c` |

### Closed recently (2026-07-21)

| Item | Notes |
|------|-------|
| Factory PPG/temp/glucose apply (safe defaults) | Erased partition → no-op |
| MAX20360 ship mode | `PwrCmd` SEAL `0xE5` (`CONFIG_REGULATOR_MAX20360_SHIP_USE_SEAL`) |
| Glucose syscall verifiers | `get_sample_data`, fasting insulin, `get_config`, `trigger_set` — `CONFIG_USERSPACE` still off |

## Documentation Status

### Active Documentation
- ✓ `docs/README.md` - Current project docs index
- ✓ `docs/sensors/SENSOR_STATUS.md` - This file
- ✓ `docs/debugging/RTT_DEBUG.md` - RTT setup (current)
- ✓ `docs/debugging/BOOT_LOG_GUIDE.md` - Boot warning/error interpretation
- ✓ `docs/ui/UI_GUIDE.md` - LVGL screens, long-press measure, lifecycle debug
- ✓ `docs/hardware/FACTORY_CALIBRATION.md` - Calibration partition
- ✓ `docs/debugging/MEMORY_TUNING.md` - RAM/LVGL pool budget
- ✓ `docs/architecture/STORAGE_NOR_RECORD_STORE.md` - NOR persistence (current)
- ✓ `docs/sensors/MAX32664_VARIANT_ARCHITECTURE.md` - Hub variant documentation (current)
- ✓ `docs/sensors/WIFI_INTEGRATION.md` - Wi-Fi driver documentation

### Status Notes
- `docs/sensors/MULTI-SENSOR-GUIDE.md` - Planning guide for multi-sensor configurations

## Current Project Focus

Based on actual implementation:
1. ✓ **MAX20360 PMIC** - Complete (regulator, charger, fuel gauge, PMIC ADC); `conf/core/pmic.conf` enables the MAX20360 charger and fuel gauge drivers
2. ✓ **MAX32664 Hubs** - Complete (all 4 variants with modular architecture)
3. ✓ **Glucose Sensor** - Complete (driver + algorithm + logging)
4. ✓ **MAX30101** - Complete (561 lines, full Zephyr sensor API)
5. ✓ **MAX302xx Temperature** - Complete (unified driver, auto-detection)
6. ✓ **VCNL3040 Proximity** - Complete (572 lines, wear detection)
7. ✓ **PPG Algorithm** - Complete (subsystem at subsys/ppg_algo/)
8. ✓ **MCP79xx RTC** - Complete (RTC API, calibration Zephyr-aligned, errata #3/#4)
9. ✓ **WExx Wi-Fi** - Complete (AT command driver)
10. ✓ **Display/UI** - LVGL carousel, long-press measure on Glucose/Vitals, lifecycle debug (`docs/ui/UI_GUIDE.md`)

## Sensor Summary Table

| Sensor | Status | Lines | I2C Addr | Config Option |
|--------|--------|-------|----------|---------------|
| MAX32664 Hub | ✓ Complete | 2571 | 0x55 (I2C1) | `CONFIG_MAX32664=y` |
| Glucose | ✓ Complete | ~800 | ADC (P0.03) | `CONFIG_GLUCOSE_SENSOR=y` |
| MAX3010x PPG | ✓ Complete | 561 | 0x57 (I2C0) | `CONFIG_MAX3010X_PPG=y` |
| MAX302xx Temp | ✓ Complete | ~400 | 0x48/0x53 (I2C0) | `CONFIG_MAX302XX=y` |
| VCNL3040 Prox | ✓ Complete | 572 | 0x60 (I2C0) | `CONFIG_VCNL3040=y` |
| PPG Algorithm | ✓ Complete | ~800 | N/A | `CONFIG_PPG_ALGO=y` |
| MCP79xx RTC | ✓ Complete | ~600 | 0x6F (I2C0) | `CONFIG_COUNTER_MCP79XX=y` |
| WExx Wi-Fi | ✓ Complete | ~1000 | UART0 | `CONFIG_WIFI_WEXX=y` |
| MAX86141 | ✓ Via Hub (child dev) | ~300 | Hub child | `CONFIG_MAX32664_RAW_PPG_SENSOR=y` |
| LIS2DS12 | ✓ Via Hub (child dev) | ~300 | Hub child | `CONFIG_MAX32664=y` |

## Product backlog (summary)

1. **MAX32664 Variant B FIFO / `set_mode`** — only major driver stub with no current SKU  
2. Optional follow-ups: factory LED-current apply, glucose skin-temp into `calibration_apply_glucose_mg_dl`, Variant D BPT UI  

Fuel-gauge `set_property` remains read-only by design.

---

**For AI Agents**: Check this file first to understand what's actually implemented vs. what's documented for future planning.


