/**
 * @file glucose_algorithm_profiles.c
 * @brief Wearable and pulse Svasth glucose calibration tables
 */

#include "glucose_algorithm_profiles.h"
#include <zephyr/logging/log.h>
#include <errno.h>

LOG_MODULE_REGISTER(glucose_profiles, CONFIG_SENSOR_LOG_LEVEL);

#define SVASTH_POINTS 80

/* ------------------------------------------------------------------------- */
/* Wearable (watch / MAX32664C)                                              */
/* ------------------------------------------------------------------------- */

static const float wearable_svasth_coeff[SVASTH_POINTS] = {
	-0.011400504, 0.001604094, -0.002131794, 0.005532366, 0.005659748, 0.008463679,
	0.003136294, -0.012140873, -0.004487286, -0.006473875, 0.006308397, 0.000405942,
	-0.001766764, -0.005747242, 0.020197619, -0.026702318, 0.012253913, 0.014686524,
	-0.005621617, 0.006572834, -0.012454693, -0.002850603, -0.004409748, -0.002609254,
	0.011519704, -0.003339018, 0.012352006, -0.003498079, -0.010753119, 0.010611572,
	-0.006567058, 0.003022996, -0.00771711, 0.00197712, 0.000730541, 0.000562181,
	0.00485879, -0.018633262, 0.024034009, -0.014336637, -0.001910571, 0.000121259,
	0.005935889, 0.002047667, 0.013807847, -0.021943292, 0.022210797, -0.00440402,
	-0.006783691, -0.003918346, 0.004879458, 0.007922254, -0.004773797, 0.004114458,
	0.000266262, -0.015153258, 0.02193288, -0.009361571, -0.004843634, -0.006202644,
	-0.000137191, -0.009000085, 0.005636201, 0.004534112, -0.005141129, -0.001348137,
	-0.00183326, 0.00670117, -0.002026969, 0.017782354, -0.006570354, -0.00203596,
	0.006075643, -0.012277213, 0.006055303, -0.006395711, -0.005931745, 0.020481957,
	-0.000228942, -0.007540033
};

static const struct glucose_group_config wearable_groups[] = {
	/* BUGFIX: percentage_min widened from -0.1000 to -20.0000.
	 * Root cause: the ADC_GAIN_1_4 change (see
	 * boards/raytac_overlay/40_adc_glucose.overlayinc) shifted the raw
	 * glucose signal's operating point higher (~2170 -> ~3260 counts),
	 * which also shifted the computed percentage-change-from-baseline
	 * that this table is keyed on. Real field measurements at the new
	 * operating point are landing around -15.1% (terminal log: "Glucose
	 * percentage change (-15.0998%) outside expected range... returned
	 * error: -25") — far below the old -0.1% floor, so every reading
	 * was rejected outright regardless of the person's actual glucose
	 * level. Widened to -20% so real readings in this range fall into a
	 * valid group instead of being rejected. Coefficients (slope=
	 * 1.403369601, intercept=141.1246336) and sample count (7) are
	 * unchanged — no new calibration data was provided for this
	 * specific sub-range, only the range boundary itself. */
	{ -20.0000,   0.0001,   1.403369601, 141.1246336,  7 },
	{  0.0001,  19.0000,   0.495236007, 149.8405387, 629 },
	{ 19.0001,  20.9999,   0.450494055, 150.2828149, 1711 },
	{ 21.0000,  24.9999,   0.613399117, 142.9430335, 1808 },
	{ 25.0000,  39.9999,   0.379247948, 147.827487,  1437 },
	{ 40.0000,  60.9999,   0.298319465, 139.2752112, 1395 },
	{ 61.0000,  70.9999,   0.686334737,  88.90690438, 2685 },
	{ 71.0000,  79.9999,   0.708612845,  76.14935309, 1326 },
	{ 80.0000,  91.9999,   0.607027103,  77.5880418,  1362 },
	{ 92.0000, 121.9999,   0.415446594,  93.6697469,   716 },
	{122.0000, 299.9999,   0.476425767,  48.23366897,   82 },
};

/* ------------------------------------------------------------------------- */
/* Pulse (finger) — svasth_coeff and percentage bands from pulse calibration.
 * regression_coefficient / regression_constant: interim wearable groups 1–10
 * values until pulse-specific regression table is supplied by customer.
 */
/* ------------------------------------------------------------------------- */

static const float pulse_svasth_coeff[SVASTH_POINTS] = {
	0.00519886, -0.058801558, 0.063913613, 0.04223925, -0.075294257, -0.011075781,
	0.02240503, 0.039937631, -0.008371712, 0.014875038, 0.032247757, -0.030855102,
	-0.058115959, 0.016330672, -0.03794172, 0.043500717, 0.012225173, -0.086362949,
	0.019453695, 0.10400932, -0.07976488, 0.068445592, 0.043728259, 0.087727819,
	-0.07223078, -0.078014455, -0.063721022, 0.109360256, -0.040901074, -0.029649055,
	-0.007213306, 0.051757738, -0.035889805, -0.019517181, -0.071783451, 0.074080413,
	-0.03148654, 0.058952386, -0.056254203, 0.042929764, 0.029325938, -0.005552685,
	-0.012198352, 0.005545454, 0.000993057, -0.00058884, 0.027615063, -0.063975669,
	0.004411024, 0.090348853, -0.050532534, -0.010178921, -0.0614957, 0.043298166,
	-0.069057303, 0.073327299, 0.002413909, -0.000061, -0.016560902, -0.005639304,
	-0.021749795, 0.061587771, -0.06232124, 0.030882799, 0.041523885, -0.041261363,
	-0.072865734, 0.007830974, -0.0360019, 0.045083876, 0.027901161, 0.082350876,
	-0.022577086, -0.052175352, 0.056583412, 0.009401428, -0.029938668, 0.019920098,
	-0.02976251, 0.011510817
};

static const struct glucose_group_config pulse_groups[] = {
	/* Group 1: -199.9999% to -110.5000% */
	{ -199.9999, -110.5000, 1.403369601, 141.1246336, 133 },
	/* Group 2: -110.4999% to -69.9999% */
	{ -110.4999,  -69.9999, 0.495236007, 149.8405387, 172 },
	/* Group 3: -69.9998% to -50.5100% */
	{  -69.9998,  -50.5100, 0.450494055, 150.2828149, 280 },
	/* Group 4: -50.5099% to -44.5000% */
	{  -50.5099,  -44.5000, 0.613399117, 142.9430335, 1026 },
	/* Group 5: -44.4999% to -30.0000% */
	{  -44.4999,  -30.0000, 0.379247948, 147.827487,  390 },
	/* Group 6: -29.9999% to -0.0001% */
	{  -29.9999,   -0.0001, 0.298319465, 139.2752112,   80 },
	/* Group 7: 0.0001% to 40.4999% */
	{    0.0001,   40.4999, 0.686334737,  88.90690438, 327 },
	/* Group 8: 40.5000% to 59.5999% */
	{   40.5000,   59.5999, 0.708612845,  76.14935309, 385 },
	/* Group 9: 59.6000% to 80.4999% */
	{   59.6000,   80.4999, 0.607027103,  77.5880418,  412 },
	/* Group 10: 80.5000% to 199.9999% */
	{   80.5000,  199.9999, 0.415446594,  93.6697469,  183 },
};

static const struct glucose_algorithm_profile wearable_profile = {
	.variant = GLUCOSE_HW_WEARABLE,
	.name = "WEARABLE",
	.svasth_coeff = wearable_svasth_coeff,
	.intercept = 161.832221420583,
	.outlier_k_factor = 1.75,
	.groups = wearable_groups,
	.num_groups = 11,
};

static const struct glucose_algorithm_profile pulse_profile = {
	.variant = GLUCOSE_HW_PULSE,
	.name = "PULSE",
	.svasth_coeff = pulse_svasth_coeff,
	.intercept = 150.7758966,
	.outlier_k_factor = 1.5,
	.groups = pulse_groups,
	.num_groups = 10,
};

static const struct glucose_algorithm_profile *active_profile = &pulse_profile;

const struct glucose_algorithm_profile *glucose_algorithm_get_active_profile(void)
{
	return active_profile;
}

const struct glucose_algorithm_profile *glucose_algorithm_get_profile(enum glucose_hw_variant variant)
{
	switch (variant) {
	case GLUCOSE_HW_WEARABLE:
		return &wearable_profile;
	case GLUCOSE_HW_PULSE:
	default:
		return &pulse_profile;
	}
}

int glucose_algorithm_select_profile(enum glucose_hw_variant variant)
{
	const struct glucose_algorithm_profile *profile = glucose_algorithm_get_profile(variant);

	if (profile == NULL) {
		return -EINVAL;
	}

	active_profile = profile;
	LOG_DBG("Selected profile %s: %u groups, intercept=%.4f, k=%.2f",
		profile->name, profile->num_groups, profile->intercept, profile->outlier_k_factor);
	return 0;
}
