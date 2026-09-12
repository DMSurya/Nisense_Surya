/**
 * @file max302xx.h
 * @brief Public API for MAX302xx Temperature Sensor
 *
 * Custom attributes and definitions for MAX302xx driver access.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_SENSOR_MAX302XX_H_
#define ZEPHYR_INCLUDE_DRIVERS_SENSOR_MAX302XX_H_

#include <zephyr/drivers/sensor.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MAX302xx custom sensor attributes
 *
 * Custom attributes specific to MAX302xx family sensors.
 * Use with sensor_attr_get() API.
 */
enum max302xx_attribute {
	/**
	 * @brief Unique device ID (6 bytes)
	 *
	 * Returns 6-byte unique device ID for MAX30208/MAX30210.
	 * MAX30205 does not support unique ID (returns -ENOTSUP).
	 *
	 * Usage:
	 *   struct sensor_value id[6];
	 *   sensor_attr_get(dev, SENSOR_CHAN_ALL, SENSOR_ATTR_MAX302XX_UNIQUE_ID, id);
	 *   // id[0].val1 to id[5].val1 contain unique ID bytes
	 */
	SENSOR_ATTR_MAX302XX_UNIQUE_ID = SENSOR_ATTR_PRIV_START,
};

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_SENSOR_MAX302XX_H_ */
