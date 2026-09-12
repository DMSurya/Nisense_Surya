/* =============================================================================
 * Storage Config Module - NVS Configuration Management
 * =============================================================================
 * Provides persistent configuration storage using NVS (Non-Volatile Storage)
 * Target: nRF52840 (Raytac MDBT50Q-DB-40)
 * ============================================================================= */

#include <zephyr/kernel.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_REGISTER(storage_config, LOG_LEVEL_INF);

#include "storage_config.h"

/* NVS configuration - partition on external flash (pm_static.yml: storage_nvs, DT: nvs_partition) */
#define NVS_PARTITION_ID	FIXED_PARTITION_ID(storage_nvs)
#define NVS_SECTOR_SIZE		4096
#define NVS_SECTOR_COUNT	32  /* 128 KB partition / 4 KB pages (storage_nvs @ 0x306000) */
#define NVS_NAMESPACE_ID	1
#define CONFIG_KEY_ID		1

/* Default configuration values */
static const struct device_config default_config = {
	.device_name = "WearableDevice",
	.rtc_trim_ppm = 0,
	.battery_low_mv = 3000,
	.log_enable = 1,
	.ble_tx_power = 8,
	.reserved = {0}
};

/* Global NVS filesystem handle */
static struct nvs_fs fs;
static bool nvs_initialized = false;

/* =============================================================================
 * NVS Initialization
 * ============================================================================= */

static int storage_config_init_nvs(void)
{
	int ret;
	const struct flash_area *fa;

	if (nvs_initialized) {
		return 0;
	}

	/* Get flash area for NVS partition */
	ret = flash_area_open(NVS_PARTITION_ID, &fa);
	if (ret != 0) {
		LOG_ERR("Failed to open NVS partition: %d", ret);
		return ret;
	}

	fs.offset = fa->fa_off;
	fs.sector_size = NVS_SECTOR_SIZE;
	fs.sector_count = NVS_SECTOR_COUNT;

	flash_area_close(fa);

	/* Get device for NVS partition (external flash mx25r64) */
	const struct device *flash_dev = FIXED_PARTITION_DEVICE(nvs_partition);
	if (flash_dev == NULL) {
		LOG_ERR("Failed to get NVS partition device");
		return -ENODEV;
	}

	/* Initialize NVS filesystem */
	fs.flash_device = flash_dev;
	ret = nvs_mount(&fs);
	if (ret != 0) {
		LOG_ERR("Failed to mount NVS: %d", ret);
		return ret;
	}

	nvs_initialized = true;
	LOG_INF("NVS initialized: offset=0x%lx, sectors=%d, size=%d KB",
		(unsigned long)fs.offset, fs.sector_count, (fs.sector_size * fs.sector_count) / 1024);

	return 0;
}

/* =============================================================================
 * Public API
 * ============================================================================= */

int config_load(struct device_config *cfg)
{
	int ret;
	size_t read_len;

	if (cfg == NULL) {
		return -EINVAL;
	}

	/* Initialize NVS if needed */
	ret = storage_config_init_nvs();
	if (ret != 0) {
		/* NVS init failed, use defaults */
		memcpy(cfg, &default_config, sizeof(*cfg));
		LOG_WRN("NVS init failed, using defaults");
		return 0;
	}

	/* Read configuration from NVS */
	read_len = sizeof(*cfg);
	ret = nvs_read(&fs, CONFIG_KEY_ID, cfg, read_len);
	if (ret < 0) {
		/* Key not found or error, use defaults */
		memcpy(cfg, &default_config, sizeof(*cfg));
		LOG_INF("Config not found in NVS, using defaults");
		return 0;
	}

	if (ret != read_len) {
		/* Partial read, use defaults */
		memcpy(cfg, &default_config, sizeof(*cfg));
		LOG_WRN("Partial config read (%d/%d bytes), using defaults", ret, read_len);
		return 0;
	}

	/* Validate configuration */
	if (cfg->rtc_trim_ppm > 127 || cfg->rtc_trim_ppm < -127) {
		LOG_WRN("Invalid rtc_trim_ppm (%d), resetting to default", cfg->rtc_trim_ppm);
		cfg->rtc_trim_ppm = default_config.rtc_trim_ppm;
	}

	if (cfg->battery_low_mv < 2500 || cfg->battery_low_mv > 3500) {
		LOG_WRN("Invalid battery_low_mv (%d), resetting to default", cfg->battery_low_mv);
		cfg->battery_low_mv = default_config.battery_low_mv;
	}

	if (cfg->ble_tx_power > 8) {
		LOG_WRN("Invalid ble_tx_power (%d), resetting to default", cfg->ble_tx_power);
		cfg->ble_tx_power = default_config.ble_tx_power;
	}

	LOG_INF("Configuration loaded from NVS");
	return 0;
}

int config_save(const struct device_config *cfg)
{
	int ret;
	int retry_count = 3;

	if (cfg == NULL) {
		return -EINVAL;
	}

	/* Initialize NVS if needed */
	ret = storage_config_init_nvs();
	if (ret != 0) {
		LOG_ERR("NVS not initialized, cannot save");
		return ret;
	}

	/* Write configuration to NVS with retry */
	do {
		ret = nvs_write(&fs, CONFIG_KEY_ID, cfg, sizeof(*cfg));
		if (ret == 0) {
			LOG_INF("Configuration saved to NVS");
			return 0;
		}

		if (ret == -ENOSPC) {
			LOG_ERR("NVS full, cannot save configuration");
			return ret;
		}

		/* Retry on other errors */
		retry_count--;
		k_msleep(10);
	} while (retry_count > 0 && ret < 0);

	LOG_ERR("Failed to save configuration after retries: %d", ret);
	return ret;
}

int config_factory_reset(void)
{
	int ret;

	/* Initialize NVS if needed */
	ret = storage_config_init_nvs();
	if (ret != 0) {
		LOG_ERR("NVS not initialized, cannot reset");
		return ret;
	}

	/* Delete configuration key */
	ret = nvs_delete(&fs, CONFIG_KEY_ID);
	if (ret == 0 || ret == -ENOENT) {
		LOG_INF("Configuration erased (factory reset)");
		return 0;
	}

	LOG_ERR("Failed to erase configuration: %d", ret);
	return ret;
}

int config_print(void)
{
	struct device_config cfg;
	int ret;

	ret = config_load(&cfg);
	if (ret != 0) {
		LOG_ERR("Failed to load config for printing: %d", ret);
		return ret;
	}

	LOG_INF("=== Device Configuration ===");
	LOG_INF("Device name:      %s", cfg.device_name);
	LOG_INF("RTC trim PPM:    %d", cfg.rtc_trim_ppm);
	LOG_INF("Battery low (mV): %d", cfg.battery_low_mv);
	LOG_INF("Log enable:      %d", cfg.log_enable);
	LOG_INF("BLE TX power:    %d dBm", cfg.ble_tx_power);
	LOG_INF("============================");

	return 0;
}

