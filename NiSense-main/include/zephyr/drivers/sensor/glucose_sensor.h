/**
 * @file glucose_sensor.h
 * @brief Glucose sensor driver API
 *
 * Public API for non-invasive glucose measurement sensor driver.
 * The driver controls an IR LED and reads glucose signal from a photodiode
 * via ADC, applying calibration and filtering algorithms.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_SENSOR_GLUCOSE_SENSOR_H_
#define ZEPHYR_INCLUDE_DRIVERS_SENSOR_GLUCOSE_SENSOR_H_

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
struct glucose_algorithm_details;

/**
 * @brief Glucose sensor result structure
 */
struct glucose_sensor_result {
	/** Glucose concentration in mg/dL */
	float glucose_mg_dl;
	
	/** Glucose concentration in mmol/L (alternative unit) */
	float glucose_mmol_l;
	
	/** Fasting insulin in μIU/mL (for HOMA-IR calculation) */
	float fasting_insulin_uiu_ml;
	
	/** Signal quality metrics */
	struct {
		/** Signal-to-noise ratio in dB */
		float snr_db;
		
		/** Sample variance (indicates signal stability) */
		float variance;
		
		/** Peak-to-peak amplitude in mV */
		float peak_to_peak_mv;
		
		/** Quality score (0.0 to 1.0, higher is better) */
		float quality_score;
	} quality;
	
	/** Measurement status flags */
	uint32_t flags;
	
	/** Timestamp when measurement was taken (Unix epoch in seconds) */
	int64_t timestamp;
	
	/** Raw ADC samples (for logging and debugging) */
	uint16_t *raw_samples;
	
	/** Per-sample timestamps in microseconds */
	int64_t *sample_timestamps;
	
	/** Calibrated voltage values in millivolts (per sample) */
	float *voltages_mv;
	
	/** Algorithm intermediate values (for detailed CSV logging) */
	struct glucose_algorithm_details *algorithm_details;
	
	/** Number of raw samples */
	uint16_t sample_count;
};

/** Measurement status flags */
#define GLUCOSE_SENSOR_FLAG_VALID          BIT(0)  /**< Measurement is valid */
#define GLUCOSE_SENSOR_FLAG_LOW_SNR        BIT(1)  /**< Low signal-to-noise ratio */
#define GLUCOSE_SENSOR_FLAG_HIGH_VARIANCE  BIT(2)  /**< High sample variance */
#define GLUCOSE_SENSOR_FLAG_OFFSET_ERROR   BIT(3)  /**< Offset calibration error */
#define GLUCOSE_SENSOR_FLAG_SATURATION     BIT(4)  /**< ADC saturation detected */
#define GLUCOSE_SENSOR_FLAG_ALGO_ERROR     BIT(5)  /**< Glucose/HOMA algorithm failed */
#define GLUCOSE_SENSOR_FLAG_OUT_OF_WINDOW  BIT(6)  /**< ADC mean outside the 50-80% recommended
							 operating window (see GLUCOSE_AFE_REVIEW.md) —
							 not rejected, but not to be trusted the way an
							 in-window reading is. */

/**
 * @brief Custom trigger types for glucose sensor (extend standard sensor_trigger_type)
 * 
 * These values extend the standard Zephyr sensor trigger types and are used
 * with the standard sensor_trigger_set() API.
 */
/** Trigger fires after each individual ADC sample */
#define GLUCOSE_TRIG_SAMPLE_READY        SENSOR_TRIG_PRIV_START

/** Trigger fires when all samples are complete and processed */
#define GLUCOSE_TRIG_MEASUREMENT_COMPLETE (SENSOR_TRIG_PRIV_START + 1)

/**
 * @brief Per-sample callback data
 */
struct glucose_sample_data {
	/** Sample number (0 to sample_count-1) */
	uint16_t sample_number;
	
	/** Total number of samples in measurement */
	uint16_t total_samples;
	
	/** Raw ADC value */
	uint16_t raw_adc_value;
	
	/** Calibrated voltage in millivolts */
	float voltage_mv;
	
	/** Timestamp in seconds (Unix epoch or uptime) */
	int64_t timestamp_sec;
};

/**
 * @brief Glucose sensor measurement completion callback
 *
 * @param dev Pointer to glucose sensor device
 * @param result Pointer to measurement result structure
 * @param user_data User-provided callback data
 */
typedef void (*glucose_sensor_callback_t)(const struct device *dev,
					  const struct glucose_sensor_result *result,
					  void *user_data);

/**
 * @brief Glucose sensor driver API
 */
__subsystem struct glucose_sensor_driver_api {
	/**
	 * @brief Trigger a glucose measurement (asynchronous)
	 *
	 * Starts a measurement sequence:
	 * 1. Activates IR LED via GPIO
	 * 2. Waits for stabilization delay
	 * 3. Captures configured number of ADC samples
	 * 4. Deactivates IR LED
	 * 5. Processes samples through calibration and filtering
	 * 6. Invokes user callback with result
	 *
	 * @param dev Pointer to glucose sensor device
	 * @return 0 on success, negative errno on error
	 */
	int (*trigger_measurement)(const struct device *dev);
	
	/**
	 * @brief Register callback for measurement completion
	 *
	 * @param dev Pointer to glucose sensor device
	 * @param callback Callback function to invoke on measurement completion
	 * @param user_data User data to pass to callback
	 * @return 0 on success, negative errno on error
	 */
	int (*set_callback)(const struct device *dev,
			    glucose_sensor_callback_t callback,
			    void *user_data);
	
	/**
	 * @brief Get latest measurement result (synchronous)
	 *
	 * Retrieves the result from the most recent measurement. If no
	 * measurement has been completed yet, returns an error.
	 *
	 * @param dev Pointer to glucose sensor device
	 * @param result Pointer to result structure to fill
	 * @return 0 on success, negative errno on error
	 */
	int (*get_result)(const struct device *dev,
			  struct glucose_sensor_result *result);
	
	/**
	 * @brief Configure runtime parameters
	 *
	 * Allows runtime configuration of sample count, delay, etc.
	 * Changes take effect on next measurement.
	 *
	 * @param dev Pointer to glucose sensor device
	 * @param num_samples Number of ADC samples per measurement
	 * @param delay_ms Stabilization delay in milliseconds
	 * @return 0 on success, negative errno on error
	 */
	int (*configure)(const struct device *dev,
			 uint16_t num_samples,
			 uint16_t delay_ms);
	
	/**
	 * @brief Perform calibration
	 *
	 * Measures dark current offset and optionally performs gain
	 * calibration with known glucose standard.
	 *
	 * @param dev Pointer to glucose sensor device
	 * @param calibration_voltage_mv Known glucose standard voltage (mV),
	 *                                or 0 for dark current only
	 * @return 0 on success, negative errno on error
	 */
	int (*calibrate)(const struct device *dev,
			 float calibration_voltage_mv);
	
	/**
	 * @brief Get current measurement state
	 *
	 * Returns the current state of the measurement process.
	 *
	 * @param dev Pointer to glucose sensor device
	 * @return Current state (enum glucose_sensor_state value)
	 */
	int (*get_state)(const struct device *dev);
	
	/**
	 * @brief Get current configuration
	 *
	 * Returns the current runtime configuration parameters.
	 *
	 * @param dev Pointer to glucose sensor device
	 * @param num_samples Pointer to store number of samples (NULL to skip)
	 * @param delay_ms Pointer to store LED delay in ms (NULL to skip)
	 * @return 0 on success, negative errno on error
	 */
	int (*get_config)(const struct device *dev,
			  uint16_t *num_samples,
			  uint16_t *delay_ms);
	
	/**
	 * @brief Set trigger handler (Zephyr sensor API compatible)
	 *
	 * Supports GLUCOSE_TRIG_SAMPLE_READY and GLUCOSE_TRIG_MEASUREMENT_COMPLETE
	 *
	 * @param dev Pointer to glucose sensor device
	 * @param trig Trigger configuration
	 * @param handler Handler function (NULL to disable)
	 * @return 0 on success, negative errno on error
	 */
	int (*trigger_set)(const struct device *dev,
			   const struct sensor_trigger *trig,
			   sensor_trigger_handler_t handler);
	
	/**
	 * @brief Get latest per-sample data
	 *
	 * Retrieves data from the most recent sample taken.
	 *
	 * @param dev Pointer to glucose sensor device
	 * @param sample Pointer to sample structure to fill
	 * @return 0 on success, negative errno on error
	 */
	int (*get_sample_data)(const struct device *dev,
			       struct glucose_sample_data *sample);
	
	/**
	 * @brief Set fasting insulin for HOMA-IR calculation
	 *
	 * Updates the fasting insulin level used in insulin resistance calculations.
	 *
	 * @param dev Pointer to glucose sensor device
	 * @param insulin_uiu_ml Fasting insulin in μIU/mL (must be >= 0)
	 * @return 0 on success, negative errno on error
	 */
	int (*set_fasting_insulin)(const struct device *dev,
				   float insulin_uiu_ml);
	
	/**
	 * @brief Get current fasting insulin value
	 *
	 * Retrieves the fasting insulin level used in calculations.
	 *
	 * @param dev Pointer to glucose sensor device
	 * @param insulin_uiu_ml Pointer to store insulin value
	 * @return 0 on success, negative errno on error
	 */
	int (*get_fasting_insulin)(const struct device *dev,
				   float *insulin_uiu_ml);

	/**
	 * @brief Abort an in-progress measurement
	 *
	 * Cancels pending work items, turns off the IR LED, and resets state
	 * to IDLE. Safe to call when already idle (returns 0).
	 *
	 * @param dev Pointer to glucose sensor device
	 * @return 0 on success, negative errno on error
	 */
	int (*abort_measurement)(const struct device *dev);
};

/**
 * @brief Trigger a glucose measurement
 *
 * @param dev Pointer to glucose sensor device
 * @return 0 on success, negative errno on error
 */
#if defined(CONFIG_USERSPACE)
__syscall int glucose_sensor_trigger_measurement(const struct device *dev);
#endif

static inline int z_impl_glucose_sensor_trigger_measurement(const struct device *dev)
{
	const struct glucose_sensor_driver_api *api =
		(const struct glucose_sensor_driver_api *)dev->api;
	
	return api->trigger_measurement(dev);
}

/**
 * @brief Register measurement completion callback
 *
 * @param dev Pointer to glucose sensor device
 * @param callback Callback function
 * @param user_data User data
 * @return 0 on success, negative errno on error
 */
#if defined(CONFIG_USERSPACE)
__syscall int glucose_sensor_set_callback(const struct device *dev,
					   glucose_sensor_callback_t callback,
					   void *user_data);
#endif

static inline int z_impl_glucose_sensor_set_callback(const struct device *dev,
						      glucose_sensor_callback_t callback,
						      void *user_data)
{
	const struct glucose_sensor_driver_api *api =
		(const struct glucose_sensor_driver_api *)dev->api;
	
	return api->set_callback(dev, callback, user_data);
}

/**
 * @brief Get latest measurement result
 *
 * @param dev Pointer to glucose sensor device
 * @param result Pointer to result structure
 * @return 0 on success, negative errno on error
 */
#if defined(CONFIG_USERSPACE)
__syscall int glucose_sensor_get_result(const struct device *dev,
					struct glucose_sensor_result *result);
#endif

static inline int z_impl_glucose_sensor_get_result(const struct device *dev,
						    struct glucose_sensor_result *result)
{
	const struct glucose_sensor_driver_api *api =
		(const struct glucose_sensor_driver_api *)dev->api;
	
	return api->get_result(dev, result);
}

/**
 * @brief Configure runtime parameters
 *
 * @param dev Pointer to glucose sensor device
 * @param num_samples Number of samples
 * @param delay_ms Delay in milliseconds
 * @return 0 on success, negative errno on error
 */
#if defined(CONFIG_USERSPACE)
__syscall int glucose_sensor_configure(const struct device *dev,
				       uint16_t num_samples,
				       uint16_t delay_ms);
#endif

static inline int z_impl_glucose_sensor_configure(const struct device *dev,
						    uint16_t num_samples,
						    uint16_t delay_ms)
{
	const struct glucose_sensor_driver_api *api =
		(const struct glucose_sensor_driver_api *)dev->api;
	
	return api->configure(dev, num_samples, delay_ms);
}

/**
 * @brief Perform calibration
 *
 * @param dev Pointer to glucose sensor device
 * @param calibration_voltage_mv Calibration voltage (mV), or 0 for dark current
 * @return 0 on success, negative errno on error
 */
#if defined(CONFIG_USERSPACE)
__syscall int glucose_sensor_calibrate(const struct device *dev,
				       float calibration_voltage_mv);
#endif

static inline int z_impl_glucose_sensor_calibrate(const struct device *dev,
						   float calibration_voltage_mv)
{
	const struct glucose_sensor_driver_api *api =
		(const struct glucose_sensor_driver_api *)dev->api;
	
	return api->calibrate(dev, calibration_voltage_mv);
}

/**
 * @brief Get current measurement state
 *
 * @param dev Pointer to glucose sensor device
 * @return Current state (enum glucose_sensor_state value)
 */
#if defined(CONFIG_USERSPACE)
__syscall int glucose_sensor_get_state(const struct device *dev);
#endif

static inline int z_impl_glucose_sensor_get_state(const struct device *dev)
{
	const struct glucose_sensor_driver_api *api =
		(const struct glucose_sensor_driver_api *)dev->api;
	
	return api->get_state(dev);
}

/**
 * @brief Get current configuration
 *
 * @param dev Pointer to glucose sensor device
 * @param num_samples Pointer to store number of samples (NULL to skip)
 * @param delay_ms Pointer to store LED delay in ms (NULL to skip)
 * @return 0 on success, negative errno on error
 */
#if defined(CONFIG_USERSPACE)
__syscall int glucose_sensor_get_config(const struct device *dev,
					uint16_t *num_samples,
					uint16_t *delay_ms);
#endif

static inline int z_impl_glucose_sensor_get_config(const struct device *dev,
						    uint16_t *num_samples,
						    uint16_t *delay_ms)
{
	const struct glucose_sensor_driver_api *api =
		(const struct glucose_sensor_driver_api *)dev->api;
	
	return api->get_config(dev, num_samples, delay_ms);
}

/**
 * @brief Set trigger handler
 *
 * @param dev Pointer to glucose sensor device
 * @param trig Trigger configuration
 * @param handler Handler function
 * @return 0 on success, negative errno on error
 */
#if defined(CONFIG_USERSPACE)
__syscall int glucose_sensor_trigger_set(const struct device *dev,
					 const struct sensor_trigger *trig,
					 sensor_trigger_handler_t handler);
#endif

static inline int z_impl_glucose_sensor_trigger_set(const struct device *dev,
						     const struct sensor_trigger *trig,
						     sensor_trigger_handler_t handler)
{
	const struct glucose_sensor_driver_api *api =
		(const struct glucose_sensor_driver_api *)dev->api;
	
	return api->trigger_set(dev, trig, handler);
}

/**
 * @brief Get latest per-sample data
 *
 * @param dev Pointer to glucose sensor device
 * @param sample Pointer to sample structure
 * @return 0 on success, negative errno on error
 */
#if defined(CONFIG_USERSPACE)
__syscall int glucose_sensor_get_sample_data(const struct device *dev,
					      struct glucose_sample_data *sample);
#endif

static inline int z_impl_glucose_sensor_get_sample_data(const struct device *dev,
							 struct glucose_sample_data *sample)
{
	const struct glucose_sensor_driver_api *api =
		(const struct glucose_sensor_driver_api *)dev->api;
	
	return api->get_sample_data(dev, sample);
}

#ifdef __cplusplus
}
#endif

/* Provide public API wrappers */
#if !defined(CONFIG_USERSPACE)
static inline int glucose_sensor_trigger_measurement(const struct device *dev)
{
	return z_impl_glucose_sensor_trigger_measurement(dev);
}

static inline int glucose_sensor_set_callback(const struct device *dev,
					       glucose_sensor_callback_t callback,
					       void *user_data)
{
	return z_impl_glucose_sensor_set_callback(dev, callback, user_data);
}

static inline int glucose_sensor_get_result(const struct device *dev,
					     struct glucose_sensor_result *result)
{
	return z_impl_glucose_sensor_get_result(dev, result);
}

static inline int glucose_sensor_configure(const struct device *dev,
					    uint16_t num_samples,
					    uint16_t delay_ms)
{
	return z_impl_glucose_sensor_configure(dev, num_samples, delay_ms);
}

static inline int glucose_sensor_calibrate(const struct device *dev,
					    float calibration_voltage_mv)
{
	return z_impl_glucose_sensor_calibrate(dev, calibration_voltage_mv);
}

static inline int glucose_sensor_get_state(const struct device *dev)
{
	return z_impl_glucose_sensor_get_state(dev);
}

static inline int glucose_sensor_get_config(const struct device *dev,
					    uint16_t *num_samples,
					    uint16_t *delay_ms)
{
	return z_impl_glucose_sensor_get_config(dev, num_samples, delay_ms);
}

static inline int glucose_sensor_trigger_set(const struct device *dev,
					      const struct sensor_trigger *trig,
					      sensor_trigger_handler_t handler)
{
	return z_impl_glucose_sensor_trigger_set(dev, trig, handler);
}

static inline int glucose_sensor_get_sample_data(const struct device *dev,
						  struct glucose_sample_data *sample)
{
	return z_impl_glucose_sensor_get_sample_data(dev, sample);
}

/**
 * @brief Set fasting insulin for HOMA-IR calculation
 *
 * @param dev Pointer to glucose sensor device
 * @param insulin_uiu_ml Fasting insulin in μIU/mL
 * @return 0 on success, negative errno on error
 */
#if defined(CONFIG_USERSPACE)
__syscall int glucose_sensor_set_fasting_insulin(const struct device *dev,
						  float insulin_uiu_ml);
#endif

static inline int z_impl_glucose_sensor_set_fasting_insulin(const struct device *dev,
							    float insulin_uiu_ml)
{
	const struct glucose_sensor_driver_api *api =
		(const struct glucose_sensor_driver_api *)dev->api;
	
	return api->set_fasting_insulin(dev, insulin_uiu_ml);
}

static inline int glucose_sensor_set_fasting_insulin(const struct device *dev,
						      float insulin_uiu_ml)
{
	return z_impl_glucose_sensor_set_fasting_insulin(dev, insulin_uiu_ml);
}

/**
 * @brief Get current fasting insulin value
 *
 * @param dev Pointer to glucose sensor device
 * @param insulin_uiu_ml Pointer to store insulin value
 * @return 0 on success, negative errno on error
 */
#if defined(CONFIG_USERSPACE)
__syscall int glucose_sensor_get_fasting_insulin(const struct device *dev,
						  float *insulin_uiu_ml);
#endif

static inline int z_impl_glucose_sensor_get_fasting_insulin(const struct device *dev,
							    float *insulin_uiu_ml)
{
	const struct glucose_sensor_driver_api *api =
		(const struct glucose_sensor_driver_api *)dev->api;
	
	return api->get_fasting_insulin(dev, insulin_uiu_ml);
}

static inline int glucose_sensor_get_fasting_insulin(const struct device *dev,
						      float *insulin_uiu_ml)
{
	return z_impl_glucose_sensor_get_fasting_insulin(dev, insulin_uiu_ml);
}

static inline int glucose_sensor_abort_measurement(const struct device *dev)
{
	const struct glucose_sensor_driver_api *api =
		(const struct glucose_sensor_driver_api *)dev->api;

	return api->abort_measurement(dev);
}

#else
/* Userspace: include syscall header */
#include <zephyr/syscalls/glucose_sensor.h>

static inline int glucose_sensor_get_sample_data(const struct device *dev,
						  struct glucose_sample_data *sample)
{
	return z_impl_glucose_sensor_get_sample_data(dev, sample);
}

static inline int glucose_sensor_set_fasting_insulin(const struct device *dev,
						      float insulin_uiu_ml)
{
	return z_impl_glucose_sensor_set_fasting_insulin(dev, insulin_uiu_ml);
}

static inline int glucose_sensor_get_fasting_insulin(const struct device *dev,
						      float *insulin_uiu_ml)
{
	return z_impl_glucose_sensor_get_fasting_insulin(dev, insulin_uiu_ml);
}

static inline int glucose_sensor_abort_measurement(const struct device *dev)
{
	const struct glucose_sensor_driver_api *api =
		(const struct glucose_sensor_driver_api *)dev->api;

	return api->abort_measurement(dev);
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_SENSOR_GLUCOSE_SENSOR_H_ */

