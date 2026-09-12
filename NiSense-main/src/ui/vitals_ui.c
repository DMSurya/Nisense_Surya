/* vitals_ui.c – Vital UI screen
 *
 * PPG measurement: HR, SpO2, Resp, Body Temp (°F from wrist MAX30208).
 * Long-press HR or SpO2 to run the PPG window.
 */

#include "vitals_ui.h"
#include "ui_common.h"
#include "ui_theme.h"
#include "ui_shell.h"
#include "resource.h"
#include "resource_icons.h"
#include "buzzer.h"
#include "ppg.h"
#if defined(CONFIG_APP_TEMP_MONITOR)
#include "temp.h"
#endif
#include "home_last_results.h"
#if defined(CONFIG_APP_HEALTH_SCHED)
#include "health_sched.h"
#endif
#include "ui_events.h"
#include <zephyr/subsys/ppg_algo/ppg_algo.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(vitals_ui, CONFIG_VITALS_UI_LOG_LEVEL);

#define PPG_SAMPLE_COUNT_DEFAULT \
	(CONFIG_PPG_ALGO_SAMPLE_RATE * CONFIG_PPG_ALGO_BUFFER_SECONDS)

/* UI widgets */
static lv_obj_t *v_status_label;
static lv_obj_t *v_cards_cont;
static struct ns_card v_hr_card;
static struct ns_card v_spo2_card;
static struct ns_card v_resp_card;
static struct ns_card v_temp_card;
static lv_obj_t *v_sample_label;

/* State */
static bool v_sampling;
static uint16_t v_current_sample;
static uint16_t v_sample_target;
static bool v_result_pending;
static volatile bool v_sample_update_pending;
static struct ppg_sample_data v_pending_sample;

static void vitals_ui_render_temp_card(void)
{
	char buf[48];
	float wrist_c = 0.0f;
	bool ok = false;

#if defined(CONFIG_APP_TEMP_MONITOR)
	struct temp_snapshot ts;

	temp_get_last(&ts);
	if (ts.wrist_valid) {
		wrist_c = ts.wrist_c;
		ok = true;
	}
#endif
	if (!ok) {
		const struct home_last_results *lr = home_last_results_get();

		if (lr->loaded && lr->wrist_valid) {
			wrist_c = lr->wrist_c;
			ok = true;
		}
	}

	if (!ui_is_obj_valid(v_temp_card.value_label, UI_SCREEN_VITALS)) {
		return;
	}
	if (ok) {
		float f = wrist_c * 9.0f / 5.0f + 32.0f;

		snprintf(buf, sizeof(buf), "%.1f", (double)f);
		lv_label_set_text(v_temp_card.value_label, buf);
		ns_card_set_status(&v_temp_card, NS_STATUS_NORMAL);
	} else {
		lv_label_set_text(v_temp_card.value_label, "--");
		ns_card_set_status(&v_temp_card, NS_STATUS_NEUTRAL);
	}
}

static void vitals_ui_render_cards(const struct ppg_algo_result *result)
{
	char buf[48];

	if (!result) {
		return;
	}

	if (ui_is_obj_valid(v_hr_card.value_label, UI_SCREEN_VITALS)) {
		if (result->hr_bpm > 0U) {
			snprintf(buf, sizeof(buf), "%u", result->hr_bpm);
		} else {
			snprintf(buf, sizeof(buf), "--");
		}
		lv_label_set_text(v_hr_card.value_label, buf);
		ns_card_set_status(&v_hr_card,
			(result->hr_bpm > 0U && result->hr_valid) ?
				NS_STATUS_NORMAL : NS_STATUS_ATTENTION);
	}

	if (ui_is_obj_valid(v_spo2_card.value_label, UI_SCREEN_VITALS)) {
		if (result->spo2_percent > 0U) {
			snprintf(buf, sizeof(buf), "%u%%", result->spo2_percent);
		} else {
			snprintf(buf, sizeof(buf), "--");
		}
		lv_label_set_text(v_spo2_card.value_label, buf);
		ns_card_set_status(&v_spo2_card,
			(result->spo2_percent > 0U && result->spo2_valid) ?
				NS_STATUS_NORMAL : NS_STATUS_ATTENTION);
	}

	if (ui_is_obj_valid(v_resp_card.value_label, UI_SCREEN_VITALS)) {
		if (result->resp_rate_bpm > 0U) {
			snprintf(buf, sizeof(buf), "%u", result->resp_rate_bpm);
		} else {
			snprintf(buf, sizeof(buf), "--");
		}
		lv_label_set_text(v_resp_card.value_label, buf);
		ns_card_set_status(&v_resp_card,
			(result->resp_rate_bpm > 0U && result->resp_valid) ?
				NS_STATUS_NORMAL : NS_STATUS_ATTENTION);
	}

	vitals_ui_render_temp_card();
}

static bool vitals_ui_merge_last_results(struct ppg_algo_result *result)
{
	bool have = false;

	if (ppg_get_result(result) == 0 &&
	    (result->hr_bpm > 0U || result->spo2_percent > 0U ||
	     result->resp_rate_bpm > 0U)) {
		have = true;
	}

	const struct home_last_results *lr = home_last_results_get();

	if (lr->loaded) {
		if (result->hr_bpm == 0U && lr->hr_bpm > 0U) {
			result->hr_bpm = lr->hr_bpm;
			result->hr_valid = lr->hr_valid;
			have = true;
		}
		if (result->spo2_percent == 0U && lr->spo2_percent > 0U) {
			result->spo2_percent = lr->spo2_percent;
			result->spo2_valid = lr->spo2_valid;
			have = true;
		}
		if (result->resp_rate_bpm == 0U && lr->resp_rate_bpm > 0U) {
			result->resp_rate_bpm = lr->resp_rate_bpm;
			result->resp_valid = lr->resp_valid;
			have = true;
		}
		if (lr->wrist_valid) {
			have = true;
		}
	}

#if defined(CONFIG_APP_TEMP_MONITOR)
	{
		struct temp_snapshot ts;

		temp_get_last(&ts);
		if (ts.wrist_valid) {
			have = true;
		}
	}
#endif

	return have;
}

static void vitals_ui_apply_last_results(void);
static void vitals_ui_on_sched_event(const struct ui_event *ev, void *user)
{
	ARG_UNUSED(user);

	if (ev == NULL || ev->type != UI_EVENT_MEAS_RESULT_READY) {
		return;
	}
	if (ev->data.meas_type == UI_MEAS_VITALS) {
		vitals_ui_apply_last_results();
	}
}

static void vitals_ui_ensure_events(void)
{
	static bool subscribed;

	if (!subscribed && ui_events_subscribe(vitals_ui_on_sched_event, NULL) == 0) {
		subscribed = true;
	}
}

static void vitals_ui_apply_last_results(void)
{
	struct ppg_algo_result result = {0};

	if (v_sampling || v_result_pending) {
		return;
	}
	if (ui_get_screen(UI_SCREEN_VITALS) == NULL) {
		return;
	}
	if (!vitals_ui_merge_last_results(&result)) {
		return;
	}

	if (ui_is_obj_valid(v_cards_cont, UI_SCREEN_VITALS)) {
		lv_obj_clear_flag(v_cards_cont, LV_OBJ_FLAG_HIDDEN);
	}
	vitals_ui_render_cards(&result);
}

/* Forward declarations */
static void v_sample_handler(enum ppg_trigger_type type, void *user_data);
static void v_complete_handler(enum ppg_trigger_type type, void *user_data);

/* -------------------------------------------------------------------------
 * PPG trigger handlers (called from work queue – NO direct LVGL access)
 * ---------------------------------------------------------------------- */
static void v_sample_handler(enum ppg_trigger_type type, void *user_data)
{
	ARG_UNUSED(type);
	ARG_UNUSED(user_data);

	if (!v_sampling && ppg_is_measurement_active()) {
		v_sampling = true;
		v_result_pending = false;
	}
	if (!v_sampling) {
		return;
	}
	if (ppg_algo_get_sample_data(&v_pending_sample) == 0) {
		v_current_sample = v_pending_sample.sample_number + 1;
		v_sample_update_pending = true;
	}
}

static void v_complete_handler(enum ppg_trigger_type type, void *user_data)
{
	ARG_UNUSED(type);
	ARG_UNUSED(user_data);

	if (!v_sampling && ppg_is_measurement_active()) {
		v_sampling = true;
	}
	if (!v_sampling) {
		return;
	}
	v_sampling = false;
	v_result_pending = true;
}

/* -------------------------------------------------------------------------
 * Screen update (called from UI thread)
 * ---------------------------------------------------------------------- */
void vitals_ui_update_screen(void)
{
	/* Process completed measurement */
	if (v_result_pending) {
		struct ppg_algo_result result = {0};
		if (ppg_get_result(&result) == 0) {
			/* Show result cards */
			if (ui_is_obj_valid(v_cards_cont, UI_SCREEN_VITALS)) {
				lv_obj_clear_flag(v_cards_cont, LV_OBJ_FLAG_HIDDEN);
			}

			vitals_ui_render_cards(&result);

			/* Status */
			if (ui_is_obj_valid(v_status_label, UI_SCREEN_VITALS)) {
				if (result.hr_valid || result.spo2_valid) {
					lv_label_set_text(v_status_label, "Done");
					lv_obj_set_style_text_color(v_status_label,
						NS_COLOR_NORMAL, LV_PART_MAIN);
				} else {
					lv_label_set_text(v_status_label, "Low Quality");
					lv_obj_set_style_text_color(v_status_label,
						NS_COLOR_RISK, LV_PART_MAIN);
				}
			}

			if (ui_is_obj_valid(v_sample_label, UI_SCREEN_VITALS)) {
				lv_obj_add_flag(v_sample_label, LV_OBJ_FLAG_HIDDEN);
			}

			/* Persist + BLE come from ppg_algo finalize / ppg_handle_result
			 * (avoid double-logging when this UI observes the same run). */
			ppg_handle_result(&result, "vitals_ui");

			LOG_INF("vitals_ui: HR=%u%s SpO2=%u%s Hb=%u.%u%s Resp=%u%s",
				result.hr_bpm, result.hr_valid ? "" : "(inv)",
				result.spo2_percent, result.spo2_valid ? "" : "(inv)",
				result.hb_g_dl_x10 / 10, result.hb_g_dl_x10 % 10,
				result.hb_valid ? "" : "(inv)",
				result.resp_rate_bpm, result.resp_valid ? "" : "(inv)");

			if (IS_ENABLED(CONFIG_BUZZER)) {
				buzzer_play_success();
			}
		}
		v_result_pending = false;
		return;
	}

	/* Update sampling progress + live cards while staged capture runs */
	if (v_sampling && v_sample_update_pending) {
		v_sample_update_pending = false;
		if (ui_is_obj_valid(v_sample_label, UI_SCREEN_VITALS)) {
			char buf[48];
			uint16_t target = ppg_get_measurement_target_samples();

			if (target == 0U) {
				target = v_pending_sample.total_samples;
			}
			snprintf(buf, sizeof(buf), "Sampling: %u/%u",
				 v_current_sample, target);
			lv_label_set_text(v_sample_label, buf);
			lv_obj_clear_flag(v_sample_label, LV_OBJ_FLAG_HIDDEN);
		}
#if defined(CONFIG_PPG_ALGO_STAGED_LIVE)
		{
			struct ppg_algo_result live = {0};

			if (ppg_get_result(&live) == 0 &&
			    (live.hr_bpm > 0U || live.spo2_percent > 0U)) {
				if (ui_is_obj_valid(v_cards_cont, UI_SCREEN_VITALS)) {
					lv_obj_clear_flag(v_cards_cont, LV_OBJ_FLAG_HIDDEN);
				}
				vitals_ui_render_cards(&live);
				if (ui_is_obj_valid(v_status_label, UI_SCREEN_VITALS)) {
					lv_label_set_text(v_status_label, "Live");
					lv_obj_set_style_text_color(v_status_label,
						NS_COLOR_INFO, LV_PART_MAIN);
				}
			}
		}
#endif
	}

	vitals_ui_apply_last_results();
}

/* -------------------------------------------------------------------------
 * Screen creation / cleanup
 * ---------------------------------------------------------------------- */
void vitals_ui_create_screen(lv_obj_t *screen)
{
	if (!screen) {
		LOG_ERR("vitals_ui_create_screen: NULL screen");
		return;
	}

	vitals_ui_ensure_events();

	ns_apply_screen_bg(screen);

	/* Register PPG triggers */
	int ret = ppg_register_sample_callback(v_sample_handler);
	if (ret) {
		LOG_WRN("vitals_ui: sample cb reg failed %d", ret);
	}
	ret = ppg_register_complete_callback(v_complete_handler);
	if (ret) {
		LOG_WRN("vitals_ui: complete cb reg failed %d", ret);
	}

	/* Title */
	ns_make_screen_title(screen, "Vital");

	v_status_label = lv_label_create(screen);
	if (v_status_label) {
		lv_label_set_text(v_status_label, "Swipe to Measure");
		lv_obj_set_style_text_color(v_status_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		lv_obj_set_style_text_font(v_status_label, UI_FONT_14, LV_PART_MAIN);
		lv_obj_align(v_status_label, LV_ALIGN_BOTTOM_MID, 0, -8);
	}

	v_cards_cont = ns_make_result_grid(screen);
	if (v_cards_cont) {
		ns_make_result_tile(v_cards_cont, "HR", NS_COLOR_HR,
				    xip_icon(RESOURCE_ID_ICON_HEART), &v_hr_card);
		ns_make_result_tile(v_cards_cont, "SpO2", NS_COLOR_SPO2,
				    xip_icon(RESOURCE_ID_ICON_SPO2), &v_spo2_card);
		ns_make_result_tile(v_cards_cont, "Resp", NS_COLOR_RESP,
				    xip_icon(RESOURCE_ID_ICON_RESP), &v_resp_card);
		ns_make_result_tile(v_cards_cont, "Temp", NS_COLOR_TEMP,
				    xip_icon(RESOURCE_ID_ICON_TEMP), &v_temp_card);
		lv_label_set_text(v_hr_card.value_label, "--");
		lv_label_set_text(v_spo2_card.value_label, "--");
		lv_label_set_text(v_resp_card.value_label, "--");
		lv_label_set_text(v_temp_card.value_label, "--");
	}

	/* Sampling progress label (shown during measurement, hidden otherwise) */
	v_sample_label = lv_label_create(screen);
	if (v_sample_label) {
		lv_label_set_text(v_sample_label, "Sampling: 0/0");
		lv_obj_set_style_text_color(v_sample_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		lv_obj_set_style_text_font(v_sample_label, UI_FONT_20, LV_PART_MAIN);
		lv_obj_align(v_sample_label, LV_ALIGN_CENTER, 0, 0);
		lv_obj_add_flag(v_sample_label, LV_OBJ_FLAG_HIDDEN);
	}

	/* Reset state on screen creation */
	v_sampling = false;
	v_result_pending = false;
	v_sample_update_pending = false;
	v_current_sample = 0;
	v_sample_target = PPG_SAMPLE_COUNT_DEFAULT;

	vitals_ui_apply_last_results();
}

void vitals_ui_cleanup_screen(void)
{
	/* Detach widget pointers only — do not stop an in-flight PPG measurement. */
	(void)ppg_unregister_sample_callback(v_sample_handler);
	(void)ppg_unregister_complete_callback(v_complete_handler);
	v_result_pending = false;
	v_sample_update_pending = false;

	v_status_label = NULL;
	v_cards_cont   = NULL;
	v_hr_card   = (struct ns_card){0};
	v_spo2_card = (struct ns_card){0};
	v_resp_card = (struct ns_card){0};
	v_temp_card = (struct ns_card){0};
	v_sample_label = NULL;
}
