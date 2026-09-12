# Multi-Sensor Driver Development Guide

## ⚠ PLANNING/REFERENCE DOCUMENT

**Last Updated**: 2026-06-14  
**Status**: Reference document — describes planned and implemented sensors  
**Actual Status**: See [`SENSOR_STATUS.md`](SENSOR_STATUS.md) for what is currently implemented

> **NiSense is a dual-mode device.** The sensor set below covers both hardware profiles:
> - **WEARABLE** (wrist): MAX32664 hub + MAX86141, LIS2DS12, MAX30208 temperature
> - **PULSE** (finger-clip): MAX30102 standalone PPG, MAX30205 temperature, 1550 nm NIRS glucose circuit
>
> Both profiles run the same Zephyr application. Profile selection is automatic.

> **Note**: Board overlay path in this document uses the old name `boards/arm/nrf52840_mdbt50q.overlay`.
> The actual path is `boards/raytac_mdbt50q_db_40_nrf52840.overlay`. I2C addresses for MAX32664
> use 0x55 (not 0x68/0x66 as shown below).

## Overview: 7 Sensors (Planned)

Your wearable device integrates multiple sensors via different buses. This guide covers hardware connections, Zephyr sensor classifications, and driver development patterns. **Not all sensors listed are implemented yet**.

---

## Sensor Inventory & Hardware Connections

### From Schematics (wearable-SNR-V2 + WEARABLE_MB-V2-R6)

| Sensor | Part | Bus | Interface | Pins | Location | Zephyr Class |
|--------|------|-----|-----------|------|----------|---|
| **1. MAX30205** | Temperature | I2C | I2C (slave) | SCL/SDA | Sensor board (U5) | `SENSOR_CHAN_AMBIENT_TEMP` |
| **2. MAX30101EFD+T** | PPG (finger) | I2C | I2C + INT | SCL/SDA/INT | Sensor board (U6) | `SENSOR_CHAN_HEART_RATE` / `SENSOR_CHAN_IR` |
| **3. VCNL3040** | Proximity/Ambient | I2C | I2C + INT | SCL/SDA/INT | Sensor board (U14) | `SENSOR_CHAN_PROX` / `SENSOR_CHAN_LIGHT` |
| **4. LIS2DS12** | Accelerometer | I2C/SPI | I2C or SPI + INT | CS/SCL/SDA/INT | Sensor board (U13) | `SENSOR_CHAN_ACCEL_X/Y/Z` |
| **5. MAX86141** | PPG (wrist) | SPI | SPI + INT | SCLK/MOSI/MISO/CS/INT | Sensor board (U4) | `SENSOR_CHAN_HEART_RATE` / `SENSOR_CHAN_IR` |
| **6. MAX32664GWED+** | Sensor Hub (v1) | I2C | I2C (slave) + INT | SCL/SDA/INT + MFIO/RSTN | Sensor board (U9) | `SENSOR_HUB` (not standard) |
| **7. MAX32664GWEC+** | Sensor Hub (v2) | I2C + SPI | I2C (slave) + SPI (master) | SCL/SDA/INT + SPI pins | Sensor board (U7) | `SENSOR_HUB` (not standard) |

**Key Observations from Schematic:**
- I2C bus pull-ups on SDA/SCL (~4.7k╬⌐ typical)
- All I2C devices share main I2C bus (SDA_SENSOR, SCL_SENSOR)
- MAX86141 is standalone SPI
- LIS2DS12 can be I2C or SPI (pin configurable)
- MAX32664 variants have different pin counts but similar function
- All sensors on 3.3V or 1.8V rails
- Interrupts are active-low, open-drain (pulled up)

---

## Zephyr Sensor Classification & Naming Conventions

### Zephyr Sensor Framework Overview

Zephyr groups sensors into **categories** via `sensor_driver_api`:

```c
struct sensor_driver_api {
    sensor_attr_set_t attr_set;
    sensor_trigger_set_t trigger_set;
    sensor_sample_fetch_t sample_fetch;
    sensor_channel_get_t channel_get;
};
```

Sensor channels are defined per-device type:

| Category | Channels | Examples |
|----------|----------|----------|
| **Temperature** | `AMBIENT_TEMP` | MAX30205, etc. |
| **Accelerometer** | `ACCEL_X/Y/Z` | LIS2DS12, BMI160 |
| **Gyroscope** | `GYRO_X/Y/Z` | — |
| **Proximity** | `PROX` | VCNL3040, apds9960 |
| **Ambient Light** | `LIGHT` | VCNL3040, OPT3001 |
| **Heart Rate** | `HEART_RATE` (custom) | MAX86141, MAX30101 |
| **PPG (IR/Red)** | `IR`, `RED` (custom) | MAX86141, MAX30101 |
| **Sensor Hub** | — (proprietary) | MAX32664 |

### Naming Without `_driver` Suffix

**Zephyr Convention**: Use `compatible` string format: `manufacturer,device`

Examples:
- `"maxim,max30205"` ← Temperature
- `"maxim,max30101efd"` ← PPG sensor
- `"vishay,vcnl3040"` ← Proximity/Ambient Light
- `"st,lis2ds12"` ← Accelerometer
- `"maxim,max86141"` ← PPG (wrist)
- `"maxim,max32664gwed"` ← Sensor Hub v1 (I2C slave)
- `"maxim,max32664gwec"` ← Sensor Hub v2 (I2C slave + SPI master)

**Directory Structure** (NO `_driver` suffix):

```
drivers/
+-- sensor/
+   +-- max30205/          ← Temperature
+   +-- max30101efd/       ← PPG (finger)
+   +-- vcnl3040/          ← Proximity + Ambient Light
+   +-- lis2ds12/          ← Accelerometer
+   +-- max86141/          ← PPG (wrist)
+   +-- max32664/          ← Sensor Hub (both variants)
+-- iio/                   ← IIO drivers (alternative)
```

---

## Hardware Connections Detail

### 1. MAX30205 (Temperature, I2C)

**Schematic**: U5 on sensor board

```
VDD --+------------------+-- 3.3V
      +                  +
     C20 (22µF)         [MAX30205]
      +                  +
GND --+------------------+-- GND
      
Pull-ups (external):
SCL --+-- 4.7k╬⌐ -- 3.3V
SDA --+
      +-- (I2C main bus, ~100kHz)

I2C Address: 0x48 (default, A0=A1=A2 tied to GND per schematic)
Interrupts: OS pin (optional, not used in your schematic)
```

**Zephyr Classification**: `SENSOR_CHAN_AMBIENT_TEMP`

**Interface**: I2C (standard Zephyr I2C sensor API)

**Register Access**: 
- 0x00: Temperature (MSB, LSB) – 12-bit ±0.0625°C resolution
- 0x01–0x03: Config (read-only for basic use)

---

### 2. MAX30101EFD+T (PPG Sensor, I2C)

**Schematic**: U6 on sensor board

```
VLED+ --+-------------------+-- 5V (from PMIC)
        +   LED anode       +
       C18 (22µF)        [MAX30101]
        +   LED cathode    +
GND ----+-------------------+-- GND

SCL/SDA: I2C main bus (100 kHz)
INT: Active-low interrupt (PPG_INT → nRF52840)

VLED+: Dual voltage (5V for LEDs, 3.3V for logic)
```

**Zephyr Classification**: 
- `SENSOR_CHAN_HEART_RATE` (computed from PPG)
- `SENSOR_CHAN_IR` (raw IR channel)
- Custom: `SENSOR_CHAN_RED` (red channel)

**Interface**: I2C (standard I2C sensor API) + GPIO interrupt

**Register Access**:
- 0x00–0x02: FIFO (128 × 3 bytes per channel)
- 0x01: Interrupt Status
- 0x02–0x03: Enable/Configure
- 0x07–0x10: LED current (8-bit DACs)
- 0x11: SPO2 config
- 0x1F: Part ID (expected 0x15)

**Differences vs MAX86141**: 
- Single optical channel (vs dual)
- Simpler FIFO (no tag system)
- Similar LED driver architecture

---

### 3. VCNL3040 (Proximity + Ambient Light, I2C)

**Schematic**: U14 on sensor board

```
VDD --+---------------------+-- 3.3V
      +                     +
     R38 (10k╬⌐)          [VCNL3040]
     C15 (1µF)           (8-pin)
      +    CATHODE ---^  LED (drives proximity IR)
      +    ANODE ---^    VLED+ (5V)
      +
GND --+---------------------+-- GND

SCL/SDA: I2C main bus
INT: Proximity interrupt (active-low)

I2C Address: 0x60 (fixed)
LED: IR LED on-board (cathode to CATHODE pin, anode to VLED+)
```

**Zephyr Classification**:
- `SENSOR_CHAN_PROX` (proximity distance)
- `SENSOR_CHAN_LIGHT` (ambient light)
- Custom: `SENSOR_CHAN_ALS` (ambient light sensor)

**Interface**: I2C + GPIO interrupt (optional)

**Register Access**:
- 0x00: Command register
- 0x04–0x05: Proximity data (16-bit)
- 0x09–0x0A: Ambient light data (16-bit)
- 0x03: Interrupt control
- 0x08: Interrupt status

**Use Case**: 
- Proximity: Detect finger on sensor (wrist PPG)
- Ambient Light: Environment illumination (glucose measurement correction)

---

### 4. LIS2DS12 (Accelerometer, I2C or SPI)

**Schematic**: U13 on sensor board

```
I2C Mode (as shown):
SCL/SP -- 4.7k╬⌐ pullups -- 3.3V ---+
SDA/SDI--                           +- I2C bus
CS --------------------------------- GND (ties to I2C mode)

VDD ----+----------------+--- 3.3V
VDD_IO-+  [LIS2DS12]    +--- 3.3V
        +  (14-pin LGA)  +
GND ----+----------------+--- GND

INT1, INT2: Interrupts (active-low)
```

**Zephyr Classification**: `SENSOR_CHAN_ACCEL_X/Y/Z`

**Interface**: I2C (pin-selectable; schematic shows I2C mode)

**Register Access**:
- 0x0F: Who Am I (expected 0x43)
- 0x20: Control register (enable XYZ)
- 0x28–0x2D: Acceleration data (6 bytes, 12-bit signed)
- 0x04–0x15: Various control/status

**I2C Address**: 
- 0x18 if SDO tied to GND (schematic shows 0x18 via R3/pull-down)
- 0x19 if SDO tied to VDD

**Use Cases**:
- Wrist motion detection (for PPG sensor quality)
- Step counting
- Activity classification

---

### 5. MAX86141 (PPG Optical, SPI)

**Schematic**: U4 on sensor board (as previously covered)

```
SPI Interface:
SCLK --^ P0.25 (nRF52840)
MOSI --^ P0.24
MISO <---- P0.23
CSB  --^ P0.22 (active-low)

Interrupts:
PPG_INT --^ GPIO0.5 (active-low, pulled up via R33 3.3k╬⌐)

Power:
VDD_ANA/DIG: 1.8V (from PMIC)
VLED: 5V (from PMIC)
PGND: Common return with GND_ANA (<0.3V tolerance)
```

**Zephyr Classification**: `SENSOR_CHAN_HEART_RATE`, `SENSOR_CHAN_IR`, custom `SENSOR_CHAN_RED`

**Interface**: SPI (4 MHz max) + GPIO interrupt

**Hardware Gotchas**:
- Voltage tolerance: VDD_ANA–DIG within ±0.3V (else ADC corrupts)
- FSM restart on register write (datasheet page 25)
- LED compliance: 124 mA @ 5V requires ~1V headroom

**Part ID**: 0x15 (verify on init)

---

### 6 & 7. MAX32664GWED+ vs MAX32664GWEC+ (Sensor Hubs)

**Both on sensor board but different variants:**

**U9 = MAX32664GWED+** (Version D):
```
I2C Slave Interface (host communication):
SCL --^ P0.xx (nRF to MAX32664)
SDA <---- (I2C slave mode)
INT --^ GPIO (optional)

I2C Master Interface (to sensors):
SENSOR_SCL --^ (communicates with MAX30101, VCNL3040, etc. via I2C master)
SENSOR_SDA

No SPI (GWED has no SPI capability)
32K clock: SENSOR_32KIN/OUT (for timing)
```

**U7 = MAX32664GWEC+** (Version C):
```
I2C Slave + SPI Master:
SCL --^ (I2C slave to host)
SDA <----
INT --^ GPIO

SPI Master (to PPG sensors):
SENSOR_MOSI --^ (drives MAX86141 as master)
SENSOR_MISO <----
SENSOR_SCK  --^
PPG_CS      --^

Hybrid mode: I2C slave from nRF52840, SPI master to MAX86141
```

**Zephyr Classification**: Proprietary driver, not standard `SENSOR_CHAN_*`

**Use Cases**:
- GWED: Acts as I2C-to-I2C bridge (proxies I2C sensors)
- GWEC: Acts as I2C-to-SPI converter (proxies SPI sensors like MAX86141)
- Both: Embedded algorithms (SpO2, HR, activity)

**Complexity**: 
- Requires vendor firmware update
- Proprietary protocol (not pure Zephyr sensor driver)
- Often used as **sensor hub only** (no custom driver development)

---

## Zephyr Ecosystem Mapping

### Standard Sensor Drivers (In-Tree)

Zephyr has native drivers for some sensors:

| Sensor | Zephyr Driver | Status | Note |
|--------|--|--|--|
| MAX30205 | `drivers/sensor/max30205/` | ✓ In-tree | Use as-is |
| LIS2DS12 | `drivers/sensor/lis2ds12/` | ✓ In-tree | Use as-is |
| VCNL3040 | `drivers/sensor/vcnl3040/` | ⚠ Limited | Only proximity, not ALS |
| MAX30101 | ✗ Not in-tree | ⚠ Out-of-tree | Need custom or use hub |
| MAX86141 | ✗ Not in-tree | ⚠ Out-of-tree | Custom driver required |
| MAX32664 | ⚠ Proprietary | ✗ No standard driver | Use vendor firmware |

### Sensor Channel Hierarchy

```
Zephyr Sensor Channels (zephyr/include/zephyr/drivers/sensor.h):

SENSOR_CHAN_ACCEL_X, Y, Z             ← LIS2DS12
SENSOR_CHAN_GYRO_X, Y, Z              ← (not in your device)
SENSOR_CHAN_MAGN_X, Y, Z              ← (not in your device)
SENSOR_CHAN_AMBIENT_TEMP              ← MAX30205
SENSOR_CHAN_OBJECT_TEMP               ← (IR temp sensor)
SENSOR_CHAN_LIGHT                     ← VCNL3040 (ALS)
SENSOR_CHAN_PROX                      ← VCNL3040 (proximity)
SENSOR_CHAN_HUMIDITY                  ← (not in your device)
SENSOR_CHAN_PRESS                     ← (not in your device)

Custom (application-defined):
SENSOR_CHAN_PRIV_START + 0            ← SENSOR_CHAN_HEART_RATE
SENSOR_CHAN_PRIV_START + 1            ← SENSOR_CHAN_IR
SENSOR_CHAN_PRIV_START + 2            ← SENSOR_CHAN_RED
SENSOR_CHAN_PRIV_START + 3            ← SENSOR_CHAN_SPO2
```

### DT Binding Naming

Zephyr binding convention: `<vendor>,<device>[variants]`

```yaml
# maxim,max30205.yaml
compatible: "maxim,max30205"

# maxim,max30101efd.yaml (note: lowercase, variant suffix)
compatible: "maxim,max30101efd+t"  ← Exact part number

# vishay,vcnl3040.yaml
compatible: "vishay,vcnl3040"

# st,lis2ds12.yaml
compatible: "st,lis2ds12"

# maxim,max86141.yaml
compatible: "maxim,max86141"

# maxim,max32664.yaml (single binding for both variants)
compatible: "maxim,max32664gwed"
          or
          "maxim,max32664gwec"
```

---

## Driver Development Priority & Approach

### Tier 1: Use In-Tree Drivers (NO Custom Code Needed)

✓ **MAX30205** (Temperature)
- Use official Zephyr driver
- Just configure via DT overlay:
```dts
&i2c0 {
    temp_sensor: max30205@48 {
        compatible = "maxim,max30205";
        reg = <0x48>;
    };
};
```

✓ **LIS2DS12** (Accelerometer)
- Use official Zephyr driver
- Configure I2C or SPI mode via DT

✓ **VCNL3040** (Proximity + Ambient Light)
- Zephyr driver available
- Minimal custom code if needed

### Tier 2: Custom Out-of-Tree Drivers (Moderate Effort)

⚠ **MAX30101EFD+T** (PPG, I2C)
- Not in Zephyr main
- Similar to MAX86141 but simpler (single channel, no tag system)
- Build custom driver following MAX86141 pattern

⚠ **MAX86141** (PPG, SPI)
- Already planned (from previous discussion)
- Full custom driver (~900 lines)
- Production-ready

### Tier 3: Vendor Firmware / Proprietary

✗ **MAX32664GWED+** / **MAX32664GWEC+** (Sensor Hubs)
- Proprietary firmware (provided by Maxim)
- Use **as-is**, no custom driver development
- Communicate via I2C slave protocol
- Optional: Wrapper layer for multi-sensor coordination

---

## Driver Integration Profiles

### In-Tree Driver Candidates
1. MAX30205 (device-tree configuration)
2. LIS2DS12 (device-tree configuration, interface selection)
3. VCNL3040 (device-tree configuration)

### Custom Out-of-Tree Driver Candidates
1. **MAX30101EFD** (I2C PPG, ~400 lines)
    - Simpler than MAX86141
    - Single optical channel
    - FIFO handling without tag system

2. **MAX86141** (SPI PPG, ~900 lines)
    - Dual-channel optical path
    - Tagged FIFO parsing
    - Higher implementation complexity

### Sensor Hub Integration Options
1. MAX32664GWED
2. MAX32664GWEC
3. Direct-sensor architecture without sensor hubs

---

## File Structure (Recommended)

```
NiSense/
+-- src/
+   +-- main.c
+   +-- sensors.c/.h         ← Multi-sensor coordinator
+   +-- max30101.c/.h        ← MAX30101 I2C PPG wrapper
+   +-- max86141.c/.h        ← MAX86141 SPI PPG wrapper
+   +-- vcnl3040.c/.h        ← Proximity wrapper (optional)
+   +-- lis2ds12.c/.h        ← Accelerometer wrapper (optional)
+   +-- ui.c                 ← UI with sensor screens
+
+-- drivers/sensor/
+   +-- max30101/            ← I2C PPG driver (custom)
+   +   +-- max30101.c
+   +   +-- max30101.h
+   +   +-- Kconfig
+   +   +-- CMakeLists.txt
+   +
+   +-- max86141/            ← SPI PPG driver (custom)
+   +   +-- max86141.c
+   +   +-- max86141.h
+   +   +-- Kconfig
+   +   +-- CMakeLists.txt
+   +
+   +-- (max30205, lis2ds12, vcnl3040 use in-tree)
+
+-- dts/bindings/sensor/
+   +-- maxim,max30101efd.yaml
+   +-- maxim,max86141.yaml
+   +-- (others use in-tree bindings)
+
+-- boards/arm/
+   +-- nrf52840_mdbt50q.overlay  ← All sensor DT nodes
+
    +-- SENSORS.md           ← Sensor guide
```

---

## Next Steps

**For each sensor, Cursor prompt should specify:**

1. ✓ `compatible` string (e.g., `"maxim,max30101efd+t"`)
2. ✓ I2C or SPI interface
3. ✓ Interrupt GPIO if applicable
4. ✓ Zephyr sensor channels to expose
5. ✓ Hardware connection details from schematic
6. ✓ No `_driver` suffix (use bare device name)
7. ✓ Follow your project standards (src/ + drivers/, DT macros, RTT logging)

**Example Cursor Prompt Format:**

> "Build custom driver for MAX30101EFD+T optical PPG sensor.
> - Bus: I2C (0x57 default, can be 0x58 via external resistor)
> - Interrupt: PPG_INT GPIO (active-low)
> - Power: 3.3V VDD, 5V VLED
> - Channels: SENSOR_CHAN_HEART_RATE, SENSOR_CHAN_IR, custom SENSOR_CHAN_RED
> - Schematic: Sensor board U6
> - Follow project standards (no _driver suffix, DT macros, RTT logging)"

---

**Ready to proceed with individual sensor prompts?** Each sensor driver can be built independently, then integrated into multi-sensor UI.


