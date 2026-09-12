/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Timed notice screen — show a short message, then restore the previous screen.
 */

#ifndef UI_NOTICE_H_
#define UI_NOTICE_H_

#include <lvgl.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI_NOTICE_DEFAULT_TIMEOUT_MS 2500U

/**
 * Show a notice screen with @p title / @p detail, then after @p timeout_ms
 * (0 = UI_NOTICE_DEFAULT_TIMEOUT_MS) restore the screen that was focused.
 * Safe from the UI / main context; uses ui_request_screen for navigation.
 */
void ui_notice_show(const char *title, const char *detail, uint32_t timeout_ms);

/** Dismiss early and restore the previous screen (no-op if not showing). */
void ui_notice_dismiss(void);

void ui_notice_create_screen(lv_obj_t *screen);
void ui_notice_cleanup_screen(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_NOTICE_H_ */
