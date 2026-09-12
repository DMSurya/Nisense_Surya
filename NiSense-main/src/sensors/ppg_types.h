/* ppg_types.h - shared PPG API types */
#pragma once

#include <zephyr/subsys/ppg_algo/ppg_algo.h>
#include <zephyr/kernel.h>

/* Legacy trigger config structure (for timer-based measurements) */
struct ppg_trigger_config {
	uint16_t interval_minutes; /* 0 = one-shot */
	bool start_immediately;
	bool include_accel;
};
