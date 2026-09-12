/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * WExx Wi-Fi Module Driver - Public API
 * 
 * Out-of-tree driver for WE10/WE20D UART-controlled Wi-Fi modules.
 * Integrates with Zephyr's Wi-Fi management ecosystem (NET_L2_WIFI_MGMT).
 *
 * Usage: Access Wi-Fi functionality through standard Zephyr net_mgmt() API:
 *   - net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0)
 *   - net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params))
 *   - net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0)
 *   - net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status))
 *
 * Events are delivered via net_mgmt_register_event_handler():
 *   - NET_EVENT_WIFI_SCAN_RESULT - Individual scan result
 *   - NET_EVENT_WIFI_SCAN_DONE - Scan complete
 *   - NET_EVENT_WIFI_CONNECT_RESULT - Connection result
 *   - NET_EVENT_WIFI_DISCONNECT_RESULT - Disconnection result
 */

#ifndef ZEPHYR_DRIVERS_WIFI_WEXX_H_
#define ZEPHYR_DRIVERS_WIFI_WEXX_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/net/wifi_mgmt.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup wexx_api WExx Wi-Fi Driver API
 * @ingroup wifi_interface
 * @{
 */

/**
 * @brief Get the WExx Wi-Fi device
 *
 * Helper macro to get the WExx device from device tree.
 * The device must be connected to a UART bus with the "we,wexx" compatible.
 *
 * @param inst Device tree instance number
 * @return Pointer to device structure
 */
#define WEXX_DEVICE_DT_INST_GET(inst) \
	DEVICE_DT_GET(DT_DRV_INST(inst))

/**
 * @brief Get the network interface for the WExx device
 *
 * @param dev WExx device pointer
 * @return Pointer to network interface, or NULL if not found
 */
static inline struct net_if *wexx_get_iface(const struct device *dev)
{
	return net_if_lookup_by_dev(dev);
}

/**
 * @brief Check if WExx module is connected to an AP
 *
 * @param dev WExx device pointer
 * @return true if connected, false otherwise
 */
static inline bool wexx_is_connected(const struct device *dev)
{
	struct net_if *iface = wexx_get_iface(dev);
	
	if (!iface) {
		return false;
	}
	
	struct wifi_iface_status status = {0};
	
	if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface,
		     &status, sizeof(status)) != 0) {
		return false;
	}
	
	return status.state == WIFI_STATE_COMPLETED;
}

/**
 * @brief Initiate a Wi-Fi scan
 *
 * Convenience wrapper around net_mgmt(NET_REQUEST_WIFI_SCAN).
 * Results are delivered via NET_EVENT_WIFI_SCAN_RESULT events.
 * Scan completion is signaled via NET_EVENT_WIFI_SCAN_DONE.
 *
 * @param dev WExx device pointer
 * @return 0 on success, negative errno on failure
 */
static inline int wexx_scan(const struct device *dev)
{
	struct net_if *iface = wexx_get_iface(dev);
	
	if (!iface) {
		return -ENODEV;
	}
	
	return net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0);
}

/**
 * @brief Connect to a Wi-Fi network
 *
 * Convenience wrapper around net_mgmt(NET_REQUEST_WIFI_CONNECT).
 * Connection result is delivered via NET_EVENT_WIFI_CONNECT_RESULT.
 *
 * @param dev WExx device pointer
 * @param ssid SSID to connect to
 * @param ssid_len Length of SSID
 * @param psk Pre-shared key (password), NULL for open networks
 * @param psk_len Length of PSK
 * @param security Security type (WIFI_SECURITY_TYPE_*)
 * @return 0 on success, negative errno on failure
 */
static inline int wexx_connect(const struct device *dev,
			       const char *ssid, size_t ssid_len,
			       const char *psk, size_t psk_len,
			       enum wifi_security_type security)
{
	struct net_if *iface = wexx_get_iface(dev);
	
	if (!iface || !ssid || ssid_len == 0) {
		return -EINVAL;
	}
	
	struct wifi_connect_req_params params = {
		.ssid = (uint8_t *)ssid,
		.ssid_length = ssid_len,
		.psk = (uint8_t *)psk,
		.psk_length = psk_len,
		.security = security,
		.channel = WIFI_CHANNEL_ANY,
		.band = WIFI_FREQ_BAND_2_4_GHZ,
	};
	
	return net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
			&params, sizeof(params));
}

/**
 * @brief Disconnect from the current Wi-Fi network
 *
 * Convenience wrapper around net_mgmt(NET_REQUEST_WIFI_DISCONNECT).
 * Disconnection result is delivered via NET_EVENT_WIFI_DISCONNECT_RESULT.
 *
 * @param dev WExx device pointer
 * @return 0 on success, negative errno on failure
 */
static inline int wexx_disconnect(const struct device *dev)
{
	struct net_if *iface = wexx_get_iface(dev);
	
	if (!iface) {
		return -ENODEV;
	}
	
	return net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
}

/**
 * @brief Get current Wi-Fi interface status
 *
 * Convenience wrapper around net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS).
 *
 * @param dev WExx device pointer
 * @param status Pointer to status structure to fill
 * @return 0 on success, negative errno on failure
 */
static inline int wexx_get_status(const struct device *dev,
				  struct wifi_iface_status *status)
{
	struct net_if *iface = wexx_get_iface(dev);
	
	if (!iface || !status) {
		return -EINVAL;
	}
	
	return net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface,
			status, sizeof(*status));
}

/* ===== Extended WExx API (Module-specific commands) ===== */

/**
 * @brief Ping result callback
 * 
 * @param dev WExx device pointer
 * @param success true if ping succeeded, false on timeout
 * @param ip_addr Target IP address
 * @param time_ms Round-trip time in milliseconds (0 if timeout)
 * @param user_data User data passed to wexx_ping()
 */
typedef void (*wexx_ping_cb_t)(const struct device *dev, bool success,
			       const char *ip_addr, uint32_t time_ms, void *user_data);

/**
 * @brief Socket type for TCP/UDP/SSL operations
 */
enum wexx_socket_type {
	WEXX_SOCKET_TCP = 0,
	WEXX_SOCKET_UDP = 1,
	WEXX_SOCKET_SSL = 2,
};

/**
 * @brief User flash storage type
 */
enum wexx_user_flash_type {
	WEXX_FLASH_ROOT_CA = 1,
	WEXX_FLASH_CLIENT_CA = 2,
	WEXX_FLASH_PRIVATE_KEY = 3,
};

/**
 * @brief Socket data receive callback
 * 
 * @param dev WExx device pointer
 * @param conn_id Connection ID
 * @param data Received data
 * @param len Length of data
 * @param user_data User data
 */
typedef void (*wexx_socket_recv_cb_t)(const struct device *dev, int conn_id,
				      const uint8_t *data, size_t len, void *user_data);

/* Forward declarations - implemented in wexx.c */
int wexx_ping(const struct device *dev, const char *ip_addr,
	      wexx_ping_cb_t cb, void *user_data);

int wexx_socket_client(const struct device *dev, enum wexx_socket_type type,
		       const char *remote_addr, uint16_t remote_port);

int wexx_socket_close(const struct device *dev, int conn_id);

int wexx_socket_send(const struct device *dev, int conn_id,
		     const uint8_t *data, size_t len);

void wexx_socket_set_recv_callback(const struct device *dev,
				   wexx_socket_recv_cb_t cb, void *user_data);

int wexx_socket_auto_recv(const struct device *dev, bool enable);

int wexx_user_flash_read(const struct device *dev, enum wexx_user_flash_type type,
			 char *buf, size_t buf_len);

int wexx_http_get(const struct device *dev, const char *host, const char *path,
		  uint16_t port, bool ssl, char *response, size_t resp_len);

int wexx_http_post(const struct device *dev, const char *host, const char *path,
		   uint16_t port, bool ssl, const char *data, 
		   char *response, size_t resp_len);

/* ===== NTP API ===== */

/**
 * @brief Sync time via NTP
 * 
 * @param dev WExx device pointer
 * @param ntp_server NTP server address (e.g., "pool.ntp.org", "time.google.com")
 * @param unix_time Pointer to store received Unix timestamp
 * @return 0 on success, negative errno on failure
 */
int wexx_ntp_sync(const struct device *dev, const char *ntp_server, uint32_t *unix_time);

/* ===== MQTT API ===== */

/**
 * @brief MQTT message receive callback
 * 
 * @param dev WExx device pointer
 * @param topic Topic name
 * @param payload Message payload
 * @param payload_len Payload length
 * @param user_data User data
 */
typedef void (*wexx_mqtt_msg_cb_t)(const struct device *dev, const char *topic,
				   const uint8_t *payload, size_t payload_len, void *user_data);

/**
 * @brief Connect to MQTT broker
 * 
 * @param dev WExx device pointer
 * @param client_id MQTT client ID
 * @param broker Broker address
 * @param port Port (1883 for TCP, 8883 for SSL)
 * @param ssl Enable SSL/TLS
 * @param username Username (NULL for anonymous)
 * @param password Password (NULL for anonymous)
 * @return 0 on success, negative errno on failure
 */
int wexx_mqtt_connect(const struct device *dev, const char *client_id,
		      const char *broker, uint16_t port, bool ssl,
		      const char *username, const char *password);

/**
 * @brief Disconnect from MQTT broker
 */
int wexx_mqtt_disconnect(const struct device *dev);

/**
 * @brief Publish MQTT message
 * 
 * @param dev WExx device pointer
 * @param topic Topic to publish to
 * @param payload Message payload
 * @param payload_len Payload length
 * @param qos QoS level (0, 1, or 2)
 * @param retain Retain flag
 * @return 0 on success, negative errno on failure
 */
int wexx_mqtt_publish(const struct device *dev, const char *topic,
		      const uint8_t *payload, size_t payload_len,
		      uint8_t qos, bool retain);

/**
 * @brief Subscribe to MQTT topic
 * 
 * @param dev WExx device pointer
 * @param topic Topic to subscribe to
 * @param qos QoS level
 * @return 0 on success, negative errno on failure
 */
int wexx_mqtt_subscribe(const struct device *dev, const char *topic, uint8_t qos);

/**
 * @brief Set MQTT message callback
 */
void wexx_mqtt_set_msg_callback(const struct device *dev, wexx_mqtt_msg_cb_t cb, void *user_data);

/**
 * @brief Check if MQTT is connected
 */
bool wexx_mqtt_is_connected(const struct device *dev);

/* ===== WebSocket API ===== */

/**
 * @brief WebSocket data receive callback
 * 
 * @param dev WExx device pointer
 * @param data Received data
 * @param len Length of data
 * @param user_data User data
 */
typedef void (*wexx_ws_recv_cb_t)(const struct device *dev,
				  const uint8_t *data, size_t len, void *user_data);

/**
 * @brief Connect to WebSocket server
 * 
 * @param dev WExx device pointer
 * @param url WebSocket URL (ws:// or wss://)
 * @return 0 on success, negative errno on failure
 */
int wexx_ws_connect(const struct device *dev, const char *url);

/**
 * @brief Send data over WebSocket
 * 
 * @param dev WExx device pointer
 * @param data Data to send
 * @param len Length of data
 * @return 0 on success, negative errno on failure
 */
int wexx_ws_send(const struct device *dev, const uint8_t *data, size_t len);

/**
 * @brief Close WebSocket connection
 */
int wexx_ws_close(const struct device *dev);

/**
 * @brief Set WebSocket receive callback
 */
void wexx_ws_set_recv_callback(const struct device *dev, wexx_ws_recv_cb_t cb, void *user_data);

/**
 * @brief Check if WebSocket is connected
 */
bool wexx_ws_is_connected(const struct device *dev);

/* ===== OTA API ===== */

/**
 * @brief OTA progress callback
 * 
 * @param dev WExx device pointer
 * @param progress Progress percentage (0-100)
 * @param status Status code (0 = in progress, 1 = done, <0 = error)
 * @param user_data User data
 */
typedef void (*wexx_ota_progress_cb_t)(const struct device *dev,
				       int progress, int status, void *user_data);

/**
 * @brief Start OTA firmware update
 * 
 * @param dev WExx device pointer
 * @param url Firmware URL
 * @param cb Progress callback
 * @param user_data User data
 * @return 0 on success, negative errno on failure
 */
int wexx_ota_start(const struct device *dev, const char *url,
		   wexx_ota_progress_cb_t cb, void *user_data);

/**
 * @brief Get OTA status
 * 
 * @return Current progress (0-100) or negative errno
 */
int wexx_ota_get_status(const struct device *dev);

/* ===== AP Mode API ===== */

/**
 * @brief Configure and enable AP (SoftAP) mode
 * 
 * @param dev WExx device pointer
 * @param ssid AP SSID
 * @param password AP password (NULL for open)
 * @param channel Wi-Fi channel (1-14)
 * @param max_clients Maximum clients (1-4)
 * @return 0 on success, negative errno on failure
 */
int wexx_ap_enable(const struct device *dev, const char *ssid,
		   const char *password, uint8_t channel, uint8_t max_clients);

/**
 * @brief Disable AP mode
 */
int wexx_ap_disable(const struct device *dev);

/**
 * @brief Get number of connected stations
 */
int wexx_ap_get_sta_count(const struct device *dev);

/* ===== UDP Server API ===== */

/**
 * @brief Start UDP server
 * 
 * @param dev WExx device pointer
 * @param port Local port to listen on
 * @param cb Receive callback
 * @param user_data User data
 * @return 0 on success, negative errno on failure
 */
int wexx_udp_server_start(const struct device *dev, uint16_t port,
			  wexx_socket_recv_cb_t cb, void *user_data);

/**
 * @brief Stop UDP server
 */
int wexx_udp_server_stop(const struct device *dev);

/**
 * @brief Send UDP response
 * 
 * @param dev WExx device pointer
 * @param remote_ip Remote IP address
 * @param remote_port Remote port
 * @param data Data to send
 * @param len Length of data
 * @return 0 on success, negative errno on failure
 */
int wexx_udp_server_send(const struct device *dev, const char *remote_ip,
			 uint16_t remote_port, const uint8_t *data, size_t len);

/* ===== User Flash Write API ===== */

/**
 * @brief Write data to user flash (certificates, keys)
 * 
 * @param dev WExx device pointer
 * @param type Flash storage type
 * @param data Data to write
 * @param len Length of data
 * @return 0 on success, negative errno on failure
 */
int wexx_user_flash_write(const struct device *dev, enum wexx_user_flash_type type,
			  const uint8_t *data, size_t len);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_WIFI_WEXX_H_ */
