# Pulse Glucose Matrices (Finger NIRS Probe)

Elimination matrices for the **PULSE** Svasth profile (finger-clip, 1550 nm IR / InGaAs + SAADC).

**Runtime:** packed into the QSPI glucose-model XIP image (not read as
`/NAND:/…` CSVs). Source files in this folder are packed/flashed via
`scripts/flash/flash_glucose_model.ps1` or BLE model transfer.

> **Note:** Pulse has **10 groups** (no `group_11.csv`). Regression coefficients in
> `glucose_algorithm_profiles.c` for groups 1–10 are interim wearable values until
> customer-supplied pulse-specific constants are integrated.

## Files

| File | Rows | Percentage band (derived_y/Y1 × 100) |
|------|------|--------------------------------------|
| `group_1.csv` | 133 | −199.9999% … −110.5000% |
| `group_2.csv` | 172 | −110.4999% … −69.9999% |
| `group_3.csv` | 280 | −69.9998% … −50.5100% |
| `group_4.csv` | 1026 | −50.5099% … −44.5000% |
| `group_5.csv` | 390 | −44.4999% … −30.0000% |
| `group_6.csv` | 80 | −29.9999% … −0.0001% |
| `group_7.csv` | 327 | 0.0001% … 40.4999% |
| `group_8.csv` | 385 | 40.5000% … 59.5999% |
| `group_9.csv` | 412 | 59.6000% … 80.4999% |
| `group_10.csv` | 183 | 80.5000% … 199.9999% |

**Total:** 3,388 data rows across 10 files.

## Profile constants

From `glucose_algorithm_profiles.c`:

- Intercept: **150.776**
- Outlier K-factor: **1.5**
- `svasth_coeff[80]` — pulse-specific regression weights

## Requirements

- Pulse matrices must be present in the **packed XIP glucose model** flashed to
  the device (primary slot) before measuring on pulse hardware.
- Profile constants come from `glucose_algorithm_profiles.c` (PULSE variant).

## CSV format

```csv
lower_bound,upper_bound,correction
```

See [../README.md](../README.md) for generation, deployment, and algorithm overview.
