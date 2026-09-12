# MAX32664 Firmware Update via I2C

**Last Updated**: 2026-02-08

## Overview

The MAX32664 firmware 40.2.2 is non-functional - all sensor commands return errors. This module provides firmware update capability via I2C bootloader protocol.

## Requirements

1. **Compatible firmware binary** (.msbl file) from Analog Devices
2. **Host-held buffer or QSPI `app` partition** for the `.msbl`
   (`include/app_data.h` — partition @ `0x726000`, ~872 KB; typical hub image ~300 KB).
   FatFS `/NAND:` is **removed**.
3. **Working I2C communication** with MAX32664

## Step 2: Place firmware on device

Program the `.msbl` into the QSPI **app** partition (or pass a RAM buffer to the
updater). Do **not** expect `/NAND:/max32664.msbl`.

## Step 3: Trigger firmware update

```c
#include "max32664_update.h"

const struct device *hub = DEVICE_DT_GET(DT_ALIAS(max32664_hub));

/* Preferred: image already in QSPI app slot */
int ret = max32664_update_firmware_from_app_partition(hub, NULL, NULL);

/* Or: host-held buffer */
/* ret = max32664_update_firmware_buf(hub, msbl, msbl_len, "MAX32664C_30.13.31.msbl", NULL, NULL); */
```

Path-based `max32664_update_firmware()` returns `-ENOTSUP` (FatFS `/NAND:` gone).

## Bootloader GPIO (AN6924)

RSTN low ≥10 ms; MFIO low ≥1 ms before RSTN rise for bootloader entry; app entry
uses MFIO high with the same RSTN timing, then ~1.8 s ready on Variant C.

## Available Firmware Versions (from Analog Devices website)

| Type | Description | Version | Hardware | Recommendation |
|------|-------------|---------|----------|----------------|
| **A** | Finger HRM & SpO2 | 10.3.0 | Finger sensor | ✗ Not for wearable |
| **B** | Wearable HRM | 20.12.0, 20.13.19 | Wrist/wearable | ⚠ No SpO2 support |
| **C** | Wearable HRM & SpO2 | 30.13.30, 30.13.31 | Wrist/wearable | ✓ **RECOMMENDED** |
| **C** | Wearable HRM & SpO2 | 32.9.23-34 | MAXM86161 + LIS | ⚠ Requires MAXM86161 |
| **D** | Finger BPT, HRM & SpO2 | 40.6.0 | Finger + BP | ✗ Avoid 40.x series |

### Current Device Firmware: 40.2.2 (DEFECTIVE)

Your device has firmware **40.2.2** which is **NOT listed on the website**. This is likely an unreleased/beta version, explaining why all commands fail. The official 40.x release is 40.6.0, but even that should be avoided based on the broken 40.2.2 behavior.

## Firmware Update Process

### Step 1: Obtain Compatible Firmware

Download from Analog Devices MAX32664 product page:

**For your hardware (MAX32664GWEC+ with MAX86141):**
- ✓ **Best choice**: Firmware C v30.13.30 or v30.13.31
  - Supports wearable applications (wrist-based sensing)
  - Full HRM + SpO2 algorithms
  - Compatible with MAX86141 PPG sensor
  - Stable, proven version

**Alternative options:**
- Firmware B v20.13.19: If you only need heart rate (no SpO2)
- Firmware D v40.6.0: **NOT recommended** (40.x series has issues)

**File format**: `.msbl` (MAX32664 Secure Bootloader format)

### Step 2: Transfer Firmware to Device

Program the `.msbl` into the QSPI **app** partition (`include/app_data.h`) or
pass a host-held buffer to `max32664_update_firmware_buf()`.

### Step 3: Trigger Firmware Update

```c
#include "max32664_update.h"

const struct device *hub = DEVICE_DT_GET(DT_ALIAS(max32664_hub));
int ret = max32664_update_firmware_from_app_partition(hub, NULL, NULL);
```

Path-based `/NAND:` update returns `-ENOTSUP`.


### Step 4: Verify New Firmware

After update, check firmware version in logs:
```
<inf> max32664: Firmware version: X.Y.Z
```

If version changed and commands work, update is successful.

## Bootloader Protocol Details

### Enter Bootloader Mode
```
Family: 0x01 (Mode Control)
Index:  0x00
Data:   0x08 (Bootloader mode)
```

### Erase Application
```
Family: 0x80 (Bootloader commands)
Index:  0x00 (Erase)
```

### Write Page
```
Family: 0x80
Index:  0x04 (Write page)
Data:   [page_num_msb, page_num_lsb, ...page_data (8KB)]
```

### Exit Bootloader
```
Family: 0x01
Index:  0x00
Data:   0x00 (Application mode)
```

## Troubleshooting

### Firmware Update Fails

1. **Check image available**: FatFS `/NAND:` is gone — confirm the host/buffer path you pass to the updater actually exists
2. **Verify I2C communication**: Hub should respond to mode read
3. **Ensure sufficient power**: Firmware write requires stable 3.3V/5V
4. **Check flash space**: Firmware is ~1MB, need at least 2MB free

### Hub Stuck in Bootloader

Manually exit:
```c
max32664_exit_bootloader(ppg_dev);
```

Or power cycle the board (hub resets to application mode).

### Invalid Firmware Binary

Symptoms:
- Update completes but hub doesn't respond
- Hub stays in bootloader mode

Solution:
- Verify firmware is for correct variant (GWEC vs GWED)
- Try different firmware version
- Contact Analog Devices for compatible binary

## API Reference

### `max32664_update_firmware()`
```c
int max32664_update_firmware(const struct device *dev, const char *firmware_path);
```
- **dev**: MAX32664 device pointer from driver
- **firmware_path**: Path to .msbl file (historical `/NAND:/…` invalid without FatFS)
- **Returns**: 0 on success, negative errno on failure

### `max32664_enter_bootloader()`
```c
int max32664_enter_bootloader(const struct device *dev);
```
Enters bootloader mode for manual firmware operations.

### `max32664_exit_bootloader()`
```c
int max32664_exit_bootloader(const struct device *dev);
```
Exits bootloader and reboots to application mode.

## Current Firmware Issues (v40.2.2)

**Your device has firmware 40.2.2 - an unreleased/beta version not found on Analog Devices website!**

Symptoms:
- ✓ I2C communication works
- ✗ AFE WHOAMI returns 0xFE (not supported)
- ✗ Output mode writes accepted but readback shows garbage (0x28)
- ✗ Algorithm enable fails (0xFE)
- ✗ AFE register write fails (0x02)
- ✗ Hub never produces data (status always 0x00)

**Root cause**: 40.2.2 is likely a pre-release/buggy version. Website shows only 40.6.0 as official 40.x release.

**Solution**: Downgrade to firmware C v30.13.30 or v30.13.31 (stable, proven versions).

## Firmware Version Compatibility

### Hardware Check
Your board: **MAX32664GWEC+ with MAX86141 PPG + LIS2DS12 accelerometer**

| Firmware | MAX86141 | MAX30101 | MAXM86161 | LIS2DS12 | Compatible? |
|----------|----------|----------|-----------|----------|-------------|
| 10.3.0 (A) | ⚠ | ⚠ | ✗ | ⚠ | Finger mode only |
| 20.13.19 (B) | ✓ | ✓ | ✗ | ✓ | Yes, HRM only |
| 30.13.30 (C) | ✓ | ✓ | ✗ | ✓ | **Yes, HRM+SpO2** |
| 32.9.34 (C) | ✗ | ✗ | ✓ | ✓ | No, needs MAXM86161 |
| 40.6.0 (D) | ⚠ | ⚠ | ✗ | ⚠ | Unknown, avoid |

## Recommended Firmware

Based on your hardware and community reports:
- **Primary choice**: **v30.13.30 or v30.13.31** (Firmware C - Wearable HRM&SpO2)
  - Stable, full feature support
  - Proven compatibility with MAX86141
  - Both heart rate and SpO2 algorithms
  - Optimized for wrist-based sensing
  
- **Alternative**: v20.13.19 (Firmware B - Wearable HRM)
  - Use if SpO2 is not required
  - Slightly lower memory footprint
  
- **Avoid**: v40.x series
  - Your 40.2.2 is completely broken
  - 40.6.0 unknown stability
  - BPT (blood pressure) feature not needed for basic HRM/SpO2

**Download**: Visit MAX32664 product page → Software & Models → Filter by firmware type C

