/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * temp.h — Temperature monitoring module.
 *
 * Owns the three temperatures on this product, each read on its own correct
 * sensor channel (the previous temp_ui.c logic guessed DIE_TEMP→AMBIENT for
 * every device):
 *
 *   - SoC Temp        : nRF52840 internal die sensor (SENSOR_CHAN_DIE_TEMP).
 *                       Silicon junction temperature — a thermal/charge signal,
 *                       NOT body temperature.
 *   - Body Temp Wrist : MAX30208 @ 0x53 (SENSOR_CHAN_AMBIENT_TEMP). Continuous,
 *                       wear-gated, auto-sampled (surface / not core).
 *   - Body Temp Finger: MAX30205 @ 0x48 (SENSOR_CHAN_AMBIENT_TEMP). Manual
 *                       spot-check only.
 *
 * Responsibilities: explicit-channel reads, idle periodic sampling + CSV
 * logging, BLE notification, and a clearly-labelled body-temp wellness band
 * (never presented as a core-temperature / fever reading).
 *
 * @author Ponmadasamy Muthuraj <ponmadasamy@live.com>
 */

#ifndef TEMP_H_
#define TEMP_H_

#include <zephyr/kernel.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Sentinel returned for an unavailable temperature (below absolute zero). */
#define TEMP_INVALID_C (-300.0f)

/**
 * @brief Body-temperature wellness band (wrist surface, not core).
 *
 * Wrist body temperature (~28–37 °C typical) is perfusion- and ambient-
 * dependent and is NOT core body temperature. These bands are a wellness
 * indicator only.
 */
enum temp_skin_band {
	TEMP_SKIN_LOW = 0,   /* cold extremity / loose contact */
	TEMP_SKIN_NORMAL,    /* typical worn range */
	TEMP_SKIN_ELEVATED,  /* warm — environment or activity */
	TEMP_SKIN_HIGH,      /* unusually warm (wellness flag) */
	TEMP_SKIN_UNKNOWN,   /* no valid reading */
};

/** Snapshot of the auto-sampled temperatures (wrist + SoC). */
struct temp_snapshot {
	float    soc_c;       /* SoC die temp, or TEMP_INVALID_C */
	float    wrist_c;     /* wrist body temp, or TEMP_INVALID_C */
	bool     soc_valid;
	bool     wrist_valid;
	uint32_t timestamp;   /* Unix seconds (0 if RTC unavailable) */
};

/**
 * @brief Initialise the temperature module.
 *
 * Resolves the three sensor devices, prepares CSV logging (when a filesystem
 * is present), and starts the idle periodic sampling timer.
 *
 * @retval 0 on success (module is usable even if some sensors are absent).
 * @retval -ENODEV if no temperature sensor is available at all.
 */
int temp_init(void);

/**
 * @brief No-op retained for call-site compatibility (was FatFS CSV seed).
 */
int temp_ensure_log_file(void);

/**
 * @brief Read the nRF SoC (die) temperature.
 * @param[out] out_c Celsius on success.
 * @retval 0 on success, negative errno otherwise.
 */
int temp_read_soc(float *out_c);

/**
 * @brief Read the wrist body temperature (MAX30208).
 * @param[out] out_c Celsius on success.
 * @retval 0 on success, negative errno otherwise.
 */
int temp_read_wrist(float *out_c);

/**
 * @brief Read the finger body temperature (MAX30205, manual spot-check).
 * @param[out] out_c Celsius on success.
 * @retval 0 on success, negative errno otherwise.
 */
int temp_read_finger(float *out_c);

/**
 * @brief Take a wrist + SoC snapshot now: read, cache, log, and BLE-notify.
 *
 * Safe to call from thread / work-queue context (performs blocking I2C and,
 * when enabled, filesystem writes). Used by the idle timer and, later, by the
 * health scheduler around each measurement cycle.
 *
 * @retval 0 if at least one sensor produced a valid reading.
 * @retval -EIO if no sensor could be read.
 */
int temp_snapshot_now(void);

/**
 * @brief Get the most recent cached snapshot.
 * @param[out] out Filled with the last snapshot.
 */
void temp_get_last(struct temp_snapshot *out);

/**
 * @brief Classify a body temperature into a wellness band.
 * @param temp_c Body temperature in Celsius.
 * @return Wellness band (see @ref temp_skin_band).
 */
enum temp_skin_band temp_classify_skin(float temp_c);

/**
 * @brief Enable or disable idle periodic sampling.
 *
 * The health scheduler pauses idle sampling while it runs an explicit
 * measurement cycle, then re-enables it afterwards, so the two do not contend
 * for the I2C bus or double-log.
 *
 * @param enable true to resume idle sampling, false to pause it.
 */
void temp_set_idle_logging(bool enable);

/**
 * @brief Set idle temperature log interval (seconds).
 *
 * Clamped to 10–3600. Restarts the idle timer. Used by BLE sampling config.
 *
 * @param sec Desired period; 0 restores CONFIG_APP_TEMP_IDLE_INTERVAL_SEC.
 * @return Applied interval in seconds.
 */
uint16_t temp_set_idle_interval_sec(uint16_t sec);

/** Current idle temperature log interval in seconds. */
uint16_t temp_get_idle_interval_sec(void);

/**
 * @brief Last VCNL3040 wear classification (-1 unknown, 0 off, 1 loose, 2 good).
 */
int temp_get_wear_state(void);

#ifdef __cplusplus
}
#endif

#endif /* TEMP_H_ */
