/* buzzer.h - PWM Buzzer Control for P0.12
 *
 * Provides tone generation using PWM for the buzzer.
 * Uses PWM1 instance to avoid conflicts with I2C0.
 */

#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Common tone frequencies (Hz) */
#define BUZZER_TONE_C4  262
#define BUZZER_TONE_D4  294
#define BUZZER_TONE_E4  330
#define BUZZER_TONE_F4  349
#define BUZZER_TONE_G4  392
#define BUZZER_TONE_A4  440
#define BUZZER_TONE_B4  494
#define BUZZER_TONE_C5  523

/* Alarm/notification tones */
#define BUZZER_TONE_BEEP    1000  /* 1 kHz beep */
#define BUZZER_TONE_ALARM   2000  /* 2 kHz alarm */
#define BUZZER_TONE_ERROR   500   /* 500 Hz low tone */

/**
 * @brief Initialize buzzer PWM driver
 *
 * @return 0 on success, negative errno otherwise
 */
int buzzer_init(void);

/**
 * @brief Play a tone at specified frequency (non-blocking)
 *
 * Starts tone and automatically stops after duration_ms using a timer.
 * Returns immediately without blocking. If duration_ms is 0, tone plays
 * indefinitely until manual buzzer_stop() call.
 *
 * @param frequency_hz Frequency in Hz (20-20000 Hz range)
 * @param duty_cycle_percent Duty cycle (0-100%)
 * @param duration_ms Duration in milliseconds (0 = indefinite)
 * @return 0 on success, negative errno otherwise
 */
int buzzer_play_tone(uint32_t frequency_hz, uint8_t duty_cycle_percent, uint32_t duration_ms);

/**
 * @brief Stop buzzer (set PWM to 0% duty cycle)
 *
 * @return 0 on success, negative errno otherwise
 */
int buzzer_stop(void);

/**
 * @brief Check if buzzer is currently playing
 *
 * @return true if playing, false if stopped
 */
bool buzzer_is_playing(void);

/**
 * @brief Set global buzzer volume scale (0-100%).
 *
 * Scales duty cycle for subsequent tone playback.
 */
int buzzer_set_volume_percent(uint8_t percent);

/**
 * @brief Get current buzzer volume scale (0-100%).
 */
uint8_t buzzer_get_volume_percent(void);

/**
 * @brief Play success pattern (2-tone ascending: C5 → E4)
 * Non-blocking sequence: 40ms C5, 60ms pause, 40ms E4
 */
void buzzer_play_success(void);

/**
 * @brief Play error pattern (3 short beeps)
 * Blocking sequence: 100ms beep, 50ms pause (×3)
 */
void buzzer_play_error(void);

/**
 * @brief Play warning pattern (2 short beeps)
 * Blocking sequence: 100ms beep, 50ms pause (×2)
 */
void buzzer_play_warning(void);
#ifdef __cplusplus
}
#endif
