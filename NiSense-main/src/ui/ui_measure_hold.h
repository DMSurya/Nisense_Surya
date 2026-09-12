/* ui_measure_hold.h — long-press on param cards to start/stop measurements */

#pragma once

#include <lvgl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ui_measure_hold_cb_t)(void *user_data);

/**
 * Long-press context: attach to an ns_card.card root.
 * Shows a left-to-right overlay while held; fires @p cb after
 * CONFIG_UI_MEASURE_LONG_PRESS_MS.
 */
struct ui_measure_hold {
	lv_obj_t *card;
	lv_obj_t *overlay;
	lv_timer_t *timer;
	uint32_t press_start_ms;
	bool armed;
	ui_measure_hold_cb_t cb;
	void *user_data;
};

/**
 * Attach press handling to @p card (must outlive the hold).
 * Creates overlay + timer on first call; safe to call once per context.
 */
void ui_measure_hold_init(struct ui_measure_hold *hold, lv_obj_t *card,
			  ui_measure_hold_cb_t cb, void *user_data);

/** Remove timer/overlay and event handler (screen cleanup). */
void ui_measure_hold_deinit(struct ui_measure_hold *hold);

/** When false, presses are ignored (e.g. carousel swipe recovery). */
void ui_measure_hold_set_armed(struct ui_measure_hold *hold, bool armed);

/** Cancel in-progress hold animation without firing the callback. */
void ui_measure_hold_cancel(struct ui_measure_hold *hold);

#ifdef __cplusplus
}
#endif
