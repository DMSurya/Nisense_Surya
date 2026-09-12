# Software Design Diagrams

**Project**: NiSense dual-mode health monitor (nRF52840)  
**Platform**: Zephyr / nRF Connect SDK v3.3.0 (Zephyr ~v4.3.x)  
**Last Updated**: 2026-07-21

Mermaid diagrams for firmware architecture. Pipeline detail for wearable PPG:  
[`05-data/max32664c_to_record_store_pipeline.mmd`](05-data/max32664c_to_record_store_pipeline.mmd). Authoritative narrative: [`docs/architecture/ARCHITECTURE_OVERVIEW.md`](../architecture/ARCHITECTURE_OVERVIEW.md).

> Persistence is NOR **`record_store`** + NVS + XIP glucose models. Export is
> BLE `…def4` sync or USB CDC shell. Narrative:
> [`STORAGE_NOR_RECORD_STORE.md`](../architecture/STORAGE_NOR_RECORD_STORE.md).
> Diagrams under `05-data/` and interaction sequences were redrawn 2026-07-21
> (filename `fat_access_ownership.mmd` now describes record-store sync ownership).

---

## Directory Structure

```
docs/diagrams/
├── 00-system-context/
│   └── system_context.mmd
├── 01-architecture/
│   ├── high_level_architecture.mmd
│   ├── c4_container.mmd
│   ├── layered_architecture.mmd
│   └── dual_mode_hw_detect.mmd
├── 02-hardware-mapping/
│   ├── deployment_diagram.mmd
│   └── mcu_peripheral_mapping.mmd
├── 03-drivers/
│   ├── ppg_spo2/
│   ├── temperature/
│   ├── rtc/
│   └── connectivity/
├── 04-interactions/
│   ├── boot_sequence.mmd
│   ├── health_cycle_sequence.mmd
│   ├── sensor_sampling_sequence.mmd
│   ├── data_upload_sequence.mmd
│   └── power_management_sequence.mmd
├── 05-data/
│   ├── sensor_data_pipeline.mmd
│   ├── buffering_and_persistence.mmd
│   └── fat_access_ownership.mmd
├── 06-behavior/
│   ├── global_system_state_machine.mmd
│   ├── error_fault_handling.mmd
│   └── recovery_flows.mmd
└── README.md
```

---

## Diagram Categories

### 00 - System Context

| Diagram | Purpose |
|---------|---------|
| [system_context.mmd](00-system-context/system_context.mmd) | System boundary and external interfaces |

### 01 - Architecture

| Diagram | Purpose |
|---------|---------|
| [high_level_architecture.mmd](01-architecture/high_level_architecture.mmd) | Subsystems (record_store / BLE / CDC) |
| [c4_container.mmd](01-architecture/c4_container.mmd) | Containers (record_store + def4) |
| [layered_architecture.mmd](01-architecture/layered_architecture.mmd) | Zephyr init levels |
| [dual_mode_hw_detect.mmd](01-architecture/dual_mode_hw_detect.mmd) | WEARABLE vs PULSE runtime profile |

### 02 - Hardware Mapping

| Diagram | Purpose |
|---------|---------|
| [deployment_diagram.mmd](02-hardware-mapping/deployment_diagram.mmd) | MCU to peripheral mapping |
| [mcu_peripheral_mapping.mmd](02-hardware-mapping/mcu_peripheral_mapping.mmd) | GPIO assignments |

### 03 - Drivers

#### PPG/SpO2
| Diagram | Purpose |
|---------|---------|
| [component_diagram.mmd](03-drivers/ppg_spo2/component_diagram.mmd) | MAX32664 driver architecture |
| [state_machine.mmd](03-drivers/ppg_spo2/state_machine.mmd) | Driver operational states |
| [data_flow-passthrough.mmd](03-drivers/ppg_spo2/data_flow-passthrough.mmd) | Sensor to result (passthrough) |

#### Temperature / RTC / Connectivity
| Diagram | Purpose |
|---------|---------|
| [temperature/component_diagram.mmd](03-drivers/temperature/component_diagram.mmd) | Temp driver |
| [temperature/state_machine.mmd](03-drivers/temperature/state_machine.mmd) | Temp states |
| [rtc/component_diagram.mmd](03-drivers/rtc/component_diagram.mmd) | RTC |
| [rtc/time_sync_flow.mmd](03-drivers/rtc/time_sync_flow.mmd) | Time sync |
| [wifi_driver_arch.mmd](03-drivers/connectivity/wifi_driver_arch.mmd) | WExx Wi-Fi |
| [ble_stack_interaction.mmd](03-drivers/connectivity/ble_stack_interaction.mmd) | BLE GATT |

### 04 - Interactions

| Diagram | Purpose |
|---------|---------|
| [boot_sequence.mmd](04-interactions/boot_sequence.mmd) | Boot → XIP / cal / record_store / UI |
| [health_cycle_sequence.mmd](04-interactions/health_cycle_sequence.mmd) | Measure cycle → loggers → record_store |
| [sensor_sampling_sequence.mmd](04-interactions/sensor_sampling_sequence.mmd) | Sample → algo → persist + live BLE |
| [data_upload_sequence.mmd](04-interactions/data_upload_sequence.mmd) | BLE …def4 pull + CDC `nisense rec` |
| [power_management_sequence.mmd](04-interactions/power_management_sequence.mmd) | `power_mgr` display + SOC |

### 05 - Data

| Diagram | Purpose |
|---------|---------|
| [sensor_data_pipeline.mmd](05-data/sensor_data_pipeline.mmd) | Sensor → UI / record_store / BLE |
| [buffering_and_persistence.mmd](05-data/buffering_and_persistence.mmd) | RAM / QSPI partitions / export |
| [fat_access_ownership.mmd](05-data/fat_access_ownership.mmd) | record_store write vs BLE/CDC sync |
| [max32664c_to_record_store_pipeline.mmd](05-data/max32664c_to_record_store_pipeline.mmd) | **MAX32664C → PPG algo → NOR** (MFIO IRQ + 200 ms poll, FIFO, feeder, chunks) |

### 06 - Behavior

| Diagram | Purpose |
|---------|---------|
| [global_system_state_machine.mmd](06-behavior/global_system_state_machine.mmd) | Wear, measure, display, storage |
| [error_fault_handling.mmd](06-behavior/error_fault_handling.mmd) | Errors (record_store / QSPI busy) |
| [recovery_flows.mmd](06-behavior/recovery_flows.mmd) | Recovery procedures |

---

## Mapping to code

| Topic | Code |
|-------|------|
| Boot / main loop | `src/main.c` |
| Health cycles | `src/sensors/health_sched.c` — staged PPG + schedule: [PPG_ALGO_PARAMS_AND_SCHEDULING.md](../architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md) |
| Record store / sync | `src/storage/record_store.c`, `src/ble/ble_record_sync.c`, `src/diag/diag_shell.c` |
| MAX32664C → NOR pipeline | `drivers/sensor/max32664/`, `subsys/ppg_algo/`, `src/sensors/ppg_logger.c` — see [MAX32664C_PPG_NOR_PIPELINE.md](../architecture/MAX32664C_PPG_NOR_PIPELINE.md) |
| Power / display idle | `src/power/power_mgr.c` (wake on meas complete via `power_mgr_activity_notify`) |
| Product SKU detect | `product_hw_detect` (Watch/Pulse) |
| Boot sequence | power → detect → Resource/Model CRC → UI |
| UI | `src/ui/ui.c`, `src/ui/ui_buttons.c` |
| Data pipeline | `src/*_logger.c` → `record_store` |

---

## Rendering

- VS Code: Markdown Preview Mermaid Support  
- CLI: `mmdc -i diagram.mmd -o diagram.svg`  
- Online: [Mermaid Live Editor](https://mermaid.live/)

---

## Maintenance

1. Update diagrams when storage, power, or health-cycle behavior changes  
2. Bump **Last Updated** in this README  
3. Keep narrative in sync with `ARCHITECTURE_OVERVIEW.md`

## Related

- [Architecture overview](../architecture/ARCHITECTURE_OVERVIEW.md)  
- [PPG params / RESP windows / BLE schedule](../architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md)  
- [NOR record store (current)](../architecture/STORAGE_NOR_RECORD_STORE.md)  
- [FatFS/MSC retired stub](../architecture/STORAGE_NOR_FAT_MSC.md)  
- [Docs index](../README.md)  
- [Glucose logging / export](../sensors/GLUCOSE_DATA_LOGGING.md)  
- [Partition layout](../build/PARTITION_LAYOUT.md)
