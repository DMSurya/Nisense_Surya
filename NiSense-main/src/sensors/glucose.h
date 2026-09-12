/* glucose.h – Glucose sensor measurement interface
 *
 * Provides simple interface to glucose sensor driver for testing
 * and integration with UI display.
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor/glucose_sensor.h>
#include <zephyr/logging/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize glucose sensor device
 *
 * @retval 0 Success
 * @retval -ENODEV Sensor device not ready
 */
int glucose_init(void);

/**
 * @brief Trigger a glucose measurement (async)
 *
 * @retval 0 Success
 * @retval negative errno on error
 */
int glucose_trigger_measurement(void);

/**
 * @brief Start glucose measurement from UI (handles USB disable)
 *
 * Disables USB if enabled, resets timer, then triggers measurement.
 * USB will be automatically re-enabled after measurement completes.
 *
 * @retval 0 Success
 * @retval negative errno on error
 */
int glucose_start_measurement_from_ui(void);

/**
 * @brief Get latest glucose measurement result
 *
 * @param[out] result Pointer to result structure
 * @retval 0 Success
 * @retval -ENODATA No measurement available yet
 */
int glucose_get_result(struct glucose_sensor_result *result);

/**
 * @brief Check if measurement is in progress
 *
 * @retval true Measurement in progress
 * @retval false Measurement idle
 */
bool glucose_is_measuring(void);

/**
 * @brief Abort an in-progress glucose measurement
 *
 * Cancels the active measurement, turns off the IR LED, and resets state
 * to IDLE. Safe to call when already idle (returns 0 immediately).
 *
 * @retval 0 Success (or already idle)
 * @retval -ENODEV Sensor not initialized
 * @retval negative errno on other error
 */
int glucose_abort_measurement(void);

/**
 * @brief Get current measurement state
 *
 * @retval >=0 Current state (0=IDLE, 1=LED_ON, 2=SAMPLING, 3=PROCESSING)
 * @retval -ENODEV Sensor not initialized
 */
int glucose_get_state(void);

/**
 * @brief Live glucose ADC sample progress during an active measurement.
 * @retval 0 Progress valid, -ENODATA if idle.
 */
int glucose_get_measurement_progress(uint16_t *taken, uint16_t *target);

/**
 * @brief Get current sensor configuration
 *
 * @param[out] num_samples Number of ADC samples (NULL to skip)
 * @param[out] delay_ms LED stabilization delay in ms (NULL to skip)
 * @retval 0 Success
 * @retval -ENODEV Sensor not initialized
 */
int glucose_get_config(uint16_t *num_samples, uint16_t *delay_ms);

/**
 * @brief Update sensor configuration for the next measurement
 *
 * @param num_samples ADC samples (10–500)
 * @param delay_ms Inter-sample / LED delay ms (100–10000)
 * @retval 0 Success
 * @retval -ENODEV Sensor not initialized
 * @retval -EINVAL Out of range
 */
int glucose_set_config(uint16_t num_samples, uint16_t delay_ms);

/**
 * @brief Set fasting insulin value for HOMA-IR calculation
 *
 * Updates the fasting insulin level (in μIU/mL) used for insulin resistance
 * calculations. This value is used in the HOMA-IR index calculation during
 * glucose measurements.
 *
 * @param insulin_uiu_ml Fasting insulin level in μIU/mL (must be > 0)
 * @retval 0 Success
 * @retval -EINVAL Invalid insulin value
 */
int glucose_set_fasting_insulin(float insulin_uiu_ml);

/**
 * @brief Get current fasting insulin value
 *
 * @param[out] insulin_uiu_ml Pointer to store current fasting insulin value
 * @retval 0 Success
 * @retval -EINVAL NULL pointer provided
 */
int glucose_get_fasting_insulin(float *insulin_uiu_ml);

/**
 * @brief Reset glucose sample timer
 * 
 * Call this when user manually triggers glucose measurement from UI.
 * This restarts the timer, ensuring next automatic measurement happens
 * 10 minutes from now.
 */
void glucose_reset_timer(void);

/**
 * @brief Check if periodic glucose timer is active
 * @return true if timer is running, false otherwise
 */
bool glucose_is_timer_active(void);

/**
 * @brief Common glucose measurement result handler
 *
 * Logs result, saves to CSV, plays beeps, and handles USB re-enable.
 * Used by both UI-triggered and RTC-scheduled measurements.
 *
 * @param result Measurement result
 * @param source Source of measurement ("UI" or "RTC" for logging)
 * @param reenable_usb Whether to re-enable USB after measurement
 */
void glucose_handle_result(struct glucose_sensor_result *result,
			   const char *source,
			   bool reenable_usb);

typedef void (*glucose_sample_callback_t)(const struct glucose_sample_data *sample,
					 void *user_data);

typedef void (*glucose_result_callback_t)(const struct glucose_sensor_result *result,
					 void *user_data);

/**
 * @brief Register a per-sample callback for glucose measurements
 *
 * @param cb Callback to invoke (set NULL to unregister)
 * @param user_data Caller-provided context
 * @retval 0 Success
 */
int glucose_register_sample_callback(glucose_sample_callback_t cb, void *user_data);

/**
 * @brief Register a completion callback for glucose measurements
 *
 * Multiple listeners are supported (health scheduler + UI). Pass NULL to clear
 * all listeners.
 *
 * @param cb Callback to invoke (set NULL to unregister all)
 * @param user_data Caller-provided context
 * @retval 0 Success
 * @retval -ENOSPC Listener table full
 */
int glucose_register_result_callback(glucose_result_callback_t cb, void *user_data);

/**
 * @brief Remove one glucose completion listener
 */
int glucose_unregister_result_callback(glucose_result_callback_t cb);

/**
 * @brief True when the active glucose prediction model is available.
 *
 * With file-based matrices, reflects validation at glucose_init (and any
 * subsequent glucose_model_revalidate()). Embedded matrices always ready.
 */
bool glucose_model_ready(void);

/** Re-run matrix file validation (no-op when matrices are embedded). */
int glucose_model_revalidate(void);

#ifdef __cplusplus
}
#endif

