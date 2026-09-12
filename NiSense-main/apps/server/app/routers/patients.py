"""Patient master + device-to-patient / device-to-algorithm mappings."""

from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy import select
from sqlalchemy.orm import Session

from ..db import get_db
from ..models import (
    Device,
    DeviceAlgorithmMap,
    DevicePatientMap,
    Patient,
)
from ..schemas import (
    DeviceAlgorithmMapCreate,
    DevicePatientMapCreate,
    PatientCreate,
    PatientOut,
)
from ..security import (
    ROLE_DEVICE_ENGINEER,
    ROLE_DOCTOR,
    ROLE_SUPER_ADMIN,
    get_current_principal,
    require_roles,
)

router = APIRouter(tags=["patients"])

_MANAGE = require_roles(ROLE_SUPER_ADMIN, ROLE_DEVICE_ENGINEER, ROLE_DOCTOR)


@router.get("/patients", response_model=list[PatientOut],
            dependencies=[Depends(get_current_principal)])
def list_patients(db: Session = Depends(get_db)) -> list[Patient]:
    return db.scalars(select(Patient)).all()


@router.post("/patients", response_model=PatientOut, status_code=status.HTTP_201_CREATED,
             dependencies=[Depends(_MANAGE)])
def create_patient(payload: PatientCreate, db: Session = Depends(get_db)) -> Patient:
    if db.scalar(select(Patient).where(Patient.nisense_id == payload.nisense_id)):
        raise HTTPException(status.HTTP_409_CONFLICT, "nisense_id already exists")
    p = Patient(**payload.model_dump())
    db.add(p)
    db.commit()
    db.refresh(p)
    return p


@router.post("/mappings/device-patient", status_code=status.HTTP_201_CREATED,
             dependencies=[Depends(_MANAGE)])
def map_device_patient(payload: DevicePatientMapCreate, db: Session = Depends(get_db)) -> dict:
    if not db.get(Device, payload.device_id):
        raise HTTPException(status.HTTP_404_NOT_FOUND, "device not found")
    if not db.get(Patient, payload.patient_id):
        raise HTTPException(status.HTTP_404_NOT_FOUND, "patient not found")
    m = DevicePatientMap(**payload.model_dump())
    db.add(m)
    db.commit()
    return {"id": m.id}


@router.get("/mappings/device-patient", dependencies=[Depends(get_current_principal)])
def list_device_patient(db: Session = Depends(get_db)) -> list[dict]:
    rows = db.scalars(select(DevicePatientMap)).all()
    return [
        {
            "id": m.id,
            "device_id": m.device_id,
            "patient_id": m.patient_id,
            "activation_date": m.activation_date,
            "expiry_date": m.expiry_date,
            "calibration_status": m.calibration_status,
        }
        for m in rows
    ]


@router.post("/mappings/device-algorithm", status_code=status.HTTP_201_CREATED,
             dependencies=[Depends(_MANAGE)])
def map_device_algorithm(payload: DeviceAlgorithmMapCreate, db: Session = Depends(get_db)) -> dict:
    if not db.get(Device, payload.device_id):
        raise HTTPException(status.HTTP_404_NOT_FOUND, "device not found")
    m = DeviceAlgorithmMap(**payload.model_dump())
    db.add(m)
    db.commit()
    return {"id": m.id}


@router.get("/mappings/device-algorithm", dependencies=[Depends(get_current_principal)])
def list_device_algorithm(db: Session = Depends(get_db)) -> list[dict]:
    rows = db.scalars(select(DeviceAlgorithmMap)).all()
    return [
        {
            "id": m.id,
            "device_id": m.device_id,
            "algorithm_version": m.algorithm_version,
            "ai_model_version": m.ai_model_version,
            "personalized_model_version": m.personalized_model_version,
        }
        for m in rows
    ]
