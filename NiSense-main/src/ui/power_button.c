/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "power_button.h"

#include "hw_button.h"

#include <zephyr/autoconf.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(power_button, LOG_LEVEL_INF);

typedef void (*power_button_handler_t)(enum hw_button_event ev);

static power_button_handler_t s_handler;
static struct hw_button s_btn;

#define SW2_NODE DT_ALIAS(sw2)

#if DT_NODE_HAS_STATUS(SW2_NODE, okay)
static const struct gpio_dt_spec s_gpio = GPIO_DT_SPEC_GET(SW2_NODE, gpios);
#define HAS_POWER_BTN 1
#else
#define HAS_POWER_BTN 0
#endif

static void forward_event(enum hw_button_event ev, void *user_data)
{
	ARG_UNUSED(user_data);

	if (s_handler != NULL) {
		s_handler(ev);
	}
}

int power_button_init(void)
{
#if !HAS_POWER_BTN
	LOG_WRN("Power button (PFN1 / sw2) not in device tree");
	return -ENOTSUP;
#else
	uint32_t hold_ms = CONFIG_UI_POWER_LONG_PRESS_MS;

	s_handler = NULL;
	int ret = hw_button_init(&s_btn, &s_gpio, hold_ms, forward_event, NULL);

	if (ret == 0) {
		LOG_INF("Power button SW2/PFN1 ready on %s pin %u",
			s_gpio.port->name, s_gpio.pin);
	} else {
		LOG_ERR("Power button SW2/PFN1 init failed on %s pin %u: %d",
			s_gpio.port->name, s_gpio.pin, ret);
	}
	return ret;
#endif
}

void power_button_set_handler(void (*handler)(enum hw_button_event ev))
{
	s_handler = handler;
}

void power_button_set_hold_ms(uint32_t hold_ms)
{
	hw_button_set_hold_ms(&s_btn, hold_ms);
}
