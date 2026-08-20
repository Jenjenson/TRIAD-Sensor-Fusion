"""Streaming audit for Unreal weather, RF, preliminary-cue, and camera telemetry.

The audit is deliberately read-only with respect to Unreal.  It snapshots the
input file sizes, streams JSONL instead of loading it into memory, and reports
simulation evidence without making real-world detection-performance claims.
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import re
import tempfile
from typing import Any, Callable, Mapping, Sequence

from .paths import DEFAULT_TRIAD_CONFIG, DEFAULT_TRIAD_LOG, DEFAULT_TRIAD_SENSOR_SAVED_DIR

DEFAULT_SAVED_DIR = DEFAULT_TRIAD_SENSOR_SAVED_DIR
DEFAULT_LOG_PATH = DEFAULT_TRIAD_LOG
DEFAULT_CONFIG_PATH = DEFAULT_TRIAD_CONFIG
MAX_JSONL_LINE_BYTES = 1_048_576
MAX_METADATA_BYTES = 1_048_576

RF_REQUIRED_FIELDS = (
    "timestampUtc",
    "simulationSeconds",
    "nodeId",
    "targetActor",
    "emitterId",
    "hostileScenarioTruth",
    "frequencyGHz",
    "nodeLongitudeDegrees",
    "nodeLatitudeDegrees",
    "nodeHeightMeters",
    "targetLongitudeDegrees",
    "targetLatitudeDegrees",
    "targetHeightMeters",
    "distanceMeters",
    "azimuthDegrees",
    "elevationDegrees",
    "lineOfSight",
    "freeSpacePathLossDb",
    "obstructionLossDb",
    "weatherProfile",
    "airSimVisualWeatherApplied",
    "weatherRainRateMillimetersPerHour",
    "weatherVisibilityMeters",
    "weatherRFSpecificAttenuationDbPerKm",
    "weatherRFLossDb",
    "receivedPowerDbm",
    "noiseFloorDbm",
    "snrDb",
    "frequencySupported",
    "inRange",
    "aboveSensitivity",
    "detected",
    "alertEvidence",
)

RF_PRELIMINARY_CUE_REQUIRED_FIELDS = (
    "timestampUtc",
    "simulationSeconds",
    "alertType",
    "cueLevel",
    "classification",
    "hostilityAssessment",
    "evidenceBasis",
    "trackId",
    "identifierSemantics",
    "positionEstimateAvailable",
    "approachEstimateAvailable",
    "bearingEstimateAvailable",
    "positionApproachSemantics",
    "confirmingNodeCount",
    "confirmingNodeIds",
    "confirmingNodes",
    "minimumConfirmingNodes",
    "maxSnrDb",
    "distanceValid",
    "distanceSemantics",
    "nearestConfirmingNodeId",
    "nearestConfirmingNodeDistanceMeters",
    "weatherProfile",
    "airSimVisualWeatherApplied",
    "weatherRainRateMillimetersPerHour",
    "weatherVisibilityMeters",
    "detectionOnly",
    "actionsTaken",
)

LEGACY_TRUTH_GATED_ALERT_REQUIRED_FIELDS = (
    "timestampUtc",
    "simulationSeconds",
    "alertType",
    "targetActor",
    "emitterId",
    "hostileScenarioTruth",
    "confirmingNodeCount",
    "confirmingNodeIds",
    "minimumConfirmingNodes",
    "detectionOnly",
    "actionsTaken",
)

FRAME_REQUIRED_FIELDS = (
    "timestampUtc",
    "nodeId",
    "frameIndex",
    "width",
    "height",
    "fovDegrees",
    "depthCaptureSource",
    "depthPngEncoding",
    "normalizationMaxMeters",
    "minimumObservedMeters",
    "maximumObservedMeters",
    "weatherProfile",
    "airSimVisualWeatherApplied",
    "weatherRainRateMillimetersPerHour",
    "weatherVisibilityMeters",
    "targetProjectionSemantics",
    "cameraAspectRatio",
    "taggedTargetCount",
    "visibleTargetCount",
    "targets",
)

_SESSION_RE = re.compile(r"(?P<stamp>\d{8}T\d{6}Z)(?:_(?P<token>[0-9A-Fa-f]+))?")
_FRAME_STAMP_RE = re.compile(r"(?P<stamp>\d{8}T\d{6})")
_LOG_TIME_RE = re.compile(
    r"^\[(?P<date>\d{4}\.\d{2}\.\d{2})-(?P<time>\d{2}\.\d{2}\.\d{2}):(?P<millis>\d{3})\]"
)
_WEATHER_RE = re.compile(
    r"TRIAD WEATHER (?P<profile>.+?) \| rain (?P<rain>[-+0-9.eE]+) mm/h "
    r"\| visibility (?P<visibility>[-+0-9.eE]+) km \| AirSim visual weather (?P<status>[^.]+)",
    re.IGNORECASE,
)
_JSONL_CAP_RE = re.compile(
    r"TRIAD JSONL telemetry stopped:\s*(?P<reason>.+?)(?:\.|$)", re.IGNORECASE
)
_CAMERA_CAP_RE = re.compile(
    r"TRIAD sensor node '(?P<node>[^']+)' stopped camera capture:\s*(?P<reason>.+?)(?:\.|$)",
    re.IGNORECASE,
)


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")


def _finite(value: object) -> float | None:
    if isinstance(value, bool):
        return None
    try:
        number = float(value)  # type: ignore[arg-type]
    except (TypeError, ValueError, OverflowError):
        return None
    return number if math.isfinite(number) else None


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


def _iso(value: datetime | None) -> str | None:
    if value is None:
        return None
    return value.astimezone(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z"
    )


def _normalise_profile(value: object) -> str:
    raw = str(value or "Unknown").strip()
    compact = re.sub(r"[^a-z0-9]+", "", raw.lower())
    aliases = {
        "clear": "Clear",
        "lightrain": "Light Rain",
        "monsoon": "Monsoon",
        "heavyrain": "Monsoon",
        "haze": "Haze",
        "fog": "Haze",
    }
    return aliases.get(compact, raw or "Unknown")


def _band_name(value: object) -> str:
    frequency = _finite(value)
    if frequency is None:
        return "Unknown"
    if abs(frequency - 2.4) <= 0.05:
        return "2.4 GHz"
    if abs(frequency - 5.8) <= 0.05:
        return "5.8 GHz"
    return f"Other ({frequency:.6g} GHz)"


@dataclass
class OnlineStats:
    count: int = 0
    minimum: float = math.inf
    maximum: float = -math.inf
    mean: float = 0.0
    m2: float = 0.0

    def add(self, value: object) -> None:
        number = _finite(value)
        if number is None:
            return
        self.count += 1
        self.minimum = min(self.minimum, number)
        self.maximum = max(self.maximum, number)
        delta = number - self.mean
        self.mean += delta / self.count
        self.m2 += delta * (number - self.mean)

    def as_dict(self) -> dict[str, float | int | None]:
        if self.count == 0:
            return {
                "count": 0,
                "min": None,
                "mean": None,
                "max": None,
                "standard_deviation": None,
            }
        deviation = math.sqrt(self.m2 / self.count)
        return {
            "count": self.count,
            "min": self.minimum,
            "mean": self.mean,
            "max": self.maximum,
            "standard_deviation": deviation,
        }


@dataclass
class SchemaTracker:
    required_fields: tuple[str, ...]
    record_count: int = 0
    complete_record_count: int = 0
    present: Counter[str] = field(default_factory=Counter)

    def observe(self, record: Mapping[str, object]) -> None:
        self.record_count += 1
        complete = True
        for name in self.required_fields:
            if name in record and record[name] is not None:
                self.present[name] += 1
            else:
                complete = False
        if complete:
            self.complete_record_count += 1

    def as_dict(self) -> dict[str, object]:
        denominator = self.record_count * len(self.required_fields)
        present_cells = sum(self.present.values())
        return {
            "record_count": self.record_count,
            "required_field_count": len(self.required_fields),
            "complete_record_count": self.complete_record_count,
            "complete_record_percent": (
                100.0 * self.complete_record_count / self.record_count
                if self.record_count
                else 0.0
            ),
            "required_cell_completeness_percent": (
                100.0 * present_cells / denominator if denominator else 0.0
            ),
            "missing_record_count_by_field": {
                name: self.record_count - self.present[name]
                for name in self.required_fields
                if self.record_count - self.present[name] > 0
            },
        }


@dataclass
class RFBucket:
    samples: int = 0
    authored_hostile_truth_samples: int = 0
    detections: int = 0
    detections_on_authored_hostile_truth: int = 0
    preliminary_cue_evidence_samples: int = 0
    legacy_truth_gated_alert_evidence_samples: int = 0
    line_of_sight_samples: int = 0
    in_range_samples: int = 0
    above_sensitivity_samples: int = 0
    visual_weather_applied_samples: int = 0
    nodes: set[str] = field(default_factory=set)
    targets: set[str] = field(default_factory=set)
    snr_db: OnlineStats = field(default_factory=OnlineStats)
    weather_rf_loss_db: OnlineStats = field(default_factory=OnlineStats)
    range_m: OnlineStats = field(default_factory=OnlineStats)
    detected_range_m: OnlineStats = field(default_factory=OnlineStats)

    def observe(self, record: Mapping[str, object]) -> None:
        self.samples += 1
        hostile = record.get("hostileScenarioTruth") is True
        detected = record.get("detected") is True
        self.authored_hostile_truth_samples += int(hostile)
        self.detections += int(detected)
        self.detections_on_authored_hostile_truth += int(hostile and detected)
        self.preliminary_cue_evidence_samples += int(
            record.get("preliminaryCueEvidence") is True
            if "preliminaryCueEvidence" in record
            else detected
        )
        self.legacy_truth_gated_alert_evidence_samples += int(
            "preliminaryCueEvidence" not in record
            and record.get("alertEvidence") is True
        )
        self.line_of_sight_samples += int(record.get("lineOfSight") is True)
        self.in_range_samples += int(record.get("inRange") is True)
        self.above_sensitivity_samples += int(record.get("aboveSensitivity") is True)
        self.visual_weather_applied_samples += int(
            record.get("airSimVisualWeatherApplied") is True
        )
        if record.get("nodeId") is not None:
            self.nodes.add(str(record["nodeId"]))
        if record.get("targetActor") is not None:
            self.targets.add(str(record["targetActor"]))
        self.snr_db.add(record.get("snrDb"))
        self.weather_rf_loss_db.add(record.get("weatherRFLossDb"))
        self.range_m.add(record.get("distanceMeters"))
        if detected:
            self.detected_range_m.add(record.get("distanceMeters"))

    def as_dict(self) -> dict[str, object]:
        return {
            "sample_count": self.samples,
            "authored_hostile_truth_sample_count": self.authored_hostile_truth_samples,
            "detection_count": self.detections,
            "detections_on_authored_hostile_truth_count": self.detections_on_authored_hostile_truth,
            "preliminary_cue_evidence_sample_count": self.preliminary_cue_evidence_samples,
            "legacy_truth_gated_alert_evidence_sample_count": self.legacy_truth_gated_alert_evidence_samples,
            "line_of_sight_sample_count": self.line_of_sight_samples,
            "in_range_sample_count": self.in_range_samples,
            "above_sensitivity_sample_count": self.above_sensitivity_samples,
            "visual_weather_applied_sample_count": self.visual_weather_applied_samples,
            "detection_rate": self.detections / self.samples if self.samples else None,
            "node_count": len(self.nodes),
            "nodes": sorted(self.nodes),
            "target_count": len(self.targets),
            "snr_db": self.snr_db.as_dict(),
            "weather_rf_loss_db": self.weather_rf_loss_db.as_dict(),
            "range_m": self.range_m.as_dict(),
            "detected_range_m": self.detected_range_m.as_dict(),
        }


@dataclass
class RFCueBucket:
    cue_records: int = 0
    generic_preliminary_cue_records: int = 0
    legacy_truth_gated_alert_records: int = 0
    detection_only_records: int = 0
    visual_weather_applied_records: int = 0
    confirming_node_count: OnlineStats = field(default_factory=OnlineStats)
    max_snr_db: OnlineStats = field(default_factory=OnlineStats)
    nearest_range_m: OnlineStats = field(default_factory=OnlineStats)
    track_ids: set[str] = field(default_factory=set)
    legacy_evaluator_targets: set[str] = field(default_factory=set)
    confirming_nodes: Counter[str] = field(default_factory=Counter)

    def observe(self, record: Mapping[str, object]) -> None:
        self.cue_records += 1
        if record.get("alertType") == "rf_multinode_preliminary_cue":
            self.generic_preliminary_cue_records += 1
        else:
            self.legacy_truth_gated_alert_records += 1
        self.detection_only_records += int(record.get("detectionOnly") is True)
        self.visual_weather_applied_records += int(
            record.get("airSimVisualWeatherApplied") is True
        )
        self.confirming_node_count.add(record.get("confirmingNodeCount"))
        self.max_snr_db.add(record.get("maxSnrDb"))
        self.nearest_range_m.add(record.get("nearestConfirmingNodeDistanceMeters"))
        if record.get("trackId") is not None:
            self.track_ids.add(str(record["trackId"]))
        if record.get("targetActor") is not None:
            self.legacy_evaluator_targets.add(str(record["targetActor"]))
        node_ids = record.get("confirmingNodeIds")
        if isinstance(node_ids, list):
            self.confirming_nodes.update(str(item) for item in node_ids)

    def as_dict(self) -> dict[str, object]:
        return {
            "cue_record_count": self.cue_records,
            "generic_preliminary_cue_record_count": self.generic_preliminary_cue_records,
            "legacy_truth_gated_alert_record_count": self.legacy_truth_gated_alert_records,
            "detection_only_record_count": self.detection_only_records,
            "visual_weather_applied_record_count": self.visual_weather_applied_records,
            "operator_track_count": len(self.track_ids),
            "legacy_evaluator_target_count": len(self.legacy_evaluator_targets),
            "confirming_node_count": self.confirming_node_count.as_dict(),
            "max_snr_db": self.max_snr_db.as_dict(),
            "nearest_confirming_node_range_m": self.nearest_range_m.as_dict(),
            "confirming_node_occurrences": dict(sorted(self.confirming_nodes.items())),
        }


@dataclass
class NodeBucket:
    samples: int = 0
    detections: int = 0
    preliminary_cue_evidence_samples: int = 0
    bands: Counter[str] = field(default_factory=Counter)
    profiles: Counter[str] = field(default_factory=Counter)
    range_m: OnlineStats = field(default_factory=OnlineStats)
    snr_db: OnlineStats = field(default_factory=OnlineStats)

    def observe(self, record: Mapping[str, object], profile: str, band: str) -> None:
        self.samples += 1
        self.detections += int(record.get("detected") is True)
        self.preliminary_cue_evidence_samples += int(
            record.get("preliminaryCueEvidence") is True
            if "preliminaryCueEvidence" in record
            else record.get("detected") is True
        )
        self.bands[band] += 1
        self.profiles[profile] += 1
        self.range_m.add(record.get("distanceMeters"))
        self.snr_db.add(record.get("snrDb"))

    def as_dict(self) -> dict[str, object]:
        return {
            "sample_count": self.samples,
            "detection_count": self.detections,
            "preliminary_cue_evidence_sample_count": self.preliminary_cue_evidence_samples,
            "bands": dict(sorted(self.bands.items())),
            "weather_profiles": dict(sorted(self.profiles.items())),
            "range_m": self.range_m.as_dict(),
            "snr_db": self.snr_db.as_dict(),
        }


def _newest(directory: Path, prefix: str) -> Path | None:
    candidates = [path for path in directory.glob(f"{prefix}*.jsonl") if path.is_file()]
    return max(candidates, key=lambda path: (path.stat().st_mtime_ns, path.name)) if candidates else None


def _session_from_path(path: Path) -> tuple[str, datetime | None]:
    match = _SESSION_RE.search(path.stem)
    if not match:
        return path.stem.removeprefix("rf_links_"), None
    identifier = match.group(0)
    try:
        started = datetime.strptime(match.group("stamp"), "%Y%m%dT%H%M%SZ").replace(
            tzinfo=timezone.utc
        )
    except ValueError:
        started = None
    return identifier, started


def _stream_jsonl(
    path: Path | None,
    observe: Callable[[Mapping[str, object]], None],
) -> dict[str, object]:
    if path is None or not path.is_file():
        return {
            "path": None if path is None else str(path),
            "exists": False,
            "snapshot_bytes": 0,
            "line_count": 0,
            "record_count": 0,
            "invalid_line_count": 0,
            "overlong_line_count": 0,
            "first_timestamp_utc": None,
            "last_timestamp_utc": None,
        }
    snapshot_bytes = path.stat().st_size
    line_count = 0
    records = 0
    invalid = 0
    overlong = 0
    first_time: datetime | None = None
    last_time: datetime | None = None
    with path.open("rb") as handle:
        while handle.tell() < snapshot_bytes:
            raw = handle.readline()
            if not raw:
                break
            line_count += 1
            if len(raw) > MAX_JSONL_LINE_BYTES:
                overlong += 1
                continue
            if not raw.strip():
                continue
            try:
                decoded = json.loads(raw.decode("utf-8-sig"))
            except (UnicodeDecodeError, json.JSONDecodeError):
                invalid += 1
                continue
            if not isinstance(decoded, dict):
                invalid += 1
                continue
            records += 1
            stamp = _parse_utc(decoded.get("timestampUtc"))
            if stamp is not None:
                first_time = stamp if first_time is None else min(first_time, stamp)
                last_time = stamp if last_time is None else max(last_time, stamp)
            observe(decoded)
    return {
        "path": str(path.resolve()),
        "exists": True,
        "snapshot_bytes": snapshot_bytes,
        "bytes_after_read": path.stat().st_size,
        "grew_during_read": path.stat().st_size > snapshot_bytes,
        "line_count": line_count,
        "record_count": records,
        "invalid_line_count": invalid,
        "overlong_line_count": overlong,
        "first_timestamp_utc": _iso(first_time),
        "last_timestamp_utc": _iso(last_time),
    }


def _load_config(path: Path | None) -> dict[str, object]:
    if path is None or not path.is_file():
        return {
            "path": None if path is None else str(path),
            "loaded": False,
            "expected_nodes": [],
        }
    try:
        decoded = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError):
        return {"path": str(path.resolve()), "loaded": False, "expected_nodes": []}
    if not isinstance(decoded, dict):
        return {"path": str(path.resolve()), "loaded": False, "expected_nodes": []}
    nodes = decoded.get("SensorNodes")
    expected_nodes = []
    if isinstance(nodes, list):
        expected_nodes = [
            str(item.get("NodeId"))
            for item in nodes
            if isinstance(item, dict)
            and item.get("NodeId") is not None
            and item.get("bEnabled", True) is not False
        ]
    weather = decoded.get("Weather") if isinstance(decoded.get("Weather"), dict) else {}
    return {
        "path": str(path.resolve()),
        "loaded": True,
        "expected_nodes": expected_nodes,
        "max_telemetry_records": decoded.get("MaxTelemetryRecords"),
        "max_telemetry_file_bytes": decoded.get("MaxTelemetryFileBytes"),
        "max_alert_records": decoded.get("MaxAlertRecords"),
        "max_frames_per_node": decoded.get("MaxFramesPerNode"),
        "weather_cycle_enabled": weather.get("bCycleProfiles") if isinstance(weather, dict) else None,
        "weather_cycle_interval_seconds": weather.get("CycleIntervalSeconds") if isinstance(weather, dict) else None,
        "weather_cycle_profiles": [
            _normalise_profile(item) for item in weather.get("CycleProfiles", [])
        ]
        if isinstance(weather, dict) and isinstance(weather.get("CycleProfiles"), list)
        else [],
    }


def _parse_log_time(line: str) -> datetime | None:
    match = _LOG_TIME_RE.match(line)
    if not match:
        return None
    try:
        base = datetime.strptime(
            f"{match.group('date')}-{match.group('time')}", "%Y.%m.%d-%H.%M.%S"
        ).replace(tzinfo=timezone.utc)
    except ValueError:
        return None
    return base.replace(microsecond=int(match.group("millis")) * 1000)


def _scan_log(path: Path, session_start: datetime | None) -> dict[str, object]:
    if not path.is_file():
        return {
            "path": str(path),
            "exists": False,
            "weather_events": [],
            "jsonl_cap_events": [],
            "camera_cap_events": [],
        }
    snapshot_bytes = path.stat().st_size
    weather_events: list[dict[str, object]] = []
    jsonl_caps: list[dict[str, object]] = []
    camera_caps: list[dict[str, object]] = []
    weather_warning_count = 0
    weather_warning_samples: list[str] = []
    airsim_runtime_startup_seen = False
    with path.open("r", encoding="utf-8-sig", errors="replace") as handle:
        while handle.tell() < snapshot_bytes:
            line = handle.readline()
            if not line:
                break
            stamp = _parse_log_time(line)
            airsim_runtime_startup_seen |= "StartupModule: AirSim TRIAD runtime plugin" in line
            in_session = session_start is None or stamp is None or stamp >= session_start
            if not in_session:
                continue
            if "Warning" in line and ("WeatherActor" in line or "weather" in line.lower()):
                weather_warning_count += 1
                if len(weather_warning_samples) < 5:
                    weather_warning_samples.append(line.strip())
            weather_match = _WEATHER_RE.search(line)
            if weather_match:
                status = weather_match.group("status").strip()
                weather_events.append(
                    {
                        "timestamp_utc": _iso(stamp),
                        "profile": _normalise_profile(weather_match.group("profile")),
                        "rain_rate_mm_per_hour": _finite(weather_match.group("rain")),
                        "visibility_km": _finite(weather_match.group("visibility")),
                        "self_reported_visual_status": status,
                        "self_reported_verified": status.lower() == "verified",
                    }
                )
            cap_match = _JSONL_CAP_RE.search(line)
            if cap_match:
                jsonl_caps.append(
                    {
                        "timestamp_utc": _iso(stamp),
                        "reason": cap_match.group("reason").strip(),
                    }
                )
            camera_match = _CAMERA_CAP_RE.search(line)
            if camera_match:
                camera_caps.append(
                    {
                        "timestamp_utc": _iso(stamp),
                        "node_id": camera_match.group("node"),
                        "reason": camera_match.group("reason").strip(),
                    }
                )
    return {
        "path": str(path.resolve()),
        "exists": True,
        "snapshot_bytes": snapshot_bytes,
        "bytes_after_read": path.stat().st_size,
        "grew_during_read": path.stat().st_size > snapshot_bytes,
        "airsim_runtime_plugin_startup_seen": airsim_runtime_startup_seen,
        "weather_events": weather_events,
        "weather_event_count": len(weather_events),
        "weather_event_verified_count": sum(
            event["self_reported_verified"] is True for event in weather_events
        ),
        "jsonl_cap_events": jsonl_caps,
        "camera_cap_events": camera_caps,
        "weather_related_warning_count": weather_warning_count,
        "weather_related_warning_samples": weather_warning_samples,
        "verification_semantics": (
            "Runtime self-report from TRIAD/AirSim application code; this is not pixel-level "
            "or screenshot-based visual verification."
        ),
    }


def _frame_filename_time(path: Path) -> datetime | None:
    match = _FRAME_STAMP_RE.search(path.name)
    if not match:
        return None
    try:
        return datetime.strptime(match.group("stamp"), "%Y%m%dT%H%M%S").replace(
            tzinfo=timezone.utc
        )
    except ValueError:
        return None


def _scan_frames(frames_root: Path, session_start: datetime | None) -> dict[str, object]:
    schema = SchemaTracker(FRAME_REQUIRED_FIELDS)
    by_profile: Counter[str] = Counter()
    by_node: Counter[str] = Counter()
    profile_visual: Counter[str] = Counter()
    paired = 0
    unpaired: list[str] = []
    invalid = 0
    first_time: datetime | None = None
    last_time: datetime | None = None
    visible_targets = OnlineStats()
    tagged_targets = OnlineStats()
    paths = sorted(frames_root.rglob("*_depth.json")) if frames_root.is_dir() else []
    for path in paths:
        filename_time = _frame_filename_time(path)
        if session_start is not None and filename_time is not None and filename_time < session_start:
            continue
        try:
            if path.stat().st_size > MAX_METADATA_BYTES:
                invalid += 1
                continue
            decoded = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, UnicodeDecodeError, json.JSONDecodeError):
            invalid += 1
            continue
        if not isinstance(decoded, dict):
            invalid += 1
            continue
        record_time = _parse_utc(decoded.get("timestampUtc")) or filename_time
        if session_start is not None and record_time is not None and record_time < session_start:
            continue
        schema.observe(decoded)
        if record_time is not None:
            first_time = record_time if first_time is None else min(first_time, record_time)
            last_time = record_time if last_time is None else max(last_time, record_time)
        profile = _normalise_profile(decoded.get("weatherProfile"))
        node = str(decoded.get("nodeId", "Unknown"))
        by_profile[profile] += 1
        by_node[node] += 1
        profile_visual[profile] += int(decoded.get("airSimVisualWeatherApplied") is True)
        visible_targets.add(decoded.get("visibleTargetCount"))
        tagged_targets.add(decoded.get("taggedTargetCount"))
        base = path.name[: -len("_depth.json")]
        rgb = path.with_name(base + "_rgb.png")
        depth = path.with_name(base + "_depth.png")
        if rgb.is_file() and depth.is_file():
            paired += 1
        elif len(unpaired) < 20:
            unpaired.append(str(path.resolve()))
    return {
        "root": str(frames_root.resolve()),
        "metadata_record_count": schema.record_count,
        "invalid_metadata_count": invalid,
        "first_timestamp_utc": _iso(first_time),
        "last_timestamp_utc": _iso(last_time),
        "by_weather_profile": dict(sorted(by_profile.items())),
        "visual_weather_applied_by_profile": dict(sorted(profile_visual.items())),
        "by_node": dict(sorted(by_node.items())),
        "paired_rgb_depth_metadata_count": paired,
        "unpaired_metadata_count": schema.record_count - paired,
        "unpaired_metadata_samples": unpaired,
        "visible_target_count": visible_targets.as_dict(),
        "tagged_target_count": tagged_targets.as_dict(),
        "schema": schema.as_dict(),
    }


def _profile_sources(
    rf: Mapping[tuple[str, str], RFBucket],
    cues: Mapping[str, RFCueBucket],
    frames: Mapping[str, object],
    log: Mapping[str, object],
) -> dict[str, dict[str, object]]:
    rf_counts: Counter[str] = Counter()
    rf_visual: Counter[str] = Counter()
    for (profile, _band), bucket in rf.items():
        rf_counts[profile] += bucket.samples
        rf_visual[profile] += bucket.visual_weather_applied_samples
    cue_counts = Counter(
        {profile: bucket.cue_records for profile, bucket in cues.items()}
    )
    cue_visual = Counter(
        {profile: bucket.visual_weather_applied_records for profile, bucket in cues.items()}
    )
    frame_counts = Counter(frames.get("by_weather_profile", {}))
    frame_visual = Counter(frames.get("visual_weather_applied_by_profile", {}))
    weather_events = log.get("weather_events", [])
    log_counts: Counter[str] = Counter()
    log_verified: Counter[str] = Counter()
    first_log: dict[str, datetime] = {}
    if isinstance(weather_events, list):
        for item in weather_events:
            if not isinstance(item, dict):
                continue
            profile = _normalise_profile(item.get("profile"))
            log_counts[profile] += 1
            log_verified[profile] += int(item.get("self_reported_verified") is True)
            stamp = _parse_utc(item.get("timestamp_utc"))
            if stamp is not None and (profile not in first_log or stamp < first_log[profile]):
                first_log[profile] = stamp
    cap_events = log.get("jsonl_cap_events", [])
    first_cap: datetime | None = None
    if isinstance(cap_events, list):
        for item in cap_events:
            if isinstance(item, dict):
                stamp = _parse_utc(item.get("timestamp_utc"))
                if stamp is not None:
                    first_cap = stamp if first_cap is None else min(first_cap, stamp)
    profiles = sorted(
        set(rf_counts) | set(cue_counts) | set(frame_counts) | set(log_counts)
    )
    result: dict[str, dict[str, object]] = {}
    for profile in profiles:
        detailed_sources = rf_counts[profile] + cue_counts[profile] + frame_counts[profile]
        log_only = log_counts[profile] > 0 and detailed_sources == 0
        first_activation_after_cap = (
            first_cap is not None
            and profile in first_log
            and first_log[profile] >= first_cap
        )
        result[profile] = {
            "log_activation_count": log_counts[profile],
            "log_verified_activation_count": log_verified[profile],
            "first_log_activation_utc": _iso(first_log.get(profile)),
            "rf_sample_count": rf_counts[profile],
            "rf_visual_weather_applied_count": rf_visual[profile],
            "rf_cue_record_count": cue_counts[profile],
            "rf_cue_visual_weather_applied_count": cue_visual[profile],
            "frame_metadata_count": frame_counts[profile],
            "frame_visual_weather_applied_count": frame_visual[profile],
            "only_in_logs_overall": log_only,
            "only_in_logs_because_telemetry_capped": log_only
            and first_activation_after_cap,
            "absent_from_detailed_rf_because_first_activation_followed_cap": (
                rf_counts[profile] == 0
                and log_counts[profile] > 0
                and first_activation_after_cap
            ),
        }
    return result


def run_runtime_audit(
    *,
    saved_dir: Path = DEFAULT_SAVED_DIR,
    log_path: Path = DEFAULT_LOG_PATH,
    config_path: Path | None = DEFAULT_CONFIG_PATH,
    rf_jsonl: Path | None = None,
    alerts_jsonl: Path | None = None,
    frames_root: Path | None = None,
) -> dict[str, object]:
    saved_dir = saved_dir.resolve()
    rf_path = rf_jsonl.resolve() if rf_jsonl is not None else _newest(saved_dir, "rf_links_")
    if rf_path is None:
        raise FileNotFoundError(f"No rf_links_*.jsonl found in {saved_dir}")
    session_id, session_start = _session_from_path(rf_path)
    if alerts_jsonl is None:
        matched = saved_dir / f"alerts_{session_id}.jsonl"
        alerts_path = matched if matched.is_file() else _newest(saved_dir, "alerts_")
    else:
        alerts_path = alerts_jsonl.resolve()
    frames_path = (frames_root or saved_dir / "frames").resolve()
    config = _load_config(config_path.resolve() if config_path is not None else None)

    rf_schema = SchemaTracker(RF_REQUIRED_FIELDS)
    rf_groups: dict[tuple[str, str], RFBucket] = defaultdict(RFBucket)
    nodes: dict[str, NodeBucket] = defaultdict(NodeBucket)

    def observe_rf(record: Mapping[str, object]) -> None:
        rf_schema.observe(record)
        profile = _normalise_profile(record.get("weatherProfile"))
        band = _band_name(record.get("frequencyGHz"))
        rf_groups[(profile, band)].observe(record)
        nodes[str(record.get("nodeId", "Unknown"))].observe(record, profile, band)

    rf_input = _stream_jsonl(rf_path, observe_rf)

    cue_schema = SchemaTracker(RF_PRELIMINARY_CUE_REQUIRED_FIELDS)
    legacy_alert_schema = SchemaTracker(LEGACY_TRUTH_GATED_ALERT_REQUIRED_FIELDS)
    cue_groups: dict[str, RFCueBucket] = defaultdict(RFCueBucket)

    def observe_alert(record: Mapping[str, object]) -> None:
        if record.get("alertType") == "rf_multinode_preliminary_cue":
            cue_schema.observe(record)
        else:
            legacy_alert_schema.observe(record)
        cue_groups[_normalise_profile(record.get("weatherProfile"))].observe(record)

    alert_input = _stream_jsonl(alerts_path, observe_alert)
    frames = _scan_frames(frames_path, session_start)
    log = _scan_log(log_path.resolve(), session_start)
    profiles = _profile_sources(rf_groups, cue_groups, frames, log)

    expected_nodes = set(str(item) for item in config.get("expected_nodes", []))
    observed_nodes = set(nodes)
    max_bytes = _finite(config.get("max_telemetry_file_bytes"))
    max_records = _finite(config.get("max_telemetry_records"))
    max_alerts = _finite(config.get("max_alert_records"))
    max_frames = _finite(config.get("max_frames_per_node"))
    explicit_jsonl_cap = bool(log.get("jsonl_cap_events"))
    cue_record_count = int(alert_input.get("record_count", 0))
    cue_limit_inferred = max_alerts is not None and cue_record_count >= int(max_alerts)
    frame_counts = frames.get("by_node", {})
    frame_limit_nodes = (
        sorted(
            node
            for node, count in frame_counts.items()
            if max_frames is not None and int(count) >= int(max_frames)
        )
        if isinstance(frame_counts, dict)
        else []
    )

    findings: list[dict[str, str]] = []
    findings.append(
        {
            "severity": "pass" if expected_nodes and expected_nodes == observed_nodes else "warn",
            "id": "node_coverage",
            "message": (
                f"Observed {len(observed_nodes)}/{len(expected_nodes)} configured enabled RF nodes."
                if expected_nodes
                else f"Observed {len(observed_nodes)} RF nodes; no configured-node baseline was loaded."
            ),
        }
    )
    observed_bands = {band for (_profile, band) in rf_groups}
    findings.append(
        {
            "severity": "pass" if {"2.4 GHz", "5.8 GHz"} <= observed_bands else "warn",
            "id": "dual_band_evidence",
            "message": "Detailed RF evidence contains both 2.4 GHz and 5.8 GHz."
            if {"2.4 GHz", "5.8 GHz"} <= observed_bands
            else "One or both requested RF bands are absent from detailed telemetry.",
        }
    )
    if explicit_jsonl_cap:
        findings.append(
            {
                "severity": "warn",
                "id": "rf_jsonl_cap",
                "message": "The Unreal log explicitly reports that detailed JSONL RF telemetry stopped at its byte limit.",
            }
        )
    if cue_limit_inferred:
        findings.append(
            {
                "severity": "warn",
                "id": "rf_cue_record_cap",
                "message": (
                    f"The legacy-named cue JSONL contains {cue_record_count} records, equal to the configured "
                    f"MaxAlertRecords={int(max_alerts)}; later on-screen/log cues continue but are not represented in this file."
                ),
            }
        )
    log_only = [
        name
        for name, detail in profiles.items()
        if detail["only_in_logs_because_telemetry_capped"] is True
    ]
    rf_absent_due_cap = [
        name
        for name, detail in profiles.items()
        if detail[
            "absent_from_detailed_rf_because_first_activation_followed_cap"
        ]
        is True
    ]
    findings.append(
        {
            "severity": "warn" if log_only or rf_absent_due_cap else "pass",
            "id": "post_cap_weather_coverage",
            "message": (
                f"Profiles only in logs after the telemetry cap: {', '.join(log_only)}."
                if log_only
                else (
                    "No weather profile is log-only across all evidence channels. "
                    + (
                        "Detailed RF samples are absent after the cap for: "
                        + ", ".join(rf_absent_due_cap)
                        + "."
                        if rf_absent_due_cap
                        else "Every logged profile has detailed evidence."
                    )
                )
            ),
        }
    )
    weather_events = log.get("weather_events", [])
    verified = int(log.get("weather_event_verified_count", 0))
    event_count = int(log.get("weather_event_count", 0))
    findings.append(
        {
            "severity": "pass" if event_count > 0 and verified == event_count else "warn",
            "id": "airsim_visual_weather_self_report",
            "message": (
                f"{verified}/{event_count} logged weather activations self-reported AirSim visual weather as verified; "
                "this is not pixel-level verification."
            ),
        }
    )
    if int(log.get("weather_related_warning_count", 0)):
        findings.append(
            {
                "severity": "warn",
                "id": "weather_runtime_warnings",
                "message": (
                    f"The log contains {int(log['weather_related_warning_count'])} weather-related warning line(s); "
                    "see log evidence in the JSON report."
                ),
            }
        )

    overall = "warn" if any(item["severity"] == "warn" for item in findings) else "pass"
    return {
        "schema_version": "1.0",
        "generated_at_utc": _utc_now(),
        "scope": {
            "simulation_only": True,
            "detection_only": True,
            "real_world_performance_claim": False,
            "visual_weather_verification_is_runtime_self_report": True,
            "audit_read_only_with_respect_to_unreal": True,
        },
        "overall_status": overall,
        "session": {
            "session_id": session_id,
            "session_start_utc_from_filename": _iso(session_start),
            "saved_dir": str(saved_dir),
        },
        "inputs": {
            "rf_jsonl": rf_input,
            "alerts_jsonl": alert_input,
            "frames_root": str(frames_path),
            "unreal_log": str(log_path.resolve()),
            "config": config,
        },
        "limits": {
            "rf_jsonl_cap_explicitly_logged": explicit_jsonl_cap,
            "configured_max_telemetry_records": max_records,
            "configured_max_telemetry_file_bytes": max_bytes,
            "rf_file_fraction_of_byte_limit": (
                float(rf_input.get("snapshot_bytes", 0)) / max_bytes
                if max_bytes and max_bytes > 0
                else None
            ),
            "rf_cue_record_limit_reached_inferred": cue_limit_inferred,
            "configured_max_alert_records": max_alerts,
            "camera_frame_limit_nodes": frame_limit_nodes,
            "configured_max_frames_per_node": max_frames,
        },
        "rf": {
            "schema": rf_schema.as_dict(),
            "by_weather_and_band": {
                profile: {
                    band: rf_groups[(profile, band)].as_dict()
                    for band in sorted(b for (p, b) in rf_groups if p == profile)
                }
                for profile in sorted({p for (p, _b) in rf_groups})
            },
            "node_coverage": {
                "expected_nodes": sorted(expected_nodes),
                "observed_nodes": sorted(observed_nodes),
                "missing_nodes": sorted(expected_nodes - observed_nodes),
                "unexpected_nodes": sorted(observed_nodes - expected_nodes),
                "all_expected_nodes_observed": bool(expected_nodes)
                and expected_nodes <= observed_nodes,
                "by_node": {
                    name: bucket.as_dict() for name, bucket in sorted(nodes.items())
                },
            },
        },
        "rf_preliminary_cues": {
            "schema": cue_schema.as_dict(),
            "legacy_truth_gated_alert_schema": legacy_alert_schema.as_dict(),
            "by_weather_profile": {
                name: bucket.as_dict()
                for name, bucket in sorted(cue_groups.items())
            },
        },
        "frames": frames,
        "weather": {
            "profile_evidence": profiles,
            "log_evidence": log,
        },
        "findings": findings,
    }


def _metric(stats: Mapping[str, object], key: str, digits: int = 1) -> str:
    value = stats.get(key)
    number = _finite(value)
    return "n/a" if number is None else f"{number:.{digits}f}"


def _fraction(numerator: int, denominator: int) -> str:
    return "n/a" if denominator <= 0 else f"{100.0 * numerator / denominator:.1f}%"


def render_markdown(report: Mapping[str, object]) -> str:
    session = report["session"]
    inputs = report["inputs"]
    limits = report["limits"]
    rf = report["rf"]
    cues = report["rf_preliminary_cues"]
    frames = report["frames"]
    weather = report["weather"]
    lines = [
        "# Unreal Weather + RF Runtime Audit",
        "",
        f"**Result:** `{report['overall_status']}`  ",
        f"**Generated:** {report['generated_at_utc']}  ",
        f"**Session:** `{session['session_id']}` (start inferred from filename: {session['session_start_utc_from_filename']})",
        "",
        "This is a streaming audit of simulation telemetry. It is detection-only, does not validate real-world detection performance, and does not contain engagement logic.",
        "",
        "## Executive findings",
        "",
    ]
    for item in report["findings"]:
        marker = {"pass": "PASS", "warn": "WARN", "fail": "FAIL"}.get(
            item["severity"], str(item["severity"]).upper()
        )
        lines.append(f"- **{marker} — {item['id']}:** {item['message']}")

    rf_input = inputs["rf_jsonl"]
    alert_input = inputs["alerts_jsonl"]
    lines.extend(
        [
            "",
            "## Runtime evidence window",
            "",
            "| Evidence | Records | First UTC | Last UTC | Integrity |",
            "|---|---:|---|---|---|",
            f"| Detailed RF JSONL | {rf_input['record_count']:,} | {rf_input['first_timestamp_utc']} | {rf_input['last_timestamp_utc']} | {rf_input['invalid_line_count']} invalid, {rf_input['overlong_line_count']} overlong |",
            f"| RF preliminary-cue JSONL (legacy filename) | {alert_input['record_count']:,} | {alert_input['first_timestamp_utc']} | {alert_input['last_timestamp_utc']} | {alert_input['invalid_line_count']} invalid, {alert_input['overlong_line_count']} overlong |",
            f"| RGB/depth frame metadata | {frames['metadata_record_count']:,} | {frames['first_timestamp_utc']} | {frames['last_timestamp_utc']} | {frames['invalid_metadata_count']} invalid |",
            "",
        ]
    )
    if limits["rf_jsonl_cap_explicitly_logged"]:
        fraction = _finite(limits.get("rf_file_fraction_of_byte_limit"))
        percent = f"{100.0 * fraction:.3f}%" if fraction is not None else "n/a"
        lines.append(
            f"> Detailed RF JSONL stopped at the configured byte cap ({percent} of the configured limit in the captured file). Counts after that timestamp must come from cue or log evidence, not detailed per-link JSONL."
        )

    lines.extend(
        [
            "",
            "## RF detections by weather and band",
            "",
            "`Preliminary cue evidence` is the truth-independent per-link RF detection policy. For legacy rows without that field, the audit migrates from `detected=true` and reports old truth-gated `alertEvidence` separately.",
            "",
            "| Weather | Band | Samples | Detected | Detected on authored hostile truth (evaluator only) | Preliminary cue evidence | Legacy truth-gated evidence | Detection rate | SNR dB min / mean / max | Weather loss dB mean / max | Range km mean / max |",
            "|---|---|---:|---:|---:|---:|---:|---:|---|---|---|",
        ]
    )
    grouped = rf["by_weather_and_band"]
    for profile, bands in grouped.items():
        for band, bucket in bands.items():
            snr = bucket["snr_db"]
            loss = bucket["weather_rf_loss_db"]
            range_m = bucket["range_m"]
            range_mean = _finite(range_m.get("mean"))
            range_max = _finite(range_m.get("max"))
            lines.append(
                f"| {profile} | {band} | {bucket['sample_count']:,} | {bucket['detection_count']:,} | "
                f"{bucket['detections_on_authored_hostile_truth_count']:,} | "
                f"{bucket['preliminary_cue_evidence_sample_count']:,} | "
                f"{bucket['legacy_truth_gated_alert_evidence_sample_count']:,} | "
                f"{_fraction(bucket['detection_count'], bucket['sample_count'])} | "
                f"{_metric(snr, 'min')} / {_metric(snr, 'mean')} / {_metric(snr, 'max')} | "
                f"{_metric(loss, 'mean', 3)} / {_metric(loss, 'max', 3)} | "
                f"{('n/a' if range_mean is None else f'{range_mean / 1000.0:.2f}')} / "
                f"{('n/a' if range_max is None else f'{range_max / 1000.0:.2f}')} |"
            )

    lines.extend(
        [
            "",
            "## Emitted preliminary RF cues by weather",
            "",
            "| Weather | Cue records | Generic v2 cues | Legacy truth-gated records | Detection-only | Operator tracks | Confirming nodes mean / max | Max SNR mean | Nearest range m mean / max |",
            "|---|---:|---:|---:|---:|---:|---|---:|---|",
        ]
    )
    for profile, bucket in cues["by_weather_profile"].items():
        confirming = bucket["confirming_node_count"]
        nearest = bucket["nearest_confirming_node_range_m"]
        lines.append(
            f"| {profile} | {bucket['cue_record_count']:,} | {bucket['generic_preliminary_cue_record_count']:,} | "
            f"{bucket['legacy_truth_gated_alert_record_count']:,} | {bucket['detection_only_record_count']:,} | "
            f"{bucket['operator_track_count']:,} | {_metric(confirming, 'mean', 2)} / {_metric(confirming, 'max', 0)} | "
            f"{_metric(bucket['max_snr_db'], 'mean')} | {_metric(nearest, 'mean', 1)} / {_metric(nearest, 'max', 1)} |"
        )

    lines.extend(
        [
            "",
            "## Weather evidence and AirSim visual status",
            "",
            "| Weather | Logged activations verified / total | RF samples | RF cue records | Camera frames | Coverage note |",
            "|---|---:|---:|---:|---:|---|",
        ]
    )
    for profile, detail in weather["profile_evidence"].items():
        if detail["only_in_logs_because_telemetry_capped"]:
            note = "Log-only; first activation followed JSONL cap"
        elif detail["absent_from_detailed_rf_because_first_activation_followed_cap"]:
            note = "No detailed RF: first activation followed JSONL cap; other evidence exists"
        else:
            note = "Detailed evidence present"
        lines.append(
            f"| {profile} | {detail['log_verified_activation_count']} / {detail['log_activation_count']} | "
            f"{detail['rf_sample_count']:,} | {detail['rf_cue_record_count']:,} | {detail['frame_metadata_count']:,} | {note} |"
        )
    log = weather["log_evidence"]
    lines.extend(
        [
            "",
            f"AirSim weather verification is a **runtime self-report**, not image-level proof. The log reported {log['weather_related_warning_count']} weather-related warning line(s).",
            "",
            "## RF node coverage",
            "",
            f"Configured enabled nodes observed: **{len(rf['node_coverage']['observed_nodes'])}/{len(rf['node_coverage']['expected_nodes'])}**.",
            "",
            "| Node | RF samples | Detections | Preliminary cue evidence | Bands | Weather profiles | Mean range km | Mean SNR dB |",
            "|---|---:|---:|---:|---|---|---:|---:|",
        ]
    )
    for node, detail in rf["node_coverage"]["by_node"].items():
        mean_range = _finite(detail["range_m"].get("mean"))
        lines.append(
            f"| {node} | {detail['sample_count']:,} | {detail['detection_count']:,} | "
            f"{detail['preliminary_cue_evidence_sample_count']:,} | {', '.join(detail['bands'])} | "
            f"{', '.join(detail['weather_profiles'])} | "
            f"{('n/a' if mean_range is None else f'{mean_range / 1000.0:.2f}')} | "
            f"{_metric(detail['snr_db'], 'mean')} |"
        )

    lines.extend(
        [
            "",
            "## Camera metadata",
            "",
            f"- Frame metadata: {frames['metadata_record_count']:,}; complete RGB/depth/metadata triplets: {frames['paired_rgb_depth_metadata_count']:,}.",
            f"- Frames by node: `{json.dumps(frames['by_node'], sort_keys=True)}`.",
            f"- Frames by weather: `{json.dumps(frames['by_weather_profile'], sort_keys=True)}`.",
            f"- Target projection semantics are metadata/frustum projections, not an occlusion-tested detector result.",
            "",
            "## Schema completeness",
            "",
            "| Stream | Records | Fully complete records | Required-cell completeness | Missing required fields |",
            "|---|---:|---:|---:|---|",
        ]
    )
    schema_rows = (
        ("RF", rf["schema"]),
        ("RF preliminary cues v2", cues["schema"]),
        ("Legacy truth-gated alerts", cues["legacy_truth_gated_alert_schema"]),
        ("Frames", frames["schema"]),
    )
    for name, schema in schema_rows:
        missing = schema["missing_record_count_by_field"]
        lines.append(
            f"| {name} | {schema['record_count']:,} | {schema['complete_record_count']:,} "
            f"({schema['complete_record_percent']:.2f}%) | {schema['required_cell_completeness_percent']:.3f}% | "
            f"{('none' if not missing else '`' + json.dumps(missing, sort_keys=True) + '`')} |"
        )

    lines.extend(
        [
            "",
            "## Interpretation boundaries",
            "",
            "- Weather RF loss, free-space loss, LOS/NLOS, and scenario truth are simulator-generated fields; they are not calibrated field measurements.",
            "- `airSimVisualWeatherApplied=true` and `visual weather verified` are application self-reports. Pixel-level visual change was not tested by this audit.",
            "- Detection rate here is the fraction of simulated link samples passing configured gates, not precision, recall, or probability of detection on real hardware.",
            "- Generic RF cues are unclassified, truth-independent, detection-only, and report `actionsTaken=none`; legacy truth-gated records are counted separately.",
            "",
            "## Reproduce",
            "",
            "```powershell",
            "cd <CLONE>\\core",
            "python -m singapore_sensor_fusion.runtime_audit `",
            f"  --saved-dir \"{report['session']['saved_dir']}\" `",
            f"  --log \"{inputs['unreal_log']}\" `",
            f"  --config \"{inputs['config']['path']}\" `",
            "  --json-output runtime_audit.json `",
            "  --markdown-output runtime_audit.md",
            "```",
            "",
            "## Audited sources",
            "",
            f"- RF: `{rf_input['path']}`",
            f"- RF cues (legacy `alerts_*.jsonl` filename): `{alert_input['path']}`",
            f"- Frames: `{inputs['frames_root']}`",
            f"- Unreal log: `{inputs['unreal_log']}`",
            f"- Config: `{inputs['config']['path']}`",
            "",
        ]
    )
    return "\n".join(lines)


def _write_atomic(path: Path, text: str) -> Path:
    path = path.resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        "w", encoding="utf-8", newline="\n", dir=path.parent, delete=False
    ) as handle:
        temporary = Path(handle.name)
        handle.write(text)
    temporary.replace(path)
    return path


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--saved-dir", type=Path, default=DEFAULT_SAVED_DIR)
    parser.add_argument("--log", type=Path, default=DEFAULT_LOG_PATH)
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG_PATH)
    parser.add_argument("--rf-jsonl", type=Path)
    parser.add_argument("--alerts-jsonl", type=Path)
    parser.add_argument("--frames-root", type=Path)
    parser.add_argument("--json-output", type=Path, default=Path("runtime_audit.json"))
    parser.add_argument(
        "--markdown-output", type=Path, default=Path("runtime_audit.md")
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    report = run_runtime_audit(
        saved_dir=args.saved_dir,
        log_path=args.log,
        config_path=args.config,
        rf_jsonl=args.rf_jsonl,
        alerts_jsonl=args.alerts_jsonl,
        frames_root=args.frames_root,
    )
    json_path = _write_atomic(
        args.json_output,
        json.dumps(report, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
    )
    markdown_path = _write_atomic(args.markdown_output, render_markdown(report))
    print(
        json.dumps(
            {
                "status": report["overall_status"],
                "session": report["session"]["session_id"],
                "rf_records": report["inputs"]["rf_jsonl"]["record_count"],
                "rf_cue_records": report["inputs"]["alerts_jsonl"]["record_count"],
                "frame_records": report["frames"]["metadata_record_count"],
                "json_output": str(json_path),
                "markdown_output": str(markdown_path),
            },
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
