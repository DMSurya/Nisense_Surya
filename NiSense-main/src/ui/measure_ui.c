/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * measure_ui.c — Unified manual "Measure" screen.
 *
 * Progress area is dynamic:
 *   idle     → hint text (bar hidden): "Press & hold to skip proximity"
 *   holding  → bar starts full and counts down over CONFIG_UI_MEASURE_LONG_PRESS_MS
 *   running  → bar shows health-scheduler measurement progress
 *   cancel   → same hold while busy: "Hold to cancel"
 *
 * Short tap (when worn) still starts a proximity-gated cycle.
 * Long-press idle: countdown then start with proximity bypass.
 * Long-press busy (manual or auto cycle): cancel the measurement.
 */

#include "measure_ui.h"
#include "ui_obj_track.h"
#include "ui_common.h"
#include "ui_theme.h"
#include "health_sched.h"
#include "ui_events.h"
#include "temp.h"
#if defined(CONFIG_UI_SCREEN_NOTICE)
#include "ui_notice.h"
#endif
#if defined(CONFIG_APP_FEATURE_PPG)
#include "ppg.h"
#endif
#if defined(CONFIG_GLUCOSE_SENSOR)
#include "glucose.h"
#endif
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(measure_ui, CONFIG_MEASURE_UI_LOG_LEVEL);

static lv_obj_t *m_btn;
static lv_obj_t *m_btn_label;
static lv_obj_t *m_step_label;
static lv_obj_t *m_bar;
static lv_timer_t *m_hold_timer;

static bool m_was_busy;
static bool m_step_timed_out;
static bool m_long_hold_fired;
static bool m_holding;
static bool m_hold_is_cancel; /* true = hold-to-cancel while cycle busy */
static bool m_cancelled_msg;
static uint32_t m_hold_start_ms;

#define MEASURE_UI_SLUG "measure"
#define HOLD_TICK_MS    50U
#define IDLE_HINT       "Press & hold to skip proximity"
#define BUSY_HINT       "Hold button to cancel"

static void measure_hold_cancel(void);

/** Idle: Measure All. Busy: stay clickable for hold-to-cancel. */
static void measure_ui_set_btn_mode(bool busy)
{
	if (!ui_is_obj_valid(m_btn, UI_SCREEN_MEASURE)) {
		return;
	}

	lv_obj_clear_state(m_btn, LV_STATE_DISABLED);
	lv_obj_add_flag(m_btn, LV_OBJ_FLAG_CLICKABLE);

	if (busy) {
		lv_obj_set_style_bg_color(m_btn, NS_COLOR_CARD, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(m_btn, LV_OPA_COVER, LV_PART_MAIN);
		if (ui_is_obj_valid(m_btn_label, UI_SCREEN_MEASURE)) {
			lv_label_set_text(m_btn_label, "Hold to cancel");
			lv_obj_set_style_text_color(m_btn_label, NS_COLOR_WARNING, LV_PART_MAIN);
		}
	} else {
		lv_obj_set_style_bg_color(m_btn, NS_COLOR_PRIMARY, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(m_btn, LV_OPA_COVER, LV_PART_MAIN);
		if (ui_is_obj_valid(m_btn_label, UI_SCREEN_MEASURE)) {
			lv_label_set_text(m_btn_label, "Measure All");
			lv_obj_set_style_text_color(m_btn_label, NS_COLOR_TEXT, LV_PART_MAIN);
		}
	}
}

static void measure_ui_set_idle_hint(void)
{
	if (ui_is_obj_valid(m_step_label, UI_SCREEN_MEASURE)) {
		lv_label_set_text(m_step_label, IDLE_HINT);
		lv_obj_set_style_text_color(m_step_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
	}
	if (ui_is_obj_valid(m_bar, UI_SCREEN_MEASURE)) {
		lv_obj_add_flag(m_bar, LV_OBJ_FLAG_HIDDEN);
		lv_bar_set_value(m_bar, 0, LV_ANIM_OFF);
	}
}

static void measure_ui_show_bar(int pct, bool anim)
{
	if (!ui_is_obj_valid(m_bar, UI_SCREEN_MEASURE)) {
		return;
	}
	lv_obj_clear_flag(m_bar, LV_OBJ_FLAG_HIDDEN);
	if (pct < 0) {
		pct = 0;
	} else if (pct > 100) {
		pct = 100;
	}
	lv_bar_set_value(m_bar, pct, anim ? LV_ANIM_ON : LV_ANIM_OFF);
}

static void measure_ui_on_event(const struct ui_event *ev, void *user)
{
	ARG_UNUSED(user);

	if (ev == NULL) {
		return;
	}

	if (ev->type == UI_EVENT_MEAS_STEP_TIMEOUT) {
		m_step_timed_out = true;
		return;
	}

	if (ev->type == UI_EVENT_MEAS_CYCLE_CANCELLED) {
		m_cancelled_msg = true;
		m_step_timed_out = false;
		if (ui_get_screen(UI_SCREEN_MEASURE) != NULL &&
		    ui_is_obj_valid(m_step_label, UI_SCREEN_MEASURE)) {
			lv_label_set_text(m_step_label, "Cancelled");
			lv_obj_set_style_text_color(m_step_label, NS_COLOR_WARNING, LV_PART_MAIN);
		}
		return;
	}

	if (ev->type == UI_EVENT_MEAS_RESULT_READY &&
	    ui_get_screen(UI_SCREEN_MEASURE) != NULL &&
	    ui_is_obj_valid(m_step_label, UI_SCREEN_MEASURE)) {
		char buf[56];

		switch (ev->data.meas_type) {
#if defined(CONFIG_APP_FEATURE_PPG)
		case UI_MEAS_VITALS: {
			struct ppg_algo_result r = {0};

			if (ppg_get_result(&r) == 0 && r.hr_bpm > 0U) {
				snprintf(buf, sizeof(buf), "HR %u  SpO2 %u%%",
					 r.hr_bpm, r.spo2_percent);
				lv_label_set_text(m_step_label, buf);
			}
			break;
		}
#endif
#if defined(CONFIG_GLUCOSE_SENSOR)
		case UI_MEAS_GLUCOSE: {
			struct glucose_sensor_result gr = {0};

			if (glucose_get_result(&gr) == 0 && gr.glucose_mg_dl > 0.0f) {
				snprintf(buf, sizeof(buf), "Glucose %.0f mg/dL",
					 (double)gr.glucose_mg_dl);
				lv_label_set_text(m_step_label, buf);
			}
			break;
		}
#endif
		default:
			break;
		}
	}
}

static void measure_ui_ensure_events(void)
{
	static bool subscribed;

	if (subscribed) {
		return;
	}

	if (ui_events_subscribe(measure_ui_on_event, NULL) == 0) {
		subscribed = true;
	}
}

static void step_to_ui(enum health_sched_step step, const char **label)
{
	switch (step) {
	case HEALTH_STEP_TEMP_PRE:  *label = "Body temperature...";   break;
	case HEALTH_STEP_PPG:       *label = "Heart rate & SpO2...";  break;
	case HEALTH_STEP_GLUCOSE:   *label = "Glucose...";            break;
	case HEALTH_STEP_TEMP_POST: *label = "Finalizing...";         break;
	case HEALTH_STEP_DONE:      *label = "Complete";              break;
	case HEALTH_STEP_IDLE:
	default:                    *label = IDLE_HINT;               break;
	}
}

static int measure_ui_start_cycle(bool skip_proximity)
{
	if (health_sched_is_busy()) {
		return -EALREADY;
	}

	int ret = health_sched_start_cycle(HEALTH_TRIGGER_MANUAL, true, skip_proximity);

	if (ret == 0) {
		LOG_INF("Manual measurement cycle started%s",
			skip_proximity ? " (prox bypass)" : "");
		measure_hold_cancel();
		measure_ui_set_btn_mode(true);
		if (ui_is_obj_valid(m_step_label, UI_SCREEN_MEASURE)) {
			lv_label_set_text(m_step_label,
				skip_proximity ? "Starting (no prox)..." : "Starting...");
			lv_obj_set_style_text_color(m_step_label, NS_COLOR_INFO, LV_PART_MAIN);
		}
		measure_ui_show_bar(0, false);
	} else if (ret == -ENODATA || ret == -EAGAIN) {
		if (ui_is_obj_valid(m_step_label, UI_SCREEN_MEASURE)) {
			lv_label_set_text(m_step_label,
				ret == -EAGAIN ? "Battery too low" : "Wear device first");
			lv_obj_set_style_text_color(m_step_label, NS_COLOR_WARNING, LV_PART_MAIN);
		}
		measure_ui_set_idle_hint();
#if defined(CONFIG_UI_SCREEN_NOTICE)
		ui_notice_show("Cannot measure",
			       ret == -EAGAIN ? "Battery too low" : "Wear device first", 0);
#endif
	} else if (ret == -EBUSY) {
		if (ui_is_obj_valid(m_step_label, UI_SCREEN_MEASURE)) {
			lv_label_set_text(m_step_label, "Sensor busy");
			lv_obj_set_style_text_color(m_step_label, NS_COLOR_WARNING, LV_PART_MAIN);
		}
	} else if (ret == -ENOENT || ret == -ENODEV) {
		/* Precheck already showed notice from health_sched. */
		measure_ui_set_idle_hint();
	} else {
		LOG_WRN("Manual cycle start failed: %d", ret);
#if defined(CONFIG_UI_SCREEN_NOTICE)
		ui_notice_show("Cannot measure", "Measurement refused", 0);
#endif
	}
	return ret;
}

static void measure_hold_cancel(void)
{
	m_holding = false;
	m_hold_is_cancel = false;
	if (m_hold_timer != NULL) {
		lv_timer_pause(m_hold_timer);
	}
}

static void measure_hold_timer_cb(lv_timer_t *timer)
{
	ARG_UNUSED(timer);

	if (!m_holding) {
		return;
	}

	const bool cancel_hold = m_hold_is_cancel;
	const bool busy_now = health_sched_is_busy();

	/* Cancel hold only valid while cycle still running. */
	if (cancel_hold && !busy_now) {
		measure_hold_cancel();
		return;
	}
	/* Start hold aborted if a cycle began elsewhere. */
	if (!cancel_hold && busy_now) {
		measure_hold_cancel();
		return;
	}

	const uint32_t duration_ms = (uint32_t)CONFIG_UI_MEASURE_LONG_PRESS_MS;
	uint32_t elapsed = k_uptime_get_32() - m_hold_start_ms;
	int remaining_pct;

	if (elapsed >= duration_ms) {
		remaining_pct = 0;
	} else {
		remaining_pct = (int)(100U - (elapsed * 100U) / duration_ms);
	}

	measure_ui_show_bar(remaining_pct, false);

	if (ui_is_obj_valid(m_step_label, UI_SCREEN_MEASURE)) {
		char buf[40];
		uint32_t left_ms = (elapsed >= duration_ms) ? 0U : (duration_ms - elapsed);

		snprintf(buf, sizeof(buf), cancel_hold ? "Cancel... %u.%us" : "Hold... %u.%us",
			 left_ms / 1000U, (left_ms / 100U) % 10U);
		lv_label_set_text(m_step_label, buf);
		lv_obj_set_style_text_color(m_step_label,
			cancel_hold ? NS_COLOR_WARNING : NS_COLOR_INFO, LV_PART_MAIN);
	}

	if (elapsed >= duration_ms) {
		measure_hold_cancel();
		m_long_hold_fired = true;
		if (cancel_hold) {
			int ret = health_sched_cancel_cycle();

			if (ret == 0) {
				LOG_INF("Measurement cancelled from Measure UI hold");
			} else {
				LOG_WRN("Cancel failed: %d", ret);
			}
		} else {
			(void)measure_ui_start_cycle(true);
		}
	}
}

static void measure_btn_press_cb(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);

	if (ui_get_screen(UI_SCREEN_MEASURE) == NULL) {
		return;
	}
	if (!ui_is_obj_valid(m_btn, UI_SCREEN_MEASURE)) {
		return;
	}

	if (code == LV_EVENT_PRESSED) {
		const bool busy = health_sched_is_busy();

		m_holding = true;
		m_hold_is_cancel = busy;
		m_hold_start_ms = k_uptime_get_32();
		measure_ui_show_bar(100, false);
		if (ui_is_obj_valid(m_step_label, UI_SCREEN_MEASURE)) {
			lv_label_set_text(m_step_label,
				busy ? "Hold to cancel..." : "Hold to skip proximity...");
			lv_obj_set_style_text_color(m_step_label,
				busy ? NS_COLOR_WARNING : NS_COLOR_INFO, LV_PART_MAIN);
		}
		if (m_hold_timer != NULL) {
			lv_timer_reset(m_hold_timer);
			lv_timer_resume(m_hold_timer);
		}
		return;
	}

	if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
		if (m_holding) {
			const bool was_cancel = m_hold_is_cancel;

			measure_hold_cancel();
			if (!m_long_hold_fired) {
				if (was_cancel && health_sched_is_busy()) {
					/* Restore live progress after aborted cancel hold. */
					measure_ui_update_screen();
				} else if (!was_cancel && !health_sched_is_busy()) {
					measure_ui_set_idle_hint();
				}
			}
		}
	}
}

static void measure_btn_click_cb(lv_event_t *e)
{
	ARG_UNUSED(e);

	if (ui_get_screen(UI_SCREEN_MEASURE) == NULL) {
		return;
	}
	if (m_long_hold_fired) {
		m_long_hold_fired = false;
		return;
	}
	if (health_sched_is_busy()) {
		/* Short tap while busy: hint only (cancel requires hold). */
		if (ui_is_obj_valid(m_step_label, UI_SCREEN_MEASURE)) {
			lv_label_set_text(m_step_label, BUSY_HINT);
			lv_obj_set_style_text_color(m_step_label, NS_COLOR_WARNING, LV_PART_MAIN);
		}
		return;
	}

	if (temp_get_wear_state() < 2
#if defined(CONFIG_APP_FEATURE_PPG)
	    && !ppg_get_proximity_disabled()
#endif
	    ) {
		if (ui_is_obj_valid(m_step_label, UI_SCREEN_MEASURE)) {
			lv_label_set_text(m_step_label, "Wear device - or hold to skip");
			lv_obj_set_style_text_color(m_step_label, NS_COLOR_WARNING, LV_PART_MAIN);
		}
		return;
	}

	(void)measure_ui_start_cycle(false);
}

void measure_ui_create_screen(lv_obj_t *screen)
{
	if (screen == NULL) {
		LOG_ERR("measure_ui: NULL screen");
		return;
	}

	measure_ui_ensure_events();
	m_step_timed_out = false;
	m_long_hold_fired = false;
	m_holding = false;
	m_hold_is_cancel = false;
	m_cancelled_msg = false;

	ns_apply_screen_bg(screen);
	ns_make_screen_title(screen, "Measure");

	lv_obj_set_scroll_dir(screen, LV_DIR_VER);

	m_step_label = ui_label_create(screen, MEASURE_UI_SLUG, "step_label");
	if (m_step_label != NULL) {
		lv_label_set_text(m_step_label, IDLE_HINT);
		lv_obj_set_width(m_step_label, 220);
		lv_label_set_long_mode(m_step_label, LV_LABEL_LONG_WRAP);
		lv_obj_set_style_text_font(m_step_label, UI_FONT_14, LV_PART_MAIN);
		lv_obj_set_style_text_color(m_step_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		lv_obj_set_style_text_align(m_step_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		lv_obj_align(m_step_label, LV_ALIGN_TOP_MID, 0, 62);
	}

	m_bar = ui_bar_create(screen, MEASURE_UI_SLUG, "progress_bar");
	if (m_bar != NULL) {
		lv_obj_set_size(m_bar, 200, 12);
		lv_obj_align(m_bar, LV_ALIGN_CENTER, 0, -10);
		lv_bar_set_range(m_bar, 0, 100);
		lv_bar_set_value(m_bar, 0, LV_ANIM_OFF);
		lv_obj_set_style_bg_color(m_bar, NS_COLOR_CARD, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(m_bar, LV_OPA_COVER, LV_PART_MAIN);
		lv_obj_set_style_bg_color(m_bar, NS_COLOR_PRIMARY, LV_PART_INDICATOR);
		lv_obj_set_style_radius(m_bar, 6, LV_PART_MAIN);
		lv_obj_set_style_radius(m_bar, 6, LV_PART_INDICATOR);
		lv_obj_add_flag(m_bar, LV_OBJ_FLAG_HIDDEN);
	}

	m_btn = ui_btn_create(screen, MEASURE_UI_SLUG, "measure_btn");
	if (m_btn != NULL) {
		lv_obj_set_size(m_btn, 200, 58);
		lv_obj_align(m_btn, LV_ALIGN_BOTTOM_MID, 0, -18);
		lv_obj_set_style_bg_color(m_btn, NS_COLOR_PRIMARY, LV_PART_MAIN);
		lv_obj_add_event_cb(m_btn, measure_btn_click_cb, LV_EVENT_CLICKED, NULL);
		lv_obj_add_event_cb(m_btn, measure_btn_press_cb, LV_EVENT_PRESSED, NULL);
		lv_obj_add_event_cb(m_btn, measure_btn_press_cb, LV_EVENT_RELEASED, NULL);
		lv_obj_add_event_cb(m_btn, measure_btn_press_cb, LV_EVENT_PRESS_LOST, NULL);

		m_btn_label = ui_label_create(m_btn, MEASURE_UI_SLUG, "measure_btn_label");
		if (m_btn_label != NULL) {
			lv_label_set_text(m_btn_label, "Measure All");
			lv_obj_set_style_text_color(m_btn_label, NS_COLOR_TEXT, LV_PART_MAIN);
			lv_obj_center(m_btn_label);
		}
	}

	if (m_hold_timer == NULL) {
		m_hold_timer = lv_timer_create(measure_hold_timer_cb, HOLD_TICK_MS, NULL);
		if (m_hold_timer != NULL) {
			lv_timer_pause(m_hold_timer);
		}
	}

	m_was_busy = false;
	measure_ui_update_screen();
}

void measure_ui_update_screen(void)
{
	if (ui_get_screen(UI_SCREEN_MEASURE) == NULL) {
		return;
	}

	bool busy = health_sched_is_busy();

	if (m_holding) {
		/* Hold countdown (start or cancel) owns the progress area. */
		return;
	}

	if (busy) {
		const char *label = BUSY_HINT;
		int pct = (int)health_sched_get_progress_pct();

		step_to_ui(health_sched_get_step(), &label);
		if (m_step_timed_out) {
			label = "Step timed out - retrying...";
		}
		if (ui_is_obj_valid(m_step_label, UI_SCREEN_MEASURE)) {
			lv_label_set_text(m_step_label, label);
			lv_obj_set_style_text_color(m_step_label, NS_COLOR_INFO, LV_PART_MAIN);
		}
		measure_ui_show_bar(pct, true);
		measure_ui_set_btn_mode(true);
	} else {
		measure_ui_set_btn_mode(false);

		if (m_was_busy) {
			bool show_cancelled = m_cancelled_msg;

			/* Transition: cycle just finished or cancelled. */
			if (ui_is_obj_valid(m_step_label, UI_SCREEN_MEASURE)) {
				if (show_cancelled) {
					lv_label_set_text(m_step_label, "Cancelled");
					lv_obj_set_style_text_color(m_step_label,
						NS_COLOR_WARNING, LV_PART_MAIN);
					m_cancelled_msg = false;
				} else if (m_step_timed_out) {
					lv_label_set_text(m_step_label, "Some steps timed out");
					lv_obj_set_style_text_color(m_step_label,
						NS_COLOR_WARNING, LV_PART_MAIN);
					m_step_timed_out = false;
				} else {
					uint32_t next = health_sched_seconds_to_next();

					if (next > 0U) {
						char hb[48];

						snprintf(hb, sizeof(hb), "Done - next auto %u:%02u",
							 next / 60U, next % 60U);
						lv_label_set_text(m_step_label, hb);
					} else {
						lv_label_set_text(m_step_label, "Complete");
					}
					lv_obj_set_style_text_color(m_step_label,
						NS_COLOR_NORMAL, LV_PART_MAIN);
				}
			}
			measure_ui_show_bar(show_cancelled ? 0 : 100, false);
		}
		/* Idle hint is restored on create / cancelled hold / failed start only. */
	}

	m_was_busy = busy;
}

void measure_ui_cleanup_screen(void)
{
	measure_hold_cancel();
	if (m_hold_timer != NULL) {
		lv_timer_delete(m_hold_timer);
		m_hold_timer = NULL;
	}
	m_btn = NULL;
	m_btn_label = NULL;
	m_step_label = NULL;
	m_bar = NULL;
	m_was_busy = false;
	m_step_timed_out = false;
	m_long_hold_fired = false;
	m_hold_is_cancel = false;
	m_cancelled_msg = false;
}
