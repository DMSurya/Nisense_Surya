#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI_SHELL_HEIGHT 20
#define UI_SHELL_TOP_INSET (UI_SHELL_HEIGHT + 2)

/**
 * @brief Create the global status strip once on lv_layer_top().
 *
 * Survives screen create/delete. Call once during UI bring-up before the
 * first lv_scr_load(). Safe to call again (no-op if already created).
 */
int ui_shell_create(void);

/**
 * @brief Ensure the strip exists (legacy name kept for call sites).
 *
 * @p screen is ignored; the bar is not parented to screens.
 */
void ui_shell_attach(lv_obj_t *screen);

/**
 * @brief No-op: strip is not owned by screen roots.
 */
void ui_shell_detach(lv_obj_t *screen);

/**
 * @brief Keep the strip above content on the top layer after a screen load.
 *
 * Does not rebuild widgets.
 */
void ui_shell_activate(lv_obj_t *screen);

/**
 * @brief Refresh dynamic shell values (time, connectivity, battery).
 *
 * Call from the UI tick; independent of per-screen *_update_screen().
 */
void ui_shell_update(void);

/**
 * @brief Update the wear indicator (VCNL3040: 0=off, 1=loose, 2=good).
 */
void ui_shell_set_wear_state(int wear_state);

/** Blink wear dot while a proximity-bypass (debug) measurement runs. */
void ui_shell_set_proximity_debug_blink(bool blink);

/**
 * @brief Subscribe to wear / BLE events (call once from ui_init).
 */
int ui_shell_init(void);

#ifdef __cplusplus
}
#endif
