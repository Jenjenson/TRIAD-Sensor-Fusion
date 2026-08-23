"""Bounded, self-expiring buffer that assembles one producer sample.

The store exists only to turn many small per-sensor HTTP posts into the single
``triad.live_rf_snapshot.v3`` document the fusion runtime already accepts.  It
deliberately holds no track state: the newest record per sensor/target key wins,
ancient records are pruned, and nothing is carried across a fusion call other
than the raw observations still inside their freshness window.  Track continuity
and association remain exactly as absent here as they are in the runtime.
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
import threading
from typing import Any, Iterable, Mapping

from .models import SimulationPerimeter, Weather

INPUT_SCHEMA = "triad.live_rf_snapshot.v3"


def _parse_timestamp(value: object) -> datetime | None:
    if not isinstance(value, str) or not value.strip():
        return None
    text = value.strip()
    normalized = text[:-1] + "+00:00" if text.endswith("Z") else text
    try:
        parsed = datetime.fromisoformat(normalized)
    except ValueError:
        return None
    if parsed.tzinfo is None or parsed.utcoffset() is None:
        return None
    return parsed.astimezone(timezone.utc)


def iso(value: datetime) -> str:
    return value.astimezone(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


@dataclass(frozen=True, slots=True)
class StoreLimits:
    """Hard memory bounds. Exceeding a cap evicts the oldest record, never the newest."""

    max_nodes: int = 128
    max_records_per_modality: int = 4096
    node_ttl_s: float = 30.0
    prune_records_after_s: float = 20.0


@dataclass(frozen=True, slots=True)
class AssembledSample:
    """One producer snapshot plus the model rows that travel beside it."""

    snapshot: dict[str, Any]
    rgb_rows: tuple[dict[str, Any], ...]
    event_rows: tuple[dict[str, Any], ...]


@dataclass(slots=True)
class _Entry:
    record: dict[str, Any]
    observed_at: datetime
    received_at: datetime


DEFAULT_WEATHER = Weather()


class ObservationStore:
    """Thread-safe latest-wins buffer for one fusion service instance."""

    def __init__(self, limits: StoreLimits | None = None) -> None:
        self._limits = limits or StoreLimits()
        self._lock = threading.Lock()
        self._nodes: dict[str, _Entry] = {}
        self._rf: dict[str, _Entry] = {}
        self._radar: dict[str, _Entry] = {}
        self._ptz: dict[str, _Entry] = {}
        self._rgb: dict[str, _Entry] = {}
        self._event: dict[str, _Entry] = {}
        self._weather: Weather = DEFAULT_WEATHER
        self._perimeter: SimulationPerimeter | None = None

    # -- ingest ---------------------------------------------------------------

    def upsert_nodes(self, records: Iterable[Mapping[str, Any]]) -> int:
        return self._upsert(self._nodes, records, key=lambda row: str(row["nodeId"]), limit=self._limits.max_nodes)

    def upsert_rf(self, records: Iterable[Mapping[str, Any]]) -> int:
        return self._upsert(
            self._rf,
            records,
            key=lambda row: f"{row['nodeId']}|{row['targetActor']}|{round(float(row['frequencyGHz']) * 1e9)}",
        )

    def upsert_radar(self, records: Iterable[Mapping[str, Any]]) -> int:
        return self._upsert(
            self._radar,
            records,
            key=lambda row: f"{row.get('sensorId')}|{row['targetActor']}",
        )

    def upsert_ptz(self, records: Iterable[Mapping[str, Any]]) -> int:
        return self._upsert(
            self._ptz,
            records,
            key=lambda row: f"{row.get('sensorId')}|{row['targetActor']}",
        )

    def upsert_visual(self, records: Iterable[Mapping[str, Any]]) -> int:
        rgb = [row for row in records if row.get("modality") == "RGB"]
        event = [row for row in records if row.get("modality") == "EVENT_CAMERA"]
        stored = self._upsert(self._rgb, rgb, key=lambda row: str(row["evidenceId"]))
        stored += self._upsert(self._event, event, key=lambda row: str(row["evidenceId"]))
        return stored

    def set_environment(
        self,
        *,
        weather: Weather | None = None,
        perimeter: SimulationPerimeter | None = None,
    ) -> None:
        with self._lock:
            if weather is not None:
                self._weather = weather
            if perimeter is not None:
                self._perimeter = perimeter

    def clear(self) -> None:
        with self._lock:
            for bucket in (self._nodes, self._rf, self._radar, self._ptz, self._rgb, self._event):
                bucket.clear()

    def _upsert(
        self,
        bucket: dict[str, _Entry],
        records: Iterable[Mapping[str, Any]],
        *,
        key,
        limit: int | None = None,
    ) -> int:
        received_at = datetime.now(timezone.utc)
        cap = limit or self._limits.max_records_per_modality
        with self._lock:
            for row in records:
                record = dict(row)
                observed_at = _parse_timestamp(record.get("timestampUtc")) or received_at
                bucket[key(record)] = _Entry(record, observed_at, received_at)
            while len(bucket) > cap:
                oldest = min(bucket, key=lambda name: bucket[name].observed_at)
                del bucket[oldest]
            return len(bucket)

    # -- assembly -------------------------------------------------------------

    def prune(self, as_of: datetime) -> int:
        """Drop expired nodes and ancient observations. Returns records removed."""

        removed = 0
        with self._lock:
            for name, entry in list(self._nodes.items()):
                if (as_of - entry.received_at).total_seconds() > self._limits.node_ttl_s:
                    del self._nodes[name]
                    removed += 1
            for bucket in (self._rf, self._radar, self._ptz, self._rgb, self._event):
                for name, entry in list(bucket.items()):
                    if (as_of - entry.observed_at).total_seconds() > self._limits.prune_records_after_s:
                        del bucket[name]
                        removed += 1
        return removed

    def assemble(self, as_of: datetime) -> AssembledSample:
        """Build the v3 producer document for ``as_of``.

        ``scenarioTargets`` is always empty: this API has no authored-truth ingest
        path, so evaluator truth cannot enter through it at all.
        """

        self.prune(as_of)
        with self._lock:
            snapshot: dict[str, Any] = {
                "schemaVersion": INPUT_SCHEMA,
                "sampleComplete": True,
                "detectionOnly": True,
                "actionsTaken": "none",
                "timestampUtc": iso(as_of),
                "simulationSeconds": as_of.timestamp(),
                "weather": self._weather.model_dump(mode="json"),
                "sensorNodes": [entry.record for entry in self._nodes.values()],
                "scenarioTargets": [],
                "detectedRFLinks": [entry.record for entry in self._rf.values()],
                "searchRadarDetections": [entry.record for entry in self._radar.values()],
                "ptzConfirmations": [entry.record for entry in self._ptz.values()],
                "producedBy": "triad-fusion-api",
                "producerSemantics": (
                    "assembled from per-sensor HTTP observations; buffering is transport "
                    "only and provides no track continuity"
                ),
            }
            if self._perimeter is not None:
                snapshot["simulationPerimeter"] = self._perimeter.model_dump(mode="json")
            return AssembledSample(
                snapshot=snapshot,
                rgb_rows=tuple(entry.record for entry in self._rgb.values()),
                event_rows=tuple(entry.record for entry in self._event.values()),
            )

    def stats(self, as_of: datetime) -> dict[str, Any]:
        with self._lock:
            def oldest_age(bucket: dict[str, _Entry]) -> float | None:
                if not bucket:
                    return None
                return round(
                    max((as_of - entry.observed_at).total_seconds() for entry in bucket.values()), 3
                )

            return {
                "sensorNodes": len(self._nodes),
                "rfLinks": len(self._rf),
                "searchRadarDetections": len(self._radar),
                "ptzConfirmations": len(self._ptz),
                "rgbModelDetections": len(self._rgb),
                "eventModelDetections": len(self._event),
                "oldestBufferedAgeSeconds": {
                    "rf": oldest_age(self._rf),
                    "searchRadar": oldest_age(self._radar),
                    "ptz": oldest_age(self._ptz),
                },
                "weatherProfile": self._weather.profile,
                "simulationPerimeterConfigured": self._perimeter is not None,
                "limits": {
                    "maxNodes": self._limits.max_nodes,
                    "maxRecordsPerModality": self._limits.max_records_per_modality,
                    "nodeTtlSeconds": self._limits.node_ttl_s,
                    "pruneRecordsAfterSeconds": self._limits.prune_records_after_s,
                },
            }
