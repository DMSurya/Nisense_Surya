# MAX32664 driver notes

## Twister / ztest

Status + MFIO hold-low contract helpers:

```text
west twister -T tests/drivers/max32664 -p native_sim
```

These tests do not require hub hardware; they cover command-status
classification (`0x00` / `0xFE` / `0xFF`) and the Normal MFIO hold-low model.

## MFIO (AN6924)

- **Normal mode:** assert MFIO low ≥300 µs, hold through write + CMD_DELAY + read, then high.
- **IRQ mode:** after edge, five dummy writes to I2C addr `0x00`; FW **32.x.y** falls back to Normal+poll.
- DT `interrupt-gpios` should use `GPIO_ACTIVE_LOW`.

## Init timing (Variant C)

RSTN ≥10 ms low, MFIO high ≥1 ms before RSTN rise, +50 ms settle, then **1800 ms** ready.

## MSBL update

Use `max32664_update_firmware_buf()` or `max32664_update_firmware_from_app_partition()`.
Path/`/NAND:` FatFS update is removed.
