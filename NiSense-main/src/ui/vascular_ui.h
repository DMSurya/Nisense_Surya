/* vascular_ui.h – Vascular UI screen header
 *
 * Displays SDNN, RMSSD, BP Sys/Dia from PPG (cuffless estimate).
 */

#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void vascular_ui_create_screen(lv_obj_t *screen);
void vascular_ui_update_screen(void);
void vascular_ui_cleanup_screen(void);

#ifdef __cplusplus
}
#endif
