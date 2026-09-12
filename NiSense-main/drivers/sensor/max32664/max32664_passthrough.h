/* =============================================================================
 * MAX32664 Passthrough Mode Implementation
 * =============================================================================
 * Purpose: Coordinate passthrough access to MAX86141 and LIS2DS12 sensors
 * Mode: Passthrough (hub disabled, direct sensor control from nRF52840)
 * 
 * Architecture:
 * - Disables hub algorithms and AFE management
 * - Sets hub output mode to PAUSE
 * - Provides unified API for sensor initialization and data collection
 * - Manages sample synchronization between PPG and accelerometer
 * ============================================================================= */

#ifndef MAX32664_PASSTHROUGH_H
#define MAX32664_PASSTHROUGH_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>

/* MAX86141 PPG Sample Structure (simplified) */
struct max86141_sample {
	uint32_t ir_raw;         /* IR LED ADC value (19-bit) */
	uint32_t red_raw;        /* Red LED ADC value (19-bit) */
	uint32_t ambient_raw;    /* Ambient light ADC value (19-bit, if enabled) */
	uint8_t fifo_tag;        /* FIFO tag byte */
	bool valid;              /* Sample validity */
};

/* LIS2DS12 Accelerometer Sample Structure (simplified) */
struct lis2ds12_sample {
	int16_t x_raw;           /* X-axis raw (16-bit two's complement) */
	int16_t y_raw;           /* Y-axis raw */
	int16_t z_raw;           /* Z-axis raw */
	float x_mg;              /* X-axis in milli-g (converted) */
	float y_mg;              /* Y-axis in milli-g */
	float z_mg;              /* Z-axis in milli-g */
};

/* Passthrough Mode Configuration */
struct max32664_passthrough_config {
	bool enable_ppg;           /* Enable PPG sensor (MAX86141) */
	bool enable_accel;         /* Enable accelerometer (LIS2DS12) */
	uint8_t ppg_sample_rate;   /* PPG sample rate index (0=25Hz, 1=50Hz, etc.) */
	uint8_t accel_odr;         /* Accelerometer ODR index */
	uint8_t led1_current;      /* Red LED current (0-255) */
	uint8_t led2_current;      /* IR LED current (0-255) */
};

/* Combined Sample Structure (PPG + Accel synchronized) */
struct max32664_passthrough_sample {
	struct max86141_sample ppg;     /* PPG data (Red, IR, Ambient) */
	struct lis2ds12_sample accel;   /* Accelerometer data (X, Y, Z) */
	uint64_t timestamp_ms;          /* Sample timestamp */
};

/* =============================================================================
 * Public API Functions
 * ============================================================================= */

/**
 * @brief Initialize MAX32664 in passthrough mode
 * 
 * Disables hub algorithms, AFE management, and output mode.
 * Initializes MAX86141 and LIS2DS12 with default settings.
 * 
 * @param dev Pointer to MAX32664 hub device
 * @return 0 on success, negative errno on failure
 */
int max32664_passthrough_mode_init(const struct device *dev);

/**
 * @brief Configure passthrough mode with custom settings
 * 
 * @param dev Pointer to MAX32664 hub device
 * @param config Pointer to configuration structure
 * @return 0 on success, negative errno on failure
 */
int max32664_passthrough_mode_configure(const struct device *dev,
					const struct max32664_passthrough_config *config);

/**
 * @brief Start data collection in passthrough mode
 * 
 * Starts MAX86141 and/or LIS2DS12 sampling based on configuration.
 * 
 * @param dev Pointer to MAX32664 hub device
 * @return 0 on success, negative errno on failure
 */
int max32664_passthrough_mode_start(const struct device *dev);

/**
 * @brief Stop data collection in passthrough mode
 * 
 * Stops MAX86141 and LIS2DS12 sampling to save power.
 * 
 * @param dev Pointer to MAX32664 hub device
 * @return 0 on success, negative errno on failure
 */
int max32664_passthrough_mode_stop(const struct device *dev);

/**
 * @brief Read synchronized PPG and accelerometer samples
 * 
 * Reads available samples from both sensors and timestamps them.
 * 
 * @param dev Pointer to MAX32664 hub device
 * @param samples Array to store combined samples
 * @param max_samples Maximum number of samples to read
 * @param count Pointer to store actual number of samples read
 * @return 0 on success, negative errno on failure
 */
int max32664_passthrough_mode_read_samples(const struct device *dev,
					    struct max32664_passthrough_sample *samples,
					    uint8_t max_samples, uint8_t *count);

/**
 * @brief Disable hub algorithms and enable passthrough mode
 * 
 * Critical setup steps:
 * 1. Disable algorithm (Family 0x52, Index 0x07, Data 0x00)
 * 2. Disable AFE (Family 0x44, Index 0x00, Data 0x00)
 * 3. Set output mode to PAUSE (Family 0x10, Index 0x00, Data 0x00)
 * 
 * @param dev Pointer to MAX32664 hub device
 * @return 0 on success, negative errno on failure
 */
int max32664_passthrough_mode_enable(const struct device *dev);

/**
 * @brief Set hub output mode (PAUSE / SENSOR / ALGO / BOTH)
 * 
 * Used to switch between PAUSE mode (register config) and SENSOR mode
 * (data acquisition via hub output FIFO, Family 0x12).
 * 
 * @param dev  Pointer to MAX32664 hub device
 * @param mode Output mode: 0x00=PAUSE, 0x01=SENSOR, 0x02=ALGO, 0x03=BOTH
 * @return 0 on success, negative errno on failure
 */
int max32664_passthrough_set_output_mode(const struct device *dev,
					 uint8_t mode);

/**
 * @brief Read PPG samples from MAX86141 FIFO via passthrough (2-channel)
 * 
 * @param dev Pointer to MAX32664 hub device
 * @param ir Pointer to store IR value
 * @param red Pointer to store Red value
 * @return Number of samples read, or negative errno on failure
 */
int max32664_passthrough_read_ppg(const struct device *dev,
				  uint32_t *ir, uint32_t *red);

/**
 * @brief Read PPG samples with green channel via passthrough
 * 
 * @param dev Pointer to MAX32664 hub device
 * @param ir_value Pointer to store IR value
 * @param red_value Pointer to store Red value
 * @param green_value Pointer to store Green value (may be NULL)
 * @return Number of samples read, or negative errno on failure
 */
int max32664_passthrough_read_ppg_ex(const struct device *dev,
				     uint32_t *ir_value, uint32_t *red_value,
				     uint32_t *green_value);

/**
 * @brief Read accelerometer data from LIS2DS12 via passthrough
 * 
 * @param dev Pointer to MAX32664 hub device
 * @param x Pointer to store X-axis (milli-g)
 * @param y Pointer to store Y-axis (milli-g)
 * @param z Pointer to store Z-axis (milli-g)
 * @return 0 on success, negative errno on failure
 */
int max32664_passthrough_read_accel(const struct device *dev,
				    int16_t *x, int16_t *y, int16_t *z);

/**
 * @brief Read combined PPG + accelerometer sample
 * 
 * @param dev Pointer to MAX32664 hub device
 * @param sample Pointer to store combined sample
 * @return 0 on success, negative errno on failure
 */
int max32664_passthrough_read_sample(const struct device *dev,
				     struct max32664_passthrough_sample *sample);

#endif /* MAX32664_PASSTHROUGH_H */
