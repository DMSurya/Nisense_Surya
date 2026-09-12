# Wi-Fi Integration Guide

**Status**: ✅ Implemented
**Last Updated**: 2026-05-26
**Platform**: nRF52840 + WExx Wi-Fi Module (WE10/WE20D)

## Overview

The device integrates Wi-Fi connectivity via the **WExx Wi-Fi module** (WE10 or WE20D), which communicates with the nRF52840 over UART using AT commands. Wi-Fi is managed through the standard Zephyr Wi-Fi management API (`net_mgmt`) and integrated with Zephyr's networking stack.

## Hardware Setup

### WExx Module
- **Module**: WE10 or WE20D (2.4GHz dual-mode Wi-Fi + optional Bluetooth)
- **Interface**: UART (AT commands)
- **UART Configuration**:
  - **UART Controller**: UART0 (P0.06 TX, P0.08 RX)
  - Baud Rate: 38400
  - Data Bits: 8
  - Stop Bits: 1
  - Parity: None
  - Flow Control: None (hardware flow control not enabled)
- **GPIO Connections**:
  - `enable-gpios` → GPIO0 pin 5 (single module enable, active high)

### Device Tree Configuration

Located in: `boards/raytac_mdbt50q_db_40_nrf52840.overlay`

```devicetree
/* boards/raytac_overlay/62_uart_wifi.overlayinc */
&uart0 {
    status = "okay";
    compatible = "nordic,nrf-uarte";
    current-speed = <38400>;
    pinctrl-0 = <&uart0_wifi>;
    pinctrl-names = "default", "sleep";
};

&{/} {
    wexx_wifi: wexx-wifi {
        status = "okay";
        compatible = "wexx";
        uart = <&uart0>;
        current-speed = <38400>;
        enable-gpios = <&gpio0 5 GPIO_ACTIVE_HIGH>;
        /* hw-flow-control; (not enabled) */
    };
};
```

## Software Stack

### Drivers
- **OOT Driver**: `drivers/wifi/wexx/` - Zephyr Wi-Fi L2 driver
- **Files**:
  - `wexx.c` - Main driver implementation (AT command layer)
  - `wexx_priv.h` - Private definitions
  - `wexx_mgmt.c` - Management request handlers
  - `wexx_event.c` - Event notifications

### Configuration
**In `prj.conf`:**
```kconfig
# Wi-Fi Module (WExx)
CONFIG_WIFI_WEXX=y
CONFIG_WIFI_WEXX_RX_BUF_SIZE=1536
CONFIG_WIFI_WEXX_TX_BUF_SIZE=512
CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS=5000
CONFIG_WIFI_WEXX_CONNECT_TIMEOUT_MS=30000

# Zephyr Networking
CONFIG_NET_L2_WIFI_MGMT=y
CONFIG_NET_STATISTICS=y
CONFIG_NET_SHELL=n
CONFIG_DHCPV4=y
```

### UI Integration
**File**: `src/ui/wifi_ui.c` / `src/ui/wifi_ui.h`

Provides LVGL-based UI screen for:
- Connection status (connected/disconnected/scanning)
- Signal strength (RSSI in dBm)
- SSID display
- IP address (DHCP)
- MAC address
- Available networks list (from scan results)
- Connect/disconnect buttons
- Network scan functionality

## API Usage

### Standard Zephyr Wi-Fi API

All operations use the standard Zephyr `net_mgmt()` API:

#### 1. Get Network Interface
```c
#include <zephyr/net/net_if.h>

struct net_if *iface = net_if_get_wifi(0);
if (!iface) {
    LOG_ERR("Wi-Fi interface not found");
    return -ENODEV;
}
```

#### 2. Scan for Networks
```c
#include <zephyr/net/wifi_mgmt.h>

int ret = net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0);
if (ret != 0) {
    LOG_ERR("Scan failed: %d", ret);
}
```

Scan results are delivered asynchronously via `NET_EVENT_WIFI_SCAN_RESULT` events. Scan completion is signaled by `NET_EVENT_WIFI_SCAN_DONE`.

#### 3. Connect to Network
```c
struct wifi_connect_req_params params = {
    .ssid = (uint8_t *)"MySSID",
    .ssid_length = strlen("MySSID"),
    .psk = (uint8_t *)"MyPassword",
    .psk_length = strlen("MyPassword"),
    .security = WIFI_SECURITY_WPA2,
    .mfp = WIFI_MFP_OPTIONAL,
    .timeout = 30000  // 30 seconds
};

int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
if (ret != 0) {
    LOG_ERR("Connect request failed: %d", ret);
}
```

Connection result delivered via `NET_EVENT_WIFI_CONNECT_RESULT` event.

#### 4. Disconnect
```c
int ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
if (ret != 0) {
    LOG_ERR("Disconnect failed: %d", ret);
}
```

#### 5. Get Interface Status
```c
struct wifi_iface_status status = {0};

int ret = net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status));
if (ret != 0) {
    LOG_ERR("Status query failed: %d", ret);
    return;
}

LOG_INF("State: %d, SSID: %.*s, RSSI: %d dBm",
        status.state,
        status.ssid_len, status.ssid,
        status.rssi);
```

**Status States:**
- `WIFI_STATE_DISCONNECTED` = 0
- `WIFI_STATE_SCANNING` = 1
- `WIFI_STATE_AUTHENTICATING` = 2
- `WIFI_STATE_ASSOCIATING` = 3
- `WIFI_STATE_ASSOCIATED` = 4
- `WIFI_STATE_COMPLETED` = 5 (ready for IP access)

### Event Handling

Register a net_mgmt event callback to receive Wi-Fi events:

```c
#include <zephyr/net/net_event.h>

static struct net_mgmt_event_callback wifi_cb;

static void wifi_event_handler(struct net_mgmt_event_callback *cb,
                               uint32_t mgmt_event,
                               struct net_if *iface)
{
    switch (mgmt_event) {
    case NET_EVENT_WIFI_SCAN_RESULT: {
        struct wifi_scan_result *scan_res = (struct wifi_scan_result *)cb->info;
        LOG_INF("Scan: SSID=%.*s, RSSI=%d dBm, Security=%d",
                scan_res->ssid_length, scan_res->ssid,
                scan_res->rssi, scan_res->security);
        break;
    }
    case NET_EVENT_WIFI_SCAN_DONE:
        LOG_INF("Scan complete");
        break;
    case NET_EVENT_WIFI_CONNECT_RESULT:
        LOG_INF("Connect result received");
        break;
    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        LOG_INF("Disconnected");
        break;
    default:
        break;
    }
}

void wifi_init(void)
{
    net_mgmt_init_event_callback(&wifi_cb, wifi_event_handler,
                                  NET_EVENT_WIFI_SCAN_RESULT |
                                  NET_EVENT_WIFI_SCAN_DONE |
                                  NET_EVENT_WIFI_CONNECT_RESULT |
                                  NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&wifi_cb);
}
```

## Configuration Storage

Wi-Fi configuration (SSID, PSK, security type) is persisted in:
- **File**: `src/storage/config_manager.c` / `include/config_manager.h`
- **Storage**: NVS / Zephyr settings (external `storage_nvs` + internal settings) — **not** `/NAND:/config.json` (FatFS removed)
- **API**:
  ```c
  struct wifi_config {
      char ssid[32];
      char psk[64];
      uint32_t security;
  };
  
  struct wifi_config cfg = config_get_wifi();
  cfg.security = WIFI_SECURITY_WPA2;
  strcpy(cfg.ssid, "NewSSID");
  strcpy(cfg.psk, "NewPassword");
  config_set_wifi(&cfg);
  ```

## Implementation Status

### ✓ Implemented
- WExx UART driver with AT command layer
- Zephyr Wi-Fi L2 integration (net_mgmt API)
- Scan functionality
- Connection/disconnection
- Event callbacks
- Configuration storage (JSON)
- LVGL UI screen (`wifi_ui.c`)
- Signal strength monitoring (RSSI)
- DHCP support

### Not Currently Implemented
- WPA3 security support (depends on WExx firmware support)
- Static IP configuration workflow
- Power saving modes (sleep, wake-on-WLAN)
- AP mode / SoftAP product path (driver optional; not used)

### Dual-transport bulk sync (protocol 3.2)

When `CONFIG_APP_FEATURE_WIFI=y` (lean STA + HTTP), BLE `f206`/`f207` negotiate a
phone-hosted HTTP session and the device POSTs packed NOR records to
`/sync/records`. See [`docs/architecture/BLE_WIFI_DUAL_TRANSPORT.md`](../architecture/BLE_WIFI_DUAL_TRANSPORT.md).
OTA over Wi-Fi uses chunked hex GET (`/ota/*`); large images may still use BLE
SMP / model / resource paths.

## Troubleshooting

### Module Not Detected
1. **Check UART pins** in board overlay
2. **Verify baud rate**: Should be 115200
3. **Check power**: WExx module requires 3.3V, verify PWR_EN GPIO
4. **Enable debug logging**:
   ```kconfig
   CONFIG_LOG_DEFAULT_LEVEL=4
   CONFIG_WIFI_WEXX_LOG_LEVEL_DBG=y
   ```

### Connection Failures
1. **Check configuration**: Verify SSID/PSK accuracy
2. **Check security type**: Ensure correct `WIFI_SECURITY_*` constant
3. **Check signal strength**: RSSI should be > -80 dBm
4. **Check timeout**: May need to increase `CONFIG_WIFI_WEXX_CONNECT_TIMEOUT_MS`
5. **Check DHCP**: Ensure `CONFIG_DHCPV4=y` is enabled

### Scan Issues
1. **Check module firmware**: Some older WExx firmware may have scan issues
2. **Increase timeout**: Try `CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS=10000`
3. **Reduce buffer sizes**: Some memory constraints may require tuning `RX_BUF_SIZE`

## Performance Notes

- **Memory**: ~800 bytes RAM for driver state
- **Flash**: WExx driver ~3-4 KB code
- **Scan Time**: Typically 2-5 seconds (depends on environment)
- **Connect Time**: 5-30 seconds (depends on network)
- **Power**: ~100-150 mA during activity, <1 mA when configured to sleep

## References

- **Zephyr Wi-Fi API**: https://docs.zephyrproject.org/latest/connectivity/networking/api/wifi.html
- **WExx Module Datasheet**: Manufacturer documentation (UART AT command set)
- **nRF Connect SDK**: https://developer.nordicsemi.com/nRF_Connect_SDK/

## Related Files

| File | Purpose |
|------|---------|
| `drivers/wifi/wexx/wexx.c` | UART + AT command implementation |
| `src/ui/wifi_ui.c` | LVGL UI screen for Wi-Fi management |
| `src/storage/config_manager.c` | Persistent Wi-Fi configuration storage |
| `boards/*.overlay` | UART + GPIO configuration |
| `prj.conf` | Feature enables and timeouts |


