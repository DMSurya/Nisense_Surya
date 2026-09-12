"""Ed25519 signature verification for glucose-model artifacts.

The private signing key lives ONLY on the Pi and is used by the `nisense-sign`
command (scripts/nisense-sign) invoked over SSH. The server verifies uploaded
signatures against the public key so it can reject tampered model images before
they are distributed to devices. Devices independently verify again on commit.
"""

from __future__ import annotations

import base64
from functools import lru_cache
from pathlib import Path

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
from cryptography.hazmat.primitives.serialization import load_pem_public_key

from .config import settings


@lru_cache
def _load_pubkey() -> Ed25519PublicKey | None:
    path = Path(settings.model_signing_pubkey_path)
    if not path.exists():
        return None
    key = load_pem_public_key(path.read_bytes())
    if isinstance(key, Ed25519PublicKey):
        return key
    return None


def verify_model_signature(data: bytes, signature_b64: str | None) -> bool:
    """Verify an Ed25519 signature (base64) over `data`.

    Returns True when the signature is valid. When signing is not required and
    no signature/public key is available, returns True (trust) so a Pi test box
    works before keys are provisioned.
    """
    if not settings.require_signed_models and not signature_b64:
        return True

    pubkey = _load_pubkey()
    if pubkey is None:
        # No key configured: only acceptable when signing is not enforced.
        return not settings.require_signed_models

    if not signature_b64:
        return False

    try:
        pubkey.verify(base64.b64decode(signature_b64), data)
        return True
    except (InvalidSignature, ValueError):
        return False
