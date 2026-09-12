/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PMIC PFN1 power button (MCU P1.04 / devicetree sw2).
 */

#ifndef POWER_BUTTON_H_
#define POWER_BUTTON_H_

#include "hw_button.h"

#ifdef __cplusplus
extern "C" {
#endif

int power_button_init(void);

/** Register UI/policy handler (called from ui_buttons_init). */
void power_button_set_handler(void (*handler)(enum hw_button_event ev));

void power_button_set_hold_ms(uint32_t hold_ms);

#ifdef __cplusplus
}
#endif

#endif /* POWER_BUTTON_H_ */
