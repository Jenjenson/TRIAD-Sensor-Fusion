"""Read-only localhost bridge from Unreal JSONL exports to the C3 dashboard.

The bridge deliberately does not manufacture detector confidence values or run
the fusion-v2 policy. Unreal RF link gates and multi-node preliminary cues are
exposed as their own simulation decisions without inferring hostility, while
the audited multimodal fusion status is reported as not evaluated until
target-associated model scores are available.

Only loopback clients are supported by default.  The HTTP surface is bounded,
has permissive CORS for the local dashboard, and contains no mutation or
engagement endpoints.
"""

from __future__ import annotations

import argparse
from collections import defaultdict
from dataclasses import dataclass, field
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
import mimetypes
from pathlib import Path
import threading
import time
from typing import Any, Iterable, Mapping
from urllib.parse import unquote, urlparse
import zlib

from singapore_sensor_fusion.paths import DEFAULT_TRIAD_SENSOR_SAVED_DIR

DEFAULT_SAVED_DIR = DEFAULT_TRIAD_SENSOR_SAVED_DIR
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8765
SNAPSHOT_SCHEMA_VERSION = "1.0"
FRESH_AFTER_SECONDS = 5.0
STALE_AFTER_SECONDS = 60.0
MAX_JSONL_BYTES = 8 * 1024 * 1024
MAX_RF_RECORDS = 8192
MAX_ALERT_RECORDS = 1024
MAX_TRACKS = 64
MAX_SCENARIO_TARGETS = 64
MAX_RF_LINKS_PER_TRACK = 16
MAX_OAK_REPORT_BYTES = 8 * 1024 * 1024
MAX_OAK_REPORT_FRAMES = 512
MAX_MODEL_DETECTIONS_PER_FRAME = 64
MODEL_FRAME_MAX_AGE_SECONDS = 10.0
VISIBLE_FRAME_NODES = ("City_Sector", "South_Sector")
LAYERED_SNAPSHOT_SCHEMA = "triad.layered_detection_snapshot.v1"

FUSION_METHOD = "max_weather_discounted_evidence_arbitrary_dependence_v2"
FUSION_SCORE_SEMANTICS = (
    "Confidence-discounted evidence index in [0,1]; not a calibrated probability. "
    "Corroborating sensors satisfy policy gates but are not numerically stacked "
    "without a validated dependence model."
)

NODE_REFERENCE: tuple[dict[str, Any], ...] = (
    {"id": "West_Sector", "short": "WEST", "latitudeDegrees": 1.321, "longitudeDegrees": 103.650, "nominalRadiusKm": 20.0},
    {"id": "Jurong_Sector", "short": "JURONG", "latitudeDegrees": 1.335, "longitudeDegrees": 103.705, "nominalRadiusKm": 20.0},
    {"id": "North_Sector", "short": "NORTH", "latitudeDegrees": 1.435, "longitudeDegrees": 103.786, "nominalRadiusKm": 20.0},
    {"id": "NorthEast_Sector", "short": "NORTH-EAST", "latitudeDegrees": 1.405, "longitudeDegrees": 103.902, "nominalRadiusKm": 20.0},
    {"id": "East_Sector", "short": "EAST", "latitudeDegrees": 1.357, "longitudeDegrees": 103.988, "nominalRadiusKm": 20.0},
    {"id": "Central_Sector", "short": "CENTRAL", "latitudeDegrees": 1.345, "longitudeDegrees": 103.780, "nominalRadiusKm": 20.0},
    {"id": "City_Sector", "short": "CITY", "latitudeDegrees": 1.286, "longitudeDegrees": 103.860, "nominalRadiusKm": 20.0},
    {"id": "South_Sector", "short": "SOUTH", "latitudeDegrees": 1.254, "longitudeDegrees": 103.823, "nominalRadiusKm": 20.0},
)


def _utc_now() -> datetime:
    return datetime.now(timezone.utc)


def _utc_text(value: datetime) -> str:
    return value.astimezone(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def _parse_utc(value: object) -> datetime | None:
    if not isinstance(value, str) or not value.strip():
        return None
    try:
        parsed = datetime.fromisoformat(value.strip().replace("Z", "+00:00"))
    except ValueError:
        return None
    if parsed.tzinfo is None:
        parsed = parsed.replace(tzinfo=timezone.utc)
    return parsed.astimezone(timezone.utc)


def _finite(value: object) -> float | None:
    if isinstance(value, bool):
        return None
    try:
        result = float(value)
    except (TypeError, ValueError):
        return None
    return result if math.isfinite(result) else None


def _round(value: object, digits: int = 3) -> float | None:
    finite = _finite(value)
    return None if finite is None else round(finite, digits)


def _unreal_tchar_crc32(value: str) -> int:
    """Mirror FCrc::StrCrc32 for the ASCII-like configured UE identifiers.

    Unreal's implementation normalizes each TCHAR to four little-endian bytes.
    The configured actor/emitter identifiers are ASCII, so UTF-32LE produces the
    same stable CRC without exposing the source value.
    """

    return zlib.crc32(value.encode("utf-32le")) & 0xFFFFFFFF


def _operator_rf_track_id(target_actor: object, emitter_id: object) -> str:
    """Return the same opaque deterministic cue ID as the Unreal plugin."""

    target_seed = target_actor if isinstance(target_actor, str) and target_actor else "unknown-target"
    emitter_seed = emitter_id if isinstance(emitter_id, str) and emitter_id else "unknown-emitter"
    seed = f"{target_seed}|{emitter_seed}"
    first = _unreal_tchar_crc32(seed)
    second = _unreal_tchar_crc32(f"triad-rf-cue-v1|{seed}")
    return f"RF-CUE-{first:08X}-{second:08X}"


def _is_operator_rf_track_id(value: object) -> bool:
    if not isinstance(value, str):
        return False
    parts = value.split("-")
    return (
        len(parts) == 4
        and parts[:2] == ["RF", "CUE"]
        and all(len(part) == 8 and all(char in "0123456789ABCDEF" for char in part) for part in parts[2:])
    )


def _preliminary_cue_evidence(record: Mapping[str, Any]) -> bool:
    """Use v2 cue evidence, or migrate older records from the RF gate itself."""

    if "preliminaryCueEvidence" in record:
        return record.get("preliminaryCueEvidence") is True
    return record.get("detected") is True


def _airspace_state(value: object) -> str | None:
    if not isinstance(value, str):
        return None
    normalized = value.strip().upper()
    return normalized if normalized in {"APPROACHING", "DEPARTING", "OUTSIDE", "INSIDE"} else None


def _neutral_ingress_corridor(value: object) -> str | None:
    """Expose only cardinal geometry, never authored identity/class labels."""

    if not isinstance(value, str):
        return None
    upper = value.strip().upper()
    for cardinal in ("EAST", "WEST", "NORTH", "SOUTH"):
        if cardinal in upper:
            return f"{cardinal}_INBOUND"
    return None


def _approach_fields(record: Mapping[str, Any]) -> dict[str, Any]:
    """Normalize optional Unreal approach metadata without inventing motion."""

    outside = record.get("outsideSimulationPerimeter")
    return {
        "airspaceState": _airspace_state(record.get("airspaceState")),
        "distanceToPerimeterMeters": _round(record.get("distanceToPerimeterMeters")),
        "approachRateMetersPerSecond": _round(record.get("approachRateMetersPerSecond")),
        "headingDegrees": _round(record.get("headingDegrees")),
        "speedMetersPerSecond": _round(record.get("speedMetersPerSecond")),
        "outsideSimulationPerimeter": outside if isinstance(outside, bool) else None,
        "ingressCorridorId": _neutral_ingress_corridor(record.get("ingressCorridorId")),
    }


def _age_seconds(timestamp: datetime | None, now: datetime) -> float | None:
    if timestamp is None:
        return None
    return max(0.0, round((now - timestamp).total_seconds(), 3))


def _source_state(age_seconds: float | None) -> str:
    if age_seconds is None:
        return "unavailable"
    if age_seconds <= FRESH_AFTER_SECONDS:
        return "fresh"
    if age_seconds <= STALE_AFTER_SECONDS:
        return "delayed"
    return "stale"


def _newest_file(root: Path, pattern: str) -> Path | None:
    try:
        files = (item for item in root.glob(pattern) if item.is_file())
        return max(files, key=lambda item: (item.stat().st_mtime_ns, item.name), default=None)
    except OSError:
        return None


def _load_oak_model_report(
    path: Path | None,
    now: datetime,
) -> tuple[dict[tuple[str, str], dict[str, Any]], dict[str, Any]]:
    """Read one bounded, atomically published OAK watcher report.

    Records are keyed by the exact ``(nodeId, frameId)`` pair.  The bridge
    never associates these image detections to an RF target actor.
    """

    records: dict[tuple[str, str], dict[str, Any]] = {}
    error: str | None = None
    payload: dict[str, Any] | None = None
    if path is not None:
        try:
            if path.stat().st_size > MAX_OAK_REPORT_BYTES:
                raise ValueError("OAK model report exceeds bounded read limit")
            candidate = json.loads(path.read_text(encoding="utf-8-sig"))
            if not isinstance(candidate, dict):
                raise ValueError("OAK model report root is not an object")
            if candidate.get("schemaVersion") != "1.0" or candidate.get("mode") != "bounded_live_unreal_oak_rgbd_watch":
                raise ValueError("OAK model report schema or mode is unsupported")
            if not isinstance(candidate.get("processedFrames"), list):
                raise ValueError("OAK model report processedFrames is not a list")
            payload = candidate
        except FileNotFoundError:
            error = "not written yet"
        except (OSError, UnicodeDecodeError, json.JSONDecodeError, ValueError) as exc:
            error = str(exc)

    raw_frames = (payload or {}).get("processedFrames")
    if isinstance(raw_frames, list):
        for raw in raw_frames[-MAX_OAK_REPORT_FRAMES:]:
            if not isinstance(raw, dict):
                continue
            node_id = raw.get("nodeId")
            frame_id = raw.get("frameId")
            if not isinstance(node_id, str) or not node_id or not isinstance(frame_id, str) or not frame_id:
                continue
            records[(node_id, frame_id)] = raw

    newest_timestamp = max(
        (_parse_utc(item.get("timestampUtc")) for item in records.values()),
        default=None,
        key=lambda value: value or datetime.min.replace(tzinfo=timezone.utc),
    )
    age = _age_seconds(newest_timestamp, now)
    return records, {
        "available": payload is not None,
        "fileName": path.name if path is not None and path.exists() else None,
        "pathConfigured": path is not None,
        "reportState": (payload or {}).get("reportState"),
        "stopReason": (payload or {}).get("stopReason"),
        "recordsRetained": len(records),
        "recordsBound": MAX_OAK_REPORT_FRAMES,
        "lastFrameAtUtc": _utc_text(newest_timestamp) if newest_timestamp is not None else None,
        "ageSeconds": age,
        "freshness": _source_state(age),
        "exactFrameMatchRequired": True,
        "targetAssociationAvailable": False,
        "error": error,
    }


def _node_reference(saved_dir: Path) -> tuple[tuple[dict[str, Any], ...], str]:
    """Prefer the active Unreal configuration while retaining a safe fallback."""

    config_path = saved_dir.parent.parent / "Config" / "SingaporeSensorFusion.json"
    try:
        payload = json.loads(config_path.read_text(encoding="utf-8-sig"))
        configured = payload.get("SensorNodes") if isinstance(payload, dict) else None
        if not isinstance(configured, list):
            raise ValueError("SensorNodes is not a list")
        short_by_id = {item["id"]: item["short"] for item in NODE_REFERENCE}
        nodes: list[dict[str, Any]] = []
        for raw in configured:
            if not isinstance(raw, dict) or raw.get("bEnabled") is False:
                continue
            node_id = raw.get("NodeId")
            latitude = _finite(raw.get("LatitudeDegrees"))
            longitude = _finite(raw.get("LongitudeDegrees"))
            range_m = _finite(raw.get("DetectionRangeMeters"))
            if not isinstance(node_id, str) or latitude is None or longitude is None or range_m is None:
                continue
            nodes.append({
                "id": node_id,
                "short": short_by_id.get(node_id, node_id.removesuffix("_Sector").upper()),
                "latitudeDegrees": latitude,
                "longitudeDegrees": longitude,
                "nominalRadiusKm": round(range_m / 1000.0, 3),
                "configuredHeightMeters": _round(raw.get("HeightMeters")),
            })
        if nodes:
            return tuple(nodes), str(config_path)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, ValueError):
        pass
    return NODE_REFERENCE, "built-in fallback matching the 20 km active-node range"


def _tail_jsonl(
    path: Path | None,
    *,
    max_records: int,
    max_bytes: int = MAX_JSONL_BYTES,
) -> tuple[list[dict[str, Any]], dict[str, int | bool]]:
    """Read a bounded tail, ignoring a partial first or final line."""

    if path is None:
        return [], {"bytesRead": 0, "recordsParsed": 0, "invalidLines": 0, "truncatedAtStart": False}
    try:
        file_size = path.stat().st_size
        start = max(0, file_size - max_bytes)
        with path.open("rb") as stream:
            stream.seek(start)
            payload = stream.read(max_bytes)
    except OSError:
        return [], {"bytesRead": 0, "recordsParsed": 0, "invalidLines": 0, "truncatedAtStart": False}

    truncated = start > 0
    if truncated:
        separator = payload.find(b"\n")
        payload = b"" if separator < 0 else payload[separator + 1 :]
    lines = payload.splitlines()
    parsed: list[dict[str, Any]] = []
    invalid = 0
    for raw_line in lines:
        if not raw_line.strip():
            continue
        try:
            item = json.loads(raw_line.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            invalid += 1
            continue
        if isinstance(item, dict):
            parsed.append(item)
        else:
            invalid += 1
    if len(parsed) > max_records:
        parsed = parsed[-max_records:]
        truncated = True
    return parsed, {
        "bytesRead": len(payload),
        "recordsParsed": len(parsed),
        "invalidLines": invalid,
        "truncatedAtStart": truncated,
    }


def _load_live_rf_snapshot(
    saved_dir: Path,
    now: datetime,
) -> tuple[dict[str, Any] | None, dict[str, Any]]:
    """Load the plugin's atomic current-sample snapshot when it is fresh."""

    path = saved_dir / "latest_rf_snapshot.json"
    payload: dict[str, Any] | None = None
    error: str | None = None
    try:
        if path.stat().st_size > MAX_JSONL_BYTES:
            raise ValueError("snapshot exceeds bounded read limit")
        candidate = json.loads(path.read_text(encoding="utf-8-sig"))
        if not isinstance(candidate, dict):
            raise ValueError("snapshot root is not an object")
        if candidate.get("schemaVersion") not in {
            "triad.live_rf_snapshot.v1",
            "triad.live_rf_snapshot.v2",
            "triad.live_rf_snapshot.v3",
        }:
            raise ValueError("snapshot schema is unsupported")
        if candidate.get("sampleComplete") is not True:
            raise ValueError("snapshot sample is incomplete")
        if candidate.get("detectionOnly") is not True or candidate.get("actionsTaken") != "none":
            raise ValueError("snapshot safety semantics are invalid")
        payload = candidate
    except FileNotFoundError:
        error = "not written yet"
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, ValueError) as exc:
        error = str(exc)

    timestamp = _parse_utc((payload or {}).get("timestampUtc"))
    age = _age_seconds(timestamp, now)
    cadence = _finite((payload or {}).get("sampleCadenceSeconds")) or 0.25
    preferred_until = max(FRESH_AFTER_SECONDS, cadence * 8.0)
    fresh = payload is not None and age is not None and age <= preferred_until
    source = {
        "available": payload is not None,
        "selected": fresh,
        "fileName": path.name if path.exists() else None,
        "lastRecordAtUtc": _utc_text(timestamp) if timestamp is not None else None,
        "ageSeconds": age,
        "freshness": _source_state(age),
        "preferredUntilSeconds": preferred_until,
        "sampleComplete": (payload or {}).get("sampleComplete"),
        "schemaVersion": (payload or {}).get("schemaVersion"),
        "detectedLinksTruncated": (payload or {}).get("detectedLinksTruncated"),
        "calibratedOperationalSystem": (payload or {}).get("calibratedOperationalSystem"),
        "calibrationStatus": (payload or {}).get("calibrationStatus"),
        "error": error,
    }
    return (payload if fresh else None), source


def _load_layered_snapshot(
    saved_dir: Path,
    now: datetime,
) -> tuple[dict[str, Any] | None, dict[str, Any]]:
    """Load the fail-closed Python fusion projection for C2 augmentation."""

    path = saved_dir / "latest_layered_snapshot.json"
    payload: dict[str, Any] | None = None
    error: str | None = None
    try:
        if path.stat().st_size > MAX_JSONL_BYTES:
            raise ValueError("layered snapshot exceeds bounded read limit")
        candidate = json.loads(path.read_text(encoding="utf-8-sig"))
        if not isinstance(candidate, dict):
            raise ValueError("layered snapshot root is not an object")
        if candidate.get("schemaVersion") != LAYERED_SNAPSHOT_SCHEMA:
            raise ValueError("layered snapshot schema is unsupported")
        if candidate.get("simulationOnly") is not True:
            raise ValueError("layered snapshot is not marked simulationOnly")
        if candidate.get("detectionOnly") is not True or candidate.get("actionsTaken") != "none":
            raise ValueError("layered snapshot safety semantics are invalid")
        if not isinstance(candidate.get("tracks"), list):
            raise ValueError("layered snapshot tracks is not an array")
        payload = candidate
    except FileNotFoundError:
        error = "not written yet"
    except (OSError, UnicodeDecodeError, json.JSONDecodeError, ValueError) as exc:
        error = str(exc)

    timestamp = _parse_utc((payload or {}).get("timestampUtc"))
    age = _age_seconds(timestamp, now)
    fresh = payload is not None and age is not None and age <= FRESH_AFTER_SECONDS
    source = {
        "available": payload is not None,
        "selected": fresh,
        "fileName": path.name if path.exists() else None,
        "lastRecordAtUtc": _utc_text(timestamp) if timestamp is not None else None,
        "ageSeconds": age,
        "freshness": _source_state(age),
        "schemaVersion": (payload or {}).get("schemaVersion"),
        "sourceUnrealSchemaVersion": (payload or {}).get("sourceUnrealSchemaVersion"),
        "error": error,
    }
    return (payload if fresh else None), source


def _records_from_live_snapshot(
    payload: Mapping[str, Any],
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    """Normalize current detected-only links and track decisions for the UI."""

    timestamp = payload.get("timestampUtc")
    weather = payload.get("weather") if isinstance(payload.get("weather"), dict) else {}
    raw_links = payload.get("detectedRFLinks") if isinstance(payload.get("detectedRFLinks"), list) else []
    links: list[dict[str, Any]] = []
    for raw in raw_links[:4096]:
        if not isinstance(raw, dict):
            continue
        preliminary_cue_evidence = _preliminary_cue_evidence(raw)
        links.append({
            "timestampUtc": raw.get("timestampUtc") or timestamp,
            "nodeId": raw.get("nodeId"),
            "targetActor": raw.get("targetActor"),
            "emitterId": raw.get("emitterId"),
            "frequencyGHz": raw.get("frequencyGHz"),
            "nodeLongitudeDegrees": raw.get("nodeLongitudeDegrees"),
            "nodeLatitudeDegrees": raw.get("nodeLatitudeDegrees"),
            "nodeHeightMeters": raw.get("nodeHeightMeters"),
            "targetLongitudeDegrees": raw.get("targetLongitudeDegrees"),
            "targetLatitudeDegrees": raw.get("targetLatitudeDegrees"),
            "targetHeightMeters": raw.get("targetHeightMeters"),
            "distanceMeters": raw.get("slantRangeMeters"),
            "lineOfSight": raw.get("lineOfSight"),
            "blockingActor": raw.get("blockingActor"),
            "weatherProfile": weather.get("profile"),
            "airSimVisualWeatherApplied": weather.get("airSimVisualWeatherApplied"),
            "weatherRainRateMillimetersPerHour": weather.get("rainRateMillimetersPerHour"),
            "weatherVisibilityMeters": weather.get("visibilityMeters"),
            "weatherRFSpecificAttenuationDbPerKm": (
                weather.get("rfSpecificAttenuationDbPerKmAt2_4GHz")
                if _band_key(raw.get("frequencyGHz")) == "rf24"
                else weather.get("rfSpecificAttenuationDbPerKmAt5_8GHz")
            ),
            "weatherRFLossDb": raw.get("weatherRFLossDb"),
            "receivedPowerDbm": raw.get("receivedPowerDbm"),
            "noiseFloorDbm": raw.get("noiseFloorDbm"),
            "snrDb": raw.get("snrDb"),
            "detected": raw.get("detected") is True,
            "preliminaryCueEvidence": preliminary_cue_evidence,
            # Compatibility alias; truth-independent in snapshot v2.
            "alertEvidence": preliminary_cue_evidence,
            **_approach_fields(raw),
        })

    raw_tracks = payload.get("tracks") if isinstance(payload.get("tracks"), list) else []
    alerts: list[dict[str, Any]] = []
    minimum_nodes_value = payload.get(
        "rfCueMinimumConfirmingNodes",
        payload.get("alertMinimumConfirmingNodes", 2),
    )
    minimum_nodes = (
        max(1, minimum_nodes_value)
        if isinstance(minimum_nodes_value, int) and not isinstance(minimum_nodes_value, bool)
        else 2
    )
    for raw in raw_tracks[:MAX_TRACKS]:
        if not isinstance(raw, dict):
            continue
        target = raw.get("targetActor")
        emitter_id = raw.get("emitterId")
        node_ids = raw.get("preliminaryCueEvidenceNodeIds")
        if not isinstance(node_ids, list):
            node_ids = raw.get("confirmingNodeIds") if isinstance(raw.get("confirmingNodeIds"), list) else []
        node_ids = sorted({str(item) for item in node_ids if isinstance(item, str) and item})
        rule_value = raw.get("rfMultinodePreliminaryCueRuleSatisfied")
        # A v1 snapshot is accepted during migration, but its old truth-gated
        # decision bit is never consumed. Re-evaluate the generic node threshold.
        rule_satisfied = rule_value is True if isinstance(rule_value, bool) else len(node_ids) >= minimum_nodes
        if not rule_satisfied:
            continue
        target_links = [
            item
            for item in links
            if item.get("targetActor") == target
            and item.get("preliminaryCueEvidence") is True
        ]
        nearest = min(target_links, key=lambda item: _finite(item.get("distanceMeters")) or math.inf, default=None)
        alerts.append({
            "timestampUtc": timestamp,
            "alertType": "rf_multinode_preliminary_cue",
            "cueLevel": "preliminary",
            "classification": "unclassified_emitter",
            "hostilityAssessment": "not_inferred",
            "evidenceBasis": "rf_detection_by_distinct_nodes",
            "trackId": _operator_rf_track_id(target, emitter_id),
            # Private in-memory join key; never copied into the API track.
            "_sourceTargetActor": target,
            "confirmingNodeIds": node_ids,
            "confirmingNodeCount": len(node_ids),
            "nearestConfirmingNodeId": (nearest or {}).get("nodeId"),
            "nearestConfirmingNodeDistanceMeters": raw.get("nearestSlantRangeMeters"),
            "maxSnrDb": raw.get("strongestSnrDb"),
            "targetLongitudeDegrees": raw.get("targetLongitudeDegrees"),
            "targetLatitudeDegrees": raw.get("targetLatitudeDegrees"),
            "targetHeightMeters": raw.get("targetHeightMeters"),
            "weatherProfile": weather.get("profile"),
            "airSimVisualWeatherApplied": weather.get("airSimVisualWeatherApplied"),
            "weatherRainRateMillimetersPerHour": weather.get("rainRateMillimetersPerHour"),
            "weatherVisibilityMeters": weather.get("visibilityMeters"),
            "detectionOnly": True,
            "actionsTaken": "none",
            **_approach_fields(raw),
        })
    return links, alerts


def _weather_from_live_snapshot(payload: Mapping[str, Any]) -> dict[str, Any]:
    raw = payload.get("weather") if isinstance(payload.get("weather"), dict) else {}
    status = str(raw.get("visualVerificationStatus") or "").upper()
    verification = "verified" if status == "VERIFIED" else "failed" if status == "REQUESTED_NOT_VERIFIED" else "unavailable"
    return {
        "profile": raw.get("profile"),
        "rainRateMmH": _round(raw.get("rainRateMillimetersPerHour")),
        "visibilityMeters": _round(raw.get("visibilityMeters")),
        "visualWeatherApplied": raw.get("airSimVisualWeatherApplied"),
        "visualWeatherVerification": verification,
        "visualWeatherRequested": raw.get("airSimVisualWeatherRequested"),
        "visualWeatherInitialized": raw.get("airSimWeatherInitialized"),
        "visualWeatherActorsVerified": raw.get("airSimWeatherActorsVerified"),
        "rainAmount": _round(raw.get("rainAmount")),
        "fogAmount": _round(raw.get("fogAmount")),
        "dustAmount": _round(raw.get("dustAmount")),
        "roadWetnessAmount": _round(raw.get("roadWetnessAmount")),
        "windScenarioVector": raw.get("airSimWindScenarioVector"),
        "rfSpecificAttenuationDbPerKm": {
            "rf24": _round(raw.get("rfSpecificAttenuationDbPerKmAt2_4GHz")),
            "rf58": _round(raw.get("rfSpecificAttenuationDbPerKmAt5_8GHz")),
        },
    }


def _perimeter_from_live_snapshot(payload: Mapping[str, Any] | None) -> dict[str, Any] | None:
    if payload is None or not isinstance(payload.get("simulationPerimeter"), dict):
        return None
    raw = payload["simulationPerimeter"]
    geometry_type = str(raw.get("geometryType") or raw.get("shape") or "").strip().lower()
    common = {
        "boundaryInclusive": raw.get("boundaryInclusive") is True,
        "legalOrNationalBoundary": raw.get("legalOrNationalBoundary") is True,
        "distanceMethod": raw.get("distanceMethod"),
        "distanceSignConvention": raw.get("distanceSignConvention")
        or "positive outside, zero boundary, negative inside",
        "scopeNotice": "Scenario evaluation perimeter only; not represented as a legal or national boundary.",
    }
    if geometry_type in {"circle", "wgs84_geodesic_circle"}:
        center_lon = _round(raw.get("centerLongitudeDegrees"), 8)
        center_lat = _round(raw.get("centerLatitudeDegrees"), 8)
        radius_m = _round(raw.get("radiusMeters"), 3)
        if (
            center_lon is None
            or center_lat is None
            or radius_m is None
            or not -180.0 <= center_lon <= 180.0
            or not -90.0 <= center_lat <= 90.0
            or radius_m <= 0.0
        ):
            return None
        return {
            "geometryType": "wgs84_geodesic_circle",
            "centerLongitudeDegrees": center_lon,
            "centerLatitudeDegrees": center_lat,
            "radiusMeters": radius_m,
            **common,
        }
    bounds = {
        "minLongitudeDegrees": _round(raw.get("minimumLongitudeDegrees"), 6),
        "maxLongitudeDegrees": _round(raw.get("maximumLongitudeDegrees"), 6),
        "minLatitudeDegrees": _round(raw.get("minimumLatitudeDegrees"), 6),
        "maxLatitudeDegrees": _round(raw.get("maximumLatitudeDegrees"), 6),
    }
    # Also accept the concise key spellings during transition between plugin
    # builds; output remains stable for the dashboard.
    if bounds["minLongitudeDegrees"] is None:
        bounds["minLongitudeDegrees"] = _round(raw.get("minLongitudeDegrees", raw.get("minLon")), 6)
    if bounds["maxLongitudeDegrees"] is None:
        bounds["maxLongitudeDegrees"] = _round(raw.get("maxLongitudeDegrees", raw.get("maxLon")), 6)
    if bounds["minLatitudeDegrees"] is None:
        bounds["minLatitudeDegrees"] = _round(raw.get("minLatitudeDegrees", raw.get("minLat")), 6)
    if bounds["maxLatitudeDegrees"] is None:
        bounds["maxLatitudeDegrees"] = _round(raw.get("maxLatitudeDegrees", raw.get("maxLat")), 6)
    if any(value is None for value in bounds.values()):
        return None
    return {
        "geometryType": raw.get("geometryType") or "axis_aligned_wgs84_rectangle",
        **bounds,
        **common,
    }


def _scenario_targets_from_live_snapshot(payload: Mapping[str, Any] | None) -> list[dict[str, Any]]:
    if payload is None or not isinstance(payload.get("scenarioTargets"), list):
        return []
    result: list[dict[str, Any]] = []
    for raw in payload["scenarioTargets"][:MAX_SCENARIO_TARGETS]:
        if not isinstance(raw, dict):
            continue
        actor = raw.get("targetActor")
        if not isinstance(actor, str) or not actor:
            continue
        result.append({
            "targetActor": actor,
            "hostileScenarioTruth": raw.get("hostileScenarioTruth") if isinstance(raw.get("hostileScenarioTruth"), bool) else None,
            "inboundApproachScenario": raw.get("inboundApproachScenario") if isinstance(raw.get("inboundApproachScenario"), bool) else None,
            "kinematicsAvailable": raw.get("kinematicsAvailable") if isinstance(raw.get("kinematicsAvailable"), bool) else None,
            "detectedThisSample": raw.get("detectedThisSample") is True,
            "detectedLinkCount": int(raw.get("detectedLinkCount")) if isinstance(raw.get("detectedLinkCount"), int) and not isinstance(raw.get("detectedLinkCount"), bool) else None,
            "longitudeDegrees": _round(raw.get("targetLongitudeDegrees"), 6),
            "latitudeDegrees": _round(raw.get("targetLatitudeDegrees"), 6),
            "altitudeMeters": _round(raw.get("targetHeightMeters")),
            **_approach_fields(raw),
        })
    return result


def _latest_timestamp(records: Iterable[Mapping[str, Any]]) -> datetime | None:
    return max((_parse_utc(item.get("timestampUtc")) for item in records), default=None, key=lambda value: value or datetime.min.replace(tzinfo=timezone.utc))


def _source_info(
    path: Path | None,
    records: list[dict[str, Any]],
    stats: Mapping[str, int | bool],
    now: datetime,
) -> dict[str, Any]:
    latest = _latest_timestamp(records)
    age = _age_seconds(latest, now)
    modified: datetime | None = None
    size: int | None = None
    if path is not None:
        try:
            metadata = path.stat()
            modified = datetime.fromtimestamp(metadata.st_mtime, timezone.utc)
            size = metadata.st_size
        except OSError:
            pass
    return {
        "available": path is not None and bool(records),
        "fileName": path.name if path is not None else None,
        "fileSizeBytes": size,
        "fileModifiedAtUtc": _utc_text(modified) if modified is not None else None,
        "lastRecordAtUtc": _utc_text(latest) if latest is not None else None,
        "ageSeconds": age,
        "freshness": _source_state(age),
        **stats,
    }


def _latest_by(records: Iterable[dict[str, Any]], key_fields: tuple[str, ...]) -> list[dict[str, Any]]:
    latest: dict[tuple[object, ...], tuple[datetime, dict[str, Any]]] = {}
    floor = datetime.min.replace(tzinfo=timezone.utc)
    for record in records:
        key = tuple(record.get(field) for field in key_fields)
        timestamp = _parse_utc(record.get("timestampUtc")) or floor
        current = latest.get(key)
        if current is None or timestamp >= current[0]:
            latest[key] = (timestamp, record)
    return [item[1] for item in latest.values()]


def _band_key(frequency_ghz: object) -> str | None:
    frequency = _finite(frequency_ghz)
    if frequency is None:
        return None
    if 2.3 <= frequency <= 2.5:
        return "rf24"
    if 5.7 <= frequency <= 5.9:
        return "rf58"
    return None


def _disposition(record: Mapping[str, Any]) -> str:
    """RF detection/corroboration does not establish hostile or friendly identity."""

    del record
    return "UNKNOWN"


def _weather(records: list[dict[str, Any]]) -> dict[str, Any]:
    if not records:
        return {
            "profile": None,
            "rainRateMmH": None,
            "visibilityMeters": None,
            "visualWeatherApplied": None,
            "visualWeatherVerification": "unavailable",
        }
    newest = max(records, key=lambda item: _parse_utc(item.get("timestampUtc")) or datetime.min.replace(tzinfo=timezone.utc))
    applicable = [item for item in records if item.get("airSimVisualWeatherApplied") is not None]
    verified = bool(applicable) and all(item.get("airSimVisualWeatherApplied") is True for item in applicable)
    any_failed = any(item.get("airSimVisualWeatherApplied") is False for item in applicable)
    return {
        "profile": newest.get("weatherProfile"),
        "rainRateMmH": _round(newest.get("weatherRainRateMillimetersPerHour")),
        "visibilityMeters": _round(newest.get("weatherVisibilityMeters")),
        "visualWeatherApplied": newest.get("airSimVisualWeatherApplied"),
        "visualWeatherVerification": "verified" if verified else "failed" if any_failed else "unavailable",
        "rfSpecificAttenuationDbPerKm": {
            band: _round(
                max(
                    (
                        value
                        for item in records
                        if _band_key(item.get("frequencyGHz")) == band
                        if (value := _finite(item.get("weatherRFSpecificAttenuationDbPerKm")))
                        is not None
                    ),
                    default=None,
                )
            )
            for band in ("rf24", "rf58")
        },
    }


def _model_detections_for_frame(
    raw_frame: Mapping[str, Any] | None,
    *,
    width: int | None,
    height: int | None,
    now: datetime,
) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    if raw_frame is None:
        return [], {
            "matched": False,
            "timestampUtc": None,
            "ageSeconds": None,
            "freshness": "unavailable",
        }

    timestamp = _parse_utc(raw_frame.get("timestampUtc"))
    age = _age_seconds(timestamp, now)
    raw_detections = raw_frame.get("detections") if isinstance(raw_frame.get("detections"), list) else []
    detections: list[dict[str, Any]] = []
    for raw in raw_detections[:MAX_MODEL_DETECTIONS_PER_FRAME]:
        if not isinstance(raw, dict):
            continue
        raw_bbox = raw.get("bboxXyxyPixels")
        if not isinstance(raw_bbox, list) or len(raw_bbox) != 4:
            continue
        bbox_values = [_finite(item) for item in raw_bbox]
        if any(item is None for item in bbox_values):
            continue
        x1, y1, x2, y2 = (float(item) for item in bbox_values if item is not None)
        if not (x2 > x1 and y2 > y1):
            continue
        if width is not None and height is not None and not (
            0.0 <= x1 < x2 <= float(width) and 0.0 <= y1 < y2 <= float(height)
        ):
            continue
        confidence = _finite(raw.get("confidence"))
        if confidence is not None and not 0.0 <= confidence <= 1.0:
            confidence = None
        slant_range = _finite(raw.get("slantRangeMeters"))
        depth_z = _finite(raw.get("depthZMeters"))
        distance = slant_range if slant_range is not None and slant_range >= 0.0 else depth_z
        if distance is not None and distance < 0.0:
            distance = None
        label = raw.get("className")
        detections.append({
            "targetActor": None,
            "label": label if isinstance(label, str) and label else "object",
            "source": "OAK RGB learned model",
            "bboxXyxyPixels": [round(x1, 3), round(y1, 3), round(x2, 3), round(y2, 3)],
            "confidence": _round(confidence, 6),
            "distanceMeters": _round(distance),
            "distanceSemantics": raw.get("rangeSemantics") or "simulation SceneDepth ranging",
            "snrDb": None,
            "frameAssociation": "exact nodeId + frameId",
            "rfTargetAssociation": "not_available",
        })
    return detections, {
        "matched": True,
        "timestampUtc": _utc_text(timestamp) if timestamp is not None else None,
        "ageSeconds": age,
        "freshness": _source_state(age),
    }


def _frame_for_node(
    saved_dir: Path,
    node_id: str,
    now: datetime,
    *,
    host: str,
    port: int,
    oak_model_frames: Mapping[tuple[str, str], dict[str, Any]] | None = None,
) -> dict[str, Any] | None:
    frame_dir = saved_dir / "frames" / node_id
    model_frames = oak_model_frames or {}
    preferred_model_frame = max(
        (
            item
            for (record_node, _), item in model_frames.items()
            if record_node == node_id
            and (model_age := _age_seconds(_parse_utc(item.get("timestampUtc")), now)) is not None
            and model_age <= MODEL_FRAME_MAX_AGE_SECONDS
        ),
        key=lambda item: _parse_utc(item.get("timestampUtc")) or datetime.min.replace(tzinfo=timezone.utc),
        default=None,
    )
    rgb_path: Path | None = None
    if preferred_model_frame is not None:
        frame_id = preferred_model_frame.get("frameId")
        reported_rgb = preferred_model_frame.get("rgbPath")
        if isinstance(frame_id, str) and isinstance(reported_rgb, str):
            candidate = frame_dir / Path(reported_rgb).name
            if candidate.name == f"{frame_id}_rgb.png" and candidate.is_file():
                rgb_path = candidate
    if rgb_path is None:
        rgb_path = _newest_file(frame_dir, "*_rgb.png")
    if rgb_path is None:
        return None
    stem = rgb_path.name.removesuffix("_rgb.png")
    metadata_path = frame_dir / f"{stem}_depth.json"
    metadata: dict[str, Any] = {}
    try:
        candidate = json.loads(metadata_path.read_text(encoding="utf-8"))
        if isinstance(candidate, dict):
            metadata = candidate
    except (OSError, UnicodeDecodeError, json.JSONDecodeError):
        pass
    timestamp = _parse_utc(metadata.get("timestampUtc"))
    try:
        file_modified = datetime.fromtimestamp(rgb_path.stat().st_mtime, timezone.utc)
        version = rgb_path.stat().st_mtime_ns
    except OSError:
        file_modified = None
        version = 0
    effective_timestamp = timestamp or file_modified
    age = _age_seconds(effective_timestamp, now)
    model_detections, model_match = _model_detections_for_frame(
        model_frames.get((node_id, stem)),
        width=metadata.get("width") if isinstance(metadata.get("width"), int) else None,
        height=metadata.get("height") if isinstance(metadata.get("height"), int) else None,
        now=now,
    )
    raw_targets = metadata.get("targets") if isinstance(metadata.get("targets"), list) else []
    visible_targets: list[dict[str, Any]] = []
    for raw in raw_targets[:MAX_TRACKS]:
        if not isinstance(raw, dict) or raw.get("intersectsFrame") is not True:
            continue
        bbox = raw.get("bboxXyxyPixels")
        if not isinstance(bbox, list) or len(bbox) != 4:
            bbox = None
        visible_targets.append({
            # Used only for this build's in-memory association and removed
            # before the frame enters the operator API payload.
            "_sourceTargetActor": raw.get("actorName"),
            "bboxXyxyPixels": [_round(item) for item in bbox] if bbox is not None else None,
            "distanceMeters": _round(raw.get("distanceMeters")),
            "projectionOnly": True,
            "classificationSemantics": "authored_projection_geometry_no_identity_inference",
        })
    return {
        "nodeId": node_id,
        "available": True,
        "imageUrl": f"http://{host}:{port}/frames/{node_id}?v={version}",
        "fileName": rgb_path.name,
        "timestampUtc": _utc_text(effective_timestamp) if effective_timestamp is not None else None,
        "ageSeconds": age,
        "freshness": _source_state(age),
        "width": metadata.get("width"),
        "height": metadata.get("height"),
        "frameIndex": metadata.get("frameIndex"),
        "weatherProfile": metadata.get("weatherProfile"),
        "visualWeatherApplied": metadata.get("airSimVisualWeatherApplied"),
        "targetProjectionSemantics": metadata.get("targetProjectionSemantics"),
        "visibleProjectionCount": len(visible_targets),
        "projectionTruthBoxes": visible_targets,
        "modelDetections": model_detections,
        "modelDetectionCount": len(model_detections),
        "modelDetectionFrameMatched": model_match["matched"],
        "modelDetectionTimestampUtc": model_match["timestampUtc"],
        "modelDetectionAgeSeconds": model_match["ageSeconds"],
        "modelDetectionFreshness": model_match["freshness"],
        "modelDetectionNotice": (
            "Cyan boxes are hash-pinned OAK RGB learned-model detections matched to this exact nodeId and frameId; no RF target association is claimed. "
            "Red/amber boxes are separate Unreal projected scenario truth."
            if model_match["matched"]
            else "No exact-frame OAK model report matched this RGB image. Red/amber projection boxes are Unreal scenario truth, not RGB-D model detections."
        ),
    }


@dataclass(slots=True)
class SnapshotBuilder:
    saved_dir: Path = DEFAULT_SAVED_DIR
    oak_report_path: Path | None = None
    host: str = DEFAULT_HOST
    port: int = DEFAULT_PORT
    cache_seconds: float = 0.25
    _cache_lock: threading.Lock = field(init=False, repr=False)
    _cache_at: float = field(init=False, repr=False, default=0.0)
    _cache: dict[str, Any] | None = field(init=False, repr=False, default=None)

    def __post_init__(self) -> None:
        self.saved_dir = Path(self.saved_dir)
        if self.oak_report_path is not None:
            self.oak_report_path = Path(self.oak_report_path)
        self._cache_lock = threading.Lock()
        self._cache_at = 0.0
        self._cache: dict[str, Any] | None = None

    def frame_path(self, node_id: str) -> Path | None:
        if node_id not in VISIBLE_FRAME_NODES:
            return None
        return _newest_file(self.saved_dir / "frames" / node_id, "*_rgb.png")

    def snapshot(self, *, force: bool = False) -> dict[str, Any]:
        monotonic_now = time.monotonic()
        with self._cache_lock:
            if not force and self._cache is not None and monotonic_now - self._cache_at <= self.cache_seconds:
                return self._cache
            result = self._build_snapshot()
            self._cache = result
            self._cache_at = monotonic_now
            return result

    def _build_snapshot(self) -> dict[str, Any]:
        now = _utc_now()
        live_payload, live_source = _load_live_rf_snapshot(self.saved_dir, now)
        layered_payload, layered_source = _load_layered_snapshot(self.saved_dir, now)
        oak_model_frames, oak_model_source = _load_oak_model_report(self.oak_report_path, now)
        rf_path = _newest_file(self.saved_dir, "rf_links_*.jsonl")
        alert_path = _newest_file(self.saved_dir, "alerts_*.jsonl")
        jsonl_rf_records, rf_stats = _tail_jsonl(rf_path, max_records=MAX_RF_RECORDS)
        logged_alert_records, alert_stats = _tail_jsonl(alert_path, max_records=MAX_ALERT_RECORDS)
        node_reference, node_config_source = _node_reference(self.saved_dir)
        if live_payload is not None:
            rf_records, alert_records = _records_from_live_snapshot(live_payload)
            selected_rf_source = "atomic-live-snapshot"
            live_nodes = live_payload.get("sensorNodes") if isinstance(live_payload.get("sensorNodes"), list) else []
            normalized_live_nodes: list[dict[str, Any]] = []
            short_by_id = {item["id"]: item["short"] for item in NODE_REFERENCE}
            for raw in live_nodes:
                if not isinstance(raw, dict) or raw.get("enabled") is False:
                    continue
                node_id = raw.get("nodeId")
                latitude = _finite(raw.get("latitudeDegrees"))
                longitude = _finite(raw.get("longitudeDegrees"))
                detection_range = _finite(raw.get("detectionRangeMeters"))
                if not isinstance(node_id, str) or latitude is None or longitude is None or detection_range is None:
                    continue
                normalized_live_nodes.append({
                    "id": node_id,
                    "short": short_by_id.get(node_id, node_id.removesuffix("_Sector").upper()),
                    "latitudeDegrees": latitude,
                    "longitudeDegrees": longitude,
                    "nominalRadiusKm": round(detection_range / 1000.0, 3),
                    "configuredHeightMeters": _round(raw.get("heightMeters")),
                })
            if normalized_live_nodes:
                node_reference = tuple(normalized_live_nodes)
                node_config_source = "latest_rf_snapshot.json sensorNodes (active Unreal configuration)"
        else:
            rf_records = jsonl_rf_records
            alert_records = logged_alert_records
            selected_rf_source = "bounded-jsonl-tail"

        rf_latest = _latest_by(rf_records, ("targetActor", "nodeId", "frequencyGHz"))
        alert_latest = _latest_by(alert_records, ("targetActor", "trackId"))
        rf_by_target: dict[str, list[dict[str, Any]]] = defaultdict(list)
        for record in rf_latest:
            target = record.get("targetActor")
            if isinstance(target, str) and target:
                rf_by_target[target].append(record)

        operator_track_by_target = {
            target: _operator_rf_track_id(
                target,
                next((item.get("emitterId") for item in records if item.get("emitterId")), None),
            )
            for target, records in rf_by_target.items()
        }
        layered_tracks = (
            layered_payload.get("tracks", [])
            if isinstance(layered_payload, Mapping)
            and isinstance(layered_payload.get("tracks"), list)
            else []
        )
        layered_by_target = {
            str(item.get("trackId")): item
            for item in layered_tracks
            if isinstance(item, Mapping)
            and isinstance(item.get("trackId"), str)
            and item.get("trackId")
        }
        for target in layered_by_target:
            operator_track_by_target.setdefault(
                target, _operator_rf_track_id(target, None)
            )
        target_by_operator_track = {
            operator_track_id: target
            for target, operator_track_id in operator_track_by_target.items()
        }
        alert_by_target: dict[str, dict[str, Any]] = {}
        for item in alert_latest:
            source_target = item.get("_sourceTargetActor") or item.get("targetActor")
            track_id = item.get("trackId")
            if isinstance(source_target, str) and source_target:
                key = source_target
            elif _is_operator_rf_track_id(track_id):
                key = target_by_operator_track.get(str(track_id), str(track_id))
            else:
                continue
            alert_by_target[key] = item

        target_names = set(rf_by_target) | set(alert_by_target) | set(layered_by_target)
        target_order = sorted(
            target_names,
            key=lambda name: max(
                [_parse_utc(item.get("timestampUtc")) or datetime.min.replace(tzinfo=timezone.utc) for item in rf_by_target.get(name, [])]
                + [_parse_utc(alert_by_target.get(name, {}).get("timestampUtc")) or datetime.min.replace(tzinfo=timezone.utc)]
                + [
                    (_parse_utc((layered_payload or {}).get("timestampUtc"))
                    if name in layered_by_target
                    else None)
                    or datetime.min.replace(tzinfo=timezone.utc)
                ]
                + [datetime.min.replace(tzinfo=timezone.utc)]
            ),
            reverse=True,
        )[:MAX_TRACKS]

        frames = [
            frame
            for node_id in VISIBLE_FRAME_NODES
            if (
                frame := _frame_for_node(
                    self.saved_dir,
                    node_id,
                    now,
                    host=self.host,
                    port=self.port,
                    oak_model_frames=oak_model_frames,
                )
            )
            is not None
        ]
        frame_targets: dict[str, list[dict[str, Any]]] = defaultdict(list)
        for frame in frames:
            for projection in frame["projectionTruthBoxes"]:
                target = projection.pop("_sourceTargetActor", None)
                if isinstance(target, str):
                    frame_targets[target].append({"nodeId": frame["nodeId"], **projection})

        tracks: list[dict[str, Any]] = []
        for target in target_order:
            layered_track = layered_by_target.get(target, {})
            links = sorted(
                rf_by_target.get(target, []),
                key=lambda item: (
                    item.get("detected") is True,
                    _finite(item.get("snrDb")) or -math.inf,
                ),
                reverse=True,
            )[:MAX_RF_LINKS_PER_TRACK]
            newest_link = max(links, key=lambda item: _parse_utc(item.get("timestampUtc")) or datetime.min.replace(tzinfo=timezone.utc), default={})
            alert = alert_by_target.get(target)
            newest_emitter_id = next((item.get("emitterId") for item in links if item.get("emitterId")), None)
            existing_track_id = (alert or {}).get("trackId")
            operator_track_id = (
                str(existing_track_id)
                if _is_operator_rf_track_id(existing_track_id)
                else operator_track_by_target.get(target)
                or (_operator_rf_track_id(target, newest_emitter_id) if not _is_operator_rf_track_id(target) else target)
            )
            latest_timestamp = max(
                [_parse_utc(item.get("timestampUtc")) or datetime.min.replace(tzinfo=timezone.utc) for item in links]
                + [_parse_utc((alert or {}).get("timestampUtc")) or datetime.min.replace(tzinfo=timezone.utc)]
                + [
                    (_parse_utc((layered_payload or {}).get("timestampUtc"))
                    if layered_track
                    else None)
                    or datetime.min.replace(tzinfo=timezone.utc)
                ]
            )
            if latest_timestamp == datetime.min.replace(tzinfo=timezone.utc):
                latest_timestamp = None
            band_summaries: dict[str, Any] = {}
            for band in ("rf24", "rf58"):
                band_links = [item for item in links if _band_key(item.get("frequencyGHz")) == band]
                detected_links = [item for item in band_links if item.get("detected") is True]
                best = max(band_links, key=lambda item: _finite(item.get("snrDb")) or -math.inf, default=None)
                band_summaries[band] = {
                    "available": bool(band_links),
                    "detectedNodeCount": len({str(item.get("nodeId")) for item in detected_links}),
                    "maxSnrDb": _round(max((_finite(item.get("snrDb")) for item in band_links if _finite(item.get("snrDb")) is not None), default=None)),
                    "bestNodeId": best.get("nodeId") if best else None,
                    "nearestDetectedRangeMeters": _round(min((_finite(item.get("distanceMeters")) for item in detected_links if _finite(item.get("distanceMeters")) is not None), default=None)),
                    "gate": "DETECTED" if detected_links else "NO_DETECTION",
                }
            confirming_ids = (alert or {}).get("confirmingNodeIds")
            if not isinstance(confirming_ids, list):
                confirming_ids = sorted({
                    str(item.get("nodeId"))
                    for item in links
                    if _preliminary_cue_evidence(item)
                })
            alerting = bool(alert and alert.get("detectionOnly") is True)
            ue_decision = "ALERT" if alerting else "DETECTED" if any(item.get("detected") is True for item in links) else "NO_DETECTION"
            disposition_source = alert or newest_link or layered_track
            layered_fusion = (
                layered_track.get("fusion")
                if isinstance(layered_track.get("fusion"), Mapping)
                else {}
            )
            active_modalities = (
                [
                    str(item)
                    for item in layered_track.get("activeModalities", [])
                    if isinstance(item, str)
                    and item in {"SEARCH_RADAR", "EO_PTZ", "THERMAL_PTZ"}
                ]
                if isinstance(layered_track.get("activeModalities"), list)
                else []
            )
            modality_evidence = (
                [
                    dict(item)
                    for item in layered_track.get("modalityEvidence", [])[:3]
                    if isinstance(item, Mapping)
                    and item.get("modality")
                    in {"SEARCH_RADAR", "EO_PTZ", "THERMAL_PTZ"}
                ]
                if isinstance(layered_track.get("modalityEvidence"), list)
                else []
            )
            raw_visual_frame = (
                layered_track.get("visualFrame")
                if isinstance(layered_track.get("visualFrame"), Mapping)
                else {}
            )
            confirmations = (
                [
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
                    for item in raw_visual_frame.get("confirmations", [])[:4]
                    if isinstance(item, Mapping)
                ]
                if isinstance(raw_visual_frame.get("confirmations"), list)
                else []
            )
            source_position = alert or newest_link
            latitude = _round(source_position.get("targetLatitudeDegrees"), 6)
            longitude = _round(source_position.get("targetLongitudeDegrees"), 6)
            altitude = _round(source_position.get("targetHeightMeters"))
            if latitude is None:
                latitude = _round(layered_track.get("latitudeDegrees"), 6)
            if longitude is None:
                longitude = _round(layered_track.get("longitudeDegrees"), 6)
            if altitude is None:
                altitude = _round(layered_track.get("heightMeters"))
            tracks.append({
                "id": operator_track_id,
                "targetActor": operator_track_id,
                "identifierSemantics": "deterministic_pseudonym_no_raw_actor_or_emitter_id",
                "disposition": _disposition(disposition_source),
                "latitudeDegrees": latitude,
                "longitudeDegrees": longitude,
                "altitudeMeters": altitude,
                "timestampUtc": _utc_text(latest_timestamp) if latest_timestamp is not None else None,
                "ageSeconds": _age_seconds(latest_timestamp, now),
                "freshness": _source_state(_age_seconds(latest_timestamp, now)),
                "ueRfDecision": ue_decision,
                "rfCueType": "rf_multinode_preliminary_cue" if alerting else None,
                "rfCueLevel": "preliminary" if alerting else None,
                "classification": "unclassified_emitter",
                "hostilityAssessment": "not_inferred",
                "confirmingNodeIds": confirming_ids[:8],
                "confirmingNodeCount": len(confirming_ids),
                "nearestConfirmingNodeId": (alert or {}).get("nearestConfirmingNodeId"),
                "nearestConfirmingNodeDistanceMeters": _round((alert or {}).get("nearestConfirmingNodeDistanceMeters")),
                "maxAlertSnrDb": _round((alert or {}).get("maxSnrDb")),
                "maxCueSnrDb": _round((alert or {}).get("maxSnrDb")),
                **_approach_fields(disposition_source),
                "distanceMeters": _round(
                    layered_track.get("nearestSensorDistanceMeters")
                ),
                "activeModalities": active_modalities,
                "modalityEvidence": modality_evidence,
                "visualEvidenceStatus": (
                    "RADAR_CUED_CONFIRMATION_AVAILABLE"
                    if confirmations
                    else "NO_RADAR_CUED_CONFIRMATION"
                ),
                "visualFrame": {
                    "transportStatus": "FRAME_AVAILABLE" if confirmations else "NO_FRAME",
                    "confirmations": confirmations,
                    "notice": (
                        "PTZ boxes are SIMULATED_SENSOR_CONFIRMATION / "
                        "SIMULATION_PROJECTION evidence, never learned-model output."
                    ),
                },
                "bands": band_summaries,
                "rfLinks": [
                    {
                        "nodeId": item.get("nodeId"),
                        "band": _band_key(item.get("frequencyGHz")),
                        "frequencyGHz": _round(item.get("frequencyGHz")),
                        "snrDb": _round(item.get("snrDb")),
                        "receivedPowerDbm": _round(item.get("receivedPowerDbm")),
                        "noiseFloorDbm": _round(item.get("noiseFloorDbm")),
                        "rangeMeters": _round(item.get("distanceMeters")),
                        "lineOfSight": item.get("lineOfSight"),
                        "weatherLossDb": _round(item.get("weatherRFLossDb")),
                        "detected": item.get("detected") is True,
                        "preliminaryCueEvidence": _preliminary_cue_evidence(item),
                        "alertEvidence": _preliminary_cue_evidence(item),
                    }
                    for item in links
                ],
                "visualProjectionObservations": frame_targets.get(target, []),
                "fusionV2": {
                    "evaluated": False,
                    "decision": "NOT_EVALUATED",
                    "reason": "Live RF gates and Unreal projection truth do not provide target-associated calibrated RGB-D/neuromorphic model scores. The bridge does not invent confidence or stack dependent RF links.",
                },
                "fusionV3": {
                    "evaluated": bool(layered_fusion),
                    "decision": layered_fusion.get("decision", "NOT_EVALUATED"),
                    "confirmationTier": layered_fusion.get(
                        "confirmationTier", "UNCONFIRMED"
                    ),
                    "operatorCueActive": layered_fusion.get("operatorCueActive") is True,
                    "score": _round(layered_fusion.get("fusedEvidenceScore"), 6),
                    "scoreSemantics": layered_fusion.get("scoreSemantics"),
                    "activeModalityFamilies": layered_fusion.get(
                        "activeModalityFamilies", []
                    ),
                    "method": layered_fusion.get("method"),
                    "simulationOnly": True,
                    "learnedModelOutputClaimedForRadarPtz": False,
                },
            })

        node_links: dict[str, list[dict[str, Any]]] = defaultdict(list)
        for record in rf_latest:
            node_id = record.get("nodeId")
            if isinstance(node_id, str):
                node_links[node_id].append(record)
        layered_sensor_by_node: dict[str, list[dict[str, Any]]] = defaultdict(list)
        raw_layered_sensors = (
            layered_payload.get("sensorNodes", [])
            if isinstance(layered_payload, Mapping)
            and isinstance(layered_payload.get("sensorNodes"), list)
            else []
        )
        for item in raw_layered_sensors:
            if not isinstance(item, Mapping):
                continue
            node_id = item.get("nodeId")
            sensor_type = item.get("sensorType", item.get("modality"))
            if (
                isinstance(node_id, str)
                and sensor_type in {"SEARCH_RADAR", "EO_PTZ", "THERMAL_PTZ"}
            ):
                layered_sensor_by_node[node_id].append(dict(item))
        nodes: list[dict[str, Any]] = []
        for reference in node_reference:
            links = node_links.get(reference["id"], [])
            long_range_sensors = layered_sensor_by_node.get(reference["id"], [])
            latest = max(links, key=lambda item: _parse_utc(item.get("timestampUtc")) or datetime.min.replace(tzinfo=timezone.utc), default=None)
            bands: dict[str, Any] = {}
            for band in ("rf24", "rf58"):
                band_links = [item for item in links if _band_key(item.get("frequencyGHz")) == band]
                detected = [item for item in band_links if item.get("detected") is True]
                strongest = max(band_links, key=lambda item: _finite(item.get("snrDb")) or -math.inf, default=None)
                bands[band] = {
                    "detectedContacts": len(detected),
                    "strongestSnrDb": _round((strongest or {}).get("snrDb")),
                    "strongestReceivedPowerDbm": _round((strongest or {}).get("receivedPowerDbm")),
                    "nearestDetectedRangeMeters": _round(min((_finite(item.get("distanceMeters")) for item in detected if _finite(item.get("distanceMeters")) is not None), default=None)),
                }
            node_age = _age_seconds(_parse_utc((latest or {}).get("timestampUtc")), now)
            if node_age is None and long_range_sensors:
                node_age = layered_source.get("ageSeconds")
            sanitized_long_range_sensors = [
                {
                    key: item.get(key)
                    for key in (
                        "sensorId",
                        "sensorType",
                        "modality",
                        "status",
                        "runtimeStatus",
                        "configured",
                        "enabled",
                        "simulated",
                        "rangeMeters",
                        "rangeSource",
                        "detectedObservationCount",
                        "lastEvidenceTimestampUtc",
                        "azimuthDeg",
                        "horizontalFovDeg",
                        "validationState",
                    )
                }
                for item in long_range_sensors
            ]
            long_range_tracking = any(
                (_finite(item.get("detectedObservationCount")) or 0.0) > 0.0
                for item in long_range_sensors
            )
            long_range_online = any(
                str(item.get("status", "")).upper() in {"ONLINE", "DEGRADED"}
                for item in long_range_sensors
            )
            nodes.append({
                **reference,
                "heightMeters": _round((latest or {}).get("nodeHeightMeters")) or reference.get("configuredHeightMeters"),
                "state": "tracking"
                if any(item.get("detected") is True for item in links) or long_range_tracking
                else "nominal"
                if links or long_range_online
                else "unavailable",
                "freshness": _source_state(node_age),
                "ageSeconds": node_age,
                "bands": bands,
                "longRangeSensors": sanitized_long_range_sensors,
            })

        rf_source = (
            {
                **live_source,
                "recordsParsed": len(rf_records),
                "invalidLines": 0,
                "truncatedAtStart": False,
            }
            if live_payload is not None
            else _source_info(rf_path, jsonl_rf_records, rf_stats, now)
        )
        alert_source = _source_info(alert_path, logged_alert_records, alert_stats, now)
        source_ages = [
            value
            for value in (
                rf_source["ageSeconds"],
                alert_source["ageSeconds"],
                layered_source.get("ageSeconds") if layered_payload is not None else None,
            )
            if value is not None
        ]
        newest_age = min(source_ages, default=None)
        live_available = (
            live_payload is not None
            or layered_payload is not None
            or bool(rf_records or alert_records or frames)
        )
        scenario_targets = _scenario_targets_from_live_snapshot(live_payload)
        scenario_truth_summary = {
            "source": "Unreal authored/discovered scenario truth; these counts are not sensor detections.",
            "targetCount": len(scenario_targets),
            "outsideTargetCount": sum(item["outsideSimulationPerimeter"] is True for item in scenario_targets),
            "approachingTargetCount": sum(item["airspaceState"] == "APPROACHING" for item in scenario_targets),
            "detectedTargetCount": sum(item["detectedThisSample"] is True for item in scenario_targets),
        }
        return {
            "schemaVersion": SNAPSHOT_SCHEMA_VERSION,
            "generatedAtUtc": _utc_text(now),
            "mode": "live-local" if live_available else "unavailable",
            "detectionOnly": True,
            "actionsTaken": "none",
            "sourceFreshness": {
                "state": _source_state(newest_age),
                "newestTelemetryAgeSeconds": newest_age,
                "freshThresholdSeconds": FRESH_AFTER_SECONDS,
                "staleThresholdSeconds": STALE_AFTER_SECONDS,
                "notice": "Freshness is wall-clock age of the latest exported record; a paused Unreal simulation will correctly appear stale.",
            },
            "sources": {
                "rfLinks": rf_source,
                "alerts": alert_source,
                "frames": frames,
                "nodeConfiguration": node_config_source,
                "latestRFSnapshot": live_source,
                "latestLayeredSnapshot": layered_source,
                "selectedRFSource": selected_rf_source,
                "oakRgbdModel": oak_model_source,
            },
            "simulationPerimeter": _perimeter_from_live_snapshot(live_payload),
            "scenarioTruthSummary": scenario_truth_summary,
            "weather": _weather_from_live_snapshot(live_payload) if live_payload is not None else _weather(rf_latest or alert_latest),
            "fusion": {
                "method": (
                    (layered_payload or {}).get("fusionPolicy", {}).get("method")
                    if isinstance((layered_payload or {}).get("fusionPolicy"), Mapping)
                    else FUSION_METHOD
                ),
                "scoreSemantics": (
                    "Strongest reliability/weather-discounted evidence index; not a "
                    "calibrated posterior probability"
                    if layered_payload is not None
                    else FUSION_SCORE_SEMANTICS
                ),
                "liveEvaluationAvailable": layered_payload is not None,
                "status": "EVALUATED" if layered_payload is not None else "NOT_EVALUATED",
                "notice": (
                    "Fresh fail-closed layered fusion is available. SEARCH_RADAR, EO_PTZ, "
                    "and THERMAL_PTZ are deterministic simulated evidence families; PTZ "
                    "projection boxes are not learned-model output."
                    if layered_payload is not None
                    else "The bridge preserves audited fusion-v2 semantics and does not convert RF SNR or scene-truth boxes into model confidence."
                ),
            },
            "nodes": nodes,
            "sensorHealth": [
                sensor
                for node in nodes
                for sensor in node.get("longRangeSensors", [])
            ],
            "tracks": tracks,
            "counts": {
                "nodes": len(nodes),
                "tracks": len(tracks),
                "rfPreliminaryCues": sum(item["ueRfDecision"] == "ALERT" for item in tracks),
                # Compatibility count name; these are generic preliminary cues.
                "rfAlerts": sum(item["ueRfDecision"] == "ALERT" for item in tracks),
                "rfDetectedTracks": sum(item["ueRfDecision"] in {"ALERT", "DETECTED"} for item in tracks),
                "dualBandDetectedTracks": sum(item["bands"]["rf24"]["gate"] == "DETECTED" and item["bands"]["rf58"]["gate"] == "DETECTED" for item in tracks),
                "outsideDetectedTracks": sum(item["outsideSimulationPerimeter"] is True for item in tracks),
                "approachingDetectedTracks": sum(item["airspaceState"] == "APPROACHING" for item in tracks),
                "searchRadarDetectedTracks": sum(
                    "SEARCH_RADAR" in item.get("activeModalities", []) for item in tracks
                ),
                "eoPtzConfirmedTracks": sum(
                    "EO_PTZ" in item.get("activeModalities", []) for item in tracks
                ),
                "thermalPtzConfirmedTracks": sum(
                    "THERMAL_PTZ" in item.get("activeModalities", []) for item in tracks
                ),
                "radarCuedVisualFrameCount": sum(
                    len(item.get("visualFrame", {}).get("confirmations", []))
                    for item in tracks
                ),
            },
        }


class BridgeRequestHandler(BaseHTTPRequestHandler):
    server_version = "TRIADC3Bridge/1.0"

    @property
    def builder(self) -> SnapshotBuilder:
        return self.server.snapshot_builder  # type: ignore[attr-defined]

    def _cors(self) -> None:
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")

    def _json(self, status: HTTPStatus, payload: Mapping[str, Any]) -> None:
        body = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self._cors()
        self.end_headers()
        self.wfile.write(body)

    def do_OPTIONS(self) -> None:  # noqa: N802 - stdlib handler API
        self.send_response(HTTPStatus.NO_CONTENT)
        self._cors()
        self.end_headers()

    def do_GET(self) -> None:  # noqa: N802 - stdlib handler API
        route = urlparse(self.path).path
        if route in {"/", "/health"}:
            snapshot = self.builder.snapshot()
            self._json(HTTPStatus.OK, {
                "service": "triad-c3-local-bridge",
                "status": "ok",
                "mode": snapshot["mode"],
                "freshness": snapshot["sourceFreshness"],
                "detectionOnly": True,
            })
            return
        if route == "/api/snapshot":
            self._json(HTTPStatus.OK, self.builder.snapshot())
            return
        if route.startswith("/frames/"):
            node_id = unquote(route.removeprefix("/frames/"))
            if "/" in node_id or "\\" in node_id or node_id not in VISIBLE_FRAME_NODES:
                self._json(HTTPStatus.NOT_FOUND, {"error": "frame not found"})
                return
            path = self.builder.frame_path(node_id)
            if path is None:
                self._json(HTTPStatus.NOT_FOUND, {"error": "frame not found"})
                return
            try:
                body = path.read_bytes()
            except OSError:
                self._json(HTTPStatus.NOT_FOUND, {"error": "frame not found"})
                return
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", mimetypes.guess_type(path.name)[0] or "application/octet-stream")
            self.send_header("Content-Length", str(len(body)))
            self._cors()
            self.end_headers()
            self.wfile.write(body)
            return
        self._json(HTTPStatus.NOT_FOUND, {"error": "not found"})

    def log_message(self, format: str, *args: object) -> None:
        print(f"[{self.log_date_time_string()}] {format % args}")


class C3BridgeServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, address: tuple[str, int], builder: SnapshotBuilder):
        super().__init__(address, BridgeRequestHandler)
        self.snapshot_builder = builder


def create_server(
    *,
    saved_dir: Path,
    host: str,
    port: int,
    oak_report_path: Path | None = None,
) -> C3BridgeServer:
    builder = SnapshotBuilder(
        saved_dir=Path(saved_dir),
        oak_report_path=oak_report_path,
        host=host,
        port=port,
    )
    return C3BridgeServer((host, port), builder)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Serve bounded Unreal sensor telemetry to the local TRIAD C3 dashboard.")
    parser.add_argument("--saved-dir", type=Path, default=DEFAULT_SAVED_DIR)
    parser.add_argument("--host", default=DEFAULT_HOST, help="Bind address; loopback is strongly recommended.")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument(
        "--oak-report",
        type=Path,
        help="Optional atomically refreshed oak_rgbd_viewer watch report.",
    )
    parser.add_argument("--snapshot", action="store_true", help="Print one JSON snapshot and exit.")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    if not 0 <= args.port <= 65535:
        raise SystemExit("--port must be in [0, 65535]")
    builder = SnapshotBuilder(
        saved_dir=args.saved_dir,
        oak_report_path=args.oak_report,
        host=args.host,
        port=args.port,
    )
    if args.snapshot:
        print(json.dumps(builder.snapshot(force=True), indent=2, ensure_ascii=False))
        return 0
    server = C3BridgeServer((args.host, args.port), builder)
    actual_host, actual_port = server.server_address[:2]
    print(f"TRIAD C3 local bridge: http://{actual_host}:{actual_port}/api/snapshot")
    print(f"Reading Unreal exports from: {args.saved_dir}")
    print("Detection only; no mutation or engagement endpoints.")
    try:
        server.serve_forever(poll_interval=0.2)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
