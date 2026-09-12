# Glucose Algorithm Model Files (Svasth)

Elimination-correction matrices for the Svasth non-invasive glucose regression pipeline.
Matrices are **profile-specific**. Firmware selects Watch vs Pulse via
`product_hw_detect()`, then binds the matching entry inside the clinical
**model pack** on NOR (`model_pack.bin` → `model-primary` / `model-secondary`).

## Directory layout (source CSVs in repo)

```
model/glucose/
├── README.md
├── wearable/           # Watch (MAX32664C) — 11 groups
└── pulse/              # Pulse — 10 groups
```

At runtime the device does **not** read CSVs from FatFS. Pack with:

```bash
python scripts/tools/pack_glucose_model.py --pack-all \
  --model-dir model/glucose --output build/model_pack.bin
```

| Slot | Offset | Size | Role |
|------|--------|------|------|
| `model-primary` | `0x1F6000` | 512 KB | Active pack (XIP) |
| `model-secondary` | `0x276000` | 512 KB | BLE / flash staging |

See [PARTITION_LAYOUT.md](../../docs/build/PARTITION_LAYOUT.md).


## Svasth algorithm (summary)

Implementation: `drivers/sensor/glucose/glucose_algorithm.c` (`glucose_intercept()`).

1. **Y1** — 80-point ADC vector × profile-specific `svasth_coeff[]`, plus profile intercept.
2. **Outlier rejection** — K-sigma on the weighted sum (`outlier_k_factor`: 1.75 wearable, 1.5 pulse).
3. **Derived percentage** — `(derived_y / Y1) × 100%` selects a group from `glucose_algorithm_profiles.c`.
4. **Y2/Y3** — Group linear regression → Y3 estimate.
5. **Elimination lookup** — Match `[lower_bound, upper_bound]` in the active group's packed matrix; apply `correction`.
6. **HOMA-IR** — Secondary metrics (`insulin_resistance_calculate()`); fasting insulin defaults to 10.0 μIU/mL.

**Profile constants** (`glucose_algorithm_profiles.c`):

| Field | WEARABLE | PULSE |
|-------|----------|-------|
| Intercept | 161.832 | 150.776 |
| K-factor | 1.75 | 1.5 |
| Matrix groups | 11 | 10 |
| ADC samples | 80 | 80 |

PPG vitals are **not** algorithm inputs; glucose runs after PPG in the health cycle to reduce optical crosstalk.

## CSV format (source)

Each file: header + data rows.

```csv
lower_bound,upper_bound,correction
1.000000,127.425600,-19.986400
128.425600,128.893800,-11.555100
```

- `lower_bound` / `upper_bound` — glucose value range for elimination match (`lower < Y3 < upper`)
- `correction` — percent correction applied to the Y3 estimate

## Generation / packing

```bash
python scripts/convert_glucose_matrices.py --variant wearable
python scripts/convert_glucose_matrices.py --variant pulse
# Pack for QSPI (see scripts/tools/pack_glucose_model.py)
```

## Runtime build mode

Production uses **XIP packed models** (`CONFIG_GLUCOSE_MATRICES_FROM_XIP`, default
on in Kconfig; see `conf/features/glucose.conf`).

Legacy embedded `.rodata` matrices and FatFS CSV streaming are no longer the
product path.

## Deployment

### J-Link / flash script (manufacturing & lab)

```powershell
# From repo root — pack + program primary (and optionally secondary)
.\scripts\flash\flash_glucose_model.ps1
```

### BLE OTA model update

Service `…def3` (`ble_model_transfer`) streams into the secondary slot, validates,
then activates. Mobile: Server page → Update model (role-gated).

> **Retired:** USB MSC copy to `/NAND:/model/glucose/` via
> `scripts/deploy/copy_glucose_model.ps1`.

## Measurement logging

Results go to the NOR **record store** (summary + `GLUCOSE_RAW` chunks), not
`/NAND:/glucose.csv`. See
[docs/sensors/GLUCOSE_DATA_LOGGING.md](../../docs/sensors/GLUCOSE_DATA_LOGGING.md)
and [docs/architecture/STORAGE_NOR_RECORD_STORE.md](../../docs/architecture/STORAGE_NOR_RECORD_STORE.md).

## Related docs

- [GLUCOSE_MATRIX_FILE_LOADING.md](../../docs/sensors/GLUCOSE_MATRIX_FILE_LOADING.md) — XIP slots and update flow
- [SENSOR_STATUS.md](../../docs/sensors/SENSOR_STATUS.md) — driver integration
- [PARTITION_LAYOUT.md](../../docs/build/PARTITION_LAYOUT.md)

## License

Proprietary Svasth calibration data — do not redistribute without authorization.
