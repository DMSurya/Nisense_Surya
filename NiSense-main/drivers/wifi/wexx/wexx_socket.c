/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * WExx Wi-Fi Module Driver - TCP/UDP/SSL Socket
 */

#include "wexx_internal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(wexx, CONFIG_WIFI_WEXX_LOG_LEVEL);

/**
 * @brief Create a TCP/UDP/SSL client socket
 */
int wexx_socket_client(const struct device *dev, enum wexx_socket_type type,
		       const char *remote_addr, uint16_t remote_port)
{
	struct wexx_data *data = dev->data;
	char cmd[256];
	
	if (!remote_addr || remote_port == 0) {
		return -EINVAL;
	}
	
	if (!wexx_is_wifi_connected(dev)) {
		LOG_ERR("Socket client failed: Wi-Fi not connected");
		return -ENOTCONN;
	}
	
	LOG_INF("Creating %s client to %s:%d",
		type == WEXX_SOCKET_TCP ? "TCP" :
		type == WEXX_SOCKET_UDP ? "UDP" : "SSL",
		remote_addr, remote_port);
	
	data->last_conn_id = -1;
	
	/* CMD+SETCLIENT=mode,remote_addr,remote_port (no brackets) */
	snprintf(cmd, sizeof(cmd), "%s%s=%d,%s,%d",
		 CMD_PREFIX, CMD_SETCLIENT, type, remote_addr, remote_port);
	
	int ret = wexx_cmd_with_response(dev, cmd, 10000);
	if (ret != 0) {
		LOG_ERR("Socket client command failed: %d", ret);
		return -EIO;
	}
	
	/* Wait for EVT+SKTCON event (up to 10 seconds for SSL handshake) */
	for (int i = 0; i < 100 && data->last_conn_id < 0; i++) {
		k_msleep(100);
	}
	
	if (data->last_conn_id < 0) {
		LOG_ERR("Socket connection timeout");
		return -ETIMEDOUT;
	}
	
	LOG_INF("Socket connected, conn_id=%d", data->last_conn_id);
	return data->last_conn_id;
}

/**
 * @brief Close a socket
 */
int wexx_socket_close(const struct device *dev, int conn_id)
{
	char cmd[32];
	
	LOG_INF("Closing socket conn_id=%d", conn_id);
	
	/* CMD+CLOSESKT=<connection_id> */
	snprintf(cmd, sizeof(cmd), "%s%s=%d", CMD_PREFIX, CMD_CLOSESKT, conn_id);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("Socket close failed: %d", ret);
		return -EIO;
	}
	
	return 0;
}

/**
 * @brief Send data over a socket
 */
int wexx_socket_send(const struct device *dev, int conn_id,
		     const uint8_t *data_buf, size_t len)
{
	char cmd[1700];
	
	if (!data_buf || len == 0 || len > 1600) {
		return -EINVAL;
	}
	
	if (!wexx_is_wifi_connected(dev)) {
		LOG_ERR("Socket send failed: Wi-Fi not connected");
		return -ENOTCONN;
	}
	
	LOG_DBG("Sending %zu bytes to conn_id=%d", len, conn_id);
	
	/* CMD+SKTSENDDATA=<data_size>,<conn_id>,<data> */
	int cmd_len = snprintf(cmd, sizeof(cmd), "%s%s=%zu,%d,",
			       CMD_PREFIX, CMD_SKTSENDDATA, len, conn_id);
	memcpy(cmd + cmd_len, data_buf, len);
	cmd[cmd_len + len] = '\0';
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("Socket send failed: %d", ret);
		return -EIO;
	}
	
	return len;
}

/**
 * @brief Set socket receive callback
 */
void wexx_socket_set_recv_callback(const struct device *dev,
				   wexx_socket_recv_cb_t cb, void *user_data)
{
	struct wexx_data *data = dev->data;
	
	data->socket_recv_cb = cb;
	data->socket_user_data = user_data;
}

/**
 * @brief Enable/disable auto receive for sockets
 */
int wexx_socket_auto_recv(const struct device *dev, bool enable)
{
	char cmd[32];
	
	LOG_INF("%s socket auto receive", enable ? "Enabling" : "Disabling");
	
	/* CMD+SKTAUTORCV=<enable> (1=enable, 2=disable per datasheet) */
	snprintf(cmd, sizeof(cmd), "%s%s=%d", CMD_PREFIX, CMD_SKTAUTORCV, enable ? 1 : 2);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("Socket auto recv failed: %d", ret);
		return -EIO;
	}
	
	return 0;
}
