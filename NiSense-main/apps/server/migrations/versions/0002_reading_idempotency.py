"""reading idempotency table + drop obsolete readings unique

Revision ID: 0002_reading_idempotency
Revises: 0001_initial
Create Date: 2026-08-02

Timescale hypertables require unique constraints to include the partition
column (``ts``). Logical ingest idempotency is therefore a companion table
on ``(device_id, type, record_id)`` rather than a UNIQUE on ``readings``.
"""

from __future__ import annotations

import sqlalchemy as sa
from alembic import op
from sqlalchemy import inspect, text

revision = "0002_reading_idempotency"
down_revision = "0001_initial"
branch_labels = None
depends_on = None


def upgrade() -> None:
    bind = op.get_bind()
    insp = inspect(bind)
    tables = set(insp.get_table_names())

    if "reading_idempotency" not in tables:
        op.create_table(
            "reading_idempotency",
            sa.Column("device_id", sa.String(length=64), nullable=False),
            sa.Column("type", sa.String(length=16), nullable=False),
            sa.Column("record_id", sa.Integer(), nullable=False),
            sa.PrimaryKeyConstraint("device_id", "type", "record_id"),
        )

    if "readings" in tables:
        for uc in insp.get_unique_constraints("readings"):
            if uc.get("name") == "uq_reading":
                op.drop_constraint("uq_reading", "readings", type_="unique")
                break

        # Backfill keys for existing rows (keep one logical key per triple).
        bind.execute(
            text(
                """
                INSERT INTO reading_idempotency (device_id, type, record_id)
                SELECT DISTINCT device_id, type, record_id
                FROM readings
                WHERE record_id IS NOT NULL
                ON CONFLICT DO NOTHING
                """
            )
        )

    if "device_version_state" in tables:
        cols = {c["name"] for c in insp.get_columns("device_version_state")}
        if "target_version" not in cols:
            op.add_column(
                "device_version_state",
                sa.Column("target_version", sa.String(length=64), nullable=True),
            )


def downgrade() -> None:
    bind = op.get_bind()
    insp = inspect(bind)
    tables = set(insp.get_table_names())

    if "device_version_state" in tables:
        cols = {c["name"] for c in insp.get_columns("device_version_state")}
        if "target_version" in cols:
            op.drop_column("device_version_state", "target_version")

    if "reading_idempotency" in tables:
        op.drop_table("reading_idempotency")

    if "readings" in tables:
        names = {uc.get("name") for uc in insp.get_unique_constraints("readings")}
        if "uq_reading" not in names:
            op.create_unique_constraint(
                "uq_reading",
                "readings",
                ["device_id", "type", "record_id", "ts"],
            )
