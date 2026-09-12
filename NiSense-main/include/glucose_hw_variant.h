/**
 * @file glucose_hw_variant.h
 * @brief Glucose profile / NOR model variant tags (1:1 with product SKU)
 *
 * SKU detect is product_hw_detect() only. This enum exists so NOR model
 * headers and algorithm profiles keep stable WEARABLE/PULSE wire names
 * (numeric values match PRODUCT_HW_WATCH / PRODUCT_HW_PULSE).
 */

#ifndef GLUCOSE_HW_VARIANT_H_
#define GLUCOSE_HW_VARIANT_H_

#include "product_hw_variant.h"

#ifdef __cplusplus
extern "C" {
#endif

enum glucose_hw_variant {
	GLUCOSE_HW_WEARABLE = PRODUCT_HW_WATCH, /**< NOR wearable model */
	GLUCOSE_HW_PULSE = PRODUCT_HW_PULSE,     /**< NOR pulse model */
};

/** Map product SKU → glucose profile / NOR variant (identity cast). */
static inline enum glucose_hw_variant
glucose_hw_variant_from_product(enum product_hw_variant sku)
{
	return (enum glucose_hw_variant)sku;
}

#ifdef __cplusplus
}
#endif

#endif /* GLUCOSE_HW_VARIANT_H_ */
