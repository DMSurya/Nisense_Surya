/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nav_button.h"

#include "hw_button.h"

#include <zephyr/autoconf.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(nav_button, LOG_LEVEL_INF);

typedef void (*nav_button_handler_t)(enum hw_button_event ev);

static nav_button_handler_t s_handler;
static struct hw_button s_btn;

#define SW1_NODE DT_ALIAS(sw1)

#if DT_NODE_HAS_STATUS(SW1_NODE, okay)
static const struct gpio_dt_spec s_gpio = GPIO_DT_SPEC_GET(SW1_NODE, gpios);
#define HAS_NAV_BTN 1
#else
#define HAS_NAV_BTN 0
#endif

static void forward_event(enum hw_button_event ev, void *user_data)
{
	ARG_UNUSED(user_data);

	if (s_handler != NULL) {
		s_handler(ev);
	}
}

int nav_button_init(void)
{
#if !HAS_NAV_BTN
	LOG_WRN("Nav button (SW1) not in device tree");
	return -ENOTSUP;
#else
	uint32_t hold_ms = CONFIG_UI_MEASURE_LONG_PRESS_MS;

	s_handler = NULL;
	int ret = hw_button_init(&s_btn, &s_gpio, hold_ms, forward_event, NULL);

	if (ret == 0) {
		LOG_INF("Nav button SW1 ready on %s pin %u",
			s_gpio.port->name, s_gpio.pin);
	} else {
		LOG_ERR("Nav button SW1 init failed on %s pin %u: %d",
			s_gpio.port->name, s_gpio.pin, ret);
	}
	return ret;
#endif
}

void nav_button_set_handler(void (*handler)(enum hw_button_event ev))
{
	s_handler = handler;
}

void nav_button_set_hold_ms(uint32_t hold_ms)
{
	hw_button_set_hold_ms(&s_btn, hold_ms);
}
