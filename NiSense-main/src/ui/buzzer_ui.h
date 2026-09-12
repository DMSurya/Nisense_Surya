/* buzzer_ui.h - Buzzer Control UI Screen Interface
 *
 * Provides UI for playing different buzzer tones.
 */

#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create buzzer control UI screen
 *
 * @param screen LVGL screen object to populate
 */
void buzzer_ui_create_screen(lv_obj_t *screen);

/**
 * @brief Update buzzer UI screen with latest data
 */
void buzzer_ui_update_screen(void);

/**
 * @brief Cleanup buzzer UI screen resources
 */
void buzzer_ui_cleanup_screen(void);

#ifdef __cplusplus
}
#endif
