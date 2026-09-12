/* temp_ui.h - Temperature sensor UI screen
 *
 * Header for MAX302xx driver display screen.
 */

#ifndef TEMP_UI_H
#define TEMP_UI_H

#include <lvgl.h>

/**
 * @brief Create temperature sensor screen
 * 
 * @return LVGL screen object or NULL on failure
 */
lv_obj_t *create_temp_screen(void);

/**
 * @brief Update temperature screen with current sensor data
 */
void update_temp_screen(void);

/**
 * @brief Cleanup temperature screen resources
 * 
 * Called when screen is being deleted to reset internal pointers.
 */
void temp_ui_cleanup_screen(void);

#endif /* TEMP_UI_H */
