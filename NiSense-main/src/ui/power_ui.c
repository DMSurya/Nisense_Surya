/* power_ui.c – Battery and PMIC LVGL screens
 *
 * - UI_SCREEN_POWER: fuel gauge, pack detection, VBAT vs VCELL
 * - UI_SCREEN_CHARGER: branded charging screen (logo + SOC)
 * - UI_SCREEN_REGULATOR: rail enable/voltage control
 */

#include "power.h"
#include "power_batt.h"
#include "power_mgr.h"
#include "ui_common.h"
#include "ui_theme.h"
#include "ui_shell.h"
#include "buzzer.h"
#include <zephyr/drivers/regulator.h>
#include <zephyr/drivers/charger.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <lvgl.h>
#include "resource_fonts.h"
#include "resource_logo.h"
#include "resource_icons.h"
#include "resource.h"
#include <stdio.h>
#include <string.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(power_ui, CONFIG_POWER_UI_LOG_LEVEL);

#if defined(CONFIG_UI_SCREEN_POWER)
static void power_ui_format_pmic_vbat(char *text_buf, size_t text_buf_sz)
{
	int32_t uv;

	if (power_batt_voltage_uv(&uv) != 0) {
		snprintf(text_buf, text_buf_sz, "VBAT: N/A");
		return;
	}

	int32_t mv = uv / 1000;

	snprintf(text_buf, text_buf_sz, "VBAT: %ld.%03ldV (PMIC ADC)",
		 (long)(mv / 1000), (long)(mv % 1000));
}

static void power_ui_format_fg_vcell(char *text_buf, size_t text_buf_sz)
{
	int32_t uv;

	if (power_batt_fg_vcell_uv(&uv) != 0) {
		snprintf(text_buf, text_buf_sz, "VCELL: N/A");
		return;
	}

	int32_t mv = uv / 1000;

	snprintf(text_buf, text_buf_sz, "VCELL: %ld.%03ldV (gauge)",
		 (long)(mv / 1000), (long)(mv % 1000));
}

static void power_ui_format_pack_line(char *text_buf, size_t text_buf_sz)
{
	struct power_batt_snapshot snap;

	if (power_batt_snapshot(&snap) != 0) {
		snprintf(text_buf, text_buf_sz, "Pack: N/A");
		return;
	}

	snprintf(text_buf, text_buf_sz, "Pack: %s",
		 power_batt_cell_state_str(snap.cell_state));
}

static void power_ui_format_soc_line(char *text_buf, size_t text_buf_sz, int soc)
{
	struct power_batt_snapshot snap;

	if (power_batt_snapshot(&snap) == 0 &&
	    snap.cell_state == POWER_BATT_CELL_ABSENT) {
		snprintf(text_buf, text_buf_sz, "SOC: -- (no pack)");
	} else {
		snprintf(text_buf, text_buf_sz, "SOC: %d%%", soc);
	}
}

static void power_ui_fill_battery_fallback(lv_obj_t **labels, int max_labels,
					   enum ui_screen screen, int *label_idx,
					   char *text_buf, size_t text_buf_sz)
{
	int soc;

	if (*label_idx < max_labels &&
	    ui_is_obj_valid(labels[*label_idx], screen)) {
		power_ui_format_pack_line(text_buf, text_buf_sz);
		lv_label_set_text(labels[*label_idx], text_buf);
	}
	(*label_idx)++;

	if (*label_idx < max_labels &&
	    ui_is_obj_valid(labels[*label_idx], screen)) {
		power_ui_format_pmic_vbat(text_buf, text_buf_sz);
		lv_label_set_text(labels[*label_idx], text_buf);
	}
	(*label_idx)++;

	if (*label_idx < max_labels &&
	    ui_is_obj_valid(labels[*label_idx], screen)) {
		power_ui_format_fg_vcell(text_buf, text_buf_sz);
		lv_label_set_text(labels[*label_idx], text_buf);
	}
	(*label_idx)++;

	if (*label_idx < max_labels &&
	    ui_is_obj_valid(labels[*label_idx], screen)) {
		lv_label_set_text(labels[*label_idx], "Current: --");
	}
	(*label_idx)++;

	if (*label_idx < max_labels &&
	    ui_is_obj_valid(labels[*label_idx], screen)) {
		if (power_batt_soc_percent(&soc) == 0) {
			power_ui_format_soc_line(text_buf, text_buf_sz, soc);
		} else {
			snprintf(text_buf, text_buf_sz, "SOC: N/A");
		}
		lv_label_set_text(labels[*label_idx], text_buf);
	}
	(*label_idx)++;

	for (int i = 5; i < 12 && *label_idx < max_labels; i++) {
		if (ui_is_obj_valid(labels[*label_idx], screen)) {
			lv_label_set_text(labels[*label_idx], "--");
		}
		(*label_idx)++;
	}
}
#endif /* CONFIG_UI_SCREEN_POWER */

/* ---------- Screen 1: Regulator Control ---------- */
#if defined(CONFIG_UI_SCREEN_REGULATOR)
static lv_obj_t *regulator_items[4];
static const char *regulator_names[] = {"BK1 (1.2V)", "BK2 (1.8V)", "BK3 (3.3V)", "BBOUT (5V)"};
static const struct device *regulator_devs[4];

static void regulator_toggle_cb(lv_event_t *e);
static void update_regulator_screen(void);

static void regulator_toggle_cb(lv_event_t *e)
{
	/* Validate event object */
	if (e == NULL) {
		LOG_ERR("regulator_toggle_cb: NULL event");
		return;
	}
	
	/* Ignore clicks if a gesture was recently detected */
	uint32_t now = k_uptime_get_32();
	if (gesture_in_progress || (now - last_gesture_time < GESTURE_DEBOUNCE_MS)) {
		gesture_in_progress = false;  /* Clear flag after debounce period */
		return;
	}
	
	/* Check if screen still exists */
	if (ui_get_screen(UI_SCREEN_REGULATOR) == NULL) {
		return;
	}
	
	int idx = (int)lv_event_get_user_data(e);
	if (idx < 0 || idx >= 6 || regulator_items[idx] == NULL) {
		return;
	}
	
	if (regulator_devs[idx] != NULL && device_is_ready(regulator_devs[idx])) {
		bool enabled = regulator_is_enabled(regulator_devs[idx]);
		if (enabled) {
			regulator_disable(regulator_devs[idx]);
		} else {
			regulator_enable(regulator_devs[idx]);
		}
		k_msleep(100);
		update_regulator_screen();
	}
}

static void update_regulator_screen(void)
{
	/* Check if screen exists */
	if (ui_get_screen(UI_SCREEN_REGULATOR) == NULL) {
		return;
	}
	
	for (int i = 0; i < 4; i++) {
		if (regulator_items[i] == NULL) {
			continue;
		}
		if (regulator_devs[i] != NULL && device_is_ready(regulator_devs[i])) {
			bool enabled = regulator_is_enabled(regulator_devs[i]);
			lv_color_t color = enabled ? NS_COLOR_NORMAL : NS_COLOR_CRITICAL;
			/* Check object is still valid before modifying */
			if (ui_is_obj_valid(regulator_items[i], UI_SCREEN_REGULATOR)) {
				lv_obj_set_style_bg_color(regulator_items[i], color, LV_PART_MAIN);
				
				char buf[32];
				/* Validate array index and names array access */
				if (i >= 0 && i < 4) {
					snprintf(buf, sizeof(buf), "%s: %s", regulator_names[i], enabled ? "ON" : "OFF");
					lv_obj_t *child = lv_obj_get_child(regulator_items[i], LV_PART_MAIN);
					if (child != NULL && ui_is_obj_valid(child, UI_SCREEN_REGULATOR)) {
						lv_label_set_text(child, buf);
					}
				}
			}
		}
	}
}

void power_ui_create_regulator_screen(lv_obj_t *screen)
{
	if (screen == NULL) {
		LOG_ERR("Failed to create regulator screen - screen is NULL");
		return;
	}
	
	ns_apply_screen_bg(screen);

	/* Title */
	lv_obj_t *title = ns_make_screen_title(screen, "Voltage Regulators");
	if (title == NULL) {
		LOG_ERR("Failed to create regulator title label - out of memory");
		ui_delete_screen(UI_SCREEN_REGULATOR);
		return;
	}
	
	/* Get regulator devices */
	regulator_devs[0] = DEVICE_DT_GET_OR_NULL(DT_ALIAS(bk1));
	regulator_devs[1] = DEVICE_DT_GET_OR_NULL(DT_ALIAS(bk2));
	regulator_devs[2] = DEVICE_DT_GET_OR_NULL(DT_ALIAS(bk3));
	regulator_devs[3] = DEVICE_DT_GET_OR_NULL(DT_ALIAS(bbout));
	
	/* Create regulator buttons */
	for (int i = 0; i < 4; i++) {
		regulator_items[i] = lv_btn_create(screen);
		if (regulator_items[i] == NULL) {
			LOG_ERR("Failed to create regulator button %d - out of memory", i);
			/* Clear all regulator_items to prevent dangling pointers */
			for (int j = 0; j < 4; j++) {
				regulator_items[j] = NULL;
			}
			ui_delete_screen(UI_SCREEN_REGULATOR);
			return;
		}
		if (regulator_items[i] != NULL) {
			lv_obj_set_size(regulator_items[i], 200, 35);
			/* Below status strip + title (was y=50, overlapped title). */
			lv_obj_align(regulator_items[i], LV_ALIGN_TOP_MID, 0,
				     UI_SHELL_TOP_INSET + 40 + i * 42);
			lv_obj_add_event_cb(regulator_items[i], regulator_toggle_cb, LV_EVENT_CLICKED, (void *)i);
		}
		
		if (regulator_items[i] != NULL) {
			lv_obj_t *label = lv_label_create(regulator_items[i]);
			if (label == NULL) {
				LOG_ERR("Failed to create regulator button %d label - out of memory", i);
				/* Clear all regulator_items to prevent dangling pointers */
				for (int j = 0; j < 4; j++) {
					regulator_items[j] = NULL;
				}
				ui_delete_screen(UI_SCREEN_REGULATOR);
				return;
			}
			/* Validate array index before accessing regulator_names */
			if (i >= 0 && i < 4) {
				lv_label_set_text(label, regulator_names[i]);
			} else {
				lv_label_set_text(label, "Unknown");
			}
		}
	}
	
	update_regulator_screen();
}

void power_ui_update_regulator_screen(void)
{
	update_regulator_screen();
}

void power_ui_cleanup_regulator_screen(void)
{
	for (int i = 0; i < 4; i++) {
		regulator_items[i] = NULL;
	}
}
#endif /* CONFIG_UI_SCREEN_REGULATOR */

/* ---------- PMIC & charger screen (Status4, USB, charger setpoints) ---------- */
#if defined(CONFIG_UI_SCREEN_CHARGER)
/* Branded charge screen: logo + status + SOC. Auto-nav on USB plug/unplug. */
#define CHARGE_LOGO_DISPLAY_PX 120
#define CHARGE_NAV_DEFER_MS    1500U

static lv_obj_t *s_chg_logo;
static lv_obj_t *s_chg_status;
static lv_obj_t *s_chg_soc;
static lv_obj_t *s_chg_bar;
static char s_chg_buf[32];

static bool s_chg_nav_active;
static enum ui_screen s_chg_prev_screen;
static bool s_chg_was_usb;
static bool s_chg_showing_fault;

/** True when UI should treat the pack as missing / faulted (not charging). */
static bool power_ui_pack_fault(const struct power_batt_snapshot *snap)
{
	int32_t i_abs;

	if (snap == NULL) {
		return false;
	}
	if (snap->cell_state == POWER_BATT_CELL_ABSENT) {
		return true;
	}
	/* Match power_batt_soc_from_snapshot USB-bench heuristic. */
	if (snap->usb_online && snap->cell_state == POWER_BATT_CELL_UNKNOWN) {
		i_abs = snap->current_ua >= 0 ? snap->current_ua : -snap->current_ua;
		if (i_abs < 30000 && snap->fg_pack_absent) {
			return true;
		}
	}
	return false;
}

static const char *charger_status_text(const struct power_batt_snapshot *snap)
{
	union charger_propval chg_val;
	const struct device *chg = power_batt_charger();

	if (power_ui_pack_fault(snap)) {
		return "Battery fault";
	}
	if (!power_batt_charger_ready() || chg == NULL) {
		return "USB connected";
	}
	if (charger_get_prop(chg, CHARGER_PROP_STATUS, &chg_val) != 0) {
		return "USB connected";
	}
	switch (chg_val.status) {
	case CHARGER_STATUS_CHARGING:
		return "Charging";
	case CHARGER_STATUS_FULL:
		return "Fully charged";
	case CHARGER_STATUS_NOT_CHARGING:
		return "USB connected";
	default:
		return "USB connected";
	}
}

static void charger_apply_logo(bool fault)
{
	const lv_image_dsc_t *fault_icon;

	if (s_chg_logo == NULL || fault == s_chg_showing_fault) {
		return;
	}

	if (fault) {
		fault_icon = xip_icon(RESOURCE_ID_ICON_BATTERY_CRITICAL);
		if (fault_icon == NULL) {
			fault_icon = xip_icon(RESOURCE_ID_ICON_WARNING);
		}
		if (fault_icon != NULL) {
			lv_image_set_src(s_chg_logo, fault_icon);
			resource_icon_apply_display_size(s_chg_logo, 64);
		}
		s_chg_showing_fault = true;
		return;
	}

	if (resource_logo_charge_ready()) {
		lv_image_set_src(s_chg_logo, &logo_charge);
		lv_obj_set_size(s_chg_logo, CHARGE_LOGO_DISPLAY_PX, CHARGE_LOGO_DISPLAY_PX);
		lv_image_set_scale(s_chg_logo,
				   (int32_t)((CHARGE_LOGO_DISPLAY_PX * 256U) / 240U));
	}
	s_chg_showing_fault = false;
}

static void update_charger_screen(void)
{
	struct power_batt_snapshot snap;
	int soc = -1;
	lv_color_t status_color = NS_COLOR_INFO;
	bool fault;

	if (ui_get_screen(UI_SCREEN_CHARGER) == NULL) {
		return;
	}

	(void)power_mgr_get_snapshot(&snap);
	(void)power_mgr_get_soc(&soc);
	fault = power_ui_pack_fault(&snap);

	if (ui_is_obj_valid(s_chg_status, UI_SCREEN_CHARGER)) {
		const char *text;

		if (!snap.usb_online) {
			text = "Unplugged";
			status_color = NS_COLOR_TEXT_DIM;
		} else if (fault) {
			text = charger_status_text(&snap);
			status_color = NS_COLOR_CRITICAL;
		} else {
			text = charger_status_text(&snap);
			if (text[0] == 'F') {
				status_color = NS_COLOR_NORMAL;
			} else if (text[0] == 'C') {
				status_color = NS_COLOR_HEALTH;
			} else {
				status_color = NS_COLOR_INFO;
			}
		}
		lv_label_set_text(s_chg_status, text);
		lv_obj_set_style_text_color(s_chg_status, status_color, LV_PART_MAIN);
	}

	charger_apply_logo(fault && snap.usb_online);

	if (ui_is_obj_valid(s_chg_soc, UI_SCREEN_CHARGER)) {
		if (fault || snap.cell_state == POWER_BATT_CELL_ABSENT) {
			snprintf(s_chg_buf, sizeof(s_chg_buf), "--%%");
			lv_obj_set_style_text_color(s_chg_soc, NS_COLOR_CRITICAL, LV_PART_MAIN);
		} else if (soc >= 0) {
			snprintf(s_chg_buf, sizeof(s_chg_buf), "%d%%", soc);
			lv_obj_set_style_text_color(s_chg_soc, NS_COLOR_TEXT, LV_PART_MAIN);
		} else {
			snprintf(s_chg_buf, sizeof(s_chg_buf), "--%%");
			lv_obj_set_style_text_color(s_chg_soc, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		}
		lv_label_set_text(s_chg_soc, s_chg_buf);
	}

	if (ui_is_obj_valid(s_chg_bar, UI_SCREEN_CHARGER)) {
		int32_t pct = (fault || soc < 0) ? 0 : soc;

		lv_bar_set_value(s_chg_bar, pct, LV_ANIM_OFF);
		lv_obj_set_style_bg_color(s_chg_bar,
					 fault ? NS_COLOR_CRITICAL :
					 ((pct >= 100) ? NS_COLOR_NORMAL : NS_COLOR_HEALTH),
					 LV_PART_INDICATOR);
	}
}

void power_ui_create_charger_screen(lv_obj_t *screen)
{
	if (screen == NULL) {
		LOG_ERR("Failed to create charging screen — out of memory");
		return;
	}

	s_chg_logo = NULL;
	s_chg_status = NULL;
	s_chg_soc = NULL;
	s_chg_bar = NULL;
	s_chg_showing_fault = false;

	ns_apply_screen_bg(screen);

	/* Prefer charge logo widget; update path swaps to fault icon when needed. */
	if (resource_logo_charge_ready() ||
	    xip_icon(RESOURCE_ID_ICON_BATTERY_CRITICAL) != NULL ||
	    xip_icon(RESOURCE_ID_ICON_WARNING) != NULL) {
		s_chg_logo = lv_image_create(screen);
		if (s_chg_logo != NULL) {
			if (resource_logo_charge_ready()) {
				lv_image_set_src(s_chg_logo, &logo_charge);
				lv_obj_set_size(s_chg_logo, CHARGE_LOGO_DISPLAY_PX,
						CHARGE_LOGO_DISPLAY_PX);
				lv_image_set_scale(
					s_chg_logo,
					(int32_t)((CHARGE_LOGO_DISPLAY_PX * 256U) / 240U));
			}
			lv_obj_align(s_chg_logo, LV_ALIGN_TOP_MID, 0, UI_SHELL_TOP_INSET + 4);
		}
	}

	s_chg_status = lv_label_create(screen);
	if (s_chg_status == NULL) {
		ui_delete_screen(UI_SCREEN_CHARGER);
		return;
	}
	lv_label_set_text(s_chg_status, "USB...");
	lv_obj_set_style_text_font(s_chg_status, UI_FONT_20, LV_PART_MAIN);
	lv_obj_set_style_text_color(s_chg_status, NS_COLOR_INFO, LV_PART_MAIN);
	lv_obj_align(s_chg_status, LV_ALIGN_TOP_MID, 0,
		     UI_SHELL_TOP_INSET + (s_chg_logo ? (CHARGE_LOGO_DISPLAY_PX + 8) : 12));

	s_chg_soc = lv_label_create(screen);
	if (s_chg_soc == NULL) {
		ui_delete_screen(UI_SCREEN_CHARGER);
		return;
	}
	lv_label_set_text(s_chg_soc, "--%");
	lv_obj_set_style_text_font(s_chg_soc, UI_FONT_28, LV_PART_MAIN);
	lv_obj_set_style_text_color(s_chg_soc, NS_COLOR_TEXT, LV_PART_MAIN);
	lv_obj_align_to(s_chg_soc, s_chg_status, LV_ALIGN_OUT_BOTTOM_MID, 0, 6);

	s_chg_bar = lv_bar_create(screen);
	if (s_chg_bar == NULL) {
		ui_delete_screen(UI_SCREEN_CHARGER);
		return;
	}
	lv_obj_set_size(s_chg_bar, 180, 10);
	lv_bar_set_range(s_chg_bar, 0, 100);
	lv_bar_set_value(s_chg_bar, 0, LV_ANIM_OFF);
	lv_obj_set_style_bg_color(s_chg_bar, NS_COLOR_CARD, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(s_chg_bar, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_radius(s_chg_bar, 5, LV_PART_MAIN);
	lv_obj_set_style_bg_color(s_chg_bar, NS_COLOR_HEALTH, LV_PART_INDICATOR);
	lv_obj_set_style_radius(s_chg_bar, 5, LV_PART_INDICATOR);
	lv_obj_align_to(s_chg_bar, s_chg_soc, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

	update_charger_screen();
}

void power_ui_update_charger_screen(void)
{
	update_charger_screen();
}

void power_ui_cleanup_charger_screen(void)
{
	s_chg_logo = NULL;
	s_chg_status = NULL;
	s_chg_soc = NULL;
	s_chg_bar = NULL;
	s_chg_showing_fault = false;
}

void power_ui_charge_nav_poll(void)
{
	struct power_batt_snapshot snap;

	if (power_mgr_get_snapshot(&snap) != 0) {
		return;
	}

	if (snap.usb_online && !s_chg_was_usb) {
		enum ui_screen cur = ui_get_current_screen();
		uint32_t since = k_uptime_get_32() - last_gesture_time;

		if (!ui_is_navigation_blocked() &&
		    cur != UI_SCREEN_CHARGER &&
		    since >= CHARGE_NAV_DEFER_MS) {
			s_chg_prev_screen = cur;
			s_chg_nav_active = true;
			ui_request_screen(UI_SCREEN_CHARGER);
			LOG_INF("USB plugged: auto-nav to charging screen (from %d)", cur);
		}
	} else if (!snap.usb_online && s_chg_was_usb) {
		if (s_chg_nav_active &&
		    ui_get_current_screen() == UI_SCREEN_CHARGER) {
			ui_request_screen(s_chg_prev_screen);
			LOG_INF("USB unplugged: restore screen %d", s_chg_prev_screen);
		}
		s_chg_nav_active = false;
	}

	s_chg_was_usb = snap.usb_online;
}
#endif /* CONFIG_UI_SCREEN_CHARGER */

#if defined(CONFIG_UI_SCREEN_POWER)
/* ---------- Battery screen (fuel gauge + pack detection) ---------- */
#define MAX_POWER_LABELS 20
static lv_obj_t *power_labels[MAX_POWER_LABELS];
static const struct device *fg_dev;
static char buf[100]; /* Reusable buffer for string formatting */
static char s_power_text[MAX_POWER_LABELS][80];

#if defined(CONFIG_LV_FONT_MONTSERRAT_14)
#define POWER_FONT_STATIC  (&lv_font_montserrat_14)
#else
#define POWER_FONT_STATIC  UI_FONT_14
#endif

static void power_ui_label_set(int idx, const char *text, enum ui_screen screen)
{
	if (idx < 0 || idx >= MAX_POWER_LABELS || text == NULL ||
	    power_labels[idx] == NULL) {
		return;
	}
	if (!ui_is_obj_valid(power_labels[idx], screen)) {
		return;
	}
	if (strncmp(s_power_text[idx], text, sizeof(s_power_text[idx])) == 0) {
		return;
	}
	strncpy(s_power_text[idx], text, sizeof(s_power_text[idx]) - 1U);
	s_power_text[idx][sizeof(s_power_text[idx]) - 1U] = '\0';
	lv_label_set_text(power_labels[idx], text);
}

static void update_power_screen(void)
{
	struct power_batt_snapshot snap;
	int soc;

	if (ui_get_screen(UI_SCREEN_POWER) == NULL) {
		return;
	}

	/* Battery I2C is owned by power_mgr — UI reads cached snapshot only. */
	if (power_mgr_get_snapshot(&snap) != 0) {
		return;
	}

	int label_idx = 0;

	if (power_batt_fuel_gauge_ready()) {
		if (label_idx < MAX_POWER_LABELS &&
		    ui_is_obj_valid(power_labels[label_idx], UI_SCREEN_POWER)) {
			snprintf(buf, sizeof(buf), "Pack: %s",
				 power_batt_cell_state_str(snap.cell_state));
			power_ui_label_set(label_idx, buf, UI_SCREEN_POWER);
		}
		label_idx++;

		if (label_idx < MAX_POWER_LABELS &&
		    ui_is_obj_valid(power_labels[label_idx], UI_SCREEN_POWER)) {
			if (snap.pmic_vbat_uv > 0) {
				snprintf(buf, sizeof(buf), "VBAT: %ld mV",
					 (long)(snap.pmic_vbat_uv / 1000));
			} else {
				snprintf(buf, sizeof(buf), "VBAT: N/A");
			}
			power_ui_label_set(label_idx, buf, UI_SCREEN_POWER);
		}
		label_idx++;

		if (label_idx < MAX_POWER_LABELS &&
		    ui_is_obj_valid(power_labels[label_idx], UI_SCREEN_POWER)) {
			if (snap.fg_vcell_uv > 0) {
				snprintf(buf, sizeof(buf), "VCELL: %ld mV",
					 (long)(snap.fg_vcell_uv / 1000));
			} else {
				snprintf(buf, sizeof(buf), "VCELL: N/A");
			}
			power_ui_label_set(label_idx, buf, UI_SCREEN_POWER);
		}
		label_idx++;

		if (label_idx < MAX_POWER_LABELS &&
		    ui_is_obj_valid(power_labels[label_idx], UI_SCREEN_POWER)) {
			int32_t current_ua = snap.current_ua;
			int32_t current_ma = current_ua / 1000;
			int32_t current_ma_abs = (current_ma < 0) ? -current_ma : current_ma;
			int32_t current_ua_frac =
				((current_ua < 0) ? -current_ua : current_ua) % 1000;

			if (current_ua > 0) {
				snprintf(buf, sizeof(buf), "Current: +%ld.%03ldmA",
					 (long)current_ma_abs, (long)current_ua_frac);
			} else if (current_ua < 0) {
				snprintf(buf, sizeof(buf), "Current: -%ld.%03ldmA",
					 (long)current_ma_abs, (long)current_ua_frac);
			} else {
				snprintf(buf, sizeof(buf), "Current: 0.000mA");
			}
			power_ui_label_set(label_idx, buf, UI_SCREEN_POWER);
		}
		label_idx++;

		if (label_idx < MAX_POWER_LABELS &&
		    ui_is_obj_valid(power_labels[label_idx], UI_SCREEN_POWER)) {
			if (power_batt_soc_from_snapshot(&snap, &soc) == 0) {
				power_ui_format_soc_line(buf, sizeof(buf), soc);
				if (snap.cell_state == POWER_BATT_CELL_ABSENT) {
					lv_obj_set_style_text_color(power_labels[label_idx],
								    NS_COLOR_TEXT_DIM, LV_PART_MAIN);
				} else if (soc > 50) {
					lv_obj_set_style_text_color(power_labels[label_idx],
								    NS_COLOR_NORMAL, LV_PART_MAIN);
				} else if (soc > 20) {
					lv_obj_set_style_text_color(power_labels[label_idx],
								    NS_COLOR_WARNING, LV_PART_MAIN);
				} else {
					lv_obj_set_style_text_color(power_labels[label_idx],
								    NS_COLOR_CRITICAL, LV_PART_MAIN);
				}
			} else {
				snprintf(buf, sizeof(buf), "SOC: Error");
			}
			power_ui_label_set(label_idx, buf, UI_SCREEN_POWER);
		}
		label_idx++;

		if (snap.fg_ext.valid) {
			if (label_idx < MAX_POWER_LABELS &&
			    ui_is_obj_valid(power_labels[label_idx], UI_SCREEN_POWER)) {
				snprintf(buf, sizeof(buf), "Remaining: %u mAh",
					 snap.fg_ext.remaining_mah);
				power_ui_label_set(label_idx, buf, UI_SCREEN_POWER);
			}
			label_idx++;

			if (label_idx < MAX_POWER_LABELS &&
			    ui_is_obj_valid(power_labels[label_idx], UI_SCREEN_POWER)) {
				snprintf(buf, sizeof(buf), "Full Cap: %u mAh",
					 snap.fg_ext.full_mah);
				power_ui_label_set(label_idx, buf, UI_SCREEN_POWER);
			}
			label_idx++;

			if (label_idx < MAX_POWER_LABELS &&
			    ui_is_obj_valid(power_labels[label_idx], UI_SCREEN_POWER)) {
				snprintf(buf, sizeof(buf), "Design: %u mAh",
					 snap.fg_ext.design_mah);
				power_ui_label_set(label_idx, buf, UI_SCREEN_POWER);
			}
			label_idx++;

			if (label_idx < MAX_POWER_LABELS &&
			    ui_is_obj_valid(power_labels[label_idx], UI_SCREEN_POWER)) {
				snprintf(buf, sizeof(buf), "Temp: %d C", snap.fg_ext.temp_c);
				power_ui_label_set(label_idx, buf, UI_SCREEN_POWER);
			}
			label_idx++;

			if (label_idx < MAX_POWER_LABELS &&
			    ui_is_obj_valid(power_labels[label_idx], UI_SCREEN_POWER)) {
				snprintf(buf, sizeof(buf), "Cycles: %u",
					 snap.fg_ext.cycle_count);
				power_ui_label_set(label_idx, buf, UI_SCREEN_POWER);
			}
			label_idx++;

			if (label_idx < MAX_POWER_LABELS &&
			    ui_is_obj_valid(power_labels[label_idx], UI_SCREEN_POWER)) {
				if (snap.fg_ext.time_to_empty_min > 0U) {
					snprintf(buf, sizeof(buf), "Time Empty: %uh%02um",
						 snap.fg_ext.time_to_empty_min / 60U,
						 snap.fg_ext.time_to_empty_min % 60U);
				} else {
					snprintf(buf, sizeof(buf), "Time Empty: --");
				}
				power_ui_label_set(label_idx, buf, UI_SCREEN_POWER);
			}
			label_idx++;

			if (label_idx < MAX_POWER_LABELS &&
			    ui_is_obj_valid(power_labels[label_idx], UI_SCREEN_POWER)) {
				if (snap.fg_ext.time_to_full_min > 0U) {
					snprintf(buf, sizeof(buf), "Time Full: %uh%02um",
						 snap.fg_ext.time_to_full_min / 60U,
						 snap.fg_ext.time_to_full_min % 60U);
				} else {
					snprintf(buf, sizeof(buf), "Time Full: --");
				}
				power_ui_label_set(label_idx, buf, UI_SCREEN_POWER);
			}
			label_idx++;

			if (label_idx < MAX_POWER_LABELS &&
			    ui_is_obj_valid(power_labels[label_idx], UI_SCREEN_POWER)) {
				int32_t ma = snap.fg_ext.avg_current_ma;
				snprintf(buf, sizeof(buf), "Avg Current: %ld mA",
					 (long)((ma < 0) ? -ma : ma));
				power_ui_label_set(label_idx, buf, UI_SCREEN_POWER);
			}
			label_idx++;
		}
	} else {
		power_ui_fill_battery_fallback(power_labels, MAX_POWER_LABELS,
					       UI_SCREEN_POWER, &label_idx,
					       buf, sizeof(buf));
	}
}

void power_ui_create_power_screen(lv_obj_t *screen)
{
	if (screen == NULL) {
		LOG_ERR("Failed to create power screen - out of memory");
		return;
	}
	
	/* Initialize all labels to NULL first (prevent stale pointers) */
	for (int i = 0; i < MAX_POWER_LABELS; i++) {
		power_labels[i] = NULL;
	}
	
	ns_apply_screen_bg(screen);

	/* Title */
	lv_obj_t *title = ns_make_screen_title(screen, "Battery");

	if (title == NULL) {
		LOG_ERR("Failed to create battery title label - out of memory");
		ui_delete_screen(UI_SCREEN_POWER);
		return;
	}

	fg_dev = power_batt_fuel_gauge();
	if (!power_batt_fuel_gauge_ready()) {
		LOG_WRN("Fuel gauge not ready on battery screen");
	}

	/* Scrollable fuel-gauge fields (12 rows). */
	lv_obj_t *content = lv_obj_create(screen);

	if (content == NULL) {
		ui_delete_screen(UI_SCREEN_POWER);
		return;
	}
	lv_obj_remove_style_all(content);
	lv_obj_set_size(content, 240, 240 - (UI_SHELL_TOP_INSET + 30));
	lv_obj_align(content, LV_ALIGN_TOP_MID, 0, UI_SHELL_TOP_INSET + 30);
	lv_obj_set_style_bg_color(content, NS_COLOR_BG, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(content, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_pad_all(content, 4, LV_PART_MAIN);
	lv_obj_set_scroll_dir(content, LV_DIR_VER);
	lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);

	const char *label_names[] = {
		"Pack", "VBAT", "VCELL", "Current", "SOC", "Remaining", "Full Cap",
		"Design", "Temp", "Cycles", "Time Empty", "Time Full", "Avg Current"
	};

	int y_offset = 0;
	int num_labels = sizeof(label_names) / sizeof(label_names[0]);
	for (int i = 0; i < MAX_POWER_LABELS && i < num_labels; i++) {
		s_power_text[i][0] = '\0';
		power_labels[i] = lv_label_create(content);
		if (power_labels[i] == NULL) {
			LOG_ERR("Failed to create power label %d - out of memory", i);
			for (int j = 0; j < i; j++) {
				power_labels[j] = NULL;
			}
			ui_delete_screen(UI_SCREEN_POWER);
			return;
		}

		snprintf(buf, sizeof(buf), "%s: --", label_names[i]);
		lv_label_set_text(power_labels[i], buf);
		strncpy(s_power_text[i], buf, sizeof(s_power_text[i]) - 1U);
		lv_obj_set_style_text_font(power_labels[i], POWER_FONT_STATIC, LV_PART_MAIN);
		lv_obj_set_style_text_color(power_labels[i], NS_COLOR_TEXT_DIM, LV_PART_MAIN);
		lv_obj_align(power_labels[i], LV_ALIGN_TOP_LEFT, 4, y_offset);
		y_offset += 14;
	}

	update_power_screen();
}

void power_ui_update_power_screen(void)
{
	update_power_screen();
}

void power_ui_cleanup_power_screen(void)
{
	for (int i = 0; i < MAX_POWER_LABELS; i++) {
		power_labels[i] = NULL;
		s_power_text[i][0] = '\0';
	}
	fg_dev = NULL;
}
#endif /* CONFIG_UI_SCREEN_POWER */

