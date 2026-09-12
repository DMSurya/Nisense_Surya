/* vascular_ui.c – Vascular UI screen
 *
 * SDNN, RMSSD, BP Sys/Dia from the same PPG acquisition window as Vital.
 * Long-press any vascular card to start/stop measurement.
 */

#include "vascular_ui.h"
#include "ui_common.h"
#include "ui_theme.h"
#include "ui_shell.h"
#include "resource.h"
#include "resource_icons.h"
#include "buzzer.h"
#include "ppg.h"
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

LOG_MODULE_REGISTER(vascular_ui, CONFIG_VASCULAR_UI_LOG_LEVEL);

#define PPG_SAMPLE_COUNT_DEFAULT \
	(CONFIG_PPG_ALGO_SAMPLE_RATE * CONFIG_PPG_ALGO_BUFFER_SECONDS)

static lv_obj_t *va_status_label;
static lv_obj_t *va_cards_cont;
static struct ns_card va_sdnn_card;
static struct ns_card va_rmssd_card;
static struct ns_card va_sys_card;
static struct ns_card va_dia_card;
static lv_obj_t *va_sample_label;

static bool va_sampling;
static uint16_t va_current_sample;
static uint16_t va_sample_target;
static bool va_result_pending;
static volatile bool va_sample_update_pending;
static struct ppg_sample_data va_pending_sample;

static void vascular_ui_render_cards(const struct ppg_algo_result *result)
{
	char buf[48];

	if (!result) {
		return;
	}

	if (ui_is_obj_valid(va_sdnn_card.value_label, UI_SCREEN_VASCULAR)) {
		if (result->hrv_valid && result->sdnn_ms > 0U) {
			snprintf(buf, sizeof(buf), "%u", result->sdnn_ms);
		} else {
			snprintf(buf, sizeof(buf), "--");
		}
		lv_label_set_text(va_sdnn_card.value_label, buf);
		ns_card_set_status(&va_sdnn_card,
				   result->hrv_valid ? NS_STATUS_NORMAL : NS_STATUS_ATTENTION);
	}

	if (ui_is_obj_valid(va_rmssd_card.value_label, UI_SCREEN_VASCULAR)) {
		if (result->hrv_valid && result->rmssd_ms > 0U) {
			snprintf(buf, sizeof(buf), "%u", result->rmssd_ms);
		} else {
			snprintf(buf, sizeof(buf), "--");
		}
		lv_label_set_text(va_rmssd_card.value_label, buf);
		ns_card_set_status(&va_rmssd_card,
				   result->hrv_valid ? NS_STATUS_NORMAL : NS_STATUS_ATTENTION);
	}

	if (ui_is_obj_valid(va_sys_card.value_label, UI_SCREEN_VASCULAR)) {
		if (result->systolic_mmhg > 0U) {
			snprintf(buf, sizeof(buf), "%u", result->systolic_mmhg);
		} else {
			snprintf(buf, sizeof(buf), "--");
		}
		lv_label_set_text(va_sys_card.value_label, buf);
		ns_card_set_status(&va_sys_card,
				   (result->systolic_mmhg > 0U && result->bp_valid) ?
					   NS_STATUS_NORMAL : NS_STATUS_ATTENTION);
	}

	if (ui_is_obj_valid(va_dia_card.value_label, UI_SCREEN_VASCULAR)) {
		if (result->diastolic_mmhg > 0U) {
			snprintf(buf, sizeof(buf), "%u", result->diastolic_mmhg);
		} else {
			snprintf(buf, sizeof(buf), "--");
		}
		lv_label_set_text(va_dia_card.value_label, buf);
		ns_card_set_status(&va_dia_card,
				   (result->diastolic_mmhg > 0U && result->bp_valid) ?
					   NS_STATUS_NORMAL : NS_STATUS_ATTENTION);
	}
}

static bool vascular_ui_merge_last_results(struct ppg_algo_result *result)
{
	if (ppg_get_result(result) == 0 &&
	    (result->hrv_valid || result->bp_valid ||
	     result->sdnn_ms > 0U || result->systolic_mmhg > 0U)) {
		return true;
	}

	/* Fall back to NVS home summary after reboot. */
	const struct home_last_results *last = home_last_results_get();

	if (last == NULL || !(last->hrv_valid || last->bp_valid)) {
		return false;
	}
	memset(result, 0, sizeof(*result));
	result->sdnn_ms = last->sdnn_ms;
	result->rmssd_ms = last->rmssd_ms;
	result->hrv_valid = last->hrv_valid;
	result->systolic_mmhg = last->systolic_mmhg;
	result->diastolic_mmhg = last->diastolic_mmhg;
	result->bp_valid = last->bp_valid;
	return true;
}

static void vascular_ui_apply_last_results(void);
static void vascular_ui_on_sched_event(const struct ui_event *ev, void *user)
{
	ARG_UNUSED(user);

	if (ev == NULL || ev->type != UI_EVENT_MEAS_RESULT_READY) {
		return;
	}
	if (ev->data.meas_type == UI_MEAS_VITALS) {
		vascular_ui_apply_last_results();
	}
}

static void vascular_ui_ensure_events(void)
{
	static bool subscribed;

	if (!subscribed && ui_events_subscribe(vascular_ui_on_sched_event, NULL) == 0) {
		subscribed = true;
	}
}

static void vascular_ui_apply_last_results(void)
{
	struct ppg_algo_result result = {0};

	if (va_sampling || va_result_pending) {
		return;
	}
	if (ui_get_screen(UI_SCREEN_VASCULAR) == NULL) {
		return;
	}
	if (!vascular_ui_merge_last_results(&result)) {
		return;
	}

	if (ui_is_obj_valid(va_cards_cont, UI_SCREEN_VASCULAR)) {
		lv_obj_clear_flag(va_cards_cont, LV_OBJ_FLAG_HIDDEN);
	}
	vascular_ui_render_cards(&result);
}

static void va_sample_handler(enum ppg_trigger_type type, void *user_data);
static void va_complete_handler(enum ppg_trigger_type type, void *user_data);

static void va_sample_handler(enum ppg_trigger_type type, void *user_data)
{
	ARG_UNUSED(type);
	ARG_UNUSED(user_data);

	if (!va_sampling && ppg_is_measurement_active()) {
		va_sampling = true;
		va_result_pending = false;
	}
	if (!va_sampling) {
		return;
	}
	if (ppg_algo_get_sample_data(&va_pending_sample) == 0) {
		va_current_sample = va_pending_sample.sample_number + 1;
		va_sample_update_pending = true;
	}
}

static void va_complete_handler(enum ppg_trigger_type type, void *user_data)
{
	ARG_UNUSED(type);
	ARG_UNUSED(user_data);

	if (!va_sampling && ppg_is_measurement_active()) {
		va_sampling = true;
	}
	if (!va_sampling) {
		return;
	}
	va_sampling = false;
	va_result_pending = true;
}

void vascular_ui_update_screen(void)
{
	if (va_result_pending) {
		struct ppg_algo_result result = {0};

		if (ppg_get_result(&result) == 0) {
			if (ui_is_obj_valid(va_cards_cont, UI_SCREEN_VASCULAR)) {
				lv_obj_clear_flag(va_cards_cont, LV_OBJ_FLAG_HIDDEN);
			}
			vascular_ui_render_cards(&result);

			if (ui_is_obj_valid(va_status_label, UI_SCREEN_VASCULAR)) {
				if (result.hrv_valid || result.bp_valid) {
					lv_label_set_text(va_status_label, "Done");
					lv_obj_set_style_text_color(va_status_label,
						NS_COLOR_NORMAL, LV_PART_MAIN);
				} else {
					lv_label_set_text(va_status_label, "Need more pulses");
					lv_obj_set_style_text_color(va_status_label,
						NS_COLOR_RISK, LV_PART_MAIN);
				}
			}
			if (ui_is_obj_valid(va_sample_label, UI_SCREEN_VASCULAR)) {
				lv_obj_add_flag(va_sample_label, LV_OBJ_FLAG_HIDDEN);
			}

			ppg_handle_result(&result, "vascular_ui");
			LOG_INF("vascular_ui: SDNN=%u RMSSD=%u BP=%u/%u",
				result.sdnn_ms, result.rmssd_ms,
				result.systolic_mmhg, result.diastolic_mmhg);

			if (IS_ENABLED(CONFIG_BUZZER)) {
				buzzer_play_success();
			}
		}
		va_result_pending = false;
		return;
	}

	if (va_sampling && va_sample_update_pending) {
		va_sample_update_pending = false;
		if (ui_is_obj_valid(va_sample_label, UI_SCREEN_VASCULAR)) {
			char buf[48];
			uint16_t target = ppg_get_measurement_target_samples();

			if (target == 0U) {
				target = va_sample_target;
			}
			snprintf(buf, sizeof(buf), "Sampling: %u/%u",
				 va_current_sample, target);
			lv_label_set_text(va_sample_label, buf);
		}
#if defined(CONFIG_PPG_ALGO_STAGED_LIVE)
		{
			struct ppg_algo_result live = {0};

			if (ppg_get_result(&live) == 0) {
				if (ui_is_obj_valid(va_cards_cont, UI_SCREEN_VASCULAR)) {
					lv_obj_clear_flag(va_cards_cont, LV_OBJ_FLAG_HIDDEN);
				}
				vascular_ui_render_cards(&live);
				if (ui_is_obj_valid(va_status_label, UI_SCREEN_VASCULAR)) {
					lv_label_set_text(va_status_label, "Live");
					lv_obj_set_style_text_color(va_status_label,
						NS_COLOR_INFO, LV_PART_MAIN);
				}
			}
		}
#endif
	}

	vascular_ui_apply_last_results();
}

void vascular_ui_create_screen(lv_obj_t *screen)
{
	if (!screen) {
		LOG_ERR("vascular_ui_create_screen: NULL screen");
		return;
	}

	vascular_ui_ensure_events();
	ns_apply_screen_bg(screen);

	(void)ppg_register_sample_callback(va_sample_handler);
	(void)ppg_register_complete_callback(va_complete_handler);

	ns_make_screen_title(screen, "Vascular");

	va_status_label = lv_label_create(screen);
	if (va_status_label) {
		lv_label_set_text(va_status_label, "Swipe to Measure");
		lv_obj_set_style_text_color(va_status_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		lv_obj_set_style_text_font(va_status_label, UI_FONT_14, LV_PART_MAIN);
		lv_obj_align(va_status_label, LV_ALIGN_BOTTOM_MID, 0, -8);
	}

	va_cards_cont = ns_make_result_grid(screen);
	if (va_cards_cont) {
		ns_make_result_tile(va_cards_cont, "SDNN", NS_COLOR_HRV,
				    xip_icon(RESOURCE_ID_ICON_HEART), &va_sdnn_card);
		ns_make_result_tile(va_cards_cont, "RMSSD", NS_COLOR_HRV,
				    xip_icon(RESOURCE_ID_ICON_RESP), &va_rmssd_card);
		ns_make_result_tile(va_cards_cont, "BP Sys", NS_COLOR_BP_SYS,
				    xip_icon(RESOURCE_ID_ICON_HEART), &va_sys_card);
		ns_make_result_tile(va_cards_cont, "BP Dia", NS_COLOR_BP_DIA,
				    xip_icon(RESOURCE_ID_ICON_HEART), &va_dia_card);

		lv_label_set_text(va_sdnn_card.value_label, "--");
		lv_label_set_text(va_rmssd_card.value_label, "--");
		lv_label_set_text(va_sys_card.value_label, "--");
		lv_label_set_text(va_dia_card.value_label, "--");
	}

	va_sample_label = lv_label_create(screen);
	if (va_sample_label) {
		lv_label_set_text(va_sample_label, "Sampling: 0/0");
		lv_obj_set_style_text_color(va_sample_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		lv_obj_set_style_text_font(va_sample_label, UI_FONT_20, LV_PART_MAIN);
		lv_obj_align(va_sample_label, LV_ALIGN_CENTER, 0, 0);
		lv_obj_add_flag(va_sample_label, LV_OBJ_FLAG_HIDDEN);
	}

	va_sampling = false;
	va_result_pending = false;
	va_sample_update_pending = false;
	va_current_sample = 0;
	va_sample_target = PPG_SAMPLE_COUNT_DEFAULT;

	vascular_ui_apply_last_results();
}

void vascular_ui_cleanup_screen(void)
{

	(void)ppg_unregister_sample_callback(va_sample_handler);
	(void)ppg_unregister_complete_callback(va_complete_handler);
	va_result_pending = false;
	va_sample_update_pending = false;

	va_status_label = NULL;
	va_cards_cont = NULL;
	va_sdnn_card = (struct ns_card){0};
	va_rmssd_card = (struct ns_card){0};
	va_sys_card = (struct ns_card){0};
	va_dia_card = (struct ns_card){0};
	va_sample_label = NULL;
}
