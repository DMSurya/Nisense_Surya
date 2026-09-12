/**
 * @file max32664_priv.h
 * @brief MAX32664 sensor hub private interface (variant ops and internal APIs)
 *
 * Private header defining:
 * - Common internal helper functions shared across variants
 * - Variant-specific operations function pointers
 * - Data structures for variant implementations
 *
 * This file is ONLY for use within the max32664 driver domain:
 * - max32664.c (common core)
 * - max32664a.c (Variant A: MaximFast)
 * - max32664b.c (Variant B: WHRM)
 * - max32664c.c (Variant C: WHRM+WSpO2)
 * - max32664d.c (Variant D: BPT)
 *
 * NOT for inclusion by application code or other drivers.
 */

#ifndef MAX32664_PRIV_H_
#define MAX32664_PRIV_H_

/* Include public API header for struct definitions (max32664_ppg_sample, etc.) */
#include <zephyr/drivers/sensor/max32664.h>

/* Include driver internal header */
#include "max32664.h"

/*
 * AN6924 Reset to Application (~p.10):
 *   RSTN low ≥10 ms; MFIO high ≥1 ms before RSTN rise; +50 ms app mode;
 *   then ~1.8 s ready for Variant C (A/D closer to ~1.0 s).
 */
#define MAX32664_RSTN_ASSERT_MS		10U
#define MAX32664_MFIO_SETUP_BEFORE_RSTN_MS	1U
#define MAX32664_APP_MODE_SETTLE_MS	50U
#define MAX32664_APP_INIT_MS_C		1800U
#define MAX32664_APP_INIT_MS_AD		1000U
/* Product path is Variant C (MAX32664C / GWEC). */
#define MAX32664_APP_INIT_MS		MAX32664_APP_INIT_MS_C
/* AN6924 default CMD_DELAY for Family 0x12 FIFO reads. */
#define MAX32664_CMD_DELAY_FIFO_MS	2U

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration of device structure */
struct max32664_data;

/**
 * @brief Variant-specific operations function pointers
 *
 * Each variant (A/B/C/D) implements these operations to handle
 * its specific hardware configuration and algorithm initialization.
 */
struct max32664_variant_ops {
	/**
	 * @brief Variant-specific initialization
	 *
	 * Called after common init (I2C, GPIO, mode check, FW version).
	 * Responsible for:
	 * - Enabling AFE (PPG sensor)
	 * - Enabling accelerometer (if required)
	 * - Enabling algorithm(s)
	 * - Configuring algorithm parameters (AGC, WHRM mode, etc.)
	 * - Setting output mode
	 *
	 * @param dev Pointer to device structure
	 * @return 0 on success, negative errno on failure
	 */
	int (*init)(const struct device *dev);

	/**
	 * @brief Parse FIFO data into sensor values
	 *
	 * Each variant has different FIFO entry format:
	 * - Variant A: 11 bytes (HR + SpO2 + confidence + PPG)
	 * - Variant B/C: 16 bytes (HR + SpO2 + accel + PPG + status)
	 * - Variant D: 10 bytes (systolic + diastolic + HR + SpO2 + status)
	 *
	 * @param dev Pointer to device structure
	 * @param data Raw FIFO data buffer
	 * @param len Length of FIFO data (should match fifo_entry_size)
	 * @return 0 on success, negative errno on failure
	 */
	int (*parse_fifo)(const struct device *dev, const uint8_t *data, size_t len);

	/**
	 * @brief FIFO entry size in bytes
	 *
	 * Used to validate FIFO reads and determine how many samples
	 * are available in the FIFO.
	 */
	size_t fifo_entry_size;

	/**
	 * @brief Dump variant-specific sensor registers for diagnostics
	 *
	 * Each variant has different sensors to dump:
	 * - Variant A: MAX30101 AFE
	 * - Variant B: MAX86141 AFE + accelerometer
	 * - Variant C: MAX86141 AFE + LIS2DS12/KX-122 accelerometer
	 * - Variant D: MAX30101 AFE
	 *
	 * @param dev Pointer to device structure
	 */
	void (*dump_registers)(const struct device *dev);

	/**
	 * @brief Human-readable variant name for logging
	 */
	const char *name;
};

/* Variant operation structures (defined in variant files) */
extern const struct max32664_variant_ops max32664a_ops;  /* MaximFast */
#if IS_ENABLED(CONFIG_MAX32664_HUB_VARIANT_B)
extern const struct max32664_variant_ops max32664b_ops;  /* WHRM (gated stub) */
#endif
extern const struct max32664_variant_ops max32664c_ops;  /* WHRM+WSpO2 */
extern const struct max32664_variant_ops max32664d_ops;  /* BPT */

/**
 * @brief Get variant ops structure for detected variant
 *
 * @param variant Detected variant type
 * @return Pointer to ops structure, or NULL if unknown
 */
const struct max32664_variant_ops *max32664_get_variant_ops(enum max32664_variant variant);

#if IS_ENABLED(CONFIG_MAX32664_MODE_HUB)
/**
 * @brief Force hub output mode 0x02 (algorithm-only) and clear runtime FIFO size
 *
 * Called from variant-C FIFO parsing when extended (mode 0x03) frames do not
 * contain any plausible 16-byte WHRM block at the expected offset — some GWEC
 * firmware builds use a different packing than CONFIG_MAX32664_HUB_MODE03_FRAME_BYTES.
 */
int max32664_hub_force_output_mode_algo_only(const struct device *dev);
#endif

/* ============================================================================
 * Common Internal Helper Functions (implemented in max32664.c)
 * Exported for use by variant-specific implementations
 * ============================================================================ */

/**
 * @brief Low-level I2C transmit with hub protocol
 *
 * Uses hub protocol with combined write + read transaction.
 * tx_buf should contain: [family, index, optional_data...]
 * rx_buf receives: [status, optional_response...]
 *
 * @param dev Pointer to device structure
 * @param tx_buf Transmit buffer (family + index + data)
 * @param tx_len Transmit buffer length
 * @param rx_buf Receive buffer for status + response data
 * @param rx_len Receive buffer length (minimum 1 for status)
 * @param delay_ms Delay after write before read (command processing time)
 * @return 0 on success, negative errno on failure
 */
int max32664_i2c_transmit(const struct device *dev,
			   uint8_t *tx_buf, uint8_t tx_len,
			   uint8_t *rx_buf, uint8_t rx_len,
			   uint16_t delay_ms);

/**
 * @brief I2C transmit with automatic retry on 0xFE (hub busy)
 *
 * @param dev Pointer to device structure
 * @param tx_buf Transmit buffer (family + index + data)
 * @param tx_len Transmit buffer length
 * @param rx_buf Receive buffer for status + response data
 * @param rx_len Receive buffer length (minimum 1 for status)
 * @param delay_ms Delay after write before read
 * @param max_retries Maximum retry attempts (0 = no retries)
 * @return 0 on success, negative errno on failure
 */
int max32664_i2c_transmit_retry(const struct device *dev,
				 uint8_t *tx_buf, uint8_t tx_len,
				 uint8_t *rx_buf, uint8_t rx_len,
				 uint16_t delay_ms,
				 uint8_t max_retries);

/**
 * @brief Coarse-grained bus lock for multi-command atomic sequences.
 *
 * The per-call mutex inside max32664_i2c_transmit() already keeps any
 * single hub command (write+delay+read) atomic. Callers that must also
 * keep a *group* of commands atomic (e.g. read 6 accel registers + feed
 * one sample) should wrap the sequence with bus_lock/bus_unlock to
 * prevent another thread's command from interleaving between the
 * individual transmits in the sequence.
 *
 * The mutex is recursive, so pre-locking does not deadlock the inner
 * per-transaction lock. Always pair every lock with exactly one unlock.
 */
void max32664_bus_lock(const struct device *dev);
void max32664_bus_unlock(const struct device *dev);

/**
 * AN6924 MFIO IRQ wake: five dummy writes of 0x00 to I2C addr 0x00.
 * Caller should hold the bus lock. No-op of GPIO reconfiguration.
 */
int max32664_mfio_irq_wake(const struct device *dev);

/**
 * Begin/end MFIO ownership for a multi-I2C hub transaction.
 * Caller must hold bus_lock. In Normal mode, MFIO stays asserted low
 * until end; in IRQ mode, performs IRQ wake once.
 */
int max32664_mfio_txn_begin(const struct device *dev, bool *held_as_output);
void max32664_mfio_txn_end(const struct device *dev, bool held_as_output);

/**
 * @brief Read from hub register (sensor/algorithm status)
 *
 * Uses Family 0x41 (read register) protocol.
 *
 * @param dev Pointer to device structure
 * @param sensor_idx Sensor index byte
 * @param reg_addr Register address byte
 * @param reg_value Pointer to store read value
 * @return 0 on success, negative errno on failure
 */
int max32664_read_sensor_reg(const struct device *dev,
			       uint8_t sensor_idx, uint8_t reg_addr,
			       uint8_t *reg_value);

/**
 * @brief Write to hub register (sensor configuration)
 *
 * Uses Family 0x40 (write register) protocol.
 *
 * @param dev Pointer to device structure
 * @param sensor_idx Sensor index byte
 * @param reg_addr Register address byte
 * @param reg_value Value to write
 * @return 0 on success, negative errno on failure
 */
int max32664_write_sensor_reg(const struct device *dev,
			       uint8_t sensor_idx, uint8_t reg_addr,
			       uint8_t reg_value);

/**
 * @brief Burst-read multiple bytes from a sensor FIFO register
 *
 * Optimized for high-throughput FIFO draining in PASSTHROUGH mode.
 * ~5x faster than calling max32664_read_sensor_reg() in a loop.
 *
 * @param dev         MAX32664 hub device
 * @param sensor_idx  Sensor index (0x00 for MAX86141, 0x04 for accel)
 * @param fifo_reg    FIFO data register address
 * @param out_buf     Output buffer for read bytes
 * @param num_bytes   Number of bytes to read
 * @return 0 on success, negative error code on failure
 */
int max32664_read_fifo_burst(const struct device *dev,
			     uint8_t sensor_idx,
			     uint8_t fifo_reg,
			     uint8_t *out_buf,
			     uint16_t num_bytes);

/**
 * @brief Stop all running algorithms and sensors
 *
 * Used when transitioning to RAW mode or before firmware update.
 *
 * @param dev Pointer to device structure
 * @return 0 on success, negative errno on failure
 */
int max32664_stop_algorithms_and_sensors(const struct device *dev);

/**
 * @brief Get hub status byte
 *
 * @param dev Pointer to device structure
 * @param status Pointer to store status byte
 * @return 0 on success, negative errno on failure
 */
int max32664_get_hub_status(const struct device *dev, uint8_t *status);

/**
 * @brief Get FIFO sample count
 *
 * @param dev Pointer to device structure
 * @param count Pointer to store sample count
 * @return 0 on success, negative errno on failure
 */
int max32664_get_fifo_count(const struct device *dev, uint8_t *count);

/**
 * @brief Parse status byte into individual flags
 *
 * @param status Status byte from hub
 * @param err0 Pointer to store ERR0 flag (can be NULL)
 * @param data_rdy Pointer to store DataRdyInt flag (can be NULL)
 * @param fifo_out_ovr Pointer to store FifoOutOvrInt flag (can be NULL)
 * @param fifo_in_ovr Pointer to store FifoInOvrInt flag (can be NULL)
 * @param host_accel_uf Pointer to store HostAccelUfInt flag (can be NULL)
 */
void max32664_parse_status(uint8_t status,
			    bool *err0, bool *data_rdy,
			    bool *fifo_out_ovr, bool *fifo_in_ovr,
			    bool *host_accel_uf);

/**
 * @brief Configure AGC (Auto Gain Control) algorithm
 *
 * Used by variants A and D (MaximFast and BPT).
 * Family 0x50, Index 0x00 (AGC algorithm configuration).
 *
 * @param dev Pointer to device structure
 * @param target_percent AGC target % of full-scale ADC (0-100)
 * @param step_size AGC step size % (0-100)
 * @param sensitivity AGC sensitivity % (0-100)
 * @param samples_avg Samples to average (0-255)
 * @return 0 on success, negative errno on failure
 */
int max32664_configure_agc(const struct device *dev,
			    uint8_t target_percent,
			    uint8_t step_size,
			    uint8_t sensitivity,
			    uint8_t samples_avg);

/**
 * @brief Configure WHRM (Wrist Heart Rate Monitor) mode
 *
 * Used by variants B and C (WHRM and WHRM+WSpO2).
 * Family 0x52, Index 0x03 (WHRM mode configuration).
 *
 * Modes:
 * - 0x00: Disabled
 * - 0x01: Basic mode (minimal motion rejection)
 * - 0x02: Standard mode (balanced)
 * - 0x03: Aggressive mode (maximum motion rejection)
 *
 * @param dev Pointer to device structure
 * @param mode WHRM mode (0x00-0x03)
 * @return 0 on success, negative errno on failure
 */
int max32664_configure_whrm_mode(const struct device *dev, uint8_t mode);

/* ============================================================================
 * AFE Access Functions (implemented in max32664_afe.c)
 * ============================================================================ */

/**
 * @brief Read AFE register via hub passthrough
 */
int max32664_afe_read_reg(const struct device *dev, uint8_t reg_addr, uint8_t *value);

/**
 * @brief Write AFE register via hub passthrough
 */
int max32664_afe_write_reg(const struct device *dev, uint8_t reg_addr, uint8_t value);

/**
 * @brief Read raw PPG samples from AFE FIFO
 */
int max32664_afe_read_fifo(const struct device *dev,
			   struct max32664_ppg_sample *samples,
			   size_t max_samples,
			   size_t *num_samples);

/**
 * @brief Configure AFE LED current
 */
int max32664_afe_set_led_current(const struct device *dev, uint8_t led, uint8_t current_ma);

/**
 * @brief Get AFE LED current
 */
int max32664_afe_get_led_current(const struct device *dev, uint8_t led, uint8_t *current_ma);

/**
 * @brief Get detected AFE type (WHOAMI value)
 */
uint8_t max32664_afe_get_type(const struct device *dev);

/**
 * @brief Check if AFE is MAX86141 type
 */
bool max32664_afe_is_max86141(const struct device *dev);

/**
 * @brief Check if AFE is MAX30101 type
 */
bool max32664_afe_is_max30101(const struct device *dev);

/* ============================================================================
 * Accelerometer Access Functions (implemented in max32664_accel.c)
 * ============================================================================ */

/**
 * @brief Read accelerometer register via hub passthrough
 */
int max32664_accel_read_reg(const struct device *dev, uint8_t reg_addr, uint8_t *value);

/**
 * @brief Write accelerometer register via hub passthrough
 */
int max32664_accel_write_reg(const struct device *dev, uint8_t reg_addr, uint8_t value);

/**
 * @brief Read accelerometer sample
 */
int max32664_accel_read(const struct device *dev, struct max32664_accel_sample *sample);

/* Feeder start/stop/is_running — declared in public max32664.h (included above). */

/**
 * @brief Configure accelerometer full-scale range
 */
int max32664_accel_set_range(const struct device *dev, uint8_t range_g);

/**
 * @brief Configure accelerometer output data rate
 */
int max32664_accel_set_odr(const struct device *dev, uint16_t odr_hz);

/**
 * @brief Get accelerometer WHOAMI
 */
uint8_t max32664_accel_get_whoami(const struct device *dev);

/**
 * @brief Feed external accelerometer data to hub (legacy API)
 */
int max32664_feed_accel(const struct device *dev, int16_t x_mg, int16_t y_mg, int16_t z_mg);

/* ============================================================================
 * Init/probe helpers (implemented in max32664_init.c)
 * ============================================================================ */

int max32664_init_regulator_power_cycle(const struct device *dev);
int max32664_init_software_reset(const struct device *dev);
int max32664_init_read_mcu_type(const struct device *dev, uint8_t *mcu_type);
int max32664_init_read_fw_version(const struct device *dev,
				 uint8_t *major, uint8_t *minor, uint8_t *patch);
int max32664_init_configure_gpios(const struct max32664_config *cfg);
int max32664_init_reset_sequence(const struct max32664_config *cfg);
int max32664_init_check_device_mode(const struct device *dev, uint8_t *mode_out);
int max32664_init_read_and_log_mcu_type(const struct device *dev, struct max32664_data *data);
int max32664_init_read_and_log_fw_version(const struct device *dev,
					  struct max32664_data *data,
					  uint8_t *major, uint8_t *minor, uint8_t *patch);
int max32664_init_read_hardware_whoami(const struct device *dev, struct max32664_data *data);
int max32664_init_detect_and_log_variant(const struct device *dev, struct max32664_data *data);
int max32664_init_variant_init(const struct device *dev, struct max32664_data *data);

/* ============================================================================
 * Runtime helpers (implemented in max32664_runtime.c)
 * ============================================================================ */

int max32664_runtime_read_output_mode(const struct device *dev, uint8_t *mode_out);
int max32664_runtime_try_output_mode(const struct device *dev,
				     uint8_t preferred_mode,
				     uint8_t fallback_mode);
#if IS_ENABLED(CONFIG_MAX32664_MODE_HUB)
int max32664_runtime_request_hub_output_mode(const struct device *dev);
int max32664_runtime_force_output_mode_algo_only(const struct device *dev);
#endif
#if !defined(CONFIG_MAX32664_MODE_RAW) && !defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
int max32664_runtime_hub_start_sampling(const struct device *dev);
int max32664_runtime_hub_stop_sampling(const struct device *dev);
#endif

/* ============================================================================
 * Sampling/data-path helpers (implemented in max32664_sampling.c)
 * ============================================================================ */

#if !defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
void max32664_sampling_poll_work_handler(struct k_work *work);
#endif

#if !defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
void max32664_sampling_interrupt_work_handler(struct k_work *work);
void max32664_sampling_gpio_interrupt_handler(const struct device *gpio_dev,
					      struct gpio_callback *cb,
					      uint32_t pins);
#endif

#if !defined(CONFIG_MAX32664_MODE_RAW) && !defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
int max32664_sampling_parse_hub_data(const struct device *dev);
int max32664_sampling_channel_get_hub(const struct device *dev,
					     enum sensor_channel chan,
					     struct sensor_value *val);
#endif

/* ============================================================================
 * RAW mode (implemented in max32664_raw.c)
 * ============================================================================ */

#if defined(CONFIG_MAX32664_MODE_RAW)
/**
 * @brief Initialize MAX32664 in RAW mode
 *
 * Puts hub in OUTPUT_MODE_PAUSE and enables passthrough access
 * to underlying MAX86141 and LIS2DS12 sensors while keeping
 * hub enabled as an I2C bridge.
 *
 * @param dev MAX32664 device
 * @return 0 on success, negative error code on failure
 */
int max32664_raw_mode_init(const struct device *dev);

/**
 * @brief Configure MAX86141 PPG sensor in RAW mode
 *
 * @param dev MAX32664 device
 * @return 0 on success, negative error code on failure
 */
int max32664_raw_configure_ppg(const struct device *dev);

/**
 * @brief Configure LIS2DS12 accelerometer in RAW mode
 *
 * @param dev MAX32664 device
 * @return 0 on success, negative error code on failure
 */
int max32664_raw_configure_accel(const struct device *dev);

/**
 * @brief Read PPG samples from MAX86141 FIFO in RAW mode
 *
 * Reads from hub FIFO (Family 0x12)
 *
 * @param dev MAX32664 device
 * @param ir_value Pointer to store IR sample (19-bit ADC value)
 * @param red_value Pointer to store Red sample (19-bit ADC value)
 * @return Number of samples read (0 or 1), or negative error code
 */
int max32664_raw_read_ppg(const struct device *dev,
			  uint32_t *ir_value, uint32_t *red_value);

/**
 * @brief Read PPG samples (IR / Red / Green)
 *
 * @param dev MAX32664 device
 * @param ir_value Pointer to store IR sample
 * @param red_value Pointer to store Red sample
 * @param green_value Pointer to store Green sample (optional)
 * @return Number of samples read (0 or 1), or negative error code
 */
int max32664_raw_read_ppg_ex(const struct device *dev,
			    uint32_t *ir_value, uint32_t *red_value,
			    uint32_t *green_value);

/**
 * @brief Drain the hub output FIFO into @p out (up to @p out_cap samples).
 *
 * Performs one FIFO count query, pops every pending frame (discarding settle
 * and invalid entries), and returns all valid IR/Red/Green triplets. Call
 * repeatedly until 0 is returned to empty the hub FIFO — matches max3010x_ppg
 * batching.
 *
 * @return Number of samples written (0 if empty), or negative errno.
 */
int max32664_raw_drain_ppg(const struct device *dev,
			   struct max32664_ppg_sample *out,
			   size_t out_cap);

/**
 * @brief Reset RAW per-session runtime state
 *
 * Clears fallback toggles, counters, cached report-size, and previous-sample
 * guards so each new measurement starts with a clean reader state.
 */
void max32664_raw_reset_session_state(const struct device *dev);

/**
 * @brief Read acceleration from LIS2DS12 in RAW mode
 *
 * Reads from hub FIFO (Family 0x12)
 *
 * @param dev MAX32664 device
 * @param x Pointer to store X acceleration (milli-g)
 * @param y Pointer to store Y acceleration (milli-g)
 * @param z Pointer to store Z acceleration (milli-g)
 * @return 0 on success, negative error code on failure
 */
int max32664_raw_read_accel(const struct device *dev,
			    int16_t *x, int16_t *y, int16_t *z);
#endif /* CONFIG_MAX32664_MODE_RAW */

#if defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
/**
 * @brief Initialize MAX32664 in PASSTHROUGH mode
 *
 * Disables hub algorithms, AFE management, and sets output to PAUSE.
 * Enables direct register access to MAX86141 and LIS2DS12.
 *
 * @param dev MAX32664 device
 * @return 0 on success, negative error code on failure
 */
int max32664_passthrough_mode_init(const struct device *dev);

/**
 * @brief Enable passthrough mode (disable hub)
 *
 * Stops algorithms, disables AFE management, sets PAUSE mode.
 *
 * @param dev MAX32664 device
 * @return 0 on success, negative error code on failure
 */
int max32664_passthrough_mode_enable(const struct device *dev);

/**
 * @brief Configure MAX86141 PPG sensor via passthrough
 *
 * @param dev MAX32664 device
 * @return 0 on success, negative error code on failure
 */
int max32664_passthrough_configure_ppg(const struct device *dev);

/**
 * @brief Configure LIS2DS12 accelerometer via passthrough
 *
 * @param dev MAX32664 device
 * @return 0 on success, negative error code on failure
 */
int max32664_passthrough_configure_accel(const struct device *dev);

/**
 * @brief Read PPG samples directly from MAX86141 via passthrough
 *
 * @param dev MAX32664 device
 * @param ir Pointer to store IR channel value
 * @param red Pointer to store Red channel value
 * @return Number of samples read, or negative errno on failure
 */
int max32664_passthrough_read_ppg(const struct device *dev, uint32_t *ir, uint32_t *red);

/**
 * @brief Read PPG samples with green channel from MAX86141 via passthrough
 *
 * @param dev MAX32664 device
 * @param ir_value Pointer to store IR sample
 * @param red_value Pointer to store Red sample
 * @param green_value Pointer to store Green sample (may be NULL)
 * @return Number of samples read, or negative errno on failure
 */
int max32664_passthrough_read_ppg_ex(const struct device *dev,
				     uint32_t *ir_value, uint32_t *red_value,
				     uint32_t *green_value);

/**
 * @brief Read accelerometer data directly from LIS2DS12 via passthrough
 *
 * @param dev MAX32664 device
 * @param x Pointer to store X-axis value
 * @param y Pointer to store Y-axis value
 * @param z Pointer to store Z-axis value
 * @return 0 on success, negative errno on failure
 */
int max32664_passthrough_read_accel(const struct device *dev, int16_t *x, int16_t *y, int16_t *z);

#endif /* CONFIG_MAX32664_MODE_PASSTHROUGH */

#ifdef __cplusplus
}
#endif

#endif /* MAX32664_PRIV_H_ */
