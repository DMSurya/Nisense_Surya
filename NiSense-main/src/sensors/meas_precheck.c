/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Storage-backend measurement pre-check (NOR record store).
 */

#include "meas_precheck.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "record_store.h"
#if defined(CONFIG_GLUCOSE_SENSOR)
#include "glucose.h"
#endif

LOG_MODULE_REGISTER(meas_precheck, LOG_LEVEL_INF);

static enum meas_storage_backend active_backend(void)
{
	return MEAS_STORAGE_RECORD_STORE;
}

static int check_storage_record_store(struct meas_precheck_result *out)
{
	struct record_store_stats st;

	record_store_get_stats(&st);
	if (st.slot_count == 0U) {
		out->missing_bits |= MEAS_NEED_STORAGE;
		out->reason = "Storage unavailable";
		return -ENODEV;
	}

	uint32_t free_slots = (st.slot_count > st.pending) ?
			      (st.slot_count - st.pending) : 0U;

	/* Hard floor: need room for one full PPG measurement (summary + raw). */
	if (free_slots < RECORD_STORE_PPG_FOOTPRINT_SLOTS) {
		out->missing_bits |= MEAS_NEED_STORAGE;
		out->reason = "Storage nearly full — sync/wipe before measuring";
		LOG_ERR("Record store free=%u < PPG footprint %u (pending=%u dropped=%u)",
			free_slots, RECORD_STORE_PPG_FOOTPRINT_SLOTS, st.pending,
			st.dropped);
		return -ENOSPC;
	}

	return 0;
}

static int check_storage(struct meas_precheck_result *out)
{
	switch (out->backend) {
	case MEAS_STORAGE_RECORD_STORE:
	default:
		return check_storage_record_store(out);
	}
}

static int check_glucose_model(struct meas_precheck_result *out)
{
#if !defined(CONFIG_GLUCOSE_SENSOR)
	ARG_UNUSED(out);
	return 0;
#else
	if (!glucose_model_ready()) {
		(void)glucose_model_revalidate();
	}
	if (!glucose_model_ready()) {
		out->missing_bits |= MEAS_NEED_GLUCOSE_MODEL;
		out->reason = "Glucose model missing";
		return -ENOENT;
	}
	return 0;
#endif
}

int meas_precheck(uint32_t needs, struct meas_precheck_result *out)
{
	int ret = 0;

	if (out == NULL) {
		return -EINVAL;
	}

	out->ok = true;
	out->missing_bits = 0U;
	out->backend = active_backend();
	out->reason = NULL;

	if ((needs & MEAS_NEED_STORAGE) != 0U) {
		ret = check_storage(out);
		if (ret != 0) {
			out->ok = false;
			return ret;
		}
	}

	if ((needs & MEAS_NEED_GLUCOSE_MODEL) != 0U) {
		ret = check_glucose_model(out);
		if (ret != 0) {
			out->ok = false;
			return ret;
		}
	}

	return 0;
}

uint32_t meas_precheck_cycle_needs(void)
{
	return MEAS_NEED_STORAGE;
}

int meas_precheck_for_step(enum health_sched_step step, struct meas_precheck_result *out)
{
	uint32_t needs = MEAS_NEED_STORAGE;

	switch (step) {
	case HEALTH_STEP_GLUCOSE:
		needs |= MEAS_NEED_GLUCOSE_MODEL;
		break;
	case HEALTH_STEP_IDLE:
	case HEALTH_STEP_DONE:
		needs = meas_precheck_cycle_needs();
		break;
	case HEALTH_STEP_TEMP_PRE:
	case HEALTH_STEP_TEMP_POST:
	case HEALTH_STEP_PPG:
	default:
		break;
	}

	return meas_precheck(needs, out);
}
