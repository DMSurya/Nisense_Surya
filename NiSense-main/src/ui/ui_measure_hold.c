/*
 * ui_measure_hold.c — long-press measure gesture with LTR progress mask.
 *
 * Used on Glucose and Vitals screens instead of a bottom Start/Stop button.
 */

#include "ui_measure_hold.h"
#include "ui_theme.h"

#include <zephyr/kernel.h>

#define HOLD_TIMER_MS  50U

static void hold_cancel_visual(struct ui_measure_hold *hold)
{
	if (hold == NULL) {
		return;
	}

	if (hold->timer != NULL) {
		lv_timer_pause(hold->timer);
	}

	if (hold->overlay != NULL && lv_obj_is_valid(hold->overlay)) {
		lv_obj_add_flag(hold->overlay, LV_OBJ_FLAG_HIDDEN);
		lv_obj_set_width(hold->overlay, 0);
	}
}

static void hold_timer_cb(lv_timer_t *timer)
{
	struct ui_measure_hold *hold = lv_timer_get_user_data(timer);

	if (hold == NULL || hold->card == NULL || !lv_obj_is_valid(hold->card)) {
		return;
	}

	const uint32_t duration_ms = (uint32_t)CONFIG_UI_MEASURE_LONG_PRESS_MS;
	uint32_t elapsed = k_uptime_get_32() - hold->press_start_ms;
	lv_coord_t card_w = lv_obj_get_width(hold->card);

	if (card_w < 1) {
		card_w = 224;
	}

	lv_coord_t prog_w = (lv_coord_t)((uint64_t)elapsed * (uint64_t)card_w / duration_ms);

	if (prog_w > card_w) {
		prog_w = card_w;
	}

	if (hold->overlay != NULL && lv_obj_is_valid(hold->overlay)) {
		lv_obj_set_width(hold->overlay, prog_w);
	}

	if (elapsed >= duration_ms) {
		ui_measure_hold_cb_t cb = hold->cb;
		void *ud = hold->user_data;

		hold_cancel_visual(hold);

		if (cb != NULL) {
			cb(ud);
		}
	}
}

static void hold_event_cb(lv_event_t *e)
{
	struct ui_measure_hold *hold = lv_event_get_user_data(e);
	lv_event_code_t code = lv_event_get_code(e);

	if (hold == NULL || hold->card == NULL) {
		return;
	}

	if (code == LV_EVENT_PRESSED) {
		if (!hold->armed) {
			return;
		}

		hold->press_start_ms = k_uptime_get_32();

		if (hold->overlay != NULL && lv_obj_is_valid(hold->overlay)) {
			lv_obj_set_width(hold->overlay, 0);
			lv_obj_clear_flag(hold->overlay, LV_OBJ_FLAG_HIDDEN);
			lv_obj_move_foreground(hold->overlay);
		}

		if (hold->timer != NULL) {
			lv_timer_reset(hold->timer);
			lv_timer_resume(hold->timer);
		}
		return;
	}

	if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
		if (hold->timer != NULL && lv_timer_get_paused(hold->timer) == false) {
			const uint32_t duration_ms = (uint32_t)CONFIG_UI_MEASURE_LONG_PRESS_MS;
			uint32_t elapsed = k_uptime_get_32() - hold->press_start_ms;

			if (elapsed < duration_ms) {
				hold_cancel_visual(hold);
			}
		}
	}
}

static void hold_create_overlay(struct ui_measure_hold *hold)
{
	if (hold->card == NULL || hold->overlay != NULL) {
		return;
	}

	lv_obj_t *overlay = lv_obj_create(hold->card);

	if (overlay == NULL) {
		return;
	}

	lv_obj_remove_style_all(overlay);
	lv_obj_set_size(overlay, 0, lv_pct(100));
	lv_obj_align(overlay, LV_ALIGN_LEFT_MID, 0, 0);
	lv_obj_set_style_bg_color(overlay, NS_COLOR_PRIMARY, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(overlay, LV_OPA_40, LV_PART_MAIN);
	lv_obj_set_style_radius(overlay, 12, LV_PART_MAIN);
	lv_obj_clear_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);

	hold->overlay = overlay;
}

void ui_measure_hold_init(struct ui_measure_hold *hold, lv_obj_t *card,
			  ui_measure_hold_cb_t cb, void *user_data)
{
	if (hold == NULL || card == NULL) {
		return;
	}

	hold->card = card;
	hold->cb = cb;
	hold->user_data = user_data;
	hold->armed = true;

	hold_create_overlay(hold);

	if (hold->timer == NULL) {
		hold->timer = lv_timer_create(hold_timer_cb, HOLD_TIMER_MS, hold);
		if (hold->timer != NULL) {
			lv_timer_pause(hold->timer);
		}
	}

	lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
	lv_obj_add_flag(card, LV_OBJ_FLAG_GESTURE_BUBBLE);
	lv_obj_add_event_cb(card, hold_event_cb, LV_EVENT_PRESSED, hold);
	lv_obj_add_event_cb(card, hold_event_cb, LV_EVENT_RELEASED, hold);
	lv_obj_add_event_cb(card, hold_event_cb, LV_EVENT_PRESS_LOST, hold);
}

void ui_measure_hold_deinit(struct ui_measure_hold *hold)
{
	if (hold == NULL) {
		return;
	}

	if (hold->card != NULL && lv_obj_is_valid(hold->card)) {
		lv_obj_remove_event_cb_with_user_data(hold->card, hold_event_cb, hold);
	}

	if (hold->timer != NULL) {
		lv_timer_delete(hold->timer);
		hold->timer = NULL;
	}

	if (hold->overlay != NULL && lv_obj_is_valid(hold->overlay)) {
		lv_obj_delete(hold->overlay);
	}

	hold->overlay = NULL;
	hold->card = NULL;
	hold->cb = NULL;
	hold->user_data = NULL;
	hold->armed = false;
}

void ui_measure_hold_set_armed(struct ui_measure_hold *hold, bool armed)
{
	if (hold == NULL) {
		return;
	}

	hold->armed = armed;
	if (!armed) {
		hold_cancel_visual(hold);
	}
}

void ui_measure_hold_cancel(struct ui_measure_hold *hold)
{
	hold_cancel_visual(hold);
}
