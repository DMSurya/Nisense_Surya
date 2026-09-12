"""initial schema + TimescaleDB hypertable

Revision ID: 0001_initial
Revises:
Create Date: 2026-07-20
"""

from __future__ import annotations

from alembic import op
from sqlalchemy import text

from app.db import Base
from app import models  # noqa: F401 - registers tables on Base.metadata

revision = "0001_initial"
down_revision = None
branch_labels = None
depends_on = None


def upgrade() -> None:
    bind = op.get_bind()
    Base.metadata.create_all(bind=bind)

    # Timescale hypertable is optional — use a SAVEPOINT so a failure does not
    # abort the outer Alembic transaction (create_extension can fail if the
    # image lacks privileges / the extension is already half-applied).
    try:
        nested = bind.begin_nested()
        bind.execute(text("CREATE EXTENSION IF NOT EXISTS timescaledb"))
        bind.execute(
            text(
                "SELECT create_hypertable('readings', 'ts', "
                "if_not_exists => TRUE, migrate_data => TRUE)"
            )
        )
        nested.commit()
    except Exception:  # noqa: BLE001
        try:
            nested.rollback()
        except Exception:  # noqa: BLE001
            pass


def downgrade() -> None:
    Base.metadata.drop_all(bind=op.get_bind())
