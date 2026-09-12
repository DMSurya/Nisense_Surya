/**
 * @file max32664b.c
 * @brief MAX32664 Variant B (WHRM) implementation
 *
 * Variant B Details:
 * - Algorithm: WHRM (Wrist Heart Rate Monitor only, no SpO2)
 * - AFE: MAX86140 or MAX86141
 * - Accelerometer: KX-122 or LIS2DS12 (REQUIRED)
 * - Sample Rate: 25Hz
 * - FIFO Entry Size: 16 bytes
 * - WHRM Mode: Motion rejection levels
 *
 * Gate (Track A4): parse_fifo is a stub. This file must not link into
 * HUB builds unless CONFIG_MAX32664_HUB_VARIANT_B_ALLOW_INCOMPLETE=y.
 */

/* Prefer defined() here — this file may be compiled before Zephyr helpers. */
#if !defined(CONFIG_MAX32664_HUB_VARIANT_B_ALLOW_INCOMPLETE)
#error "MAX32664 Variant B HUB FIFO parse is not implemented " \
	"(parse_fifo → -ENOTSUP). Keep CONFIG_MAX32664_HUB_VARIANT_B=n, " \
	"or set CONFIG_MAX32664_HUB_VARIANT_B_ALLOW_INCOMPLETE=y only for " \
	"bring-up experiments."
#endif

#include "max32664_priv.h"

LOG_MODULE_DECLARE(max32664, CONFIG_SENSOR_LOG_LEVEL);

static int max32664b_init(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	uint8_t tx[8], rx[8];
	uint8_t dump_buf[33];
	int ret;
	
	LOG_INF("Initializing Variant B (WHRM - MAX86140/141 + KX-122)...");
	
	/* 1. Enable MAX86140/141 AFE */
	tx[0] = 0x44; /* Family: Enable Sensor */
	tx[1] = 0x00; /* Index: MAX86141 (AFE) */
	tx[2] = 0x01; /* Enable */
	ret = max32664_i2c_transmit_retry(dev, tx, 3, rx, 1, 250, 3);
	if (ret) {
		LOG_ERR("Failed to enable MAX86141 AFE: %d (status=0x%02x)", ret, rx[0]);
		return ret;
	}
	LOG_INF("MAX86141 AFE enabled, status=0x%02x", rx[0]);
	k_msleep(50);
	
	/* 2. Dump MAX86141 registers for diagnostics */
	tx[0] = 0x43;
	tx[1] = 0x00;
	ret = max32664_i2c_transmit(dev, tx, 2, dump_buf, 33, 50);
	if (ret == 0 && dump_buf[0] == 0x00) {
		LOG_HEXDUMP_INF(&dump_buf[1], 32, "MAX86141 Register Dump:");
	}
	
	/* 3. Enable accelerometer (MANDATORY)
	 * ADI UG Sensor Mode Enable (accel row): 0x44 0x04 <en> <host_sel>
	 * — two write bytes, not the 3-byte optical-AFE form.
	 */
	LOG_INF("Enabling KX-122/LIS2DS12 accelerometer (MANDATORY for B)...");
	tx[0] = 0x44;
	tx[1] = 0x04;
	tx[2] = 0x01; /* Enable */
	tx[3] = 0x00; /* Hub-side accel */
	ret = max32664_i2c_transmit_retry(dev, tx, 4, rx, 1, 100, 3);
	if (ret) {
		LOG_ERR("FATAL: Failed to enable accelerometer: %d (status=0x%02x)", ret, rx[0]);
		LOG_ERR("Variant B REQUIRES accelerometer for WHRM algorithm");
		return -ENODEV;
	}
	LOG_INF("Accelerometer enabled, status=0x%02x", rx[0]);
	data->accel_enabled = true;
	k_msleep(50);
	
	/* 4. Enable WHRM algorithm */
	LOG_INF("Checking WHRM algorithm (AFE + Accel fusion)...");
	tx[0] = 0x52;
	tx[1] = 0x07;
	tx[2] = 0x01;
	ret = max32664_i2c_transmit_retry(dev, tx, 3, rx, 1, 500, 3);
	if (ret || rx[0] == 0x02) {
		LOG_INF("WHRM algorithm pre-enabled by firmware (status=0x%02x) - ready", rx[0]);
	} else if (rx[0] == 0x00) {
		LOG_INF("WHRM algorithm enabled successfully, status=0x%02x", rx[0]);
	}
	
	/* 5. Configure WHRM mode */
	if (data->accel_enabled) {
		LOG_INF("Configuring WHRM mode with motion rejection...");
		ret = max32664_configure_whrm_mode(dev, MAX32664_WHRM_MODE_STANDARD);
		if (ret) {
			LOG_WRN("Failed to set WHRM mode: %d - using default", ret);
		}
	}
	
	k_msleep(500);
	return 0;
}

static int max32664b_parse_fifo(const struct device *dev, const uint8_t *data, size_t len)
{
	LOG_ERR("Variant B FIFO parsing not yet implemented");
	return -ENOTSUP;
}

const struct max32664_variant_ops max32664b_ops = {
	.init = max32664b_init,
	.parse_fifo = max32664b_parse_fifo,
	.fifo_entry_size = 16,
	.name = "Variant B (WHRM)",
};
