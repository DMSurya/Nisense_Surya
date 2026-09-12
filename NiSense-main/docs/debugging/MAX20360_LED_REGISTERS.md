# MAX20360 LED Control Register Analysis

**Last Updated**: 2026-02-08  
**Status**: Reference document (register map). LED write boundary testing is complete.

## Overview
This document provides the complete LED control register specifications for the MAX20360 PMIC, extracted from the official datasheet.

## LED Register Map

| Register | Address | Name | Function |
|----------|---------|------|----------|
| LEDCommon | 0x78 | LED Common Control | Boost loop control, open detection, current step |
| LED0Ref | 0x79 | LED0 Reference | LED0 dropout regulation voltage |
| LED0Ctr | 0x7A | LED0 Control | LED0 enable and current setting |
| LED1Ctr | 0x7B | LED1 Control | LED1 enable and current setting |
| LED2Ctr | 0x7C | LED2 Control | LED2 enable and current setting |

## Detailed Register Descriptions

### LEDCommon (0x78)
```
BIT     7        6   5   4   3   2   1   0
Field   LED_BoostLoop  -   -   LED_Open[2:0]   LEDIStep[1:0]
Access  Write,Read     -   -   Read Only       Write,Read
```

**LED_BoostLoop (Bit 7)**: Boost/LED0 closed-loop operation control
- 0: Boost voltage unrelated to LED0 dropout voltage
- 1: Boost voltage increased to adjust LED0 dropout voltage (max +5V)

**LED_Open[4:2] (Read Only)**: LEDx open detection
- Bit 2: LED2 open detection (1 = VLED2 ≥ VLED_DET)
- Bit 3: LED1 open detection (1 = VLED1 ≥ VLED_DET)  
- Bit 4: LED0 open detection (1 = VLED0 ≥ VLED_DET)

**LEDIStep[1:0]**: LED current step-size control
- 00: 0.6mA steps
- 01: 1.0mA steps
- 10: 1.2mA steps
- 11: RESERVED

### LED0Ref (0x79)
```
BIT     7   6   5   4   3   2   1         0
Field   -   -   -   -   -   -   LED0_REFSEL[1:0]
Access  -   -   -   -   -   -   Write,Read
```

**LED0_REFSEL[1:0]**: LED0 dropout regulation voltage (valid only if LED_BoostLoop = 1)
- 00: 0.2V
- 01: 0.3V
- 10: 0.4V
- 11: 0.5V

### LED0Ctr (0x7A) - PRIMARY TEST REGISTER
```
BIT     7     6     5     4   3   2   1   0
Field   LED0En[2:0]       LED0ISet[4:0]
Access  Write,Read        Write,Read
```

**LED0En[7:5]**: LED0 driver enable control
- 000: Off
- 001: LED0 On ← **PRIMARY TEST VALUE**
- 010: Controlled by internal charger status signal
- 011: Controlled by MPC3
- 100: Controlled by MPC4
- 101: Controlled by MPC5
- 110: Controlled by MPC6
- 111: Controlled by MPC7

**LED0ISet[4:0]**: LED0 current setting
Current = (LED0ISet + 1) × LEDIStep

Examples with LEDIStep = 00 (0.6mA steps):
- 00000: 0.6mA ← **MINIMUM SAFE CURRENT**
- 00001: 1.2mA
- 00010: 1.8mA
- ...
- 11000: 15.0mA (maximum)

### LED1Ctr (0x7B) and LED2Ctr (0x7C)
Same bit layout as LED0Ctr, controlling LED1 and LED2 respectively.

## Test Implementation

### Current Test Strategy
Using LED0Ctr (0x7A) for write boundary testing:

1. **LED ON**: Write 0x20 to 0x7A
   - LED0En = 001 (LED0 On)
   - LED0ISet = 00000 (0.6mA minimum current)

2. **LED OFF**: Write 0x00 to 0x7A
   - LED0En = 000 (Off)
   - LED0ISet = 00000

### Register Value Calculations

**LED ON (0x20)**:
```
Bit:    7  6  5  4  3  2  1  0
Value:  0  0  1  0  0  0  0  0
Field:  |LED0En=001| |LED0ISet=00000|
Result: LED0 enabled, 0.6mA current
```

**LED OFF (0x00)**:
```
Bit:    7  6  5  4  3  2  1  0  
Value:  0  0  0  0  0  0  0  0
Field:  |LED0En=000| |LED0ISet=00000|
Result: LED0 disabled
```

## Test Significance

This LED control test is **CRITICAL** because:

1. **Write Boundary Test**: First write operation to determine if PMIC accepts ANY control register writes
2. **Safe Operation**: LED control is typically less protected than power rail control
3. **Visual Feedback**: LED operation provides immediate visual confirmation
4. **Isolated Test**: LED control is independent of Buck/LDO regulator protection mechanisms

## Expected Test Outcomes

### If LED Test Succeeds (Status 0xFF)
- PMIC accepts some write operations
- Issue is specific to regulator control registers (Buck/LDO)
- Hardware and I2C communication are functional
- Focus investigation on regulator-specific write protection

### If LED Test Fails (Status 0xC1)
- ALL PMIC control writes are blocked
- Fundamental write protection or hardware issue
- Need to investigate PMIC unlock sequences
- Possible I2C timing or electrical issues

## Hardware Notes

- LED outputs: LED0, LED1, LED2
- Current range: 0.6mA to 30.0mA (depending on step size)
- Supply voltage: Typically from boost converter
- Protection: Open circuit detection available
