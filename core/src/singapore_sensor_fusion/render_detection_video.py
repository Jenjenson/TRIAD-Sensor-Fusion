"""Render an evidence-labelled MP4 from Unreal sensor-capture frames.

The renderer deliberately distinguishes simulation ground truth, RF/fusion
alerts, and Event-YOLO candidates.  In particular, candidates inferred from
``proxy_event_stack_v1`` are never labelled as true positives or as physical
event-camera measurements.

Run with::

    python -m singapore_sensor_fusion.render_detection_video \
        --frames-root <TRIAD_PROJECT>/Saved/SingaporeSensorFusion/frames \
        --node City_Sector --output detection_demo.mp4

Pass ``--event-model`` to add a side-by-side RGB-derived event proxy and run
the hash-pinned :class:`~singapore_sensor_fusion.event_yolo.EventYOLOAdapter`.
OpenCV is preferred for image I/O and MP4 encoding.  Pillow plus an ``ffmpeg``
executable is the fallback; a clear error is raised if neither route exists.
"""

from __future__ import annotations

import argparse
from collections import deque
from dataclasses import dataclass
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from typing import Any, Callable, Iterable, Mapping, Protocol, Sequence
from uuid import uuid4

import numpy as np
from numpy.typing import NDArray

from .event_camera import PROXY_EVENT_ENCODING, ProxyEventProjector
from .event_yolo import EventYOLOAdapter


_FILENAME_TIME = re.compile(r"(?P<stamp>\d{8}T\d{6})(?:[._]\d+|f)?Z", re.IGNORECASE)
_EVENT_DISCLAIMER = (
    "RGB-derived proxy_event_stack_v1 is not equivalent to the unavailable "
    "training preprocessing or to a physical neuromorphic sensor. Event-YOLO "
    "outputs are candidates only; this video makes no true-positive claim."
)
_GROUND_TRUTH_DISCLAIMER = (
    "Boxes labelled SIM GROUND TRUTH come from Unreal metadata, not a detector."
)
_DETECTION_ALERT_LABEL = "RF MULTI-NODE DETECTION ALERT"
_SCENARIO_RED_TEAM_QUALIFIER = "SCENARIO RED-TEAM"


def _utc_now() -> datetime:
    return datetime.now(timezone.utc)


def _parse_timestamp(value: object) -> datetime | None:
    if not isinstance(value, str) or not value.strip():
        return None
    text = value.strip()
    if text.endswith("Z"):
        text = text[:-1] + "+00:00"
    try:
        parsed = datetime.fromisoformat(text)
    except ValueError:
        match = _FILENAME_TIME.search(value)
        if match is None:
            return None
        parsed = datetime.strptime(match.group("stamp"), "%Y%m%dT%H%M%S").replace(
            tzinfo=timezone.utc
        )
    if parsed.tzinfo is None or parsed.utcoffset() is None:
        parsed = parsed.replace(tzinfo=timezone.utc)
    return parsed.astimezone(timezone.utc)


def _json_object(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        return {}
    return value if isinstance(value, dict) else {}


@dataclass(frozen=True, slots=True)
class FrameRecord:
    rgb_path: Path
    metadata_path: Path | None
    timestamp: datetime
    node_id: str
    frame_index: int | None
    metadata: Mapping[str, Any]


def _frame_record(rgb_path: Path, node_id: str) -> FrameRecord | None:
    suffix = "_rgb.png"
    if not rgb_path.name.lower().endswith(suffix):
        return None
    stem = rgb_path.name[: -len(suffix)]
    metadata_path = rgb_path.with_name(stem + "_depth.json")
    metadata = _json_object(metadata_path) if metadata_path.is_file() else {}
    timestamp = _parse_timestamp(
        metadata.get("timestampUtc", metadata.get("timestamp", rgb_path.name))
    )
    if timestamp is None:
        try:
            timestamp = datetime.fromtimestamp(rgb_path.stat().st_mtime, tz=timezone.utc)
        except OSError:
            return None
    raw_index = metadata.get("frameIndex")
    frame_index = int(raw_index) if isinstance(raw_index, (int, float)) else None
    actual_node = metadata.get("nodeId", node_id)
    if not isinstance(actual_node, str) or not actual_node.strip():
        actual_node = node_id
    return FrameRecord(
        rgb_path=rgb_path.resolve(),
        metadata_path=metadata_path.resolve() if metadata_path.is_file() else None,
        timestamp=timestamp,
        node_id=actual_node.strip(),
        frame_index=frame_index,
        metadata=metadata,
    )


def _node_directories(frames_root: Path, node_id: str | None) -> list[tuple[str, Path]]:
    root = frames_root.expanduser().resolve()
    if node_id:
        candidate = root / node_id
        if candidate.is_dir():
            return [(node_id, candidate)]
        if root.is_dir() and root.name.casefold() == node_id.casefold():
            return [(node_id, root)]
        raise FileNotFoundError(f"Frame directory for node {node_id!r} was not found under {root}")
    if not root.is_dir():
        raise FileNotFoundError(f"Frames root was not found: {root}")
    if any(root.glob("*_rgb.png")):
        return [(root.name, root)]
    return [(item.name, item) for item in root.iterdir() if item.is_dir()]


def _bounded_uniform(records: Sequence[FrameRecord], maximum: int) -> list[FrameRecord]:
    if len(records) <= maximum:
        return list(records)
    if maximum == 1:
        return [records[-1]]
    indices = [round(index * (len(records) - 1) / (maximum - 1)) for index in range(maximum)]
    return [records[index] for index in indices]


def select_frame_records(
    frames_root: str | Path,
    *,
    node_id: str | None = None,
    max_frames: int = 180,
    cadence_seconds: float = 0.0,
    session_gap_seconds: float = 30.0,
) -> list[FrameRecord]:
    """Select a bounded, chronological sample from the latest frame session.

    A session boundary is a timestamp gap larger than ``session_gap_seconds``.
    When no node is requested, the node whose session has the newest frame is
    selected.  Uniform bounding preserves both ends of a long session.
    """

    if not isinstance(max_frames, int) or max_frames < 1:
        raise ValueError("max_frames must be an integer >= 1")
    if not math.isfinite(cadence_seconds) or cadence_seconds < 0.0:
        raise ValueError("cadence_seconds must be finite and >= 0")
    if not math.isfinite(session_gap_seconds) or session_gap_seconds <= 0.0:
        raise ValueError("session_gap_seconds must be finite and > 0")

    sessions: list[list[FrameRecord]] = []
    for discovered_id, directory in _node_directories(Path(frames_root), node_id):
        records = [
            record
            for path in directory.glob("*_rgb.png")
            if (record := _frame_record(path, discovered_id)) is not None
        ]
        records.sort(key=lambda item: (item.timestamp, item.rgb_path.name))
        if not records:
            continue
        start = 0
        for index in range(1, len(records)):
            gap = (records[index].timestamp - records[index - 1].timestamp).total_seconds()
            if gap > session_gap_seconds:
                start = index
        sessions.append(records[start:])
    if not sessions:
        raise FileNotFoundError(f"No timestamped *_rgb.png frames were found under {frames_root}")
    latest = max(sessions, key=lambda values: values[-1].timestamp)

    cadence_filtered: list[FrameRecord] = []
    for record in latest:
        if not cadence_filtered:
            cadence_filtered.append(record)
            continue
        delta = (record.timestamp - cadence_filtered[-1].timestamp).total_seconds()
        if delta + 1e-9 >= cadence_seconds:
            cadence_filtered.append(record)
    if latest[-1] is not cadence_filtered[-1] and len(cadence_filtered) < max_frames:
        cadence_filtered.append(latest[-1])
    return _bounded_uniform(cadence_filtered, max_frames)


@dataclass(frozen=True, slots=True)
class AlertRecord:
    timestamp: datetime
    node_id: str | None
    target_id: str | None
    hostile: bool
    payload: Mapping[str, Any]
    confirming_node_ids: tuple[str, ...] = ()
    nearest_node_id: str | None = None
    nearest_node_distance_meters: float | None = None
    scenario_red_team: bool = False


def _first_text(value: Mapping[str, Any], names: Iterable[str]) -> str | None:
    for name in names:
        item = value.get(name)
        if isinstance(item, str) and item.strip():
            return item.strip()
    return None


def _truthy_hostile(value: Mapping[str, Any]) -> bool:
    for key in (
        "hostile",
        "isHostile",
        "enemy",
        "isEnemy",
        "hostileScenarioTruth",
    ):
        if isinstance(value.get(key), bool):
            return bool(value[key])
    text = _first_text(
        value,
        ("affiliation", "classification", "threatClass", "threat_class", "team", "status"),
    )
    return text is not None and text.casefold() in {
        "hostile",
        "enemy",
        "foe",
        "red",
        "confirmed_hostile",
    }


def _scenario_red_team(value: Mapping[str, Any]) -> bool:
    """Return explicit scenario affiliation without inferring it from a detection.

    A sensor alert, confidence score, or generic ``hostile`` field is not enough
    to establish scenario affiliation.  The overlay may use the red-team
    qualifier only when the source record carries scenario-truth provenance.
    """

    for key in (
        "hostileScenarioTruth",
        "redTeamScenario",
        "red_team_scenario",
        "scenarioRedTeam",
        "scenario_red_team",
    ):
        if value.get(key) is True:
            return True
    text = _first_text(
        value,
        (
            "scenarioAffiliation",
            "scenario_affiliation",
            "scenarioTeam",
            "scenario_team",
            "scenarioSide",
            "scenario_side",
        ),
    )
    return text is not None and text.casefold().replace("-", "_").replace(" ", "_") in {
        "red",
        "red_team",
        "hostile",
        "enemy",
    }


def _valid_distance_meters(value: object) -> float | None:
    distance = _number(value)
    return distance if distance is not None and distance >= 0.0 else None


def _alert_nearest_node(
    value: Mapping[str, Any],
) -> tuple[str | None, float | None]:
    """Extract the nearest RF-node range from current and legacy alert schemas."""

    nearest_node_id = _first_text(
        value,
        (
            "nearestNodeId",
            "nearest_node_id",
            "nearestConfirmingNodeId",
            "nearest_confirming_node_id",
        ),
    )
    for key in (
        "nearestNodeDistanceMeters",
        "nearest_node_distance_meters",
        "nearestConfirmingNodeDistanceMeters",
        "nearest_confirming_node_distance_meters",
        "minimumNodeDistanceMeters",
        "minNodeDistanceMeters",
        "rangeMeters",
        "distanceMeters",
    ):
        distance = _valid_distance_meters(value.get(key))
        if distance is not None:
            return nearest_node_id, distance

    for key in ("nearestNode", "nearest_node", "nearestConfirmingNode"):
        nested = value.get(key)
        if not isinstance(nested, dict):
            continue
        nested_id = _first_text(nested, ("nodeId", "node_id", "id", "name"))
        for distance_key in (
            "distanceMeters",
            "distance_meters",
            "slantDistanceMeters",
            "slant_distance_meters",
            "rangeMeters",
            "range_meters",
        ):
            distance = _valid_distance_meters(nested.get(distance_key))
            if distance is not None:
                return nested_id or nearest_node_id, distance

    candidates: list[tuple[float, str | None]] = []
    for key in ("confirmingNodes", "confirming_nodes", "nodeDetections"):
        nodes = value.get(key)
        if not isinstance(nodes, list):
            continue
        for node in nodes:
            if not isinstance(node, dict):
                continue
            node_id = _first_text(node, ("nodeId", "node_id", "id", "name"))
            distance = next(
                (
                    parsed
                    for distance_key in (
                        "distanceMeters",
                        "distance_meters",
                        "slantDistanceMeters",
                        "slant_distance_meters",
                        "rangeMeters",
                        "range_meters",
                    )
                    if (parsed := _valid_distance_meters(node.get(distance_key)))
                    is not None
                ),
                None,
            )
            if distance is not None:
                candidates.append((distance, node_id))

    for key in ("nodeDistancesMeters", "node_distances_meters"):
        distances = value.get(key)
        if not isinstance(distances, dict):
            continue
        for node_id, raw_distance in distances.items():
            distance = _valid_distance_meters(raw_distance)
            if distance is not None:
                candidates.append(
                    (distance, str(node_id).strip() if str(node_id).strip() else None)
                )
    if candidates:
        distance, node_id = min(candidates, key=lambda item: item[0])
        return node_id or nearest_node_id, distance
    return nearest_node_id, None


def _format_distance(distance_meters: object) -> str | None:
    """Format a valid range in metres, switching to kilometres at 1 km."""

    distance = _valid_distance_meters(distance_meters)
    if distance is None:
        return None
    if distance >= 1_000.0:
        return f"{distance / 1_000.0:.2f} km"
    return f"{distance:.0f} m"


def load_alert_records(path: str | Path | None, *, max_records: int = 50_000) -> list[AlertRecord]:
    """Read a bounded tail of alert JSONL records, ignoring malformed lines."""

    if path is None:
        return []
    alert_path = Path(path).expanduser().resolve()
    if not alert_path.is_file():
        raise FileNotFoundError(f"Alerts JSONL was not found: {alert_path}")
    if not isinstance(max_records, int) or max_records < 1:
        raise ValueError("max_records must be an integer >= 1")
    bounded: deque[AlertRecord] = deque(maxlen=max_records)
    with alert_path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            try:
                value = json.loads(line)
            except json.JSONDecodeError:
                continue
            if not isinstance(value, dict):
                continue
            timestamp = _parse_timestamp(
                value.get("timestampUtc", value.get("timestamp", value.get("timestamp_utc")))
            )
            if timestamp is None:
                continue
            node = _first_text(value, ("nodeId", "node_id", "receiverId", "receiver_id"))
            target = _first_text(
                value,
                (
                    "targetId",
                    "target_id",
                    "trackId",
                    "track_id",
                    "targetActor",
                    "emitterId",
                ),
            )
            raw_confirming_nodes = value.get("confirmingNodeIds", ())
            confirming_nodes = (
                tuple(
                    item.strip()
                    for item in raw_confirming_nodes
                    if isinstance(item, str) and item.strip()
                )
                if isinstance(raw_confirming_nodes, list)
                else ()
            )
            nearest_node_id, nearest_node_distance_meters = _alert_nearest_node(value)
            bounded.append(
                AlertRecord(
                    timestamp=timestamp,
                    node_id=node,
                    target_id=target,
                    hostile=_truthy_hostile(value),
                    payload=value,
                    confirming_node_ids=confirming_nodes,
                    nearest_node_id=nearest_node_id,
                    nearest_node_distance_meters=nearest_node_distance_meters,
                    scenario_red_team=_scenario_red_team(value),
                )
            )
    return sorted(bounded, key=lambda item: item.timestamp)


def _target_ids(metadata: Mapping[str, Any]) -> set[str]:
    targets = metadata.get("targets", ())
    if not isinstance(targets, list):
        return set()
    result: set[str] = set()
    for target in targets:
        if isinstance(target, dict):
            target_id = _first_text(
                target,
                ("targetId", "target_id", "id", "name", "actorName", "targetActor"),
            )
            if target_id:
                result.add(target_id.casefold())
    return result


def recent_matching_alert(
    record: FrameRecord,
    alerts: Sequence[AlertRecord],
    *,
    window_seconds: float = 5.0,
) -> AlertRecord | None:
    if not math.isfinite(window_seconds) or window_seconds < 0.0:
        raise ValueError("window_seconds must be finite and >= 0")
    targets = _target_ids(record.metadata)
    matches: list[AlertRecord] = []
    for alert in alerts:
        age = (record.timestamp - alert.timestamp).total_seconds()
        if age < -0.25 or age > window_seconds:
            continue
        if alert.node_id and alert.node_id.casefold() != record.node_id.casefold():
            continue
        if alert.confirming_node_ids and record.node_id.casefold() not in {
            item.casefold() for item in alert.confirming_node_ids
        }:
            continue
        if alert.target_id and targets and alert.target_id.casefold() not in targets:
            continue
        matches.append(alert)
    return max(matches, key=lambda item: item.timestamp) if matches else None


@dataclass(frozen=True, slots=True)
class GroundTruthOverlay:
    box_xyxy: tuple[int, int, int, int]
    hostile: bool
    label: str
    target_id: str
    distance_meters: float | None = None


def _number(value: object) -> float | None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    number = float(value)
    return number if math.isfinite(number) else None


def _box_values(value: object, *, format_hint: str = "") -> tuple[float, float, float, float] | None:
    if isinstance(value, (list, tuple)) and len(value) == 4:
        numbers = tuple(_number(item) for item in value)
        if all(item is not None for item in numbers):
            x1, y1, third, fourth = (float(item) for item in numbers)  # type: ignore[arg-type]
            if "xywh" in format_hint.casefold():
                return x1, y1, x1 + third, y1 + fourth
            return x1, y1, third, fourth
    if not isinstance(value, dict):
        return None
    aliases = (
        ("xMin", "yMin", "xMax", "yMax"),
        ("xmin", "ymin", "xmax", "ymax"),
        ("minX", "minY", "maxX", "maxY"),
        ("left", "top", "right", "bottom"),
        ("x1", "y1", "x2", "y2"),
    )
    for names in aliases:
        numbers = tuple(_number(value.get(name)) for name in names)
        if all(item is not None for item in numbers):
            return tuple(float(item) for item in numbers)  # type: ignore[return-value]
    xywh = tuple(_number(value.get(name)) for name in ("x", "y", "width", "height"))
    if all(item is not None for item in xywh):
        x, y, width, height = (float(item) for item in xywh)  # type: ignore[arg-type]
        return x, y, x + width, y + height
    return None


def extract_ground_truth_overlays(
    metadata: Mapping[str, Any], width: int, height: int
) -> list[GroundTruthOverlay]:
    """Normalize supported ``targets`` box schemas into drawable overlays."""

    if width < 1 or height < 1:
        raise ValueError("width and height must be positive")
    raw_targets = metadata.get("targets", ())
    if not isinstance(raw_targets, list):
        return []
    overlays: list[GroundTruthOverlay] = []
    normalized_keys = (
        "bbox_xyxy_normalized",
        "bboxNormalized",
        "bbox_normalized",
        "screenBoxNormalized",
        "screen_bounds_normalized",
    )
    pixel_keys = (
        "bbox_xyxy",
        "bboxXyxyPixels",
        "bboxPixels",
        "bbox_pixels",
        "screenBoxPixels",
        "bbox",
    )
    for index, target in enumerate(raw_targets):
        if not isinstance(target, dict):
            continue
        if target.get("inFront") is False or target.get("intersectsFrame") is False:
            continue
        values: tuple[float, float, float, float] | None = None
        normalized = False
        format_hint = str(target.get("bboxFormat", target.get("bbox_format", "")))
        for key in normalized_keys:
            if key in target:
                values = _box_values(target[key], format_hint=format_hint)
                normalized = values is not None
                break
        if values is None:
            for key in pixel_keys:
                if key in target:
                    values = _box_values(target[key], format_hint=format_hint)
                    if values is not None:
                        normalized = all(0.0 <= item <= 1.0 for item in values)
                        break
        if values is None:
            continue
        x1, y1, x2, y2 = values
        if normalized:
            x1, x2 = x1 * width, x2 * width
            y1, y2 = y1 * height, y2 * height
        left = max(0, min(width - 1, int(round(x1))))
        top = max(0, min(height - 1, int(round(y1))))
        right = max(0, min(width - 1, int(round(x2))))
        bottom = max(0, min(height - 1, int(round(y2))))
        if right <= left or bottom <= top:
            continue
        target_id = _first_text(
            target,
            ("targetId", "target_id", "id", "name", "actorName", "targetActor"),
        ) or f"target-{index + 1}"
        hostile = _truthy_hostile(target)
        side = "HOSTILE" if hostile else "FRIENDLY"
        distance_meters = _valid_distance_meters(
            target.get("distanceMeters", target.get("distance_meters"))
        )
        formatted_distance = _format_distance(distance_meters)
        range_label = f" | RANGE {formatted_distance}" if formatted_distance else ""
        overlays.append(
            GroundTruthOverlay(
                box_xyxy=(left, top, right, bottom),
                hostile=hostile,
                # Keep the range before the actor name so it remains visible
                # when a box begins near the right edge of a camera frame.
                label=f"SIM GROUND TRUTH | {side}{range_label} | {target_id}",
                target_id=target_id,
                distance_meters=distance_meters,
            )
        )
    return overlays


class ImageOperations(Protocol):
    name: str

    def read_rgb(self, path: Path) -> NDArray[np.uint8]: ...

    def resize(self, image: NDArray[np.uint8], width: int, height: int) -> NDArray[np.uint8]: ...

    def box(
        self,
        image: NDArray[np.uint8],
        bounds: tuple[int, int, int, int],
        color: tuple[int, int, int],
        label: str,
    ) -> None: ...

    def text(
        self,
        image: NDArray[np.uint8],
        text: str,
        origin: tuple[int, int],
        color: tuple[int, int, int],
        *,
        scale: float = 0.55,
    ) -> None: ...

    def banner(self, image: NDArray[np.uint8], text: str) -> None: ...


class _Cv2Operations:
    name = "opencv"

    def __init__(self) -> None:
        import cv2

        self.cv2 = cv2

    def read_rgb(self, path: Path) -> NDArray[np.uint8]:
        raw = self.cv2.imread(str(path), self.cv2.IMREAD_COLOR)
        if raw is None:
            raise RuntimeError(f"OpenCV could not read frame: {path}")
        return self.cv2.cvtColor(raw, self.cv2.COLOR_BGR2RGB)

    def resize(self, image: NDArray[np.uint8], width: int, height: int) -> NDArray[np.uint8]:
        return self.cv2.resize(image, (width, height), interpolation=self.cv2.INTER_AREA)

    def box(
        self,
        image: NDArray[np.uint8],
        bounds: tuple[int, int, int, int],
        color: tuple[int, int, int],
        label: str,
    ) -> None:
        left, top, right, bottom = bounds
        self.cv2.rectangle(image, (left, top), (right, bottom), color, 2)
        self.text(image, label, (left, max(16, top - 5)), color, scale=0.48)

    def text(
        self,
        image: NDArray[np.uint8],
        text: str,
        origin: tuple[int, int],
        color: tuple[int, int, int],
        *,
        scale: float = 0.55,
    ) -> None:
        font = self.cv2.FONT_HERSHEY_SIMPLEX
        size, baseline = self.cv2.getTextSize(text, font, scale, 1)
        x, y = origin
        x = max(0, min(image.shape[1] - 1, x))
        y = max(size[1] + 2, min(image.shape[0] - 1, y))
        self.cv2.rectangle(
            image,
            (x, y - size[1] - 3),
            (min(image.shape[1] - 1, x + size[0] + 4), min(image.shape[0] - 1, y + baseline + 2)),
            (0, 0, 0),
            -1,
        )
        self.cv2.putText(image, text, (x + 2, y), font, scale, color, 1, self.cv2.LINE_AA)

    def banner(self, image: NDArray[np.uint8], text: str) -> None:
        height = min(44, image.shape[0])
        image[:height, :, :] = np.array((180, 0, 0), dtype=np.uint8)
        self.cv2.putText(
            image,
            text,
            (12, min(30, height - 7)),
            self.cv2.FONT_HERSHEY_SIMPLEX,
            0.75,
            (255, 255, 255),
            2,
            self.cv2.LINE_AA,
        )


class _PillowOperations:
    name = "pillow"

    def __init__(self) -> None:
        from PIL import Image, ImageDraw, ImageFont

        self.Image = Image
        self.ImageDraw = ImageDraw
        self.font = ImageFont.load_default()

    def read_rgb(self, path: Path) -> NDArray[np.uint8]:
        with self.Image.open(path) as source:
            return np.asarray(source.convert("RGB"), dtype=np.uint8).copy()

    def resize(self, image: NDArray[np.uint8], width: int, height: int) -> NDArray[np.uint8]:
        result = self.Image.fromarray(image).resize((width, height), self.Image.Resampling.LANCZOS)
        return np.asarray(result, dtype=np.uint8).copy()

    def _mutate(self, image: NDArray[np.uint8], callback: Callable[[Any], None]) -> None:
        converted = self.Image.fromarray(image)
        callback(self.ImageDraw.Draw(converted))
        image[:] = np.asarray(converted, dtype=np.uint8)

    def box(
        self,
        image: NDArray[np.uint8],
        bounds: tuple[int, int, int, int],
        color: tuple[int, int, int],
        label: str,
    ) -> None:
        left, top, right, bottom = bounds
        self._mutate(image, lambda draw: draw.rectangle(bounds, outline=color, width=2))
        self.text(image, label, (left, max(12, top - 12)), color, scale=0.5)

    def text(
        self,
        image: NDArray[np.uint8],
        text: str,
        origin: tuple[int, int],
        color: tuple[int, int, int],
        *,
        scale: float = 0.55,
    ) -> None:
        del scale
        x, y = origin

        def draw_text(draw: Any) -> None:
            bounds = draw.textbbox((x, y), text, font=self.font)
            draw.rectangle((bounds[0] - 2, bounds[1] - 2, bounds[2] + 2, bounds[3] + 2), fill=(0, 0, 0))
            draw.text((x, y), text, fill=color, font=self.font)

        self._mutate(image, draw_text)

    def banner(self, image: NDArray[np.uint8], text: str) -> None:
        height = min(40, image.shape[0])

        def draw_banner(draw: Any) -> None:
            draw.rectangle((0, 0, image.shape[1], height), fill=(180, 0, 0))
            draw.text((12, 13), text, fill=(255, 255, 255), font=self.font)

        self._mutate(image, draw_banner)


def _default_image_operations() -> ImageOperations:
    try:
        return _Cv2Operations()
    except (ImportError, ModuleNotFoundError):
        try:
            return _PillowOperations()
        except (ImportError, ModuleNotFoundError) as exc:
            raise RuntimeError(
                "Video rendering needs OpenCV (recommended), or Pillow plus ffmpeg. "
                "Install opencv-python, or install pillow and make ffmpeg available on PATH."
            ) from exc


class VideoEncoder(Protocol):
    name: str

    def write(self, frame_rgb: NDArray[np.uint8]) -> None: ...

    def close(self) -> None: ...


class _Cv2Encoder:
    name = "opencv-mp4v"

    def __init__(self, path: Path, fps: float, size: tuple[int, int]) -> None:
        import cv2

        self.cv2 = cv2
        self.path = path
        self.writer = cv2.VideoWriter(
            str(path), cv2.VideoWriter_fourcc(*"mp4v"), fps, size
        )
        if not self.writer.isOpened():
            self.writer.release()
            raise RuntimeError(f"OpenCV could not open an MP4 encoder for {path}")

    def write(self, frame_rgb: NDArray[np.uint8]) -> None:
        self.writer.write(self.cv2.cvtColor(frame_rgb, self.cv2.COLOR_RGB2BGR))

    def close(self) -> None:
        self.writer.release()
        if not self.path.is_file() or self.path.stat().st_size == 0:
            raise RuntimeError("OpenCV closed the video writer without producing an MP4")


class _FfmpegEncoder:
    name = "ffmpeg-libx264"

    def __init__(self, path: Path, fps: float, size: tuple[int, int]) -> None:
        executable = shutil.which("ffmpeg")
        if executable is None:
            raise RuntimeError("ffmpeg was not found on PATH")
        width, height = size
        command = [
            executable,
            "-hide_banner",
            "-loglevel",
            "error",
            "-y",
            "-f",
            "rawvideo",
            "-pix_fmt",
            "rgb24",
            "-s",
            f"{width}x{height}",
            "-r",
            str(fps),
            "-i",
            "-",
            "-an",
            "-c:v",
            "libx264",
            "-pix_fmt",
            "yuv420p",
            "-movflags",
            "+faststart",
            str(path),
        ]
        self.path = path
        self.process = subprocess.Popen(
            command, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE
        )

    def write(self, frame_rgb: NDArray[np.uint8]) -> None:
        if self.process.stdin is None:
            raise RuntimeError("ffmpeg input pipe is unavailable")
        self.process.stdin.write(np.ascontiguousarray(frame_rgb).tobytes())

    def close(self) -> None:
        if self.process.stdin is not None:
            self.process.stdin.close()
        stderr = self.process.stderr.read().decode("utf-8", errors="replace") if self.process.stderr else ""
        return_code = self.process.wait()
        if return_code != 0 or not self.path.is_file() or self.path.stat().st_size == 0:
            raise RuntimeError(f"ffmpeg failed to encode the MP4: {stderr.strip()}")


EncoderFactory = Callable[[Path, float, tuple[int, int]], VideoEncoder]


def _encoder_factory(preference: str) -> EncoderFactory:
    normalized = preference.casefold()
    if normalized == "cv2":
        return lambda path, fps, size: _Cv2Encoder(path, fps, size)
    if normalized == "ffmpeg":
        return lambda path, fps, size: _FfmpegEncoder(path, fps, size)
    if normalized != "auto":
        raise ValueError("encoder preference must be auto, cv2, or ffmpeg")

    def automatic(path: Path, fps: float, size: tuple[int, int]) -> VideoEncoder:
        try:
            return _Cv2Encoder(path, fps, size)
        except (ImportError, ModuleNotFoundError, RuntimeError) as cv_error:
            try:
                return _FfmpegEncoder(path, fps, size)
            except RuntimeError as ffmpeg_error:
                raise RuntimeError(
                    "No MP4 encoder is available. Install opencv-python, or install ffmpeg "
                    f"and put it on PATH. OpenCV: {cv_error}; ffmpeg: {ffmpeg_error}"
                ) from ffmpeg_error

    return automatic


@dataclass(frozen=True, slots=True)
class RenderConfig:
    frames_root: Path
    output_path: Path
    node_id: str | None = None
    alerts_path: Path | None = None
    manifest_path: Path | None = None
    max_frames: int = 180
    cadence_seconds: float = 0.0
    fps: float = 6.0
    session_gap_seconds: float = 30.0
    alert_window_seconds: float = 5.0
    event_model_path: Path | None = None
    event_device: str = "cpu"
    event_confidence: float = 0.25
    encoder: str = "auto"
    show_simulation_truth: bool = False


def build_video_manifest(
    *,
    config: RenderConfig,
    records: Sequence[FrameRecord],
    encoder_name: str,
    image_backend: str,
    output_path: Path,
    alert_frame_count: int,
    candidate_count: int,
    ground_truth_box_count: int,
    event_enabled: bool,
    created_utc: datetime | None = None,
) -> dict[str, Any]:
    """Build the machine-readable provenance manifest for a rendered video."""

    if not records:
        raise ValueError("records must not be empty")
    created = created_utc or _utc_now()
    duration = len(records) / config.fps
    return {
        "schemaVersion": "singapore_detection_video_manifest_v1",
        "createdUtc": created.astimezone(timezone.utc).isoformat().replace("+00:00", "Z"),
        "videoPath": str(output_path.resolve()),
        "nodeId": records[0].node_id,
        "frameCount": len(records),
        "fps": config.fps,
        "durationSeconds": duration,
        "cadenceSeconds": config.cadence_seconds,
        "sessionStartUtc": records[0].timestamp.isoformat().replace("+00:00", "Z"),
        "sessionEndUtc": records[-1].timestamp.isoformat().replace("+00:00", "Z"),
        "sources": {
            "framesRoot": str(config.frames_root.expanduser().resolve()),
            "alertsJsonl": str(config.alerts_path.expanduser().resolve()) if config.alerts_path else None,
            "eventCheckpoint": str(config.event_model_path.expanduser().resolve()) if config.event_model_path else None,
        },
        "rendering": {"encoder": encoder_name, "imageBackend": image_backend},
        "counts": {
            "alertFrames": alert_frame_count,
            "eventYoloCandidates": candidate_count,
            "simulationGroundTruthBoxes": ground_truth_box_count,
        },
        "event": {
            "enabled": event_enabled,
            "sourceEncoding": PROXY_EVENT_ENCODING if event_enabled else None,
            "candidateLabel": "EVENT-YOLO CANDIDATE / RGB-DERIVED PROXY" if event_enabled else None,
            "disclaimer": _EVENT_DISCLAIMER if event_enabled else None,
            "inputProvenance": (
                {
                    "origin": "RGB frame differencing",
                    "physicalNeuromorphicSensorData": False,
                    "trainingPreprocessingEquivalent": False,
                    "candidateIsConfirmedDetection": False,
                }
                if event_enabled
                else None
            ),
        },
        "groundTruth": {
            "enabled": config.show_simulation_truth,
            "source": "Unreal simulation metadata",
            "overlayLabel": "SIM GROUND TRUTH" if config.show_simulation_truth else None,
            "disclaimer": _GROUND_TRUTH_DISCLAIMER,
        },
        "groundTruthDisclaimer": _GROUND_TRUTH_DISCLAIMER,
        "alertLabel": _DETECTION_ALERT_LABEL,
        "scenarioRedTeamAlertLabel": (
            f"{_DETECTION_ALERT_LABEL} | {_SCENARIO_RED_TEAM_QUALIFIER}"
        ),
        "hostilityInferredFromSensorEvidence": False,
        "detectionOnly": True,
    }


def _event_candidates(
    observations: Sequence[Any], width: int, height: int
) -> list[tuple[tuple[int, int, int, int], str]]:
    candidates: list[tuple[tuple[int, int, int, int], str]] = []
    for observation in observations:
        measurements = getattr(observation, "measurements", {})
        if not isinstance(measurements, Mapping):
            continue
        box = measurements.get("bbox_xyxy_normalized")
        values = _box_values(box)
        if values is None:
            continue
        x1, y1, x2, y2 = values
        bounds = (
            max(0, min(width - 1, int(round(x1 * width)))),
            max(0, min(height - 1, int(round(y1 * height)))),
            max(0, min(width - 1, int(round(x2 * width)))),
            max(0, min(height - 1, int(round(y2 * height)))),
        )
        if bounds[2] <= bounds[0] or bounds[3] <= bounds[1]:
            continue
        confidence = float(getattr(observation, "detection_probability", 0.0))
        label = f"EVENT-YOLO CANDIDATE / RGB-DERIVED PROXY | {confidence:.2f}"
        candidates.append((bounds, label))
    return candidates


def render_detection_video(
    config: RenderConfig,
    *,
    image_operations: ImageOperations | None = None,
    encoder_factory: EncoderFactory | None = None,
    event_adapter: EventYOLOAdapter | None = None,
) -> dict[str, Any]:
    """Render a labelled MP4 and atomically publish it with a JSON manifest."""

    if not math.isfinite(config.fps) or config.fps <= 0.0:
        raise ValueError("fps must be finite and > 0")
    records = select_frame_records(
        config.frames_root,
        node_id=config.node_id,
        max_frames=config.max_frames,
        cadence_seconds=config.cadence_seconds,
        session_gap_seconds=config.session_gap_seconds,
    )
    alerts = load_alert_records(config.alerts_path)
    operations = image_operations or _default_image_operations()
    event_enabled = config.event_model_path is not None or event_adapter is not None
    if event_enabled and event_adapter is None:
        assert config.event_model_path is not None
        event_adapter = EventYOLOAdapter(
            config.event_model_path,
            confidence_threshold=config.event_confidence,
            device=config.event_device,
        )
    projector = ProxyEventProjector() if event_enabled else None

    output = config.output_path.expanduser().resolve()
    if output.suffix.casefold() != ".mp4":
        raise ValueError("output_path must have an .mp4 suffix")
    manifest_path = (
        config.manifest_path.expanduser().resolve()
        if config.manifest_path
        else output.with_suffix(".manifest.json")
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    token = uuid4().hex
    temporary_video = output.with_name(f".{output.stem}.{token}.tmp.mp4")
    temporary_manifest = manifest_path.with_name(f".{manifest_path.name}.{token}.tmp")

    first = operations.read_rgb(records[0].rgb_path)
    height, width = first.shape[:2]
    render_width = width * (2 if event_enabled else 1)
    render_height = height
    # yuv420p and several native MP4 encoders require even dimensions.
    render_width -= render_width % 2
    render_height -= render_height % 2
    factory = encoder_factory or _encoder_factory(config.encoder)
    encoder = factory(temporary_video, config.fps, (render_width, render_height))
    alert_frame_count = 0
    candidate_count = 0
    ground_truth_box_count = 0
    close_attempted = False

    try:
        for index, record in enumerate(records):
            rgb = first.copy() if index == 0 else operations.read_rgb(record.rgb_path)
            if rgb.shape[:2] != (height, width):
                rgb = operations.resize(rgb, width, height)
            # Keep detector input free of render-only boxes, banners, and text;
            # otherwise the overlay itself would create artificial proxy events.
            event_input = rgb.copy() if event_enabled else None
            truth = (
                extract_ground_truth_overlays(record.metadata, width, height)
                if config.show_simulation_truth
                else []
            )
            ground_truth_box_count += len(truth)
            for overlay in truth:
                color = (255, 40, 40) if overlay.hostile else (40, 230, 70)
                operations.box(rgb, overlay.box_xyxy, color, overlay.label)

            matched_alert = recent_matching_alert(
                record, alerts, window_seconds=config.alert_window_seconds
            )
            status = "RF/FUSION STATUS: SCANNING"
            if matched_alert is not None:
                alert_distance = _format_distance(
                    matched_alert.nearest_node_distance_meters
                )
                alert_range = (
                    f" | NEAREST RF NODE {alert_distance}" if alert_distance else ""
                )
                alert_label = _DETECTION_ALERT_LABEL
                scenario_qualifier = ""
                if matched_alert.scenario_red_team:
                    alert_label += f" | {_SCENARIO_RED_TEAM_QUALIFIER}"
                    scenario_qualifier = f" | {_SCENARIO_RED_TEAM_QUALIFIER}"
                operations.banner(rgb, f"{alert_label}{alert_range}")
                status = (
                    f"RF/FUSION STATUS: DETECTION ALERT ACTIVE"
                    f"{scenario_qualifier}{alert_range}"
                )
                alert_frame_count += 1
            operations.text(
                rgb,
                f"NODE {record.node_id} | {record.timestamp.isoformat().replace('+00:00', 'Z')}",
                (8, height - 28),
                (255, 255, 255),
                scale=0.48,
            )
            operations.text(rgb, status, (8, height - 8), (255, 255, 255), scale=0.48)

            if event_enabled:
                assert projector is not None and event_adapter is not None
                assert event_input is not None
                proxy = projector.push(
                    event_input, timestamp_seconds=record.timestamp.timestamp()
                )
                if proxy is None:
                    proxy = np.zeros_like(rgb)
                    operations.text(
                        proxy,
                        "EVENT PROXY WARM-UP | FIRST FRAME BLANK",
                        (10, 42),
                        (255, 220, 0),
                        scale=0.55,
                    )
                else:
                    observations = event_adapter.infer(
                        proxy,
                        node_id=record.node_id,
                        timestamp=record.timestamp,
                        frame_id=str(record.frame_index if record.frame_index is not None else index),
                        source_encoding=PROXY_EVENT_ENCODING,
                    )
                    candidates = _event_candidates(observations, width, height)
                    candidate_count += len(candidates)
                    for bounds, label in candidates:
                        operations.box(proxy, bounds, (255, 220, 0), label)
                operations.text(
                    proxy,
                    "EVENT-YOLO OUTPUTS: PROXY CANDIDATES ONLY | NOT CONFIRMED DETECTIONS",
                    (8, height - 8),
                    (255, 220, 0),
                    scale=0.44,
                )
                operations.text(
                    proxy,
                    (
                        "EVENT INPUT: RGB-DERIVED PROXY | NOT A PHYSICAL EVENT CAMERA "
                        "| NOT TRAINING-EQUIVALENT"
                    ),
                    (8, 18),
                    (255, 220, 0),
                    scale=0.44,
                )
                rendered = np.concatenate((rgb, proxy), axis=1)
            else:
                rendered = rgb
            rendered = np.ascontiguousarray(rendered[:render_height, :render_width, :3], dtype=np.uint8)
            encoder.write(rendered)
        close_attempted = True
        encoder.close()
        manifest = build_video_manifest(
            config=config,
            records=records,
            encoder_name=encoder.name,
            image_backend=operations.name,
            output_path=output,
            alert_frame_count=alert_frame_count,
            candidate_count=candidate_count,
            ground_truth_box_count=ground_truth_box_count,
            event_enabled=event_enabled,
        )
        temporary_manifest.write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        os.replace(temporary_video, output)
        os.replace(temporary_manifest, manifest_path)
        return manifest
    except BaseException:
        if not close_attempted:
            try:
                encoder.close()
            except Exception:
                pass
        for temporary in (temporary_video, temporary_manifest):
            try:
                temporary.unlink(missing_ok=True)
            except OSError:
                pass
        raise


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Render a simulation-labelled drone-detection MP4 from Unreal frames."
    )
    parser.add_argument("--frames-root", type=Path, required=True)
    parser.add_argument("--output", dest="output_path", type=Path, required=True)
    parser.add_argument("--node", dest="node_id")
    parser.add_argument("--alerts-jsonl", dest="alerts_path", type=Path)
    parser.add_argument("--manifest", dest="manifest_path", type=Path)
    parser.add_argument("--max-frames", type=int, default=180)
    parser.add_argument("--cadence-seconds", type=float, default=0.0)
    parser.add_argument("--fps", type=float, default=6.0)
    parser.add_argument("--session-gap-seconds", type=float, default=30.0)
    parser.add_argument("--alert-window-seconds", type=float, default=5.0)
    parser.add_argument("--event-model", dest="event_model_path", type=Path)
    parser.add_argument("--event-device", default="cpu")
    parser.add_argument("--event-confidence", type=float, default=0.25)
    parser.add_argument("--encoder", choices=("auto", "cv2", "ffmpeg"), default="auto")
    parser.add_argument(
        "--show-sim-truth",
        dest="show_simulation_truth",
        action="store_true",
        help=(
            "Draw Unreal metadata boxes for debugging. Disabled by default so "
            "simulation truth cannot be mistaken for detector output."
        ),
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = _parser().parse_args(argv)
    config = RenderConfig(**vars(arguments))
    try:
        manifest = render_detection_video(config)
    except Exception as exc:
        print(f"render_detection_video: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
