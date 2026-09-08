"""Deterministic, simulation-only placement replay export for operator UIs.

The exporter joins an existing placement request/recommendation with the
configured Unreal demo-target trajectories.  It intentionally produces an
offline analytic replay, not live Unreal telemetry and not calibrated field
performance.  Live dashboards should continue to consume the layered-runtime
``/api/snapshot`` feed; this artifact is for reproducible demonstrations and
review of a recommendation before any site is authorised.
"""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import re
from typing import Any, Mapping, Sequence

from .contracts import (
    PlacementRecommendation,
    PlacementRequest,
    family_for_modality,
    stable_digest,
)
from .planning_pipeline import (
    BRANCH_AND_BOUND_ALGORITHM,
    STATIC_SOLVER_RESULT_SCHEMA,
)
from .study_contract import ALLOWED_SENSOR_CLASSES, PROHIBITED_DEPLOYED_MODALITIES
from .unreal_config import write_new_json_file


DASHBOARD_REPLAY_SCHEMA = "triad.dashboard_replay.v1"
DASHBOARD_REPLAY_GENERATOR = "deterministic_range_gate_demo_replay_v2"
DEFAULT_MAP_PACKAGE = "/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"

_WGS84_EQUATORIAL_RADIUS_METERS = 6_378_137.0
_NODE_ID_CHARACTER = re.compile(r"[^A-Za-z0-9_]+")
_DETECTION_SCORE_SEMANTICS = (
    "deterministic range/weather evidence index in [0,1]; not a calibrated "
    "probability of detection"
)

_BASE_QUALITY = {
    "wideband_rf": 0.90,
    "mmwave": 0.91,
    "rgb": 0.84,
    "event_camera": 0.88,
    "SEARCH_RADAR": 0.94,
    "EO_PTZ": 0.86,
    "THERMAL_PTZ": 0.88,
}
_BASE_LATENCY_MS = {
    "wideband_rf": 45.0,
    "mmwave": 60.0,
    "rgb": 120.0,
    "event_camera": 25.0,
    "SEARCH_RADAR": 80.0,
    "EO_PTZ": 220.0,
    "THERMAL_PTZ": 250.0,
}
# A short, explicit synthetic dwell makes the replay demonstrate acquisition
# instead of presenting every track as detected at its first timeline sample.
# These are display-model assumptions, not measured hardware timings.
_ACQUISITION_DELAY_SECONDS = {
    "wideband_rf": 1.0,
    "mmwave": 2.0,
    "rgb": 3.0,
    "event_camera": 1.0,
    "SEARCH_RADAR": 2.0,
    "EO_PTZ": 3.0,
    "THERMAL_PTZ": 3.0,
}
_DETECTION_THRESHOLD = 0.35


def _is_prohibited_modality(modality: object) -> bool:
    normalized = str(modality).strip().casefold()
    return normalized.startswith(tuple(PROHIBITED_DEPLOYED_MODALITIES))


def _json_object(path: str | Path, *, label: str) -> dict[str, Any]:
    value = json.loads(Path(path).read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise TypeError(f"{label} root must be an object")
    return value


def load_recommendation(path: str | Path) -> PlacementRecommendation:
    """Load and cryptographically self-validate a recommendation artifact."""

    value = _json_object(path, label="recommendation")
    return PlacementRecommendation(
        request_id=value["requestId"],
        status=value["status"],
        algorithm=value["algorithm"],
        selected_options=tuple(value.get("selectedOptions", ())),
        total_cost_units=value["totalCostUnits"],
        metrics=value["metrics"],
        infeasibility_reasons=tuple(value.get("infeasibilityReasons", ())),
        input_digest=value["inputDigest"],
        recommendation_digest=value["recommendationDigest"],
        schema_version=value.get("schemaVersion", ""),
    )


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
        raise ValueError(f"{name} must be greater than zero")
    return result


def _safe_node_id(site_id: str) -> str:
    suffix = _NODE_ID_CHARACTER.sub("_", site_id.strip()).strip("_")
    if not suffix:
        raise ValueError("site_id cannot be converted to a safe Unreal NodeId")
    return f"PLACEMENT_{suffix}"[:96]


def _geo_offset(
    latitude_degrees: float,
    longitude_degrees: float,
    east_meters: float,
    north_meters: float,
) -> tuple[float, float]:
    latitude_radians = math.radians(latitude_degrees)
    cosine = max(abs(math.cos(latitude_radians)), 0.000001)
    return (
        latitude_degrees
        + math.degrees(north_meters / _WGS84_EQUATORIAL_RADIUS_METERS),
        longitude_degrees
        + math.degrees(
            east_meters / (_WGS84_EQUATORIAL_RADIUS_METERS * cosine)
        ),
    )


def _enu_offset(
    origin_latitude_degrees: float,
    origin_longitude_degrees: float,
    latitude_degrees: float,
    longitude_degrees: float,
) -> tuple[float, float]:
    mean_latitude = math.radians(
        (origin_latitude_degrees + latitude_degrees) * 0.5
    )
    east = math.radians(longitude_degrees - origin_longitude_degrees) * (
        _WGS84_EQUATORIAL_RADIUS_METERS * max(abs(math.cos(mean_latitude)), 0.000001)
    )
    north = math.radians(latitude_degrees - origin_latitude_degrees) * (
        _WGS84_EQUATORIAL_RADIUS_METERS
    )
    return east, north


def _expanded_targets(config: Mapping[str, Any]) -> list[dict[str, Any]]:
    raw_targets = config.get("DemoTargets", ())
    if not isinstance(raw_targets, Sequence) or isinstance(raw_targets, (str, bytes)):
        raise TypeError("scenario DemoTargets must be an array")

    expanded: list[dict[str, Any]] = []
    for raw in raw_targets:
        if not isinstance(raw, Mapping):
            raise TypeError("scenario DemoTargets entries must be objects")
        if raw.get("bEnabled", True) is not True:
            continue
        requested_count = int(raw.get("SpawnCount", 1))
        spawn_count = max(1, min(requested_count, 64))
        columns = max(1, min(int(raw.get("FormationColumns", 4)), spawn_count))
        rows = math.ceil(spawn_count / columns)
        spacing = max(_finite("FormationSpacingMeters", raw.get("FormationSpacingMeters", 25.0)), 0.0)
        start_latitude = _finite("StartLatitudeDegrees", raw["StartLatitudeDegrees"])
        start_longitude = _finite("StartLongitudeDegrees", raw["StartLongitudeDegrees"])
        base_name = str(raw.get("ActorName") or "Drone1")

        for index in range(spawn_count):
            if len(expanded) >= 64:
                return expanded
            row = index // columns
            column = index % columns
            first_index = row * columns
            targets_in_row = min(columns, spawn_count - first_index)
            east = (column - (targets_in_row - 1) * 0.5) * spacing
            north = (row - (rows - 1) * 0.5) * spacing
            latitude, longitude = _geo_offset(
                start_latitude,
                start_longitude,
                east,
                north,
            )
            item = dict(raw)
            item["ActorName"] = (
                f"{base_name}_{index + 1:02d}" if spawn_count > 1 else base_name
            ).replace(" ", "_")
            item["StartLatitudeDegrees"] = latitude
            item["StartLongitudeDegrees"] = longitude
            item["SpawnCount"] = 1
            expanded.append(item)
    return expanded


def _trajectory_sample(
    target: Mapping[str, Any],
    simulation_seconds: float,
) -> dict[str, Any]:
    trajectory = str(target.get("Trajectory", "Stationary")).strip().lower()
    start_latitude = _finite("StartLatitudeDegrees", target["StartLatitudeDegrees"])
    start_longitude = _finite("StartLongitudeDegrees", target["StartLongitudeDegrees"])
    start_height = _finite("StartHeightMeters", target.get("StartHeightMeters", 0.0))
    east = north = up = speed = 0.0
    heading = 0.0

    if trajectory == "linear":
        raw_direction = target.get("LinearDirectionEnu", {})
        if not isinstance(raw_direction, Mapping):
            raise TypeError("LinearDirectionEnu must be an object")
        dx = _finite("LinearDirectionEnu.X", raw_direction.get("X", 0.0))
        dy = _finite("LinearDirectionEnu.Y", raw_direction.get("Y", 0.0))
        dz = _finite("LinearDirectionEnu.Z", raw_direction.get("Z", 0.0))
        length = math.sqrt(dx * dx + dy * dy + dz * dz)
        if length > 1e-12:
            dx, dy, dz = dx / length, dy / length, dz / length
        else:
            dx = dy = dz = 0.0
        path_length = max(
            _finite("LinearDistanceMeters", target.get("LinearDistanceMeters", 0.0)),
            0.0,
        )
        configured_speed = max(
            _finite(
                "LinearSpeedMetersPerSecond",
                target.get("LinearSpeedMetersPerSecond", 0.0),
            ),
            0.0,
        )
        travel = configured_speed * max(simulation_seconds, 0.0)
        path_position = 0.0
        direction_sign = 1.0
        if path_length > 1e-12:
            if target.get("bPingPongLinearPath", False) is True:
                cycle_position = math.fmod(travel, path_length * 2.0)
                if cycle_position <= path_length:
                    path_position = cycle_position
                else:
                    path_position = path_length * 2.0 - cycle_position
                    direction_sign = -1.0
            else:
                path_position = math.fmod(travel, path_length)
        east, north, up = (
            dx * path_position,
            dy * path_position,
            dz * path_position,
        )
        speed = configured_speed if path_length > 1e-12 else 0.0
        if speed > 0.0 and length > 1e-12:
            heading = (
                math.degrees(math.atan2(dx * direction_sign, dy * direction_sign))
                + 360.0
            ) % 360.0
    elif trajectory == "circular":
        radius = max(
            _finite("CircularRadiusMeters", target.get("CircularRadiusMeters", 0.0)),
            0.0,
        )
        angular_speed = _finite(
            "AngularSpeedDegreesPerSecond",
            target.get("AngularSpeedDegreesPerSecond", 0.0),
        )
        angle = math.radians(angular_speed * simulation_seconds)
        east = radius * math.cos(angle)
        north = radius * math.sin(angle)
        angular_radians_per_second = math.radians(angular_speed)
        velocity_east = -radius * angular_radians_per_second * math.sin(angle)
        velocity_north = radius * angular_radians_per_second * math.cos(angle)
        speed = math.hypot(velocity_east, velocity_north)
        if speed > 1e-12:
            heading = (
                math.degrees(math.atan2(velocity_east, velocity_north)) + 360.0
            ) % 360.0
    elif trajectory != "stationary":
        raise ValueError(f"unsupported demo trajectory: {target.get('Trajectory')!r}")

    latitude, longitude = _geo_offset(
        start_latitude,
        start_longitude,
        east,
        north,
    )
    return {
        "tSeconds": round(simulation_seconds, 6),
        "location": {
            "latitudeDegrees": round(latitude, 10),
            "longitudeDegrees": round(longitude, 10),
            "heightMeters": round(start_height + up, 6),
            "heightReference": "WGS84_ELLIPSOID",
        },
        "offsetEnuMeters": {
            "east": round(east, 6),
            "north": round(north, 6),
            "up": round(up, 6),
        },
        "headingDegrees": round(heading, 6),
        "speedMetersPerSecond": round(speed, 6),
    }


def _range_for_modality(modality: str, template: Mapping[str, Any]) -> float | None:
    if modality == "SEARCH_RADAR":
        if template.get("bEnableSearchRadar", False) is not True:
            return None
        return max(_finite("SearchRadarRangeMeters", template.get("SearchRadarRangeMeters", 5000.0)), 5000.0)
    if modality == "EO_PTZ":
        if template.get("bEnableEOPTZ", False) is not True:
            return None
        return max(_finite("EOPTZConfirmationRangeMeters", template.get("EOPTZConfirmationRangeMeters", 500.0)), 500.0)
    if modality == "THERMAL_PTZ":
        if template.get("bEnableThermalPTZ", False) is not True:
            return None
        return max(_finite("ThermalPTZConfirmationRangeMeters", template.get("ThermalPTZConfirmationRangeMeters", 500.0)), 500.0)
    if modality == "rgb" and template.get("bCaptureCameraFrames", False) is not True:
        return None
    if modality not in _BASE_QUALITY:
        raise ValueError(f"unsupported replay modality: {modality!r}")
    return _positive("DetectionRangeMeters", template.get("DetectionRangeMeters", 1000.0))


def _stage0_sensor_class(modality: str) -> str | None:
    """Map legacy modalities onto the frozen Stage-0 deployment scope.

    ``None`` means the modality is explicitly prohibited for deployment and
    must not appear as a replay sensor or detection.  EO PTZ is treated as an
    RGB camera presentation, while thermal PTZ remains thermal and is filtered.
    """

    normalized = modality.strip().casefold()
    if any(normalized.startswith(item) for item in PROHIBITED_DEPLOYED_MODALITIES):
        return None
    mapping = {
        "wideband_rf": "passive_rf",
        "mmwave": "radar",
        "search_radar": "radar",
        "rgb": "rgb",
        "eo_ptz": "rgb",
        "event_camera": "event_camera",
    }
    sensor_class = mapping.get(normalized)
    if sensor_class not in ALLOWED_SENSOR_CLASSES:
        raise ValueError(f"legacy modality has no Stage-0 sensor-class mapping: {modality!r}")
    return sensor_class


def _weather_factor(weather_profile: str, modality: str) -> float:
    weather = weather_profile.strip().casefold()
    family = family_for_modality(modality)
    table = {
        "clear": {"passive_rf": 1.0, "active_radar": 1.0, "visual": 1.0},
        "lightrain": {"passive_rf": 0.97, "active_radar": 0.96, "visual": 0.82},
        "monsoon": {"passive_rf": 0.90, "active_radar": 0.92, "visual": 0.55},
        "haze": {"passive_rf": 0.95, "active_radar": 0.97, "visual": 0.65},
    }
    return table.get(weather, {"passive_rf": 0.90, "active_radar": 0.90, "visual": 0.70})[family]


def _slant_range_meters(
    first: Mapping[str, Any],
    second: Mapping[str, Any],
) -> float:
    east, north = _enu_offset(
        _finite("first.latitudeDegrees", first["latitudeDegrees"]),
        _finite("first.longitudeDegrees", first["longitudeDegrees"]),
        _finite("second.latitudeDegrees", second["latitudeDegrees"]),
        _finite("second.longitudeDegrees", second["longitudeDegrees"]),
    )
    vertical = _finite("second.heightMeters", second.get("heightMeters", 0.0)) - _finite(
        "first.heightMeters", first.get("heightMeters", 0.0)
    )
    return math.sqrt(east * east + north * north + vertical * vertical)


def _modality_records(
    request: PlacementRequest,
    recommendation: PlacementRecommendation,
) -> tuple[list[dict[str, Any]], dict[str, dict[str, Any]], dict[str, Any]]:
    package_by_id = {item.package_id: item for item in request.sensor_packages}
    sensors: list[dict[str, Any]] = []
    sensor_index: dict[str, dict[str, Any]] = {}
    represented_classes: set[str] = set()
    filtered_modalities: set[str] = set()
    for selected in recommendation.selected_options:
        package_id = str(selected["packageId"])
        package = package_by_id.get(package_id)
        if package is None:
            raise ValueError(f"recommendation references unknown package {package_id!r}")
        site_id = str(selected["siteId"])
        node_id = _safe_node_id(site_id)
        modalities: list[dict[str, Any]] = []
        for modality in sorted(package.modalities):
            sensor_class = _stage0_sensor_class(modality)
            if sensor_class is None:
                filtered_modalities.add(modality)
                continue
            nominal_range = _range_for_modality(modality, package.unreal_node_template)
            if nominal_range is None:
                continue
            represented_classes.add(sensor_class)
            modalities.append(
                {
                    "sensorId": f"{node_id}:{modality}",
                    "modality": modality,
                    "sensorClass": sensor_class,
                    "family": family_for_modality(modality),
                    "nominalRangeMeters": round(nominal_range, 6),
                    "simulated": True,
                    "calibrated": False,
                }
            )
        location = dict(selected["location"])
        record = {
            "nodeId": node_id,
            "optionId": str(selected["optionId"]),
            "siteId": site_id,
            "packageId": package_id,
            "orientationId": str(selected.get("orientationId", "default")),
            "failureDomainId": str(selected["failureDomainId"]),
            "location": location,
            "modalities": modalities,
            "installedCostUnits": _finite(
                "installedCostUnits", selected["installedCostUnits"]
            ),
            "rationale": {
                "summary": (
                    f"Selected by {recommendation.algorithm} as part of the declared "
                    "simulation-feasible layout; global optimality and site approval are not inferred."
                ),
                "codes": [
                    "SELECTED_BY_DECLARED_SOLVER",
                    "MULTI_FAMILY_PACKAGE" if len(package.families) > 1 else "SINGLE_FAMILY_PACKAGE",
                    "DECLARED_FAILURE_DOMAIN",
                ],
                "families": list(package.families),
                "marginalCoverageMetricAvailable": False,
            },
        }
        sensors.append(record)
        sensor_index[node_id] = record
    missing_classes = sorted(set(ALLOWED_SENSOR_CLASSES) - represented_classes)
    filtered = sorted(filtered_modalities)
    warnings: list[str] = []
    if filtered:
        warnings.append(
            "Filtered prohibited deployed modality/modalities from the legacy "
            f"recommendation: {', '.join(filtered)}."
        )
    if missing_classes:
        warnings.append(
            "Legacy recommendation does not represent Stage-0 sensor class(es): "
            f"{', '.join(missing_classes)}."
        )
    if filtered or missing_classes:
        warnings.append(
            "This legacy v1 recommendation is not a complete Stage-0 four-class placement result."
        )
    scope = {
        "allowedSensorClasses": list(ALLOWED_SENSOR_CLASSES),
        "prohibitedDeployedModalities": list(PROHIBITED_DEPLOYED_MODALITIES),
        "representedSensorClasses": sorted(represented_classes),
        "missingSensorClasses": missing_classes,
        "filteredRecommendationModalities": filtered,
        "legacyRecommendationScopeComplete": not filtered and not missing_classes,
        "warnings": warnings,
    }
    return sensors, sensor_index, scope


def _score_detection(
    *,
    modality: str,
    nominal_range_meters: float,
    range_meters: float,
    weather_profile: str,
    rf_emitting: bool,
    emitter_enabled: bool,
    radar_cross_section_square_meters: float,
) -> tuple[bool, float, float, float]:
    if modality == "wideband_rf" and (not rf_emitting or not emitter_enabled):
        return False, 0.0, nominal_range_meters, 0.0
    weather = _weather_factor(weather_profile, modality)
    effective_range = nominal_range_meters * weather
    ratio = range_meters / max(effective_range, 1e-9)
    range_score = max(0.0, 1.0 - 0.62 * ratio**1.2)
    rcs_factor = 1.0
    if family_for_modality(modality) == "active_radar":
        rcs_factor = min(1.15, max(0.50, (radar_cross_section_square_meters / 0.03) ** 0.25))
    confidence = min(0.995, _BASE_QUALITY[modality] * weather * range_score * rcs_factor)
    detected = range_meters <= effective_range + 1e-9 and confidence >= _DETECTION_THRESHOLD
    latency = _BASE_LATENCY_MS[modality] + range_meters * 0.02
    return detected, confidence, effective_range, latency


def _time_values(duration_seconds: float, cadence_seconds: float) -> list[float]:
    steps = int(math.floor(duration_seconds / cadence_seconds + 1e-12))
    values = [round(index * cadence_seconds, 9) for index in range(steps + 1)]
    if not math.isclose(values[-1], duration_seconds, rel_tol=0.0, abs_tol=1e-9):
        values.append(duration_seconds)
    return values


def _validate_solver_proof(recommendation: Mapping[str, Any]) -> None:
    """Validate the optional, exact v2 proof exposed by the demo dashboard."""

    proof = recommendation.get("solverProof")
    optimality_claimed = (
        recommendation.get("optimalityProvenForPrecomputedBinaryModel") is True
    )
    if proof is None:
        # Backward compatibility: v1 replay artifacts predate the explicit
        # proof block. New demo-adapter output always supplies and validates it.
        return
    if not isinstance(proof, Mapping):
        raise TypeError("recommendation.solverProof must be an object")
    if not optimality_claimed:
        raise ValueError("solverProof requires an explicit precomputed-model proof claim")
    if recommendation.get("status") != "OPTIMAL":
        raise ValueError("solverProof requires an OPTIMAL recommendation")
    if recommendation.get("algorithm") != BRANCH_AND_BOUND_ALGORITHM:
        raise ValueError("solverProof requires the robust branch-and-bound algorithm")
    if proof.get("schemaVersion") != STATIC_SOLVER_RESULT_SCHEMA:
        raise ValueError("solverProof must use the robust solver-result v2 schema")
    if proof.get("terminationReason") != "SEARCH_EXHAUSTED_OPTIMAL":
        raise ValueError("solverProof termination reason is not an optimality proof")
    if proof.get("nodeLimitReached") is not False:
        raise ValueError("solverProof optimal result must not report a reached node limit")
    if proof.get("boundSemantics") != "MATCHED_INCUMBENT_BY_EXHAUSTIVE_SEARCH":
        raise ValueError("solverProof has unexpected bound semantics")

    explored = proof.get("exploredNodeCount")
    node_limit = proof.get("nodeLimit")
    if isinstance(explored, bool) or not isinstance(explored, int) or explored < 0:
        raise ValueError("solverProof exploredNodeCount must be a non-negative integer")
    if (
        isinstance(node_limit, bool)
        or not isinstance(node_limit, int)
        or node_limit <= 0
    ):
        raise ValueError("solverProof nodeLimit must be a positive integer")
    if explored > node_limit:
        raise ValueError("solverProof exploredNodeCount exceeds nodeLimit")

    incumbent = _finite(
        "solverProof.incumbentObjectiveCostUnits",
        proof.get("incumbentObjectiveCostUnits"),
    )
    lower_bound = _finite(
        "solverProof.objectiveCostLowerBoundUnits",
        proof.get("objectiveCostLowerBoundUnits"),
    )
    absolute_gap = _finite(
        "solverProof.absoluteCostOptimalityGapUnits",
        proof.get("absoluteCostOptimalityGapUnits"),
    )
    relative_gap = _finite(
        "solverProof.relativeCostOptimalityGap",
        proof.get("relativeCostOptimalityGap"),
    )
    total_cost = _finite(
        "recommendation.totalCostUnits", recommendation.get("totalCostUnits")
    )
    if min(incumbent, lower_bound, absolute_gap, relative_gap, total_cost) < 0.0:
        raise ValueError("solverProof costs and gaps must be non-negative")
    if not math.isclose(incumbent, total_cost, rel_tol=0.0, abs_tol=1e-9):
        raise ValueError("solverProof incumbent contradicts recommendation total cost")
    if not math.isclose(lower_bound, incumbent, rel_tol=0.0, abs_tol=1e-9):
        raise ValueError("solverProof lower bound does not close on the incumbent")
    if absolute_gap != 0.0 or relative_gap != 0.0:
        raise ValueError("solverProof optimal result must report zero cost gaps")


def validate_dashboard_replay(value: Mapping[str, Any]) -> None:
    """Fail closed on the safety and deterministic-integrity fields."""

    if value.get("schemaVersion") != DASHBOARD_REPLAY_SCHEMA:
        raise ValueError("unsupported dashboard replay schema")
    for key, expected in {
        "simulationOnly": True,
        "detectionOnly": True,
        "operationalUseAuthorized": False,
        "siteAuthorizationInferred": False,
        "calibratedProbabilities": False,
    }.items():
        if value.get(key) is not expected:
            raise ValueError(f"dashboard replay safety field {key} must be {expected}")
    digest = value.get("replayDigest")
    if not isinstance(digest, str) or len(digest) != 64:
        raise ValueError("replayDigest must be a SHA-256 hex digest")
    payload = dict(value)
    payload.pop("replayDigest", None)
    if stable_digest(payload) != digest:
        raise ValueError("replayDigest does not match replay content")
    if value.get("actionsTaken") != "none":
        raise ValueError("dashboard replay must not claim or take an action")
    recommendation = value.get("recommendation")
    if not isinstance(recommendation, Mapping):
        raise TypeError("recommendation must be an object")
    _validate_solver_proof(recommendation)
    sensors = value.get("sensors")
    if not isinstance(sensors, Sequence) or isinstance(sensors, (str, bytes)):
        raise TypeError("sensors must be an array")
    represented_sensor_classes: set[str] = set()
    sensor_ids: set[str] = set()
    for sensor in sensors:
        if not isinstance(sensor, Mapping):
            raise TypeError("sensor entries must be objects")
        modalities = sensor.get("modalities")
        if not isinstance(modalities, Sequence) or isinstance(
            modalities, (str, bytes)
        ):
            raise TypeError("sensor.modalities must be an array")
        for modality in modalities:
            if not isinstance(modality, Mapping):
                raise TypeError("sensor modality entries must be objects")
            modality_name = modality.get("modality")
            if _is_prohibited_modality(modality_name):
                raise ValueError("sensor contains a prohibited deployed modality")
            sensor_class = str(modality.get("sensorClass", ""))
            if sensor_class not in ALLOWED_SENSOR_CLASSES:
                raise ValueError("sensor modality class is outside the frozen Stage-0 scope")
            if modality.get("simulated") is not True or modality.get("calibrated") is not False:
                raise ValueError("dashboard sensor modality must remain simulated and uncalibrated")
            sensor_id = str(modality.get("sensorId", ""))
            if not sensor_id or sensor_id in sensor_ids:
                raise ValueError("dashboard sensor IDs must be non-empty and unique")
            sensor_ids.add(sensor_id)
            represented_sensor_classes.add(sensor_class)
    timeline = value.get("timeline")
    if not isinstance(timeline, Mapping):
        raise TypeError("timeline must be an object")
    detections = timeline.get("detections")
    if not isinstance(detections, Sequence) or isinstance(detections, (str, bytes)):
        raise TypeError("timeline.detections must be an array")
    for detection in detections:
        if not isinstance(detection, Mapping):
            raise TypeError("detection entries must be objects")
        confidence = _finite("detection.confidence", detection.get("confidence"))
        if not 0.0 <= confidence <= 1.0:
            raise ValueError("detection confidence must be in [0,1]")
        if detection.get("confidenceSemantics") != _DETECTION_SCORE_SEMANTICS:
            raise ValueError("detection confidence semantics are missing or incorrect")
        if _is_prohibited_modality(detection.get("modality")):
            raise ValueError("detection contains a prohibited deployed modality")
        sensor_class = detection.get("sensorClass")
        if sensor_class not in ALLOWED_SENSOR_CLASSES:
            raise ValueError("detection sensorClass is outside the frozen Stage-0 scope")
        if detection.get("sensorId") not in sensor_ids:
            raise ValueError("detection references an unknown dashboard sensor")
    scope = value.get("stage0Scope")
    if not isinstance(scope, Mapping):
        raise TypeError("stage0Scope must be an object")
    if set(scope.get("representedSensorClasses", ())) - set(ALLOWED_SENSOR_CLASSES):
        raise ValueError("stage0Scope contains an unsupported sensor class")
    if set(scope.get("representedSensorClasses", ())) != represented_sensor_classes:
        raise ValueError("stage0Scope represented classes do not match dashboard sensors")
    if any(
        str(item).casefold().startswith(tuple(PROHIBITED_DEPLOYED_MODALITIES))
        for item in scope.get("representedSensorClasses", ())
    ):
        raise ValueError("stage0Scope represents a prohibited deployed modality")
    if recommendation.get("solverProof") is not None:
        for key, expected in {
            "executionClass": "DEMO_STUDY_ONLY",
            "productionOptimizationAuthorized": False,
            "physicalDeploymentAuthorized": False,
        }.items():
            if scope.get(key) != expected:
                raise ValueError(
                    f"solver-proof replay stage0Scope.{key} must remain {expected!r}"
                )
        blockers = scope.get("blockers")
        if (
            isinstance(blockers, (str, bytes))
            or not isinstance(blockers, Sequence)
            or not blockers
        ):
            raise ValueError("solver-proof replay must retain Stage-0 blockers")


def build_dashboard_replay(
    request: PlacementRequest,
    recommendation: PlacementRecommendation,
    scenario_config: Mapping[str, Any],
    *,
    scenario_id: str | None = None,
    map_package: str = DEFAULT_MAP_PACKAGE,
    duration_seconds: float = 150.0,
    cadence_seconds: float = 1.0,
) -> dict[str, Any]:
    """Build one deterministic dashboard-ready replay artifact."""

    duration = _positive("duration_seconds", duration_seconds)
    cadence = _positive("cadence_seconds", cadence_seconds)
    if cadence > duration:
        raise ValueError("cadence_seconds must not exceed duration_seconds")
    if recommendation.request_id != request.request_id:
        raise ValueError("request and recommendation IDs do not match")
    if recommendation.status != "FEASIBLE":
        raise ValueError("dashboard replay requires a FEASIBLE recommendation")
    if not recommendation.selected_options:
        raise ValueError("dashboard replay requires at least one selected sensor option")
    if not isinstance(map_package, str) or not map_package.startswith("/Game/Maps/"):
        raise ValueError("map_package must be an Unreal /Game/Maps package")

    scenario_by_id = {item.scenario_id: item for item in request.scenarios}
    selected_scenario_id = scenario_id or request.scenarios[0].scenario_id
    scenario = scenario_by_id.get(selected_scenario_id)
    if scenario is None:
        raise ValueError(f"unknown placement scenario {selected_scenario_id!r}")
    targets = _expanded_targets(scenario_config)
    if not targets:
        raise ValueError("scenario config contains no enabled demo targets")
    sensors, sensor_index, stage0_scope = _modality_records(request, recommendation)
    time_values = _time_values(duration, cadence)

    tracks: list[dict[str, Any]] = []
    detections: list[dict[str, Any]] = []
    detection_families_by_track_time: dict[tuple[str, float], set[str]] = {}
    detection_nodes_by_track_time: dict[tuple[str, float], set[str]] = {}
    detection_domains_by_track_time: dict[tuple[str, float], set[str]] = {}

    for target_index, target in enumerate(targets, start=1):
        track_id = f"SIM-TRACK-{target_index:03d}"
        emitter = target.get("RFEmitter", {})
        emitter = emitter if isinstance(emitter, Mapping) else {}
        emitter_enabled = emitter.get("bEnabled", True) is True
        samples: list[dict[str, Any]] = []
        for sample_index, simulation_seconds in enumerate(time_values):
            sample = _trajectory_sample(target, simulation_seconds)
            location = sample["location"]
            horizontal_east, horizontal_north = _enu_offset(
                request.aoi.center.latitude_degrees,
                request.aoi.center.longitude_degrees,
                float(location["latitudeDegrees"]),
                float(location["longitudeDegrees"]),
            )
            sample["distanceToAoiCenterMeters"] = round(
                math.hypot(horizontal_east, horizontal_north), 6
            )
            sample["insideAoi"] = (
                sample["distanceToAoiCenterMeters"] <= request.aoi.radius_meters + 1e-9
            )
            samples.append(sample)

            for sensor in sensors:
                node_id = str(sensor["nodeId"])
                sensor_location = sensor["location"]
                slant_range = _slant_range_meters(sensor_location, location)
                for modality in sensor["modalities"]:
                    modality_name = str(modality["modality"])
                    if (
                        simulation_seconds + 1e-9
                        < _ACQUISITION_DELAY_SECONDS[modality_name]
                    ):
                        continue
                    detected, confidence, effective_range, latency = _score_detection(
                        modality=modality_name,
                        nominal_range_meters=float(modality["nominalRangeMeters"]),
                        range_meters=slant_range,
                        weather_profile=scenario.weather_profile,
                        rf_emitting=scenario.rf_emitting,
                        emitter_enabled=emitter_enabled,
                        radar_cross_section_square_meters=scenario.radar_cross_section_square_meters,
                    )
                    if not detected:
                        continue
                    event = {
                        "eventId": (
                            f"det:{track_id}:{node_id}:{modality_name}:{sample_index:05d}"
                        ),
                        "tSeconds": round(simulation_seconds, 6),
                        "trackId": track_id,
                        "nodeId": node_id,
                        "sensorId": str(modality["sensorId"]),
                        "modality": modality_name,
                        "sensorClass": str(modality["sensorClass"]),
                        "family": str(modality["family"]),
                        "rangeMeters": round(slant_range, 3),
                        "effectiveRangeMeters": round(effective_range, 3),
                        "confidence": round(confidence, 6),
                        "confidenceSemantics": _DETECTION_SCORE_SEMANTICS,
                        "latencyMilliseconds": round(latency, 3),
                        "source": "SYNTHETIC_ANALYTIC_REPLAY",
                        "detected": True,
                        "simulationOnly": True,
                    }
                    detections.append(event)
                    key = (track_id, round(simulation_seconds, 6))
                    detection_families_by_track_time.setdefault(key, set()).add(
                        str(modality["family"])
                    )
                    detection_nodes_by_track_time.setdefault(key, set()).add(node_id)
                    detection_domains_by_track_time.setdefault(key, set()).add(
                        str(sensor_index[node_id]["failureDomainId"])
                    )

        track_events = [item for item in detections if item["trackId"] == track_id]
        first_detection = min(
            (float(item["tSeconds"]) for item in track_events),
            default=None,
        )
        first_corroborated: float | None = None
        for simulation_seconds in time_values:
            key = (track_id, round(simulation_seconds, 6))
            if (
                len(detection_families_by_track_time.get(key, set()))
                >= request.constraints.minimum_family_count
                and len(detection_nodes_by_track_time.get(key, set()))
                >= request.constraints.minimum_site_redundancy
                and len(detection_domains_by_track_time.get(key, set()))
                >= request.constraints.minimum_failure_domain_redundancy
            ):
                first_corroborated = round(simulation_seconds, 6)
                break
        tracks.append(
            {
                "trackId": track_id,
                "displayName": str(target.get("ActorName") or track_id),
                "scenarioKind": "SIMULATED_UAS_INGRESS",
                "ingressCorridorId": str(target.get("IngressCorridorId") or "UNSPECIFIED"),
                "inboundApproachScenario": target.get("bInboundApproachScenario", False) is True,
                "trajectory": str(target.get("Trajectory", "Stationary")),
                "samples": samples,
                "outcome": {
                    "status": "DETECTED_IN_SIMULATION" if first_detection is not None else "NOT_DETECTED_IN_SIMULATION",
                    "firstDetectionSeconds": first_detection,
                    "firstCorroboratedDetectionSeconds": first_corroborated,
                    "detectionEventCount": len(track_events),
                },
            }
        )

    corroborated_count = sum(
        item["outcome"]["firstCorroboratedDetectionSeconds"] is not None
        for item in tracks
    )
    recommendation_metrics = recommendation.to_dict()["metrics"]
    payload: dict[str, Any] = {
        "schemaVersion": DASHBOARD_REPLAY_SCHEMA,
        "replayId": (
            f"{request.request_id}:{selected_scenario_id}:"
            f"{recommendation.recommendation_digest[:12]}"
        ),
        "simulationOnly": True,
        "detectionOnly": True,
        "operationalUseAuthorized": False,
        "siteAuthorizationInferred": False,
        "calibratedProbabilities": False,
        "actionsTaken": "none",
        "map": {
            "unrealPackage": map_package,
            "aoi": request.aoi.to_dict(),
        },
        "scenario": {
            "scenarioId": scenario.scenario_id,
            "weatherProfile": scenario.weather_profile,
            "rfEmitting": scenario.rf_emitting,
            "radarCrossSectionSquareMeters": scenario.radar_cross_section_square_meters,
            "weight": scenario.weight,
            "runtimeConfigEnabledInSource": scenario_config.get("bEnabled") is True,
            "stage0Authorization": "NOT_INFERRED",
        },
        "stage0Scope": stage0_scope,
        "recommendation": {
            "requestId": request.request_id,
            "status": recommendation.status,
            "algorithm": recommendation.algorithm,
            "sourceRecommendationDigest": recommendation.recommendation_digest,
            "totalCostUnits": recommendation.total_cost_units,
            "selectedSiteCount": len(sensors),
            "metrics": recommendation_metrics,
            "rationale": (
                "The displayed layout is the exact selected set in the source "
                "recommendation. Network metrics are retained verbatim; per-site "
                "marginal coverage was not present in the v1 recommendation. "
                "Prohibited legacy modalities are filtered and missing Stage-0 "
                "sensor classes remain explicit."
            ),
        },
        "sensors": sensors,
        "timeline": {
            "timeBasis": "RELATIVE_SIMULATION_SECONDS",
            "durationSeconds": round(duration, 6),
            "cadenceSeconds": round(cadence, 6),
            "tracks": tracks,
            "detections": detections,
            "successSummary": {
                "simulatedTrackCount": len(tracks),
                "detectedTrackCount": sum(
                    item["outcome"]["firstDetectionSeconds"] is not None
                    for item in tracks
                ),
                "corroboratedTrackCount": corroborated_count,
                "allTracksDetected": all(
                    item["outcome"]["firstDetectionSeconds"] is not None
                    for item in tracks
                ),
            },
        },
        "detectionPolicy": {
            "minimumIndependentFamilyCount": request.constraints.minimum_family_count,
            "minimumContributingSiteCount": request.constraints.minimum_site_redundancy,
            "minimumFailureDomainCount": request.constraints.minimum_failure_domain_redundancy,
            "confidenceThreshold": _DETECTION_THRESHOLD,
            "syntheticAcquisitionDelaySecondsByModality": dict(
                _ACQUISITION_DELAY_SECONDS
            ),
        },
        "modelCard": {
            "modelId": DASHBOARD_REPLAY_GENERATOR,
            "deterministic": True,
            "confidenceSemantics": _DETECTION_SCORE_SEMANTICS,
            "calibrated": False,
            "limitations": [
                "Offline range-gate demonstration; it is not live Unreal telemetry.",
                "No terrain/foliage occlusion, multipath, antenna pattern, camera FOV, or slew dynamics are evaluated.",
                "Latency values are deterministic display fixtures, not measured hardware latency.",
                "Synthetic per-modality acquisition delays are display fixtures, not measured dwell or track-initiation timings.",
                "The legacy v1 placement package has no event-camera modality and its prohibited thermal PTZ modality is filtered from this replay.",
                "Use triad.layered_detection_snapshot.v1 for the live audited sensor path.",
            ],
        },
        "provenance": {
            "generator": DASHBOARD_REPLAY_GENERATOR,
            "placementRequestDigest": stable_digest(request.to_dict()),
            "recommendationDigest": recommendation.recommendation_digest,
            "scenarioConfigDigest": stable_digest(dict(scenario_config)),
        },
    }
    payload["replayDigest"] = stable_digest(payload)
    validate_dashboard_replay(payload)
    return payload


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Export a deterministic simulation-only placement/drone/detection replay "
            "for an operator dashboard."
        )
    )
    parser.add_argument("request", type=Path, help="triad.placement_request.v1 JSON")
    parser.add_argument(
        "recommendation", type=Path, help="triad.placement_recommendation.v1 JSON"
    )
    parser.add_argument("scenario_config", type=Path, help="Unreal scenario JSON")
    parser.add_argument("--output", type=Path, required=True, help="new replay JSON path")
    parser.add_argument("--scenario-id", help="request scenario partition; defaults to first")
    parser.add_argument("--map-package", default=DEFAULT_MAP_PACKAGE)
    parser.add_argument("--duration-seconds", type=float, default=150.0)
    parser.add_argument("--cadence-seconds", type=float, default=1.0)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    request = PlacementRequest.load(args.request)
    recommendation = load_recommendation(args.recommendation)
    scenario_config = _json_object(args.scenario_config, label="scenario config")
    replay = build_dashboard_replay(
        request,
        recommendation,
        scenario_config,
        scenario_id=args.scenario_id,
        map_package=args.map_package,
        duration_seconds=args.duration_seconds,
        cadence_seconds=args.cadence_seconds,
    )
    write_new_json_file(replay, args.output)
    print(
        json.dumps(
            {
                "schemaVersion": DASHBOARD_REPLAY_SCHEMA,
                "output": str(args.output),
                "replayDigest": replay["replayDigest"],
                "selectedSiteCount": len(replay["sensors"]),
                "simulatedTrackCount": replay["timeline"]["successSummary"]["simulatedTrackCount"],
                "detectedTrackCount": replay["timeline"]["successSummary"]["detectedTrackCount"],
                "simulationOnly": True,
            },
            separators=(",", ":"),
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
