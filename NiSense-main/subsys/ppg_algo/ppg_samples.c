/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem - Sample Buffer Management
 *
 * Manages circular buffers for raw PPG and accelerometer samples.
 */

#include "ppg_algo_priv.h"
#include <stdlib.h>
#include <string.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(ppg_algo, CONFIG_PPG_ALGO_LOG_LEVEL);

int ppg_samples_init(struct ppg_sample_buffer *buffer, uint16_t capacity, bool include_accel)
{
	if (!buffer || capacity == 0 || capacity > PPG_MAX_SAMPLES) {
		return -EINVAL;
	}

	/* Free any existing buffers */
	ppg_samples_free(buffer);

	memset(buffer, 0, sizeof(*buffer));
	buffer->capacity = capacity;

	/* Allocate PPG channels (always needed) */
	buffer->ir = k_malloc(sizeof(uint32_t) * capacity);
	if (!buffer->ir) {
		goto error;
	}

	buffer->red = k_malloc(sizeof(uint32_t) * capacity);
	if (!buffer->red) {
		goto error;
	}

	buffer->green = k_malloc(sizeof(uint32_t) * capacity);
	if (!buffer->green) {
		goto error;
	}

	buffer->ambient = k_malloc(sizeof(uint32_t) * capacity);
	if (!buffer->ambient) {
		goto error;
	}

	/* ir_ac buffer for respiration rate algorithm */
	buffer->ir_ac = k_malloc(sizeof(int32_t) * capacity);
	if (!buffer->ir_ac) {
		goto error;
	}

#if defined(CONFIG_PPG_LOG_DC_AC)
	buffer->ir_dc = k_malloc(sizeof(int32_t) * capacity);
	buffer->red_dc = k_malloc(sizeof(int32_t) * capacity);
	buffer->green_dc = k_malloc(sizeof(int32_t) * capacity);
	buffer->red_ac = k_malloc(sizeof(int32_t) * capacity);
	buffer->green_ac = k_malloc(sizeof(int32_t) * capacity);
	if (!buffer->ir_dc || !buffer->red_dc || !buffer->green_dc ||
	    !buffer->red_ac || !buffer->green_ac) {
		goto error;
	}
#endif

	/* Allocate accelerometer channels if requested */
	if (include_accel) {
		buffer->accel_x = k_malloc(sizeof(int16_t) * capacity);
		if (!buffer->accel_x) {
			goto error;
		}

		buffer->accel_y = k_malloc(sizeof(int16_t) * capacity);
		if (!buffer->accel_y) {
			goto error;
		}

		buffer->accel_z = k_malloc(sizeof(int16_t) * capacity);
		if (!buffer->accel_z) {
			goto error;
		}

		buffer->has_accel = true;
	}

	buffer->capture_dt_ms = k_malloc(sizeof(uint16_t) * capacity);
	if (!buffer->capture_dt_ms) {
		goto error;
	}
	buffer->capture_t0_uptime_ms = 0U;
	buffer->capture_t0_set = false;

	buffer->count = 0;

	LOG_DBG("Sample buffer initialized: capacity=%u, accel=%s",
		capacity, include_accel ? "yes" : "no");

	return 0;

error:
	ppg_samples_free(buffer);
	LOG_ERR("Failed to allocate sample buffer (capacity=%u)", capacity);
	return -ENOMEM;
}

void ppg_samples_free(struct ppg_sample_buffer *buffer)
{
	if (!buffer) {
		return;
	}

	if (buffer->ir) {
		k_free(buffer->ir);
		buffer->ir = NULL;
	}

	if (buffer->red) {
		k_free(buffer->red);
		buffer->red = NULL;
	}

	if (buffer->green) {
		k_free(buffer->green);
		buffer->green = NULL;
	}

	if (buffer->ambient) {
		k_free(buffer->ambient);
		buffer->ambient = NULL;
	}

	if (buffer->ir_ac) {
		k_free(buffer->ir_ac);
		buffer->ir_ac = NULL;
	}

#if defined(CONFIG_PPG_LOG_DC_AC)
	if (buffer->ir_dc) {
		k_free(buffer->ir_dc);
		buffer->ir_dc = NULL;
	}
	if (buffer->red_dc) {
		k_free(buffer->red_dc);
		buffer->red_dc = NULL;
	}
	if (buffer->green_dc) {
		k_free(buffer->green_dc);
		buffer->green_dc = NULL;
	}
	if (buffer->red_ac) {
		k_free(buffer->red_ac);
		buffer->red_ac = NULL;
	}
	if (buffer->green_ac) {
		k_free(buffer->green_ac);
		buffer->green_ac = NULL;
	}
#endif

	if (buffer->accel_x) {
		k_free(buffer->accel_x);
		buffer->accel_x = NULL;
	}

	if (buffer->accel_y) {
		k_free(buffer->accel_y);
		buffer->accel_y = NULL;
	}

	if (buffer->accel_z) {
		k_free(buffer->accel_z);
		buffer->accel_z = NULL;
	}

	if (buffer->capture_dt_ms) {
		k_free(buffer->capture_dt_ms);
		buffer->capture_dt_ms = NULL;
	}

	buffer->count = 0;
	buffer->capacity = 0;
	buffer->has_accel = false;
	buffer->capture_t0_uptime_ms = 0U;
	buffer->capture_t0_set = false;

	LOG_DBG("Sample buffer freed");
}

/*
 * BUGFIX / signal processing: sample-and-hold for isolated channel dropouts,
 * with range validation. A sample of exactly 0, or an implausible magnitude
 * above the MAX86141's real 19-bit ADC ceiling (524287 / 0x7FFFF — field
 * data showed values up to 4294967295, a classic unsigned-wraparound
 * artifact), is never a genuine reading. Hold the last known-good sample
 * instead of passing either through, so neither a flat 0 nor a nonsense
 * billion-count spike ever reaches the algorithm, the record store, or the
 * exported data. Module-level state (persists for the life of the firmware
 * run), so only the very first sample ever taken after power-on can
 * legitimately still be 0.
 */
#define MAX86141_ADC_FULL_SCALE 0x7FFFFU /* 19-bit ADC, MAX86141 datasheet */

static uint32_t s_last_good_ir;
static uint32_t s_last_good_red;
static uint32_t s_last_good_green;

int ppg_samples_add(struct ppg_sample_buffer *buffer,
		    uint32_t ir, uint32_t red, uint32_t green,
		    uint32_t ambient, int32_t ir_ac,
#if defined(CONFIG_PPG_LOG_DC_AC)
		    int32_t ir_dc, int32_t red_dc, int32_t green_dc,
		    int32_t red_ac, int32_t green_ac,
#endif
		    int16_t accel_x, int16_t accel_y, int16_t accel_z)
{
	if (!buffer || !buffer->ir || !buffer->red) {
		return -EINVAL;
	}

	if (buffer->count >= buffer->capacity) {
		return -ENOMEM;
	}

	uint16_t idx = buffer->count;

	bool ir_plausible = (ir != 0U) && (ir <= MAX86141_ADC_FULL_SCALE);
	bool red_plausible = (red != 0U) && (red <= MAX86141_ADC_FULL_SCALE);
	bool green_plausible = (green != 0U) && (green <= MAX86141_ADC_FULL_SCALE);

	if (ir_plausible) {
		s_last_good_ir = ir;
	} else {
		ir = (s_last_good_ir != 0U) ? s_last_good_ir : 0U;
	}
	if (red_plausible) {
		s_last_good_red = red;
	} else {
		red = (s_last_good_red != 0U) ? s_last_good_red : 0U;
	}
	if (green_plausible) {
		s_last_good_green = green;
	} else {
		green = (s_last_good_green != 0U) ? s_last_good_green : 0U;
	}

	buffer->ir[idx] = ir;
	buffer->red[idx] = red;
	buffer->green[idx] = green;

	if (buffer->ambient) {
		buffer->ambient[idx] = ambient;
	}
	if (buffer->ir_ac) {
		buffer->ir_ac[idx] = ir_ac;
	}

#if defined(CONFIG_PPG_LOG_DC_AC)
	if (buffer->ir_dc) {
		buffer->ir_dc[idx] = ir_dc;
	}
	if (buffer->red_dc) {
		buffer->red_dc[idx] = red_dc;
	}
	if (buffer->green_dc) {
		buffer->green_dc[idx] = green_dc;
	}
	if (buffer->red_ac) {
		buffer->red_ac[idx] = red_ac;
	}
	if (buffer->green_ac) {
		buffer->green_ac[idx] = green_ac;
	}
#endif

	if (buffer->has_accel) {
		buffer->accel_x[idx] = accel_x;
		buffer->accel_y[idx] = accel_y;
		buffer->accel_z[idx] = accel_z;
	}

	if (buffer->capture_dt_ms) {
		uint32_t now_up = k_uptime_get_32();

		if (!buffer->capture_t0_set) {
			buffer->capture_t0_uptime_ms = now_up;
			buffer->capture_t0_set = true;
			buffer->capture_dt_ms[idx] = 0U;
		} else {
			uint32_t dt = now_up - buffer->capture_t0_uptime_ms;

			buffer->capture_dt_ms[idx] =
				(uint16_t)MIN(dt, (uint32_t)UINT16_MAX);
		}
	}

	buffer->count++;

	return 0;
}
