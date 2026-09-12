"""Firmware, glucose-model, and multi-image bundle artifact store.

Artifacts are stored via the StorageBackend (Pi disk or S3/Blob). Model
artifacts carry an Ed25519 signature (produced by the Pi `nisense-sign`
command) which is verified here before acceptance and again on-device before
the A/B slot is flipped. Bundle artifacts are ZIP files with a
`nisense-ota-v1` manifest; per-component SHA-256 hashes are verified on upload,
and any embedded model component is signature-checked when required.

The mobile gateway fetches the latest artifact and relays it (SMP DFU for
firmware, model-transfer GATT for model, XIP-transfer GATT for assets, or
orchestrated multi-step for bundles).
"""

from __future__ import annotations

import io

from fastapi import APIRouter, Depends, File, Form, HTTPException, Response, UploadFile, status
from fastapi.responses import StreamingResponse
from sqlalchemy import select
from sqlalchemy.orm import Session

from ..artifact_bundle import BundleValidationError, verify_bundle_zip
from ..db import get_db
from ..models import Artifact, DeviceVersionState
from ..schemas import AppliedVersionIn, ArtifactOut
from ..security import (
    ROLE_DEVICE_ENGINEER,
    ROLE_SUPER_ADMIN,
    get_current_principal,
    require_roles,
)
from ..signing import verify_model_signature
from ..storage import get_storage, sha256_hex

router = APIRouter(tags=["artifacts"])

_MANAGE = require_roles(ROLE_SUPER_ADMIN, ROLE_DEVICE_ENGINEER)
_KINDS = {"firmware", "model", "bundle"}


@router.post("/artifacts", response_model=ArtifactOut, status_code=status.HTTP_201_CREATED,
             dependencies=[Depends(_MANAGE)])
async def upload_artifact(
    kind: str = Form(...),
    version: str = Form(...),
    variant: str | None = Form(None),
    notes: str | None = Form(None),
    signature_b64: str | None = Form(None),
    file: UploadFile = File(...),
    db: Session = Depends(get_db),
) -> Artifact:
    if kind not in _KINDS:
        raise HTTPException(status.HTTP_400_BAD_REQUEST, "kind must be firmware|model|bundle")

    data = await file.read()
    if kind == "model" and not verify_model_signature(data, signature_b64):
        raise HTTPException(status.HTTP_400_BAD_REQUEST, "model signature verification failed")
    if kind == "bundle":
        try:
            verify_bundle_zip(data)
        except BundleValidationError as exc:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, str(exc)) from exc

    if db.scalar(
        select(Artifact).where(
            Artifact.kind == kind,
            Artifact.variant == (variant or ""),
            Artifact.version == version,
        )
    ):
        raise HTTPException(status.HTTP_409_CONFLICT, "artifact version already exists")

    key = f"{kind}/{variant or 'common'}/{version}/{file.filename}"
    get_storage().put(key, data)

    art = Artifact(
        kind=kind,
        variant=variant or "",
        version=version,
        filename=file.filename or f"{kind}.bin",
        storage_key=key,
        sha256=sha256_hex(data),
        signature_b64=signature_b64,
        size_bytes=len(data),
        notes=notes,
    )
    db.add(art)
    db.commit()
    db.refresh(art)
    return art


@router.get("/artifacts", response_model=list[ArtifactOut],
            dependencies=[Depends(get_current_principal)])
def list_artifacts(
    db: Session = Depends(get_db),
    kind: str | None = None,
    variant: str | None = None,
) -> list[Artifact]:
    stmt = select(Artifact)
    if kind:
        stmt = stmt.where(Artifact.kind == kind)
    if variant is not None:
        stmt = stmt.where(Artifact.variant == variant)
    return db.scalars(stmt.order_by(Artifact.created_at.desc())).all()


@router.get("/artifacts/latest", response_model=ArtifactOut,
            dependencies=[Depends(get_current_principal)])
def latest_artifact(
    kind: str,
    variant: str | None = None,
    db: Session = Depends(get_db),
) -> Artifact:
    stmt = select(Artifact).where(Artifact.kind == kind)
    if variant is not None:
        stmt = stmt.where(Artifact.variant == variant)
    art = db.scalars(stmt.order_by(Artifact.created_at.desc()).limit(1)).first()
    if not art:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "no artifact found")
    return art


@router.get("/artifacts/{artifact_id}/download",
            dependencies=[Depends(get_current_principal)])
def download_artifact(artifact_id: str, db: Session = Depends(get_db)) -> StreamingResponse:
    art = db.get(Artifact, artifact_id)
    if not art:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "artifact not found")
    data = get_storage().get(art.storage_key)
    headers = {
        "Content-Disposition": f'attachment; filename="{art.filename}"',
        "X-Artifact-SHA256": art.sha256,
    }
    if art.signature_b64:
        headers["X-Artifact-Signature"] = art.signature_b64
    media = "application/zip" if art.kind == "bundle" else "application/octet-stream"
    return StreamingResponse(io.BytesIO(data), media_type=media, headers=headers)


@router.delete("/artifacts/{artifact_id}", status_code=status.HTTP_204_NO_CONTENT,
               dependencies=[Depends(_MANAGE)])
def delete_artifact(artifact_id: str, db: Session = Depends(get_db)) -> Response:
    """Remove an artifact so the same (kind, variant, version) can be re-uploaded."""
    art = db.get(Artifact, artifact_id)
    if not art:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "artifact not found")
    try:
        get_storage().delete(art.storage_key)
    except Exception:
        # DB row still removed even if the blob is already gone.
        pass
    db.delete(art)
    db.commit()
    return Response(status_code=status.HTTP_204_NO_CONTENT)


@router.post("/artifacts/applied-version", status_code=status.HTTP_201_CREATED,
             dependencies=[Depends(get_current_principal)])
def report_applied_version(payload: AppliedVersionIn, db: Session = Depends(get_db)) -> dict:
    st = DeviceVersionState(
        device_id=payload.device_id,
        kind=payload.kind,
        applied_version=payload.applied_version,
    )
    db.add(st)
    db.commit()
    return {"status": "recorded"}
