/*
 * ui_obj_track.c — LVGL widget naming + create/delete lifecycle trace.
 *
 * Requires CONFIG_LV_USE_OBJ_NAME. Delete/create logs require
 * CONFIG_APP_UI_OBJ_LIFECYCLE_DEBUG (off in production to save flash/RTT).
 */

#include "ui_obj_track.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

LOG_MODULE_REGISTER(ui_obj_track, CONFIG_UI_OBJ_TRACK_LOG_LEVEL);

#if defined(CONFIG_LV_USE_OBJ_NAME)

static void slug_to_path(char *buf, size_t len, const char *screen_slug, const char *part)
{
	if (screen_slug == NULL || part == NULL || len == 0U) {
		return;
	}
	(void)snprintf(buf, len, "%s/%s", screen_slug, part);
}

static void resolved_name(lv_obj_t *obj, char *buf, size_t len)
{
	if (buf == NULL || len == 0U) {
		return;
	}
	buf[0] = '\0';
	if (obj == NULL) {
		(void)snprintf(buf, len, "(null)");
		return;
	}
	lv_obj_get_name_resolved(obj, buf, len);
}

#if defined(CONFIG_APP_UI_OBJ_LIFECYCLE_DEBUG)

static void delete_log_cb(lv_event_t *e)
{
	if (lv_event_get_code(e) != LV_EVENT_DELETE) {
		return;
	}

	lv_obj_t *obj = lv_event_get_target(e);
	char name[64];

	resolved_name(obj, name, sizeof(name));
	LOG_DBG("LVGL DELETE %s", name);
}

static void hook_delete(lv_obj_t *obj)
{
	if (obj == NULL) {
		return;
	}
	lv_obj_add_event_cb(obj, delete_log_cb, LV_EVENT_DELETE, NULL);
}

static void track_tree(lv_obj_t *obj, const char *prefix, uint32_t index_in_parent, int depth)
{
	if (obj == NULL || depth > 12) {
		return;
	}

	hook_delete(obj);

	const char *existing = lv_obj_get_name(obj);

	if (existing == NULL || existing[0] == '\0') {
		char auto_name[48];

		(void)snprintf(auto_name, sizeof(auto_name), "%s/n%u",
			       prefix, (unsigned)index_in_parent);
		lv_obj_set_name(obj, auto_name);
	}

	char path[64];

	resolved_name(obj, path, sizeof(path));

	uint32_t n = lv_obj_get_child_count(obj);

	for (uint32_t i = 0; i < n; i++) {
		track_tree(lv_obj_get_child(obj, i), path, i, depth + 1);
	}
}

#endif /* CONFIG_APP_UI_OBJ_LIFECYCLE_DEBUG */

void ui_obj_name(lv_obj_t *obj, const char *screen_slug, const char *part)
{
	if (obj == NULL || screen_slug == NULL || part == NULL) {
		return;
	}

	char path[48];

	slug_to_path(path, sizeof(path), screen_slug, part);
	lv_obj_set_name(obj, path);

#if defined(CONFIG_APP_UI_OBJ_LIFECYCLE_DEBUG)
	LOG_DBG("LVGL CREATE %s", path);
	hook_delete(obj);
#endif
}

void ui_obj_name_screen(lv_obj_t *screen, const char *screen_slug)
{
	if (screen == NULL || screen_slug == NULL) {
		return;
	}

	lv_obj_set_name(screen, screen_slug);

#if defined(CONFIG_APP_UI_OBJ_LIFECYCLE_DEBUG)
	LOG_DBG("LVGL CREATE screen %s", screen_slug);
	hook_delete(screen);
#endif
}

void ui_obj_track_screen_tree(lv_obj_t *screen, const char *screen_slug)
{
#if defined(CONFIG_APP_UI_OBJ_LIFECYCLE_DEBUG)
	if (screen == NULL || screen_slug == NULL) {
		return;
	}
	track_tree(screen, screen_slug, 0U, 0);
#else
	ARG_UNUSED(screen);
	ARG_UNUSED(screen_slug);
#endif
}

void ui_obj_log_screen_create(const char *screen_slug)
{
#if defined(CONFIG_APP_UI_OBJ_LIFECYCLE_DEBUG)
	LOG_DBG("screen create begin: %s", screen_slug ? screen_slug : "?");
#else
	ARG_UNUSED(screen_slug);
#endif
}

void ui_obj_log_screen_delete_begin(const char *screen_slug, uint32_t child_count)
{
#if defined(CONFIG_APP_UI_OBJ_LIFECYCLE_DEBUG)
	LOG_DBG("screen delete begin: %s (direct_children=%u)",
		screen_slug ? screen_slug : "?", (unsigned)child_count);
#else
	ARG_UNUSED(screen_slug);
	ARG_UNUSED(child_count);
#endif
}

void ui_obj_log_screen_delete_end(const char *screen_slug)
{
#if defined(CONFIG_APP_UI_OBJ_LIFECYCLE_DEBUG)
	LOG_DBG("screen delete end: %s", screen_slug ? screen_slug : "?");
#else
	ARG_UNUSED(screen_slug);
#endif
}

lv_obj_t *ui_label_create(lv_obj_t *parent, const char *screen_slug, const char *part)
{
	lv_obj_t *obj = lv_label_create(parent);

	ui_obj_name(obj, screen_slug, part);
	return obj;
}

lv_obj_t *ui_btn_create(lv_obj_t *parent, const char *screen_slug, const char *part)
{
	lv_obj_t *obj = lv_btn_create(parent);

	ui_obj_name(obj, screen_slug, part);
	return obj;
}

lv_obj_t *ui_bar_create(lv_obj_t *parent, const char *screen_slug, const char *part)
{
	lv_obj_t *obj = lv_bar_create(parent);

	ui_obj_name(obj, screen_slug, part);
	return obj;
}

#else /* !CONFIG_LV_USE_OBJ_NAME */

void ui_obj_name(lv_obj_t *obj, const char *screen_slug, const char *part)
{
	ARG_UNUSED(obj);
	ARG_UNUSED(screen_slug);
	ARG_UNUSED(part);
}

void ui_obj_name_screen(lv_obj_t *screen, const char *screen_slug)
{
	ARG_UNUSED(screen);
	ARG_UNUSED(screen_slug);
}

void ui_obj_track_screen_tree(lv_obj_t *screen, const char *screen_slug)
{
	ARG_UNUSED(screen);
	ARG_UNUSED(screen_slug);
}

void ui_obj_log_screen_create(const char *screen_slug)
{
	ARG_UNUSED(screen_slug);
}

void ui_obj_log_screen_delete_begin(const char *screen_slug, uint32_t child_count)
{
	ARG_UNUSED(screen_slug);
	ARG_UNUSED(child_count);
}

void ui_obj_log_screen_delete_end(const char *screen_slug)
{
	ARG_UNUSED(screen_slug);
}

lv_obj_t *ui_label_create(lv_obj_t *parent, const char *screen_slug, const char *part)
{
	ARG_UNUSED(screen_slug);
	ARG_UNUSED(part);
	return lv_label_create(parent);
}

lv_obj_t *ui_btn_create(lv_obj_t *parent, const char *screen_slug, const char *part)
{
	ARG_UNUSED(screen_slug);
	ARG_UNUSED(part);
	return lv_btn_create(parent);
}

lv_obj_t *ui_bar_create(lv_obj_t *parent, const char *screen_slug, const char *part)
{
	ARG_UNUSED(screen_slug);
	ARG_UNUSED(part);
	return lv_bar_create(parent);
}

#endif /* CONFIG_LV_USE_OBJ_NAME */
