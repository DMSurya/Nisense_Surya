/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Timed notice screen — same look as other carousel screens, not in swipe
 * rotation. Remembers the previous screen and restores it on timeout/dismiss.
 * Show/dismiss are deferred onto the LVGL context via lv_async_call.
 */

#include "ui_notice.h"
#include "ui_common.h"
#include "ui_theme.h"
#include "ui_obj_track.h"

#include <lvgl.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ui_notice, LOG_LEVEL_INF);

#define NOTICE_SLUG "notice"

static lv_obj_t *s_title_label;
static lv_obj_t *s_detail_label;
static lv_timer_t *s_timer;

static enum ui_screen s_return_screen = UI_SCREEN_DEVICE_INFO;
static bool s_active;
static char s_title[48];
static char s_detail[96];
static uint32_t s_timeout_ms = UI_NOTICE_DEFAULT_TIMEOUT_MS;

static void notice_timer_cb(lv_timer_t *timer);
static void notice_stop_timer(void);
static void ui_notice_apply_content(void);
static void ui_notice_show_lvgl(void);
static void ui_notice_dismiss_lvgl(void);

static void notice_timer_cb(lv_timer_t *timer)
{
	ARG_UNUSED(timer);
	ui_notice_dismiss_lvgl();
}

static void notice_stop_timer(void)
{
	if (s_timer != NULL) {
		lv_timer_del(s_timer);
		s_timer = NULL;
	}
}

static void ui_notice_apply_content(void)
{
	if (ui_is_obj_valid(s_title_label, UI_SCREEN_NOTICE)) {
		lv_label_set_text(s_title_label, s_title[0] != '\0' ? s_title : "Notice");
	}
	if (ui_is_obj_valid(s_detail_label, UI_SCREEN_NOTICE)) {
		lv_label_set_text(s_detail_label, s_detail[0] != '\0' ? s_detail : "");
	}
}

void ui_notice_create_screen(lv_obj_t *screen)
{
	if (screen == NULL) {
		return;
	}

	ns_apply_screen_bg(screen);
	ns_make_screen_title(screen, "Notice");

	s_title_label = ui_label_create(screen, NOTICE_SLUG, "title");
	if (s_title_label != NULL) {
		lv_obj_set_width(s_title_label, 220);
		lv_label_set_long_mode(s_title_label, LV_LABEL_LONG_WRAP);
		lv_obj_set_style_text_font(s_title_label, UI_FONT_20, LV_PART_MAIN);
		lv_obj_set_style_text_color(s_title_label, NS_COLOR_WARNING, LV_PART_MAIN);
		lv_obj_set_style_text_align(s_title_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		lv_obj_align(s_title_label, LV_ALIGN_CENTER, 0, -24);
	}

	s_detail_label = ui_label_create(screen, NOTICE_SLUG, "detail");
	if (s_detail_label != NULL) {
		lv_obj_set_width(s_detail_label, 220);
		lv_label_set_long_mode(s_detail_label, LV_LABEL_LONG_WRAP);
		lv_obj_set_style_text_font(s_detail_label, UI_FONT_14, LV_PART_MAIN);
		lv_obj_set_style_text_color(s_detail_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		lv_obj_set_style_text_align(s_detail_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
		lv_obj_align(s_detail_label, LV_ALIGN_CENTER, 0, 24);
	}

	ui_notice_apply_content();
}

void ui_notice_cleanup_screen(void)
{
	notice_stop_timer();
	s_title_label = NULL;
	s_detail_label = NULL;
	s_active = false;
}

static void ui_notice_dismiss_lvgl(void)
{
	enum ui_screen back;

	if (!s_active) {
		return;
	}

	notice_stop_timer();
	s_active = false;
	back = s_return_screen;
	if (back >= UI_SCREEN_COUNT || back == UI_SCREEN_NOTICE) {
		back = UI_SCREEN_DEVICE_INFO;
	}
	LOG_INF("Notice dismiss → screen %d", (int)back);
	ui_request_screen(back);
}

static void dismiss_async_cb(void *user_data)
{
	ARG_UNUSED(user_data);
	ui_notice_dismiss_lvgl();
}

void ui_notice_dismiss(void)
{
	lv_async_call(dismiss_async_cb, NULL);
}

static void ui_notice_show_lvgl(void)
{
	enum ui_screen cur = ui_get_current_screen();

	if (cur != UI_SCREEN_NOTICE) {
		s_return_screen = cur;
	}

	s_active = true;
	LOG_INF("Notice show: \"%s\" / \"%s\" (%u ms) return=%d",
		s_title, s_detail, s_timeout_ms, (int)s_return_screen);

	if (cur == UI_SCREEN_NOTICE && ui_get_screen(UI_SCREEN_NOTICE) != NULL) {
		ui_notice_apply_content();
	} else {
		ui_request_screen(UI_SCREEN_NOTICE);
	}

	notice_stop_timer();
	s_timer = lv_timer_create(notice_timer_cb, s_timeout_ms, NULL);
	if (s_timer != NULL) {
		lv_timer_set_repeat_count(s_timer, 1);
	}
}

static void show_async_cb(void *user_data)
{
	ARG_UNUSED(user_data);
	ui_notice_show_lvgl();
}

void ui_notice_show(const char *title, const char *detail, uint32_t timeout_ms)
{
	if (timeout_ms == 0U) {
		timeout_ms = UI_NOTICE_DEFAULT_TIMEOUT_MS;
	}
	s_timeout_ms = timeout_ms;

	if (title != NULL && title[0] != '\0') {
		strncpy(s_title, title, sizeof(s_title) - 1U);
		s_title[sizeof(s_title) - 1U] = '\0';
	} else {
		strncpy(s_title, "Notice", sizeof(s_title) - 1U);
		s_title[sizeof(s_title) - 1U] = '\0';
	}

	if (detail != NULL) {
		strncpy(s_detail, detail, sizeof(s_detail) - 1U);
		s_detail[sizeof(s_detail) - 1U] = '\0';
	} else {
		s_detail[0] = '\0';
	}

	lv_async_call(show_async_cb, NULL);
}
