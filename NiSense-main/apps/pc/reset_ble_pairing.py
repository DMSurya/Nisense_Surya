#!/usr/bin/env python3
"""Clear PC bond or fully reset Windows BLE pairing (CLI for Firmware page actions).

Usage::

    # Clear PC Bond (~2 s) — same as Firmware → Clear PC Bond
    python reset_ble_pairing.py D0:7D:94:C6:76:DC

    # Reset BT (~45 s) — same as Firmware → Reset BT
    python reset_ble_pairing.py D0:7D:94:C6:76:DC --full-reset

    # Also restart bthserv (requires Administrator PowerShell)
    python reset_ble_pairing.py D0:7D:94:C6:76:DC --full-reset --restart-bthserv
"""
from __future__ import annotations

import argparse
import asyncio
import logging
import sys

logging.basicConfig(level=logging.INFO, format="%(levelname)s %(message)s")
log = logging.getLogger(__name__)


async def main() -> int:
    parser = argparse.ArgumentParser(description="Clear WinRT BLE pairing state")
    parser.add_argument("address", help="BLE MAC address of the HCM device")
    parser.add_argument(
        "--full-reset",
        action="store_true",
        help="Unpair + bounce Bluetooth radio (slow; use when OPERATION_ALREADY_IN_PROGRESS)",
    )
    parser.add_argument(
        "--restart-bthserv",
        action="store_true",
        help="Also restart bthserv (requires Administrator)",
    )
    args = parser.parse_args()

    if sys.platform != "win32":
        log.error("This tool is only for Windows WinRT BLE")
        return 1

    if args.full_reset:
        from winrt_pairing import reset_bluetooth_stack
        ok = await reset_bluetooth_stack(
            args.address,
            bounce_radio=True,
            restart_service=args.restart_bthserv,
        )
    else:
        from winrt_pairing import forget_device_pairing
        ok = await forget_device_pairing(args.address)

    if ok:
        log.info("Done for %s", args.address)
    else:
        log.warning("No pairing identities found for %s", args.address)
    return 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
