/**
 * @file accel_ui.h
 * @brief Accelerometer visualization UI screen
 */

#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create accelerometer UI screen
 * 
 * @return Pointer to created screen object, or NULL on failure
 */
lv_obj_t *ui_create_screen_accel(void);

/**
 * @brief Update accelerometer visualization
 * 
 * Reads accelerometer data and updates UI elements
 */
void ui_update_screen_accel(void);

/** Clear widget pointers after the accel screen is removed */
void accel_ui_cleanup_screen(void);

#ifdef __cplusplus
}
#endif
