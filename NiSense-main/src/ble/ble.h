/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Application-level BLE integration module.
 * Owns measurement control, PMIC control, connection handling, and periodic
 * PMIC/proximity publish logic that was previously inlined in main.c.
 */

#ifndef APP_BLE_H_
#define APP_BLE_H_

#ifdef CONFIG_APP_FEATURE_BLE

/**
 * @brief Initialise BLE GATT server and register all application callbacks.
 *
 * Calls ble_gatt_init() then registers the measurement-control, PMIC-control
 * and connection callbacks.  Must be called once during application startup.
 *
 * @return 0 on success, negative errno on failure.
 */
int app_ble_init(void);

/**
 * @brief Periodic BLE tick – call from the main loop on every iteration.
 *
 * Publishes a PMIC and proximity snapshot over BLE notifications once per
 * second when a central is connected.
 */
void app_ble_tick(void);

#endif /* CONFIG_APP_FEATURE_BLE */

#endif /* APP_BLE_H_ */
