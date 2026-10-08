/**
 * @file glucose_insulin_predict.h
 * @brief Glucose-range -> predicted fasting insulin lookup, for HOMA-IR
 *
 * BACKGROUND (2026-10-06): HOMA-IR has always been calculated here as
 *   HOMA-IR = calibrated_glucose x fasting_insulin / 405
 * (see insulin_resistance_calculate() in glucose_algorithm.c), but
 * fasting_insulin itself was always a single fixed constant
 * (DEFAULT_FASTING_INSULIN_UIU_ML, 10.0 uIU/mL) whenever the application
 * hadn't explicitly set a real measured value — so every device, every
 * person, every glucose reading produced a HOMA-IR that moved only
 * because glucose moved, never because insulin did.
 *
 * This file replaces that single fixed constant with a lookup table: the
 * glucose reading that just came in selects a band, and that band's
 * value becomes the predicted fasting insulin for this reading's HOMA-IR
 * calculation. It only ever runs when the application hasn't set a real
 * measured fasting insulin value (see the call site in
 * glucose_sensor.c) — an explicitly-set real value is never overridden.
 *
 * WHAT THIS IS, AND IS NOT:
 *   - This is NOT the "Predicted Insulin = A + B*Glucose + C*Slope +
 *     D*Variation" regression described in the team's own design notes.
 *     That regression needs its A/B/C/D coefficients fitted against real
 *     paired reference data (NiSense glucose pattern vs lab fasting
 *     glucose vs lab fasting insulin, per person) that does not exist
 *     yet. Building a "regression" without that data would just be
 *     curve-fitting against nothing.
 *   - What IS implemented is a simpler, single-reading version of the
 *     same idea: a monotonically increasing step table, grounded in the
 *     same glucose band boundaries already used for the watch's own
 *     on-screen alert thresholds (54 / 70 / 100 / 126 / 180 / 250 mg/dL —
 *     see Section 8 of the Glucose Parameter report), with an insulin
 *     value per band that increases as glucose rises — reflecting the
 *     general, well-established population-level pattern that higher
 *     fasting glucose correlates with higher fasting insulin /
 *     insulin resistance, WITHOUT claiming a precise per-person number.
 *   - The table values below are a reasoned placeholder, not a fitted
 *     model and not a clinical measurement. They should be treated
 *     exactly the way the team's own notes describe the earlier fixed
 *     "10.0" value should be treated: replace them once real paired
 *     reference data lets the A/B/C/D regression actually be fitted.
 *     glucose_predict_fasting_insulin_uiu_ml() is the single function to
 *     change (or replace entirely) when that happens — nothing else in
 *     the firmware needs to change.
 *
 * This file intentionally does NOT do multi-reading averaging/slope/
 * trend analysis (baseline-over-5-readings, recovery behaviour, etc.) —
 * per direction, every glucose reading independently selects its own
 * predicted insulin from the table below, with no dependency on prior
 * readings. A history-aware version (layer 1 in the team's original
 * notes) can be added later as a separate, optional refinement on top
 * of this if wanted — this file's job is just the per-reading lookup.
 */

#ifndef GLUCOSE_INSULIN_PREDICT_H
#define GLUCOSE_INSULIN_PREDICT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Predict fasting insulin (uIU/mL) from a single glucose reading
 *
 * Selects a predicted fasting insulin value from a fixed, monotonically
 * increasing glucose-range table (see glucose_insulin_predict.c for the
 * table itself and the reasoning behind each band). This is a
 * placeholder model — see the file header above — intended to replace
 * the single fixed DEFAULT_FASTING_INSULIN_UIU_ML constant everywhere
 * HOMA-IR is calculated, not a validated clinical prediction.
 *
 * @param glucose_mg_dl Glucose reading, mg/dL (any finite value; out-of-
 *                       range inputs are clamped to the lowest/highest
 *                       band rather than extrapolated)
 * @return Predicted fasting insulin, uIU/mL
 */
float glucose_predict_fasting_insulin_uiu_ml(float glucose_mg_dl);

#ifdef __cplusplus
}
#endif

#endif /* GLUCOSE_INSULIN_PREDICT_H */
