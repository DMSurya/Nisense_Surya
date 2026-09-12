/* resource_logo.h - Runtime-resolved Resource store logos (boot / charge) */

#pragma once

#include <lvgl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Resolve logo descriptors from the Resource table.
 *
 * Call after resource_init() and before UI splash / charger screen creation.
 */
void resource_logo_init(void);

#if defined(CONFIG_UI_STARTUP_LOGO)
/** Boot splash logo (RESOURCE_ID_LOGO_BOOT). data is NULL if unresolved. */
extern lv_image_dsc_t logo;
#endif

#if defined(CONFIG_UI_SCREEN_CHARGER)
/** Charging screen logo (RESOURCE_ID_LOGO_CHARGE). */
extern lv_image_dsc_t logo_charge;

/** True when logo_charge.data points at a valid Resource mmap payload. */
bool resource_logo_charge_ready(void);
#endif

#ifdef __cplusplus
}
#endif
