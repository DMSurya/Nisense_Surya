/* device_prefs.c - Apply brightness/volume preferences to hardware */

#include <errno.h>
#include "device_prefs.h"
#include "buzzer.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(device_prefs, LOG_LEVEL_INF);

#if DT_NODE_HAS_STATUS(DT_NODELABEL(lcd_vled), okay)
static const struct gpio_dt_spec lcd_vled_gpio =
	GPIO_DT_SPEC_GET(DT_NODELABEL(lcd_vled), enable_gpios);
#define HAS_LCD_VLED 1
#else
#define HAS_LCD_VLED 0
#endif

int device_prefs_apply_brightness(uint8_t percent)
{
	if (percent > 100U) {
		return -EINVAL;
	}

#if HAS_LCD_VLED
	if (!gpio_is_ready_dt(&lcd_vled_gpio)) {
		LOG_WRN("LCD VLED GPIO not ready");
		return -ENODEV;
	}

	/* Drive P0.04 enable directly. regulator_disable() alone does not turn
	 * the backlight off when regulator-boot-on left refcnt >= 1 and BLE init
	 * called regulator_enable() again. */
	return gpio_pin_set_dt(&lcd_vled_gpio, percent > 0U ? 1 : 0);
#else
	ARG_UNUSED(percent);
	LOG_DBG("LCD VLED GPIO not present in device tree");
	return -ENOTSUP;
#endif
}

int device_prefs_apply_volume(uint8_t percent)
{
	if (percent > 100U) {
		return -EINVAL;
	}

	return buzzer_set_volume_percent(percent);
}
