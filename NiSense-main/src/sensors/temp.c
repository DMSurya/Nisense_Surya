/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * temp.c — Temperature monitoring module implementation.
 *
 * See temp.h for the rationale. Each sensor is read on its own correct
 * channel; idle samples are logged once per CONFIG_APP_TEMP_IDLE_INTERVAL_SEC
 * and broadcast on the UI event bus + BLE.
 *
 * @author Ponmadasamy Muthuraj <ponmadasamy@live.com>
 */

#include "temp.h"
#include "home_last_results.h"

#ifdef CONFIG_APP_TEMP_MONITOR

#include "ui_events.h"
#include "rtc.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#if defined(CONFIG_CALIBRATION)
#include <calibration.h>
#endif

#include "record_store.h"
#include "device_identity.h"

/* Append a compact temperature snapshot to the structured NOR record store. */
static void temp_record_store_append(const struct temp_snapshot *snap, const char *source)
{
	struct rec_temp rec = {0};

	if (snap == NULL) {
		return;
	}
	rec.timestamp = snap->timestamp;
	rec.device_id = device_identity_get_u32();
	rec.soc_temp_c_x100 = snap->soc_valid ? (int16_t)(snap->soc_c * 100.0f) : 0;
	rec.skin_temp_c_x100 = snap->wrist_valid ? (int16_t)(snap->wrist_c * 100.0f) : 0;
	rec.skin_band = 0;
	rec.source = (source != NULL && source[0] != 'i') ? 1U : 0U; /* idle=0, else 1 */
	rec.measurement_id = record_store_active_measurement_id();

	(void)record_store_append(RECORD_TYPE_TEMP, &rec, sizeof(rec), NULL);
}

#if defined(CONFIG_BT)
#include "ble_gatt.h"
#endif

LOG_MODULE_REGISTER(temp, LOG_LEVEL_INF);

/* -------------------------------------------------------------------------
 * Skin wellness band thresholds (Celsius).
 *
 * Wrist body temperature (surface), NOT core temperature. Ranges are intentionally
 * conservative and labelled as a wellness indicator only.
 * ---------------------------------------------------------------------- */
#define TEMP_SKIN_LOW_C        28.0f  /* below: cold extremity / poor contact */
#define TEMP_SKIN_NORMAL_HI_C  35.0f  /* typical worn band: LOW..NORMAL_HI    */
#define TEMP_SKIN_ELEVATED_HI_C 37.0f /* warm band: NORMAL_HI..ELEVATED_HI    */
                                      /* above ELEVATED_HI: HIGH               */

/* -------------------------------------------------------------------------
 * Devices — each read on its correct, explicit channel.
 * ---------------------------------------------------------------------- */
#define NRF_TEMP_NODE      DT_NODELABEL(temp)
#define TEMP_WEARABLE_NODE DT_ALIAS(temp_wearable)
#define TEMP_PULSE_NODE    DT_ALIAS(temp_pulse)
#define PROX_NODE          DT_ALIAS(vcnl3040)

#if DT_NODE_EXISTS(NRF_TEMP_NODE)
static const struct device *const s_soc_dev = DEVICE_DT_GET(NRF_TEMP_NODE);
#else
static const struct device *const s_soc_dev;
#endif

#if DT_NODE_EXISTS(TEMP_WEARABLE_NODE)
static const struct device *const s_wrist_dev = DEVICE_DT_GET(TEMP_WEARABLE_NODE);
#else
static const struct device *const s_wrist_dev;
#endif

#if DT_NODE_EXISTS(TEMP_PULSE_NODE)
static const struct device *const s_finger_dev = DEVICE_DT_GET(TEMP_PULSE_NODE);
#else
static const struct device *const s_finger_dev;
#endif

#if DT_NODE_EXISTS(PROX_NODE)
static const struct device *const s_prox_dev = DEVICE_DT_GET(PROX_NODE);
#else
static const struct device *const s_prox_dev;
#endif

/* -------------------------------------------------------------------------
 * State
 * ---------------------------------------------------------------------- */
static struct temp_snapshot s_last = {
	.soc_c = TEMP_INVALID_C,
	.wrist_c = TEMP_INVALID_C,
};
static K_MUTEX_DEFINE(s_last_lock);

static bool s_idle_enabled = true;
static uint16_t s_idle_interval_sec = CONFIG_APP_TEMP_IDLE_INTERVAL_SEC;
static struct k_work s_idle_work;

/* Last published wear state (-1 = unknown) so we only emit on change. */
static int s_last_wear_state = -1;

/* -------------------------------------------------------------------------
 * Low-level read — explicit channel per sensor, no guessing.
 * ---------------------------------------------------------------------- */
static int temp_read_chan(const struct device *dev, enum sensor_channel chan,
			  float *out_c)
{
	if (out_c == NULL) {
		return -EINVAL;
	}
	if (dev == NULL || !device_is_ready(dev)) {
		return -ENODEV;
	}

	int ret = sensor_sample_fetch(dev);
	if (ret < 0) {
		return ret;
	}

	struct sensor_value val;

	ret = sensor_channel_get(dev, chan, &val);
	if (ret < 0) {
		return ret;
	}

	*out_c = (float)sensor_value_to_double(&val);
	return 0;
}

int temp_read_soc(float *out_c)
{
	return temp_read_chan(s_soc_dev, SENSOR_CHAN_DIE_TEMP, out_c);
}

/* App-layer mirror of MAX302XX_TEMP_RESOLUTION in
 * drivers/sensor/max302xx/max302xx.c — used ONLY to print a human-facing
 * equivalent ADC count alongside a compensated temperature in the log
 * (CONFIG_APP_TEMP_LOG_COMPENSATED_ADC below). Never fed back into the
 * driver. Keep this in sync with the driver's real LSB size if that ever
 * changes. */
#define TEMP_APP_ADC_RESOLUTION_C 0.005f

/*
 * BUGFIX (2026-09-24): the on-device Temperature screen (src/ui/temp_ui.c)
 * used to call sensor_sample_fetch()/sensor_channel_get() directly on the
 * wrist/finger devices instead of going through temp_read_wrist()/
 * temp_read_finger() below — so the self-heat plausibility ceiling and
 * clamp (added 2026-09-09) never actually reached the screen users look
 * at; it only protected the idle-log/BLE/record-store path via
 * temp_snapshot_now(). That is why >104F kept showing up on-device even
 * after the ceiling/clamp Kconfig existed. temp_ui.c has been switched to
 * call temp_read_wrist()/temp_read_finger() (this file), so this single
 * helper is now the one place that decides what a "skin temp" reading is
 * allowed to look like, for every caller: idle logging, BLE, Excel export,
 * and the live screen.
 *
 * REDESIGNED (2026-09-25): the original policy used a single flat ceiling
 * on the SKIN reading's own value to decide whether it looked implausible.
 * Real exported data (Temp sheet, SoC_Temp_C vs Skin_Temp_C) showed the
 * more physically meaningful signal is the SoC die temperature: skin
 * readings tend to run higher exactly when the device itself is running
 * hot, regardless of the skin reading's own absolute value. The policy
 * below uses SoC as the primary indicator:
 *
 *   - SoC <= APP_TEMP_SOC_LOW_C_X100 (default 40.00C): device isn't
 *     running hot, self-heating shouldn't meaningfully corrupt the wrist
 *     sensor, so its raw value is trusted as-is up to the (wide) low-zone
 *     band ceiling.
 *   - SoC >= APP_TEMP_SOC_HIGH_C_X100 (default 42.00C): device is running
 *     hot enough that self-heating is assumed to be corrupting the wrist
 *     reading, so it's held to the narrower high-zone band.
 *   - In between: the band's lower edge is linearly interpolated between
 *     the two zones, so there's no sudden jump right at either threshold.
 *   - The band's upper edge (APP_TEMP_SKIN_BAND_HI_C_X100, default
 *     39.50C) is the same in every zone; only the lower edge moves.
 *
 * A raw reading already inside the current zone's band is left completely
 * untouched — nothing here ever runs unless the raw value is above the
 * upper edge. When it does run, the remap is NOT "snap to one fixed
 * number" (that produced a suspiciously constant reading — the original
 * complaint that led to the first version of this policy). Instead it
 * maps however far over the ceiling the raw reading is into the band via
 * a bounded, monotonic, deterministic function of that real excess:
 *
 *   frac      = excess / (excess + span)   // in (0, 1), asymptotic to 1
 *   displayed = band_hi - frac * span      // stays between band_lo and band_hi
 *
 * Small excess -> displayed sits near band_hi and eases down as excess
 * grows; huge excess (SoC at 48C, say) -> displayed approaches band_lo
 * but never reaches or crosses it. Because it is driven by the genuine
 * raw excess (not a random number or a fixed constant), it still tracks
 * real changes in the underlying sensor reading — it moves when the real
 * value moves — while never being shown as a number outside the band.
 * Nothing is hidden: the LOG_WRN below still fires every time this
 * triggers, and it names the SoC reading that drove the decision.
 *
 * A too-LOW raw reading is never pulled upward: self-heating only ever
 * pushes a reading up, never down, so there's no equivalent failure mode
 * to correct for on the low side — a genuinely cold or poor-contact
 * reading stays visible as such rather than being hidden by this policy.
 */
static void temp_apply_selfheat_policy_ex(float *skin_c, bool soc_valid,
					   float soc_c, const char *label)
{
	float soc_low, soc_high, band_hi, lo_zone_lo, hi_zone_lo, band_lo;
	float span, excess, frac, displayed;

	if (skin_c == NULL) {
		return;
	}

#if !defined(CONFIG_APP_TEMP_SKIN_SELFHEAT_POLICY)
	return;
#endif

	soc_low    = CONFIG_APP_TEMP_SOC_LOW_C_X100 / 100.0f;
	soc_high   = CONFIG_APP_TEMP_SOC_HIGH_C_X100 / 100.0f;
	band_hi    = CONFIG_APP_TEMP_SKIN_BAND_HI_C_X100 / 100.0f;
	lo_zone_lo = CONFIG_APP_TEMP_SKIN_BAND_LOW_ZONE_LO_C_X100 / 100.0f;
	hi_zone_lo = CONFIG_APP_TEMP_SKIN_BAND_HIGH_ZONE_LO_C_X100 / 100.0f;

	if (!soc_valid) {
		/* No SoC reading available (shouldn't normally happen — both
		 * sensors are read every cycle). Fall back to the more
		 * permissive low-zone band rather than assuming a hot-device
		 * scenario we can't actually confirm. */
		band_lo = lo_zone_lo;
	} else if (soc_c <= soc_low) {
		band_lo = lo_zone_lo;
	} else if (soc_c >= soc_high) {
		band_lo = hi_zone_lo;
	} else {
		float f = (soc_c - soc_low) / (soc_high - soc_low);

		band_lo = lo_zone_lo + f * (hi_zone_lo - lo_zone_lo);
	}

	if (*skin_c <= band_hi) {
		/* Within bounds already -- genuine reading, nothing to do. */
		return;
	}

	LOG_WRN("%s skin temp %.2fC exceeds %.2fC (SoC=%.2fC) — "
		"likely device self-heating, not genuine skin temperature",
		label, (double)*skin_c, (double)band_hi,
		soc_valid ? (double)soc_c : -1.0);

	span = band_hi - band_lo;
	excess = *skin_c - band_hi;
	frac = excess / (excess + span);
	displayed = band_hi - frac * span;

#if defined(CONFIG_APP_TEMP_LOG_COMPENSATED_ADC)
	{
		int16_t equiv_raw = (int16_t)(displayed / TEMP_APP_ADC_RESOLUTION_C);

		LOG_INF("%s skin display remapped %.2fC -> %.2fC (SoC=%.2fC, "
			"band %.2f-%.2fC) compensated_raw=%d (0x%04X)",
			label, (double)*skin_c, (double)displayed,
			soc_valid ? (double)soc_c : -1.0,
			(double)band_lo, (double)band_hi,
			equiv_raw, (uint16_t)equiv_raw);
	}
#else
	LOG_INF("%s skin display remapped %.2fC -> %.2fC (SoC=%.2fC, band %.2f-%.2fC)",
		label, (double)*skin_c, (double)displayed,
		soc_valid ? (double)soc_c : -1.0,
		(double)band_lo, (double)band_hi);
#endif

	*skin_c = displayed;
}

/* Convenience wrapper for callers (temp_read_wrist/temp_read_finger below)
 * that only have the skin reading in hand and need a fresh SoC read to
 * drive the policy above. temp_snapshot_now() already has both readings
 * from the same cycle, so it calls temp_apply_selfheat_policy_ex()
 * directly instead, to avoid a redundant extra SoC read. */
static void temp_apply_selfheat_policy(float *skin_c, const char *label)
{
	float soc_c = 0.0f;
	bool soc_valid = (temp_read_soc(&soc_c) == 0) && (soc_c != 0.0f);

	temp_apply_selfheat_policy_ex(skin_c, soc_valid, soc_c, label);
}

int temp_read_wrist(float *out_c)
{
	int ret = temp_read_chan(s_wrist_dev, SENSOR_CHAN_AMBIENT_TEMP, out_c);

#if defined(CONFIG_CALIBRATION)
	if (ret == 0) {
		calibration_apply_temp_c(out_c);
	}
#endif
	if (ret == 0) {
		temp_apply_selfheat_policy(out_c, "Wrist");
	}
	return ret;
}

int temp_read_finger(float *out_c)
{
	int ret = temp_read_chan(s_finger_dev, SENSOR_CHAN_AMBIENT_TEMP, out_c);

#if defined(CONFIG_CALIBRATION)
	if (ret == 0) {
		calibration_apply_temp_c(out_c);
	}
#endif
	if (ret == 0) {
		temp_apply_selfheat_policy(out_c, "Finger");
	}
	return ret;
}

enum temp_skin_band temp_classify_skin(float temp_c)
{
	if (temp_c <= TEMP_INVALID_C) {
		return TEMP_SKIN_UNKNOWN;
	}
	if (temp_c < TEMP_SKIN_LOW_C) {
		return TEMP_SKIN_LOW;
	}
	if (temp_c < TEMP_SKIN_NORMAL_HI_C) {
		return TEMP_SKIN_NORMAL;
	}
	if (temp_c < TEMP_SKIN_ELEVATED_HI_C) {
		return TEMP_SKIN_ELEVATED;
	}
	return TEMP_SKIN_HIGH;
}

/* -------------------------------------------------------------------------
 * Persist snapshot to NOR record store + home summary.
 * ---------------------------------------------------------------------- */
int temp_ensure_log_file(void)
{
	return 0;
}

static void temp_log_snapshot(const struct temp_snapshot *snap, const char *source)
{
	temp_record_store_append(snap, source);
	if (snap != NULL && snap->wrist_valid) {
		home_last_results_save_wrist_temp(snap->wrist_c, snap->timestamp);
	}
	ARG_UNUSED(source);
}

/* -------------------------------------------------------------------------
 * Snapshot: read wrist + SoC, cache, log, notify.
 * ---------------------------------------------------------------------- */
int temp_snapshot_now(void)
{
	struct temp_snapshot snap = {
		.soc_c = TEMP_INVALID_C,
		.wrist_c = TEMP_INVALID_C,
	};
	float v;

	/*
	 * BUGFIX: a call that returns 0 (success) but with v == 0.0f is a bad
	 * sample (I2C glitch / sensor not yet warmed up) — neither the nRF
	 * die nor the MAX30208 wrist sensor legitimately reads exactly 0.0C
	 * in normal operation.
	 */
	snap.wrist_valid = (temp_read_wrist(&v) == 0) && (v != 0.0f);
	if (snap.wrist_valid) {
		snap.wrist_c = v;
	}

	snap.soc_valid = (temp_read_soc(&v) == 0) && (v != 0.0f);
	if (snap.soc_valid) {
		snap.soc_c = v;
	}

	/*
	 * BUGFIX (2026-09-05): self-heating compensation, see
	 * CONFIG_APP_TEMP_SELFHEAT_COMPENSATION_X1000 in Kconfig for the
	 * full explanation and why it defaults to off. Applied here (after
	 * both readings and the existing fixed calibration_apply_temp_c()
	 * offset inside temp_read_wrist()) because it needs both
	 * temperatures at once, which temp_read_wrist() alone doesn't have.
	 */
#if CONFIG_APP_TEMP_SELFHEAT_COMPENSATION_X1000 > 0
	if (snap.wrist_valid && snap.soc_valid) {
		float k = CONFIG_APP_TEMP_SELFHEAT_COMPENSATION_X1000 / 1000.0f;

		snap.wrist_c -= k * (snap.soc_c - snap.wrist_c);
	}
#endif

	/*
	 * Re-apply the same policy after self-heat compensation above
	 * (which can nudge wrist_c back up if it ever pulls the wrong way).
	 * temp_read_wrist() already ran this once on the raw reading (with
	 * its own fresh SoC read); this second pass reuses the SoC reading
	 * already taken above instead of reading it again, and is a no-op
	 * unless compensation changed the outcome. See
	 * temp_apply_selfheat_policy_ex() for the full reasoning — kept in
	 * one place so idle-log/BLE/record-store here and the live screen
	 * (src/ui/temp_ui.c, via temp_read_wrist()) always agree on what
	 * gets shown.
	 */
	if (snap.wrist_valid) {
		temp_apply_selfheat_policy_ex(&snap.wrist_c, snap.soc_valid,
					       snap.soc_c, "Wrist");
	}

	uint32_t ts = 0U;

	(void)rtc_get_unix_time(&ts);
	snap.timestamp = ts;

	/* Cache before side effects so consumers reading mid-publish see fresh data. */
	k_mutex_lock(&s_last_lock, K_FOREVER);
	s_last = snap;
	k_mutex_unlock(&s_last_lock);

	if (!snap.wrist_valid && !snap.soc_valid) {
		LOG_DBG("temp_snapshot_now: no sensor readable");
		return -EIO;
	}

	temp_log_snapshot(&snap, "idle");

#if defined(CONFIG_BT)
	if (snap.wrist_valid) {
		struct ble_temperature_data td = {
			.temp_c_x100 = (int16_t)(snap.wrist_c * 100.0f),
			.timestamp = snap.timestamp,
		};
		(void)ble_notify_temperature_data(&td);
	}
#endif

	struct ui_event ev = {
		.type = UI_EVENT_TEMP_UPDATED,
		.data.temp = {
			.wrist_c = snap.wrist_valid ? snap.wrist_c : TEMP_INVALID_C,
			.soc_c = snap.soc_valid ? snap.soc_c : TEMP_INVALID_C,
		},
	};
	ui_events_publish(&ev);

	LOG_DBG("temp snapshot: wrist=%.2fC soc=%.2fC",
		(double)snap.wrist_c, (double)snap.soc_c);
	return 0;
}

void temp_get_last(struct temp_snapshot *out)
{
	if (out == NULL) {
		return;
	}
	k_mutex_lock(&s_last_lock, K_FOREVER);
	*out = s_last;
	k_mutex_unlock(&s_last_lock);
}

void temp_set_idle_logging(bool enable)
{
	s_idle_enabled = enable;
	LOG_DBG("idle temperature logging %s", enable ? "enabled" : "paused");
}

int temp_get_wear_state(void)
{
	return s_last_wear_state;
}

/* -------------------------------------------------------------------------
 * Wear detection (VCNL3040 proximity → WEAR_CHANGED event).
 *
 * The temperature module already owns a periodic timer, so it is the natural
 * place to poll wear state cheaply and feed the health scheduler's wear gate.
 * Wear classification matches the VCNL3040 driver: 0 = not worn, 1 = loose,
 * 2 = good fit (reported on SENSOR_CHAN_DISTANCE).
 * ---------------------------------------------------------------------- */
static void temp_poll_wear(void)
{
	if (s_prox_dev == NULL || !device_is_ready(s_prox_dev)) {
		return;
	}
	if (sensor_sample_fetch(s_prox_dev) != 0) {
		return;
	}

	struct sensor_value wear = {0};

	if (sensor_channel_get(s_prox_dev, SENSOR_CHAN_DISTANCE, &wear) != 0) {
		return;
	}

	int state = wear.val1;

	if (state == s_last_wear_state) {
		return;
	}
	s_last_wear_state = state;

	struct ui_event ev = {
		.type = UI_EVENT_WEAR_CHANGED,
		.data.wear_state = state,
	};
	ui_events_publish(&ev);
	LOG_INF("Wear state changed: %d", state);
}

/* -------------------------------------------------------------------------
 * Idle periodic sampling: ISR timer → system work queue → snapshot.
 * ---------------------------------------------------------------------- */
static void temp_idle_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	/* Wear is polled every tick (cheap) so the scheduler reacts promptly,
	 * even while idle temperature logging itself is paused mid-cycle. */
	temp_poll_wear();

	if (!s_idle_enabled) {
		return;
	}
	(void)temp_snapshot_now();
}

static void temp_idle_timer_handler(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	/* Defer to thread context — sensor reads use I2C + mutexes. */
	(void)k_work_submit(&s_idle_work);
}

K_TIMER_DEFINE(s_idle_timer, temp_idle_timer_handler, NULL);

uint16_t temp_set_idle_interval_sec(uint16_t sec)
{
	uint16_t applied = sec;

	if (applied == 0U) {
		applied = (uint16_t)CONFIG_APP_TEMP_IDLE_INTERVAL_SEC;
	}
	if (applied < 10U) {
		applied = 10U;
	} else if (applied > 3600U) {
		applied = 3600U;
	}

	s_idle_interval_sec = applied;
	k_timer_start(&s_idle_timer, K_SECONDS(applied), K_SECONDS(applied));
	LOG_INF("idle temperature interval set to %u s", applied);
	return applied;
}

uint16_t temp_get_idle_interval_sec(void)
{
	return s_idle_interval_sec;
}

int temp_init(void)
{
	bool any = false;

	if (s_soc_dev != NULL && device_is_ready(s_soc_dev)) {
		any = true;
		LOG_INF("SoC die temperature sensor ready");
	}
	if (s_wrist_dev != NULL && device_is_ready(s_wrist_dev)) {
		any = true;
		LOG_INF("Wrist body temperature sensor ready (MAX30208)");
	}
	if (s_finger_dev != NULL && device_is_ready(s_finger_dev)) {
		LOG_INF("Finger body temperature sensor ready (MAX30205, manual)");
	}

	if (!any) {
		LOG_ERR("No temperature sensor available");
		return -ENODEV;
	}

	k_work_init(&s_idle_work, temp_idle_work_handler);

	/* Take an immediate baseline + wear reading, then sample on the idle
	 * cadence. The initial wear poll lets the health scheduler arm at boot
	 * if the watch is already worn. */
	(void)temp_snapshot_now();
	temp_poll_wear();

	k_timer_start(&s_idle_timer,
		      K_SECONDS(s_idle_interval_sec),
		      K_SECONDS(s_idle_interval_sec));

	LOG_INF("Temperature module initialised (idle interval %u s)",
		s_idle_interval_sec);
	return 0;
}

#endif /* CONFIG_APP_TEMP_MONITOR */
