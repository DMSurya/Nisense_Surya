# MAX32664 Auto-Detection Implementation

**Last Updated**: 2026-02-08

> **Note**: This document describes an intermediate 2-variant (GWEC/GWED) detection approach.
> The system has since been refactored to a 4-variant (A/B/C/D) architecture with a 5-step
> detection chain. See `MAX32664_VARIANT_ARCHITECTURE.md` for the current variant model.
> for the current design.

## Summary
Consolidated MAX32664 driver to use **AFE WHOAMI register-based auto-detection** instead of device tree compatible strings. This eliminates the need for separate GWEC and GWED bindings.

## Changes Made

### 1. Driver Code (`drivers/sensor/max32664/`)

#### `max32664.h`
- **Added AFE WHOAMI constants**:
  ```c
  #define MAX32664_AFE_WHOAMI_MAX86141  0x25  /* GWEC variant */
  #define MAX32664_AFE_WHOAMI_MAX86161  0x36  /* GWEC variant (alternative) */
  #define MAX32664_AFE_WHOAMI_MAX30101  0x1E  /* GWED variant (typical) */
  ```
- **Removed `variant` field from `max32664_config` structure** - now runtime-detected only

#### `max32664.c`
- **Added `max32664_read_afe_whoami()` function** (Family 0x41, Index 0x00, Reg 0xFF):
  - Reads AFE sensor ID to determine variant
  - Returns 0x25/0x36 for MAX8614x (GWEC)
  - Returns 0x1E or other for MAX30101 (GWED)

- **Updated `max32664_init()` to auto-detect variant**:
  ```c
  /* Step 6: Auto-detect variant by reading AFE WHOAMI register */
  uint8_t afe_id;
  ret = max32664_read_afe_whoami(dev, &afe_id);
  
  /* Map AFE ID to variant */
  if (afe_id == MAX32664_AFE_WHOAMI_MAX86141 || afe_id == MAX32664_AFE_WHOAMI_MAX86161) {
      data->variant = MAX32664_VARIANT_GWEC;
      LOG_INF("AFE WHOAMI: 0x%02x (MAX8614x detected - GWEC variant)", afe_id);
  } else {
      data->variant = MAX32664_VARIANT_GWED;
      LOG_INF("AFE WHOAMI: 0x%02x (MAX30101 or other - GWED variant)", afe_id);
  }
  ```

- **Consolidated device instantiation macros**:
  - Removed separate `MAX32664_GWEC_INIT` and `MAX32664_GWED_INIT`
  - Single `MAX32664_INIT` macro without `.variant` field
  - Changed `DT_DRV_COMPAT` from `maxim_max32664_gwec` to `maxim_max32664`

### 2. Device Tree Bindings (`dts/bindings/sensor/`)

#### Created `maxim,max32664.yaml` (Unified Binding)
- **Compatible**: `"maxim,max32664"`
- **Description**: Auto-detects GWEC vs GWED via AFE WHOAMI register
- **Properties**: Same as before (reg, reset-gpios, interrupt-gpios, poll-interval-ms, etc.)
- **Documentation**: Explains MCU Type (A/B/C/D) vs Sensor Variant (GWEC/GWED) distinction

#### Previous Binding Names
- `maxim,max32664-gwec.yaml` - No longer needed
- `maxim,max32664-gwed.yaml` - No longer needed

### 3. Device Tree Overlay (`boards/`)

#### `raytac_mdbt50q_db_40_nrf52840.overlay`
- **Changed compatible** from `"maxim,max32664-gwed"` to `"maxim,max32664"`
- **Updated comments** to reflect auto-detection behavior

## Detection Logic

### MCU Type vs Sensor Variant
- **MCU Type** (Family 0xFF/0x00): Silicon revision (MAX32664A/B/C/D)
  - Read during init for informational logging
  - Does NOT determine sensor variant
  
- **Sensor Variant** (AFE WHOAMI Family 0x41/0x00/0xFF): Sensor configuration
  - MAX86141 (0x25) or MAX86161 (0x36) → **GWEC** (wrist PPG + accel)
  - MAX30101 (0x1E) or other → **GWED** (finger PPG + prox/ALS)

### Detection Sequence
1. Initialize I2C and GPIOs
2. Perform hardware reset (if RSTN connected)
3. Verify application mode (Family 0x02/0x00)
4. Read MCU Type (Family 0xFF/0x00) - informational only
5. Read firmware version (Family 0xFF/0x03)
6. **Read AFE WHOAMI** (Family 0x41/0x00/0xFF) - **determines variant**
7. Configure AFE enable command based on detected variant:
   - GWEC: Family 0x44, Index 0x03 (MAX86141)
   - GWED: Family 0x44, Index 0x00 (MAX30101)

## Benefits

### 1. Simplified Device Tree
- **One binding** instead of two separate GWEC/GWED bindings
- User doesn't need to know which sensors are connected downstream
- Same overlay works for both variants (auto-detects at runtime)

### 2. Robust Detection
- **Eliminates DT misconfiguration** - can't specify wrong variant
- AFE WHOAMI is reliable hardware-based identification
- Fails with clear error if AFE not accessible

### 3. Future-Proof
- **Easily extensible** to new AFE variants (e.g., MAX86178)
- Just add new WHOAMI constant and detection case
- No new bindings or DT changes needed

## Expected Log Output

### GWEC Variant (MAX86141)
```
MAX32664: Initializing MAX32664 hub @ 0x55...
MAX32664: Device mode: 0x00 (application mode)
MAX32664: MCU Type: 0x01 (MAX32664B silicon revision)
MAX32664: Firmware version: 30.2.2
MAX32664: AFE WHOAMI: 0x25 (MAX8614x detected - GWEC variant)
MAX32664: Variant: GWEC (wrist PPG + accelerometer)
MAX32664: Enabling MAX86141 AFE (GWEC)
MAX32664: AFE (optical sensor) enabled
```

### GWED Variant (MAX30101)
```
MAX32664: Initializing MAX32664 hub @ 0x55...
MAX32664: Device mode: 0x00 (application mode)
MAX32664: MCU Type: 0x01 (MAX32664B silicon revision)
MAX32664: Firmware version: 30.2.2
MAX32664: AFE WHOAMI: 0x1E (MAX30101 or other - GWED variant)
MAX32664: Variant: GWED (finger PPG + proximity/ALS)
MAX32664: Enabling MAX30101 AFE (GWED)
MAX32664: AFE (optical sensor) enabled
```

## Migration Guide

### For Users Updating Device Trees
**Old GWED configuration:**
```dts
max32664@55 {
    compatible = "maxim,max32664-gwed";
    reg = <0x55>;
    /* ... */
};
```

**New unified configuration:**
```dts
max32664@55 {
    compatible = "maxim,max32664";  /* Auto-detects variant */
    reg = <0x55>;
    /* ... */
};
```

**No other changes needed!** Same properties work for both variants.

## Testing

### Verification Steps
1. Build firmware with updated DT overlay
2. Flash to nRF52840
3. Check RTT logs for AFE WHOAMI detection
4. Verify correct variant detected (GWEC vs GWED)
5. Confirm AFE enables with correct index (0x03 vs 0x00)

### Expected Results
- ✓ Driver initializes without errors
- ✓ AFE WHOAMI read returns expected value (0x25 for GWEC, 0x1E for GWED)
- ✓ Variant auto-detected matches actual hardware
- ✓ AFE enable command uses correct index for detected variant
- ✓ Polling starts and data flows

## Files Modified
- `drivers/sensor/max32664/max32664.h` - Added AFE constants, removed config variant
- `drivers/sensor/max32664/max32664.c` - Added AFE detection, unified instantiation
- `dts/bindings/sensor/maxim,max32664.yaml` - New unified binding
- `boards/raytac_mdbt50q_db_40_nrf52840.overlay` - Updated compatible string

## Superseded Binding Files
- `dts/bindings/sensor/maxim,max32664-gwec.yaml`
- `dts/bindings/sensor/maxim,max32664-gwed.yaml`

