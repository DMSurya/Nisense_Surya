# Glucose Logging Changes (historical)

**Status**: Historical only. Superseded for operations by
[`GLUCOSE_DATA_LOGGING.md`](GLUCOSE_DATA_LOGGING.md).

Earlier revisions documented:

- Multi-file then single-file FatFS `/NAND:/glucose.csv` (38 columns)
- USB MSC host copy of CSV logs
- Streaming matrix CSV lookup from FAT

**Current system (2026-07):**

- Measurement persistence → NOR `record_store` (summary + `GLUCOSE_RAW` chunks)
- Export → CDC `nisense rec export` or BLE `…def4` → phone CSV
- Matrices → packed XIP glucose-model partitions (not FAT)

See [`STORAGE_NOR_RECORD_STORE.md`](../architecture/STORAGE_NOR_RECORD_STORE.md).
