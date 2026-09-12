/* ui_hold_overlay.h — full-screen hold countdown for hardware buttons */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_hold_overlay_cb_t)(void *user_data);

/**
 * Show a modal overlay with a large seconds countdown.
 * Call ui_hold_overlay_cancel() on early release.
 */
void ui_hold_overlay_begin(const char *title, const char *subtitle,
			   uint32_t duration_ms, ui_hold_overlay_cb_t on_complete,
			   void *user_data);

void ui_hold_overlay_cancel(void);
/** Destroy overlay immediately (LVGL thread only). */
void ui_hold_overlay_dismiss(void);
bool ui_hold_overlay_active(void);

#ifdef __cplusplus
}
#endif
