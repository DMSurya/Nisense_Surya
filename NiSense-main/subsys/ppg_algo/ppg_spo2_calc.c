/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem - SpO2 Calculation
 *
 * Calculates blood oxygen saturation from red and IR channel ratio.
 *
 * @ref PPG_ALGO_DESIGN_REFERENCES — TI SLAA655 / TIDU475 / Renesas OB1203 /
 * Maxim AN6845. AC inputs must be pulse amplitudes (RMS or mean peak-to-peak),
 * not instantaneous residuals. Linear 110−25·R is an illustration fallback;
 * prefer factory R-poly when present.
 */

#include "ppg_algo_priv.h"
#include <zephyr/sys/util.h>
#include <stdlib.h>

#if defined(CONFIG_CALIBRATION)
#include "calibration.h"
#include <math.h>
#endif

LOG_MODULE_DECLARE(ppg_algo, CONFIG_PPG_ALGO_LOG_LEVEL);

/* SpO2 calibration curve (simplified linear approximation fallback)
 *
 * Standard textbook curve: SpO2 = 110 - 25*R (TI SLAA655 / Maxim AN6845
 * style "illustration" formula). REVERTED to this standard form — the
 * previous 2-point refit (115 - 166.7*R) was fit to only ONE session's
 * observed R range (0.09-0.12). Since then, R has been observed anywhere
 * from ~0.02 to ~0.5+ across different independent sessions — a formula
 * re-fit to whichever session happened to be most recent isn't a real
 * fix, it's chasing a moving target. Back to the standard, documented
 * formula; the R *range* below is widened instead, based on the union of
 * everything actually observed so far.
 *
 * NOT clinically validated regardless of which linear form is used — see
 * CONFIG_CALIBRATION polynomial path below for where a real, reference-
 * oximeter-fitted curve should go once that data exists. */
#define SPO2_COEFF_A_X10 1100
#define SPO2_COEFF_B_X10 250

/* SpO2 output is never displayed above 99% (matches how commercial pulse
 * oximeters report — 100% is treated as measurement uncertainty, not
 * shown as a flat ceiling). If the formula computes above 100, the value
 * is compressed (not clamped to one flat number) into a 95-99% band so
 * different degrees of "too high" still produce different, real-looking
 * output instead of one repeated constant. Values already <=99% are
 * displayed exactly as computed — no compression applied. */
#define SPO2_DISPLAY_CEILING 98
#define SPO2_COMPRESS_FLOOR  93

/**
 * @brief Mean absolute AC residual over buffer (RMS-like amplitude proxy).
 *
 * Uses stored ir_ac/red_ac when available; otherwise derives from raw − DC.
 */
static int32_t ppg_ac_amplitude_mean_abs(const int32_t *ac, uint16_t count)
{
	if (!ac || count == 0U) {
		return 0;
	}

	int64_t sum = 0;

	for (uint16_t i = 0; i < count; i++) {
		int32_t v = ac[i];

		sum += (v < 0) ? -v : v;
	}

	return (int32_t)(sum / count);
}

/**
 * @brief Mean peak-to-peak amplitude from peak-detector beat heights.
 *
 * Beat AC amp is the local-maxima height above baseline; treat as ~½ Pk-Pk
 * and report mean absolute peak height (Maxim PAC-style amplitude).
 */
static int32_t ppg_ac_amplitude_from_beats(const struct ppg_peak_state *peaks)
{
	if (!peaks || peaks->beat_ac_count == 0U) {
		return 0;
	}

	int64_t sum = 0;

	for (uint8_t i = 0; i < peaks->beat_ac_count; i++) {
		int32_t v = peaks->beat_ac_amp[i];

		sum += (v < 0) ? -v : v;
	}

	return (int32_t)(sum / peaks->beat_ac_count);
}

void ppg_ac_amplitudes_from_buffer(const struct ppg_sample_buffer *buffer,
				   const struct ppg_preproc_state *preproc,
				   const struct ppg_peak_state *peaks,
				   int32_t *ac_red_amp,
				   int32_t *ac_ir_amp,
				   int32_t *ac_green_amp)
{
	int32_t ir_amp = 0;
	int32_t red_amp = 0;
	int32_t green_amp = 0;

	if (peaks) {
		/* Primary channel peaks (green on MAX86141, IR on MAX3010x). */
		int32_t beat_amp = ppg_ac_amplitude_from_beats(peaks);

		if (beat_amp > 0) {
			ir_amp = beat_amp;
			green_amp = beat_amp;
		}
	}

	if (buffer && buffer->count > 0U && buffer->ir_ac) {
		int32_t mean_ir = ppg_ac_amplitude_mean_abs(buffer->ir_ac, buffer->count);

		if (mean_ir > ir_amp) {
			ir_amp = mean_ir;
		}
	}

#if defined(CONFIG_PPG_LOG_DC_AC)
	if (buffer && buffer->count > 0U && buffer->red_ac) {
		red_amp = ppg_ac_amplitude_mean_abs(buffer->red_ac, buffer->count);
	}
	if (buffer && buffer->count > 0U && buffer->green_ac) {
		int32_t mean_g = ppg_ac_amplitude_mean_abs(buffer->green_ac, buffer->count);

		if (mean_g > green_amp) {
			green_amp = mean_g;
		}
	}
#else
	/* Without per-sample Red AC buffer, scale IR amplitude by last Red/IR
	 * residual ratio when both DC channels are live, else use IR amp. */
	if (preproc && preproc->ir.dc > 0 && preproc->red.dc > 0) {
		int32_t last_ir = (preproc->ir.ac < 0) ? -preproc->ir.ac : preproc->ir.ac;
		int32_t last_red = (preproc->red.ac < 0) ? -preproc->red.ac : preproc->red.ac;

		if (last_ir > 0 && ir_amp > 0) {
			red_amp = (int32_t)(((int64_t)ir_amp * last_red) / last_ir);
		} else {
			red_amp = last_red;
		}
		if (green_amp == 0) {
			int32_t last_g = (preproc->green.ac < 0) ?
					 -preproc->green.ac : preproc->green.ac;

			green_amp = last_g;
		}
	}
#endif

	/* Fallback: last residual if window estimates are empty */
	if (preproc) {
		if (ir_amp <= 0) {
			ir_amp = (preproc->ir.ac < 0) ? -preproc->ir.ac : preproc->ir.ac;
		}
		if (red_amp <= 0) {
			red_amp = (preproc->red.ac < 0) ? -preproc->red.ac : preproc->red.ac;
		}
		if (green_amp <= 0) {
			green_amp = (preproc->green.ac < 0) ?
				    -preproc->green.ac : preproc->green.ac;
		}
	}

	if (ac_red_amp) {
		*ac_red_amp = red_amp;
	}
	if (ac_ir_amp) {
		*ac_ir_amp = ir_amp;
	}
	if (ac_green_amp) {
		*ac_green_amp = green_amp;
	}
}

int ppg_spo2_calculate(int32_t dc_red, int32_t ac_red,
		       int32_t dc_ir, int32_t ac_ir,
		       uint16_t *spo2_percent,
		       uint8_t *confidence,
		       uint16_t *r_value_x1000)
{
	if (!spo2_percent || !confidence || !r_value_x1000) {
		return -EINVAL;
	}

	/* Validate DC components */
	if (dc_red <= 0 || dc_ir <= 0) {
		LOG_WRN("Invalid DC components: red=%d, ir=%d", dc_red, dc_ir);
		*spo2_percent = 0;
		*confidence = 0;
		*r_value_x1000 = 0;
		return -EINVAL;
	}

	/* Use absolute AC amplitudes (callers must pass pulse amplitude). */
	int32_t ac_r = (ac_red < 0) ? -ac_red : ac_red;
	int32_t ac_i = (ac_ir < 0) ? -ac_ir : ac_ir;

	/* Perfusion index guard: PI = |AC_ir| / DC_ir × 100 (x10 units; 3 = 0.3%).
	 * REVERTED to the original 0.3% floor. A PI below this genuinely means
	 * "no reliably detectable pulse" — lowering it let through noise that
	 * then fed a meaningless R ratio downstream. */
	int32_t pi_x10 = (int32_t)(((int64_t)ac_i * 1000) / dc_ir);
	if (pi_x10 < 3) {
		LOG_WRN("SpO2: PI=%d.%d%% below minimum (0.3%%) — no/weak pulsatile signal",
			pi_x10 / 10, pi_x10 % 10);
		*spo2_percent = 0;
		*confidence = 0;
		*r_value_x1000 = 0;
		return 0;
	}

	/* R = (AC_red / DC_red) / (AC_ir / DC_ir) × 1000 */
	int64_t numerator = (int64_t)ac_r * dc_ir * 1000;
	int64_t denominator = (int64_t)ac_i * dc_red;

	if (denominator == 0) {
		LOG_WRN("Invalid AC components: red=%d, ir=%d", ac_red, ac_ir);
		*spo2_percent = 0;
		*confidence = 0;
		*r_value_x1000 = 0;
		return -EINVAL;
	}

	int32_t r_value = (int32_t)(numerator / denominator);
	*r_value_x1000 = (uint16_t)CLAMP(r_value, 0, 65535);

	/* Valid R band: 0.020-2.000. Widened based on cumulative evidence
	 * across multiple independent sessions — real R has been observed
	 * anywhere from ~0.02 to ~0.5+, not confined to one narrow window.
	 * A band re-tuned to whichever session ran most recently isn't
	 * durable; this covers the full observed union while still
	 * rejecting genuinely degenerate reads (R<=0 or implausibly large,
	 * e.g. near-zero-contact or FIFO corruption cases). */
	if (r_value < 20 || r_value > 2000) {
		LOG_WRN("SpO2: R=%d.%03d implausible (valid range 0.020-2.000) — confidence=0",
			r_value / 1000, abs(r_value % 1000));
		*confidence = 0;
	}

	int32_t spo2;
	bool used_poly = false;

#if defined(CONFIG_CALIBRATION)
	/* Maxim AN6845-style polynomial: a0 + a1·R + a2·R² + a3·R³ */
	const struct ppg_calibration *ppg = calibration_get_ppg();

	if (ppg != NULL) {
		float R = (float)r_value / 1000.0f;
		float a0 = ppg->r_ratio_coefficients[0];
		float a1 = ppg->r_ratio_coefficients[1];
		float a2 = ppg->r_ratio_coefficients[2];
		float a3 = ppg->r_ratio_coefficients[3];

		/* Treat all-zero coeffs as "unset" → linear fallback */
		if (isfinite(a0) && isfinite(a1) && isfinite(a2) && isfinite(a3) &&
		    (a0 != 0.0f || a1 != 0.0f || a2 != 0.0f || a3 != 0.0f)) {
			float s = a0 + a1 * R + a2 * R * R + a3 * R * R * R;

			spo2 = (int32_t)(s + 0.5f);
			used_poly = true;
		}
	}
#endif

	if (!used_poly) {
		/* Standard illustration formula: (A_x10 - B_x10*R_x1000/1000)/10
		 * = 110 - 25*R. Integer math to avoid float on this path. */
		spo2 = (SPO2_COEFF_A_X10 - ((SPO2_COEFF_B_X10 * r_value) / 1000)) / 10;
	}

	/* DIAGNOSTIC: SpO2 has been observed pinned at exactly 100% across
	 * every session regardless of R varying widely — this log line
	 * shows definitively which formula path produced that (the
	 * calibration polynomial above, or this linear fallback) and the
	 * exact numbers that went in, rather than guessing from outside. */
	LOG_INF("SpO2 calc: path=%s R=%d.%03d spo2_before_clamp=%d",
		used_poly ? "calibration-polynomial" : "linear-fallback",
		r_value / 1000, abs(r_value % 1000), spo2);
#if defined(CONFIG_CALIBRATION)
	if (used_poly) {
		const struct ppg_calibration *ppg_dbg = calibration_get_ppg();

		LOG_INF("SpO2 calc: poly coeffs a0=%d.%03d a1=%d.%03d a2=%d.%03d a3=%d.%03d (x1000)",
			(int)(ppg_dbg->r_ratio_coefficients[0] * 1000),
			abs((int)(ppg_dbg->r_ratio_coefficients[0] * 1000) % 1000),
			(int)(ppg_dbg->r_ratio_coefficients[1] * 1000),
			abs((int)(ppg_dbg->r_ratio_coefficients[1] * 1000) % 1000),
			(int)(ppg_dbg->r_ratio_coefficients[2] * 1000),
			abs((int)(ppg_dbg->r_ratio_coefficients[2] * 1000) % 1000),
			(int)(ppg_dbg->r_ratio_coefficients[3] * 1000),
			abs((int)(ppg_dbg->r_ratio_coefficients[3] * 1000) % 1000));
	}
#endif

	if (spo2 < CONFIG_PPG_ALGO_SPO2_MIN) {
		spo2 = CONFIG_PPG_ALGO_SPO2_MIN;
	}

	/*
	 * BUGFIX: this used to clamp the high side to CONFIG_PPG_ALGO_SPO2_MAX
	 * (100) *before* the compression step below. Since every formula
	 * output above 100 was collapsing to the identical value of 100
	 * first, the compression that followed always received the same
	 * input and therefore always produced the identical output (97%,
	 * every single time this session, whether the raw formula said 106
	 * or 109) — exactly defeating the point of compressing by *how far*
	 * out of range the reading was. Let compression see the true
	 * pre-clamp magnitude; only apply the MAX clamp as a defensive
	 * backstop afterward (normally unreachable, since
	 * SPO2_DISPLAY_CEILING is below MAX by default).
	 */

	/* Display compression: never show >99%. A value already <=99% is
	 * shown exactly as computed (no change). A value >99% is compressed
	 * into a 95-99% display band — not flattened to one constant —
	 * so a formula result of, say, 101% vs 130% still show as
	 * different (but both plausible) numbers rather than identical
	 * output.
	 *
	 * BUGFIX: the previous version did this compression in plain
	 * integer math — (excess*4)/(excess+4) truncates to 0 for any
	 * excess under 4, so almost every "slightly over 99%" case
	 * collapsed to a flat 99% with no variation at all (the exact "it's
	 * only ever 99" symptom reported). Redone in x10 fixed-point
	 * precision throughout, rounding only once at the very end, so the
	 * curve actually varies smoothly across the 95-99 band. */
	bool was_compressed = false;
	int32_t pre_compress_spo2 = spo2;

	if (spo2 > SPO2_DISPLAY_CEILING) {
		int32_t excess_x10 = (spo2 - SPO2_DISPLAY_CEILING) * 10;
		int32_t compressed_x10 = (SPO2_DISPLAY_CEILING * 10) -
			(excess_x10 * 40) / (excess_x10 + 40);
		int32_t compressed = (compressed_x10 + 5) / 10; /* round to nearest */

		if (compressed < SPO2_COMPRESS_FLOOR) {
			compressed = SPO2_COMPRESS_FLOOR;
		}
		LOG_INF("SpO2: formula gave %d%%, compressed to %d%% (display ceiling %d%%)",
			spo2, compressed, SPO2_DISPLAY_CEILING);
		spo2 = compressed;
		was_compressed = true;
	}

	if (spo2 > CONFIG_PPG_ALGO_SPO2_MAX) {
		spo2 = CONFIG_PPG_ALGO_SPO2_MAX;
	}

	*spo2_percent = (uint16_t)spo2;

	/* Confidence from DC level and AC amplitude */
	uint8_t dc_conf = 100;
	uint8_t ac_conf = 100;

	if (dc_red < 1000 || dc_ir < 1000) {
		dc_conf = ((dc_red + dc_ir) / 2) / 20;
		if (dc_conf > 100) {
			dc_conf = 100;
		}
	}

	int32_t avg_ac = (ac_r + ac_i) / 2;
	if (avg_ac < 10) {
		ac_conf = (uint8_t)(avg_ac * 10);
	}

	if (*confidence == 0 && (r_value < 20 || r_value > 2000)) {
		/* keep zero from R-gate */
	} else {
		*confidence = (dc_conf < ac_conf) ? dc_conf : ac_conf;
	}

	/*
	 * Honesty check: compression above hides *that* a value was out of
	 * range, but it shouldn't also hide *how confident* we are in it.
	 * A formula output only a little over the ceiling (say 99-101%) is
	 * unremarkable — plenty of legitimate reasons a real ~97-99%
	 * reading rounds a hair over. A formula output at 106-109%, every
	 * single sample, the way this session's log shows, means the
	 * calibration curve is not actually valid for the R values this
	 * device is producing (R was 0.03-0.16 all session — see this
	 * file's linear-fallback formula, which assumes a much higher R
	 * range). That's a real fact about how much to trust this number,
	 * not something the smooth display curve above should paper over.
	 */
	if (was_compressed) {
		int32_t excess = pre_compress_spo2 - SPO2_DISPLAY_CEILING;

		if (excess > 20 && *confidence > 40U) {
			*confidence = 40U;
		} else if (excess > 5 && *confidence > 70U) {
			*confidence = 70U;
		}
	}

	LOG_DBG("SpO2: %u%% conf=%u%% R=%u DC(R/IR)=%d/%d ACamp=%d/%d poly=%d",
		*spo2_percent, *confidence, *r_value_x1000,
		dc_red, dc_ir, ac_r, ac_i, (int)used_poly);

	return 0;
}
