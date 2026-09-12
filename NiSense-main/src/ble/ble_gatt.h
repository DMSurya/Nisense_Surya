/* =============================================================================
 * BLE GATT Service Module - Header
 * =============================================================================
 * Comprehensive BLE GATT profile for wearable health monitor
 * 
 * Services:
 *   - Battery Service (0x180F) — SOC % via 0x2A19 (CONFIG_BT_BAS)
 *   - Current Time Service (0x1805) — calendar date/time/day via 0x2A2B (CONFIG_BT_CTS)
 *   - Device Information Service (0x180A) — when enabled in stack
 *   - Health Thermometer Service (0x1809) — temperature via vendor f102 / future HTS
 *   - Custom Wearable Config Service — Device configuration
 *   - Custom Sensor Data Service — PMIC voltage/current/rails (f101/f109); no SIG
 *     equivalent for pack voltage or charge current — use vendor characteristics
 *   - Custom WiFi Config Service — WiFi credential management
 *
 * Standard vs vendor (power domain):
 *   | Data              | BLE exposure                          |
 *   |-------------------|---------------------------------------|
 *   | Battery %         | SIG BAS 0x180F / 0x2A19               |
 *   | VBAT, rails, I    | Vendor f101 / f109                    |
 *   | Battery temp      | Vendor f109 (no dedicated SIG service)|
 *   | Future (BT 5.2+)  | BAS 0x2BED Level Status, 0x2BF0 Energy|
 * 
 * Target: nRF52840 (Raytac MDBT50Q-DB-40)
 * ============================================================================= */

#ifndef BLE_GATT_H
#define BLE_GATT_H

/* GATT protocol version — bump minor on additive changes, major on breaking layout changes. */
#define BLE_GATT_PROTOCOL_VERSION_MAJOR 3U
#define BLE_GATT_PROTOCOL_VERSION_MINOR 2U

#include <zephyr/kernel.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Suppress UUID encoding shift warnings (harmless Zephyr macro quirk) */
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshift-count-overflow"
#endif

/* =============================================================================
 * Custom Service UUIDs (Base: 12345678-1234-5678-1234-56789abcXXXX)
 * ============================================================================= */

/* Wearable Configuration Service UUID: ...def0 */
#define BT_UUID_WEARABLE_SVC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)

/* Sensor Data Streaming Service UUID: ...def1 */
#define BT_UUID_SENSOR_SVC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)

/* WiFi Configuration Service UUID: ...def2 */
#define BT_UUID_WIFI_SVC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef2)

/* =============================================================================
 * Wearable Config Service Characteristics (0xf0XX)
 * ============================================================================= */

/* Device Name (R/W): UTF-8 string, max 32 bytes */
#define BT_UUID_DEVICE_NAME_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf001)

/* RTC Trim PPM (R/W): int32_t, range +/-200 */
#define BT_UUID_RTC_TRIM_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf003)

/* Battery Low Threshold (R/W): uint16_t mV, range 2500-3500 */
#define BT_UUID_BATTERY_LOW_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf004)

/* Display Brightness (R/W): uint8_t 0=off, 1-100=on (GPIO enable; not PWM) */
#define BT_UUID_BRIGHTNESS_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf005)

#define BT_UUID_SCREEN_TIMEOUT_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf017)

#define BT_UUID_BATT_POLL_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf018)

#define BT_UUID_BATT_SLEEP_SOC_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf019)

/* Buzzer Volume (R/W): uint8_t 0-100 percent */
#define BT_UUID_VOLUME_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf006)

/* PPG Source Preference (R/W): uint8_t enum (0=unset, 1=auto, 2=max86141, 3=max3010x) */
#define BT_UUID_PPG_PREF_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf007)

/* Security Profile (Read-only): uint8_t 0=open, 1=secure (pair required for health data) */
#define BT_UUID_SECURITY_PROFILE_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf008)

/* Hardware Device ID (Read-only): UTF-8 uppercase hex from hwinfo (first 8 bytes) */
#define BT_UUID_DEVICE_ID_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf015)

/* Pairing Status (Read-only): struct ble_pairing_status — open read for MITM ceremony */
#define BT_UUID_PAIRING_STATUS_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf016)

/* Device Build (Read-only): struct ble_device_build_data — FW / git / uptime / records */
#define BT_UUID_DEVICE_BUILD_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf01a)

/* Sampling Config (R/W): struct ble_sampling_config — PPG/glucose test knobs */
#define BT_UUID_SAMPLING_CONFIG_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf01b)

/** Security profile values for CHRC f008 */
#define BLE_SECURITY_PROFILE_OPEN   0U
#define BLE_SECURITY_PROFILE_SECURE 1U

/* =============================================================================
 * Sensor Data Service Characteristics (0xf1XX) - All NOTIFY
 * ============================================================================= */

/* PMIC Status (Notify): struct ble_pmic_data */
#define BT_UUID_PMIC_STATUS_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf101)

/* Temperature (Notify): struct ble_temperature_data */
#define BT_UUID_TEMPERATURE_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf102)

/* SpO2 (Notify): REMOVED - merged into Vitals (f10b) */
/* #define BT_UUID_SPO2_CHRC_VAL ... f103 */

/* Heart Rate (Notify): REMOVED - merged into Vitals (f10b) */
/* #define BT_UUID_HEART_RATE_CHRC_VAL ... f104 */

/* Glucose (Notify): struct ble_glucose_data */
#define BT_UUID_GLUCOSE_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf105)

/* Glucose Per-sample Stream (Notify): struct ble_glucose_sample_data */
#define BT_UUID_GLUCOSE_SAMPLE_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf107)

/* Glucose Algorithm Details (Notify): struct ble_glucose_algo_data */
#define BT_UUID_GLUCOSE_ALGO_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf108)

/* PMIC Extended Status (Notify): struct ble_pmic_ext_data */
#define BT_UUID_PMIC_EXT_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf109)

/* Proximity Status (Read/Notify): struct ble_proximity_status */
#define BT_UUID_PROXIMITY_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf10a)

/* Vitals (Notify): struct ble_vitals_data - HR + SpO2 + Hb + RespRate combined */
#define BT_UUID_VITALS_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf10b)

/* Combined Sensor Data (Notify): All sensors in one packet */
#define BT_UUID_SENSOR_ALL_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf1ff)

/* =============================================================================
 * WiFi Config Service Characteristics (0xf2XX)
 * ============================================================================= */

/* WiFi Enable (R/W): uint8_t boolean */
#define BT_UUID_WIFI_ENABLE_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf201)

/* WiFi SSID (R/W): UTF-8 string, max 32 bytes */
#define BT_UUID_WIFI_SSID_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf202)

/* WiFi Password (W only): UTF-8 string, max 64 bytes - ENCRYPTED */
#define BT_UUID_WIFI_PASSWORD_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf203)

/* WiFi Status (R/Notify): struct ble_wifi_status */
#define BT_UUID_WIFI_STATUS_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf204)

/* WiFi Connect (W): uint8_t trigger (1 = connect, 0 = disconnect) */
#define BT_UUID_WIFI_CONNECT_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf205)

/* WiFi Bulk Session Control (W): START / ABORT + endpoint params */
#define BT_UUID_WIFI_SESSION_CTRL_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf206)

/* WiFi Bulk Session Status (R/Notify): struct wifi_bulk_status */
#define BT_UUID_WIFI_SESSION_STATUS_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf207)

/* =============================================================================
 * Wearable Config Service - Measurement Control (0xf0XX)
 * ============================================================================= */

/* Measurement Control (W): [cmd, type] — start/stop measurement */
#define BT_UUID_MEAS_CTRL_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf010)

/* Measurement Status (Notify): struct ble_meas_status */
#define BT_UUID_MEAS_STATUS_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf011)

/* PPG Stream Decimation (R/W): uint8_t 1-33, default 3 (~10 Hz over BLE) */
#define BT_UUID_PPG_DECIMATE_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf012)

/* PMIC Control (W): [cmd, target, value_le16?] */
#define BT_UUID_PMIC_CTRL_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf013)

/* Admin Control (W, encrypted): [cmd]
 *   0x01 = delete all bonds and disconnect
 */
#define BT_UUID_ADMIN_CTRL_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf014)

/* Admin commands */
#define ADMIN_CMD_DELETE_BONDS  0x01U
/** Trigger diag_monitor_dump_now() (requires CONFIG_APP_FEATURE_DIAG_MONITOR). */
#define ADMIN_CMD_MEM_DUMP      0x02U

/* =============================================================================
 * Sensor Data Service - PPG Raw Stream (0xf106)
 * ============================================================================= */

/* Accelerometer Stream (Notify): struct ble_accel_sample — gated by CONFIG_BLE_ACCEL_STREAMING */
#define BT_UUID_ACCEL_STREAM_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf10c)

/* PPG Raw Stream (Notify): struct ble_ppg_sample — gated by CONFIG_BLE_PPG_STREAMING */
#define BT_UUID_PPG_STREAM_CHRC_VAL \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcf106)

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

/* =============================================================================
 * BLE Data Structures (packed for BLE transmission)
 * ============================================================================= */

/** PMIC status data structure */
struct ble_pmic_data {
	uint16_t battery_mv;      /* Battery voltage in mV */
	int16_t  current_ma;      /* Charge/discharge current in mA (signed) */
	uint8_t  soc_percent;     /* State of charge 0-100% */
	uint8_t  charger_status;  /* 0=not charging, 1=charging, 2=full */
	uint16_t buck1_mv;        /* Buck1 voltage */
	uint16_t buck2_mv;        /* Buck2 voltage */
} __packed;

/** PMIC extended telemetry (f109). Bytes 0–27 = v1; bytes 28+ = v2 append. */
struct ble_pmic_ext_data {
	uint16_t buck3_mv;            /* Buck3 voltage (mV) */
	uint16_t bbout_mv;            /* Buck-boost output voltage (mV) */
	uint8_t  buck1_enabled;       /* 1 if enabled */
	uint8_t  buck2_enabled;       /* 1 if enabled */
	uint8_t  buck3_enabled;       /* 1 if enabled */
	uint8_t  bbout_enabled;       /* 1 if enabled */
	uint16_t charge_voltage_mv;   /* Charger constant-voltage setpoint (mV) */
	uint16_t charge_current_ma;   /* Charger constant-current setpoint (mA) */
	int16_t  battery_temp_c;      /* Battery temp in degC */
	uint16_t cycle_count;         /* Charge cycles */
	uint16_t remaining_mah;       /* Remaining capacity (mAh) */
	uint16_t full_mah;            /* Full charge capacity (mAh) */
	uint16_t design_mah;          /* Design capacity (mAh) */
	uint16_t time_to_empty_min;   /* Runtime to empty (minutes) */
	uint16_t time_to_full_min;    /* Runtime to full (minutes) */
	int16_t  avg_current_ma;      /* Average current (mA, signed) */
	/* --- payload v2 (append-only; old clients ignore trailing bytes) --- */
	uint16_t vbat_mv;             /* PMIC VBAT (mV); 0 if unavailable */
	uint16_t vcell_mv;            /* Fuel-gauge VCELL (mV); 0 if unavailable */
	uint8_t  flags;               /* bit0 usb, bit1 bat_good, bit2 bat_reg_done,
				       * bit3 sys_bat_lim, bits4–5 cell_state
				       * (0=absent, 1=present, 2=unknown) */
	uint8_t  reserved;            /* pad for alignment */
} __packed;

/** Flag bits for ble_pmic_ext_data.flags (v2) */
#define BLE_PMIC_EXT_FLAG_USB_ONLINE   (1U << 0)
#define BLE_PMIC_EXT_FLAG_BAT_GOOD     (1U << 1)
#define BLE_PMIC_EXT_FLAG_BAT_REG_DONE (1U << 2)
#define BLE_PMIC_EXT_FLAG_SYS_BAT_LIM  (1U << 3)
#define BLE_PMIC_EXT_CELL_SHIFT        4U
#define BLE_PMIC_EXT_CELL_ABSENT       0U
#define BLE_PMIC_EXT_CELL_PRESENT      1U
#define BLE_PMIC_EXT_CELL_UNKNOWN      2U

/** Device build / identity snapshot (f01a, read-only).
 *  v1: 44 B, v2: 64 B (+per-type pending), v3: 72 B (+dropped + crc_fail).
 */
struct ble_device_build_data {
	char     fw_version[16];      /* NUL-terminated FW_VERSION_STRING */
	char     git_hash[12];        /* NUL-terminated short git hash */
	uint32_t uptime_s;            /* Seconds since boot */
	uint32_t pending_records;     /* record_store pending count (all types) */
	uint8_t  device_id[8];        /* First 8 bytes of hwinfo device ID */
	uint32_t pending_glucose;     /* Pending RECORD_TYPE_GLUCOSE */
	uint32_t pending_vitals;      /* Pending RECORD_TYPE_VITALS */
	uint32_t pending_temp;        /* Pending RECORD_TYPE_TEMP */
	uint32_t pending_ppg_raw;     /* Pending RECORD_TYPE_PPG_RAW */
	uint32_t pending_glucose_raw; /* Pending RECORD_TYPE_GLUCOSE_RAW */
	uint32_t dropped;             /* v3: ring eviction count */
	uint32_t crc_fail_count;      /* v3: CRC mismatches on read */
} __packed;

/**
 * Sampling / schedule test knobs (f01b, R/W).
 *
 * Write applies immediately for glucose ADC; PPG count is used on the next
 * vitals/HR/SpO2 start via BLE Measurement Control. Write 0 in a field to
 * restore that field's firmware default (except auto_enabled: 0=off, 1=on).
 *
 * flags bit0 (BLE_SAMP_FLAG_DISABLE_PROX): persistently disable PPG proximity
 * guard for bench / scheduler / record testing.
 *
 * schedule_interval_sec: 0 = adaptive ladder; otherwise fixed seconds between
 * auto cycles (clamped 60–3600).
 *
 * current_interval_sec: read-only effective interval now (ladder step or fixed).
 * Ignored on write; always refreshed on read.
 * temp_idle_interval_sec: idle wrist/SoC temp log period (10–3600); 0 = Kconfig default.
 * Makes the GATT payload 14 bytes (accepts legacy 10 / 12 B writes).
 */
#define BLE_SAMP_FLAG_DISABLE_PROX 0x01U

struct ble_sampling_config {
	uint16_t ppg_sample_count;       /* PPG acquisition samples */
	uint16_t glucose_num_samples;    /* ADC samples per glucose (10–500) */
	uint16_t glucose_delay_ms;       /* Inter-sample / LED delay ms */
	uint8_t  flags;                  /* BLE_SAMP_FLAG_* */
	uint8_t  auto_enabled;           /* 0=off, 1=on (health auto-sched) */
	uint16_t schedule_interval_sec;  /* 0=adaptive; else fixed interval */
	uint16_t current_interval_sec;   /* R: effective now; W: ignored */
	uint16_t temp_idle_interval_sec; /* idle temp log period; 0=default */
} __packed;

/** Temperature data structure */
struct ble_temperature_data {
	int16_t  temp_c_x100;     /* Temperature in Celsius * 100 (e.g., 3650 = 36.50°C) */
	uint32_t timestamp;       /* Unix timestamp */
} __packed;

/** SpO2 data structure */
struct ble_spo2_data {
	uint8_t  spo2_percent;    /* SpO2 percentage 0-100 */
	uint8_t  confidence;      /* Measurement confidence 0-100 */
	uint8_t  finger_status;   /* 0=no finger, 1=finger detected */
	int16_t  accel_x;         /* Latest accel X (mg) */
	int16_t  accel_y;         /* Latest accel Y (mg) */
	int16_t  accel_z;         /* Latest accel Z (mg) */
	uint32_t timestamp;       /* Unix timestamp */
} __packed;

/** Heart Rate data structure */
struct ble_hr_data {
	uint16_t hr_bpm;          /* Heart rate in BPM */
	uint8_t  confidence;      /* Measurement confidence 0-100 */
	uint8_t  finger_status;   /* 0=no finger, 1=finger detected */
	int16_t  accel_x;         /* Latest accel X (mg) */
	int16_t  accel_y;         /* Latest accel Y (mg) */
	int16_t  accel_z;         /* Latest accel Z (mg) */
	uint32_t timestamp;       /* Unix timestamp */
} __packed;

/** Vitals combined data (Vital + Vascular). Packed size = 26 bytes. */
struct ble_vitals_data {
	uint16_t hr_bpm;          /* Heart rate in BPM */
	uint8_t  hr_confidence;   /* HR confidence 0-100 */
	uint8_t  spo2_percent;    /* SpO2 percentage 0-100 */
	uint8_t  spo2_confidence; /* SpO2 confidence 0-100 */
	uint16_t hb_g_dl_x10;    /* Hemoglobin g/dL * 10 */
	uint8_t  hb_confidence;   /* Hb confidence 0-100 */
	uint8_t  resp_rate_bpm;   /* Respiration rate BPM */
	uint8_t  resp_confidence; /* RespRate confidence 0-100 */
	uint16_t r_value_x1000;   /* SpO2 R-value * 1000 */
	uint8_t  quality;         /* Signal quality 0-100 */
	uint8_t  flags;           /* hr:0 spo2:1 hb:2 resp:3 finger:4 hrv:5 bp:6 */
	uint32_t timestamp;       /* Unix timestamp */
	uint16_t sdnn_ms;         /* HRV SDNN (ms) */
	uint16_t rmssd_ms;        /* HRV RMSSD (ms) */
	uint16_t systolic_mmhg;   /* Estimated systolic BP */
	uint16_t diastolic_mmhg;  /* Estimated diastolic BP */
} __packed;

/** Glucose data structure */
struct ble_glucose_data {
	uint16_t glucose_mg_dl;   /* Glucose level in mg/dL */
	uint8_t  quality;         /* Measurement quality 0-100 */
	uint32_t timestamp;       /* Unix timestamp */
} __packed;

/** Glucose algorithm details — all intermediate values from watch_intercept + HOMA-IR.
 * Uses float (not double) to keep the BLE notification ≤ 120 bytes.
 */
struct ble_glucose_algo_data {
	float  tot_coeff;          /* weighted_coeff_sum */
	float  intercept;          /* svasth_intercept */
	float  y1_value;           /* initial_glucose_estimate */
	float  avg_val;            /* coefficient_mean */
	float  std_dev;            /* coefficient_stddev */
	float  up_lim;             /* outlier_upper_limit */
	float  ll_lim;             /* outlier_lower_limit */
	int32_t p_count;           /* positive_outlier_count */
	int32_t n_count;           /* negative_outlier_count */
	float  p_val;              /* outlier_positive_sum */
	float  n_val;              /* outlier_negative_sum */
	float  p_plus_n;           /* outlier_total_removed */
	float  y2_val;             /* glucose_derived_value */
	float  y2_percent;         /* glucose_percentage_change */
	int32_t group_cd;          /* matched group index 1-11 */
	float  y2_factor;          /* regression_coefficient */
	float  y2_factor_val;      /* regression_weighted_value */
	float  const_val;          /* regression_constant */
	float  y3_value;           /* group_glucose_estimate */
	int32_t y3_row_no;         /* matched matrix row index */
	float  elim_per;           /* elimination_percentage */
	float  elim_val;           /* elimination_correction */
	int32_t y_value;           /* predicted_glucose_mg_dl */
	float  calibration_factor; /* glucose_baseline_mg_dl */
	float  ag_adjusted;        /* calibrated_glucose_mg_dl */
	float  normalized_glucose; /* normalized_glucose_mmol_l */
	float  actual_insulin;     /* fasting_insulin uIU/mL */
	float  insulin_correction; /* insulin_correction_factor */
	float  insulin_ratio;      /* insulin_sensitivity_ratio */
	float  inverse_ratio;      /* inverse_insulin_ratio */
	float  homa_ir_index;      /* HOMA-IR index */
} __packed;

/** Glucose per-sample stream data structure */
struct ble_glucose_sample_data {
	uint16_t sample_number;   /* 0..total_samples-1 */
	uint16_t total_samples;   /* total samples in this measurement */
	uint16_t raw_adc_value;   /* Raw ADC count */
	int16_t  voltage_mv_x100; /* Calibrated voltage in mV * 100 */
	uint32_t timestamp;       /* Unix timestamp (seconds) */
} __packed;

/** WiFi status structure */
struct ble_wifi_status {
	uint8_t  connected;       /* 0=disconnected, 1=connecting, 2=connected */
	int8_t   rssi_dbm;        /* Signal strength in dBm */
	uint8_t  ip_addr[4];      /* IP address bytes */
	char     ssid[32];        /* Current SSID */
} __packed;

/** Proximity status data structure */
struct ble_proximity_status {
	uint8_t  contact;         /* 0=no contact, 1=contact */
	uint8_t  wear_state;      /* Driver wear-state enum value */
	uint16_t proximity_raw;   /* Raw proximity count */
	uint16_t proximity_filt;  /* Filtered proximity count */
	uint32_t timestamp;       /* Unix timestamp */
} __packed;

/** Combined sensor data (all sensors in one notification) — protocol v2 layout */
struct ble_sensor_all_data {
	struct ble_pmic_data        pmic;
	struct ble_temperature_data temperature;
	struct ble_vitals_data      vitals;
	struct ble_glucose_data     glucose;
	struct ble_proximity_status proximity;
} __packed;

/** Measurement status notification (real-time progress) */
struct ble_meas_status {
	uint8_t  active;           /* 1 if measurement running, 0 if idle */
	uint8_t  type;             /* 1=HR, 2=SpO2, 3=Glucose, 4=Vitals (legacy 1/2 map to vitals) */
	uint8_t  percent_complete; /* 0-100 */
	uint8_t  quality;          /* Signal quality 0-100 */
	uint16_t samples_taken;    /* Samples collected so far */
	uint16_t samples_target;   /* Total samples needed */
} __packed;

/** Accelerometer sample for BLE streaming (25 Hz, decimated to ~10 Hz) */
struct ble_accel_sample {
	uint16_t seq;           /**< Rolling sequence counter */
	int16_t  x_mg;         /**< X-axis in milli-g */
	int16_t  y_mg;         /**< Y-axis in milli-g */
	int16_t  z_mg;         /**< Z-axis in milli-g */
	uint32_t timestamp_ms; /**< k_uptime_get_32() millisecond timestamp */
} __packed;

/** PPG raw sample for BLE streaming */
struct ble_ppg_sample {
	uint16_t sample_num;       /* Rolling counter */
	uint32_t raw_ir;           /* Raw IR photodiode count */
	uint32_t raw_red;          /* Raw Red photodiode count */
	uint32_t raw_green;        /* Raw Green photodiode count */
	int16_t  accel_x;          /* Accel X in mg */
	int16_t  accel_y;          /* Accel Y in mg */
	int16_t  accel_z;          /* Accel Z in mg */
	uint32_t timestamp_ms;     /* Millisecond timestamp */
} __packed;

/** Measurement control command bytes */
#define BLE_MEAS_CMD_START 0x01
#define BLE_MEAS_CMD_STOP  0x02
#define BLE_MEAS_CMD_RESET_SYNC 0x03
#define BLE_MEAS_TYPE_HR      0x01  /* legacy, kept for BLE backward-compat */
#define BLE_MEAS_TYPE_SPO2    0x02  /* legacy, kept for BLE backward-compat */
#define BLE_MEAS_TYPE_GLUCOSE 0x03
#define BLE_MEAS_TYPE_VITALS  0x04  /* unified HR+SpO2+Hb+RespRate */
#define BLE_MEAS_TYPE_ACCEL   0x05  /* DEPRECATED: accel uses f10c subscribe-only, not meas ctrl */

/** Optional 3rd byte flags for Measurement Control writes (len=3) */
#define BLE_MEAS_FLAG_SKIP_PROX 0x01  /* start with PPG proximity bypass (device hold) */

/** Callback invoked when client writes measurement control characteristic.
 * @param flags optional flags (0 when write length < 3)
 * Returns 0 on success, -EINVAL if cmd/type is unknown (triggers ATT error). */
typedef int (*ble_meas_ctrl_cb)(uint8_t cmd, uint8_t type, uint8_t flags);

/** PMIC control command bytes */
#define BLE_PMIC_CMD_ENABLE      0x01
#define BLE_PMIC_CMD_DISABLE     0x02
#define BLE_PMIC_CMD_SET_VOLTAGE 0x03

/** PMIC control target bytes */
#define BLE_PMIC_TARGET_BK1      0x01
#define BLE_PMIC_TARGET_BK2      0x02
#define BLE_PMIC_TARGET_BK3      0x03
#define BLE_PMIC_TARGET_BBOUT    0x04

/** Callback invoked when client writes PMIC control characteristic */
typedef void (*ble_pmic_ctrl_cb)(uint8_t cmd, uint8_t target, uint16_t value);

/* =============================================================================
 * BLE Connection State
 * ============================================================================= */

/** BLE pairing state enum */
enum ble_pairing_state {
	BLE_PAIRING_IDLE = 0,
	BLE_PAIRING_WAITING_PASSKEY,
	BLE_PAIRING_CONFIRM_PASSKEY,
	BLE_PAIRING_BONDING,
	BLE_PAIRING_COMPLETE,
	BLE_PAIRING_FAILED,
};

/** GATT payload for CHRC f016 (5 bytes, little-endian passkey). */
struct ble_pairing_status {
	uint8_t state;   /* enum ble_pairing_state */
	uint32_t passkey; /* 6-digit SMP code when state is confirm/bonding */
};

/** BLE connection status callback */
typedef void (*ble_connection_cb)(bool connected);

/** BLE pairing callback (passkey display) */
typedef void (*ble_pairing_cb)(enum ble_pairing_state state, uint32_t passkey);

/* =============================================================================
 * Public API
 * ============================================================================= */

/**
 * @brief Initialize BLE GATT services and start advertising
 * @return 0 on success, negative errno on failure
 */
int ble_gatt_init(void);

/**
 * @brief Stop BLE advertising
 * @return 0 on success, negative errno on failure
 */
int ble_gatt_stop_advertising(void);

/**
 * @brief Start BLE advertising
 * @return 0 on success, negative errno on failure
 */
int ble_gatt_start_advertising(void);

/**
 * @brief Check if BLE is connected
 * @return true if a central device is connected
 */
bool ble_gatt_is_connected(void);

/**
 * @brief True after controller ready and advertising has started successfully.
 */
bool ble_gatt_is_ready(void);

/**
 * @brief True if bt_enable / ready callback / advertising start failed.
 */
bool ble_gatt_init_failed(void);

/**
 * @brief Get current connection handle
 * @return Connection pointer, or NULL if not connected
 */
void *ble_gatt_get_connection(void);

/**
 * @brief Register connection state callback
 * @param cb Callback function (NULL to unregister)
 */
void ble_gatt_register_connection_cb(ble_connection_cb cb);

/**
 * @brief Register pairing state callback
 * @param cb Callback function (NULL to unregister)
 */
void ble_gatt_register_pairing_cb(ble_pairing_cb cb);

/**
 * @brief Current pairing state (updated from SMP auth callbacks).
 */
enum ble_pairing_state ble_gatt_get_pairing_state(void);

/**
 * @brief Last passkey shown for numeric-comparison pairing (0 if none).
 */
uint32_t ble_gatt_get_pairing_passkey(void);

/**
 * @brief Confirm passkey for pairing (called from UI)
 * @param accept true to accept, false to reject
 * @return 0 on success, negative errno on failure
 */
int ble_gatt_confirm_passkey(bool accept);

/**
 * @brief Disconnect current connection
 * @return 0 on success, negative errno on failure
 */
int ble_gatt_disconnect(void);

/**
 * @brief Delete all stored BLE bonds (calls bt_unpair on all bonded peers)
 *
 * Disconnects the current peer first, then erases bond keys from NVS.
 * After this call, the next connection will require re-pairing.
 *
 * @return 0 on success, negative errno on failure
 */
int ble_gatt_delete_bonds(void);

/**
 * @brief Return the number of stored bonds
 * @return Number of bonded peers (0 if none)
 */
int ble_gatt_get_bond_count(void);

/* =============================================================================
 * Sensor Data Notification API
 * ============================================================================= */

/**
 * @brief Update SIG Battery Service level (0x2A19) and notify subscribers
 * @param level State of charge 0–100 (0 when USB bench / no pack)
 * @return 0 on success, negative errno on failure
 */
int ble_gatt_set_battery_level(uint8_t level);

/**
 * @brief Refresh Device Build (f01a) read buffer (uptime / pending records).
 *
 * Call on connect and with the periodic PMIC poll so Link one-shot reads
 * see current values without waiting for a GATT read callback.
 */
void ble_gatt_refresh_device_build(void);

/**
 * @brief PPG sample count for the next BLE-started measurement.
 *
 * Returns the f01b override if set, otherwise
 * CONFIG_PPG_ALGO_SAMPLE_RATE * CONFIG_PPG_ALGO_BUFFER_SECONDS
 * (buffer defaults to 20 s when RESP is enabled, else 12 s).
 */
uint32_t ble_gatt_get_ppg_sample_count(void);

/**
 * @brief Notify PMIC status data
 * @param data PMIC status data
 * @return 0 on success, negative errno on failure
 */
int ble_notify_pmic_data(const struct ble_pmic_data *data);

/**
 * @brief Notify PMIC extended telemetry
 * @param data PMIC extended data
 * @return 0 on success, negative errno on failure
 */
int ble_notify_pmic_ext_data(const struct ble_pmic_ext_data *data);

/**
 * @brief Notify temperature data
 * @param data Temperature data
 * @return 0 on success, negative errno on failure
 */
int ble_notify_temperature_data(const struct ble_temperature_data *data);

/**
 * @brief Notify vitals data (HR + SpO2 + Hb + RespRate combined)
 * @param data Vitals data
 * @return 0 on success, negative errno on failure
 */
int ble_notify_vitals_data(const struct ble_vitals_data *data);

/**
 * @brief Notify glucose data
 * @param data Glucose data
 * @return 0 on success, negative errno on failure
 */
int ble_notify_glucose_data(const struct ble_glucose_data *data);

/**
 * @brief Notify glucose per-sample data (live sampling stream)
 * @param data Glucose per-sample data
 * @return 0 on success, negative errno on failure
 */
int ble_notify_glucose_sample_data(const struct ble_glucose_sample_data *data);

/**
 * @brief Notify glucose algorithm details (sent once after measurement completes)
 * @param data Algorithm intermediate values
 * @return 0 on success, negative errno on failure
 */
int ble_notify_glucose_algo_data(const struct ble_glucose_algo_data *data);

/**
 * @brief Notify all sensor data at once
 * @param data All sensor data combined
 * @return 0 on success, negative errno on failure
 */
int ble_notify_all_sensor_data(const struct ble_sensor_all_data *data);

/**
 * @brief Publish cached sensor_all aggregate (5 s tick from app_ble_tick)
 * @return 0 on success, negative errno on failure
 */
int ble_gatt_publish_sensor_all_snapshot(void);

/**
 * @brief Seed BLE notify caches from persisted last_home measurement summary.
 *
 * Called on connect so mobile clients receive last vitals/glucose/temp/HOMA-IR
 * as soon as they subscribe to the notify characteristics.
 */
void ble_gatt_seed_from_last_results(void);

/**
 * @brief Notify proximity status
 * @param status Proximity status data
 * @return 0 on success, negative errno on failure
 */
int ble_notify_proximity_status(const struct ble_proximity_status *status);

/**
 * @brief Notify WiFi status change
 * @param status WiFi status data
 * @return 0 on success, negative errno on failure
 */
int ble_notify_wifi_status(const struct ble_wifi_status *status);

/**
 * @brief Legacy compatibility: notify generic sensor data
 * @param data Pointer to sensor data structure
 * @param len Length of data in bytes
 * @return 0 on success, negative errno on failure
 */
int ble_notify_sensor_data(const void *data, size_t len);

/**
 * @brief Notify measurement status update
 * @param status Current measurement status
 * @return 0 on success, negative errno on failure
 */
int ble_notify_meas_status(const struct ble_meas_status *status);

/**
 * @brief Notify accelerometer sample (requires CONFIG_BLE_ACCEL_STREAMING)
 * @param sample Accelerometer XYZ sample
 * @return 0 on success, negative errno on failure
 */
int ble_notify_accel_sample(const struct ble_accel_sample *sample);

/**
 * @brief Notify PPG raw sample (requires CONFIG_BLE_PPG_STREAMING)
 * @param sample Raw PPG + accelerometer sample
 * @return 0 on success, negative errno on failure
 */
int ble_notify_ppg_sample(const struct ble_ppg_sample *sample);

/**
 * @brief Register measurement control callback
 * @param cb Callback invoked when client writes measurement control characteristic
 */
void ble_gatt_register_meas_ctrl_cb(ble_meas_ctrl_cb cb);

/**
 * @brief Register PMIC control callback
 * @param cb Callback invoked when client writes PMIC control characteristic
 */
void ble_gatt_register_pmic_ctrl_cb(ble_pmic_ctrl_cb cb);

/**
 * @brief Get PPG stream decimation factor (default 3, configurable via BLE)
 * @return Decimation factor: send 1 out of every N PPG samples over BLE
 */
uint8_t ble_gatt_get_ppg_decimate(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_GATT_H */
