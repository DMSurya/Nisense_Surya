/**
 * @file max32664_init.c
 * @brief MAX32664 initialization and probe helpers
 */

#include "max32664_priv.h"
#include <zephyr/drivers/regulator.h>

LOG_MODULE_DECLARE(max32664, CONFIG_SENSOR_LOG_LEVEL);

int max32664_init_regulator_power_cycle(const struct device *dev)
{
	const struct max32664_config *cfg = dev->config;
	int ret;

	if (cfg->vin_supply == NULL || !device_is_ready(cfg->vin_supply)) {
		LOG_DBG("No vin-supply configured for power cycle");
		return -ENOTSUP;
	}

	LOG_INF("Power cycling hub via regulator...");

	ret = regulator_disable(cfg->vin_supply);
	if (ret && ret != -EALREADY) {
		LOG_WRN("Failed to disable regulator: %d", ret);
	}

	k_msleep(200);

	ret = regulator_enable(cfg->vin_supply);
	if (ret && ret != -EALREADY) {
		LOG_ERR("Failed to enable regulator: %d", ret);
		return ret;
	}

	LOG_INF("Waiting %u ms for hub boot (AN6924 Variant C)...",
		MAX32664_APP_INIT_MS);
	k_msleep(MAX32664_APP_INIT_MS);

	LOG_INF("Hub power cycle complete");
	return 0;
}

int max32664_init_software_reset(const struct device *dev)
{
	uint8_t tx[3], rx[2];
	int ret;

	LOG_INF("Attempting software reset via bootloader mode...");

	tx[0] = 0x01;
	tx[1] = 0x00;
	tx[2] = 0x02;
	ret = max32664_i2c_transmit(dev, tx, 3, rx, 1, 2000);

	if (ret == 0 && rx[0] == 0x00) {
		LOG_INF("Software reset command accepted, waiting for reboot...");
		k_msleep(MAX32664_APP_INIT_MS);
		return 0;
	}

	LOG_DBG("Reset command failed (ret=%d, status=0x%02x), trying bootloader toggle...",
		ret, rx[0]);

	tx[0] = 0x01;
	tx[1] = 0x00;
	tx[2] = 0x08;
	ret = max32664_i2c_transmit(dev, tx, 3, rx, 1, 1000);
	if (ret != 0) {
		LOG_DBG("Bootloader entry failed: ret=%d, status=0x%02x", ret, rx[0]);
		return ret;
	}

	k_msleep(500);

	tx[0] = 0x01;
	tx[1] = 0x00;
	tx[2] = 0x00;
	ret = max32664_i2c_transmit(dev, tx, 3, rx, 1, 2000);
	if (ret != 0) {
		LOG_DBG("Bootloader exit failed: ret=%d, status=0x%02x", ret, rx[0]);
		return ret;
	}

	LOG_INF("Bootloader toggle complete, waiting for app mode...");
	k_msleep(MAX32664_APP_INIT_MS);

	return 0;
}

int max32664_init_read_mcu_type(const struct device *dev, uint8_t *mcu_type)
{
	uint8_t tx[2] = {0xFF, 0x00};
	uint8_t rx[2];
	int ret;

	ret = max32664_i2c_transmit_retry(dev, tx, sizeof(tx), rx, sizeof(rx), 10, 3);
	if (ret) {
		return ret;
	}

	*mcu_type = rx[1];
	return 0;
}

int max32664_init_read_fw_version(const struct device *dev,
				    uint8_t *major, uint8_t *minor, uint8_t *patch)
{
	uint8_t tx[2] = {0xFF, 0x03};
	uint8_t rx[4];
	int ret;

	ret = max32664_i2c_transmit_retry(dev, tx, sizeof(tx), rx, sizeof(rx), 10, 3);
	if (ret) {
		return ret;
	}

	*major = rx[1];
	*minor = rx[2];
	*patch = rx[3];

	return 0;
}

int max32664_init_configure_gpios(const struct max32664_config *cfg)
{
	int ret;

	if (cfg->rstn_gpio.port != NULL) {
		if (!gpio_is_ready_dt(&cfg->rstn_gpio)) {
			LOG_ERR("RSTN GPIO device not ready");
			return -ENODEV;
		}
		ret = gpio_pin_configure_dt(&cfg->rstn_gpio, GPIO_OUTPUT);
		if (ret) {
			LOG_ERR("Failed to configure RSTN GPIO: %d", ret);
			return ret;
		}
		LOG_INF("RSTN GPIO configured");
	} else {
		LOG_INF("I2C-only mode: RSTN not controlled (assuming external reset)");
	}

	if (cfg->int_gpio.port != NULL) {
		if (!gpio_is_ready_dt(&cfg->int_gpio)) {
			LOG_ERR("MFIO GPIO device not ready");
			return -ENODEV;
		}
		ret = gpio_pin_configure_dt(&cfg->int_gpio, GPIO_OUTPUT);
		if (ret) {
			LOG_ERR("Failed to configure MFIO GPIO: %d", ret);
			return ret;
		}
		LOG_INF("MFIO GPIO configured for wake-up protocol");
	} else {
		LOG_INF("I2C-only mode: MFIO not connected (hub stays awake)");
	}

	return 0;
}

int max32664_init_reset_sequence(const struct max32664_config *cfg)
{
	/*
	 * AN6924 Reset to Application (~p.10):
	 * RSTN assert ≥10 ms; MFIO inactive-high ≥1 ms before RSTN release;
	 * +50 ms app-mode settle; then MAX32664_APP_INIT_MS ready wait.
	 * DT assumes GPIO_ACTIVE_LOW on both pins (logical 1 = phys LOW).
	 */
	if (cfg->rstn_gpio.port != NULL) {
		LOG_INF("Performing hardware reset sequence (AN6924 app entry)...");
		gpio_pin_set_dt(&cfg->rstn_gpio, 1); /* assert RSTN → phys LOW */
		k_msleep(MAX32664_RSTN_ASSERT_MS);
		if (cfg->int_gpio.port != NULL) {
			gpio_pin_set_dt(&cfg->int_gpio, 0); /* MFIO high for app mode */
		}
		k_msleep(MAX32664_MFIO_SETUP_BEFORE_RSTN_MS);
		gpio_pin_set_dt(&cfg->rstn_gpio, 0); /* release RSTN */
		k_msleep(MAX32664_APP_MODE_SETTLE_MS);
	} else {
		if (cfg->int_gpio.port != NULL) {
			gpio_pin_set_dt(&cfg->int_gpio, 0);
		}
		k_msleep(250);
	}
	k_msleep(MAX32664_APP_INIT_MS);
	return 0;
}

int max32664_init_check_device_mode(const struct device *dev, uint8_t *mode_out)
{
	uint8_t tx[2] = {0x02, 0x00};
	uint8_t rx[3];
	int ret = max32664_i2c_transmit_retry(dev, tx, 2, rx, 3, 10, 3);

	if (ret) {
		LOG_ERR("Failed to read device mode: %d", ret);
		return ret;
	}

	uint8_t current_mode = rx[2];
	if (current_mode == 0x02) {
		LOG_ERR("Device in reset mode - POWER CYCLE REQUIRED (no RSTN pin available)");
		return -ENODEV;
	} else if (current_mode == 0x08) {
		LOG_INF("Device in bootloader mode - attempting exit to application...");
		uint8_t tx2[3] = {0x01, 0x00, 0x00};
		uint8_t rx2[1];

		ret = max32664_i2c_transmit_retry(dev, tx2, 3, rx2, 1, 50, 3);
		if (ret) {
			LOG_ERR("Failed to exit bootloader mode: %d", ret);
			return ret;
		}

		k_msleep(100);
		ret = max32664_i2c_transmit_retry(dev, tx, 2, rx, 3, 10, 3);
		if (ret) {
			return ret;
		}
		current_mode = rx[2];
	}

	*mode_out = current_mode;
	return 0;
}

int max32664_init_read_and_log_mcu_type(const struct device *dev, struct max32664_data *data)
{
	uint8_t mcu_type;
	int ret = max32664_init_read_mcu_type(dev, &mcu_type);

	if (!ret) {
		const char *mcu_str = (mcu_type == MAX32664_MCU_TYPE_MAX32625) ? "MAX32625" :
				      (mcu_type == MAX32664_MCU_TYPE_MAX32660_64) ? "MAX32660/MAX32664" : "Unknown";
		LOG_INF("MCU Type: 0x%02x (%s family)", mcu_type, mcu_str);
		data->mcu_type = mcu_type;
	}

	return ret;
}

int max32664_init_read_and_log_fw_version(const struct device *dev,
					   struct max32664_data *data,
					   uint8_t *major, uint8_t *minor, uint8_t *patch)
{
	int ret = max32664_init_read_fw_version(dev, major, minor, patch);

	if (!ret) {
		LOG_INF("Firmware version: %u.%u.%u", *major, *minor, *patch);
		data->firmware_version = (*major << 16) | (*minor << 8) | *patch;
		if (*major >= 40) {
			LOG_INF("Firmware v40.x detected: Waiting 3000ms for hub stabilization...");
			k_msleep(3000);
		}
	}

	return ret;
}

int max32664_init_read_hardware_whoami(const struct device *dev, struct max32664_data *data)
{
	int ret;

	if (data->variant == MAX32664_VARIANT_A || data->variant == MAX32664_VARIANT_D) {
		ret = max32664_read_sensor_reg(dev, 0x03, 0xFF, &data->afe_whoami);
		if (ret) {
			LOG_WRN("Failed to read MAX3010x Part ID: %d", ret);
			data->afe_whoami = 0x00;
		} else {
			/* Part ID 0x15 = MAX30101/30102/30105 (DS); 0x11 = MAX30100 only. */
			const char *afe_name =
				(data->afe_whoami == MAX32664_AFE_WHOAMI_MAX3010X) ? "MAX3010x (30101/02/05)" :
				(data->afe_whoami == MAX32664_AFE_WHOAMI_MAX30100) ? "MAX30100" : "UNKNOWN";
			LOG_INF("AFE: %s (Part ID=0x%02X)", afe_name, data->afe_whoami);
		}
	} else if (data->variant == MAX32664_VARIANT_B || data->variant == MAX32664_VARIANT_C) {
		ret = max32664_read_sensor_reg(dev, 0x00, 0xFF, &data->afe_whoami);
		if (ret) {
			LOG_WRN("Failed to read MAX86140/141 WHOAMI: %d", ret);
			data->afe_whoami = 0x00;
		} else {
			const char *afe_name = (data->afe_whoami == MAX32664_AFE_WHOAMI_MAX86140) ? "MAX86140" :
					       (data->afe_whoami == MAX32664_AFE_WHOAMI_MAX86141) ? "MAX86141" :
					       (data->afe_whoami == MAX32664_AFE_WHOAMI_MAX86161) ? "MAXM86161" :
					       (data->afe_whoami == MAX32664_AFE_WHOAMI_MAXM86146) ? "MAXM86146" : "UNKNOWN";
			LOG_INF("AFE: %s (WHOAMI=0x%02X)", afe_name, data->afe_whoami);
		}
	}

	if (data->accel_required) {
		ret = max32664_read_sensor_reg(dev, 0x04, 0x0F, &data->accel_whoami);
		if (ret) {
			LOG_WRN("Failed to read accelerometer WHOAMI: %d", ret);
			data->accel_whoami = 0x00;
		} else {
			const char *accel_name = (data->accel_whoami == MAX32664_ACCEL_WHOAMI_KX122) ? "KX-122" :
						 (data->accel_whoami == MAX32664_ACCEL_WHOAMI_LIS2DS12) ? "LIS2DS12" : "UNKNOWN";
			LOG_INF("Accel: %s (WHOAMI=0x%02X) - %s",
				accel_name, data->accel_whoami,
				data->accel_required ? "REQUIRED" : "Optional");
		}
	}

	return 0;
}

int max32664_init_detect_and_log_variant(const struct device *dev, struct max32664_data *data)
{
	const struct max32664_config *cfg = dev->config;
	uint8_t fw_major = (data->firmware_version >> 16) & 0xFF;
	const char *variant_names[] = {"UNKNOWN", "A", "B", "C", "D"};
	const char *algo_names[] = {"", "MaximFast", "WHRM", "WHRM+WSpO2", "BPT"};
	const char *hint_names[] = {"none", "gwec", "gwed"};
	enum max32664_variant fw_variant;

	data->calibration_required = false;
	data->accel_required = false;

	/* Primary classification: firmware major (Maxim packaging convention). */
	if (fw_major < 20) {
		fw_variant = MAX32664_VARIANT_A;
	} else if (fw_major == 20) {
		fw_variant = MAX32664_VARIANT_B;
		data->accel_required = true;
	} else if (fw_major >= 30 && fw_major <= 32) {
		fw_variant = MAX32664_VARIANT_C;
		data->accel_required = true;
	} else if (fw_major >= 40) {
		fw_variant = MAX32664_VARIANT_D;
		data->calibration_required = true;
	} else {
		fw_variant = MAX32664_VARIANT_UNKNOWN;
	}

	data->variant = fw_variant;

	LOG_INF("Variant classify: FW %u.%u.%u → %s; DT hint=%s",
		(data->firmware_version >> 16) & 0xFF,
		(data->firmware_version >> 8) & 0xFF,
		data->firmware_version & 0xFF,
		variant_names[fw_variant],
		hint_names[cfg->variant_hint]);

	/*
	 * DT variant-hint vs FW-major: hard fail on conflict (no silent override).
	 * gwec = wrist family (B/C); gwed = finger family (A/D).
	 */
	if (cfg->variant_hint == MAX32664_HINT_GWEC) {
		if (fw_variant == MAX32664_VARIANT_UNKNOWN) {
			LOG_WRN("FW major unrecognised; applying DT hint gwec → Variant C");
			data->variant = MAX32664_VARIANT_C;
			data->accel_required = true;
			data->calibration_required = false;
		} else if (fw_variant != MAX32664_VARIANT_B &&
			   fw_variant != MAX32664_VARIANT_C) {
			LOG_ERR("Variant conflict: DT hint=gwec (expect B/C) but FW major "
				"classified Variant %s — refusing init",
				variant_names[fw_variant]);
			return -ENODEV;
		}
	} else if (cfg->variant_hint == MAX32664_HINT_GWED) {
		if (fw_variant == MAX32664_VARIANT_UNKNOWN) {
			LOG_WRN("FW major unrecognised; applying DT hint gwed → Variant D");
			data->variant = MAX32664_VARIANT_D;
			data->accel_required = false;
			data->calibration_required = true;
		} else if (fw_variant != MAX32664_VARIANT_A &&
			   fw_variant != MAX32664_VARIANT_D) {
			LOG_ERR("Variant conflict: DT hint=gwed (expect A/D) but FW major "
				"classified Variant %s — refusing init",
				variant_names[fw_variant]);
			return -ENODEV;
		}
	} else if (fw_variant == MAX32664_VARIANT_UNKNOWN) {
		LOG_ERR("Variant UNKNOWN (FW major %u) and no DT variant-hint — refusing init",
			fw_major);
		return -ENODEV;
	}

#if IS_ENABLED(CONFIG_MAX32664_MODE_HUB) && !IS_ENABLED(CONFIG_MAX32664_HUB_VARIANT_B)
	if (data->variant == MAX32664_VARIANT_B) {
		LOG_ERR("HUB Variant B is build-gated (FIFO parse stub). "
			"Use Variant C FW / RAW mode, or enable "
			"CONFIG_MAX32664_HUB_VARIANT_B + "
			"CONFIG_MAX32664_HUB_VARIANT_B_ALLOW_INCOMPLETE");
		return -ENOTSUP;
	}
#endif

	/*
	 * Capability probe (HUB only): briefly enable the matching algo index.
	 *
	 * RAW / PASSTHROUGH never arm WHRM here — FW major + DT hint already
	 * classify, and enabling then disarming races raw_mode_init()'s
	 * stop_algorithms (Track B sensor-only ownership).
	 */
#if defined(CONFIG_MAX32664_MODE_RAW) || defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
	LOG_INF("Algo enable-probe skipped (%s mode — sensor-only / no WHRM arm)",
#if defined(CONFIG_MAX32664_MODE_RAW)
		"RAW"
#else
		"PASSTHROUGH"
#endif
	);
#else /* CONFIG_MAX32664_MODE_HUB */
	{
		uint8_t algo_idx = 0xFF;
		uint8_t tx[3], rx[2];
		int ret_probe;
		bool probe_ok = false;
		bool probe_attempted = false;

		switch (data->variant) {
		case MAX32664_VARIANT_A:
			algo_idx = 0x00;
			break;
		case MAX32664_VARIANT_B:
			algo_idx = 0x01;
			break;
		case MAX32664_VARIANT_C:
			algo_idx = 0x02;
			break;
		case MAX32664_VARIANT_D:
			algo_idx = 0x03;
			break;
		default:
			break;
		}

		if (algo_idx != 0xFF) {
			probe_attempted = true;
			tx[0] = 0x52;
			tx[1] = algo_idx;
			tx[2] = 0x01;
			ret_probe = max32664_i2c_transmit(dev, tx, 3, rx, 1, 100);
			/*
			 * 0x00 = accepted; 0x02 = ERR_UNAVAIL_FUNC / already armed on some
			 * GWEC builds (still corroborates the algo index exists).
			 */
			if (ret_probe == 0 && (rx[0] == 0x00 || rx[0] == 0x02)) {
				probe_ok = true;
			} else {
				LOG_WRN("Variant %s algo probe failed: ret=%d status=0x%02x",
					variant_names[data->variant], ret_probe,
					(ret_probe == 0) ? rx[0] : 0xFF);
			}

			/* Disarm so a later HUB start does not inherit a half-armed probe. */
			tx[2] = 0x00;
			(void)max32664_i2c_transmit(dev, tx, 3, rx, 1, 100);
		}

		if (probe_attempted && !probe_ok) {
			if (cfg->variant_hint != MAX32664_HINT_NONE) {
				LOG_WRN("Algo probe failed but DT hint=%s agrees with Variant %s — continuing",
					hint_names[cfg->variant_hint], variant_names[data->variant]);
			} else {
				LOG_ERR("Algo probe failed and no DT variant-hint to corroborate "
					"FW classification (Variant %s) — refusing init",
					variant_names[data->variant]);
				return -ENODEV;
			}
		}

		LOG_INF("Firmware: %d.%d.%d, Variant: MAX32664%s (hint=%s, probe=%s)",
			(data->firmware_version >> 16) & 0xFF,
			(data->firmware_version >> 8) & 0xFF,
			data->firmware_version & 0xFF,
			variant_names[data->variant],
			hint_names[cfg->variant_hint],
			!probe_attempted ? "n/a" : (probe_ok ? "ok" : "failed+hint"));
	}
#endif /* HUB vs RAW/PASSTHROUGH */

#if defined(CONFIG_MAX32664_MODE_RAW) || defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
	LOG_INF("Firmware: %d.%d.%d, Variant: MAX32664%s (hint=%s, probe=skipped)",
		(data->firmware_version >> 16) & 0xFF,
		(data->firmware_version >> 8) & 0xFF,
		data->firmware_version & 0xFF,
		variant_names[data->variant],
		hint_names[cfg->variant_hint]);
#endif

	int ret_whoami = max32664_init_read_hardware_whoami(dev, data);
	if (ret_whoami) {
		LOG_WRN("Failed to read hardware WHOAMI: %d", ret_whoami);
	}

	if (data->variant != MAX32664_VARIANT_UNKNOWN) {
		LOG_INF("Algorithm: %s @ %dHz",
			algo_names[data->variant],
			(data->variant == MAX32664_VARIANT_A ||
			 data->variant == MAX32664_VARIANT_D) ? 100 : 25);
		if (data->calibration_required && !data->calibration_done) {
			LOG_WRN(" BPT calibration required before BP readings valid");
		}
	}

	return 0;
}

int max32664_init_variant_init(const struct device *dev, struct max32664_data *data)
{
#if defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
	return max32664_passthrough_mode_init(dev);
#elif defined(CONFIG_MAX32664_MODE_RAW)
	LOG_INF("Initializing in RAW mode (hub passthrough access)...");
	int ret = max32664_raw_mode_init(dev);
	if (ret) {
		LOG_ERR("RAW mode initialization failed: %d", ret);
		return ret;
	}
	LOG_INF("RAW mode initialization complete");
	return 0;
#else
	const struct max32664_variant_ops *ops = max32664_get_variant_ops(data->variant);
	if (!ops || !ops->init) {
		LOG_ERR("No initialization function for variant %d", data->variant);
		return -ENOTSUP;
	}
	int ret = ops->init(dev);
	if (ret) {
		LOG_ERR("Variant %s initialization failed: %d", ops->name, ret);
		return ret;
	}
	LOG_INF("Variant %s hardware initialization complete", ops->name);
	k_msleep(500);
	return 0;
#endif
}
