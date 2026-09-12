/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "home_last_results.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <errno.h>

#ifdef CONFIG_SETTINGS
#include <zephyr/settings/settings.h>
#define HOME_LAST_KEY "home/last"
#endif

LOG_MODULE_REGISTER(home_last, LOG_LEVEL_INF);

static struct home_last_results s_last;
static K_MUTEX_DEFINE(s_lock);

static void cache_clear(void)
{
	memset(&s_last, 0, sizeof(s_last));
}

#ifdef CONFIG_SETTINGS
static int home_direct_load(const char *key, size_t len, settings_read_cb read_cb,
			    void *cb_arg, void *param)
{
	ARG_UNUSED(key);
	struct home_last_results *out = param;

	if (len != sizeof(*out)) {
		return -EINVAL;
	}
	ssize_t rc = read_cb(cb_arg, out, sizeof(*out));

	return (rc < 0) ? (int)rc : 0;
}
#endif

static void persist_locked(void)
{
	s_last.loaded = true;
#ifdef CONFIG_SETTINGS
	int ret = settings_save_one(HOME_LAST_KEY, &s_last, sizeof(s_last));

	if (ret != 0) {
		LOG_WRN("home/last save failed: %d", ret);
	}
#endif
}

void home_last_results_load(void)
{
	k_mutex_lock(&s_lock, K_FOREVER);
	cache_clear();

#ifdef CONFIG_SETTINGS
	struct home_last_results tmp;

	memset(&tmp, 0, sizeof(tmp));
	if (settings_load_subtree_direct(HOME_LAST_KEY, home_direct_load, &tmp) == 0 &&
	    tmp.loaded) {
		s_last = tmp;
		LOG_INF("Home summary loaded from NVS");
	}
#endif
	k_mutex_unlock(&s_lock);
}

const struct home_last_results *home_last_results_get(void)
{
	return &s_last;
}

void home_last_results_save_ppg(uint16_t hr_bpm, bool hr_valid,
			      uint8_t spo2_percent, bool spo2_valid,
			      uint16_t hb_g_dl_x10, bool hb_valid,
			      uint8_t resp_rate_bpm, bool resp_valid,
			      uint16_t sdnn_ms, uint16_t rmssd_ms, bool hrv_valid,
			      uint16_t systolic_mmhg, uint16_t diastolic_mmhg,
			      bool bp_valid, uint32_t timestamp_sec)
{
	k_mutex_lock(&s_lock, K_FOREVER);
	if (hr_bpm > 0U) {
		s_last.hr_bpm = hr_bpm;
		s_last.hr_valid = hr_valid;
	}
	if (spo2_percent > 0U) {
		s_last.spo2_percent = spo2_percent;
		s_last.spo2_valid = spo2_valid;
	}
	if (hb_g_dl_x10 > 0U) {
		s_last.hb_g_dl_x10 = hb_g_dl_x10;
		s_last.hb_valid = hb_valid;
	}
	if (resp_rate_bpm > 0U) {
		s_last.resp_rate_bpm = resp_rate_bpm;
		s_last.resp_valid = resp_valid;
	}
	if (hrv_valid && sdnn_ms > 0U) {
		s_last.sdnn_ms = sdnn_ms;
		s_last.rmssd_ms = rmssd_ms;
		s_last.hrv_valid = true;
	}
	/* Persist BP whenever systolic is present (match HR/Hb value>0 policy). */
	if (systolic_mmhg > 0U) {
		s_last.systolic_mmhg = systolic_mmhg;
		s_last.diastolic_mmhg = diastolic_mmhg;
		s_last.bp_valid = bp_valid;
	}
	if (timestamp_sec > 0U) {
		s_last.timestamp = timestamp_sec;
	}
	persist_locked();
	k_mutex_unlock(&s_lock);
}

void home_last_results_save_glucose(uint16_t mg_dl, uint32_t timestamp_sec)
{
	k_mutex_lock(&s_lock, K_FOREVER);
	if (mg_dl > 0U) {
		s_last.glucose_mg_dl = mg_dl;
		s_last.glucose_valid = true;
	}
	if (timestamp_sec > 0U) {
		s_last.timestamp = timestamp_sec;
	}
	persist_locked();
	k_mutex_unlock(&s_lock);
}

void home_last_results_save_glucose_algo(uint16_t mg_dl, float insulin_uiu_ml,
					 float homa_ir_index, uint32_t timestamp_sec)
{
	k_mutex_lock(&s_lock, K_FOREVER);
	if (mg_dl > 0U) {
		s_last.glucose_mg_dl = mg_dl;
		s_last.glucose_valid = true;
	}
	if (insulin_uiu_ml > 0.0f) {
		s_last.insulin_uiu_ml = insulin_uiu_ml;
		s_last.insulin_valid = true;
	}
	if (homa_ir_index > 0.0f) {
		s_last.homa_ir_index = homa_ir_index;
		s_last.homa_valid = true;
	}
	if (timestamp_sec > 0U) {
		s_last.timestamp = timestamp_sec;
	}
	persist_locked();
	k_mutex_unlock(&s_lock);
}

void home_last_results_save_wrist_temp(float wrist_c, uint32_t timestamp_sec)
{
	k_mutex_lock(&s_lock, K_FOREVER);
	if (wrist_c > -100.0f) {
		s_last.wrist_c = wrist_c;
		s_last.wrist_valid = true;
	}
	if (timestamp_sec > 0U) {
		s_last.timestamp = timestamp_sec;
	}
	persist_locked();
	k_mutex_unlock(&s_lock);
}
