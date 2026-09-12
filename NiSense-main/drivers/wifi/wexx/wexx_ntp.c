/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * WExx Wi-Fi Module Driver - NTP Protocol
 */

#include "wexx_internal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(wexx, CONFIG_WIFI_WEXX_LOG_LEVEL);

/**
 * @brief Initialize NTP state
 */
void wexx_ntp_init_state(struct wexx_data *data)
{
	k_sem_init(&data->ntp_sem, 0, 1);
	data->ntp_unix_time = 0;
	data->ntp_success = false;
}

/**
 * @brief Handle NTP-related events
 */
void wexx_ntp_handle_event(const struct device *dev, const char *event_name, const char *params)
{
	struct wexx_data *data = dev->data;

	if (strcmp(event_name, EVT_NTP) == 0) {
		/* EVT+NTP=<unix_timestamp> */
		uint32_t unix_time;
		if (sscanf(params, "%u", &unix_time) == 1) {
			data->ntp_unix_time = unix_time;
			data->ntp_success = true;
			LOG_INF("NTP time received: %u", unix_time);
			k_sem_give(&data->ntp_sem);
		}
	} else if (strcmp(event_name, EVT_NTPFAIL) == 0) {
		LOG_ERR("NTP sync failed");
		data->ntp_success = false;
		k_sem_give(&data->ntp_sem);
	}
}

/**
 * @brief NTP time sync
 */
int wexx_ntp_sync(const struct device *dev, const char *ntp_server, uint32_t *unix_time)
{
	struct wexx_data *data = dev->data;
	char cmd[256];
	
	if (!ntp_server) {
		return -EINVAL;
	}
	
	if (!wexx_is_wifi_connected(dev)) {
		LOG_ERR("NTP sync failed: Wi-Fi not connected");
		return -ENOTCONN;
	}
	
	LOG_INF("NTP sync from %s", ntp_server);
	
	k_sem_reset(&data->ntp_sem);
	data->ntp_unix_time = 0;
	data->ntp_success = false;
	
	/* CMD+NTP=server (no brackets per WExx spec) */
	snprintf(cmd, sizeof(cmd), "%s%s=%s", CMD_PREFIX, CMD_NTP, ntp_server);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("NTP command failed: %d", ret);
		return -EIO;
	}
	
	ret = k_sem_take(&data->ntp_sem, K_SECONDS(10));
	if (ret != 0) {
		LOG_ERR("NTP response timeout");
		return -ETIMEDOUT;
	}
	
	if (!data->ntp_success) {
		LOG_ERR("NTP sync failed");
		return -EIO;
	}
	
	if (unix_time) {
		*unix_time = data->ntp_unix_time;
	}
	
	LOG_INF("NTP sync successful: %u", data->ntp_unix_time);
	return 0;
}
