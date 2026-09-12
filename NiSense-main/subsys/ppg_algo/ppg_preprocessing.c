/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem - Preprocessing and Filtering
 *
 * Implements DC/AC extraction, baseline removal, and noise filtering.
 *
 * @ref PPG_ALGO_DESIGN_REFERENCES — TI SLAA655 / TIDU475 DC baseline;
 * DC window ≥32 samples (~1.3 s @ 25 Hz) so baseline tracks tissue DC
 * rather than the pulse (fixes circular SNR / bad AC amplitude).
 */

#include "ppg_algo_priv.h"

LOG_MODULE_DECLARE(ppg_algo, CONFIG_PPG_ALGO_LOG_LEVEL);

/* Moving average window for DC component (Kconfig, default 32). */
#define DC_WINDOW_SIZE CONFIG_PPG_ALGO_DC_WINDOW_SAMPLES

void ppg_preproc_init(struct ppg_preproc_state *state)
{
	if (!state) {
		return;
	}

	memset(state, 0, sizeof(*state));
	state->initialized = true;
}

void ppg_preproc_filter(struct ppg_preproc_state *state,
			uint32_t ir_raw, uint32_t red_raw, uint32_t green_raw,
			uint32_t ambient_raw,
			int32_t *ir_dc, int32_t *ir_ac,
			int32_t *red_dc, int32_t *red_ac,
			int32_t *green_dc, int32_t *green_ac)
{
	if (!state) {
		return;
	}

	/* Ambient correction: clamp subtraction to avoid underflow */
	uint32_t amb_dc = 0;

	if (ambient_raw > 0) {
		state->ambient.sum = state->ambient.sum
				    - (state->ambient.sum / DC_WINDOW_SIZE)
				    + ambient_raw;
		state->ambient.dc = state->ambient.sum / DC_WINDOW_SIZE;
		amb_dc = (uint32_t)state->ambient.dc;
	}

	/* Subtract ambient DC offset before processing */
	if (ir_raw > amb_dc) {
		ir_raw -= amb_dc;
	}
	if (red_raw > amb_dc) {
		red_raw -= amb_dc;
	}
	if (green_raw > amb_dc) {
		green_raw -= amb_dc;
	}

	/* First sample - initialize */
	if (!state->initialized || state->ir.count == 0) {
		state->ir.dc = ir_raw;
		state->red.dc = red_raw;
		state->green.dc = green_raw;
		state->ir.prev_sample = ir_raw;
		state->red.prev_sample = red_raw;
		state->green.prev_sample = green_raw;
		state->ir.sum = ir_raw * DC_WINDOW_SIZE;
		state->red.sum = red_raw * DC_WINDOW_SIZE;
		state->green.sum = green_raw * DC_WINDOW_SIZE;
		state->ir.count = 1;
		state->red.count = 1;
		state->green.count = 1;
		state->initialized = true;
	}

	/* Update DC component using moving average */
	state->ir.sum = state->ir.sum - (state->ir.sum / DC_WINDOW_SIZE) + ir_raw;
	state->ir.dc = state->ir.sum / DC_WINDOW_SIZE;

	state->red.sum = state->red.sum - (state->red.sum / DC_WINDOW_SIZE) + red_raw;
	state->red.dc = state->red.sum / DC_WINDOW_SIZE;

	state->green.sum = state->green.sum - (state->green.sum / DC_WINDOW_SIZE) + green_raw;
	state->green.dc = state->green.sum / DC_WINDOW_SIZE;

	/* Calculate AC component (signal minus DC baseline) */
	state->ir.ac = (int32_t)ir_raw - state->ir.dc;
	state->red.ac = (int32_t)red_raw - state->red.dc;
	state->green.ac = (int32_t)green_raw - state->green.dc;

	/* Update previous sample */
	state->ir.prev_sample = ir_raw;
	state->red.prev_sample = red_raw;
	state->green.prev_sample = green_raw;

	/* Increment counts */
	state->ir.count++;
	state->red.count++;
	state->green.count++;

	/* Output DC and AC components */
	if (ir_dc) {
		*ir_dc = state->ir.dc;
	}
	if (ir_ac) {
		*ir_ac = state->ir.ac;
	}
	if (red_dc) {
		*red_dc = state->red.dc;
	}
	if (red_ac) {
		*red_ac = state->red.ac;
	}
	if (green_dc) {
		*green_dc = state->green.dc;
	}
	if (green_ac) {
		*green_ac = state->green.ac;
	}
}
