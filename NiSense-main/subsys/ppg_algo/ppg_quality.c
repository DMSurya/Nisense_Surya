/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem - Signal Quality Assessment
 *
 * Calculates SNR, perfusion index, skewness SQI, and motion artifacts.
 *
 * @ref PPG_ALGO_DESIGN_REFERENCES — Maxim PPG algo AC SNR
 * (PAC peak-to-peak / residual noise); Elgendi 2016 skewness SQI.
 */

#include "ppg_algo_priv.h"
#include <math.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(ppg_algo, CONFIG_PPG_ALGO_LOG_LEVEL);

/* Quality thresholds */
#define MIN_SNR_DB_EXCELLENT 20
#define MIN_SNR_DB_GOOD 15
#define MIN_SNR_DB_FAIR 10
#define MIN_SNR_DB_POOR 5

#define MIN_PI_EXCELLENT 30  /* 3.0% PI */
#define MIN_PI_GOOD 15       /* 1.5% PI */
#define MIN_PI_FAIR 5        /* 0.5% PI */

#define MOTION_THRESHOLD_MG 100

/** Integer absolute value for int32 */
static int32_t iabs32(int32_t v)
{
	return (v < 0) ? -v : v;
}

void ppg_quality_calculate(const struct ppg_sample_buffer *buffer,
			   const struct ppg_preproc_state *preproc,
			   struct ppg_quality_metrics *metrics)
{
	if (!buffer || !preproc || !metrics) {
		return;
	}

	memset(metrics, 0, sizeof(*metrics));

	/*
	 * Perfusion index from AC amplitude / DC.
	 * Prefer mean |ir_ac| over the window (pulse amplitude), not last residual.
	 */
	int32_t ac_amp = 0;

	if (buffer->ir_ac && buffer->count > 0U) {
		int64_t sum = 0;

		for (uint16_t i = 0; i < buffer->count; i++) {
			sum += iabs32(buffer->ir_ac[i]);
		}
		ac_amp = (int32_t)(sum / buffer->count);
	} else {
		ac_amp = iabs32(preproc->ir.ac);
	}

	if (preproc->ir.dc > 0) {
		uint32_t pi_x1000 = ((uint64_t)ac_amp * 100000ULL) / (uint32_t)preproc->ir.dc;

		metrics->perfusion_index_x10 = (uint8_t)CLAMP(pi_x1000 / 100U, 0U, 255U);
	}

	/*
	 * Maxim-style AC SNR: 20·log10(PAC_pp / PNoise).
	 * PAC ≈ 2 × mean |AC| (peak-to-peak proxy).
	 * Noise ≈ RMS of high-frequency residual: successive AC differences
	 * (pulse-model residual), not stddev of the pulse itself.
	 */
	if (buffer->ir_ac && buffer->count > 10U) {
		int64_t diff_sq = 0;
		uint16_t n_diff = 0;

		for (uint16_t i = 1; i < buffer->count; i++) {
			int32_t d = buffer->ir_ac[i] - buffer->ir_ac[i - 1U];

			diff_sq += (int64_t)d * (int64_t)d;
			n_diff++;
		}

		if (n_diff > 0U && ac_amp > 0) {
			int32_t noise = (int32_t)sqrt((double)(diff_sq / n_diff));
			/* Successive-diff noise overestimates; scale toward RMS of
			 * residual after a crude 1-sample predictor. */
			if (noise > 0) {
				int32_t pac_pp = ac_amp * 2;
				float snr_ratio = (float)pac_pp / (float)noise;
				float snr_db = 20.0f * log10f(snr_ratio);

				if (snr_db < 0.0f) {
					snr_db = 0.0f;
				} else if (snr_db > 60.0f) {
					snr_db = 60.0f;
				}
				metrics->snr_db_x10 = (uint16_t)(snr_db * 10.0f);
			}
		}
	}

	/*
	 * Elgendi skewness SQI (SSQI) over IR AC window.
	 * Positive skew typical of clean PPG; near-zero / negative → unfit.
	 * Stored as skew × 10 in [-100, 100].
	 */
	if (buffer->ir_ac && buffer->count > 10U) {
		int64_t sum = 0;
		uint16_t n = buffer->count;

		for (uint16_t i = 0; i < n; i++) {
			sum += buffer->ir_ac[i];
		}
		int32_t mean = (int32_t)(sum / n);
		int64_t m2 = 0;
		int64_t m3 = 0;

		for (uint16_t i = 0; i < n; i++) {
			int32_t d = buffer->ir_ac[i] - mean;

			m2 += (int64_t)d * d;
			m3 += (int64_t)d * d * d;
		}
		m2 /= n;
		m3 /= n;

		if (m2 > 0) {
			int32_t s = (int32_t)sqrt((double)m2);
			int32_t s3 = s * s * s;

			if (s3 > 0) {
				int32_t skew_x10 = (int32_t)((m3 * 10) / s3);

				metrics->skewness_x10 = (int16_t)CLAMP(skew_x10, -100, 100);
			}
		}
	}

	/* Motion from accelerometer successive L1 deltas */
	if (buffer->has_accel && buffer->count > 5) {
		uint32_t sum_mag = 0;

		for (uint16_t i = 1; i < buffer->count; i++) {
			int32_t dx = abs(buffer->accel_x[i] - buffer->accel_x[i - 1]);
			int32_t dy = abs(buffer->accel_y[i] - buffer->accel_y[i - 1]);
			int32_t dz = abs(buffer->accel_z[i] - buffer->accel_z[i - 1]);

			sum_mag += (uint32_t)(dx + dy + dz);
		}

		metrics->motion_magnitude = sum_mag / (buffer->count - 1);
		metrics->motion_detected = (metrics->motion_magnitude > MOTION_THRESHOLD_MG);
	}

	/* Overall quality from SNR + PI (+ soft skewness hint) */
	uint8_t snr_db = metrics->snr_db_x10 / 10;
	uint8_t pi = metrics->perfusion_index_x10;

	if (pi < MIN_PI_FAIR || snr_db < MIN_SNR_DB_POOR) {
		metrics->quality = PPG_QUALITY_NO_SIGNAL;
	} else if (metrics->motion_detected || snr_db < MIN_SNR_DB_FAIR || pi < MIN_PI_GOOD) {
		metrics->quality = PPG_QUALITY_POOR;
	} else if (snr_db < MIN_SNR_DB_GOOD || pi < MIN_PI_EXCELLENT) {
		metrics->quality = PPG_QUALITY_FAIR;
	} else if (snr_db < MIN_SNR_DB_EXCELLENT) {
		metrics->quality = PPG_QUALITY_GOOD;
	} else {
		metrics->quality = PPG_QUALITY_EXCELLENT;
	}

	/* Downgrade one step if skewness SQI is near-zero / negative (Elgendi) */
	if (metrics->skewness_x10 < 5 && metrics->quality > PPG_QUALITY_NO_SIGNAL) {
		metrics->quality = (enum ppg_signal_quality)(metrics->quality - 1);
	}

	LOG_DBG("Quality: %u, SNR: %u.%u dB, PI: %u.%u%%, skew×10=%d, Motion: %s (%u mg)",
		metrics->quality,
		metrics->snr_db_x10 / 10, metrics->snr_db_x10 % 10,
		metrics->perfusion_index_x10 / 10, metrics->perfusion_index_x10 % 10,
		metrics->skewness_x10,
		metrics->motion_detected ? "YES" : "NO",
		metrics->motion_magnitude);
}
