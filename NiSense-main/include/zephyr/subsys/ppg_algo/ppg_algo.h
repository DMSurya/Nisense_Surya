/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem - Public API
 *
 * Zephyr subsystem for Heart Rate and SpO2 calculation from raw PPG sensor data.
 * Consumes data from MAX86141 PPG sensor and LIS2DS12 accelerometer.
 */

#ifndef ZEPHYR_SUBSYS_PPG_ALGO_PPG_ALGO_H_
#define ZEPHYR_SUBSYS_PPG_ALGO_PPG_ALGO_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ppg_algo PPG Algorithm Subsystem
 * @ingroup subsys
 * @{
 */

/**
 * @brief Signal quality levels
 */
enum ppg_signal_quality {
	PPG_QUALITY_NO_SIGNAL = 0,   /**< No finger/contact detected */
	PPG_QUALITY_POOR = 1,        /**< Signal present but too noisy */
	PPG_QUALITY_FAIR = 2,        /**< Acceptable for trending only */
	PPG_QUALITY_GOOD = 3,        /**< Good quality readings */
	PPG_QUALITY_EXCELLENT = 4,   /**< Excellent wearable signal quality */
};

/** Quality reason codes (bitmask) — why a reading is invalid or degraded.
 *  Multiple bits may be set simultaneously; inspect @ref ppg_algo_result::quality_reason.
 */
#define PPG_QUALITY_REASON_OK                 0x00U /**< All checks passed */
#define PPG_QUALITY_REASON_NO_CONTACT         BIT(0) /**< No skin contact detected */
#define PPG_QUALITY_REASON_LOW_PERFUSION      BIT(1) /**< Perfusion index below minimum */
#define PPG_QUALITY_REASON_MOTION             BIT(2) /**< Motion artifact corrupts signal */
#define PPG_QUALITY_REASON_LOW_SNR            BIT(3) /**< SNR below minimum floor */
#define PPG_QUALITY_REASON_OUT_OF_RANGE       BIT(4) /**< Result outside physiologic range */
#define PPG_QUALITY_REASON_INSUFFICIENT_BEATS BIT(5) /**< Too few RR intervals for HR */
#define PPG_QUALITY_REASON_LOW_CONFIDENCE     BIT(6) /**< HR confidence below threshold */
#define PPG_QUALITY_REASON_SPO2_LOW_CONFIDENCE BIT(7) /**< SpO2 confidence below threshold —
							  kept separate from BIT(6): these used to
							  share one bit, so a chronically low HR
							  confidence silently invalidated SpO2 too,
							  even when SpO2's own confidence was fine
							  (confirmed on-device: spo2_valid=0 at
							  spo2 conf=100%, solely because hr conf
							  was 20%). See CHANGES doc, 2026-08-29. */

/**
 * @brief Measurement type for PPG processing
 */
enum ppg_measurement_type {
	PPG_MEAS_VITALS = 0, /**< Unified: HR + SpO2 + Hb + RespRate in one session */
};

/**
 * @brief PPG source capability flags (bitmask returned by
 *        ppg_algo_get_capabilities()).
 *
 * Both currently-supported chips (MAX30102 and MAX86141) can do HR and SpO2,
 * but only the hub path exposes motion rejection via the host-fed feeder.
 * UI code can use this to gray-out or hide features that the currently
 * selected hardware cannot deliver.
 */
#define PPG_CAP_HR               BIT(0)  /**< Heart rate measurement */
#define PPG_CAP_SPO2             BIT(1)  /**< Pulse oximetry */
#define PPG_CAP_GREEN_LED        BIT(2)  /**< Dedicated green LED channel */
#define PPG_CAP_MOTION_REJECTION BIT(3)  /**< Accelerometer-fed motion rejection */
#define PPG_CAP_HEMOGLOBIN       BIT(4)  /**< Total hemoglobin estimation */
#define PPG_CAP_RESPIRATION      BIT(5)  /**< Respiration rate estimation */

/**
 * @brief PPG algorithm result structure
 */
struct ppg_algo_result {
	/* Heart Rate */
	uint16_t hr_bpm;              /**< Heart rate in BPM (0 = invalid) */
	uint8_t hr_confidence;        /**< HR confidence 0-100% */
	bool hr_valid;                /**< HR reading is valid */
	
	/* SpO2 */
	uint16_t spo2_percent;        /**< SpO2 percentage (0 = invalid) */
	uint8_t spo2_confidence;      /**< SpO2 confidence 0-100% */
	bool spo2_valid;              /**< SpO2 reading is valid */
	
	/* Signal Quality */
	enum ppg_signal_quality quality;  /**< Overall signal quality */
	uint8_t perfusion_index;      /**< Perfusion index (PI) x10, e.g., 25 = 2.5% */
	uint16_t snr_db;              /**< Signal-to-noise ratio x10 dB */
	
	/* Motion */
	bool motion_detected;         /**< Motion artifact detected */
	uint16_t motion_magnitude;    /**< Accel magnitude (mg) */

	/* Quality reasons */
	uint8_t quality_reason;       /**< Bitmask of PPG_QUALITY_REASON_* flags (0 = all clear) */

	/* Raw metrics (for debugging/research) */
	uint32_t dc_red;              /**< DC component of red channel */
	uint32_t dc_ir;               /**< DC component of IR channel */
	uint32_t dc_green;            /**< DC component of green channel */
	uint32_t ac_red;              /**< AC amplitude of red channel */
	uint32_t ac_ir;               /**< AC amplitude of IR channel */
	uint32_t ac_green;            /**< AC amplitude of green channel */
	bool green_valid;             /**< Green channel has usable pulsatile signal */
	uint16_t r_value_x1000;       /**< R value * 1000 for SpO2 */

	/* Hemoglobin (experimental — empiric green/IR model; not clinical) */
	uint16_t hb_g_dl_x10;         /**< Total hemoglobin g/dL × 10 (142 = 14.2 g/dL) */
	uint8_t  hb_confidence;       /**< Hb confidence 0-100% */
	bool     hb_valid;            /**< Hb reading is valid */

	/* Respiration */
	uint8_t  resp_rate_bpm;       /**< Respiration rate in breaths/min */
	uint8_t  resp_confidence;     /**< Resp confidence 0-100% */
	bool     resp_valid;          /**< Resp reading is valid */

	/* HRV from PPG R-R (experimental PRV — not ECG HRV; Task Force windows) */
	uint16_t sdnn_ms;             /**< SDNN in ms (sample stddev of RR) */
	uint16_t rmssd_ms;            /**< RMSSD in ms */
	bool     hrv_valid;           /**< HRV valid (≥20 RR intervals) */

	/* Cuffless BP estimate (experimental; PPG morphology + HR; not clinical) */
	uint16_t systolic_mmhg;       /**< Estimated systolic mmHg */
	uint16_t diastolic_mmhg;      /**< Estimated diastolic mmHg */
	bool     bp_valid;            /**< BP estimate valid */

	/* Timing */
	uint32_t timestamp_ms;        /**< Result timestamp (uptime) */
	uint16_t rr_interval_ms;      /**< Last R-R interval in ms */
	
	uint16_t sample_count;        /**< Number of samples processed */
};

/**
 * @brief Per-sample data for real-time monitoring
 */
struct ppg_sample_data {
	uint16_t sample_number;       /**< Sample index in current window */
	uint16_t total_samples;       /**< Total samples in window */
	uint32_t raw_red;             /**< Raw red LED ADC value */
	uint32_t raw_ir;              /**< Raw IR LED ADC value */
	uint32_t raw_green;           /**< Raw green LED ADC value */
	uint32_t dc_red;              /**< DC component red */
	uint32_t dc_ir;               /**< DC component IR */
	uint32_t dc_green;            /**< DC component green */
	uint32_t ac_red;              /**< AC amplitude red */
	uint32_t ac_ir;               /**< AC amplitude IR */
	uint32_t ac_green;            /**< AC amplitude green */
	int16_t accel_x;              /**< Accel X (mg) */
	int16_t accel_y;              /**< Accel Y (mg) */
	int16_t accel_z;              /**< Accel Z (mg) */
	uint32_t timestamp_ms;        /**< Sample timestamp */
};

/**
 * @brief Trigger types for PPG events
 *
 * Append-only: SAMPLE_READY=0 and MEASUREMENT_COMPLETE=1 stay stable for
 * existing handlers. Staged-live triggers follow when CONFIG_PPG_ALGO_STAGED_LIVE.
 */
enum ppg_trigger_type {
	PPG_TRIG_SAMPLE_READY = 0,         /**< Fires after each sample */
	PPG_TRIG_MEASUREMENT_COMPLETE = 1, /**< Session end (after live tail) */
	PPG_TRIG_PARTIAL_VITALS = 2,       /**< Early HR/SpO2 ready (staged live) */
	PPG_TRIG_PARTIAL_VASCULAR = 3,     /**< Hb/BP/Resp floor (staged live) */
	PPG_TRIG_RECORD_READY = 4,         /**< Authoritative NOR/record window done */
	PPG_TRIG_COUNT,
};

/**
 * @brief Trigger callback function type
 *
 * @param type Trigger type that fired
 * @param user_data User-provided context data
 */
typedef void (*ppg_trigger_handler_t)(enum ppg_trigger_type type, void *user_data);

/**
 * @brief PPG subsystem configuration
 */
struct ppg_algo_config {
	uint16_t sample_rate_hz;      /**< Target sample rate  */
	uint16_t sample_count;        /**< Samples per measurement window */
	bool motion_rejection;        /**< Enable motion artifact rejection */
	uint8_t quality_threshold;    /**< Min quality % for valid results */
};

/**
 * @brief Initialize PPG algorithm subsystem
 *
 * Must be called once at system startup before using any other PPG functions.
 *
 * @retval 0 Success
 * @retval -ENODEV Required sensors not available
 * @retval negative Other errors
 */
int ppg_algo_init(void);

/**
 * @brief Start a PPG measurement
 *
 * Configures the subsystem and begins sampling from MAX86141 and LIS2DS12.
 * Triggers will fire as samples are processed and when measurement completes.
 *
 * @param config Measurement configuration (NULL for defaults)
 * @retval 0 Success
 * @retval -EBUSY Measurement already in progress
 * @retval -EINVAL Invalid configuration
 * @retval negative Other errors
 */
int ppg_algo_start_measurement(const struct ppg_algo_config *config);

/**
 * @brief Stop ongoing PPG measurement
 *
 * @retval 0 Success
 * @retval -EALREADY Not measuring
 */
int ppg_algo_stop_measurement(void);

/**
 * @brief Check if measurement is in progress
 *
 * @return true if measuring, false otherwise
 */
bool ppg_algo_is_measuring(void);

/**
 * @brief True while finalize_measurement() is running (sensor shutdown + CSV).
 */
bool ppg_algo_is_finalizing(void);

/**
 * @brief Samples (or hub frames) targeted for the current measurement
 *
 * After ppg_algo_start_measurement() succeeds, returns the effective
 * sample_count from the active config (e.g. HUB mode may clamp a raw-path
 * request of 600 down to 30 frames). Returns 0 if not measuring.
 */
uint16_t ppg_algo_get_measurement_target_samples(void);

/**
 * @brief Register trigger handler
 *
 * @param type Trigger type to handle
 * @param handler Callback function
 * @param user_data User context passed to callback
 * @retval 0 Success
 * @retval -EINVAL Invalid parameters
 */
int ppg_algo_trigger_set(enum ppg_trigger_type type,
			 ppg_trigger_handler_t handler,
			 void *user_data);

/**
 * @brief Get latest measurement result
 *
 * @param result Output buffer for result
 * @retval 0 Success
 * @retval -ENODATA No result available yet
 * @retval -EINVAL Invalid parameter
 */
int ppg_algo_get_result(struct ppg_algo_result *result);

/**
 * @brief Get latest per-sample data
 *
 * Call from SAMPLE_READY trigger handler to get current sample data.
 *
 * @param sample Output buffer for sample data
 * @retval 0 Success
 * @retval -ENODATA No sample available
 * @retval -EINVAL Invalid parameter
 */
int ppg_algo_get_sample_data(struct ppg_sample_data *sample);

/**
 * @brief Reset algorithm state
 *
 * Call when sensor contact is lost or to clear history.
 */
void ppg_algo_reset(void);

/**
 * @brief Get subsystem version string
 *
 * @return Version string (e.g., "2.0.0")
 */
const char *ppg_algo_version(void);

/**
 * @brief Get the human-readable name of the currently selected PPG source.
 *
 * Selection is performed once at ppg_algo_init() via product_hw_detect()
 * (Watch vs Pulse), then binding the matching DT endpoint exclusively
 * (ncs,ppg-primary = Watch / max86141 or hub; ncs,ppg-secondary = Pulse /
 * max3010x). Not an ordered primary→secondary fallback.
 *
 * @return Device name string, or "none" if init has not run / failed.
 */
const char *ppg_algo_get_source_name(void);

/**
 * @brief Get a bitmask of capabilities the selected PPG source supports.
 *
 * Composed of PPG_CAP_* flags. UI code should consult this before
 * offering features that depend on specific hardware (e.g. motion
 * rejection, green LED channel for robust HR).
 *
 * @return Capability bitmask; 0 if no source selected.
 */
uint32_t ppg_algo_get_capabilities(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_SUBSYS_PPG_ALGO_PPG_ALGO_H_ */
