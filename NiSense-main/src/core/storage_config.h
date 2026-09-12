/* =============================================================================
 * Storage Config Module - Header
 * =============================================================================
 * Provides persistent configuration storage using NVS
 * Target: nRF52840 (Raytac MDBT50Q-DB-40)
 * ============================================================================= */

#ifndef STORAGE_CONFIG_H
#define STORAGE_CONFIG_H

#include <stdint.h>

/* Device configuration structure */
struct device_config {
	char device_name[32];        /* BLE advertise name */
	int32_t rtc_trim_ppm;        /* RTC trim ppm (±127 OSCTRIM); Zephyr polarity */
	uint16_t battery_low_mv;     /* Low battery threshold (2800–3500) */
	uint8_t log_enable;          /* Enable LittleFS logging */
	uint8_t ble_tx_power;        /* BLE TX power dBm (0–8) */
	uint8_t reserved[8];         /* Future fields */
};

/**
 * @brief Load configuration from NVS
 * @param cfg Pointer to device_config structure to fill
 * @return 0 on success, negative errno on failure
 */
int config_load(struct device_config *cfg);

/**
 * @brief Save configuration to NVS
 * @param cfg Pointer to device_config structure to save
 * @return 0 on success, negative errno on failure
 */
int config_save(const struct device_config *cfg);

/**
 * @brief Factory reset (erase configuration)
 * @return 0 on success, negative errno on failure
 */
int config_factory_reset(void);

/**
 * @brief Print current configuration (debug)
 * @return 0 on success, negative errno on failure
 */
int config_print(void);

#endif /* STORAGE_CONFIG_H */

