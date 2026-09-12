"""
HCM BLE Protocol Definitions
============================
UUID constants and struct packers for the HCM wearable firmware GATT profile.

Protocol version: 2.0 (sensor_all uses vitals+proximity layout; see BLE_GATT_PROTOCOL_VERSION)

Base UUID: 12345678-1234-5678-1234-56789abcXXXX
Services:
  - Wearable Config Service  (def0)
  - Sensor Data Service      (def1)
  - WiFi Config Service      (def2)
"""

import struct
from dataclasses import dataclass
from typing import Optional

PROTOCOL_VERSION_MAJOR = 3
PROTOCOL_VERSION_MINOR = 2

BLE_SECURITY_PROFILE_OPEN = 0
BLE_SECURITY_PROFILE_SECURE = 1

# Mirrors enum ble_pairing_state in src/ble/ble_gatt.h (CHRC f016 byte 0).
BLE_PAIRING_IDLE = 0
BLE_PAIRING_WAITING_PASSKEY = 1
BLE_PAIRING_CONFIRM_PASSKEY = 2
BLE_PAIRING_BONDING = 3
BLE_PAIRING_COMPLETE = 4
BLE_PAIRING_FAILED = 5

# ---------------------------------------------------------------------------
# UUID helpers
# ---------------------------------------------------------------------------
_BASE = "12345678-1234-5678-1234-56789abc{:04x}"


def _uuid(chrc_id: int) -> str:
    return _BASE.format(chrc_id)


# ---------------------------------------------------------------------------
# Service UUIDs
# ---------------------------------------------------------------------------
SVC_WEARABLE_CONFIG = _BASE.format(0xDEF0)
SVC_SENSOR_DATA     = _BASE.format(0xDEF1)
SVC_WIFI_CONFIG     = _BASE.format(0xDEF2)

# ---------------------------------------------------------------------------
# Wearable Config Service characteristics  (0xf0XX)
# ---------------------------------------------------------------------------
CHRC_DEVICE_NAME    = _uuid(0xF001)   # R/W  UTF-8 string, max 32 bytes
CHRC_RTC_TRIM       = _uuid(0xF003)   # R/W  int32_t PPM, range ±200
CHRC_BATTERY_LOW    = _uuid(0xF004)   # R/W  uint16_t mV, range 2500-3500
CHRC_BRIGHTNESS     = _uuid(0xF005)   # R/W  uint8_t 0-100 %
CHRC_VOLUME         = _uuid(0xF006)   # R/W  uint8_t 0-100 %
CHRC_PPG_PREF       = _uuid(0xF007)   # R/W  uint8_t enum (0=unset, 1=auto, 2=max86141, 3=max3010x)
CHRC_SECURITY_PROFILE = _uuid(0xF008) # R    uint8_t 0=open 1=secure (pair required for health data)
CHRC_DEVICE_ID        = _uuid(0xF015) # R    UTF-8 hwinfo hex (first 8 bytes), read-only
CHRC_PAIRING_STATUS   = _uuid(0xF016) # R    struct ble_pairing_status (state u8 + passkey u32)
CHRC_MEAS_CTRL      = _uuid(0xF010)   # W    [cmd, type]
CHRC_MEAS_STATUS    = _uuid(0xF011)   # Notify struct ble_meas_status
CHRC_PPG_DECIMATE   = _uuid(0xF012)   # R/W  uint8_t 1-33
CHRC_PMIC_CTRL      = _uuid(0xF013)   # W    [cmd, target, value_le16?]
CHRC_ADMIN_CTRL     = _uuid(0xF014)   # W    [cmd]; requires encrypted link

# Bluetooth SIG Current Time Service (0x1805) — calendar date/time/day-of-week
SVC_CURRENT_TIME    = "00001805-0000-1000-8000-00805f9b34fb"
CHRC_CURRENT_TIME   = "00002a2b-0000-1000-8000-00805f9b34fb"  # R/W/Notify 10-byte Current Time

# ---------------------------------------------------------------------------
# Sensor Data Service characteristics  (0xf1XX)
# ---------------------------------------------------------------------------
CHRC_PMIC           = _uuid(0xF101)   # Notify struct ble_pmic_data
CHRC_TEMPERATURE    = _uuid(0xF102)   # Notify struct ble_temperature_data
# CHRC_SPO2 / CHRC_HR (0xF103/0xF104) REMOVED from firmware - merged into CHRC_VITALS (0xF10B)
CHRC_VITALS         = _uuid(0xF10B)   # Notify struct ble_vitals_data (HR+SpO2+Hb+RespRate unified)
CHRC_GLUCOSE        = _uuid(0xF105)   # Notify struct ble_glucose_data
CHRC_GLUCOSE_SAMPLE = _uuid(0xF107)   # Notify struct ble_glucose_sample_data
CHRC_GLUCOSE_ALGO   = _uuid(0xF108)   # Notify struct ble_glucose_algo_data
CHRC_PMIC_EXT       = _uuid(0xF109)   # Notify struct ble_pmic_ext_data
CHRC_PROXIMITY      = _uuid(0xF10A)   # R/Notify struct ble_proximity_status
CHRC_ACCEL_STREAM   = _uuid(0xF10C)   # Notify struct ble_accel_sample (subscribe-only)
CHRC_PPG_STREAM     = _uuid(0xF106)   # Notify struct ble_ppg_sample
CHRC_SENSOR_ALL     = _uuid(0xF1FF)   # Notify combined packet (protocol v2)

# ---------------------------------------------------------------------------
# WiFi Config Service characteristics  (0xf2XX)
# ---------------------------------------------------------------------------
CHRC_WIFI_ENABLE    = _uuid(0xF201)   # R/W  uint8_t
CHRC_WIFI_SSID      = _uuid(0xF202)   # R/W  UTF-8 string max 32
CHRC_WIFI_PASSWORD  = _uuid(0xF203)   # W    UTF-8 string max 64
CHRC_WIFI_STATUS    = _uuid(0xF204)   # R/Notify struct ble_wifi_status
CHRC_WIFI_CONNECT   = _uuid(0xF205)   # W    uint8_t 1=connect 0=disconnect
CHRC_WIFI_BULK_CTRL = _uuid(0xF206)   # W    bulk session START/ACK/ABORT (WIFI feature)
CHRC_WIFI_BULK_STAT = _uuid(0xF207)   # R/N  bulk session status 18 B (WIFI feature)

# Nordic SMP / MCUmgr DFU characteristic (requires authenticated link in firmware).
CHRC_SMP            = "da2e7828-fbce-4e01-ae9e-261174997c48"

# GATT characteristics that require an encrypted (bonded) link — OPEN profile baseline.
# Reading these while unpaired on Windows can start a hidden OS pairing transaction
# that wedges explicit MITM pair_async (OPERATION_ALREADY_IN_PROGRESS).
ENCRYPTED_BLE_CHARACTERISTICS_OPEN = frozenset({
    CHRC_WIFI_SSID.lower(),
    CHRC_WIFI_PASSWORD.lower(),
    CHRC_PMIC_CTRL.lower(),
    CHRC_ADMIN_CTRL.lower(),
    CHRC_SMP.lower(),
})

# Additional L2 encrypted characteristics when CONFIG_BLE_SECURITY_PROFILE_SECURE=y
# Note: brightness (f005) and volume (f006) stay L1 OPEN per ble_gatt_security.h.
ENCRYPTED_BLE_CHARACTERISTICS_SECURE_EXTRA = frozenset({
    CHRC_DEVICE_NAME.lower(),
    CHRC_RTC_TRIM.lower(),
    CHRC_BATTERY_LOW.lower(),
    CHRC_PPG_PREF.lower(),
    CHRC_PPG_DECIMATE.lower(),
    CHRC_MEAS_CTRL.lower(),
    CHRC_WIFI_ENABLE.lower(),
    CHRC_WIFI_CONNECT.lower(),
    CHRC_WIFI_STATUS.lower(),
    CHRC_PMIC.lower(),
    CHRC_TEMPERATURE.lower(),
    CHRC_PMIC_EXT.lower(),
    CHRC_PROXIMITY.lower(),
    CHRC_ACCEL_STREAM.lower(),
})

# L3 authenticated (MITM) — SECURE profile health + credential chars
AUTHENTICATED_BLE_CHARACTERISTICS = frozenset({
    CHRC_WIFI_SSID.lower(),
    CHRC_WIFI_PASSWORD.lower(),
    CHRC_PMIC_CTRL.lower(),
    CHRC_ADMIN_CTRL.lower(),
    CHRC_SMP.lower(),
    CHRC_VITALS.lower(),
    CHRC_GLUCOSE.lower(),
    CHRC_GLUCOSE_SAMPLE.lower(),
    CHRC_GLUCOSE_ALGO.lower(),
    CHRC_PPG_STREAM.lower(),
    CHRC_SENSOR_ALL.lower(),
})

# Backward-compatible alias (OPEN profile set)
ENCRYPTED_BLE_CHARACTERISTICS = ENCRYPTED_BLE_CHARACTERISTICS_OPEN


def encrypted_characteristics_for_profile(profile: int) -> frozenset:
    """Return UUID set to skip on unpaired Windows GATT reads for the given profile."""
    if profile == BLE_SECURITY_PROFILE_SECURE:
        return ENCRYPTED_BLE_CHARACTERISTICS_OPEN | ENCRYPTED_BLE_CHARACTERISTICS_SECURE_EXTRA
    return ENCRYPTED_BLE_CHARACTERISTICS_OPEN


# Notify characteristics requiring MITM bond in SECURE profile
HEALTH_NOTIFY_CHARACTERISTICS = (
    CHRC_VITALS,
    CHRC_GLUCOSE,
    CHRC_GLUCOSE_SAMPLE,
    CHRC_GLUCOSE_ALGO,
    CHRC_PPG_STREAM,
    CHRC_SENSOR_ALL,
)

# Always subscribable without MITM (even in SECURE profile)
PUBLIC_NOTIFY_CHARACTERISTICS = (
    CHRC_MEAS_STATUS,
)

# ---------------------------------------------------------------------------
# Measurement control commands / types
# ---------------------------------------------------------------------------
MEAS_CMD_START      = 0x01
MEAS_CMD_STOP       = 0x02
MEAS_CMD_RESET_SYNC = 0x03
MEAS_TYPE_HR        = 0x01  # legacy, still accepted by firmware
MEAS_TYPE_SPO2      = 0x02  # legacy, still accepted by firmware
MEAS_TYPE_GLUCOSE   = 0x03
MEAS_TYPE_VITALS    = 0x04  # unified HR+SpO2+Hb+RespRate
MEAS_TYPE_ACCEL     = 0x05  # DEPRECATED: use CHRC_ACCEL_STREAM subscribe-only

# PMIC control commands / targets
PMIC_CMD_ENABLE      = 0x01
PMIC_CMD_DISABLE     = 0x02
PMIC_CMD_SET_VOLTAGE = 0x03
PMIC_TARGET_BK1      = 0x01
PMIC_TARGET_BK2      = 0x02
PMIC_TARGET_BK3      = 0x03
PMIC_TARGET_BBOUT    = 0x04

# Admin control commands
ADMIN_CMD_DELETE_BONDS = 0x01  # Delete bond for current peer and disconnect

# ---------------------------------------------------------------------------
# Friendly-name registry for the GATT service explorer
# ---------------------------------------------------------------------------
# Maps lower-case UUIDs to human-readable names so the explorer can show
# "Vitals (HR+SpO2+Hb)" instead of a raw 128-bit UUID, like nRF Connect.
# Includes our custom services/characteristics, the Nordic SMP (DFU) service,
# and the most common adopted SIG 16-bit UUIDs.
GATT_NAMES = {
    # --- Custom services ---
    SVC_WEARABLE_CONFIG.lower(): "Wearable Config Service",
    SVC_SENSOR_DATA.lower():     "Sensor Data Service",
    SVC_WIFI_CONFIG.lower():     "WiFi Config Service",

    # --- Wearable Config characteristics ---
    CHRC_DEVICE_NAME.lower():    "Device Name",
    CHRC_RTC_TRIM.lower():       "RTC Trim (PPM)",
    CHRC_BATTERY_LOW.lower():    "Battery Low Threshold",
    CHRC_BRIGHTNESS.lower():     "Display Brightness",
    CHRC_VOLUME.lower():         "Buzzer Volume",
    CHRC_PPG_PREF.lower():       "PPG Sensor Preference",
    CHRC_SECURITY_PROFILE.lower(): "Security Profile",
    CHRC_DEVICE_ID.lower():        "Hardware Device ID",
    CHRC_PAIRING_STATUS.lower():   "Pairing Status",
    CHRC_MEAS_CTRL.lower():      "Measurement Control",
    CHRC_MEAS_STATUS.lower():    "Measurement Status",
    CHRC_PPG_DECIMATE.lower():   "PPG Stream Decimation",
    CHRC_PMIC_CTRL.lower():      "PMIC Control",
    CHRC_ADMIN_CTRL.lower():     "Admin Control",
    SVC_CURRENT_TIME.lower():    "Current Time Service",
    CHRC_CURRENT_TIME.lower():   "Current Time",

    # --- Sensor Data characteristics ---
    CHRC_PMIC.lower():           "PMIC Telemetry",
    CHRC_TEMPERATURE.lower():    "Temperature",
    CHRC_VITALS.lower():         "Vitals (HR+SpO2+Hb+Resp)",
    CHRC_GLUCOSE.lower():        "Glucose",
    CHRC_GLUCOSE_SAMPLE.lower(): "Glucose Raw Sample",
    CHRC_GLUCOSE_ALGO.lower():   "Glucose Algorithm",
    CHRC_PMIC_EXT.lower():       "PMIC Extended",
    CHRC_PROXIMITY.lower():      "Proximity / Wear State",
    CHRC_ACCEL_STREAM.lower():   "Accelerometer Stream",
    CHRC_PPG_STREAM.lower():     "PPG Raw Stream",
    CHRC_SENSOR_ALL.lower():     "Combined Sensor Packet",

    # --- WiFi Config characteristics ---
    CHRC_WIFI_ENABLE.lower():    "WiFi Enable",
    CHRC_WIFI_SSID.lower():      "WiFi SSID",
    CHRC_WIFI_PASSWORD.lower():  "WiFi Password",
    CHRC_WIFI_STATUS.lower():    "WiFi Status",
    CHRC_WIFI_CONNECT.lower():   "WiFi Connect",
    CHRC_WIFI_BULK_CTRL.lower(): "WiFi Bulk Session Control",
    CHRC_WIFI_BULK_STAT.lower(): "WiFi Bulk Session Status",

    # --- Nordic SMP / MCUmgr (BLE DFU) ---
    "8d53dc1d-1db7-4cd3-868b-8a527460aa84": "SMP Service (BLE DFU)",
    "da2e7828-fbce-4e01-ae9e-261174997c48": "SMP Characteristic",

    # --- Adopted SIG services (16-bit, normalised to full 128-bit base) ---
    "00001800-0000-1000-8000-00805f9b34fb": "Generic Access",
    "00001801-0000-1000-8000-00805f9b34fb": "Generic Attribute",
    "0000180a-0000-1000-8000-00805f9b34fb": "Device Information",
    "0000180f-0000-1000-8000-00805f9b34fb": "Battery Service",

    # --- Adopted SIG characteristics ---
    "00002a00-0000-1000-8000-00805f9b34fb": "Device Name",
    "00002a01-0000-1000-8000-00805f9b34fb": "Appearance",
    "00002a05-0000-1000-8000-00805f9b34fb": "Service Changed",
    "00002a19-0000-1000-8000-00805f9b34fb": "Battery Level",
    "00002a29-0000-1000-8000-00805f9b34fb": "Manufacturer Name",
    "00002a24-0000-1000-8000-00805f9b34fb": "Model Number",
    "00002a26-0000-1000-8000-00805f9b34fb": "Firmware Revision",
    "00002a28-0000-1000-8000-00805f9b34fb": "Software Revision",

    # --- Common descriptors ---
    "00002902-0000-1000-8000-00805f9b34fb": "Client Characteristic Config (CCCD)",
    "00002901-0000-1000-8000-00805f9b34fb": "Characteristic User Description",
}


def gatt_name(uuid: str, fallback: str = "") -> str:
    """Resolve a friendly name for a UUID, falling back to the bleak
    description or the short 16-bit form of the UUID."""
    name = GATT_NAMES.get(str(uuid).lower())
    if name:
        return name
    if fallback and fallback not in ("Unknown", "Vendor specific"):
        return fallback
    # Show the short 16-bit id for adopted UUIDs (xxxx in 0000xxxx-...base)
    u = str(uuid).lower()
    if u.endswith("-0000-1000-8000-00805f9b34fb") and u.startswith("0000"):
        return f"0x{u[4:8].upper()}"
    return "Custom"


# ---------------------------------------------------------------------------
# Struct packers (little-endian, packed, matching firmware __packed structs)
# ---------------------------------------------------------------------------

# struct ble_pmic_data  (10 bytes): battery_mv u16, current_ma i16, soc%, charger, buck1 u16, buck2 u16
_PMIC = struct.Struct("<HhBBHH")

# struct ble_pmic_ext_data (28 bytes):
# buck3_mv u16, bbout_mv u16,
# buck1_en u8, buck2_en u8, buck3_en u8, bbout_en u8,
# charge_voltage_mv u16, charge_current_ma u16,
# battery_temp_c i16, cycle_count u16, remaining_mah u16, full_mah u16,
# design_mah u16, time_to_empty_min u16, time_to_full_min u16,
# avg_current_ma i16
_PMIC_EXT = struct.Struct("<HHBBBBHHhHHHHHHh")

# struct ble_temperature_data  (6 bytes): temp_c_x100 i16, timestamp u32
_TEMP = struct.Struct("<hI")

# struct ble_spo2_data  (7 bytes): kept for SENSOR_ALL aggregate decode
_SPO2 = struct.Struct("<BBBI")

# struct ble_hr_data  (8 bytes): kept for SENSOR_ALL aggregate decode
_HR = struct.Struct("<HBBI")

# struct ble_vitals_data (18 bytes): hr_bpm u16, hr_conf u8, spo2_pct u8, spo2_conf u8,
# hb_x10 u16, hb_conf u8, resp_bpm u8, resp_conf u8, r_val_x1000 u16, quality u8, flags u8, ts u32
# flags: hr_valid:BIT(0), spo2_valid:BIT(1), hb_valid:BIT(2), resp_valid:BIT(3), finger_on:BIT(4)
_VITALS = struct.Struct("<HBBBHBBBHBBI")

# struct ble_glucose_data  (7 bytes): glucose_mg_dl u16, quality u8, timestamp u32
_GLUCOSE = struct.Struct("<HBI")

# struct ble_glucose_algo_data (124 bytes):
# 26 floats + 5 int32s matching ble_glucose_algo_data __packed
# Field order matches ble_gatt.h exactly:
#  tot_coeff, intercept, y1, avg, std, up_lim, ll_lim,
#  p_count(i), n_count(i),
#  p_val, n_val, p_plus_n, y2_val, y2_pct,
#  group_cd(i), y2_factor, y2_fv, const_val, y3_value,
#  y3_row_no(i), elim_per, elim_val,
#  y_value(i),
#  calib_factor, ag_adjusted, norm_glucose,
#  actual_insulin, insulin_corr, insulin_ratio, inverse_ratio, homa_ir
# 31 fields × 4 bytes = 124 bytes  (matches ble_glucose_algo_data __packed)
# f×7  i×2  f×5  i f  f×3  i  f×2  i  f×8
_GLUCOSE_ALGO = struct.Struct("<fffffffii fffff if fff i ff i ffffffff")

# struct ble_glucose_sample_data (12 bytes): sample u16, total u16, raw u16,
# voltage_mv_x100 i16, timestamp u32
_GLUCOSE_SAMPLE = struct.Struct("<HHHhI")

# struct ble_wifi_status  (38 bytes): connected u8, rssi i8, ip[4], ssid[32]
_WIFI_STATUS = struct.Struct("<Bb4s32s")

# struct ble_meas_status  (8 bytes): active u8, type u8, pct_complete u8, quality u8, taken u16, target u16
_MEAS_STATUS = struct.Struct("<BBBBHH")

# struct ble_proximity_status (10 bytes): contact u8, wear_state u8, raw u16, filt u16, ts u32
_PROXIMITY = struct.Struct("<BBHHI")

# struct ble_ppg_sample  (24 bytes): sample_num u16, ir u32, red u32, green u32, ax i16, ay i16, az i16, ts u32
_PPG_SAMPLE = struct.Struct("<HIII hhh I")

# struct ble_accel_sample  (12 bytes): seq u16, x_mg i16, y_mg i16, z_mg i16, timestamp_ms u32
_ACCEL_SAMPLE = struct.Struct("<HhhhI")

# ---------------------------------------------------------------------------
# Decoded dataclass types
# ---------------------------------------------------------------------------

@dataclass
class PairingStatus:
    state: int
    passkey: int


@dataclass
class PmicData:
    battery_mv: int
    current_ma: int
    soc_percent: int
    charger_status: int
    buck1_mv: int
    buck2_mv: int


@dataclass
class PmicExtData:
    buck3_mv: int
    bbout_mv: int
    buck1_enabled: int
    buck2_enabled: int
    buck3_enabled: int
    bbout_enabled: int
    charge_voltage_mv: int
    charge_current_ma: int
    battery_temp_c: int
    cycle_count: int
    remaining_mah: int
    full_mah: int
    design_mah: int
    time_to_empty_min: int
    time_to_full_min: int
    avg_current_ma: int


@dataclass
class TemperatureData:
    temp_c: float
    timestamp: int


@dataclass
class SpO2Data:
    spo2_percent: int
    confidence: int
    finger_detected: bool
    timestamp: int


@dataclass
class HrData:
    hr_bpm: int
    confidence: int
    finger_detected: bool
    timestamp: int


@dataclass
class VitalsData:
    hr_bpm: int
    hr_confidence: int
    spo2_percent: int
    spo2_confidence: int
    hb_g_dl_x10: int       # Hemoglobin g/dL * 10  (e.g. 135 = 13.5 g/dL)
    hb_confidence: int
    resp_rate_bpm: int
    resp_confidence: int
    r_value_x1000: int     # SpO2 R-value * 1000
    quality: int
    flags: int             # BIT(0)=hr_valid, BIT(1)=spo2_valid, BIT(2)=hb_valid, BIT(3)=resp_valid, BIT(4)=finger_on
    timestamp: int

    @property
    def hr_valid(self) -> bool:
        return bool(self.flags & 0x01)

    @property
    def spo2_valid(self) -> bool:
        return bool(self.flags & 0x02)

    @property
    def hb_valid(self) -> bool:
        return bool(self.flags & 0x04)

    @property
    def resp_valid(self) -> bool:
        return bool(self.flags & 0x08)

    @property
    def finger_on(self) -> bool:
        return bool(self.flags & 0x10)

    @property
    def hb_g_dl(self) -> float:
        return round(self.hb_g_dl_x10 / 10.0, 1)


@dataclass
class GlucoseData:
    glucose_mg_dl: int
    quality: int
    timestamp: int

    @property
    def glucose_mmol_l(self) -> float:
        return round(self.glucose_mg_dl / 18.0182, 2)


@dataclass
class GlucoseSampleData:
    sample_number: int
    total_samples: int
    raw_adc_value: int
    voltage_mv: float
    timestamp: int


@dataclass
class GlucoseAlgoData:
    tot_coeff: float
    intercept: float
    y1_value: float
    avg_val: float
    std_dev: float
    up_lim: float
    ll_lim: float
    p_count: int
    n_count: int
    p_val: float
    n_val: float
    p_plus_n: float
    y2_val: float
    y2_percent: float
    group_cd: int
    y2_factor: float
    y2_factor_val: float
    const_val: float
    y3_value: float
    y3_row_no: int
    elim_per: float
    elim_val: float
    y_value: int
    calibration_factor: float
    ag_adjusted: float
    normalized_glucose: float
    actual_insulin: float
    insulin_correction: float
    insulin_ratio: float
    inverse_ratio: float
    homa_ir_index: float


@dataclass
class WifiStatus:
    connected: int      # 0=disconnected, 1=connecting, 2=connected
    rssi_dbm: int
    ip_addr: str
    ssid: str


@dataclass
class MeasStatus:
    active: bool
    meas_type: int
    percent_complete: int
    quality: int
    samples_taken: int
    samples_target: int


@dataclass
class ProximityStatus:
    contact: bool
    wear_state: int
    proximity_raw: int
    proximity_filt: int
    timestamp: int


@dataclass
class PpgSample:
    sample_num: int
    raw_ir: int
    raw_red: int
    raw_green: int
    accel_x: int
    accel_y: int
    accel_z: int
    timestamp_ms: int


@dataclass
class AccelSample:
    seq: int
    x_mg: int
    y_mg: int
    z_mg: int
    timestamp_ms: int

    @property
    def magnitude_mg(self) -> float:
        import math
        return round(math.sqrt(self.x_mg**2 + self.y_mg**2 + self.z_mg**2), 1)


@dataclass
class SensorAllData:
    """Decoded ble_sensor_all_data (0xF1FF) — protocol v2 aggregate."""
    pmic: PmicData
    temperature: TemperatureData
    vitals: VitalsData
    glucose: GlucoseData
    proximity: ProximityStatus


# ---------------------------------------------------------------------------
# Decode functions
# ---------------------------------------------------------------------------

def decode_pmic(data: bytes) -> Optional[PmicData]:
    if len(data) < _PMIC.size:
        return None
    batt, cur, soc, chg, b1, b2 = _PMIC.unpack_from(data)
    return PmicData(batt, cur, soc, chg, b1, b2)


def decode_pmic_ext(data: bytes) -> Optional[PmicExtData]:
    if len(data) < _PMIC_EXT.size:
        return None
    (
        buck3_mv, bbout_mv,
        buck1_enabled, buck2_enabled, buck3_enabled, bbout_enabled,
        charge_voltage_mv, charge_current_ma,
        battery_temp_c, cycle_count, remaining_mah, full_mah,
        design_mah, time_to_empty_min, time_to_full_min,
        avg_current_ma,
    ) = _PMIC_EXT.unpack_from(data)
    return PmicExtData(
        buck3_mv=buck3_mv,
        bbout_mv=bbout_mv,
        buck1_enabled=buck1_enabled,
        buck2_enabled=buck2_enabled,
        buck3_enabled=buck3_enabled,
        bbout_enabled=bbout_enabled,
        charge_voltage_mv=charge_voltage_mv,
        charge_current_ma=charge_current_ma,
        battery_temp_c=battery_temp_c,
        cycle_count=cycle_count,
        remaining_mah=remaining_mah,
        full_mah=full_mah,
        design_mah=design_mah,
        time_to_empty_min=time_to_empty_min,
        time_to_full_min=time_to_full_min,
        avg_current_ma=avg_current_ma,
    )


def decode_temperature(data: bytes) -> Optional[TemperatureData]:
    if len(data) < _TEMP.size:
        return None
    raw, ts = _TEMP.unpack_from(data)
    return TemperatureData(round(raw / 100.0, 2), ts)


def decode_spo2(data: bytes) -> Optional[SpO2Data]:
    if len(data) < _SPO2.size:
        return None
    pct, conf, finger, ts = _SPO2.unpack_from(data)
    return SpO2Data(pct, conf, bool(finger), ts)


def decode_hr(data: bytes) -> Optional[HrData]:
    if len(data) < _HR.size:
        return None
    bpm, conf, finger, ts = _HR.unpack_from(data)
    return HrData(bpm, conf, bool(finger), ts)


def decode_vitals(data: bytes) -> Optional[VitalsData]:
    if len(data) < _VITALS.size:
        return None
    (hr_bpm, hr_conf, spo2_pct, spo2_conf, hb_x10, hb_conf,
     resp_bpm, resp_conf, r_val, quality, flags, ts) = _VITALS.unpack_from(data)
    return VitalsData(hr_bpm, hr_conf, spo2_pct, spo2_conf, hb_x10, hb_conf,
                      resp_bpm, resp_conf, r_val, quality, flags, ts)


def decode_glucose(data: bytes) -> Optional[GlucoseData]:
    if len(data) < _GLUCOSE.size:
        return None
    mgdl, qual, ts = _GLUCOSE.unpack_from(data)
    return GlucoseData(mgdl, qual, ts)


def decode_glucose_sample(data: bytes) -> Optional[GlucoseSampleData]:
    if len(data) < _GLUCOSE_SAMPLE.size:
        return None
    snum, total, raw, mv_x100, ts = _GLUCOSE_SAMPLE.unpack_from(data)
    return GlucoseSampleData(snum, total, raw, mv_x100 / 100.0, ts)


def decode_glucose_algo(data: bytes) -> Optional[GlucoseAlgoData]:
    if len(data) < _GLUCOSE_ALGO.size:
        return None
    (
        tot_coeff, intercept, y1, avg, std, up, ll,
        p_count, n_count,
        p_val, n_val, p_plus_n, y2_val, y2_pct,
        group_cd, y2_factor, y2_fv, const_val, y3_value,
        y3_row_no, elim_per, elim_val,
        y_value,
        calib, ag, norm, insulin, corr, ratio, inv, homa,
    ) = _GLUCOSE_ALGO.unpack_from(data)
    return GlucoseAlgoData(
        tot_coeff=tot_coeff, intercept=intercept, y1_value=y1,
        avg_val=avg, std_dev=std, up_lim=up, ll_lim=ll,
        p_count=p_count, n_count=n_count,
        p_val=p_val, n_val=n_val, p_plus_n=p_plus_n,
        y2_val=y2_val, y2_percent=y2_pct,
        group_cd=group_cd, y2_factor=y2_factor, y2_factor_val=y2_fv,
        const_val=const_val, y3_value=y3_value,
        y3_row_no=y3_row_no, elim_per=elim_per, elim_val=elim_val,
        y_value=y_value,
        calibration_factor=calib, ag_adjusted=ag, normalized_glucose=norm,
        actual_insulin=insulin, insulin_correction=corr,
        insulin_ratio=ratio, inverse_ratio=inv, homa_ir_index=homa,
    )


def decode_wifi_status(data: bytes) -> Optional[WifiStatus]:
    if len(data) < _WIFI_STATUS.size:
        return None
    conn, rssi, ip_raw, ssid_raw = _WIFI_STATUS.unpack_from(data)
    ip = ".".join(str(b) for b in ip_raw)
    ssid = ssid_raw.rstrip(b"\x00").decode("utf-8", errors="replace")
    return WifiStatus(conn, rssi, ip, ssid)


def decode_meas_status(data: bytes) -> Optional[MeasStatus]:
    if len(data) < _MEAS_STATUS.size:
        return None
    active, mtype, pct, qual, taken, target = _MEAS_STATUS.unpack_from(data)
    return MeasStatus(bool(active), mtype, pct, qual, taken, target)


def decode_proximity_status(data: bytes) -> Optional[ProximityStatus]:
    if len(data) < _PROXIMITY.size:
        return None
    contact, wear_state, raw, filt, ts = _PROXIMITY.unpack_from(data)
    return ProximityStatus(bool(contact), wear_state, raw, filt, ts)


def decode_ppg_sample(data: bytes) -> Optional[PpgSample]:
    if len(data) < _PPG_SAMPLE.size:
        return None
    snum, ir, red, green, ax, ay, az, ts = _PPG_SAMPLE.unpack_from(data)
    return PpgSample(snum, ir, red, green, ax, ay, az, ts)


def decode_accel_sample(data: bytes) -> Optional[AccelSample]:
    if len(data) < _ACCEL_SAMPLE.size:
        return None
    seq, x_mg, y_mg, z_mg, ts_ms = _ACCEL_SAMPLE.unpack_from(data)
    return AccelSample(seq=seq, x_mg=x_mg, y_mg=y_mg, z_mg=z_mg, timestamp_ms=ts_ms)


# ble_sensor_all_data v2: pmic(10) + temp(6) + vitals(18) + glucose(7) + proximity(10) = 51 bytes
_SENSOR_ALL_SIZE = _PMIC.size + _TEMP.size + _VITALS.size + _GLUCOSE.size + _PROXIMITY.size


def decode_security_profile(data: bytes) -> Optional[int]:
    if len(data) < 1:
        return None
    val = data[0]
    if val not in (BLE_SECURITY_PROFILE_OPEN, BLE_SECURITY_PROFILE_SECURE):
        return None
    return val


_PAIRING_STATUS = struct.Struct("<BI")


def decode_pairing_status(data: bytes) -> Optional[PairingStatus]:
    """Decode CHRC f016: state u8 + passkey u32 LE."""
    if len(data) < _PAIRING_STATUS.size:
        return None
    state, passkey = _PAIRING_STATUS.unpack_from(data)
    if state > BLE_PAIRING_FAILED:
        return None
    return PairingStatus(state=state, passkey=passkey)


def decode_sensor_all(data: bytes) -> Optional[SensorAllData]:
    if len(data) < _SENSOR_ALL_SIZE:
        return None
    off = 0
    pmic = decode_pmic(data[off:]);              off += _PMIC.size
    temp = decode_temperature(data[off:]);       off += _TEMP.size
    vitals = decode_vitals(data[off:]);          off += _VITALS.size
    gluc = decode_glucose(data[off:]);            off += _GLUCOSE.size
    prox = decode_proximity_status(data[off:])
    if None in (pmic, temp, vitals, gluc, prox):
        return None
    return SensorAllData(pmic, temp, vitals, gluc, prox)

def encode_meas_ctrl(cmd: int, meas_type: int = 0) -> bytes:
    """Encode measurement control command [cmd, type]."""
    return bytes([cmd & 0xFF, meas_type & 0xFF])


def encode_pmic_ctrl(cmd: int, target: int, value_mv: Optional[int] = None) -> bytes:
    """Encode PMIC control command [cmd, target, value_le16?]."""
    if value_mv is None:
        return bytes([cmd & 0xFF, target & 0xFF])
    return struct.pack("<BBH", cmd & 0xFF, target & 0xFF, int(value_mv) & 0xFFFF)


def encode_current_time(dt=None, reason: int = 0x02) -> bytes:
    """Encode Bluetooth SIG Current Time (0x2A2B), 10 bytes.

    Uses local wall-clock fields. Day-of-week: 1=Monday … 7=Sunday (ISO).
    *reason* is the CTS adjust-reason byte (default: external reference).
    """
    from datetime import datetime

    if dt is None:
        dt = datetime.now()
    wday = dt.isoweekday()  # 1=Mon … 7=Sun
    return struct.pack(
        "<HBBBBBBBB",
        int(dt.year) & 0xFFFF,
        int(dt.month) & 0xFF,
        int(dt.day) & 0xFF,
        int(dt.hour) & 0xFF,
        int(dt.minute) & 0xFF,
        int(dt.second) & 0xFF,
        wday & 0xFF,
        0,  # fractions256
        int(reason) & 0xFF,
    )


def encode_rtc_trim(ppm: int) -> bytes:
    """Encode signed PPM trim as little-endian int32."""
    return struct.pack("<i", ppm)


def encode_battery_low(threshold_mv: int) -> bytes:
    """Encode battery low threshold as little-endian uint16."""
    return struct.pack("<H", threshold_mv)


def encode_uint8(val: int) -> bytes:
    return bytes([val & 0xFF])


def encode_string(s: str, max_len: int) -> bytes:
    encoded = s.encode("utf-8")[:max_len]
    return encoded


MEAS_TYPE_NAMES = {
    MEAS_TYPE_HR: "HR",
    MEAS_TYPE_SPO2: "SpO2",
    MEAS_TYPE_GLUCOSE: "Glucose",
    MEAS_TYPE_VITALS: "Vitals",
    MEAS_TYPE_ACCEL: "Accel (deprecated)",
}
