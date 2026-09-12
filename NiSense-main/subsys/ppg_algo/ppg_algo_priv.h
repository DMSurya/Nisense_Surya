/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem - Private Implementation Header
 *
 * Internal structures and functions for PPG subsystem components.
 */

#ifndef PPG_ALGO_PRIV_H_
#define PPG_ALGO_PRIV_H_

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/subsys/ppg_algo/ppg_algo.h>
#include <zephyr/logging/log.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Subsystem version */
#define PPG_ALGO_VERSION_STRING "2.3.0"

/* Maximum samples in buffer (50Hz × 12s = 600 samples, with headroom) */
#define PPG_MAX_SAMPLES 800

/* Small timestamped accel history used to pair each PPG sample with the
 * nearest accel sample in time (RAW path) without blocking sensor reads. */
#define PPG_ACCEL_HISTORY_SIZE 16

/**
 * @brief PPG hardware source runtime-selected by probe_ppg_source().
 *
 * Used internally to dispatch sensor-specific attribute calls (e.g. shutdown
 * semantics differ between MAX30102 and MAX86141) and to drive the
 * host-fed accelerometer feeder which is only meaningful on the hub path.
 */
enum ppg_source_type {
	PPG_SOURCE_NONE = 0,
	PPG_SOURCE_MAX3010X,      /**< Standalone MAX30102/MAX30101 on I2C */
	PPG_SOURCE_MAX86141,      /**< MAX86141 behind MAX32664 hub (RAW/passthrough mode).
	                           *   Hub bridges raw FIFO bytes; nRF runs on-chip DSP. */
	PPG_SOURCE_MAX32664_HUB,  /**< MAX32664 hub in HUB mode — hub firmware runs WHRM+
	                           *   WSpO2 internally and emits HR/SpO2 via custom
	                           *   channels (SENSOR_CHAN_MAX32664_HR / _SPO2 / _QUALITY).
	                           *   ppg_algo skips on-chip DSP on this source. */
};

/**
 * @brief Sample buffer for raw sensor data
 */
struct ppg_sample_buffer {
	uint32_t *ir;                /**< IR LED samples */
	uint32_t *red;               /**< Red LED samples */
	uint32_t *green;             /**< Green LED samples */
	uint32_t *ambient;           /**< Ambient light samples (slot 4, for correction) */
	int32_t  *ir_ac;             /**< IR AC component per sample (for resp rate) */
#if defined(CONFIG_PPG_LOG_DC_AC)
	int32_t  *ir_dc;             /**< IR DC per sample (NOR PPG_RAW) */
	int32_t  *red_dc;
	int32_t  *green_dc;
	int32_t  *red_ac;
	int32_t  *green_ac;
#endif
	int16_t *accel_x;            /**< Accel X samples */
	int16_t *accel_y;            /**< Accel Y samples */
	int16_t *accel_z;            /**< Accel Z samples */
	/** Milliseconds since measurement start when each sample was captured. */
	uint16_t *capture_dt_ms;
	uint32_t capture_t0_uptime_ms; /**< Uptime of first sample (for dt base) */
	bool capture_t0_set;
	uint16_t count;              /**< Current sample count */
	uint16_t capacity;           /**< Buffer capacity */
	bool has_accel;              /**< Accel data available */
};

/**
 * @brief Filtering state for one channel
 */
struct ppg_filter_state {
	int32_t dc;                  /**< DC component */
	int32_t ac;                  /**< AC component */
	int32_t prev_sample;         /**< Previous sample for delta */
	uint32_t sum;                /**< Running sum for averaging */
	uint16_t count;              /**< Sample count in current window */
};

/**
 * @brief Preprocessing state
 */
struct ppg_preproc_state {
	struct ppg_filter_state ir;
	struct ppg_filter_state red;
	struct ppg_filter_state green;
	struct ppg_filter_state ambient;
	bool initialized;
};

/**
 * @brief Respiration rate estimation state
 */
struct ppg_resp_state {
	uint32_t last_peak_sample;      /**< Sample index of last breath peak */
	uint16_t inter_breath_samples[8]; /**< Last 8 inter-breath intervals (samples) */
	uint8_t  breath_count;          /**< Number of detected breaths */
	uint8_t  ib_head;               /**< Ring buffer head */
	int32_t  envelope;              /**< Current envelope IIR state */
};

/** R-R history length for HR + HRV (SDNN/RMSSD). ~60–120 intervals. */
#define PPG_RR_HISTORY_SIZE 96

/**
 * @brief Peak detection state
 */
struct ppg_peak_state {
	uint32_t last_peak_time_ms;
	uint32_t rr_intervals[PPG_RR_HISTORY_SIZE]; /**< Recent R-R intervals (ms) */
	uint8_t rr_count;
	int32_t threshold;           /**< Adaptive threshold */
	int32_t signal_max;          /**< Maximum AC signal seen (for calibration) */
	uint32_t sample_count;       /**< Total samples processed (for calibration phase) */
	bool peak_detected;
	/* Local-maxima state */
	int32_t prev_ac;             /**< Previous AC sample */
	bool was_rising;             /**< Previous slope was rising */
	int32_t candidate_peak_ac;   /**< AC at provisional peak */
	uint32_t candidate_peak_time_ms;
	bool have_candidate;
	/* Per-beat AC amplitudes (peak height) for SpO2 / PI */
	int32_t beat_ac_amp[PPG_RR_HISTORY_SIZE];
	uint8_t beat_ac_count;
};

/**
 * @brief Quality metrics
 */
struct ppg_quality_metrics {
	uint16_t snr_db_x10;          /**< SNR in dB x10 */
	uint8_t perfusion_index_x10;  /**< PI in % x10 */
	int16_t skewness_x10;         /**< Elgendi SSQI skewness × 10 */
	uint16_t motion_magnitude;    /**< Motion in mg */
	bool motion_detected;
	enum ppg_signal_quality quality;
};

/**
 * @brief Trigger handler entry
 */
struct ppg_trigger_entry {
	ppg_trigger_handler_t handler;
	void *user_data;
	bool enabled;
};

/**
 * @brief Main subsystem state
 */
struct ppg_algo_state {
	/* Configuration */
	struct ppg_algo_config config;
	
	/* Sensor devices */
	const struct device *ppg_dev;   /**< Active PPG sensor (MAX30102 or MAX86141) */
	const struct device *accel_dev; /**< LIS2DS12 accelerometer */
	const struct device *hub_dev;   /**< MAX32664 hub (NULL if classic path) */
	enum ppg_source_type source_type;
	
	/* Sample buffer */
	struct ppg_sample_buffer buffer;
	
	/* Processing state */
	struct ppg_preproc_state preproc;
	struct ppg_peak_state peaks;
	struct ppg_quality_metrics quality;
	struct ppg_resp_state resp;
	
	/* Results */
	struct ppg_algo_result latest_result;
	struct ppg_sample_data latest_sample;
	bool result_ready;
	bool sample_ready;
	
	/* Triggers — sized for PPG_TRIG_COUNT */
	struct ppg_trigger_entry triggers[PPG_TRIG_COUNT];
	
	/* State */
	bool measuring;
	bool finalizing;  /**< finalize/session-stop post-sample path active */
	bool initialized;
	bool green_channel_available;  /**< True if SENSOR_CHAN_GREEN read succeeds */
	bool accel_cache_valid;        /**< True when cached accel_mg has a fresh sample */
	int16_t accel_cached_x_mg;
	int16_t accel_cached_y_mg;
	int16_t accel_cached_z_mg;
	uint32_t accel_cached_ts_ms;
	int16_t accel_hist_x_mg[PPG_ACCEL_HISTORY_SIZE];
	int16_t accel_hist_y_mg[PPG_ACCEL_HISTORY_SIZE];
	int16_t accel_hist_z_mg[PPG_ACCEL_HISTORY_SIZE];
	uint32_t accel_hist_ts_ms[PPG_ACCEL_HISTORY_SIZE];
	uint8_t accel_hist_head;       /**< next write slot in accel history */
	uint8_t accel_hist_count;      /**< valid sample count in accel history */
	uint32_t start_time_ms;
	uint16_t samples_processed;
	uint16_t record_target;        /**< Authoritative window (NOR) sample count */
	uint16_t session_target;       /**< record_target + live tail */
	bool partial_vitals_fired;
	bool partial_vascular_fired;
	bool record_ready_fired;
	uint8_t  no_contact_frames;    /**< Consecutive frames with scd_state=0 (no skin contact) */
	uint16_t last_good_hr_bpm;     /**< Last valid HR for motion hold */
	uint8_t  last_good_hr_conf;
	
	/* Sensor work items */
	struct k_work_delayable ppg_work;
	struct k_work_delayable accel_work;

#ifdef CONFIG_PPG_ALGO_DEDICATED_WORKQUEUE
	/* Dedicated work queue (avoids blocking system workqueue during I2C reads) */
	struct k_work_q ppg_work_q;
#endif

	/* Thread safety */
	struct k_mutex lock;
};

/* Global subsystem state (defined in ppg_algo.c) */
extern struct ppg_algo_state ppg_state;

/* ========== Component Functions ========== */

/**
 * @brief Initialize sample buffer
 *
 * @param buffer Sample buffer to initialize
 * @param capacity Maximum number of samples
 * @param include_accel Whether to allocate accel buffers
 * @retval 0 Success
 * @retval -ENOMEM Out of memory
 */
int ppg_samples_init(struct ppg_sample_buffer *buffer, uint16_t capacity, bool include_accel);

/**
 * @brief Free sample buffer memory
 */
void ppg_samples_free(struct ppg_sample_buffer *buffer);

/**
 * @brief Add sample to buffer
 *
 * @param buffer Sample buffer
 * @param ir IR LED value
 * @param red Red LED value
 * @param green Green LED value (optional)
 * @param accel_x Accel X (optional)
 * @param accel_y Accel Y (optional)
 * @param accel_z Accel Z (optional)
 * @retval 0 Success
 * @retval -ENOMEM Buffer full
 */
int ppg_samples_add(struct ppg_sample_buffer *buffer,
		    uint32_t ir, uint32_t red, uint32_t green,
		    uint32_t ambient, int32_t ir_ac,
#if defined(CONFIG_PPG_LOG_DC_AC)
		    int32_t ir_dc, int32_t red_dc, int32_t green_dc,
		    int32_t red_ac, int32_t green_ac,
#endif
		    int16_t accel_x, int16_t accel_y, int16_t accel_z);

/**
 * @brief Initialize preprocessing filters
 */
void ppg_preproc_init(struct ppg_preproc_state *state);

/**
 * @brief Process one sample through filters
 *
 * @param state Preprocessing state
 * @param ir_raw Raw IR value
 * @param red_raw Raw red value
 * @param green_raw Raw green value (optional)
 * @param[out] ir_dc DC component IR
 * @param[out] ir_ac AC component IR
 * @param[out] red_dc DC component red
 * @param[out] red_ac AC component red
 * @param[out] green_dc DC component green (NULL if not needed)
 * @param[out] green_ac AC component green (NULL if not needed)
 */
void ppg_preproc_filter(struct ppg_preproc_state *state,
			uint32_t ir_raw, uint32_t red_raw, uint32_t green_raw,
			uint32_t ambient_raw,
			int32_t *ir_dc, int32_t *ir_ac,
			int32_t *red_dc, int32_t *red_ac,
			int32_t *green_dc, int32_t *green_ac);

/**
 * @brief Initialize peak detection
 */
void ppg_peak_detect_init(struct ppg_peak_state *state);

/**
 * @brief Detect peaks in filtered signal
 *
 * @param state Peak detection state
 * @param ac_signal AC component
 * @param timestamp_ms Current timestamp
 * @param[out] rr_interval_ms R-R interval if peak detected
 * @retval true Peak detected
 * @retval false No peak
 */
bool ppg_peak_detect(struct ppg_peak_state *state,
		     int32_t ac_signal,
		     uint32_t timestamp_ms,
		     uint16_t *rr_interval_ms);

/**
 * @brief Calculate heart rate from R-R intervals
 *
 * @param state Peak detection state
 * @param[out] hr_bpm Heart rate in BPM
 * @param[out] confidence Confidence 0-100%
 * @retval 0 Success
 * @retval -ENODATA Not enough data
 */
int ppg_hr_calculate(struct ppg_peak_state *state,
		     uint16_t *hr_bpm,
		     uint8_t *confidence);

/**
 * @brief Calculate SDNN / RMSSD from R-R intervals
 *
 * Requires ≥20 intervals for a valid result.
 *
 * @param state Peak detection state with RR history
 * @param[out] sdnn_ms SDNN in ms
 * @param[out] rmssd_ms RMSSD in ms
 * @retval 0 Success
 * @retval -ENODATA Not enough R-R intervals
 * @retval -EINVAL Invalid arguments
 */
int ppg_hrv_calculate(const struct ppg_peak_state *state,
		      uint16_t *sdnn_ms,
		      uint16_t *rmssd_ms);

/**
 * @brief Sir's spreadsheet-method HR/HRV/BP (formulas_of_Miisky.xlsx +
 * formula_in_each_column.doc). Runs on the first 300 of the collected
 * samples in the buffer. See ppg_spreadsheet_hrv_bp.c for the full
 * column-by-column formula mapping and caveats.
 */
struct ppg_ss_result {
	bool     valid;
	uint16_t hr_bpm;
	uint16_t sdnn_x100;
	uint16_t rmssd_x100;
	uint16_t systolic_x100;
	uint16_t diastolic_x100;
	uint16_t peak_count;
};

int ppg_spreadsheet_calculate(const struct ppg_sample_buffer *buffer,
			      struct ppg_ss_result *out);

/**
 * @brief Approximate cuffless BP from PPG morphology + HR
 *
 * Uses IR/Red pulse amplitude ratio and heart rate. Estimate only —
 * not a clinical cuff reading.
 *
 * @param hr_bpm Heart rate in BPM
 * @param ac_ir / dc_ir IR AC/DC
 * @param ac_red / dc_red Red AC/DC
 * @param[out] systolic_mmhg Estimated systolic
 * @param[out] diastolic_mmhg Estimated diastolic
 * @retval 0 Success
 * @retval -EINVAL Weak signal or out-of-range HR
 */
int ppg_bp_estimate(uint16_t hr_bpm,
		    uint32_t ac_ir, uint32_t dc_ir,
		    uint32_t ac_red, uint32_t dc_red,
		    uint16_t *systolic_mmhg,
		    uint16_t *diastolic_mmhg);

/**
 * @brief Calculate SpO2 from R-value
 *
 * @param dc_red DC component red
 * @param ac_red AC amplitude red (pulse amplitude, not residual)
 * @param dc_ir DC component IR
 * @param ac_ir AC amplitude IR (pulse amplitude, not residual)
 * @param[out] spo2_percent SpO2 percentage
 * @param[out] confidence Confidence 0-100%
 * @param[out] r_value_x1000 R-value * 1000
 * @retval 0 Success
 * @retval -EINVAL Invalid signal
 */
int ppg_spo2_calculate(int32_t dc_red, int32_t ac_red,
		       int32_t dc_ir, int32_t ac_ir,
		       uint16_t *spo2_percent,
		       uint8_t *confidence,
		       uint16_t *r_value_x1000);

/**
 * @brief Derive Red/IR/Green AC pulse amplitudes from buffer + peaks
 *
 * Prefers mean |AC| over the window and/or mean beat peak height.
 * Used by SpO2, PI, Hb, and BP so all share the same amplitude definition.
 */
void ppg_ac_amplitudes_from_buffer(const struct ppg_sample_buffer *buffer,
				   const struct ppg_preproc_state *preproc,
				   const struct ppg_peak_state *peaks,
				   int32_t *ac_red_amp,
				   int32_t *ac_ir_amp,
				   int32_t *ac_green_amp);

/**
 * @brief Estimate total hemoglobin from green/IR ratio
 *
 * Uses empirical linear model:
 *   tHb = K1×R_gr + K2×(DC_ir/(DC_ir+DC_red)) + K3
 * where R_gr = (AC_green/DC_green) / (AC_ir/DC_ir)
 *
 * Coefficients come from Kconfig (×10, so K1_X10=-850 → K1=-85.0).
 *
 * @param dc_green DC component green
 * @param ac_green AC amplitude green
 * @param dc_ir    DC component IR
 * @param ac_ir    AC amplitude IR
 * @param dc_red   DC component red
 * @param[out] hb_g_dl_x10  Hb g/dL × 10 (142 = 14.2 g/dL)
 * @param[out] confidence   0-100 %
 * @retval 0 Success
 * @retval -EINVAL Signal too weak
 */
int ppg_hb_calculate(int32_t dc_green, int32_t ac_green,
		     int32_t dc_ir,    int32_t ac_ir,
		     int32_t dc_red,
		     uint16_t *hb_g_dl_x10,
		     uint8_t *confidence);

/**
 * @brief Estimate respiration rate from IR AC amplitude modulation
 *
 * Processes the ir_ac array stored in the sample buffer using envelope
 * peak detection at 0.1–0.5 Hz (6–30 breaths/min).
 *
 * @param ir_ac         Array of IR AC values (one per sample)
 * @param count         Number of samples
 * @param sample_rate   Samples per second
 * @param state         Persistent resp state (reset before first call)
 * @param[out] resp_bpm Respiration rate (breaths/min)
 * @param[out] confidence 0-100 %
 * @retval 0 Success
 * @retval -ENODATA Insufficient data (< 20 s)
 * @retval -EINVAL  Invalid input
 */
int ppg_resp_calculate(const int32_t *ir_ac, uint16_t count,
		       uint16_t sample_rate,
		       struct ppg_resp_state *state,
		       uint8_t *resp_bpm,
		       uint8_t *confidence);

/**
 * @brief Calculate signal quality metrics
 *
 * @param buffer Sample buffer with raw data
 * @param preproc Preprocessing state
 * @param[out] metrics Quality metrics
 */
void ppg_quality_calculate(const struct ppg_sample_buffer *buffer,
			   const struct ppg_preproc_state *preproc,
			   struct ppg_quality_metrics *metrics);

/**
 * @brief Fire trigger if registered
 *
 * @param type Trigger type
 */
void ppg_trigger_fire(enum ppg_trigger_type type);

#ifdef __cplusplus
}
#endif

#endif /* PPG_ALGO_PRIV_H_ */
