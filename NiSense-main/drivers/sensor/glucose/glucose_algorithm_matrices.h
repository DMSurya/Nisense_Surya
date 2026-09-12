/**
 * @file glucose_algorithm_matrices.h
 * @brief Group elimination matrices for Svasth glucose prediction algorithm
 *
 * This file contains the elimination correction matrices for all 11 groups.
 * Each matrix row contains:
 *   - Column 0: Lower bound of glucose value range
 *   - Column 1: Upper bound of glucose value range  
 *   - Column 2: Elimination correction value to apply
 *
 * Matrix source (choice GLUCOSE_MATRIX_SOURCE):
 *   1. EMBEDDED  — const arrays in app flash (~200 KB)
 *   2. FROM_FILE — legacy FatFS CSV (removed stack)
 *   3. XIP       — packed blob on glucose-model QSPI (production default)
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_GLUCOSE_ALGORITHM_MATRICES_H_
#define ZEPHYR_DRIVERS_SENSOR_GLUCOSE_ALGORITHM_MATRICES_H_

#include <stdint.h>
#include <stdbool.h>

/* Matrix column count */
#define GROUP_COL 3

/* Matrix row counts for each group */
#define GROUP_ROW_ONE       7
#define GROUP_ROW_TWO       629
#define GROUP_ROW_THREE     1711
#define GROUP_ROW_FOUR      1808
#define GROUP_ROW_FIVE      1437
#define GROUP_ROW_SIX       1395
#define GROUP_ROW_SEVEN     2685
#define GROUP_ROW_EIGHT     1326
#define GROUP_ROW_NINE      1362
#define GROUP_ROW_TEN       716
#define GROUP_ROW_ELEVEN    82

/* Runtime elimination lookup — XIP packed model backend. */
#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP)
#define GLUCOSE_MATRICES_RUNTIME_LOOKUP 1
#endif

#ifdef GLUCOSE_MATRICES_RUNTIME_LOOKUP

/**
 * @brief Validate that the active XIP model slot is present and usable.
 *
 * @return 0 on success, negative errno on failure
 */
int glucose_matrix_files_validate(void);

/**
 * @brief Lookup elimination correction for a group at a glucose estimate.
 *
 * Returns the correction percentage for the first row where:
 *   lower < glucose_estimate < upper.
 *
 * @param group_num Group number (1-based)
 * @param glucose_estimate Y3 glucose estimate
 * @param correction_percent Output correction percentage (matrix column 3)
 * @param matched_row Output matched row index (0-based), may be NULL
 * @return 0 on match, -ERANGE if no matching range, negative errno on error
 */
int glucose_matrix_lookup_correction(uint8_t group_num,
									 float glucose_estimate,
									 float *correction_percent,
									 uint16_t *matched_row);

#endif /* GLUCOSE_MATRICES_RUNTIME_LOOKUP */

#ifdef CONFIG_GLUCOSE_SENSOR_MATRICES_EMBEDDED

#ifdef CONFIG_GLUCOSE_SENSOR_FULL_ALGORITHM_MATRICES

/* Group 1: Percentage range >= -0.1000% and < 0.0001% */
extern const float group_one_matrix[GROUP_ROW_ONE][GROUP_COL];

/* Group 2: Percentage range 0.0001% to 19.0000% */
extern const float group_two_matrix[GROUP_ROW_TWO][GROUP_COL];

/* Group 3: Percentage range 19.0001% to 20.9999% */
extern const float group_three_matrix[GROUP_ROW_THREE][GROUP_COL];

/* Group 4: Percentage range 21.0000% to 24.9999% */
extern const float group_four_matrix[GROUP_ROW_FOUR][GROUP_COL];

/* Group 5: Percentage range 25.0000% to 39.9999% */
extern const float group_five_matrix[GROUP_ROW_FIVE][GROUP_COL];

/* Group 6: Percentage range 40.0000% to 60.9999% */
extern const float group_six_matrix[GROUP_ROW_SIX][GROUP_COL];

/* Group 7: Percentage range 61.0000% to 70.9999% */
extern const float group_seven_matrix[GROUP_ROW_SEVEN][GROUP_COL];

/* Group 8: Percentage range 71.0000% to 79.9999% */
extern const float group_eight_matrix[GROUP_ROW_EIGHT][GROUP_COL];

/* Group 9: Percentage range 80.0000% to 91.9999% */
extern const float group_nine_matrix[GROUP_ROW_NINE][GROUP_COL];

/* Group 10: Percentage range 92.0000% to 121.9999% */
extern const float group_ten_matrix[GROUP_ROW_TEN][GROUP_COL];

/* Group 11: Percentage range 122.0000% to 299.9999% */
extern const float group_eleven_matrix[GROUP_ROW_ELEVEN][GROUP_COL];

#else

/* Placeholder mode: single-row empty matrices */
extern const float group_one_matrix[GROUP_ROW_ONE][GROUP_COL];
extern const float group_two_matrix[1][GROUP_COL];
extern const float group_three_matrix[1][GROUP_COL];
extern const float group_four_matrix[1][GROUP_COL];
extern const float group_five_matrix[1][GROUP_COL];
extern const float group_six_matrix[1][GROUP_COL];
extern const float group_seven_matrix[1][GROUP_COL];
extern const float group_eight_matrix[1][GROUP_COL];
extern const float group_nine_matrix[1][GROUP_COL];
extern const float group_ten_matrix[1][GROUP_COL];
extern const float group_eleven_matrix[1][GROUP_COL];

#endif /* CONFIG_GLUCOSE_SENSOR_FULL_ALGORITHM_MATRICES */

#endif /* CONFIG_GLUCOSE_SENSOR_MATRICES_EMBEDDED */

#endif /* ZEPHYR_DRIVERS_SENSOR_GLUCOSE_ALGORITHM_MATRICES_H_ */

