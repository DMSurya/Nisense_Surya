"""Authentication & RBAC.

OIDC provider is pluggable (Keycloak self-hosted, or managed Cognito/Azure AD
B2C, or a local `dev` HS256 issuer for the Pi test box) so business logic never
depends on a specific identity provider. Bearer tokens are validated and the
caller's roles are extracted for role-based access control.

Customer roles (from the spec):
  super_admin, clinical_trial_admin, doctor, investigator, patient,
  device_engineer, ai_analyst
"""

from __future__ import annotations

import time
from dataclasses import dataclass, field
from functools import lru_cache

import httpx
import jwt
from fastapi import Depends, HTTPException, status
from fastapi.security import HTTPAuthorizationCredentials, HTTPBearer

from .config import settings

# --- Roles -----------------------------------------------------------------

ROLE_SUPER_ADMIN = "super_admin"
ROLE_CT_ADMIN = "clinical_trial_admin"
ROLE_DOCTOR = "doctor"
ROLE_INVESTIGATOR = "investigator"
ROLE_PATIENT = "patient"
ROLE_DEVICE_ENGINEER = "device_engineer"
ROLE_AI_ANALYST = "ai_analyst"

ALL_ROLES = [
    ROLE_SUPER_ADMIN,
    ROLE_CT_ADMIN,
    ROLE_DOCTOR,
    ROLE_INVESTIGATOR,
    ROLE_PATIENT,
    ROLE_DEVICE_ENGINEER,
    ROLE_AI_ANALYST,
]


@dataclass
class Principal:
    subject: str
    username: str
    email: str | None = None
    roles: list[str] = field(default_factory=list)

    def has_any(self, roles: list[str]) -> bool:
        return ROLE_SUPER_ADMIN in self.roles or any(r in self.roles for r in roles)


bearer_scheme = HTTPBearer(auto_error=True)


# --- JWKS cache (for RS256 IdPs) ------------------------------------------


class _JwksCache:
    def __init__(self) -> None:
        self._keys: dict[str, object] = {}
        self._fetched_at = 0.0
        self._ttl = 3600.0

    def _jwks_url(self) -> str:
        if settings.oidc_jwks_url:
            return settings.oidc_jwks_url
        return f"{settings.oidc_issuer.rstrip('/')}/protocol/openid-connect/certs"

    def get_key(self, kid: str):
        if kid in self._keys and (time.time() - self._fetched_at) < self._ttl:
            return self._keys[kid]
        self._refresh()
        return self._keys.get(kid)

    def _refresh(self) -> None:
        url = self._jwks_url()
        resp = httpx.get(url, timeout=10.0)
        resp.raise_for_status()
        jwks = resp.json()
        keys = {}
        for jwk in jwks.get("keys", []):
            kid = jwk.get("kid")
            if not kid:
                continue
            keys[kid] = jwt.algorithms.RSAAlgorithm.from_jwk(jwk)
        self._keys = keys
        self._fetched_at = time.time()


@lru_cache
def _jwks() -> _JwksCache:
    return _JwksCache()


# --- Token verification ----------------------------------------------------


def _extract_roles(claims: dict) -> list[str]:
    """Roles may live in realm_access.roles (Keycloak), 'roles', or 'cognito:groups'."""
    roles: list[str] = []
    ra = claims.get("realm_access") or {}
    roles += ra.get("roles", []) if isinstance(ra, dict) else []
    roles += claims.get("roles", []) if isinstance(claims.get("roles"), list) else []
    roles += claims.get("cognito:groups", []) if isinstance(
        claims.get("cognito:groups"), list
    ) else []
    # Normalize + keep only known roles.
    return [r for r in {r.lower() for r in roles} if r in ALL_ROLES]


def verify_token(token: str) -> Principal:
    provider = settings.oidc_provider
    try:
        if provider == "dev":
            claims = jwt.decode(
                token,
                settings.dev_jwt_secret,
                algorithms=["HS256"],
                audience=settings.oidc_audience,
                options={"verify_aud": False},
            )
        else:
            header = jwt.get_unverified_header(token)
            key = _jwks().get_key(header.get("kid", ""))
            if key is None:
                raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Unknown signing key")
            claims = jwt.decode(
                token,
                key=key,
                algorithms=["RS256"],
                audience=settings.oidc_audience,
                issuer=settings.oidc_issuer,
                options={"verify_aud": False},
            )
    except HTTPException:
        raise
    except jwt.PyJWTError as exc:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, f"Invalid token: {exc}") from exc

    subject = claims.get("sub") or claims.get("username") or "unknown"
    username = (
        claims.get("preferred_username")
        or claims.get("username")
        or claims.get("email")
        or subject
    )
    return Principal(
        subject=subject,
        username=username,
        email=claims.get("email"),
        roles=_extract_roles(claims),
    )


# --- FastAPI dependencies --------------------------------------------------


def get_current_principal(
    creds: HTTPAuthorizationCredentials = Depends(bearer_scheme),
) -> Principal:
    return verify_token(creds.credentials)


def require_roles(*roles: str):
    """Dependency factory enforcing that the caller has one of `roles`."""

    def _dep(principal: Principal = Depends(get_current_principal)) -> Principal:
        if not principal.has_any(list(roles)):
            raise HTTPException(
                status.HTTP_403_FORBIDDEN,
                f"Requires one of roles: {', '.join(roles)}",
            )
        return principal

    return _dep
