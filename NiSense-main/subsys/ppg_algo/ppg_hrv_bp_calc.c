/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem — HRV (SDNN/RMSSD) and cuffless BP estimate.
 */

#include "ppg_algo_priv.h"

LOG_MODULE_DECLARE(ppg_algo, CONFIG_PPG_ALGO_LOG_LEVEL);

/*
 * BUGFIX (part of "SDNN_ms/RMSSD_ms always 0"): 20 RR intervals in a
 * fixed 20s acquisition window requires HR > ~66 bpm with zero missed
 * beats — unreachable for a large share of normal resting heart rates
 * even on a clean signal (e.g. the exported patient session: 62 bpm,
 * ~20 beats possible at best, so one missed detection anywhere fails
 * the gate). At the lower end of the HR range this code already treats
 * as valid elsewhere (40 bpm, see ppg_bp_estimate's range), a 20s
 * window physically contains only ~13 beats — 20 would be provably
 * impossible to reach, not just unlikely. Lowering to 10 keeps this a
 * short-window estimate (real full SDNN/RMSSD reference practice wants
 * several minutes; this is not that) while making it achievable across
 * the HR range the device is meant to support. ppg_algo.c also now has
 * a spreadsheet-method fallback for whatever this still misses.
 */
#define HRV_MIN_RR_INTERVALS 10

/* Integer square root (floor) for variance → stddev without float. */
static uint32_t isqrt_u32(uint32_t n)
{
	uint32_t x = n;
	uint32_t y = 0U;
	uint32_t b = 1U << 30;

	while (b > x) {
		b >>= 2;
	}
	while (b != 0U) {
		if (x >= y + b) {
			x -= y + b;
			y = (y >> 1) + b;
		} else {
			y >>= 1;
		}
		b >>= 2;
	}
	return y;
}

int ppg_hrv_calculate(const struct ppg_peak_state *state,
		      uint16_t *sdnn_ms,
		      uint16_t *rmssd_ms)
{
	if (!state || !sdnn_ms || !rmssd_ms) {
		return -EINVAL;
	}

	*sdnn_ms = 0;
	*rmssd_ms = 0;

	if (state->rr_count < HRV_MIN_RR_INTERVALS) {
		return -ENODATA;
	}

	uint8_t n = state->rr_count;
	uint32_t sum = 0U;

	for (uint8_t i = 0; i < n; i++) {
		sum += state->rr_intervals[i];
	}

	uint32_t mean = sum / n;
	uint64_t var_acc = 0ULL;

	for (uint8_t i = 0; i < n; i++) {
		int32_t d = (int32_t)state->rr_intervals[i] - (int32_t)mean;

		var_acc += (uint64_t)((int64_t)d * (int64_t)d);
	}

	*sdnn_ms = (uint16_t)isqrt_u32((uint32_t)(var_acc / n));

	/* RMSSD = sqrt(mean of squared successive RR differences) */
	uint64_t succ_acc = 0ULL;
	uint8_t succ_n = (uint8_t)(n - 1U);

	for (uint8_t i = 1; i < n; i++) {
		int32_t d = (int32_t)state->rr_intervals[i] -
			    (int32_t)state->rr_intervals[i - 1U];

		succ_acc += (uint64_t)((int64_t)d * (int64_t)d);
	}

	*rmssd_ms = (uint16_t)isqrt_u32((uint32_t)(succ_acc / succ_n));

	LOG_DBG("HRV: SDNN=%u ms RMSSD=%u ms (n=%u)",
		*sdnn_ms, *rmssd_ms, n);

	return 0;
}

int ppg_bp_estimate(uint16_t hr_bpm,
		    uint32_t ac_ir, uint32_t dc_ir,
		    uint32_t ac_red, uint32_t dc_red,
		    uint16_t *systolic_mmhg,
		    uint16_t *diastolic_mmhg)
{
	if (!systolic_mmhg || !diastolic_mmhg) {
		return -EINVAL;
	}

	*systolic_mmhg = 0;
	*diastolic_mmhg = 0;

	if (hr_bpm < 40U || hr_bpm > 180U || dc_ir < 500U || ac_ir == 0U) {
		return -EINVAL;
	}

	/* Pulse amplitude ratios ×1000 (perfusion-like morphology features). */
	uint32_t par_ir = (ac_ir * 1000U) / dc_ir;
	uint32_t par_red = 0U;

	if (dc_red >= 500U && ac_red > 0U) {
		par_red = (ac_red * 1000U) / dc_red;
	}

	/* Blend IR/Red PAR; prefer IR when red is weak. */
	uint32_t par = (par_red > 0U) ? ((par_ir * 2U + par_red) / 3U) : par_ir;

	/* Clamp PAR to a plausible wearable range. */
	if (par < 5U) {
		par = 5U;
	} else if (par > 80U) {
		par = 80U;
	}

	/*
	 * Empiric cuffless estimate (NOT clinical-grade):
	 *   MAP ≈ 75 + HR/4 + PAR/2
	 *   PP  ≈ 30 + PAR/2 − |HR−70|/5
	 *   SYS = MAP + 2·PP/3
	 *   DIA = MAP − PP/3
	 */
	int32_t hr_off = (int32_t)hr_bpm - 70;
	int32_t hr_abs = (hr_off < 0) ? -hr_off : hr_off;

	int32_t map = 75 + ((int32_t)hr_bpm / 4) + ((int32_t)par / 2);
	int32_t pp = 30 + ((int32_t)par / 2) - (hr_abs / 5);

	if (pp < 20) {
		pp = 20;
	} else if (pp > 70) {
		pp = 70;
	}

	int32_t sys = map + (pp * 2) / 3;
	int32_t dia = map - pp / 3;

	if (sys < 85) {
		sys = 85;
	} else if (sys > 180) {
		sys = 180;
	}
	if (dia < 50) {
		dia = 50;
	} else if (dia > 110) {
		dia = 110;
	}
	if (dia >= sys) {
		dia = sys - 20;
		if (dia < 50) {
			dia = 50;
		}
	}

	*systolic_mmhg = (uint16_t)sys;
	*diastolic_mmhg = (uint16_t)dia;

	LOG_DBG("BP est: %u/%u mmHg (HR=%u PAR=%u)",
		*systolic_mmhg, *diastolic_mmhg, hr_bpm, par);

	return 0;
}
