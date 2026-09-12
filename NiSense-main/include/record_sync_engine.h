/* =============================================================================
 * Transport-agnostic Record Sync Engine
 * ============================================================================= */

#ifndef RECORD_SYNC_ENGINE_H
#define RECORD_SYNC_ENGINE_H

#include "record_store.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RECORD_SYNC_MODE_SUMMARY_ONLY 0x01U
#define RECORD_SYNC_MODE_FULL         0x02U

/*
 * Shared logical frame format used by BLE record sync and Wi-Fi bulk sync:
 * record_id:u32 | type:u16 | parent_id:u32 | measurement_id:u32 |
 * payload_len:u16 | payload[]
 */
#define RECORD_SYNC_FRAME_HDR_SIZE 16U
#define RECORD_SYNC_FRAME_MAX_SIZE (RECORD_SYNC_FRAME_HDR_SIZE + RECORD_PAYLOAD_MAX)

typedef int (*record_sync_iter_cb_t)(const uint8_t *frame, uint16_t frame_len,
				     uint32_t record_id, void *user_data);

int record_sync_pack_frame(const struct rec_frame_header *hdr, const uint8_t *payload,
			   size_t payload_len, uint8_t *frame, size_t frame_cap,
			   uint16_t *frame_len);

int record_sync_read_next(uint32_t *cursor_id, uint8_t mode,
			  struct rec_frame_header *hdr, uint8_t *payload,
			  size_t *payload_len);

int record_sync_iterate(uint32_t after_id, uint8_t mode, record_sync_iter_cb_t cb,
			void *user_data, uint32_t *last_id, uint32_t *count);

bool record_sync_mode_valid(uint8_t mode);

#ifdef __cplusplus
}
#endif

#endif /* RECORD_SYNC_ENGINE_H */
