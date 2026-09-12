/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Wi-Fi UI Screen
 *
 * LVGL-based UI for displaying Wi-Fi connection status, scan results,
 * and managing Wi-Fi configuration.
 */

#ifndef WIFI_UI_H_
#define WIFI_UI_H_

#include <lvgl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the Wi-Fi UI screen
 *
 * Creates LVGL screen with connection status, IP address, signal strength,
 * MAC address when connected. Shows available networks list when disconnected.
 *
 * @return Pointer to the created screen, or NULL on failure
 */
lv_obj_t *wifi_ui_create(void);

/**
 * @brief Update the Wi-Fi UI with current status
 *
 * Refreshes connection status, IP address, signal strength, and other info.
 * Should be called periodically (e.g., every 500ms).
 */
void wifi_ui_update(void);

/**
 * @brief Cleanup the Wi-Fi UI screen
 *
 * Frees resources and stops any pending operations.
 */
void wifi_ui_cleanup(void);

/**
 * @brief Start Wi-Fi AP scan
 *
 * Initiates scan for available access points. Results displayed in list.
 */
void wifi_ui_start_scan(void);

/**
 * @brief Get the Wi-Fi UI screen object
 *
 * @return Pointer to the screen object
 */
lv_obj_t *wifi_ui_get_screen(void);

/**
 * @brief Create the Wi-Fi UI screen (dynamic loading)
 *
 * Creates LVGL screen objects dynamically.
 *
 * @param parent Parent screen object
 */
void wifi_ui_create_screen(lv_obj_t *parent);

/**
 * @brief Update the Wi-Fi UI screen (dynamic loading)
 *
 * Updates screen content with current data.
 */
void wifi_ui_update_screen(void);

/**
 * @brief Get current Wi-Fi connection state for global UI shell.
 * @return true if Wi-Fi is connected.
 */
bool wifi_ui_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_UI_H_ */
