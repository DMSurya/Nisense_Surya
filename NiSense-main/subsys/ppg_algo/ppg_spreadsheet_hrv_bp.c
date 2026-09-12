/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem — Spreadsheet-method HR / HRV / BP
 *
 * Faithful port of formulas_of_Miisky.xlsx ("UPDATED FORMULAS" sheet) +
 * formula_in_each_column.doc (fills in AR/AX-BC gaps), as an alternative
 * to the windowed-DC/AC approach in ppg_algo.c / ppg_hrv_bp_calc.c. Runs
 * on the first PPG_SS_SAMPLE_COUNT (300) of the up-to-500 collected
 * samples, per explicit instruction.
 *
 * Column mapping confirmed against both source documents:
 *   L,M,N  = running (expanding-window) mean of IR/Red/Green from sample 0
 *   O,P,Q  = running (expanding-window) peak-to-peak range of IR/Red/Green
 *   R,S,T  = O/L, P/M, Q/N            (per-channel AC/DC ratio)
 *   AG,AF  = O/L, P/M                 (IR_NORM, RED_NORM — same as R,S)
 *   AI     = 0.4*S + 0.4*AG + 0.2*AH  (composite "FUSION PPG" signal;
 *                                      AH/GREEN_NORM is defined as 0 in
 *                                      the doc, so green's weight
 *                                      contributes 0 — kept faithful to
 *                                      that even though it looks like a
 *                                      possible gap in their own sheet)
 *   AJ     = local-maximum peak flag on AI
 *   AL/AM/AN = peak time / previous peak time / RR interval (AL-AM)
 *   AO     = AI value at the peak ("PEAK_VALUE")
 *   AP     = MIN(AI) between this peak and the next ("VALLEY_VALUE")
 *   AQ     = AO - AP                  (PULSE_AMPLITUDE)
 *   AR     = AQ / AP                  (NORMALISED_PULSE — confirmed by
 *                                      formula_in_each_column.doc; this
 *                                      is what BP actually uses, NOT an
 *                                      RR-interval ratio)
 *   BJ/BK  = 1.03*AR+4 / 1.02*AR+3    (systolic / diastolic BP)
 *   BA/BB/BC/BD/BE = RR_CLEAN / RR_DIFF / RR_DIFF² / RMSSD / SDNN
 *                     (SDNN = STDEV.S of raw RR intervals; RMSSD =
 *                     sqrt(mean of squared successive RR differences))
 *   CE     = HR = 60 / mean(RR interval, seconds)
 *
 * NOTE ON UNITS: same caveat as before — the reference spreadsheet's own
 * example output is not confirmed to be on a real ms/mmHg scale. This
 * port is structurally faithful to the formulas above and uses real
 * elapsed time (capture_dt_ms) rather than an assumed fixed sample
 * period, but does not invent a scale factor beyond what the formulas
 * themselves define. Treat outputs as experimental until verified by
 * hand against the spreadsheet (see the RTT worked-example log below)
 * and against real reference measurements.
 *
 * ALSO NOTE: BI/HB = MEDIAN(AA3:AE302) in the sheet spans a rectangular
 * block across 5 *differently-defined* columns (AVG_OD_RED, AVG_OD_GREEN,
 * BASELINE, OD_RED-ODIR, 5-moving-avg) and 300 rows — mixing columns with
 * different meanings into one MEDIAN() doesn't have a clear physical
 * interpretation, and looks like it may be a typo in the original sheet
 * (e.g. meant to be a single column like AC3:AC302). This module does
 * NOT implement BI/HB for that reason — flagging it for confirmation
 * rather than guessing at a fix.
 */

#include "ppg_algo_priv.h"
#include <stdlib.h>

LOG_MODULE_DECLARE(ppg_algo, CONFIG_PPG_ALGO_LOG_LEVEL);

#define PPG_SS_SAMPLE_COUNT 300U /* per explicit instruction: 300 of the 500 */
#define PPG_SS_MIN_PEAKS    3U   /* need at least a couple of intervals */

/* isqrt via Newton's method on integers. */
static uint32_t ppg_ss_isqrt(uint64_t n)
{
	if (n == 0ULL) {
		return 0U;
	}
	uint64_t x = n;
	uint64_t y = (x + 1U) / 2U;

	while (y < x) {
		x = y;
		y = (x + n / x) / 2U;
	}
	return (uint32_t)x;
}

int ppg_spreadsheet_calculate(const struct ppg_sample_buffer *buffer,
			      struct ppg_ss_result *out)
{
	if (!buffer || !out || !buffer->ir || !buffer->red || !buffer->green) {
		return -EINVAL;
	}

	memset(out, 0, sizeof(*out));

	uint16_t n = (buffer->count < PPG_SS_SAMPLE_COUNT) ?
		buffer->count : PPG_SS_SAMPLE_COUNT;

	if (n < 10U) {
		return -ENODATA; /* not enough samples to be meaningful */
	}

	int64_t sum_ir = 0, sum_red = 0, sum_green = 0;
	uint32_t min_ir = UINT32_MAX, max_ir = 0;
	uint32_t min_red = UINT32_MAX, max_red = 0;
	uint32_t min_green = UINT32_MAX, max_green = 0;

	int32_t *ai = k_malloc(n * sizeof(int32_t));

	if (!ai) {
		return -ENOMEM;
	}

	uint16_t nominal_dt_ms = (uint16_t)(1000U / CONFIG_PPG_ALGO_SAMPLE_RATE);

	for (uint16_t i = 0; i < n; i++) {
		sum_ir += buffer->ir[i];
		sum_red += buffer->red[i];
		sum_green += buffer->green[i];

		if (buffer->ir[i] < min_ir) min_ir = buffer->ir[i];
		if (buffer->ir[i] > max_ir) max_ir = buffer->ir[i];
		if (buffer->red[i] < min_red) min_red = buffer->red[i];
		if (buffer->red[i] > max_red) max_red = buffer->red[i];
		if (buffer->green[i] < min_green) min_green = buffer->green[i];
		if (buffer->green[i] > max_green) max_green = buffer->green[i];

		int64_t dc_ir = sum_ir / (int64_t)(i + 1U);
		int64_t dc_red = sum_red / (int64_t)(i + 1U);
		uint32_t ac_ir = max_ir - min_ir;
		uint32_t ac_red = max_red - min_red;

		/* AG,S = IR/Red AC/DC ratio x1000. GREEN_NORM (AH) is defined
		 * as a literal 0 in the doc, so it contributes nothing to AI
		 * below — kept faithful to that rather than substituting a
		 * real green ratio the sheet doesn't actually use here. */
		int32_t ratio_ir_x1000 = (dc_ir > 0) ?
			(int32_t)((int64_t)ac_ir * 1000 / dc_ir) : 0;
		int32_t ratio_red_x1000 = (dc_red > 0) ?
			(int32_t)((int64_t)ac_red * 1000 / dc_red) : 0;

		/* AI = 0.4*S + 0.4*AG + 0.2*AH(=0) */
		ai[i] = (int32_t)((400LL * ratio_red_x1000 +
				   400LL * ratio_ir_x1000) / 1000LL);
	}

	/* Peak detection: AJ = local maximum of AI vs immediate neighbours. */
	uint16_t peak_idx[PPG_SS_SAMPLE_COUNT];
	uint16_t peak_count = 0;

	for (uint16_t i = 1; i < n - 1U; i++) {
		if (ai[i] > ai[i - 1U] && ai[i] > ai[i + 1U]) {
			peak_idx[peak_count++] = i;
		}
	}

	if (peak_count < PPG_SS_MIN_PEAKS) {
		k_free(ai);
		return -ENODATA;
	}

	/* AL/AM/AN: peak-to-peak RR intervals in ms, using real per-sample
	 * elapsed time when captured, else the nominal sample period. */
	uint32_t intervals_ms[PPG_SS_SAMPLE_COUNT];
	uint16_t interval_count = peak_count - 1U;

	for (uint16_t k = 0; k < interval_count; k++) {
		uint16_t i0 = peak_idx[k];
		uint16_t i1 = peak_idx[k + 1U];

		if (buffer->capture_dt_ms) {
			intervals_ms[k] = buffer->capture_dt_ms[i1] -
					  buffer->capture_dt_ms[i0];
		} else {
			intervals_ms[k] = (uint32_t)(i1 - i0) * nominal_dt_ms;
		}
	}

	/* AO/AP/AQ/AR: peak value, valley (min AI between this peak and the
	 * next), pulse amplitude, and normalized pulse — averaged across all
	 * detected pulses in this window for one BP estimate per measurement. */
	int64_t sum_ar_x1000 = 0;
	uint16_t ar_count = 0;

	for (uint16_t k = 0; k < interval_count; k++) {
		uint16_t i0 = peak_idx[k];
		uint16_t i1 = peak_idx[k + 1U];
		int32_t peak_val = ai[i0];
		int32_t valley_val = ai[i0];

		for (uint16_t j = i0; j <= i1; j++) {
			if (ai[j] < valley_val) {
				valley_val = ai[j];
			}
		}

		if (valley_val != 0) {
			int32_t pulse_amp = peak_val - valley_val;
			int32_t ar_x1000 = (int32_t)(((int64_t)pulse_amp * 1000) / valley_val);

			sum_ar_x1000 += ar_x1000;
			ar_count++;
		}
	}

	k_free(ai);

	/* HR = 60000 / mean interval (ms). */
	uint64_t sum_interval = 0;

	for (uint16_t k = 0; k < interval_count; k++) {
		sum_interval += intervals_ms[k];
	}
	uint32_t mean_interval_ms = (uint32_t)(sum_interval / interval_count);

	out->hr_bpm = (mean_interval_ms > 0) ?
		(uint16_t)(60000U / mean_interval_ms) : 0U;

	/* SDNN = STDEV.S of raw RR intervals (ms); RMSSD = sqrt(mean of
	 * squared successive RR differences) — matches BE/BD in the sheet. */
	int64_t sum_dev_sq = 0;

	for (uint16_t k = 0; k < interval_count; k++) {
		int64_t d = (int64_t)intervals_ms[k] - (int64_t)mean_interval_ms;

		sum_dev_sq += d * d;
	}
	uint32_t sdnn_ms = (interval_count > 1U) ?
		ppg_ss_isqrt((uint64_t)(sum_dev_sq / (interval_count - 1U))) : 0U;

	int64_t sum_succ_sq = 0;
	uint16_t succ_n = (interval_count > 0U) ? (interval_count - 1U) : 0U;

	for (uint16_t k = 1; k < interval_count; k++) {
		int64_t d = (int64_t)intervals_ms[k] - (int64_t)intervals_ms[k - 1U];

		sum_succ_sq += d * d;
	}
	uint32_t rmssd_ms = (succ_n > 0U) ?
		ppg_ss_isqrt((uint64_t)(sum_succ_sq / succ_n)) : 0U;

	out->sdnn_x100 = (uint16_t)MIN(sdnn_ms * 100U, UINT16_MAX);
	out->rmssd_x100 = (uint16_t)MIN(rmssd_ms * 100U, UINT16_MAX);

	/* BP = linear function of AR (normalized pulse amplitude), averaged
	 * across all detected pulses in this window — confirmed formula:
	 * systolic = 1.03*AR + 4, diastolic = 1.02*AR + 3. */
	if (ar_count > 0U) {
		int32_t ar_mean_x1000 = (int32_t)(sum_ar_x1000 / ar_count);

		out->systolic_x100 = (uint16_t)CLAMP(
			(103 * ar_mean_x1000 / 1000) + 400, 0, UINT16_MAX);
		out->diastolic_x100 = (uint16_t)CLAMP(
			(102 * ar_mean_x1000 / 1000) + 300, 0, UINT16_MAX);
	}

	out->peak_count = peak_count;
	out->valid = true;

	/* Debug: one worked example — first 5 raw samples that went in, and
	 * the final computed result, so this can be checked by hand against
	 * the spreadsheet. */
	LOG_INF("SS-ALGO worked example (first %u of %u samples used):",
		n, buffer->count);
	for (uint16_t i = 0; i < MIN(5U, n); i++) {
		LOG_INF("  sample[%u]: IR=%u Red=%u Green=%u",
			i, buffer->ir[i], buffer->red[i], buffer->green[i]);
	}
	LOG_INF("  -> peaks=%u HR=%u bpm SDNN=%u.%02u RMSSD=%u.%02u "
		"Sys=%u.%02u Dia=%u.%02u (AR-based, %u pulses averaged)",
		out->peak_count, out->hr_bpm,
		out->sdnn_x100 / 100U, out->sdnn_x100 % 100U,
		out->rmssd_x100 / 100U, out->rmssd_x100 % 100U,
		out->systolic_x100 / 100U, out->systolic_x100 % 100U,
		out->diastolic_x100 / 100U, out->diastolic_x100 % 100U,
		ar_count);

	return 0;
}
