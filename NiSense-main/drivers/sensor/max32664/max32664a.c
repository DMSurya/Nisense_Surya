/**
 * @file max32664a.c
 * @brief MAX32664 Variant A (MaximFast) implementation
 *
 * Variant A Details:
 * - Algorithm: MaximFast (Fast HR + SpO2)
 * - AFE: MAX30101 (single-LED PPG optimized for finger)
 * - Accelerometer: None required
 * - Sample Rate: 100Hz (high speed)
 * - FIFO Entry Size: 11 bytes
 * - AGC: Auto Gain Control for optimal signal
 */

#include "max32664_priv.h"

LOG_MODULE_DECLARE(max32664, CONFIG_SENSOR_LOG_LEVEL);

static int max32664a_init(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	uint8_t tx[8], rx[8];
	uint8_t dump_buf[33];
	int ret;
	
	LOG_INF("Initializing Variant A (MaximFast - MAX30101 + AGC)...");
	
	/* 1. Enable MAX30101 AFE */
	if (data->afe_pre_enabled) {
		LOG_INF("MAX30101 AFE already enabled by firmware v%u.%u.%u - skipping enable command",
			(data->firmware_version >> 16) & 0xFF,
			(data->firmware_version >> 8) & 0xFF,
			data->firmware_version & 0xFF);
	} else {
		tx[0] = 0x44; /* Family: Enable Sensor */
		tx[1] = 0x03; /* Index: MAX30101 (AFE) */
		tx[2] = 0x01; /* Enable */
		ret = max32664_i2c_transmit_retry(dev, tx, 3, rx, 1, 250, 3);
		if (ret || (rx[0] != 0x00 && rx[0] != 0xFE)) {
			LOG_ERR("Failed to enable MAX30101 AFE: %d (status=0x%02x)", ret, rx[0]);
			return ret;
		}
		if (rx[0] == 0xFE) {
			LOG_INF("MAX30101 AFE pre-enabled by firmware (status=0x%02x) - ready", rx[0]);
			data->afe_pre_enabled = true;
		} else {
			LOG_INF("MAX30101 AFE enabled, status=0x%02x", rx[0]);
		}
	}
	k_msleep(50);
	
	/* 2. Dump MAX30101 registers for diagnostics */
	tx[0] = 0x43; /* Family: Dump Registers */
	tx[1] = 0x03; /* Index: MAX30101 */
	ret = max32664_i2c_transmit(dev, tx, 2, dump_buf, 33, 50);
	if (ret == 0 && dump_buf[0] == 0x00) {
		LOG_HEXDUMP_INF(&dump_buf[1], 32, "MAX30101 Register Dump:");
	} else if ((ret == -EINVAL) && dump_buf[0] == 0xFE) {
		LOG_DBG("Register dump not available (status=0x%02x) - non-critical", dump_buf[0]);
	} else {
		LOG_WRN("Failed to dump MAX30101 registers: %d (status=0x%02x)", ret, dump_buf[0]);
	}
	
	/* 3. Enable MaximFast algorithm */
	if (data->afe_pre_enabled) {
		LOG_INF("MaximFast algorithm already enabled by firmware - skipping enable command");
	} else {
		LOG_INF("Enabling MaximFast algorithm...");
		tx[0] = 0x52; /* Family: Algorithm Enable */
		tx[1] = 0x00; /* Index: MaximFast */
		tx[2] = 0x01; /* Enable */
		ret = max32664_i2c_transmit_retry(dev, tx, 3, rx, 1, 500, 3);
		if (ret == 0 && rx[0] == 0x00) {
			LOG_INF("MaximFast algorithm enabled, status=0x%02x", rx[0]);
		} else if ((ret == -EINVAL) && (rx[0] == 0x02 || rx[0] == 0xFE)) {
			LOG_INF("MaximFast algorithm pre-enabled by firmware (status=0x%02x) - ready", rx[0]);
		} else {
			LOG_ERR("Failed to enable MaximFast algorithm: %d (status=0x%02x)", ret, rx[0]);
			return ret ? ret : -EIO;
		}
	}
	
	/* 4. Configure AGC parameters for optimal signal quality */
	LOG_INF("Configuring AGC for variant A (MaximFast)...");
	ret = max32664_configure_agc(dev, 75, 10, 50, 10);
	if (ret) {
		LOG_WRN("Failed to configure AGC: %d - using firmware defaults", ret);
	}
	
	k_msleep(500);
	return 0;
}

/**
 * @brief Parse Variant A FIFO data (MaximFast algorithm)
 *
 * MaximFast FIFO Format (11 bytes per sample):
 * - Byte 0: Status (0x00=calculating, 0x01=valid, 0xFF=error)
 * - Byte 1: HR (beats per minute)
 * - Byte 2: HR confidence (0-100%)
 * - Byte 3: SpO2 (0-100%)
 * - Byte 4: SpO2 confidence (0-100%)
 * - Bytes 5-7: IR LED raw value (24-bit, LSB first)
 * - Bytes 8-10: Red LED raw value (24-bit, LSB first)
 *
 * @param dev Device structure
 * @param fifo_data Raw FIFO data buffer (includes status byte at [0])
 * @param len Length of data (should be 12: 1 status + 11 algo data)
 * @return 0 on success, negative errno on failure
 */
static int max32664a_parse_fifo(const struct device *dev, const uint8_t *fifo_data, size_t len)
{
	struct max32664_data *data = dev->data;

	/* MaximFast format: 1 cmd status + 11 bytes algo data = 12 bytes min */
	if (len < 12) {
		LOG_ERR("Invalid FIFO entry size: %zu (expected >= 12)", len);
		return -EINVAL;
	}

	/* Parse command status (byte 0) and algo status (byte 1) */
	uint8_t cmd_status = fifo_data[0];
	uint8_t algo_status = fifo_data[1];
	
	if (cmd_status != 0x00) {
		LOG_WRN("MaximFast FIFO read error: cmd_status=0x%02x", cmd_status);
		return -EIO;
	}

	/* Parse Heart Rate (byte 2) */
	uint8_t hr = fifo_data[2];
	uint8_t hr_confidence = fifo_data[3];

	/* Parse SpO2 (byte 4) */
	uint8_t spo2 = fifo_data[4];
	uint8_t spo2_confidence = fifo_data[5];

	/* Parse PPG raw values (24-bit, LSB first) */
	uint32_t ir = fifo_data[6] | ((uint32_t)fifo_data[7] << 8) | ((uint32_t)fifo_data[8] << 16);
	uint32_t red = fifo_data[9] | ((uint32_t)fifo_data[10] << 8) | ((uint32_t)fifo_data[11] << 16);

	/* Update data structure with all parsed values */
	data->sample.heart_rate = hr * 10;  /* Store as 0.1 BPM for consistency */
	data->sample.hr_confidence = hr_confidence;
	data->sample.spo2 = (float)spo2;
	data->sample.spo2_confidence = spo2_confidence;
	data->sample.signal_quality = (hr_confidence + spo2_confidence) / 2;
	data->sample.ppg_ir = ir;
	data->sample.ppg_red = red;

	/* Log based on algorithm status */
	switch (algo_status) {
	case 0x00:
		LOG_DBG("MaximFast: Calculating... HR=%u, SpO2=%u%%, IR=%u", hr, spo2, ir);
		break;
	case 0x01:
		LOG_INF("MaximFast: VALID - HR=%u BPM (conf=%u%%), SpO2=%u%% (conf=%u%%)",
			hr, hr_confidence, spo2, spo2_confidence);
		LOG_DBG("PPG: IR=%u, Red=%u", ir, red);
		break;
	case 0x03:
		LOG_DBG("MaximFast: No finger detected");
		break;
	case 0xFF:
		LOG_DBG("MaximFast: No data available");
		return -ENODATA;
	default:
		LOG_DBG("MaximFast: Status=0x%02x, HR=%u, SpO2=%u%%", algo_status, hr, spo2);
		break;
	}

	return 0;
}

const struct max32664_variant_ops max32664a_ops = {
	.init = max32664a_init,
	.parse_fifo = max32664a_parse_fifo,
	.fifo_entry_size = 12,  /* 1 status + 11 algo data */
	.name = "Variant A (MaximFast)",
};
