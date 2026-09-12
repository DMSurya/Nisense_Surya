"""Artifact storage backend (cloud-portable).

`local`  -> Raspberry Pi disk under NISENSE_STORAGE_LOCAL_DIR.
`s3`     -> AWS S3 or Azure Blob (S3-compatible endpoint) via boto3 if present.

Business logic depends only on StorageBackend, so switching Pi <-> cloud is a
config change, not a code change.
"""

from __future__ import annotations

import hashlib
import os
from abc import ABC, abstractmethod
from pathlib import Path

from .config import settings


class StorageBackend(ABC):
    @abstractmethod
    def put(self, key: str, data: bytes) -> str: ...

    @abstractmethod
    def get(self, key: str) -> bytes: ...

    @abstractmethod
    def exists(self, key: str) -> bool: ...

    @abstractmethod
    def delete(self, key: str) -> None: ...


class LocalStorage(StorageBackend):
    def __init__(self, root: str) -> None:
        self.root = Path(root)
        self.root.mkdir(parents=True, exist_ok=True)

    def _path(self, key: str) -> Path:
        # Prevent path traversal.
        safe = key.replace("..", "_").lstrip("/")
        p = self.root / safe
        p.parent.mkdir(parents=True, exist_ok=True)
        return p

    def put(self, key: str, data: bytes) -> str:
        self._path(key).write_bytes(data)
        return key

    def get(self, key: str) -> bytes:
        return self._path(key).read_bytes()

    def exists(self, key: str) -> bool:
        return self._path(key).exists()

    def delete(self, key: str) -> None:
        p = self._path(key)
        if p.exists():
            p.unlink()


class S3Storage(StorageBackend):
    def __init__(self) -> None:
        import boto3  # imported lazily; only needed for the s3 backend

        kwargs = {"region_name": settings.s3_region}
        if settings.s3_endpoint_url:
            kwargs["endpoint_url"] = settings.s3_endpoint_url
        self._s3 = boto3.client("s3", **kwargs)
        self._bucket = settings.s3_bucket

    def put(self, key: str, data: bytes) -> str:
        self._s3.put_object(Bucket=self._bucket, Key=key, Body=data)
        return key

    def get(self, key: str) -> bytes:
        return self._s3.get_object(Bucket=self._bucket, Key=key)["Body"].read()

    def exists(self, key: str) -> bool:
        try:
            self._s3.head_object(Bucket=self._bucket, Key=key)
            return True
        except Exception:
            return False

    def delete(self, key: str) -> None:
        self._s3.delete_object(Bucket=self._bucket, Key=key)


_backend: StorageBackend | None = None


def get_storage() -> StorageBackend:
    global _backend
    if _backend is None:
        if settings.storage_backend == "s3":
            _backend = S3Storage()
        else:
            _backend = LocalStorage(settings.storage_local_dir)
    return _backend


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()
