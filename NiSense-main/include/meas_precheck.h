/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Storage-backend measurement pre-check (NOR record store).
 */

#ifndef MEAS_PRECHECK_H_
#define MEAS_PRECHECK_H_

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/sys/util.h>

#include "health_sched.h"

#ifdef __cplusplus
extern "C" {
#endif

enum meas_storage_backend {
	MEAS_STORAGE_RECORD_STORE = 0,
};

enum meas_need_bits {
	MEAS_NEED_STORAGE       = BIT(0), /**< Persist results (record store) */
	MEAS_NEED_GLUCOSE_MODEL = BIT(1), /**< XIP / matrix model blob */
};

struct meas_precheck_result {
	bool ok;
	uint32_t missing_bits;
	enum meas_storage_backend backend;
	const char *reason; /**< Short static string for log / notice UI */
};

int meas_precheck(uint32_t needs, struct meas_precheck_result *out);
int meas_precheck_for_step(enum health_sched_step step, struct meas_precheck_result *out);
uint32_t meas_precheck_cycle_needs(void);

#ifdef __cplusplus
}
#endif

#endif /* MEAS_PRECHECK_H_ */
