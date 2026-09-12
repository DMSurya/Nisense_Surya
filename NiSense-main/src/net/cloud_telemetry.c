/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Cloud telemetry — publishes vitals and glucose JSON to MQTT when enabled
 * in config.json and Wi-Fi is connected.
 */

#include "cloud_telemetry.h"

#if defined(CONFIG_APP_FEATURE_CLOUD_TELEMETRY)

#include "mqtt_client.h"
#include "config_manager.h"
#include "ble_gatt.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_REGISTER(cloud_telemetry, LOG_LEVEL_INF);

#define CLOUD_JSON_MAX     320
#define CLOUD_TOPIC_MAX    96
#define CLOUD_CONNECT_DELAY_SEC 3

enum cloud_msg_type {
	CLOUD_MSG_VITALS = 1,
	CLOUD_MSG_GLUCOSE = 2,
};

struct cloud_msg {
	enum cloud_msg_type type;
	union {
		struct ble_vitals_data vitals;
		struct ble_glucose_data glucose;
	} u;
};

static struct k_work_delayable connect_work;
static struct k_work publish_work;
static struct cloud_msg pending_msg;
static bool initialized;

static const struct config_cloud *cloud_cfg(void)
{
#if defined(CONFIG_APP_FEATURE_CONFIG_PERSIST)
	return &config_manager_get()->cloud;
#else
	static const struct config_cloud disabled = { .enabled = false };
	return &disabled;
#endif
}

static bool cloud_enabled(void)
{
	const struct config_cloud *cloud = cloud_cfg();

	return cloud->enabled && cloud->mqtt_host[0] != '\0';
}

static void build_topic(char *topic, size_t topic_len, const char *suffix)
{
	const struct config_cloud *cloud = cloud_cfg();
	const char *prefix = cloud->mqtt_topic_prefix[0] != '\0' ?
			     cloud->mqtt_topic_prefix : "hcm/telemetry";

	snprintf(topic, topic_len, "%s/%s", prefix, suffix);
}

static int cloud_mqtt_ensure_connected(void)
{
	const struct config_cloud *cloud = cloud_cfg();

	if (!cloud_enabled()) {
		return -ENOENT;
	}

	if (mqtt_client_is_connected()) {
		return 0;
	}

	const char *user = cloud->mqtt_username[0] != '\0' ? cloud->mqtt_username : NULL;
	const char *pass = cloud->mqtt_password[0] != '\0' ? cloud->mqtt_password : NULL;
	const char *client_id = cloud->mqtt_client_id[0] != '\0' ?
				cloud->mqtt_client_id : NULL;
	uint16_t port = cloud->mqtt_port ? cloud->mqtt_port :
			(cloud->use_ssl ? 8883U : 1883U);

	return mqtt_client_connect(client_id, cloud->mqtt_host, port,
				   cloud->use_ssl, user, pass);
}

static void connect_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!cloud_enabled()) {
		return;
	}

	int ret = cloud_mqtt_ensure_connected();
	if (ret != 0) {
		LOG_WRN("Cloud MQTT connect deferred: %d", ret);
	}
}

static void publish_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!cloud_enabled()) {
		return;
	}

	int ret = cloud_mqtt_ensure_connected();
	if (ret != 0) {
		LOG_WRN("Cloud publish skipped (MQTT not connected): %d", ret);
		return;
	}

	char json[CLOUD_JSON_MAX];
	char topic[CLOUD_TOPIC_MAX];
	size_t payload_len;

	if (pending_msg.type == CLOUD_MSG_VITALS) {
		const struct ble_vitals_data *v = &pending_msg.u.vitals;

		build_topic(topic, sizeof(topic), "vitals");
		payload_len = (size_t)snprintf(json, sizeof(json),
			"{\"ts\":%u,\"hr\":%u,\"hr_conf\":%u,"
			"\"spo2\":%u,\"spo2_conf\":%u,"
			"\"hb_x10\":%u,\"hb_conf\":%u,"
			"\"resp\":%u,\"resp_conf\":%u,"
			"\"quality\":%u,\"flags\":%u}",
			v->timestamp, v->hr_bpm, v->hr_confidence,
			v->spo2_percent, v->spo2_confidence,
			v->hb_g_dl_x10, v->hb_confidence,
			v->resp_rate_bpm, v->resp_confidence,
			v->quality, v->flags);
	} else {
		const struct ble_glucose_data *g = &pending_msg.u.glucose;

		build_topic(topic, sizeof(topic), "glucose");
		payload_len = (size_t)snprintf(json, sizeof(json),
			"{\"ts\":%u,\"mg_dl\":%u,\"quality\":%u}",
			g->timestamp, g->glucose_mg_dl, g->quality);
	}

	ret = mqtt_client_publish(topic, (const uint8_t *)json, payload_len, 0, false);
	if (ret != 0) {
		LOG_WRN("Cloud MQTT publish failed: %d", ret);
	} else {
		LOG_INF("Cloud telemetry published to %s", topic);
	}
}

int cloud_telemetry_init(void)
{
	if (initialized) {
		return 0;
	}

	int ret = mqtt_client_init();
	if (ret != 0) {
		LOG_WRN("Cloud telemetry: MQTT init failed: %d", ret);
		return ret;
	}

	k_work_init_delayable(&connect_work, connect_work_handler);
	k_work_init(&publish_work, publish_work_handler);
	initialized = true;

	if (cloud_enabled()) {
		LOG_INF("Cloud telemetry enabled (broker %s)", cloud_cfg()->mqtt_host);
	} else {
		LOG_INF("Cloud telemetry ready (disabled in config)");
	}

	return 0;
}

void cloud_telemetry_on_wifi_status(const struct ble_wifi_status *status)
{
	if (!initialized || !cloud_enabled() || status == NULL) {
		return;
	}

	if (status->connected == 2) {
		(void)k_work_schedule(&connect_work, K_SECONDS(CLOUD_CONNECT_DELAY_SEC));
	} else if (status->connected == 0) {
		(void)k_work_cancel_delayable(&connect_work);
		(void)mqtt_client_disconnect();
	}
}

void cloud_telemetry_publish_vitals(const struct ble_vitals_data *data)
{
	if (!initialized || !cloud_enabled() || data == NULL) {
		return;
	}

	pending_msg.type = CLOUD_MSG_VITALS;
	pending_msg.u.vitals = *data;
	(void)k_work_submit(&publish_work);
}

void cloud_telemetry_publish_glucose(const struct ble_glucose_data *data)
{
	if (!initialized || !cloud_enabled() || data == NULL) {
		return;
	}

	pending_msg.type = CLOUD_MSG_GLUCOSE;
	pending_msg.u.glucose = *data;
	(void)k_work_submit(&publish_work);
}

#else /* !CONFIG_APP_FEATURE_CLOUD_TELEMETRY */

#include <errno.h>

int cloud_telemetry_init(void)
{
	return -ENOTSUP;
}

void cloud_telemetry_on_wifi_status(const struct ble_wifi_status *status)
{
	ARG_UNUSED(status);
}

void cloud_telemetry_publish_vitals(const struct ble_vitals_data *data)
{
	ARG_UNUSED(data);
}

void cloud_telemetry_publish_glucose(const struct ble_glucose_data *data)
{
	ARG_UNUSED(data);
}

#endif /* CONFIG_APP_FEATURE_CLOUD_TELEMETRY */
