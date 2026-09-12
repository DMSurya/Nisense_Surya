/* =============================================================================
 * BLE GATT Service Module - Implementation
 * =============================================================================
 * Comprehensive BLE GATT profile for wearable health monitor
 * 
 * Services implemented:
 *   1. Wearable Configuration Service - Device settings, RTC time sync
 *   2. Sensor Data Service - PMIC, temperature, SpO2, HR, glucose streaming
 *   3. WiFi Configuration Service - SSID/password/connect management
 * 
 * Security:
 *   - Passkey display pairing (DISPLAY capability)
 *   - Bonding enabled for persistent pairing
 *   - Encrypted connections for WiFi credentials
 * 
 * Target: nRF52840 (Raytac MDBT50Q-DB-40)
 * ============================================================================= */

#include "ble_gatt.h"
#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP)
#include "ble_model_transfer.h"
#endif
#if defined(CONFIG_RESOURCE) && defined(CONFIG_BT)
#include "ble_resource_transfer.h"
#endif
#include "ble_record_sync.h"
#include "power_mgr.h"
#include "ble_gatt_security.h"
#include "storage_config.h"
#include "config_manager.h"
#include "rtc.h"
#if defined(CONFIG_BT_CTS)
#include "ble_cts.h"
#endif
#include "device_prefs.h"
#include "device_identity.h"
#include "diag_monitor.h"
#include "git_version.h"
#include "record_store.h"
#include <zephyr/drivers/hwinfo.h>
#include <zephyr/devicetree.h>
#include "product_hw_variant.h"
#if defined(CONFIG_APP_FEATURE_CLOUD_TELEMETRY)
#include "cloud_telemetry.h"
#endif
#include "home_last_results.h"
#if defined(CONFIG_GLUCOSE_SENSOR)
#include "glucose.h"
#endif
#if defined(CONFIG_APP_FEATURE_PPG)
#include "ppg.h"
#endif
#if defined(CONFIG_APP_HEALTH_SCHED)
#include "health_sched.h"
#endif
#if defined(CONFIG_APP_TEMP_MONITOR)
#include "temp.h"
#endif
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/byteorder.h>
#include <string.h>
#include <errno.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/sys/atomic.h>
#if defined(CONFIG_WIFI_WEXX)
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#endif
#if defined(CONFIG_APP_FEATURE_WIFI) && defined(CONFIG_WIFI_WEXX)
#include "wifi_bulk_session.h"
#endif

LOG_MODULE_REGISTER(ble_gatt, LOG_LEVEL_INF);

/* =============================================================================
 * UUID Definitions
 * ============================================================================= */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshift-count-overflow"
#endif

/* Service UUIDs */
static struct bt_uuid_128 wearable_svc_uuid = BT_UUID_INIT_128(BT_UUID_WEARABLE_SVC_VAL);
static struct bt_uuid_128 sensor_svc_uuid = BT_UUID_INIT_128(BT_UUID_SENSOR_SVC_VAL);
static struct bt_uuid_128 wifi_svc_uuid = BT_UUID_INIT_128(BT_UUID_WIFI_SVC_VAL);

/* Wearable Config Service Characteristics */
static struct bt_uuid_128 device_name_chrc_uuid = BT_UUID_INIT_128(BT_UUID_DEVICE_NAME_CHRC_VAL);
static struct bt_uuid_128 rtc_trim_chrc_uuid = BT_UUID_INIT_128(BT_UUID_RTC_TRIM_CHRC_VAL);
static struct bt_uuid_128 battery_low_chrc_uuid = BT_UUID_INIT_128(BT_UUID_BATTERY_LOW_CHRC_VAL);
static struct bt_uuid_128 brightness_chrc_uuid = BT_UUID_INIT_128(BT_UUID_BRIGHTNESS_CHRC_VAL);
static struct bt_uuid_128 screen_timeout_chrc_uuid = BT_UUID_INIT_128(BT_UUID_SCREEN_TIMEOUT_CHRC_VAL);
static struct bt_uuid_128 batt_poll_chrc_uuid = BT_UUID_INIT_128(BT_UUID_BATT_POLL_CHRC_VAL);
static struct bt_uuid_128 batt_sleep_soc_chrc_uuid = BT_UUID_INIT_128(BT_UUID_BATT_SLEEP_SOC_CHRC_VAL);
static struct bt_uuid_128 volume_chrc_uuid = BT_UUID_INIT_128(BT_UUID_VOLUME_CHRC_VAL);
static struct bt_uuid_128 ppg_pref_chrc_uuid = BT_UUID_INIT_128(BT_UUID_PPG_PREF_CHRC_VAL);
static struct bt_uuid_128 security_profile_chrc_uuid = BT_UUID_INIT_128(BT_UUID_SECURITY_PROFILE_CHRC_VAL);
static struct bt_uuid_128 device_id_chrc_uuid = BT_UUID_INIT_128(BT_UUID_DEVICE_ID_CHRC_VAL);
static struct bt_uuid_128 pairing_status_chrc_uuid = BT_UUID_INIT_128(BT_UUID_PAIRING_STATUS_CHRC_VAL);
static struct bt_uuid_128 device_build_chrc_uuid = BT_UUID_INIT_128(BT_UUID_DEVICE_BUILD_CHRC_VAL);
static struct bt_uuid_128 sampling_config_chrc_uuid = BT_UUID_INIT_128(BT_UUID_SAMPLING_CONFIG_CHRC_VAL);
static struct bt_uuid_128 pmic_ctrl_chrc_uuid  = BT_UUID_INIT_128(BT_UUID_PMIC_CTRL_CHRC_VAL);
static struct bt_uuid_128 admin_ctrl_chrc_uuid = BT_UUID_INIT_128(BT_UUID_ADMIN_CTRL_CHRC_VAL);

/* Sensor Data Service Characteristics */
static struct bt_uuid_128 pmic_status_chrc_uuid = BT_UUID_INIT_128(BT_UUID_PMIC_STATUS_CHRC_VAL);
static struct bt_uuid_128 temperature_chrc_uuid = BT_UUID_INIT_128(BT_UUID_TEMPERATURE_CHRC_VAL);
static struct bt_uuid_128 vitals_chrc_uuid = BT_UUID_INIT_128(BT_UUID_VITALS_CHRC_VAL);
static struct bt_uuid_128 glucose_chrc_uuid = BT_UUID_INIT_128(BT_UUID_GLUCOSE_CHRC_VAL);
static struct bt_uuid_128 glucose_sample_chrc_uuid = BT_UUID_INIT_128(BT_UUID_GLUCOSE_SAMPLE_CHRC_VAL);
static struct bt_uuid_128 glucose_algo_chrc_uuid = BT_UUID_INIT_128(BT_UUID_GLUCOSE_ALGO_CHRC_VAL);
static struct bt_uuid_128 pmic_ext_chrc_uuid = BT_UUID_INIT_128(BT_UUID_PMIC_EXT_CHRC_VAL);
static struct bt_uuid_128 proximity_chrc_uuid = BT_UUID_INIT_128(BT_UUID_PROXIMITY_CHRC_VAL);
static struct bt_uuid_128 sensor_all_chrc_uuid = BT_UUID_INIT_128(BT_UUID_SENSOR_ALL_CHRC_VAL);

/* WiFi Config Service Characteristics */
static struct bt_uuid_128 wifi_enable_chrc_uuid = BT_UUID_INIT_128(BT_UUID_WIFI_ENABLE_CHRC_VAL);
static struct bt_uuid_128 wifi_ssid_chrc_uuid = BT_UUID_INIT_128(BT_UUID_WIFI_SSID_CHRC_VAL);
static struct bt_uuid_128 wifi_password_chrc_uuid = BT_UUID_INIT_128(BT_UUID_WIFI_PASSWORD_CHRC_VAL);
static struct bt_uuid_128 wifi_status_chrc_uuid = BT_UUID_INIT_128(BT_UUID_WIFI_STATUS_CHRC_VAL);
static struct bt_uuid_128 wifi_connect_chrc_uuid = BT_UUID_INIT_128(BT_UUID_WIFI_CONNECT_CHRC_VAL);
#if defined(CONFIG_APP_FEATURE_WIFI) && defined(CONFIG_WIFI_WEXX)
static struct bt_uuid_128 wifi_session_ctrl_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_WIFI_SESSION_CTRL_CHRC_VAL);
static struct bt_uuid_128 wifi_session_status_chrc_uuid =
	BT_UUID_INIT_128(BT_UUID_WIFI_SESSION_STATUS_CHRC_VAL);
#endif

/* Measurement Control / Status Characteristics */
static struct bt_uuid_128 meas_ctrl_chrc_uuid = BT_UUID_INIT_128(BT_UUID_MEAS_CTRL_CHRC_VAL);
static struct bt_uuid_128 meas_status_chrc_uuid = BT_UUID_INIT_128(BT_UUID_MEAS_STATUS_CHRC_VAL);
static struct bt_uuid_128 ppg_decimate_chrc_uuid = BT_UUID_INIT_128(BT_UUID_PPG_DECIMATE_CHRC_VAL);
#if defined(CONFIG_BLE_PPG_STREAMING)
static struct bt_uuid_128 ppg_stream_chrc_uuid = BT_UUID_INIT_128(BT_UUID_PPG_STREAM_CHRC_VAL);
#endif
#if defined(CONFIG_BLE_ACCEL_STREAMING)
static struct bt_uuid_128 accel_stream_chrc_uuid = BT_UUID_INIT_128(BT_UUID_ACCEL_STREAM_CHRC_VAL);
#endif

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

/* =============================================================================
 * Connection and Pairing State
 * ============================================================================= */

static struct bt_conn *current_conn;

/* Stack readiness for status-bar / UI: set in bt_ready_cb after adv starts. */
static bool ble_stack_ready;
static bool ble_stack_failed;
static enum ble_pairing_state pairing_state = BLE_PAIRING_IDLE;
static uint32_t current_passkey;
static bool passkey_confirmed_on_device;
static ble_connection_cb connection_callback;
static ble_pairing_cb pairing_callback;
static ble_meas_ctrl_cb meas_ctrl_callback;
static ble_pmic_ctrl_cb pmic_ctrl_callback;

static void sync_power_mgr_config(void);

/* =============================================================================
 * Characteristic Data Buffers
 * ============================================================================= */

/* Wearable Config Service */
static char device_name_buf[32] = "NiSense";
static char device_id_buf[32] = "UNKNOWN";
static struct ble_device_build_data device_build_buf;
static int32_t rtc_trim_buf = 0;        /* PPM range +/-127 (OSCTRIM) */
static uint16_t battery_low_buf = 3000; /* mV */
static uint8_t brightness_buf = 80;     /* 0=off, 1-100=on (GPIO backlight enable) */
static uint16_t screen_timeout_buf = 60;
static uint8_t batt_poll_buf = 10;
static uint8_t batt_sleep_soc_buf = 5;
static uint8_t volume_buf = 50;         /* 0-100% */
static uint8_t ppg_pref_buf = 0;        /* enum config_ppg_preference */
#if defined(CONFIG_BLE_SECURITY_PROFILE_SECURE)
static uint8_t security_profile_buf = BLE_SECURITY_PROFILE_SECURE;
#else
static uint8_t security_profile_buf = BLE_SECURITY_PROFILE_OPEN;
#endif

/* Cached fields for sensor_all (0xF1FF) aggregate notifications */
static struct ble_sensor_all_data sensor_all_cache;
static struct ble_glucose_algo_data glucose_algo_cache;
static bool glucose_algo_cache_valid;

/* WiFi Config Service */
static uint8_t wifi_enable_buf = 0;
static char wifi_ssid_buf[33] = "";
static char wifi_password_buf[65] = "";
static struct ble_wifi_status wifi_status_buf;
#if defined(CONFIG_APP_FEATURE_WIFI) && defined(CONFIG_WIFI_WEXX)
static struct wifi_bulk_status wifi_session_status_buf;
#endif

/* Measurement status + PPG stream */
static struct ble_meas_status meas_status_buf;
static struct ble_proximity_status proximity_status_buf;
static uint8_t ppg_decimate_buf = 1; /* default: stream every sample */

/* Default acquisition length: rate × algo buffer (Kconfig; 20 s if RESP). */
#define BLE_PPG_SAMPLE_COUNT_DEFAULT \
	((uint16_t)(CONFIG_PPG_ALGO_SAMPLE_RATE * CONFIG_PPG_ALGO_BUFFER_SECONDS))

#define BLE_PPG_SAMPLE_COUNT_MIN 50U
#define BLE_PPG_SAMPLE_COUNT_MAX 2000U

static struct ble_sampling_config sampling_config_buf = {
	.ppg_sample_count = BLE_PPG_SAMPLE_COUNT_DEFAULT,
	.glucose_num_samples = 80U,
	.glucose_delay_ms = 2000U,
	.flags = 0U,
	.auto_enabled = 1U,
	.schedule_interval_sec = 0U,
	.current_interval_sec = 600U, /* ladder default idx 1 */
	.temp_idle_interval_sec = 0U,
};

static void sampling_config_persist(void)
{
	struct app_config *cfg = config_manager_get_mutable();

	if (cfg == NULL) {
		return;
	}
	cfg->ppg.sample_count = sampling_config_buf.ppg_sample_count;
	cfg->ppg.glucose_num_samples = sampling_config_buf.glucose_num_samples;
	cfg->ppg.glucose_delay_ms = sampling_config_buf.glucose_delay_ms;
	cfg->ppg.flags = sampling_config_buf.flags;
	cfg->ppg.stream_decimate = ppg_decimate_buf;
	cfg->ppg.health_auto_enabled = sampling_config_buf.auto_enabled ? 1U : 0U;
	cfg->ppg.health_interval_sec = sampling_config_buf.schedule_interval_sec;
	cfg->ppg.temp_idle_interval_sec = sampling_config_buf.temp_idle_interval_sec;
	(void)config_manager_save();
}

static void sampling_config_apply_persisted(const struct app_config *cfg)
{
	if (cfg == NULL) {
		return;
	}

	if (cfg->ppg.sample_count != 0U) {
		sampling_config_buf.ppg_sample_count = cfg->ppg.sample_count;
	} else {
		sampling_config_buf.ppg_sample_count = BLE_PPG_SAMPLE_COUNT_DEFAULT;
	}

	if (cfg->ppg.glucose_num_samples != 0U) {
		sampling_config_buf.glucose_num_samples = cfg->ppg.glucose_num_samples;
	}
	if (cfg->ppg.glucose_delay_ms != 0U) {
		sampling_config_buf.glucose_delay_ms = cfg->ppg.glucose_delay_ms;
	}

	sampling_config_buf.flags = cfg->ppg.flags;

	if (cfg->ppg.stream_decimate >= 1U && cfg->ppg.stream_decimate <= 33U) {
		ppg_decimate_buf = cfg->ppg.stream_decimate;
	}

	sampling_config_buf.auto_enabled = cfg->ppg.health_auto_enabled ? 1U : 0U;
	sampling_config_buf.schedule_interval_sec = cfg->ppg.health_interval_sec;
	sampling_config_buf.temp_idle_interval_sec = cfg->ppg.temp_idle_interval_sec;

#if defined(CONFIG_GLUCOSE_SENSOR)
	if (cfg->ppg.glucose_num_samples != 0U && cfg->ppg.glucose_delay_ms != 0U) {
		(void)glucose_set_config(cfg->ppg.glucose_num_samples,
					 cfg->ppg.glucose_delay_ms);
	} else if (cfg->ppg.glucose_num_samples != 0U || cfg->ppg.glucose_delay_ms != 0U) {
		uint16_t ns = sampling_config_buf.glucose_num_samples;
		uint16_t delay = sampling_config_buf.glucose_delay_ms;

		(void)glucose_get_config(&ns, &delay);
		if (cfg->ppg.glucose_num_samples != 0U) {
			ns = cfg->ppg.glucose_num_samples;
		}
		if (cfg->ppg.glucose_delay_ms != 0U) {
			delay = cfg->ppg.glucose_delay_ms;
		}
		(void)glucose_set_config(ns, delay);
		sampling_config_buf.glucose_num_samples = ns;
		sampling_config_buf.glucose_delay_ms = delay;
	}
#endif

#if defined(CONFIG_APP_FEATURE_PPG)
	{
		bool disable_prox = (cfg->ppg.flags & BLE_SAMP_FLAG_DISABLE_PROX) != 0U;

		ppg_set_proximity_disabled(disable_prox);
#if defined(CONFIG_APP_HEALTH_SCHED)
		health_sched_notify_prox_policy_changed();
#endif
	}
#endif

#if defined(CONFIG_APP_HEALTH_SCHED)
	health_sched_set_auto_enabled(sampling_config_buf.auto_enabled != 0U);
	health_sched_set_interval_sec(sampling_config_buf.schedule_interval_sec);
	sampling_config_buf.current_interval_sec =
		(uint16_t)health_sched_current_interval_sec();
#endif

#if defined(CONFIG_APP_TEMP_MONITOR)
	sampling_config_buf.temp_idle_interval_sec =
		temp_set_idle_interval_sec(sampling_config_buf.temp_idle_interval_sec);
#endif

	LOG_INF("sampling restored: ppg=%u gluc=%u delay=%u flags=0x%02x decim=%u "
		"auto=%u interval=%u current=%u temp_idle=%u",
		sampling_config_buf.ppg_sample_count,
		sampling_config_buf.glucose_num_samples,
		sampling_config_buf.glucose_delay_ms,
		sampling_config_buf.flags,
		ppg_decimate_buf,
		sampling_config_buf.auto_enabled,
		sampling_config_buf.schedule_interval_sec,
		sampling_config_buf.current_interval_sec,
		sampling_config_buf.temp_idle_interval_sec);
}

/* SIG Battery Service (0x180F) — single-byte level, no CONFIG_BT_BAS (saves RAM). */
static uint8_t bas_battery_level;
static bool bas_notify_enabled;

/* Delayed advertising restart avoids transient -ENOMEM/-EBUSY failures right
 * after disconnection while controller/L2CAP resources are still draining. */
static struct k_work_delayable adv_restart_work;
static bool adv_restart_work_inited;

/* Run bond wipe off the LVGL thread to avoid re-entering the host stack. */
static struct k_work delete_bonds_work;
static bool delete_bonds_work_inited;

static void delete_bonds_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	LOG_INF("Deleting all BLE bonds (deferred)");

	if (current_conn) {
		int disc_err = bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		if (disc_err && disc_err != -ENOTCONN) {
			LOG_WRN("Disconnect before bond wipe failed: %d", disc_err);
		}
	}

	int ret = bt_unpair(BT_ID_DEFAULT, NULL);
	if (ret) {
		LOG_ERR("bt_unpair failed: %d", ret);
	} else {
		LOG_INF("All bonds deleted");
	}
}

/* === Notification subscription tracking ===
 * Updated in CCC callbacks; checked before every bt_gatt_notify() call to
 * avoid the attr-lookup + ECANCELED path when the client hasn't subscribed. */
enum {
	NOTIFY_ID_PMIC        = 0,
	NOTIFY_ID_TEMP        = 1,
	NOTIFY_ID_VITALS      = 2,
	NOTIFY_ID_GLUCOSE     = 4,
	NOTIFY_ID_GLUC_SAMP   = 5,
	NOTIFY_ID_GLUC_ALGO   = 6,
	NOTIFY_ID_PMIC_EXT    = 7,
	NOTIFY_ID_PROXIMITY   = 8,
	NOTIFY_ID_SENSOR_ALL  = 9,
	NOTIFY_ID_WIFI_STATUS = 10,
	NOTIFY_ID_MEAS_STATUS = 11,
	NOTIFY_ID_PPG_STREAM  = 12,
	NOTIFY_ID_ACCEL_STREAM = 13,
	NOTIFY_ID_WIFI_SESSION_STATUS = 14,
	NOTIFY_ID_COUNT       = 15,
};
static ATOMIC_DEFINE(notify_subscribed, NOTIFY_ID_COUNT);

/* === GATT attribute pointer cache for notify hot path ===
 * bt_gatt_find_by_uuid() does a linear scan of the entire GATT table on every
 * call — O(n) over ~50 attributes.  At 33 Hz PPG streaming that amounts to
 * ~1600 scans/sec.  Pre-resolving the attr pointers once in bt_ready_cb()
 * replaces the scan with a single array index on every notify call.
 *
 * The cache is populated after bt_enable() completes; service definitions are
 * registered at link time via BT_GATT_SERVICE_DEFINE so they are always
 * present when bt_ready_cb() runs. */
static const struct bt_gatt_attr *notify_attr_cache[NOTIFY_ID_COUNT];

static int notify_characteristic(uint8_t id, const void *data, size_t len);

/* UUID → NOTIFY_ID mapping (build-time constants). */
static const struct bt_uuid *const notify_uuid_map[NOTIFY_ID_COUNT] = {
	[NOTIFY_ID_PMIC]        = &pmic_status_chrc_uuid.uuid,
	[NOTIFY_ID_TEMP]        = &temperature_chrc_uuid.uuid,
	[NOTIFY_ID_VITALS]      = &vitals_chrc_uuid.uuid,
	/* index 3 intentionally unused (gap in NOTIFY_ID enum) */
	[NOTIFY_ID_GLUCOSE]     = &glucose_chrc_uuid.uuid,
	[NOTIFY_ID_GLUC_SAMP]   = &glucose_sample_chrc_uuid.uuid,
	[NOTIFY_ID_GLUC_ALGO]   = &glucose_algo_chrc_uuid.uuid,
	[NOTIFY_ID_PMIC_EXT]    = &pmic_ext_chrc_uuid.uuid,
	[NOTIFY_ID_PROXIMITY]   = &proximity_chrc_uuid.uuid,
	[NOTIFY_ID_SENSOR_ALL]  = &sensor_all_chrc_uuid.uuid,
	[NOTIFY_ID_WIFI_STATUS] = &wifi_status_chrc_uuid.uuid,
	[NOTIFY_ID_MEAS_STATUS] = &meas_status_chrc_uuid.uuid,
#if defined(CONFIG_BLE_PPG_STREAMING)
	[NOTIFY_ID_PPG_STREAM]  = &ppg_stream_chrc_uuid.uuid,
#endif
#if defined(CONFIG_BLE_ACCEL_STREAMING)
	[NOTIFY_ID_ACCEL_STREAM] = &accel_stream_chrc_uuid.uuid,
#endif
#if defined(CONFIG_APP_FEATURE_WIFI) && defined(CONFIG_WIFI_WEXX)
	[NOTIFY_ID_WIFI_SESSION_STATUS] = &wifi_session_status_chrc_uuid.uuid,
#endif
};

static void ble_gatt_cache_notify_attrs(void)
{
	for (int i = 0; i < NOTIFY_ID_COUNT; i++) {
		if (notify_uuid_map[i] == NULL) {
			continue;
		}
		notify_attr_cache[i] = bt_gatt_find_by_uuid(NULL, 0, notify_uuid_map[i]);
		if (!notify_attr_cache[i]) {
			LOG_WRN("GATT notify cache: ID %d attr not found", i);
		}
	}
	LOG_DBG("GATT notify attr cache populated (%d entries)", NOTIFY_ID_COUNT);
}

/* === Slow advertising after ADV_FAST_TIMEOUT_MS of no connection ===
 * Reduces power consumption while idle. Fast interval is restored each time
 * advertising is (re)started after a disconnect. */
#define ADV_FAST_TIMEOUT_MS  30000U
#define ADV_SLOW_INT_MIN     1600U  /* 1.0 s in 0.625 ms units */
#define ADV_SLOW_INT_MAX     1920U  /* 1.2 s in 0.625 ms units */
#define ADV_WATCHDOG_MS      5000U
static struct k_work_delayable adv_slow_work;
static struct k_work_delayable adv_watchdog_work;
static bool adv_is_slow;

/* === PPG BLE TX drop counter ===
 * Incremented when bt_gatt_notify() returns -ENOMEM (TX queue full at 33 Hz). */
static uint32_t ppg_drop_count;
#define PPG_STREAM_RUNTIME_DECIMATE_MAX 8U
static uint8_t  ppg_stream_runtime_decimate = 1U;
static uint8_t  ppg_stream_runtime_phase;
static uint16_t ppg_stream_enomem_streak;
static uint16_t ppg_stream_success_streak;

#define NISENSE_WATCH_BLE_NAME "NiSense Watch"
#define NISENSE_PULSE_BLE_NAME "NiSense Pulse"
/* Max AD value bytes for a single name field (31-byte PDU minus 2-byte header). */
#define BLE_ADV_NAME_FIELD_MAX 29U

static const uint8_t adv_flags[] = { BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR };

/* Primary ADV: flags + 128-bit service UUID (name goes in scan response — too long
 * to fit alongside UUID in one 31-byte PDU). */
static struct bt_data adv_payload[2];
/* Scan response: complete local name for "NiSense Watch" / "NiSense Pulse". */
static struct bt_data sd_payload[1];

static bool ble_name_is_generic(const char *name)
{
	if (name == NULL || name[0] == '\0') {
		return true;
	}

	return (strcmp(name, "HCM") == 0) ||
	       (strcmp(name, "HealthMonitor") == 0) ||
	       (strcmp(name, "WearableDevice") == 0) ||
	       (strcmp(name, CONFIG_BT_DEVICE_NAME) == 0);
}

static void ble_gatt_apply_product_name(void)
{
	const char *product;

	if (!ble_name_is_generic(device_name_buf)) {
		return;
	}

#if defined(CONFIG_GLUCOSE_SENSOR) || DT_HAS_ALIAS(max32664_hub)
	{
		enum product_hw_variant hw = product_hw_detect();

		product = (hw == PRODUCT_HW_WATCH) ? NISENSE_WATCH_BLE_NAME
						   : NISENSE_PULSE_BLE_NAME;
		LOG_INF("BLE product name from HW detect: %s (%s)",
			product, product_hw_variant_name(hw));
	}
#else
	product = NISENSE_WATCH_BLE_NAME;
	LOG_INF("BLE product name (default): %s", product);
#endif

	strncpy(device_name_buf, product, sizeof(device_name_buf) - 1);
	device_name_buf[sizeof(device_name_buf) - 1] = '\0';

	struct app_config *cfg = config_manager_get_mutable();

	if (cfg && ble_name_is_generic(cfg->device.name)) {
		strncpy(cfg->device.name, device_name_buf, sizeof(cfg->device.name) - 1);
		cfg->device.name[sizeof(cfg->device.name) - 1] = '\0';
		config_manager_save();
	}
}

static void ble_gatt_refresh_adv_payload(void)
{
	size_t name_len = strlen(device_name_buf);

	if (name_len > BLE_ADV_NAME_FIELD_MAX) {
		name_len = BLE_ADV_NAME_FIELD_MAX;
	}

	adv_payload[0].type = BT_DATA_FLAGS;
	adv_payload[0].data_len = sizeof(adv_flags);
	adv_payload[0].data = adv_flags;

	adv_payload[1].type = BT_DATA_UUID128_ALL;
	adv_payload[1].data_len = sizeof(sensor_svc_uuid.val);
	adv_payload[1].data = sensor_svc_uuid.val;

	sd_payload[0].type = BT_DATA_NAME_COMPLETE;
	sd_payload[0].data_len = name_len;
	sd_payload[0].data = device_name_buf;
}

static int ble_gatt_restart_advertising_if_idle(void)
{
	int stop_ret;

	if (current_conn) {
		return 0;
	}

	stop_ret = bt_le_adv_stop();
	if (stop_ret && stop_ret != -EALREADY) {
		LOG_WRN("adv refresh: stop failed (%d)", stop_ret);
	}

	return ble_gatt_start_advertising();
}

/* Slow-advertising work: switch from fast (~30 ms) to slow (~1 s) interval
 * once ADV_FAST_TIMEOUT_MS elapses without a connection, saving power. */
static void adv_slow_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (current_conn || adv_is_slow) {
		return;
	}

	int stop_ret = bt_le_adv_stop();
	if (stop_ret && stop_ret != -EALREADY) {
		LOG_WRN("adv_slow: stop failed (%d)", stop_ret);
	}

	struct bt_le_adv_param slow_param = BT_LE_ADV_PARAM_INIT(
		BT_LE_ADV_OPT_CONN,
		ADV_SLOW_INT_MIN,
		ADV_SLOW_INT_MAX,
		NULL);

	int ret = bt_le_adv_start(&slow_param, adv_payload, ARRAY_SIZE(adv_payload),
				  sd_payload, ARRAY_SIZE(sd_payload));
	if (!ret) {
		adv_is_slow = true;
		LOG_INF("Switched to slow BLE advertising (~1 s interval)");
	} else {
		LOG_WRN("adv_slow: start failed (%d), restoring fast advertising", ret);
		int fast_ret = ble_gatt_start_advertising();
		if (fast_ret && fast_ret != -EALREADY) {
			LOG_ERR("adv_slow: fast advertising restore failed (%d)", fast_ret);
		}
	}
}

static void adv_restart_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	int ret = ble_gatt_start_advertising();
	if (ret == -EALREADY) {
		return;
	}

	if (ret == -ENOMEM || ret == -EBUSY || ret == -EAGAIN) {
		LOG_WRN("Advertising restart deferred (ret=%d), retrying", ret);
		(void)k_work_reschedule(&adv_restart_work, K_MSEC(500));
		return;
	}

	if (ret) {
		LOG_ERR("Advertising restart failed: %d", ret);
	}
}

/* Advertising watchdog: when disconnected, periodically ensure connectable
 * advertising is active so transient controller states can't leave us silent. */
static void adv_watchdog_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	if (current_conn) {
		return;
	}

	int ret = ble_gatt_start_advertising();
	if (ret) {
		LOG_WRN("adv_watchdog: advertising ensure failed (%d)", ret);
	}

	(void)k_work_reschedule(&adv_watchdog_work, K_MSEC(ADV_WATCHDOG_MS));
}

/* =============================================================================
 * Wearable Config Service Callbacks
 * ============================================================================= */

/* Device Name (R/W) */
static ssize_t read_device_name(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 device_name_buf, strlen(device_name_buf));
}

static ssize_t write_device_name(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				 const void *buf, uint16_t len, uint16_t offset,
				 uint8_t flags)
{
	if (offset + len > sizeof(device_name_buf) - 1) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(device_name_buf + offset, buf, len);
	device_name_buf[offset + len] = '\0';

	/* Update BLE device name */
	bt_set_name(device_name_buf);
	ble_gatt_refresh_adv_payload();
	(void)ble_gatt_restart_advertising_if_idle();

	/* Persist to config */
	struct app_config *cfg = config_manager_get_mutable();
	if (cfg) {
		strncpy(cfg->device.name, device_name_buf, sizeof(cfg->device.name));
		config_manager_save();
	}

	LOG_INF("Device name updated via BLE: %s", device_name_buf);
	return len;
}

/* RTC Trim PPM (R/W) */
static ssize_t read_rtc_trim(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			     void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &rtc_trim_buf, sizeof(rtc_trim_buf));
}

static ssize_t write_rtc_trim(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			      const void *buf, uint16_t len, uint16_t offset,
			      uint8_t flags)
{
	if (len != sizeof(int32_t)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	int32_t trim_val;
	int hw_ret;
	struct device_config scfg;

	memcpy(&trim_val, buf, sizeof(trim_val));

	/* Match MCP7940N OSCTRIM magnitude (±127); reject before persist */
	if (trim_val < -127 || trim_val > 127) {
		LOG_WRN("RTC trim out of range: %d (allowed ±127)", trim_val);
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	hw_ret = rtc_set_trim_ppm(trim_val);
	if (hw_ret) {
		LOG_WRN("RTC trim hardware write failed: %d", hw_ret);
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	rtc_trim_buf = trim_val;
	LOG_INF("RTC trim set via BLE: %d PPM", trim_val);

	if (config_load(&scfg) == 0) {
		scfg.rtc_trim_ppm = trim_val;
		(void)config_save(&scfg);
	}

	return len;
}

/* Battery Low Threshold (R/W) */
static ssize_t read_battery_low(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &battery_low_buf, sizeof(battery_low_buf));
}

static ssize_t write_battery_low(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				 const void *buf, uint16_t len, uint16_t offset,
				 uint8_t flags)
{
	if (len != sizeof(uint16_t)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint16_t threshold;
	memcpy(&threshold, buf, sizeof(threshold));

	if (threshold < 2500 || threshold > 3500) {
		LOG_WRN("Battery threshold out of range: %d mV", threshold);
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	battery_low_buf = threshold;

	{
		struct device_config scfg;
		struct app_config *cfg = config_manager_get_mutable();

		if (config_load(&scfg) == 0) {
			scfg.battery_low_mv = threshold;
			(void)config_save(&scfg);
		}
		if (cfg) {
			cfg->device.batt_low_mv = threshold;
			config_manager_save();
		}
	}

	sync_power_mgr_config();

	LOG_INF("Battery low threshold set via BLE: %d mV", threshold);
	return len;
}

/* Brightness (R/W) */
static ssize_t read_brightness(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			       void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &brightness_buf, sizeof(brightness_buf));
}

static ssize_t write_brightness(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	if (len != sizeof(uint8_t)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t val = ((const uint8_t *)buf)[0];
	if (val > 100) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	brightness_buf = val;

	struct app_config *cfg = config_manager_get_mutable();
	if (cfg) {
		cfg->device.brightness = val;
		config_manager_save();
	}

	(void)device_prefs_apply_brightness(val);

	LOG_INF("Brightness set via BLE: %d (on/off enable)", val);
	return len;
}

static void sync_power_mgr_config(void)
{
	power_mgr_apply_config();
}

/* Screen idle timeout (R/W) — seconds */
static ssize_t read_screen_timeout(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				   void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &screen_timeout_buf, sizeof(screen_timeout_buf));
}

static ssize_t write_screen_timeout(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				    const void *buf, uint16_t len, uint16_t offset,
				    uint8_t flags)
{
	uint16_t val;

	if (len != sizeof(uint16_t)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	memcpy(&val, buf, sizeof(val));
	if (val < 10U || val > 3600U) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	screen_timeout_buf = val;
	struct app_config *cfg = config_manager_get_mutable();
	if (cfg) {
		cfg->device.sleep_timeout_s = val;
		config_manager_save();
	}
	sync_power_mgr_config();
	return len;
}

/* Battery poll interval (R/W) — seconds */
static ssize_t read_batt_poll(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			      void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &batt_poll_buf, sizeof(batt_poll_buf));
}

static ssize_t write_batt_poll(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			       const void *buf, uint16_t len, uint16_t offset,
			       uint8_t flags)
{
	if (len != sizeof(uint8_t)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	uint8_t val = ((const uint8_t *)buf)[0];
	if (val < 5U || val > 60U) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	batt_poll_buf = val;
	struct app_config *cfg = config_manager_get_mutable();
	if (cfg) {
		cfg->device.batt_poll_interval_s = val;
		config_manager_save();
	}
	sync_power_mgr_config();
	return len;
}

/* Critical battery SOC (R/W) — percent */
static ssize_t read_batt_sleep_soc(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				   void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &batt_sleep_soc_buf, sizeof(batt_sleep_soc_buf));
}

static ssize_t write_batt_sleep_soc(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				    const void *buf, uint16_t len, uint16_t offset,
				    uint8_t flags)
{
	if (len != sizeof(uint8_t)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}
	uint8_t val = ((const uint8_t *)buf)[0];
	if (val < 1U || val > 30U) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}
	batt_sleep_soc_buf = val;
	struct app_config *cfg = config_manager_get_mutable();
	if (cfg) {
		cfg->device.batt_sleep_soc_pct = val;
		config_manager_save();
	}
	sync_power_mgr_config();
	return len;
}

/* Volume (R/W) */
static ssize_t read_volume(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			   void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &volume_buf, sizeof(volume_buf));
}

static ssize_t write_volume(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			    const void *buf, uint16_t len, uint16_t offset,
			    uint8_t flags)
{
	if (len != sizeof(uint8_t)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t val = ((const uint8_t *)buf)[0];
	if (val > 100) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	volume_buf = val;

	struct app_config *cfg = config_manager_get_mutable();
	if (cfg) {
		cfg->device.volume = val;
		config_manager_save();
	}

	(void)device_prefs_apply_volume(val);

	LOG_INF("Volume set via BLE: %d%%", val);
	return len;
}

static ssize_t read_ppg_pref(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			     void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &ppg_pref_buf, sizeof(ppg_pref_buf));
}

static ssize_t write_ppg_pref(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			      const void *buf, uint16_t len, uint16_t offset,
			      uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(offset);
	ARG_UNUSED(flags);

	if (len != sizeof(uint8_t)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t pref = ((const uint8_t *)buf)[0];
	if (pref > (uint8_t)CONFIG_PPG_PREF_MAX3010X) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	ppg_pref_buf = pref;

	/* Persist to config (optional key: omitted when UNSET) */
	struct app_config *cfg = config_manager_get_mutable();
	if (cfg) {
		cfg->ppg.preference = (enum config_ppg_preference)pref;
		(void)config_manager_save();
	}

	LOG_INF("PPG preference updated via BLE: %u", pref);
	return len;
}

static ssize_t read_security_profile(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				     void *buf, uint16_t len, uint16_t offset)
{
	ARG_UNUSED(attr);

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &security_profile_buf, sizeof(security_profile_buf));
}

/* Hardware Device ID (Read-only) — hwinfo hex, same as NAND CSV Device_ID */
static ssize_t read_device_id(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			      void *buf, uint16_t len, uint16_t offset)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);

	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 device_id_buf, strlen(device_id_buf));
}

static void device_build_fill(struct ble_device_build_data *out)
{
	struct record_store_stats st;
	uint8_t id[16];
	int n;

	memset(out, 0, sizeof(*out));
	strncpy(out->fw_version, FW_VERSION_STRING, sizeof(out->fw_version) - 1U);
	strncpy(out->git_hash, GIT_HASH, sizeof(out->git_hash) - 1U);
	out->uptime_s = (uint32_t)(k_uptime_get() / 1000);
	record_store_get_stats(&st);
	out->pending_records = st.pending;
	out->pending_glucose = st.pending_glucose;
	out->pending_vitals = st.pending_vitals;
	out->pending_temp = st.pending_temp;
	out->pending_ppg_raw = st.pending_ppg_raw;
	out->pending_glucose_raw = st.pending_glucose_raw;
	out->dropped = st.dropped;
	out->crc_fail_count = st.crc_fail_count;

	n = hwinfo_get_device_id(id, sizeof(id));
	if (n > 0) {
		size_t copy = (n < 8) ? (size_t)n : 8U;

		memcpy(out->device_id, id, copy);
	}
}

void ble_gatt_refresh_device_build(void)
{
	device_build_fill(&device_build_buf);
}

static ssize_t read_device_build(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				void *buf, uint16_t len, uint16_t offset)
{
	ARG_UNUSED(attr);

	device_build_fill(&device_build_buf);
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &device_build_buf, sizeof(device_build_buf));
}

/* Pairing status (Read-only) — lets the central poll watch-side SMP progress */
static ssize_t read_pairing_status(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				   void *buf, uint16_t len, uint16_t offset)
{
	uint8_t data[5];

	ARG_UNUSED(attr);

	data[0] = (uint8_t)pairing_state;
	sys_put_le32(current_passkey, &data[1]);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, data, sizeof(data));
}

static ssize_t write_admin_ctrl(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len, uint16_t offset,
				uint8_t flags)
{
	ARG_UNUSED(attr);
	ARG_UNUSED(offset);
	ARG_UNUSED(flags);

	if (len < 1U) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t cmd = ((const uint8_t *)buf)[0];

	switch (cmd) {
	case ADMIN_CMD_DELETE_BONDS:
		LOG_INF("Admin: delete bonds requested by peer");
		/* Delete bond for the requesting peer specifically.
		 * bt_unpair with NULL address clears ALL bonds for this identity. */
		bt_unpair(BT_ID_DEFAULT, bt_conn_get_dst(conn));
		/* Disconnect cleanly; the peer should also clear its OS-level bond. */
		bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		break;
	case ADMIN_CMD_MEM_DUMP:
		LOG_INF("Admin: memory dump requested by peer");
		diag_monitor_dump_now();
		break;
	default:
		LOG_WRN("Admin: unknown command 0x%02x", cmd);
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	return len;
}

static ssize_t write_pmic_ctrl(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			       const void *buf, uint16_t len, uint16_t offset,
			       uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(offset);
	ARG_UNUSED(flags);

	if (len != 2U && len != 4U) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t cmd = ((const uint8_t *)buf)[0];
	uint8_t target = ((const uint8_t *)buf)[1];
	uint16_t value = 0U;

	if (len == 4U) {
		value = sys_get_le16(&((const uint8_t *)buf)[2]);
	}

	LOG_INF("PMIC ctrl via BLE: cmd=%u target=%u value=%u", cmd, target, value);

	if (pmic_ctrl_callback) {
		pmic_ctrl_callback(cmd, target, value);
	}

	return len;
}

/* =============================================================================
 * Sensor Data Service - CCC Callbacks (Notify enable/disable)
 * ============================================================================= */

static void ble_seed_fill_gaps_from_vitals_record(struct ble_vitals_data *v)
{
	struct rec_vitals rec;
	size_t len = 0;

	if (record_store_read_latest_type(RECORD_TYPE_VITALS, NULL, &rec, &len) != 0 ||
	    len < sizeof(rec)) {
		return;
	}

	if (v->timestamp == 0U || rec.timestamp >= v->timestamp) {
		v->timestamp = rec.timestamp;
	}
	if (v->hr_bpm == 0U && rec.hr_bpm > 0U) {
		v->hr_bpm = rec.hr_bpm;
		v->hr_confidence = rec.hr_conf;
		v->flags |= BIT(0);
	}
	if (v->spo2_percent == 0U && rec.spo2_percent > 0U) {
		v->spo2_percent = rec.spo2_percent;
		v->spo2_confidence = rec.spo2_conf;
		v->flags |= BIT(1);
	}
	if (v->hb_g_dl_x10 == 0U && rec.hb_g_dl_x10 > 0U) {
		v->hb_g_dl_x10 = rec.hb_g_dl_x10;
		v->hb_confidence = rec.hb_conf > 0U ? rec.hb_conf : 50U;
		v->flags |= BIT(2);
	}
	if (v->resp_rate_bpm == 0U && rec.resp_rate_bpm > 0U) {
		v->resp_rate_bpm = rec.resp_rate_bpm;
		v->resp_confidence = rec.resp_conf;
		v->flags |= BIT(3);
	}
	if (v->sdnn_ms == 0U && rec.sdnn_ms > 0U) {
		v->sdnn_ms = rec.sdnn_ms;
		v->rmssd_ms = rec.rmssd_ms;
		v->flags |= BIT(5);
	}
	if (v->systolic_mmhg == 0U && rec.systolic_mmhg > 0U) {
		v->systolic_mmhg = rec.systolic_mmhg;
		v->diastolic_mmhg = rec.diastolic_mmhg;
		v->flags |= BIT(6);
	}

	LOG_INF("BLE seed overlay vitals record: Hb=%u BP=%u/%u",
		rec.hb_g_dl_x10, rec.systolic_mmhg, rec.diastolic_mmhg);
}

static void ble_seed_fill_from_glucose_record(void)
{
	struct rec_glucose rec;
	size_t len = 0;

	if (record_store_read_latest_type(RECORD_TYPE_GLUCOSE, NULL, &rec, &len) != 0 ||
	    len < sizeof(rec)) {
		return;
	}

	if (sensor_all_cache.glucose.glucose_mg_dl == 0U && rec.glucose_mg_dl > 0U) {
		sensor_all_cache.glucose.glucose_mg_dl = rec.glucose_mg_dl;
		sensor_all_cache.glucose.quality = rec.quality;
		sensor_all_cache.glucose.timestamp = rec.timestamp;
	}

	if (rec.actual_insulin > 0.0f || rec.homa_ir_index > 0.0f) {
		memset(&glucose_algo_cache, 0, sizeof(glucose_algo_cache));
		glucose_algo_cache.tot_coeff = rec.tot_coeff;
		glucose_algo_cache.intercept = rec.intercept;
		glucose_algo_cache.y1_value = rec.y1_value;
		glucose_algo_cache.avg_val = rec.avg_val;
		glucose_algo_cache.std_dev = rec.std_dev;
		glucose_algo_cache.up_lim = rec.up_lim;
		glucose_algo_cache.ll_lim = rec.ll_lim;
		glucose_algo_cache.p_count = rec.p_count;
		glucose_algo_cache.n_count = rec.n_count;
		glucose_algo_cache.p_val = rec.p_val;
		glucose_algo_cache.n_val = rec.n_val;
		glucose_algo_cache.p_plus_n = rec.p_plus_n;
		glucose_algo_cache.y2_val = rec.y2_val;
		glucose_algo_cache.y2_percent = rec.y2_percent;
		glucose_algo_cache.group_cd = rec.group_cd;
		glucose_algo_cache.y2_factor = rec.y2_factor;
		glucose_algo_cache.y2_factor_val = rec.y2_factor_val;
		glucose_algo_cache.const_val = rec.const_val;
		glucose_algo_cache.y3_value = rec.y3_value;
		glucose_algo_cache.y3_row_no = rec.y3_row_no;
		glucose_algo_cache.elim_per = rec.elim_per;
		glucose_algo_cache.elim_val = rec.elim_val;
		glucose_algo_cache.y_value = rec.y_value;
		glucose_algo_cache.calibration_factor = rec.calibration_factor;
		glucose_algo_cache.ag_adjusted = rec.ag_adjusted;
		glucose_algo_cache.normalized_glucose = rec.normalized_glucose;
		glucose_algo_cache.actual_insulin = rec.actual_insulin;
		glucose_algo_cache.insulin_correction = rec.insulin_correction;
		glucose_algo_cache.insulin_ratio = rec.insulin_ratio;
		glucose_algo_cache.inverse_ratio = rec.inverse_ratio;
		glucose_algo_cache.homa_ir_index = rec.homa_ir_index;
		glucose_algo_cache_valid = true;

		LOG_INF("BLE seed overlay glucose record: insulin=%.2f homa=%.2f",
			(double)rec.actual_insulin, (double)rec.homa_ir_index);
	}
}

#if defined(CONFIG_GLUCOSE_SENSOR)
static void ble_seed_insulin_homa_fallback(void)
{
	if (glucose_algo_cache_valid &&
	    (glucose_algo_cache.actual_insulin > 0.0f ||
	     glucose_algo_cache.homa_ir_index > 0.0f)) {
		return;
	}

	const uint16_t gluc = sensor_all_cache.glucose.glucose_mg_dl;

	if (gluc == 0U) {
		return;
	}

	float fasting = 0.0f;

	if (glucose_get_fasting_insulin(&fasting) != 0 || fasting <= 0.0f) {
		return;
	}

	/* Match glucose driver: Insulin card shows fasting insulin; classic
	 * HOMA-IR = (glucose_mg/dL × insulin_μU/mL) / 405.
	 */
	if (!glucose_algo_cache_valid) {
		memset(&glucose_algo_cache, 0, sizeof(glucose_algo_cache));
	}
	glucose_algo_cache.actual_insulin = fasting;
	glucose_algo_cache.homa_ir_index =
		((float)gluc * fasting) / 405.0f;
	glucose_algo_cache_valid = true;

	LOG_INF("BLE seed insulin/HOMA fallback: insulin=%.2f homa=%.2f (gluc=%u)",
		(double)fasting, (double)glucose_algo_cache.homa_ir_index, gluc);
}
#endif

void ble_gatt_seed_from_last_results(void)
{
	home_last_results_load();

	const struct home_last_results *lr = home_last_results_get();
	struct ble_vitals_data *v = &sensor_all_cache.vitals;

	memset(v, 0, sizeof(*v));

	if (lr->loaded) {
		v->timestamp = lr->timestamp;

		/*
		 * Match device UI last-result merge (vitals_ui): show when value > 0,
		 * not only when the validity flag is set.
		 */
		if (lr->hr_bpm > 0U) {
			v->hr_bpm = lr->hr_bpm;
			v->hr_confidence = lr->hr_valid ? 80U : 50U;
			v->flags |= BIT(0);
		}
		if (lr->spo2_percent > 0U) {
			v->spo2_percent = lr->spo2_percent;
			v->spo2_confidence = lr->spo2_valid ? 80U : 50U;
			v->flags |= BIT(1);
		}
		if (lr->hb_g_dl_x10 > 0U) {
			v->hb_g_dl_x10 = lr->hb_g_dl_x10;
			v->hb_confidence = lr->hb_valid ? 80U : 50U;
			v->flags |= BIT(2);
		}
		if (lr->resp_rate_bpm > 0U) {
			v->resp_rate_bpm = lr->resp_rate_bpm;
			v->resp_confidence = lr->resp_valid ? 80U : 50U;
			v->flags |= BIT(3);
		}
		if (lr->sdnn_ms > 0U || lr->rmssd_ms > 0U) {
			v->sdnn_ms = lr->sdnn_ms;
			v->rmssd_ms = lr->rmssd_ms;
			v->flags |= BIT(5);
		}
		if (lr->systolic_mmhg > 0U) {
			v->systolic_mmhg = lr->systolic_mmhg;
			v->diastolic_mmhg = lr->diastolic_mmhg;
			v->flags |= BIT(6);
		}

		if (lr->wrist_valid && lr->wrist_c > -100.0f) {
			sensor_all_cache.temperature.temp_c_x100 =
				(int16_t)(lr->wrist_c * 100.0f);
			sensor_all_cache.temperature.timestamp = lr->timestamp;
		}

		if (lr->glucose_mg_dl > 0U) {
			sensor_all_cache.glucose.glucose_mg_dl = lr->glucose_mg_dl;
			sensor_all_cache.glucose.timestamp = lr->timestamp;
		}

		if (lr->insulin_uiu_ml > 0.0f || lr->homa_ir_index > 0.0f ||
		    lr->insulin_valid || lr->homa_valid) {
			memset(&glucose_algo_cache, 0, sizeof(glucose_algo_cache));
			if (lr->insulin_uiu_ml > 0.0f || lr->insulin_valid) {
				glucose_algo_cache.actual_insulin = lr->insulin_uiu_ml;
			}
			if (lr->homa_ir_index > 0.0f || lr->homa_valid) {
				glucose_algo_cache.homa_ir_index = lr->homa_ir_index;
			}
			glucose_algo_cache_valid = true;
		}
	}

	/* home/last omits BP unless bp_valid; Hb/insulin may also be missing.
	 * Latest summary records are the authoritative last-reading source.
	 */
	ble_seed_fill_gaps_from_vitals_record(v);
	ble_seed_fill_from_glucose_record();
#if defined(CONFIG_GLUCOSE_SENSOR)
	ble_seed_insulin_homa_fallback();
#endif

	LOG_INF("BLE cache seeded: HR=%u SpO2=%u Hb=%u Resp=%u SDNN=%u BP=%u/%u "
		"gluc=%u insulin=%.2f homa=%.2f flags=0x%02x",
		v->hr_bpm, v->spo2_percent, v->hb_g_dl_x10, v->resp_rate_bpm,
		v->sdnn_ms, v->systolic_mmhg, v->diastolic_mmhg,
		sensor_all_cache.glucose.glucose_mg_dl,
		(double)glucose_algo_cache.actual_insulin,
		(double)glucose_algo_cache.homa_ir_index, v->flags);
}

static struct k_work_delayable seed_cache_work;
static bool seed_cache_work_inited;

static struct k_work_delayable snapshot_push_work;
static bool snapshot_push_work_inited;
static ATOMIC_DEFINE(snapshot_push_pending, NOTIFY_ID_COUNT);

static void ble_gatt_schedule_snapshot_push(int notify_id);

static void seed_cache_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	ble_gatt_seed_from_last_results();

	/*
	 * CCC snapshot push (~750 ms) often runs before this 3 s seed. Re-push
	 * seeded last-home values so already-subscribed clients update.
	 */
	static const int seed_ids[] = {
		NOTIFY_ID_TEMP,
		NOTIFY_ID_VITALS,
		NOTIFY_ID_GLUCOSE,
		NOTIFY_ID_GLUC_ALGO,
		NOTIFY_ID_SENSOR_ALL,
	};

	for (size_t i = 0; i < ARRAY_SIZE(seed_ids); i++) {
		if (atomic_test_bit(notify_subscribed, seed_ids[i])) {
			ble_gatt_schedule_snapshot_push(seed_ids[i]);
		}
	}
}

static void ble_gatt_seed_from_last_results_async(void)
{
	if (!seed_cache_work_inited) {
		k_work_init_delayable(&seed_cache_work, seed_cache_work_handler);
		seed_cache_work_inited = true;
	}

	/*
	 * Defer well past CCC storm + MTU/security. FatFS/QSPI during that
	 * window freezes LVGL (touch) and can stall ATT (no RTT progress).
	 */
	(void)k_work_reschedule(&seed_cache_work, K_MSEC(3000));
}

static void ble_gatt_push_cached_snapshot(int notify_id)
{
	switch (notify_id) {
	case NOTIFY_ID_TEMP:
		(void)notify_characteristic(NOTIFY_ID_TEMP,
					    &sensor_all_cache.temperature,
					    sizeof(sensor_all_cache.temperature));
		break;
	case NOTIFY_ID_VITALS:
		(void)notify_characteristic(NOTIFY_ID_VITALS,
					    &sensor_all_cache.vitals,
					    sizeof(sensor_all_cache.vitals));
		break;
	case NOTIFY_ID_GLUCOSE:
		(void)notify_characteristic(NOTIFY_ID_GLUCOSE,
					    &sensor_all_cache.glucose,
					    sizeof(sensor_all_cache.glucose));
		break;
	case NOTIFY_ID_GLUC_ALGO:
		if (glucose_algo_cache_valid) {
			(void)notify_characteristic(NOTIFY_ID_GLUC_ALGO,
						    &glucose_algo_cache,
						    sizeof(glucose_algo_cache));
		}
		break;
	case NOTIFY_ID_SENSOR_ALL:
		(void)notify_characteristic(NOTIFY_ID_SENSOR_ALL,
					    &sensor_all_cache,
					    sizeof(sensor_all_cache));
		break;
	default:
		break;
	}
}

static void snapshot_push_work_handler(struct k_work *work)
{
	ARG_UNUSED(work);

	for (int id = 0; id < NOTIFY_ID_COUNT; id++) {
		if (atomic_test_and_clear_bit(snapshot_push_pending, id)) {
			ble_gatt_push_cached_snapshot(id);
		}
	}
}

static void ble_gatt_schedule_snapshot_push(int notify_id)
{
	atomic_set_bit(snapshot_push_pending, notify_id);
	if (!snapshot_push_work_inited) {
		k_work_init_delayable(&snapshot_push_work, snapshot_push_work_handler);
		snapshot_push_work_inited = true;
	}
	/* Coalesce many CCC enables into one push after ATT settles. */
	(void)k_work_reschedule(&snapshot_push_work, K_MSEC(750));
}

static void ccc_notify_changed(int notify_id, const char *name, uint16_t value)
{
	if (value == BT_GATT_CCC_NOTIFY) {
		atomic_set_bit(notify_subscribed, notify_id);
		/* Never bt_gatt_notify() from CCC write context — stalls ATT. */
		ble_gatt_schedule_snapshot_push(notify_id);
	} else {
		atomic_clear_bit(notify_subscribed, notify_id);
	}
	LOG_INF("%s notifications %s", name, value ? "enabled" : "disabled");
}

/* Helper macro: update subscription bit and log */
#define CCC_CHANGED(id, name) \
	static void name##_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) \
	{ \
		if (value == BT_GATT_CCC_NOTIFY) { \
			atomic_set_bit(notify_subscribed, id); \
		} else { \
			atomic_clear_bit(notify_subscribed, id); \
		} \
		LOG_INF(#name " notifications %s", value ? "enabled" : "disabled"); \
	}

CCC_CHANGED(NOTIFY_ID_PMIC,        pmic)
static void temperature_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	ccc_notify_changed(NOTIFY_ID_TEMP, "temperature", value);
}
static void vitals_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	ccc_notify_changed(NOTIFY_ID_VITALS, "vitals", value);
}
static void glucose_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	ccc_notify_changed(NOTIFY_ID_GLUCOSE, "glucose", value);
}
CCC_CHANGED(NOTIFY_ID_GLUC_SAMP,   glucose_sample)
static void glucose_algo_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	ccc_notify_changed(NOTIFY_ID_GLUC_ALGO, "glucose_algo", value);
}
CCC_CHANGED(NOTIFY_ID_PMIC_EXT,    pmic_ext)
CCC_CHANGED(NOTIFY_ID_PROXIMITY,   proximity)
static void sensor_all_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	ccc_notify_changed(NOTIFY_ID_SENSOR_ALL, "sensor_all", value);
}
/* =============================================================================
 * WiFi Config Service Callbacks
 * ============================================================================= */

/* WiFi Enable (R/W) */
static ssize_t read_wifi_enable(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				void *buf, uint16_t len, uint16_t offset)
{
	const struct config_wifi *wifi = config_get_wifi();
	wifi_enable_buf = wifi ? wifi->enabled : 0;
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &wifi_enable_buf, sizeof(wifi_enable_buf));
}

static ssize_t write_wifi_enable(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				 const void *buf, uint16_t len, uint16_t offset,
				 uint8_t flags)
{
	if (len != sizeof(uint8_t)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	wifi_enable_buf = ((const uint8_t *)buf)[0] ? 1 : 0;

	struct app_config *cfg = config_manager_get_mutable();
	if (cfg) {
		cfg->wifi.enabled = wifi_enable_buf;
		config_manager_save();
	}

	LOG_INF("WiFi %s via BLE", wifi_enable_buf ? "enabled" : "disabled");
	return len;
}

/* WiFi SSID (R/W) */
static ssize_t read_wifi_ssid(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			      void *buf, uint16_t len, uint16_t offset)
{
	const struct config_wifi *wifi = config_get_wifi();
	if (wifi) {
		strncpy(wifi_ssid_buf, wifi->ssid, sizeof(wifi_ssid_buf));
	}
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 wifi_ssid_buf, strlen(wifi_ssid_buf));
}

static ssize_t write_wifi_ssid(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			       const void *buf, uint16_t len, uint16_t offset,
			       uint8_t flags)
{
	if (offset + len > sizeof(wifi_ssid_buf) - 1) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(wifi_ssid_buf + offset, buf, len);
	wifi_ssid_buf[offset + len] = '\0';

	struct app_config *cfg = config_manager_get_mutable();
	if (cfg) {
		strncpy(cfg->wifi.ssid, wifi_ssid_buf, sizeof(cfg->wifi.ssid));
		config_manager_save();
	}

	LOG_INF("WiFi SSID set via BLE: %s", wifi_ssid_buf);
	return len;
}

/* WiFi Password (W only - encrypted connection required) */
static ssize_t write_wifi_password(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				   const void *buf, uint16_t len, uint16_t offset,
				   uint8_t flags)
{
	if (offset + len > sizeof(wifi_password_buf) - 1) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(wifi_password_buf + offset, buf, len);
	wifi_password_buf[offset + len] = '\0';

	struct app_config *cfg = config_manager_get_mutable();
	if (cfg) {
		strncpy(cfg->wifi.password, wifi_password_buf, sizeof(cfg->wifi.password));
		config_manager_save();
	}

	LOG_INF("WiFi password updated via BLE");
	return len;
}

/* WiFi Status (R/Notify) */
static ssize_t read_wifi_status(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &wifi_status_buf, sizeof(wifi_status_buf));
}

static void wifi_status_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	if (value == BT_GATT_CCC_NOTIFY) {
		atomic_set_bit(notify_subscribed, NOTIFY_ID_WIFI_STATUS);
	} else {
		atomic_clear_bit(notify_subscribed, NOTIFY_ID_WIFI_STATUS);
	}
	LOG_INF("WiFi status notifications %s", value ? "enabled" : "disabled");
}

/* WiFi Connect (W) - Trigger connect/disconnect */
static ssize_t write_wifi_connect(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				  const void *buf, uint16_t len, uint16_t offset,
				  uint8_t flags)
{
	if (len != sizeof(uint8_t)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t connect = ((const uint8_t *)buf)[0];
	LOG_INF("WiFi %s requested via BLE", connect ? "connect" : "disconnect");

#if defined(CONFIG_WIFI_WEXX)
	const struct device *wifi_dev = DEVICE_DT_GET_OR_NULL(DT_ALIAS(wifi0));

	if (!wifi_dev) {
		LOG_WRN("WiFi device not found");
		return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
	}

	{
		struct ble_wifi_status ws = {0};

		if (connect) {
			ws.connected = 1; /* connecting */
			size_t slen = MIN(strlen(wifi_ssid_buf), sizeof(ws.ssid) - 1U);
			memcpy(ws.ssid, wifi_ssid_buf, slen);
		}
		(void)ble_notify_wifi_status(&ws);
	}

	if (connect) {
		struct net_if *iface = net_if_lookup_by_dev(wifi_dev);
		if (!iface) {
			LOG_ERR("WiFi network interface not found");
			return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
		}
		struct wifi_connect_req_params params = {
			.ssid = (uint8_t *)wifi_ssid_buf,
			.ssid_length = (uint8_t)strlen(wifi_ssid_buf),
			.psk = (uint8_t *)wifi_password_buf,
			.psk_length = (uint8_t)strlen(wifi_password_buf),
			.security = (wifi_password_buf[0] == '\0') ?
				    WIFI_SECURITY_TYPE_NONE : WIFI_SECURITY_TYPE_PSK,
			.channel = WIFI_CHANNEL_ANY,
			.timeout = SYS_FOREVER_MS
		};
		int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
				   &params, sizeof(params));
		if (ret) {
			LOG_WRN("WiFi connect request failed: %d", ret);
		}
	} else {
		struct net_if *iface = net_if_lookup_by_dev(wifi_dev);
		if (iface) {
			net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
		}
		{
			struct ble_wifi_status ws = {0};

			ws.connected = 0;
			(void)ble_notify_wifi_status(&ws);
		}
	}
#else
	LOG_WRN("WiFi not enabled in this build");
#endif

	return len;
}

#if defined(CONFIG_APP_FEATURE_WIFI) && defined(CONFIG_WIFI_WEXX)
/* WiFi Bulk Session Control (W)
 * START: [cmd][mode][flags_le16][port_le16][after_id_le32][token_len][host_len][token][host]
 * ACK:   [cmd][up_to_id_le32]
 * ABORT: [cmd]
 */
static ssize_t write_wifi_session_ctrl(struct bt_conn *conn,
				       const struct bt_gatt_attr *attr,
				       const void *buf, uint16_t len,
				       uint16_t offset, uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);

	const uint8_t *p = buf;

	if (offset != 0U || len < 1U) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	switch (p[0]) {
	case WIFI_BULK_CMD_START:
	{
		struct wifi_bulk_start_params params = {0};
		uint8_t token_len;
		uint8_t host_len;
		uint16_t min_len;
		int rc;

		if (len < 12U) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}

		params.mode = p[1];
		params.flags = sys_get_le16(&p[2]);
		params.port = sys_get_le16(&p[4]);
		params.after_id = sys_get_le32(&p[6]);
		token_len = p[10];
		host_len = p[11];
		if (token_len == 0U || token_len >= WIFI_BULK_TOKEN_MAX ||
		    host_len == 0U || host_len >= WIFI_BULK_HOST_MAX) {
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}

		min_len = (uint16_t)(12U + token_len + host_len);
		if (len < min_len) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}

		memcpy(params.token, &p[12], token_len);
		params.token[token_len] = '\0';
		memcpy(params.host, &p[12 + token_len], host_len);
		params.host[host_len] = '\0';

		rc = wifi_bulk_session_start(&params);
		if (rc == -EBUSY) {
			return BT_GATT_ERR(BT_ATT_ERR_WRITE_REQ_REJECTED);
		}
		if (rc != 0) {
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}
		LOG_INF("WiFi bulk START host=%s port=%u after=%u mode=%u flags=0x%04x",
			params.host, params.port, params.after_id, params.mode,
			params.flags);
		break;
	}
	case WIFI_BULK_CMD_ACK:
	{
		uint32_t up_to_id;
		uint8_t ack_mode;
		int rc;

		if (len < 5U) {
			return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
		}
		up_to_id = sys_get_le32(&p[1]);
		/* Optional mode byte; 0 / omitted → session mode / FULL. */
		ack_mode = (len >= 6U) ? p[5] : 0U;
		rc = wifi_bulk_session_ack(up_to_id, ack_mode);
		if (rc != 0) {
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}
		LOG_INF("WiFi bulk ACK up_to=%u mode=%u", up_to_id, ack_mode);
		break;
	}
	case WIFI_BULK_CMD_ABORT:
		wifi_bulk_session_abort();
		LOG_INF("WiFi bulk ABORT");
		break;
	default:
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	return len;
}

static ssize_t read_wifi_session_status(struct bt_conn *conn,
					const struct bt_gatt_attr *attr,
					void *buf, uint16_t len,
					uint16_t offset)
{
	wifi_bulk_session_get_status(&wifi_session_status_buf);
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &wifi_session_status_buf,
				 sizeof(wifi_session_status_buf));
}

static void wifi_session_status_ccc_changed(const struct bt_gatt_attr *attr,
					    uint16_t value)
{
	ARG_UNUSED(attr);
	if (value == BT_GATT_CCC_NOTIFY) {
		atomic_set_bit(notify_subscribed, NOTIFY_ID_WIFI_SESSION_STATUS);
	} else {
		atomic_clear_bit(notify_subscribed, NOTIFY_ID_WIFI_SESSION_STATUS);
	}
	LOG_INF("WiFi session status notifications %s", value ? "enabled" : "disabled");
}

static void wifi_bulk_status_changed(const struct wifi_bulk_status *status)
{
	if (status == NULL) {
		return;
	}
	memcpy(&wifi_session_status_buf, status, sizeof(wifi_session_status_buf));
	if (atomic_test_bit(notify_subscribed, NOTIFY_ID_WIFI_SESSION_STATUS)) {
		(void)notify_characteristic(NOTIFY_ID_WIFI_SESSION_STATUS,
					    &wifi_session_status_buf,
					    sizeof(wifi_session_status_buf));
	}
}
#endif /* CONFIG_APP_FEATURE_WIFI && CONFIG_WIFI_WEXX */

/* =============================================================================
 * Measurement Control / Status / PPG Decimation Callbacks
 * ============================================================================= */

static ssize_t write_meas_ctrl(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			       const void *buf, uint16_t len, uint16_t offset,
			       uint8_t flags)
{
	ARG_UNUSED(flags);

	if (len < 1 || len > 3) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t cmd       = ((const uint8_t *)buf)[0];
	uint8_t type      = (len >= 2) ? ((const uint8_t *)buf)[1] : 0;
	uint8_t meas_flags = (len >= 3) ? ((const uint8_t *)buf)[2] : 0;

	LOG_INF("Meas ctrl via BLE: cmd=%u type=%u flags=0x%02x", cmd, type, meas_flags);

	if (meas_ctrl_callback) {
		int cb_ret = meas_ctrl_callback(cmd, type, meas_flags);
		if (cb_ret == -EINVAL) {
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}
	}

	return len;
}

static void meas_status_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	if (value == BT_GATT_CCC_NOTIFY) {
		atomic_set_bit(notify_subscribed, NOTIFY_ID_MEAS_STATUS);
		/* Push current state immediately so clients can resync on subscribe. */
		(void)ble_notify_meas_status(&meas_status_buf);
	} else {
		atomic_clear_bit(notify_subscribed, NOTIFY_ID_MEAS_STATUS);
	}
	LOG_INF("Meas status notifications %s", value ? "enabled" : "disabled");
}

static ssize_t read_ppg_decimate(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				 void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &ppg_decimate_buf, sizeof(ppg_decimate_buf));
}

static void sampling_config_refresh_from_drivers(void)
{
#if defined(CONFIG_GLUCOSE_SENSOR)
	uint16_t ns = 0;
	uint16_t delay = 0;

	if (glucose_get_config(&ns, &delay) == 0) {
		if (ns > 0U) {
			sampling_config_buf.glucose_num_samples = ns;
		}
		if (delay > 0U) {
			sampling_config_buf.glucose_delay_ms = delay;
		}
	}
#endif
	if (sampling_config_buf.ppg_sample_count == 0U) {
		sampling_config_buf.ppg_sample_count = BLE_PPG_SAMPLE_COUNT_DEFAULT;
	}
#if defined(CONFIG_APP_FEATURE_PPG)
	if (ppg_get_proximity_disabled()) {
		sampling_config_buf.flags |= BLE_SAMP_FLAG_DISABLE_PROX;
	} else {
		sampling_config_buf.flags &= (uint8_t)~BLE_SAMP_FLAG_DISABLE_PROX;
	}
#endif
#if defined(CONFIG_APP_HEALTH_SCHED)
	sampling_config_buf.auto_enabled = health_sched_auto_enabled() ? 1U : 0U;
	sampling_config_buf.schedule_interval_sec =
		(uint16_t)health_sched_get_configured_interval_sec();
	sampling_config_buf.current_interval_sec =
		(uint16_t)health_sched_current_interval_sec();
#endif
#if defined(CONFIG_APP_TEMP_MONITOR)
	sampling_config_buf.temp_idle_interval_sec = temp_get_idle_interval_sec();
#endif
}

static ssize_t read_sampling_config(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				    void *buf, uint16_t len, uint16_t offset)
{
	sampling_config_refresh_from_drivers();
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &sampling_config_buf, sizeof(sampling_config_buf));
}

static ssize_t write_sampling_config(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				     const void *buf, uint16_t len, uint16_t offset,
				     uint8_t flags)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);

	const struct ble_sampling_config *in = buf;
	uint16_t ppg_count;
	uint16_t gluc_ns;
	uint16_t gluc_delay;
	uint16_t sched_sec;
	uint16_t temp_idle_sec;
	uint8_t auto_en;

	/* 10 B = legacy; 12 B = schedule+current; 14 B = +temp idle. */
	if (offset != 0U ||
	    (len != sizeof(struct ble_sampling_config) && len != 12U && len != 10U)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	ppg_count = sys_le16_to_cpu(in->ppg_sample_count);
	gluc_ns = sys_le16_to_cpu(in->glucose_num_samples);
	gluc_delay = sys_le16_to_cpu(in->glucose_delay_ms);
	sched_sec = sys_le16_to_cpu(in->schedule_interval_sec);
	auto_en = in->auto_enabled ? 1U : 0U;
	temp_idle_sec = (len >= 14U) ? sys_le16_to_cpu(in->temp_idle_interval_sec)
				     : sampling_config_buf.temp_idle_interval_sec;

	if (ppg_count == 0U) {
		ppg_count = BLE_PPG_SAMPLE_COUNT_DEFAULT;
	} else if (ppg_count < BLE_PPG_SAMPLE_COUNT_MIN ||
		   ppg_count > BLE_PPG_SAMPLE_COUNT_MAX) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	if (gluc_ns == 0U) {
		gluc_ns = sampling_config_buf.glucose_num_samples;
	} else if (gluc_ns != 80U) {
		/*
		 * BUGFIX: this used to allow [10,500] here, matching what
		 * glucose_sensor_configure() used to accept — but that
		 * driver-level check was tightened to require exactly 80
		 * (the glucose algorithm is hard-fixed to 80 raw ADC
		 * samples; anything else silently produced
		 * GLUCOSE_SENSOR_FLAG_ALGO_ERROR / glucose_raw=0 — see
		 * F-FW-03 in CHANGES_2026-08-26.md). This BLE-layer check
		 * was left at the old, wider range, so a write of anything
		 * in [10,500] except 80 would pass validation *here*, get
		 * written into sampling_config_buf below, and then fail
		 * deep inside glucose_set_config() -> BT_ATT_ERR_UNLIKELY —
		 * while the characteristic's in-RAM copy had already been
		 * updated to the rejected value, leaving the read-back view
		 * and the sensor's actual config disagreeing. Reject it
		 * immediately, at the same boundary the driver now enforces,
		 * instead of accepting it here and failing one layer down.
		 */
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	if (gluc_delay == 0U) {
		gluc_delay = sampling_config_buf.glucose_delay_ms;
	} else if (gluc_delay < 100U || gluc_delay > 10000U) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	if (sched_sec != 0U && (sched_sec < 60U || sched_sec > 3600U)) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	sampling_config_buf.ppg_sample_count = ppg_count;
	sampling_config_buf.glucose_num_samples = gluc_ns;
	sampling_config_buf.glucose_delay_ms = gluc_delay;
	sampling_config_buf.flags = in->flags;
	sampling_config_buf.auto_enabled = auto_en;
	sampling_config_buf.schedule_interval_sec = sched_sec;

#if defined(CONFIG_GLUCOSE_SENSOR)
	{
		int rc = glucose_set_config(gluc_ns, gluc_delay);

		if (rc != 0) {
			LOG_WRN("glucose_set_config failed: %d", rc);
			return BT_GATT_ERR(BT_ATT_ERR_UNLIKELY);
		}
	}
#endif

#if defined(CONFIG_APP_FEATURE_PPG)
	{
		bool disable_prox = (in->flags & BLE_SAMP_FLAG_DISABLE_PROX) != 0U;

		ppg_set_proximity_disabled(disable_prox);
#if defined(CONFIG_APP_HEALTH_SCHED)
		health_sched_notify_prox_policy_changed();
#endif
	}
#endif

#if defined(CONFIG_APP_HEALTH_SCHED)
	health_sched_set_auto_enabled(auto_en != 0U);
	health_sched_set_interval_sec(sched_sec);
	sampling_config_buf.current_interval_sec =
		(uint16_t)health_sched_current_interval_sec();
#endif

#if defined(CONFIG_APP_TEMP_MONITOR)
	if (len >= 14U) {
		if (temp_idle_sec != 0U &&
		    (temp_idle_sec < 10U || temp_idle_sec > 3600U)) {
			return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
		}
		sampling_config_buf.temp_idle_interval_sec =
			temp_set_idle_interval_sec(temp_idle_sec);
	}
#endif

	sampling_config_persist();

	LOG_INF("sampling cfg: ppg=%u gluc_ns=%u delay_ms=%u flags=0x%02x "
		"auto=%u interval=%u current=%u temp_idle=%u",
		ppg_count, gluc_ns, gluc_delay, in->flags, auto_en, sched_sec,
		sampling_config_buf.current_interval_sec,
		sampling_config_buf.temp_idle_interval_sec);
	return len;
}

uint32_t ble_gatt_get_ppg_sample_count(void)
{
	if (sampling_config_buf.ppg_sample_count == 0U) {
		return BLE_PPG_SAMPLE_COUNT_DEFAULT;
	}
	return sampling_config_buf.ppg_sample_count;
}

static ssize_t write_ppg_decimate(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				  const void *buf, uint16_t len, uint16_t offset,
				  uint8_t flags)
{
	if (len != sizeof(uint8_t)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
	}

	uint8_t val = ((const uint8_t *)buf)[0];
	if (val < 1 || val > 33) {
		return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
	}

	ppg_decimate_buf = val;
	ppg_stream_runtime_decimate = val;
	ppg_stream_runtime_phase = 0U;
	{
		struct app_config *cfg = config_manager_get_mutable();

		if (cfg != NULL) {
			cfg->ppg.stream_decimate = val;
			(void)config_manager_save();
		}
	}
	LOG_INF("PPG BLE decimation set to %u (1/%u rate)", val, val);
	return len;
}

static ssize_t read_proximity_status(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				     void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset,
				 &proximity_status_buf, sizeof(proximity_status_buf));
}

#if defined(CONFIG_BLE_PPG_STREAMING)
static void ppg_stream_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	if (value == BT_GATT_CCC_NOTIFY) {
		atomic_set_bit(notify_subscribed, NOTIFY_ID_PPG_STREAM);
		ppg_drop_count = 0;
		ppg_stream_runtime_decimate = 1U;
		ppg_stream_runtime_phase = 0U;
		ppg_stream_enomem_streak = 0U;
		ppg_stream_success_streak = 0U;
	} else {
		atomic_clear_bit(notify_subscribed, NOTIFY_ID_PPG_STREAM);
	}
	LOG_INF("PPG stream notifications %s", value ? "enabled" : "disabled");
}
#endif

#if defined(CONFIG_BLE_ACCEL_STREAMING)
static void accel_stream_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	if (value == BT_GATT_CCC_NOTIFY) {
		atomic_set_bit(notify_subscribed, NOTIFY_ID_ACCEL_STREAM);
	} else {
		atomic_clear_bit(notify_subscribed, NOTIFY_ID_ACCEL_STREAM);
	}
	LOG_INF("Accel stream notifications %s", value ? "enabled" : "disabled");
}
#endif

/* =============================================================================
 * SIG Battery Service (0x180F / 0x2A19)
 * ============================================================================= */

static ssize_t read_bas_battery_level(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				      void *buf, uint16_t len, uint16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &bas_battery_level,
				 sizeof(bas_battery_level));
}

static void bas_battery_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	ARG_UNUSED(attr);
	bas_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
	LOG_DBG("BAS notifications %s", bas_notify_enabled ? "enabled" : "disabled");
}

/* =============================================================================
 * GATT Service Definitions
 * ============================================================================= */

BT_GATT_SERVICE_DEFINE(battery_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_BAS),

	BT_GATT_CHARACTERISTIC(BT_UUID_BAS_BATTERY_LEVEL,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ,
			       read_bas_battery_level, NULL, &bas_battery_level),
	BT_GATT_CCC(bas_battery_ccc_changed, BLE_GATT_PERM_CCC_OPEN),
);

/* Wearable Configuration Service */
BT_GATT_SERVICE_DEFINE(wearable_config_svc,
	BT_GATT_PRIMARY_SERVICE(&wearable_svc_uuid),

	/* Device Name */
	BT_GATT_CHARACTERISTIC(&device_name_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BLE_PERM_DEVICE_NAME,
		read_device_name, write_device_name, device_name_buf),

	/* RTC Trim PPM */
	BT_GATT_CHARACTERISTIC(&rtc_trim_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BLE_PERM_RTC_TRIM,
		read_rtc_trim, write_rtc_trim, &rtc_trim_buf),

	/* Battery Low Threshold */
	BT_GATT_CHARACTERISTIC(&battery_low_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BLE_PERM_BATTERY_LOW,
		read_battery_low, write_battery_low, &battery_low_buf),

	/* Brightness */
	BT_GATT_CHARACTERISTIC(&brightness_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BLE_PERM_BRIGHTNESS,
		read_brightness, write_brightness, &brightness_buf),

	/* Display idle timeout (seconds) */
	BT_GATT_CHARACTERISTIC(&screen_timeout_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BLE_PERM_BRIGHTNESS,
		read_screen_timeout, write_screen_timeout, &screen_timeout_buf),

	/* Battery poll interval (seconds) */
	BT_GATT_CHARACTERISTIC(&batt_poll_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BLE_PERM_BRIGHTNESS,
		read_batt_poll, write_batt_poll, &batt_poll_buf),

	/* Critical battery SOC threshold (percent) */
	BT_GATT_CHARACTERISTIC(&batt_sleep_soc_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BLE_PERM_BRIGHTNESS,
		read_batt_sleep_soc, write_batt_sleep_soc, &batt_sleep_soc_buf),

	/* Volume */
	BT_GATT_CHARACTERISTIC(&volume_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BLE_PERM_VOLUME,
		read_volume, write_volume, &volume_buf),

	/* PPG Source Preference */
	BT_GATT_CHARACTERISTIC(&ppg_pref_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BLE_PERM_PPG_PREF,
		read_ppg_pref, write_ppg_pref, &ppg_pref_buf),

	/* Security Profile (Read-only): 0=open, 1=secure */
	BT_GATT_CHARACTERISTIC(&security_profile_chrc_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ,
		read_security_profile, NULL, &security_profile_buf),

	/* Hardware Device ID (Read-only): hwinfo hex string */
	BT_GATT_CHARACTERISTIC(&device_id_chrc_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ,
		read_device_id, NULL, device_id_buf),

	/* Pairing status (Read-only): watch SMP state for central ceremony gating */
	BT_GATT_CHARACTERISTIC(&pairing_status_chrc_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ,
		read_pairing_status, NULL, NULL),

	/* Device Build (Read-only): FW / git / uptime / pending / device_id */
	BT_GATT_CHARACTERISTIC(&device_build_chrc_uuid.uuid,
		BT_GATT_CHRC_READ,
		BT_GATT_PERM_READ,
		read_device_build, NULL, &device_build_buf),

	/* Sampling Config (R/W): PPG count + glucose ADC knobs for testing */
	BT_GATT_CHARACTERISTIC(&sampling_config_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BLE_PERM_SAMPLING_CFG,
		read_sampling_config, write_sampling_config, &sampling_config_buf),

	/* Measurement Control (Write) */
	BT_GATT_CHARACTERISTIC(&meas_ctrl_chrc_uuid.uuid,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BLE_PERM_MEAS_CTRL,
		NULL, write_meas_ctrl, NULL),

	/* PPG Stream Decimation (R/W) */
	BT_GATT_CHARACTERISTIC(&ppg_decimate_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BLE_PERM_PPG_DECIMATE,
		read_ppg_decimate, write_ppg_decimate, &ppg_decimate_buf),

	/* PMIC Control (Write) — requires encrypted/authenticated link per profile */
	BT_GATT_CHARACTERISTIC(&pmic_ctrl_chrc_uuid.uuid,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BLE_PERM_PMIC_CTRL,
		NULL, write_pmic_ctrl, NULL),

	/* Admin Control (Write): delete bond for requesting peer */
	BT_GATT_CHARACTERISTIC(&admin_ctrl_chrc_uuid.uuid,
		BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
		BLE_PERM_ADMIN_CTRL,
		NULL, write_admin_ctrl, NULL),
);

/* Sensor Data Streaming Service */
BT_GATT_SERVICE_DEFINE(sensor_data_svc,
	BT_GATT_PRIMARY_SERVICE(&sensor_svc_uuid),

	/* PMIC Status */
	BT_GATT_CHARACTERISTIC(&pmic_status_chrc_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(pmic_ccc_changed, BLE_PERM_CCC_PMIC),

	/* Temperature */
	BT_GATT_CHARACTERISTIC(&temperature_chrc_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(temperature_ccc_changed, BLE_PERM_CCC_TEMP),

	/* Vitals (HR + SpO2 + Hb + RespRate) */
	BT_GATT_CHARACTERISTIC(&vitals_chrc_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(vitals_ccc_changed, BLE_PERM_CCC_VITALS),

	/* Glucose */
	BT_GATT_CHARACTERISTIC(&glucose_chrc_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(glucose_ccc_changed, BLE_PERM_CCC_GLUCOSE),

	/* Glucose Per-sample Stream */
	BT_GATT_CHARACTERISTIC(&glucose_sample_chrc_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(glucose_sample_ccc_changed, BLE_PERM_CCC_GLUC_SAMP),

	/* Glucose Algorithm Details */
	BT_GATT_CHARACTERISTIC(&glucose_algo_chrc_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(glucose_algo_ccc_changed, BLE_PERM_CCC_GLUC_ALGO),

	/* PMIC Extended Status */
	BT_GATT_CHARACTERISTIC(&pmic_ext_chrc_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(pmic_ext_ccc_changed, BLE_PERM_CCC_PMIC_EXT),

	/* Proximity Status */
	BT_GATT_CHARACTERISTIC(&proximity_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BLE_PERM_PROXIMITY_READ,
		read_proximity_status, NULL, &proximity_status_buf),
	BT_GATT_CCC(proximity_ccc_changed, BLE_PERM_CCC_PROXIMITY),

	/* All Sensors Combined */
	BT_GATT_CHARACTERISTIC(&sensor_all_chrc_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(sensor_all_ccc_changed, BLE_PERM_CCC_SENSOR_ALL),

	/* Measurement Status (Notify) */
	BT_GATT_CHARACTERISTIC(&meas_status_chrc_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(meas_status_ccc_changed, BLE_PERM_CCC_MEAS_STAT),

#if defined(CONFIG_BLE_PPG_STREAMING)
	/* PPG Raw Stream (Notify) */
	BT_GATT_CHARACTERISTIC(&ppg_stream_chrc_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(ppg_stream_ccc_changed, BLE_PERM_CCC_PPG),
#endif

#if defined(CONFIG_BLE_ACCEL_STREAMING)
	/* Accelerometer Stream (Notify, ~10 Hz) */
	BT_GATT_CHARACTERISTIC(&accel_stream_chrc_uuid.uuid,
		BT_GATT_CHRC_NOTIFY,
		BT_GATT_PERM_NONE,
		NULL, NULL, NULL),
	BT_GATT_CCC(accel_stream_ccc_changed, BLE_PERM_CCC_ACCEL),
#endif
);

	/* WiFi Configuration Service */
	BT_GATT_SERVICE_DEFINE(wifi_config_svc,
		BT_GATT_PRIMARY_SERVICE(&wifi_svc_uuid),

	/* WiFi Enable */
	BT_GATT_CHARACTERISTIC(&wifi_enable_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BLE_PERM_WIFI_ENABLE,
		read_wifi_enable, write_wifi_enable, &wifi_enable_buf),

	/* WiFi SSID */
	BT_GATT_CHARACTERISTIC(&wifi_ssid_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
		BLE_PERM_WIFI_SSID,
		read_wifi_ssid, write_wifi_ssid, wifi_ssid_buf),

	/* WiFi Password */
	BT_GATT_CHARACTERISTIC(&wifi_password_chrc_uuid.uuid,
		BT_GATT_CHRC_WRITE,
		BLE_PERM_WIFI_PASSWORD,
		NULL, write_wifi_password, wifi_password_buf),

	/* WiFi Status */
	BT_GATT_CHARACTERISTIC(&wifi_status_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BLE_PERM_WIFI_STATUS_R,
		read_wifi_status, NULL, &wifi_status_buf),
	BT_GATT_CCC(wifi_status_ccc_changed, BLE_PERM_WIFI_STATUS_CCC),

	/* WiFi Connect */
	BT_GATT_CHARACTERISTIC(&wifi_connect_chrc_uuid.uuid,
		BT_GATT_CHRC_WRITE,
		BLE_PERM_WIFI_CONNECT,
		NULL, write_wifi_connect, NULL),

#if defined(CONFIG_APP_FEATURE_WIFI) && defined(CONFIG_WIFI_WEXX)
	/* WiFi Bulk Session Control */
	BT_GATT_CHARACTERISTIC(&wifi_session_ctrl_chrc_uuid.uuid,
		BT_GATT_CHRC_WRITE,
		BLE_PERM_WIFI_SESSION_CTRL,
		NULL, write_wifi_session_ctrl, NULL),

	/* WiFi Bulk Session Status */
	BT_GATT_CHARACTERISTIC(&wifi_session_status_chrc_uuid.uuid,
		BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
		BLE_PERM_WIFI_SESSION_STATUS_R,
		read_wifi_session_status, NULL, &wifi_session_status_buf),
	BT_GATT_CCC(wifi_session_status_ccc_changed,
		BLE_PERM_WIFI_SESSION_STATUS_CCC),
#endif
);

/* =============================================================================
 * Connection Callbacks
 * ============================================================================= */

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		LOG_ERR("Connection failed (err %u): %s", err, addr);
		return;
	}

	LOG_INF("Connected: %s", addr);
	LOG_INF("Awaiting host CTS Current Time sync (0x1805 / 0x2A2B)");
	current_conn = bt_conn_ref(conn);

	if (adv_restart_work_inited) {
		(void)k_work_cancel_delayable(&adv_restart_work);
	}

	/* Cancel slow-advertising timeout — connection established */
	adv_is_slow = false;
	(void)k_work_cancel_delayable(&adv_slow_work);
	(void)k_work_cancel_delayable(&adv_watchdog_work);

	/* Reset PPG drop counter for fresh connection */
	ppg_drop_count = 0;
	ppg_stream_runtime_decimate = 1U;
	ppg_stream_runtime_phase = 0U;
	ppg_stream_enomem_streak = 0U;
	ppg_stream_success_streak = 0U;

	/* Android/FlutterBluePlus initiates MTU exchange immediately after service
	 * discovery. Do not start a peripheral-side exchange here; simultaneous
	 * requests have caused connect-time stalls on some phones.
	 */

	/* Request connection parameters with a generous supervision timeout.
	 * Default Zephyr timeout is 420 ms — any RF gap that long drops the link.
	 * 6 000 ms keeps the link alive through brief interference common with
	 * wearables near the body.
	 *   interval : 30 – 50 ms  (1.25 ms units → 24 / 40)
	 *   latency  : 0
	 *   timeout  : 6 000 ms    (10 ms units  → 600)
	 */
	static const struct bt_le_conn_param conn_params =
		BT_LE_CONN_PARAM_INIT(24, 40, 0, 600);
	int cp_err = bt_conn_le_param_update(conn, &conn_params);
	if (cp_err && cp_err != -EALREADY) {
		LOG_WRN("Connection param update failed: %d", cp_err);
	}

	/* Do not issue an immediate explicit DLE request here.
	 * Some controller/peer combinations stall on HCI LE Set Data Length
	 * (opcode 0x2022), which can trigger a host assert on command timeout.
	 * Keep link stability first; MTU negotiation still handles our payloads. */

	/* Keep default 1M PHY for compatibility.
	 * Some centrals reject an explicit 2M request (HCI status 0x3A), which is
	 * benign but noisy in logs (bt_conn prints an error asynchronously).
	 */

	if (connection_callback) {
		connection_callback(true);
	}

	ble_gatt_seed_from_last_results_async();
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Disconnected: %s (reason %u)", addr, reason);

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}

#if defined(CONFIG_GLUCOSE_MATRICES_FROM_XIP)
	/* Abort any in-flight glucose-model transfer on link loss. */
	ble_model_transfer_reset();
#endif
#if defined(CONFIG_RESOURCE) && defined(CONFIG_BT)
	ble_resource_transfer_reset();
#endif
	ble_record_sync_reset();
#if defined(CONFIG_APP_FEATURE_WIFI) && defined(CONFIG_WIFI_WEXX)
	wifi_bulk_session_abort();
#endif

	/* FAILED: pairing is over — return UI to idle once the link drops.
	 * COMPLETE is kept briefly so a bonded reconnect can show success. */
	if (pairing_state == BLE_PAIRING_FAILED) {
		pairing_state = BLE_PAIRING_IDLE;
		if (pairing_callback) {
			pairing_callback(BLE_PAIRING_IDLE, 0);
		}
	} else if (pairing_state != BLE_PAIRING_COMPLETE) {
		pairing_state = BLE_PAIRING_IDLE;
	}
	passkey_confirmed_on_device = false;
	current_passkey = 0U;

	/* Clear all subscription bits — client will re-subscribe after reconnect */
	for (int i = 0; i < NOTIFY_ID_COUNT; i++) {
		atomic_clear_bit(notify_subscribed, i);
		atomic_clear_bit(snapshot_push_pending, i);
	}
	if (seed_cache_work_inited) {
		(void)k_work_cancel_delayable(&seed_cache_work);
	}
	if (snapshot_push_work_inited) {
		(void)k_work_cancel_delayable(&snapshot_push_work);
	}

	if (connection_callback) {
		connection_callback(false);
	}

	/* Restart advertising with delayed worker + retries to avoid transient
	 * controller resource errors right after disconnect. */
	if (adv_restart_work_inited) {
		(void)k_work_reschedule(&adv_restart_work, K_MSEC(150));
		(void)k_work_reschedule(&adv_watchdog_work, K_MSEC(ADV_WATCHDOG_MS));
	} else {
		(void)ble_gatt_start_advertising();
	}
}

#if defined(CONFIG_BT_SMP)
static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (!err) {
		LOG_INF("Security changed: %s level %u", addr, level);
	} else {
		LOG_ERR("Security failed: %s level %u err %d", addr, level, err);
	}
}
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
#if defined(CONFIG_BT_SMP)
	.security_changed = security_changed,
#endif
};

/* =============================================================================
 * Pairing Callbacks (Passkey Display)
 * ============================================================================= */

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Passkey for %s: %06u", addr, passkey);

	passkey_confirmed_on_device = false;
	current_passkey = passkey;
	pairing_state = BLE_PAIRING_CONFIRM_PASSKEY;

	if (pairing_callback) {
		pairing_callback(BLE_PAIRING_CONFIRM_PASSKEY, passkey);
	}
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Confirm passkey for %s: %06u", addr, passkey);

	passkey_confirmed_on_device = false;
	current_passkey = passkey;
	pairing_state = BLE_PAIRING_CONFIRM_PASSKEY;

	if (pairing_callback) {
		pairing_callback(BLE_PAIRING_CONFIRM_PASSKEY, passkey);
	}

	/* User confirms on watch (Accept) and on HCM Monitor PC app. Do not
	 * auto-confirm here — premature peripheral confirm without a matching
	 * WinRT deferral leaves SMP waiting until timeout (err 9). */
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Pairing cancelled: %s", addr);

	if (pairing_state == BLE_PAIRING_FAILED) {
		return;
	}

	pairing_state = BLE_PAIRING_FAILED;

	if (pairing_callback) {
		pairing_callback(BLE_PAIRING_FAILED, 0);
	}

	/* Do NOT force-disconnect here. On Windows (WinRT) numeric-comparison
	 * pairing, tearing down the ACL mid-ceremony leaves the host stuck in
	 * OPERATION_ALREADY_IN_PROGRESS. Keeping the link up lets the central
	 * simply retry pairing on the same connection. Explicit cleanup is still
	 * available via the app Disconnect/Forget controls or the device Forget
	 * button (ble_gatt_delete_bonds()).
	 */
}

static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Pairing complete: %s, bonded: %s", addr, bonded ? "yes" : "no");

	pairing_state = BLE_PAIRING_COMPLETE;

	if (pairing_callback) {
		pairing_callback(BLE_PAIRING_COMPLETE, 0);
	}
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_ERR("Pairing failed: %s, reason: %d (host may have rejected the code)", addr,
		reason);

	if (pairing_state == BLE_PAIRING_FAILED) {
		return;
	}

	pairing_state = BLE_PAIRING_FAILED;

	if (pairing_callback) {
		pairing_callback(BLE_PAIRING_FAILED, 0);
	}

	/* Intentionally do NOT bt_unpair() or force a disconnect here.
	 *
	 * Auto-unpairing wiped good bonds and, combined with a mid-ceremony ACL
	 * teardown, raced the Windows (WinRT) pairing state machine — the exact
	 * cause of the OPERATION_ALREADY_IN_PROGRESS wedge. Because the device
	 * enforces MITM + LE Secure Connections and is bondable, a subsequent
	 * authenticated pairing simply overwrites any stale bond, so no manual
	 * wipe is needed on the common retry path. Explicit recovery is still
	 * available via ble_gatt_delete_bonds() (app "Unpair All" / device
	 * "Forget"). Keeping the link up lets the central retry pairing cleanly.
	 */
}

static struct bt_conn_auth_cb auth_cb_display = {
	.passkey_display = auth_passkey_display,
	.passkey_confirm = auth_passkey_confirm,
	.cancel = auth_cancel,
};

static struct bt_conn_auth_info_cb auth_info_cb = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed,
};

/* =============================================================================
 * Public API Implementation
 * ============================================================================= */

/* Called from BT RX thread once the controller reports ready. */
static void bt_ready_cb(int err)
{
	if (err) {
		LOG_ERR("Bluetooth enable failed: %d", err);
		ble_stack_ready = false;
		ble_stack_failed = true;
		return;
	}

	/* Load persistent settings (restores bond keys from NVS). */
	int ret = settings_load();
	if (ret) {
		LOG_WRN("settings_load failed: %d (bonds may not be restored)", ret);
	}

	ret = bt_conn_auth_cb_register(&auth_cb_display);
	if (ret) {
		LOG_ERR("Auth callback registration failed: %d", ret);
	}

	ret = bt_conn_auth_info_cb_register(&auth_info_cb);
	if (ret) {
		LOG_ERR("Auth info callback registration failed: %d", ret);
	}

	bt_set_name(device_name_buf);

	ret = ble_gatt_start_advertising();
	if (ret) {
		LOG_ERR("BLE advertising start failed: %d", ret);
		ble_stack_ready = false;
		ble_stack_failed = true;
		return;
	}

	/* Pre-resolve GATT attribute pointers for the notify hot path. */
	ble_gatt_cache_notify_attrs();

	ble_stack_failed = false;
	ble_stack_ready = true;
	LOG_INF("BLE GATT ready — advertising as '%s'", device_name_buf);
}

int ble_gatt_init(void)
{
	LOG_INF("Initializing BLE GATT services (async)...");

	if (device_identity_get_hex(device_id_buf, sizeof(device_id_buf)) < 0) {
		strncpy(device_id_buf, "UNKNOWN", sizeof(device_id_buf));
	}
	LOG_INF("Hardware device ID: %s", device_id_buf);
	ble_gatt_refresh_device_build();

	if (!adv_restart_work_inited) {
		k_work_init_delayable(&adv_restart_work, adv_restart_work_handler);
		k_work_init_delayable(&adv_slow_work, adv_slow_work_handler);
		k_work_init_delayable(&adv_watchdog_work, adv_watchdog_work_handler);
		adv_restart_work_inited = true;
	}

	/* Load configuration */
	const struct app_config *cfg = config_manager_get();
	if (cfg) {
		strncpy(device_name_buf, cfg->device.name, sizeof(device_name_buf));
		brightness_buf = cfg->device.brightness;
		volume_buf = cfg->device.volume;
		ppg_pref_buf = (uint8_t)cfg->ppg.preference;
		screen_timeout_buf = cfg->device.sleep_timeout_s;
		batt_poll_buf = cfg->device.batt_poll_interval_s;
		batt_sleep_soc_buf = cfg->device.batt_sleep_soc_pct;
		if (cfg->device.batt_low_mv >= 2800U) {
			battery_low_buf = cfg->device.batt_low_mv;
		}
		sampling_config_apply_persisted(cfg);
		/* Keep stream runtime in sync with restored f012 value. */
		ppg_stream_runtime_decimate = ppg_decimate_buf;
		ppg_stream_runtime_phase = 0U;
	}

	ble_gatt_apply_product_name();
	ble_gatt_refresh_adv_payload();

	{
		struct device_config scfg;

		if (config_load(&scfg) == 0) {
			rtc_trim_buf = scfg.rtc_trim_ppm;
			battery_low_buf = scfg.battery_low_mv;
			(void)rtc_set_trim_ppm(rtc_trim_buf);
		}
	}

	(void)device_prefs_apply_brightness(brightness_buf);
	(void)device_prefs_apply_volume(volume_buf);

#if defined(CONFIG_BT_CTS)
	{
		int cts_ret = ble_cts_init();

		if (cts_ret != 0) {
			LOG_WRN("CTS init failed: %d (continuing without CTS)", cts_ret);
		}
	}
#endif

#if defined(CONFIG_APP_FEATURE_WIFI) && defined(CONFIG_WIFI_WEXX)
	wifi_bulk_session_init();
	wifi_bulk_session_register_status_cb(wifi_bulk_status_changed);
#endif

	/* Start BLE stack asynchronously — bt_ready_cb() is called from the BT
	 * RX thread once the controller is ready.  Using a callback (not NULL)
	 * prevents bt_enable() from blocking main() with K_FOREVER, which would
	 * freeze the display if the radio takes longer than expected to init. */
	ble_stack_ready = false;
	ble_stack_failed = false;

	int ret = bt_enable(bt_ready_cb);
	if (ret) {
		LOG_ERR("bt_enable failed: %d", ret);
		ble_stack_failed = true;
		return ret;
	}

	LOG_INF("BLE stack starting — advertising will begin when controller is ready");
	return 0;
}

int ble_gatt_start_advertising(void)
{
	struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM_INIT(
		BT_LE_ADV_OPT_CONN,
		BT_GAP_ADV_FAST_INT_MIN_1,
		BT_GAP_ADV_FAST_INT_MAX_1,
		NULL);

	int ret = bt_le_adv_start(&adv_param, adv_payload, ARRAY_SIZE(adv_payload),
				  sd_payload, ARRAY_SIZE(sd_payload));
	if (ret == -EALREADY) {
		LOG_DBG("Advertising already active");
		return 0;
	}
	if (ret) {
		LOG_ERR("Advertising start failed: %d", ret);
		return ret;
	}

	LOG_INF("BLE advertising started: %s", device_name_buf);

	/* Schedule switch to slow advertising after ADV_FAST_TIMEOUT_MS */
	adv_is_slow = false;
	(void)k_work_reschedule(&adv_slow_work, K_MSEC(ADV_FAST_TIMEOUT_MS));

	/* Keep a lightweight periodic guard while disconnected. */
	if (!current_conn) {
		(void)k_work_reschedule(&adv_watchdog_work, K_MSEC(ADV_WATCHDOG_MS));
	}

	return 0;
}

int ble_gatt_stop_advertising(void)
{
	(void)k_work_cancel_delayable(&adv_watchdog_work);
	(void)k_work_cancel_delayable(&adv_slow_work);

	int ret = bt_le_adv_stop();
	if (ret) {
		LOG_ERR("Advertising stop failed: %d", ret);
		return ret;
	}
	LOG_INF("BLE advertising stopped");
	return 0;
}

bool ble_gatt_is_connected(void)
{
	return current_conn != NULL;
}

bool ble_gatt_is_ready(void)
{
	return ble_stack_ready;
}

bool ble_gatt_init_failed(void)
{
	return ble_stack_failed;
}

void *ble_gatt_get_connection(void)
{
	return current_conn;
}

void ble_gatt_register_connection_cb(ble_connection_cb cb)
{
	connection_callback = cb;
}

void ble_gatt_register_pairing_cb(ble_pairing_cb cb)
{
	pairing_callback = cb;
}

enum ble_pairing_state ble_gatt_get_pairing_state(void)
{
	return pairing_state;
}

uint32_t ble_gatt_get_pairing_passkey(void)
{
	return current_passkey;
}

int ble_gatt_confirm_passkey(bool accept)
{
	if (!current_conn) {
		return -ENOTCONN;
	}

	if (accept) {
		if (passkey_confirmed_on_device) {
			return 0;
		}
		int err = bt_conn_auth_passkey_confirm(current_conn);

		if (err == 0) {
			passkey_confirmed_on_device = true;
			pairing_state = BLE_PAIRING_BONDING;
			LOG_INF("Passkey confirmed on device — waiting for central");
			if (pairing_callback) {
				pairing_callback(BLE_PAIRING_BONDING, current_passkey);
			}
		}
		return err;
	}

	passkey_confirmed_on_device = false;
	return bt_conn_auth_cancel(current_conn);
}

int ble_gatt_disconnect(void)
{
	if (!current_conn) {
		return -ENOTCONN;
	}
	return bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

/* Bond count helper used by bt_foreach_bond */
static void _bond_count_cb(const struct bt_bond_info *info, void *user_data)
{
	ARG_UNUSED(info);
	int *count = user_data;
	(*count)++;
}

int ble_gatt_get_bond_count(void)
{
	int count = 0;

	bt_foreach_bond(BT_ID_DEFAULT, _bond_count_cb, &count);
	return count;
}

int ble_gatt_delete_bonds(void)
{
	if (!delete_bonds_work_inited) {
		k_work_init(&delete_bonds_work, delete_bonds_work_handler);
		delete_bonds_work_inited = true;
	}

	if (k_work_submit(&delete_bonds_work) < 0) {
		LOG_WRN("Bond wipe already pending");
		return -EALREADY;
	}

	return 0;
}

/* =============================================================================
 * Sensor Data Notification Functions
 * ============================================================================= */

static int notify_characteristic(uint8_t id, const void *data, size_t len)
{
	if (current_conn == NULL) {
		return -ENOTCONN;
	}

	struct bt_conn *conn = bt_conn_ref(current_conn);

	if (!conn) {
		return -ENOTCONN;
	}

	struct bt_conn_info info;

	if (bt_conn_get_info(conn, &info) != 0 ||
	    info.state != BT_CONN_STATE_CONNECTED) {
		bt_conn_unref(conn);
		return -ENOTCONN;
	}

	const struct bt_gatt_attr *attr = notify_attr_cache[id];

	if (!attr) {
		bt_conn_unref(conn);
		return -ENOENT;
	}

	int ret = bt_gatt_notify(conn, attr, data, len);

	bt_conn_unref(conn);
	return ret;
}

/* Subscription-gated notify helpers — skip attr lookup when not subscribed */
#define NOTIFY_IF_SUBSCRIBED(id, uuid_var, data, len) \
	(atomic_test_bit(notify_subscribed, id) \
		? notify_characteristic((id), (data), (len)) \
		: 0)

int ble_notify_pmic_data(const struct ble_pmic_data *data)
{
	sensor_all_cache.pmic = *data;
	return NOTIFY_IF_SUBSCRIBED(NOTIFY_ID_PMIC, pmic_status_chrc_uuid, data, sizeof(*data));
}

int ble_gatt_set_battery_level(uint8_t level)
{
	const struct bt_gatt_attr *attr;
	int ret = 0;

	if (level > 100U) {
		level = 100U;
	}

	if (bas_battery_level == level && !bas_notify_enabled) {
		return 0;
	}

	bas_battery_level = level;

	if (!bas_notify_enabled || current_conn == NULL) {
		return 0;
	}

	struct bt_conn *conn = bt_conn_ref(current_conn);

	if (conn == NULL) {
		return 0;
	}

	attr = bt_gatt_find_by_uuid(NULL, 0, BT_UUID_BAS_BATTERY_LEVEL);
	if (attr == NULL) {
		bt_conn_unref(conn);
		return -ENOENT;
	}

	ret = bt_gatt_notify(conn, attr, &bas_battery_level, sizeof(bas_battery_level));
	bt_conn_unref(conn);
	if (ret == -ENOMEM) {
		LOG_DBG("BAS notify dropped (TX queue full)");
		return 0;
	}

	return ret;
}

int ble_notify_pmic_ext_data(const struct ble_pmic_ext_data *data)
{
	return NOTIFY_IF_SUBSCRIBED(NOTIFY_ID_PMIC_EXT, pmic_ext_chrc_uuid, data, sizeof(*data));
}

int ble_notify_temperature_data(const struct ble_temperature_data *data)
{
	sensor_all_cache.temperature = *data;
	return NOTIFY_IF_SUBSCRIBED(NOTIFY_ID_TEMP, temperature_chrc_uuid, data, sizeof(*data));
}

int ble_notify_vitals_data(const struct ble_vitals_data *data)
{
	sensor_all_cache.vitals = *data;
	return NOTIFY_IF_SUBSCRIBED(NOTIFY_ID_VITALS, vitals_chrc_uuid, data, sizeof(*data));
}

int ble_notify_glucose_data(const struct ble_glucose_data *data)
{
	sensor_all_cache.glucose = *data;
	return NOTIFY_IF_SUBSCRIBED(NOTIFY_ID_GLUCOSE, glucose_chrc_uuid, data, sizeof(*data));
}

int ble_notify_glucose_sample_data(const struct ble_glucose_sample_data *data)
{
	return NOTIFY_IF_SUBSCRIBED(NOTIFY_ID_GLUC_SAMP, glucose_sample_chrc_uuid,
				    data, sizeof(*data));
}

int ble_notify_glucose_algo_data(const struct ble_glucose_algo_data *data)
{
	glucose_algo_cache = *data;
	glucose_algo_cache_valid = true;
	return NOTIFY_IF_SUBSCRIBED(NOTIFY_ID_GLUC_ALGO, glucose_algo_chrc_uuid,
				    data, sizeof(*data));
}

int ble_notify_all_sensor_data(const struct ble_sensor_all_data *data)
{
	return NOTIFY_IF_SUBSCRIBED(NOTIFY_ID_SENSOR_ALL, sensor_all_chrc_uuid, data, sizeof(*data));
}

int ble_gatt_publish_sensor_all_snapshot(void)
{
	return ble_notify_all_sensor_data(&sensor_all_cache);
}

int ble_notify_proximity_status(const struct ble_proximity_status *status)
{
	memcpy(&proximity_status_buf, status, sizeof(proximity_status_buf));
	sensor_all_cache.proximity = *status;
	return NOTIFY_IF_SUBSCRIBED(NOTIFY_ID_PROXIMITY, proximity_chrc_uuid,
				    status, sizeof(*status));
}

int ble_notify_wifi_status(const struct ble_wifi_status *status)
{
	memcpy(&wifi_status_buf, status, sizeof(wifi_status_buf));
#if defined(CONFIG_APP_FEATURE_CLOUD_TELEMETRY)
	cloud_telemetry_on_wifi_status(status);
#endif
	return NOTIFY_IF_SUBSCRIBED(NOTIFY_ID_WIFI_STATUS, wifi_status_chrc_uuid,
				    status, sizeof(*status));
}

int ble_notify_sensor_data(const void *data, size_t len)
{
	/* Legacy compatibility — uses all-sensor characteristic */
	return NOTIFY_IF_SUBSCRIBED(NOTIFY_ID_SENSOR_ALL, sensor_all_chrc_uuid, data, len);
}

int ble_notify_meas_status(const struct ble_meas_status *status)
{
	memcpy(&meas_status_buf, status, sizeof(meas_status_buf));
	return NOTIFY_IF_SUBSCRIBED(NOTIFY_ID_MEAS_STATUS, meas_status_chrc_uuid,
				    status, sizeof(*status));
}

int ble_notify_ppg_sample(const struct ble_ppg_sample *sample)
{
#if defined(CONFIG_BLE_PPG_STREAMING)
	if (!atomic_test_bit(notify_subscribed, NOTIFY_ID_PPG_STREAM)) {
		return 0;
	}

	if (ppg_stream_runtime_decimate > 1U) {
		ppg_stream_runtime_phase++;
		if ((ppg_stream_runtime_phase % ppg_stream_runtime_decimate) != 0U) {
			return 0;
		}
	}

	int ret = notify_characteristic(NOTIFY_ID_PPG_STREAM, sample, sizeof(*sample));

	if (ret == -ENOMEM) {
		/* TX queue full — log every 33 drops (~1 second at 33 Hz) */
		ppg_drop_count++;
		ppg_stream_enomem_streak++;
		ppg_stream_success_streak = 0U;

		if (ppg_stream_enomem_streak >= 8U &&
		    ppg_stream_runtime_decimate < PPG_STREAM_RUNTIME_DECIMATE_MAX) {
			uint8_t next_decimate = (uint8_t)(ppg_stream_runtime_decimate * 2U);
			ppg_stream_runtime_decimate =
				(next_decimate > PPG_STREAM_RUNTIME_DECIMATE_MAX)
				? PPG_STREAM_RUNTIME_DECIMATE_MAX : next_decimate;
			ppg_stream_enomem_streak = 0U;
			LOG_WRN("PPG backpressure: runtime decimation increased to 1/%u",
				ppg_stream_runtime_decimate);
		}

		if ((ppg_drop_count % 33U) == 1U) {
			LOG_WRN("PPG BLE TX queue full: %u samples dropped",
				ppg_drop_count);
		}
	} else if (ret == 0) {
		if (ppg_stream_enomem_streak > 0U) {
			ppg_stream_enomem_streak--;
		}

		ppg_stream_success_streak++;
		if (ppg_stream_runtime_decimate > 1U && ppg_stream_success_streak >= 128U) {
			uint8_t next_decimate = (uint8_t)(ppg_stream_runtime_decimate / 2U);
			ppg_stream_runtime_decimate = (next_decimate < 1U) ? 1U : next_decimate;
			ppg_stream_success_streak = 0U;
			LOG_INF("PPG backpressure: runtime decimation relaxed to 1/%u",
				ppg_stream_runtime_decimate);
		}
	}

	return ret;
#else
	ARG_UNUSED(sample);
	return -ENOTSUP;
#endif
}

int ble_notify_accel_sample(const struct ble_accel_sample *sample)
{
#if defined(CONFIG_BLE_ACCEL_STREAMING)
	int ret = NOTIFY_IF_SUBSCRIBED(NOTIFY_ID_ACCEL_STREAM, accel_stream_chrc_uuid,
				       sample, sizeof(*sample));

	/* Drop silently on backpressure — avoids host TX queue desync asserts. */
	if (ret == -ENOMEM || ret == -ENOTCONN) {
		return 0;
	}

	return ret;
#else
	ARG_UNUSED(sample);
	return -ENOTSUP;
#endif
}

void ble_gatt_register_meas_ctrl_cb(ble_meas_ctrl_cb cb)
{
	meas_ctrl_callback = cb;
}

void ble_gatt_register_pmic_ctrl_cb(ble_pmic_ctrl_cb cb)
{
	pmic_ctrl_callback = cb;
}

uint8_t ble_gatt_get_ppg_decimate(void)
{
	return ppg_decimate_buf;
}

