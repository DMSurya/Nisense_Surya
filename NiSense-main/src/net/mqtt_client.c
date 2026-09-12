/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * MQTT Client Wrapper Implementation
 */

#include "mqtt_client.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <errno.h>

LOG_MODULE_REGISTER(mqtt_client, LOG_LEVEL_INF);

static const struct device *wifi_dev;

int mqtt_client_init(void)
{
	wifi_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(wifi0));
	if (!wifi_dev || !device_is_ready(wifi_dev)) {
		LOG_ERR("Wi-Fi device not ready for MQTT client");
		return -ENODEV;
	}
	LOG_INF("MQTT Client initialized");
	return 0;
}

int mqtt_client_connect(const char *client_id, const char *broker, uint16_t port, bool ssl,
                        const char *username, const char *password)
{
	if (!wifi_dev) {
		int ret = mqtt_client_init();
		if (ret != 0) {
			return ret;
		}
	}

	if (!wexx_is_connected(wifi_dev)) {
		LOG_ERR("MQTT connect failed: Wi-Fi not connected");
		return -ENOTCONN;
	}

	if (!broker) {
		return -EINVAL;
	}

	char auto_id[32];
	const char *effective_client_id = client_id;
	if (!effective_client_id || effective_client_id[0] == '\0') {
		snprintf(auto_id, sizeof(auto_id), "HCM_%08X", (uint32_t)k_uptime_get_32());
		effective_client_id = auto_id;
	}

	LOG_INF("Connecting to MQTT broker %s:%d as client '%s' (SSL: %s)",
	        broker, port, effective_client_id, ssl ? "Yes" : "No");

	int ret = wexx_mqtt_connect(wifi_dev, effective_client_id, broker, port, ssl, username, password);
	if (ret != 0) {
		LOG_ERR("MQTT connect failed: %d", ret);
	} else {
		LOG_INF("MQTT connected successfully");
	}

	return ret;
}

int mqtt_client_disconnect(void)
{
	if (!wifi_dev) {
		return -ENODEV;
	}

	LOG_INF("Disconnecting from MQTT broker");
	int ret = wexx_mqtt_disconnect(wifi_dev);
	if (ret != 0) {
		LOG_WRN("MQTT disconnect error: %d", ret);
	}
	return ret;
}

int mqtt_client_publish(const char *topic, const uint8_t *payload, size_t payload_len,
                        uint8_t qos, bool retain)
{
	if (!wifi_dev) {
		return -ENODEV;
	}

	if (!mqtt_client_is_connected()) {
		LOG_ERR("MQTT publish failed: client not connected");
		return -ENOTCONN;
	}

	if (!topic || !payload) {
		return -EINVAL;
	}

	LOG_INF("Publishing to topic '%s' (%zu bytes, QoS %d, retain %d)",
	        topic, payload_len, qos, retain);

	int ret = wexx_mqtt_publish(wifi_dev, topic, payload, payload_len, qos, retain);
	if (ret != 0) {
		LOG_ERR("MQTT publish failed: %d", ret);
	}
	return ret;
}

int mqtt_client_subscribe(const char *topic, uint8_t qos)
{
	if (!wifi_dev) {
		return -ENODEV;
	}

	if (!mqtt_client_is_connected()) {
		LOG_ERR("MQTT subscribe failed: client not connected");
		return -ENOTCONN;
	}

	if (!topic) {
		return -EINVAL;
	}

	LOG_INF("Subscribing to topic '%s' (QoS %d)", topic, qos);

	int ret = wexx_mqtt_subscribe(wifi_dev, topic, qos);
	if (ret != 0) {
		LOG_ERR("MQTT subscribe failed: %d", ret);
	}
	return ret;
}

void mqtt_client_set_callback(wexx_mqtt_msg_cb_t cb, void *user_data)
{
	if (!wifi_dev) {
		int ret = mqtt_client_init();
		if (ret != 0) {
			return;
		}
	}

	wexx_mqtt_set_msg_callback(wifi_dev, cb, user_data);
}

bool mqtt_client_is_connected(void)
{
	if (!wifi_dev) {
		return false;
	}

	return wexx_mqtt_is_connected(wifi_dev);
}
