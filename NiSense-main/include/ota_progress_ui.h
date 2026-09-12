/* =============================================================================
 * On-device OTA progress UI (app LVGL — not MCUboot)
 * =============================================================================
 * Shown while BLE transfers Resource store, glucose model, or SMP firmware into
 * slot1. MCUboot apply after reboot has no LCD (40 KB boot partition).
 * ============================================================================= */

#ifndef OTA_PROGRESS_UI_H
#define OTA_PROGRESS_UI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Begin / show progress overlay for @p phase (e.g. "Resource store"). */
void ota_progress_ui_begin(const char *phase);

/** Update percent 0–100 and optional detail line. Throttle callers to ~2–5%. */
void ota_progress_ui_set(uint8_t percent, const char *detail);

/** Hide overlay; @p success selects success/error styling briefly. */
void ota_progress_ui_end(bool success, const char *msg);

bool ota_progress_ui_is_active(void);

/** Register MCUmgr img upload progress hooks (no-op if DFU disabled). */
void ota_progress_smp_hooks_init(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_PROGRESS_UI_H */
