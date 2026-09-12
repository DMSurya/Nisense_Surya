/* device_info_ui.h – Home screen (digital clock) */

#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void device_info_ui_create_screen(lv_obj_t *screen);
void device_info_ui_update_screen(void);
void device_info_ui_on_fonts_ready(void);
int device_info_ui_init(void);
void device_info_ui_cleanup_screen(void);

/** Force weekday/date redraw on next update (e.g. after BLE CTS sync). */
void device_info_ui_invalidate_clock(void);

#ifdef __cplusplus
}
#endif
