"""FastAPI application entrypoint for the NiSense platform API."""

from __future__ import annotations

import logging

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from sqlalchemy import text

from . import __version__
from .config import settings
from .db import Base, engine
from .routers import (
    artifacts,
    auth,
    devices,
    export,
    health,
    ingest,
    patients,
    sensors,
    users,
)

logging.basicConfig(level=settings.log_level)
log = logging.getLogger("nisense")


def _init_db_dev() -> None:
    """Dev convenience: create tables + TimescaleDB hypertable + seed.

    In prod, schema is managed by Alembic (`dev.sh migrate`) and this is skipped.
    """
    Base.metadata.create_all(bind=engine)
    with engine.begin() as conn:
        try:
            conn.execute(text("CREATE EXTENSION IF NOT EXISTS timescaledb"))
            conn.execute(
                text(
                    "SELECT create_hypertable('readings', 'ts', "
                    "if_not_exists => TRUE, migrate_data => TRUE)"
                )
            )
            log.info("TimescaleDB hypertable ready on readings.ts")
        except Exception as exc:  # noqa: BLE001 - Timescale optional in some envs
            log.warning("TimescaleDB hypertable not created (%s)", exc)

    from .seed import seed_defaults

    seed_defaults()


def create_app() -> FastAPI:
    app = FastAPI(
        title="NiSense Digital Health Ecosystem API",
        version=__version__,
        description="Device provisioning, measurement ingestion, and "
        "firmware/model distribution for the NiSense platform.",
    )

    app.add_middleware(
        CORSMiddleware,
        allow_origins=settings.cors_origins,
        allow_credentials=True,
        allow_methods=["*"],
        allow_headers=["*"],
    )

    # Health at root (probes) and under the API prefix.
    app.include_router(health.router)

    p = settings.api_prefix
    app.include_router(health.router, prefix=p)
    app.include_router(auth.router, prefix=p)
    app.include_router(users.router, prefix=p)
    app.include_router(devices.router, prefix=p)
    app.include_router(sensors.router, prefix=p)
    app.include_router(patients.router, prefix=p)
    app.include_router(ingest.router, prefix=p)
    app.include_router(export.router, prefix=p)
    app.include_router(artifacts.router, prefix=p)

    @app.on_event("startup")
    def _startup() -> None:
        log.info("NiSense API %s starting (env=%s, oidc=%s, storage=%s)",
                 __version__, settings.env, settings.oidc_provider,
                 settings.storage_backend)
        if settings.env == "dev":
            _init_db_dev()

    return app


app = create_app()
