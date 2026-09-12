/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * @brief Pause LVGL XIP while QSPI NOR program/erase is in progress
 *
 * record_store (and any other NOR writers) hold this refcount around
 * flash write/erase so main skips LVGL (XIP fonts/icons) briefly.
 */

#ifndef NOR_QSPI_BUSY_H_
#define NOR_QSPI_BUSY_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** True while a NOR writer owns QSPI program/erase (pause LVGL XIP). */
bool nor_qspi_busy(void);

/** Increment busy depth (main skips ui_update / lv_timer_handler). */
void nor_qspi_busy_acquire(void);

/** Decrement busy depth. */
void nor_qspi_busy_release(void);

#ifdef __cplusplus
}
#endif

#endif /* NOR_QSPI_BUSY_H_ */
