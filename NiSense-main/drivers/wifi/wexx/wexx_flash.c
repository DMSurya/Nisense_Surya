/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * WExx Wi-Fi Module Driver - User Flash Storage
 */

#include "wexx_internal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(wexx, CONFIG_WIFI_WEXX_LOG_LEVEL);

/**
 * @brief Initialize flash state
 */
void wexx_flash_init_state(struct wexx_data *data)
{
	k_sem_init(&data->flash_sem, 0, 1);
	data->flash_status = -1;
}

/**
 * @brief Handle flash-related events
 */
void wexx_flash_handle_event(const struct device *dev, const char *event_name, const char *params)
{
	struct wexx_data *data = dev->data;

	if (strcmp(event_name, EVT_USERFLASHOK) == 0) {
		LOG_INF("User flash write successful");
		data->flash_status = 0;
		k_sem_give(&data->flash_sem);
	} else if (strcmp(event_name, EVT_USERFLASHERR) == 0) {
		LOG_ERR("User flash write failed");
		data->flash_status = -1;
		k_sem_give(&data->flash_sem);
	}
}

/**
 * @brief Read data from user flash storage
 */
int wexx_user_flash_read(const struct device *dev, enum wexx_user_flash_type type,
			 char *buf, size_t buf_len)
{
	struct wexx_data *data = dev->data;
	char cmd[32];
	
	if (!buf || buf_len == 0) {
		return -EINVAL;
	}
	
	LOG_INF("Reading user flash type=%d", type);
	
	/* CMD+USERFLASHREAD=<type> */
	snprintf(cmd, sizeof(cmd), "%s%s=%d", CMD_PREFIX, CMD_USERFLASHREAD, type);
	
	int ret = wexx_cmd_get_response(dev, cmd, data->response_buf, 
					sizeof(data->response_buf), 5000);
	if (ret != 0) {
		LOG_ERR("User flash read failed: %d", ret);
		return -EIO;
	}
	
	/* Response: RSP=00,<data> */
	const char *data_start = strchr(data->response_buf + 4, ',');
	if (data_start) {
		data_start++;
		strncpy(buf, data_start, buf_len - 1);
		buf[buf_len - 1] = '\0';
		return strlen(buf);
	}
	
	return 0;
}

/**
 * @brief Write data to user flash
 */
int wexx_user_flash_write(const struct device *dev, enum wexx_user_flash_type type,
			  const uint8_t *data_buf, size_t len)
{
	struct wexx_data *data = dev->data;
	char cmd[128];
	
	if (!data_buf || len == 0) {
		return -EINVAL;
	}
	
	LOG_INF("User flash write type=%d, len=%zu", type, len);
	
	k_sem_reset(&data->flash_sem);
	data->flash_status = -1;
	
	/* CMD+USERFLASHWRITE=type,length followed by data */
	snprintf(cmd, sizeof(cmd), "%s%s=%d,%zu",
		 CMD_PREFIX, CMD_USERFLASHWRITE, type, len);
	
	int ret = wexx_send_command(dev, cmd);
	if (ret != 0) {
		return ret;
	}
	
	/* Send binary data after command */
	k_mutex_lock(&data->cmd_mutex, K_FOREVER);
	for (size_t i = 0; i < len; i++) {
		uart_poll_out(data->uart_dev, data_buf[i]);
	}
	k_mutex_unlock(&data->cmd_mutex);
	
	/* Wait for write acknowledgment */
	ret = k_sem_take(&data->flash_sem, K_SECONDS(30));
	if (ret != 0) {
		LOG_ERR("User flash write timeout");
		return -ETIMEDOUT;
	}
	
	return data->flash_status == 0 ? 0 : -EIO;
}
