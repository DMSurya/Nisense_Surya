# Wearable Glucose Matrices (MAX32664 variant C)

Elimination matrices for the **WEARABLE** Svasth profile (wrist-worn, 1550 nm NIRS path).

**Runtime:** packed into the QSPI glucose-model XIP image (not read as
`/NAND:/…` CSVs). Source files in this folder are packed/flashed via
`scripts/flash/flash_glucose_model.ps1` or BLE model transfer.

## Files

| File | Rows | Percentage band (derived_y/Y1 × 100) |
|------|------|--------------------------------------|
| `group_1.csv` | 7 | −0.1000% … 0.0001% |
| `group_2.csv` | 629 | 0.0001% … 19.0000% |
| `group_3.csv` | 1711 | 19.0001% … 20.9999% |
| `group_4.csv` | 1808 | 21.0000% … 24.9999% |
| `group_5.csv` | 1437 | 25.0000% … 39.9999% |
| `group_6.csv` | 1395 | 40.0000% … 60.9999% |
| `group_7.csv` | 2685 | 61.0000% … 70.9999% |
| `group_8.csv` | 1326 | 71.0000% … 79.9999% |
| `group_9.csv` | 1362 | 80.0000% … 91.9999% |
| `group_10.csv` | 716 | 92.0000% … 121.9999% |
| `group_11.csv` | 82 | 122.0000% … 299.9999% |

**Total:** 13,158 data rows across 11 files.

## Profile constants

From `glucose_algorithm_profiles.c`:

- Intercept: **161.832**
- Outlier K-factor: **1.75**
- `svasth_coeff[80]` — wearable-specific regression weights

## CSV format

```csv
lower_bound,upper_bound,correction
```

See [../README.md](../README.md) for generation, deployment, and algorithm overview.
