"""Auth endpoints.

`/auth/dev-token` mints a local HS256 token for the Pi test box (dev provider
only) so the web/mobile clients and API can be exercised without a running IdP.
In prod, tokens come from Keycloak/Cognito/Azure AD B2C and this endpoint is
disabled.

`/auth/me` returns the caller's identity + roles from the validated token.
"""

from __future__ import annotations

import time

import jwt
from fastapi import APIRouter, Depends, HTTPException, status

from ..config import settings
from ..security import ALL_ROLES, Principal, get_current_principal
from ..schemas import DevTokenRequest, MeResponse, TokenResponse

router = APIRouter(prefix="/auth", tags=["auth"])


@router.post("/dev-token", response_model=TokenResponse)
def dev_token(req: DevTokenRequest) -> TokenResponse:
    if settings.oidc_provider != "dev":
        raise HTTPException(
            status.HTTP_403_FORBIDDEN,
            "dev-token is only available with the 'dev' OIDC provider",
        )
    roles = [r for r in req.roles if r in ALL_ROLES] or ["super_admin"]
    now = int(time.time())
    claims = {
        "sub": f"dev:{req.username}",
        "preferred_username": req.username,
        "email": f"{req.username}@example.local",
        "aud": settings.oidc_audience,
        "iss": "nisense-dev",
        "iat": now,
        "exp": now + req.ttl_seconds,
        "realm_access": {"roles": roles},
    }
    token = jwt.encode(claims, settings.dev_jwt_secret, algorithm="HS256")
    return TokenResponse(access_token=token, expires_in=req.ttl_seconds)


@router.get("/me", response_model=MeResponse)
def me(principal: Principal = Depends(get_current_principal)) -> MeResponse:
    return MeResponse(
        subject=principal.subject,
        username=principal.username,
        email=principal.email,
        roles=principal.roles,
    )
