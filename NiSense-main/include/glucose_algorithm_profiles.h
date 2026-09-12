/**
 * @file glucose_algorithm_profiles.h
 * @brief Wearable vs pulse glucose algorithm calibration profiles
 */

#ifndef GLUCOSE_ALGORITHM_PROFILES_H_
#define GLUCOSE_ALGORITHM_PROFILES_H_

#include "glucose_hw_variant.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Per-group linear regression and matrix metadata */
struct glucose_group_config {
	double percentage_min;
	double percentage_max;
	double regression_coefficient;
	double regression_constant;
	uint32_t matrix_rows;
};

/** Full calibration set for one hardware variant */
struct glucose_algorithm_profile {
	enum glucose_hw_variant variant;
	const char *name;
	const float *svasth_coeff;
	double intercept;
	double outlier_k_factor;
	const struct glucose_group_config *groups;
	uint8_t num_groups;
};

const struct glucose_algorithm_profile *glucose_algorithm_get_active_profile(void);
const struct glucose_algorithm_profile *glucose_algorithm_get_profile(enum glucose_hw_variant variant);

/** Select active profile from product_hw_detect() → glucose_hw_variant_from_product(). */
int glucose_algorithm_select_profile(enum glucose_hw_variant variant);

#ifdef __cplusplus
}
#endif

#endif /* GLUCOSE_ALGORITHM_PROFILES_H_ */
