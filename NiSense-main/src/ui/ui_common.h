/* ui_common.h – Shared UI screen management interface
 *
 * Provides common interface for screen modules to interact with UI framework.
 * This allows screens to be split into separate modules while maintaining
 * a clean interface with the main UI manager.
 *
 * Architecture:
 * - UI_SCREEN_DEVICE_INFO is always index 0 (guaranteed first screen)
 * - Other screens are conditionally compiled via Kconfig
 * - Enum values auto-generate based on enabled screens
 * - UI_SCREEN_COUNT reflects only enabled screens
 */

#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

/* UI_FONT_14 / UI_FONT_20 / UI_FONT_28 macros (XIP TinyTTF vs compiled
 * Montserrat, picked automatically by Kconfig). Included here so every
 * *_ui.c that pulls in ui_common.h can style fonts without its own
 * #include "resource_fonts.h". */
#include "resource_fonts.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Screen Type Enum - Conditional compilation
 * 
 * Device Info is ALWAYS index 0 and always available.
 * Other screens get sequential indices only if enabled via Kconfig.
 * UI_SCREEN_COUNT automatically reflects the number of enabled screens.
 * ============================================================================= */
enum ui_screen {
	UI_SCREEN_DEVICE_INFO = 0,  /* Always first — home clock */

#if defined(CONFIG_UI_SCREEN_GLUCOSE)
	UI_SCREEN_GLUCOSE,          /* Metabolic */
#endif
#if defined(CONFIG_UI_SCREEN_VITALS)
	UI_SCREEN_VITALS,           /* Vital */
#endif
#if defined(CONFIG_UI_SCREEN_VASCULAR)
	UI_SCREEN_VASCULAR,         /* Vascular */
#endif
#if defined(CONFIG_UI_SCREEN_MEASURE)
	UI_SCREEN_MEASURE,          /* Measure All */
#endif
#if defined(CONFIG_UI_SCREEN_HOME_INFO)
	UI_SCREEN_HOME_INFO,
#endif
#if defined(CONFIG_UI_SCREEN_POWER)
	UI_SCREEN_POWER,
#endif
#if defined(CONFIG_UI_SCREEN_REGULATOR)
	UI_SCREEN_REGULATOR,
#endif
#if defined(CONFIG_UI_SCREEN_CHARGER)
	UI_SCREEN_CHARGER,
#endif
#if defined(CONFIG_UI_SCREEN_RTC)
	UI_SCREEN_RTC,
#endif
#if defined(CONFIG_UI_SCREEN_PPG_SAMPLE)
	UI_SCREEN_PPG_SAMPLE,
#endif
#if defined(CONFIG_UI_SCREEN_TEMPERATURE)
	UI_SCREEN_TEMPERATURE,
#endif
#if defined(CONFIG_UI_SCREEN_USB)
	UI_SCREEN_USB,
#endif
#if defined(CONFIG_UI_SCREEN_STORAGE)
	UI_SCREEN_STORAGE,
#endif
#if defined(CONFIG_UI_SCREEN_NOTICE)
	UI_SCREEN_NOTICE,           /* Transient; skipped by swipe carousel */
#endif
#if defined(CONFIG_UI_SCREEN_PROXIMITY)
	UI_SCREEN_PROXIMITY,
#endif
#if defined(CONFIG_UI_SCREEN_BUZZER)
	UI_SCREEN_BUZZER,
#endif
#if defined(CONFIG_UI_SCREEN_WIFI) && defined(CONFIG_WIFI)
	UI_SCREEN_WIFI,
#endif
#if defined(CONFIG_UI_SCREEN_BLE) && defined(CONFIG_BT)
	UI_SCREEN_BLE,
#endif
#if defined(CONFIG_UI_SCREEN_MAX32664_UPDATE)
	UI_SCREEN_MAX32664_UPDATE,
#endif
#if defined(CONFIG_UI_SCREEN_ACCEL)
	UI_SCREEN_ACCEL,
#endif

	UI_SCREEN_COUNT  /* Auto-calculated based on enabled screens */
};

/* =============================================================================
 * Note: With conditional enum, all screens in the enum are enabled by definition.
 * No need for UI_SCREEN_ENABLED() macros - if screen is in enum, it's enabled.
 * ============================================================================= */

/**
 * @brief Get screen object for a given screen index
 * 
 * @param screen_idx Screen index (from enum ui_screen)
 * @return Pointer to screen object, or NULL if screen doesn't exist
 */
lv_obj_t *ui_get_screen(enum ui_screen screen_idx);

/**
 * @brief Validate LVGL object before use
 * 
 * Checks if object pointer is non-NULL and belongs to a valid screen.
 * 
 * @param obj LVGL object to validate
 * @param expected_screen Screen the object should belong to
 * @return true if object is valid, false otherwise
 */
bool ui_is_obj_valid(lv_obj_t *obj, enum ui_screen expected_screen);

/**
 * @brief Request screen deletion
 * 
 * Marks a screen for deletion. Should be called if screen creation fails.
 * 
 * @param screen_idx Screen index to delete
 */
void ui_delete_screen(enum ui_screen screen_idx);

/**
 * @brief Request navigation to a screen (thread-safe)
 *
 * Switches the active screen to @p screen_idx. Safe to call from any thread
 * (e.g. a BLE callback): the actual switch is deferred onto the LVGL context
 * via lv_async_call(). No-op if the index is out of range.
 *
 * @param screen_idx Target screen index (from enum ui_screen)
 */
void ui_request_screen(enum ui_screen screen_idx);

/**
 * @brief Advance to the next enabled screen (wraps). Thread-safe.
 */
void ui_request_next_screen(void);

/**
 * @brief Get the index of the currently active screen.
 *
 * @return Current screen index (from enum ui_screen).
 */
enum ui_screen ui_get_current_screen(void);

/**
 * @brief True while screen navigation should be blocked (active measurement).
 */
bool ui_is_navigation_blocked(void);

/**
 * @brief Gesture state variables (exported for screen modules)
 * 
 * These are needed by screen modules to debounce button clicks during gestures.
 */
extern volatile bool gesture_in_progress;
extern uint32_t last_gesture_time;

/**
 * @brief Gesture debounce time constants
 */
#define GESTURE_DEBOUNCE_MS 300

#ifdef __cplusplus
}
#endif

