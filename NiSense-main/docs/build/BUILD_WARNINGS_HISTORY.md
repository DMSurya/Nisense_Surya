# Build Warnings History

**Last Updated**: 2026-07-21  
Notable compile/link issues and fixes for NiSense (NCS 3.3 / nRF52840).

---

## 2026-07 — record_store era

| Issue | Symptom | Fix |
|-------|---------|-----|
| UDC stack too small for CDC shell | MPU fault / stack overflow on `udc_nrfx`, regs `0xAAAAAAAA` (~1 min after boot when USB enumerates) | `CONFIG_UDC_NRF_THREAD_STACK_SIZE=1536` (+ `USBD_THREAD_STACK_SIZE=2048`) in [`conf/features/shell.conf`](../../conf/features/shell.conf) — defaults 512/1024 when MSC USB overlay is off |
| Toolchain PATH drops `System32` | Link fails: `cmd.exe` not recognized | [`scripts/setup/build_project.ps1`](../../scripts/setup/build_project.ps1) restores Windows system PATH after nrfutil env |
| Unknown DT vendor `hcm` | DTC warning on custom nodes | [`dts/bindings/vendor-prefixes.txt`](../../dts/bindings/vendor-prefixes.txt) |
| FLASH ~98% / RAM ~92% | Tight link with CDC shell + `record_store` | Documented in [BUILD_STATUS.md](BUILD_STATUS.md); trim features before adding large stacks |
| FatFS / USB MSC removed | Old docs/scripts assume `/NAND:` | Persistence is NOR `record_store`; see [STORAGE_NOR_RECORD_STORE.md](../architecture/STORAGE_NOR_RECORD_STORE.md) |

---

## 2026-06 — RAM / LVGL

| Issue | Symptom | Fix |
|-------|---------|-----|
| LVGL pool 38,912 B | `region 'RAM' overflowed` in `noinit` | Use **31,744 B** (`diag_monitor.conf` / practical production); see [MEMORY_TUNING.md](../debugging/MEMORY_TUNING.md) |
| libc malloc arena ~0 | BusFault in `sys_heap_init` at POST_KERNEL | `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=4096` in `conf/core/memory.conf` |
| Imprecise bus fault near `0x20040010` | Past end of SRAM | Stack/heap corruption; check `main` LVGL stack and arena |

---

## Recurring notes

- Prefer RTT for logs; shell is **USB CDC** (`nisense …`), not RTT shell.
- Size reports: `arm-zephyr-eabi-size` on `build_sdk_v330/NiSense/zephyr/zephyr.elf`.
- DT / Kconfig changes often need a pristine rebuild when overlays change partition layout.

---

## Related

- [BUILD_STATUS.md](BUILD_STATUS.md)  
- [OTA_BOOTLOADER.md](OTA_BOOTLOADER.md)  
- [BOOT_LOG_GUIDE.md](../debugging/BOOT_LOG_GUIDE.md)  
