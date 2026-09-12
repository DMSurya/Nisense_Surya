/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * ui_events.h — Lightweight in-process event bus for the agentic UI.
 *
 * Decouples the measurement layer (health scheduler, temperature, glucose,
 * PPG, BLE, wear detection) from the UI agent that reacts to those events
 * (auto-navigation, results slideshow, status home screen).
 *
 * Producers call ui_events_publish(); consumers register with
 * ui_events_subscribe(). Subscribers are stored in a fixed-size array (no heap)
 * to suit the RAM-constrained nRF52840.
 *
 * Threading: publish from thread / work-queue context only (NOT from ISRs).
 * The existing sensor modules already defer ISR work to the system work queue,
 * so this is satisfied by construction. Callbacks must be quick and must not
 * block; UI consumers should defer real work onto the LVGL context via
 * lv_async_call() / ui_request_screen().
 *
 * @author Ponmadasamy Muthuraj <ponmadasamy@live.com>
 */

#ifndef UI_EVENTS_H_
#define UI_EVENTS_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Measurement parameter classes carried by result events. */
enum ui_meas_type {
	UI_MEAS_TEMP = 0,
	UI_MEAS_VITALS,
	UI_MEAS_GLUCOSE,
};

/** Event identifiers published on the bus. */
enum ui_event_type {
	/** An automatic measurement cycle has begun (scheduler). */
	UI_EVENT_MEAS_CYCLE_STARTED = 0,
	/** All measurements in a cycle finished — UI may run the slideshow. */
	UI_EVENT_MEAS_CYCLE_COMPLETE,
	/** User cancelled an in-progress cycle (no slideshow). */
	UI_EVENT_MEAS_CYCLE_CANCELLED,
	/** A single measurement result is ready (see data.meas_type). */
	UI_EVENT_MEAS_RESULT_READY,
	/** Fresh temperature sample available (idle or in-cycle). */
	UI_EVENT_TEMP_UPDATED,
	/** Central connected over BLE. */
	UI_EVENT_BLE_CONNECTED,
	/** Central disconnected over BLE. */
	UI_EVENT_BLE_DISCONNECTED,
	/** Wear state changed (see data.wear_state). */
	UI_EVENT_WEAR_CHANGED,
	/** Battery dropped below the low threshold. */
	UI_EVENT_BATTERY_LOW,
	/** Health scheduler step guard timed out (see data.sched_step). */
	UI_EVENT_MEAS_STEP_TIMEOUT,
	/** Staged PPG: early HR/SpO2 ready — UI may show Vitals live. */
	UI_EVENT_PPG_PARTIAL_VITALS,
	/** Staged PPG: Hb/BP/Resp floor — UI may show Vascular live. */
	UI_EVENT_PPG_PARTIAL_VASCULAR,
	/** Staged PPG: authoritative record window done (NOR written); live tail may continue. */
	UI_EVENT_PPG_RECORD_READY,
};

/** Event payload. Only the field relevant to @ref ui_event.type is valid. */
struct ui_event {
	enum ui_event_type type;
	union {
		/** UI_EVENT_MEAS_RESULT_READY */
		enum ui_meas_type meas_type;
		/** UI_EVENT_TEMP_UPDATED — Celsius values, NaN-safe sentinels */
		struct {
			float wrist_c;   /* body temp (wrist), <-273 if invalid */
			float soc_c;     /* nRF die / SoC temp, <-273 if invalid */
		} temp;
		/** UI_EVENT_WEAR_CHANGED — matches VCNL3040 wear classification */
		int wear_state;
		/** UI_EVENT_BATTERY_LOW */
		int battery_soc;
		/** UI_EVENT_MEAS_STEP_TIMEOUT — health_sched_step value */
		int sched_step;
	} data;
};

/**
 * @brief Event subscriber callback.
 *
 * @param ev   Event (valid only for the duration of the call).
 * @param user Opaque pointer supplied at subscribe time.
 */
typedef void (*ui_event_cb_t)(const struct ui_event *ev, void *user);

/**
 * @brief Register a subscriber.
 *
 * @param cb   Callback (must be non-NULL).
 * @param user Opaque pointer passed back on every event.
 * @retval 0 on success.
 * @retval -EINVAL if cb is NULL.
 * @retval -ENOSPC if the subscriber table is full.
 */
int ui_events_subscribe(ui_event_cb_t cb, void *user);

/**
 * @brief Publish an event to all subscribers (synchronous fan-out).
 *
 * Safe to call when no subscribers are registered. Do not call from ISR
 * context.
 *
 * @param ev Event to deliver (must be non-NULL).
 */
void ui_events_publish(const struct ui_event *ev);

#ifdef __cplusplus
}
#endif

#endif /* UI_EVENTS_H_ */
