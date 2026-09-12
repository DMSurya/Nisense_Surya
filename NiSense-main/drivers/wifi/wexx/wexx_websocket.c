/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * WExx Wi-Fi Module Driver - WebSocket Protocol
 */

#include "wexx_internal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(wexx, CONFIG_WIFI_WEXX_LOG_LEVEL);

/**
 * @brief Initialize WebSocket state
 */
void wexx_ws_init_state(struct wexx_data *data)
{
	k_sem_init(&data->ws_sem, 0, 1);
	data->ws_connected = false;
	data->ws_conn_id = -1;
	data->ws_recv_cb = NULL;
	data->ws_user_data = NULL;
}

/**
 * @brief Handle WebSocket-related events
 */
void wexx_ws_handle_event(const struct device *dev, const char *event_name, const char *params)
{
	struct wexx_data *data = dev->data;

	if (strcmp(event_name, EVT_WSCONNECTED) == 0) {
		/* EVT+WSCONNECTED=<conn_id> */
		int conn_id;
		if (sscanf(params, "%d", &conn_id) == 1) {
			data->ws_conn_id = conn_id;
			data->ws_connected = true;
			LOG_INF("WebSocket connected, conn_id=%d", conn_id);
			k_sem_give(&data->ws_sem);
		}
	} else if (strcmp(event_name, EVT_WSDATA) == 0) {
		/* EVT+WSDATA=<len>,<data> */
		int len;
		char *data_ptr;
		if (sscanf(params, "%d,", &len) == 1) {
			data_ptr = strchr(params, ',');
			if (data_ptr && data->ws_recv_cb) {
				data_ptr++;
				data->ws_recv_cb(dev, (uint8_t *)data_ptr, len, data->ws_user_data);
			}
		}
	} else if (strcmp(event_name, EVT_WSCLOSED) == 0) {
		LOG_INF("WebSocket closed");
		data->ws_connected = false;
	} else if (strcmp(event_name, EVT_WSFAIL) == 0) {
		LOG_ERR("WebSocket connection failed");
		data->ws_connected = false;
		k_sem_give(&data->ws_sem);
	}
}

/**
 * @brief Connect to WebSocket server
 */
int wexx_ws_connect(const struct device *dev, const char *url)
{
	struct wexx_data *data = dev->data;
	char cmd[256];
	
	if (!url) {
		return -EINVAL;
	}
	
	LOG_INF("WebSocket connect to: %s", url);
	
	k_sem_reset(&data->ws_sem);
	data->ws_connected = false;
	
	/* CMD+WSCONNECT=url (no brackets per WExx spec) */
	snprintf(cmd, sizeof(cmd), "%s%s=%s", CMD_PREFIX, CMD_WSCONNECT, url);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("WebSocket connect command failed: %d", ret);
		return -EIO;
	}
	
	ret = k_sem_take(&data->ws_sem, K_SECONDS(15));
	if (ret != 0) {
		LOG_ERR("WebSocket connect timeout");
		return -ETIMEDOUT;
	}
	
	return data->ws_connected ? 0 : -ECONNREFUSED;
}

/**
 * @brief Send data over WebSocket
 */
int wexx_ws_send(const struct device *dev, const uint8_t *data_buf, size_t len)
{
	struct wexx_data *data = dev->data;
	char cmd[512];
	
	if (!data_buf || len == 0) {
		return -EINVAL;
	}
	
	if (!data->ws_connected) {
		LOG_ERR("WebSocket send failed: not connected");
		return -ENOTCONN;
	}
	
	/* CMD+WSSEND=length,data */
	snprintf(cmd, sizeof(cmd), "%s%s=%zu,%.*s",
		 CMD_PREFIX, CMD_WSSEND, len, (int)len, data_buf);
	
	return wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
}

/**
 * @brief Close WebSocket connection
 */
int wexx_ws_close(const struct device *dev)
{
	struct wexx_data *data = dev->data;
	char cmd[64];
	
	LOG_INF("WebSocket close");
	
	snprintf(cmd, sizeof(cmd), "%s%s", CMD_PREFIX, CMD_WSCLOSE);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	
	data->ws_connected = false;
	
	return ret;
}

/**
 * @brief Set WebSocket receive callback
 */
void wexx_ws_set_recv_callback(const struct device *dev, wexx_ws_recv_cb_t cb, void *user_data)
{
	struct wexx_data *data = dev->data;
	data->ws_recv_cb = cb;
	data->ws_user_data = user_data;
}

/**
 * @brief Check if WebSocket is connected
 */
bool wexx_ws_is_connected(const struct device *dev)
{
	struct wexx_data *data = dev->data;
	return data->ws_connected;
}
