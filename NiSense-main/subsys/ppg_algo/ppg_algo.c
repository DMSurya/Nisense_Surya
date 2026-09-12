/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * PPG Algorithm Subsystem - Core Orchestrator
 *
 * Manages PPG measurement lifecycle, sensor coordination, and result generation.
 * Acts as the central coordinator between PPG sensor (MAX30101 or MAX86141),
 * optional LIS2DS12 accelerometer, and the processing pipeline
 * (filtering, peak detection, quality analysis).
 */

#include "ppg_algo_priv.h"
#include "ppg_logger.h"
#include "rtc.h"
#include "led_status.h"
#include "product_hw_variant.h"
#if defined(CONFIG_APP_HEALTH_SCHED)
#include "health_sched.h"
#endif
#if defined(CONFIG_CALIBRATION)
#include <calibration.h>
#endif
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/util.h>
#include <max3010x_ppg.h>    /* Unified MAX3010x sensor attributes (supports MAX30101/02/05) */
#include <zephyr/drivers/sensor/max32664.h>  /* HUB mode custom channels (HR/SpO2/Quality) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Host-fed accelerometer feeder (hub path only). Declared locally to avoid
 * pulling the MAX32664 driver's private header into the subsystem. The
 * symbols are provided by drivers/sensor/max32664/max32664_accel_feeder.c
 * when CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER=y; otherwise we stub them. */
#if defined(CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER)
extern int max32664_accel_feeder_start(const struct device *hub);
extern int max32664_accel_feeder_stop(const struct device *hub);
#else
static inline int max32664_accel_feeder_start(const struct device *hub)
{ ARG_UNUSED(hub); return 0; }
static inline int max32664_accel_feeder_stop(const struct device *hub)
{ ARG_UNUSED(hub); return 0; }
#endif

LOG_MODULE_REGISTER(ppg_algo, CONFIG_PPG_ALGO_LOG_LEVEL);

/* Global subsystem state */
struct ppg_algo_state ppg_state = {
	.initialized = false,
	.measuring = false,
};

#ifdef CONFIG_PPG_ALGO_DEDICATED_WORKQUEUE
/* Dedicated work queue thread stack - keeps PPG I2C reads off system workqueue */
static K_THREAD_STACK_DEFINE(ppg_wq_stack, CONFIG_PPG_ALGO_WORKQUEUE_STACK_SIZE);
#endif

/* Effective sample rate: DT sample-rate-hz on Watch/Pulse PPG endpoints, else Kconfig. */
#if DT_HAS_CHOSEN(ncs_ppg_primary) && \
	DT_NODE_HAS_PROP(DT_CHOSEN(ncs_ppg_primary), sample_rate_hz)
#define PPG_SAMPLE_RATE_HZ  DT_PROP(DT_CHOSEN(ncs_ppg_primary), sample_rate_hz)
#elif DT_HAS_CHOSEN(ncs_ppg_secondary) && \
	DT_NODE_HAS_PROP(DT_CHOSEN(ncs_ppg_secondary), sample_rate_hz)
#define PPG_SAMPLE_RATE_HZ  DT_PROP(DT_CHOSEN(ncs_ppg_secondary), sample_rate_hz)
#else
#define PPG_SAMPLE_RATE_HZ  CONFIG_PPG_ALGO_SAMPLE_RATE
#endif

/* Default configuration */
static const struct ppg_algo_config default_config = {
	.sample_rate_hz = PPG_SAMPLE_RATE_HZ,
	.sample_count = PPG_SAMPLE_RATE_HZ * CONFIG_PPG_ALGO_BUFFER_SECONDS,
	.motion_rejection = IS_ENABLED(CONFIG_PPG_ALGO_MOTION_REJECTION),
	.quality_threshold = CONFIG_PPG_ALGO_QUALITY_THRESHOLD,
};

/* Forward declarations */
static void ppg_sensor_work_handler(struct k_work *work);
static void accel_sensor_work_handler(struct k_work *work);
static void process_sample(uint32_t ir, uint32_t red, uint32_t green,
			   uint32_t ambient,
			   int16_t accel_x, int16_t accel_y, int16_t accel_z);
static void finalize_measurement(void);
static void ppg_maybe_advance_staged(void);

static void accel_history_push_locked(int16_t x_mg, int16_t y_mg, int16_t z_mg,
				     uint32_t ts_ms)
{
	uint8_t idx = ppg_state.accel_hist_head;

	ppg_state.accel_hist_x_mg[idx] = x_mg;
	ppg_state.accel_hist_y_mg[idx] = y_mg;
	ppg_state.accel_hist_z_mg[idx] = z_mg;
	ppg_state.accel_hist_ts_ms[idx] = ts_ms;

	ppg_state.accel_hist_head = (uint8_t)((idx + 1U) % PPG_ACCEL_HISTORY_SIZE);
	if (ppg_state.accel_hist_count < PPG_ACCEL_HISTORY_SIZE) {
		ppg_state.accel_hist_count++;
	}
}

static bool accel_history_get_nearest(uint32_t target_ts_ms,
				      int16_t *x_mg, int16_t *y_mg, int16_t *z_mg)
{
	bool found = false;
	uint32_t best_delta = UINT32_MAX;
	int16_t best_x = 0, best_y = 0, best_z = 0;

	k_mutex_lock(&ppg_state.lock, K_FOREVER);
	if (ppg_state.accel_hist_count > 0U) {
		uint8_t oldest = (uint8_t)((ppg_state.accel_hist_head +
					   PPG_ACCEL_HISTORY_SIZE -
					   ppg_state.accel_hist_count) %
					   PPG_ACCEL_HISTORY_SIZE);

		for (uint8_t i = 0; i < ppg_state.accel_hist_count; i++) {
			uint8_t idx = (uint8_t)((oldest + i) % PPG_ACCEL_HISTORY_SIZE);
			uint32_t ts = ppg_state.accel_hist_ts_ms[idx];
			uint32_t delta = (target_ts_ms >= ts) ?
				(target_ts_ms - ts) : (ts - target_ts_ms);
			if (!found || delta < best_delta) {
				best_delta = delta;
				best_x = ppg_state.accel_hist_x_mg[idx];
				best_y = ppg_state.accel_hist_y_mg[idx];
				best_z = ppg_state.accel_hist_z_mg[idx];
				found = true;
			}
		}
	}
	k_mutex_unlock(&ppg_state.lock);

	if (found) {
		*x_mg = best_x;
		*y_mg = best_y;
		*z_mg = best_z;
	}

	return found;
}

/* Convert Zephyr sensor_value acceleration (m/s^2) to milli-g.
 * Keeps fractional precision (val2) instead of truncating to val1 only. */
static int16_t accel_sensor_value_to_mg(const struct sensor_value *v)
{
	int64_t micro_ms2 = (int64_t)v->val1 * 1000000LL + (int64_t)v->val2;
	/* 1 g = 9.80665 m/s^2 = 9,806,650 micro m/s^2 */
	return (int16_t)(micro_ms2 / 9807LL);
}

/* Refresh cached accel sample from accel_dev (LIS2DS12 path).
 * Returns 0 on success, negative errno on failure. */
static int accel_cache_refresh_once(void)
{
	struct sensor_value accel_values[3];
	int16_t accel_x_mg, accel_y_mg, accel_z_mg;
	int ret;

	if (!ppg_state.accel_dev) {
		return -ENODEV;
	}

	ret = sensor_sample_fetch(ppg_state.accel_dev);
	if (ret) {
		return ret;
	}

	ret = sensor_channel_get(ppg_state.accel_dev, SENSOR_CHAN_ACCEL_XYZ, accel_values);
	if (ret) {
		return ret;
	}

	accel_x_mg = accel_sensor_value_to_mg(&accel_values[0]);
	accel_y_mg = accel_sensor_value_to_mg(&accel_values[1]);
	accel_z_mg = accel_sensor_value_to_mg(&accel_values[2]);
	uint32_t ts_ms = k_uptime_get_32();

	k_mutex_lock(&ppg_state.lock, K_FOREVER);
	ppg_state.accel_cached_x_mg = accel_x_mg;
	ppg_state.accel_cached_y_mg = accel_y_mg;
	ppg_state.accel_cached_z_mg = accel_z_mg;
	ppg_state.accel_cached_ts_ms = ts_ms;
	ppg_state.accel_cache_valid = true;
	accel_history_push_locked(accel_x_mg, accel_y_mg, accel_z_mg, ts_ms);
	k_mutex_unlock(&ppg_state.lock);

	return 0;
}

/* ========== PPG Source Probe ========== */
/*
 * Exclusive SKU bind (not primary→secondary fallback):
 *   product_hw_detect() → WATCH or PULSE, then bind only that path.
 *   Compile-time CONFIG_MAX32664_MODE_* selects hub vs RAW/passthrough child.
 *
 * DT chosen slots still name the endpoints:
 *   ncs,ppg-primary   → Watch (max86141 / hub C)
 *   ncs,ppg-secondary → Pulse (max3010x direct)
 */

#define HAS_CHOSEN_PRIMARY   DT_HAS_CHOSEN(ncs_ppg_primary)
#define HAS_CHOSEN_SECONDARY DT_HAS_CHOSEN(ncs_ppg_secondary)

#define NODE_IS_MAX86141(n)     DT_NODE_HAS_COMPAT(n, maxim_max86141_ppg)
#define NODE_IS_MAX32664_HUB(n) DT_NODE_HAS_COMPAT(n, maxim_max32664)

/*
 * HUB-mode: max86141_ppg is not compiled; bind hub alias for Watch path.
 */
#if defined(CONFIG_MAX32664_MODE_HUB) && \
	DT_NODE_HAS_STATUS(DT_ALIAS(max32664_hub), okay)
#define WATCH_PPG_DEV              DEVICE_DT_GET_OR_NULL(DT_ALIAS(max32664_hub))
#define WATCH_PPG_IS_MAX86141      0
#define WATCH_PPG_IS_HUB           1
#elif HAS_CHOSEN_PRIMARY
#define WATCH_PPG_DEV              DEVICE_DT_GET_OR_NULL(DT_CHOSEN(ncs_ppg_primary))
#define WATCH_PPG_IS_MAX86141      NODE_IS_MAX86141(DT_CHOSEN(ncs_ppg_primary))
#define WATCH_PPG_IS_HUB           NODE_IS_MAX32664_HUB(DT_CHOSEN(ncs_ppg_primary))
#else
#define WATCH_PPG_DEV              NULL
#define WATCH_PPG_IS_MAX86141      0
#define WATCH_PPG_IS_HUB           0
#endif

#if HAS_CHOSEN_SECONDARY
#define PULSE_PPG_DEV              DEVICE_DT_GET_OR_NULL(DT_CHOSEN(ncs_ppg_secondary))
#define PULSE_PPG_IS_MAX86141      NODE_IS_MAX86141(DT_CHOSEN(ncs_ppg_secondary))
#define PULSE_PPG_IS_HUB           NODE_IS_MAX32664_HUB(DT_CHOSEN(ncs_ppg_secondary))
#else
#define PULSE_PPG_DEV              NULL
#define PULSE_PPG_IS_MAX86141      0
#define PULSE_PPG_IS_HUB           0
#endif

static enum ppg_source_type infer_source_type(bool is_max86141, bool is_hub)
{
	if (is_hub) {
		return PPG_SOURCE_MAX32664_HUB;
	}
	return is_max86141 ? PPG_SOURCE_MAX86141 : PPG_SOURCE_MAX3010X;
}

static const char *source_path_name(enum ppg_source_type t)
{
	switch (t) {
	case PPG_SOURCE_MAX32664_HUB: return "hub-direct (WHRM)";
	case PPG_SOURCE_MAX86141:
#if defined(CONFIG_MAX32664_MODE_PASSTHROUGH)
		return "hub passthrough (passthrough)";
#else
		return "hub passthrough (raw)";
#endif
	case PPG_SOURCE_MAX3010X:     return "classic MAX3010x";
	default:                      return "none";
	}
}

/**
 * @brief Bind PPG exclusively for the detected product SKU (Watch or Pulse).
 */
static const struct device *probe_ppg_source(enum ppg_source_type *out_type)
{
	enum product_hw_variant sku = product_hw_detect();
	const struct device *watch_dev = WATCH_PPG_DEV;
	const struct device *pulse_dev = PULSE_PPG_DEV;
	const struct device *sel = NULL;
	enum ppg_source_type sel_type = PPG_SOURCE_NONE;
	const char *sel_slot = NULL;

	/* Other SKU absent is expected — DBG only. */
	if (sku == PRODUCT_HW_WATCH) {
		if (pulse_dev != NULL && !device_is_ready(pulse_dev)) {
			LOG_DBG("PPG Pulse endpoint '%s' not ready (Watch SKU — ignored)",
				pulse_dev->name);
		} else if (pulse_dev != NULL && device_is_ready(pulse_dev)) {
			LOG_DBG("PPG Pulse endpoint ready but SKU is Watch — not selected");
		}

		sel = watch_dev;
		sel_type = infer_source_type(WATCH_PPG_IS_MAX86141, WATCH_PPG_IS_HUB);
		sel_slot = "watch";
	} else {
		if (watch_dev != NULL && !device_is_ready(watch_dev)) {
			LOG_DBG("PPG Watch endpoint '%s' not ready (Pulse SKU — ignored)",
				watch_dev->name);
		} else if (watch_dev != NULL && device_is_ready(watch_dev)) {
			LOG_DBG("PPG Watch endpoint ready but SKU is Pulse — not selected");
		}

		sel = pulse_dev;
		sel_type = infer_source_type(PULSE_PPG_IS_MAX86141, PULSE_PPG_IS_HUB);
		sel_slot = "pulse";

		/*
		 * Pulse with hub D in HUB mode: prefer hub when Pulse DT points
		 * at 3010x but hub is ready and not variant C (already Pulse SKU).
		 */
#if defined(CONFIG_MAX32664_MODE_HUB) && \
	DT_NODE_HAS_STATUS(DT_ALIAS(max32664_hub), okay)
		{
			const struct device *hub =
				DEVICE_DT_GET_OR_NULL(DT_ALIAS(max32664_hub));

			if (hub != NULL && device_is_ready(hub)) {
				sel = hub;
				sel_type = PPG_SOURCE_MAX32664_HUB;
				sel_slot = "pulse-hub";
			}
		}
#endif
	}

	if (sel == NULL) {
		LOG_ERR("PPG: no device for %s SKU (DT endpoint missing)",
			product_hw_variant_name(sku));
		*out_type = PPG_SOURCE_NONE;
		return NULL;
	}

	if (!device_is_ready(sel)) {
		LOG_ERR("PPG: %s SKU selected but '%s' (%s) is NOT ready",
			product_hw_variant_name(sku), sel->name, sel_slot);
		*out_type = PPG_SOURCE_NONE;
		return NULL;
	}

	*out_type = sel_type;
	LOG_INF("PPG source selected: %s (%s) [SKU=%s]",
		sel->name, source_path_name(*out_type),
		product_hw_variant_name(sku));
	return sel;
}

/* ========== PPG Sensor Power Management ========== */
/* Dispatches to the per-chip convention:
 *   - MAX30102/MAX30101: private SENSOR_ATTR_MAX3010X_SHUTDOWN attribute
 *     (val1=1 → shutdown, val1=0 → wake).
 *   - MAX86141 (via MAX32664 hub): standard SAMPLING_FREQUENCY attribute
 *     (val1=0 → shutdown, val1>0 → sampling). val1 value is just a "go"
 *     flag at this layer; actual ODR is set by DT sample-rate property. */

/**
 * @brief Put PPG sensor into shutdown mode (LEDs off, <1 uA on MAX3010x).
 */
static int ppg_sensor_shutdown(void)
{
	if (!ppg_state.ppg_dev) {
		return -ENODEV;
	}

#if defined(CONFIG_MAX32664_MODE_RAW)
	/* Stop hub output FIFO polling before AFE shutdown (RAW path). */
	if (ppg_state.source_type == PPG_SOURCE_MAX86141 && ppg_state.hub_dev != NULL) {
		struct sensor_value hub_off = { 0 };

		(void)sensor_attr_set(ppg_state.hub_dev, SENSOR_CHAN_ALL,
				      SENSOR_ATTR_SAMPLING_FREQUENCY, &hub_off);
	}
#endif

	struct sensor_value val = { .val1 = 0, .val2 = 0 };
	enum sensor_attribute attr = SENSOR_ATTR_SAMPLING_FREQUENCY;

	switch (ppg_state.source_type) {
	case PPG_SOURCE_MAX3010X:
		val.val1 = 1; /* 1 = shutdown for MAX3010x custom attr */
		attr = (enum sensor_attribute)SENSOR_ATTR_MAX3010X_SHUTDOWN;
		break;
	case PPG_SOURCE_MAX86141:
	case PPG_SOURCE_MAX32664_HUB:
		/* Both the max86141_ppg child (RAW mode) and the max32664_hub
		 * itself (HUB mode) accept SAMPLING_FREQUENCY val=0 as "stop".
		 * On the hub device this triggers PAUSE (output mode 0x00). */
		val.val1 = 0;
		break;
	default:
		return -ENODEV;
	}

	return sensor_attr_set(ppg_state.ppg_dev, SENSOR_CHAN_ALL, attr, &val);
}

/**
 * @brief Wake PPG sensor from shutdown (LEDs resume, FIFO flushed).
 */
static int ppg_sensor_wakeup(void)
{
	if (!ppg_state.ppg_dev) {
		return -ENODEV;
	}
	struct sensor_value val = { .val1 = 0, .val2 = 0 };
	enum sensor_attribute attr = SENSOR_ATTR_SAMPLING_FREQUENCY;

	switch (ppg_state.source_type) {
	case PPG_SOURCE_MAX3010X:
		val.val1 = 0; /* 0 = wake from shutdown */
		attr = (enum sensor_attribute)SENSOR_ATTR_MAX3010X_SHUTDOWN;
		break;
	case PPG_SOURCE_MAX86141:
	case PPG_SOURCE_MAX32664_HUB:
		/* Any non-zero val1 starts sampling. On the hub this switches
		 * output mode to 0x02 (algorithm data) and launches poll_work.
		 * The rate field is informational for now; the hub runs at the
		 * variant-specific fixed rate (25 Hz for WHRM+WSpO2 v3x). */
		val.val1 = ppg_state.config.sample_rate_hz ?
			   ppg_state.config.sample_rate_hz : 25;
		break;
	default:
		return -ENODEV;
	}

	int err = sensor_attr_set(ppg_state.ppg_dev, SENSOR_CHAN_ALL, attr, &val);

#if defined(CONFIG_MAX32664_MODE_RAW)
	/* Hub must poll status / output FIFO so Family 0x12 fills (GWEC 30.x). */
	if (err == 0 && ppg_state.source_type == PPG_SOURCE_MAX86141 &&
	    ppg_state.hub_dev != NULL) {
		struct sensor_value hub_on = { .val1 = 1, .val2 = 0 };

		err = sensor_attr_set(ppg_state.hub_dev, SENSOR_CHAN_ALL,
				      SENSOR_ATTR_SAMPLING_FREQUENCY, &hub_on);
		if (err != 0) {
			LOG_WRN("RAW hub sampling start failed: %d", err);
		}
	}
#endif

	return err;
}

/* Helper: schedule work on dedicated queue if available, else system queue */
#ifdef CONFIG_PPG_ALGO_DEDICATED_WORKQUEUE
#define ppg_work_reschedule(dwork, delay) \
	k_work_reschedule_for_queue(&ppg_state.ppg_work_q, (dwork), (delay))
#else
#define ppg_work_reschedule(dwork, delay) \
	k_work_reschedule((dwork), (delay))
#endif

/* ========== Public API ========== */

int ppg_algo_init(void)
{
	if (ppg_state.initialized) {
		LOG_WRN("Already initialized");
		return 0;
	}

	/* Initialize mutex */
	k_mutex_init(&ppg_state.lock);

	/*
	 * Late binding: don't hard-fail init if no PPG is ready yet.
	 * The app may request measurements later; at that point we probe and
	 * select whatever PPG source is present/ready.
	 */
	ppg_state.ppg_dev = NULL;
	ppg_state.source_type = PPG_SOURCE_NONE;

	/* Resolve the MAX32664 hub handle. It anchors the host-fed accelerometer
	 * feeder (I2C passthrough to LIS2DS12, then I2C write of accel samples
	 * back to the hub's input FIFO for motion rejection).
	 *
	 *   - PPG_SOURCE_MAX32664_HUB: the hub *is* the PPG device.
	 *                              hub_dev == ppg_dev.
	 *   - PPG_SOURCE_MAX86141:     hub is the parent of ppg_dev (raw/passthrough).
	 *                              Resolved via DT alias.
	 *   - PPG_SOURCE_MAX3010X:     hub may be present (firmware update builds)
	 *                              but not involved in PPG; leave NULL so the
	 *                              feeder stays dormant.
	 */
	ppg_state.hub_dev = NULL;

	/* Get LIS2DS12 accelerometer device (optional — motion rejection only).
	 *
	 * The accel-sensor alias points at the lis2ds12 child of the hub. In HUB
	 * mode the LIS2DS12 is driven by max32664_accel_feeder.c via I2C
	 * passthrough (NOT as a Zephyr sensor device), so the lis2ds12_accel
	 * driver is intentionally not compiled (see drivers/sensor/max32664/
	 * CMakeLists.txt). Taking DEVICE_DT_GET_OR_NULL() on the alias would
	 * then leave an undefined __device_dts_ord reference at link time.
	 *
	 * Only resolve the alias as a real sensor device when we're in a mode
	 * that actually compiles the driver (RAW / PASSTHROUGH) or when the
	 * MAX32664 driver is disabled entirely (classic MAX3010x-only builds,
	 * where the alias would just point at an accel on the host bus).
	 */
#if DT_NODE_HAS_STATUS(DT_ALIAS(accel_sensor), okay) && \
	(defined(CONFIG_MAX32664_MODE_RAW) || \
	 defined(CONFIG_MAX32664_MODE_PASSTHROUGH) || \
	 !defined(CONFIG_MAX32664))
	ppg_state.accel_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(accel_sensor));
	if (!ppg_state.accel_dev || !device_is_ready(ppg_state.accel_dev)) {
		LOG_WRN("Accel sensor (accel-sensor alias) not ready - motion rejection disabled");
		ppg_state.accel_dev = NULL;
	}
#else
	/* HUB mode: accel is accessed via hub passthrough, not the Zephyr
	 * sensor API. Leave accel_dev NULL so motion-rejection paths fall back
	 * to the hub-side feeder. */
	ppg_state.accel_dev = NULL;
#endif

	/* Initialize default configuration */
	memcpy(&ppg_state.config, &default_config, sizeof(struct ppg_algo_config));

	/* Initialize work items */
	k_work_init_delayable(&ppg_state.ppg_work, ppg_sensor_work_handler);
	k_work_init_delayable(&ppg_state.accel_work, accel_sensor_work_handler);

#ifdef CONFIG_PPG_ALGO_DEDICATED_WORKQUEUE
	/* Start dedicated work queue for PPG sensor reads.
	 * This keeps blocking I2C passthrough reads (~100-200ms per FIFO drain)
	 * off the system work queue, preventing UI/BLE stalls. */
	k_work_queue_init(&ppg_state.ppg_work_q);
	k_work_queue_start(&ppg_state.ppg_work_q, ppg_wq_stack,
			   K_THREAD_STACK_SIZEOF(ppg_wq_stack),
			   CONFIG_PPG_ALGO_WORKQUEUE_PRIORITY, NULL);
	k_thread_name_set(&ppg_state.ppg_work_q.thread, "ppg_wq");
	LOG_INF("  Work queue: dedicated (stack=%u, prio=%d)",
		CONFIG_PPG_ALGO_WORKQUEUE_STACK_SIZE,
		CONFIG_PPG_ALGO_WORKQUEUE_PRIORITY);
#else
	LOG_INF("  Work queue: system (shared)");
#endif

	/* Initialize processing components */
	ppg_preproc_init(&ppg_state.preproc);
	ppg_peak_detect_init(&ppg_state.peaks);

	ppg_state.initialized = true;

	LOG_INF("PPG Algorithm Subsystem v%s initialized", PPG_ALGO_VERSION_STRING);
	LOG_INF("  Source: (deferred until measurement start)");
	LOG_INF("  Sample rate: %u Hz", ppg_state.config.sample_rate_hz);
	LOG_INF("  Buffer size: %u samples", ppg_state.config.sample_count);
	LOG_INF("  Motion rejection: %s",
		(ppg_state.accel_dev || ppg_state.hub_dev) ? "enabled" : "disabled");
	LOG_INF("  Hub feeder: %s",
#if defined(CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER)
		ppg_state.hub_dev ? "available (starts on measurement)" : "n/a");
#else
		ppg_state.hub_dev ? "not built (CONFIG_MAX32664_EXTERNAL_ACCEL_FEEDER off)" :
				    "n/a");
#endif

	return 0;
}

int ppg_algo_start_measurement(const struct ppg_algo_config *config)
{
	int ret;

	if (!ppg_state.initialized) {
		LOG_ERR("Not initialized");
		return -ENODEV;
	}

	/* Late binding: probe/select a PPG source at measurement start. */
	if (ppg_state.ppg_dev == NULL || !device_is_ready(ppg_state.ppg_dev)) {
		ppg_state.ppg_dev = probe_ppg_source(&ppg_state.source_type);
		if (!ppg_state.ppg_dev) {
			LOG_ERR("No PPG source ready at measurement start");
			return -ENODEV;
		}

		/* Resolve hub_dev based on selected source. */
		if (ppg_state.source_type == PPG_SOURCE_MAX32664_HUB) {
			ppg_state.hub_dev = ppg_state.ppg_dev;
		} else if (ppg_state.source_type == PPG_SOURCE_MAX86141) {
#if DT_NODE_HAS_STATUS(DT_ALIAS(max32664_hub), okay)
			ppg_state.hub_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(max32664_hub));
			if (ppg_state.hub_dev && !device_is_ready(ppg_state.hub_dev)) {
				LOG_WRN("MAX32664 hub not ready; accel feeder will be idle");
				ppg_state.hub_dev = NULL;
			}
#else
			ppg_state.hub_dev = NULL;
#endif
		} else {
			ppg_state.hub_dev = NULL;
		}

		LOG_INF("PPG source bound at start: %s (%s)",
			ppg_state.ppg_dev->name, source_path_name(ppg_state.source_type));
	}

	k_mutex_lock(&ppg_state.lock, K_FOREVER);

	if (ppg_state.measuring) {
		k_mutex_unlock(&ppg_state.lock);
		LOG_WRN("Measurement already in progress");
		return -EBUSY;
	}

	/* Apply configuration */
	if (config) {
		memcpy(&ppg_state.config, config, sizeof(struct ppg_algo_config));
	}

	/* Validate configuration */
	if (ppg_state.config.sample_count > PPG_MAX_SAMPLES) {
		k_mutex_unlock(&ppg_state.lock);
		LOG_ERR("Sample count %u exceeds maximum %u",
			ppg_state.config.sample_count, PPG_MAX_SAMPLES);
		return -EINVAL;
	}

	/* HUB mode: one "sample" == one algorithm frame, and the hub emits
	 * those at ~1 Hz (WHRM+WSpO2 v3x). Without a clamp a typical raw-path
	 * sample_count (e.g. 600 @ 50 Hz = 12 s) would translate to 10 min
	 * of hub waiting and be mistaken for a hang. Clamp to a sensible
	 * window; callers wanting finer control can set sample_count
	 * explicitly in the config they pass to start_measurement(). */
#define PPG_ALGO_HUB_MAX_FRAMES 30
	if (ppg_state.source_type == PPG_SOURCE_MAX32664_HUB &&
	    ppg_state.config.sample_count > PPG_ALGO_HUB_MAX_FRAMES) {
		LOG_INF("HUB mode: clamping sample_count %u -> %u frames (~%u s @ 1 Hz)",
			ppg_state.config.sample_count,
			PPG_ALGO_HUB_MAX_FRAMES, PPG_ALGO_HUB_MAX_FRAMES);
		ppg_state.config.sample_count = PPG_ALGO_HUB_MAX_FRAMES;
	}

	ppg_state.record_target = ppg_state.config.sample_count;
	ppg_state.session_target = ppg_state.record_target;
#if defined(CONFIG_PPG_ALGO_STAGED_LIVE)
	{
		uint16_t tail = (uint16_t)CONFIG_PPG_ALGO_LIVE_TAIL_SAMPLES;

		if (ppg_state.source_type == PPG_SOURCE_MAX32664_HUB) {
			/* Hub ~1 Hz: keep tail short so session stays interactive. */
			tail = MIN(tail, 10U);
		}
		if ((uint32_t)ppg_state.record_target + (uint32_t)tail >
		    (uint32_t)PPG_MAX_SAMPLES) {
			tail = (uint16_t)(PPG_MAX_SAMPLES - ppg_state.record_target);
		}
		ppg_state.session_target =
			(uint16_t)(ppg_state.record_target + tail);
	}
#endif
	ppg_state.partial_vitals_fired = false;
	ppg_state.partial_vascular_fired = false;
	ppg_state.record_ready_fired = false;

	/* Allocate sample buffer for the authoritative record window only.
	 * Live-tail samples continue peak/preproc processing without growing
	 * this buffer (700× multi-channel arrays OOMs nRF52840 with DC/AC log). */
	bool need_accel = (ppg_state.accel_dev != NULL);
	ret = ppg_samples_init(&ppg_state.buffer, ppg_state.record_target, need_accel);
	if (ret) {
		k_mutex_unlock(&ppg_state.lock);
		LOG_ERR("Failed to initialize sample buffer: %d", ret);
		return ret;
	}
	LOG_INF("PPG buffer capacity=%u (session target=%u, live tail in RAM-free path)",
		ppg_state.record_target, ppg_state.session_target);

	/* Reset processing state */
	ppg_preproc_init(&ppg_state.preproc);
	ppg_peak_detect_init(&ppg_state.peaks);
	ppg_state.samples_processed = 0;
	ppg_state.no_contact_frames = 0;
	ppg_state.result_ready = false;
	ppg_state.sample_ready = false;
	ppg_state.accel_cache_valid = false;
	ppg_state.accel_cached_x_mg = 0;
	ppg_state.accel_cached_y_mg = 0;
	ppg_state.accel_cached_z_mg = 0;
	ppg_state.accel_cached_ts_ms = 0;
	ppg_state.accel_hist_head = 0;
	ppg_state.accel_hist_count = 0;
	ppg_state.green_channel_available = false;
	ppg_state.start_time_ms = k_uptime_get_32();

#if defined(CONFIG_MAX32664_MODE_RAW)
	/* Seed LED PA from factory cal once per session (host AGC adjusts after). */
	if (ppg_state.source_type == PPG_SOURCE_MAX86141) {
		max32664_raw_seed_led_pa_from_cal();
	}
#endif

	/* Mark as measuring */
	ppg_state.measuring = true;
	ppg_state.finalizing = false;

	k_mutex_unlock(&ppg_state.lock);

	/* Wake PPG sensor from shutdown (LEDs on, FIFO flushed) */
	ret = ppg_sensor_wakeup();
	if (ret) {
		LOG_ERR("Failed to wake PPG sensor: %d", ret);
		ppg_state.measuring = false;
		ppg_samples_free(&ppg_state.buffer);
		return ret;
	}
	if (ppg_state.source_type == PPG_SOURCE_MAX32664_HUB) {
		LOG_INF("PPG hub sampling started (WHRM drives LEDs; wrist path "
			"often emphasizes green; red/IR may be dim or multiplexed)");
	} else {
		LOG_INF("PPG sensor woken from shutdown (LEDs on)");
	}

	/* Kick the host-fed accelerometer feeder after the AFE is live so the
	 * first 25 Hz feed lands while the hub-side algo is ready to consume
	 * it. No-op on the classic MAX30102 path (hub_dev == NULL) and on the
	 * hub path when the DT accel-source is not "external" (feeder's own
	 * guard short-circuits). */
	if (ppg_state.hub_dev != NULL) {
		(void)max32664_accel_feeder_start(ppg_state.hub_dev);
	}

	/* Start sensor sampling work handlers */
	if (need_accel) {
		int acc_ret = accel_cache_refresh_once();
		if (acc_ret == 0) {
			LOG_DBG("Accel cache primed before first PPG sample");
		}
	}

	ppg_work_reschedule(&ppg_state.ppg_work, K_MSEC(0)); /* Start immediately */

	if (need_accel) {
		ppg_work_reschedule(&ppg_state.accel_work, K_MSEC(0));
	}

	LOG_INF("Measurement started: type=VITALS, rate=%uHz, record=%u session=%u",
		ppg_state.config.sample_rate_hz,
		ppg_state.record_target, ppg_state.session_target);

	/* Visible UI-level indicator (board RGB) that measurement is active,
	 * even when MAX86141 optical pulses are hard to perceive by eye. */
	led_status_set(LED_STATUS_MEASURING);

	return 0;
}

int ppg_algo_stop_measurement(void)
{
	int ret;

	if (!ppg_state.initialized) {
		return -ENODEV;
	}

	k_mutex_lock(&ppg_state.lock, K_FOREVER);

	if (!ppg_state.measuring) {
		k_mutex_unlock(&ppg_state.lock);
		return -EALREADY;
	}

	/* Cancel work items */
	k_work_cancel_delayable(&ppg_state.ppg_work);
	k_work_cancel_delayable(&ppg_state.accel_work);

	/* Stop the feeder before the AFE so no 0x14/0x04 writes race the hub
	 * shutdown on the MAX86141 path. Safe no-op on the classic path. */
	if (ppg_state.hub_dev != NULL) {
		(void)max32664_accel_feeder_stop(ppg_state.hub_dev);
	}

	/* Enter shutdown mode (LEDs off) */
	ret = ppg_sensor_shutdown();
	if (ret) {
		LOG_WRN("Failed to shutdown PPG sensor: %d (continuing)", ret);
	} else {
		LOG_INF("PPG sensor shutdown (LEDs off)");
	}

	/* Free buffer */
	ppg_samples_free(&ppg_state.buffer);

	ppg_state.measuring = false;
	ppg_state.finalizing = false;

	k_mutex_unlock(&ppg_state.lock);

	LOG_INF("Measurement stopped");
	led_status_clear(LED_STATUS_MEASURING);

	return 0;
}

bool ppg_algo_is_measuring(void)
{
	return ppg_state.measuring;
}

bool ppg_algo_is_finalizing(void)
{
	return ppg_state.finalizing;
}

uint16_t ppg_algo_get_measurement_target_samples(void)
{
	uint16_t n = 0;

	if (!ppg_state.initialized) {
		return 0;
	}

	k_mutex_lock(&ppg_state.lock, K_FOREVER);
	if (ppg_state.measuring) {
		n = ppg_state.session_target > 0U ? ppg_state.session_target :
						    ppg_state.config.sample_count;
	}
	k_mutex_unlock(&ppg_state.lock);
	return n;
}

int ppg_algo_trigger_set(enum ppg_trigger_type type,
			 ppg_trigger_handler_t handler,
			 void *user_data)
{
	if (type >= ARRAY_SIZE(ppg_state.triggers)) {
		return -EINVAL;
	}

	k_mutex_lock(&ppg_state.lock, K_FOREVER);

	ppg_state.triggers[type].handler = handler;
	ppg_state.triggers[type].user_data = user_data;
	ppg_state.triggers[type].enabled = (handler != NULL);

	k_mutex_unlock(&ppg_state.lock);

	return 0;
}

int ppg_algo_get_result(struct ppg_algo_result *result)
{
	if (!result) {
		return -EINVAL;
	}

	k_mutex_lock(&ppg_state.lock, K_FOREVER);

	if (!ppg_state.result_ready) {
		k_mutex_unlock(&ppg_state.lock);
		return -ENODATA;
	}

	memcpy(result, &ppg_state.latest_result, sizeof(struct ppg_algo_result));

	k_mutex_unlock(&ppg_state.lock);

	return 0;
}

int ppg_algo_get_sample_data(struct ppg_sample_data *sample)
{
	if (!sample) {
		return -EINVAL;
	}

	k_mutex_lock(&ppg_state.lock, K_FOREVER);

	if (!ppg_state.sample_ready) {
		k_mutex_unlock(&ppg_state.lock);
		return -ENODATA;
	}

	memcpy(sample, &ppg_state.latest_sample, sizeof(struct ppg_sample_data));

	k_mutex_unlock(&ppg_state.lock);

	return 0;
}

void ppg_algo_reset(void)
{
	k_mutex_lock(&ppg_state.lock, K_FOREVER);

	ppg_preproc_init(&ppg_state.preproc);
	ppg_peak_detect_init(&ppg_state.peaks);
	ppg_state.samples_processed = 0;
	ppg_state.no_contact_frames = 0;
	ppg_state.result_ready = false;
	ppg_state.sample_ready = false;

	k_mutex_unlock(&ppg_state.lock);

	LOG_INF("Algorithm state reset");
}

const char *ppg_algo_version(void)
{
	return PPG_ALGO_VERSION_STRING;
}

const char *ppg_algo_get_source_name(void)
{
	if (!ppg_state.initialized || ppg_state.ppg_dev == NULL) {
		return "none";
	}
	return ppg_state.ppg_dev->name;
}

uint32_t ppg_algo_get_capabilities(void)
{
	if (!ppg_state.initialized) {
		return 0;
	}

	/* Both chips cover HR + SpO2 via red/IR LEDs. The differences the UI
	 * cares about today are (a) green-LED-assisted HR robustness and
	 * (b) motion rejection via the accel feeder. Keep the map here so
	 * any future source (e.g. MAX30102 variant without green) can declare
	 * its own capability bits in one place. */
	switch (ppg_state.source_type) {
	case PPG_SOURCE_MAX32664_HUB:
		/* HUB mode: WHRM+WSpO2 runs inside the hub firmware. HR/SpO2
		 * are first-class; motion rejection is always available because
		 * the accel feeder IS the reason the host is involved. Raw
		 * green-LED signal is NOT exposed up to ppg_algo (only the
		 * algorithm outputs are), so GREEN_LED capability is off. */
		return PPG_CAP_HR | PPG_CAP_SPO2 | PPG_CAP_MOTION_REJECTION;
	case PPG_SOURCE_MAX86141:
		return PPG_CAP_HR | PPG_CAP_SPO2 | PPG_CAP_GREEN_LED |
		       PPG_CAP_HEMOGLOBIN | PPG_CAP_RESPIRATION |
		       (ppg_state.hub_dev ? PPG_CAP_MOTION_REJECTION : 0);
	case PPG_SOURCE_MAX3010X:
		/* MAX30102 is IR+Red only; MAX30101 adds green but we don't
		 * distinguish here yet. Motion rejection is off on this path
		 * (no accel wired in the usual rev). */
		return PPG_CAP_HR | PPG_CAP_SPO2;
	default:
		return 0;
	}
}

/* ========== Internal Helpers ========== */

/* ----------------------------------------------------------------------------
 * Hub-direct sample handler (MAX32664 in HUB mode)
 *
 * In HUB mode the MAX32664 runs WHRM+WSpO2 on-chip. Each sensor_sample_fetch()
 * call blocks on the hub's sample_sem until the next algorithm update is
 * produced (typically ~1 Hz for v3x firmware). We read the algorithm outputs
 * via custom channels and stash them directly into ppg_state.latest_result,
 * bypassing the nRF's peak-detect / SpO2 math entirely.
 *
 * samples_processed counts hub updates, not raw PPG samples. A measurement
 * ends after config.sample_count hub updates or on explicit stop.
 * --------------------------------------------------------------------------*/
static void process_hub_sample(void)
{
	const struct device *dev = ppg_state.ppg_dev;
	struct sensor_value hr_v, spo2_v, hr_conf_v, spo2_conf_v, qual_v;
	struct sensor_value ir_v, red_v, green_v;
	struct sensor_value ax_v, ay_v, az_v;
	uint32_t ir_u32 = 0U, red_u32 = 0U, green_u32 = 0U;
	int16_t accel_x = 0, accel_y = 0, accel_z = 0;
	bool hub_accel_available = false;
	const char *accel_source = "hub";
	int ret;

	ret = sensor_channel_get(dev, SENSOR_CHAN_MAX32664_HR, &hr_v);
	if (ret) {
		LOG_WRN("hub HR channel_get failed: %d", ret);
		return;
	}
	ret = sensor_channel_get(dev, SENSOR_CHAN_MAX32664_SPO2, &spo2_v);
	if (ret) {
		LOG_WRN("hub SpO2 channel_get failed: %d", ret);
		/* non-fatal — some firmwares only emit HR until finger is stable */
	}
	if (sensor_channel_get(dev, SENSOR_CHAN_MAX32664_HR_CONFIDENCE, &hr_conf_v)) {
		hr_conf_v.val1 = 0;
	}
	if (sensor_channel_get(dev, SENSOR_CHAN_MAX32664_SPO2_CONFIDENCE, &spo2_conf_v)) {
		spo2_conf_v.val1 = 0;
	}
	if (sensor_channel_get(dev, SENSOR_CHAN_MAX32664_QUALITY, &qual_v)) {
		qual_v.val1 = 0;
	}

	/* Skin Contact Detection: abort early if the hub reports no contact
	 * for CONFIG_PPG_ALGO_NO_CONTACT_ABORT_FRAMES consecutive frames.
	 * scd_state == 0 means no contact; any non-zero value means contact. */
#if defined(CONFIG_PPG_ALGO_NO_CONTACT_ABORT_FRAMES)
	struct sensor_value scd_v = { .val1 = 0 };
	(void)sensor_channel_get(dev, SENSOR_CHAN_MAX32664_SCD_STATE, &scd_v);
	if (scd_v.val1 == 0) {
		ppg_state.no_contact_frames++;
		if (ppg_state.no_contact_frames >= CONFIG_PPG_ALGO_NO_CONTACT_ABORT_FRAMES) {
			LOG_WRN("No skin contact for %u frames — aborting measurement",
				ppg_state.no_contact_frames);
			ppg_state.no_contact_frames = 0;
			finalize_measurement();
			return;
		}
	} else {
		if (ppg_state.no_contact_frames > 0) {
			LOG_DBG("Skin contact restored (scd_state=%d, was no-contact for %u frames)",
				scd_v.val1, ppg_state.no_contact_frames);
		}
		ppg_state.no_contact_frames = 0;
	}
#endif /* CONFIG_PPG_ALGO_NO_CONTACT_ABORT_FRAMES */

	/* Raw PPG + accel for CSV (same paths as max32664_channel_get_hub). */
	if (sensor_channel_get(dev, SENSOR_CHAN_IR, &ir_v) == 0) {
		ir_u32 = (uint32_t)MAX(ir_v.val1, 0);
	}
	if (sensor_channel_get(dev, SENSOR_CHAN_RED, &red_v) == 0) {
		red_u32 = (uint32_t)MAX(red_v.val1, 0);
	}
	if (sensor_channel_get(dev, SENSOR_CHAN_GREEN, &green_v) == 0) {
		green_u32 = (uint32_t)MAX(green_v.val1, 0);
	}
	if (sensor_channel_get(dev, SENSOR_CHAN_ACCEL_X, &ax_v) == 0) {
		accel_x = (int16_t)ax_v.val1;
		hub_accel_available = true;
	}
	if (sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Y, &ay_v) == 0) {
		accel_y = (int16_t)ay_v.val1;
		hub_accel_available = true;
	}
	if (sensor_channel_get(dev, SENSOR_CHAN_ACCEL_Z, &az_v) == 0) {
		accel_z = (int16_t)az_v.val1;
		hub_accel_available = true;
	}

	/* In HUB algo-only mode, accel channels may be absent/zero even though
	 * an external feeder is active. Fall back to direct accel_dev sampling so
	 * CSV accel columns still reflect wrist motion. */
	if ((!hub_accel_available || (accel_x == 0 && accel_y == 0 && accel_z == 0)) &&
	    ppg_state.accel_dev && device_is_ready(ppg_state.accel_dev)) {
		struct sensor_value accel_xyz[3];
		accel_source = "accel_dev";
		if (sensor_sample_fetch(ppg_state.accel_dev) == 0 &&
		    sensor_channel_get(ppg_state.accel_dev, SENSOR_CHAN_ACCEL_XYZ,
				     accel_xyz) == 0) {
			accel_x = accel_sensor_value_to_mg(&accel_xyz[0]);
			accel_y = accel_sensor_value_to_mg(&accel_xyz[1]);
			accel_z = accel_sensor_value_to_mg(&accel_xyz[2]);
		} else {
			accel_source = "accel_dev_fail";
		}
	}

	/* HR channel is reported as BPM with val1=integer, val2=tenths×100000.
	 * Re-assemble to a running integer BPM. SpO2 is reported as percent. */
	uint16_t hr_bpm   = (uint16_t)hr_v.val1;
	uint16_t spo2_pct = (uint16_t)spo2_v.val1;
	uint8_t  hr_conf  = (uint8_t)MIN(hr_conf_v.val1, 100);
	uint8_t  spo2_conf = (uint8_t)MIN(spo2_conf_v.val1, 100);
	uint8_t  qual     = (uint8_t)MIN(qual_v.val1, 100);

	k_mutex_lock(&ppg_state.lock, K_FOREVER);

	/* Keep the "last good" sample and a running best for the result.
	 * We update the result struct continuously so any caller reading
	 * ppg_algo_get_result() during the window sees the latest hub values. */
	struct ppg_algo_result *r = &ppg_state.latest_result;
	r->hr_bpm           = hr_bpm;
	r->hr_confidence    = hr_conf;
	r->hr_valid         = (hr_bpm > 0 &&
	                       hr_conf >= ppg_state.config.quality_threshold);
	r->spo2_percent     = spo2_pct;
	r->spo2_confidence  = spo2_conf;
	r->spo2_valid       = (spo2_pct > 0 &&
	                       spo2_conf >= ppg_state.config.quality_threshold);
	r->quality          = (enum ppg_signal_quality)qual;
	r->sample_count     = ppg_state.samples_processed;
	r->timestamp_ms     = k_uptime_get_32();

	ret = ppg_samples_add(&ppg_state.buffer, ir_u32, red_u32, green_u32,
			      0U, 0,
#if defined(CONFIG_PPG_LOG_DC_AC)
			      0, 0, 0, 0, 0,
#endif
			      accel_x, accel_y, accel_z);
	if (ret == -ENOMEM) {
		/* Record window full — continue hub live/session counting. */
		ret = 0;
	} else if (ret) {
		LOG_WRN("hub: ppg_samples_add failed: %d (sample rejected)", ret);
		k_mutex_unlock(&ppg_state.lock);
		return;
	}

	/* Per-sample snapshot for SAMPLE_READY subscribers (UI). */
	ppg_state.latest_sample.sample_number = ppg_state.samples_processed;
	ppg_state.latest_sample.total_samples = ppg_state.session_target > 0U ?
						ppg_state.session_target :
						ppg_state.config.sample_count;
	ppg_state.latest_sample.timestamp_ms  = r->timestamp_ms;
	ppg_state.latest_sample.raw_ir = ir_u32;
	ppg_state.latest_sample.raw_red = red_u32;
	ppg_state.latest_sample.raw_green = green_u32;
	ppg_state.latest_sample.accel_x = accel_x;
	ppg_state.latest_sample.accel_y = accel_y;
	ppg_state.latest_sample.accel_z = accel_z;
	ppg_state.sample_ready = true;
	ppg_state.samples_processed++;
	r->sample_count = ppg_state.samples_processed;

	k_mutex_unlock(&ppg_state.lock);

	LOG_INF("[hub %u/%u] HR=%u bpm (conf %u%%) SpO2=%u%% (conf %u%%) Q=%u ACC=%s (%d,%d,%d)",
		ppg_state.samples_processed, ppg_state.session_target,
		hr_bpm, hr_conf, spo2_pct, spo2_conf, qual,
		accel_source, accel_x, accel_y, accel_z);

	ppg_trigger_fire(PPG_TRIG_SAMPLE_READY);

	ppg_maybe_advance_staged();
}

static void ppg_sensor_work_handler(struct k_work *work)
{
	struct sensor_value val_ir, val_red, val_green, val_ambient;
	int ret;

	if (!ppg_state.measuring) {
		return;
	}

	if (ppg_state.source_type == PPG_SOURCE_MAX32664_HUB) {
		/* Single blocking fetch per tick — the hub only emits a new
		 * algorithm frame ~1x/sec, so looping would just spin-wait. */
		ret = sensor_sample_fetch(ppg_state.ppg_dev);
		if (ret == -ETIMEDOUT) {
			/* Expected during finger-on warm-up / hub boot. */
			LOG_DBG("hub fetch timeout (no new algorithm frame)");
		} else if (ret == -ECANCELED) {
			/* Hub paused — caller will reschedule or stop. */
		} else if (ret) {
			LOG_WRN("hub sample_fetch failed: %d", ret);
		} else {
			process_hub_sample();
		}

		if (ppg_state.measuring) {
			/* Re-poll at 250 ms. Hub algorithm frames arrive ~1 Hz
			 * so most ticks will time out cheaply inside the hub's
			 * sem_take; this keeps the thread responsive to stop. */
			ppg_work_reschedule(&ppg_state.ppg_work, K_MSEC(250));
		}
		return;
	}

	/* ── Raw sample path (MAX3010x classic, or MAX86141 via hub raw mode) ── */

	/* Drain all available FIFO samples from the custom MAX30101 driver.
	 * The driver caches a full FIFO drain internally; each sample_fetch()
	 * returns the next queued sample.  When no more data is available
	 * it returns -EAGAIN, signalling us to reschedule for the next poll. */
	while (ppg_state.measuring) {
		uint32_t ppg_sample_ts_ms = k_uptime_get_32();

		ret = sensor_sample_fetch(ppg_state.ppg_dev);
		if (ret == -EAGAIN) {
			break;  /* FIFO empty — wait for more data */
		}
		if (ret == -ECANCELED) {
			break;  /* Sensor in shutdown */
		}
		if (ret) {
			LOG_ERR("Failed to fetch PPG sample: %d", ret);
			break;
		}

		/* Get IR channel */
		ret = sensor_channel_get(ppg_state.ppg_dev, SENSOR_CHAN_IR, &val_ir);
		if (ret) {
			LOG_ERR("Failed to get IR channel: %d", ret);
			break;
		}

		/* Get Red channel */
		ret = sensor_channel_get(ppg_state.ppg_dev, SENSOR_CHAN_RED, &val_red);
		if (ret) {
			LOG_ERR("Failed to get Red channel: %d", ret);
			break;
		}

		/* Green — always sampled; MAX86141 RAW requires it for HR + Hb. */
		val_green.val1 = 0;
		ret = sensor_channel_get(ppg_state.ppg_dev, SENSOR_CHAN_GREEN, &val_green);
		if (ret == 0 && max32664_hub_adc_plausible((uint32_t)val_green.val1)) {
			ppg_state.green_channel_available = true;
		} else if (ret != 0 && ppg_state.buffer.count < 5) {
			LOG_DBG("Green channel_get: %d (logging 0)", ret);
		}

		/* Ambient slot (MAX86141 hub FIFO slot4 / SENSOR_CHAN_LIGHT). */
		val_ambient.val1 = 0;
		(void)sensor_channel_get(ppg_state.ppg_dev, SENSOR_CHAN_LIGHT, &val_ambient);

		/* Process IR, Red, Green, ambient */
		int16_t accel_x = 0;
		int16_t accel_y = 0;
		int16_t accel_z = 0;

		/* Capture accelerometer data for sample packing/logging.
		 * Use cached accel populated by accel_work to keep this RAW hot path
		 * non-blocking at 25 Hz. accel_work owns sensor_sample_fetch().
		 *
		 * Priority:
		 *  1) cached accel_dev sample (LIS2DS12)
		 *  2) hub_dev channels (fallback when accel is only visible via hub)
		 */
		if (ppg_state.accel_dev && device_is_ready(ppg_state.accel_dev)) {
			(void)accel_history_get_nearest(ppg_sample_ts_ms,
						       &accel_x, &accel_y, &accel_z);
		} else if (ppg_state.hub_dev && device_is_ready(ppg_state.hub_dev)) {
			struct sensor_value ax, ay, az;

			if (sensor_channel_get(ppg_state.hub_dev, SENSOR_CHAN_ACCEL_X, &ax) == 0 &&
			    sensor_channel_get(ppg_state.hub_dev, SENSOR_CHAN_ACCEL_Y, &ay) == 0 &&
			    sensor_channel_get(ppg_state.hub_dev, SENSOR_CHAN_ACCEL_Z, &az) == 0) {
				accel_x = (int16_t)ax.val1;
				accel_y = (int16_t)ay.val1;
				accel_z = (int16_t)az.val1;
			}
		}

		process_sample((uint32_t)val_ir.val1, (uint32_t)val_red.val1,
			       (uint32_t)val_green.val1, (uint32_t)val_ambient.val1,
			       accel_x, accel_y, accel_z);
	}

	/* Reschedule if still measuring */
	if (ppg_state.measuring) {
		/* Poll at ~1x the sensor output rate.
		 * Custom driver drains entire FIFO per call, so this timer
		 * determines how often we check for new data.
		 * 50 Hz → 20ms between checks. */
		uint32_t poll_ms = 1000 / ppg_state.config.sample_rate_hz;
		if (poll_ms < 5) {
			poll_ms = 5; /* Floor at 5ms to avoid starving other threads */
		}
		ppg_work_reschedule(&ppg_state.ppg_work, K_MSEC(poll_ms));
	}
}

static void accel_sensor_work_handler(struct k_work *work)
{
	int ret;

	ARG_UNUSED(work);  /* Prevent unused warning - we use ppg_state.accel_work directly */

	if (!ppg_state.measuring || !ppg_state.accel_dev) {
		return;
	}

	ret = accel_cache_refresh_once();
	if (ret) {
		LOG_WRN("Failed to refresh accel cache: %d", ret);
		goto reschedule;
	}

reschedule:
	/* Reschedule if still measuring */
	if (ppg_state.measuring) {
		uint32_t period_ms = 1000 / ppg_state.config.sample_rate_hz;
		ppg_work_reschedule(&ppg_state.accel_work, K_MSEC(period_ms));
	}
}

static void process_sample(uint32_t ir, uint32_t red, uint32_t green,
			   uint32_t ambient,
			   int16_t accel_x, int16_t accel_y, int16_t accel_z)
{
	int32_t ir_dc, ir_ac, red_dc, red_ac;
	int32_t green_dc, green_ac;
	int32_t hr_ac;
	uint16_t rr_interval_ms = 0;

	k_mutex_lock(&ppg_state.lock, K_FOREVER);

	ppg_preproc_filter(&ppg_state.preproc, ir, red, green,
			   ambient,
			   &ir_dc, &ir_ac, &red_dc, &red_ac,
			   &green_dc, &green_ac);

	int ret = ppg_samples_add(&ppg_state.buffer, ir, red, green,
				  ambient, ir_ac,
#if defined(CONFIG_PPG_LOG_DC_AC)
				  ir_dc, red_dc, green_dc, red_ac, green_ac,
#endif
				  accel_x, accel_y, accel_z);
	if (ret == -ENOMEM) {
		/* Buffer holds record window only; live-tail keeps DSP going. */
		ret = 0;
	} else if (ret) {
		if (!ppg_state.measuring || ppg_state.finalizing) {
			LOG_DBG("Sample ignored after stop/finalize: %d", ret);
		} else {
			LOG_ERR("Failed to add sample: %d", ret);
		}
		k_mutex_unlock(&ppg_state.lock);
		return;
	}

	/* Per-sample debug is intentionally gated: high-rate INFO logs can flood
	 * RTT in immediate mode and make touch/UI appear unresponsive. */
#if defined(CONFIG_PPG_ALGO_DEBUG_LOGGING)
	if (ppg_state.samples_processed < 10U ||
	    (ppg_state.samples_processed % 25U) == 0U) {
		LOG_INF("[%u/%u] IR=%u Red=%u Grn=%u AC_IR=%d AC_Red=%d AC_Grn=%d",
			ppg_state.samples_processed, ppg_state.config.sample_count,
			ir, red, green, ir_ac, red_ac, ppg_state.preproc.green.ac);
	}
#endif

	/* HR source selection:
	 * - MAX86141 RAW wearable: always green AC (fail closed at finalize if missing)
	 * - MAX3010x / other: IR AC */
	if (ppg_state.source_type == PPG_SOURCE_MAX86141) {
		hr_ac = ppg_state.preproc.green.ac;
	} else {
		hr_ac = ir_ac;
	}

	/* Peak detection for HR */
	ppg_peak_detect(&ppg_state.peaks, hr_ac,
				    k_uptime_get_32(), &rr_interval_ms);

	/* Update per-sample data */
	ppg_state.latest_sample.sample_number = ppg_state.samples_processed;
	ppg_state.latest_sample.total_samples = ppg_state.session_target > 0U ?
						ppg_state.session_target :
						ppg_state.config.sample_count;
	ppg_state.latest_sample.raw_ir = ir;
	ppg_state.latest_sample.raw_red = red;
	ppg_state.latest_sample.raw_green = green;
	ppg_state.latest_sample.dc_ir = ir_dc;
	ppg_state.latest_sample.dc_red = red_dc;
	ppg_state.latest_sample.dc_green = green_dc;
	ppg_state.latest_sample.ac_ir = ir_ac;
	ppg_state.latest_sample.ac_red = red_ac;
	ppg_state.latest_sample.ac_green = green_ac;
	ppg_state.latest_sample.accel_x = accel_x;
	ppg_state.latest_sample.accel_y = accel_y;
	ppg_state.latest_sample.accel_z = accel_z;
	ppg_state.latest_sample.timestamp_ms = k_uptime_get_32();
	ppg_state.sample_ready = true;

	ppg_state.samples_processed++;

	k_mutex_unlock(&ppg_state.lock);

	/* Fire sample ready trigger */
	ppg_trigger_fire(PPG_TRIG_SAMPLE_READY);

	ppg_maybe_advance_staged();
}

/* ========== Medical Validity Gate ========== */
/*
 * BUGFIX (2026-09-05): a much lower confidence floor than
 * ppg_state.config.quality_threshold (default 50%), used only to catch
 * genuinely near-zero-signal readings (0-14%) that shouldn't be shown
 * as if they were real measurements — e.g. "SpO2 = 85%, conf = 0%" from
 * a real export. Readings between this floor and 100% pass through
 * freely per the 2026-08-31 fix (a merely-lower-confidence fresh
 * reading is still preferred over a stale one) — this only blocks the
 * "there is effectively no signal at all" case. See
 * ppg_apply_quality_gate() and the Hb validity checks below for where
 * this is used.
 */
#define PPG_MIN_TRUSTWORTHY_CONFIDENCE 15U

/**
 * @brief Apply unified validity gate to a finalised result (wellness heuristics).
 *
 * Populates result->quality_reason, applies motion/quality confidence penalties,
 * then re-derives hr_valid / spo2_valid using physiologic range, signal quality,
 * beat count, and confidence threshold in combination.
 *
 * Must be called while ppg_state.lock is held and all result fields are filled.
 */
static void ppg_apply_quality_gate(struct ppg_algo_result *result)
{
	uint8_t reason = PPG_QUALITY_REASON_OK;

	/* Perfusion index floor (RAW path only).
	 * PI < 0.3% (perfusion_index x10 < 3) typically means no skin contact.
	 * Skip for HUB mode — the hub runs its own SCD algorithm. */
	if (ppg_state.source_type != PPG_SOURCE_MAX32664_HUB &&
	    result->perfusion_index < 3U) {
		reason |= PPG_QUALITY_REASON_LOW_PERFUSION;
	}

	/* Motion: always flag reason. When motion_rejection is enabled,
	 * invalidate SpO2 and hold last good HR (do not invent a new HR).
	 * Otherwise soft-halve confidence (legacy wellness behaviour). */
	if (result->motion_detected) {
		reason |= PPG_QUALITY_REASON_MOTION;
		if (ppg_state.config.motion_rejection) {
			result->spo2_valid = false;
			result->spo2_confidence = 0;
			if (ppg_state.last_good_hr_bpm > 0U) {
				result->hr_bpm = ppg_state.last_good_hr_bpm;
				result->hr_confidence = ppg_state.last_good_hr_conf;
			} else {
				result->hr_confidence = 0;
			}
		} else {
			result->hr_confidence   = result->hr_confidence   / 2U;
			result->spo2_confidence = result->spo2_confidence / 2U;
		}
	}

	/* SNR / signal quality floor */
	if (result->quality <= PPG_QUALITY_POOR) {
		reason |= PPG_QUALITY_REASON_LOW_SNR;
	}

	/* Insufficient RR intervals for reliable HR average (RAW path only) */
	if (ppg_state.source_type != PPG_SOURCE_MAX32664_HUB &&
	    ppg_state.peaks.rr_count < 3U) {
		reason |= PPG_QUALITY_REASON_INSUFFICIENT_BEATS;
	}

	/* Physiologic range checks.
	 * CONFIG_PPG_ALGO_HR_MIN/MAX are only generated by Kconfig when
	 * PPG_ALGO_HR_ENABLED=y; provide hard-coded fallbacks so this gate
	 * compiles unconditionally with medically accepted defaults. */
#ifndef CONFIG_PPG_ALGO_HR_MIN
#define CONFIG_PPG_ALGO_HR_MIN 40
#endif
#ifndef CONFIG_PPG_ALGO_HR_MAX
#define CONFIG_PPG_ALGO_HR_MAX 200
#endif
#ifndef CONFIG_PPG_ALGO_SPO2_MIN
#define CONFIG_PPG_ALGO_SPO2_MIN 70
#endif
#ifndef CONFIG_PPG_ALGO_SPO2_MAX
#define CONFIG_PPG_ALGO_SPO2_MAX 100
#endif
	if (result->hr_bpm > 0U &&
	    (result->hr_bpm < CONFIG_PPG_ALGO_HR_MIN ||
	     result->hr_bpm > CONFIG_PPG_ALGO_HR_MAX)) {
		reason |= PPG_QUALITY_REASON_OUT_OF_RANGE;
	}
	if (result->spo2_percent > 0U &&
	    (result->spo2_percent < CONFIG_PPG_ALGO_SPO2_MIN ||
	     result->spo2_percent > CONFIG_PPG_ALGO_SPO2_MAX)) {
		reason |= PPG_QUALITY_REASON_OUT_OF_RANGE;
	}

	/*
	 * Low confidence after any motion penalty.
	 *
	 * BUGFIX (2026-08-29): these two used to set the SAME shared
	 * PPG_QUALITY_REASON_LOW_CONFIDENCE bit, and block_spo2 included
	 * that bit — so whenever HR's confidence was below threshold, the
	 * shared bit got set and SpO2 was ALSO invalidated by it, even
	 * though SpO2's own confidence was independently fine. Confirmed
	 * on-device: "Quality gate: 0x4a hr_valid=0 (61 BPM conf=20%)
	 * spo2_valid=0 (91% conf=100%)" — SpO2 at 100% confidence, marked
	 * invalid anyway, purely because HR's confidence was low. This is
	 * also most of why HR_BPM and SpO2_Pct showed up constant across
	 * separate measurements in Excel exports: once marked invalid,
	 * ppg_logger.c's carry-forward logic substitutes the last known
	 * good value instead of persisting the fresh (but "invalid") one —
	 * so a chronically-low HR confidence was silently freezing SpO2's
	 * displayed/exported value too, cycle after cycle. Split into two
	 * independent bits so one channel's confidence can no longer gate
	 * the other's validity.
	 *
	 * BUGFIX (2026-09-05): the 2026-08-31 change below this comment
	 * removed confidence from blocking validity *entirely*, per direct
	 * request, so a fresh 48-70% confidence reading would stop being
	 * discarded in favor of a stale one. That worked, but went slightly
	 * too far — real exports afterward showed rows like "SpO2 = 85%,
	 * conf = 0%" and "Hb = 10.1 g/dL, conf = 0%": a genuinely
	 * meaningless reading (no real signal at all) still being written
	 * as if it were a real measurement, which is worse than showing a
	 * stale value. The threshold below is now MIN_TRUSTWORTHY_CONFIDENCE
	 * (15%) instead of the full quality_threshold (50%) — low enough
	 * that the 20-70% readings from the 08-31 fix still pass through
	 * untouched, high enough to catch true near-zero-signal cases.
	 */
	if (result->hr_confidence < PPG_MIN_TRUSTWORTHY_CONFIDENCE) {
		reason |= PPG_QUALITY_REASON_LOW_CONFIDENCE;
	}
	if (result->spo2_confidence < PPG_MIN_TRUSTWORTHY_CONFIDENCE) {
		reason |= PPG_QUALITY_REASON_SPO2_LOW_CONFIDENCE;
	}

	result->quality_reason = reason;

	/*
	 * BUGFIX (2026-08-31, explicit request): a merely-lower-confidence
	 * fresh reading (roughly 15-100%, see MIN_TRUSTWORTHY_CONFIDENCE
	 * above) is no longer swapped out for a stale carried-forward one.
	 *
	 * Why: after the 2026-08-29 cross-contamination fix, HR still came
	 * out "constant" in Excel exports (e.g. 60 BPM across 5 separate
	 * measurements) even though the terminal log showed HR being
	 * computed freshly and reasonably each time — just usually at
	 * 20-48% confidence, under the old 50% threshold. Every one of
	 * those fresh values was being discarded in favor of whatever the
	 * first high-confidence reading happened to be, cycle after cycle.
	 * Per direct instruction: a fresh reading at 70% confidence — or
	 * lower — is preferred over an old one that happened to be more
	 * confident. The confidence column already exists precisely so a
	 * consumer (app, spreadsheet, person) can judge trust themselves;
	 * using it as a hard gate that hides the fresh value defeats that
	 * purpose. LOW_CONFIDENCE/SPO2_LOW_CONFIDENCE are back in the block
	 * masks below as of 2026-09-05, but only for the much lower floor
	 * set above — see that comment for why.
	 *
	 * Genuine failure conditions are unaffected: NO_CONTACT,
	 * LOW_PERFUSION (signal too weak to mean anything at all),
	 * OUT_OF_RANGE, INSUFFICIENT_BEATS, and MOTION still block and
	 * still trigger carry-forward as before — those indicate there's
	 * no trustworthy fresh value to show at all, which is different
	 * from "a value was computed, but confidence is only middling."
	 */
	const uint8_t block_all = PPG_QUALITY_REASON_NO_CONTACT
				| PPG_QUALITY_REASON_LOW_PERFUSION
				| PPG_QUALITY_REASON_OUT_OF_RANGE;

	uint8_t block_spo2 = block_all | PPG_QUALITY_REASON_SPO2_LOW_CONFIDENCE;
	uint8_t block_hr = block_all | PPG_QUALITY_REASON_INSUFFICIENT_BEATS
				      | PPG_QUALITY_REASON_LOW_CONFIDENCE;

	if (ppg_state.config.motion_rejection &&
	    (reason & PPG_QUALITY_REASON_MOTION)) {
		block_spo2 |= PPG_QUALITY_REASON_MOTION;
		if (ppg_state.last_good_hr_bpm == 0U) {
			block_hr |= PPG_QUALITY_REASON_MOTION;
		}
	}

	result->hr_valid = (result->hr_bpm > 0U) && !(reason & block_hr);
	result->spo2_valid = (result->spo2_percent > 0U) && !(reason & block_spo2);

	/* Remember last good HR for motion hold */
	if (result->hr_valid && result->hr_bpm > 0U &&
	    !(reason & PPG_QUALITY_REASON_MOTION)) {
		ppg_state.last_good_hr_bpm = result->hr_bpm;
		ppg_state.last_good_hr_conf = result->hr_confidence;
	}

	/* Wearable MAX86141: green required for HR/Hb — fail closed (SpO2 stays Red/IR). */
	if (ppg_state.source_type == PPG_SOURCE_MAX86141 &&
	    !ppg_state.green_channel_available) {
		result->hr_valid = false;
		result->hb_valid = false;
		LOG_WRN("MAX86141: green never plausible — HR/Hb invalidated");
	}

	if (reason != PPG_QUALITY_REASON_OK) {
		LOG_INF("Quality gate: 0x%02x hr_valid=%d (%u BPM conf=%u%%) "
			"spo2_valid=%d (%u%% conf=%u%%)",
			reason,
			(int)result->hr_valid,   result->hr_bpm,      result->hr_confidence,
			(int)result->spo2_valid, result->spo2_percent, result->spo2_confidence);
	}
}

#if defined(CONFIG_PPG_ALGO_STAGED_LIVE)
/* Caller must hold ppg_state.lock. Fills latest_result from RAW DSP state. */
static void ppg_compute_raw_result_locked(void)
{
	uint16_t hr_bpm = 0;
	uint8_t hr_conf = 0;
	uint16_t spo2_pct = 0;
	uint8_t spo2_conf = 0;
	uint16_t r_value = 0;
	int32_t ac_red_amp = 0;
	int32_t ac_ir_amp = 0;
	int32_t ac_green_amp = 0;
	struct ppg_algo_result *result = &ppg_state.latest_result;

	ppg_quality_calculate(&ppg_state.buffer, &ppg_state.preproc, &ppg_state.quality);
	ppg_ac_amplitudes_from_buffer(&ppg_state.buffer, &ppg_state.preproc,
				      &ppg_state.peaks,
				      &ac_red_amp, &ac_ir_amp, &ac_green_amp);

	if (IS_ENABLED(CONFIG_PPG_ALGO_HR_ENABLED)) {
		ppg_hr_calculate(&ppg_state.peaks, &hr_bpm, &hr_conf);
	}
	if (IS_ENABLED(CONFIG_PPG_ALGO_SPO2_ENABLED)) {
		ppg_spo2_calculate(ppg_state.preproc.red.dc, ac_red_amp,
				   ppg_state.preproc.ir.dc, ac_ir_amp,
				   &spo2_pct, &spo2_conf, &r_value);
	}

	memset(result, 0, sizeof(*result));
#if defined(CONFIG_CALIBRATION)
	calibration_apply_ppg_hr_spo2(&hr_bpm, &spo2_pct);
#endif
	result->hr_bpm = hr_bpm;
	result->hr_confidence = hr_conf;
	result->hr_valid = (hr_conf >= ppg_state.config.quality_threshold);
	result->spo2_percent = spo2_pct;
	result->spo2_confidence = spo2_conf;
	result->spo2_valid = (spo2_conf >= ppg_state.config.quality_threshold);
	result->quality = ppg_state.quality.quality;
	result->perfusion_index = ppg_state.quality.perfusion_index_x10;
	result->snr_db = ppg_state.quality.snr_db_x10;
	result->motion_detected = ppg_state.quality.motion_detected;
	result->motion_magnitude = ppg_state.quality.motion_magnitude;
	result->dc_red = (uint32_t)ppg_state.preproc.red.dc;
	result->dc_ir = (uint32_t)ppg_state.preproc.ir.dc;
	result->dc_green = (uint32_t)ppg_state.preproc.green.dc;
	result->ac_red = (uint32_t)((ac_red_amp < 0) ? -ac_red_amp : ac_red_amp);
	result->ac_ir = (uint32_t)((ac_ir_amp < 0) ? -ac_ir_amp : ac_ir_amp);
	result->ac_green = (uint32_t)((ac_green_amp < 0) ? -ac_green_amp : ac_green_amp);
	result->green_valid = ppg_state.green_channel_available &&
			      (result->dc_green > 500U) && (result->ac_green > 0U);
	result->r_value_x1000 = r_value;

	if (IS_ENABLED(CONFIG_PPG_ALGO_HB_ENABLED) && result->green_valid) {
		int hb_ret = ppg_hb_calculate(
			(int32_t)result->dc_green, (int32_t)result->ac_green,
			(int32_t)result->dc_ir, (int32_t)result->ac_ir,
			(int32_t)result->dc_red,
			&result->hb_g_dl_x10, &result->hb_confidence);

		result->hb_valid = (hb_ret == 0) &&
				   (result->hb_confidence >= PPG_MIN_TRUSTWORTHY_CONFIDENCE);
		if (hb_ret != 0 && (ppg_state.samples_processed % 100U) == 0U) {
			LOG_INF("Hb pending: ret=%d dc_g=%u ac_g=%u dc_ir=%u ac_ir=%u",
				hb_ret, result->dc_green, result->ac_green,
				result->dc_ir, result->ac_ir);
		}
	} else if (IS_ENABLED(CONFIG_PPG_ALGO_HB_ENABLED) &&
		   (ppg_state.samples_processed % 100U) == 0U) {
		LOG_INF("Hb skipped: green_valid=0 avail=%u dc_g=%u ac_g=%u",
			ppg_state.green_channel_available ? 1U : 0U,
			result->dc_green, result->ac_green);
	}
	if (IS_ENABLED(CONFIG_PPG_ALGO_RESP_ENABLED) && ppg_state.buffer.ir_ac != NULL) {
		int resp_ret = ppg_resp_calculate(
			ppg_state.buffer.ir_ac, ppg_state.buffer.count,
			ppg_state.config.sample_rate_hz, &ppg_state.resp,
			&result->resp_rate_bpm, &result->resp_confidence);

		result->resp_valid = (resp_ret == 0);
	}
	if (IS_ENABLED(CONFIG_PPG_ALGO_HRV_ENABLED)) {
		int hrv_ret = ppg_hrv_calculate(&ppg_state.peaks,
						&result->sdnn_ms, &result->rmssd_ms);

		result->hrv_valid = (hrv_ret == 0);
		if (ppg_state.peaks.rr_count > 0U) {
			result->rr_interval_ms = (uint16_t)ppg_state.peaks.rr_intervals[
				ppg_state.peaks.rr_count - 1U];
		}

		/*
		 * BUGFIX: SDNN_ms/RMSSD_ms always 0 in persisted vitals
		 * records. ppg_hrv_calculate() needs >= HRV_MIN_RR_INTERVALS
		 * (20) RR intervals from real peak detection — at a fixed
		 * 20s acquisition window, that requires an HR above ~66 bpm
		 * with *zero* missed beats, so it fails for a large share of
		 * normal resting heart rates even on a clean signal (the
		 * exported patient data: 62 bpm, ~20 beats possible, 0 or 1
		 * missed detection away from the threshold every time).
		 *
		 * finalize_measurement() already has a working fallback for
		 * exactly this case — ppg_spreadsheet_calculate(), a direct
		 * port of the client-provided reference spreadsheet with a
		 * much lower bar (>= PPG_SS_MIN_PEAKS = 3 peaks in the first
		 * 300 samples) — but that path only fed the BLE live/finalize
		 * results, never the record this function persists (the one
		 * that becomes the Excel export). Reuse it here so the value
		 * that gets recorded isn't silently left at the RR-method's
		 * unmet-threshold default of 0.
		 */
		if (!result->hrv_valid) {
			struct ppg_ss_result ss_result;
			int ss_ret = ppg_spreadsheet_calculate(&ppg_state.buffer, &ss_result);

			if (ss_ret == 0 && ss_result.valid && ss_result.sdnn_x100 > 0U) {
				result->sdnn_ms = (uint16_t)(ss_result.sdnn_x100 / 100U);
				result->rmssd_ms = (uint16_t)(ss_result.rmssd_x100 / 100U);
				result->hrv_valid = true;
				LOG_INF("HRV: RR-method short (rr_count=%u < 20), used "
					"spreadsheet-method fallback: SDNN=%u RMSSD=%u ms",
					ppg_state.peaks.rr_count,
					result->sdnn_ms, result->rmssd_ms);
			}
		}
	}
	if (IS_ENABLED(CONFIG_PPG_ALGO_BP_ENABLED) && result->hr_bpm > 0U) {
		int bp_ret = ppg_bp_estimate(result->hr_bpm,
					     result->ac_ir, result->dc_ir,
					     result->ac_red, result->dc_red,
					     &result->systolic_mmhg,
					     &result->diastolic_mmhg);

		result->bp_valid = (bp_ret == 0) && (ppg_state.peaks.rr_count >= 10U);
		if (!result->bp_valid && (ppg_state.samples_processed % 100U) == 0U) {
			LOG_INF("BP pending: ret=%d rr=%u hr=%u ac_ir=%u",
				bp_ret, ppg_state.peaks.rr_count,
				result->hr_bpm, result->ac_ir);
		}
	}
	result->timestamp_ms = k_uptime_get_32();
	result->sample_count = ppg_state.samples_processed;
	ppg_apply_quality_gate(result);
	ppg_state.result_ready = true;
}

static void ppg_log_vitals_snapshot(uint16_t log_count, uint32_t start_time_ms)
{
	struct ppg_sample_buffer *buffer = &ppg_state.buffer;
	uint32_t timestamp_sec = 0;
	float total_time_sec;
	uint16_t rate_hz;
	const struct ppg_algo_result *res = &ppg_state.latest_result;
	static const int16_t zero_accel[PPG_MAX_SAMPLES];
	const int16_t *log_ax;
	const int16_t *log_ay;
	const int16_t *log_az;
	struct ppg_logger_vitals_params vp;

	if (log_count == 0U || buffer->ir == NULL) {
		return;
	}
	if (log_count > buffer->count) {
		log_count = buffer->count;
	}

	LOG_INF("PPG vitals CSV flush starting (%u samples)", log_count);
	if (rtc_get_unix_time(&timestamp_sec) != 0) {
		timestamp_sec = k_uptime_get_32() / 1000;
	}
	total_time_sec = (k_uptime_get_32() - start_time_ms) / 1000.0f;
	rate_hz = ppg_state.config.sample_rate_hz;
	log_ax = buffer->has_accel ? buffer->accel_x : zero_accel;
	log_ay = buffer->has_accel ? buffer->accel_y : zero_accel;
	log_az = buffer->has_accel ? buffer->accel_z : zero_accel;

	vp = (struct ppg_logger_vitals_params){
		.ir = buffer->ir,
		.red = buffer->red,
		.green = buffer->green,
		.ambient = buffer->ambient,
#if defined(CONFIG_PPG_LOG_DC_AC)
		.ir_dc = buffer->ir_dc,
		.red_dc = buffer->red_dc,
		.green_dc = buffer->green_dc,
		.ir_ac = buffer->ir_ac,
		.red_ac = buffer->red_ac,
		.green_ac = buffer->green_ac,
#endif
		.accel_x = log_ax,
		.accel_y = log_ay,
		.accel_z = log_az,
		.count = log_count,
		.timestamp_sec = timestamp_sec,
		.capture_dt_ms = buffer->capture_dt_ms,
		.capture_t0_uptime_ms = buffer->capture_t0_uptime_ms,
		.capture_t0_set = buffer->capture_t0_set,
		.hr_bpm = res->hr_bpm,
		.hr_confidence = res->hr_confidence,
		.hr_valid = res->hr_valid,
		.spo2_percent = res->spo2_percent,
		.spo2_confidence = res->spo2_confidence,
		.spo2_valid = res->spo2_valid,
		.r_value_x1000 = res->r_value_x1000,
		.hb_g_dl_x10 = res->hb_g_dl_x10,
		.hb_confidence = res->hb_confidence,
		.hb_valid = res->hb_valid,
		.resp_rate_bpm = res->resp_rate_bpm,
		.resp_confidence = res->resp_confidence,
		.resp_valid = res->resp_valid,
		.sdnn_ms = res->sdnn_ms,
		.rmssd_ms = res->rmssd_ms,
		.hrv_valid = res->hrv_valid,
		.systolic_mmhg = res->systolic_mmhg,
		.diastolic_mmhg = res->diastolic_mmhg,
		.bp_valid = res->bp_valid,
		.quality = (uint8_t)res->quality,
		.perfusion_index_x10 = res->perfusion_index,
		.snr_db_x10 = res->snr_db,
		.quality_reason = res->quality_reason,
		.led_pa = 0,
		.motion_bin = res->motion_detected ?
			((res->motion_magnitude > 250U) ? 2U : 1U) : 0U,
		.skin_tone_bin = 0,
		.ac_ir_amp = (uint16_t)CLAMP(res->ac_ir, 0U, 65535U),
		.sample_rate_hz = rate_hz,
		.total_time_sec = total_time_sec,
	};
#if defined(CONFIG_MAX32664_MODE_RAW)
	if (ppg_state.source_type == PPG_SOURCE_MAX86141) {
		vp.led_pa = max32664_raw_get_led_pa();
	}
#endif
	if (ppg_logger_log_vitals_measurement(&vp) != 0) {
		LOG_ERR("Vitals CSV log failed");
	}
}

static void ppg_do_record_ready(void)
{
	uint32_t start_time_ms;
	uint16_t log_count;

	k_mutex_lock(&ppg_state.lock, K_FOREVER);
	if (ppg_state.record_ready_fired || !ppg_state.measuring) {
		k_mutex_unlock(&ppg_state.lock);
		return;
	}

	if (ppg_state.source_type == PPG_SOURCE_MAX32664_HUB) {
		struct ppg_algo_result *result = &ppg_state.latest_result;

		result->sample_count = ppg_state.samples_processed;
		result->timestamp_ms = k_uptime_get_32();
#if defined(CONFIG_CALIBRATION)
		calibration_apply_ppg_hr_spo2(&result->hr_bpm, &result->spo2_percent);
#endif
		ppg_apply_quality_gate(result);
		ppg_state.result_ready = true;
	} else {
		ppg_compute_raw_result_locked();
#if defined(CONFIG_MAX32664_MODE_RAW)
		if (ppg_state.source_type == PPG_SOURCE_MAX86141 &&
		    ppg_state.hub_dev != NULL) {
			struct ppg_algo_result *r = &ppg_state.latest_result;

			(void)max32664_raw_host_agc(ppg_state.hub_dev,
						    r->dc_ir, r->ac_ir,
						    r->dc_red, r->ac_red,
						    r->dc_green, r->ac_green);
		}
#endif
	}

	start_time_ms = ppg_state.start_time_ms;
	log_count = MIN(ppg_state.buffer.count, ppg_state.record_target);
	ppg_state.record_ready_fired = true;
	k_mutex_unlock(&ppg_state.lock);

	LOG_INF("PPG RECORD_READY at %u (session target %u) "
		"Hb=%u.%u(v=%u) BP=%u/%u(v=%u) rr=%u",
		log_count, ppg_state.session_target,
		ppg_state.latest_result.hb_g_dl_x10 / 10U,
		ppg_state.latest_result.hb_g_dl_x10 % 10U,
		ppg_state.latest_result.hb_valid ? 1U : 0U,
		ppg_state.latest_result.systolic_mmhg,
		ppg_state.latest_result.diastolic_mmhg,
		ppg_state.latest_result.bp_valid ? 1U : 0U,
		ppg_state.peaks.rr_count);
	/* NOR write here; health_sched post-guard arms in finalize (not while
	 * a live tail would still be driving SFH LEDs). */
	ppg_log_vitals_snapshot(log_count, start_time_ms);
	ppg_trigger_fire(PPG_TRIG_RECORD_READY);

	if (ppg_state.session_target <= ppg_state.record_target) {
		finalize_measurement();
	}
}
#endif /* CONFIG_PPG_ALGO_STAGED_LIVE */

static void ppg_maybe_advance_staged(void)
{
#if defined(CONFIG_PPG_ALGO_STAGED_LIVE)
	uint16_t n = ppg_state.samples_processed;
	uint16_t partial_n = (uint16_t)CONFIG_PPG_ALGO_PARTIAL_VITALS_SAMPLES;
	const struct ppg_algo_result *r;

	if (partial_n > ppg_state.record_target) {
		partial_n = ppg_state.record_target;
	}

	if (!ppg_state.partial_vitals_fired && n >= partial_n) {
		if (ppg_state.source_type != PPG_SOURCE_MAX32664_HUB) {
			k_mutex_lock(&ppg_state.lock, K_FOREVER);
			ppg_compute_raw_result_locked();
			k_mutex_unlock(&ppg_state.lock);
		}
		ppg_state.partial_vitals_fired = true;
		LOG_INF("PPG PARTIAL_VITALS at sample %u", n);
		ppg_trigger_fire(PPG_TRIG_PARTIAL_VITALS);
	} else if (ppg_state.partial_vitals_fired &&
		   ppg_state.source_type != PPG_SOURCE_MAX32664_HUB &&
		   (n % 25U) == 0U) {
		/* Keep refining through live tail — peaks/RR still accrue after
		 * RECORD_READY while the sample buffer stays at capacity. */
		k_mutex_lock(&ppg_state.lock, K_FOREVER);
		ppg_compute_raw_result_locked();
		k_mutex_unlock(&ppg_state.lock);
	}

	if (!ppg_state.partial_vascular_fired && n >= partial_n) {
		if (ppg_state.source_type != PPG_SOURCE_MAX32664_HUB) {
			k_mutex_lock(&ppg_state.lock, K_FOREVER);
			ppg_compute_raw_result_locked();
			k_mutex_unlock(&ppg_state.lock);
		}
		r = &ppg_state.latest_result;
		if (r->bp_valid || r->hb_valid || r->hrv_valid || r->resp_valid ||
		    n >= ppg_state.record_target) {
			ppg_state.partial_vascular_fired = true;
			LOG_INF("PPG PARTIAL_VASCULAR at sample %u (Hb=%u.%u bp=%u/%u rr=%u)",
				n,
				r->hb_g_dl_x10 / 10U, r->hb_g_dl_x10 % 10U,
				r->systolic_mmhg, r->diastolic_mmhg,
				ppg_state.peaks.rr_count);
			ppg_trigger_fire(PPG_TRIG_PARTIAL_VASCULAR);
		}
	}

	if (!ppg_state.record_ready_fired && n >= ppg_state.record_target) {
		ppg_do_record_ready();
	}

	if (ppg_state.record_ready_fired && n >= ppg_state.session_target) {
		finalize_measurement();
	}
#else
	if (ppg_state.samples_processed >= ppg_state.session_target) {
		finalize_measurement();
	}
#endif
}

static void finalize_measurement(void)
{
	uint16_t hr_bpm = 0;
	uint8_t hr_conf = 0;
	uint16_t spo2_pct = 0;
	uint8_t spo2_conf = 0;
	uint16_t r_value = 0;
	struct ppg_sample_buffer *buffer = NULL;
	uint16_t buffer_count = 0;
	uint32_t start_time_ms = 0;
	bool skip_vitals_log = false;

	k_mutex_lock(&ppg_state.lock, K_FOREVER);

#if defined(CONFIG_PPG_ALGO_STAGED_LIVE)
	/* After RECORD_READY + live tail: one last refine, then stop AFE.
	 * NOR vitals were already written at RECORD_READY. */
	if (ppg_state.record_ready_fired) {
		if (ppg_state.source_type != PPG_SOURCE_MAX32664_HUB) {
			ppg_compute_raw_result_locked();
		}
		{
			const struct ppg_algo_result *r = &ppg_state.latest_result;

			LOG_INF("PPG session complete after live tail (%u/%u): "
				"Hb=%u.%u (valid=%u green=%u) BP=%u/%u (valid=%u rr=%u) "
				"SDNN=%u RMSSD=%u",
				ppg_state.samples_processed, ppg_state.session_target,
				r->hb_g_dl_x10 / 10U, r->hb_g_dl_x10 % 10U,
				r->hb_valid ? 1U : 0U, r->green_valid ? 1U : 0U,
				r->systolic_mmhg, r->diastolic_mmhg,
				r->bp_valid ? 1U : 0U, ppg_state.peaks.rr_count,
				r->sdnn_ms, r->rmssd_ms);
		}
		start_time_ms = ppg_state.start_time_ms;
		ppg_state.measuring = false;
		k_work_cancel_delayable(&ppg_state.ppg_work);
		k_work_cancel_delayable(&ppg_state.accel_work);
		k_mutex_unlock(&ppg_state.lock);
		skip_vitals_log = true;
		goto post_finalize;
	}
#endif

	if (ppg_state.source_type == PPG_SOURCE_MAX32664_HUB) {
		/* HUB mode: process_hub_sample() has been updating
		 * ppg_state.latest_result continuously. All we need to do is
		 * pin the final timestamp/sample_count, mark it ready, and
		 * tear down the work items — no on-chip DSP to run. */
		struct ppg_algo_result *result = &ppg_state.latest_result;

		result->sample_count = ppg_state.samples_processed;
		result->timestamp_ms = k_uptime_get_32();
		/* hr_valid / spo2_valid flags were set on each hub update based
		 * on the hub's confidence. Keep them — they're the authoritative
		 * "is this reading trustworthy" signal in HUB mode. */

#if defined(CONFIG_CALIBRATION)
		calibration_apply_ppg_hr_spo2(&result->hr_bpm, &result->spo2_percent);
#endif
		hr_bpm    = result->hr_bpm;
		hr_conf   = result->hr_confidence;
		spo2_pct  = result->spo2_percent;
		spo2_conf = result->spo2_confidence;
		r_value   = 0;
		uint8_t quality_snap = (uint8_t)result->quality;
		uint16_t frames_snap = ppg_state.samples_processed;

		ppg_apply_quality_gate(result);
		ppg_state.result_ready = true;
		start_time_ms = ppg_state.start_time_ms;
		buffer = &ppg_state.buffer;
		buffer_count = ppg_state.buffer.count;

		ppg_state.measuring = false;
		k_work_cancel_delayable(&ppg_state.ppg_work);
		k_work_cancel_delayable(&ppg_state.accel_work);

		k_mutex_unlock(&ppg_state.lock);

		LOG_INF("Measurement complete (HUB): HR=%u BPM (%u%% conf), "
			"SpO2=%u%% (%u%% conf), Q=%u, frames=%u",
			hr_bpm, hr_conf, spo2_pct, spo2_conf,
			quality_snap, frames_snap);

		goto post_finalize;
	}

	/* Calculate quality metrics */
	ppg_quality_calculate(&ppg_state.buffer, &ppg_state.preproc,
			      &ppg_state.quality);

	/* Windowed AC pulse amplitudes (TI SLAA655 / Maxim PAC) — not last residual */
	int32_t ac_red_amp = 0;
	int32_t ac_ir_amp = 0;
	int32_t ac_green_amp = 0;

	ppg_ac_amplitudes_from_buffer(&ppg_state.buffer, &ppg_state.preproc,
				      &ppg_state.peaks,
				      &ac_red_amp, &ac_ir_amp, &ac_green_amp);

	/* Calculate HR */
	if (IS_ENABLED(CONFIG_PPG_ALGO_HR_ENABLED)) {
		ppg_hr_calculate(&ppg_state.peaks, &hr_bpm, &hr_conf);
	}

	/* Calculate SpO2 from pulse amplitudes */
	if (IS_ENABLED(CONFIG_PPG_ALGO_SPO2_ENABLED)) {
		int32_t avg_dc_red = ppg_state.preproc.red.dc;
		int32_t avg_dc_ir = ppg_state.preproc.ir.dc;

		ppg_spo2_calculate(avg_dc_red, ac_red_amp, avg_dc_ir, ac_ir_amp,
				   &spo2_pct, &spo2_conf, &r_value);
	}

	/* Fill result structure */
	struct ppg_algo_result *result = &ppg_state.latest_result;
	memset(result, 0, sizeof(*result));

#if defined(CONFIG_CALIBRATION)
	calibration_apply_ppg_hr_spo2(&hr_bpm, &spo2_pct);
#endif

	result->hr_bpm = hr_bpm;
	result->hr_confidence = hr_conf;
	result->hr_valid = (hr_conf >= ppg_state.config.quality_threshold);

	result->spo2_percent = spo2_pct;
	result->spo2_confidence = spo2_conf;
	result->spo2_valid = (spo2_conf >= ppg_state.config.quality_threshold);

	result->quality = ppg_state.quality.quality;
	result->perfusion_index = ppg_state.quality.perfusion_index_x10;
	result->snr_db = ppg_state.quality.snr_db_x10;
	result->motion_detected = ppg_state.quality.motion_detected;
	result->motion_magnitude = ppg_state.quality.motion_magnitude;

	result->dc_red   = (uint32_t)ppg_state.preproc.red.dc;
	result->dc_ir    = (uint32_t)ppg_state.preproc.ir.dc;
	result->dc_green = (uint32_t)ppg_state.preproc.green.dc;
	result->ac_red   = (uint32_t)((ac_red_amp < 0) ? -ac_red_amp : ac_red_amp);
	result->ac_ir    = (uint32_t)((ac_ir_amp < 0) ? -ac_ir_amp : ac_ir_amp);
	result->ac_green = (uint32_t)((ac_green_amp < 0) ? -ac_green_amp : ac_green_amp);
	result->green_valid = ppg_state.green_channel_available &&
			      (result->dc_green > 500U) &&
			      (result->ac_green > 0U);
	result->r_value_x1000 = r_value;

	/* Hemoglobin estimation (requires green LED) — uses pulse amplitudes */
	if (IS_ENABLED(CONFIG_PPG_ALGO_HB_ENABLED) && result->green_valid) {
		int hb_ret = ppg_hb_calculate(
			(int32_t)result->dc_green, (int32_t)result->ac_green,
			(int32_t)result->dc_ir,    (int32_t)result->ac_ir,
			(int32_t)result->dc_red,
			&result->hb_g_dl_x10, &result->hb_confidence);
		result->hb_valid = (hb_ret == 0) &&
				   (result->hb_confidence >= PPG_MIN_TRUSTWORTHY_CONFIDENCE);
		if (hb_ret && hb_ret != -EINVAL && hb_ret != -ERANGE) {
			LOG_WRN("Hb calc failed: %d", hb_ret);
		}
	}

	/* Respiration rate estimation (requires ≥20 s of IR AC data) */
	if (IS_ENABLED(CONFIG_PPG_ALGO_RESP_ENABLED) &&
	    ppg_state.buffer.ir_ac != NULL) {
		int resp_ret = ppg_resp_calculate(
			ppg_state.buffer.ir_ac,
			ppg_state.buffer.count,
			ppg_state.config.sample_rate_hz,
			&ppg_state.resp,
			&result->resp_rate_bpm,
			&result->resp_confidence);
		result->resp_valid = (resp_ret == 0);
		if (resp_ret && resp_ret != -ENODATA && resp_ret != -ERANGE) {
			LOG_WRN("Resp calc failed: %d", resp_ret);
		}
	}

	/* HRV (SDNN / RMSSD) from RR history — experimental PRV */
	if (IS_ENABLED(CONFIG_PPG_ALGO_HRV_ENABLED)) {
		int hrv_ret = ppg_hrv_calculate(&ppg_state.peaks,
						&result->sdnn_ms,
						&result->rmssd_ms);

		result->hrv_valid = (hrv_ret == 0);
		if (hrv_ret && hrv_ret != -ENODATA) {
			LOG_WRN("HRV calc failed: %d", hrv_ret);
		}
		if (ppg_state.peaks.rr_count > 0U) {
			result->rr_interval_ms =
				(uint16_t)ppg_state.peaks.rr_intervals[
					ppg_state.peaks.rr_count - 1U];
		}
	}

	/* Sir's spreadsheet-method HR/HRV (formulas_of_Miisky.xlsx +
	 * formula_in_each_column.doc), run on the first 300 of the up-to-500
	 * collected samples. Feeds result->hr_bpm/sdnn_ms/rmssd_ms — same
	 * Vitals record schema, same Excel columns, no export-side changes
	 * needed — replacing them when this method produces a valid result.
	 * BP is deliberately excluded (see below). */
	struct ppg_ss_result ss_result;
	int ss_ret = ppg_spreadsheet_calculate(&ppg_state.buffer, &ss_result);

	if (ss_ret != 0 && ss_ret != -ENODATA) {
		LOG_WRN("Spreadsheet-method calc failed: %d", ss_ret);
	}

	if (ss_ret == 0 && ss_result.valid) {
		result->hr_bpm = ss_result.hr_bpm;
		result->sdnn_ms = (uint16_t)(ss_result.sdnn_x100 / 100U);
		result->rmssd_ms = (uint16_t)(ss_result.rmssd_x100 / 100U);
		result->hrv_valid = (ss_result.sdnn_x100 > 0U);

		/* BP intentionally NOT taken from this method: BJ/BK
		 * (1.03*AR+4, 1.02*AR+3) are applied to a dimensionless
		 * pulse-amplitude ratio, not a calibrated pressure — verified
		 * by hand this prints as single-digit values, not mmHg. Left
		 * unset here so it falls through to ppg_bp_estimate() below,
		 * which at least outputs in a real mmHg range. */
	}

	/* Approximate cuffless BP — experimental, off by default */
	if (IS_ENABLED(CONFIG_PPG_ALGO_BP_ENABLED) && result->hr_bpm > 0U &&
	    !result->bp_valid) {
		int bp_ret = ppg_bp_estimate(result->hr_bpm,
					     result->ac_ir, result->dc_ir,
					     result->ac_red, result->dc_red,
					     &result->systolic_mmhg,
					     &result->diastolic_mmhg);

		result->bp_valid = (bp_ret == 0) &&
				   (ppg_state.peaks.rr_count >= 10U);
		if (bp_ret != 0 && bp_ret != -EINVAL) {
			LOG_WRN("BP estimate failed: %d", bp_ret);
		}
	}

	result->timestamp_ms = k_uptime_get_32();
	result->sample_count = ppg_state.samples_processed;

	ppg_apply_quality_gate(result);
	ppg_state.result_ready = true;
	buffer = &ppg_state.buffer;
	buffer_count = ppg_state.buffer.count;
	start_time_ms = ppg_state.start_time_ms;

#if defined(CONFIG_MAX32664_MODE_RAW)
	/* Host AGC for next measurement — independent DC/AC targeting per
	 * channel (IR/Red/Green), see max32664_raw_host_agc(). */
	if (ppg_state.source_type == PPG_SOURCE_MAX86141 &&
	    ppg_state.hub_dev != NULL) {
		(void)max32664_raw_host_agc(ppg_state.hub_dev,
					    result->dc_ir, result->ac_ir,
					    result->dc_red, result->ac_red,
					    result->dc_green, result->ac_green);
	}
#endif

	const uint16_t rr_count_snap = ppg_state.peaks.rr_count;
	const bool green_valid_snap = result->green_valid;
	const uint32_t dc_green_snap = result->dc_green;
	const uint16_t sys_snap = result->systolic_mmhg;
	const uint16_t dia_snap = result->diastolic_mmhg;
	const bool bp_valid_snap = result->bp_valid;
	const uint16_t hb_snap = result->hb_g_dl_x10;
	const uint16_t hr_snap = result->hr_bpm;
	const uint8_t hr_conf_snap = result->hr_confidence;
	const bool hr_valid_snap = result->hr_valid;
	const uint16_t spo2_snap = result->spo2_percent;
	const uint8_t spo2_conf_snap = result->spo2_confidence;
	const bool spo2_valid_snap = result->spo2_valid;
	const uint8_t resp_snap = result->resp_rate_bpm;
	const uint16_t sdnn_snap = result->sdnn_ms;
	const uint16_t rmssd_snap = result->rmssd_ms;
	const uint8_t quality_snap = result->quality;

	/* Stop measurement */
	ppg_state.measuring = false;
	k_work_cancel_delayable(&ppg_state.ppg_work);
	k_work_cancel_delayable(&ppg_state.accel_work);

	k_mutex_unlock(&ppg_state.lock);

	LOG_INF("Measurement complete (VITALS): HR=%s%u BPM (%u%% conf), "
		"SpO2=%s%u%% (%u%% conf), Hb=%u.%u g/dL (green=%u dc_g=%u), "
		"Resp=%u bpm, SDNN=%u RMSSD=%u BP=%u/%u (bp_valid=%u rr=%u), "
		"Quality=0x%02X",
		hr_valid_snap ? "" : "invalid ",
		hr_snap, hr_conf_snap,
		spo2_valid_snap ? "" : "invalid ",
		spo2_snap, spo2_conf_snap,
		hb_snap / 10U, hb_snap % 10U,
		green_valid_snap ? 1U : 0U, dc_green_snap,
		resp_snap, sdnn_snap, rmssd_snap,
		sys_snap, dia_snap,
		bp_valid_snap ? 1U : 0U, rr_count_snap,
		quality_snap);

post_finalize:
	(void)0; /* label target — shared teardown below runs for both paths */
	ppg_state.finalizing = true;
#if defined(CONFIG_APP_HEALTH_SCHED)
	/* Arm CSV/shutdown guard once AFE stop is about to run (not at mid-session
	 * RECORD_READY). skip_vitals_log means NOR already flushed at record-ready. */
	health_sched_ppg_post_sample();
#endif
	led_status_clear(LED_STATUS_MEASURING);

	/* Stop the host-fed accelerometer feeder before AFE shutdown. Mirrors
	 * the pair in ppg_algo_stop_measurement() so natural completion (600
	 * samples reached) tears down the feeder too, instead of leaving it
	 * hammering the hub at 25 Hz after LEDs are off. */
	if (ppg_state.hub_dev != NULL) {
		(void)max32664_accel_feeder_stop(ppg_state.hub_dev);
	}

	/* Enter shutdown mode (SFH / MAX86141 LEDs off).
	 * Do not k_work_cancel_*_sync here — finalize runs from the PPG work
	 * handler; sync-cancel would deadlock. Drain refuses LED restore when
	 * hub sampling==false after this attr_set(0). */
	{
		int shdn_ret = ppg_sensor_shutdown();

		if (shdn_ret != 0) {
			LOG_WRN("PPG sensor shutdown failed: %d (SFH LEDs may stay on)",
				shdn_ret);
		} else {
			LOG_INF("PPG sensor shutdown (SFH LEDs off)");
		}
	}

	/* Log raw samples before advancing the health cycle — glucose start
	 * disables USB/FAT; logging after ppg_trigger_fire races that step. */
	if (!skip_vitals_log && buffer && buffer_count > 0) {
		LOG_INF("PPG vitals CSV flush starting (%u samples)", buffer_count);
		uint32_t timestamp_sec = 0;
		if (rtc_get_unix_time(&timestamp_sec) != 0) {
			timestamp_sec = k_uptime_get_32() / 1000;
		}
		float total_time_sec = (k_uptime_get_32() - start_time_ms) / 1000.0f;
		uint16_t rate_hz = ppg_state.config.sample_rate_hz;
		const struct ppg_algo_result *res = &ppg_state.latest_result;

		/* When accel wasn't captured into the buffer (no accel_dev at
		 * measurement start), supply zero-filled arrays so the logger
		 * can still write a valid CSV row. */
		static const int16_t zero_accel[PPG_MAX_SAMPLES];
		const int16_t *log_ax = buffer->has_accel ? buffer->accel_x : zero_accel;
		const int16_t *log_ay = buffer->has_accel ? buffer->accel_y : zero_accel;
		const int16_t *log_az = buffer->has_accel ? buffer->accel_z : zero_accel;
		/* Ambient array may be NULL for HUB mode; pass NULL is safe */
		const uint32_t *log_amb = buffer->ambient;

		if (!buffer->has_accel) {
			LOG_WRN("No accel in buffer — logging zeros for accel columns");
		}

		struct ppg_logger_vitals_params vp = {
			.ir          = buffer->ir,
			.red         = buffer->red,
			.green       = buffer->green,
			.ambient     = log_amb,
#if defined(CONFIG_PPG_LOG_DC_AC)
			.ir_dc       = buffer->ir_dc,
			.red_dc      = buffer->red_dc,
			.green_dc    = buffer->green_dc,
			.ir_ac       = buffer->ir_ac,
			.red_ac      = buffer->red_ac,
			.green_ac    = buffer->green_ac,
#endif
			.accel_x     = log_ax,
			.accel_y     = log_ay,
			.accel_z     = log_az,
			.count       = buffer_count,
			.timestamp_sec = timestamp_sec,
			.capture_dt_ms = buffer->capture_dt_ms,
			.capture_t0_uptime_ms = buffer->capture_t0_uptime_ms,
			.capture_t0_set = buffer->capture_t0_set,
			.hr_bpm      = res->hr_bpm,
			.hr_confidence = res->hr_confidence,
			.hr_valid    = res->hr_valid,
			.spo2_percent = res->spo2_percent,
			.spo2_confidence = res->spo2_confidence,
			.spo2_valid  = res->spo2_valid,
			.r_value_x1000 = res->r_value_x1000,
			.hb_g_dl_x10 = res->hb_g_dl_x10,
			.hb_confidence = res->hb_confidence,
			.hb_valid    = res->hb_valid,
			.resp_rate_bpm = res->resp_rate_bpm,
			.resp_confidence = res->resp_confidence,
			.resp_valid  = res->resp_valid,
			.sdnn_ms     = res->sdnn_ms,
			.rmssd_ms    = res->rmssd_ms,
			.hrv_valid   = res->hrv_valid,
			.systolic_mmhg = res->systolic_mmhg,
			.diastolic_mmhg = res->diastolic_mmhg,
			.bp_valid    = res->bp_valid,
			.quality     = (uint8_t)res->quality,
			.perfusion_index_x10 = res->perfusion_index,
			.snr_db_x10  = res->snr_db,
			.quality_reason = res->quality_reason,
			.led_pa = 0,
			.motion_bin = res->motion_detected ?
				((res->motion_magnitude > 250U) ? 2U : 1U) : 0U,
			.skin_tone_bin = 0, /* unknown until factory/UI sets */
			.ac_ir_amp = (uint16_t)CLAMP(res->ac_ir, 0U, 65535U),
			.sample_rate_hz = rate_hz,
			.total_time_sec = total_time_sec,
		};

#if defined(CONFIG_MAX32664_MODE_RAW)
		if (ppg_state.source_type == PPG_SOURCE_MAX86141) {
			vp.led_pa = max32664_raw_get_led_pa();
		}
#endif

		LOG_INF("Arms hooks: R=%u ACir=%u Qreason=0x%02x motion_bin=%u led_pa=0x%02x",
			res->r_value_x1000, res->ac_ir, res->quality_reason,
			vp.motion_bin, vp.led_pa);

		int log_ret = ppg_logger_log_vitals_measurement(&vp);
		if (log_ret) {
			LOG_ERR("Vitals CSV log failed: %d", log_ret);
		} else {
			LOG_DBG("Vitals CSV log complete, advancing health cycle");
		}
	}

	/* Advance health scheduler only after vitals CSV is flushed. */
	ppg_trigger_fire(PPG_TRIG_MEASUREMENT_COMPLETE);

	/* Free buffer after triggers fire (apps may still need data) */
	k_mutex_lock(&ppg_state.lock, K_FOREVER);
	ppg_state.finalizing = false;
	ppg_samples_free(&ppg_state.buffer);
	k_mutex_unlock(&ppg_state.lock);
}
