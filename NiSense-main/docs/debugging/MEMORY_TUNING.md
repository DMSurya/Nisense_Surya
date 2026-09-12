# Memory Tuning Guide

**Platform**: nRF52840 (256 KB RAM) · nRF Connect SDK v3.3.0  
**Last Updated**: 2026-07-30

NiSense uses several independent RAM pools. Size each from **measured peak under stress**, not idle, with ~15–25% headroom.

**Era note:** FatFS / USB MSC are removed. Persistence is NOR `record_store` + NVS;
optional shell is USB CDC (`APP_FEATURE_SHELL` → USB). Link snapshots:
[BUILD_STATUS.md](../build/BUILD_STATUS.md). Lifecycle:
[BUILD_FLASH_DEPLOY.md](../build/BUILD_FLASH_DEPLOY.md).

---

## Link baselines (zephyr.elf)

### Production default (shell/USB **off**) — 2026-07-30

```
FLASH:  ~901000 B / 982704 B  (91.69%)
RAM:    see linker "Memory region" for the exact build
```

### Diagnostic (shell + USB, `SHELL_MINIMAL`) — budget stress

```
FLASH:  ~963–969 KB / 982704 B  (~98%)
```

Very little headroom — do not treat as the default production image.

Authoritative snapshot: [BUILD_STATUS.md](../build/BUILD_STATUS.md). Older
“966 KB with CDC shell” rows remain useful as historical stress numbers only.

---

## RAM pools on NiSense

| Pool | Kconfig | Runtime visibility |
|------|---------|-------------------|
| Static BSS / stacks / driver noinit | link-time only | `ram_budget` boot log, `.map` `_image_ram_size` |
| System heap (`k_malloc` / some drivers) | `CONFIG_HEAP_MEM_POOL_SIZE` | `diag_monitor` → **heap peak** |
| LVGL widgets / draw | `CONFIG_LV_Z_MEM_POOL_SIZE` | `diag_monitor` → **LVGL max_used** |
| Thread stacks | `CONFIG_*_STACK_SIZE` | `diag_monitor` → **thread high-watermark** |
| Picolibc malloc arena | `CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE` | `ram_budget` boot log |
| BLE GATT / USB / BT buffers | various `CONFIG_BT_*`, `CONFIG_UDC_*` | Link-time only (`feature_audit`, `.map`) |

Link failures like `region 'RAM' overflowed by N bytes` in section **`noinit`** are **static** — runtime heap stats will not show them. Use the linker map or [BUILD_STATUS.md](../build/BUILD_STATUS.md).

### Boot visibility (`ram_budget`)

`src/core/ram_budget.c` logs at boot (RTT module `ram_budget`):

- `_end` — end of static BSS
- **static_used** — bytes from SRAM base to `_end`
- **libc_malloc_tail** or **static arena** size

`diag_monitor_dump_now()` prints the same via `ram_budget_print_linker_tail()`.

---

## Constraints (nRF52840 full feature set)

These were measured with current `prj.conf` features (BLE, glucose, PPG/MAX32664+MAX3010x,
display, BLE DFU; USB/shell per row above) and core + feature overlays applied.

| Layout | diag | LVGL pool | Links? | Static RAM | Headroom |
|--------|------|-----------|--------|------------|----------|
| **Production intent** | off | 38,912 (`display.conf`) | **No** | 265,980 B | **−3,836 B** overflow |
| **Practical production** | off | 31,744 | Yes | 258,779 B | +3,365 B |
| **Tuning** | on | 31,744 (`diag_monitor.conf`) | Yes | 261,373 B | +772 B |

**Takeaways:**

1. **`CONFIG_APP_FEATURE_DIAG_MONITOR=n` alone does not free enough RAM** to restore the 38,912 B LVGL pool — you still overflow by ~3.8 KB.
2. The **31,744 B LVGL pool** in `diag_monitor.conf` exists to fund diag thread + stack bumps (`MAIN`, `BT_TX_PROCESSOR`, etc.), not only the monitor itself.
3. **Tuning builds have ~772 B linker headroom** — treat that as the hard ceiling; do not grow static pools without a matching trim elsewhere.
4. **`diag_monitor` adds ~110 KB flash** (mostly `CONFIG_DEBUG_OPTIMIZATIONS=y`) and **~2.5 KB RAM** vs the same LVGL layout with diag off.

### libc malloc tail (critical on full-RAM builds)

Zephyr places **static** BSS/data first, then a **dynamic libc malloc arena** from linker symbol `_end` to the top of SRAM (`0x20040000` on nRF52840).

When the build fills RAM (`_end` ≈ top of SRAM), the dynamic libc arena is **~0 bytes**. **`SYS_INIT(malloc_prepare)`** runs at **POST_KERNEL** (before RTT / `main`) and **BusFaults immediately** in `sys_heap_init` — no log output.

A deferred crash at ~2 s is the same root cause when something first calls `malloc()` with a zero-size dynamic arena.

**Fix (applied in `conf/core/memory.conf` + `conf/features/display.conf`):**

```ini
CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=4096   # static arena in BSS
CONFIG_LV_Z_MEM_POOL_SIZE=38912             # display.conf (−4 KB vs old 43008 to fund arena)
```

Do **not** leave `COMMON_LIBC_MALLOC_ARENA_SIZE` at default `-1` on full-RAM nRF52840 builds.
(FatFS LFN heap mode is obsolete — filesystem feature is off.)

---

## CLI builds and feature overlays

`CMakeLists.txt` selects `conf/features/*.conf` from `prj.conf` and sets
`NiSense_EXTRA_CONF_FILE`. Reconfigure after feature toggles; sticky
`-DCONFIG_APP_FEATURE_*=y` can ignore `prj.conf=n` — see
[BUILD_FLASH_DEPLOY.md](../build/BUILD_FLASH_DEPLOY.md). The nRF Connect
extension builds the same tree automatically.

**Plain `west build` from the shell omits feature overlays** unless you pass them:

```powershell
# Minimal pattern (semicolon-separated, match CMakeLists default list):
west build --build-dir build_sdk_v330 --board raytac_mdbt50q_db_40/nrf52840 --sysbuild -- `
  "-DNiSense_EXTRA_CONF_FILE=conf/core/bootloader.conf;conf/core/memory.conf;…;conf/features/display.conf;…"
```

Without overlays, feature Kconfig symbols stay off even when `CONFIG_APP_FEATURE_*=y` in `prj.conf`.

Use `scripts/setup/feature_audit.ps1` as a reference for the overlay list and env setup.

---

## Step 1 — Enable the diagnostics monitor (tuning build only)

In `prj.conf`:

```ini
CONFIG_APP_FEATURE_DIAG_MONITOR=y
```

CMakeLists appends `conf/features/diag_monitor.conf`, which sets:

- LVGL pool **31,744 B** (overrides `display.conf`)
- `CONFIG_DIAG_MONITOR_STACK_SIZE=1536`, interval **5 s**
- `CONFIG_THREAD_ANALYZER=y`, heap runtime stats, sched thread usage
- **`CONFIG_DEBUG_OPTIMIZATIONS=y`** — large flash cost; tuning-only

Or override at build time:

```powershell
-DCONFIG_APP_FEATURE_DIAG_MONITOR=y
# plus conf/features/diag_monitor.conf in NiSense_EXTRA_CONF_FILE
```

This enables:

- Periodic RTT dump of thread stacks + heap + LVGL pool + linker tail
- `CONFIG_SYS_HEAP_RUNTIME_STATS` for accurate peak tracking
- Boot baseline dump from `main.c`

`CONFIG_THREAD_ANALYZER_AUTO=n` — the diag thread calls `thread_analyzer_print` on its own cadence (saves RAM vs a second auto thread).

### Trigger a dump on demand

| Method | How |
|--------|-----|
| **UI** | Long-press the **clock** in the top status bar (~1 s) |
| **BLE** | Write `0x02` to admin characteristic `…abcf014` (bonded + authenticated in secure profile) |
| **Code** | `diag_monitor_dump_now()` from any thread |

Example dump sections:

```
========== DIAG DUMP #3  @ uptime 420000 ms ==========
-- threads (stack high-watermark + CPU %):
-- heap:
  system heap (total …): used=… peak=… free=…
-- LVGL:
  LVGL pool (CONFIG_LV_Z_MEM_POOL_SIZE=31744): used=…% max_used=…
-- linker:
  _end=…  static_used=…  libc_malloc_tail=…
========== END DIAG DUMP #3 ==========
```

---

## Step 2 — Stress test, then read peaks

Run **worst case** for 5–10 minutes before trusting numbers:

1. BLE connected, BAS + notify burst, optional PPG/accel stream
2. Visit every UI screen; start/stop a glucose cycle (measure-all) — exercises `record_store`
3. Optional: BLE `…def4` record sync pull or CDC `nisense rec stats`
4. Wi-Fi scan (if enabled)

### Verified peaks (2026-07-06, tuning build)

| Thread / pool | Peak | Size | Notes |
|---------------|------|------|-------|
| `main` | 5,776 B | 8,192 B | LVGL + UI init |
| `bt_tx_processor` | 744 B | 1,024 B | nRF52840: use `CONFIG_BT_TX_PROCESSOR_STACK_SIZE` |
| `glucose_sensor` | 3,760 B | 5,120 B | Full algorithm + measure-all |
| `ppg_wq` | 2,304 B | 3,072 B | Logger / record_store flush spike |
| System heap | ~10.2 KB | 32.5 KB | ~31% peak |
| LVGL pool | 27,352 B | 31,744 B | ~86% max_used on home + navigation |

### Sizing rules

| Metric | Set Kconfig to |
|--------|----------------|
| Heap `peak` | `peak × 1.15` (15% margin) |
| LVGL `max_used` | `max_used × 1.20` (20% margin; UI spikes) |
| Thread `unused` | increase stack if unused &lt; 15% of stack size |

Edit `conf/core/memory.conf`, `conf/features/display.conf`, and feature overlays (`ble.conf`, `glucose.conf`, …) from these values.

**Do not** raise LVGL toward 38,912 B on the full feature set without removing ≥4 KB static RAM elsewhere.

---

## Step 3 — Static / per-feature RAM (`feature_audit.ps1`)

```powershell
.\scripts\setup\feature_audit.ps1
```

Builds each `APP_FEATURE_*` on a display baseline and reports **flash + RAM delta** per feature.

After a full build:

```powershell
arm-zephyr-eabi-size build_sdk_v330\NiSense\zephyr\zephyr.elf
```

Inspect `build_sdk_v330/NiSense/zephyr/zephyr_pre0.map` for `_image_ram_size` and `noinit` growth when link fails.

---

## Production vs tuning builds

| Build | `APP_FEATURE_DIAG_MONITOR` | LVGL pool | Notes |
|-------|---------------------------|-----------|-------|
| **Production (default)** | `n` | 31,744 B required to link full features* | No periodic dumps; long-press/BLE admin dump compiled out |
| **Production (display.conf only)** | `n` | 38,912 B | **Does not link** today (−3,836 B) |
| **Tuning / bring-up** | `y` | 31,744 B | +110 KB flash, +2.5 KB RAM vs tuned-off; ~772 B headroom |

\*Until ~4 KB static RAM is reclaimed, ship with 31,744 B LVGL or disable a feature — see [plan](#plan).

Long-press and BLE admin dump are compiled out when the feature is disabled.

---

## Plan

### Immediate (production)

1. Keep **`CONFIG_APP_FEATURE_DIAG_MONITOR=n`** in `prj.conf`.
2. Ship with **`CONFIG_LV_Z_MEM_POOL_SIZE=31744`** (move into `display.conf` or document as production override) **or** trim ~4 KB static RAM then restore 38,912 B.
3. Keep **`CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE=4096`** — boot stability depends on it.

### Before increasing any pool

1. Enable tuning build (`diag_monitor=y`).
2. Run the [stress procedure](#step-2--stress-test-then-read-peaks).
3. Record heap peak, LVGL `max_used`, and worst thread high-watermarks.
4. Apply sizing rules with 15–25% headroom.
5. Rebuild production (`diag=n`) and confirm `_image_ram_size` ≤ 262,144 − 512 B margin.

### Optional improvements

| Item | Benefit |
|------|---------|
| Split `CONFIG_DEBUG_OPTIMIZATIONS` out of `diag_monitor.conf` | Smaller tuning flash (~110 KB today); use `-Og` only when debugging in Ozone |
| Promote 31,744 B LVGL to `display.conf` | Single source of truth; avoids production link surprise |
| `feature_audit.ps1` after each new `APP_FEATURE_*` | Catch static RAM regressions early |
| Revisit heap if glucose + CDC shell + Wi-Fi run together | Peak was ~31% — headroom exists but monitor after changes |

### USB + CDC shell stacks (`APP_FEATURE_SHELL=y` → USB)

Owned by **`conf/features/usb.conf`** (shell overlay is shell-only):

| Kconfig | Value | Notes |
|---------|-------|-------|
| `CONFIG_UDC_NRF_THREAD_STACK_SIZE` | **1536** | Default 512 overflows (`udc_nrfx`) during CDC enumerate |
| `CONFIG_USBD_THREAD_STACK_SIZE` | **2048** | Default 1024 is tight with CDC ACM |
| `CONFIG_SHELL_STACK_SIZE` | 2048 | In `shell.conf`; shell thread |
| `CONFIG_SHELL_MINIMAL` | **y** | Selected by `APP_FEATURE_SHELL` (FLASH budget) |

MSC class stays off. If USB is on without shell, the same `usb.conf` stacks apply.

### Regression checklist

- [x] Link succeeds — verified 2026-07-30 production path (~901 KB FLASH, shell/USB off). See [BUILD_STATUS.md](../build/BUILD_STATUS.md). Re-check after enabling shell/USB (~98% FLASH).
- [ ] Boot: `ram_budget` shows static arena 4096 B, no malloc tail warning  
  **Procedure:** Flash build → RTT → confirm `ram_budget` log lines; no BusFault before `main`.
- [ ] Measure-all + BLE connected completes without fault  
  **Procedure:** Bond phone → long-press measure-all → watch RTT for fault; dashboard stays live.
- [ ] LVGL `max_used` &lt; 90% of pool under navigation stress  
  **Procedure:** Enable `diag_monitor` (or shell dump) → swipe all screens for 60 s → `max_used` / pool &lt; 0.90.

---

## Bus fault investigation (imprecise data bus error)

If you see a crash like:

```
***** BUS FAULT *****
Imprecise data bus error
Faulting thread: main
r0/a1: 0x20040010
```

### What it means

- **`0x20040010` is past nRF52840 RAM** (256 KB ends at `0x2003FFFF`). Usually **heap corruption**, **stack overflow on `main`**, or a **wild pointer** — not a normal I²C NACK.
- **`main`** runs `ui_update()` + `lv_timer_handler()` — LVGL refresh is stack-heavy. Overflow corrupts adjacent RAM and can fault later inside `malloc`.

### Fix order (recommended)

1. **Flash latest firmware** — includes PMIC I²C parent-device fix and libc arena fix.
2. **Build with `CONFIG_APP_FEATURE_DIAG_MONITOR=y`** and reproduce — check **`main` stack unused** before the crash.
3. If `main` stack is tight, raise `CONFIG_MAIN_STACK_SIZE` in `conf/core/memory.conf` (currently **8192**).
4. If heap **peak** is &gt; 90%, raise `CONFIG_HEAP_MEM_POOL_SIZE` — do not shrink below measured peak.
5. Check boot `ram_budget` — if **libc tail &lt; 512 B** with dynamic arena, fix [libc malloc tail](#libc-malloc-tail-critical-on-full-ram-builds) first.
6. Use **Ozone / J-Link** on halt — resolve PC with:
   ```powershell
   arm-zephyr-eabi-addr2line -e build_sdk_v330\NiSense\zephyr\zephyr.elf -f -C <pc> <lr>
   ```

---

## Related docs

- [BUILD_FLASH_DEPLOY.md](../build/BUILD_FLASH_DEPLOY.md) — build / flash / feature toggles
- [BUILD_STATUS.md](../build/BUILD_STATUS.md) — link results
- [STORAGE_NOR_RECORD_STORE.md](../architecture/STORAGE_NOR_RECORD_STORE.md) — persistence / CDC shell
- [RTT_DEBUG.md](RTT_DEBUG.md) — RTT logs; CDC shell for `nisense rec`
- [UI_GUIDE.md](../ui/UI_GUIDE.md) — LVGL screens and pool interaction
- `scripts/setup/feature_audit.ps1` — per-feature RAM deltas
- `src/core/ram_budget.c`, `include/ram_budget.h` — linker tail helpers
