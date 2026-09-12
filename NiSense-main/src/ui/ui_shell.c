#include "ui_shell.h"
#include "power_batt.h"
#include "power_mgr.h"

#include "ui_theme.h"
#include "ui_common.h"
#include "rtc.h"
#include "resource_fonts.h"
#include "resource_icons.h"
#include "resource.h"
#include "ui_events.h"
#include "diag_monitor.h"

#if defined(CONFIG_BT)
#include "ble_gatt.h"
#endif

#if defined(CONFIG_WIFI)
#include <zephyr/device.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#endif

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(ui_shell, CONFIG_UI_SHELL_LOG_LEVEL);

#define SHELL_H UI_SHELL_HEIGHT

#if defined(CONFIG_LV_FONT_MONTSERRAT_14)
#define SHELL_FONT (&lv_font_montserrat_14)
#else
#define SHELL_FONT UI_FONT_14
#endif

static lv_obj_t *s_bar;
static lv_obj_t *s_time_label;
static lv_obj_t *s_ble_icon;
static lv_obj_t *s_wifi_icon;
static lv_obj_t *s_wear_dot;
static lv_obj_t *s_battery_icon;
static lv_obj_t *s_battery_fault_draw; /* body outline for 20px strip */
static lv_obj_t *s_battery_fault_tip;
static lv_obj_t *s_battery_label;

static uint32_t s_last_update_ms;
static uint32_t s_last_batt_ms;
static int s_cached_batt = -1;
static int s_wear_state = -1;
static bool s_prox_debug_blink;
static bool s_prox_blink_on;
static bool s_created;

static const char *ui_shell_battery_symbol_for_soc(int soc)
{
	if (soc < 0) {
		return LV_SYMBOL_BATTERY_EMPTY;
	}
	if (soc < 20) {
		return LV_SYMBOL_BATTERY_EMPTY;
	}
	if (soc < 40) {
		return LV_SYMBOL_BATTERY_1;
	}
	if (soc < 60) {
		return LV_SYMBOL_BATTERY_2;
	}
	if (soc < 80) {
		return LV_SYMBOL_BATTERY_3;
	}
	return LV_SYMBOL_BATTERY_FULL;
}

static int ui_shell_read_battery_percent(void)
{
	int soc = -1;

	if (power_mgr_get_soc(&soc) == 0) {
		return soc;
	}
	return -1;
}

/**
 * Status strip is only 20 px tall — 40×40 XIP icons scale to unreadable
 * speckles. Always use a crisp drawn battery + "!" for pack fault.
 */
static void ui_shell_ensure_battery_fault_visual(void)
{
	const int16_t body_w = 24;
	const int16_t body_h = 12;

	if (s_bar == NULL || !lv_obj_is_valid(s_bar)) {
		return;
	}

	if (s_battery_fault_draw == NULL) {
		s_battery_fault_draw = lv_obj_create(s_bar);
		if (s_battery_fault_draw != NULL) {
			lv_obj_remove_style_all(s_battery_fault_draw);
			lv_obj_set_size(s_battery_fault_draw, body_w, body_h);
			lv_obj_set_style_radius(s_battery_fault_draw, 2, LV_PART_MAIN);
			lv_obj_set_style_border_width(s_battery_fault_draw, 2, LV_PART_MAIN);
			lv_obj_set_style_border_color(s_battery_fault_draw, NS_COLOR_CRITICAL,
						      LV_PART_MAIN);
			lv_obj_set_style_bg_opa(s_battery_fault_draw, LV_OPA_TRANSP, LV_PART_MAIN);
			lv_obj_align(s_battery_fault_draw, LV_ALIGN_RIGHT_MID, -8, 0);
			lv_obj_add_flag(s_battery_fault_draw, LV_OBJ_FLAG_HIDDEN);
			lv_obj_clear_flag(s_battery_fault_draw,
					  LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
		}
	}
	if (s_battery_fault_tip == NULL && s_battery_fault_draw != NULL) {
		s_battery_fault_tip = lv_obj_create(s_bar);
		if (s_battery_fault_tip != NULL) {
			lv_obj_remove_style_all(s_battery_fault_tip);
			lv_obj_set_size(s_battery_fault_tip, 3, 6);
			lv_obj_set_style_radius(s_battery_fault_tip, 1, LV_PART_MAIN);
			lv_obj_set_style_bg_color(s_battery_fault_tip, NS_COLOR_CRITICAL,
						  LV_PART_MAIN);
			lv_obj_set_style_bg_opa(s_battery_fault_tip, LV_OPA_COVER, LV_PART_MAIN);
			lv_obj_align_to(s_battery_fault_tip, s_battery_fault_draw,
					LV_ALIGN_OUT_RIGHT_MID, 1, 0);
			lv_obj_add_flag(s_battery_fault_tip, LV_OBJ_FLAG_HIDDEN);
			lv_obj_clear_flag(s_battery_fault_tip,
					  LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
		}
	}
}

static void ui_shell_set_battery_fault_visible(bool fault)
{
	ui_shell_ensure_battery_fault_visual();

	if (fault) {
		if (s_battery_icon != NULL) {
			lv_obj_add_flag(s_battery_icon, LV_OBJ_FLAG_HIDDEN);
		}
		if (s_battery_fault_draw != NULL) {
			lv_obj_clear_flag(s_battery_fault_draw, LV_OBJ_FLAG_HIDDEN);
		}
		if (s_battery_fault_tip != NULL) {
			lv_obj_clear_flag(s_battery_fault_tip, LV_OBJ_FLAG_HIDDEN);
		}
		/* "!" inside the cell — Montserrat renders this; XIP does not fit. */
		if (s_battery_label != NULL) {
			lv_label_set_text(s_battery_label, "!");
			lv_obj_set_style_text_color(s_battery_label, NS_COLOR_CRITICAL,
						    LV_PART_MAIN);
			lv_obj_align(s_battery_label, LV_ALIGN_RIGHT_MID, -16, -1);
		}
		return;
	}

	if (s_battery_fault_draw != NULL) {
		lv_obj_add_flag(s_battery_fault_draw, LV_OBJ_FLAG_HIDDEN);
	}
	if (s_battery_fault_tip != NULL) {
		lv_obj_add_flag(s_battery_fault_tip, LV_OBJ_FLAG_HIDDEN);
	}
	if (s_battery_icon != NULL) {
		lv_obj_clear_flag(s_battery_icon, LV_OBJ_FLAG_HIDDEN);
	}
	if (s_battery_label != NULL) {
		lv_obj_align(s_battery_label, LV_ALIGN_RIGHT_MID, -4, 0);
	}
}

static void ui_shell_apply_wear_visuals(void)
{
	if (s_wear_dot == NULL) {
		return;
	}

	if (s_prox_debug_blink) {
		if (s_prox_blink_on) {
			lv_obj_set_style_bg_color(s_wear_dot, NS_COLOR_WARNING, LV_PART_MAIN);
			lv_obj_clear_flag(s_wear_dot, LV_OBJ_FLAG_HIDDEN);
		} else {
			lv_obj_add_flag(s_wear_dot, LV_OBJ_FLAG_HIDDEN);
		}
		return;
	}

	switch (s_wear_state) {
	case 2:
		lv_obj_set_style_bg_color(s_wear_dot, NS_COLOR_NORMAL, LV_PART_MAIN);
		lv_obj_clear_flag(s_wear_dot, LV_OBJ_FLAG_HIDDEN);
		break;
	case 1:
		lv_obj_set_style_bg_color(s_wear_dot, NS_COLOR_WARNING, LV_PART_MAIN);
		lv_obj_clear_flag(s_wear_dot, LV_OBJ_FLAG_HIDDEN);
		break;
	case 0:
		lv_obj_set_style_bg_color(s_wear_dot, NS_COLOR_CRITICAL, LV_PART_MAIN);
		lv_obj_clear_flag(s_wear_dot, LV_OBJ_FLAG_HIDDEN);
		break;
	default:
		lv_obj_add_flag(s_wear_dot, LV_OBJ_FLAG_HIDDEN);
		break;
	}
}

/*
 * Status-bar connectivity colors (same severity tokens as battery):
 *   red    (CRITICAL) — feature / hardware not available
 *   yellow (WARNING)  — available but init/config failed or not up yet
 *   green  (NORMAL)   — success (BLE ready/connected, Wi-Fi associated)
 */
#if defined(CONFIG_WIFI)
static bool ui_shell_wifi_device_available(void)
{
	const struct device *dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(wifi0));

	return dev != NULL;
}

static bool ui_shell_wifi_device_ready(void)
{
	const struct device *dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(wifi0));

	return (dev != NULL) && device_is_ready(dev);
}

static bool ui_shell_wifi_is_connected(void)
{
	const struct device *dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(wifi0));
	struct net_if *iface;
	struct wifi_iface_status status = { 0 };

	if (dev == NULL || !device_is_ready(dev)) {
		return false;
	}

	iface = net_if_lookup_by_dev(dev);
	if (iface == NULL) {
		return false;
	}

	if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status, sizeof(status)) != 0) {
		return false;
	}

	return status.state == WIFI_STATE_COMPLETED;
}
#endif

static void ui_shell_apply_connectivity_visuals(void)
{
	if (s_ble_icon != NULL) {
#if defined(CONFIG_BT)
		lv_color_t ble_color;

		if (ble_gatt_is_connected() || ble_gatt_is_ready()) {
			ble_color = NS_COLOR_NORMAL;
		} else if (ble_gatt_init_failed()) {
			ble_color = NS_COLOR_WARNING;
		} else {
			/* Still starting (async bt_enable) — treat as not-yet-OK. */
			ble_color = NS_COLOR_WARNING;
		}
		lv_obj_set_style_text_color(s_ble_icon, ble_color, LV_PART_MAIN);
#else
		lv_obj_set_style_text_color(s_ble_icon, NS_COLOR_CRITICAL, LV_PART_MAIN);
#endif
	}

	if (s_wifi_icon != NULL) {
#if defined(CONFIG_WIFI)
		lv_color_t wifi_color;

		if (!ui_shell_wifi_device_available()) {
			wifi_color = NS_COLOR_CRITICAL;
		} else if (ui_shell_wifi_is_connected()) {
			wifi_color = NS_COLOR_NORMAL;
		} else if (!ui_shell_wifi_device_ready()) {
			wifi_color = NS_COLOR_WARNING;
		} else {
			/* Module up but not associated / configured. */
			wifi_color = NS_COLOR_WARNING;
		}
		lv_obj_set_style_text_color(s_wifi_icon, wifi_color, LV_PART_MAIN);
#else
		lv_obj_set_style_text_color(s_wifi_icon, NS_COLOR_CRITICAL, LV_PART_MAIN);
#endif
	}
}

static void ui_shell_apply_wear_visuals_async_cb(void *user_data)
{
	ARG_UNUSED(user_data);
	ui_shell_apply_wear_visuals();
}

static void ui_shell_apply_connectivity_async_cb(void *user_data)
{
	ARG_UNUSED(user_data);
	ui_shell_apply_connectivity_visuals();
}

#if defined(CONFIG_APP_FEATURE_DIAG_MONITOR)
static void ui_shell_status_long_press_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) {
		return;
	}

	diag_monitor_dump_now();
	LOG_INF("Memory dump triggered (status bar long-press)");
}
#endif

static void ui_shell_schedule_wear_visuals(void)
{
	if (lv_async_call(ui_shell_apply_wear_visuals_async_cb, NULL) != LV_RESULT_OK) {
		ui_shell_apply_wear_visuals();
	}
}

static void ui_shell_schedule_connectivity_visuals(void)
{
	if (lv_async_call(ui_shell_apply_connectivity_async_cb, NULL) != LV_RESULT_OK) {
		ui_shell_apply_connectivity_visuals();
	}
}

void ui_shell_set_proximity_debug_blink(bool blink)
{
	s_prox_debug_blink = blink;
	if (!blink) {
		s_prox_blink_on = false;
	}
	ui_shell_schedule_wear_visuals();
}

void ui_shell_set_wear_state(int wear_state)
{
	if (wear_state < 0 || wear_state > 2) {
		s_wear_state = -1;
	} else {
		s_wear_state = wear_state;
	}
	ui_shell_schedule_wear_visuals();
}

static void on_shell_ui_event(const struct ui_event *ev, void *user)
{
	ARG_UNUSED(user);

	if (ev == NULL) {
		return;
	}

	if (ev->type == UI_EVENT_WEAR_CHANGED) {
		ui_shell_set_wear_state(ev->data.wear_state);
	} else if (ev->type == UI_EVENT_BLE_CONNECTED ||
		   ev->type == UI_EVENT_BLE_DISCONNECTED) {
		ui_shell_schedule_connectivity_visuals();
	}
}

static lv_obj_t *ui_shell_make_symbol(lv_obj_t *parent, const char *sym, lv_align_t align,
				      int x_ofs)
{
	lv_obj_t *lbl = lv_label_create(parent);

	if (lbl == NULL) {
		return NULL;
	}

	lv_label_set_text(lbl, sym);
	lv_obj_set_style_text_font(lbl, SHELL_FONT, LV_PART_MAIN);
	lv_obj_set_style_text_color(lbl, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
	lv_obj_align(lbl, align, x_ofs, 0);
	return lbl;
}

int ui_shell_create(void)
{
	lv_obj_t *layer;

	if (s_created && s_bar != NULL && lv_obj_is_valid(s_bar)) {
		return 0;
	}

	layer = lv_layer_top();
	if (layer == NULL) {
		LOG_ERR("lv_layer_top unavailable");
		return -ENODEV;
	}

	s_bar = lv_obj_create(layer);
	if (s_bar == NULL) {
		LOG_ERR("Status strip create failed");
		return -ENOMEM;
	}

	lv_obj_remove_style_all(s_bar);
	lv_obj_set_size(s_bar, 240, SHELL_H);
	lv_obj_align(s_bar, LV_ALIGN_TOP_MID, 0, 0);
	lv_obj_set_style_bg_color(s_bar, NS_COLOR_CARD, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(s_bar, LV_OPA_60, LV_PART_MAIN);
	lv_obj_set_style_border_color(s_bar, NS_COLOR_BORDER, LV_PART_MAIN);
	lv_obj_set_style_border_width(s_bar, 1, LV_PART_MAIN);
	lv_obj_set_style_pad_all(s_bar, 0, LV_PART_MAIN);
	lv_obj_clear_flag(s_bar, LV_OBJ_FLAG_SCROLLABLE);
	/* Let gestures pass through to the active screen underneath. */
	lv_obj_clear_flag(s_bar, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_flag(s_bar, LV_OBJ_FLAG_EVENT_BUBBLE);

	s_ble_icon = ui_shell_make_symbol(s_bar, LV_SYMBOL_BLUETOOTH, LV_ALIGN_LEFT_MID, 2);
	s_wifi_icon = ui_shell_make_symbol(s_bar, LV_SYMBOL_WIFI, LV_ALIGN_LEFT_MID, 22);
	s_battery_icon = ui_shell_make_symbol(s_bar, LV_SYMBOL_BATTERY_FULL, LV_ALIGN_RIGHT_MID,
					      -38);
	s_battery_fault_draw = NULL;
	s_battery_fault_tip = NULL;
	s_time_label = lv_label_create(s_bar);
	s_battery_label = lv_label_create(s_bar);
	s_wear_dot = lv_obj_create(s_bar);

	if (s_wear_dot != NULL) {
		lv_obj_remove_style_all(s_wear_dot);
		lv_obj_set_size(s_wear_dot, 10, 10);
		lv_obj_set_style_radius(s_wear_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
		lv_obj_set_style_bg_opa(s_wear_dot, LV_OPA_COVER, LV_PART_MAIN);
		lv_obj_align(s_wear_dot, LV_ALIGN_LEFT_MID, 42, 0);
		lv_obj_clear_flag(s_wear_dot, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
		lv_obj_add_flag(s_wear_dot, LV_OBJ_FLAG_HIDDEN);
	}

	if (s_time_label != NULL) {
		lv_label_set_text(s_time_label, "--:--");
		lv_obj_set_style_text_color(s_time_label, NS_COLOR_TEXT, LV_PART_MAIN);
		lv_obj_set_style_text_font(s_time_label, SHELL_FONT, LV_PART_MAIN);
		lv_obj_align(s_time_label, LV_ALIGN_CENTER, 0, 0);
#if defined(CONFIG_APP_FEATURE_DIAG_MONITOR)
		lv_obj_add_flag(s_time_label, LV_OBJ_FLAG_CLICKABLE);
		lv_obj_add_event_cb(s_time_label, ui_shell_status_long_press_cb,
				    LV_EVENT_LONG_PRESSED, NULL);
#endif
	}

	if (s_battery_label != NULL) {
		lv_label_set_text(s_battery_label, "--%");
		lv_obj_set_style_text_color(s_battery_label, NS_COLOR_TEXT, LV_PART_MAIN);
		lv_obj_set_style_text_font(s_battery_label, SHELL_FONT, LV_PART_MAIN);
		lv_obj_align(s_battery_label, LV_ALIGN_RIGHT_MID, -4, 0);
	}

	lv_obj_move_foreground(s_bar);

	s_last_update_ms = 0;
	s_last_batt_ms = 0;
	s_cached_batt = -1;
	s_created = true;

	ui_shell_apply_wear_visuals();
	ui_shell_apply_connectivity_visuals();

	LOG_INF("Status strip created on lv_layer_top (persistent)");
	return 0;
}

void ui_shell_attach(lv_obj_t *screen)
{
	ARG_UNUSED(screen);
	(void)ui_shell_create();
}

void ui_shell_detach(lv_obj_t *screen)
{
	ARG_UNUSED(screen);
	/* Strip lives on lv_layer_top; not owned by screen roots. */
}

void ui_shell_activate(lv_obj_t *screen)
{
	ARG_UNUSED(screen);

	if (!s_created || s_bar == NULL || !lv_obj_is_valid(s_bar)) {
		(void)ui_shell_create();
		return;
	}

	/* Keep strip above any new content on the top layer (overlays may still
	 * draw later and cover it — by design). */
	lv_obj_move_foreground(s_bar);
	s_last_update_ms = 0;
	ui_shell_apply_wear_visuals();
	ui_shell_apply_connectivity_visuals();
}

int ui_shell_init(void)
{
	int ret = ui_shell_create();

	if (ret != 0) {
		return ret;
	}

	return ui_events_subscribe(on_shell_ui_event, NULL);
}

void ui_shell_update(void)
{
	uint32_t now_ms;
	uint32_t ts = 0;
	char buf[16];

	if (s_bar == NULL || !lv_obj_is_valid(s_bar)) {
		return;
	}

	now_ms = k_uptime_get_32();
	if (now_ms - s_last_update_ms < 500U) {
		return;
	}
	s_last_update_ms = now_ms;

	if (s_prox_debug_blink) {
		s_prox_blink_on = !s_prox_blink_on;
		ui_shell_apply_wear_visuals();
	}

	if (rtc_get_unix_time(&ts) == 0 && ts > 0U && s_time_label != NULL) {
		uint32_t sec_day = ts % 86400U;
		uint32_t hh = sec_day / 3600U;
		uint32_t mm = (sec_day % 3600U) / 60U;

		snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned int)hh, (unsigned int)mm);
		lv_label_set_text(s_time_label, buf);
	}

	if (now_ms - s_last_batt_ms >= 5000U) {
		s_cached_batt = ui_shell_read_battery_percent();
		s_last_batt_ms = now_ms;
	}
	if (s_battery_label != NULL) {
		struct power_batt_snapshot snap;
		bool pack_fault = false;

		if (power_mgr_get_snapshot(&snap) == 0) {
			if (snap.cell_state == POWER_BATT_CELL_ABSENT) {
				pack_fault = true;
			} else if (snap.usb_online &&
				   snap.cell_state == POWER_BATT_CELL_UNKNOWN &&
				   snap.fg_pack_absent) {
				int32_t i_abs = snap.current_ua >= 0 ? snap.current_ua :
								       -snap.current_ua;

				/* Match power_batt USB-bench "no pack" SoC=0 path. */
				if (i_abs < 30000) {
					pack_fault = true;
				}
			}
		}

		if (pack_fault) {
			ui_shell_set_battery_fault_visible(true);
		} else if (s_cached_batt >= 0) {
			lv_color_t batt_color;

			ui_shell_set_battery_fault_visible(false);
			snprintf(buf, sizeof(buf), "%d%%", s_cached_batt);
			lv_label_set_text(s_battery_label, buf);
			if (s_cached_batt >= 50) {
				batt_color = NS_COLOR_NORMAL;
			} else if (s_cached_batt >= 20) {
				batt_color = NS_COLOR_WARNING;
			} else {
				batt_color = NS_COLOR_CRITICAL;
			}
			lv_obj_set_style_text_color(s_battery_label, batt_color, LV_PART_MAIN);
			if (s_battery_icon != NULL) {
				lv_label_set_text(s_battery_icon,
						  ui_shell_battery_symbol_for_soc(s_cached_batt));
				lv_obj_set_style_text_color(s_battery_icon, batt_color, LV_PART_MAIN);
			}
		} else {
			ui_shell_set_battery_fault_visible(false);
			if (s_battery_icon != NULL) {
				lv_label_set_text(s_battery_icon, "--");
				lv_obj_set_style_text_color(s_battery_icon, NS_COLOR_TEXT_DIM,
							    LV_PART_MAIN);
			}
			lv_label_set_text(s_battery_label, "--%");
			lv_obj_set_style_text_color(s_battery_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		}
	}

	ui_shell_apply_connectivity_visuals();
}
