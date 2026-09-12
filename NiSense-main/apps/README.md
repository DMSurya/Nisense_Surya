# NiSense host applications

Client apps that talk to the HCM wearable over BLE. Firmware (Zephyr) lives at the repo root.
Cloud API / web console live beside the BLE clients below.

| App | Path | Platforms | Purpose |
|-----|------|-----------|---------|
| **HCM Monitor** | [`pc/`](pc/) | Windows, Linux, macOS | Python/Qt desktop — lab, logging, DFU, GATT explorer |
| **HCM Mobile** | [`mobile/`](mobile/) | Android (iOS later) | Flutter — Bluetooth, Dashboard (last-reading + hold-to-start), Sync, DFU, cloud |
| **Platform API** | [`server/`](server/) | Docker / Pi | FastAPI ingest, RBAC, artifacts, **Excel export** |
| **Web console** | [`web/`](web/) | Browser | Admin UI + Data Browser export |

**Operator checklist** (firmware build/flash, shell, screens, Excel):  
[`docs/guides/PLATFORM_GUIDE.md`](../docs/guides/PLATFORM_GUIDE.md).  
**Firmware lifecycle:** [`docs/build/BUILD_FLASH_DEPLOY.md`](../docs/build/BUILD_FLASH_DEPLOY.md) ·  
**Repo root:** [`README.md`](../README.md).

## Quick start

**PC monitor:**

```powershell
cd apps/pc
pip install -r requirements.txt
python hcm_monitor.py
```

Or from repo root: `.\scripts\run_hcm_monitor.ps1`

**Mobile (Android):**

Full toolchain setup (Flutter, Android Studio, JDK 17, SDK, device): [`mobile/README.md`](mobile/README.md).  
Connect/pair flow after install: [`docs/clients/ANDROID_MONITOR.md`](../docs/clients/ANDROID_MONITOR.md).

**Server + web:**

```bash
cd apps/server && ./scripts/dev.sh up
cd apps/web && npm install && npm run dev
```

## Protocol & data path

Shared GATT definitions: [`docs/ble/`](../docs/ble/) and [`pc/hcm_protocol.py`](pc/hcm_protocol.py) (Python reference).

Measurement history: device NOR [`record_store`](../docs/architecture/STORAGE_NOR_RECORD_STORE.md) →
mobile BLE `…def4` pull (`mobile/lib/ble/record_sync_client.dart`) → local SQLite +
`HCM_Logs/` CSV/XLSX → optional `POST /api/v1/ingest/readings` →
`GET /api/v1/export/readings.xlsx` / web **Export to Excel**.
