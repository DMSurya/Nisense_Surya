/* ui.h – LVGL Display + Touch Interface Layer
 *
 * Initializes LVGL, enables gesture handling for FT6336 touchscreen,
 * and provides simple UI color update functions.
 */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize LVGL screen and attach gesture callback.
 *
 * This function:
 *  - powers up the display (via chosen zephyr,display)
 *  - installs LV_EVENT_GESTURE handler
 *  - returns 0 on success
 */
int ui_init(void);

/**
 * @brief Update UI screens with latest data.
 * Should be called periodically from main loop.
 */
void ui_update(void);

#ifdef __cplusplus
}
#endif
