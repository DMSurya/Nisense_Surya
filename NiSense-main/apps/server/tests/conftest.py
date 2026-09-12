"""Pytest bootstrap: keep unit tests off the Postgres driver when possible."""

from __future__ import annotations

import os

# Must run before any `app.*` import so Settings / create_engine see SQLite.
os.environ["NISENSE_DATABASE_URL"] = "sqlite:///:memory:"
os.environ.setdefault("NISENSE_OIDC_PROVIDER", "dev")
