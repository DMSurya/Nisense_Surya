"""
Windows (WinRT) authenticated BLE pairing helper.

Bleak 0.22's built-in ``BleakClient.pair()`` only performs a *Just Works*
(``CONFIRM_ONLY``) ceremony, which does NOT satisfy a characteristic that
requires authenticated (MITM) encryption — e.g. the MCUmgr SMP characteristic
gated behind ``CONFIG_MCUMGR_TRANSPORT_BT_PERM_RW_AUTHEN``.

The NiSense firmware advertises a *DisplayYesNo* IO capability (it registers
both ``passkey_display`` and ``passkey_confirm``).  Paired with a Windows
*KeyboardDisplay* central this resolves to **Numeric Comparison**: both sides
show the same 6-digit code and the user confirms they match.  On the WinRT
side that maps to ``DevicePairingKinds.CONFIRM_PIN_MATCH``.

**Important:** Windows can start pairing when the app first touches an
encrypted GATT attribute (e.g. WiFi SSID).  A ``PairingRequested`` handler must
be registered for the *entire* connection so the UI can service any OS-driven
pairing prompt that appears during encrypted access.
"""
from __future__ import annotations

import asyncio
import logging
import subprocess
import sys
import time
from typing import Awaitable, Callable, Optional

log = logging.getLogger(__name__)

# Mirror Bleak's own import fallback (winrt -> bleak_winrt).
try:  # pragma: no cover - platform specific
    from winrt.windows.devices.bluetooth import BluetoothCacheMode, BluetoothLEDevice
    from winrt.windows.devices.bluetooth.genericattributeprofile import GattSession
    from winrt.windows.devices.enumeration import (
        DeviceInformation,
        DevicePairingKinds,
        DevicePairingProtectionLevel,
        DevicePairingResultStatus,
    )
except ImportError:  # pragma: no cover - older bleak wheels
    from bleak_winrt.windows.devices.bluetooth import (  # type: ignore
        BluetoothCacheMode,
        BluetoothLEDevice,
    )
    from bleak_winrt.windows.devices.bluetooth.genericattributeprofile import (  # type: ignore
        GattSession,
    )
    from bleak_winrt.windows.devices.enumeration import (  # type: ignore
        DeviceInformation,
        DevicePairingKinds,
        DevicePairingProtectionLevel,
        DevicePairingResultStatus,
    )


# Callback signature: (pin, kind_name) -> awaitable[bool]
ConfirmCallback = Callable[[str, str], Awaitable[bool]]
PAIR_ASYNC_TIMEOUT_SEC = 75.0
PAIR_BUSY_POLL_INTERVAL_SEC = 2.0
PAIR_BUSY_SETTLE_TIMEOUT_SEC = 90.0

_PAIRING_CEREMONY_MITM = (
    DevicePairingKinds.DISPLAY_PIN
    | DevicePairingKinds.PROVIDE_PIN
    | DevicePairingKinds.CONFIRM_PIN_MATCH
)

PAIRING_BUSY_MSG = (
    "Windows BLE pairing is wedged (OPERATION_ALREADY_IN_PROGRESS). This is a "
    "known Windows stack bug, not a watch fault. Recovery: watch Forget → "
    "HCM Monitor Clear PC Bond → wait 30 s → Firmware page Reset BT → reboot "
    "PC if still stuck. Then Connect once and Pair once."
)


def pairing_kind_label(kind) -> str:
    """Human-readable WinRT ``DevicePairingKinds`` name (``str()`` is often ``'8'``)."""
    for value, name in (
        (DevicePairingKinds.CONFIRM_PIN_MATCH, "CONFIRM_PIN_MATCH"),
        (DevicePairingKinds.DISPLAY_PIN, "DISPLAY_PIN"),
        (DevicePairingKinds.PROVIDE_PIN, "PROVIDE_PIN"),
        (DevicePairingKinds.CONFIRM_ONLY, "CONFIRM_ONLY"),
    ):
        if kind == value:
            return name
    return str(kind)


def pairing_kind_auto_accepts(kind) -> bool:
    """Numeric-comparison kinds: either side can Accept first (symmetric SMP)."""
    label = pairing_kind_label(kind).upper()
    if "CONFIRM_PIN_MATCH" in label or "DISPLAY_PIN" in label:
        return True
    try:
        return int(str(kind).strip()) in (8, 2)
    except ValueError:
        return False


def pairing_result_label(status) -> str:
    """Human-readable WinRT ``DevicePairingResultStatus`` name."""
    for value, name in (
        (DevicePairingResultStatus.PAIRED, "PAIRED"),
        (DevicePairingResultStatus.ALREADY_PAIRED, "ALREADY_PAIRED"),
        (DevicePairingResultStatus.NOT_READY_TO_PAIR, "NOT_READY_TO_PAIR"),
        (DevicePairingResultStatus.NOT_PAIRED, "NOT_PAIRED"),
        (DevicePairingResultStatus.OPERATION_ALREADY_IN_PROGRESS, "OPERATION_ALREADY_IN_PROGRESS"),
        (DevicePairingResultStatus.REQUIRED_HANDLER_NOT_REGISTERED, "REQUIRED_HANDLER_NOT_REGISTERED"),
        (DevicePairingResultStatus.PROTECTION_LEVEL_COULD_NOT_BE_MET, "PROTECTION_LEVEL_COULD_NOT_BE_MET"),
        (DevicePairingResultStatus.FAILED, "FAILED"),
        (DevicePairingResultStatus.AUTHENTICATION_FAILURE, "AUTHENTICATION_FAILURE"),
        (DevicePairingResultStatus.AUTHENTICATION_TIMEOUT, "AUTHENTICATION_TIMEOUT"),
    ):
        if status == value:
            return name
    return str(status)


_DEFAULT_PAIR_LEVELS = (
    DevicePairingProtectionLevel.ENCRYPTION_AND_AUTHENTICATION,
    DevicePairingProtectionLevel.ENCRYPTION,
)


async def _default_pair_on_information(pairing) -> str:
    """OS ``DeviceInformationPairing.PairAsync`` — no Custom handler."""
    if pairing.is_paired:
        return "paired"
    if not pairing.can_pair:
        log.warning("default PairAsync: can_pair=False")
        return "error"

    attempts = list(_DEFAULT_PAIR_LEVELS) + [None]
    last_status = None
    for level in attempts:
        label = "default" if level is None else repr(level)
        log.info("default PairAsync attempt (%s)", label)
        try:
            if level is None:
                coro = pairing.pair_async()
            else:
                coro = pairing.pair_async(level)
            result = await asyncio.wait_for(coro, timeout=PAIR_ASYNC_TIMEOUT_SEC)
        except asyncio.TimeoutError:
            log.warning("default PairAsync timed out after %ss", PAIR_ASYNC_TIMEOUT_SEC)
            return "timeout"
        except asyncio.CancelledError:
            return "disconnected"

        status = result.status
        last_status = status
        log.info(
            "default PairAsync result status=%s protection_used=%r",
            pairing_result_label(status),
            getattr(result, "protection_level_used", None),
        )
        if status in (
            DevicePairingResultStatus.PAIRED,
            DevicePairingResultStatus.ALREADY_PAIRED,
        ):
            return "paired"
        if status == DevicePairingResultStatus.OPERATION_ALREADY_IN_PROGRESS:
            return "busy_stale"
        if status in (
            DevicePairingResultStatus.PROTECTION_LEVEL_COULD_NOT_BE_MET,
            DevicePairingResultStatus.FAILED,
        ):
            continue

    log.warning(
        "default PairAsync exhausted attempts (last=%s)",
        pairing_result_label(last_status),
    )
    return "error"


def _backend_requester(bleak_client):
    """Return the WinRT BLEDevice requester from a frontend BleakClient."""
    backend = getattr(bleak_client, "_backend", None)
    if backend is None:
        raise RuntimeError("BleakClient has no WinRT backend")
    requester = getattr(backend, "_requester", None)
    if requester is None:
        raise RuntimeError("WinRT requester not available (device not connected?)")
    return requester


def _live_device_information(bleak_client) -> "DeviceInformation":
    """Return the DeviceInformation tied to the active Bleak link.

    For ``pair_async``, prefer a single ``create_from_id_async`` refresh (Bleak
    does this) rather than opening parallel ``BluetoothLEDevice`` handles.
    """
    requester = _backend_requester(bleak_client)
    dev_info = getattr(requester, "device_information", None)
    if dev_info is None:
        raise RuntimeError("WinRT requester has no device_information (not connected?)")
    return dev_info


async def _device_information(bleak_client) -> "DeviceInformation":
    """Return the live Bleak requester DeviceInformation."""
    return _live_device_information(bleak_client)


async def is_paired(bleak_client) -> bool:
    """True if the connected device is already paired/bonded at the OS level."""
    return await is_paired_session(bleak_client)


async def is_paired_session(bleak_client) -> bool:
    """True if the active Bleak link's DeviceInformation reports paired.

    Uses only the live requester ``DeviceInformation`` — never opens parallel
    WinRT device handles (those wedge ``pair_async``).
    """
    try:
        if not bool(getattr(bleak_client, "is_connected", False)):
            return False
        return bool(_live_device_information(bleak_client).pairing.is_paired)
    except Exception as exc:  # pragma: no cover - defensive
        log.warning("is_paired_session() check failed: %s", exc)
        return False


def _address_to_uint64(address: str) -> int:
    """Convert MAC string (AA:BB:CC:DD:EE:FF) to integer for WinRT APIs."""
    return int(address.replace(":", "").replace("-", ""), 16)


def _address_token(address: str) -> str:
    """Normalize a BLE MAC to uppercase hex token used in WinRT IDs."""
    return address.replace(":", "").replace("-", "").upper()


def _id_contains_address(info_id: str, token: str) -> bool:
    """Match MAC token against WinRT ids that use colon/dash-separated hex."""
    if not token:
        return False
    normalized = info_id.replace(":", "").replace("-", "").upper()
    return token in normalized


async def _fresh_device_information(dev_info) -> "DeviceInformation":
    """Return a refreshed ``DeviceInformation`` for ``pair_async`` (Bleak pattern)."""
    info_id = getattr(dev_info, "id", None)
    if not info_id:
        raise RuntimeError("device_information has no id")
    return await DeviceInformation.create_from_id_async(info_id)


async def _open_winrt_gatt_session(ble):
    """Open GattSession with ``maintain_connection`` — no GATT service read.

    ``get_gatt_services_async`` on SECURE firmware can start an implicit OS
    pairing ceremony that never fires ``PairingRequested``, wedging the next
    explicit ``pair_async`` in ``OPERATION_ALREADY_IN_PROGRESS``.
    """
    gatt_session = await GattSession.from_device_id_async(ble.bluetooth_device_id)
    if not gatt_session.can_maintain_connection:
        gatt_session.close()
        raise RuntimeError("device does not support GATT sessions")
    gatt_session.maintain_connection = True
    log.info("WinRT GattSession open (maintain_connection, services deferred)")
    return gatt_session


def _close_winrt_gatt_link(gatt_session, status_token=None) -> None:
    if gatt_session is None:
        return
    if status_token is not None:
        try:
            gatt_session.remove_session_status_changed(status_token)
        except Exception as exc:
            log.debug("remove_session_status_changed: %s", exc)
    try:
        gatt_session.maintain_connection = False
    except Exception as exc:
        log.debug("maintain_connection=False: %s", exc)
    try:
        gatt_session.close()
    except Exception as exc:
        log.debug("gatt session close: %s", exc)


async def _enumerate_ble_device_infos_by_selector(address: str) -> list:
    """Enumerate BLE DeviceInformation records and keep entries matching address."""
    token = _address_token(address)
    if not token:
        return []

    infos = []
    try:
        selector = BluetoothLEDevice.get_device_selector()
        # Python winrt requires the additional-properties overload (selector-only
        # raises "'str' object cannot be interpreted as an integer").
        results = await DeviceInformation.find_all_async(selector, [])
        for info in results:
            try:
                info_id = getattr(info, "id", "") or ""
                name = getattr(info, "name", "") or ""
                if _id_contains_address(info_id, token) or token in name.upper():
                    infos.append(info)
            except Exception:
                continue
    except Exception as exc:
        log.debug("selector enumeration failed: %s", exc)

    return infos


async def _candidate_device_infos(bleak_client, *, live_only: bool = False) -> list:
    """Return DeviceInformation objects that may hold pairing state.

    When *live_only* is True (connected session), use only the Bleak requester
    object — never ``from_bluetooth_address_async`` or enumeration.
    """
    infos = []
    seen_ids = set()

    async def _add(info):
        if info is None:
            return
        info_id = getattr(info, "id", None)
        if info_id and info_id not in seen_ids:
            seen_ids.add(info_id)
            infos.append(info)

    try:
        await _add(_live_device_information(bleak_client))
    except Exception:
        pass

    if live_only:
        return infos

    try:
        requester = _backend_requester(bleak_client)
        await _add(getattr(requester, "device_information", None))
    except Exception:
        pass

    try:
        addr = getattr(bleak_client, "address", "") or ""
        if addr:
            ble = await BluetoothLEDevice.from_bluetooth_address_async(_address_to_uint64(addr))
            if ble is not None:
                await _add(getattr(ble, "device_information", None))
                try:
                    ble.close()
                except Exception:
                    pass
    except Exception:
        pass

    try:
        addr = getattr(bleak_client, "address", "") or ""
        if addr:
            for info in await _enumerate_ble_device_infos_by_selector(addr):
                await _add(info)
    except Exception:
        pass

    return infos


async def is_paired_any(bleak_client) -> bool:
    """Return True if any known WinRT identity for this device is paired."""
    for info in await _candidate_device_infos(bleak_client):
        try:
            if bool(info.pairing.is_paired):
                return True
        except Exception:
            pass
    return False


async def wait_for_pairing_settled(
    bleak_client,
    *,
    timeout_sec: float = PAIR_BUSY_SETTLE_TIMEOUT_SEC,
    poll_interval_sec: float = PAIR_BUSY_POLL_INTERVAL_SEC,
) -> bool:
    """Wait for an in-flight WinRT pairing ceremony to finish.

    Windows cannot cancel an in-flight pairing ceremony; callers must poll
    until the device bonds or the OS clears the busy state before retrying.

    Aborts early if the BLE link drops: once the device disconnects the WinRT
    requester is gone, so ``is_paired_session()`` can only fail (it was spinning
    ~14×/"requester not available" in the field). A dropped link means the
    ceremony will never settle on this handle, so we stop and let the caller
    run a reconnect-based recovery instead.

    :returns: True if the device is paired when the wait ends.
    """
    deadline = time.monotonic() + timeout_sec
    while time.monotonic() < deadline:
        if not bool(getattr(bleak_client, "is_connected", False)):
            log.warning("wait_for_pairing_settled: link dropped — aborting wait")
            return False
        if await is_paired_session(bleak_client):
            log.info("Pairing settled — device is now paired")
            return True
        await asyncio.sleep(poll_interval_sec)
    return bool(getattr(bleak_client, "is_connected", False)) and \
        await is_paired_session(bleak_client)


class WinrtPairingSession:
    """Registers a pairing handler for the lifetime of one Bleak connection.

    Keeps numeric-comparison / passkey UI wired for pairing triggered by
    encrypted GATT access on the live connection.
    """

    def __init__(self) -> None:
        self._token: Optional[object] = None
        self._custom = None
        self._pairing = None
        self._confirm_cb: Optional[ConfirmCallback] = None
        self._loop: Optional[asyncio.AbstractEventLoop] = None
        self._address = ""
        self._explicit_pair_active = False
        self._ceremony_seen = False
        self._pair_lock = asyncio.Lock()

    @property
    def attached(self) -> bool:
        return self._token is not None

    def _ensure_handler_on(self, custom, pairing) -> None:
        """Register ``PairingRequested`` on the exact ``custom`` used for ``pair_async``.

        WinRT returns a new Python wrapper per ``DeviceInformation`` fetch; the
        handler must live on the same ``custom`` instance passed to ``pair_async``
        or Windows returns REQUIRED_HANDLER_NOT_REGISTERED (16).
        """
        if self._token is not None and self._custom is not None:
            try:
                self._custom.remove_pairing_requested(self._token)
            except Exception as exc:
                log.debug("remove_pairing_requested before re-bind: %s", exc)
            self._token = None

        self._pairing = pairing
        self._custom = custom
        self._token = custom.add_pairing_requested(self._on_pairing_requested)

    async def attach(
        self,
        bleak_client,
        confirm_cb: ConfirmCallback,
        loop: asyncio.AbstractEventLoop,
    ) -> None:
        """Register ``PairingRequested`` on the active Bleak link."""
        dev_info = _live_device_information(bleak_client)
        await self.attach_to_device_information(
            dev_info, confirm_cb, loop, getattr(bleak_client, "address", "") or ""
        )

    async def attach_to_device_information(
        self,
        dev_info,
        confirm_cb: ConfirmCallback,
        loop: asyncio.AbstractEventLoop,
        address: str,
    ) -> None:
        """Register ``PairingRequested`` on a WinRT ``DeviceInformation`` object."""
        pairing = dev_info.pairing
        custom = pairing.custom
        self._confirm_cb = confirm_cb
        self._loop = loop
        self._address = address or ""
        self._ensure_handler_on(custom, pairing)
        log.info(
            "WinRT pairing handler attached for %s (is_paired=%s can_pair=%s)",
            self._address,
            bool(pairing.is_paired),
            bool(pairing.can_pair),
        )

    def detach_handler_only(self) -> None:
        """Remove the WinRT handler token but keep confirm_cb/loop for OOB re-attach."""
        if self._token is not None and self._custom is not None:
            try:
                self._custom.remove_pairing_requested(self._token)
            except Exception as exc:
                log.debug("remove_pairing_requested: %s", exc)
        self._token = None
        self._custom = None
        self._pairing = None
        self._explicit_pair_active = False

    def detach(self) -> None:
        """Remove the handler when the BLE link is torn down."""
        self.detach_handler_only()
        self._confirm_cb = None
        self._loop = None
        self._address = ""
        self._ceremony_seen = False

    async def _pair_on_custom(self, custom, pairing) -> str:
        """Run ``pair_async`` using the session handler already on ``custom``."""
        if self._confirm_cb is None or self._loop is None:
            raise RuntimeError("WinRT pairing session not attached (no confirm callback)")

        self._ensure_handler_on(custom, pairing)
        self._explicit_pair_active = True
        self._ceremony_seen = False
        try:
            result = await asyncio.wait_for(
                custom.pair_async(
                    _PAIRING_CEREMONY_MITM,
                    DevicePairingProtectionLevel.ENCRYPTION_AND_AUTHENTICATION,
                ),
                timeout=PAIR_ASYNC_TIMEOUT_SEC,
            )
        except asyncio.TimeoutError as exc:
            if isinstance(exc.__cause__, asyncio.CancelledError):
                log.warning("pair_async cancelled (link dropped during pairing)")
                return "disconnected"
            log.warning("pair_async timed out after %ss", PAIR_ASYNC_TIMEOUT_SEC)
            return "timeout"
        except asyncio.CancelledError:
            log.warning("pair_async cancelled")
            return "disconnected"
        finally:
            self._explicit_pair_active = False

        status = result.status
        if status in (
            DevicePairingResultStatus.PAIRED,
            DevicePairingResultStatus.ALREADY_PAIRED,
        ):
            log.info("Paired (protection level %r)", result.protection_level_used)
            self._pairing = pairing
            self._custom = custom
            return "paired"

        if status == DevicePairingResultStatus.OPERATION_ALREADY_IN_PROGRESS:
            if not self._ceremony_seen:
                log.warning(
                    "pair_async returned OPERATION_ALREADY_IN_PROGRESS "
                    "(no PairingRequested — stale Windows ceremony)"
                )
                return "busy_stale"
            log.warning("pair_async returned OPERATION_ALREADY_IN_PROGRESS")
            return "busy"

        raise RuntimeError(f"Pairing failed: {status!r}")

    async def pair_default_protection(self, bleak_client) -> str:
        """Pair via standard ``DeviceInformationPairing.PairAsync`` (not Custom).

        Microsoft guidance for authenticated BLE: let the OS pick the ceremony
        instead of ``CustomPairing.PairAsync`` with explicit MITM kinds.
        """
        if not bool(getattr(bleak_client, "is_connected", False)):
            return "disconnected"

        async with self._pair_lock:
            pairing = _live_device_information(bleak_client).pairing
            address = getattr(bleak_client, "address", "") or ""
            log.info("Starting WinRT default PairAsync for %s", address)
            return await _default_pair_on_information(pairing)

    async def _run_pair_async(self, custom, pairing) -> str:
        """``pair_async`` wrapper that never raises — returns outcome strings only."""
        try:
            return await self._pair_on_custom(custom, pairing)
        except RuntimeError as exc:
            log.warning("WinRT pair_async failed: %s", exc)
            return "error"

    async def pair(self, bleak_client) -> str:
        """Explicitly start a WinRT MITM pairing ceremony via ``pair_async``.

        Uses the **live** Bleak requester ``DeviceInformation`` only — never
        ``create_from_id_async`` while a ``GattSession`` is open (second handle
        wedges ``pair_async`` in ``OPERATION_ALREADY_IN_PROGRESS``).
        """
        if not bool(getattr(bleak_client, "is_connected", False)):
            return "disconnected"

        async with self._pair_lock:
            address = getattr(bleak_client, "address", "") or ""
            self._address = address

            if self._custom is not None and self._pairing is not None and self.attached:
                pairing = self._pairing
                custom = self._custom
            else:
                live_di = _live_device_information(bleak_client)
                pairing = live_di.pairing
                custom = pairing.custom
                self._ensure_handler_on(custom, pairing)

            if pairing.is_paired:
                return "paired"
            if not pairing.can_pair:
                log.warning("Device reports it cannot be paired")
                return "error"

            log.info(
                "Starting WinRT pair_async for %s (is_paired=%s can_pair=%s)",
                address,
                bool(pairing.is_paired),
                bool(pairing.can_pair),
            )
            return await self._run_pair_async(custom, pairing)

    def _on_pairing_requested(self, sender, args) -> None:
        """Runs on a WinRT thread-pool thread."""
        self._ceremony_seen = True
        kind = args.pairing_kind
        loop = self._loop
        confirm_cb = self._confirm_cb
        if loop is None or confirm_cb is None:
            log.warning("PairingRequested with no session callback — completing deferral")
            try:
                deferral = args.get_deferral()
                deferral.complete()
            except Exception as exc:
                log.warning("PairingRequested orphan deferral: %s", exc)
            return

        log.info(
            "WinRT PairingRequested (kind=%s explicit=%s)",
            pairing_kind_label(kind),
            self._explicit_pair_active,
        )

        if kind == DevicePairingKinds.CONFIRM_ONLY:
            if not self._explicit_pair_active:
                log.warning(
                    "Rejecting implicit CONFIRM_ONLY (Just Works) — MITM required "
                    "(if pair_async then returns busy_stale, run oob-pair with no prior connect)"
                )
                # Must complete the deferral even when rejecting; returning without
                # GetDeferral/Complete leaves Windows in OPERATION_ALREADY_IN_PROGRESS
                # until reboot (seen after first connect on SECURE firmware).
                try:
                    deferral = args.get_deferral()
                    deferral.complete()
                except Exception as exc:
                    log.warning("CONFIRM_ONLY implicit reject deferral: %s", exc)
                return
            try:
                args.accept()
            except Exception as exc:
                log.warning("CONFIRM_ONLY accept failed: %s", exc)
            return

        deferral = args.get_deferral()
        pin = ""
        try:
            pin = (args.pin or "").strip()
        except Exception:
            pin = ""

        if not pin:
            try:
                pin = (getattr(args, "pin", None) or "").strip()
            except Exception:
                pin = ""

        log.info(
            "WinRT pairing ceremony requested (kind=%s pin=%s)",
            pairing_kind_label(kind),
            pin or "<none>",
        )

        if not pin and kind in (
            DevicePairingKinds.CONFIRM_PIN_MATCH,
            DevicePairingKinds.DISPLAY_PIN,
            DevicePairingKinds.PROVIDE_PIN,
        ):
            log.warning(
                "PairingRequested kind=%s but PIN empty — compare code on watch LCD",
                kind,
            )

        fut = asyncio.run_coroutine_threadsafe(
            confirm_cb(pin, pairing_kind_label(kind)), loop
        )

        def _on_done(f):
            try:
                accepted = bool(f.result())
            except Exception as exc:
                log.warning("pairing confirm callback failed: %s", exc)
                accepted = False
            try:
                if accepted:
                    if kind == DevicePairingKinds.PROVIDE_PIN:
                        args.accept(pin)
                    else:
                        args.accept()
            except Exception as exc:
                log.warning("pairing accept failed: %s", exc)
            finally:
                try:
                    deferral.complete()
                except Exception:
                    pass

        fut.add_done_callback(_on_done)

async def unpair(bleak_client) -> bool:
    """Remove the OS-level bond for the connected device."""
    dev_info = _live_device_information(bleak_client)
    result = await dev_info.pairing.unpair_async()
    return result.status == 0  # DeviceUnpairingResultStatus.UNPAIRED


async def unpair_all(bleak_client) -> bool:
    """Best-effort unpair across all known WinRT identities for this device."""
    any_changed = False
    infos = await _candidate_device_infos(bleak_client)
    for info in infos:
        try:
            pairing = info.pairing
            if not bool(pairing.is_paired):
                continue
            result = await pairing.unpair_async()
            status = getattr(result, "status", None)
            if status == 0:
                any_changed = True
            log.info("unpair_all: %s -> %r", getattr(info, "id", "<unknown>"), status)
            await asyncio.sleep(0.3)
        except Exception as exc:
            log.warning("unpair_all: failed on %s: %s", getattr(info, "id", "<unknown>"), exc)
    return any_changed


async def _close_device_information(info) -> None:
    try:
        close = getattr(info, "close", None)
        if callable(close):
            close()
    except Exception:
        pass


async def forget_device_pairing(address: str, *, bleak_client=None) -> bool:
    """Unpair all known WinRT identities for a MAC.

    While a Bleak link is active, only touch the live requester
    ``DeviceInformation``.  Opening ``from_bluetooth_address_async`` or
    enumerating extra handles during an active session wedges ``pair_async``.
    """
    if not address:
        return False

    connected = bleak_client is not None and bool(
        getattr(bleak_client, "is_connected", False)
    )
    log.info(
        "Forgetting WinRT pairing for %s (connected=%s)",
        address,
        connected,
    )

    infos = []
    if connected:
        try:
            infos.extend(
                await _candidate_device_infos(bleak_client, live_only=True)
            )
        except Exception as exc:
            log.debug("live forget candidate infos: %s", exc)
    else:
        if bleak_client is not None:
            try:
                infos.extend(await _candidate_device_infos(bleak_client))
            except Exception:
                pass
        if not infos:
            for info in await _enumerate_ble_device_infos_by_selector(address):
                infos.append(info)
        if not infos:
            try:
                ble = await resolve_bluetooth_le_device(address, timeout=8.0)
                if ble is not None:
                    di = getattr(ble, "device_information", None)
                    if di is not None:
                        infos.append(di)
                    try:
                        ble.close()
                    except Exception:
                        pass
            except Exception as exc:
                log.debug("resolve in forget: %s", exc)

    if not infos:
        log.warning("No WinRT device identities found for %s", address)

    any_action = False
    seen = set()
    for info in infos:
        info_id = getattr(info, "id", None)
        if not info_id or info_id in seen:
            continue
        seen.add(info_id)
        try:
            result = await info.pairing.unpair_async()
            log.info("forget unpair: %s -> %r", info_id, getattr(result, "status", None))
            any_action = True
        except Exception as exc:
            log.debug("forget unpair failed: %s", exc)
        await _close_device_information(info)
        await asyncio.sleep(0.1)

    await asyncio.sleep(2.0)
    return any_action


async def restart_bluetooth_service() -> bool:
    if sys.platform != "win32":
        return False
    ps = r"""
try {
  Restart-Service -Name bthserv -Force -ErrorAction Stop
  'ok'
} catch {
  $_.Exception.Message
}
"""
    try:
        proc = await asyncio.to_thread(
            subprocess.run,
            ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", ps],
            capture_output=True,
            text=True,
            timeout=60,
            check=False,
        )
        ok = "ok" in (proc.stdout or "")
        if ok:
            log.info("Bluetooth Support Service (bthserv) restarted")
            await asyncio.sleep(3.0)
        else:
            log.warning(
                "bthserv restart failed (needs Administrator?): %s",
                (proc.stderr or proc.stdout or "").strip()[:300],
            )
        return ok
    except Exception as exc:
        log.warning("restart_bluetooth_service failed: %s", exc)
        return False


async def bounce_bluetooth_radio() -> bool:
    if sys.platform != "win32":
        return False
    ps = r"""
$changed = $false
Get-PnpDevice -Class Bluetooth -ErrorAction SilentlyContinue |
  Where-Object { $_.InstanceId -notmatch 'BTH\\MS' } |
  ForEach-Object {
    try {
      Disable-PnpDevice -InstanceId $_.InstanceId -Confirm:$false -ErrorAction SilentlyContinue
      $changed = $true
    } catch {}
  }
Start-Sleep -Seconds 2
Get-PnpDevice -Class Bluetooth -ErrorAction SilentlyContinue |
  Where-Object { $_.InstanceId -notmatch 'BTH\\MS' } |
  ForEach-Object {
    try {
      Enable-PnpDevice -InstanceId $_.InstanceId -Confirm:$false -ErrorAction SilentlyContinue
    } catch {}
  }
if ($changed) { Write-Output 'ok' }
"""
    try:
        proc = await asyncio.to_thread(
            subprocess.run,
            ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", ps],
            capture_output=True,
            text=True,
            timeout=120,
            check=False,
        )
        ok = "ok" in (proc.stdout or "")
        if ok:
            log.info("Bluetooth radio bounced")
            await asyncio.sleep(2.0)
        return ok
    except Exception as exc:
        log.warning("bounce_bluetooth_radio failed: %s", exc)
        return False


async def reset_bluetooth_stack(
    address: str,
    *,
    bounce_radio: bool = True,
    restart_service: bool = False,
) -> bool:
    log.info("Full Bluetooth stack reset for %s", address)
    await forget_device_pairing(address)
    ok = False
    if bounce_radio:
        ok = await bounce_bluetooth_radio() or ok
    if restart_service:
        ok = await restart_bluetooth_service() or ok
    return ok


def _winrt_device_id_str(device_id) -> str:
    """Normalize WinRT device id (str in pywinrt, object with .id in bleak_winrt)."""
    if device_id is None:
        return "?"
    if isinstance(device_id, str):
        return device_id
    return str(getattr(device_id, "id", device_id))


async def resolve_bluetooth_le_device(
    address: str,
    *,
    timeout: float = 15.0,
):
    """Open ``BluetoothLEDevice`` using the same scan-derived address Bleak uses.

    ``from_bluetooth_address_async(int(MAC))`` without a prior scan can return a
    different WinRT identity than enumeration/Bleak connect — ``can_pair=True``
    but immediate ``OPERATION_ALREADY_IN_PROGRESS`` with no ``PairingRequested``.
    """
    from bleak import BleakScanner
    from bleak.backends.winrt.scanner import BleakScannerWinRT

    device = await BleakScanner.find_device_by_address(
        address, timeout=timeout, backend=BleakScannerWinRT
    )
    if device is None:
        log.warning("resolve_bluetooth_le_device: %s not found in scan", address)
        return None

    data = device.details
    evt = (data.adv or data.scan) if data is not None else None
    if evt is None:
        log.warning("resolve_bluetooth_le_device: no advertisement payload for %s", address)
        return None

    bt_addr = evt.bluetooth_address
    addr_type = getattr(evt, "bluetooth_address_type", None)
    try:
        if addr_type is not None:
            ble = await BluetoothLEDevice.from_bluetooth_address_async(bt_addr, addr_type)
        else:
            ble = await BluetoothLEDevice.from_bluetooth_address_async(bt_addr)
    except TypeError:
        ble = await BluetoothLEDevice.from_bluetooth_address_async(bt_addr)

    if ble is None:
        log.warning("resolve_bluetooth_le_device: WinRT open failed for %s", address)
        return None

    log.info(
        "resolve_bluetooth_le_device: %s -> id=%s name=%r",
        address,
        _winrt_device_id_str(getattr(ble, "device_id", None)),
        getattr(ble, "name", None),
    )
    return ble


async def resolve_device_information_id(
    address: str,
    *,
    timeout: float = 20.0,
) -> Optional[str]:
    """Scan-resolve a WinRT ``DeviceInformation.id`` and close the BLE handle."""
    ble = await resolve_bluetooth_le_device(address, timeout=timeout)
    if ble is None:
        return None
    try:
        return ble.device_information.id
    finally:
        try:
            ble.close()
        except Exception as exc:
            log.debug("resolve_device_information_id close: %s", exc)


async def _try_clear_phantom_pairing(dev_info) -> None:
    """Best-effort unpair when Windows reports an in-flight phantom ceremony."""
    pairing = dev_info.pairing
    if pairing.is_paired:
        return
    try:
        result = await pairing.unpair_async()
        log.info("phantom unpair_async status=%r", result.status)
    except Exception as exc:
        log.debug("phantom unpair_async: %s", exc)


async def is_paired_os(address: str) -> bool:
    """True if Windows reports a bond for *address* without an active GattSession."""
    ble = None
    try:
        ble = await resolve_bluetooth_le_device(address, timeout=10.0)
        if ble is None:
            return False
        return bool(ble.device_information.pairing.is_paired)
    except Exception as exc:
        log.debug("is_paired_os(%s): %s", address, exc)
        return False
    finally:
        if ble is not None:
            try:
                ble.close()
            except Exception:
                pass


async def winrt_oob_mitm_pair(
    address: str,
    session: "WinrtPairingSession",
    confirm_cb: ConfirmCallback,
    loop: asyncio.AbstractEventLoop,
) -> str:
    """Run MITM ``pair_async`` via ``DeviceInformation`` only — no open BLE link.

    Holding ``BluetoothLEDevice`` / ``GattSession`` handles while calling
    ``pair_async`` often returns immediate ``OPERATION_ALREADY_IN_PROGRESS``
    with no ``PairingRequested``.  Resolve the device id from scan, close the
    transient handle, then pair through ``create_from_id_async`` (Windows
    Settings style).
    """
    log.info("WinRT OOB pair (DeviceInformation only) for %s", address)
    try:
        di_id = await resolve_device_information_id(address, timeout=20.0)
        if di_id is None:
            log.warning("OOB pair: device not found at %s", address)
            return "disconnected"

        await asyncio.sleep(0.5)

        dev_info = await DeviceInformation.create_from_id_async(di_id)
        pairing = dev_info.pairing
        if pairing.is_paired:
            log.info("OOB pair: already paired at OS")
            return "paired"
        if not pairing.can_pair:
            log.warning("OOB pair: can_pair=False")
            return "error"

        await _try_clear_phantom_pairing(dev_info)
        await asyncio.sleep(1.5)
        dev_info = await DeviceInformation.create_from_id_async(di_id)
        pairing = dev_info.pairing
        custom = pairing.custom

        await session.attach_to_device_information(
            dev_info, confirm_cb, loop, address
        )
        log.info("OOB Custom pair_async (MITM, no GattSession)")
        outcome = await session._run_pair_async(custom, pairing)
        if outcome != "busy_stale":
            return outcome

        log.warning("OOB pair busy_stale — phantom unpair + refresh, retry once")
        await asyncio.sleep(3.0)
        await _try_clear_phantom_pairing(dev_info)
        await asyncio.sleep(1.0)
        dev_info = await DeviceInformation.create_from_id_async(di_id)
        pairing = dev_info.pairing
        custom = pairing.custom
        await session.attach_to_device_information(
            dev_info, confirm_cb, loop, address
        )
        return await session._run_pair_async(custom, pairing)
    except Exception as exc:
        log.warning(
            "OOB pair failed: %s: %r",
            type(exc).__name__,
            exc,
            exc_info=log.isEnabledFor(logging.DEBUG),
        )
        return "error"


async def winrt_default_oob_pair(address: str) -> str:
    """Default OS ``PairAsync`` on ``DeviceInformation`` only — no Custom handler."""
    log.info("WinRT default OOB pair for %s", address)
    try:
        di_id = await resolve_device_information_id(address, timeout=20.0)
        if di_id is None:
            return "disconnected"
        await asyncio.sleep(0.5)
        dev_info = await DeviceInformation.create_from_id_async(di_id)
        return await _default_pair_on_information(dev_info.pairing)
    except Exception as exc:
        log.warning(
            "default OOB pair failed: %s: %r",
            type(exc).__name__,
            exc,
            exc_info=log.isEnabledFor(logging.DEBUG),
        )
        return "error"


def _bleak_real_get_services(backend):
    """Return Bleak's unpatched ``get_services`` for *backend*."""
    stored = getattr(backend, "_winrt_orig_get_services", None)
    if stored is not None:
        return stored
    from bleak.backends.winrt.client import BleakClientWinRT

    return BleakClientWinRT.get_services.__get__(backend, type(backend))


async def _force_full_gatt_discovery(backend) -> int:
    """Run uncached GATT discovery and assign ``backend.services``.

    After *defer_services* connect, ``backend.services`` may be an empty
    collection; Bleak's ``get_services()`` then returns early without walking
    characteristics.  Always clear before rediscovery.

    Uses the stored unpatched ``get_services`` (connect hooks replace the bound
    method with defer/link-only stubs that must not run post-connect).
    """
    import warnings

    get_services = _bleak_real_get_services(backend)
    if get_services is not getattr(backend, "get_services", None):
        backend.get_services = get_services  # type: ignore[method-assign]

    backend.services = None
    log.info("GATT discovery: get_services (UNCACHED)")

    last_exc: Optional[BaseException] = None
    for attempt in (1, 2):
        try:
            with warnings.catch_warnings():
                warnings.simplefilter("ignore", FutureWarning)
                collection = await get_services(
                    service_cache_mode=BluetoothCacheMode.UNCACHED,
                    cache_mode=BluetoothCacheMode.UNCACHED,
                )
            backend.services = collection
            n_services = len(collection.services)
            n_chars = len(collection.characteristics)
            log.info(
                "GATT discovery complete: %d services, %d characteristics",
                n_services,
                n_chars,
            )
            return n_services
        except asyncio.CancelledError as exc:
            last_exc = exc
            if attempt == 1 and bool(getattr(backend, "is_connected", False)):
                log.warning("GATT discovery cancelled — retrying once")
                backend.services = None
                await asyncio.sleep(1.0)
                continue
            raise RuntimeError("GATT discovery cancelled (link dropped?)") from exc
        except BaseException as exc:
            if not bool(getattr(backend, "is_connected", False)):
                raise RuntimeError("GATT discovery failed: not connected") from exc
            raise

    raise RuntimeError("GATT discovery failed") from last_exc


def _gatt_error_needs_pair(exc: BaseException) -> bool:
    """True when a GATT read failed because the link is connected but not bonded."""
    msg = str(exc).lower()
    return (
        "insufficient authentication" in msg
        or "insufficient encryption" in msg
        or "0x05" in msg
        or "0x0f" in msg
    )


async def _escalate_pair_after_gatt_auth(
    session: "WinrtPairingSession",
    bleak_client,
    address: str,
) -> str:
    """Pair after auth-gated GATT read — Custom MITM in-session, then OOB fallback.

    Default ``DeviceInformationPairing.PairAsync`` returns instant FAILED with
    ``protection_used=NONE`` on SECURE MITM firmware and poisons the next
    ``CustomPairing.pair_async`` with ``OPERATION_ALREADY_IN_PROGRESS``.  Skip it.
    """
    confirm_cb = session._confirm_cb
    loop = session._loop
    if confirm_cb is None or loop is None:
        return "error"

    log.info(
        "GATT auth gate hit — Custom pair_async (MITM, in-session) "
        "(Windows does not auto-start SMP on Insufficient Authentication alone)"
    )
    outcome = await session.pair(bleak_client)
    if outcome == "paired":
        return "paired"
    if outcome == "busy" and session._ceremony_seen:
        if await wait_for_pairing_settled(bleak_client, timeout_sec=60.0):
            return "paired"
        outcome = "busy_stale"
    if outcome not in ("error", "busy_stale", "busy"):
        return outcome

    log.info(
        "In-session Custom pair_async returned %s — disconnecting for OOB pair "
        "(no default PairAsync — it wedges Custom pair_async on SECURE firmware)",
        outcome,
    )
    session.detach_handler_only()
    try:
        await bleak_client.disconnect()
    except Exception as exc:
        log.debug("disconnect before OOB pair: %s", exc)
    await asyncio.sleep(2.0)
    return await winrt_oob_mitm_pair(address, session, confirm_cb, loop)


async def _pair_connected_then_oob(
    session: "WinrtPairingSession",
    bleak_client,
    address: str,
    *,
    confirm_cb: ConfirmCallback,
    loop: asyncio.AbstractEventLoop,
    recover_on_busy: bool = True,
) -> str:
    """Custom MITM on a defer-GATT link; OOB + radio bounce if WinRT is wedged."""
    log.info(
        "Pair-first: Custom pair_async on defer-GATT link "
        "(no characteristic enumeration before pair)"
    )
    outcome = await session.pair(bleak_client)
    if outcome == "paired":
        return "paired"
    if outcome == "busy" and session._ceremony_seen:
        if await wait_for_pairing_settled(bleak_client, timeout_sec=60.0):
            return "paired"
        outcome = "busy_stale"
    if outcome not in ("error", "busy_stale", "busy"):
        return outcome

    log.info("In-session pair returned %s — disconnecting for OOB pair", outcome)
    session.detach_handler_only()
    try:
        await bleak_client.disconnect()
    except Exception as exc:
        log.debug("disconnect before OOB pair: %s", exc)
    await asyncio.sleep(2.0)

    if recover_on_busy and outcome == "busy_stale":
        log.info("busy_stale — forget + radio bounce before OOB pair")
        await reset_bluetooth_stack(address, bounce_radio=True)
        await asyncio.sleep(5.0)

    return await winrt_oob_mitm_pair(address, session, confirm_cb, loop)


async def winrt_gatt_trigger_pair(
    bleak_client,
    session: "WinrtPairingSession",
    *,
    address: str,
    trigger_uuid: str,
    timeout: float = 90.0,
    pair_first: bool = True,
) -> str:
    """Pair on Windows SECURE firmware, optionally verify with an auth GATT read.

    **Default (``pair_first=True``):** Custom ``pair_async`` on a defer-GATT link
    *before* any service/characteristic enumeration.  Full GATT discovery before
    ``pair_async`` wedges WinRT in ``OPERATION_ALREADY_IN_PROGRESS`` (no passkey).

    **Legacy (``pair_first=False``):** discover all services, read auth char, then
    pair — kept for regression comparison only; do not use in production.
    """
    if not bool(getattr(bleak_client, "is_connected", False)):
        return "disconnected"

    confirm_cb = session._confirm_cb
    loop = session._loop
    if confirm_cb is None or loop is None:
        return "error"

    if pair_first:
        outcome = await _pair_connected_then_oob(
            session, bleak_client, address, confirm_cb=confirm_cb, loop=loop
        )
        if outcome != "paired":
            return outcome
        if not trigger_uuid:
            return "paired"
        backend = getattr(bleak_client, "_backend", None)
        if backend is None:
            return "paired"
        try:
            await _force_full_gatt_discovery(backend)
            data = await bleak_client.read_gatt_char(trigger_uuid)
            log.info("Post-pair auth char read ok (%d bytes)", len(data or b""))
        except Exception as exc:
            log.warning("Post-pair auth char read failed: %s", exc)
            return "paired" if await is_paired_session(bleak_client) else "error"
        return "paired"

    backend = getattr(bleak_client, "_backend", None)
    if backend is None:
        return "error"

    try:
        n_services = await _force_full_gatt_discovery(backend)
    except asyncio.CancelledError:
        return "disconnected"
    except RuntimeError as exc:
        log.warning("GATT discovery failed: %s", exc)
        return "disconnected" if "not connected" in str(exc).lower() or "cancelled" in str(exc).lower() else "error"
    except Exception as exc:
        log.warning("GATT discovery failed: %s", exc)
        return "error"

    if session._ceremony_seen:
        log.info("PairingRequested fired during GATT discovery")

    if n_services == 0:
        log.warning("GATT discovery returned no services")
        return "error"

    log.info("GATT trigger: read %s", trigger_uuid)
    read_exc = None
    try:
        data = await bleak_client.read_gatt_char(trigger_uuid)
        log.info("GATT trigger read ok (%d bytes) — already bonded?", len(data or b""))
        if await is_paired_session(bleak_client):
            return "paired"
    except Exception as exc:
        read_exc = exc
        log.info("GATT trigger read raised: %s", exc)

    if session._ceremony_seen:
        log.info("PairingRequested fired during auth char read")

    if read_exc is not None and _gatt_error_needs_pair(read_exc):
        log.warning(
            "Legacy gatt-trigger path: auth read before pair wedges WinRT — "
            "use pair_first=True (default)"
        )
        outcome = await _escalate_pair_after_gatt_auth(session, bleak_client, address)
        if outcome == "paired":
            return "paired"
        return outcome

    if read_exc is None:
        return "paired" if await is_paired_session(bleak_client) else "error"

    deadline = time.monotonic() + min(timeout, 30.0)
    while time.monotonic() < deadline:
        try:
            if await is_paired_session(bleak_client):
                log.info("GATT trigger: device now paired at OS")
                return "paired"
        except Exception:
            pass
        if session._ceremony_seen:
            await asyncio.sleep(0.25)
            continue
        await asyncio.sleep(0.5)

    if session._ceremony_seen:
        log.warning("GATT trigger: ceremony seen but bond not confirmed")
        return "timeout"
    log.warning("GATT trigger: no PairingRequested and not paired")
    return "error"


async def _defer_get_services(backend, *args, **kwargs) -> object:
    """Skip WinRT GATT reads during connect — pair before any service access."""
    from bleak.backends.service import BleakGATTServiceCollection

    log.info("WinRT connect: deferring GATT service discovery until after pair")
    return BleakGATTServiceCollection()


async def _link_only_get_services(backend, *args, **kwargs) -> object:
    """Bring up the WinRT GATT session without walking characteristics.

    Bleak's full ``get_services()`` enumerates every characteristic and
    descriptor.  On Windows SECURE firmware that can start an implicit pairing
    ceremony (often without ``PairingRequested``) and wedge the next
    ``pair_async`` in ``OPERATION_ALREADY_IN_PROGRESS``.
    """
    from bleak.backends.service import BleakGATTServiceCollection
    from bleak.backends.winrt.client import FutureLike, _ensure_success

    if backend.services is not None:
        return backend.services

    log.info("WinRT link-up: GATT service list only (no characteristic walk)")
    services = _ensure_success(
        await FutureLike(
            backend._requester.get_gatt_services_async(BluetoothCacheMode.CACHED)
        ),
        "services",
        "Could not get GATT services",
    )
    try:
        await asyncio.sleep(0.1)
    finally:
        for service in services:
            try:
                service.close()
            except Exception as exc:
                log.debug("link-only service close: %s", exc)

    return BleakGATTServiceCollection()


def clear_bleak_gatt_cache(bleak_client) -> None:
    """Drop Bleak's in-connect service table so a later discovery can run."""
    backend = getattr(bleak_client, "_backend", None)
    if backend is not None:
        backend.services = None


async def bleak_connect_with_pairing_handler(
    bleak_client,
    attach_fn: Callable[[object], Awaitable[None]],
    *,
    lazy_services: bool = False,
    link_only_services: bool = False,
    defer_services: bool = False,
    **connect_kwargs,
) -> bool:
    """Connect via Bleak after registering ``PairingRequested``.

    Hooks ``GattSession.from_device_id_async`` so the pairing handler is live
    before Bleak's in-connect ``get_services()``.

    When *defer_services* is True, skip all GATT reads during connect — use
    before ``pair_async`` on Windows SECURE (even cached service list can wedge
    pairing).  After bond, run normal service discovery.

    When *link_only_services* is True, only ``get_gatt_services_async(CACHED)``
    runs — no characteristic/descriptor walk.  Prefer *defer_services* for
    unbonded SECURE devices.

    When *lazy_services* is True (and neither defer nor link_only), service
    discovery uses OS-cached GATT metadata only.
    """
    hook = {"attach": attach_fn, "done": False}
    orig_session_factory = GattSession.from_device_id_async
    backend = getattr(bleak_client, "_backend", None)
    orig_get_services = getattr(backend, "get_services", None) if backend else None

    async def from_device_id_hook(device_id):
        if not hook["done"]:
            hook["done"] = True
            log.info("Attaching WinRT pairing handler before Bleak GATT discovery")
            await hook["attach"](bleak_client)
        return await orig_session_factory(device_id)

    async def cached_get_services(*args, **kwargs):
        if orig_get_services is None:
            raise RuntimeError("Bleak backend has no get_services()")
        return await orig_get_services(
            service_cache_mode=BluetoothCacheMode.CACHED,
            cache_mode=BluetoothCacheMode.CACHED,
        )

    async def link_only_hook(*args, **kwargs):
        return await _link_only_get_services(backend)

    async def defer_get_services_hook(*args, **kwargs):
        return await _defer_get_services(backend)

    patch_get_services = None
    if defer_services and backend is not None and orig_get_services is not None:
        patch_get_services = defer_get_services_hook
    elif link_only_services and backend is not None and orig_get_services is not None:
        patch_get_services = link_only_hook
    elif lazy_services and backend is not None and orig_get_services is not None:
        patch_get_services = cached_get_services

    if backend is not None and orig_get_services is not None:
        backend._winrt_orig_get_services = orig_get_services  # type: ignore[attr-defined]

    GattSession.from_device_id_async = from_device_id_hook  # type: ignore[method-assign]
    if patch_get_services is not None:
        backend.get_services = patch_get_services  # type: ignore[method-assign]
    try:
        ok = await bleak_client.connect(**connect_kwargs)
        if defer_services or link_only_services:
            clear_bleak_gatt_cache(bleak_client)
        return ok
    except Exception as exc:
        log.error("Bleak connect failed: %s", exc)
        raise
    finally:
        GattSession.from_device_id_async = orig_session_factory  # type: ignore[method-assign]
        if patch_get_services is not None and backend is not None and orig_get_services is not None:
            backend.get_services = orig_get_services  # type: ignore[method-assign]


async def bleak_connect_defer_only(bleak_client, **connect_kwargs) -> bool:
    """Connect with GATT discovery deferred and **no** ``PairingRequested`` hook."""
    backend = getattr(bleak_client, "_backend", None)
    orig_get_services = getattr(backend, "get_services", None) if backend else None

    async def defer_get_services_hook(*args, **kwargs):
        return await _defer_get_services(backend)

    patch_get_services = None
    if backend is not None and orig_get_services is not None:
        backend._winrt_orig_get_services = orig_get_services  # type: ignore[attr-defined]
        patch_get_services = defer_get_services_hook

    if patch_get_services is not None:
        backend.get_services = patch_get_services  # type: ignore[method-assign]
    try:
        ok = await bleak_client.connect(**connect_kwargs)
        if patch_get_services is not None:
            clear_bleak_gatt_cache(bleak_client)
        return ok
    finally:
        if patch_get_services is not None and backend is not None and orig_get_services is not None:
            backend.get_services = orig_get_services  # type: ignore[method-assign]