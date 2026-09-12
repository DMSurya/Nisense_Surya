/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * WExx Wi-Fi Module Driver - HTTP Protocol
 */

#include "wexx_internal.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(wexx, CONFIG_WIFI_WEXX_LOG_LEVEL);

/**
 * @brief Initialize HTTP state
 */
void wexx_http_init_state(struct wexx_data *data)
{
	k_sem_init(&data->http_sem, 0, 1);
	data->http_status = -1;
	data->http_response_len = 0;
}

/**
 * @brief Handle HTTP-related events
 */
void wexx_http_handle_event(const struct device *dev, const char *event_name, const char *params)
{
	struct wexx_data *data = dev->data;

	if (strcmp(event_name, EVT_HTTPSTATUS) == 0) {
		/* EVT+HTTPSTATUS=[200 OK],<html_data> */
		int http_status = 0;
		char *data_ptr;
		
		/* Parse status from [200 OK] format */
		if (params[0] == '[') {
			http_status = atoi(params + 1);
		} else {
			http_status = atoi(params);
		}
		
		if (http_status > 0) {
			data->http_status = http_status;
			data_ptr = strchr(params, ',');
			if (data_ptr) {
				data_ptr++;
				strncpy(data->http_response, data_ptr, 
					sizeof(data->http_response) - 1);
				data->http_response[sizeof(data->http_response) - 1] = '\0';
				data->http_response_len = strlen(data_ptr);
			}
			LOG_INF("HTTP response: status=%d", http_status);
			k_sem_give(&data->http_sem);
		}
	} else if (strcmp(event_name, EVT_HTTPCONFAIL) == 0) {
		LOG_ERR("HTTP connection failed");
		data->http_status = -1;
		k_sem_give(&data->http_sem);
	}
}

/**
 * @brief HTTP GET request
 */
int wexx_http_get(const struct device *dev, const char *host, const char *path,
		  uint16_t port, bool ssl, char *response, size_t resp_len)
{
	struct wexx_data *data = dev->data;
	char cmd[512];
	
	if (!host || !path) {
		return -EINVAL;
	}
	
	if (!wexx_is_wifi_connected(dev)) {
		LOG_ERR("HTTP GET failed: Wi-Fi not connected");
		return -ENOTCONN;
	}
	
	LOG_INF("HTTP GET %s://%s:%d%s", ssl ? "https" : "http", host, port, path);
	
	/* Reset HTTP state */
	k_sem_reset(&data->http_sem);
	data->http_response[0] = '\0';
	data->http_response_len = 0;
	data->http_status = -1;
	
	/* CMD+HTTP=[host],[path],[method],[port],[ssl_en] */
	snprintf(cmd, sizeof(cmd), "%s%s=[%s],[%s],[1],[%d],[%d]",
		 CMD_PREFIX, CMD_HTTP, host, path, port, ssl ? 1 : 0);
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("HTTP command failed: %d", ret);
		return -EIO;
	}
	
	/* Wait for HTTP response event (up to 30 seconds) */
	ret = k_sem_take(&data->http_sem, K_SECONDS(30));
	if (ret != 0) {
		LOG_ERR("HTTP response timeout");
		return -ETIMEDOUT;
	}
	
	/* Copy response if buffer provided */
	if (response && resp_len > 0) {
		strncpy(response, data->http_response, resp_len - 1);
		response[resp_len - 1] = '\0';
	}
	
	return data->http_status;
}

/**
 * @brief HTTP POST request
 */
int wexx_http_post(const struct device *dev, const char *host, const char *path,
		   uint16_t port, bool ssl, const char *post_data,
		   char *response, size_t resp_len)
{
	struct wexx_data *data = dev->data;
	char cmd[1024];
	
	if (!host || !path) {
		return -EINVAL;
	}
	
	if (!wexx_is_wifi_connected(dev)) {
		LOG_ERR("HTTP POST failed: Wi-Fi not connected");
		return -ENOTCONN;
	}
	
	LOG_INF("HTTP POST %s://%s:%d%s", ssl ? "https" : "http", host, port, path);
	
	/* Reset HTTP state */
	k_sem_reset(&data->http_sem);
	data->http_response[0] = '\0';
	data->http_response_len = 0;
	data->http_status = -1;
	
	/* CMD+HTTP=[host],[path],[method],[port],[ssl_en],[data] */
	if (post_data && post_data[0] != '\0') {
		snprintf(cmd, sizeof(cmd), "%s%s=[%s],[%s],[2],[%d],[%d],[%s]",
			 CMD_PREFIX, CMD_HTTP, host, path, port, ssl ? 1 : 0, post_data);
	} else {
		snprintf(cmd, sizeof(cmd), "%s%s=[%s],[%s],[2],[%d],[%d]",
			 CMD_PREFIX, CMD_HTTP, host, path, port, ssl ? 1 : 0);
	}
	
	int ret = wexx_cmd_with_response(dev, cmd, CONFIG_WIFI_WEXX_RESPONSE_TIMEOUT_MS);
	if (ret != 0) {
		LOG_ERR("HTTP command failed: %d", ret);
		return -EIO;
	}
	
	/* Wait for HTTP response event (up to 30 seconds) */
	ret = k_sem_take(&data->http_sem, K_SECONDS(30));
	if (ret != 0) {
		LOG_ERR("HTTP response timeout");
		return -ETIMEDOUT;
	}
	
	/* Copy response if buffer provided */
	if (response && resp_len > 0) {
		strncpy(response, data->http_response, resp_len - 1);
		response[resp_len - 1] = '\0';
	}
	
	return data->http_status;
}
