/* led_status.h – RGB LED Status Manager
 *
 * Manages RGB LED for system status indication with animations.
 * Uses MAX20360 LED0/1/2 current sinks.
 * Priority-based: Higher priority states override lower ones.
 */

#pragma once

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED status states (in priority order, highest first)
 */
typedef enum {
	LED_STATUS_ERROR,           /* Rapid red blink - fault condition */
	LED_STATUS_BATTERY_CRITICAL,/* Fast red blink - shutdown imminent */
	LED_STATUS_BATTERY_LOW,     /* Slow red blink - charge soon */
	LED_STATUS_CHARGING,        /* Breathing orange/yellow - charging */
	LED_STATUS_CHARGE_COMPLETE, /* Solid green - fully charged */
	LED_STATUS_MEASURING,       /* Blue pulse - sensor measurement active */
	LED_STATUS_BLE_CONNECTED,   /* Brief cyan flash - BLE connected */
	LED_STATUS_IDLE,            /* Off - normal operation */
	LED_STATUS_COUNT
} led_status_t;

/**
 * @brief Initialize LED status manager
 * 
 * Starts background thread for LED animations.
 * 
 * @return 0 on success
 */
int led_status_init(void);

/**
 * @brief Set LED status state
 * 
 * Higher priority states override lower ones.
 * Call with LED_STATUS_IDLE to clear a state.
 * 
 * @param status The status to set
 */
void led_status_set(led_status_t status);

/**
 * @brief Clear a specific status (allow lower priority to show)
 * 
 * @param status The status to clear
 */
void led_status_clear(led_status_t status);

/**
 * @brief Get current active status
 * 
 * @return The highest priority active status
 */
led_status_t led_status_get(void);

/**
 * @brief Trigger a one-shot LED flash
 * 
 * Briefly shows a color then returns to current status.
 * 
 * @param rgb_pattern RGB pattern index (0-7 from rgb.h)
 * @param duration_ms Flash duration in milliseconds
 */
void led_status_flash(int rgb_pattern, uint32_t duration_ms);

/**
 * @brief Force LED off (override all states)
 * 
 * @param force true to force off, false to resume normal operation
 */
void led_status_force_off(bool force);

#ifdef __cplusplus
}
#endif
