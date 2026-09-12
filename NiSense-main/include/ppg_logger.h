/*
 * Copyright (c) 2025 AARMS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PPG_LOGGER_H_
#define PPG_LOGGER_H_

#include <zephyr/subsys/ppg_algo/ppg_algo.h>
#include <zephyr/kernel.h>

/**
 * @brief Parameters for a single unified vitals measurement log entry.
 */
struct ppg_logger_vitals_params {
	const uint32_t *ir;
	const uint32_t *red;
	const uint32_t *green;
	const uint32_t *ambient;
#if defined(CONFIG_PPG_LOG_DC_AC)
	const int32_t *ir_dc;
	const int32_t *red_dc;
	const int32_t *green_dc;
	const int32_t *ir_ac;
	const int32_t *red_ac;
	const int32_t *green_ac;
#endif
	const int16_t  *accel_x;
	const int16_t  *accel_y;
	const int16_t  *accel_z;
	uint16_t count;

	/** Summary / calculation-complete wall time (unix seconds). */
	uint32_t timestamp_sec;
	/**
	 * Optional per-sample capture offsets from measurement start (ms).
	 * Length == count. Used with capture_t0_uptime_ms to derive raw
	 * sample wall-clock times at log time.
	 */
	const uint16_t *capture_dt_ms;
	uint32_t capture_t0_uptime_ms;
	bool     capture_t0_set;

	uint16_t sample_rate_hz;
	float    total_time_sec;

	uint16_t hr_bpm;
	uint8_t  hr_confidence;
	bool     hr_valid;

	uint16_t spo2_percent;
	uint8_t  spo2_confidence;
	bool     spo2_valid;
	uint16_t r_value_x1000;

	uint16_t hb_g_dl_x10;
	uint8_t  hb_confidence;
	bool     hb_valid;

	uint8_t  resp_rate_bpm;
	uint8_t  resp_confidence;
	bool     resp_valid;

	uint16_t sdnn_ms;
	uint16_t rmssd_ms;
	bool     hrv_valid;

	uint16_t systolic_mmhg;
	uint16_t diastolic_mmhg;
	bool     bp_valid;

	uint8_t  quality;
	uint8_t  perfusion_index_x10;
	uint16_t snr_db_x10;

	/* Study / Arms metadata (ISO 80601-2-61 hooks) */
	uint8_t  quality_reason;     /**< PPG_QUALITY_REASON_* bitmask */
	uint8_t  led_pa;             /**< Host AGC LED PA register */
	uint8_t  motion_bin;         /**< 0=still,1=mild,2=motion */
	uint8_t  skin_tone_bin;      /**< 0=unknown; 1–6 Fitzpatrick if set */
	uint16_t ac_ir_amp;          /**< IR AC pulse amplitude for study logs */
};

int ppg_logger_init(void);

/** @brief No-op retained for call-site compatibility (was FatFS CSV seed). */
int ppg_logger_ensure_file(void);

/**
 * @brief Persist vitals summary + PPG_RAW chunks to the NOR record store.
 */
int ppg_logger_log_vitals_measurement(const struct ppg_logger_vitals_params *p);

uint32_t ppg_logger_get_count(void);

#endif /* PPG_LOGGER_H_ */
