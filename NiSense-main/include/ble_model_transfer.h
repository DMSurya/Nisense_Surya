/* =============================================================================
 * BLE Glucose Model Transfer Service
 * =============================================================================
 * Custom GATT service (UUID base ...def3) for over-the-air update of the
 * glucose elimination-matrix model into the secondary A/B slot, with CRC and
 * optional Ed25519 signature verification before the active slot is flipped.
 *
 * Service:  12345678-1234-5678-1234-56789abcdef3
 *   f301 Control (Write, encrypted)   BEGIN / COMMIT / ABORT
 *   f302 Data    (Write, encrypted)   raw image chunks (appended)
 *   f303 Status  (Read/Notify)        struct ble_model_status
 *
 * Flow: BEGIN(total,version,variant) -> erase *inactive* slot -> stream chunks to
 * f302 -> COMMIT(signature) -> validate CRC + ota_compat + signature -> flip
 * active slot -> re-init algorithm. ABORT resets state at any time.
 * ============================================================================= */

#ifndef BLE_MODEL_TRANSFER_H
#define BLE_MODEL_TRANSFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Control characteristic command bytes (first byte of a write to f301). */
#define BLE_MODEL_CMD_BEGIN   0x01U
#define BLE_MODEL_CMD_COMMIT  0x02U
#define BLE_MODEL_CMD_ABORT   0x03U

/** Transfer state (reported in ble_model_status.state). */
enum ble_model_state {
	BLE_MODEL_STATE_IDLE = 0,
	BLE_MODEL_STATE_ERASING,
	BLE_MODEL_STATE_RECEIVING,
	BLE_MODEL_STATE_READY,      /* all bytes received, awaiting COMMIT */
	BLE_MODEL_STATE_COMMITTED,
	BLE_MODEL_STATE_ERROR,
};

/** Status characteristic payload (f303, Read/Notify). */
struct ble_model_status {
	uint8_t  state;          /* enum ble_model_state */
	int8_t   error;          /* 0 ok, negative on failure */
	uint32_t received;       /* bytes written to secondary slot */
	uint32_t total;          /* expected total bytes */
	uint32_t active_version; /* model_version of the active slot */
	uint8_t  active_slot;    /* GLUCOSE_MODEL_SLOT_* currently active */
} __packed;

/** Reset transfer state (called on disconnect). */
void ble_model_transfer_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_MODEL_TRANSFER_H */
