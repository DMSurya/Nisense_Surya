/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * WExx Wi-Fi Module Driver - AP Mode & UDP Server
 */

#include "wexx_internal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(wexx, CONFIG_WIFI_WEXX_LOG_LEVEL);

/**
 * @brief Initialize AP mode state
 */
void wexx_ap_init_state(struct wexx_data *data)
{
	data->ap_enabled = false;
	data->ap_sta_count = 0;
	data->udp_server_running = false;
	data->udp_server_port = 0;
	data->udp_server_cb = NULL;
	data->udp_server_user_data = NULL;
}

/**
 * @brief Handle AP mode related events
 */
void wexx_ap_handle_event(const struct device *dev, const char *event_name, const char *params)
{
	struct wexx_data *data = dev->data;

	if (strcmp(event_name, EVT_STACON) == 0) {
		/* Station connected to our AP */
		data->ap_sta_count++;
		LOG_INF("Station connected to AP (total: %d)", data->ap_sta_count);
	} else if (strcmp(event_name, EVT_STADISCON) == 0) {
		/* Station disconnected from our AP */
		if (data->ap_sta_count > 0) {
			data->ap_sta_count--;
		}
		LOG_INF("Station disconnected from AP (total: %d)", data->ap_sta_count);
	}
}

/**
 * @brief Configure and enable AP mode
 */
int wexx_ap_enable(const struct device *dev, const char *ssid,
		   const char *password, uint8_t channel, uint8_t max_clients)
{
	struct wexx_data *data = dev->data;
	char cmd[256];
	
	if (!ssid) {
		return -EINVAL;
	}
	
	LOG_INF("AP enable: SSID=%s, ch=%d", ssid, channel);
	
	/* First set to AP or concurrent mode */
	int ret = wexx_set_mode(dev, WEXX_MODE_CONCURRENT);
	if (ret != 0) {
		LOG_ERR("Failed to set concurrent mode: %d", ret);
		return ret;
	}
	
	/* CMD+SETAP=ssid,password,channel,max_clients */
	if (password && strlen(password) > 0) {
		snprintf(cmd, sizeof(cmd), "%s%s=%s,%s,%d,%d",
			 CMD_PREFIX, CMD_SETAP, ssid, password, channel, max_clients);
	} else {
		snprintf(cmd, sizeof(cmd), "%s%s=%s,,%d,%d",
			 CMD_PREFIX, CMD_SETAP, ssid, channel, max_clients);
	}
	
	ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret == 0) {
		data->ap_enabled = true;
	}
	
	return ret;
}

/**
 * @brief Disable AP mode
 */
int wexx_ap_disable(const struct device *dev)
{
	struct wexx_data *data = dev->data;
	
	LOG_INF("AP disable");
	
	/* Switch back to station mode only */
	int ret = wexx_set_mode(dev, WEXX_MODE_STA);
	if (ret == 0) {
		data->ap_enabled = false;
		data->ap_sta_count = 0;
	}
	
	return ret;
}

/**
 * @brief Get number of connected stations
 */
int wexx_ap_get_sta_count(const struct device *dev)
{
	struct wexx_data *data = dev->data;
	return data->ap_sta_count;
}

/* ===== UDP Server ===== */

/**
 * @brief Start UDP server
 */
int wexx_udp_server_start(const struct device *dev, uint16_t port,
			  wexx_socket_recv_cb_t cb, void *user_data)
{
	struct wexx_data *data = dev->data;
	char cmd[128];
	
	if (data->udp_server_running) {
		LOG_WRN("UDP server already running");
		return -EALREADY;
	}
	
	LOG_INF("UDP server start on port %d", port);
	
	data->udp_server_cb = cb;
	data->udp_server_user_data = user_data;
	data->udp_server_port = port;
	
	/* CMD+SETSERVER=1,port (1=UDP) */
	snprintf(cmd, sizeof(cmd), "%s%s=1,%d",
		 CMD_PREFIX, CMD_SETSERVER, port);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret == 0) {
		data->udp_server_running = true;
	}
	
	return ret;
}

/**
 * @brief Stop UDP server
 */
int wexx_udp_server_stop(const struct device *dev)
{
	struct wexx_data *data = dev->data;
	char cmd[64];
	
	if (!data->udp_server_running) {
		return 0;
	}
	
	LOG_INF("UDP server stop");
	
	/* Close the server socket */
	snprintf(cmd, sizeof(cmd), "%s%s=0", CMD_PREFIX, CMD_CLOSESKT);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	
	data->udp_server_running = false;
	data->udp_server_cb = NULL;
	data->udp_server_user_data = NULL;
	
	return ret;
}

/**
 * @brief Send UDP response
 */
int wexx_udp_server_send(const struct device *dev, const char *remote_ip,
			 uint16_t remote_port, const uint8_t *data_buf, size_t len)
{
	char cmd[512];
	
	if (!remote_ip || !data_buf || len == 0) {
		return -EINVAL;
	}
	
	/* CMD+SKTSENDDATA=conn_id,remote_ip,remote_port,len,data */
	snprintf(cmd, sizeof(cmd), "%s%s=0,%s,%d,%zu,%.*s",
		 CMD_PREFIX, CMD_SKTSENDDATA, remote_ip, remote_port, len,
		 (int)len, data_buf);
	
	return wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
}
