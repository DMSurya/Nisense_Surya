/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * WExx Wi-Fi Module Driver - MQTT Protocol
 */

#include "wexx_internal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(wexx, CONFIG_WIFI_WEXX_LOG_LEVEL);

/**
 * @brief Initialize MQTT state
 */
void wexx_mqtt_init_state(struct wexx_data *data)
{
	k_sem_init(&data->mqtt_sem, 0, 1);
	data->mqtt_connected = false;
	data->mqtt_status = -1;
	data->mqtt_msg_cb = NULL;
	data->mqtt_user_data = NULL;
}

/**
 * @brief Handle MQTT-related events
 */
void wexx_mqtt_handle_event(const struct device *dev, const char *event_name, const char *params)
{
	struct wexx_data *data = dev->data;

	if (strcmp(event_name, EVT_MQTTCONRES) == 0) {
		/* EVT+MQTTCONRES=<status> (0=success) */
		int status;
		if (sscanf(params, "%d", &status) == 1) {
			data->mqtt_status = status;
			data->mqtt_connected = (status == 0);
			LOG_INF("MQTT connect result: %d", status);
			k_sem_give(&data->mqtt_sem);
		}
	} else if (strcmp(event_name, EVT_MQTTSUBRES) == 0) {
		/* EVT+MQTTSUBRES=<status> */
		int status;
		if (sscanf(params, "%d", &status) == 1) {
			data->mqtt_status = status;
			LOG_INF("MQTT subscribe result: %d", status);
			k_sem_give(&data->mqtt_sem);
		}
	} else if (strcmp(event_name, EVT_MQTTPUBRES) == 0) {
		/* EVT+MQTTPUBRES=<status> */
		int status;
		if (sscanf(params, "%d", &status) == 1) {
			data->mqtt_status = status;
			LOG_INF("MQTT publish result: %d", status);
			k_sem_give(&data->mqtt_sem);
		}
	} else if (strcmp(event_name, EVT_MQTTMSG) == 0) {
		/* EVT+MQTTMSG=<topic>,<len>,<payload> */
		char topic[128];
		int len;
		char *payload_ptr;
		
		if (sscanf(params, "%127[^,],%d,", topic, &len) == 2) {
			payload_ptr = strchr(params, ',');
			if (payload_ptr) {
				payload_ptr = strchr(payload_ptr + 1, ',');
				if (payload_ptr) {
					payload_ptr++;
					strncpy(data->mqtt_msg_topic, topic, sizeof(data->mqtt_msg_topic) - 1);
					strncpy(data->mqtt_msg_payload, payload_ptr, sizeof(data->mqtt_msg_payload) - 1);
					data->mqtt_msg_len = len;
					LOG_INF("MQTT message: topic=%s, len=%d", topic, len);
					if (data->mqtt_msg_cb) {
						data->mqtt_msg_cb(dev, topic, (uint8_t *)payload_ptr, 
								  len, data->mqtt_user_data);
					}
				}
			}
		}
	} else if (strcmp(event_name, EVT_MQTTDISCON) == 0) {
		LOG_INF("MQTT disconnected");
		data->mqtt_connected = false;
	}
}

/**
 * @brief MQTT connect
 */
int wexx_mqtt_connect(const struct device *dev, const char *client_id,
		      const char *broker, uint16_t port, bool ssl,
		      const char *username, const char *password)
{
	struct wexx_data *data = dev->data;
	char cmd[512];
	
	if (!client_id || !broker) {
		return -EINVAL;
	}
	
	if (!wexx_is_wifi_connected(dev)) {
		LOG_ERR("MQTT connect failed: Wi-Fi not connected");
		return -ENOTCONN;
	}
	
	LOG_INF("MQTT connect to %s:%d (ssl=%d)", broker, port, ssl);
	
	/* Reset MQTT state */
	k_sem_reset(&data->mqtt_sem);
	data->mqtt_status = -1;
	data->mqtt_connected = false;
	
	/* CMD+MQTTCON=client_id,server,port,ssl_en[,username,password] (no brackets) */
	if (username && password) {
		snprintf(cmd, sizeof(cmd), "%s%s=%s,%s,%d,%d,%s,%s",
			 CMD_PREFIX, CMD_MQTTCON, client_id, broker, port, 
			 ssl ? 1 : 0, username, password);
	} else {
		snprintf(cmd, sizeof(cmd), "%s%s=%s,%s,%d,%d",
			 CMD_PREFIX, CMD_MQTTCON, client_id, broker, port, ssl ? 1 : 0);
	}
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("MQTT connect command failed: %d", ret);
		return -EIO;
	}
	
	/* Wait for MQTT connect response (up to 30 seconds for SSL) */
	ret = k_sem_take(&data->mqtt_sem, K_SECONDS(30));
	if (ret != 0) {
		LOG_ERR("MQTT connect timeout");
		return -ETIMEDOUT;
	}
	
	if (data->mqtt_status != 0) {
		LOG_ERR("MQTT connect failed: status=%d", data->mqtt_status);
		return -EIO;
	}
	
	return 0;
}

/**
 * @brief MQTT disconnect
 */
int wexx_mqtt_disconnect(const struct device *dev)
{
	struct wexx_data *data = dev->data;
	char cmd[64];
	
	if (!data->mqtt_connected) {
		return 0;
	}
	
	snprintf(cmd, sizeof(cmd), "%s%s", CMD_PREFIX, CMD_MQTTDISCON);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("MQTT disconnect failed: %d", ret);
		return -EIO;
	}
	
	data->mqtt_connected = false;
	return 0;
}

/**
 * @brief MQTT publish
 */
int wexx_mqtt_publish(const struct device *dev, const char *topic,
		      const uint8_t *payload, size_t payload_len,
		      uint8_t qos, bool retain)
{
	struct wexx_data *data = dev->data;
	char cmd[1024];
	
	if (!topic || !payload) {
		return -EINVAL;
	}
	
	if (!data->mqtt_connected) {
		LOG_ERR("MQTT publish failed: not connected");
		return -ENOTCONN;
	}
	
	LOG_INF("MQTT publish to %s, len=%zu", topic, payload_len);
	
	k_sem_reset(&data->mqtt_sem);
	data->mqtt_status = -1;
	
	/* CMD+MQTTPUB=topic,qos,retain,message (no brackets) */
	snprintf(cmd, sizeof(cmd), "%s%s=%s,%d,%d,%.*s",
		 CMD_PREFIX, CMD_MQTTPUB, topic, qos, retain ? 1 : 0,
		 (int)payload_len, payload);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("MQTT publish command failed: %d", ret);
		return -EIO;
	}
	
	ret = k_sem_take(&data->mqtt_sem, K_SECONDS(10));
	if (ret != 0) {
		LOG_ERR("MQTT publish timeout");
		return -ETIMEDOUT;
	}
	
	return data->mqtt_status == 0 ? 0 : -EIO;
}

/**
 * @brief MQTT subscribe
 */
int wexx_mqtt_subscribe(const struct device *dev, const char *topic, uint8_t qos)
{
	struct wexx_data *data = dev->data;
	char cmd[256];
	
	if (!topic) {
		return -EINVAL;
	}
	
	if (!data->mqtt_connected) {
		LOG_ERR("MQTT subscribe failed: not connected");
		return -ENOTCONN;
	}
	
	LOG_INF("MQTT subscribe to %s, qos=%d", topic, qos);
	
	k_sem_reset(&data->mqtt_sem);
	data->mqtt_status = -1;
	
	/* CMD+MQTTSUB=topic,qos (no brackets) */
	snprintf(cmd, sizeof(cmd), "%s%s=%s,%d",
		 CMD_PREFIX, CMD_MQTTSUB, topic, qos);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("MQTT subscribe command failed: %d", ret);
		return -EIO;
	}
	
	ret = k_sem_take(&data->mqtt_sem, K_SECONDS(10));
	if (ret != 0) {
		LOG_ERR("MQTT subscribe timeout");
		return -ETIMEDOUT;
	}
	
	return data->mqtt_status == 0 ? 0 : -EIO;
}

/**
 * @brief Set MQTT message callback
 */
void wexx_mqtt_set_msg_callback(const struct device *dev, wexx_mqtt_msg_cb_t cb, void *user_data)
{
	struct wexx_data *data = dev->data;
	data->mqtt_msg_cb = cb;
	data->mqtt_user_data = user_data;
}

/**
 * @brief Check if MQTT is connected
 */
bool wexx_mqtt_is_connected(const struct device *dev)
{
	struct wexx_data *data = dev->data;
	return data->mqtt_connected;
}
