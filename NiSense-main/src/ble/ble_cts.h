/* ble_cts.h – Bluetooth SIG Current Time Service (0x1805) glue */

#ifndef BLE_CTS_H
#define BLE_CTS_H

#ifdef __cplusplus
extern "C" {
#endif

/** Register CTS callbacks that read/write the MCP7940 calendar. */
int ble_cts_init(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_CTS_H */
