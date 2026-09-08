#!/usr/bin/env python3
"""Forward TRIAD layered sensor-fusion snapshots to Globe C2.

The bridge transports detections only. Authored scenario truth is retained solely as
non-operational evaluation metadata and never drives classification or alert state.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import logging
import math
import os
import re
import time
from collections import defaultdict
from collections.abc import Iterable, Mapping
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.parse import urlencode
from urllib.request import Request, urlopen


_DEFAULT_REPOSITORY_ROOT = Path(__file__).resolve().parents[3]
_DEFAULT_TRIAD_PROJECT = Path(
    os.environ.get("TRIAD_UNREAL_PROJECT", _DEFAULT_REPOSITORY_ROOT.parent / "TRIAD")
)
DEFAULT_SNAPSHOT = Path(
    os.environ.get(
        "TRIAD_LAYERED_SNAPSHOT",
        _DEFAULT_TRIAD_PROJECT / "Saved" / "SingaporeSensorFusion" / "latest_layered_snapshot.json",
    )
)
DEFAULT_C2_URL = "http://127.0.0.1:3000"
EXPECTED_SCHEMA = "triad.layered_detection_snapshot.v1"
ID_PREFIX = "triad:"
MAX_SNAPSHOT_BYTES = 8 * 1024 * 1024
MAX_SENSORS = 128
MAX_TRACKS = 512
MAX_OBSERVATIONS = 16_384
API_PROFILES = {"standard", "globe"}
RETRYABLE_HTTP_STATUS = {408, 425, 429, 500, 502, 503, 504}

LOGGER = logging.getLogger("triad-c2-bridge")

SENSOR_TYPES = {
    "wideband_rf": "WIDEBAND_RF",
    "mmwave": "MMWAVE",
    "rgb": "RGB",
    "event_camera": "EVENT_CAMERA",
    "search_radar": "SEARCH_RADAR",
    "eo_ptz": "EO_PTZ",
    "thermal_ptz": "THERMAL_PTZ",
    "WIDEBAND_RF": "WIDEBAND_RF",
    "MMWAVE": "MMWAVE",
    "RGB": "RGB",
    "EVENT_CAMERA": "EVENT_CAMERA",
    "SEARCH_RADAR": "SEARCH_RADAR",
    "EO_PTZ": "EO_PTZ",
    "THERMAL_PTZ": "THERMAL_PTZ",
}
SENSOR_SUFFIXES = {
    "WIDEBAND_RF": "rf-wideband",
    "MMWAVE": "mmwave",
    "RGB": "rgb",
    "EVENT_CAMERA": "event",
    "SEARCH_RADAR": "search-radar",
    "EO_PTZ": "eo-ptz",
    "THERMAL_PTZ": "thermal-ptz",
}
STANDARD_SENSOR_TYPES = {
    "WIDEBAND_RF": "RF",
    "MMWAVE": "RADAR",
    "SEARCH_RADAR": "RADAR",
    "RGB": "OPTICAL",
    "EVENT_CAMERA": "OPTICAL",
    "EO_PTZ": "EO_IR",
    "THERMAL_PTZ": "EO_IR",
}
STANDARD_DETECTION_FIELDS = {
    "id",
    "drone",
    "lat",
    "lon",
    "alt",
    "speed",
    "heading",
    "rf",
    "cls",
    "status",
    "det",
    "sources",
}
MODALITY_NAMES = {
    "wideband_rf": "WIDEBAND_RF",
    "mmwave": "MMWAVE",
    "rgb": "RGB",
    "event_camera": "EVENT_CAMERA",
}
MODALITY_FAMILIES = {
    "WIDEBAND_RF": "PASSIVE_RF",
    "MMWAVE": "ACTIVE_RADAR",
    "SEARCH_RADAR": "ACTIVE_RADAR",
    "RGB": "VISUAL",
    "EVENT_CAMERA": "VISUAL",
    "EO_PTZ": "VISUAL",
    "THERMAL_PTZ": "VISUAL",
}
SUMMARY_KEYS = {
    "WIDEBAND_RF": "widebandRF",
    "MMWAVE": "mmWave",
    "RGB": "rgb",
    "EVENT_CAMERA": "eventCamera",
}
LONG_RANGE_MODALITIES = ("SEARCH_RADAR", "EO_PTZ", "THERMAL_PTZ")
CONFIRMATION_MODALITIES = {"EO_PTZ", "THERMAL_PTZ"}
CONFIRMATION_FRAME_PATH = re.compile(
    r"^RadarPtzFrames/(eo|thermal)/([A-Za-z0-9_-]{1,80})/latest\.(?:png|jpe?g|webp)$"
)


class BridgeError(RuntimeError):
    """A recoverable snapshot or C2 API error."""


class C2Error(BridgeError):
    """A recoverable C2 transport or response error."""


class C2HTTPError(C2Error):
    """A C2 HTTP response that was not successful."""

    def __init__(self, status_code: int, detail: str) -> None:
        self.status_code = status_code
        super().__init__(f"C2 returned HTTP {status_code}: {detail}")


def _number(value: Any) -> float | None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    result = float(value)
    return result if math.isfinite(result) else None


def _bounded_number(value: Any, minimum: float, maximum: float) -> float | None:
    number = _number(value)
    return number if number is not None and minimum <= number <= maximum else None


def _text(value: Any) -> str | None:
    return value.strip() if isinstance(value, str) and value.strip() else None


def _limited_text(value: Any, limit: int) -> str | None:
    text = _text(value)
    return text[:limit] if text else None


def _modality(value: Any) -> str | None:
    text = _text(value)
    if not text:
        return None
    return SENSOR_TYPES.get(text) or SENSOR_TYPES.get(text.casefold())


def _iso_timestamp(value: Any) -> str | None:
    text = _text(value)
    if not text:
        return None
    try:
        datetime.fromisoformat(text.replace("Z", "+00:00"))
    except ValueError:
        return None
    return text


def _neutral_id(namespace: str, *values: Any) -> str:
    material = "\x1f".join(_text(value) or "" for value in values)
    digest = hashlib.sha256(
        f"triad-c2-{namespace}-v1:{material}".encode("utf-8")
    ).hexdigest()[:16].upper()
    return f"{namespace.upper()}-{digest}"


def _c2_id(value: Any) -> str | None:
    identifier = _text(value)
    if not identifier:
        return None
    return identifier if identifier.startswith(ID_PREFIX) else f"{ID_PREFIX}{identifier}"


def _operator_track_id(value: Any) -> str | None:
    """Return a stable neutral identifier without exposing authored actor names."""

    identifier = _text(value)
    if not identifier:
        return None
    digest = hashlib.sha256(
        f"triad-c2-operator-track-v1:{identifier}".encode("utf-8")
    ).hexdigest()[:12].upper()
    return f"SG-UAS-{digest}"


def _operator_corridor(value: Any) -> str | None:
    """Preserve only neutral ingress direction; remove authored affiliation text."""

    corridor = _text(value)
    if not corridor:
        return None
    upper = corridor.upper()
    for direction in ("EAST", "WEST", "NORTH", "SOUTH"):
        if direction in upper:
            return f"{direction}_INBOUND"
    return None


def _string_list(value: Any, limit: int = 128) -> list[str]:
    if not isinstance(value, list):
        return []
    result: list[str] = []
    for item in value[:limit]:
        text = _text(item)
        if text and text not in result:
            result.append(text)
    return result


def _selected_evidence(contributions: Any) -> dict[str, Mapping[str, Any]]:
    selected: dict[str, Mapping[str, Any]] = {}
    if not isinstance(contributions, list):
        return selected
    for contribution in contributions:
        if not isinstance(contribution, Mapping):
            continue
        modality = _text(contribution.get("modality"))
        evidence = contribution.get("selectedEvidence")
        if modality and isinstance(evidence, Mapping):
            selected[modality] = evidence
    return selected


def _observation_index(snapshot: Mapping[str, Any], key: str) -> dict[str, list[Mapping[str, Any]]]:
    result: dict[str, list[Mapping[str, Any]]] = defaultdict(list)
    observations = snapshot.get("observations")
    rows = observations.get(key) if isinstance(observations, Mapping) else None
    if not isinstance(rows, list):
        return result
    for row in rows[:MAX_OBSERVATIONS]:
        if not isinstance(row, Mapping):
            continue
        target_id = _text(row.get("targetId"))
        if target_id:
            result[target_id].append(row)
    return result


def _sensor_id_from_node(node_id: Any, modality: str) -> str | None:
    node = _text(node_id)
    suffix = SENSOR_SUFFIXES.get(modality)
    return f"{ID_PREFIX}{node}:{suffix}" if node and suffix else None


def build_sensor_payloads(snapshot: dict[str, Any]) -> list[dict[str, Any]]:
    """Register every layered sensor with its real modality and node location."""
    sensors = snapshot.get("sensorNodes")
    if not isinstance(sensors, list):
        return []

    payloads: list[dict[str, Any]] = []
    seen: set[str] = set()
    observed_at = _iso_timestamp(snapshot.get("sourceUnrealSnapshotTimestampUtc")) or _iso_timestamp(
        snapshot.get("timestampUtc")
    )
    for sensor in sensors[:MAX_SENSORS]:
        if not isinstance(sensor, Mapping):
            continue
        sensor_type = _modality(sensor.get("sensorType")) or _modality(sensor.get("modality"))
        sensor_id = _c2_id(sensor.get("sensorId"))
        node_id = _text(sensor.get("nodeId"))
        latitude = _number(sensor.get("latitudeDegrees"))
        longitude = _number(sensor.get("longitudeDegrees"))
        range_m = _number(sensor.get("rangeMeters"))
        reported_status = _text(
            sensor.get("runtimeStatus")
            if "runtimeStatus" in sensor
            else sensor.get("status")
        )
        if sensor_type in LONG_RANGE_MODALITIES and (
            sensor.get("configured") is not True or sensor.get("enabled") is not True
        ):
            reported_status = "OFFLINE"
        if reported_status not in {"ONLINE", "DEGRADED", "OFFLINE"}:
            reported_status = "OFFLINE"
        azimuth_deg = _number(sensor.get("azimuthDeg"))
        if azimuth_deg is not None and not 0.0 <= azimuth_deg <= 360.0:
            azimuth_deg = None
        horizontal_fov_deg = _number(sensor.get("horizontalFovDeg"))
        if horizontal_fov_deg is not None and not 0.0 < horizontal_fov_deg <= 360.0:
            horizontal_fov_deg = None
        if (
            not sensor_type
            or not sensor_id
            or sensor_id in seen
            or not node_id
            or latitude is None
            or longitude is None
            or not -90 <= latitude <= 90
            or not -180 <= longitude <= 180
            or range_m is None
            or range_m < 0
        ):
            continue
        seen.add(sensor_id)
        payloads.append(
            {
                "id": sensor_id,
                "type": sensor_type,
                "name": f"{node_id} / {sensor_type.replace('_', ' ')}",
                "nodeId": node_id,
                "rangeM": range_m,
                "band": _limited_text(sensor.get("band"), 300),
                "method": _limited_text(sensor.get("method"), 300),
                "reportedStatus": reported_status,
                "observedAtUtc": observed_at,
                "lat": latitude,
                "lon": longitude,
                "azimuthDeg": azimuth_deg,
                "horizontalFovDeg": horizontal_fov_deg,
            }
        )
    return [
        {key: value for key, value in payload.items() if value is not None}
        for payload in payloads
    ]


def build_standard_sensor_payloads(snapshot: dict[str, Any]) -> list[dict[str, Any]]:
    """Project live sensors onto the exact sensor contract supplied by the operator.

    The standard API has no explicit source-health field. Only sensors whose source
    health is ONLINE receive a heartbeat; unavailable sensors are deregistered by
    :class:`TriadBridge` instead of being accidentally promoted to ONLINE.
    """

    projected: list[dict[str, Any]] = []
    for payload in build_sensor_payloads(snapshot):
        if payload.get("reportedStatus") != "ONLINE":
            continue
        sensor_type = STANDARD_SENSOR_TYPES.get(str(payload.get("type")))
        if sensor_type is None:
            continue
        item = {
            "id": payload["id"],
            "type": sensor_type,
            "name": payload["name"],
            "rangeM": payload["rangeM"],
            "band": payload.get("band"),
            "lat": payload.get("lat"),
            "lon": payload.get("lon"),
        }
        projected.append({key: value for key, value in item.items() if value is not None})
    return projected


def _evidence_sensor_ids(
    modality: str,
    rows: Iterable[Mapping[str, Any]],
    selected: Mapping[str, Any] | None,
) -> list[str]:
    key = "receiverId" if modality == "WIDEBAND_RF" else "radarId"
    identifiers = {
        identifier
        for row in rows
        if (identifier := _c2_id(row.get(key))) is not None
    }
    if selected:
        identifier = _sensor_id_from_node(selected.get("nodeId"), modality)
        if identifier:
            identifiers.add(identifier)
    return sorted(identifiers)


def _min_number(rows: Iterable[Mapping[str, Any]], key: str) -> float | None:
    values = [number for row in rows if (number := _number(row.get(key))) is not None]
    return min(values) if values else None


def _max_number(rows: Iterable[Mapping[str, Any]], key: str) -> float | None:
    values = [number for row in rows if (number := _number(row.get(key))) is not None]
    return max(values) if values else None


def _frequency_ghz(row: Mapping[str, Any]) -> float | None:
    """Normalize current and legacy RF observation frequency fields."""

    for key in ("frequencyGHz", "centerFrequencyGHz"):
        value = _number(row.get(key))
        if value is not None and value > 0.0:
            return value
    value_hz = _number(row.get("centerFrequencyHz"))
    return value_hz / 1e9 if value_hz is not None and value_hz > 0.0 else None


def _supplied_long_range_evidence(
    track: Mapping[str, Any],
    fusion: Mapping[str, Any],
) -> list[dict[str, Any]]:
    """Normalize fusion-v3 radar/PTZ evidence into the strict C2 contract.

    Families are derived locally instead of trusting an input family label. EO and
    thermal remain distinct views but a single VISUAL corroboration family.
    """

    rows = track.get("modalityEvidence")
    if not isinstance(rows, list):
        return []
    score_semantics = _limited_text(fusion.get("scoreSemantics"), 400)
    result: list[dict[str, Any]] = []
    seen: set[str] = set()
    for raw in rows[:12]:
        if not isinstance(raw, Mapping):
            continue
        modality = _modality(raw.get("modality"))
        if modality not in LONG_RANGE_MODALITIES or modality in seen:
            continue
        seen.add(modality)
        detected = raw.get("detected") is True
        sensor_ids = sorted(
            {
                sensor_id
                for value in (raw.get("sensorIds") if isinstance(raw.get("sensorIds"), list) else [])[:128]
                if (sensor_id := _c2_id(value)) is not None
            }
        ) if detected else []
        node_count = _bounded_number(raw.get("nodeCount"), 0.0, 128.0)
        evidence: dict[str, Any] = {
            "modality": modality,
            "family": MODALITY_FAMILIES[modality],
            "detected": detected,
            "sensorIds": sensor_ids,
            "nodeCount": int(node_count) if node_count is not None else len(sensor_ids),
            "score": _bounded_number(raw.get("score"), 0.0, 1.0),
            "scoreSemantics": score_semantics,
            "latencyMs": _bounded_number(raw.get("latencyMs"), 0.0, 10_000_000.0),
            "rangeM": _bounded_number(raw.get("rangeM"), 0.0, 10_000_000.0),
        }
        if modality == "SEARCH_RADAR":
            evidence.update(
                {
                    "radialSpeedMps": _bounded_number(
                        raw.get("radialSpeedMps"), -10_000.0, 10_000.0
                    ),
                    "closingSpeedMps": _bounded_number(
                        raw.get("closingSpeedMps"), -10_000.0, 10_000.0
                    ),
                    "method": "SIMULATED deterministic search-radar measurement; not field validated",
                }
            )
        elif modality == "EO_PTZ":
            evidence["method"] = (
                "SIMULATED radar-cued EO projection confirmation; not learned-model output"
            )
        else:
            evidence["method"] = (
                "SIMULATED radar-cued synthetic thermal confirmation; not learned-model output"
            )
        result.append({key: value for key, value in evidence.items() if value is not None})
    return result


def _modality_evidence(
    track: Mapping[str, Any],
    rf_rows: list[Mapping[str, Any]],
    mmwave_rows: list[Mapping[str, Any]],
) -> list[dict[str, Any]]:
    summary = track.get("modalitySummary")
    summary = summary if isinstance(summary, Mapping) else {}
    fusion = track.get("fusion")
    fusion = fusion if isinstance(fusion, Mapping) else {}
    selected = _selected_evidence(fusion.get("contributions"))
    score_semantics = _text(fusion.get("scoreSemantics"))
    result: list[dict[str, Any]] = []

    for modality in ("WIDEBAND_RF", "MMWAVE", "RGB", "EVENT_CAMERA"):
        item = summary.get(SUMMARY_KEYS[modality])
        item = item if isinstance(item, Mapping) else {}
        source_key = next((key for key, value in MODALITY_NAMES.items() if value == modality), "")
        chosen = selected.get(source_key)
        rows = rf_rows if modality == "WIDEBAND_RF" else mmwave_rows if modality == "MMWAVE" else []
        detected = item.get("detected") is True
        sensor_ids = _evidence_sensor_ids(modality, rows, chosen) if detected else []
        evidence: dict[str, Any] = {
            "modality": modality,
            "family": MODALITY_FAMILIES[modality],
            "detected": detected,
            "sensorIds": sensor_ids,
            "nodeCount": int(_number(item.get("detectingNodeCount")) or len(sensor_ids)),
            "method": _text(item.get("method")),
        }
        if chosen:
            evidence.update(
                {
                    "score": _number(chosen.get("discountedEvidenceScore")),
                    "scoreSemantics": score_semantics,
                    "latencyMs": _number(chosen.get("latencyMilliseconds")),
                    "rangeM": _number(chosen.get("rangeMeters")),
                    "modelId": _text(chosen.get("scoreOrigin")),
                }
            )
        if modality == "WIDEBAND_RF":
            frequencies = sorted(
                {
                    round(value, 6)
                    for row in rows
                    if (value := _frequency_ghz(row)) is not None
                }
            )
            evidence.update(
                {
                    "bands": _string_list(item.get("bands"), 32),
                    "frequenciesGHz": frequencies,
                    "strongestSnrDb": _number(item.get("strongestSnrDb"))
                    if _number(item.get("strongestSnrDb")) is not None
                    else _max_number(rows, "snrDb"),
                    "rangeM": evidence.get("rangeM") or _min_number(rows, "rangeMeters"),
                }
            )
        elif modality == "MMWAVE":
            nearest = min(rows, key=lambda row: _number(row.get("rangeMeters")) or math.inf) if rows else None
            evidence.update(
                {
                    "strongestSnrDb": _max_number(rows, "snrDb"),
                    "rangeM": _number(item.get("nearestRangeMeters"))
                    or evidence.get("rangeM")
                    or _min_number(rows, "rangeMeters"),
                    "radialSpeedMps": _number(nearest.get("radialVelocityMetersPerSecond")) if nearest else None,
                    "closingSpeedMps": _number(item.get("maximumClosingVelocityMetersPerSecond")),
                }
            )
        elif modality == "RGB":
            evidence["associationSemantics"] = _text(item.get("associationSemantics"))
        else:
            evidence["proxyInput"] = item.get("proxyInputUsed") is True
            evidence["associationSemantics"] = _text(item.get("associationSemantics"))
        result.append({key: value for key, value in evidence.items() if value is not None})
    result.extend(_supplied_long_range_evidence(track, fusion))
    return result


def _sanitize_model_box(value: Any, *, index: int) -> dict[str, Any] | None:
    if not isinstance(value, Mapping):
        return None
    if value.get("kind") != "MODEL_DETECTION" or value.get("source") != "MODEL_OUTPUT":
        return None
    bbox_raw = value.get("bbox")
    if not isinstance(bbox_raw, list) or len(bbox_raw) != 4:
        return None
    bbox = [_number(item) for item in bbox_raw]
    if any(item is None for item in bbox):
        return None
    x_min, y_min, x_max, y_max = (float(item) for item in bbox if item is not None)
    coordinate_space = _text(value.get("coordinateSpace")) or "PIXELS"
    limit = 1.0 if coordinate_space == "NORMALIZED" else 100_000.0
    if (
        coordinate_space not in {"NORMALIZED", "PIXELS"}
        or x_min < 0
        or y_min < 0
        or x_max > limit
        or y_max > limit
        or x_max <= x_min
        or y_max <= y_min
    ):
        return None
    sensor_type = _text(value.get("sensorType"))
    if sensor_type not in {"RGB", "EVENT_CAMERA"}:
        return None
    confidence = _number(value.get("confidence"))
    if confidence is not None and not 0 <= confidence <= 1:
        confidence = None
    range_m = _number(value.get("rangeM"))
    if range_m is not None and not 0.0 <= range_m <= 10_000_000.0:
        range_m = None
    result = {
        "id": _text(value.get("id")) or f"visual-box-{index}",
        "kind": "MODEL_DETECTION",
        "sensorType": sensor_type,
        "label": _text(value.get("label")) or "UAS candidate",
        "bbox": [x_min, y_min, x_max, y_max],
        "coordinateSpace": coordinate_space,
        "source": "MODEL_OUTPUT",
    }
    if confidence is not None:
        result["confidence"] = confidence
    if range_m is not None:
        result["rangeM"] = range_m
        result["rangeSemantics"] = "SIMULATION_DEPTH_POST_DETECTION"
    return result


def _legacy_visual_evidence(
    track: Mapping[str, Any],
    evidence: list[dict[str, Any]],
) -> tuple[str, dict[str, Any] | None]:
    """Return only supplied model boxes; never synthesize geometry from track truth."""
    raw = track.get("visualEvidence")
    if not isinstance(raw, Mapping):
        summary = track.get("modalitySummary")
        summary = summary if isinstance(summary, Mapping) else {}
        observation_rows: list[tuple[str, Mapping[str, Any]]] = []
        for summary_key, sensor_type in (("rgb", "RGB"), ("eventCamera", "EVENT_CAMERA")):
            item = summary.get(summary_key)
            rows = item.get("observations") if isinstance(item, Mapping) else None
            if isinstance(rows, list):
                observation_rows.extend(
                    (sensor_type, row) for row in rows if isinstance(row, Mapping)
                )
        candidates = [
            (sensor_type, row)
            for sensor_type, row in observation_rows
            if isinstance(row.get("bboxXyxyPixels"), list)
        ]
        if candidates:
            candidates.sort(
                key=lambda entry: (
                    _text(entry[1].get("timestampUtc")) or "",
                    _text(entry[1].get("frameId")) or "",
                ),
                reverse=True,
            )
            selected_type, selected_row = candidates[0]
            selected_frame = _text(selected_row.get("frameId"))
            selected_node = _text(selected_row.get("nodeId"))
            same_frame = [
                (sensor_type, row)
                for sensor_type, row in candidates
                if sensor_type == selected_type
                and _text(row.get("frameId")) == selected_frame
                and _text(row.get("nodeId")) == selected_node
            ]
            boxes = []
            for index, (sensor_type, row) in enumerate(same_frame):
                visual_semantics = _text(row.get("visualSemantics"))
                if not visual_semantics or not visual_semantics.startswith("model_detector_bounding_box"):
                    continue
                proxy_candidate = (
                    sensor_type == "EVENT_CAMERA" and row.get("proxyInput") is True
                )
                class_label = _text(row.get("className")) or "UAS candidate"
                boxes.append(
                    _sanitize_model_box(
                        {
                            "id": row.get("evidenceId"),
                            "sensorType": sensor_type,
                            "label": (
                                f"PROXY CANDIDATE | {class_label}"
                                if proxy_candidate
                                else class_label
                            ),
                            "confidence": row.get("score"),
                            "rangeM": (
                                row.get("rangeMeters")
                                if sensor_type == "RGB"
                                else None
                            ),
                            "bbox": row.get("bboxXyxyPixels"),
                            "coordinateSpace": "PIXELS",
                            "kind": "MODEL_DETECTION",
                            "source": "MODEL_OUTPUT",
                        },
                        index=index,
                    )
                )
            model_boxes = [box for box in boxes if box is not None]
            if model_boxes:
                selected_is_proxy = (
                    selected_type == "EVENT_CAMERA"
                    and selected_row.get("proxyInput") is True
                )
                return "MODEL_DETECTION_AVAILABLE", {
                    "frameId": selected_frame,
                    "timestampUtc": _text(selected_row.get("timestampUtc")),
                    "nodeId": selected_node,
                    "modelId": _text(selected_row.get("modelId")) or next(
                        (item.get("modelId") for item in evidence if item.get("modality") == selected_type),
                        None,
                    ),
                    "modelBoxes": model_boxes,
                    "debugProjectionBoxes": [],
                    "transportStatus": "SOURCE_FRAME_NOT_EXPORTED",
                    "note": (
                        "Violet boxes are RGB-derived event-model proxy candidates only: "
                        "non-physical, not training-equivalent, diagnostic-only, and excluded "
                        "from fusion-family counting. No projection-truth box is shown."
                        if selected_is_proxy
                        else "Cyan boxes are actual OAK RGB model outputs. Association to a "
                        "simulated track used projection IoU only. SIM DEPTH, when present, is "
                        "post-detection Unreal simulation depth and is not RF distance. No "
                        "projection-truth box is shown."
                    ),
                }
    if isinstance(raw, Mapping):
        model_rows = raw.get("modelBoxes")
        model_boxes = [
            box
            for index, row in enumerate(model_rows if isinstance(model_rows, list) else [])
            if (box := _sanitize_model_box(row, index=index)) is not None
        ]
        frame: dict[str, Any] = {
            "frameId": _text(raw.get("frameId")),
            "timestampUtc": _text(raw.get("timestampUtc")),
            "nodeId": _text(raw.get("nodeId")),
            "modelId": _text(raw.get("modelId")),
            "width": _number(raw.get("width")),
            "height": _number(raw.get("height")),
            "modelBoxes": model_boxes,
            # Projection/truth geometry is deliberately never forwarded to the
            # operational C2 display, even when it exists in a debug snapshot.
            "debugProjectionBoxes": [],
            "transportStatus": "SOURCE_FRAME_NOT_EXPORTED",
            "note": (
                "Box coordinates are supplied detector metadata; the source image is not "
                "transported. SIM DEPTH, when present, is post-detection Unreal simulation "
                "depth and is not RF distance."
            ),
        }
        frame = {key: value for key, value in frame.items() if value is not None}
        return ("MODEL_DETECTION_AVAILABLE" if model_boxes else "MODEL_METADATA_ONLY", frame)

    visual = next(
        (item for item in evidence if item.get("detected") and item.get("modality") in {"RGB", "EVENT_CAMERA"}),
        None,
    )
    if not visual:
        return "UNAVAILABLE", None
    frame = {
        "nodeId": visual.get("sensorIds", [None])[0] if visual.get("sensorIds") else None,
        "modelId": visual.get("modelId"),
        "modelBoxes": [],
        "debugProjectionBoxes": [],
        "transportStatus": "SOURCE_FRAME_NOT_EXPORTED",
        "note": "A visual model contribution exists, but this snapshot does not export its frame or bounding-box coordinates.",
    }
    return "MODEL_METADATA_ONLY", {key: value for key, value in frame.items() if value is not None}


def _sanitize_confirmation_box(
    value: Any,
    *,
    modality: str,
    width: float,
    height: float,
    confirmation_id: str,
    index: int,
) -> dict[str, Any] | None:
    """Accept only explicit simulated-projection PTZ geometry.

    A radar/PTZ analytic box can never enter C2 as MODEL_DETECTION/MODEL_OUTPUT.
    """

    if not isinstance(value, Mapping):
        return None
    if (
        value.get("kind") != "SIMULATED_SENSOR_CONFIRMATION"
        or value.get("source") != "SIMULATION_PROJECTION"
        or value.get("sensorType") != modality
    ):
        return None
    bbox_raw = value.get("bbox")
    if not isinstance(bbox_raw, list) or len(bbox_raw) != 4:
        return None
    bbox = [_number(item) for item in bbox_raw]
    if any(item is None for item in bbox):
        return None
    x_min, y_min, x_max, y_max = (float(item) for item in bbox if item is not None)
    coordinate_space = _text(value.get("coordinateSpace")) or "PIXELS"
    max_x = 1.0 if coordinate_space == "NORMALIZED" else width
    max_y = 1.0 if coordinate_space == "NORMALIZED" else height
    if (
        coordinate_space not in {"NORMALIZED", "PIXELS"}
        or x_min < 0.0
        or y_min < 0.0
        or x_max > max_x
        or y_max > max_y
        or x_max <= x_min
        or y_max <= y_min
    ):
        return None
    confidence = _bounded_number(value.get("confidence"), 0.0, 1.0)
    result: dict[str, Any] = {
        "id": _neutral_id("sim-box", confirmation_id, value.get("id"), index),
        "kind": "SIMULATED_SENSOR_CONFIRMATION",
        "sensorType": modality,
        "label": "SIMULATED SENSOR CONFIRMATION",
        "bbox": [x_min, y_min, x_max, y_max],
        "coordinateSpace": coordinate_space,
        "source": "SIMULATION_PROJECTION",
    }
    if confidence is not None:
        result["confidence"] = confidence
    return result


def _sanitize_confirmation(value: Any, *, index: int) -> dict[str, Any] | None:
    if not isinstance(value, Mapping):
        return None
    modality = _modality(value.get("modality")) or _modality(value.get("sensorType"))
    if modality not in CONFIRMATION_MODALITIES or value.get("simulated") is not True:
        return None
    # Fusion rejects occluded/missing-frame records. Re-check both flags at this
    # trust boundary while allowing older audited snapshots that omitted them.
    if value.get("hasFrame") is False or value.get("lineOfSight") is False:
        return None
    timestamp = _iso_timestamp(value.get("timestampUtc"))
    node_id = _limited_text(value.get("nodeId"), 160)
    sensor_id = _c2_id(value.get("sensorId"))
    path = _limited_text(value.get("relativePath"), 240) or _limited_text(
        value.get("frameRelativePath"), 240
    )
    width = _bounded_number(value.get("width"), 1.0, 16_384.0)
    height = _bounded_number(value.get("height"), 1.0, 16_384.0)
    if not timestamp or not node_id or not sensor_id or not path or width is None or height is None:
        return None
    match = CONFIRMATION_FRAME_PATH.fullmatch(path)
    expected_directory = "eo" if modality == "EO_PTZ" else "thermal"
    if not match or match.group(1) != expected_directory or match.group(2) != node_id:
        return None

    confirmation_id = _neutral_id("sim-confirm", value.get("id"), sensor_id, timestamp, index)
    boxes_raw = value.get("boxes")
    boxes = [
        box
        for box_index, raw in enumerate(boxes_raw[:32] if isinstance(boxes_raw, list) else [])
        if (
            box := _sanitize_confirmation_box(
                raw,
                modality=modality,
                width=width,
                height=height,
                confirmation_id=confirmation_id,
                index=box_index,
            )
        ) is not None
    ]

    reticle_payload: dict[str, Any] | None = None
    reticle = value.get("reticle")
    if isinstance(reticle, Mapping):
        coordinate_space = _text(reticle.get("coordinateSpace")) or "PIXELS"
        x = _number(reticle.get("x"))
        y = _number(reticle.get("y"))
        max_x = 1.0 if coordinate_space == "NORMALIZED" else width
        max_y = 1.0 if coordinate_space == "NORMALIZED" else height
        if (
            coordinate_space in {"NORMALIZED", "PIXELS"}
            and x is not None
            and y is not None
            and 0.0 <= x <= max_x
            and 0.0 <= y <= max_y
        ):
            reticle_payload = {"x": x, "y": y, "coordinateSpace": coordinate_space}

    return {
        "id": confirmation_id,
        "modality": modality,
        "label": (
            "SIM EO RADAR-CUED CONFIRMATION"
            if modality == "EO_PTZ"
            else "SIM THERMAL RADAR-CUED CONFIRMATION"
        ),
        "simulated": True,
        "timestampUtc": timestamp,
        "nodeId": node_id,
        "sensorId": sensor_id,
        "relativePath": path,
        "width": int(width),
        "height": int(height),
        "reticle": reticle_payload,
        "boxes": boxes,
    }


def _visual_evidence(
    track: Mapping[str, Any],
    evidence: list[dict[str, Any]],
) -> tuple[str, dict[str, Any] | None]:
    """Combine genuine model boxes with tightly scoped simulated PTZ frames."""

    legacy_status, legacy_frame = _legacy_visual_evidence(track, evidence)
    raw_frame = track.get("visualFrame")
    raw_confirmations = raw_frame.get("confirmations") if isinstance(raw_frame, Mapping) else None
    confirmations = [
        confirmation
        for index, raw in enumerate(
            raw_confirmations[:4] if isinstance(raw_confirmations, list) else []
        )
        if (confirmation := _sanitize_confirmation(raw, index=index)) is not None
    ]
    if not confirmations:
        return legacy_status, legacy_frame

    model_boxes = (
        legacy_frame.get("modelBoxes", [])
        if isinstance(legacy_frame, Mapping) and isinstance(legacy_frame.get("modelBoxes"), list)
        else []
    )
    frame = {
        "timestampUtc": max(item["timestampUtc"] for item in confirmations),
        "nodeId": confirmations[0]["nodeId"],
        "modelBoxes": model_boxes,
        "debugProjectionBoxes": [],
        "confirmations": confirmations,
        "transportStatus": "FRAME_AVAILABLE",
        "note": (
            "Radar-cued EO and thermal tiles are simulated Unreal frames. Their dashed "
            "boxes are SIMULATED_SENSOR_CONFIRMATION/SIMULATION_PROJECTION evidence, "
            "never learned-model output or physical sensor video."
        ),
    }
    return "RADAR_CUED_CONFIRMATION_AVAILABLE", frame


def _weather_payload(snapshot: Mapping[str, Any]) -> dict[str, Any] | None:
    weather = snapshot.get("weather")
    if not isinstance(weather, Mapping):
        return None
    profile = _text(weather.get("profile"))
    rain = _number(weather.get("rainRateMillimetersPerHour"))
    visibility = _number(weather.get("visibilityMeters"))
    if not profile or rain is None or visibility is None:
        return None
    return {"profile": profile, "rainRateMmH": max(0.0, rain), "visibilityM": max(0.0, visibility)}


def _track_estimate_payload(track: Mapping[str, Any]) -> dict[str, Any] | None:
    estimate = track.get("trackEstimate")
    if not isinstance(estimate, Mapping):
        return None
    method = _text(estimate.get("method"))
    if (
        not method
        or estimate.get("truthCoordinatesForwardedToC2") is not False
        or estimate.get("calibratedFieldAccuracyClaimed") is not False
        or estimate.get("truthSeededSimulationSurrogate") is not False
        or estimate.get("measurementDerivedLocalization") is not True
    ):
        return None
    return {
        "horizontalOneSigmaM": _number(estimate.get("horizontalOneSigmaMeters")),
        "speedOneSigmaMps": _number(estimate.get("speedOneSigmaMetersPerSecond")),
        "headingOneSigmaDeg": _number(estimate.get("headingOneSigmaDegrees")),
        "method": method,
        "truthCoordinatesForwardedToC2": False,
        "calibratedFieldAccuracyClaimed": False,
        "truthSeededSimulationSurrogate": False,
        "measurementDerivedLocalization": True,
        "uncertaintyEmpiricallyCalibrated": estimate.get("uncertaintyEmpiricallyCalibrated") is True,
        "sigmaSemantics": _text(estimate.get("sigmaSemantics")),
    }


def _simulation_perimeter_payload(snapshot: Mapping[str, Any]) -> dict[str, Any] | None:
    perimeter = snapshot.get("simulationPerimeter")
    if not isinstance(perimeter, Mapping) or perimeter.get("enabled") is not True:
        return None
    if perimeter.get("legalOrNationalBoundary") is not False:
        return None
    geometry_type = str(
        perimeter.get("geometryType") or perimeter.get("shape") or ""
    ).strip().lower()
    common = {
        "referenceName": _text(perimeter.get("referenceName"))
        or "Singapore_Simulation_Perimeter",
        "legalOrNationalBoundary": False,
        "purpose": _text(perimeter.get("purpose"))
        or "Simulation approach/inside/departure classification only.",
    }
    if geometry_type in {"circle", "wgs84_geodesic_circle"}:
        center_lat = _number(perimeter.get("centerLatitudeDegrees"))
        center_lon = _number(perimeter.get("centerLongitudeDegrees"))
        radius_m = _number(perimeter.get("radiusMeters"))
        if (
            center_lat is None
            or center_lon is None
            or radius_m is None
            or not -90.0 <= center_lat <= 90.0
            or not -180.0 <= center_lon <= 180.0
            or radius_m <= 0.0
        ):
            return None
        return {
            **common,
            "geometryType": "wgs84_geodesic_circle",
            "centerLat": center_lat,
            "centerLon": center_lon,
            "radiusM": radius_m,
        }
    min_lat = _number(perimeter.get("minimumLatitudeDegrees"))
    max_lat = _number(perimeter.get("maximumLatitudeDegrees"))
    min_lon = _number(perimeter.get("minimumLongitudeDegrees"))
    max_lon = _number(perimeter.get("maximumLongitudeDegrees"))
    if (
        min_lat is None
        or max_lat is None
        or min_lon is None
        or max_lon is None
        or min_lat >= max_lat
        or min_lon >= max_lon
    ):
        return None
    return {
        **common,
        "geometryType": "axis_aligned_wgs84_rectangle",
        "minLat": min_lat,
        "maxLat": max_lat,
        "minLon": min_lon,
        "maxLon": max_lon,
    }


def build_operating_context_payload(snapshot: Mapping[str, Any]) -> dict[str, Any] | None:
    """Build track-independent weather/perimeter context for the C3 panel."""

    timestamp = _text(snapshot.get("sourceUnrealSnapshotTimestampUtc")) or _text(
        snapshot.get("timestampUtc")
    )
    if not timestamp or snapshot.get("detectionOnly") is not True:
        return None
    weather = _weather_payload(snapshot)
    perimeter = _simulation_perimeter_payload(snapshot)
    if weather is None and perimeter is None:
        return None
    payload: dict[str, Any] = {
        "sourceTimestampUtc": timestamp,
        "detectionOnly": True,
        "weather": weather,
        "simulationPerimeter": perimeter,
    }
    return {key: value for key, value in payload.items() if value is not None}


def _fusion_tier(
    fusion: Mapping[str, Any],
    evidence: Iterable[Mapping[str, Any]] = (),
) -> str:
    decision = _text(fusion.get("decision"))
    families = {
        family
        for item in evidence
        if item.get("detected") is True
        and (family := _text(item.get("family"))) in {"PASSIVE_RF", "ACTIVE_RADAR", "VISUAL"}
    }
    family_count = len(families)
    if not families:
        family_count = int(_number(fusion.get("activeModalityFamilyCount")) or 0)
    if fusion.get("preliminaryCue") is True or decision in {"PRELIMINARY_RF_CUE", "HOLD_FOR_CORROBORATION"} or family_count <= 1:
        return "PRELIMINARY"
    tier = _text(fusion.get("confirmationTier"))
    if tier == "CONFIRMED":
        # Confirmation is exactly three independent families. EO and thermal are
        # both VISUAL, and mmWave/search radar are both ACTIVE_RADAR.
        return (
            "CONFIRMED"
            if {"PASSIVE_RF", "ACTIVE_RADAR", "VISUAL"}.issubset(families)
            else "CORROBORATED"
        )
    if tier == "CORROBORATED":
        return "CORROBORATED" if family_count >= 2 else "PRELIMINARY"
    return tier if tier == "UNCONFIRMED" else "UNCONFIRMED"


def build_detection_payloads(snapshot: dict[str, Any]) -> list[dict[str, Any]]:
    """Map active sensor cues and fused detections without using scenario truth."""
    tracks = snapshot.get("tracks")
    if not isinstance(tracks, list):
        return []
    rf_index = _observation_index(snapshot, "widebandRFLinks")
    mmwave_index = _observation_index(snapshot, "mmWaveLinks")
    weather = _weather_payload(snapshot)
    timestamp = _text(snapshot.get("timestampUtc"))
    simulation_perimeter = _simulation_perimeter_payload(snapshot)
    payloads: list[dict[str, Any]] = []
    seen_ids: set[str] = set()

    for track in tracks[:MAX_TRACKS]:
        if not isinstance(track, Mapping):
            continue
        track_id = _text(track.get("trackId"))
        operator_track_id = _operator_track_id(track_id)
        c2_id = _c2_id(operator_track_id)
        latitude = _number(track.get("latitudeDegrees"))
        longitude = _number(track.get("longitudeDegrees"))
        track_estimate = _track_estimate_payload(track)
        fusion = track.get("fusion")
        fusion = fusion if isinstance(fusion, Mapping) else {}
        if (
            not track_id
            or not c2_id
            or track.get("positionAvailable") is not True
            or track_estimate is None
            or latitude is None
            or longitude is None
            or not -90 <= latitude <= 90
            or not -180 <= longitude <= 180
        ):
            continue
        if c2_id in seen_ids:
            continue
        if fusion.get("operatorCueActive") is not True:
            continue

        evidence = _modality_evidence(
            track,
            rf_index.get(track_id, []),
            mmwave_index.get(track_id, []),
        )
        detected_evidence = [item for item in evidence if item.get("detected") is True]
        if not detected_evidence:
            continue
        active = [item["modality"] for item in detected_evidence]
        sources = sorted(
            {
                sensor_id
                for item in detected_evidence
                for sensor_id in item.get("sensorIds", [])
                if isinstance(sensor_id, str)
            }
        )
        tier = _fusion_tier(fusion, detected_evidence)
        visual_status, visual_frame = _visual_evidence(track, evidence)
        alert = track.get("alert")
        alert = alert if isinstance(alert, Mapping) else {}
        rf_item = next((item for item in evidence if item["modality"] == "WIDEBAND_RF"), {})
        rf_bands = rf_item.get("bands") if isinstance(rf_item.get("bands"), list) else []
        approach_state = _text(track.get("airspaceState"))
        if approach_state not in {"APPROACHING", "RECEDING", "CROSSING", "INSIDE_PERIMETER", "UNKNOWN"}:
            approach_state = "UNKNOWN"
        nearest_range = _number(fusion.get("nearestContributingRangeMeters"))
        if nearest_range is None:
            evidence_ranges = [
                value
                for item in detected_evidence
                if (value := _number(item.get("rangeM"))) is not None
            ]
            nearest_range = min(evidence_ranges, default=None)
        payload: dict[str, Any] = {
            "id": c2_id,
            "drone": "UNKNOWN UAS",
            "targetType": _text(track.get("targetType")) or "UAS",
            "lat": latitude,
            "lon": longitude,
            "alt": _number(track.get("heightMeters")),
            "speed": _number(track.get("speedMetersPerSecond")),
            "heading": _number(track.get("headingDegrees")),
            "rf": " / ".join(rf_bands) if rf_bands else None,
            "cls": (
                "SUSPECT"
                if fusion.get("detectionAlert") is True
                and tier in {"CORROBORATED", "CONFIRMED"}
                else "UNKNOWN"
            ),
            "status": "TRACKING",
            "det": "+".join(active),
            "sources": sources,
            "approachState": approach_state,
            "outsideSingapore": (
                track.get("outsideSimulationPerimeter")
                if isinstance(track.get("outsideSimulationPerimeter"), bool)
                else None
            ),
            "perimeterDistanceM": _number(track.get("distanceToPerimeterMeters")),
            "timeToPerimeterS": _number(track.get("timeToSimulationPerimeterSeconds")),
            "approachRateMps": _number(track.get("approachRateMetersPerSecond")),
            "ingressCorridorId": _operator_corridor(track.get("ingressCorridorId")),
            "fusionDecision": _text(fusion.get("decision")),
            "fusionTier": tier,
            "operatorCueActive": True,
            "preliminaryCue": fusion.get("preliminaryCue") is True,
            "alertCategory": _text(alert.get("category")),
            "fusionScore": _number(fusion.get("fusedEvidenceScore")),
            "fusionScoreSemantics": _text(fusion.get("scoreSemantics")),
            "activeModalities": active,
            "modalityEvidence": evidence,
            "fusionLatencyMs": _number(fusion.get("decisionLatencyMilliseconds")),
            "visualEvidenceStatus": visual_status,
            "visualFrame": visual_frame,
            "weather": weather,
            "sourceTimestampUtc": timestamp,
            "detectionOnly": snapshot.get("detectionOnly") is not False,
            "trackEstimate": track_estimate,
            "simulationPerimeter": simulation_perimeter,
        }
        if nearest_range is not None:
            # Auxiliary nearest contributing sensor range. Lat/lon remain the
            # absolute track-position source, so no bearing is required.
            payload["distanceM"] = nearest_range
        seen_ids.add(c2_id)
        payloads.append({key: value for key, value in payload.items() if value is not None})
    return payloads


def build_standard_detection_payloads(snapshot: dict[str, Any]) -> list[dict[str, Any]]:
    """Project detections onto the documented ``POST /api/cuas`` schema only."""

    projected: list[dict[str, Any]] = []
    for payload in build_detection_payloads(snapshot):
        item = {key: value for key, value in payload.items() if key in STANDARD_DETECTION_FIELDS}
        methods: list[str] = []
        for modality in payload.get("activeModalities", []):
            method = STANDARD_SENSOR_TYPES.get(str(modality))
            if method and method not in methods:
                methods.append(method)
        if methods:
            item["det"] = "+".join(methods)
        projected.append(item)
    return projected


def read_snapshot(path: Path) -> dict[str, Any]:
    try:
        size = path.stat().st_size
        if size > MAX_SNAPSHOT_BYTES:
            raise BridgeError(f"snapshot is {size} bytes; limit is {MAX_SNAPSHOT_BYTES} bytes")
        with path.open("r", encoding="utf-8") as handle:
            snapshot = json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        raise BridgeError(f"cannot read {path}: {exc}") from exc

    if not isinstance(snapshot, dict):
        raise BridgeError("snapshot root must be a JSON object")
    schema = snapshot.get("schemaVersion")
    if schema != EXPECTED_SCHEMA:
        raise BridgeError(f"unsupported snapshot schema: {schema!r}")
    if snapshot.get("simulationOnly") is not True or snapshot.get("detectionOnly") is not True:
        raise BridgeError("snapshot must explicitly declare simulationOnly and detectionOnly")
    required_safety_values = {
        "suppliedRFModelsUsed": False,
        "legacyUnrealDetectedRFLinksConsumed": True,
        "rawUnrealDetectedRFLinksConsumed": True,
        "calibratedOperationalSystem": False,
        "actionsTaken": "none",
    }
    for key, expected in required_safety_values.items():
        if snapshot.get(key) != expected:
            raise BridgeError(f"snapshot safety field {key} must be {expected!r}")
    authority = snapshot.get("observationAuthority")
    if not isinstance(authority, Mapping):
        raise BridgeError("snapshot is missing observationAuthority")
    required_authority_values = {
        "trackSeedPolicy": "current_validated_sensor_observations_only",
        "scenarioTargetsUsedAsDetectionSeeds": False,
        "scenarioTargetsUsedForOperationalState": False,
        "rawUnrealDetectedRFLinksConsumed": True,
        "truthRegeneratedRFEnabled": False,
        "truthRegeneratedMmWaveEnabled": False,
        "rfOnlyTruthPositionFallbackEnabled": False,
    }
    for key, expected in required_authority_values.items():
        if authority.get(key) != expected:
            raise BridgeError(f"snapshot observationAuthority.{key} must be {expected!r}")
    for key in ("timestampUtc", "sourceUnrealSnapshotTimestampUtc"):
        value = _text(snapshot.get(key))
        if not value:
            raise BridgeError(f"snapshot is missing {key}")
        try:
            datetime.fromisoformat(value.replace("Z", "+00:00"))
        except ValueError as exc:
            raise BridgeError(f"snapshot {key} is not a valid ISO-8601 timestamp") from exc
    return snapshot


def snapshot_age_seconds(snapshot: Mapping[str, Any], path: Path, *, now: float | None = None) -> float:
    """Return the oldest age across file, layered, and source-Unreal timestamps."""
    current = time.time() if now is None else now
    ages = [current - path.stat().st_mtime]
    for key in ("timestampUtc", "sourceUnrealSnapshotTimestampUtc"):
        value = _text(snapshot.get(key))
        if not value:
            raise BridgeError(f"snapshot is missing {key}")
        try:
            parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
        except ValueError as exc:
            raise BridgeError(f"snapshot {key} is not a valid ISO-8601 timestamp") from exc
        if parsed.tzinfo is None:
            parsed = parsed.replace(tzinfo=timezone.utc)
        delta = current - parsed.timestamp()
        if delta < -5.0:
            raise BridgeError(f"snapshot {key} is more than 5 seconds in the future")
        ages.append(max(0.0, delta))
    return max(ages)


class C2Client:
    def __init__(
        self,
        base_url: str,
        timeout: float,
        attempts: int = 3,
        retry_delay: float = 0.2,
    ) -> None:
        self.base_url = base_url.rstrip("/")
        self.timeout = timeout
        self.attempts = max(1, attempts)
        self.retry_delay = max(0.0, retry_delay)

    def _request(
        self, method: str, path: str, payload: dict[str, Any] | None = None
    ) -> dict[str, Any]:
        data = None
        headers = {"Accept": "application/json"}
        if payload is not None:
            data = json.dumps(payload, separators=(",", ":")).encode("utf-8")
            headers["Content-Type"] = "application/json"
        request = Request(f"{self.base_url}{path}", data=data, headers=headers, method=method)
        body = b""
        for attempt in range(self.attempts):
            try:
                with urlopen(request, timeout=self.timeout) as response:
                    body = response.read(MAX_SNAPSHOT_BYTES)
                break
            except HTTPError as exc:
                detail = exc.read(1024).decode("utf-8", errors="replace")
                if exc.code not in RETRYABLE_HTTP_STATUS or attempt + 1 >= self.attempts:
                    raise C2HTTPError(exc.code, detail) from exc
                LOGGER.warning(
                    "C2 %s %s returned HTTP %d; retrying (%d/%d)",
                    method,
                    path,
                    exc.code,
                    attempt + 2,
                    self.attempts,
                )
            except (URLError, TimeoutError, OSError) as exc:
                if attempt + 1 >= self.attempts:
                    raise C2Error(f"cannot reach C2 at {self.base_url}: {exc}") from exc
                LOGGER.warning(
                    "cannot reach C2 at %s; retrying %s %s (%d/%d)",
                    self.base_url,
                    method,
                    path,
                    attempt + 2,
                    self.attempts,
                )
            if self.retry_delay > 0.0:
                time.sleep(self.retry_delay * (2**attempt))
        if not body:
            return {}
        try:
            decoded = json.loads(body)
        except json.JSONDecodeError as exc:
            raise C2Error("C2 returned invalid JSON") from exc
        return decoded if isinstance(decoded, dict) else {}

    def get(self, path: str) -> dict[str, Any]:
        return self._request("GET", path)

    def post(self, path: str, payload: dict[str, Any]) -> dict[str, Any]:
        return self._request("POST", path, payload)

    def delete(self, path: str, identifier: str) -> None:
        self._request("DELETE", f"{path}?{urlencode({'id': identifier})}")


class TriadBridge:
    def __init__(self, client: C2Client, api_profile: str = "standard") -> None:
        if api_profile not in API_PROFILES:
            raise ValueError(f"unsupported API profile: {api_profile}")
        self.client = client
        self.api_profile = api_profile
        self.active_detection_ids: set[str] = set()
        self.registered_sensor_ids: set[str] = set()

    def discover_existing(self) -> None:
        detections = self.client.get("/api/cuas").get("detections", [])
        sensors = self.client.get("/api/cuas/sensor").get("sensors", [])
        self.active_detection_ids = _ids_with_prefix(detections)
        self.registered_sensor_ids = _ids_with_prefix(sensors)

    def send_sensor_heartbeats(
        self,
        snapshot: dict[str, Any],
        *,
        source_fresh: bool = True,
    ) -> int:
        if self.api_profile == "standard":
            payloads = build_standard_sensor_payloads(snapshot) if source_fresh else []
        else:
            payloads = build_sensor_payloads(snapshot)
            context = build_operating_context_payload(snapshot)
            if context is not None:
                self.client.post("/api/cuas/context", context)
        current_ids = {payload["id"] for payload in payloads}
        for payload in payloads:
            self.client.post("/api/cuas/sensor", payload)
        for sensor_id in self.registered_sensor_ids - current_ids:
            self.client.delete("/api/cuas/sensor", sensor_id)
        self.registered_sensor_ids = current_ids
        return len(payloads)

    def send_detections(self, snapshot: dict[str, Any]) -> int:
        payloads = (
            build_standard_detection_payloads(snapshot)
            if self.api_profile == "standard"
            else build_detection_payloads(snapshot)
        )
        current_ids = {payload["id"] for payload in payloads}
        successful_ids: set[str] = set()
        failed_delete_ids: set[str] = set()
        errors: list[str] = []
        for payload in payloads:
            try:
                self.client.post("/api/cuas", payload)
                successful_ids.add(payload["id"])
            except BridgeError as exc:
                errors.append(str(exc))

        for detection_id in self.active_detection_ids - current_ids:
            try:
                self.client.delete("/api/cuas", detection_id)
            except BridgeError as exc:
                errors.append(str(exc))
                failed_delete_ids.add(detection_id)

        self.active_detection_ids = (
            self.active_detection_ids.intersection(current_ids)
            | successful_ids
            | failed_delete_ids
        )
        if errors:
            raise C2Error(f"{len(errors)} C2 request(s) failed; first error: {errors[0]}")
        return len(successful_ids)

    def clear_detections(self) -> None:
        errors = []
        for detection_id in self.active_detection_ids:
            try:
                self.client.delete("/api/cuas", detection_id)
            except BridgeError as exc:
                errors.append(str(exc))
        if not errors:
            self.active_detection_ids.clear()
        else:
            raise C2Error(f"failed to clear {len(errors)} TRIAD detection(s): {errors[0]}")


def _ids_with_prefix(items: Any) -> set[str]:
    if not isinstance(items, list):
        return set()
    return {
        identifier
        for item in items
        if isinstance(item, dict)
        and (identifier := _text(item.get("id"))) is not None
        and identifier.startswith(ID_PREFIX)
    }


def run_once(snapshot_path: Path, bridge: TriadBridge, stale_after: float = 5.0) -> None:
    bridge.discover_existing()
    try:
        snapshot = read_snapshot(snapshot_path)
        age = snapshot_age_seconds(snapshot, snapshot_path)
    except (BridgeError, OSError):
        if bridge.active_detection_ids:
            bridge.clear_detections()
        raise
    if age > stale_after:
        if bridge.active_detection_ids:
            bridge.clear_detections()
        sensor_count = bridge.send_sensor_heartbeats(snapshot, source_fresh=False)
        raise BridgeError(
            f"TRIAD snapshot source is {age:.1f}s old; registered {sensor_count} stale/offline sensors but refused detections"
        )
    try:
        sensor_count = bridge.send_sensor_heartbeats(snapshot)
        detection_count = bridge.send_detections(snapshot)
    except BridgeError as exc:
        try:
            if bridge.active_detection_ids:
                bridge.clear_detections()
        except BridgeError as clear_exc:
            raise BridgeError(f"{exc}; additionally failed to clear detections: {clear_exc}") from exc
        raise
    LOGGER.info(
        "forwarded snapshot %s: %d sensors, %d active cues/detections",
        snapshot.get("timestampUtc", "(no timestamp)"),
        sensor_count,
        detection_count,
    )


def run_forever(
    snapshot_path: Path,
    bridge: TriadBridge,
    interval: float,
    sensor_heartbeat: float,
    stale_after: float,
) -> None:
    last_snapshot_token: tuple[int, int] | None = None
    next_sensor_heartbeat = 0.0
    stale_cleared = False
    c2_discovered = False
    reconnect_delay = 0.5

    while True:
        try:
            if not c2_discovered:
                bridge.discover_existing()
                c2_discovered = True
                reconnect_delay = 0.5
                last_snapshot_token = None
                next_sensor_heartbeat = 0.0
                LOGGER.info("connected to C2 at %s", bridge.client.base_url)
            stat = snapshot_path.stat()
            file_age = max(0.0, time.time() - stat.st_mtime)
            token = (stat.st_mtime_ns, stat.st_size)
            if file_age > stale_after:
                if not stale_cleared and bridge.active_detection_ids:
                    bridge.clear_detections()
                    LOGGER.warning("TRIAD snapshot file is %.1fs old; cleared live detections", file_age)
                if token != last_snapshot_token:
                    snapshot = read_snapshot(snapshot_path)
                    sensor_count = bridge.send_sensor_heartbeats(snapshot, source_fresh=False)
                    last_snapshot_token = token
                    LOGGER.warning(
                        "registered %d sensor records with stale source timestamps; they remain OFFLINE",
                        sensor_count,
                    )
                stale_cleared = True
                time.sleep(interval)
                continue

            if token == last_snapshot_token:
                time.sleep(interval)
                continue

            snapshot = read_snapshot(snapshot_path)
            source_age = snapshot_age_seconds(snapshot, snapshot_path)
            if source_age > stale_after:
                if not stale_cleared and bridge.active_detection_ids:
                    bridge.clear_detections()
                sensor_count = bridge.send_sensor_heartbeats(snapshot, source_fresh=False)
                LOGGER.warning(
                    "TRIAD layered/source timestamp is %.1fs old; registered %d OFFLINE sensors, refused cues",
                    source_age,
                    sensor_count,
                )
                stale_cleared = True
                last_snapshot_token = token
                time.sleep(interval)
                continue
            now = time.monotonic()
            if now >= next_sensor_heartbeat:
                sensor_count = bridge.send_sensor_heartbeats(snapshot)
                next_sensor_heartbeat = now + sensor_heartbeat
            else:
                sensor_count = 0
            detection_count = bridge.send_detections(snapshot)
            last_snapshot_token = token
            stale_cleared = False
            LOGGER.info(
                "snapshot %s: %d sensor heartbeat(s), %d active cues/detections",
                snapshot.get("timestampUtc", "(no timestamp)"),
                sensor_count,
                detection_count,
            )
        except C2Error as exc:
            LOGGER.error("%s", exc)
            c2_discovered = False
            last_snapshot_token = None
            next_sensor_heartbeat = 0.0
            time.sleep(reconnect_delay)
            reconnect_delay = min(10.0, reconnect_delay * 2.0)
            continue
        except (BridgeError, OSError) as exc:
            LOGGER.error("%s", exc)
            if bridge.active_detection_ids:
                try:
                    bridge.clear_detections()
                    LOGGER.warning("cleared TRIAD detections after source failure")
                except C2Error as clear_exc:
                    LOGGER.error("failed to clear detections after error: %s", clear_exc)
                    c2_discovered = False
                    last_snapshot_token = None
        except KeyboardInterrupt:
            LOGGER.info("bridge stopped")
            return
        time.sleep(interval)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Forward TRIAD layered telemetry to a local C2 API")
    parser.add_argument("--snapshot", type=Path, default=DEFAULT_SNAPSHOT)
    parser.add_argument(
        "--c2",
        default=os.environ.get("TRIAD_C2_URL", DEFAULT_C2_URL),
        help="C2 base URL (or TRIAD_C2_URL)",
    )
    parser.add_argument(
        "--api-profile",
        choices=sorted(API_PROFILES),
        default=os.environ.get("TRIAD_C2_API_PROFILE", "standard"),
        help="standard uses only the attached API; globe enables TRIAD UI extensions",
    )
    parser.add_argument("--interval", type=float, default=0.5, help="poll interval in seconds")
    parser.add_argument("--sensor-heartbeat", type=float, default=30.0, help="sensor heartbeat period")
    parser.add_argument(
        "--stale-after",
        type=float,
        default=5.0,
        help="clear detections when the snapshot is older than this many seconds",
    )
    parser.add_argument("--timeout", type=float, default=3.0, help="HTTP timeout")
    parser.add_argument(
        "--request-attempts",
        type=int,
        default=3,
        help="attempts per transient C2 request",
    )
    parser.add_argument(
        "--retry-delay",
        type=float,
        default=0.2,
        help="initial delay between transient C2 request attempts",
    )
    parser.add_argument("--once", action="store_true", help="forward one snapshot and exit")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()
    for name in ("interval", "sensor_heartbeat", "stale_after", "timeout"):
        if getattr(args, name) <= 0:
            parser.error(f"--{name.replace('_', '-')} must be greater than zero")
    if args.request_attempts <= 0:
        parser.error("--request-attempts must be greater than zero")
    if args.retry_delay < 0:
        parser.error("--retry-delay must be zero or greater")
    return args


def main() -> int:
    args = parse_args()
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
    )
    bridge = TriadBridge(
        C2Client(
            args.c2,
            args.timeout,
            attempts=args.request_attempts,
            retry_delay=args.retry_delay,
        ),
        api_profile=args.api_profile,
    )
    try:
        if args.once:
            run_once(args.snapshot, bridge, args.stale_after)
        else:
            run_forever(args.snapshot, bridge, args.interval, args.sensor_heartbeat, args.stale_after)
    except BridgeError as exc:
        LOGGER.error("%s", exc)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
