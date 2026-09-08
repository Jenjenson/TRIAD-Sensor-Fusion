"""Adapt the bounded four-class study result for the desktop replay viewer.

The adapter consumes ``triad.four_class_placement_demo_study.v1`` and emits a
self-validating ``triad.dashboard_replay.v1`` document.  Only the exact robust
branch-and-bound selection is displayed.  Detection events are sourced from
the versioned precompute rows; no range gate, calibrated probability, latency
model, or live Unreal observation is invented here.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import re
from typing import Any, Mapping, Sequence

from .contracts import stable_digest
from .dashboard_replay import (
    DASHBOARD_REPLAY_SCHEMA,
    DEFAULT_MAP_PACKAGE,
    _DETECTION_SCORE_SEMANTICS,
    validate_dashboard_replay,
)
from .demo_study import DEMO_STUDY_SCHEMA
from .planning_pipeline import (
    BRANCH_AND_BOUND_ALGORITHM,
    CANDIDATE_TRAJECTORY_PRECOMPUTE_SCHEMA,
    DEMO_SURROGATE_SOURCE,
    SENSOR_CLASSES,
    STATIC_SOLVER_RESULT_SCHEMA,
)
from .study_contract import ALLOWED_SENSOR_CLASSES, PROHIBITED_DEPLOYED_MODALITIES
from .unreal_config import write_new_json_file


DASHBOARD_DEMO_ADAPTER = "four_class_precompute_dashboard_adapter_v2"
EXPECTED_SELECTED_CANDIDATE_IDS = ("demo-south", "demo-west")
EXPECTED_TRAJECTORY_IDS = (
    "demo-ingress-east",
    "demo-ingress-north",
    "demo-ingress-south",
    "demo-ingress-west-rf-silent",
)
DEFAULT_ACQUISITION_INTERVAL_SECONDS = 1.0
_MAXIMUM_ACQUISITION_INTERVAL_SECONDS = 5.0
_EARTH_RADIUS_METERS = 6_378_137.0
_DEMO_ORIGIN_LATITUDE_DEGREES = 1.30709615
_DEMO_ORIGIN_LONGITUDE_DEGREES = 103.84288055
_SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


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


def _mapping(name: str, value: object) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise TypeError(f"{name} must be an object")
    return value


def _sequence(name: str, value: object) -> Sequence[object]:
    if isinstance(value, (str, bytes)) or not isinstance(value, Sequence):
        raise TypeError(f"{name} must be an array")
    return value


def _text(name: str, value: object) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{name} must be a non-empty string")
    return value.strip()


def _digest_bound(name: str, value: Mapping[str, Any], digest_key: str) -> str:
    digest = value.get(digest_key)
    if not isinstance(digest, str) or not _SHA256_RE.fullmatch(digest):
        raise ValueError(f"{name}.{digest_key} must be a lowercase SHA-256 digest")
    payload = dict(value)
    payload.pop(digest_key, None)
    if stable_digest(payload) != digest:
        raise ValueError(f"{name}.{digest_key} does not match its content")
    return digest


def _exact_solver_proof(value: Mapping[str, Any]) -> dict[str, Any]:
    """Validate and retain the v2 proof fields displayed by the dashboard."""

    if value.get("schemaVersion") != STATIC_SOLVER_RESULT_SCHEMA:
        raise ValueError("dashboard adapter requires the v2 robust solver result")

    termination_reason = _text(
        "exact.terminationReason", value.get("terminationReason")
    )
    node_limit_reached = value.get("nodeLimitReached")
    if not isinstance(node_limit_reached, bool):
        raise TypeError("exact.nodeLimitReached must be boolean")
    if node_limit_reached != termination_reason.startswith("NODE_LIMIT_"):
        raise ValueError("exact node-limit flag contradicts its termination reason")

    explored_node_count = value.get("exploredNodeCount")
    node_limit = value.get("nodeLimit")
    if (
        isinstance(explored_node_count, bool)
        or not isinstance(explored_node_count, int)
        or explored_node_count < 0
    ):
        raise ValueError("exact.exploredNodeCount must be a non-negative integer")
    if (
        isinstance(node_limit, bool)
        or not isinstance(node_limit, int)
        or node_limit <= 0
    ):
        raise ValueError("exact.nodeLimit must be a positive integer")
    if explored_node_count > node_limit:
        raise ValueError("exact explored-node count cannot exceed its node limit")

    incumbent = _finite(
        "exact.incumbentObjectiveCostUnits",
        value.get("incumbentObjectiveCostUnits"),
    )
    lower_bound = _finite(
        "exact.objectiveCostLowerBoundUnits",
        value.get("objectiveCostLowerBoundUnits"),
    )
    absolute_gap = _finite(
        "exact.absoluteCostOptimalityGapUnits",
        value.get("absoluteCostOptimalityGapUnits"),
    )
    relative_gap = _finite(
        "exact.relativeCostOptimalityGap",
        value.get("relativeCostOptimalityGap"),
    )
    if min(incumbent, lower_bound, absolute_gap, relative_gap) < 0.0:
        raise ValueError("exact solver proof costs and gaps must be non-negative")
    if lower_bound > incumbent + 1e-9:
        raise ValueError("exact solver lower bound cannot exceed its incumbent")
    expected_absolute_gap = max(0.0, incumbent - min(incumbent, lower_bound))
    expected_relative_gap = (
        0.0
        if expected_absolute_gap <= 1e-15
        else expected_absolute_gap / max(abs(incumbent), 1e-15)
    )
    if not math.isclose(
        absolute_gap, expected_absolute_gap, rel_tol=0.0, abs_tol=1e-9
    ):
        raise ValueError("exact absolute cost gap contradicts incumbent and lower bound")
    if not math.isclose(
        relative_gap, expected_relative_gap, rel_tol=0.0, abs_tol=1e-12
    ):
        raise ValueError("exact relative cost gap contradicts incumbent and lower bound")

    bound_semantics = _text("exact.boundSemantics", value.get("boundSemantics"))
    if bound_semantics != "MATCHED_INCUMBENT_BY_EXHAUSTIVE_SEARCH":
        raise ValueError("completed exact result has unexpected bound semantics")
    if termination_reason != "SEARCH_EXHAUSTED_OPTIMAL":
        raise ValueError("completed exact result has an unexpected termination reason")
    if node_limit_reached:
        raise ValueError("completed exact result cannot report a reached node limit")
    if not math.isclose(lower_bound, incumbent, rel_tol=0.0, abs_tol=1e-9):
        raise ValueError("completed exact result must close its cost lower bound")
    if absolute_gap != 0.0 or relative_gap != 0.0:
        raise ValueError("completed exact result must report zero optimality gap")

    return {
        "schemaVersion": STATIC_SOLVER_RESULT_SCHEMA,
        "terminationReason": termination_reason,
        "incumbentObjectiveCostUnits": incumbent,
        "objectiveCostLowerBoundUnits": lower_bound,
        "absoluteCostOptimalityGapUnits": absolute_gap,
        "relativeCostOptimalityGap": relative_gap,
        "nodeLimitReached": node_limit_reached,
        "exploredNodeCount": explored_node_count,
        "nodeLimit": node_limit,
        "boundSemantics": bound_semantics,
    }


def _load_json_without_duplicate_keys(path: str | Path) -> dict[str, Any]:
    source = Path(path).resolve(strict=True)

    def reject_duplicates(pairs: list[tuple[str, object]]) -> dict[str, object]:
        result: dict[str, object] = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"{source.name} contains duplicate JSON key {key!r}")
            result[key] = value
        return result

    value = json.loads(
        source.read_text(encoding="utf-8-sig"), object_pairs_hook=reject_duplicates
    )
    if not isinstance(value, dict):
        raise TypeError("four-class study root must be an object")
    return value


def _safe_node_id(candidate_id: str) -> str:
    suffix = re.sub(r"[^A-Za-z0-9_]+", "_", candidate_id).strip("_")
    if not suffix:
        raise ValueError("candidate ID cannot be converted to a dashboard node ID")
    return f"PLACEMENT_{suffix}"[:96]


def _location_from_enu(
    east_meters: float, north_meters: float, up_meters: float
) -> dict[str, object]:
    latitude_radians = math.radians(_DEMO_ORIGIN_LATITUDE_DEGREES)
    latitude = _DEMO_ORIGIN_LATITUDE_DEGREES + math.degrees(
        north_meters / _EARTH_RADIUS_METERS
    )
    longitude = _DEMO_ORIGIN_LONGITUDE_DEGREES + math.degrees(
        east_meters
        / (_EARTH_RADIUS_METERS * max(abs(math.cos(latitude_radians)), 0.000001))
    )
    return {
        "latitudeDegrees": round(latitude, 10),
        "longitudeDegrees": round(longitude, 10),
        "heightMeters": round(up_meters, 6),
        "heightReference": "SYNTHETIC_LOCAL_UP_NOT_SURVEY_DATUM",
    }


def _source_indexes(study: Mapping[str, Any]) -> dict[str, Any]:
    if study.get("schemaVersion") != DEMO_STUDY_SCHEMA:
        raise ValueError("unsupported four-class demo-study schema")
    study_digest = _digest_bound("study", study, "artifactSha256")
    if study.get("executionClass") != "DEMO_STUDY_ONLY":
        raise ValueError("adapter only accepts the explicitly demo-only study")
    if study.get("fieldPerformanceClaimed") is not False:
        raise ValueError("source study must not claim field performance")
    if study.get("physicalDeploymentAuthorized") is not False:
        raise ValueError("source study must not authorize physical deployment")

    readiness = _mapping("study.stage0Readiness", study.get("stage0Readiness"))
    _digest_bound("study.stage0Readiness", readiness, "receiptSha256")
    if readiness.get("executionClass") != "DEMO_STUDY_ONLY":
        raise ValueError("Stage-0 receipt is not demo-only")
    if readiness.get("productionOptimizationAuthorized") is not False:
        raise ValueError("Stage-0 production optimization must remain blocked")
    if readiness.get("physicalDeploymentAuthorized") is not False:
        raise ValueError("Stage-0 physical deployment must remain blocked")
    blockers = tuple(_sequence("study.stage0Readiness.blockers", readiness.get("blockers")))
    if not blockers:
        raise ValueError("Stage-0 blocker list must not be empty")

    precompute = _mapping(
        "study.candidateTrajectoryPrecompute",
        study.get("candidateTrajectoryPrecompute"),
    )
    if precompute.get("schemaVersion") != CANDIDATE_TRAJECTORY_PRECOMPUTE_SCHEMA:
        raise ValueError("unsupported candidate/trajectory precompute schema")
    precompute_digest = _digest_bound(
        "study.candidateTrajectoryPrecompute", precompute, "artifactSha256"
    )
    for key, expected in {
        "executionClass": "DEMO_STUDY_ONLY",
        "sourceClass": DEMO_SURROGATE_SOURCE,
        "exactSimulatorReplayComplete": False,
        "measuredCalibrationUsed": False,
        "rfGeometryQueried": False,
        "fieldPerformanceClaimed": False,
        "physicalDeploymentAuthorized": False,
    }.items():
        if precompute.get(key) != expected:
            raise ValueError(f"precompute truth label {key} must remain {expected!r}")

    comparison = _mapping(
        "study.staticSolverComparison", study.get("staticSolverComparison")
    )
    if comparison.get("samePrecomputeArtifactSha256") != precompute_digest:
        raise ValueError("solver comparison is not bound to the supplied precompute")
    exact = _mapping(
        "study.staticSolverComparison.robustBranchAndBound",
        comparison.get("robustBranchAndBound"),
    )
    exact_digest = _digest_bound(
        "study.staticSolverComparison.robustBranchAndBound",
        exact,
        "resultSha256",
    )
    if exact.get("algorithm") != BRANCH_AND_BOUND_ALGORITHM:
        raise ValueError("dashboard adapter requires the robust branch-and-bound result")
    if exact.get("status") != "OPTIMAL" or exact.get("optimalityProven") is not True:
        raise ValueError("dashboard adapter requires a completed optimal static solve")
    if exact.get("infeasibilityProven") is not False:
        raise ValueError("source exact result has contradictory proof flags")
    if exact.get("physicalDeploymentAuthorized") is not False:
        raise ValueError("source exact result must remain review-only")
    solver_proof = _exact_solver_proof(exact)
    exact_metrics = _mapping(
        "study.staticSolverComparison.robustBranchAndBound.metrics",
        exact.get("metrics"),
    )
    total_cost = _finite(
        "exact.metrics.totalCostUnits", exact_metrics.get("totalCostUnits")
    )
    if not math.isclose(
        total_cost,
        solver_proof["incumbentObjectiveCostUnits"],
        rel_tol=0.0,
        abs_tol=1e-9,
    ):
        raise ValueError("exact incumbent cost contradicts the selected-set metrics")
    selected_ids = tuple(
        sorted(
            _text("selectedCandidateIds[]", item)
            for item in _sequence(
                "exact.selectedCandidateIds", exact.get("selectedCandidateIds")
            )
        )
    )
    if selected_ids != EXPECTED_SELECTED_CANDIDATE_IDS:
        raise ValueError(
            "bounded adapter expected the current exact selection "
            f"{EXPECTED_SELECTED_CANDIDATE_IDS}, received {selected_ids}"
        )

    candidate_index: dict[str, Mapping[str, Any]] = {}
    for index, raw in enumerate(
        _sequence("precompute.candidates", precompute.get("candidates"))
    ):
        candidate = _mapping(f"precompute.candidates[{index}]", raw)
        candidate_id = _text("candidate.candidateId", candidate.get("candidateId"))
        if candidate_id in candidate_index:
            raise ValueError(f"duplicate precompute candidate {candidate_id!r}")
        candidate_index[candidate_id] = candidate
    if any(candidate_id not in candidate_index for candidate_id in selected_ids):
        raise ValueError("exact selection references a candidate absent from precompute")
    selected_cost = sum(
        _finite(
            f"precompute.candidates[{candidate_id}].costUnits",
            candidate_index[candidate_id].get("costUnits"),
        )
        for candidate_id in selected_ids
    )
    if not math.isclose(
        selected_cost,
        solver_proof["incumbentObjectiveCostUnits"],
        rel_tol=0.0,
        abs_tol=1e-9,
    ):
        raise ValueError("exact incumbent cost contradicts selected candidate costs")

    trajectory_index: dict[str, Mapping[str, Any]] = {}
    for index, raw in enumerate(
        _sequence("precompute.trajectories", precompute.get("trajectories"))
    ):
        trajectory = _mapping(f"precompute.trajectories[{index}]", raw)
        trajectory_id = _text(
            "trajectory.trajectoryId", trajectory.get("trajectoryId")
        )
        if trajectory_id in trajectory_index:
            raise ValueError(f"duplicate precompute trajectory {trajectory_id!r}")
        trajectory_index[trajectory_id] = trajectory
    if tuple(sorted(trajectory_index)) != EXPECTED_TRAJECTORY_IDS:
        raise ValueError(
            "bounded adapter requires exactly the four study ingress trajectories"
        )

    calibration_bundle = _mapping(
        "study.sensorModelCalibration", study.get("sensorModelCalibration")
    )
    calibration_digest = _digest_bound(
        "study.sensorModelCalibration", calibration_bundle, "bundleSha256"
    )
    if calibration_bundle.get("allModelsMeasured") is not False:
        raise ValueError("demo model bundle must remain explicitly unmeasured")
    if calibration_bundle.get("fieldPerformanceClaimed") is not False:
        raise ValueError("demo model bundle must not claim field performance")
    calibration_index: dict[str, Mapping[str, Any]] = {}
    for index, raw in enumerate(
        _sequence("calibrationBundle.calibrations", calibration_bundle.get("calibrations"))
    ):
        calibration = _mapping(f"calibrations[{index}]", raw)
        sensor_class = _text("calibration.sensorClass", calibration.get("sensorClass"))
        if calibration.get("evidenceClass") != "SYNTHETIC_DEMO_ASSUMPTION":
            raise ValueError("adapter accepts only explicitly synthetic demo calibrations")
        if calibration.get("datasetSha256") is not None:
            raise ValueError("synthetic calibration must not imply a measured dataset")
        calibration_index[sensor_class] = calibration
    if set(calibration_index) != set(SENSOR_CLASSES):
        raise ValueError("calibration bundle must contain exactly all four sensor classes")

    result_index: dict[tuple[str, str, str], Mapping[str, Any]] = {}
    for index, raw in enumerate(
        _sequence("precompute.results", precompute.get("results"))
    ):
        row = _mapping(f"precompute.results[{index}]", raw)
        key = (
            _text("result.candidateId", row.get("candidateId")),
            _text("result.trajectoryId", row.get("trajectoryId")),
            _text("result.sensorClass", row.get("sensorClass")),
        )
        if key in result_index:
            raise ValueError(f"duplicate precompute result row {key!r}")
        if row.get("sourceClass") != DEMO_SURROGATE_SOURCE:
            raise ValueError("precompute result has an unexpected source class")
        for flag in ("exactSimulatorReplay", "measuredCalibration", "rfGeometryQueried"):
            if row.get(flag) is not False:
                raise ValueError(f"precompute result {flag} must remain false")
        result_index[key] = row
    expected_selected_rows = {
        (candidate_id, trajectory_id, sensor_class)
        for candidate_id in selected_ids
        for trajectory_id in EXPECTED_TRAJECTORY_IDS
        for sensor_class in SENSOR_CLASSES
    }
    if not expected_selected_rows.issubset(result_index):
        missing = sorted(expected_selected_rows - set(result_index))
        raise ValueError(f"selected candidate precompute rows are incomplete: {missing}")

    return {
        "studyDigest": study_digest,
        "precompute": precompute,
        "precomputeDigest": precompute_digest,
        "exact": exact,
        "exactDigest": exact_digest,
        "solverProof": solver_proof,
        "selectedIds": selected_ids,
        "candidateIndex": candidate_index,
        "trajectoryIndex": trajectory_index,
        "calibrationIndex": calibration_index,
        "calibrationDigest": calibration_digest,
        "resultIndex": result_index,
        "readiness": readiness,
        "blockers": blockers,
    }


def _point_components(raw: object, *, name: str) -> tuple[float, float, float]:
    point = _sequence(name, raw)
    if len(point) != 3:
        raise ValueError(f"{name} must contain east, north, and up")
    return tuple(_finite(f"{name}[]", item) for item in point)  # type: ignore[return-value]


def _track_samples(
    trajectory: Mapping[str, Any], acquisition_interval: float
) -> tuple[list[dict[str, Any]], dict[float, tuple[float, float, float]]]:
    source_points: list[tuple[float, float, float, float]] = []
    for index, raw in enumerate(_sequence("trajectory.points", trajectory.get("points"))):
        point = _mapping(f"trajectory.points[{index}]", raw)
        source_time = _finite("trajectory point timeSeconds", point.get("timeSeconds"))
        east, north, up = _point_components(
            point.get("enuMeters"), name="trajectory point enuMeters"
        )
        source_points.append((source_time, east, north, up))
    if not source_points:
        raise ValueError("trajectory has no source points")
    if [item[0] for item in source_points] != sorted({item[0] for item in source_points}):
        raise ValueError("trajectory point times must be strictly increasing")

    position_at_source_time = {
        item[0]: (item[1], item[2], item[3]) for item in source_points
    }

    def motion(index: int) -> tuple[float, float]:
        if len(source_points) == 1:
            return 0.0, 0.0
        left = source_points[index]
        right = source_points[index + 1] if index + 1 < len(source_points) else source_points[index - 1]
        dt = right[0] - left[0]
        if dt < 0.0:
            left, right, dt = right, left, -dt
        if dt <= 0.0:
            return 0.0, 0.0
        de = right[1] - left[1]
        dn = right[2] - left[2]
        du = right[3] - left[3]
        speed = math.sqrt(de * de + dn * dn + du * du) / dt
        heading = (math.degrees(math.atan2(de, dn)) + 360.0) % 360.0
        return heading, speed

    first = source_points[0]
    samples: list[dict[str, Any]] = [
        {
            "tSeconds": 0.0,
            "location": _location_from_enu(first[1], first[2], first[3]),
            "offsetEnuMeters": {
                "east": first[1],
                "north": first[2],
                "up": first[3],
            },
            "headingDegrees": motion(0)[0],
            "speedMetersPerSecond": 0.0,
            "distanceToAoiCenterMeters": round(math.hypot(first[1], first[2]), 6),
            "insideAoi": math.hypot(first[1], first[2]) <= 1000.0 + 1e-9,
            "displayPhase": "INITIAL_NO_DETECTION_ACQUISITION_INTERVAL",
        }
    ]
    for index, (source_time, east, north, up) in enumerate(source_points):
        heading, speed = motion(index)
        samples.append(
            {
                "tSeconds": round(acquisition_interval + source_time, 6),
                "sourcePrecomputeTimeSeconds": source_time,
                "location": _location_from_enu(east, north, up),
                "offsetEnuMeters": {"east": east, "north": north, "up": up},
                "headingDegrees": round(heading, 6),
                "speedMetersPerSecond": round(speed, 6),
                "distanceToAoiCenterMeters": round(math.hypot(east, north), 6),
                "insideAoi": math.hypot(east, north) <= 1000.0 + 1e-9,
                "displayPhase": "VERSIONED_PRECOMPUTE_TRAJECTORY",
            }
        )
    return samples, position_at_source_time


def _range_at_first_detection(
    candidate: Mapping[str, Any],
    position_at_source_time: Mapping[float, tuple[float, float, float]],
    first_detection_seconds: float,
) -> float:
    target = position_at_source_time.get(first_detection_seconds)
    if target is None:
        raise ValueError(
            "precompute firstDetectionSeconds does not identify a trajectory point"
        )
    sensor = _point_components(candidate.get("poseEnuMeters"), name="candidate.poseEnuMeters")
    return math.sqrt(sum((left - right) ** 2 for left, right in zip(sensor, target, strict=True)))


def build_dashboard_replay_from_demo_study(
    study: Mapping[str, Any],
    *,
    map_package: str = DEFAULT_MAP_PACKAGE,
    acquisition_interval_seconds: float = DEFAULT_ACQUISITION_INTERVAL_SECONDS,
) -> dict[str, Any]:
    """Create the bounded desktop replay from the versioned demo result."""

    if not isinstance(map_package, str) or not map_package.startswith("/Game/Maps/"):
        raise ValueError("map_package must be an Unreal /Game/Maps package")
    acquisition = _finite(
        "acquisition_interval_seconds", acquisition_interval_seconds
    )
    if not 0.0 < acquisition <= _MAXIMUM_ACQUISITION_INTERVAL_SECONDS:
        raise ValueError(
            "acquisition_interval_seconds must be in (0, 5] for this bounded replay"
        )
    indexes = _source_indexes(study)
    selected_ids: tuple[str, ...] = indexes["selectedIds"]
    candidates: Mapping[str, Mapping[str, Any]] = indexes["candidateIndex"]
    trajectories: Mapping[str, Mapping[str, Any]] = indexes["trajectoryIndex"]
    calibrations: Mapping[str, Mapping[str, Any]] = indexes["calibrationIndex"]
    results: Mapping[tuple[str, str, str], Mapping[str, Any]] = indexes[
        "resultIndex"
    ]

    sensors: list[dict[str, Any]] = []
    sensor_id_by_candidate_class: dict[tuple[str, str], str] = {}
    node_id_by_candidate: dict[str, str] = {}
    for candidate_id in selected_ids:
        candidate = candidates[candidate_id]
        candidate_classes = tuple(
            sorted(
                _text("candidate.sensorClasses[]", item)
                for item in _sequence(
                    "candidate.sensorClasses", candidate.get("sensorClasses")
                )
            )
        )
        if candidate_classes != tuple(sorted(SENSOR_CLASSES)):
            raise ValueError(
                f"selected candidate {candidate_id!r} does not contain all four classes"
            )
        east, north, up = _point_components(
            candidate.get("poseEnuMeters"), name="candidate.poseEnuMeters"
        )
        node_id = _safe_node_id(candidate_id)
        node_id_by_candidate[candidate_id] = node_id
        modalities: list[dict[str, Any]] = []
        for sensor_class in candidate_classes:
            calibration = calibrations[sensor_class]
            parameters = _mapping(
                "calibration.parameters", calibration.get("parameters")
            )
            sensor_id = f"{node_id}:{sensor_class}"
            sensor_id_by_candidate_class[(candidate_id, sensor_class)] = sensor_id
            modalities.append(
                {
                    "sensorId": sensor_id,
                    "modality": sensor_class,
                    "sensorClass": sensor_class,
                    "family": _text(
                        "calibration.evidenceFamily",
                        calibration.get("evidenceFamily"),
                    ),
                    "nominalRangeMeters": _finite(
                        "calibration maximumRangeMeters",
                        parameters.get("maximumRangeMeters"),
                    ),
                    "simulated": True,
                    "calibrated": False,
                    "evidenceClass": "SYNTHETIC_DEMO_ASSUMPTION",
                    "calibrationId": calibration.get("calibrationId"),
                }
            )
        sensors.append(
            {
                "nodeId": node_id,
                "optionId": candidate_id,
                "siteId": _text("candidate.siteId", candidate.get("siteId")),
                "packageId": "four-class-demo-package",
                "orientationId": f"yaw-{_finite('candidate.yawDegrees', candidate.get('yawDegrees')):g}",
                "failureDomainId": _text(
                    "candidate.failureDomainId", candidate.get("failureDomainId")
                ),
                "location": _location_from_enu(east, north, up),
                "modalities": modalities,
                "installedCostUnits": _finite(
                    "candidate.costUnits", candidate.get("costUnits")
                ),
                "rationale": {
                    "summary": (
                        "Exact member of the completed robust branch-and-bound "
                        "selection over the bound synthetic precompute; mount approval "
                        "and field performance are not inferred."
                    ),
                    "codes": [
                        "EXACT_ROBUST_SELECTION_MEMBER",
                        "FOUR_CLASS_DEMO_PACKAGE",
                        "STAGE0_BLOCKED",
                    ],
                    "families": ["active_radar", "passive_rf", "visual"],
                    "marginalCoverageMetricAvailable": False,
                },
            }
        )

    tracks: list[dict[str, Any]] = []
    detections: list[dict[str, Any]] = []
    maximum_source_time = 0.0
    positive_intervals: list[float] = []
    for trajectory_id in EXPECTED_TRAJECTORY_IDS:
        trajectory = trajectories[trajectory_id]
        samples, position_at_source_time = _track_samples(trajectory, acquisition)
        source_times = sorted(position_at_source_time)
        maximum_source_time = max(maximum_source_time, source_times[-1])
        positive_intervals.extend(
            right - left
            for left, right in zip(source_times, source_times[1:])
            if right > left
        )
        track_id = f"SIM-{trajectory_id.upper().replace('-', '_')}"
        track_events: list[dict[str, Any]] = []
        for candidate_id in selected_ids:
            candidate = candidates[candidate_id]
            calibration_by_class = calibrations
            for sensor_class in SENSOR_CLASSES:
                row = results[(candidate_id, trajectory_id, sensor_class)]
                first_raw = row.get("firstDetectionSeconds")
                fraction = _unit(
                    "precompute detectionFraction", row.get("detectionFraction")
                )
                if first_raw is None:
                    if fraction != 0.0:
                        raise ValueError(
                            "positive detectionFraction requires firstDetectionSeconds"
                        )
                    continue
                first_source_time = _finite(
                    "precompute firstDetectionSeconds", first_raw
                )
                if fraction <= 0.0:
                    raise ValueError(
                        "firstDetectionSeconds requires positive detectionFraction"
                    )
                event_time = round(acquisition + first_source_time, 6)
                if event_time <= 0.0:
                    raise ValueError("adapter must retain an initial no-detection interval")
                mean_quality = _unit("precompute meanQuality", row.get("meanQuality"))
                calibration = calibration_by_class[sensor_class]
                parameters = _mapping(
                    "calibration.parameters", calibration.get("parameters")
                )
                event = {
                    "eventId": f"det:{track_id}:{candidate_id}:{sensor_class}",
                    "tSeconds": event_time,
                    "sourcePrecomputeTimeSeconds": first_source_time,
                    "trackId": track_id,
                    "nodeId": node_id_by_candidate[candidate_id],
                    "sensorId": sensor_id_by_candidate_class[
                        (candidate_id, sensor_class)
                    ],
                    "modality": sensor_class,
                    "sensorClass": sensor_class,
                    "family": row.get("evidenceFamily"),
                    "rangeMeters": round(
                        _range_at_first_detection(
                            candidate, position_at_source_time, first_source_time
                        ),
                        3,
                    ),
                    "effectiveRangeMeters": _finite(
                        "calibration maximumRangeMeters",
                        parameters.get("maximumRangeMeters"),
                    ),
                    "confidence": mean_quality,
                    "confidenceSemantics": _DETECTION_SCORE_SEMANTICS,
                    "confidenceSourceField": "meanQuality",
                    "detectionFraction": fraction,
                    "latencyMilliseconds": round(event_time * 1000.0, 3),
                    "latencySemantics": (
                        "DISPLAY_TIME_FROM_REPLAY_START_DERIVED_FROM_ACQUISITION_"
                        "OFFSET_PLUS_PRECOMPUTE_FIRST_DETECTION_NOT_HARDWARE_LATENCY"
                    ),
                    "source": DEMO_SURROGATE_SOURCE,
                    "sourcePrecomputeRowSha256": stable_digest(dict(row)),
                    "exactSimulatorReplay": False,
                    "measuredCalibration": False,
                    "rfGeometryQueried": False,
                    "detected": True,
                    "simulationOnly": True,
                }
                detections.append(event)
                track_events.append(event)
        first_detection = min(
            (float(item["tSeconds"]) for item in track_events), default=None
        )
        tracks.append(
            {
                "trackId": track_id,
                "displayName": trajectory_id,
                "scenarioKind": "SYNTHETIC_DEMO_UAS_INGRESS",
                "scenarioId": trajectory.get("scenarioId"),
                "partitionId": trajectory.get("partitionId"),
                "rfEmitting": trajectory.get("rfEmitting"),
                "ingressCorridorId": trajectory_id,
                "inboundApproachScenario": True,
                "trajectory": "VERSIONED_PRECOMPUTE_POLYLINE",
                "samples": samples,
                "outcome": {
                    "status": (
                        "DETECTED_IN_SYNTHETIC_PRECOMPUTE"
                        if first_detection is not None
                        else "NO_DETECTION_IN_SYNTHETIC_PRECOMPUTE"
                    ),
                    "firstDetectionSeconds": first_detection,
                    "firstCorroboratedDetectionSeconds": None,
                    "corroborationStatus": (
                        "NOT_DERIVED_AGGREGATE_ROWS_DO_NOT_PROVE_SIMULTANEOUS_FUSION"
                    ),
                    "detectionEventCount": len(track_events),
                },
            }
        )

    detections.sort(
        key=lambda item: (
            float(item["tSeconds"]),
            str(item["trackId"]),
            str(item["sensorId"]),
        )
    )
    cadence = min(positive_intervals) if positive_intervals else acquisition
    detected_track_count = sum(
        item["outcome"]["firstDetectionSeconds"] is not None for item in tracks
    )
    exact = indexes["exact"]
    exact_metrics = dict(_mapping("exact.metrics", exact.get("metrics")))
    payload: dict[str, Any] = {
        "schemaVersion": DASHBOARD_REPLAY_SCHEMA,
        "replayId": f"four-class-demo:{indexes['exactDigest'][:12]}",
        "simulationOnly": True,
        "detectionOnly": True,
        "operationalUseAuthorized": False,
        "siteAuthorizationInferred": False,
        "calibratedProbabilities": False,
        "actionsTaken": "none",
        "map": {
            "unrealPackage": map_package,
            "aoi": {
                "id": "istana-1km-demo-adapter",
                "center": {
                    "latitudeDegrees": _DEMO_ORIGIN_LATITUDE_DEGREES,
                    "longitudeDegrees": _DEMO_ORIGIN_LONGITUDE_DEGREES,
                    "heightMeters": 0.0,
                    "heightReference": "SYNTHETIC_LOCAL_UP_NOT_SURVEY_DATUM",
                },
                "radiusMeters": 1000.0,
                "candidateBufferMeters": 0.0,
                "centerProvenance": (
                    "Adapter-bound provisional public-reference Istana centre; "
                    "the source demo ENU cache does not contain a surveyed origin."
                ),
                "surfaceHeightSource": "SYNTHETIC_LOCAL_UP_NOT_SURVEY_DATUM",
            },
            "coordinateBinding": {
                "source": "ADAPTER_DECLARED_PROVISIONAL_ORIGIN",
                "surveyRegistered": False,
                "performanceInferenceAllowed": False,
            },
        },
        "scenario": {
            "scenarioId": "four-versioned-demo-ingress-trajectories",
            "weatherProfile": "PER_TRAJECTORY_VERSIONED_DEMO_FACTOR",
            "rfEmitting": "PER_TRAJECTORY",
            "trajectoryScenarioIds": sorted(
                str(item.get("scenarioId")) for item in trajectories.values()
            ),
            "runtimeConfigEnabledInSource": False,
            "stage0Authorization": "BLOCKED_DEMO_STUDY_ONLY",
        },
        "stage0Scope": {
            "allowedSensorClasses": list(ALLOWED_SENSOR_CLASSES),
            "prohibitedDeployedModalities": list(PROHIBITED_DEPLOYED_MODALITIES),
            "representedSensorClasses": sorted(SENSOR_CLASSES),
            "missingSensorClasses": [],
            "filteredRecommendationModalities": [],
            "legacyRecommendationScopeComplete": True,
            "contractState": indexes["readiness"].get("contractState"),
            "executionClass": "DEMO_STUDY_ONLY",
            "productionOptimizationAuthorized": False,
            "physicalDeploymentAuthorized": False,
            "blockers": list(indexes["blockers"]),
            "warnings": [
                "Stage-0 is blocked; this is a synthetic four-class study replay only.",
                "No displayed candidate is an approved physical mount.",
                "No detection is live Unreal telemetry, measured performance, or a calibrated probability.",
            ],
        },
        "recommendation": {
            "requestId": "four-class-demo-study",
            "status": exact.get("status"),
            "algorithm": exact.get("algorithm"),
            "sourceRecommendationDigest": indexes["exactDigest"],
            "sourcePrecomputeArtifactSha256": indexes["precomputeDigest"],
            "totalCostUnits": exact_metrics.get("totalCostUnits"),
            "selectedSiteCount": len(sensors),
            "selectedCandidateIds": list(selected_ids),
            "optimalityProvenForPrecomputedBinaryModel": True,
            "solverProof": dict(indexes["solverProof"]),
            "coverageMetricLabel": "Precomputed-model worst coverage",
            "metrics": exact_metrics,
            "rationale": (
                "The dashboard maps exactly the completed robust branch-and-bound "
                "selection. Optimality applies only to the bound synthetic binary "
                "precompute model; site approval and field performance are not inferred."
            ),
        },
        "sensors": sensors,
        "timeline": {
            "timeBasis": "RELATIVE_SYNTHETIC_REPLAY_SECONDS",
            "durationSeconds": round(acquisition + maximum_source_time, 6),
            "cadenceSeconds": round(cadence, 6),
            "initialNoDetectionIntervalSeconds": acquisition,
            "tracks": tracks,
            "detections": detections,
            "successSummary": {
                "simulatedTrackCount": len(tracks),
                "detectedTrackCount": detected_track_count,
                "corroboratedTrackCount": 0,
                "allTracksDetected": detected_track_count == len(tracks),
                "corroborationNotEvaluatedFromAggregateRows": True,
            },
        },
        "detectionPolicy": {
            "eventEligibility": (
                "VERSIONED_PRECOMPUTE_ROW_FIRST_DETECTION_SECONDS_IS_NOT_NULL"
            ),
            "confidenceSourceField": "meanQuality",
            "minimumIndependentFamilyCount": exact_metrics.get("policy", {}).get(
                "minimumEvidenceFamilies"
            ),
            "minimumContributingSiteCount": exact_metrics.get("policy", {}).get(
                "minimumSiteRedundancy"
            ),
            "minimumFailureDomainCount": exact_metrics.get("policy", {}).get(
                "minimumFailureDomainRedundancy"
            ),
            "confidenceThreshold": None,
            "initialDisplayAcquisitionIntervalSeconds": acquisition,
            "acquisitionIntervalSemantics": (
                "DISPLAY_ONLY_TIMELINE_OFFSET_NOT_MEASURED_SENSOR_DWELL"
            ),
            "simultaneousFusionClaimed": False,
        },
        "modelCard": {
            "modelId": DASHBOARD_DEMO_ADAPTER,
            "deterministic": True,
            "confidenceSemantics": _DETECTION_SCORE_SEMANTICS,
            "calibrated": False,
            "limitations": [
                "Offline synthetic study replay; not live Unreal telemetry.",
                "Events expose only firstDetectionSeconds and meanQuality already present in each versioned precompute row.",
                "The one-second initial acquisition interval is a display offset, not measured dwell or latency.",
                "Aggregate rows do not prove simultaneous multi-family fusion, so corroboration is deliberately unset.",
                "RF geometry was hash-loaded by the source study but was not queried by this kinematic surrogate.",
                "The adapter binds a provisional public-reference ENU origin because the source demo cache is not survey registered.",
            ],
        },
        "provenance": {
            "generator": DASHBOARD_DEMO_ADAPTER,
            "sourceStudyArtifactSha256": indexes["studyDigest"],
            "sourcePrecomputeArtifactSha256": indexes["precomputeDigest"],
            "sourceExactSolverResultSha256": indexes["exactDigest"],
            "sourceCalibrationBundleSha256": indexes["calibrationDigest"],
            "eventTimingRule": (
                "replay tSeconds = display acquisition interval + source row firstDetectionSeconds"
            ),
            "sourceRowsRetainedBySha256": True,
        },
    }
    payload["replayDigest"] = stable_digest(payload)
    validate_dashboard_replay(payload)
    return payload


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Adapt a triad.four_class_placement_demo_study.v1 artifact into a "
            "create-new triad.dashboard_replay.v1 JSON file."
        )
    )
    parser.add_argument("study", type=Path, help="four-class demo-study JSON")
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--map-package", default=DEFAULT_MAP_PACKAGE)
    parser.add_argument(
        "--acquisition-interval-seconds",
        type=float,
        default=DEFAULT_ACQUISITION_INTERVAL_SECONDS,
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    study = _load_json_without_duplicate_keys(args.study)
    replay = build_dashboard_replay_from_demo_study(
        study,
        map_package=args.map_package,
        acquisition_interval_seconds=args.acquisition_interval_seconds,
    )
    write_new_json_file(replay, args.output)
    print(
        json.dumps(
            {
                "schemaVersion": DASHBOARD_REPLAY_SCHEMA,
                "output": str(args.output),
                "replayDigest": replay["replayDigest"],
                "selectedCandidateIds": replay["recommendation"][
                    "selectedCandidateIds"
                ],
                "simulatedTrackCount": replay["timeline"]["successSummary"][
                    "simulatedTrackCount"
                ],
                "detectedTrackCount": replay["timeline"]["successSummary"][
                    "detectedTrackCount"
                ],
                "executionClass": "DEMO_STUDY_ONLY",
            },
            separators=(",", ":"),
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


__all__ = [
    "DASHBOARD_DEMO_ADAPTER",
    "DEFAULT_ACQUISITION_INTERVAL_SECONDS",
    "EXPECTED_SELECTED_CANDIDATE_IDS",
    "EXPECTED_TRAJECTORY_IDS",
    "build_dashboard_replay_from_demo_study",
    "main",
]
