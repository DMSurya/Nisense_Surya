/**
 * @file lis2ds12_regs.h
 * @brief LIS2DS12 Accelerometer - Register definitions and hub passthrough helpers
 *
 * Provides:
 * - Complete LIS2DS12 register map
 * - Transparent read/write wrappers that hide the hub passthrough layer
 *
 * Usage:
 *   uint8_t val;
 *   lis2ds12_read_reg(hub_dev, LIS2DS12_REG_WHO_AM_I, &val);
 *   lis2ds12_write_reg(hub_dev, LIS2DS12_REG_CTRL1, 0x30);
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIS2DS12_REGS_H_
#define LIS2DS12_REGS_H_

#include <zephyr/device.h>
#include "max32664_priv.h"

/* Hub sensor index for LIS2DS12 (always 0x04 on MAX32664 GWEC) */
#define HUB_SENSOR_IDX_LIS2DS12     0x04

/* ============================================================================
 * LIS2DS12 Register Map
 * ============================================================================ */

#define LIS2DS12_REG_WHO_AM_I       0x0F
#define LIS2DS12_REG_CTRL1          0x20
#define LIS2DS12_REG_CTRL2          0x21
#define LIS2DS12_REG_CTRL3          0x22
#define LIS2DS12_REG_CTRL4          0x23
#define LIS2DS12_REG_CTRL5          0x24
#define LIS2DS12_REG_FIFO_CTRL      0x25
#define LIS2DS12_REG_STATUS         0x27
#define LIS2DS12_REG_OUT_X_L        0x28
#define LIS2DS12_REG_OUT_X_H        0x29
#define LIS2DS12_REG_OUT_Y_L        0x2A
#define LIS2DS12_REG_OUT_Y_H        0x2B
#define LIS2DS12_REG_OUT_Z_L        0x2C
#define LIS2DS12_REG_OUT_Z_H        0x2D

/* CTRL1 register bit positions */
#define LIS2DS12_ODR_POS            4
#define LIS2DS12_FS_POS             2
#define LIS2DS12_HF_ODR_POS         1
#define LIS2DS12_BDU_POS            0

/* Output data rates (CTRL1 ODR[3:0]) */
#define LIS2DS12_ODR_POWER_DOWN     0x00
#define LIS2DS12_ODR_1HZ            0x01
#define LIS2DS12_ODR_12_5HZ         0x02
#define LIS2DS12_ODR_25HZ           0x03
#define LIS2DS12_ODR_50HZ           0x04
#define LIS2DS12_ODR_100HZ          0x05
#define LIS2DS12_ODR_200HZ          0x06
#define LIS2DS12_ODR_400HZ          0x07
#define LIS2DS12_ODR_800HZ          0x08

/* Full scale selection (CTRL1 FS[1:0]) */
#define LIS2DS12_FS_2G              0x00
#define LIS2DS12_FS_4G              0x02
#define LIS2DS12_FS_8G              0x03
#define LIS2DS12_FS_16G             0x01

/* WHO_AM_I values */
#define LIS2DS12_WHOAMI_VALUE       0x43
#define LIS2DS12_WHOAMI_ALT         0x1D

/* Sensitivity values in micro-g/LSB for 14-bit output */
#define LIS2DS12_SENS_2G            61    /* 0.061 mg/LSB */
#define LIS2DS12_SENS_4G            122   /* 0.122 mg/LSB */
#define LIS2DS12_SENS_8G            244   /* 0.244 mg/LSB */
#define LIS2DS12_SENS_16G           488   /* 0.488 mg/LSB */

/* Gravity constant (m/s²) in micro units */
#define GRAVITY_MS2                 9806650

/* ============================================================================
 * Transparent Register Access Wrappers
 *
 * These inline functions hide the hub passthrough layer so callers
 * simply pass (hub_dev, register, data) without repeating the sensor index.
 * ============================================================================ */

/**
 * @brief Read a LIS2DS12 register via hub passthrough
 *
 * @param hub  MAX32664 hub device
 * @param reg  LIS2DS12 register address
 * @param val  Pointer to store the read value
 * @return 0 on success, negative errno on failure
 */
static inline int lis2ds12_read_reg(const struct device *hub,
				    uint8_t reg, uint8_t *val)
{
	return max32664_read_sensor_reg(hub, HUB_SENSOR_IDX_LIS2DS12, reg, val);
}

/**
 * @brief Write a LIS2DS12 register via hub passthrough
 *
 * @param hub  MAX32664 hub device
 * @param reg  LIS2DS12 register address
 * @param val  Value to write
 * @return 0 on success, negative errno on failure
 */
static inline int lis2ds12_write_reg(const struct device *hub,
				     uint8_t reg, uint8_t val)
{
	return max32664_write_sensor_reg(hub, HUB_SENSOR_IDX_LIS2DS12, reg, val);
}

#endif /* LIS2DS12_REGS_H_ */
