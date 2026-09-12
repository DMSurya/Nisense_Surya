/* rtc_ui.c – RTC UI screen
 *
 * Handles LVGL UI screen creation and updates for RTC clock display.
 * Separated from ui.c for better modularity.
 */

#include "rtc.h"
#include "ui_common.h"
#include "ui_theme.h"
#include <lvgl.h>
#include "resource_fonts.h"
#include <stdio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(rtc_ui, CONFIG_RTC_UI_LOG_LEVEL);

/* UI Screen Variables */
static lv_obj_t *rtc_date_label;
static lv_obj_t *rtc_time_label;
static lv_obj_t *rtc_alarm_status_label;

static void update_rtc_screen(void)
{
	/* Check if screen exists */
	if (ui_get_screen(UI_SCREEN_RTC) == NULL) {
		return;
	}
	
	struct tm rtc_tm;
	char buf[64];
	
	if (rtc_read_time(&rtc_tm) == 0) {
		/* Format date */
		if (ui_is_obj_valid(rtc_date_label, UI_SCREEN_RTC)) {
			snprintf(buf, sizeof(buf), "Date: %04d-%02d-%02d",
				rtc_tm.tm_year + 1900, rtc_tm.tm_mon + 1, rtc_tm.tm_mday);
			lv_label_set_text(rtc_date_label, buf);
		}
		
		/* Format time */
		if (ui_is_obj_valid(rtc_time_label, UI_SCREEN_RTC)) {
			snprintf(buf, sizeof(buf), "Time: %02d:%02d:%02d",
				rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec);
			lv_label_set_text(rtc_time_label, buf);
		}
	} else {
		if (ui_is_obj_valid(rtc_date_label, UI_SCREEN_RTC)) {
			lv_label_set_text(rtc_date_label, "Date: N/A");
		}
		if (ui_is_obj_valid(rtc_time_label, UI_SCREEN_RTC)) {
			lv_label_set_text(rtc_time_label, "Time: N/A");
		}
	}
	
	/* Alarm status */
	if (ui_is_obj_valid(rtc_alarm_status_label, UI_SCREEN_RTC)) {
		if (rtc_is_alarm_active()) {
			lv_label_set_text(rtc_alarm_status_label, "Alarm: Armed");
			lv_obj_set_style_text_color(rtc_alarm_status_label, NS_COLOR_NORMAL, LV_PART_MAIN);
		} else {
			lv_label_set_text(rtc_alarm_status_label, "Alarm: Disarmed");
			lv_obj_set_style_text_color(rtc_alarm_status_label, NS_COLOR_CRITICAL, LV_PART_MAIN);
		}
	}
}

void rtc_ui_create_screen(lv_obj_t *screen)
{
	if (screen == NULL) {
		LOG_ERR("Failed to create RTC screen - screen is NULL");
		return;
	}
	
	ns_apply_screen_bg(screen);
	
	/* Title (placed below the global status strip by the helper) */
	lv_obj_t *title = ns_make_screen_title(screen, "RTC Clock");
	if (title == NULL) {
		LOG_ERR("Failed to create RTC title label - out of memory");
		ui_delete_screen(UI_SCREEN_RTC);
		return;
	}
	
	/* Date label */
	rtc_date_label = lv_label_create(screen);
	if (rtc_date_label == NULL) {
		LOG_ERR("Failed to create RTC date label - out of memory");
		ui_delete_screen(UI_SCREEN_RTC);
		return;
	}
	if (rtc_date_label != NULL) {
		lv_label_set_text(rtc_date_label, "Date: --");
		lv_obj_set_style_text_font(rtc_date_label, UI_FONT_20, LV_PART_MAIN);
		lv_obj_set_style_text_color(rtc_date_label, NS_COLOR_TEXT, LV_PART_MAIN);
		lv_obj_align(rtc_date_label, LV_ALIGN_CENTER, 0, -30);
	}
	
	/* Time label */
	rtc_time_label = lv_label_create(screen);
	if (rtc_time_label == NULL) {
		LOG_ERR("Failed to create RTC time label - out of memory");
		ui_delete_screen(UI_SCREEN_RTC);
		return;
	}
	if (rtc_time_label != NULL) {
		lv_label_set_text(rtc_time_label, "Time: --");
		lv_obj_set_style_text_font(rtc_time_label, UI_FONT_20, LV_PART_MAIN);
		lv_obj_set_style_text_color(rtc_time_label, NS_COLOR_TEXT, LV_PART_MAIN);
		lv_obj_align(rtc_time_label, LV_ALIGN_CENTER, 0, 0);
	}
	
	/* Alarm status label */
	rtc_alarm_status_label = lv_label_create(screen);
	if (rtc_alarm_status_label == NULL) {
		LOG_ERR("Failed to create RTC alarm status label - out of memory");
		ui_delete_screen(UI_SCREEN_RTC);
		return;
	}
	if (rtc_alarm_status_label != NULL) {
		lv_label_set_text(rtc_alarm_status_label, "Alarm: --");
		lv_obj_set_style_text_font(rtc_alarm_status_label, UI_FONT_14, LV_PART_MAIN);
		lv_obj_set_style_text_color(rtc_alarm_status_label, NS_COLOR_TEXT, LV_PART_MAIN);
		lv_obj_align(rtc_alarm_status_label, LV_ALIGN_CENTER, 0, 40);
	}
	
	update_rtc_screen();
}

void rtc_ui_update_screen(void)
{
	update_rtc_screen();
}

void rtc_ui_cleanup_screen(void)
{
	rtc_date_label = NULL;
	rtc_time_label = NULL;
	rtc_alarm_status_label = NULL;
}

