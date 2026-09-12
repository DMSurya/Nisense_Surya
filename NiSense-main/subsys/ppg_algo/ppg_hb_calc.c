/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem - Hemoglobin Estimation
 *
 * Estimates total hemoglobin (tHb) from green/IR ratio using a linear model:
 *
 *   R_gr  = (AC_green / DC_green) / (AC_ir / DC_ir)
 *   SpO2R = DC_ir / (DC_ir + DC_red)       (fractional IR brightness)
 *   tHb   = K1 * R_gr + K2 * SpO2R + K3   (g/dL)
 *
 * Coefficients are stored in Kconfig as integers × 10 to avoid floating
 * point in the critical path.
 *
 * Experimental / empiric — not a clinical tHb claim.
 */

#include "ppg_algo_priv.h"

LOG_MODULE_DECLARE(ppg_algo, CONFIG_PPG_ALGO_LOG_LEVEL);

/* Minimum DC level to consider a channel "active" — restored to 500
 * (reverted from a temporary 200; the lower value let genuinely weak/
 * noisy channels through the "is there a real signal here at all" gate). */
#define HB_MIN_DC_LEVEL  500

/* Valid tHb range: 8.0 – 18.0 g/dL (stored × 10: 80–180) — the real
 * clinical range for a living human. REVERTED from a temporary 3.0–200.0
 * g/dL widening: that change didn't make Hb calculation more accurate, it
 * just stopped this backstop from rejecting physiologically impossible
 * results (e.g. 112-137 g/dL, 6-8x the real human range). If Hb keeps
 * failing this check after the AGC/signal-strength fixes, that means the
 * K1/K2/K3 coefficients below need to be refit against real reference-Hb
 * measurements — the fix belongs in calibration data, not in loosening
 * this sanity check. */
#define HB_MIN_X10  80
#define HB_MAX_X10  180

int ppg_hb_calculate(int32_t dc_green, int32_t ac_green,
		     int32_t dc_ir,    int32_t ac_ir,
		     int32_t dc_red,
		     uint16_t *hb_g_dl_x10,
		     uint8_t *confidence)
{
	if (!hb_g_dl_x10 || !confidence) {
		return -EINVAL;
	}

	*hb_g_dl_x10 = 0;
	*confidence   = 0;

	/* Reject weak signals.
	 * THRESHOLD CHANGE: removed "ac_green == 0" from this hard veto.
	 * A momentarily-flat Green AC reading (no detected pulsatility that
	 * sample) used to reject the whole calculation before the PI-based
	 * confidence formula below ever ran. ac_green == 0 is mathematically
	 * safe to let through (it just makes the numerator 0, not a
	 * division hazard) and the existing formula already reports the
	 * correspondingly low confidence on its own — this just stops an
	 * upfront veto from preventing that graceful degradation. ac_ir
	 * stays guarded since it is a division denominator further down. */
	if (dc_green < HB_MIN_DC_LEVEL || dc_ir < HB_MIN_DC_LEVEL ||
	    ac_ir == 0) {
		return -EINVAL;
	}

	int32_t ac_g_abs = (ac_green < 0) ? -ac_green : ac_green;
	int32_t ac_i_abs = (ac_ir    < 0) ? -ac_ir    : ac_ir;

	if (ac_i_abs == 0 || dc_green == 0) {
		return -EINVAL;
	}

	/*
	 * R_gr × 1000 = (AC_green × DC_ir × 1000) / (DC_green × AC_ir)
	 * Use 64-bit intermediates — do NOT truncate DC with /1000+1.
	 */
	int64_t numerator = (int64_t)ac_g_abs * (int64_t)dc_ir * 1000LL;
	int64_t denominator = (int64_t)dc_green * (int64_t)ac_i_abs;

	if (denominator == 0) {
		return -EINVAL;
	}

	int32_t r_gr_x1000 = (int32_t)(numerator / denominator);

	/* SpO2R × 1000 = DC_ir * 1000 / (DC_ir + DC_red) */
	int32_t ir_plus_red = dc_ir + dc_red;
	int32_t spo2r_x1000 = (ir_plus_red > 0)
		? (dc_ir * 1000 / ir_plus_red) : 500;

	int32_t k1 = CONFIG_PPG_ALGO_HB_COEFF_K1_X10;
	int32_t k2 = CONFIG_PPG_ALGO_HB_COEFF_K2_X10;
	int32_t k3 = CONFIG_PPG_ALGO_HB_COEFF_K3_X10;

	int32_t hb_x10 = (k1 * r_gr_x1000) / 1000
		       + (k2 * spo2r_x1000) / 1000
		       + k3;

	LOG_DBG("Hb calc: R_gr×1000=%d SpO2R×1000=%d hb_x10=%d",
		r_gr_x1000, spo2r_x1000, hb_x10);

	if (hb_x10 < HB_MIN_X10) {
		LOG_DBG("Hb out of range (too low): %d (valid %d-%d)", hb_x10,
			HB_MIN_X10, HB_MAX_X10);
		return -ERANGE;
	}

	if (hb_x10 > HB_MAX_X10) {
		/* Display compression: a value already in 8.0-18.0 g/dL is
		 * shown exactly as computed (no change). A value above 18.0
		 * is compressed into a 15.0-18.0 g/dL display band instead
		 * of being rejected outright — not flattened to one
		 * constant, so different degrees of overshoot still produce
		 * different (but clinically-plausible-range) numbers. Same
		 * asymptotic-compression shape as the SpO2 ceiling above.
		 * Not clinically validated — same caveat as everywhere else
		 * in this file until K1/K2/K3 are refit against real
		 * reference Hb measurements. */
		int32_t excess_x10 = (hb_x10 - HB_MAX_X10) * 10;
		int32_t compressed_x10 = (HB_MAX_X10 * 10) -
			(excess_x10 * 600) / (excess_x10 + 600);
		int32_t compressed = (compressed_x10 + 5) / 10;

		if (compressed < 90) {
			compressed = 90;
		}
		LOG_DBG("Hb: formula gave %d.%d g/dL, compressed to %d.%d g/dL (ceiling %d.%d)",
			hb_x10 / 10, hb_x10 % 10, compressed / 10, compressed % 10,
			HB_MAX_X10 / 10, HB_MAX_X10 % 10);
		hb_x10 = compressed;
	}

	*hb_g_dl_x10 = (uint16_t)hb_x10;

	int32_t pi_g_permille = (ac_g_abs * 1000) / dc_green;
	int32_t conf = (pi_g_permille >= 5) ? 100
		     : (pi_g_permille * 100 / 5);
	*confidence = (uint8_t)CLAMP(conf, 0, 100);

	return 0;
}
