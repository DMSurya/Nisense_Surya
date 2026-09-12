# UI Guide — LVGL Screens and Measurement UX

**Last Updated**: 2026-08-05  
**Config overlay**: `conf/features/display.conf`

> **Mobile companion** mirrors Measure long-press over BLE: hold Start Vitals/Glucose
> for `CONFIG_UI_MEASURE_LONG_PRESS_MS` (default 5 s), then Measurement Control
> (`f010`) with flag `0x01` (skip proximity). See
> [`guides/PLATFORM_GUIDE.md`](../guides/PLATFORM_GUIDE.md) and
> [`clients/ANDROID_MONITOR.md`](../clients/ANDROID_MONITOR.md).

> **PPG staged live** (algorithm + sample floors):  
> [`architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md`](../architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md) §3a.

---

## Architecture

| Layer | Files | Role |
|-------|-------|------|
| Carousel / navigation | `src/ui/ui.c` | Lazy screen create/delete, SW1 navigation, swipe; `ui_is_navigation_blocked()` |
| Status strip | `src/ui/ui_shell.c` | **One** persistent top bar on `lv_layer_top` (BLE / Wi‑Fi / wear / time / battery); refreshed by `ui_shell_update()` only |
| Screen modules | `src/*_ui.c` | One module per screen; each has its own `*_update_screen()` for content below `UI_SHELL_TOP_INSET` |
| Auto-nav / slideshow | `src/ui/ui_agent.c` | Staged PPG screen requests + post-cycle results overlay |
| Theme / widgets | `src/ui/ui_theme.c`, `src/ui/ui_common.c` | Cards, fonts, colors |
| Resource store | `src/xip_*.c`, external flash | Logo, icons (compiled Montserrat for shell text) |
| Lifecycle debug | `src/ui/ui_obj_track.c` | Widget naming + CREATE/DELETE RTT logs |
| Long-press measure | `src/ui/measure_ui.c` | Measure button hold countdown + cycle progress |

Screens are **created on demand** and destroyed when leaving the carousel (except Device Info home). The status strip is **not** recreated on navigation — only the screen content area changes. `safe_delete_screen()` in `ui.c` runs screen-specific cleanup before `lv_obj_del()`; `ui_shell_detach()` is a no-op for the strip.

Content titles/layouts reserve space with `UI_SHELL_TOP_INSET` (22 px). Overlays (hold / notice / slideshow) may still draw above the strip.

---

## Screen list (typical full build)

| Index | Screen | Kconfig | Notes |
|-------|--------|---------|-------|
| 0 | Home (clock) | always on with display | Digital clock + date; wear border |
| 1 | Metabolic | `CONFIG_UI_SCREEN_GLUCOSE` | BL-GLU / INS-RES / BL-HEM / HOMA-IR (display) |
| 2 | Vital | `CONFIG_UI_SCREEN_VITALS` | HR / SpO2 / Resp / Body Temp °F (display + live mid-capture) |
| 3 | Vascular | `CONFIG_UI_SCREEN_VASCULAR` | SDNN / RMSSD / BP Sys/Dia (display + live mid-capture) |
| 4 | Measure | `CONFIG_UI_SCREEN_MEASURE` | Only place to start a scan (tap / 5 s hold) |
| — | USB / Storage / BLE / … | per `CONFIG_UI_SCREEN_*` | After Measure |

Battery (`UI_SCREEN_POWER`), PMIC & Charger (`UI_SCREEN_CHARGER`), and Info
(`UI_SCREEN_HOME_INFO`) are **off** in the product display conf. Use CDC shell
`nisense power` / `nisense info`, BLE `f101`/`f109` (PMIC), and `f01a` (device
build) instead. Source remains behind Kconfig for bring-up builds. Charge UX
still lives in `power_ui.c` for bring-up builds and reports **Battery fault** /
USB-no-pack clearly when the FG sees an absent cell.

Enable screens in `conf/features/display.conf` or menuconfig **NiSense — UI Screen Selection**.

---

## Staged live PPG UX (`CONFIG_PPG_ALGO_STAGED_LIVE`)

During a health-cycle or Measure-started PPG session:

| Stage | Approx. samples @ 25 Hz | UI |
|-------|-------------------------|-----|
| Partial vitals | ~300 | Auto-nav to **Vital**; cards show live HR/SpO2 (“Live”) |
| Partial vascular | when Hb/BP/HRV/Resp ready, or at 500 | Auto-nav to **Vascular**; live SDNN/BP |
| Record ready | 500 | NOR vitals committed; SFH LEDs stop when live tail = 0 |
| Session complete | same as 500 with product defaults | Advance to **glucose** → Metabolic; then slideshow |

Vital / Vascular cards refresh from `ppg_get_result()` while sampling (not only at Done).

### Swipe / navigation lock

| Condition | Swipes blocked? |
|-----------|-----------------|
| Staged live PPG / health cycle busy | **No** — user can follow auto-nav or browse results |
| Glucose ADC measuring | **Yes** |
| Staged live **off** + PPG/health busy | **Yes** (legacy) |

Rationale: compiled fonts removed the old NOR font touch-lock; blocking swipes fought staged auto-nav.

---

## Status strip — battery fault

The strip is **20 px** tall (`UI_SHELL_HEIGHT`). Pack absent / USB-bench-no-cell shows a **drawn** red battery outline + tip and Montserrat `!` (not a scaled 40×40 XIP icon — that collapsed to speckles). Normal path keeps LVGL battery symbols + `NN%`.

---

## Long-press measurement (Measure screen only)

Result screens (Metabolic / Vital / Vascular) are display-only for *starting* a scan. Start from **Measure**:

1. **Idle** — progress bar hidden; hint *"Press & hold to skip proximity"*.
2. **Short tap** (when worn) — starts a proximity-gated full cycle.
3. **Press and hold** (~5 s, `CONFIG_UI_MEASURE_LONG_PRESS_MS`) — progress bar appears **full** and counts **down**; on complete, starts with proximity bypass.
4. **Running** — same progress bar shows live measurement progress; **Measure All** is disabled until the cycle finishes. Hold hints use ASCII `...`. With staged live, Vital/Vascular may still update via auto-nav while Measure shows progress.

### Measure All screen (`measure_ui.c`)

| Gesture | Behaviour |
|---------|-----------|
| **Short tap** | Full cycle only if wear state is **good** (proximity ≥ 2). |
| **Long press** (~5 s) | Full cycle with **proximity bypass** — progress bar counts down during the hold. |
| **Auto timer** | Still **wear-gated** only; proximity never bypassed. |

Button is at the bottom of the screen (no proximity hint — wear dot is in the status bar).

| Setting | Default | Range |
|---------|---------|-------|
| `CONFIG_UI_MEASURE_LONG_PRESS_MS` | 5000 ms | 800–10000 (menuconfig) |

Hints: Measure idle shows *"Press & hold to skip proximity"*; result screens say *"Swipe to Measure to scan"*.

**API** (`ui_measure_hold.h`) — still available for other long-press overlays; Measure screen drives its own bar countdown.

---

## LVGL lifecycle debug (hardfault hunting)

Enabled in dev via `display.conf`:

```text
CONFIG_LV_USE_OBJ_NAME=y
CONFIG_APP_UI_OBJ_LIFECYCLE_DEBUG=y
CONFIG_UI_OBJ_TRACK_LOG_LEVEL_DBG=y
CONFIG_UI_LOG_LEVEL_DBG=y
```

**What you see in RTT:**

```
<dbg> ui_obj_track: LVGL CREATE screen home
<dbg> ui_obj_track: LVGL DELETE home/step_label
```

**Helpers** (`ui_obj_track.h`):

- `ui_obj_name(obj, "glucose", "value_label")` — sets LVGL object name `glucose/value_label`
- `ui_label_create()` / `ui_btn_create()` — named create + DELETE hook
- `ui_obj_track_screen_tree(screen, slug)` — auto-names unnamed children `slug/n0`, `slug/n1`, …

**Production:** set `CONFIG_APP_UI_OBJ_LIFECYCLE_DEBUG=n` and per-module log levels to `INF`.

**RAM note:** Setting **all** `*_UI_LOG_LEVEL_DBG` increases RTT/formatting cost; the link-time RAM limit is dominated by `CONFIG_LV_Z_MEM_POOL_SIZE`. See [BUILD_STATUS.md](../build/BUILD_STATUS.md).

---

## Per-module log levels

Menuconfig: **NiSense — UI module log levels**

Defaults in `display.conf`: `ui_obj_track` + `ui` at DBG; individual screens at INF.

Bump one screen to DBG while debugging that module only, e.g.:

```text
CONFIG_GLUCOSE_UI_LOG_LEVEL_DBG=y
```

---

## Fonts and memory

| Setting | Value | Notes |
|---------|-------|-------|
| `CONFIG_LV_Z_MEM_POOL_SIZE` | **31,744 B** (links) / **38,912 B** in `display.conf` | Prefer 31,744 B for production or trim static RAM |
| `CONFIG_LV_Z_VDB_SIZE` | 20% | Double-buffered partial refresh |
| Fonts | Compiled Montserrat + XIP icons | Shell uses Montserrat 14; FA battery glyphs are not in that face — use LV symbols or drawn art |

Missing glyphs or malloc failures → check pool size and that only one heavy screen is loaded.

---

## Storage / USB screens (obsolete)

FatFS userdata and USB MSC are **removed**. Legacy UI paths are inactive:

| Legacy | Status |
|--------|--------|
| `src/ui/usb_ui.c` MSC toggle (`APP_FEATURE_USB`) | Feature off — do not document as product path |
| Storage screen (`CONFIG_UI_SCREEN_STORAGE` / FatFS format) | Obsolete with `APP_FEATURE_FILE_SYSTEM=n` |

Measurement history export is **BLE `…def4`** or optional USB CDC `nisense rec …`
(when `APP_FEATURE_SHELL=y`) — see
[STORAGE_NOR_RECORD_STORE.md](../architecture/STORAGE_NOR_RECORD_STORE.md).

---

## Related docs

- [PPG_ALGO_PARAMS_AND_SCHEDULING.md](../architecture/PPG_ALGO_PARAMS_AND_SCHEDULING.md) — staged live, algo floors, SFH stop
- [BOOT_LOG_GUIDE.md](../debugging/BOOT_LOG_GUIDE.md) — boot RTT messages
- [RTT_DEBUG.md](../debugging/RTT_DEBUG.md) — RTT logs; CDC shell
- [subsys/ppg_algo/README.md](../../subsys/ppg_algo/README.md) — vitals integration
- [GLUCOSE_DATA_LOGGING.md](../sensors/GLUCOSE_DATA_LOGGING.md) — glucose UI + record_store export
- [STORAGE_NOR_RECORD_STORE.md](../architecture/STORAGE_NOR_RECORD_STORE.md) — NOR persistence
