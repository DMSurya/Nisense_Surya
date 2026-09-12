#include "ui.h"
#include "ui_common.h"
#include "rgb.h"
#include "buzzer.h"
#include "resource_fonts.h"
#include "ui_shell.h"
#include "power_mgr.h"
#include "ui_buttons.h"
#include "ui_obj_track.h"
#include "ui_hold_overlay.h"
#if defined(CONFIG_APP_UI_AGENT)
#include "ui_agent.h"
#endif
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

/* Conditional includes based on Kconfig */
#if defined(CONFIG_UI_SCREEN_POWER) || defined(CONFIG_UI_SCREEN_CHARGER) || \
	defined(CONFIG_UI_SCREEN_REGULATOR)
#include "power.h"
#endif

#if defined(CONFIG_UI_SCREEN_MEASURE)
#include "measure_ui.h"
#endif

#if defined(CONFIG_APP_HEALTH_SCHED)
#include "health_sched.h"
#endif
#if defined(CONFIG_APP_FEATURE_PPG)
#include "ppg.h"
#include <zephyr/subsys/ppg_algo/ppg_algo.h>
#endif
#if defined(CONFIG_GLUCOSE_SENSOR)
#include "glucose.h"
#endif
/* Device Info is always included */
#include "device_info_ui.h"
#if defined(CONFIG_UI_SCREEN_HOME_INFO)
#include "home_info_ui.h"
#endif

/* RTC header always included for timestamp caching (used by glucose logger) */
#include "rtc.h"

#if defined(CONFIG_UI_SCREEN_GLUCOSE)
#include "glucose_ui.h"
#endif

#if defined(CONFIG_UI_SCREEN_VITALS)
#include "vitals_ui.h"
#endif

#if defined(CONFIG_UI_SCREEN_VASCULAR)
#include "vascular_ui.h"
#endif

#if defined(CONFIG_UI_SCREEN_PPG_SAMPLE)
#include "ppg_sample_ui.h"
#endif

#if defined(CONFIG_UI_SCREEN_TEMPERATURE)
#include "temp_ui.h"
#endif

#if defined(CONFIG_UI_SCREEN_NOTICE)
#include "ui_notice.h"
#endif

#if defined(CONFIG_UI_SCREEN_PROXIMITY)
#include "proximity_ui.h"
#endif

#if defined(CONFIG_UI_SCREEN_BUZZER)
#include "buzzer_ui.h"
#endif

#if defined(CONFIG_UI_SCREEN_WIFI) && defined(CONFIG_WIFI)
#include "wifi_ui.h"
#endif

#if defined(CONFIG_UI_SCREEN_BLE) && defined(CONFIG_BT)
#include "ble_ui.h"
#include "ble_gatt.h"
#endif

#if defined(CONFIG_UI_SCREEN_MAX32664_UPDATE)
#include "max32664_update_ui.h"
#endif

#if defined(CONFIG_UI_SCREEN_ACCEL)
#include "accel_ui.h"
#endif

/* Legacy includes for devices that may be needed by multiple screens */
#include <zephyr/drivers/regulator.h>
#include <zephyr/drivers/charger.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/drivers/hwinfo.h>

LOG_MODULE_REGISTER(ui, CONFIG_UI_LOG_LEVEL);

#include "resource_logo.h"
#if defined(CONFIG_UI_STARTUP_LOGO)
/* logo descriptor is defined in logo_stubs_resource.c */
#endif

/* SW1 navigation is handled by nav_button.c + ui_buttons.c */


#if defined(CONFIG_LV_USE_ASSERT_MALLOC)
/**
 * @brief LVGL memory allocation failure handler
 * 
 * Called by LVGL when memory allocation fails (if CONFIG_LV_USE_ASSERT_MALLOC is enabled).
 * This helps diagnose if the BusFault is actually a memory pool exhaustion issue.
 * 
 * Note: LVGL's assertion system will call this via LV_ASSERT_MALLOC macro.
 * The exact mechanism depends on LVGL's internal implementation.
 */
void lv_assert_handler(const char *file, int line, const char *func, const char *expr)
{
	LOG_ERR("=== LVGL ASSERTION FAILED ===");
	LOG_ERR("File: %s", file ? file : "unknown");
	LOG_ERR("Line: %d", line);
	LOG_ERR("Function: %s", func ? func : "unknown");
	LOG_ERR("Expression: %s", expr ? expr : "unknown");
	
	/* Check if this is a memory allocation failure */
	if (expr && (strstr(expr, "malloc") != NULL || strstr(expr, "alloc") != NULL || 
	             strstr(expr, "mem") != NULL)) {
		LOG_ERR("*** LVGL MEMORY ALLOCATION FAILURE DETECTED ***");
		LOG_ERR("LVGL memory pool is exhausted!");
		LOG_ERR("Current pool size: %d bytes", CONFIG_LV_Z_MEM_POOL_SIZE);
		LOG_ERR("Consider increasing CONFIG_LV_Z_MEM_POOL_SIZE");
	}
	
	/* Halt execution - this is a fatal error */
	k_panic();
}

#endif /* CONFIG_LV_USE_ASSERT_MALLOC */

/* ---------- Screen Management ---------- */
/* enum ui_screen is defined in ui_common.h with conditional values */
/* Device Info is always index 0, others are conditional */

static enum ui_screen current_screen = UI_SCREEN_DEVICE_INFO;
static lv_obj_t *screens[UI_SCREEN_COUNT];
/* Minimal blank screen: used only while deleting the active screen when no other
 * managed screen exists, so we never lv_obj_del() the display's active screen.
 */
static lv_obj_t *fallback_screen;

/* Gesture state tracking to prevent accidental button clicks during swipes */
volatile bool gesture_in_progress = false;
uint32_t last_gesture_time = 0;
uint32_t last_screen_switch_time = 0;
#define GESTURE_DEBOUNCE_MS 300
#define SCREEN_SWITCH_DEBOUNCE_MS 500

/* Forward declarations */
static void screen_gesture_cb(lv_event_t *e);
static void global_button_click_filter(lv_event_t *e);
static void safe_delete_screen(enum ui_screen screen_idx, bool restore_home);
static lv_obj_t *create_screen_by_index(enum ui_screen screen_idx);

/** Transient screens (notice) are not part of the swipe carousel. */
static bool ui_screen_in_carousel(enum ui_screen screen_idx)
{
#if defined(CONFIG_UI_SCREEN_NOTICE)
	if (screen_idx == UI_SCREEN_NOTICE) {
		return false;
	}
#endif
	ARG_UNUSED(screen_idx);
	return true;
}

static enum ui_screen ui_carousel_step(enum ui_screen from, int delta)
{
	enum ui_screen cur = from;
	int guard = (int)UI_SCREEN_COUNT + 1;

	while (guard-- > 0) {
		int next = ((int)cur + delta) % (int)UI_SCREEN_COUNT;

		if (next < 0) {
			next += (int)UI_SCREEN_COUNT;
		}
		cur = (enum ui_screen)next;
		if (ui_screen_in_carousel(cur)) {
			return cur;
		}
	}
	return UI_SCREEN_DEVICE_INFO;
}

static lv_obj_t *ensure_fallback_screen(void)
{
	if (fallback_screen == NULL) {
		fallback_screen = lv_obj_create(NULL);
		if (fallback_screen != NULL) {
			lv_obj_set_style_bg_color(fallback_screen, lv_color_black(), LV_PART_MAIN);
		}
	}
	return fallback_screen;
}

/**
 * @brief Get screen name string for logging
 * 
 * With conditional enum, we need to map enum values to names at runtime.
 * 
 * @param screen_idx Screen index (must be < UI_SCREEN_COUNT)
 * @return Screen name string, or "Unknown" if invalid
 */
static const char *get_screen_name(enum ui_screen screen_idx)
{
	switch (screen_idx) {
	case UI_SCREEN_DEVICE_INFO: return "Home";
#if defined(CONFIG_UI_SCREEN_HOME_INFO)
	case UI_SCREEN_HOME_INFO: return "Info";
#endif
#if defined(CONFIG_UI_SCREEN_MEASURE)
	case UI_SCREEN_MEASURE: return "Measure";
#endif
#if defined(CONFIG_UI_SCREEN_POWER)
	case UI_SCREEN_POWER: return "Battery";
#endif
#if defined(CONFIG_UI_SCREEN_REGULATOR)
	case UI_SCREEN_REGULATOR: return "Regulator";
#endif
#if defined(CONFIG_UI_SCREEN_CHARGER)
	case UI_SCREEN_CHARGER: return "Charge";
#endif
#if defined(CONFIG_UI_SCREEN_RTC)
	case UI_SCREEN_RTC: return "RTC";
#endif
#if defined(CONFIG_UI_SCREEN_GLUCOSE)
	case UI_SCREEN_GLUCOSE: return "Metabolic";
#endif
#if defined(CONFIG_UI_SCREEN_VITALS)
	case UI_SCREEN_VITALS: return "Vital";
#endif
#if defined(CONFIG_UI_SCREEN_VASCULAR)
	case UI_SCREEN_VASCULAR: return "Vascular";
#endif
#if defined(CONFIG_UI_SCREEN_PPG_SAMPLE)
	case UI_SCREEN_PPG_SAMPLE: return "PPG Sample";
#endif
#if defined(CONFIG_UI_SCREEN_TEMPERATURE)
	case UI_SCREEN_TEMPERATURE: return "Temperature";
#endif
#if defined(CONFIG_UI_SCREEN_USB)
	case UI_SCREEN_USB: return "USB";
#endif
#if defined(CONFIG_UI_SCREEN_STORAGE)
	case UI_SCREEN_STORAGE: return "Storage";
#endif
#if defined(CONFIG_UI_SCREEN_NOTICE)
	case UI_SCREEN_NOTICE: return "Notice";
#endif
#if defined(CONFIG_UI_SCREEN_PROXIMITY)
	case UI_SCREEN_PROXIMITY: return "Proximity";
#endif
#if defined(CONFIG_UI_SCREEN_BUZZER)
	case UI_SCREEN_BUZZER: return "Buzzer";
#endif
#if defined(CONFIG_UI_SCREEN_WIFI) && defined(CONFIG_WIFI)
	case UI_SCREEN_WIFI: return "Wi-Fi";
#endif
#if defined(CONFIG_UI_SCREEN_BLE) && defined(CONFIG_BT)
	case UI_SCREEN_BLE: return "BLE";
#endif
#if defined(CONFIG_UI_SCREEN_MAX32664_UPDATE)
	case UI_SCREEN_MAX32664_UPDATE: return "MAX32664 Update";
#endif
#if defined(CONFIG_UI_SCREEN_ACCEL)
	case UI_SCREEN_ACCEL: return "Accel";
#endif
	default: return "Unknown";
	}
}

/** Lowercase slug for LVGL object paths (e.g. UI_SCREEN_MEASURE → "measure"). */
static const char *get_screen_slug(enum ui_screen screen_idx)
{
	switch (screen_idx) {
	case UI_SCREEN_DEVICE_INFO: return "home";
#if defined(CONFIG_UI_SCREEN_HOME_INFO)
	case UI_SCREEN_HOME_INFO: return "info";
#endif
#if defined(CONFIG_UI_SCREEN_MEASURE)
	case UI_SCREEN_MEASURE: return "measure";
#endif
#if defined(CONFIG_UI_SCREEN_POWER)
	case UI_SCREEN_POWER: return "battery";
#endif
#if defined(CONFIG_UI_SCREEN_REGULATOR)
	case UI_SCREEN_REGULATOR: return "regulator";
#endif
#if defined(CONFIG_UI_SCREEN_CHARGER)
	case UI_SCREEN_CHARGER: return "charge";
#endif
#if defined(CONFIG_UI_SCREEN_RTC)
	case UI_SCREEN_RTC: return "rtc";
#endif
#if defined(CONFIG_UI_SCREEN_GLUCOSE)
	case UI_SCREEN_GLUCOSE: return "metabolic";
#endif
#if defined(CONFIG_UI_SCREEN_VITALS)
	case UI_SCREEN_VITALS: return "vital";
#endif
#if defined(CONFIG_UI_SCREEN_VASCULAR)
	case UI_SCREEN_VASCULAR: return "vascular";
#endif
#if defined(CONFIG_UI_SCREEN_PPG_SAMPLE)
	case UI_SCREEN_PPG_SAMPLE: return "ppg_sample";
#endif
#if defined(CONFIG_UI_SCREEN_TEMPERATURE)
	case UI_SCREEN_TEMPERATURE: return "temperature";
#endif
#if defined(CONFIG_UI_SCREEN_USB)
	case UI_SCREEN_USB: return "usb";
#endif
#if defined(CONFIG_UI_SCREEN_STORAGE)
	case UI_SCREEN_STORAGE: return "storage";
#endif
#if defined(CONFIG_UI_SCREEN_NOTICE)
	case UI_SCREEN_NOTICE: return "notice";
#endif
#if defined(CONFIG_UI_SCREEN_PROXIMITY)
	case UI_SCREEN_PROXIMITY: return "proximity";
#endif
#if defined(CONFIG_UI_SCREEN_BUZZER)
	case UI_SCREEN_BUZZER: return "buzzer";
#endif
#if defined(CONFIG_UI_SCREEN_WIFI) && defined(CONFIG_WIFI)
	case UI_SCREEN_WIFI: return "wifi";
#endif
#if defined(CONFIG_UI_SCREEN_BLE) && defined(CONFIG_BT)
	case UI_SCREEN_BLE: return "ble";
#endif
#if defined(CONFIG_UI_SCREEN_MAX32664_UPDATE)
	case UI_SCREEN_MAX32664_UPDATE: return "max32664";
#endif
#if defined(CONFIG_UI_SCREEN_ACCEL)
	case UI_SCREEN_ACCEL: return "accel";
#endif
	default: return "unknown";
	}
}

/**
 * @brief Validate LVGL object before use
 * 
 * Checks if object pointer is non-NULL and belongs to a valid screen.
 * 
 * @param obj LVGL object to validate
 * @param expected_screen Screen the object should belong to
 * @return true if object is valid, false otherwise
 */
static bool is_obj_valid(lv_obj_t *obj, enum ui_screen expected_screen)
{
	if (obj == NULL || expected_screen >= UI_SCREEN_COUNT) {
		return false;
	}
	
	if (screens[expected_screen] == NULL) {
		return false;
	}
	
	return true;
}

/* Export screen management functions for use by screen modules */
lv_obj_t *ui_get_screen(enum ui_screen screen_idx)
{
	if (screen_idx >= UI_SCREEN_COUNT) {
		return NULL;
	}
	return screens[screen_idx];
}

bool ui_is_obj_valid(lv_obj_t *obj, enum ui_screen expected_screen)
{
	return is_obj_valid(obj, expected_screen);
}

/**
 * @brief Safely delete a UI screen with proper cleanup
 * 
 * Deletes a screen and all its child objects, cleaning up static pointers.
 * If the screen being deleted is currently active: remove its widget tree first
 * (lower peak RAM than loading another full screen while the old tree still
 * exists), then load another managed screen or a tiny fallback, then delete
 * the old screen root.
 *
 * @param screen_idx   Screen index to delete
 * @param restore_home If true and no other managed screen exists, recreate the
 *                     Device Info home on the fallback so the system is left on
 *                     a real screen (used by standalone deletes). switch_to_screen()
 *                     passes false: it creates+loads the target itself, so
 *                     recreating the (heavy) home here would needlessly hold a
 *                     second full screen tree in the LVGL heap at the same time.
 */
static void safe_delete_screen(enum ui_screen screen_idx, bool restore_home)
{
	if (screen_idx >= UI_SCREEN_COUNT || screens[screen_idx] == NULL) {
		return;
	}
	
	/* Clean up static pointers based on which screen is being deleted */
	switch (screen_idx) {
#if defined(CONFIG_UI_SCREEN_REGULATOR)
	case UI_SCREEN_REGULATOR:
		power_ui_cleanup_regulator_screen();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_CHARGER)
	case UI_SCREEN_CHARGER:
		power_ui_cleanup_charger_screen();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_POWER)
	case UI_SCREEN_POWER:
		power_ui_cleanup_power_screen();
		break;
#endif
	/* Device Info is always enabled (index 0) */
	case UI_SCREEN_DEVICE_INFO:
		device_info_ui_cleanup_screen();
		break;
#if defined(CONFIG_UI_SCREEN_HOME_INFO)
	case UI_SCREEN_HOME_INFO:
		home_info_ui_cleanup_screen();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_MEASURE)
	case UI_SCREEN_MEASURE:
		measure_ui_cleanup_screen();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_RTC)
	case UI_SCREEN_RTC:
		rtc_ui_cleanup_screen();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_GLUCOSE)
	case UI_SCREEN_GLUCOSE:
		glucose_ui_cleanup_screen();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_VITALS)
	case UI_SCREEN_VITALS:
		vitals_ui_cleanup_screen();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_VASCULAR)
	case UI_SCREEN_VASCULAR:
		vascular_ui_cleanup_screen();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_PPG_SAMPLE)
	case UI_SCREEN_PPG_SAMPLE:
		ppg_sample_ui_cleanup_screen();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_TEMPERATURE)
	case UI_SCREEN_TEMPERATURE:
		temp_ui_cleanup_screen();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_NOTICE)
	case UI_SCREEN_NOTICE:
		ui_notice_cleanup_screen();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_PROXIMITY)
	case UI_SCREEN_PROXIMITY:
		proximity_ui_cleanup_screen();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_BUZZER)
	case UI_SCREEN_BUZZER:
		buzzer_ui_cleanup_screen();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_WIFI) && defined(CONFIG_WIFI)
	case UI_SCREEN_WIFI:
		/* Remove net_mgmt callbacks and clear Wi-Fi UI state */
		wifi_ui_cleanup();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_BLE) && defined(CONFIG_BT)
	case UI_SCREEN_BLE:
		/* Unregister BLE callbacks and clear BLE UI state */
		ble_ui_cleanup_screen();
		break;
#endif
#if defined(CONFIG_UI_SCREEN_MAX32664_UPDATE)
	case UI_SCREEN_MAX32664_UPDATE:
		max32664_update_screen_destroy();  /* Re-enable USB if disabled */
		break;
#endif
#if defined(CONFIG_UI_SCREEN_ACCEL)
	case UI_SCREEN_ACCEL:
		accel_ui_cleanup_screen();
		break;
#endif
	default:
		break;
	}

	lv_obj_t *to_delete = screens[screen_idx];
	bool restore_device_info_after = false;
	const char *slug = get_screen_slug(screen_idx);

	if (to_delete != NULL) {
		ui_obj_log_screen_delete_begin(slug, lv_obj_get_child_count(to_delete));
	}

	ui_shell_detach(to_delete); /* no-op: strip not owned by screen */

	if (current_screen == screen_idx) {
		/* Deleting the active screen: never delete it while it is still "active".
		 * Strip children first so we do not briefly hold two full screen trees in RAM,
		 * then load another screen (or a tiny fallback), then delete the old root.
		 */
		int other_idx = -1;
		for (int i = 0; i < UI_SCREEN_COUNT; i++) {
			if (i != screen_idx && screens[i] != NULL) {
				other_idx = i;
				break;
			}
		}

		lv_obj_clean(to_delete);

		if (other_idx >= 0) {
			current_screen = (enum ui_screen)other_idx;
			lv_scr_load(screens[other_idx]);
		} else {
			lv_obj_t *fb = ensure_fallback_screen();
			if (fb != NULL) {
				lv_scr_load(fb);
			}
			/* Only recreate the home for standalone deletes; switch_to_screen()
			 * (restore_home == false) loads its target right after this. */
			restore_device_info_after = restore_home;
		}
	}

	lv_obj_del(to_delete);
	screens[screen_idx] = NULL;
	ui_obj_log_screen_delete_end(slug);

	if (restore_device_info_after) {
		lv_obj_t *di = create_screen_by_index(UI_SCREEN_DEVICE_INFO);
		if (di != NULL) {
			lv_scr_load(di);
		}
		current_screen = UI_SCREEN_DEVICE_INFO;
	}
}

void ui_delete_screen(enum ui_screen screen_idx)
{
	safe_delete_screen(screen_idx, true);
}

/**
 * @brief Create a screen by index on-demand
 * 
 * With conditional enum, all screens in the enum are valid.
 * 
 * @param screen_idx Screen index to create
 * @return Pointer to created screen, or NULL on failure
 */
static lv_obj_t *create_screen_by_index(enum ui_screen screen_idx)
{
	if (screen_idx >= UI_SCREEN_COUNT) {
		LOG_ERR("Invalid screen index: %d", screen_idx);
		return NULL;
	}
	
	/* Check if screen already exists - should not happen with single-screen policy */
	if (screens[screen_idx] != NULL) {
		LOG_WRN("Screen %d (%s) already exists — deleting stale instance",
			screen_idx, get_screen_name(screen_idx));
		safe_delete_screen(screen_idx, false);
	}
	
	LOG_INF("Creating screen %d (%s)", screen_idx, get_screen_name(screen_idx));
	
	lv_obj_t *screen = NULL;
	
	switch (screen_idx) {
	/* Device Info is always available (index 0) */
#if defined(CONFIG_UI_SCREEN_DEVICE_INFO)
	case UI_SCREEN_DEVICE_INFO:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			/* Register early so ui_delete_screen() can free partial-create failures */
			screens[screen_idx] = screen;
			device_info_ui_create_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_HOME_INFO)
	case UI_SCREEN_HOME_INFO:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			home_info_ui_create_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_MEASURE)
	case UI_SCREEN_MEASURE:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			measure_ui_create_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_POWER)
	case UI_SCREEN_POWER:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			power_ui_create_power_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_REGULATOR)
	case UI_SCREEN_REGULATOR:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			power_ui_create_regulator_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_CHARGER)
	case UI_SCREEN_CHARGER:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			power_ui_create_charger_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_RTC)
	case UI_SCREEN_RTC:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			rtc_ui_create_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_GLUCOSE)
	case UI_SCREEN_GLUCOSE:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			glucose_ui_create_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_VITALS)
	case UI_SCREEN_VITALS:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			vitals_ui_create_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_VASCULAR)
	case UI_SCREEN_VASCULAR:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			vascular_ui_create_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_PPG_SAMPLE)
	case UI_SCREEN_PPG_SAMPLE:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			ppg_sample_ui_create_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_TEMPERATURE)
	case UI_SCREEN_TEMPERATURE:
		screen = create_temp_screen();
		if (screen != NULL) {
			screens[screen_idx] = screen;
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_NOTICE)
	case UI_SCREEN_NOTICE:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			ui_notice_create_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_PROXIMITY)
	case UI_SCREEN_PROXIMITY:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			proximity_ui_create_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_BUZZER)
	case UI_SCREEN_BUZZER:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			buzzer_ui_create_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_WIFI) && defined(CONFIG_WIFI)
	case UI_SCREEN_WIFI:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			wifi_ui_create_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_BLE) && defined(CONFIG_BT)
	case UI_SCREEN_BLE:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			ble_ui_create_screen(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_MAX32664_UPDATE)
	case UI_SCREEN_MAX32664_UPDATE:
		screen = lv_obj_create(NULL);
		if (screen != NULL) {
			screens[screen_idx] = screen;
			max32664_update_screen_create(screen);
		}
		break;
#endif

#if defined(CONFIG_UI_SCREEN_ACCEL)
	case UI_SCREEN_ACCEL:
		screen = ui_create_screen_accel();
		break;
#endif

	default:
		LOG_ERR("Unknown or disabled screen index: %d", screen_idx);
		return NULL;
	}

	if (screen == NULL) {
		LOG_ERR("Failed to create screen %d (%s) - out of memory", 
				screen_idx, get_screen_name(screen_idx));
		return NULL;
	}

	/* Some screens may self-delete via ui_delete_screen() on partial-create failure */
	if (screens[screen_idx] == NULL) {
		LOG_ERR("Screen %d (%s) creation aborted (deleted during init)",
			screen_idx, get_screen_name(screen_idx));
		return NULL;
	}

	/* Add gesture handler */
	lv_obj_add_event_cb(screen, screen_gesture_cb, LV_EVENT_GESTURE, NULL);

	/* Add global button click event filter for haptic feedback */
	lv_obj_add_event_cb(screen, global_button_click_filter, LV_EVENT_CLICKED, NULL);

	/* Store in array */
	screens[screen_idx] = screen;

	/* Status strip is persistent on lv_layer_top (created once); ensure only. */
	ui_shell_attach(screen);

	ui_obj_name_screen(screen, get_screen_slug(screen_idx));
	ui_obj_track_screen_tree(screen, get_screen_slug(screen_idx));
	ui_obj_log_screen_create(get_screen_slug(screen_idx));

	LOG_INF("Screen %d (%s) created successfully", screen_idx, get_screen_name(screen_idx));
	return screen;
}

/**
 * @brief Switch to a different screen (with on-demand creation)
 * 
 * @param target_screen_idx Target screen index
 * @return 0 on success, negative error code on failure
 */
static int switch_to_screen(enum ui_screen target_screen_idx)
{
	if (target_screen_idx >= UI_SCREEN_COUNT) {
		LOG_ERR("Invalid target screen index: %d", target_screen_idx);
		return -EINVAL;
	}

	/* Tear down transient top-layer UI before swapping the single active screen. */
	ui_hold_overlay_cancel();
#if defined(CONFIG_APP_UI_AGENT)
	ui_agent_dismiss_slideshow();
#endif

	/* Skip if already on target screen */
	if (current_screen == target_screen_idx && screens[target_screen_idx] != NULL) {
		LOG_DBG("Already on screen %d (%s)", target_screen_idx, 
		        get_screen_name(target_screen_idx));
		return 0;
	}
	
	enum ui_screen old_screen_idx = current_screen;

	/* Always destroy the outgoing screen before allocating the next one so only
	 * one screen tree lives in the LVGL pool at a time. restore_home=false:
	 * switch_to_screen() creates and loads the target immediately below. */
	if (old_screen_idx != target_screen_idx && screens[old_screen_idx] != NULL) {
		LOG_DBG("Freeing screen %d (%s) before creating %d (%s)",
			old_screen_idx, get_screen_name(old_screen_idx),
			target_screen_idx, get_screen_name(target_screen_idx));
		safe_delete_screen(old_screen_idx, false);
	}

	/* Create new screen (recreated on every visit) */
	lv_obj_t *new_screen = create_screen_by_index(target_screen_idx);

	/* Check if creation succeeded */
	if (new_screen == NULL) {
		LOG_ERR("Failed to create screen %d (%s), attempting to recreate previous screen %d (%s)",
		        target_screen_idx, get_screen_name(target_screen_idx),
		        old_screen_idx, get_screen_name(old_screen_idx));
		/* Try to recreate the old screen since we deleted it */
		new_screen = create_screen_by_index(old_screen_idx);
		if (new_screen != NULL) {
			lv_scr_load(new_screen);
			current_screen = old_screen_idx;
		}
		return -ENOMEM;
	}
	
	/* Load new screen */
	lv_scr_load(new_screen);
	ui_shell_activate(new_screen);
	
	/* Update current_screen index */
	current_screen = target_screen_idx;
	
	LOG_INF("Switched to screen %d (%s)", target_screen_idx, 
	        get_screen_name(target_screen_idx));

#if defined(CONFIG_UI_SCREEN_DEVICE_INFO)
	if (target_screen_idx == UI_SCREEN_DEVICE_INFO) {
		device_info_ui_update_screen();
	}
#endif
#if defined(CONFIG_UI_SCREEN_HOME_INFO)
	if (target_screen_idx == UI_SCREEN_HOME_INFO) {
		home_info_ui_update_screen();
	}
#endif

	return 0;
}

/* ---------- Thread-safe screen navigation request ---------- */
/* A BLE/auth callback runs in the BT RX thread, not the LVGL context, so it
 * must not call switch_to_screen() directly. Defer the switch onto the LVGL
 * context via lv_async_call(). */
static volatile int pending_screen_request = -1;

static void ui_async_switch_cb(void *user_data)
{
	ARG_UNUSED(user_data);
	int idx = pending_screen_request;
	pending_screen_request = -1;
	if (idx >= 0 && idx < UI_SCREEN_COUNT) {
		switch_to_screen((enum ui_screen)idx);
	}
}

void ui_request_screen(enum ui_screen screen_idx)
{
	if (screen_idx >= UI_SCREEN_COUNT) {
		return;
	}
	pending_screen_request = (int)screen_idx;
	lv_async_call(ui_async_switch_cb, NULL);
}

void ui_request_next_screen(void)
{
	enum ui_screen next = ui_carousel_step(ui_get_current_screen(), 1);

	ui_request_screen(next);
}

enum ui_screen ui_get_current_screen(void)
{
	return current_screen;
}

bool ui_is_navigation_blocked(void)
{
	/*
	 * Staged live auto-navs Vitals → Vascular → Metabolic while PPG (and the
	 * health cycle) are still running. Blocking swipes fights that UX and is
	 * no longer needed for font/NOR safety (compiled fonts).
	 *
	 * Keep a short lock only while the glucose ADC finger check is active.
	 */
#if defined(CONFIG_GLUCOSE_SENSOR)
	if (glucose_is_measuring()) {
		return true;
	}
#endif
#if !defined(CONFIG_PPG_ALGO_STAGED_LIVE)
#if defined(CONFIG_APP_HEALTH_SCHED)
	if (health_sched_is_busy()) {
		return true;
	}
#endif
#if defined(CONFIG_APP_FEATURE_PPG)
	if (ppg_is_measurement_active() || ppg_algo_is_measuring()) {
		return true;
	}
#endif
#endif
	return false;
}

#if defined(CONFIG_UI_SCREEN_BLE) && defined(CONFIG_BT)
struct ui_pairing_nav_msg {
	enum ble_pairing_state state;
	uint32_t passkey;
};

static void ui_pairing_on_lvgl(void *user_data)
{
	struct ui_pairing_nav_msg *msg = user_data;

	if (msg == NULL) {
		return;
	}

	/* Update pairing state before any slow screen create — otherwise SMP can
	 * fail and the FAILED async runs before the passkey is ever shown. */
	ble_ui_handle_pairing_event(msg->state, msg->passkey);

	if (msg->state == BLE_PAIRING_CONFIRM_PASSKEY ||
	    msg->state == BLE_PAIRING_WAITING_PASSKEY ||
	    msg->state == BLE_PAIRING_BONDING ||
	    msg->state == BLE_PAIRING_COMPLETE ||
	    msg->state == BLE_PAIRING_FAILED) {
		switch_to_screen(UI_SCREEN_BLE);
		ble_ui_update_screen();
	}

	k_free(msg);
}

/* Global pairing-state callback, registered once at ui_init(). Unlike the
 * BLE screen's own handler (which only exists while that screen is created),
 * this is always active, so a host-initiated pairing request can pop the
 * pairing screen no matter which screen the user is currently viewing. */
static void ui_global_pairing_cb(enum ble_pairing_state state, uint32_t passkey)
{
	struct ui_pairing_nav_msg *msg = k_malloc(sizeof(*msg));

	if (msg == NULL) {
		return;
	}

	msg->state = state;
	msg->passkey = passkey;
	lv_async_call(ui_pairing_on_lvgl, msg);
}
#endif

/* ---------- Gesture/Swipe Handler ---------- */
static void screen_gesture_cb(lv_event_t *e)
{
	/* Validate event object */
	if (e == NULL) {
		LOG_ERR("screen_gesture_cb: NULL event");
		return;
	}
	
	uint32_t now = k_uptime_get_32();
	power_mgr_activity_notify();
	if (now - last_screen_switch_time < SCREEN_SWITCH_DEBOUNCE_MS) {
		return;
	}
	
	gesture_in_progress = true;
	last_gesture_time = now;
	
	lv_indev_t *indev = lv_indev_get_act();
	if (indev == NULL) {
		LOG_WRN("screen_gesture_cb: No active input device");
		gesture_in_progress = false;
		return;
	}
	
	lv_dir_t dir = lv_indev_get_gesture_dir(indev);
	enum ui_screen target_screen = current_screen;
	
	if (dir == LV_DIR_LEFT) {
		/* Swipe left - next screen (wraps; skips transient notice) */
		target_screen = ui_carousel_step(current_screen, 1);
		LOG_INF("Gesture: Swipe left to screen %d (%s)", 
		        target_screen, get_screen_name(target_screen));
	} else if (dir == LV_DIR_RIGHT) {
		/* Swipe right - previous screen (wraps; skips transient notice) */
		target_screen = ui_carousel_step(current_screen, -1);
		LOG_INF("Gesture: Swipe right to screen %d (%s)", 
		        target_screen, get_screen_name(target_screen));
	} else {
		/* Not a left/right swipe */
		gesture_in_progress = false;
		return;
	}

	if (ui_is_navigation_blocked()) {
		LOG_WRN("Swipe ignored — measurement in progress");
		gesture_in_progress = false;
		return;
	}
	
	/* Attempt to switch screen */
	int ret = switch_to_screen(target_screen);
	if (ret == 0) {
		last_screen_switch_time = now;
		/* Touch feedback beep for swipe gesture (fire-and-forget, timer auto-stops) */
		buzzer_play_tone(BUZZER_TONE_BEEP, 30, 30);  /* 1kHz, 30% duty, 30ms */
	} else {
		LOG_ERR("Screen switch failed: %d", ret);
	}
	
	gesture_in_progress = false;
}

/* ---------- Global Button Click Event Filter ---------- */
/**
 * @brief Global event filter for button click feedback
 * 
 * Intercepts all LV_EVENT_CLICKED events and plays a short beep for haptic feedback.
 * Applied to display object to catch all button clicks.
 */
static void global_button_click_filter(lv_event_t *e)
{
	lv_event_code_t code = lv_event_get_code(e);
	
	if (code == LV_EVENT_CLICKED) {
		/* Play short beep for all button clicks (fire-and-forget, timer auto-stops) */
		buzzer_play_tone(BUZZER_TONE_BEEP, 30, 30);  /* Auto-stops after 30ms */
	}
}

/* ---------- UI Initialization ---------- */
int ui_init(void)
{
	const struct device *disp = DEVICE_DT_GET_OR_NULL(DT_CHOSEN(zephyr_display));
	if (disp == NULL) {
		LOG_ERR("Display device not found in device tree");
		return -ENODEV;
	}
	if (!device_is_ready(disp)) {
		LOG_ERR("Display device not ready");
		return -ENODEV;
	}

	display_blanking_off(disp);

	/* Show startup logo from Resource store */
#if defined(CONFIG_UI_STARTUP_LOGO)
	/* Verify logo data before attempting to display */
	LOG_INF("Logo descriptor: data=%p, size=%u, w=%u, h=%u",
	        (void *)logo.data, logo.data_size, logo.header.w, logo.header.h);

	const uint8_t *logo_bytes = (const uint8_t *)logo.data;
	if (logo_bytes == NULL || logo.data_size == 0U) {
		LOG_WRN("Logo data missing — skipping splash");
	} else if (logo_bytes[0] == 0xFF && logo_bytes[1] == 0xFF &&
	    logo_bytes[2] == 0xFF && logo_bytes[3] == 0xFF) {
		LOG_WRN("Logo data appears erased (0xFFFFFFFF) - Resource store not flashed!");
	} else {
		LOG_INF("Logo data first 4 bytes: 0x%02x 0x%02x 0x%02x 0x%02x",
		        logo_bytes[0], logo_bytes[1], logo_bytes[2], logo_bytes[3]);
	}

	if (logo_bytes != NULL && logo.data_size > 0U) {
	lv_obj_t *logo_img = lv_image_create(lv_screen_active());
	if (logo_img != NULL) {
		lv_image_set_src(logo_img, &logo);
		lv_obj_align(logo_img, LV_ALIGN_CENTER, 0, 0);
		LOG_INF("Displayed startup logo from Resource store");
		lv_refr_now(NULL);
		k_msleep(1500);
		lv_obj_clean(lv_screen_active());
	} else {
		LOG_WRN("Failed to create startup logo image - continuing anyway");
	}
	}
#endif

#if defined(CONFIG_RESOURCE_FONTS)
	/* Fonts before any screen widgets — LVGL pool is empty, screens get real sizes. */
	if (resource_fonts_init() != 0) {
		LOG_WRN("XIP fonts init failed — screens use Montserrat fallback");
	}
#endif

	/* ---------- 3. Persistent status strip (lv_layer_top), then first screen ---------- */
	if (ui_shell_create() != 0) {
		LOG_WRN("Status strip create failed");
	}

	current_screen = UI_SCREEN_DEVICE_INFO;  /* Always start with Device Info */
	
	lv_obj_t *first_screen = create_screen_by_index(current_screen);
	if (first_screen == NULL) {
		LOG_ERR("Failed to create initial screen %d (%s)", 
		        current_screen, get_screen_name(current_screen));
		return -ENOMEM;
	}
	
	/* ---------- 4. Load first screen ---------- */
	lv_scr_load(first_screen);
	LOG_INF("Loaded initial screen: %d (%s)", current_screen, 
	        get_screen_name(current_screen));

	ui_shell_activate(first_screen);

	if (ui_shell_init() != 0) {
		LOG_WRN("Status strip event subscribe failed");
	}

	if (ui_buttons_init() != 0) {
		LOG_WRN("Hardware buttons init failed");
	}
	
	/* With conditional enum, UI_SCREEN_COUNT IS the number of enabled screens */
	LOG_INF("Multi-screen UI initialized with %d screens (Device Info always first)", 
	        UI_SCREEN_COUNT);

#if defined(CONFIG_UI_SCREEN_BLE) && defined(CONFIG_BT)
	/* Register an always-active pairing callback so a host-initiated pairing
	 * request auto-navigates to the BLE pairing screen from any screen. */
	ble_gatt_register_pairing_cb(ui_global_pairing_cb);
#endif
	
	/* Diagnostic: Check LVGL memory pool usage after initialization */
	#if defined(CONFIG_LV_USE_ASSERT_MALLOC)
	/* Log memory pool status to help diagnose allocation issues */
	LOG_INF("LVGL memory pool status:");
	LOG_INF("  Pool size: %d bytes", CONFIG_LV_Z_MEM_POOL_SIZE);
	/* Note: LVGL memory monitoring functions may not be available in all builds
	 * If available, we could call lv_mem_get_size() and lv_mem_get_free() here
	 */
	#endif
	
	return 0;
}

/* Update function to refresh dynamic data */
void ui_update(void)
{
	static uint32_t last_update = 0;
	static uint32_t last_rtc_cache_update = 0;
#if defined(CONFIG_UI_SCREEN_POWER)
	static uint32_t last_power_ui_ms;
#endif
	uint32_t now = k_uptime_get_32();
	bool screen_updated = false;

#if defined(CONFIG_UI_SCREEN_CHARGER)
	power_ui_charge_nav_poll();
#endif
	
	/* Update only the currently visible screen every 500ms */
	if (now - last_update > 500) {
		ui_shell_update();

		if (current_screen < UI_SCREEN_COUNT && screens[current_screen] != NULL) {
			/* Don't invalidate entire screen - let LVGL handle partial updates
			 * automatically. Invalidating the whole screen causes visible refresh
			 * and faded colors. Only invalidate specific objects that change.
			 */
			switch (current_screen) {
#if defined(CONFIG_UI_SCREEN_REGULATOR)
			case UI_SCREEN_REGULATOR:
				power_ui_update_regulator_screen();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_CHARGER)
			case UI_SCREEN_CHARGER:
				power_ui_update_charger_screen();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_POWER)
			case UI_SCREEN_POWER:
				if (now - last_power_ui_ms >= 2000U) {
					power_ui_update_power_screen();
					last_power_ui_ms = now;
					screen_updated = true;
				}
				break;
#endif
			/* Device Info is always enabled (index 0) */
			case UI_SCREEN_DEVICE_INFO:
				device_info_ui_update_screen();
				screen_updated = true;
				break;
#if defined(CONFIG_UI_SCREEN_HOME_INFO)
			case UI_SCREEN_HOME_INFO:
				home_info_ui_update_screen();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_MEASURE)
			case UI_SCREEN_MEASURE:
				measure_ui_update_screen();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_RTC)
			case UI_SCREEN_RTC:
				rtc_ui_update_screen();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_GLUCOSE)
			case UI_SCREEN_GLUCOSE:
				glucose_ui_update_screen();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_VITALS)
			case UI_SCREEN_VITALS:
				vitals_ui_update_screen();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_VASCULAR)
			case UI_SCREEN_VASCULAR:
				vascular_ui_update_screen();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_PPG_SAMPLE)
			case UI_SCREEN_PPG_SAMPLE:
				ppg_sample_ui_update_screen();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_TEMPERATURE)
			case UI_SCREEN_TEMPERATURE:
				update_temp_screen();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_PROXIMITY)
			case UI_SCREEN_PROXIMITY:
				proximity_ui_update_screen();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_BUZZER)
			case UI_SCREEN_BUZZER:
				buzzer_ui_update_screen();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_WIFI) && defined(CONFIG_WIFI)
			case UI_SCREEN_WIFI:
				wifi_ui_update_screen();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_BLE) && defined(CONFIG_BT)
			case UI_SCREEN_BLE:
				ble_ui_update_screen();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_MAX32664_UPDATE)
			case UI_SCREEN_MAX32664_UPDATE:
				max32664_update_screen_refresh();
				screen_updated = true;
				break;
#endif
#if defined(CONFIG_UI_SCREEN_ACCEL)
			case UI_SCREEN_ACCEL:
				ui_update_screen_accel();
				screen_updated = true;
				break;
#endif
			default:
				break;
			}
		}
		last_update = now;
	}
	
	/* Update RTC cache every second for timestamp formatter */
	/* This keeps RTC timestamps accurate without device calls during logging */
	if (now - last_rtc_cache_update > 1000) {
		rtc_update_cache();
		last_rtc_cache_update = now;
	}
}
