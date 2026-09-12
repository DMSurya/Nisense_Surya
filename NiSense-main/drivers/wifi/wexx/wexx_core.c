/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * WExx Wi-Fi Module Driver - Core Implementation
 *
 * UART-based driver for WE10/WE20D Wi-Fi modules with ASCII AT command interface.
 * Integrates with Zephyr's Wi-Fi management ecosystem (NET_L2_WIFI_MGMT).
 *
 * This core module handles:
 * - UART communication and RX thread
 * - Device initialization
 * - Event parsing and routing to protocol handlers
 * - Wi-Fi connect/disconnect/scan (Zephyr wifi_mgmt API)
 * - Ping and basic socket operations
 *
 * Protocol-specific functionality is in separate modules:
 * - wexx_http.c    (CONFIG_WIFI_WEXX_HTTP)
 * - wexx_mqtt.c    (CONFIG_WIFI_WEXX_MQTT)
 * - wexx_websocket.c (CONFIG_WIFI_WEXX_WEBSOCKET)
 * - wexx_ntp.c     (CONFIG_WIFI_WEXX_NTP)
 * - wexx_socket.c  (CONFIG_WIFI_WEXX_SOCKET)
 * - wexx_ap.c      (CONFIG_WIFI_WEXX_AP)
 * - wexx_ota.c     (CONFIG_WIFI_WEXX_OTA)
 * - wexx_flash.c   (CONFIG_WIFI_WEXX_FLASH)
 */

#include "wexx_internal.h"
#include <zephyr/logging/log.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/offloaded_netdev.h>
#include <zephyr/net/net_offload.h>

LOG_MODULE_REGISTER(wexx, CONFIG_WIFI_WEXX_LOG_LEVEL);

/* Thread stack */
K_THREAD_STACK_DEFINE(wexx_rx_stack, CONFIG_WIFI_WEXX_RX_THREAD_STACK_SIZE);

/* Forward declarations */
static void wexx_rx_thread_entry(void *p1, void *p2, void *p3);
static void wexx_process_line(const struct device *dev, const char *line);

/* Convert WExx security to Zephyr wifi_security_type */
static enum wifi_security_type wexx_to_wifi_security(uint8_t wexx_sec)
{
	switch (wexx_sec) {
	case 0: return WIFI_SECURITY_TYPE_NONE;
	case 1: return WIFI_SECURITY_TYPE_WEP;
	case 2: return WIFI_SECURITY_TYPE_WPA_PSK;
	case 3: return WIFI_SECURITY_TYPE_PSK;
	case 4: return WIFI_SECURITY_TYPE_PSK;
	case 5: return WIFI_SECURITY_TYPE_SAE;
	default: return WIFI_SECURITY_TYPE_UNKNOWN;
	}
}

/* ===== UART Helpers ===== */

static void wexx_uart_isr(const struct device *uart_dev, void *user_data)
{
	const struct device *dev = user_data;
	struct wexx_data *data = dev->data;
	uint8_t c;
	
	while (uart_irq_update(uart_dev) && uart_irq_is_pending(uart_dev)) {
		if (uart_irq_rx_ready(uart_dev)) {
			while (uart_fifo_read(uart_dev, &c, 1) > 0) {
				ring_buf_put(&data->rx_ring, &c, 1);
			}
			k_wakeup(data->rx_tid);
		}
	}
}

static void wexx_rx_thread_entry(void *p1, void *p2, void *p3)
{
	const struct device *dev = p1;
	uint8_t c;
	
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	
	if (!dev || !dev->data) {
		LOG_ERR("RX thread: invalid device pointer");
		return;
	}
	
	struct wexx_data *data = dev->data;
	
	LOG_DBG("RX thread started");
	
	while (1) {
		while (ring_buf_get(&data->rx_ring, &c, 1) > 0) {
			if (c == '\n') {
				if (data->line_pos > 0 && data->line_buf[data->line_pos - 1] == '\r') {
					data->line_pos--;
				}
				data->line_buf[data->line_pos] = '\0';
				
				if (data->line_pos > 0) {
					LOG_DBG("RX: %s", data->line_buf);
					wexx_process_line(dev, data->line_buf);
				}
				data->line_pos = 0;
			} else if (c != '\r' && data->line_pos < sizeof(data->line_buf) - 1) {
				data->line_buf[data->line_pos++] = c;
			}
		}
		
		k_sleep(K_MSEC(10));
	}
}

/* ===== Command/Response Helpers ===== */

static int parse_status_code(const char *response)
{
	if (strncmp(response, RSP_PREFIX, strlen(RSP_PREFIX)) != 0) {
		return -1;
	}
	
	const char *status_str = response + strlen(RSP_PREFIX);
	char *endptr;
	long status = strtol(status_str, &endptr, 10);
	
	if (endptr == status_str) {
		return -1;
	}
	
	return (int)status;
}

static int parse_mac(const char *mac_str, uint8_t *mac_bytes)
{
	int values[6];
	if (sscanf(mac_str, "%02x%02x%02x%02x%02x%02x",
		   &values[0], &values[1], &values[2],
		   &values[3], &values[4], &values[5]) != 6) {
		if (sscanf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x",
			   &values[0], &values[1], &values[2],
			   &values[3], &values[4], &values[5]) != 6) {
			return -EINVAL;
		}
	}
	for (int i = 0; i < 6; i++) {
		mac_bytes[i] = (uint8_t)values[i];
	}
	return 0;
}

int wexx_send_command(const struct device *dev, const char *cmd)
{
	const struct wexx_config *config = dev->config;
	
	if (!device_is_ready(config->uart)) {
		LOG_ERR("UART not ready");
		return -ENODEV;
	}
	
	LOG_DBG("TX: %s", cmd);
	
	for (size_t i = 0; cmd[i] != '\0'; i++) {
		uart_poll_out(config->uart, cmd[i]);
	}
	
	uart_poll_out(config->uart, '\r');
	uart_poll_out(config->uart, '\n');
	
	return 0;
}

static int wexx_wait_response(const struct device *dev, int timeout_ms)
{
	struct wexx_data *data = dev->data;
	
	k_sem_reset(&data->response_sem);
	data->response_len = 0;
	data->last_status = -1;
	
	int ret = k_sem_take(&data->response_sem, K_MSEC(timeout_ms));
	if (ret != 0) {
		LOG_WRN("Response timeout");
		return -ETIMEDOUT;
	}
	
	return data->last_status;
}

int wexx_cmd_with_response(const struct device *dev, const char *cmd, int timeout_ms)
{
	struct wexx_data *data = dev->data;
	int ret;
	
	k_mutex_lock(&data->cmd_mutex, K_FOREVER);
	k_sem_reset(&data->response_sem);
	
	ret = wexx_send_command(dev, cmd);
	if (ret != 0) {
		k_mutex_unlock(&data->cmd_mutex);
		return ret;
	}
	
	ret = wexx_wait_response(dev, timeout_ms);
	k_mutex_unlock(&data->cmd_mutex);
	
	return ret;
}

int wexx_cmd_get_response(const struct device *dev, const char *cmd, 
			  char *resp_buf, size_t resp_buf_len, int timeout_ms)
{
	struct wexx_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->cmd_mutex, K_FOREVER);
	k_sem_reset(&data->response_sem);
	data->response_len = 0;
	data->last_status = -1;

	ret = wexx_send_command(dev, cmd);
	if (ret != 0) {
		k_mutex_unlock(&data->cmd_mutex);
		return ret;
	}

	ret = k_sem_take(&data->response_sem, K_MSEC(timeout_ms));
	if (ret != 0) {
		k_mutex_unlock(&data->cmd_mutex);
		return -ETIMEDOUT;
	}

	if (resp_buf && resp_buf_len > 0) {
		strncpy(resp_buf, data->response_buf, resp_buf_len - 1);
		resp_buf[resp_buf_len - 1] = '\0';
	}

	int status = data->last_status;
	k_mutex_unlock(&data->cmd_mutex);
	return status;
}

int wexx_set_mode(const struct device *dev, enum wexx_wifi_mode mode)
{
	char cmd[64];
	
	LOG_INF("Setting Wi-Fi mode: %d", mode);
	snprintf(cmd, sizeof(cmd), "%s%s=%d", CMD_PREFIX, CMD_WIFIMODE, mode);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("Set mode failed: %d", ret);
		return -EIO;
	}
	
	return 0;
}

/* ===== Scan Result Parsing ===== */

static void parse_scan_result(const struct device *dev, const char *params)
{
	if (!dev || !dev->data || !params) {
		return;
	}
	
	struct wexx_data *data = dev->data;
	
	if (data->scan_count >= CONFIG_WIFI_WEXX_MAX_SCAN_RESULTS) {
		return;
	}
	
	struct wexx_scan_result_internal *result = &data->scan_results[data->scan_count];
	memset(result, 0, sizeof(*result));
	
	char ssid[WIFI_SSID_MAX_LEN + 1];
	int num, channel, security, rssi;
	char mac_str[18];
	
	int parsed = sscanf(params, "%d,%32[^,],%d,%d,%d,%17s",
			    &num, ssid, &channel, &security, &rssi, mac_str);
	
	if (parsed >= 5) {
		strncpy(result->ssid, ssid, WIFI_SSID_MAX_LEN);
		result->channel = (uint8_t)channel;
		result->security = (uint8_t)security;
		result->rssi = (int8_t)rssi;
		
		if (parsed >= 6) {
			parse_mac(mac_str, result->mac);
		}
		
		data->scan_count++;
		LOG_DBG("Scan result %d: %s (ch%d, sec%d, rssi%d)",
			num, result->ssid, result->channel, result->security, result->rssi);
		
		if (data->scan_cb && data->iface) {
			struct wifi_scan_result zephyr_result = {0};
			
			strncpy(zephyr_result.ssid, result->ssid, WIFI_SSID_MAX_LEN);
			zephyr_result.ssid_length = strlen(result->ssid);
			zephyr_result.channel = result->channel;
			zephyr_result.security = wexx_to_wifi_security(result->security);
			zephyr_result.rssi = result->rssi;
			memcpy(zephyr_result.mac, result->mac, 6);
			zephyr_result.mac_length = 6;
			zephyr_result.band = WIFI_FREQ_BAND_2_4_GHZ;
			
			data->scan_cb(data->iface, 0, &zephyr_result);
		}
	}
}

static void parse_connected(const struct device *dev, const char *params)
{
	if (!dev || !dev->data) {
		LOG_ERR("parse_connected: invalid device");
		return;
	}
	
	struct wexx_data *data = dev->data;
	
	if (params && params[0] != '\0') {
		strncpy(data->ip_addr, params, sizeof(data->ip_addr) - 1);
		data->ip_addr[sizeof(data->ip_addr) - 1] = '\0';
	} else {
		strncpy(data->ip_addr, "0.0.0.0", sizeof(data->ip_addr) - 1);
	}
	data->conn_state = WEXX_STATE_CONNECTED;
	LOG_INF("Connected, IP: %s", data->ip_addr);
	
	if (data->iface) {
		wifi_mgmt_raise_connect_result_event(data->iface, 0);
		net_if_dormant_off(data->iface);
	}
}

/* ===== Event Processing ===== */

static void wexx_process_line(const struct device *dev, const char *line)
{
	if (!dev || !dev->data || !line) {
		return;
	}
	
	struct wexx_data *data = dev->data;
	
	/* Check for response */
	if (strncmp(line, RSP_PREFIX, strlen(RSP_PREFIX)) == 0) {
		strncpy(data->response_buf, line, sizeof(data->response_buf) - 1);
		data->response_buf[sizeof(data->response_buf) - 1] = '\0';
		data->response_len = strlen(line);
		data->last_status = parse_status_code(line);
		k_sem_give(&data->response_sem);
		return;
	}
	
	/* Check for event */
	if (strncmp(line, EVT_PREFIX, strlen(EVT_PREFIX)) == 0) {
		const char *event_start = line + strlen(EVT_PREFIX);
		const char *params = strchr(event_start, '=');
		char event_name[32];
		
		if (params) {
			size_t name_len = params - event_start;
			if (name_len >= sizeof(event_name)) {
				name_len = sizeof(event_name) - 1;
			}
			strncpy(event_name, event_start, name_len);
			event_name[name_len] = '\0';
			params++;
		} else {
			strncpy(event_name, event_start, sizeof(event_name) - 1);
			event_name[sizeof(event_name) - 1] = '\0';
			params = "";
		}
		
		LOG_DBG("Event: %s, params: %s", event_name, params);
		
		/* ===== Core events ===== */
		if (strcmp(event_name, EVT_READY) == 0) {
			data->module_ready = true;
		} else if (strcmp(event_name, EVT_CONTOAP) == 0) {
			parse_connected(dev, params);
		} else if (strcmp(event_name, EVT_DISCON) == 0) {
			data->conn_state = WEXX_STATE_DISCONNECTED;
			data->ip_addr[0] = '\0';
			LOG_INF("Disconnected from AP");
			if (data->iface) {
				wifi_mgmt_raise_disconnect_result_event(data->iface, 0);
				net_if_dormant_on(data->iface);
			}
		} else if (strcmp(event_name, EVT_TIMEOUT) == 0) {
			data->conn_state = WEXX_STATE_ERROR;
			if (data->iface) {
				wifi_mgmt_raise_connect_result_event(data->iface, -ETIMEDOUT);
			}
		} else if (strcmp(event_name, EVT_SCANAP) == 0) {
			parse_scan_result(dev, params);
		} else if (strcmp(event_name, EVT_SCANAPDONE) == 0) {
			data->scan_in_progress = false;
			LOG_INF("Scan complete, found %zu APs", data->scan_count);
			if (data->scan_cb && data->iface) {
				data->scan_cb(data->iface, 0, NULL);
			}
		} else if (strcmp(event_name, EVT_PING) == 0) {
			LOG_INF("Ping response received");
			if (data->ping_cb) {
				data->ping_cb(dev, true, data->ping_ip, 0, data->ping_user_data);
				data->ping_cb = NULL;
			}
		} else if (strcmp(event_name, EVT_PINGTIMEOUT) == 0) {
			LOG_WRN("Ping timeout");
			if (data->ping_cb) {
				data->ping_cb(dev, false, data->ping_ip, 0, data->ping_user_data);
				data->ping_cb = NULL;
			}
		} else if (strcmp(event_name, EVT_SKTCON) == 0) {
			int conn_id;
			if (sscanf(params, "%d", &conn_id) == 1) {
				data->last_conn_id = conn_id;
				LOG_INF("Socket connected, conn_id=%d", conn_id);
			}
		} else if (strcmp(event_name, EVT_SKTDATA) == 0) {
			int data_len, conn_id;
			char *data_ptr;
			
			if (sscanf(params, "%d,%d,", &data_len, &conn_id) == 2) {
				data_ptr = strchr(params, ',');
				if (data_ptr) {
					data_ptr = strchr(data_ptr + 1, ',');
					if (data_ptr) {
						data_ptr++;
						LOG_INF("Socket data received: conn_id=%d, len=%d", conn_id, data_len);
						if (data->socket_recv_cb) {
							data->socket_recv_cb(dev, conn_id, (uint8_t *)data_ptr, 
									     data_len, data->socket_user_data);
						}
					}
				}
			}
		}
		
		/* ===== Route to protocol handlers ===== */
#if defined(CONFIG_WIFI_WEXX_HTTP)
		else if (strcmp(event_name, EVT_HTTPSTATUS) == 0 ||
			 strcmp(event_name, EVT_HTTPCONFAIL) == 0) {
			wexx_http_handle_event(dev, event_name, params);
		}
#endif

#if defined(CONFIG_WIFI_WEXX_NTP)
		else if (strcmp(event_name, EVT_NTP) == 0 ||
			 strcmp(event_name, EVT_NTPFAIL) == 0) {
			wexx_ntp_handle_event(dev, event_name, params);
		}
#endif

#if defined(CONFIG_WIFI_WEXX_MQTT)
		else if (strncmp(event_name, "MQTT", 4) == 0) {
			wexx_mqtt_handle_event(dev, event_name, params);
		}
#endif

#if defined(CONFIG_WIFI_WEXX_WEBSOCKET)
		else if (strncmp(event_name, "WS", 2) == 0) {
			wexx_ws_handle_event(dev, event_name, params);
		}
#endif

#if defined(CONFIG_WIFI_WEXX_OTA)
		else if (strncmp(event_name, "OTA", 3) == 0) {
			wexx_ota_handle_event(dev, event_name, params);
		}
#endif

#if defined(CONFIG_WIFI_WEXX_AP)
		else if (strcmp(event_name, EVT_STACON) == 0 ||
			 strcmp(event_name, EVT_STADISCON) == 0) {
			wexx_ap_handle_event(dev, event_name, params);
		}
#endif

#if defined(CONFIG_WIFI_WEXX_FLASH)
		else if (strcmp(event_name, EVT_USERFLASHOK) == 0 ||
			 strcmp(event_name, EVT_USERFLASHERR) == 0) {
			wexx_flash_handle_event(dev, event_name, params);
		}
#endif

		return;
	}
}

/* ===== Wi-Fi Management API ===== */

static int wexx_mgmt_scan(const struct device *dev, struct wifi_scan_params *params,
			  scan_result_cb_t cb)
{
	struct wexx_data *data = dev->data;
	char cmd[32];
	
	ARG_UNUSED(params);
	
	LOG_INF("Starting AP scan");
	
	data->scan_count = 0;
	data->scan_in_progress = true;
	data->scan_cb = cb;
	memset(data->scan_results, 0, sizeof(data->scan_results));
	
	snprintf(cmd, sizeof(cmd), "%s%s", CMD_PREFIX, CMD_SCANAP);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("Scan start failed: %d", ret);
		data->scan_in_progress = false;
		data->scan_cb = NULL;
		return -EIO;
	}
	
	return 0;
}

static int wexx_mgmt_connect(const struct device *dev,
			     struct wifi_connect_req_params *params)
{
	struct wexx_data *data = dev->data;
	char cmd[256];
	
	if (!params->ssid || params->ssid_length == 0) {
		return -EINVAL;
	}
	
	LOG_INF("Connecting to AP: %.*s", params->ssid_length, params->ssid);
	
	memset(data->ssid, 0, sizeof(data->ssid));
	memcpy(data->ssid, params->ssid, MIN(params->ssid_length, WIFI_SSID_MAX_LEN));
	data->security = params->security;
	data->conn_state = WEXX_STATE_CONNECTING;
	
	if (params->psk && params->psk_length > 0) {
		snprintf(cmd, sizeof(cmd), "%s%s=%.*s,%.*s",
			 CMD_PREFIX, CMD_CONTOAP,
			 params->ssid_length, params->ssid,
			 params->psk_length, params->psk);
	} else {
		snprintf(cmd, sizeof(cmd), "%s%s=%.*s",
			 CMD_PREFIX, CMD_CONTOAP,
			 params->ssid_length, params->ssid);
	}
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("Connect command failed: %d", ret);
		data->conn_state = WEXX_STATE_ERROR;
		wifi_mgmt_raise_connect_result_event(data->iface, -EIO);
		return -EIO;
	}
	
	return 0;
}

static int wexx_mgmt_disconnect(const struct device *dev)
{
	struct wexx_data *data = dev->data;
	char cmd[32];
	
	LOG_INF("Disconnecting from AP");
	snprintf(cmd, sizeof(cmd), "%s%s", CMD_PREFIX, CMD_DISCONN);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("Disconnect failed: %d", ret);
		return -EIO;
	}
	
	data->conn_state = WEXX_STATE_DISCONNECTED;
	data->ip_addr[0] = '\0';
	
	if (data->iface) {
		wifi_mgmt_raise_disconnect_result_event(data->iface, 0);
		net_if_dormant_on(data->iface);
	}
	
	return 0;
}

static int wexx_mgmt_iface_status(const struct device *dev, struct wifi_iface_status *status)
{
	struct wexx_data *data = dev->data;
	
	memset(status, 0, sizeof(*status));
	
	switch (data->conn_state) {
	case WEXX_STATE_CONNECTED:
		status->state = WIFI_STATE_COMPLETED;
		break;
	case WEXX_STATE_CONNECTING:
		status->state = WIFI_STATE_ASSOCIATING;
		break;
	case WEXX_STATE_DISCONNECTED:
	default:
		status->state = WIFI_STATE_DISCONNECTED;
		break;
	}
	
	if (data->conn_state == WEXX_STATE_CONNECTED) {
		strncpy(status->ssid, data->ssid, WIFI_SSID_MAX_LEN);
		status->ssid_len = strlen(data->ssid);
		status->channel = data->channel;
		status->rssi = data->rssi;
		status->security = data->security;
		status->band = WIFI_FREQ_BAND_2_4_GHZ;
		status->iface_mode = WIFI_MODE_INFRA;
		status->link_mode = WIFI_LINK_MODE_UNKNOWN;
	}
	
	return 0;
}

/* ===== Ping API ===== */

int wexx_ping(const struct device *dev, const char *ip_addr,
	      wexx_ping_cb_t cb, void *user_data)
{
	struct wexx_data *data = dev->data;
	char cmd[64];
	
	if (!ip_addr) {
		return -EINVAL;
	}
	
	if (!wexx_is_wifi_connected(dev)) {
		LOG_ERR("Ping failed: Wi-Fi not connected");
		return -ENOTCONN;
	}
	
	LOG_INF("Pinging %s", ip_addr);
	
	data->ping_cb = cb;
	data->ping_user_data = user_data;
	strncpy(data->ping_ip, ip_addr, sizeof(data->ping_ip) - 1);
	
	/* CMD+PING=ip (no brackets per WExx spec) */
	snprintf(cmd, sizeof(cmd), "%s%s=%s", CMD_PREFIX, CMD_PING, ip_addr);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("Ping command failed: %d", ret);
		data->ping_cb = NULL;
		return -EIO;
	}
	
	return 0;
}

/* ===== Reset ===== */

static int wexx_reset(const struct device *dev)
{
	struct wexx_data *data = dev->data;
	char cmd[32];
	
	LOG_INF("Resetting WExx module");
	
	data->module_ready = false;
	snprintf(cmd, sizeof(cmd), "%s%s", CMD_PREFIX, CMD_RESET);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("Reset failed: %d", ret);
		return -EIO;
	}
	
	for (int i = 0; i < 50 && !data->module_ready; i++) {
		k_msleep(100);
	}
	
	if (!data->module_ready) {
		LOG_WRN("Module not ready after reset");
		return -ETIMEDOUT;
	}
	
	LOG_INF("Module ready after reset");
	return 0;
}

/* ===== Network Interface ===== */

static struct net_offload wexx_offload = {
	/* All function pointers NULL - Wi-Fi mgmt API is used instead */
};

static void wexx_iface_init(struct net_if *iface)
{
	const struct device *dev = net_if_get_device(iface);
	struct wexx_data *data = dev->data;
	
	LOG_DBG("Network interface init");
	
	data->iface = iface;
	iface->if_dev->offload = &wexx_offload;
	
	net_if_set_link_addr(iface, data->mac_addr, sizeof(data->mac_addr),
			     NET_LINK_ETHERNET);
	
	net_if_dormant_on(iface);
}

static enum offloaded_net_if_types wexx_get_type(void)
{
	return L2_OFFLOADED_NET_IF_TYPE_WIFI;
}

static const struct wifi_mgmt_ops wexx_mgmt_ops = {
	.scan = wexx_mgmt_scan,
	.connect = wexx_mgmt_connect,
	.disconnect = wexx_mgmt_disconnect,
	.iface_status = wexx_mgmt_iface_status,
};

static const struct net_wifi_mgmt_offload wexx_api = {
	.wifi_iface.iface_api.init = wexx_iface_init,
	.wifi_iface.get_type = wexx_get_type,
	.wifi_mgmt_api = &wexx_mgmt_ops,
};

/* ===== Driver Initialization ===== */

static int wexx_dev_init(const struct device *dev)
{
	struct wexx_data *data = dev->data;
	const struct wexx_config *config = dev->config;
	int ret;
	
	LOG_INF("Initializing WExx Wi-Fi driver");
	
	/* Initialize ring buffer */
	ring_buf_init(&data->rx_ring, sizeof(data->rx_buffer), data->rx_buffer);
	
	/* Initialize synchronization */
	k_sem_init(&data->response_sem, 0, 1);
	k_sem_init(&data->tx_sem, 1, 1);
	k_mutex_init(&data->cmd_mutex);
	
	/* Initialize protocol-specific state */
#if defined(CONFIG_WIFI_WEXX_HTTP)
	wexx_http_init_state(data);
#endif
#if defined(CONFIG_WIFI_WEXX_NTP)
	wexx_ntp_init_state(data);
#endif
#if defined(CONFIG_WIFI_WEXX_MQTT)
	wexx_mqtt_init_state(data);
#endif
#if defined(CONFIG_WIFI_WEXX_WEBSOCKET)
	wexx_ws_init_state(data);
#endif
#if defined(CONFIG_WIFI_WEXX_OTA)
	wexx_ota_init_state(data);
#endif
#if defined(CONFIG_WIFI_WEXX_AP)
	wexx_ap_init_state(data);
#endif
#if defined(CONFIG_WIFI_WEXX_FLASH)
	wexx_flash_init_state(data);
#endif
	
	/* Generate a random MAC address */
	data->mac_addr[0] = 0x02;
	data->mac_addr[1] = 0x00;
	data->mac_addr[2] = 0xAA;
	data->mac_addr[3] = 0xBB;
	data->mac_addr[4] = 0xCC;
	data->mac_addr[5] = 0x00;
	
	/* Configure GPIO */
	if (gpio_is_ready_dt(&config->enable_gpio)) {
		ret = gpio_pin_configure_dt(&config->enable_gpio, GPIO_OUTPUT_ACTIVE);
		if (ret != 0) {
			LOG_ERR("Failed to configure enable GPIO: %d", ret);
			return ret;
		}
		data->enable_gpio = config->enable_gpio;
	} else {
		LOG_WRN("Enable GPIO not configured");
	}
	
	/* Check UART */
	if (!device_is_ready(config->uart)) {
		LOG_ERR("UART device not ready");
		return -ENODEV;
	}
	
	data->uart_dev = config->uart;
	
	/* Configure UART */
	struct uart_config uart_cfg = {
		.baudrate = config->baudrate,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = config->hw_flow_control ? 
			     UART_CFG_FLOW_CTRL_RTS_CTS : UART_CFG_FLOW_CTRL_NONE
	};
	
	ret = uart_configure(config->uart, &uart_cfg);
	if (ret != 0) {
		LOG_ERR("Failed to configure UART: %d", ret);
		return ret;
	}
	
	/* Set up UART interrupt */
	uart_irq_callback_user_data_set(config->uart, wexx_uart_isr, (void *)dev);
	uart_irq_rx_enable(config->uart);
	
	/* Create RX thread */
	data->rx_tid = k_thread_create(&data->rx_thread, wexx_rx_stack,
				       CONFIG_WIFI_WEXX_RX_THREAD_STACK_SIZE,
				       wexx_rx_thread_entry,
				       (void *)dev, NULL, NULL,
				       K_PRIO_COOP(7), 0, K_NO_WAIT);
	k_thread_name_set(data->rx_tid, "wexx_rx");
	
	data->initialized = true;
	
	/* Enable module */
	if (gpio_is_ready_dt(&data->enable_gpio)) {
		gpio_pin_set_dt(&data->enable_gpio, 1);
	}

	/* Try querying firmware version */
	char version_buf[64] = {0};
	int version_ok = 0;
	int ret_version = wexx_cmd_get_response(dev, "CMD?VERSION", version_buf, sizeof(version_buf), 300);
	if (ret_version == 0 && strncmp(version_buf, "RSP=00,", 7) == 0) {
		LOG_INF("WExx module already running, version: %s", version_buf + 7);
		data->module_ready = true;
		version_ok = 1;
	}

	if (!version_ok) {
		LOG_INF("Waiting for module ready...");
		for (int i = 0; i < 50 && !data->module_ready; i++) {
			k_msleep(100);
		}
		if (!data->module_ready) {
			LOG_WRN("Module did not send READY event, attempting reset");
			wexx_reset(dev);
		}
	}
	
	/* Set STA mode by default */
	ret = wexx_set_mode(dev, WEXX_MODE_STA);
	if (ret != 0) {
		LOG_WRN("Failed to set STA mode");
	}
	
	LOG_INF("WExx driver initialized");
	return 0;
}

/* Device tree instantiation */
#define DT_DRV_COMPAT wexx

#define WEXX_INIT(inst)                                                            \
	static struct wexx_data wexx_data_##inst;                                  \
	                                                                           \
	static const struct wexx_config wexx_config_##inst = {                     \
		.uart = DEVICE_DT_GET(DT_INST_PROP(inst, uart)),                   \
		.enable_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, enable_gpios, {0}),  \
		.baudrate = DT_INST_PROP_OR(inst, current_speed, 38400),           \
		.hw_flow_control = DT_INST_PROP_OR(inst, hw_flow_control, false),  \
	};                                                                         \
	                                                                           \
	NET_DEVICE_DT_INST_OFFLOAD_DEFINE(inst,                                    \
					  wexx_dev_init,                           \
					  NULL,                                    \
					  &wexx_data_##inst,                       \
					  &wexx_config_##inst,                     \
					  CONFIG_WIFI_WEXX_INIT_PRIORITY,          \
					  &wexx_api,                               \
					  1500);

DT_INST_FOREACH_STATUS_OKAY(WEXX_INIT)
