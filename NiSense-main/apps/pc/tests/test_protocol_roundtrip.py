#!/usr/bin/env python3
"""Verify hcm_protocol struct sizes match firmware ble_gatt.h layouts."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from hcm_protocol import (
    PROTOCOL_VERSION_MAJOR,
    PROTOCOL_VERSION_MINOR,
    _PMIC,
    _TEMP,
    _VITALS,
    _GLUCOSE,
    _PROXIMITY,
    _SENSOR_ALL_SIZE,
    decode_sensor_all,
)

# Expected sizes from firmware __packed structs (protocol v2).
EXPECTED = {
    "pmic": 10,
    "temp": 6,
    "vitals": 18,
    "glucose": 7,
    "proximity": 10,
    "sensor_all": 51,
}


def main() -> int:
    assert PROTOCOL_VERSION_MAJOR == 2
    assert _PMIC.size == EXPECTED["pmic"]
    assert _TEMP.size == EXPECTED["temp"]
    assert _VITALS.size == EXPECTED["vitals"]
    assert _GLUCOSE.size == EXPECTED["glucose"]
    assert _PROXIMITY.size == EXPECTED["proximity"]
    assert _SENSOR_ALL_SIZE == EXPECTED["sensor_all"]

    # Round-trip a zero-filled aggregate packet.
    payload = bytes(_SENSOR_ALL_SIZE)
    parsed = decode_sensor_all(payload)
    assert parsed is not None
    assert parsed.pmic.battery_mv == 0
    assert parsed.vitals.hr_bpm == 0
    assert parsed.proximity.wear_state == 0

    print(f"Protocol v{PROTOCOL_VERSION_MAJOR}.{PROTOCOL_VERSION_MINOR} struct sizes OK")
    print(f"  sensor_all = {_SENSOR_ALL_SIZE} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
