/* =============================================================================
 * Wi-Fi Bulk Record Session
 * ============================================================================= */

#ifndef WIFI_BULK_SESSION_H
#define WIFI_BULK_SESSION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/toolchain.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_BULK_CMD_START 0x01U
#define WIFI_BULK_CMD_ACK   0x02U
#define WIFI_BULK_CMD_ABORT 0x03U

#define WIFI_BULK_FLAG_OTA_MODEL    0x0001U
#define WIFI_BULK_FLAG_OTA_RESOURCE 0x0002U
#define WIFI_BULK_FLAG_OTA_FIRMWARE 0x0004U

#define WIFI_BULK_HOST_MAX  64U
#define WIFI_BULK_TOKEN_MAX 96U
#define WIFI_BULK_PATH_MAX  48U

enum wifi_bulk_state {
	WIFI_BULK_STATE_IDLE = 0,
	WIFI_BULK_STATE_PROVISIONING,
	WIFI_BULK_STATE_ASSOCIATING,
	WIFI_BULK_STATE_SESSION_READY,
	WIFI_BULK_STATE_TRANSFERRING,
	WIFI_BULK_STATE_COMPLETING,
	WIFI_BULK_STATE_TEARDOWN,
	WIFI_BULK_STATE_ERROR,
};

struct wifi_bulk_start_params {
	char host[WIFI_BULK_HOST_MAX];
	char token[WIFI_BULK_TOKEN_MAX];
	uint16_t port;
	uint16_t flags;
	uint32_t after_id;
	uint8_t mode;
	bool ssl;
};

struct wifi_bulk_status {
	uint8_t state;
	int8_t error;
	uint32_t pending;
	uint32_t cursor_id;
	uint32_t ack_id;
	uint32_t sent_count;
} __packed;

typedef void (*wifi_bulk_status_cb_t)(const struct wifi_bulk_status *status);
typedef int (*wifi_bulk_asset_write_cb_t)(const uint8_t *data, size_t len, void *user_data);

void wifi_bulk_session_init(void);
void wifi_bulk_session_register_status_cb(wifi_bulk_status_cb_t cb);
int wifi_bulk_session_start(const struct wifi_bulk_start_params *params);
void wifi_bulk_session_abort(void);
void wifi_bulk_session_get_status(struct wifi_bulk_status *status);
/**
 * @brief Reclaim / watermark after bulk pull.
 * @param mode RECORD_ACK_MODE_SUMMARY or RECORD_ACK_MODE_FULL
 *             (0 = use last session mode, defaulting to FULL).
 */
int wifi_bulk_session_ack(uint32_t up_to_id, uint8_t mode);
int wifi_bulk_get_asset(const char *path, wifi_bulk_asset_write_cb_t write_cb, void *user_data);
int wifi_bulk_get_asset_chunked(const char *path, uint32_t total_hint,
				wifi_bulk_asset_write_cb_t write_cb, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_BULK_SESSION_H */
