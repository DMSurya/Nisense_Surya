/* =============================================================================
 * BLE Resources Transfer Service
 * =============================================================================
 * Custom GATT service (UUID base ...def5) for over-the-air update of LVGL
 * fonts/icons/logos into the inactive resource A/B slot.
 *
 * Service:  12345678-1234-5678-1234-56789abcdef5
 *   f501 Control (Write, encrypted)   BEGIN / COMMIT / ABORT
 *   f502 Data    (Write, encrypted)   raw image chunks
 *   f503 Status  (Read/Notify)        struct ble_resource_status
 *
 * Flow: BEGIN(total) -> erase inactive slot -> stream -> COMMIT -> validate
 * magic/CRC/ota_compat -> flip active slot (NVS).
 * ============================================================================= */

#ifndef BLE_RESOURCE_TRANSFER_H
#define BLE_RESOURCE_TRANSFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_RESOURCE_CMD_BEGIN   0x01U
#define BLE_RESOURCE_CMD_COMMIT  0x02U
#define BLE_RESOURCE_CMD_ABORT   0x03U

enum ble_resource_state {
	BLE_RESOURCE_STATE_IDLE = 0,
	BLE_RESOURCE_STATE_ERASING,
	BLE_RESOURCE_STATE_RECEIVING,
	BLE_RESOURCE_STATE_READY,
	BLE_RESOURCE_STATE_COMMITTED,
	BLE_RESOURCE_STATE_ERROR,
};

struct ble_resource_status {
	uint8_t  state;
	int8_t   error;
	uint32_t received;
	uint32_t total;
	uint32_t active_version; /* partition header.version of active slot */
	uint8_t  active_slot;
} __packed;

void ble_resource_transfer_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_RESOURCE_TRANSFER_H */
