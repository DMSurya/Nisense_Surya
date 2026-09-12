/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Application-level BLE integration module.
 * Contains measurement control, PMIC control, connection management, and the
 * periodic PMIC/proximity publish logic previously inlined in main.c.
 */

#include "ble.h"

#ifdef CONFIG_APP_FEATURE_BLE

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/drivers/charger.h>
#include <zephyr/devicetree.h>

#if defined(CONFIG_PPG_ALGO)
#include <zephyr/subsys/ppg_algo/ppg_algo.h>
#endif

#include "ble_gatt.h"
#include "rtc.h"
#include "ppg.h"
#include "glucose.h"
#include "ui_events.h"
#include "power_batt.h"
#include "power_mgr.h"
#if defined(CONFIG_APP_FEATURE_USB)
#endif

LOG_MODULE_REGISTER(app_ble, LOG_LEVEL_DBG);

/* Temperature sensor aliases — optional, compile out if not wired in DT */
#if DT_NODE_EXISTS(DT_ALIAS(temp_wearable))
static const struct device *ble_temp_wearable = DEVICE_DT_GET(DT_ALIAS(temp_wearable));
#else
static const struct device *ble_temp_wearable;
#endif
#if DT_NODE_EXISTS(DT_ALIAS(temp_pulse))
static const struct device *ble_temp_pulse = DEVICE_DT_GET(DT_ALIAS(temp_pulse));
#else
static const struct device *ble_temp_pulse;
#endif

#if defined(CONFIG_BLE_ACCEL_STREAMING)
/* Accel stream decimation: publish every 3rd call (~10 Hz when called at 25-33 Hz) */
#define BLE_ACCEL_DECIMATE 3U
static uint8_t ble_accel_phase;
static uint16_t ble_accel_seq;
#endif

/* sensor_all aggregate cache lives in ble_gatt.c (updated on each notify) */

/* -------------------------------------------------------------------------
 * Measurement status helpers
 * ---------------------------------------------------------------------- */

static void ble_send_meas_unavailable(uint8_t type)
{
	struct ble_meas_status ms = {
		.active           = 0,
		.type             = type,
		.percent_complete = 0,
		.quality          = 0,
		.samples_taken    = 0,
		.samples_target   = 0,
	};

	(void)ble_notify_meas_status(&ms);
}

static void ble_send_meas_status_now(uint8_t type)
{
	struct ble_meas_status ms = {
		.active           = 0,
		.type             = type,
		.percent_complete = 0,
		.quality          = 0,
		.samples_taken    = 0,
		.samples_target   = 0,
	};

	switch (type) {
	case BLE_MEAS_TYPE_HR:
	case BLE_MEAS_TYPE_SPO2:
	case BLE_MEAS_TYPE_VITALS:
#if defined(CONFIG_APP_FEATURE_PPG)
		ms.active = ppg_is_measurement_active() ? 1U : 0U;
		if (ms.active) {
			ms.samples_target = ppg_get_measurement_target_samples();
		}
#endif
		break;

	case BLE_MEAS_TYPE_GLUCOSE:
#if defined(CONFIG_GLUCOSE_SENSOR)
		ms.active = glucose_is_measuring() ? 1U : 0U;
#endif
		break;

	default:
		return;
	}

	(void)ble_notify_meas_status(&ms);
}

/* -------------------------------------------------------------------------
 * Measurement start
 * ---------------------------------------------------------------------- */

static int ble_start_measurement_by_type(uint8_t type, uint8_t flags)
{
	int ret = -ENOTSUP;
	const bool skip_prox = (flags & BLE_MEAS_FLAG_SKIP_PROX) != 0U;

	switch (type) {
	case BLE_MEAS_TYPE_HR:
	case BLE_MEAS_TYPE_SPO2:
	case BLE_MEAS_TYPE_VITALS:
#if defined(CONFIG_APP_FEATURE_PPG)
		if (ppg_is_measurement_active()) {
			ble_send_meas_status_now(type);
			return -EALREADY;
		}
		/* Match device UI long-press: start with proximity bypass. */
		ppg_set_proximity_bypass(skip_prox);
		ret = ppg_start_measurement(ble_gatt_get_ppg_sample_count());
		if (ret != 0 && skip_prox) {
			ppg_set_proximity_bypass(false);
		}
#else
		LOG_WRN("PPG measurement requested over BLE but PPG feature is disabled");
		ret = -ENOTSUP;
#endif
		break;

	case BLE_MEAS_TYPE_GLUCOSE:
#if defined(CONFIG_GLUCOSE_SENSOR)
		if (glucose_is_measuring()) {
			ble_send_meas_status_now(type);
			return -EALREADY;
		}
		ret = glucose_start_measurement_from_ui();
#else
		LOG_WRN("Glucose measurement requested over BLE but glucose sensor is disabled");
		ret = -ENOTSUP;
#endif
		break;

	default:
		LOG_WRN("Unknown BLE measurement type: %u", type);
		ret = -EINVAL;
		break;
	}

	return ret;
}

/* -------------------------------------------------------------------------
 * Deferred PPG stop work (disconnect path)
 * ---------------------------------------------------------------------- */

#if defined(CONFIG_APP_FEATURE_PPG)
static struct k_work_delayable ble_ppg_stop_work;
static bool ble_ppg_stop_work_inited;

static void ble_ppg_stop_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (ppg_is_measurement_active()) {
		LOG_INF("BLE disconnected: stopping active PPG measurement (deferred)");
		(void)ppg_stop_measurement();
	}
}
#endif

/* -------------------------------------------------------------------------
 * Connection callback
 * ---------------------------------------------------------------------- */

static void ble_connection_handler(bool connected)
{
	/* Broadcast link state so the UI agent can auto-navigate to the BLE
	 * screen on connect and restore the prior screen on disconnect. Runs in
	 * the BT RX thread (work-queue equivalent) — safe to publish. */
	struct ui_event ev = {
		.type = connected ? UI_EVENT_BLE_CONNECTED : UI_EVENT_BLE_DISCONNECTED,
	};
	ui_events_publish(&ev);

	if (!connected) {
#if defined(CONFIG_APP_FEATURE_PPG)
		if (!ble_ppg_stop_work_inited) {
			k_work_init_delayable(&ble_ppg_stop_work, ble_ppg_stop_work_handler);
			ble_ppg_stop_work_inited = true;
		}

		if (ppg_is_measurement_active()) {
			/* Keep BT callback quick; run sensor stop out-of-band. */
			(void)k_work_reschedule(&ble_ppg_stop_work, K_NO_WAIT);
		}
#endif
	}
}

/* -------------------------------------------------------------------------
 * Measurement control handler
 * ---------------------------------------------------------------------- */

#if defined(CONFIG_APP_FEATURE_PPG)
static void ble_log_ppg_state(const char *ctx, uint8_t cmd, uint8_t type)
{
	bool ppg_active = ppg_is_measurement_active();
#if defined(CONFIG_PPG_ALGO)
	bool backend_measuring = ppg_algo_is_measuring();
#else
	bool backend_measuring = ppg_active;
#endif

	LOG_INF("BLE meas %s: cmd=%u type=%u app=%u backend=%u target=%u",
		ctx,
		cmd,
		type,
		ppg_active ? 1U : 0U,
		backend_measuring ? 1U : 0U,
		(unsigned)ppg_get_measurement_target_samples());
}
#endif

static int ble_meas_ctrl_handler(uint8_t cmd, uint8_t type, uint8_t flags)
{
	int ret = -1;

#if defined(CONFIG_APP_FEATURE_PPG)
	if (cmd == BLE_MEAS_CMD_START || cmd == BLE_MEAS_CMD_STOP) {
		ble_log_ppg_state("pre", cmd, type);
	}
#endif

	switch (cmd) {
	case BLE_MEAS_CMD_START:
		ret = ble_start_measurement_by_type(type, flags);
		if (ret == 0) {
			LOG_INF("BLE measurement start accepted: type=%u flags=0x%02x",
				type, flags);
		} else if (ret == -EALREADY) {
			LOG_INF("BLE measurement already active: type=%u", type);
		} else if (ret == -EINVAL) {
			/* Unknown type — propagate as ATT error; no notification. */
			return -EINVAL;
		} else {
			LOG_ERR("BLE measurement start failed: type=%u ret=%d", type, ret);
			ble_send_meas_unavailable(type);
		}
		break;

	case BLE_MEAS_CMD_STOP:
		/* Some clients send STOP with no type byte (len=1), which is mapped
		 * to type=0 in GATT write parser. Treat this as a generic stop-any
		 * request and push idle state so the app can re-sync after reset. */
#if defined(CONFIG_APP_FEATURE_PPG)
		ppg_set_proximity_bypass(false);
#endif
		if (type == 0U) {
#if defined(CONFIG_APP_FEATURE_PPG)
			if (ppg_is_measurement_active()) {
				ret = ppg_stop_measurement();
				if (ret == 0) {
					LOG_INF("BLE generic stop accepted (type=0)");
				} else {
					LOG_ERR("BLE generic stop failed (type=0) ret=%d", ret);
				}
			} else {
				LOG_INF("BLE generic stop (type=0): already idle");
			}
			ble_send_meas_unavailable(BLE_MEAS_TYPE_VITALS);
#else
			LOG_WRN("PPG stop requested over BLE but PPG features are disabled");
#endif
			ble_send_meas_unavailable(BLE_MEAS_TYPE_GLUCOSE);
			break;
		}

		switch (type) {
		case BLE_MEAS_TYPE_HR:
		case BLE_MEAS_TYPE_SPO2:
		case BLE_MEAS_TYPE_VITALS:
#if defined(CONFIG_APP_FEATURE_PPG)
			ret = ppg_stop_measurement();
			if (ret == 0) {
				LOG_INF("BLE measurement stop accepted: type=%u", type);
			} else {
				LOG_ERR("BLE measurement stop failed: type=%u ret=%d", type, ret);
			}
#else
			LOG_WRN("PPG stop requested over BLE but PPG features are disabled");
			ble_send_meas_unavailable(type);
#endif
			break;

		case BLE_MEAS_TYPE_GLUCOSE:
#if defined(CONFIG_APP_FEATURE_GLUCOSE)
			if (glucose_is_measuring()) {
				ret = glucose_abort_measurement();
				if (ret == 0) {
					LOG_INF("BLE glucose stop accepted: type=%u", type);
				} else {
					LOG_ERR("BLE glucose stop failed: type=%u ret=%d", type, ret);
				}
			} else {
				LOG_INF("BLE glucose stop (type=%u): already idle", type);
			}
#else
			LOG_WRN("Glucose stop requested over BLE but glucose features are disabled");
#endif
			ble_send_meas_unavailable(type);
			break;

		default:
			LOG_WRN("Unknown BLE measurement type for stop: %u", type);
			return -EINVAL;
		}
		break;

	case BLE_MEAS_CMD_RESET_SYNC:
		/* App-requested reset/sync flow after reconnect:
		 * 1) stop any active measurement state in firmware
		 * 2) publish idle state for all measurement types */
#if defined(CONFIG_APP_FEATURE_PPG)
		if (ppg_is_measurement_active()) {
			ret = ppg_stop_measurement();
			if (ret == 0) {
				LOG_INF("BLE reset/sync: active PPG measurement stopped");
			} else {
				LOG_WRN("BLE reset/sync: stop measurement failed ret=%d", ret);
			}
		}
#endif
#if defined(CONFIG_APP_FEATURE_GLUCOSE)
		if (glucose_is_measuring()) {
			(void)glucose_abort_measurement();
			LOG_INF("BLE reset/sync: active glucose measurement aborted");
		}
#endif
		ble_send_meas_unavailable(BLE_MEAS_TYPE_VITALS);
		ble_send_meas_unavailable(BLE_MEAS_TYPE_GLUCOSE);
		LOG_INF("BLE reset/sync completed");
		break;

	default:
		LOG_WRN("Unknown BLE measurement command: %u", cmd);
		return -EINVAL;
	}

	return 0;
}

/* -------------------------------------------------------------------------
 * PMIC control handler
 * ---------------------------------------------------------------------- */

static const struct device *pmic_target_to_dev(uint8_t target)
{
	switch (target) {
	case BLE_PMIC_TARGET_BK1:
		return DEVICE_DT_GET_OR_NULL(DT_ALIAS(bk1));
	case BLE_PMIC_TARGET_BK2:
		return DEVICE_DT_GET_OR_NULL(DT_ALIAS(bk2));
	case BLE_PMIC_TARGET_BK3:
		return DEVICE_DT_GET_OR_NULL(DT_ALIAS(bk3));
	case BLE_PMIC_TARGET_BBOUT:
		return DEVICE_DT_GET_OR_NULL(DT_ALIAS(bbout));
	default:
		return NULL;
	}
}

static uint8_t charger_status_to_ble(uint32_t status)
{
	switch (status) {
	case CHARGER_STATUS_CHARGING:
		return 1;
	case CHARGER_STATUS_FULL:
		return 2;
	case CHARGER_STATUS_NOT_CHARGING:
	default:
		return 0;
	}
}

static void ble_publish_pmic_snapshot(void)
{
	const struct device *chg = DEVICE_DT_GET_OR_NULL(DT_ALIAS(charger0));
	const struct device *bk1 = DEVICE_DT_GET_OR_NULL(DT_ALIAS(bk1));
	const struct device *bk2 = DEVICE_DT_GET_OR_NULL(DT_ALIAS(bk2));
	const struct device *bk3 = DEVICE_DT_GET_OR_NULL(DT_ALIAS(bk3));
	const struct device *bbout = DEVICE_DT_GET_OR_NULL(DT_ALIAS(bbout));

	union charger_propval chg_val;
	int32_t uv = 0;
	struct power_batt_snapshot batt;

	struct ble_pmic_data pmic = {0};
	struct ble_pmic_ext_data ext = {0};

	if (power_mgr_get_snapshot(&batt) != 0) {
		return;
	}

	if (batt.pmic_vbat_uv > 0) {
		pmic.battery_mv = (uint16_t)(batt.pmic_vbat_uv / 1000);
	} else if (batt.fg_vcell_uv > 0) {
		pmic.battery_mv = (uint16_t)(batt.fg_vcell_uv / 1000);
	}

	{
		int soc = 0;

		if (power_mgr_get_soc(&soc) == 0) {
			pmic.soc_percent = (uint8_t)soc;
		}
	}

	pmic.current_ma = (int16_t)(batt.current_ua / 1000);

	if (batt.fg_ext.valid) {
		ext.battery_temp_c = batt.fg_ext.temp_c;
		ext.cycle_count = batt.fg_ext.cycle_count;
		ext.remaining_mah = batt.fg_ext.remaining_mah;
		ext.full_mah = batt.fg_ext.full_mah;
		ext.design_mah = batt.fg_ext.design_mah;
		ext.time_to_empty_min = batt.fg_ext.time_to_empty_min;
		ext.time_to_full_min = batt.fg_ext.time_to_full_min;
		ext.avg_current_ma = batt.fg_ext.avg_current_ma;
	}

	if (ext.design_mah == 0U) {
		ext.design_mah = POWER_BATT_DESIGN_CAPACITY_MAH;
	}
	if (ext.full_mah == 0U || ext.full_mah > ext.design_mah * 2U ||
	    ext.full_mah < ext.design_mah / 2U) {
		ext.full_mah = ext.design_mah;
	}
	if (ext.remaining_mah > ext.full_mah && ext.full_mah > 0U &&
	    pmic.soc_percent <= 100U) {
		ext.remaining_mah = (uint16_t)((uint32_t)ext.full_mah * pmic.soc_percent / 100U);
	}

	if (chg && device_is_ready(chg)) {
		if (batt.cell_state == POWER_BATT_CELL_ABSENT) {
			/* USB-only: charger FSM may look "charging" — report idle. */
			pmic.charger_status = 0;
		} else if (charger_get_prop(chg, CHARGER_PROP_STATUS, &chg_val) == 0) {
			pmic.charger_status = charger_status_to_ble(chg_val.status);
		}
		if (charger_get_prop(chg, CHARGER_PROP_CONSTANT_CHARGE_VOLTAGE_UV, &chg_val) == 0) {
			ext.charge_voltage_mv = (uint16_t)(chg_val.const_charge_voltage_uv / 1000);
		}
		if (charger_get_prop(chg, CHARGER_PROP_CONSTANT_CHARGE_CURRENT_UA, &chg_val) == 0) {
			ext.charge_current_ma = (uint16_t)(chg_val.const_charge_current_ua / 1000);
		}
	}

	if (bk1 && device_is_ready(bk1)) {
		ext.buck1_enabled = regulator_is_enabled(bk1) ? 1U : 0U;
		if (regulator_get_voltage(bk1, &uv) == 0) {
			pmic.buck1_mv = (uint16_t)(uv / 1000);
		}
	}

	if (bk2 && device_is_ready(bk2)) {
		ext.buck2_enabled = regulator_is_enabled(bk2) ? 1U : 0U;
		if (regulator_get_voltage(bk2, &uv) == 0) {
			pmic.buck2_mv = (uint16_t)(uv / 1000);
		}
	}

	if (bk3 && device_is_ready(bk3)) {
		ext.buck3_enabled = regulator_is_enabled(bk3) ? 1U : 0U;
		if (regulator_get_voltage(bk3, &uv) == 0) {
			ext.buck3_mv = (uint16_t)(uv / 1000);
		}
	}

	if (bbout && device_is_ready(bbout)) {
		ext.bbout_enabled = regulator_is_enabled(bbout) ? 1U : 0U;
		if (regulator_get_voltage(bbout, &uv) == 0) {
			ext.bbout_mv = (uint16_t)(uv / 1000);
		}
	}

	if (batt.pmic_vbat_uv > 0) {
		ext.vbat_mv = (uint16_t)(batt.pmic_vbat_uv / 1000);
	}
	if (batt.fg_vcell_uv > 0) {
		ext.vcell_mv = (uint16_t)(batt.fg_vcell_uv / 1000);
	}

	{
		uint8_t cell;

		if (batt.cell_state == POWER_BATT_CELL_PRESENT) {
			cell = BLE_PMIC_EXT_CELL_PRESENT;
		} else if (batt.cell_state == POWER_BATT_CELL_ABSENT) {
			cell = BLE_PMIC_EXT_CELL_ABSENT;
		} else {
			cell = BLE_PMIC_EXT_CELL_UNKNOWN;
		}

		ext.flags = 0U;
		if (batt.usb_online) {
			ext.flags |= BLE_PMIC_EXT_FLAG_USB_ONLINE;
		}
		if (batt.bat_good) {
			ext.flags |= BLE_PMIC_EXT_FLAG_BAT_GOOD;
		}
		if (batt.bat_reg_done) {
			ext.flags |= BLE_PMIC_EXT_FLAG_BAT_REG_DONE;
		}
		if (batt.sys_bat_lim) {
			ext.flags |= BLE_PMIC_EXT_FLAG_SYS_BAT_LIM;
		}
		ext.flags |= (uint8_t)((cell & 0x3U) << BLE_PMIC_EXT_CELL_SHIFT);
	}

	(void)ble_notify_pmic_data(&pmic);
	(void)ble_notify_pmic_ext_data(&ext);

	(void)ble_gatt_set_battery_level(pmic.soc_percent);
	ble_gatt_refresh_device_build();
}

/* Read one temperature sensor, returning Celsius * 100.  Tries AMBIENT_TEMP
 * first (external I2C sensor), then DIE_TEMP (nRF internal). */
static bool ble_read_temperature(const struct device *dev, int16_t *temp_c_x100)
{
	if (!dev || !device_is_ready(dev)) {
		return false;
	}
	if (sensor_sample_fetch(dev) < 0) {
		return false;
	}
	struct sensor_value v = {0};
	if (sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &v) < 0 &&
	    sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP, &v) < 0) {
		return false;
	}
	/* sensor_value: val1 = integer degC, val2 = microdegC (signed) */
	*temp_c_x100 = (int16_t)(v.val1 * 100 + v.val2 / 10000);
	return true;
}

static void ble_publish_temperature_snapshot(void)
{
	struct ble_temperature_data td = {0};
	uint32_t ts = 0U;

	rtc_get_unix_time(&ts);
	td.timestamp = ts;

	int16_t t = 0;
	if (ble_read_temperature(ble_temp_wearable, &t) ||
	    ble_read_temperature(ble_temp_pulse, &t)) {
		td.temp_c_x100 = t;
		(void)ble_notify_temperature_data(&td);
	}
}

static void ble_publish_sensor_all_snapshot(void)
{
	(void)ble_gatt_publish_sensor_all_snapshot();
}

#if defined(CONFIG_BLE_ACCEL_STREAMING)
static void ble_publish_accel_snapshot(void)
{
	ble_accel_phase++;
	if ((ble_accel_phase % BLE_ACCEL_DECIMATE) != 0U) {
		return;
	}

	const struct device *accel_dev =
		DEVICE_DT_GET_OR_NULL(DT_ALIAS(accel_sensor));

	if (!accel_dev || !device_is_ready(accel_dev)) {
		return;
	}

	struct sensor_value sv[3];
	if (sensor_sample_fetch_chan(accel_dev, SENSOR_CHAN_ACCEL_XYZ) < 0) {
		return;
	}
	sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_X, &sv[0]);
	sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_Y, &sv[1]);
	sensor_channel_get(accel_dev, SENSOR_CHAN_ACCEL_Z, &sv[2]);

	/* Convert m/s² to milli-g: 1 g = 9.80665 m/s² ≈ 9807 mm/s² */
	struct ble_accel_sample s = {
		.seq          = ble_accel_seq++,
		.x_mg         = (int16_t)(((int64_t)sv[0].val1 * 1000000LL +
					   (int64_t)sv[0].val2) / 9807LL),
		.y_mg         = (int16_t)(((int64_t)sv[1].val1 * 1000000LL +
					   (int64_t)sv[1].val2) / 9807LL),
		.z_mg         = (int16_t)(((int64_t)sv[2].val1 * 1000000LL +
					   (int64_t)sv[2].val2) / 9807LL),
		.timestamp_ms = k_uptime_get_32(),
	};

	(void)ble_notify_accel_sample(&s);
}
#endif /* CONFIG_BLE_ACCEL_STREAMING */

static void ble_pmic_ctrl_handler(uint8_t cmd, uint8_t target, uint16_t value)
{
	const struct device *dev = pmic_target_to_dev(target);

	if (!dev || !device_is_ready(dev)) {
		LOG_WRN("PMIC ctrl target %u unavailable", target);
		return;
	}

	switch (cmd) {
	case BLE_PMIC_CMD_ENABLE:
		(void)regulator_enable(dev);
		break;
	case BLE_PMIC_CMD_DISABLE:
		(void)regulator_disable(dev);
		break;
	case BLE_PMIC_CMD_SET_VOLTAGE:
		if (value < 600U || value > 5500U) {
			LOG_WRN("PMIC voltage out of range: %u mV", value);
			return;
		}
		(void)regulator_set_voltage(dev, (int32_t)value * 1000, (int32_t)value * 1000);
		break;
	default:
		LOG_WRN("Unknown PMIC ctrl cmd=%u", cmd);
		return;
	}

	/* Push immediate state after control command for app confirmation */
	ble_publish_pmic_snapshot();
}

/* -------------------------------------------------------------------------
 * Proximity snapshot
 * ---------------------------------------------------------------------- */

#define BLE_PROX_NODE DT_ALIAS(vcnl3040)

static void ble_publish_proximity_snapshot(void)
{
	struct ble_proximity_status prox = {0};
	uint32_t ts = 0U;
	const struct device *prox_dev = DEVICE_DT_GET_OR_NULL(BLE_PROX_NODE);

	rtc_get_unix_time(&ts);
	prox.timestamp = ts;

	if (!prox_dev || !device_is_ready(prox_dev)) {
		(void)ble_notify_proximity_status(&prox);
		return;
	}

	if (sensor_sample_fetch(prox_dev) == 0) {
		struct sensor_value wear = {0};
		struct sensor_value prox_val = {0};

		if (sensor_channel_get(prox_dev, SENSOR_CHAN_DISTANCE, &wear) == 0) {
			prox.wear_state = (wear.val1 > UINT8_MAX) ? UINT8_MAX : (uint8_t)wear.val1;
			prox.contact = (wear.val1 != 0) ? 1U : 0U;
		}

		if (sensor_channel_get(prox_dev, SENSOR_CHAN_PROX, &prox_val) == 0) {
			uint32_t raw = (prox_val.val1 < 0) ? 0U : (uint32_t)prox_val.val1;
			uint32_t filt = (prox_val.val2 < 0) ? 0U : (uint32_t)prox_val.val2;

			prox.proximity_raw = (raw > UINT16_MAX) ? UINT16_MAX : (uint16_t)raw;
			prox.proximity_filt = (filt > UINT16_MAX) ? UINT16_MAX : (uint16_t)filt;
		}
	}

	(void)ble_notify_proximity_status(&prox);
}

/* -------------------------------------------------------------------------
 * Public API
 * ---------------------------------------------------------------------- */

int app_ble_init(void)
{
	LOG_INF("Starting BLE GATT (async, name='%s')...", CONFIG_BT_DEVICE_NAME);

	int ret = ble_gatt_init();

	if (ret != 0) {
		LOG_ERR("BLE GATT init failed: %d", ret);
		return ret;
	}

	ble_gatt_register_meas_ctrl_cb(ble_meas_ctrl_handler);
	ble_gatt_register_pmic_ctrl_cb(ble_pmic_ctrl_handler);
	ble_gatt_register_connection_cb(ble_connection_handler);
	LOG_INF("BLE measurement/PMIC control callbacks registered");

	return 0;
}

void app_ble_tick(void)
{
	static uint32_t last_pmic_ms;
	static uint32_t last_temp_ms;
#if defined(CONFIG_BLE_ACCEL_STREAMING)
	static uint32_t last_accel_ms;
#endif
	uint32_t now_ms = k_uptime_get_32();

	if (ble_gatt_is_connected() && (now_ms - last_pmic_ms) >= power_mgr_poll_interval_ms()) {
		last_pmic_ms = now_ms;
		ble_publish_pmic_snapshot();
		ble_publish_proximity_snapshot();
	}

	if (ble_gatt_is_connected() && (now_ms - last_temp_ms) >= 5000U) {
		last_temp_ms = now_ms;
		ble_publish_temperature_snapshot();
		ble_publish_sensor_all_snapshot();
	}

#if defined(CONFIG_BLE_ACCEL_STREAMING)
	if (ble_gatt_is_connected() && (now_ms - last_accel_ms) >= 100U) {
		last_accel_ms = now_ms;
		ble_publish_accel_snapshot();
	}
#endif
}

#endif /* CONFIG_APP_FEATURE_BLE */
