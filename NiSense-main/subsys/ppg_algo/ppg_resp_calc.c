/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem - Respiration Rate Estimation
 *
 * Estimates respiration rate (RR) from the amplitude modulation of the
 * IR AC signal.  Breathing causes a low-frequency (~0.1–0.5 Hz) variation
 * in pulse amplitude — this is extracted with a simple envelope follower
 * and then peak-counted.
 *
 * Requirements:
 *   - Minimum 20 s of valid IR AC samples
 *   - Sample rate provided by caller
 *   - Output range: 6–30 breaths/min
 */

#include "ppg_algo_priv.h"

LOG_MODULE_DECLARE(ppg_algo, CONFIG_PPG_ALGO_LOG_LEVEL);

/* Valid respiration range (breaths/min) */
#define RESP_MIN_BPM 6
#define RESP_MAX_BPM 30

/* Minimum samples needed: 20 s × sample_rate */
#define RESP_MIN_SECONDS 20

/* Respiration can never produce a result if the sample buffer holds less
 * than RESP_MIN_SECONDS of data — catch the misconfiguration at build time. */
BUILD_ASSERT(CONFIG_PPG_ALGO_BUFFER_SECONDS >= RESP_MIN_SECONDS,
	     "CONFIG_PPG_ALGO_RESP_ENABLED requires CONFIG_PPG_ALGO_BUFFER_SECONDS >= 20");

/* IIR alpha for envelope follower (× 256).
 * alpha = 1/16 → time constant ~16 samples (~0.5 s at 33 Hz) */
#define ENV_ALPHA_X256 16

int ppg_resp_calculate(const int32_t *ir_ac, uint16_t count,
		       uint16_t sample_rate,
		       struct ppg_resp_state *state,
		       uint8_t *resp_bpm,
		       uint8_t *confidence)
{
	if (!ir_ac || !state || !resp_bpm || !confidence || sample_rate == 0) {
		return -EINVAL;
	}

	*resp_bpm   = 0;
	*confidence = 0;

	uint16_t min_samples = (uint16_t)RESP_MIN_SECONDS * sample_rate;

	if (count < min_samples) {
		LOG_DBG("Resp: not enough data (%u < %u samples)", count, min_samples);
		return -ENODATA;
	}

	/*
	 * Step 1 – build a rectified envelope of the IR AC signal.
	 * Use a simple IIR peak-follower: env = alpha×|ir_ac| + (1-alpha)×env
	 * (with fast attack, slow release to track breath amplitude).
	 */
	int32_t env = 0;
	uint32_t n_peaks = 0;
	uint32_t last_peak_idx = 0;
	uint32_t interval_sum = 0;
	bool was_rising = false;
	int32_t prev_env = 0;

	/* First pass: build envelope and detect peaks */
	for (uint16_t i = 0; i < count; i++) {
		int32_t abs_ac = (ir_ac[i] < 0) ? -ir_ac[i] : ir_ac[i];

		/* IIR envelope */
		env = (ENV_ALPHA_X256 * abs_ac + (256 - ENV_ALPHA_X256) * env) / 256;

		/* Peak detection on the envelope */
		bool rising = (env > prev_env);

		if (!rising && was_rising && i > 0) {
			/* Falling edge → envelope peak (breath peak) */
			if (last_peak_idx == 0) {
				last_peak_idx = i;
			} else {
				uint32_t interval = i - last_peak_idx;
				/* Filter: must be in 0.1–0.5 Hz range */
				uint32_t min_interval = sample_rate * 2; /* 2 s → 30 bpm max */
				uint32_t max_interval = sample_rate * 10; /* 10 s → 6 bpm min */

				if (interval >= min_interval && interval <= max_interval) {
					interval_sum += interval;
					n_peaks++;
					last_peak_idx = i;
				}
			}
		}

		prev_env = env;
		was_rising = rising;
	}

	if (n_peaks < 2) {
		LOG_DBG("Resp: only %u peaks detected — insufficient", n_peaks);
		return -ENODATA;
	}

	/* Average inter-breath interval in samples */
	uint32_t avg_interval = interval_sum / n_peaks;

	if (avg_interval == 0) {
		return -EINVAL;
	}

	/* breaths/min = 60 × sample_rate / avg_interval */
	uint32_t bpm = ((uint32_t)60 * sample_rate) / avg_interval;

	if (bpm < RESP_MIN_BPM || bpm > RESP_MAX_BPM) {
		LOG_DBG("Resp BPM out of range: %u", bpm);
		return -ERANGE;
	}

	*resp_bpm = (uint8_t)bpm;

	/*
	 * Confidence: scale by number of complete breaths detected.
	 * 3+ full breaths → 100 %, 2 breaths → 70 %, scaled linearly.
	 */
	uint32_t conf = (n_peaks >= 4) ? 100 : (n_peaks * 25);
	*confidence = (uint8_t)CLAMP(conf, 0, 100);

	LOG_DBG("Resp: %u bpm (%u peaks, avg_interval=%u samples, conf=%u%%)",
		bpm, n_peaks, avg_interval, *confidence);

	return 0;
}
