/**
 * @file product_hw_detect.c
 * @brief Runtime Watch vs Pulse product SKU detection
 */

#include "product_hw_variant.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#if DT_HAS_ALIAS(max32664_hub)
#include <zephyr/drivers/sensor/max32664.h>
#endif

LOG_MODULE_REGISTER(product_hw, LOG_LEVEL_INF);

static enum product_hw_variant cached_variant = PRODUCT_HW_PULSE;
static bool detect_done;

static enum product_hw_variant detect_impl(void)
{
#if IS_ENABLED(CONFIG_PRODUCT_HW_VARIANT_FORCE_WATCH) || \
	IS_ENABLED(CONFIG_GLUCOSE_HW_VARIANT_FORCE_WEARABLE)
	LOG_INF("Product HW: WATCH (Kconfig force)");
	return PRODUCT_HW_WATCH;
#elif IS_ENABLED(CONFIG_PRODUCT_HW_VARIANT_FORCE_PULSE) || \
	IS_ENABLED(CONFIG_GLUCOSE_HW_VARIANT_FORCE_PULSE)
	LOG_INF("Product HW: PULSE (Kconfig force)");
	return PRODUCT_HW_PULSE;
#else
#if DT_HAS_ALIAS(max32664_hub)
	const struct device *hub = DEVICE_DT_GET_OR_NULL(DT_ALIAS(max32664_hub));

	/* The physical MAX32664 hub needs time after power-up to finish its
	 * own internal boot before device_is_ready() means anything. This is
	 * called once, right after power_init(), and the result is cached for
	 * the whole session — a single too-early check can permanently
	 * misidentify a WATCH device as PULSE. Retry with short delays. */
	if (hub != NULL) {
		bool ready = device_is_ready(hub);
		int attempts = 0;

		while (!ready && attempts < 10) {
			k_msleep(50);
			ready = device_is_ready(hub);
			attempts++;
		}

		if (!ready) {
			LOG_WRN("Product HW: hub not ready after %d retries (%d ms)",
				attempts, attempts * 50);
		} else if (attempts > 0) {
			LOG_INF("Product HW: hub became ready after %d retries (%d ms)",
				attempts, attempts * 50);
		}

		if (ready) {
			enum max32664_variant_type variant = max32664_get_variant(hub);

			if (variant == MAX32664_VARIANT_TYPE_C) {
				LOG_INF("Product HW: WATCH (MAX32664 variant C / GWEC ready)");
				return PRODUCT_HW_WATCH;
			}

			LOG_INF("Product HW: PULSE (MAX32664 hub ready, variant %d != C)",
				(int)variant);
			return PRODUCT_HW_PULSE;
		}
	}

	LOG_INF("Product HW: PULSE (MAX32664 hub not ready)");
#else
	LOG_INF("Product HW: PULSE (max32664_hub alias not in devicetree)");
#endif
	return PRODUCT_HW_PULSE;
#endif
}

enum product_hw_variant product_hw_detect(void)
{
	if (!detect_done) {
		cached_variant = detect_impl();
		detect_done = true;
	}

	return cached_variant;
}

const char *product_hw_variant_name(enum product_hw_variant variant)
{
	switch (variant) {
	case PRODUCT_HW_WATCH:
		return "WATCH";
	case PRODUCT_HW_PULSE:
	default:
		return "PULSE";
	}
}
