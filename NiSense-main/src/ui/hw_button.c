/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "hw_button.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(hw_button, LOG_LEVEL_INF);

#define HOLD_POLL_MS   50U
#define DEBOUNCE_MS    30U
#define POLL_MS        50U

static void schedule_hold_poll(struct hw_button *btn)
{
	(void)k_work_reschedule(&btn->hold_work, K_MSEC(HOLD_POLL_MS));
}

static void hold_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct hw_button *btn = CONTAINER_OF(dwork, struct hw_button, hold_work);

	if (!btn->pressed) {
		return;
	}

	uint32_t elapsed = k_uptime_get_32() - btn->press_ms;

	if (!btn->hold_complete && elapsed >= btn->hold_ms) {
		btn->hold_complete = true;
		if (btn->cb != NULL) {
			btn->cb(HW_BUTTON_HOLD_COMPLETE, btn->user_data);
		}
	} else if (!btn->hold_armed && elapsed >= HW_BUTTON_HOLD_ARM_MS) {
		btn->hold_armed = true;
		if (btn->cb != NULL) {
			btn->cb(HW_BUTTON_HOLD_ARMED, btn->user_data);
		}
	}

	if (btn->pressed && !btn->hold_complete) {
		schedule_hold_poll(btn);
	}
}

static void process_level(struct hw_button *btn, bool down)
{
	if (down && !btn->pressed) {
		btn->pressed = true;
		btn->hold_complete = false;
		btn->hold_armed = false;
		btn->press_ms = k_uptime_get_32();
		if (btn->cb != NULL) {
			btn->cb(HW_BUTTON_PRESSED, btn->user_data);
		}
		schedule_hold_poll(btn);
		return;
	}

	if (!down && btn->pressed) {
		btn->pressed = false;
		(void)k_work_cancel_delayable(&btn->hold_work);

		uint32_t elapsed = k_uptime_get_32() - btn->press_ms;

		if (btn->hold_complete) {
			if (btn->cb != NULL) {
				btn->cb(HW_BUTTON_RELEASE_AFTER_HOLD, btn->user_data);
			}
		} else if (btn->hold_armed) {
			/* Hold UI was shown — release aborts, do not also fire SHORT */
			if (btn->cb != NULL) {
				btn->cb(HW_BUTTON_HOLD_CANCEL, btn->user_data);
			}
		} else if (elapsed < HW_BUTTON_SHORT_MAX_MS) {
			if (btn->cb != NULL) {
				btn->cb(HW_BUTTON_SHORT, btn->user_data);
			}
		} else if (btn->cb != NULL) {
			btn->cb(HW_BUTTON_HOLD_CANCEL, btn->user_data);
		}

		btn->hold_complete = false;
		btn->hold_armed = false;
	}
}

static void sample_button(struct hw_button *btn)
{
	int logical;
	bool down;

	if (btn->gpio == NULL) {
		return;
	}

	logical = gpio_pin_get_dt(btn->gpio);
	if (logical < 0) {
		return;
	}

	down = logical > 0;
	process_level(btn, down);
}

static void debounce_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct hw_button *btn = CONTAINER_OF(dwork, struct hw_button, debounce_work);

	sample_button(btn);
}

static void poll_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct hw_button *btn = CONTAINER_OF(dwork, struct hw_button, poll_work);

	sample_button(btn);

	if (btn->registered) {
		(void)k_work_reschedule(&btn->poll_work, K_MSEC(POLL_MS));
	}
}

static void gpio_handler(const struct device *port, struct gpio_callback *cb,
			 uint32_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(pins);

	struct hw_button *btn = CONTAINER_OF(cb, struct hw_button, gpio_cb);

	if (btn->gpio == NULL) {
		return;
	}

	(void)k_work_reschedule(&btn->debounce_work, K_MSEC(DEBOUNCE_MS));
}

int hw_button_init(struct hw_button *btn, const struct gpio_dt_spec *gpio,
		   uint32_t hold_ms, hw_button_cb_t cb, void *user_data)
{
	int raw;
	int logical;

	if (btn == NULL || gpio == NULL || cb == NULL) {
		return -EINVAL;
	}

	if (btn->registered) {
		return 0;
	}

	if (!gpio_is_ready_dt(gpio)) {
		return -ENODEV;
	}

	btn->gpio = gpio;
	btn->cb = cb;
	btn->user_data = user_data;
	btn->hold_ms = hold_ms;
	btn->pressed = false;
	btn->hold_complete = false;
	btn->hold_armed = false;

	k_work_init_delayable(&btn->hold_work, hold_work_handler);
	k_work_init_delayable(&btn->debounce_work, debounce_work_handler);
	k_work_init_delayable(&btn->poll_work, poll_work_handler);

	int ret = gpio_pin_configure_dt(gpio, GPIO_INPUT);

	if (ret != 0) {
		return ret;
	}

	ret = gpio_pin_interrupt_configure_dt(gpio, GPIO_INT_EDGE_BOTH);
	if (ret != 0) {
		return ret;
	}

	gpio_init_callback(&btn->gpio_cb, gpio_handler, BIT(gpio->pin));
	ret = gpio_add_callback(gpio->port, &btn->gpio_cb);
	if (ret != 0) {
		return ret;
	}

	raw = gpio_pin_get(gpio->port, gpio->pin);
	logical = gpio_pin_get_dt(gpio);
	btn->registered = true;

	LOG_INF("Button %s pin %u: raw=%d logical=%d (%s), hold %u ms",
		gpio->port->name, gpio->pin, raw, logical,
		logical > 0 ? "pressed" : "released", hold_ms);

	(void)k_work_schedule(&btn->poll_work, K_MSEC(POLL_MS));
	return 0;
}

void hw_button_set_hold_ms(struct hw_button *btn, uint32_t hold_ms)
{
	if (btn != NULL && hold_ms >= 300U) {
		btn->hold_ms = hold_ms;
	}
}
