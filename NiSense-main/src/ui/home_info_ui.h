/* home_info_ui.h – Device identity / storage info screen (swipe from home) */

#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void home_info_ui_create_screen(lv_obj_t *screen);
void home_info_ui_update_screen(void);
void home_info_ui_on_fonts_ready(void);
void home_info_ui_cleanup_screen(void);

#ifdef __cplusplus
}
#endif
