/* ppg_sample_ui.h – PPG raw waveform screen header */

#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void ppg_sample_ui_create_screen(lv_obj_t *screen);
void ppg_sample_ui_update_screen(void);
void ppg_sample_ui_cleanup_screen(void);

#ifdef __cplusplus
}
#endif
