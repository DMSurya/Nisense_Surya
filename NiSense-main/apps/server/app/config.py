"""Application configuration (12-factor: all via environment).

Cloud-portable: the same settings drive the Raspberry Pi test box and an
AWS/Azure deployment. Adapters (DB, object store, identity) are selected by
these values so business logic never hard-codes an infrastructure provider.
"""

from __future__ import annotations

from functools import lru_cache
from typing import Literal

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_prefix="NISENSE_", env_file=".env", extra="ignore"
    )

    # --- General -----------------------------------------------------------
    env: Literal["dev", "prod"] = "dev"
    log_level: str = "INFO"
    api_prefix: str = "/api/v1"
    # Public base domain (DDNS -> static IP) used for OAuth redirects / CORS.
    public_base_url: str = "https://mpsaami.ddns.net"
    cors_origins: list[str] = Field(
        default_factory=lambda: [
            "https://mpsaami.ddns.net",
            "http://localhost:5173",
        ]
    )

    # --- Database (Postgres + TimescaleDB) ---------------------------------
    database_url: str = "postgresql+psycopg2://nisense:nisense@db:5432/nisense"

    # --- Identity / OIDC ---------------------------------------------------
    # provider: "keycloak" (self-hosted), "cognito" or "azuread" (managed), or
    # "dev" (local HS256 tokens, no external IdP) for the Pi test box.
    oidc_provider: Literal["keycloak", "cognito", "azuread", "dev"] = "dev"
    # Public issuer URL (matches Keycloak KC_HTTP_RELATIVE_PATH=/auth).
    oidc_issuer: str = "https://mpsaami.ddns.net/auth/realms/nisense"
    # Keycloak access-token aud is typically "account"; audience verify is off.
    oidc_audience: str = "account"
    oidc_jwks_url: str = ""  # derived from issuer when empty
    # Dev-mode shared secret for locally-minted HS256 tokens (dev provider only)
    dev_jwt_secret: str = "change-me-dev-secret-please-set-a-long-value"

    # --- Object / artifact storage ----------------------------------------
    # backend: "local" (Pi disk) or "s3" (AWS S3 / Azure Blob via S3 API).
    storage_backend: Literal["local", "s3"] = "local"
    storage_local_dir: str = "/data/artifacts"
    s3_bucket: str = ""
    s3_endpoint_url: str = ""  # set for Azure Blob / MinIO; empty = AWS S3
    s3_region: str = "us-east-1"

    # --- Model signing -----------------------------------------------------
    # Path to the Ed25519 public key (PEM) used to verify uploaded model
    # signatures server-side. Signing itself happens via the nisense-sign
    # command (private key stays on the Pi).
    model_signing_pubkey_path: str = "/data/keys/model_ed25519_pub.pem"
    require_signed_models: bool = False

    # --- Provisioning ------------------------------------------------------
    provisioning_token_ttl_hours: int = 168  # 7 days


@lru_cache
def get_settings() -> Settings:
    return Settings()


settings = get_settings()
