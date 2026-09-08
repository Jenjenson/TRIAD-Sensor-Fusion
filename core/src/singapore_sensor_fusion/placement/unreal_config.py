"""Review-only Unreal SensorNodes patch generation.

This module never reads, merges, or overwrites the host project's live
``SingaporeSensorFusion.json``.  It produces a separate, allow-listed patch
that a human can review or use with an isolated scenario-config override.
"""

from __future__ import annotations

from copy import deepcopy
import json
import math
import os
from pathlib import Path
import re
import tempfile
from typing import Any, Mapping

from .contracts import (
    CandidateOption,
    PlacementRecommendation,
    PlacementRequest,
    PlacementSurvey,
    UNREAL_PATCH_SCHEMA,
    canonical_json,
    stable_digest,
)
from .coverage import evaluate_selection


ALLOWED_SENSOR_NODE_FIELDS = frozenset(
    {
        "NodeId",
        "LongitudeDegrees",
        "LatitudeDegrees",
        "HeightMeters",
        "DetectionRangeMeters",
        "SupportedFrequenciesGHz",
        "ReceiveAntennaGainDbi",
        "ReceiverSensitivityDbm",
        "BandwidthMHz",
        "NoiseFigureDb",
        "SystemLossDb",
        "bEnabled",
        "MarkerScale",
        "bVisualizeNode",
        "VisualBeaconHeightMeters",
        "VisualLabelWorldSizeMeters",
        "VisualRangeRingSegments",
        "bCaptureCameraFrames",
        "bTrackNearestTarget",
        "CameraFieldOfViewDegrees",
        "CameraCaptureWidth",
        "CameraCaptureHeight",
        "CameraCaptureCadenceSeconds",
        "CameraRelativeRotation",
        "DepthNormalizationMaxMeters",
        "bEnableSearchRadar",
        "SearchRadarRangeMeters",
        "SearchRadarElevationFieldOfRegardDegrees",
        "SearchRadarDetectionThreshold",
        "SearchRadarRangeNoiseSigmaMeters",
        "SearchRadarBearingNoiseSigmaDegrees",
        "SearchRadarElevationNoiseSigmaDegrees",
        "SearchRadarRadialVelocityNoiseSigmaMetersPerSecond",
        "bEnableEOPTZ",
        "EOPTZConfirmationRangeMeters",
        "EOPTZFieldOfViewDegrees",
        "bEnableThermalPTZ",
        "ThermalPTZConfirmationRangeMeters",
        "ThermalPTZFieldOfViewDegrees",
        "PTZCaptureWidth",
        "PTZCaptureHeight",
        "PTZSlewRateDegreesPerSecond",
        "PTZSettleSeconds",
        "PTZCaptureCadenceSeconds",
        "PTZLineOfSightTraceChannel",
        "bPTZTraceComplex",
        "bVisualizeLongRangeSensorCue",
        "LongRangeVisualizationSeconds",
    }
)
_NODE_ID_CHARACTER = re.compile(r"[^A-Za-z0-9_]+")
_BOOLEAN_FIELDS = frozenset(
    {
        "bEnabled",
        "bVisualizeNode",
        "bCaptureCameraFrames",
        "bTrackNearestTarget",
        "bEnableSearchRadar",
        "bEnableEOPTZ",
        "bEnableThermalPTZ",
        "bPTZTraceComplex",
        "bVisualizeLongRangeSensorCue",
    }
)
_NUMBER_BOUNDS: dict[str, tuple[float, float | None]] = {
    # Match FTRIADGeodeticSensorNode metadata and the tighter runtime clamps.
    "DetectionRangeMeters": (0.0, None),
    "BandwidthMHz": (0.001, None),
    "MarkerScale": (0.1, None),
    "VisualBeaconHeightMeters": (10.0, None),
    "VisualLabelWorldSizeMeters": (1.0, None),
    "CameraFieldOfViewDegrees": (5.0, 170.0),
    "CameraCaptureCadenceSeconds": (0.1, None),
    "DepthNormalizationMaxMeters": (1.0, None),
    "SearchRadarRangeMeters": (5000.0, None),
    "SearchRadarElevationFieldOfRegardDegrees": (1.0, 180.0),
    "SearchRadarDetectionThreshold": (0.0, 1.0),
    "SearchRadarRangeNoiseSigmaMeters": (0.0, None),
    "SearchRadarBearingNoiseSigmaDegrees": (0.0, None),
    "SearchRadarElevationNoiseSigmaDegrees": (0.0, None),
    "SearchRadarRadialVelocityNoiseSigmaMetersPerSecond": (0.0, None),
    "EOPTZConfirmationRangeMeters": (500.0, None),
    "EOPTZFieldOfViewDegrees": (1.0, 45.0),
    "ThermalPTZConfirmationRangeMeters": (500.0, None),
    "ThermalPTZFieldOfViewDegrees": (1.0, 45.0),
    "PTZSlewRateDegreesPerSecond": (1.0, None),
    "PTZSettleSeconds": (0.0, None),
    "PTZCaptureCadenceSeconds": (0.1, None),
    "LongRangeVisualizationSeconds": (0.05, None),
}
_FINITE_NUMBER_FIELDS = frozenset(
    {
        "ReceiveAntennaGainDbi",
        "ReceiverSensitivityDbm",
        "NoiseFigureDb",
        "SystemLossDb",
        "LongitudeDegrees",
        "LatitudeDegrees",
        "HeightMeters",
    }
)
_INTEGER_BOUNDS: dict[str, tuple[int, int | None]] = {
    "VisualRangeRingSegments": (24, 256),
    "CameraCaptureWidth": (16, 4096),
    "CameraCaptureHeight": (16, 4096),
    "PTZCaptureWidth": (64, 4096),
    "PTZCaptureHeight": (64, 4096),
}
_NONNEGATIVE_INTEGER_FIELDS = frozenset({"PTZLineOfSightTraceChannel"})


def _finite_number(value: object, *, field: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise TypeError(f"{field} must be numeric")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"{field} must be finite")
    return result


def _validate_node_field(field: str, value: object) -> None:
    if field in _BOOLEAN_FIELDS:
        if not isinstance(value, bool):
            raise TypeError(f"{field} must be boolean")
        return
    if field == "NodeId":
        if not isinstance(value, str) or not value.strip():
            raise ValueError("NodeId must be a non-empty string")
        return
    if field == "SupportedFrequenciesGHz":
        if isinstance(value, (str, bytes)) or not isinstance(value, (list, tuple)) or not value:
            raise TypeError("SupportedFrequenciesGHz must be a non-empty numeric array")
        if any(_finite_number(item, field=field) <= 0.0 for item in value):
            raise ValueError("SupportedFrequenciesGHz entries must be > 0")
        return
    if field == "CameraRelativeRotation":
        if not isinstance(value, Mapping) or set(value) != {"Pitch", "Yaw", "Roll"}:
            raise TypeError("CameraRelativeRotation must contain Pitch, Yaw, and Roll")
        for component in ("Pitch", "Yaw", "Roll"):
            _finite_number(value[component], field=f"CameraRelativeRotation.{component}")
        return
    if field in _INTEGER_BOUNDS:
        minimum, maximum = _INTEGER_BOUNDS[field]
        if isinstance(value, bool) or not isinstance(value, int):
            raise TypeError(f"{field} must be an integer")
        if value < minimum or (maximum is not None and value > maximum):
            upper = f", {maximum}" if maximum is not None else ""
            raise ValueError(f"{field} must be in [{minimum}{upper}]")
        return
    if field in _NONNEGATIVE_INTEGER_FIELDS:
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            raise ValueError(f"{field} must be a non-negative integer")
        return
    if field in _NUMBER_BOUNDS:
        result = _finite_number(value, field=field)
        minimum, maximum = _NUMBER_BOUNDS[field]
        if result < minimum or (maximum is not None and result > maximum):
            upper = f", {maximum:g}" if maximum is not None else ""
            raise ValueError(f"{field} must be in [{minimum:g}{upper}]")
        return
    if field in _FINITE_NUMBER_FIELDS:
        result = _finite_number(value, field=field)
        if field == "LatitudeDegrees" and not -90.0 <= result <= 90.0:
            raise ValueError("LatitudeDegrees must be in [-90, 90]")
        if field == "LongitudeDegrees" and not -180.0 <= result <= 180.0:
            raise ValueError("LongitudeDegrees must be in [-180, 180]")
        return
    raise AssertionError(f"missing validation schema for allow-listed field {field!r}")


def _safe_node_id(site_id: str) -> str:
    suffix = _NODE_ID_CHARACTER.sub("_", site_id.strip()).strip("_")
    if not suffix:
        raise ValueError("site_id cannot be converted to a safe Unreal NodeId")
    return f"PLACEMENT_{suffix}"[:96]


def _allowlisted_node_fields(value: Mapping[str, Any], *, source: str) -> dict[str, Any]:
    unknown = sorted(set(value) - ALLOWED_SENSOR_NODE_FIELDS)
    if unknown:
        raise ValueError(f"{source} contains unsupported SensorNode fields: {unknown}")
    for field, item in value.items():
        try:
            _validate_node_field(field, item)
        except (TypeError, ValueError) as error:
            raise type(error)(f"{source}: {error}") from error
    return deepcopy(dict(value))


def _node_for_option(
    request: PlacementRequest,
    option: CandidateOption,
) -> dict[str, Any]:
    packages = {item.package_id: item for item in request.sensor_packages}
    package = packages.get(option.package_id)
    if package is None:
        raise ValueError(f"unknown sensor package {option.package_id!r}")
    if option.location.height_reference != "WGS84_ELLIPSOID":
        raise ValueError("Unreal SensorNodes require WGS84_ELLIPSOID heights")
    result = _allowlisted_node_fields(
        package.unreal_node_template,
        source=f"package {package.package_id!r}",
    )
    result.update(
        _allowlisted_node_fields(
            option.unreal_node_overrides,
            source=f"option {option.option_id!r}",
        )
    )
    # Surveyed identity and coordinates are authoritative and cannot be
    # replaced by a package template or caller-provided override.
    result.update(
        {
            "NodeId": _safe_node_id(option.site_id),
            "LongitudeDegrees": option.location.longitude_degrees,
            "LatitudeDegrees": option.location.latitude_degrees,
            "HeightMeters": option.location.height_meters,
            "bEnabled": True,
            "CameraRelativeRotation": {
                "Pitch": option.camera_pitch_degrees,
                "Yaw": option.camera_yaw_degrees,
                "Roll": option.camera_roll_degrees,
            },
        }
    )
    return result


def build_unreal_sensor_nodes_patch(
    request: PlacementRequest,
    survey: PlacementSurvey,
    recommendation: PlacementRecommendation,
) -> dict[str, object]:
    if recommendation.status != "FEASIBLE":
        raise ValueError("an Unreal config patch cannot be built from a non-feasible recommendation")
    if request.request_id != survey.request_id or request.request_id != recommendation.request_id:
        raise ValueError("request, survey, and recommendation IDs do not match")
    expected_input_digest = stable_digest(
        {"request": request.to_dict(), "survey": survey.to_dict()}
    )
    if recommendation.input_digest != expected_input_digest:
        raise ValueError(
            "recommendation input digest does not match the supplied request and survey"
        )
    if recommendation.recommendation_digest != recommendation.computed_recommendation_digest:
        raise ValueError("recommendation digest does not match recommendation content")
    option_by_id = {item.option_id: item for item in survey.options}
    if len(set(recommendation.selected_option_ids)) != len(recommendation.selected_option_ids):
        raise ValueError("recommendation contains duplicate option IDs")
    selected: list[CandidateOption] = []
    for option_id in recommendation.selected_option_ids:
        option = option_by_id.get(option_id)
        if option is None:
            raise ValueError(f"recommendation references unknown option {option_id!r}")
        if not option.feasible:
            raise ValueError(f"recommendation references infeasible option {option_id!r}")
        selected.append(option)
    if len({item.site_id for item in selected}) != len(selected):
        raise ValueError("recommendation selects more than one option at a physical site")
    if len(selected) > request.constraints.maximum_sites:
        raise ValueError("recommendation exceeds the maximum-site constraint")
    package_by_id = {item.package_id: item for item in request.sensor_packages}
    total_cost = round(
        sum(
            item.site_cost_units + package_by_id[item.package_id].cost_units
            for item in selected
        ),
        9,
    )
    if not math.isclose(
        total_cost,
        recommendation.total_cost_units,
        rel_tol=0.0,
        abs_tol=1e-9,
    ):
        raise ValueError("recommendation total cost does not match selected options")
    if (
        request.constraints.budget_units is not None
        and total_cost > request.constraints.budget_units + 1e-9
    ):
        raise ValueError("recommendation exceeds the budget constraint")
    recomputed_metrics = evaluate_selection(
        request, survey, recommendation.selected_option_ids
    )
    if not recomputed_metrics["coverageFeasible"]:
        raise ValueError("recommendation selection does not satisfy the coverage policy")
    if canonical_json(recomputed_metrics) != canonical_json(
        recommendation.to_dict()["metrics"]
    ):
        raise ValueError("recommendation metrics do not match the selected options")
    expected_rows = [
        {
            "optionId": option.option_id,
            "siteId": option.site_id,
            "packageId": option.package_id,
            "orientationId": option.orientation_id,
            "failureDomainId": option.failure_domain_id,
            "installedCostUnits": round(
                option.site_cost_units + package_by_id[option.package_id].cost_units,
                9,
            ),
            "location": option.location.to_dict(),
        }
        for option in selected
    ]
    if canonical_json(expected_rows) != canonical_json(
        recommendation.to_dict()["selectedOptions"]
    ):
        raise ValueError("recommendation option details do not match the survey")
    nodes = [_node_for_option(request, item) for item in selected]
    node_ids = [str(item["NodeId"]) for item in nodes]
    if len(set(node_ids)) != len(node_ids):
        raise ValueError("selected sites produce duplicate Unreal NodeIds")
    return {
        "schemaVersion": UNREAL_PATCH_SCHEMA,
        "requestId": request.request_id,
        "sourceRecommendationDigest": recommendation.recommendation_digest,
        "SensorNodes": nodes,
        "applySemantics": (
            "review_required_separate_patch; never merged into or written over the live "
            "SingaporeSensorFusion.json by this package"
        ),
        "simulationOnly": True,
        "siteAuthorizationInferred": False,
    }


def write_new_json_file(value: Mapping[str, Any], path: str | Path) -> Path:
    """Atomically create a new JSON artifact and refuse every overwrite."""

    destination = Path(path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists():
        raise FileExistsError(f"refusing to overwrite existing artifact: {destination}")
    payload = json.dumps(value, indent=2, ensure_ascii=False, allow_nan=False) + "\n"
    file_descriptor, temporary_name = tempfile.mkstemp(
        prefix=f".{destination.name}.", suffix=".tmp", dir=destination.parent
    )
    temporary = Path(temporary_name)
    try:
        with os.fdopen(file_descriptor, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(payload)
            handle.flush()
            os.fsync(handle.fileno())
        # A same-directory hard link publishes the completed inode and fails
        # if another process created the destination after our first check.
        os.link(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)
    return destination
