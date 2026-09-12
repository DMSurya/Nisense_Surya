/*
 * Copyright (c) 2025 NiSense
 * SPDX-License-Identifier: Apache-2.0
 *
 * MAX3010x Unified PPG Driver - Public API
 *
 * Custom sensor attributes for runtime configuration of MAX30101, MAX30102,
 * and MAX30105 pulse oximeter / heart-rate sensors.
 *
 * Include this header from any module that needs to call sensor_attr_set()
 * on a MAX3010x device.
 */

#ifndef MAX3010X_PPG_H_
#define MAX3010X_PPG_H_

#include <zephyr/drivers/sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Custom sensor attributes for MAX3010x driver.
 *
 * Use with sensor_attr_set() on SENSOR_CHAN_ALL:
 * @code
 *   struct sensor_value val = { .val1 = 1 };  // 1 = shutdown
 *   sensor_attr_set(dev, SENSOR_CHAN_ALL,
 *                   (enum sensor_attribute)SENSOR_ATTR_MAX3010X_SHUTDOWN, &val);
 * @endcode
 *
 * Supported on all variants (MAX30101, MAX30102, MAX30105) unless noted.
 */
enum max3010x_ppg_attr {
	/** Enter/exit shutdown mode.  val1=1 → shutdown (LEDs off, <1 uA).
	 *  val1=0 → wakeup (LEDs resume, FIFO flushed). */
	SENSOR_ATTR_MAX3010X_SHUTDOWN = SENSOR_ATTR_PRIV_START,

	/** Set Red LED pulse amplitude.  val1 = 0x00..0xFF (0.2 mA per LSB). */
	SENSOR_ATTR_MAX3010X_LED1_PA,

	/** Set IR LED pulse amplitude.   val1 = 0x00..0xFF (0.2 mA per LSB). */
	SENSOR_ATTR_MAX3010X_LED2_PA,

	/** Set Green LED pulse amplitude. val1 = 0x00..0xFF (0.2 mA per LSB).
	 *  NOTE: Returns -ENOTSUP on MAX30102 (no Green LED). */
	SENSOR_ATTR_MAX3010X_LED3_PA,

	/** Flush FIFO (reset read/write pointers, discard stale data). */
	SENSOR_ATTR_MAX3010X_FIFO_FLUSH,
};

/* ============================================================================
 * Backwards Compatibility Aliases
 *
 * Allow existing code using SENSOR_ATTR_MAX30101_* to work unchanged.
 * New code should use the MAX3010X_* variants.
 * ============================================================================ */

/** @deprecated Use SENSOR_ATTR_MAX3010X_SHUTDOWN instead */
#define SENSOR_ATTR_MAX30101_SHUTDOWN    SENSOR_ATTR_MAX3010X_SHUTDOWN

/** @deprecated Use SENSOR_ATTR_MAX3010X_LED1_PA instead */
#define SENSOR_ATTR_MAX30101_LED1_PA     SENSOR_ATTR_MAX3010X_LED1_PA

/** @deprecated Use SENSOR_ATTR_MAX3010X_LED2_PA instead */
#define SENSOR_ATTR_MAX30101_LED2_PA     SENSOR_ATTR_MAX3010X_LED2_PA

/** @deprecated Use SENSOR_ATTR_MAX3010X_LED3_PA instead */
#define SENSOR_ATTR_MAX30101_LED3_PA     SENSOR_ATTR_MAX3010X_LED3_PA

/** @deprecated Use SENSOR_ATTR_MAX3010X_FIFO_FLUSH instead */
#define SENSOR_ATTR_MAX30101_FIFO_FLUSH  SENSOR_ATTR_MAX3010X_FIFO_FLUSH

#ifdef __cplusplus
}
#endif

#endif /* MAX3010X_PPG_H_ */
