/**
 * @file wifi_bulk_session.c
 * @brief BLE-triggered Wi-Fi bulk transfer of packed record frames.
 */

#include "wifi_bulk_session.h"
#include "config_manager.h"
#include "http_client.h"
#include "record_store.h"
#include "record_sync_engine.h"
#include "wifi_ota_transfer.h"

#include <zephyr/device.h>
#include <zephyr/drivers/wifi/wexx.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(wifi_bulk_session, CONFIG_LOG_DEFAULT_LEVEL);

#define WIFI_BULK_THREAD_STACK_SIZE 3072
#define WIFI_BULK_THREAD_PRIORITY   8
#define WIFI_BULK_POST_PATH         "/sync/records"
#define WIFI_BULK_ASSOC_TIMEOUT_MS  30000
#define WIFI_BULK_POST_BODY_MAX     704U
#define WIFI_BULK_RESP_MAX          128U
#define WIFI_BULK_ASSET_RESP_MAX    512U
#define WIFI_BULK_ASSET_CHUNK_MAX   240U
#define WIFI_BULK_OTA_CHUNK_PATH_MAX 96U

static struct wifi_bulk_start_params g_params;
static struct wifi_bulk_status g_status;
static wifi_bulk_status_cb_t g_status_cb;
static K_MUTEX_DEFINE(g_lock);
static atomic_t g_abort;
static bool g_started;

static struct k_thread g_thread;
static k_tid_t g_tid;
static K_THREAD_STACK_DEFINE(g_stack, WIFI_BULK_THREAD_STACK_SIZE);

static uint8_t g_frame[RECORD_SYNC_FRAME_MAX_SIZE];
static uint8_t g_payload[RECORD_PAYLOAD_MAX];
/* POST body and OTA hex scratch are mutually exclusive in the session thread. */
static union {
	char body[WIFI_BULK_POST_BODY_MAX];
	char asset[WIFI_BULK_ASSET_RESP_MAX];
} g_xfer;
static char g_response[WIFI_BULK_RESP_MAX];

#define g_body           (g_xfer.body)
#define g_asset_response (g_xfer.asset)

static void publish_status_locked(void)
{
	struct wifi_bulk_status snapshot = g_status;
	wifi_bulk_status_cb_t cb = g_status_cb;

	k_mutex_unlock(&g_lock);
	if (cb != NULL) {
		cb(&snapshot);
	}
	k_mutex_lock(&g_lock, K_FOREVER);
}

static void set_status(uint8_t state, int err)
{
	k_mutex_lock(&g_lock, K_FOREVER);
	g_status.state = state;
	g_status.error = (int8_t)((err < -128) ? -128 : err);
	g_status.pending = record_store_pending();
	publish_status_locked();
	k_mutex_unlock(&g_lock);
}

static void update_progress(uint32_t cursor_id, uint32_t ack_id, uint32_t sent_count)
{
	k_mutex_lock(&g_lock, K_FOREVER);
	g_status.cursor_id = cursor_id;
	g_status.ack_id = ack_id;
	g_status.sent_count = sent_count;
	g_status.pending = record_store_pending();
	publish_status_locked();
	k_mutex_unlock(&g_lock);
}

void wifi_bulk_session_init(void)
{
	k_mutex_lock(&g_lock, K_FOREVER);
	memset(&g_params, 0, sizeof(g_params));
	memset(&g_status, 0, sizeof(g_status));
	g_status.state = WIFI_BULK_STATE_IDLE;
	g_status.pending = record_store_pending();
	atomic_set(&g_abort, 0);
	g_started = true;
	k_mutex_unlock(&g_lock);
}

void wifi_bulk_session_register_status_cb(wifi_bulk_status_cb_t cb)
{
	k_mutex_lock(&g_lock, K_FOREVER);
	g_status_cb = cb;
	k_mutex_unlock(&g_lock);
}

void wifi_bulk_session_get_status(struct wifi_bulk_status *status)
{
	if (status == NULL) {
		return;
	}
	k_mutex_lock(&g_lock, K_FOREVER);
	*status = g_status;
	k_mutex_unlock(&g_lock);
}

static int wifi_connect_configured(void)
{
	const struct device *wifi_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(wifi0));
	const struct config_wifi *wifi_cfg;
	int64_t deadline;

	if (wifi_dev == NULL || !device_is_ready(wifi_dev)) {
		return -ENODEV;
	}

	if (wexx_is_connected(wifi_dev)) {
		return 0;
	}

	wifi_cfg = config_get_wifi();
	if (wifi_cfg == NULL || !wifi_cfg->enabled || wifi_cfg->ssid[0] == '\0') {
		return -ENOTCONN;
	}

	set_status(WIFI_BULK_STATE_ASSOCIATING, 0);
	{
		enum wifi_security_type security = (wifi_cfg->password[0] == '\0') ?
			WIFI_SECURITY_TYPE_NONE : WIFI_SECURITY_TYPE_PSK;
		int rc = wexx_connect(wifi_dev, wifi_cfg->ssid, strlen(wifi_cfg->ssid),
				      wifi_cfg->password, strlen(wifi_cfg->password),
				      security);

		if (rc != 0 && rc != -EALREADY) {
			return rc;
		}
	}

	deadline = k_uptime_get() + WIFI_BULK_ASSOC_TIMEOUT_MS;
	while (k_uptime_get() < deadline) {
		if (atomic_get(&g_abort) != 0) {
			return -ECANCELED;
		}
		if (wexx_is_connected(wifi_dev)) {
			return 0;
		}
		k_sleep(K_MSEC(250));
	}

	return -ETIMEDOUT;
}

static void wifi_disconnect_policy(void)
{
	const struct device *wifi_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(wifi0));

	if (wifi_dev != NULL && device_is_ready(wifi_dev)) {
		(void)wexx_disconnect(wifi_dev);
	}
}

static void hex_encode(const uint8_t *src, size_t src_len, char *dst, size_t dst_len)
{
	static const char hex[] = "0123456789abcdef";
	size_t out = 0U;

	for (size_t i = 0U; i < src_len && out + 2U < dst_len; i++) {
		dst[out++] = hex[src[i] >> 4];
		dst[out++] = hex[src[i] & 0x0fU];
	}
	dst[out] = '\0';
}

static int parse_ack(const char *response, uint32_t *ack_id)
{
	const char *p;
	unsigned long value;

	if (response == NULL || ack_id == NULL) {
		return -EINVAL;
	}

	p = strstr(response, "ACK ");
	if (p != NULL) {
		p += 4;
	} else {
		p = strstr(response, "\"up_to_id\"");
		if (p == NULL) {
			p = strstr(response, "\"ack\"");
		}
		if (p == NULL) {
			return -ENOENT;
		}
		p = strchr(p, ':');
		if (p == NULL) {
			return -ENOENT;
		}
		p++;
	}

	while (*p == ' ' || *p == '\t') {
		p++;
	}
	value = strtoul(p, NULL, 10);
	if (value == 0UL && *p != '0') {
		return -EINVAL;
	}
	*ack_id = (uint32_t)value;
	return 0;
}

static int post_frame(const struct wifi_bulk_start_params *params, const uint8_t *frame,
		      uint16_t frame_len, uint32_t *ack_id)
{
	char *hex;
	int body_len;
	int http_status;

	body_len = snprintf(g_body, sizeof(g_body), "token=%s&count=1&frame=",
			    params->token);
	if (body_len < 0 || body_len >= (int)sizeof(g_body)) {
		return -EMSGSIZE;
	}
	hex = &g_body[body_len];
	hex_encode(frame, frame_len, hex, sizeof(g_body) - (size_t)body_len);

	memset(g_response, 0, sizeof(g_response));
	http_status = http_client_post(params->host, WIFI_BULK_POST_PATH, params->port,
				       params->ssl, g_body, g_response,
				       sizeof(g_response));
	if (http_status < 200 || http_status >= 300) {
		return (http_status < 0) ? http_status : -EIO;
	}

	return parse_ack(g_response, ack_id);
}

static void session_thread_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	struct wifi_bulk_start_params params;
	uint32_t cursor;
	uint32_t sent = 0U;
	uint32_t ack = 0U;
	int rc;

	k_mutex_lock(&g_lock, K_FOREVER);
	params = g_params;
	cursor = params.after_id;
	g_status.cursor_id = cursor;
	g_status.ack_id = 0U;
	g_status.sent_count = 0U;
	publish_status_locked();
	k_mutex_unlock(&g_lock);

	atomic_set(&g_abort, 0);
	set_status(WIFI_BULK_STATE_PROVISIONING, 0);

	rc = wifi_connect_configured();
	if (rc != 0) {
		set_status(WIFI_BULK_STATE_ERROR, rc);
		goto out;
	}

	rc = http_client_init();
	if (rc != 0) {
		set_status(WIFI_BULK_STATE_ERROR, rc);
		goto out;
	}

	set_status(WIFI_BULK_STATE_SESSION_READY, 0);
	set_status(WIFI_BULK_STATE_TRANSFERRING, 0);

	while (atomic_get(&g_abort) == 0) {
		struct rec_frame_header hdr;
		size_t payload_len = 0U;
		uint16_t frame_len = 0U;

		rc = record_sync_read_next(&cursor, params.mode, &hdr, g_payload,
					   &payload_len);
		if (rc != 0) {
			if (rc == -ENOENT) {
				rc = 0;
			}
			break;
		}

		rc = record_sync_pack_frame(&hdr, g_payload, payload_len, g_frame,
					    sizeof(g_frame), &frame_len);
		if (rc != 0) {
			break;
		}

		rc = post_frame(&params, g_frame, frame_len, &ack);
		if (rc != 0) {
			break;
		}

		sent++;
		update_progress(cursor, ack, sent);
	}

	if (atomic_get(&g_abort) != 0) {
		set_status(WIFI_BULK_STATE_TEARDOWN, 0);
		goto out;
	}

	if (rc == 0) {
		update_progress(cursor, ack, sent);

		if ((params.flags & (WIFI_BULK_FLAG_OTA_MODEL | WIFI_BULK_FLAG_OTA_RESOURCE |
				     WIFI_BULK_FLAG_OTA_FIRMWARE)) != 0U) {
			/* Stay in TRANSFERRING so the phone keeps the HTTP server up. */
			set_status(WIFI_BULK_STATE_TRANSFERRING, 0);
			int ota_rc = wifi_ota_apply_requested(params.flags);

			if (ota_rc != 0) {
				LOG_WRN("Wi-Fi OTA apply soft-failed: %d (records still synced)",
					ota_rc);
			}
		}

		set_status(WIFI_BULK_STATE_COMPLETING, 0);
		update_progress(cursor, ack, sent);
	} else {
		set_status(WIFI_BULK_STATE_ERROR, rc);
	}

out:
	wifi_disconnect_policy();
	k_mutex_lock(&g_lock, K_FOREVER);
	if (g_status.state != WIFI_BULK_STATE_ERROR) {
		g_status.state = WIFI_BULK_STATE_IDLE;
		g_status.error = 0;
	}
	publish_status_locked();
	g_tid = NULL;
	k_mutex_unlock(&g_lock);
}

int wifi_bulk_session_start(const struct wifi_bulk_start_params *params)
{
	if (params == NULL || params->host[0] == '\0' || params->port == 0U ||
	    !record_sync_mode_valid(params->mode)) {
		return -EINVAL;
	}

	if (!g_started) {
		wifi_bulk_session_init();
	}

	k_mutex_lock(&g_lock, K_FOREVER);
	if (g_tid != NULL) {
		k_mutex_unlock(&g_lock);
		return -EBUSY;
	}

	g_params = *params;
	g_params.host[sizeof(g_params.host) - 1U] = '\0';
	g_params.token[sizeof(g_params.token) - 1U] = '\0';
	g_status.state = WIFI_BULK_STATE_IDLE;
	g_status.error = 0;
	g_status.pending = record_store_pending();
	g_status.cursor_id = params->after_id;
	g_status.ack_id = 0U;
	g_status.sent_count = 0U;

	g_tid = k_thread_create(&g_thread, g_stack, K_THREAD_STACK_SIZEOF(g_stack),
				session_thread_fn, NULL, NULL, NULL,
				WIFI_BULK_THREAD_PRIORITY, 0, K_NO_WAIT);
	k_thread_name_set(g_tid, "wifi_bulk");
	k_mutex_unlock(&g_lock);

	return 0;
}

void wifi_bulk_session_abort(void)
{
	atomic_set(&g_abort, 1);
}

int wifi_bulk_session_ack(uint32_t up_to_id, uint8_t mode)
{
	uint8_t ack_mode = mode;
	int rc;

	if (ack_mode == 0U) {
		ack_mode = g_params.mode;
	}
	if (ack_mode != RECORD_ACK_MODE_SUMMARY &&
	    ack_mode != RECORD_ACK_MODE_FULL) {
		ack_mode = RECORD_ACK_MODE_FULL;
	}

	rc = record_store_ack_synced(up_to_id, ack_mode);
	if (rc == 0) {
		update_progress(up_to_id, up_to_id, g_status.sent_count);
	}
	return rc;
}

int wifi_bulk_get_asset(const char *path, wifi_bulk_asset_write_cb_t write_cb, void *user_data)
{
	return wifi_bulk_get_asset_chunked(path, 0U, write_cb, user_data);
}

int wifi_bulk_get_asset_chunked(const char *path, uint32_t total_hint,
				wifi_bulk_asset_write_cb_t write_cb, void *user_data)
{
	struct wifi_bulk_start_params params;
	char chunk_path[WIFI_BULK_OTA_CHUNK_PATH_MAX];
	uint32_t offset = 0U;
	int http_status;

	if (path == NULL || path[0] != '/' || write_cb == NULL) {
		return -EINVAL;
	}
	if (strlen(path) >= WIFI_BULK_PATH_MAX) {
		return -EMSGSIZE;
	}

	k_mutex_lock(&g_lock, K_FOREVER);
	params = g_params;
	k_mutex_unlock(&g_lock);

	if (params.host[0] == '\0' || params.port == 0U || params.token[0] == '\0') {
		return -ENOTCONN;
	}

	http_status = wifi_connect_configured();
	if (http_status != 0) {
		return http_status;
	}

	http_status = http_client_init();
	if (http_status != 0) {
		return http_status;
	}

	do {
		int n = snprintf(chunk_path, sizeof(chunk_path),
				 "%s?token=%s&offset=%u&len=%u&encoding=hex", path,
				 params.token, offset, WIFI_BULK_ASSET_CHUNK_MAX);

		if (n < 0 || n >= (int)sizeof(chunk_path)) {
			return -EMSGSIZE;
		}

		memset(g_asset_response, 0, sizeof(g_asset_response));
		http_status = http_client_get(params.host, chunk_path, params.port,
					      params.ssl, g_asset_response,
					      sizeof(g_asset_response));
		if (http_status < 200 || http_status >= 300) {
			return (http_status < 0) ? http_status : -EIO;
		}

		{
			size_t hex_len = strnlen(g_asset_response, sizeof(g_asset_response));
			size_t bin_len;
			int wr;

			if (hex_len == 0U) {
				break;
			}
			if ((hex_len & 1U) != 0U) {
				return -EINVAL;
			}
			bin_len = hex_len / 2U;
			if (bin_len > sizeof(g_frame)) {
				return -EMSGSIZE;
			}
			for (size_t i = 0U; i < bin_len; i++) {
				unsigned int byte = 0U;

				if (sscanf(&g_asset_response[i * 2U], "%2x", &byte) != 1) {
					return -EINVAL;
				}
				g_frame[i] = (uint8_t)byte;
			}
			wr = write_cb(g_frame, bin_len, user_data);
			if (wr != 0) {
				return wr;
			}
			offset += (uint32_t)bin_len;
			if (bin_len < WIFI_BULK_ASSET_CHUNK_MAX) {
				break;
			}
			if (total_hint != 0U && offset >= total_hint) {
				break;
			}
		}
	} while (atomic_get(&g_abort) == 0);

	return (atomic_get(&g_abort) != 0) ? -ECANCELED : 0;
}
