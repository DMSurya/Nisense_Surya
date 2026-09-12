"""Unit tests that don't require a database (security + signing)."""

from __future__ import annotations

import base64

import jwt
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

from app.config import settings
from app.security import _extract_roles, verify_token


def test_dev_token_roundtrip():
    settings.oidc_provider = "dev"
    token = jwt.encode(
        {
            "sub": "dev:tester",
            "preferred_username": "tester",
            "realm_access": {"roles": ["super_admin", "doctor", "bogus"]},
        },
        settings.dev_jwt_secret,
        algorithm="HS256",
    )
    p = verify_token(token)
    assert p.username == "tester"
    assert "super_admin" in p.roles
    assert "doctor" in p.roles
    assert "bogus" not in p.roles  # unknown roles filtered
    assert p.has_any(["patient"])  # super_admin implies all


def test_extract_roles_variants():
    assert set(_extract_roles({"roles": ["doctor"]})) == {"doctor"}
    assert set(_extract_roles({"cognito:groups": ["ai_analyst"]})) == {"ai_analyst"}


def test_model_signature_verify(monkeypatch, tmp_path):
    from app import signing

    priv = Ed25519PrivateKey.generate()
    pub = priv.public_key()
    from cryptography.hazmat.primitives import serialization

    pub_pem = pub.public_bytes(
        serialization.Encoding.PEM,
        serialization.PublicFormat.SubjectPublicKeyInfo,
    )
    keyfile = tmp_path / "pub.pem"
    keyfile.write_bytes(pub_pem)

    monkeypatch.setattr(settings, "model_signing_pubkey_path", str(keyfile))
    monkeypatch.setattr(settings, "require_signed_models", True)
    signing._load_pubkey.cache_clear()

    data = b"packed-model-image"
    sig = base64.b64encode(priv.sign(data)).decode()
    assert signing.verify_model_signature(data, sig) is True
    assert signing.verify_model_signature(data + b"x", sig) is False
