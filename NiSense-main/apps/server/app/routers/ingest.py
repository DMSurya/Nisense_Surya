"""Measurement ingestion + CSV upload + readings query.

- Batch readings upload is idempotent on (device_id, type, record_id) via the
  ``reading_idempotency`` table so the mobile gateway can safely retry and
  backfill offline-queued records (Timescale-safe; no UNIQUE on readings.ts).
- CSV upload stores the file via the StorageBackend and records a checksum;
  re-uploading the same file (same checksum) is a no-op.
"""

from __future__ import annotations

import datetime as dt

from fastapi import APIRouter, Depends, File, Form, Query, UploadFile, status
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session

from ..db import get_db
from ..models import CsvFile, Reading, ReadingIdempotency
from ..schemas import IngestResult, ReadingBatch, ReadingOut
from ..security import get_current_principal
from ..storage import get_storage, sha256_hex

router = APIRouter(tags=["ingest"])


def _claim_idempotency(db: Session, device_id: str, type_: str, record_id: int) -> bool:
    """Return True if this logical key was newly claimed; False if already seen."""
    try:
        with db.begin_nested():
            db.add(
                ReadingIdempotency(
                    device_id=device_id,
                    type=type_,
                    record_id=record_id,
                )
            )
            db.flush()
        return True
    except IntegrityError:
        return False


@router.post("/ingest/readings", response_model=IngestResult,
             dependencies=[Depends(get_current_principal)])
def ingest_readings(batch: ReadingBatch, db: Session = Depends(get_db)) -> IngestResult:
    if not batch.readings:
        return IngestResult(accepted=0, duplicates=0, max_record_id=None)

    accepted = 0
    duplicates = 0
    max_record_id: int | None = None

    for r in batch.readings:
        if r.record_id is not None:
            max_record_id = max(max_record_id or 0, r.record_id)
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
    return IngestResult(accepted=accepted, duplicates=duplicates, max_record_id=max_record_id)


@router.get("/readings", response_model=list[ReadingOut],
            dependencies=[Depends(get_current_principal)])
def query_readings(
    db: Session = Depends(get_db),
    device_id: str | None = None,
    patient_id: str | None = None,
    type: str | None = None,
    since: dt.datetime | None = None,
    until: dt.datetime | None = None,
    limit: int = Query(200, le=2000),
) -> list[Reading]:
    stmt = select(Reading)
    if device_id:
        stmt = stmt.where(Reading.device_id == device_id)
    if patient_id:
        stmt = stmt.where(Reading.patient_id == patient_id)
    if type:
        stmt = stmt.where(Reading.type == type)
    if since:
        stmt = stmt.where(Reading.ts >= since)
    if until:
        stmt = stmt.where(Reading.ts <= until)
    stmt = stmt.order_by(Reading.ts.desc()).limit(limit)
    return db.scalars(stmt).all()


@router.post("/ingest/csv", status_code=status.HTTP_201_CREATED,
             dependencies=[Depends(get_current_principal)])
async def upload_csv(
    file: UploadFile = File(...),
    device_id: str | None = Form(None),
    patient_id: str | None = Form(None),
    db: Session = Depends(get_db),
) -> dict:
    data = await file.read()
    checksum = sha256_hex(data)

    existing = db.scalar(select(CsvFile).where(CsvFile.checksum == checksum))
    if existing:
        return {
            "id": existing.id,
            "checksum": checksum,
            "status": "duplicate",
            "validation_status": existing.validation_status,
        }

    row_count = max(0, data.count(b"\n") - 1)
    key = f"csv/{device_id or 'unknown'}/{checksum}_{file.filename}"
    get_storage().put(key, data)

    rec = CsvFile(
        name=file.filename or "upload.csv",
        device_id=device_id,
        patient_id=patient_id,
        checksum=checksum,
        row_count=row_count,
        upload_status="uploaded",
        validation_status="valid" if row_count > 0 else "empty",
        storage_key=key,
    )
    db.add(rec)
    db.commit()
    return {
        "id": rec.id,
        "checksum": checksum,
        "rows": row_count,
        "status": "uploaded",
        "validation_status": rec.validation_status,
    }
