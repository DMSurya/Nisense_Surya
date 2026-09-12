"""
HCM Backend (PySide6 QObject)
==============================
Qt/QML bridge: exposes BLE data as Q_PROPERTYs + signals.
Runs bleak (asyncio) on a separate thread via qasync.
"""

import asyncio
import json
import logging
import platform
import sys
import time
from pathlib import Path
from typing import Optional

# WinRT error code: BLE GATT operations cancelled while stack is re-establishing
# encryption or processing service-changed indications.
_WINERROR_CANCELLED = -2147023673  # 0x800704C7 ERROR_CANCELLED

# WinRT error code: GATT session object invalidated by a 'services changed'
# indication (e.g. firmware updated its service table).  All GATT ops on the
# existing session immediately fail with this; no amount of retrying helps.
_WINERROR_RO_E_CLOSED = -2147483629  # 0x80000013 RO_E_CLOSED


def _is_transient_ble_error(exc: Exception) -> bool:
    """Return True for WinRT/BlueZ errors that can be retried after a short wait."""
    if hasattr(exc, "winerror") and exc.winerror == _WINERROR_CANCELLED:
        return True
    msg = str(exc).lower()
    return "canceled by the user" in msg or "cancelled by the user" in msg


def _is_session_dead_error(exc: Exception) -> bool:
    """Return True when the WinRT GATT session has been permanently invalidated.

    This happens after a 'services changed' indication causes Windows to close
    the underlying GattSession.  Retrying against the same session is pointless;
    a fresh disconnect + reconnect is required.
    """
    if hasattr(exc, "winerror") and exc.winerror == _WINERROR_RO_E_CLOSED:
        return True
    msg = str(exc).lower()
    return "object has been closed" in msg or "session was closed" in msg


from PySide6.QtCore import (
    Property, QObject, Signal, Slot, QThread,
)
from qasync import asyncSlot

from bleak.exc import BleakCharacteristicNotFoundError

from hcm_client import HCMClient, RECONNECT_TIMEOUT, NOTIFY_CHARACTERISTICS
from hcm_logger import LOG_ROOT, SessionLogger
from cloud_gateway import upload_session_files, load_gateway_config
from smp_client import SMPClient, SMPError
from hcm_protocol import (
    CHRC_GLUCOSE, CHRC_GLUCOSE_ALGO, CHRC_GLUCOSE_SAMPLE, CHRC_MEAS_STATUS, CHRC_PMIC,
    CHRC_PMIC_EXT,
    CHRC_PROXIMITY, CHRC_SENSOR_ALL,
    PMIC_TARGET_BBOUT, PMIC_TARGET_BK1, PMIC_TARGET_BK2, PMIC_TARGET_BK3,
    CHRC_PPG_STREAM, CHRC_ACCEL_STREAM, CHRC_TEMPERATURE, CHRC_VITALS, CHRC_WIFI_STATUS,
    MEAS_TYPE_GLUCOSE, MEAS_TYPE_HR, MEAS_TYPE_SPO2, MEAS_TYPE_VITALS,
    BLE_SECURITY_PROFILE_OPEN, BLE_SECURITY_PROFILE_SECURE,
    HEALTH_NOTIFY_CHARACTERISTICS, PUBLIC_NOTIFY_CHARACTERISTICS,
    decode_glucose, decode_glucose_algo, decode_glucose_sample, decode_meas_status, decode_pmic,
    decode_pmic_ext,
    decode_proximity_status,
    decode_ppg_sample, decode_accel_sample, decode_sensor_all, decode_vitals, decode_temperature, decode_wifi_status,
    SensorAllData, VitalsData,
)

log = logging.getLogger(__name__)
KNOWN_DEVICES_FILE = Path(LOG_ROOT) / "known_devices.json"

# Development-friendly reconnect policy: keep background retries alive
# until user disconnects manually or the link is restored.
AUTO_RECONNECT_MAX_ATTEMPTS = 0  # 0 = unlimited retries
AUTO_RECONNECT_BASE_DELAY_S = 2
AUTO_RECONNECT_MAX_DELAY_S = 20

# Auto measurement interval defaults (app-side periodic HR/SpO2 trigger)
MEAS_INTERVAL_ENABLED = False
MEAS_INTERVAL_SEC = 60        # seconds between measurement cycles
MEAS_INTERVAL_TYPE = 0        # 0=HR, 1=SpO2, 2=Both (HR then SpO2)
MEAS_INTERVAL_TIMEOUT_SEC = 90  # per-measurement timeout

# Pairing: optional f016 poll auto-completes PC when watch Accepts first;
# manual PC Accept works immediately (either order, same as nRF Connect).
PAIR_CONFIRM_TIMEOUT_SEC = 90.0
PAIR_WATCH_POLL_INTERVAL_SEC = 0.35


class ScanResult(QObject):
    """Single scan result, exposed to QML."""
    def __init__(self, name: str, address: str, rssi: int, parent=None):
        super().__init__(parent)
        self._name = name
        self._address = address
        self._rssi = rssi

    @Property(str)
    def name(self) -> str: return self._name

    @Property(str)
    def address(self) -> str: return self._address

    @Property(int)
    def rssi(self) -> int: return self._rssi


class HCMBackend(QObject):
    """
    Main backend object exposed to QML as ``backend``.

    All ``@Slot`` methods can be called from QML.
    All ``Signal``s feed into QML property bindings / live charts.
    """

    # ------------------------------------------------------------------
    # Connection signals
    # ------------------------------------------------------------------
    connectionStateChanged = Signal(bool)    # True = connected
    scanResultsChanged     = Signal()
    isScanningChanged      = Signal(bool)
    scanComplete           = Signal(int)     # emits count of found devices
    knownDevicesChanged    = Signal()
    errorOccurred          = Signal(str)
    statusChanged          = Signal(str)
    reconnectPolicyChanged = Signal()
    measIntervalChanged    = Signal()
    isConnectingChanged    = Signal(bool)    # True while a connection attempt is in progress

    # ------------------------------------------------------------------
    # Sensor data signals
    # ------------------------------------------------------------------
    hrUpdated       = Signal(int, int)          # hr_bpm, confidence
    spo2Updated     = Signal(int, int)          # spo2_percent, confidence
    hbUpdated       = Signal(float, int)        # hb_g_dl (float), confidence
    respRateUpdated = Signal(int, int)          # resp_rate_bpm, confidence
    glucoseUpdated  = Signal(int, float, int)   # mg_dl, mmol_l, quality
    glucoseSampleUpdated = Signal(int, int, float, int, int)
    # ^ sample_number, total_samples, voltage_mv, raw_adc_value, timestamp
    tempUpdated     = Signal(float)             # celsius
    pmicUpdated     = Signal(int, int, int, int)# battery_mv, current_ma, soc, charger
    # buck3_mv, bbout_mv, b1_en, b2_en, b3_en, bbout_en,
    # chg_v_mv, chg_i_ma, batt_temp_c, cycles, rem_mah, full_mah,
    # design_mah, tte_min, ttf_min, avg_ma
    pmicExtUpdated  = Signal(int, int, int, int, int, int, int, int,
                             int, int, int, int, int, int, int, int)
    wifiUpdated          = Signal(int, int, str, str)# connected, rssi, ip, ssid
    wifiConnectedChanged  = Signal(int)  # 0=disconnected, 1=connecting, 2=connected
    wifiIpChanged         = Signal(str)
    wifiRssiChanged       = Signal(int)
    measStatusUpdated = Signal(bool, int, int, int)  # active, type, pct, quality
    proximityUpdated = Signal(int, int, int, int, int)  # contact, wear_state, raw, filt, timestamp
    ppgSampleReceived = Signal(int, int, int, int, int, int, int, int)
    # ^ sample_num, ir, red, green, ax, ay, az, ts_ms
    accelUpdated = Signal(int, int, int, int)  # x_mg, y_mg, z_mg, timestamp_ms

    # ------------------------------------------------------------------
    # BLE DFU (MCUmgr SMP) signals
    # ------------------------------------------------------------------
    dfuProgress    = Signal(int, int)   # bytes_sent, total_bytes
    dfuStatus      = Signal(str)        # human-readable status string
    dfuError       = Signal(str)        # error description (empty = none)
    dfuActiveChanged = Signal(bool)     # True while upload is in progress

    # ------------------------------------------------------------------
    # BLE pairing signals (authenticated / numeric-comparison)
    # ------------------------------------------------------------------
    pairingPasskey      = Signal(str)   # 6-digit code to compare against device LCD
    pairingStateChanged = Signal(str)   # "pairing" | "confirm" | "paired" | "failed"

    # ------------------------------------------------------------------
    # GATT explorer signals
    # ------------------------------------------------------------------
    gattTableChanged = Signal()         # gattTableJson property updated
    gattScanningChanged = Signal(bool)  # True while discovery is running
    connInfoChanged  = Signal()         # negotiated MTU / address updated

    # ------------------------------------------------------------------
    # Config signals (read-back from device after connect)
    # ------------------------------------------------------------------
    deviceNameChanged   = Signal(str)
    hardwareDeviceIdChanged = Signal(str)
    rtcTimeChanged      = Signal(int)
    rtcTrimChanged      = Signal(int)
    batteryLowChanged   = Signal(int)
    brightnessChanged   = Signal(int)
    volumeChanged       = Signal(int)
    ppgDecimateChanged  = Signal(int)
    ppgPreferenceChanged = Signal(int)
    securityProfileChanged = Signal(int)
    wifiSsidChanged     = Signal(str)
    patientNameChanged  = Signal(str)
    glucoseLogStatusChanged = Signal()

    def __init__(self, adapter: Optional[str] = None, parent=None) -> None:
        super().__init__(parent)
        self._adapter = adapter
        self._client = HCMClient(adapter=adapter)
        self._client.set_disconnected_callback(self._on_client_disconnected)
        self._logger = SessionLogger()
        self._scan_results: list = []
        self._known_devices: list = []
        self._last_device_address = ""
        self._is_connecting = False
        self._connecting_address = ""
        self._manual_disconnect = False
        self._connect_setup_recovering = False
        self._link_phase = "idle"
        self._auto_reconnect_task: Optional[asyncio.Task] = None
        self._session_teardown_done = False
        self._auto_reconnect_max_attempts = AUTO_RECONNECT_MAX_ATTEMPTS
        self._auto_reconnect_base_delay_s = AUTO_RECONNECT_BASE_DELAY_S
        self._auto_reconnect_max_delay_s = AUTO_RECONNECT_MAX_DELAY_S

        # Auto measurement interval
        self._meas_interval_enabled = MEAS_INTERVAL_ENABLED
        self._meas_interval_sec = MEAS_INTERVAL_SEC
        self._meas_interval_type = MEAS_INTERVAL_TYPE
        self._meas_interval_task: Optional[asyncio.Task] = None
        self._meas_done_event: Optional[asyncio.Event] = None
        self._meas_active_flag = False
        self._last_config_read_exc: Optional[Exception] = None

        # Cached property values
        self._connected = False
        self._is_scanning = False
        self._device_name = ""
        self._hardware_device_id = ""
        self._rtc_time = 0
        self._rtc_trim = 0
        self._battery_low = 3000
        self._brightness = 80
        self._volume = 50
        self._ppg_decimate = 3
        self._ppg_preference = 0
        self._security_profile = BLE_SECURITY_PROFILE_OPEN
        self._is_paired = False
        self._accel_x: int = 0
        self._accel_y: int = 0
        self._accel_z: int = 0
        self._wifi_ssid = ""
        self._wifi_connected = 0
        self._wifi_ip = ""
        self._wifi_rssi = 0
        self._patient_name = ""
        self._csv_write_blocked = False
        self._csv_deferred_rows = 0
        self._csv_write_status = "CSV: Ready"
        # DFU state
        self._dfu_active = False
        self._dfu_task: Optional[asyncio.Task] = None
        # Pairing state (authenticated / numeric-comparison)
        self._pair_confirm_future: Optional[asyncio.Future] = None
        self._pairing_state = "idle"
        self._pairing_lock = asyncio.Lock()
        self._ble_session_lock = asyncio.Lock()
        self._clean_session_active = False
        self._pairing_in_flight = False
        self._link_pairing_clean = False  # True when link only had f008 probe (safe for pair_async)
        self._winrt_pairing_session = None  # WinrtPairingSession on Windows
        self._suppress_auto_reconnect = False  # Block reconnect loop after pairing failure
        self._pending_post_pair_hooks = False
        self._shutting_down = False
        # GATT explorer state
        self._gatt_table_json = "[]"
        self._gatt_scanning = False
        self._gatt_task: Optional[asyncio.Task] = None
        self._conn_mtu = 0
        self._load_known_devices()

    def _set_link_phase(self, phase: str, detail: str = "") -> None:
        """Track connect/pair/DFU stage for disconnect diagnostics."""
        prev = self._link_phase
        self._link_phase = phase
        if detail:
            log.info("link phase: %s → %s — %s", prev, phase, detail)
        elif prev != phase:
            log.info("link phase: %s → %s", prev, phase)

    def _link_context_summary(self) -> str:
        """Compact snapshot when BleakClient is missing."""
        return (
            f"phase={self._link_phase}, "
            f"backend_connected={self._connected}, "
            f"bleak_connected={self._client.is_connected}, "
            f"pairing_in_flight={self._pairing_in_flight}, "
            f"connect_setup={self._connect_setup_recovering}, "
            f"manual_disconnect={self._manual_disconnect}"
        )

    async def _sleep_unless_shutdown(self, seconds: float) -> bool:
        """Sleep in short slices; return False if the app is shutting down."""
        deadline = time.monotonic() + max(0.0, seconds)
        while time.monotonic() < deadline:
            if self._shutting_down:
                return False
            await asyncio.sleep(min(0.5, deadline - time.monotonic()))
        return not self._shutting_down

    def _log_link_drop(self, address: str, *, expected: bool, note: str = "") -> None:
        msg = (
            "BLE link dropped addr=%s expected=%s phase=%s "
            "pairing=%s connect_setup=%s manual=%s backend_connected=%s"
        )
        args = (
            address or "?",
            expected,
            self._link_phase,
            self._pairing_in_flight,
            self._connect_setup_recovering,
            self._manual_disconnect,
            self._connected,
        )
        if note:
            log.info(msg + " — %s", *args, note)
        elif expected:
            log.info(msg + " (recovery continues)", *args)
        else:
            log.warning(msg, *args)

    def _report_async_error(self, action: str, exc: Exception) -> None:
        message = f"{action} failed: {exc}"
        log.warning(message)
        self.errorOccurred.emit(message)
        self.statusChanged.emit(message)

    # ------------------------------------------------------------------
    # QML-readable properties
    # ------------------------------------------------------------------
    @Property(bool, notify=connectionStateChanged)
    def connected(self) -> bool:
        return self._connected

    @Property(str, notify=connectionStateChanged)
    def connectedAddress(self) -> str:
        """Address of the currently connected device, or empty string."""
        return self._last_device_address if self._connected else ""

    @Property(bool, notify=isConnectingChanged)
    def isConnecting(self) -> bool:
        return self._is_connecting

    @Property(str, notify=isConnectingChanged)
    def connectingAddress(self) -> str:
        """Address currently being connected to (non-empty only while isConnecting)."""
        return self._connecting_address

    @Property(str, notify=deviceNameChanged)
    def deviceName(self) -> str:
        return self._device_name

    @Property(str, notify=hardwareDeviceIdChanged)
    def hardwareDeviceId(self) -> str:
        """Hwinfo hex ID from firmware (CSV Device_ID / cloud upload key)."""
        return self._hardware_device_id

    @Property(int, notify=rtcTimeChanged)
    def rtcTime(self) -> int:
        return self._rtc_time

    @Property(int, notify=rtcTrimChanged)
    def rtcTrim(self) -> int:
        return self._rtc_trim

    @Property(int, notify=batteryLowChanged)
    def batteryLow(self) -> int:
        return self._battery_low

    @Property(int, notify=brightnessChanged)
    def brightness(self) -> int:
        return self._brightness

    @Property(int, notify=volumeChanged)
    def volume(self) -> int:
        return self._volume

    @Property(int, notify=ppgDecimateChanged)
    def ppgDecimate(self) -> int:
        return self._ppg_decimate

    @Property(int, notify=ppgPreferenceChanged)
    def ppgPreference(self) -> int:
        return self._ppg_preference

    @Property(int, notify=securityProfileChanged)
    def securityProfile(self) -> int:
        """0=open (unpaired health OK), 1=secure (pair required for health data)."""
        return self._security_profile

    @Property(bool, notify=securityProfileChanged)
    def isSecureProfile(self) -> bool:
        return self._security_profile == BLE_SECURITY_PROFILE_SECURE

    @Property(bool, notify=pairingStateChanged)
    def isPaired(self) -> bool:
        return self._is_paired

    @Property(bool, notify=pairingStateChanged)
    def needsPairingForHealth(self) -> bool:
        return self.isSecureProfile and not self._is_paired

    @Property(str, notify=pairingStateChanged)
    def pairingState(self) -> str:
        return self._pairing_state

    @Property(int, notify=accelUpdated)
    def accelX(self) -> int:
        return self._accel_x

    @Property(int, notify=accelUpdated)
    def accelY(self) -> int:
        return self._accel_y

    @Property(int, notify=accelUpdated)
    def accelZ(self) -> int:
        return self._accel_z

    @Property(str, notify=wifiSsidChanged)
    def wifiSsid(self) -> str:
        return self._wifi_ssid

    @Property(int, notify=wifiConnectedChanged)
    def wifiConnected(self) -> int:
        return self._wifi_connected

    @Property(str, notify=wifiIpChanged)
    def wifiIp(self) -> str:
        return self._wifi_ip

    @Property(int, notify=wifiRssiChanged)
    def wifiRssi(self) -> int:
        return self._wifi_rssi

    @Property(str, notify=patientNameChanged)
    def patientName(self) -> str:
        return self._patient_name

    @Property(bool, notify=glucoseLogStatusChanged)
    def csvWriteBlocked(self) -> bool:
        return self._csv_write_blocked

    @Property(int, notify=glucoseLogStatusChanged)
    def csvDeferredRows(self) -> int:
        return self._csv_deferred_rows

    @Property(str, notify=glucoseLogStatusChanged)
    def csvWriteStatus(self) -> str:
        return self._csv_write_status

    @Property(bool, notify=dfuActiveChanged)
    def dfuActive(self) -> bool:
        return self._dfu_active

    def _set_dfu_active(self, active: bool) -> None:
        if active != self._dfu_active:
            self._dfu_active = active
            self.dfuActiveChanged.emit(active)

    # ------------------------------------------------------------------
    # GATT explorer properties
    # ------------------------------------------------------------------
    @Property(str, notify=gattTableChanged)
    def gattTableJson(self) -> str:
        """The discovered GATT table, as a JSON string for QML to parse."""
        return self._gatt_table_json

    @Property(bool, notify=gattScanningChanged)
    def gattScanning(self) -> bool:
        return self._gatt_scanning

    @Property(int, notify=connInfoChanged)
    def connMtu(self) -> int:
        """Negotiated ATT MTU of the active connection (0 if unknown)."""
        return self._conn_mtu

    def _refresh_glucose_log_status(self) -> None:
        locked, deferred = self._logger.glucose_write_status()
        if locked:
            status = f"CSV: Locked (buffering {deferred} row{'s' if deferred != 1 else ''})"
        elif deferred > 0:
            status = f"CSV: Retrying flush ({deferred} buffered)"
        else:
            status = "CSV: Ready"

        if (locked != self._csv_write_blocked or
                deferred != self._csv_deferred_rows or
                status != self._csv_write_status):
            self._csv_write_blocked = locked
            self._csv_deferred_rows = deferred
            self._csv_write_status = status
            self.glucoseLogStatusChanged.emit()

    @Slot(str)
    def setPatientName(self, name: str) -> None:
        name = name.strip()
        if name == self._patient_name:
            return
        self._patient_name = name
        self._logger.set_patient_name(name)
        self.patientNameChanged.emit(name)

    @Property("QVariantList", notify=scanResultsChanged)
    def scanResults(self) -> list:
        return self._scan_results

    @Property(bool, notify=isScanningChanged)
    def isScanning(self) -> bool:
        return self._is_scanning

    @Property("QVariantList", notify=knownDevicesChanged)
    def knownDevices(self) -> list:
        return self._known_devices

    @Property(bool, notify=reconnectPolicyChanged)
    def autoReconnectUnlimited(self) -> bool:
        return self._auto_reconnect_max_attempts == 0

    @Property(int, notify=reconnectPolicyChanged)
    def autoReconnectMaxAttempts(self) -> int:
        return self._auto_reconnect_max_attempts

    @Property(int, notify=reconnectPolicyChanged)
    def autoReconnectBaseDelaySec(self) -> int:
        return self._auto_reconnect_base_delay_s

    @Property(int, notify=reconnectPolicyChanged)
    def autoReconnectMaxDelaySec(self) -> int:
        return self._auto_reconnect_max_delay_s

    @Property(bool, notify=measIntervalChanged)
    def measIntervalEnabled(self) -> bool:
        return self._meas_interval_enabled

    @Property(int, notify=measIntervalChanged)
    def measIntervalSec(self) -> int:
        return self._meas_interval_sec

    @Property(int, notify=measIntervalChanged)
    def measIntervalType(self) -> int:
        return self._meas_interval_type

    # ------------------------------------------------------------------
    # Slots: scan & connect
    # ------------------------------------------------------------------
    @asyncSlot()
    async def scan(self) -> None:
        if self._is_scanning:
            return
        self._is_scanning = True
        self.isScanningChanged.emit(True)
        self.statusChanged.emit("Scanning for HCM devices…")
        try:
            devices = await self._client.scan()
            self._scan_results = [
                {"name": d.name or "HCM", "address": d.address, "rssi": rssi}
                for d, rssi in devices
            ]
            self.scanResultsChanged.emit()
            self.scanComplete.emit(len(devices))
            self.statusChanged.emit(f"Found {len(devices)} device(s)")
        except Exception as exc:
            self.errorOccurred.emit(str(exc))
            self.scanComplete.emit(0)
        finally:
            self._is_scanning = False
            self.isScanningChanged.emit(False)

    async def _post_connect_gatt_discovery(self, raw_client) -> None:
        """Run post-connect GATT discovery (paired refresh vs unpaired in-connect table)."""
        skip_gatt_refresh = False
        if sys.platform == "win32" and raw_client is not None:
            try:
                from winrt_pairing import is_paired_session
                skip_gatt_refresh = not await is_paired_session(raw_client)
            except Exception as exc:
                log.debug("pairing state before GATT refresh: %s", exc)
        if skip_gatt_refresh:
            log.info("Windows unpaired: using in-connect GATT table (no uncached refresh)")
            self._client.accept_connect_services()
        else:
            self._client.invalidate_services_cache()
            await self._client.discover_services()

    async def _connect_windows_impl(self, address: str) -> bool:
        """Windows: minimal BLE link first, MITM pair before full GATT/config."""
        if self._auto_reconnect_task and not self._auto_reconnect_task.done():
            self._auto_reconnect_task.cancel()
            try:
                await self._auto_reconnect_task
            except (asyncio.CancelledError, Exception):
                pass

        self._set_link_phase("connecting", address)
        self._is_connecting = True
        self._connecting_address = address
        self.isConnectingChanged.emit(True)
        self.statusChanged.emit(f"Connecting to {address}…")
        self._session_teardown_done = False
        self._connect_setup_recovering = True
        self._suppress_auto_reconnect = False
        self._last_device_address = address

        try:
            max_link_attempts = 2
            for link_attempt in range(max_link_attempts):
                if link_attempt > 0:
                    self.statusChanged.emit("Refreshing link after GATT service change…")
                    log.info(
                        "Reconnecting to %s after WinRT GATT session reset (attempt %d)",
                        address,
                        link_attempt + 1,
                    )
                    try:
                        await self._client.disconnect()
                    except Exception:
                        pass
                    await asyncio.sleep(3.0)

                cached_profile = self._known_security_profile(address)
                if cached_profile is not None:
                    self._security_profile = cached_profile
                    self.securityProfileChanged.emit(cached_profile)

                assume_secure = (
                    cached_profile != BLE_SECURITY_PROFILE_OPEN
                    if cached_profile is not None
                    else True
                )
                if assume_secure:
                    from winrt_pairing import is_paired_os

                    self._last_device_address = address
                    if not await is_paired_os(address):
                        log.info(
                            "Windows SECURE unpaired — OOB pair before GattSession "
                            "(get_gatt_services_async wedges pair_async on WinRT)"
                        )
                        self._set_pairing_state("pairing")
                        self.statusChanged.emit(
                            "Pairing with watch — no GATT until bonded…"
                        )
                        self._pairing_in_flight = True
                        try:
                            if not await self._windows_oob_mitm_pair():
                                self._suppress_auto_reconnect = True
                                self.errorOccurred.emit(
                                    "Pairing failed — Forget on watch, run "
                                    "reset_ble_pairing.py --full-reset, retry"
                                )
                                self._set_pairing_state("failed")
                                return False
                        finally:
                            self._pairing_in_flight = False

                skip_f008 = cached_profile != BLE_SECURITY_PROFILE_OPEN
                if not await self._connect_minimal_for_pairing(
                    address, skip_profile_read=skip_f008
                ):
                    if link_attempt + 1 < max_link_attempts:
                        continue
                    self.errorOccurred.emit("Connection failed")
                    return False

                if not self._client.is_connected:
                    if link_attempt + 1 < max_link_attempts:
                        continue
                    self.errorOccurred.emit("Connection dropped during setup")
                    return False

                self._persist_security_profile(address, self._security_profile)
                await self._refresh_paired_state()

                if self._pending_post_pair_hooks and self._client.is_connected:
                    self._pending_post_pair_hooks = False
                    raw = self._client.client
                    if raw is not None:
                        await self._on_pairing_settled(raw)

                self._set_link_phase("gatt_refresh")
                session_dead = False
                try:
                    self._client.invalidate_services_cache()
                    await self._client.discover_services()
                except Exception as exc:
                    if _is_session_dead_error(exc):
                        session_dead = True
                        log.warning("GATT refresh invalidated (services changed): %s", exc)
                    else:
                        log.warning("GATT refresh failed: %s", exc)

                if session_dead and link_attempt + 1 < max_link_attempts:
                    continue

                await asyncio.sleep(2.0 if link_attempt == 0 else 1.5)

                if not self._client.is_connected:
                    if link_attempt + 1 < max_link_attempts:
                        continue
                    self.errorOccurred.emit("Connection dropped during setup")
                    return False

                self._set_link_phase("config_read")
                self._last_config_read_exc = None
                config_ok = await self._read_all_config()
                if not config_ok:
                    last_exc = self._last_config_read_exc
                    if last_exc is not None and _is_session_dead_error(last_exc):
                        if link_attempt + 1 < max_link_attempts:
                            continue
                    try:
                        await self._client.disconnect()
                    except Exception:
                        pass
                    self.errorOccurred.emit("Connection dropped during configuration read")
                    return False

                notify_ok = await self._subscribe_notifications()
                if not notify_ok or not self._client.is_connected:
                    try:
                        await self._client.disconnect()
                    except Exception:
                        pass
                    self.errorOccurred.emit("Connection dropped during notification setup")
                    return False

                return await self._finish_connect_session(address)

            self.errorOccurred.emit("Connection failed after GATT service change recovery")
            return False
        finally:
            self._connect_setup_recovering = False
            if not self._connected and not self._shutting_down:
                self._is_connecting = False
                self._connecting_address = ""
                self.isConnectingChanged.emit(False)
                if not self._session_teardown_done:
                    self._finalize_session_teardown(emit_connection_signal=False)

    async def _connect_to_address_impl(self, address: str) -> bool:
        if sys.platform == "win32":
            if self._ble_session_lock.locked():
                log.info("Connect/pair already in progress — ignoring duplicate request")
                return False
            async with self._ble_session_lock:
                return await self._connect_windows_impl(address)

        self._set_link_phase("connecting", address)
        self._is_connecting = True
        self._connecting_address = address
        self.isConnectingChanged.emit(True)
        self.statusChanged.emit(f"Connecting to {address}…")
        self._session_teardown_done = False
        self._connect_setup_recovering = True
        pairing_attach = None
        if sys.platform == "win32":
            async def pairing_attach(bleak_client) -> None:
                await self._attach_winrt_pairing(bleak_client)

        config_ok = False
        raw_client = None
        pair_first_on_connect = False
        try:
            max_link_attempts = 2 if sys.platform == "win32" else 1
            settle_sec = 3.0 if sys.platform == "win32" else 1.5

            for link_attempt in range(max_link_attempts):
                if link_attempt > 0:
                    self.statusChanged.emit("Refreshing GATT after services-changed…")
                    log.info(
                        "Reconnecting to %s after WinRT GATT session reset (attempt %d)",
                        address, link_attempt + 1,
                    )
                    self._client.invalidate_services_cache()
                    await asyncio.sleep(3.0)
                    ok = await self._client.connect(
                        address=address,
                        timeout=RECONNECT_TIMEOUT,
                        winrt_pairing_attach=pairing_attach,
                        winrt_lazy_services=(sys.platform == "win32"),
                    )
                    if not ok:
                        self.errorOccurred.emit("Connection failed after GATT refresh")
                        return False
                else:
                    ok = await self._client.connect(
                        address=address,
                        timeout=RECONNECT_TIMEOUT,
                        winrt_pairing_attach=pairing_attach,
                        winrt_lazy_services=(sys.platform == "win32"),
                    )
                    if not ok:
                        self.errorOccurred.emit("Connection failed")
                        return False

                self._manual_disconnect = False
                self._last_device_address = address
                self._upsert_known_device(address, self._device_name or "HCM")
                raw_client = self._client.client

                self._set_link_phase("gatt_discovery")
                try:
                    await self._post_connect_gatt_discovery(raw_client)
                except Exception as exc:
                    log.warning("GATT service discovery failed: %s", exc)

                self.statusChanged.emit("Connected — reading config…")
                await asyncio.sleep(settle_sec)

                if not self._client.is_connected:
                    if link_attempt + 1 < max_link_attempts:
                        try:
                            await self._client.disconnect()
                        except Exception:
                            pass
                        continue
                    self.errorOccurred.emit("Connection dropped before config read")
                    return False

                # Windows SECURE unpaired: pair on this minimal link before any
                # other GATT (config reads / CCC) — avoids defer-then-teardown wedge.
                if sys.platform == "win32":
                    self._last_config_read_exc = None
                    self._set_link_phase("security_probe")
                    probe_ok = await self._read_connect_probe()
                    if not probe_ok:
                        if not self._client.is_connected:
                            if link_attempt + 1 < max_link_attempts:
                                try:
                                    await self._client.disconnect()
                                except Exception:
                                    pass
                                continue
                            self.errorOccurred.emit("Connection dropped before security probe")
                            return False
                        last_exc = self._last_config_read_exc
                        if last_exc is not None and _is_session_dead_error(last_exc):
                            if link_attempt + 1 < max_link_attempts:
                                try:
                                    await self._client.disconnect()
                                except Exception:
                                    pass
                                continue
                        try:
                            await self._client.disconnect()
                        except Exception:
                            pass
                        self.statusChanged.emit("Disconnected")
                        self.errorOccurred.emit("Connection dropped during security probe")
                        return False

                    if self.isSecureProfile and not self._is_paired:
                        log.info(
                            "Windows SECURE unpaired — clean-session pair "
                            "(connect already touched GATT)"
                        )
                        self._set_link_phase(
                            "pair_clean_session",
                            "forget PC bond → radio settle → minimal reconnect → pair_async",
                        )
                        self._link_pairing_clean = False
                        self._set_pairing_state("pairing")
                        self.statusChanged.emit(
                            "Secure device — preparing authenticated pairing…"
                        )
                        self._pairing_in_flight = True
                        try:
                            paired_ok = await self._windows_mitm_pair_clean_session(
                                stale_recovery=False,
                            )
                        finally:
                            self._pairing_in_flight = False
                        if not paired_ok:
                            try:
                                if self._client.is_connected:
                                    await self._client.disconnect()
                            except Exception:
                                pass
                            self.statusChanged.emit("Disconnected")
                            self.errorOccurred.emit(
                                "Pairing failed — Forget on watch, Reset BT, then retry"
                            )
                            self._set_pairing_state("failed")
                            return False
                        pair_first_on_connect = True
                        config_ok = True
                        break

                session_dead = False
                config_ok = False
                self._set_link_phase("config_read")
                for attempt in range(3):
                    self._last_config_read_exc = None
                    config_ok = await self._read_all_config()
                    if config_ok:
                        break
                    if not self._client.is_connected:
                        break
                    last_exc = self._last_config_read_exc
                    if last_exc is not None and _is_session_dead_error(last_exc):
                        session_dead = True
                        log.warning("WinRT GATT session invalidated (services changed)")
                        break
                    if attempt < 2:
                        log.info("Config read attempt %d failed, retrying in 1.5 s…", attempt + 1)
                        await asyncio.sleep(1.5)

                if config_ok:
                    break

                if session_dead and link_attempt + 1 < max_link_attempts:
                    try:
                        await self._client.disconnect()
                    except Exception:
                        pass
                    continue

                try:
                    await self._client.disconnect()
                except Exception:
                    pass
                self.statusChanged.emit("Disconnected")
                self.errorOccurred.emit("Connection dropped during initial configuration read")
                return False

            if not config_ok or not self._client.is_connected:
                self.errorOccurred.emit("Connection dropped during initial configuration read")
                return False

            # Pair-first path already ran _on_pairing_settled (config + notify).
            if pair_first_on_connect:
                return await self._finish_connect_session(address)

            notify_ok = await self._subscribe_notifications()
            if not notify_ok or not self._client.is_connected:
                try:
                    await self._client.disconnect()
                except Exception:
                    pass
                self.statusChanged.emit("Disconnected")
                self.errorOccurred.emit("Connection dropped during notification setup")
                return False

            return await self._finish_connect_session(address)
        finally:
            self._connect_setup_recovering = False
            if not self._connected and not self._shutting_down:
                self._is_connecting = False
                self._connecting_address = ""
                self.isConnectingChanged.emit(False)
                if not self._session_teardown_done:
                    self._finalize_session_teardown(emit_connection_signal=False)

    @asyncSlot(str)
    async def connectToAddress(self, address: str) -> None:
        if self._ble_session_lock.locked() or self._is_connecting:
            log.info("Connect already in progress")
            return
        try:
            async with self._ble_session_lock:
                await self._connect_to_address_impl(address)
        except asyncio.CancelledError:
            raise
        except Exception as exc:
            if self._shutting_down:
                return
            log.error("connectToAddress failed: %s", exc, exc_info=True)

    @Slot(bool)
    def setAutoReconnectUnlimited(self, enabled: bool) -> None:
        self._auto_reconnect_max_attempts = 0 if enabled else max(self._auto_reconnect_max_attempts, 5)
        self.reconnectPolicyChanged.emit()
        self._save_known_devices()

    @Slot(int)
    def setAutoReconnectMaxAttempts(self, attempts: int) -> None:
        self._auto_reconnect_max_attempts = max(0, min(int(attempts), 1000000))
        self.reconnectPolicyChanged.emit()
        self._save_known_devices()

    @Slot(int)
    def setAutoReconnectBaseDelaySec(self, delay_s: int) -> None:
        self._auto_reconnect_base_delay_s = max(1, min(int(delay_s), 60))
        if self._auto_reconnect_max_delay_s < self._auto_reconnect_base_delay_s:
            self._auto_reconnect_max_delay_s = self._auto_reconnect_base_delay_s
        self.reconnectPolicyChanged.emit()
        self._save_known_devices()

    @Slot(int)
    def setAutoReconnectMaxDelaySec(self, delay_s: int) -> None:
        self._auto_reconnect_max_delay_s = max(self._auto_reconnect_base_delay_s, min(int(delay_s), 600))
        self.reconnectPolicyChanged.emit()
        self._save_known_devices()

    @Slot(bool)
    def setMeasIntervalEnabled(self, enabled: bool) -> None:
        self._meas_interval_enabled = bool(enabled)
        self.measIntervalChanged.emit()
        self._save_known_devices()
        if enabled and self._connected:
            self._start_meas_interval_task()
        else:
            self._stop_meas_interval_task()

    @Slot(int)
    def setMeasIntervalSec(self, secs: int) -> None:
        self._meas_interval_sec = max(10, min(int(secs), 3600))
        self.measIntervalChanged.emit()
        self._save_known_devices()

    @Slot(int)
    def setMeasIntervalType(self, mtype: int) -> None:
        self._meas_interval_type = max(0, min(int(mtype), 2))
        self.measIntervalChanged.emit()
        self._save_known_devices()

    @asyncSlot()
    async def autoConnectKnown(self) -> None:
        if self._connected:
            return
        if self._ble_session_lock.locked() or self._is_connecting:
            log.debug("Auto-connect skipped — connect/pair already active")
            return

        target = self._last_device_address
        if not target and self._known_devices:
            target = self._known_devices[0].get("address", "")

        if not target:
            self.statusChanged.emit("No known paired device to auto-connect")
            return

        try:
            await self._connect_to_address_impl(target)
        except asyncio.CancelledError:
            raise
        except Exception as exc:
            if self._shutting_down:
                log.debug("autoConnectKnown aborted during shutdown: %s", exc)
                return
            log.error("autoConnectKnown failed: %s", exc, exc_info=True)
            if not self._shutting_down:
                self._set_pairing_state("failed")
                self.errorOccurred.emit(f"Auto-connect failed: {exc}")
            if self._is_connecting and not self._shutting_down:
                self._is_connecting = False
                self._connecting_address = ""
                self.isConnectingChanged.emit(False)

    @asyncSlot(str)
    async def unpairDevice(self, address: str) -> None:
        # Set manual_disconnect BEFORE any BLE operation so that when the
        # link drops (as a side-effect of unpair) _on_client_disconnected
        # will NOT spawn an auto-reconnect task.
        self._manual_disconnect = True
        if self._auto_reconnect_task and not self._auto_reconnect_task.done():
            self._auto_reconnect_task.cancel()
        self._stop_meas_interval_task()

        try:
            # Tell the firmware to clear its bond table FIRST (while still connected).
            if self._connected and self._client.is_connected:
                try:
                    await self._client.admin_delete_bonds()
                except Exception as exc:
                    log.warning("admin_delete_bonds failed (firmware may not support it): %s", exc)

            await self._client.unpair(address)
        except Exception as exc:
            self._report_async_error("Unpair device", exc)
            # Even if unpair failed, still remove locally and tear down.

        self._remove_known_device(address)
        if self._connected and self._last_device_address.lower() == address.lower():
            self._connected = False
            self._finalize_session_teardown(emit_connection_signal=True)
        elif not self._connected:
            self._finalize_session_teardown(emit_connection_signal=False)
        self._manual_disconnect = False
        self.statusChanged.emit(f"Unpaired device: {address}")

    @asyncSlot(str)
    async def forgetDevice(self, address: str) -> None:
        """Remove from known-devices list and stop all background reconnect activity.

        Unlike ``unpairDevice``, this does NOT call the OS-level BLE unpair —
        it simply stops tracking the device and prevents auto-reconnect.
        If the device is currently connected it is disconnected first.
        """
        # Cancel any background reconnect immediately.
        self._manual_disconnect = True
        if self._auto_reconnect_task and not self._auto_reconnect_task.done():
            self._auto_reconnect_task.cancel()
        self._stop_meas_interval_task()

        if self._connected and self._last_device_address.lower() == address.lower():
            try:
                await self._client.disconnect()
            except Exception as exc:
                log.warning("forgetDevice: disconnect failed: %s", exc)
            self._connected = False
            self._finalize_session_teardown(emit_connection_signal=True)

        self._manual_disconnect = False
        self._remove_known_device(address)
        self.statusChanged.emit(f"Removed known device: {address}")

    @asyncSlot()
    async def disconnect(self) -> None:
        self._manual_disconnect = True
        if self._auto_reconnect_task and not self._auto_reconnect_task.done():
            self._auto_reconnect_task.cancel()
        self._stop_meas_interval_task()
        try:
            await self._client.disconnect()
        except Exception as exc:
            self._report_async_error("Disconnect", exc)
        self._connected = False
        self._finalize_session_teardown(emit_connection_signal=True)
        self.statusChanged.emit("Disconnected")

    def prepare_shutdown(self) -> None:
        """Synchronous pre-quit hook — stop accepting BLE notify callbacks."""
        self._shutting_down = True
        self._client.suppress_notifications()

    async def shutdown(self) -> None:
        """Gracefully tear down all BLE activity.  Call this before the event
        loop exits (wire to QApplication.aboutToQuit)."""
        self.prepare_shutdown()
        log.info("Backend shutdown: cancelling background tasks")

        # Stop measurement interval before anything else.
        self._stop_meas_interval_task()

        # Cancel any pending auto-reconnect so it cannot spawn new connections.
        if self._auto_reconnect_task and not self._auto_reconnect_task.done():
            self._auto_reconnect_task.cancel()
            try:
                await self._auto_reconnect_task
            except (asyncio.CancelledError, Exception):
                pass

        # Mark as a clean, intentional teardown so _on_client_disconnected
        # does not try to launch another reconnect task.
        self._manual_disconnect = True

        # Clear the connecting state in case shutdown happens mid-setup.
        if self._is_connecting:
            self._is_connecting = False
            self._connecting_address = ""
            self.isConnectingChanged.emit(False)

        if self._connected or self._client.is_connected:
            try:
                await self._client.disconnect()
            except Exception as exc:
                log.debug("Shutdown disconnect error (ignored): %s", exc)

        self._connected = False
        self._finalize_session_teardown(emit_connection_signal=False)
        self._logger.close_all()
        log.info("Backend shutdown complete")

    # ------------------------------------------------------------------
    # Slots: config read/write
    # ------------------------------------------------------------------
    @asyncSlot()
    async def syncRtcTime(self) -> None:
        try:
            await self._sync_rtc_from_host()
        except Exception as exc:
            self._report_async_error("RTC sync", exc)
            return
        self.statusChanged.emit("RTC synced")

    async def _sync_rtc_from_host(self) -> None:
        """Push host wall-clock to the watch via CTS Current Time (0x2A2B)."""
        await self._client.set_rtc_time()
        ts = await self._client.get_rtc_time()
        self._rtc_time = ts
        self.rtcTimeChanged.emit(ts)
        log.info("CTS synced from host: %u", ts)

    async def _sync_encrypted_wearable_config(self) -> None:
        """RTC, trim, and other GATT config that requires an encrypted bond (SECURE)."""
        name = await self._client.get_device_name()
        self._device_name = name
        self.deviceNameChanged.emit(name)

        await self._sync_rtc_from_host()

        trim = await self._client.get_rtc_trim()
        self._rtc_trim = trim
        self.rtcTrimChanged.emit(trim)

        batt_low = await self._client.get_battery_low_threshold()
        self._battery_low = batt_low
        self.batteryLowChanged.emit(batt_low)

        decimate = await self._client.get_ppg_decimate()
        self._ppg_decimate = decimate
        self.ppgDecimateChanged.emit(decimate)

        try:
            pref = await self._client.get_ppg_preference()
            self._ppg_preference = pref
            self.ppgPreferenceChanged.emit(pref)
        except Exception:
            pass

    @asyncSlot(int)
    async def setRtcTrim(self, ppm: int) -> None:
        await self._client.set_rtc_trim(ppm)
        self._rtc_trim = ppm
        self.rtcTrimChanged.emit(ppm)

    @asyncSlot(str)
    async def setDeviceName(self, name: str) -> None:
        await self._client.set_device_name(name)
        self._device_name = name
        self.deviceNameChanged.emit(name)

    @asyncSlot(int)
    async def setBatteryLow(self, mv: int) -> None:
        await self._client.set_battery_low_threshold(mv)
        self._battery_low = mv
        self.batteryLowChanged.emit(mv)

    @asyncSlot(int)
    async def setBrightness(self, pct: int) -> None:
        try:
            await self._client.set_brightness(pct)
        except Exception as exc:
            self._report_async_error("Set brightness", exc)
            return
        self._brightness = pct
        self.brightnessChanged.emit(pct)

    @asyncSlot(int)
    async def setVolume(self, pct: int) -> None:
        try:
            await self._client.set_volume(pct)
        except Exception as exc:
            self._report_async_error("Set volume", exc)
            return
        self._volume = pct
        self.volumeChanged.emit(pct)

    @asyncSlot(int)
    async def setPpgDecimate(self, factor: int) -> None:
        await self._client.set_ppg_decimate(factor)
        self._ppg_decimate = factor
        self.ppgDecimateChanged.emit(factor)

    @asyncSlot(int)
    async def setPpgPreference(self, pref: int) -> None:
        try:
            await self._client.set_ppg_preference(pref)
        except Exception as exc:
            self._report_async_error("Set PPG preference", exc)
            return
        self._ppg_preference = pref
        self.ppgPreferenceChanged.emit(pref)

    @asyncSlot(str, str)
    async def setWifiCredentials(self, ssid: str, password: str) -> None:
        await self._client.set_wifi_ssid(ssid)
        if password:
            await self._client.set_wifi_password(password)
        self._wifi_ssid = ssid
        self.wifiSsidChanged.emit(ssid)

    @asyncSlot()
    async def wifiConnect(self) -> None:
        await self._client.wifi_connect()
        self.statusChanged.emit("WiFi connect requested")

    @asyncSlot()
    async def wifiDisconnect(self) -> None:
        await self._client.wifi_disconnect()
        self.statusChanged.emit("WiFi disconnect requested")

    # ------------------------------------------------------------------
    # Slots: log file management
    # ------------------------------------------------------------------
    @Slot(str)
    def deleteLogFile(self, file_url: str) -> None:
        """Delete a log file given its file:// URL (called from QML)."""
        import pathlib
        try:
            path = pathlib.Path(file_url.replace("file:///", "").replace("file://", ""))
            if path.is_file():
                path.unlink()
                log.info("Deleted log file: %s", path)
            else:
                log.warning("deleteLogFile: not a file: %s", path)
        except Exception as exc:
            log.warning("deleteLogFile failed: %s", exc)

    @asyncSlot()
    async def uploadSessionLogs(self) -> None:
        """Upload recent CSV logs via gateway.json (REST or MQTT)."""
        cfg = load_gateway_config(LOG_ROOT)
        if not (cfg.get("rest_url") or cfg.get("mqtt_host")):
            self.errorOccurred.emit(
                f"Configure {LOG_ROOT / 'gateway.json'} with rest_url or mqtt_host"
            )
            return

        paths = sorted(
            LOG_ROOT.glob("*.csv"),
            key=lambda p: p.stat().st_mtime,
            reverse=True,
        )[:20]
        if not paths:
            self.statusChanged.emit("No CSV logs to upload")
            return

        self.statusChanged.emit(f"Uploading {len(paths)} log file(s)…")
        device_id = self._hardware_device_id or self._device_name or "HCM"

        def _run() -> list:
            return upload_session_files(LOG_ROOT, paths, device_id=device_id, config=cfg)

        try:
            results = await asyncio.to_thread(_run)
        except Exception as exc:
            self.errorOccurred.emit(f"Upload failed: {exc}")
            return

        ok_count = sum(1 for _, ok, _ in results if ok)
        fail_count = len(results) - ok_count
        if fail_count == 0:
            self.statusChanged.emit(f"Uploaded {ok_count} file(s) to cloud gateway")
        else:
            first_err = next((msg for _, ok, msg in results if not ok), "unknown error")
            self.errorOccurred.emit(
                f"Upload partial: {ok_count} ok, {fail_count} failed — {first_err}"
            )

    @asyncSlot()
    async def startHr(self) -> None:
        try:
            await self._client.start_hr()
        except Exception as exc:
            self._report_async_error("Start HR", exc)
            return
        self.statusChanged.emit("HR measurement started")

    @asyncSlot()
    async def startSpo2(self) -> None:
        try:
            await self._client.start_spo2()
        except Exception as exc:
            self._report_async_error("Start SpO2", exc)
            return
        self.statusChanged.emit("SpO2 measurement started")

    @asyncSlot()
    async def startVitals(self) -> None:
        """Start unified vitals measurement (HR + SpO2 + Hb + RespRate)."""
        try:
            await self._client.start_vitals()
        except Exception as exc:
            self._report_async_error("Start vitals", exc)
            return
        self.statusChanged.emit("Vitals measurement started")

    @asyncSlot()
    async def startGlucose(self) -> None:
        try:
            await self._client.start_glucose()
        except Exception as exc:
            self._report_async_error("Start glucose", exc)
            return
        self.statusChanged.emit("Glucose measurement started")

    @asyncSlot()
    async def stopMeasurement(self) -> None:
        await self._client.stop_measurement()
        self.statusChanged.emit("Measurement stopped")

    @asyncSlot(int)
    async def pmicEnable(self, target: int) -> None:
        try:
            await self._client.pmic_enable(target)
        except Exception as exc:
            self._report_async_error("PMIC enable", exc)
            return
        self.statusChanged.emit(f"PMIC enabled target {target}")

    @asyncSlot(int)
    async def pmicDisable(self, target: int) -> None:
        try:
            await self._client.pmic_disable(target)
        except Exception as exc:
            self._report_async_error("PMIC disable", exc)
            return
        self.statusChanged.emit(f"PMIC disabled target {target}")

    @asyncSlot(int, int)
    async def pmicSetVoltage(self, target: int, mv: int) -> None:
        try:
            await self._client.pmic_set_voltage(target, mv)
        except Exception as exc:
            self._report_async_error("PMIC set voltage", exc)
            return
        self.statusChanged.emit(f"PMIC set target {target} to {mv} mV")

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------
    async def _finish_connect_session(self, address: str) -> bool:
        """Mark connect complete and start background tasks (shared tail)."""
        self._set_link_phase("connected", address)
        self._connected = True
        self._is_connecting = False
        self._connecting_address = ""
        self.isConnectingChanged.emit(False)
        self.connectionStateChanged.emit(True)
        if self.needsPairingForHealth:
            self.statusChanged.emit(
                "Connected — pair to sync clock, RTC, and health data"
            )
        else:
            self.statusChanged.emit("Connected")

        try:
            defer_gatt_scan = False
            raw_client = self._client.client
            if sys.platform == "win32" and raw_client is not None:
                try:
                    from winrt_pairing import is_paired_session
                    defer_gatt_scan = not await is_paired_session(raw_client)
                except Exception as exc:
                    log.debug("pairing state before GATT explorer scan: %s", exc)
                    defer_gatt_scan = True
            if defer_gatt_scan:
                log.info("Deferring GATT explorer scan until paired (Windows)")
            else:
                self._gatt_task = asyncio.get_event_loop().create_task(
                    self._gatt_discover_task()
                )
        except Exception as exc:
            log.debug("Could not start GATT discovery: %s", exc)

        self._upsert_known_device(address, self._device_name or "HCM")

        current = asyncio.current_task()
        if (self._auto_reconnect_task and not self._auto_reconnect_task.done() and
                self._auto_reconnect_task is not current):
            self._auto_reconnect_task.cancel()

        if self._meas_interval_enabled:
            self._start_meas_interval_task()
        return True

    async def _read_connect_probe(self) -> bool:
        """Read only open identity chars — safe before MITM pair on Windows."""
        try:
            try:
                profile = await self._client.get_security_profile()
                self._security_profile = profile
                self.securityProfileChanged.emit(profile)
            except Exception:
                self._security_profile = BLE_SECURITY_PROFILE_OPEN
                self.securityProfileChanged.emit(BLE_SECURITY_PROFILE_OPEN)

            try:
                hw_id = await self._client.get_hardware_device_id()
                self._hardware_device_id = hw_id.strip()
                self._logger.set_device_id(self._hardware_device_id)
                self.hardwareDeviceIdChanged.emit(self._hardware_device_id)
                log.info("Hardware device ID: %s", self._hardware_device_id)
            except Exception as exc:
                log.warning("Hardware device ID read failed: %s", exc)
                self._hardware_device_id = ""
                self.hardwareDeviceIdChanged.emit("")

            await self._refresh_paired_state()
            return True
        except Exception as exc:
            if _is_transient_ble_error(exc):
                log.warning("Connect probe cancelled by OS (stack settling): %s", exc)
            else:
                log.warning("Connect probe failed: %s", exc)
            self._last_config_read_exc = exc
            return False

    async def _read_all_config(self) -> bool:
        """Read current config from device after connection."""
        self._link_pairing_clean = False
        try:
            try:
                profile = await self._client.get_security_profile()
                self._security_profile = profile
                self.securityProfileChanged.emit(profile)
            except Exception:
                self._security_profile = BLE_SECURITY_PROFILE_OPEN
                self.securityProfileChanged.emit(BLE_SECURITY_PROFILE_OPEN)

            try:
                hw_id = await self._client.get_hardware_device_id()
                self._hardware_device_id = hw_id.strip()
                self._logger.set_device_id(self._hardware_device_id)
                self.hardwareDeviceIdChanged.emit(self._hardware_device_id)
                log.info("Hardware device ID: %s", self._hardware_device_id)
            except Exception as exc:
                log.warning("Hardware device ID read failed: %s", exc)
                self._hardware_device_id = ""
                self.hardwareDeviceIdChanged.emit("")

            await self._refresh_paired_state()
            secure_unpaired = self.isSecureProfile and not self._is_paired

            if not secure_unpaired:
                await self._sync_encrypted_wearable_config()
            else:
                log.info(
                    "Secure profile unpaired: RTC and encrypted config deferred until Pair"
                )

            # Brightness/volume are L1 OPEN even on SECURE profile (ble_gatt_security.h).
            # WiFi SSID is L3 AUTH — skip while unpaired on Windows.
            brightness = await self._client.get_brightness()
            self._brightness = brightness
            self.brightnessChanged.emit(brightness)

            volume = await self._client.get_volume()
            self._volume = volume
            self.volumeChanged.emit(volume)

            can_read_wifi_ssid = self._is_paired or sys.platform != "win32"
            if can_read_wifi_ssid:
                try:
                    ssid = await self._client.get_wifi_ssid()
                    self._wifi_ssid = ssid
                    self.wifiSsidChanged.emit(ssid)
                except Exception as ssid_exc:
                    log.debug("WiFi SSID read failed (link may not be encrypted yet): %s", ssid_exc)
            else:
                log.debug("Skipping WiFi SSID read on unpaired Windows session")
            return True

        except Exception as exc:
            if _is_transient_ble_error(exc):
                log.warning("Config read cancelled by OS (stack settling): %s", exc)
            else:
                log.warning("Config read failed: %s", exc)
            self._last_config_read_exc = exc
            return False

    async def _refresh_wifi_ssid_after_pair(self) -> None:
        """Re-read WiFi SSID once the link is encrypted (post-pair)."""
        if not self._connected or self._client.client is None:
            return
        try:
            ssid = await self._client.get_wifi_ssid()
            self._wifi_ssid = ssid
            self.wifiSsidChanged.emit(ssid)
            log.info("WiFi SSID refreshed after pairing: %s", ssid or "(empty)")
        except Exception as exc:
            log.debug("WiFi SSID refresh after pair failed: %s", exc)

    async def _refresh_paired_state(self) -> None:
        """Update isPaired from the OS bond state (Windows WinRT)."""
        paired = False
        raw_client = self._client.client if self._client else None
        if sys.platform == "win32" and raw_client is not None:
            try:
                from winrt_pairing import is_paired_session
                paired = await is_paired_session(raw_client)
            except Exception as exc:
                log.debug("Could not determine pairing state: %s", exc)
        else:
            paired = True
        if paired != self._is_paired:
            self._is_paired = paired
            if paired:
                self._set_pairing_state("paired")
            elif not self._pairing_in_flight:
                self._set_pairing_state("idle")

    async def _subscribe_notifications(self) -> bool:
        if not self._client.is_connected:
            log.warning("Skipping notification subscription: not connected")
            return False
        try:
            await self._refresh_paired_state()
            if sys.platform == "win32" and self.isSecureProfile and not self._is_paired:
                # Any CCC/subscribe before MITM pair can wedge Windows in
                # OPERATION_ALREADY_IN_PROGRESS (no PairingRequested).
                log.info(
                    "Windows unpaired: deferring notification subscriptions until paired"
                )
                return True
            self._link_pairing_clean = False
            if self.isSecureProfile and not self._is_paired:
                log.info("Secure profile: unpaired — subscribing to public notifies only")
                await self._client.subscribe_uuids(
                    self._on_notification, PUBLIC_NOTIFY_CHARACTERISTICS
                )
            else:
                await self._client.subscribe_all(self._on_notification)
            log.info("Notification subscriptions active")
            return True
        except Exception as exc:
            log.warning("Notification subscribe failed: %s", exc)
            return False

    async def _subscribe_health_after_pair(self) -> None:
        """Subscribe health streams after MITM pairing on a SECURE-profile device."""
        if not self.isSecureProfile or not self._is_paired:
            return
        if not self._client.is_connected:
            return
        try:
            operational = tuple(
                u for u in NOTIFY_CHARACTERISTICS
                if u not in HEALTH_NOTIFY_CHARACTERISTICS
                and u not in PUBLIC_NOTIFY_CHARACTERISTICS
            )
            await self._client.subscribe_uuids(self._on_notification, operational)
            await self._client.subscribe_uuids(self._on_notification, HEALTH_NOTIFY_CHARACTERISTICS)
            log.info("Health notification subscriptions enabled after pairing")
        except Exception as exc:
            log.warning("Post-pair health subscribe failed: %s", exc)

    def _finalize_session_teardown(self, emit_connection_signal: bool) -> None:
        """Close CSV loggers once per link drop (user or device initiated)."""
        if self._session_teardown_done:
            return
        self._session_teardown_done = True
        self._detach_winrt_pairing()
        self._logger.close_all()
        # Reset Wi-Fi live state — it reflects the device, not the local network.
        self._wifi_connected = 0
        self._wifi_ip = ""
        self._wifi_rssi = 0
        self.wifiConnectedChanged.emit(0)
        self.wifiIpChanged.emit("")
        self.wifiRssiChanged.emit(0)
        # Clear GATT explorer state — it belongs to the (now gone) connection.
        self._gatt_table_json = "[]"
        self.gattTableChanged.emit()
        self._conn_mtu = 0
        self.connInfoChanged.emit()
        self._link_pairing_clean = False
        if self._is_paired:
            self._is_paired = False
            self._set_pairing_state("idle")
        if emit_connection_signal:
            self.connectionStateChanged.emit(False)

    def _on_client_disconnected(self, address: str) -> None:
        expected = bool(
            self._manual_disconnect
            or self._connect_setup_recovering
            or self._pairing_in_flight
        )
        note = ""
        if self._connect_setup_recovering:
            if self._link_phase.startswith("pair_"):
                note = "normal during Windows clean-session pairing (forget/disconnect before reconnect)"
            elif self._manual_disconnect:
                note = "intentional teardown for pairing recovery"
        self._log_link_drop(address, expected=expected, note=note)

        if self._connect_setup_recovering:
            if self._pairing_in_flight and self._connected:
                self._connected = False
                self.connectionStateChanged.emit(False)
            # Intentional disconnect during clean-session pairing must not abort UI.
            if not self._manual_disconnect:
                fut = self._pair_confirm_future
                if fut is not None and not fut.done():
                    fut.set_result(False)
                if self._pairing_in_flight or self._is_connecting:
                    self._set_pairing_state("failed")
                if self._is_connecting:
                    self._is_connecting = False
                    self._connecting_address = ""
                    self.isConnectingChanged.emit(False)
            return

        was_connected = self._connected
        self._connected = False

        # If the link drops while still in the setup phase (_is_connecting=True
        # but _connected not yet set), clear the connecting state so the button
        # returns to idle instead of being stuck on "Connecting…" forever.
        if self._is_connecting:
            self._is_connecting = False
            self._connecting_address = ""
            self.isConnectingChanged.emit(False)

        manual = self._manual_disconnect
        # User disconnect: HCMBackend.disconnect() will call _finalize_session_teardown.
        # Device drop: tear down here.
        if not manual:
            self._finalize_session_teardown(emit_connection_signal=was_connected)
            self.statusChanged.emit("Disconnected")

        if manual:
            self._manual_disconnect = False
            return

        self._stop_meas_interval_task()

        target = address or self._last_device_address
        if not target:
            return

        if self._suppress_auto_reconnect:
            log.info("Auto-reconnect suppressed (pairing failed — connect manually)")
            return

        loop = asyncio.get_event_loop()
        if self._auto_reconnect_task and not self._auto_reconnect_task.done():
            return
        self._auto_reconnect_task = loop.create_task(self._auto_reconnect(target))

    def _start_meas_interval_task(self) -> None:
        if self._meas_interval_task and not self._meas_interval_task.done():
            return
        # Create a fresh event each time the task is (re)started so there's
        # no stale state from a previous cancelled cycle.
        self._meas_done_event = asyncio.Event()
        loop = asyncio.get_event_loop()
        self._meas_interval_task = loop.create_task(self._meas_interval_loop())

    def _stop_meas_interval_task(self) -> None:
        if self._meas_interval_task and not self._meas_interval_task.done():
            self._meas_interval_task.cancel()

    async def _meas_interval_loop(self) -> None:
        """App-side periodic HR/SpO2 measurement scheduler."""
        if self._meas_done_event is None:
            self._meas_done_event = asyncio.Event()

        first_cycle = True
        while True:
            # Skip the wait on the very first cycle so a measurement fires
            # immediately when the scheduler is enabled.
            if first_cycle:
                first_cycle = False
            else:
                await asyncio.sleep(self._meas_interval_sec)

            if not self._connected or not self._meas_interval_enabled:
                return

            if self._meas_interval_type == 0:
                types_to_run = ["hr"]
            elif self._meas_interval_type == 1:
                types_to_run = ["spo2"]
            else:
                types_to_run = ["vitals"]  # type=2 (Both): use unified vitals (single measurement)

            for meas_type in types_to_run:
                if not self._connected or not self._meas_interval_enabled:
                    return
                try:
                    self._meas_done_event.clear()
                    if meas_type == "hr":
                        await self._client.start_hr()
                        self.statusChanged.emit("Auto HR measurement started")
                    elif meas_type == "spo2":
                        await self._client.start_spo2()
                        self.statusChanged.emit("Auto SpO2 measurement started")
                    else:
                        await self._client.start_vitals()
                        self.statusChanged.emit("Auto vitals measurement started")

                    try:
                        await asyncio.wait_for(
                            self._meas_done_event.wait(),
                            timeout=float(MEAS_INTERVAL_TIMEOUT_SEC),
                        )
                    except asyncio.TimeoutError:
                        log.warning("Auto-measurement timed out; stopping")
                        try:
                            await self._client.stop_measurement()
                        except Exception:
                            pass
                except asyncio.CancelledError:
                    raise
                except Exception as exc:
                    log.warning("Auto-measurement error: %s", exc)

    async def _auto_reconnect(self, address: str) -> None:
        attempt = 0

        while True:
            if self._manual_disconnect or self._connected:
                return
            if self._suppress_auto_reconnect:
                return
            if self._pairing_in_flight or self._connect_setup_recovering:
                log.debug("Auto-reconnect deferred — pairing or connect setup active")
                return

            attempt += 1
            if self._auto_reconnect_max_attempts > 0 and attempt > self._auto_reconnect_max_attempts:
                self.errorOccurred.emit(
                    f"Auto-reconnect stopped after {self._auto_reconnect_max_attempts} attempts"
                )
                return

            self.statusChanged.emit(
                f"Reconnecting to {address} (attempt {attempt}, background)…"
            )

            ok = await self._connect_to_address_impl(address)
            if ok:
                self.statusChanged.emit("Reconnected")
                return

            delay_s = min(
                self._auto_reconnect_base_delay_s * (2 ** min(attempt - 1, 4)),
                self._auto_reconnect_max_delay_s,
            )
            self.statusChanged.emit(
                f"Reconnect attempt {attempt} failed; retrying in {delay_s}s"
            )
            await asyncio.sleep(delay_s)

    # ------------------------------------------------------------------
    # BLE DFU (MCUmgr SMP) public interface
    # ------------------------------------------------------------------
    @Slot(str)
    def startDfuUpload(self, image_path: str) -> None:
        """
        Called from QML to start a DFU image upload.

        :param image_path:  Local filesystem path to the signed firmware binary
                            (e.g. build_sdk_v330/NiSense/zephyr/zephyr.signed.bin).
        """
        if self._dfu_active:
            self.dfuError.emit("DFU already in progress")
            return
        if not self._connected:
            self.dfuError.emit("Not connected to device")
            return
        loop = asyncio.get_event_loop()
        self._dfu_task = loop.create_task(self._dfu_upload_task(image_path))

    @Slot()
    def cancelDfu(self) -> None:
        """Cancel an in-progress DFU upload."""
        # Unblock a pending pairing-confirm wait so the WinRT deferral completes
        # (rejecting), otherwise the ceremony — and the DFU task — hang forever.
        fut = self._pair_confirm_future
        if fut is not None and not fut.done():
            fut.set_result(False)
        if self._dfu_task and not self._dfu_task.done():
            self._dfu_task.cancel()
            self.dfuStatus.emit("DFU cancelled")
            self._set_pairing_state("failed")
            self._set_dfu_active(False)

    # ------------------------------------------------------------------
    # Authenticated pairing (numeric comparison)
    # ------------------------------------------------------------------
    def _set_pairing_state(self, state: str) -> None:
        if self._pairing_state != state:
            self._pairing_state = state
            self.pairingStateChanged.emit(state)

    def _dismiss_pairing_dialog(self, *, cancel_pending: bool = False) -> None:
        """Close the global passkey overlay in QML."""
        if cancel_pending:
            fut = self._pair_confirm_future
            if fut is not None and not fut.done():
                fut.set_result(False)
        self.pairingPasskey.emit("")
        if self._pairing_state in ("confirm", "pairing"):
            self._set_pairing_state("idle")

    @Slot()
    def dismissPairingDialog(self) -> None:
        """User dismissed the passkey overlay (Reject/Close)."""
        self._dismiss_pairing_dialog(cancel_pending=True)

    @Slot(bool)
    def confirmPairing(self, accept: bool) -> None:
        """Called from QML when the user accepts/rejects the displayed passkey."""
        fut = self._pair_confirm_future
        if fut is None or fut.done():
            return
        if not accept:
            fut.set_result(False)
            return
        fut.set_result(True)

    @staticmethod
    def _pairing_kind_auto_accepts(kind: str) -> bool:
        """Numeric-comparison / display-pin ceremonies (watch Accept + PC deferral)."""
        from winrt_pairing import pairing_kind_auto_accepts
        return pairing_kind_auto_accepts(kind)

    async def _wait_pairing_user_confirm(
        self,
        *,
        numeric: bool,
        expected_pin: Optional[int],
        timeout: float,
    ) -> bool:
        """Wait for manual PC Accept and/or watch Accept (f016 poll → auto PC)."""
        from hcm_protocol import BLE_PAIRING_BONDING

        deadline = time.monotonic() + timeout
        char_available = True

        while time.monotonic() < deadline:
            fut = self._pair_confirm_future
            if fut is not None and fut.done():
                return bool(fut.result())

            if not self._client.is_connected:
                if self._pairing_in_flight:
                    await asyncio.sleep(PAIR_WATCH_POLL_INTERVAL_SEC)
                    continue
                return False

            if numeric and char_available:
                try:
                    await self._client.discover_services_for_pairing()
                    ps = await self._client.get_pairing_status()
                    if ps is not None and ps.state >= BLE_PAIRING_BONDING:
                        if expected_pin is None or ps.passkey in (0, expected_pin):
                            log.info(
                                "Watch confirmed passkey (state=%s) — accepting on PC",
                                ps.state,
                            )
                            self.statusChanged.emit(
                                "Watch confirmed — finishing pair on PC…"
                            )
                            return True
                except BleakCharacteristicNotFoundError:
                    char_available = False
                    log.debug("CHRC f016 unavailable — manual PC Accept required")
                except Exception as exc:
                    log.debug("pairing status poll: %s", exc)

            await asyncio.sleep(PAIR_WATCH_POLL_INTERVAL_SEC)

        log.warning("Pairing confirmation timed out after %ss", timeout)
        return False

    async def _confirm_pairing_cb(self, pin: str, kind: str) -> bool:
        """Surface the passkey to the GUI and complete the WinRT pairing deferral.

        Numeric comparison is symmetric (same as nRF Connect): accept on either
        side first. PC **Accept** completes WinRT immediately; if the watch Accepts
        first, f016 ``BLE_PAIRING_BONDING`` auto-completes PC.
        """
        loop = asyncio.get_event_loop()
        self._pair_confirm_future = loop.create_future()
        display = pin.strip() if pin else "------"
        numeric = self._pairing_kind_auto_accepts(kind)
        expected_pin = int(pin) if pin and pin.isdigit() else None
        log.info(
            "Pairing confirm UI (kind=%s numeric=%s pin=%s)",
            kind,
            numeric,
            pin or "<none>",
        )
        self.pairingPasskey.emit(display)
        self._set_pairing_state("confirm")
        if pin:
            if numeric:
                self.statusChanged.emit(
                    f"Pairing code {pin} — verify on both screens, then Accept "
                    f"on either device (watch first auto-completes PC via f016)"
                )
            else:
                self.statusChanged.emit(
                    f"Pairing: confirm code {pin} matches the device, then Accept"
                )
        else:
            self.statusChanged.emit(
                "Pairing: compare the code on the watch, then Accept here"
            )
        try:
            return await self._wait_pairing_user_confirm(
                numeric=numeric,
                expected_pin=expected_pin,
                timeout=PAIR_CONFIRM_TIMEOUT_SEC,
            )
        except asyncio.CancelledError:
            return False
        finally:
            self._pair_confirm_future = None
            # WinRT deferral answered — hide passkey while bonding finishes.
            self._dismiss_pairing_dialog(cancel_pending=False)

    async def _attach_winrt_pairing(self, raw_client) -> None:
        """Register a persistent WinRT pairing handler for this connection."""
        if sys.platform != "win32":
            return
        try:
            from winrt_pairing import WinrtPairingSession
        except Exception as exc:
            log.warning("WinRT pairing helper unavailable: %s", exc)
            return

        loop = asyncio.get_event_loop()
        if self._winrt_pairing_session is None:
            self._winrt_pairing_session = WinrtPairingSession()
        await self._winrt_pairing_session.attach(
            raw_client,
            self._confirm_pairing_cb,
            loop,
        )

    def _detach_winrt_pairing(self) -> None:
        if self._winrt_pairing_session is not None:
            self._winrt_pairing_session.detach()

    async def _cancel_gatt_discovery(self) -> None:
        task = self._gatt_task
        if task is None or task.done():
            self._gatt_task = None
            return
        task.cancel()
        try:
            await task
        except asyncio.CancelledError:
            pass
        except Exception as exc:
            log.debug("GATT discovery cancel: %s", exc)
        self._gatt_task = None

    async def _start_gatt_explorer_if_needed(self, raw_client) -> None:
        if self._gatt_task and not self._gatt_task.done():
            return
        if sys.platform == "win32" and raw_client is not None:
            try:
                from winrt_pairing import is_paired_session
                if not await is_paired_session(raw_client):
                    return
            except Exception:
                return
        loop = asyncio.get_event_loop()
        self._gatt_task = loop.create_task(self._gatt_discover_task())

    async def _connect_minimal_for_pairing(
        self, address: str, *, skip_profile_read: bool = False
    ) -> bool:
        """Connect with pairing handler only — no config prefetch, no notify CCC.

        Windows wedges ``pair_async`` if GATT notify/read runs before MITM pair.
        When *skip_profile_read* is True, use cached profile (default SECURE) and
        defer the f008 read until after bond — matches nRF Connect connect-then-pair.
        """
        self._set_link_phase("pair_minimal_connect", address)
        self._is_connecting = True
        self._connecting_address = address
        self.isConnectingChanged.emit(True)
        pairing_attach = None
        if sys.platform == "win32":
            async def pairing_attach(bleak_client) -> None:
                await self._attach_winrt_pairing(bleak_client)

        try:
            defer_services = False
            if sys.platform == "win32":
                from winrt_pairing import is_paired_os
                defer_services = not await is_paired_os(address)

            ok = await self._client.connect(
                address=address,
                timeout=RECONNECT_TIMEOUT,
                winrt_pairing_attach=pairing_attach,
                winrt_defer_services=defer_services,
            )
            if not ok:
                return False

            self._manual_disconnect = False
            self._last_device_address = address
            self._client.reset_gatt_discovery_state()

            if skip_profile_read:
                profile = self._known_security_profile(address)
                if profile is None:
                    profile = BLE_SECURITY_PROFILE_SECURE
                self._security_profile = profile
                self.securityProfileChanged.emit(profile)
                log.debug(
                    "Minimal connect: deferring f008 read (profile=%d from cache/default)",
                    profile,
                )
            else:
                try:
                    profile = await self._client.get_security_profile()
                    self._security_profile = profile
                    self.securityProfileChanged.emit(profile)
                except Exception as exc:
                    log.debug("security profile read (minimal connect): %s", exc)
                    if _is_session_dead_error(exc):
                        log.warning("Minimal connect: GATT session dead during f008 read")
                        return False

            self._upsert_known_device(address, self._device_name or "HCM")
            self._is_connecting = False
            self._connecting_address = ""
            self.isConnectingChanged.emit(False)
            self.connectionStateChanged.emit(True)
            log.info("Minimal connect for pairing: %s (GATT deferred until after bond)", address)
            self._link_pairing_clean = True
            return True
        except Exception as exc:
            log.warning("Minimal pairing connect failed: %s", exc)
            return False
        finally:
            if self._is_connecting:
                self._is_connecting = False
                self._connecting_address = ""
                self.isConnectingChanged.emit(False)

    async def _windows_oob_mitm_pair(self, *, quiet: bool = False) -> bool:
        """Pair via WinRT before any Bleak GattSession (Windows SECURE)."""
        if sys.platform != "win32":
            return True

        from winrt_pairing import WinrtPairingSession, is_paired_os, winrt_oob_mitm_pair

        address = self._last_device_address
        if not address:
            return False

        if await is_paired_os(address):
            self._set_pairing_state("paired")
            return True

        if self._shutting_down:
            return False

        if not quiet:
            self.statusChanged.emit("Waiting for watch to advertise…")
        if not await self._client.wait_for_advertising(address, timeout=25.0):
            log.warning("OOB pair: %s not seen in scan — trying anyway", address)

        if self._winrt_pairing_session is None:
            self._winrt_pairing_session = WinrtPairingSession()
        session = self._winrt_pairing_session
        loop = asyncio.get_event_loop()

        self._set_link_phase("pair_oob", "WinRT pair_async before GattSession")
        if not quiet:
            self._set_pairing_state("pairing")

        try:
            outcome = await winrt_oob_mitm_pair(
                address,
                session,
                self._confirm_pairing_cb,
                loop,
            )
        finally:
            session.detach()

        return await self._interpret_pair_outcome(
            outcome,
            None,
            quiet=quiet,
            allow_clean_fallback=True,
            allow_reconnect=False,
        )

    async def _windows_mitm_pair_clean_session(
        self, *, quiet: bool = False, stale_recovery: bool = False
    ) -> bool:
        """Disconnect, clear PC bond, optional radio bounce, OOB ``pair_async``.

        Fallback when OOB or on-link pair returns ``busy_stale``. Never runs
        ``pair_async`` after ``get_gatt_services_async`` on the same session.
        """
        from winrt_pairing import (
            PAIRING_BUSY_MSG,
            forget_device_pairing,
            is_paired_session,
            reset_bluetooth_stack,
        )

        address = self._last_device_address
        if not address:
            return False

        if self._clean_session_active:
            log.warning("Clean pairing session already active")
            return False

        if not quiet:
            self.statusChanged.emit("Preparing clean Windows pairing session…")
        log.info("Windows clean MITM pair session for %s", address)
        self._clean_session_active = True
        self._set_link_phase("pair_forget", "disconnect + forget WinRT bond")

        self._dismiss_pairing_dialog(cancel_pending=True)

        if self._auto_reconnect_task and not self._auto_reconnect_task.done():
            self._auto_reconnect_task.cancel()
            try:
                await self._auto_reconnect_task
            except (asyncio.CancelledError, Exception):
                pass

        await self._cancel_gatt_discovery()
        fut = self._pair_confirm_future
        if fut is not None and not fut.done():
            fut.set_result(False)

        self._manual_disconnect = True
        self._connect_setup_recovering = True
        try:
            try:
                await self._client.unsubscribe_all_notifications()
            except Exception as exc:
                log.debug("clean pair notify teardown: %s", exc)

            if self._winrt_pairing_session is not None:
                self._winrt_pairing_session.detach()

            raw_for_forget = self._client.client if self._client.is_connected else None
            if raw_for_forget is not None:
                try:
                    from winrt_pairing import is_paired_session

                    if await is_paired_session(raw_for_forget):
                        try:
                            await self._client.admin_delete_bonds()
                            await asyncio.sleep(0.5)
                        except Exception as exc:
                            log.warning("clean pair: admin_delete_bonds: %s", exc)
                except Exception as exc:
                    log.debug("clean pair: paired check before forget: %s", exc)
                await forget_device_pairing(address, bleak_client=raw_for_forget)

            if self._client.is_connected:
                await self._client.disconnect()

            if stale_recovery:
                if not quiet:
                    self.statusChanged.emit("Resetting Bluetooth radio (wedged ceremony)…")
                log.info("Stale WinRT ceremony — radio bounce before reconnect")
                self._set_link_phase("pair_radio_reset", "BT radio bounce after wedged pair")
                await reset_bluetooth_stack(
                    address, bounce_radio=True, restart_service=False
                )
                if not await self._sleep_unless_shutdown(5.0):
                    return False

            if not quiet:
                self.statusChanged.emit("Waiting for Windows Bluetooth to settle…")
            settle_sec = 12.0 if stale_recovery else 8.0
            self._set_link_phase("pair_settle_wait", f"{settle_sec:.0f}s after forget/radio reset")
            if not await self._sleep_unless_shutdown(settle_sec):
                return False

            if self._shutting_down:
                return False

            if not quiet:
                self.statusChanged.emit("Pairing (no GATT) after stack reset…")

            self._set_link_phase("pair_oob", "OOB pair_async after forget/radio reset")
            if await self._windows_oob_mitm_pair(quiet=quiet):
                return True

            log.warning("Clean OOB pair failed — full Windows BT stack reset")
            if not quiet:
                self.statusChanged.emit("Resetting Bluetooth stack (Windows)…")
            from winrt_pairing import reset_bluetooth_stack

            try:
                await self._client.unsubscribe_all_notifications()
            except Exception:
                pass
            if self._client.is_connected:
                await self._client.disconnect()
            if self._winrt_pairing_session is not None:
                self._winrt_pairing_session.detach()
                self._winrt_pairing_session = None

            await reset_bluetooth_stack(
                address, bounce_radio=True, restart_service=True
            )
            if not await self._sleep_unless_shutdown(15.0):
                return False

            if self._shutting_down:
                return False

            if await self._windows_oob_mitm_pair(quiet=quiet):
                return True

            if not quiet:
                self._set_pairing_state("failed")
                self.errorOccurred.emit(str(PAIRING_BUSY_MSG))
            return False
        finally:
            self._clean_session_active = False
            self._manual_disconnect = False

    async def _prepare_windows_pairing_session(self) -> None:
        """Deprecated path — Windows MITM pairing uses ``_windows_mitm_pair_clean_session``."""
        return

    async def _on_pairing_settled(self, raw_client) -> None:
        """Run post-pair hooks: encrypted config (RTC), WiFi SSID, health notifies, GATT scan."""
        await self._refresh_paired_state()
        if self.isSecureProfile and not self._is_paired:
            log.warning("Post-pair hook: session still unpaired — skipping encrypted sync")
        else:
            try:
                await self._read_all_config()
                self.statusChanged.emit("RTC synced from PC")
            except Exception as exc:
                log.warning("Post-pair config sync failed: %s", exc)
                self.statusChanged.emit("Pairing OK but config sync failed")
        await self._subscribe_notifications()
        await self._refresh_wifi_ssid_after_pair()
        if self.isSecureProfile:
            await self._subscribe_health_after_pair()
        await self._start_gatt_explorer_if_needed(raw_client)
        self._link_pairing_clean = False

    async def _interpret_pair_outcome(
        self,
        outcome: str,
        raw_client,
        *,
        quiet: bool = False,
        allow_clean_fallback: bool = True,
        allow_reconnect: bool = True,
    ) -> bool:
        """Map WinRT ``pair_async`` outcome to success / clean-session retry / fail."""
        from winrt_pairing import PAIRING_BUSY_MSG, wait_for_pairing_settled

        log.info("WinRT pair_async outcome: %s", outcome)

        if outcome == "paired" or (
            outcome == "busy"
            and raw_client is not None
            and await wait_for_pairing_settled(raw_client, timeout_sec=45.0)
        ):
            self._set_pairing_state("paired")
            if not quiet:
                self.statusChanged.emit("Paired successfully")
            if raw_client is not None:
                await self._on_pairing_settled(raw_client)
            else:
                self._pending_post_pair_hooks = True
            return True

        if (
            allow_clean_fallback
            and not self._clean_session_active
            and outcome in ("busy", "busy_stale", "timeout", "error")
        ):
            log.warning("Pair %s — falling back to clean session", outcome)
            return await self._windows_mitm_pair_clean_session(
                quiet=quiet,
                stale_recovery=(outcome == "busy_stale"),
            )

        if outcome == "disconnected":
            if allow_reconnect and sys.platform == "win32":
                if await self._recover_pairing_after_link_drop(quiet=quiet):
                    return True
            log.warning("Pair ended: link dropped during ceremony")
        elif not quiet:
            self._set_pairing_state("failed")
            self.errorOccurred.emit(str(PAIRING_BUSY_MSG))
        return False

    async def _recover_pairing_after_link_drop(self, *, quiet: bool = False) -> bool:
        """Reconnect after a mid-ceremony drop; bond may have completed or pair can retry."""
        address = self._last_device_address
        if not address:
            return False

        log.info("Pairing: link dropped mid-ceremony — reconnecting")
        if not quiet:
            self.statusChanged.emit("Pairing interrupted — reconnecting…")

        self._dismiss_pairing_dialog(cancel_pending=True)
        if self._winrt_pairing_session is not None:
            self._winrt_pairing_session.detach()

        await asyncio.sleep(2.0)

        if not await self._connect_minimal_for_pairing(address):
            log.warning("Pairing recovery reconnect failed")
            return False

        raw_client = self._client.client
        if raw_client is None:
            return False

        from winrt_pairing import is_paired_session

        if await is_paired_session(raw_client):
            log.info("Bond completed despite link drop")
            self._set_pairing_state("paired")
            if not quiet:
                self.statusChanged.emit("Paired successfully")
            await self._on_pairing_settled(raw_client)
            return True

        session = self._winrt_pairing_session
        if session is None or not session.attached:
            await self._attach_winrt_pairing(raw_client)
            session = self._winrt_pairing_session
        if session is None:
            return False

        if not quiet:
            self.statusChanged.emit("Retrying pairing after reconnect…")
            self._set_pairing_state("pairing")

        try:
            outcome = await session.pair(raw_client)
        except Exception as exc:
            log.warning("post-disconnect pair_async raised: %s", exc)
            outcome = "error"

        return await self._interpret_pair_outcome(
            outcome,
            raw_client,
            quiet=quiet,
            allow_clean_fallback=True,
            allow_reconnect=False,
        )

    async def _pair_on_minimal_link(self, *, quiet: bool = False) -> bool:
        """Run ``pair_async`` on a link that only ran the security probe.

        Only valid when ``_link_pairing_clean`` is True (minimal reconnect).
        Otherwise delegates to the clean-session path.
        """
        if sys.platform != "win32":
            return True

        if not self._link_pairing_clean:
            log.info("Link not pairing-clean — using clean-session pair")
            return await self._windows_mitm_pair_clean_session(stale_recovery=True)

        from winrt_pairing import is_paired_session

        raw_client = self._client.client if self._client else None
        if raw_client is None or not self._client.is_connected:
            return False

        if await is_paired_session(raw_client):
            self._set_pairing_state("paired")
            await self._on_pairing_settled(raw_client)
            return True

        session = self._winrt_pairing_session
        if session is None or not session.attached:
            await self._attach_winrt_pairing(raw_client)
            session = self._winrt_pairing_session
        if session is None:
            return False

        if not quiet:
            self.statusChanged.emit("Starting authenticated pairing…")
            self._set_pairing_state("pairing")

        try:
            outcome = await session.pair(raw_client)
        except Exception as exc:
            log.warning("pair_async raised: %s", exc)
            outcome = "error"

        return await self._interpret_pair_outcome(
            outcome,
            raw_client,
            quiet=quiet,
            allow_clean_fallback=not self._clean_session_active,
        )

    async def _recover_windows_pairing_and_retry(self, *, quiet: bool = False) -> bool:
        """Delegate to the clean-session MITM pair path."""
        if sys.platform != "win32":
            return False
        return await self._windows_mitm_pair_clean_session(quiet=quiet)

    async def _ensure_paired(self, raw_client, *, quiet: bool = False) -> bool:
        """
        Ensure the device is paired/bonded with MITM authentication before
        accessing authenticated characteristics (e.g. the SMP/DFU char).

        On Windows SECURE profile this runs OOB ``pair_async`` (no GattSession)
        before Bleak connect; clean-session recovery on ``busy_stale``.
        """
        if sys.platform != "win32":
            return True
        try:
            from winrt_pairing import is_paired_os, is_paired_session
        except Exception as exc:
            log.warning("WinRT pairing helper unavailable: %s", exc)
            return True

        async with self._pairing_lock:
            try:
                if raw_client is not None and await is_paired_session(raw_client):
                    self._set_pairing_state("paired")
                    await self._on_pairing_settled(raw_client)
                    return True

                address = self._last_device_address
                if address and await is_paired_os(address):
                    self._set_pairing_state("paired")
                    return True

                self._pairing_in_flight = True
                if self._client.is_connected:
                    self._manual_disconnect = True
                    try:
                        await self._client.disconnect()
                    finally:
                        self._manual_disconnect = False
                return await self._windows_oob_mitm_pair(quiet=quiet)
            except Exception as exc:
                log.error("Pairing failed: %s", exc)
                self._set_pairing_state("failed")
                msg = f"Pairing failed: {exc}"
                if self._dfu_active:
                    self.dfuError.emit(msg)
                else:
                    self.errorOccurred.emit(msg)
                return False
            finally:
                self._pairing_in_flight = False

    @asyncSlot()
    async def pairDevice(self) -> None:
        """Manually trigger authenticated pairing from QML (e.g. a Pair button)."""
        if self._is_connecting or self._connect_setup_recovering:
            self.errorOccurred.emit("Connection still setting up — wait a moment, then Pair")
            return
        if self._pairing_in_flight:
            self.statusChanged.emit("Pairing already in progress")
            return
        if not self._connected or not self._client.is_connected:
            self.errorOccurred.emit("Not connected")
            return
        try:
            self._set_pairing_state("pairing")
            raw_client = self._client.client
            if raw_client is None:
                self.errorOccurred.emit(
                    f"BleakClient not available ({self._link_context_summary()})"
                )
                self._set_pairing_state("failed")
                return
            ok = await self._ensure_paired(raw_client)
            if not ok:
                self._set_pairing_state("failed")
        except Exception as exc:
            log.error("pairDevice failed: %s", exc)
            self._set_pairing_state("failed")
            self.errorOccurred.emit(str(exc))

    @asyncSlot()
    async def forgetPairing(self) -> None:
        """Remove the OS-level bond for the connected device (clears stuck state)."""
        # Unblock any pending confirm so a wedged ceremony can be torn down.
        fut = self._pair_confirm_future
        if fut is not None and not fut.done():
            fut.set_result(False)
        if sys.platform != "win32":
            self.statusChanged.emit("Forget pairing is only supported on Windows here")
            return
        raw_client = self._client.client if self._client else None
        if raw_client is None:
            self.errorOccurred.emit("Not connected")
            return
        try:
            from winrt_pairing import unpair
            ok = await unpair(raw_client)
            self._is_paired = False
            self._set_pairing_state("idle")
            if ok:
                self.statusChanged.emit("Pairing removed — you can pair again")
            else:
                self.statusChanged.emit("Device was not paired")
        except Exception as exc:
            log.warning("forgetPairing failed: %s", exc)
            self.errorOccurred.emit(f"Forget pairing failed: {exc}")

    @asyncSlot(bool)
    async def recoverWindowsPairing(self, bounce_radio: bool = True) -> None:
        """Manual stack reset (Firmware → Reset BT). Unpair + optional radio bounce."""
        if sys.platform != "win32":
            self.statusChanged.emit("Reset BT is only needed on Windows")
            return
        address = self._last_device_address
        if not address and self._known_devices:
            address = self._known_devices[0].get("address", "")
        if not address:
            self.errorOccurred.emit("No device address for pairing reset")
            return

        self.statusChanged.emit("Resetting Bluetooth stack…")
        await self._cancel_gatt_discovery()

        fut = self._pair_confirm_future
        if fut is not None and not fut.done():
            fut.set_result(False)

        if self._winrt_pairing_session is not None:
            self._winrt_pairing_session.detach()

        was_connected = self._connected or self._client.is_connected
        self._manual_disconnect = True
        if self._client.is_connected:
            try:
                await self._client.disconnect()
            except Exception as exc:
                log.debug("disconnect before BT reset: %s", exc)
            await asyncio.sleep(1.0)

        self._connected = False
        self._is_connecting = False
        self._connecting_address = ""
        self.isConnectingChanged.emit(False)
        if was_connected:
            self.connectionStateChanged.emit(False)
        self._manual_disconnect = False

        try:
            from winrt_pairing import reset_bluetooth_stack
            await reset_bluetooth_stack(address, bounce_radio=bool(bounce_radio))
        except Exception as exc:
            log.warning("recoverWindowsPairing failed: %s", exc)
            self.errorOccurred.emit(f"Reset BT failed: {exc}")
            return

        self._set_pairing_state("failed")
        self.statusChanged.emit(
            "Bluetooth reset done — reconnect, then tap Pair once."
        )

    async def _dfu_upload_task(self, image_path: str) -> None:
        """Async DFU worker: upload → set pending → reset."""
        from pathlib import Path as _Path
        path = _Path(image_path)
        if not path.exists():
            self.dfuError.emit(f"File not found: {image_path}")
            return

        self._set_dfu_active(True)
        self.dfuError.emit("")
        self.dfuStatus.emit(f"Starting DFU upload: {path.name}")
        self._set_link_phase("dfu", path.name)

        try:
            if not self._client.is_connected:
                raise RuntimeError(
                    f"Not connected ({self._link_context_summary()}). "
                    "Wait for connect/pairing to finish, then retry DFU."
                )
            raw_client = self._client.client  # underlying BleakClient
            if raw_client is None:
                raise RuntimeError(
                    f"BleakClient handle cleared ({self._link_context_summary()}). "
                    "Link may have dropped during pairing — reconnect and pair first."
                )

            # The SMP/DFU characteristic requires authenticated encryption.
            # Pair (numeric comparison) before touching it, or the WinRT stack
            # rejects access with "attribute requires authentication".
            self.dfuStatus.emit("Verifying device pairing…")
            if not await self._ensure_paired(raw_client):
                self.dfuStatus.emit("DFU aborted — pairing required")
                return

            async with SMPClient(raw_client) as smp:
                def _progress(sent: int, total: int) -> None:
                    self.dfuProgress.emit(sent, total)
                    pct = int(sent * 100 / total) if total else 0
                    self.dfuStatus.emit(f"Uploading… {pct}% ({sent}/{total} bytes)")

                self.dfuStatus.emit("Uploading firmware image…")
                await smp.upload_image(path, progress_cb=_progress)

                self.dfuStatus.emit("Marking image as pending…")
                await smp.set_image_pending(confirm=False)

                self.dfuStatus.emit("Sending reset — device will reboot and apply update…")
                await smp.reset_device()

            self.dfuStatus.emit("DFU complete — device is rebooting")
            self.dfuProgress.emit(0, 0)  # reset progress bar
            await self._dfu_reconnect_after_reset()

        except asyncio.CancelledError:
            self.dfuStatus.emit("DFU cancelled")
        except SMPError as exc:
            log.error("DFU SMP error: %s", exc)
            self.dfuError.emit(str(exc))
            self.dfuStatus.emit("DFU failed")
        except Exception as exc:
            log.error("DFU error: %s", exc)
            self.dfuError.emit(str(exc))
            self.dfuStatus.emit("DFU failed")
        finally:
            self._set_dfu_active(False)
            self._dfu_task = None
            if self._connected:
                self._set_link_phase("connected")
            elif not self._connect_setup_recovering and not self._pairing_in_flight:
                self._set_link_phase("idle")

    async def _dfu_reconnect_after_reset(self) -> None:
        """Wait for MCUboot swap and auto-reconnect after DFU reset."""
        addr = self._last_device_address
        if not addr:
            self.dfuStatus.emit("DFU done — reconnect manually from Scan")
            return

        self._connected = False
        self.connectionStateChanged.emit(False)
        self._manual_disconnect = False

        self.dfuStatus.emit("Waiting for device to reboot…")
        await asyncio.sleep(10.0)

        for attempt in range(12):
            self.dfuStatus.emit(f"Reconnecting after DFU ({attempt + 1}/12)…")
            try:
                ok = await self._connect_to_address_impl(addr)
                if ok:
                    self.dfuStatus.emit("Reconnected after DFU")
                    return
            except Exception as exc:
                log.debug("DFU reconnect attempt %d failed: %s", attempt + 1, exc)
            await asyncio.sleep(4.0)

        self.dfuError.emit("Could not reconnect after DFU — use Scan page")

    # ------------------------------------------------------------------
    # GATT explorer
    # ------------------------------------------------------------------
    @Slot()
    def refreshGattTable(self) -> None:
        """Called from QML to (re)discover the connected peer's GATT table."""
        if not self._connected:
            self.errorOccurred.emit("Not connected")
            return
        if self._gatt_task and not self._gatt_task.done():
            return  # discovery already running
        loop = asyncio.get_event_loop()
        self._gatt_task = loop.create_task(self._gatt_discover_task())

    async def _gatt_discover_task(self) -> None:
        self._gatt_scanning = True
        self.gattScanningChanged.emit(True)
        try:
            table = await self._client.discover_gatt(self._security_profile)
            self._gatt_table_json = json.dumps(table)
            self.gattTableChanged.emit()

            # Capture negotiated MTU (bleak >= 0.21 exposes mtu_size).
            raw_client = self._client.client
            mtu = getattr(raw_client, "mtu_size", 0) if raw_client else 0
            if mtu and mtu != self._conn_mtu:
                self._conn_mtu = mtu
                self.connInfoChanged.emit()
        except Exception as exc:
            log.warning("GATT discovery failed: %s", exc)
            self.errorOccurred.emit(f"GATT discovery failed: {exc}")
        finally:
            self._gatt_scanning = False
            self.gattScanningChanged.emit(False)

    def _load_known_devices(self) -> None:
        try:
            if KNOWN_DEVICES_FILE.exists():
                content = json.loads(KNOWN_DEVICES_FILE.read_text(encoding="utf-8"))
                items = content.get("devices", [])
                self._known_devices = [
                    {
                        "name": str(item.get("name", "HCM")),
                        "address": str(item.get("address", "")),
                        "last_seen": int(item.get("last_seen", 0)),
                    }
                    for item in items if item.get("address")
                ]
                self._last_device_address = str(content.get("last_device_address", ""))

                reconnect_cfg = content.get("reconnect_policy", {})
                self._auto_reconnect_max_attempts = max(
                    0,
                    min(int(reconnect_cfg.get("max_attempts", self._auto_reconnect_max_attempts)), 1000000),
                )
                self._auto_reconnect_base_delay_s = max(
                    1,
                    min(int(reconnect_cfg.get("base_delay_s", self._auto_reconnect_base_delay_s)), 60),
                )
                self._auto_reconnect_max_delay_s = max(
                    self._auto_reconnect_base_delay_s,
                    min(int(reconnect_cfg.get("max_delay_s", self._auto_reconnect_max_delay_s)), 600),
                )
                self.reconnectPolicyChanged.emit()

                interval_cfg = content.get("meas_interval", {})
                self._meas_interval_enabled = bool(interval_cfg.get("enabled", self._meas_interval_enabled))
                self._meas_interval_sec = max(
                    10, min(int(interval_cfg.get("interval_sec", self._meas_interval_sec)), 3600),
                )
                self._meas_interval_type = max(
                    0, min(int(interval_cfg.get("meas_type", self._meas_interval_type)), 2),
                )
                self.measIntervalChanged.emit()
            else:
                self._known_devices = []
                self._last_device_address = ""
        except Exception as exc:
            log.warning("Failed to load known devices: %s", exc)
            self._known_devices = []
            self._last_device_address = ""

    def _save_known_devices(self) -> None:
        try:
            KNOWN_DEVICES_FILE.parent.mkdir(parents=True, exist_ok=True)
            payload = {
                "last_device_address": self._last_device_address,
                "devices": self._known_devices,
                "reconnect_policy": {
                    "max_attempts": self._auto_reconnect_max_attempts,
                    "base_delay_s": self._auto_reconnect_base_delay_s,
                    "max_delay_s": self._auto_reconnect_max_delay_s,
                },
                "meas_interval": {
                    "enabled": self._meas_interval_enabled,
                    "interval_sec": self._meas_interval_sec,
                    "meas_type": self._meas_interval_type,
                },
            }
            KNOWN_DEVICES_FILE.write_text(
                json.dumps(payload, indent=2),
                encoding="utf-8",
            )
        except Exception as exc:
            log.warning("Failed to save known devices: %s", exc)

    def _known_security_profile(self, address: str) -> Optional[int]:
        """Last known f008 value from ``known_devices.json``, if any."""
        if not address:
            return None
        lowered = address.lower()
        for item in self._known_devices:
            if item.get("address", "").lower() != lowered:
                continue
            val = item.get("security_profile")
            if val in (BLE_SECURITY_PROFILE_OPEN, BLE_SECURITY_PROFILE_SECURE):
                return int(val)
        return None

    def _persist_security_profile(self, address: str, profile: int) -> None:
        if not address or profile not in (
            BLE_SECURITY_PROFILE_OPEN,
            BLE_SECURITY_PROFILE_SECURE,
        ):
            return
        lowered = address.lower()
        for item in self._known_devices:
            if item.get("address", "").lower() == lowered:
                if item.get("security_profile") != profile:
                    item["security_profile"] = profile
                    self._save_known_devices()
                return

    def _upsert_known_device(self, address: str, name: str) -> None:
        if not address:
            return

        now_ts = int(time.time())
        for item in self._known_devices:
            if item.get("address", "").lower() == address.lower():
                item["name"] = name or item.get("name", "HCM")
                item["last_seen"] = now_ts
                self._last_device_address = address
                self.knownDevicesChanged.emit()
                self._save_known_devices()
                return

        self._known_devices.insert(0, {
            "name": name or "HCM",
            "address": address,
            "last_seen": now_ts,
            "security_profile": self._security_profile,
        })
        self._last_device_address = address
        self.knownDevicesChanged.emit()
        self._save_known_devices()

    def _remove_known_device(self, address: str) -> None:
        if not address:
            return

        lowered = address.lower()
        before = len(self._known_devices)
        self._known_devices = [
            item for item in self._known_devices
            if item.get("address", "").lower() != lowered
        ]

        if before != len(self._known_devices):
            if self._last_device_address.lower() == lowered:
                self._last_device_address = (
                    self._known_devices[0].get("address", "")
                    if self._known_devices else ""
                )
            self.knownDevicesChanged.emit()
            self._save_known_devices()

    def _apply_vitals(self, parsed: VitalsData) -> None:
        """Emit vitals signals and log from a decoded VitalsData object."""
        self._logger.log_vitals(parsed)
        if parsed.hr_valid:
            self.hrUpdated.emit(parsed.hr_bpm, parsed.hr_confidence)
        if parsed.spo2_valid:
            self.spo2Updated.emit(parsed.spo2_percent, parsed.spo2_confidence)
        if parsed.hb_valid:
            self.hbUpdated.emit(parsed.hb_g_dl, parsed.hb_confidence)
        if parsed.resp_valid:
            self.respRateUpdated.emit(parsed.resp_rate_bpm, parsed.resp_confidence)

    def _on_notification(self, uuid: str, data: bytes) -> None:
        """Decode incoming notification and emit the appropriate Qt signal."""
        if self._shutting_down or self._session_teardown_done:
            return
        try:
            uuid_lower = uuid.lower()

            if uuid_lower == CHRC_VITALS.lower():
                parsed = decode_vitals(data)
                if parsed:
                    self._apply_vitals(parsed)

            elif uuid_lower == CHRC_GLUCOSE.lower():
                parsed = decode_glucose(data)
                if parsed:
                    self._logger.log_glucose(parsed)
                    self._refresh_glucose_log_status()
                    self.glucoseUpdated.emit(
                        parsed.glucose_mg_dl, parsed.glucose_mmol_l, parsed.quality
                    )

            elif uuid_lower == CHRC_GLUCOSE_SAMPLE.lower():
                parsed = decode_glucose_sample(data)
                if parsed:
                    self._logger.log_glucose_sample(parsed)
                    self._refresh_glucose_log_status()
                    self.glucoseSampleUpdated.emit(
                        parsed.sample_number,
                        parsed.total_samples,
                        parsed.voltage_mv,
                        parsed.raw_adc_value,
                        parsed.timestamp,
                    )

            elif uuid_lower == CHRC_GLUCOSE_ALGO.lower():
                parsed = decode_glucose_algo(data)
                if parsed:
                    log.info("Glucose algo notification received (len=%d)", len(data))
                    self._logger.set_glucose_algo(parsed)
                    self._refresh_glucose_log_status()
                else:
                    log.warning("Glucose algo decode failed (len=%d, expected=124)", len(data))

            elif uuid_lower == CHRC_TEMPERATURE.lower():
                parsed = decode_temperature(data)
                if parsed:
                    self._logger.log_temperature(parsed)
                    self.tempUpdated.emit(parsed.temp_c)

            elif uuid_lower == CHRC_PMIC.lower():
                parsed = decode_pmic(data)
                if parsed:
                    self.pmicUpdated.emit(
                        parsed.battery_mv, parsed.current_ma,
                        parsed.soc_percent, parsed.charger_status
                    )

            elif uuid_lower == CHRC_PMIC_EXT.lower():
                parsed = decode_pmic_ext(data)
                if parsed:
                    self.pmicExtUpdated.emit(
                        parsed.buck3_mv,
                        parsed.bbout_mv,
                        parsed.buck1_enabled,
                        parsed.buck2_enabled,
                        parsed.buck3_enabled,
                        parsed.bbout_enabled,
                        parsed.charge_voltage_mv,
                        parsed.charge_current_ma,
                        parsed.battery_temp_c,
                        parsed.cycle_count,
                        parsed.remaining_mah,
                        parsed.full_mah,
                        parsed.design_mah,
                        parsed.time_to_empty_min,
                        parsed.time_to_full_min,
                        parsed.avg_current_ma,
                    )

            elif uuid_lower == CHRC_WIFI_STATUS.lower():
                parsed = decode_wifi_status(data)
                if parsed:
                    if parsed.ssid and parsed.ssid != self._wifi_ssid:
                        self._wifi_ssid = parsed.ssid
                        self.wifiSsidChanged.emit(parsed.ssid)
                    self._wifi_connected = parsed.connected
                    self._wifi_ip = parsed.ip_addr
                    self._wifi_rssi = parsed.rssi_dbm
                    self.wifiConnectedChanged.emit(parsed.connected)
                    self.wifiIpChanged.emit(parsed.ip_addr)
                    self.wifiRssiChanged.emit(parsed.rssi_dbm)
                    self.wifiUpdated.emit(
                        parsed.connected, parsed.rssi_dbm, parsed.ip_addr, parsed.ssid
                    )

            elif uuid_lower == CHRC_MEAS_STATUS.lower():
                parsed = decode_meas_status(data)
                if parsed:
                    # Firmware uses active=0, pct=0, quality=0 as an explicit
                    # response for unsupported/failed measurement requests.
                    # meas_type=0 is the idle/startup broadcast — not a failure.
                    if (not parsed.active and parsed.percent_complete == 0 and
                            parsed.quality == 0 and parsed.meas_type != 0):
                        type_name = {
                            1: "HR",
                            2: "SpO2",
                            3: "Glucose",
                        }.get(parsed.meas_type, f"type {parsed.meas_type}")
                        self.errorOccurred.emit(
                            f"{type_name} feature unavailable or measurement start failed"
                        )

                    # Signal the interval task that a measurement has finished.
                    was_active = self._meas_active_flag
                    self._meas_active_flag = bool(parsed.active)
                    if was_active and not parsed.active and self._meas_done_event is not None:
                        self._meas_done_event.set()

                    self.measStatusUpdated.emit(
                        parsed.active, parsed.meas_type,
                        parsed.percent_complete, parsed.quality
                    )

            elif uuid_lower == CHRC_PROXIMITY.lower():
                parsed = decode_proximity_status(data)
                if parsed:
                    self.proximityUpdated.emit(
                        1 if parsed.contact else 0,
                        parsed.wear_state,
                        parsed.proximity_raw,
                        parsed.proximity_filt,
                        parsed.timestamp,
                    )

            elif uuid_lower == CHRC_PPG_STREAM.lower():
                parsed = decode_ppg_sample(data)
                if parsed:
                    self._logger.log_ppg(parsed)
                    self.ppgSampleReceived.emit(
                        parsed.sample_num, parsed.raw_ir, parsed.raw_red,
                        parsed.raw_green, parsed.accel_x, parsed.accel_y,
                        parsed.accel_z, parsed.timestamp_ms
                    )

            elif uuid_lower == CHRC_ACCEL_STREAM.lower():
                parsed = decode_accel_sample(data)
                if parsed:
                    self._accel_x = parsed.x_mg
                    self._accel_y = parsed.y_mg
                    self._accel_z = parsed.z_mg
                    self.accelUpdated.emit(
                        parsed.x_mg, parsed.y_mg, parsed.z_mg, parsed.timestamp_ms
                    )

            elif uuid_lower == CHRC_SENSOR_ALL.lower():
                parsed = decode_sensor_all(data)
                if parsed:
                    self.pmicUpdated.emit(
                        parsed.pmic.battery_mv, parsed.pmic.current_ma,
                        parsed.pmic.soc_percent, parsed.pmic.charger_status,
                    )
                    self.tempUpdated.emit(parsed.temperature.temp_c)
                    if (parsed.vitals.flags or parsed.vitals.hr_bpm or
                            parsed.vitals.spo2_percent):
                        self._apply_vitals(parsed.vitals)
                    if parsed.glucose.glucose_mg_dl > 0:
                        self._logger.log_glucose(parsed.glucose)
                        self._refresh_glucose_log_status()
                        self.glucoseUpdated.emit(
                            parsed.glucose.glucose_mg_dl,
                            parsed.glucose.glucose_mmol_l,
                            parsed.glucose.quality,
                        )
                    self.proximityUpdated.emit(
                        int(parsed.proximity.contact),
                        parsed.proximity.wear_state,
                        parsed.proximity.proximity_raw,
                        parsed.proximity.proximity_filt,
                        parsed.proximity.timestamp,
                    )
        except Exception as exc:
            log.warning("Notification decode failed for %s (len=%d): %s", uuid, len(data), exc)
