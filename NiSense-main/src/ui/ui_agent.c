/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * ui_agent.c — Agentic UI presenter (results slideshow).
 *
 * Listens on the measurement event bus. When the health scheduler finishes a
 * cycle it builds a full-screen overlay on the LVGL top layer and auto-cycles
 * big icon + value cards. The overlay floats above the active screen, so the
 * normal swipe carousel underneath is untouched and "returns" automatically
 * when the overlay is dismissed (touch or loop cap).
 *
 * Threading: events arrive from the system work queue. LVGL is single-threaded,
 * so every UI mutation is deferred to the LVGL context with lv_async_call() —
 * the same pattern ui.c uses for host-initiated screen switches.
 *
 * @author Ponmadasamy Muthuraj <ponmadasamy@live.com>
 */

#include "ui_agent.h"

#ifdef CONFIG_APP_UI_AGENT

#include "ui_events.h"
#include "ui_hold_overlay.h"
#include "ui_obj_track.h"
#include "ui_common.h"   /* UI_FONT_*, last_gesture_time */
#include "ui_theme.h"
#if defined(CONFIG_UI_SCREEN_BLE) && defined(CONFIG_BT)
#include "ble_gatt.h"
#endif
#include "resource_icons.h"
#include "resource.h"
#include "resource_fonts.h"
#include "temp.h"

#include <lvgl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

#if defined(CONFIG_APP_FEATURE_PPG)
#include "ppg.h"
#endif
#if defined(CONFIG_GLUCOSE_SENSOR)
#include "glucose.h"
#include <zephyr/drivers/sensor/glucose_sensor.h>
#endif

LOG_MODULE_REGISTER(ui_agent, CONFIG_UI_AGENT_LOG_LEVEL);

#define SLIDE_MS    CONFIG_APP_UI_AGENT_SLIDE_MS
#define MAX_LOOPS   CONFIG_APP_UI_AGENT_MAX_LOOPS
#define DEFER_MS    CONFIG_APP_UI_AGENT_DEFER_MS
#define MAX_SLIDES  10
#define AGENT_SLUG  "agent_slideshow"

struct agent_slide {
	const lv_image_dsc_t *icon;
	const char           *title;
	char                  value[16];
	const char           *unit;
	lv_color_t            accent;
	enum ns_status        status;
};

/* All LVGL state below is touched only from the LVGL context. */
static lv_obj_t   *s_overlay;
static lv_obj_t   *s_icon;
static lv_obj_t   *s_title;
static lv_obj_t   *s_value;
static lv_obj_t   *s_unit;
static lv_obj_t   *s_dot;
static lv_obj_t   *s_index;
static lv_timer_t *s_timer;

static struct agent_slide s_slides[MAX_SLIDES];
static uint8_t s_slide_count;
static uint8_t s_slide_idx;
static uint16_t s_loops;

static const lv_image_dsc_t *safe_xip_icon(uint16_t asset_id)
{
	const lv_image_dsc_t *icon = xip_icon(asset_id);

	if (icon == NULL || icon->data == NULL || icon->header.w == 0U ||
	    icon->header.h == 0U) {
		return NULL;
	}
	return icon;
}

/* -------------------------------------------------------------------------
 * Slide construction (LVGL context).
 * ---------------------------------------------------------------------- */
static void add_slide(const lv_image_dsc_t *icon, const char *title,
		      const char *value, const char *unit, lv_color_t accent,
		      enum ns_status status)
{
	if (s_slide_count >= MAX_SLIDES) {
		return;
	}
	struct agent_slide *s = &s_slides[s_slide_count++];

	s->icon = icon;
	s->title = title;
	s->unit = unit;
	s->accent = accent;
	s->status = status;
	snprintf(s->value, sizeof(s->value), "%s", value);
}

#if defined(CONFIG_GLUCOSE_SENSOR)
static enum ns_status glucose_status(float mg_dl)
{
	if (mg_dl < (float)CONFIG_APP_HEALTH_GLUCOSE_CRIT_LOW ||
	    mg_dl > (float)CONFIG_APP_HEALTH_GLUCOSE_CRIT_HIGH) {
		return NS_STATUS_CRITICAL;
	}
	if (mg_dl < (float)CONFIG_APP_HEALTH_GLUCOSE_ABN_LOW ||
	    mg_dl > (float)CONFIG_APP_HEALTH_GLUCOSE_ABN_HIGH) {
		return NS_STATUS_RISK;
	}
	return NS_STATUS_NORMAL;
}
#endif

static enum ns_status skin_band_to_status(enum temp_skin_band band)
{
	switch (band) {
	case TEMP_SKIN_LOW:      return NS_STATUS_INFO;
	case TEMP_SKIN_ELEVATED: return NS_STATUS_ATTENTION;
	case TEMP_SKIN_HIGH:     return NS_STATUS_RISK;
	case TEMP_SKIN_NORMAL:   return NS_STATUS_NORMAL;
	default:                 return NS_STATUS_NEUTRAL;
	}
}

/* Gather the latest results into the slide list. Returns count. */
static uint8_t build_slides(void)
{
	char buf[16];

	s_slide_count = 0;

#if defined(CONFIG_GLUCOSE_SENSOR)
	struct glucose_sensor_result gr;

	if (glucose_get_result(&gr) == 0 && (gr.flags & GLUCOSE_SENSOR_FLAG_VALID)) {
		snprintf(buf, sizeof(buf), "%u", (unsigned)gr.glucose_mg_dl);
		add_slide(safe_xip_icon(RESOURCE_ID_ICON_GLUCOSE), "BL-GLU", buf, "mg/dL",
			  NS_COLOR_GLUCOSE, glucose_status(gr.glucose_mg_dl));
	}
#endif

#if defined(CONFIG_APP_FEATURE_PPG)
	struct ppg_algo_result pr = {0};

	if (ppg_get_result(&pr) == 0) {
		if (pr.hr_valid && pr.hr_bpm > 0U) {
			snprintf(buf, sizeof(buf), "%u", pr.hr_bpm);
			add_slide(safe_xip_icon(RESOURCE_ID_ICON_HEART), "HR", buf,
				  "bpm", NS_COLOR_HR, NS_STATUS_NORMAL);
		}
		if (pr.spo2_valid && pr.spo2_percent > 0U) {
			snprintf(buf, sizeof(buf), "%u", pr.spo2_percent);
			add_slide(safe_xip_icon(RESOURCE_ID_ICON_SPO2), "SpO2", buf, "%",
				  NS_COLOR_SPO2,
				  (pr.spo2_percent < 92U) ? NS_STATUS_RISK : NS_STATUS_NORMAL);
		}
		if (pr.resp_valid && pr.resp_rate_bpm > 0U) {
			snprintf(buf, sizeof(buf), "%u", pr.resp_rate_bpm);
			add_slide(safe_xip_icon(RESOURCE_ID_ICON_RESP), "Resp", buf,
				  "bpm", NS_COLOR_RESP, NS_STATUS_NORMAL);
		}
		if (pr.hb_valid && pr.hb_g_dl_x10 > 0U) {
			snprintf(buf, sizeof(buf), "%u.%u",
				 pr.hb_g_dl_x10 / 10U, pr.hb_g_dl_x10 % 10U);
			add_slide(safe_xip_icon(RESOURCE_ID_ICON_HEMOGLOBIN), "BL-HEM", buf,
				  "g/dL", NS_COLOR_HB, NS_STATUS_NORMAL);
		}
		if (pr.hrv_valid && pr.sdnn_ms > 0U) {
			snprintf(buf, sizeof(buf), "%u", pr.sdnn_ms);
			add_slide(safe_xip_icon(RESOURCE_ID_ICON_HEART), "SDNN", buf,
				  "ms", NS_COLOR_HRV, NS_STATUS_NORMAL);
		}
		if (pr.bp_valid && pr.systolic_mmhg > 0U) {
			snprintf(buf, sizeof(buf), "%u/%u",
				 pr.systolic_mmhg, pr.diastolic_mmhg);
			add_slide(safe_xip_icon(RESOURCE_ID_ICON_HEART), "BP", buf,
				  "mmHg", NS_COLOR_BP_SYS, NS_STATUS_NORMAL);
		}
	}
#endif

	/* Body temp (wrist → °F). */
	struct temp_snapshot ts;

	temp_get_last(&ts);
	if (ts.wrist_valid) {
		float f = ts.wrist_c * 9.0f / 5.0f + 32.0f;

		snprintf(buf, sizeof(buf), "%.1f", (double)f);
		add_slide(safe_xip_icon(RESOURCE_ID_ICON_TEMP), "Body Temp", buf, "F",
			  NS_COLOR_TEMP, skin_band_to_status(temp_classify_skin(ts.wrist_c)));
	}

	return s_slide_count;
}

/* -------------------------------------------------------------------------
 * Overlay rendering (LVGL context).
 * ---------------------------------------------------------------------- */
static void show_slide(uint8_t idx)
{
	if (idx >= s_slide_count) {
		return;
	}
	const struct agent_slide *s = &s_slides[idx];

	if (s_icon) {
		if (s->icon) {
			lv_image_set_src(s_icon, s->icon);
			lv_obj_clear_flag(s_icon, LV_OBJ_FLAG_HIDDEN);
		} else {
			lv_obj_add_flag(s_icon, LV_OBJ_FLAG_HIDDEN);
		}
	}
	if (s_title) {
		lv_label_set_text(s_title, s->title);
	}
	if (s_value) {
		lv_label_set_text(s_value, s->value);
		lv_obj_set_style_text_color(s_value, s->accent, LV_PART_MAIN);
	}
	if (s_unit) {
		lv_label_set_text(s_unit, s->unit);
	}
	if (s_dot) {
		lv_obj_set_style_bg_color(s_dot, ns_status_color(s->status), LV_PART_MAIN);
	}
	if (s_index) {
		char ib[8];

		snprintf(ib, sizeof(ib), "%u/%u", (unsigned)(idx + 1), (unsigned)s_slide_count);
		lv_label_set_text(s_index, ib);
	}
}

static void destroy_overlay(void)
{
	if (s_timer != NULL) {
		lv_timer_delete(s_timer);
		s_timer = NULL;
	}

	if (s_overlay != NULL && lv_obj_is_valid(s_overlay)) {
		ui_obj_log_screen_delete_begin(AGENT_SLUG, lv_obj_get_child_count(s_overlay));
		lv_obj_delete(s_overlay);
		ui_obj_log_screen_delete_end(AGENT_SLUG);
	}

	s_overlay = NULL;
	s_icon = NULL;
	s_title = NULL;
	s_value = NULL;
	s_unit = NULL;
	s_dot = NULL;
	s_index = NULL;
	s_slide_count = 0;
	s_slide_idx = 0;
	s_loops = 0;
}

static void overlay_touch_cb(lv_event_t *e)
{
	ARG_UNUSED(e);
	LOG_DBG("Slideshow dismissed by touch");
	destroy_overlay();
}

static void slide_timer_cb(lv_timer_t *timer)
{
	ARG_UNUSED(timer);

	s_slide_idx++;
	if (s_slide_idx >= s_slide_count) {
		s_slide_idx = 0;
		s_loops++;
		if (MAX_LOOPS > 0 && s_loops >= (uint16_t)MAX_LOOPS) {
			LOG_DBG("Slideshow auto-dismissed after %u loops", s_loops);
			destroy_overlay();
			return;
		}
	}
	show_slide(s_slide_idx);
}

static void build_overlay(void)
{
	ui_hold_overlay_cancel();

	if (s_overlay != NULL && lv_obj_is_valid(s_overlay)) {
		destroy_overlay();
	}

	s_overlay = lv_obj_create(lv_layer_top());
	if (s_overlay == NULL) {
		LOG_WRN("Slideshow overlay alloc failed");
		return;
	}

	ui_obj_name_screen(s_overlay, AGENT_SLUG);
	ui_obj_log_screen_create(AGENT_SLUG);

	lv_obj_remove_style_all(s_overlay);
	lv_obj_set_size(s_overlay, 240, 240);
	lv_obj_align(s_overlay, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_style_bg_color(s_overlay, NS_COLOR_BG, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_event_cb(s_overlay, overlay_touch_cb, LV_EVENT_CLICKED, NULL);

	lv_obj_t *hdr = ui_label_create(s_overlay, AGENT_SLUG, "header");

	lv_label_set_text(hdr, "Results");
	lv_obj_set_style_text_font(hdr, UI_FONT_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(hdr, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
	lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 14);

	s_icon = lv_image_create(s_overlay);
	ui_obj_name(s_icon, AGENT_SLUG, "icon");
	lv_obj_align(s_icon, LV_ALIGN_TOP_MID, 0, 44);

	s_dot = lv_obj_create(s_overlay);
	ui_obj_name(s_dot, AGENT_SLUG, "status_dot");
	lv_obj_remove_style_all(s_dot);
	lv_obj_set_size(s_dot, 12, 12);
	lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(s_dot, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_align(s_dot, LV_ALIGN_TOP_MID, 32, 50);

	s_title = ui_label_create(s_overlay, AGENT_SLUG, "title");
	lv_obj_set_style_text_font(s_title, UI_FONT_20, LV_PART_MAIN);
	lv_obj_set_style_text_color(s_title, NS_COLOR_TEXT, LV_PART_MAIN);
	lv_obj_align(s_title, LV_ALIGN_CENTER, 0, 6);

	s_value = ui_label_create(s_overlay, AGENT_SLUG, "value");
	lv_obj_set_style_text_font(s_value, UI_FONT_28, LV_PART_MAIN);
	lv_obj_align(s_value, LV_ALIGN_CENTER, 0, 40);

	s_unit = ui_label_create(s_overlay, AGENT_SLUG, "unit");
	lv_obj_set_style_text_font(s_unit, UI_FONT_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(s_unit, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
	lv_obj_align(s_unit, LV_ALIGN_CENTER, 0, 70);

	s_index = ui_label_create(s_overlay, AGENT_SLUG, "index");
	lv_obj_set_style_text_font(s_index, UI_FONT_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(s_index, NS_COLOR_SECONDARY, LV_PART_MAIN);
	lv_obj_align(s_index, LV_ALIGN_BOTTOM_MID, 0, -28);

	lv_obj_t *hint = ui_label_create(s_overlay, AGENT_SLUG, "hint");

	lv_label_set_text(hint, "Tap to dismiss");
	lv_obj_set_style_text_font(hint, UI_FONT_14, LV_PART_MAIN);
	lv_obj_set_style_text_color(hint, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
	lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -8);

	ui_obj_track_screen_tree(s_overlay, AGENT_SLUG);
}

/* Runs in LVGL context (via lv_async_call). */
static void slideshow_start_async(void *user)
{
	ARG_UNUSED(user);

	if (s_overlay != NULL && lv_obj_is_valid(s_overlay)) {
		return;  /* already showing */
	}

#if defined(CONFIG_RESOURCE_FONTS) && defined(CONFIG_APP_FONT_PROFILE_RESOURCE_RUNTIME)
	if (!resource_fonts_is_ready()) {
		LOG_WRN("Slideshow skipped: XIP fonts not ready");
		return;
	}
#endif

	/* Politeness: don't hijack the screen if the user is actively touching. */
	uint32_t since = k_uptime_get_32() - last_gesture_time;

	if (since < (uint32_t)DEFER_MS) {
		LOG_DBG("Slideshow deferred (user active %u ms ago)", since);
		return;
	}

	if (build_slides() == 0U) {
		LOG_DBG("Slideshow: no valid results to show");
		return;
	}

	build_overlay();
	if (!s_overlay) {
		return;
	}

	s_slide_idx = 0;
	s_loops = 0;
	show_slide(0);

	s_timer = lv_timer_create(slide_timer_cb, (uint32_t)SLIDE_MS, NULL);
	if (!s_timer) {
		LOG_WRN("Slideshow timer alloc failed");
		destroy_overlay();
		return;
	}
	LOG_INF("Slideshow started (%u cards)", s_slide_count);
}

/* -------------------------------------------------------------------------
 * BLE connection auto-navigation.
 *
 * On connect, jump to the BLE screen (remembering the screen we left) so the
 * wearer sees link + pairing status; on disconnect, restore the prior screen
 * unless the user has since navigated away themselves.
 * ---------------------------------------------------------------------- */
#if defined(CONFIG_UI_SCREEN_BLE) && defined(CONFIG_BT)
static bool s_ble_nav_active;
static enum ui_screen s_ble_prev_screen;
static struct k_work_delayable s_ble_nav_work;
static bool s_ble_nav_work_inited;

#define BLE_CONNECT_NAV_DELAY_MS 2000

static void ble_connect_nav_async(void *user)
{
	ARG_UNUSED(user);

	enum ui_screen cur = ui_get_current_screen();

	/* Don't stack navigations or fight the pairing auto-nav already on BLE. */
	if (s_ble_nav_active || cur == UI_SCREEN_BLE) {
		return;
	}

	/* Politeness: leave the user alone if they are actively interacting. */
	uint32_t since = k_uptime_get_32() - last_gesture_time;

	if (since < (uint32_t)DEFER_MS) {
		return;
	}

	/* A results slideshow floats on the top layer and would hide the BLE
	 * screen we are about to show — dismiss it first. */
	if (s_overlay != NULL) {
		destroy_overlay();
	}

	s_ble_prev_screen = cur;
	s_ble_nav_active = true;
	ui_request_screen(UI_SCREEN_BLE);
	LOG_INF("BLE connected: auto-nav to BLE screen (from %d)", cur);
}

static void ble_connect_nav_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	lv_async_call(ble_connect_nav_async, NULL);
}

static void ble_disconnect_nav_async(void *user)
{
	ARG_UNUSED(user);

	if (s_ble_nav_work_inited) {
		(void)k_work_cancel_delayable(&s_ble_nav_work);
	}

	if (!s_ble_nav_active) {
		return;
	}

	/* Do not yank the user off the BLE screen while pairing is active or
	 * has just failed — ui_agent's connect-nav restore was racing SMP and
	 * hid the passkey after ~300 ms (see disconnect during reason 19). */
	enum ble_pairing_state ps = ble_gatt_get_pairing_state();

	if (ps == BLE_PAIRING_CONFIRM_PASSKEY || ps == BLE_PAIRING_WAITING_PASSKEY ||
	    ps == BLE_PAIRING_BONDING || ps == BLE_PAIRING_FAILED) {
		s_ble_nav_active = false;
		return;
	}

	s_ble_nav_active = false;

	/* Only restore if the user is still on the BLE screen we navigated to;
	 * if they swiped elsewhere, respect their choice. */
	if (ui_get_current_screen() == UI_SCREEN_BLE) {
		ui_request_screen(s_ble_prev_screen);
		LOG_DBG("BLE disconnected: restoring screen %d", s_ble_prev_screen);
	}
}
#endif /* CONFIG_UI_SCREEN_BLE && CONFIG_BT */

/* -------------------------------------------------------------------------
 * Event subscription (work-queue context → defer to LVGL).
 * ---------------------------------------------------------------------- */
static void on_ui_event(const struct ui_event *ev, void *user)
{
	ARG_UNUSED(user);

	switch (ev->type) {
#if defined(CONFIG_PPG_ALGO_STAGED_LIVE)
	case UI_EVENT_PPG_PARTIAL_VITALS:
#if defined(CONFIG_UI_SCREEN_VITALS)
		ui_request_screen(UI_SCREEN_VITALS);
#endif
		break;
	case UI_EVENT_PPG_PARTIAL_VASCULAR:
#if defined(CONFIG_UI_SCREEN_VASCULAR)
		ui_request_screen(UI_SCREEN_VASCULAR);
#endif
		break;
	case UI_EVENT_PPG_RECORD_READY:
		/* Stay on Vascular (or Vitals) through live tail. */
		break;
#endif
	case UI_EVENT_MEAS_RESULT_READY:
		if (ev->data.meas_type == UI_MEAS_GLUCOSE) {
#if defined(CONFIG_UI_SCREEN_GLUCOSE)
			ui_request_screen(UI_SCREEN_GLUCOSE);
#endif
		} else if (ev->data.meas_type == UI_MEAS_VITALS) {
			/* PPG session ended (incl. live tail) — glucose step starts next. */
#if defined(CONFIG_UI_SCREEN_GLUCOSE) && defined(CONFIG_PPG_ALGO_STAGED_LIVE)
			ui_request_screen(UI_SCREEN_GLUCOSE);
#endif
		}
		break;
	case UI_EVENT_MEAS_CYCLE_COMPLETE:
		lv_async_call(slideshow_start_async, NULL);
		break;
#if defined(CONFIG_UI_SCREEN_BLE) && defined(CONFIG_BT)
	case UI_EVENT_BLE_CONNECTED:
		/* Delay screen create until after CCC/MTU/security settle —
		 * immediate auto-nav + FatFS seed starved LVGL/touch. */
		if (!s_ble_nav_work_inited) {
			k_work_init_delayable(&s_ble_nav_work, ble_connect_nav_work_handler);
			s_ble_nav_work_inited = true;
		}
		(void)k_work_reschedule(&s_ble_nav_work, K_MSEC(BLE_CONNECT_NAV_DELAY_MS));
		break;
	case UI_EVENT_BLE_DISCONNECTED:
		lv_async_call(ble_disconnect_nav_async, NULL);
		break;
#endif
	default:
		break;
	}
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */
bool ui_agent_slideshow_active(void)
{
	return s_overlay != NULL && lv_obj_is_valid(s_overlay);
}

void ui_agent_dismiss_slideshow(void)
{
	destroy_overlay();
}

int ui_agent_init(void)
{
	int ret = ui_events_subscribe(on_ui_event, NULL);

	if (ret < 0) {
		LOG_ERR("UI agent event subscribe failed: %d", ret);
		return ret;
	}
	LOG_INF("UI agent ready (slide %d ms, max loops %d, defer %d ms)",
		SLIDE_MS, MAX_LOOPS, DEFER_MS);
	return 0;
}

#endif /* CONFIG_APP_UI_AGENT */
