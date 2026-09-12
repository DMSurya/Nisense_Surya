# MAX32664 Sensor Hub - Variant-Based Modular Architecture

**Created**: 2026-01-19  
**Status**: Current implementation  
**Driver Version**: v2.0 (Modular Refactor)

## Overview

The MAX32664 sensor hub driver has been refactored from a monolithic 2246-line file into a modular variant-based architecture (Jan 2026). This document explains the new structure, dispatch system, and variant support.

## Architecture Summary

### Before (Monolithic)
- Single `max32664.c` file: 2246 lines
- All 4 variants (A/B/C/D) in one file with massive switch statements
- Difficult to maintain and extend
- Code duplication between variants

### After (Modular)
- Common core: `max32664.c` (1959 lines)
- Variant-specific files: `max32664a/b/c/d.c` (110-250 lines each)
- Function pointer dispatch via `max32664_variant_ops` structure
- Private API: `max32664_priv.h` with 9 exported helper functions
- **Total reduction**: 287 lines removed, better organization

## File Organization

```
drivers/sensor/max32664/
+-- CMakeLists.txt           # All 5 files compiled together
+-- max32664.c              # Common core (1959 lines)
+   +-- I2C communication (transmit/retry/parse)
+   +-- Variant detection (AFE WHOAMI, firmware version, device tree)
+   +-- FIFO management (get count, flush)
+   +-- Status parsing (ERR0, data_rdy, overflows)
+   +-- Init sequence dispatch to variant ops
+   +-- Sensor API (sample_fetch, channel_get, attr_set)
+-- max32664_priv.h         # Private API (241 lines)
+   +-- max32664_variant_ops structure definition
+   +-- extern declarations for variant ops (a/b/c/d)
+   +-- 9 exported helper function prototypes
+-- max32664a.c             # Variant A: MaximFast (110 lines)
+   +-- MAX30101 AFE enable
+   +-- MaximFast algorithm enable
+   +-- AGC configuration (75% target, 10% step)
+-- max32664b.c             # Variant B: WHRM (120 lines)
+   +-- MAX86141 AFE enable
+   +-- KX-122 accelerometer enable (MANDATORY)
+   +-- WHRM algorithm enable
+   +-- Motion rejection configuration (0x02)
+-- max32664c.c             # Variant C: WHRM+WSpO2 (252 lines)
+   +-- MAX86141 AFE enable
+   +-- KX-122 accelerometer enable (MANDATORY)
+   +-- WHRM algorithm enable
+   +-- FIFO parsing (16-byte: HR, SpO2, accel XYZ, PPG)
+   +-- Variant-specific parse_fifo implementation
+-- max32664d.c             # Variant D: BPT (130 lines)
+   +-- MAX30101 AFE enable
+   +-- BPT algorithm enable
+   +-- AGC configuration (70% target, 12% step)
+   +-- Calibration reminder logic
+-- max32664_algo.c         # Algorithm wrapper (shared)
```

## Variant Operations Structure

### Function Pointer Dispatch

```c
/* max32664_priv.h */
struct max32664_variant_ops {
	/* Variant-specific initialization */
	int (*init)(const struct device *dev);
	
	/* FIFO data parsing (variant C has custom parsing) */
	int (*parse_fifo)(const struct device *dev, const uint8_t *fifo_data, size_t len);
	
	/* Expected FIFO entry size (bytes) */
	size_t (*fifo_entry_size)(void);
	
	/* Variant name string */
	const char *(*name)(void);
};

/* Exported variant ops structures */
extern const struct max32664_variant_ops max32664a_ops;
extern const struct max32664_variant_ops max32664b_ops;
extern const struct max32664_variant_ops max32664c_ops;
extern const struct max32664_variant_ops max32664d_ops;
```

### Dispatch in Main Driver

```c
/* max32664.c */
const struct max32664_variant_ops *max32664_get_variant_ops(enum max32664_variant variant)
{
	switch (variant) {
	case MAX32664_VARIANT_A:
		return &max32664a_ops;
	case MAX32664_VARIANT_B:
		return &max32664b_ops;
	case MAX32664_VARIANT_C:
		return &max32664c_ops;
	case MAX32664_VARIANT_D:
		return &max32664d_ops;
	default:
		return NULL;
	}
}

/* In init function */
const struct max32664_variant_ops *ops = max32664_get_variant_ops(data->variant);
if (ops && ops->init) {
	ret = ops->init(dev);  /* Dispatches to variant-specific init */
}
```

## Variant Details

### Variant A: MaximFast Algorithm
- **Sensor**: MAX30101 PPG (3-LED: red/IR/green)
- **Algorithm**: MaximFast (HR + SpO2)
- **Sample Rate**: 100 Hz
- **Accelerometer**: Optional (not required)
- **FIFO Format**: 11 bytes (Status + HR[2] + HR_Conf + SpO2 + SpO2_Conf + ...)
- **AGC Settings**: 75% target, 10% step, 50% sensitivity, 10 samples
- **Use Case**: Finger-based spot measurement

### Variant B: WHRM (Heart Rate Only)
- **Sensor**: MAX86140/MAX86141 PPG (4-channel green LED)
- **Accelerometer**: KX-122/LIS2DS12 (MANDATORY - motion compensation)
- **Algorithm**: WHRM (Wrist Heart Rate Monitor)
- **Sample Rate**: 25 Hz
- **FIFO Format**: 14 bytes (Status + HR[2] + HR_Conf + Accel XYZ + ...)
- **Motion Rejection**: Standard mode (0x02)
- **Use Case**: Continuous wrist-worn heart rate monitoring

### Variant C: WHRM+WSpO2 (Heart Rate + SpO2)
- **Sensor**: MAX86141 PPG (4-channel green LED)
- **Accelerometer**: KX-122/LIS2DS12 (MANDATORY - motion compensation)
- **Algorithm**: WHRM + WSpO2 v3x
- **Sample Rate**: 25 Hz
- **FIFO Format (output mode 0x02)**: 16 bytes of WHRM fields only (HR, RR, SpO2, confidence, SCD state, etc.) — see `max32664c.c` table comment above `max32664c_parse_fifo()`.
- **Output mode 0x03 (optional, Kconfig)**: Host may request a larger read (`CONFIG_MAX32664_HUB_MODE03_FRAME_BYTES`, default 34). The parser slides a 16-byte WHRM sanity detector across the payload; GWEC 30.2.x often does not match a fixed "18 B sensor + 16 B algo" layout, in which case the driver forces mode **0x02** for the rest of the sampling session after the first unparseable frame.
- **Custom Parsing**: `max32664c_parse_fifo()`; optional LED self-test (`CONFIG_MAX32664_LED_SELF_TEST`) runs before WHRM owns the MAX86141 LED registers.
- **Use Case**: Continuous wrist-worn HR + SpO2 monitoring

### Variant D: BPT (Blood Pressure Tracking)
- **Sensor**: MAX30101 PPG (3-LED: red/IR/green)
- **Algorithm**: BPT (Blood Pressure Tracking)
- **Sample Rate**: 100 Hz
- **Calibration**: REQUIRED (user must calibrate with reference BP cuff)
- **FIFO Format**: 13 bytes (Status + SYS[2] + DIA[2] + HR[2] + SpO2 + Conf + ...)
- **AGC Settings**: 70% target, 12% step, 50% sensitivity, 10 samples
- **Use Case**: Blood pressure estimation (after calibration)
- **Warning**: BP measurements invalid until calibration completed

## Exported Helper Functions (Private API)

The following 9 functions are exported from `max32664.c` for use by variant files (NO static keywords):

```c
/* I2C Communication */
int max32664_i2c_transmit(const struct device *dev, uint8_t *tx_buf, size_t tx_len,
                          uint8_t *rx_buf, size_t rx_len, int timeout_ms);
int max32664_i2c_transmit_retry(const struct device *dev, uint8_t *tx_buf, size_t tx_len,
                                 uint8_t *rx_buf, size_t rx_len, int timeout_ms, int retries);

/* Status & FIFO */
int max32664_get_hub_status(const struct device *dev, uint8_t *status);
void max32664_parse_status(uint8_t status, bool *err0, bool *data_rdy,
                             bool *fifo_out_ovr, bool *fifo_in_ovr, bool *host_accel_uf);
int max32664_get_fifo_count(const struct device *dev, uint8_t *count);

/* Configuration */
int max32664_configure_agc(const struct device *dev, uint8_t percent_target,
                            uint8_t percent_step_size, uint8_t sensitivity, uint8_t avg_samples);
int max32664_configure_whrm_mode(const struct device *dev, uint8_t mode);

/* Register Access */
int max32664_read_sensor_register(const struct device *dev, uint8_t sensor_idx,
                                   uint8_t reg_addr, uint8_t *reg_value);
int max32664_read_sensor_attribute(const struct device *dev, uint8_t sensor_idx,
                                     uint8_t attr_idx, uint8_t *attr_value);
```

## Naming Convention

### Primary Naming: Variant A/B/C/D
Use these in **all code**:
- `MAX32664_VARIANT_A` / `MAX32664_VARIANT_B` / `MAX32664_VARIANT_C` / `MAX32664_VARIANT_D`
- "Variant A", "Variant B", "Variant C", "Variant D" in logs and comments
- File names: `max32664a.c`, `max32664b.c`, `max32664c.c`, `max32664d.c`

### Hardware Alias Names (Reference)
Use in documentation only for hardware identification:
- GWEC = Variant C hardware (MAX86141 + KX-122)
- GWED = Variant D hardware (MAX30101 + VCNL3040)
- Example: "MAX32664GWEC (Variant C)" or "Variant C (alias: GWEC)"

### Device Tree Compatible Strings
- `maxim,max32664-gwec` → Auto-detects as Variant C
- `maxim,max32664-gwed` → Auto-detects as Variant D
- `maxim,max32664` → Generic, runtime detection via AFE WHOAMI

## Variant Detection Sequence

The driver uses multi-stage fallback detection:

1. **Device Tree Compatible** (Highest Priority)
   - Check compatible string: `gwec` → Variant C, `gwed` → Variant D
   - Always correct if explicitly specified

2. **AFE WHOAMI Register** (Family 0x41, Index 0x00, Reg 0xFF)
   - 0x25 = MAX86141 → Variant B/C
   - 0x1E = MAX30101 → Variant A/D
   - Requires firmware support

3. **Firmware Version + Accelerometer Check**
   - Variant C: MAX86141 + Accelerometer present (check via 0x41, 0x04, 0x0F → 0x43)
   - Variant B: MAX86141 + No SpO2 support
   - Variant D: MAX30101 + BPT algorithm supported
   - Variant A: MAX30101 + No BPT algorithm

4. **I2C Address Heuristic** (Fallback)
   - 0x68 → Likely Variant C (GWEC common address)
   - 0x55 → Default, check other methods
   - 0x66 → Likely Variant D (GWED common address)

## Migration from GWEC/GWED Naming

### What Changed (Jan 2026 Refactor)
- ✓ Removed `MAX32664_VARIANT_GWEC` and `MAX32664_VARIANT_GWED` enum values
- ✓ Updated all code logic to use `MAX32664_VARIANT_A/B/C/D`
- ✓ Split monolithic driver into 5 files (core + 4 variants)
- ✓ Removed 400+ lines of duplicate variant initialization code
- ✓ Removed static keywords from 9 exported helper functions
- ✓ Updated comments to use "Variant" as primary naming

### What Stayed
- ✓ Device tree compatible strings: `gwec`/`gwed` (for backward compatibility)
- ✓ Documentation references to GWEC/GWED hardware names as aliases
- ✓ I2C addresses: 0x68 (Variant C), 0x66 (Variant D), 0x55 (default)

## Usage Examples

### Application Code
```c
/* Get variant-specific sensor data */
struct sensor_value hr, spo2;
sensor_channel_get(hub_dev, SENSOR_CHAN_MAX32664_HR, &hr);
sensor_channel_get(hub_dev, SENSOR_CHAN_MAX32664_SPO2, &spo2);

/* Check variant at runtime */
struct max32664_data *data = hub_dev->data;
switch (data->variant) {
case MAX32664_VARIANT_A:
	LOG_INF("MaximFast algorithm (finger PPG)");
	break;
case MAX32664_VARIANT_C:
	LOG_INF("WHRM+WSpO2 algorithm (wrist PPG + accel)");
	break;
default:
	LOG_WRN("Unknown variant");
}
```

### Device Tree Configuration
```dts
/* Variant C (WHRM+WSpO2) */
max32664_wrist: max32664@55 {
	compatible = "maxim,max32664-gwec";  /* Explicitly Variant C */
	reg = <0x55>;
	reset-gpios = <&gpio0 20 GPIO_ACTIVE_LOW>;
	interrupt-gpios = <&gpio1 6 GPIO_ACTIVE_LOW>;
	poll-interval-ms = <200>;
};

/* Variant D (BPT) */
max32664_finger: max32664@55 {
	compatible = "maxim,max32664-gwed";  /* Explicitly Variant D */
	reg = <0x55>;
	reset-gpios = <&gpio0 20 GPIO_ACTIVE_LOW>;
	interrupt-gpios = <&gpio1 6 GPIO_ACTIVE_LOW>;
};
```

## Build Configuration

### CMakeLists.txt
```cmake
zephyr_library_sources_ifdef(CONFIG_MAX32664
	max32664.c      # Common core
	max32664a.c     # Variant A
	max32664b.c     # Variant B
	max32664c.c     # Variant C
	max32664d.c     # Variant D
	max32664_algo.c # Algorithm wrapper
)
```

### prj.conf
```conf
CONFIG_MAX32664=y
CONFIG_MAX32664_POLLING_INTERVAL_MS=200
CONFIG_MAX32664_ENABLE_INTERRUPT_MODE=y
```

## Benefits of New Architecture

1. **Maintainability**: Each variant in separate file (~100-250 lines vs 2000+ monolithic)
2. **Extensibility**: Add new variants by creating `max32664e.c` + ops structure
3. **Code Reuse**: Common I2C/FIFO/detection logic shared via helper functions
4. **Clarity**: Function pointer dispatch makes control flow explicit
5. **Testing**: Each variant can be tested independently
6. **Compile Time**: Parallel builds possible with separate files

## Related Documentation

- **Driver Implementation**: `drivers/sensor/max32664/max32664.c`
- **Private API**: `drivers/sensor/max32664/max32664_priv.h`
- **Public API**: `include/zephyr/drivers/sensor/max32664.h`
- **Device Tree Bindings**: `dts/bindings/sensor/maxim,max32664-gwec.yaml`
- **Implementation Status**: `docs/sensors/SENSOR_STATUS.md`
- **Project Standards**: repository coding and review standards
- **Main Instructions**: `.github/copilot-instructions.md`

---

**For AI Agents**: This document is the authoritative reference for MAX32664 architecture. Always reference this when working with MAX32664 driver code.

