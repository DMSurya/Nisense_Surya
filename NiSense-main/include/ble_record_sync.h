/* =============================================================================
 * BLE Record Sync Service
 * =============================================================================
 * Device → phone bulk pull of the NOR record_store (summaries ± raw chunks).
 *
 * Service:  12345678-1234-5678-1234-56789abcdef4
 *   f401 Control (Write, encrypted)   START / ACK / ABORT
 *   f402 Data    (Notify)             framed records (MTU-fragmented)
 *   f403 Status  (Read/Notify)        struct ble_record_sync_status
 *
 * Flow: START(after_id, mode) → Data notify frames → ACK(up_to_id[, mode]).
 * ACK wire: [cmd=0x02][up_to:u32 LE][mode:u8?] — 6 bytes preferred; legacy
 * 5-byte ACK is treated as FULL reclaim. SUMMARY ACK updates watermark only
 * (never reclaims raw); FULL ACK advances the ring tail.
 * ============================================================================= */

#ifndef BLE_RECORD_SYNC_H
#define BLE_RECORD_SYNC_H

#include <stdint.h>
#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_REC_CMD_START  0x01U
#define BLE_REC_CMD_ACK    0x02U
#define BLE_REC_CMD_ABORT  0x03U

#define BLE_REC_MODE_SUMMARY_ONLY 0x01U
#define BLE_REC_MODE_FULL         0x02U

enum ble_record_sync_state {
	BLE_REC_STATE_IDLE = 0,
	BLE_REC_STATE_SENDING,
	BLE_REC_STATE_DONE,
	BLE_REC_STATE_ERROR,
};

/** Status characteristic payload (f403). */
struct ble_record_sync_status {
	uint8_t  state;       /* enum ble_record_sync_state */
	int8_t   error;       /* 0 ok, negative on failure */
	uint32_t pending;     /* records still unsynced */
	uint32_t cursor_id;   /* last record_id sent (or after_id at start) */
	uint32_t sent_count;  /* records notified in this session */
} __packed;

/**
 * @brief Packed logical record frame (before MTU fragmentation).
 *
 * Wire v2: record_id:u32 | type:u16 | parent_id:u32 | measurement_id:u32 |
 *          payload_len:u16 | payload[]
 */
#define BLE_REC_FRAME_HDR_SIZE 16U

/** Fragment preamble on each Data notify: flags:u8 | offset:u16 | total:u16 */
#define BLE_REC_FRAG_HDR_SIZE  5U
#define BLE_REC_FRAG_MORE      0x01U

void ble_record_sync_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_RECORD_SYNC_H */
