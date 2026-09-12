/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Battery read helpers: PMIC ADC + fuel gauge + combined pack detection.
 */
#ifndef POWER_BATT_H_
#define POWER_BATT_H_

#include <zephyr/device.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Li-ion pack: Born Engineers KM20 Megnus class (380 mAh, 3.7 V nominal). */
#define POWER_BATT_DESIGN_CAPACITY_MAH 380
#define POWER_BATT_NOMINAL_VOLTAGE_MV  3700
#define POWER_BATT_FULL_VOLTAGE_MV     4200
#define POWER_BATT_EMPTY_VOLTAGE_MV    3000
/** Reuse window for periodic BLE / scheduler reads (ms). */
#define POWER_BATT_SNAPSHOT_CACHE_MS   5000

/** Pack presence from combined PMIC + fuel gauge heuristics. */
enum power_batt_cell_state {
	POWER_BATT_CELL_UNKNOWN = -1,
	POWER_BATT_CELL_ABSENT = 0,
	POWER_BATT_CELL_PRESENT = 1,
};

/** Snapshot of battery-related readings (PMIC + gauge + charger context). */
struct power_batt_snapshot {
	enum power_batt_cell_state cell_state;
	int32_t pmic_vbat_uv;
	int32_t fg_vcell_uv;
	int32_t current_ua;
	int fg_soc_percent;
	bool usb_online;
	bool bat_good;
	bool bat_reg_done;
	/** True when MAX17260 Status.Bst is set (battery absent). */
	bool fg_pack_absent;
	bool sys_bat_lim;
	/** Slow-changing fuel gauge fields (refreshed at most every 10 s). */
	struct {
		bool valid;
		int16_t temp_c;
		uint16_t cycle_count;
		uint16_t remaining_mah;
		uint16_t full_mah;
		uint16_t design_mah;
		uint16_t time_to_empty_min;
		uint16_t time_to_full_min;
		int16_t avg_current_ma;
	} fg_ext;
};

/** @return Fuel gauge device or NULL. */
const struct device *power_batt_fuel_gauge(void);

/** @return Charger device or NULL. */
const struct device *power_batt_charger(void);

/** @return PMIC regulators parent device or NULL. */
const struct device *power_batt_pmic(void);

/** True when the fuel gauge driver is bound and ready. */
bool power_batt_fuel_gauge_ready(void);

/** True when the charger driver is bound and ready. */
bool power_batt_charger_ready(void);

/** True when the PMIC parent (IVMON/status) is ready. */
bool power_batt_pmic_ready(void);

/**
 * @brief Fill a snapshot with PMIC, gauge, and charger readings.
 *
 * Also evaluates @ref power_batt_cell_state from both sources.
 */
int power_batt_snapshot(struct power_batt_snapshot *snap);

/**
 * @brief Return a recent snapshot without re-hitting I2C when cache is fresh.
 *
 * @param max_age_ms Maximum cache age in milliseconds (0 = always refresh).
 */
int power_batt_snapshot_cached(struct power_batt_snapshot *snap, uint32_t max_age_ms);

/** @return true when a Li+ pack is considered present. */
bool power_batt_cell_present(void);

/** Human-readable pack state for UI ("Present", "Absent", "Unknown"). */
const char *power_batt_cell_state_str(enum power_batt_cell_state state);

/**
 * @brief Read battery pin voltage in microvolts (PMIC internal ADC, IVMON BAT).
 */
int power_batt_voltage_uv(int32_t *voltage_uv);

/** @brief Read fuel gauge VCELL in microvolts (model terminal voltage). */
int power_batt_fg_vcell_uv(int32_t *voltage_uv);

/**
 * @brief Read battery state-of-charge percent (0–100).
 *
 * Uses fuel gauge when a pack is present; returns 0 when absent.
 */
int power_batt_soc_percent(int *soc_percent);

/**
 * @brief SOC from a cached snapshot when available (see power_batt_snapshot_cached).
 */
int power_batt_soc_percent_cached(int *soc_percent, uint32_t max_age_ms);

/**
 * @brief Derive SOC percent from an existing snapshot (no I2C).
 */
int power_batt_soc_from_snapshot(const struct power_batt_snapshot *snap, int *soc_percent);

/**
 * @brief Reconcile gauge SOC with pack presence and PMIC VBAT.
 *
 * When no pack is detected, forces *soc_percent to 0.  Otherwise clamps
 * gross REPSOC vs voltage mismatch.  Returns true if adjusted.
 */
bool power_batt_clamp_soc_to_voltage(uint16_t battery_mv, uint8_t *soc_percent);

#ifdef __cplusplus
}
#endif

#endif /* POWER_BATT_H_ */
