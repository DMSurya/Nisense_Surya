#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void max32664_update_screen_create(lv_obj_t *parent);
void max32664_update_screen_refresh(void);
void max32664_update_screen_destroy(void);  /* Cleanup USB state when screen exits */

#ifdef __cplusplus
}
#endif
