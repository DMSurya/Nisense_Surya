"""Device Master CRUD, provisioning tokens, and device registration."""

from __future__ import annotations

import datetime as dt
import secrets

from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel
from sqlalchemy import select
from sqlalchemy.orm import Session

from ..config import settings
from ..db import get_db
from ..models import Device, ProvisioningToken
from ..schemas import DeviceCreate, DeviceOut, ProvisionTokenOut
from ..security import (
    ROLE_DEVICE_ENGINEER,
    ROLE_SUPER_ADMIN,
    get_current_principal,
    require_roles,
)

router = APIRouter(prefix="/devices", tags=["devices"])

_MANAGE = require_roles(ROLE_SUPER_ADMIN, ROLE_DEVICE_ENGINEER)


@router.get("", response_model=list[DeviceOut], dependencies=[Depends(get_current_principal)])
def list_devices(db: Session = Depends(get_db)) -> list[Device]:
    return db.scalars(select(Device)).all()


@router.post("", response_model=DeviceOut, status_code=status.HTTP_201_CREATED,
             dependencies=[Depends(_MANAGE)])
def create_device(payload: DeviceCreate, db: Session = Depends(get_db)) -> Device:
    if db.scalar(select(Device).where(Device.device_code == payload.device_code)):
        raise HTTPException(status.HTTP_409_CONFLICT, "device_code already exists")
    dev = Device(**payload.model_dump())
    db.add(dev)
    db.commit()
    db.refresh(dev)
    return dev


@router.get("/{device_id}", response_model=DeviceOut,
            dependencies=[Depends(get_current_principal)])
def get_device(device_id: str, db: Session = Depends(get_db)) -> Device:
    dev = db.get(Device, device_id)
    if not dev:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "device not found")
    return dev


@router.patch("/{device_id}", response_model=DeviceOut, dependencies=[Depends(_MANAGE)])
def update_device(device_id: str, payload: DeviceCreate, db: Session = Depends(get_db)) -> Device:
    dev = db.get(Device, device_id)
    if not dev:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "device not found")
    for k, v in payload.model_dump(exclude_unset=True).items():
        setattr(dev, k, v)
    db.commit()
    db.refresh(dev)
    return dev


@router.post("/{device_id}/provision-token", response_model=ProvisionTokenOut,
             dependencies=[Depends(_MANAGE)])
def issue_provision_token(device_id: str, db: Session = Depends(get_db)) -> ProvisionTokenOut:
    dev = db.get(Device, device_id)
    if not dev:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "device not found")
    tok = ProvisioningToken(
        token=secrets.token_urlsafe(32),
        device_id=device_id,
        expires_at=dt.datetime.now(dt.timezone.utc)
        + dt.timedelta(hours=settings.provisioning_token_ttl_hours),
    )
    db.add(tok)
    db.commit()
    return ProvisionTokenOut(token=tok.token, device_id=device_id, expires_at=tok.expires_at)


class RegisterRequest(BaseModel):
    token: str
    hw_id: str
    ble_id: str | None = None
    firmware_version: str | None = None


@router.post("/register", response_model=DeviceOut, dependencies=[Depends(get_current_principal)])
def register_device(req: RegisterRequest, db: Session = Depends(get_db)) -> Device:
    """Device registration relayed by the mobile gateway using a provisioning token."""
    tok = db.scalar(select(ProvisioningToken).where(ProvisioningToken.token == req.token))
    if not tok or tok.used:
        raise HTTPException(status.HTTP_400_BAD_REQUEST, "invalid or used token")
    if tok.expires_at < dt.datetime.now(dt.timezone.utc):
        raise HTTPException(status.HTTP_400_BAD_REQUEST, "token expired")

    dev = db.get(Device, tok.device_id) if tok.device_id else None
    if dev is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "token has no device")

    dev.hw_id = req.hw_id
    dev.ble_id = req.ble_id or dev.ble_id
    dev.firmware_version = req.firmware_version or dev.firmware_version
    dev.provisioned = True
    tok.used = True
    db.commit()
    db.refresh(dev)
    return dev
