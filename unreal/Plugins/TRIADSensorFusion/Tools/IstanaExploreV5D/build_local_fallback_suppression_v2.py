#!/usr/bin/env python3
"""Build the additive V2 local-fallback render suppression derivative.

V2 preserves the immutable V1 implementation and canonical/RF inputs. It
reuses the proven byte-block omission machinery under an independent schema,
contract, exact three-group allow-list, and output namespace.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parent
CONTRACT_PATH = ROOT / "local_fallback_suppression_v2.contract.json"
CONTRACT_SCHEMA = (
    "triad.istana_explore_v5d.local_fallback_suppression_contract.v2"
)
METADATA_SCHEMA = (
    "triad.istana_explore_v5d.local_fallback_suppression_metadata.v2"
)
MANIFEST_SCHEMA = (
    "triad.istana_explore_v5d.local_fallback_suppression_manifest.v2"
)
EXPECTED_SOURCE_KEYS = (
    "OSM:way:46521250",
    "OSM:way:1551538490",
    "OSM:way:429681826",
)
EXPECTED_GROUPS = (
    "OSM_way_46521250_P00",
    "OSM_way_1551538490_P00",
    "OSM_way_429681826_P00",
)


_V1_PATH = ROOT / "build_local_fallback_suppression_v1.py"
_V1_EXPECTED_BYTES = 17594
_V1_EXPECTED_SHA256 = (
    "D8B35571AD49E07FD226D8D65EAD6D7F809E3C347D19D779E5ABBCFA7B6E1B80"
)
_V1_PAYLOAD = _V1_PATH.read_bytes()
if (
    len(_V1_PAYLOAD) != _V1_EXPECTED_BYTES
    or hashlib.sha256(_V1_PAYLOAD).hexdigest().upper() != _V1_EXPECTED_SHA256
):
    raise RuntimeError("Immutable V1 suppression implementation hash guard failed.")
_SPEC = importlib.util.spec_from_file_location(
    "_triad_local_fallback_suppression_v1_for_v2", _V1_PATH
)
if not _SPEC or not _SPEC.loader:
    raise RuntimeError("Could not load the immutable V1 suppression implementation.")
_IMPLEMENTATION = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = _IMPLEMENTATION
_SPEC.loader.exec_module(_IMPLEMENTATION)

# The reused implementation resolves all version/scope values from its module
# globals at call time. Only this private module instance is retargeted; the V1
# module, contract, generated outputs, and public import identity are untouched.
_IMPLEMENTATION.CONTRACT_PATH = CONTRACT_PATH
_IMPLEMENTATION.CONTRACT_SCHEMA = CONTRACT_SCHEMA
_IMPLEMENTATION.METADATA_SCHEMA = METADATA_SCHEMA
_IMPLEMENTATION.MANIFEST_SCHEMA = MANIFEST_SCHEMA
_IMPLEMENTATION.EXPECTED_SOURCE_KEYS = EXPECTED_SOURCE_KEYS
_IMPLEMENTATION.EXPECTED_GROUPS = EXPECTED_GROUPS
_IMPLEMENTATION.__file__ = str(Path(__file__).resolve())

canonical_bytes = _IMPLEMENTATION.canonical_bytes
sha256_bytes = _IMPLEMENTATION.sha256_bytes
file_record = _IMPLEMENTATION.file_record
scan_obj = _IMPLEMENTATION.scan_obj


def load_contract(path: Path = CONTRACT_PATH) -> dict[str, Any]:
    """Load and fail-close the exact V2 contract."""

    candidate = json.loads(path.read_text(encoding="utf-8"))
    if candidate.get("schema") != CONTRACT_SCHEMA:
        raise ValueError("Unexpected V2 local-fallback suppression contract schema.")
    entries = candidate.get("suppression", {}).get("sourceKeys", [])
    if tuple(row.get("sourceKey") for row in entries) != EXPECTED_SOURCE_KEYS:
        raise ValueError("V2 suppression scope must remain the exact three admitted OSM keys.")
    if tuple(row.get("objGroup") for row in entries) != EXPECTED_GROUPS:
        raise ValueError("V2 suppression OBJ group roster changed.")
    implementation = candidate.get("implementationBase", {})
    if implementation != {
        "role": "immutableV1ByteBlockOmissionImplementation",
        "file": _V1_PATH.name,
        "bytes": _V1_EXPECTED_BYTES,
        "sha256": _V1_EXPECTED_SHA256,
    }:
        raise ValueError("V2 contract lost its exact immutable V1 implementation pin.")
    return _IMPLEMENTATION.load_contract(path)


def validate_inputs(
    input_dir: Path,
    contract: dict[str, Any],
) -> list[dict[str, Any]]:
    return _IMPLEMENTATION.validate_inputs(input_dir, contract)


def validate_suppression_metadata(
    input_dir: Path,
    contract: dict[str, Any],
) -> list[dict[str, Any]]:
    return _IMPLEMENTATION.validate_suppression_metadata(input_dir, contract)


def filter_render_obj(
    payload: bytes,
    group_names: Iterable[str],
) -> tuple[bytes, list[dict[str, Any]]]:
    """Delete exactly the three V2 OBJ group blocks without rewriting bytes."""

    targets = tuple(group_names)
    if targets != EXPECTED_GROUPS:
        raise ValueError("Filter scope must be the exact three admitted V2 OBJ groups.")
    return _IMPLEMENTATION.filter_render_obj(payload, targets)


def build(
    input_dir: Path,
    output_dir: Path,
    contract_path: Path = CONTRACT_PATH,
) -> dict[str, Any]:
    """Generate the deterministic V2 derivative in a separate directory."""

    load_contract(contract_path)
    return _IMPLEMENTATION.build(input_dir, output_dir, contract_path)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input-dir",
        required=True,
        type=Path,
        help="Hash-pinned 2026-08-31 GeneratedCurrent directory.",
    )
    parser.add_argument(
        "--output-dir",
        required=True,
        type=Path,
        help="Separate directory for the additive V2 filtered derivative.",
    )
    parser.add_argument("--contract", type=Path, default=CONTRACT_PATH)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest = build(args.input_dir, args.output_dir, args.contract)
    print(json.dumps(manifest, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
