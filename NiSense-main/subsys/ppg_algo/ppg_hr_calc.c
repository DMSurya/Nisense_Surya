/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem - Heart Rate Calculation
 *
 * Calculates heart rate from detected R-R intervals.
 */

#include "ppg_algo_priv.h"

LOG_MODULE_DECLARE(ppg_algo, CONFIG_PPG_ALGO_LOG_LEVEL);

/* HR calculation requires at least 3 R-R intervals */
#define MIN_RR_INTERVALS 3

int ppg_hr_calculate(struct ppg_peak_state *state,
		     uint16_t *hr_bpm,
		     uint8_t *confidence)
{
	if (!state || !hr_bpm || !confidence) {
		return -EINVAL;
	}

	/* Need minimum R-R intervals */
	if (state->rr_count < MIN_RR_INTERVALS) {
		*hr_bpm = 0;
		*confidence = 0;
		return -ENODATA;
	}

	/* Calculate average R-R interval */
	uint32_t sum = 0;
	uint32_t min_rr = UINT32_MAX;
	uint32_t max_rr = 0;

	for (uint8_t i = 0; i < state->rr_count; i++) {
		uint32_t rr = state->rr_intervals[i];
		sum += rr;
		if (rr < min_rr) {
			min_rr = rr;
		}
		if (rr > max_rr) {
			max_rr = rr;
		}
	}

	uint32_t avg_rr_ms = sum / state->rr_count;

	/* Ectopic beat rejection: discard R-R intervals more than 30% from the
	 * initial mean. Premature or post-ectopic intervals corrupt the average
	 * HR significantly; removing them improves accuracy without requiring a
	 * separate arrhythmia detector. */
	uint32_t filtered_sum   = 0U;
	uint8_t  filtered_count = 0U;
	uint32_t filtered_min   = UINT32_MAX;
	uint32_t filtered_max   = 0U;

	for (uint8_t i = 0; i < state->rr_count; i++) {
		uint32_t rr = state->rr_intervals[i];
		if (rr >= (avg_rr_ms * 70U) / 100U &&
		    rr <= (avg_rr_ms * 130U) / 100U) {
			filtered_sum += rr;
			filtered_count++;
			if (rr < filtered_min) {
				filtered_min = rr;
			}
			if (rr > filtered_max) {
				filtered_max = rr;
			}
		}
	}
	if (filtered_count >= MIN_RR_INTERVALS) {
		avg_rr_ms = filtered_sum / filtered_count;
		/*
		 * BUGFIX (chronic HR_Conf ~20%, confirmed across every
		 * captured session): the variability calculation below used
		 * to use min_rr/max_rr from the *unfiltered* interval set —
		 * exactly the raw values this ectopic-rejection step exists
		 * to discard. A single outlier RR interval (one missed or
		 * extra beat detection — common on real PPG signal, not a
		 * sign of anything wrong) would blow out that unfiltered
		 * range, push variability_percent past 50%, and floor
		 * confidence at 20% — even when the actual reported HR (which
		 * DOES use the filtered, robust average) was perfectly good.
		 * Use the same filtered set for both, so the confidence
		 * reflects the quality of the value actually being reported,
		 * not the quality of data that was already thrown out.
		 */
		min_rr = filtered_min;
		max_rr = filtered_max;
	}

	/* Calculate HR in BPM: HR = 60000 / avg_RR_ms */
	if (avg_rr_ms > 0) {
		*hr_bpm = (60000 + avg_rr_ms / 2) / avg_rr_ms; /* Round */
	} else {
		*hr_bpm = 0;
		*confidence = 0;
		return -EINVAL;
	}

	/* Calculate confidence based on R-R interval variability */
	/* Lower variability = higher confidence */
	uint32_t range = max_rr - min_rr;
	uint32_t variability_percent = (range * 100) / avg_rr_ms;

	/* Confidence: 100% if variability < 10%, decreases linearly */
	if (variability_percent < 10) {
		*confidence = 100;
	} else if (variability_percent < 50) {
		*confidence = 100 - ((variability_percent - 10) * 2);
	} else {
		*confidence = 20; /* Minimum confidence */
	}

	/* Further reduce confidence if we don't have enough intervals */
	if (state->rr_count < 5) {
		*confidence = (*confidence * state->rr_count) / 5;
	}

	LOG_DBG("HR: %u BPM, confidence: %u%%, RR intervals: %u, avg: %u ms",
		*hr_bpm, *confidence, state->rr_count, avg_rr_ms);

	return 0;
}
