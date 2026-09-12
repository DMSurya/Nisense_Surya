/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Active-low GPIO button helper (press / short / long-hold).
 */

#ifndef HW_BUTTON_H_
#define HW_BUTTON_H_

#include <stdint.h>

#include <zephyr/drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum hw_button_event {
	HW_BUTTON_PRESSED,
	HW_BUTTON_HOLD_ARMED,
	HW_BUTTON_SHORT,
	HW_BUTTON_HOLD_COMPLETE,
	HW_BUTTON_HOLD_CANCEL,
	HW_BUTTON_RELEASE_AFTER_HOLD,
};

typedef void (*hw_button_cb_t)(enum hw_button_event ev, void *user_data);

struct hw_button {
	const struct gpio_dt_spec *gpio;
	struct gpio_callback gpio_cb;
	struct k_work_delayable hold_work;
	struct k_work_delayable debounce_work;
	struct k_work_delayable poll_work;
	hw_button_cb_t cb;
	void *user_data;
	uint32_t hold_ms;
	uint32_t press_ms;
	bool pressed;
	bool hold_complete;
	bool hold_armed;
	bool registered;
};

#define HW_BUTTON_SHORT_MAX_MS 400U
/** Delay before HOLD_ARMED (overlay) so a quick tap stays SHORT-only. */
#define HW_BUTTON_HOLD_ARM_MS  250U

int hw_button_init(struct hw_button *btn, const struct gpio_dt_spec *gpio,
		   uint32_t hold_ms, hw_button_cb_t cb, void *user_data);

void hw_button_set_hold_ms(struct hw_button *btn, uint32_t hold_ms);

#ifdef __cplusplus
}
#endif

#endif /* HW_BUTTON_H_ */
