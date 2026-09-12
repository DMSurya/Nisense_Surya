/*
 * @file glucose_sensor.h
 * @brief System call handlers for glucose sensor driver (stub)
 *
 * NOTE: Zephyr auto-generates syscall handlers for in-tree drivers.
 * For out-of-tree drivers, syscalls require additional build system
 * configuration. This file provides stub implementations that may need
 * adjustment based on your Zephyr build configuration.
 *
 * If userspace support is not required, this file can be ignored.
 * If userspace support is required, you may need to:
 * 1. Add syscall definitions to syscall_list.txt
 * 2. Configure build system to generate syscall stubs
 * 3. Or implement syscall handlers manually
 */

#ifndef ZEPHYR_INCLUDE_SYSCALLS_GLUCOSE_SENSOR_H_
#define ZEPHYR_INCLUDE_SYSCALLS_GLUCOSE_SENSOR_H_

#include <zephyr/drivers/sensor/glucose_sensor.h>
#include <zephyr/syscall.h>

#if defined(CONFIG_USERSPACE)

/* Stub implementations - adjust based on your Zephyr version and build config */
static inline int z_vrfy_glucose_sensor_trigger_measurement(const struct device *dev)
{
	Z_OOPS(Z_SYSCALL_OBJ(dev, K_OBJ_DRIVER_GLUCOSE_SENSOR));
	return z_impl_glucose_sensor_trigger_measurement(dev);
}
#define z_vrfy_glucose_sensor_trigger_measurement z_vrfy_glucose_sensor_trigger_measurement

static inline int z_vrfy_glucose_sensor_set_callback(const struct device *dev,
						       glucose_sensor_callback_t callback,
						       void *user_data)
{
	Z_OOPS(Z_SYSCALL_OBJ(dev, K_OBJ_DRIVER_GLUCOSE_SENSOR));
	return z_impl_glucose_sensor_set_callback(dev, callback, user_data);
}
#define z_vrfy_glucose_sensor_set_callback z_vrfy_glucose_sensor_set_callback

static inline int z_vrfy_glucose_sensor_get_result(const struct device *dev,
						     struct glucose_sensor_result *result)
{
	Z_OOPS(Z_SYSCALL_OBJ(dev, K_OBJ_DRIVER_GLUCOSE_SENSOR));
	Z_OOPS(Z_SYSCALL_MEMORY_WRITE(result, sizeof(struct glucose_sensor_result)));
	return z_impl_glucose_sensor_get_result(dev, result);
}
#define z_vrfy_glucose_sensor_get_result z_vrfy_glucose_sensor_get_result

static inline int z_vrfy_glucose_sensor_configure(const struct device *dev,
						    uint16_t num_samples,
						    uint16_t delay_ms)
{
	Z_OOPS(Z_SYSCALL_OBJ(dev, K_OBJ_DRIVER_GLUCOSE_SENSOR));
	return z_impl_glucose_sensor_configure(dev, num_samples, delay_ms);
}
#define z_vrfy_glucose_sensor_configure z_vrfy_glucose_sensor_configure

static inline int z_vrfy_glucose_sensor_calibrate(const struct device *dev,
						    float calibration_voltage_mv)
{
	Z_OOPS(Z_SYSCALL_OBJ(dev, K_OBJ_DRIVER_GLUCOSE_SENSOR));
	return z_impl_glucose_sensor_calibrate(dev, calibration_voltage_mv);
}
#define z_vrfy_glucose_sensor_calibrate z_vrfy_glucose_sensor_calibrate

static inline int z_vrfy_glucose_sensor_get_state(const struct device *dev)
{
	Z_OOPS(Z_SYSCALL_OBJ(dev, K_OBJ_DRIVER_GLUCOSE_SENSOR));
	return z_impl_glucose_sensor_get_state(dev);
}
#define z_vrfy_glucose_sensor_get_state z_vrfy_glucose_sensor_get_state

static inline int z_vrfy_glucose_sensor_get_config(const struct device *dev,
						    uint16_t *num_samples,
						    uint16_t *delay_ms)
{
	Z_OOPS(Z_SYSCALL_OBJ(dev, K_OBJ_DRIVER_GLUCOSE_SENSOR));
	if (num_samples != NULL) {
		Z_OOPS(Z_SYSCALL_MEMORY_WRITE(num_samples, sizeof(*num_samples)));
	}
	if (delay_ms != NULL) {
		Z_OOPS(Z_SYSCALL_MEMORY_WRITE(delay_ms, sizeof(*delay_ms)));
	}
	return z_impl_glucose_sensor_get_config(dev, num_samples, delay_ms);
}
#define z_vrfy_glucose_sensor_get_config z_vrfy_glucose_sensor_get_config

static inline int z_vrfy_glucose_sensor_trigger_set(const struct device *dev,
						     const struct sensor_trigger *trig,
						     sensor_trigger_handler_t handler)
{
	Z_OOPS(Z_SYSCALL_OBJ(dev, K_OBJ_DRIVER_GLUCOSE_SENSOR));
	if (trig != NULL) {
		Z_OOPS(Z_SYSCALL_MEMORY_READ(trig, sizeof(*trig)));
	}
	/* Callback installs are supervisor-only — reject non-NULL from userspace. */
	Z_OOPS(handler != NULL);
	return z_impl_glucose_sensor_trigger_set(dev, trig, handler);
}
#define z_vrfy_glucose_sensor_trigger_set z_vrfy_glucose_sensor_trigger_set

static inline int z_vrfy_glucose_sensor_get_sample_data(const struct device *dev,
							 struct glucose_sample_data *sample)
{
	Z_OOPS(Z_SYSCALL_OBJ(dev, K_OBJ_DRIVER_GLUCOSE_SENSOR));
	Z_OOPS(Z_SYSCALL_MEMORY_WRITE(sample, sizeof(*sample)));
	return z_impl_glucose_sensor_get_sample_data(dev, sample);
}
#define z_vrfy_glucose_sensor_get_sample_data z_vrfy_glucose_sensor_get_sample_data

static inline int z_vrfy_glucose_sensor_set_fasting_insulin(const struct device *dev,
							     float insulin_uiu_ml)
{
	Z_OOPS(Z_SYSCALL_OBJ(dev, K_OBJ_DRIVER_GLUCOSE_SENSOR));
	return z_impl_glucose_sensor_set_fasting_insulin(dev, insulin_uiu_ml);
}
#define z_vrfy_glucose_sensor_set_fasting_insulin z_vrfy_glucose_sensor_set_fasting_insulin

static inline int z_vrfy_glucose_sensor_get_fasting_insulin(const struct device *dev,
							     float *insulin_uiu_ml)
{
	Z_OOPS(Z_SYSCALL_OBJ(dev, K_OBJ_DRIVER_GLUCOSE_SENSOR));
	Z_OOPS(Z_SYSCALL_MEMORY_WRITE(insulin_uiu_ml, sizeof(*insulin_uiu_ml)));
	return z_impl_glucose_sensor_get_fasting_insulin(dev, insulin_uiu_ml);
}
#define z_vrfy_glucose_sensor_get_fasting_insulin z_vrfy_glucose_sensor_get_fasting_insulin

#endif /* CONFIG_USERSPACE */

#endif /* ZEPHYR_INCLUDE_SYSCALLS_GLUCOSE_SENSOR_H_ */
