/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * WExx Wi-Fi Module Driver - Internal Header
 *
 * Shared data structures and helper functions for protocol-specific modules.
 */

#ifndef WEXX_INTERNAL_H_
#define WEXX_INTERNAL_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/ring_buffer.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/drivers/wifi/wexx.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Command prefixes and terminators */
#define CMD_PREFIX      "CMD+"
#define CMD_QUERY       "CMD?"
#define RSP_PREFIX      "RSP="
#define EVT_PREFIX      "EVT+"
#define CMD_TERMINATOR  "\r\n"

/* Common commands */
#define CMD_RESET       "RESET"
#define CMD_FACRESET    "FACRESET"
#define CMD_WIFIMODE    "WIFIMODE"
#define CMD_CONTOAP     "CONTOAP"
#define CMD_DISCONN     "DISCONN"
#define CMD_SCANAP      "SCANAP"
#define CMD_SETAP       "SETAP"
#define CMD_AUTOCONAP   "AUTOCONAP"
#define CMD_VERSION     "VERSION"
#define CMD_WIFI        "WIFI"
#define CMD_PING        "PING"
#define CMD_SETCLIENT   "SETCLIENT"
#define CMD_SETSERVER   "SETSERVER"
#define CMD_CLOSESKT    "CLOSESKT"
#define CMD_SKTSENDDATA "SKTSENDDATA"
#define CMD_SKTRCVDATA  "SKTRCVDATA"
#define CMD_SKTAUTORCV  "SKTAUTORCV"

/* HTTP commands */
#define CMD_HTTP        "HTTP"
#define CMD_HTTPCLOSE   "HTTPCLOSE"

/* NTP commands */
#define CMD_NTP         "NTP"

/* MQTT commands */
#define CMD_MQTTCON     "MQTTCON"
#define CMD_MQTTPUB     "MQTTPUB"
#define CMD_MQTTSUB     "MQTTSUB"
#define CMD_MQTTDISCON  "MQTTDISCON"

/* WebSocket commands */
#define CMD_WSCONNECT   "WSCONNECT"
#define CMD_WSSEND      "WSSEND"
#define CMD_WSCLOSE     "WSCLOSE"

/* OTA commands */
#define CMD_OTASTART    "OTASTART"
#define CMD_OTASTATUS   "OTASTATUS"

/* Flash commands */
#define CMD_USERFLASHREAD  "USERFLASHREAD"
#define CMD_USERFLASHWRITE "USERFLASHWRITE"

/* Events */
#define EVT_READY       "READY"
#define EVT_CONTOAP     "CONTOAP"
#define EVT_DISCON      "DISCON"
#define EVT_SCANAP      "SCANAP"
#define EVT_SCANAPDONE  "SCANAPDONE"
#define EVT_TIMEOUT     "TIMEOUT"
#define EVT_STACON      "STACON"
#define EVT_STADISCON   "STADISCON"
#define EVT_PING        "PING"
#define EVT_PINGTIMEOUT "PINGTIMEOUT"
#define EVT_DETAILS     "DETAILS"
#define EVT_SKTCON      "SKTCON"
#define EVT_SKTDATA     "SKTDATA"
#define EVT_SKTDISCON   "SKTDISCON"
#define EVT_HTTPSTATUS  "HTTPSTATUS"
#define EVT_HTTPCONFAIL "HTTPCONFAIL"
#define EVT_NTP         "NTP"
#define EVT_NTPFAIL     "NTPFAIL"
#define EVT_MQTTCONRES  "MQTTCONRES"
#define EVT_MQTTSUBRES  "MQTTSUBRES"
#define EVT_MQTTPUBRES  "MQTTPUBRES"
#define EVT_MQTTMSG     "MQTTMSG"
#define EVT_MQTTDISCON  "MQTTDISCON"
#define EVT_OTAPROGRESS "OTAPROGRESS"
#define EVT_OTADONE     "OTADONE"
#define EVT_OTAFAIL     "OTAFAIL"
#define EVT_WSCONNECTED "WSCONNECTED"
#define EVT_WSDATA      "WSDATA"
#define EVT_WSCLOSED    "WSCLOSED"
#define EVT_WSFAIL      "WSFAIL"
#define EVT_USERFLASHOK "USERFLASHOK"
#define EVT_USERFLASHERR "USERFLASHERR"

/* Wi-Fi mode settings */
enum wexx_wifi_mode {
	WEXX_MODE_STA = 1,
	WEXX_MODE_AP = 2,
	WEXX_MODE_CONCURRENT = 3
};

/* Connection state */
enum wexx_conn_state {
	WEXX_STATE_DISCONNECTED = 0,
	WEXX_STATE_CONNECTING = 1,
	WEXX_STATE_CONNECTED = 2,
	WEXX_STATE_ERROR = 3
};

/* Internal scan result structure */
struct wexx_scan_result_internal {
	char ssid[WIFI_SSID_MAX_LEN + 1];
	uint8_t channel;
	uint8_t security;
	int8_t rssi;
	uint8_t mac[6];
};

/* Device data structure */
struct wexx_data {
	/* Network interface */
	struct net_if *iface;
	
	/* UART device */
	const struct device *uart_dev;
	
	/* GPIO for chip enable */
	struct gpio_dt_spec enable_gpio;
	
	/* Ring buffer for UART RX */
	struct ring_buf rx_ring;
	uint8_t rx_buffer[CONFIG_WIFI_WEXX_RX_BUF_SIZE];
	
	/* Response buffer */
	char response_buf[CONFIG_WIFI_WEXX_TX_BUF_SIZE];
	size_t response_len;
	
	/* Line buffer for parsing */
	char line_buf[CONFIG_WIFI_WEXX_LINE_BUF_SIZE];
	size_t line_pos;
	
	/* Synchronization */
	struct k_sem response_sem;
	struct k_sem tx_sem;
	struct k_mutex cmd_mutex;
	
	/* State */
	bool initialized;
	bool module_ready;
	enum wexx_conn_state conn_state;
	
	/* MAC address */
	uint8_t mac_addr[6];
	
	/* Connection info */
	char ssid[WIFI_SSID_MAX_LEN + 1];
	char ip_addr[NET_IPV4_ADDR_LEN];
	char gateway[NET_IPV4_ADDR_LEN];
	int8_t rssi;
	uint8_t channel;
	enum wifi_security_type security;
	
	/* Scan state */
	struct wexx_scan_result_internal scan_results[CONFIG_WIFI_WEXX_MAX_SCAN_RESULTS];
	size_t scan_count;
	bool scan_in_progress;
	scan_result_cb_t scan_cb;
	
	/* Ping state */
	wexx_ping_cb_t ping_cb;
	void *ping_user_data;
	char ping_ip[NET_IPV4_ADDR_LEN];
	
	/* Socket state */
	wexx_socket_recv_cb_t socket_recv_cb;
	void *socket_user_data;
	int last_conn_id;
	
#if defined(CONFIG_WIFI_WEXX_HTTP)
	/* HTTP state */
	char http_response[CONFIG_WIFI_WEXX_HTTP_RESPONSE_SIZE];
	size_t http_response_len;
	int http_status;
	struct k_sem http_sem;
#endif

#if defined(CONFIG_WIFI_WEXX_NTP)
	/* NTP state */
	struct k_sem ntp_sem;
	uint32_t ntp_unix_time;
	bool ntp_success;
#endif

#if defined(CONFIG_WIFI_WEXX_MQTT)
	/* MQTT state */
	struct k_sem mqtt_sem;
	bool mqtt_connected;
	int mqtt_status;
	char mqtt_msg_topic[128];
	char mqtt_msg_payload[256];
	size_t mqtt_msg_len;
	wexx_mqtt_msg_cb_t mqtt_msg_cb;
	void *mqtt_user_data;
#endif

#if defined(CONFIG_WIFI_WEXX_WEBSOCKET)
	/* WebSocket state */
	struct k_sem ws_sem;
	bool ws_connected;
	int ws_conn_id;
	wexx_ws_recv_cb_t ws_recv_cb;
	void *ws_user_data;
#endif

#if defined(CONFIG_WIFI_WEXX_OTA)
	/* OTA state */
	struct k_sem ota_sem;
	int ota_progress;
	int ota_status;
	wexx_ota_progress_cb_t ota_cb;
	void *ota_user_data;
#endif

#if defined(CONFIG_WIFI_WEXX_AP)
	/* AP mode state */
	bool ap_enabled;
	uint8_t ap_sta_count;
	
	/* UDP server state */
	bool udp_server_running;
	uint16_t udp_server_port;
	wexx_socket_recv_cb_t udp_server_cb;
	void *udp_server_user_data;
#endif

#if defined(CONFIG_WIFI_WEXX_FLASH)
	/* User flash state */
	struct k_sem flash_sem;
	int flash_status;
#endif

	/* Last response status */
	int last_status;
	
	/* Thread for RX processing */
	struct k_thread rx_thread;
	k_tid_t rx_tid;
};

/* Device configuration structure */
struct wexx_config {
	const struct device *uart;
	struct gpio_dt_spec enable_gpio;
	uint32_t baudrate;
	bool hw_flow_control;
};

/* ===== Internal API functions (implemented in wexx.c) ===== */

/**
 * @brief Send command to module (without waiting for response)
 */
int wexx_send_command(const struct device *dev, const char *cmd);

/**
 * @brief Send command and wait for response
 * @return Status code from response (0=success), or negative errno
 */
int wexx_cmd_with_response(const struct device *dev, const char *cmd, int timeout_ms);

/**
 * @brief Send command and get full response string
 */
int wexx_cmd_get_response(const struct device *dev, const char *cmd, 
			  char *resp_buf, size_t resp_buf_len, int timeout_ms);

/**
 * @brief Set Wi-Fi mode (STA, AP, Concurrent)
 */
int wexx_set_mode(const struct device *dev, enum wexx_wifi_mode mode);

/**
 * @brief Check if Wi-Fi is connected (internal/fast version)
 * Uses direct struct access instead of net_mgmt API for lower overhead.
 */
static inline bool wexx_is_wifi_connected(const struct device *dev)
{
	struct wexx_data *data = dev->data;
	return data->conn_state == WEXX_STATE_CONNECTED;
}

/* ===== Event handlers (called from wexx.c process_line) ===== */

#if defined(CONFIG_WIFI_WEXX_HTTP)
void wexx_http_handle_event(const struct device *dev, const char *event_name, const char *params);
void wexx_http_init_state(struct wexx_data *data);
#endif

#if defined(CONFIG_WIFI_WEXX_NTP)
void wexx_ntp_handle_event(const struct device *dev, const char *event_name, const char *params);
void wexx_ntp_init_state(struct wexx_data *data);
#endif

#if defined(CONFIG_WIFI_WEXX_MQTT)
void wexx_mqtt_handle_event(const struct device *dev, const char *event_name, const char *params);
void wexx_mqtt_init_state(struct wexx_data *data);
#endif

#if defined(CONFIG_WIFI_WEXX_WEBSOCKET)
void wexx_ws_handle_event(const struct device *dev, const char *event_name, const char *params);
void wexx_ws_init_state(struct wexx_data *data);
#endif

#if defined(CONFIG_WIFI_WEXX_OTA)
void wexx_ota_handle_event(const struct device *dev, const char *event_name, const char *params);
void wexx_ota_init_state(struct wexx_data *data);
#endif

#if defined(CONFIG_WIFI_WEXX_AP)
void wexx_ap_handle_event(const struct device *dev, const char *event_name, const char *params);
void wexx_ap_init_state(struct wexx_data *data);
#endif

#if defined(CONFIG_WIFI_WEXX_FLASH)
void wexx_flash_handle_event(const struct device *dev, const char *event_name, const char *params);
void wexx_flash_init_state(struct wexx_data *data);
#endif

#endif /* WEXX_INTERNAL_H_ */
