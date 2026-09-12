/**
 * @file record_sync_engine.c
 * @brief Shared record_store sync selection and frame packing.
 */

#include "record_sync_engine.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <errno.h>
#include <string.h>

LOG_MODULE_REGISTER(record_sync_engine, CONFIG_LOG_DEFAULT_LEVEL);

static uint32_t payload_parent_id(uint8_t type, const uint8_t *payload, size_t len)
{
	if (type == RECORD_TYPE_PPG_RAW && len >= sizeof(struct rec_ppg_raw_hdr)) {
		return ((const struct rec_ppg_raw_hdr *)payload)->parent_id;
	}
	if (type == RECORD_TYPE_GLUCOSE_RAW &&
	    len >= sizeof(struct rec_glucose_raw_hdr)) {
		return ((const struct rec_glucose_raw_hdr *)payload)->parent_id;
	}
	return 0U;
}

static uint32_t payload_measurement_id(uint8_t type, uint8_t version,
				       const uint8_t *payload, size_t len)
{
	if (version < 2U) {
		return 0U;
	}

	switch (type) {
	case RECORD_TYPE_GLUCOSE:
		if (len >= sizeof(struct rec_glucose)) {
			return ((const struct rec_glucose *)payload)->measurement_id;
		}
		break;
	case RECORD_TYPE_VITALS:
		if (len >= sizeof(struct rec_vitals)) {
			return ((const struct rec_vitals *)payload)->measurement_id;
		}
		break;
	case RECORD_TYPE_TEMP:
		if (len >= sizeof(struct rec_temp)) {
			return ((const struct rec_temp *)payload)->measurement_id;
		}
		break;
	case RECORD_TYPE_PPG_RAW:
		if (len >= sizeof(struct rec_ppg_raw_hdr)) {
			return ((const struct rec_ppg_raw_hdr *)payload)->measurement_id;
		}
		break;
	case RECORD_TYPE_GLUCOSE_RAW:
		if (len >= sizeof(struct rec_glucose_raw_hdr)) {
			return ((const struct rec_glucose_raw_hdr *)payload)->measurement_id;
		}
		break;
	default:
		break;
	}
	return 0U;
}

bool record_sync_mode_valid(uint8_t mode)
{
	return mode == RECORD_SYNC_MODE_SUMMARY_ONLY || mode == RECORD_SYNC_MODE_FULL;
}

int record_sync_pack_frame(const struct rec_frame_header *hdr, const uint8_t *payload,
			   size_t payload_len, uint8_t *frame, size_t frame_cap,
			   uint16_t *frame_len)
{
	uint32_t parent;
	uint32_t mid;

	if (hdr == NULL || payload == NULL || frame == NULL || frame_len == NULL) {
		return -EINVAL;
	}
	if (payload_len > RECORD_PAYLOAD_MAX ||
	    frame_cap < RECORD_SYNC_FRAME_HDR_SIZE + payload_len) {
		return -EMSGSIZE;
	}

	parent = payload_parent_id((uint8_t)hdr->type, payload, payload_len);
	mid = payload_measurement_id((uint8_t)hdr->type, hdr->version, payload, payload_len);

	sys_put_le32(hdr->record_id, &frame[0]);
	sys_put_le16((uint16_t)hdr->type, &frame[4]);
	sys_put_le32(parent, &frame[6]);
	sys_put_le32(mid, &frame[10]);
	sys_put_le16((uint16_t)payload_len, &frame[14]);
	memcpy(&frame[RECORD_SYNC_FRAME_HDR_SIZE], payload, payload_len);
	*frame_len = (uint16_t)(RECORD_SYNC_FRAME_HDR_SIZE + payload_len);

	return 0;
}

int record_sync_read_next(uint32_t *cursor_id, uint8_t mode,
			  struct rec_frame_header *hdr, uint8_t *payload,
			  size_t *payload_len)
{
	uint32_t cursor;
	int rc;

	if (cursor_id == NULL || hdr == NULL || payload == NULL || payload_len == NULL) {
		return -EINVAL;
	}
	if (!record_sync_mode_valid(mode)) {
		return -EINVAL;
	}

	cursor = *cursor_id;
	for (;;) {
		rc = record_store_read_next(cursor, hdr, payload, payload_len);
		if (rc != 0) {
			return rc;
		}

		cursor = hdr->record_id;
		*cursor_id = cursor;

		if (mode == RECORD_SYNC_MODE_FULL || RECORD_TYPE_IS_SUMMARY(hdr->type)) {
			return 0;
		}
	}
}

int record_sync_iterate(uint32_t after_id, uint8_t mode, record_sync_iter_cb_t cb,
			void *user_data, uint32_t *last_id, uint32_t *count)
{
	uint8_t frame[RECORD_SYNC_FRAME_MAX_SIZE];
	uint8_t payload[RECORD_PAYLOAD_MAX];
	uint32_t cursor = after_id;
	uint32_t sent = 0U;

	if (cb == NULL) {
		return -EINVAL;
	}
	if (!record_sync_mode_valid(mode)) {
		return -EINVAL;
	}

	for (;;) {
		struct rec_frame_header hdr;
		size_t payload_len = 0U;
		uint16_t frame_len = 0U;
		int rc = record_sync_read_next(&cursor, mode, &hdr, payload, &payload_len);

		if (rc != 0) {
			if (rc == -ENOENT) {
				break;
			}
			return rc;
		}

		rc = record_sync_pack_frame(&hdr, payload, payload_len, frame, sizeof(frame),
					    &frame_len);
		if (rc != 0) {
			return rc;
		}

		rc = cb(frame, frame_len, hdr.record_id, user_data);
		if (rc != 0) {
			return rc;
		}
		sent++;
	}

	if (last_id != NULL) {
		*last_id = cursor;
	}
	if (count != NULL) {
		*count = sent;
	}
	return 0;
}
