/**
 * @file max32664d.c
 * @brief MAX32664 Variant D (BPT) implementation
 *
 * Variant D Details:
 * - Algorithm: BPT (Blood Pressure Trending)
 * - AFE: MAX30101
 * - Accelerometer: None required
 * - Sample Rate: 100Hz
 * - FIFO Entry Size: 16 bytes (BPT v3.x format)
 * - Calibration: REQUIRED before use (reference BP measurement)
 * - AGC: Auto Gain Control for optimal signal
 *
 * BPT FIFO Format (16 bytes per sample):
 * - Byte 0: Status (0x00=calculating, 0x01=complete, 0x02=progress)
 * - Byte 1: HR (beats per minute)
 * - Byte 2: HR confidence (0-100%)
 * - Byte 3: SpO2 (0-100%)
 * - Byte 4: SpO2 confidence (0-100%)
 * - Byte 5-6: Systolic BP (mmHg, MSB first if >8-bit)
 * - Byte 7-8: Diastolic BP (mmHg, MSB first if >8-bit)
 * - Byte 9: BP confidence (0-100%)
 * - Byte 10: BP progress (0-100%, during calibration)
 * - Bytes 11-15: Reserved/padding
 */

#include "max32664_priv.h"

LOG_MODULE_DECLARE(max32664, CONFIG_SENSOR_LOG_LEVEL);

static int max32664d_init(const struct device *dev)
{
	struct max32664_data *data = dev->data;
	uint8_t tx[8], rx[8];
	uint8_t dump_buf[33];
	int ret;
	
	LOG_INF("Initializing Variant D (BPT - MAX30101 + AGC + Calibration)...");
	
	/* 1. Enable MAX30101 AFE */
	if (data->afe_pre_enabled) {
		LOG_INF("MAX30101 AFE already enabled by firmware - skipping");
	} else {
		tx[0] = 0x44;
		tx[1] = 0x03;
		tx[2] = 0x01;
		ret = max32664_i2c_transmit_retry(dev, tx, 3, rx, 1, 250, 3);
		if (ret || (rx[0] != 0x00 && rx[0] != 0xFE)) {
			LOG_ERR("Failed to enable MAX30101 AFE: %d (status=0x%02x)", ret, rx[0]);
			return ret;
		}
		if (rx[0] == 0xFE) {
			data->afe_pre_enabled = true;
		}
		LOG_INF("MAX30101 AFE enabled, status=0x%02x", rx[0]);
	}
	k_msleep(50);
	
	/* 2. Dump MAX30101 registers */
	tx[0] = 0x43;
	tx[1] = 0x03;
	ret = max32664_i2c_transmit(dev, tx, 2, dump_buf, 33, 50);
	if (ret == 0 && dump_buf[0] == 0x00) {
		LOG_HEXDUMP_INF(&dump_buf[1], 32, "MAX30101 Register Dump:");
	}
	
	/* 3. Enable BPT algorithm */
	if (data->afe_pre_enabled) {
		LOG_INF("BPT algorithm already enabled by firmware - skipping");
	} else {
		LOG_INF("Enabling BPT algorithm...");
		tx[0] = 0x52;
		tx[1] = 0x02;
		tx[2] = 0x01;
		ret = max32664_i2c_transmit_retry(dev, tx, 3, rx, 1, 500, 3);
		if (ret == 0 && rx[0] == 0x00) {
			LOG_INF("BPT algorithm enabled, status=0x%02x", rx[0]);
		} else if ((ret == -EINVAL) && (rx[0] == 0x02 || rx[0] == 0xFE)) {
			LOG_INF("BPT algorithm pre-enabled (status=0x%02x)", rx[0]);
		} else {
			LOG_ERR("Failed to enable BPT algorithm: %d (status=0x%02x)", ret, rx[0]);
			return ret ? ret : -EIO;
		}
	}
	
	/* 4. Configure AGC (more aggressive for BP accuracy) */
	LOG_INF("Configuring AGC for variant D (BPT)...");
	ret = max32664_configure_agc(dev, 70, 12, 50, 10);
	if (ret) {
		LOG_WRN("Failed to configure AGC: %d - using firmware defaults", ret);
	}
	
	/* 5. BPT calibration reminder */
	if (!data->calibration_done) {
		LOG_WRN("═══ BPT CALIBRATION REQUIRED ═══");
		LOG_WRN("Blood Pressure readings are INVALID until user calibration");
	}
	
	k_msleep(500);
	return 0;
}

/**
 * @brief Parse Variant D FIFO data (BPT algorithm)
 *
 * BPT FIFO Format varies by firmware version. Common format (16 bytes):
 * - Byte 0: Status (0x00=calculating, 0x01=complete, 0x02=progress, 0xFF=error)
 * - Byte 1: HR (beats per minute, 0-255 BPM)
 * - Byte 2: HR confidence (0-100%)
 * - Byte 3: SpO2 (0-100%)
 * - Byte 4: SpO2 confidence (0-100%)
 * - Byte 5: Systolic BP (mmHg, low byte)
 * - Byte 6: Systolic BP (mmHg, high byte) - typically 0 for normal BP
 * - Byte 7: Diastolic BP (mmHg, low byte)
 * - Byte 8: Diastolic BP (mmHg, high byte) - typically 0 for normal BP
 * - Byte 9: BP confidence (0-100%)
 * - Byte 10: BP progress (0-100%, during calibration/measurement)
 * - Bytes 11-15: Reserved/padding or extended data
 *
 * Status values:
 * - 0x00: Algorithm initializing/calculating
 * - 0x01: Valid measurement complete
 * - 0x02: Measurement in progress (check progress byte)
 * - 0x03: Finger not detected
 * - 0x04: Motion detected (hold still)
 * - 0xFF: Error or no data
 *
 * @param dev Device structure
 * @param fifo_data Raw FIFO data buffer (includes status byte at [0])
 * @param len Length of data
 * @return 0 on success, negative errno on failure
 */
static int max32664d_parse_fifo(const struct device *dev, const uint8_t *fifo_data, size_t len)
{
	struct max32664_data *data = dev->data;

	/* BPT format: min 11 bytes (1 status + 10 data) */
	if (len < 11) {
		LOG_ERR("Invalid FIFO entry size: %zu (expected >= 11)", len);
		return -EINVAL;
	}

	/* Parse status byte (byte 0 is I2C command status, byte 1 is BPT status) */
	uint8_t cmd_status = fifo_data[0];
	uint8_t bpt_status = fifo_data[1];
	
	if (cmd_status != 0x00) {
		LOG_WRN("BPT FIFO read error: cmd_status=0x%02x", cmd_status);
		return -EIO;
	}

	/* Parse Heart Rate (byte 2) */
	uint8_t hr = fifo_data[2];
	uint8_t hr_confidence = fifo_data[3];

	/* Parse SpO2 (byte 4) */
	uint8_t spo2 = fifo_data[4];
	uint8_t spo2_confidence = fifo_data[5];

	/* Parse Blood Pressure (16-bit, LSB first) */
	uint16_t systolic = fifo_data[6] | ((uint16_t)fifo_data[7] << 8);
	uint16_t diastolic = fifo_data[8] | ((uint16_t)fifo_data[9] << 8);
	uint8_t bp_confidence = fifo_data[10];
	uint8_t bp_progress = (len > 11) ? fifo_data[11] : 0;

	/* Sanity check values */
	if (systolic > 300 || diastolic > 200) {
		/* Values out of physiological range - likely parsing error or no data */
		LOG_DBG("BPT: Values out of range (sys=%u, dia=%u) - status=0x%02x",
			systolic, diastolic, bpt_status);
	}

	/* Update data structure with all parsed values */
	data->sample.heart_rate = hr * 10;  /* Store as 0.1 BPM for consistency */
	data->sample.hr_confidence = hr_confidence;
	data->sample.spo2 = (float)spo2;
	data->sample.spo2_confidence = spo2_confidence;
	data->sample.systolic_bp = systolic;
	data->sample.diastolic_bp = diastolic;
	data->sample.bp_confidence = bp_confidence;
	data->sample.bp_progress = bp_progress;
	data->sample.signal_quality = (hr_confidence + spo2_confidence + bp_confidence) / 3;

	/* Log based on BPT status */
	switch (bpt_status) {
	case 0x00:
		LOG_DBG("BPT: Initializing (HR=%u, SpO2=%u%%, progress=%u%%)",
			hr, spo2, bp_progress);
		break;
	case 0x01:
		LOG_INF("BPT: VALID - BP=%u/%u mmHg, HR=%u BPM, SpO2=%u%% (conf=%u%%)",
			systolic, diastolic, hr, spo2, bp_confidence);
		break;
	case 0x02:
		LOG_DBG("BPT: In progress (%u%%) - HR=%u, SpO2=%u%%",
			bp_progress, hr, spo2);
		break;
	case 0x03:
		LOG_DBG("BPT: No finger detected");
		break;
	case 0x04:
		LOG_DBG("BPT: Motion detected - hold still");
		break;
	case 0xFF:
		LOG_DBG("BPT: No data available");
		return -ENODATA;
	default:
		LOG_DBG("BPT: Unknown status 0x%02x (HR=%u, SpO2=%u%%)", bpt_status, hr, spo2);
		break;
	}

	return 0;
}

const struct max32664_variant_ops max32664d_ops = {
	.init = max32664d_init,
	.parse_fifo = max32664d_parse_fifo,
	.fifo_entry_size = 16,  /* BPT v3.x uses 16 bytes */
	.name = "Variant D (BPT)",
};
