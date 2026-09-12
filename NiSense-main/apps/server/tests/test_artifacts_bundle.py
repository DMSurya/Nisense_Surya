"""Tests for artifact upload validation (bundle ZIP + SHA)."""

from __future__ import annotations

import hashlib
import io
import json
import zipfile

import pytest

from app.artifact_bundle import BundleValidationError, verify_bundle_zip
from app.config import settings


def _sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _make_bundle(components: list[dict], blobs: dict[str, bytes]) -> bytes:
    """Build an in-memory nisense-ota-v1 ZIP."""
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as zf:
        manifest = {
            "format": "nisense-ota-v1",
            "version": "0.0.1",
            "components": components,
            "transfer_order": [c["kind"] for c in components],
        }
        zf.writestr("manifest.json", json.dumps(manifest))
        for name, data in blobs.items():
            zf.writestr(name, data)
    return buf.getvalue()


def test_verify_bundle_ok(monkeypatch):
    monkeypatch.setattr(settings, "require_signed_models", False)
    resource = b"resource-bytes"
    fw = b"firmware-bytes"
    data = _make_bundle(
        [
            {"kind": "resource", "file": "resource.bin", "sha256": _sha(resource), "size": len(resource)},
            {"kind": "firmware", "file": "zephyr.signed.bin", "sha256": _sha(fw), "size": len(fw)},
        ],
        {"resource.bin": resource, "zephyr.signed.bin": fw},
    )
    verify_bundle_zip(data)


def test_verify_bundle_bad_sha(monkeypatch):
    monkeypatch.setattr(settings, "require_signed_models", False)
    resource = b"resource-bytes"
    data = _make_bundle(
        [
            {
                "kind": "resource",
                "file": "resource.bin",
                "sha256": "0" * 64,
                "size": len(resource),
            },
        ],
        {"resource.bin": resource},
    )
    with pytest.raises(BundleValidationError, match="sha256 mismatch"):
        verify_bundle_zip(data)


def test_verify_bundle_missing_manifest():
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as zf:
        zf.writestr("resource.bin", b"x")
    with pytest.raises(BundleValidationError, match="manifest"):
        verify_bundle_zip(buf.getvalue())


def test_verify_bundle_bad_format():
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as zf:
        zf.writestr("manifest.json", json.dumps({"format": "other", "components": []}))
    with pytest.raises(BundleValidationError, match="nisense-ota-v1"):
        verify_bundle_zip(buf.getvalue())


def test_verify_bundle_model_requires_sig_when_enforced(monkeypatch):
    monkeypatch.setattr(settings, "require_signed_models", True)
    model = b"gm-model"
    data = _make_bundle(
        [
            {
                "kind": "model",
                "file": "glucose_model_wearable.bin",
                "sha256": _sha(model),
                "size": len(model),
                "variant": "wearable",
            },
        ],
        {"glucose_model_wearable.bin": model},
    )
    with pytest.raises(BundleValidationError, match="requires a valid signature"):
        verify_bundle_zip(data)
