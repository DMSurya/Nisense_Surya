/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Central power / battery policy for NiSense (MAX20360 + 380 mAh Li-ion).
 *
 * Owns periodic fuel-gauge sampling, display idle sleep (touch wake), and
 * low-battery protection. BLE / UI / health scheduler consume cached state
 * instead of polling I2C independently.
 */
#ifndef POWER_MGR_H_
#define POWER_MGR_H_

#include <stdbool.h>
#include <stdint.h>

#include "power_batt.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Policy state (derived from fuel-gauge SOC and charger status). */
enum power_mgr_state {
	POWER_MGR_STATE_NORMAL = 0,
	POWER_MGR_STATE_LOW,       /* SOC below low threshold — warn, slow cadence */
	POWER_MGR_STATE_CRITICAL,  /* SOC very low — display off, no auto health */
	POWER_MGR_STATE_CHARGING,  /* USB present, recovering from critical */
};

/** KM20 Megnus 380 mAh / 3.7 V — recommended defaults when config unset. */
#define POWER_MGR_DEFAULT_POLL_INTERVAL_S   10U
#define POWER_MGR_DEFAULT_SLEEP_TIMEOUT_S   60U
#define POWER_MGR_DEFAULT_SLEEP_SOC_PCT     5U   /* critical: stop heavy work */
#define POWER_MGR_DEFAULT_LOW_SOC_PCT       15U  /* warn */
#define POWER_MGR_DEFAULT_RESUME_SOC_PCT    20U  /* exit critical while charging */

int power_mgr_init(void);

/** Call from main loop (~20 ms); handles display idle timeout. */
void power_mgr_tick(void);

/** User interaction (touch, button) — resets display idle timer and wakes panel. */
void power_mgr_activity_notify(void);

/** Reload intervals/thresholds after BLE or config file changes. */
void power_mgr_apply_config(void);

enum power_mgr_state power_mgr_get_state(void);
bool power_mgr_display_asleep(void);

/** Cached SOC (0–100) or negative errno. */
int power_mgr_get_soc(int *soc_percent);

/** Latest snapshot (no I2C if cache is fresh). */
int power_mgr_get_snapshot(struct power_batt_snapshot *snap);

/** Milliseconds between battery polls / PMIC BLE publishes. */
uint32_t power_mgr_poll_interval_ms(void);

/** False when optical health cycles should be suppressed. */
bool power_mgr_allow_heavy_work(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_MGR_H_ */
