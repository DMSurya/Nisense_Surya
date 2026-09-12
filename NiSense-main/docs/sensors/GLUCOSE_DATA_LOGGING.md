# Glucose Data Logging

**Last updated**: 2026-07-20  
**Status**: Current (NOR hybrid records; FatFS CSV removed)

## Overview

Each completed glucose measurement appends:

1. One **`RECORD_TYPE_GLUCOSE`** summary (clinical result + full algorithm /
   profile intermediates).
2. Zero or more **`RECORD_TYPE_GLUCOSE_RAW`** chunks (ADC + mV×10) with
   `parent_id` = summary `record_id`.

Implementation: [`src/sensors/glucose_logger.c`](../../src/sensors/glucose_logger.c),
schema in [`include/record_store.h`](../../include/record_store.h).

There is **no** on-device `/NAND:/glucose.csv`. Export is:

- USB CDC: `nisense rec export <summary_id>`
- BLE record sync (`…def4`) → mobile `RecordCsvExporter` → `HCM_Logs/*_Glucose.csv` (+ `_Glucose_Raw.csv`)

Architecture detail: [`STORAGE_NOR_RECORD_STORE.md`](../architecture/STORAGE_NOR_RECORD_STORE.md).

---

## Summary payload (`struct rec_glucose`)

| Group | Fields |
|-------|--------|
| Identity | `timestamp`, `device_id`, `glucose_mg_dl`, `quality`, `variant`, `model_version` |
| Profile | `intercept`, `outlier_k` (from active watch/pulse profile) |
| Stage intermediates | `tot_coeff`, `y1_value`, `avg_val`, `std_dev`, `up_lim`, `ll_lim`, `p_count`, `n_count`, `p_val`, `n_val`, `p_plus_n`, `y2_val`, `y2_percent`, `group_cd`, `y2_factor`, `y2_factor_val`, `const_val`, `y3_value`, `y3_row_no`, `elim_per`, `elim_val`, `y_value` |
| HOMA path | `calibration_factor`, `ag_adjusted`, `normalized_glucose`, `actual_insulin`, `insulin_correction`, `insulin_ratio`, `inverse_ratio`, `homa_ir_index` |

Sources at measure time: `glucose_algorithm_details` +
`glucose_algorithm_get_active_profile()` (+ XIP model version when enabled).

## Raw chunks (`RECORD_TYPE_GLUCOSE_RAW`)

Header: `parent_id`, `total_samples`, `chunk_index`, `chunk_count`, `n_in_chunk`.  
Per sample: `uint16` ADC, `int16` voltage_mV × 10.

## CSV export columns

### `*_Glucose.csv` (one row per summary)

`Timestamp_unix`, `Patient`, `Device_ID`, `Glucose_mg_dL`, `Quality`, `Variant`,
`Model_Version`, then algo/profile floats listed above (shell/mobile share the
same header set). No separate Date/Time columns.

### `*_Glucose_Raw.csv`

`Sample_Index`, `ADC`, `Voltage_mV`, `parent_id`

## Host-side live session logs

While BLE-connected, the mobile `SessionLogger` may still append a thinner
vitals/glucose row stream under `HCM_Logs/` for the live session. That is
independent of the NOR bulk pull path used for full analytics / AI.

## Related

- [`GLUCOSE_MATRIX_FILE_LOADING.md`](GLUCOSE_MATRIX_FILE_LOADING.md) — XIP models (not FAT CSVs at runtime)
- [`model/glucose/README.md`](../../model/glucose/README.md) — pack & flash matrices
- [`STORAGE_NOR_RECORD_STORE.md`](../architecture/STORAGE_NOR_RECORD_STORE.md)
- Historical note: [`GLUCOSE_LOGGING_CHANGES.md`](GLUCOSE_LOGGING_CHANGES.md)
