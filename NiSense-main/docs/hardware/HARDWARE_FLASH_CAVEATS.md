# External Flash Memory (MX25R6435F) Hardware Caveats

## Overview
This document outlines critical hardware considerations for the Macronix MX25R6435F 64Mb QSPI NOR flash memory used in the project.

**Device**: MX25R6435F (64Mb / 8 MB)  
**Interface**: QSPI (Quad SPI)  
**Location**: External flash on MDBT50Q-DB-40 board

---

## Hardware Caveats

### 1. Pull-up Resistors: RESET#/HOLD#/WP#

**Issue**: RESET#/HOLD# and WP# should be pulled to VCC if not actively controlled by GPIO. The datasheet indicates internal pull-ups are present but disabled when physically connected.

**Current Status**: 
- ✓ **WP# (IO2)**: Connected to P0.21 via QSPI interface (not separate GPIO)
- ✓ **HOLD# (IO3)**: Connected to P1.0 via QSPI interface (not separate GPIO)
- ⚠ **RESET#**: Not explicitly configured in device tree (may not be used)

**Action Required**:
- Verify hardware schematic: Are pull-up resistors present on WP#/HOLD# pins?
- If using RESET# pin, ensure pull-up resistor is present
- Nordic QSPI driver should handle these pins automatically, but verify behavior

**Device Tree Reference**:
```dts
qspi_flash: qspi_flash {
    group1 {
        psels = <NRF_PSEL(QSPI_SCK, 0, 19)>,  /* CLK: P0.19 */
            <NRF_PSEL(QSPI_IO0, 0, 23)>,     /* IO0/MOSI: P0.23 */
            <NRF_PSEL(QSPI_IO1, 0, 22)>,     /* IO1/MISO: P0.22 */
            <NRF_PSEL(QSPI_IO2, 0, 21)>,     /* IO2/WP: P0.21 */
            <NRF_PSEL(QSPI_IO3, 1, 0)>,      /* IO3/HOLD: P1.0 */
            <NRF_PSEL(QSPI_CSN, 0, 17)>;    /* CS: P0.17 */
    };
};
```

---

### 2. SPI Timing: CS# Setup/Hold Times

**Issue**: Ensure CS# setup/hold times and SCLK frequency comply with timing specifications (e.g., tCSS0 ≥ 20 ns for CS# fall-to-SCLK-rise setup).

**Current Status**:
- ✓ **SCK Frequency**: Configured to 32 MHz in device tree (`sck-frequency = <32000000>`)
- ✓ **Timing Compliance**: Nordic QSPI driver should handle CS# timing automatically
- ⚠ **Verification Needed**: Test at maximum frequency to ensure timing margins

**Device Tree Configuration**:
```dts
mx25r64: mx25r6435f@0 {
    compatible = "nordic,qspi-nor";
    sck-frequency = <32000000>;  /* 32 MHz - verify timing compliance */
    /* ... */
};
```

**Action Required**:
- Verify CS# setup time (tCSS0 ≥ 20 ns) is met at 32 MHz
- Consider reducing frequency if timing issues occur
- Monitor for flash read/write errors that might indicate timing problems

---

### 3. Power Sequencing

**Issue**: The flash should power up after or simultaneously with the MCU. Verify PMIC (MAX20360) ramp timing.

**Current Status**:
- ⚠ **Not Explicitly Verified**: Power sequencing depends on hardware design
- ✓ **PMIC Driver**: MAX20360 regulator driver is configured and initialized
- ⚠ **Verification Needed**: Measure actual power-up timing

**Action Required**:
1. **Hardware Verification**:
   - Measure VCC flash vs VCC MCU power-up timing
   - Verify flash VCC is stable before MCU attempts QSPI access
   - Check if flash requires explicit reset sequence after power-up

2. **Software Verification**:
   - Add initialization delay if flash access fails on cold boot
   - Monitor for flash initialization errors in logs
   - Consider adding flash device ID check during startup

**Related Code**:
- PMIC initialization: `drivers/regulator/regulator_max20360.c`
- Flash access: Handled by Zephyr's `nordic,qspi-nor` driver

---

### 4. Write Cycle Timing: WIP Bit Polling

**Issue**: Page program takes ~3–10 ms; sector erase takes ~300–400 ms. Firmware must implement proper status register polling (WIP bit) before issuing new commands.

**Current Status**:
- ✓ **Zephyr Flash Driver**: Nordic QSPI NOR driver should handle WIP polling automatically
- ✓ **record_store / NVS / XIP**: App writes go through flash APIs; coordinate with `nor_qspi_busy`
- ⚠ **Verification Needed**: Verify driver properly polls WIP bit under concurrent XIP reads

**Action Required**:
1. **Verify Driver Behavior**:
   - Check that `nordic,qspi-nor` driver polls status register (WIP bit) after writes
   - Monitor flash operations for proper completion
   - Test rapid `record_store_append` under UI XIP font/icon load

2. **Timing Expectations**:
   - Page program: 3–10 ms (typical 4 ms)
   - Sector erase: 300–400 ms (typical 350 ms)
   - Block erase: ~1.5–2 s (64 KB blocks)

3. **Testing**:
   - Measure-all cycles that append summaries + raw chunks
   - Monitor for timeout errors during flash operations
   - Confirm main skips `ui_update` while `nor_qspi_busy()` is true

**Related Code**:
- Flash driver: Zephyr in-tree `drivers/flash/nor/qspi_nor.c`
- Persistence: `src/storage/record_store.c`, `include/nor_qspi_busy.h`
- Detail: [`docs/architecture/STORAGE_NOR_RECORD_STORE.md`](../architecture/STORAGE_NOR_RECORD_STORE.md)

---

### 5. Deep Power-Down (DPD)

**Issue**: To achieve 0.007 µA standby, firmware must issue DP (Deep Power-Down) command when device is idle, and RES (Release from Deep Power-Down) when needed.

**Current Status**:
- ✓ **Device Tree Configuration**: DPD is enabled in device tree
  ```dts
  has-dpd;
  t-enter-dpd = <10000>;   /* 10 ms enter delay */
  t-exit-dpd = <35000>;    /* 35 ms exit delay */
  ```
- ⚠ **Driver Support**: Verify Nordic QSPI driver implements DPD commands
- ⚠ **Application Control**: May need explicit DPD entry/exit in application code

**Action Required**:
1. **Verify Driver Support**:
   - Check if `nordic,qspi-nor` driver automatically enters DPD during sleep
   - Verify DPD exit timing (35 ms) is respected
   - Test power consumption in sleep mode

2. **Application Integration**:
   - Consider explicit DPD entry before entering deep sleep
   - Ensure DPD exit before flash access after wake-up
   - Add delays if flash access fails immediately after wake-up

3. **Power Consumption Testing**:
   - Measure current consumption in sleep mode
   - Verify 0.007 µA standby is achieved (or close to it)
   - Compare with/without DPD enabled

**Device Tree Configuration**:
```dts
mx25r64: mx25r6435f@0 {
    compatible = "nordic,qspi-nor";
    has-dpd;
    t-enter-dpd = <10000>;   /* 10 ms - time to enter DPD */
    t-exit-dpd = <35000>;    /* 35 ms - time to exit DPD */
    /* ... */
};
```

---

## Operator verification checklist

Lab/hardware owners should tick these against the EV kit / production PCB (not assumed Pass in CI):

| # | Item | Status | Notes |
|---|------|--------|-------|
| 1 | Schematic pull-ups on WP#/HOLD#/RESET# | ☐ Operator | Nordic QSPI muxes IO2/IO3; confirm board |
| 2 | Reliable QSPI @ 32 MHz (no CRC/read errors under load) | ☐ Operator | Stress with measure-all + XIP fonts |
| 3 | Flash VCC ready before first QSPI access | ☐ Operator | Scope PMIC vs MCU rails |
| 4 | WIP / busy: `record_store` + XIP concurrent use | ☐ Operator | UI skips refresh while `nor_qspi_busy` |
| 5 | DPD enter/exit if shipping low-Iq modes | ☐ Operator | Optional; measure sleep current |

Software-side: Zephyr `nordic,qspi-nor` owns WIP polling; app uses `record_store` + NVS + XIP (not LittleFS/FatFS).

---

## Related Documentation

- **MX25R6435F Datasheet**: Macronix MX25R6435F 64Mb QSPI NOR Flash
- **Nordic QSPI Driver**: Zephyr `drivers/flash/nor/qspi_nor.c`
- **Partitions**: [PARTITION_LAYOUT.md](../build/PARTITION_LAYOUT.md), `boards/raytac_overlay/51_qspi.overlayinc`
- **Persistence**: [STORAGE_NOR_RECORD_STORE.md](../architecture/STORAGE_NOR_RECORD_STORE.md)
- **PMIC / ship SEAL**: [MAX20360.md](../drivers/MAX20360.md), `regulator_max20360.c`

---

## Notes

1. **Zephyr Driver Assumptions**: Nordic QSPI NOR handles WIP/timing; verify if field failures appear.
2. **Hardware-Dependent**: Pull-ups and sequencing need schematic/scope confirmation.
3. **Testing Priority**: Power sequencing + concurrent XIP/`record_store` first.
4. **Deep Power-Down**: Optional for ultra-low Iq; disable if not needed.

---

**Last Updated**: 2026-07-21  
**Status**: Active reference — open items are operator verify, not code TODOs


