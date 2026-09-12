/* ui_buttons.h — PMIC PFN1 + SW1 hardware button integration */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Register GPIO IRQ + poll (call from main before ui_init). */
int ui_buttons_gpio_early_init(void);

/** Register UI handlers after LVGL is up (call from ui_init). */
int ui_buttons_init(void);
void ui_buttons_apply_config(void);

#ifdef __cplusplus
}
#endif
