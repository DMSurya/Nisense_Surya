/* glucose_ui.h – Metabolic (glucose) UI screen header
 *
 * Declarations for Metabolic panel (BL-GLU / INS-RES / BL-HEM / HOMA-IR).
 */

#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create glucose measurement UI screen
 *
 * @param screen LVGL screen object to populate
 */
void glucose_ui_create_screen(lv_obj_t *screen);

/**
 * @brief Update glucose UI screen with latest data
 */
void glucose_ui_update_screen(void);

/**
 * @brief Cleanup glucose UI screen resources
 */
void glucose_ui_cleanup_screen(void);

#ifdef __cplusplus
}
#endif
