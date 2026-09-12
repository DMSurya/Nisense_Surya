/**
 * @file max32664.h
 * @brief MAX32664 Sensor Hub - Unified Sensor Ecosystem API
 *
 * This driver provides a unified interface to the MAX32664 biometric sensor hub
 * and its underlying sensors (MAX86141, MAX30101, LIS2DS12).
 *
 * Architecture:
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    MAX32664 Sensor Hub                          │
 * │   ┌───────────┐   ┌───────────┐   ┌───────────┐                │
 * │   │ MAX86141  │   │ MAX30101  │   │ LIS2DS12  │                │
 * │   │  PPG AFE  │   │  PPG AFE  │   │  Accel    │                │
 * │   └─────┬─────┘   └─────┬─────┘   └─────┬─────┘                │
 * │         └───────────────┼───────────────┘                       │
 * │                         ▼                                       │
 * │              ┌──────────────────┐                               │
 * │              │   Hub Firmware   │ ◄── Algorithms (optional)    │
 * │              └────────┬─────────┘                               │
 * └───────────────────────┼─────────────────────────────────────────┘
 *                         │ I2C (0x55)
 *                         ▼
 *                    nRF52840 MCU
 *
 * Operating Modes:
 *   - HUB_MODE: Hub runs HR/SpO2 algorithms internally
 *   - RAW_MODE: Hub outputs raw PPG samples for on-chip processing
 *   - PASSTHROUGH: Direct register access to underlying sensors
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_SENSOR_MAX32664_H_
#define ZEPHYR_INCLUDE_DRIVERS_SENSOR_MAX32664_H_

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Sensor Channels
 * ============================================================================ */

/**
 * @brief Custom sensor channels for MAX32664 ecosystem
 *
 * Standard Zephyr channels also supported:
 * - SENSOR_CHAN_IR: PPG IR value (raw ADC counts)
 * - SENSOR_CHAN_RED: PPG Red value (raw ADC counts)
 * - SENSOR_CHAN_GREEN: PPG Green value (MAX86141 only)
 * - SENSOR_CHAN_ACCEL_X/Y/Z: Accelerometer (milli-g)
 */
#define SENSOR_CHAN_MAX32664_SPO2            (SENSOR_CHAN_PRIV_START + 0)  /* SpO2 % (0-100) */
#define SENSOR_CHAN_MAX32664_HR              (SENSOR_CHAN_PRIV_START + 1)  /* HR in 0.1 BPM */
#define SENSOR_CHAN_MAX32664_QUALITY         (SENSOR_CHAN_PRIV_START + 2)  /* Signal quality (0-100) */
#define SENSOR_CHAN_MAX32664_AMBIENT         (SENSOR_CHAN_PRIV_START + 3)  /* Ambient light */
#define SENSOR_CHAN_MAX32664_SYSTOLIC_BP     (SENSOR_CHAN_PRIV_START + 4)  /* Systolic BP (mmHg) */
#define SENSOR_CHAN_MAX32664_DIASTOLIC_BP    (SENSOR_CHAN_PRIV_START + 5)  /* Diastolic BP (mmHg) */
#define SENSOR_CHAN_MAX32664_BP_CONFIDENCE   (SENSOR_CHAN_PRIV_START + 6)  /* BP confidence */
#define SENSOR_CHAN_MAX32664_HR_CONFIDENCE   (SENSOR_CHAN_PRIV_START + 7)  /* HR confidence */
#define SENSOR_CHAN_MAX32664_SPO2_CONFIDENCE (SENSOR_CHAN_PRIV_START + 8)  /* SpO2 confidence */
#define SENSOR_CHAN_MAX32664_SCD_STATE       (SENSOR_CHAN_PRIV_START + 9)  /* SCD: 0=no contact, >0=contact */

/**
 * Hub RAW FIFO fillers on FW 30.2.2: exact 0x7FFFF and 0x7ECxx near-fillers.
 * Do not use a low floor like 0x70000 — real green DC under finger contact
 * routinely sits in 400k–500k and must be kept.
 */
#define MAX32664_HUB_ADC_INVALID_LO          0x7EC00U
#define MAX32664_HUB_ADC_INVALID_EXACT       0x7FFFFU

static inline bool max32664_hub_adc_is_filler(uint32_t adc)
{
	return (adc == MAX32664_HUB_ADC_INVALID_EXACT) ||
	       ((adc >= MAX32664_HUB_ADC_INVALID_LO) &&
		(adc < MAX32664_HUB_ADC_INVALID_EXACT));
}

static inline uint32_t max32664_hub_adc_sanitize(uint32_t adc)
{
	return max32664_hub_adc_is_filler(adc) ? 0U : adc;
}

static inline bool max32664_hub_adc_plausible(uint32_t adc)
{
	return (adc > 0U) && !max32664_hub_adc_is_filler(adc);
}

/* ============================================================================
 * Operating Modes
 * ============================================================================ */

/**
 * @brief Hub operating modes
 */
enum max32664_mode {
	MAX32664_MODE_HUB = 0,        /**< Hub algorithms (HR/SpO2/BPT) */
	MAX32664_MODE_RAW = 1,        /**< Raw PPG samples output */
	MAX32664_MODE_PASSTHROUGH = 2 /**< Direct sensor register access */
};

/**
 * @brief Hub variant (auto-detected via AFE WHOAMI)
 * 
 * Note: Variants align with Maxim's A/B/C/D naming convention:
 * - Variant A/D: MAX30101 AFE (finger PPG, GWED hardware)
 * - Variant B/C: MAX86141 AFE (wrist PPG, GWEC hardware)
 * 
 * GWEC/GWED are legacy hardware revision names.
 */
enum max32664_variant_type {
	MAX32664_VARIANT_TYPE_UNKNOWN = 0,
	MAX32664_VARIANT_TYPE_A,      /**< MaximFast: HR+SpO2 @100Hz, MAX30101, no accel required */
	MAX32664_VARIANT_TYPE_B,      /**< WHRM: HR @25Hz, MAX86141, KX-122 REQUIRED */
	MAX32664_VARIANT_TYPE_C,      /**< WHRM+WSpO2: HR+SpO2 @25Hz, MAX86141, KX-122 REQUIRED (GWEC) */
	MAX32664_VARIANT_TYPE_D,      /**< BPT: BP+HR+SpO2 @100Hz, MAX30101, calibration REQUIRED (GWED) */
};

/* Legacy aliases for backward compatibility */
#define MAX32664_VARIANT_GWEC MAX32664_VARIANT_TYPE_C
#define MAX32664_VARIANT_GWED MAX32664_VARIANT_TYPE_D

/**
 * @brief Detected AFE type
 */
enum max32664_afe_type {
	MAX32664_AFE_UNKNOWN = 0,
	MAX32664_AFE_MAX86141,    /**< Wrist PPG AFE (WHOAMI: 0x25) */
	MAX32664_AFE_MAX86161,    /**< Wrist PPG AFE (WHOAMI: 0x36) */
	MAX32664_AFE_MAX30101,    /**< Finger PPG AFE family (Part ID 0x15: MAX30101/02/05) */
};

/* ============================================================================
 * Custom Attributes
 * ============================================================================ */

/**
 * @brief Custom attributes for sensor_attr_set/get
 */
enum max32664_attribute {
	/** Feed external accelerometer data to hub algorithm */
	SENSOR_ATTR_MAX32664_ACCEL_DATA = SENSOR_ATTR_PRIV_START,
	
	/** Set operating mode (val1 = enum max32664_mode) */
	SENSOR_ATTR_MAX32664_MODE,
	
	/** Get detected variant (val1 = enum max32664_variant_type A/B/C/D) */
	SENSOR_ATTR_MAX32664_VARIANT,
	
	/** Get detected AFE type (val1 = enum max32664_afe_type) */
	SENSOR_ATTR_MAX32664_AFE_TYPE,
};

/* ============================================================================
 * Hub Information API
 * ============================================================================ */

/**
 * @brief Get detected hub variant
 *
 * @param dev MAX32664 device pointer
 * @return enum max32664_variant_type (A/B/C/D)
 */
enum max32664_variant_type max32664_get_variant(const struct device *dev);

/**
 * @brief Get detected AFE type
 *
 * @param dev MAX32664 device pointer
 * @return enum max32664_afe_type
 */
enum max32664_afe_type max32664_get_afe_type(const struct device *dev);

/**
 * @brief Get AFE WHOAMI value
 *
 * @param dev MAX32664 device pointer
 * @return Part ID byte (0x25=MAX86141, 0x36=MAX86161, 0x15=MAX30101/02/05)
 */
uint8_t max32664_get_afe_whoami(const struct device *dev);

/**
 * @brief Get hub firmware version
 *
 * @param dev MAX32664 device pointer
 * @param major Output: major version (can be NULL)
 * @param minor Output: minor version (can be NULL)
 * @param patch Output: patch version (can be NULL)
 * @return 0 on success, negative errno on failure
 */
int max32664_get_firmware_version(const struct device *dev,
				  uint8_t *major, uint8_t *minor, uint8_t *patch);

/**
 * @brief Bootloader transport interface exposed to firmware updater
 *
 * This provides only the bus and GPIO handles required by the firmware
 * updater without exposing internal driver config structures.
 */
struct max32664_bootloader_io {
	struct i2c_dt_spec bus;
	struct gpio_dt_spec int_gpio;
	struct gpio_dt_spec rstn_gpio;
};

/**
 * @brief Get MAX32664 transport interface for bootloader operations
 *
 * @param dev MAX32664 device pointer
 * @param io Output transport interface snapshot
 * @return 0 on success, negative errno on failure
 */
int max32664_get_bootloader_io(const struct device *dev,
			      struct max32664_bootloader_io *io);

/**
 * @brief Coarse-grained hub bus lock for multi-transaction sequences
 *
 * Firmware update and other out-of-band I2C paths that call
 * max32664_get_bootloader_io() + raw i2c_* must take this lock so they
 * do not interleave with the driver's per-command transmit path or the
 * host accel feeder. Soft-recursive with max32664_i2c_transmit().
 *
 * Always pair lock with unlock.
 */
void max32664_bus_lock(const struct device *dev);
void max32664_bus_unlock(const struct device *dev);

/**
 * @brief Host-fed accel feeder control (no-op stubs if feeder not compiled)
 *
 * Firmware update must stop the feeder so it cannot touch the hub bus
 * during bootloader programming.
 */
#if defined(CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER)
int max32664_accel_feeder_start(const struct device *hub);
int max32664_accel_feeder_stop(const struct device *hub);
bool max32664_accel_feeder_is_running(void);
#else
static inline int max32664_accel_feeder_start(const struct device *hub)
{
	(void)hub;
	return 0;
}
static inline int max32664_accel_feeder_stop(const struct device *hub)
{
	(void)hub;
	return 0;
}
static inline bool max32664_accel_feeder_is_running(void)
{
	return false;
}
#endif

/* ============================================================================
 * Mode Control API
 * ============================================================================ */

/**
 * @brief Set hub operating mode
 *
 * @param dev MAX32664 device pointer
 * @param mode Operating mode to set
 * @return 0 on success, negative errno on failure
 */
int max32664_set_mode(const struct device *dev, enum max32664_mode mode);

/**
 * @brief Get current operating mode
 *
 * @param dev MAX32664 device pointer
 * @return Current operating mode
 */
enum max32664_mode max32664_get_mode(const struct device *dev);

/* ============================================================================
 * PPG AFE Direct Access API (MAX86141 / MAX30101)
 * ============================================================================ */

/**
 * @brief Raw PPG sample from AFE
 */
struct max32664_ppg_sample {
	uint32_t red;       /**< Red LED ADC value */
	uint32_t ir;        /**< IR LED ADC value */
	uint32_t green;     /**< Green LED ADC value (MAX86141 only) */
	uint32_t ambient;   /**< Ambient / no-LED slot (hub byte 10-12, slot4) */
	uint32_t timestamp; /**< Sample timestamp (ms) */
};

/**
 * @brief Read AFE register via hub passthrough
 *
 * Reads a register from the underlying PPG AFE (MAX86141 or MAX30101)
 * via hub command Family 0x41 (Read Sensor Attribute).
 *
 * @param dev MAX32664 device pointer
 * @param reg_addr AFE register address
 * @param value Output: register value
 * @return 0 on success, negative errno on failure
 */
int max32664_afe_read_reg(const struct device *dev, uint8_t reg_addr, uint8_t *value);

/**
 * @brief Write AFE register via hub passthrough
 *
 * Writes a register to the underlying PPG AFE via hub command.
 * Note: Some registers may be protected by hub firmware.
 *
 * @param dev MAX32664 device pointer
 * @param reg_addr AFE register address
 * @param value Value to write
 * @return 0 on success, negative errno on failure
 */
int max32664_afe_write_reg(const struct device *dev, uint8_t reg_addr, uint8_t value);

/**
 * @brief Read raw PPG samples from AFE FIFO
 *
 * Reads raw PPG samples directly from the AFE FIFO via hub.
 * Available when the build selects RAW mode via CONFIG_MAX32664_MODE_RAW.
 *
 * @param dev MAX32664 device pointer
 * @param samples Output buffer for samples
 * @param max_samples Maximum samples to read
 * @param num_samples Output: actual number of samples read
 * @return 0 on success, negative errno on failure
 */
int max32664_afe_read_fifo(const struct device *dev,
			   struct max32664_ppg_sample *samples,
			   size_t max_samples,
			   size_t *num_samples);

/**
 * @brief Configure AFE LED current
 *
 * @param dev MAX32664 device pointer
 * @param led LED index (0=Red, 1=IR, 2=Green)
 * @param current_ma LED current in mA (0.0 to 50.0, resolution depends on AFE)
 * @return 0 on success, negative errno on failure
 */
int max32664_afe_set_led_current(const struct device *dev, uint8_t led, uint8_t current_ma);

/**
 * @brief Get AFE LED current
 *
 * @param dev MAX32664 device pointer
 * @param led LED index (0=Red, 1=IR, 2=Green)
 * @param current_ma Output: LED current in mA
 * @return 0 on success, negative errno on failure
 */
int max32664_afe_get_led_current(const struct device *dev, uint8_t led, uint8_t *current_ma);

/**
 * @brief Host-side LED AGC for RAW mode (next measurement PA seed)
 *
 * Independently targets each channel's DC operating point (IR/Red/Green
 * each have their own reference DC band) and reports each channel's AC
 * amplitude against its own reference band for visibility — AC is not
 * forced to a target the way DC is, since AC/DC ratio (perfusion index)
 * is largely a property of the subject/contact, not LED current alone.
 * Steps each channel's PA up/down independently between measurements.
 * No-op outside CONFIG_MAX32664_MODE_RAW builds (stub returns 0).
 *
 * @param hub MAX32664 hub device (may be NULL)
 * @param dc_ir Last measurement mean IR DC
 * @param ac_ir Last measurement IR AC (pulsatile) amplitude
 * @param dc_red Last measurement mean Red DC
 * @param ac_red Last measurement Red AC (pulsatile) amplitude
 * @param dc_green Last measurement mean Green DC
 * @param ac_green Last measurement Green AC (pulsatile) amplitude
 * @return Current IR-channel PA register value after update
 */
uint8_t max32664_raw_host_agc(const struct device *hub,
			       uint32_t dc_ir, uint32_t ac_ir,
			       uint32_t dc_red, uint32_t ac_red,
			       uint32_t dc_green, uint32_t ac_green);

/**
 * @brief Current host-controlled RAW LED PA register value
 */
uint8_t max32664_raw_get_led_pa(void);

/**
 * @brief Seed RAW LED PA from factory calibration (µA → PA), else keep default
 */
void max32664_raw_seed_led_pa_from_cal(void);

/* ============================================================================
 * Accelerometer Direct Access API (LIS2DS12)
 * ============================================================================ */

/**
 * @brief Accelerometer sample
 */
struct max32664_accel_sample {
	int16_t x;          /**< X-axis in milli-g */
	int16_t y;          /**< Y-axis in milli-g */
	int16_t z;          /**< Z-axis in milli-g */
	uint32_t timestamp; /**< Sample timestamp (ms) */
};

/**
 * @brief Read accelerometer data via hub
 *
 * Reads XYZ acceleration from LIS2DS12 connected to hub's SPI bus.
 *
 * @param dev MAX32664 device pointer
 * @param sample Output: accelerometer sample
 * @return 0 on success, negative errno on failure (-ENOTSUP if GWED variant)
 */
int max32664_accel_read(const struct device *dev, struct max32664_accel_sample *sample);

/**
 * @brief Read accelerometer register via hub passthrough
 *
 * @param dev MAX32664 device pointer
 * @param reg_addr LIS2DS12 register address
 * @param value Output: register value
 * @return 0 on success, negative errno on failure
 */
int max32664_accel_read_reg(const struct device *dev, uint8_t reg_addr, uint8_t *value);

/**
 * @brief Write accelerometer register via hub passthrough
 *
 * @param dev MAX32664 device pointer
 * @param reg_addr LIS2DS12 register address
 * @param value Value to write
 * @return 0 on success, negative errno on failure
 */
int max32664_accel_write_reg(const struct device *dev, uint8_t reg_addr, uint8_t value);

/**
 * @brief Configure accelerometer full-scale range
 *
 * @param dev MAX32664 device pointer
 * @param range_g Full-scale range in g (2, 4, 8, or 16)
 * @return 0 on success, negative errno on failure
 */
int max32664_accel_set_range(const struct device *dev, uint8_t range_g);

/**
 * @brief Configure accelerometer output data rate
 *
 * @param dev MAX32664 device pointer
 * @param odr_hz Output data rate in Hz (1, 12, 25, 50, 100, 200, 400, 800)
 * @return 0 on success, negative errno on failure
 */
int max32664_accel_set_odr(const struct device *dev, uint16_t odr_hz);

/**
 * @brief Get accelerometer WHOAMI
 *
 * @param dev MAX32664 device pointer
 * @return WHOAMI byte (0x43 for LIS2DS12), 0 if not available
 */
uint8_t max32664_accel_get_whoami(const struct device *dev);

/* ============================================================================
 * External Sensor Feed API (for hub algorithms)
 * ============================================================================ */

/**
 * @brief Feed external accelerometer data to hub
 *
 * When accel-source = "external" in device tree, use this to feed
 * accelerometer data from an external source to the hub's WHRM algorithm.
 * Should be called at 25Hz.
 *
 * @param dev MAX32664 device pointer
 * @param x_mg X-axis in milli-g
 * @param y_mg Y-axis in milli-g
 * @param z_mg Z-axis in milli-g
 * @return 0 on success, negative errno on failure
 */
int max32664_feed_accel(const struct device *dev, int16_t x_mg, int16_t y_mg, int16_t z_mg);

/* ============================================================================
 * Algorithm Control API
 * ============================================================================ */

/**
 * @brief Start hub algorithms (HR/SpO2)
 *
 * Enables the appropriate algorithms based on detected variant.
 * Available when the build selects HUB mode via CONFIG_MAX32664_MODE_HUB.
 *
 * @param dev MAX32664 device pointer
 * @return 0 on success, negative errno on failure
 */
int max32664_algo_start(const struct device *dev);

/**
 * @brief Stop hub algorithms
 *
 * @param dev MAX32664 device pointer
 * @return 0 on success, negative errno on failure
 */
int max32664_algo_stop(const struct device *dev);

/**
 * @brief Check if hub algorithms are running
 *
 * @param dev MAX32664 device pointer
 * @return true if algorithms running, false otherwise
 */
bool max32664_algo_is_running(const struct device *dev);

/**
 * @brief Idle sleep path: stop sampling and leave MFIO high so the hub may
 *        enter AN6924 deep sleep when idle (no ShutdownLP command).
 *
 * RAW/HUB/PASSTHROUGH: cancels poll/IRQ work and pauses output when applicable.
 * Status 0xFF afterward usually means the hub is asleep until the next MFIO wake.
 */
int max32664_sleep(const struct device *dev);

/**
 * @brief Hub ShutdownLP0/LP1 (AN6924 Table 25 ~p.85).
 *
 * @param retain_ram false → ShutdownLP0 (0x01 0x00 0x01, power-cycle to wake);
 *                   true  → ShutdownLP1 (0x01 0x00 0x10, RSTN or power-cycle).
 */
int max32664_shutdown(const struct device *dev, bool retain_ram);

/**
 * @brief Soft reset via Family 0x01 Index 0x00 Data 0x02, then APP_INIT wait.
 */
int max32664_soft_reset(const struct device *dev);

/**
 * @brief Read SpO2 calibration coefficients (HUB; Family 0x51/0x07/0x00).
 * Experimental — requires CONFIG_MAX32664_HUB_SPO2_CAL_REPORT.
 *
 * @param a_b_c_x1e5 output array of 3 int32 values (coeff × 1e5)
 */
int max32664_read_spo2_calibration(const struct device *dev, int32_t a_b_c_x1e5[3]);

/**
 * @brief Write SpO2 calibration coefficients (HUB; Family 0x50/0x07/0x00).
 */
int max32664_write_spo2_calibration(const struct device *dev,
				    const int32_t a_b_c_x1e5[3]);

/**
 * Classify hub command status byte for retries / diagnostics.
 * 0x00 OK; 0xFE busy (retry); 0xFF unknown/asleep; else error.
 */
enum max32664_cmd_status_class {
	MAX32664_CMD_STATUS_OK = 0,
	MAX32664_CMD_STATUS_BUSY,
	MAX32664_CMD_STATUS_UNKNOWN,
	MAX32664_CMD_STATUS_ERROR,
};

static inline enum max32664_cmd_status_class
max32664_classify_cmd_status(uint8_t status)
{
	if (status == 0x00) {
		return MAX32664_CMD_STATUS_OK;
	}
	if (status == 0xFE) {
		return MAX32664_CMD_STATUS_BUSY;
	}
	if (status == 0xFF) {
		return MAX32664_CMD_STATUS_UNKNOWN;
	}
	return MAX32664_CMD_STATUS_ERROR;
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_SENSOR_MAX32664_H_ */

