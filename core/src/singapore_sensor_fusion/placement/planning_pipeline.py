"""Versioned, fail-closed planning primitives for the Stage-0 placement study.

This module deliberately separates four different kinds of evidence:

* an actual, hash-bound RF geometry resource load;
* measured calibration records versus synthetic demo assumptions;
* immutable per-candidate/per-trajectory precomputed results; and
* deterministic static selection and bounded continuous-pose challengers.

Nothing in this module approves a mount, asserts field performance, mutates an
Unreal project, or authorizes physical deployment.  Synthetic helpers are
visibly labelled at every output boundary.
"""

from __future__ import annotations

from dataclasses import dataclass, field
import hashlib
import json
import math
import os
from pathlib import Path
import random
import re
import stat
from types import MappingProxyType
from typing import Any, Callable, Iterable, Mapping, Sequence

from .contracts import stable_digest
from .study_contract import Stage0StudyContract


RF_GEOMETRY_SCHEMA = "triad.istana_public_view.rf_geometry.v1"
RF_MATERIAL_CATALOG_SCHEMA = "triad.rf_material_catalog.v1"
RF_SCENE_CONTRACT_SCHEMA = "triad.istana_public_view.rf_scene_contract.v2"
RF_LOAD_RECEIPT_SCHEMA = "triad.rf_actual_file_load_receipt.v1"
STAGE0_READINESS_RECEIPT_SCHEMA = "triad.stage0_readiness_receipt.v1"
MODEL_CALIBRATION_BUNDLE_SCHEMA = "triad.sensor_model_calibration_bundle.v1"
CANDIDATE_TRAJECTORY_PRECOMPUTE_SCHEMA = (
    "triad.candidate_trajectory_precompute.v1"
)
STATIC_SOLVER_RESULT_SCHEMA = "triad.robust_static_solver_result.v2"
POSE_REFINEMENT_COMPARISON_SCHEMA = "triad.pose_refinement_comparison.v1"

SENSOR_CLASSES = ("radar", "passive_rf", "rgb", "event_camera")
SENSOR_FAMILIES = MappingProxyType(
    {
        "radar": "active_radar",
        "passive_rf": "passive_rf",
        "rgb": "visual",
        "event_camera": "visual",
    }
)
SYNTHETIC_DEMO_EVIDENCE = "SYNTHETIC_DEMO_ASSUMPTION"
MEASURED_CALIBRATION_EVIDENCE = "MEASURED_CALIBRATION"
DEMO_SURROGATE_SOURCE = "SYNTHETIC_DEMO_KINEMATIC_SURROGATE_V1"
EXACT_SIMULATOR_REPLAY_SOURCE = "EXACT_SIMULATOR_REPLAY_V1"
STRICT_EVIDENCE_EXECUTION_CLASS = "STAGE0_EVIDENCE_INTAKE_ONLY"
GREEDY_LOCAL_ALGORITHM = "deterministic_greedy_local_search_v1"
BRANCH_AND_BOUND_ALGORITHM = "deterministic_robust_branch_and_bound_v1"
PATTERN_SEARCH_ALGORITHM = "bounded_derivative_free_pattern_search_v1"
EVOLUTIONARY_ALGORITHM = "seeded_evolutionary_pose_challenger_v1"

_SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


def _finite(name: str, value: object) -> float:
    if isinstance(value, bool):
        raise TypeError(f"{name} must be numeric")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"{name} must be finite")
    return result


def _positive(name: str, value: object) -> float:
    result = _finite(name, value)
    if result <= 0.0:
        raise ValueError(f"{name} must be > 0")
    return result


def _unit(name: str, value: object) -> float:
    result = _finite(name, value)
    if not 0.0 <= result <= 1.0:
        raise ValueError(f"{name} must be in [0, 1]")
    return result


def _text(name: str, value: object) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{name} must be a non-empty string")
    return value.strip()


def _stat_identity(value: os.stat_result) -> tuple[int, int, int, int, int]:
    return (
        value.st_dev,
        value.st_ino,
        value.st_size,
        value.st_mtime_ns,
        value.st_ctime_ns,
    )


def _read_stable_file_bytes(path: Path) -> bytes:
    """Read one immutable regular-file snapshot without a hash/parse reopen."""

    path_before = path.stat()
    if not stat.S_ISREG(path_before.st_mode):
        raise ValueError(f"{path.name} must be a regular file")
    with path.open("rb") as handle:
        handle_before = os.fstat(handle.fileno())
        data = handle.read()
        handle_after = os.fstat(handle.fileno())
    path_after = path.stat()
    identities = {
        _stat_identity(path_before),
        _stat_identity(handle_before),
        _stat_identity(handle_after),
        _stat_identity(path_after),
    }
    if len(identities) != 1 or len(data) != handle_after.st_size:
        raise ValueError(f"{path.name} changed while it was being read")
    return data


def _load_json_bytes_without_duplicate_keys(
    name: str, data: bytes
) -> Mapping[str, Any]:
    def reject_duplicates(pairs: list[tuple[str, object]]) -> dict[str, object]:
        result: dict[str, object] = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"{name} contains duplicate JSON key {key!r}")
            result[key] = value
        return result

    value = json.loads(
        data.decode("utf-8", errors="strict"),
        object_pairs_hook=reject_duplicates,
    )
    if not isinstance(value, Mapping):
        raise TypeError(f"{name} must contain one JSON object")
    return value


def _require_sequence(name: str, value: object) -> Sequence[object]:
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence):
        raise TypeError(f"{name} must be an array")
    return value


def _require_mapping(name: str, value: object) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise TypeError(f"{name} must be an object")
    return value


def _require_sha(name: str, value: object) -> str:
    digest = _text(name, value).lower()
    if not _SHA256_RE.fullmatch(digest):
        raise ValueError(f"{name} must be a lowercase SHA-256 digest")
    return digest


def _json_ready(value: Any) -> Any:
    if isinstance(value, Mapping):
        return {key: _json_ready(item) for key, item in value.items()}
    if isinstance(value, (tuple, list)):
        return [_json_ready(item) for item in value]
    return value


@dataclass(frozen=True, slots=True)
class RFGeometryLoadReceipt:
    """Receipt proving the exact three source files that were parsed."""

    geometry_file: str
    material_catalog_file: str
    scene_contract_file: str
    geometry_sha256: str
    material_catalog_sha256: str
    scene_contract_sha256: str
    geometry_asset_id: str
    revision: str
    solid_count: int
    surface_count: int
    vertex_count: int
    triangle_count: int
    partition_cell_count: int
    positive_volume_overlap_pair_count: int
    material_calibration_states: tuple[str, ...]
    covers_one_kilometre_aoi: bool
    field_validated: bool
    survey_controlled: bool
    external_acceptance_context_bound: bool = False

    @property
    def survey_truth_ready(self) -> bool:
        return (
            self.field_validated
            and self.survey_controlled
            and self.external_acceptance_context_bound
            and bool(self.material_calibration_states)
            and set(self.material_calibration_states) == {"CALIBRATED"}
        )

    def to_dict(self) -> dict[str, object]:
        payload: dict[str, object] = {
            "schemaVersion": RF_LOAD_RECEIPT_SCHEMA,
            "files": {
                "geometry": {
                    "name": self.geometry_file,
                    "sha256": self.geometry_sha256,
                },
                "materialCatalog": {
                    "name": self.material_catalog_file,
                    "sha256": self.material_catalog_sha256,
                },
                "sceneContract": {
                    "name": self.scene_contract_file,
                    "sha256": self.scene_contract_sha256,
                },
            },
            "geometryAssetId": self.geometry_asset_id,
            "revision": self.revision,
            "counts": {
                "solids": self.solid_count,
                "surfaces": self.surface_count,
                "vertices": self.vertex_count,
                "triangles": self.triangle_count,
                "partitionCells": self.partition_cell_count,
                "positiveVolumeOverlapPairs": self.positive_volume_overlap_pair_count,
            },
            "materialCalibrationStates": list(self.material_calibration_states),
            "coversOneKilometreAoi": self.covers_one_kilometre_aoi,
            "fieldValidated": self.field_validated,
            "surveyControlled": self.survey_controlled,
            "externalAcceptanceContextBound": self.external_acceptance_context_bound,
            "surveyTruthReady": self.survey_truth_ready,
            "authority": (
                "FIELD_VALIDATED"
                if self.survey_truth_ready
                else "SIMULATION_READY_ASSUMPTION_BOUND"
            ),
            "physicalDeploymentAuthorized": False,
        }
        payload["receiptSha256"] = stable_digest(payload)
        return payload


def load_and_validate_rf_geometry(
    geometry_path: str | Path,
    material_catalog_path: str | Path,
    scene_contract_path: str | Path,
) -> RFGeometryLoadReceipt:
    """Parse and cross-check an actual RF geometry/catalog/contract triple.

    The validation intentionally performs more than a file-exists check: raw
    byte hashes must match the links embedded in the geometry and scene
    contract, declared counts must match the arrays that were loaded, triangle
    indexes must resolve, and every surface material must exist in the catalog.
    """

    geometry_file = Path(geometry_path).resolve(strict=True)
    catalog_file = Path(material_catalog_path).resolve(strict=True)
    scene_file = Path(scene_contract_path).resolve(strict=True)
    for name, path in (
        ("geometry", geometry_file),
        ("material catalog", catalog_file),
        ("scene contract", scene_file),
    ):
        if not path.is_file():
            raise ValueError(f"{name} path must resolve to a regular file")

    geometry_bytes = _read_stable_file_bytes(geometry_file)
    catalog_bytes = _read_stable_file_bytes(catalog_file)
    scene_bytes = _read_stable_file_bytes(scene_file)
    geometry_sha = hashlib.sha256(geometry_bytes).hexdigest()
    catalog_sha = hashlib.sha256(catalog_bytes).hexdigest()
    scene_sha = hashlib.sha256(scene_bytes).hexdigest()
    geometry = _load_json_bytes_without_duplicate_keys(
        geometry_file.name, geometry_bytes
    )
    catalog = _load_json_bytes_without_duplicate_keys(catalog_file.name, catalog_bytes)
    scene = _load_json_bytes_without_duplicate_keys(scene_file.name, scene_bytes)

    if geometry.get("schemaVersion") != RF_GEOMETRY_SCHEMA:
        raise ValueError("unexpected RF geometry schemaVersion")
    if catalog.get("schemaVersion") != RF_MATERIAL_CATALOG_SCHEMA:
        raise ValueError("unexpected RF material catalog schemaVersion")
    if scene.get("schemaVersion") != RF_SCENE_CONTRACT_SCHEMA:
        raise ValueError("unexpected RF scene contract schemaVersion")
    geometry_asset_id = _text("geometry.assetId", geometry.get("assetId"))
    if _text("scene.assetId", scene.get("assetId")) != geometry_asset_id:
        raise ValueError("geometry and scene-contract asset IDs do not match")
    if scene.get("revision") != geometry.get("revision"):
        raise ValueError("geometry and scene-contract revisions do not match")
    if _require_sha("geometry.materialCatalogSha256", geometry.get("materialCatalogSha256")) != catalog_sha:
        raise ValueError("geometry materialCatalogSha256 does not match catalog bytes")
    if _require_sha("geometry.contractSha256", geometry.get("contractSha256")) != scene_sha:
        raise ValueError("geometry contractSha256 does not match scene-contract bytes")

    declared_catalog_name = _text("geometry.materialCatalog", geometry.get("materialCatalog"))
    if declared_catalog_name.casefold() != catalog_file.name.casefold():
        raise ValueError("geometry materialCatalog does not name the supplied catalog")
    material_binding = _require_mapping("scene.materialBinding", scene.get("materialBinding"))
    if _require_sha("scene.materialBinding.catalogSha256", material_binding.get("catalogSha256")) != catalog_sha:
        raise ValueError("scene material catalog hash does not match catalog bytes")
    if _text("scene.materialBinding.catalog", material_binding.get("catalog")).casefold() != catalog_file.name.casefold():
        raise ValueError("scene material binding does not name the supplied catalog")

    vertices = _require_sequence("geometry.verticesMeters", geometry.get("verticesMeters"))
    triangles = _require_sequence("geometry.triangles", geometry.get("triangles"))
    surfaces = _require_sequence("geometry.surfaces", geometry.get("surfaces"))
    solids = _require_sequence("geometry.solids", geometry.get("solids"))
    metrics = _require_mapping("geometry.metrics", geometry.get("metrics"))
    count_pairs = (
        ("vertexCount", len(vertices)),
        ("triangleCount", len(triangles)),
        ("surfaceCount", len(surfaces)),
        ("solidCount", len(solids)),
    )
    for metric_name, observed in count_pairs:
        declared = metrics.get(metric_name)
        if isinstance(declared, bool) or not isinstance(declared, int) or declared != observed:
            raise ValueError(
                f"geometry metrics.{metric_name}={declared!r} does not match loaded count {observed}"
            )

    for index, raw_vertex in enumerate(vertices):
        vertex = _require_sequence(f"geometry.verticesMeters[{index}]", raw_vertex)
        if len(vertex) != 3:
            raise ValueError(
                f"geometry.verticesMeters[{index}] must have three coordinates"
            )
        for axis, coordinate in zip(("x", "y", "z"), vertex, strict=True):
            if isinstance(coordinate, bool) or not isinstance(
                coordinate, (int, float)
            ):
                raise TypeError(
                    f"geometry.verticesMeters[{index}].{axis} must be numeric"
                )
            if not math.isfinite(float(coordinate)):
                raise ValueError(
                    f"geometry.verticesMeters[{index}].{axis} must be finite"
                )

    for index, raw_triangle in enumerate(triangles):
        triangle = _require_sequence(f"geometry.triangles[{index}]", raw_triangle)
        if len(triangle) != 3:
            raise ValueError(f"geometry.triangles[{index}] must have three indexes")
        for vertex_index in triangle:
            if (
                isinstance(vertex_index, bool)
                or not isinstance(vertex_index, int)
                or not 0 <= vertex_index < len(vertices)
            ):
                raise ValueError(
                    f"geometry.triangles[{index}] references invalid vertex {vertex_index!r}"
                )

    catalog_materials = _require_sequence("catalog.materials", catalog.get("materials"))
    material_ids: set[str] = set()
    calibration_states: set[str] = set()
    for index, raw_material in enumerate(catalog_materials):
        material = _require_mapping(f"catalog.materials[{index}]", raw_material)
        material_id = _text(f"catalog.materials[{index}].materialId", material.get("materialId"))
        if material_id in material_ids:
            raise ValueError(f"duplicate catalog materialId {material_id!r}")
        material_ids.add(material_id)
        calibration_states.add(
            _text(
                f"catalog.materials[{index}].calibrationState",
                material.get("calibrationState"),
            )
        )

    surface_ids: set[str] = set()
    for index, raw_surface in enumerate(surfaces):
        surface = _require_mapping(f"geometry.surfaces[{index}]", raw_surface)
        surface_id = _text(f"geometry.surfaces[{index}].surfaceId", surface.get("surfaceId"))
        if surface_id in surface_ids:
            raise ValueError(f"duplicate geometry surfaceId {surface_id!r}")
        surface_ids.add(surface_id)
        material_id = _text(f"geometry.surfaces[{index}].materialId", surface.get("materialId"))
        if material_id not in material_ids:
            raise ValueError(f"surface {surface_id!r} references unknown material {material_id!r}")
        triangle_start = surface.get("triangleStart")
        triangle_count = surface.get("triangleCount")
        if any(isinstance(item, bool) or not isinstance(item, int) for item in (triangle_start, triangle_count)):
            raise TypeError(f"surface {surface_id!r} triangle bounds must be integers")
        if triangle_start < 0 or triangle_count <= 0 or triangle_start + triangle_count > len(triangles):
            raise ValueError(f"surface {surface_id!r} triangle bounds are outside the loaded array")

    solid_ids: set[str] = set()
    for index, raw_solid in enumerate(solids):
        solid = _require_mapping(f"geometry.solids[{index}]", raw_solid)
        solid_id = _text(f"geometry.solids[{index}].solidId", solid.get("solidId"))
        if solid_id in solid_ids:
            raise ValueError(f"duplicate geometry solidId {solid_id!r}")
        solid_ids.add(solid_id)
        if _text(f"geometry.solids[{index}].materialId", solid.get("materialId")) not in material_ids:
            raise ValueError(f"solid {solid_id!r} references an unknown material")

    coverage = _require_mapping(
        "geometry.modeledCoverageEnvelope", geometry.get("modeledCoverageEnvelope")
    )
    partition_cell_count = metrics.get("surroundingPartitionCellCount")
    if (
        isinstance(partition_cell_count, bool)
        or not isinstance(partition_cell_count, int)
        or partition_cell_count <= 0
    ):
        raise ValueError("geometry must declare a positive surroundingPartitionCellCount")
    overlap_count = metrics.get("positiveVolumeOverlapPairCount")
    if isinstance(overlap_count, bool) or not isinstance(overlap_count, int):
        raise TypeError("geometry positiveVolumeOverlapPairCount must be an integer")
    if overlap_count != 0:
        raise ValueError("RF geometry contains declared positive-volume overlaps")
    if coverage.get("coversOneKilometreAoi") is not True:
        raise ValueError("RF geometry does not cover the declared one-kilometre AOI")
    truth_flags = _require_mapping(
        "geometry.modeledCoverageEnvelope.studyDomain.truthFlags",
        _require_mapping(
            "geometry.modeledCoverageEnvelope.studyDomain",
            coverage.get("studyDomain"),
        ).get("truthFlags"),
    )
    field_validated = coverage.get("fieldValidated") is True
    survey_controlled = coverage.get("surveyControlled") is True
    if truth_flags.get("fieldValidated") is not field_validated:
        raise ValueError("coverage fieldValidated flags disagree")
    if truth_flags.get("surveyControlled") is not survey_controlled:
        raise ValueError("coverage surveyControlled flags disagree")

    return RFGeometryLoadReceipt(
        geometry_file=geometry_file.name,
        material_catalog_file=catalog_file.name,
        scene_contract_file=scene_file.name,
        geometry_sha256=geometry_sha,
        material_catalog_sha256=catalog_sha,
        scene_contract_sha256=scene_sha,
        geometry_asset_id=geometry_asset_id,
        revision=_text("geometry.revision", geometry.get("revision")),
        solid_count=len(solids),
        surface_count=len(surfaces),
        vertex_count=len(vertices),
        triangle_count=len(triangles),
        partition_cell_count=partition_cell_count,
        positive_volume_overlap_pair_count=overlap_count,
        material_calibration_states=tuple(sorted(calibration_states)),
        covers_one_kilometre_aoi=coverage.get("coversOneKilometreAoi") is True,
        field_validated=field_validated,
        survey_controlled=survey_controlled,
    )


def assess_stage0_readiness(contract: Stage0StudyContract) -> dict[str, object]:
    """Return a deterministic blocker receipt without self-approving evidence."""

    if not isinstance(contract, Stage0StudyContract):
        raise TypeError("contract must be a validated Stage0StudyContract")
    payload = contract.to_dict()
    artifacts = payload["artifactBindings"]
    gates = payload["readiness"]["gates"]
    mount_regions = payload["mountRegionPolicy"]["regions"]
    approvals = payload["governance"]["approvals"]

    unverified_artifact_ids = sorted(
        item["artifactId"] for item in artifacts if item["readiness"] != "VERIFIED"
    )
    blocked_gate_ids = sorted(
        item["gateId"] for item in gates if item["status"] != "PASSED"
    )
    ineligible_mount_ids = sorted(
        item["regionId"]
        for item in mount_regions
        if item["eligibleForCandidateGeneration"] is not True
    )
    incomplete_approval_roles = sorted(
        item["role"] for item in approvals if item["decision"] != "APPROVED"
    )
    scope = payload["scope"]
    evidence_ready = (
        payload["contractState"] == "FROZEN"
        and not unverified_artifact_ids
        and not blocked_gate_ids
        and any(item["eligibleForCandidateGeneration"] is True for item in mount_regions)
        and not incomplete_approval_roles
    )
    production_authorized = (
        evidence_ready and scope["productionOptimizerImplementationAuthorized"] is True
    )
    deployment_authorized = (
        evidence_ready and scope["physicalDeploymentAuthorized"] is True
    )
    demo_allowed = not any(item["decision"] == "REJECTED" for item in approvals)
    blockers: list[str] = []
    if payload["contractState"] != "FROZEN":
        blockers.append("STAGE0_CONTRACT_NOT_FROZEN")
    if unverified_artifact_ids:
        blockers.append("UNVERIFIED_ARTIFACT_BINDINGS")
    if blocked_gate_ids:
        blockers.append("READINESS_GATES_NOT_PASSED")
    if not any(item["eligibleForCandidateGeneration"] is True for item in mount_regions):
        blockers.append("NO_ELIGIBLE_MOUNT_REGION")
    if incomplete_approval_roles:
        blockers.append("GOVERNANCE_APPROVALS_INCOMPLETE")
    if scope["productionOptimizerImplementationAuthorized"] is not True:
        blockers.append("PRODUCTION_OPTIMIZER_NOT_AUTHORIZED")
    if scope["physicalDeploymentAuthorized"] is not True:
        blockers.append("PHYSICAL_DEPLOYMENT_NOT_AUTHORIZED")

    receipt: dict[str, object] = {
        "schemaVersion": STAGE0_READINESS_RECEIPT_SCHEMA,
        "studyId": contract.study_id,
        "contractSha256": contract.digest,
        "contractState": payload["contractState"],
        "executionClass": (
            "PRODUCTION_AUTHORIZED" if production_authorized else "DEMO_STUDY_ONLY"
        ),
        "demoStudyExecutionAllowed": demo_allowed,
        "productionOptimizationAuthorized": production_authorized,
        "physicalDeploymentAuthorized": deployment_authorized,
        "unverifiedArtifactIds": unverified_artifact_ids,
        "blockedGateIds": blocked_gate_ids,
        "ineligibleMountRegionIds": ineligible_mount_ids,
        "incompleteApprovalRoles": incomplete_approval_roles,
        "blockers": blockers,
    }
    receipt["receiptSha256"] = stable_digest(receipt)
    return receipt


@dataclass(frozen=True, slots=True)
class SensorModelCalibration:
    """One versioned sensor-model declaration with explicit evidence class."""

    calibration_id: str
    sensor_class: str
    evidence_class: str
    maximum_range_meters: float
    minimum_quality: float
    horizontal_fov_degrees: float
    range_exponent: float = 1.0
    dataset_sha256: str | None = None
    limitations: tuple[str, ...] = ()
    hardware_id: str | None = None
    hardware_revision: str | None = None

    def __post_init__(self) -> None:
        object.__setattr__(self, "calibration_id", _text("calibration_id", self.calibration_id))
        sensor_class = _text("sensor_class", self.sensor_class)
        if sensor_class not in SENSOR_CLASSES:
            raise ValueError(f"sensor_class must be selected from {SENSOR_CLASSES}")
        object.__setattr__(self, "sensor_class", sensor_class)
        if self.evidence_class not in (
            SYNTHETIC_DEMO_EVIDENCE,
            MEASURED_CALIBRATION_EVIDENCE,
        ):
            raise ValueError("evidence_class must explicitly be synthetic demo or measured")
        object.__setattr__(
            self,
            "maximum_range_meters",
            _positive("maximum_range_meters", self.maximum_range_meters),
        )
        object.__setattr__(
            self, "minimum_quality", _unit("minimum_quality", self.minimum_quality)
        )
        fov = _positive("horizontal_fov_degrees", self.horizontal_fov_degrees)
        if fov > 360.0:
            raise ValueError("horizontal_fov_degrees must be <= 360")
        object.__setattr__(self, "horizontal_fov_degrees", fov)
        object.__setattr__(
            self, "range_exponent", _positive("range_exponent", self.range_exponent)
        )
        if self.evidence_class == MEASURED_CALIBRATION_EVIDENCE:
            if self.dataset_sha256 is None:
                raise ValueError("measured calibration requires dataset_sha256")
            object.__setattr__(
                self, "dataset_sha256", _require_sha("dataset_sha256", self.dataset_sha256)
            )
        elif self.dataset_sha256 is not None:
            raise ValueError("synthetic demo calibration must not cite a measured dataset hash")
        if (self.hardware_id is None) != (self.hardware_revision is None):
            raise ValueError(
                "hardware_id and hardware_revision must either both be supplied or both be absent"
            )
        if self.hardware_id is not None:
            object.__setattr__(self, "hardware_id", _text("hardware_id", self.hardware_id))
            object.__setattr__(
                self,
                "hardware_revision",
                _text("hardware_revision", self.hardware_revision),
            )
        if (
            self.evidence_class == SYNTHETIC_DEMO_EVIDENCE
            and self.hardware_id is not None
        ):
            raise ValueError("synthetic demo calibration must not bind production hardware")
        limits = tuple(_text("limitations[]", item) for item in self.limitations)
        if not limits:
            raise ValueError("limitations must state the model boundary")
        object.__setattr__(self, "limitations", limits)

    @property
    def family(self) -> str:
        return SENSOR_FAMILIES[self.sensor_class]

    def to_dict(self) -> dict[str, object]:
        result: dict[str, object] = {
            "calibrationId": self.calibration_id,
            "sensorClass": self.sensor_class,
            "evidenceClass": self.evidence_class,
            "evidenceFamily": self.family,
            "parameters": {
                "maximumRangeMeters": self.maximum_range_meters,
                "minimumQuality": self.minimum_quality,
                "horizontalFovDegrees": self.horizontal_fov_degrees,
                "rangeExponent": self.range_exponent,
            },
            "datasetSha256": self.dataset_sha256,
            "limitations": list(self.limitations),
        }
        if self.hardware_id is not None:
            result["hardwareId"] = self.hardware_id
            result["hardwareRevision"] = self.hardware_revision
        return result


def build_calibration_bundle(
    calibrations: Iterable[SensorModelCalibration],
) -> dict[str, object]:
    records = tuple(
        sorted(
            calibrations,
            key=lambda item: (item.sensor_class, item.hardware_id or ""),
        )
    )
    if not records or {item.sensor_class for item in records} != set(SENSOR_CLASSES):
        raise ValueError("calibration bundle must cover every allowed sensor class")
    if len({item.calibration_id for item in records}) != len(records):
        raise ValueError("calibration bundle calibration IDs must be unique")
    measured = all(
        item.evidence_class == MEASURED_CALIBRATION_EVIDENCE for item in records
    )
    if measured:
        if any(
            item.hardware_id is None or item.hardware_revision is None
            for item in records
        ):
            raise ValueError(
                "measured calibration bundle records must bind hardware ID and revision"
            )
        if len({item.hardware_id for item in records}) != len(records):
            raise ValueError("measured calibration hardware IDs must be unique")
    elif (
        len(records) != len(SENSOR_CLASSES)
        or len({item.sensor_class for item in records}) != len(records)
    ):
        raise ValueError(
            "synthetic demo calibration bundle must contain exactly one record per sensor class"
        )
    payload: dict[str, object] = {
        "schemaVersion": MODEL_CALIBRATION_BUNDLE_SCHEMA,
        "calibrations": [item.to_dict() for item in records],
        "allModelsMeasured": measured,
        "scoreSemantics": (
            "MEASURED_CALIBRATION_WITHIN_DECLARED_LIMITS"
            if measured
            else "SYNTHETIC_DEMO_EVIDENCE_INDEX_NOT_A_PROBABILITY"
        ),
        "measuredCalibrationEvidenceBound": measured,
        "fieldPerformanceClaimed": False,
        "physicalDeploymentAuthorized": False,
    }
    payload["bundleSha256"] = stable_digest(payload)
    return payload


def default_synthetic_demo_calibrations() -> tuple[SensorModelCalibration, ...]:
    """Return visible demo assumptions, never a substitute for measured data."""

    common_limit = (
        "Synthetic deterministic dashboard/replay input only; not fitted to measured sensor data.",
        "No clutter, false-alarm, calibration-drift, or field-performance claim.",
    )
    return (
        SensorModelCalibration(
            "demo-radar-v1",
            "radar",
            SYNTHETIC_DEMO_EVIDENCE,
            1200.0,
            0.22,
            360.0,
            1.35,
            limitations=common_limit,
        ),
        SensorModelCalibration(
            "demo-passive-rf-v1",
            "passive_rf",
            SYNTHETIC_DEMO_EVIDENCE,
            1500.0,
            0.18,
            360.0,
            1.10,
            limitations=common_limit
            + ("RF-silent targets are always ineligible in this surrogate.",),
        ),
        SensorModelCalibration(
            "demo-rgb-v1",
            "rgb",
            SYNTHETIC_DEMO_EVIDENCE,
            850.0,
            0.28,
            96.0,
            1.65,
            limitations=common_limit
            + ("Line of sight, illumination, privacy masks, and pixels-on-target require exact replay.",),
        ),
        SensorModelCalibration(
            "demo-event-camera-v1",
            "event_camera",
            SYNTHETIC_DEMO_EVIDENCE,
            700.0,
            0.25,
            110.0,
            1.45,
            limitations=common_limit
            + ("Does not model physical contrast thresholds, refractory behavior, or event noise.",),
        ),
    )


@dataclass(frozen=True, slots=True)
class CandidateSensorPose:
    candidate_id: str
    site_id: str
    failure_domain_id: str
    x_meters: float
    y_meters: float
    z_meters: float
    yaw_degrees: float
    sensor_classes: tuple[str, ...] = SENSOR_CLASSES
    cost_units: float = 1.0

    def __post_init__(self) -> None:
        for name in ("candidate_id", "site_id", "failure_domain_id"):
            object.__setattr__(self, name, _text(name, getattr(self, name)))
        for name in ("x_meters", "y_meters", "z_meters", "yaw_degrees"):
            object.__setattr__(self, name, _finite(name, getattr(self, name)))
        classes = tuple(_text("sensor_classes[]", item) for item in self.sensor_classes)
        if not classes or len(set(classes)) != len(classes):
            raise ValueError("sensor_classes must be non-empty and unique")
        if any(item not in SENSOR_CLASSES for item in classes):
            raise ValueError(f"sensor_classes must be selected from {SENSOR_CLASSES}")
        object.__setattr__(self, "sensor_classes", classes)
        cost = _finite("cost_units", self.cost_units)
        if cost < 0.0:
            raise ValueError("cost_units must be >= 0")
        object.__setattr__(self, "cost_units", cost)

    def to_dict(self) -> dict[str, object]:
        return {
            "candidateId": self.candidate_id,
            "siteId": self.site_id,
            "failureDomainId": self.failure_domain_id,
            "poseEnuMeters": [self.x_meters, self.y_meters, self.z_meters],
            "yawDegrees": self.yaw_degrees,
            "sensorClasses": list(self.sensor_classes),
            "costUnits": self.cost_units,
        }


@dataclass(frozen=True, slots=True)
class TrajectoryPoint:
    time_seconds: float
    x_meters: float
    y_meters: float
    z_meters: float

    def __post_init__(self) -> None:
        for name in ("time_seconds", "x_meters", "y_meters", "z_meters"):
            object.__setattr__(self, name, _finite(name, getattr(self, name)))
        if self.time_seconds < 0.0:
            raise ValueError("time_seconds must be >= 0")


@dataclass(frozen=True, slots=True)
class TargetTrajectory:
    trajectory_id: str
    scenario_id: str
    partition_id: str
    points: tuple[TrajectoryPoint, ...]
    rf_emitting: bool
    sensor_quality_factors: Mapping[str, float] = field(default_factory=dict)
    critical: bool = False

    def __post_init__(self) -> None:
        for name in ("trajectory_id", "scenario_id", "partition_id"):
            object.__setattr__(self, name, _text(name, getattr(self, name)))
        if not isinstance(self.rf_emitting, bool) or not isinstance(self.critical, bool):
            raise TypeError("rf_emitting and critical must be boolean")
        points = tuple(self.points)
        if not points:
            raise ValueError("points must not be empty")
        if any(not isinstance(item, TrajectoryPoint) for item in points):
            raise TypeError("points must contain TrajectoryPoint values")
        times = tuple(item.time_seconds for item in points)
        if tuple(sorted(times)) != times or len(set(times)) != len(times):
            raise ValueError("trajectory times must be strictly increasing")
        object.__setattr__(self, "points", points)
        factors = {
            _text("sensor_quality_factors key", key): _unit(
                f"sensor_quality_factors[{key!r}]", value
            )
            for key, value in self.sensor_quality_factors.items()
        }
        if any(key not in SENSOR_CLASSES for key in factors):
            raise ValueError("sensor_quality_factors contains an unknown sensor class")
        object.__setattr__(self, "sensor_quality_factors", MappingProxyType(factors))

    def to_dict(self) -> dict[str, object]:
        return {
            "trajectoryId": self.trajectory_id,
            "scenarioId": self.scenario_id,
            "partitionId": self.partition_id,
            "rfEmitting": self.rf_emitting,
            "critical": self.critical,
            "sensorQualityFactors": dict(self.sensor_quality_factors),
            "points": [
                {
                    "timeSeconds": item.time_seconds,
                    "enuMeters": [item.x_meters, item.y_meters, item.z_meters],
                }
                for item in self.points
            ],
        }


@dataclass(frozen=True, slots=True)
class CandidateTrajectoryResult:
    candidate_id: str
    trajectory_id: str
    scenario_id: str
    partition_id: str
    sensor_class: str
    evidence_family: str
    detection_fraction: float
    first_detection_seconds: float | None
    mean_quality: float
    source_class: str
    exact_simulator_replay: bool
    measured_calibration: bool
    rf_geometry_queried: bool

    def to_dict(self) -> dict[str, object]:
        return {
            "candidateId": self.candidate_id,
            "trajectoryId": self.trajectory_id,
            "scenarioId": self.scenario_id,
            "partitionId": self.partition_id,
            "sensorClass": self.sensor_class,
            "evidenceFamily": self.evidence_family,
            "detectionFraction": self.detection_fraction,
            "firstDetectionSeconds": self.first_detection_seconds,
            "meanQuality": self.mean_quality,
            "sourceClass": self.source_class,
            "exactSimulatorReplay": self.exact_simulator_replay,
            "measuredCalibration": self.measured_calibration,
            "rfGeometryQueried": self.rf_geometry_queried,
        }


def _angular_difference_degrees(left: float, right: float) -> float:
    return abs((left - right + 180.0) % 360.0 - 180.0)


def precompute_demo_candidate_trajectory_results(
    candidates: Iterable[CandidateSensorPose],
    trajectories: Iterable[TargetTrajectory],
    calibrations: Iterable[SensorModelCalibration],
    *,
    rf_geometry_receipt_sha256: str,
) -> dict[str, object]:
    """Build a deterministic dashboard/replay cache from explicit demo priors.

    This helper never traces the RF geometry and never claims exact Unreal or
    measured-model evidence.  It is useful for UI plumbing and solver contract
    tests while the Stage-0 evidence gates remain blocked.
    """

    receipt_sha = _require_sha(
        "rf_geometry_receipt_sha256", rf_geometry_receipt_sha256
    )
    candidate_rows = tuple(sorted(candidates, key=lambda item: item.candidate_id))
    trajectory_rows = tuple(sorted(trajectories, key=lambda item: item.trajectory_id))
    calibration_rows = tuple(sorted(calibrations, key=lambda item: item.sensor_class))
    if len({item.candidate_id for item in candidate_rows}) != len(candidate_rows):
        raise ValueError("candidate IDs must be unique")
    if len({item.trajectory_id for item in trajectory_rows}) != len(trajectory_rows):
        raise ValueError("trajectory IDs must be unique")
    calibration_by_class = {item.sensor_class: item for item in calibration_rows}
    if set(calibration_by_class) != set(SENSOR_CLASSES):
        raise ValueError("calibrations must contain the four allowed sensor classes")
    if any(
        item.evidence_class != SYNTHETIC_DEMO_EVIDENCE
        for item in calibration_rows
    ):
        raise ValueError("demo precompute accepts only explicitly synthetic calibrations")

    results: list[CandidateTrajectoryResult] = []
    for candidate in candidate_rows:
        for trajectory in trajectory_rows:
            for sensor_class in sorted(candidate.sensor_classes):
                model = calibration_by_class[sensor_class]
                detections = 0
                first_detection: float | None = None
                quality_sum = 0.0
                for point in trajectory.points:
                    dx = point.x_meters - candidate.x_meters
                    dy = point.y_meters - candidate.y_meters
                    dz = point.z_meters - candidate.z_meters
                    distance = math.sqrt(dx * dx + dy * dy + dz * dz)
                    bearing = math.degrees(math.atan2(dy, dx))
                    in_fov = (
                        model.horizontal_fov_degrees >= 360.0 - 1e-12
                        or _angular_difference_degrees(
                            bearing, candidate.yaw_degrees
                        )
                        <= model.horizontal_fov_degrees * 0.5 + 1e-12
                    )
                    range_margin = max(
                        0.0, 1.0 - distance / model.maximum_range_meters
                    )
                    quality = (
                        range_margin**model.range_exponent
                        * trajectory.sensor_quality_factors.get(sensor_class, 1.0)
                        if in_fov
                        else 0.0
                    )
                    if sensor_class == "passive_rf" and not trajectory.rf_emitting:
                        quality = 0.0
                    quality_sum += quality
                    if quality + 1e-12 >= model.minimum_quality:
                        detections += 1
                        if first_detection is None:
                            first_detection = point.time_seconds
                point_count = len(trajectory.points)
                results.append(
                    CandidateTrajectoryResult(
                        candidate_id=candidate.candidate_id,
                        trajectory_id=trajectory.trajectory_id,
                        scenario_id=trajectory.scenario_id,
                        partition_id=trajectory.partition_id,
                        sensor_class=sensor_class,
                        evidence_family=model.family,
                        detection_fraction=round(detections / point_count, 9),
                        first_detection_seconds=first_detection,
                        mean_quality=round(quality_sum / point_count, 9),
                        source_class=DEMO_SURROGATE_SOURCE,
                        exact_simulator_replay=False,
                        measured_calibration=False,
                        rf_geometry_queried=False,
                    )
                )

    calibration_bundle = build_calibration_bundle(calibration_rows)
    inputs = {
        "candidates": [item.to_dict() for item in candidate_rows],
        "trajectories": [item.to_dict() for item in trajectory_rows],
        "calibrationBundleSha256": calibration_bundle["bundleSha256"],
        "rfGeometryReceiptSha256": receipt_sha,
    }
    payload: dict[str, object] = {
        "schemaVersion": CANDIDATE_TRAJECTORY_PRECOMPUTE_SCHEMA,
        "revision": 1,
        "executionClass": "DEMO_STUDY_ONLY",
        "sourceClass": DEMO_SURROGATE_SOURCE,
        "scoreSemantics": "SYNTHETIC_DEMO_EVIDENCE_INDEX_NOT_A_PROBABILITY",
        "bindings": {
            "inputSha256": stable_digest(inputs),
            "calibrationBundleSha256": calibration_bundle["bundleSha256"],
            "rfGeometryReceiptSha256": receipt_sha,
        },
        "candidates": inputs["candidates"],
        "trajectories": inputs["trajectories"],
        "results": [
            item.to_dict()
            for item in sorted(
                results,
                key=lambda item: (
                    item.candidate_id,
                    item.trajectory_id,
                    item.sensor_class,
                ),
            )
        ],
        "allCandidateTrajectorySensorRowsPresent": True,
        "exactSimulatorReplayComplete": False,
        "measuredCalibrationUsed": False,
        "rfGeometryQueried": False,
        "fieldPerformanceClaimed": False,
        "physicalDeploymentAuthorized": False,
    }
    payload["artifactSha256"] = stable_digest(payload)
    return payload


@dataclass(frozen=True, slots=True)
class RobustDemand:
    demand_id: str
    scenario_id: str
    weight: float = 1.0
    critical: bool = False

    def __post_init__(self) -> None:
        object.__setattr__(self, "demand_id", _text("demand_id", self.demand_id))
        object.__setattr__(self, "scenario_id", _text("scenario_id", self.scenario_id))
        object.__setattr__(self, "weight", _positive("weight", self.weight))
        if not isinstance(self.critical, bool):
            raise TypeError("critical must be boolean")


@dataclass(frozen=True, slots=True)
class StaticCandidate:
    candidate_id: str
    site_id: str
    failure_domain_id: str
    cost_units: float
    evidence_families_by_demand: Mapping[str, tuple[str, ...]]

    def __post_init__(self) -> None:
        for name in ("candidate_id", "site_id", "failure_domain_id"):
            object.__setattr__(self, name, _text(name, getattr(self, name)))
        cost = _finite("cost_units", self.cost_units)
        if cost < 0.0:
            raise ValueError("cost_units must be >= 0")
        object.__setattr__(self, "cost_units", cost)
        frozen: dict[str, tuple[str, ...]] = {}
        for demand_id, families in self.evidence_families_by_demand.items():
            key = _text("demand_id", demand_id)
            values = tuple(sorted({_text("evidence_family", item) for item in families}))
            if values:
                frozen[key] = values
        object.__setattr__(
            self, "evidence_families_by_demand", MappingProxyType(frozen)
        )


@dataclass(frozen=True, slots=True)
class RobustStaticProblem:
    problem_id: str
    demands: tuple[RobustDemand, ...]
    candidates: tuple[StaticCandidate, ...]
    maximum_sites: int
    minimum_evidence_families: int = 2
    minimum_site_redundancy: int = 1
    minimum_failure_domain_redundancy: int = 1
    minimum_scenario_coverage_fraction: float = 1.0
    budget_units: float | None = None

    def __post_init__(self) -> None:
        object.__setattr__(self, "problem_id", _text("problem_id", self.problem_id))
        if isinstance(self.maximum_sites, bool) or not isinstance(self.maximum_sites, int) or self.maximum_sites <= 0:
            raise ValueError("maximum_sites must be a positive integer")
        for name in (
            "minimum_evidence_families",
            "minimum_site_redundancy",
            "minimum_failure_domain_redundancy",
        ):
            value = getattr(self, name)
            if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
                raise ValueError(f"{name} must be a positive integer")
        object.__setattr__(
            self,
            "minimum_scenario_coverage_fraction",
            _unit(
                "minimum_scenario_coverage_fraction",
                self.minimum_scenario_coverage_fraction,
            ),
        )
        if self.budget_units is not None:
            budget = _finite("budget_units", self.budget_units)
            if budget < 0.0:
                raise ValueError("budget_units must be >= 0")
            object.__setattr__(self, "budget_units", budget)
        demands = tuple(sorted(self.demands, key=lambda item: item.demand_id))
        candidates = tuple(sorted(self.candidates, key=lambda item: item.candidate_id))
        demand_ids = {item.demand_id for item in demands}
        if not demands or len(demand_ids) != len(demands):
            raise ValueError("demands must be non-empty with unique IDs")
        if not candidates or len({item.candidate_id for item in candidates}) != len(candidates):
            raise ValueError("candidates must be non-empty with unique IDs")
        unknown = sorted(
            {
                demand_id
                for candidate in candidates
                for demand_id in candidate.evidence_families_by_demand
                if demand_id not in demand_ids
            }
        )
        if unknown:
            raise ValueError(f"candidate evidence references unknown demands: {unknown}")
        object.__setattr__(self, "demands", demands)
        object.__setattr__(self, "candidates", candidates)


def validate_precompute_truth_contract(artifact: Mapping[str, Any]) -> None:
    """Validate the provenance/truth envelope consumed by static solvers.

    The original demo cache and a future file-backed exact-replay cache share a
    row shape.  This gate keeps those evidence classes explicit and prevents a
    caller from stripping the provenance fields, recomputing ``artifactSha256``,
    and then passing the remaining coverage fractions to a solver.

    Cross-file hashes and Stage-0 authority are validated by
    :mod:`singapore_sensor_fusion.placement.evidence_intake`; this function is
    intentionally the lower-level invariant that every solver entry point can
    enforce without creating an import cycle.
    """

    expected_keys = {
        "schemaVersion",
        "revision",
        "executionClass",
        "sourceClass",
        "scoreSemantics",
        "bindings",
        "candidates",
        "trajectories",
        "results",
        "allCandidateTrajectorySensorRowsPresent",
        "exactSimulatorReplayComplete",
        "measuredCalibrationUsed",
        "rfGeometryQueried",
        "fieldPerformanceClaimed",
        "physicalDeploymentAuthorized",
        "artifactSha256",
    }
    actual_keys = set(artifact)
    if actual_keys != expected_keys:
        missing = sorted(expected_keys - actual_keys)
        unexpected = sorted(actual_keys - expected_keys)
        raise ValueError(
            "precompute truth envelope has scope drift; "
            f"missing={missing}, unexpected={unexpected}"
        )
    if artifact.get("schemaVersion") != CANDIDATE_TRAJECTORY_PRECOMPUTE_SCHEMA:
        raise ValueError("unexpected precompute schemaVersion")
    revision = artifact.get("revision")
    if isinstance(revision, bool) or not isinstance(revision, int) or revision <= 0:
        raise ValueError("precompute revision must be a positive integer")
    bindings = _require_mapping("artifact.bindings", artifact.get("bindings"))
    candidate_rows = _require_sequence("artifact.candidates", artifact.get("candidates"))
    trajectory_rows = _require_sequence(
        "artifact.trajectories", artifact.get("trajectories")
    )
    results = _require_sequence("artifact.results", artifact.get("results"))

    for flag in (
        "allCandidateTrajectorySensorRowsPresent",
        "exactSimulatorReplayComplete",
        "measuredCalibrationUsed",
        "rfGeometryQueried",
        "fieldPerformanceClaimed",
        "physicalDeploymentAuthorized",
    ):
        if not isinstance(artifact.get(flag), bool):
            raise TypeError(f"precompute {flag} must be boolean")
    if artifact["allCandidateTrajectorySensorRowsPresent"] is not True:
        raise ValueError("precompute must declare a complete candidate/trajectory row grid")
    if artifact["fieldPerformanceClaimed"] is not False:
        raise ValueError("precompute intake must not claim field performance")
    if artifact["physicalDeploymentAuthorized"] is not False:
        raise ValueError("precompute intake must not authorize physical deployment")

    execution_class = _text("executionClass", artifact.get("executionClass"))
    if execution_class == "DEMO_STUDY_ONLY":
        expected_source = DEMO_SURROGATE_SOURCE
        expected_semantics = "SYNTHETIC_DEMO_EVIDENCE_INDEX_NOT_A_PROBABILITY"
        expected_exact = False
        expected_measured = False
        expected_rf = False
    elif execution_class == STRICT_EVIDENCE_EXECUTION_CLASS:
        expected_source = EXACT_SIMULATOR_REPLAY_SOURCE
        expected_semantics = "MEASURED_CALIBRATION_WITHIN_DECLARED_LIMITS"
        expected_exact = True
        expected_measured = True
        expected_rf = True
    else:
        raise ValueError(f"unsupported precompute executionClass {execution_class!r}")
    if artifact.get("sourceClass") != expected_source:
        raise ValueError("precompute sourceClass does not match its execution class")
    if artifact.get("scoreSemantics") != expected_semantics:
        raise ValueError("precompute scoreSemantics does not match its execution class")
    if artifact["exactSimulatorReplayComplete"] is not expected_exact:
        raise ValueError("precompute exact-simulator truth flag is inconsistent")
    if artifact["measuredCalibrationUsed"] is not expected_measured:
        raise ValueError("precompute measured-calibration truth flag is inconsistent")
    if artifact["rfGeometryQueried"] is not expected_rf:
        raise ValueError("precompute RF-geometry truth flag is inconsistent")

    if execution_class == STRICT_EVIDENCE_EXECUTION_CLASS:
        binding_keys = {
            "stage0ContractSha256",
            "stage0ContractFileSha256",
            "nativeRfReceiptFileSha256",
            "nativeRfRunToken",
            "calibrationBundleFileSha256",
            "calibrationDatasetFileSha256ByHardwareId",
            "candidateCatalogFileSha256",
            "trajectoryCorpusFileSha256",
            "scenarioPartitionFileSha256ById",
            "evaluationEvidenceFileSha256ByHardwareId",
        }
        if set(bindings) != binding_keys:
            missing = sorted(binding_keys - set(bindings))
            unexpected = sorted(set(bindings) - binding_keys)
            raise ValueError(
                "strict precompute bindings have scope drift; "
                f"missing={missing}, unexpected={unexpected}"
            )
        for field in (
            "stage0ContractSha256",
            "stage0ContractFileSha256",
            "nativeRfReceiptFileSha256",
            "calibrationBundleFileSha256",
            "candidateCatalogFileSha256",
            "trajectoryCorpusFileSha256",
        ):
            raw_digest = _text(f"bindings.{field}", bindings[field])
            if raw_digest != raw_digest.lower() or not _SHA256_RE.fullmatch(raw_digest):
                raise ValueError(f"bindings.{field} must be a lowercase SHA-256 digest")
        _text("bindings.nativeRfRunToken", bindings["nativeRfRunToken"])
        for field in (
            "calibrationDatasetFileSha256ByHardwareId",
            "scenarioPartitionFileSha256ById",
            "evaluationEvidenceFileSha256ByHardwareId",
        ):
            digest_map = _require_mapping(f"bindings.{field}", bindings[field])
            if not digest_map:
                raise ValueError(f"bindings.{field} must not be empty")
            for key, raw_digest in digest_map.items():
                _text(f"bindings.{field} key", key)
                digest = _text(f"bindings.{field}[{key!r}]", raw_digest)
                if digest != digest.lower() or not _SHA256_RE.fullmatch(digest):
                    raise ValueError(
                        f"bindings.{field}[{key!r}] must be a lowercase SHA-256 digest"
                    )

        candidate_keys = {
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
        }
        candidate_ids: list[str] = []
        for index, raw_candidate in enumerate(candidate_rows):
            candidate = _require_mapping(
                f"artifact.candidates[{index}]", raw_candidate
            )
            if set(candidate) != candidate_keys:
                raise ValueError(
                    f"artifact.candidates[{index}] has scope drift; "
                    f"missing={sorted(candidate_keys - set(candidate))}, "
                    f"unexpected={sorted(set(candidate) - candidate_keys)}"
                )
            candidate_id = _text("candidateId", candidate["candidateId"])
            candidate_ids.append(candidate_id)
            for field in ("siteId", "mountRegionId", "failureDomainId"):
                _text(field, candidate[field])
            pose = _require_sequence("poseEnuMeters", candidate["poseEnuMeters"])
            if len(pose) != 3:
                raise ValueError("strict candidate poseEnuMeters must have three values")
            [_finite("poseEnuMeters[]", value) for value in pose]
            if _finite("heightAglMeters", candidate["heightAglMeters"]) < 0.0:
                raise ValueError("heightAglMeters must be >= 0")
            _finite("yawDegrees", candidate["yawDegrees"])
            if _finite("costUnits", candidate["costUnits"]) < 0.0:
                raise ValueError("costUnits must be >= 0")
            classes = [
                _text("sensorClasses[]", value)
                for value in _require_sequence(
                    "sensorClasses", candidate["sensorClasses"]
                )
            ]
            if (
                not classes
                or len(classes) != len(set(classes))
                or not set(classes) <= set(SENSOR_CLASSES)
            ):
                raise ValueError("strict candidate sensorClasses are invalid")
            for field in (
                "hardwareIdsBySensorClass",
                "hardwareRevisionsBySensorClass",
            ):
                values = _require_mapping(field, candidate[field])
                if set(values) != set(classes):
                    raise ValueError(f"{field} must exactly match sensorClasses")
                for sensor_class, value in values.items():
                    _text(f"{field}[{sensor_class!r}]", value)
        if not candidate_ids or candidate_ids != sorted(set(candidate_ids)):
            raise ValueError("strict candidates must be non-empty, unique, and sorted")

        trajectory_keys = {
            "trajectoryId",
            "scenarioId",
            "partitionId",
            "targetClassId",
            "critical",
            "rfEmitting",
            "points",
        }
        trajectory_ids: list[str] = []
        for index, raw_trajectory in enumerate(trajectory_rows):
            trajectory = _require_mapping(
                f"artifact.trajectories[{index}]", raw_trajectory
            )
            if set(trajectory) != trajectory_keys:
                raise ValueError(
                    f"artifact.trajectories[{index}] has scope drift; "
                    f"missing={sorted(trajectory_keys - set(trajectory))}, "
                    f"unexpected={sorted(set(trajectory) - trajectory_keys)}"
                )
            trajectory_ids.append(
                _text("trajectoryId", trajectory["trajectoryId"])
            )
            for field in ("scenarioId", "partitionId", "targetClassId"):
                _text(field, trajectory[field])
            for field in ("critical", "rfEmitting"):
                if not isinstance(trajectory[field], bool):
                    raise TypeError(f"trajectory {field} must be boolean")
            points = _require_sequence("trajectory.points", trajectory["points"])
            if len(points) < 2:
                raise ValueError("strict trajectory must contain at least two points")
            previous_time: float | None = None
            for point_index, raw_point in enumerate(points):
                point = _require_mapping(
                    f"trajectory.points[{point_index}]", raw_point
                )
                if set(point) != {"timeSeconds", "enuMeters"}:
                    raise ValueError("strict trajectory point has scope drift")
                time = _finite("timeSeconds", point["timeSeconds"])
                if time < 0.0 or (
                    previous_time is not None and time <= previous_time
                ):
                    raise ValueError(
                        "strict trajectory point times must be nonnegative and increasing"
                    )
                previous_time = time
                coordinates = _require_sequence("enuMeters", point["enuMeters"])
                if len(coordinates) != 3:
                    raise ValueError("strict trajectory enuMeters must have three values")
                [_finite("enuMeters[]", value) for value in coordinates]
        if not trajectory_ids or len(trajectory_ids) != len(set(trajectory_ids)):
            raise ValueError("strict trajectories must be non-empty and unique")

    base_row_keys = {
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
    }
    strict_row_keys = base_row_keys | {
        "hardwareId",
        "hardwareRevision",
        "calibrationId",
        "evaluatorEvidenceFileSha256",
    }
    for index, raw in enumerate(results):
        row = _require_mapping(f"artifact.results[{index}]", raw)
        expected_row_keys = (
            strict_row_keys
            if execution_class == STRICT_EVIDENCE_EXECUTION_CLASS
            else base_row_keys
        )
        if set(row) != expected_row_keys:
            missing = sorted(expected_row_keys - set(row))
            unexpected = sorted(set(row) - expected_row_keys)
            raise ValueError(
                f"artifact.results[{index}] has scope drift; "
                f"missing={missing}, unexpected={unexpected}"
            )
        sensor_class = _text("sensorClass", row.get("sensorClass"))
        if sensor_class not in SENSOR_CLASSES:
            raise ValueError(f"unknown result sensorClass {sensor_class!r}")
        if row.get("sourceClass") != expected_source:
            raise ValueError("precompute result sourceClass is inconsistent")
        for flag in ("exactSimulatorReplay", "measuredCalibration", "rfGeometryQueried"):
            if not isinstance(row.get(flag), bool):
                raise TypeError(f"precompute result {flag} must be boolean")
        if row["exactSimulatorReplay"] is not expected_exact:
            raise ValueError("precompute result exactSimulatorReplay is inconsistent")
        if row["measuredCalibration"] is not expected_measured:
            raise ValueError("precompute result measuredCalibration is inconsistent")
        expected_row_rf = (
            sensor_class in ("radar", "passive_rf")
            if execution_class == STRICT_EVIDENCE_EXECUTION_CLASS
            else False
        )
        if row["rfGeometryQueried"] is not expected_row_rf:
            raise ValueError("precompute result rfGeometryQueried is inconsistent")
        if execution_class == STRICT_EVIDENCE_EXECUTION_CLASS:
            for field in ("hardwareId", "hardwareRevision", "calibrationId"):
                _text(field, row[field])
            evaluator_sha = _text(
                "evaluatorEvidenceFileSha256",
                row["evaluatorEvidenceFileSha256"],
            )
            if (
                evaluator_sha != evaluator_sha.lower()
                or not _SHA256_RE.fullmatch(evaluator_sha)
            ):
                raise ValueError(
                    "evaluatorEvidenceFileSha256 must be a lowercase SHA-256 digest"
                )
        fraction = _unit("detectionFraction", row.get("detectionFraction"))
        _unit("meanQuality", row.get("meanQuality"))
        first_detection = row.get("firstDetectionSeconds")
        if first_detection is None:
            if fraction > 0.0:
                raise ValueError(
                    "positive detectionFraction requires firstDetectionSeconds"
                )
        else:
            first = _finite("firstDetectionSeconds", first_detection)
            if first < 0.0:
                raise ValueError("firstDetectionSeconds must be >= 0")
            if fraction <= 0.0:
                raise ValueError(
                    "firstDetectionSeconds requires positive detectionFraction"
                )


def static_problem_from_precompute(
    artifact: Mapping[str, Any],
    *,
    minimum_detection_fraction: float,
    maximum_sites: int,
    minimum_evidence_families: int = 2,
    minimum_site_redundancy: int = 1,
    minimum_failure_domain_redundancy: int = 1,
    minimum_scenario_coverage_fraction: float = 1.0,
    budget_units: float | None = None,
) -> RobustStaticProblem:
    """Translate a validated precompute artifact into the robust set model."""

    validate_precompute_truth_contract(artifact)
    expected_digest = artifact.get("artifactSha256")
    without_digest = dict(_json_ready(artifact))
    without_digest.pop("artifactSha256", None)
    if expected_digest != stable_digest(without_digest):
        raise ValueError("precompute artifactSha256 does not match its content")
    if artifact.get("executionClass") == STRICT_EVIDENCE_EXECUTION_CLASS:
        raise ValueError(
            "strict structural evidence intake is not a static-solver authorization; "
            "a robust-outcome validator covering the frozen Stage-0 requirements is required"
        )
    threshold = _unit("minimum_detection_fraction", minimum_detection_fraction)
    trajectory_rows = _require_sequence("artifact.trajectories", artifact.get("trajectories"))
    demand_by_trajectory: dict[str, RobustDemand] = {}
    trajectory_metadata: dict[str, Mapping[str, Any]] = {}
    for index, raw in enumerate(trajectory_rows):
        row = _require_mapping(f"artifact.trajectories[{index}]", raw)
        trajectory_id = _text("trajectoryId", row.get("trajectoryId"))
        if trajectory_id in demand_by_trajectory:
            raise ValueError(f"duplicate trajectoryId {trajectory_id!r}")
        trajectory_metadata[trajectory_id] = row
        demand_by_trajectory[trajectory_id] = RobustDemand(
            demand_id=trajectory_id,
            scenario_id=_text("scenarioId", row.get("scenarioId")),
            critical=row.get("critical") is True,
        )

    candidate_rows = _require_sequence("artifact.candidates", artifact.get("candidates"))
    metadata: dict[str, Mapping[str, Any]] = {}
    for index, raw in enumerate(candidate_rows):
        row = _require_mapping(f"artifact.candidates[{index}]", raw)
        candidate_id = _text("candidateId", row.get("candidateId"))
        if candidate_id in metadata:
            raise ValueError(f"duplicate candidateId {candidate_id!r}")
        metadata[candidate_id] = row

    family_sets: dict[str, dict[str, set[str]]] = {
        candidate_id: {} for candidate_id in metadata
    }
    expected_result_keys = {
        (candidate_id, trajectory_id, sensor_class)
        for candidate_id, candidate in metadata.items()
        for trajectory_id in demand_by_trajectory
        for sensor_class in _require_sequence(
            f"candidate {candidate_id}.sensorClasses", candidate.get("sensorClasses")
        )
    }
    observed_result_keys: set[tuple[str, str, str]] = set()
    results = _require_sequence("artifact.results", artifact.get("results"))
    for index, raw in enumerate(results):
        row = _require_mapping(f"artifact.results[{index}]", raw)
        candidate_id = _text("candidateId", row.get("candidateId"))
        trajectory_id = _text("trajectoryId", row.get("trajectoryId"))
        if candidate_id not in metadata or trajectory_id not in demand_by_trajectory:
            raise ValueError("result references an unknown candidate or trajectory")
        sensor_class = _text("sensorClass", row.get("sensorClass"))
        key = (candidate_id, trajectory_id, sensor_class)
        if key in observed_result_keys:
            raise ValueError(f"duplicate precompute result key {key!r}")
        observed_result_keys.add(key)
        if row.get("scenarioId") != trajectory_metadata[trajectory_id].get("scenarioId"):
            raise ValueError("result scenarioId does not match its trajectory")
        if row.get("partitionId") != trajectory_metadata[trajectory_id].get("partitionId"):
            raise ValueError("result partitionId does not match its trajectory")
        if sensor_class not in SENSOR_FAMILIES:
            raise ValueError(f"unknown result sensorClass {sensor_class!r}")
        if row.get("evidenceFamily") != SENSOR_FAMILIES[sensor_class]:
            raise ValueError("result evidenceFamily does not match its sensor class")
        fraction = _unit("detectionFraction", row.get("detectionFraction"))
        if fraction + 1e-12 >= threshold:
            family_sets[candidate_id].setdefault(trajectory_id, set()).add(
                _text("evidenceFamily", row.get("evidenceFamily"))
            )
    if observed_result_keys != expected_result_keys:
        missing = sorted(expected_result_keys - observed_result_keys)
        extra = sorted(observed_result_keys - expected_result_keys)
        raise ValueError(
            f"precompute result grid is incomplete or out of scope; missing={missing}, extra={extra}"
        )

    candidates_out = tuple(
        StaticCandidate(
            candidate_id=candidate_id,
            site_id=_text("siteId", row.get("siteId")),
            failure_domain_id=_text(
                "failureDomainId", row.get("failureDomainId")
            ),
            cost_units=_finite("costUnits", row.get("costUnits")),
            evidence_families_by_demand={
                demand_id: tuple(sorted(families))
                for demand_id, families in family_sets[candidate_id].items()
            },
        )
        for candidate_id, row in sorted(metadata.items())
    )
    return RobustStaticProblem(
        problem_id=f"precompute:{artifact['artifactSha256']}",
        demands=tuple(demand_by_trajectory.values()),
        candidates=candidates_out,
        maximum_sites=maximum_sites,
        minimum_evidence_families=minimum_evidence_families,
        minimum_site_redundancy=minimum_site_redundancy,
        minimum_failure_domain_redundancy=minimum_failure_domain_redundancy,
        minimum_scenario_coverage_fraction=minimum_scenario_coverage_fraction,
        budget_units=budget_units,
    )


def evaluate_static_selection(
    problem: RobustStaticProblem, selected_candidate_ids: Iterable[str]
) -> dict[str, object]:
    selected_ids = tuple(sorted(set(selected_candidate_ids)))
    candidate_by_id = {item.candidate_id: item for item in problem.candidates}
    missing = [item for item in selected_ids if item not in candidate_by_id]
    if missing:
        raise ValueError(f"unknown selected candidate IDs: {missing}")
    selected = [candidate_by_id[item] for item in selected_ids]
    if len({item.site_id for item in selected}) != len(selected):
        raise ValueError("at most one candidate may be selected per physical site")
    if len(selected) > problem.maximum_sites:
        raise ValueError("selection exceeds maximum_sites")
    cost = sum(item.cost_units for item in selected)
    if problem.budget_units is not None and cost > problem.budget_units + 1e-9:
        raise ValueError("selection exceeds budget_units")

    by_scenario: dict[str, list[RobustDemand]] = {}
    for demand in problem.demands:
        by_scenario.setdefault(demand.scenario_id, []).append(demand)
    scenario_metrics: dict[str, dict[str, object]] = {}
    uncovered_critical: list[str] = []
    total_weight = 0.0
    covered_weight = 0.0
    progress_weight = 0.0
    for scenario_id, demands in sorted(by_scenario.items()):
        scenario_total = sum(item.weight for item in demands)
        scenario_covered = 0.0
        scenario_progress = 0.0
        for demand in demands:
            contributors = [
                item
                for item in selected
                if demand.demand_id in item.evidence_families_by_demand
            ]
            families = {
                family
                for candidate in contributors
                for family in candidate.evidence_families_by_demand[demand.demand_id]
            }
            sites = {item.site_id for item in contributors}
            domains = {item.failure_domain_id for item in contributors}
            family_ratio = min(
                1.0, len(families) / problem.minimum_evidence_families
            )
            site_ratio = min(1.0, len(sites) / problem.minimum_site_redundancy)
            domain_ratio = min(
                1.0,
                len(domains) / problem.minimum_failure_domain_redundancy,
            )
            progress = min(family_ratio, site_ratio, domain_ratio)
            scenario_progress += demand.weight * progress
            is_covered = progress >= 1.0 - 1e-12
            if is_covered:
                scenario_covered += demand.weight
            elif demand.critical:
                uncovered_critical.append(demand.demand_id)
        fraction = scenario_covered / scenario_total
        progress_fraction = scenario_progress / scenario_total
        scenario_metrics[scenario_id] = {
            "coverageFraction": round(fraction, 9),
            "progressFraction": round(progress_fraction, 9),
        }
        total_weight += scenario_total
        covered_weight += scenario_covered
        progress_weight += scenario_progress

    worst = min(
        (float(item["coverageFraction"]) for item in scenario_metrics.values()),
        default=0.0,
    )
    worst_progress = min(
        (float(item["progressFraction"]) for item in scenario_metrics.values()),
        default=0.0,
    )
    feasible = (
        worst + 1e-12 >= problem.minimum_scenario_coverage_fraction
        and not uncovered_critical
    )
    return {
        "coverageFeasible": feasible,
        "worstScenarioCoverageFraction": round(worst, 9),
        "weightedCoverageFraction": round(covered_weight / total_weight, 9),
        "worstScenarioProgressFraction": round(worst_progress, 9),
        "weightedProgressFraction": round(progress_weight / total_weight, 9),
        "coverageByScenario": scenario_metrics,
        "uncoveredCriticalDemandIds": sorted(set(uncovered_critical)),
        "selectedSiteCount": len(selected),
        "selectedFailureDomainCount": len(
            {item.failure_domain_id for item in selected}
        ),
        "totalCostUnits": round(cost, 9),
        "policy": {
            "maximumSites": problem.maximum_sites,
            "minimumEvidenceFamilies": problem.minimum_evidence_families,
            "minimumSiteRedundancy": problem.minimum_site_redundancy,
            "minimumFailureDomainRedundancy": problem.minimum_failure_domain_redundancy,
            "minimumScenarioCoverageFraction": problem.minimum_scenario_coverage_fraction,
            "budgetUnits": problem.budget_units,
        },
    }


def _static_key(
    metrics: Mapping[str, object], selected_ids: tuple[str, ...]
) -> tuple[float, ...]:
    cost = float(metrics["totalCostUnits"])
    if metrics["coverageFeasible"] is True:
        return (
            1.0,
            -cost,
            -float(len(selected_ids)),
            float(metrics["worstScenarioCoverageFraction"]),
            float(metrics["weightedCoverageFraction"]),
        )
    return (
        0.0,
        float(metrics["worstScenarioProgressFraction"]),
        float(metrics["weightedProgressFraction"]),
        float(metrics["worstScenarioCoverageFraction"]),
        float(metrics["weightedCoverageFraction"]),
        -cost,
        -float(len(selected_ids)),
    )


def _static_allowed(
    problem: RobustStaticProblem,
    candidate_by_id: Mapping[str, StaticCandidate],
    selected_ids: tuple[str, ...],
) -> bool:
    if len(selected_ids) > problem.maximum_sites:
        return False
    selected = [candidate_by_id[item] for item in selected_ids]
    if len({item.site_id for item in selected}) != len(selected):
        return False
    cost = sum(item.cost_units for item in selected)
    return problem.budget_units is None or cost <= problem.budget_units + 1e-9


@dataclass(frozen=True, slots=True)
class StaticSolverResult:
    status: str
    algorithm: str
    selected_candidate_ids: tuple[str, ...]
    metrics: Mapping[str, object]
    explored_node_count: int
    node_limit: int | None
    optimality_proven: bool
    infeasibility_proven: bool
    termination_reason: str
    incumbent_objective_cost_units: float | None
    objective_cost_lower_bound_units: float | None
    absolute_cost_optimality_gap_units: float | None
    relative_cost_optimality_gap: float | None
    bound_semantics: str

    def to_dict(self) -> dict[str, object]:
        payload: dict[str, object] = {
            "schemaVersion": STATIC_SOLVER_RESULT_SCHEMA,
            "status": self.status,
            "algorithm": self.algorithm,
            "selectedCandidateIds": list(self.selected_candidate_ids),
            "metrics": _json_ready(self.metrics),
            "exploredNodeCount": self.explored_node_count,
            "nodeLimit": self.node_limit,
            "nodeLimitReached": self.termination_reason.startswith("NODE_LIMIT"),
            "optimalityProven": self.optimality_proven,
            "infeasibilityProven": self.infeasibility_proven,
            "terminationReason": self.termination_reason,
            "objective": "MINIMIZE_TOTAL_COST_UNITS_WITH_DETERMINISTIC_LEXICOGRAPHIC_TIE_BREAKS",
            "incumbentObjectiveCostUnits": self.incumbent_objective_cost_units,
            "objectiveCostLowerBoundUnits": self.objective_cost_lower_bound_units,
            "absoluteCostOptimalityGapUnits": self.absolute_cost_optimality_gap_units,
            "relativeCostOptimalityGap": self.relative_cost_optimality_gap,
            "boundSemantics": self.bound_semantics,
            "scoreSemantics": "PRECOMPUTED_SIMULATION_EVIDENCE_INDEX",
            "physicalDeploymentAuthorized": False,
        }
        payload["resultSha256"] = stable_digest(payload)
        return payload


def solve_greedy_local_search(problem: RobustStaticProblem) -> StaticSolverResult:
    """Deterministic greedy construction, removal, and one-for-one swaps."""

    candidate_by_id = {item.candidate_id: item for item in problem.candidates}
    selected: tuple[str, ...] = ()
    metrics = evaluate_static_selection(problem, selected)
    while len(selected) < problem.maximum_sites and not metrics["coverageFeasible"]:
        incumbent_key = _static_key(metrics, selected)
        best: tuple[tuple[float, ...], tuple[str, ...], dict[str, object]] | None = None
        for candidate in problem.candidates:
            if candidate.candidate_id in selected:
                continue
            ids = tuple(sorted((*selected, candidate.candidate_id)))
            if not _static_allowed(problem, candidate_by_id, ids):
                continue
            candidate_metrics = evaluate_static_selection(problem, ids)
            key = _static_key(candidate_metrics, ids)
            row = (key, ids, candidate_metrics)
            if best is None or key > best[0] or (key == best[0] and ids < best[1]):
                best = row
        if best is None or best[0] <= incumbent_key:
            break
        _, selected, metrics = best

    changed = True
    while changed:
        changed = False
        incumbent_key = _static_key(metrics, selected)
        alternatives: list[tuple[tuple[float, ...], tuple[str, ...], dict[str, object]]] = []
        for removed in selected:
            ids = tuple(item for item in selected if item != removed)
            candidate_metrics = evaluate_static_selection(problem, ids)
            alternatives.append((_static_key(candidate_metrics, ids), ids, candidate_metrics))
        for removed in selected:
            retained = tuple(item for item in selected if item != removed)
            for added in problem.candidates:
                if added.candidate_id in selected:
                    continue
                ids = tuple(sorted((*retained, added.candidate_id)))
                if not _static_allowed(problem, candidate_by_id, ids):
                    continue
                candidate_metrics = evaluate_static_selection(problem, ids)
                alternatives.append((_static_key(candidate_metrics, ids), ids, candidate_metrics))
        if alternatives:
            best_key = max(item[0] for item in alternatives)
            best = min(
                (item for item in alternatives if item[0] == best_key),
                key=lambda item: item[1],
            )
            if best[0] > incumbent_key:
                _, selected, metrics = best
                changed = True

    return StaticSolverResult(
        status="FEASIBLE" if metrics["coverageFeasible"] else "UNRESOLVED",
        algorithm=GREEDY_LOCAL_ALGORITHM,
        selected_candidate_ids=selected,
        metrics=MappingProxyType(metrics),
        explored_node_count=0,
        node_limit=None,
        optimality_proven=False,
        infeasibility_proven=False,
        termination_reason=(
            "HEURISTIC_FEASIBLE_LOCAL_OPTIMUM"
            if metrics["coverageFeasible"]
            else "HEURISTIC_STALLED_WITHOUT_FEASIBLE_LAYOUT"
        ),
        incumbent_objective_cost_units=(
            float(metrics["totalCostUnits"])
            if metrics["coverageFeasible"]
            else None
        ),
        objective_cost_lower_bound_units=None,
        absolute_cost_optimality_gap_units=None,
        relative_cost_optimality_gap=None,
        bound_semantics="NOT_APPLICABLE_TO_HEURISTIC_BASELINE",
    )


def _global_structural_cost_lower_bound(problem: RobustStaticProblem) -> float:
    """Return a conservative cost bound that remains valid at any node limit.

    Any non-vacuous feasible layout must contain enough distinct contributors
    to satisfy both the site- and failure-domain-redundancy requirements.  The
    sum of the same number of globally cheapest candidate costs is therefore a
    (possibly loose) lower bound even when those cheapest candidates share a
    site or domain.  Evidence-family and coverage constraints can only raise
    the true optimum, never lower this bound.
    """

    requires_coverage = (
        problem.minimum_scenario_coverage_fraction > 0.0
        or any(item.critical for item in problem.demands)
    )
    if not requires_coverage:
        return 0.0
    minimum_contributors = max(
        1,
        problem.minimum_site_redundancy,
        problem.minimum_failure_domain_redundancy,
    )
    costs = sorted(item.cost_units for item in problem.candidates)
    if len(costs) < minimum_contributors:
        # This case cannot be feasible, but zero is still a valid bound for a
        # node-limited search that has not yet emitted an infeasibility proof.
        return 0.0
    return float(sum(costs[:minimum_contributors]))


def _cost_gap(
    incumbent_cost_units: float | None,
    lower_bound_cost_units: float | None,
) -> tuple[float | None, float | None]:
    if incumbent_cost_units is None or lower_bound_cost_units is None:
        return None, None
    # Clamp only tiny floating-point disagreement; a reported lower bound must
    # never exceed the incumbent it is bounding.
    lower = min(incumbent_cost_units, lower_bound_cost_units)
    absolute = max(0.0, incumbent_cost_units - lower)
    relative = (
        0.0
        if absolute <= 1e-15
        else absolute / max(abs(incumbent_cost_units), 1e-15)
    )
    return absolute, relative


def solve_robust_branch_and_bound(
    problem: RobustStaticProblem, *, node_limit: int = 250_000
) -> StaticSolverResult:
    """Solve the robust static subset problem with a dependency-free bound.

    A completed search returns an optimality or infeasibility proof for this
    precomputed binary model.  Hitting ``node_limit`` is reported as bounded,
    never disguised as proof.
    """

    if isinstance(node_limit, bool) or not isinstance(node_limit, int) or node_limit <= 0:
        raise ValueError("node_limit must be a positive integer")
    candidate_by_id = {item.candidate_id: item for item in problem.candidates}
    ordered = tuple(
        sorted(
            problem.candidates,
            key=lambda item: (
                -sum(len(value) for value in item.evidence_families_by_demand.values()),
                item.cost_units,
                item.candidate_id,
            ),
        )
    )
    greedy = solve_greedy_local_search(problem)
    best_ids: tuple[str, ...] | None = (
        greedy.selected_candidate_ids if greedy.status == "FEASIBLE" else None
    )
    best_metrics: dict[str, object] | None = (
        dict(greedy.metrics) if best_ids is not None else None
    )
    explored = 0
    limit_hit = False

    def better(ids: tuple[str, ...], metrics: Mapping[str, object]) -> bool:
        nonlocal best_ids, best_metrics
        if best_ids is None or best_metrics is None:
            return True
        key = _static_key(metrics, ids)
        incumbent = _static_key(best_metrics, best_ids)
        return key > incumbent or (key == incumbent and ids < best_ids)

    def optimistic_feasible(selected: tuple[str, ...], start: int) -> bool:
        # Combine every remaining option, including mutually exclusive options
        # at one site. That deliberate relaxation can only overestimate future
        # evidence and is therefore safe for an infeasibility prune.
        pool = [candidate_by_id[item] for item in selected] + list(ordered[start:])
        scenario_totals: dict[str, float] = {}
        scenario_covered: dict[str, float] = {}
        for demand in problem.demands:
            scenario_totals[demand.scenario_id] = (
                scenario_totals.get(demand.scenario_id, 0.0) + demand.weight
            )
            contributors = [
                candidate
                for candidate in pool
                if demand.demand_id in candidate.evidence_families_by_demand
            ]
            families = {
                family
                for candidate in contributors
                for family in candidate.evidence_families_by_demand[demand.demand_id]
            }
            covered = (
                len(families) >= problem.minimum_evidence_families
                and len({item.site_id for item in contributors})
                >= problem.minimum_site_redundancy
                and len({item.failure_domain_id for item in contributors})
                >= problem.minimum_failure_domain_redundancy
            )
            if covered:
                scenario_covered[demand.scenario_id] = (
                    scenario_covered.get(demand.scenario_id, 0.0) + demand.weight
                )
            elif demand.critical:
                return False
        return all(
            scenario_covered.get(scenario_id, 0.0) / total + 1e-12
            >= problem.minimum_scenario_coverage_fraction
            for scenario_id, total in scenario_totals.items()
        )

    def visit(index: int, selected: tuple[str, ...]) -> None:
        nonlocal explored, limit_hit, best_ids, best_metrics
        if limit_hit:
            return
        if explored >= node_limit:
            limit_hit = True
            return
        explored += 1
        if not _static_allowed(problem, candidate_by_id, selected):
            return
        cost = sum(candidate_by_id[item].cost_units for item in selected)
        if best_metrics is not None and cost > float(best_metrics["totalCostUnits"]) + 1e-9:
            return
        metrics = evaluate_static_selection(problem, selected)
        if metrics["coverageFeasible"]:
            if better(selected, metrics):
                best_ids, best_metrics = selected, metrics
            return
        if index >= len(ordered) or len(selected) >= problem.maximum_sites:
            return
        if not optimistic_feasible(selected, index):
            return
        candidate = ordered[index]
        include_ids = tuple(sorted((*selected, candidate.candidate_id)))
        visit(index + 1, include_ids)
        visit(index + 1, selected)

    visit(0, ())
    completed = not limit_hit
    if best_ids is None or best_metrics is None:
        empty_metrics = evaluate_static_selection(problem, ())
        lower_bound = None if completed else _global_structural_cost_lower_bound(problem)
        return StaticSolverResult(
            status="INFEASIBLE_PROVEN" if completed else "UNRESOLVED_NODE_LIMIT",
            algorithm=BRANCH_AND_BOUND_ALGORITHM,
            selected_candidate_ids=(),
            metrics=MappingProxyType(empty_metrics),
            explored_node_count=explored,
            node_limit=node_limit,
            optimality_proven=False,
            infeasibility_proven=completed,
            termination_reason=(
                "SEARCH_EXHAUSTED_INFEASIBLE"
                if completed
                else "NODE_LIMIT_WITHOUT_FEASIBLE_INCUMBENT"
            ),
            incumbent_objective_cost_units=None,
            objective_cost_lower_bound_units=lower_bound,
            absolute_cost_optimality_gap_units=None,
            relative_cost_optimality_gap=None,
            bound_semantics=(
                "NOT_APPLICABLE_INFEASIBILITY_PROVEN"
                if completed
                else "GLOBAL_STRUCTURAL_MINIMUM_SELECTION_COST"
            ),
        )
    incumbent_cost = sum(candidate_by_id[item].cost_units for item in best_ids)
    lower_bound = (
        incumbent_cost
        if completed
        else _global_structural_cost_lower_bound(problem)
    )
    absolute_gap, relative_gap = _cost_gap(incumbent_cost, lower_bound)
    return StaticSolverResult(
        status="OPTIMAL" if completed else "FEASIBLE_NODE_LIMIT",
        algorithm=BRANCH_AND_BOUND_ALGORITHM,
        selected_candidate_ids=best_ids,
        metrics=MappingProxyType(best_metrics),
        explored_node_count=explored,
        node_limit=node_limit,
        optimality_proven=completed,
        infeasibility_proven=False,
        termination_reason=(
            "SEARCH_EXHAUSTED_OPTIMAL"
            if completed
            else "NODE_LIMIT_WITH_FEASIBLE_INCUMBENT"
        ),
        incumbent_objective_cost_units=incumbent_cost,
        objective_cost_lower_bound_units=lower_bound,
        absolute_cost_optimality_gap_units=absolute_gap,
        relative_cost_optimality_gap=relative_gap,
        bound_semantics=(
            "MATCHED_INCUMBENT_BY_EXHAUSTIVE_SEARCH"
            if completed
            else "GLOBAL_STRUCTURAL_MINIMUM_SELECTION_COST"
        ),
    )


@dataclass(frozen=True, slots=True)
class PoseBounds:
    lower: tuple[float, ...]
    upper: tuple[float, ...]

    def __post_init__(self) -> None:
        lower = tuple(_finite("lower[]", item) for item in self.lower)
        upper = tuple(_finite("upper[]", item) for item in self.upper)
        if not lower or len(lower) != len(upper):
            raise ValueError("lower and upper must be non-empty and have equal dimensions")
        if any(left >= right for left, right in zip(lower, upper, strict=True)):
            raise ValueError("every lower bound must be less than its upper bound")
        object.__setattr__(self, "lower", lower)
        object.__setattr__(self, "upper", upper)

    def clip(self, pose: Sequence[float]) -> tuple[float, ...]:
        if len(pose) != len(self.lower):
            raise ValueError("pose dimension does not match bounds")
        return tuple(
            min(right, max(left, _finite("pose[]", value)))
            for value, left, right in zip(pose, self.lower, self.upper, strict=True)
        )


@dataclass(frozen=True, slots=True)
class PoseRefinementResult:
    algorithm: str
    best_pose: tuple[float, ...]
    best_score: float
    evaluation_count: int
    evaluation_budget: int
    seed: int | None
    trace_sha256: str

    def to_dict(self) -> dict[str, object]:
        return {
            "algorithm": self.algorithm,
            "bestPose": list(self.best_pose),
            "bestScore": self.best_score,
            "evaluationCount": self.evaluation_count,
            "evaluationBudget": self.evaluation_budget,
            "seed": self.seed,
            "traceSha256": self.trace_sha256,
        }


def _validate_refinement_inputs(
    initial_pose: Sequence[float], bounds: PoseBounds, evaluation_budget: int
) -> tuple[float, ...]:
    if isinstance(evaluation_budget, bool) or not isinstance(evaluation_budget, int) or evaluation_budget <= 0:
        raise ValueError("evaluation_budget must be a positive integer")
    pose = tuple(_finite("initial_pose[]", item) for item in initial_pose)
    clipped = bounds.clip(pose)
    if clipped != pose:
        raise ValueError("initial_pose must lie inside bounds")
    return pose


def refine_pose_pattern_search(
    initial_pose: Sequence[float],
    bounds: PoseBounds,
    objective: Callable[[tuple[float, ...]], float],
    *,
    evaluation_budget: int,
) -> PoseRefinementResult:
    """Maximize a black-box score with bounded derivative-free pattern search."""

    best_pose = _validate_refinement_inputs(initial_pose, bounds, evaluation_budget)
    trace: list[tuple[tuple[float, ...], float]] = []

    def evaluate(pose: tuple[float, ...]) -> float:
        score = _finite("objective result", objective(pose))
        trace.append((pose, score))
        return score

    best_score = evaluate(best_pose)
    steps = [
        (right - left) * 0.25
        for left, right in zip(bounds.lower, bounds.upper, strict=True)
    ]
    dimension = 0
    direction = 1.0
    attempts_without_improvement = 0
    while len(trace) < evaluation_budget:
        raw = list(best_pose)
        raw[dimension] += direction * steps[dimension]
        candidate = bounds.clip(raw)
        score = evaluate(candidate)
        if score > best_score + 1e-15 or (
            abs(score - best_score) <= 1e-15 and candidate < best_pose
        ):
            best_pose, best_score = candidate, score
            attempts_without_improvement = 0
        else:
            attempts_without_improvement += 1
        direction *= -1.0
        if direction > 0.0:
            dimension = (dimension + 1) % len(best_pose)
        if attempts_without_improvement >= 2 * len(best_pose):
            steps = [max(item * 0.5, 1e-9) for item in steps]
            attempts_without_improvement = 0

    trace_payload = [
        {"pose": list(pose), "score": score} for pose, score in trace
    ]
    return PoseRefinementResult(
        algorithm=PATTERN_SEARCH_ALGORITHM,
        best_pose=best_pose,
        best_score=best_score,
        evaluation_count=len(trace),
        evaluation_budget=evaluation_budget,
        seed=None,
        trace_sha256=stable_digest(trace_payload),
    )


def refine_pose_evolutionary(
    initial_pose: Sequence[float],
    bounds: PoseBounds,
    objective: Callable[[tuple[float, ...]], float],
    *,
    evaluation_budget: int,
    seed: int,
) -> PoseRefinementResult:
    """Maximize the same black-box score with a seeded evolutionary challenger."""

    initial = _validate_refinement_inputs(initial_pose, bounds, evaluation_budget)
    if isinstance(seed, bool) or not isinstance(seed, int) or seed < 0:
        raise ValueError("seed must be an integer >= 0")
    rng = random.Random(seed)
    trace: list[tuple[tuple[float, ...], float]] = []

    def evaluate(pose: tuple[float, ...]) -> float:
        score = _finite("objective result", objective(pose))
        trace.append((pose, score))
        return score

    best_pose = initial
    best_score = evaluate(initial)
    population: list[tuple[float, tuple[float, ...]]] = [(best_score, initial)]
    while len(trace) < evaluation_budget:
        parent = population[rng.randrange(len(population))][1]
        progress = len(trace) / evaluation_budget
        sigma_scale = max(0.025, 0.30 * (1.0 - progress))
        child = bounds.clip(
            tuple(
                value + rng.gauss(0.0, (right - left) * sigma_scale)
                for value, left, right in zip(
                    parent, bounds.lower, bounds.upper, strict=True
                )
            )
        )
        score = evaluate(child)
        population.append((score, child))
        population = sorted(
            population,
            key=lambda item: (-item[0], item[1]),
        )[: max(2, min(8, evaluation_budget // 4 or 2))]
        if score > best_score + 1e-15 or (
            abs(score - best_score) <= 1e-15 and child < best_pose
        ):
            best_pose, best_score = child, score

    trace_payload = [
        {"pose": list(pose), "score": score} for pose, score in trace
    ]
    return PoseRefinementResult(
        algorithm=EVOLUTIONARY_ALGORITHM,
        best_pose=best_pose,
        best_score=best_score,
        evaluation_count=len(trace),
        evaluation_budget=evaluation_budget,
        seed=seed,
        trace_sha256=stable_digest(trace_payload),
    )


def compare_pose_refiners(
    initial_pose: Sequence[float],
    bounds: PoseBounds,
    objective: Callable[[tuple[float, ...]], float],
    *,
    evaluation_budget_per_method: int,
    evolutionary_seed: int,
) -> dict[str, object]:
    """Run both pose-only methods with exactly equal evaluation budgets."""

    pattern = refine_pose_pattern_search(
        initial_pose,
        bounds,
        objective,
        evaluation_budget=evaluation_budget_per_method,
    )
    evolutionary = refine_pose_evolutionary(
        initial_pose,
        bounds,
        objective,
        evaluation_budget=evaluation_budget_per_method,
        seed=evolutionary_seed,
    )
    payload: dict[str, object] = {
        "schemaVersion": POSE_REFINEMENT_COMPARISON_SCHEMA,
        "scope": "CONTINUOUS_POSE_REFINEMENT_ONLY",
        "equalEvaluationBudget": True,
        "evaluationBudgetPerMethod": evaluation_budget_per_method,
        "results": [pattern.to_dict(), evolutionary.to_dict()],
        "exactSimulatorReplayRequiredForAcceptance": True,
        "physicalDeploymentAuthorized": False,
    }
    payload["comparisonSha256"] = stable_digest(payload)
    return payload


__all__ = [
    "BRANCH_AND_BOUND_ALGORITHM",
    "CANDIDATE_TRAJECTORY_PRECOMPUTE_SCHEMA",
    "DEMO_SURROGATE_SOURCE",
    "EXACT_SIMULATOR_REPLAY_SOURCE",
    "EVOLUTIONARY_ALGORITHM",
    "GREEDY_LOCAL_ALGORITHM",
    "MEASURED_CALIBRATION_EVIDENCE",
    "MODEL_CALIBRATION_BUNDLE_SCHEMA",
    "PATTERN_SEARCH_ALGORITHM",
    "RF_LOAD_RECEIPT_SCHEMA",
    "SENSOR_CLASSES",
    "STATIC_SOLVER_RESULT_SCHEMA",
    "STRICT_EVIDENCE_EXECUTION_CLASS",
    "SYNTHETIC_DEMO_EVIDENCE",
    "CandidateSensorPose",
    "CandidateTrajectoryResult",
    "PoseBounds",
    "PoseRefinementResult",
    "RFGeometryLoadReceipt",
    "RobustDemand",
    "RobustStaticProblem",
    "SensorModelCalibration",
    "StaticCandidate",
    "StaticSolverResult",
    "TargetTrajectory",
    "TrajectoryPoint",
    "assess_stage0_readiness",
    "build_calibration_bundle",
    "compare_pose_refiners",
    "default_synthetic_demo_calibrations",
    "evaluate_static_selection",
    "load_and_validate_rf_geometry",
    "precompute_demo_candidate_trajectory_results",
    "refine_pose_evolutionary",
    "refine_pose_pattern_search",
    "solve_greedy_local_search",
    "solve_robust_branch_and_bound",
    "static_problem_from_precompute",
    "validate_precompute_truth_contract",
]
