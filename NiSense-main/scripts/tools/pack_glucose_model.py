#!/usr/bin/env python3
"""Pack glucose elimination-matrix CSVs into a memory-mapped XIP binary.

The output binary mirrors the on-device layout described in
``include/glucose_model_xip.h``:

    [ glucose_model_header (128 B) ]
    [ glucose_model_group_entry * num_groups (16 B each) ]
    [ float32[row_count][3] per group ... ]

CRC32 is IEEE 802.3 (== zlib.crc32 == Zephyr crc32_ieee) so the device and the
packer agree. The blob is flashed into the primary glucose-model slot at
manufacturing (like resource) and can also be shipped over BLE for A/B update.

Usage:
    python pack_glucose_model.py --variant wearable \\
        --model-dir model/glucose --output build/glucose_model_wearable.bin \\
        --version 1 --build-id "v3.3.0"
"""

from __future__ import annotations

import argparse
import struct
import sys
import time
import zlib
from pathlib import Path

# --- Must match include/glucose_model_xip.h --------------------------------
MAGIC = 0x4C444D47  # "GMDL"
FORMAT_VERSION = 1
GROUP_COL = 3
PARTITION_SIZE = 0x80000  # 512 KB per model A/B slot
HEADER_SIZE = 128
ENTRY_SIZE = 16

VARIANTS = {
    "wearable": 0,
    "pulse": 1,
}

HEADER_FMT = "<IHHHHIIII32s68s"
ENTRY_FMT = "<HHIII"

assert struct.calcsize(HEADER_FMT) == HEADER_SIZE, struct.calcsize(HEADER_FMT)
assert struct.calcsize(ENTRY_FMT) == ENTRY_SIZE, struct.calcsize(ENTRY_FMT)


def _clean_field(value: str) -> str:
    """Strip whitespace, tabs and surrounding double-quotes from a CSV field."""
    return value.strip().strip('"').strip()


def _parse_csv(path: Path) -> list[tuple[float, float, float]]:
    rows: list[tuple[float, float, float]] = []
    with path.open("r", encoding="utf-8") as fh:
        for raw in fh:
            line = raw.strip()
            if not line:
                continue
            parts = [_clean_field(p) for p in line.split(",")]
            if len(parts) < GROUP_COL:
                continue
            try:
                lo, hi, corr = (float(parts[0]), float(parts[1]), float(parts[2]))
            except ValueError:
                # Tolerate header / non-numeric lines while still in the header
                # region; once real data has been parsed, a bad line is an error.
                if not rows:
                    continue
                raise SystemExit(f"{path}: malformed numeric row: {line!r}")
            rows.append((lo, hi, corr))
    return rows


def _discover_groups(variant_dir: Path) -> list[Path]:
    groups: dict[int, Path] = {}
    for csv in variant_dir.glob("group_*.csv"):
        stem = csv.stem  # group_N
        try:
            num = int(stem.split("_", 1)[1])
        except (IndexError, ValueError):
            continue
        groups[num] = csv
    return [groups[n] for n in sorted(groups)]


def pack(variant: str, model_dir: Path, version: int, build_id: str) -> bytes:
    if variant not in VARIANTS:
        raise SystemExit(f"unknown variant '{variant}' (expected {list(VARIANTS)})")

    variant_dir = model_dir / variant
    if not variant_dir.is_dir():
        raise SystemExit(f"model dir not found: {variant_dir}")

    csv_paths = _discover_groups(variant_dir)
    if not csv_paths:
        raise SystemExit(f"no group_*.csv files found in {variant_dir}")

    num_groups = len(csv_paths)
    entries: list[bytes] = []
    data_blobs: list[bytes] = []
    data_offset = HEADER_SIZE + num_groups * ENTRY_SIZE

    for idx, csv_path in enumerate(csv_paths):
        group_num = int(csv_path.stem.split("_", 1)[1])
        rows = _parse_csv(csv_path)
        blob = b"".join(struct.pack("<fff", *row) for row in rows)
        crc = zlib.crc32(blob) & 0xFFFFFFFF
        entries.append(
            struct.pack(ENTRY_FMT, group_num, len(rows), data_offset, crc, 0)
        )
        data_blobs.append(blob)
        print(
            f"  group {group_num:>2}: {len(rows):>5} rows, "
            f"{len(blob):>7} bytes @ 0x{data_offset:06X}  crc=0x{crc:08X}"
        )
        data_offset += len(blob)

    entries_bin = b"".join(entries)
    data_bin = b"".join(data_blobs)
    payload = entries_bin + data_bin
    payload_crc = zlib.crc32(payload) & 0xFFFFFFFF
    total_size = HEADER_SIZE + len(payload)

    if total_size > PARTITION_SIZE:
        raise SystemExit(
            f"packed model {total_size} bytes exceeds slot size {PARTITION_SIZE}"
        )

    header = struct.pack(
        HEADER_FMT,
        MAGIC,
        FORMAT_VERSION,
        VARIANTS[variant],
        num_groups,
        GROUP_COL,
        total_size,
        payload_crc,
        int(time.time()),
        version & 0xFFFFFFFF,
        build_id.encode("utf-8")[:31].ljust(32, b"\0"),
        b"\0" * 68,
    )
    return header + payload




# --- Model pack (wearable + pulse GMDL entries) ---------------------------
PACK_MAGIC = 0x504C444D  # "MDLP"
PACK_FORMAT_VERSION = 1
PACK_HEADER_SIZE = 128
PACK_ENTRY_SIZE = 16
PACK_HEADER_FMT = "<IHHIIII32s72s"
PACK_ENTRY_FMT = "<HHIII"
assert struct.calcsize(PACK_HEADER_FMT) == PACK_HEADER_SIZE
assert struct.calcsize(PACK_ENTRY_FMT) == PACK_ENTRY_SIZE

ENTRY_TYPE = {"wearable": 0, "pulse": 1}


def pack_all(model_dir: Path, version: int, build_id: str) -> bytes:
    """Build one model-pack containing wearable + pulse GMDL blobs."""
    gmdls = []
    for name in ("wearable", "pulse"):
        blob = pack(name, model_dir, version, build_id)
        gmdls.append((ENTRY_TYPE[name], blob))

    num = len(gmdls)
    data_offset = PACK_HEADER_SIZE + num * PACK_ENTRY_SIZE
    entries = []
    blobs = []
    for typ, blob in gmdls:
        crc = zlib.crc32(blob) & 0xFFFFFFFF
        entries.append(struct.pack(PACK_ENTRY_FMT, typ, 0, data_offset, len(blob), crc))
        print(f"  pack entry type={typ}: {len(blob)} bytes @ 0x{data_offset:06X}")
        blobs.append(blob)
        data_offset += len(blob)

    entries_bin = b"".join(entries)
    data_bin = b"".join(blobs)
    payload = entries_bin + data_bin
    payload_crc = zlib.crc32(payload) & 0xFFFFFFFF
    total_size = PACK_HEADER_SIZE + len(payload)
    if total_size > PARTITION_SIZE:
        raise SystemExit(f"pack {total_size} exceeds slot {PARTITION_SIZE}")

    header = struct.pack(
        PACK_HEADER_FMT,
        PACK_MAGIC,
        PACK_FORMAT_VERSION,
        num,
        total_size,
        payload_crc,
        int(time.time()),
        version & 0xFFFFFFFF,
        build_id.encode("utf-8")[:31].ljust(32, b"\0"),
        b"\0" * 72,
    )
    return header + payload


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--variant", choices=sorted(VARIANTS),
                    help="Single GMDL variant (omit with --pack-all)")
    ap.add_argument("--pack-all", action="store_true",
                    help="Build model pack with wearable+pulse entries")
    ap.add_argument("--model-dir", default="model/glucose", type=Path)
    ap.add_argument("--output", required=True, type=Path)
    ap.add_argument("--version", type=int, default=1)
    ap.add_argument("--build-id", default="")
    args = ap.parse_args(argv)

    if args.pack_all:
        print(f"Packing model pack (wearable+pulse) dir={args.model_dir}")
        blob = pack_all(args.model_dir, args.version, args.build_id)
    else:
        if not args.variant:
            raise SystemExit("--variant is required unless --pack-all")
        print(f"Packing glucose model: variant={args.variant} dir={args.model_dir}")
        blob = pack(args.variant, args.model_dir, args.version, args.build_id)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(blob)
    print(
        f"Wrote {args.output} ({len(blob)} bytes, "
        f"{len(blob) / 1024:.1f} KB / {PARTITION_SIZE // 1024} KB slot)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
