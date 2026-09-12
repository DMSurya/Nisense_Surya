/* ppg_sample_ui.c – PPG raw waveform display screen
 *
 * Streams live IR/Red/Green sample counts during a PPG measurement.
 * Does not calculate vitals itself; delegates to ppg_algo via vitals_ui.
 * Useful for bring-up and signal-quality checking.
 */

#include "ppg_sample_ui.h"
#include "ui_common.h"
#include "ui_theme.h"
#include "ppg.h"
#include <zephyr/subsys/ppg_algo/ppg_algo.h>
#include <lvgl.h>
#include <stdio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ppg_sample_ui, CONFIG_PPG_SAMPLE_UI_LOG_LEVEL);

#define PPG_SAMPLE_COUNT_DEFAULT \
	(CONFIG_PPG_ALGO_SAMPLE_RATE * CONFIG_PPG_ALGO_BUFFER_SECONDS)

/* Widgets */
static lv_obj_t *ps_status_label;
static lv_obj_t *ps_raw_label;
static lv_obj_t *ps_ctrl_btn;
static lv_obj_t *ps_ctrl_btn_label;

/* State */
static bool ps_sampling;
static volatile bool ps_update_pending;
static struct ppg_sample_data ps_pending;
static uint16_t ps_sample_count;
static uint16_t ps_sample_target;

static void ps_ctrl_btn_cb(lv_event_t *e);
static void ps_sample_handler(enum ppg_trigger_type type, void *user_data);
static void ps_complete_handler(enum ppg_trigger_type type, void *user_data);

static void ps_ctrl_btn_cb(lv_event_t *e)
{
	if (!e) {
		return;
	}
	if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
		return;
	}
	if (ui_get_screen(UI_SCREEN_PPG_SAMPLE) == NULL) {
		return;
	}

	if (!ps_sampling) {
		int ret = ppg_start_measurement(PPG_SAMPLE_COUNT_DEFAULT);
		if (ret) {
			LOG_ERR("ppg_sample_ui: start failed %d", ret);
			if (ui_is_obj_valid(ps_status_label, UI_SCREEN_PPG_SAMPLE)) {
				lv_label_set_text(ps_status_label,
					ret == -ENODATA ? "No Contact" : "Error");
				lv_obj_set_style_text_color(ps_status_label,
					NS_COLOR_CRITICAL, LV_PART_MAIN);
			}
			return;
		}
		ps_sample_target = ppg_get_measurement_target_samples();
		if (ps_sample_target == 0U) {
			ps_sample_target = PPG_SAMPLE_COUNT_DEFAULT;
		}
		ps_sampling = true;
		ps_sample_count = 0;
		ps_update_pending = false;
		if (ui_is_obj_valid(ps_status_label, UI_SCREEN_PPG_SAMPLE)) {
			lv_label_set_text(ps_status_label, "Sampling...");
			lv_obj_set_style_text_color(ps_status_label,
				NS_COLOR_INFO, LV_PART_MAIN);
		}
		if (ui_is_obj_valid(ps_ctrl_btn_label, UI_SCREEN_PPG_SAMPLE)) {
			lv_label_set_text(ps_ctrl_btn_label, "Stop");
		}
	} else {
		ppg_stop_measurement();
		ps_sampling = false;
		if (ui_is_obj_valid(ps_status_label, UI_SCREEN_PPG_SAMPLE)) {
			lv_label_set_text(ps_status_label, "Stopped");
			lv_obj_set_style_text_color(ps_status_label,
				NS_COLOR_RISK, LV_PART_MAIN);
		}
		if (ui_is_obj_valid(ps_ctrl_btn_label, UI_SCREEN_PPG_SAMPLE)) {
			lv_label_set_text(ps_ctrl_btn_label, "Start");
		}
	}
}

static void ps_sample_handler(enum ppg_trigger_type type, void *user_data)
{
	ARG_UNUSED(type);
	ARG_UNUSED(user_data);

	if (!ps_sampling && ppg_is_measurement_active()) {
		ps_sampling = true;
	}
	if (!ps_sampling) {
		return;
	}
	if (ppg_algo_get_sample_data(&ps_pending) == 0) {
		ps_sample_count = ps_pending.sample_number + 1;
		ps_update_pending = true;
	}
}

static void ps_complete_handler(enum ppg_trigger_type type, void *user_data)
{
	ARG_UNUSED(type);
	ARG_UNUSED(user_data);

	ps_sampling = false;
	if (ui_is_obj_valid(ps_status_label, UI_SCREEN_PPG_SAMPLE)) {
		/* Note: this runs in work-queue context — set a flag instead in
		 * production, but for this debug screen a direct write is acceptable
		 * since LVGL is thread-safe when accessed from same priority as UI. */
	}
}

void ppg_sample_ui_update_screen(void)
{
	if (!ps_sampling && !ps_update_pending) {
		return;
	}
	if (ps_update_pending) {
		ps_update_pending = false;
		if (ui_is_obj_valid(ps_raw_label, UI_SCREEN_PPG_SAMPLE)) {
			char buf[80];
			snprintf(buf, sizeof(buf),
				 "IR: %u\nRed: %u\nGrn: %u\nSample: %u/%u",
				 (unsigned)ps_pending.raw_ir,
				 (unsigned)ps_pending.raw_red,
				 (unsigned)ps_pending.raw_green,
				 ps_sample_count, ps_sample_target);
			lv_label_set_text(ps_raw_label, buf);
		}
	}
}

void ppg_sample_ui_create_screen(lv_obj_t *screen)
{
	if (!screen) {
		LOG_ERR("ppg_sample_ui_create_screen: NULL");
		return;
	}

	ns_apply_screen_bg(screen);

	int ret = ppg_register_sample_callback(ps_sample_handler);
	if (ret) {
		LOG_WRN("ppg_sample_ui: sample cb %d", ret);
	}
	ret = ppg_register_complete_callback(ps_complete_handler);
	if (ret) {
		LOG_WRN("ppg_sample_ui: complete cb %d", ret);
	}

	ns_make_screen_title(screen, "PPG Samples");

	ps_status_label = lv_label_create(screen);
	if (ps_status_label) {
		lv_label_set_text(ps_status_label, "Ready");
		lv_obj_set_style_text_color(ps_status_label, NS_COLOR_NORMAL, LV_PART_MAIN);
		lv_obj_set_style_text_font(ps_status_label, UI_FONT_14, LV_PART_MAIN);
		lv_obj_align(ps_status_label, LV_ALIGN_TOP_MID, 0, 58);
	}

	ps_raw_label = lv_label_create(screen);
	if (ps_raw_label) {
		lv_label_set_text(ps_raw_label, "IR: --\nRed: --\nGrn: --\nSample: 0/0");
		lv_obj_set_style_text_color(ps_raw_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		lv_obj_set_style_text_font(ps_raw_label, UI_FONT_14, LV_PART_MAIN);
		lv_obj_align(ps_raw_label, LV_ALIGN_CENTER, 0, -20);
	}

	ps_ctrl_btn = lv_btn_create(screen);
	if (ps_ctrl_btn) {
		lv_obj_align(ps_ctrl_btn, LV_ALIGN_BOTTOM_MID, 0, -10);
		lv_obj_set_size(ps_ctrl_btn, 100, 40);
		lv_obj_add_event_cb(ps_ctrl_btn, ps_ctrl_btn_cb, LV_EVENT_CLICKED, NULL);

		ps_ctrl_btn_label = lv_label_create(ps_ctrl_btn);
		if (ps_ctrl_btn_label) {
			lv_label_set_text(ps_ctrl_btn_label, "Start");
			lv_obj_center(ps_ctrl_btn_label);
		}
	}

	ps_sampling = false;
	ps_update_pending = false;
	ps_sample_count = 0;
	ps_sample_target = PPG_SAMPLE_COUNT_DEFAULT;
}

void ppg_sample_ui_cleanup_screen(void)
{
	(void)ppg_unregister_sample_callback(ps_sample_handler);
	(void)ppg_unregister_complete_callback(ps_complete_handler);
	ps_update_pending = false;
	ps_status_label    = NULL;
	ps_raw_label       = NULL;
	ps_ctrl_btn        = NULL;
	ps_ctrl_btn_label  = NULL;
}
