/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SW1 screen navigation button (MCU P0.29 / devicetree sw1).
 */

#ifndef NAV_BUTTON_H_
#define NAV_BUTTON_H_

#include "hw_button.h"

#ifdef __cplusplus
extern "C" {
#endif

int nav_button_init(void);

/** Register UI handler (called from ui_buttons_init). */
void nav_button_set_handler(void (*handler)(enum hw_button_event ev));

void nav_button_set_hold_ms(uint32_t hold_ms);

#ifdef __cplusplus
}
#endif

#endif /* NAV_BUTTON_H_ */
