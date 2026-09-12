/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem - Trigger Mechanism
 *
 * Implements event triggers for sample-ready and measurement-complete events.
 */

#include "ppg_algo_priv.h"

LOG_MODULE_DECLARE(ppg_algo, CONFIG_PPG_ALGO_LOG_LEVEL);

void ppg_trigger_fire(enum ppg_trigger_type type)
{
	if (type >= ARRAY_SIZE(ppg_state.triggers)) {
		return;
	}

	struct ppg_trigger_entry *entry = &ppg_state.triggers[type];

	if (entry->enabled && entry->handler) {
		LOG_DBG("Firing trigger type %u", type);
		entry->handler(type, entry->user_data);
	}
}
