"""Seed baseline data (roles are implicit via tokens; seed an admin user row)."""

from __future__ import annotations

import logging

from sqlalchemy import select

from .db import SessionLocal
from .models import User, UserRole
from .security import ROLE_SUPER_ADMIN

log = logging.getLogger("nisense.seed")


def seed_defaults() -> None:
    with SessionLocal() as db:
        admin = db.scalar(select(User).where(User.username == "admin"))
        if admin is None:
            admin = User(username="admin", email="admin@example.local",
                         first_name="Super", last_name="Admin")
            admin.roles.append(UserRole(role=ROLE_SUPER_ADMIN))
            db.add(admin)
            db.commit()
            log.info("Seeded default admin user")
