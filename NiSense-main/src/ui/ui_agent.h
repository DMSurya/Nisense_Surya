/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * ui_agent.h — The agentic UI presenter.
 *
 * Subscribes to the measurement event bus and proactively presents results.
 * Phase 2 responsibility: the post-cycle "results slideshow" — a full-screen
 * overlay (rendered on the LVGL top layer so it floats above whatever screen
 * is active, without disturbing the screen carousel) that auto-cycles big
 * icon + value cards (Body Temp → Heart Rate → SpO2 → Hb → Glucose) and
 * dismisses on touch or after a bounded number of loops.
 *
 * Later phases extend this agent with BLE connection auto-navigation and the
 * status home screen.
 *
 * @author Ponmadasamy Muthuraj <ponmadasamy@live.com>
 */

#ifndef UI_AGENT_H_
#define UI_AGENT_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the UI agent.
 *
 * Subscribes to the UI event bus. Must be called from the LVGL/UI context
 * after ui_init() and the event bus are ready.
 *
 * @retval 0 on success, negative errno otherwise.
 */
int ui_agent_init(void);

/**
 * @brief True while the results slideshow overlay is showing.
 */
bool ui_agent_slideshow_active(void);

/**
 * @brief Dismiss the slideshow overlay immediately (no-op if not showing).
 *
 * Call from the LVGL/UI context only.
 */
void ui_agent_dismiss_slideshow(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_AGENT_H_ */
