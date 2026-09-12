/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * MAX20360 fuel gauge (ModelGauge m5 / MAX17260) helpers for power_batt / diag.
 */
#ifndef MAX20360_FG_H_
#define MAX20360_FG_H_

#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * MAX17260 Status bit3 (Bst): 0 = battery present, 1 = battery absent.
 * Use with care — polarity is opposite of a naive "bit set = good" check.
 */
#define MAX20360_FG_STATUS_BST  BIT(3)

#ifdef __cplusplus
extern "C" {
#endif

/** Read raw STATUS register (0x00). */
int max20360_fg_read_status(const struct device *dev, uint16_t *status);

/** Read DieTemp (34h) in °C. */
int max20360_fg_read_die_temp_c(const struct device *dev, int16_t *temp_c);

/** Read AvgTA (16h) thermistor average in °C. */
int max20360_fg_read_avg_ta_c(const struct device *dev, int16_t *temp_c);

#ifdef __cplusplus
}
#endif

#endif /* MAX20360_FG_H_ */
