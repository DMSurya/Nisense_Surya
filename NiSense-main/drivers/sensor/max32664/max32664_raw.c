/**
 * @file max32664_raw.c
 * @brief MAX32664 RAW Mode - Hub reads sensors, outputs raw ADC via output FIFO
 *
 * RAW mode: Hub firmware reads MAX86141 FIFO internally (proper multi-byte
 * I2C/SPI) and outputs raw sensor data through the hub's output FIFO
 * (Family 0x12). Sensor register configuration uses hub passthrough commands
 * (Family 0x40/0x41) with transparent wrapper functions from max86141_regs.h
 * and lis2ds12_regs.h.
 *
 * The nRF52840 runs the ppg_algo subsystem to compute HR/SpO2 from the raw
 * PPG ADC values.
 *
 * For PASSTHROUGH mode (hub as bridge, MAX86141 FIFO via burst read for tests),
 * see max32664_passthrough.c — do not use that AFE path from this file.
 *
 * Architecture (RAW Mode):
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    MAX32664 Sensor Hub                          │
 * │   ┌───────────┐              ┌───────────┐                     │
 * │   │ MAX86141  │ ◄──SPI────── │ LIS2DS12  │                     │
 * │   │  PPG AFE  │              │  Accel    │                     │
 * │   └─────┬─────┘              └─────┬─────┘                     │
 * │         │                          │                            │
 * │         └──────────┬───────────────┘                            │
 * │                    │                                            │
 * │         ┌──────────▼──────────┐                                 │
 * │         │  Hub Firmware       │ ◄── OUTPUT_MODE_SENSOR_DATA    │
 * │         │  Reads FIFO data    │     (0x01): raw ADC output     │
 * │         │  (multi-byte I2C)   │                                 │
 * │         └──────────┬──────────┘                                 │
 * │                    │ Output FIFO (Family 0x12)                  │
 * └────────────────────┼────────────────────────────────────────────┘
 *                      │ I2C (0x55)
 *                      ▼
 *               ┌──────────────┐
 *               │   nRF52840   │
 *               │              │
 *               │ ┌──────────┐ │
 *               │ │ PPG Algo │ │ ◄── On-chip HR/SpO2 processing
 *               │ └──────────┘ │
 *               └──────────────┘
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT maxim_max32664

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "max32664.h"
#include "max32664_priv.h"
#include "max86141_regs.h"
#include "max86141_green.h"
#include "lis2ds12_regs.h"

#if defined(CONFIG_CALIBRATION)
#include "calibration.h"
#endif

LOG_MODULE_DECLARE(max32664, CONFIG_SENSOR_LOG_LEVEL);

/* LIS2DS12 @ 25 Hz, ±2 g, BDU=1 — matches max32664c host-feed path (GWEC 30.2.x). */
static int raw_lis2ds12_prepare_host_feed(const struct device *dev)
{
	uint8_t whoami;
	int ret;

	ret = lis2ds12_read_reg(dev, LIS2DS12_REG_WHO_AM_I, &whoami);
	if (ret) {
		LOG_ERR("External accel: LIS2DS12 WHOAMI read failed: %d", ret);
		return ret;
	}
	if (whoami != LIS2DS12_WHOAMI_VALUE && whoami != LIS2DS12_WHOAMI_ALT) {
		LOG_WRN("External accel: unexpected WHOAMI 0x%02x (expected 0x43/0x1D)",
			whoami);
	} else {
		LOG_INF("External accel: LIS2DS12 WHOAMI=0x%02x via passthrough", whoami);
	}

	uint8_t ctrl1 = (LIS2DS12_ODR_25HZ << LIS2DS12_ODR_POS) |
		       (LIS2DS12_FS_2G << LIS2DS12_FS_POS) |
		       (1U << LIS2DS12_BDU_POS);

	ret = lis2ds12_write_reg(dev, LIS2DS12_REG_CTRL1, ctrl1);
	if (ret) {
		LOG_ERR("External accel: CTRL1 write failed: %d", ret);
		return ret;
	}
	ret = lis2ds12_write_reg(dev, LIS2DS12_REG_CTRL2, 0x00);
	if (ret) {
		LOG_WRN("External accel: CTRL2 write failed: %d (continuing)", ret);
	}

	LOG_INF("External accel: LIS2DS12 prepared @ 25 Hz, ±2 g, BDU=1");
	return 0;
}

/* ============================================================================
 * RAW Mode State
 * ============================================================================ */

/** RAW mode specific data stored in max32664_data */
struct max32664_raw_state {
	bool ppg_enabled;
	bool accel_enabled;
	uint8_t ppg_sample_rate;    /* Hz */
	uint8_t ppg_led_current[3]; /* IR, Red, Green (mA) */
	int16_t accel_x, accel_y, accel_z; /* Last accel readings (milli-g) */
	uint32_t ppg_ir, ppg_red, ppg_green; /* Last PPG readings */
};

void max32664_raw_reset_session_state(const struct device *dev)
{
	struct max32664_data *data = dev->data;

	if (!data) {
		return;
	}

	memset(&data->raw_runtime, 0, sizeof(data->raw_runtime));
	data->raw_runtime.settle_discard_left =
		(uint16_t)CONFIG_MAX32664_RAW_SETTLE_DISCARD_SAMPLES;
	LOG_INF("RAW session state reset (settle discard %u samples @ ~25 Hz)",
		data->raw_runtime.settle_discard_left);
}

/* ============================================================================
 * RAW Mode Initialization
 * ============================================================================ */

/**
 * @brief Initialize MAX32664 in RAW mode
 *
 * This puts the hub in OUTPUT_MODE_PAUSE and enables passthrough access
 * to the underlying MAX86141 and LIS2DS12 sensors.
 *
 * @param dev MAX32664 device
 * @return 0 on success, negative error code on failure
 */
int max32664_raw_mode_init(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	const struct max32664_config *cfg = dev->config;
	int ret;
	uint8_t rx[1];

	LOG_INF("=== Initializing MAX32664 RAW Mode ===");
	max32664_raw_reset_session_state(dev);

	/* Verify we have the right hardware (GWEC variant with MAX86141 + LIS2DS12) */
	if (data->variant != MAX32664_VARIANT_C &&
	    data->variant != MAX32664_VARIANT_B) {
		LOG_ERR("RAW mode requires GWEC variant (B or C), detected: %d", 
			data->variant);
		return -ENOTSUP;
	}

	/* Verify AFE is MAX86141 */
	if (data->afe_whoami != 0x25 && data->afe_whoami != 0x36) {
		LOG_ERR("RAW mode requires MAX86141 AFE (0x25), got: 0x%02X",
			data->afe_whoami);
		return -ENOTSUP;
	}

	/* Variant C WHRM: accelerometer is mandatory (AN6924 / AlgoHub). */
	if (data->variant == MAX32664_VARIANT_C) {
		bool whoami_ok = (data->accel_whoami == 0x43 || data->accel_whoami == 0x1D);
		bool feeder_ok = cfg->accel_external &&
				 IS_ENABLED(CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER);

		if (!whoami_ok && !feeder_ok) {
			LOG_ERR("Variant C requires accelerometer (WHOAMI=0x%02X) or "
				"accel-source=external + EXTERNAL_ACCEL_FEEDER",
				data->accel_whoami);
			return -ENODEV;
		}
		if (!whoami_ok && feeder_ok) {
			LOG_WRN("Variant C: accel WHOAMI missing; relying on external feeder");
		}
	} else if (data->accel_whoami != 0x43 && data->accel_whoami != 0x1D) {
		LOG_WRN("No accelerometer detected (WHOAMI: 0x%02X), motion rejection disabled",
			data->accel_whoami);
	}

	LOG_INF("RAW Mode: MAX86141 (0x%02X) + LIS2DS12 (0x%02X)",
		data->afe_whoami, data->accel_whoami);

	/* Stop any running algorithms and put hub in PAUSE mode */
	ret = max32664_stop_algorithms_and_sensors(dev);
	if (ret) {
		LOG_ERR("Failed to stop hub algorithms: %d", ret);
		return ret;
	}

	/* Per Maxim UG Table 14: Enable Sensor uses **4** bytes; last byte selects
	 * data source (0x00 = hub-side, 0x01 = host-fed WRITE_INPUT / 0x14).
	 * GWEC 30.2.x does not auto-poll LIS2DS12 — boards with accel-source=external
	 * in DT must use 0x44 0x04 0x01 0x01 plus CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER
	 * or Family 0x12 stays empty while output mode is 0x01.
	 */
	uint8_t tx4[4];

	tx4[0] = 0x44;
	tx4[1] = HUB_SENSOR_IDX_MAX86141;
	tx4[2] = 0x01;
	tx4[3] = 0x00; /* hub-side PPG */
	LOG_DBG("Enabling MAX86141 (0x44 00 01 00)...");
	ret = max32664_i2c_transmit_retry(dev, tx4, sizeof(tx4), rx, 1, 100, 3);
	if (ret || rx[0] != MAX32664_STATUS_SUCCESS) {
		LOG_ERR("Failed to enable MAX86141: ret=%d, status=0x%02X", ret, rx[0]);
		return ret ? ret : -EIO;
	}
	k_msleep(50);

	if (cfg->accel_external) {
#if !IS_ENABLED(CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER)
		if (data->variant == MAX32664_VARIANT_C) {
			LOG_ERR("Variant C + accel-source=external requires "
				"CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER");
			return -ENODEV;
		}
		LOG_WRN("DT accel-source=external but CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER "
			"is disabled — enable it in prj.conf or hub output FIFO may stay empty");
#endif
		ret = raw_lis2ds12_prepare_host_feed(dev);
		if (ret) {
			LOG_WRN("External LIS2DS12 prep failed: %d (continuing)", ret);
		}

		tx4[0] = 0x44;
		tx4[1] = HUB_SENSOR_IDX_LIS2DS12;
		tx4[2] = 0x01;
		tx4[3] = 0x01; /* host-fed accel → Family 0x14 */
		LOG_DBG("Enabling accelerometer host-side feed (0x44 04 01 01)...");
		ret = max32664_i2c_transmit_retry(dev, tx4, sizeof(tx4), rx, 1, 250, 3);
		if (ret || rx[0] != MAX32664_STATUS_SUCCESS) {
			LOG_WRN("Host-side accel enable failed: ret=%d, status=0x%02X",
				ret, rx[0]);
		} else {
			LOG_INF("Accelerometer host-side feed enabled (status=0x%02x)", rx[0]);
		}
		data->accel_enabled = (ret == 0 && rx[0] == MAX32664_STATUS_SUCCESS);
		k_msleep(50);
	} else {
		tx4[0] = 0x44;
		tx4[1] = HUB_SENSOR_IDX_LIS2DS12;
		tx4[2] = 0x01;
		tx4[3] = 0x00; /* hub auto-polls KX122 / internal bus */
		LOG_DBG("Enabling accelerometer hub-side (0x44 04 01 00)...");
		ret = max32664_i2c_transmit_retry(dev, tx4, sizeof(tx4), rx, 1, 100, 3);
		if (ret || rx[0] != MAX32664_STATUS_SUCCESS) {
			LOG_WRN("Hub-side accel enable failed: ret=%d, status=0x%02X",
				ret, rx[0]);
		}
		data->accel_enabled = (ret == 0 && rx[0] == MAX32664_STATUS_SUCCESS);
		k_msleep(50);
	}

	/* Configure MAX86141 for RAW mode sampling */
	ret = max32664_raw_configure_ppg(dev);
	if (ret) {
		LOG_ERR("Failed to configure MAX86141: %d", ret);
		return ret;
	}

	/* Configure LIS2DS12 for continuous sampling */
	ret = max32664_raw_configure_accel(dev);
	if (ret) {
		LOG_WRN("Failed to configure LIS2DS12: %d (continuing without accel)", ret);
	}

	/*
	 * No LED bring-up at init. Keep AFE dark (PA=0 + SHDN) until measurement
	 * start (attr_set SAMPLING_FREQUENCY). RAW is sensor-only — WHRM stays off.
	 */
	(void)max86141_ppg_leds_set(dev, false, &data->raw_runtime.leds_enabled);
	(void)max86141_write_reg(dev, MAX86141_REG_SYSTEM_CTRL, 0x02); /* SHDN */
	LOG_INF("RAW: LEDs idle/off; host LED bring-up deferred to measurement start");

	/* NOTE: Output mode is set to SENSOR_DATA (0x01) in max32664_post_init_setup().
	 * This tells the hub to actively read sensor FIFOs and populate its output FIFO.
	 * We read the output FIFO via Family 0x12 in max32664_raw_read_ppg_ex(). */

	LOG_INF("=== RAW Mode Initialization Complete ===");
	LOG_INF("Use max86141_ppg and lis2ds12_accel sensor devices");

	return 0;
}

/**
 * @brief Configure MAX86141 for RAW mode PPG sampling
 */
int max32664_raw_configure_ppg(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	int ret;
	uint8_t val;

	LOG_INF("Configuring MAX86141 for RAW mode...");

	/* First, read current SYSTEM_CTRL to see if in shutdown */
	ret = max86141_read_reg(dev, MAX86141_REG_SYSTEM_CTRL, &val);
	if (ret) {
		LOG_ERR("Read SYSTEM_CTRL failed: %d", ret);
		/* Continue — subsequent writes still attempt a known-good map.
		 * WHRM/AEC is not used in sensor-only RAW (Track B). */
	}
	/*
	 * PPG_CONFIG1: ALC on, PPG2_RGE=01, PPG1_RGE=10, PPG_TINT=10 (58.7 µs).
	 * Sample rate is PPG_CONFIG2; LED count is LED_SEQ (not this register).
	 */
	ret = max86141_write_reg(dev, MAX86141_REG_PPG_CONFIG1, 0x1A);
	if (ret) {
		LOG_ERR("Write PPG_CONFIG1 failed: %d", ret);
		return ret;
	}
	LOG_DBG("MAX86141 PPG_CONFIG1 configured: 0x1A (TINT=58.7us)");

	/* PPG_CONFIG2: No sample averaging (SMP_AVG=0) -> FIFO output = ADC rate (25 Hz).
	 * 8x averaging (0x03) was previously used but reduced the FIFO output to 3.125 Hz,
	 * causing a 300-sample measurement to take ~96 s instead of 12 s. */
	ret = max86141_write_reg(dev, MAX86141_REG_PPG_CONFIG2, 0x00);
	if (ret) {
		LOG_ERR("Write PPG_CONFIG2 failed: %d", ret);
		return ret;
	}
	LOG_DBG("MAX86141 PPG_CONFIG2 configured: ~25 Hz, no averaging");

	/*
	 * SFH7074 LED map: LED1=Green, LED2=Red, LED3=IR.
	 * LEDC1=LED3(IR), LEDC2=LED2(Red), LEDC3=LED1(Green), LEDC4=DIRECT_AMBIENT.
	 * RAW ignores DT child LED currents — host AGC owns PA (max86141_ppg_set_led_pa).
	 */
	ret = max86141_write_reg(dev, MAX86141_REG_LED_SEQ1, MAX86141_LED_SEQ1_PRODUCT);
	if (ret) {
		LOG_ERR("Write LED_SEQ1 failed: %d", ret);
		return ret;
	}
	ret = max86141_write_reg(dev, MAX86141_REG_LED_SEQ2, MAX86141_LED_SEQ2_PRODUCT);
	if (ret) {
		LOG_ERR("Write LED_SEQ2 failed: %d", ret);
		return ret;
	}
	ret = max86141_write_reg(dev, MAX86141_REG_LED_SEQ3, MAX86141_LED_SEQ3_PRODUCT);
	if (ret) {
		LOG_ERR("Write LED_SEQ3 failed: %d", ret);
		return ret;
	}
	LOG_DBG("MAX86141 LED_SEQ: IR→Red→Green→Ambient (SFH7074)");

	/* LED_RANGE1: each RGE=01 → 62 mA full-scale. */
	ret = max86141_write_reg(dev, MAX86141_REG_LED_RANGE1, MAX86141_LED_RANGE1_62MA);
	if (ret) {
		LOG_ERR("Write LED_RANGE1 failed: %d", ret);
		return ret;
	}
	ret = max86141_write_reg(dev, MAX86141_REG_LED_RANGE2, 0x01);
	if (ret) {
		LOG_ERR("Write LED_RANGE2 failed: %d", ret);
		return ret;
	}
	LOG_DBG("MAX86141 LED_RANGE1=0x%02X (62 mA FS x3)",
		MAX86141_LED_RANGE1_62MA);

	/* Dual PD bias: SFH PD1→PD1IN, SFH PD3→PD2IN. */
	ret = max86141_write_reg(dev, MAX86141_REG_PD_CONFIG,
				 MAX86141_PD_CONFIG_SFH7074_DUAL);
	if (ret) {
		LOG_ERR("Write PD_CONFIG failed: %d", ret);
		return ret;
	}
	LOG_DBG("MAX86141 PD_CONFIG=0x%02X (SFH7074 dual PD bias)",
		MAX86141_PD_CONFIG_SFH7074_DUAL);

	/*
	 * Program PA=0 here. Measurement start / drain assert PA=0x7F (~half of
	 * 62 mA FS) via max86141_ppg_leds_set(true). Leaving PA high across init
	 * would leave LEDs glowing until host start.
	 */
	ret = max86141_write_reg(dev, MAX86141_REG_LED1_DRV_CUR, 0x00);
	if (ret) {
		LOG_ERR("Write LED1_PA failed: %d", ret);
		return ret;
	}
	data->raw_runtime.leds_enabled = false;

	ret = max86141_write_reg(dev, MAX86141_REG_LED2_DRV_CUR, 0x00);
	if (ret) {
		LOG_ERR("Write LED2_PA failed: %d", ret);
		return ret;
	}

	ret = max86141_write_reg(dev, MAX86141_REG_LED3_DRV_CUR, 0x00);
	if (ret) {
		LOG_ERR("Write LED3_PA failed: %d", ret);
		return ret;
	}
	LOG_DBG("MAX86141 LED1/LED2/LED3 PA idle (0); drive on at measurement start");

	/* Leave AFE in shutdown — LEDs stay dark until measurement start. */
	ret = max86141_write_reg(dev, MAX86141_REG_SYSTEM_CTRL, 0x02);
	if (ret) {
		LOG_ERR("Failed to shutdown MAX86141: %d", ret);
		return ret;
	}
	LOG_DBG("MAX86141 left in SHDN (LEDs off until measure)");
	k_msleep(10);

	/* Read back key registers to verify */
	ret = max86141_read_reg(dev, MAX86141_REG_SYSTEM_CTRL, &val);
	LOG_DBG("MAX86141 SYSTEM_CTRL after config: 0x%02X (expect 0x02 SHDN)", val);
	
	ret = max86141_read_reg(dev, MAX86141_REG_PPG_CONFIG1, &val);
	LOG_DBG("MAX86141 PPG_CONFIG1: 0x%02X (expect 0x1A)", val);
	
	ret = max86141_read_reg(dev, MAX86141_REG_LED_SEQ1, &val);
	LOG_DBG("MAX86141 LED_SEQ1: 0x%02X (expect 0x%02X)", val,
		MAX86141_LED_SEQ1_PRODUCT);
	
	ret = max86141_read_reg(dev, MAX86141_REG_LED2_DRV_CUR, &val);
	LOG_DBG("MAX86141 LED2_DRV_CUR: 0x%02X (expect 0x00 idle)", val);

	ret = max86141_read_reg(dev, MAX86141_REG_LED3_DRV_CUR, &val);
	LOG_DBG("MAX86141 LED3_DRV_CUR: 0x%02X (expect 0x00 idle)", val);

	ret = max86141_read_reg(dev, MAX86141_REG_LED_SEQ2, &val);
	LOG_DBG("MAX86141 LED_SEQ2: 0x%02X (expect 0x%02X, LEDC3=Green)", val,
		MAX86141_LED_SEQ2_PRODUCT);

	ret = max86141_read_reg(dev, MAX86141_REG_LED1_DRV_CUR, &val);
	LOG_DBG("MAX86141 LED1_DRV_CUR: 0x%02X (expect 0x00 idle)", val);

	ret = max86141_read_reg(dev, MAX86141_REG_LED_RANGE1, &val);
	LOG_DBG("MAX86141 LED_RANGE1: 0x%02X (expect 0x%02X)", val,
		MAX86141_LED_RANGE1_62MA);

	ret = max86141_read_reg(dev, MAX86141_REG_LED_RANGE2, &val);
	LOG_DBG("MAX86141 LED_RANGE2: 0x%02X (expect 0x01)", val);

	ret = max86141_read_reg(dev, MAX86141_REG_PD_CONFIG, &val);
	LOG_DBG("MAX86141 PD_CONFIG: 0x%02X (expect 0x%02X)", val,
		MAX86141_PD_CONFIG_SFH7074_DUAL);

	ret = max86141_read_reg(dev, MAX86141_REG_INT_STATUS1, &val);
	LOG_DBG("MAX86141 INT_STATUS1: 0x%02X", val);

	LOG_INF("MAX86141 configured: 25Hz IR/Red/Green/Ambient SEQ, dual-PD bias, PA=0, SHDN");
	return 0;
}

/**
 * @brief Configure LIS2DS12 for RAW mode acceleration sampling
 */
int max32664_raw_configure_accel(const struct device *dev)
{
	int ret;

	LOG_INF("Configuring LIS2DS12 for RAW mode...");

	/* Verify WHOAMI first */
	uint8_t whoami;
	ret = lis2ds12_read_reg(dev, LIS2DS12_REG_WHO_AM_I, &whoami);
	if (ret || (whoami != 0x43 && whoami != 0x1D)) {
		LOG_ERR("LIS2DS12 WHOAMI failed: ret=%d, whoami=0x%02X", ret, whoami);
		return ret ? ret : -ENODEV;
	}
	LOG_DBG("LIS2DS12 detected: WHOAMI=0x%02X", whoami);

	/* CTRL1: 25 Hz, ±2 g, BDU=1 (same encoding as host-feed / max32664c) */
	uint8_t ctrl1 = (LIS2DS12_ODR_25HZ << LIS2DS12_ODR_POS) |
		       (LIS2DS12_FS_2G << LIS2DS12_FS_POS) |
		       (1U << LIS2DS12_BDU_POS);

	ret = lis2ds12_write_reg(dev, LIS2DS12_REG_CTRL1, ctrl1);
	if (ret) return ret;
	LOG_DBG("LIS2DS12 CTRL1 configured: 25Hz, ±2g, BDU=1");

	/* CTRL2: Defaults (no filters) */
	ret = lis2ds12_write_reg(dev, LIS2DS12_REG_CTRL2, 0x00);
	if (ret) return ret;
	LOG_DBG("LIS2DS12 CTRL2 configured: default");

	LOG_INF("LIS2DS12 configured: 25Hz, ±2g");
	return 0;
}

/* ============================================================================
 * RAW Mode PPG Data Access
 * ============================================================================ */

static int max32664_raw_read_ppg_via_afe_fifo(const struct device *dev,
					      uint32_t *ir_value,
					      uint32_t *red_value,
					      uint32_t *green_value)
{
	uint8_t cnt;
	int ret;
	int drained = 0;
	uint32_t ir = 0;
	uint32_t red = 0;

	ret = max86141_read_reg(dev, MAX86141_REG_FIFO_DATA_CNT, &cnt);
	if (ret) {
		return ret;
	}

	if (cnt < 6U) {
		return 0;
	}

	while (cnt >= 6U && drained < 48) {
		uint8_t raw[6];

		ret = max86141_read_fifo_burst(dev, raw, sizeof(raw));
		if (ret) {
			return ret;
		}

		/* Product SEQ: LEDC1=IR(LED3), LEDC2=Red(LED2), LEDC3=Green(LED1). */
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
	/* AFE direct FIFO path only reads 2 slots (6 bytes) — green not yet burst-parsed here */
	if (green_value) {
		*green_value = 0;
	}

	return 1;
}

/**
 * @brief Drain the hub output FIFO into @p out (up to @p out_cap samples).
 *
 * Performs one FIFO count query, pops every pending frame (discarding settle
 * and invalid entries), and returns all valid IR/Red/Green triplets. Call
 * repeatedly until 0 is returned to empty the hub FIFO — matches max3010x_ppg
 * batching.
 *
 * @param dev MAX32664 device
 * @param out Output sample array
 * @param out_cap Capacity of @p out
 * @return Number of samples written (0 if empty), or negative errno
 */
int max32664_raw_drain_ppg(const struct device *dev,
			   struct max32664_ppg_sample *out,
			   size_t out_cap)
{
	int ret;
	struct max32664_data *hub_data = dev->data;
	struct max32664_raw_runtime *rt = &hub_data->raw_runtime;
	uint8_t tx_buf[2];
	uint8_t remaining = 0xFF;
	int rem_ret = -EAGAIN;
	bool fifo_drained = false;
	const uint32_t max_valid_adc = CONFIG_MAX32664_RAW_MAX_VALID_ADC;
	const uint32_t near_sat_adc = CONFIG_MAX32664_RAW_NEAR_SAT_ADC;
	size_t n_out = 0U;

	if (out_cap == 0U) {
		return 0;
	}

	hub_data->sample.fifo_accel_valid = false;

	/*
	 * Never resurrect SFH LEDs after STOP. A late ppg_algo fetch / in-flight
	 * poll_work used to hit this PA restore after attr_set(0) had already
	 * cleared leds_enabled + SHDN — LEDs kept blinking past session end.
	 */
	if (!hub_data->sampling) {
		return 0;
	}

	/*
	 * Conditional PA restore (sensor-only RAW: host owns LED_PA).
	 * Only reassert when runtime flag is clear or IR PA readback is 0 —
	 * avoids per-drain I2C while recovering from hub glitches.
	 */
	{
		uint8_t ir_pa = 0U;
		bool need_restore = !rt->leds_enabled;

		if (!need_restore) {
			if (max86141_read_reg(dev, MAX86141_REG_LED3_DRV_CUR, &ir_pa) != 0 ||
			    ir_pa == 0U) {
				need_restore = true;
			}
		}
		if (need_restore) {
			(void)max86141_ppg_leds_set(dev, true, &rt->leds_enabled);
		}
	}

	if (rt->use_afe_fifo_fallback) {
		uint32_t ir = 0U;
		uint32_t red = 0U;
		uint32_t green = 0U;

		ret = max32664_raw_read_ppg_via_afe_fifo(dev, &ir, &red, &green);
		if (ret < 0) {
			LOG_ERR("RAW fallback AFE FIFO read failed: %d", ret);
			return ret;
		}
		if (ret == 0) {
			return 0;
		}
		if (rt->settle_discard_left > 0U) {
			rt->settle_discard_left--;
			rt->settle_discarded++;
			if (rt->settle_discarded == 1U) {
				LOG_INF("RAW settle: discarding first %u FIFO frames after LED wake",
					(unsigned int)CONFIG_MAX32664_RAW_SETTLE_DISCARD_SAMPLES);
			}
			if (rt->settle_discard_left == 0U) {
				LOG_INF("RAW settle complete (%u frames discarded); accepting samples",
					rt->settle_discarded);
			}
			return 0;
		}
		out[0].ir = ir;
		out[0].red = red;
		out[0].green = green;
		return 1;
	}

	size_t payload_bytes = 0U;

	if (rt->sensor_report_bytes == 0U) {
		uint8_t fw_major = (hub_data->firmware_version >> 16) & 0xFF;
		uint8_t fw_minor = (hub_data->firmware_version >> 8) & 0xFF;
		uint8_t fw_patch = hub_data->firmware_version & 0xFF;

		if (fw_major == 30U && fw_minor == 2U && fw_patch == 2U) {
			rt->sensor_report_bytes = 24U;
			LOG_INF("RAW sensor report size query skipped on FW 30.2.2; using %u bytes/sample",
				rt->sensor_report_bytes);
		} else {
			uint8_t size_tx[2] = {0x11, 0x05};
			uint8_t size_rx[2] = {0};

			ret = max32664_i2c_transmit_retry(dev, size_tx, sizeof(size_tx),
							  size_rx, sizeof(size_rx),
							  10, 3);
			if ((ret == 0) && (size_rx[0] == 0x00) &&
			    (size_rx[1] >= 6U) && (size_rx[1] <= 64U)) {
				rt->sensor_report_bytes = size_rx[1];
				LOG_INF("RAW sensor report size from hub: %u bytes/sample (0x11,0x05)",
					rt->sensor_report_bytes);
			} else {
				rt->sensor_report_bytes = 24U;
				LOG_WRN("RAW sensor report size query failed (ret=%d st=0x%02x sz=%u); using fallback=%u",
					ret, size_rx[0], size_rx[1], rt->sensor_report_bytes);
			}
		}
	}

	payload_bytes = rt->sensor_report_bytes;
	if (hub_data->fifo_entry_size_runtime > 0U) {
		payload_bytes = hub_data->fifo_entry_size_runtime;
	}

	if (payload_bytes > 64U) {
		LOG_WRN("RAW FIFO payload %zu capped to 64", payload_bytes);
		payload_bytes = 64U;
	}

	if (!rt->payload_logged) {
		LOG_INF("RAW hub FIFO: %zu data bytes/sample (+1 status) (variant=%d)",
			payload_bytes, hub_data->variant);
		rt->payload_logged = true;
	}

	uint8_t count_resp[2];

	tx_buf[0] = 0x12;
	tx_buf[1] = 0x00;
	ret = max32664_i2c_transmit(dev, tx_buf, 2, count_resp, 2, MAX32664_CMD_DELAY_FIFO_MS);
	if (ret) {
		LOG_ERR("Hub FIFO query failed: %d", ret);
		return ret;
	}

	uint8_t num_samples = count_resp[1];

	rt->read_count++;

	if (num_samples == 0U) {
		if (rt->read_count <= CONFIG_MAX32664_RAW_FIFO_LOG_INITIAL_READS ||
		    (rt->read_count % CONFIG_MAX32664_RAW_FIFO_LOG_EVERY_N_READS) == 0U) {
			LOG_DBG("Hub FIFO empty (read #%u)", rt->read_count);
		}
		return 0;
	}

	const bool log_summary_now =
		(rt->read_count <= CONFIG_MAX32664_RAW_FIFO_LOG_INITIAL_READS) ||
		((rt->read_count % CONFIG_MAX32664_RAW_FIFO_LOG_EVERY_N_READS) == 0U);
	const bool log_dump_now =
		IS_ENABLED(CONFIG_MAX32664_RAW_FIFO_DEBUG_DUMP) &&
		((rt->read_count <= CONFIG_MAX32664_RAW_FIFO_DEBUG_DUMP_INITIAL_READS) ||
		 ((rt->read_count % CONFIG_MAX32664_RAW_FIFO_DEBUG_DUMP_EVERY_N_READS) == 0U));

	const size_t read_size = payload_bytes + 1U;
	uint8_t sample_data[65];
	uint8_t read_cmd2[2] = {0x12, 0x01};
	uint8_t read_cmd3[3] = {0x12, 0x01, 0x01};
	uint8_t *read_cmd = rt->use_fifo_read_count_arg ? read_cmd3 : read_cmd2;
	const size_t read_cmd_len = rt->use_fifo_read_count_arg ? sizeof(read_cmd3) : sizeof(read_cmd2);

	if (read_size < 7U || payload_bytes < 6U) {
		rt->rejected_count++;
		LOG_WRN("RAW frame too short (payload=%zu read_size=%zu), skipping batch",
			payload_bytes, read_size);
		return 0;
	}

	uint32_t last_ir = 0U;
	uint32_t last_red = 0U;
	uint32_t last_green = 0U;
	uint32_t last_ambient = 0U;

	for (uint8_t s = 0; s < num_samples; s++) {
		uint32_t led1 = 0U;
		uint32_t led2 = 0U;
		uint32_t led3 = 0U;
		uint32_t led4 = 0U;
		uint8_t led1_meta = 0U;
		uint8_t led2_meta = 0U;
		uint8_t led3_meta = 0U;
		uint8_t led4_meta = 0U;
		int16_t ax = 0;
		int16_t ay = 0;
		int16_t az = 0;
		bool accel_valid = false;

		ret = max32664_i2c_transmit(dev, read_cmd, read_cmd_len,
					    sample_data, read_size,
					    MAX32664_CMD_DELAY_FIFO_MS);
		if (ret) {
			LOG_ERR("Hub FIFO read failed (sample %u/%u): %d",
				s + 1, num_samples, ret);
			return (n_out > 0U) ? (int)n_out : ret;
		}

		if (payload_bytes >= 3U) {
			uint32_t led1_word = ((uint32_t)sample_data[1] << 16) |
					     ((uint32_t)sample_data[2] << 8) |
					     (uint32_t)sample_data[3];

			led1 = led1_word & 0x7FFFFU;
			led1_meta = (uint8_t)((led1_word >> 19) & 0x1FU);
		}
		if (payload_bytes >= 6U) {
			uint32_t led2_word = ((uint32_t)sample_data[4] << 16) |
					     ((uint32_t)sample_data[5] << 8) |
					     (uint32_t)sample_data[6];

			led2 = led2_word & 0x7FFFFU;
			led2_meta = (uint8_t)((led2_word >> 19) & 0x1FU);
		}
		if (payload_bytes >= 9U) {
			uint32_t led3_word = ((uint32_t)sample_data[7] << 16) |
					     ((uint32_t)sample_data[8] << 8) |
					     (uint32_t)sample_data[9];

			led3 = led3_word & 0x7FFFFU;
			led3_meta = (uint8_t)((led3_word >> 19) & 0x1FU);
		}
		if (payload_bytes >= 12U) {
			uint32_t led4_word = ((uint32_t)sample_data[10] << 16) |
					     ((uint32_t)sample_data[11] << 8) |
					     (uint32_t)sample_data[12];

			led4 = led4_word & 0x7FFFFU;
			led4_meta = (uint8_t)((led4_word >> 19) & 0x1FU);
		}

		uint32_t ir = led1;
		uint32_t red = led2;
		uint32_t green_raw = led3;
		/* Hub may still present a 4th word; product SEQ now includes
		 * DIRECT_AMBIENT so this is a real ambient exposure. */
		uint32_t ambient = max86141_green_sanitize_adc(led4);

		/*
		 * Post-LED-wake settle: drop early frames so SHDN/PA exit and
		 * stale FIFO never reach the algo (see Kconfig settle discard).
		 */
		if (rt->settle_discard_left > 0U) {
			rt->settle_discard_left--;
			rt->settle_discarded++;
			if (rt->settle_discarded == 1U) {
				LOG_INF("RAW settle: discarding first %u FIFO frames after LED wake",
					(unsigned int)CONFIG_MAX32664_RAW_SETTLE_DISCARD_SAMPLES);
			}
			if (rt->settle_discard_left == 0U) {
				LOG_INF("RAW settle complete (%u frames discarded); accepting samples",
					rt->settle_discarded);
			}
			continue;
		}

		if (payload_bytes >= 24U) {
			ax = (int16_t)(((uint16_t)sample_data[19] << 8) |
				       (uint16_t)sample_data[20]);
			ay = (int16_t)(((uint16_t)sample_data[21] << 8) |
				       (uint16_t)sample_data[22]);
			az = (int16_t)(((uint16_t)sample_data[23] << 8) |
				       (uint16_t)sample_data[24]);
			accel_valid = true;
		}

		uint32_t green = max86141_green_sanitize_adc(green_raw);

		/*
		 * Hold last good green across true hub fillers (0x7FFFF) when
		 * IR/Red show contact. Zeroing those gaps kills HR/Hb.
		 */
		if ((green == 0U) && (last_green > 0U) &&
		    (ir > 50000U) && (red > 50000U)) {
			green = last_green;
		}

		last_ir = ir;
		last_red = red;
		last_green = green;
		last_ambient = ambient;

		if (log_dump_now) {
			LOG_DBG("RAW FIFO pop %u/%u (read #%u): IR=%u Red=%u Grn=%u Amb=%u | Acc=%s(%d,%d,%d)",
				s + 1U, num_samples, rt->read_count,
				ir, red, green, ambient,
				accel_valid ? "" : "N/A ",
				accel_valid ? ax : 0,
				accel_valid ? ay : 0,
				accel_valid ? az : 0);
			LOG_DBG("RAW FIFO meta %u/%u: L1=0x%02x L2=0x%02x L3=0x%02x L4=0x%02x LED=%u/%u/%u/%u",
				s + 1U, num_samples,
				led1_meta, led2_meta, led3_meta, led4_meta,
				led1, led2, led3, led4);
		}

		if ((ir == 0U) || (red == 0U) ||
		    (ir > max_valid_adc) || (red > max_valid_adc) ||
		    (ir >= near_sat_adc) || (red >= near_sat_adc)) {
			rt->rejected_count++;
			if (rt->rejected_count <= 10U || (rt->rejected_count % 50U) == 0U) {
				LOG_WRN("RAW invalid sample rejected: IR=%u RED=%u (rej=%u)",
					ir, red, rt->rejected_count);
			}
			continue;
		}

		if (rt->prev_valid &&
		    (((ir < 1024U) && (rt->prev_ir > 50000U)) ||
		     ((red < 1024U) && (rt->prev_red > 50000U)))) {
			rt->rejected_count++;
			if (rt->rejected_count <= 10U || (rt->rejected_count % 50U) == 0U) {
				LOG_WRN("RAW reset-pattern sample rejected: prev=(%u,%u) now=(%u,%u)",
					rt->prev_ir, rt->prev_red, ir, red);
			}
			continue;
		}

		if (n_out < out_cap) {
			out[n_out].ir = ir;
			out[n_out].red = red;
			out[n_out].green = green;
			out[n_out].ambient = ambient;
			n_out++;
		}

		rt->prev_ir = ir;
		rt->prev_red = red;
		rt->prev_valid = true;
	}

	if (log_summary_now) {
		LOG_INF("Hub FIFO: popped %u, kept %zu/%zu valid (IR=%u Red=%u Grn=%u Amb=%u, read #%u)",
			num_samples, n_out, out_cap, last_ir, last_red, last_green, last_ambient,
			rt->read_count);
		if (log_dump_now && n_out > 0U) {
			LOG_HEXDUMP_DBG(&sample_data[1], payload_bytes, "Raw sensor data");
		}
	}

	rem_ret = max32664_get_fifo_count(dev, &remaining);
	fifo_drained = (rem_ret == 0) && (remaining < num_samples);

	if (fifo_drained) {
		rt->fifo_stagnant_reads = 0;
	} else if (rem_ret == 0) {
		if (rt->fifo_stagnant_reads < UINT8_MAX) {
			rt->fifo_stagnant_reads++;
		}
		if (!rt->use_fifo_read_count_arg && rt->fifo_stagnant_reads >= 3U) {
			rt->use_fifo_read_count_arg = true;
			LOG_WRN("RAW FIFO not draining with 2-byte read cmd; switching to 3-byte read cmd (0x12,0x01,0x01)");
		}
		if (rt->use_fifo_read_count_arg && rt->fifo_stagnant_reads >= 8U) {
			rt->use_afe_fifo_fallback = true;
			LOG_WRN("RAW hub FIFO remains non-draining; switching to direct MAX86141 FIFO read fallback");
		}
	}

	if ((rt->read_count <= 10U) || ((rt->read_count % 50U) == 0U)) {
		uint8_t hub_status = 0U;

		if (max32664_get_hub_status(dev, &hub_status) == 0) {
			bool err0 = false;
			bool data_rdy = false;
			bool fifo_out_ovr = false;
			bool fifo_in_ovr = false;
			bool host_accel_uf = false;

			max32664_parse_status(hub_status, &err0, &data_rdy,
					     &fifo_out_ovr, &fifo_in_ovr, &host_accel_uf);
			if (fifo_out_ovr || fifo_in_ovr || host_accel_uf || err0) {
				LOG_WRN("RAW hub status anomalies: 0x%02x (err=%d out_ovr=%d in_ovr=%d accel_uf=%d)",
					hub_status, err0, fifo_out_ovr, fifo_in_ovr, host_accel_uf);
			}
		}
	}

	return (int)n_out;
}

int max32664_raw_read_ppg_ex(const struct device *dev,
			     uint32_t *ir_value, uint32_t *red_value,
			     uint32_t *green_value)
{
	struct max32664_ppg_sample one;
	int n;

	n = max32664_raw_drain_ppg(dev, &one, 1U);
	if (n <= 0) {
		return n;
	}

	*ir_value = one.ir;
	*red_value = one.red;
	if (green_value) {
		*green_value = one.green;
	}
	return 1;
}

int max32664_raw_read_ppg(const struct device *dev,
			  uint32_t *ir_value, uint32_t *red_value)
{
	return max32664_raw_read_ppg_ex(dev, ir_value, red_value, NULL);
}

/* ============================================================================
 * RAW Mode Accelerometer Data Access
 * ============================================================================ */

/**
 * @brief Read acceleration from LIS2DS12
 *
 * @param dev MAX32664 device
 * @param x Pointer to store X acceleration (milli-g)
 * @param y Pointer to store Y acceleration (milli-g)
 * @param z Pointer to store Z acceleration (milli-g)
 * @return 0 on success, negative error code on failure
 */
int max32664_raw_read_accel(const struct device *dev,
			    int16_t *x, int16_t *y, int16_t *z)
{
	uint8_t data[6];
	int ret;

	/* Read all 6 output registers */
	for (int i = 0; i < 6; i++) {
		ret = lis2ds12_read_reg(dev, LIS2DS12_REG_OUT_X_L + i, &data[i]);
		if (ret) {
			LOG_ERR("Failed to read accel register %d: %d", i, ret);
			return ret;
		}
	}

	/* Convert to signed 16-bit (raw values) */
	int16_t raw_x = (int16_t)((data[1] << 8) | data[0]);
	int16_t raw_y = (int16_t)((data[3] << 8) | data[2]);
	int16_t raw_z = (int16_t)((data[5] << 8) | data[4]);

	/* Convert to milli-g at ±2g range (0.061 mg/LSB for 16-bit) */
	*x = (raw_x * 61) / 1000;
	*y = (raw_y * 61) / 1000;
	*z = (raw_z * 61) / 1000;

	return 0;
}

/* Host AGC: independent closed-loop targeting per LED channel.
 *
 * CORRECTED 2026-08-24: IR narrowed from 20000-420000 to actively target
 * the reference operating point (IR_DC~250000, per the SpO2~95.6%/
 * Hb~14g/dL worked example) instead of a very wide band that left IR
 * sitting at ~180-200k (below target) without ever triggering a
 * correction, since that already fell inside the old 20k-420k range.
 * LO/HI give a ±14-15% window around each target so the AGC doesn't
 * hunt/oscillate on every small fluctuation.
 *
 * EXTENDED: Red and Green now get their own DC bands and are driven by
 * their own independently-tracked PA (max86141_ppg_get/set_led_pa_red()/
 * _green()) instead of being a fixed fraction of IR's current. That old
 * scheme could only ever be "correct" if the subject's actual per-channel
 * perfusion index happened to match whatever single session the fraction
 * was hand-tuned from — it had no way to correct itself.
 *
 * AC is tracked against the same reference point (IR_AC~2000, Red_AC~
 * 1200, Green_AC~500) but treated as a secondary/diagnostic signal, not
 * a second target PA can always hit: R = (AC/DC) is, in the ideal linear
 * region, mostly current-independent (see red_pa_from() comment above) —
 * it's a property of tissue perfusion and optical coupling. If DC is
 * already inside its band and AC is still outside its band, that's a
 * real perfusion-index mismatch for this subject/contact, not something
 * more or less LED current can fix without pushing DC out of its own
 * safe window. This is logged clearly instead of silently forcing PA
 * outside the DC-safe range chasing a number current can't control.
 */
#define RAW_AGC_DC_LO         215000U
#define RAW_AGC_DC_HI         285000U
#define RAW_AGC_RED_DC_LO     221000U
#define RAW_AGC_RED_DC_HI     299000U
#define RAW_AGC_GREEN_DC_LO   229500U
#define RAW_AGC_GREEN_DC_HI   310500U
#define RAW_AGC_AC_IR_LO      1300U
#define RAW_AGC_AC_IR_HI      2700U
#define RAW_AGC_AC_RED_LO      780U
#define RAW_AGC_AC_RED_HI     1620U
#define RAW_AGC_AC_GREEN_LO    325U
#define RAW_AGC_AC_GREEN_HI    675U
#define RAW_AGC_STEP    0x10U

/* One channel's step-and-log logic, shared by IR/Red/Green so all three
 * behave identically apart from their target bands. DC is the primary,
 * safety-relevant target (keeps the ADC in a usable, non-saturating
 * range); AC is reported for visibility only — see block comment above
 * for why it isn't a second thing PA chases on its own. */
static uint8_t agc_step_channel(const char *name, uint32_t dc, uint32_t dc_lo, uint32_t dc_hi,
				 uint32_t ac, uint32_t ac_lo, uint32_t ac_hi, uint8_t pa)
{
	if (dc == 0U) {
		return pa;
	}

	if (dc < dc_lo && pa < MAX86141_LED_PA_MAX) {
		uint16_t next = (uint16_t)pa + RAW_AGC_STEP;

		if (next > MAX86141_LED_PA_MAX) {
			next = MAX86141_LED_PA_MAX;
		}
		pa = (uint8_t)next;
		LOG_INF("RAW AGC %s: DC=%u low -> PA=0x%02X", name, dc, pa);
	} else if (dc > dc_hi && pa > MAX86141_LED_PA_MIN) {
		int16_t next = (int16_t)pa - (int16_t)RAW_AGC_STEP;

		if (next < (int16_t)MAX86141_LED_PA_MIN) {
			next = (int16_t)MAX86141_LED_PA_MIN;
		}
		pa = (uint8_t)next;
		LOG_INF("RAW AGC %s: DC=%u high -> PA=0x%02X", name, dc, pa);
	} else if (ac != 0U && (ac < ac_lo || ac > ac_hi)) {
		LOG_WRN("RAW AGC %s: DC=%u in-band but AC=%u outside [%u,%u] — "
			"perfusion-index mismatch for this contact, not a PA problem",
			name, dc, ac, ac_lo, ac_hi);
	}

	return pa;
}

uint8_t max32664_raw_host_agc(const struct device *hub,
			       uint32_t dc_ir, uint32_t ac_ir,
			       uint32_t dc_red, uint32_t ac_red,
			       uint32_t dc_green, uint32_t ac_green)
{
	uint8_t ir_pa = max86141_ppg_get_led_pa();
	uint8_t red_pa = max86141_ppg_get_led_pa_red();
	uint8_t green_pa = max86141_ppg_get_led_pa_green();

	ARG_UNUSED(hub);

	ir_pa = agc_step_channel("IR", dc_ir, RAW_AGC_DC_LO, RAW_AGC_DC_HI,
				  ac_ir, RAW_AGC_AC_IR_LO, RAW_AGC_AC_IR_HI, ir_pa);
	max86141_ppg_set_led_pa(ir_pa);

	red_pa = agc_step_channel("RED", dc_red, RAW_AGC_RED_DC_LO, RAW_AGC_RED_DC_HI,
				   ac_red, RAW_AGC_AC_RED_LO, RAW_AGC_AC_RED_HI, red_pa);
	max86141_ppg_set_led_pa_red(red_pa);

	green_pa = agc_step_channel("GREEN", dc_green, RAW_AGC_GREEN_DC_LO, RAW_AGC_GREEN_DC_HI,
				     ac_green, RAW_AGC_AC_GREEN_LO, RAW_AGC_AC_GREEN_HI, green_pa);
	max86141_ppg_set_led_pa_green(green_pa);

	return ir_pa;
}

uint8_t max32664_raw_get_led_pa(void)
{
	return max86141_ppg_get_led_pa();
}

void max32664_raw_seed_led_pa_from_cal(void)
{
#if defined(CONFIG_CALIBRATION)
	const struct ppg_calibration *ppg = calibration_get_ppg();

	if (ppg == NULL) {
		return;
	}

	/* Prefer IR current; 62 mA FS → PA ≈ ua * 255 / 62000 */
	uint16_t ua = ppg->led_current_ir_ua;

	if (ua == 0U) {
		ua = ppg->led_current_red_ua;
	}
	if (ua == 0U) {
		return;
	}

	uint32_t pa = ((uint32_t)ua * 255U) / 62000U;

	if (pa < MAX86141_LED_PA_MIN) {
		pa = MAX86141_LED_PA_MIN;
	} else if (pa > MAX86141_LED_PA_MAX) {
		pa = MAX86141_LED_PA_MAX;
	}
	max86141_ppg_set_led_pa((uint8_t)pa);
	LOG_INF("RAW: seeded LED PA=0x%02X from factory %u uA", (uint8_t)pa, ua);
#endif
}

/* ============================================================================
 * Sensor API Registration (creates child devices in RAW mode)
 * ============================================================================ */

#ifdef CONFIG_MAX32664_RAW_PPG_SENSOR
/* Forward declaration - implemented in max86141_ppg.c */
extern const struct sensor_driver_api max86141_ppg_driver_api;
#endif

#ifdef CONFIG_MAX32664_RAW_ACCEL_SENSOR
/* Forward declaration - implemented in lis2ds12_accel.c */
extern const struct sensor_driver_api lis2ds12_accel_driver_api;
#endif
