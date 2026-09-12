/* ui_theme.c – NiSense brand theme helpers
 *
 * Shared card styling and status-color mapping. A single static lv_style_t is
 * reused by every card to keep RAM/flash footprint low (the device has only a
 * few KB of headroom), instead of applying inline styles per object.
 */

#include "ui_theme.h"
#include "ui_common.h"   /* UI_FONT_* */
#include "ui_shell.h"    /* UI_SHELL_TOP_INSET */
#include "ui_obj_track.h"
#include <stddef.h>

/* Shared card style — initialized once, referenced by all cards. */
static lv_style_t s_card_style;
static bool s_card_style_ready;

static void ns_card_style_init(void)
{
	if (s_card_style_ready) {
		return;
	}
	lv_style_init(&s_card_style);
	lv_style_set_bg_color(&s_card_style, NS_COLOR_CARD);
	lv_style_set_bg_opa(&s_card_style, LV_OPA_COVER);
	lv_style_set_border_color(&s_card_style, NS_COLOR_BORDER);
	lv_style_set_border_width(&s_card_style, 1);
	lv_style_set_radius(&s_card_style, 12);
	lv_style_set_pad_all(&s_card_style, 8);
	lv_style_set_pad_column(&s_card_style, 8);
	s_card_style_ready = true;
}

lv_color_t ns_status_color(enum ns_status level)
{
	switch (level) {
	case NS_STATUS_NORMAL:    return NS_COLOR_NORMAL;
	case NS_STATUS_ATTENTION: return NS_COLOR_WARNING;
	case NS_STATUS_RISK:      return NS_COLOR_RISK;
	case NS_STATUS_CRITICAL:  return NS_COLOR_CRITICAL;
	case NS_STATUS_INFO:      return NS_COLOR_INFO;
	case NS_STATUS_NEUTRAL:   /* fallthrough */
	default:                  return NS_COLOR_TEXT_DIM;
	}
}

void ns_apply_screen_bg(lv_obj_t *screen)
{
	if (!screen) {
		return;
	}
	lv_obj_set_style_bg_color(screen, NS_COLOR_BG, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
}

void ns_make_param_card(lv_obj_t *parent, const char *name, lv_color_t accent,
			const void *icon_src, struct ns_card *out)
{
	if (!parent || !out) {
		return;
	}
	ns_card_style_init();

	/* Root card container */
	lv_obj_t *card = lv_obj_create(parent);
	lv_obj_remove_style_all(card);
	lv_obj_add_style(card, &s_card_style, LV_PART_MAIN);
	lv_obj_set_size(card, 224, 56);
	lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

	/* Icon slot (left) */
	lv_obj_t *icon = NULL;
	if (icon_src) {
		icon = lv_image_create(card);
		lv_image_set_src(icon, icon_src);
		lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);
	}

	/* Parameter name (top-left, after icon) */
	lv_obj_t *name_label = lv_label_create(card);
	lv_label_set_text(name_label, name ? name : "");
	lv_obj_set_style_text_color(name_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
	const lv_font_t *font14 = UI_FONT_14;
	const lv_font_t *font20 = UI_FONT_20;
	lv_obj_set_style_text_font(name_label, font14, LV_PART_MAIN);
	lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, icon_src ? 48 : 0, 0);

	/* Big value (right) */
	lv_obj_t *value_label = lv_label_create(card);
	lv_label_set_text(value_label, "--");
	lv_obj_set_style_text_color(value_label, accent, LV_PART_MAIN);
	lv_obj_set_style_text_font(value_label, font20, LV_PART_MAIN);
	lv_obj_align(value_label, LV_ALIGN_BOTTOM_LEFT, icon_src ? 48 : 0, 0);

	/* Status dot (right edge) */
	lv_obj_t *status_dot = lv_obj_create(card);
	lv_obj_remove_style_all(status_dot);
	lv_obj_set_size(status_dot, 12, 12);
	lv_obj_set_style_radius(status_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
	lv_obj_set_style_bg_color(status_dot, NS_COLOR_NORMAL, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(status_dot, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_clear_flag(status_dot, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_align(status_dot, LV_ALIGN_RIGHT_MID, 0, 0);

	out->card = card;
	out->icon = icon;
	out->name_label = name_label;
	out->value_label = value_label;
	out->status_dot = status_dot;
}

/* Match legacy home health tiles (device_info 2×2 grid). */
#define NS_RESULT_TILE_W         110
#define NS_RESULT_TILE_H         62
#define NS_RESULT_TILE_GAP       6
#define NS_RESULT_TILE_ICON_GAP  44

void ns_make_result_tile(lv_obj_t *parent, const char *name, lv_color_t accent,
			 const void *icon_src, struct ns_card *out)
{
	if (!parent || !out) {
		return;
	}
	ns_card_style_init();

	lv_obj_t *card = lv_obj_create(parent);
	lv_obj_remove_style_all(card);
	lv_obj_add_style(card, &s_card_style, LV_PART_MAIN);
	lv_obj_set_size(card, NS_RESULT_TILE_W, NS_RESULT_TILE_H);
	lv_obj_set_style_pad_all(card, 4, LV_PART_MAIN);
	lv_obj_set_style_radius(card, 10, LV_PART_MAIN);
	lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

	const lv_coord_t text_x = icon_src ? NS_RESULT_TILE_ICON_GAP : 0;

	lv_obj_t *icon = NULL;
	if (icon_src) {
		icon = lv_image_create(card);
		lv_image_set_src(icon, icon_src);
		/* Native XIP icon size (40 px) — same as legacy home tiles. */
		lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);
	}

	lv_obj_t *status_dot = lv_obj_create(card);
	lv_obj_remove_style_all(status_dot);
	lv_obj_set_size(status_dot, 8, 8);
	lv_obj_set_style_radius(status_dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
	lv_obj_set_style_bg_color(status_dot, NS_COLOR_NORMAL, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(status_dot, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_clear_flag(status_dot, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_align(status_dot, LV_ALIGN_RIGHT_MID, 0, 0);

	lv_obj_t *name_label = lv_label_create(card);
	lv_label_set_text(name_label, name ? name : "");
	lv_obj_set_style_text_color(name_label, NS_COLOR_TEXT_DIM, LV_PART_MAIN);
	lv_obj_set_style_text_font(name_label, UI_FONT_14, LV_PART_MAIN);
	lv_obj_align(name_label, LV_ALIGN_TOP_LEFT, text_x, 0);

	lv_obj_t *value_label = lv_label_create(card);
	lv_label_set_text(value_label, "--");
	lv_label_set_long_mode(value_label, LV_LABEL_LONG_CLIP);
	lv_obj_set_width(value_label, NS_RESULT_TILE_W - text_x - 14);
	lv_obj_set_style_text_color(value_label, accent, LV_PART_MAIN);
	lv_obj_set_style_text_font(value_label, UI_FONT_20, LV_PART_MAIN);
	lv_obj_align(value_label, LV_ALIGN_BOTTOM_LEFT, text_x, 0);

	out->card = card;
	out->icon = icon;
	out->name_label = name_label;
	out->value_label = value_label;
	out->status_dot = status_dot;
}

lv_obj_t *ns_make_result_grid(lv_obj_t *parent)
{
	if (parent == NULL) {
		return NULL;
	}

	const lv_coord_t gap = NS_RESULT_TILE_GAP;
	const lv_coord_t grid_w = (lv_coord_t)(NS_RESULT_TILE_W * 2 + gap);
	const lv_coord_t grid_h = (lv_coord_t)(NS_RESULT_TILE_H * 2 + gap);

	lv_obj_t *grid = lv_obj_create(parent);
	if (grid == NULL) {
		return NULL;
	}
	lv_obj_remove_style_all(grid);
	lv_obj_set_size(grid, grid_w, grid_h);
	lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, UI_SHELL_TOP_INSET + 40);
	lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
	lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_BETWEEN,
			      LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER);
	lv_obj_set_style_pad_row(grid, gap, LV_PART_MAIN);
	lv_obj_set_style_pad_column(grid, gap, LV_PART_MAIN);
	lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
	return grid;
}

void ns_card_set_status(struct ns_card *card, enum ns_status level)
{
	if (!card || !card->status_dot) {
		return;
	}
	lv_obj_set_style_bg_color(card->status_dot, ns_status_color(level),
				  LV_PART_MAIN);
}

lv_obj_t *ns_make_screen_title(lv_obj_t *parent, const char *text)
{
	if (!parent) {
		return NULL;
	}

	lv_obj_t *title = lv_label_create(parent);
	if (!title) {
		return NULL;
	}

	lv_label_set_text(title, text ? text : "");
	lv_obj_set_style_text_font(title, UI_FONT_20, LV_PART_MAIN);
	lv_obj_set_style_text_color(title, NS_COLOR_SECONDARY, LV_PART_MAIN);
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_SHELL_TOP_INSET + 8);

	const char *slug = lv_obj_get_name(parent);

	if (slug != NULL && slug[0] != '\0') {
		ui_obj_name(title, slug, "title");
	}
	return title;
}
