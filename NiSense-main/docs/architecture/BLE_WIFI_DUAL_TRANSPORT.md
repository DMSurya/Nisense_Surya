# BLE + Wi-Fi Dual Transport Architecture

**Status**: Implemented (protocol 3.2)  
**Last updated**: 2026-08-01

## Verdict

NiSense keeps **BLE as the always-on control plane** and uses **on-demand Wi-Fi STA** as a high-bandwidth data plane. SoftAP and Wi-Fi Direct are **not** product paths. The phone hosts a short-lived local HTTP transfer server; the device is an HTTP client (WExx).

## Roles

| Plane | Responsibilities |
|-------|------------------|
| BLE | Discovery, bonding, config, live notifies, Wi-Fi credential write (`…def2`), bulk session START/ACK/ABORT (`f206`/`f207`), SUMMARY sync, OTA fallback |
| Wi-Fi STA | FULL record sync POST `/sync/records`, optional chunked OTA GET `/ota/*` |

## Provisioning UX

1. **Phone scans nearby Wi-Fi APs** (Android `wifi_scan`; iOS falls back to manual SSID —
   Apple blocks general AP scan without special entitlements).
2. User selects SSID (security shown) and enters PSK; vault stores known networks.
3. App writes SSID/PSK over bonded BLE `…def2` and triggers connect.
4. Device joins the AP and reports success/failure on Wi-Fi status notify (`…f204`).
5. For bulk sync: phone starts `LocalTransferServer`, writes session START on `f206`.
6. Device POSTs packed record frames; phone stores; BLE ACK reclaim.
7. Optional OTA flags: device streams model/resource over Wi-Fi; firmware via BLE SMP.
8. Device disconnects Wi-Fi after session.

Hotspot fallback remains available when premises join fails.

## Wire formats

### `f206` START (protocol 3.2)

`cmd=0x01 | mode:u8 | flags:u16le | port:u16le | after_id:u32le | token_len:u8 | host_len:u8 | token | host`

Flags: model=`0x01`, resource=`0x02`, firmware=`0x04`.

ACK=`0x02` + `up_to_id:u32le`. ABORT=`0x03`.

### `f207` status (18 B packed)

`state:u8 | error:i8 | pending:u32 | cursor_id:u32 | ack_id:u32 | sent_count:u32`

### HTTP record POST

`POST /sync/records` body: `token=…&count=1&frame=<hex>`  
Response JSON: `{ "ack": <id>, "up_to_id": <id> }`

### Shared record frame

Same 16 B header as BLE `…def4` (`record_sync_engine`).

## Modules

| Path | Role |
|------|------|
| `src/storage/record_sync_engine.c` | Transport-agnostic packing/iteration |
| `src/net/wifi_bulk_session.c` | Session state machine + HTTP POST |
| `src/net/wifi_ota_transfer.c` | Chunked hex OTA GET helpers |
| `apps/mobile/lib/services/bulk_session_controller.dart` | Orchestration + BLE fallback |
| `apps/mobile/lib/net/local_transfer_server.dart` | Phone HTTP endpoint |
| `apps/mobile/lib/services/wifi_network_vault.dart` | Encrypted SSID→PSK vault |

## Memory / feature gate

**Production default: `CONFIG_APP_FEATURE_WIFI=y`** (lean STA + HTTP bulk sync).

**Measured after reclaim (2026-08-01):** FLASH **954,372 B (97.1%)** · ~28 KB free;
RAM **240,356 B (91.7%)** · ~22 KB free — meets ≥20 KB FLASH / ≥12 KB RAM gate.

Reclaim: lean WExx buffers/stacks, bulk-session buffer union, heap/LVGL/RTT trim,
log level 3, BLE live PPG/accel streams off. Keep shell/USB off with Wi-Fi.

Lean overlay: [`conf/features/wifi.conf`](../../conf/features/wifi.conf).

## Failure / recovery

| Failure | Recovery |
|---------|----------|
| Wi-Fi join fail | Mobile falls back to BLE `…def4` / SMP |
| Mid-transfer drop | Resume with `after_id`; no reclaim until BLE ACK |
| App kill | Phone server stops; device errors and tears down Wi-Fi |
| Unauthorized LAN | Token required (Bearer or `token=` / query) |

## Related

- [`../ble/BLE_DATA_SECURITY_DATABASE.md`](../ble/BLE_DATA_SECURITY_DATABASE.md)
- [`../sensors/WIFI_INTEGRATION.md`](../sensors/WIFI_INTEGRATION.md)
- [`STORAGE_NOR_RECORD_STORE.md`](STORAGE_NOR_RECORD_STORE.md)
- [`../build/BUILD_STATUS.md`](../build/BUILD_STATUS.md)
