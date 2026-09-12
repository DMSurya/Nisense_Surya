/* ui_obj_track.h — LVGL object naming and create/delete lifecycle logging */

#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Tag a widget with screen/part (requires CONFIG_LV_USE_OBJ_NAME).
 * Also registers LV_EVENT_DELETE logging when lifecycle debug is on.
 */
void ui_obj_name(lv_obj_t *obj, const char *screen_slug, const char *part);

/** Name the screen root (e.g. slug "measure"). */
void ui_obj_name_screen(lv_obj_t *screen, const char *screen_slug);

/**
 * After a screen module finishes building its tree: auto-name any unnamed
 * descendants and hook delete logging on every widget.
 */
void ui_obj_track_screen_tree(lv_obj_t *screen, const char *screen_slug);

void ui_obj_log_screen_create(const char *screen_slug);
void ui_obj_log_screen_delete_begin(const char *screen_slug, uint32_t child_count);
void ui_obj_log_screen_delete_end(const char *screen_slug);

/* Named widget helpers (log CREATE at DBG, set name, hook DELETE). */
lv_obj_t *ui_label_create(lv_obj_t *parent, const char *screen_slug, const char *part);
lv_obj_t *ui_btn_create(lv_obj_t *parent, const char *screen_slug, const char *part);
lv_obj_t *ui_bar_create(lv_obj_t *parent, const char *screen_slug, const char *part);

#ifdef __cplusplus
}
#endif
