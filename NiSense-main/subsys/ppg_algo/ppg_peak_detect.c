/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem - Peak Detection
 *
 * Detects heartbeat peaks in filtered PPG signal for HR calculation.
 *
 * @ref PPG_ALGO_DESIGN_REFERENCES — Charlton et al. Physiol Meas 2022
 * (MSPTD/qppg benchmarks); MSPTDfast; Scholkmann AMPD.
 * MCU-light local-maxima + refractory (not threshold-crossing alone).
 */

#include "ppg_algo_priv.h"

LOG_MODULE_DECLARE(ppg_algo, CONFIG_PPG_ALGO_LOG_LEVEL);

/* Peak detection constants */
#define MIN_PEAK_DISTANCE_MS 300  /* Minimum 300ms between peaks (200 BPM max) */
#define MAX_PEAK_DISTANCE_MS 2000 /* Maximum 2000ms between peaks (30 BPM min) */
#define THRESHOLD_FACTOR 60       /* Threshold = 60% of signal range */
#define THRESHOLD_DECAY_FACTOR 95 /* Decay threshold by 5% each sample for adaptation */
#define CALIBRATION_SAMPLES 50    /* Collect N samples before enabling peak detection */

void ppg_peak_detect_init(struct ppg_peak_state *state)
{
	if (!state) {
		return;
	}

	memset(state, 0, sizeof(*state));
	state->threshold = 0; /* Auto-calibrate from actual signal */
}

bool ppg_peak_detect(struct ppg_peak_state *state,
		     int32_t ac_signal,
		     uint32_t timestamp_ms,
		     uint16_t *rr_interval_ms)
{
	if (!state) {
		return false;
	}

	bool peak_found = false;

	/* Track signal maximum for adaptive threshold */
	if (ac_signal > state->signal_max) {
		state->signal_max = ac_signal;
	}

	state->sample_count++;

	/* Calibration phase: gather signal statistics, no peak detection */
	if (state->sample_count <= CALIBRATION_SAMPLES) {
		state->prev_ac = ac_signal;
		state->was_rising = false;
		state->candidate_peak_ac = 0;
		state->candidate_peak_time_ms = 0;
		state->have_candidate = false;

		if (state->sample_count == CALIBRATION_SAMPLES) {
			state->threshold = (state->signal_max * THRESHOLD_FACTOR) / 100;
			if (state->threshold < 1) {
				state->threshold = 1;
			}
			LOG_INF("Peak detect calibrated: signal_max=%d, threshold=%d",
				state->signal_max, state->threshold);
		}
		return false;
	}

	/* Slowly decay threshold so we adapt to decreasing signal amplitude */
	state->threshold = (state->threshold * THRESHOLD_DECAY_FACTOR) / 100;
	if (state->threshold < 1) {
		state->threshold = 1;
	}

	/* Update threshold upward when signal exceeds it */
	if (ac_signal > state->threshold) {
		state->threshold = (ac_signal * THRESHOLD_FACTOR) / 100;
	}

	/*
	 * Local-maxima detector: confirm a beat when the AC signal rises then
	 * falls while above the adaptive threshold (Charlton MSPTD/AMPD family,
	 * MCU-light). Threshold-crossing alone was causing double-counts.
	 */
	bool rising = (ac_signal > state->prev_ac);

	if (rising) {
		if (ac_signal > state->threshold) {
			state->candidate_peak_ac = ac_signal;
			state->candidate_peak_time_ms = timestamp_ms;
			state->have_candidate = true;
		}
	} else if (state->was_rising && state->have_candidate) {
		uint32_t time_since_last_peak =
			timestamp_ms - state->last_peak_time_ms;

		if (state->candidate_peak_ac > state->threshold &&
		    time_since_last_peak >= MIN_PEAK_DISTANCE_MS) {
			peak_found = true;

			if (state->last_peak_time_ms > 0 &&
			    time_since_last_peak <= MAX_PEAK_DISTANCE_MS) {
				if (state->rr_count < ARRAY_SIZE(state->rr_intervals)) {
					state->rr_intervals[state->rr_count] =
						time_since_last_peak;
					state->rr_count++;
				} else {
					memmove(&state->rr_intervals[0],
						&state->rr_intervals[1],
						sizeof(uint32_t) *
						(ARRAY_SIZE(state->rr_intervals) - 1));
					state->rr_intervals[ARRAY_SIZE(state->rr_intervals) - 1] =
						time_since_last_peak;
				}

				if (rr_interval_ms) {
					*rr_interval_ms = (uint16_t)time_since_last_peak;
				}
			}

			/* Persist per-beat AC amplitude for SpO2 Pk-Pk / RMS */
			if (state->beat_ac_count < ARRAY_SIZE(state->beat_ac_amp)) {
				state->beat_ac_amp[state->beat_ac_count] =
					state->candidate_peak_ac;
				state->beat_ac_count++;
			} else {
				memmove(&state->beat_ac_amp[0], &state->beat_ac_amp[1],
					sizeof(int32_t) *
					(ARRAY_SIZE(state->beat_ac_amp) - 1));
				state->beat_ac_amp[ARRAY_SIZE(state->beat_ac_amp) - 1] =
					state->candidate_peak_ac;
			}

			state->last_peak_time_ms = state->candidate_peak_time_ms;
			state->peak_detected = true;
		}
		state->have_candidate = false;
	}

	state->prev_ac = ac_signal;
	state->was_rising = rising;

	return peak_found;
}
