/* home_info_ui.c – Device identity screen (FW, ID, uptime, record store)

 * Swipe right from the home vitals cards to reach this screen. Keeps the
 * product home uncluttered while still exposing firmware identity on-device.
 */

#include "home_info_ui.h"
#include "ui_common.h"
#include "ui_shell.h"
#include "ui_theme.h"
#include "git_version.h"
#include "record_store.h"

#include <lvgl.h>
#include "resource_fonts.h"
#include <stdio.h>
#include <string.h>
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/version.h>

#if defined(CONFIG_PPG_ALGO)
#include <zephyr/subsys/ppg_algo/ppg_algo.h>
#endif

LOG_MODULE_REGISTER(home_info_ui, CONFIG_HOME_INFO_UI_LOG_LEVEL);

#define HOME_INFO_LINES 5

static lv_obj_t *s_info_labels[HOME_INFO_LINES];
static bool s_static_info_valid;
static uint32_t s_last_uptime_sec;

#if defined(CONFIG_LV_FONT_MONTSERRAT_14)
#define INFO_FONT_STATIC (&lv_font_montserrat_14)
#else
#define INFO_FONT_STATIC UI_FONT_14
#endif

static void set_label_if_changed(lv_obj_t *lbl, const char *text)
{
	const char *cur;

	if (lbl == NULL || text == NULL) {
		return;
	}
	cur = lv_label_get_text(lbl);
	if (cur != NULL && strcmp(cur, text) == 0) {
		return;
	}
	lv_label_set_text(lbl, text);
}

static void update_static_info(void)
{
	char buf[48];

	if (ui_get_screen(UI_SCREEN_HOME_INFO) == NULL) {
		return;
	}

	if (ui_is_obj_valid(s_info_labels[0], UI_SCREEN_HOME_INFO)) {
		snprintf(buf, sizeof(buf), "FW %s #%s", FW_VERSION_STRING, GIT_HASH);
		set_label_if_changed(s_info_labels[0], buf);
	}
	if (ui_is_obj_valid(s_info_labels[1], UI_SCREEN_HOME_INFO)) {
		uint8_t dev_id[16];
		char hex[17] = {0};
		int ret = hwinfo_get_device_id(dev_id, sizeof(dev_id));

		if (ret > 0) {
			int n = (ret < 8) ? ret : 8;

			for (int i = 0; i < n; i++) {
				snprintf(&hex[i * 2], sizeof(hex) - i * 2, "%02x", dev_id[i]);
			}
			snprintf(buf, sizeof(buf), "ID %s", hex);
		} else {
			snprintf(buf, sizeof(buf), "ID N/A");
		}
		set_label_if_changed(s_info_labels[1], buf);
	}
	if (ui_is_obj_valid(s_info_labels[3], UI_SCREEN_HOME_INFO)) {
#if defined(CONFIG_PPG_ALGO)
		snprintf(buf, sizeof(buf), "PPG %s", ppg_algo_version());
#else
		snprintf(buf, sizeof(buf), "Build %.11s", __DATE__);
#endif
		set_label_if_changed(s_info_labels[3], buf);
	}

	s_static_info_valid = true;
}

static void update_storage_summary_label(void)
{
	char buf[48];
	struct record_store_stats st;

	if (!ui_is_obj_valid(s_info_labels[4], UI_SCREEN_HOME_INFO)) {
		return;
	}

	record_store_get_stats(&st);
	snprintf(buf, sizeof(buf), "Rec %u pend", st.pending);
	set_label_if_changed(s_info_labels[4], buf);
}

static void update_uptime_label(void)
{
	char buf[24];
	uint32_t up_sec;

	if (!ui_is_obj_valid(s_info_labels[2], UI_SCREEN_HOME_INFO)) {
		return;
	}

	up_sec = (uint32_t)(k_uptime_get() / 1000);
	if (up_sec == s_last_uptime_sec) {
		return;
	}
	s_last_uptime_sec = up_sec;

	snprintf(buf, sizeof(buf), "Up %u:%02u:%02u",
		 up_sec / 3600U, (up_sec / 60U) % 60U, up_sec % 60U);
	set_label_if_changed(s_info_labels[2], buf);
}

static void update_home_info_screen(void)
{
	if (ui_get_screen(UI_SCREEN_HOME_INFO) == NULL) {
		return;
	}

	if (!s_static_info_valid) {
		update_static_info();
	}
	update_uptime_label();
	update_storage_summary_label();
}

void home_info_ui_create_screen(lv_obj_t *screen)
{
	if (screen == NULL) {
		LOG_ERR("Home info screen is NULL");
		return;
	}

	ns_apply_screen_bg(screen);

	lv_obj_t *title = ns_make_screen_title(screen, "Info");

	if (title == NULL) {
		ui_delete_screen(UI_SCREEN_HOME_INFO);
		return;
	}

	lv_obj_t *content = lv_obj_create(screen);

	if (content == NULL) {
		ui_delete_screen(UI_SCREEN_HOME_INFO);
		return;
	}
	lv_obj_remove_style_all(content);
	lv_obj_set_size(content, 240, 240 - (UI_SHELL_TOP_INSET + 34));
	lv_obj_align(content, LV_ALIGN_TOP_MID, 0, UI_SHELL_TOP_INSET + 34);
	lv_obj_set_style_pad_all(content, 10, LV_PART_MAIN);
	lv_obj_set_style_pad_row(content, 6, LV_PART_MAIN);
	lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
	lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
			      LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_bg_color(content, NS_COLOR_BG, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(content, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

	for (int i = 0; i < HOME_INFO_LINES; i++) {
		s_info_labels[i] = lv_label_create(content);
		if (s_info_labels[i] != NULL) {
			lv_label_set_text(s_info_labels[i], "");
			lv_obj_set_style_text_font(s_info_labels[i], INFO_FONT_STATIC,
						    LV_PART_MAIN);
			lv_obj_set_style_text_color(s_info_labels[i], NS_COLOR_TEXT_DIM,
						    LV_PART_MAIN);
		}
	}

	s_static_info_valid = false;
	update_home_info_screen();
	LOG_INF("Home info screen ready");
}

void home_info_ui_update_screen(void)
{
	update_home_info_screen();
}

void home_info_ui_on_fonts_ready(void)
{
	if (ui_get_screen(UI_SCREEN_HOME_INFO) == NULL) {
		return;
	}

	s_static_info_valid = false;
	s_last_uptime_sec = UINT32_MAX;
	update_home_info_screen();
}

void home_info_ui_cleanup_screen(void)
{
	s_static_info_valid = false;
	s_last_uptime_sec = 0;
	for (int i = 0; i < HOME_INFO_LINES; i++) {
		s_info_labels[i] = NULL;
	}
}
