/**
 * @file glucose_insulin_predict.c
 * @brief Glucose-range -> predicted fasting insulin lookup table
 *
 * See glucose_insulin_predict.h for the full background and the caveats
 * on what this table is (a reasoned placeholder) and is not (a fitted
 * regression against real reference data).
 */

#include "glucose_insulin_predict.h"
#include <zephyr/logging/log.h>
#include <stddef.h>

LOG_MODULE_REGISTER(glucose_insulin_predict, CONFIG_SENSOR_LOG_LEVEL);

/*
 * Band upper edges (mg/dL, inclusive) and the predicted fasting insulin
 * (uIU/mL) for that band. A reading selects the first row whose
 * glucose_upper_mg_dl is >= the reading; the last row catches everything
 * above its lower neighbour's edge.
 *
 * Band boundaries deliberately reuse the same clinical cut points the
 * watch already uses for its own on-screen glucose status (54 / 70 / 100
 * / 126 / 180 / 250 mg/dL — see glucose_classify_status() and Section 8
 * of the Glucose Parameter report), so "which band a reading is in" and
 * "what the screen calls that reading" always agree, and so the table
 * stays anchored to numbers already reviewed/approved rather than a new,
 * separate set of cut points invented just for this table.
 *
 * Insulin values increase monotonically with glucose, reflecting the
 * general population-level pattern (higher fasting glucose correlates
 * with higher fasting insulin / insulin resistance) without claiming
 * per-person accuracy. The top band (>250, severe hyperglycaemia) is
 * capped rather than extrapolated further upward: at that extreme,
 * fasting insulin can in reality go either very high (severe insulin
 * resistance) or very low (beta-cell failure / untreated Type 1) — a
 * single table cannot distinguish those, so this band is the least
 * reliable one and is flagged as such in the LOG_DBG below.
 */
struct insulin_band {
	float glucose_upper_mg_dl; /* inclusive upper edge of this band */
	float insulin_uiu_ml;
};

static const struct insulin_band s_bands[] = {
	{ 53.0f,  3.0f },  /* < 54: severe hypoglycaemia */
	{ 69.0f,  4.0f },  /* 54-69: hypoglycaemia */
	{ 84.0f,  5.5f },  /* 70-84: low-normal fasting */
	{ 99.0f,  7.5f },  /* 85-99: normal fasting, upper half */
	{ 109.0f, 10.5f }, /* 100-109: early impaired fasting glucose */
	{ 125.0f, 13.5f }, /* 110-125: impaired fasting glucose (prediabetes) */
	{ 139.0f, 17.0f }, /* 126-139: diabetes range, lower */
	{ 159.0f, 20.5f }, /* 140-159: diabetes range */
	{ 180.0f, 24.0f }, /* 160-180: diabetes range, upper (watch NORMAL ceiling) */
	{ 250.0f, 27.5f }, /* 181-250: diabetes range, RISK */
	{ 1.0e6f, 30.0f }, /* > 250: severe hyperglycaemia, CRITICAL - least reliable band, see header */
};

#define NUM_BANDS (sizeof(s_bands) / sizeof(s_bands[0]))

float glucose_predict_fasting_insulin_uiu_ml(float glucose_mg_dl)
{
	size_t i;
	float predicted;

	if (glucose_mg_dl <= s_bands[0].glucose_upper_mg_dl) {
		predicted = s_bands[0].insulin_uiu_ml;
	} else {
		predicted = s_bands[NUM_BANDS - 1].insulin_uiu_ml;
		for (i = 0; i < NUM_BANDS; i++) {
			if (glucose_mg_dl <= s_bands[i].glucose_upper_mg_dl) {
				predicted = s_bands[i].insulin_uiu_ml;
				break;
			}
		}
	}

	LOG_DBG("Glucose %.1f mg/dL -> predicted fasting insulin %.2f uIU/mL%s",
		(double)glucose_mg_dl, (double)predicted,
		(glucose_mg_dl > 250.0f)
			? " (severe hyperglycaemia band - least reliable, see header)"
			: "");

	return predicted;
}
