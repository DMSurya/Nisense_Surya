"""SQLAlchemy ORM models for the NiSense platform (Phase 1 core).

Covers Module 1 (Platform Administration) and the core of Module 3 (Device Data
Management) from the customer spec. Table names/columns are chosen so later
phases (clinical trials, statistics, AI training, dashboards) can be added
without refactoring these.
"""

from __future__ import annotations

import datetime as dt
import uuid

from sqlalchemy import (
    JSON,
    Boolean,
    DateTime,
    Float,
    ForeignKey,
    Integer,
    String,
    Text,
    UniqueConstraint,
    func,
)
from sqlalchemy.orm import Mapped, mapped_column, relationship

from .db import Base


def _uuid() -> str:
    return str(uuid.uuid4())


def _now() -> dt.datetime:
    return dt.datetime.now(dt.timezone.utc)


# --- Module 1: Platform Administration ------------------------------------


class User(Base):
    __tablename__ = "users"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    subject: Mapped[str | None] = mapped_column(String(255), unique=True, index=True)
    username: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    email: Mapped[str | None] = mapped_column(String(255), index=True)
    first_name: Mapped[str | None] = mapped_column(String(64))
    last_name: Mapped[str | None] = mapped_column(String(64))
    mobile1: Mapped[str | None] = mapped_column(String(32))
    mobile2: Mapped[str | None] = mapped_column(String(32))
    is_active: Mapped[bool] = mapped_column(Boolean, default=True)
    created_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)

    roles: Mapped[list["UserRole"]] = relationship(
        back_populates="user", cascade="all, delete-orphan"
    )


class UserRole(Base):
    __tablename__ = "user_roles"
    __table_args__ = (UniqueConstraint("user_id", "role", name="uq_user_role"),)

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    user_id: Mapped[str] = mapped_column(ForeignKey("users.id", ondelete="CASCADE"))
    role: Mapped[str] = mapped_column(String(32), index=True)

    user: Mapped[User] = relationship(back_populates="roles")


class Patient(Base):
    __tablename__ = "patients"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    nisense_id: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    name: Mapped[str | None] = mapped_column(String(128))
    gender: Mapped[str | None] = mapped_column(String(16))
    age: Mapped[int | None] = mapped_column(Integer)
    created_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)


# --- Module 1/3: Device & Sensor Masters ----------------------------------


class Device(Base):
    __tablename__ = "devices"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    device_code: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    device_type: Mapped[str | None] = mapped_column(String(64))
    hw_id: Mapped[str | None] = mapped_column(String(64), index=True)  # hwinfo hex
    ble_id: Mapped[str | None] = mapped_column(String(64), index=True)
    version: Mapped[str | None] = mapped_column(String(32))
    controller: Mapped[str | None] = mapped_column(String(64))
    sensors: Mapped[str | None] = mapped_column(String(255))
    communication: Mapped[str | None] = mapped_column(String(64))
    display: Mapped[str | None] = mapped_column(String(64))
    firmware_version: Mapped[str | None] = mapped_column(String(32))
    pcb_version: Mapped[str | None] = mapped_column(String(32))
    bom: Mapped[dict | None] = mapped_column(JSON)
    provisioned: Mapped[bool] = mapped_column(Boolean, default=False)
    created_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)


class Sensor(Base):
    __tablename__ = "sensors"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    sensor_id: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    part_number: Mapped[str | None] = mapped_column(String(128))
    datasheet_url: Mapped[str | None] = mapped_column(String(512))
    supplier: Mapped[str | None] = mapped_column(String(128))
    cost: Mapped[float | None] = mapped_column(Float)
    moq: Mapped[int | None] = mapped_column(Integer)
    currency: Mapped[str | None] = mapped_column(String(8))
    technical_parameters: Mapped[dict | None] = mapped_column(JSON)
    created_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)


# --- Module 3: Mappings ----------------------------------------------------


class DevicePatientMap(Base):
    __tablename__ = "device_patient_map"
    __table_args__ = (
        UniqueConstraint("device_id", "patient_id", name="uq_device_patient"),
    )

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    device_id: Mapped[str] = mapped_column(ForeignKey("devices.id", ondelete="CASCADE"))
    patient_id: Mapped[str] = mapped_column(ForeignKey("patients.id", ondelete="CASCADE"))
    activation_date: Mapped[dt.datetime | None] = mapped_column(DateTime(timezone=True))
    expiry_date: Mapped[dt.datetime | None] = mapped_column(DateTime(timezone=True))
    calibration_status: Mapped[str | None] = mapped_column(String(32))
    created_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)


class DeviceAlgorithmMap(Base):
    __tablename__ = "device_algorithm_map"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    device_id: Mapped[str] = mapped_column(
        ForeignKey("devices.id", ondelete="CASCADE"), index=True
    )
    algorithm_version: Mapped[str | None] = mapped_column(String(32))
    ai_model_version: Mapped[str | None] = mapped_column(String(32))
    personalized_model_version: Mapped[str | None] = mapped_column(String(32))
    created_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)


# --- Module 3: Measurements / data transfer --------------------------------


class ReadingIdempotency(Base):
    """Logical ingest key for readings with a device record_id.

    Kept as a separate table (not a UNIQUE on ``readings``) so TimescaleDB
    hypertables on ``readings.ts`` remain valid — hypertables require unique
    constraints to include the partition column.
    """

    __tablename__ = "reading_idempotency"

    device_id: Mapped[str] = mapped_column(String(64), primary_key=True)
    type: Mapped[str] = mapped_column(String(16), primary_key=True)
    record_id: Mapped[int] = mapped_column(Integer, primary_key=True)


class Reading(Base):
    """Time-series measurement. Backed by a TimescaleDB hypertable on `ts`.

    `type` is glucose|vitals|temp|ppg_raw|glucose_raw; scalar `value` holds
    the primary metric and `extra` (JSONB) carries the full record (algo
    trace, raw ADC samples, etc.).
    Idempotency for rows with ``record_id`` is enforced via
    :class:`ReadingIdempotency` on ``(device_id, type, record_id)``.
    """

    __tablename__ = "readings"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    ts: Mapped[dt.datetime] = mapped_column(
        DateTime(timezone=True), index=True, nullable=False
    )
    device_id: Mapped[str] = mapped_column(String(64), index=True, nullable=False)
    patient_id: Mapped[str | None] = mapped_column(String(64), index=True)
    type: Mapped[str] = mapped_column(String(16), index=True, nullable=False)
    record_id: Mapped[int | None] = mapped_column(Integer)
    value: Mapped[float | None] = mapped_column(Float)
    quality: Mapped[int | None] = mapped_column(Integer)
    extra: Mapped[dict | None] = mapped_column(JSON)
    created_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)


class CsvFile(Base):
    __tablename__ = "csv_files"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    name: Mapped[str] = mapped_column(String(255), index=True)
    device_id: Mapped[str | None] = mapped_column(String(64), index=True)
    patient_id: Mapped[str | None] = mapped_column(String(64), index=True)
    checksum: Mapped[str] = mapped_column(String(64), index=True)  # sha256 hex
    row_count: Mapped[int | None] = mapped_column(Integer)
    upload_status: Mapped[str] = mapped_column(String(16), default="uploaded")
    validation_status: Mapped[str] = mapped_column(String(16), default="pending")
    storage_key: Mapped[str | None] = mapped_column(String(512))
    uploaded_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)


# --- Module 1/3: Firmware & model artifacts --------------------------------


class Artifact(Base):
    """Signed firmware or glucose-model artifact stored via StorageBackend."""

    __tablename__ = "artifacts"
    __table_args__ = (
        UniqueConstraint("kind", "variant", "version", name="uq_artifact_version"),
    )

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    kind: Mapped[str] = mapped_column(String(16), index=True)  # firmware | model
    variant: Mapped[str | None] = mapped_column(String(32))  # wearable | pulse | ""
    version: Mapped[str] = mapped_column(String(64), index=True)
    filename: Mapped[str] = mapped_column(String(255))
    storage_key: Mapped[str] = mapped_column(String(512))
    sha256: Mapped[str] = mapped_column(String(64))
    signature_b64: Mapped[str | None] = mapped_column(Text)  # Ed25519 (model)
    size_bytes: Mapped[int] = mapped_column(Integer)
    notes: Mapped[str | None] = mapped_column(Text)
    created_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)


class DeviceVersionState(Base):
    __tablename__ = "device_version_state"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    device_id: Mapped[str] = mapped_column(String(64), index=True)
    kind: Mapped[str] = mapped_column(String(16))  # firmware | model
    applied_version: Mapped[str | None] = mapped_column(String(64))
    target_version: Mapped[str | None] = mapped_column(String(64))
    reported_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)


# --- Ops -------------------------------------------------------------------


class ProvisioningToken(Base):
    __tablename__ = "provisioning_tokens"

    id: Mapped[str] = mapped_column(String(36), primary_key=True, default=_uuid)
    token: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    device_id: Mapped[str | None] = mapped_column(String(64), index=True)
    expires_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True))
    used: Mapped[bool] = mapped_column(Boolean, default=False)
    created_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)


class AuditLog(Base):
    __tablename__ = "audit_log"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    ts: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now, index=True)
    actor: Mapped[str | None] = mapped_column(String(255), index=True)
    action: Mapped[str] = mapped_column(String(64))
    target: Mapped[str | None] = mapped_column(String(255))
    detail: Mapped[dict | None] = mapped_column(JSON)
