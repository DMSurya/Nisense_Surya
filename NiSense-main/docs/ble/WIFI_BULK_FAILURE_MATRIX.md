# Wi-Fi Bulk Sync — Failure / Recovery Matrix

**Last updated**: 2026-08-01

Manual + automated checks for BLE↔Wi-Fi dual transport.

## Automated

```powershell
cd apps/mobile
dart test test/bulk_session_protocol_test.dart
```

Covers START/ACK/ABORT encode/decode and 18-byte status layout (protocol 3.2).

## Manual matrix

| # | Scenario | Expected |
|---|----------|----------|
| 1 | Premises AP + vault hit | Device joins; Wi-Fi FULL sync completes; BLE ACK reclaims |
| 2 | Premises AP unknown PSK | Prompt once; vault save; sync proceeds |
| 3 | Join fail → hotspot guide | Settings prompt; hotspot vault; STA join; sync |
| 4 | Hotspot declined / fail | Fall back to BLE `…def4` |
| 5 | Kill app mid Wi-Fi transfer | Device session errors; Wi-Fi teardown; no false reclaim |
| 6 | BLE drop mid Wi-Fi transfer | Transfer may continue until TTL; reconnect BLE for ACK |
| 7 | Unauthorized LAN client | HTTP 401 without token |
| 8 | Resume after interrupt | Restart with last durable `after_id` |
| 9 | OTA flags set | Device streams model/resource over Wi-Fi; firmware via BLE SMP after session |
| 10 | SSID auto-detect | `network_info_plus` pre-fills dialog when location/Nearby Wi-Fi allowed |
| 11 | Wi-Fi feature off in firmware | `f206`/`f207` absent; Sync over Wi-Fi falls back to BLE |

## Device logs to watch (RTT)

- `WiFi bulk START host=…`
- `wifi_bulk_session` state transitions
- `WiFi bulk ACK up_to=…`
- `record sync done` on BLE fallback path
