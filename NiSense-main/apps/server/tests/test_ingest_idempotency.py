"""Ingest idempotency on (device_id, type, record_id) without a DB server.

Uses an in-memory SQLite engine + the same claim helper as the router.
"""

from __future__ import annotations

import datetime as dt

import pytest
from sqlalchemy import create_engine, func, select
from sqlalchemy.orm import Session, sessionmaker
from sqlalchemy.pool import StaticPool

from app.db import Base
from app.models import Reading, ReadingIdempotency
from app.routers.ingest import _claim_idempotency
from app.schemas import ReadingBatch, ReadingIn


@pytest.fixture()
def db() -> Session:
    engine = create_engine(
        "sqlite:///:memory:",
        connect_args={"check_same_thread": False},
        poolclass=StaticPool,
        future=True,
    )
    Base.metadata.create_all(
        bind=engine,
        tables=[Reading.__table__, ReadingIdempotency.__table__],
    )
    SessionLocal = sessionmaker(bind=engine, autoflush=False, autocommit=False, future=True)
    session = SessionLocal()
    try:
        yield session
    finally:
        session.close()
        engine.dispose()


def _ingest(db: Session, batch: ReadingBatch) -> tuple[int, int]:
    """Mirror routers.ingest.ingest_readings without FastAPI deps."""
    accepted = 0
    duplicates = 0
    for r in batch.readings:
        if r.record_id is not None:
            if not _claim_idempotency(db, batch.device_id, r.type, r.record_id):
                duplicates += 1
                continue
        db.add(
            Reading(
                ts=r.ts,
                device_id=batch.device_id,
                patient_id=r.patient_id,
                type=r.type,
                record_id=r.record_id,
                value=r.value,
                quality=r.quality,
                extra=r.extra,
            )
        )
        accepted += 1
    db.commit()
    return accepted, duplicates


def test_retry_same_logical_key_is_duplicate(db: Session):
    ts1 = dt.datetime(2026, 8, 1, 12, 0, tzinfo=dt.timezone.utc)
    ts2 = dt.datetime(2026, 8, 1, 12, 5, tzinfo=dt.timezone.utc)
    batch1 = ReadingBatch(
        device_id="dev-1",
        readings=[
            ReadingIn(ts=ts1, type="glucose", record_id=42, value=110.0),
        ],
    )
    a1, d1 = _ingest(db, batch1)
    assert (a1, d1) == (1, 0)

    # Same (device, type, record_id) with a different timestamp must not insert again.
    batch2 = ReadingBatch(
        device_id="dev-1",
        readings=[
            ReadingIn(ts=ts2, type="glucose", record_id=42, value=111.0),
        ],
    )
    a2, d2 = _ingest(db, batch2)
    assert (a2, d2) == (0, 1)
    assert db.scalar(select(func.count()).select_from(Reading)) == 1
    assert db.scalar(select(func.count()).select_from(ReadingIdempotency)) == 1


def test_same_record_id_different_types_both_accepted(db: Session):
    ts = dt.datetime(2026, 8, 1, 12, 0, tzinfo=dt.timezone.utc)
    batch = ReadingBatch(
        device_id="dev-1",
        readings=[
            ReadingIn(ts=ts, type="glucose", record_id=7, value=100.0),
            ReadingIn(ts=ts, type="vitals", record_id=7, value=72.0),
        ],
    )
    accepted, duplicates = _ingest(db, batch)
    assert (accepted, duplicates) == (2, 0)


def test_null_record_id_always_inserts(db: Session):
    ts = dt.datetime(2026, 8, 1, 12, 0, tzinfo=dt.timezone.utc)
    batch = ReadingBatch(
        device_id="dev-1",
        readings=[
            ReadingIn(ts=ts, type="glucose", record_id=None, value=90.0),
            ReadingIn(ts=ts, type="glucose", record_id=None, value=91.0),
        ],
    )
    accepted, duplicates = _ingest(db, batch)
    assert (accepted, duplicates) == (2, 0)
    assert len(db.scalars(select(Reading)).all()) == 2
