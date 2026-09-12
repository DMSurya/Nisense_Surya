# NiSense Platform Guide — Build, Flash, Test, Screens, Export

**Last updated**: 2026-07-30

End-to-end operator checklist: firmware, mobile companion, optional USB CDC shell,
and cloud Excel export. Canonical lifecycle detail:
[`build/BUILD_FLASH_DEPLOY.md`](../build/BUILD_FLASH_DEPLOY.md).  
Repo map: [`README.md`](../../README.md) · Hub: [`docs/README.md`](../README.md).

---

## 1. Firmware — build, flash, verify

### Build

From the repo root (nRF Connect SDK **v3.3.0** toolchain):

```powershell
.\scripts\setup\build_project.ps1
.\scripts\setup\build_project.ps1 -Clean    # after toggling APP_FEATURE_SHELL/USB
```

Or VS Code: **Ctrl+Shift+B** → **Build + Flash + Monitor**.

Toggle features in **`prj.conf` only**. Do not leave sticky
`-DCONFIG_APP_FEATURE_SHELL=y` in IDE Extra CMake args — it overrides `=n`
and keeps USB linked. Details: [BUILD_FLASH_DEPLOY.md § Sticky -D](../build/BUILD_FLASH_DEPLOY.md#sticky--dconfig--common-footgun).

Scripts: [`scripts/README.md`](../../scripts/README.md).  
Memory: [`debugging/MEMORY_TUNING.md`](../debugging/MEMORY_TUNING.md) ·
[`build/BUILD_STATUS.md`](../build/BUILD_STATUS.md).

### Flash

```powershell
.\scripts\flash\flash_jlink.ps1
```

QSPI Resource / Model / calibration (separate from app image):

```powershell
.\scripts\flash\flash_qspi_stores.ps1
.\scripts\flash\flash_glucose_model.ps1          # Model wrapper
.\scripts\flash\flash_resource.ps1               # Resource wrapper
```

Partitions / OTA: [`build/PARTITION_LAYOUT.md`](../build/PARTITION_LAYOUT.md),
[`build/OTA_BOOTLOADER.md`](../build/OTA_BOOTLOADER.md).

### RTT logs (not the interactive shell)

```powershell
.\scripts\rtt\rtt_monitor.ps1
```

Expect after a vitals run something like:

```text
<inf> ppg_logger: Vitals #N: HR=… SpO2=… Hb=… Resp=… SDNN=… BP=…/…
<inf> ble_gatt: BLE cache seeded: HR=… Hb=… BP=…/… gluc=… insulin=… homa=…
```

Hb needs a valid **green LED** path (`green=1` in the vitals complete log).  
BP is estimated whenever `hr_bpm > 0` (even under `LOW_CONF`); `bp_valid` stays stricter.

---

## 2. USB CDC shell (optional)

Interactive shell is **USB CDC ACM**, not RTT. Production builds often leave it
**off** (`CONFIG_APP_FEATURE_SHELL=n`) to save ~FLASH.

To enable: set in `prj.conf`:

```text
CONFIG_APP_FEATURE_SHELL=y
CONFIG_APP_FEATURE_USB=y    # selected by shell; set explicitly for clarity
```

Then **pristine rebuild**. Shell merges `conf/features/shell.conf` (`SHELL_MINIMAL`);
USB merges `conf/features/usb.conf` (USBD + CDC). Shell **cannot** run without USB.
Expect ~**98%** FLASH with the full product feature set.

| Command | Purpose |
|---------|---------|
| `nisense rec stats` | Pending summary/raw counts, slot usage |
| `nisense rec summary [N]` | Dump recent summary records |
| `nisense rec get <id>` | Summary + raw chunks for one measurement |
| `nisense rec export <id>` | CSV text for one measurement |
| `nisense rec ack <id>` | Mark synced through id |
| `nisense rec reset` | Factory-erase record store |
| `nisense power` / `nisense info` | Power / device info (when UI power screens are off) |

Full table: [`architecture/STORAGE_NOR_RECORD_STORE.md`](../architecture/STORAGE_NOR_RECORD_STORE.md).  
RTT vs shell: [`debugging/RTT_DEBUG.md`](../debugging/RTT_DEBUG.md).

---

## 3. Mobile app — build, install, screens

### Build / run

Full toolchain (Flutter, JDK 17, Android SDK): [`apps/mobile/README.md`](../../apps/mobile/README.md).

```powershell
cd apps\mobile
flutter pub get
flutter test
flutter run -d <device_id>
```

Connect / pair ceremony: [`clients/ANDROID_MONITOR.md`](../clients/ANDROID_MONITOR.md).  
BLE checklist: [`ble/BLE_TEST_CHECKLIST.md`](../ble/BLE_TEST_CHECKLIST.md).

### Bottom navigation (AppShell)

| Tab | Screen | What it does |
|-----|--------|--------------|
| **Bluetooth** | Scan | Scan, known devices, connect / disconnect |
| **Dashboard** | Live vitals | Last-reading seed on connect; hold-to-start measure; battery / wear |
| **Raw** | Streams | PPG / glucose raw charts (former Charts) |
| **Settings** | Device prefs | RTC, brightness, volume, Wi‑Fi, PMIC lab |
| **Sync** | Records | NOR pull, local SQLite, CSV/XLSX, optional cloud push |
| **Firmware** | DFU | Bond / forget, SMP image upload |

**Server / Cloud** is the app-bar cloud icon (not a bottom tab) → OIDC login, server URL, OTA artifacts.

### Dashboard usage

1. Connect and pair (SECURE). Within ~3–4 s the firmware seeds BLE cache from
   `home/last` **plus** latest VITALS/GLUCOSE records, then re-notifies subscribers.
2. Cards show last readings (HR, SpO₂, Hb, resp, glucose, temp, insulin, HOMA, SDNN, BP)
   when those values exist on-device.
3. **Start Vitals / Start Glucose** — press and hold **5 s**
   (`measureLongPressMs` = `CONFIG_UI_MEASURE_LONG_PRESS_MS`). Bar counts down like the
   watch Measure screen; on complete the app writes Measurement Control (`f010`) with
   flag **`0x01` (skip proximity)**.
4. **Stop** — immediate stop write (no hold).
5. Tap a metric card → **Metric History** (local SQLite). App-bar / overflow →
   **Device Info** (polling refresh) and **All Records**.

Device Measure UX (watch): [`ui/UI_GUIDE.md`](../ui/UI_GUIDE.md).

### Sync / local store / cloud

| Action | Where |
|--------|-------|
| Pull NOR via BLE `…def4` | Sync tab (or Server screen helpers) |
| Persist locally | `sqflite` via `RecordLocalStore` |
| CSV / full export | Sync / All Records → `HCM_Logs/` |
| Push to API | Sync → best-effort `POST /api/v1/ingest/readings` |

---

## 4. Server + web — Excel export

### Server

```bash
cd apps/server
cp .env.example .env
./scripts/dev.sh up          # API :8000
```

Excel export (auth required):

```http
GET /api/v1/export/readings.xlsx?device_id=&patient_id=&type=&since=&until=
Authorization: Bearer <token>
```

Sheets mirror mobile XLSX layout: **Summary**, **Glucose**, **Vitals**, **Temp**,
**PPG_Raw**, **Glucose_Raw** (plus `Other_*` for unknown types).

Ingest: `POST /api/v1/ingest/readings` (idempotent on `(type, record_id)`).  
Details: [`apps/server/README.md`](../../apps/server/README.md).

### Web admin

```bash
cd apps/web
npm install
npm run dev                  # http://localhost:5173 → proxies /api to :8000
```

**Data Browser**: type filters (incl. PPG/Glucose raw), **since / until** dates,
**Export to Excel** (downloads `readings.xlsx` with the same filters).

---

## 5. Suggested smoke test (one sitting)

1. **Flash** firmware + (optional) glucose model; open RTT.
2. On watch: Measure long-press → full cycle (or mobile hold Start Vitals / Glucose).
3. **CDC**: `nisense rec stats` → pending summaries increase.
4. **Phone**: connect → Dashboard fills last readings in ~5 s.
5. **Sync**: pull records → CSVs / local DB; optional cloud push.
6. **Web/API**: open Data Browser → Export Excel; or `curl` the export URL with a token.

---

## 6. Related docs

| Topic | Doc |
|-------|-----|
| Mobile from-scratch setup | [`apps/mobile/README.md`](../../apps/mobile/README.md) |
| Android connect gates | [`clients/ANDROID_MONITOR.md`](../clients/ANDROID_MONITOR.md) |
| GATT / security / `f010` flags | [`ble/BLE_DATA_SECURITY_DATABASE.md`](../ble/BLE_DATA_SECURITY_DATABASE.md) |
| BLE manual tests | [`ble/BLE_TEST_CHECKLIST.md`](../ble/BLE_TEST_CHECKLIST.md) |
| Record store + shell | [`architecture/STORAGE_NOR_RECORD_STORE.md`](../architecture/STORAGE_NOR_RECORD_STORE.md) |
| Watch UI long-press | [`ui/UI_GUIDE.md`](../ui/UI_GUIDE.md) |
| Host apps index | [`apps/README.md`](../../apps/README.md) |
| Web console | [`apps/web/README.md`](../../apps/web/README.md) |
