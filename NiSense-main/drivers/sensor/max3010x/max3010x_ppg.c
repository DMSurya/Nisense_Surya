/*
 * Copyright (c) 2025 NiSense
 * SPDX-License-Identifier: Apache-2.0
 *
 * MAX3010x Pulse Oximeter / Heart-Rate Sensor - Unified Driver
 *
 * Supports MAX30101, MAX30102, and MAX30105 variants via DT compatible strings.
 * The variant is determined at compile time from the device tree compatible:
 *   - "maxim,max30101-ppg" → 3 LEDs (Red + IR + Green)
 *   - "maxim,max30102-ppg" → 2 LEDs (Red + IR only)
 *   - "maxim,max30105-ppg" → 3 LEDs (Red + IR + Green, particle sensor)
 *
 * Full-featured out-of-tree driver with:
 *   - Proper FIFO drain (reads ALL available samples per fetch)
 *   - Runtime LED current, shutdown/wakeup via sensor_attr_set()
 *   - Returns -EAGAIN when FIFO is empty (no re-reading stale data)
 *   - Starts in shutdown after init (LEDs off until app wakes sensor)
 *   - Auto-clamps Green LED settings for MAX30102
 */

#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include "max3010x_ppg.h"

LOG_MODULE_REGISTER(max3010x_ppg, CONFIG_SENSOR_LOG_LEVEL);

/* ========================================================================== */
/*  I2C Helpers                                                               */
/* ========================================================================== */

static inline int reg_read(const struct i2c_dt_spec *i2c,
			   uint8_t reg, uint8_t *val)
{
	return i2c_reg_read_byte_dt(i2c, reg, val);
}

static inline int reg_write(const struct i2c_dt_spec *i2c,
			    uint8_t reg, uint8_t val)
{
	return i2c_reg_write_byte_dt(i2c, reg, val);
}

/* ========================================================================== */
/*  FIFO Management                                                           */
/* ========================================================================== */

/**
 * @brief Read FIFO write/overflow/read pointers in a single I2C burst.
 *
 * Registers 0x04–0x06 are adjacent, so one burst read gets all three.
 *
 * @param[out] available  Number of unread samples in FIFO (0–32)
 * @return 0 on success, negative errno on I2C failure
 */
static int fifo_available(const struct i2c_dt_spec *i2c, uint8_t *available)
{
	uint8_t ptrs[3]; /* [0]=WR_PTR, [1]=OVF_CTR, [2]=RD_PTR */
	int ret;

	ret = i2c_burst_read_dt(i2c, MAX3010X_REG_FIFO_WR_PTR, ptrs, 3);
	if (ret) {
		return ret;
	}

	uint8_t wr  = ptrs[0] & MAX3010X_FIFO_PTR_MASK;
	uint8_t ovf = ptrs[1] & MAX3010X_FIFO_PTR_MASK;
	uint8_t rd  = ptrs[2] & MAX3010X_FIFO_PTR_MASK;

	int16_t count = (int16_t)wr - (int16_t)rd;

	if (count < 0) {
		/* Write pointer wrapped around */
		count += MAX3010X_FIFO_DEPTH;
	} else if (count == 0 && ovf > 0) {
		/* Pointers equal but overflow occurred → FIFO is full (32 samples) */
		count = MAX3010X_FIFO_DEPTH;
	}

	*available = (uint8_t)count;
	return 0;
}

/**
 * @brief Drain all available samples from device FIFO into internal buffer.
 *
 * Uses a single burst read for efficiency (one I2C START, all bytes, one STOP).
 * Parses the raw byte stream into 18-bit sample values indexed by
 * [sample_number][channel_number].
 *
 * After this call:
 *   data->fifo_count    = number of samples available
 *   data->fifo_read_idx = 0 (ready to serve)
 *   data->fifo_buf[][] = parsed samples
 *
 * @return 0 if data available, -EAGAIN if FIFO empty, negative errno on error
 */
static int fifo_drain(const struct device *dev)
{
	struct max3010x_ppg_data *data = dev->data;
	const struct max3010x_ppg_config *config = dev->config;
	uint8_t avail = 0;
	int ret;

	ret = fifo_available(&config->i2c, &avail);
	if (ret) {
		LOG_ERR("Cannot read FIFO pointers: %d", ret);
		return ret;
	}

	if (avail == 0) {
		data->fifo_count = 0;
		data->fifo_read_idx = 0;
		return -EAGAIN;
	}

	/* Clamp to buffer capacity */
	if (avail > MAX3010X_FIFO_DEPTH) {
		avail = MAX3010X_FIFO_DEPTH;
	}

	uint16_t bytes_per_sample = data->num_channels * MAX3010X_BYTES_PER_CHANNEL;
	uint16_t total_bytes = avail * bytes_per_sample;

	/* Burst-read all FIFO data in one I2C transaction.
	 * MAX3010x auto-increments FIFO_RD_PTR after each sample's bytes. */
	uint8_t raw_buf[MAX3010X_MAX_FIFO_BYTES];

	ret = i2c_burst_read_dt(&config->i2c, MAX3010X_REG_FIFO_DATA,
				raw_buf, total_bytes);
	if (ret) {
		LOG_ERR("FIFO burst read failed (%u bytes): %d", total_bytes, ret);
		return ret;
	}

	/* Parse 3-byte big-endian values into 32-bit samples */
	uint16_t idx = 0;

	for (uint8_t s = 0; s < avail; s++) {
		for (uint8_t c = 0; c < data->num_channels; c++) {
			uint32_t val = ((uint32_t)raw_buf[idx] << 16) |
				       ((uint32_t)raw_buf[idx + 1] << 8) |
				       ((uint32_t)raw_buf[idx + 2]);
			data->fifo_buf[s][c] = val & MAX3010X_ADC_MASK;
			idx += MAX3010X_BYTES_PER_CHANNEL;
		}
	}

	data->fifo_count = avail;
	data->fifo_read_idx = 0;

	return 0;
}

/**
 * @brief Flush device FIFO (reset all pointers to zero).
 */
static int fifo_flush(const struct i2c_dt_spec *i2c)
{
	int ret;

	ret = reg_write(i2c, MAX3010X_REG_FIFO_WR_PTR, 0);
	if (ret) {
		return ret;
	}
	ret = reg_write(i2c, MAX3010X_REG_OVF_CTR, 0);
	if (ret) {
		return ret;
	}
	return reg_write(i2c, MAX3010X_REG_FIFO_RD_PTR, 0);
}

/* ========================================================================== */
/*  Sensor API: sample_fetch                                                  */
/* ========================================================================== */

/**
 * @brief Fetch the next available sample from the MAX3010x FIFO.
 *
 * Strategy:
 *   1. If the internal buffer has unserved samples, pop the next one.
 *   2. Otherwise, drain the entire device FIFO into the internal buffer.
 *   3. Return -EAGAIN if no new data is available (FIFO was empty).
 *
 * This means callers should loop until -EAGAIN to process all queued data:
 * @code
 *   while (sensor_sample_fetch(dev) == 0) {
 *       sensor_channel_get(dev, SENSOR_CHAN_IR, &ir);
 *       sensor_channel_get(dev, SENSOR_CHAN_RED, &red);
 *       process(ir, red);
 *   }
 * @endcode
 */
static int max3010x_sample_fetch(const struct device *dev,
				 enum sensor_channel chan)
{
	struct max3010x_ppg_data *data = dev->data;

	if (data->in_shutdown) {
		return -ECANCELED;
	}

	/* Serve next sample from internal buffer if available */
	if (data->fifo_read_idx < data->fifo_count) {
		for (uint8_t c = 0; c < data->num_channels; c++) {
			data->raw[c] = data->fifo_buf[data->fifo_read_idx][c];
		}
		data->fifo_read_idx++;
		return 0;
	}

	/* Internal buffer empty — drain device FIFO */
	int ret = fifo_drain(dev);

	if (ret) {
		return ret;  /* -EAGAIN if empty, other negative = I2C error */
	}

	/* Serve the first sample from the freshly drained buffer */
	for (uint8_t c = 0; c < data->num_channels; c++) {
		data->raw[c] = data->fifo_buf[0][c];
	}
	data->fifo_read_idx = 1;

	return 0;
}

/* ========================================================================== */
/*  Sensor API: channel_get                                                   */
/* ========================================================================== */

static int max3010x_channel_get(const struct device *dev,
				enum sensor_channel chan,
				struct sensor_value *val)
{
	struct max3010x_ppg_data *data = dev->data;
	const struct max3010x_ppg_config *config = dev->config;
	uint8_t led_ch;

	switch (chan) {
	case SENSOR_CHAN_RED:
		led_ch = 0;
		break;
	case SENSOR_CHAN_IR:
		led_ch = 1;
		break;
	case SENSOR_CHAN_GREEN:
		/* Green channel not available on MAX30102 */
		if (config->num_leds < 3) {
			return -ENOTSUP;
		}
		led_ch = 2;
		break;
	default:
		return -ENOTSUP;
	}

	uint8_t slot = data->map[led_ch];

	if (slot >= data->num_channels) {
		return -ENOTSUP;  /* Channel not active in current mode */
	}

	val->val1 = (int32_t)data->raw[slot];
	val->val2 = 0;

	return 0;
}

/* ========================================================================== */
/*  Sensor API: attr_set                                                      */
/* ========================================================================== */

static int max3010x_attr_set(const struct device *dev,
			     enum sensor_channel chan,
			     enum sensor_attribute attr,
			     const struct sensor_value *val)
{
	struct max3010x_ppg_data *data = dev->data;
	const struct max3010x_ppg_config *config = dev->config;
	int ret;

	switch ((int)attr) {
	case SENSOR_ATTR_MAX3010X_SHUTDOWN:
		if (val->val1) {
			/* Enter shutdown: set SHDN bit in MODE_CFG */
			uint8_t mode;

			ret = reg_read(&config->i2c, MAX3010X_REG_MODE_CFG, &mode);
			if (ret) {
				return ret;
			}
			ret = reg_write(&config->i2c, MAX3010X_REG_MODE_CFG,
					mode | MAX3010X_MODE_SHDN);
			if (ret) {
				return ret;
			}
			data->in_shutdown = true;
			LOG_INF("MAX3010x shutdown (LEDs off)");
		} else {
			/* Wakeup: clear SHDN bit, flush stale FIFO data */
			uint8_t mode;

			ret = reg_read(&config->i2c, MAX3010X_REG_MODE_CFG, &mode);
			if (ret) {
				return ret;
			}
			ret = reg_write(&config->i2c, MAX3010X_REG_MODE_CFG,
					mode & ~MAX3010X_MODE_SHDN);
			if (ret) {
				return ret;
			}

			/* Re-write LED PA and slot config after wakeup.
			 * Ensures registers are correct even if the part's
			 * internal state was affected by shutdown/wakeup. */
			(void)reg_write(&config->i2c, MAX3010X_REG_LED1_PA, config->led1_pa);
			(void)reg_write(&config->i2c, MAX3010X_REG_LED2_PA, config->led2_pa);
			if (config->num_leds >= 3) {
				(void)reg_write(&config->i2c, MAX3010X_REG_LED3_PA,
						config->led3_pa);
			}
			if (config->mode == MAX3010X_MODE_MULTI) {
				uint8_t ml1 = (config->slot[1] << 4) | config->slot[0];
				uint8_t ml2 = (config->slot[3] << 4) | config->slot[2];
				(void)reg_write(&config->i2c, MAX3010X_REG_MULTI_LED1, ml1);
				(void)reg_write(&config->i2c, MAX3010X_REG_MULTI_LED2, ml2);
			}

			/* Flush FIFO so first read gets fresh data */
			(void)fifo_flush(&config->i2c);
			data->fifo_count = 0;
			data->fifo_read_idx = 0;
			data->in_shutdown = false;
			LOG_INF("MAX3010x wakeup (LEDs on, FIFO flushed, regs refreshed)");
		}
		return 0;

	case SENSOR_ATTR_MAX3010X_LED1_PA:
		ret = reg_write(&config->i2c, MAX3010X_REG_LED1_PA,
				(uint8_t)val->val1);
		if (ret == 0) {
			LOG_DBG("LED1 PA set to 0x%02x", (uint8_t)val->val1);
		}
		return ret;

	case SENSOR_ATTR_MAX3010X_LED2_PA:
		ret = reg_write(&config->i2c, MAX3010X_REG_LED2_PA,
				(uint8_t)val->val1);
		if (ret == 0) {
			LOG_DBG("LED2 PA set to 0x%02x", (uint8_t)val->val1);
		}
		return ret;

	case SENSOR_ATTR_MAX3010X_LED3_PA:
		/* Green LED not available on MAX30102 */
		if (config->num_leds < 3) {
			LOG_WRN("LED3 (Green) not available on MAX30102");
			return -ENOTSUP;
		}
		ret = reg_write(&config->i2c, MAX3010X_REG_LED3_PA,
				(uint8_t)val->val1);
		if (ret == 0) {
			LOG_DBG("LED3 PA set to 0x%02x", (uint8_t)val->val1);
		}
		return ret;

	case SENSOR_ATTR_MAX3010X_FIFO_FLUSH:
		data->fifo_count = 0;
		data->fifo_read_idx = 0;
		return fifo_flush(&config->i2c);

	default:
		return -ENOTSUP;
	}
}

/* ========================================================================== */
/*  Initialization                                                            */
/* ========================================================================== */

static int max3010x_init(const struct device *dev)
{
	const struct max3010x_ppg_config *config = dev->config;
	struct max3010x_ppg_data *data = dev->data;
	uint8_t part_id;
	uint8_t reg_val;
	int ret;

	if (!device_is_ready(config->i2c.bus)) {
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	/* Log variant info */
	const char *variant_name;
	switch (config->variant) {
	case MAX3010X_VARIANT_30101:
		variant_name = "MAX30101";
		break;
	case MAX3010X_VARIANT_30102:
		variant_name = "MAX30102";
		break;
	case MAX3010X_VARIANT_30105:
		variant_name = "MAX30105";
		break;
	default:
		variant_name = "MAX3010x";
		break;
	}
	LOG_INF("Initializing %s (%u LEDs)", variant_name, config->num_leds);

	/* ---- Verify Part ID ---- */
	ret = reg_read(&config->i2c, MAX3010X_REG_PART_ID, &part_id);
	if (ret) {
		/*
		 * Single-firmware bring-up:
		 * On wearable builds the MAX3010x is physically absent, so probing
		 * will fail with -EIO/-ENXIO at boot. Treat this as "device not
		 * present" (non-fatal) so higher layers can auto-select another PPG
		 * source (e.g. MAX32664+MAX86141) based on device_is_ready().
		 */
		LOG_WRN("MAX3010x not present or not responding (Part ID read failed: %d)", ret);
		return -ENODEV;
	}
	if (part_id != MAX3010X_PART_ID) {
		LOG_WRN("MAX3010x not present (Part ID 0x%02x, expected 0x%02x)",
			part_id, MAX3010X_PART_ID);
		return -ENODEV;
	}

	/* Read Revision ID for chip identification logging */
	uint8_t rev_id;
	ret = reg_read(&config->i2c, MAX3010X_REG_REV_ID, &rev_id);
	if (ret) {
		LOG_WRN("Cannot read Rev ID: %d", ret);
		rev_id = 0xFF;
	}
	LOG_INF("Chip identification: Part ID=0x%02x, Rev ID=0x%02x", part_id, rev_id);

	/* ---- Software Reset ---- */
	ret = reg_write(&config->i2c, MAX3010X_REG_MODE_CFG, MAX3010X_MODE_RESET);
	if (ret) {
		LOG_ERR("Reset failed: %d", ret);
		return -EIO;
	}

	/* Wait for reset bit to self-clear (typically <1 ms) */
	do {
		k_sleep(K_MSEC(1));
		ret = reg_read(&config->i2c, MAX3010X_REG_MODE_CFG, &reg_val);
		if (ret) {
			LOG_ERR("Post-reset read failed: %d", ret);
			return -EIO;
		}
	} while (reg_val & MAX3010X_MODE_RESET);

	/* ---- FIFO Configuration ---- */
	ret = reg_write(&config->i2c, MAX3010X_REG_FIFO_CFG, config->fifo_cfg);
	if (ret) {
		LOG_ERR("FIFO cfg failed: %d", ret);
		return -EIO;
	}

	/* ---- Set Operating Mode FIRST ----
	 * After reset, MODE[2:0]=000 (undefined). Some MAX3010x revisions
	 * may not commit LED PA and multi-LED slot registers until a valid
	 * mode is active. Set the target mode before programming them. */
	ret = reg_write(&config->i2c, MAX3010X_REG_MODE_CFG, config->mode);
	if (ret) {
		LOG_ERR("Mode set failed: %d", ret);
		return -EIO;
	}

	/* ---- SpO2 / ADC Configuration ---- */
	ret = reg_write(&config->i2c, MAX3010X_REG_SPO2_CFG, config->spo2_cfg);
	if (ret) {
		LOG_ERR("SpO2 cfg failed: %d", ret);
		return -EIO;
	}

	/* ---- LED Pulse Amplitudes ---- */
	ret = reg_write(&config->i2c, MAX3010X_REG_LED1_PA, config->led1_pa);
	if (ret) {
		return -EIO;
	}
	ret = reg_write(&config->i2c, MAX3010X_REG_LED2_PA, config->led2_pa);
	if (ret) {
		return -EIO;
	}
	/* Only write LED3 for 3-LED variants */
	if (config->num_leds >= 3) {
		ret = reg_write(&config->i2c, MAX3010X_REG_LED3_PA, config->led3_pa);
		if (ret) {
			return -EIO;
		}
	}

	/* ---- Multi-LED Slot Control (only for multi-LED mode) ---- */
	if (config->mode == MAX3010X_MODE_MULTI) {
		uint8_t ml1 = (config->slot[1] << 4) | config->slot[0];
		uint8_t ml2 = (config->slot[3] << 4) | config->slot[2];

		ret = reg_write(&config->i2c, MAX3010X_REG_MULTI_LED1, ml1);
		if (ret) {
			return -EIO;
		}
		ret = reg_write(&config->i2c, MAX3010X_REG_MULTI_LED2, ml2);
		if (ret) {
			return -EIO;
		}

		/* Readback verify critical registers */
		uint8_t rb_led3, rb_ml1, rb_ml2, rb_mode;
		reg_read(&config->i2c, MAX3010X_REG_LED3_PA, &rb_led3);
		reg_read(&config->i2c, MAX3010X_REG_MULTI_LED1, &rb_ml1);
		reg_read(&config->i2c, MAX3010X_REG_MULTI_LED2, &rb_ml2);
		reg_read(&config->i2c, MAX3010X_REG_MODE_CFG, &rb_mode);
		LOG_INF("Register verify: LED3_PA=0x%02x, ML1=0x%02x, ML2=0x%02x, MODE=0x%02x",
			rb_led3, rb_ml1, rb_ml2, rb_mode);
	}

	/* ---- Build Channel Map ---- */
	data->num_channels = 0;
	for (uint8_t i = 0; i < MAX3010X_MAX_CHANNELS; i++) {
		data->map[i] = MAX3010X_MAX_CHANNELS; /* Mark as inactive */
	}

	switch (config->mode) {
	case MAX3010X_MODE_HR:
		/* Heart Rate: only Red LED (slot 0) */
		data->map[0] = 0;  /* Red → FIFO slot 0 */
		data->num_channels = 1;
		break;

	case MAX3010X_MODE_SPO2:
		/* SpO2: Red (slot 0) + IR (slot 1) */
		data->map[0] = 0;  /* Red → FIFO slot 0 */
		data->map[1] = 1;  /* IR  → FIFO slot 1 */
		data->num_channels = 2;
		break;

	case MAX3010X_MODE_MULTI:
		/* Multi-LED: map from slot config, respecting LED count */
		for (uint8_t s = 0; s < 4 && data->num_channels < MAX3010X_MAX_CHANNELS; s++) {
			uint8_t slot_val = config->slot[s] & 0x07;

			if (slot_val >= MAX3010X_SLOT_RED &&
			    slot_val <= MAX3010X_SLOT_GREEN) {
				/* Skip Green slot if only 2 LEDs (MAX30102) */
				if (slot_val == MAX3010X_SLOT_GREEN && config->num_leds < 3) {
					LOG_WRN("Slot %u configured for Green LED but "
						"MAX30102 has no Green LED - skipping", s + 1);
					continue;
				}
				data->map[slot_val - 1] = data->num_channels;
				data->num_channels++;
			}
		}
		LOG_INF("Multi-LED channel map: Red→%u, IR→%u, Green→%u (num_ch=%u)",
			data->map[0], data->map[1], data->map[2], data->num_channels);
		break;

	default:
		LOG_ERR("Unknown mode: 0x%02x", config->mode);
		return -EINVAL;
	}

	/* ---- Flush FIFO (discard any data acquired during config) ---- */
	(void)fifo_flush(&config->i2c);
	data->fifo_count = 0;
	data->fifo_read_idx = 0;

	/* ---- Enter Shutdown Immediately ----
	 * LEDs are off until the application explicitly wakes the sensor
	 * via sensor_attr_set(SENSOR_ATTR_MAX3010X_SHUTDOWN, {.val1 = 0}). */
	ret = reg_write(&config->i2c, MAX3010X_REG_MODE_CFG,
			config->mode | MAX3010X_MODE_SHDN);
	if (ret) {
		LOG_WRN("Post-init shutdown failed: %d (LEDs may stay on)", ret);
	}
	data->in_shutdown = true;

	LOG_INF("%s ready (Part ID 0x%02x, mode=%u, ch=%u, "
		"LED1/2/3=0x%02x/0x%02x/0x%02x, ADC_RGE=%u, SR=%u, PW=%u) — shutdown",
		variant_name, part_id, config->mode, data->num_channels,
		config->led1_pa, config->led2_pa,
		(config->num_leds >= 3) ? config->led3_pa : 0,
		(config->spo2_cfg >> MAX3010X_SPO2_ADC_RGE_SHIFT) & 0x03,
		(config->spo2_cfg >> MAX3010X_SPO2_SR_SHIFT) & 0x07,
		(config->spo2_cfg >> MAX3010X_SPO2_PW_SHIFT) & 0x03);

	return 0;
}

/* ========================================================================== */
/*  Driver API                                                                */
/* ========================================================================== */

static DEVICE_API(sensor, max3010x_ppg_api) = {
	.sample_fetch = max3010x_sample_fetch,
	.channel_get  = max3010x_channel_get,
	.attr_set     = max3010x_attr_set,
};

/* ========================================================================== */
/*  Device Instantiation Macros                                               */
/* ========================================================================== */

/* Map DT enum index for "mode" property to MAX3010X register value.
 *   0 = "hr"    → 0x02
 *   1 = "spo2"  → 0x03
 *   2 = "multi" → 0x07 */
#define MAX3010X_MODE_FROM_DT(inst) \
	(DT_INST_ENUM_IDX(inst, mode) == 0 ? MAX3010X_MODE_HR : \
	 DT_INST_ENUM_IDX(inst, mode) == 1 ? MAX3010X_MODE_SPO2 : \
	 MAX3010X_MODE_MULTI)

/* Build SPO2_CFG register value from DT properties */
#define MAX3010X_SPO2_CFG_FROM_DT(inst) \
	((DT_INST_PROP(inst, adc_range) << MAX3010X_SPO2_ADC_RGE_SHIFT) | \
	 (DT_INST_PROP(inst, sample_rate) << MAX3010X_SPO2_SR_SHIFT) | \
	 (DT_INST_PROP(inst, pulse_width) << MAX3010X_SPO2_PW_SHIFT))

/* Build FIFO_CFG register value from DT properties */
#define MAX3010X_FIFO_CFG_FROM_DT(inst) \
	((DT_INST_PROP(inst, smp_ave) << MAX3010X_FIFO_SMP_AVE_SHIFT) | \
	 (DT_INST_PROP(inst, fifo_rollover) ? MAX3010X_FIFO_ROLLOVER_EN : 0) | \
	 (DT_INST_PROP(inst, fifo_a_full) & MAX3010X_FIFO_A_FULL_MASK))

/*
 * Multi-compatible instantiation.
 *
 * We use separate DT_DRV_COMPAT for each variant because all variants share
 * Part ID 0x15 and cannot be reliably distinguished at runtime. The variant
 * is determined by which compatible string is used in the device tree.
 */

/* ---- MAX30101 (3 LEDs) ---- */
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT maxim_max30101_ppg

#define MAX30101_PPG_INIT(inst)                                                \
	static struct max3010x_ppg_data max30101_ppg_data_##inst;              \
                                                                               \
	static const struct max3010x_ppg_config max30101_ppg_config_##inst = { \
		.i2c      = I2C_DT_SPEC_INST_GET(inst),                        \
		.variant  = MAX3010X_VARIANT_30101,                            \
		.num_leds = MAX3010X_LEDS_30101,                               \
		.mode     = MAX3010X_MODE_FROM_DT(inst),                       \
		.spo2_cfg = MAX3010X_SPO2_CFG_FROM_DT(inst),                   \
		.fifo_cfg = MAX3010X_FIFO_CFG_FROM_DT(inst),                   \
		.led1_pa  = DT_INST_PROP(inst, led1_pa),                       \
		.led2_pa  = DT_INST_PROP(inst, led2_pa),                       \
		.led3_pa  = DT_INST_PROP(inst, led3_pa),                       \
		.slot     = { DT_INST_PROP(inst, slot1),                       \
			      DT_INST_PROP(inst, slot2),                       \
			      DT_INST_PROP(inst, slot3),                       \
			      DT_INST_PROP(inst, slot4) },                     \
	};                                                                     \
                                                                               \
	SENSOR_DEVICE_DT_INST_DEFINE(inst,                                     \
				     max3010x_init, NULL,                      \
				     &max30101_ppg_data_##inst,                \
				     &max30101_ppg_config_##inst,              \
				     POST_KERNEL,                              \
				     CONFIG_SENSOR_INIT_PRIORITY,              \
				     &max3010x_ppg_api);

DT_INST_FOREACH_STATUS_OKAY(MAX30101_PPG_INIT)

/* ---- MAX30102 (2 LEDs) ---- */
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT maxim_max30102_ppg

#define MAX30102_PPG_INIT(inst)                                                \
	static struct max3010x_ppg_data max30102_ppg_data_##inst;              \
                                                                               \
	static const struct max3010x_ppg_config max30102_ppg_config_##inst = { \
		.i2c      = I2C_DT_SPEC_INST_GET(inst),                        \
		.variant  = MAX3010X_VARIANT_30102,                            \
		.num_leds = MAX3010X_LEDS_30102,                               \
		.mode     = MAX3010X_MODE_FROM_DT(inst),                       \
		.spo2_cfg = MAX3010X_SPO2_CFG_FROM_DT(inst),                   \
		.fifo_cfg = MAX3010X_FIFO_CFG_FROM_DT(inst),                   \
		.led1_pa  = DT_INST_PROP(inst, led1_pa),                       \
		.led2_pa  = DT_INST_PROP(inst, led2_pa),                       \
		.led3_pa  = 0,  /* No Green LED on MAX30102 */                 \
		.slot     = { DT_INST_PROP(inst, slot1),                       \
			      DT_INST_PROP(inst, slot2),                       \
			      0,  /* Clamp slot3 to disabled */                \
			      0 }, /* Clamp slot4 to disabled */               \
	};                                                                     \
                                                                               \
	SENSOR_DEVICE_DT_INST_DEFINE(inst,                                     \
				     max3010x_init, NULL,                      \
				     &max30102_ppg_data_##inst,                \
				     &max30102_ppg_config_##inst,              \
				     POST_KERNEL,                              \
				     CONFIG_SENSOR_INIT_PRIORITY,              \
				     &max3010x_ppg_api);

DT_INST_FOREACH_STATUS_OKAY(MAX30102_PPG_INIT)

/* ---- MAX30105 (3 LEDs, particle sensor variant) ---- */
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT maxim_max30105_ppg

#define MAX30105_PPG_INIT(inst)                                                \
	static struct max3010x_ppg_data max30105_ppg_data_##inst;              \
                                                                               \
	static const struct max3010x_ppg_config max30105_ppg_config_##inst = { \
		.i2c      = I2C_DT_SPEC_INST_GET(inst),                        \
		.variant  = MAX3010X_VARIANT_30105,                            \
		.num_leds = MAX3010X_LEDS_30105,                               \
		.mode     = MAX3010X_MODE_FROM_DT(inst),                       \
		.spo2_cfg = MAX3010X_SPO2_CFG_FROM_DT(inst),                   \
		.fifo_cfg = MAX3010X_FIFO_CFG_FROM_DT(inst),                   \
		.led1_pa  = DT_INST_PROP(inst, led1_pa),                       \
		.led2_pa  = DT_INST_PROP(inst, led2_pa),                       \
		.led3_pa  = DT_INST_PROP(inst, led3_pa),                       \
		.slot     = { DT_INST_PROP(inst, slot1),                       \
			      DT_INST_PROP(inst, slot2),                       \
			      DT_INST_PROP(inst, slot3),                       \
			      DT_INST_PROP(inst, slot4) },                     \
	};                                                                     \
                                                                               \
	SENSOR_DEVICE_DT_INST_DEFINE(inst,                                     \
				     max3010x_init, NULL,                      \
				     &max30105_ppg_data_##inst,                \
				     &max30105_ppg_config_##inst,              \
				     POST_KERNEL,                              \
				     CONFIG_SENSOR_INIT_PRIORITY,              \
				     &max3010x_ppg_api);

DT_INST_FOREACH_STATUS_OKAY(MAX30105_PPG_INIT)
