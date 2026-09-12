# Client applications

Host-side apps for the NiSense HCM wearable. BLE protocol and security are documented under [`docs/ble/`](../ble/).

**Operator checklist:** [`guides/PLATFORM_GUIDE.md`](../guides/PLATFORM_GUIDE.md).

## Applications

| Client | Location | Doc |
|--------|----------|-----|
| PC monitor (Qt/Python) | [`apps/pc/`](../../apps/pc/) | [`apps/pc/README.md`](../../apps/pc/README.md) |
| Mobile companion (Flutter) | [`apps/mobile/`](../../apps/mobile/) | [`apps/mobile/README.md`](../../apps/mobile/README.md), [`ANDROID_MONITOR.md`](ANDROID_MONITOR.md) |
| Platform API | [`apps/server/`](../../apps/server/) | [`apps/server/README.md`](../../apps/server/README.md) |
| Web console | [`apps/web/`](../../apps/web/) | [`apps/web/README.md`](../../apps/web/README.md) |

## Flow and pairing

- **PC monitor:** [`BLE_MONITOR_FLOW_GATES.md`](../ble/BLE_MONITOR_FLOW_GATES.md) — Windows WinRT pairing, Linux BlueZ.
- **Mobile:** OS-native SMP (numeric comparison); SECURE firmware profile; default advertise name **NiSense**.
- **GATT security matrix:** [`BLE_DATA_SECURITY_DATABASE.md`](../ble/BLE_DATA_SECURITY_DATABASE.md)
- **Last-reading seed + hold-to-start:** [`ANDROID_MONITOR.md`](ANDROID_MONITOR.md)
- **NOR record sync / CSV / XLSX / cloud:** BLE `…def4` → mobile Sync → `HCM_Logs/` + optional ingest →
  `GET /api/v1/export/readings.xlsx`; see [`STORAGE_NOR_RECORD_STORE.md`](../architecture/STORAGE_NOR_RECORD_STORE.md).

## Development firmware profiles

| Profile | Overlay | Use |
|---------|---------|-----|
| SECURE (production) | default in `conf/features/ble.conf` | Mobile app, phone nRF Connect |
| OPEN (dev) | `conf/features/ble_open.conf` | PC monitor without MITM pairing |
