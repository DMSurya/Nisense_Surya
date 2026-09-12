/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * HTTP Client Wrapper Implementation
 */

#include "http_client.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/wifi/wexx.h>
#include <errno.h>

LOG_MODULE_REGISTER(http_client, LOG_LEVEL_INF);

static const struct device *wifi_dev;

int http_client_init(void)
{
	wifi_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(wifi0));
	if (!wifi_dev || !device_is_ready(wifi_dev)) {
		LOG_ERR("Wi-Fi device not ready for HTTP client");
		return -ENODEV;
	}
	LOG_INF("HTTP Client initialized");
	return 0;
}

int http_client_get(const char *host, const char *path, uint16_t port, bool ssl,
                    char *response, size_t resp_len)
{
	if (!wifi_dev) {
		int ret = http_client_init();
		if (ret != 0) {
			return ret;
		}
	}

	if (!wexx_is_connected(wifi_dev)) {
		LOG_ERR("HTTP GET failed: Wi-Fi not connected");
		return -ENOTCONN;
	}

	if (!host || !path || !response || resp_len == 0) {
		return -EINVAL;
	}

	LOG_INF("HTTP GET: %s://%s:%d%s", ssl ? "https" : "http", host, port, path);

	int http_status = wexx_http_get(wifi_dev, host, path, port, ssl, response, resp_len);

	if (http_status >= 200 && http_status < 300) {
		LOG_INF("HTTP GET success (status %d)", http_status);
	} else if (http_status > 0) {
		LOG_WRN("HTTP GET warning: server status %d", http_status);
	} else {
		LOG_ERR("HTTP GET driver failure: %d", http_status);
	}

	return http_status;
}

int http_client_post(const char *host, const char *path, uint16_t port, bool ssl,
                     const char *data, char *response, size_t resp_len)
{
	if (!wifi_dev) {
		int ret = http_client_init();
		if (ret != 0) {
			return ret;
		}
	}

	if (!wexx_is_connected(wifi_dev)) {
		LOG_ERR("HTTP POST failed: Wi-Fi not connected");
		return -ENOTCONN;
	}

	if (!host || !path || !response || resp_len == 0) {
		return -EINVAL;
	}

	LOG_INF("HTTP POST: %s://%s:%d%s", ssl ? "https" : "http", host, port, path);

	int http_status = wexx_http_post(wifi_dev, host, path, port, ssl, data, response, resp_len);

	if (http_status >= 200 && http_status < 300) {
		LOG_INF("HTTP POST success (status %d)", http_status);
	} else if (http_status > 0) {
		LOG_WRN("HTTP POST warning: server status %d", http_status);
	} else {
		LOG_ERR("HTTP POST driver failure: %d", http_status);
	}

	return http_status;
}
