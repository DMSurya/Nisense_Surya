/* proximity_ui.h - Proximity sensor UI screen
 *
 * UI for VCNL3040 proximity sensor
 */

#ifndef PROXIMITY_UI_H_
#define PROXIMITY_UI_H_

#include <lvgl.h>

/**
 * @brief Create proximity sensor screen
 * @param screen LVGL screen object to populate
 */
void proximity_ui_create_screen(lv_obj_t *screen);

/**
 * @brief Update proximity screen with current sensor data
 */
void proximity_ui_update_screen(void);

/**
 * @brief Cleanup proximity screen static pointers
 */
void proximity_ui_cleanup_screen(void);

#endif /* PROXIMITY_UI_H_ */
