# Hardware Reference - nRF52840 Wearable Health Monitor

**Last Updated**: 2026-05-26  
**Build Status**: ✓ Successful (Zero Warnings)  
**Memory**: Flash 95.62% | RAM 86.44%

---

## Quick Reference - Pin Summary

### GPIO Pin Mapping

| Pin | Function | Direction | Description | Active Level |
|-----|----------|-----------|-------------|--------------|
| **I2C0 Bus** |
| P0.13 | I2C0_SDA | Bidirectional | Main board I2C data | - |
| P0.14 | I2C0_SCL | Output | Main board I2C clock | - |
| **I2C1 Bus** |
| P0.16 | I2C1_SDA | Bidirectional | Sensor hub I2C data | - |
| P0.15 | I2C1_SCL | Output | Sensor hub I2C clock | - |
| **SPI2 Display** |
| P1.08 | SPI2_SCK | Output | Display SPI clock | - |
| P0.26 | SPI2_MOSI | Output | Display SPI data | - |
| P1.09 | SPI2_CS | Output | Display chip select | Active Low |
| P1.10 | DISP_DC | Output | Display data/command | High=Data |
| P1.12 | DISP_RST | Output | Display reset | Active Low |
| **QSPI Flash** |
| P0.17 | QSPI_CS | Output | External flash select | Active Low |
| P0.19 | QSPI_SCK | Output | QSPI clock (32MHz) | - |
| P0.22 | QSPI_IO0 | Bidirectional | QSPI MISO | - |
| P0.23 | QSPI_IO1 | Bidirectional | QSPI MOSI | - |
| P0.21 | QSPI_IO2 | Bidirectional | QSPI WP# (pull-up) | - |
| P1.00 | QSPI_IO3 | Bidirectional | QSPI HOLD# (pull-up) | - |
| **UART0 Wi-Fi** |
| P0.06 | UART_TX | Output | WExx module RX | - |
| P0.08 | UART_RX | Input | WExx module TX | - |
| P0.05 | WIFI_EN | Output | WExx CHIP_EN | Active High |
| **ADC** |
| P0.03 | AIN1 | Input | Glucose `GLUC_SIGNAL` (INA122UA out) | Analog |
| **PWM** |
| P0.12 | PWM1_CH0 | Output | Buzzer PWM | Inverted |
| **GPIO Control** |
| P0.04 | LCD_VLED | Output | Display backlight enable | Active High |
| P0.24 | IR_LED | Output | Glucose `IRLED_POWER` (SMC1550 path) | Active High |
| P0.29 | SW1 | Input | Screen navigation button | Active Low |
| P0.31 | PMIC_INT | Input | MAX20360 interrupt | Active Low |
| P1.01 | TOUCH_RST | Output | FT6336 reset | Active Low |
| P1.02 | TEMP_INT | Input | MAX30208 data ready | Active Low |
| P1.03 | RTC_INT | Input | MCP7940N alarm | Active Low |
| P1.04 | PMIC_PFN1 | Input | SW2 power button (KIN) | Active Low |
| P1.15 | PMIC_PFN2 | Input | KOUT mirrors SW2 (KIN mode) | Active Low |
| P1.13 | IVMON | Input | PMIC current monitor | Analog |

---

## I2C Bus Configuration

### I2C0 - Main Board Bus (100 kHz)
**Pins**: P0.13 (SDA), P0.14 (SCL)  
**Pull-ups**: External 4.7k╬⌐ to 3.3V

| Device | Address | Description | Driver Config |
|--------|---------|-------------|---------------|
| MAX20360 PMIC | 0x28 | Power management, regulators | `CONFIG_REGULATOR_MAX20360=y` |
| MAX20360 Fuel Gauge | 0x36 | Battery SOC estimation | `CONFIG_FUEL_GAUGE_MAX20360=y` |
| FT6336 Touch | 0x38 | Capacitive touch controller | `CONFIG_INPUT_FT5336=y` |
| MAX30205 | 0x48 | Pulse/finger temperature | `CONFIG_MAX302XX=y` |
| MAX30208 | 0x53 | Wearable temperature | `CONFIG_MAX302XX=y` |
| MAX30102 PPG | 0x57 | HR/SpO2 standalone (fallback path) | `CONFIG_MAX3010X_PPG=y` |
| VCNL3040 | 0x60 | Proximity/ALS sensor | `CONFIG_VCNL3040=y` |
| MCP7940N | 0x6F | Real-time clock (RTC API) | `CONFIG_COUNTER_MCP79XX=y` |

### I2C1 - Sensor Hub Bus (100 kHz)
**Pins**: P0.16 (SDA), P0.15 (SCL)  
**Note**: External pullups via dual PCA9306 bidirectional level shifters (3.3V ↔ 1.8V). PCA1 handles I2C SDA/SCL; PCA2 handles MFIO and RESET signal isolation.

| Device | Address | Description | Driver Config |
|--------|---------|-------------|---------------|
| MAX32664 Hub | 0x55 | PPG sensor hub (GWEC variant) | `CONFIG_MAX32664=y` |

**MAX32664 Internal Sensors** (accessed via hub passthrough):
- MAX86141 PPG AFE (Green/Red/IR LEDs)
- LIS2DS12 Accelerometer

---

## SPI/QSPI Configuration

### SPI2 - Display Bus (20 MHz)
| Signal | Pin | Description |
|--------|-----|-------------|
| SCK | P1.08 | SPI clock |
| MOSI | P0.26 | SPI data out |
| CS# | P1.09 | Chip select (active low) |
| DC | P1.10 | Data/command |
| RST# | P1.12 | Display reset (active low) |

**Display**: ST7789V 240x240 16-bit color  
**Config**: `CONFIG_DISPLAY=y`, `CONFIG_MIPI_DBI=y`

### QSPI - External Flash (32 MHz)
**Device**: Macronix MX25R6435F (8MB / 64Mbit)  
**JEDEC ID**: 0xC2 0x28 0x17

| Signal | Pin | Description |
|--------|-----|-------------|
| CS# | P0.17 | Chip select |
| SCK | P0.19 | Clock |
| IO0 | P0.22 | MISO |
| IO1 | P0.23 | MOSI |
| IO2 | P0.21 | WP# (needs pull-up) |
| IO3 | P1.00 | HOLD# (needs pull-up) |

---

## Power Architecture

### MAX20360 PMIC Regulators

| Rail | Output | Range | Step | Loads |
|------|--------|-------|------|-------|
| BK1 | 1.2V | 0.55-3.7V | 10mV | nRF52840 Core |
| BK2 | 1.8V | 0.55-2.125V | 25mV | MAX32664, NLSX5004, 1.8V I/O |
| BK3 | Default | 0.55-3.7V | 10mV | General |
| BBOUT | 5.0V | 2.5-5.0V | 50mV | USB VBUS, PPG LEDs, Sensors |
| L2OUT | 3.0V | 0.8-3.3V | 100mV | Buzzer transistor driver |

### Charger Configuration
- **Chemistry**: Li-Ion 4.2V
- **Charge Current**: 500mA default
- **Input Current Limit**: 500mA

---

## Memory Layout

### Internal Flash (1MB)
| Partition | Address | Size | Purpose |
|-----------|---------|------|---------|
| MCUboot | 0x00000 | 40KB | Bootloader (locked) |
| image-0 (Slot0) | 0x0A000 | 960KB | Primary application |
| runtime | 0xE8000 | 96KB | BSS, heap, stacks |

### External Flash (8MB QSPI)

Canonical map: [`PARTITION_LAYOUT.md`](../build/PARTITION_LAYOUT.md) /
`boards/raytac_overlay/51_qspi.overlayinc`.

| Partition | Offset | Size | Purpose |
|-----------|--------|------|---------|
| image-1 (OTA) | `0x000000` | 968 KB | OTA staging |
| image-scratch | `0x0f2000` | 16 KB | MCUboot scratch |
| resource-primary | `0x0f6000` | 512 KB | Logos/fonts/icons |
| resource-secondary | `0x176000` | 512 KB | Asset OTA staging |
| calibration | `0x1f6000` | 64 KB | Factory calibration (RO) |
| storage_nvs | `0x206000` | 128 KB | External NVS / settings |
| glucose-model-primary | `0x226000` | 256 KB | Active packed glucose model |
| glucose-model-secondary | `0x266000` | 256 KB | Model staging (BLE/flash) |
| record_store | `0x2a6000` | ~5.35 MB | Measurement summaries + raw chunks |

> FatFS `userdata` / USB MSC `/NAND:` is **removed**.
> **QSPI map:** use [PARTITION_LAYOUT.md](../build/PARTITION_LAYOUT.md) (table above may lag).

### RAM (256KB)

Static layout grows with `CONFIG_APP_FEATURE_USB` / shell.

**Production default (shell/USB off, 2026-07-30)** — [BUILD_STATUS.md](../build/BUILD_STATUS.md):

| Region | Used | % of region |
|--------|------|-------------|
| FLASH | ~901,000 B | ~91.7% of 982,704 B |
| RAM | see linker output | typically high 80s–low 90s % |

**With shell + USB (`SHELL_MINIMAL`)** expect ~**98%** FLASH — diagnostic only.

| Pool (typical) | Size | Purpose |
|----------------|------|---------|
| Main heap (`k_heap`) | 32 KB | generic alloc (`CONFIG_HEAP_MEM_POOL_SIZE`) |
| LVGL pool | **31,744 B** (links) / **38,912 B** (overflows) | UI widgets, draw |
| Picolibc malloc arena | 4 KB | Static BSS arena |
| Main stack | 8 KB | Main thread (LVGL + init) |

With **`APP_FEATURE_USB=y`**, expect extra static RAM for USBD/UDC stacks + buf pools
(`usb.conf`: UDC 1536, USBD 2048). MSC is not used.

Boot log: module `ram_budget` prints `_end` and libc arena size. Runtime peaks: enable `CONFIG_APP_FEATURE_DIAG_MONITOR` — [MEMORY_TUNING.md](../debugging/MEMORY_TUNING.md).

---

## Sensor Quick Reference

| Sensor | Type | Interface | Address | Interrupt | Config |
|--------|------|-----------|---------|-----------|--------|
| MAX32664 | PPG Hub | I2C1 | 0x55 | P1.06 MFIO + P0.20 RSTN | `CONFIG_MAX32664=y` |
| MAX86141 | PPG AFE | via Hub | - | - | Passthrough |
| LIS2DS12 | Accel | via Hub | - | - | Passthrough |
| MAX30205 | Temp (spot) | I2C0 | 0x48 | - | `CONFIG_MAX302XX=y` |
| MAX30208 | Temp (wear) | I2C0 | 0x53 | P1.02 | `CONFIG_MAX302XX=y` |
| VCNL3040 | Proximity | I2C0 | 0x60 | - | `CONFIG_VCNL3040=y` |
| MCP7940N | RTC | I2C0 | 0x6F | P1.03 | `CONFIG_COUNTER_MCP79XX=y` |
| Glucose NIRS AFE | ADC + GPIO | AIN1 / P0.24 | P0.03 / P0.24 | - | `CONFIG_GLUCOSE_SENSOR=y` |
| FT6336 | Touch | I2C0 | 0x38 | - | `CONFIG_INPUT_FT5336=y` |

**Glucose NIRS AFE:** **SMC1550 (1550 nm) + SMC1050 (1050 nm) paralleled**
(both sides of PD, one `IRLED_POWER`) → G11193-03 → OPA358 (TIA **Rf = R7 =
10 MΩ**, **Cf = C2 ≈ 0.8 pF**, EN→3V3) → INA122UA (**RG pins 1&8 shorted to
GND**, REF=GND) → SAADC. Dual emitters are **not** sequenced in firmware.
Full chain: [`../sensors/GLUCOSE_ANALOG_FRONTEND.md`](../sensors/GLUCOSE_ANALOG_FRONTEND.md).

**MCP7940N notes:** Zephyr RTC API driver (`microchip,mcp79xx`). VBATEN is set on calendar `set_time`. Power-fail (PWRFAIL) history is **not** used by firmware; writing RTCWKDAY clears it. Periodic HW alarm is off by default (`CONFIG_APP_RTC_PERIODIC_ALARM=n`). Datasheet: DS20005010J.

---

## Communication Interfaces

### UART0 - Wi-Fi Module
- **Baud**: 38400
- **TX**: P0.06 → WExx RXD
- **RX**: P0.08 ← WExx TXD
- **Module Enable**: P0.05 (CHIP_EN, active high)
- **Config**: `CONFIG_WIFI_WEXX=y`

### USB Device
- **Mode**: CDC ACM serial (diagnostics shell VCOM) when `APP_FEATURE_SHELL=y`
  (selects `APP_FEATURE_USB`). USB-only (`USB=y`, `SHELL=n`) enables the stack
  without the Zephyr shell.
- **Overlays**: `conf/features/usb.conf` (stack) · `conf/features/shell.conf` (shell)
- **DT**: always present (`64_usb_cdc` / `65_shell_uart`); drivers gated by Kconfig
- **Not used**: Mass Storage Class (removed with FatFS)
- **Shell**: `nisense rec …` / `nisense cfg show` — see
  [`STORAGE_NOR_RECORD_STORE.md`](../architecture/STORAGE_NOR_RECORD_STORE.md)
- Logging remains on **RTT** channel 0
- Lifecycle / sticky CLI: [`BUILD_FLASH_DEPLOY.md`](../build/BUILD_FLASH_DEPLOY.md)

### Bluetooth LE
- **Hardware**: nRF52840 integrated
- **Default name**: `NiSense` (`CONFIG_BT_DEVICE_NAME`); after HW detect may
  become **NiSense Watch** / **NiSense Pulse**
- **Services**: Wearable config, sensor data, Wi‑Fi config, glucose model
  transfer (`…def3`), record sync (`…def4`)
- **Config**: `CONFIG_BT=y` / `APP_FEATURE_BLE`

---

## Device Tree Aliases

```dts
aliases {
    /* PMIC and power */
    pmic0 = &max20360;
    bk1 = &max20360_bk1;
    bk2 = &max20360_bk2;
    bk3 = &max20360_bk3;
    bbout = &max20360_bbout;
    l2out = &max20360_l2out;
    fuelgauge = &max20360_fuel_gauge;
    charger0 = &max20360_charger;
    
    /* Sensors */
    max32664-hub = &max32664_hub;
    ppg-sensor = &max86141_ppg;
    accel-sensor = &lis2ds12_accel;
    temp-wearable = &max30208;
    temp-pulse = &max30205;
    vcnl3040 = &vcnl3040;
    glucose0 = &glucose_sensor;
    
    /* Peripherals */
    rtc0 = &mcp7940n;
    display0 = &st7789v_display;
    buzzer0 = &buzzer_pwm;
    wifi0 = &wexx_wifi;
    watchdog0 = &wdt0;
    zephyr-udc0 = &usbd;
    
    /* Buttons */
    sw1 = &sw1;  /* P0.29 - Screen navigation */
    sw2 = &sw2;  /* P1.04 - PMIC PFN1 */
};
```

---

## Build Configuration Reference

### Critical prj.conf Settings
```kconfig
# Memory
CONFIG_MAIN_STACK_SIZE=6144
CONFIG_HEAP_MEM_POOL_SIZE=40960
CONFIG_LV_Z_MEM_POOL_SIZE=32768

# Enabled Features
CONFIG_APP_FEATURE_SPO2=y
CONFIG_APP_FEATURE_GLUCOSE=n      # Saves ~200KB flash
# Production Watch PPG path: RAW FIFO → host ppg_algo (default in conf/features/max32664.conf).
# HUB = Maxim WHRM+WSpO2 on-chip; PASSTHROUGH = diagnostic AFE bridge only.
CONFIG_MAX32664_MODE_RAW=y
CONFIG_PPG_ALGO=y

# Debug
CONFIG_LOG_BACKEND_RTT=y
CONFIG_MPU_STACK_GUARD=y
```

### Flash/RAM Utilization

**Production default (2026-07-30, shell/USB off)** — [BUILD_STATUS.md](../build/BUILD_STATUS.md):

```
Memory region         Used Size  Region Size  %age Used
           FLASH:     ~901000 B     982704 B    ~91.7%
```

**Shell + USB stress** (~98% FLASH). Verify locally with `west build`.

See [MEMORY_TUNING.md](../debugging/MEMORY_TUNING.md) · [BUILD_FLASH_DEPLOY.md](../build/BUILD_FLASH_DEPLOY.md).
---

## Hardware Caveats

1. **QSPI WP#/HOLD#**: P0.21 and P1.00 require external pull-ups to VCC
2. **I2C1 Pullups**: Disabled internally - NLSX5004 provides 1.8V pullups
3. **newlib-nano**: `sscanf("%f")` fails silently - use `strtof()`
4. **Shell**: USB CDC ACM when `APP_FEATURE_SHELL=y` (selects USB); RTT channel 0 is logs only
5. **QSPI busy**: Coordinate NOR program/erase with `nor_qspi_busy` around XIP / record_store / model writes
6. **MAX30208 Address**: Latched at power-on from GPIO pins (0x53 = GPIO1=1, GPIO0=1)

---

## Reference Files

- **Device Tree Overlay**: [boards/raytac_mdbt50q_db_40_nrf52840.overlay](../../../boards/raytac_mdbt50q_db_40_nrf52840.overlay)
- **Project Config**: [prj.conf](../../../prj.conf)
- **Pin Control**: See overlay `&pinctrl` section
- **PMIC Driver**: [drivers/regulator/max20360/](../../../drivers/regulator/)
- **Sensor Drivers**: [drivers/sensor/](../../../drivers/sensor/)
