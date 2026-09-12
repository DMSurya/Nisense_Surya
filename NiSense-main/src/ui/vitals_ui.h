/* vitals_ui.h – Vital UI screen header
 *
 * Displays HR, SpO2, Resp, Body Temp from PPG + wrist temp.
 */

#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void vitals_ui_create_screen(lv_obj_t *screen);
void vitals_ui_update_screen(void);
void vitals_ui_cleanup_screen(void);

#ifdef __cplusplus
}
#endif
