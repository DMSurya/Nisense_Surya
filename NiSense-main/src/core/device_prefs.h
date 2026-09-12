/* device_prefs.h - Apply device preference settings from BLE / config */

#ifndef DEVICE_PREFS_H
#define DEVICE_PREFS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Apply display backlight (0 = off, 1–100 = on).
 *
 * Hardware: fixed regulator on GPIO P0.04 (LCD_VLED). No PWM dimming on this PCB.
 */
int device_prefs_apply_brightness(uint8_t percent);

/**
 * @brief Apply buzzer volume scale (0-100%).
 */
int device_prefs_apply_volume(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_PREFS_H */
