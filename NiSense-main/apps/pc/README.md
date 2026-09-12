# HCM BLE Monitor

Qt/QML desktop app for the NiSense HCM wearable over BLE.

**Flow reference:** [docs/ble/BLE_MONITOR_FLOW_GATES.md](../../docs/ble/BLE_MONITOR_FLOW_GATES.md)

## Firmware security profiles

Production builds use **SECURE** (`CONFIG_BLE_SECURITY_PROFILE_SECURE=y` in `conf/features/ble.conf`). Health telemetry and most config chars require an encrypted or MITM-authenticated link.

For local development without pairing, add the OPEN overlay when building:

```powershell
# Example: append to your west/build extra conf
# conf/features/ble_open.conf  →  CONFIG_BLE_SECURITY_PROFILE_OPEN=y
```

The device exposes the active profile on GATT characteristic `f008` (0 = OPEN, 1 = SECURE). The monitor reads this on connect and gates config reads, notification subscriptions, and the GATT explorer accordingly.

## Cloud build profile (Phase 4)

Enable Wi-Fi + MQTT telemetry on the device:

```text
# prj.conf
CONFIG_APP_FEATURE_CLOUD_TELEMETRY=y
```

CMake auto-enables `conf/features/wifi.conf` and `conf/features/cloud.conf`. Keep **SECURE BLE** for WiFi credential provisioning.

Device MQTT settings live in `config.json` on the FAT volume:

```json
"cloud": {
  "enabled": true,
  "mqtt_host": "broker.example.com",
  "mqtt_port": 1883,
  "mqtt_client_id": "HCM-001",
  "mqtt_topic_prefix": "hcm/telemetry",
  "mqtt_username": "",
  "mqtt_password": "",
  "use_ssl": false
}
```

Topics: `{mqtt_topic_prefix}/vitals` and `{mqtt_topic_prefix}/glucose` (compact JSON).

### Monitor gateway upload

Copy `gateway.json.example` to `~/HCM_Logs/gateway.json` and set `rest_url` or `mqtt_host`. Use **Logs → Upload** to POST recent CSV session files.

## Setup

```powershell
cd apps/pc
pip install -r requirements.txt
```

**Windows:** Windows 10 1709+ (WinRT).  
**Linux:** BlueZ 5.43+; user in `bluetooth` group.  
**macOS:** Bleak/CoreBluetooth (experimental; pairing behavior may differ from Windows).

## Run

```powershell
python hcm_monitor.py
python hcm_monitor.py --device D0:7D:94:C6:76:DC
python hcm_monitor.py --debug          # Verbose BLE connect/pairing logs
python hcm_monitor.py --adapter hci0   # Linux only
```

Logs: `~/HCM_Logs/`

## Windows pairing (numeric comparison)

1. Connect (pairing **not** required for the BLE link).
2. On **Windows SECURE**, pairing runs **before any GATT** (OOB `pair_async` via WinRT — same idea as Windows Settings / nRF Connect). Passkey dialog appears when `PairingRequested` fires.
3. Confirm the 6-digit code on **both** screens; **Accept on either device first** (either order works). If the watch Accepts first while connected, PC may auto-confirm via f016; during OOB pair, tap **Accept** on PC when ready.
4. After paired: Bleak connects, full GATT, health streams, WiFi SSID, DFU.

On OPEN-profile dev firmware, the dashboard works immediately without pairing; WiFi SSID and DFU still require pair.

Pairing uses `pair_async` → `PairingRequested` → global dialog in `qml/main.qml`.

## Troubleshooting pairing

| Symptom | Steps |
|---------|--------|
| Disconnect during Windows pair | Expected during clean-session forget; use `--debug` and look for `link phase:` in logs |
| No passkey dialog / `pair_async` busy | On the **watch BLE screen**, tap **Forget** (clears device bonds). Then disconnect, wait 30 s, reconnect, Pair once. Log should show `PairingRequested` after `Starting WinRT pair_async`. Do **not** run two monitor instances. |
| Empty pairing popup flash | Fixed: dialog opens only when the 6-digit code arrives (`pairingPasskey`). |
| `OPERATION_ALREADY_IN_PROGRESS` | Scan → **Unpair All** (device + PC) or Firmware → **Clear PC Bond** → wait 30 s → reconnect → Pair once. Implicit Just Works during GATT is now rejected; if still busy, **Reset BT** or `reset_ble_pairing.py --full-reset` (Admin). **Debug:** `python pair_minimal_test.py <MAC> scan` then `oob-pair` / `connect-pair` — no recovery ladder. |
| Still busy | **Reset BT** or `python reset_ble_pairing.py <MAC> --full-reset` (~45 s). |
| Last resort | Admin PowerShell: `python reset_ble_pairing.py <MAC> --full-reset --restart-bthserv` |

`ALREADY_UNPAIRED` from forget is normal; it does not clear an in-flight ceremony by itself.

## Bond management buttons

| UI label | Page | API | Does |
|----------|------|-----|------|
| **Unpair All** | Scan | `unpairDevice` | Firmware `admin_delete_bonds` + Windows unpair + remove from list |
| **Remove** | Scan | `forgetDevice` | Remove from known-devices list only (no bond clear) |
| **Clear PC Bond** | Firmware | `forgetPairing` | Windows WinRT unpair only (~2 s) |
| **Reset BT** | Firmware | `recoverWindowsPairing` | Clear PC bond + Bluetooth radio bounce (~45 s) |

## DFU over BLE

Firmware page → select `.bin` / signed image → **Flash**. Requires paired link (SMP characteristic is authenticated). See `smp_client.py`.

## Layout

```
hcm_monitor.py      Entry point (Qt + asyncio)
hcm_backend.py      QML backend: connect, pairing, DFU, logging
hcm_client.py       Bleak client; WinRT pairing hook on connect
hcm_protocol.py     UUIDs and GATT decode/encode
winrt_pairing.py    Windows pairing rules (see module docstring)
reset_ble_pairing.py  CLI forget / stack reset
smp_client.py       MCUmgr SMP upload
hcm_logger.py       Session CSV logging
cloud_gateway.py    Optional REST/MQTT log upload (gateway.json)
qml/                UI pages
tests/              Protocol roundtrip + WinRT pairing repro scripts
```

### Tests

```powershell
cd apps/pc
python tests/test_protocol_roundtrip.py
python tests/pair_minimal_test.py D5:C9:63:90:78:40 scan
```

## Development notes

- **Do not** call `pair_async` during connect — handler is attached via `bleak_connect_with_pairing_handler()` before Bleak's internal `get_services()`.
- **Do not** read encrypted config on Windows while unpaired on SECURE profile (`hcm_backend._read_all_config`).
- **Do not** read WiFi SSID on Windows while unpaired (`hcm_backend._read_all_config`).
- Pairing recovery is **manual only** (no startup radio bounce).
- When changing connect/pairing behaviour, update `docs/ble/BLE_MONITOR_FLOW_GATES.md` in the same change.
