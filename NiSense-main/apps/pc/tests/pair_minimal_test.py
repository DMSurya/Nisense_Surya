#!/usr/bin/env python3
"""Minimal WinRT pairing repro — no hcm_backend, no recovery ladder.

Use after watch Forget + ``reset_ble_pairing.py --full-reset``.

Examples::

    python pair_minimal_test.py D5:C9:63:90:78:40 scan
    python pair_minimal_test.py D5:C9:63:90:78:40 gatt-pair
    python pair_minimal_test.py D5:C9:63:90:78:40 gatt-pair-legacy
    python pair_minimal_test.py D5:C9:63:90:78:40 default-oob
    python pair_minimal_test.py D5:C9:63:90:78:40 default-pair
"""
from __future__ import annotations

import argparse
import asyncio
import logging
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)-8s %(name)s: %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("pair_minimal_test")


async def _confirm_cb(pin: str, kind: str) -> bool:
    print(f"\n>>> PairingRequested kind={kind} pin={pin or '<none>'}")
    print(">>> Type y + Enter to Accept on PC (watch can Accept first either order)")
    try:
        answer = await asyncio.to_thread(input, "Accept on PC? [y/N]: ")
    except EOFError:
        return False
    return answer.strip().lower() in ("y", "yes")


async def _find_device(address: str):
    from bleak import BleakScanner
    from bleak.backends.winrt.scanner import BleakScannerWinRT

    for attempt in (1, 2):
        device = await BleakScanner.find_device_by_address(
            address, timeout=25.0, backend=BleakScannerWinRT
        )
        if device is not None:
            return device
        if attempt == 1:
            log.warning("Scan pass 1: %s not seen — retrying once", address)
            await asyncio.sleep(2.0)
    return None


async def cmd_scan(address: str) -> int:
    from winrt_pairing import (
        _enumerate_ble_device_infos_by_selector,
        _winrt_device_id_str,
        resolve_bluetooth_le_device,
    )

    print(f"\n=== scan-resolve: {address} ===")
    ble = await resolve_bluetooth_le_device(address, timeout=20.0)
    if ble is None:
        print("FAIL: not found via Bleak scan + WinRT open")
        return 1

    di = ble.device_information
    pairing = di.pairing
    print(f"  device_id: {_winrt_device_id_str(ble.device_id)}")
    print(f"  name:      {getattr(ble, 'name', '')!r}")
    print(f"  is_paired: {pairing.is_paired}")
    print(f"  can_pair:  {pairing.can_pair}")
    ble.close()

    infos = await _enumerate_ble_device_infos_by_selector(address)
    print(f"\n=== enumeration ({len(infos)} match) ===")
    for info in infos:
        p = info.pairing
        print(f"  id={info.id}")
        print(f"    is_paired={p.is_paired} can_pair={p.can_pair}")
    return 0


async def cmd_oob_pair(address: str) -> int:
    from winrt_pairing import WinrtPairingSession, winrt_oob_mitm_pair

    print(f"\n=== oob-pair: {address} (Custom pair_async, NO Bleak connect) ===")
    print("Run this FIRST after watch Forget + reset_ble_pairing --full-reset.")
    print("Do NOT run connect-pair or gatt-pair before this in the same session.")
    session = WinrtPairingSession()
    loop = asyncio.get_event_loop()
    outcome = await winrt_oob_mitm_pair(address, session, _confirm_cb, loop)
    print(f"\nRESULT: {outcome}")
    if outcome != "paired":
        print(
            "\nIf busy_stale: watch BLE screen → Forget, then:\n"
            "  python reset_ble_pairing.py <MAC> --full-reset --restart-bthserv\n"
            "Reboot PC if still stuck. Test nRF Connect BLE app (not J-Link serial)."
        )
    return 0 if outcome == "paired" else 1


async def cmd_connect_pair(address: str) -> int:
    from bleak import BleakClient
    from winrt_pairing import WinrtPairingSession, bleak_connect_with_pairing_handler

    print(f"\n=== connect-pair: {address} (Custom pair_async after defer connect) ===")
    device = await _find_device(address)
    if device is None:
        print("FAIL: scan did not find device")
        return 1

    client = BleakClient(device)
    session = WinrtPairingSession()
    loop = asyncio.get_event_loop()

    async def attach(bleak_client) -> None:
        await session.attach(bleak_client, _confirm_cb, loop)

    await bleak_connect_with_pairing_handler(
        client, attach, timeout=20.0, defer_services=True
    )
    print(f"Connected: {client.is_connected}")

    outcome = await session.pair(client)
    print(f"\nRESULT: {outcome}")
    if outcome == "busy_stale":
        print(
            "\nconnect-pair opens GattSession and may wedge WinRT before pair_async.\n"
            "Try oob-pair FIRST (no connect): python pair_minimal_test.py <MAC> oob-pair"
        )

    session.detach()
    await client.disconnect()
    return 0 if outcome == "paired" else 1


async def cmd_default_pair(address: str) -> int:
    from bleak import BleakClient
    from winrt_pairing import WinrtPairingSession, bleak_connect_defer_only

    print(f"\n=== default-pair: {address} (OS PairAsync, no Custom handler) ===")
    print("Watch for Windows system pairing UI.")
    device = await _find_device(address)
    if device is None:
        print("FAIL: scan did not find device")
        return 1

    client = BleakClient(device)
    session = WinrtPairingSession()

    await bleak_connect_defer_only(client, timeout=20.0)
    print(f"Connected: {client.is_connected}")

    outcome = await session.pair_default_protection(client)
    print(f"\nRESULT: {outcome}")

    await client.disconnect()
    return 0 if outcome == "paired" else 1


async def cmd_default_oob(address: str) -> int:
    from winrt_pairing import winrt_default_oob_pair

    print(f"\n=== default-oob: {address} (OS PairAsync, no link, no Custom handler) ===")
    print("Watch for Windows system pairing UI.")
    outcome = await winrt_default_oob_pair(address)
    print(f"\nRESULT: {outcome}")
    return 0 if outcome == "paired" else 1


async def cmd_gatt_pair(address: str, *, pair_first: bool = True) -> int:
    from bleak import BleakClient
    from hcm_protocol import CHRC_WIFI_SSID
    from winrt_pairing import WinrtPairingSession, bleak_connect_with_pairing_handler, winrt_gatt_trigger_pair

    label = "pair-first + auth verify" if pair_first else "LEGACY: GATT before pair (wedges WinRT)"
    print(f"\n=== gatt-pair: {address} ({label}) ===")
    device = await _find_device(address)
    if device is None:
        print("FAIL: scan did not find device")
        return 1

    client = BleakClient(device)
    session = WinrtPairingSession()
    loop = asyncio.get_event_loop()

    async def attach(bleak_client) -> None:
        await session.attach(bleak_client, _confirm_cb, loop)

    await bleak_connect_with_pairing_handler(
        client, attach, timeout=20.0, defer_services=True
    )
    print(f"Connected: {client.is_connected}")

    outcome = await winrt_gatt_trigger_pair(
        client,
        session,
        address=address,
        trigger_uuid=CHRC_WIFI_SSID,
        timeout=90.0,
        pair_first=pair_first,
    )
    print(f"\nRESULT: {outcome}")

    session.detach()
    try:
        await client.disconnect()
    except Exception:
        pass

    if outcome == "paired":
        from winrt_pairing import is_paired_os
        if await is_paired_os(address):
            print("Verified: is_paired_os=True")
            return 0
        print("WARN: pair reported success but is_paired_os=False")
    return 0 if outcome == "paired" else 1


async def main() -> int:
    if sys.platform != "win32":
        log.error("Windows only")
        return 1

    parser = argparse.ArgumentParser(description="Minimal WinRT BLE pair repro")
    parser.add_argument("address", help="BLE MAC, e.g. D5:C9:63:90:78:40")
    parser.add_argument(
        "mode",
        nargs="?",
        default="scan",
        choices=(
            "scan",
            "oob-pair",
            "connect-pair",
            "default-pair",
            "default-oob",
            "gatt-pair",
            "gatt-pair-legacy",
        ),
        help="scan | gatt-pair | connect-pair | … (default: scan)",
    )
    parser.add_argument("--debug", action="store_true")
    args = parser.parse_args()

    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)

    handlers = {
        "scan": cmd_scan,
        "oob-pair": cmd_oob_pair,
        "connect-pair": cmd_connect_pair,
        "default-pair": cmd_default_pair,
        "default-oob": cmd_default_oob,
        "gatt-pair": lambda addr: cmd_gatt_pair(addr, pair_first=True),
        "gatt-pair-legacy": lambda addr: cmd_gatt_pair(addr, pair_first=False),
    }
    return await handlers[args.mode](args.address)


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
