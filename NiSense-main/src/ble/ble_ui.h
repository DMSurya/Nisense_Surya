/* =============================================================================
 * BLE UI Screen - Header
 * =============================================================================
 * BLE pairing and connection status UI screen
 * Displays passkey during pairing, connection status, and device info
 * ============================================================================= */

#ifndef BLE_UI_H
#define BLE_UI_H

#include <lvgl.h>
#include "ble_gatt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create BLE pairing/status UI screen
 * @param screen LVGL screen object to populate
 */
void ble_ui_create_screen(lv_obj_t *screen);

/**
 * @brief Update BLE UI screen with current status
 */
void ble_ui_update_screen(void);

/**
 * @brief Cleanup BLE UI screen resources
 */
void ble_ui_cleanup_screen(void);

/**
 * @brief Forward a pairing-state event to the BLE screen.
 *
 * Called by the always-active global pairing callback (in ui.c) so the BLE
 * screen renders the current passkey / pairing state even when it was not the
 * screen that registered the callback.
 *
 * @param state   Current pairing state
 * @param passkey 6-digit passkey to display (valid for CONFIRM/WAITING states)
 */
void ble_ui_handle_pairing_event(enum ble_pairing_state state, uint32_t passkey);

#ifdef __cplusplus
}
#endif

#endif /* BLE_UI_H */
