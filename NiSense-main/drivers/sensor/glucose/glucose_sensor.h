/**
 * @file glucose_sensor.h
 * @brief Glucose sensor driver internal definitions
 *
 * Internal header for glucose sensor driver implementation.
 * This file should not be included by application code.
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_GLUCOSE_SENSOR_INTERNAL_H_
#define ZEPHYR_DRIVERS_SENSOR_GLUCOSE_SENSOR_INTERNAL_H_

#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

/* Include public API for full type definitions */
#include <zephyr/drivers/sensor/glucose_sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Driver-specific data structure */
struct glucose_sensor_data {
	/** Glucose sensor device (for accessing config in work handlers) */
	const struct device *dev;
	
	/** ADC channel specification */
	struct adc_dt_spec adc_channel;
	
	/** User callback function */
	glucose_sensor_callback_t callback;
	
	/** User callback data */
	void *user_data;
	
	/** Per-sample trigger handler (Zephyr sensor API) */
	sensor_trigger_handler_t sample_handler;
	const struct sensor_trigger *sample_trigger;
	
	/** Measurement complete trigger handler (Zephyr sensor API) */
	sensor_trigger_handler_t complete_handler;
	const struct sensor_trigger *complete_trigger;
	
	/** Latest per-sample data */
	struct glucose_sample_data latest_sample;
	
	/** Latest measurement result */
	struct glucose_sensor_result result;
	
	/** Measurement state */
	atomic_t state;
	
	/** Runtime configuration */
	struct {
		uint16_t num_samples;
		uint16_t delay_ms;
		uint16_t sample_rate_hz;
	} config;
	
	/** Calibration parameters */
	struct {
		float offset_mv;
		float gain;
	} calibration;
	
	/** Work queue for async operations */
	struct k_work_delayable measurement_work;
	
	/** Work queue for sample processing */
	struct k_work process_work;
	
	/** ADC samples buffer */
	uint16_t *samples;
	
	/** Filtered samples buffer */
	float *filtered_samples;
	
	/** Sample timestamps buffer (microseconds) */
	int64_t *sample_timestamps;
	
	/** Sample count */
	uint16_t sample_count;
};

/** Measurement states */
enum glucose_sensor_state {
	GLUCOSE_SENSOR_STATE_IDLE = 0,
	GLUCOSE_SENSOR_STATE_LED_ON,
	GLUCOSE_SENSOR_STATE_SAMPLING,
	GLUCOSE_SENSOR_STATE_ACQUISITION_COMPLETE,  /* Brief state after sampling */
	GLUCOSE_SENSOR_STATE_PROCESSING,
};

/** Driver-specific configuration structure */
struct glucose_sensor_config {
	/** IR LED GPIO specification */
	struct gpio_dt_spec ir_led_gpio;
	
	/** ADC channel specification */
	struct adc_dt_spec adc_channel;
	
	/** Device tree configuration */
	struct {
		uint16_t num_samples;
		uint16_t delay_ms;
		uint16_t sample_rate_hz;
		float calibration_offset_mv;
		float calibration_gain;
	} dt_config;
};

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_SENSOR_GLUCOSE_SENSOR_INTERNAL_H_ */

