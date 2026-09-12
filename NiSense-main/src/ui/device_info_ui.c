/* device_info_ui.c – Home screen (digital clock)

 *
 * Carousel index 0. Shows a large digital clock, date, and a short status
 * line (next auto-scan / swipe hint). Wear state is reflected as a subtle
 * border glow. Health values live on Metabolic / Vital / Vascular screens.
 */

#include "ui_common.h"
#include "ui_theme.h"
#include "ui_shell.h"
#include "ui_events.h"
#include "rtc.h"

#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <zephyr/logging/log.h>

#if defined(CONFIG_APP_HEALTH_SCHED)
#include "health_sched.h"
#endif
#if defined(CONFIG_APP_TEMP_MONITOR)
#include "temp.h"
#endif

LOG_MODULE_REGISTER(device_info_ui, CONFIG_DEVICE_INFO_UI_LOG_LEVEL);

static lv_obj_t *s_wear_frame;
static lv_obj_t *s_time_label;
static lv_obj_t *s_seconds_label;
static lv_obj_t *s_date_label;
static lv_obj_t *s_weekday_label;
static lv_obj_t *s_status_label;
static int s_wear_state = -1;
static uint32_t s_last_shown_min = UINT32_MAX;

static const char *const s_weekdays[] = {
	"Sunday", "Monday", "Tuesday", "Wednesday",
	"Thursday", "Friday", "Saturday",
};

static const char *const s_months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

static void apply_home_wear_border(void)
{
	if (s_wear_frame == NULL ||
	    !ui_is_obj_valid(s_wear_frame, UI_SCREEN_DEVICE_INFO)) {
		return;
	}

	switch (s_wear_state) {
	case 2:
		lv_obj_set_style_border_width(s_wear_frame, 2, LV_PART_MAIN);
		lv_obj_set_style_border_color(s_wear_frame, NS_COLOR_NORMAL, LV_PART_MAIN);
		lv_obj_set_style_border_opa(s_wear_frame, LV_OPA_60, LV_PART_MAIN);
		break;
	case 1:
		lv_obj_set_style_border_width(s_wear_frame, 2, LV_PART_MAIN);
		lv_obj_set_style_border_color(s_wear_frame, NS_COLOR_WARNING, LV_PART_MAIN);
		lv_obj_set_style_border_opa(s_wear_frame, LV_OPA_70, LV_PART_MAIN);
		break;
	case 0:
		lv_obj_set_style_border_width(s_wear_frame, 2, LV_PART_MAIN);
		lv_obj_set_style_border_color(s_wear_frame, NS_COLOR_CRITICAL, LV_PART_MAIN);
		lv_obj_set_style_border_opa(s_wear_frame, LV_OPA_50, LV_PART_MAIN);
		break;
	default:
		lv_obj_set_style_border_width(s_wear_frame, 0, LV_PART_MAIN);
		break;
	}
}

static void update_status_line(void)
{
	char buf[56];

	if (!ui_is_obj_valid(s_status_label, UI_SCREEN_DEVICE_INFO)) {
		return;
	}

#if defined(CONFIG_APP_HEALTH_SCHED)
	if (health_sched_is_busy()) {
		lv_label_set_text(s_status_label, "Scanning...");
		lv_obj_set_style_text_color(s_status_label, NS_COLOR_INFO, LV_PART_MAIN);
		return;
	}

	uint32_t next = health_sched_seconds_to_next();

	if (next > 0U) {
		snprintf(buf, sizeof(buf), "Next scan  %u:%02u",
			 next / 60U, next % 60U);
		lv_label_set_text(s_status_label, buf);
		lv_obj_set_style_text_color(s_status_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		return;
	}
#endif

	lv_label_set_text(s_status_label, "Swipe for results");
	lv_obj_set_style_text_color(s_status_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
}

static void update_clock(void)
{
	struct tm rtc_tm;
	char buf[48];

	if (rtc_read_time(&rtc_tm) != 0) {
		if (ui_is_obj_valid(s_time_label, UI_SCREEN_DEVICE_INFO)) {
			lv_label_set_text(s_time_label, "--:--");
		}
		if (ui_is_obj_valid(s_seconds_label, UI_SCREEN_DEVICE_INFO)) {
			lv_label_set_text(s_seconds_label, "--");
		}
		if (ui_is_obj_valid(s_date_label, UI_SCREEN_DEVICE_INFO)) {
			lv_label_set_text(s_date_label, "Date unavailable");
		}
		if (ui_is_obj_valid(s_weekday_label, UI_SCREEN_DEVICE_INFO)) {
			lv_label_set_text(s_weekday_label, "");
		}
		return;
	}

	if (ui_is_obj_valid(s_time_label, UI_SCREEN_DEVICE_INFO)) {
		snprintf(buf, sizeof(buf), "%02d:%02d", rtc_tm.tm_hour, rtc_tm.tm_min);
		lv_label_set_text(s_time_label, buf);
	}
	if (ui_is_obj_valid(s_seconds_label, UI_SCREEN_DEVICE_INFO)) {
		snprintf(buf, sizeof(buf), "%02d", rtc_tm.tm_sec);
		lv_label_set_text(s_seconds_label, buf);
		if (ui_is_obj_valid(s_time_label, UI_SCREEN_DEVICE_INFO)) {
			lv_obj_align_to(s_seconds_label, s_time_label,
					LV_ALIGN_OUT_RIGHT_TOP, 6, 0);
		}
	}

	/* Refresh date/weekday when calendar minute (or day) changes. */
	uint32_t min_key =
		((uint32_t)(rtc_tm.tm_year & 0xFF) << 24) |
		((uint32_t)(rtc_tm.tm_mon & 0x0F) << 20) |
		((uint32_t)(rtc_tm.tm_mday & 0x1F) << 15) |
		((uint32_t)(rtc_tm.tm_hour & 0x1F) << 10) |
		((uint32_t)(rtc_tm.tm_min & 0x3F));

	if (min_key != s_last_shown_min) {
		s_last_shown_min = min_key;

		if (ui_is_obj_valid(s_weekday_label, UI_SCREEN_DEVICE_INFO)) {
			int w = rtc_tm.tm_wday;

			if (w < 0 || w > 6) {
				w = 0;
			}
			lv_label_set_text(s_weekday_label, s_weekdays[w]);
		}
		if (ui_is_obj_valid(s_date_label, UI_SCREEN_DEVICE_INFO)) {
			int m = rtc_tm.tm_mon;

			if (m < 0 || m > 11) {
				m = 0;
			}
			snprintf(buf, sizeof(buf), "%s %d, %04d",
				 s_months[m], rtc_tm.tm_mday, rtc_tm.tm_year + 1900);
			lv_label_set_text(s_date_label, buf);
		}
	}
}

static void update_device_info_screen(void)
{
	if (ui_get_screen(UI_SCREEN_DEVICE_INFO) == NULL) {
		return;
	}

#if defined(CONFIG_APP_TEMP_MONITOR)
	if (s_wear_state < 0) {
		s_wear_state = temp_get_wear_state();
	}
#endif
	update_clock();
	update_status_line();
	apply_home_wear_border();
}

void device_info_ui_create_screen(lv_obj_t *screen)
{
	if (screen == NULL) {
		return;
	}

	ns_apply_screen_bg(screen);

	s_wear_frame = lv_obj_create(screen);
	if (s_wear_frame != NULL) {
		lv_obj_remove_style_all(s_wear_frame);
		lv_obj_set_size(s_wear_frame, 236, 236);
		lv_obj_center(s_wear_frame);
		lv_obj_set_style_radius(s_wear_frame, 16, LV_PART_MAIN);
		lv_obj_set_style_border_width(s_wear_frame, 0, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(s_wear_frame, LV_OPA_TRANSP, LV_PART_MAIN);
		lv_obj_clear_flag(s_wear_frame, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_clear_flag(s_wear_frame, LV_OBJ_FLAG_SCROLLABLE);
	}

	s_weekday_label = lv_label_create(screen);
	if (s_weekday_label != NULL) {
		lv_label_set_text(s_weekday_label, "");
		lv_obj_set_style_text_font(s_weekday_label, UI_FONT_14, LV_PART_MAIN);
		lv_obj_set_style_text_color(s_weekday_label, NS_COLOR_SECONDARY, LV_PART_MAIN);
		lv_obj_align(s_weekday_label, LV_ALIGN_TOP_MID, 0, UI_SHELL_TOP_INSET + 28);
	}

	/* HH:MM — hero clock (Montserrat 48 when compiled fonts; XIP uses large). */
	s_time_label = lv_label_create(screen);
	if (s_time_label != NULL) {
		lv_label_set_text(s_time_label, "--:--");
		lv_obj_set_style_text_font(s_time_label, UI_FONT_CLOCK, LV_PART_MAIN);
		lv_obj_set_style_text_color(s_time_label, NS_COLOR_TEXT, LV_PART_MAIN);
		/* Slightly below vertical center; seconds sit at top-right of HH:MM. */
		lv_obj_align(s_time_label, LV_ALIGN_CENTER, -16, -8);
	}

	s_seconds_label = lv_label_create(screen);
	if (s_seconds_label != NULL) {
		lv_label_set_text(s_seconds_label, "--");
		lv_obj_set_style_text_font(s_seconds_label, UI_FONT_20, LV_PART_MAIN);
		lv_obj_set_style_text_color(s_seconds_label, NS_COLOR_PRIMARY, LV_PART_MAIN);
		if (s_time_label != NULL) {
			lv_obj_align_to(s_seconds_label, s_time_label,
					LV_ALIGN_OUT_RIGHT_TOP, 6, 0);
		} else {
			lv_obj_align(s_seconds_label, LV_ALIGN_CENTER, 100, -10);
		}
	}

	s_date_label = lv_label_create(screen);
	if (s_date_label != NULL) {
		lv_label_set_text(s_date_label, "");
		lv_obj_set_style_text_font(s_date_label, UI_FONT_20, LV_PART_MAIN);
		lv_obj_set_style_text_color(s_date_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		lv_obj_align(s_date_label, LV_ALIGN_CENTER, 0, 36);
	}

	s_status_label = lv_label_create(screen);
	if (s_status_label != NULL) {
		lv_label_set_text(s_status_label, "Swipe for results");
		lv_obj_set_style_text_font(s_status_label, UI_FONT_14, LV_PART_MAIN);
		lv_obj_set_style_text_color(s_status_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		lv_obj_align(s_status_label, LV_ALIGN_BOTTOM_MID, 0, -18);
	}

	s_last_shown_min = UINT32_MAX;
#if defined(CONFIG_APP_TEMP_MONITOR)
	s_wear_state = temp_get_wear_state();
#endif
	update_device_info_screen();
}

void device_info_ui_update_screen(void)
{
	update_device_info_screen();
}

void device_info_ui_invalidate_clock(void)
{
	s_last_shown_min = UINT32_MAX;
}

void device_info_ui_on_fonts_ready(void)
{
	if (ui_is_obj_valid(s_time_label, UI_SCREEN_DEVICE_INFO)) {
		lv_obj_set_style_text_font(s_time_label, UI_FONT_CLOCK, LV_PART_MAIN);
	}
	if (ui_is_obj_valid(s_seconds_label, UI_SCREEN_DEVICE_INFO)) {
		lv_obj_set_style_text_font(s_seconds_label, UI_FONT_20, LV_PART_MAIN);
	}
	if (ui_is_obj_valid(s_date_label, UI_SCREEN_DEVICE_INFO)) {
		lv_obj_set_style_text_font(s_date_label, UI_FONT_20, LV_PART_MAIN);
	}
	if (ui_is_obj_valid(s_weekday_label, UI_SCREEN_DEVICE_INFO)) {
		lv_obj_set_style_text_font(s_weekday_label, UI_FONT_14, LV_PART_MAIN);
	}
	if (ui_is_obj_valid(s_status_label, UI_SCREEN_DEVICE_INFO)) {
		lv_obj_set_style_text_font(s_status_label, UI_FONT_14, LV_PART_MAIN);
	}
}

void device_info_ui_cleanup_screen(void)
{
	s_wear_frame = NULL;
	s_time_label = NULL;
	s_seconds_label = NULL;
	s_date_label = NULL;
	s_weekday_label = NULL;
	s_status_label = NULL;
	s_last_shown_min = UINT32_MAX;
}

static void home_refresh_async(void *user)
{
	ARG_UNUSED(user);
	device_info_ui_update_screen();
}

static void on_home_ui_event(const struct ui_event *ev, void *user)
{
	ARG_UNUSED(user);

	if (ev == NULL) {
		return;
	}

	switch (ev->type) {
	case UI_EVENT_MEAS_CYCLE_STARTED:
	case UI_EVENT_MEAS_CYCLE_COMPLETE:
	case UI_EVENT_MEAS_STEP_TIMEOUT:
	case UI_EVENT_TEMP_UPDATED:
		lv_async_call(home_refresh_async, NULL);
		break;
	case UI_EVENT_WEAR_CHANGED:
		s_wear_state = ev->data.wear_state;
		lv_async_call(home_refresh_async, NULL);
		break;
	default:
		break;
	}
}

int device_info_ui_init(void)
{
	int ret = ui_events_subscribe(on_home_ui_event, NULL);

	if (ret < 0) {
		LOG_ERR("Home screen event subscribe failed: %d", ret);
	}
	return ret;
}
