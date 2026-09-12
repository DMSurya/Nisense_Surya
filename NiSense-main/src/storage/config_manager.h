/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Configuration Manager
 *
 * Persists packed struct app_config via Zephyr settings (NVS key cm/app).
 * Supports structured groups for Wi-Fi, device settings, and cloud.
 */

#ifndef CONFIG_MANAGER_H_
#define CONFIG_MANAGER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum lengths for configuration strings */
#define CONFIG_WIFI_SSID_MAX_LEN      32
#define CONFIG_WIFI_PASSWORD_MAX_LEN  64
#define CONFIG_DEVICE_NAME_MAX_LEN    32

/** Wi-Fi security type */
enum config_wifi_security {
	CONFIG_WIFI_SEC_OPEN = 0,
	CONFIG_WIFI_SEC_WPA_PSK = 1,
	CONFIG_WIFI_SEC_WPA2_PSK = 2,
	CONFIG_WIFI_SEC_WPA3_PSK = 3,
};

/** Wi-Fi configuration structure */
struct config_wifi {
	bool enabled;
	bool auto_connect;
	char ssid[CONFIG_WIFI_SSID_MAX_LEN + 1];
	char password[CONFIG_WIFI_PASSWORD_MAX_LEN + 1];
	enum config_wifi_security security;
	/* Static IP configuration (optional) */
	bool use_dhcp;
	char static_ip[16];
	char gateway[16];
	char netmask[16];
};

/** Device configuration structure */
struct config_device {
	char name[CONFIG_DEVICE_NAME_MAX_LEN + 1];
	uint8_t brightness;       /* Display backlight 0=off, 1-100=on (GPIO enable only) */
	uint8_t volume;           /* Buzzer volume 0-100 */
	bool auto_sleep;          /* Enable display idle sleep */
	uint16_t sleep_timeout_s; /* Display idle timeout (seconds) */
	uint8_t batt_poll_interval_s; /* Fuel gauge poll / PMIC BLE cadence (5-60 s) */
	uint8_t batt_sleep_soc_pct;  /* Critical SOC: display off, no auto health */
	uint8_t batt_low_soc_pct;    /* Low SOC warning threshold */
	uint8_t batt_resume_soc_pct; /* Exit critical when charging reaches this SOC */
	uint16_t batt_low_mv;        /* Legacy BLE mV threshold (stored; PMIC uses SOC policy) */
	uint16_t measure_hold_ms;    /* SW1 long-press to start measure */
	uint16_t power_hold_ms;      /* PFN1 long-press shutdown countdown */
};

/** Logging configuration structure */
struct config_logging {
	bool enabled;
	bool auto_upload;         /* Auto-upload logs when connected */
	uint16_t interval_s;      /* Logging interval in seconds */
};

/** Cloud/MQTT configuration structure */
struct config_cloud {
	bool enabled;
	char mqtt_host[64];
	uint16_t mqtt_port;
	char mqtt_client_id[64];
	char mqtt_username[32];
	char mqtt_password[64];
	char mqtt_topic_prefix[48];
	bool use_ssl;
};

/**
 * @brief PPG source preference (only used when multiple sources are present).
 *
 * When only one PPG source is available, selection is always automatic and
 * this preference is ignored.
 */
enum config_ppg_preference {
	CONFIG_PPG_PREF_UNSET = 0,   /* No runtime preference configured */
	CONFIG_PPG_PREF_AUTO = 1,    /* Auto-select (first ready) */
	CONFIG_PPG_PREF_MAX86141 = 2,/* Prefer MAX86141 via MAX32664 */
	CONFIG_PPG_PREF_MAX3010X = 3 /* Prefer standalone MAX3010x */
};

/** PPG configuration structure */
struct config_ppg {
	enum config_ppg_preference preference;
	/** PPG acquisition samples for BLE/UI starts; 0 = Kconfig default */
	uint16_t sample_count;
	/** Glucose ADC samples; 0 = leave driver/DT default until first write */
	uint16_t glucose_num_samples;
	/** Glucose inter-sample delay ms; 0 = leave driver/DT default */
	uint16_t glucose_delay_ms;
	/** BLE_SAMP_FLAG_* (e.g. disable proximity) */
	uint8_t flags;
	/** PPG BLE stream decimation 1–33; 0 = firmware default (1) */
	uint8_t stream_decimate;
	/** Health auto-sched: 0=off, 1=on (default on) */
	uint8_t health_auto_enabled;
	/** Fixed schedule interval sec; 0 = adaptive ladder */
	uint16_t health_interval_sec;
	/** Idle temp log interval sec; 0 = use CONFIG_APP_TEMP_IDLE_INTERVAL_SEC */
	uint16_t temp_idle_interval_sec;
};

/** Main configuration structure - groups all config sections */
struct app_config {
	uint32_t version;           /* Config file version */
	struct config_wifi wifi;
	struct config_device device;
	struct config_logging logging;
	struct config_cloud cloud;
	struct config_ppg ppg;
};

#if defined(CONFIG_APP_FEATURE_CONFIG_PERSIST)

/**
 * @brief Initialize configuration manager
 *
 * Loads configuration from NVS/settings. If absent, saves defaults.
 *
 * @return 0 on success, negative errno on failure
 */
int config_manager_init(void);

/**
 * @brief Load configuration from NVS/settings
 *
 * @return 0 on success, negative errno on failure
 */
int config_manager_load(void);

/**
 * @brief Save configuration to NVS/settings
 *
 * @return 0 on success, negative errno on failure
 */
int config_manager_save(void);

/**
 * @brief Reset configuration to defaults
 *
 * Resets all configuration to default values and saves to flash.
 *
 * @return 0 on success, negative errno on failure
 */
int config_manager_reset_defaults(void);

/**
 * @brief Get current configuration
 *
 * @return Pointer to current configuration (read-only)
 */
const struct app_config *config_manager_get(void);

/**
 * @brief Get mutable configuration
 *
 * Use this to modify configuration, then call config_manager_save().
 *
 * @return Pointer to mutable configuration
 */
struct app_config *config_manager_get_mutable(void);

/**
 * @brief Get Wi-Fi configuration
 *
 * @return Pointer to Wi-Fi configuration
 */
const struct config_wifi *config_get_wifi(void);

/**
 * @brief Set Wi-Fi configuration
 *
 * @param wifi Wi-Fi configuration to set
 * @return 0 on success, negative errno on failure
 */
int config_set_wifi(const struct config_wifi *wifi);

/**
 * @brief Get cloud/MQTT configuration
 *
 * @return Pointer to cloud configuration
 */
const struct config_cloud *config_get_cloud(void);

/**
 * @brief Get device configuration
 *
 * @return Pointer to device configuration
 */
const struct config_device *config_get_device(void);

/**
 * @brief Check if a persisted config blob exists in NVS
 */
bool config_manager_file_exists(void);

/**
 * @brief Get settings key used for persistence
 */
const char *config_manager_get_path(void);

#else /* config persistence not enabled - provide no-op stubs */

static inline int config_manager_init(void)
{
	return 0;
}

static inline int config_manager_load(void)
{
	return -ENOTSUP;
}

static inline int config_manager_save(void)
{
	return -ENOTSUP;
}

static inline int config_manager_reset_defaults(void)
{
	return -ENOTSUP;
}

static inline const struct app_config *config_manager_get(void)
{
	static const struct app_config cfg;
	return &cfg;
}

static inline struct app_config *config_manager_get_mutable(void)
{
	static struct app_config cfg;
	return &cfg;
}

static inline const struct config_wifi *config_get_wifi(void)
{
	static const struct config_wifi wifi = {
		.enabled = false,
		.auto_connect = false,
		.ssid = "",
		.password = "",
		.security = CONFIG_WIFI_SEC_OPEN,
		.use_dhcp = true,
		.static_ip = "",
		.gateway = "",
		.netmask = "",
	};
	return &wifi;
}

static inline int config_set_wifi(const struct config_wifi *wifi)
{
	(void)wifi;
	return -ENOTSUP;
}

static inline const struct config_cloud *config_get_cloud(void)
{
	static const struct config_cloud cloud = {
		.enabled = false,
		.mqtt_host = "",
		.mqtt_port = 1883,
		.mqtt_client_id = "",
		.mqtt_username = "",
		.mqtt_password = "",
		.mqtt_topic_prefix = "hcm/telemetry",
		.use_ssl = false,
	};
	return &cloud;
}

static inline const struct config_device *config_get_device(void)
{
	static const struct config_device device = {
		.name = "NiSense",
		.brightness = 80,
		.volume = 70,
		.auto_sleep = true,
		.sleep_timeout_s = 60,
		.batt_poll_interval_s = 10,
		.batt_sleep_soc_pct = 5,
		.batt_low_soc_pct = 15,
		.batt_resume_soc_pct = 20,
		.batt_low_mv = 3400,
		.measure_hold_ms = 1200,
		.power_hold_ms = 12000,
	};
	return &device;
}

static inline bool config_manager_file_exists(void)
{
	return false;
}

static inline const char *config_manager_get_path(void)
{
	return "";
}

#endif /* CONFIG_APP_FEATURE_CONFIG_PERSIST */

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_MANAGER_H_ */
