/* rgb.h – MAX20360 RGB LED control abstraction */

#pragma once

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RGB Pattern Count and Names */
#define RGB_PATTERN_COUNT 8

extern const char *rgb_color_names[RGB_PATTERN_COUNT];

/**
 * @brief Initialize MAX20360 LED controller if required
 */
int rgb_init(void);

/**
 * @brief Set LED pattern by index (0–7)
 *
 *  0 – OFF  
 *  1 – Blue  
 *  2 – Red  
 *  3 – Green  
 *  4 – Magenta  
 *  5 – Cyan  
 *  6 – Yellow  
 *  7 – White  
 */
int rgb_set_pattern(int index);

#ifdef __cplusplus
}
#endif
