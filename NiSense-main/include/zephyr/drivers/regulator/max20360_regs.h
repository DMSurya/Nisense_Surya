/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Public MAX20360 register definitions used by application modules.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_REGULATOR_MAX20360_REGS_H_
#define ZEPHYR_INCLUDE_DRIVERS_REGULATOR_MAX20360_REGS_H_

#include <zephyr/sys/util.h>

/* PFN pin status/config register */
#define MAX20360_REG_PFN              0x7D

/* PFN2 mode selection (bits [7:4] of MAX20360_REG_PFN) */
#define MAX20360_PFN2_MODE_MASK       GENMASK(7, 4)
#define MAX20360_PFN2_MODE_BATLOW     (0x01 << 4)  /* Battery low output (active low) */

#endif /* ZEPHYR_INCLUDE_DRIVERS_REGULATOR_MAX20360_REGS_H_ */
