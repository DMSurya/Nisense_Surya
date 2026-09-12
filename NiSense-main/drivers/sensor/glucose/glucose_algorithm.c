/**
 * @file glucose_algorithm.c
 * @brief Svasth Glucose Prediction Algorithm - NIRS-based glucose calculation
 *
 * This file implements the Svasth glucose prediction algorithm based on
 * near-infrared spectroscopy (NIRS) analysis of photodiode samples from
 * a non-invasive glucose sensor.
 *
 * Algorithm Overview:
 * ===================
 * The algorithm processes 80 raw ADC samples through a multi-stage pipeline:
 * 1. Coefficient-weighted linear combination (Y1 calculation)
 * 2. Statistical outlier detection and removal using k-sigma method
 * 3. Derived value calculation after outlier elimination
 * 4. Percentage-based grouping and linear regression (Y2/Y3 calculation)
 * 5. Group-specific elimination correction using lookup matrices
 * 6. Final glucose prediction value in mg/dL
 *
 * Algorithm source: Customer-provided working implementation (tested and verified)
 *
 * @note This algorithm requires exactly 80 ADC samples for accurate results
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include "glucose_algorithm.h"
#include "glucose_algorithm_profiles.h"
#include <zephyr/drivers/sensor/glucose_algorithm_config.h>
#include "glucose_algorithm_matrices.h"

/* Dedicated log level (default WRN via prj.conf) so the hot-loop algorithm
 * does not spam the RTT channel at INF during every matrix multiply. */
LOG_MODULE_REGISTER(glucose_algorithm, CONFIG_GLUCOSE_ALGORITHM_LOG_LEVEL);

/* ============================================================================
 * Algorithm Constants
 * ============================================================================ */

/* Number of ADC samples required for glucose calculation */
#define TOTAL_POINTS 80

/**
 * @brief Matrix access abstraction (embedded wearable matrices only)
 */
#ifdef GLUCOSE_MATRICES_RUNTIME_LOOKUP
/* Runtime lookup (file or XIP) uses direct lookup API; no MATRIX_GET macro. */
#else
/* Embedded mode: direct access to compile-time matrix arrays */
static inline double matrix_get_embedded(int group_idx, uint32_t row, uint32_t col)
{
    /* Map group index (0-10) to corresponding matrix array */
    switch (group_idx) {
        case 0: return (double)group_one_matrix[row][col];
        case 1: return (double)group_two_matrix[row][col];
        case 2: return (double)group_three_matrix[row][col];
        case 3: return (double)group_four_matrix[row][col];
        case 4: return (double)group_five_matrix[row][col];
        case 5: return (double)group_six_matrix[row][col];
        case 6: return (double)group_seven_matrix[row][col];
        case 7: return (double)group_eight_matrix[row][col];
        case 8: return (double)group_nine_matrix[row][col];
        case 9: return (double)group_ten_matrix[row][col];
        case 10: return (double)group_eleven_matrix[row][col];
        default: return 0.0; /* Should never happen */
    }
}

#define MATRIX_GET(group_idx, row, col) \
	matrix_get_embedded((group_idx), (row), (col))
#endif

/**
 * @brief Svasth Glucose Prediction Algorithm - Main calculation function
 *
 * This function implements the complete Svasth glucose prediction algorithm
 * based on near-infrared spectroscopy (NIRS) analysis. The algorithm processes
 * 80 raw ADC samples through a multi-stage pipeline to predict blood glucose
 * concentration in mg/dL.
 *
 * Algorithm Stages:
 * =================
 * Stage 1 - Coefficient-Weighted Sum (Y1):
 *   - Multiply each ADC sample by its corresponding Svasth coefficient
 *   - Sum all weighted values and add intercept constant
 *   - Result: Y1_value = intercept + sum(sample[i] * coefficient[i])
 *
 * Stage 2 - Statistical Outlier Detection:
 *   - Calculate mean and standard deviation of weighted coefficients
 *   - Use k-sigma method (k=1.75) to identify outliers beyond:
 *     lower_limit = mean - (k * stddev)
 *     upper_limit = mean + (k * stddev)
 *   - Mark samples outside these limits as outliers
 *
 * Stage 3 - Outlier Elimination and Derived Value:
 *   - Remove positive and negative outliers from Y1
 *   - Calculate: derived_y = Y1 - sum(outliers)
 *   - Calculate percentage: derived_ybyy1 = (derived_y / Y1) * 100%
 *
 * Stage 4 - Group-Based Linear Regression:
 *   - Classify derived_ybyy1 percentage into one of 11 groups
 *   - Apply group-specific linear regression: Y3 = (derived_y * y2_coeff) + y2_constant
 *   - Each group has unique coefficients optimized for its range
 *
 * Stage 5 - Elimination Correction:
 *   - Lookup Y3 value in group-specific elimination matrix
 *   - Find matching range [col1, col2] where col1 < Y3 < col2
 *   - Apply elimination correction value (col3) from matrix
 *   - Final prediction: pred_result = Y3 + elimination_correction
 *
 * @param data Array of 80 raw ADC samples (uint16_t values, 0-4095 range)
 * @param n Number of samples (must be exactly 80)
 * @param details Optional pointer to structure for storing intermediate values (can be NULL)
 *
 * @return Predicted glucose value in mg/dL (milligrams per deciliter)
 *         Returns -1 on error (invalid sample count)
 *
 * @note This algorithm has been tested and verified by customer
 */
int32_t glucose_intercept(uint16_t data[], int n, struct glucose_algorithm_details *details)
{
    const struct glucose_algorithm_profile *profile = glucose_algorithm_get_active_profile();
    const float *svasth_coeff = profile->svasth_coeff;
    const struct glucose_group_config *group_configs = profile->groups;
    const uint8_t num_groups = profile->num_groups;

#if !defined(GLUCOSE_MATRICES_RUNTIME_LOOKUP)
    if (profile->variant == GLUCOSE_HW_PULSE) {
        LOG_ERR("Pulse profile requires XIP matrices "
                "(CONFIG_GLUCOSE_MATRICES_FROM_XIP)");
        return -1;
    }
#endif
    /* ========================================================================
     * Variable Declarations
     * ======================================================================== */
    
    /* Loop counters */
    int i, j;
    
    /* Outlier detection counters */
    int positive_outlier_count = 0;  /* Number of positive outliers found */
    int negative_outlier_count = 0;  /* Number of negative outliers found */
    int matrix_match_count = 0;      /* Number of matrix matches (debug) */
    int matched_group_idx = -1;      /* Matched group index (0-10, or -1 if not found) */
    int matched_matrix_row = 0;      /* Matched elimination matrix row (0-based) */
    
    /* Statistical outlier detection limits */
    double outlier_lower_limit;      /* Lower bound for outlier detection */
    double outlier_upper_limit;       /* Upper bound for outlier detection */
    
    /* Stage 1: Coefficient-weighted calculation */
    double weighted_coeff_sum = 0.0;           /* Sum of all weighted coefficients */
    double initial_glucose_estimate = 0;       /* Y1: Initial glucose estimate */
    double outlier_positive_sum = 0;           /* Sum of positive outliers */
    double outlier_negative_sum = 0;           /* Sum of negative outliers */
    
    /* Stage 3: Derived values after outlier elimination */
    double glucose_derived_value;              /* derived_y: Glucose after outlier removal */
    double glucose_percentage_change;          /* derived_ybyy1: Percentage change from Y1 */
    
    /* Stage 4: Group-based linear regression variables */
    double regression_weighted_value = 0.0;    /* y2_coeff_value: derived_y * y2_coeff */
    double group_glucose_estimate = 0.0;       /* y3_value: Glucose estimate from group regression */
    
    /* Stage 5: Elimination correction */
    double elimination_correction = 0.0;       /* elim_value: Correction from lookup matrix */
    double elimination_percentage = 0.0;       /* elim_per: Matrix correction percentage */
    
    /* Algorithm constants from active hardware profile */
    double svasth_intercept = profile->intercept;
    double outlier_k_factor = profile->outlier_k_factor;
    
    /* Working arrays */
    double weighted_coefficients[TOTAL_POINTS];  /* Each sample multiplied by its coefficient */
    double sorted_coefficients[TOTAL_POINTS];     /* Sorted copy for outlier detection (not used in final calc) */
    double outlier_flags[TOTAL_POINTS];           /* Array marking which samples are outliers */
    
    /* Statistical calculation variables */
    double coefficient_mean = 0;        /* Mean of weighted coefficients */
    double coefficient_variance_sum = 0; /* Sum of squared differences for variance */
    double coefficient_variance = 0;     /* Variance of weighted coefficients */
    double coefficient_stddev = 0;       /* Standard deviation of weighted coefficients */
    double temp_swap = 0;                /* Temporary variable for sorting */
    
    /* Final result */
    int predicted_glucose_mg_dl = 0;    /* Final glucose prediction in mg/dL */

    /* ========================================================================
     * Input Validation
     * ======================================================================== */
    if (n != TOTAL_POINTS) {
        LOG_ERR("Invalid sample count: expected %d, got %d", TOTAL_POINTS, n);
        return -1;
    }

    /* ========================================================================
     * Stage 1: Coefficient-Weighted Sum Calculation (Y1)
     * ========================================================================
     * Multiply each ADC sample by its corresponding Svasth coefficient and
     * sum all weighted values. Add the intercept constant to get the initial
     * glucose estimate (Y1).
     */
    for (i = 0; i < TOTAL_POINTS; i++) {
        weighted_coefficients[i] = (float)data[i] * svasth_coeff[i];
        weighted_coeff_sum += weighted_coefficients[i];
    }
    initial_glucose_estimate = weighted_coeff_sum + svasth_intercept;

    /* ========================================================================
     * Stage 2: Statistical Outlier Detection (K-Sigma Method)
     * ========================================================================
     * Calculate mean and standard deviation of weighted coefficients.
     * Use k-sigma method (k=1.75) to identify outliers beyond normal distribution.
     * 
     * Note: Sorting is performed but not strictly necessary for variance calculation,
     * but kept for compatibility with original algorithm.
     */
    
    /* Copy coefficients for sorting (used in original algorithm) */
    for (i = 0; i < TOTAL_POINTS; i++) {
        sorted_coefficients[i] = weighted_coefficients[i];
    }

    /* Bubble sort (for compatibility with original algorithm) */
    for (i = 0; i < TOTAL_POINTS; i++) {
        for (j = i + 1; j < TOTAL_POINTS; j++) {
            if (sorted_coefficients[i] > sorted_coefficients[j]) {
                temp_swap = sorted_coefficients[i];
                sorted_coefficients[i] = sorted_coefficients[j];
                sorted_coefficients[j] = temp_swap;
            }
        }
    }

    /* Calculate mean of weighted coefficients */
    coefficient_mean = weighted_coeff_sum / TOTAL_POINTS;

    /* Calculate variance: sum of squared differences from mean */
    for (i = 0; i < TOTAL_POINTS; i++) {
        coefficient_variance_sum += (weighted_coefficients[i] - coefficient_mean) * 
                                     (weighted_coefficients[i] - coefficient_mean);
    }

    coefficient_variance = coefficient_variance_sum / TOTAL_POINTS;
    coefficient_stddev = sqrt(coefficient_variance);

    /* Calculate outlier detection limits using k-sigma method */
    outlier_lower_limit = coefficient_mean - (outlier_k_factor * coefficient_stddev);
    outlier_upper_limit = coefficient_mean + (outlier_k_factor * coefficient_stddev);

    /* Identify outliers: values outside k-sigma limits */
    for (i = 0; i < TOTAL_POINTS; i++) {
        if (weighted_coefficients[i] <= outlier_lower_limit) {
            /* Negative outlier: value below lower limit */
            outlier_flags[i] = weighted_coefficients[i];
            outlier_negative_sum += weighted_coefficients[i];
            negative_outlier_count++;
        } else if (weighted_coefficients[i] >= outlier_upper_limit) {
            /* Positive outlier: value above upper limit */
            outlier_flags[i] = weighted_coefficients[i];
            outlier_positive_sum += weighted_coefficients[i];
            positive_outlier_count++;
        } else {
            /* Normal value: within acceptable range */
            outlier_flags[i] = 0;
        }
    }

    /* Total outlier sum (positive + negative) */
    double outlier_total_removed = outlier_positive_sum + outlier_negative_sum;

    /* ========================================================================
     * Stage 3: Outlier Elimination and Derived Value Calculation
     * ========================================================================
     * Remove outliers from initial estimate and calculate percentage change.
     * This derived value represents the glucose estimate after outlier correction.
     */
    glucose_derived_value = initial_glucose_estimate - outlier_total_removed;
    
    /* Calculate percentage change: (derived_y / Y1) * 100% */
    glucose_percentage_change = (glucose_derived_value / initial_glucose_estimate) * 100.0;
    
    /* Round to 4 decimal places for precision */
    glucose_percentage_change = round(glucose_percentage_change * 10000.0) / 10000.0;

    /* ========================================================================
     * Stage 4 & 5: Group-Based Linear Regression and Elimination Correction
     * ========================================================================
     * Classify the glucose percentage change into one of 11 groups, each with
     * group-specific linear regression coefficients. Apply the regression to
     * get Y3 estimate, then lookup elimination correction from group matrix.
     * 
     * Uses iteration over group configuration table to eliminate code repetition.
     */
    matrix_match_count = 0;
    elimination_correction = 0.0;
    
    /* Find matching group by iterating through configuration table */
    bool group_found = false;
    for (uint32_t group_idx = 0; group_idx < num_groups; group_idx++) {
        const struct glucose_group_config *config = &group_configs[group_idx];
        
        /* Check if percentage falls within this group's range */
        bool in_range = false;
        if (group_idx == 0) {
            /* Group 1: special case with exclusive upper bound */
            in_range = (glucose_percentage_change >= config->percentage_min && 
                       glucose_percentage_change < config->percentage_max);
        } else {
            /* Groups 2-11: inclusive bounds */
            in_range = (glucose_percentage_change >= config->percentage_min && 
                       glucose_percentage_change <= config->percentage_max);
        }
        
        if (in_range) {
            group_found = true;
            matched_group_idx = group_idx;  /* Store matched group index */
            
            /* Apply group-specific linear regression: Y3 = (derived_y * coeff) + constant */
            regression_weighted_value = glucose_derived_value * config->regression_coefficient;
            group_glucose_estimate = regression_weighted_value + config->regression_constant;
            
            /* Lookup elimination correction in group's matrix */
#ifdef GLUCOSE_MATRICES_RUNTIME_LOOKUP
            {
                float correction_percent = 0.0f;
                uint16_t matched_row = 0;
                int lookup_ret = glucose_matrix_lookup_correction(
                    (uint8_t)(group_idx + 1),
                    (float)group_glucose_estimate,
                    &correction_percent,
                    &matched_row);

                if (lookup_ret == 0) {
                    elimination_percentage = correction_percent;
                    elimination_correction = group_glucose_estimate * (elimination_percentage / 100.0);
                    matrix_match_count = 1;
                    matched_matrix_row = (int)matched_row;
                } else if (lookup_ret == -ERANGE) {
                    LOG_WRN("Matrix group %d: no row matched Y3=%.4f — elim correction skipped",
                            group_idx + 1, group_glucose_estimate);
                } else {
                    LOG_ERR("Matrix lookup failed for group %d: %d", group_idx + 1, lookup_ret);
                    predicted_glucose_mg_dl = (int32_t)(glucose_derived_value);
                    break;
                }
            }
#else
            for (uint32_t matrix_row = 0; matrix_row < config->matrix_rows; matrix_row++) {
                double matrix_lower_bound = MATRIX_GET(group_idx, matrix_row, 0);
                double matrix_upper_bound = MATRIX_GET(group_idx, matrix_row, 1);
                double matrix_correction = MATRIX_GET(group_idx, matrix_row, 2);
                
                /* Check if Y3 value falls within this matrix row's range */
                if (group_glucose_estimate >= matrix_lower_bound && 
                    group_glucose_estimate <= matrix_upper_bound) {
                    elimination_percentage = matrix_correction;
                    elimination_correction = group_glucose_estimate * (elimination_percentage / 100.0);
                    matrix_match_count++;
                    break; /* Found matching range, no need to continue */
                }
            }
#endif
            
            /* Calculate final prediction: Y3 + elimination correction */
            predicted_glucose_mg_dl = (int32_t)(group_glucose_estimate + elimination_correction);
            break; /* Found matching group, no need to check others */
        }
    }
    
    /* Fallback: Use derived value directly if outside all group ranges */
    if (!group_found) {
        predicted_glucose_mg_dl = (int32_t)(glucose_derived_value);
        LOG_WRN("Glucose percentage change (%.4f%%) outside expected range [%.1f%% to %.1f%%], using fallback",
                glucose_percentage_change,
                group_configs[0].percentage_min,
                group_configs[num_groups - 1].percentage_max);
    }

    /* Enhanced validation logging: Output all algorithm values for verification */
    LOG_DBG("Stage 1-3: tot_coeff=%.4f, intercept=%.4f, Y1=%.2f, mean=%.4f, std=%.4f, "
            "ll=%.4f, ul=%.4f, pos_out=+%d, neg_out=-%d, neg_sum=%.4f, pos_sum=%.4f, total_rm=%.4f",
            weighted_coeff_sum, svasth_intercept, initial_glucose_estimate,
            coefficient_mean, coefficient_stddev, outlier_lower_limit,
            outlier_upper_limit, positive_outlier_count, negative_outlier_count,
            outlier_negative_sum, outlier_positive_sum, outlier_total_removed);
    
        LOG_DBG("Stage 4-5: Y2=%.2f, change=%.4f%%, group=%d, y2_coeff=%.4f, y2_val=%.4f, "
            "const=%.4f, Y3=%.4f, mtx_match=%d, elim_per=%.4f, elim=%.4f, predicted=%d mg/dL",
            glucose_derived_value, glucose_percentage_change, 
            matched_group_idx >= 0 ? matched_group_idx + 1 : 0,
            matched_group_idx >= 0 ? group_configs[matched_group_idx].regression_coefficient : 0.0,
            regression_weighted_value,
            matched_group_idx >= 0 ? group_configs[matched_group_idx].regression_constant : 0.0,
            group_glucose_estimate, matrix_match_count, elimination_percentage,
            elimination_correction,
            predicted_glucose_mg_dl);

    /* ========================================================================
     * Populate Details Structure for CSV Logging (if provided)
     * ======================================================================== */
    if (details != NULL) {
        /* Validation fields */
        details->tot_coeff = weighted_coeff_sum;
        details->intercept = svasth_intercept;
        details->y1_value = initial_glucose_estimate;
        
        /* Statistical analysis */
        details->avg_val = coefficient_mean;
        details->std_dev = coefficient_stddev;
        details->up_lim = outlier_upper_limit;
        details->ll_lim = outlier_lower_limit;
        
        /* Outlier detection results */
        details->p_count = positive_outlier_count;
        details->n_count = negative_outlier_count;
        details->p_val = outlier_positive_sum;
        details->n_val = outlier_negative_sum;
        details->p_plus_n = outlier_total_removed;
        
        /* Derived values */
        details->y2_val = glucose_derived_value;
        details->y2_percent = glucose_percentage_change;
        
        /* Group regression (use stored index to avoid redundant search) */
        if (matched_group_idx >= 0) {
            const struct glucose_group_config *config = &group_configs[matched_group_idx];
            details->group_cd = (int32_t)(matched_group_idx + 1);  /* Groups are 1-indexed */
            details->y2_factor = config->regression_coefficient;
            details->y2_factor_val = regression_weighted_value;
            details->const_val = config->regression_constant;
        } else {
            details->group_cd = 0;
            details->y2_factor = 0.0;
            details->y2_factor_val = 0.0;
            details->const_val = 0.0;
        }
        
        /* Y3 and elimination */
        details->y3_value = group_glucose_estimate;
        details->y3_row_no = matrix_match_count > 0 ? matched_matrix_row : 0;
        details->elim_val = elimination_correction;
        details->elim_per = elimination_percentage;
        
        /* Final result */
        details->y_value = predicted_glucose_mg_dl;
    }

    return predicted_glucose_mg_dl;
}

/**
 * @brief Calculate insulin resistance (HOMA-IR) metrics from glucose and insulin
 *
 * Implements standardized insulin resistance calculations including HOMA-IR index
 * and intermediate metrics using the Homeostasis Model Assessment (HOMA-IR) method.
 *
 * Calculation Steps:
 * ==================
 * 1. glucose_baseline_mg_dl: Input baseline glucose measurement
 * 2. calibrated_glucose_mg_dl: Apply calibration adjustment (+5%)
 * 3. normalized_glucose_mmol_l: Normalize to mmol/L physiological units
 * 4. fasting_insulin: Input fasting insulin measurement
 * 5. insulin_correction_factor: Calculate percentage change
 * 6. insulin_sensitivity_ratio: Calculate glucose/insulin ratio
 * 7. inverse_insulin_ratio: Calculate reciprocal ratio for HOMA-IR formula
 * 8. homa_ir_index: Final HOMA-IR index = (inverse_ratio × calibrated_glucose) / 405
 *
 * Reference: Matthews et al. Diabetologia. 1985 Jun;28(6):412-9.
 *
 * @param details Pointer to algorithm details structure to populate
 * @param glucose_mg_dl Measured glucose value in mg/dL
 * @param fasting_insulin_uiu_ml Fasting insulin value in μIU/mL
 * 
 * @return 0 on success, -EINVAL if inputs invalid
 */
int insulin_resistance_calculate(struct glucose_algorithm_details *details,
                                 double glucose_mg_dl,
                                 double fasting_insulin_uiu_ml)
{
    if (details == NULL) {
        return -EINVAL;
    }

    /* Validate inputs */
    if (glucose_mg_dl <= 0.0) {
        LOG_WRN("Invalid glucose value: %.2f mg/dL", glucose_mg_dl);
        return -EINVAL;
    }
    
    if (fasting_insulin_uiu_ml < 0.0) {
        LOG_WRN("Invalid insulin value: %.2f μIU/mL", fasting_insulin_uiu_ml);
        return -EINVAL;
    }

    /* Read configuration values from Kconfig  */
    double calibration_adjustment = INSULIN_CALIBRATION_ADJUSTMENT_PERCENT;
    double normalization_divisor = GLUCOSE_NORMALIZATION_DIVISOR;
    int homa_ir_divisor = HOMA_IR_DIVISOR;
    double threshold_optimal = HOMA_IR_INTERPRETATION_THRESHOLD_OPTIMAL;
    double threshold_normal = HOMA_IR_INTERPRETATION_THRESHOLD_NORMAL;
    double threshold_moderate = HOMA_IR_INTERPRETATION_THRESHOLD_MODERATE;

    /* ========================================================================
     * HOMA-IR Calculation Steps
     * ======================================================================== */

    /* Step 1: Baseline glucose (use glucose value as reference point) */
    double glucose_baseline_mg_dl = glucose_mg_dl;
    details->calibration_factor = glucose_baseline_mg_dl;
    LOG_DBG("glucose_baseline: %.6f mg/dL", glucose_baseline_mg_dl);

    /* Step 2: Apply calibration adjustment (typically +5% correction) */
    double calibrated_glucose_mg_dl = glucose_baseline_mg_dl * calibration_adjustment;
    details->ag_adjusted = calibrated_glucose_mg_dl;
    LOG_DBG("calibrated_glucose: %.6f mg/dL (%.2f%% adjustment)", 
            calibrated_glucose_mg_dl, (calibration_adjustment - 1.0) * 100.0);

    /* Step 3: Normalize glucose to mmol/L range
     *         This converts glucose from mg/dL to mmol/L scale
     *         Formula: normalized = calibrated / normalization_divisor */
    double normalized_glucose_mmol_l = calibrated_glucose_mg_dl / normalization_divisor;
    details->normalized_glucose = normalized_glucose_mmol_l;
    LOG_DBG("normalized_glucose: %.6f mmol/L", normalized_glucose_mmol_l);

    /* Step 4: Use fasting insulin directly (already in µIU/mL) */
    double fasting_insulin = fasting_insulin_uiu_ml;
    details->actual_insulin = fasting_insulin;
    LOG_DBG("fasting_insulin: %.6f μIU/mL", fasting_insulin);

    /* Step 5: Calculate insulin correction factor (percentage change)
     *         Shows how much fasting insulin differs from normalized glucose
     *         Formula: correction = (insulin - normalized) / normalized */
    double insulin_correction_factor = 0.0;
    if (normalized_glucose_mmol_l != 0.0) {
        insulin_correction_factor = (fasting_insulin - normalized_glucose_mmol_l) / normalized_glucose_mmol_l;
        details->insulin_correction = insulin_correction_factor;
        LOG_DBG("insulin_correction_factor: %.6f (percentage change)", insulin_correction_factor);
    } else {
        LOG_WRN("normalized_glucose is zero, cannot calculate insulin correction");
        details->insulin_correction = 0.0;
    }

    /* Step 6: Calculate insulin sensitivity ratio (normalized / fasting)
     *         Shows normalized glucose relative to fasting insulin
     *         Formula: ratio = normalized / fasting, avoid division by zero */
    double insulin_sensitivity_ratio = 0.0;
    if (fasting_insulin != 0.0) {
        insulin_sensitivity_ratio = normalized_glucose_mmol_l / fasting_insulin;
        details->insulin_ratio = insulin_sensitivity_ratio;
        LOG_DBG("insulin_sensitivity_ratio: %.6f", insulin_sensitivity_ratio);
    } else {
        LOG_WRN("fasting_insulin is zero, setting sensitivity ratio to 0");
        details->insulin_ratio = 0.0;
    }

    /* Step 7: Calculate inverse insulin ratio (normalized / sensitivity_ratio)
     *         Reciprocal transformation of the sensitivity ratio
     *         Formula: inverse = normalized / ratio, avoid division by zero */
    double inverse_insulin_ratio = 0.0;
    if (insulin_sensitivity_ratio != 0.0) {
        inverse_insulin_ratio = normalized_glucose_mmol_l / insulin_sensitivity_ratio;
        details->inverse_ratio = inverse_insulin_ratio;
        LOG_DBG("inverse_insulin_ratio: %.6f", inverse_insulin_ratio);
    } else {
        LOG_WRN("insulin_sensitivity_ratio is zero, cannot calculate inverse ratio");
        details->inverse_ratio = 0.0;
    }

    /* Step 8: Calculate HOMA-IR Index (Homeostasis Model Assessment for Insulin Resistance)
     *         Formula: HOMA-IR = (glucose_mg_dl × fasting_insulin) / divisor
     *         The standard divisor is 405 (per Matthews et al., Diabetologia 1985)
     *         HOMA-IR values:
     *         - < optimal threshold: Optimal insulin sensitivity
     *         - optimal to normal threshold: Normal insulin sensitivity
     *         - normal to moderate threshold: Moderate insulin resistance
     *         - >= moderate threshold: High insulin resistance */
    double homa_ir_index = 0.0;
    if (inverse_insulin_ratio != 0.0 && calibrated_glucose_mg_dl != 0.0) {
        homa_ir_index = (inverse_insulin_ratio * calibrated_glucose_mg_dl) / homa_ir_divisor;
        details->homa_ir_index = homa_ir_index;
        LOG_INF("HOMA-IR Index: %.6f", homa_ir_index);
        
        /* Log HOMA-IR interpretation based on configured thresholds */
        if (homa_ir_index < threshold_optimal) {
            LOG_INF("HOMA-IR Interpretation: Optimal insulin sensitivity (HOMA-IR < %.1f)", threshold_optimal);
        } else if (homa_ir_index < threshold_normal) {
            LOG_INF("HOMA-IR Interpretation: Normal insulin sensitivity (HOMA-IR %.1f-%.1f)", 
                    threshold_optimal, threshold_normal);
        } else if (homa_ir_index < threshold_moderate) {
            LOG_INF("HOMA-IR Interpretation: Moderate insulin resistance (HOMA-IR %.1f-%.1f)", 
                    threshold_normal, threshold_moderate);
        } else {
            LOG_INF("HOMA-IR Interpretation: High insulin resistance (HOMA-IR >= %.1f)", threshold_moderate);
        }
    } else {
        LOG_WRN("Cannot calculate HOMA-IR: inverse_ratio=%.6f, calibrated_glucose=%.6f", 
                inverse_insulin_ratio, calibrated_glucose_mg_dl);
        details->homa_ir_index = 0.0;
    }

    /* Log summary of calculations */
    LOG_INF("=== Insulin Resistance (HOMA-IR) Calculation Complete ===");
    LOG_INF("Input: Glucose=%.2f mg/dL, Fasting Insulin=%.2f μIU/mL", 
            glucose_mg_dl, fasting_insulin_uiu_ml);
    LOG_INF("Processing: Calibrated=%.6f, Normalized=%.6f mmol/L", 
            calibrated_glucose_mg_dl, normalized_glucose_mmol_l);
    LOG_INF("Ratios: Sensitivity=%.6f, Inverse=%.6f", 
            insulin_sensitivity_ratio, inverse_insulin_ratio);
    LOG_INF("Output: HOMA-IR Index=%.6f", homa_ir_index);

    return 0;
}

#ifdef GLUCOSE_MATRICES_RUNTIME_LOOKUP
/**
 * @brief Validate the active XIP glucose model slot.
 * @return 0 on success
 */
int glucose_algorithm_init(void)
{
    int ret;

    ret = glucose_matrix_files_validate();
    if (ret < 0) {
        LOG_ERR("Glucose matrix validation failed: %d", ret);
        return ret;
    }

    LOG_INF("Glucose algorithm initialized (XIP matrices)");
    return 0;
}

void glucose_algorithm_cleanup(void)
{
}
#endif /* GLUCOSE_MATRICES_RUNTIME_LOOKUP */
