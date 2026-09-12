# Architecture Overview — NiSense Dual-Mode Health Monitor

**Purpose**: System architecture reference (firmware layers, power policy, UI model, NOR persistence)  
**Last Updated**: 2026-08-05

> **Dual-mode device**: the same nRF52840 firmware supports two hardware profiles.
> *WEARABLE* (wrist) uses the MAX32664 hub + MAX86141 for HR/SpO2/Hb/Resp.
> *PULSE* (finger-clip) uses the MAX30102 + 1550 nm NIRS circuit for HR/SpO2/glucose/HOMA-IR.
> Product SKU is auto-detected at runtime by `product_hw_detect()` (MAX32664
> Variant C present → WATCH / wearable glucose; else PULSE).

**Related docs**

| Topic | Path |
|-------|------|
| Build / flash / USB·shell features | `docs/build/BUILD_FLASH_DEPLOY.md` |
| Mermaid diagrams (sequences, layers) | `docs/diagrams/` (FatFS diagrams are historical) |
| UI carousel + single-screen policy | `docs/ui/UI_GUIDE.md` |
| PMIC / fuel gauge driver | `docs/drivers/MAX20360.md` |
| Hardware pins | `docs/hardware/HARDWARE_REFERENCE.md` |
| Partition map | `docs/build/PARTITION_LAYOUT.md` |
| **NOR record store (current)** | `docs/architecture/STORAGE_NOR_RECORD_STORE.md` |
| **PPG windows, staged live, RESP, BLE schedule** | `docs/architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md` |
| **UI staged nav / battery strip** | `docs/ui/UI_GUIDE.md` |
| Glucose logging / export ops | `docs/sensors/GLUCOSE_DATA_LOGGING.md` |
| Health measurement cycle | `docs/diagrams/04-interactions/health_cycle_sequence.mmd` |
| Dual-mode HW detect | `docs/diagrams/01-architecture/dual_mode_hw_detect.mmd` |

---

## Application layers (current)

```
+-----------------------------------------------------------------------------+
|  main.c          Entry, init order, main loop (BLE tick, power_mgr_tick)    |
+-----------------------------------------------------------------------------+
|  UI (LVGL)       ui.c — single active carousel screen (create on visit,     |
|                  destroy on leave). ui_shell.c status strip per screen.     |
|                  Transient top-layer: ui_agent (results), ui_hold_overlay   |
|                  (button hold countdown).                                   |
+-----------------------------------------------------------------------------+
|  Input           SW1 P0.29 nav_button.c — short: next screen;               |
|                  long: measure hold → health_sched manual cycle.            |
|                  PFN1 P1.04 power_button.c — short: wake/home;              |
|                  long: power-off countdown. ui_buttons.c wires both.        |
+-----------------------------------------------------------------------------+
|  Power policy    power_mgr.c — owns fuel-gauge poll cache, display idle     |
|                  sleep, critical-SOC policy, PFN2 battery-low events.       |
|                  power_batt.c — snapshot API / I2C throttle.                |
|                  battery_low.c — PMIC PFN2 → P1.15 IRQ.                     |
|                  power.c — rail bring-up (legacy PMIC helpers).             |
+-----------------------------------------------------------------------------+
|  Health          health_sched.c — wear-gated auto/manual measurement        |
|                  cycles; SOC clamps; defers to ppg/glucose/temp.            |
|                  Loggers append to NOR record_store during the cycle.       |
+-----------------------------------------------------------------------------+
|  Persistence     record_store.c — append-only typed summaries + raw chunks  |
|                  on QSPI `record_store`. Glucose models: XIP packed slots.  |
|                  Export: BLE …def4 sync and/or USB CDC `nisense rec …`.     |
+-----------------------------------------------------------------------------+
|  Connectivity    ble_gatt.c / ble.c — vitals stream, PMIC notify, config    |
|                  (screen timeout, poll interval, critical SOC, holds).      |
|                  ble_record_sync.c — bulk NOR pull (service …def4).         |
|                  config_manager.c — NVS / Zephyr settings (not FAT JSON).   |
|                  diag_shell.c — CDC ACM shell when APP_FEATURE_SHELL=y     |
|                  (selects APP_FEATURE_USB; SHELL_MINIMAL for FLASH).       |
+-----------------------------------------------------------------------------+
|  Agents          ui_agent.c — post-cycle results slideshow (top layer).     |
+-----------------------------------------------------------------------------+
|  Zephyr + OOT    Kernel, I2C/SPI/ADC, out-of-tree drivers under drivers/    |
+-----------------------------------------------------------------------------+
```

### Single-screen-alive rule

Only **one** carousel screen object tree exists in the LVGL heap at a time. `switch_to_screen()` in `ui.c`:

1. Cancels transient overlays (`ui_hold_overlay`, `ui_agent` slideshow)
2. Runs screen-specific `*_cleanup_screen()` and `ui_shell_detach()` (frees status-strip heap)
3. Deletes the outgoing screen root, then creates the target screen

Top-layer overlays are **build → destroy** (not hidden persistently). See `docs/ui/UI_GUIDE.md`.

### Power manager defaults (380 mAh KM20 Li-ion)

| Setting | Default | Owner |
|---------|---------|-------|
| Fuel gauge poll | 10 s | `power_mgr` |
| Display idle timeout | 60 s | `power_mgr` + `config_manager` |
| Critical SOC (no auto health) | 5% | `power_mgr` + `health_sched` |
| Low SOC warning | 15% | `power_mgr` |
| Resume while charging | 20% | `power_mgr` |
| PMIC PFN2 VBAT threshold | 3400 mV | `battery_low` + BLE |

Display backlight is **GPIO on/off** (P0.04 regulator); no PWM dimming.

---

## System Architecture Overview (hardware)

```
+-----------------------------------------------------------------------------+
+                        nRF52840 (256KB RAM, 1MB Flash)                      +
+-----------------------------------------------------------------------------+
+                                                                             +
+  +--------------+    +--------------+    +--------------+                   +
+  +   main.c     +    +    ui.c      +    + power_mgr.c  +                   +
+  +  Entry Point +--->+ LVGL Screens +    + power_batt   +                   +
+  + Fatal Handler+    + Touch/Buttons|    + battery_low  +                   +
+  +--------------+    +--------------+    +--------------+                   +
+         +                   +                   +                           +
+         v                   v                   v                           +
+  +---------------------------------------------------------------------+    +
+  +                    Zephyr RTOS Kernel (v4.3.99)                     +    +
+  +  +---------+  +---------+  +---------+  +---------+  +---------+    +    +
+  +  + I2C Bus +  +   SPI   +  +   ADC   +  +   BLE   +  + USB CDC +    +    +
+  +  + Driver  +  + Driver  +  + Driver  +  + Stack   +  +  (shell) +    +    +
+  +  +----+----+  +----+----+  +----+----+  +----+----+  +----+----+    +    +
+  +-------+-----------+-----------+-----------+-----------+-------------+    +
+          +           +           +           +           +                  +
+----------+-----------+-----------+-----------+-----------+------------------+
           +           +           +           +           +
           v           v           v           v           v
+------------------------------------------------------------------------------+
+                              HARDWARE LAYER                                  +
+------------------------------------------------------------------------------+
+                                                                              +
+  I2C0 (P0.13/14)                    SPI2 (P1.8/P0.26)     QSPI (8MB)         +
+  +-------------+                    +-------------+       +-------------+    +
+  + MAX20360    + PMIC @ 0x28        +  ST7789V    +       +  MX25R64    +    +
+  + MAX20360-FG + FuelGauge @ 0x36   +  Display    +       +  Ext Flash  +    +
+  + MCP7940N   + RTC @ 0x6F          +  240x240    +       +  XIP+recs   +    +
+  + FT6336     + Touch @ 0x38        +-------------+       +-------------+    +
+  + MAX30205   + Temp @ 0x48                                                  +
+  + MAX30208   + Temp @ 0x53                                                  +
+  + MAX30102   + PPG @ 0x57 (fallback)                                        +
+  + VCNL3040   + Prox @ 0x60         UART0 (P0.06/08)                         +
+  +-------------+                    +-------------+                          +
+                                     + WExx Wi-Fi  +                          +
+  I2C1 (P0.16/15)                    + 38400 baud  +       GPIO               +
+  +-------------+                    +-------------+       +-------------+    +
+  + MAX32664    + Hub @ 0x55         ADC (P0.03)           + IR LED P0.24+    +
+  +  MAX86141   + PPG child          +-------------+       + Buzzer P0.12+    +
+  +  LIS2DS12   + Accel child        + INA122UA    +       +-------------+    +
+  +-------------+                    + Glucose Amp +                          +
+                                     +-------------+                          +
+                                                                              +
+------------------------------------------------------------------------------+
```

---

## MAX20360 PMIC (Power Management IC)

```
+----------------------------------------------------------------+
+                    MAX20360 PMIC @ I2C 0x28                    +
+----------------------------------------------------------------+
+                                                                +
+  +--------------+   +--------------+   +--------------+        +
+  +    Buck1     +   +    Buck2     +   +  Buck-Boost  +        +
+  +  0.55-1.18V  +   +  0.55-2.125V +   +  2.5-5.0V    +        +
+  +   10mV step  +   +   25mV step  +   +   50mV step  +        +
+  +  → 1.2V Core +   +  → 1.8V I/O  +   +  → 5V Sensor +        +
+  +--------------+   +--------------+   +--------------+        +
+                                                                +
+  +--------------+   +--------------+   +--------------+        +
+  +   Charger    +   +  Fuel Gauge  +   +   L2OUT LDO  +        +
+  + Li-Ion 4.2V  +   +  SOC Est.    +   +  0.8-3.3V    +        +
+  +  500mA max   +   +  @ 0x36      +   +  → 3V Buzzer +        +
+  +--------------+   +--------------+   +--------------+        +
+                                                                +
+  CRITICAL: Write-unlock required before register writes!       +
+  Sequence: LockMsk(0x83) → Password 0x55 to LockUnlock(0x84)   +
+  Init order: CFG0 → CFG1 → ENA → VSET                          +
+----------------------------------------------------------------+

Files: drivers/regulator/max20360/, drivers/charger/max20360/,
       drivers/fuel_gauge/max20360/, src/power/power.c, src/power/power_batt.c,
       src/power/power_mgr.c, src/battery_low.c
```

---

## MAX32664 Sensor Hub (3 Operating Modes + 4 Variants)

```
+------------------------------------------------------------------+
+         MAX32664 Sensor Hub @ I2C 0x55 - Operating Modes         +
+------------------------------------------------------------------+
+                                                                  +
+  +--- RAW Mode (Active) --------+  +--- HUB Mode --------------+ +
+  + Hub outputs raw FIFO data    +  + Use hub's internal algos  + +
+  + nRF52840 runs HR/SpO2 algos  +  + WHRM/MaximFast/BPT algos  + +
+  + MAX86141 + LIS2DS12 FIFO     +  + Hub processes all data    + +
+  + On-chip ppg_algo subsystem   +  + CONFIG_MAX32664_MODE_     + +
+  + CONFIG_MAX32664_MODE_        +  +   HUB=y                   + +
+  +   RAW=y ✓ ACTIVE            +  +---------------------------+ +
+  +-----------------------------+                                 +
+                                                                  +
+  +--- PASSTHROUGH Mode --------+                                 +
+  + Hub acts as I2C bridge       +  Current Mode: RAW             +
+  + Direct sensor register access+  Defined in: conf/features/    +
+  + CONFIG_MAX32664_MODE_        +    max32664.conf               +
+  +   PASSTHROUGH=y             +                                 +
+  +-----------------------------+                                 +
+                                                                  +
+  Hardware Variants (auto-detected via firmware version):         +
+  • A: MaximFast (MAX30101, HR+SpO2, 100Hz)                       +
+  • B: WHRM (MAX86141+KX-122, HR, 25Hz)                           +
+  • C: WHRM+WSpO2 (MAX86141+KX-122, HR+SpO2, 25Hz) ← GWEC         +
+  • D: BPT (MAX30101, Blood Pressure, calibration required)       +
+                                                                  +
+  ⚠ Mode selection is HARDWARE DEPENDENT - verify AFE/accel      +
+  before switching modes in prj.conf                              +
+------------------------------------------------------------------+

Files: drivers/sensor/max32664/max32664.c (core driver)
       drivers/sensor/max32664/max32664_passthrough.c (PASSTHROUGH mode)
       drivers/sensor/max32664/max32664_raw.c (RAW mode coordinator)
       drivers/sensor/max32664/max32664a/b/c/d.c (HUB mode variants)
       drivers/sensor/max32664/max32664_priv.h (private API)
```

---

## On-Chip PPG Algorithm (PASSTHROUGH/RAW Mode)

```
+----------------------------------------------------------------------------+
+              On-Chip PPG Algorithm (nRF52840) — RAW product path            +
+----------------------------------------------------------------------------+
+                                                                            +
+  Purpose: Host DSP for HR/SpO2/Hb/Resp/HRV/BP from MAX86141 (SFH7074)      +
+  Status: ✓ ENABLED (CONFIG_MAX32664_MODE_RAW=y + CONFIG_PPG_ALGO=y)        +
+                                                                            +
+  Signal pipeline:                                                          +
+  1. DC moving-average (≥32 samples) + ambient subtract                     +
+  2. Local-maxima peaks (green AC on watch; IR on pulse MAX3010x)           +
+  3. HR from filtered R–R; SpO2 from window Red/IR AC R-value               +
+  4. Quality gate (PI, SNR, motion, confidence)                             +
+  5. Optional Hb / Resp / HRV / empiric BP                                  +
+                                                                            +
+  Staged live (product):                                                    +
+  • ~300 samples → PARTIAL_VITALS (Vital UI)                                +
+  • ≤500 → PARTIAL_VASCULAR (Vascular UI)                                   +
+  • 500 → RECORD_READY (NOR write) + SHDN SFH (LIVE_TAIL=0)                 +
+  • MEASUREMENT_COMPLETE → glucose / Metabolic / slideshow                  +
+                                                                            +
+  Configuration (see PPG_ALGO_PARAMS_AND_SCHEDULING.md):                     +
+  • BUFFER / APP_HEALTH_PPG_SECONDS: RESP=y → 20 s / 500 @ 25 Hz            +
+  • STAGED_LIVE=y, PARTIAL_VITALS=300, LIVE_TAIL=0                          +
+  • MOTION_REJECTION=y, HB/BP/HRV/RESP=y, QUALITY_THRESHOLD=50              +
+  • BLE f01b overrides sample count + schedule; NVS persist                 +
+                                                                            +
+  ⚠ Wellness-grade only — not FDA/CE clinical claims                        +
+----------------------------------------------------------------------------+

Files: subsys/ppg_algo/ppg_algo.c (coordinator + staged live)
       subsys/ppg_algo/ppg_preprocessing.c / ppg_peak_detect.c
       subsys/ppg_algo/ppg_hr_calc.c / ppg_spo2_calc.c / ppg_quality.c
       subsys/ppg_algo/ppg_hb_calc.c / ppg_resp_calc.c / ppg_hrv_bp_calc.c
       src/sensors/ppg.c / src/ui/ui_agent.c
       Docs: architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md
             architecture/PPG_ALGO_DESIGN_REFERENCES.md
             subsys/ppg_algo/README.md
```

---

## NOR record store, BLE sync, and CDC shell

FatFS and USB MSC are **removed**. Measurement persistence and matrix loading use
QSPI NOR directly:

```
+-----------------------------------------------------------------+
+         record_store + BLE …def4 + USB CDC shell                +
+-----------------------------------------------------------------+
+                                                                 +
+  Device path (firmware logs)                                    +
+  • ppg_logger / glucose_logger / temp append typed slots        +
+  • Summary (GLUCOSE/VITALS/TEMP) + raw chunks (PPG_RAW/…)       +
+  • Join via record_id / parent_id (not timestamp)               +
+                                                                 +
+  Host export                                                    +
+  • BLE service …def4: f401 Control, f402 Data, f403 Status      +
+  • USB CDC shell: `nisense rec stats|summary|get|export|ack…`   +
+    (APP_FEATURE_SHELL → USB; logs stay on RTT; often off in     +
+     production for FLASH headroom)                              +
+                                                                 +
+  Models                                                         +
+  • Clinical model pack in model-* A/B slots (wearable+pulse)     +
+  • Flash via scripts/flash/flash_glucose_model.ps1 -Variant pack +
+    or BLE …def3                                                  +
+                                                                 +
+  Shared QSPI caveat                                             +
+  • Same MX25R64 holds Resource store, models, and record_store      +
+  • main skips ui_update while nor_qspi_busy / asleep            +
+-----------------------------------------------------------------+

Files: `src/storage/record_store.c`, `include/record_store.h`,
       `src/ble/ble_record_sync.c`, `src/diag/diag_shell.c`,
       callers: `health_sched.c`, `*_logger.c`

Detail: **`docs/architecture/STORAGE_NOR_RECORD_STORE.md`**.  
Ops / CSV columns: `docs/sensors/GLUCOSE_DATA_LOGGING.md`.

---

## Wi-Fi Module (WExx)

```
+----------------------------------------------------------------+
+              Wi-Fi Module WE10/WE20D (UART AT Commands)        +
+----------------------------------------------------------------+
+                                                                +
+  Purpose: 2.4GHz Wi-Fi connectivity via UART AT commands       +
+  Status: Active AT-command integration (data-stack limited)    +
+                                                                +
+  +---------------------------------------------------------+   +
+  + Hardware Configuration:                                 +   +
+  + • UART0: P0.06 (TX) → WExx RXD, P0.08 (RX) ← WExx TXD   +   +
+  + • CHIP_EN: P0.05 (GPIO output, active-high)             +   +
+  + • Baud Rate: 38400 (configurable)                       +   +
+  + • No hardware flow control (RTS/CTS optional)           +   +
+  +---------------------------------------------------------+   +
+                                                                +
+  What Works:                                                   +
+  • AT command interface (scan, connect, disconnect)            +
+  • Basic wifi_mgmt integration                                 +
+  • Configuration storage in NVS (config_manager)               +
+                                                                +
+  Not Currently Included:                                       +
+  • HTTP client stack integration                               +
+  • MQTT client stack integration                               +
+  • Full Zephyr network manager integration                     +
+                                                                +
+  AT Command Examples:                                          +
+  • CMD+WIFIMODE,1              (Station mode)                  +
+  • CMD+CONTOAP,ssid,pwd,sec    (Connect to AP)                 +
+  • CMD+DISCONN                 (Disconnect)                    +
+  • CMD+SCANAP                  (Scan networks)                 +
+  • CMD?VERSION                 (Query version)                 +
+                                                                +
+  Memory Footprint:                                             +
+  • Flash: ~25-30KB (driver + AT parser)                        +
+  • RAM: ~5-8KB (RX/TX buffers + state)                         +
+----------------------------------------------------------------+

Files: drivers/wifi/wexx/wexx.c, boards/*.overlay (UART0 + GPIO config)
```

---

## Glucose Sensor System

Canonical AFE / passives / DT details:
[`docs/sensors/GLUCOSE_ANALOG_FRONTEND.md`](../sensors/GLUCOSE_ANALOG_FRONTEND.md).

```
+-----------------------------------------------------------------------+
+                    Glucose Sensor Signal Path                         +
+-----------------------------------------------------------------------+
+                                                                       +
+  +------------+    +------------+    +-------------+    +----------+  +
+  + IR LEDs    +--->+ BC847      +--->+ G11193-03   +--->+ OPA358   +  +
+  + SMC1550    +    + + SI2333   +    + Photodiode  +    + TIA      +  +
+  + 1550nm +   +    + MOSFET     +    + G11193_OUT  +    + Rf=10MΩ  +  +
+  + SMC1050    +    +------------+    + (both λ)    +    + Cf≈0.8pF +  +
+  + 1050nm     +                                              +        +
+  + P0.24 gang +----------------------------------------------+        +
+                                                              v        +
+                                         +----------+    +----------+  +
+                                         + ADC AIN1 +----+ INA122UA +  +
+                                         + P0.03    +    + RG short +  +
+                                         + 12-bit   +    + REF=GND  +  +
+                                         +----------+    +----------+  +
+                                               +                       +
+                                               v                       +
+  +----------------------------------------------------------+         +
+  +                  Glucose Algorithm                       +         +
+  +  • 80 raw ADC samples @ sample-rate-hz=2 (DT)            +         +
+  +  • Quality metrics (SNR, stability) on voltage path      +         +
+  +  • Elimination correction (matrix groups)                +         +
+  +  • Matrices: XIP packed model (glucose-model-primary)    +         +
+  +----------------------------------------------------------+         +
+                                         +                             +
+                                         v                             +
+                            +------------------+                       +
+                            + glucose_logger.c +                       +
+                            + → record_store   +                       +
+                            +   GLUCOSE+RAW    +                       +
+                            +------------------+                       +
+-----------------------------------------------------------------------+

Files: drivers/sensor/glucose/glucose_sensor.c
       drivers/sensor/glucose/glucose_algorithm.c
       drivers/sensor/glucose/glucose_model_xip.c (XIP packed)
       src/sensors/glucose.c, src/sensors/glucose_logger.c, src/storage/record_store.c
```

---

## Flash Partition Layout

See **`docs/build/PARTITION_LAYOUT.md`** for the authoritative map and change
procedure. Summary (DTS-based, Partition Manager disabled):

### Internal Flash (1 MB)
| Offset | Size | Partition |
|--------|------|-----------|
| 0x00000 | 40 KB | MCUBoot bootloader |
| 0x0A000 | 968 KB | Slot0 (primary app) |
| 0xFC000 | 16 KB | Settings NVS (BLE bonds) |

### External QSPI Flash (8 MB — MX25R64)
| Offset | Size | Partition |
|--------|------|-----------|
| 0x000000 | 968 KB | Slot1 (OTA staging) |
| 0x0F2000 | 16 KB | MCUboot scratch |
| 0x0F6000 | 512 KB | Resource primary (UI) |
| 0x176000 | 512 KB | Resource secondary |
| 0x1F6000 | 512 KB | Model primary (clinical pack) |
| 0x276000 | 512 KB | Model secondary |
| 0x2F6000 | 64 KB | Calibration (RO) |
| 0x306000 | 128 KB | External NVS / config |
| 0x326000 | 4 MB | `record_store` |
| 0x726000 | ~872 KB | App (hub MSBL ~300 KB / blobs) |

Canonical detail: `docs/build/PARTITION_LAYOUT.md`.

---

## Other Sensors

### MAX30101 PPG (Standalone @ 0x57)
- 3 LEDs (Red, IR, Green), 32-sample FIFO
- Alternative to hub for direct PPG access
- Status: ✓ COMPLETE (561 lines)

### MAX302xx Temperature (@ 0x48, 0x53)
- Auto-detects MAX30205/MAX30208/MAX30210
- MAX30205 @ 0x48: Pulse/spot check
- MAX30208 @ 0x53: Wearable continuous
- Status: ✓ COMPLETE

### VCNL3040 Proximity (@ 0x60)
- Wear detection (NOT_WORN &lt; 500, WORN_GOOD ≥ **30000** on current HW)
- Status: ✓ COMPLETE (572 lines)

### MCP7940N RTC (@ 0x6F)
- Battery-backed real-time clock
- Uses Zephyr Counter API
- Status: ✓ COMPLETE

---

## MAX32664 Firmware Update System

```
+-----------------------------------------------------------------+
+              MAX32664 Firmware Update System                    +
+-----------------------------------------------------------------+
+                                                                 +
+  Purpose: Update MAX32664 sensor hub firmware via .msbl files   +
+                                                                 +
+  Bootloader Protocol:                                           +
+  1. Enter bootloader: Family 0x01, Index 0x00, Data 0x08        +
+  2. Erase app:        Family 0x80, Index 0x00                   +
+  3. Set num pages:    Family 0x80, Index 0x01                   +
+  4. Set IV:           Family 0x80, Index 0x02 (11 bytes)        +
+  5. Set Auth:         Family 0x80, Index 0x03 (16 bytes)        +
+  6. Write pages:      Family 0x80, Index 0x04 (8KB each)        +
+  7. Exit bootloader:  Family 0x01, Index 0x00, Data 0x00        +
+                                                                 +
+  Memory Requirements:                                           +
+  • Page buffer: 8192 bytes (heap-allocated)                     +
+  • Thread stack: 3072 bytes (file I/O + I2C)                    +
+  • Chunked I2C writes: 128 bytes per transaction                +
+                                                                 +
+  Files: *.msbl path API remains; FatFS `/NAND:` is gone — host  +
+  must supply bytes via another transport before update runs.    +
+  UI: max32664_update_ui.c (file picker obsolete without FatFS)  +
+-----------------------------------------------------------------+

Files: src/sensors/max32664_update.c, src/max32664_update.h,
       src/ui/max32664_update_ui.c, src/max32664_update_ui.h
```

