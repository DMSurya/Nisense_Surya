/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Configuration Manager — persist packed struct app_config via Zephyr settings.
 */

#include <zephyr/kernel.h>
#if defined(CONFIG_APP_FEATURE_CONFIG_PERSIST)
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <string.h>
#include <errno.h>

#include "config_manager.h"

LOG_MODULE_REGISTER(config_mgr, LOG_LEVEL_INF);

#define CONFIG_SETTINGS_KEY "cm/app"
#define CONFIG_FILE_VERSION 2

#define DEFAULT_WIFI_SSID     "TEST"
#define DEFAULT_WIFI_PASSWORD "12345678"

static struct app_config g_config;
static bool g_persisted;

static void config_set_defaults(struct app_config *cfg)
{
	memset(cfg, 0, sizeof(*cfg));

	cfg->version = CONFIG_FILE_VERSION;

	cfg->wifi.enabled = true;
	cfg->wifi.auto_connect = true;
	strncpy(cfg->wifi.ssid, DEFAULT_WIFI_SSID, sizeof(cfg->wifi.ssid) - 1);
	strncpy(cfg->wifi.password, DEFAULT_WIFI_PASSWORD, sizeof(cfg->wifi.password) - 1);
	cfg->wifi.security = CONFIG_WIFI_SEC_WPA2_PSK;
	cfg->wifi.use_dhcp = true;

	strncpy(cfg->device.name, "NiSense", sizeof(cfg->device.name) - 1);
	cfg->device.brightness = 80;
	cfg->device.volume = 50;
	cfg->device.auto_sleep = true;
	cfg->device.sleep_timeout_s = 60;
	cfg->device.batt_poll_interval_s = 10;
	cfg->device.batt_sleep_soc_pct = 5;
	cfg->device.batt_low_soc_pct = 15;
	cfg->device.batt_resume_soc_pct = 20;
	cfg->device.batt_low_mv = 3400;
	cfg->device.measure_hold_ms = 1200;
	cfg->device.power_hold_ms = 12000;

	cfg->logging.enabled = true;
	cfg->logging.auto_upload = false;
	cfg->logging.interval_s = 300;

	cfg->cloud.enabled = false;
	cfg->cloud.mqtt_port = 1883;
	strncpy(cfg->cloud.mqtt_topic_prefix, "hcm/telemetry",
		sizeof(cfg->cloud.mqtt_topic_prefix) - 1);

	cfg->ppg.preference = CONFIG_PPG_PREF_UNSET;
	cfg->ppg.sample_count = 0;          /* Kconfig default at apply time */
	cfg->ppg.glucose_num_samples = 0;
	cfg->ppg.glucose_delay_ms = 0;
	cfg->ppg.flags = 0;
	cfg->ppg.stream_decimate = 0;
	cfg->ppg.health_auto_enabled = 1;
	cfg->ppg.health_interval_sec = 0;   /* adaptive ladder */
	cfg->ppg.temp_idle_interval_sec = 0; /* Kconfig default */
}

#ifdef CONFIG_SETTINGS
static int cfg_direct_load(const char *key, size_t len, settings_read_cb read_cb,
			   void *cb_arg, void *param)
{
	ARG_UNUSED(key);
	struct app_config *cfg = param;
	size_t n;

	/* Start from defaults so newly added fields survive older NVS blobs. */
	config_set_defaults(cfg);

	if (len == 0U || len > sizeof(*cfg)) {
		return -EINVAL;
	}

	n = len;
	ssize_t rc = read_cb(cb_arg, cfg, n);

	if (rc < 0) {
		return (int)rc;
	}
	/* Older shorter records leave trailing defaults in place. */
	if (cfg->version == 0U) {
		cfg->version = CONFIG_FILE_VERSION;
	}
	return 0;
}
#endif

int config_manager_init(void)
{
	int ret;

	LOG_INF("Initializing config manager");
	config_set_defaults(&g_config);

	ret = config_manager_load();
	if (ret != 0) {
		LOG_INF("Config not found or invalid, saving defaults");
		ret = config_manager_save();
		if (ret != 0) {
			LOG_WRN("Failed to save default config: %d", ret);
		}
	}

	LOG_INF("Config manager initialized");
	LOG_INF("  Wi-Fi SSID: %s", g_config.wifi.ssid);
	LOG_INF("  Wi-Fi enabled: %s", g_config.wifi.enabled ? "yes" : "no");
	LOG_INF("  Device name: %s", g_config.device.name);
	return 0;
}

int config_manager_load(void)
{
#ifdef CONFIG_SETTINGS
	struct app_config tmp;

	memset(&tmp, 0, sizeof(tmp));
	if (settings_load_subtree_direct(CONFIG_SETTINGS_KEY, cfg_direct_load, &tmp) == 0 &&
	    tmp.version != 0U) {
		g_config = tmp;
		g_persisted = true;
		LOG_INF("Configuration loaded from NVS (%s)", CONFIG_SETTINGS_KEY);
		return 0;
	}
	g_persisted = false;
	return -ENOENT;
#else
	return -ENOTSUP;
#endif
}

int config_manager_save(void)
{
#ifdef CONFIG_SETTINGS
	int ret = settings_save_one(CONFIG_SETTINGS_KEY, &g_config, sizeof(g_config));

	if (ret != 0) {
		LOG_ERR("Failed to save config: %d", ret);
		return ret;
	}
	g_persisted = true;
	LOG_INF("Configuration saved (%u bytes)", (unsigned)sizeof(g_config));
	return 0;
#else
	return -ENOTSUP;
#endif
}

int config_manager_reset_defaults(void)
{
	LOG_INF("Resetting configuration to defaults");
	config_set_defaults(&g_config);
	return config_manager_save();
}

const struct app_config *config_manager_get(void)
{
	return &g_config;
}

struct app_config *config_manager_get_mutable(void)
{
	return &g_config;
}

const struct config_wifi *config_get_wifi(void)
{
	return &g_config.wifi;
}

const struct config_cloud *config_get_cloud(void)
{
	return &g_config.cloud;
}

int config_set_wifi(const struct config_wifi *wifi)
{
	if (!wifi) {
		return -EINVAL;
	}

	memcpy(&g_config.wifi, wifi, sizeof(g_config.wifi));
	return config_manager_save();
}

const struct config_device *config_get_device(void)
{
	return &g_config.device;
}

bool config_manager_file_exists(void)
{
	return g_persisted;
}

const char *config_manager_get_path(void)
{
	return CONFIG_SETTINGS_KEY;
}

#endif /* CONFIG_APP_FEATURE_CONFIG_PERSIST */
