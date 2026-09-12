/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * measure_ui.h — Unified manual "Measure" screen.
 *
 * A single screen that runs the full measurement cycle on demand (body temp +
 * HR/SpO2/Hb + glucose, including the finger spot-check) via the health
 * scheduler, and shows live progress with a per-step status. Results are then
 * presented by the UI agent's slideshow.
 *
 * @author Ponmadasamy Muthuraj <ponmadasamy@live.com>
 */

#ifndef MEASURE_UI_H_
#define MEASURE_UI_H_

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Create the Measure screen widgets on @p screen. */
void measure_ui_create_screen(lv_obj_t *screen);

/** Refresh progress/status (called periodically from the UI update loop). */
void measure_ui_update_screen(void);

/** Release static widget references. */
void measure_ui_cleanup_screen(void);

#ifdef __cplusplus
}
#endif

#endif /* MEASURE_UI_H_ */
