/* power.h : MAX20360 Power and Regulator Initialization
 *
 * Handles enabling all Buck/LDO rails from the MAX20360 PMIC.
 * Safe to call at early boot before display/touch are configured.
 */

#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/drivers/fuel_gauge.h>
#include <zephyr/logging/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize all power rails required for the board.
 *
 *  - Configures Buck1 (VDD_CORE)
 *  - Configures Buck2 (VDD_IO)
 *  - Configures Buck‑Boost (VCC_5V)
 *  - Verifies read‑back voltages
 *
 * @retval 0 on success, negative errno otherwise
 */
int power_init(void);

/**
 * @brief Test power rail voltages.
 *
 * Reads back the configured voltages of each buck rail and prints them out.
 */
void power_test(void);

/* UI Screen Functions (power_ui.c) */
#include <lvgl.h>

/**
 * @brief Create regulator control UI screen
 *
 * @param screen LVGL screen object to populate
 */
void power_ui_create_regulator_screen(lv_obj_t *screen);

/**
 * @brief Update regulator UI screen with latest data
 */
void power_ui_update_regulator_screen(void);

/**
 * @brief Cleanup regulator UI screen resources
 */
void power_ui_cleanup_regulator_screen(void);

/**
 * @brief Create branded charging UI screen (logo + SOC)
 *
 * @param screen LVGL screen object to populate
 */
void power_ui_create_charger_screen(lv_obj_t *screen);

/**
 * @brief Update charger UI screen with latest data
 */
void power_ui_update_charger_screen(void);

/**
 * @brief Cleanup charger UI screen resources
 */
void power_ui_cleanup_charger_screen(void);

#if defined(CONFIG_UI_SCREEN_CHARGER)
/**
 * @brief Auto-navigate to/from charging screen on USB plug events.
 *
 * Call from the LVGL/UI tick path. Rising USB edge opens the charging screen;
 * falling edge restores the prior screen if still on charger.
 */
void power_ui_charge_nav_poll(void);
#endif

/**
 * @brief Create power monitor UI screen (voltage & current from fuel gauge)
 *
 * @param screen LVGL screen object to populate
 */
#if defined(CONFIG_UI_SCREEN_POWER)
void power_ui_create_power_screen(lv_obj_t *screen);

/**
 * @brief Update power UI screen with latest data
 */
void power_ui_update_power_screen(void);

/**
 * @brief Cleanup power UI screen resources
 */
void power_ui_cleanup_power_screen(void);
#endif

#ifdef __cplusplus
}
#endif
