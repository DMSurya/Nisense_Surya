"""nisense-ota-v1 ZIP validation (no DB / FastAPI deps)."""

from __future__ import annotations

import io
import json
import zipfile

from .signing import verify_model_signature
from .storage import sha256_hex


class BundleValidationError(ValueError):
    """Raised when a bundle ZIP fails structural or integrity checks."""


def verify_bundle_zip(data: bytes) -> None:
    """Validate nisense-ota-v1 ZIP: manifest + per-component sha256 (+ model sig).

    Raises BundleValidationError on failure.
    """
    try:
        zf = zipfile.ZipFile(io.BytesIO(data))
    except zipfile.BadZipFile as exc:
        raise BundleValidationError(f"invalid zip: {exc}") from exc

    names = {n.split("/")[-1]: n for n in zf.namelist() if not n.endswith("/")}
    if "manifest.json" not in names:
        raise BundleValidationError("bundle missing manifest.json")

    try:
        manifest = json.loads(zf.read(names["manifest.json"]))
    except json.JSONDecodeError as exc:
        raise BundleValidationError(f"bad manifest: {exc}") from exc

    if manifest.get("format") != "nisense-ota-v1":
        raise BundleValidationError("manifest format must be nisense-ota-v1")

    components = manifest.get("components")
    if not isinstance(components, list) or not components:
        raise BundleValidationError("manifest has no components")

    for comp in components:
        if not isinstance(comp, dict):
            raise BundleValidationError("bad component entry")
        fname = str(comp.get("file", "")).split("/")[-1]
        want = str(comp.get("sha256", "")).lower()
        kind = comp.get("kind")
        if not fname or not want or not kind:
            raise BundleValidationError("component needs kind/file/sha256")
        if fname not in names:
            raise BundleValidationError(f"zip missing {fname}")
        blob = zf.read(names[fname])
        got = sha256_hex(blob).lower()
        if got != want:
            raise BundleValidationError(
                f"sha256 mismatch for {fname}: got {got} want {want}"
            )
        if kind == "model":
            sig = comp.get("signature")
            if isinstance(sig, str) and sig:
                if not verify_model_signature(blob, sig):
                    raise BundleValidationError(
                        f"model signature verification failed for {fname}"
                    )
            elif not verify_model_signature(blob, None):
                raise BundleValidationError(
                    f"model component {fname} requires a valid signature"
                )
