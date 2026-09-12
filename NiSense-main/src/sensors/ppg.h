/* ppg.h - PPG measurement interface
 *
 * Provides helper functions for PPG subsystem access.
 * Wraps subsys/ppg_algo API for application convenience.
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/subsys/ppg_algo/ppg_algo.h>
#include <zephyr/logging/log.h>
#include "ppg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize PPG measurement system
 *
 * Initializes the PPG algorithm subsystem.
 *
 * @retval 0 Success
 * @retval negative errno on error
 */
int ppg_init(void);

/**
 * @brief Start PPG measurement
 * 
 * @param sample_count Number of samples to collect
 * @retval 0 Success
 * @retval negative errno on error
 */
/**
 * @brief Bypass VCNL3040 proximity guard for one measurement (debug / bench).
 *
 * When true, ppg_start_measurement() does not require skin contact and the
 * mid-run abort guard is disabled until cleared.
 */
void ppg_set_proximity_bypass(bool bypass);

/** @return true when the one-shot proximity bypass is active. */
bool ppg_get_proximity_bypass(void);

/**
 * @brief Persistently disable the proximity guard (bench / record / sched test).
 *
 * Unlike [ppg_set_proximity_bypass], this survives health-sched cycle cleanup.
 * When disabled, PPG starts without wear and auto-sched treats the device as worn.
 */
void ppg_set_proximity_disabled(bool disabled);

/** @return true when the persistent proximity disable is active. */
bool ppg_get_proximity_disabled(void);

int ppg_start_measurement(uint32_t sample_count);

/**
 * @brief Live sample progress for the active measurement.
 * @retval 0 Progress valid, -ENODATA if idle.
 */
int ppg_get_measurement_progress(uint16_t *taken, uint16_t *target);

/**
 * @brief Effective sample/frame target for the active measurement
 *
 * Mirrors ppg_algo_get_measurement_target_samples(). Use after a successful
 * ppg_start_measurement() so UI progress matches HUB-mode clamps.
 */
uint16_t ppg_get_measurement_target_samples(void);

/**
 * @brief Check whether a measurement is currently active
 *
 * @return true if active, false otherwise
 */
bool ppg_is_measurement_active(void);

/**
 * @brief Stop PPG measurement
 * 
 * @retval 0 Success
 * @retval negative errno on error
 */
int ppg_stop_measurement(void);

/**
 * @brief Get latest PPG measurement result
 *
 * @param[out] result Pointer to result structure
 * @retval 0 Success
 * @retval -ENODATA No measurement available yet
 */
int ppg_get_result(struct ppg_algo_result *result);

/**
 * @brief Register sample callback
 * 
 * @param handler Callback function
 * @retval 0 Success
 * @retval negative errno on error
 */
int ppg_register_sample_callback(ppg_trigger_handler_t handler);

/**
 * @brief Register measurement-complete callback
 * 
 * @param handler Callback function
 * @retval 0 Success
 * @retval negative errno on error
 */
int ppg_register_complete_callback(ppg_trigger_handler_t handler);

/**
 * @brief Remove a previously registered measurement-complete callback
 *
 * @param handler Same function pointer passed to ppg_register_complete_callback()
 * @retval 0 Success
 * @retval -ENOENT Handler was not registered
 */
int ppg_unregister_complete_callback(ppg_trigger_handler_t handler);

/**
 * @brief Remove a previously registered per-sample callback
 */
int ppg_unregister_sample_callback(ppg_trigger_handler_t handler);

/* ========== UTILITY FUNCTIONS ========== */

/**
 * @brief Common PPG measurement result handler
 *
 * Logs result and handles post-measurement tasks.
 * Used by both UI-triggered and timer-scheduled measurements.
 *
 * @param result Measurement result
 * @param source Source of measurement ("UI" or "Timer")
 */
void ppg_handle_result(const struct ppg_algo_result *result,
		       const char *source);

#ifdef __cplusplus
}
#endif
