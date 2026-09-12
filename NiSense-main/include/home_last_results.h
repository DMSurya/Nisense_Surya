/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Persistent last-measurement summary for home / vitals / metabolic screens.
 * Stored as an NVS settings blob (key home/last).
 */

#ifndef HOME_LAST_RESULTS_H_
#define HOME_LAST_RESULTS_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct home_last_results {
	uint32_t timestamp;
	uint16_t hr_bpm;
	uint8_t  spo2_percent;
	uint16_t glucose_mg_dl;
	float    wrist_c;
	uint16_t hb_g_dl_x10;
	uint8_t  resp_rate_bpm;
	float    insulin_uiu_ml;
	float    homa_ir_index;
	uint16_t sdnn_ms;
	uint16_t rmssd_ms;
	uint16_t systolic_mmhg;
	uint16_t diastolic_mmhg;
	bool     hr_valid;
	bool     spo2_valid;
	bool     glucose_valid;
	bool     wrist_valid;
	bool     hb_valid;
	bool     resp_valid;
	bool     insulin_valid;
	bool     homa_valid;
	bool     hrv_valid;
	bool     bp_valid;
	bool     loaded;
};

void home_last_results_load(void);

/** @return Pointer to cached results (always non-NULL). */
const struct home_last_results *home_last_results_get(void);

void home_last_results_save_ppg(uint16_t hr_bpm, bool hr_valid,
			      uint8_t spo2_percent, bool spo2_valid,
			      uint16_t hb_g_dl_x10, bool hb_valid,
			      uint8_t resp_rate_bpm, bool resp_valid,
			      uint16_t sdnn_ms, uint16_t rmssd_ms, bool hrv_valid,
			      uint16_t systolic_mmhg, uint16_t diastolic_mmhg,
			      bool bp_valid, uint32_t timestamp_sec);
void home_last_results_save_glucose(uint16_t mg_dl, uint32_t timestamp_sec);
void home_last_results_save_glucose_algo(uint16_t mg_dl, float insulin_uiu_ml,
					 float homa_ir_index, uint32_t timestamp_sec);
void home_last_results_save_wrist_temp(float wrist_c, uint32_t timestamp_sec);

#ifdef __cplusplus
}
#endif

#endif /* HOME_LAST_RESULTS_H_ */
