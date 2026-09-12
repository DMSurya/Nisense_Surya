/**
 * @file glucose_algorithm.h
 * @brief Glucose calculation algorithm interface
 *
 * Public interface for glucose_intercept prediction algorithm.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_SENSOR_GLUCOSE_ALGORITHM_H_
#define ZEPHYR_INCLUDE_DRIVERS_SENSOR_GLUCOSE_ALGORITHM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Algorithm intermediate values structure for CSV logging
 *
 * Contains all intermediate calculation values from the glucose_intercept algorithm
 * for comprehensive data logging and analysis.
 */
struct glucose_algorithm_details {
	/* Intercept constant */
	double intercept;                    /* svasth_intercept: 161.832221420583 */

	/* Stage 1: Initial weighted calculation */
	double y1_value;                     /* initial_glucose_estimate */

	/* Stage 2: Statistical analysis */
	double avg_val;                      /* coefficient_mean */
	double std_dev;                      /* coefficient_stddev */

	/* Outlier detection results */
	int32_t p_count;                     /* positive_outlier_count */
	int32_t n_count;                     /* negative_outlier_count */
	double p_val;                        /* outlier_positive_sum */
	double n_val;                        /* outlier_negative_sum */
	double p_plus_n;                     /* outlier_total_removed */

	/* Stage 3: Derived value after outlier removal */
	double y2_val;                       /* glucose_derived_value */
	double y2_percent;                   /* glucose_percentage_change */

	/* Stage 4: Group-based regression */
	int32_t group_cd;                    /* Matched group index (1-11) */
	double y2_factor;                    /* regression_coefficient from matched group */
	double y2_factor_val;                /* regression_weighted_value = y2_val * y2_factor */
	double const_val;                    /* regression_constant from matched group */
	double y3_value;                     /* group_glucose_estimate */

	/* Stage 5: Elimination correction */
	int32_t y3_row_no;                   /* Matched matrix row index */
	double elim_val;                     /* elimination_correction */
	double elim_per;                     /* elimination_percentage */

	/* Final result */
	int32_t y_value;                     /* predicted_glucose_mg_dl */

	/* Validation fields (for algorithm verification) */
	double tot_coeff;                    /* weighted_coeff_sum: Sum before adding intercept */
	double up_lim;                       /* outlier_upper_limit: Upper bound for outlier detection */
	double ll_lim;                       /* outlier_lower_limit: Lower bound for outlier detection */

	/* Stage 6: Insulin Resistance Calculations (HOMA-IR Index) */
	double calibration_factor;           /* glucose_baseline_mg_dl: Baseline glucose measurement */
	double ag_adjusted;                  /* calibrated_glucose_mg_dl: Glucose after calibration (+5%) */
	double normalized_glucose;           /* normalized_glucose_mmol_l: Normalized glucose in mmol/L */
	double actual_insulin;               /* fasting_insulin: Fasting insulin value (μIU/mL) */
	double insulin_correction;           /* insulin_correction_factor: Percentage change in insulin vs glucose */
	double insulin_ratio;                /* insulin_sensitivity_ratio: Normalized glucose / fasting insulin ratio */
	double inverse_ratio;                /* inverse_insulin_ratio: Reciprocal of sensitivity ratio */
	double homa_ir_index;                /* homa_ir_index: HOMA-IR (Homeostasis Model Assessment Index) */
};

/**
 * @brief Calculate glucose value from ADC samples using the active hardware profile
 *
 * @param data Array of 80 raw ADC samples (uint16_t values)
 * @param n Number of samples (must be 80)
 * @param details Optional pointer to structure for storing intermediate values (can be NULL)
 *
 * @return Predicted glucose value in mg/dL, or negative value on error
 */
int32_t glucose_intercept(uint16_t data[], int n, struct glucose_algorithm_details *details);

/** @deprecated Use glucose_intercept(); retained for BLE/docs compatibility. */
static inline int32_t watch_intercept(uint16_t data[], int n,
				      struct glucose_algorithm_details *details)
{
	return glucose_intercept(data, n, details);
}

/**
 * @brief Calculate insulin resistance metrics (HOMA-IR and related indices)
 *
 * Implements HOMA-IR (Homeostasis Model Assessment) calculation for insulin resistance:
 * Step 1: Apply calibration adjustment to baseline glucose
 * Step 2: Normalize glucose to mmol/L scale
 * Step 3: Calculate percentage change between insulin and normalized glucose
 * Step 4: Calculate insulin sensitivity ratio (glucose / insulin)
 * Step 5: Calculate inverse ratio for HOMA-IR formula
 * Step 6: Calculate final HOMA-IR index = (inverse_ratio × calibrated_glucose) / 405
 *
 * @param details Pointer to algorithm details structure to populate
 * @param glucose_mg_dl Measured glucose value in mg/dL (baseline input)
 * @param fasting_insulin_uiu_ml Fasting insulin value in μIU/mL (insulin input)
 * 
 * @return 0 on success, negative errno on error
 */
int insulin_resistance_calculate(struct glucose_algorithm_details *details,
                                 double glucose_mg_dl,
                                 double fasting_insulin_uiu_ml);

#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP)
/**
 * @brief Validate the active XIP glucose model (call before measure / after BLE update).
 * @return 0 on success, negative errno on failure
 */
int glucose_algorithm_init(void);

/** @brief No-op cleanup for the XIP backend (API symmetry). */
void glucose_algorithm_cleanup(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_SENSOR_GLUCOSE_ALGORITHM_H_ */
