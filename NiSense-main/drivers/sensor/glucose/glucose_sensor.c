/**
 * @file glucose_sensor.c
 * @brief Glucose sensor driver implementation
 *
 * Driver for non-invasive glucose measurement system using optical sensing.
 * Controls IR LED (1550nm) via GPIO and reads glucose signal from photodiode
 * via ADC, applying calibration and filtering algorithms.
 *
 * Hardware architecture (schematic-confirmed; see
 * docs/sensors/GLUCOSE_ANALOG_FRONTEND.md):
 * - IR LEDs: SMC1550 (1550 nm) + SMC1050 (1050 nm) in parallel on both sides
 *   of the PD; single P0.24 GPIO -> BC847 NPN -> SI2333CDS MOSFET
 * - Photodiode (Hamamatsu G11193-03) -> OPA358 TIA (Rf=R7=10MΩ, Cf=C2≈0.8pF;
 *   EN hard-tied to 3V3) -> INA122UA (RG pins 1&8 shorted to GND, REF=GND)
 *   -> P0.03/AIN1 SAADC (mixed-λ photocurrent; no per-λ sequencing)
 *
 * @note Custom glucose_sensor_driver_api — not Zephyr sensor_driver_api.
 *       Discrete analog AFE; no register-mapped sensor IC.
 */

#define DT_DRV_COMPAT glucose_sensor

#include "glucose_sensor.h"
#include "glucose_algorithm.h"
#include <zephyr/drivers/sensor/glucose_algorithm_config.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/util.h>
#include <stdlib.h>
#include <math.h>
#if defined(CONFIG_CALIBRATION)
#include <calibration.h>
#endif

/* RTC time helper (Unix seconds) implemented in rtc.c */
extern int rtc_get_unix_time(uint32_t *unix_secs);

LOG_MODULE_REGISTER(glucose_sensor, CONFIG_SENSOR_LOG_LEVEL);

/* ADC → mV for logging/quality only (algo uses raw counts).
 * Multiplier undoes DT zephyr,gain — NOT the INA122 gain.
 *
 * MUST match boards/raytac_overlay/40_adc_glucose.overlayinc's
 * `zephyr,gain` property exactly (multiplier = reciprocal of that
 * fraction): ADC_GAIN_1_6 -> 6.0f, ADC_GAIN_1_5 -> 5.0f,
 * ADC_GAIN_1_4 -> 4.0f, etc. If you change one, change the other — see
 * the comment in that overlay file for the full explanation of what
 * this line controls (raw ADC operating point / dynamic range usage)
 * and why it also affects the glucose regression's input scale.
 *
 * 2026-08-27: moved from ADC_GAIN_1_6 to ADC_GAIN_1_5 to shift the raw
 * ADC operating point from ~2170 counts (~53% FS) up toward ~2600
 * counts (~64% FS).
 * 2026-08-28: moved again, ADC_GAIN_1_5 -> ADC_GAIN_1_4, to push above
 * a 3000-count floor (~3260 counts, ~80% FS). This is right at the
 * ceiling of the 50-80% window GLUCOSE_AFE_REVIEW.md recommends —
 * expect GLUCOSE_SENSOR_FLAG_OUT_OF_WINDOW / _SATURATION to start
 * appearing more often than at the previous setting if the signal
 * drifts up at all. See the long comment in the overlay file before
 * pushing this further.
 */
#define ADC_MAX_VALUE           4095.0f
#define ADC_REF_VOLTAGE_MV      600.0f  /* 0.6V internal reference (in mV) */
#define ADC_GAIN_MULTIPLIER     4.0f    /* undo SAADC ADC_GAIN_1_4 */

/* Glucose conversion constants */
#define GLUCOSE_MG_DL_TO_MMOL_L 0.0555f

/* Forward declarations */
static void glucose_sensor_measurement_work_handler(struct k_work *work);
static void glucose_sensor_process_work_handler(struct k_work *work);
static int glucose_sensor_sample_adc(struct glucose_sensor_data *data,
				     uint16_t *samples, uint16_t count);

/* Dedicated workqueue for glucose sensor (prevents blocking system workqueue)
 * Priority 10 (low) ensures it doesn't block LVGL (priority 7) or other tasks.
 * Stack size tunable via CONFIG_GLUCOSE_SENSOR_WORKQ_STACK_SIZE (default 4096).
 * Historical values: 8192 (embedded matrices), 6144 (interim), 4096 (file-based).
 */
K_THREAD_STACK_DEFINE(glucose_workq_stack, CONFIG_GLUCOSE_SENSOR_WORKQ_STACK_SIZE);
static struct k_work_q glucose_workq;
static bool glucose_workq_initialized = false;
static void glucose_sensor_process_samples(struct glucose_sensor_data *data);
static void glucose_sensor_calculate_quality(struct glucose_sensor_data *data,
					     float *voltages, uint16_t count);

/**
 * @brief Compare function for qsort (median filter)
 */
static int compare_float(const void *a, const void *b)
{
	float fa = *(const float *)a;
	float fb = *(const float *)b;
	return (fa > fb) - (fa < fb);
}

/**
 * @brief Apply median filter to reduce impulse noise
 */
static void apply_median_filter(float *samples, uint16_t count)
{
#if CONFIG_GLUCOSE_SENSOR_MEDIAN_FILTER_ENABLE
	float *sorted = (float *)k_malloc(count * sizeof(float));
	
	if (sorted == NULL) {
		LOG_WRN("Median filter: insufficient memory, skipping");
		return;
	}
	
	/* Sort samples */
	memcpy(sorted, samples, count * sizeof(float));
	qsort(sorted, count, sizeof(float), compare_float);
	
	/* Use median value (middle element) */
	float median = sorted[count / 2];
	
	/* Replace outliers (>3 std dev from median) with median */
	float sum_sq_diff = 0.0f;
	for (uint16_t i = 0; i < count; i++) {
		float diff = samples[i] - median;
		sum_sq_diff += diff * diff;
	}
	float std_dev = sqrtf(sum_sq_diff / count);
	
	for (uint16_t i = 0; i < count; i++) {
		if (fabsf(samples[i] - median) > 3.0f * std_dev) {
			samples[i] = median;
		}
	}
	
	k_free(sorted);
#endif /* CONFIG_GLUCOSE_SENSOR_MEDIAN_FILTER_ENABLE */
}

/**
 * @brief Apply moving average filter
 */
static float apply_moving_average(float *samples, uint16_t count)
{
#if CONFIG_GLUCOSE_SENSOR_ENABLE_FILTERING
	const uint16_t window_size = CONFIG_GLUCOSE_SENSOR_FILTER_WINDOW_SIZE;
	float sum = 0.0f;
	uint16_t valid_samples = 0;
	
	for (uint16_t i = 0; i < count; i++) {
		sum += samples[i];
		valid_samples++;
		
		/* Apply windowed average */
		if (valid_samples >= window_size) {
			samples[i] = sum / valid_samples;
			sum -= samples[i - window_size + 1];
			valid_samples = window_size - 1;
		} else {
			samples[i] = sum / valid_samples;
		}
	}
	
	/* Return average of last window */
	return sum / valid_samples;
#else
	/* No filtering - return simple average */
	float sum = 0.0f;
	for (uint16_t i = 0; i < count; i++) {
		sum += samples[i];
	}
	return sum / count;
#endif /* CONFIG_GLUCOSE_SENSOR_ENABLE_FILTERING */
}

/**
 * @brief Sample ADC for glucose signal with per-sample timestamps
 */
static int glucose_sensor_sample_adc(struct glucose_sensor_data *data,
				     uint16_t *samples, uint16_t count)
{
	int ret;
	
	/* Configure ADC channel using device tree specification */
	ret = adc_channel_setup_dt(&data->adc_channel);
	if (ret) {
		LOG_ERR("ADC channel setup failed: %d", ret);
		return ret;
	}
	
	/* Calculate delay between samples */
	uint32_t sample_delay_us = 1000000 / data->config.sample_rate_hz;
	
	/* Store count in a local variable to ensure it's preserved */
	uint16_t num_samples = count;
	
	/* Allocate timestamp array if needed */
	if (data->sample_timestamps == NULL) {
		data->sample_timestamps = k_malloc(count * sizeof(int64_t));
		if (data->sample_timestamps == NULL) {
			LOG_ERR("Failed to allocate timestamp buffer");
			return -ENOMEM;
		}
	}
	
	/* Sample loop - iterate through all requested samples */
	for (uint16_t i = 0; i < num_samples; i++) {
		/* Abort may set state to IDLE while this loop is sleeping. */
		if (atomic_get(&data->state) != GLUCOSE_SENSOR_STATE_SAMPLING) {
			LOG_INF("ADC sampling aborted at sample %u/%u", i, num_samples);
			return -ECANCELED;
		}

		/* Capture timestamp in seconds using RTC Unix time when available */
		uint32_t unix_secs = 0;
		if (rtc_get_unix_time(&unix_secs) == 0) {
			data->sample_timestamps[i] = (int64_t)unix_secs;
		} else {
			/* Fallback to uptime-based seconds if RTC is unavailable */
			data->sample_timestamps[i] = (int64_t)(k_uptime_get() / 1000);
		}
		
		struct adc_sequence seq = {
			.buffer = &samples[i],
			.buffer_size = sizeof(uint16_t),
			.channels = BIT(data->adc_channel.channel_id),
			.resolution = data->adc_channel.resolution,
		};
		
		ret = adc_read(data->adc_channel.dev, &seq);
		if (ret) {
			LOG_ERR("ADC read failed at sample %u/%u: %d", i, num_samples, ret);
			return ret;
		}
		
		/* Convert ADC to voltage for logging */
		float voltage_mv = ((float)samples[i] / ADC_MAX_VALUE) *
				   ADC_REF_VOLTAGE_MV * ADC_GAIN_MULTIPLIER;
		voltage_mv -= data->calibration.offset_mv;
		voltage_mv *= data->calibration.gain;
		
		/* Update latest sample data for per-sample callback */
		data->latest_sample.sample_number = i;
		data->latest_sample.total_samples = num_samples;
		data->latest_sample.raw_adc_value = samples[i];
		data->latest_sample.voltage_mv = voltage_mv;
		data->latest_sample.timestamp_sec = data->sample_timestamps[i];
		
		/* Fire per-sample trigger callback if registered */
		if (data->sample_handler) {
			data->sample_handler(data->dev, data->sample_trigger);
		}
		
		/* Log with timestamp in seconds (Unix epoch), voltage in mV and V */
		LOG_DBG("Sample[%u/%u] = %u (0x%04X) @ %lld sec, Voltage: %.3f mV (%.3f V)", 
			i, num_samples, samples[i], samples[i], 
			data->sample_timestamps[i],
			(double)voltage_mv, (double)(voltage_mv / 1000.0f));
		
		/* Wait before next sample (except last); re-check abort after sleep. */
		if (i + 1 < num_samples) {
			k_usleep(sample_delay_us);
			if (atomic_get(&data->state) != GLUCOSE_SENSOR_STATE_SAMPLING) {
				LOG_INF("ADC sampling aborted after sample %u/%u",
					i + 1U, num_samples);
				return -ECANCELED;
			}
		}
	}
	return 0;
}

/**
 * @brief Process ADC samples through calibration and filtering
 */
static void glucose_sensor_process_samples(struct glucose_sensor_data *data)
{
	uint16_t count = data->sample_count;
	uint16_t *raw_samples = data->samples;
	/* Allocate voltage storage for result if needed */
	if (data->result.voltages_mv == NULL) {
		data->result.voltages_mv = k_malloc(count * sizeof(float));
		if (data->result.voltages_mv == NULL) {
			LOG_ERR("Failed to allocate voltages_mv buffer");
			return;
		}
	}
	
	float *voltages = data->filtered_samples;
	
	/* Convert raw ADC samples to voltage (mV) - no logging here, already logged during acquisition */
	for (uint16_t i = 0; i < count; i++) {
		/* Convert ADC value to voltage (in mV):
		 * V_mV = (raw / 4095) * 600mV * 6
		 */
		float voltage_mv = ((float)raw_samples[i] / ADC_MAX_VALUE) *
				   ADC_REF_VOLTAGE_MV * ADC_GAIN_MULTIPLIER;
		
		/* Apply calibration offset (dark current compensation)
		 * 
		 * This subtracts the dark current offset measured during calibration
		 * or loaded from device tree. See glucose_sensor_impl_calibrate() for
		 * detailed explanation of why this calibration is necessary.
		 * 
		 * The offset removes DC bias from:
		 * - Photodiode dark current (thermal leakage)
		 * - Amplifier input offset voltage
		 * - ADC offset errors
		 * - Reference voltage drift
		 * 
		 * This ensures only light-induced signal changes are used for glucose
		 * calculation, improving accuracy and dynamic range.
		 * 
		 * The offset is always applied if it exists (from auto-calibration,
		 * device tree, or manual calibration). If offset is 0, this subtraction
		 * is effectively a no-op.
		 */
		voltage_mv -= data->calibration.offset_mv;
		
		/* Apply calibration gain */
		voltage_mv *= data->calibration.gain;
		
		voltages[i] = voltage_mv;
		
		/* Store calibrated voltage in result for CSV logging */
		data->result.voltages_mv[i] = voltage_mv;
	}
	
	/* Check for ADC saturation */
	bool saturation = false;
	for (uint16_t i = 0; i < count; i++) {
		if (raw_samples[i] >= (ADC_MAX_VALUE - 10)) {
			saturation = true;
			break;
		}
	}
	
	if (saturation) {
		data->result.flags |= GLUCOSE_SENSOR_FLAG_SATURATION;
		LOG_WRN("ADC saturation detected");
	}

	/*
	 * Operating-window check, per the LED-intensity characterization
	 * methodology (1050_1550.docx): target 50-80% of ADC full scale as
	 * an initial engineering point, with an explicit headroom
	 * requirement of ADCpeak < 0.8*4095 (~3276) so subject-to-subject
	 * variation doesn't push some people into saturation.
	 *
	 * IMPORTANT — correcting a real misunderstanding: a reading near
	 * 4085 (ADC_MAX_VALUE-10) is NOT a target — it IS the saturation
	 * threshold checked above. That document explicitly says not to
	 * target continuous full-scale readings and recommends 50-80%
	 * (roughly 2048-3276 on a 12-bit ADC) as the starting operating
	 * band, determined experimentally per-subject. This log line
	 * reports where the actual reading sits in that band so it can be
	 * checked against the recommended range, rather than assuming
	 * "closer to 4095 is better."
	 */
	int32_t mean_raw = 0;

	for (uint16_t i = 0; i < count; i++) {
		mean_raw += raw_samples[i];
	}
	mean_raw = (count > 0) ? (mean_raw / (int32_t)count) : 0;

	int32_t pct_of_fullscale = (mean_raw * 100) / (int32_t)ADC_MAX_VALUE;

	if (pct_of_fullscale > 80) {
		data->result.flags |= GLUCOSE_SENSOR_FLAG_OUT_OF_WINDOW;
		LOG_WRN("Glucose ADC: mean=%d (%d%% of full scale) — above the "
			"recommended 50-80%% operating window, low headroom "
			"before saturation. Consider reducing analog front-end "
			"gain, not increasing it.",
			mean_raw, pct_of_fullscale);
	} else if (pct_of_fullscale < 50) {
		data->result.flags |= GLUCOSE_SENSOR_FLAG_OUT_OF_WINDOW;
		LOG_DBG("Glucose ADC: mean=%d (%d%% of full scale) — below the "
			"recommended 50-80%% operating window (weak signal, "
			"not saturated).",
			mean_raw, pct_of_fullscale);
	} else {
		LOG_DBG("Glucose ADC: mean=%d (%d%% of full scale) — within the "
			"recommended 50-80%% operating window.",
			mean_raw, pct_of_fullscale);
	}
	
	/* Apply median filter (outlier rejection) - for quality metrics only */
	apply_median_filter(voltages, count);
	
	/* Apply moving average filter - for quality metrics only */
	float avg_voltage_mv = apply_moving_average(voltages, count);
	
	/* Allocate algorithm details structure for CSV logging */
	if (data->result.algorithm_details == NULL) {
		data->result.algorithm_details = k_malloc(sizeof(struct glucose_algorithm_details));
		if (data->result.algorithm_details == NULL) {
			LOG_WRN("Failed to allocate algorithm_details (CSV will have empty columns)");
		}
	}
	
	/* Calculate glucose concentration using watch_intercept algorithm */
	/* The algorithm expects exactly 80 raw ADC samples */
	int32_t glucose_raw = 0;
	bool algo_ok = true;
	if (count == 80) {
		glucose_raw = glucose_intercept(raw_samples, count, data->result.algorithm_details);
		if (glucose_raw < 0) {
			LOG_ERR("Glucose algorithm returned error: %d", glucose_raw);
			glucose_raw = 0;
			algo_ok = false;
		}
	} else {
		LOG_ERR("Invalid sample count for algorithm: expected 80, got %u", count);
		glucose_raw = 0;
		algo_ok = false;
	}
	
	/* Clamp to reasonable physiological range (0-600 mg/dL) */
	if (glucose_raw < 0) {
		glucose_raw = 0;
	} else if (glucose_raw > 600) {
		glucose_raw = 600;
	}
	
	float glucose_mg_dl = (float)glucose_raw;

#if defined(CONFIG_CALIBRATION)
	/* Factory clamp / temp-comp when calibration_model section is valid.
	 * Skin temp not available in-driver → NaN skips temp compensation. */
	calibration_apply_glucose_mg_dl(&glucose_mg_dl, NAN);
#endif

	/* Store results */
	data->result.glucose_mg_dl = glucose_mg_dl;
	data->result.glucose_mmol_l = glucose_mg_dl * GLUCOSE_MG_DL_TO_MMOL_L;
	if (algo_ok) {
		data->result.flags |= GLUCOSE_SENSOR_FLAG_VALID;
	} else {
		data->result.flags |= GLUCOSE_SENSOR_FLAG_ALGO_ERROR;
	}
	
	/* Initialize fasting insulin if not set (default from Kconfig)
	 * This can be updated via config manager or set by application before measurement
	 */
	if (data->result.fasting_insulin_uiu_ml <= 0.0f) {
		data->result.fasting_insulin_uiu_ml = (float)DEFAULT_FASTING_INSULIN_UIU_ML;
	}
	
	/* Calculate insulin resistance metrics (HOMA-IR) if algorithm details available */
	if (algo_ok && data->result.algorithm_details != NULL) {
		int ret = insulin_resistance_calculate(data->result.algorithm_details,
											   (double)glucose_mg_dl,
											   (double)data->result.fasting_insulin_uiu_ml);
		if (ret < 0) {
			LOG_WRN("Insulin resistance calculation failed: %d", ret);
			data->result.flags |= GLUCOSE_SENSOR_FLAG_ALGO_ERROR;
		} else {
			LOG_INF("HOMA-IR calculation complete: index=%.6f",
				data->result.algorithm_details->homa_ir_index);
		}
	} else if (!algo_ok) {
		LOG_WRN("Skipping HOMA-IR calculation due to invalid glucose algorithm result");
	}
	
	/* Calculate signal quality metrics */
#if CONFIG_GLUCOSE_SENSOR_SIGNAL_QUALITY_METRICS
	glucose_sensor_calculate_quality(data, voltages, count);
#endif
	
	LOG_DBG("Glucose: %.1f mg/dL (%.2f mmol/L), voltage: %.2f mV",
		(double)glucose_mg_dl, (double)data->result.glucose_mmol_l,
		(double)avg_voltage_mv);

	/* result.timestamp = calculation complete (raw samples keep capture times). */
	{
		uint32_t unix_secs = 0;

		if (rtc_get_unix_time(&unix_secs) == 0) {
			data->result.timestamp = (int64_t)unix_secs;
		} else {
			data->result.timestamp = (int64_t)(k_uptime_get() / 1000);
		}
	}
	
	/* Invoke legacy callback if registered (deprecated, use trigger API instead) */
	if (data->callback) {
		data->callback(data->adc_channel.dev, &data->result, data->user_data);
	}
	
	/* Fire measurement complete trigger callback if registered */
	if (data->complete_handler) {
		data->complete_handler(data->dev, data->complete_trigger);
	}
}

/**
 * @brief Calculate signal quality metrics
 */
static void glucose_sensor_calculate_quality(struct glucose_sensor_data *data,
					      float *voltages, uint16_t count)
{
#if CONFIG_GLUCOSE_SENSOR_SIGNAL_QUALITY_METRICS
	float sum = 0.0f;
	float sum_sq = 0.0f;
	float min_v = voltages[0];
	float max_v = voltages[0];
	
	/* Calculate mean, variance, min, max */
	for (uint16_t i = 0; i < count; i++) {
		sum += voltages[i];
		sum_sq += voltages[i] * voltages[i];
		if (voltages[i] < min_v) {
			min_v = voltages[i];
		}
		if (voltages[i] > max_v) {
			max_v = voltages[i];
		}
	}
	
	float mean = sum / count;
	float variance = (sum_sq / count) - (mean * mean);
	float std_dev = sqrtf(variance);
	
	/* Peak-to-peak amplitude */
	float peak_to_peak = max_v - min_v;
	
	/* Signal-to-noise ratio (SNR) in dB */
	/* SNR = 20 * log10(signal / noise), where noise = std_dev */
	float snr_db = 0.0f;
	if (std_dev > 0.0f) {
		snr_db = 20.0f * log10f(fabsf(mean) / std_dev);
	}
	
	/* Quality score (0.0 to 1.0) based on SNR and variance */
	float quality_score = 1.0f;
	if (snr_db < 20.0f) {
		quality_score *= (snr_db / 20.0f);  /* Penalize low SNR */
		data->result.flags |= GLUCOSE_SENSOR_FLAG_LOW_SNR;
	}
	if (variance > 100.0f) {  /* High variance threshold (mV²) */
		quality_score *= 0.5f;
		data->result.flags |= GLUCOSE_SENSOR_FLAG_HIGH_VARIANCE;
	}
	/*
	 * Surface signal-validity problems as a lower quality_score
	 * instead of a fabricated-but-confident-looking output. This
	 * covers what the earlier operating-window / saturation checks
	 * already detect but previously only logged — the result now
	 * carries that information so downstream (BLE/UI/record_store)
	 * can show "low confidence, re-measure" rather than a plain
	 * number indistinguishable from a trustworthy reading.
	 */
	if (data->result.flags & GLUCOSE_SENSOR_FLAG_SATURATION) {
		quality_score *= 0.3f;
	} else if (data->result.flags & GLUCOSE_SENSOR_FLAG_OUT_OF_WINDOW) {
		quality_score *= 0.6f;
	}
	
	/* Store quality metrics */
	data->result.quality.snr_db = snr_db;
	data->result.quality.variance = variance;
	data->result.quality.peak_to_peak_mv = peak_to_peak;
	data->result.quality.quality_score = quality_score;
	
	LOG_DBG("Quality: SNR=%.1f dB, variance=%.2f, p2p=%.2f mV, score=%.2f",
		(double)snr_db, (double)variance, (double)peak_to_peak,
		(double)quality_score);
#endif /* CONFIG_GLUCOSE_SENSOR_SIGNAL_QUALITY_METRICS */
}

/**
 * @brief Measurement work handler - starts sampling after delay
 */
static void glucose_sensor_measurement_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct glucose_sensor_data *data =
		CONTAINER_OF(dwork, struct glucose_sensor_data, measurement_work);
	const struct device *dev = data->dev;
	const struct glucose_sensor_config *cfg = dev->config;
	
	LOG_DBG("Starting ADC sampling (%u samples)", data->config.num_samples);
	
	atomic_set(&data->state, GLUCOSE_SENSOR_STATE_SAMPLING);
	
	/* Allocate sample buffers if needed */
	if (data->samples == NULL) {
		data->samples = k_malloc(data->config.num_samples * sizeof(uint16_t));
		if (data->samples == NULL) {
			LOG_ERR("Failed to allocate samples buffer");
			goto error;
		}
	}
	
	if (data->filtered_samples == NULL) {
		data->filtered_samples = k_malloc(data->config.num_samples * sizeof(float));
		if (data->filtered_samples == NULL) {
			LOG_ERR("Failed to allocate filtered samples buffer");
			goto error;
		}
	}
	
	/* Sample ADC */
	int ret = glucose_sensor_sample_adc(data, data->samples, data->config.num_samples);
	if (ret == -ECANCELED) {
		/* Abort set state to IDLE and IR off; do not process or complete. */
		LOG_DBG("ADC sampling cancelled — leaving IDLE");
		(void)gpio_pin_set_dt(&cfg->ir_led_gpio, 0);
		atomic_set(&data->state, GLUCOSE_SENSOR_STATE_IDLE);
		return;
	}
	if (ret) {
		LOG_ERR("ADC sampling failed: %d", ret);
		goto error;
	}

	/* Drop late results if abort won the race after the last sample. */
	if (atomic_get(&data->state) != GLUCOSE_SENSOR_STATE_SAMPLING) {
		LOG_DBG("Measurement aborted after sampling — skip processing");
		(void)gpio_pin_set_dt(&cfg->ir_led_gpio, 0);
		atomic_set(&data->state, GLUCOSE_SENSOR_STATE_IDLE);
		return;
	}
	
	data->sample_count = data->config.num_samples;
	
	/* Capture measurement time using RTC Unix seconds when available */
	uint32_t unix_secs = 0;
	if (rtc_get_unix_time(&unix_secs) == 0) {
		/* Store Unix seconds in result.timestamp */
		data->result.timestamp = (int64_t)unix_secs;
	} else {
		/* Fallback to uptime-based seconds if RTC is unavailable */
		uint32_t uptime_sec = k_uptime_get() / 1000;
		data->result.timestamp = (int64_t)uptime_sec;
	}
	
	/* Store reference to raw samples and timestamps for external access */
	data->result.raw_samples = data->samples;
	data->result.sample_timestamps = data->sample_timestamps;
	data->result.sample_count = data->sample_count;
	
	/* Turn off IR LED */
	gpio_pin_set_dt(&cfg->ir_led_gpio, 0);
	LOG_DBG("IR LED deactivated");
	
	/* Brief state to show "DATA ACQUISITION COMPLETE" message on UI */
	atomic_set(&data->state, GLUCOSE_SENSOR_STATE_ACQUISITION_COMPLETE);
	
	/* Note: UI polling will catch this state briefly before processing starts. */
	atomic_set(&data->state, GLUCOSE_SENSOR_STATE_PROCESSING);
	
	/* Process samples on dedicated workqueue */
	k_work_submit_to_queue(&glucose_workq, &data->process_work);
	
	return;
	
error:
	/* Turn off IR LED on error */
	gpio_pin_set_dt(&cfg->ir_led_gpio, 0);
	atomic_set(&data->state, GLUCOSE_SENSOR_STATE_IDLE);
}

/**
 * @brief Process work handler - processes samples and invokes callback
 */
static void glucose_sensor_process_work_handler(struct k_work *work)
{
	struct glucose_sensor_data *data =
		CONTAINER_OF(work, struct glucose_sensor_data, process_work);

	/* Abort may have raced to IDLE after process_work was submitted. */
	if (atomic_get(&data->state) != GLUCOSE_SENSOR_STATE_PROCESSING &&
	    atomic_get(&data->state) != GLUCOSE_SENSOR_STATE_ACQUISITION_COMPLETE) {
		LOG_DBG("Process work skipped — measurement aborted");
		atomic_set(&data->state, GLUCOSE_SENSOR_STATE_IDLE);
		return;
	}

	glucose_sensor_process_samples(data);

	atomic_set(&data->state, GLUCOSE_SENSOR_STATE_IDLE);
}

/**
 * @brief Trigger measurement - async operation
 */
static int glucose_sensor_impl_trigger_measurement(const struct device *dev)
{
	struct glucose_sensor_data *data = dev->data;
	const struct glucose_sensor_config *cfg = dev->config;
	
	if (atomic_get(&data->state) != GLUCOSE_SENSOR_STATE_IDLE) {
		LOG_WRN("Measurement already in progress");
		return -EBUSY;
	}
	
	/* Reset result flags */
	data->result.flags = 0;
	
	/* Turn on IR LED */
	int ret = gpio_pin_set_dt(&cfg->ir_led_gpio, 1);
	if (ret) {
		LOG_ERR("Failed to activate IR LED: %d", ret);
		return ret;
	}
	
	LOG_DBG("IR LED activated, waiting %u ms for stabilization",
		data->config.delay_ms);
	
	atomic_set(&data->state, GLUCOSE_SENSOR_STATE_LED_ON);
	
	/* Schedule sampling after delay on dedicated workqueue */
	k_work_schedule_for_queue(&glucose_workq, &data->measurement_work,
				  K_MSEC(data->config.delay_ms));
	
	return 0;
}

/**
 * @brief Set callback function
 */
static int glucose_sensor_impl_set_callback(const struct device *dev,
				       glucose_sensor_callback_t callback,
				       void *user_data)
{
	struct glucose_sensor_data *data = dev->data;
	
	data->callback = callback;
	data->user_data = user_data;
	
	return 0;
}

/**
 * @brief Get latest measurement result
 */
static int glucose_sensor_impl_get_result(const struct device *dev,
				      struct glucose_sensor_result *result)
{
	struct glucose_sensor_data *data = dev->data;
	
	if (!(data->result.flags & GLUCOSE_SENSOR_FLAG_VALID)) {
		return -ENODATA;
	}
	
	memcpy(result, &data->result, sizeof(struct glucose_sensor_result));
	
	return 0;
}

/**
 * @brief Configure runtime parameters
 */
static int glucose_sensor_impl_configure(const struct device *dev,
				    uint16_t num_samples,
				    uint16_t delay_ms)
{
	struct glucose_sensor_data *data = dev->data;
	
	if (atomic_get(&data->state) != GLUCOSE_SENSOR_STATE_IDLE) {
		return -EBUSY;
	}
	
	/*
	 * BUGFIX (F-FW-03): the glucose algorithm hard-requires exactly 80
	 * raw ADC samples (see the "count == 80" check where
	 * glucose_intercept() is invoked) — anything else falls through to
	 * GLUCOSE_SENSOR_FLAG_ALGO_ERROR and glucose_raw=0. This function
	 * previously accepted any value from 10 to 500, so a BLE app or
	 * stale NVS config value in that range could silently break every
	 * subsequent measurement. Clamp to what the algorithm can actually
	 * use while it's fixed-length.
	 */
	if (num_samples != 80) {
		return -EINVAL;
	}
	
	if (delay_ms < 100 || delay_ms > 10000) {
		return -EINVAL;
	}
	
	data->config.num_samples = num_samples;
	data->config.delay_ms = delay_ms;
	
	LOG_DBG("Configured: %u samples, %u ms delay", num_samples, delay_ms);
	
	return 0;
}

/**
 * @brief Perform calibration
 * 
 * This function performs calibration to compensate for sensor offset and gain errors.
 * Two types of calibration are supported:
 * 1. Dark current calibration (calibration_voltage_mv == 0): Measures baseline offset
 * 2. Gain calibration (calibration_voltage_mv > 0): Adjusts gain using known standard
 */
static int glucose_sensor_impl_calibrate(const struct device *dev,
				    float calibration_voltage_mv)
{
	struct glucose_sensor_data *data = dev->data;
	
	if (atomic_get(&data->state) != GLUCOSE_SENSOR_STATE_IDLE) {
		return -EBUSY;
	}
	
	if (calibration_voltage_mv == 0.0f) {
		/* ========================================================================
		 * DARK CURRENT CALIBRATION
		 * ========================================================================
		 * 
		 * Purpose:
		 * --------
		 * Dark current calibration measures the baseline DC offset voltage when
		 * the IR LED is OFF. This offset is caused by:
		 * 
		 * 1. Photodiode dark current: Even without light, photodiodes generate
		 *    a small leakage current (typically 1-100 nA) due to thermal effects
		 *    and semiconductor junction properties.
		 * 
		 * 2. Amplifier offset: The INA122 instrumentation amplifier has input
		 *    offset voltage (typically +/-250 uV) and input bias current that
		 *    creates a DC offset at the output.
		 * 
		 * 3. ADC offset: The nRF52840 ADC may have a small offset error.
		 * 
		 * 4. Ground/reference drift: Small variations in reference voltages or
		 *    ground potential can appear as offset.
		 * 
		 * Why Calibration is Critical:
		 * -----------------------------
		 * The glucose measurement relies on detecting small changes in photodiode
		 * current when IR light (1550nm) passes through tissue. These changes
		 * are typically in the range of microvolts to millivolts. If the dark
		 * current offset is not subtracted, it can:
		 * 
		 * - Mask small signal changes: A 1mV glucose signal could be hidden by
		 *   a 1000mV dark current offset
		 * - Cause measurement errors: The offset adds to the measured signal,
		 *   leading to incorrect glucose calculations
		 * - Reduce dynamic range: The offset consumes ADC range, reducing
		 *   available range for actual signal
		 * 
		 * Calibration Process:
		 * --------------------
		 * 1. IR LED is kept OFF (GPIO inactive) - ensures no light reaches photodiode
		 * 2. 10 ADC samples are collected with 500ms delays between samples
		 *    (total time ~5 seconds)
		 * 3. Each sample is converted to voltage: V = (ADC/4095) * 0.6V * 6 * 1000mV
		 *    - ADC_MAX_VALUE = 4095 (12-bit ADC)
		 *    - ADC_REF_VOLTAGE_MV = 600mV (internal reference)
		 *    - ADC_GAIN_MULTIPLIER = 6 (INA122 gain factor)
		 * 4. Average of all samples is calculated to reduce noise
		 * 5. This average becomes the calibration offset stored in
		 *    data->calibration.offset_mv
		 * 
		 * Usage in Measurements:
		 * ----------------------
		 * During normal glucose measurements, this offset is subtracted from each
		 * ADC sample before processing:
		 *   corrected_voltage = measured_voltage - dark_current_offset
		 * 
		 * This ensures that only the light-induced signal changes are used for
		 * glucose calculation, not the DC bias.
		 * 
		 * When to Perform Calibration:
		 * ---------------------------
		 * - At device startup (if CONFIG_GLUCOSE_SENSOR_AUTO_CALIBRATE is enabled)
		 * - After temperature changes (dark current varies with temperature)
		 * - Periodically (drift over time)
		 * - After sensor replacement or hardware changes
		 * - If measurements become inaccurate
		 * 
		 * Note: Calibration should be performed with the sensor in a dark
		 * environment (LED off) and stable temperature conditions.
		 */
		LOG_INF("Performing dark current calibration (LED off)");
		
		/*
		 * BUGFIX (F-FW-01 / "dark calibration is broken"):
		 * glucose_sensor_sample_adc() aborts on its very first
		 * iteration unless state == SAMPLING (see the check right
		 * at the top of that function) — but this function only
		 * ever checked/left state == IDLE above, so every dark-cal
		 * attempt failed at sample 0 with -ECANCELED, every time.
		 * Enter SAMPLING for the duration of this internal call,
		 * then always return to IDLE afterward (success or not) —
		 * mirroring how a normal measurement transitions state.
		 */
		atomic_set(&data->state, GLUCOSE_SENSOR_STATE_SAMPLING);

		/* Sample ADC with LED off to measure baseline offset */
		uint16_t dark_samples[10];
		int ret = glucose_sensor_sample_adc(data, dark_samples, 10);

		atomic_set(&data->state, GLUCOSE_SENSOR_STATE_IDLE);

		if (ret) {
			LOG_ERR("Dark current calibration failed: %d", ret);
			return ret;
		}
		
		/* Calculate average dark current voltage
		 * 
		 * For each sample:
		 * 1. Convert 12-bit ADC value (0-4095) to voltage in mV
		 * 2. Apply ADC reference voltage scaling (0.6V internal ref)
		 * 3. Apply amplifier gain (6x from INA122)
		 * 4. Convert to millivolts
		 * 
		 * Formula: V_mv = (ADC_value / 4095) * 600mV * 6 * 1000
		 */
		float sum = 0.0f;
		for (int i = 0; i < 10; i++) {
			float v = ((float)dark_samples[i] / ADC_MAX_VALUE) *
				  ADC_REF_VOLTAGE_MV * ADC_GAIN_MULTIPLIER;
			sum += v;
		}
		float avg_dark = sum / 10.0f;
		
		/* Store calibration offset for use in all future measurements
		 * This value will be subtracted from each ADC sample during
		 * glucose measurement processing to remove DC bias.
		 */
		data->calibration.offset_mv = avg_dark;
		
		LOG_INF("Dark current offset: %.2f mV", (double)avg_dark);
	} else {
		/* Gain calibration with known standard */
		LOG_INF("Performing gain calibration at %.2f mV",
			(double)calibration_voltage_mv);
		
		/* Gain calibration with voltage standard - partial implementation
		 * 
		 * This function receives a known glucose standard voltage (mV) for calibration.
		 * Full implementation would require:
		 * 1. Triggering a measurement with the known glucose standard
		 * 2. Comparing measured voltage to expected calibration_voltage_mv
		 * 3. Calculating gain correction factor: gain_new = (expected / measured) * gain_old
		 * 4. Updating data->calibration.gain with the new factor
		 * 5. Optionally storing calibration to non-volatile storage
		 * 
		 * Currently, the calibration voltage is logged but not used to adjust gain.
		 * Integration with glucose.c/.h algorithms may be needed for complete workflow.
		 * 
		 * The dark current offset calibration (calibration_voltage_mv == 0) is fully
		 * implemented and working.
		 */
		LOG_WRN("Gain calibration with voltage standard partially implemented");
		LOG_INF("Expected calibration voltage: %.2f mV", (double)calibration_voltage_mv);
		
		/* Gain calibration implementation:
		 * User must:
		 * 1. Apply a known glucose standard/reference solution
		 * 2. Call this function with the expected voltage for that standard
		 * 3. Trigger measurement via UI or API
		 * 4. System will measure actual voltage and calculate gain correction
		 * 
		 * For now, we store the calibration voltage for reference.
		 * The gain adjustment should be done after measurement completion
		 * by comparing measured voltage to this calibration_voltage_mv.
		 * 
		 * Formula: new_gain = (calibration_voltage_mv / measured_voltage) * current_gain
		 * 
		 * Note: This requires integration with measurement workflow and NVS storage.
		 * Current offset calibration (calibration_voltage_mv == 0) is fully working.
		 */
		
		/* Store calibration reference for future gain adjustment */
		data->calibration.gain = 1.0f;  /* Reset to unity gain for calibration */
		LOG_INF("Gain reset to 1.0 for calibration. Perform measurement with standard solution.");
		LOG_INF("After measurement, adjust gain = (%.2f mV / measured_voltage) * 1.0",
			(double)calibration_voltage_mv);
	}
	
	return 0;
}

/**
 * @brief Get current measurement state
 */
static int glucose_sensor_impl_get_state(const struct device *dev)
{
	struct glucose_sensor_data *data = dev->data;
	return atomic_get(&data->state);
}

/**
 * @brief Get current configuration
 */
static int glucose_sensor_impl_get_config(const struct device *dev,
					  uint16_t *num_samples,
					  uint16_t *delay_ms)
{
	struct glucose_sensor_data *data = dev->data;
	
	if (num_samples != NULL) {
		*num_samples = data->config.num_samples;
	}
	if (delay_ms != NULL) {
		*delay_ms = data->config.delay_ms;
	}
	
	return 0;
}

/**
 * @brief Set trigger handler (Zephyr sensor API compatible)
 */
static int glucose_sensor_impl_trigger_set(const struct device *dev,
					    const struct sensor_trigger *trig,
					    sensor_trigger_handler_t handler)
{
	struct glucose_sensor_data *data = dev->data;
	
	if (trig == NULL) {
		return -EINVAL;
	}
	
	/* Cast to int to suppress -Wswitch warning for custom trigger types */
	switch ((int)trig->type) {
	case GLUCOSE_TRIG_SAMPLE_READY:
		data->sample_handler = handler;
		data->sample_trigger = trig;
		LOG_DBG("Per-sample trigger handler %s",
			handler ? "registered" : "unregistered");
		break;
		
	case GLUCOSE_TRIG_MEASUREMENT_COMPLETE:
		data->complete_handler = handler;
		data->complete_trigger = trig;
		LOG_DBG("Measurement complete trigger handler %s",
			handler ? "registered" : "unregistered");
		break;
		
	default:
		LOG_ERR("Unsupported trigger type: %d", trig->type);
		return -ENOTSUP;
	}
	
	return 0;
}

/**
 * @brief Get latest per-sample data
 */
static int glucose_sensor_impl_get_sample_data(const struct device *dev,
						struct glucose_sample_data *sample)
{
	struct glucose_sensor_data *data = dev->data;
	
	if (sample == NULL) {
		return -EINVAL;
	}
	
	/* Copy latest sample data */
	memcpy(sample, &data->latest_sample, sizeof(struct glucose_sample_data));
	
	return 0;
}

/**
 * @brief Set fasting insulin for HOMA-IR calculation
 */
static int glucose_sensor_impl_set_fasting_insulin(const struct device *dev,
						   float insulin_uiu_ml)
{
	struct glucose_sensor_data *data = dev->data;
	
	if (insulin_uiu_ml < 0.0f) {
		LOG_ERR("Invalid insulin value: %.2f (must be >= 0)",
			(double)insulin_uiu_ml);
		return -EINVAL;
	}
	
	/* Store insulin value for next measurement */
	data->result.fasting_insulin_uiu_ml = insulin_uiu_ml;
	LOG_INF("Fasting insulin set to %.2f uIU/mL", (double)insulin_uiu_ml);
	
	return 0;
}

/**
 * @brief Get current fasting insulin value
 */
static int glucose_sensor_impl_get_fasting_insulin(const struct device *dev,
						   float *insulin_uiu_ml)
{
	struct glucose_sensor_data *data = dev->data;
	
	if (insulin_uiu_ml == NULL) {
		return -EINVAL;
	}
	
	*insulin_uiu_ml = data->result.fasting_insulin_uiu_ml;
	
	return 0;
}

/**
 * @brief Abort an in-progress measurement
 */
static int glucose_sensor_impl_abort_measurement(const struct device *dev)
{
	struct glucose_sensor_data *data = dev->data;
	const struct glucose_sensor_config *cfg = dev->config;

	if (atomic_get(&data->state) == GLUCOSE_SENSOR_STATE_IDLE) {
		return 0;
	}

	/* Cancel scheduled and queued work items first. */
	(void)k_work_cancel_delayable(&data->measurement_work);
	(void)k_work_cancel(&data->process_work);

	/* Turn IR LED off regardless of current sub-state. */
	(void)gpio_pin_set_dt(&cfg->ir_led_gpio, 0);

	atomic_set(&data->state, GLUCOSE_SENSOR_STATE_IDLE);

	LOG_INF("Glucose measurement aborted");
	return 0;
}

/**
 * @brief Driver API structure
 */
static const struct glucose_sensor_driver_api glucose_sensor_api = {
	.trigger_measurement = glucose_sensor_impl_trigger_measurement,
	.set_callback = glucose_sensor_impl_set_callback,
	.get_result = glucose_sensor_impl_get_result,
	.configure = glucose_sensor_impl_configure,
	.calibrate = glucose_sensor_impl_calibrate,
	.get_state = glucose_sensor_impl_get_state,
	.get_config = glucose_sensor_impl_get_config,
	.trigger_set = glucose_sensor_impl_trigger_set,
	.get_sample_data = glucose_sensor_impl_get_sample_data,
	.set_fasting_insulin = glucose_sensor_impl_set_fasting_insulin,
	.get_fasting_insulin = glucose_sensor_impl_get_fasting_insulin,
	.abort_measurement   = glucose_sensor_impl_abort_measurement,
};

/**
 * @brief Initialize glucose sensor driver
 */
static int glucose_sensor_init(const struct device *dev)
{
	struct glucose_sensor_data *data = dev->data;
	const struct glucose_sensor_config *cfg = dev->config;
	int ret;
	
	LOG_INF("Initializing glucose sensor");
	
	/* Store device reference for work handlers */
	data->dev = dev;
	
	/* Configure IR LED GPIO */
	if (!gpio_is_ready_dt(&cfg->ir_led_gpio)) {
		LOG_ERR("IR LED GPIO device not ready");
		return -ENODEV;
	}
	
	ret = gpio_pin_configure_dt(&cfg->ir_led_gpio, GPIO_OUTPUT_INACTIVE);
	if (ret) {
		LOG_ERR("Failed to configure IR LED GPIO: %d", ret);
		return ret;
	}
	
	/* Verify ADC device */
	if (!device_is_ready(cfg->adc_channel.dev)) {
		LOG_ERR("ADC device not ready");
		return -ENODEV;
	}
	
	/* Initialize data structure - copy ADC channel spec */
	data->adc_channel = cfg->adc_channel;
	
	/* Load device tree configuration */
	data->config.num_samples = cfg->dt_config.num_samples;
	data->config.delay_ms = cfg->dt_config.delay_ms;
	data->config.sample_rate_hz = cfg->dt_config.sample_rate_hz;
	
	/* Load calibration from device tree */
	data->calibration.offset_mv = cfg->dt_config.calibration_offset_mv;
	data->calibration.gain = cfg->dt_config.calibration_gain;
	
	/* Initialize work queues */
	/* Initialize dedicated workqueue (once for all instances) */
	if (!glucose_workq_initialized) {
		k_work_queue_start(&glucose_workq,
				   glucose_workq_stack,
				   K_THREAD_STACK_SIZEOF(glucose_workq_stack),
				   10,  /* Priority 10 (low, won't block LVGL at priority 7) */
				   NULL);
		k_thread_name_set(&glucose_workq.thread, "glucose_sensor");
		glucose_workq_initialized = true;
		LOG_INF("Glucose sensor workqueue initialized (priority 10)");
	}
	
	k_work_init_delayable(&data->measurement_work,
			      glucose_sensor_measurement_work_handler);
	k_work_init(&data->process_work, glucose_sensor_process_work_handler);
	
	atomic_set(&data->state, GLUCOSE_SENSOR_STATE_IDLE);

	/* XIP matrices: glucose_algorithm_init() runs from glucose_init()
	 * in src/sensors/glucose.c after usb_msc_init() mounts FAT (POST_KERNEL here is
	 * too early — FS type not registered yet).
	 */
	
	LOG_INF("Glucose sensor initialized: %u samples, %u ms delay, "
		"offset=%.2f mV, gain=%.3f",
		data->config.num_samples, data->config.delay_ms,
		(double)data->calibration.offset_mv,
		(double)data->calibration.gain);
	
	return 0;
}

/* Device tree instantiation */
#define GLUCOSE_SENSOR_INIT(n) \
	static struct glucose_sensor_data glucose_sensor_data_##n; \
	\
	static const struct glucose_sensor_config glucose_sensor_config_##n = { \
		.ir_led_gpio = GPIO_DT_SPEC_INST_GET(n, ir_led_gpios), \
		.adc_channel = ADC_DT_SPEC_INST_GET(n), \
		.dt_config = { \
			.num_samples = DT_INST_PROP_OR(n, num_samples, \
				       CONFIG_GLUCOSE_SENSOR_DEFAULT_SAMPLES), \
			.delay_ms = DT_INST_PROP_OR(n, led_delay_ms, \
				     CONFIG_GLUCOSE_SENSOR_DEFAULT_DELAY_MS), \
			.sample_rate_hz = DT_INST_PROP_OR(n, sample_rate_hz, \
					 CONFIG_GLUCOSE_SENSOR_DEFAULT_SAMPLE_RATE_HZ), \
			.calibration_offset_mv = DT_INST_PROP_OR(n, \
							 calibration_offset_mv, 0), \
			.calibration_gain = DT_INST_PROP_OR(n, \
					    calibration_gain, 1000) / 1000.0f, \
		}, \
	}; \
	\
	DEVICE_DT_INST_DEFINE(n, \
			      glucose_sensor_init, \
			      PM_DEVICE_DT_INST_DEFINE(n, glucose_sensor_pm_action), \
			      &glucose_sensor_data_##n, \
			      &glucose_sensor_config_##n, \
			      POST_KERNEL, \
			      CONFIG_GLUCOSE_SENSOR_INIT_PRIORITY, \
			      &glucose_sensor_api);

/* Power management action - conditionally compiled
 * Only enabled when both CONFIG_PM_DEVICE and CONFIG_GLUCOSE_SENSOR_POWER_MANAGEMENT
 * are enabled. Handles suspend/resume by controlling IR LED and ADC state.
 */
#if defined(CONFIG_PM_DEVICE) && defined(CONFIG_GLUCOSE_SENSOR_POWER_MANAGEMENT)
static int glucose_sensor_pm_action(const struct device *dev,
				     enum pm_device_action action)
{
	struct glucose_sensor_data *data = dev->data;
	const struct glucose_sensor_config *cfg = dev->config;
	
	switch (action) {
	case PM_DEVICE_ACTION_SUSPEND:
		/* Turn off IR LED during suspend */
		gpio_pin_set_dt(&cfg->ir_led_gpio, 0);
		break;
	case PM_DEVICE_ACTION_RESUME:
		/* Re-initialize on resume */
		glucose_sensor_init(dev);
		break;
	default:
		return -ENOTSUP;
	}
	
	return 0;
}
#else
#define glucose_sensor_pm_action NULL
#endif

DT_INST_FOREACH_STATUS_OKAY(GLUCOSE_SENSOR_INIT)

