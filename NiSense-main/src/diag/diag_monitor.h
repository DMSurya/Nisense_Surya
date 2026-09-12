/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Diagnostics Monitor
 *
 * Low-priority background thread that periodically dumps:
 *   - Per-thread stack high-watermark and CPU usage (thread analyzer)
 *   - System heap runtime stats (free / allocated / max allocated)
 *   - Zephyr memslabs (if enabled)
 *
 * Intended for RAM tuning / memory stress observation during development.
 * Controlled by CONFIG_APP_FEATURE_DIAG_MONITOR (master) and
 * Kconfig.diag_monitor / conf/features/diag_monitor.conf.
 */

#ifndef NISENSE_DIAG_MONITOR_H_
#define NISENSE_DIAG_MONITOR_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Force an immediate diagnostics dump (stack + heap).
 *
 * Safe to call from any preemptible thread context. If
 * CONFIG_APP_FEATURE_DIAG_MONITOR is disabled this is a no-op.
 */
void diag_monitor_dump_now(void);

#ifdef __cplusplus
}
#endif

#endif /* NISENSE_DIAG_MONITOR_H_ */
