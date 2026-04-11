from __future__ import annotations

import os
from pathlib import Path

from path_helpers import native_binary_default, runtime_tmp_dir, scratch_dir


def project_root() -> Path:
    return Path(__file__).resolve().parents[1]


def native_binary_default() -> Path:
    return project_root() / "build" / "native" / "dj1000"


def scratch_dir(name: str | None = None) -> Path:
    root = project_root() / "tmp_debug"
    if not name:
        return root
    return root / name


def runtime_tmp_dir() -> Path:
    return scratch_dir()


def reference_dataset_default() -> Path | None:
    value = os.environ.get("DJ1000_REFERENCE_DATASET")
    if not value:
        return None
    return Path(value).expanduser().resolve()
