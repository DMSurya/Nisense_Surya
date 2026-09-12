"""User & role management (RBAC admin)."""

from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException, Response, status
from sqlalchemy import select
from sqlalchemy.orm import Session

from ..db import get_db
from ..models import User, UserRole
from ..schemas import UserCreate, UserOut, UserUpdate
from ..security import ALL_ROLES, ROLE_SUPER_ADMIN, require_roles

router = APIRouter(prefix="/users", tags=["users"])


def _to_out(u: User) -> UserOut:
    return UserOut(
        id=u.id,
        username=u.username,
        email=u.email,
        first_name=u.first_name,
        last_name=u.last_name,
        mobile1=u.mobile1,
        mobile2=u.mobile2,
        is_active=u.is_active,
        created_at=u.created_at,
        roles=[r.role for r in u.roles],
    )


def _set_roles(u: User, roles: list[str]) -> None:
    valid = [r for r in roles if r in ALL_ROLES]
    u.roles.clear()
    for r in valid:
        u.roles.append(UserRole(role=r))


@router.get("", response_model=list[UserOut], dependencies=[Depends(require_roles(ROLE_SUPER_ADMIN))])
def list_users(db: Session = Depends(get_db)) -> list[UserOut]:
    users = db.scalars(select(User)).all()
    return [_to_out(u) for u in users]


@router.post("", response_model=UserOut, status_code=status.HTTP_201_CREATED,
             dependencies=[Depends(require_roles(ROLE_SUPER_ADMIN))])
def create_user(payload: UserCreate, db: Session = Depends(get_db)) -> UserOut:
    if db.scalar(select(User).where(User.username == payload.username)):
        raise HTTPException(status.HTTP_409_CONFLICT, "username already exists")
    u = User(
        username=payload.username,
        email=payload.email,
        first_name=payload.first_name,
        last_name=payload.last_name,
        mobile1=payload.mobile1,
        mobile2=payload.mobile2,
        subject=payload.subject,
    )
    _set_roles(u, payload.roles)
    db.add(u)
    db.commit()
    db.refresh(u)
    return _to_out(u)


@router.get("/{user_id}", response_model=UserOut,
            dependencies=[Depends(require_roles(ROLE_SUPER_ADMIN))])
def get_user(user_id: str, db: Session = Depends(get_db)) -> UserOut:
    u = db.get(User, user_id)
    if not u:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "user not found")
    return _to_out(u)


@router.patch("/{user_id}", response_model=UserOut,
              dependencies=[Depends(require_roles(ROLE_SUPER_ADMIN))])
def update_user(user_id: str, payload: UserUpdate, db: Session = Depends(get_db)) -> UserOut:
    u = db.get(User, user_id)
    if not u:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "user not found")
    data = payload.model_dump(exclude_unset=True)
    roles = data.pop("roles", None)
    for k, v in data.items():
        setattr(u, k, v)
    if roles is not None:
        _set_roles(u, roles)
    db.commit()
    db.refresh(u)
    return _to_out(u)


@router.delete(
    "/{user_id}",
    status_code=status.HTTP_204_NO_CONTENT,
    response_class=Response,
    dependencies=[Depends(require_roles(ROLE_SUPER_ADMIN))],
)
def delete_user(user_id: str, db: Session = Depends(get_db)) -> Response:
    u = db.get(User, user_id)
    if u:
        db.delete(u)
        db.commit()
    return Response(status_code=status.HTTP_204_NO_CONTENT)
