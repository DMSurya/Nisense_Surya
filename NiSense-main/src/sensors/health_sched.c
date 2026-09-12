/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * health_sched.c — Agentic health measurement scheduler.
 *
 * State machine driven entirely from the system work queue. Each cycle step
 * kicks off an asynchronous measurement (PPG / glucose) and waits for that
 * sensor's existing completion callback to advance the machine; temperature
 * steps are synchronous (fast I2C). A guard timeout prevents a stuck sensor
 * from wedging the cycle.
 *
 * See health_sched.h for the design rationale.
 *
 * @author Ponmadasamy Muthuraj <ponmadasamy@live.com>
 */

#include "health_sched.h"

#ifdef CONFIG_APP_HEALTH_SCHED

#include "ui_events.h"
#include "ui_shell.h"
#include "temp.h"
#include "rtc.h"
#include "record_store.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>

#if defined(CONFIG_GLUCOSE_SENSOR)
#include "glucose.h"
#include <zephyr/drivers/sensor/glucose_sensor.h>
#endif
#if defined(CONFIG_APP_FEATURE_PPG)
#include "ppg.h"
#endif
#include "power_batt.h"
#include "power_mgr.h"
#include "meas_precheck.h"
#if defined(CONFIG_UI_SCREEN_NOTICE)
#include "ui_notice.h"
#endif

LOG_MODULE_REGISTER(health_sched, LOG_LEVEL_INF);

/* -------------------------------------------------------------------------
 * Adaptive cadence ladder (seconds). Index 1 (10 min) is the default.
 * Tighten = move toward index 0; relax = move toward the end.
 * ---------------------------------------------------------------------- */
static const uint32_t s_ladder_sec[] = { 300, 600, 900, 1200, 1800 };
#define LADDER_LEN     ((int)ARRAY_SIZE(s_ladder_sec))
#define LADDER_DEFAULT 1
#define LADDER_FASTEST 0

/* Relax only after this many consecutive stable cycles (hysteresis). */
#define STABLE_CYCLES_TO_RELAX 2

/* Glucose thresholds (mg/dL) — Kconfig-tunable clinical bands. */
#define G_CRIT_LOW   CONFIG_APP_HEALTH_GLUCOSE_CRIT_LOW
#define G_CRIT_HIGH  CONFIG_APP_HEALTH_GLUCOSE_CRIT_HIGH
#define G_ABN_LOW    CONFIG_APP_HEALTH_GLUCOSE_ABN_LOW
#define G_ABN_HIGH   CONFIG_APP_HEALTH_GLUCOSE_ABN_HIGH

/* Rate-of-change thresholds (mg/dL per minute). */
#define ROC_MOVING   1.0f
#define ROC_FAST     2.0f
#define ROC_CRITICAL 3.0f

/* PPG sample target for health cycles — rate × APP_HEALTH_PPG_SECONDS
 * (20 s / 500 with RESP; 12 s / 300 without). */
#if defined(CONFIG_APP_FEATURE_PPG)
#define PPG_CYCLE_SAMPLES \
	(CONFIG_PPG_ALGO_SAMPLE_RATE * CONFIG_APP_HEALTH_PPG_SECONDS)
#endif

/* Per-step guard while sampling; post-sample finalize uses a shorter re-arm. */
#define STEP_GUARD_MS (CONFIG_APP_HEALTH_STEP_GUARD_SEC * 1000)
#define PPG_POST_GUARD_MS (CONFIG_APP_HEALTH_PPG_POST_GUARD_SEC * 1000)

/* Battery clamp thresholds (% SoC). */
#define BATT_CLAMP_LOW   20   /* below: interval floor 15 min */
#define BATT_CLAMP_CRIT  10   /* below: idle-temp only, 30 min */

/* -------------------------------------------------------------------------
 * State
 * ---------------------------------------------------------------------- */
static bool s_inited;
static bool s_auto_enabled = true;
static bool s_worn;                 /* updated from WEAR_CHANGED events */
static int  s_ladder_idx = LADDER_DEFAULT;
static uint32_t s_fixed_interval_sec; /* 0 = adaptive ladder */
static uint8_t s_stable_run;        /* consecutive stable cycles */

static enum health_sched_step    s_step = HEALTH_STEP_IDLE;
static enum health_sched_trigger s_trigger = HEALTH_TRIGGER_AUTO;
static bool s_include_finger;
static bool s_skip_proximity;

/* Glucose trend memory for rate-of-change. */
static float    s_prev_glucose = -1.0f;
static uint32_t s_prev_glucose_ts;

/* Latest PPG snapshot captured at completion (for cadence modifiers). */
static uint16_t s_last_hr;
static uint16_t s_last_spo2;
static bool     s_last_spo2_valid;
static bool     s_last_hr_valid;

static struct k_work        s_step_work;   /* advances the state machine */
static struct k_work_delayable s_guard_work;   /* per-step timeout */
static struct k_timer       s_cycle_timer;  /* fires the auto cycle */

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */
static int read_battery_soc(void)
{
	int soc = -1;

	if (power_mgr_get_soc(&soc) == 0) {
		return soc;
	}

	return -1;
}

/** True when USB can supply the rail — pack SoC is not a power budget constraint. */
static bool usb_powered(void)
{
	struct power_batt_snapshot snap;

	if (power_mgr_get_snapshot(&snap) != 0) {
		return false;
	}
	return snap.usb_online;
}

/**
 * Optical auto cycles need either usable pack SoC or USB power.
 * Absent/0% pack on USB must not block measurements (bench / no-cell).
 */
static bool optical_power_ok(void)
{
	int soc;

	if (usb_powered()) {
		return true;
	}
	if (!power_mgr_allow_heavy_work()) {
		return false;
	}
	soc = read_battery_soc();
	if (soc < 0) {
		return true; /* unknown — do not block */
	}
	return soc >= BATT_CLAMP_CRIT;
}

static void publish_event(enum ui_event_type type)
{
	struct ui_event ev = { .type = type };

	ui_events_publish(&ev);
}

static void publish_result(enum ui_meas_type meas)
{
	struct ui_event ev = {
		.type = UI_EVENT_MEAS_RESULT_READY,
		.data.meas_type = meas,
	};
	ui_events_publish(&ev);
}

static void arm_cycle_timer(void)
{
	uint32_t sec = (s_fixed_interval_sec != 0U) ? s_fixed_interval_sec
						    : s_ladder_sec[s_ladder_idx];

	/* One-shot from *now* — always call after a cycle ends (or wear-on). */
	k_timer_start(&s_cycle_timer, K_SECONDS(sec), K_NO_WAIT);
	LOG_INF("Next auto cycle in %u s (%s)", sec,
		s_fixed_interval_sec != 0U ? "fixed" : "ladder");
}

static void stop_cycle_timer(void)
{
	k_timer_stop(&s_cycle_timer);
}

/* -------------------------------------------------------------------------
 * Smart adaptive cadence
 *
 * Safety-asymmetric: tighten immediately on concerning data, relax only after
 * sustained stability. Battery state can clamp the floor.
 * ---------------------------------------------------------------------- */
static void cadence_tighten(int steps)
{
	int prev = s_ladder_idx;

	s_ladder_idx = MAX(LADDER_FASTEST, s_ladder_idx - steps);
	s_stable_run = 0;
	if (s_ladder_idx != prev) {
		LOG_INF("Cadence tighten %d -> %d", prev, s_ladder_idx);
	}
}

static void cadence_hold(void)
{
	s_stable_run = 0;
}

static void cadence_relax_if_stable(void)
{
	s_stable_run++;
	if (s_stable_run >= STABLE_CYCLES_TO_RELAX) {
		int prev = s_ladder_idx;

		s_ladder_idx = MIN(LADDER_LEN - 1, s_ladder_idx + 1);
		s_stable_run = 0;
		if (s_ladder_idx != prev) {
			LOG_INF("Cadence relax %d -> %d (stable)", prev, s_ladder_idx);
		}
	}
}

/* Apply battery clamp to the chosen ladder index (floor only — never speeds up). */
static void cadence_apply_battery_clamp(void)
{
	int soc;

	/* USB supplies the rail — pack SoC must not stretch the schedule. */
	if (usb_powered()) {
		return;
	}

	soc = read_battery_soc();
	if (soc < 0) {
		return;
	}

	/* Find the smallest ladder index whose interval still respects the floor. */
	if (soc < BATT_CLAMP_CRIT) {
		/* 30 min floor — last ladder entry. */
		s_ladder_idx = LADDER_LEN - 1;
		LOG_WRN("Battery %d%% critical: clamping cadence to %u s", soc,
			s_ladder_sec[s_ladder_idx]);
	} else if (soc < BATT_CLAMP_LOW) {
		/* 15 min floor — clamp idx so interval >= 900 s. */
		for (int i = 0; i < LADDER_LEN; i++) {
			if (s_ladder_sec[i] >= 900U) {
				s_ladder_idx = MAX(s_ladder_idx, i);
				break;
			}
		}
		LOG_INF("Battery %d%% low: cadence floor 15 min (idx %d)", soc, s_ladder_idx);
	}
}

/*
 * Reason over the freshly completed cycle and pick the next interval.
 * Called at HEALTH_STEP_DONE.
 */
static void cadence_evaluate(float glucose_mg_dl, bool glucose_valid)
{
	if (s_fixed_interval_sec != 0U) {
		/* BLE/test fixed interval — leave ladder alone. */
		return;
	}

	if (!glucose_valid) {
		/* No glucose this cycle — hold cadence, don't reset trend. */
		cadence_hold();
		cadence_apply_battery_clamp();
		return;
	}

	/* Rate of change vs previous valid sample. */
	float roc = 0.0f;
	uint32_t now = 0U;

	(void)rtc_get_unix_time(&now);
	if (s_prev_glucose >= 0.0f && now > s_prev_glucose_ts) {
		float dt_min = (float)(now - s_prev_glucose_ts) / 60.0f;

		if (dt_min > 0.1f) {
			roc = (glucose_mg_dl - s_prev_glucose) / dt_min;
		}
	}
	float aroc = fabsf(roc);

	s_prev_glucose = glucose_mg_dl;
	s_prev_glucose_ts = now;

	/* Primary glucose classification. */
	if (glucose_mg_dl < G_CRIT_LOW || glucose_mg_dl > G_CRIT_HIGH || aroc > ROC_CRITICAL) {
		s_ladder_idx = LADDER_FASTEST;
		s_stable_run = 0;
		LOG_WRN("Glucose CRITICAL (%.0f mg/dL, ROC %.1f): cadence -> fastest",
			(double)glucose_mg_dl, (double)roc);
		/* The UI agent classifies the value from the cycle-complete event and
		 * raises the on-screen/buzzer alert; cadence just goes to fastest. */
	} else if (glucose_mg_dl < G_ABN_LOW || glucose_mg_dl > G_ABN_HIGH || aroc > ROC_FAST) {
		cadence_tighten(1);
		LOG_INF("Glucose abnormal (%.0f mg/dL, ROC %.1f): tighten",
			(double)glucose_mg_dl, (double)roc);
	} else if (aroc > ROC_MOVING) {
		cadence_hold();
	} else {
		/* Stable range and slow ROC. Apply vitals modifiers before relaxing. */
		bool modifier = false;

		if (s_last_spo2_valid && s_last_spo2 > 0U && s_last_spo2 < 92U) {
			modifier = true;
		}
		if (s_last_hr_valid && s_last_hr > 0U &&
		    (s_last_hr > 100U || s_last_hr < 45U)) {
			modifier = true;
		}

		if (modifier) {
			cadence_tighten(1);
			LOG_INF("Vitals modifier (HR %u, SpO2 %u): tighten despite stable glucose",
				s_last_hr, s_last_spo2);
		} else {
			cadence_relax_if_stable();
		}
	}

	cadence_apply_battery_clamp();
}

/* -------------------------------------------------------------------------
 * Cycle state machine — runs on the system work queue.
 * ---------------------------------------------------------------------- */
static void clear_proximity_debug(void)
{
	s_skip_proximity = false;
#if defined(CONFIG_APP_FEATURE_PPG)
	ppg_set_proximity_bypass(false);
#endif
	ui_shell_set_proximity_debug_blink(false);
}

static void arm_proximity_debug(bool skip)
{
	s_skip_proximity = skip;
#if defined(CONFIG_APP_FEATURE_PPG)
	ppg_set_proximity_bypass(skip);
#endif
	ui_shell_set_proximity_debug_blink(skip);
}

static void advance_to(enum health_sched_step step)
{
	s_step = step;
	(void)k_work_submit(&s_step_work);
}

static void finish_step_guard(void)
{
	(void)k_work_cancel_delayable(&s_guard_work);
}

void health_sched_ppg_post_sample(void)
{
	if (s_step != HEALTH_STEP_PPG) {
		return;
	}

	LOG_DBG("PPG sampling done — post-sample guard %u s", CONFIG_APP_HEALTH_PPG_POST_GUARD_SEC);
	(void)k_work_reschedule(&s_guard_work, K_MSEC(PPG_POST_GUARD_MS));
}

/* Completion hooks fired by the sensor modules; bounce onto the work queue. */
#if defined(CONFIG_APP_FEATURE_PPG)
static void on_ppg_complete(enum ppg_trigger_type type, void *user_data)
{
	ARG_UNUSED(user_data);
	if (type != PPG_TRIG_MEASUREMENT_COMPLETE) {
		return;
	}
	if (s_step != HEALTH_STEP_PPG) {
		return;
	}

	struct ppg_algo_result r = {0};

	if (ppg_get_result(&r) == 0) {
		s_last_hr = r.hr_bpm;
		s_last_hr_valid = r.hr_valid;
		s_last_spo2 = r.spo2_percent;
		s_last_spo2_valid = r.spo2_valid;
	}

	finish_step_guard();
	publish_result(UI_MEAS_VITALS);
	advance_to(HEALTH_STEP_GLUCOSE);
}
#endif

#if defined(CONFIG_GLUCOSE_SENSOR)
static float s_cycle_glucose;
static bool  s_cycle_glucose_valid;

static void on_glucose_complete(const struct glucose_sensor_result *result, void *user_data)
{
	ARG_UNUSED(user_data);
	if (s_step != HEALTH_STEP_GLUCOSE) {
		return;
	}

	s_cycle_glucose_valid = (result != NULL) &&
				((result->flags & GLUCOSE_SENSOR_FLAG_VALID) != 0U);
	s_cycle_glucose = s_cycle_glucose_valid ? result->glucose_mg_dl : -1.0f;

	finish_step_guard();
	publish_result(UI_MEAS_GLUCOSE);
	advance_to(HEALTH_STEP_TEMP_POST);
}
#endif

static void guard_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	LOG_WRN("Cycle step %d timed out — aborting and advancing", s_step);

	struct ui_event timeout_ev = {
		.type = UI_EVENT_MEAS_STEP_TIMEOUT,
		.data.sched_step = (int)s_step,
	};
	ui_events_publish(&timeout_ev);

#if defined(CONFIG_APP_FEATURE_PPG)
	if (s_step == HEALTH_STEP_PPG) {
		/* Sampling finished but CSV flush still running — give it one more window. */
		if (ppg_algo_is_finalizing()) {
			LOG_WRN("PPG finalize still running — extending post-sample guard");
			(void)k_work_reschedule(&s_guard_work, K_MSEC(PPG_POST_GUARD_MS));
			return;
		}
		(void)ppg_stop_measurement();
		advance_to(HEALTH_STEP_GLUCOSE);
		return;
	}
#endif
#if defined(CONFIG_GLUCOSE_SENSOR)
	if (s_step == HEALTH_STEP_GLUCOSE) {
		(void)glucose_abort_measurement();
		s_cycle_glucose_valid = false;
		advance_to(HEALTH_STEP_TEMP_POST);
		return;
	}
#endif
	/* Temperature steps are synchronous; nothing to abort. */
	advance_to(HEALTH_STEP_DONE);
}

static void arm_guard(void)
{
	(void)k_work_reschedule(&s_guard_work, K_MSEC(STEP_GUARD_MS));
}

static void step_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	switch (s_step) {
	case HEALTH_STEP_TEMP_PRE:
		temp_set_idle_logging(false);  /* avoid double-sampling during cycle */
		{
			struct temp_snapshot last;
			uint32_t now = 0U;
			bool need_sample = true;

			temp_get_last(&last);
			(void)rtc_get_unix_time(&now);
			if (last.wrist_valid && now > last.timestamp &&
			    (now - last.timestamp) < 120U) {
				need_sample = false;
				LOG_DBG("TEMP_PRE: reuse wrist sample (%u s old)",
					now - last.timestamp);
			}
			if (need_sample) {
				(void)temp_snapshot_now();
			}
		}
		publish_result(UI_MEAS_TEMP);
		advance_to(HEALTH_STEP_PPG);
		break;

	case HEALTH_STEP_PPG:
#if defined(CONFIG_APP_FEATURE_PPG)
	{
		LOG_INF("PPG step: %u samples (%u s @ %u Hz)",
			PPG_CYCLE_SAMPLES, CONFIG_APP_HEALTH_PPG_SECONDS,
			CONFIG_PPG_ALGO_SAMPLE_RATE);
		int ret = ppg_start_measurement(PPG_CYCLE_SAMPLES);

		if (ret == 0) {
			arm_guard();   /* wait for on_ppg_complete */
		} else {
			LOG_WRN("PPG start failed (%d) — skipping to glucose", ret);
			advance_to(HEALTH_STEP_GLUCOSE);
		}
	}
#else
		advance_to(HEALTH_STEP_GLUCOSE);
#endif
		break;

	case HEALTH_STEP_GLUCOSE:
#if defined(CONFIG_GLUCOSE_SENSOR)
	{
		s_cycle_glucose_valid = false;
		{
			struct meas_precheck_result pre = {0};

			if (meas_precheck_for_step(HEALTH_STEP_GLUCOSE, &pre) != 0 || !pre.ok) {
				LOG_WRN("Glucose step skipped: %s",
					pre.reason != NULL ? pre.reason : "model/storage");
				advance_to(HEALTH_STEP_TEMP_POST);
				break;
			}
		}
		int ret = glucose_start_measurement_from_ui();

		if (ret == 0) {
			arm_guard();   /* wait for on_glucose_complete */
		} else {
			LOG_WRN("Glucose start failed (%d) — skipping", ret);
			advance_to(HEALTH_STEP_TEMP_POST);
		}
	}
#else
		advance_to(HEALTH_STEP_TEMP_POST);
#endif
		break;

	case HEALTH_STEP_TEMP_POST:
		(void)temp_snapshot_now();
#if defined(CONFIG_GLUCOSE_SENSOR)
		if (s_include_finger) {
			float fc;

			if (temp_read_finger(&fc) == 0) {
				LOG_INF("Finger body temp: %.2f C", (double)fc);
			}
		}
#endif
		advance_to(HEALTH_STEP_DONE);
		break;

	case HEALTH_STEP_DONE:
#if defined(CONFIG_GLUCOSE_SENSOR)
		cadence_evaluate(s_cycle_glucose, s_cycle_glucose_valid);
#else
		cadence_evaluate(0.0f, false);
#endif
		temp_set_idle_logging(true);
		clear_proximity_debug();
		record_store_end_measurement();
		s_step = HEALTH_STEP_IDLE;
		/* Wake panel if it slept during the long cycle so results show. */
		power_mgr_activity_notify();
		publish_event(UI_EVENT_MEAS_CYCLE_COMPLETE);
		LOG_INF("Measurement cycle complete");

		/* Interval always counts from end of measurement (auto or manual).
		 * start_cycle() stops any pending timer when a cycle begins. */
		if (s_auto_enabled && s_worn) {
			arm_cycle_timer();
		}
		break;

	case HEALTH_STEP_IDLE:
	default:
		break;
	}
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */
int health_sched_start_cycle(enum health_sched_trigger trigger, bool include_finger,
			       bool skip_proximity)
{
	if (!s_inited) {
		return -ENODEV;
	}
	if (s_step != HEALTH_STEP_IDLE) {
		return -EALREADY;
	}

	{
		struct meas_precheck_result pre = {0};
		int pre_ret = meas_precheck(meas_precheck_cycle_needs(), &pre);

		if (pre_ret != 0 || !pre.ok) {
			LOG_WRN("Measurement precheck failed: %s (%d)",
				pre.reason != NULL ? pre.reason : "storage", pre_ret);
#if defined(CONFIG_UI_SCREEN_NOTICE)
			if (trigger == HEALTH_TRIGGER_MANUAL) {
				ui_notice_show("Cannot measure",
					       pre.reason != NULL ? pre.reason : "Storage unavailable",
					       0);
			}
#endif
			if (trigger == HEALTH_TRIGGER_AUTO && s_auto_enabled && s_worn) {
				arm_cycle_timer();
			}
			return pre_ret != 0 ? pre_ret : -ENOENT;
		}
	}

	/* A standalone measurement may be running from an individual screen's
	 * Measure button. Never stomp it — that would cause the very optical
	 * crosstalk this sequential scheduler exists to avoid. Skip and re-arm. */
	bool sensor_busy = false;

#if defined(CONFIG_GLUCOSE_SENSOR)
	sensor_busy = sensor_busy || glucose_is_measuring();
#endif
#if defined(CONFIG_APP_FEATURE_PPG)
	sensor_busy = sensor_busy || ppg_is_measurement_active();
#endif
	if (sensor_busy) {
		LOG_INF("Sensor busy (manual measurement) — deferring %s cycle",
			trigger == HEALTH_TRIGGER_MANUAL ? "manual" : "auto");
		if (trigger == HEALTH_TRIGGER_AUTO && s_auto_enabled && s_worn) {
			arm_cycle_timer();
		}
		return -EBUSY;
	}

	if (trigger == HEALTH_TRIGGER_AUTO && !optical_power_ok()) {
		int soc = read_battery_soc();

		LOG_WRN("Battery %d%% / no USB: skipping auto cycle (idle temp only)", soc);
		cadence_apply_battery_clamp();
		arm_cycle_timer();
		return -EAGAIN;
	}

	/* Committed to run: cancel any pending auto countdown. The next auto
	 * slot is armed only when this cycle reaches HEALTH_STEP_DONE (or a
	 * failed-start path above re-arms). Manual measure therefore resets
	 * the schedule to "interval after this measurement ends". */
	stop_cycle_timer();

	s_trigger = trigger;
	s_include_finger = include_finger;

	/* Manual long-press or persistent bench disable may bypass proximity. */
	{
		bool force_skip = false;

#if defined(CONFIG_APP_FEATURE_PPG)
		force_skip = ppg_get_proximity_disabled();
#endif
		arm_proximity_debug((skip_proximity && trigger == HEALTH_TRIGGER_MANUAL) ||
				   force_skip);
	}

	LOG_INF("Starting %s measurement cycle%s%s",
		trigger == HEALTH_TRIGGER_MANUAL ? "manual" : "auto",
		include_finger ? " (with finger)" : "",
		s_skip_proximity ? " [prox bypass]" : "");

	{
		uint32_t mid = 0;

		if (record_store_begin_measurement(&mid) != 0) {
			LOG_WRN("measurement_id allocate failed — cycle records ungrouped");
		} else {
			LOG_INF("measurement_id=%u", mid);
		}
	}

	publish_event(UI_EVENT_MEAS_CYCLE_STARTED);
	advance_to(HEALTH_STEP_TEMP_PRE);
	return 0;
}

bool health_sched_is_busy(void)
{
	return s_step != HEALTH_STEP_IDLE;
}

int health_sched_cancel_cycle(void)
{
	enum health_sched_step was;

	if (!s_inited) {
		return -ENODEV;
	}

	was = s_step;
	if (was == HEALTH_STEP_IDLE || was == HEALTH_STEP_DONE) {
		return -EALREADY;
	}

	LOG_INF("Cancelling measurement cycle (step %d)", (int)was);

	finish_step_guard();
	(void)k_work_cancel(&s_step_work);

	/* Mark idle before stopping sensors so completion hooks ignore late events. */
	s_step = HEALTH_STEP_IDLE;

#if defined(CONFIG_APP_FEATURE_PPG)
	if (was == HEALTH_STEP_PPG || ppg_is_measurement_active()) {
		(void)ppg_stop_measurement();
	}
#endif
#if defined(CONFIG_GLUCOSE_SENSOR)
	if (was == HEALTH_STEP_GLUCOSE || glucose_is_measuring()) {
		(void)glucose_abort_measurement();
	}
#endif

	temp_set_idle_logging(true);
	clear_proximity_debug();
	record_store_end_measurement();
	publish_event(UI_EVENT_MEAS_CYCLE_CANCELLED);

	if (s_auto_enabled && s_worn) {
		arm_cycle_timer();
	}

	LOG_INF("Measurement cycle cancelled");
	return 0;
}

enum health_sched_step health_sched_get_step(void)
{
	return s_step;
}

uint32_t health_sched_seconds_to_next(void)
{
	if (!s_auto_enabled || !s_worn || s_step != HEALTH_STEP_IDLE) {
		return 0U;
	}
	return (uint32_t)(k_timer_remaining_ticks(&s_cycle_timer) / CONFIG_SYS_CLOCK_TICKS_PER_SEC);
}

uint32_t health_sched_current_interval_sec(void)
{
	if (s_fixed_interval_sec != 0U) {
		return s_fixed_interval_sec;
	}
	return s_ladder_sec[s_ladder_idx];
}

void health_sched_set_interval_sec(uint32_t sec)
{
	if (sec == 0U) {
		s_fixed_interval_sec = 0U;
		LOG_INF("Schedule interval: adaptive ladder (idx %d = %u s)",
			s_ladder_idx, s_ladder_sec[s_ladder_idx]);
	} else {
		if (sec < 60U) {
			sec = 60U;
		} else if (sec > 3600U) {
			sec = 3600U;
		}
		s_fixed_interval_sec = sec;
		LOG_INF("Schedule interval: fixed %u s", sec);
	}

	if (s_auto_enabled && s_worn && s_step == HEALTH_STEP_IDLE) {
		arm_cycle_timer();
	}
}

uint32_t health_sched_get_configured_interval_sec(void)
{
	return s_fixed_interval_sec;
}

void health_sched_set_auto_enabled(bool enabled)
{
	s_auto_enabled = enabled;
	if (enabled && s_worn && s_step == HEALTH_STEP_IDLE) {
		arm_cycle_timer();
	} else if (!enabled) {
		stop_cycle_timer();
	}
	LOG_INF("Auto scheduling %s", enabled ? "enabled" : "disabled");
}

bool health_sched_auto_enabled(void)
{
	return s_auto_enabled;
}

bool health_sched_skip_proximity_active(void)
{
	return s_skip_proximity;
}

void health_sched_notify_prox_policy_changed(void)
{
	if (!s_inited) {
		return;
	}

#if defined(CONFIG_APP_FEATURE_PPG)
	if (ppg_get_proximity_disabled()) {
		if (!s_worn) {
			s_worn = true;
			LOG_INF("Prox disabled: treating as worn for auto-sched");
		}
		if (s_auto_enabled && s_step == HEALTH_STEP_IDLE) {
			arm_cycle_timer();
		}
		return;
	}
#endif
	/* Re-enable: drop synthetic worn; next WEAR_CHANGED will re-arm. */
	s_worn = false;
	stop_cycle_timer();
	LOG_INF("Prox enabled: waiting for wear to arm auto-sched");
}

uint8_t health_sched_get_progress_pct(void)
{
	int base = 0;
	int span = 10;

	switch (s_step) {
	case HEALTH_STEP_TEMP_PRE:
		return 5;
	case HEALTH_STEP_PPG:
		base = 10;
		span = 30;
		break;
	case HEALTH_STEP_GLUCOSE:
		base = 40;
		span = 35;
		break;
	case HEALTH_STEP_TEMP_POST:
		return 95;
	case HEALTH_STEP_DONE:
		return 100;
	case HEALTH_STEP_IDLE:
	default:
		return 0;
	}

#if defined(CONFIG_APP_FEATURE_PPG)
	if (s_step == HEALTH_STEP_PPG) {
		uint16_t taken = 0U;
		uint16_t target = 0U;

		if (ppg_get_measurement_progress(&taken, &target) == 0 && target > 0U) {
			uint32_t sub = ((uint32_t)taken * (uint32_t)span) / (uint32_t)target;

			return (uint8_t)MIN(base + (int)sub, base + span);
		}
	}
#endif
#if defined(CONFIG_GLUCOSE_SENSOR)
	if (s_step == HEALTH_STEP_GLUCOSE) {
		uint16_t taken = 0U;
		uint16_t target = 0U;

		if (glucose_get_measurement_progress(&taken, &target) == 0 && target > 0U) {
			uint32_t sub = ((uint32_t)taken * (uint32_t)span) / (uint32_t)target;

			return (uint8_t)MIN(base + (int)sub, base + span);
		}
		int state = glucose_get_state();

		if (state >= 4) {
			return (uint8_t)(base + span - 5);
		}
		if (state >= 2) {
			return (uint8_t)(base + span / 2);
		}
	}
#endif
	return (uint8_t)base;
}

/* -------------------------------------------------------------------------
 * Event handling — wear gating + battery alerts.
 * ---------------------------------------------------------------------- */
static void on_ui_event(const struct ui_event *ev, void *user)
{
	ARG_UNUSED(user);

	switch (ev->type) {
	case UI_EVENT_WEAR_CHANGED: {
		bool now_worn = (ev->data.wear_state != 0);

#if defined(CONFIG_APP_FEATURE_PPG)
		if (ppg_get_proximity_disabled()) {
			now_worn = true;
		}
#endif
		if (now_worn == s_worn) {
			break;
		}
		s_worn = now_worn;
		LOG_INF("Wear state: %s", s_worn ? "worn" : "not worn");

		if (s_worn && s_auto_enabled && s_step == HEALTH_STEP_IDLE) {
			/* Fresh interval from wear-on (not mid-countdown). */
			arm_cycle_timer();
		} else if (!s_worn) {
			stop_cycle_timer();
		}
		break;
	}
	default:
		break;
	}
}

/* Timer ISR → work queue (start a cycle in thread context). */
static struct k_work s_cycle_trigger_work;

static void cycle_trigger_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	int ret = health_sched_start_cycle(HEALTH_TRIGGER_AUTO, false, false);

	/* Timer already expired. If a cycle was in progress (-EALREADY), DONE
	 * will re-arm. If start failed without re-arming, recover here. */
	if (ret == -EALREADY) {
		LOG_DBG("Auto timer fired during active cycle — will re-arm at DONE");
	}
}

static void cycle_timer_handler(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	(void)k_work_submit(&s_cycle_trigger_work);
}

int health_sched_init(void)
{
	if (s_inited) {
		return 0;
	}

	k_work_init(&s_step_work, step_work_handler);
	k_work_init(&s_cycle_trigger_work, cycle_trigger_work_handler);
	k_work_init_delayable(&s_guard_work, guard_work_handler);
	k_timer_init(&s_cycle_timer, cycle_timer_handler, NULL);

	int ret = ui_events_subscribe(on_ui_event, NULL);

	if (ret < 0) {
		LOG_ERR("Event subscribe failed: %d", ret);
		return ret;
	}

#if defined(CONFIG_APP_FEATURE_PPG)
	(void)ppg_register_complete_callback(on_ppg_complete);
#endif
#if defined(CONFIG_GLUCOSE_SENSOR)
	(void)glucose_register_result_callback(on_glucose_complete, NULL);
#endif

	s_inited = true;
	s_ladder_idx = LADDER_DEFAULT;

	LOG_INF("Health scheduler ready (default interval %u s, wear-gated)",
		s_ladder_sec[s_ladder_idx]);
#if defined(CONFIG_APP_FEATURE_PPG)
	if (ppg_get_proximity_disabled()) {
		s_worn = true;
		LOG_INF("Prox disabled at boot: treating as worn for auto-sched");
	} else {
		LOG_INF("Waiting for wear detection to arm auto cycle");
	}
#else
	LOG_INF("Waiting for wear detection to arm auto cycle");
#endif
	return 0;
}

#endif /* CONFIG_APP_HEALTH_SCHED */
