"""Portable filesystem defaults for the TRIAD simulation toolchain.

Command-line arguments remain authoritative. These defaults make a fresh clone
usable without embedding a contributor's workstation paths in source control.
"""

from __future__ import annotations

import os
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[3]


def _environment_path(name: str, fallback: Path) -> Path:
    value = os.environ.get(name)
    return Path(value).expanduser() if value else fallback


# A sibling TRIAD checkout is the portable convention. Existing installations
# can set TRIAD_UNREAL_PROJECT to any Unreal project directory.
DEFAULT_TRIAD_PROJECT = _environment_path(
    "TRIAD_UNREAL_PROJECT",
    REPOSITORY_ROOT.parent / "TRIAD",
)
DEFAULT_TRIAD_SENSOR_SAVED_DIR = DEFAULT_TRIAD_PROJECT / "Saved" / "SingaporeSensorFusion"
_HOST_CONFIG = DEFAULT_TRIAD_PROJECT / "Config" / "SingaporeSensorFusion.json"
DEFAULT_TRIAD_CONFIG = _environment_path(
    "TRIAD_SCENARIO_CONFIG",
    _HOST_CONFIG
    if _HOST_CONFIG.is_file()
    else REPOSITORY_ROOT / "unreal" / "Config" / "SingaporeSensorFusion.json",
)
DEFAULT_TRIAD_LOG = DEFAULT_TRIAD_PROJECT / "Saved" / "Logs" / "TRIAD.log"

DEFAULT_OUTPUT_DIRECTORY = _environment_path(
    "TRIAD_FUSION_OUTPUT_DIR",
    REPOSITORY_ROOT / "outputs",
)
DEFAULT_MODEL_ROOT = _environment_path(
    "TRIAD_MODEL_ROOT",
    REPOSITORY_ROOT / "core" / "models",
)
