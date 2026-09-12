# MCP79xx RTC Driver (out-of-tree)

## Overview

Zephyr **RTC API** driver for Microchip MCP79xx (product part: **MCP7940N**).
Compatible: `microchip,mcp79xx`. Year register uses datasheet encoding
(years since 2000 → POSIX `tm_year` offset **100**).

Datasheet baseline: **DS20005010J** (Rev J).  
Errata: **DS80000611** (Family Silicon Errata).

## Configuration

```ini
CONFIG_COUNTER=y
CONFIG_COUNTER_MCP79XX=y
CONFIG_RTC=y
CONFIG_RTC_ALARM=y          # optional API surface; product does not arm by default
CONFIG_RTC_CALIBRATION=y
CONFIG_APP_RTC_PERIODIC_ALARM=n   # keep n — experimental only
```

Disable upstream if present: `CONFIG_COUNTER_MICROCHIP_MCP7940N=n`.

## Device tree

```dts
&i2c0 {
    mcp7940n: mcp7940n@6f {
        compatible = "microchip,mcp79xx";
        reg = <0x6f>;
        variant = "mcp7940n";
        status = "okay";
        int-gpios = <&gpio1 3 GPIO_ACTIVE_LOW>;
    };
};

/ {
    aliases {
        rtc0 = &mcp7940n;
    };
};
```

## Calibration (Zephyr-aligned)

`rtc_set_calibration` / `rtc_set_trim_ppm`:

| App / Zephyr | Meaning | OSCTRIM (Reg 5-15) |
|--------------|---------|---------------------|
| Positive ppm/ppb | Increase RTC frequency | `SIGN=1`, add clocks |
| Negative | Decrease RTC frequency | `SIGN=0`, subtract clocks |

Range: **±127** register steps. Driver maps **1 LSB = 1 ppm** (datasheet ≈1.017 ppm/LSB; ~1.7% residual accepted). BLE/storage reject values outside ±127 before persist.

## Errata handling

| # | Issue | Status |
|---|--------|--------|
| 1 | Date increment in 12h mode | Mitigated — 24h forced |
| 2 | Spurious min/hour alarm with trim (SIGN=0) | **Deferred (Track B)** — do not enable product HW alarms without verify-on-IRQ |
| 3 | Date change on month/year write | Second Date write after D→M→Y burst |
| 4 | Weekday change after osc restart | Read-back/rewrite RTCWKDAY after ST |

## Product decisions / known gaps (by design)

- **PWRFAIL / power-fail timestamps:** not read; any `set_time` (RTCWKDAY write) clears them per datasheet. Not a product requirement today — do not “fix” by adding silent history unless product asks.
- **BBRAM (64 B @ 0x20–0x5F):** unused; upstream BBRAM driver is for `microchip,mcp7940n`, not this compatible.
- **Square-wave / MFP clock out:** unused; MFP is IRQ input only.
- **Periodic 1-minute alarm:** UI/bring-up chrome only; default **off** (`CONFIG_APP_RTC_PERIODIC_ALARM=n`). IRQ path clears ALMxIF with error checks; **does not** dispatch `rtc_alarm*_callback` until Track B.
- **`variant` DT property:** declared, not gated in code — assumes N-series VBATEN on `set_time`.

## Track B (deferred — do not implement without requirement)

1. Dispatch IRQ to `rtc_alarm*_callback`.
2. Errata #2: on match, verify RTCMIN/RTCHOUR before callback (**hard prerequisite** for item 1).
3. Optional: gate VBATEN / features on `variant`.

## Files

```
drivers/counter/mcp79xx.c
drivers/counter/CMakeLists.txt
drivers/counter/Kconfig
dts/bindings/counter/microchip,mcp79xx.yaml
```

## Credits

- Original upstream counter driver: Peter Bigot Consulting, Laird Connectivity
- Out-of-tree RTC path / year fix / errata + calibration alignment: AARMS (2026)
- License: Apache-2.0
