"""Strict file-backed evidence intake for a Stage-0 placement study.

This module validates the structural boundary between a self-contained demo
precompute and a file-backed evidence package.  Structural intake is not a
Stage-1 outcome review or solver authorization.  This module never runs an
optimizer, edits Unreal, or treats a native RF file test as field calibration.

All manifest file references are normalized relative paths beneath an
operator-supplied approved root.  Every file is read from a stable regular-file
snapshot, byte-counted, SHA-256 checked, and then semantically cross-bound.
"""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import math
import os
from pathlib import Path, PurePosixPath
import stat
import unicodedata
from typing import Any, Mapping, Sequence

from .contracts import stable_digest
from .planning_pipeline import (
    CANDIDATE_TRAJECTORY_PRECOMPUTE_SCHEMA,
    EXACT_SIMULATOR_REPLAY_SOURCE,
    MEASURED_CALIBRATION_EVIDENCE,
    MODEL_CALIBRATION_BUNDLE_SCHEMA,
    SENSOR_CLASSES,
    SENSOR_FAMILIES,
    STRICT_EVIDENCE_EXECUTION_CLASS,
    SensorModelCalibration,
    validate_precompute_truth_contract,
)
from .study_contract import Stage0StudyContract


EVIDENCE_INTAKE_MANIFEST_SCHEMA = "triad.placement_evidence_intake_manifest.v1"
EVIDENCE_INTAKE_RECEIPT_SCHEMA = "triad.placement_evidence_intake_receipt.v1"
CANDIDATE_CATALOG_SCHEMA = "triad.placement_candidate_catalog.v1"
TRAJECTORY_CORPUS_SCHEMA = "triad.placement_trajectory_corpus.v1"
SCENARIO_PARTITION_SCHEMA = "triad.placement_scenario_partition.v1"
EVALUATOR_EVIDENCE_SCHEMA = "triad.sensor_evaluator_evidence.v1"
NATIVE_RF_RECEIPT_SCHEMA = (
    "triad.rf.one_kilometre_v2.actual_file_native_transaction.receipt.v3"
)
NATIVE_RF_AUTOMATION_FILTER = (
    "TRIAD.RF.IndexedGeometryQuery.OneKilometreV2ActualFile"
)
STRICT_SCORE_SEMANTICS = "MEASURED_CALIBRATION_WITHIN_DECLARED_LIMITS"
EVALUATOR_RESULT_ROW_FIELDS = (
    "candidateId",
    "trajectoryId",
    "scenarioId",
    "partitionId",
    "sensorClass",
    "evidenceFamily",
    "detectionFraction",
    "firstDetectionSeconds",
    "meanQuality",
    "sourceClass",
    "exactSimulatorReplay",
    "measuredCalibration",
    "rfGeometryQueried",
    "hardwareId",
    "hardwareRevision",
    "calibrationId",
)
ROBUST_OUTCOME_SEMANTICS_NOT_VALIDATED = (
    "FUSED_VALID_DETECTION_AND_TRACK",
    "P95_AND_CVAR_LATENCY",
    "TRACK_CONTINUITY_AND_BREAKS",
    "LOCALIZATION_ERROR_AND_COVARIANCE",
    "FALSE_DETECTIONS_AND_FALSE_TRACKS",
    "OUTAGE_RESILIENCE",
    "RESOURCE_AND_CUE_LOAD",
    "CRITICAL_ZONE_METRICS",
    "REQUIREMENTS_THRESHOLDS",
)
_SHA256_LENGTH = 64
_MAX_JSON_BYTES = 256 * 1024 * 1024
_STREAM_CHUNK_BYTES = 1024 * 1024


def _mapping(name: str, value: object) -> Mapping[str, Any]:
    if not isinstance(value, Mapping) or any(not isinstance(key, str) for key in value):
        raise TypeError(f"{name} must be an object with string keys")
    return value


def _sequence(name: str, value: object) -> Sequence[object]:
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence):
        raise TypeError(f"{name} must be an array")
    return value


def _exact(name: str, value: object, fields: Sequence[str]) -> Mapping[str, Any]:
    item = _mapping(name, value)
    expected = set(fields)
    actual = set(item)
    if actual != expected:
        missing = sorted(expected - actual)
        unexpected = sorted(actual - expected)
        raise ValueError(
            f"{name} has scope drift; missing={missing}, unexpected={unexpected}"
        )
    return item


def _text(name: str, value: object) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{name} must be non-empty text")
    if unicodedata.normalize("NFC", value) != value:
        raise ValueError(f"{name} must use NFC-normalized Unicode")
    return value.strip()


def _sha(name: str, value: object) -> str:
    result = _text(name, value)
    if (
        len(result) != _SHA256_LENGTH
        or result != result.lower()
        or any(character not in "0123456789abcdef" for character in result)
    ):
        raise ValueError(f"{name} must be a lowercase SHA-256 digest")
    return result


def _native_sha(name: str, value: object) -> str:
    """Accept native receipt hex casing while returning one canonical digest."""

    result = _text(name, value)
    normalized = result.lower()
    if (
        len(normalized) != _SHA256_LENGTH
        or any(character not in "0123456789abcdef" for character in normalized)
    ):
        raise ValueError(f"{name} must be a SHA-256 digest")
    return normalized


def _integer(name: str, value: object, *, minimum: int = 0) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value < minimum:
        raise ValueError(f"{name} must be an integer >= {minimum}")
    return value


def _finite(name: str, value: object) -> float:
    if isinstance(value, bool):
        raise TypeError(f"{name} must be numeric")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"{name} must be finite")
    return result


def _unit(name: str, value: object) -> float:
    result = _finite(name, value)
    if not 0.0 <= result <= 1.0:
        raise ValueError(f"{name} must be in [0, 1]")
    return result


def _literal(name: str, value: object, expected: object) -> None:
    if value != expected or type(value) is not type(expected):
        raise ValueError(f"{name} must be {expected!r}")


def _json_ready(value: Any) -> Any:
    if isinstance(value, Mapping):
        return {key: _json_ready(item) for key, item in value.items()}
    if isinstance(value, (tuple, list)):
        return [_json_ready(item) for item in value]
    return value


def _reject_json_constant(value: str) -> None:
    raise ValueError(f"JSON non-finite constant {value!r} is prohibited")


def _decode_json(name: str, data: bytes) -> Mapping[str, Any]:
    if len(data) > _MAX_JSON_BYTES:
        raise ValueError(f"{name} exceeds the {_MAX_JSON_BYTES}-byte JSON limit")
    if data.startswith(b"\xef\xbb\xbf"):
        raise ValueError(f"{name} must be UTF-8 without a byte-order mark")

    def reject_duplicates(pairs: list[tuple[str, object]]) -> dict[str, object]:
        result: dict[str, object] = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"{name} contains duplicate JSON key {key!r}")
            result[key] = value
        return result

    text = data.decode("utf-8", errors="strict")
    value = json.loads(
        text,
        object_pairs_hook=reject_duplicates,
        parse_constant=_reject_json_constant,
    )
    return _mapping(name, value)


def _contains_reparse_point(path: Path) -> bool:
    try:
        info = path.lstat()
    except OSError as exc:
        raise ValueError(f"unable to inspect evidence path {path}") from exc
    attributes = getattr(info, "st_file_attributes", 0)
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return path.is_symlink() or bool(attributes & reparse_flag)


def _normalized_os_path(path: Path) -> str:
    return os.path.normcase(os.path.abspath(os.fspath(path)))


def _assert_lexically_contained(root: Path, path: Path, name: str) -> Path:
    lexical = Path(os.path.abspath(os.fspath(path)))
    try:
        relative = lexical.relative_to(root)
    except ValueError as exc:
        raise ValueError(f"{name} escapes the approved evidence root") from exc
    current = root
    for part in relative.parts:
        current = current / part
        if _contains_reparse_point(current):
            raise ValueError(f"{name} traverses a symlink or reparse point: {current}")
    try:
        resolved = lexical.resolve(strict=True)
        resolved.relative_to(root)
    except (OSError, ValueError) as exc:
        raise ValueError(f"{name} escapes the approved evidence root") from exc
    if _normalized_os_path(resolved) != _normalized_os_path(lexical):
        raise ValueError(f"{name} resolves through a symlink or reparse point")
    return lexical


def _stat_record(value: os.stat_result) -> tuple[int, int, int, int, int]:
    return (
        value.st_dev,
        value.st_ino,
        value.st_size,
        value.st_mtime_ns,
        value.st_ctime_ns,
    )


def _stable_file_id(value: os.stat_result, path: Path) -> tuple[str, ...]:
    if value.st_ino:
        return ("FILE_ID", str(value.st_dev), str(value.st_ino))
    # A canonical path cannot distinguish hardlink aliases.  If the host does
    # not expose a stable file identifier, fail closed instead of weakening the
    # distinct-evidence-role guarantee.
    raise ValueError(f"filesystem does not expose a stable file identity for {path}")


def _open_handle_path(stream: object) -> Path | None:
    if os.name == "nt":
        try:
            import ctypes
            from ctypes import wintypes
            import msvcrt

            get_final_path = ctypes.windll.kernel32.GetFinalPathNameByHandleW
            get_final_path.argtypes = [
                wintypes.HANDLE,
                wintypes.LPWSTR,
                wintypes.DWORD,
                wintypes.DWORD,
            ]
            get_final_path.restype = wintypes.DWORD
            handle = msvcrt.get_osfhandle(stream.fileno())
            size = get_final_path(handle, None, 0, 0)
            if size == 0:
                raise OSError("GetFinalPathNameByHandleW failed")
            buffer = ctypes.create_unicode_buffer(size + 1)
            written = get_final_path(handle, buffer, len(buffer), 0)
            if written == 0 or written >= len(buffer):
                raise OSError("GetFinalPathNameByHandleW returned an invalid path")
            raw = buffer.value
            if raw.startswith("\\\\?\\UNC\\"):
                raw = "\\\\" + raw[8:]
            elif raw.startswith("\\\\?\\"):
                raw = raw[4:]
            return Path(os.path.abspath(raw))
        except (AttributeError, ImportError, OSError, ValueError) as exc:
            raise ValueError("unable to authenticate the opened Windows file path") from exc
    descriptor_link = Path(f"/proc/self/fd/{stream.fileno()}")
    if descriptor_link.exists():
        return Path(os.path.realpath(descriptor_link))
    return None


@dataclass(frozen=True, slots=True)
class FileSnapshot:
    path: Path
    byte_length: int
    sha256: str
    stable_identity: tuple[str, ...]
    data: bytes | None


def _snapshot_file(
    root: Path,
    candidate: Path,
    name: str,
    *,
    load_json_bytes: bool,
) -> FileSnapshot:
    path = _assert_lexically_contained(root, candidate, name)
    before_path = os.stat(path, follow_symlinks=False)
    if not stat.S_ISREG(before_path.st_mode):
        raise ValueError(f"{name} must resolve to a regular file")
    if load_json_bytes and before_path.st_size > _MAX_JSON_BYTES:
        raise ValueError(f"{name} exceeds the {_MAX_JSON_BYTES}-byte JSON limit")
    digest = hashlib.sha256()
    total = 0
    blocks: list[bytes] | None = [] if load_json_bytes else None
    with path.open("rb") as stream:
        handle_before = os.fstat(stream.fileno())
        opened_path = _open_handle_path(stream)
        if (
            opened_path is not None
            and _normalized_os_path(opened_path) != _normalized_os_path(path)
        ):
            raise ValueError(f"{name} opened through a symlink, junction, or alias")
        while True:
            block = stream.read(_STREAM_CHUNK_BYTES)
            if not block:
                break
            total += len(block)
            if load_json_bytes and total > _MAX_JSON_BYTES:
                raise ValueError(
                    f"{name} exceeds the {_MAX_JSON_BYTES}-byte JSON limit"
                )
            digest.update(block)
            if blocks is not None:
                blocks.append(block)
        handle_after = os.fstat(stream.fileno())
    path_after = _assert_lexically_contained(root, path, name)
    after_path = os.stat(path_after, follow_symlinks=False)
    identities = {
        _stat_record(before_path),
        _stat_record(handle_before),
        _stat_record(handle_after),
        _stat_record(after_path),
    }
    if len(identities) != 1 or total != handle_after.st_size:
        raise ValueError(f"{name} changed while it was being read")
    return FileSnapshot(
        path=path,
        byte_length=total,
        sha256=digest.hexdigest(),
        stable_identity=_stable_file_id(handle_after, path),
        data=None if blocks is None else b"".join(blocks),
    )


def _normalized_relative_path(name: str, value: object) -> PurePosixPath:
    raw = _text(name, value)
    if (
        raw.startswith(("/", "\\"))
        or "\\" in raw
        or ":" in raw
        or "%" in raw
        or any(ord(character) < 32 for character in raw)
    ):
        raise ValueError(f"{name} must be a normalized relative POSIX path")
    relative = PurePosixPath(raw)
    if str(relative) != raw or raw in (".", ""):
        raise ValueError(f"{name} must be a normalized relative POSIX path")
    if any(part in ("", ".", "..") for part in relative.parts):
        raise ValueError(f"{name} must not contain empty, dot, or parent components")
    return relative


@dataclass(frozen=True, slots=True)
class BoundFile:
    role: str
    relative_path: str
    resolved_path: Path
    byte_length: int
    sha256: str
    stable_identity: tuple[str, ...]
    data: bytes | None

    def receipt_row(self) -> dict[str, object]:
        return {
            "role": self.role,
            "relativePath": self.relative_path,
            "bytes": self.byte_length,
            "sha256": self.sha256,
        }


def _resolve_binding(
    root: Path,
    role: str,
    value: object,
    *,
    json_document: bool,
) -> BoundFile:
    binding = _exact(role, value, ("relativePath", "bytes", "sha256"))
    relative = _normalized_relative_path(
        f"{role}.relativePath", binding["relativePath"]
    )
    expected_bytes = _integer(f"{role}.bytes", binding["bytes"])
    expected_sha = _sha(f"{role}.sha256", binding["sha256"])
    unresolved = root.joinpath(*relative.parts)
    try:
        snapshot = _snapshot_file(
            root,
            unresolved,
            role,
            load_json_bytes=json_document,
        )
    except OSError as exc:
        raise ValueError(f"{role} does not resolve to an existing file") from exc
    if snapshot.byte_length != expected_bytes:
        raise ValueError(
            f"{role}.bytes mismatch: expected {expected_bytes}, "
            f"observed {snapshot.byte_length}"
        )
    if snapshot.sha256 != expected_sha:
        raise ValueError(
            f"{role}.sha256 mismatch: expected {expected_sha}, "
            f"observed {snapshot.sha256}"
        )
    return BoundFile(
        role=role,
        relative_path=relative.as_posix(),
        resolved_path=snapshot.path,
        byte_length=snapshot.byte_length,
        sha256=snapshot.sha256,
        stable_identity=snapshot.stable_identity,
        data=snapshot.data,
    )


def _resolve_unbound_json(root: Path, path: str | Path, role: str) -> BoundFile:
    candidate = Path(path)
    if not candidate.is_absolute():
        candidate = root / candidate
    try:
        snapshot = _snapshot_file(
            root,
            candidate,
            role,
            load_json_bytes=True,
        )
    except OSError as exc:
        raise ValueError(f"{role} does not resolve to an existing file") from exc
    return BoundFile(
        role=role,
        relative_path=snapshot.path.relative_to(root).as_posix(),
        resolved_path=snapshot.path,
        byte_length=snapshot.byte_length,
        sha256=snapshot.sha256,
        stable_identity=snapshot.stable_identity,
        data=snapshot.data,
    )


def _json_bytes(bound: BoundFile) -> bytes:
    if bound.data is None:
        raise ValueError(f"{bound.role} was not admitted as a bounded JSON document")
    return bound.data


def _verify_internal_digest(
    name: str, payload: Mapping[str, Any], digest_field: str
) -> str:
    digest = _sha(f"{name}.{digest_field}", payload.get(digest_field))
    body = _json_ready(payload)
    body.pop(digest_field, None)
    observed = stable_digest(body)
    if observed != digest:
        raise ValueError(
            f"{name}.{digest_field} does not match canonical artifact content"
        )
    return digest


def _assert_stage0_ready(contract: Stage0StudyContract) -> Mapping[str, Any]:
    payload = contract.to_dict()
    if payload["contractState"] != "FROZEN":
        raise ValueError("strict evidence intake requires a FROZEN Stage-0 contract")
    unresolved = sorted(
        item["artifactId"]
        for item in payload["artifactBindings"]
        if item["readiness"] != "VERIFIED"
    )
    if unresolved:
        raise ValueError(f"Stage-0 contains unverified artifacts: {unresolved}")
    blocked = sorted(
        item["gateId"]
        for item in payload["readiness"]["gates"]
        if item["status"] != "PASSED"
    )
    if blocked:
        raise ValueError(f"Stage-0 readiness gates are not passed: {blocked}")
    if not any(
        item["eligibleForCandidateGeneration"] is True
        for item in payload["mountRegionPolicy"]["regions"]
    ):
        raise ValueError("Stage-0 has no eligible mount region")
    pending = sorted(
        item["role"]
        for item in payload["governance"]["approvals"]
        if item["decision"] != "APPROVED"
    )
    if pending:
        raise ValueError(f"Stage-0 governance approvals are incomplete: {pending}")
    if payload["solverBenchmark"]["status"] != "FROZEN":
        raise ValueError("Stage-0 solver benchmark must be FROZEN")
    for index, hardware in enumerate(payload["hardwareInventory"]):
        for field in ("manufacturer", "model", "hardwareRevision"):
            value = _text(f"hardwareInventory[{index}].{field}", hardware[field])
            upper = value.upper()
            if "TBD" in upper or "UNSELECTED" in upper or upper.endswith("_EXAMPLE"):
                raise ValueError(
                    f"hardwareInventory[{index}].{field} remains a placeholder"
                )
    return payload


def _stage0_artifacts(
    root: Path,
    stage0_file: BoundFile,
    payload: Mapping[str, Any],
) -> dict[str, dict[str, object]]:
    result: dict[str, dict[str, object]] = {}
    stable_ids: dict[tuple[str, ...], str] = {}
    for index, raw in enumerate(_sequence("artifactBindings", payload.get("artifactBindings"))):
        item = _mapping(f"artifactBindings[{index}]", raw)
        artifact_id = _text(f"artifactBindings[{index}].artifactId", item["artifactId"])
        if artifact_id in result:
            raise ValueError(f"duplicate Stage-0 artifact ID {artifact_id!r}")
        _literal(
            f"artifactBindings[{index}].readiness",
            item.get("readiness"),
            "VERIFIED",
        )
        relative = _normalized_relative_path(
            f"artifactBindings[{index}].uri", item["uri"]
        )
        candidate = stage0_file.resolved_path.parent.joinpath(*relative.parts)
        try:
            snapshot = _snapshot_file(
                root,
                candidate,
                f"Stage-0 artifact {artifact_id!r}",
                load_json_bytes=False,
            )
        except OSError as exc:
            raise ValueError(
                f"Stage-0 artifact {artifact_id!r} does not resolve"
            ) from exc
        expected = _sha(f"artifactBindings[{index}].sha256", item["sha256"])
        if snapshot.sha256 != expected:
            raise ValueError(
                f"Stage-0 artifact {artifact_id!r} changed after contract validation"
            )
        previous = stable_ids.get(snapshot.stable_identity)
        if previous is not None:
            raise ValueError(
                f"Stage-0 artifacts {previous!r} and {artifact_id!r} are hardlink/file aliases"
            )
        stable_ids[snapshot.stable_identity] = artifact_id
        result[artifact_id] = {
            "role": item["role"],
            "path": snapshot.path,
            "sha256": snapshot.sha256,
            "bytes": snapshot.byte_length,
            "stableIdentity": snapshot.stable_identity,
        }
    return result


def _require_stage0_file(
    name: str,
    bound: BoundFile,
    artifact_id: str,
    expected_role: str,
    artifacts: Mapping[str, Mapping[str, object]],
) -> None:
    if artifact_id not in artifacts:
        raise ValueError(f"{name} references unknown Stage-0 artifact {artifact_id!r}")
    artifact = artifacts[artifact_id]
    if artifact["role"] != expected_role:
        raise ValueError(
            f"{name} Stage-0 artifact role must be {expected_role!r}, "
            f"got {artifact['role']!r}"
        )
    if (
        artifact["path"] != bound.resolved_path
        or artifact["sha256"] != bound.sha256
        or artifact["bytes"] != bound.byte_length
        or artifact["stableIdentity"] != bound.stable_identity
    ):
        raise ValueError(f"{name} does not match its Stage-0 artifact bytes")


def _reject_unauthorized_cross_set_aliases(
    manifest_files: Sequence[BoundFile],
    artifacts: Mapping[str, Mapping[str, object]],
    authorized_artifact_by_manifest_role: Mapping[str, str],
) -> None:
    """Reject physical aliases except the manifest-to-Stage-0 bindings we expect."""

    artifact_id_by_identity = {
        tuple(item["stableIdentity"]): artifact_id
        for artifact_id, item in artifacts.items()
    }
    for bound in manifest_files:
        aliased_artifact_id = artifact_id_by_identity.get(bound.stable_identity)
        if aliased_artifact_id is None:
            continue
        expected_artifact_id = authorized_artifact_by_manifest_role.get(bound.role)
        if aliased_artifact_id != expected_artifact_id:
            raise ValueError(
                f"manifest role {bound.role!r} is a hardlink/file alias of unrelated "
                f"Stage-0 artifact {aliased_artifact_id!r}"
            )


def _validate_native_rf_receipt(
    root: Path,
    bound: BoundFile,
    stage0_payload: Mapping[str, Any],
    artifacts: Mapping[str, Mapping[str, object]],
) -> str:
    receipt_artifacts = [
        item for item in artifacts.values() if item["role"] == "RF_NATIVE_TEST_RECEIPT"
    ]
    if len(receipt_artifacts) != 1:
        raise ValueError(
            "frozen Stage-0 must bind exactly one RF_NATIVE_TEST_RECEIPT artifact"
        )
    receipt_artifact = receipt_artifacts[0]
    if (
        receipt_artifact["path"] != bound.resolved_path
        or receipt_artifact["sha256"] != bound.sha256
        or receipt_artifact["bytes"] != bound.byte_length
        or receipt_artifact["stableIdentity"] != bound.stable_identity
    ):
        raise ValueError(
            "native RF PASS receipt is not authenticated by the frozen Stage-0 bytes"
        )
    receipt = _decode_json(bound.role, _json_bytes(bound))
    _literal("nativeRfReceipt.Schema", receipt.get("Schema"), NATIVE_RF_RECEIPT_SCHEMA)
    _literal("nativeRfReceipt.Status", receipt.get("Status"), "PASS")
    run_token = _text("nativeRfReceipt.RunToken", receipt.get("RunToken"))
    _literal(
        "nativeRfReceipt.ExactAutomationSuccessCount",
        receipt.get("ExactAutomationSuccessCount"),
        1,
    )
    _literal("nativeRfReceipt.NativeProjectIdleAfter", receipt.get("NativeProjectIdleAfter"), True)
    _literal("nativeRfReceipt.FailureRollbackArmed", receipt.get("FailureRollbackArmed"), True)
    automation = _mapping("nativeRfReceipt.Automation", receipt.get("Automation"))
    _literal("nativeRfReceipt.Automation.Status", automation.get("Status"), "PASS")
    _literal(
        "nativeRfReceipt.Automation.Filter",
        automation.get("Filter"),
        NATIVE_RF_AUTOMATION_FILTER,
    )
    _literal("nativeRfReceipt.Automation.ExitCode", automation.get("ExitCode"), 0)
    _literal(
        "nativeRfReceipt.Automation.ExactFilterSuccessfulCompletions",
        automation.get("ExactFilterSuccessfulCompletions"),
        1,
    )
    _literal(
        "nativeRfReceipt.Automation.FailureCompletions",
        automation.get("FailureCompletions"),
        0,
    )

    resource_rows = _sequence(
        "nativeRfReceipt.WorkspaceResources", receipt.get("WorkspaceResources")
    )
    if len(resource_rows) != 3:
        raise ValueError("native RF receipt must bind exactly geometry/catalog/scene resources")
    resources: list[BoundFile] = []
    resource_ids: set[tuple[str, ...]] = set()
    for index, raw in enumerate(resource_rows):
        item = _mapping(f"nativeRfReceipt.WorkspaceResources[{index}]", raw)
        _literal(
            f"nativeRfReceipt.WorkspaceResources[{index}].Present",
            item.get("Present"),
            True,
        )
        raw_path = _text(
            f"nativeRfReceipt.WorkspaceResources[{index}].Path", item.get("Path")
        )
        path = Path(raw_path)
        if not path.is_absolute():
            relative = _normalized_relative_path(
                f"nativeRfReceipt.WorkspaceResources[{index}].Path", raw_path
            )
            path = root.joinpath(*relative.parts)
        try:
            snapshot = _snapshot_file(
                root,
                path,
                f"nativeRfReceipt.WorkspaceResources[{index}]",
                load_json_bytes=False,
            )
        except OSError as exc:
            raise ValueError("native RF receipt resource does not resolve") from exc
        expected_bytes = _integer(
            f"nativeRfReceipt.WorkspaceResources[{index}].Bytes", item.get("Bytes")
        )
        expected_sha = _native_sha(
            f"nativeRfReceipt.WorkspaceResources[{index}].Sha256", item.get("Sha256")
        )
        if snapshot.byte_length != expected_bytes or snapshot.sha256 != expected_sha:
            raise ValueError("native RF receipt resource bytes no longer match its receipt")
        if snapshot.stable_identity in resource_ids:
            raise ValueError("native RF workspace resources must be distinct files")
        resource_ids.add(snapshot.stable_identity)
        resources.append(
            BoundFile(
                role=f"nativeRfWorkspaceResource[{index}]",
                relative_path=snapshot.path.relative_to(root).as_posix(),
                resolved_path=snapshot.path,
                byte_length=snapshot.byte_length,
                sha256=snapshot.sha256,
                stable_identity=snapshot.stable_identity,
                data=None,
            )
        )

    artifact_by_role = {
        item["role"]: item for item in artifacts.values()
    }
    for role in ("RF_GEOMETRY", "RF_MATERIAL_CATALOG"):
        target = artifact_by_role[role]
        matches = [
            item
            for item in resources
            if item.resolved_path == target["path"] and item.sha256 == target["sha256"]
        ]
        if len(matches) != 1:
            raise ValueError(
                f"native RF receipt does not cross-bind the Stage-0 {role} artifact"
            )
    scene_resources = [
        item for item in resources if item.resolved_path.name.lower().endswith(".contract.json")
    ]
    if len(scene_resources) != 1:
        raise ValueError("native RF receipt must include exactly one scene-contract resource")
    return run_token


def _validate_calibration_bundle(
    bound: BoundFile,
    datasets: Mapping[str, BoundFile],
    hardware_by_id: Mapping[str, Mapping[str, Any]],
    expected_hardware_ids: set[str],
) -> dict[str, str]:
    payload = _exact(
        "calibrationBundle",
        _decode_json(bound.role, _json_bytes(bound)),
        (
            "schemaVersion",
            "calibrations",
            "allModelsMeasured",
            "scoreSemantics",
            "measuredCalibrationEvidenceBound",
            "fieldPerformanceClaimed",
            "physicalDeploymentAuthorized",
            "bundleSha256",
        ),
    )
    _literal(
        "calibrationBundle.schemaVersion",
        payload["schemaVersion"],
        MODEL_CALIBRATION_BUNDLE_SCHEMA,
    )
    _verify_internal_digest("calibrationBundle", payload, "bundleSha256")
    _literal("calibrationBundle.allModelsMeasured", payload["allModelsMeasured"], True)
    _literal(
        "calibrationBundle.scoreSemantics",
        payload["scoreSemantics"],
        STRICT_SCORE_SEMANTICS,
    )
    _literal(
        "calibrationBundle.measuredCalibrationEvidenceBound",
        payload["measuredCalibrationEvidenceBound"],
        True,
    )
    _literal("calibrationBundle.fieldPerformanceClaimed", payload["fieldPerformanceClaimed"], False)
    _literal(
        "calibrationBundle.physicalDeploymentAuthorized",
        payload["physicalDeploymentAuthorized"],
        False,
    )
    rows = _sequence("calibrationBundle.calibrations", payload["calibrations"])
    calibration_ids: dict[str, str] = {}
    seen_calibration_ids: set[str] = set()
    for index, raw in enumerate(rows):
        row = _exact(
            f"calibrationBundle.calibrations[{index}]",
            raw,
            (
                "calibrationId",
                "sensorClass",
                "hardwareId",
                "hardwareRevision",
                "evidenceClass",
                "evidenceFamily",
                "parameters",
                "datasetSha256",
                "limitations",
            ),
        )
        parameters = _exact(
            f"calibrationBundle.calibrations[{index}].parameters",
            row["parameters"],
            (
                "maximumRangeMeters",
                "minimumQuality",
                "horizontalFovDegrees",
                "rangeExponent",
            ),
        )
        sensor_class = _text(
            f"calibrationBundle.calibrations[{index}].sensorClass",
            row["sensorClass"],
        )
        hardware_id = _text(
            f"calibrationBundle.calibrations[{index}].hardwareId",
            row["hardwareId"],
        )
        if (
            hardware_id in calibration_ids
            or hardware_id not in hardware_by_id
            or hardware_id not in expected_hardware_ids
        ):
            raise ValueError(
                "calibration bundle hardware IDs must be candidate-referenced, "
                "known, and unique"
            )
        hardware = hardware_by_id[hardware_id]
        if sensor_class not in SENSOR_CLASSES or hardware["sensorClass"] != sensor_class:
            raise ValueError("calibration sensor class does not match its hardware")
        _literal(
            "calibration hardwareRevision",
            row["hardwareRevision"],
            hardware["hardwareRevision"],
        )
        if row["evidenceFamily"] != SENSOR_FAMILIES[sensor_class]:
            raise ValueError("calibration evidenceFamily does not match sensorClass")
        dataset_sha = _sha(
            f"calibrationBundle.calibrations[{index}].datasetSha256",
            row["datasetSha256"],
        )
        if dataset_sha != datasets[hardware_id].sha256:
            raise ValueError(
                f"calibration dataset hash for hardware {hardware_id!r} "
                "does not match actual bytes"
            )
        limitations = tuple(
            _text(f"calibration limitations[{item_index}]", item)
            for item_index, item in enumerate(
                _sequence("calibration limitations", row["limitations"])
            )
        )
        model = SensorModelCalibration(
            calibration_id=_text("calibrationId", row["calibrationId"]),
            sensor_class=sensor_class,
            evidence_class=_text("evidenceClass", row["evidenceClass"]),
            maximum_range_meters=parameters["maximumRangeMeters"],
            minimum_quality=parameters["minimumQuality"],
            horizontal_fov_degrees=parameters["horizontalFovDegrees"],
            range_exponent=parameters["rangeExponent"],
            dataset_sha256=dataset_sha,
            limitations=limitations,
            hardware_id=hardware_id,
            hardware_revision=row["hardwareRevision"],
        )
        if model.evidence_class != MEASURED_CALIBRATION_EVIDENCE:
            raise ValueError("strict intake accepts only measured calibration records")
        if model.calibration_id in seen_calibration_ids:
            raise ValueError("calibration bundle calibration IDs must be unique")
        seen_calibration_ids.add(model.calibration_id)
        calibration_ids[hardware_id] = model.calibration_id
    if set(calibration_ids) != expected_hardware_ids:
        raise ValueError(
            "calibration bundle must contain exactly the candidate-referenced hardware IDs"
        )
    return calibration_ids


def _validate_candidate_catalog(
    bound: BoundFile,
    stage0: Mapping[str, Any],
) -> list[dict[str, Any]]:
    payload = _exact(
        "candidateCatalog",
        _decode_json(bound.role, _json_bytes(bound)),
        ("schemaVersion", "candidates", "artifactSha256"),
    )
    _literal(
        "candidateCatalog.schemaVersion", payload["schemaVersion"], CANDIDATE_CATALOG_SCHEMA
    )
    _verify_internal_digest("candidateCatalog", payload, "artifactSha256")
    regions = {
        item["regionId"]: item
        for item in stage0["mountRegionPolicy"]["regions"]
        if item["eligibleForCandidateGeneration"] is True
    }
    hardware = {
        item["hardwareId"]: item for item in stage0["hardwareInventory"]
    }
    result: list[dict[str, Any]] = []
    seen: set[str] = set()
    for index, raw in enumerate(_sequence("candidateCatalog.candidates", payload["candidates"])):
        row = _exact(
            f"candidateCatalog.candidates[{index}]",
            raw,
            (
                "candidateId",
                "siteId",
                "mountRegionId",
                "failureDomainId",
                "poseEnuMeters",
                "heightAglMeters",
                "yawDegrees",
                "sensorClasses",
                "hardwareIdsBySensorClass",
                "hardwareRevisionsBySensorClass",
                "costUnits",
            ),
        )
        candidate_id = _text("candidateId", row["candidateId"])
        if candidate_id in seen:
            raise ValueError(f"duplicate candidateId {candidate_id!r}")
        seen.add(candidate_id)
        region_id = _text("mountRegionId", row["mountRegionId"])
        if region_id not in regions:
            raise ValueError(f"candidate {candidate_id!r} uses an ineligible mount region")
        region = regions[region_id]
        failure_domain = _text("failureDomainId", row["failureDomainId"])
        if failure_domain != region["resources"]["failureDomainId"]:
            raise ValueError(f"candidate {candidate_id!r} failure domain does not match its mount")
        pose = _sequence("poseEnuMeters", row["poseEnuMeters"])
        if len(pose) != 3:
            raise ValueError("poseEnuMeters must have exactly three coordinates")
        [_finite(f"poseEnuMeters[{axis}]", value) for axis, value in enumerate(pose)]
        height = _finite("heightAglMeters", row["heightAglMeters"])
        height_range = region["heightAglMeters"]
        if not height_range["minimum"] <= height <= height_range["maximum"]:
            raise ValueError(f"candidate {candidate_id!r} height is outside its mount range")
        yaw = _finite("yawDegrees", row["yawDegrees"])
        azimuth = region["azimuthDegrees"]
        if not azimuth["minimum"] <= yaw <= azimuth["maximum"]:
            raise ValueError(f"candidate {candidate_id!r} yaw is outside its mount range")
        classes = tuple(
            _text("sensorClasses[]", item)
            for item in _sequence("sensorClasses", row["sensorClasses"])
        )
        if not classes or len(set(classes)) != len(classes):
            raise ValueError("sensorClasses must be non-empty and unique")
        if not set(classes) <= set(region["allowedSensorClasses"]):
            raise ValueError(f"candidate {candidate_id!r} uses a disallowed sensor class")
        hardware_ids = _mapping("hardwareIdsBySensorClass", row["hardwareIdsBySensorClass"])
        if set(hardware_ids) != set(classes):
            raise ValueError("hardwareIdsBySensorClass must exactly match sensorClasses")
        hardware_revisions = _mapping(
            "hardwareRevisionsBySensorClass",
            row["hardwareRevisionsBySensorClass"],
        )
        if set(hardware_revisions) != set(classes):
            raise ValueError(
                "hardwareRevisionsBySensorClass must exactly match sensorClasses"
            )
        for sensor_class, hardware_id_raw in hardware_ids.items():
            hardware_id = _text("hardwareId", hardware_id_raw)
            if hardware_id not in hardware:
                raise ValueError(f"candidate references unknown hardware {hardware_id!r}")
            if hardware[hardware_id]["sensorClass"] != sensor_class:
                raise ValueError("candidate hardware sensor class mismatch")
            _literal(
                "candidate hardware revision",
                hardware_revisions[sensor_class],
                hardware[hardware_id]["hardwareRevision"],
            )
        cost = _finite("costUnits", row["costUnits"])
        if cost < 0.0:
            raise ValueError("costUnits must be >= 0")
        result.append(_json_ready(row))
    if not result:
        raise ValueError("candidate catalog must not be empty")
    if [item["candidateId"] for item in result] != sorted(seen):
        raise ValueError("candidate catalog must be sorted by candidateId")
    return result


def _validate_scenario_partitions(
    bound_by_partition: Mapping[str, BoundFile],
    stage0: Mapping[str, Any],
) -> dict[str, str]:
    declared = stage0["scenarioCorpus"]["partitions"]
    required_dimensions = set(stage0["scenarioCorpus"]["requiredDimensions"])
    common_seeds = list(stage0["scenarioCorpus"]["commonRandomSeeds"])
    scenario_to_partition: dict[str, str] = {}
    for partition_name, declaration in declared.items():
        partition_id = declaration["partitionId"]
        bound = bound_by_partition[partition_id]
        payload = _exact(
            f"scenarioPartition[{partition_id}]",
            _decode_json(bound.role, _json_bytes(bound)),
            (
                "schemaVersion",
                "partitionId",
                "purpose",
                "commonRandomSeeds",
                "scenarios",
                "artifactSha256",
            ),
        )
        _literal(
            f"scenarioPartition[{partition_id}].schemaVersion",
            payload["schemaVersion"],
            SCENARIO_PARTITION_SCHEMA,
        )
        _verify_internal_digest(
            f"scenarioPartition[{partition_id}]", payload, "artifactSha256"
        )
        _literal(f"scenarioPartition[{partition_id}].partitionId", payload["partitionId"], partition_id)
        _literal(f"scenarioPartition[{partition_id}].purpose", payload["purpose"], declaration["purpose"])
        if payload["commonRandomSeeds"] != common_seeds:
            raise ValueError(f"scenario partition {partition_id!r} seed schedule drift")
        rows = _sequence(f"scenarioPartition[{partition_id}].scenarios", payload["scenarios"])
        ids: list[str] = []
        for index, raw in enumerate(rows):
            row = _exact(
                f"scenarioPartition[{partition_id}].scenarios[{index}]",
                raw,
                ("scenarioId", "dimensionValues"),
            )
            scenario_id = _text("scenarioId", row["scenarioId"])
            dimensions = _mapping("dimensionValues", row["dimensionValues"])
            if set(dimensions) != required_dimensions:
                raise ValueError(f"scenario {scenario_id!r} does not cover required dimensions")
            for dimension, value in dimensions.items():
                _text(f"scenario {scenario_id}.{dimension}", value)
            if scenario_id in scenario_to_partition:
                raise ValueError(f"duplicate scenarioId {scenario_id!r}")
            scenario_to_partition[scenario_id] = partition_id
            ids.append(scenario_id)
        if ids != declaration["scenarioIds"]:
            raise ValueError(f"scenario partition {partition_id!r} IDs/order drift")
    return scenario_to_partition


def _validate_trajectory_corpus(
    bound: BoundFile,
    stage0: Mapping[str, Any],
    scenario_to_partition: Mapping[str, str],
) -> list[dict[str, Any]]:
    payload = _exact(
        "trajectoryCorpus",
        _decode_json(bound.role, _json_bytes(bound)),
        ("schemaVersion", "coordinateFrame", "samplePeriodSeconds", "trajectories", "artifactSha256"),
    )
    _literal("trajectoryCorpus.schemaVersion", payload["schemaVersion"], TRAJECTORY_CORPUS_SCHEMA)
    _verify_internal_digest("trajectoryCorpus", payload, "artifactSha256")
    declaration = stage0["targetAndTrajectoryEnvelope"]["trajectoryCorpus"]
    _literal("trajectoryCorpus.coordinateFrame", payload["coordinateFrame"], declaration["coordinateFrame"])
    sample_period = _finite("trajectoryCorpus.samplePeriodSeconds", payload["samplePeriodSeconds"])
    if not math.isclose(sample_period, declaration["samplePeriodSeconds"], rel_tol=0.0, abs_tol=1e-12):
        raise ValueError("trajectory corpus sample period does not match Stage-0")
    target_ids = {
        item["targetClassId"]
        for item in stage0["targetAndTrajectoryEnvelope"]["targetClasses"]
    }
    critical_ids = set(stage0["requirements"]["criticalTrajectoryIds"])
    horizons = declaration["timeHorizonSeconds"]
    rows_out: list[dict[str, Any]] = []
    seen: set[str] = set()
    for index, raw in enumerate(_sequence("trajectoryCorpus.trajectories", payload["trajectories"])):
        row = _exact(
            f"trajectoryCorpus.trajectories[{index}]",
            raw,
            (
                "trajectoryId",
                "scenarioId",
                "partitionId",
                "targetClassId",
                "critical",
                "rfEmitting",
                "points",
            ),
        )
        trajectory_id = _text("trajectoryId", row["trajectoryId"])
        if trajectory_id in seen:
            raise ValueError(f"duplicate trajectoryId {trajectory_id!r}")
        seen.add(trajectory_id)
        scenario_id = _text("scenarioId", row["scenarioId"])
        if scenario_id not in scenario_to_partition:
            raise ValueError(f"trajectory {trajectory_id!r} references an unknown scenario")
        if row["partitionId"] != scenario_to_partition[scenario_id]:
            raise ValueError(f"trajectory {trajectory_id!r} partition does not match its scenario")
        if row["targetClassId"] not in target_ids:
            raise ValueError(f"trajectory {trajectory_id!r} references an unknown target class")
        _literal("trajectory.critical", row["critical"], trajectory_id in critical_ids)
        if not isinstance(row["rfEmitting"], bool):
            raise TypeError("trajectory.rfEmitting must be boolean")
        points = _sequence("trajectory.points", row["points"])
        if len(points) < 2:
            raise ValueError("trajectory must contain at least two points")
        times: list[float] = []
        for point_index, raw_point in enumerate(points):
            point = _exact(
                f"trajectory.points[{point_index}]", raw_point, ("timeSeconds", "enuMeters")
            )
            time = _finite("trajectory point timeSeconds", point["timeSeconds"])
            coordinates = _sequence("trajectory point enuMeters", point["enuMeters"])
            if len(coordinates) != 3:
                raise ValueError("trajectory point enuMeters must have three coordinates")
            [_finite("trajectory coordinate", value) for value in coordinates]
            times.append(time)
        if not math.isclose(times[0], 0.0, rel_tol=0.0, abs_tol=1e-12):
            raise ValueError("trajectory timestamps must start at zero")
        if any(
            not math.isclose(right - left, sample_period, rel_tol=0.0, abs_tol=1e-9)
            for left, right in zip(times, times[1:])
        ):
            raise ValueError("trajectory timestamps must follow the frozen sample period")
        duration = times[-1]
        if not horizons["minimum"] <= duration <= horizons["maximum"]:
            raise ValueError("trajectory duration lies outside the Stage-0 horizon")
        rows_out.append(_json_ready(row))
    expected_ids = list(declaration["trajectoryIds"])
    if [row["trajectoryId"] for row in rows_out] != expected_ids:
        raise ValueError("trajectory corpus IDs/order do not match Stage-0")
    return rows_out


def evaluator_result_rows_sha256(
    rows: Sequence[Mapping[str, Any]],
) -> str:
    """Digest the exact non-circular evaluator result rows in canonical order."""

    normalized: list[dict[str, Any]] = []
    keys: list[tuple[str, str, str]] = []
    expected_fields = set(EVALUATOR_RESULT_ROW_FIELDS)
    for index, raw in enumerate(rows):
        row = _mapping(f"evaluatorResultRows[{index}]", raw)
        if set(row) != expected_fields:
            raise ValueError(
                f"evaluatorResultRows[{index}] has scope drift; "
                f"missing={sorted(expected_fields - set(row))}, "
                f"unexpected={sorted(set(row) - expected_fields)}"
            )
        normalized.append({field: _json_ready(row[field]) for field in EVALUATOR_RESULT_ROW_FIELDS})
        keys.append(
            (
                _text("candidateId", row["candidateId"]),
                _text("trajectoryId", row["trajectoryId"]),
                _text("sensorClass", row["sensorClass"]),
            )
        )
    if not normalized or keys != sorted(keys) or len(keys) != len(set(keys)):
        raise ValueError("evaluator result rows must be non-empty, unique, and sorted")
    return stable_digest(normalized)


def _validate_evaluation_evidence(
    files: Mapping[str, BoundFile],
    *,
    hardware_by_id: Mapping[str, Mapping[str, Any]],
    calibration_ids: Mapping[str, str],
    stage0_contract_sha256: str,
    native_rf_receipt_sha256: str,
    calibration_bundle_sha256: str,
    dataset_files: Mapping[str, BoundFile],
    candidate_catalog_sha256: str,
    trajectory_corpus_sha256: str,
    scenario_files: Mapping[str, BoundFile],
) -> dict[str, tuple[int, str]]:
    scenario_hashes = {
        key: scenario_files[key].sha256 for key in sorted(scenario_files)
    }
    result_bindings: dict[str, tuple[int, str]] = {}
    for hardware_id in sorted(files):
        hardware = hardware_by_id[hardware_id]
        sensor_class = hardware["sensorClass"]
        payload = _exact(
            f"evaluationEvidence[{hardware_id}]",
            _decode_json(files[hardware_id].role, _json_bytes(files[hardware_id])),
            (
                "schemaVersion",
                "sensorClass",
                "hardwareId",
                "hardwareRevision",
                "calibrationId",
                "evidenceClass",
                "stage0ContractSha256",
                "nativeRfReceiptFileSha256",
                "calibrationBundleFileSha256",
                "calibrationDatasetFileSha256",
                "candidateCatalogFileSha256",
                "trajectoryCorpusFileSha256",
                "scenarioPartitionFileSha256ById",
                "exactSimulatorReplayComplete",
                "resultRowCount",
                "resultRowsSha256",
                "fieldPerformanceClaimed",
                "physicalDeploymentAuthorized",
                "artifactSha256",
            ),
        )
        _literal(
            f"evaluationEvidence[{hardware_id}].schemaVersion",
            payload["schemaVersion"],
            EVALUATOR_EVIDENCE_SCHEMA,
        )
        _verify_internal_digest(
            f"evaluationEvidence[{hardware_id}]", payload, "artifactSha256"
        )
        _literal(
            f"evaluationEvidence[{hardware_id}].sensorClass",
            payload["sensorClass"],
            sensor_class,
        )
        _literal(
            f"evaluationEvidence[{hardware_id}].hardwareId",
            payload["hardwareId"],
            hardware_id,
        )
        _literal(
            f"evaluationEvidence[{hardware_id}].hardwareRevision",
            payload["hardwareRevision"],
            hardware["hardwareRevision"],
        )
        _literal(
            f"evaluationEvidence[{hardware_id}].calibrationId",
            payload["calibrationId"],
            calibration_ids[hardware_id],
        )
        _literal(
            f"evaluationEvidence[{hardware_id}].evidenceClass",
            payload["evidenceClass"],
            EXACT_SIMULATOR_REPLAY_SOURCE,
        )
        expected = {
            "stage0ContractSha256": stage0_contract_sha256,
            "nativeRfReceiptFileSha256": native_rf_receipt_sha256,
            "calibrationBundleFileSha256": calibration_bundle_sha256,
            "calibrationDatasetFileSha256": dataset_files[hardware_id].sha256,
            "candidateCatalogFileSha256": candidate_catalog_sha256,
            "trajectoryCorpusFileSha256": trajectory_corpus_sha256,
            "scenarioPartitionFileSha256ById": scenario_hashes,
        }
        for field, value in expected.items():
            _literal(f"evaluationEvidence[{hardware_id}].{field}", payload[field], value)
        _literal(
            f"evaluationEvidence[{hardware_id}].exactSimulatorReplayComplete",
            payload["exactSimulatorReplayComplete"],
            True,
        )
        _literal(
            f"evaluationEvidence[{hardware_id}].fieldPerformanceClaimed",
            payload["fieldPerformanceClaimed"],
            False,
        )
        _literal(
            f"evaluationEvidence[{hardware_id}].physicalDeploymentAuthorized",
            payload["physicalDeploymentAuthorized"],
            False,
        )
        result_count = _integer(
            f"evaluationEvidence[{hardware_id}].resultRowCount",
            payload["resultRowCount"],
            minimum=1,
        )
        result_digest = _sha(
            f"evaluationEvidence[{hardware_id}].resultRowsSha256",
            payload["resultRowsSha256"],
        )
        result_bindings[hardware_id] = (result_count, result_digest)
    return result_bindings


def _validate_precompute(
    bound: BoundFile,
    *,
    expected_bindings: Mapping[str, object],
    candidates: Sequence[Mapping[str, Any]],
    trajectories: Sequence[Mapping[str, Any]],
    calibration_ids: Mapping[str, str],
    evaluation_files: Mapping[str, BoundFile],
    evaluator_result_bindings: Mapping[str, tuple[int, str]],
) -> Mapping[str, Any]:
    payload = _decode_json(bound.role, _json_bytes(bound))
    validate_precompute_truth_contract(payload)
    _verify_internal_digest("precompute", payload, "artifactSha256")
    if payload["bindings"] != expected_bindings:
        raise ValueError("precompute bindings do not exactly match the admitted evidence files")
    if payload["candidates"] != list(candidates):
        raise ValueError("precompute candidates do not exactly match the candidate catalog")
    if payload["trajectories"] != list(trajectories):
        raise ValueError("precompute trajectories do not exactly match the trajectory corpus")

    candidate_by_id = {row["candidateId"]: row for row in candidates}
    trajectory_by_id = {row["trajectoryId"]: row for row in trajectories}
    expected_keys = {
        (candidate["candidateId"], trajectory["trajectoryId"], sensor_class)
        for candidate in candidates
        for trajectory in trajectories
        for sensor_class in candidate["sensorClasses"]
    }
    observed_keys: list[tuple[str, str, str]] = []
    result_rows_by_hardware: dict[str, list[dict[str, Any]]] = {
        hardware_id: [] for hardware_id in evaluation_files
    }
    for index, raw in enumerate(payload["results"]):
        row = _mapping(f"precompute.results[{index}]", raw)
        candidate_id = _text("candidateId", row["candidateId"])
        trajectory_id = _text("trajectoryId", row["trajectoryId"])
        sensor_class = _text("sensorClass", row["sensorClass"])
        key = (candidate_id, trajectory_id, sensor_class)
        if candidate_id not in candidate_by_id or trajectory_id not in trajectory_by_id:
            raise ValueError("precompute row references an unknown candidate or trajectory")
        candidate = candidate_by_id[candidate_id]
        trajectory = trajectory_by_id[trajectory_id]
        if sensor_class not in candidate["sensorClasses"]:
            raise ValueError("precompute row sensor is absent from its candidate")
        _literal("precompute row scenarioId", row["scenarioId"], trajectory["scenarioId"])
        _literal("precompute row partitionId", row["partitionId"], trajectory["partitionId"])
        _literal(
            "precompute row evidenceFamily", row["evidenceFamily"], SENSOR_FAMILIES[sensor_class]
        )
        _literal(
            "precompute row hardwareId",
            row["hardwareId"],
            candidate["hardwareIdsBySensorClass"][sensor_class],
        )
        hardware_id = row["hardwareId"]
        _literal(
            "precompute row hardwareRevision",
            row["hardwareRevision"],
            candidate["hardwareRevisionsBySensorClass"][sensor_class],
        )
        _literal(
            "precompute row calibrationId",
            row["calibrationId"],
            calibration_ids[hardware_id],
        )
        _literal(
            "precompute row evaluatorEvidenceFileSha256",
            row["evaluatorEvidenceFileSha256"],
            evaluation_files[hardware_id].sha256,
        )
        if sensor_class == "passive_rf" and trajectory["rfEmitting"] is False:
            if _unit("silent-target passive-RF detectionFraction", row["detectionFraction"]) != 0.0:
                raise ValueError("RF-silent trajectories must retain an explicit zero passive-RF row")
            if row["firstDetectionSeconds"] is not None:
                raise ValueError("RF-silent passive-RF rows cannot declare a first detection")
        first = row["firstDetectionSeconds"]
        if first is not None:
            point_times = [point["timeSeconds"] for point in trajectory["points"]]
            if not any(
                math.isclose(float(first), float(item), rel_tol=0.0, abs_tol=1e-9)
                for item in point_times
            ):
                raise ValueError("firstDetectionSeconds is not a trajectory sample time")
        result_rows_by_hardware[hardware_id].append(
            {field: _json_ready(row[field]) for field in EVALUATOR_RESULT_ROW_FIELDS}
        )
        observed_keys.append(key)
    if observed_keys != sorted(observed_keys):
        raise ValueError("precompute rows must be lexicographically sorted")
    if len(observed_keys) != len(set(observed_keys)):
        raise ValueError("precompute contains duplicate candidate/trajectory/sensor rows")
    if set(observed_keys) != expected_keys:
        missing = sorted(expected_keys - set(observed_keys))
        extra = sorted(set(observed_keys) - expected_keys)
        raise ValueError(f"precompute row grid mismatch; missing={missing}, extra={extra}")
    for hardware_id, rows in result_rows_by_hardware.items():
        expected_count, expected_digest = evaluator_result_bindings[hardware_id]
        if len(rows) != expected_count:
            raise ValueError(
                f"precompute result count for hardware {hardware_id!r} does not "
                "match its evaluator receipt"
            )
        observed_digest = evaluator_result_rows_sha256(rows)
        if observed_digest != expected_digest:
            raise ValueError(
                f"precompute result rows for hardware {hardware_id!r} do not "
                "match the evaluator-bound output digest"
            )
    return payload


def validate_evidence_intake(
    manifest_path: str | Path,
    approved_root: str | Path,
) -> dict[str, object]:
    """Validate one complete file-backed evidence package and return a receipt.

    Successful validation means only that the structural package and its raw
    detection-row provenance are internally consistent.  Robust fused-outcome
    semantics remain outside this schema, so Stage-1 review, solver execution,
    and physical deployment stay unauthorized.
    """

    try:
        root = Path(approved_root).resolve(strict=True)
    except OSError as exc:
        raise ValueError("approved evidence root does not exist") from exc
    if not root.is_dir():
        raise ValueError("approved evidence root must be a directory")
    manifest_file = _resolve_unbound_json(root, manifest_path, "manifest")
    manifest = _exact(
        "manifest",
        _decode_json("manifest", _json_bytes(manifest_file)),
        (
            "schemaVersion",
            "intakeId",
            "executionClass",
            "stage0Contract",
            "nativeRfReceipt",
            "calibrationBundle",
            "calibrationDatasets",
            "candidateCatalog",
            "trajectoryCorpus",
            "scenarioPartitions",
            "evaluationEvidence",
            "precompute",
            "stage1AuthorizationPresented",
        ),
    )
    _literal("manifest.schemaVersion", manifest["schemaVersion"], EVIDENCE_INTAKE_MANIFEST_SCHEMA)
    intake_id = _text("manifest.intakeId", manifest["intakeId"])
    _literal("manifest.executionClass", manifest["executionClass"], STRICT_EVIDENCE_EXECUTION_CLASS)
    _literal("manifest.stage1AuthorizationPresented", manifest["stage1AuthorizationPresented"], False)

    files: list[BoundFile] = [manifest_file]
    stage0_file = _resolve_binding(
        root, "stage0Contract", manifest["stage0Contract"], json_document=True
    )
    native_rf_file = _resolve_binding(
        root, "nativeRfReceipt", manifest["nativeRfReceipt"], json_document=True
    )
    calibration_file = _resolve_binding(
        root, "calibrationBundle", manifest["calibrationBundle"], json_document=True
    )
    candidate_file = _resolve_binding(
        root, "candidateCatalog", manifest["candidateCatalog"], json_document=True
    )
    trajectory_file = _resolve_binding(
        root, "trajectoryCorpus", manifest["trajectoryCorpus"], json_document=True
    )
    precompute_file = _resolve_binding(
        root, "precompute", manifest["precompute"], json_document=True
    )
    files.extend((stage0_file, native_rf_file, calibration_file, candidate_file, trajectory_file, precompute_file))

    dataset_files: dict[str, BoundFile] = {}
    dataset_artifact_ids: dict[str, str] = {}
    dataset_sensor_classes: dict[str, str] = {}
    dataset_hardware_revisions: dict[str, str] = {}
    for index, raw in enumerate(_sequence("manifest.calibrationDatasets", manifest["calibrationDatasets"])):
        row = _exact(
            f"manifest.calibrationDatasets[{index}]",
            raw,
            (
                "sensorClass",
                "hardwareId",
                "hardwareRevision",
                "stage0ArtifactId",
                "file",
            ),
        )
        sensor_class = _text("calibration dataset sensorClass", row["sensorClass"])
        hardware_id = _text("calibration dataset hardwareId", row["hardwareId"])
        hardware_revision = _text(
            "calibration dataset hardwareRevision", row["hardwareRevision"]
        )
        if hardware_id in dataset_files or sensor_class not in SENSOR_CLASSES:
            raise ValueError(
                "calibrationDatasets must contain unique hardware IDs from allowed classes"
            )
        bound = _resolve_binding(
            root,
            f"calibrationDataset[{hardware_id}]",
            row["file"],
            json_document=False,
        )
        if bound.byte_length == 0:
            raise ValueError("calibration dataset files must not be empty")
        dataset_files[hardware_id] = bound
        dataset_artifact_ids[hardware_id] = _text(
            "stage0ArtifactId", row["stage0ArtifactId"]
        )
        dataset_sensor_classes[hardware_id] = sensor_class
        dataset_hardware_revisions[hardware_id] = hardware_revision
        files.append(bound)
    if not dataset_files:
        raise ValueError("calibrationDatasets must not be empty")

    evaluation_files: dict[str, BoundFile] = {}
    evaluation_sensor_classes: dict[str, str] = {}
    evaluation_hardware_revisions: dict[str, str] = {}
    for index, raw in enumerate(_sequence("manifest.evaluationEvidence", manifest["evaluationEvidence"])):
        row = _exact(
            f"manifest.evaluationEvidence[{index}]",
            raw,
            ("sensorClass", "hardwareId", "hardwareRevision", "file"),
        )
        sensor_class = _text("evaluation evidence sensorClass", row["sensorClass"])
        hardware_id = _text("evaluation evidence hardwareId", row["hardwareId"])
        hardware_revision = _text(
            "evaluation evidence hardwareRevision", row["hardwareRevision"]
        )
        if hardware_id in evaluation_files or sensor_class not in SENSOR_CLASSES:
            raise ValueError(
                "evaluationEvidence must contain unique hardware IDs from allowed classes"
            )
        bound = _resolve_binding(
            root,
            f"evaluationEvidence[{hardware_id}]",
            row["file"],
            json_document=True,
        )
        if bound.byte_length == 0:
            raise ValueError("evaluation evidence files must not be empty")
        evaluation_files[hardware_id] = bound
        evaluation_sensor_classes[hardware_id] = sensor_class
        evaluation_hardware_revisions[hardware_id] = hardware_revision
        files.append(bound)
    if not evaluation_files:
        raise ValueError("evaluationEvidence must not be empty")

    scenario_files: dict[str, BoundFile] = {}
    scenario_artifact_ids: dict[str, str] = {}
    for index, raw in enumerate(_sequence("manifest.scenarioPartitions", manifest["scenarioPartitions"])):
        row = _exact(
            f"manifest.scenarioPartitions[{index}]",
            raw,
            ("partitionId", "stage0ArtifactId", "file"),
        )
        partition_id = _text("scenario partitionId", row["partitionId"])
        if partition_id in scenario_files:
            raise ValueError(f"duplicate scenario partition {partition_id!r}")
        bound = _resolve_binding(
            root,
            f"scenarioPartition[{partition_id}]",
            row["file"],
            json_document=True,
        )
        scenario_files[partition_id] = bound
        scenario_artifact_ids[partition_id] = _text("stage0ArtifactId", row["stage0ArtifactId"])
        files.append(bound)

    identities = [item.stable_identity for item in files]
    if len(identities) != len(set(identities)):
        raise ValueError(
            "each manifest evidence role must bind a distinct physical file; "
            "hardlink aliases are prohibited"
        )

    stage0_mapping = _decode_json(stage0_file.role, _json_bytes(stage0_file))
    artifacts = _stage0_artifacts(root, stage0_file, stage0_mapping)
    stage0_contract = Stage0StudyContract.from_mapping(
        stage0_mapping,
        artifact_base_directory=stage0_file.resolved_path.parent,
        verified_artifact_sha256={
            artifact_id: str(item["sha256"])
            for artifact_id, item in artifacts.items()
        },
    )
    stage0 = _assert_stage0_ready(stage0_contract)

    candidate_artifact_id = stage0["solverBenchmark"]["candidateSetArtifactId"]
    _require_stage0_file("candidateCatalog", candidate_file, candidate_artifact_id, "SOLVER_CANDIDATES", artifacts)
    trajectory_artifact_id = stage0["targetAndTrajectoryEnvelope"]["trajectoryCorpus"]["artifactId"]
    _require_stage0_file("trajectoryCorpus", trajectory_file, trajectory_artifact_id, "TARGET_TRAJECTORIES", artifacts)

    declared_partitions = stage0["scenarioCorpus"]["partitions"]
    expected_partition_ids = {item["partitionId"] for item in declared_partitions.values()}
    if set(scenario_files) != expected_partition_ids:
        raise ValueError("manifest scenarioPartitions do not exactly match Stage-0")
    role_by_name = {
        "train": "TRAIN_SCENARIOS",
        "validation": "VALIDATION_SCENARIOS",
        "test": "TEST_SCENARIOS",
    }
    for name, declaration in declared_partitions.items():
        partition_id = declaration["partitionId"]
        expected_artifact = declaration["scenarioSetArtifactId"]
        if scenario_artifact_ids[partition_id] != expected_artifact:
            raise ValueError(f"scenario partition {partition_id!r} has the wrong Stage-0 artifact ID")
        _require_stage0_file(
            f"scenarioPartition[{partition_id}]",
            scenario_files[partition_id],
            expected_artifact,
            role_by_name[name],
            artifacts,
        )

    native_run_token = _validate_native_rf_receipt(root, native_rf_file, stage0, artifacts)
    candidates = _validate_candidate_catalog(candidate_file, stage0)
    hardware_by_id = {
        item["hardwareId"]: item for item in stage0["hardwareInventory"]
    }
    candidate_hardware_ids = {
        hardware_id
        for candidate in candidates
        for hardware_id in candidate["hardwareIdsBySensorClass"].values()
    }
    if set(dataset_files) != candidate_hardware_ids:
        raise ValueError(
            "calibrationDatasets must exactly cover candidate-referenced hardware IDs"
        )
    if set(evaluation_files) != candidate_hardware_ids:
        raise ValueError(
            "evaluationEvidence must exactly cover candidate-referenced hardware IDs"
        )
    for hardware_id in sorted(candidate_hardware_ids):
        hardware = hardware_by_id[hardware_id]
        sensor_class = hardware["sensorClass"]
        revision = hardware["hardwareRevision"]
        _literal(
            f"calibrationDataset[{hardware_id}].sensorClass",
            dataset_sensor_classes[hardware_id],
            sensor_class,
        )
        _literal(
            f"calibrationDataset[{hardware_id}].hardwareRevision",
            dataset_hardware_revisions[hardware_id],
            revision,
        )
        _literal(
            f"evaluationEvidence[{hardware_id}].sensorClass",
            evaluation_sensor_classes[hardware_id],
            sensor_class,
        )
        _literal(
            f"evaluationEvidence[{hardware_id}].hardwareRevision",
            evaluation_hardware_revisions[hardware_id],
            revision,
        )
        expected_artifact = hardware["calibrationArtifactId"]
        if dataset_artifact_ids[hardware_id] != expected_artifact:
            raise ValueError(
                f"calibration dataset for hardware {hardware_id!r} has the "
                "wrong Stage-0 artifact ID"
            )
        _require_stage0_file(
            f"calibrationDataset[{hardware_id}]",
            dataset_files[hardware_id],
            expected_artifact,
            "SENSOR_CALIBRATION",
            artifacts,
        )

    native_receipt_artifact_id = next(
        artifact_id
        for artifact_id, item in artifacts.items()
        if item["role"] == "RF_NATIVE_TEST_RECEIPT"
    )
    authorized_cross_set_aliases = {
        native_rf_file.role: native_receipt_artifact_id,
        candidate_file.role: candidate_artifact_id,
        trajectory_file.role: trajectory_artifact_id,
        **{
            scenario_files[declaration["partitionId"]].role: declaration[
                "scenarioSetArtifactId"
            ]
            for declaration in declared_partitions.values()
        },
        **{
            dataset_files[hardware_id].role: dataset_artifact_ids[hardware_id]
            for hardware_id in candidate_hardware_ids
        },
    }
    _reject_unauthorized_cross_set_aliases(
        files,
        artifacts,
        authorized_cross_set_aliases,
    )

    calibration_ids = _validate_calibration_bundle(
        calibration_file,
        dataset_files,
        hardware_by_id,
        candidate_hardware_ids,
    )
    scenario_to_partition = _validate_scenario_partitions(scenario_files, stage0)
    trajectories = _validate_trajectory_corpus(trajectory_file, stage0, scenario_to_partition)
    evaluator_result_bindings = _validate_evaluation_evidence(
        evaluation_files,
        hardware_by_id=hardware_by_id,
        calibration_ids=calibration_ids,
        stage0_contract_sha256=stage0_contract.digest,
        native_rf_receipt_sha256=native_rf_file.sha256,
        calibration_bundle_sha256=calibration_file.sha256,
        dataset_files=dataset_files,
        candidate_catalog_sha256=candidate_file.sha256,
        trajectory_corpus_sha256=trajectory_file.sha256,
        scenario_files=scenario_files,
    )

    expected_bindings: dict[str, object] = {
        "stage0ContractSha256": stage0_contract.digest,
        "stage0ContractFileSha256": stage0_file.sha256,
        "nativeRfReceiptFileSha256": native_rf_file.sha256,
        "nativeRfRunToken": native_run_token,
        "calibrationBundleFileSha256": calibration_file.sha256,
        "calibrationDatasetFileSha256ByHardwareId": {
            key: dataset_files[key].sha256 for key in sorted(dataset_files)
        },
        "candidateCatalogFileSha256": candidate_file.sha256,
        "trajectoryCorpusFileSha256": trajectory_file.sha256,
        "scenarioPartitionFileSha256ById": {
            key: scenario_files[key].sha256 for key in sorted(scenario_files)
        },
        "evaluationEvidenceFileSha256ByHardwareId": {
            key: evaluation_files[key].sha256 for key in sorted(evaluation_files)
        },
    }
    precompute = _validate_precompute(
        precompute_file,
        expected_bindings=expected_bindings,
        candidates=candidates,
        trajectories=trajectories,
        calibration_ids=calibration_ids,
        evaluation_files=evaluation_files,
        evaluator_result_bindings=evaluator_result_bindings,
    )

    receipt: dict[str, object] = {
        "schemaVersion": EVIDENCE_INTAKE_RECEIPT_SCHEMA,
        "valid": True,
        "intakeId": intake_id,
        "executionClass": STRICT_EVIDENCE_EXECUTION_CLASS,
        "approvedRoot": str(root),
        "manifestFileSha256": manifest_file.sha256,
        "stage0ContractSha256": stage0_contract.digest,
        "stage0ContractFileSha256": stage0_file.sha256,
        "nativeRfReceiptFileSha256": native_rf_file.sha256,
        "nativeRfRunToken": native_run_token,
        "calibrationBundleFileSha256": calibration_file.sha256,
        "candidateCatalogFileSha256": candidate_file.sha256,
        "trajectoryCorpusFileSha256": trajectory_file.sha256,
        "precomputeFileSha256": precompute_file.sha256,
        "precomputeArtifactSha256": precompute["artifactSha256"],
        "candidateCount": len(candidates),
        "trajectoryCount": len(trajectories),
        "resultRowCount": len(precompute["results"]),
        "sensorClasses": list(SENSOR_CLASSES),
        "scenarioPartitionIds": sorted(scenario_files),
        "fileBindings": [
            item.receipt_row() for item in sorted(files, key=lambda item: item.role)
        ],
        "validationScope": "STRUCTURAL_EVIDENCE_PACKAGE_INTAKE_ONLY",
        "structuralEvidencePackageValid": True,
        "stage0OutcomeEvidenceComplete": False,
        "stage1OutcomeReviewReady": False,
        "staticSolverInputAuthorized": False,
        "missingRobustOutcomeSemantics": list(
            ROBUST_OUTCOME_SEMANTICS_NOT_VALIDATED
        ),
        "stage1AuthorizationPresented": False,
        "solverExecutionAuthorized": False,
        "productionOptimizationAuthorized": False,
        "physicalDeploymentAuthorized": False,
        "nativeRfEvidenceScope": "ACTUAL_FILE_LOAD_AND_QUERY_MECHANICS_NOT_FIELD_CALIBRATION",
        "fieldPerformanceClaimed": False,
    }
    receipt["receiptSha256"] = stable_digest(receipt)
    return receipt


__all__ = [
    "CANDIDATE_CATALOG_SCHEMA",
    "EVIDENCE_INTAKE_MANIFEST_SCHEMA",
    "EVIDENCE_INTAKE_RECEIPT_SCHEMA",
    "EVALUATOR_EVIDENCE_SCHEMA",
    "EVALUATOR_RESULT_ROW_FIELDS",
    "NATIVE_RF_AUTOMATION_FILTER",
    "NATIVE_RF_RECEIPT_SCHEMA",
    "ROBUST_OUTCOME_SEMANTICS_NOT_VALIDATED",
    "SCENARIO_PARTITION_SCHEMA",
    "TRAJECTORY_CORPUS_SCHEMA",
    "evaluator_result_rows_sha256",
    "validate_evidence_intake",
]
