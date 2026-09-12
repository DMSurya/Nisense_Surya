/**
 * @file max32664_protocol.c
 * @brief MAX32664 register/FIFO/status/algorithm protocol helpers
 */

#include "max32664_priv.h"

LOG_MODULE_DECLARE(max32664, CONFIG_SENSOR_LOG_LEVEL);

int max32664_read_sensor_reg(const struct device *dev,
			     uint8_t sensor_idx,
			     uint8_t reg_addr,
			     uint8_t *reg_value)
{
	struct max32664_data *data = dev->data;
	uint8_t tx[3] = {0x41, sensor_idx, reg_addr};
	uint8_t rx[2];
	int ret;

	uint8_t fw_major = (data->firmware_version >> 16) & 0xFF;
	uint8_t max_retries = (fw_major >= 40) ? 5 : 3;
	uint16_t delay_ms = (fw_major >= 40) ? 50 : 10;

	ret = max32664_i2c_transmit_retry(dev, tx, sizeof(tx), rx, sizeof(rx), delay_ms, max_retries);
	if (ret) {
		return ret;
	}

	*reg_value = rx[1];
	return 0;
}

int max32664_write_sensor_reg(const struct device *dev,
			      uint8_t sensor_idx,
			      uint8_t reg_addr,
			      uint8_t reg_value)
{
	struct max32664_data *data = dev->data;
	uint8_t tx[4] = {0x40, sensor_idx, reg_addr, reg_value};
	uint8_t rx[1];
	int ret;

	uint8_t fw_major = (data->firmware_version >> 16) & 0xFF;
	uint8_t max_retries = (fw_major >= 40) ? 5 : 3;
	uint16_t delay_ms = (fw_major >= 40) ? 50 : 10;

	ret = max32664_i2c_transmit_retry(dev, tx, sizeof(tx), rx, sizeof(rx), delay_ms, max_retries);
	if (ret) {
		LOG_ERR("Write sensor reg 0x%02X failed: %d", reg_addr, ret);
		return ret;
	}

	if (rx[0] != 0x00) {
		LOG_ERR("Write sensor reg 0x%02X status error: 0x%02X", reg_addr, rx[0]);
		return -EIO;
	}

	return 0;
}

int max32664_feed_accel(const struct device *dev,
			int16_t x, int16_t y, int16_t z)
{
	const struct max32664_config *config = dev->config;
	uint8_t tx[9];
	uint8_t rx[1];
	int ret;

	if (!config->accel_external) {
		return -ENOTSUP;
	}

	tx[0] = 0x14;
	tx[1] = 0x00;
	tx[2] = 0x04;
	tx[3] = (uint8_t)(x & 0xFF);
	tx[4] = (uint8_t)((x >> 8) & 0xFF);
	tx[5] = (uint8_t)(y & 0xFF);
	tx[6] = (uint8_t)((y >> 8) & 0xFF);
	tx[7] = (uint8_t)(z & 0xFF);
	tx[8] = (uint8_t)((z >> 8) & 0xFF);

	ret = max32664_i2c_transmit_retry(dev, tx, 9, rx, 1, 50, 3);
	if (ret) {
		return ret;
	}
	if (rx[0] != MAX32664_STATUS_SUCCESS) {
		return -EIO;
	}

	return 0;
}

int max32664_read_fifo_burst(const struct device *dev,
			     uint8_t sensor_idx,
			     uint8_t fifo_reg,
			     uint8_t *out_buf,
			     uint16_t num_bytes)
{
	const struct max32664_config *cfg = dev->config;
	bool mfio_held = false;
	int ret = 0;

	if (num_bytes == 0) {
		return 0;
	}

	/*
	 * AN6924 Normal MFIO: hold MFIO low across the entire burst
	 * (write+delay+read per byte). Prior code pulsed then released
	 * before I2C — that violates the hold-low contract.
	 */
	max32664_bus_lock(dev);
	(void)max32664_mfio_txn_begin(dev, &mfio_held);

	uint8_t tx[3] = {0x41, sensor_idx, fifo_reg};
	uint8_t rx[2];

	for (uint16_t i = 0; i < num_bytes; i++) {
		ret = i2c_write_dt(&cfg->bus, tx, sizeof(tx));
		if (ret) {
			LOG_ERR("FIFO burst write failed at byte %u: %d", i, ret);
			ret = -EBUSY;
			goto out;
		}

		k_usleep(1500);

		ret = i2c_read_dt(&cfg->bus, rx, sizeof(rx));
		if (ret) {
			LOG_ERR("FIFO burst read failed at byte %u: %d", i, ret);
			ret = -EBUSY;
			goto out;
		}

		if (rx[0] != 0x00) {
			if (rx[0] == MAX32664_STATUS_ERR_TRY_AGAIN) {
				k_usleep(5000);
				ret = i2c_write_dt(&cfg->bus, tx, sizeof(tx));
				if (ret) {
					ret = -EBUSY;
					goto out;
				}
				k_usleep(3000);
				ret = i2c_read_dt(&cfg->bus, rx, sizeof(rx));
				if (ret || rx[0] != 0x00) {
					LOG_ERR("FIFO burst retry failed at byte %u: status=0x%02X",
						i, rx[0]);
					ret = -EIO;
					goto out;
				}
			} else {
				LOG_ERR("FIFO burst error at byte %u: status=0x%02X", i, rx[0]);
				ret = -EIO;
				goto out;
			}
		}

		out_buf[i] = rx[1];
	}

	ret = 0;
out:
	max32664_mfio_txn_end(dev, mfio_held);
	max32664_bus_unlock(dev);
	return ret;
}

int max32664_get_hub_status(const struct device *dev, uint8_t *status)
{
	uint8_t tx[2] = {0x00, 0x00};
	uint8_t rx[2];
	int ret;

	ret = max32664_i2c_transmit_retry(dev, tx, sizeof(tx), rx, sizeof(rx), 10, 3);
	if (ret) {
		return ret;
	}

	*status = rx[1];
	return 0;
}

int max32664_get_fifo_count(const struct device *dev, uint8_t *count)
{
	uint8_t tx[2] = {0x12, 0x00};
	uint8_t rx[2];
	int ret;

	ret = max32664_i2c_transmit_retry(dev, tx, sizeof(tx), rx, sizeof(rx), 10, 3);
	if (ret) {
		return ret;
	}

	*count = rx[1];
	return 0;
}

void max32664_parse_status(uint8_t status,
			   bool *err0,
			   bool *data_rdy,
			   bool *fifo_out_ovr,
			   bool *fifo_in_ovr,
			   bool *host_accel_uf)
{
	if (err0) {
		*err0 = (status & MAX32664_STATUS_ERR0) != 0;
	}
	if (data_rdy) {
		*data_rdy = (status & MAX32664_STATUS_DATA_RDY) != 0;
	}
	if (fifo_out_ovr) {
		*fifo_out_ovr = (status & MAX32664_STATUS_FIFO_OUT_OVR) != 0;
	}
	if (fifo_in_ovr) {
		*fifo_in_ovr = (status & MAX32664_STATUS_FIFO_IN_OVR) != 0;
	}
	if (host_accel_uf) {
		*host_accel_uf = (status & MAX32664_STATUS_HOST_ACCEL_UF) != 0;
	}
}

int max32664_configure_agc(const struct device *dev,
			   uint8_t target_percent,
			   uint8_t step_size,
			   uint8_t sensitivity,
			   uint8_t samples_avg)
{
	struct max32664_data *data = dev->data;
	uint8_t tx[4];
	uint8_t rx[1];
	int ret;

	LOG_INF("Configuring AGC: target=%u%%, step=%u%%, sens=%u%%, avg=%u samples",
		target_percent, step_size, sensitivity, samples_avg);

	tx[0] = 0x50;
	tx[1] = 0x00;
	tx[2] = MAX32664_AGC_PARAM_TARGET;
	tx[3] = target_percent;
	ret = max32664_i2c_transmit_retry(dev, tx, 4, rx, 1, 10, 3);
	if (ret) {
		LOG_ERR("Failed to set AGC target: %d", ret);
		return ret;
	}
	k_msleep(10);

	tx[2] = MAX32664_AGC_PARAM_STEP_SIZE;
	tx[3] = step_size;
	ret = max32664_i2c_transmit_retry(dev, tx, 4, rx, 1, 10, 3);
	if (ret) {
		LOG_ERR("Failed to set AGC step size: %d", ret);
		return ret;
	}
	k_msleep(10);

	tx[2] = MAX32664_AGC_PARAM_SENSITIVITY;
	tx[3] = sensitivity;
	ret = max32664_i2c_transmit_retry(dev, tx, 4, rx, 1, 10, 3);
	if (ret) {
		LOG_ERR("Failed to set AGC sensitivity: %d", ret);
		return ret;
	}
	k_msleep(10);

	tx[2] = MAX32664_AGC_PARAM_SAMPLES_AVG;
	tx[3] = samples_avg;
	ret = max32664_i2c_transmit_retry(dev, tx, 4, rx, 1, 10, 3);
	if (ret) {
		LOG_ERR("Failed to set AGC samples avg: %d", ret);
		return ret;
	}

	data->agc.target_percent = target_percent;
	data->agc.step_size = step_size;
	data->agc.sensitivity = sensitivity;
	data->agc.samples_avg = samples_avg;
	data->agc.configured = true;

	LOG_INF("AGC configuration complete");
	return 0;
}

int max32664_configure_whrm_mode(const struct device *dev, uint8_t mode)
{
	struct max32664_data *data = dev->data;
	uint8_t tx[4];
	uint8_t rx[1];
	int ret;

	if (mode >= MAX32664_WHRM_MODE_STANDARD && !data->accel_enabled) {
		LOG_ERR("WHRM mode %u requires accelerometer (not available)", mode);
		return -ENOTSUP;
	}

	LOG_INF("Setting WHRM AEC mode to 0x%02x", mode);

	tx[0] = 0x50;
	tx[1] = 0x02;
	tx[2] = 0x0B;
	tx[3] = (mode > 0) ? 0x01 : 0x00;

	ret = max32664_i2c_transmit_retry(dev, tx, 4, rx, 1, 100, 3);
	if (ret == 0 && rx[0] == 0x00) {
		LOG_INF("WHRM AEC configured: 0x%02x", tx[3]);
	} else if (rx[0] == 0x02) {
		LOG_INF("WHRM AEC pre-configured by firmware (status=0x%02x)", rx[0]);
		ret = 0;
	} else {
		LOG_WRN("WHRM AEC config returned status=0x%02x (ret=%d) - continuing", rx[0], ret);
		ret = 0;
	}

	return ret;
}
