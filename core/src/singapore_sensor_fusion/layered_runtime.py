"""Build a layered-detection snapshot from current simulated sensor observations.

Operator contacts are seeded only by fresh Unreal ``detectedRFLinks``, search
radar/PTZ observations, or explicitly fusion-eligible model reports.  Authored
``scenarioTargets`` are retained solely as optional evaluator metadata: they do
not create contacts, regenerate observations, or provide an operational track
position.  Unreal v3 search-radar and radar-cued EO/thermal records are
validated fail-closed and preserve explicit simulation-projection provenance.

All outputs are simulation-only and detection-only.  Scenario disposition is
kept separate from sensor evidence and no engagement action is represented.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import time
from typing import Any, Iterable, Mapping, Sequence

from .event_camera import PROXY_EVENT_ENCODING
from .fusion_v3 import (
    FUSION_V3_METHOD,
    LayeredEvidence,
    LayeredModality,
    fuse_layered_evidence,
)
from .geodesy import ENU, Geodetic, enu_to_geodetic
from .paths import DEFAULT_OUTPUT_DIRECTORY, DEFAULT_TRIAD_SENSOR_SAVED_DIR
from .wideband_rf import WidebandChannel


INPUT_SCHEMA = "triad.live_rf_snapshot.v1"
INPUT_SCHEMAS = frozenset(
    (INPUT_SCHEMA, "triad.live_rf_snapshot.v2", "triad.live_rf_snapshot.v3")
)
OUTPUT_SCHEMA = "triad.layered_detection_snapshot.v1"
DEFAULT_INPUT = DEFAULT_TRIAD_SENSOR_SAVED_DIR / "latest_rf_snapshot.json"
DEFAULT_OUTPUT = DEFAULT_TRIAD_SENSOR_SAVED_DIR / "latest_layered_snapshot.json"
DEFAULT_OAK_REPORT = DEFAULT_OUTPUT_DIRECTORY / "oak_rgbd_live_watch_latest.json"
DEFAULT_EVENT_REPORT = DEFAULT_OUTPUT_DIRECTORY / "scaled_sensor_fusion_experiment.json"
MAX_INPUT_BYTES = 8 * 1024 * 1024
MAX_LINKS_PER_MODALITY = 4096
DEFAULT_NODE_RF_RANGE_METERS = 20_000.0
DEFAULT_SEARCH_RADAR_RANGE_METERS = 5_000.0
DEFAULT_PTZ_CONFIRMATION_RANGE_METERS = 2_000.0
OAK_REPORT_SCHEMA = "1.0"
OAK_REPORT_MODE = "bounded_live_unreal_oak_rgbd_watch"
_SHA256 = re.compile(r"^[0-9a-fA-F]{64}$")
_RADAR_PTZ_RELATIVE_PATH = re.compile(
    r"^RadarPtzFrames/(?:eo|thermal)/[^/]+/[^/]+\.(?:png|json)$",
    re.IGNORECASE,
)


WIDEBAND_CHANNELS = tuple(
    WidebandChannel(
        frequency,
        bandwidth_hz=2e6,
        dwell_time_ms=8.0,
        false_alarm_probability=1e-7,
    )
    for frequency in (
        169.4e6,
        433.92e6,
        868.0e6,
        915.0e6,
        1.28e9,
        2.412e9,
        2.437e9,
        2.462e9,
        5.765e9,
        5.795e9,
        5.825e9,
        6.2e9,
    )
)


def _parse_timestamp(value: object) -> datetime:
    if not isinstance(value, str) or not value.strip():
        raise ValueError("timestamp must be a non-empty ISO-8601 string")
    text = value.strip()
    normalized = text[:-1] + "+00:00" if text.endswith("Z") else text
    result = datetime.fromisoformat(normalized)
    if result.tzinfo is None or result.utcoffset() is None:
        raise ValueError("timestamp must include an offset")
    return result.astimezone(timezone.utc)


def _number(value: object, default: float = 0.0) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return default
    result = float(value)
    return result if math.isfinite(result) else default


def _read_json(path: Path, *, required: bool = False) -> dict[str, Any] | None:
    try:
        if path.stat().st_size > MAX_INPUT_BYTES:
            raise ValueError(f"{path} exceeds the {MAX_INPUT_BYTES}-byte input limit")
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        if required:
            raise
        return None
    except (OSError, json.JSONDecodeError):
        if required:
            raise
        return None
    if not isinstance(value, dict):
        if required:
            raise ValueError(f"{path} must contain a JSON object")
        return None
    return value


def _normalise_runtime_status(value: object) -> str:
    status = str(value).strip().upper() if value is not None else ""
    return status if status in {"ONLINE", "DEGRADED", "OFFLINE"} else "OFFLINE"


def _nonempty_text(value: object) -> str | None:
    if not isinstance(value, str) or not value.strip():
        return None
    return value.strip()


def _safe_radar_ptz_relative_path(
    value: object,
    *,
    node_id: str,
    sensor_type: str,
    extension: str,
) -> str | None:
    """Return a bounded Saved-directory relative path, or fail closed.

    No absolute path, drive name, parent traversal, or unrelated Saved subtree
    is accepted.  The bridge subsequently applies the same allow-list before
    serving a frame.
    """

    text = _nonempty_text(value)
    if text is None:
        return None
    normalized = text.replace("\\", "/")
    if normalized.startswith("/") or ":" in normalized or ".." in normalized.split("/"):
        return None
    modality_dir = "eo" if sensor_type == "EO_PTZ" else "thermal"
    expected_prefix = f"RadarPtzFrames/{modality_dir}/{node_id}/"
    if not normalized.casefold().startswith(expected_prefix.casefold()):
        return None
    expected_path = f"{expected_prefix}latest{extension}"
    if normalized.casefold() != expected_path.casefold():
        return None
    if not _RADAR_PTZ_RELATIVE_PATH.fullmatch(normalized):
        return None
    return normalized


def _bounded_confidence(value: object) -> float | None:
    score = _number(value, math.nan)
    return score if math.isfinite(score) and 0.0 <= score <= 1.0 else None


def _optional_number(value: object) -> float | None:
    result = _number(value, math.nan)
    return result if math.isfinite(result) else None


def _rejection(
    *,
    input_index: int,
    sensor_type: object,
    node_id: object,
    target_actor: object,
    reason: str,
) -> dict[str, Any]:
    return {
        "inputIndex": input_index,
        "sensorType": _nonempty_text(sensor_type),
        "nodeId": _nonempty_text(node_id),
        "targetActor": _nonempty_text(target_actor),
        "reason": reason,
    }


def _normalise_detected_rf_links(
    snapshot: Mapping[str, Any],
    *,
    as_of: datetime,
    nodes_by_id: Mapping[str, Mapping[str, Any]],
    maximum_age_s: float,
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    """Validate and reduce Unreal RF detections to observation-only evidence.

    Unreal includes evaluator annotations (target coordinates, hostility, and
    authored kinematics) on each link.  Those fields are deliberately omitted
    from the accepted record so an RF-only contact cannot inherit truth state.
    """

    raw_items = snapshot.get("detectedRFLinks", [])
    if not isinstance(raw_items, list):
        return [], [
            _rejection(
                input_index=-1,
                sensor_type="WIDEBAND_RF",
                node_id=None,
                target_actor=None,
                reason="detectedRFLinks must be an array",
            )
        ]

    accepted: list[dict[str, Any]] = []
    rejected: list[dict[str, Any]] = []
    seen: set[tuple[str, str, int, str]] = set()
    for index, raw in enumerate(raw_items[:MAX_LINKS_PER_MODALITY]):
        if not isinstance(raw, Mapping):
            rejected.append(
                _rejection(
                    input_index=index,
                    sensor_type="WIDEBAND_RF",
                    node_id=None,
                    target_actor=None,
                    reason="record is not an object",
                )
            )
            continue

        node_id = _nonempty_text(raw.get("nodeId"))
        target_id = _nonempty_text(raw.get("targetActor"))
        timestamp_text = _nonempty_text(raw.get("timestampUtc"))
        node = nodes_by_id.get(node_id or "", {})
        frequency_ghz = _number(raw.get("frequencyGHz"), math.nan)
        range_m = _number(raw.get("slantRangeMeters"), math.nan)
        received_power_dbm = _number(raw.get("receivedPowerDbm"), math.nan)
        noise_floor_dbm = _number(raw.get("noiseFloorDbm"), math.nan)
        snr_db = _number(raw.get("snrDb"), math.nan)
        configured_range_m, _ = _node_rf_range(node)
        sensitivity_dbm = _optional_number(node.get("receiverSensitivityDbm"))
        supported = node.get("supportedFrequenciesGHz")
        supported_frequencies = (
            [
                float(value)
                for value in supported
                if isinstance(value, (int, float))
                and not isinstance(value, bool)
                and math.isfinite(float(value))
            ]
            if isinstance(supported, list)
            else []
        )

        reason: str | None = None
        if raw.get("detected") is not True:
            reason = "detected must be true"
        elif node_id not in nodes_by_id:
            reason = "nodeId is not an enabled, spawned sensor site"
        elif _normalise_runtime_status(node.get("runtimeStatus")) == "OFFLINE":
            reason = "RF receiver runtime status is OFFLINE"
        elif target_id is None:
            reason = "targetActor is missing"
        elif timestamp_text is None or not _timestamp_is_fresh(
            timestamp_text, as_of, maximum_age_s
        ):
            reason = "timestamp is missing, stale, or in the future"
        elif not math.isfinite(frequency_ghz) or frequency_ghz <= 0.0:
            reason = "frequencyGHz must be finite and > 0"
        elif supported_frequencies and not any(
            abs(frequency_ghz - value) <= 1e-6 for value in supported_frequencies
        ):
            reason = "frequencyGHz is not supported by this node"
        elif not math.isfinite(range_m) or range_m <= 0.0:
            reason = "slantRangeMeters must be finite and > 0"
        elif range_m > configured_range_m + 1e-6:
            reason = "slantRangeMeters exceeds the configured node RF envelope"
        elif not all(
            math.isfinite(value)
            for value in (received_power_dbm, noise_floor_dbm, snr_db)
        ):
            reason = "receivedPowerDbm, noiseFloorDbm, and snrDb must be finite"
        elif abs((received_power_dbm - noise_floor_dbm) - snr_db) > 0.25:
            reason = "snrDb is inconsistent with receivedPowerDbm minus noiseFloorDbm"
        elif sensitivity_dbm is not None and received_power_dbm < sensitivity_dbm - 1e-6:
            reason = "receivedPowerDbm is below the configured receiver sensitivity"
        elif not isinstance(raw.get("lineOfSight"), bool):
            reason = "lineOfSight must be an explicit boolean"

        if reason is not None:
            rejected.append(
                _rejection(
                    input_index=index,
                    sensor_type="WIDEBAND_RF",
                    node_id=node_id,
                    target_actor=target_id,
                    reason=reason,
                )
            )
            continue

        assert node_id is not None and target_id is not None and timestamp_text is not None
        duplicate_key = (
            node_id,
            target_id,
            round(frequency_ghz * 1e9),
            timestamp_text,
        )
        if duplicate_key in seen:
            rejected.append(
                _rejection(
                    input_index=index,
                    sensor_type="WIDEBAND_RF",
                    node_id=node_id,
                    target_actor=target_id,
                    reason="duplicate node/target/frequency/timestamp record",
                )
            )
            continue
        seen.add(duplicate_key)

        margin_db = (
            max(0.0, received_power_dbm - sensitivity_dbm)
            if sensitivity_dbm is not None
            else max(0.0, snr_db)
        )
        score = 0.70 + 0.30 * (1.0 - math.exp(-margin_db / 12.0))
        link_id = _nonempty_text(raw.get("linkId")) or (
            f"{node_id}|{target_id}|{frequency_ghz:.6f}"
        )
        accepted.append(
            {
                "id": link_id,
                "linkId": link_id,
                "evidenceId": f"rf:{link_id}:{timestamp_text}",
                "timestampUtc": timestamp_text,
                "nodeId": node_id,
                "receiverId": f"triad:{node_id}:rf-wideband",
                "targetActor": target_id,
                "targetId": target_id,
                "emitterId": _nonempty_text(raw.get("emitterId")),
                "detected": True,
                "acceptedForFusion": True,
                "frequencyGHz": frequency_ghz,
                "centerFrequencyHz": frequency_ghz * 1e9,
                "frequencyBandLabel": _nonempty_text(raw.get("frequencyBandLabel"))
                or f"{frequency_ghz:g} GHz",
                "slantRangeMeters": range_m,
                "rangeMeters": range_m,
                "rangeSemantics": "sensor_to_target_3d_slant_range",
                "azimuthDegrees": _optional_number(raw.get("azimuthDegrees")),
                "elevationDegrees": _optional_number(raw.get("elevationDegrees")),
                "lineOfSight": raw.get("lineOfSight"),
                "blockingActor": _nonempty_text(raw.get("blockingActor")),
                "receivedPowerDbm": received_power_dbm,
                "noiseFloorDbm": noise_floor_dbm,
                "snrDb": snr_db,
                "weatherRFLossDb": _optional_number(raw.get("weatherRFLossDb")),
                "score": score,
                "scoreSemantics": (
                    "uncalibrated analytic evidence index from the binary Unreal RF "
                    "detection and excess receiver-sensitivity margin"
                ),
                "method": (
                    "validated current Unreal analytic RF link-budget detection; "
                    "not a measured or calibrated field result"
                ),
                "kind": "SIMULATED_SENSOR_DETECTION",
                "source": "UNREAL_ANALYTIC_RF_LINK",
                "simulated": True,
                "calibratedDetector": False,
                "detectionOnly": True,
                "modelOutputClaimed": False,
                "fieldValidated": False,
            }
        )
    return accepted, rejected


def _normalise_search_radar_detections(
    snapshot: Mapping[str, Any],
    *,
    as_of: datetime,
    nodes_by_id: Mapping[str, Mapping[str, Any]],
    maximum_age_s: float,
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    raw_items = snapshot.get("searchRadarDetections", [])
    if not isinstance(raw_items, list):
        return [], [
            _rejection(
                input_index=-1,
                sensor_type="SEARCH_RADAR",
                node_id=None,
                target_actor=None,
                reason="searchRadarDetections must be an array",
            )
        ]
    accepted: list[dict[str, Any]] = []
    rejected: list[dict[str, Any]] = []
    seen: set[tuple[str, str, str]] = set()
    for index, raw in enumerate(raw_items[:MAX_LINKS_PER_MODALITY]):
        if not isinstance(raw, Mapping):
            rejected.append(
                _rejection(
                    input_index=index,
                    sensor_type="SEARCH_RADAR",
                    node_id=None,
                    target_actor=None,
                    reason="record is not an object",
                )
            )
            continue
        node_id = _nonempty_text(raw.get("nodeId"))
        target_id = _nonempty_text(raw.get("targetActor"))
        sensor_id = _nonempty_text(raw.get("sensorId"))
        track_id = _nonempty_text(raw.get("trackId"))
        timestamp_text = _nonempty_text(raw.get("timestampUtc"))
        node = nodes_by_id.get(node_id or "", {})
        reason: str | None = None
        if raw.get("kind") != "SIMULATED_SENSOR_DETECTION":
            reason = "kind must be SIMULATED_SENSOR_DETECTION"
        elif raw.get("source") != "ANALYTIC_SEARCH_RADAR":
            reason = "source must be ANALYTIC_SEARCH_RADAR"
        elif raw.get("simulated") is not True or raw.get("calibratedDetector") is not False:
            reason = "search radar must declare simulated=true and calibratedDetector=false"
        elif raw.get("detectionOnly") is not True:
            reason = "search radar must declare detectionOnly=true"
        elif raw.get("sensorType") != "SEARCH_RADAR":
            reason = "sensorType must be SEARCH_RADAR"
        elif node_id not in nodes_by_id:
            reason = "nodeId is not an enabled, spawned sensor site"
        elif node.get("searchRadarConfigured") is not True:
            reason = "search radar is not explicitly configured at this node"
        elif _normalise_runtime_status(node.get("searchRadarRuntimeStatus")) == "OFFLINE":
            reason = "search radar runtime status is OFFLINE"
        elif target_id is None:
            reason = "targetActor is missing"
        elif sensor_id != f"{node_id}:SEARCH_RADAR":
            reason = "sensorId does not match nodeId:SEARCH_RADAR"
        elif track_id is None:
            reason = "trackId is missing"
        elif timestamp_text is None or not _timestamp_is_fresh(
            timestamp_text, as_of, maximum_age_s
        ):
            reason = "timestamp is missing, stale, or in the future"
        elif not isinstance(raw.get("lineOfSight"), bool):
            reason = "lineOfSight must be an explicit boolean"

        range_m = _number(raw.get("rangeMeters"), math.nan)
        bearing_deg = _number(raw.get("bearingDegrees"), math.nan)
        elevation_deg = _number(raw.get("elevationDegrees"), math.nan)
        radial_velocity = _number(raw.get("radialVelocityMetersPerSecond"), math.nan)
        confidence = _bounded_confidence(raw.get("confidence"))
        rcs_m2 = _number(raw.get("radarCrossSectionSquareMeters"), math.nan)
        configured_envelope_m = _number(
            node.get("searchRadarRangeMeters"), DEFAULT_SEARCH_RADAR_RANGE_METERS
        )
        declared_envelope_m = _number(
            raw.get("rangeEnvelopeMeters"), configured_envelope_m
        )
        envelope_m = min(configured_envelope_m, declared_envelope_m)
        azimuth_for_deg = _number(raw.get("azimuthFieldOfRegardDegrees"), 360.0)
        elevation_for_deg = _number(
            raw.get("elevationFieldOfRegardDegrees"), math.nan
        )
        if reason is None and (not math.isfinite(range_m) or range_m <= 0.0):
            reason = "rangeMeters must be finite and > 0"
        elif reason is None and (
            not math.isfinite(configured_envelope_m)
            or configured_envelope_m <= 0.0
            or not math.isfinite(declared_envelope_m)
            or declared_envelope_m <= 0.0
        ):
            reason = "configured and declared search-radar envelopes must be finite and > 0"
        elif reason is None and declared_envelope_m > configured_envelope_m + 1e-6:
            reason = "declared rangeEnvelopeMeters exceeds the configured node envelope"
        elif reason is None and range_m > envelope_m:
            reason = "rangeMeters is outside the declared search-radar envelope"
        elif reason is None and (
            not math.isfinite(azimuth_for_deg) or not 0.0 < azimuth_for_deg <= 360.0
        ):
            reason = "azimuthFieldOfRegardDegrees must be in (0, 360]"
        elif reason is None and (
            not math.isfinite(elevation_for_deg)
            or not 0.0 < elevation_for_deg <= 180.0
        ):
            reason = "elevationFieldOfRegardDegrees must be in (0, 180]"
        elif reason is None and (
            not math.isfinite(bearing_deg) or not 0.0 <= bearing_deg < 360.0
        ):
            reason = "bearingDegrees must be in [0, 360)"
        elif reason is None and (
            not math.isfinite(elevation_deg) or not -90.0 <= elevation_deg <= 90.0
        ):
            reason = "elevationDegrees must be in [-90, 90]"
        elif reason is None and not math.isfinite(radial_velocity):
            reason = "radialVelocityMetersPerSecond must be finite"
        elif reason is None and confidence is None:
            reason = "confidence must be finite and in [0, 1]"
        elif reason is None and (not math.isfinite(rcs_m2) or rcs_m2 < 0.0):
            reason = "radarCrossSectionSquareMeters must be finite and >= 0"

        if reason is not None:
            rejected.append(
                _rejection(
                    input_index=index,
                    sensor_type=raw.get("sensorType"),
                    node_id=node_id,
                    target_actor=target_id,
                    reason=reason,
                )
            )
            continue
        assert node_id is not None and target_id is not None
        assert sensor_id is not None and track_id is not None and timestamp_text is not None
        assert confidence is not None
        duplicate_key = (sensor_id, target_id, timestamp_text)
        if duplicate_key in seen:
            rejected.append(
                _rejection(
                    input_index=index,
                    sensor_type="SEARCH_RADAR",
                    node_id=node_id,
                    target_actor=target_id,
                    reason="duplicate sensor/target/timestamp record",
                )
            )
            continue
        seen.add(duplicate_key)
        accepted.append(
            {
                "id": _nonempty_text(raw.get("id"))
                or f"radar:{sensor_id}:{track_id}:{timestamp_text}",
                "evidenceId": f"radar:{sensor_id}:{track_id}:{timestamp_text}",
                "timestampUtc": timestamp_text,
                "simulationSeconds": _optional_number(raw.get("simulationSeconds")),
                "nodeId": node_id,
                "sensorId": sensor_id,
                "sensorType": "SEARCH_RADAR",
                "modality": "SEARCH_RADAR",
                "trackId": track_id,
                "targetActor": target_id,
                "detected": True,
                "acceptedForFusion": True,
                "rangeMeters": range_m,
                "rangeM": range_m,
                "bearingDegrees": bearing_deg,
                "bearingDeg": bearing_deg,
                "elevationDegrees": elevation_deg,
                "elevationDeg": elevation_deg,
                "radialVelocityMetersPerSecond": radial_velocity,
                "radialSpeedMps": radial_velocity,
                "closingSpeedMetersPerSecond": max(0.0, -radial_velocity),
                "closingSpeedMps": max(0.0, -radial_velocity),
                "confidence": confidence,
                "score": confidence,
                "radarCrossSectionSquareMeters": rcs_m2,
                "lineOfSight": raw.get("lineOfSight"),
                "blockingActor": _nonempty_text(raw.get("blockingActor")),
                "weatherProfile": raw.get("weatherProfile"),
                "weatherVisibilityMeters": _optional_number(
                    raw.get("weatherVisibilityMeters")
                ),
                "weatherRainRateMillimetersPerHour": _optional_number(
                    raw.get("weatherRainRateMillimetersPerHour")
                ),
                "rangeEnvelopeMeters": envelope_m,
                "azimuthFieldOfRegardDegrees": azimuth_for_deg,
                "elevationFieldOfRegardDegrees": elevation_for_deg,
                "method": (
                    "Unreal deterministic simulated search-radar measurement; confidence is "
                    "an evidence index, not a calibrated probability or measured field result"
                ),
                "kind": "SIMULATED_SENSOR_DETECTION",
                "source": "ANALYTIC_SEARCH_RADAR",
                "simulated": True,
                "calibratedDetector": False,
                "detectionOnly": True,
                "confidenceSemantics": raw.get("confidenceSemantics"),
                "radarCrossSectionSemantics": raw.get(
                    "radarCrossSectionSemantics"
                ),
                "measurementNoiseModel": raw.get("measurementNoiseModel"),
                "modelOutputClaimed": False,
                "fieldValidated": False,
            }
        )
    return accepted, rejected


def _normalise_ptz_confirmations(
    snapshot: Mapping[str, Any],
    *,
    as_of: datetime,
    nodes_by_id: Mapping[str, Mapping[str, Any]],
    accepted_radar: Sequence[Mapping[str, Any]],
    maximum_age_s: float,
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    raw_items = snapshot.get("ptzConfirmations", [])
    if not isinstance(raw_items, list):
        return [], [
            _rejection(
                input_index=-1,
                sensor_type=None,
                node_id=None,
                target_actor=None,
                reason="ptzConfirmations must be an array",
            )
        ]
    radar_cues = {
        (
            str(item.get("targetActor")),
            str(item.get("sensorId")),
            str(item.get("trackId")),
        ): item
        for item in accepted_radar
    }
    accepted: list[dict[str, Any]] = []
    rejected: list[dict[str, Any]] = []
    seen: set[tuple[str, str, str]] = set()
    for index, raw in enumerate(raw_items[:MAX_LINKS_PER_MODALITY]):
        if not isinstance(raw, Mapping):
            rejected.append(
                _rejection(
                    input_index=index,
                    sensor_type=None,
                    node_id=None,
                    target_actor=None,
                    reason="record is not an object",
                )
            )
            continue
        sensor_type = _nonempty_text(raw.get("sensorType"))
        node_id = _nonempty_text(raw.get("nodeId"))
        target_id = _nonempty_text(raw.get("targetActor"))
        sensor_id = _nonempty_text(raw.get("sensorId"))
        track_id = _nonempty_text(raw.get("trackId"))
        timestamp_text = _nonempty_text(raw.get("timestampUtc"))
        radar_cue_sensor_id = _nonempty_text(raw.get("radarCueSensorId"))
        node = nodes_by_id.get(node_id or "", {})
        cue_key = (target_id or "", radar_cue_sensor_id or "", track_id or "")
        radar_cue = radar_cues.get(cue_key)
        expected_modality = (
            "EO_VISIBLE" if sensor_type == "EO_PTZ" else "THERMAL_SYNTHETIC"
        )
        reason: str | None = None
        if raw.get("kind") != "SIMULATED_SENSOR_CONFIRMATION":
            reason = "kind must be SIMULATED_SENSOR_CONFIRMATION"
        elif raw.get("source") != "SIMULATION_PROJECTION":
            reason = "source must be SIMULATION_PROJECTION"
        elif raw.get("confirmationMethod") != "SIMULATION_PROJECTION_TRUTH":
            reason = "confirmationMethod must be SIMULATION_PROJECTION_TRUTH"
        elif raw.get("boxSource") != "DEBUG_PROJECTION":
            reason = "boxSource must be DEBUG_PROJECTION"
        elif raw.get("simulated") is not True or raw.get("calibratedDetector") is not False:
            reason = "PTZ must declare simulated=true and calibratedDetector=false"
        elif raw.get("actionsTaken") != "none":
            reason = "PTZ must declare actionsTaken=none"
        elif sensor_type not in {"EO_PTZ", "THERMAL_PTZ"}:
            reason = "sensorType must be EO_PTZ or THERMAL_PTZ"
        elif node_id not in nodes_by_id:
            reason = "nodeId is not an enabled, spawned sensor site"
        elif sensor_type in {"EO_PTZ", "THERMAL_PTZ"}:
            configured_key = (
                "eoPtzConfigured"
                if sensor_type == "EO_PTZ"
                else "thermalPtzConfigured"
            )
            status_key = (
                "eoPtzRuntimeStatus"
                if sensor_type == "EO_PTZ"
                else "thermalPtzRuntimeStatus"
            )
            if node.get(configured_key) is not True:
                reason = f"{sensor_type} is not explicitly configured at this node"
            elif _normalise_runtime_status(node.get(status_key)) == "OFFLINE":
                reason = f"{sensor_type} runtime status is OFFLINE"
        if reason is None and target_id is None:
            reason = "targetActor is missing"
        elif reason is None and sensor_id != f"{node_id}:{sensor_type}":
            reason = "sensorId does not match nodeId:sensorType"
        elif reason is None and track_id is None:
            reason = "trackId is missing"
        elif reason is None and raw.get("modality") != expected_modality:
            reason = f"modality must be {expected_modality}"
        elif reason is None and (timestamp_text is None or not _timestamp_is_fresh(
            timestamp_text, as_of, maximum_age_s
        )):
            reason = "timestamp is missing, stale, or in the future"
        elif reason is None and raw.get("confirmed") is not True:
            reason = "confirmed must be true"
        elif reason is None and raw.get("lineOfSight") is not True:
            reason = "lineOfSight must be true; occluded PTZ records cannot confirm"
        elif reason is None and raw.get("hasFrame") is not True:
            reason = "hasFrame must be true for an accepted PTZ confirmation"
        elif reason is None and str(raw.get("slewState", "")).strip().upper() != "SETTLED":
            reason = "slewState must be SETTLED"
        elif reason is None and (radar_cue_sensor_id is None or radar_cue is None):
            reason = (
                "fresh accepted radar cue with the same targetActor, sensorId, and "
                "trackId is missing"
            )
        elif reason is None and sensor_type == "THERMAL_PTZ" and raw.get("syntheticThermal") is not True:
            reason = "THERMAL_PTZ must declare syntheticThermal=true"

        confidence = _bounded_confidence(raw.get("confidence"))
        cue_age_s = _number(raw.get("cueAgeSeconds"), math.nan)
        range_m = _number(raw.get("rangeMeters"), math.nan)
        width = _number(raw.get("imageWidthPixels", raw.get("width")), math.nan)
        height = _number(raw.get("imageHeightPixels", raw.get("height")), math.nan)
        fov_deg = _number(raw.get("fovDegrees", raw.get("horizontalFovDeg")), math.nan)
        pixel_extent_width = _number(raw.get("pixelExtentWidth"), math.nan)
        pixel_extent_height = _number(raw.get("pixelExtentHeight"), math.nan)
        weather_confidence_factor = _optional_number(
            raw.get("weatherConfidenceFactor")
        )
        range_field = (
            "eoPtzConfirmationRangeMeters"
            if sensor_type == "EO_PTZ"
            else "thermalPtzConfirmationRangeMeters"
        )
        range_envelope_m = _number(
            node.get(range_field), DEFAULT_PTZ_CONFIRMATION_RANGE_METERS
        )
        frame_path = _safe_radar_ptz_relative_path(
            raw.get("frameRelativePath", raw.get("relativePath")),
            node_id=node_id or "",
            sensor_type=sensor_type or "",
            extension=".png",
        )
        metadata_path_value = raw.get("metadataRelativePath")
        metadata_path = (
            _safe_radar_ptz_relative_path(
                metadata_path_value,
                node_id=node_id or "",
                sensor_type=sensor_type or "",
                extension=".json",
            )
            if metadata_path_value is not None
            else None
        )
        box = _box(raw.get("boundingBoxPixels"))
        if reason is None and confidence is None:
            reason = "confidence must be finite and in [0, 1]"
        elif reason is None and raw.get("weatherConfidenceFactor") is not None and (
            weather_confidence_factor is None
            or not 0.0 <= weather_confidence_factor <= 1.0
        ):
            reason = "weatherConfidenceFactor must be finite and in [0, 1]"
        elif reason is None and (
            not math.isfinite(cue_age_s) or not 0.0 <= cue_age_s <= maximum_age_s
        ):
            reason = "cueAgeSeconds is outside the fresh evidence window"
        elif reason is None and (
            not math.isfinite(range_m)
            or range_m <= 0.0
            or not math.isfinite(range_envelope_m)
            or range_envelope_m <= 0.0
            or range_m > range_envelope_m
        ):
            reason = "rangeMeters is outside the configured PTZ confirmation envelope"
        elif reason is None and radar_cue is not None and abs(
            range_m - _number(radar_cue.get("rangeMeters"), math.nan)
        ) > max(5.0, _number(radar_cue.get("rangeMeters"), 0.0) * 0.02):
            reason = "PTZ rangeMeters does not match its associated radar cue"
        elif reason is None and (
            not math.isfinite(width)
            or not math.isfinite(height)
            or width <= 0.0
            or height <= 0.0
        ):
            reason = "image dimensions must be finite and > 0"
        elif reason is None and (not math.isfinite(fov_deg) or not 0.0 < fov_deg <= 180.0):
            reason = "horizontal field of view must be in (0, 180]"
        elif reason is None and box is None:
            reason = "boundingBoxPixels is missing or invalid"
        elif reason is None and (
            not math.isfinite(pixel_extent_width)
            or not math.isfinite(pixel_extent_height)
            or pixel_extent_width < 2.0
            or pixel_extent_height < 2.0
        ):
            reason = "pixel extent must be at least 2 pixels in each dimension"
        elif reason is None and box is not None and (
            box[0] < 0.0 or box[1] < 0.0 or box[2] > width or box[3] > height
        ):
            reason = "boundingBoxPixels lies outside the image"
        elif reason is None and frame_path is None:
            reason = "frame path is missing or outside the RadarPtzFrames allow-list"
        elif reason is None and metadata_path_value is not None and metadata_path is None:
            reason = "metadata path is outside the RadarPtzFrames allow-list"

        if reason is not None:
            rejected.append(
                _rejection(
                    input_index=index,
                    sensor_type=sensor_type,
                    node_id=node_id,
                    target_actor=target_id,
                    reason=reason,
                )
            )
            continue
        assert sensor_type is not None and node_id is not None and target_id is not None
        assert sensor_id is not None and track_id is not None and timestamp_text is not None
        assert radar_cue_sensor_id is not None and confidence is not None
        assert frame_path is not None and box is not None
        duplicate_key = (sensor_id, target_id, timestamp_text)
        if duplicate_key in seen:
            rejected.append(
                _rejection(
                    input_index=index,
                    sensor_type=sensor_type,
                    node_id=node_id,
                    target_actor=target_id,
                    reason="duplicate sensor/target/timestamp record",
                )
            )
            continue
        seen.add(duplicate_key)
        label = (
            "SIM EO RADAR-CUED CONFIRMATION"
            if sensor_type == "EO_PTZ"
            else "SIM THERMAL RADAR-CUED CONFIRMATION"
        )
        box_id = f"{sensor_id}:{track_id}:projection"
        standard_box = {
            "id": box_id,
            "kind": "SIMULATED_SENSOR_CONFIRMATION",
            "sensorType": sensor_type,
            "label": "SIMULATED SENSOR CONFIRMATION",
            "confidence": confidence,
            "bbox": [round(value, 3) for value in box],
            "coordinateSpace": "PIXELS",
            "source": "SIMULATION_PROJECTION",
        }
        reticle = raw.get("reticle") if isinstance(raw.get("reticle"), Mapping) else {}
        reticle_space = str(reticle.get("coordinateSpace", "PIXELS")).upper()
        reticle_x = _number(reticle.get("x"), (box[0] + box[2]) * 0.5)
        reticle_y = _number(reticle.get("y"), (box[1] + box[3]) * 0.5)
        if reticle_space not in {"PIXELS", "NORMALIZED"}:
            reticle_space = "PIXELS"
        accepted.append(
            {
                "id": _nonempty_text(raw.get("id"))
                or f"ptz:{sensor_id}:{track_id}:{timestamp_text}",
                "evidenceId": f"ptz:{sensor_id}:{track_id}:{timestamp_text}",
                "timestampUtc": timestamp_text,
                "simulationSeconds": _optional_number(raw.get("simulationSeconds")),
                "nodeId": node_id,
                "sensorId": sensor_id,
                "sensorType": sensor_type,
                "modality": sensor_type,
                "sourceModality": expected_modality,
                "trackId": track_id,
                "targetActor": target_id,
                "radarCueSensorId": radar_cue_sensor_id,
                "cueAgeSeconds": cue_age_s,
                "slewState": "SETTLED",
                "confirmed": True,
                "lineOfSight": True,
                "blockingActor": _nonempty_text(raw.get("blockingActor")),
                "hasFrame": True,
                "acceptedForFusion": True,
                "rangeMeters": range_m,
                "rangeM": range_m,
                "confidence": confidence,
                "score": confidence,
                "fovDegrees": fov_deg,
                "horizontalFovDeg": _number(raw.get("horizontalFovDeg"), fov_deg),
                "azimuthDeg": _optional_number(raw.get("azimuthDeg")),
                "elevationDeg": _optional_number(raw.get("elevationDeg")),
                "imageWidthPixels": int(round(width)),
                "imageHeightPixels": int(round(height)),
                "width": int(round(width)),
                "height": int(round(height)),
                "pixelExtentWidth": pixel_extent_width,
                "pixelExtentHeight": pixel_extent_height,
                "boundingBoxPixels": [round(value, 3) for value in box],
                "frameRelativePath": frame_path,
                "relativePath": frame_path,
                "metadataRelativePath": metadata_path,
                "syntheticThermal": sensor_type == "THERMAL_PTZ",
                "thermalSemantics": raw.get("thermalSemantics")
                if sensor_type == "THERMAL_PTZ"
                else None,
                "simulated": True,
                "calibratedDetector": False,
                "kind": "SIMULATED_SENSOR_CONFIRMATION",
                "source": "SIMULATION_PROJECTION",
                "confirmationMethod": "SIMULATION_PROJECTION_TRUTH",
                "inputBoxSource": "DEBUG_PROJECTION",
                "sourceLabel": raw.get("label"),
                "weatherProfile": raw.get("weatherProfile"),
                "weatherVisibilityMeters": _optional_number(
                    raw.get("weatherVisibilityMeters")
                ),
                "weatherRainRateMillimetersPerHour": _optional_number(
                    raw.get("weatherRainRateMillimetersPerHour")
                ),
                "weatherConfidenceFactor": weather_confidence_factor,
                "weatherConfidenceSemantics": raw.get(
                    "weatherConfidenceSemantics"
                ),
                "occlusionSemantics": raw.get("occlusionSemantics"),
                "label": label,
                "reticle": {
                    "x": round(reticle_x, 6),
                    "y": round(reticle_y, 6),
                    "coordinateSpace": reticle_space,
                    "label": "RADAR CUE RETICLE",
                },
                "boxes": [standard_box],
                "projectionTruthOnly": True,
                "modelOutputClaimed": False,
                "modelId": None,
                "method": (
                    "radar-cued deterministic Unreal projection confirmation; simulated "
                    "ground-truth box, not a learned-model detection or field result"
                ),
                "rangeSemantics": "radar-cued range; not stereo or monocular camera depth",
                "eoBoxAndRangeCountAsSeparateEvidence": False,
            }
        )
    return accepted, rejected


def _long_range_sensor_records(
    node: Mapping[str, Any],
    *,
    common: Mapping[str, Any],
    radar_rows: Sequence[Mapping[str, Any]],
    ptz_rows: Sequence[Mapping[str, Any]],
) -> list[dict[str, Any]]:
    node_id = str(node.get("nodeId", "unknown"))
    stack_rows = node.get("longRangeSensorStack")
    stack_by_type = {
        str(item.get("sensorType")): item
        for item in stack_rows
        if isinstance(item, Mapping) and _nonempty_text(item.get("sensorType"))
    } if isinstance(stack_rows, list) else {}
    specifications = (
        (
            "SEARCH_RADAR",
            "searchRadarConfigured",
            "searchRadarRuntimeStatus",
            "searchRadarRangeMeters",
            DEFAULT_SEARCH_RADAR_RANGE_METERS,
            "simulated 360-degree long-range search radar",
        ),
        (
            "EO_PTZ",
            "eoPtzConfigured",
            "eoPtzRuntimeStatus",
            "eoPtzConfirmationRangeMeters",
            DEFAULT_PTZ_CONFIRMATION_RANGE_METERS,
            "radar-cued simulated visible-light zoom PTZ",
        ),
        (
            "THERMAL_PTZ",
            "thermalPtzConfigured",
            "thermalPtzRuntimeStatus",
            "thermalPtzConfirmationRangeMeters",
            DEFAULT_PTZ_CONFIRMATION_RANGE_METERS,
            "radar-cued synthetic thermal PTZ",
        ),
    )
    records: list[dict[str, Any]] = []
    for sensor_type, configured_key, status_key, range_key, default_range, band in specifications:
        stack = stack_by_type.get(sensor_type, {})
        configured_value = node.get(configured_key)
        configured = (
            configured_value
            if isinstance(configured_value, bool)
            else stack.get("enabled") is True
        )
        status_value = node.get(status_key, stack.get("runtimeStatus"))
        status = _normalise_runtime_status(status_value) if configured else "OFFLINE"
        range_m = _number(node.get(range_key, stack.get("rangeMeters")), default_range)
        if not math.isfinite(range_m) or range_m <= 0.0:
            range_m = default_range
        observations = (
            [item for item in radar_rows if item.get("nodeId") == node_id]
            if sensor_type == "SEARCH_RADAR"
            else [
                item
                for item in ptz_rows
                if item.get("nodeId") == node_id and item.get("sensorType") == sensor_type
            ]
        )
        newest = max(
            observations,
            key=lambda item: str(item.get("timestampUtc", "")),
            default={},
        )
        records.append(
            {
                **common,
                "sensorId": _nonempty_text(stack.get("sensorId"))
                or f"{node_id}:{sensor_type}",
                "sensorType": sensor_type,
                "modality": sensor_type,
                "band": band,
                "rangeMeters": range_m,
                "rangeSource": range_key if range_key in node else "simulation_default",
                "configured": bool(configured),
                "enabled": bool(configured),
                "status": status,
                "runtimeStatus": status,
                "simulated": True,
                "fieldValidated": False,
                "detectedObservationCount": len(observations),
                "lastEvidenceTimestampUtc": newest.get("timestampUtc"),
                "azimuthDeg": newest.get("azimuthDeg", newest.get("bearingDegrees")),
                "horizontalFovDeg": newest.get(
                    "horizontalFovDeg",
                    newest.get(
                        "azimuthFieldOfRegardDegrees",
                        stack.get("horizontalFovDegrees"),
                    ),
                ),
                "method": (
                    "health follows the explicit Unreal v3 per-sensor runtime status; "
                    "detections and claimed range are deterministic simulation outputs"
                ),
                "validationState": (
                    "SIMULATION ONLY; ONLINE means the simulated component is running, not "
                    "that physical long-range performance has been field validated"
                ),
                "healthSource": "unreal_v3_explicit_sensor_runtime_status",
            }
        )
    return records


def _node_rf_range(node: Mapping[str, Any]) -> tuple[float, str]:
    configured = _number(node.get("detectionRangeMeters"), math.nan)
    if math.isfinite(configured) and configured > 0.0:
        return configured, "unreal_node_detectionRangeMeters"
    return DEFAULT_NODE_RF_RANGE_METERS, "legacy_compatibility_default_20000m"


def _timestamp_age_seconds(value: object, as_of: datetime) -> float | None:
    try:
        return (as_of - _parse_timestamp(value)).total_seconds()
    except (TypeError, ValueError):
        return None


def _timestamp_is_fresh(
    value: object,
    as_of: datetime,
    maximum_age_s: float,
    *,
    future_tolerance_s: float = 0.1,
) -> bool:
    age = _timestamp_age_seconds(value, as_of)
    return age is not None and -future_tolerance_s <= age <= maximum_age_s


def write_json_atomic(value: Mapping[str, Any], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(value, indent=2, sort_keys=False, allow_nan=False),
        encoding="utf-8",
    )
    temporary.replace(path)


def _observed_target_type(
    rgb_rows: Sequence[Mapping[str, Any]],
    event_rows: Sequence[Mapping[str, Any]],
) -> str:
    """Return only a classification supported by fusion-eligible model evidence."""

    labels = {
        str(row.get("className", "")).strip().casefold()
        for row in (*rgb_rows, *event_rows)
        if row.get("fusionEligible") is True and str(row.get("className", "")).strip()
    }
    if any("drone" in label or "uas" in label for label in labels):
        return "UAS_CANDIDATE"
    return "UNCLASSIFIED_AERIAL_CONTACT"


def _weather_id(snapshot: Mapping[str, Any]) -> tuple[str, float, float]:
    weather = snapshot.get("weather")
    if not isinstance(weather, Mapping):
        return "Unknown", 0.0, 30_000.0
    return (
        str(weather.get("profile", "Unknown")),
        max(0.0, _number(weather.get("rainRateMillimetersPerHour"))),
        max(1.0, _number(weather.get("visibilityMeters"), 30_000.0)),
    )


def _visual_weather_multiplier(visibility_m: float, rain_rate_mm_h: float, *, event: bool) -> float:
    visibility_factor = min(1.0, max(0.35, visibility_m / 12_000.0))
    rain_factor = 1.0 / (1.0 + (0.012 if event else 0.020) * max(0.0, rain_rate_mm_h))
    floor = 0.45 if event else 0.35
    return max(floor, min(1.0, visibility_factor * rain_factor))


def _search_radar_position_estimate(
    rows: Sequence[Mapping[str, Any]],
    nodes_by_id: Mapping[str, Mapping[str, Any]],
) -> tuple[Geodetic, float] | None:
    """Convert accepted polar search-radar measurements to a weighted position.

    The conversion is measurement-derived inside the simulator, but its
    uncertainty is an engineering assumption and is not field calibrated.
    """

    candidates: list[tuple[Geodetic, float, float]] = []
    for row in rows:
        node = nodes_by_id.get(str(row.get("nodeId")))
        if node is None:
            continue
        range_m = _number(row.get("rangeMeters"), math.nan)
        bearing_deg = _number(row.get("bearingDegrees"), math.nan)
        elevation_deg = _number(row.get("elevationDegrees"), math.nan)
        confidence = _bounded_confidence(row.get("confidence"))
        if (
            not math.isfinite(range_m)
            or range_m <= 0.0
            or not math.isfinite(bearing_deg)
            or not math.isfinite(elevation_deg)
            or confidence is None
        ):
            continue
        origin = Geodetic(
            _number(node.get("latitudeDegrees")),
            _number(node.get("longitudeDegrees")),
            _number(node.get("heightMeters")),
        )
        elevation_rad = math.radians(elevation_deg)
        bearing_rad = math.radians(bearing_deg)
        horizontal_m = range_m * math.cos(elevation_rad)
        measurement = enu_to_geodetic(
            ENU(
                horizontal_m * math.sin(bearing_rad),
                horizontal_m * math.cos(bearing_rad),
                range_m * math.sin(elevation_rad),
            ),
            origin,
        )
        weight = max(0.05, confidence) ** 2
        candidates.append((measurement, weight, range_m))
    if not candidates:
        return None
    total_weight = sum(item[1] for item in candidates)
    estimate = Geodetic(
        sum(item[0].latitude_deg * item[1] for item in candidates) / total_weight,
        sum(item[0].longitude_deg * item[1] for item in candidates) / total_weight,
        sum(item[0].altitude_m * item[1] for item in candidates) / total_weight,
    )
    weighted_range_m = sum(item[2] * item[1] for item in candidates) / total_weight
    assumed_sigma_m = max(3.0, min(50.0, weighted_range_m * 0.005))
    return estimate, assumed_sigma_m


def _estimate_track_state(
    *,
    target_id: str,
    rf_node_count: int,
    mmwave_detected: bool,
    rgb_detected: bool,
    event_detected: bool,
    search_radar_rows: Sequence[Mapping[str, Any]] = (),
    nodes_by_id: Mapping[str, Mapping[str, Any]] | None = None,
    eo_ptz_detected: bool = False,
    thermal_ptz_detected: bool = False,
) -> dict[str, object]:
    """Create an observation-derived track state without simulator-truth fallback."""

    radar_estimate = _search_radar_position_estimate(
        search_radar_rows, nodes_by_id or {}
    )
    strongest_radar = max(
        search_radar_rows,
        key=lambda row: _number(row.get("confidence"), -math.inf),
        default=None,
    )
    speed_m_s: float | None = None
    heading_deg: float | None = None
    speed_sigma_m_s: float | None = None
    heading_sigma_deg: float | None = None
    if strongest_radar is not None:
        radial_velocity_m_s = _number(
            strongest_radar.get("radialVelocityMetersPerSecond"), math.nan
        )
        bearing_deg = _number(strongest_radar.get("bearingDegrees"), math.nan)
        if math.isfinite(radial_velocity_m_s) and math.isfinite(bearing_deg):
            speed_m_s = abs(radial_velocity_m_s)
            heading_deg = (
                bearing_deg + (180.0 if radial_velocity_m_s < 0.0 else 0.0)
            ) % 360.0
            speed_sigma_m_s = max(1.5, speed_m_s * 0.15)
            heading_sigma_deg = 25.0

    if radar_estimate is not None:
        position_sigma_m = radar_estimate[1]
        if eo_ptz_detected or thermal_ptz_detected:
            method = (
                "measurement-derived simulated search-radar polar position with "
                "radar-cued PTZ confirmation; speed/course are a radial-only radar "
                "approximation and not a full velocity solution"
            )
        else:
            method = (
                "measurement-derived simulated search-radar polar position; speed/course "
                "are a radial-only radar approximation and not a full velocity solution"
            )
    else:
        position_sigma_m = None
        evidence_labels = []
        if rf_node_count:
            evidence_labels.append(f"{rf_node_count} RF node(s)")
        if mmwave_detected:
            evidence_labels.append("mmWave")
        if rgb_detected:
            evidence_labels.append("RGB model")
        if event_detected:
            evidence_labels.append("event model")
        method = (
            "position unavailable: current "
            + (", ".join(evidence_labels) if evidence_labels else "sensor")
            + " evidence does not provide an observation-derived geodetic fix"
        )

    return {
        "position": radar_estimate[0] if radar_estimate is not None else None,
        "positionAvailable": radar_estimate is not None,
        "speedMetersPerSecond": speed_m_s,
        "headingDegrees": heading_deg,
        "kinematicsAvailable": speed_m_s is not None and heading_deg is not None,
        "horizontalOneSigmaMeters": position_sigma_m,
        "speedOneSigmaMetersPerSecond": speed_sigma_m_s,
        "headingOneSigmaDegrees": heading_sigma_deg,
        "method": method,
        "truthCoordinatesForwardedToC2": False,
        "calibratedFieldAccuracyClaimed": False,
        "truthSeededSimulationSurrogate": False,
        "measurementDerivedLocalization": radar_estimate is not None,
        "kinematicsTruthSeededSimulationSurrogate": False,
        "radialOnlyKinematicsApproximation": speed_m_s is not None,
        "searchRadarMeasurementCount": len(search_radar_rows),
        "searchRadarPolarGeometryConsumed": radar_estimate is not None,
        "uncertaintyEmpiricallyCalibrated": False,
        "sigmaSemantics": (
            "engineering assumptions for accepted radar observations; not measured covariance"
            if radar_estimate is not None
            else "unavailable because no observation-derived localization exists"
        ),
    }


def _estimated_perimeter_state(
    position: Geodetic | None,
    speed_m_s: float | None,
    heading_deg: float | None,
    perimeter: object,
) -> dict[str, object]:
    if position is None or speed_m_s is None or heading_deg is None:
        return {
            "state": "UNAVAILABLE",
            "outside": None,
            "distanceMeters": None,
            "approachRateMetersPerSecond": None,
            "timeToPerimeterSeconds": None,
        }
    if not isinstance(perimeter, Mapping) or perimeter.get("enabled") is False:
        return {
            "state": "UNAVAILABLE",
            "outside": None,
            "distanceMeters": None,
            "approachRateMetersPerSecond": None,
            "timeToPerimeterSeconds": None,
        }
    minimum_lon = _number(perimeter.get("minimumLongitudeDegrees"), math.nan)
    maximum_lon = _number(perimeter.get("maximumLongitudeDegrees"), math.nan)
    minimum_lat = _number(perimeter.get("minimumLatitudeDegrees"), math.nan)
    maximum_lat = _number(perimeter.get("maximumLatitudeDegrees"), math.nan)
    if not all(math.isfinite(value) for value in (minimum_lon, maximum_lon, minimum_lat, maximum_lat)):
        return {
            "state": "UNAVAILABLE",
            "outside": None,
            "distanceMeters": None,
            "approachRateMetersPerSecond": None,
            "timeToPerimeterSeconds": None,
        }
    mid_lat = math.radians((minimum_lat + maximum_lat) * 0.5)
    meters_per_lon_degree = 111_320.0 * max(0.01, math.cos(mid_lat))
    meters_per_lat_degree = 111_132.0
    inward_east = 0.0
    inward_north = 0.0
    if position.longitude_deg < minimum_lon:
        inward_east = (minimum_lon - position.longitude_deg) * meters_per_lon_degree
    elif position.longitude_deg > maximum_lon:
        inward_east = (maximum_lon - position.longitude_deg) * meters_per_lon_degree
    if position.latitude_deg < minimum_lat:
        inward_north = (minimum_lat - position.latitude_deg) * meters_per_lat_degree
    elif position.latitude_deg > maximum_lat:
        inward_north = (maximum_lat - position.latitude_deg) * meters_per_lat_degree
    outside = inward_east != 0.0 or inward_north != 0.0
    if not outside:
        edge_distance = min(
            (position.longitude_deg - minimum_lon) * meters_per_lon_degree,
            (maximum_lon - position.longitude_deg) * meters_per_lon_degree,
            (position.latitude_deg - minimum_lat) * meters_per_lat_degree,
            (maximum_lat - position.latitude_deg) * meters_per_lat_degree,
        )
        return {
            "state": "INSIDE",
            "outside": False,
            "distanceMeters": -max(0.0, edge_distance),
            "approachRateMetersPerSecond": 0.0,
            "timeToPerimeterSeconds": None,
        }
    distance = math.hypot(inward_east, inward_north)
    velocity_east = speed_m_s * math.sin(math.radians(heading_deg))
    velocity_north = speed_m_s * math.cos(math.radians(heading_deg))
    approach_rate = (
        velocity_east * inward_east + velocity_north * inward_north
    ) / max(distance, 1e-9)
    deadband = max(0.0, _number(perimeter.get("phaseRateDeadbandMetersPerSecond"), 0.25))
    state = "APPROACHING" if approach_rate > deadband else "DEPARTING" if approach_rate < -deadband else "OUTSIDE"
    return {
        "state": state,
        "outside": True,
        "distanceMeters": distance,
        "approachRateMetersPerSecond": approach_rate,
        "timeToPerimeterSeconds": distance / approach_rate if approach_rate > deadband else None,
    }


def _box(value: object, *, normalized: bool = False, width: float = 1.0, height: float = 1.0) -> tuple[float, float, float, float] | None:
    if not isinstance(value, Sequence) or isinstance(value, (str, bytes)) or len(value) != 4:
        return None
    numbers = tuple(_number(item, math.nan) for item in value)
    if not all(math.isfinite(item) for item in numbers):
        return None
    x1, y1, x2, y2 = numbers
    if normalized:
        x1, x2 = x1 * width, x2 * width
        y1, y2 = y1 * height, y2 * height
    if x2 <= x1 or y2 <= y1:
        return None
    return x1, y1, x2, y2


def _iou(left: tuple[float, float, float, float], right: tuple[float, float, float, float]) -> float:
    ix1, iy1 = max(left[0], right[0]), max(left[1], right[1])
    ix2, iy2 = min(left[2], right[2]), min(left[3], right[3])
    intersection = max(0.0, ix2 - ix1) * max(0.0, iy2 - iy1)
    union = (left[2] - left[0]) * (left[3] - left[1]) + (right[2] - right[0]) * (right[3] - right[1]) - intersection
    return intersection / union if union > 0.0 else 0.0


def _associate_box(
    detection_box: tuple[float, float, float, float],
    metadata: Mapping[str, Any],
) -> tuple[str, float] | None:
    targets = metadata.get("targets")
    if not isinstance(targets, list):
        return None
    candidates: list[tuple[float, str]] = []
    for target in targets:
        if not isinstance(target, Mapping) or target.get("intersectsFrame") is False:
            continue
        truth_box = _box(target.get("bboxXyxyPixels"))
        actor = target.get("actorName")
        if truth_box is None or not isinstance(actor, str) or not actor:
            continue
        truth_width = truth_box[2] - truth_box[0]
        truth_height = truth_box[3] - truth_box[1]
        if truth_width < 2.0 or truth_height < 2.0:
            # A sub-pixel or near-sub-pixel projection cannot support a
            # defensible detector association at the current resolution.
            continue
        detection_area = (detection_box[2] - detection_box[0]) * (
            detection_box[3] - detection_box[1]
        )
        truth_area = truth_width * truth_height
        area_ratio = detection_area / truth_area
        if not 0.15 <= area_ratio <= 25.0:
            continue
        score = _iou(detection_box, truth_box)
        if score > 0.0:
            candidates.append((score, actor))
    candidates.sort(reverse=True)
    if not candidates or candidates[0][0] < 0.02:
        return None
    if len(candidates) > 1 and candidates[1][0] >= candidates[0][0] * 0.85:
        return None
    return candidates[0][1], candidates[0][0]


def _load_metadata(path: object) -> dict[str, Any] | None:
    if not isinstance(path, str) or not path:
        return None
    return _read_json(Path(path))


def _checkpoint_integrity_verified(inputs: Mapping[str, Any]) -> bool:
    declared = inputs.get("declaredTrustedSha256")
    verified = inputs.get("verifiedCheckpointSha256")
    return (
        isinstance(declared, str)
        and isinstance(verified, str)
        and _SHA256.fullmatch(declared) is not None
        and _SHA256.fullmatch(verified) is not None
        and declared.casefold() == verified.casefold()
    )


def _frame_is_debug_clean(frame: Mapping[str, Any]) -> bool:
    if frame.get("debugVisualsExcludedFromSensorCapture") is True:
        return True
    metadata = _load_metadata(frame.get("metadataPath"))
    return bool(metadata and metadata.get("debugVisualsExcludedFromSensorCapture") is True)


def load_rgb_sensor_health(report_path: Path | None) -> list[dict[str, Any]]:
    """Load per-node OAK watcher health independently of detector candidates.

    A processed frame with zero detections is still a useful pipeline
    heartbeat.  Health records retain the watcher state, checkpoint integrity,
    frame timestamp, and clean-capture provenance so the registry can decide
    ONLINE/DEGRADED/OFFLINE without inheriting Unreal node health.
    """

    report = _read_json(report_path) if report_path else None
    if not report:
        return []
    inputs = report.get("inputs")
    inputs = inputs if isinstance(inputs, Mapping) else {}
    report_node_id = inputs.get("nodeId")
    frames = report.get("processedFrames")
    frames = frames if isinstance(frames, list) else []
    newest_by_node: dict[str, tuple[datetime, Mapping[str, Any]]] = {}
    for frame in frames[-256:]:
        if not isinstance(frame, Mapping):
            continue
        node_id = frame.get("nodeId", report_node_id)
        if not isinstance(node_id, str) or not node_id.strip():
            continue
        try:
            timestamp = _parse_timestamp(frame.get("timestampUtc"))
        except (TypeError, ValueError):
            continue
        existing = newest_by_node.get(node_id)
        if existing is None or timestamp > existing[0]:
            newest_by_node[node_id] = (timestamp, frame)

    node_ids = set(newest_by_node)
    if isinstance(report_node_id, str) and report_node_id.strip():
        node_ids.add(report_node_id)
    if not node_ids:
        return []

    schema_supported = (
        report.get("schemaVersion") == OAK_REPORT_SCHEMA
        and report.get("mode") == OAK_REPORT_MODE
    )
    checkpoint_verified = _checkpoint_integrity_verified(inputs)
    report_updated = report.get("updatedAtUtc")
    report_state = str(report.get("reportState", "unknown")).strip().lower()
    rows: list[dict[str, Any]] = []
    for node_id in sorted(node_ids):
        newest = newest_by_node.get(node_id)
        frame = newest[1] if newest is not None else None
        rows.append(
            {
                "nodeId": node_id,
                "reportAvailable": True,
                "reportSchemaSupported": schema_supported,
                "reportState": report_state,
                "reportUpdatedAtUtc": report_updated if isinstance(report_updated, str) else None,
                "lastProcessedFrameTimestampUtc": (
                    newest[0].isoformat(timespec="milliseconds").replace("+00:00", "Z")
                    if newest is not None
                    else None
                ),
                "processedFrameAvailable": newest is not None,
                "debugCleanCaptureProvenance": (
                    _frame_is_debug_clean(frame) if frame is not None else False
                ),
                "checkpointIntegrityVerified": checkpoint_verified,
                "detectorCandidateRequiredForHealth": False,
            }
        )
    return rows


def load_rgb_sensor_health_reports(
    report_paths: Iterable[Path | None],
) -> list[dict[str, Any]]:
    """Merge OAK watcher health rows, retaining the newest row per node."""

    selected: dict[str, tuple[tuple[datetime, bool, bool, bool], dict[str, Any]]] = {}
    for report_path in report_paths:
        for row in load_rgb_sensor_health(report_path):
            node_id = str(row.get("nodeId", ""))
            timestamps: list[datetime] = []
            for field in ("lastProcessedFrameTimestampUtc", "reportUpdatedAtUtc"):
                try:
                    timestamps.append(_parse_timestamp(row.get(field)))
                except (TypeError, ValueError):
                    continue
            newest = max(timestamps, default=datetime.min.replace(tzinfo=timezone.utc))
            rank = (
                newest,
                row.get("reportState") == "running",
                row.get("checkpointIntegrityVerified") is True,
                row.get("debugCleanCaptureProvenance") is True,
            )
            existing = selected.get(node_id)
            if existing is None or rank > existing[0]:
                selected[node_id] = (rank, row)
    return [selected[node_id][1] for node_id in sorted(selected)]


def load_rgb_evidence(report_path: Path | None) -> list[dict[str, Any]]:
    report = _read_json(report_path) if report_path else None
    if not report:
        return []
    rows: list[dict[str, Any]] = []
    frames = report.get("processedFrames")
    if not isinstance(frames, list):
        return rows
    for frame in frames[-256:]:
        if not isinstance(frame, Mapping):
            continue
        metadata = _load_metadata(frame.get("metadataPath"))
        if not metadata or metadata.get("debugVisualsExcludedFromSensorCapture") is not True:
            continue
        node_id = frame.get("nodeId")
        timestamp = frame.get("timestampUtc")
        detections = frame.get("detections")
        if not isinstance(node_id, str) or not isinstance(timestamp, str) or not isinstance(detections, list):
            continue
        for index, detection in enumerate(detections):
            if not isinstance(detection, Mapping):
                continue
            bbox = _box(detection.get("bboxXyxyPixels"))
            association = _associate_box(bbox, metadata) if bbox else None
            if not association:
                continue
            target_id, overlap = association
            rows.append(
                {
                    "evidenceId": f"rgb:{node_id}:{frame.get('frameId', 'frame')}:{index}",
                    "targetId": target_id,
                    "nodeId": node_id,
                    "timestampUtc": timestamp,
                    "score": min(1.0, max(0.0, _number(detection.get("confidence")))),
                    "rangeMeters": _number(detection.get("slantRangeMeters"), math.nan),
                    "associationIoU": overlap,
                    "associationSemantics": "unreal_projection_iou_debug_association",
                    "associationProvenance": "evaluator_truth_projection_debug_only",
                    "fusionEligible": False,
                    "fusionExclusionReason": (
                        "track association used evaluator-truth projection IoU; an estimated-track "
                        "or calibrated operational association has not been established"
                    ),
                    "modelId": "oak_rgb_yolo:14dd5b34c4a7",
                    "proxyInput": False,
                    "frameId": str(frame.get("frameId", "unknown")),
                    "frameFileName": Path(str(frame.get("rgbPath", ""))).name or None,
                    "bboxXyxyPixels": [round(value, 3) for value in bbox],
                    "className": str(detection.get("className", "drone candidate")),
                    "visualSemantics": "model_detector_bounding_box_on_debug-clean_sensor_frame",
                    "debugVisualsExcludedFromSensorCapture": True,
                }
            )
    return rows


def load_rgb_evidence_reports(
    report_paths: Iterable[Path | None],
) -> list[dict[str, Any]]:
    """Merge per-node OAK watcher reports without duplicating evidence IDs."""

    rows: list[dict[str, Any]] = []
    seen: set[str] = set()
    for report_path in report_paths:
        for row in load_rgb_evidence(report_path):
            evidence_id = str(row.get("evidenceId", ""))
            if not evidence_id or evidence_id in seen:
                continue
            seen.add(evidence_id)
            rows.append(row)
    return rows


_EVENT_FRAME_SUFFIX = re.compile(r"_rgb$")


def load_event_evidence(report_path: Path | None) -> list[dict[str, Any]]:
    report = _read_json(report_path) if report_path else None
    if not report:
        return []
    event = report.get("event_camera")
    if not isinstance(event, Mapping):
        return []
    frames_root = event.get("frames_root")
    samples = event.get("observation_samples")
    if not isinstance(frames_root, str) or not isinstance(samples, list):
        return []
    rows: list[dict[str, Any]] = []
    for index, sample in enumerate(samples[-512:]):
        if not isinstance(sample, Mapping):
            continue
        measurements = sample.get("measurements")
        node_id = sample.get("node_id")
        timestamp = sample.get("timestamp")
        if not isinstance(measurements, Mapping) or not isinstance(node_id, str) or not isinstance(timestamp, str):
            continue
        frame_id = measurements.get("frame_id")
        if not isinstance(frame_id, str):
            continue
        base = _EVENT_FRAME_SUFFIX.sub("", frame_id)
        metadata_path = Path(frames_root) / node_id / f"{base}_depth.json"
        metadata = _read_json(metadata_path)
        if not metadata or metadata.get("debugVisualsExcludedFromSensorCapture") is not True:
            continue
        width = _number(measurements.get("frame_width"), 640.0)
        height = _number(measurements.get("frame_height"), 360.0)
        bbox = _box(
            measurements.get("bbox_xyxy_normalized"),
            normalized=True,
            width=width,
            height=height,
        )
        association = _associate_box(bbox, metadata) if bbox else None
        if not association:
            continue
        target_id, overlap = association
        source_encoding = measurements.get("source_encoding", event.get("source_encoding"))
        preprocessing_equivalent = (
            measurements.get(
                "preprocessing_equivalent_to_training",
                event.get("preprocessing_equivalent_to_training"),
            )
            is True
        )
        physical_event_input = (
            measurements.get(
                "physical_neuromorphic_sensor_data",
                event.get("physical_neuromorphic_sensor_data"),
            )
            is True
        )
        proxy_input = source_encoding == PROXY_EVENT_ENCODING
        sensor_health_eligible = (
            physical_event_input and preprocessing_equivalent and not proxy_input
        )
        # Target association below is evaluator-truth projection IoU.  Even a
        # future physical/native event input may prove pipeline health, but it
        # cannot contribute to operational fusion until association is based
        # on an estimated track or a calibrated operational method.
        fusion_eligible = False
        if proxy_input:
            fusion_exclusion_reason = (
                "RGB-derived proxy candidate is correlated with RGB and cannot count as a "
                "separate corroborating family"
            )
        elif not physical_event_input:
            fusion_exclusion_reason = (
                "physical neuromorphic input provenance was not explicitly established"
            )
        elif not preprocessing_equivalent:
            fusion_exclusion_reason = (
                "training-equivalent event preprocessing was not explicitly established"
            )
        else:
            fusion_exclusion_reason = (
                "track association used evaluator-truth projection IoU; an estimated-track "
                "or calibrated operational association has not been established"
            )
        rows.append(
            {
                "evidenceId": f"event:{node_id}:{base}:{index}",
                "targetId": target_id,
                "nodeId": node_id,
                "timestampUtc": timestamp,
                "score": min(1.0, max(0.0, _number(sample.get("detection_probability")))),
                "rangeMeters": math.nan,
                "associationIoU": overlap,
                "associationSemantics": "unreal_projection_iou_debug_association",
                "associationProvenance": "evaluator_truth_projection_debug_only",
                "modelId": str(sample.get("model_id", "fred_event_yolo26s_p2")),
                "sourceEncoding": str(source_encoding) if source_encoding is not None else None,
                "proxyInput": proxy_input,
                "physicalNeuromorphicSensorData": physical_event_input,
                "trainingPreprocessingEquivalent": preprocessing_equivalent,
                "sensorHealthEligible": sensor_health_eligible,
                "fusionEligible": fusion_eligible,
                "fusionExclusionReason": fusion_exclusion_reason,
                "frameId": base,
                "frameFileName": f"{base}_rgb.png",
                "bboxXyxyPixels": [round(value, 3) for value in bbox],
                "className": str(measurements.get("class_name", "event candidate")),
                "visualSemantics": (
                    "model_detector_bounding_box_on_debug-clean_proxy-event-frame"
                    if proxy_input
                    else "model_detector_bounding_box_on_debug-clean-native-event-frame"
                ),
                "debugVisualsExcludedFromSensorCapture": True,
            }
        )
    return rows


def _closest_visual_rows(
    rows: Iterable[Mapping[str, Any]], target_id: str, as_of: datetime, maximum_age_s: float
) -> list[Mapping[str, Any]]:
    selected: list[Mapping[str, Any]] = []
    for row in rows:
        if row.get("targetId") != target_id:
            continue
        try:
            timestamp = _parse_timestamp(row.get("timestampUtc"))
        except (ValueError, TypeError):
            continue
        age = (as_of - timestamp).total_seconds()
        if -0.1 <= age <= maximum_age_s:
            selected.append(row)
    return selected


def _newest_health_timestamp(
    rows: Iterable[Mapping[str, Any]],
    fields: Sequence[str],
    as_of: datetime,
) -> tuple[str | None, float | None]:
    timestamps: list[datetime] = []
    for row in rows:
        for field in fields:
            try:
                timestamps.append(_parse_timestamp(row.get(field)))
            except (TypeError, ValueError):
                continue
    if not timestamps:
        return None, None
    newest = max(timestamps)
    age = max(0.0, (as_of - newest).total_seconds())
    return newest.isoformat(timespec="milliseconds").replace("+00:00", "Z"), round(age, 3)


def _rgb_sensor_health(
    node_id: str,
    *,
    as_of: datetime,
    maximum_age_s: float,
    health_rows: Iterable[Mapping[str, Any]],
    evidence_rows: Iterable[Mapping[str, Any]],
) -> dict[str, Any]:
    node_health = [row for row in health_rows if row.get("nodeId") == node_id]
    node_evidence = [row for row in evidence_rows if row.get("nodeId") == node_id]
    online_health: list[Mapping[str, Any]] = []
    recent_health: list[Mapping[str, Any]] = []
    for row in node_health:
        heartbeat_fresh = _timestamp_is_fresh(
            row.get("reportUpdatedAtUtc"),
            as_of,
            maximum_age_s,
            future_tolerance_s=maximum_age_s,
        )
        frame_fresh = _timestamp_is_fresh(
            row.get("lastProcessedFrameTimestampUtc"),
            as_of,
            maximum_age_s,
            future_tolerance_s=maximum_age_s,
        )
        if heartbeat_fresh or frame_fresh:
            recent_health.append(row)
        if (
            heartbeat_fresh
            and frame_fresh
            and row.get("reportSchemaSupported") is True
            and row.get("reportState") == "running"
            and row.get("checkpointIntegrityVerified") is True
            and row.get("debugCleanCaptureProvenance") is True
        ):
            online_health.append(row)

    fresh_evidence = [
        row
        for row in node_evidence
        if _timestamp_is_fresh(row.get("timestampUtc"), as_of, maximum_age_s)
    ]
    timestamp, age = _newest_health_timestamp(
        online_health or recent_health or fresh_evidence or node_health or node_evidence,
        (
            "lastProcessedFrameTimestampUtc",
            "reportUpdatedAtUtc",
            "timestampUtc",
        ),
        as_of,
    )
    if online_health:
        return {
            "status": "ONLINE",
            "healthSource": "fresh_oak_watcher_heartbeat_and_processed_frame",
            "healthTimestampUtc": timestamp,
            "healthAgeSeconds": age,
            "validationState": (
                "LIVE SIMULATION RGB PIPELINE HEALTH: active OAK watcher heartbeat and "
                "processed frame are fresh, the trusted checkpoint hash is verified, and "
                "debug-clean capture provenance is explicit; ONLINE is runtime health, not "
                "field acceptance or physical range validation"
            ),
            "healthMethod": (
                "fail-closed per-node OAK watcher heartbeat + processed-frame freshness + "
                "checkpoint-integrity + clean-capture provenance; detections are not required"
            ),
        }
    if recent_health or fresh_evidence:
        return {
            "status": "DEGRADED",
            "healthSource": (
                "incomplete_or_inactive_oak_watcher_health"
                if recent_health
                else "fresh_rgb_evidence_without_complete_watcher_health"
            ),
            "healthTimestampUtc": timestamp,
            "healthAgeSeconds": age,
            "validationState": (
                "DEGRADED SIMULATION RGB PIPELINE: recent OAK report/evidence exists but one "
                "or more ONLINE requirements (active watcher, fresh heartbeat and frame, "
                "verified checkpoint, debug-clean provenance) is absent; field/physical range "
                "remains unvalidated"
            ),
            "healthMethod": (
                "fail-closed per-node OAK health; Unreal node freshness and detector-candidate "
                "presence alone cannot set RGB ONLINE"
            ),
        }
    return {
        "status": "OFFLINE",
        "healthSource": "no_fresh_oak_watcher_health",
        "healthTimestampUtc": timestamp,
        "healthAgeSeconds": age,
        "validationState": (
            "OFFLINE: no fresh per-node OAK watcher health; OFFLINE ACCEPTANCE EVIDENCE ONLY "
            "exists historically for City/South and does not indicate live RGB status or "
            "field/physical range"
        ),
        "healthMethod": (
            "fail-closed per-node OAK health; Unreal node status is intentionally not inherited"
        ),
    }


def _event_sensor_health(
    node_id: str,
    *,
    as_of: datetime,
    maximum_age_s: float,
    evidence_rows: Iterable[Mapping[str, Any]],
) -> dict[str, Any]:
    node_rows = [row for row in evidence_rows if row.get("nodeId") == node_id]
    fresh_rows = [
        row
        for row in node_rows
        if _timestamp_is_fresh(row.get("timestampUtc"), as_of, maximum_age_s)
    ]
    native_rows = [
        row
        for row in fresh_rows
        if row.get("physicalNeuromorphicSensorData") is True
        and row.get("trainingPreprocessingEquivalent") is True
        and row.get("proxyInput") is not True
        and row.get("sensorHealthEligible") is True
    ]
    timestamp, age = _newest_health_timestamp(
        native_rows or fresh_rows or node_rows,
        ("timestampUtc",),
        as_of,
    )
    if native_rows:
        return {
            "status": "ONLINE",
            "healthSource": "fresh_native_event_evidence_with_explicit_provenance",
            "healthTimestampUtc": timestamp,
            "healthAgeSeconds": age,
            "validationState": (
                "LIVE NATIVE EVENT INPUT DECLARED: fresh physical neuromorphic provenance and "
                "training-equivalent preprocessing are explicit; runtime ONLINE is not field-range validation"
            ),
            "healthMethod": (
                "fail-closed per-node native-event provenance and freshness gate"
            ),
        }
    if fresh_rows:
        proxy_present = any(row.get("proxyInput") is True for row in fresh_rows)
        return {
            "status": "DEGRADED",
            "healthSource": (
                "fresh_rgb_difference_proxy_event_candidate"
                if proxy_present
                else "fresh_event_candidate_without_validated_native_provenance"
            ),
            "healthTimestampUtc": timestamp,
            "healthAgeSeconds": age,
            "validationState": (
                "DEGRADED DIAGNOSTIC RGB-DIFFERENCE PROXY ONLY: not validated physical "
                "neuromorphic sensing and cannot report the event sensor ONLINE"
                if proxy_present
                else "DEGRADED: fresh event candidate lacks explicit physical neuromorphic and "
                "training-equivalent provenance; it cannot report the event sensor ONLINE"
            ),
            "healthMethod": (
                "fail-closed per-node event provenance; RGB-derived proxy activity is never "
                "treated as ONLINE physical neuromorphic sensing"
            ),
        }
    return {
        "status": "OFFLINE",
        "healthSource": "no_fresh_validated_native_event_health",
        "healthTimestampUtc": timestamp,
        "healthAgeSeconds": age,
        "validationState": (
            "OFFLINE: no fresh validated physical neuromorphic report; configured RGB-difference "
            "proxy capability and historical candidates do not establish live event sensing"
        ),
        "healthMethod": (
            "fail-closed per-node event provenance; Unreal node status is intentionally not inherited"
        ),
    }


def _no_evidence_fusion(target_id: str, as_of: datetime) -> dict[str, Any]:
    return {
        "schemaVersion": "3.0",
        "method": FUSION_V3_METHOD,
        "targetTrackId": target_id,
        "asOfUtc": as_of.isoformat(timespec="milliseconds").replace("+00:00", "Z"),
        "decision": "NO_CURRENT_EVIDENCE",
        "detectionAlert": False,
        "preliminaryCue": False,
        "operatorCueActive": False,
        "confirmationTier": "UNCONFIRMED",
        "fusedEvidenceScore": 0.0,
        "activeModalities": [],
        "activeModalityFamilyCount": 0,
        "decisionLatencyMilliseconds": None,
        "contributions": [],
        "independenceAssumedForNumericScore": False,
        "corroboratingScoresAddedOrMultiplied": False,
        "detectionOnly": True,
        "engagementLogicPresent": False,
    }


def build_layered_snapshot(
    unreal_snapshot: Mapping[str, Any],
    *,
    rgb_rows: Iterable[Mapping[str, Any]] = (),
    rgb_health_rows: Iterable[Mapping[str, Any]] = (),
    event_rows: Iterable[Mapping[str, Any]] = (),
    visual_max_age_s: float = 2.0,
) -> dict[str, Any]:
    if unreal_snapshot.get("schemaVersion") not in INPUT_SCHEMAS:
        raise ValueError(f"unsupported Unreal snapshot schema: {unreal_snapshot.get('schemaVersion')!r}")
    if unreal_snapshot.get("sampleComplete") is False:
        raise ValueError("Unreal snapshot is incomplete")
    if unreal_snapshot.get("schemaVersion") == "triad.live_rf_snapshot.v3":
        if unreal_snapshot.get("sampleComplete") is not True:
            raise ValueError("Unreal v3 snapshot must declare sampleComplete=true")
        if (
            unreal_snapshot.get("detectionOnly") is not True
            or unreal_snapshot.get("actionsTaken") != "none"
        ):
            raise ValueError("Unreal v3 snapshot safety semantics are invalid")
    as_of = _parse_timestamp(unreal_snapshot.get("timestampUtc"))
    timestamp_s = _number(unreal_snapshot.get("simulationSeconds"), as_of.timestamp())
    weather_name, rain_rate, visibility_m = _weather_id(unreal_snapshot)
    nodes = [item for item in unreal_snapshot.get("sensorNodes", []) if isinstance(item, Mapping) and item.get("enabled") is not False and item.get("spawned") is not False]
    targets = [item for item in unreal_snapshot.get("scenarioTargets", []) if isinstance(item, Mapping)]
    nodes_by_id = {
        str(item.get("nodeId")): item
        for item in nodes
        if _nonempty_text(item.get("nodeId")) is not None
    }
    scenario_by_target_id = {
        str(item.get("targetActor")): item
        for item in targets
        if _nonempty_text(item.get("targetActor")) is not None
    }
    rf_items, rf_rejections = _normalise_detected_rf_links(
        unreal_snapshot,
        as_of=as_of,
        nodes_by_id=nodes_by_id,
        maximum_age_s=visual_max_age_s,
    )
    if unreal_snapshot.get("schemaVersion") == "triad.live_rf_snapshot.v3":
        search_radar_items, search_radar_rejections = _normalise_search_radar_detections(
            unreal_snapshot,
            as_of=as_of,
            nodes_by_id=nodes_by_id,
            maximum_age_s=visual_max_age_s,
        )
        ptz_items, ptz_rejections = _normalise_ptz_confirmations(
            unreal_snapshot,
            as_of=as_of,
            nodes_by_id=nodes_by_id,
            accepted_radar=search_radar_items,
            maximum_age_s=visual_max_age_s,
        )
    else:
        search_radar_items, search_radar_rejections = [], []
        ptz_items, ptz_rejections = [], []
    rgb_items = tuple(rgb_rows)
    rgb_health_items = tuple(rgb_health_rows)
    event_items = tuple(event_rows)
    model_contact_ids = {
        target_id
        for row in (*rgb_items, *event_items)
        if row.get("fusionEligible") is True
        and (target_id := _nonempty_text(row.get("targetId"))) is not None
        and _timestamp_is_fresh(row.get("timestampUtc"), as_of, visual_max_age_s)
    }
    contact_ids = sorted(
        {
            str(item["targetActor"])
            for item in (*rf_items, *search_radar_items, *ptz_items)
        }
        | model_contact_ids
    )

    rf_links: list[dict[str, Any]] = list(rf_items)
    mmwave_links: list[dict[str, Any]] = []
    output_tracks: list[dict[str, Any]] = []
    sensor_records: list[dict[str, Any]] = []
    enriched_search_radar: list[dict[str, Any]] = []
    enriched_ptz_confirmations: list[dict[str, Any]] = []
    for node in nodes:
        node_id = str(node.get("nodeId", "unknown"))
        unreal_status = _normalise_runtime_status(node.get("runtimeStatus"))
        rf_range_m, rf_range_source = _node_rf_range(node)
        common = {
            "nodeId": node_id,
            "latitudeDegrees": _number(node.get("latitudeDegrees")),
            "longitudeDegrees": _number(node.get("longitudeDegrees")),
            "heightMeters": _number(node.get("heightMeters")),
        }
        sensor_records.append(
            {
                **common,
                "sensorId": f"triad:{node_id}:rf-wideband",
                "modality": "wideband_rf",
                "band": "0.169-6.2 GHz configurable channel bank",
                "rangeMeters": rf_range_m,
                "rangeSource": rf_range_source,
                "status": unreal_status,
                "method": (
                    "current Unreal analytic receive-only RF link detections, validated "
                    f"against the per-node detectionRangeMeters envelope ({rf_range_m:g} m); "
                    "not field-calibrated range"
                ),
                "validationState": (
                    "SIMULATION RF RUNTIME HEALTH follows this Unreal node runtimeStatus; "
                    "configured range and analytic detection are not field validation"
                ),
            }
        )
        sensor_records.append(
            {
                **common,
                "sensorId": f"triad:{node_id}:mmwave",
                "modality": "mmwave",
                "band": "76-81 GHz FMCW",
                "rangeMeters": 500.0,
                "status": "OFFLINE",
                "method": (
                    "no current Unreal mmWave observation array is available; truth-driven "
                    "radar-equation regeneration is disabled and mmWave is excluded from fusion"
                ),
                "validationState": (
                    "NO OBSERVATION-AUTHORITATIVE MMWAVE STREAM; registered for architecture "
                    "visibility only and never used to seed or corroborate a contact"
                ),
            }
        )
        if unreal_snapshot.get("schemaVersion") == "triad.live_rf_snapshot.v3":
            sensor_records.extend(
                _long_range_sensor_records(
                    node,
                    common=common,
                    radar_rows=search_radar_items,
                    ptz_rows=ptz_items,
                )
            )
        if node.get("cameraCaptureConfigured") is True:
            rgb_health = _rgb_sensor_health(
                node_id,
                as_of=as_of,
                maximum_age_s=visual_max_age_s,
                health_rows=rgb_health_items,
                evidence_rows=rgb_items,
            )
            event_health = _event_sensor_health(
                node_id,
                as_of=as_of,
                maximum_age_s=visual_max_age_s,
                evidence_rows=event_items,
            )
            sensor_records.extend(
                (
                    {
                        **common,
                        "sensorId": f"triad:{node_id}:rgb",
                        "modality": "rgb",
                        "band": "visible RGB + raw uint32-mm simulation depth",
                        "rangeMeters": 0.0,
                        "status": rgb_health["status"],
                        "method": (
                            "OAK one-class RGB detector with post-detection raw uint32-mm "
                            "simulation depth; offline clean acceptance: City 48/60 frames, "
                            "South 37/60 frames, observed simulation slant 15.8-339.0 m; "
                            "field/physical range unvalidated; live health is derived from the "
                            "per-node OAK watcher, never inherited from Unreal node status"
                        ),
                        **{key: value for key, value in rgb_health.items() if key != "status"},
                    },
                    {
                        **common,
                        "sensorId": f"triad:{node_id}:event",
                        "modality": "event_camera",
                        "band": "RGB-derived proxy event stack (non-physical)",
                        "rangeMeters": 0.0,
                        "status": event_health["status"],
                        "method": (
                            "FRED event detector; the current RGB-derived proxy (RGB-difference) "
                            "stack is non-physical and not training-equivalent, offline candidates "
                            "inconclusive, and native event-camera range unvalidated; live "
                            "health requires fresh native physical provenance and is never "
                            "inherited from Unreal node status"
                        ),
                        **{key: value for key, value in event_health.items() if key != "status"},
                    },
                )
            )

    for target_id in contact_ids:
        target = scenario_by_target_id.get(target_id, {})
        evidence: list[LayeredEvidence] = []
        target_rf = [item for item in rf_items if item.get("targetActor") == target_id]
        target_mmwave: list[dict[str, Any]] = []
        target_search_radar = [
            item for item in search_radar_items if item.get("targetActor") == target_id
        ]
        target_ptz = [item for item in ptz_items if item.get("targetActor") == target_id]
        target_eo_ptz = [
            item for item in target_ptz if item.get("sensorType") == "EO_PTZ"
        ]
        target_thermal_ptz = [
            item for item in target_ptz if item.get("sensorType") == "THERMAL_PTZ"
        ]

        for row in target_rf:
            evidence.append(
                LayeredEvidence(
                    evidence_id=str(row["evidenceId"]),
                    target_track_id=target_id,
                    node_id=str(row["nodeId"]),
                    modality=LayeredModality.WIDEBAND_RF,
                    timestamp=_parse_timestamp(row["timestampUtc"]),
                    raw_score=_number(row.get("score")),
                    source_reliability=0.82,
                    weather_multiplier=1.0,
                    latency_ms=20.0,
                    range_m=_number(row.get("rangeMeters")),
                    correlation_group=f"rf:{row['nodeId']}",
                    score_origin=str(row.get("method")),
                )
            )

        for row in target_search_radar:
            evidence.append(
                LayeredEvidence(
                    evidence_id=str(row["evidenceId"]),
                    target_track_id=target_id,
                    node_id=str(row["nodeId"]),
                    modality=LayeredModality.SEARCH_RADAR,
                    timestamp=_parse_timestamp(row["timestampUtc"]),
                    raw_score=_number(row.get("confidence")),
                    source_reliability=0.94,
                    weather_multiplier=1.0,
                    latency_ms=35.0,
                    range_m=_number(row.get("rangeMeters")),
                    correlation_group=f"search_radar:{row['sensorId']}",
                    score_origin=(
                        "Unreal deterministic simulated search-radar evidence index; "
                        "not learned-model output"
                    ),
                )
            )
        for row in target_eo_ptz:
            evidence.append(
                LayeredEvidence(
                    evidence_id=str(row["evidenceId"]),
                    target_track_id=target_id,
                    node_id=str(row["nodeId"]),
                    modality=LayeredModality.EO_PTZ,
                    timestamp=_parse_timestamp(row["timestampUtc"]),
                    raw_score=_number(row.get("confidence")),
                    source_reliability=0.90,
                    weather_multiplier=1.0,
                    latency_ms=max(0.0, _number(row.get("cueAgeSeconds"))) * 1_000.0,
                    range_m=_number(row.get("rangeMeters")),
                    correlation_group=f"eo_ptz:{row['sensorId']}:{row['trackId']}",
                    score_origin=(
                        "radar-cued deterministic simulated EO confirmation whose raw "
                        "confidence already contains Unreal weatherConfidenceFactor; one "
                        "record covers its projection box and radar-cued range"
                    ),
                )
            )
        for row in target_thermal_ptz:
            evidence.append(
                LayeredEvidence(
                    evidence_id=str(row["evidenceId"]),
                    target_track_id=target_id,
                    node_id=str(row["nodeId"]),
                    modality=LayeredModality.THERMAL_PTZ,
                    timestamp=_parse_timestamp(row["timestampUtc"]),
                    raw_score=_number(row.get("confidence")),
                    source_reliability=0.88,
                    weather_multiplier=1.0,
                    latency_ms=max(0.0, _number(row.get("cueAgeSeconds"))) * 1_000.0,
                    range_m=_number(row.get("rangeMeters")),
                    correlation_group=f"thermal_ptz:{row['sensorId']}:{row['trackId']}",
                    score_origin=(
                        "radar-cued deterministic synthetic thermal confirmation whose raw "
                        "confidence already contains Unreal weatherConfidenceFactor; one "
                        "record covers its projection box and radar-cued range"
                    ),
                )
            )

        selected_rgb = _closest_visual_rows(rgb_items, target_id, as_of, visual_max_age_s)
        selected_event = _closest_visual_rows(event_items, target_id, as_of, visual_max_age_s)
        fusion_rgb = [row for row in selected_rgb if row.get("fusionEligible") is True]
        fusion_event = [
            row for row in selected_event if row.get("fusionEligible") is True
        ]
        for row in fusion_rgb:
            range_value = _number(row.get("rangeMeters"), math.nan)
            evidence.append(
                LayeredEvidence(
                    evidence_id=str(row["evidenceId"]),
                    target_track_id=target_id,
                    node_id=str(row["nodeId"]),
                    modality=LayeredModality.RGB,
                    timestamp=_parse_timestamp(row["timestampUtc"]),
                    raw_score=_number(row.get("score")),
                    source_reliability=0.90,
                    weather_multiplier=_visual_weather_multiplier(visibility_m, rain_rate, event=False),
                    latency_ms=95.0,
                    range_m=range_value if math.isfinite(range_value) and range_value >= 0.0 else None,
                    correlation_group=f"rgb:{row['nodeId']}:{row['timestampUtc']}",
                    score_origin=str(row.get("modelId", "oak_rgb_yolo")),
                )
            )
        for row in fusion_event:
            evidence.append(
                LayeredEvidence(
                    evidence_id=str(row["evidenceId"]),
                    target_track_id=target_id,
                    node_id=str(row["nodeId"]),
                    modality=LayeredModality.EVENT_CAMERA,
                    timestamp=_parse_timestamp(row["timestampUtc"]),
                    raw_score=_number(row.get("score")),
                    source_reliability=0.84,
                    weather_multiplier=_visual_weather_multiplier(visibility_m, rain_rate, event=True),
                    latency_ms=45.0,
                    range_m=None,
                    correlation_group=f"event:{row['nodeId']}:{row['timestampUtc']}",
                    score_origin=str(row.get("modelId", "fred_event_yolo")),
                )
            )

        fusion = fuse_layered_evidence(evidence, as_of=as_of)
        rf_node_count = len({str(item.get("nodeId")) for item in target_rf})
        track_estimate = _estimate_track_state(
            target_id=target_id,
            rf_node_count=rf_node_count,
            mmwave_detected=bool(target_mmwave),
            rgb_detected=bool(fusion_rgb),
            event_detected=bool(fusion_event),
            search_radar_rows=target_search_radar,
            nodes_by_id=nodes_by_id,
            eo_ptz_detected=bool(target_eo_ptz),
            thermal_ptz_detected=bool(target_thermal_ptz),
        )
        estimated_position = track_estimate["position"]
        assert estimated_position is None or isinstance(estimated_position, Geodetic)
        estimated_speed = _optional_number(track_estimate["speedMetersPerSecond"])
        estimated_heading = _optional_number(track_estimate["headingDegrees"])
        perimeter_state = _estimated_perimeter_state(
            estimated_position,
            estimated_speed,
            estimated_heading,
            unreal_snapshot.get("simulationPerimeter"),
        )
        truth = target.get("hostileScenarioTruth")
        operator_cue = bool(fusion.get("operatorCueActive"))
        outside_state = perimeter_state["outside"]
        outside = outside_state is True
        airspace_state = str(perimeter_state["state"])
        if operator_cue and outside and airspace_state == "APPROACHING":
            cue_category = (
                "APPROACH_EARLY_WARNING"
                if fusion.get("preliminaryCue")
                else "APPROACH_CORROBORATED_DETECTION"
            )
        elif fusion.get("detectionAlert"):
            cue_category = "CORROBORATED_DETECTION"
        elif fusion.get("preliminaryCue"):
            cue_category = "PRELIMINARY_RF_CUE"
        else:
            cue_category = "NO_OPERATOR_CUE"
        approach_projection = {
            "airspaceState": airspace_state,
            "approachState": airspace_state,
            "distanceToPerimeterMeters": round(float(perimeter_state["distanceMeters"]), 3)
            if perimeter_state["distanceMeters"] is not None
            else None,
            "approachRateMetersPerSecond": round(
                float(perimeter_state["approachRateMetersPerSecond"]), 3
            )
            if perimeter_state["approachRateMetersPerSecond"] is not None
            else None,
            "timeToSimulationPerimeterSeconds": round(
                float(perimeter_state["timeToPerimeterSeconds"]), 3
            )
            if perimeter_state["timeToPerimeterSeconds"] is not None
            else None,
            "outsideSimulationPerimeter": outside_state,
            "estimatedTargetLatitudeDegrees": (
                estimated_position.latitude_deg if estimated_position is not None else None
            ),
            "estimatedTargetLongitudeDegrees": (
                estimated_position.longitude_deg if estimated_position is not None else None
            ),
            "estimatedTargetHeightMeters": (
                estimated_position.altitude_m if estimated_position is not None else None
            ),
        }
        track_search_radar = [
            {**item, **approach_projection} for item in target_search_radar
        ]
        track_ptz_confirmations = [
            {**item, **approach_projection} for item in target_ptz
        ]
        enriched_search_radar.extend(track_search_radar)
        enriched_ptz_confirmations.extend(track_ptz_confirmations)
        active_long_range_modalities = [
            modality
            for modality, rows in (
                ("SEARCH_RADAR", target_search_radar),
                ("EO_PTZ", target_eo_ptz),
                ("THERMAL_PTZ", target_thermal_ptz),
            )
            if rows
        ]
        active_modalities = (
            (["WIDEBAND_RF"] if target_rf else [])
            + active_long_range_modalities
            + (["RGB_MODEL"] if fusion_rgb else [])
            + (["EVENT_MODEL"] if fusion_event else [])
        )

        def _ptz_modality_evidence(
            sensor_type: str,
            rows: Sequence[Mapping[str, Any]],
        ) -> dict[str, Any]:
            best = max(
                rows,
                key=lambda item: _number(item.get("confidence"), -math.inf),
                default={},
            )
            return {
                "modality": sensor_type,
                "family": "VISUAL",
                "fusionFamily": "VISUAL",
                "evidenceFamily": sensor_type,
                "detected": bool(rows),
                "sensorIds": sorted({str(item.get("sensorId")) for item in rows}),
                "nodeCount": len({str(item.get("nodeId")) for item in rows}),
                "score": _optional_number(best.get("confidence")),
                "method": (
                    "SIMULATED_SENSOR_CONFIRMATION from radar-cued Unreal projection; "
                    "not learned-model output"
                ),
                "modelId": None,
                "rangeM": _optional_number(best.get("rangeMeters")),
                "latencyMs": (
                    round(_number(best.get("cueAgeSeconds")) * 1_000.0, 3)
                    if best
                    else None
                ),
                "simulated": True,
                "fieldValidated": False,
            }

        best_search_radar = max(
            target_search_radar,
            key=lambda item: _number(item.get("confidence"), -math.inf),
            default={},
        )
        long_range_modality_evidence = (
            [
                {
                    "modality": "WIDEBAND_RF",
                    "family": "PASSIVE_RF",
                    "fusionFamily": "PASSIVE_RF",
                    "evidenceFamily": "WIDEBAND_RF",
                    "detected": True,
                    "sensorIds": sorted(
                        {str(item.get("receiverId")) for item in target_rf}
                    ),
                    "nodeCount": rf_node_count,
                    "rangeM": min(
                        _number(item.get("rangeMeters")) for item in target_rf
                    ),
                    "score": max(_number(item.get("score")) for item in target_rf),
                    "method": "validated current Unreal analytic RF observations",
                    "modelId": None,
                    "simulated": True,
                    "fieldValidated": False,
                }
            ]
            if target_rf
            else []
        ) + [
            {
                "modality": "SEARCH_RADAR",
                "family": "ACTIVE_RADAR",
                "fusionFamily": "ACTIVE_RADAR",
                "evidenceFamily": "SEARCH_RADAR",
                "detected": bool(target_search_radar),
                "sensorIds": sorted(
                    {str(item.get("sensorId")) for item in target_search_radar}
                ),
                "nodeCount": len(
                    {str(item.get("nodeId")) for item in target_search_radar}
                ),
                "rangeM": _optional_number(best_search_radar.get("rangeMeters")),
                "radialSpeedMps": _optional_number(
                    best_search_radar.get("radialVelocityMetersPerSecond")
                ),
                "closingSpeedMps": _optional_number(
                    best_search_radar.get("closingSpeedMetersPerSecond")
                ),
                "score": _optional_number(best_search_radar.get("confidence")),
                "method": (
                    "Unreal deterministic simulated search-radar measurement; not a "
                    "calibrated probability or field result"
                ),
                "simulated": True,
                "fieldValidated": False,
            },
            _ptz_modality_evidence("EO_PTZ", target_eo_ptz),
            _ptz_modality_evidence("THERMAL_PTZ", target_thermal_ptz),
        ]
        visual_confirmations = [
            {
                key: item.get(key)
                for key in (
                    "id",
                    "modality",
                    "label",
                    "simulated",
                    "timestampUtc",
                    "nodeId",
                    "sensorId",
                    "relativePath",
                    "frameRelativePath",
                    "metadataRelativePath",
                    "width",
                    "height",
                    "reticle",
                    "boxes",
                    "confidence",
                    "lineOfSight",
                    "blockingActor",
                    "hasFrame",
                    "weatherProfile",
                    "weatherVisibilityMeters",
                    "weatherRainRateMillimetersPerHour",
                    "weatherConfidenceFactor",
                    "weatherConfidenceSemantics",
                    "occlusionSemantics",
                    "rangeM",
                    "rangeSemantics",
                    "azimuthDeg",
                    "elevationDeg",
                    "horizontalFovDeg",
                    "approachState",
                    "distanceToPerimeterMeters",
                )
            }
            for item in track_ptz_confirmations[:4]
        ]
        output_tracks.append(
            {
                "trackId": target_id,
                "targetType": _observed_target_type(fusion_rgb, fusion_event),
                "positionAvailable": estimated_position is not None,
                "latitudeDegrees": (
                    estimated_position.latitude_deg if estimated_position is not None else None
                ),
                "longitudeDegrees": (
                    estimated_position.longitude_deg if estimated_position is not None else None
                ),
                "heightMeters": (
                    estimated_position.altitude_m if estimated_position is not None else None
                ),
                "speedMetersPerSecond": estimated_speed,
                "headingDegrees": estimated_heading,
                "airspaceState": airspace_state,
                "distanceToPerimeterMeters": round(float(perimeter_state["distanceMeters"]), 3)
                if perimeter_state["distanceMeters"] is not None
                else None,
                "approachRateMetersPerSecond": round(
                    float(perimeter_state["approachRateMetersPerSecond"]), 3
                )
                if perimeter_state["approachRateMetersPerSecond"] is not None
                else None,
                "timeToSimulationPerimeterSeconds": round(
                    float(perimeter_state["timeToPerimeterSeconds"]), 3
                )
                if perimeter_state["timeToPerimeterSeconds"] is not None
                else None,
                "outsideSimulationPerimeter": outside_state,
                "ingressCorridorId": "UNAVAILABLE_FROM_CURRENT_OBSERVATIONS",
                "nearestSensorDistanceMeters": round(
                    min(
                        (
                            _number(item.get("rangeMeters"), math.inf)
                            for item in (
                                target_rf
                                + target_search_radar
                                + target_eo_ptz
                                + target_thermal_ptz
                                + fusion_rgb
                            )
                        ),
                        default=math.inf,
                    ),
                    3,
                )
                if target_rf
                or target_search_radar
                or target_eo_ptz
                or target_thermal_ptz
                or any(_optional_number(item.get("rangeMeters")) is not None for item in fusion_rgb)
                else None,
                "trackEstimate": {
                    key: value
                    for key, value in track_estimate.items()
                    if key != "position"
                },
                "scenarioTruth": {
                    "available": bool(target),
                    "hostile": truth if isinstance(truth, bool) else None,
                    "latitudeDegrees": _optional_number(
                        target.get("targetLatitudeDegrees")
                    ),
                    "longitudeDegrees": _optional_number(
                        target.get("targetLongitudeDegrees")
                    ),
                    "heightMeters": _optional_number(target.get("targetHeightMeters")),
                    "speedMetersPerSecond": _optional_number(
                        target.get("speedMetersPerSecond")
                    ),
                    "headingDegrees": _optional_number(target.get("headingDegrees")),
                    "airspaceState": (
                        str(target.get("airspaceState", "UNKNOWN"))
                        if target
                        else "UNAVAILABLE"
                    ),
                    "outsideSimulationPerimeter": (
                        target.get("outsideSimulationPerimeter") if target else None
                    ),
                    "distanceToPerimeterMeters": (
                        target.get("distanceToPerimeterMeters") if target else None
                    ),
                    "approachRateMetersPerSecond": (
                        target.get("approachRateMetersPerSecond") if target else None
                    ),
                    "semantics": "evaluator-only authored truth; never forwarded as the operational C2 estimate",
                },
                "modalitySummary": {
                    "widebandRF": {
                        "detected": bool(target_rf),
                        "detectingNodeCount": rf_node_count,
                        "bands": sorted(
                            {
                                str(item.get("frequencyBandLabel"))
                                for item in target_rf
                            }
                        ),
                        "strongestSnrDb": (
                            round(
                                max(_number(item.get("snrDb")) for item in target_rf),
                                3,
                            )
                            if target_rf
                            else None
                        ),
                        "observations": target_rf,
                        "method": "validated current Unreal analytic RF observations",
                        "suppliedRFModelsUsed": False,
                    },
                    "mmWave": {
                        "detected": False,
                        "detectingNodeCount": 0,
                        "nearestRangeMeters": None,
                        "maximumClosingVelocityMetersPerSecond": None,
                        "method": "excluded: no observation-authoritative Unreal mmWave stream",
                    },
                    "searchRadar": {
                        "detected": bool(target_search_radar),
                        "detectingNodeCount": len(
                            {str(item.get("nodeId")) for item in target_search_radar}
                        ),
                        "nearestRangeMeters": round(
                            min(
                                (
                                    _number(item.get("rangeMeters"), math.inf)
                                    for item in target_search_radar
                                ),
                                default=math.inf,
                            ),
                            3,
                        )
                        if target_search_radar
                        else None,
                        "maximumClosingVelocityMetersPerSecond": round(
                            max(
                                (
                                    _number(
                                        item.get("closingSpeedMetersPerSecond"), 0.0
                                    )
                                    for item in target_search_radar
                                ),
                                default=0.0,
                            ),
                            3,
                        ),
                        "observations": track_search_radar,
                        "method": "deterministic simulated long-range search radar",
                        "learnedModelOutput": False,
                    },
                    "eoPtz": {
                        "confirmed": bool(target_eo_ptz),
                        "confirmationCount": len(target_eo_ptz),
                        "observations": [
                            item
                            for item in track_ptz_confirmations
                            if item.get("sensorType") == "EO_PTZ"
                        ],
                        "rangeSemantics": "radar-cued range, never EO box/depth stacking",
                        "learnedModelOutput": False,
                    },
                    "thermalPtz": {
                        "confirmed": bool(target_thermal_ptz),
                        "confirmationCount": len(target_thermal_ptz),
                        "observations": [
                            item
                            for item in track_ptz_confirmations
                            if item.get("sensorType") == "THERMAL_PTZ"
                        ],
                        "syntheticThermal": True,
                        "rangeSemantics": "radar-cued range, never thermal image depth",
                        "learnedModelOutput": False,
                    },
                    "rgb": {
                        "detected": bool(fusion_rgb),
                        "candidateObserved": bool(selected_rgb),
                        "observationCount": len(selected_rgb),
                        "fusionEligibleObservationCount": len(fusion_rgb),
                        "diagnosticOnlyObservationCount": len(selected_rgb) - len(fusion_rgb),
                        "fusionPolicy": "fail closed: evaluator-truth projection IoU is diagnostic-only; estimated-track or calibrated association is required",
                        "associationSemantics": "Unreal projection IoU; debug association, not an identification claim",
                        "observations": [
                            {
                                key: item.get(key)
                                for key in (
                                    "evidenceId",
                                    "nodeId",
                                    "timestampUtc",
                                    "score",
                                    "rangeMeters",
                                    "modelId",
                                    "frameId",
                                    "frameFileName",
                                    "bboxXyxyPixels",
                                    "className",
                                    "visualSemantics",
                                    "associationIoU",
                                    "associationProvenance",
                                    "fusionEligible",
                                    "fusionExclusionReason",
                                )
                            }
                            for item in selected_rgb
                        ],
                    },
                    "eventCamera": {
                        "detected": bool(fusion_event),
                        "candidateObserved": bool(selected_event),
                        "observationCount": len(selected_event),
                        "fusionEligibleObservationCount": len(fusion_event),
                        "diagnosticOnlyObservationCount": len(selected_event) - len(fusion_event),
                        "fusionPolicy": "fail closed: event input must be explicitly physical, training-equivalent, non-proxy, and associated without evaluator-truth projection before it can contribute to the shared visual family; RGB and event evidence never corroborate each other",
                        "proxyInputUsed": any(bool(item.get("proxyInput")) for item in selected_event),
                        "associationSemantics": "Unreal projection IoU; debug association, not an identification claim",
                        "observations": [
                            {
                                key: item.get(key)
                                for key in (
                                    "evidenceId",
                                    "nodeId",
                                    "timestampUtc",
                                    "score",
                                    "modelId",
                                    "frameId",
                                    "frameFileName",
                                    "bboxXyxyPixels",
                                    "className",
                                    "visualSemantics",
                                    "associationIoU",
                                    "associationProvenance",
                                    "sourceEncoding",
                                    "proxyInput",
                                    "physicalNeuromorphicSensorData",
                                    "trainingPreprocessingEquivalent",
                                    "sensorHealthEligible",
                                    "fusionEligible",
                                    "fusionExclusionReason",
                                )
                            }
                            for item in selected_event
                        ],
                    },
                },
                "activeModalities": active_modalities,
                "modalityEvidence": long_range_modality_evidence,
                "visualEvidenceStatus": (
                    "RADAR_CUED_CONFIRMATION_AVAILABLE"
                    if visual_confirmations
                    else "NO_RADAR_CUED_CONFIRMATION"
                ),
                "visualFrame": {
                    "transportStatus": (
                        "FRAME_AVAILABLE" if visual_confirmations else "NO_FRAME"
                    ),
                    "confirmations": visual_confirmations,
                    "notice": (
                        "SIMULATED_SENSOR_CONFIRMATION boxes use SIMULATION_PROJECTION "
                        "provenance and are not learned-model detections."
                    ),
                },
                "fusion": fusion,
                "alert": {
                    "active": operator_cue,
                    "category": cue_category,
                    "rule": "sensor-derived preliminary or corroborated cue; scenario hostility is never used",
                    "hostilityInferredBySensors": False,
                },
            }
        )

    rf_links.sort(key=lambda item: (_number(item.get("snrDb")), str(item.get("targetId"))), reverse=True)
    mmwave_links.sort(key=lambda item: (_number(item.get("snrDb")), str(item.get("targetId"))), reverse=True)
    output_tracks.sort(key=lambda item: (not bool(item["alert"]["active"]), str(item["trackId"])))
    detected_tracks = [item for item in output_tracks if item["fusion"].get("operatorCueActive")]
    corroborated_tracks = [item for item in output_tracks if item["fusion"].get("detectionAlert")]
    return {
        "schemaVersion": OUTPUT_SCHEMA,
        "timestampUtc": as_of.isoformat(timespec="milliseconds").replace("+00:00", "Z"),
        "sourceUnrealSchemaVersion": unreal_snapshot.get("schemaVersion"),
        "sourceUnrealSnapshotTimestampUtc": unreal_snapshot.get("timestampUtc"),
        "simulationSeconds": timestamp_s,
        "simulationOnly": True,
        "detectionOnly": True,
        "actionsTaken": "none",
        "suppliedRFModelsUsed": False,
        "legacyUnrealDetectedRFLinksConsumed": True,
        "rawUnrealDetectedRFLinksConsumed": True,
        "calibratedOperationalSystem": False,
        "observationAuthority": {
            "trackSeedPolicy": "current_validated_sensor_observations_only",
            "scenarioTargetsUsedAsDetectionSeeds": False,
            "scenarioTargetsUsedForOperationalState": False,
            "rawUnrealDetectedRFLinksConsumed": True,
            "truthRegeneratedRFEnabled": False,
            "truthRegeneratedMmWaveEnabled": False,
            "rfOnlyTruthPositionFallbackEnabled": False,
        },
        "rfSimulationPolicy": {
            "receiveOnlyDetectionSimulation": True,
            "observationSource": "Unreal detectedRFLinks",
            "configuredChannelCentersHz": [channel.center_frequency_hz for channel in WIDEBAND_CHANNELS],
            "layeredRuntimeRegeneratesLinks": False,
            "sixPointTwoGHzSemantics": "detection-only noncompliant-emission hypothesis; never a friendly baseline or transmitter command",
            "realRFHardwareOperated": False,
        },
        "weather": {
            "profile": weather_name,
            "rainRateMillimetersPerHour": rain_rate,
            "visibilityMeters": visibility_m,
        },
        "simulationPerimeter": unreal_snapshot.get("simulationPerimeter"),
        "sensorNodes": sensor_records,
        "tracks": output_tracks,
        "searchRadarDetections": enriched_search_radar,
        "ptzConfirmations": enriched_ptz_confirmations,
        "longRangeEvidenceValidation": {
            "policy": (
                "fail closed: only fresh finite in-envelope RF/search-radar records and "
                "settled, explicitly confirmed PTZ records with a fresh associated radar "
                "cue and allow-listed frame path may contribute"
            ),
            "acceptedRFDetectionCount": len(rf_links),
            "rejectedRFDetectionCount": len(rf_rejections),
            "acceptedSearchRadarDetectionCount": len(enriched_search_radar),
            "rejectedSearchRadarDetectionCount": len(search_radar_rejections),
            "acceptedPtzConfirmationCount": len(enriched_ptz_confirmations),
            "rejectedPtzConfirmationCount": len(ptz_rejections),
            "rejectedSearchRadarDetections": search_radar_rejections,
            "rejectedRFDetections": rf_rejections,
            "rejectedPtzConfirmations": ptz_rejections,
            "modelOutputClaimed": False,
            "eoPixelFrameSemantics": (
                "real Unreal rendered pixel capture with simulated radar-cued projection "
                "confirmation; no learned EO model is claimed"
            ),
            "thermalFrameSemantics": (
                "synthetic Unreal thermal-like rendering and simulated radar-cued "
                "projection confirmation; no physical thermal imager or learned model is claimed"
            ),
        },
        "observations": {
            "widebandRFLinks": rf_links[:MAX_LINKS_PER_MODALITY],
            "mmWaveLinks": mmwave_links[:MAX_LINKS_PER_MODALITY],
            "searchRadarDetections": enriched_search_radar[:MAX_LINKS_PER_MODALITY],
            "ptzConfirmations": enriched_ptz_confirmations[:MAX_LINKS_PER_MODALITY],
            "rgbAssociationsAvailable": len(rgb_items),
            "rgbHealthReportsAvailable": len(rgb_health_items),
            "eventAssociationsAvailable": len(event_items),
        },
        "summary": {
            "sensorSiteCount": len(nodes),
            "registeredSensorCount": len(sensor_records),
            "scenarioTargetCount": len(targets),
            "observationContactCount": len(output_tracks),
            "detectedTrackCount": len(detected_tracks),
            "preliminaryCueCount": sum(bool(item["fusion"].get("preliminaryCue")) for item in output_tracks),
            "corroboratedDetectionCount": len(corroborated_tracks),
            "confirmedTrackCount": sum(item["fusion"].get("decision") == "CONFIRMED_TRACK" for item in output_tracks),
            "activeOperatorCueCount": sum(bool(item["alert"]["active"]) for item in output_tracks),
            "evaluationHostileCueCount": sum(
                bool(item["alert"]["active"]) and item["scenarioTruth"].get("hostile") is True
                for item in output_tracks
            ),
            "outsideDetectedTrackCount": sum(bool(item["outsideSimulationPerimeter"]) for item in detected_tracks),
            "widebandRFDetectedLinkCount": len(rf_links),
            "mmWaveDetectedLinkCount": len(mmwave_links),
            "searchRadarDetectionCount": len(enriched_search_radar),
            "eoPtzConfirmationCount": sum(
                item.get("sensorType") == "EO_PTZ"
                for item in enriched_ptz_confirmations
            ),
            "thermalPtzConfirmationCount": sum(
                item.get("sensorType") == "THERMAL_PTZ"
                for item in enriched_ptz_confirmations
            ),
            "longRangeEvidenceRejectedCount": len(rf_rejections)
            + len(search_radar_rejections)
            + len(ptz_rejections),
        },
        "fusionPolicy": {
            "method": FUSION_V3_METHOD,
            "numericScoreStacking": False,
            "minimumIndependentFamiliesForAlert": 2,
            "minimumIndependentFamiliesForConfirmation": 3,
            "minimumRFNodesForPreliminaryCue": 2,
            "hostilitySource": "evaluator-only authored simulation truth; excluded from sensor cue generation",
        },
        "limitations": [
            "Accepted RF links are deterministic Unreal simulation observations, not measured field performance or calibrated detection probabilities.",
            "The layered runtime does not regenerate RF or mmWave evidence from scenario truth; mmWave remains excluded until Unreal provides a current observation-authoritative stream.",
            "Operational position is available only from accepted search-radar polar geometry. RF/model-only contacts remain unlocated, and radar speed/course is only a radial-component approximation.",
            "Scenario targets and exact authored coordinates/kinematics are evaluator-only metadata and never seed a contact or fill an operational state estimate.",
            "The event path currently uses proxy_event_stack_v1 when no native event stream is available.",
            "RGB/event target association uses Unreal projection IoU for simulation debugging and is not an identification claim.",
            "SEARCH_RADAR, EO_PTZ, and THERMAL_PTZ evidence is generated by deterministic Unreal simulation. It is not a learned-model result, measured sensor result, calibrated probability, or physical range validation.",
            "EO/PTZ projection boxes and their radar-cued range are one confirmation record and are never counted as two independent evidence items.",
            "PTZ FRAME_AVAILABLE means Unreal declared an allow-listed atomic frame path in the v3 snapshot; the runtime does not claim pixel transport independently of the bridge file check.",
            "Legacy camera frames without debugVisualsExcludedFromSensorCapture=true are rejected from fusion.",
        ],
    }


def run_once(
    input_path: Path,
    output_path: Path,
    *,
    oak_reports: Iterable[Path | None],
    event_report: Path | None,
    visual_max_age_s: float,
) -> dict[str, Any]:
    unreal = _read_json(input_path, required=True)
    assert unreal is not None
    oak_report_paths = tuple(oak_reports)
    result = build_layered_snapshot(
        unreal,
        rgb_rows=load_rgb_evidence_reports(oak_report_paths),
        rgb_health_rows=load_rgb_sensor_health_reports(oak_report_paths),
        event_rows=load_event_evidence(event_report),
        visual_max_age_s=visual_max_age_s,
    )
    write_json_atomic(result, output_path)
    return result


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Generate the TRIAD layered-detection snapshot")
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument(
        "--oak-report",
        dest="oak_reports",
        type=Path,
        action="append",
        help="Per-node OAK watcher report. Repeat for multiple nodes; defaults to the legacy single report.",
    )
    parser.add_argument("--event-report", type=Path, default=DEFAULT_EVENT_REPORT)
    parser.add_argument("--visual-max-age", type=float, default=2.0)
    parser.add_argument("--interval", type=float, default=0.25)
    parser.add_argument("--once", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    if args.interval <= 0.0 or args.visual_max_age <= 0.0:
        raise SystemExit("--interval and --visual-max-age must be > 0")
    last_token: tuple[int, int] | None = None
    while True:
        try:
            stat = args.input.stat()
            token = (stat.st_mtime_ns, stat.st_size)
            if token != last_token:
                result = run_once(
                    args.input,
                    args.output,
                    oak_reports=args.oak_reports or (DEFAULT_OAK_REPORT,),
                    event_report=args.event_report,
                    visual_max_age_s=args.visual_max_age,
                )
                last_token = token
                print(
                    json.dumps(
                        {
                            "timestampUtc": result["timestampUtc"],
                            **result["summary"],
                            "output": str(args.output),
                        },
                        separators=(",", ":"),
                    ),
                    flush=True,
                )
            if args.once:
                return 0
        except KeyboardInterrupt:
            return 0
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            print(json.dumps({"error": str(exc)}, separators=(",", ":")), flush=True)
            if args.once:
                return 1
        time.sleep(args.interval)


if __name__ == "__main__":
    raise SystemExit(main())
