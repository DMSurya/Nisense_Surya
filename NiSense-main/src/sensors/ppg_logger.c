/*
 * Copyright (c) 2025 AARMS
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Data Logger — NOR record store (vitals summary + PPG_RAW chunks).
 *
 * VITALS.timestamp = calculation-complete wall time.
 * Each PPG_RAW sample carries capture wall time (unix + ms).
 * MAX86141 wearable always logs IR/Red/Green (+ DC/AC when PPG_LOG_DC_AC).
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "record_store.h"
#include "device_identity.h"
#include "ppg_logger.h"
#include "home_last_results.h"
#include <string.h>
#include <errno.h>

LOG_MODULE_REGISTER(ppg_logger, LOG_LEVEL_INF);

static K_MUTEX_DEFINE(ppg_log_mutex);
static uint32_t ppg_measurement_count;
static bool initialized;

static void ppg_sample_wall_time(const struct ppg_logger_vitals_params *p,
				 uint16_t sample_index,
				 uint32_t finalize_uptime_ms,
				 uint32_t *unix_sec_out,
				 uint16_t *ms_out)
{
	uint32_t unix_sec = p->timestamp_sec;
	uint16_t ms = 0U;

	if (p->capture_t0_set && p->capture_dt_ms != NULL &&
	    sample_index < p->count) {
		uint32_t sample_up =
			p->capture_t0_uptime_ms + p->capture_dt_ms[sample_index];
		uint32_t age_ms = finalize_uptime_ms - sample_up;
		uint64_t base_ms = (uint64_t)p->timestamp_sec * 1000ULL;
		uint64_t unix_ms = (age_ms < base_ms) ? (base_ms - age_ms) : 0ULL;

		unix_sec = (uint32_t)(unix_ms / 1000ULL);
		ms = (uint16_t)(unix_ms % 1000ULL);
	} else if (p->sample_rate_hz > 0U) {
		uint32_t add_ms =
			((uint32_t)sample_index * 1000U) / p->sample_rate_hz;
		uint64_t unix_ms =
			((uint64_t)p->timestamp_sec * 1000ULL) -
			((uint64_t)(p->count > 0U ? (p->count - 1U) : 0U) *
			 1000ULL / p->sample_rate_hz) +
			add_ms;

		/* Fallback: reconstruct from end-of-window approx. */
		unix_sec = (uint32_t)(unix_ms / 1000ULL);
		ms = (uint16_t)(unix_ms % 1000ULL);
	}

	*unix_sec_out = unix_sec;
	*ms_out = ms;
}

static int ppg_append_raw_chunks(uint32_t parent_id,
				 const struct ppg_logger_vitals_params *p)
{
	uint16_t total = p->count;
	uint16_t per = (uint16_t)REC_PPG_SAMPLES_PER_CHUNK;
	uint16_t chunk_count;
	uint16_t written = 0U;
	uint16_t flags = REC_PPG_RAW_FLAG_ACCEL | REC_PPG_RAW_FLAG_TIMESTAMP;
	uint8_t payload[RECORD_PAYLOAD_MAX];
	uint32_t finalize_uptime_ms = k_uptime_get_32();

	if (total == 0U || p->ir == NULL || p->red == NULL || p->green == NULL) {
		return 0;
	}

	if (per == 0U) {
		return 0;
	}

#if defined(CONFIG_PPG_LOG_DC_AC)
	if (p->ir_dc != NULL && p->red_dc != NULL && p->green_dc != NULL &&
	    p->ir_ac != NULL && p->red_ac != NULL && p->green_ac != NULL) {
		flags |= REC_PPG_RAW_FLAG_DC_AC;
	}
#endif

	chunk_count = (uint16_t)((total + per - 1U) / per);

	for (uint16_t ci = 0; ci < chunk_count; ci++) {
		struct rec_ppg_raw_hdr *hdr = (struct rec_ppg_raw_hdr *)payload;
		uint16_t start = (uint16_t)(ci * per);
		uint16_t n = (uint16_t)((start + per <= total) ? per : (total - start));
		size_t len = REC_PPG_RAW_HDR_SIZE + (size_t)n * REC_PPG_SAMPLE_SIZE;
		int rc;

		memset(payload, 0, sizeof(payload));
		hdr->parent_id = parent_id;
		hdr->sample_rate_hz = p->sample_rate_hz;
		hdr->total_samples = total;
		hdr->chunk_index = ci;
		hdr->chunk_count = chunk_count;
		hdr->n_in_chunk = n;
		hdr->flags = flags;
		hdr->measurement_id = record_store_active_measurement_id();

		for (uint16_t i = 0; i < n; i++) {
			uint16_t s = (uint16_t)(start + i);
			struct rec_ppg_sample *sm =
				(struct rec_ppg_sample *)&payload[REC_PPG_RAW_HDR_SIZE +
								  i * REC_PPG_SAMPLE_SIZE];
			uint32_t ts_unix;
			uint16_t ts_ms;

			sm->ir = p->ir[s];
			sm->red = p->red[s];
			sm->green = p->green[s];
#if defined(CONFIG_PPG_LOG_DC_AC)
			if ((flags & REC_PPG_RAW_FLAG_DC_AC) != 0U) {
				sm->ir_dc = p->ir_dc[s];
				sm->red_dc = p->red_dc[s];
				sm->green_dc = p->green_dc[s];
				sm->ir_ac = p->ir_ac[s];
				sm->red_ac = p->red_ac[s];
				sm->green_ac = p->green_ac[s];
			}
#endif
			sm->ax = (p->accel_x != NULL) ? p->accel_x[s] : 0;
			sm->ay = (p->accel_y != NULL) ? p->accel_y[s] : 0;
			sm->az = (p->accel_z != NULL) ? p->accel_z[s] : 0;

			ppg_sample_wall_time(p, s, finalize_uptime_ms, &ts_unix, &ts_ms);
			sm->timestamp_unix = ts_unix;
			sm->timestamp_ms = ts_ms;
		}

		rc = record_store_append(RECORD_TYPE_PPG_RAW, payload, len, NULL);
		if (rc != 0) {
			LOG_ERR("PPG_RAW: append failed rc=%d at chunk %u/%u (parent=%u)",
				rc, ci, chunk_count, parent_id);
			return rc;
		}
		written++;
	}

	LOG_INF("PPG_RAW: parent=%u samples=%u chunks=%u", parent_id, total, written);
	return 0;
}

static int ppg_record_store_append(const struct ppg_logger_vitals_params *p,
				   uint16_t *out_hr_bpm, uint8_t *out_spo2_percent,
				   uint16_t *out_hb_g_dl_x10, uint8_t *out_resp_rate_bpm,
				   uint16_t *out_sdnn_ms, uint16_t *out_rmssd_ms,
				   uint16_t *out_systolic_mmhg, uint16_t *out_diastolic_mmhg)
{
	struct rec_vitals rec = {0};
	uint32_t parent_id = 0;
	int rc;

	/*
	 * BUGFIX: carry forward the last known-good, non-zero reading for
	 * each headline vital when this cycle didn't produce a valid/non-zero
	 * one, instead of persisting a literal 0. The per-field valid bits in
	 * rec.flags still accurately reflect whether *this specific cycle's*
	 * reading was genuine.
	 */
	const struct home_last_results *last = home_last_results_get();

	uint16_t hr_bpm = p->hr_bpm;
	uint8_t spo2_percent = (uint8_t)p->spo2_percent;
	uint16_t hb_g_dl_x10 = p->hb_g_dl_x10;
	uint8_t resp_rate_bpm = p->resp_rate_bpm;
	uint16_t sdnn_ms = p->sdnn_ms;
	uint16_t rmssd_ms = p->rmssd_ms;
	uint16_t systolic_mmhg = p->systolic_mmhg;
	uint16_t diastolic_mmhg = p->diastolic_mmhg;

	if ((!p->hr_valid || hr_bpm == 0U) && last->hr_valid && last->hr_bpm != 0U) {
		hr_bpm = last->hr_bpm;
	}
	if ((!p->spo2_valid || spo2_percent == 0U) && last->spo2_valid && last->spo2_percent != 0U) {
		spo2_percent = last->spo2_percent;
	}
	if ((!p->hb_valid || hb_g_dl_x10 == 0U) && last->hb_valid && last->hb_g_dl_x10 != 0U) {
		hb_g_dl_x10 = last->hb_g_dl_x10;
	}
	if ((!p->resp_valid || resp_rate_bpm == 0U) && last->resp_valid && last->resp_rate_bpm != 0U) {
		resp_rate_bpm = last->resp_rate_bpm;
	}
	if ((!p->hrv_valid || sdnn_ms == 0U) && last->hrv_valid && last->sdnn_ms != 0U) {
		sdnn_ms = last->sdnn_ms;
		rmssd_ms = last->rmssd_ms;
	}
	if ((!p->bp_valid || systolic_mmhg == 0U) && last->bp_valid && last->systolic_mmhg != 0U) {
		systolic_mmhg = last->systolic_mmhg;
		diastolic_mmhg = last->diastolic_mmhg;
	}

	rec.timestamp = p->timestamp_sec;
	rec.device_id = device_identity_get_u32();
	rec.hr_bpm = hr_bpm;
	rec.hr_conf = p->hr_confidence;
	rec.spo2_percent = spo2_percent;
	rec.spo2_conf = p->spo2_confidence;
	rec.hb_conf = p->hb_confidence;
	rec.hb_g_dl_x10 = hb_g_dl_x10;
	rec.resp_rate_bpm = resp_rate_bpm;
	rec.resp_conf = p->resp_confidence;
	rec.sdnn_ms = sdnn_ms;
	rec.rmssd_ms = rmssd_ms;
	rec.systolic_mmhg = systolic_mmhg;
	rec.diastolic_mmhg = diastolic_mmhg;
	rec.quality = p->quality;
	rec.flags = (uint8_t)((p->hr_valid ? 0x01U : 0U) |
			      (p->spo2_valid ? 0x02U : 0U) |
			      (p->hb_valid ? 0x04U : 0U) |
			      (p->resp_valid ? 0x08U : 0U) |
			      (p->hrv_valid ? 0x10U : 0U) |
			      (p->bp_valid ? 0x20U : 0U));
	rec.snr_db_x10 = p->snr_db_x10;
	rec.perfusion_index_x10 = p->perfusion_index_x10;
	rec.quality_reason = p->quality_reason;
	rec.sample_rate_hz = p->sample_rate_hz;
	rec.sample_count = p->count;
	rec.measurement_id = record_store_active_measurement_id();
	rec.led_pa = p->led_pa;
	rec.motion_bin = p->motion_bin;
	rec.skin_tone_bin = p->skin_tone_bin;
	rec.r_value_x1000 = p->r_value_x1000;
	rec.ac_ir_amp = p->ac_ir_amp;

	LOG_INF("VITALS study: Qreason=0x%02x led_pa=0x%02x motion_bin=%u "
		"skin=%u R=%u ACir=%u",
		p->quality_reason, p->led_pa, p->motion_bin,
		p->skin_tone_bin, p->r_value_x1000, p->ac_ir_amp);
	rc = record_store_append(RECORD_TYPE_VITALS, &rec, sizeof(rec), &parent_id);
	if (rc != 0) {
		return rc;
	}

	*out_hr_bpm = hr_bpm;
	*out_spo2_percent = spo2_percent;
	*out_hb_g_dl_x10 = hb_g_dl_x10;
	*out_resp_rate_bpm = resp_rate_bpm;
	*out_sdnn_ms = sdnn_ms;
	*out_rmssd_ms = rmssd_ms;
	*out_systolic_mmhg = systolic_mmhg;
	*out_diastolic_mmhg = diastolic_mmhg;

	rc = ppg_append_raw_chunks(parent_id, p);
	if (rc != 0) {
		LOG_WRN("VITALS id=%u stored; PPG raw incomplete rc=%d", parent_id, rc);
	}
	return 0;
}

int ppg_logger_ensure_file(void)
{
	return 0;
}

int ppg_logger_init(void)
{
	if (initialized) {
		return 0;
	}
	initialized = true;
	LOG_INF("PPG logger initialized (record store)");
	return 0;
}

int ppg_logger_log_vitals_measurement(const struct ppg_logger_vitals_params *p)
{
	if (p == NULL) {
		return -EINVAL;
	}

	if (!initialized) {
		(void)ppg_logger_init();
	}

	k_mutex_lock(&ppg_log_mutex, K_FOREVER);

	/*
	 * BUGFIX: home_last_results (what the live BLE dashboard reads) was
	 * being updated from this cycle's RAW values, while the persisted
	 * record (what the Excel export shows) used the CARRIED-FORWARD
	 * values from ppg_record_store_append() below. When a cycle's fresh
	 * reading was invalid, those two could disagree — e.g. the app
	 * showing an old cached Hb from days ago while the Excel export
	 * correctly carried forward today's last good reading. Both now use
	 * the exact same final values.
	 */
	uint16_t final_hr_bpm = p->hr_bpm;
	uint8_t final_spo2_percent = (uint8_t)p->spo2_percent;
	uint16_t final_hb_g_dl_x10 = p->hb_g_dl_x10;
	uint8_t final_resp_rate_bpm = p->resp_rate_bpm;
	uint16_t final_sdnn_ms = p->sdnn_ms;
	uint16_t final_rmssd_ms = p->rmssd_ms;
	uint16_t final_systolic_mmhg = p->systolic_mmhg;
	uint16_t final_diastolic_mmhg = p->diastolic_mmhg;

	(void)ppg_record_store_append(p, &final_hr_bpm, &final_spo2_percent,
				      &final_hb_g_dl_x10, &final_resp_rate_bpm,
				      &final_sdnn_ms, &final_rmssd_ms,
				      &final_systolic_mmhg, &final_diastolic_mmhg);
	ppg_measurement_count++;

	home_last_results_save_ppg(final_hr_bpm, p->hr_valid || (final_hr_bpm != 0U),
				   final_spo2_percent, p->spo2_valid || (final_spo2_percent != 0U),
				   final_hb_g_dl_x10, p->hb_valid || (final_hb_g_dl_x10 != 0U),
				   final_resp_rate_bpm, p->resp_valid || (final_resp_rate_bpm != 0U),
				   final_sdnn_ms, final_rmssd_ms,
				   p->hrv_valid || (final_sdnn_ms != 0U),
				   final_systolic_mmhg, final_diastolic_mmhg,
				   p->bp_valid || (final_systolic_mmhg != 0U),
				   p->timestamp_sec);
	k_mutex_unlock(&ppg_log_mutex);

	LOG_INF("Vitals #%u: HR=%u SpO2=%u Hb=%u.%u Resp=%u SDNN=%u BP=%u/%u",
		ppg_measurement_count, final_hr_bpm, final_spo2_percent,
		final_hb_g_dl_x10 / 10U, final_hb_g_dl_x10 % 10U, final_resp_rate_bpm,
		final_sdnn_ms, final_systolic_mmhg, final_diastolic_mmhg);
	if (final_hr_bpm != p->hr_bpm || final_spo2_percent != (uint8_t)p->spo2_percent ||
	    final_hb_g_dl_x10 != p->hb_g_dl_x10) {
		/*
		 * BUGFIX (misleading debug log, found 2026-08-29): this line
		 * used to print p->hr_bpm/p->spo2_percent/p->hb_g_dl_x10 —
		 * the FRESH, pre-carry-forward values — while the actual
		 * persisted record (what Excel/BLE show) uses whatever
		 * ppg_record_store_append() returned above, which may have
		 * substituted a carried-forward value instead. The two could
		 * silently disagree, which is exactly what made this look
		 * like a device-vs-export bug during investigation, when the
		 * device's own fresh values were fine — only the persisted
		 * ones had been substituted. Log both explicitly whenever
		 * they differ, so this is visible going forward instead of
		 * requiring a cross-reference against the Excel export.
		 */
		LOG_INF("  (fresh this cycle: HR=%u SpO2=%u Hb=%u.%u — record store used a "
			"carried-forward value for at least one field, see rec.flags)",
			p->hr_bpm, p->spo2_percent, p->hb_g_dl_x10 / 10U, p->hb_g_dl_x10 % 10U);
	}
	return 0;
}

uint32_t ppg_logger_get_count(void)
{
	return ppg_measurement_count;
}
