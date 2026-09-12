/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Full-screen hold countdown on lv_layer_top(). Built on demand and destroyed
 * on dismiss so only one transient overlay tree exists (same model as ui_agent).
 */

#include "ui_hold_overlay.h"

#include "ui_agent.h"
#include "ui_obj_track.h"
#include "ui_theme.h"
#include "resource_fonts.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ui_hold_overlay, CONFIG_UI_LOG_LEVEL);

#define OVERLAY_SLUG     "hold_overlay"
#define OVERLAY_TIMER_MS 200U

static lv_obj_t *s_panel;
static lv_obj_t *s_title;
static lv_obj_t *s_subtitle;
static lv_obj_t *s_count;
static lv_obj_t *s_arc;
static lv_timer_t *s_timer;
static uint32_t s_start_ms;
static uint32_t s_duration_ms;
static uint32_t s_last_remain_s;
static int32_t s_last_arc_pct;
static ui_hold_overlay_cb_t s_complete_cb;
static void *s_complete_ud;
static bool s_complete_fired;

static void destroy_overlay(void)
{
	if (s_timer != NULL) {
		lv_timer_delete(s_timer);
		s_timer = NULL;
	}

	if (s_panel != NULL && lv_obj_is_valid(s_panel)) {
		ui_obj_log_screen_delete_begin(OVERLAY_SLUG, lv_obj_get_child_count(s_panel));
		lv_obj_delete(s_panel);
		ui_obj_log_screen_delete_end(OVERLAY_SLUG);
	}

	s_panel = NULL;
	s_title = NULL;
	s_subtitle = NULL;
	s_count = NULL;
	s_arc = NULL;
	s_last_remain_s = UINT32_MAX;
	s_last_arc_pct = -1;
	s_complete_cb = NULL;
	s_complete_ud = NULL;
	s_complete_fired = false;
}

static void overlay_timer_cb(lv_timer_t *timer)
{
	ARG_UNUSED(timer);

	if (s_panel == NULL || !lv_obj_is_valid(s_panel)) {
		return;
	}

	uint32_t elapsed = k_uptime_get_32() - s_start_ms;

	if (elapsed >= s_duration_ms) {
		if (!s_complete_fired) {
			ui_hold_overlay_cb_t cb = s_complete_cb;
			void *ud = s_complete_ud;

			s_complete_fired = true;
			destroy_overlay();
			if (cb != NULL) {
				cb(ud);
			}
		}
		return;
	}

	uint32_t remain_ms = s_duration_ms - elapsed;
	uint32_t remain_s = (remain_ms + 999U) / 1000U;

	if (remain_s == 0U) {
		remain_s = 1U;
	}

	char buf[8];

	if (remain_s != s_last_remain_s) {
		snprintf(buf, sizeof(buf), "%u", remain_s);
		lv_label_set_text(s_count, buf);
		s_last_remain_s = remain_s;
	}

	if (s_arc != NULL && lv_obj_is_valid(s_arc)) {
		int32_t pct = (int32_t)((uint64_t)elapsed * 100ULL / s_duration_ms);

		if (pct != s_last_arc_pct) {
			lv_arc_set_value(s_arc, pct);
			s_last_arc_pct = pct;
		}
	}
}

static void build_overlay(const char *title, const char *subtitle)
{
	if (s_panel != NULL && lv_obj_is_valid(s_panel)) {
		destroy_overlay();
	}

	if (ui_agent_slideshow_active()) {
		ui_agent_dismiss_slideshow();
	}

	s_panel = lv_obj_create(lv_layer_top());
	if (s_panel == NULL) {
		LOG_WRN("Hold overlay alloc failed");
		return;
	}

	ui_obj_name_screen(s_panel, OVERLAY_SLUG);
	ui_obj_log_screen_create(OVERLAY_SLUG);

	lv_obj_remove_style_all(s_panel);
	lv_obj_set_size(s_panel, LV_PCT(100), LV_PCT(100));
	lv_obj_set_style_bg_color(s_panel, lv_color_black(), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(s_panel, LV_OPA_80, LV_PART_MAIN);
	lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);

	s_title = ui_label_create(s_panel, OVERLAY_SLUG, "title");
	lv_obj_set_style_text_font(s_title, UI_FONT_20, LV_PART_MAIN);
	lv_obj_set_style_text_color(s_title, NS_COLOR_TEXT, LV_PART_MAIN);
	lv_label_set_text(s_title, title != NULL ? title : "");
	lv_obj_align(s_title, LV_ALIGN_CENTER, 0, -72);

	s_count = ui_label_create(s_panel, OVERLAY_SLUG, "count");
	lv_obj_set_style_text_font(s_count, UI_FONT_28, LV_PART_MAIN);
	lv_obj_set_style_text_color(s_count, NS_COLOR_PRIMARY, LV_PART_MAIN);
	lv_label_set_text(s_count, "0");
	lv_obj_align(s_count, LV_ALIGN_CENTER, 0, -8);

	s_arc = lv_arc_create(s_panel);
	ui_obj_name(s_arc, OVERLAY_SLUG, "arc");
	lv_obj_set_size(s_arc, 180, 180);
	lv_arc_set_rotation(s_arc, 135);
	lv_arc_set_bg_angles(s_arc, 0, 270);
	lv_arc_set_range(s_arc, 0, 100);
	lv_arc_set_value(s_arc, 0);
	lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
	lv_obj_clear_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_set_style_arc_color(s_arc, NS_COLOR_PRIMARY, LV_PART_INDICATOR);
	lv_obj_set_style_arc_width(s_arc, 10, LV_PART_INDICATOR);
	lv_obj_set_style_arc_color(s_arc, NS_COLOR_CARD, LV_PART_MAIN);
	lv_obj_set_style_arc_width(s_arc, 10, LV_PART_MAIN);
	lv_obj_align(s_arc, LV_ALIGN_CENTER, 0, -8);

	s_subtitle = ui_label_create(s_panel, OVERLAY_SLUG, "subtitle");
	lv_obj_set_style_text_font(s_subtitle, UI_FONT_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(s_subtitle, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
	lv_label_set_text(s_subtitle, subtitle != NULL ? subtitle : "");
	lv_obj_align(s_subtitle, LV_ALIGN_CENTER, 0, 72);

	ui_obj_track_screen_tree(s_panel, OVERLAY_SLUG);

	s_timer = lv_timer_create(overlay_timer_cb, OVERLAY_TIMER_MS, NULL);
	if (s_timer == NULL) {
		destroy_overlay();
	}
}

static struct {
	char title[32];
	char subtitle[48];
	uint32_t duration_ms;
	ui_hold_overlay_cb_t cb;
	void *user_data;
} s_pending;

static void overlay_begin_lvgl(void *user_data)
{
	ARG_UNUSED(user_data);

	s_duration_ms = s_pending.duration_ms;
	s_complete_cb = s_pending.cb;
	s_complete_ud = s_pending.user_data;
	s_complete_fired = false;

	build_overlay(s_pending.title, s_pending.subtitle);
	if (s_panel == NULL) {
		return;
	}

	s_start_ms = k_uptime_get_32();
	s_last_remain_s = UINT32_MAX;
	s_last_arc_pct = -1;
	overlay_timer_cb(s_timer);
	lv_obj_move_foreground(s_panel);

	if (s_timer != NULL) {
		lv_timer_reset(s_timer);
		lv_timer_resume(s_timer);
	}
}

static void overlay_dismiss_lvgl(void *user_data)
{
	ARG_UNUSED(user_data);
	destroy_overlay();
}

void ui_hold_overlay_begin(const char *title, const char *subtitle,
			   uint32_t duration_ms, ui_hold_overlay_cb_t on_complete,
			   void *user_data)
{
	if (duration_ms < 300U) {
		duration_ms = 300U;
	}

	strncpy(s_pending.title, title != NULL ? title : "", sizeof(s_pending.title) - 1);
	s_pending.title[sizeof(s_pending.title) - 1] = '\0';
	strncpy(s_pending.subtitle, subtitle != NULL ? subtitle : "",
		sizeof(s_pending.subtitle) - 1);
	s_pending.subtitle[sizeof(s_pending.subtitle) - 1] = '\0';
	s_pending.duration_ms = duration_ms;
	s_pending.cb = on_complete;
	s_pending.user_data = user_data;

	lv_async_call(overlay_begin_lvgl, NULL);
}

void ui_hold_overlay_cancel(void)
{
	lv_async_call(overlay_dismiss_lvgl, NULL);
}

void ui_hold_overlay_dismiss(void)
{
	destroy_overlay();
}

bool ui_hold_overlay_active(void)
{
	return s_panel != NULL && lv_obj_is_valid(s_panel);
}
