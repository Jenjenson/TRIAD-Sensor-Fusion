"""Local operator dashboard for TRIAD placement replay and live viewing.

Replay mode animates a self-validating ``triad.dashboard_replay.v1`` artifact.
Optional live mode polls only the bounded, GET-only loopback bridge and keeps
that current runtime layer visually separate from the frozen recommendation.
Neither mode represents calibrated field performance or authorizes engagement.
"""

from __future__ import annotations

import argparse
from bisect import bisect_left, bisect_right
from dataclasses import dataclass
from datetime import datetime, timezone
import ipaddress
import json
import math
from pathlib import Path
import queue
import threading
import time
from typing import Any, Callable, Mapping, Sequence
from urllib.error import HTTPError, URLError
from urllib.parse import urlsplit
from urllib.request import HTTPRedirectHandler, ProxyHandler, Request, build_opener

from .contracts import PlacementRequest
from .dashboard_replay import (
    build_dashboard_replay,
    load_recommendation,
    validate_dashboard_replay,
)


_EARTH_RADIUS_METERS = 6_378_137.0
_DEFAULT_WINDOW_SIZE = "1440x860"
_MINIMUM_WINDOW_SIZE = (1040, 680)
_LIVE_SNAPSHOT_SCHEMA = "1.0"
_DEFAULT_LIVE_URL = "http://127.0.0.1:8765/api/snapshot"
_DEFAULT_LIVE_POLL_SECONDS = 0.25
_DEFAULT_LIVE_TIMEOUT_SECONDS = 0.75
_MIN_LIVE_POLL_SECONDS = 0.10
_MAX_LIVE_POLL_SECONDS = 10.0
_MIN_LIVE_TIMEOUT_SECONDS = 0.10
_MAX_LIVE_TIMEOUT_SECONDS = 5.0
_MAX_LIVE_SNAPSHOT_BYTES = 2 * 1024 * 1024
_MAX_LIVE_NODES = 256
_MAX_LIVE_TRACKS = 128
_PLACEMENT_HORIZONTAL_TOLERANCE_METERS = 1.0
_PLACEMENT_HEIGHT_TOLERANCE_METERS = 1.0
_MAX_LIVE_ENVELOPE_AGE_SECONDS = 10.0
_MAX_LIVE_ENVELOPE_FUTURE_SKEW_SECONDS = 2.0


def _finite(name: str, value: object) -> float:
    if isinstance(value, bool):
        raise TypeError(f"{name} must be numeric")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"{name} must be finite")
    return result


def _repository_root() -> Path:
    return Path(__file__).resolve().parents[4]


def _mode_banner_text(replay: Mapping[str, Any], *, live: bool) -> str:
    scope = replay.get("stage0Scope")
    demo_only = (
        isinstance(scope, Mapping)
        and scope.get("executionClass") == "DEMO_STUDY_ONLY"
    )
    mode = "LIVE-LOCAL VIEW · DETECTION-ONLY" if live else "STUDY-MODE REPLAY · NOT LIVE"
    if demo_only:
        mode += " · SYNTHETIC DEMO_STUDY_ONLY"
    return mode + " · NO DEPLOYMENT · NOT FIELD-CALIBRATED"


def _solver_proof_text(recommendation: Mapping[str, Any]) -> str | None:
    proof = recommendation.get("solverProof")
    if not isinstance(proof, Mapping):
        return None
    node_status = "REACHED" if proof["nodeLimitReached"] is True else "NOT REACHED"
    return (
        f"Termination: {proof['terminationReason']}\n"
        f"Incumbent: {float(proof['incumbentObjectiveCostUnits']):.3f} cost units · "
        f"conservative lower bound: "
        f"{float(proof['objectiveCostLowerBoundUnits']):.3f}\n"
        f"Absolute gap: {float(proof['absoluteCostOptimalityGapUnits']):.3f} · "
        f"relative gap: {float(proof['relativeCostOptimalityGap']) * 100.0:.3f}%\n"
        f"Node limit: {node_status} · {int(proof['exploredNodeCount']):,} explored / "
        f"{int(proof['nodeLimit']):,} limit"
    )


def location_to_enu(
    location: Mapping[str, Any],
    origin: Mapping[str, Any],
) -> tuple[float, float, float]:
    """Project a small WGS84 offset into a dashboard-local ENU frame."""

    latitude = _finite("location.latitudeDegrees", location["latitudeDegrees"])
    longitude = _finite("location.longitudeDegrees", location["longitudeDegrees"])
    height = _finite("location.heightMeters", location.get("heightMeters", 0.0))
    origin_latitude = _finite(
        "origin.latitudeDegrees", origin["latitudeDegrees"]
    )
    origin_longitude = _finite(
        "origin.longitudeDegrees", origin["longitudeDegrees"]
    )
    origin_height = _finite("origin.heightMeters", origin.get("heightMeters", 0.0))
    mean_latitude = math.radians((latitude + origin_latitude) * 0.5)
    east = math.radians(longitude - origin_longitude) * (
        _EARTH_RADIUS_METERS * max(abs(math.cos(mean_latitude)), 0.000001)
    )
    north = math.radians(latitude - origin_latitude) * _EARTH_RADIUS_METERS
    return east, north, height - origin_height


@dataclass(frozen=True, slots=True)
class MapProjector:
    """Responsive ENU-to-canvas projection for the circular study AOI."""

    width: float
    height: float
    radius_meters: float
    padding_pixels: float = 42.0

    def __post_init__(self) -> None:
        if self.width <= 0.0 or self.height <= 0.0:
            raise ValueError("projector dimensions must be positive")
        if self.radius_meters <= 0.0:
            raise ValueError("radius_meters must be positive")
        if self.padding_pixels < 0.0:
            raise ValueError("padding_pixels must be non-negative")

    @property
    def scale(self) -> float:
        available = max(
            1.0,
            min(self.width, self.height) - 2.0 * self.padding_pixels,
        )
        return available / (2.0 * self.radius_meters)

    def project(self, east_meters: float, north_meters: float) -> tuple[float, float]:
        return (
            self.width * 0.5 + east_meters * self.scale,
            self.height * 0.5 - north_meters * self.scale,
        )


class LiveSnapshotError(RuntimeError):
    """Base error for the bounded local live-view transport."""


class LiveSnapshotMalformedError(LiveSnapshotError):
    """The loopback endpoint returned data that is unsafe to display as live."""


class LiveSnapshotUnavailableError(LiveSnapshotError):
    """The loopback endpoint could not be reached or returned an HTTP failure."""


class _RejectRedirects(HTTPRedirectHandler):
    """Keep a loopback request from being redirected to a different authority."""

    def redirect_request(
        self,
        req: Request,
        fp: Any,
        code: int,
        msg: str,
        headers: Any,
        newurl: str,
    ) -> None:
        return None


def validate_live_snapshot_url(value: str) -> str:
    """Return a normalized, exact loopback snapshot URL or fail closed."""

    if not isinstance(value, str) or not value.strip():
        raise ValueError("--live-url must be a non-empty loopback URL")
    candidate = value.strip()
    parsed = urlsplit(candidate)
    if parsed.scheme.lower() != "http":
        raise ValueError("--live-url must use http on loopback")
    if parsed.username is not None or parsed.password is not None:
        raise ValueError("--live-url must not contain credentials")
    host = parsed.hostname
    if host is None:
        raise ValueError("--live-url must include a loopback host")
    try:
        port = parsed.port
    except ValueError as exc:
        raise ValueError("--live-url contains an invalid port") from exc
    if port is not None and not 1 <= port <= 65_535:
        raise ValueError("--live-url port must be in [1, 65535]")
    if host.lower() != "localhost":
        try:
            address = ipaddress.ip_address(host)
        except ValueError as exc:
            raise ValueError("--live-url host must be a loopback IP or localhost") from exc
        if not address.is_loopback:
            raise ValueError("--live-url host must resolve explicitly to loopback")
    if parsed.path.rstrip("/") != "/api/snapshot":
        raise ValueError("--live-url path must be exactly /api/snapshot")
    if parsed.query or parsed.fragment:
        raise ValueError("--live-url must not contain a query or fragment")
    return candidate


def _decode_json_without_duplicate_keys(raw: bytes) -> Mapping[str, Any]:
    def reject_duplicates(pairs: list[tuple[str, object]]) -> dict[str, object]:
        result: dict[str, object] = {}
        for key, item in pairs:
            if key in result:
                raise LiveSnapshotMalformedError(
                    f"live snapshot contains duplicate key {key!r}"
                )
            result[key] = item
        return result

    try:
        decoded = raw.decode("utf-8-sig")
        value = json.loads(decoded, object_pairs_hook=reject_duplicates)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise LiveSnapshotMalformedError(
            "live snapshot is not valid UTF-8 JSON"
        ) from exc
    if not isinstance(value, Mapping):
        raise LiveSnapshotMalformedError("live snapshot root must be an object")
    return value


def _parse_live_generated_at(value: object) -> datetime:
    if not isinstance(value, str) or not value.strip():
        raise LiveSnapshotMalformedError("live snapshot generatedAtUtc is missing")
    candidate = value.strip()
    try:
        parsed = datetime.fromisoformat(candidate.replace("Z", "+00:00"))
    except ValueError as exc:
        raise LiveSnapshotMalformedError(
            "live snapshot generatedAtUtc is not a valid ISO-8601 timestamp"
        ) from exc
    if parsed.tzinfo is None:
        raise LiveSnapshotMalformedError(
            "live snapshot generatedAtUtc must include an explicit UTC offset"
        )
    return parsed.astimezone(timezone.utc)


def _bounded_live_number(
    name: str,
    value: object,
    *,
    minimum: float,
    maximum: float,
) -> float:
    try:
        result = _finite(name, value)
    except (TypeError, ValueError, KeyError) as exc:
        raise LiveSnapshotMalformedError(f"{name} must be finite") from exc
    if not minimum <= result <= maximum:
        raise LiveSnapshotMalformedError(
            f"{name} must be in [{minimum}, {maximum}]"
        )
    return result


def validate_live_snapshot(value: Mapping[str, Any]) -> dict[str, Any]:
    """Validate the bounded C3 bridge projection used by the graphical client."""

    if not isinstance(value, Mapping):
        raise LiveSnapshotMalformedError("live snapshot root must be an object")
    if value.get("schemaVersion") != _LIVE_SNAPSHOT_SCHEMA:
        raise LiveSnapshotMalformedError("live snapshot schemaVersion is unsupported")
    if value.get("detectionOnly") is not True or value.get("actionsTaken") != "none":
        raise LiveSnapshotMalformedError(
            "live snapshot must be detection-only and report actionsTaken=none"
        )
    mode = value.get("mode")
    if mode not in {"live-local", "unavailable"}:
        raise LiveSnapshotMalformedError("live snapshot mode is unsupported")
    _parse_live_generated_at(value.get("generatedAtUtc"))
    freshness = value.get("sourceFreshness")
    if not isinstance(freshness, Mapping):
        raise LiveSnapshotMalformedError("live snapshot sourceFreshness is missing")
    freshness_state = freshness.get("state")
    if freshness_state not in {"fresh", "delayed", "stale", "unavailable"}:
        raise LiveSnapshotMalformedError("live snapshot freshness state is unsupported")
    if freshness_state == "fresh" and mode != "live-local":
        raise LiveSnapshotMalformedError(
            "an unavailable live snapshot cannot claim fresh telemetry"
        )
    fresh_threshold = _bounded_live_number(
        "sourceFreshness.freshThresholdSeconds",
        freshness.get("freshThresholdSeconds"),
        minimum=0.01,
        maximum=60.0,
    )
    stale_threshold = _bounded_live_number(
        "sourceFreshness.staleThresholdSeconds",
        freshness.get("staleThresholdSeconds"),
        minimum=fresh_threshold,
        maximum=3_600.0,
    )
    age_value = freshness.get("newestTelemetryAgeSeconds")
    age = None
    if age_value is not None:
        age = _bounded_live_number(
            "sourceFreshness.newestTelemetryAgeSeconds",
            age_value,
            minimum=0.0,
            maximum=1_000_000_000.0,
        )
    if freshness_state == "fresh" and (age is None or age > fresh_threshold):
        raise LiveSnapshotMalformedError(
            "fresh live snapshot age exceeds its declared fresh threshold"
        )
    if freshness_state == "delayed" and (
        age is None or age <= fresh_threshold or age > stale_threshold
    ):
        raise LiveSnapshotMalformedError(
            "delayed live snapshot age is inconsistent with its thresholds"
        )
    if freshness_state == "stale" and (age is None or age <= stale_threshold):
        raise LiveSnapshotMalformedError(
            "stale live snapshot age does not exceed its stale threshold"
        )
    if freshness_state == "unavailable" and age is not None:
        raise LiveSnapshotMalformedError(
            "unavailable live snapshot must not claim a telemetry age"
        )
    sources = value.get("sources")
    if not isinstance(sources, Mapping):
        raise LiveSnapshotMalformedError("live snapshot sources are missing")

    nodes = value.get("nodes")
    tracks = value.get("tracks")
    if not isinstance(nodes, list) or len(nodes) > _MAX_LIVE_NODES:
        raise LiveSnapshotMalformedError(
            f"live snapshot nodes must be an array of at most {_MAX_LIVE_NODES} items"
        )
    if not isinstance(tracks, list) or len(tracks) > _MAX_LIVE_TRACKS:
        raise LiveSnapshotMalformedError(
            f"live snapshot tracks must be an array of at most {_MAX_LIVE_TRACKS} items"
        )

    node_ids: set[str] = set()
    for index, node in enumerate(nodes):
        if not isinstance(node, Mapping):
            raise LiveSnapshotMalformedError(f"nodes[{index}] must be an object")
        node_id = node.get("id")
        if not isinstance(node_id, str) or not node_id.strip() or node_id in node_ids:
            raise LiveSnapshotMalformedError(
                f"nodes[{index}].id must be non-empty and unique"
            )
        node_ids.add(node_id)
        _bounded_live_number(
            f"nodes[{index}].latitudeDegrees",
            node.get("latitudeDegrees"),
            minimum=-90.0,
            maximum=90.0,
        )
        _bounded_live_number(
            f"nodes[{index}].longitudeDegrees",
            node.get("longitudeDegrees"),
            minimum=-180.0,
            maximum=180.0,
        )
        if node.get("heightMeters") is not None:
            _bounded_live_number(
                f"nodes[{index}].heightMeters",
                node.get("heightMeters"),
                minimum=-1_000.0,
                maximum=100_000.0,
            )

    track_ids: set[str] = set()
    for index, track in enumerate(tracks):
        if not isinstance(track, Mapping):
            raise LiveSnapshotMalformedError(f"tracks[{index}] must be an object")
        track_id = track.get("id")
        if not isinstance(track_id, str) or not track_id.strip() or track_id in track_ids:
            raise LiveSnapshotMalformedError(
                f"tracks[{index}].id must be non-empty and unique"
            )
        track_ids.add(track_id)
        latitude = track.get("latitudeDegrees")
        longitude = track.get("longitudeDegrees")
        if (latitude is None) != (longitude is None):
            raise LiveSnapshotMalformedError(
                f"tracks[{index}] must provide both latitude and longitude or neither"
            )
        if latitude is not None:
            _bounded_live_number(
                f"tracks[{index}].latitudeDegrees",
                latitude,
                minimum=-90.0,
                maximum=90.0,
            )
            _bounded_live_number(
                f"tracks[{index}].longitudeDegrees",
                longitude,
                minimum=-180.0,
                maximum=180.0,
            )
        if track.get("altitudeMeters") is not None:
            _bounded_live_number(
                f"tracks[{index}].altitudeMeters",
                track.get("altitudeMeters"),
                minimum=-1_000.0,
                maximum=100_000.0,
            )
        track_freshness = track.get("freshness")
        if track_freshness not in {"fresh", "delayed", "stale", "unavailable"}:
            raise LiveSnapshotMalformedError(
                f"tracks[{index}].freshness is unsupported"
            )
        track_age = None
        if track.get("ageSeconds") is not None:
            track_age = _bounded_live_number(
                f"tracks[{index}].ageSeconds",
                track.get("ageSeconds"),
                minimum=0.0,
                maximum=1_000_000_000.0,
            )
        if track_freshness == "fresh" and (
            track_age is None or track_age > fresh_threshold
        ):
            raise LiveSnapshotMalformedError(
                f"tracks[{index}] claims fresh evidence outside the fresh threshold"
            )
        if track_freshness == "delayed" and (
            track_age is None
            or track_age <= fresh_threshold
            or track_age > stale_threshold
        ):
            raise LiveSnapshotMalformedError(
                f"tracks[{index}] delayed age is inconsistent with source thresholds"
            )
        if track_freshness == "stale" and (
            track_age is None or track_age <= stale_threshold
        ):
            raise LiveSnapshotMalformedError(
                f"tracks[{index}] stale age is inconsistent with source thresholds"
            )
        if track_freshness == "unavailable" and track_age is not None:
            raise LiveSnapshotMalformedError(
                f"tracks[{index}] unavailable evidence must not claim an age"
            )
        if track.get("distanceMeters") is not None:
            _bounded_live_number(
                f"tracks[{index}].distanceMeters",
                track.get("distanceMeters"),
                minimum=0.0,
                maximum=100_000_000.0,
            )
    return dict(value)


class LiveSnapshotClient:
    """GET-only, no-redirect, size- and time-bounded loopback client."""

    def __init__(
        self,
        url: str,
        *,
        timeout_seconds: float = _DEFAULT_LIVE_TIMEOUT_SECONDS,
        max_bytes: int = _MAX_LIVE_SNAPSHOT_BYTES,
    ) -> None:
        self.url = validate_live_snapshot_url(url)
        timeout = _finite("timeout_seconds", timeout_seconds)
        if not _MIN_LIVE_TIMEOUT_SECONDS <= timeout <= _MAX_LIVE_TIMEOUT_SECONDS:
            raise ValueError(
                "live timeout must be between "
                f"{_MIN_LIVE_TIMEOUT_SECONDS:g} and {_MAX_LIVE_TIMEOUT_SECONDS:g} seconds"
            )
        if isinstance(max_bytes, bool) or not isinstance(max_bytes, int):
            raise TypeError("max_bytes must be an integer")
        if not 1_024 <= max_bytes <= _MAX_LIVE_SNAPSHOT_BYTES:
            raise ValueError(
                f"max_bytes must be in [1024, {_MAX_LIVE_SNAPSHOT_BYTES}]"
            )
        self.timeout_seconds = timeout
        self.max_bytes = max_bytes
        # Never inherit HTTP(S)_PROXY, WinHTTP, or system proxy settings for a
        # loopback-only telemetry channel.
        self._opener = build_opener(ProxyHandler({}), _RejectRedirects())

    def fetch(self) -> dict[str, Any]:
        request = Request(
            self.url,
            headers={"Accept": "application/json", "Cache-Control": "no-store"},
            method="GET",
        )
        try:
            with self._opener.open(request, timeout=self.timeout_seconds) as response:
                status = int(response.getcode())
                if status != 200:
                    raise LiveSnapshotUnavailableError(
                        f"live snapshot endpoint returned HTTP {status}"
                    )
                content_length = response.headers.get("Content-Length")
                if content_length is not None:
                    try:
                        declared_length = int(content_length)
                    except ValueError as exc:
                        raise LiveSnapshotMalformedError(
                            "live snapshot Content-Length is invalid"
                        ) from exc
                    if declared_length < 0 or declared_length > self.max_bytes:
                        raise LiveSnapshotMalformedError(
                            "live snapshot exceeds the bounded byte limit"
                        )
                raw = response.read(self.max_bytes + 1)
        except LiveSnapshotError:
            raise
        except (HTTPError, URLError, OSError, TimeoutError) as exc:
            raise LiveSnapshotUnavailableError(
                f"live snapshot endpoint is unavailable: {exc}"
            ) from exc
        if len(raw) > self.max_bytes:
            raise LiveSnapshotMalformedError(
                "live snapshot exceeds the bounded byte limit"
            )
        return validate_live_snapshot(_decode_json_without_duplicate_keys(raw))


@dataclass(frozen=True, slots=True)
class PlacementComparison:
    """Fail-closed relationship between reference and current runtime nodes."""

    status: str
    missing_node_ids: tuple[str, ...] = ()
    extra_node_ids: tuple[str, ...] = ()
    coordinate_mismatches: tuple[str, ...] = ()
    reason: str = ""

    @property
    def matched(self) -> bool:
        return self.status == "matched"


def compare_live_placement_to_reference(
    replay: Mapping[str, Any],
    snapshot: Mapping[str, Any],
) -> PlacementComparison:
    """Require fresh telemetry, exact IDs, and bounded 3D coordinate deltas."""

    freshness = snapshot.get("sourceFreshness")
    state = freshness.get("state") if isinstance(freshness, Mapping) else None
    if snapshot.get("mode") != "live-local" or state != "fresh":
        return PlacementComparison(
            status="not-comparable",
            reason="only a fresh live-local snapshot can match the frozen reference",
        )
    reference_items = replay.get("sensors")
    live_items = snapshot.get("nodes")
    if not isinstance(reference_items, list) or not isinstance(live_items, list):
        return PlacementComparison(
            status="not-comparable",
            reason="reference or live node roster is unavailable",
        )
    reference_by_id = {
        str(item["nodeId"]): item
        for item in reference_items
        if isinstance(item, Mapping) and isinstance(item.get("nodeId"), str)
    }
    live_by_id = {
        str(item["id"]): item
        for item in live_items
        if isinstance(item, Mapping) and isinstance(item.get("id"), str)
    }
    missing = tuple(sorted(set(reference_by_id) - set(live_by_id)))
    extra = tuple(sorted(set(live_by_id) - set(reference_by_id)))
    coordinate_mismatches: list[str] = []
    for node_id in sorted(set(reference_by_id) & set(live_by_id)):
        reference_location = reference_by_id[node_id].get("location")
        live_node = live_by_id[node_id]
        if not isinstance(reference_location, Mapping):
            coordinate_mismatches.append(f"{node_id}: reference location missing")
            continue
        if reference_location.get("heightReference") != "WGS84_ELLIPSOID":
            coordinate_mismatches.append(
                f"{node_id}: reference height datum is not WGS84_ELLIPSOID"
            )
            continue
        try:
            east, north, _up = location_to_enu(live_node, reference_location)
            horizontal_distance = math.hypot(east, north)
            reference_height = _finite(
                "reference heightMeters", reference_location.get("heightMeters")
            )
            live_height = _finite("live heightMeters", live_node.get("heightMeters"))
            height_distance = abs(live_height - reference_height)
        except (KeyError, TypeError, ValueError):
            coordinate_mismatches.append(f"{node_id}: complete 3D coordinate missing")
            continue
        if (
            horizontal_distance > _PLACEMENT_HORIZONTAL_TOLERANCE_METERS
            or height_distance > _PLACEMENT_HEIGHT_TOLERANCE_METERS
        ):
            coordinate_mismatches.append(
                f"{node_id}: horizontal={horizontal_distance:.2f}m "
                f"height={height_distance:.2f}m"
            )
    if missing or extra or coordinate_mismatches:
        return PlacementComparison(
            status="mismatched",
            missing_node_ids=missing,
            extra_node_ids=extra,
            coordinate_mismatches=tuple(coordinate_mismatches),
            reason="runtime node IDs or coordinates differ from the frozen reference",
        )
    return PlacementComparison(
        status="matched",
        reason="fresh runtime node IDs and 3D coordinates match the frozen reference",
    )


class LiveSnapshotModel:
    """Fail-closed current-state view layered over a frozen recommendation."""

    def __init__(
        self,
        reference_replay: Mapping[str, Any],
        *,
        monotonic_clock: Callable[[], float] | None = None,
        utc_clock: Callable[[], datetime] | None = None,
    ) -> None:
        validate_dashboard_replay(reference_replay)
        self.reference_replay = reference_replay
        self._monotonic_clock = monotonic_clock or time.monotonic
        self._utc_clock = utc_clock or (lambda: datetime.now(timezone.utc))
        self._received_monotonic: float | None = None
        self._reported_source_age: float | None = None
        self._fresh_threshold = 0.0
        self._stale_threshold = 0.0
        self._latest_generated_at: datetime | None = None
        self.snapshot: dict[str, Any] | None = None
        self.source_state = "unavailable"
        self.status_detail = "waiting for the first bounded loopback snapshot"
        self.comparison = PlacementComparison(
            status="not-comparable", reason="no fresh live snapshot"
        )

    def accept_snapshot(self, value: Mapping[str, Any]) -> bool:
        try:
            snapshot = validate_live_snapshot(value)
            generated_at = _parse_live_generated_at(snapshot["generatedAtUtc"])
            received_monotonic = _finite(
                "monotonic receipt time", self._monotonic_clock()
            )
            now_utc = self._utc_clock()
            if not isinstance(now_utc, datetime) or now_utc.tzinfo is None:
                raise LiveSnapshotMalformedError(
                    "local UTC clock must return a timezone-aware datetime"
                )
            envelope_age = (
                now_utc.astimezone(timezone.utc) - generated_at
            ).total_seconds()
            if envelope_age > _MAX_LIVE_ENVELOPE_AGE_SECONDS:
                raise LiveSnapshotMalformedError(
                    "live snapshot generatedAtUtc is too old for this local receipt"
                )
            if envelope_age < -_MAX_LIVE_ENVELOPE_FUTURE_SKEW_SECONDS:
                raise LiveSnapshotMalformedError(
                    "live snapshot generatedAtUtc is implausibly in the future"
                )
        except (LiveSnapshotMalformedError, TypeError, ValueError) as exc:
            self.mark_malformed(str(exc))
            return False
        if self._latest_generated_at is not None:
            if generated_at < self._latest_generated_at:
                self.mark_malformed(
                    "live snapshot generatedAtUtc retrogressed; replay rejected"
                )
                return False
            if generated_at == self._latest_generated_at:
                if self.snapshot is None or snapshot != self.snapshot:
                    self.mark_malformed(
                        "live snapshot changed without advancing generatedAtUtc"
                    )
                    return False
                # An exact repeated body is harmless, but it must not reset the
                # monotonic receipt age or reauthorize expired evidence.
                return True
        freshness = snapshot["sourceFreshness"]
        self._latest_generated_at = generated_at
        self._received_monotonic = received_monotonic
        source_age = freshness.get("newestTelemetryAgeSeconds")
        self._reported_source_age = (
            None if source_age is None else float(source_age)
        )
        self._fresh_threshold = float(freshness["freshThresholdSeconds"])
        self._stale_threshold = float(freshness["staleThresholdSeconds"])
        self.snapshot = snapshot
        self.source_state = str(freshness["state"])
        self.status_detail = str(
            freshness.get("notice")
            or f"bridge reported {self.source_state} source telemetry"
        )
        self.comparison = compare_live_placement_to_reference(
            self.reference_replay, snapshot
        )
        return True

    def mark_unavailable(self, detail: str) -> None:
        self.snapshot = None
        self.source_state = "unavailable"
        self.status_detail = detail or "live snapshot endpoint is unavailable"
        self.comparison = PlacementComparison(
            status="not-comparable", reason="live snapshot is unavailable"
        )

    def mark_malformed(self, detail: str) -> None:
        self.snapshot = None
        self.source_state = "malformed"
        self.status_detail = detail or "live snapshot failed validation"
        self.comparison = PlacementComparison(
            status="not-comparable", reason="live snapshot is malformed"
        )

    @property
    def local_elapsed_seconds(self) -> float:
        if self._received_monotonic is None:
            return 0.0
        try:
            current = _finite("monotonic current time", self._monotonic_clock())
        except (TypeError, ValueError):
            return math.inf
        return max(0.0, current - self._received_monotonic)

    @property
    def effective_source_age_seconds(self) -> float | None:
        if self.snapshot is None or self._reported_source_age is None:
            return None
        return self._reported_source_age + self.local_elapsed_seconds

    @property
    def effective_source_state(self) -> str:
        if self.snapshot is None or self.source_state in {"unavailable", "malformed"}:
            return self.source_state
        if self.snapshot.get("mode") != "live-local":
            return "unavailable"
        age = self.effective_source_age_seconds
        if age is None:
            return "unavailable"
        if age <= self._fresh_threshold:
            return "fresh"
        if age <= self._stale_threshold:
            return "delayed"
        return "stale"

    @property
    def effective_comparison(self) -> PlacementComparison:
        if self.effective_source_state != "fresh":
            return PlacementComparison(
                status="not-comparable",
                reason="locally aged telemetry is no longer fresh",
            )
        return self.comparison

    @property
    def detection_rendering_enabled(self) -> bool:
        return (
            self.snapshot is not None
            and self.effective_source_state == "fresh"
            and self.snapshot.get("mode") == "live-local"
        )

    @property
    def reference_deployment_claim_authorized(self) -> bool:
        return self.detection_rendering_enabled and self.effective_comparison.matched

    @property
    def nodes_for_display(self) -> tuple[Mapping[str, Any], ...]:
        if self.snapshot is None or self.effective_source_state not in {
            "fresh",
            "delayed",
            "stale",
        }:
            return ()
        return tuple(self.snapshot["nodes"])

    @property
    def tracks_for_display(self) -> tuple[Mapping[str, Any], ...]:
        if not self.detection_rendering_enabled or self.snapshot is None:
            return ()
        fresh_threshold = float(
            self.snapshot["sourceFreshness"]["freshThresholdSeconds"]
        )
        return tuple(
            item
            for item in self.snapshot["tracks"]
            if item.get("latitudeDegrees") is not None
            and item.get("longitudeDegrees") is not None
            and item.get("freshness") == "fresh"
            and item.get("ageSeconds") is not None
            and float(item["ageSeconds"]) + self.local_elapsed_seconds
            <= fresh_threshold
        )

    @staticmethod
    def track_is_detection(track: Mapping[str, Any]) -> bool:
        fusion = track.get("fusionV3")
        return (
            track.get("ueRfDecision") in {"ALERT", "DETECTED"}
            or (
                isinstance(fusion, Mapping)
                and (
                    fusion.get("operatorCueActive") is True
                    or fusion.get("decision") in {"CONFIRMED_TRACK", "DETECTED"}
                )
            )
        )

    def cue_links(self) -> tuple[tuple[str, str], ...]:
        if not self.detection_rendering_enabled:
            return ()
        node_ids = {
            str(item["id"])
            for item in self.nodes_for_display
            if isinstance(item.get("id"), str)
        }
        links: set[tuple[str, str]] = set()
        for track in self.tracks_for_display:
            if not self.track_is_detection(track):
                continue
            track_id = str(track["id"])
            candidates: set[str] = set()
            confirming = track.get("confirmingNodeIds")
            if isinstance(confirming, list):
                candidates.update(str(item) for item in confirming if isinstance(item, str))
            rf_links = track.get("rfLinks")
            if isinstance(rf_links, list):
                candidates.update(
                    str(item["nodeId"])
                    for item in rf_links
                    if isinstance(item, Mapping)
                    and item.get("detected") is True
                    and isinstance(item.get("nodeId"), str)
                )
            evidence = track.get("modalityEvidence")
            if isinstance(evidence, list):
                for item in evidence:
                    if not isinstance(item, Mapping) or item.get("detected") is not True:
                        continue
                    sensor_ids = item.get("sensorIds")
                    if isinstance(sensor_ids, list):
                        candidates.update(
                            sensor_id.rsplit(":", 1)[0]
                            for sensor_id in sensor_ids
                            if isinstance(sensor_id, str) and ":" in sensor_id
                        )
            visual_frame = track.get("visualFrame")
            confirmations = (
                visual_frame.get("confirmations")
                if isinstance(visual_frame, Mapping)
                else None
            )
            if isinstance(confirmations, list):
                candidates.update(
                    str(item["nodeId"])
                    for item in confirmations
                    if isinstance(item, Mapping)
                    and isinstance(item.get("nodeId"), str)
                )
            links.update(
                (node_id, track_id)
                for node_id in candidates
                if node_id in node_ids
            )
        return tuple(sorted(links))

    def provenance_text(self) -> str:
        if self.snapshot is None:
            return self.status_detail
        sources = self.snapshot.get("sources", {})
        selected = (
            sources.get("selectedRFSource", "unknown")
            if isinstance(sources, Mapping)
            else "unknown"
        )
        node_configuration = (
            sources.get("nodeConfiguration", "unknown")
            if isinstance(sources, Mapping)
            else "unknown"
        )
        generated = self.snapshot.get("generatedAtUtc", "unknown")
        age = self.effective_source_age_seconds
        age_text = "unknown age" if age is None else f"{float(age):.2f}s old"
        return (
            f"Generated {generated} · locally aged {age_text} · RF source {selected} · "
            f"node source {node_configuration}"
        )

    def display_signature(self) -> tuple[object, ...]:
        """Small state signature that changes when local freshness expires."""

        return (
            self.effective_source_state,
            self.effective_comparison.status,
            self.reference_deployment_claim_authorized,
            tuple(str(item["id"]) for item in self.tracks_for_display),
            self.cue_links(),
        )


class ReplayModel:
    """Pure playback state used by the GUI and headless tests."""

    def __init__(self, replay: Mapping[str, Any]) -> None:
        validate_dashboard_replay(replay)
        self.replay = dict(replay)
        timeline = replay["timeline"]
        self.duration_seconds = _finite(
            "timeline.durationSeconds", timeline["durationSeconds"]
        )
        self.cadence_seconds = _finite(
            "timeline.cadenceSeconds", timeline["cadenceSeconds"]
        )
        self.current_seconds = 0.0
        self.playing = False
        self.speed = 1.0
        self._tracks = tuple(timeline["tracks"])
        self._detections = tuple(
            sorted(
                timeline["detections"],
                key=lambda item: (
                    float(item["tSeconds"]),
                    str(item["trackId"]),
                    str(item["sensorId"]),
                ),
            )
        )
        self._detection_times = tuple(
            float(item["tSeconds"]) for item in self._detections
        )

    @property
    def tracks(self) -> tuple[Mapping[str, Any], ...]:
        return self._tracks

    @property
    def detections(self) -> tuple[Mapping[str, Any], ...]:
        return self._detections

    def reset(self) -> None:
        self.current_seconds = 0.0
        self.playing = False

    def seek(self, seconds: float) -> None:
        self.current_seconds = min(max(_finite("seconds", seconds), 0.0), self.duration_seconds)

    def advance(self, elapsed_wall_seconds: float) -> bool:
        """Advance playback and return True when the replay reaches its end."""

        if not self.playing:
            return False
        elapsed = max(_finite("elapsed_wall_seconds", elapsed_wall_seconds), 0.0)
        self.seek(self.current_seconds + elapsed * self.speed)
        finished = self.current_seconds >= self.duration_seconds - 1e-9
        if finished:
            self.playing = False
        return finished

    @staticmethod
    def _interpolate_location(
        first: Mapping[str, Any],
        second: Mapping[str, Any],
        fraction: float,
    ) -> dict[str, float | str]:
        left = first["location"]
        right = second["location"]
        return {
            "latitudeDegrees": float(left["latitudeDegrees"])
            + (float(right["latitudeDegrees"]) - float(left["latitudeDegrees"]))
            * fraction,
            "longitudeDegrees": float(left["longitudeDegrees"])
            + (float(right["longitudeDegrees"]) - float(left["longitudeDegrees"]))
            * fraction,
            "heightMeters": float(left["heightMeters"])
            + (float(right["heightMeters"]) - float(left["heightMeters"]))
            * fraction,
            "heightReference": str(
                left.get("heightReference", "WGS84_ELLIPSOID")
            ),
        }

    def track_state(self, track: Mapping[str, Any]) -> dict[str, Any]:
        samples = tuple(track["samples"])
        if not samples:
            raise ValueError("dashboard track has no samples")
        times = [float(item["tSeconds"]) for item in samples]
        upper = bisect_right(times, self.current_seconds)
        if upper <= 0:
            return dict(samples[0])
        if upper >= len(samples):
            return dict(samples[-1])
        first = samples[upper - 1]
        second = samples[upper]
        start = float(first["tSeconds"])
        stop = float(second["tSeconds"])
        fraction = 0.0 if stop <= start else (self.current_seconds - start) / (stop - start)
        state = dict(first)
        state["tSeconds"] = self.current_seconds
        state["location"] = self._interpolate_location(first, second, fraction)
        for key in ("headingDegrees", "speedMetersPerSecond", "distanceToAoiCenterMeters"):
            if key in first and key in second:
                state[key] = float(first[key]) + (float(second[key]) - float(first[key])) * fraction
        state["insideAoi"] = bool(
            first.get("insideAoi", False)
            if fraction < 0.5
            else second.get("insideAoi", False)
        )
        return state

    def active_detections(self) -> tuple[Mapping[str, Any], ...]:
        # Keep the most recent cue visible until the next sample, but never
        # reveal a future replay event before its timestamp.
        lookback = max(self.cadence_seconds * 1.05, 0.1)
        start = bisect_left(
            self._detection_times, self.current_seconds - lookback - 1e-9
        )
        stop = bisect_right(self._detection_times, self.current_seconds + 1e-9)
        return self._detections[start:stop]

    def recent_detections(self, limit: int = 10) -> tuple[Mapping[str, Any], ...]:
        if limit <= 0:
            return ()
        stop = bisect_right(self._detection_times, self.current_seconds + 1e-9)
        return self._detections[max(0, stop - limit):stop]

    def detected_track_ids(self) -> frozenset[str]:
        stop = bisect_right(self._detection_times, self.current_seconds + 1e-9)
        return frozenset(str(item["trackId"]) for item in self._detections[:stop])


def _load_json(path: Path) -> Mapping[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, Mapping):
        raise TypeError("replay root must be an object")
    validate_dashboard_replay(value)
    return value


def build_default_replay(
    *,
    scenario_id: str | None,
    duration_seconds: float,
    cadence_seconds: float,
) -> Mapping[str, Any]:
    root = _repository_root()
    request = PlacementRequest.load(
        root / "core" / "examples" / "istana_1km_placement_request.json"
    )
    recommendation = load_recommendation(
        root / "core" / "reports" / "istana_1km_recommendation.json"
    )
    scenario_path = root / "unreal" / "Config" / "IstanaSensorPlacement.base.json"
    scenario = json.loads(scenario_path.read_text(encoding="utf-8-sig"))
    if not isinstance(scenario, Mapping):
        raise TypeError("default Unreal scenario root must be an object")
    return build_dashboard_replay(
        request,
        recommendation,
        scenario,
        scenario_id=scenario_id,
        duration_seconds=duration_seconds,
        cadence_seconds=cadence_seconds,
    )


class OperatorDashboard:
    """Tk map for a replay or a separated live layer over its reference."""

    def __init__(
        self,
        model: ReplayModel,
        *,
        autoplay: bool = False,
        live_client: LiveSnapshotClient | None = None,
        live_poll_seconds: float = _DEFAULT_LIVE_POLL_SECONDS,
    ) -> None:
        try:
            import tkinter as tk
            from tkinter import ttk
        except ImportError as exc:  # pragma: no cover - platform dependent
            raise RuntimeError(
                "This Python installation does not include Tk support. "
                "Use a standard Windows Python build with tkinter enabled."
            ) from exc

        self.tk = tk
        self.ttk = ttk
        self.model = model
        self.root = tk.Tk()
        self.root.title("TRIAD — Istana placement replay")
        self.root.geometry(_DEFAULT_WINDOW_SIZE)
        self.root.minsize(*_MINIMUM_WINDOW_SIZE)
        self.root.configure(bg="#0b1015")
        self._last_tick = time.monotonic()
        poll_seconds = _finite("live_poll_seconds", live_poll_seconds)
        if not _MIN_LIVE_POLL_SECONDS <= poll_seconds <= _MAX_LIVE_POLL_SECONDS:
            raise ValueError(
                "live poll interval must be between "
                f"{_MIN_LIVE_POLL_SECONDS:g} and {_MAX_LIVE_POLL_SECONDS:g} seconds"
            )
        self.live_client = live_client
        self.live_model = (
            LiveSnapshotModel(model.replay) if live_client is not None else None
        )
        self._live_poll_seconds = poll_seconds
        self._live_results: queue.SimpleQueue[tuple[str, object]] = queue.SimpleQueue()
        self._live_fetch_inflight = False
        self._next_live_fetch = 0.0
        self._last_live_display_signature = (
            self.live_model.display_signature()
            if self.live_model is not None
            else None
        )
        self._selected_sensor_id = str(model.replay["sensors"][0]["nodeId"])
        self._selected_sensor_source = "reference"
        self._sensor_hitboxes: list[tuple[float, float, float, str, str]] = []
        self._configure_style()
        self._build_ui()
        self.model.playing = autoplay and live_client is None
        self._sync_controls()
        self._redraw()
        self.root.after(50, self._tick)

    def _configure_style(self) -> None:
        style = self.ttk.Style(self.root)
        style.theme_use("clam")
        style.configure("Root.TFrame", background="#0b1015")
        style.configure("Top.TFrame", background="#101820")
        style.configure("Panel.TFrame", background="#111a23")
        style.configure(
            "Title.TLabel",
            background="#101820",
            foreground="#eef7f8",
            font=("Segoe UI Semibold", 15),
        )
        style.configure(
            "Status.TLabel",
            background="#101820",
            foreground="#75e6c4",
            font=("Segoe UI Semibold", 9),
        )
        style.configure(
            "PanelTitle.TLabel",
            background="#111a23",
            foreground="#eef7f8",
            font=("Segoe UI Semibold", 11),
        )
        style.configure(
            "Body.TLabel",
            background="#111a23",
            foreground="#c7d3da",
            font=("Segoe UI", 9),
        )
        style.configure(
            "Muted.TLabel",
            background="#111a23",
            foreground="#81919b",
            font=("Segoe UI", 8),
        )
        style.configure(
            "Metric.TLabel",
            background="#111a23",
            foreground="#75e6c4",
            font=("Cascadia Mono", 10),
        )
        style.configure(
            "Action.TButton",
            background="#223241",
            foreground="#eef7f8",
            borderwidth=0,
            padding=(12, 7),
            font=("Segoe UI Semibold", 9),
        )
        style.map("Action.TButton", background=[("active", "#2d4354")])
        style.configure(
            "Primary.TButton",
            background="#2e8b76",
            foreground="#f5ffff",
            borderwidth=0,
            padding=(14, 7),
            font=("Segoe UI Semibold", 9),
        )
        style.map("Primary.TButton", background=[("active", "#3ca58e")])
        style.configure(
            "Horizontal.TScale",
            background="#0b1015",
            troughcolor="#23313d",
        )

    def _build_ui(self) -> None:
        tk = self.tk
        ttk = self.ttk
        top = ttk.Frame(self.root, style="Top.TFrame", padding=(18, 12))
        top.pack(fill=tk.X)
        ttk.Label(top, text="TRIAD / SENSOR PLACEMENT", style="Title.TLabel").pack(side=tk.LEFT)
        self.mode_label = ttk.Label(
            top,
            text=_mode_banner_text(
                self.model.replay, live=self.live_model is not None
            ),
            style="Status.TLabel",
        )
        self.mode_label.pack(side=tk.LEFT, padx=(18, 0))
        self.time_label = ttk.Label(top, text="00:00.0", style="Status.TLabel")
        self.time_label.pack(side=tk.RIGHT)

        body = ttk.Frame(self.root, style="Root.TFrame")
        body.pack(fill=tk.BOTH, expand=True)
        map_frame = ttk.Frame(body, style="Root.TFrame")
        map_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.canvas = tk.Canvas(
            map_frame,
            bg="#0b1117",
            highlightthickness=0,
            cursor="crosshair",
        )
        self.canvas.pack(fill=tk.BOTH, expand=True)
        self.canvas.bind("<Configure>", lambda _event: self._redraw())
        self.canvas.bind("<Button-1>", self._select_sensor_at)

        panel = ttk.Frame(body, style="Panel.TFrame", padding=(16, 14), width=360)
        panel.pack(side=tk.RIGHT, fill=tk.Y)
        panel.pack_propagate(False)
        ttk.Label(
            panel,
            text=(
                "Frozen recommendation reference"
                if self.live_model is not None
                else "Recommendation"
            ),
            style="PanelTitle.TLabel",
        ).pack(anchor=tk.W)
        recommendation = self.model.replay["recommendation"]
        metric = recommendation["metrics"]
        scope = self.model.replay.get("stage0Scope", {})
        coverage_label = recommendation.get("coverageMetricLabel")
        if not isinstance(coverage_label, str) or not coverage_label.strip():
            coverage_label = (
                "Worst-scenario coverage"
                if scope.get("legacyRecommendationScopeComplete") is True
                else "Legacy-reported coverage"
            )
        summary = (
            f"{recommendation['selectedSiteCount']} sites  ·  "
            f"{recommendation['totalCostUnits']:.1f} cost units\n"
            f"{coverage_label}  "
            f"{float(metric.get('worstScenarioCoverageFraction', 0.0)) * 100.0:.1f}%"
        )
        ttk.Label(panel, text=summary, style="Metric.TLabel", justify=tk.LEFT).pack(
            anchor=tk.W, pady=(6, 2)
        )
        algorithm = str(recommendation["algorithm"])
        solver_proof_text = _solver_proof_text(recommendation)
        ttk.Label(
            panel,
            text=f"Solver: {algorithm}",
            style="Muted.TLabel",
            wraplength=326,
            justify=tk.LEFT,
        ).pack(anchor=tk.W, pady=(0, 6 if solver_proof_text is not None else 12))
        if solver_proof_text is not None:
            ttk.Label(
                panel,
                text="Solver proof · v2",
                style="PanelTitle.TLabel",
            ).pack(anchor=tk.W, pady=(0, 3))
            ttk.Label(
                panel,
                text=solver_proof_text,
                style="Body.TLabel",
                wraplength=326,
                justify=tk.LEFT,
            ).pack(anchor=tk.W, pady=(0, 12))
        scope_warnings = tuple(
            str(item)
            for item in scope.get("warnings", ())
        )
        if scope_warnings:
            ttk.Label(
                panel,
                text="Scope: " + " ".join(scope_warnings),
                style="Muted.TLabel",
                wraplength=326,
                justify=tk.LEFT,
            ).pack(anchor=tk.W, pady=(0, 12))

        if self.live_model is not None:
            ttk.Separator(panel).pack(fill=tk.X, pady=(0, 12))
            ttk.Label(panel, text="Live simulation", style="PanelTitle.TLabel").pack(
                anchor=tk.W
            )
            self.live_status_label = ttk.Label(
                panel,
                text="UNAVAILABLE · REFERENCE NOT COMPARABLE",
                style="Metric.TLabel",
                wraplength=326,
                justify=tk.LEFT,
            )
            self.live_status_label.pack(anchor=tk.W, pady=(5, 2))
            self.live_provenance_label = ttk.Label(
                panel,
                text="Waiting for the first bounded loopback snapshot",
                style="Muted.TLabel",
                wraplength=326,
                justify=tk.LEFT,
            )
            self.live_provenance_label.pack(anchor=tk.W, pady=(0, 12))

        ttk.Separator(panel).pack(fill=tk.X, pady=(0, 12))
        ttk.Label(panel, text="Selected placement", style="PanelTitle.TLabel").pack(anchor=tk.W)
        self.sensor_title = ttk.Label(panel, text="", style="Metric.TLabel")
        self.sensor_title.pack(anchor=tk.W, pady=(5, 2))
        self.sensor_detail = ttk.Label(
            panel,
            text="",
            style="Body.TLabel",
            wraplength=326,
            justify=tk.LEFT,
        )
        self.sensor_detail.pack(anchor=tk.W)
        self.rationale_detail = ttk.Label(
            panel,
            text="",
            style="Muted.TLabel",
            wraplength=326,
            justify=tk.LEFT,
        )
        self.rationale_detail.pack(anchor=tk.W, pady=(5, 12))

        ttk.Separator(panel).pack(fill=tk.X, pady=(0, 12))
        ttk.Label(panel, text="Detection feed", style="PanelTitle.TLabel").pack(anchor=tk.W)
        self.detection_summary = ttk.Label(panel, text="", style="Metric.TLabel")
        self.detection_summary.pack(anchor=tk.W, pady=(5, 4))
        self.event_text = tk.Text(
            panel,
            height=14,
            wrap=tk.WORD,
            bg="#111a23",
            fg="#b9c8d0",
            insertbackground="#eef7f8",
            selectbackground="#2e8b76",
            relief=tk.FLAT,
            font=("Cascadia Mono", 8),
            padx=0,
            pady=0,
            state=tk.DISABLED,
        )
        self.event_text.pack(fill=tk.BOTH, expand=True)

        controls = ttk.Frame(self.root, style="Root.TFrame", padding=(18, 10))
        controls.pack(fill=tk.X)
        self.run_button = ttk.Button(
            controls,
            text="Run simulated ingress",
            style="Primary.TButton",
            command=self._run,
        )
        self.run_button.pack(side=tk.LEFT)
        self.pause_button = ttk.Button(
            controls,
            text="Pause",
            style="Action.TButton",
            command=self._pause,
        )
        self.pause_button.pack(side=tk.LEFT, padx=(8, 0))
        self.reset_button = ttk.Button(
            controls,
            text="Reset",
            style="Action.TButton",
            command=self._reset,
        )
        self.reset_button.pack(side=tk.LEFT, padx=(8, 14))
        self.timeline_value = tk.DoubleVar(value=0.0)
        self.timeline = ttk.Scale(
            controls,
            from_=0.0,
            to=self.model.duration_seconds,
            variable=self.timeline_value,
            command=self._seek,
        )
        self.timeline.pack(side=tk.LEFT, fill=tk.X, expand=True)
        initial_speed = f"{self.model.speed:g}×"
        self.speed_value = tk.StringVar(value=initial_speed)
        self.speed_control = ttk.Combobox(
            controls,
            textvariable=self.speed_value,
            values=("0.5×", "1×", "2×", "4×", "8×", "16×"),
            state="readonly",
            width=5,
            font=("Segoe UI", 9),
        )
        self.speed_control.pack(side=tk.LEFT, padx=(14, 0))
        self.speed_control.bind("<<ComboboxSelected>>", self._set_speed)
        self.root.bind("<space>", lambda _event: self._toggle_playback())
        self.root.bind("r", lambda _event: self._reset())

    def _run(self) -> None:
        if self.live_model is not None:
            return
        if self.model.current_seconds >= self.model.duration_seconds - 1e-9:
            self.model.seek(0.0)
        self.model.playing = True
        self._last_tick = time.monotonic()
        self._sync_controls()

    def _pause(self) -> None:
        if self.live_model is not None:
            return
        self.model.playing = False
        self._sync_controls()

    def _reset(self) -> None:
        if self.live_model is not None:
            return
        self.model.reset()
        self._sync_controls()
        self._redraw()

    def _toggle_playback(self) -> None:
        if self.model.playing:
            self._pause()
        else:
            self._run()

    def _seek(self, value: str) -> None:
        if self.live_model is not None:
            return
        self.model.seek(float(value))
        self._redraw()

    def _set_speed(self, _event: object) -> None:
        if self.live_model is not None:
            return
        self.model.speed = float(self.speed_value.get().replace("×", ""))

    def _start_live_fetch(self) -> None:
        if self.live_client is None or self._live_fetch_inflight:
            return
        self._live_fetch_inflight = True

        def fetch() -> None:
            try:
                self._live_results.put(("snapshot", self.live_client.fetch()))
            except Exception as exc:  # delivered and classified on the Tk thread
                self._live_results.put(("error", exc))

        threading.Thread(
            target=fetch,
            name="triad-live-snapshot-poll",
            daemon=True,
        ).start()

    def _drain_live_results(self) -> bool:
        if self.live_model is None:
            return False
        changed = False
        while True:
            try:
                kind, value = self._live_results.get_nowait()
            except queue.Empty:
                break
            self._live_fetch_inflight = False
            changed = True
            if kind == "snapshot" and isinstance(value, Mapping):
                self.live_model.accept_snapshot(value)
            elif isinstance(value, LiveSnapshotMalformedError):
                self.live_model.mark_malformed(str(value))
            else:
                self.live_model.mark_unavailable(str(value))
        return changed

    def _tick(self) -> None:
        now = time.monotonic()
        elapsed = min(max(now - self._last_tick, 0.0), 0.25)
        self._last_tick = now
        live_changed = self._drain_live_results()
        if self.live_model is not None:
            if now >= self._next_live_fetch and not self._live_fetch_inflight:
                self._next_live_fetch = now + self._live_poll_seconds
                self._start_live_fetch()
            display_signature = self.live_model.display_signature()
            if display_signature != self._last_live_display_signature:
                self._last_live_display_signature = display_signature
                live_changed = True
            if live_changed:
                self._sync_controls()
                self._redraw()
        elif self.model.playing:
            self.model.advance(elapsed)
            self._sync_controls()
            self._redraw()
        self.root.after(50, self._tick)

    def _sync_controls(self) -> None:
        if self.live_model is not None:
            self.time_label.configure(
                text=f"LIVE · {self.live_model.effective_source_state.upper()}"
            )
            self.run_button.configure(text="Live polling")
            self.run_button.state(["disabled"])
            self.pause_button.state(["disabled"])
            self.reset_button.state(["disabled"])
            self.timeline.state(["disabled"])
            self.speed_control.configure(state="disabled")
            return
        self.timeline_value.set(self.model.current_seconds)
        minutes = int(self.model.current_seconds // 60.0)
        seconds = self.model.current_seconds - minutes * 60.0
        self.time_label.configure(
            text=f"T+{minutes:02d}:{seconds:04.1f} / {self.model.duration_seconds:.0f}s"
        )
        self.pause_button.state(["!disabled"] if self.model.playing else ["disabled"])
        self.run_button.configure(
            text=(
                "Resume"
                if 0.0 < self.model.current_seconds < self.model.duration_seconds
                else "Run simulated ingress"
            )
        )

    def _selected_sensor(self) -> Mapping[str, Any]:
        for sensor in self.model.replay["sensors"]:
            if sensor["nodeId"] == self._selected_sensor_id:
                return sensor
        return self.model.replay["sensors"][0]

    def _selected_live_node(self) -> Mapping[str, Any] | None:
        if self.live_model is None:
            return None
        for node in self.live_model.nodes_for_display:
            if node.get("id") == self._selected_sensor_id:
                return node
        return None

    @staticmethod
    def _comparison_detail(comparison: PlacementComparison) -> str:
        if comparison.matched:
            return "REFERENCE MATCHED"
        if comparison.status != "mismatched":
            return "REFERENCE NOT COMPARABLE"
        details: list[str] = []
        if comparison.missing_node_ids:
            details.append("missing " + ", ".join(comparison.missing_node_ids))
        if comparison.extra_node_ids:
            details.append("extra " + ", ".join(comparison.extra_node_ids))
        if comparison.coordinate_mismatches:
            details.append("coordinate mismatch " + "; ".join(comparison.coordinate_mismatches))
        return "REFERENCE MISMATCH · " + (" · ".join(details) or comparison.reason)

    def _update_live_panel(self) -> None:
        assert self.live_model is not None
        comparison_text = self._comparison_detail(
            self.live_model.effective_comparison
        )
        self.live_status_label.configure(
            text=(
                f"{self.live_model.effective_source_state.upper()} · "
                f"{comparison_text}"
            )
        )
        self.live_provenance_label.configure(
            text=self.live_model.provenance_text()
        )

        live_node = (
            self._selected_live_node()
            if self._selected_sensor_source == "live"
            else None
        )
        if live_node is not None:
            sensor_types = []
            long_range = live_node.get("longRangeSensors")
            if isinstance(long_range, list):
                sensor_types = [
                    str(item.get("sensorType", item.get("modality")))
                    for item in long_range
                    if isinstance(item, Mapping)
                    and item.get("sensorType", item.get("modality"))
                ]
            self.sensor_title.configure(text=f"LIVE / {live_node['id']}")
            self.sensor_detail.configure(
                text=(
                    f"{float(live_node['latitudeDegrees']):.6f}, "
                    f"{float(live_node['longitudeDegrees']):.6f}\n"
                    f"Runtime state: {live_node.get('state', 'unknown')}\n"
                    f"Sensors: {', '.join(sensor_types) or 'not reported'}"
                )
            )
            self.rationale_detail.configure(
                text=(
                    "Current runtime node, not a solver rationale. "
                    + comparison_text
                )
            )
        else:
            sensor = self._selected_sensor()
            location = sensor["location"]
            modalities = ", ".join(
                str(item["modality"]) for item in sensor["modalities"]
            ) or "No in-scope replay modality"
            self.sensor_title.configure(text=f"REFERENCE / {sensor['siteId']}")
            self.sensor_detail.configure(
                text=(
                    f"{float(location['latitudeDegrees']):.6f}, "
                    f"{float(location['longitudeDegrees']):.6f}\n"
                    f"Failure domain: {sensor['failureDomainId']}\n"
                    f"Sensors: {modalities}"
                )
            )
            self.rationale_detail.configure(
                text=f"Why selected: {sensor['rationale']['summary']}"
            )

        tracks = self.live_model.tracks_for_display
        detected = tuple(
            item for item in tracks if self.live_model.track_is_detection(item)
        )
        self.detection_summary.configure(
            text=(
                f"{len(detected)} current detected tracks · "
                f"{len(self.live_model.cue_links())} current cue links"
                if self.live_model.detection_rendering_enabled
                else (
                    "0 live tracks displayed · "
                    f"{self.live_model.effective_source_state.upper()}"
                )
            )
        )
        lines: list[str] = []
        if not self.live_model.detection_rendering_enabled:
            lines.append(
                "Live tracks and cues are suppressed because the source is not fresh.\n"
                f"{self.live_model.status_detail}\n"
            )
        else:
            if not self.live_model.effective_comparison.matched:
                lines.append(
                    "LIVE SCENARIO IS NOT THE FROZEN RECOMMENDATION REFERENCE\n\n"
                )
            for track in reversed(tracks[-11:]):
                modalities = track.get("activeModalities")
                modality_text = (
                    ",".join(str(item) for item in modalities)
                    if isinstance(modalities, list) and modalities
                    else "RF/unspecified"
                )
                fusion = track.get("fusionV3")
                fusion_decision = (
                    str(fusion.get("decision", "NOT_EVALUATED"))
                    if isinstance(fusion, Mapping)
                    else "NOT_EVALUATED"
                )
                range_value = track.get("distanceMeters")
                range_text = (
                    "range unavailable"
                    if range_value is None
                    else f"{float(range_value):.0f} m"
                )
                age_value = track.get("ageSeconds")
                age_text = (
                    "age unavailable"
                    if age_value is None
                    else f"{float(age_value):.2f}s old"
                )
                lines.append(
                    f"{track['id']}\n"
                    f"         {track.get('ueRfDecision', 'NO_DETECTION')} / "
                    f"{fusion_decision}\n"
                    f"         {modality_text} · {range_text} · {age_text}\n"
                )
        if not lines:
            lines.append("Fresh live snapshot contains no current detected tracks.")
        self.event_text.configure(state=self.tk.NORMAL)
        self.event_text.delete("1.0", self.tk.END)
        self.event_text.insert("1.0", "".join(lines))
        self.event_text.configure(state=self.tk.DISABLED)

    def _update_panel(self) -> None:
        if self.live_model is not None:
            self._update_live_panel()
            return
        sensor = self._selected_sensor()
        location = sensor["location"]
        modalities = ", ".join(
            str(item["modality"]) for item in sensor["modalities"]
        ) or "No in-scope replay modality"
        self.sensor_title.configure(text=str(sensor["siteId"]))
        self.sensor_detail.configure(
            text=(
                f"{float(location['latitudeDegrees']):.6f}, "
                f"{float(location['longitudeDegrees']):.6f}\n"
                f"Failure domain: {sensor['failureDomainId']}\n"
                f"Sensors: {modalities}"
            )
        )
        rationale = sensor["rationale"]
        self.rationale_detail.configure(
            text=f"Why selected: {rationale['summary']}"
        )

        detected = self.model.detected_track_ids()
        total = len(self.model.tracks)
        active = self.model.active_detections()
        self.detection_summary.configure(
            text=f"{len(detected)}/{total} tracks detected · {len(active)} active cues"
        )
        recent = self.model.recent_detections(11)
        lines = []
        for event in reversed(recent):
            latency_semantics = str(event.get("latencySemantics", ""))
            latency_text = (
                f"acquired at T+{float(event['latencyMilliseconds']) / 1000.0:.1f} s"
                if latency_semantics.startswith("DISPLAY_TIME_FROM_REPLAY_START")
                else f"display latency {float(event['latencyMilliseconds']):.0f} ms"
            )
            lines.append(
                f"{float(event['tSeconds']):6.1f}s  {event['trackId']}\n"
                f"         {event['modality']} / {event['nodeId']}  "
                f"{float(event['rangeMeters']):.0f} m\n"
                f"         index {float(event['confidence']):.2f}  ·  "
                f"{latency_text}\n"
            )
        if not lines:
            lines.append("Waiting for model-derived detection evidence…")
        self.event_text.configure(state=self.tk.NORMAL)
        self.event_text.delete("1.0", self.tk.END)
        self.event_text.insert("1.0", "".join(lines))
        self.event_text.configure(state=self.tk.DISABLED)

    def _select_sensor_at(self, event: Any) -> None:
        for x, y, radius, node_id, source in reversed(self._sensor_hitboxes):
            if (event.x - x) ** 2 + (event.y - y) ** 2 <= radius**2:
                self._selected_sensor_id = node_id
                self._selected_sensor_source = source
                self._redraw()
                return

    def _draw_live_scene(
        self,
        canvas: Any,
        projector: MapProjector,
        origin: Mapping[str, Any],
        *,
        height: float,
    ) -> None:
        assert self.live_model is not None
        self._sensor_hitboxes = []
        reference_positions: dict[str, tuple[float, float]] = {}
        for sensor in self.model.replay["sensors"]:
            east, north, _up = location_to_enu(sensor["location"], origin)
            x, y = projector.project(east, north)
            node_id = str(sensor["nodeId"])
            reference_positions[node_id] = (x, y)
            selected = (
                self._selected_sensor_source == "reference"
                and node_id == self._selected_sensor_id
            )
            if selected:
                canvas.create_oval(
                    x - 14,
                    y - 14,
                    x + 14,
                    y + 14,
                    outline="#48a9d4",
                    width=2,
                )
            canvas.create_rectangle(
                x - 8,
                y - 8,
                x + 8,
                y + 8,
                fill="",
                outline="#48a9d4",
                width=2,
                dash=(3, 2),
            )
            canvas.create_text(
                x + 11,
                y - 9,
                text=f"REF {sensor['siteId']}",
                anchor="sw",
                fill="#7dbbd6",
                font=("Segoe UI", 8),
            )
            self._sensor_hitboxes.append((x, y, 16.0, node_id, "reference"))

        live_positions: dict[str, tuple[float, float]] = {}
        effective_state = self.live_model.effective_source_state
        live_color = "#75e6c4" if effective_state == "fresh" else "#657987"
        for node in self.live_model.nodes_for_display:
            east, north, _up = location_to_enu(
                {
                    "latitudeDegrees": node["latitudeDegrees"],
                    "longitudeDegrees": node["longitudeDegrees"],
                    "heightMeters": (
                        node["heightMeters"]
                        if node.get("heightMeters") is not None
                        else origin.get("heightMeters", 0.0)
                    ),
                },
                origin,
            )
            x, y = projector.project(east, north)
            node_id = str(node["id"])
            live_positions[node_id] = (x, y)
            selected = (
                self._selected_sensor_source == "live"
                and node_id == self._selected_sensor_id
            )
            if selected:
                canvas.create_oval(
                    x - 15,
                    y - 15,
                    x + 15,
                    y + 15,
                    outline=live_color,
                    width=2,
                )
            canvas.create_rectangle(
                x - 7,
                y - 7,
                x + 7,
                y + 7,
                fill=live_color,
                outline="#e5fffa" if effective_state == "fresh" else "#81919b",
                width=1,
            )
            canvas.create_text(
                x + 11,
                y + 10,
                text=f"LIVE {node_id}",
                anchor="nw",
                fill=live_color,
                font=("Segoe UI", 8),
            )
            self._sensor_hitboxes.append((x, y, 16.0, node_id, "live"))

        track_positions: dict[str, tuple[float, float]] = {}
        for track in self.live_model.tracks_for_display:
            east, north, _up = location_to_enu(
                {
                    "latitudeDegrees": track["latitudeDegrees"],
                    "longitudeDegrees": track["longitudeDegrees"],
                    "heightMeters": (
                        track["altitudeMeters"]
                        if track.get("altitudeMeters") is not None
                        else origin.get("heightMeters", 0.0)
                    ),
                },
                origin,
            )
            x, y = projector.project(east, north)
            track_id = str(track["id"])
            track_positions[track_id] = (x, y)
            color = (
                "#75e6c4"
                if self.live_model.track_is_detection(track)
                else "#f2a65a"
            )
            canvas.create_polygon(
                x,
                y - 6,
                x + 6,
                y,
                x,
                y + 6,
                x - 6,
                y,
                fill=color,
                outline="#071015",
                width=1,
            )
            canvas.create_text(
                x + 9,
                y - 7,
                text=track_id,
                anchor="sw",
                fill=color,
                font=("Cascadia Mono", 7),
            )

        for node_id, track_id in self.live_model.cue_links():
            if node_id not in live_positions or track_id not in track_positions:
                continue
            sensor_x, sensor_y = live_positions[node_id]
            target_x, target_y = track_positions[track_id]
            canvas.create_line(
                sensor_x,
                sensor_y,
                target_x,
                target_y,
                fill="#75e6c4",
                width=2,
                dash=(4, 3),
            )

        comparison_text = self._comparison_detail(
            self.live_model.effective_comparison
        )
        status_color = (
            "#75e6c4"
            if self.live_model.reference_deployment_claim_authorized
            else "#f2a65a"
            if effective_state == "fresh"
            else "#d47676"
        )
        canvas.create_text(
            18,
            18,
            text=f"LIVE {effective_state.upper()} · {comparison_text}",
            anchor="nw",
            fill=status_color,
            font=("Segoe UI Semibold", 9),
        )
        canvas.create_text(
            18,
            height - 18,
            text=(
                "□ frozen recommendation reference    ■ current runtime node    "
                "◆ fresh current track    ┄ current detection cue"
            ),
            anchor="sw",
            fill="#81919b",
            font=("Segoe UI", 8),
        )

    def _redraw(self) -> None:
        canvas = self.canvas
        width = max(float(canvas.winfo_width()), 1.0)
        height = max(float(canvas.winfo_height()), 1.0)
        canvas.delete("all")
        aoi = self.model.replay["map"]["aoi"]
        origin = aoi["center"]
        radius = float(aoi["radiusMeters"])
        projector = MapProjector(width, height, radius)
        center_x, center_y = projector.project(0.0, 0.0)

        for fraction, label in ((0.25, "250 m"), (0.5, "500 m"), (0.75, "750 m"), (1.0, "1 km AOI")):
            ring = radius * fraction * projector.scale
            canvas.create_oval(
                center_x - ring,
                center_y - ring,
                center_x + ring,
                center_y + ring,
                outline="#263743" if fraction < 1.0 else "#466270",
                width=1 if fraction < 1.0 else 2,
            )
            canvas.create_text(
                center_x + 7,
                center_y - ring + 11,
                text=label,
                anchor="w",
                fill="#657987",
                font=("Segoe UI", 8),
            )
        canvas.create_line(center_x, 20, center_x, height - 20, fill="#1b2933", width=1)
        canvas.create_line(20, center_y, width - 20, center_y, fill="#1b2933", width=1)
        canvas.create_text(center_x, 16, text="N", fill="#91a7b3", font=("Segoe UI Semibold", 9))
        canvas.create_text(width - 18, center_y, text="E", fill="#91a7b3", font=("Segoe UI Semibold", 9))
        canvas.create_text(center_x + 8, center_y + 12, text="ISTANA AOI", anchor="nw", fill="#516671", font=("Segoe UI", 8))

        if self.live_model is not None:
            self._draw_live_scene(
                canvas,
                projector,
                origin,
                height=height,
            )
            self._update_panel()
            return

        track_state: dict[str, Mapping[str, Any]] = {}
        detected_ids = self.model.detected_track_ids()
        for track in self.model.tracks:
            points: list[float] = []
            for sample in track["samples"]:
                east, north, _up = location_to_enu(sample["location"], origin)
                x, y = projector.project(east, north)
                points.extend((x, y))
            if len(points) >= 4:
                canvas.create_line(*points, fill="#263842", width=1, smooth=True)
            state = self.model.track_state(track)
            track_state[str(track["trackId"])] = state
            east, north, _up = location_to_enu(state["location"], origin)
            x, y = projector.project(east, north)
            color = "#75e6c4" if track["trackId"] in detected_ids else "#f2a65a"
            canvas.create_polygon(
                x,
                y - 5,
                x + 5,
                y,
                x,
                y + 5,
                x - 5,
                y,
                fill=color,
                outline="#071015",
                width=1,
            )

        sensor_positions: dict[str, tuple[float, float]] = {}
        self._sensor_hitboxes = []
        for sensor in self.model.replay["sensors"]:
            east, north, _up = location_to_enu(sensor["location"], origin)
            x, y = projector.project(east, north)
            node_id = str(sensor["nodeId"])
            sensor_positions[node_id] = (x, y)
            selected = node_id == self._selected_sensor_id
            radius_px = 9.0 if selected else 7.0
            if selected:
                canvas.create_oval(
                    x - 14,
                    y - 14,
                    x + 14,
                    y + 14,
                    outline="#75e6c4",
                    width=2,
                )
            canvas.create_rectangle(
                x - radius_px,
                y - radius_px,
                x + radius_px,
                y + radius_px,
                fill="#48a9d4",
                outline="#d7f5ff",
                width=1,
            )
            canvas.create_text(
                x + 11,
                y - 9,
                text=str(sensor["siteId"]),
                anchor="sw",
                fill="#a9c2cc",
                font=("Segoe UI", 8),
            )
            self._sensor_hitboxes.append((x, y, 16.0, node_id, "reference"))

        drawn_links: set[tuple[str, str]] = set()
        for event in self.model.active_detections():
            key = (str(event["nodeId"]), str(event["trackId"]))
            if key in drawn_links or key[0] not in sensor_positions or key[1] not in track_state:
                continue
            drawn_links.add(key)
            sensor_x, sensor_y = sensor_positions[key[0]]
            east, north, _up = location_to_enu(track_state[key[1]]["location"], origin)
            target_x, target_y = projector.project(east, north)
            canvas.create_line(
                sensor_x,
                sensor_y,
                target_x,
                target_y,
                fill="#75e6c4",
                width=2,
                dash=(4, 3),
            )
        canvas.create_text(
            18,
            height - 18,
            text="■ recommended sensor node    ◆ simulated UAS    ┄ active detection",
            anchor="sw",
            fill="#81919b",
            font=("Segoe UI", 8),
        )
        self._update_panel()

    def run(self) -> None:
        self.root.mainloop()


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Open the local TRIAD placement/drone/detection dashboard. "
            "Without --replay it builds a deterministic in-memory study replay "
            "from the repository fixtures. --live-url adds a separately labelled "
            "GET-only loopback view over that frozen reference."
        )
    )
    parser.add_argument("--replay", type=Path, help="triad.dashboard_replay.v1 JSON")
    parser.add_argument("--scenario-id", help="placement scenario ID for the default replay")
    parser.add_argument("--duration-seconds", type=float, default=150.0)
    parser.add_argument("--cadence-seconds", type=float, default=1.0)
    parser.add_argument("--speed", type=float, default=1.0)
    parser.add_argument("--autoplay", action="store_true")
    parser.add_argument(
        "--live-url",
        help=(
            "exact loopback /api/snapshot URL, normally "
            f"{_DEFAULT_LIVE_URL}"
        ),
    )
    parser.add_argument(
        "--live-poll-seconds",
        type=float,
        default=_DEFAULT_LIVE_POLL_SECONDS,
        help="bounded live polling interval (0.1 to 10 seconds)",
    )
    parser.add_argument(
        "--live-timeout-seconds",
        type=float,
        default=_DEFAULT_LIVE_TIMEOUT_SECONDS,
        help="bounded per-request timeout (0.1 to 5 seconds)",
    )
    parser.add_argument(
        "--validate-only",
        action="store_true",
        help="validate/build the replay and print a summary without opening a window",
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    replay = (
        _load_json(args.replay)
        if args.replay is not None
        else build_default_replay(
            scenario_id=args.scenario_id,
            duration_seconds=args.duration_seconds,
            cadence_seconds=args.cadence_seconds,
        )
    )
    model = ReplayModel(replay)
    if not math.isfinite(args.speed) or args.speed <= 0.0:
        raise ValueError("--speed must be finite and greater than zero")
    model.speed = args.speed
    live_client = None
    if args.live_url is not None:
        if args.autoplay:
            raise ValueError("--autoplay is replay-only and cannot be used with --live-url")
        poll_seconds = _finite("--live-poll-seconds", args.live_poll_seconds)
        if not _MIN_LIVE_POLL_SECONDS <= poll_seconds <= _MAX_LIVE_POLL_SECONDS:
            raise ValueError(
                "--live-poll-seconds must be between "
                f"{_MIN_LIVE_POLL_SECONDS:g} and {_MAX_LIVE_POLL_SECONDS:g}"
            )
        live_client = LiveSnapshotClient(
            args.live_url,
            timeout_seconds=args.live_timeout_seconds,
        )
    if args.validate_only:
        if live_client is not None:
            live_model = LiveSnapshotModel(model.replay)
            exit_code = 0
            try:
                live_model.accept_snapshot(live_client.fetch())
            except LiveSnapshotMalformedError as exc:
                live_model.mark_malformed(str(exc))
                exit_code = 2
            except LiveSnapshotError as exc:
                live_model.mark_unavailable(str(exc))
                exit_code = 2
            snapshot = live_model.snapshot or {}
            validation_summary: dict[str, Any] = {
                "schemaVersion": model.replay["schemaVersion"],
                "referenceReplayDigest": model.replay["replayDigest"],
                "liveSnapshotSchemaVersion": snapshot.get("schemaVersion"),
                "liveState": live_model.effective_source_state,
                "liveMode": snapshot.get("mode", "unavailable"),
                "placementComparison": live_model.effective_comparison.status,
                "referenceDeploymentClaimAuthorized": (
                    live_model.reference_deployment_claim_authorized
                ),
                "detectionRenderingEnabled": live_model.detection_rendering_enabled,
                "liveNodeCount": len(live_model.nodes_for_display),
                "liveTrackCount": len(live_model.tracks_for_display),
                "simulationOnly": True,
                "detectionOnly": True,
            }
            reference_proof = model.replay["recommendation"].get("solverProof")
            if isinstance(reference_proof, Mapping):
                validation_summary["referenceSolverProof"] = dict(reference_proof)
            print(json.dumps(validation_summary, separators=(",", ":")))
            return exit_code
        summary = model.replay["timeline"]["successSummary"]
        validation_summary = {
            "schemaVersion": model.replay["schemaVersion"],
            "replayDigest": model.replay["replayDigest"],
            "selectedSiteCount": len(model.replay["sensors"]),
            "simulatedTrackCount": summary["simulatedTrackCount"],
            "detectedTrackCount": summary["detectedTrackCount"],
            "allTracksDetected": summary["allTracksDetected"],
            "simulationOnly": True,
        }
        solver_proof = model.replay["recommendation"].get("solverProof")
        if isinstance(solver_proof, Mapping):
            validation_summary["solverProof"] = dict(solver_proof)
        print(json.dumps(validation_summary, separators=(",", ":")))
        return 0
    OperatorDashboard(
        model,
        autoplay=args.autoplay,
        live_client=live_client,
        live_poll_seconds=args.live_poll_seconds,
    ).run()
    return 0


if __name__ == "__main__":  # pragma: no cover - manual desktop entry point
    raise SystemExit(main())
