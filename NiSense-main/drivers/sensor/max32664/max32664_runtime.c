/**
 * @file max32664_runtime.c
 * @brief MAX32664 runtime helpers (output mode + HUB sampling lifecycle)
 */

#include "max32664_priv.h"

#if !defined(CONFIG_MAX32664_MODE_RAW) && !defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
#include "max86141_regs.h"
#endif

LOG_MODULE_DECLARE(max32664, CONFIG_SENSOR_LOG_LEVEL);

int max32664_runtime_read_output_mode(const struct device *dev, uint8_t *mode_out)
{
	uint8_t tx[2] = {0x11, 0x00};
	uint8_t rx[2];
	int ret;

	ret = max32664_i2c_transmit_retry(dev, tx, sizeof(tx), rx, sizeof(rx), 10, 3);
	if (ret) {
		return ret;
	}

	*mode_out = rx[1];
	return 0;
}

int max32664_runtime_try_output_mode(const struct device *dev,
				      uint8_t preferred_mode,
				      uint8_t fallback_mode)
{
	uint8_t tx[3];
	int ret;

	LOG_INF("Setting output mode to 0x%02x (preferred)...", preferred_mode);
	tx[0] = 0x10;
	tx[1] = 0x00;
	tx[2] = preferred_mode;

	ret = max32664_i2c_transmit_retry(dev, tx, 3, NULL, 0, 10, 3);
	if (ret) {
		LOG_ERR("Failed to send output mode command: %d", ret);
		return ret;
	}

	k_msleep(50);

	uint8_t read_mode = 0xFF;
	ret = max32664_runtime_read_output_mode(dev, &read_mode);
	if (ret == 0 && read_mode == preferred_mode) {
		LOG_INF("Output mode 0x%02x set (read-back=0x%02x)", preferred_mode, read_mode);
		return 0;
	}
	if (ret == 0) {
		LOG_WRN("Output mode read-back mismatch (got 0x%02x, expected 0x%02x)",
			read_mode, preferred_mode);
	} else {
		LOG_WRN("Failed to read output mode: %d", ret);
	}

	LOG_INF("Retrying with fallback mode 0x%02x...", fallback_mode);
	tx[0] = 0x10;
	tx[1] = 0x00;
	tx[2] = fallback_mode;

	ret = max32664_i2c_transmit_retry(dev, tx, 3, NULL, 0, 10, 3);
	if (ret) {
		LOG_ERR("Failed to send fallback mode command: %d", ret);
		return ret;
	}

	k_msleep(50);

	read_mode = 0xFF;
	ret = max32664_runtime_read_output_mode(dev, &read_mode);
	if (ret == 0 && read_mode == fallback_mode) {
		LOG_INF("Fallback mode 0x%02x set (read-back=0x%02x)", fallback_mode, read_mode);
		return 0;
	}
	if (ret == 0) {
		LOG_ERR("Fallback mode read-back mismatch (got 0x%02x, expected 0x%02x)",
			read_mode, fallback_mode);
		return -EINVAL;
	}

	LOG_ERR("Failed to verify fallback mode: %d", ret);
	return -EINVAL;
}

#if IS_ENABLED(CONFIG_MAX32664_MODE_HUB)
int max32664_runtime_request_hub_output_mode(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	int ret;

#if IS_ENABLED(CONFIG_MAX32664_HUB_OUTPUT_MODE_ALGO_PLUS_SENSOR)
	LOG_INF("HUB output: requesting ALGO+SENSOR (0x03, %u B/entry), "
		"fallback to ALGO-only (0x02)",
		(unsigned)CONFIG_MAX32664_HUB_MODE03_FRAME_BYTES);

	ret = max32664_runtime_try_output_mode(dev, 0x03, 0x02);
	if (ret) {
		LOG_ERR("HUB output: both 0x03 and 0x02 rejected: %d", ret);
		data->fifo_entry_size_runtime = 0;
		return ret;
	}

	uint8_t active_mode = 0xFF;
	(void)max32664_runtime_read_output_mode(dev, &active_mode);
	if (active_mode == 0x03) {
		data->fifo_entry_size_runtime = CONFIG_MAX32664_HUB_MODE03_FRAME_BYTES;
		LOG_INF("HUB output: mode 0x03 active (entry=%u B incl. sensor prefix)",
			(unsigned)data->fifo_entry_size_runtime);
	} else {
		data->fifo_entry_size_runtime = 0;
		LOG_WRN("HUB output: firmware fell back to mode 0x%02x  raw PPG "
			"diagnostics unavailable for this boot", active_mode);
	}
	return 0;
#else
	LOG_INF("HUB output: requesting ALGO-only (0x02), fallback to PAUSE");
	ret = max32664_runtime_try_output_mode(dev, 0x02, 0x00);
	data->fifo_entry_size_runtime = 0;
	return ret;
#endif
}

int max32664_runtime_force_output_mode_algo_only(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	int ret;

	LOG_WRN("Forcing hub output mode 0x02 (algorithm-only)  extended FIFO "
		"layout did not contain a valid WHRM block");
	ret = max32664_runtime_try_output_mode(dev, 0x02, 0x00);
	if (ret) {
		LOG_ERR("Failed to force output mode 0x02: %d", ret);
		return ret;
	}
	data->fifo_entry_size_runtime = 0;
	return 0;
}
#endif

#if !defined(CONFIG_MAX32664_MODE_RAW) && !defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
#if IS_ENABLED(CONFIG_MAX32664_LOG_AFE_LED_ON_SAMPLING_START)
static void max32664_hub_log_afe_led_state(const struct device *dev)
{
	struct max32664_data *data = dev->data;

	if (data->variant != MAX32664_VARIANT_B && data->variant != MAX32664_VARIANT_C) {
		return;
	}

	k_msleep(120);

	uint8_t seq1 = 0, pa1 = 0, pa2 = 0, pa3 = 0, range1 = 0, sys = 0;

	(void)max32664_read_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
				       MAX86141_REG_LED_SEQ1, &seq1);
	(void)max32664_read_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
				       MAX86141_REG_LED1_DRV_CUR, &pa1);
	(void)max32664_read_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
				       MAX86141_REG_LED2_DRV_CUR, &pa2);
	(void)max32664_read_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
				       MAX86141_REG_LED3_DRV_CUR, &pa3);
	(void)max32664_read_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
				       MAX86141_REG_LED_RANGE1, &range1);
	(void)max32664_read_sensor_reg(dev, HUB_SENSOR_IDX_MAX86141,
				       MAX86141_REG_SYSTEM_CTRL, &sys);

	LOG_INF("MAX86141 ~120ms after sampling start: SEQ1=0x%02x "
		"LED1_PA=0x%02x LED2_PA=0x%02x LED3_PA=0x%02x RANGE1=0x%02x "
		"SYSTEM_CTRL=0x%02x (SFH7074: 1=grn 2=red 3=IR; IR needs camera)",
		seq1, pa1, pa2, pa3, range1, sys);
}
#endif

static void max32664_hub_ensure_afe_ppg_running(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	uint8_t seq1 = 0, sys = 0, pa1 = 0, pa2 = 0;

	if (data->variant != MAX32664_VARIANT_B && data->variant != MAX32664_VARIANT_C) {
		return;
	}

	(void)max86141_read_reg(dev, MAX86141_REG_LED_SEQ1, &seq1);
	(void)max86141_read_reg(dev, MAX86141_REG_LED1_DRV_CUR, &pa1);
	(void)max86141_read_reg(dev, MAX86141_REG_LED2_DRV_CUR, &pa2);
	(void)max86141_read_reg(dev, MAX86141_REG_SYSTEM_CTRL, &sys);

	if (seq1 == 0x00 && pa1 == 0x00 && pa2 == 0x00) {
		LOG_WRN("HUB: MAX86141 LED path idle; applying SFH7074 product SEQ "
			"(IR→Red→Green) + dual-PD bias");

		(void)max86141_write_reg(dev, MAX86141_REG_FIFO_CONFIG1, 0x7F);
		(void)max86141_write_reg(dev, MAX86141_REG_PPG_SYNC_CTRL, 0x00);
		(void)max86141_write_reg(dev, MAX86141_REG_PPG_CONFIG1, 0x1A);
		(void)max86141_write_reg(dev, MAX86141_REG_PPG_CONFIG2, 0x00);
		(void)max86141_write_reg(dev, MAX86141_REG_LED_SEQ1,
					 MAX86141_LED_SEQ1_PRODUCT);
		(void)max86141_write_reg(dev, MAX86141_REG_LED_SEQ2,
					 MAX86141_LED_SEQ2_PRODUCT);
		(void)max86141_write_reg(dev, MAX86141_REG_LED_SEQ3,
					 MAX86141_LED_SEQ3_PRODUCT);
		(void)max86141_write_reg(dev, MAX86141_REG_LED_RANGE1,
					 MAX86141_LED_RANGE1_62MA);
		(void)max86141_write_reg(dev, MAX86141_REG_PD_CONFIG,
					 MAX86141_PD_CONFIG_SFH7074_DUAL);
		(void)max86141_write_reg(dev, MAX86141_REG_LED1_DRV_CUR, 0x40);
		(void)max86141_write_reg(dev, MAX86141_REG_LED2_DRV_CUR, 0x40);
		(void)max86141_write_reg(dev, MAX86141_REG_LED3_DRV_CUR, 0x40);
		(void)max86141_write_reg(dev, MAX86141_REG_SYSTEM_CTRL, 0x00);
		return;
	}

	if (data->variant == MAX32664_VARIANT_C && pa1 != 0x00 && pa2 == 0x00) {
		uint8_t target = pa1 < 0x40 ? 0x40 : pa1;

		LOG_INF("HUB: LED2 was 0; restoring product SEQ + Red/IR PA "
			"(LED1=0x%02x → LED2/LED3=0x%02x, was SEQ1=0x%02x)",
			pa1, target, seq1);
		(void)max86141_write_reg(dev, MAX86141_REG_LED_SEQ1,
					 MAX86141_LED_SEQ1_PRODUCT);
		(void)max86141_write_reg(dev, MAX86141_REG_LED_SEQ2,
					 MAX86141_LED_SEQ2_PRODUCT);
		(void)max86141_write_reg(dev, MAX86141_REG_LED_SEQ3,
					 MAX86141_LED_SEQ3_PRODUCT);
		(void)max86141_write_reg(dev, MAX86141_REG_LED_RANGE1,
					 MAX86141_LED_RANGE1_62MA);
		(void)max86141_write_reg(dev, MAX86141_REG_PD_CONFIG,
					 MAX86141_PD_CONFIG_SFH7074_DUAL);
		(void)max86141_write_reg(dev, MAX86141_REG_LED2_DRV_CUR, target);
		(void)max86141_write_reg(dev, MAX86141_REG_LED3_DRV_CUR, target);
		(void)max86141_write_reg(dev, MAX86141_REG_SYSTEM_CTRL, 0x00);
		return;
	}

	if (sys != 0x00) {
		LOG_WRN("HUB: MAX86141 SYSTEM_CTRL=0x%02x  clearing to exit shutdown", sys);
		(void)max86141_write_reg(dev, MAX86141_REG_SYSTEM_CTRL, 0x00);
	}
}

int max32664_runtime_hub_start_sampling(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	int ret;

	if (data->sampling) {
		LOG_DBG("Hub already sampling");
		return 0;
	}

	LOG_INF("HUB mode: starting sampling (%u ms poll)",
		data->config.poll_interval_ms);

	max32664_hub_ensure_afe_ppg_running(dev);

	ret = max32664_runtime_request_hub_output_mode(dev);
	if (ret) {
		LOG_ERR("HUB mode: failed to set output mode for sampling: %d", ret);
		return ret;
	}

	max32664_hub_ensure_afe_ppg_running(dev);

#if IS_ENABLED(CONFIG_MAX32664_HUB_OUTPUT_MODE_ALGO_PLUS_SENSOR)
	data->hub_mode03_fallback_done = false;
#endif

	k_sem_reset(&data->sample_sem);

	data->sampling = true;
	k_work_schedule(&data->poll_work, K_NO_WAIT);

#if IS_ENABLED(CONFIG_MAX32664_LOG_AFE_LED_ON_SAMPLING_START)
	max32664_hub_log_afe_led_state(dev);
#endif
	return 0;
}

int max32664_runtime_hub_stop_sampling(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	int ret;

	if (!data->sampling) {
		return 0;
	}

	LOG_INF("HUB mode: stopping sampling (PAUSE)");

	data->sampling = false;
	(void)k_work_cancel_delayable(&data->poll_work);

	ret = max32664_runtime_try_output_mode(dev, 0x00, 0x00);
	if (ret) {
		LOG_WRN("HUB mode: PAUSE command failed: %d (continuing)", ret);
	}

	if (data->variant == MAX32664_VARIANT_B || data->variant == MAX32664_VARIANT_C) {
		int afe_ret = max86141_write_reg(dev, MAX86141_REG_SYSTEM_CTRL, 0x02);

		if (afe_ret != 0) {
			LOG_WRN("HUB stop: MAX86141 shutdown (0x02) failed: %d  LEDs may stay on",
				afe_ret);
		} else {
			LOG_DBG("HUB stop: MAX86141 in shutdown  LEDs off");
		}
	}

	return 0;
}
#endif
