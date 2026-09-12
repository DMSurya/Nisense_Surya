/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * ui_events.c — Fixed-capacity event bus implementation.
 *
 * @author Ponmadasamy Muthuraj <ponmadasamy@live.com>
 */

#include "ui_events.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <errno.h>

LOG_MODULE_REGISTER(ui_events, LOG_LEVEL_INF);

/* Small fixed table — current consumers: UI agent, BLE auto-nav, home status.
 * Bump if more subscribers are added; keeps allocation static. */
#define UI_EVENTS_MAX_SUBSCRIBERS 8

struct ui_event_sub {
	ui_event_cb_t cb;
	void         *user;
};

static struct ui_event_sub s_subs[UI_EVENTS_MAX_SUBSCRIBERS];
static uint8_t s_sub_count;
static struct k_spinlock s_lock;

int ui_events_subscribe(ui_event_cb_t cb, void *user)
{
	if (cb == NULL) {
		return -EINVAL;
	}

	k_spinlock_key_t key = k_spin_lock(&s_lock);

	if (s_sub_count >= UI_EVENTS_MAX_SUBSCRIBERS) {
		k_spin_unlock(&s_lock, key);
		LOG_ERR("Subscriber table full (%d)", UI_EVENTS_MAX_SUBSCRIBERS);
		return -ENOSPC;
	}

	s_subs[s_sub_count].cb = cb;
	s_subs[s_sub_count].user = user;
	s_sub_count++;

	k_spin_unlock(&s_lock, key);
	return 0;
}

void ui_events_publish(const struct ui_event *ev)
{
	if (ev == NULL) {
		return;
	}

	/* Snapshot the table under lock, then invoke callbacks unlocked so a
	 * subscriber that publishes (or subscribes) cannot deadlock. The table
	 * is append-only and small, so copying is cheap. */
	struct ui_event_sub local[UI_EVENTS_MAX_SUBSCRIBERS];
	uint8_t count;

	k_spinlock_key_t key = k_spin_lock(&s_lock);
	count = s_sub_count;
	for (uint8_t i = 0; i < count; i++) {
		local[i] = s_subs[i];
	}
	k_spin_unlock(&s_lock, key);

	for (uint8_t i = 0; i < count; i++) {
		if (local[i].cb != NULL) {
			local[i].cb(ev, local[i].user);
		}
	}
}
