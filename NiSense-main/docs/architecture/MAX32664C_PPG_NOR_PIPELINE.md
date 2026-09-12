# MAX32664C → NOR record_store pipeline

**Last updated**: 2026-08-05  
**Diagram**: [`../diagrams/05-data/max32664c_to_record_store_pipeline.mmd`](../diagrams/05-data/max32664c_to_record_store_pipeline.mmd)

Wearable GWEC (Variant C) path from hub bring-up through PPG algo to append-only
NOR slots. Companion narrative for the Mermaid pipeline.

**Algorithm / staged live**: [PPG_ALGO_PARAMS_AND_SCHEDULING.md](PPG_ALGO_PARAMS_AND_SCHEDULING.md) §3a.
Optics: **SFH7074** (Green/Red/IR) on MAX86141; host owns PA + SHDN in RAW.

---

## Mode fork (read first)

| Build | Chosen PPG device | Middle of pipe | Default? |
|-------|-------------------|----------------|----------|
| `CONFIG_MAX32664_MODE_RAW=y` | `max86141_ppg` | Family `0x12` RAW LED frames → on-chip DSP | **Yes** (`conf/features/max32664.conf`) |
| `CONFIG_MAX32664_MODE_HUB=y` | `max32664_hub` | WHRM+WSpO2 frames → `process_hub_sample` | Alternate |

Both share: variant C init, accel feeder, `ppg_logger` → `record_store`, health_sched PPG step, BLE vitals notify.

---

## RSTN + MFIO (current hardware)

Current boards wire **RSTN = P0.20** and **MFIO = P1.06** (see
`boards/raytac_overlay/31_sensor_max32664_hub.overlayinc`). Older “not wired”
notes are obsolete.

| Phase | RSTN | MFIO |
|-------|------|------|
| Boot reset | Pulsed for hardware reset | Held for **application** mode entry |
| Bootloader update | Used with MFIO for BL entry | Select bootloader vs app |
| After init / sampling | Released | Reconfigured as **GPIO input**, falling-edge IRQ; hub IRQ enabled via cmd **`0xB8`** |

**Wake model while sampling**

1. **Primary:** MFIO falling edge → `interrupt_work` → `sample_sem`
2. **Fallback:** `poll_work` every **200 ms** (FIFO count / status) → `sample_sem`  
   Needed because GWEC RAW output often fills Family `0x12` without asserting DataRdy on every batch.
3. **Consume (RAW):** `ppg_algo` @ **40 ms** pulls `max86141_ppg` → `max32664_raw_drain_ppg()` (on-demand I2C), independent of who woke the sem.

---

## Stage summary

| # | Stage | Code | Iteration |
|---|--------|------|-----------|
| 1 | Boot / Variant C | `max32664_init` → RSTN+MFIO reset → `max32664c_init` | Once |
| 2 | Start | `health_sched` PPG → `ppg_algo_start_measurement` | Once per cycle |
| 3 | Accel feeder | `max32664_accel_feeder` | **Forever @ 40 ms (25 Hz)** while active |
| 4A | RAW wake + drain | MFIO IRQ + poll 200 ms; `max32664_raw_drain_ppg` + child refill | Wake: IRQ/poll; drain: **per algo pull** (`for` frames; `while` queue &lt; 32 push ≤16) |
| 4B | HUB WHRM | MFIO IRQ + poll; `max32664_sampling_parse_hub_data` | **per wake**: discard ≤192 stale; parse **newest** 1 frame |
| 5 | Algo tick | `ppg_sensor_work_handler` | RAW: **40 ms** + inner `while` fetch; HUB: **250 ms** / 1 frame |
| 5a | Staged publish | `ppg_maybe_advance_staged` | PARTIAL_VITALS @ ~300; PARTIAL_VASCULAR; refine ~1 Hz |
| 6 | Per-sample gates | prox miss, `SAMPLE_READY`, BLE progress | Every buffer add; BLE every **5** samples |
| 7a | Record ready | `ppg_do_record_ready` @ `record_target` (**500**) | **1× VITALS** + raw chunks; buffer capacity full |
| 7b | Finalize / SHDN | `finalize_measurement` @ `session_target` | Stop feeder; hub stop; SFH PA=0 + SHDN (`LIVE_TAIL=0` ⇒ same as 500) |
| 8 | Logger | (at RECORD_READY when staged) | Already flushed in 7a |
| 9 | NOR slot | `record_store_append` | One QSPI program per append |
| 10 | Handoff | BLE vitals + `MEASUREMENT_COMPLETE` → glucose | Once |

After stop, `max32664_raw_drain_ppg` returns 0 when `sampling == false` so late fetches cannot resurrect SFH LEDs.

---

## Key constants

| Item | Value |
|------|-------|
| Sample rate | **25 Hz** |
| Health PPG / record window | **20 s → 500** with RESP (`APP_HEALTH_PPG_SECONDS`) |
| Algo buffer capacity | **`record_target` only** (not session+tail) |
| Live tail (product) | **0** — AFE/SFH off at record count |
| Partial vitals | **300** samples (~12 s) |
| Algo buffer | Same RESP rule (`PPG_ALGO_BUFFER_SECONDS`) |
| HUB frame clamp | **≤30** algo updates |
| MFIO | **P1.06**, hub IRQ `0xB8` |
| RSTN | **P0.20** |
| Hub poll (fallback) | **200 ms** |
| Accel feed | **25 Hz** / 40 ms |
| NOR slot | **256 B** (16 hdr + 240 payload) |
| PPG sample pack | **42 B** (`rec_ppg_sample`) |
| Samples / `PPG_RAW` chunk | **5** → 500 samples ⇒ **100** chunks (20 s @ 25 Hz) |
| Step / post guards | **90 s** / **30 s** |

---

## Iteration detail

### Accel feeder (parallel)

```
while feeder_active:
    bus_lock → read LIS2DS12 OUT_X..Z (6 B)
    max32664_feed_accel (Family 0x14)
    sleep 40 ms
```

Hard-stop after ~25 consecutive I2C failures.

### RAW wake + FIFO drain

```
# Wake (either path)
on MFIO falling:  sem_give(sample_sem)
every 200 ms:     if FIFO count > 0: sem_give(sample_sem)

# Drain (algo / child pull — default consumer)
on max86141_ppg fetch / refill:
    n = FIFO count (0x12/0x00)
    for s in 0 .. n-1:
        pop frame (0x12/0x01)
        unpack IR/Red/Green (+accel if present)
    while child_queue_len < 32:
        push batch of min(16, space)
```

### Algo RAW tick

```
every 40 ms:
    while measuring:
        fetch → if -EAGAIN: break
        process_sample (preproc, peaks, buffer++)
        fire SAMPLE_READY / BLE progress
        if samples_processed >= target: finalize; break
        # target = APP_HEALTH_PPG_SECONDS × rate (500 with RESP default)
```

### Algo HUB tick

```
every 250 ms:
    fetch (wait sample_sem)   # MFIO or poll woke it
    process_hub_sample (1 WHRM frame → buffer)
    if samples_processed >= min(requested, 30): finalize
```

### NOR chunk write

```
parent_id = append(VITALS, rec_vitals)
K = ceil(total_samples / 5)
for ci in 0 .. K-1:
    hdr.parent_id = parent_id
    hdr.chunk_index / chunk_count = ci / K
    n = min(5, remaining)
    for i in 0 .. n-1:
        pack rec_ppg_sample[i]   # 42 B
    append(PPG_RAW, hdr+samples) # no out_id
```

Join key for export/sync: **`parent_id`**, not timestamp.

---

## Divergence: live BLE vs NOR

| | Live BLE | NOR |
|--|----------|-----|
| Mid-run | progress (+ optional PPG stream) | — |
| End | vitals notify | VITALS + all PPG_RAW |
| Later | — | `…def4` pull / CDC `nisense rec` |

Finalize writes NOR **before** `MEASUREMENT_COMPLETE` so health_sched cannot race the QSPI append.

---

## Related

- [`STORAGE_NOR_RECORD_STORE.md`](STORAGE_NOR_RECORD_STORE.md)
- [`../sensors/MAX32664_VARIANT_ARCHITECTURE.md`](../sensors/MAX32664_VARIANT_ARCHITECTURE.md)
- [`../hardware/HARDWARE_REFERENCE.md`](../hardware/HARDWARE_REFERENCE.md)
- [`../../subsys/ppg_algo/README.md`](../../subsys/ppg_algo/README.md)
- Code: `drivers/sensor/max32664/`, `subsys/ppg_algo/`, `src/sensors/ppg_logger.c`, `src/storage/record_store.c`, `src/sensors/health_sched.c`
