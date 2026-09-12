/**
 * @file lis2ds12_accel.c
 * @brief LIS2DS12 Accelerometer - Zephyr Sensor API via MAX32664 Hub Passthrough
 *
 * This driver exposes the LIS2DS12 accelerometer as a standard Zephyr sensor device.
 * It accesses the LIS2DS12 through the MAX32664 hub's passthrough mode.
 *
 * Supported Channels:
 *   - SENSOR_CHAN_ACCEL_X: X-axis acceleration (m/s²)
 *   - SENSOR_CHAN_ACCEL_Y: Y-axis acceleration (m/s²)
 *   - SENSOR_CHAN_ACCEL_Z: Z-axis acceleration (m/s²)
 *   - SENSOR_CHAN_ACCEL_XYZ: All axes at once
 *
 * Usage:
 *   const struct device *accel_dev = DEVICE_DT_GET(DT_NODELABEL(lis2ds12_accel));
 *   sensor_sample_fetch(accel_dev);
 *   sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_XYZ, accel_vals);
 *
 * Note: Built when CONFIG_MAX32664_RAW_ACCEL_SENSOR=y (RAW mode; see Kconfig).
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT st_lis2ds12_accel

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#include "max32664.h"
#include "max32664_priv.h"
#include "lis2ds12_regs.h"

LOG_MODULE_REGISTER(lis2ds12_accel, CONFIG_LIS2DS12_ACCEL_LOG_LEVEL);

/* CTRL1 register bits */
#define LIS2DS12_ODR_POS            4
#define LIS2DS12_FS_POS             2
#define LIS2DS12_HF_ODR_POS         1
#define LIS2DS12_BDU_POS            0

/* Output data rates (CTRL1 ODR[3:0]) */
#define LIS2DS12_ODR_POWER_DOWN     0x00
#define LIS2DS12_ODR_1HZ            0x01
#define LIS2DS12_ODR_12_5HZ         0x02
#define LIS2DS12_ODR_25HZ           0x03
#define LIS2DS12_ODR_50HZ           0x04
#define LIS2DS12_ODR_100HZ          0x05
#define LIS2DS12_ODR_200HZ          0x06
#define LIS2DS12_ODR_400HZ          0x07
#define LIS2DS12_ODR_800HZ          0x08

/* Full scale selection (CTRL1 FS[1:0]) */
#define LIS2DS12_FS_2G              0x00
#define LIS2DS12_FS_4G              0x02
#define LIS2DS12_FS_8G              0x03
#define LIS2DS12_FS_16G             0x01

/* Gravity constant (m/s²) */
#define GRAVITY_MS2                 9806650  /* micro-m/s² */

/* Sensitivity values in micro-g/LSB for 14-bit output */
#define LIS2DS12_SENS_2G            61    /* 0.061 mg/LSB */
#define LIS2DS12_SENS_4G            122   /* 0.122 mg/LSB */
#define LIS2DS12_SENS_8G            244   /* 0.244 mg/LSB */
#define LIS2DS12_SENS_16G           488   /* 0.488 mg/LSB */

/* ============================================================================
 * Driver Configuration
 * ============================================================================ */

struct lis2ds12_accel_config {
	const struct device *hub_dev;  /* Parent MAX32664 device */
	uint8_t odr;                   /* Output data rate */
	uint8_t fs;                    /* Full scale selection */
};

struct lis2ds12_accel_data {
	int16_t x_raw;      /* Raw X-axis (14-bit left-justified in 16-bit) */
	int16_t y_raw;      /* Raw Y-axis */
	int16_t z_raw;      /* Raw Z-axis */
	uint8_t fs;         /* Current full scale setting */
	bool configured;    /* Configuration complete flag */
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get sensitivity in micro-g/LSB based on full scale
 */
static int32_t lis2ds12_get_sensitivity(uint8_t fs)
{
	switch (fs) {
	case LIS2DS12_FS_2G:
		return LIS2DS12_SENS_2G;
	case LIS2DS12_FS_4G:
		return LIS2DS12_SENS_4G;
	case LIS2DS12_FS_8G:
		return LIS2DS12_SENS_8G;
	case LIS2DS12_FS_16G:
		return LIS2DS12_SENS_16G;
	default:
		return LIS2DS12_SENS_2G;
	}
}

/**
 * @brief Convert raw value to m/s² sensor_value
 *
 * @param raw Raw 14-bit accelerometer value (left-justified in 16-bit)
 * @param sensitivity Sensitivity in micro-g/LSB
 * @param val Output sensor_value in m/s²
 */
static void lis2ds12_raw_to_ms2(int16_t raw, int32_t sensitivity,
				struct sensor_value *val)
{
	/* The LIS2DS12 sensitivity constants (e.g. 61 µg/LSB for ±2g) are
	 * defined for the 16-bit left-justified register value directly.
	 * Do NOT right-shift by 2; that would divide the result by 4. */

	/* Convert to micro-g: raw * sensitivity (micro-g/LSB) */
	int32_t micro_g = (int32_t)raw * sensitivity;
	
	/* Convert to m/s²: micro_g * GRAVITY / 1_000_000 */
	/* To preserve precision: (micro_g * GRAVITY_MS2) / 1_000_000 / 1_000_000 */
	/* = micro_g * 9.806650 / 1_000_000 */
	int64_t micro_ms2 = ((int64_t)micro_g * GRAVITY_MS2) / 1000000LL;
	
	/* Store as val1 (integer m/s²) + val2 (fractional in micro) */
	val->val1 = micro_ms2 / 1000000LL;
	val->val2 = micro_ms2 % 1000000LL;
	
	/* Handle negative values */
	if (val->val1 < 0 && val->val2 > 0) {
		val->val2 = -val->val2;
	}
}

/* ============================================================================
 * Sensor API Implementation
 * ============================================================================ */

static int lis2ds12_accel_sample_fetch(const struct device *dev,
				       enum sensor_channel chan)
{
	const struct lis2ds12_accel_config *cfg = dev->config;
	struct lis2ds12_accel_data *data = dev->data;
	const struct device *hub = cfg->hub_dev;

	if (!device_is_ready(hub)) {
		LOG_ERR("Hub device not ready");
		return -ENODEV;
	}


#if defined(CONFIG_MAX32664_MODE_RAW)
	/* In RAW mode, FIFO-embedded accel may be available via a cached handoff
	 * from max32664_raw_read_ppg_ex(). On MAX32664C + LIS2DS12 builds this can
	 * be intentionally disabled because hub firmware may assume KX12x mapping;
	 * in that case this block is skipped and we fall back to passthrough reads.
	 *
	 * Convert FIFO mg (1 mg/LSB) to the raw 16-bit value that
	 * lis2ds12_raw_to_ms2() expects (sensitivity µg/LSB, e.g. 61 for ±2g):
	 *   raw = fifo_mg * 1000 / sensitivity_µg_per_lsb
	 * Round-trip error is at most ±0.03 mg — negligible for motion rejection.
	 */
	{
		struct max32664_data *hub_data = hub->data;
		static uint32_t fifo_accel_reject_count;

		if (hub_data->sample.fifo_accel_valid) {
			int16_t x_mg = hub_data->sample.fifo_accel_x_mg;
			int16_t y_mg = hub_data->sample.fifo_accel_y_mg;
			int16_t z_mg = hub_data->sample.fifo_accel_z_mg;

			/* Guard against occasional misaligned FIFO accel words that produce
			 * impossible magnitudes (e.g., >20g). Do not feed those into CSV. */
			if ((x_mg < -8000) || (x_mg > 8000) ||
			    (y_mg < -8000) || (y_mg > 8000) ||
			    (z_mg < -8000) || (z_mg > 8000)) {
				fifo_accel_reject_count++;
				if (fifo_accel_reject_count <= 10U ||
				    (fifo_accel_reject_count % 50U) == 0U) {
					LOG_WRN("Discarding implausible FIFO accel: X=%d Y=%d Z=%d mg (rej=%u)",
						x_mg, y_mg, z_mg, fifo_accel_reject_count);
				}
				hub_data->sample.fifo_accel_valid = false;
				/* Fall through to passthrough LIS2DS12 read below. */
			} else {
			int32_t sens = lis2ds12_get_sensitivity(cfg->fs);

			data->x_raw = (int16_t)
				((int32_t)x_mg * 1000 / sens);
			data->y_raw = (int16_t)
				((int32_t)y_mg * 1000 / sens);
			data->z_raw = (int16_t)
				((int32_t)z_mg * 1000 / sens);

			hub_data->sample.fifo_accel_valid = false;

			LOG_DBG("FIFO accel @%u ms: X=%d mg, Y=%d mg, Z=%d mg",
				k_uptime_get_32(), x_mg, y_mg, z_mg);
			return 0;
			}
		}
	}
#endif /* CONFIG_MAX32664_MODE_RAW */

	/* Fallback: passthrough read of LIS2DS12 output registers */
	uint8_t raw_data[6];
	uint8_t status;
	int ret;

	/* Check data ready status */
	ret = lis2ds12_read_reg(hub, LIS2DS12_REG_STATUS, &status);
	if (ret) {
		LOG_ERR("Failed to read status: %d", ret);
		return ret;
	}

	if (!(status & 0x01)) {
		LOG_DBG("Accel data not ready");
		return -EAGAIN;
	}

	/* Read all 6 output registers */
	for (int i = 0; i < 6; i++) {
		ret = lis2ds12_read_reg(hub, LIS2DS12_REG_OUT_X_L + i, &raw_data[i]);
		if (ret) {
			LOG_ERR("Failed to read accel reg %d: %d", i, ret);
			return ret;
		}
	}

	/* Parse raw values (16-bit little-endian, 14-bit left-justified) */
	data->x_raw = (int16_t)((raw_data[1] << 8) | raw_data[0]);
	data->y_raw = (int16_t)((raw_data[3] << 8) | raw_data[2]);
	data->z_raw = (int16_t)((raw_data[5] << 8) | raw_data[4]);

	LOG_DBG("Accel passthrough @%u ms: X=%d, Y=%d, Z=%d (raw)",
		k_uptime_get_32(), data->x_raw, data->y_raw, data->z_raw);

	return 0;
}

static int lis2ds12_accel_channel_get(const struct device *dev,
				      enum sensor_channel chan,
				      struct sensor_value *val)
{
	const struct lis2ds12_accel_config *cfg = dev->config;
	struct lis2ds12_accel_data *data = dev->data;
	int32_t sensitivity = lis2ds12_get_sensitivity(cfg->fs);

	switch (chan) {
	case SENSOR_CHAN_ACCEL_X:
		lis2ds12_raw_to_ms2(data->x_raw, sensitivity, val);
		break;

	case SENSOR_CHAN_ACCEL_Y:
		lis2ds12_raw_to_ms2(data->y_raw, sensitivity, val);
		break;

	case SENSOR_CHAN_ACCEL_Z:
		lis2ds12_raw_to_ms2(data->z_raw, sensitivity, val);
		break;

	case SENSOR_CHAN_ACCEL_XYZ:
		lis2ds12_raw_to_ms2(data->x_raw, sensitivity, &val[0]);
		lis2ds12_raw_to_ms2(data->y_raw, sensitivity, &val[1]);
		lis2ds12_raw_to_ms2(data->z_raw, sensitivity, &val[2]);
		break;

	default:
		LOG_ERR("Unsupported channel: %d", chan);
		return -ENOTSUP;
	}

	return 0;
}

static int lis2ds12_accel_attr_set(const struct device *dev,
				   enum sensor_channel chan,
				   enum sensor_attribute attr,
				   const struct sensor_value *val)
{
	const struct lis2ds12_accel_config *cfg = dev->config;
	struct lis2ds12_accel_data *data = dev->data;
	const struct device *hub = cfg->hub_dev;
	uint8_t ctrl1;
	int ret;

	/* Read current CTRL1 */
	ret = lis2ds12_read_reg(hub, LIS2DS12_REG_CTRL1, &ctrl1);
	if (ret) {
		return ret;
	}

	switch (attr) {
	case SENSOR_ATTR_SAMPLING_FREQUENCY:
		/* Set ODR based on val->val1 (Hz) */
		{
			uint8_t odr;
			int freq = val->val1;
			
			if (freq <= 1) odr = LIS2DS12_ODR_1HZ;
			else if (freq <= 12) odr = LIS2DS12_ODR_12_5HZ;
			else if (freq <= 25) odr = LIS2DS12_ODR_25HZ;
			else if (freq <= 50) odr = LIS2DS12_ODR_50HZ;
			else if (freq <= 100) odr = LIS2DS12_ODR_100HZ;
			else if (freq <= 200) odr = LIS2DS12_ODR_200HZ;
			else if (freq <= 400) odr = LIS2DS12_ODR_400HZ;
			else odr = LIS2DS12_ODR_800HZ;
			
			ctrl1 = (ctrl1 & 0x0F) | (odr << LIS2DS12_ODR_POS);
		}
		break;

	case SENSOR_ATTR_FULL_SCALE:
		/* Set full scale based on val->val1 (g) */
		{
			uint8_t fs;
			int range = val->val1;
			
			if (range <= 2) fs = LIS2DS12_FS_2G;
			else if (range <= 4) fs = LIS2DS12_FS_4G;
			else if (range <= 8) fs = LIS2DS12_FS_8G;
			else fs = LIS2DS12_FS_16G;
			
			ctrl1 = (ctrl1 & 0xF3) | (fs << LIS2DS12_FS_POS);
			data->fs = fs;
		}
		break;

	default:
		return -ENOTSUP;
	}

	/* Write updated CTRL1 */
	ret = lis2ds12_write_reg(hub, LIS2DS12_REG_CTRL1, ctrl1);
	return ret;
}

static int lis2ds12_accel_attr_get(const struct device *dev,
				   enum sensor_channel chan,
				   enum sensor_attribute attr,
				   struct sensor_value *val)
{
	const struct lis2ds12_accel_config *cfg = dev->config;
	const struct device *hub = cfg->hub_dev;
	uint8_t ctrl1;
	int ret;

	ret = max32664_read_sensor_reg(hub, HUB_SENSOR_IDX_LIS2DS12,
				       LIS2DS12_REG_CTRL1, &ctrl1);
	if (ret) {
		return ret;
	}

	switch (attr) {
	case SENSOR_ATTR_SAMPLING_FREQUENCY:
		{
			uint8_t odr = (ctrl1 >> LIS2DS12_ODR_POS) & 0x0F;
			static const int odr_hz[] = {0, 1, 12, 25, 50, 100, 200, 400, 800};
			
			val->val1 = (odr < ARRAY_SIZE(odr_hz)) ? odr_hz[odr] : 0;
			val->val2 = 0;
		}
		break;

	case SENSOR_ATTR_FULL_SCALE:
		{
			uint8_t fs = (ctrl1 >> LIS2DS12_FS_POS) & 0x03;
			static const int fs_g[] = {2, 16, 4, 8};  /* FS encoding: 00=2g, 01=16g, 10=4g, 11=8g */
			
			val->val1 = fs_g[fs];
			val->val2 = 0;
		}
		break;

	default:
		return -ENOTSUP;
	}

	return 0;
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

static int lis2ds12_accel_init(const struct device *dev)
{
	const struct lis2ds12_accel_config *cfg = dev->config;
	struct lis2ds12_accel_data *data = dev->data;
	const struct device *hub = cfg->hub_dev;
	uint8_t who_am_i;
	int ret;

	LOG_INF("Initializing LIS2DS12 accelerometer via hub passthrough...");

	/* Wait for hub device to be ready */
	if (!device_is_ready(hub)) {
		LOG_ERR("Hub device %s not ready", hub->name);
		return -ENODEV;
	}

	/* Verify LIS2DS12 is present by reading WHO_AM_I */
	ret = lis2ds12_read_reg(hub, LIS2DS12_REG_WHO_AM_I, &who_am_i);
	if (ret) {
		LOG_ERR("Failed to read LIS2DS12 WHO_AM_I: %d", ret);
		return ret;
	}

	if (who_am_i != 0x43 && who_am_i != 0x1D) {
		LOG_ERR("Invalid LIS2DS12 WHO_AM_I: 0x%02X (expected 0x43)", who_am_i);
		return -ENODEV;
	}

	LOG_INF("LIS2DS12 detected: WHO_AM_I 0x%02X", who_am_i);

	/* Store full scale setting */
	data->fs = cfg->fs;

	/* Configure CTRL1: ODR, Full Scale, BDU */
	uint8_t ctrl1 = (cfg->odr << LIS2DS12_ODR_POS) | 
			(cfg->fs << LIS2DS12_FS_POS) |
			(1 << LIS2DS12_BDU_POS);  /* Enable Block Data Update */

	ret = max32664_write_sensor_reg(hub, HUB_SENSOR_IDX_LIS2DS12,
					LIS2DS12_REG_CTRL1, ctrl1);
	if (ret) {
		LOG_ERR("Failed to configure CTRL1: %d", ret);
		return ret;
	}

	/* Configure CTRL2: Defaults (no auto-increment, no reboot) */
	ret = lis2ds12_write_reg(hub, LIS2DS12_REG_CTRL2, 0x00);
	if (ret) {
		LOG_ERR("Failed to configure CTRL2: %d", ret);
		return ret;
	}

	data->configured = true;

	LOG_INF("LIS2DS12 initialized: ODR=%u, FS=%ug",
		cfg->odr, 2 << (cfg->fs == 0x01 ? 3 : cfg->fs));

	return 0;
}

/* ============================================================================
 * Driver API
 * ============================================================================ */

static const struct sensor_driver_api lis2ds12_accel_driver_api = {
	.sample_fetch = lis2ds12_accel_sample_fetch,
	.channel_get = lis2ds12_accel_channel_get,
	.attr_set = lis2ds12_accel_attr_set,
	.attr_get = lis2ds12_accel_attr_get,
};

/* ============================================================================
 * Device Instantiation
 * ============================================================================ */

#if DT_HAS_COMPAT_STATUS_OKAY(st_lis2ds12_accel)

#define LIS2DS12_ACCEL_DEFINE(inst)                                             \
	static struct lis2ds12_accel_data lis2ds12_accel_data_##inst;            \
                                                                                \
	static const struct lis2ds12_accel_config lis2ds12_accel_config_##inst = { \
		.hub_dev = DEVICE_DT_GET(DT_INST_PARENT(inst)),                  \
		.odr = DT_INST_PROP_OR(inst, odr, LIS2DS12_ODR_25HZ),            \
		.fs = DT_INST_PROP_OR(inst, fs, LIS2DS12_FS_2G),                 \
	};                                                                       \
                                                                                \
	DEVICE_DT_INST_DEFINE(inst, lis2ds12_accel_init, NULL,                   \
			      &lis2ds12_accel_data_##inst,                        \
			      &lis2ds12_accel_config_##inst,                      \
			      POST_KERNEL,                                       \
			      CONFIG_MAX32664_CHILD_SENSOR_INIT_PRIORITY,          \
			      &lis2ds12_accel_driver_api);

DT_INST_FOREACH_STATUS_OKAY(LIS2DS12_ACCEL_DEFINE)

#else /* No device tree node - use hub device directly */

/* When no separate DT node exists, the accel sensor is accessed via hub device */
/* Application should use max32664_raw_read_accel() directly */

#endif /* DT_HAS_COMPAT_STATUS_OKAY(st_lis2ds12_accel) */
