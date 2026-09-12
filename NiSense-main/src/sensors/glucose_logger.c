/**
 * @file glucose_logger.c
 * @brief Glucose logger — fat summary + GLUCOSE_RAW chunks in NOR record store.
 */

#include "glucose_logger.h"
#include "home_last_results.h"
#include "device_identity.h"
#include "record_store.h"
#include "glucose_algorithm_profiles.h"
#include "rtc.h"
#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP)
#include "glucose_model_xip.h"
#endif
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor/glucose_sensor.h>
#include <zephyr/drivers/sensor/glucose_algorithm.h>
#include <string.h>
#include <errno.h>

LOG_MODULE_REGISTER(glucose_logger, LOG_LEVEL_INF);

static K_MUTEX_DEFINE(logger_mutex);

static int glucose_append_raw_chunks(uint32_t parent_id,
				     const struct glucose_sensor_result *result)
{
	uint16_t total = result->sample_count;
	uint16_t per = (uint16_t)REC_GLUCOSE_SAMPLES_PER_CHUNK;
	uint16_t chunk_count;
	uint16_t written = 0U;
	uint8_t payload[RECORD_PAYLOAD_MAX];

	if (total == 0U || result->raw_samples == NULL || per == 0U) {
		return 0;
	}

	chunk_count = (uint16_t)((total + per - 1U) / per);

	for (uint16_t ci = 0; ci < chunk_count; ci++) {
		struct rec_glucose_raw_hdr *hdr = (struct rec_glucose_raw_hdr *)payload;
		uint16_t start = (uint16_t)(ci * per);
		uint16_t n = (uint16_t)((start + per <= total) ? per : (total - start));
		size_t len = REC_GLUCOSE_RAW_HDR_SIZE + (size_t)n * REC_GLUCOSE_RAW_SAMPLE_SIZE;
		int rc;

		memset(payload, 0, sizeof(payload));
		hdr->parent_id = parent_id;
		hdr->total_samples = total;
		hdr->chunk_index = ci;
		hdr->chunk_count = chunk_count;
		hdr->n_in_chunk = n;
		hdr->measurement_id = record_store_active_measurement_id();

		for (uint16_t i = 0; i < n; i++) {
			uint16_t s = (uint16_t)(start + i);
			struct rec_glucose_raw_sample *sm =
				(struct rec_glucose_raw_sample *)&payload[REC_GLUCOSE_RAW_HDR_SIZE +
									  i * REC_GLUCOSE_RAW_SAMPLE_SIZE];
			float mv = 0.0f;

			sm->adc = result->raw_samples[s];
			if (result->voltages_mv != NULL) {
				mv = result->voltages_mv[s];
				if (mv > 3276.7f) {
					mv = 3276.7f;
				} else if (mv < -3276.8f) {
					mv = -3276.8f;
				}
			}
			sm->mv_x10 = (int16_t)(mv * 10.0f + (mv >= 0.0f ? 0.5f : -0.5f));
			if (result->sample_timestamps != NULL) {
				sm->timestamp_unix = (uint32_t)result->sample_timestamps[s];
			} else {
				sm->timestamp_unix = (uint32_t)result->timestamp;
			}
		}

		rc = record_store_append(RECORD_TYPE_GLUCOSE_RAW, payload, len, NULL);
		if (rc != 0) {
			LOG_ERR("GLUCOSE_RAW: append failed rc=%d at chunk %u/%u (parent=%u)",
				rc, ci, chunk_count, parent_id);
			return rc;
		}
		written++;
	}

	LOG_INF("GLUCOSE_RAW: parent=%u samples=%u chunks=%u", parent_id, total, written);
	return 0;
}

static int glucose_record_store_append(const struct glucose_sensor_result *result,
				       uint32_t *calc_done_sec_out)
{
	const struct glucose_algorithm_details *algo = result->algorithm_details;
	const struct glucose_algorithm_profile *prof =
		glucose_algorithm_get_active_profile();
	struct rec_glucose rec = {0};
	uint32_t parent_id = 0;
	uint32_t calc_done_sec = 0;
	int rc;

	/* Summary timestamp = calculation complete (now), not ADC capture time. */
	if (rtc_get_unix_time(&calc_done_sec) != 0) {
		calc_done_sec = (uint32_t)(k_uptime_get() / 1000);
	}

	rec.timestamp = calc_done_sec;
	rec.device_id = device_identity_get_u32();
	rec.glucose_mg_dl = (uint16_t)result->glucose_mg_dl;
	rec.quality = (uint8_t)(result->quality.quality_score * 100.0f);
	if (prof != NULL) {
		rec.variant = (uint8_t)prof->variant;
		rec.intercept = (float)prof->intercept;
		rec.outlier_k = (float)prof->outlier_k_factor;
	}
#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP)
	rec.model_version = glucose_model_xip_active_version();
#endif
	if (algo != NULL) {
		rec.tot_coeff = (float)algo->tot_coeff;
		rec.y1_value = (float)algo->y1_value;
		rec.avg_val = (float)algo->avg_val;
		rec.std_dev = (float)algo->std_dev;
		rec.up_lim = (float)algo->up_lim;
		rec.ll_lim = (float)algo->ll_lim;
		rec.p_count = algo->p_count;
		rec.n_count = algo->n_count;
		rec.p_val = (float)algo->p_val;
		rec.n_val = (float)algo->n_val;
		rec.p_plus_n = (float)algo->p_plus_n;
		rec.y2_val = (float)algo->y2_val;
		rec.y2_percent = (float)algo->y2_percent;
		rec.group_cd = algo->group_cd;
		rec.y2_factor = (float)algo->y2_factor;
		rec.y2_factor_val = (float)algo->y2_factor_val;
		rec.const_val = (float)algo->const_val;
		rec.y3_value = (float)algo->y3_value;
		rec.y3_row_no = algo->y3_row_no;
		rec.elim_per = (float)algo->elim_per;
		rec.elim_val = (float)algo->elim_val;
		rec.y_value = algo->y_value;
		rec.calibration_factor = (float)algo->calibration_factor;
		rec.ag_adjusted = (float)algo->ag_adjusted;
		rec.normalized_glucose = (float)algo->normalized_glucose;
		rec.actual_insulin = (float)algo->actual_insulin;
		rec.insulin_correction = (float)algo->insulin_correction;
		rec.insulin_ratio = (float)algo->insulin_ratio;
		rec.inverse_ratio = (float)algo->inverse_ratio;
		rec.homa_ir_index = (float)algo->homa_ir_index;
		/* Prefer profile intercept when details also carries one */
		if (algo->intercept != 0.0) {
			rec.intercept = (float)algo->intercept;
		}
	}
	rec.measurement_id = record_store_active_measurement_id();

	rc = record_store_append(RECORD_TYPE_GLUCOSE, &rec, sizeof(rec), &parent_id);
	if (rc != 0) {
		return rc;
	}

	if (calc_done_sec_out != NULL) {
		*calc_done_sec_out = calc_done_sec;
	}

	rc = glucose_append_raw_chunks(parent_id, result);
	if (rc != 0) {
		LOG_WRN("Glucose summary id=%u stored; raw incomplete rc=%d", parent_id, rc);
	}
	return 0;
}

int glucose_logger_ensure_file(void)
{
	return 0;
}

int glucose_logger_init(void)
{
	LOG_INF("Glucose logger initialized (record store)");
	return 0;
}

int glucose_logger_log_measurement(const struct glucose_sensor_result *result)
{
	const struct glucose_algorithm_details *algo;
	uint32_t calc_done_sec = 0;

	if (!result) {
		return -EINVAL;
	}

	k_mutex_lock(&logger_mutex, K_FOREVER);
	(void)glucose_record_store_append(result, &calc_done_sec);

	algo = result->algorithm_details;
	home_last_results_save_glucose_algo((uint16_t)result->glucose_mg_dl,
					    algo ? (float)algo->actual_insulin : 0.0f,
					    algo ? (float)algo->homa_ir_index : 0.0f,
					    calc_done_sec);
	k_mutex_unlock(&logger_mutex);

	LOG_INF("Glucose logged: %.1f mg/dL", (double)result->glucose_mg_dl);
	return 0;
}

int glucose_logger_get_current_file(char *buf, size_t buf_len)
{
	if (!buf || buf_len == 0) {
		return -EINVAL;
	}
	strncpy(buf, "record_store", buf_len - 1U);
	buf[buf_len - 1U] = '\0';
	return 0;
}

int glucose_logger_close_file(void)
{
	return 0;
}

int glucose_logger_get_fs_stats(uint64_t *total_bytes, uint64_t *free_bytes)
{
	struct record_store_stats st;

	if (!total_bytes || !free_bytes) {
		return -EINVAL;
	}

	record_store_get_stats(&st);
	*total_bytes = (uint64_t)st.slot_count * RECORD_SLOT_SIZE;
	*free_bytes = (uint64_t)(st.slot_count > st.pending ?
				 (st.slot_count - st.pending) : 0U) * RECORD_SLOT_SIZE;
	return 0;
}
