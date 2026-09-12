"""Pydantic v2 request/response schemas."""

from __future__ import annotations

import datetime as dt

from pydantic import BaseModel, ConfigDict, Field


class ORMModel(BaseModel):
    model_config = ConfigDict(from_attributes=True)


# --- Auth ------------------------------------------------------------------


class DevTokenRequest(BaseModel):
    username: str = "admin"
    roles: list[str] = Field(default_factory=lambda: ["super_admin"])
    ttl_seconds: int = 3600


class TokenResponse(BaseModel):
    access_token: str
    token_type: str = "bearer"
    expires_in: int


class MeResponse(BaseModel):
    subject: str
    username: str
    email: str | None = None
    roles: list[str]


# --- Users -----------------------------------------------------------------


class UserCreate(BaseModel):
    username: str
    email: str | None = None
    first_name: str | None = None
    last_name: str | None = None
    mobile1: str | None = None
    mobile2: str | None = None
    subject: str | None = None
    roles: list[str] = Field(default_factory=list)


class UserUpdate(BaseModel):
    email: str | None = None
    first_name: str | None = None
    last_name: str | None = None
    mobile1: str | None = None
    mobile2: str | None = None
    is_active: bool | None = None
    roles: list[str] | None = None


class UserOut(ORMModel):
    id: str
    username: str
    email: str | None
    first_name: str | None
    last_name: str | None
    mobile1: str | None
    mobile2: str | None
    is_active: bool
    created_at: dt.datetime
    roles: list[str] = Field(default_factory=list)


# --- Devices ---------------------------------------------------------------


class DeviceCreate(BaseModel):
    device_code: str
    device_type: str | None = None
    hw_id: str | None = None
    ble_id: str | None = None
    version: str | None = None
    controller: str | None = None
    sensors: str | None = None
    communication: str | None = None
    display: str | None = None
    firmware_version: str | None = None
    pcb_version: str | None = None
    bom: dict | None = None


class DeviceOut(ORMModel):
    id: str
    device_code: str
    device_type: str | None
    hw_id: str | None
    ble_id: str | None
    version: str | None
    controller: str | None
    sensors: str | None
    communication: str | None
    display: str | None
    firmware_version: str | None
    pcb_version: str | None
    provisioned: bool
    created_at: dt.datetime


class ProvisionTokenOut(BaseModel):
    token: str
    device_id: str | None
    expires_at: dt.datetime


# --- Sensors ---------------------------------------------------------------


class SensorCreate(BaseModel):
    sensor_id: str
    part_number: str | None = None
    datasheet_url: str | None = None
    supplier: str | None = None
    cost: float | None = None
    moq: int | None = None
    currency: str | None = None
    technical_parameters: dict | None = None


class SensorOut(ORMModel):
    id: str
    sensor_id: str
    part_number: str | None
    supplier: str | None
    cost: float | None
    moq: int | None
    currency: str | None
    created_at: dt.datetime


# --- Patients & mappings ---------------------------------------------------


class PatientCreate(BaseModel):
    nisense_id: str
    name: str | None = None
    gender: str | None = None
    age: int | None = None


class PatientOut(ORMModel):
    id: str
    nisense_id: str
    name: str | None
    gender: str | None
    age: int | None
    created_at: dt.datetime


class DevicePatientMapCreate(BaseModel):
    device_id: str
    patient_id: str
    activation_date: dt.datetime | None = None
    expiry_date: dt.datetime | None = None
    calibration_status: str | None = None


class DeviceAlgorithmMapCreate(BaseModel):
    device_id: str
    algorithm_version: str | None = None
    ai_model_version: str | None = None
    personalized_model_version: str | None = None


# --- Ingestion -------------------------------------------------------------


class ReadingIn(BaseModel):
    ts: dt.datetime
    type: str  # glucose | vitals | temp | ppg_raw | glucose_raw
    record_id: int | None = None
    value: float | None = None
    quality: int | None = None
    patient_id: str | None = None
    extra: dict | None = None


class ReadingBatch(BaseModel):
    device_id: str
    readings: list[ReadingIn]


class IngestResult(BaseModel):
    accepted: int
    duplicates: int
    max_record_id: int | None = None


class ReadingOut(ORMModel):
    id: int
    ts: dt.datetime
    device_id: str
    patient_id: str | None
    type: str
    record_id: int | None
    value: float | None
    quality: int | None
    extra: dict | None


# --- Artifacts -------------------------------------------------------------


class ArtifactOut(ORMModel):
    id: str
    kind: str
    variant: str | None
    version: str
    filename: str
    sha256: str
    size_bytes: int
    notes: str | None
    created_at: dt.datetime


class AppliedVersionIn(BaseModel):
    device_id: str
    kind: str  # firmware | model | bundle
    applied_version: str
