/**
 * @file max32664_passthrough.c
 * @brief MAX32664 PASSTHROUGH Mode - Hub as I2C bridge for register access
 *
 * In PASSTHROUGH mode the MAX32664 hub provides:
 *   - Register read/write: Family 0x40/0x41 (passthrough access)
 *   - Hub output: PAUSE mode (no hub-packaged sensor stream on Family 0x12)
 *
 * PPG samples: read the MAX86141 FIFO using max86141_read_fifo_burst()
 * (wrapper over max32664_read_fifo_burst). That path performs consecutive
 * Family 0x41 reads with one MFIO wake — suitable for validating passthrough
 * and FIFO draining. It is intentionally **not** used from RAW mode
 * (max32664_raw_read_ppg_ex uses hub output FIFO Family 0x12 only).
 *
 * Note: looping max86141_read_reg(FIFO_DATA) one byte at a time is not the
 * same as the burst helper and can return repeated first bytes; use the burst
 * API for FIFO_DATA.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "max32664_priv.h"
#include "max32664_passthrough.h"
#include "max86141_regs.h"
#include "lis2ds12_regs.h"

LOG_MODULE_DECLARE(max32664, CONFIG_SENSOR_LOG_LEVEL);

/* ============================================================================
 * Hub Output Mode Control
 * ============================================================================ */

/**
 * @brief Set the hub output mode
 *
 * @param dev  MAX32664 hub device
 * @param mode 0x00=PAUSE, 0x01=SENSOR_DATA, 0x02=ALGO, 0x03=BOTH
 * @return 0 on success, negative errno on failure
 */
int max32664_passthrough_set_output_mode(const struct device *dev, uint8_t mode)
{
	uint8_t tx[3] = {0x10, 0x00, mode};
	uint8_t rx[1];
	int ret;

	ret = max32664_i2c_transmit(dev, tx, 3, rx, 1, 2);
	if (ret) {
		LOG_ERR("Failed to set output mode 0x%02X: %d", mode, ret);
		return ret;
	}
	if (rx[0] != 0x00) {
		LOG_WRN("Set output mode 0x%02X status=0x%02X", mode, rx[0]);
	}

	return 0;
}

/* ============================================================================
 * Passthrough Mode Enable (output PAUSE, algorithm managed separately)
 * ============================================================================ */

int max32664_passthrough_mode_enable(const struct device *dev)
{
	int ret;

	LOG_INF("Enabling PASSTHROUGH mode...");

	/* Set output mode to PAUSE initially.
	 * Algorithm will be enabled when measurement starts. */
	ret = max32664_passthrough_set_output_mode(dev, 0x00);
	if (ret) {
		LOG_ERR("Failed to set PAUSE mode: %d", ret);
		return ret;
	}

	LOG_INF("Hub in PAUSE mode (algorithm will be enabled on measurement start)");
	return 0;
}

/* ============================================================================
 * Passthrough Mode Initialization
 * ============================================================================ */

int max32664_passthrough_mode_init(const struct device *dev)
{
	int ret;

	LOG_INF("Initializing PASSTHROUGH mode (I2C bridge for register access)...");

	/* Put hub in PAUSE mode — hub acts as I2C bridge only.
	 * Sensors were already enabled during variant detection (Family 0x44).
	 * Register access is via Family 0x40 (write) / 0x41 (read). */
	ret = max32664_passthrough_mode_enable(dev);
	if (ret) {
		LOG_ERR("Failed to enable passthrough mode: %d", ret);
		return ret;
	}

	LOG_INF("PASSTHROUGH initialization complete (AFE FIFO via burst read when measuring)");
	return 0;
}

int max32664_passthrough_read_ppg_ex(const struct device *dev,
				     uint32_t *ir_value, uint32_t *red_value,
				     uint32_t *green_value)
{
	uint8_t cnt;
	int ret;
	int drained = 0;
	static bool path_logged;

	if (!path_logged) {
		LOG_INF("PASSTHROUGH PPG: MAX86141 FIFO via hub burst read "
			"(not hub Family 0x12 — use CONFIG_MAX32664_MODE_RAW for that path)");
		path_logged = true;
	}

	ret = max86141_read_reg(dev, MAX86141_REG_FIFO_DATA_CNT, &cnt);
	if (ret) {
		return ret;
	}

	/* FIFO_DATA_CNT = bytes in FIFO; expect contiguous LED words only (no FIFO
	 * timestamp tags). max86141_ppg forces PPG_SYNC_CTRL timestamp off in
	 * PASSTHROUGH so each sample is 6 bytes (2× 19-bit) for this LED config. */
	if (cnt < 6) {
		return 0;
	}

	uint32_t ir = 0;
	uint32_t red = 0;

	while (cnt >= 6 && drained < 48) {
		uint8_t raw[6];

		ret = max86141_read_fifo_burst(dev, raw, sizeof(raw));
		if (ret) {
			return ret;
		}

		ir = ((uint32_t)raw[0] << 16) | ((uint32_t)raw[1] << 8) | raw[2];
		ir &= 0x7FFFFU;
		red = ((uint32_t)raw[3] << 16) | ((uint32_t)raw[4] << 8) | raw[5];
		red &= 0x7FFFFU;
		drained++;

		ret = max86141_read_reg(dev, MAX86141_REG_FIFO_DATA_CNT, &cnt);
		if (ret) {
			return ret;
		}
	}

	*ir_value = ir;
	*red_value = red;
	if (green_value) {
		/* Direct AFE FIFO burst still reads 2 LED words; green via hub RAW path. */
		*green_value = 0;
	}

	return 1;
}

int max32664_passthrough_read_ppg(const struct device *dev,
				  uint32_t *ir, uint32_t *red)
{
	return max32664_passthrough_read_ppg_ex(dev, ir, red, NULL);
}

/* ============================================================================
 * Passthrough Accelerometer Reading (direct LIS2DS12 registers)
 * ============================================================================ */

int max32664_passthrough_read_accel(const struct device *dev,
				    int16_t *x, int16_t *y, int16_t *z)
{
	uint8_t data[6];
	int ret;

	for (int i = 0; i < 6; i++) {
		ret = lis2ds12_read_reg(dev, LIS2DS12_REG_OUT_X_L + i, &data[i]);
		if (ret) {
			LOG_ERR("Accel reg 0x%02X read failed: %d",
				LIS2DS12_REG_OUT_X_L + i, ret);
			return ret;
		}
	}

	int16_t raw_x = (int16_t)((data[1] << 8) | data[0]);
	int16_t raw_y = (int16_t)((data[3] << 8) | data[2]);
	int16_t raw_z = (int16_t)((data[5] << 8) | data[4]);

	/* Convert to milli-g at ±2g range (0.061 mg/LSB for 16-bit) */
	*x = (raw_x * 61) / 1000;
	*y = (raw_y * 61) / 1000;
	*z = (raw_z * 61) / 1000;

	return 0;
}

/* ============================================================================
 * Passthrough Sensor Configuration (used by RAW mode init, not PASSTHROUGH)
 *
 * In PASSTHROUGH mode the child drivers (max86141_ppg.c, lis2ds12_accel.c)
 * configure sensors directly. These functions exist for RAW mode compatibility
 * where there may not be separate child sensor devices.
 * ============================================================================ */

int max32664_passthrough_configure_ppg(const struct device *dev)
{
	int ret;
	uint8_t val;

	LOG_INF("Configuring MAX86141 (basic RAW mode config)...");

	ret = max86141_read_reg(dev, MAX86141_REG_SYSTEM_CTRL, &val);
	if (ret) {
		LOG_ERR("SYSTEM_CTRL read failed: %d", ret);
		return ret;
	}
	LOG_DBG("SYSTEM_CTRL before config: 0x%02X", val);

	/* FIFO: 128 samples, rollover enabled */
	ret = max86141_write_reg(dev, MAX86141_REG_FIFO_CONFIG1, 0x7F);
	if (ret) return ret;

	/* PPG_CONFIG1: ADC ranges + PPG_TINT (not sample rate / LED count) */
	ret = max86141_write_reg(dev, MAX86141_REG_PPG_CONFIG1, 0x1A);
	if (ret) return ret;

	/* PPG_CONFIG2: Sample average = 8 */
	ret = max86141_write_reg(dev, MAX86141_REG_PPG_CONFIG2, 0x03);
	if (ret) return ret;

	/* LED sequence: LED1 (IR), LED2 (Red) */
	ret = max86141_write_reg(dev, MAX86141_REG_LED_SEQ1, 0x21);
	if (ret) return ret;

	/* LED currents */
	ret = max86141_write_reg(dev, MAX86141_REG_LED1_DRV_CUR, 0x40);
	if (ret) return ret;
	ret = max86141_write_reg(dev, MAX86141_REG_LED2_DRV_CUR, 0x40);
	if (ret) return ret;

	/* Start sampling */
	ret = max86141_write_reg(dev, MAX86141_REG_SYSTEM_CTRL, 0x00);
	if (ret) return ret;
	k_msleep(50);

	/* Verify */
	max86141_read_reg(dev, MAX86141_REG_SYSTEM_CTRL, &val);
	LOG_DBG("SYSTEM_CTRL after config: 0x%02X", val);

	LOG_INF("MAX86141 configured: 25Hz, 2-LED (IR+Red)");
	return 0;
}

int max32664_passthrough_configure_accel(const struct device *dev)
{
	int ret;
	uint8_t whoami;

	LOG_INF("Configuring LIS2DS12 (basic RAW mode config)...");

	ret = lis2ds12_read_reg(dev, LIS2DS12_REG_WHO_AM_I, &whoami);
	if (ret || (whoami != LIS2DS12_WHOAMI_VALUE && whoami != LIS2DS12_WHOAMI_ALT)) {
		LOG_ERR("LIS2DS12 WHOAMI failed: ret=%d, whoami=0x%02X", ret, whoami);
		return ret ? ret : -ENODEV;
	}
	LOG_DBG("LIS2DS12 detected: WHOAMI=0x%02X", whoami);

	/* CTRL1: ODR = 25Hz, ±2g, high-resolution */
	ret = lis2ds12_write_reg(dev, LIS2DS12_REG_CTRL1, 0x30);
	if (ret) return ret;

	/* CTRL2: Defaults */
	ret = lis2ds12_write_reg(dev, LIS2DS12_REG_CTRL2, 0x00);
	if (ret) return ret;

	LOG_INF("LIS2DS12 configured: 25Hz, ±2g");
	return 0;
}

/* ============================================================================
 * Combined Sample Reading (convenience for application code)
 * ============================================================================ */

int max32664_passthrough_read_sample(const struct device *dev,
				     struct max32664_passthrough_sample *sample)
{
	int ret;

	if (!sample) {
		return -EINVAL;
	}

	memset(sample, 0, sizeof(*sample));
	sample->timestamp_ms = k_uptime_get();

	ret = max32664_passthrough_read_ppg(dev, &sample->ppg.ir_raw,
					    &sample->ppg.red_raw);
	if (ret < 0) {
		LOG_ERR("PPG read failed: %d", ret);
		return ret;
	}
	sample->ppg.valid = (ret > 0);

	ret = max32664_passthrough_read_accel(dev,
					      &sample->accel.x_raw,
					      &sample->accel.y_raw,
					      &sample->accel.z_raw);
	if (ret < 0) {
		LOG_DBG("Accel read failed: %d (continuing without)", ret);
		sample->accel.x_raw = 0;
		sample->accel.y_raw = 0;
		sample->accel.z_raw = 0;
	}

	return 0;
}
