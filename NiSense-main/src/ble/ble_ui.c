/* =============================================================================
 * BLE UI Screen - Implementation
 * =============================================================================
 * BLE pairing and connection status UI screen
 * 
 * Features:
 * - Connection status display (connected/disconnected/advertising)
 * - Passkey display during pairing
 * - Accept/Reject buttons for passkey confirmation
 * - Connected device address display
 * - Signal strength indicator (when connected)
 * ============================================================================= */

#include "ble_ui.h"
#include "ble_gatt.h"
#include "ui_common.h"
#include "ui_theme.h"
#include "ui_shell.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <stdio.h>
#include <string.h>

LOG_MODULE_REGISTER(ble_ui, CONFIG_BLE_UI_LOG_LEVEL);

enum ble_ui_btn_mode {
	BLE_UI_BTN_MODE_NONE = 0,
	BLE_UI_BTN_MODE_PAIRING,
	BLE_UI_BTN_MODE_CONNECTED,
	BLE_UI_BTN_MODE_ADVERTISING,
};

static void accept_btn_event_cb(lv_event_t *e);
static void reject_btn_event_cb(lv_event_t *e);
static void disconnect_btn_event_cb(lv_event_t *e);
static void forget_btn_event_cb(lv_event_t *e);

/* =============================================================================
 * UI State
 * ============================================================================= */

static struct {
	lv_obj_t *screen;
	
	/* Status area */
	lv_obj_t *title_label;
	lv_obj_t *status_label;
	lv_obj_t *status_icon;
	
	/* Passkey area (shown during pairing) */
	lv_obj_t *passkey_container;
	lv_obj_t *passkey_label;
	lv_obj_t *passkey_value;
	lv_obj_t *passkey_hint;
	
	/* Pairing buttons */
	lv_obj_t *btn_container;
	lv_obj_t *accept_btn;
	lv_obj_t *reject_btn;

	/* Forget bonds button */
	lv_obj_t *forget_btn;
	lv_obj_t *bond_count_label;

	/* Device info area */
	lv_obj_t *info_container;
	lv_obj_t *device_addr_label;
	lv_obj_t *device_name_label;
	
	/* State tracking */
	bool initialized;
	enum ble_pairing_state last_pairing_state;
	uint32_t displayed_passkey;
	bool connected;

	enum ble_ui_btn_mode last_btn_mode;
} ble_ui_state;

static lv_obj_t *ble_ui_btn_label(lv_obj_t *btn)
{
	if (!ui_is_obj_valid(btn, UI_SCREEN_BLE)) {
		return NULL;
	}

	lv_obj_t *label = lv_obj_get_child(btn, 0);

	return ui_is_obj_valid(label, UI_SCREEN_BLE) ? label : NULL;
}

static void ble_ui_clear_button_callbacks(void)
{
	if (ui_is_obj_valid(ble_ui_state.accept_btn, UI_SCREEN_BLE)) {
		lv_obj_remove_event_cb(ble_ui_state.accept_btn, accept_btn_event_cb);
		lv_obj_remove_event_cb(ble_ui_state.accept_btn, disconnect_btn_event_cb);
	}
	if (ui_is_obj_valid(ble_ui_state.reject_btn, UI_SCREEN_BLE)) {
		lv_obj_remove_event_cb(ble_ui_state.reject_btn, reject_btn_event_cb);
		lv_obj_remove_event_cb(ble_ui_state.reject_btn, forget_btn_event_cb);
	}
}

static void ble_ui_apply_button_mode(enum ble_ui_btn_mode mode)
{
	if (mode == ble_ui_state.last_btn_mode) {
		return;
	}

	if (!ui_is_obj_valid(ble_ui_state.btn_container, UI_SCREEN_BLE) ||
	    !ui_is_obj_valid(ble_ui_state.accept_btn, UI_SCREEN_BLE) ||
	    !ui_is_obj_valid(ble_ui_state.reject_btn, UI_SCREEN_BLE)) {
		return;
	}

	ble_ui_clear_button_callbacks();

	switch (mode) {
	case BLE_UI_BTN_MODE_PAIRING:
		lv_obj_clear_flag(ble_ui_state.btn_container, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(ble_ui_state.accept_btn, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(ble_ui_state.reject_btn, LV_OBJ_FLAG_HIDDEN);

		lv_obj_add_event_cb(ble_ui_state.accept_btn, accept_btn_event_cb,
				    LV_EVENT_CLICKED, NULL);
		lv_obj_add_event_cb(ble_ui_state.reject_btn, reject_btn_event_cb,
				    LV_EVENT_CLICKED, NULL);

		if (ble_ui_btn_label(ble_ui_state.accept_btn)) {
			lv_label_set_text(ble_ui_btn_label(ble_ui_state.accept_btn), "Accept");
			lv_obj_set_style_bg_color(ble_ui_state.accept_btn, NS_COLOR_NORMAL, 0);
		}
		if (ble_ui_btn_label(ble_ui_state.reject_btn)) {
			lv_label_set_text(ble_ui_btn_label(ble_ui_state.reject_btn), "Reject");
			lv_obj_set_style_bg_color(ble_ui_state.reject_btn, NS_COLOR_CRITICAL, 0);
		}
		break;

	case BLE_UI_BTN_MODE_CONNECTED:
		lv_obj_clear_flag(ble_ui_state.btn_container, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(ble_ui_state.accept_btn, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(ble_ui_state.reject_btn, LV_OBJ_FLAG_HIDDEN);

		lv_obj_add_event_cb(ble_ui_state.accept_btn, disconnect_btn_event_cb,
				    LV_EVENT_CLICKED, NULL);
		lv_obj_add_event_cb(ble_ui_state.reject_btn, forget_btn_event_cb,
				    LV_EVENT_CLICKED, NULL);

		if (ble_ui_btn_label(ble_ui_state.accept_btn)) {
			lv_label_set_text(ble_ui_btn_label(ble_ui_state.accept_btn), "Disconnect");
			lv_obj_set_style_bg_color(ble_ui_state.accept_btn, NS_COLOR_RISK, 0);
		}
		if (ble_ui_btn_label(ble_ui_state.reject_btn)) {
			lv_label_set_text(ble_ui_btn_label(ble_ui_state.reject_btn), "Forget");
			lv_obj_set_style_bg_color(ble_ui_state.reject_btn, NS_COLOR_CARD, 0);
		}
		break;

	case BLE_UI_BTN_MODE_ADVERTISING:
	default:
		lv_obj_add_flag(ble_ui_state.btn_container, LV_OBJ_FLAG_HIDDEN);
		break;
	}

	ble_ui_state.last_btn_mode = mode;
}

static void ble_ui_async_update_cb(void *user_data)
{
	ARG_UNUSED(user_data);
	ble_ui_update_screen();
}

static void ble_ui_request_update(void)
{
	if (!ble_ui_state.initialized || !ble_ui_state.screen) {
		return;
	}

	/* Ensure LVGL object updates run in LVGL context, not BT callback thread. */
	lv_async_call(ble_ui_async_update_cb, NULL);
}

static void ble_ui_hide_passkey(void)
{
	ble_ui_state.displayed_passkey = 0U;

	if (ui_is_obj_valid(ble_ui_state.passkey_container, UI_SCREEN_BLE)) {
		lv_obj_add_flag(ble_ui_state.passkey_container, LV_OBJ_FLAG_HIDDEN);
	}
	if (ui_is_obj_valid(ble_ui_state.passkey_value, UI_SCREEN_BLE)) {
		lv_label_set_text(ble_ui_state.passkey_value, "------");
	}
	if (ui_is_obj_valid(ble_ui_state.passkey_hint, UI_SCREEN_BLE)) {
		lv_label_set_text(ble_ui_state.passkey_hint, "");
	}
}

/* =============================================================================
 * Button Callbacks
 * ============================================================================= */

static void accept_btn_event_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
		/* Debounce check */
		if (gesture_in_progress || 
		    (k_uptime_get_32() - last_gesture_time) < GESTURE_DEBOUNCE_MS) {
			return;
		}
		
		LOG_INF("Passkey accepted by user");
		int err = ble_gatt_confirm_passkey(true);

		if (ui_is_obj_valid(ble_ui_state.passkey_hint, UI_SCREEN_BLE)) {
			if (err == 0) {
				lv_label_set_text(ble_ui_state.passkey_hint,
						  "Sent — confirm in HCM Monitor");
			} else {
				lv_label_set_text(ble_ui_state.passkey_hint,
						  "Confirm failed — tap Pair on PC");
			}
		}
		if (err == 0) {
			ble_ui_state.last_pairing_state = BLE_PAIRING_BONDING;
			ble_ui_hide_passkey();
			if (ui_is_obj_valid(ble_ui_state.status_label, UI_SCREEN_BLE)) {
				lv_label_set_text(ble_ui_state.status_label, "Bonding...");
				lv_obj_set_style_text_color(ble_ui_state.status_label,
							    NS_COLOR_WARNING, 0);
			}
		}
	}
}

static void reject_btn_event_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
		/* Debounce check */
		if (gesture_in_progress || 
		    (k_uptime_get_32() - last_gesture_time) < GESTURE_DEBOUNCE_MS) {
			return;
		}
		
		LOG_INF("Passkey rejected by user");
		ble_gatt_confirm_passkey(false);
		
		/* Update UI */
		if (ui_is_obj_valid(ble_ui_state.passkey_hint, UI_SCREEN_BLE)) {
			lv_label_set_text(ble_ui_state.passkey_hint, "Pairing cancelled");
		}
	}
}

static void disconnect_btn_event_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
		if (gesture_in_progress || 
		    (k_uptime_get_32() - last_gesture_time) < GESTURE_DEBOUNCE_MS) {
			return;
		}
		
		LOG_INF("Disconnect requested by user");
		ble_gatt_disconnect();
	}
}

static void forget_btn_event_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
		if (gesture_in_progress ||
		    (k_uptime_get_32() - last_gesture_time) < GESTURE_DEBOUNCE_MS) {
			return;
		}

		LOG_INF("Forget all bonds requested by user");
		ble_gatt_delete_bonds();

		if (ble_ui_state.bond_count_label) {
			lv_label_set_text(ble_ui_state.bond_count_label, "Bonds: 0");
		}
		ble_ui_request_update();
	}
}

/* =============================================================================
 * Pairing State Callback
 * ============================================================================= */

static void ble_pairing_state_handler(enum ble_pairing_state state, uint32_t passkey)
{
	ble_ui_state.last_pairing_state = state;
	if (passkey > 0U) {
		ble_ui_state.displayed_passkey = passkey;
	} else if (state == BLE_PAIRING_COMPLETE ||
		   state == BLE_PAIRING_FAILED ||
		   state == BLE_PAIRING_IDLE) {
		ble_ui_state.displayed_passkey = 0U;
	}

	LOG_INF("BLE pairing state changed: %d, passkey: %06u", state,
		(unsigned)ble_ui_state.displayed_passkey);
	ble_ui_request_update();
}

/* Public entry point: the always-active global pairing callback in ui.c
 * forwards events here so the BLE screen stays in sync even when it was not
 * the screen that registered the callback. */
void ble_ui_handle_pairing_event(enum ble_pairing_state state, uint32_t passkey)
{
	ble_pairing_state_handler(state, passkey);
}

/* =============================================================================
 * UI Creation
 * ============================================================================= */

void ble_ui_create_screen(lv_obj_t *screen)
{
	if (!screen) {
		LOG_ERR("NULL screen passed to ble_ui_create_screen");
		return;
	}
	
	ble_ui_state.screen = screen;
	ble_ui_state.initialized = false;
	/* Preserve in-flight pairing: ui_global_pairing_cb may fire before this
	 * screen is created; resetting to IDLE hid the passkey on the LCD. */
	ble_ui_state.last_pairing_state = ble_gatt_get_pairing_state();
	ble_ui_state.displayed_passkey = ble_gatt_get_pairing_passkey();
	ble_ui_state.connected = ble_gatt_is_connected();
	ble_ui_state.last_btn_mode = BLE_UI_BTN_MODE_NONE;
	
	ns_apply_screen_bg(screen);

	/* Create title */
	ble_ui_state.title_label = lv_label_create(screen);
	lv_label_set_text(ble_ui_state.title_label, "BLE");
	lv_obj_set_style_text_font(ble_ui_state.title_label, UI_FONT_20, 0);
	lv_obj_set_style_text_color(ble_ui_state.title_label, NS_COLOR_SECONDARY, 0);
	lv_obj_align(ble_ui_state.title_label, LV_ALIGN_TOP_MID, 0, UI_SHELL_TOP_INSET + 10);
	
	/* Create status label */
	ble_ui_state.status_label = lv_label_create(screen);
	lv_label_set_text(ble_ui_state.status_label, "Advertising...");
	lv_obj_set_style_text_font(ble_ui_state.status_label, UI_FONT_14, 0);
	lv_obj_align(ble_ui_state.status_label, LV_ALIGN_TOP_MID, 0, UI_SHELL_TOP_INSET + 40);
	
	/* Create passkey container (initially hidden) */
	ble_ui_state.passkey_container = lv_obj_create(screen);
	lv_obj_set_size(ble_ui_state.passkey_container, 200, 100);
	lv_obj_align(ble_ui_state.passkey_container, LV_ALIGN_CENTER, 0, -10);
	lv_obj_set_style_bg_color(ble_ui_state.passkey_container, NS_COLOR_PRIMARY, 0);
	lv_obj_set_style_bg_opa(ble_ui_state.passkey_container, LV_OPA_100, 0);
	lv_obj_set_style_radius(ble_ui_state.passkey_container, 10, 0);
	lv_obj_set_style_border_width(ble_ui_state.passkey_container, 2, 0);
	lv_obj_set_style_border_color(ble_ui_state.passkey_container, NS_COLOR_SECONDARY, 0);
	lv_obj_add_flag(ble_ui_state.passkey_container, LV_OBJ_FLAG_HIDDEN);
	
	/* Passkey title */
	ble_ui_state.passkey_label = lv_label_create(ble_ui_state.passkey_container);
	lv_label_set_text(ble_ui_state.passkey_label, "Pairing Code:");
	lv_obj_set_style_text_color(ble_ui_state.passkey_label, NS_COLOR_TEXT, 0);
	lv_obj_set_style_text_font(ble_ui_state.passkey_label, UI_FONT_14, 0);
	lv_obj_align(ble_ui_state.passkey_label, LV_ALIGN_TOP_MID, 0, 5);
	
	/* Passkey value (large digits) */
	ble_ui_state.passkey_value = lv_label_create(ble_ui_state.passkey_container);
	lv_label_set_text(ble_ui_state.passkey_value, "------");
	lv_obj_set_style_text_color(ble_ui_state.passkey_value, NS_COLOR_TEXT, 0);
	lv_obj_set_style_text_font(ble_ui_state.passkey_value, UI_FONT_28, 0);
	lv_obj_align(ble_ui_state.passkey_value, LV_ALIGN_CENTER, 0, 0);
	
	/* Passkey hint */
	ble_ui_state.passkey_hint = lv_label_create(ble_ui_state.passkey_container);
	lv_label_set_text(ble_ui_state.passkey_hint, "Tap Accept if it matches");
	lv_obj_set_style_text_color(ble_ui_state.passkey_hint, NS_COLOR_TEXT_DIM, 0);
	lv_obj_set_style_text_font(ble_ui_state.passkey_hint, UI_FONT_14, 0);
	lv_obj_align(ble_ui_state.passkey_hint, LV_ALIGN_BOTTOM_MID, 0, -5);
	
	/* Create button container for pairing confirmation */
	ble_ui_state.btn_container = lv_obj_create(screen);
	lv_obj_set_size(ble_ui_state.btn_container, 220, 50);
	lv_obj_align(ble_ui_state.btn_container, LV_ALIGN_BOTTOM_MID, 0, -60);
	lv_obj_set_style_bg_opa(ble_ui_state.btn_container, LV_OPA_0, 0);
	lv_obj_set_style_border_width(ble_ui_state.btn_container, 0, 0);
	lv_obj_set_style_pad_all(ble_ui_state.btn_container, 0, 0);
	lv_obj_add_flag(ble_ui_state.btn_container, LV_OBJ_FLAG_HIDDEN);
	lv_obj_set_flex_flow(ble_ui_state.btn_container, LV_FLEX_FLOW_ROW);
	lv_obj_set_flex_align(ble_ui_state.btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, 
	                       LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
	
	/* Accept button */
	ble_ui_state.accept_btn = lv_btn_create(ble_ui_state.btn_container);
	lv_obj_set_size(ble_ui_state.accept_btn, 90, 40);
	lv_obj_set_style_bg_color(ble_ui_state.accept_btn, NS_COLOR_NORMAL, 0);
	lv_obj_add_event_cb(ble_ui_state.accept_btn, accept_btn_event_cb, LV_EVENT_CLICKED, NULL);
	
	lv_obj_t *accept_label = lv_label_create(ble_ui_state.accept_btn);
	lv_label_set_text(accept_label, "Accept");
	lv_obj_center(accept_label);
	
	/* Reject button */
	ble_ui_state.reject_btn = lv_btn_create(ble_ui_state.btn_container);
	lv_obj_set_size(ble_ui_state.reject_btn, 90, 40);
	lv_obj_set_style_bg_color(ble_ui_state.reject_btn, NS_COLOR_CRITICAL, 0);
	lv_obj_add_event_cb(ble_ui_state.reject_btn, reject_btn_event_cb, LV_EVENT_CLICKED, NULL);
	
	lv_obj_t *reject_label = lv_label_create(ble_ui_state.reject_btn);
	lv_label_set_text(reject_label, "Reject");
	lv_obj_center(reject_label);
	
	/* Create device info container */
	ble_ui_state.info_container = lv_obj_create(screen);
	lv_obj_set_size(ble_ui_state.info_container, 220, 60);
	lv_obj_align(ble_ui_state.info_container, LV_ALIGN_BOTTOM_MID, 0, -10);
	lv_obj_set_style_bg_color(ble_ui_state.info_container, NS_COLOR_CARD, 0);
	lv_obj_set_style_bg_opa(ble_ui_state.info_container, LV_OPA_100, 0);
	lv_obj_set_style_radius(ble_ui_state.info_container, 8, 0);
	lv_obj_set_style_border_width(ble_ui_state.info_container, 0, 0);
	lv_obj_set_style_pad_all(ble_ui_state.info_container, 5, 0);
	
	/* Device name label */
	ble_ui_state.device_name_label = lv_label_create(ble_ui_state.info_container);
	lv_label_set_text(ble_ui_state.device_name_label, "Device: NiSense");
	lv_obj_set_style_text_color(ble_ui_state.device_name_label, NS_COLOR_TEXT_DIM, 0);
	lv_obj_set_style_text_font(ble_ui_state.device_name_label, UI_FONT_14, 0);
	lv_obj_align(ble_ui_state.device_name_label, LV_ALIGN_TOP_LEFT, 5, 5);
	
	/* Connected device address */
	ble_ui_state.device_addr_label = lv_label_create(ble_ui_state.info_container);
	lv_label_set_text(ble_ui_state.device_addr_label, "Peer: --");
	lv_obj_set_style_text_color(ble_ui_state.device_addr_label, NS_COLOR_TEXT_DIM, 0);
	lv_obj_set_style_text_font(ble_ui_state.device_addr_label, UI_FONT_14, 0);
	lv_obj_align(ble_ui_state.device_addr_label, LV_ALIGN_BOTTOM_LEFT, 5, -5);

	/* Bond count label */
	ble_ui_state.bond_count_label = lv_label_create(screen);
	lv_obj_set_style_text_color(ble_ui_state.bond_count_label, NS_COLOR_TEXT_DIM, 0);
	lv_obj_set_style_text_font(ble_ui_state.bond_count_label, UI_FONT_14, 0);
	lv_obj_align(ble_ui_state.bond_count_label, LV_ALIGN_TOP_RIGHT, -8, UI_SHELL_TOP_INSET + 10);

	/* Forget all bonds button */
	ble_ui_state.forget_btn = lv_btn_create(screen);
	lv_obj_set_size(ble_ui_state.forget_btn, 130, 36);
	lv_obj_align(ble_ui_state.forget_btn, LV_ALIGN_TOP_RIGHT, -8, UI_SHELL_TOP_INSET + 30);
	lv_obj_set_style_bg_color(ble_ui_state.forget_btn, NS_COLOR_CARD, 0);
	lv_obj_add_event_cb(ble_ui_state.forget_btn, forget_btn_event_cb, LV_EVENT_CLICKED, NULL);

	lv_obj_t *forget_label = lv_label_create(ble_ui_state.forget_btn);
	lv_label_set_text(forget_label, "Forget");
	lv_obj_center(forget_label);

	ble_ui_state.initialized = true;
	
	/* Initial update */
	ble_ui_request_update();
	
	LOG_INF("BLE UI screen created");
}

/* =============================================================================
 * UI Update
 * ============================================================================= */

void ble_ui_update_screen(void)
{
	if (!ble_ui_state.initialized || !ble_ui_state.screen) {
		return;
	}
	
	bool connected = ble_gatt_is_connected();
	enum ble_pairing_state pairing_state = ble_ui_state.last_pairing_state;
	bool pairing_active = (pairing_state == BLE_PAIRING_CONFIRM_PASSKEY ||
			       pairing_state == BLE_PAIRING_WAITING_PASSKEY ||
			       pairing_state == BLE_PAIRING_BONDING);
	ble_ui_state.connected = connected;

	/* Pairing UI first — while connected, the block below would otherwise
	 * repurpose Accept as Disconnect and hide the passkey. */
	switch (pairing_state) {
	case BLE_PAIRING_CONFIRM_PASSKEY:
	case BLE_PAIRING_WAITING_PASSKEY:
		/* Show passkey */
		if (ui_is_obj_valid(ble_ui_state.passkey_container, UI_SCREEN_BLE)) {
			lv_obj_clear_flag(ble_ui_state.passkey_container, LV_OBJ_FLAG_HIDDEN);
		}
		
		char passkey_str[8];

		snprintf(passkey_str, sizeof(passkey_str), "%06u",
			 (unsigned)(ble_ui_state.displayed_passkey % 1000000U));
		if (ui_is_obj_valid(ble_ui_state.passkey_value, UI_SCREEN_BLE)) {
			lv_label_set_text(ble_ui_state.passkey_value, passkey_str);
		}

		if (ui_is_obj_valid(ble_ui_state.passkey_hint, UI_SCREEN_BLE)) {
			lv_label_set_text(ble_ui_state.passkey_hint,
					  "Confirm in HCM Monitor on PC");
		}

		if (ui_is_obj_valid(ble_ui_state.status_label, UI_SCREEN_BLE)) {
			lv_label_set_text(ble_ui_state.status_label, "Pairing request...");
			lv_obj_set_style_text_color(ble_ui_state.status_label, NS_COLOR_WARNING, 0);
		}
		break;
		
	case BLE_PAIRING_BONDING:
		ble_ui_hide_passkey();
		if (ui_is_obj_valid(ble_ui_state.status_label, UI_SCREEN_BLE)) {
			lv_label_set_text(ble_ui_state.status_label, "Bonding...");
			lv_obj_set_style_text_color(ble_ui_state.status_label, NS_COLOR_WARNING, 0);
		}
		if (ui_is_obj_valid(ble_ui_state.btn_container, UI_SCREEN_BLE)) {
			lv_obj_add_flag(ble_ui_state.btn_container, LV_OBJ_FLAG_HIDDEN);
		}
		break;
		
	case BLE_PAIRING_COMPLETE:
		ble_ui_hide_passkey();
		break;
		
	case BLE_PAIRING_FAILED:
		ble_ui_hide_passkey();
		if (ui_is_obj_valid(ble_ui_state.status_label, UI_SCREEN_BLE)) {
			lv_label_set_text(ble_ui_state.status_label, "Pairing failed");
			lv_obj_set_style_text_color(ble_ui_state.status_label, NS_COLOR_CRITICAL, 0);
		}
		break;
		
	case BLE_PAIRING_IDLE:
	default:
		ble_ui_hide_passkey();
		break;
	}

	/* Wire action buttons once per mode — ui_update() polls every 500 ms. */
	if (pairing_state == BLE_PAIRING_CONFIRM_PASSKEY ||
	    pairing_state == BLE_PAIRING_WAITING_PASSKEY) {
		ble_ui_apply_button_mode(BLE_UI_BTN_MODE_PAIRING);
	} else if (pairing_state == BLE_PAIRING_BONDING) {
		/* Accept/Reject hidden in BONDING case above */
	} else if (pairing_state == BLE_PAIRING_FAILED) {
		ble_ui_apply_button_mode(BLE_UI_BTN_MODE_ADVERTISING);
	} else if (connected) {
		ble_ui_apply_button_mode(BLE_UI_BTN_MODE_CONNECTED);
	} else {
		ble_ui_apply_button_mode(BLE_UI_BTN_MODE_ADVERTISING);
	}

	/* Connection status text */
	if (connected && !pairing_active) {
		if (pairing_state == BLE_PAIRING_FAILED) {
			if (ui_is_obj_valid(ble_ui_state.status_label, UI_SCREEN_BLE)) {
				lv_label_set_text(ble_ui_state.status_label, "Pairing failed");
				lv_obj_set_style_text_color(ble_ui_state.status_label, NS_COLOR_CRITICAL, 0);
			}
		} else if (pairing_state == BLE_PAIRING_COMPLETE) {
			if (ui_is_obj_valid(ble_ui_state.status_label, UI_SCREEN_BLE)) {
				lv_label_set_text(ble_ui_state.status_label, "Paired & connected");
				lv_obj_set_style_text_color(ble_ui_state.status_label, NS_COLOR_NORMAL, 0);
			}
		} else if (ui_is_obj_valid(ble_ui_state.status_label, UI_SCREEN_BLE)) {
			lv_label_set_text(ble_ui_state.status_label, "Connected");
			lv_obj_set_style_text_color(ble_ui_state.status_label, NS_COLOR_NORMAL, 0);
		}
	} else if (!connected && !pairing_active &&
		   pairing_state != BLE_PAIRING_FAILED &&
		   ui_is_obj_valid(ble_ui_state.status_label, UI_SCREEN_BLE)) {
		lv_label_set_text(ble_ui_state.status_label, "Advertising...");
		lv_obj_set_style_text_color(ble_ui_state.status_label, NS_COLOR_SECONDARY, 0);
	}

	/* Update device info */
	if (ui_is_obj_valid(ble_ui_state.device_name_label, UI_SCREEN_BLE)) {
		lv_label_set_text(ble_ui_state.device_name_label, "Device: " CONFIG_BT_DEVICE_NAME);
	}

	/* Update bond count */
	if (ui_is_obj_valid(ble_ui_state.bond_count_label, UI_SCREEN_BLE)) {
		int bonds = ble_gatt_get_bond_count();
		char bond_str[24];
		snprintf(bond_str, sizeof(bond_str), "Bonds: %d", bonds);
		lv_label_set_text(ble_ui_state.bond_count_label, bond_str);
		if (ui_is_obj_valid(ble_ui_state.forget_btn, UI_SCREEN_BLE)) {
			if (bonds > 0 && !connected) {
				lv_obj_clear_flag(ble_ui_state.forget_btn, LV_OBJ_FLAG_HIDDEN);
			} else {
				lv_obj_add_flag(ble_ui_state.forget_btn, LV_OBJ_FLAG_HIDDEN);
			}
		}
	}

	if (ui_is_obj_valid(ble_ui_state.device_addr_label, UI_SCREEN_BLE)) {
		if (connected) {
			void *conn = ble_gatt_get_connection();
			char addr_str[BT_ADDR_LE_STR_LEN] = "Unknown";
			
			if (conn) {
				struct bt_conn_info info;
				if (bt_conn_get_info((struct bt_conn *)conn, &info) == 0) {
					bt_addr_le_to_str(info.le.dst, addr_str, sizeof(addr_str));
				}
			}
			
			char peer_text[64];
			snprintf(peer_text, sizeof(peer_text), "Peer: %s", addr_str);
			lv_label_set_text(ble_ui_state.device_addr_label, peer_text);
		} else {
			lv_label_set_text(ble_ui_state.device_addr_label, "Peer: --");
		}
	}
}

/* =============================================================================
 * Cleanup
 * ============================================================================= */

void ble_ui_cleanup_screen(void)
{
	/* Stop deferred LVGL updates before the widget tree is destroyed. */
	ble_ui_state.initialized = false;
	ble_ui_state.screen = NULL;
	(void)lv_async_call_cancel(ble_ui_async_update_cb, NULL);

	memset(&ble_ui_state, 0, sizeof(ble_ui_state));
	
	LOG_DBG("BLE UI screen cleaned up");
}
