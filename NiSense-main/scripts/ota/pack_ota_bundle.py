#!/usr/bin/env python3
"""Pack a nisense-ota-v1 ZIP bundle for mobile multi-image OTA.

Usage:
  python scripts/ota/pack_ota_bundle.py \\
    --version 1.2.3 \\
    --firmware build/.../zephyr.signed.bin \\
    --resource build/.../resource.bin \\
    --model model/glucose/out/glucose_model_wearable.bin \\
    --model-variant wearable \\
    --model-version 12 \\
    --model-signature sig.b64 \\
    --out dist/nisense_ota_1.2.3.zip
"""

from __future__ import annotations

import argparse
import hashlib
import json
import zipfile
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def add_component(components: list, kind: str, src: Path, arcname: str, **extra) -> None:
    if not src.is_file():
        raise FileNotFoundError(src)
    components.append(
        {
            "kind": kind,
            "file": arcname,
            "sha256": sha256_file(src),
            "size": src.stat().st_size,
            **extra,
        }
    )


def main() -> int:
    ap = argparse.ArgumentParser(description="Pack nisense-ota-v1 ZIP")
    ap.add_argument("--version", required=True)
    ap.add_argument("--firmware", type=Path, help="Signed app image (.bin)")
    ap.add_argument("--resource", type=Path, help="resource.bin")
    ap.add_argument("--model", type=Path, help="Packed glucose_model_*.bin")
    ap.add_argument("--model-variant", choices=("wearable", "pulse"), default="wearable")
    ap.add_argument("--model-version", type=int, default=0)
    ap.add_argument("--model-signature", type=Path, help="Optional Ed25519 sig file (raw or b64 text)")
    ap.add_argument("--fw-version-min", default=None, help="Manifest requires.fw_version_min")
    ap.add_argument("--resource-format", type=int, default=1)
    ap.add_argument("--model-format", type=int, default=1)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()

    if not any((args.firmware, args.resource, args.model)):
        ap.error("at least one of --firmware / --resource / --model is required")

    components: list[dict] = []
    files: list[tuple[Path, str]] = []

    if args.resource:
        add_component(components, "resource", args.resource, "resource.bin")
        files.append((args.resource, "resource.bin"))

    if args.model:
        model_name = f"glucose_model_{args.model_variant}.bin"
        sig_b64 = None
        if args.model_signature and args.model_signature.is_file():
            raw = args.model_signature.read_bytes().strip()
            try:
                # already base64 text
                sig_b64 = raw.decode("ascii")
            except UnicodeDecodeError:
                import base64

                sig_b64 = base64.b64encode(raw).decode("ascii")
        extra = {
            "variant": args.model_variant,
            "version": args.model_version,
        }
        if sig_b64:
            extra["signature"] = sig_b64
        add_component(components, "model", args.model, model_name, **extra)
        files.append((args.model, model_name))

    if args.firmware:
        add_component(components, "firmware", args.firmware, "zephyr.signed.bin")
        files.append((args.firmware, "zephyr.signed.bin"))

    provides = {
        "resource_format": args.resource_format,
        "model_format": args.model_format,
        "model_version": args.model_version,
        "model_variant": args.model_variant if args.model else None,
    }
    requires = {}
    if args.fw_version_min:
        requires["fw_version_min"] = args.fw_version_min
    if args.model:
        requires["model_variant"] = args.model_variant

    manifest = {
        "format": "nisense-ota-v1",
        "version": args.version,
        "requires": requires,
        "provides": {k: v for k, v in provides.items() if v is not None},
        "components": components,
        "transfer_order": ["resource", "model", "firmware"],
    }

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.out, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("manifest.json", json.dumps(manifest, indent=2) + "\n")
        for src, arc in files:
            zf.write(src, arcname=arc)

    print(f"Wrote {args.out} ({args.out.stat().st_size} bytes)")
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
