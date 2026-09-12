"""Sensor Master CRUD."""

from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException, Response, status
from sqlalchemy import select
from sqlalchemy.orm import Session

from ..db import get_db
from ..models import Sensor
from ..schemas import SensorCreate, SensorOut
from ..security import (
    ROLE_DEVICE_ENGINEER,
    ROLE_SUPER_ADMIN,
    get_current_principal,
    require_roles,
)

router = APIRouter(prefix="/sensors", tags=["sensors"])

_MANAGE = require_roles(ROLE_SUPER_ADMIN, ROLE_DEVICE_ENGINEER)


@router.get("", response_model=list[SensorOut], dependencies=[Depends(get_current_principal)])
def list_sensors(db: Session = Depends(get_db)) -> list[Sensor]:
    return db.scalars(select(Sensor)).all()


@router.post("", response_model=SensorOut, status_code=status.HTTP_201_CREATED,
             dependencies=[Depends(_MANAGE)])
def create_sensor(payload: SensorCreate, db: Session = Depends(get_db)) -> Sensor:
    if db.scalar(select(Sensor).where(Sensor.sensor_id == payload.sensor_id)):
        raise HTTPException(status.HTTP_409_CONFLICT, "sensor_id already exists")
    s = Sensor(**payload.model_dump())
    db.add(s)
    db.commit()
    db.refresh(s)
    return s


@router.patch("/{pk}", response_model=SensorOut, dependencies=[Depends(_MANAGE)])
def update_sensor(pk: str, payload: SensorCreate, db: Session = Depends(get_db)) -> Sensor:
    s = db.get(Sensor, pk)
    if not s:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "sensor not found")
    for k, v in payload.model_dump(exclude_unset=True).items():
        setattr(s, k, v)
    db.commit()
    db.refresh(s)
    return s


@router.delete(
    "/{pk}",
    status_code=status.HTTP_204_NO_CONTENT,
    response_class=Response,
    dependencies=[Depends(_MANAGE)],
)
def delete_sensor(pk: str, db: Session = Depends(get_db)) -> Response:
    s = db.get(Sensor, pk)
    if s:
        db.delete(s)
        db.commit()
    return Response(status_code=status.HTTP_204_NO_CONTENT)
