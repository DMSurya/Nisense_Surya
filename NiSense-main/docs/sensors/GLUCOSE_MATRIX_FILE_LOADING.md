# Glucose matrix / clinical model loading

**Last updated**: 2026-07-29

Runtime matrices come from the **Model** A/B store on external QSPI (XIP mmap),
not FatFS.

| Slot | Label | Offset | Size |
|------|-------|--------|------|
| Active / staging | `model-primary` / `model-secondary` | `0x1F6000` / `0x276000` | 512 KB each |

## Pack format

Each slot is an **`MDLP` model pack** containing typed entries. Manufacturing
flashes `model_pack.bin` (wearable + pulse `GMDL` blobs). BLE `…def3` updates
the inactive pack slot after CRC.

Boot:

1. `product_hw_detect()` → Watch or Pulse  
2. `model_store_boot_verify()` / `glucose_model_xip_init()` — pack CRC, then bind
   glucose entry matching SKU  
3. Profile + matrix validate before measure  

Legacy bare `GMDL` at slot start is still accepted for one migration path.

Code: [`include/glucose_model_xip.h`](../../include/glucose_model_xip.h),
[`drivers/sensor/glucose/glucose_model_xip.c`](../../drivers/sensor/glucose/glucose_model_xip.c).

Flash: [`scripts/flash/flash_glucose_model.ps1`](../../scripts/flash/flash_glucose_model.ps1)
`-Variant pack` (default).

See [`PARTITION_LAYOUT.md`](../build/PARTITION_LAYOUT.md).
