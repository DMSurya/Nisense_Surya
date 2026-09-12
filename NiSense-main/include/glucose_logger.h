/**
 * @file glucose_logger.h
 * @brief Glucose measurement logger — NOR record store + home summary.
 */

#ifndef GLUCOSE_LOGGER_H_
#define GLUCOSE_LOGGER_H_

#include <zephyr/drivers/sensor/glucose_sensor.h>
#include <stdint.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

int glucose_logger_init(void);

/** @brief No-op retained for call-site compatibility (was FatFS CSV seed). */
int glucose_logger_ensure_file(void);

/**
 * @brief Persist a glucose measurement to the NOR record store.
 */
int glucose_logger_log_measurement(const struct glucose_sensor_result *result);

int glucose_logger_get_current_file(char *buf, size_t buf_len);
int glucose_logger_close_file(void);
int glucose_logger_get_fs_stats(uint64_t *total_bytes, uint64_t *free_bytes);

#ifdef __cplusplus
}
#endif

#endif /* GLUCOSE_LOGGER_H_ */
