/**
 * @file glucose_algorithm_config.h
 * @brief Glucose algorithm configuration constants from Kconfig and device tree
 *
 * This header provides centralized access to glucose algorithm configuration
 * parameters from Kconfig options and device tree properties. This replaces
 * hardcoded constants throughout the algorithm with configurable values.
 */

#ifndef GLUCOSE_ALGORITHM_CONFIG_H
#define GLUCOSE_ALGORITHM_CONFIG_H

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>

#define GLUCOSE_SENSOR_NODE DT_NODELABEL(glucose_sensor)

/* ============================================================================
 * Algorithm Constants from Kconfig
 * ============================================================================ */

/** @brief Watch Intercept Algorithm Constant
 *  Default: 161.832221420583 (empirically determined from calibration matrix)
 *  This value affects the baseline prediction before corrections are applied.
 */
#define GLUCOSE_ALGORITHM_INTERCEPT_CONSTANT \
    (CONFIG_GLUCOSE_ALGORITHM_INTERCEPT_CONSTANT / 1000.0)

/** @brief K-Sigma Outlier Detection Factor
 *  Default: 1.75 (tuned for sensor noise characteristics)
 *  Higher values (e.g., 3.0) are less aggressive at removing outliers.
 *  Lower values (e.g., 1.0) are more aggressive.
 */
#define GLUCOSE_ALGORITHM_OUTLIER_K_FACTOR \
    (CONFIG_GLUCOSE_ALGORITHM_OUTLIER_K_FACTOR / 1000.0)

/** @brief Glucose Normalization Factor
 *  Divisor for converting baseline glucose to mmol/L normalization.
 *  Default: 18 (empirically derived from glucose calibration data)
 */
#define GLUCOSE_NORMALIZATION_DIVISOR \
    CONFIG_GLUCOSE_NORMALIZATION_DIVISOR

/** @brief HOMA-IR Calculation Divisor
 *  Standard divisor for HOMA-IR calculation: HOMA-IR = (glucose * insulin) / divisor
 *  Default: 405 (standard HOMA-IR formula divisor per Matthews et al. 1985)
 */
#define HOMA_IR_DIVISOR \
    CONFIG_HOMA_IR_DIVISOR

/** @brief Insulin Calibration Adjustment Factor
 *  Multiplied by fasting insulin to account for measurement calibration.
 *  Default: 1.05 (5% adjustment factor)
 */
#define INSULIN_CALIBRATION_ADJUSTMENT_PERCENT \
    (CONFIG_INSULIN_CALIBRATION_ADJUSTMENT_PERCENT / 1000.0)

/** @brief Default Fasting Insulin Level (uIU/mL)
 *  Used when fasting insulin is not provided by user.
 *  Default: 10.0 uIU/mL (typical value from HOMA-IR research)
 *  Range: 2.0-50.0 uIU/mL (typical clinical range)
 */
#define DEFAULT_FASTING_INSULIN_UIU_ML \
    (CONFIG_DEFAULT_FASTING_INSULIN_UIU_ML / 100.0)

/** @brief HOMA-IR Optimal Upper Threshold
 *  Below this value: insulin sensitive (optimal)
 *  Default: 1.0 (standard HOMA-IR cutoff for healthy individuals)
 */
#define HOMA_IR_INTERPRETATION_THRESHOLD_OPTIMAL \
    (CONFIG_HOMA_IR_INTERPRETATION_THRESHOLD_OPTIMAL / 1000.0)

/** @brief HOMA-IR Normal Upper Threshold
 *  Between optimal and this value: normal insulin sensitivity
 *  Default: 2.0 (intermediate value between optimal and impaired)
 */
#define HOMA_IR_INTERPRETATION_THRESHOLD_NORMAL \
    (CONFIG_HOMA_IR_INTERPRETATION_THRESHOLD_NORMAL / 1000.0)

/** @brief HOMA-IR Moderate Upper Threshold
 *  Between normal and this value: moderate insulin resistance
 *  Default: 3.0 (indicates beginning of clinically significant IR)
 */
#define HOMA_IR_INTERPRETATION_THRESHOLD_MODERATE \
    (CONFIG_HOMA_IR_INTERPRETATION_THRESHOLD_MODERATE / 1000.0)

/* ============================================================================
 * Device Tree Configuration Helpers
 * ============================================================================
 */

/** @brief Get algorithm intercept constant from device tree (if available)
 *  Returns Kconfig value if device tree property not available.
 *  Device tree path: /glucose-sensor/algorithm-intercept-constant
 */
static inline double glucose_config_get_intercept_constant(const struct device *dev)
{
#if DT_NODE_HAS_STATUS(GLUCOSE_SENSOR_NODE, okay) && DT_NODE_HAS_PROP(GLUCOSE_SENSOR_NODE, algorithm_intercept_constant)
    return (double)DT_PROP(GLUCOSE_SENSOR_NODE, algorithm_intercept_constant) / 1000.0;
#else
    return GLUCOSE_ALGORITHM_INTERCEPT_CONSTANT;
#endif
}

/** @brief Get outlier k-factor from device tree (if available)
 *  Returns Kconfig value if device tree property not available.
 *  Device tree path: /glucose-sensor/algorithm-outlier-k-factor
 */
static inline double glucose_config_get_outlier_k_factor(const struct device *dev)
{
#if DT_NODE_HAS_STATUS(GLUCOSE_SENSOR_NODE, okay) && DT_NODE_HAS_PROP(GLUCOSE_SENSOR_NODE, algorithm_outlier_k_factor)
    return (double)DT_PROP(GLUCOSE_SENSOR_NODE, algorithm_outlier_k_factor) / 1000.0;
#else
    return GLUCOSE_ALGORITHM_OUTLIER_K_FACTOR;
#endif
}

/** @brief Get HOMA-IR divisor from device tree (if available)
 *  Returns Kconfig value if device tree property not available.
 *  Device tree path: /glucose-sensor/homa-ir-divisor
 */
static inline int glucose_config_get_homa_ir_divisor(const struct device *dev)
{
#if DT_NODE_HAS_STATUS(GLUCOSE_SENSOR_NODE, okay) && DT_NODE_HAS_PROP(GLUCOSE_SENSOR_NODE, homa_ir_divisor)
    return DT_PROP(GLUCOSE_SENSOR_NODE, homa_ir_divisor);
#else
    return HOMA_IR_DIVISOR;
#endif
}

/** @brief Get insulin calibration adjustment from device tree (if available)
 *  Returns Kconfig value if device tree property not available.
 *  Device tree path: /glucose-sensor/insulin-calibration-adjustment
 *  Note: Device tree stores as fixed-point (1050 = 1.05)
 */
static inline double glucose_config_get_insulin_calibration_adjustment(const struct device *dev)
{
#if DT_NODE_HAS_STATUS(GLUCOSE_SENSOR_NODE, okay) && DT_NODE_HAS_PROP(GLUCOSE_SENSOR_NODE, insulin_calibration_adjustment)
    return (double)DT_PROP(GLUCOSE_SENSOR_NODE, insulin_calibration_adjustment) / 1000.0;
#else
    return INSULIN_CALIBRATION_ADJUSTMENT_PERCENT;
#endif
}

/** @brief Get HOMA-IR interpretation threshold for optimal range
 *  Returns Kconfig value if device tree property not available.
 *  Device tree path: /glucose-sensor/homa-ir-threshold-optimal
 */
static inline double glucose_config_get_homa_ir_threshold_optimal(const struct device *dev)
{
#if DT_NODE_HAS_STATUS(GLUCOSE_SENSOR_NODE, okay) && DT_NODE_HAS_PROP(GLUCOSE_SENSOR_NODE, homa_ir_threshold_optimal)
    return (double)DT_PROP(GLUCOSE_SENSOR_NODE, homa_ir_threshold_optimal) / 1000.0;
#else
    return HOMA_IR_INTERPRETATION_THRESHOLD_OPTIMAL;
#endif
}

/** @brief Get HOMA-IR interpretation threshold for normal range
 *  Returns Kconfig value if device tree property not available.
 *  Device tree path: /glucose-sensor/homa-ir-threshold-normal
 */
static inline double glucose_config_get_homa_ir_threshold_normal(const struct device *dev)
{
#if DT_NODE_HAS_STATUS(GLUCOSE_SENSOR_NODE, okay) && DT_NODE_HAS_PROP(GLUCOSE_SENSOR_NODE, homa_ir_threshold_normal)
    return (double)DT_PROP(GLUCOSE_SENSOR_NODE, homa_ir_threshold_normal) / 1000.0;
#else
    return HOMA_IR_INTERPRETATION_THRESHOLD_NORMAL;
#endif
}

/** @brief Get HOMA-IR interpretation threshold for moderate range
 *  Returns Kconfig value if device tree property not available.
 *  Device tree path: /glucose-sensor/homa-ir-threshold-moderate
 */
static inline double glucose_config_get_homa_ir_threshold_moderate(const struct device *dev)
{
#if DT_NODE_HAS_STATUS(GLUCOSE_SENSOR_NODE, okay) && DT_NODE_HAS_PROP(GLUCOSE_SENSOR_NODE, homa_ir_threshold_moderate)
    return (double)DT_PROP(GLUCOSE_SENSOR_NODE, homa_ir_threshold_moderate) / 1000.0;
#else
    return HOMA_IR_INTERPRETATION_THRESHOLD_MODERATE;
#endif
}

#endif /* GLUCOSE_ALGORITHM_CONFIG_H */
