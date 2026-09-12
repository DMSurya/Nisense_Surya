# MAX32664 Driver Analysis - Executive Summary

**Analysis Date**: February 11, 2026 (updated 2026-06-14)  
**Hardware Profile**: WEARABLE (wrist) — MAX32664 hub present  
**Configuration**: CONFIG_MAX32664_MODE_RAW=y (see `conf/features/max32664.conf`)  
**Total Driver Size**: 8,799 lines across 16 files

> This driver is active only in the **WEARABLE** profile. In the **PULSE** (finger-clip)
> profile the MAX32664 hub is absent; the system falls back to the standalone MAX30102
> driver and the 1550 nm NIRS glucose circuit. See `drivers/sensor/glucose/glucose_hw_detect.c`.

---

## Key Findings

### Bottom Line
The MAX32664 driver is **well-structured and functional**, but contains:
- **~200 lines of dead code** (driver algorithms)
- **~800 lines of unused code** (HUB mode variants, if not supported)
- **Proper conditional compilation** prevents bloat when modes are disabled

✓ **No functional issues** in PASSTHROUGH mode  
⚠ **No breaking changes** needed; only optimization opportunity

### Boot log (FW 30.2.2)

Hub init may log `report period cmd returned ret=-22 status=0x01` — firmware returns `ERR_UNAVAIL_FUNC` for that AN6924 command; the driver continues. See [BOOT_LOG_GUIDE.md](../debugging/BOOT_LOG_GUIDE.md).

---

## The Three Modes Explained

### RAW Mode (ACTIVE - Current)
**Status**: Fully functional and used in production (`CONFIG_MAX32664_MODE_RAW=y`)

```
Hub firmware outputs raw sensor FIFO (Family 0x12, hub algorithms disabled)
→
nRF52840 pops raw PPG/accel samples from the hub output FIFO
→
App uses subsys/ppg_algo for HR/SpO2 calculation
```

**Key files**:
- max32664.c - Core driver
- max32664_raw.c - RAW mode: hub output FIFO handling
- max86141_ppg.c - PPG child sensor device (`CONFIG_MAX32664_RAW_PPG_SENSOR=y`)
- lis2ds12_accel.c - Accel child sensor device
- max32664_accel_feeder.c - Host-fed accel into hub input FIFO

### PASSTHROUGH Mode
**Status**: Available; used for diagnostics, not selected in the current project configuration

```
Hub acts as I2C register bridge
→
nRF52840 accesses MAX86141 & LIS2DS12 registers directly
→
Shares the ppg_algo path (max32664_passthrough.c burst reads)
```

### HUB Mode (UNSUPPORTED - Dead Code)
**Status**: Not used in this project

```
Hub runs internal algorithms (MaximFast, WHRM, WHRM+WSpO2, BPT)
→
nRF52840 reads processed results from hub
→
No driver-level algorithm needed

Files only active if CONFIG_MAX32664_MODE_HUB=y:
- max32664a.c (180 lines) - MaximFast variant
- max32664b.c (99 lines) - WHRM variant
- max32664c.c (305 lines) - WHRM+WSpO2 variant
- max32664d.c (221 lines) - BPT variant
- max32664_algo.c (170 lines) - Algorithm wrappers
```

---

## What's Dead Code?

### 1. Driver Algorithm Layer (~200 lines)

**Files**:
- `max32664_algo.c` (170 lines)
- `max32664_algo.h` (25 lines)

**Why dead**:
- Functions never called anywhere in codebase
- Replaced by `subsys/ppg_algo/` (proper Zephyr subsystem)
- Only included/used in HUB mode (unsupported)

**Functions in graveyard**:
```c
max32664_calculate_spo2()        // ← NOT CALLED
max32664_compute_heart_rate()    // ← NOT CALLED
max32664_calculate_signal_quality() // ← NOT CALLED
max32664_classify_activity()     // ← NOT CALLED
```

**Evidence**:
```bash
$ grep -r "max32664_calculate_spo2" src/
# Result: No matches

$ grep -r "max32664_calculate_spo2" subsys/
# Result: No matches (subsys has its own ppg_algo_calculate_spo2)
```

### 2. HUB Mode Variants (~800 lines)

**Files**:
- `max32664a.c` (180 lines)
- `max32664b.c` (99 lines)
- `max32664c.c` (305 lines)
- `max32664d.c` (221 lines)

**Why dead**:
- Only compiled if `CONFIG_MAX32664_MODE_HUB=y`
- HUB mode is not enabled in prj.conf
- Project uses PASSTHROUGH mode exclusively

**Current status**:
```cmake
if(CONFIG_MAX32664_MODE_HUB)  # This is FALSE (disabled)
    zephyr_library_sources(
        max32664a.c     # NOT COMPILED
        max32664b.c     # NOT COMPILED
        max32664c.c     # NOT COMPILED
        max32664d.c     # NOT COMPILED
        max32664_algo.c # NOT COMPILED
    )
endif()
```

---

## Data Flow (PASSTHROUGH Mode)

```
+-------------------------+
+   Application           +
+  (ppg.c, accel_ui.c)    + ← Uses subsys/ppg_algo for HR/SpO2
+----------+--------------+
           +
    +------+------+
    →             →
PPG Sensor   Accel Sensor
    +             +
    +           sensor_sample_fetch(accel_dev)
    +             +
    +        lis2ds12_accel_sample_fetch()
    +             +
sensor_sample_fetch(ppg_dev)    max32664_raw_read_accel()
    +             +
max86141_ppg_sample_fetch()    max32664_i2c_transmit()
    +             +
max32664_raw_read_ppg()        /
    +            /
     \          /
      \        /
       max32664_i2c_transmit()
       (Family 0x41: Read Sensor)
            +
      +-----v------+
      +   MAX32664 +  ← Hub acts as I2C bridge (no algorithms)
      +    Hub     +
      +-----+------+
            +
      +-----+------------+
      +                  +
   MAX86141 FIFO    LIS2DS12 via SPI
   (PPG data)       (Accel data)
```

**NOT in this flow**:
```
✗ max32664_algo.c functions
✗ max32664a/b/c/d.c variant initialization
✗ Hub internal algorithms
```

---

## Cleanup Recommendations

### Immediate (Phase 1) - Safe & Quick
**Remove ~200 lines that are definitely dead**

```
Delete:
  - max32664_algo.c
  - max32664_algo.h

Modify:
  - max32664.c (remove include)
  - CMakeLists.txt (remove from HUB section)

Impact: 
  - 195 lines removed
  - ~6KB binary reduction
  - No functional changes
  
Time: 30 minutes
Risk: LOW
```

### Optional (Phase 2) - If No HUB Ever Needed
**Remove ~800 lines of HUB mode support**

```
Delete:
  - max32664a.c
  - max32664b.c
  - max32664c.c
  - max32664d.c

Modify:
  - CMakeLists.txt (remove HUB conditional)
  - Kconfig (make HUB mode unavailable)
  - max32664_priv.h (remove variant ops)
  - max32664.c (simplify dispatch)

Impact:
  - 805 lines removed
  - ~20KB binary reduction
  - Simpler driver architecture
  
Time: 1 hour
Risk: LOW (if HUB will never be used)
```

### Nice to Have (Phase 3) - Polish
**Refactor remaining code for clarity**

```
Partition:
  - max32664_accel.c (separate HUB-only functions)
  - max32664_passthrough.h (remove unimplemented APIs)

Document:
  - Keep related notes under docs/sensors/ and docs/drivers/
  - Create VARIANTS.md, OPERATING_MODES.md, etc.

Time: 4 hours
Risk: MEDIUM (refactoring activities)
```

---

## Files Status Summary

| Category | Count | Lines | Example Files |
|----------|-------|-------|----------------|
| **ACTIVE** | 7 files | ~5,900 | max32664.c, max86141_ppg.c, lis2ds12_accel.c |
| **CONDITIONAL** | 3 files | ~1,000 | max32664_raw.c, max32664_passthrough.c, max32664_accel.c |
| **DEAD (Algorithm)** | 2 files | ~195 | max32664_algo.c, max32664_algo.h |
| **DEAD (HUB)** | 4 files | ~805 | max32664a/b/c/d.c |
| **TOTAL** | 16 files | ~8,799 | - |

---

## Configuration Impact

### Current Build (RAW)
```kconfig
CONFIG_MAX32664=y
CONFIG_MAX32664_MODE_RAW=y
CONFIG_MAX32664_RAW_PPG_SENSOR=y
CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER=y
```
(see `conf/features/max32664.conf` for the authoritative settings)

**Compilation result:**
```
✓ max32664.c        (3006 lines)
✓ max32664_afe.c    (287 lines)
✓ max32664_accel.c  (469 lines)
✓ max32664_raw.c    (613 lines)
✓ max32664_passthrough.c (248 lines)
✓ max86141_ppg.c    (431 lines)
✓ lis2ds12_accel.c  (461 lines)

✗ max32664_algo.c   (NOT compiled - good!)
✗ max32664a.c       (NOT compiled - good!)
✗ max32664b.c       (NOT compiled - good!)
✗ max32664c.c       (NOT compiled - good!)
✗ max32664d.c       (NOT compiled - good!)
```

---

## Architecture Quality Assessment

### ✓ What's Good
- Proper mode separation via CMakeLists.txt conditionals
- Clean dependency hierarchy (app → subsystem → drivers → hardware)
- Well-documented variant specifications
- Comprehensive device tree configuration
- PPG algorithm migrated to proper subsystem (subsys/ppg_algo)

### ⚠ What Could Improve
- Remove dead algorithm code (blocking PR readability)
- Consider removing HUB mode if not needed (binary bloat)
- Complete or remove unimplemented API functions (max32664_passthrough.h)
- Move documentation to .github (reduce inline comments)
- Add conditional guards to variant ops declarations

### What's Right-Sized
- max32664.c (large but appropriate for unified driver)
- Sensor driver wrappers (max86141_ppg.c, lis2ds12_accel.c)
- Raw mode implementation (max32664_raw.c)
- AFE and accelerometer layer (max32664_afe.c, max32664_accel.c)

---

## Quick Decision Tree

### Question 1: Will HUB mode ever be used?

**YES**: Keep everything as-is  
→ No action needed, driver is complete

**NO**: Remove HUB mode (Phase 2)  
→ Delete variants a/b/c/d; update CMakeLists, Kconfig, priv.h

**UNSURE**: Do Phase 1 only  
→ Remove algorithms (safe), keep HUB infrastructure

### Question 2: Are driver algorithms still needed?

**NO** (they're not called): Do Phase 1  
→ Remove max32664_algo.c/h immediately

**MAYBE**: Skip Phase 1, keep for reference  
→ Keep redundant code but document why

---

## Documentation References

Related detailed documentation:

| Document | Location | Purpose |
|----------|----------|---------|
| **Architecture** | `docs/sensors/MAX32664_VARIANT_ARCHITECTURE.md` | Variant model and mode map |
| **Auto Detection** | `docs/sensors/MAX32664_AUTO_DETECTION.md` | Runtime variant detection flow |
| **Firmware Update** | `docs/sensors/MAX32664_FIRMWARE_UPDATE.md` | Update workflow and constraints |
| **This Document** | `docs/sensors/MAX32664_SUMMARY.md` | Executive overview |

---

## Conclusion

The MAX32664 driver is production-ready for RAW mode and the subsystem-based algorithm path (`subsys/ppg_algo`) is the active architecture. PASSTHROUGH remains available for diagnostics.

---

**Generated**: February 11, 2026  
**Analyst**: Code Analysis Tool  
**Status**: Ready for Implementation  
**Risk Level**: ✓ LOW (Phase 1), ⚠ MEDIUM (Phase 2-3)


