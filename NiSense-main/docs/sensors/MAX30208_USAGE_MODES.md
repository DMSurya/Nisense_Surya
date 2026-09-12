# MAX30208 Temperature Sensor - Usage Modes

**Last Updated**: 2026-02-08

> **Note**: The project uses a unified `MAX302xx` driver (`CONFIG_MAX302XX=y`) that auto-detects
> MAX30205/MAX30208/MAX30210 at runtime. The Kconfig names below (e.g., `CONFIG_MAX30208`) are
> illustrative of the MAX30208-specific features; actual configuration uses `CONFIG_MAX302XX`.
> Compatible string in device tree: uses the unified driver.

## Overview

The MAX30208 driver supports two operating modes:
1. **Synchronous (Polling)** - Default, simple blocking mode
2. **Asynchronous (Interrupt-driven)** - Power-efficient, non-blocking mode

Both modes implement the complete datasheet-specified workflow using FIFO registers and STATUS polling.

---

## Mode 1: SYNCHRONOUS (Polling)

### How It Works
```
+-----------------------------------------+
+  1. Trigger Conversion                  +
+     sensor_sample_fetch() called        +
+     - Write 0xC1 to TEMP_SETUP (0x14)   +
+--------------+--------------------------+
               →
+-----------------------------------------+
+  2. Poll STATUS Register                +
+     - Read STATUS (0x00) every 5ms      +
+     - Wait for TEMP_RDY bit = 1         +
+     - Timeout: 100ms                    +
+--------------+--------------------------+
               →
+-----------------------------------------+
+  3. Read FIFO Data Count                +
+     - Read FIFO_DATA_COUNT (0x07)       +
+     - Verify samples available          +
+--------------+--------------------------+
               →
+-----------------------------------------+
+  4. Read Temperature from FIFO          +
+     - Burst read 2 bytes from           +
+       FIFO_DATA (0x08)                  +
+     - MSB = T[15:8], LSB = T[7:0]       +
+--------------+--------------------------+
               →
+-----------------------------------------+
+  5. Return Data                         +
+     sensor_channel_get() returns value  +
+     - temp_C = temp_raw * 0.005         +
+-----------------------------------------+
```

### Usage Example
```c
#include <zephyr/drivers/sensor.h>

void read_temperature_sync(const struct device *temp_sensor)
{
    struct sensor_value temp_val;
    int ret;
    
    /* Trigger conversion and wait for completion (blocks ~15-50ms) */
    ret = sensor_sample_fetch(temp_sensor);
    if (ret < 0) {
        LOG_ERR("Failed to fetch temperature: %d", ret);
        return;
    }
    
    /* Get temperature value from cache */
    ret = sensor_channel_get(temp_sensor, SENSOR_CHAN_AMBIENT_TEMP, &temp_val);
    if (ret < 0) {
        LOG_ERR("Failed to get temperature: %d", ret);
        return;
    }
    
    /* Convert to integer representation (newlib-nano does not support %f) */
    int temp_int = temp_val.val1;
    int temp_frac = temp_val.val2 / 10000;  /* 2 decimal places */
    LOG_INF("Temperature: %d.%02d°C", temp_int, temp_frac < 0 ? -temp_frac : temp_frac);
}
```

> **⚠ newlib-nano caveat**: Do NOT use `%f` in `LOG_INF()` — newlib-nano does not support
> floating-point printf. Use fixed-point integer formatting as shown above.

### When to Use
- ✓ Simple applications without strict power requirements
- ✓ Polling-based architectures
- ✓ No interrupt GPIO available
- ✗ Battery-powered devices (wastes power during polling)
- ✗ Real-time systems (blocks for 15-50ms)

---

## Mode 2: ASYNCHRONOUS (Interrupt-Driven)

### How It Works
```
+-----------------------------------------+
+  1. Setup (Once at Startup)             +
+     sensor_trigger_set() called         +
+     - Register callback handler         +
+     - Enable TEMP_RDY interrupt (0x01)  +
+     - Configure GPIO0 as INTB (0x20)    +
+--------------+--------------------------+
               →
+-----------------------------------------+
+  2. Trigger Conversion                  +
+     sensor_sample_fetch() called        +
+     - Write 0xC1 to TEMP_SETUP (0x14)   +
+     - RETURN IMMEDIATELY (non-blocking) +
+--------------+--------------------------+
               →
+-----------------------------------------+
+  3. MCU Can Sleep or Do Other Work      +
+     - Conversion runs in background     +
+     - Typical: 15-50ms                  +
+--------------+--------------------------+
               →
+-----------------------------------------+
+  4. Interrupt Fires (GPIO0 → Low)       +
+     - MAX30208 pulls GPIO0 low          +
+     - nRF52840 detects falling edge     +
+--------------+--------------------------+
               →
+-----------------------------------------+
+  5. ISR → Work Queue                    +
+     - Read STATUS (0x00) to clear IRQ   +
+     - Verify TEMP_RDY bit = 1           +
+--------------+--------------------------+
               →
+-----------------------------------------+
+  6. Read FIFO Data                      +
+     - Check FIFO_DATA_COUNT (0x07)      +
+     - Read FIFO_DATA (0x08) - 2 bytes   +
+--------------+--------------------------+
               →
+-----------------------------------------+
+  7. Call User Handler                   +
+     your_callback(dev, trigger) called  +
+     - Updated temp_raw in driver data   +
+     - Call sensor_channel_get() to read +
+-----------------------------------------+
```

### Usage Example
```c
#include <zephyr/drivers/sensor.h>

/* Callback invoked when temperature conversion completes */
static void temp_ready_callback(const struct device *dev,
                                const struct sensor_trigger *trigger)
{
    struct sensor_value temp_val;
    int ret;
    
    /* Data already read by interrupt handler - just get it */
    ret = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_val);
    if (ret < 0) {
        LOG_ERR("Failed to get temperature: %d", ret);
        return;
    }
    
    int temp_int = temp_val.val1;
    int temp_frac = temp_val.val2 / 10000;
    LOG_INF("Temperature (async): %d.%02d°C", temp_int, temp_frac < 0 ? -temp_frac : temp_frac);
    
    /* Update UI, send BLE notification, etc. */
    update_temperature_display(temp_c);
}

void setup_temperature_async(const struct device *temp_sensor)
{
    struct sensor_trigger trig = {
        .type = SENSOR_TRIG_DATA_READY,
        .chan = SENSOR_CHAN_AMBIENT_TEMP,
    };
    
    /* Enable interrupt mode */
    int ret = sensor_trigger_set(temp_sensor, &trig, temp_ready_callback);
    if (ret < 0) {
        LOG_ERR("Failed to set trigger: %d", ret);
        return;
    }
    
    LOG_INF("Temperature sensor configured in ASYNC mode");
}

void trigger_temperature_read_async(const struct device *temp_sensor)
{
    /* Start conversion - returns immediately */
    int ret = sensor_sample_fetch(temp_sensor);
    if (ret < 0) {
        LOG_ERR("Failed to start conversion: %d", ret);
        return;
    }
    
    /* MCU can now sleep or do other work */
    /* temp_ready_callback() will be called in 15-50ms */
}
```

### When to Use
- ✓ Battery-powered wearables (saves power)
- ✓ Low-latency systems (MCU can handle other tasks during conversion)
- ✓ Periodic sampling with sleep between readings
- ✗ No interrupt GPIO available in hardware
- ✗ Simple prototyping (adds complexity)

---

## Hardware Configuration

### Device Tree (Sync Mode - No Interrupt)
```dts
&i2c0 {
    max30208: max30208@53 {
        compatible = "maxim,max30208";
        reg = <0x53>;
        status = "okay";
        /* No int-gpios = sync mode only */
    };
};
```

### Device Tree (Async Mode - With Interrupt)
```dts
&i2c0 {
    max30208: max30208@53 {
        compatible = "maxim,max30208";
        reg = <0x53>;
        status = "okay";
        int-gpios = <&gpio1 2 GPIO_ACTIVE_LOW>;  /* P1.02 */
    };
};
```

### Kconfig (Async Mode)
```kconfig
CONFIG_MAX30208=y
CONFIG_MAX30208_TRIGGER=y  # Enable interrupt support

# Choose trigger mode:
CONFIG_MAX30208_TRIGGER_GLOBAL_THREAD=y  # Use system work queue (recommended)
# OR
CONFIG_MAX30208_TRIGGER_OWN_THREAD=y     # Dedicated thread (lower latency)
CONFIG_MAX30208_THREAD_PRIORITY=10
CONFIG_MAX30208_THREAD_STACK_SIZE=1024
```

---

## Performance Comparison

| Metric                  | Sync Mode      | Async Mode     |
|-------------------------|----------------|----------------|
| **Power Consumption**   | Higher         | Lower          |
| **Latency**             | 15-50ms block  | 0ms + callback |
| **Code Complexity**     | Simple         | Moderate       |
| **CPU During Convert**  | Polling loop   | Free           |
| **Typical Use Case**    | Desktop/AC     | Battery/Mobile |

---

## Register Access Summary

| Step | Register         | Address | Description                          | Mode  |
|------|------------------|---------|--------------------------------------|-------|
| 1    | TEMP_SETUP       | 0x14    | Write 0xC1 to trigger conversion     | Both  |
| 2    | STATUS           | 0x00    | Poll for TEMP_RDY bit (sync)         | Sync  |
| 2a   | INTERRUPT_ENABLE | 0x01    | Enable TEMP_RDYEN (async setup)      | Async |
| 2b   | GPIO_SETUP       | 0x20    | Configure GPIO0 as INTB (async)      | Async |
| 3    | STATUS           | 0x00    | Read to clear interrupt (async)      | Async |
| 4    | FIFO_DATA_COUNT  | 0x07    | Check samples available              | Both  |
| 5    | FIFO_DATA        | 0x08    | Burst read 2 bytes (MSB/LSB)         | Both  |

---

## Validation Criteria

### Sync Mode
- `sensor_sample_fetch()` should complete in 15-50ms.
- `sensor_channel_get()` should return valid temperature samples.
- Operation should be valid without interrupt GPIO configuration.
- Operation should be valid with `CONFIG_MAX30208_TRIGGER` disabled.

### Async Mode
- `sensor_trigger_set()` should accept a valid callback handler.
- `sensor_sample_fetch()` should return immediately (target <1ms).
- Callback delivery should occur within 15-50ms after conversion.
- GPIO interrupt path should trigger and deliver temperature data.
- Returning to sync mode with NULL handler should be stable.

---

## Troubleshooting

### Sync Mode Issues
**Problem**: Timeout waiting for TEMP_RDY  
**Solution**: Check I2C communication, verify address 0x53, ensure Part ID register (0xFF) returns expected value (e.g., 0x08 for MAX30208)

**Problem**: FIFO empty  
**Solution**: Increase polling timeout, check STATUS register manually

### Async Mode Issues
**Problem**: Callback never called  
**Solution**: Verify int-gpios in DT, check GPIO interrupt configuration, enable CONFIG_MAX30208_TRIGGER

**Problem**: Interrupt fires but no data  
**Solution**: Check FIFO_DATA_COUNT, verify STATUS.TEMP_RDY bit, add debug logs in work handler

**Problem**: "Interrupt GPIO not configured"  
**Solution**: Add `int-gpios = <&gpio1 2 GPIO_ACTIVE_LOW>;` to device tree

---

## Current Implementation Status

✓ **Implemented**:
- FIFO-based temperature read (registers 0x07/0x08)
- STATUS polling for TEMP_RDY bit
- GPIO interrupt configuration (0x20 setup)
- Async mode flag in driver data
- Mode switching via sensor_trigger_set()

✓ **Tested**:
- Sync mode with polling
- Part ID detection (0x30 at address 0x53)
- I2C communication

• **Open Validation Gaps**:
- Full async mode validation with interrupt GPIO
- Power consumption characterization
- Edge-triggered interrupt validation

