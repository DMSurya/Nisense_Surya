/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * health_sched.h — Agentic health measurement scheduler.
 *
 * The "measurement agent": owns the unified, wear-gated, sequential measurement
 * cycle and the smart adaptive cadence. Replaces the standalone glucose timer
 * as the single owner of when automatic measurements happen.
 *
 * Per cycle (only when the watch is worn):
 *     temp(pre) -> PPG (rate × APP_HEALTH_PPG_SECONDS) -> glucose -> temp(post)
 *
 * Sequential by design: the glucose IR LED and the MAX86141 PPG photodiodes
 * share an optical path, and both drive high-current LEDs off the BBOUT 5 V
 * rail — overlapping them risks optical crosstalk and current spikes.
 *
 * After each cycle the agent reasons over the new glucose value + its rate of
 * change (plus SpO2/HR/skin-temp modifiers and a battery clamp) to pick the
 * next interval from a ladder. It tightens immediately on concerning data and
 * relaxes only after sustained stability (safety-asymmetric hysteresis).
 * The auto timer always starts from cycle *end* (manual or auto); starting a
 * cycle cancels any pending countdown so a manual measure resets the schedule.
 *
 * All measurement results continue to flow through the existing per-sensor
 * paths (CSV logging, BLE notify); this module only orchestrates timing and
 * emits UI events for the UI agent.
 *
 * @author Ponmadasamy Muthuraj <ponmadasamy@live.com>
 */

#ifndef HEALTH_SCHED_H_
#define HEALTH_SCHED_H_

#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Steps of the active measurement cycle (for progress reporting / UI). */
enum health_sched_step {
	HEALTH_STEP_IDLE = 0,
	HEALTH_STEP_TEMP_PRE,
	HEALTH_STEP_PPG,
	HEALTH_STEP_GLUCOSE,
	HEALTH_STEP_TEMP_POST,
	HEALTH_STEP_DONE,
};

/** What triggered the current/last cycle. */
enum health_sched_trigger {
	HEALTH_TRIGGER_AUTO = 0,   /* scheduler timer (worn) */
	HEALTH_TRIGGER_MANUAL,     /* user pressed Measure */
};

/**
 * @brief Initialise and arm the health scheduler.
 *
 * Subscribes to wear/battery events, registers measurement-complete hooks, and
 * starts the adaptive interval timer. Call once after the sensor modules
 * (glucose, PPG, temp) are initialised.
 *
 * @retval 0 on success, negative errno otherwise.
 */
int health_sched_init(void);

/**
 * @brief Start a measurement cycle now.
 *
 * Used by the auto timer and by the manual Measure screen. Ignored if a cycle
 * is already running (returns -EALREADY).
 *
 * On a successful start the pending auto timer is stopped. When the cycle
 * finishes, the scheduler re-arms the auto interval from that moment (same
 * for auto and manual triggers), so a manual measure resets the schedule.
 *
 * @param trigger Whether this is an automatic or manual cycle.
 * @param include_finger When true (manual), also takes a finger spot-check.
 * @param skip_proximity When true, PPG proximity guard is bypassed (bench/debug).
 * @retval 0 cycle started, -EALREADY if one is in progress, negative errno otherwise.
 */
int health_sched_start_cycle(enum health_sched_trigger trigger, bool include_finger,
			       bool skip_proximity);

/** @return true while a skip-proximity (debug) cycle is active. */
bool health_sched_skip_proximity_active(void);

/**
 * @brief Re-evaluate wear gating after PPG proximity-disable policy changes.
 *
 * When proximity is force-disabled for bench testing, treat the device as worn
 * so auto-sched can arm; when re-enabled, wait for a real wear event.
 */
void health_sched_notify_prox_policy_changed(void);

/**
 * @brief Cancel the active measurement cycle (manual or scheduled).
 *
 * Stops PPG/glucose if running, clears step guards, returns to idle, and
 * re-arms the auto interval from now (same as a normal cycle end). Safe to
 * call when idle (returns -EALREADY).
 *
 * @retval 0 cancelled, -EALREADY if idle, -ENODEV if not initialised.
 */
int health_sched_cancel_cycle(void);

/**
 * @brief Overall cycle progress 0–100 (includes sub-step sample progress).
 */
uint8_t health_sched_get_progress_pct(void);

/**
 * @brief True while a measurement cycle is running.
 */
bool health_sched_is_busy(void);

/**
 * @brief Re-arm the PPG step guard for post-sample finalize (CSV flush).
 *
 * Called from ppg_algo when sampling is complete but before the vitals CSV
 * row is written. Prevents the sampling guard from expiring during a long
 * FatFS append.
 */
void health_sched_ppg_post_sample(void);

/**
 * @brief Current cycle step (HEALTH_STEP_IDLE when not running).
 */
enum health_sched_step health_sched_get_step(void);

/**
 * @brief Seconds until the next automatic cycle (0 if disabled / busy / not worn).
 *
 * While a cycle runs this returns 0; after DONE the full interval is armed again.
 */
uint32_t health_sched_seconds_to_next(void);

/**
 * @brief Current interval used for the next auto arm (fixed or ladder).
 */
uint32_t health_sched_current_interval_sec(void);

/**
 * @brief Set a fixed auto-schedule interval in seconds.
 *
 * @param sec 0 restores adaptive ladder; otherwise clamps to 60–3600 and
 *            disables adaptive cadence until cleared. Re-arms the timer if idle.
 */
void health_sched_set_interval_sec(uint32_t sec);

/**
 * @brief Configured fixed interval (0 if adaptive ladder is active).
 */
uint32_t health_sched_get_configured_interval_sec(void);

/**
 * @brief Enable or disable automatic scheduling.
 *
 * Manual cycles still work when disabled. The agent auto-disables auto cycles
 * when the watch is not worn and re-enables when worn again.
 */
void health_sched_set_auto_enabled(bool enabled);

/** True when auto wear-gated measurement cycles are armed. */
bool health_sched_auto_enabled(void);

#ifdef __cplusplus
}
#endif

#endif /* HEALTH_SCHED_H_ */
