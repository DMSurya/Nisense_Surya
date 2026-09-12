/*
 * PPG measurement interface - helper functions for subsystem access
 *
 * Provides utility functions that wrap PPG algorithm subsystem API.
 */

#include "ppg.h"
#include "ppg_logger.h"
#include "ui_common.h"
#include "ui_events.h"
#include "buzzer.h"
#include "rtc.h"
#include <zephyr/kernel.h>
#include <zephyr/subsys/ppg_algo/ppg_algo.h>
#include <zephyr/drivers/sensor.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#if defined(CONFIG_BT)
#include "ble_gatt.h"
#endif
#if defined(CONFIG_APP_FEATURE_CLOUD_TELEMETRY)
#include "cloud_telemetry.h"
#endif

LOG_MODULE_REGISTER(ppg, LOG_LEVEL_DBG);

/* VCNL3040 proximity sensor — used to detect skin contact before and after measurement.
 * Strong guard: fail-closed if the sensor is absent or unreadable. */
#define PPG_VCNL3040_NODE DT_ALIAS(vcnl3040)
/* Consecutive no-contact samples required before aborting an active run.
 * At 25 Hz this is ~200 ms, enough to ignore brief proximity glitches. */
#define PPG_PROX_ABORT_MISS_SAMPLES ((uint8_t)CONFIG_PPG_PROX_ABORT_MISS_SAMPLES)
#if DT_NODE_HAS_STATUS(PPG_VCNL3040_NODE, okay)
static const struct device *ppg_prox_dev = DEVICE_DT_GET(PPG_VCNL3040_NODE);
#define PPG_PROX_AVAILABLE 1
#else
static const struct device *ppg_prox_dev;
#define PPG_PROX_AVAILABLE 0
#endif

static bool ppg_initialized = false;
static bool ppg_core_triggers_registered;
static ppg_trigger_handler_t sample_handlers[2];
static ppg_trigger_handler_t complete_handlers[2];
static bool measurement_active;
static uint16_t active_target_samples;
static uint16_t last_samples_taken;
static uint16_t last_samples_target;
static bool ppg_prox_bypass;
static bool ppg_prox_force_disabled; /* Persistent bench/test: ignore wear for PPG */
#define PPG_PROX_GUARD_ENABLED IS_ENABLED(CONFIG_PPG_PROX_GUARD_ENABLE)
/* Proximity contact state snapshotted at measurement end; used to override result. */
static bool ppg_prox_no_contact_at_complete;
static uint8_t ppg_prox_miss_streak;
static bool ppg_prox_abort_requested;
static struct k_work_delayable ppg_prox_guard_work;
static bool ppg_prox_guard_work_inited;

static bool ppg_prox_guard_suppressed(void)
{
	return ppg_prox_bypass || ppg_prox_force_disabled;
}

/* Progress/log throttling for per-sample callback path.
 * Keeping these finite avoids RTT/BLE callback overhead from stretching
 * a 25 Hz, 300-sample run into minutes under heavy logging. */
#define PPG_PROGRESS_NOTIFY_EVERY_N_SAMPLES ((uint16_t)CONFIG_PPG_PROGRESS_NOTIFY_EVERY_N_SAMPLES)
#define PPG_SAMPLE_LOG_INITIAL_SAMPLES      ((uint16_t)CONFIG_PPG_SAMPLE_LOG_INITIAL_SAMPLES)

#if defined(CONFIG_BT)
struct ppg_ble_notify_counters {
	uint32_t meas_status_attempts;
	uint32_t meas_status_success;
	uint32_t meas_status_fail;
	uint32_t vitals_attempts;
	uint32_t vitals_success;
	uint32_t vitals_fail;
};

static struct ppg_ble_notify_counters notify_counters;

static void log_notify_counters(const char *context)
{
	LOG_INF("BLE notify counters (%s): ms=%u/%u fail=%u vitals=%u/%u fail=%u",
		context,
		notify_counters.meas_status_success,
		notify_counters.meas_status_attempts,
		notify_counters.meas_status_fail,
		notify_counters.vitals_success,
		notify_counters.vitals_attempts,
		notify_counters.vitals_fail);
}

static void notify_meas_status_counted(const struct ble_meas_status *ms)
{
	int ret;

	notify_counters.meas_status_attempts++;
	ret = ble_notify_meas_status(ms);
	if (ret == 0) {
		notify_counters.meas_status_success++;
	} else {
		notify_counters.meas_status_fail++;
		LOG_WRN("ble_notify_meas_status failed: %d", ret);
	}
}

static void notify_vitals_counted(const struct ble_vitals_data *v)
{
	int ret;

	notify_counters.vitals_attempts++;
	ret = ble_notify_vitals_data(v);
	if (ret == 0) {
		notify_counters.vitals_success++;
	} else {
		notify_counters.vitals_fail++;
		LOG_WRN("ble_notify_vitals_data failed: %d", ret);
	}
}
#endif

/**
 * @brief Query the VCNL3040 for current skin contact.
 * @return true if contact detected; false if no contact or sensor unavailable.
 */
static bool ppg_prox_contact_ok(void)
{
#if PPG_PROX_AVAILABLE
	if (!device_is_ready(ppg_prox_dev)) {
		return false;
	}
	struct sensor_value wear_val;
	if (sensor_sample_fetch(ppg_prox_dev) < 0 ||
	    sensor_channel_get(ppg_prox_dev, SENSOR_CHAN_DISTANCE, &wear_val) < 0) {
		return false;
	}
	/* Require WORN_GOOD (val1==2) — WORN_LOOSE (val1==1) is a transitional
	 * state during removal and must NOT be treated as contact-OK. */
	return (wear_val.val1 >= 2);
#else
	return false;
#endif
}

/* Start-path contact check: warms up the IIR filter before deciding.
 *
 * The driver IIR filter (alpha=0.25 on rising edge) starts at 0 at boot.
 * ppg_prox_contact_ok() does one fetch per call, so a cold sensor needs
 * ~4 calls before the filtered value converges to WORN_GOOD. The guard
 * path is fine (sensor stays warm during measurement), but on the start
 * path the user has just placed their hand and only one fetch has run.
 *
 * This function does 4 back-to-back fetches (~12 ms total at IT=8T) so
 * the filter is saturated before the go/no-go decision, then logs the
 * actual values for tuning diagnostics.
 */
static bool ppg_prox_start_check(void)
{
#if PPG_PROX_AVAILABLE
	if (!device_is_ready(ppg_prox_dev)) {
		LOG_WRN("PPG start check: proximity sensor not ready");
		return false;
	}
	/* Warm up the IIR filter */
	for (int i = 0; i < 4; i++) {
		int fetch_ret = sensor_sample_fetch(ppg_prox_dev);
		if (fetch_ret < 0) {
			LOG_WRN("PPG prox start: fetch[%d] failed: %d", i, fetch_ret);
		}
	}
	struct sensor_value wear_val;
	struct sensor_value prox_val = {0};
	if (sensor_channel_get(ppg_prox_dev, SENSOR_CHAN_DISTANCE, &wear_val) < 0) {
		LOG_WRN("PPG start check: proximity channel read failed");
		return false;
	}
	(void)sensor_channel_get(ppg_prox_dev, SENSOR_CHAN_PROX, &prox_val);
	LOG_INF("PPG prox start: raw=%d filt=%d wear=%d (need 2=WORN_GOOD)",
		prox_val.val1, prox_val.val2, (int)wear_val.val1);
	return (wear_val.val1 >= 2);
#else
	return false;
#endif
}

#if defined(CONFIG_BT)
static void notify_vitals_counted(const struct ble_vitals_data *v);
#endif /* CONFIG_BT */

/* Periodic proximity guard. Runs independently of sample callbacks so a
 * contact-loss abort still triggers even if raw samples are being rejected. */
static void ppg_prox_guard_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (!PPG_PROX_GUARD_ENABLED || ppg_prox_guard_suppressed() || !measurement_active || ppg_prox_abort_requested) {
		return;
	}

	if (!ppg_prox_contact_ok()) {
		if (ppg_prox_miss_streak < UINT8_MAX) {
			ppg_prox_miss_streak++;
		}

		if (ppg_prox_miss_streak >= PPG_PROX_ABORT_MISS_SAMPLES) {
			int stop_ret;

			ppg_prox_abort_requested = true;
			ppg_prox_no_contact_at_complete = true;
			LOG_WRN("PPG aborting: contact lost (periodic guard) after %u misses",
				(unsigned)ppg_prox_miss_streak);
			stop_ret = ppg_stop_measurement();
			if (stop_ret) {
				LOG_WRN("PPG stop after periodic contact-loss failed: %d", stop_ret);
			}
			return;
		}
	} else {
		ppg_prox_miss_streak = 0U;
	}

	if (measurement_active && !ppg_prox_abort_requested) {
		(void)k_work_reschedule(&ppg_prox_guard_work, K_MSEC(40));
	}
}

static void ppg_sample_dispatch(enum ppg_trigger_type type, void *user_data);
static void ppg_complete_dispatch(enum ppg_trigger_type type, void *user_data);
#if defined(CONFIG_PPG_ALGO_STAGED_LIVE)
static void ppg_staged_dispatch(enum ppg_trigger_type type, void *user_data);
#endif

#if defined(CONFIG_BT)
static void ppg_ble_notify_vitals_from_result(const struct ppg_algo_result *result)
{
	uint32_t ts = 0;
	uint8_t no_contact;
	struct ble_vitals_data vd;

	if (result == NULL) {
		return;
	}
	rtc_get_unix_time(&ts);
	no_contact = (result->quality_reason &
		      (PPG_QUALITY_REASON_NO_CONTACT |
		       PPG_QUALITY_REASON_LOW_PERFUSION)) ? 1U : 0U;
	vd = (struct ble_vitals_data){
		.hr_bpm = result->hr_bpm,
		.hr_confidence = result->hr_confidence,
		.spo2_percent = (uint8_t)MIN(result->spo2_percent, 100U),
		.spo2_confidence = result->spo2_confidence,
		.hb_g_dl_x10 = result->hb_g_dl_x10,
		.hb_confidence = result->hb_confidence,
		.resp_rate_bpm = result->resp_rate_bpm,
		.resp_confidence = result->resp_confidence,
		.r_value_x1000 = result->r_value_x1000,
		.quality = (uint8_t)(result->quality * 25U),
		.flags = (result->hr_valid ? BIT(0) : 0U) |
			 (result->spo2_valid ? BIT(1) : 0U) |
			 (result->hb_valid ? BIT(2) : 0U) |
			 (result->resp_valid ? BIT(3) : 0U) |
			 (no_contact ? 0U : BIT(4)) |
			 (result->hrv_valid ? BIT(5) : 0U) |
			 (result->bp_valid ? BIT(6) : 0U),
		.timestamp = ts,
		.sdnn_ms = result->sdnn_ms,
		.rmssd_ms = result->rmssd_ms,
		.systolic_mmhg = result->systolic_mmhg,
		.diastolic_mmhg = result->diastolic_mmhg,
	};
	notify_vitals_counted(&vd);
}
#endif

#if defined(CONFIG_PPG_ALGO_STAGED_LIVE)
static void ppg_staged_dispatch(enum ppg_trigger_type type, void *user_data)
{
	ARG_UNUSED(user_data);
	struct ppg_algo_result result;
	struct ui_event ev = {0};

	if (ppg_get_result(&result) != 0) {
		return;
	}

#if defined(CONFIG_BT)
	ppg_ble_notify_vitals_from_result(&result);
#endif

	switch (type) {
	case PPG_TRIG_PARTIAL_VITALS:
		ev.type = UI_EVENT_PPG_PARTIAL_VITALS;
		break;
	case PPG_TRIG_PARTIAL_VASCULAR:
		ev.type = UI_EVENT_PPG_PARTIAL_VASCULAR;
		break;
	case PPG_TRIG_RECORD_READY:
		ev.type = UI_EVENT_PPG_RECORD_READY;
		break;
	default:
		return;
	}
	(void)ui_events_publish(&ev);
}
#endif /* CONFIG_PPG_ALGO_STAGED_LIVE */

static int ppg_register_core_triggers(void)
{
	int ret;

	if (ppg_core_triggers_registered) {
		return 0;
	}

	ret = ppg_algo_trigger_set(PPG_TRIG_SAMPLE_READY, ppg_sample_dispatch, NULL);
	if (ret) {
		return ret;
	}

	ret = ppg_algo_trigger_set(PPG_TRIG_MEASUREMENT_COMPLETE,
				   ppg_complete_dispatch, NULL);
	if (ret) {
		return ret;
	}

#if defined(CONFIG_PPG_ALGO_STAGED_LIVE)
	ret = ppg_algo_trigger_set(PPG_TRIG_PARTIAL_VITALS, ppg_staged_dispatch, NULL);
	if (ret) {
		return ret;
	}
	ret = ppg_algo_trigger_set(PPG_TRIG_PARTIAL_VASCULAR, ppg_staged_dispatch, NULL);
	if (ret) {
		return ret;
	}
	ret = ppg_algo_trigger_set(PPG_TRIG_RECORD_READY, ppg_staged_dispatch, NULL);
	if (ret) {
		return ret;
	}
#endif

	ppg_core_triggers_registered = true;
	return 0;
}

static void ppg_sample_dispatch(enum ppg_trigger_type type, void *user_data)
{
	ARG_UNUSED(type);
	ARG_UNUSED(user_data);
	struct ppg_sample_data sd = {0};
	bool have_sd = (ppg_algo_get_sample_data(&sd) == 0);

	/* Mid-run contact guard: if contact is lost for several consecutive
	 * samples, stop early instead of waiting for a guaranteed-invalid result. */
	if (PPG_PROX_GUARD_ENABLED && !ppg_prox_guard_suppressed() && measurement_active && !ppg_prox_abort_requested) {
		bool prox_no_contact = !ppg_prox_contact_ok();

		if (prox_no_contact) {
			if (ppg_prox_miss_streak < UINT8_MAX) {
				ppg_prox_miss_streak++;
			}
			if (ppg_prox_miss_streak >= PPG_PROX_ABORT_MISS_SAMPLES) {
				int stop_ret;

				ppg_prox_abort_requested = true;
				ppg_prox_no_contact_at_complete = true;
				LOG_WRN("PPG aborting: contact lost (proximity) after %u misses",
					(unsigned)ppg_prox_miss_streak);
				/* Use app wrapper so local measurement state is always reconciled
				 * with backend state on early-abort paths. */
				stop_ret = ppg_stop_measurement();
				if (stop_ret) {
					LOG_WRN("PPG stop after contact loss failed: %d", stop_ret);
				}
			}
		} else {
			ppg_prox_miss_streak = 0U;
		}
	}

	if (have_sd) {
		uint16_t sample_1based = (uint16_t)(sd.sample_number + 1U);

		/* Every sample at DBG — same pattern as glucose Sample[n/N]. */
		LOG_DBG("PPG sample[%u/%u]: IR=%u RED=%u GRN=%u",
			(unsigned)sample_1based,
			(unsigned)sd.total_samples,
			(unsigned)sd.raw_ir,
			(unsigned)sd.raw_red,
			(unsigned)sd.raw_green);
	}

#if defined(CONFIG_BT)
	/* Notify measurement progress */
	if (have_sd) {
		uint16_t sample_1based = (uint16_t)(sd.sample_number + 1U);
		bool is_periodic_progress = ((sample_1based % PPG_PROGRESS_NOTIFY_EVERY_N_SAMPLES) == 0U);
		bool is_last_sample = (sd.total_samples > 0U) && (sample_1based >= sd.total_samples);

		last_samples_taken = sd.sample_number + 1U;
		last_samples_target = sd.total_samples;

		/* Derive per-sample signal quality from perfusion index (AC/DC ratio).
		 * PI = ac_ir / dc_ir.  Clamp: PI >= 2% (pi_x100 >= 200) → quality = 100. */
		uint8_t sample_quality = 0U;
		if (sd.dc_ir > 0U) {
			uint32_t pi_x100 = (uint32_t)((uint64_t)sd.ac_ir * 10000U / sd.dc_ir);
			sample_quality = (pi_x100 >= 200U) ? 100U
					: (uint8_t)((pi_x100 * 100U) / 200U);
		}

		struct ble_meas_status ms = {
			.active           = 1,
			.type             = BLE_MEAS_TYPE_VITALS,
			.percent_complete = (sd.total_samples > 0)
				? (uint8_t)((((uint32_t)sd.sample_number + 1U) * 100U) / sd.total_samples)
				: 0,
			.quality          = sample_quality,
			.samples_taken    = sd.sample_number + 1U,
			.samples_target   = sd.total_samples,
		};
		if (is_periodic_progress || is_last_sample ||
		    sample_1based <= PPG_SAMPLE_LOG_INITIAL_SAMPLES) {
			notify_meas_status_counted(&ms);
		}

#if defined(CONFIG_BLE_PPG_STREAMING)
		/* Stream every captured sample to BLE. */
		struct ble_ppg_sample ps = {
			.sample_num   = sd.sample_number,
			.raw_ir       = sd.raw_ir,
			.raw_red      = sd.raw_red,
			.raw_green    = sd.raw_green,
			.accel_x      = sd.accel_x,
			.accel_y      = sd.accel_y,
			.accel_z      = sd.accel_z,
			.timestamp_ms = sd.timestamp_ms,
		};
		ble_notify_ppg_sample(&ps);
#endif /* CONFIG_BLE_PPG_STREAMING */

#if defined(CONFIG_PPG_ALGO_STAGED_LIVE)
		/* Throttled live vitals during refine / live tail (~1 Hz). */
		if (is_periodic_progress) {
			struct ppg_algo_result live;

			if (ppg_get_result(&live) == 0 &&
			    (live.hr_bpm > 0U || live.spo2_percent > 0U)) {
				ppg_ble_notify_vitals_from_result(&live);
			}
		}
#endif
	}
#endif /* CONFIG_BT */

	for (int i = 0; i < (int)ARRAY_SIZE(sample_handlers); i++) {
		if (sample_handlers[i] != NULL) {
			sample_handlers[i](PPG_TRIG_SAMPLE_READY, NULL);
		}
	}
}

static void ppg_complete_dispatch(enum ppg_trigger_type type, void *user_data)
{
	ARG_UNUSED(type);
	ARG_UNUSED(user_data);

	measurement_active = false;
	(void)k_work_cancel_delayable(&ppg_prox_guard_work);
 	ppg_prox_abort_requested = false;
 	ppg_prox_miss_streak = 0U;

	/* Snapshot proximity state at measurement end so ppg_get_result() can
	 * overlay NO_CONTACT consistently across BLE and UI callers. */
	ppg_prox_no_contact_at_complete = PPG_PROX_GUARD_ENABLED && !ppg_prox_guard_suppressed() &&
		(ppg_prox_no_contact_at_complete || !ppg_prox_contact_ok());
	if (ppg_prox_no_contact_at_complete) {
		LOG_WRN("PPG: proximity sensor reports NO_CONTACT at measurement end");
	}

#if defined(CONFIG_BT)
	/* Publish final result to BLE regardless of validity so clients can update UI. */
	struct ppg_algo_result result;
	struct ppg_sample_data sample_snapshot;
	int16_t accel_x = 0;
	int16_t accel_y = 0;
	int16_t accel_z = 0;
	struct ble_meas_status ms_done = {
		.active           = 0,
		.type             = BLE_MEAS_TYPE_VITALS,
		.percent_complete = 0,
		.quality          = 0,
		.samples_taken    = last_samples_taken,
		.samples_target   = (last_samples_target > 0U)
			? last_samples_target
			: active_target_samples,
	};

	if (ms_done.samples_target > 0U) {
		uint32_t pct = ((uint32_t)ms_done.samples_taken * 100U) / (uint32_t)ms_done.samples_target;
		if (pct > 100U) {
			pct = 100U;
		}
		ms_done.percent_complete = (uint8_t)pct;
	}

	if (ppg_algo_get_sample_data(&sample_snapshot) == 0) {
		accel_x = sample_snapshot.accel_x;
		accel_y = sample_snapshot.accel_y;
		accel_z = sample_snapshot.accel_z;
	}

	if (ppg_get_result(&result) == 0) {
		uint32_t ts = 0;
		rtc_get_unix_time(&ts);

		/* Log quality reason bitmask for RTT diagnostics */
		if (result.quality_reason != PPG_QUALITY_REASON_OK) {
			LOG_WRN("PPG quality gate: 0x%02x [%s%s%s%s%s%s%s%s]",
				result.quality_reason,
				(result.quality_reason & PPG_QUALITY_REASON_NO_CONTACT)         ? "NO_CONTACT " : "",
				(result.quality_reason & PPG_QUALITY_REASON_LOW_PERFUSION)      ? "LOW_PI " : "",
				(result.quality_reason & PPG_QUALITY_REASON_MOTION)             ? "MOTION " : "",
				(result.quality_reason & PPG_QUALITY_REASON_LOW_SNR)            ? "LOW_SNR " : "",
				(result.quality_reason & PPG_QUALITY_REASON_OUT_OF_RANGE)       ? "OOB " : "",
				(result.quality_reason & PPG_QUALITY_REASON_INSUFFICIENT_BEATS) ? "INSUF_BEATS " : "",
				(result.quality_reason & PPG_QUALITY_REASON_LOW_CONFIDENCE)     ? "HR_LOW_CONF " : "",
				(result.quality_reason & PPG_QUALITY_REASON_SPO2_LOW_CONFIDENCE) ? "SPO2_LOW_CONF" : "");
		}

		uint8_t no_contact = (result.quality_reason &
				      (PPG_QUALITY_REASON_NO_CONTACT |
				       PPG_QUALITY_REASON_LOW_PERFUSION)) ? 1U : 0U;

		struct ble_vitals_data vd = {
			.hr_bpm          = result.hr_bpm,
			.hr_confidence   = result.hr_confidence,
			.spo2_percent    = (uint8_t)MIN(result.spo2_percent, 100U),
			.spo2_confidence = result.spo2_confidence,
			.hb_g_dl_x10     = result.hb_g_dl_x10,
			.hb_confidence   = result.hb_confidence,
			.resp_rate_bpm   = result.resp_rate_bpm,
			.resp_confidence = result.resp_confidence,
			.r_value_x1000   = result.r_value_x1000,
			.quality         = (uint8_t)(result.quality * 25U),
			.flags           = (result.hr_valid   ? BIT(0) : 0U) |
					   (result.spo2_valid ? BIT(1) : 0U) |
					   (result.hb_valid   ? BIT(2) : 0U) |
					   (result.resp_valid ? BIT(3) : 0U) |
					   (no_contact        ? 0U     : BIT(4)) |
					   (result.hrv_valid  ? BIT(5) : 0U) |
					   (result.bp_valid   ? BIT(6) : 0U),
			.timestamp       = ts,
			.sdnn_ms         = result.sdnn_ms,
			.rmssd_ms        = result.rmssd_ms,
			.systolic_mmhg   = result.systolic_mmhg,
			.diastolic_mmhg  = result.diastolic_mmhg,
		};
		notify_vitals_counted(&vd);

#if defined(CONFIG_APP_FEATURE_CLOUD_TELEMETRY)
		cloud_telemetry_publish_vitals(&vd);
#endif

		ms_done.percent_complete = 100;
		ms_done.quality = vd.quality;
		ms_done.samples_taken = result.sample_count;
		ms_done.samples_target = (active_target_samples > 0U) ?
			active_target_samples : result.sample_count;
	} else {
		LOG_WRN("PPG completion without final result; sending BLE inactive status (taken=%u, target=%u)",
			(unsigned)ms_done.samples_taken,
			(unsigned)ms_done.samples_target);
	}

	notify_meas_status_counted(&ms_done);
	log_notify_counters("complete_dispatch");
#endif
	active_target_samples = 0U;
	last_samples_taken = 0U;
	last_samples_target = 0U;

	for (int i = 0; i < (int)ARRAY_SIZE(complete_handlers); i++) {
		if (complete_handlers[i] != NULL) {
			complete_handlers[i](PPG_TRIG_MEASUREMENT_COMPLETE, NULL);
		}
	}
}

static int ppg_add_handler_slot(ppg_trigger_handler_t *slots, size_t count,
				ppg_trigger_handler_t handler)
{
	if (handler == NULL) {
		return -EINVAL;
	}

	for (size_t i = 0; i < count; i++) {
		if (slots[i] == handler) {
			return 0;
		}
	}

	for (size_t i = 0; i < count; i++) {
		if (slots[i] == NULL) {
			slots[i] = handler;
			return 0;
		}
	}

	return -ENOSPC;
}

static int ppg_remove_handler_slot(ppg_trigger_handler_t *slots, size_t count,
				   ppg_trigger_handler_t handler)
{
	if (handler == NULL) {
		return -EINVAL;
	}

	for (size_t i = 0; i < count; i++) {
		if (slots[i] == handler) {
			slots[i] = NULL;
			return 0;
		}
	}

	return -ENOENT;
}

int ppg_init(void)
{
	if (ppg_initialized) {
		LOG_INF("PPG already initialized");
		return 0;
	}

	/* Initialize subsystem */
	int ret = ppg_algo_init();
	if (ret) {
		LOG_ERR("PPG subsystem init failed: %d", ret);
		return ret;
	}

	/* Core sample/complete triggers must be present even when HR/SpO2 UI
	 * screens are not created, so BLE-triggered measurements still report
	 * progress/completion and update transport state correctly. */
	ret = ppg_register_core_triggers();
	if (ret) {
		LOG_ERR("Failed to register core PPG triggers: %d", ret);
		return ret;
	}

	/* Initialize logger if any PPG feature is enabled */
#if defined(CONFIG_APP_FEATURE_PPG)
	ret = ppg_logger_init();
	if (ret) {
		LOG_WRN("PPG logger init failed: %d (continuing)", ret);
	}
#endif

	ppg_initialized = true;
	LOG_INF("PPG initialized (vitals enabled)");
	return 0;
}

int ppg_start_measurement(uint32_t sample_count)
{
	if (!ppg_initialized) {
		LOG_ERR("PPG not initialized");
		return -ENODEV;
	}

	/* Refuse to start if proximity sensor confirms no skin contact.
	 * Uses the start-path check (IIR warm-up + diagnostic log) rather than
	 * the lightweight guard-path check to avoid false-blocks at boot.
	 * Returns -ENODATA so callers can show a specific "no contact" message. */
	if (PPG_PROX_GUARD_ENABLED && !ppg_prox_guard_suppressed() && !ppg_prox_start_check()) {
		LOG_WRN("PPG start blocked: proximity sensor indicates no contact");
		return -ENODATA;
	}

	/* Build configuration structure */
	ppg_prox_no_contact_at_complete = false;
	ppg_prox_miss_streak = 0U;
	ppg_prox_abort_requested = false;
	last_samples_taken = 0U;
	last_samples_target = (sample_count > UINT16_MAX) ? UINT16_MAX : (uint16_t)sample_count;
	struct ppg_algo_config config = {
		.sample_rate_hz = CONFIG_PPG_ALGO_SAMPLE_RATE,
		.sample_count = sample_count,
		.motion_rejection = IS_ENABLED(CONFIG_PPG_ALGO_MOTION_REJECTION),
		.quality_threshold = CONFIG_PPG_ALGO_QUALITY_THRESHOLD
	};

	int ret = ppg_algo_start_measurement(&config);
	if (ret == 0) {
		active_target_samples = ppg_algo_get_measurement_target_samples();
		if (active_target_samples == 0U) {
			active_target_samples = (sample_count > UINT16_MAX) ? UINT16_MAX : (uint16_t)sample_count;
		}
		measurement_active = true;

		if (PPG_PROX_GUARD_ENABLED && !ppg_prox_guard_suppressed()) {
			if (!ppg_prox_guard_work_inited) {
				k_work_init_delayable(&ppg_prox_guard_work, ppg_prox_guard_work_handler);
				ppg_prox_guard_work_inited = true;
			}
			(void)k_work_reschedule(&ppg_prox_guard_work, K_MSEC(40));
		}

#if defined(CONFIG_BT)
		/* Emit an initial ACTIVE status immediately so BLE clients do not wait
		 * for first accepted sample to learn a measurement is running. */
		{
			struct ble_meas_status ms_start = {
				.active           = 1,
				.type             = BLE_MEAS_TYPE_VITALS,
				.percent_complete = 0,
				.quality          = 0,
				.samples_taken    = 0,
				.samples_target   = active_target_samples,
			};
			notify_meas_status_counted(&ms_start);
		}
#endif
	}

	return ret;
}

uint16_t ppg_get_measurement_target_samples(void)
{
	return ppg_algo_get_measurement_target_samples();
}

int ppg_stop_measurement(void)
{
	if (!ppg_initialized) {
		LOG_ERR("PPG not initialized");
		return -ENODEV;
	}

	enum ppg_measurement_type stopped_type __unused = PPG_MEAS_VITALS;
	uint16_t stopped_taken = last_samples_taken;
	uint16_t stopped_target = (active_target_samples > 0U) ?
		active_target_samples : last_samples_target;

	int ret = ppg_algo_stop_measurement();
	if (ret == 0) {
		measurement_active = false;
		(void)k_work_cancel_delayable(&ppg_prox_guard_work);
		active_target_samples = 0U;
		ppg_prox_abort_requested = false;
		ppg_prox_miss_streak = 0U;
		last_samples_taken = 0U;
		last_samples_target = 0U;

#if defined(CONFIG_BT)
		{
			struct ble_meas_status ms_stop = {
				.active           = 0,
				.type             = BLE_MEAS_TYPE_VITALS,
				.percent_complete = 0,
				.quality          = 0,
				.samples_taken    = stopped_taken,
				.samples_target   = stopped_target,
			};

			if (ms_stop.samples_target > 0U) {
				uint32_t pct = ((uint32_t)ms_stop.samples_taken * 100U) /
					(uint32_t)ms_stop.samples_target;
				if (pct > 100U) {
					pct = 100U;
				}
				ms_stop.percent_complete = (uint8_t)pct;
			}

			notify_meas_status_counted(&ms_stop);
		}
#endif
		return 0;
	}

	/* If backend already transitioned to idle, treat stop as successful and
	 * self-heal local active flags to avoid stale BLE/UI "already active" state. */
	if (ret == -EALREADY && !ppg_algo_is_measuring()) {
		measurement_active = false;
		(void)k_work_cancel_delayable(&ppg_prox_guard_work);
		active_target_samples = 0U;
		ppg_prox_abort_requested = false;
		ppg_prox_miss_streak = 0U;
		last_samples_taken = 0U;
		last_samples_target = 0U;

#if defined(CONFIG_BT)
		{
			struct ble_meas_status ms_stop = {
				.active           = 0,
				.type             = BLE_MEAS_TYPE_VITALS,
				.percent_complete = 0,
				.quality          = 0,
				.samples_taken    = stopped_taken,
				.samples_target   = stopped_target,
			};

			if (ms_stop.samples_target > 0U) {
				uint32_t pct = ((uint32_t)ms_stop.samples_taken * 100U) /
					(uint32_t)ms_stop.samples_target;
				if (pct > 100U) {
					pct = 100U;
				}
				ms_stop.percent_complete = (uint8_t)pct;
			}

			notify_meas_status_counted(&ms_stop);
		}
#endif
		return 0;
	}

	return ret;
}

bool ppg_is_measurement_active(void)
{
	if (measurement_active && !ppg_algo_is_measuring()) {
		measurement_active = false;
		(void)k_work_cancel_delayable(&ppg_prox_guard_work);
		active_target_samples = 0U;
		ppg_prox_abort_requested = false;
		ppg_prox_miss_streak = 0U;
	}

	return measurement_active;
}

int ppg_get_result(struct ppg_algo_result *result)
{
	if (!ppg_initialized) {
		return -ENODEV;
	}

	int ret = ppg_algo_get_result(result);

	if (ret == 0 && ppg_prox_no_contact_at_complete && !ppg_prox_guard_suppressed()) {
		/* Override: proximity sensor independently confirmed no contact
		 * at measurement end — ensure both BLE and UI see NO_CONTACT. */
		result->quality_reason |= PPG_QUALITY_REASON_NO_CONTACT;
		result->hr_valid   = false;
		result->spo2_valid = false;
	}

	return ret;
}

int ppg_register_sample_callback(ppg_trigger_handler_t handler)
{
	if (!ppg_initialized) {
		LOG_ERR("PPG not initialized");
		return -ENODEV;
	}

	if (handler == NULL) {
		sample_handlers[0] = NULL;
		sample_handlers[1] = NULL;
	} else {
		int ret = ppg_add_handler_slot(sample_handlers, ARRAY_SIZE(sample_handlers),
					       handler);

		if (ret != 0) {
			return ret;
		}
	}

	return ppg_register_core_triggers();
}

int ppg_unregister_sample_callback(ppg_trigger_handler_t handler)
{
	if (!ppg_initialized) {
		return -ENODEV;
	}

	return ppg_remove_handler_slot(sample_handlers, ARRAY_SIZE(sample_handlers),
				       handler);
}

int ppg_register_complete_callback(ppg_trigger_handler_t handler)
{
	if (!ppg_initialized) {
		LOG_ERR("PPG not initialized");
		return -ENODEV;
	}

	if (handler == NULL) {
		complete_handlers[0] = NULL;
		complete_handlers[1] = NULL;
	} else {
		int ret = ppg_add_handler_slot(complete_handlers,
						 ARRAY_SIZE(complete_handlers),
						 handler);

		if (ret != 0) {
			return ret;
		}
	}

	return ppg_register_core_triggers();
}

int ppg_unregister_complete_callback(ppg_trigger_handler_t handler)
{
	if (!ppg_initialized) {
		return -ENODEV;
	}

	return ppg_remove_handler_slot(complete_handlers, ARRAY_SIZE(complete_handlers),
				       handler);
}

void ppg_handle_result(const struct ppg_algo_result *result,
		       const char *source)
{
	if (result == NULL) {
		LOG_ERR("ppg_handle_result: NULL result");
		return;
	}

	/* Log completion */
	LOG_INF("%s PPG measurement completed:", source ? source : "Unknown");
	if (result->hr_valid) {
		LOG_INF("  HR: %u BPM (confidence: %u%%)", result->hr_bpm,
			result->hr_confidence);
	} else {
		LOG_WRN("  HR: invalid (last=%u BPM, confidence=%u%%)", result->hr_bpm,
			result->hr_confidence);
	}
	if (result->spo2_valid) {
		LOG_INF("  SpO2: %u%% (confidence: %u%%)", result->spo2_percent,
			result->spo2_confidence);
	} else {
		LOG_WRN("  SpO2: invalid (last=%u%%, confidence=%u%%)",
			result->spo2_percent, result->spo2_confidence);
	}
	LOG_INF("  PI: %u.%u%%, Quality: %u",
		result->perfusion_index / 10, result->perfusion_index % 10,
		result->quality);
	LOG_INF("  R Value: %u, Motion: %s",
		result->r_value_x1000,
		result->motion_detected ? "Yes" : "No");

	if (result->hr_valid || result->spo2_valid) {
		/* Play success beep pattern (non-blocking) */
		buzzer_play_success();
	} else {
		buzzer_play_warning();
	}

#if defined(CONFIG_BT)
	uint32_t ts = 0;
	struct ppg_sample_data sample_snapshot;
	int16_t accel_x = 0;
	int16_t accel_y = 0;
	int16_t accel_z = 0;
	rtc_get_unix_time(&ts);

	if (ppg_algo_get_sample_data(&sample_snapshot) == 0) {
		accel_x = sample_snapshot.accel_x;
		accel_y = sample_snapshot.accel_y;
		accel_z = sample_snapshot.accel_z;
	}

	struct ble_vitals_data vd = {
		.hr_bpm          = result->hr_bpm,
		.hr_confidence   = result->hr_confidence,
		.spo2_percent    = (uint8_t)MIN(result->spo2_percent, 100U),
		.spo2_confidence = result->spo2_confidence,
		.hb_g_dl_x10     = result->hb_g_dl_x10,
		.hb_confidence   = result->hb_confidence,
		.resp_rate_bpm   = result->resp_rate_bpm,
		.resp_confidence = result->resp_confidence,
		.r_value_x1000   = result->r_value_x1000,
		.quality         = (uint8_t)(result->quality * 25U),
		.flags           = (result->hr_valid   ? BIT(0) : 0U) |
				   (result->spo2_valid ? BIT(1) : 0U) |
				   (result->hb_valid   ? BIT(2) : 0U) |
				   (result->resp_valid ? BIT(3) : 0U) |
				   BIT(4) |
				   (result->hrv_valid  ? BIT(5) : 0U) |
				   (result->bp_valid   ? BIT(6) : 0U),
		.timestamp       = ts,
		.sdnn_ms         = result->sdnn_ms,
		.rmssd_ms        = result->rmssd_ms,
		.systolic_mmhg   = result->systolic_mmhg,
		.diastolic_mmhg  = result->diastolic_mmhg,
	};
	notify_vitals_counted(&vd);

	/* Measurement complete: push idle status */
	struct ble_meas_status ms_done = {
		.active           = 0,
		.type             = BLE_MEAS_TYPE_VITALS,
		.percent_complete = 100,
		.quality          = vd.quality,
		.samples_taken    = result->sample_count,
		.samples_target   = (active_target_samples > 0U) ?
			active_target_samples : result->sample_count,
	};
	notify_meas_status_counted(&ms_done);
	log_notify_counters("ppg_handle_result");
#endif /* CONFIG_BT */
}

void ppg_set_proximity_bypass(bool bypass)
{
	ppg_prox_bypass = bypass;
	if (bypass) {
		ppg_prox_abort_requested = false;
		ppg_prox_miss_streak = 0U;
		ppg_prox_no_contact_at_complete = false;
	}
}

bool ppg_get_proximity_bypass(void)
{
	return ppg_prox_bypass;
}

void ppg_set_proximity_disabled(bool disabled)
{
	ppg_prox_force_disabled = disabled;
	if (disabled) {
		ppg_prox_abort_requested = false;
		ppg_prox_miss_streak = 0U;
		ppg_prox_no_contact_at_complete = false;
		(void)k_work_cancel_delayable(&ppg_prox_guard_work);
	}
	LOG_INF("PPG proximity guard %s (bench/test)", disabled ? "DISABLED" : "enabled");
}

bool ppg_get_proximity_disabled(void)
{
	return ppg_prox_force_disabled;
}

int ppg_get_measurement_progress(uint16_t *taken, uint16_t *target)
{
	if (!measurement_active) {
		return -ENODATA;
	}
	if (taken != NULL) {
		*taken = last_samples_taken;
	}
	if (target != NULL) {
		*target = (last_samples_target > 0U) ? last_samples_target : active_target_samples;
	}
	return 0;
}
