/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * MAX20360 IVMON multiplexer control, exported by the regulator driver.
 *
 * The IVMON mux register (IVMONCfg @ 0x1A) lives in the PMIC register map
 * (7-bit 0x28).  It is not covered by LockMsk (only regulator VSET and
 * charger EOC are lockable), so selection is a plain I2C write.  The
 * out-of-tree ADC driver (adc_max20360) reads the muxed voltage through the
 * internal SAR ADC on the haptic/ADC slave (7-bit 0x50).
 */

#ifndef ZEPHYR_DRIVERS_REGULATOR_MAX20360_IVMON_H_
#define ZEPHYR_DRIVERS_REGULATOR_MAX20360_IVMON_H_

#include <zephyr/device.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Point the IVMON mux at a rail (or disable it)
 *
 * @param dev   MAX20360 parent regulator device (maxim,max20360-regulator)
 * @param ivmon_cntl  MAX20360_IVMON_CNTL_* channel selector
 * @param ratio       MAX20360_IVMON_RATIO_* divider setting
 * @return 0 on success, negative errno on failure
 */
int max20360_ivmon_select(const struct device *dev, uint8_t ivmon_cntl, uint8_t ratio);

/**
 * @brief Read PMIC Status4 (BatGood, BatRegDone, BstFault) @ 0x05
 *
 * @param dev   MAX20360 parent regulator device (maxim,max20360-regulator)
 * @param status4  Raw Status4 byte
 */
int max20360_read_status4(const struct device *dev, uint8_t *status4);

/**
 * @brief Read PMIC Status3 (SysBatLim, ChgSysLim) @ 0x04
 */
int max20360_read_status3(const struct device *dev, uint8_t *status3);

/** PMIC INT (P0.31) callback — invoked from driver work queue after INT0–INT3 read. */
typedef void (*max20360_pmic_int_cb_t)(const struct device *dev,
				       const uint8_t int_status[4]);

/**
 * @brief Enable MAX20360 INT GPIO and unmask battery/charger alerts.
 *
 * @param dev  Parent regulator device (@c maxim,max20360-regulator / regulators node)
 * @param cb   App callback (may be NULL); keep work minimal
 * @return 0 on success, negative errno on failure
 */
int max20360_pmic_int_init(const struct device *dev, max20360_pmic_int_cb_t cb);

/**
 * @brief Take the shared PMIC I²C mutex (parent regulator / regulators node).
 *
 * Hold across multi-register RMW sequences (charger BatPD probe, unlock/write/lock).
 */
int max20360_pmic_lock(const struct device *dev);

/** @brief Release the shared PMIC I²C mutex. */
void max20360_pmic_unlock(const struct device *dev);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_DRIVERS_REGULATOR_MAX20360_IVMON_H_ */

