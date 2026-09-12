/**
 * @file max302xx.c
 * @brief Unified Body Sensor Driver for MAX302xx Family
 *
 * Auto-detects and supports MAX30205, MAX30208, or MAX30210 at runtime.
 * Only one sensor is present on the PCB - driver probes Part ID at init.
 *
 * MAX30205 Features (Part ID: None, Config readable):
 *   - Accuracy: +/-0.1C, Range: -40C to +105C, Resolution: 0.00390625C (13-bit)
 *   - No FIFO, no GPIO, continuous conversion only
 *   - Lowest power, simplest variant
 *
 * MAX30208 Features (Part ID: 0x08 or 0x30):
 *   - Accuracy: +/-0.1C, Range: -40C to +105C, Resolution: 0.005C (16-bit)
 *   - 32-sample FIFO (2-byte burst read per sample)
 *   - GPIO interrupt support for async temperature ready
 *   - Unique device ID (6 bytes at 0x31-0x36)
 *   - Note: Some variants report Part ID 0x30 instead of 0x08
 *
 * MAX30210 Features (Part ID: 0x45):
 *   - Accuracy: +/-0.1C, Range: -40C to +105C, Resolution: 0.005C (16-bit)
 *   - 64-sample FIFO (3-byte burst: TAG + Temperature MSB/LSB)
 *   - Autonomous conversion mode with programmable rate
 *   - Temperature slope detection (rate-of-change alerts)
 *   - Enhanced interrupt modes
 *   - Unique device ID (6 bytes at 0x30-0x35)
 *
 * Detection Method:
 *   1. Read Part Identifier register (0xFF)
 *   2. 0x08 or 0x30 = MAX30208, 0x45 = MAX30210
 *   3. If neither, try reading config register (0x01) for MAX30205
 *   4. Initialize sensor-specific features based on detected type
 *
 * Operating Modes:
 *   MAX30205: Continuous conversion only
 *   MAX30208: Sync (STATUS polling) or Async (GPIO interrupt)
 *   MAX30210: Sync, Async, or Autonomous (self-triggered at programmed rate)
 *
 * I2C Address Latching:
 *   Address sampled at power-on reset (POR) from pin states.
 *   Base: 0x50, Address = 0x50 + (GPIO1<<1) + GPIO0
 *   Software reset does NOT re-latch address (only hardware POR does).
 *   For battery-powered systems, address remains stable across reboots.
 */

#define DT_DRV_COMPAT maxim_max30208 /* Device tree compatible - keep original for DT binding */

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(max302xx, CONFIG_MAX302XX_LOG_LEVEL);

/* Custom sensor attributes */
enum max302xx_attribute {
	SENSOR_ATTR_MAX302XX_UNIQUE_ID = SENSOR_ATTR_PRIV_START,
};

/* Common register map (MAX30208/MAX30210 share most) */
#define MAX302XX_REG_STATUS 0x00		 /* Interrupt status */
#define MAX302XX_REG_INT_ENABLE 0x01	 /* Interrupt enable (0x02 for MAX30210) */
#define MAX302XX_REG_FIFO_WR_PTR 0x04	 /* FIFO write pointer */
#define MAX302XX_REG_FIFO_RD_PTR 0x05	 /* FIFO read pointer */
#define MAX302XX_REG_FIFO_OVF_CNT 0x06	 /* FIFO overflow counter */
#define MAX302XX_REG_FIFO_DATA_CNT 0x07	 /* FIFO data counter */
#define MAX302XX_REG_FIFO_DATA 0x08		 /* FIFO data */
#define MAX302XX_REG_FIFO_CFG1 0x09		 /* FIFO configuration 1 */
#define MAX302XX_REG_FIFO_CFG2 0x0A		 /* FIFO configuration 2 */
#define MAX302XX_REG_SYS_CTRL 0x0C		 /* System control (MAX30208) */
#define MAX302XX_REG_SYSTEM_CONFIG 0x11	 /* System config (MAX30210) */
#define MAX302XX_REG_PIN_CONFIG 0x12	 /* Pin config (MAX30210) */
#define MAX302XX_REG_ALARM_HIGH_MSB 0x10 /* Alarm high MSB (MAX30208) */
#define MAX302XX_REG_ALARM_HIGH_LSB 0x11 /* Alarm high LSB (MAX30208) */
#define MAX302XX_REG_ALARM_LOW_MSB 0x12	 /* Alarm low MSB (MAX30208) */
#define MAX302XX_REG_ALARM_LOW_LSB 0x13	 /* Alarm low LSB (MAX30208) */
#define MAX302XX_REG_TEMP_SETUP 0x14	 /* Temperature setup (MAX30208) */
#define MAX302XX_REG_TEMP_DATA_MSB 0x15	 /* Temperature data MSB */
#define MAX302XX_REG_TEMP_DATA_LSB 0x16	 /* Temperature data LSB */
#define MAX302XX_REG_GPIO_SETUP 0x20	 /* GPIO setup */
#define MAX302XX_REG_GPIO_CTRL 0x21		 /* GPIO control */

/* MAX30210 specific registers */
#define MAX30210_REG_INT_ENABLE 0x02	  /* Interrupt enable */
#define MAX30210_REG_ALARM_HI_SETUP 0x20  /* High alarm setup */
#define MAX30210_REG_ALARM_LO_SETUP 0x21  /* Low alarm setup */
#define MAX30210_REG_ALARM_HI_MSB 0x22	  /* High alarm MSB */
#define MAX30210_REG_ALARM_HI_LSB 0x23	  /* High alarm LSB */
#define MAX30210_REG_ALARM_LO_MSB 0x24	  /* Low alarm MSB */
#define MAX30210_REG_ALARM_LO_LSB 0x25	  /* Low alarm LSB */
#define MAX30210_REG_TEMP_INC_THRESH 0x26 /* Temperature increase threshold */
#define MAX30210_REG_TEMP_DEC_THRESH 0x27 /* Temperature decrease threshold */
#define MAX30210_REG_TEMP_CONFIG1 0x28	  /* Temp config 1 (slope detect) */
#define MAX30210_REG_TEMP_CONFIG2 0x29	  /* Temp config 2 (auto mode) */
#define MAX30210_REG_TEMP_CONTROL 0x2A	  /* Temp control (AUTO/CONVERT_T) */
#define MAX30210_REG_TEMP_SLOPE_MSB 0x2D  /* Temperature slope MSB */
#define MAX30210_REG_TEMP_SLOPE_LSB 0x2E  /* Temperature slope LSB */

/* MAX30208 unique ID registers */
#define MAX302XX_REG_PART_ID1 0x31 /* Part ID 1 (unique ID) */
#define MAX302XX_REG_PART_ID2 0x32 /* Part ID 2 (unique ID) */
#define MAX302XX_REG_PART_ID3 0x33 /* Part ID 3 (unique ID) */
#define MAX302XX_REG_PART_ID4 0x34 /* Part ID 4 (unique ID) */
#define MAX302XX_REG_PART_ID5 0x35 /* Part ID 5 (unique ID) */
#define MAX302XX_REG_PART_ID6 0x36 /* Part ID 6 (unique ID) */

/* MAX30210 unique ID registers (different addresses) */
#define MAX30210_REG_UNIQUE_ID1 0x30 /* Unique ID 1 */
#define MAX30210_REG_UNIQUE_ID2 0x31 /* Unique ID 2 */
#define MAX30210_REG_UNIQUE_ID3 0x32 /* Unique ID 3 */
#define MAX30210_REG_UNIQUE_ID4 0x33 /* Unique ID 4 */
#define MAX30210_REG_UNIQUE_ID5 0x34 /* Unique ID 5 */
#define MAX30210_REG_UNIQUE_ID6 0x35 /* Unique ID 6 */

/* Part Identifier register */
#define MAX302XX_REG_PART_IDENTIFIER 0xFF /* Part Identifier */
#define MAX30208_PART_ID 0x08			  /* MAX30208 Part ID (standard) */
#define MAX30208_PART_ID_ALT 0x30		  /* MAX30208 Part ID (alternate variant) */
#define MAX30210_PART_ID 0x45			  /* MAX30210 Part ID */
/* Note: MAX30205 has NO Part ID register - detected via fallback */

/* MAX30205 registers (simplified) */
#define MAX30205_TEMP_REG 0x00	 /* Temperature register */
#define MAX30205_CONFIG_REG 0x01 /* Configuration register */

/* Status register bits (common) */
#define MAX302XX_STATUS_TEMP_RDY BIT(0) /* Temperature data ready */
#define MAX302XX_STATUS_TEMP_HI BIT(1)	/* High temperature alarm */
#define MAX302XX_STATUS_TEMP_LO BIT(2)	/* Low temperature alarm */

/* MAX30210 additional status bits */
#define MAX30210_STATUS_TEMP_INC_FAST BIT(4) /* Temperature increasing fast */
#define MAX30210_STATUS_TEMP_DEC_FAST BIT(5) /* Temperature decreasing fast */

/* Interrupt enable bits */
#define MAX302XX_INT_EN_TEMP_RDY BIT(0) /* Temperature ready interrupt */
#define MAX302XX_INT_EN_TEMP_HI BIT(1)	/* High temperature interrupt */
#define MAX302XX_INT_EN_TEMP_LO BIT(2)	/* Low temperature interrupt */

/* Temperature setup register bits (0x14 MAX30208, 0x2A MAX30210) */
#define MAX302XX_TEMP_SETUP_RFU 0xC0	   /* Reserved - must write as 11b */
#define MAX302XX_TEMP_SETUP_CONVERT BIT(0) /* Start temperature conversion */

/* System control register bits */
#define MAX302XX_SYS_CTRL_RESET BIT(0) /* Software reset */

/* MAX30210 TEMP_CONTROL bits */
#define MAX30210_TEMP_CTRL_AUTO BIT(1)	  /* Autonomous mode enable */
#define MAX30210_TEMP_CTRL_CONVERT BIT(0) /* Convert trigger */

/* Temperature conversion resolutions */
#define MAX302XX_TEMP_RESOLUTION 0.005f		 /* 0.005C per LSB (16-bit MAX30208/30210) */
#define MAX30205_TEMP_RESOLUTION 0.00390625f /* 0.00390625C per LSB (13-bit) */

/* FIFO depths */
#define MAX30208_FIFO_DEPTH 32 /* 32 samples */
#define MAX30210_FIFO_DEPTH 64 /* 64 samples */

/* Sensor type enumeration */
enum temp_sensor_type
{
	TEMP_SENSOR_UNKNOWN = 0,
	TEMP_SENSOR_MAX30205, /* Simplest - no FIFO */
	TEMP_SENSOR_MAX30208, /* Advanced - 32 FIFO, 2-byte */
	TEMP_SENSOR_MAX30210  /* Most advanced - 64 FIFO, 3-byte with TAG */
};

struct MAX302XX_config
{
	struct i2c_dt_spec i2c;
#ifdef CONFIG_MAX302XX_TRIGGER
	struct gpio_dt_spec int_gpio;
#endif
	int16_t alarm_high;
	int16_t alarm_low;
	uint8_t fifo_samples; /* Number of samples to read from FIFO (1-32 for MAX30208, 1-64 for MAX30210) */
};

struct MAX302XX_data
{
	int16_t temp_raw;				   /* Raw 16-bit temperature value */
	enum temp_sensor_type sensor_type; /* Detected sensor type */
	uint8_t fifo_depth;				   /* FIFO depth: 0=no FIFO, 32=MAX30208, 64=MAX30210 */
	uint8_t fifo_entry_size;		   /* Bytes per FIFO entry: 2=MAX30208, 3=MAX30210 (TAG+temp) */
	uint8_t unique_id[6];			   /* Unique device ID (MAX30208/MAX30210) */
	bool async_mode;				   /* True = interrupt-driven, False = polling */
	bool initialized;				   /* True after successful init (handles power-on timing) */
#ifdef CONFIG_MAX302XX_TRIGGER
	const struct device *dev;
	struct gpio_callback gpio_cb;
	sensor_trigger_handler_t temp_ready_handler;
	const struct sensor_trigger *temp_ready_trigger;
#if defined(CONFIG_MAX302XX_TRIGGER_OWN_THREAD)
	K_KERNEL_STACK_MEMBER(thread_stack, CONFIG_MAX302XX_THREAD_STACK_SIZE);
	struct k_thread thread;
	struct k_sem sem;
#endif
	struct k_work work;
#endif /* CONFIG_MAX302XX_TRIGGER */
};

/* Forward declarations */
static int MAX302XX_wait_for_ready(const struct device *dev, uint32_t timeout_ms);

/**
 * @brief Read single register
 * @param dev Device instance
 * @param reg Register address
 * @param val Pointer to store read value
 * @return 0 on success, negative errno on failure
 */
static int MAX302XX_reg_read(const struct device *dev, uint8_t reg, uint8_t *val)
{
	const struct MAX302XX_config *config = dev->config;
	return i2c_write_read_dt(&config->i2c, &reg, 1, val, 1);
}

/**
 * @brief Write single register
 * @param dev Device instance
 * @param reg Register address
 * @param val Value to write
 * @return 0 on success, negative errno on failure
 */
static int MAX302XX_reg_write(const struct device *dev, uint8_t reg, uint8_t val)
{
	const struct MAX302XX_config *config = dev->config;
	uint8_t buf[2] = {reg, val};
	return i2c_write_dt(&config->i2c, buf, sizeof(buf));
}

/**
 * @brief Read 16-bit temperature data from FIFO with averaging
 *
 * Implements Step 4-5 from datasheet:
 * - Read FIFO_DATA_COUNT (0x07) to check samples available
 * - Burst read 2 bytes from FIFO_DATA (0x08) for each sample
 * - Average multiple samples for noise reduction
 *
 * FIFO Usage:
 * - FIFO depth: 16 samples (32 bytes)
 * - Each sample: 2 bytes (16-bit signed, big-endian)
 * - Roll-over mode: Oldest data overwritten when FIFO full
 *
 * Multi-sample averaging:
 * - Configurable via device tree (fifo-samples property)
 * - Reduces thermal noise and improves accuracy
 * - Typical: 4-8 samples for balance between speed and accuracy
 *
 * @param dev Device instance
 * @param temp Pointer to store averaged temperature (int16_t)
 * @return 0 on success, negative errno on failure
 */
/**
 * @brief Read temperature data from FIFO (IC-specific handling)
 *
 * MAX30205: Direct temperature register read (no FIFO)
 * MAX30208: 2-byte burst read from FIFO (temperature MSB/LSB)
 * MAX30210: 3-byte burst read from FIFO (TAG + temperature MSB/LSB)
 *
 * @param dev Device instance
 * @param temp Pointer to store averaged temperature
 * @return 0 on success, negative errno on failure
 */
static int MAX302XX_read_temp(const struct device *dev, int16_t *temp)
{
	const struct MAX302XX_config *config = dev->config;
	const struct MAX302XX_data *data = dev->data;
	uint8_t reg;
	int ret;

	/* MAX30205: No FIFO - read temperature register directly */
	if (data->sensor_type == TEMP_SENSOR_MAX30205)
	{
		uint8_t temp_data[2];
		reg = MAX30205_TEMP_REG;
		ret = i2c_write_read_dt(&config->i2c, &reg, 1, temp_data, 2);
		if (ret < 0)
		{
			return ret;
		}
		*temp = (int16_t)((temp_data[0] << 8) | temp_data[1]);
		return 0;
	}

	/* MAX30208/MAX30210: Read from FIFO with averaging */
	uint8_t data_count;
	reg = MAX302XX_REG_FIFO_DATA_CNT;
	ret = i2c_write_read_dt(&config->i2c, &reg, 1, &data_count, 1);
	if (ret < 0)
	{
		LOG_ERR("Failed to read FIFO count: %d", ret);
		return ret;
	}

	if (data_count == 0)
	{
		LOG_WRN("FIFO empty");
		return -ENODATA;
	}

	uint8_t samples_to_read = MIN(config->fifo_samples, data_count);
	int32_t temp_sum = 0;

	for (uint8_t i = 0; i < samples_to_read; i++)
	{
		reg = MAX302XX_REG_FIFO_DATA;

		if (data->sensor_type == TEMP_SENSOR_MAX30210)
		{
			/* MAX30210: 3-byte read (TAG + temp) */
			uint8_t fifo_data[3];
			ret = i2c_write_read_dt(&config->i2c, &reg, 1, fifo_data, 3);
			if (ret < 0)
			{
				return ret;
			}
			/* TAG byte in fifo_data[0], temp in [1][2] */
			int16_t sample = (int16_t)((fifo_data[1] << 8) | fifo_data[2]);
			temp_sum += sample;
		}
		else
		{
			/* MAX30208: 2-byte read */
			uint8_t fifo_data[2];
			ret = i2c_write_read_dt(&config->i2c, &reg, 1, fifo_data, 2);
			if (ret < 0)
			{
				return ret;
			}
			int16_t sample = (int16_t)((fifo_data[0] << 8) | fifo_data[1]);
			temp_sum += sample;
		}
	}

	*temp = (int16_t)(temp_sum / samples_to_read);
	return 0;
}

/**
 * @brief Start temperature conversion (IC-specific)
 *
 * MAX30205: Always in continuous mode - no trigger needed
 * MAX30208: Write 0xC1 to TEMP_SENSOR_SETUP (0x14)
 * MAX30210: Write CONVERT_T bit to TEMP_CONTROL (0x2A)
 */
static int MAX302XX_start_conversion(const struct device *dev)
{
	const struct MAX302XX_data *data = dev->data;
	int ret;

	/* MAX30205 = 1, MAX30208 = 2, MAX30210 = 3 (UNKNOWN = 0) */
	if (data->sensor_type == TEMP_SENSOR_MAX30205)
	{
		/* Continuous mode - no trigger needed */
		return 0;
	}
	else if (data->sensor_type == TEMP_SENSOR_MAX30210)
	{
		/* MAX30210: Write CONVERT_T to TEMP_CONTROL */
		ret = MAX302XX_reg_write(dev, MAX30210_REG_TEMP_CONTROL, MAX30210_TEMP_CTRL_CONVERT);
	}
	else
	{
		/* MAX30208: Write TEMP_SETUP */
		uint8_t setup = MAX302XX_TEMP_SETUP_RFU | MAX302XX_TEMP_SETUP_CONVERT;
		ret = MAX302XX_reg_write(dev, MAX302XX_REG_TEMP_SETUP, setup);
	}

	if (ret < 0)
	{
		LOG_ERR("Failed to start conversion: %d", ret);
	}
	return ret;
}

/**
 * @brief Poll STATUS register for TEMP_RDY bit
 *
 * Polls STATUS (0x00) register waiting for TEMP_RDY bit (bit 0) = 1.
 * Typical wait: 15-50ms for conversion to complete.
 *
 * @param dev Device instance
 * @param timeout_ms Maximum time to wait in milliseconds
 * @return 0 on success, -ETIMEDOUT if conversion doesn't complete
 */
static int MAX302XX_wait_for_ready(const struct device *dev, uint32_t timeout_ms)
{
	uint8_t status;
	uint8_t fifo_count;
	int ret;
	uint32_t start_time = k_uptime_get_32();

	while ((k_uptime_get_32() - start_time) < timeout_ms)
	{
		ret = MAX302XX_reg_read(dev, MAX302XX_REG_STATUS, &status);
		if (ret < 0)
		{
			return ret;
		}

		/* Check TEMP_RDY bit */
		if (status & MAX302XX_STATUS_TEMP_RDY)
		{
			LOG_DBG("Conversion ready (Status Bit) after %d ms",
					(uint32_t)(k_uptime_get_32() - start_time));
			return 0;
		}

		/* Fallback: Check FIFO count. If data exists, conversion is done. */
		ret = MAX302XX_reg_read(dev, MAX302XX_REG_FIFO_DATA_CNT, &fifo_count);
		if (ret == 0 && fifo_count > 0)
		{
			LOG_DBG("Conversion ready (FIFO Count: %d) after %d ms",
					fifo_count, (uint32_t)(k_uptime_get_32() - start_time));
			return 0;
		}

		/* Poll every 5ms */
		k_msleep(5);
	}

	/* Debugging: Read FIFO count on timeout to see if conversion actually happened */
	uint8_t count = 0;
	if (MAX302XX_reg_read(dev, MAX302XX_REG_FIFO_DATA_CNT, &count) == 0) {
		LOG_ERR("Timeout waiting for TEMP_RDY. Status: 0x%02X, FIFO Count: %d", status, count);
	} else {
		LOG_ERR("Timeout waiting for TEMP_RDY. Status: 0x%02X, Failed to read FIFO count", status);
	}
	
	return -ETIMEDOUT;
}

/**
 * @brief Sample temperature (sensor_sample_fetch implementation)
 *
 * SYNC MODE (async_mode = false):
 *   1. Trigger N conversions to fill FIFO (N = fifo_samples config)
 *   2. Wait for each conversion to complete (~15-50ms each)
 *   3. Read all N samples from FIFO with burst reads
 *   4. Average the samples for noise reduction
 *   Returns: 0 with averaged data in temp_raw
 *
 * ASYNC MODE (async_mode = true, interrupt enabled):
 *   1. Trigger conversion (write 0xC1 to reg 0x14)
 *   2. Return immediately (0 = success)
 *   Interrupt handler will read data when ready and call user callback
 */
static int MAX302XX_sample_fetch(const struct device *dev,
								 enum sensor_channel chan)
{
	struct MAX302XX_data *data = dev->data;
	const struct MAX302XX_config *config = dev->config;
	int ret;

	if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_AMBIENT_TEMP)
	{
		return -ENOTSUP;
	}

	/* Verify device is initialized (should succeed at boot if powered) */
	if (!data->initialized)
	{
		LOG_ERR("Device not initialized - init failed at boot");
		return -ENODEV;
	}
	
	/* Diagnostic: Verify I2C communication by reading Part ID */
	uint8_t test_read;
	ret = MAX302XX_reg_read(dev, MAX302XX_REG_PART_IDENTIFIER, &test_read);
	if (ret < 0) {
		LOG_ERR("I2C communication failed (Part ID read error: %d). Sensor powered? Address correct?", ret);
		LOG_ERR("Expected I2C address: 0x%02X on I2C%d", config->i2c.addr, 
		        config->i2c.bus ? 1 : 0); /* Simplified - actual bus detection would need more work */
		return -EIO;
	}
	LOG_DBG("I2C OK: Part ID = 0x%02X, Sensor type = %d (1=MAX30205, 2=MAX30208, 3=MAX30210)", test_read, data->sensor_type);
	
	/* MAX30205: Continuous conversion mode - just read the register directly */
	if (data->sensor_type == TEMP_SENSOR_MAX30205) {
		LOG_DBG("MAX30205 mode - reading temperature register directly (continuous conversion)");
		ret = MAX302XX_read_temp(dev, &data->temp_raw);
		if (ret < 0) {
			LOG_ERR("Failed to read MAX30205 temperature: %d", ret);
			return ret;
		}
		return 0; /* Success */
	}

#ifdef CONFIG_MAX302XX_TRIGGER
	/* ASYNC MODE: Trigger single conversion, interrupt will handle read */
	if (data->async_mode && data->temp_ready_handler)
	{
		ret = MAX302XX_start_conversion(dev);
		if (ret < 0)
		{
			LOG_ERR("Failed to start conversion: %d", ret);
			return ret;
		}
		LOG_DBG("Conversion started (async mode - waiting for interrupt)");
		return 0; /* Success - data will be available in interrupt */
	}
#endif

	/* SYNC MODE: Fill FIFO with multiple samples for averaging */
	uint8_t samples_to_collect = config->fifo_samples;

	if (samples_to_collect > 1)
	{
		LOG_DBG("Filling FIFO with %d samples for averaging", samples_to_collect);
	}

	/* Trigger multiple conversions to fill FIFO */
	for (uint8_t i = 0; i < samples_to_collect; i++)
	{
		/* Trigger conversion */
		ret = MAX302XX_start_conversion(dev);
		if (ret < 0)
		{
			LOG_ERR("Failed to start conversion %d: %d", i, ret);
			return ret;
		}

		/* Wait for this conversion to complete (typical 15-50ms) */
		ret = MAX302XX_wait_for_ready(dev, 100);
		if (ret < 0)
		{
			LOG_ERR("Timeout waiting for conversion %d: %d", i, ret);
			return ret;
		}
	}

	LOG_DBG("All %d conversions complete, reading from FIFO", samples_to_collect);

	/* Read all samples from FIFO and average them */
	ret = MAX302XX_read_temp(dev, &data->temp_raw);
	if (ret < 0)
	{
		LOG_ERR("Failed to read temperature: %d", ret);
		return ret;
	}

	return 0;
}

/**
 * @brief Get temperature channel value (sensor_channel_get implementation)
 */
/**
 * @brief Get temperature channel value (sensor_channel_get implementation)
 */
static int MAX302XX_channel_get(const struct device *dev,
								enum sensor_channel chan,
								struct sensor_value *val)
{
	const struct MAX302XX_data *data = dev->data;
	float temp_c;
	float resolution;

	if (chan != SENSOR_CHAN_AMBIENT_TEMP)
	{
		return -ENOTSUP;
	}

	/* Use correct resolution based on detected sensor type */
	if (data->sensor_type == TEMP_SENSOR_MAX30205)
	{
		resolution = MAX30205_TEMP_RESOLUTION; /* 0.00390625degC */
	}
	else
	{
		resolution = MAX302XX_TEMP_RESOLUTION; /* 0.005degC (MAX30208/30210) */
	}

	/* Convert raw value to degrees Celsius */
	temp_c = (float)data->temp_raw * resolution;

	/* Split into val1 (integer) and val2 (fractional in millionths) */
	val->val1 = (int32_t)temp_c;
	val->val2 = (int32_t)((temp_c - (float)val->val1) * 1000000.0f);

	return 0;
}

/**
 * @brief Get sensor attribute (sensor_attr_get implementation)
 * 
 * Supports reading unique device ID via SENSOR_ATTR_MAX302XX_UNIQUE_ID.
 * Unique ID is 6 bytes stored during initialization.
 * 
 * @param dev Device instance
 * @param chan Sensor channel (ignored)
 * @param attr Attribute to read
 * @param val Pointer to store attribute value
 * @return 0 on success, negative errno on failure
 */
static int MAX302XX_attr_get(const struct device *dev,
							 enum sensor_channel chan,
							 enum sensor_attribute attr,
							 struct sensor_value *val)
{
	const struct MAX302XX_data *data = dev->data;

	if ((enum sensor_attribute)attr == (enum sensor_attribute)SENSOR_ATTR_MAX302XX_UNIQUE_ID) {
		/* Return unique ID as 6 bytes in val array
		 * Application should cast sensor_value array to uint8_t[6]
		 * or read via val[0].val1 to val[5].val1
		 */
		if (data->sensor_type == TEMP_SENSOR_MAX30205) {
			/* MAX30205 has no unique ID */
			return -ENOTSUP;
		}
		
		/* Store unique ID bytes in sensor_value array */
		for (int i = 0; i < 6; i++) {
			val[i].val1 = data->unique_id[i];
			val[i].val2 = 0;
		}
		return 0;
	}

	return -ENOTSUP;
}

#ifdef CONFIG_MAX302XX_TRIGGER

/**
 * @brief Set temperature threshold
 */
__maybe_unused
static int MAX302XX_set_threshold(const struct device *dev, uint8_t reg_msb,
								  int16_t threshold)
{
	int ret;

	ret = MAX302XX_reg_write(dev, reg_msb, (threshold >> 8) & 0xFF);
	if (ret < 0)
	{
		return ret;
	}

	return MAX302XX_reg_write(dev, reg_msb + 1, threshold & 0xFF);
}

/**
 * @brief Handle interrupt work (ASYNC MODE)
 *
 * Reads STATUS to clear IRQ, reads temperature from FIFO,
 * and calls user handler with updated data.
 */
static void MAX302XX_work_handler(struct k_work *work)
{
	struct MAX302XX_data *data = CONTAINER_OF(work, struct MAX302XX_data, work);
	const struct device *dev = data->dev;
	uint8_t status;
	int ret;

	/* Read and clear interrupt status */
	ret = MAX302XX_reg_read(dev, MAX302XX_REG_STATUS, &status);
	if (ret < 0)
	{
		LOG_ERR("Failed to read status: %d", ret);
		return;
	}

	if (status & MAX302XX_STATUS_TEMP_RDY)
	{
		/* Read temperature from FIFO */
		ret = MAX302XX_read_temp(dev, &data->temp_raw);
		if (ret < 0)
		{
			LOG_ERR("Failed to read temp in IRQ handler: %d", ret);
			return;
		}

		/* Call user handler if registered */
		if (data->temp_ready_handler)
		{
			data->temp_ready_handler(dev, data->temp_ready_trigger);
		}
	}
}

/**
 * @brief GPIO callback
 */
static void MAX302XX_gpio_callback(const struct device *dev,
								   struct gpio_callback *cb, uint32_t pins)
{
	struct MAX302XX_data *data = CONTAINER_OF(cb, struct MAX302XX_data, gpio_cb);

	ARG_UNUSED(dev);
	ARG_UNUSED(pins);

#if defined(CONFIG_MAX302XX_TRIGGER_OWN_THREAD)
	k_sem_give(&data->sem);
#elif defined(CONFIG_MAX302XX_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&data->work);
#endif
}

/**
 * @brief Set trigger (sensor_trigger_set implementation)
 *
 * Enables TEMP_RDY interrupt by writing to INTERRUPT_ENABLE register.
 *
 * Note: User must call sensor_sample_fetch() to trigger conversion.
 * In async mode, conversion starts and returns immediately.
 * When conversion completes, interrupt fires and handler is called.
 */
static int MAX302XX_trigger_set(const struct device *dev,
								const struct sensor_trigger *trig,
								sensor_trigger_handler_t handler)
{
	struct MAX302XX_data *data = dev->data;
	const struct MAX302XX_config *config = dev->config;
	uint8_t int_en = 0;
	int ret;

	if (trig->type != SENSOR_TRIG_DATA_READY)
	{
		return -ENOTSUP;
	}

	if (!config->int_gpio.port)
	{
		LOG_WRN("Interrupt GPIO not configured - use sync mode");
		return -ENOTSUP;
	}

	data->temp_ready_handler = handler;
	data->temp_ready_trigger = trig;

	/* Enable or disable TEMP_RDY interrupt */
	if (handler)
	{
		int_en = MAX302XX_INT_EN_TEMP_RDY;
		data->async_mode = true; /* Enable async mode */
		LOG_INF("Temperature interrupt enabled (async mode)");
	}
	else
	{
		data->async_mode = false; /* Disable async mode */
		LOG_INF("Temperature interrupt disabled (sync mode)");
	}

	ret = MAX302XX_reg_write(dev, MAX302XX_REG_INT_ENABLE, int_en);
	if (ret < 0)
	{
		LOG_ERR("Failed to set interrupt enable: %d", ret);
		return ret;
	}

	return 0;
}

#if defined(CONFIG_MAX302XX_TRIGGER_OWN_THREAD)
static void MAX302XX_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	struct MAX302XX_data *data = p1;

	while (1)
	{
		k_sem_take(&data->sem, K_FOREVER);
		MAX302XX_work_handler(&data->work);
	}
}
#endif

/**
 * @brief Initialize trigger support
 */
static int MAX302XX_init_interrupt(const struct device *dev)
{
	struct MAX302XX_data *data = dev->data;
	const struct MAX302XX_config *config = dev->config;
	int ret;

	if (!config->int_gpio.port)
	{
		LOG_DBG("No interrupt GPIO configured");
		return 0;
	}

	if (!gpio_is_ready_dt(&config->int_gpio))
	{
		LOG_ERR("Interrupt GPIO device not ready");
		return -ENODEV;
	}

	/* CRITICAL: Do NOT configure GPIO0 as INTB to avoid I2C address change!
	 *
	 * Per datasheet Table 3, configuring GPIO0_MODE = 11 (INTB) forces bit[0] = 0
	 * in I2C address, changing address from 0x53 -> 0x52.
	 *
	 * SOLUTION: Keep GPIO0 in default INPUT mode (GPIO0_MODE = 10)
	 * - max302xx GPIO0 pin will drive low when interrupt fires (open-drain)
	 * - External pullup + wire to nRF P1.02 handles signal routing
	 * - I2C address remains stable at 0x52 (GPIO1=1, GPIO0 reading = 0 via external wire)
	 * - Enable interrupt via INTERRUPT_ENABLE register only (no GPIO_SETUP write needed)
	 *
	 * Note: Comment out GPIO_SETUP write to prevent address change
	 */

	/* DO NOT WRITE GPIO_SETUP - causes address change!
	ret = MAX302XX_reg_write(dev, MAX302XX_REG_GPIO_SETUP,
							 MAX302XX_GPIO_SETUP_ASYNC_MODE);
	if (ret < 0) {
		LOG_ERR("Failed to configure GPIO_SETUP: %d", ret);
		return ret;
	}
	*/

	LOG_INF("Skipping GPIO_SETUP write to keep I2C address stable at 0x%02X", config->i2c.addr);

	/* Configure nRF52840 GPIO as input with pull-up for interrupt */
	ret = gpio_pin_configure_dt(&config->int_gpio, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0)
	{
		LOG_ERR("Failed to configure interrupt GPIO: %d", ret);
		return ret;
	}

	gpio_init_callback(&data->gpio_cb, MAX302XX_gpio_callback,
					   BIT(config->int_gpio.pin));

	ret = gpio_add_callback(config->int_gpio.port, &data->gpio_cb);
	if (ret < 0)
	{
		LOG_ERR("Failed to add GPIO callback: %d", ret);
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(&config->int_gpio,
										  GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0)
	{
		LOG_ERR("Failed to configure GPIO interrupt: %d", ret);
		return ret;
	}

#if defined(CONFIG_MAX302XX_TRIGGER_OWN_THREAD)
	k_sem_init(&data->sem, 0, K_SEM_MAX_LIMIT);
	k_thread_create(&data->thread, data->thread_stack,
					CONFIG_MAX302XX_THREAD_STACK_SIZE,
					MAX302XX_thread, data, NULL, NULL,
					K_PRIO_COOP(CONFIG_MAX302XX_THREAD_PRIORITY),
					0, K_NO_WAIT);
#elif defined(CONFIG_MAX302XX_TRIGGER_GLOBAL_THREAD)
	data->work.handler = MAX302XX_work_handler;
#endif

	return 0;
}

#endif /* CONFIG_MAX302XX_TRIGGER */

/**
 * @brief Initialize max302xx device
 */
static int MAX302XX_init(const struct device *dev)
{
	const struct MAX302XX_config *config = dev->config;
	struct MAX302XX_data *data = dev->data;
	uint8_t part_id;
	int ret;

	if (!i2c_is_ready_dt(&config->i2c))
	{
		LOG_ERR("I2C bus not ready");
		return -ENODEV;
	}

	/* Detect sensor type by reading Part Identifier (0xFF) */
	ret = MAX302XX_reg_read(dev, MAX302XX_REG_PART_IDENTIFIER, &part_id);

	/*
	 * BUGFIX: skin-temperature reading a flat 0.0 for an entire session
	 * (see BLE/record exports) traces to this — same boot-race class as
	 * VCNL3040 (see the retry loop in vcnl3040_init() for the full
	 * explanation): this sensor is on the Buck1/Buck2 rail that
	 * power_init() only enables from inside main(), well after this
	 * driver's own MAX302XX_init() has already run (and failed) at
	 * POST_KERNEL time.
	 *
	 * UPDATE 2026-08-31: widened this from 3s to 5s after a session
	 * where the 3s budget ran out with zero success. Widening did NOT
	 * help — the very next captured session (2026-09-01) failed
	 * completely again at 5s, AND a full I2C0 bus scan taken well after
	 * boot (t=29s, long past any plausible timing race) still showed
	 * neither this sensor nor VCNL3040 on the bus at all, while 4 other
	 * I2C0 devices (including one further down the address range) came
	 * up fine. That rules out "still just needs more time" — a device
	 * that isn't there, or isn't electrically connected, doesn't show up
	 * no matter how long you wait, and confirms the Aug-31 note below
	 * was right to suspect a genuine hardware/connection issue over a
	 * pure timing one.
	 *
	 * UPDATE 2026-09-01 (reverted the widening): worse, the 5s budget
	 * measurably hurt VCNL3040 — with two MAX302XX instances on this
	 * build (temp_wearable, temp_pulse) each burning a full failed
	 * retry serially before VCNL3040 gets a turn, the 2026-08-31
	 * session showed VCNL3040's first attempt at ~6.5s (matching a 3s+3s
	 * budget); the 2026-09-01 session — after this was widened to 5s —
	 * pushed VCNL3040's first attempt out to ~10.5s (matching 5s+5s).
	 * Since more retry time isn't fixing this sensor anyway, the only
	 * effect of widening it further is delaying VCNL3040's shot, which
	 * DOES have real evidence of recovering when it isn't starved for
	 * time. Reverted to a smaller budget — not zero, since a genuine
	 * short timing margin issue may still exist underneath the larger
	 * hardware problem, but no longer willing to trade VCNL3040's
	 * reliability for a retry duration that hasn't once succeeded here.
	 *
	 * If this specific unit keeps failing completely (not intermittently)
	 * after this change, the next diagnostic step is a physical check —
	 * MAX30208/MAX30205 solder joints, I2C0 continuity, and ideally a
	 * scope/multimeter on the Buck1/Buck2 rail itself during boot to
	 * confirm what voltage is actually present and when — not more
	 * firmware retry tuning. Three different budgets (200ms/3s/5s) have
	 * now been tried without a fix; a fourth guess isn't likely to
	 * either.
	 */
	for (int attempt = 0; ret != 0 && attempt < 20; attempt++)
	{
		k_msleep(50);
		ret = MAX302XX_reg_read(dev, MAX302XX_REG_PART_IDENTIFIER, &part_id);
	}

	LOG_INF("Part ID register (0xFF) read: ret=%d, value=0x%02X", ret, part_id);

	if (ret == 0 && (part_id == MAX30208_PART_ID || part_id == MAX30208_PART_ID_ALT))
	{
		/* MAX30208 detected (Part ID: 0x08 or 0x30) */
		data->sensor_type = TEMP_SENSOR_MAX30208;
		data->fifo_depth = MAX30208_FIFO_DEPTH;
		data->fifo_entry_size = 2; /* 2 bytes per entry */
		LOG_INF("MAX30208 detected (Part ID: 0x%02X)", part_id);

		/* Read and store unique device ID (0x31-0x36) */
		for (int i = 0; i < 6; i++)
		{
			ret = MAX302XX_reg_read(dev, MAX302XX_REG_PART_ID1 + i, &data->unique_id[i]);
			if (ret != 0)
			{
				data->unique_id[i] = 0;
			}
		}
		LOG_INF("Unique ID: %02X:%02X:%02X:%02X:%02X:%02X",
				data->unique_id[0], data->unique_id[1], data->unique_id[2],
				data->unique_id[3], data->unique_id[4], data->unique_id[5]);

		/* Reset and configure */
		ret = MAX302XX_reg_write(dev, MAX302XX_REG_SYS_CTRL, MAX302XX_SYS_CTRL_RESET);
		if (ret < 0)
		{
			LOG_ERR("Failed to reset: %d", ret);
			return ret;
		}
		k_msleep(10);

		/* Configure FIFO */
		ret = MAX302XX_reg_write(dev, MAX302XX_REG_FIFO_CFG2, 0x01);
		if (ret < 0)
		{
			LOG_ERR("Failed to configure FIFO: %d", ret);
			return ret;
		}
	}
	else if (ret != 0 || (part_id != MAX30208_PART_ID && part_id != MAX30208_PART_ID_ALT && part_id != MAX30210_PART_ID))
	{
		/* Fallback: Try MAX30205 (no Part ID register - will read 0xFF or random value) */
		LOG_WRN("Part ID 0x%02X doesn't match MAX30208 (0x08/0x30) or MAX30210 (0x45)", part_id);
		LOG_INF("Attempting MAX30205 fallback (MAX30205 has no Part ID register)");
		data->sensor_type = TEMP_SENSOR_MAX30205;
		data->fifo_depth = 0;			   /* No FIFO */
		data->fifo_entry_size = 0;		   /* Not applicable */
		
		/* Verify it's actually MAX30205 by reading config register */
		uint8_t config_test;
		ret = MAX302XX_reg_read(dev, MAX30205_CONFIG_REG, &config_test);
		if (ret != 0) {
			LOG_ERR("Failed to read config register - sensor not responding");
			return -ENODEV;
		}
		LOG_INF("MAX30205 mode activated (simplest variant - no FIFO/GPIO, continuous conversion)");
		/* No reset/config needed - MAX30205 powers up in continuous mode */
	}
	else if (ret == 0 && part_id == MAX30210_PART_ID)
	{
		/* MAX30210 detected (Part ID: 0x45) */
		data->sensor_type = TEMP_SENSOR_MAX30210;
		data->fifo_depth = MAX30210_FIFO_DEPTH;
		data->fifo_entry_size = 3; /* 3 bytes: TAG + temp */
		LOG_INF("MAX30210 detected (Part ID: 0x%02X) - Autonomous variant", part_id);

		/* Read and store unique device ID (0x30-0x35) */
		for (int i = 0; i < 6; i++)
		{
			ret = MAX302XX_reg_read(dev, MAX30210_REG_UNIQUE_ID1 + i, &data->unique_id[i]);
			if (ret != 0)
			{
				LOG_WRN("Failed to read unique ID byte %d: %d", i, ret);
				data->unique_id[i] = 0; /* Default on error */
			}
		}
		LOG_INF("Unique ID: %02X:%02X:%02X:%02X:%02X:%02X",
				data->unique_id[0], data->unique_id[1], data->unique_id[2],
				data->unique_id[3], data->unique_id[4], data->unique_id[5]);

		/* Reset using MAX30210 register */
		ret = MAX302XX_reg_write(dev, MAX302XX_REG_SYSTEM_CONFIG, MAX302XX_SYS_CTRL_RESET);
		if (ret < 0)
		{
			LOG_ERR("Failed to reset: %d", ret);
			return ret;
		}
		k_msleep(10);

		/* Configure FIFO */
		ret = MAX302XX_reg_write(dev, MAX302XX_REG_FIFO_CFG2, 0x01);
		if (ret < 0)
		{
			LOG_ERR("Failed to configure FIFO: %d", ret);
			return ret;
		}

		/* Disable autonomous mode initially (use sync/async) */
		ret = MAX302XX_reg_write(dev, MAX30210_REG_TEMP_CONTROL, 0x00);
		if (ret < 0)
		{
			LOG_WRN("Failed to disable AUTO mode: %d", ret);
		}
	}
	else
	{
		/* Unexpected Part ID or no response - try MAX30205 fallback */
		LOG_WRN("Unexpected Part ID: 0x%02X (expected 0x08/0x30 for MAX30208 or 0x45 for MAX30210)", part_id);
		LOG_INF("Attempting MAX30205 fallback (no Part ID register on MAX30205)");
		
		uint8_t config_reg;
		ret = MAX302XX_reg_read(dev, MAX30205_CONFIG_REG, &config_reg);
		if (ret == 0)
		{
			data->sensor_type = TEMP_SENSOR_MAX30205;
			data->fifo_depth = 0; /* No FIFO */
			data->fifo_entry_size = 0;
			memset(data->unique_id, 0, sizeof(data->unique_id)); /* No unique ID */
			LOG_INF("MAX30205 mode activated (simplest variant - no FIFO/GPIO, continuous conversion)");
			LOG_INF("DEBUG: sensor_type=%d (1=MAX30205, enum has UNKNOWN=0 offset)", data->sensor_type);

			/* Configure continuous conversion mode */
			ret = MAX302XX_reg_write(dev, MAX30205_CONFIG_REG, 0x00);
			if (ret < 0)
			{
				LOG_ERR("Failed to configure MAX30205: %d", ret);
				return ret;
			}
			k_msleep(10);
		}
		else
		{
			LOG_ERR("No MAX302xx sensor detected (Part ID: 0x%02X, config read failed: %d)", part_id, ret);
			LOG_ERR("Check: 1) I2C address (current: 0x%02X), 2) Power rail enabled, 3) Pull-ups present", 
			        config->i2c.addr);
			data->initialized = false;
			return -ENODEV;
		}
	}

	/* Initialize to sync mode by default */
	data->async_mode = false;
	data->initialized = true;

	LOG_INF("Sensor initialized: type=%d, FIFO depth=%d, entry size=%d bytes",
			data->sensor_type, data->fifo_depth, data->fifo_entry_size);

#ifdef CONFIG_MAX302XX_TRIGGER
	data->dev = dev;
	ret = MAX302XX_init_interrupt(dev);
	if (ret < 0)
	{
		LOG_WRN("Failed to initialize interrupt: %d (continuing without async mode)", ret);
		/* Don't fail init - interrupt is optional, sync mode still works */
	}
#endif

	LOG_INF("max302xx initialized in SYNC mode");
	return 0;
}

static const struct sensor_driver_api MAX302XX_driver_api = {
	.sample_fetch = MAX302XX_sample_fetch,
	.channel_get = MAX302XX_channel_get,
	.attr_get = MAX302XX_attr_get,
#ifdef CONFIG_MAX302XX_TRIGGER
	.trigger_set = MAX302XX_trigger_set,
#endif
};

#define MAX302XX_INIT(n)                                                        \
	static struct MAX302XX_data MAX302XX_data_##n;                              \
                                                                                \
	static const struct MAX302XX_config MAX302XX_config_##n = {                 \
		.i2c = I2C_DT_SPEC_INST_GET(n),                                         \
		IF_ENABLED(CONFIG_MAX302XX_TRIGGER,                                     \
				   (.int_gpio = GPIO_DT_SPEC_INST_GET_OR(n, int_gpios, {0}), )) \
			.alarm_high = DT_INST_PROP_OR(n, temp_alarm_high, 0) / 5,           \
		.alarm_low = DT_INST_PROP_OR(n, temp_alarm_low, 0) / 5,                 \
		.fifo_samples = DT_INST_PROP_OR(n, fifo_samples, 1),                    \
	};                                                                          \
                                                                                \
	SENSOR_DEVICE_DT_INST_DEFINE(n, MAX302XX_init, NULL,                        \
								 &MAX302XX_data_##n, &MAX302XX_config_##n,      \
								 POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,      \
								 &MAX302XX_driver_api);

DT_INST_FOREACH_STATUS_OKAY(MAX302XX_INIT)
