/**
 * @file product_hw_variant.h
 * @brief Runtime product SKU detection: Watch vs Pulse
 *
 * Single source of truth for hardware variant. PPG, glucose models, BLE
 * product name, etc. all consume product_hw_detect() — not glucose-specific.
 */

#ifndef PRODUCT_HW_VARIANT_H_
#define PRODUCT_HW_VARIANT_H_

#ifdef __cplusplus
extern "C" {
#endif

enum product_hw_variant {
	PRODUCT_HW_WATCH = 0, /**< MAX32664C + MAX86141 (wearable) */
	PRODUCT_HW_PULSE,     /**< MAX32664D and/or MAX3010x (finger / pulse) */
};

/**
 * @brief Detect product SKU once and cache the result.
 *
 * Watch when max32664_hub is ready and hub variant == C; else Pulse.
 * Optional force: CONFIG_PRODUCT_HW_VARIANT_FORCE_WATCH / _PULSE
 * (or legacy CONFIG_GLUCOSE_HW_VARIANT_FORCE_WEARABLE / _PULSE).
 */
enum product_hw_variant product_hw_detect(void);

/** @return "WATCH" or "PULSE". */
const char *product_hw_variant_name(enum product_hw_variant variant);

#ifdef __cplusplus
}
#endif

#endif /* PRODUCT_HW_VARIANT_H_ */
