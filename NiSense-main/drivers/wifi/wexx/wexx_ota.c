/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * WExx Wi-Fi Module Driver - OTA Firmware Update
 */

#include "wexx_internal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(wexx, CONFIG_WIFI_WEXX_LOG_LEVEL);

/**
 * @brief Initialize OTA state
 */
void wexx_ota_init_state(struct wexx_data *data)
{
	k_sem_init(&data->ota_sem, 0, 1);
	data->ota_progress = 0;
	data->ota_status = 0;
	data->ota_cb = NULL;
	data->ota_user_data = NULL;
}

/**
 * @brief Handle OTA-related events
 */
void wexx_ota_handle_event(const struct device *dev, const char *event_name, const char *params)
{
	struct wexx_data *data = dev->data;

	if (strcmp(event_name, EVT_OTAPROGRESS) == 0) {
		/* EVT+OTAPROGRESS=<percent> */
		int progress;
		if (sscanf(params, "%d", &progress) == 1) {
			data->ota_progress = progress;
			LOG_INF("OTA progress: %d%%", progress);
			if (data->ota_cb) {
				data->ota_cb(dev, progress, 0, data->ota_user_data);
			}
		}
	} else if (strcmp(event_name, EVT_OTADONE) == 0) {
		LOG_INF("OTA completed");
		data->ota_progress = 100;
		data->ota_status = 1;
		if (data->ota_cb) {
			data->ota_cb(dev, 100, 1, data->ota_user_data);
		}
		k_sem_give(&data->ota_sem);
	} else if (strcmp(event_name, EVT_OTAFAIL) == 0) {
		LOG_ERR("OTA failed");
		data->ota_status = -1;
		if (data->ota_cb) {
			data->ota_cb(dev, data->ota_progress, -1, data->ota_user_data);
		}
		k_sem_give(&data->ota_sem);
	}
}

/**
 * @brief Start OTA firmware update
 */
int wexx_ota_start(const struct device *dev, const char *url,
		   wexx_ota_progress_cb_t cb, void *user_data)
{
	struct wexx_data *data = dev->data;
	char cmd[256];
	
	if (!url) {
		return -EINVAL;
	}
	
	LOG_INF("OTA start from: %s", url);
	
	k_sem_reset(&data->ota_sem);
	data->ota_progress = 0;
	data->ota_status = 0;
	data->ota_cb = cb;
	data->ota_user_data = user_data;
	
	/* CMD+OTASTART=url */
	snprintf(cmd, sizeof(cmd), "%s%s=%s", CMD_PREFIX, CMD_OTASTART, url);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("OTA start command failed: %d", ret);
		return -EIO;
	}
	
	return 0;
}

/**
 * @brief Get OTA status
 */
int wexx_ota_get_status(const struct device *dev)
{
	struct wexx_data *data = dev->data;
	
	if (data->ota_status < 0) {
		return data->ota_status;
	}
	
	return data->ota_progress;
}
