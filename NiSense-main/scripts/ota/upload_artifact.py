#!/usr/bin/env python3
"""Upload a nisense-ota-v1 ZIP (or firmware/model bin) to the platform API.

Examples:
  # Dev token + bundle
  python scripts/ota/upload_artifact.py \\
    --base-url http://localhost:8000/api/v1 \\
    --dev-token --roles device_engineer \\
    --kind bundle --version 1.2.3 \\
    --file dist/nisense_ota_1.2.3.zip

  # Existing bearer token
  python scripts/ota/upload_artifact.py \\
    --base-url http://localhost:8000/api/v1 \\
    --token "$TOKEN" \\
    --kind firmware --version 1.2.3 \\
    --file build_sdk_v330/NiSense/zephyr/zephyr.signed.bin
"""

from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.request
from pathlib import Path


def _multipart(fields: dict[str, str], file_field: str, filename: str, data: bytes) -> tuple[bytes, str]:
    boundary = "----NiSenseBoundary7MA4YWxkTrZu0gW"
    parts: list[bytes] = []
    for name, value in fields.items():
        parts.append(
            f"--{boundary}\r\n"
            f'Content-Disposition: form-data; name="{name}"\r\n\r\n'
            f"{value}\r\n".encode()
        )
    parts.append(
        (
            f"--{boundary}\r\n"
            f'Content-Disposition: form-data; name="{file_field}"; filename="{filename}"\r\n'
            f"Content-Type: application/octet-stream\r\n\r\n"
        ).encode()
        + data
        + b"\r\n"
    )
    parts.append(f"--{boundary}--\r\n".encode())
    body = b"".join(parts)
    return body, f"multipart/form-data; boundary={boundary}"


def mint_dev_token(base: str, username: str, roles: list[str]) -> str:
    url = f"{base.rstrip('/')}/auth/dev-token"
    payload = json.dumps({"username": username, "roles": roles}).encode()
    req = urllib.request.Request(
        url,
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req) as resp:
        out = json.loads(resp.read().decode())
    token = out.get("access_token") or out.get("token")
    if not token:
        raise SystemExit(f"dev-token response missing token: {out}")
    return token


def main() -> int:
    ap = argparse.ArgumentParser(description="Upload OTA artifact to NiSense API")
    ap.add_argument("--base-url", default="http://localhost:8000/api/v1")
    ap.add_argument("--token", help="Bearer token (skip with --dev-token)")
    ap.add_argument("--dev-token", action="store_true", help="Mint a local HS256 token")
    ap.add_argument("--username", default="admin")
    ap.add_argument("--roles", default="device_engineer",
                    help="Comma-separated roles for --dev-token")
    ap.add_argument("--kind", required=True, choices=("firmware", "model", "bundle"))
    ap.add_argument("--version", required=True)
    ap.add_argument("--variant", default="")
    ap.add_argument("--notes", default="")
    ap.add_argument("--signature-b64", default="", help="Ed25519 signature for kind=model")
    ap.add_argument("--file", type=Path, required=True)
    args = ap.parse_args()

    if not args.file.is_file():
        raise SystemExit(f"file not found: {args.file}")

    token = args.token
    if args.dev_token:
        token = mint_dev_token(args.base_url, args.username, [r.strip() for r in args.roles.split(",") if r.strip()])
    if not token:
        raise SystemExit("provide --token or --dev-token")

    fields = {
        "kind": args.kind,
        "version": args.version,
    }
    if args.variant:
        fields["variant"] = args.variant
    if args.notes:
        fields["notes"] = args.notes
    if args.signature_b64:
        fields["signature_b64"] = args.signature_b64

    body, ctype = _multipart(fields, "file", args.file.name, args.file.read_bytes())
    url = f"{args.base_url.rstrip('/')}/artifacts"
    req = urllib.request.Request(
        url,
        data=body,
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": ctype,
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(req) as resp:
            out = json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        err = e.read().decode(errors="replace")
        print(f"HTTP {e.code}: {err}", file=sys.stderr)
        return 1

    print(json.dumps(out, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
