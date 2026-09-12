"""
HCM BLE Client
==============
Async bleak-based client for the HCM wearable firmware.
Handles device scan, connection, characteristic read/write, and notification subscriptions.

Platforms: Windows 10 1709+ (WinRT), Linux (BlueZ 5.43+)
"""

import asyncio
import logging
import platform
import time
import warnings
from typing import Awaitable, Callable, Dict, List, Optional

from bleak import BleakClient, BleakScanner
from bleak.backends.device import BLEDevice
from bleak.backends.scanner import AdvertisementData
from bleak.exc import BleakCharacteristicNotFoundError, BleakError

from hcm_protocol import (
    CHRC_ADMIN_CTRL,
    ENCRYPTED_BLE_CHARACTERISTICS,
    encrypted_characteristics_for_profile,
    HEALTH_NOTIFY_CHARACTERISTICS,
    PUBLIC_NOTIFY_CHARACTERISTICS,
    BLE_SECURITY_PROFILE_OPEN,
    CHRC_BATTERY_LOW, CHRC_BRIGHTNESS, CHRC_DEVICE_ID, CHRC_DEVICE_NAME, CHRC_GLUCOSE,
    CHRC_GLUCOSE_ALGO, CHRC_GLUCOSE_SAMPLE,
    CHRC_MEAS_CTRL, CHRC_MEAS_STATUS, CHRC_PMIC,
    CHRC_PMIC_CTRL, CHRC_PMIC_EXT,
    CHRC_PROXIMITY,
    CHRC_PPG_DECIMATE, CHRC_PPG_PREF, CHRC_PPG_STREAM, CHRC_CURRENT_TIME, CHRC_RTC_TRIM,
    CHRC_ACCEL_STREAM,
    SVC_SENSOR_DATA, SVC_WEARABLE_CONFIG, SVC_WIFI_CONFIG,
    CHRC_SENSOR_ALL, CHRC_TEMPERATURE, CHRC_VITALS, CHRC_VOLUME,
    CHRC_WIFI_CONNECT, CHRC_WIFI_ENABLE, CHRC_WIFI_PASSWORD,
    CHRC_WIFI_SSID, CHRC_WIFI_STATUS,
    ADMIN_CMD_DELETE_BONDS,
    PMIC_CMD_DISABLE, PMIC_CMD_ENABLE, PMIC_CMD_SET_VOLTAGE,
    decode_glucose, decode_glucose_sample, decode_meas_status, decode_pmic,
    decode_pmic_ext,
    decode_proximity_status,
    decode_ppg_sample, decode_accel_sample, decode_sensor_all, decode_vitals, decode_temperature, decode_wifi_status,
    encode_battery_low, encode_meas_ctrl, encode_pmic_ctrl, encode_current_time, encode_rtc_trim,
    encode_string, encode_uint8, GlucoseData, VitalsData, SensorAllData, MeasStatus, PmicData, PmicExtData,
    PpgSample, AccelSample, ProximityStatus, TemperatureData, WifiStatus,
    gatt_name,
)

log = logging.getLogger(__name__)


def format_ble_error(exc: BaseException) -> str:
    """One-line summary for logs (WinRT hresult / BlueZ errno when present)."""
    parts = [f"{type(exc).__name__}: {exc}"]
    for attr in ("winerror", "hresult", "errno"):
        val = getattr(exc, attr, None)
        if val is not None:
            parts.append(f"{attr}={val}")
    return " | ".join(parts)


def _decode_printable(raw: bytes) -> Optional[str]:
    """Return a UTF-8 string if the bytes look like printable text, else None."""
    if not raw:
        return None
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        return None
    # Treat as text only if every char is printable (allow trailing NULs).
    stripped = text.rstrip("\x00")
    if stripped and all(c.isprintable() or c in "\r\n\t" for c in stripped):
        return stripped
    return None


DEVICE_NAME = "HCM"
DEVICE_NAME_ALIASES = ("HCM", "HealthMonitor")
HCM_SERVICE_UUIDS = {
    SVC_WEARABLE_CONFIG.lower(),
    SVC_SENSOR_DATA.lower(),
    SVC_WIFI_CONFIG.lower(),
}
SCAN_TIMEOUT = 10.0      # seconds (initial discovery scan)
RECONNECT_TIMEOUT = 15.0 # seconds (direct-address reconnect after disconnect)
RECONNECT_DELAY = 3.0    # seconds

# Characteristics that support notifications
NOTIFY_CHARACTERISTICS = (
    CHRC_PMIC, CHRC_TEMPERATURE, CHRC_VITALS,
    CHRC_GLUCOSE, CHRC_GLUCOSE_SAMPLE, CHRC_GLUCOSE_ALGO, CHRC_PMIC_EXT,
    CHRC_PPG_STREAM, CHRC_ACCEL_STREAM, CHRC_SENSOR_ALL,
    CHRC_MEAS_STATUS, CHRC_PROXIMITY, CHRC_WIFI_STATUS,
)


class HCMClient:
    """
    BLE client for the HCM wearable device.

    Usage::

        client = HCMClient()
        await client.connect()
        await client.subscribe_all(my_callback)
        await client.start_hr()
        ...
        await client.disconnect()
    """

    def __init__(self, adapter: Optional[str] = None) -> None:
        self._adapter = adapter          # e.g. "hci0" on Linux
        self._client: Optional[BleakClient] = None
        self._device: Optional[BLEDevice] = None
        self._connected = False
        self._connected_address: Optional[str] = None
        self._callbacks: Dict[str, List[Callable]] = {}
        self._services_ready = False
        self._disconnected_cb: Optional[Callable[[str], None]] = None
        self._notify_active = False

    def set_disconnected_callback(self, callback: Optional[Callable[[str], None]]) -> None:
        """Register callback invoked when BLE link drops unexpectedly."""
        self._disconnected_cb = callback

    # ------------------------------------------------------------------
    # Scan
    # ------------------------------------------------------------------

    async def scan(self, timeout: float = SCAN_TIMEOUT) -> list:
        """Scan for HCM devices.  Returns list of (BLEDevice, rssi) tuples."""
        # bleak 0.20+ removed detection_callback from discover(); use
        # return_adv=True to get AdvertisementData alongside each device.
        kwargs: dict = {"timeout": timeout, "return_adv": True}
        if self._adapter and platform.system() == "Linux":
            kwargs["adapter"] = self._adapter

        results = await BleakScanner.discover(**kwargs)
        # results: Dict[str, Tuple[BLEDevice, AdvertisementData]]
        found = []
        for device, adv in results.values():
            name = adv.local_name or device.name or ""
            lname = name.lower()
            service_uuids = {u.lower() for u in (adv.service_uuids or [])}

            name_match = any(alias.lower() in lname for alias in DEVICE_NAME_ALIASES)
            service_match = bool(HCM_SERVICE_UUIDS.intersection(service_uuids))

            if name_match or service_match:
                shown_name = name or "Unknown"
                log.info("Found: %s (%s) RSSI=%d dBm", shown_name, device.address, adv.rssi)
                found.append((device, adv.rssi))
        return found

    async def wait_for_advertising(self, address: str, timeout: float = 20.0) -> bool:
        """Block until *address* appears in a scan (watch slow-adv after bond clear)."""
        deadline = time.monotonic() + max(1.0, timeout)
        while time.monotonic() < deadline:
            try:
                found = await self.scan(min(6.0, deadline - time.monotonic()))
            except Exception as exc:
                log.debug("wait_for_advertising scan: %s", exc)
                await asyncio.sleep(1.0)
                continue
            for dev, _rssi in found:
                if dev.address.lower() == address.lower():
                    return True
            await asyncio.sleep(0.5)
        return False

    # ------------------------------------------------------------------
    # Connect / Disconnect
    # ------------------------------------------------------------------

    async def connect(
        self,
        device: Optional[BLEDevice] = None,
        address: Optional[str] = None,
        timeout: float = SCAN_TIMEOUT,
        winrt_pairing_attach: Optional[Callable[[BleakClient], Awaitable[None]]] = None,
        winrt_lazy_services: bool = False,
        winrt_link_only_services: bool = False,
        winrt_defer_services: bool = False,
    ) -> bool:
        """
        Connect to the HCM device.
        - If *device* is given, connect directly.
        - If *address* is given, connect by MAC/UUID.
        - Otherwise scan and connect to the first HCM found.
        - On Windows, pass *winrt_pairing_attach* so PairingRequested is registered
          before Bleak's in-connect ``get_services()`` (see ``winrt_pairing``).
        Returns True on success.
        """
        if device is None and address is None:
            devices = await self.scan(timeout)
            if not devices:
                log.error("No HCM device found during scan")
                return False
            device, _ = devices[0]  # scan() returns (BLEDevice, rssi) tuples

        if device is not None:
            address = device.address

        if device is None and address is not None and platform.system() == "Windows":
            log.info("Windows pre-scan for %s …", address)
            try:
                scan_t = max(6.0, min(timeout, 12.0))
                found = await self.scan(scan_t)
                for dev, _rssi in found:
                    if dev.address.lower() == address.lower():
                        device = dev
                        address = dev.address
                        self._device = dev
                        log.info("Pre-scan resolved %s", address)
                        break
                if device is None:
                    log.warning(
                        "Pre-scan: %s not advertising — trying direct connect",
                        address,
                    )
            except Exception as pre_exc:
                log.warning("Pre-scan failed: %s", pre_exc)

        log.info("Connecting to %s …", address)
        client = BleakClient(address, disconnected_callback=self._on_disconnected)
        self._client = client
        try:
            await self._bleak_connect(
                client,
                timeout,
                winrt_pairing_attach,
                winrt_lazy_services=winrt_lazy_services,
                winrt_link_only_services=winrt_link_only_services,
                winrt_defer_services=winrt_defer_services,
            )
            self._connected = True
            self._connected_address = address
            self._device = device
            # Bleak connect() already ran get_services(); backend refresh uses use_cached=False.
            log.info("Connected to %s", address)
            return True
        except Exception as exc:
            log.error("Connection failed: %s", format_ble_error(exc))

            # Windows can temporarily fail direct-address reconnect if the
            # peripheral is still rotating/refreshing advertising state. Do a
            # short scan fallback and reconnect using a fresh BLEDevice object.
            if address is not None:
                try:
                    devices = await self.scan(timeout=max(timeout, 10.0))
                    target = None
                    for dev, _rssi in devices:
                        if dev.address.lower() == address.lower():
                            target = dev
                            break

                    if target is None and devices:
                        # Fallback to first HCM device when address isn't visible.
                        target = devices[0][0]

                    if target is not None:
                        log.info("Retrying connect via scan result: %s", target.address)
                        client2 = BleakClient(target.address, disconnected_callback=self._on_disconnected)
                        self._client = client2
                        await self._bleak_connect(
                            client2,
                            timeout,
                            winrt_pairing_attach,
                            winrt_lazy_services=winrt_lazy_services,
                            winrt_link_only_services=winrt_link_only_services,
                            winrt_defer_services=winrt_defer_services,
                        )
                        self._connected = True
                        self._connected_address = target.address
                        self._device = target
                        log.info("Connected to %s", target.address)
                        return True
                except Exception as fallback_exc:
                    log.error(
                        "Fallback scan reconnect failed: %s",
                        format_ble_error(fallback_exc),
                    )

            self._connected = False
            self._connected_address = None
            self._services_ready = False
            return False

    async def _bleak_connect(
        self,
        client: BleakClient,
        timeout: float,
        winrt_pairing_attach: Optional[Callable[[BleakClient], Awaitable[None]]],
        *,
        winrt_lazy_services: bool = False,
        winrt_link_only_services: bool = False,
        winrt_defer_services: bool = False,
    ) -> None:
        if platform.system() == "Windows" and winrt_pairing_attach is not None:
            from winrt_pairing import bleak_connect_with_pairing_handler
            await bleak_connect_with_pairing_handler(
                client,
                winrt_pairing_attach,
                lazy_services=winrt_lazy_services,
                link_only_services=winrt_link_only_services,
                defer_services=winrt_defer_services,
                timeout=timeout,
            )
        else:
            await client.connect(timeout=timeout)

    def reset_gatt_discovery_state(self) -> None:
        """Clear Bleak's service table so discovery can run after pair-first connect."""
        self._services_ready = False
        client = self._client
        if client is None:
            return
        try:
            from winrt_pairing import clear_bleak_gatt_cache
            clear_bleak_gatt_cache(client)
        except Exception:
            backend = getattr(client, "_backend", None)
            if backend is not None:
                backend.services = None

    async def discover_services_for_pairing(self) -> None:
        """Cached GATT discovery during MITM ceremony (after PairingRequested)."""
        if not self._client or not self._client.is_connected:
            raise RuntimeError("Not connected")
        if self._services_ready:
            return
        self.reset_gatt_discovery_state()
        get_services = getattr(self._client, "get_services", None)
        if callable(get_services):
            try:
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore", FutureWarning)
                    await get_services(use_cached=True)
            except TypeError:
                await get_services()
        self._services_ready = True

    def suppress_notifications(self) -> None:
        """Drop bleak notify callbacks immediately (safe during app shutdown)."""
        self._notify_active = False

    async def disconnect(self) -> None:
        """Gracefully disconnect."""
        self.suppress_notifications()
        self._services_ready = False
        client = self._client
        if client and client.is_connected:
            await self.unsubscribe_all_notifications()
            await asyncio.sleep(0.25)
            try:
                await asyncio.wait_for(client.disconnect(), timeout=12.0)
            except asyncio.TimeoutError:
                log.warning("Bleak disconnect timed out — clearing local session")
            except Exception as exc:
                log.warning("Bleak disconnect failed: %s", format_ble_error(exc))
        self._connected = False
        self._client = None
        self._connected_address = None
        log.info("Disconnected")

    def invalidate_services_cache(self) -> None:
        """Force a fresh GATT discovery on the next operation."""
        self._services_ready = False

    def accept_connect_services(self) -> None:
        """Trust Bleak's in-connect GATT discovery; skip an uncached refresh.

        On Windows, a second ``get_services(use_cached=False)`` while unpaired
        can provoke an implicit Just Works pairing attempt that wedges MITM
        ``pair_async``.  Call this when the session is connected but not bonded.
        """
        if self._client and self._client.is_connected:
            self._services_ready = True

    async def discover_services(self) -> None:
        """Refresh GATT service table (``use_cached=False`` on WinRT).

        Bleak's ``connect()`` already discovers services.  Call this once after
        connect so a firmware UUID change is visible and Service Changed
        indications can settle before config reads.
        """
        await self._ensure_services_ready()

    async def _ensure_services_ready(self) -> None:
        if not self._client or not self._client.is_connected:
            raise RuntimeError("Not connected")
        if self._services_ready:
            return
        # Force fresh GATT discovery; avoids Windows OS GATT cache returning
        # stale characteristic UUIDs after firmware reflash with new UUIDs.
        get_services = getattr(self._client, "get_services", None)
        if callable(get_services):
            try:
                with warnings.catch_warnings():
                    warnings.simplefilter("ignore", FutureWarning)
                    await get_services(use_cached=False)
            except TypeError:
                # Older bleak versions don't support use_cached; fall back.
                await get_services()
        else:
            # Newer Bleak variants can expose services without get_services().
            # Accessing `.services` is enough to ensure discovery has happened
            # (or trigger the backend's lazy discovery path).
            _ = getattr(self._client, "services", None)
            log.debug("BleakClient.get_services() unavailable; using services property")
        # Yield to the event loop so the WinRT stack can process any pending
        # Service Changed indications sent by the firmware's GATT caching layer.
        # These indications can cancel subsequent GATT reads if not drained first.
        await asyncio.sleep(0)
        self._services_ready = True

    def _on_disconnected(self, client: BleakClient) -> None:
        address = self._connected_address or getattr(client, "address", "") or "?"
        log.warning(
            "BLE transport disconnected (addr=%s, bleak_connected=%s)",
            address,
            bool(getattr(client, "is_connected", False)),
        )
        self._connected = False
        self._services_ready = False
        self._client = None
        self._connected_address = None
        if self._disconnected_cb:
            try:
                self._disconnected_cb(address)
            except Exception as exc:
                log.debug("Disconnected callback error: %s", exc)

    @property
    def is_connected(self) -> bool:
        return self._connected and self._client is not None and self._client.is_connected

    @property
    def client(self) -> "Optional[BleakClient]":
        """The underlying BleakClient, or None if not connected.  Used by SMPClient."""
        return self._client

    @property
    def connected_address(self) -> Optional[str]:
        return self._connected_address

    # ------------------------------------------------------------------
    # Notification subscriptions
    # ------------------------------------------------------------------

    async def subscribe(self, uuid: str, callback: Callable) -> None:
        """Subscribe to a single characteristic notification."""
        if not self.is_connected:
            raise RuntimeError("Not connected")
        client = self._client
        if client is None:
            raise RuntimeError("Not connected")
        await self._ensure_services_ready()
        try:
            def _wrap(sender, data, _cb=callback):
                if not self._notify_active:
                    return
                _cb(sender, data)

            await client.start_notify(uuid, _wrap)
            log.debug("Subscribed to %s", uuid)
        except BleakCharacteristicNotFoundError as exc:
            if uuid.lower() == CHRC_PPG_STREAM.lower():
                log.info("PPG stream characteristic not present (f106) in current firmware")
            elif uuid.lower() == CHRC_ACCEL_STREAM.lower():
                log.info("Accel stream characteristic not present (f10c) in current firmware")
            else:
                log.warning("Could not subscribe to %s: %s", uuid[-4:], exc)
        except Exception as exc:
            log.warning("Could not subscribe to %s: %s", uuid[-4:], exc)

    async def unsubscribe(self, uuid: str) -> None:
        if not self._client:
            return
        try:
            await self._client.stop_notify(uuid)
        except Exception:
            pass

    async def unsubscribe_all_notifications(self) -> None:
        """Best-effort: stop all known notification characteristics."""
        if not self._client:
            return
        for uuid in NOTIFY_CHARACTERISTICS:
            try:
                await self._client.stop_notify(uuid)
            except Exception:
                # Characteristic may not exist / not subscribed / link already down
                pass

    async def subscribe_all(self, callback: Callable) -> None:
        """Subscribe all notifiable characteristics."""
        await self.subscribe_uuids(callback, NOTIFY_CHARACTERISTICS)

    async def subscribe_uuids(self, callback: Callable, uuids) -> None:
        """
        Subscribe a subset of notifiable characteristics.
        *callback* signature: ``callback(uuid: str, data: bytes) -> None``
        """
        self._notify_active = True
        for uuid in uuids:
            if not self.is_connected:
                raise RuntimeError("Disconnected during notification subscription")

            def _wrap(sender, data, _uuid=uuid, _cb=callback):
                if not self._notify_active:
                    return
                _cb(_uuid, bytes(data))

            await self.subscribe(uuid, _wrap)

    # ------------------------------------------------------------------
    # GATT introspection (service explorer)
    # ------------------------------------------------------------------

    async def discover_gatt(self, security_profile: int = BLE_SECURITY_PROFILE_OPEN) -> list:
        """
        Enumerate the full GATT table of the connected peer.

        Returns a list of service dicts (JSON-serialisable)::

            [
              {
                "uuid": "...", "handle": 1, "name": "...", "kind": "primary",
                "characteristics": [
                  {
                    "uuid": "...", "handle": 3, "name": "...",
                    "properties": ["read", "notify"],
                    "value_hex": "ab12", "value_text": "…" | null,
                    "descriptors": [ {"uuid": "...", "handle": 5, "name": "..."} ]
                  }
                ]
              }
            ]

        Readable characteristics are read once (best-effort) so the explorer
        can show their current value, mirroring nRF Connect's behaviour.
        """
        if not self.is_connected:
            raise RuntimeError("Not connected")
        client = self._client
        if client is None:
            raise RuntimeError("Not connected")
        await self._ensure_services_ready()

        services_collection = getattr(client, "services", None)
        if services_collection is None:
            return []

        encrypted_skip = encrypted_characteristics_for_profile(security_profile)
        skip_encrypted_reads = False
        if platform.system() == "Windows" and client.is_connected:
            try:
                from winrt_pairing import is_paired_session
                skip_encrypted_reads = not await is_paired_session(client)
            except Exception as exc:
                log.debug("discover_gatt pairing check: %s", exc)
                skip_encrypted_reads = True

        out: list = []
        for service in services_collection:
            svc_uuid = str(service.uuid)
            svc_entry = {
                "uuid": svc_uuid,
                "handle": getattr(service, "handle", -1),
                "name": gatt_name(svc_uuid, service.description),
                "kind": "primary",
                "characteristics": [],
            }

            for chrc in service.characteristics:
                chrc_uuid = str(chrc.uuid)
                props = list(chrc.properties)  # e.g. ["read", "notify"]
                entry = {
                    "uuid": chrc_uuid,
                    "handle": getattr(chrc, "handle", -1),
                    "name": gatt_name(chrc_uuid, chrc.description),
                    "properties": props,
                    "value_hex": None,
                    "value_text": None,
                    "descriptors": [],
                }

                # Best-effort read of readable characteristics so the UI can
                # display a live value.  Never let one failing read abort the
                # whole discovery.
                if "read" in props:
                    if skip_encrypted_reads and chrc_uuid.lower() in encrypted_skip:
                        log.debug(
                            "Skipping encrypted GATT read %s while unpaired",
                            chrc_uuid[-8:],
                        )
                    else:
                        try:
                            raw = bytes(await client.read_gatt_char(chrc_uuid))
                            entry["value_hex"] = raw.hex()
                            entry["value_text"] = _decode_printable(raw)
                        except Exception as exc:  # noqa: BLE001
                            log.debug("Read %s failed during GATT scan: %s", chrc_uuid[-4:], exc)

                for desc in chrc.descriptors:
                    desc_uuid = str(desc.uuid)
                    entry["descriptors"].append({
                        "uuid": desc_uuid,
                        "handle": getattr(desc, "handle", -1),
                        "name": gatt_name(desc_uuid, getattr(desc, "description", "")),
                    })

                svc_entry["characteristics"].append(entry)

            out.append(svc_entry)

        log.info("GATT discovery: %d services", len(out))
        return out

    # ------------------------------------------------------------------
    # Generic read / write helpers
    # ------------------------------------------------------------------

    async def read(self, uuid: str) -> bytes:
        if not self.is_connected:
            raise RuntimeError("Not connected")
        client = self._client
        if client is None:
            raise RuntimeError("Not connected")
        await self._ensure_services_ready()
        try:
            return bytes(await client.read_gatt_char(uuid))
        except BleakCharacteristicNotFoundError as exc:
            raise RuntimeError(f"Characteristic {uuid} was not found") from exc
        except BleakError as exc:
            raise RuntimeError(str(exc)) from exc

    async def write(self, uuid: str, data: bytes, with_response: bool = True) -> None:
        if not self.is_connected:
            raise RuntimeError("Not connected")
        client = self._client
        if client is None:
            raise RuntimeError("Not connected")
        await self._ensure_services_ready()
        try:
            await client.write_gatt_char(uuid, data, response=with_response)
        except BleakCharacteristicNotFoundError as exc:
            raise RuntimeError(f"Characteristic {uuid} was not found") from exc
        except BleakError as exc:
            raise RuntimeError(str(exc)) from exc

    # ------------------------------------------------------------------
    # Wearable Config operations
    # ------------------------------------------------------------------

    async def get_device_name(self) -> str:
        return (await self.read(CHRC_DEVICE_NAME)).decode("utf-8", errors="replace")

    async def get_hardware_device_id(self) -> str:
        """Read hwinfo-based device ID (uppercase hex). Requires firmware with CHRC f015."""
        return (await self.read(CHRC_DEVICE_ID)).decode("utf-8", errors="replace").strip("\x00")

    async def set_device_name(self, name: str) -> None:
        await self.write(CHRC_DEVICE_NAME, encode_string(name, 31))

    async def get_rtc_time(self) -> int:
        """Read CTS Current Time; return approximate Unix seconds for UI (naive local)."""
        import struct
        from datetime import datetime

        raw = await self.read(CHRC_CURRENT_TIME)
        if len(raw) < 10:
            raise ValueError(f"CTS Current Time short read: {len(raw)} bytes")
        year, mon, mday, hours, mins, sec, _wday, _frac, _reason = struct.unpack_from(
            "<HBBBBBBBB", raw, 0
        )
        dt = datetime(year, mon, mday, hours, mins, sec)
        return int(dt.timestamp())

    async def set_rtc_time(self, unix_ts: Optional[int] = None) -> None:
        """Sync device RTC via CTS Current Time (local wall clock + day-of-week)."""
        from datetime import datetime

        if unix_ts is None:
            dt = datetime.now()
        else:
            dt = datetime.fromtimestamp(unix_ts)
        await self.write(CHRC_CURRENT_TIME, encode_current_time(dt))
        log.info("CTS Current Time synced: %s", dt.isoformat(sep=" ", timespec="seconds"))

    async def get_rtc_trim(self) -> int:
        import struct
        raw = await self.read(CHRC_RTC_TRIM)
        return struct.unpack_from("<i", raw)[0]

    async def set_rtc_trim(self, ppm: int) -> None:
        await self.write(CHRC_RTC_TRIM, encode_rtc_trim(ppm))

    async def get_battery_low_threshold(self) -> int:
        import struct
        raw = await self.read(CHRC_BATTERY_LOW)
        return struct.unpack_from("<H", raw)[0]

    async def set_battery_low_threshold(self, mv: int) -> None:
        await self.write(CHRC_BATTERY_LOW, encode_battery_low(mv))

    async def get_brightness(self) -> int:
        return (await self.read(CHRC_BRIGHTNESS))[0]

    async def set_brightness(self, pct: int) -> None:
        await self.write(CHRC_BRIGHTNESS, encode_uint8(pct))

    async def get_volume(self) -> int:
        return (await self.read(CHRC_VOLUME))[0]

    async def set_volume(self, pct: int) -> None:
        await self.write(CHRC_VOLUME, encode_uint8(pct))

    async def get_ppg_decimate(self) -> int:
        return (await self.read(CHRC_PPG_DECIMATE))[0]

    async def set_ppg_decimate(self, factor: int) -> None:
        await self.write(CHRC_PPG_DECIMATE, encode_uint8(factor))

    async def get_ppg_preference(self) -> int:
        """Returns uint8 enum (0=unset, 1=auto, 2=max86141, 3=max3010x)."""
        return (await self.read(CHRC_PPG_PREF))[0]

    async def get_security_profile(self) -> int:
        """Returns 0=open (unpaired health OK) or 1=secure (pair required)."""
        from hcm_protocol import CHRC_SECURITY_PROFILE, decode_security_profile
        raw = await self.read(CHRC_SECURITY_PROFILE)
        profile = decode_security_profile(raw)
        return profile if profile is not None else 0

    async def get_pairing_status(self) -> Optional["PairingStatus"]:
        """Read watch-side SMP pairing state (CHRC f016). None if unavailable."""
        from hcm_protocol import CHRC_PAIRING_STATUS, PairingStatus, decode_pairing_status
        raw = await self.read(CHRC_PAIRING_STATUS)
        status = decode_pairing_status(raw)
        if status is None:
            return None
        return status

    async def set_ppg_preference(self, pref: int) -> None:
        """Set uint8 enum (0=unset, 1=auto, 2=max86141, 3=max3010x)."""
        await self.write(CHRC_PPG_PREF, encode_uint8(pref))

    # ------------------------------------------------------------------
    # Measurement control
    # ------------------------------------------------------------------

    async def start_hr(self) -> None:
        from hcm_protocol import MEAS_CMD_START, MEAS_TYPE_HR
        await self.write(CHRC_MEAS_CTRL, encode_meas_ctrl(MEAS_CMD_START, MEAS_TYPE_HR))
        log.info("HR measurement started")

    async def start_spo2(self) -> None:
        from hcm_protocol import MEAS_CMD_START, MEAS_TYPE_SPO2
        await self.write(CHRC_MEAS_CTRL, encode_meas_ctrl(MEAS_CMD_START, MEAS_TYPE_SPO2))
        log.info("SpO2 measurement started")

    async def start_vitals(self) -> None:
        """Start unified vitals measurement (HR + SpO2 + Hb + RespRate). Preferred over start_hr/start_spo2."""
        from hcm_protocol import MEAS_CMD_START, MEAS_TYPE_VITALS
        await self.write(CHRC_MEAS_CTRL, encode_meas_ctrl(MEAS_CMD_START, MEAS_TYPE_VITALS))
        log.info("Vitals measurement started")

    async def start_glucose(self) -> None:
        from hcm_protocol import MEAS_CMD_START, MEAS_TYPE_GLUCOSE
        await self.write(CHRC_MEAS_CTRL, encode_meas_ctrl(MEAS_CMD_START, MEAS_TYPE_GLUCOSE))
        log.info("Glucose measurement started")

    async def stop_measurement(self) -> None:
        from hcm_protocol import MEAS_CMD_STOP
        await self.write(CHRC_MEAS_CTRL, encode_meas_ctrl(MEAS_CMD_STOP, 0))
        log.info("Measurement stopped")

    async def get_proximity_status(self) -> Optional[ProximityStatus]:
        return decode_proximity_status(await self.read(CHRC_PROXIMITY))

    async def pmic_enable(self, target: int) -> None:
        await self.write(CHRC_PMIC_CTRL, encode_pmic_ctrl(PMIC_CMD_ENABLE, target), with_response=True)

    async def pmic_disable(self, target: int) -> None:
        await self.write(CHRC_PMIC_CTRL, encode_pmic_ctrl(PMIC_CMD_DISABLE, target), with_response=True)

    async def pmic_set_voltage(self, target: int, mv: int) -> None:
        await self.write(
            CHRC_PMIC_CTRL,
            encode_pmic_ctrl(PMIC_CMD_SET_VOLTAGE, target, mv),
            with_response=True,
        )

    # ------------------------------------------------------------------
    # WiFi operations
    # ------------------------------------------------------------------

    async def get_wifi_enable(self) -> bool:
        return bool((await self.read(CHRC_WIFI_ENABLE))[0])

    async def set_wifi_enable(self, enable: bool) -> None:
        await self.write(CHRC_WIFI_ENABLE, encode_uint8(1 if enable else 0))

    async def get_wifi_ssid(self) -> str:
        return (await self.read(CHRC_WIFI_SSID)).decode("utf-8", errors="replace")

    async def set_wifi_ssid(self, ssid: str) -> None:
        await self.write(CHRC_WIFI_SSID, encode_string(ssid, 32))

    async def set_wifi_password(self, password: str) -> None:
        await self.write(CHRC_WIFI_PASSWORD, encode_string(password, 64))

    async def wifi_connect(self) -> None:
        await self.write(CHRC_WIFI_CONNECT, bytes([1]))
        log.info("WiFi connect requested")

    async def wifi_disconnect(self) -> None:
        await self.write(CHRC_WIFI_CONNECT, bytes([0]))
        log.info("WiFi disconnect requested")

    async def admin_delete_bonds(self) -> None:
        """Write ADMIN_CMD_DELETE_BONDS to the firmware's admin control characteristic.

        This instructs the firmware to delete its NVS bond entry for the
        current connection's peer address, then disconnect.  The caller should
        subsequently call ``unpair()`` to also remove the OS-level bond.

        Raises RuntimeError if not connected, or if the characteristic is not
        found (older firmware).
        """
        if not (self._client and self._client.is_connected):
            raise RuntimeError("Not connected — cannot send admin_delete_bonds")
        await self._client.write_gatt_char(
            CHRC_ADMIN_CTRL,
            bytes([ADMIN_CMD_DELETE_BONDS]),
            response=False,
        )

    async def unpair(self, address: Optional[str] = None) -> None:
        """Unpair a bonded device when supported by the backend/platform."""
        target = address or self._connected_address
        if not target:
            raise RuntimeError("No device address provided for unpair")

        # Use active connection when possible.
        if self._client and self._client.is_connected and self._connected_address == target:
            if not hasattr(self._client, "unpair"):
                raise RuntimeError("Unpair is not supported on this platform")
            await self._client.unpair()
            self._connected = False
            self._services_ready = False
            self._client = None
            self._connected_address = None
            return

        temp_client = BleakClient(target)
        try:
            await temp_client.connect()
            if not hasattr(temp_client, "unpair"):
                raise RuntimeError("Unpair is not supported on this platform")
            await temp_client.unpair()
        finally:
            try:
                if temp_client.is_connected:
                    await temp_client.disconnect()
            except Exception:
                pass

    async def get_wifi_status(self) -> Optional[WifiStatus]:
        return decode_wifi_status(await self.read(CHRC_WIFI_STATUS))

    # ------------------------------------------------------------------
    # Notification decode helpers (call from subscription callback)
    # ------------------------------------------------------------------

    @staticmethod
    def decode_notification(uuid: str, data: bytes):
        """
        Decode a notification payload by UUID.
        Returns a typed dataclass or None if unknown/malformed.
        """
        decoders = {
            CHRC_PMIC:           decode_pmic,
            CHRC_PMIC_EXT:       decode_pmic_ext,
            CHRC_TEMPERATURE:    decode_temperature,
            CHRC_VITALS:         decode_vitals,
            CHRC_GLUCOSE:        decode_glucose,
            CHRC_GLUCOSE_SAMPLE: decode_glucose_sample,
            CHRC_PPG_STREAM:     decode_ppg_sample,
            CHRC_ACCEL_STREAM:   decode_accel_sample,
            CHRC_MEAS_STATUS:    decode_meas_status,
            CHRC_PROXIMITY:      decode_proximity_status,
            CHRC_WIFI_STATUS:    decode_wifi_status,
            CHRC_SENSOR_ALL:     decode_sensor_all,
        }
        decoder = decoders.get(uuid.lower())
        if decoder:
            return decoder(data)
        return None
