#include "glucose.h"
#include "glucose_logger.h"
#include "buzzer.h"
#include "rtc.h"
#include <zephyr/drivers/sensor/glucose_sensor.h>
#include <zephyr/drivers/sensor/glucose_algorithm_config.h>
#include "glucose_hw_variant.h"
#include "product_hw_variant.h"
#include "glucose_algorithm_profiles.h"
/* Runtime matrix lookup — XIP packed model (FatFS CSV path removed). */
#define GLUCOSE_RUNTIME_MATRICES IS_ENABLED(CONFIG_GLUCOSE_MATRICES_FROM_XIP)

#if GLUCOSE_RUNTIME_MATRICES
#include <zephyr/drivers/sensor/glucose_algorithm.h>
#endif
#if defined(CONFIG_BT)
#include "ble_gatt.h"
#endif
#if defined(CONFIG_APP_FEATURE_CLOUD_TELEMETRY)
#include "cloud_telemetry.h"
#endif
#include <errno.h>
#include <stdio.h>
#include <math.h>

LOG_MODULE_REGISTER(glucose, LOG_LEVEL_INF);

/* Glucose sampling interval tracking */
#define GLUCOSE_SAMPLE_INTERVAL_MIN CONFIG_GLUCOSE_SAMPLING_INTERVAL_MINUTES  // From Kconfig
/* See the ROC guard in glucose_handle_result() for the reasoning behind
 * this specific value (~17x margin below the one real artifact observed
 * on-device: 52.6 mg/dL/min). */
#define GLUCOSE_LIVE_ROC_IMPLAUSIBLE_MG_DL_MIN 15.0f

/* Forward declarations */
static void glucose_periodic_timer_handler(struct k_timer *timer);
static void glucose_periodic_work_handler(struct k_work *work);

/* Kernel timer for periodic glucose sampling (hardware-independent) */
K_TIMER_DEFINE(glucose_timer, glucose_periodic_timer_handler, NULL);

/* Work queue for deferred glucose measurement (ISR context → thread context) */
static struct k_work glucose_periodic_work;
static bool glucose_work_initialized = false;

/* Glucose sensor device — GLUCOSE_SENSOR_NODE = DT_NODELABEL(glucose_sensor)
 * is defined in glucose_algorithm_config.h (included above). Do not redefine here. */
static const struct device *glucose_dev = DEVICE_DT_GET_OR_NULL(GLUCOSE_SENSOR_NODE);
static bool glucose_ready = false;
#if GLUCOSE_RUNTIME_MATRICES
static bool glucose_model_ok;
#else
static bool glucose_model_ok = true;
#endif

static glucose_sample_callback_t glucose_sample_cb = NULL;
static void *glucose_sample_cb_user = NULL;

struct glucose_result_listener {
	glucose_result_callback_t cb;
	void *user_data;
};

static struct glucose_result_listener glucose_result_listeners[2];

/* Fasting insulin value for HOMA-IR calculation (default from Kconfig) */
static float glucose_fasting_insulin_uiu_ml = DEFAULT_FASTING_INSULIN_UIU_ML;
static K_MUTEX_DEFINE(glucose_insulin_mutex);

static uint16_t glucose_prog_taken;
static uint16_t glucose_prog_target;

/* Live-display ROC (rate-of-change) guard state.
 *
 * See the guard logic in glucose_handle_result() for the reasoning. This
 * tracks the last reading that passed the guard, separate from any
 * historical-record logging — the CSV/record store below always gets the
 * true raw result, this only affects what the phone/LCD show live. */
static float glucose_last_good_mg_dl;
static int64_t glucose_last_good_timestamp;
static bool glucose_have_last_good;

/* Forward declarations */
static void glucose_sample_trigger_handler(const struct device *dev,
					   const struct sensor_trigger *trigger);
static void glucose_complete_trigger_handler(const struct device *dev,
					     const struct sensor_trigger *trigger);

/**
 * @brief Per-sample trigger handler (fires for each ADC sample)
 */
static void glucose_sample_trigger_handler(const struct device *dev,
					   const struct sensor_trigger *trigger)
{
	ARG_UNUSED(trigger);
	
	/* Get per-sample data */
	struct glucose_sample_data sample;
	int ret = glucose_sensor_get_sample_data(dev, &sample);
	if (ret < 0) {
		return;
	}
	
	/* Notify registered listener (UI or other consumer) */
	if (glucose_sample_cb != NULL) {
		glucose_sample_cb(&sample, glucose_sample_cb_user);
	}

	glucose_prog_taken = (uint16_t)(sample.sample_number + 1U);
	glucose_prog_target = sample.total_samples;

#if defined(CONFIG_BT)
	/* Stream per-sample values so desktop can plot live measurement waveform. */
	int32_t voltage_mv_x100 = (int32_t)(sample.voltage_mv * 100.0f);
	if (voltage_mv_x100 > INT16_MAX) {
		voltage_mv_x100 = INT16_MAX;
	} else if (voltage_mv_x100 < INT16_MIN) {
		voltage_mv_x100 = INT16_MIN;
	}

	uint32_t ts = (sample.timestamp_sec > 0) ? (uint32_t)sample.timestamp_sec : 0U;
	struct ble_glucose_sample_data ble_sample = {
		.sample_number = sample.sample_number,
		.total_samples = sample.total_samples,
		.raw_adc_value = sample.raw_adc_value,
		.voltage_mv_x100 = (int16_t)voltage_mv_x100,
		.timestamp = ts,
	};
	(void)ble_notify_glucose_sample_data(&ble_sample);

	/* Also publish progress updates during sampling. */
	uint16_t taken = (uint16_t)(sample.sample_number + 1U);
	uint16_t target = sample.total_samples;
	uint8_t pct = 0U;
	if (target > 0U) {
		uint32_t scaled = ((uint32_t)taken * 100U) / (uint32_t)target;
		pct = (uint8_t)MIN(scaled, 100U);
	}
	struct ble_meas_status ms_live = {
		.active           = 1,
		.type             = BLE_MEAS_TYPE_GLUCOSE,
		.percent_complete = pct,
		.quality          = 0,
		.samples_taken    = taken,
		.samples_target   = target,
	};
	(void)ble_notify_meas_status(&ms_live);
#endif
}

/**
 * @brief Common glucose measurement result handler
 *
 * Handles logging, CSV export, beeps, and USB re-enable for both
 * UI-triggered and RTC-scheduled measurements.
 */
void glucose_handle_result(struct glucose_sensor_result *result,
			   const char *source,
			   bool reenable_usb)
{
	ARG_UNUSED(reenable_usb);

	if (result == NULL) {
		LOG_ERR("glucose_handle_result: NULL result");
		return;
	}
	
	/* Log completion */
	LOG_INF("%s glucose measurement completed:", source ? source : "Unknown");
	LOG_INF("  Timestamp: %lld", result->timestamp);
	LOG_INF("  Glucose: %.1f mg/dL (%.2f mmol/L)",
		(double)result->glucose_mg_dl,
		(double)result->glucose_mmol_l);
	
#if CONFIG_GLUCOSE_SENSOR_SIGNAL_QUALITY_METRICS
	if (result->quality.snr_db > 0.0f) {
		LOG_INF("  SNR: %.1f dB, Quality: %.2f",
			(double)result->quality.snr_db,
			(double)result->quality.quality_score);
	}
#endif

	/* Live-display ROC guard: catch measurement artifacts SNR/quality
	 * checks miss. health_sched.c already flags ROC > 3 mg/dL/min as
	 * "critical" (real diabetic swings can genuinely hit that), but only
	 * to speed up re-measurement — it doesn't stop a bad single reading
	 * from being shown/notified immediately. On-device log evidence: two
	 * consecutive real readings 125 -> 253 mg/dL, ~2.4 min apart (ROC
	 * 52.6 mg/dL/min) — roughly 17x the existing "critical" threshold,
	 * both passing quality_score=1.00. Even extreme real physiology
	 * (IV dextrose, severe insulin reaction) rarely sustains double-digit
	 * mg/dL/min for long, so a jump this large is far more likely a
	 * signal-chain artifact than genuine glucose movement.
	 *
	 * CHANGED 2026-08-31 (explicit request): this used to hold the
	 * guarded value for BLE/live-display only and always write the raw,
	 * unguarded value to the record store — on the reasoning that the
	 * permanent record should reflect what was actually measured. That
	 * meant an implausible spike (e.g. 92 -> 239 mg/dL in ~2.5 minutes)
	 * still showed up untouched in the Excel export, which is exactly
	 * the "disturbance" that export is meant to avoid for anyone reading
	 * it. Same guard, now applied to both: the value written to the
	 * record store is the same held value sent over BLE. The raw
	 * measurement is never silently lost — it's still fully visible in
	 * this LOG_WRN line below, so anyone auditing the terminal log can
	 * see exactly what the sensor actually produced and when the guard
	 * intervened; it's just not what ends up in the permanent record. */
	float glucose_live_mg_dl = result->glucose_mg_dl;
	bool glucose_roc_guard_triggered = false;

	if ((result->flags & GLUCOSE_SENSOR_FLAG_VALID) && glucose_have_last_good) {
		/*
		 * BUGFIX (2026-09-01, root cause of "glucose stuck in Excel"):
		 * result->timestamp is in SECONDS (see glucose_sensor.c —
		 * both branches store unix_secs or k_uptime_get()/1000, never
		 * milliseconds). This line used to be named dt_ms and get fed
		 * straight into "/ 60000.0f" as if it already were
		 * milliseconds — so a real ~150 second gap between
		 * measurements (2.5 minutes) computed as dt_min = 150/60000 =
		 * 0.0025, correctly rounding to the "0.0 min" seen in the log
		 * ("raw 84.0 -> 86.0 over 0.0 min"). Dividing a normal 2 mg/dL
		 * change by a near-zero time made the computed ROC look
		 * astronomically large (816, 407, 268, 298, 236 mg/dL/min in
		 * one session) on every single measurement after the first,
		 * so the guard fired every time and the record store baseline
		 * (glucose_last_good_mg_dl) never advanced past that first
		 * reading — exactly the "always 84" symptom. This bug existed
		 * before the 2026-08-31 change; extending the guard to also
		 * gate the record store that day is what made a previously
		 * BLE/UI-only cosmetic issue show up in the permanent Excel
		 * export. Fixed at the source: treat the delta as seconds.
		 */
		int64_t dt_sec = result->timestamp - glucose_last_good_timestamp;

		if (dt_sec > 0) {
			float dt_min = (float)dt_sec / 60.0f;
			float roc = fabsf(result->glucose_mg_dl - glucose_last_good_mg_dl) / dt_min;

			if (roc > GLUCOSE_LIVE_ROC_IMPLAUSIBLE_MG_DL_MIN) {
				LOG_WRN("Glucose ROC implausible: %.1f mg/dL/min "
					"(raw %.1f -> %.1f over %.1f min) — holding "
					"last good %.1f mg/dL for BLE/UI *and* the "
					"record store this time (see 2026-08-31 note "
					"above); raw value is only in this log line",
					(double)roc, (double)glucose_last_good_mg_dl,
					(double)result->glucose_mg_dl, (double)dt_min,
					(double)glucose_last_good_mg_dl);
				glucose_live_mg_dl = glucose_last_good_mg_dl;
				result->glucose_mg_dl = glucose_last_good_mg_dl;
				result->glucose_mmol_l = glucose_last_good_mg_dl / 18.0182f;
				glucose_roc_guard_triggered = true;
			}
		}
	}

	if ((result->flags & GLUCOSE_SENSOR_FLAG_VALID) &&
	    !glucose_roc_guard_triggered) {
		/*
		 * BUGFIX (2026-08-31): this used to compare
		 * glucose_live_mg_dl == result->glucose_mg_dl to decide
		 * whether this reading passed the guard and should become
		 * the new baseline. That comparison stopped working once the
		 * guard branch above started overwriting result->glucose_mg_dl
		 * too (both sides of the comparison became the same value
		 * whenever the guard fired, always reading as "passed"). Use
		 * the explicit flag set above instead — a rejected reading
		 * must not become the new baseline, or a single implausible
		 * spike could anchor the guard to a bad reference point.
		 */
		glucose_last_good_mg_dl = result->glucose_mg_dl;
		glucose_last_good_timestamp = result->timestamp;
		glucose_have_last_good = true;
	}

	if (result->flags & GLUCOSE_SENSOR_FLAG_VALID) {
		/* Log measurement to CSV file */
		int ret = glucose_logger_log_measurement(result);
		if (ret < 0) {
			LOG_ERR("Failed to log measurement: %d", ret);
			buzzer_play_error();
		} else {
			char file_path[128];
			if (glucose_logger_get_current_file(file_path, sizeof(file_path)) == 0) {
				LOG_INF("  Logged to: %s", file_path);
			}
			buzzer_play_success();
		}

#if defined(CONFIG_BT)
		/* Notify glucose data over BLE */
		uint32_t ts = (uint32_t)result->timestamp;
		struct ble_glucose_data gd = {
			.glucose_mg_dl = (uint16_t)glucose_live_mg_dl,
			.quality = (uint8_t)MIN((int)(result->quality.quality_score * 100.0f), 100),
			.timestamp = ts,
		};
		ble_notify_glucose_data(&gd);

#if defined(CONFIG_APP_FEATURE_CLOUD_TELEMETRY)
		cloud_telemetry_publish_glucose(&gd);
#endif

		/* Notify full algorithm details so desktop app can log all 40 columns */
		if (result->algorithm_details != NULL) {
			const struct glucose_algorithm_details *a = result->algorithm_details;
			struct ble_glucose_algo_data ad = {
				.tot_coeff          = (float)a->tot_coeff,
				.intercept          = (float)a->intercept,
				.y1_value           = (float)a->y1_value,
				.avg_val            = (float)a->avg_val,
				.std_dev            = (float)a->std_dev,
				.up_lim             = (float)a->up_lim,
				.ll_lim             = (float)a->ll_lim,
				.p_count            = a->p_count,
				.n_count            = a->n_count,
				.p_val              = (float)a->p_val,
				.n_val              = (float)a->n_val,
				.p_plus_n           = (float)a->p_plus_n,
				.y2_val             = (float)a->y2_val,
				.y2_percent         = (float)a->y2_percent,
				.group_cd           = a->group_cd,
				.y2_factor          = (float)a->y2_factor,
				.y2_factor_val      = (float)a->y2_factor_val,
				.const_val          = (float)a->const_val,
				.y3_value           = (float)a->y3_value,
				.y3_row_no          = a->y3_row_no,
				.elim_per           = (float)a->elim_per,
				.elim_val           = (float)a->elim_val,
				.y_value            = a->y_value,
				.calibration_factor = (float)a->calibration_factor,
				.ag_adjusted        = (float)a->ag_adjusted,
				.normalized_glucose = (float)a->normalized_glucose,
				.actual_insulin     = (float)a->actual_insulin,
				.insulin_correction = (float)a->insulin_correction,
				.insulin_ratio      = (float)a->insulin_ratio,
				.inverse_ratio      = (float)a->inverse_ratio,
				.homa_ir_index      = (float)a->homa_ir_index,
			};
			int algo_ret = ble_notify_glucose_algo_data(&ad);
			if (algo_ret) {
				LOG_WRN("BLE algo notify failed: %d (payload=%u)",
					algo_ret, (unsigned)sizeof(ad));
			}
		}

		/* Notify measurement complete */
		struct ble_meas_status ms_done = {
			.active           = 0,
			.type             = BLE_MEAS_TYPE_GLUCOSE,
			.percent_complete = 100,
			.quality          = gd.quality,
			.samples_taken    = 0,
			.samples_target   = 0,
		};
		ble_notify_meas_status(&ms_done);
#endif /* CONFIG_BT */
	} else {
		LOG_ERR("%s glucose measurement failed - invalid result", source ? source : "Unknown");
		buzzer_play_warning();
	}
}

/**
 * @brief Measurement complete trigger handler (fires once per measurement)
 */
static void glucose_complete_trigger_handler(const struct device *dev,
					     const struct sensor_trigger *trigger)
{
	ARG_UNUSED(trigger);
	
	/* Get final result */
	struct glucose_sensor_result result;
	int ret = glucose_sensor_get_result(dev, &result);
	if (ret < 0) {
		LOG_ERR("Failed to get measurement result: %d", ret);
		return;
	}

	glucose_handle_result(&result, "Measurement", false);
	
	/* Notify all registered listeners (scheduler + UI screens). */
	for (size_t i = 0; i < ARRAY_SIZE(glucose_result_listeners); i++) {
		if (glucose_result_listeners[i].cb != NULL) {
			glucose_result_listeners[i].cb(&result,
				glucose_result_listeners[i].user_data);
		}
	}
}

int glucose_register_sample_callback(glucose_sample_callback_t cb, void *user_data)
{
	glucose_sample_cb = cb;
	glucose_sample_cb_user = user_data;
	return 0;
}

static int glucose_add_result_listener(glucose_result_callback_t cb, void *user_data)
{
	if (cb == NULL) {
		return -EINVAL;
	}

	for (size_t i = 0; i < ARRAY_SIZE(glucose_result_listeners); i++) {
		if (glucose_result_listeners[i].cb == cb) {
			glucose_result_listeners[i].user_data = user_data;
			return 0;
		}
	}

	for (size_t i = 0; i < ARRAY_SIZE(glucose_result_listeners); i++) {
		if (glucose_result_listeners[i].cb == NULL) {
			glucose_result_listeners[i].cb = cb;
			glucose_result_listeners[i].user_data = user_data;
			return 0;
		}
	}

	return -ENOSPC;
}

int glucose_register_result_callback(glucose_result_callback_t cb, void *user_data)
{
	if (cb == NULL) {
		for (size_t i = 0; i < ARRAY_SIZE(glucose_result_listeners); i++) {
			glucose_result_listeners[i].cb = NULL;
			glucose_result_listeners[i].user_data = NULL;
		}
		return 0;
	}

	return glucose_add_result_listener(cb, user_data);
}

int glucose_unregister_result_callback(glucose_result_callback_t cb)
{
	if (cb == NULL) {
		return -EINVAL;
	}

	for (size_t i = 0; i < ARRAY_SIZE(glucose_result_listeners); i++) {
		if (glucose_result_listeners[i].cb == cb) {
			glucose_result_listeners[i].cb = NULL;
			glucose_result_listeners[i].user_data = NULL;
			return 0;
		}
	}

	return -ENOENT;
}

int glucose_init(void)
{
	int ret;
	
	if (glucose_dev == NULL) {
		LOG_ERR("Glucose sensor device not found in device tree");
		return -ENODEV;
	}
	
	if (!device_is_ready(glucose_dev)) {
		LOG_ERR("Glucose sensor device not ready");
		return -ENODEV;
	}

	enum product_hw_variant sku = product_hw_detect();
	enum glucose_hw_variant hw = glucose_hw_variant_from_product(sku);
	int profile_ret = glucose_algorithm_select_profile(hw);
	int matrix_ret = 0;

	if (profile_ret < 0) {
		LOG_ERR("Glucose profile selection failed: %d", profile_ret);
	}

	LOG_INF("Glucose SKU from product detect: %s → profile tag %d",
		product_hw_variant_name(sku), (int)hw);

#if GLUCOSE_RUNTIME_MATRICES
	if (profile_ret == 0) {
		matrix_ret = glucose_algorithm_init();
	}
#endif

	if (profile_ret == 0) {
		const struct glucose_algorithm_profile *profile =
			glucose_algorithm_get_active_profile();

#if GLUCOSE_RUNTIME_MATRICES
		if (matrix_ret < 0) {
			LOG_ERR("Glucose matrix validation failed: %d", matrix_ret);
			LOG_WRN("Predictions may be inaccurate without a valid model");
			glucose_model_ok = false;
		} else {
			glucose_model_ok = true;
		}
		LOG_INF("Glucose profile: %s | %u groups | intercept=%.4f k=%.2f | model %s",
			profile->name,
			profile->num_groups,
			profile->intercept,
			profile->outlier_k_factor,
			matrix_ret == 0 ? "OK" : "MISSING");
#else
		LOG_INF("Glucose profile: %s | %u groups | intercept=%.4f k=%.2f | matrices=embedded",
			profile->name,
			profile->num_groups,
			profile->intercept,
			profile->outlier_k_factor);
#endif
	}

	/* Initialize data logger */
	ret = glucose_logger_init();
	if (ret < 0) {
		LOG_ERR("Failed to initialize glucose logger: %d", ret);
		return ret;
	}
	
	/* Register trigger handlers (Zephyr sensor API) */
	struct sensor_trigger sample_trig = {
		.type = GLUCOSE_TRIG_SAMPLE_READY,
		.chan = SENSOR_CHAN_ALL
	};
	ret = glucose_sensor_trigger_set(glucose_dev, &sample_trig, glucose_sample_trigger_handler);
	if (ret < 0) {
		LOG_ERR("Failed to set sample trigger: %d", ret);
		return ret;
	}
	LOG_INF("Per-sample trigger handler registered");
	
	struct sensor_trigger complete_trig = {
		.type = GLUCOSE_TRIG_MEASUREMENT_COMPLETE,
		.chan = SENSOR_CHAN_ALL
	};
	ret = glucose_sensor_trigger_set(glucose_dev, &complete_trig, glucose_complete_trigger_handler);
	if (ret < 0) {
		LOG_ERR("Failed to set complete trigger: %d", ret);
		return ret;
	}
	LOG_INF("Measurement complete trigger handler registered");
	
	/* Perform dark current calibration if enabled */
#if CONFIG_GLUCOSE_SENSOR_AUTO_CALIBRATE
	LOG_INF("Performing dark current calibration...");
	ret = glucose_sensor_calibrate(glucose_dev, 0.0f);
	if (ret) {
		LOG_WRN("Dark current calibration failed: %d", ret);
	} else {
		LOG_INF("Dark current calibration completed");
	}
#else
	LOG_INF("Skipping auto-calibration (using device tree value or manual calibration)");
#endif
	
	glucose_ready = true;
	LOG_INF("Glucose sensor initialized");
	
	/* Initialize periodic work queue for timer-triggered measurements */
	if (!glucose_work_initialized) {
		k_work_init(&glucose_periodic_work, glucose_periodic_work_handler);
		glucose_work_initialized = true;
		LOG_INF("Glucose periodic work queue initialized");
	}
	
#if defined(CONFIG_APP_HEALTH_SCHED)
	/* The agentic health scheduler now owns automatic measurement timing and
	 * sequences glucose after PPG/temp. Do NOT start the standalone glucose
	 * timer — it would double-trigger and contend for the optical path. */
	LOG_INF("Periodic glucose timer owned by health scheduler (standalone timer disabled)");
#else
	/* Start periodic glucose sampling timer (10-minute intervals) */
	LOG_INF("Starting periodic glucose sampling timer (every %d minutes)",
		GLUCOSE_SAMPLE_INTERVAL_MIN);
	k_timer_start(&glucose_timer, K_MINUTES(GLUCOSE_SAMPLE_INTERVAL_MIN),
		      K_MINUTES(GLUCOSE_SAMPLE_INTERVAL_MIN));
#endif
	
	return 0;
}

bool glucose_model_ready(void)
{
	return glucose_model_ok;
}

int glucose_model_revalidate(void)
{
#if GLUCOSE_RUNTIME_MATRICES
	int ret = glucose_algorithm_init();

	glucose_model_ok = (ret == 0);
	return ret;
#else
	glucose_model_ok = true;
	return 0;
#endif
}

int glucose_trigger_measurement(void)
{
	if (!glucose_ready) {
		LOG_ERR("Glucose sensor not initialized");
		return -EIO;
	}
	
	return glucose_sensor_trigger_measurement(glucose_dev);
}

int glucose_start_measurement_from_ui(void)
{
	if (!glucose_ready) {
		LOG_ERR("Glucose sensor not initialized");
		return -EIO;
	}
	
	/* Set fasting insulin value from module variable */
	float fasting_insulin_uiu_ml;
	k_mutex_lock(&glucose_insulin_mutex, K_FOREVER);
	fasting_insulin_uiu_ml = glucose_fasting_insulin_uiu_ml;
	k_mutex_unlock(&glucose_insulin_mutex);

	int ret = glucose_sensor_set_fasting_insulin(glucose_dev, fasting_insulin_uiu_ml);
	if (ret < 0) {
		LOG_WRN("Failed to set fasting insulin on device: %d", ret);
		/* Continue anyway with default value */
	}
	
	/* Reset RTC glucose sample counter (next auto-sample in 10min) */
	glucose_reset_timer();

	glucose_prog_taken = 0U;
	glucose_prog_target = 0U;
	uint16_t ns = 80U;

	if (glucose_get_config(&ns, NULL) == 0 && ns > 0U) {
		glucose_prog_target = ns;
	}
	
	return glucose_sensor_trigger_measurement(glucose_dev);
}

int glucose_get_result(struct glucose_sensor_result *result)
{
	if (!glucose_ready) {
		return -ENODEV;
	}
	
	return glucose_sensor_get_result(glucose_dev, result);
}

bool glucose_is_measuring(void)
{
	if (!glucose_ready) {
		return false;
	}
	
	/* Get current state from driver */
	int state = glucose_sensor_get_state(glucose_dev);
	
	/* Return true if not idle */
	return (state != 0);  /* GLUCOSE_SENSOR_STATE_IDLE = 0 */
}

int glucose_abort_measurement(void)
{
	int ret;

	if (!glucose_ready) {
		return -ENODEV;
	}

	ret = glucose_sensor_abort_measurement(glucose_dev);

	return ret;
}

int glucose_get_state(void)
{
	if (!glucose_ready) {
		return -ENODEV;
	}
	
	return glucose_sensor_get_state(glucose_dev);
}

int glucose_get_measurement_progress(uint16_t *taken, uint16_t *target)
{
	if (!glucose_ready || !glucose_is_measuring()) {
		return -ENODATA;
	}
	if (taken != NULL) {
		*taken = glucose_prog_taken;
	}
	if (target != NULL) {
		*target = (glucose_prog_target > 0U) ? glucose_prog_target : 80U;
	}
	return 0;
}

int glucose_get_config(uint16_t *num_samples, uint16_t *delay_ms)
{
	if (!glucose_ready) {
		return -ENODEV;
	}
	
	return glucose_sensor_get_config(glucose_dev, num_samples, delay_ms);
}

int glucose_set_config(uint16_t num_samples, uint16_t delay_ms)
{
	if (!glucose_ready) {
		return -ENODEV;
	}

	return glucose_sensor_configure(glucose_dev, num_samples, delay_ms);
}

/**
 * @brief Work handler for periodic glucose measurement
 * 
 * Deferred from timer ISR to thread context to allow blocking operations
 * (USB disable uses mutexes which cannot be used in ISR context).
 */
static void glucose_periodic_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);
	
	LOG_INF("Periodic glucose work: starting measurement");

	/* Trigger glucose measurement (async) */
	int ret = glucose_trigger_measurement();
	if (ret < 0) {
		LOG_ERR("Failed to trigger glucose measurement: %d", ret);
	} else {
		LOG_INF("Glucose measurement triggered successfully");
	}
}

/**
 * @brief Periodic glucose sampling timer handler
 * 
 * Fires every 10 minutes to trigger automatic glucose measurement.
 * Uses kernel timer (k_timer) for hardware independence.
 * 
 * NOTE: Runs in ISR context - must not perform blocking operations!
 * Submits work to system workqueue for actual measurement trigger.
 */
static void glucose_periodic_timer_handler(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	
	LOG_INF("Periodic timer: %d minutes elapsed, submitting work for glucose measurement",
		    GLUCOSE_SAMPLE_INTERVAL_MIN);
	
	/* Submit work to system workqueue (thread context)
	 * Cannot call usb_msc_disable() here - ISR context prohibits mutex usage */
	k_work_submit(&glucose_periodic_work);
}

void glucose_reset_timer(void)
{
#if defined(CONFIG_APP_HEALTH_SCHED)
	/* The health scheduler owns automatic timing; the standalone glucose
	 * timer is never started, so do NOT (re)arm it here — that would
	 * double-trigger measurements and contend for the optical path. */
	LOG_DBG("Glucose timer reset ignored (owned by health scheduler)");
#else
	/* Restart timer from now (resets 10-minute countdown) */
	k_timer_start(&glucose_timer, K_MINUTES(GLUCOSE_SAMPLE_INTERVAL_MIN),
		      K_MINUTES(GLUCOSE_SAMPLE_INTERVAL_MIN));
	LOG_INF("Glucose timer reset - next auto-sample in %d minutes",
		GLUCOSE_SAMPLE_INTERVAL_MIN);
#endif
}

bool glucose_is_timer_active(void)
{
	return (k_timer_status_get(&glucose_timer) > 0 ||
		k_timer_remaining_get(&glucose_timer) > 0);
}

int glucose_set_fasting_insulin(float insulin_uiu_ml)
{
	if (insulin_uiu_ml < 0.0f) {
		LOG_ERR("Invalid fasting insulin value: %.2f (must be >= 0)",
			(double)insulin_uiu_ml);
		return -EINVAL;
	}
	
	k_mutex_lock(&glucose_insulin_mutex, K_FOREVER);
	glucose_fasting_insulin_uiu_ml = insulin_uiu_ml;
	k_mutex_unlock(&glucose_insulin_mutex);
	
	LOG_INF("Fasting insulin set to %.2f uIU/mL", (double)insulin_uiu_ml);
	return 0;
}

int glucose_get_fasting_insulin(float *insulin_uiu_ml)
{
	if (insulin_uiu_ml == NULL) {
		return -EINVAL;
	}
	
	k_mutex_lock(&glucose_insulin_mutex, K_FOREVER);
	*insulin_uiu_ml = glucose_fasting_insulin_uiu_ml;
	k_mutex_unlock(&glucose_insulin_mutex);
	
	return 0;
}
