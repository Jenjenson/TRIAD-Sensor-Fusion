"""Fail-closed Stage-0 contract for a sensor-placement study.

This module freezes study inputs and benchmark rules.  It deliberately does
not contain, select, or invoke a placement optimizer.  The validator rejects
unknown fields so a misspelled field or an attempted modality expansion cannot
silently change the study scope.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
from types import MappingProxyType
from typing import Any, Mapping, Sequence
from urllib.parse import unquote, urlsplit

from .contracts import stable_digest

STAGE0_STUDY_SCHEMA = "triad.sensor_placement_stage0_study_contract.v1"
STAGE0_VALIDATION_RECEIPT_SCHEMA = (
    "triad.sensor_placement_stage0_study_contract_validation.v1"
)

ALLOWED_SENSOR_CLASSES = (
    "radar",
    "passive_rf",
    "rgb",
    "event_camera",
)
PROHIBITED_DEPLOYED_MODALITIES = ("thermal", "depth")

REQUIRED_SCENARIO_DIMENSIONS = (
    "TARGET_KINEMATICS_SIZE_ASPECT_AND_RCS",
    "RF_EMITTING_AND_RF_SILENT",
    "WEATHER_ILLUMINATION_AND_WIND_VEGETATION",
    "APERTURE_STATE_AND_RF_MATERIAL_REALIZATION",
    "SENSOR_NOISE_CALIBRATION_CLOCK_AND_DROPS",
    "NETWORK_COMPUTE_AND_FAILURE_DOMAIN_OUTAGE",
    "CLUTTER_AND_NON_TARGET_TRAFFIC",
)

REQUIRED_GATE_IDS = (
    "ENVIRONMENT",
    "RF_PROPAGATION",
    "RADAR_MODEL",
    "PASSIVE_RF_MODEL",
    "RGB_MODEL",
    "EVENT_CAMERA_MODEL",
    "FUSION_POLICY",
    "EVALUATION_CORPUS",
    "MOUNT_PERMISSIONS_AND_RESOURCES",
    "PRIVACY",
)

REQUIRED_SOLVER_METHODS = (
    "GREEDY_BASELINE",
    "EXACT_MILP_OR_CP_SAT",
    "BAYESIAN_CONTINUOUS_REFINEMENT",
    "EVOLUTIONARY_MULTIOBJECTIVE",
)

REQUIRED_BENCHMARK_OUTPUTS = (
    "FEASIBILITY_AND_HARD_CONSTRAINTS",
    "UNWEIGHTED_COMPONENT_METRICS",
    "PARETO_FRONT",
    "COST_AND_RESOURCE_ACCOUNTING",
    "OUTAGE_RESILIENCE",
    "UNCERTAINTY_INTERVALS",
    "SOLVER_RUNTIME_BOUNDS_AND_GAPS",
    "FINAL_EXACT_SIMULATOR_REPLAY",
)

REQUIRED_APPROVER_ROLES = (
    "STUDY_OWNER",
    "SITE_AUTHORITY",
    "PRIVACY_REVIEWER",
    "RF_SAFETY_REVIEWER",
    "MODEL_VALIDATION_LEAD",
)

ARTIFACT_ROLES = frozenset(
    {
        "MAP",
        "AOI_ZONE_CATALOG",
        "RENDER_GEOMETRY",
        "COLLISION_GEOMETRY",
        "RF_GEOMETRY",
        "RF_MATERIAL_CATALOG",
        "RF_NATIVE_TEST_RECEIPT",
        "RADAR_SENSOR_MODEL",
        "PASSIVE_RF_SENSOR_MODEL",
        "RGB_SENSOR_MODEL",
        "EVENT_CAMERA_SENSOR_MODEL",
        "RGB_DETECTOR",
        "EVENT_CAMERA_DETECTOR",
        "FUSION_POLICY",
        "TARGET_TRAJECTORIES",
        "TRAIN_SCENARIOS",
        "VALIDATION_SCENARIOS",
        "TEST_SCENARIOS",
        "MOUNT_REGION_GEOMETRY",
        "SOLVER_CANDIDATES",
        "SENSOR_CALIBRATION",
        "ANTENNA_PATTERN",
        "MOUNT_PERMISSION",
        "PRIVACY_MASK",
        "PROHIBITED_VIEW_GEOMETRY",
        "MAINTENANCE_ENVELOPE",
        "APPROVAL_EVIDENCE",
        "RL_ENVIRONMENT_CONTRACT",
    }
)

REQUIRED_ARTIFACT_ROLES = frozenset(
    {
        "MAP",
        "AOI_ZONE_CATALOG",
        "RENDER_GEOMETRY",
        "COLLISION_GEOMETRY",
        "RF_GEOMETRY",
        "RF_MATERIAL_CATALOG",
        "RF_NATIVE_TEST_RECEIPT",
        "RADAR_SENSOR_MODEL",
        "PASSIVE_RF_SENSOR_MODEL",
        "RGB_SENSOR_MODEL",
        "EVENT_CAMERA_SENSOR_MODEL",
        "RGB_DETECTOR",
        "EVENT_CAMERA_DETECTOR",
        "FUSION_POLICY",
        "TARGET_TRAJECTORIES",
        "TRAIN_SCENARIOS",
        "VALIDATION_SCENARIOS",
        "TEST_SCENARIOS",
        "MOUNT_REGION_GEOMETRY",
        "SOLVER_CANDIDATES",
        "SENSOR_CALIBRATION",
        "ANTENNA_PATTERN",
        "PRIVACY_MASK",
        "PROHIBITED_VIEW_GEOMETRY",
        "MAINTENANCE_ENVELOPE",
    }
)

SINGLETON_ARTIFACT_ROLES = frozenset(
    {
        "MAP",
        "AOI_ZONE_CATALOG",
        "RENDER_GEOMETRY",
        "COLLISION_GEOMETRY",
        "RF_GEOMETRY",
        "RF_MATERIAL_CATALOG",
        "RF_NATIVE_TEST_RECEIPT",
        "RGB_DETECTOR",
        "EVENT_CAMERA_DETECTOR",
        "FUSION_POLICY",
        "TARGET_TRAJECTORIES",
        "TRAIN_SCENARIOS",
        "VALIDATION_SCENARIOS",
        "TEST_SCENARIOS",
        "MOUNT_REGION_GEOMETRY",
        "SOLVER_CANDIDATES",
    }
)

_SHA256_RE = re.compile(r"[0-9a-f]{64}\Z")


def _object(path: str, value: object) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise TypeError(f"{path} must be an object")
    if any(not isinstance(key, str) for key in value):
        raise TypeError(f"{path} keys must be strings")
    return value


def _exact_object(
    path: str, value: object, required_keys: Sequence[str]
) -> Mapping[str, Any]:
    item = _object(path, value)
    required = set(required_keys)
    missing = sorted(required - set(item))
    unknown = sorted(set(item) - required)
    if missing:
        raise ValueError(f"{path} is missing required fields: {missing}")
    if unknown:
        raise ValueError(f"{path} contains unsupported fields (scope drift): {unknown}")
    return item


def _array(path: str, value: object, *, nonempty: bool = True) -> Sequence[object]:
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence):
        raise TypeError(f"{path} must be an array")
    if nonempty and not value:
        raise ValueError(f"{path} must not be empty")
    return value


def _text(path: str, value: object) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{path} must be a non-empty string")
    return value.strip()


def _literal(path: str, value: object, expected: object) -> None:
    if value != expected or type(value) is not type(expected):
        raise ValueError(f"{path} must be {expected!r}")


def _choice(path: str, value: object, choices: Sequence[str]) -> str:
    selected = _text(path, value)
    if selected not in choices:
        raise ValueError(f"{path} must be one of {list(choices)}")
    return selected


def _boolean(path: str, value: object) -> bool:
    if not isinstance(value, bool):
        raise TypeError(f"{path} must be boolean")
    return value


def _number(
    path: str,
    value: object,
    *,
    minimum: float | None = None,
    maximum: float | None = None,
    minimum_inclusive: bool = True,
) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise TypeError(f"{path} must be numeric")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"{path} must be finite")
    if minimum is not None:
        invalid = result < minimum if minimum_inclusive else result <= minimum
        if invalid:
            operator = ">=" if minimum_inclusive else ">"
            raise ValueError(f"{path} must be {operator} {minimum}")
    if maximum is not None and result > maximum:
        raise ValueError(f"{path} must be <= {maximum}")
    return result


def _integer(
    path: str,
    value: object,
    *,
    minimum: int = 0,
    maximum: int | None = None,
) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"{path} must be an integer")
    if value < minimum or (maximum is not None and value > maximum):
        suffix = f" and <= {maximum}" if maximum is not None else ""
        raise ValueError(f"{path} must be >= {minimum}{suffix}")
    return value


def _sha256(path: str, value: object) -> str:
    digest = _text(path, value)
    if not _SHA256_RE.fullmatch(digest):
        raise ValueError(f"{path} must be a lowercase 64-character SHA-256 digest")
    return digest


def _utc_timestamp(path: str, value: object) -> str:
    timestamp = _text(path, value)
    if not timestamp.endswith("Z"):
        raise ValueError(f"{path} must be an ISO-8601 UTC timestamp ending in Z")
    try:
        parsed = datetime.fromisoformat(timestamp[:-1] + "+00:00")
    except ValueError as exc:
        raise ValueError(f"{path} must be an ISO-8601 UTC timestamp") from exc
    if parsed.tzinfo is None or parsed.utcoffset() != timezone.utc.utcoffset(parsed):
        raise ValueError(f"{path} must be UTC")
    return timestamp


def _unique_texts(
    path: str, value: object, *, nonempty: bool = True
) -> tuple[str, ...]:
    result = tuple(
        _text(f"{path}[{index}]", item)
        for index, item in enumerate(_array(path, value, nonempty=nonempty))
    )
    if len(set(result)) != len(result):
        raise ValueError(f"{path} must contain unique values")
    return result


def _exact_members(
    path: str, value: object, expected: Sequence[str]
) -> tuple[str, ...]:
    result = _unique_texts(path, value)
    if set(result) != set(expected):
        raise ValueError(f"{path} must contain exactly {list(expected)}")
    return result


def _range(
    path: str,
    value: object,
    *,
    minimum: float | None = None,
    maximum: float | None = None,
) -> tuple[float, float]:
    item = _exact_object(path, value, ("minimum", "maximum"))
    low = _number(f"{path}.minimum", item["minimum"], minimum=minimum, maximum=maximum)
    high = _number(f"{path}.maximum", item["maximum"], minimum=minimum, maximum=maximum)
    if low > high:
        raise ValueError(f"{path}.minimum must be <= {path}.maximum")
    return low, high


def _artifact_ref(
    path: str,
    value: object,
    artifacts: Mapping[str, tuple[str, str]],
    expected_roles: str | Sequence[str],
    *,
    require_verified: bool = False,
) -> str:
    artifact_id = _text(path, value)
    if artifact_id not in artifacts:
        raise ValueError(f"{path} references unknown artifact {artifact_id!r}")
    roles = (
        (expected_roles,) if isinstance(expected_roles, str) else tuple(expected_roles)
    )
    actual_role = artifacts[artifact_id][0]
    if actual_role not in roles:
        raise ValueError(
            f"{path} must reference role {list(roles)}, got {actual_role!r}"
        )
    if require_verified and artifacts[artifact_id][1] != "VERIFIED":
        raise ValueError(f"{path} must reference a VERIFIED artifact")
    return artifact_id


def _resolve_verified_artifact_path(
    path: str,
    uri: str,
    artifact_base_directory: Path | None,
) -> Path:
    windows_absolute = re.fullmatch(r"[A-Za-z]:[\\/].+", uri) is not None
    if windows_absolute:
        candidate = Path(uri)
    else:
        parsed = urlsplit(uri)
        if parsed.scheme:
            if parsed.scheme.lower() != "file":
                raise ValueError(
                    f"{path} VERIFIED artifact URI must resolve to a local file, "
                    f"not scheme {parsed.scheme!r}"
                )
            if parsed.netloc not in ("", "localhost"):
                raise ValueError(
                    f"{path} VERIFIED artifact URI must not use a remote file host"
                )
            if parsed.query or parsed.fragment:
                raise ValueError(
                    f"{path} VERIFIED artifact URI must not contain query or fragment data"
                )
            local_text = unquote(parsed.path)
            if re.fullmatch(r"/[A-Za-z]:/.*", local_text):
                local_text = local_text[1:]
            candidate = Path(local_text)
        else:
            candidate = Path(uri)

    if not candidate.is_absolute():
        if artifact_base_directory is None:
            raise ValueError(
                f"{path} VERIFIED relative artifact URI requires a contract base directory"
            )
        candidate = artifact_base_directory / candidate
    try:
        resolved = candidate.resolve(strict=True)
    except OSError as exc:
        raise ValueError(
            f"{path} VERIFIED artifact does not resolve to a local file: {candidate}"
        ) from exc
    if not resolved.is_file():
        raise ValueError(
            f"{path} VERIFIED artifact is not a regular local file: {resolved}"
        )
    return resolved


def _file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def _validate_artifacts(
    value: object,
    artifact_base_directory: Path | None,
    verified_artifact_sha256: Mapping[str, str] | None = None,
) -> dict[str, tuple[str, str]]:
    artifacts: dict[str, tuple[str, str]] = {}
    role_counts: dict[str, int] = {}
    for index, raw in enumerate(_array("artifactBindings", value)):
        path = f"artifactBindings[{index}]"
        item = _exact_object(
            path,
            raw,
            ("artifactId", "role", "sha256", "uri", "provenance", "readiness"),
        )
        artifact_id = _text(f"{path}.artifactId", item["artifactId"])
        if artifact_id in artifacts:
            raise ValueError(
                f"artifactBindings contains duplicate artifactId {artifact_id!r}"
            )
        role = _choice(f"{path}.role", item["role"], tuple(sorted(ARTIFACT_ROLES)))
        expected_sha256 = _sha256(f"{path}.sha256", item["sha256"])
        uri = _text(f"{path}.uri", item["uri"])
        _text(f"{path}.provenance", item["provenance"])
        readiness = _choice(
            f"{path}.readiness", item["readiness"], ("UNVERIFIED", "VERIFIED")
        )
        if readiness == "VERIFIED":
            if verified_artifact_sha256 is None:
                resolved_path = _resolve_verified_artifact_path(
                    f"{path}.uri", uri, artifact_base_directory
                )
                actual_sha256 = _file_sha256(resolved_path)
            else:
                try:
                    actual_sha256 = verified_artifact_sha256[artifact_id]
                except KeyError as exc:
                    raise ValueError(
                        f"{path} VERIFIED artifact was not supplied by the preverified reader"
                    ) from exc
            if actual_sha256 != expected_sha256:
                raise ValueError(
                    f"{path}.sha256 does not match local artifact bytes: "
                    f"expected {expected_sha256}, observed {actual_sha256}"
                )
        artifacts[artifact_id] = (role, readiness)
        role_counts[role] = role_counts.get(role, 0) + 1

    if verified_artifact_sha256 is not None:
        expected_preverified = {
            artifact_id
            for artifact_id, (_, readiness) in artifacts.items()
            if readiness == "VERIFIED"
        }
        unexpected = sorted(set(verified_artifact_sha256) - expected_preverified)
        if unexpected:
            raise ValueError(
                "preverified artifact identities contain undeclared IDs: "
                f"{unexpected}"
            )

    missing_roles = sorted(REQUIRED_ARTIFACT_ROLES - set(role_counts))
    if missing_roles:
        raise ValueError(f"artifactBindings is missing required roles: {missing_roles}")
    duplicated_singletons = sorted(
        role for role in SINGLETON_ARTIFACT_ROLES if role_counts.get(role, 0) != 1
    )
    if duplicated_singletons:
        raise ValueError(
            "artifactBindings must contain exactly one artifact for singleton roles: "
            f"{duplicated_singletons}"
        )
    return artifacts


def _validate_scope(value: object) -> None:
    item = _exact_object(
        "scope",
        value,
        (
            "studyType",
            "allowedSensorClasses",
            "prohibitedDeployedModalities",
            "thermalPolicy",
            "depthPolicy",
            "productionOptimizerImplementationAuthorized",
            "physicalDeploymentAuthorized",
        ),
    )
    _literal("scope.studyType", item["studyType"], "STATIC_FIXED_SITE_PLACEMENT")
    _exact_members(
        "scope.allowedSensorClasses",
        item["allowedSensorClasses"],
        ALLOWED_SENSOR_CLASSES,
    )
    _exact_members(
        "scope.prohibitedDeployedModalities",
        item["prohibitedDeployedModalities"],
        PROHIBITED_DEPLOYED_MODALITIES,
    )
    _literal(
        "scope.thermalPolicy", item["thermalPolicy"], "PROHIBITED_DEPLOYED_MODALITY"
    )
    _literal(
        "scope.depthPolicy",
        item["depthPolicy"],
        "EVALUATOR_TRUTH_OR_DIAGNOSTIC_ONLY_NOT_DEPLOYED",
    )
    _literal(
        "scope.productionOptimizerImplementationAuthorized",
        item["productionOptimizerImplementationAuthorized"],
        False,
    )
    _literal(
        "scope.physicalDeploymentAuthorized",
        item["physicalDeploymentAuthorized"],
        False,
    )


def _validate_requirements(
    value: object,
) -> tuple[tuple[str, ...], tuple[str, ...], int]:
    item = _exact_object(
        "requirements",
        value,
        (
            "objectives",
            "thresholds",
            "hardConstraints",
            "criticalZoneIds",
            "criticalTrajectoryIds",
        ),
    )
    objectives = _exact_object(
        "requirements.objectives",
        item["objectives"],
        (
            "policy",
            "primary",
            "paretoMetrics",
            "hardConstraintsPrecedeRanking",
            "uncalibratedScoreLabel",
        ),
    )
    _literal(
        "requirements.objectives.policy",
        objectives["policy"],
        "CONSTRAINT_FIRST_PARETO",
    )
    _literal(
        "requirements.objectives.primary",
        objectives["primary"],
        "MINIMIZE_LIFECYCLE_COST_UNITS",
    )
    _exact_members(
        "requirements.objectives.paretoMetrics",
        objectives["paretoMetrics"],
        (
            "LIFECYCLE_COST",
            "DETECTION_AND_TRACK_LATENCY",
            "LOCALIZATION_ERROR",
            "OUTAGE_RESILIENCE",
            "RESOURCE_USE",
            "OPERATOR_CUE_LOAD",
        ),
    )
    _literal(
        "requirements.objectives.hardConstraintsPrecedeRanking",
        objectives["hardConstraintsPrecedeRanking"],
        True,
    )
    _literal(
        "requirements.objectives.uncalibratedScoreLabel",
        objectives["uncalibratedScoreLabel"],
        "SIMULATION_EVIDENCE_INDEX",
    )

    thresholds = _exact_object(
        "requirements.thresholds",
        item["thresholds"],
        (
            "minimumWorstScenarioFusedDetectionFraction",
            "maximumCvarTimeToLocalizedTrackSeconds",
            "maximumP95TimeToFirstDetectionSeconds",
            "minimumTrackContinuityFraction",
            "maximumTrackBreakRate",
            "maximumPositionRmseMeters",
            "maximumVelocityRmseMetersPerSecond",
            "maximumFalseDetectionsPerSensorHour",
            "maximumFalseFusedTracksPerScenarioHour",
            "minimumSingleNodeOutageCoverageFraction",
            "minimumFailureDomainOutageCoverageFraction",
            "confidenceLevel",
            "cvarAlpha",
        ),
    )
    for name in (
        "minimumWorstScenarioFusedDetectionFraction",
        "minimumTrackContinuityFraction",
        "maximumTrackBreakRate",
        "minimumSingleNodeOutageCoverageFraction",
        "minimumFailureDomainOutageCoverageFraction",
        "confidenceLevel",
        "cvarAlpha",
    ):
        _number(
            f"requirements.thresholds.{name}",
            thresholds[name],
            minimum=0.0,
            maximum=1.0,
        )
    for name in (
        "maximumCvarTimeToLocalizedTrackSeconds",
        "maximumP95TimeToFirstDetectionSeconds",
        "maximumPositionRmseMeters",
        "maximumVelocityRmseMetersPerSecond",
    ):
        _number(
            f"requirements.thresholds.{name}",
            thresholds[name],
            minimum=0.0,
            minimum_inclusive=False,
        )
    for name in (
        "maximumFalseDetectionsPerSensorHour",
        "maximumFalseFusedTracksPerScenarioHour",
    ):
        _number(f"requirements.thresholds.{name}", thresholds[name], minimum=0.0)
    if float(thresholds["cvarAlpha"]) <= 0.5:
        raise ValueError("requirements.thresholds.cvarAlpha must be > 0.5")
    if float(thresholds["confidenceLevel"]) <= 0.5:
        raise ValueError("requirements.thresholds.confidenceLevel must be > 0.5")

    constraints = _exact_object(
        "requirements.hardConstraints",
        item["hardConstraints"],
        (
            "maximumSites",
            "maximumTotalSensors",
            "maximumSensorsPerClass",
            "minimumIndependentEvidenceFamilies",
            "minimumDistinctSites",
            "minimumFailureDomains",
            "minimumRfLocalizationBaselineMeters",
            "lifecycleBudgetUnits",
            "powerBudgetWatts",
            "backhaulBudgetMbps",
            "computeBudgetUnits",
            "storageBudgetGbPerDay",
            "operatorCueBudgetPerMinute",
        ),
    )
    maximum_sites = _integer(
        "requirements.hardConstraints.maximumSites",
        constraints["maximumSites"],
        minimum=1,
    )
    maximum_total = _integer(
        "requirements.hardConstraints.maximumTotalSensors",
        constraints["maximumTotalSensors"],
        minimum=1,
    )
    per_class = _exact_object(
        "requirements.hardConstraints.maximumSensorsPerClass",
        constraints["maximumSensorsPerClass"],
        ALLOWED_SENSOR_CLASSES,
    )
    if (
        sum(
            _integer(
                f"requirements.hardConstraints.maximumSensorsPerClass.{sensor_class}",
                per_class[sensor_class],
                minimum=1,
            )
            for sensor_class in ALLOWED_SENSOR_CLASSES
        )
        < maximum_total
    ):
        raise ValueError(
            "requirements.hardConstraints.maximumSensorsPerClass cannot support maximumTotalSensors"
        )
    minimum_sites = _integer(
        "requirements.hardConstraints.minimumDistinctSites",
        constraints["minimumDistinctSites"],
        minimum=1,
    )
    if minimum_sites > maximum_sites:
        raise ValueError("minimumDistinctSites must be <= maximumSites")
    minimum_independent_families = _integer(
        "requirements.hardConstraints.minimumIndependentEvidenceFamilies",
        constraints["minimumIndependentEvidenceFamilies"],
        minimum=1,
        maximum=3,
    )
    _integer(
        "requirements.hardConstraints.minimumFailureDomains",
        constraints["minimumFailureDomains"],
        minimum=1,
    )
    for name in (
        "minimumRfLocalizationBaselineMeters",
        "lifecycleBudgetUnits",
        "powerBudgetWatts",
        "backhaulBudgetMbps",
        "computeBudgetUnits",
        "storageBudgetGbPerDay",
        "operatorCueBudgetPerMinute",
    ):
        _number(
            f"requirements.hardConstraints.{name}",
            constraints[name],
            minimum=0.0,
            minimum_inclusive=False,
        )
    critical_zones = _unique_texts(
        "requirements.criticalZoneIds", item["criticalZoneIds"]
    )
    critical_trajectories = _unique_texts(
        "requirements.criticalTrajectoryIds", item["criticalTrajectoryIds"]
    )
    return critical_zones, critical_trajectories, minimum_independent_families


def _validate_critical_zone_catalog(
    value: object,
    artifacts: Mapping[str, tuple[str, str]],
    critical_zone_ids: Sequence[str],
) -> str:
    item = _exact_object(
        "criticalZoneCatalog",
        value,
        ("artifactId", "coordinateFrame", "zoneIds"),
    )
    artifact_id = _artifact_ref(
        "criticalZoneCatalog.artifactId",
        item["artifactId"],
        artifacts,
        "AOI_ZONE_CATALOG",
    )
    _text("criticalZoneCatalog.coordinateFrame", item["coordinateFrame"])
    declared_zone_ids = set(
        _unique_texts("criticalZoneCatalog.zoneIds", item["zoneIds"])
    )
    missing = sorted(set(critical_zone_ids) - declared_zone_ids)
    if missing:
        raise ValueError(
            "requirements.criticalZoneIds contains IDs absent from the hash-bound "
            f"criticalZoneCatalog: {missing}"
        )
    return artifact_id


def _validate_band(path: str, value: object) -> tuple[str, float, float]:
    item = _exact_object(
        path, value, ("bandId", "minimumFrequencyHz", "maximumFrequencyHz")
    )
    band_id = _text(f"{path}.bandId", item["bandId"])
    low = _number(
        f"{path}.minimumFrequencyHz",
        item["minimumFrequencyHz"],
        minimum=0.0,
        minimum_inclusive=False,
    )
    high = _number(
        f"{path}.maximumFrequencyHz",
        item["maximumFrequencyHz"],
        minimum=0.0,
        minimum_inclusive=False,
    )
    if low >= high:
        raise ValueError(f"{path}.minimumFrequencyHz must be < maximumFrequencyHz")
    return band_id, low, high


def _validate_lens(path: str, value: object) -> str:
    item = _exact_object(
        path,
        value,
        (
            "lensId",
            "focalLengthMm",
            "horizontalFovDegrees",
            "verticalFovDegrees",
            "apertureFNumber",
        ),
    )
    lens_id = _text(f"{path}.lensId", item["lensId"])
    _number(
        f"{path}.focalLengthMm",
        item["focalLengthMm"],
        minimum=0.0,
        minimum_inclusive=False,
    )
    for name in ("horizontalFovDegrees", "verticalFovDegrees"):
        _number(
            f"{path}.{name}",
            item[name],
            minimum=0.0,
            maximum=180.0,
            minimum_inclusive=False,
        )
    _number(
        f"{path}.apertureFNumber",
        item["apertureFNumber"],
        minimum=0.0,
        minimum_inclusive=False,
    )
    return lens_id


def _validate_resolution(path: str, value: object) -> None:
    item = _exact_object(path, value, ("widthPixels", "heightPixels"))
    _integer(f"{path}.widthPixels", item["widthPixels"], minimum=1)
    _integer(f"{path}.heightPixels", item["heightPixels"], minimum=1)


def _validate_inventory(
    value: object, artifacts: Mapping[str, tuple[str, str]]
) -> tuple[set[str], dict[str, set[str]]]:
    seen_ids: set[str] = set()
    seen_classes: set[str] = set()
    passive_band_ids: set[str] = set()
    sensor_evidence: dict[str, set[str]] = {
        sensor_class: set() for sensor_class in ALLOWED_SENSOR_CLASSES
    }
    for index, raw in enumerate(_array("hardwareInventory", value)):
        path = f"hardwareInventory[{index}]"
        item = _exact_object(
            path,
            raw,
            (
                "hardwareId",
                "sensorClass",
                "manufacturer",
                "model",
                "hardwareRevision",
                "quantityAvailable",
                "lifecycleCostUnits",
                "massKg",
                "powerWatts",
                "backhaulMbps",
                "computeUnits",
                "storageGbPerDay",
                "modelArtifactId",
                "calibrationArtifactId",
                "configuration",
            ),
        )
        hardware_id = _text(f"{path}.hardwareId", item["hardwareId"])
        if hardware_id in seen_ids:
            raise ValueError(
                f"hardwareInventory contains duplicate hardwareId {hardware_id!r}"
            )
        seen_ids.add(hardware_id)
        sensor_class = _choice(
            f"{path}.sensorClass", item["sensorClass"], ALLOWED_SENSOR_CLASSES
        )
        seen_classes.add(sensor_class)
        for name in ("manufacturer", "model", "hardwareRevision"):
            _text(f"{path}.{name}", item[name])
        _integer(f"{path}.quantityAvailable", item["quantityAvailable"], minimum=1)
        for name in (
            "lifecycleCostUnits",
            "massKg",
            "powerWatts",
            "backhaulMbps",
            "computeUnits",
            "storageGbPerDay",
        ):
            _number(f"{path}.{name}", item[name], minimum=0.0)
        expected_model_role = {
            "radar": "RADAR_SENSOR_MODEL",
            "passive_rf": "PASSIVE_RF_SENSOR_MODEL",
            "rgb": "RGB_SENSOR_MODEL",
            "event_camera": "EVENT_CAMERA_SENSOR_MODEL",
        }[sensor_class]
        model_artifact_id = _artifact_ref(
            f"{path}.modelArtifactId",
            item["modelArtifactId"],
            artifacts,
            expected_model_role,
        )
        calibration_artifact_id = _artifact_ref(
            f"{path}.calibrationArtifactId",
            item["calibrationArtifactId"],
            artifacts,
            "SENSOR_CALIBRATION",
        )
        sensor_evidence[sensor_class].update(
            (model_artifact_id, calibration_artifact_id)
        )

        config_path = f"{path}.configuration"
        if sensor_class == "radar":
            config = _exact_object(
                config_path,
                item["configuration"],
                (
                    "frequencyBands",
                    "waveformIds",
                    "antennaPatternArtifactId",
                    "updateRateHz",
                    "azimuthFieldOfRegardDegrees",
                    "elevationFieldOfRegardDegrees",
                    "detectionModelId",
                    "falseAlarmModelId",
                ),
            )
            bands = [
                _validate_band(f"{config_path}.frequencyBands[{band_index}]", band)
                for band_index, band in enumerate(
                    _array(f"{config_path}.frequencyBands", config["frequencyBands"])
                )
            ]
            if len({band[0] for band in bands}) != len(bands):
                raise ValueError(
                    f"{config_path}.frequencyBands bandId values must be unique"
                )
            _unique_texts(f"{config_path}.waveformIds", config["waveformIds"])
            antenna_artifact_id = _artifact_ref(
                f"{config_path}.antennaPatternArtifactId",
                config["antennaPatternArtifactId"],
                artifacts,
                "ANTENNA_PATTERN",
            )
            sensor_evidence[sensor_class].add(antenna_artifact_id)
            _number(
                f"{config_path}.updateRateHz",
                config["updateRateHz"],
                minimum=0.0,
                minimum_inclusive=False,
            )
            _number(
                f"{config_path}.azimuthFieldOfRegardDegrees",
                config["azimuthFieldOfRegardDegrees"],
                minimum=0.0,
                maximum=360.0,
                minimum_inclusive=False,
            )
            _number(
                f"{config_path}.elevationFieldOfRegardDegrees",
                config["elevationFieldOfRegardDegrees"],
                minimum=0.0,
                maximum=180.0,
                minimum_inclusive=False,
            )
            _text(f"{config_path}.detectionModelId", config["detectionModelId"])
            _text(f"{config_path}.falseAlarmModelId", config["falseAlarmModelId"])
        elif sensor_class == "passive_rf":
            config = _exact_object(
                config_path,
                item["configuration"],
                (
                    "frequencyBands",
                    "antennaPatternArtifactId",
                    "polarizations",
                    "instantaneousBandwidthHz",
                    "dwellSeconds",
                    "clockClass",
                    "supportsAoa",
                    "supportsTdoa",
                    "detectionModelId",
                ),
            )
            bands = [
                _validate_band(f"{config_path}.frequencyBands[{band_index}]", band)
                for band_index, band in enumerate(
                    _array(f"{config_path}.frequencyBands", config["frequencyBands"])
                )
            ]
            band_ids = {band[0] for band in bands}
            if len(band_ids) != len(bands) or passive_band_ids.intersection(band_ids):
                raise ValueError(
                    "passive RF frequency band IDs must be globally unique"
                )
            passive_band_ids.update(band_ids)
            antenna_artifact_id = _artifact_ref(
                f"{config_path}.antennaPatternArtifactId",
                config["antennaPatternArtifactId"],
                artifacts,
                "ANTENNA_PATTERN",
            )
            sensor_evidence[sensor_class].add(antenna_artifact_id)
            polarizations = _unique_texts(
                f"{config_path}.polarizations", config["polarizations"]
            )
            unsupported_polarizations = sorted(
                set(polarizations)
                - {
                    "HORIZONTAL",
                    "VERTICAL",
                    "CIRCULAR_LEFT",
                    "CIRCULAR_RIGHT",
                    "DUAL_LINEAR",
                }
            )
            if unsupported_polarizations:
                raise ValueError(
                    f"{config_path}.polarizations contains unsupported values {unsupported_polarizations}"
                )
            _number(
                f"{config_path}.instantaneousBandwidthHz",
                config["instantaneousBandwidthHz"],
                minimum=0.0,
                minimum_inclusive=False,
            )
            _number(
                f"{config_path}.dwellSeconds",
                config["dwellSeconds"],
                minimum=0.0,
                minimum_inclusive=False,
            )
            _text(f"{config_path}.clockClass", config["clockClass"])
            _boolean(f"{config_path}.supportsAoa", config["supportsAoa"])
            _boolean(f"{config_path}.supportsTdoa", config["supportsTdoa"])
            _text(f"{config_path}.detectionModelId", config["detectionModelId"])
        elif sensor_class == "rgb":
            config = _exact_object(
                config_path,
                item["configuration"],
                (
                    "lensOptions",
                    "resolution",
                    "frameRateHz",
                    "exposurePolicy",
                    "detectorArtifactId",
                    "imagingModelId",
                ),
            )
            lens_ids = [
                _validate_lens(f"{config_path}.lensOptions[{lens_index}]", lens)
                for lens_index, lens in enumerate(
                    _array(f"{config_path}.lensOptions", config["lensOptions"])
                )
            ]
            if len(set(lens_ids)) != len(lens_ids):
                raise ValueError(
                    f"{config_path}.lensOptions lensId values must be unique"
                )
            _validate_resolution(f"{config_path}.resolution", config["resolution"])
            _number(
                f"{config_path}.frameRateHz",
                config["frameRateHz"],
                minimum=0.0,
                minimum_inclusive=False,
            )
            _choice(
                f"{config_path}.exposurePolicy",
                config["exposurePolicy"],
                ("FIXED", "AUTO_BOUNDED", "HDR_BOUNDED"),
            )
            detector_artifact_id = _artifact_ref(
                f"{config_path}.detectorArtifactId",
                config["detectorArtifactId"],
                artifacts,
                "RGB_DETECTOR",
            )
            sensor_evidence[sensor_class].add(detector_artifact_id)
            _text(f"{config_path}.imagingModelId", config["imagingModelId"])
        else:
            config = _exact_object(
                config_path,
                item["configuration"],
                (
                    "lensOptions",
                    "resolution",
                    "temporalParameters",
                    "detectorArtifactId",
                    "eventRepresentation",
                    "imagingModelId",
                ),
            )
            lens_ids = [
                _validate_lens(f"{config_path}.lensOptions[{lens_index}]", lens)
                for lens_index, lens in enumerate(
                    _array(f"{config_path}.lensOptions", config["lensOptions"])
                )
            ]
            if len(set(lens_ids)) != len(lens_ids):
                raise ValueError(
                    f"{config_path}.lensOptions lensId values must be unique"
                )
            _validate_resolution(f"{config_path}.resolution", config["resolution"])
            temporal = _exact_object(
                f"{config_path}.temporalParameters",
                config["temporalParameters"],
                (
                    "positiveContrastThreshold",
                    "negativeContrastThreshold",
                    "refractoryPeriodMicroseconds",
                    "timestampResolutionMicroseconds",
                    "noiseRateEventsPerPixelSecond",
                    "maximumEventRatePerSecond",
                ),
            )
            for name in ("positiveContrastThreshold", "negativeContrastThreshold"):
                _number(
                    f"{config_path}.temporalParameters.{name}",
                    temporal[name],
                    minimum=0.0,
                    maximum=1.0,
                    minimum_inclusive=False,
                )
            for name in (
                "refractoryPeriodMicroseconds",
                "timestampResolutionMicroseconds",
            ):
                _number(
                    f"{config_path}.temporalParameters.{name}",
                    temporal[name],
                    minimum=0.0,
                    minimum_inclusive=False,
                )
            for name in ("noiseRateEventsPerPixelSecond", "maximumEventRatePerSecond"):
                _number(
                    f"{config_path}.temporalParameters.{name}",
                    temporal[name],
                    minimum=0.0,
                )
            detector_artifact_id = _artifact_ref(
                f"{config_path}.detectorArtifactId",
                config["detectorArtifactId"],
                artifacts,
                "EVENT_CAMERA_DETECTOR",
            )
            sensor_evidence[sensor_class].add(detector_artifact_id)
            _choice(
                f"{config_path}.eventRepresentation",
                config["eventRepresentation"],
                ("RAW_EVENTS", "VOXEL_GRID", "TIME_SURFACE"),
            )
            _text(f"{config_path}.imagingModelId", config["imagingModelId"])

    if seen_classes != set(ALLOWED_SENSOR_CLASSES):
        raise ValueError(
            "hardwareInventory must contain every and only the four allowed sensor classes: "
            f"{list(ALLOWED_SENSOR_CLASSES)}"
        )
    return passive_band_ids, sensor_evidence


def _validate_target_envelope(
    value: object,
    artifacts: Mapping[str, tuple[str, str]],
    passive_band_ids: set[str],
    critical_trajectory_ids: Sequence[str],
) -> tuple[tuple[str, ...], str]:
    item = _exact_object(
        "targetAndTrajectoryEnvelope",
        value,
        ("targetClasses", "trajectoryCorpus"),
    )
    target_ids: set[str] = set()
    for index, raw in enumerate(
        _array("targetAndTrajectoryEnvelope.targetClasses", item["targetClasses"])
    ):
        path = f"targetAndTrajectoryEnvelope.targetClasses[{index}]"
        target = _exact_object(
            path,
            raw,
            (
                "targetClassId",
                "lengthMeters",
                "widthMeters",
                "heightMeters",
                "speedMetersPerSecond",
                "accelerationMetersPerSecondSquared",
                "altitudeAglMeters",
                "radarCrossSectionSquareMeters",
                "aspectDegrees",
                "rfEmissionStates",
                "rfBandIds",
                "transmitPowerDbm",
                "dutyCycleFraction",
            ),
        )
        target_id = _text(f"{path}.targetClassId", target["targetClassId"])
        if target_id in target_ids:
            raise ValueError(f"target class IDs must be unique: {target_id!r}")
        target_ids.add(target_id)
        for name in (
            "lengthMeters",
            "widthMeters",
            "heightMeters",
            "speedMetersPerSecond",
            "accelerationMetersPerSecondSquared",
            "altitudeAglMeters",
            "radarCrossSectionSquareMeters",
        ):
            _range(f"{path}.{name}", target[name], minimum=0.0)
        _range(
            f"{path}.aspectDegrees", target["aspectDegrees"], minimum=0.0, maximum=360.0
        )
        _exact_members(
            f"{path}.rfEmissionStates",
            target["rfEmissionStates"],
            ("EMITTING", "SILENT"),
        )
        target_band_ids = set(_unique_texts(f"{path}.rfBandIds", target["rfBandIds"]))
        unknown_bands = sorted(target_band_ids - passive_band_ids)
        if unknown_bands:
            raise ValueError(
                f"{path}.rfBandIds references unknown passive-RF bands {unknown_bands}"
            )
        _range(f"{path}.transmitPowerDbm", target["transmitPowerDbm"])
        _range(
            f"{path}.dutyCycleFraction",
            target["dutyCycleFraction"],
            minimum=0.0,
            maximum=1.0,
        )

    corpus = _exact_object(
        "targetAndTrajectoryEnvelope.trajectoryCorpus",
        item["trajectoryCorpus"],
        (
            "artifactId",
            "trajectoryIds",
            "coordinateFrame",
            "trajectoryTypes",
            "timeHorizonSeconds",
            "samplePeriodSeconds",
            "containsCompleteIngressAndEgress",
        ),
    )
    trajectory_artifact_id = _artifact_ref(
        "targetAndTrajectoryEnvelope.trajectoryCorpus.artifactId",
        corpus["artifactId"],
        artifacts,
        "TARGET_TRAJECTORIES",
    )
    trajectory_ids = _unique_texts(
        "targetAndTrajectoryEnvelope.trajectoryCorpus.trajectoryIds",
        corpus["trajectoryIds"],
    )
    missing_critical = sorted(set(critical_trajectory_ids) - set(trajectory_ids))
    if missing_critical:
        raise ValueError(
            f"critical trajectory IDs are absent from trajectoryCorpus: {missing_critical}"
        )
    _text(
        "targetAndTrajectoryEnvelope.trajectoryCorpus.coordinateFrame",
        corpus["coordinateFrame"],
    )
    _unique_texts(
        "targetAndTrajectoryEnvelope.trajectoryCorpus.trajectoryTypes",
        corpus["trajectoryTypes"],
    )
    _range(
        "targetAndTrajectoryEnvelope.trajectoryCorpus.timeHorizonSeconds",
        corpus["timeHorizonSeconds"],
        minimum=0.0,
    )
    _number(
        "targetAndTrajectoryEnvelope.trajectoryCorpus.samplePeriodSeconds",
        corpus["samplePeriodSeconds"],
        minimum=0.0,
        minimum_inclusive=False,
    )
    _literal(
        "targetAndTrajectoryEnvelope.trajectoryCorpus.containsCompleteIngressAndEgress",
        corpus["containsCompleteIngressAndEgress"],
        True,
    )
    return trajectory_ids, trajectory_artifact_id


def _validate_scenario_corpus(
    value: object, artifacts: Mapping[str, tuple[str, str]]
) -> tuple[dict[str, str], set[str]]:
    item = _exact_object(
        "scenarioCorpus",
        value,
        ("requiredDimensions", "commonRandomSeeds", "partitions"),
    )
    _exact_members(
        "scenarioCorpus.requiredDimensions",
        item["requiredDimensions"],
        REQUIRED_SCENARIO_DIMENSIONS,
    )
    seeds = tuple(
        _integer(f"scenarioCorpus.commonRandomSeeds[{index}]", seed, minimum=0)
        for index, seed in enumerate(
            _array("scenarioCorpus.commonRandomSeeds", item["commonRandomSeeds"])
        )
    )
    if len(set(seeds)) != len(seeds):
        raise ValueError("scenarioCorpus.commonRandomSeeds must be unique")

    partitions = _exact_object(
        "scenarioCorpus.partitions", item["partitions"], ("train", "validation", "test")
    )
    expected = {
        "train": ("TRAINING_AND_TUNING", "TRAIN_SCENARIOS"),
        "validation": ("MODEL_AND_SOLVER_SELECTION", "VALIDATION_SCENARIOS"),
        "test": ("FINAL_HELD_OUT_EVALUATION_ONLY", "TEST_SCENARIOS"),
    }
    partition_ids: dict[str, str] = {}
    partition_artifact_ids: set[str] = set()
    all_scenario_ids: set[str] = set()
    for name, (purpose, role) in expected.items():
        path = f"scenarioCorpus.partitions.{name}"
        partition = _exact_object(
            path,
            partitions[name],
            (
                "partitionId",
                "scenarioSetArtifactId",
                "scenarioIds",
                "purpose",
                "frozen",
            ),
        )
        partition_id = _text(f"{path}.partitionId", partition["partitionId"])
        if partition_id in partition_ids.values():
            raise ValueError("scenario partition IDs must be unique")
        partition_ids[name] = partition_id
        partition_artifact_ids.add(
            _artifact_ref(
                f"{path}.scenarioSetArtifactId",
                partition["scenarioSetArtifactId"],
                artifacts,
                role,
            )
        )
        scenario_ids = set(
            _unique_texts(f"{path}.scenarioIds", partition["scenarioIds"])
        )
        overlap = sorted(all_scenario_ids.intersection(scenario_ids))
        if overlap:
            raise ValueError(
                f"train/validation/test scenario IDs must be disjoint: {overlap}"
            )
        all_scenario_ids.update(scenario_ids)
        _literal(f"{path}.purpose", partition["purpose"], purpose)
        _literal(f"{path}.frozen", partition["frozen"], True)
    return partition_ids, partition_artifact_ids


def _validate_fusion(
    value: object,
    artifacts: Mapping[str, tuple[str, str]],
    required_minimum_independent_families: int,
) -> str:
    item = _exact_object(
        "fusionPolicy",
        value,
        (
            "policyId",
            "policyVersion",
            "policyArtifactId",
            "decisionRule",
            "minimumIndependentFamilies",
            "familyAssignments",
            "rgbEventCorrelationTreatment",
            "sameFamilyEvidenceMayCorroborate",
            "sameFamilyScoresMayBeSummed",
            "simulatorTruthMaySeedOperationalTracks",
        ),
    )
    _text("fusionPolicy.policyId", item["policyId"])
    _text("fusionPolicy.policyVersion", item["policyVersion"])
    policy_artifact_id = _artifact_ref(
        "fusionPolicy.policyArtifactId",
        item["policyArtifactId"],
        artifacts,
        "FUSION_POLICY",
    )
    _literal(
        "fusionPolicy.decisionRule",
        item["decisionRule"],
        "MINIMUM_INDEPENDENT_EVIDENCE_FAMILIES",
    )
    fusion_minimum_independent_families = _integer(
        "fusionPolicy.minimumIndependentFamilies",
        item["minimumIndependentFamilies"],
        minimum=1,
        maximum=3,
    )
    if fusion_minimum_independent_families != required_minimum_independent_families:
        raise ValueError(
            "fusionPolicy.minimumIndependentFamilies must equal "
            "requirements.hardConstraints.minimumIndependentEvidenceFamilies"
        )
    assignments = _exact_object(
        "fusionPolicy.familyAssignments",
        item["familyAssignments"],
        ("active_radar", "passive_rf", "visual"),
    )
    _exact_members(
        "fusionPolicy.familyAssignments.active_radar",
        assignments["active_radar"],
        ("radar",),
    )
    _exact_members(
        "fusionPolicy.familyAssignments.passive_rf",
        assignments["passive_rf"],
        ("passive_rf",),
    )
    _exact_members(
        "fusionPolicy.familyAssignments.visual",
        assignments["visual"],
        ("rgb", "event_camera"),
    )
    _literal(
        "fusionPolicy.rgbEventCorrelationTreatment",
        item["rgbEventCorrelationTreatment"],
        "CORRELATED_SINGLE_VISUAL_FAMILY",
    )
    _literal(
        "fusionPolicy.sameFamilyEvidenceMayCorroborate",
        item["sameFamilyEvidenceMayCorroborate"],
        False,
    )
    _literal(
        "fusionPolicy.sameFamilyScoresMayBeSummed",
        item["sameFamilyScoresMayBeSummed"],
        False,
    )
    _literal(
        "fusionPolicy.simulatorTruthMaySeedOperationalTracks",
        item["simulatorTruthMaySeedOperationalTracks"],
        False,
    )
    return policy_artifact_id


def _validate_mount_regions(
    value: object, artifacts: Mapping[str, tuple[str, str]]
) -> tuple[set[str], set[str], bool]:
    item = _exact_object(
        "mountRegionPolicy",
        value,
        ("coordinateFrame", "candidateGeometryArtifactId", "selectionRule", "regions"),
    )
    _text("mountRegionPolicy.coordinateFrame", item["coordinateFrame"])
    mount_evidence_ids = {
        _artifact_ref(
            "mountRegionPolicy.candidateGeometryArtifactId",
            item["candidateGeometryArtifactId"],
            artifacts,
            "MOUNT_REGION_GEOMETRY",
        )
    }
    privacy_evidence_ids: set[str] = set()
    _literal(
        "mountRegionPolicy.selectionRule",
        item["selectionRule"],
        "ONLY_ELIGIBLE_REGIONS_MAY_ENTER_CANDIDATE_GENERATION",
    )
    region_ids: set[str] = set()
    any_eligible = False
    for index, raw in enumerate(_array("mountRegionPolicy.regions", item["regions"])):
        path = f"mountRegionPolicy.regions[{index}]"
        region = _exact_object(
            path,
            raw,
            (
                "regionId",
                "geometryElementIds",
                "allowedSensorClasses",
                "heightAglMeters",
                "azimuthDegrees",
                "permissions",
                "resources",
                "privacy",
                "eligibleForCandidateGeneration",
            ),
        )
        region_id = _text(f"{path}.regionId", region["regionId"])
        if region_id in region_ids:
            raise ValueError(f"mount region IDs must be unique: {region_id!r}")
        region_ids.add(region_id)
        _unique_texts(f"{path}.geometryElementIds", region["geometryElementIds"])
        allowed_classes = _unique_texts(
            f"{path}.allowedSensorClasses", region["allowedSensorClasses"]
        )
        unsupported = sorted(set(allowed_classes) - set(ALLOWED_SENSOR_CLASSES))
        if unsupported:
            raise ValueError(
                f"{path}.allowedSensorClasses contains prohibited/unknown classes {unsupported}"
            )
        _range(f"{path}.heightAglMeters", region["heightAglMeters"], minimum=0.0)
        _range(
            f"{path}.azimuthDegrees",
            region["azimuthDegrees"],
            minimum=0.0,
            maximum=360.0,
        )

        permissions = _exact_object(
            f"{path}.permissions",
            region["permissions"],
            (
                "siteAuthority",
                "status",
                "evidenceArtifactId",
                "accessApproved",
                "structuralApproved",
                "radarEmissionApproved",
            ),
        )
        _text(f"{path}.permissions.siteAuthority", permissions["siteAuthority"])
        permission_status = _choice(
            f"{path}.permissions.status",
            permissions["status"],
            ("PENDING", "APPROVED", "REJECTED"),
        )
        permission_evidence = permissions["evidenceArtifactId"]
        permission_evidence_id: str | None = None
        if permission_status == "APPROVED":
            permission_evidence_id = _artifact_ref(
                f"{path}.permissions.evidenceArtifactId",
                permission_evidence,
                artifacts,
                "MOUNT_PERMISSION",
            )
            mount_evidence_ids.add(permission_evidence_id)
        elif permission_evidence is not None:
            raise ValueError(
                f"{path}.permissions.evidenceArtifactId must be null unless APPROVED"
            )
        access_approved = _boolean(
            f"{path}.permissions.accessApproved", permissions["accessApproved"]
        )
        structural_approved = _boolean(
            f"{path}.permissions.structuralApproved", permissions["structuralApproved"]
        )
        radar_approved = _boolean(
            f"{path}.permissions.radarEmissionApproved",
            permissions["radarEmissionApproved"],
        )
        if permission_status != "APPROVED" and (
            access_approved or structural_approved or radar_approved
        ):
            raise ValueError(
                f"{path}.permissions approvals must be false unless status is APPROVED"
            )

        resources = _exact_object(
            f"{path}.resources",
            region["resources"],
            (
                "maximumSensorCount",
                "maximumInstalledMassKg",
                "availablePowerWatts",
                "availableBackhaulMbps",
                "availableComputeUnits",
                "availableStorageGbPerDay",
                "failureDomainId",
                "powerDomainId",
                "backhaulDomainId",
                "maintenanceEnvelopeArtifactId",
            ),
        )
        _integer(
            f"{path}.resources.maximumSensorCount",
            resources["maximumSensorCount"],
            minimum=1,
        )
        for name in (
            "maximumInstalledMassKg",
            "availablePowerWatts",
            "availableBackhaulMbps",
            "availableComputeUnits",
            "availableStorageGbPerDay",
        ):
            _number(f"{path}.resources.{name}", resources[name], minimum=0.0)
        for name in ("failureDomainId", "powerDomainId", "backhaulDomainId"):
            _text(f"{path}.resources.{name}", resources[name])
        maintenance_artifact_id = _artifact_ref(
            f"{path}.resources.maintenanceEnvelopeArtifactId",
            resources["maintenanceEnvelopeArtifactId"],
            artifacts,
            "MAINTENANCE_ENVELOPE",
        )
        mount_evidence_ids.add(maintenance_artifact_id)

        privacy = _exact_object(
            f"{path}.privacy",
            region["privacy"],
            (
                "reviewStatus",
                "privacyMaskArtifactId",
                "prohibitedViewGeometryArtifactId",
                "rawImageryExportAllowed",
                "maximumRetentionHours",
            ),
        )
        privacy_status = _choice(
            f"{path}.privacy.reviewStatus",
            privacy["reviewStatus"],
            ("PENDING", "APPROVED", "REJECTED"),
        )
        privacy_mask_artifact_id = _artifact_ref(
            f"{path}.privacy.privacyMaskArtifactId",
            privacy["privacyMaskArtifactId"],
            artifacts,
            "PRIVACY_MASK",
        )
        prohibited_view_artifact_id = _artifact_ref(
            f"{path}.privacy.prohibitedViewGeometryArtifactId",
            privacy["prohibitedViewGeometryArtifactId"],
            artifacts,
            "PROHIBITED_VIEW_GEOMETRY",
        )
        privacy_evidence_ids.update(
            (privacy_mask_artifact_id, prohibited_view_artifact_id)
        )
        raw_export = _boolean(
            f"{path}.privacy.rawImageryExportAllowed",
            privacy["rawImageryExportAllowed"],
        )
        _number(
            f"{path}.privacy.maximumRetentionHours",
            privacy["maximumRetentionHours"],
            minimum=0.0,
        )
        if raw_export and privacy_status != "APPROVED":
            raise ValueError(
                f"{path}.privacy raw imagery export requires an APPROVED privacy review"
            )

        eligible = _boolean(
            f"{path}.eligibleForCandidateGeneration",
            region["eligibleForCandidateGeneration"],
        )
        if eligible:
            any_eligible = True
            if (
                permission_status != "APPROVED"
                or not access_approved
                or not structural_approved
            ):
                raise ValueError(
                    f"{path} cannot be eligible without approved access and structure"
                )
            if permission_evidence_id is None:
                raise ValueError(
                    f"{path} cannot be eligible without permission evidence"
                )
            _artifact_ref(
                f"{path}.permissions.evidenceArtifactId",
                permission_evidence_id,
                artifacts,
                "MOUNT_PERMISSION",
                require_verified=True,
            )
            if "radar" in allowed_classes and not radar_approved:
                raise ValueError(
                    f"{path} cannot allow radar without radar-emission approval"
                )
            if privacy_status != "APPROVED":
                raise ValueError(f"{path} cannot be eligible without privacy approval")
            _artifact_ref(
                f"{path}.privacy.privacyMaskArtifactId",
                privacy_mask_artifact_id,
                artifacts,
                "PRIVACY_MASK",
                require_verified=True,
            )
            _artifact_ref(
                f"{path}.privacy.prohibitedViewGeometryArtifactId",
                prohibited_view_artifact_id,
                artifacts,
                "PROHIBITED_VIEW_GEOMETRY",
                require_verified=True,
            )
    return mount_evidence_ids, privacy_evidence_ids, any_eligible


def _validate_readiness(
    value: object,
    artifacts: Mapping[str, tuple[str, str]],
    expected_evidence_by_gate: Mapping[str, set[str]],
) -> dict[str, str]:
    item = _exact_object(
        "readiness",
        value,
        ("gates", "productionOptimizerImplementationGate", "physicalDeploymentGate"),
    )
    gate_ids: set[str] = set()
    gate_statuses: dict[str, str] = {}
    for index, raw in enumerate(_array("readiness.gates", item["gates"])):
        path = f"readiness.gates[{index}]"
        gate = _exact_object(
            path, raw, ("gateId", "status", "evidenceArtifactIds", "blockingReasons")
        )
        gate_id = _choice(f"{path}.gateId", gate["gateId"], REQUIRED_GATE_IDS)
        if gate_id in gate_ids:
            raise ValueError(f"readiness gate IDs must be unique: {gate_id!r}")
        gate_ids.add(gate_id)
        status = _choice(
            f"{path}.status", gate["status"], ("PENDING", "BLOCKED", "PASSED")
        )
        gate_statuses[gate_id] = status
        evidence_ids = _unique_texts(
            f"{path}.evidenceArtifactIds", gate["evidenceArtifactIds"], nonempty=False
        )
        for artifact_id in evidence_ids:
            if artifact_id not in artifacts:
                raise ValueError(
                    f"{path}.evidenceArtifactIds references unknown artifact {artifact_id!r}"
                )
        expected_evidence = expected_evidence_by_gate[gate_id]
        actual_evidence = set(evidence_ids)
        if actual_evidence != expected_evidence:
            missing = sorted(expected_evidence - actual_evidence)
            unexpected = sorted(actual_evidence - expected_evidence)
            raise ValueError(
                f"{path}.evidenceArtifactIds must exactly match the artifacts bound "
                f"to gate {gate_id!r}; missing={missing}, unexpected={unexpected}"
            )
        reasons = _unique_texts(
            f"{path}.blockingReasons", gate["blockingReasons"], nonempty=False
        )
        if status == "PASSED":
            if not evidence_ids or reasons:
                raise ValueError(
                    f"{path} PASSED requires evidence and no blocking reasons"
                )
            unverified = sorted(
                artifact_id
                for artifact_id in evidence_ids
                if artifacts[artifact_id][1] != "VERIFIED"
            )
            if unverified:
                raise ValueError(
                    f"{path} PASSED references unverified evidence {unverified}"
                )
        elif not reasons:
            raise ValueError(f"{path} {status} requires at least one blocking reason")
    if gate_ids != set(REQUIRED_GATE_IDS):
        raise ValueError(
            f"readiness.gates must contain exactly {list(REQUIRED_GATE_IDS)}"
        )
    _literal(
        "readiness.productionOptimizerImplementationGate",
        item["productionOptimizerImplementationGate"],
        "BLOCKED",
    )
    _literal(
        "readiness.physicalDeploymentGate", item["physicalDeploymentGate"], "BLOCKED"
    )
    return gate_statuses


def _validate_solver_benchmark(
    value: object,
    artifacts: Mapping[str, tuple[str, str]],
    partitions: Mapping[str, str],
) -> tuple[set[str], str, str]:
    item = _exact_object(
        "solverBenchmark",
        value,
        (
            "benchmarkId",
            "status",
            "candidateSetArtifactId",
            "solverTracks",
            "equalBudget",
            "sameBudgetForAllTracks",
            "commonRandomNumbersRequired",
            "selectionPartitionId",
            "finalEvaluationPartitionId",
            "requiredOutputs",
            "finalExactSimulatorReplayRequired",
            "maximumHardConstraintViolations",
        ),
    )
    _text("solverBenchmark.benchmarkId", item["benchmarkId"])
    _choice("solverBenchmark.status", item["status"], ("PLANNED", "FROZEN"))
    _artifact_ref(
        "solverBenchmark.candidateSetArtifactId",
        item["candidateSetArtifactId"],
        artifacts,
        "SOLVER_CANDIDATES",
    )
    expected_roles = {
        "GREEDY_BASELINE": ("BASELINE", False, "NONE_BASELINE_ONLY"),
        "EXACT_MILP_OR_CP_SAT": (
            "PRIMARY_STATIC_SELECTOR",
            False,
            "BOUND_GAP_OR_INFEASIBILITY_PROOF",
        ),
        "BAYESIAN_CONTINUOUS_REFINEMENT": (
            "CONTINUOUS_REFINEMENT_CHALLENGER",
            True,
            "NONE_CHALLENGER_ONLY",
        ),
        "EVOLUTIONARY_MULTIOBJECTIVE": (
            "MULTIOBJECTIVE_CHALLENGER",
            True,
            "NONE_CHALLENGER_ONLY",
        ),
    }
    methods: set[str] = set()
    solver_ids: set[str] = set()
    exact_solver_id = ""
    for index, raw in enumerate(
        _array("solverBenchmark.solverTracks", item["solverTracks"])
    ):
        path = f"solverBenchmark.solverTracks[{index}]"
        track = _exact_object(
            path,
            raw,
            ("solverId", "method", "role", "stochastic", "optimalityEvidence"),
        )
        solver_id = _text(f"{path}.solverId", track["solverId"])
        if solver_id in solver_ids:
            raise ValueError(
                f"solverBenchmark solver IDs must be unique: {solver_id!r}"
            )
        solver_ids.add(solver_id)
        method = _choice(f"{path}.method", track["method"], REQUIRED_SOLVER_METHODS)
        if method in methods:
            raise ValueError(f"solverBenchmark methods must be unique: {method!r}")
        methods.add(method)
        role, stochastic, evidence = expected_roles[method]
        _literal(f"{path}.role", track["role"], role)
        _literal(f"{path}.stochastic", track["stochastic"], stochastic)
        _literal(f"{path}.optimalityEvidence", track["optimalityEvidence"], evidence)
        if method == "EXACT_MILP_OR_CP_SAT":
            exact_solver_id = solver_id
    if methods != set(REQUIRED_SOLVER_METHODS):
        raise ValueError(
            f"solverBenchmark must contain exactly methods {list(REQUIRED_SOLVER_METHODS)}"
        )

    budget = _exact_object(
        "solverBenchmark.equalBudget",
        item["equalBudget"],
        (
            "budgetId",
            "highFidelitySimulatorCalls",
            "candidateEvaluations",
            "wallClockSeconds",
            "cpuCoreHours",
            "gpuHours",
            "randomSeeds",
            "hardwareProfile",
        ),
    )
    budget_id = _text("solverBenchmark.equalBudget.budgetId", budget["budgetId"])
    for name in (
        "highFidelitySimulatorCalls",
        "candidateEvaluations",
        "wallClockSeconds",
    ):
        _integer(f"solverBenchmark.equalBudget.{name}", budget[name], minimum=1)
    for name in ("cpuCoreHours", "gpuHours"):
        _number(f"solverBenchmark.equalBudget.{name}", budget[name], minimum=0.0)
    budget_seeds = tuple(
        _integer(f"solverBenchmark.equalBudget.randomSeeds[{index}]", seed, minimum=0)
        for index, seed in enumerate(
            _array("solverBenchmark.equalBudget.randomSeeds", budget["randomSeeds"])
        )
    )
    if len(set(budget_seeds)) != len(budget_seeds):
        raise ValueError("solverBenchmark.equalBudget.randomSeeds must be unique")
    _text("solverBenchmark.equalBudget.hardwareProfile", budget["hardwareProfile"])
    _literal(
        "solverBenchmark.sameBudgetForAllTracks", item["sameBudgetForAllTracks"], True
    )
    _literal(
        "solverBenchmark.commonRandomNumbersRequired",
        item["commonRandomNumbersRequired"],
        True,
    )
    _literal(
        "solverBenchmark.selectionPartitionId",
        item["selectionPartitionId"],
        partitions["validation"],
    )
    _literal(
        "solverBenchmark.finalEvaluationPartitionId",
        item["finalEvaluationPartitionId"],
        partitions["test"],
    )
    _exact_members(
        "solverBenchmark.requiredOutputs",
        item["requiredOutputs"],
        REQUIRED_BENCHMARK_OUTPUTS,
    )
    _literal(
        "solverBenchmark.finalExactSimulatorReplayRequired",
        item["finalExactSimulatorReplayRequired"],
        True,
    )
    _literal(
        "solverBenchmark.maximumHardConstraintViolations",
        item["maximumHardConstraintViolations"],
        0,
    )
    return solver_ids, exact_solver_id, budget_id


def _validate_rl_decision(
    value: object,
    artifacts: Mapping[str, tuple[str, str]],
    solver_ids: set[str],
    exact_solver_id: str,
    budget_id: str,
    partitions: Mapping[str, str],
) -> None:
    item = _exact_object(
        "rlGoNoGo",
        value,
        (
            "decision",
            "problemClass",
            "sequentialUseCaseId",
            "environmentContractArtifactId",
            "justification",
            "experimentAuthorized",
            "productionUseAuthorized",
            "deterministicBaselineSolverId",
            "equalBudgetId",
            "selectionPartitionId",
            "finalEvaluationPartitionId",
            "goCriteria",
        ),
    )
    decision = _choice(
        "rlGoNoGo.decision",
        item["decision"],
        ("NO_GO_STATIC_PLACEMENT", "GO_SEQUENTIAL_EXPERIMENT_ONLY"),
    )
    _text("rlGoNoGo.justification", item["justification"])
    experiment_authorized = _boolean(
        "rlGoNoGo.experimentAuthorized", item["experimentAuthorized"]
    )
    _literal("rlGoNoGo.productionUseAuthorized", item["productionUseAuthorized"], False)
    baseline_id = _text(
        "rlGoNoGo.deterministicBaselineSolverId", item["deterministicBaselineSolverId"]
    )
    if baseline_id not in solver_ids or baseline_id != exact_solver_id:
        raise ValueError(
            "rlGoNoGo.deterministicBaselineSolverId must be the exact static solver"
        )
    _literal("rlGoNoGo.equalBudgetId", item["equalBudgetId"], budget_id)
    _literal(
        "rlGoNoGo.selectionPartitionId",
        item["selectionPartitionId"],
        partitions["validation"],
    )
    _literal(
        "rlGoNoGo.finalEvaluationPartitionId",
        item["finalEvaluationPartitionId"],
        partitions["test"],
    )
    if decision == "NO_GO_STATIC_PLACEMENT":
        _literal("rlGoNoGo.problemClass", item["problemClass"], "STATIC_PLACEMENT")
        if (
            item["sequentialUseCaseId"] is not None
            or item["environmentContractArtifactId"] is not None
        ):
            raise ValueError(
                "static-placement RL no-go must not declare a sequential environment"
            )
        _literal("rlGoNoGo.experimentAuthorized", experiment_authorized, False)
    else:
        _literal(
            "rlGoNoGo.problemClass",
            item["problemClass"],
            "SEQUENTIAL_DEPLOYMENT_OR_SENSOR_TASKING",
        )
        _text("rlGoNoGo.sequentialUseCaseId", item["sequentialUseCaseId"])
        _artifact_ref(
            "rlGoNoGo.environmentContractArtifactId",
            item["environmentContractArtifactId"],
            artifacts,
            "RL_ENVIRONMENT_CONTRACT",
            require_verified=True,
        )
        _literal("rlGoNoGo.experimentAuthorized", experiment_authorized, True)

    criteria = _exact_object(
        "rlGoNoGo.goCriteria",
        item["goCriteria"],
        (
            "genuineSequentialStateAndActionsRequired",
            "minimumHeldOutRobustImprovementFraction",
            "maximumHardConstraintViolations",
            "sameBudgetAsDeterministicBaselineRequired",
            "disjointMapGeneralizationRequired",
            "completeAuditTrailRequired",
            "finalExactSimulatorReplayRequired",
        ),
    )
    _literal(
        "rlGoNoGo.goCriteria.genuineSequentialStateAndActionsRequired",
        criteria["genuineSequentialStateAndActionsRequired"],
        True,
    )
    _number(
        "rlGoNoGo.goCriteria.minimumHeldOutRobustImprovementFraction",
        criteria["minimumHeldOutRobustImprovementFraction"],
        minimum=0.0,
        maximum=1.0,
        minimum_inclusive=False,
    )
    _literal(
        "rlGoNoGo.goCriteria.maximumHardConstraintViolations",
        criteria["maximumHardConstraintViolations"],
        0,
    )
    for name in (
        "sameBudgetAsDeterministicBaselineRequired",
        "disjointMapGeneralizationRequired",
        "completeAuditTrailRequired",
        "finalExactSimulatorReplayRequired",
    ):
        _literal(f"rlGoNoGo.goCriteria.{name}", criteria[name], True)


def _validate_governance(
    value: object,
    contract_state: str,
    artifacts: Mapping[str, tuple[str, str]],
) -> None:
    item = _exact_object(
        "governance",
        value,
        ("approvalState", "requiredApproverRoles", "approvals", "changeControlPolicy"),
    )
    approval_state = _choice(
        "governance.approvalState", item["approvalState"], ("DRAFT", "APPROVED")
    )
    _exact_members(
        "governance.requiredApproverRoles",
        item["requiredApproverRoles"],
        REQUIRED_APPROVER_ROLES,
    )
    records: dict[str, str] = {}
    for index, raw in enumerate(_array("governance.approvals", item["approvals"])):
        path = f"governance.approvals[{index}]"
        approval = _exact_object(
            path,
            raw,
            ("role", "decision", "reviewerId", "decidedAtUtc", "evidenceArtifactId"),
        )
        role = _choice(f"{path}.role", approval["role"], REQUIRED_APPROVER_ROLES)
        if role in records:
            raise ValueError(f"governance approval roles must be unique: {role!r}")
        decision = _choice(
            f"{path}.decision",
            approval["decision"],
            ("PENDING", "APPROVED", "REJECTED"),
        )
        records[role] = decision
        if decision == "PENDING":
            if any(
                approval[name] is not None
                for name in ("reviewerId", "decidedAtUtc", "evidenceArtifactId")
            ):
                raise ValueError(f"{path} PENDING approval metadata must be null")
        else:
            _text(f"{path}.reviewerId", approval["reviewerId"])
            _utc_timestamp(f"{path}.decidedAtUtc", approval["decidedAtUtc"])
            _artifact_ref(
                f"{path}.evidenceArtifactId",
                approval["evidenceArtifactId"],
                artifacts,
                "APPROVAL_EVIDENCE",
            )
    if set(records) != set(REQUIRED_APPROVER_ROLES):
        raise ValueError(
            f"governance.approvals must contain exactly roles {list(REQUIRED_APPROVER_ROLES)}"
        )
    _literal(
        "governance.changeControlPolicy",
        item["changeControlPolicy"],
        "MATERIAL_CHANGE_REQUIRES_NEW_STUDY_REVISION_AND_REAPPROVAL",
    )
    if contract_state == "FROZEN":
        if approval_state != "APPROVED" or set(records.values()) != {"APPROVED"}:
            raise ValueError("a FROZEN Stage-0 contract requires every approval")
    elif approval_state != "DRAFT":
        raise ValueError("a DRAFT Stage-0 contract must have DRAFT approvalState")


def _artifact_ids_for_roles(
    artifacts: Mapping[str, tuple[str, str]], *roles: str
) -> set[str]:
    expected_roles = set(roles)
    return {
        artifact_id
        for artifact_id, (role, _) in artifacts.items()
        if role in expected_roles
    }


def _validate_contract(
    value: object,
    artifact_base_directory: Path | None,
    verified_artifact_sha256: Mapping[str, str] | None = None,
) -> Mapping[str, Any]:
    item = _exact_object(
        "stage0StudyContract",
        value,
        (
            "schemaVersion",
            "studyId",
            "studyRevision",
            "contractState",
            "referenceEpochUtc",
            "scope",
            "requirements",
            "criticalZoneCatalog",
            "hardwareInventory",
            "targetAndTrajectoryEnvelope",
            "scenarioCorpus",
            "fusionPolicy",
            "mountRegionPolicy",
            "artifactBindings",
            "readiness",
            "solverBenchmark",
            "rlGoNoGo",
            "governance",
        ),
    )
    _literal("schemaVersion", item["schemaVersion"], STAGE0_STUDY_SCHEMA)
    _text("studyId", item["studyId"])
    _integer("studyRevision", item["studyRevision"], minimum=1)
    contract_state = _choice(
        "contractState", item["contractState"], ("DRAFT", "FROZEN")
    )
    _utc_timestamp("referenceEpochUtc", item["referenceEpochUtc"])

    artifacts = _validate_artifacts(
        item["artifactBindings"],
        artifact_base_directory,
        verified_artifact_sha256,
    )
    _validate_scope(item["scope"])
    (
        critical_zone_ids,
        critical_trajectory_ids,
        minimum_independent_families,
    ) = _validate_requirements(item["requirements"])
    zone_catalog_artifact_id = _validate_critical_zone_catalog(
        item["criticalZoneCatalog"], artifacts, critical_zone_ids
    )
    passive_band_ids, sensor_evidence = _validate_inventory(
        item["hardwareInventory"], artifacts
    )
    _, trajectory_artifact_id = _validate_target_envelope(
        item["targetAndTrajectoryEnvelope"],
        artifacts,
        passive_band_ids,
        critical_trajectory_ids,
    )
    partitions, scenario_artifact_ids = _validate_scenario_corpus(
        item["scenarioCorpus"], artifacts
    )
    fusion_artifact_id = _validate_fusion(
        item["fusionPolicy"], artifacts, minimum_independent_families
    )
    mount_evidence_ids, privacy_evidence_ids, any_eligible_mount = (
        _validate_mount_regions(item["mountRegionPolicy"], artifacts)
    )
    expected_gate_evidence = {
        "ENVIRONMENT": _artifact_ids_for_roles(
            artifacts, "MAP", "RENDER_GEOMETRY", "COLLISION_GEOMETRY"
        )
        | {zone_catalog_artifact_id},
        "RF_PROPAGATION": _artifact_ids_for_roles(
            artifacts,
            "RF_GEOMETRY",
            "RF_MATERIAL_CATALOG",
            "RF_NATIVE_TEST_RECEIPT",
        ),
        "RADAR_MODEL": set(sensor_evidence["radar"]),
        "PASSIVE_RF_MODEL": set(sensor_evidence["passive_rf"]),
        "RGB_MODEL": set(sensor_evidence["rgb"]),
        "EVENT_CAMERA_MODEL": set(sensor_evidence["event_camera"]),
        "FUSION_POLICY": {fusion_artifact_id},
        "EVALUATION_CORPUS": {trajectory_artifact_id} | scenario_artifact_ids,
        "MOUNT_PERMISSIONS_AND_RESOURCES": mount_evidence_ids,
        "PRIVACY": privacy_evidence_ids,
    }
    gate_statuses = _validate_readiness(
        item["readiness"], artifacts, expected_gate_evidence
    )
    if any_eligible_mount:
        for gate_id in ("MOUNT_PERMISSIONS_AND_RESOURCES", "PRIVACY"):
            if gate_statuses[gate_id] != "PASSED":
                raise ValueError(
                    "eligible mount regions require matching PASSED readiness gates: "
                    f"{gate_id} is {gate_statuses[gate_id]}"
                )
    solver_ids, exact_solver_id, budget_id = _validate_solver_benchmark(
        item["solverBenchmark"], artifacts, partitions
    )
    _validate_rl_decision(
        item["rlGoNoGo"],
        artifacts,
        solver_ids,
        exact_solver_id,
        budget_id,
        partitions,
    )
    _validate_governance(item["governance"], contract_state, artifacts)
    return item


def _deep_freeze(value: Any) -> Any:
    if isinstance(value, Mapping):
        return MappingProxyType(
            {key: _deep_freeze(item) for key, item in value.items()}
        )
    if isinstance(value, (list, tuple)):
        return tuple(_deep_freeze(item) for item in value)
    return value


def _json_ready(value: Any) -> Any:
    if isinstance(value, Mapping):
        return {key: _json_ready(item) for key, item in value.items()}
    if isinstance(value, (list, tuple)):
        return [_json_ready(item) for item in value]
    return value


def _load_json_without_duplicate_keys(path: str | Path) -> object:
    def reject_duplicates(pairs: list[tuple[str, object]]) -> dict[str, object]:
        result: dict[str, object] = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"duplicate JSON object key {key!r}")
            result[key] = value
        return result

    return json.loads(
        Path(path).read_text(encoding="utf-8"), object_pairs_hook=reject_duplicates
    )


@dataclass(frozen=True, slots=True)
class Stage0StudyContract:
    """Validated immutable Stage-0 study declaration."""

    payload: Mapping[str, Any]
    artifact_base_directory: Path | None = field(
        default=None, repr=False, compare=False
    )
    verified_artifact_sha256: Mapping[str, str] | None = field(
        default=None, repr=False, compare=False
    )

    def __post_init__(self) -> None:
        artifact_base_directory = self.artifact_base_directory
        if artifact_base_directory is not None:
            artifact_base_directory = Path(artifact_base_directory).resolve()
        preverified = self.verified_artifact_sha256
        validated = _validate_contract(
            self.payload,
            artifact_base_directory,
            preverified,
        )
        object.__setattr__(self, "payload", _deep_freeze(_json_ready(validated)))
        object.__setattr__(self, "artifact_base_directory", artifact_base_directory)
        object.__setattr__(self, "verified_artifact_sha256", None)

    @classmethod
    def from_mapping(
        cls,
        value: Mapping[str, Any],
        *,
        artifact_base_directory: str | Path | None = None,
        verified_artifact_sha256: Mapping[str, str] | None = None,
    ) -> "Stage0StudyContract":
        return cls(
            value,
            None if artifact_base_directory is None else Path(artifact_base_directory),
            verified_artifact_sha256,
        )

    @classmethod
    def load(cls, path: str | Path) -> "Stage0StudyContract":
        source_path = Path(path).resolve(strict=True)
        value = _load_json_without_duplicate_keys(source_path)
        return cls(
            _object("stage0StudyContract", value),
            source_path.parent,
        )

    @property
    def schema_version(self) -> str:
        return str(self.payload["schemaVersion"])

    @property
    def study_id(self) -> str:
        return str(self.payload["studyId"])

    @property
    def digest(self) -> str:
        return stable_digest(self.to_dict())

    def to_dict(self) -> dict[str, Any]:
        return _json_ready(self.payload)

    def validation_receipt(self) -> dict[str, object]:
        return {
            "schemaVersion": STAGE0_VALIDATION_RECEIPT_SCHEMA,
            "valid": True,
            "contractSchemaVersion": self.schema_version,
            "studyId": self.study_id,
            "studyRevision": int(self.payload["studyRevision"]),
            "contractState": str(self.payload["contractState"]),
            "contractSha256": self.digest,
            "productionOptimizerImplementationAuthorized": False,
            "physicalDeploymentAuthorized": False,
        }


def validate_stage0_study_contract(
    value: Mapping[str, Any],
    *,
    artifact_base_directory: str | Path | None = None,
) -> Stage0StudyContract:
    """Validate and freeze a Stage-0 contract, raising on any ambiguity."""

    return Stage0StudyContract.from_mapping(
        value,
        artifact_base_directory=artifact_base_directory,
    )


__all__ = [
    "ALLOWED_SENSOR_CLASSES",
    "PROHIBITED_DEPLOYED_MODALITIES",
    "REQUIRED_GATE_IDS",
    "REQUIRED_SCENARIO_DIMENSIONS",
    "REQUIRED_SOLVER_METHODS",
    "STAGE0_STUDY_SCHEMA",
    "STAGE0_VALIDATION_RECEIPT_SCHEMA",
    "Stage0StudyContract",
    "validate_stage0_study_contract",
]
