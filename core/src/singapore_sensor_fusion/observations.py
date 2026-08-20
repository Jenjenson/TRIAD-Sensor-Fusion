"""Versioned timestamped observation schema shared by simulated modalities."""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from enum import StrEnum
import json
import math
from typing import Any, Mapping


class SensorModality(StrEnum):
    RF = "rf"
    RGBD = "rgbd"
    EVENT_CAMERA = "event_camera"


def _probability(name: str, value: float) -> float:
    result = float(value)
    if not math.isfinite(result) or not 0.0 <= result <= 1.0:
        raise ValueError(f"{name} must be finite and in [0, 1]")
    return result


def _identifier(name: str, value: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{name} must be a non-empty string")
    return value.strip()


@dataclass(frozen=True, slots=True)
class SensorObservation:
    """One JSON-safe, timezone-aware sensor observation."""

    observation_id: str
    timestamp: datetime
    node_id: str
    modality: SensorModality
    detection_probability: float
    confidence_level: float
    target_track_id: str | None = None
    frequency_hz: float | None = None
    position_ecef_m: tuple[float, float, float] | None = None
    model_id: str | None = None
    measurements: Mapping[str, Any] = field(default_factory=dict)
    schema_version: str = "1.0"

    def __post_init__(self) -> None:
        object.__setattr__(self, "observation_id", _identifier("observation_id", self.observation_id))
        object.__setattr__(self, "node_id", _identifier("node_id", self.node_id))
        timestamp = self.timestamp
        if not isinstance(timestamp, datetime) or timestamp.tzinfo is None or timestamp.utcoffset() is None:
            raise ValueError("timestamp must be a timezone-aware datetime")
        object.__setattr__(self, "timestamp", timestamp.astimezone(timezone.utc))
        try:
            modality = SensorModality(self.modality)
        except ValueError as exc:
            raise ValueError(f"unsupported modality: {self.modality!r}") from exc
        object.__setattr__(self, "modality", modality)
        object.__setattr__(
            self,
            "detection_probability",
            _probability("detection_probability", self.detection_probability),
        )
        object.__setattr__(
            self, "confidence_level", _probability("confidence_level", self.confidence_level)
        )
        if self.target_track_id is not None:
            object.__setattr__(
                self, "target_track_id", _identifier("target_track_id", self.target_track_id)
            )
        if self.frequency_hz is not None:
            frequency = float(self.frequency_hz)
            if not math.isfinite(frequency) or frequency <= 0.0:
                raise ValueError("frequency_hz must be finite and > 0")
            object.__setattr__(self, "frequency_hz", frequency)
        if self.position_ecef_m is not None:
            if len(self.position_ecef_m) != 3:
                raise ValueError("position_ecef_m must have three components")
            position = tuple(float(component) for component in self.position_ecef_m)
            if not all(math.isfinite(component) for component in position):
                raise ValueError("position_ecef_m must be finite")
            object.__setattr__(self, "position_ecef_m", position)
        if self.model_id is not None:
            object.__setattr__(self, "model_id", _identifier("model_id", self.model_id))
        if not isinstance(self.measurements, Mapping):
            raise TypeError("measurements must be a mapping")
        measurements = dict(self.measurements)
        try:
            json.dumps(measurements, allow_nan=False)
        except (TypeError, ValueError) as exc:
            raise ValueError("measurements must contain JSON-safe finite values") from exc
        object.__setattr__(self, "measurements", measurements)
        object.__setattr__(self, "schema_version", _identifier("schema_version", self.schema_version))

    def to_dict(self) -> dict[str, Any]:
        timestamp = self.timestamp.isoformat(timespec="microseconds").replace("+00:00", "Z")
        return {
            "schema_version": self.schema_version,
            "observation_id": self.observation_id,
            "timestamp": timestamp,
            "node_id": self.node_id,
            "modality": self.modality.value,
            "detection_probability": self.detection_probability,
            "confidence_level": self.confidence_level,
            "target_track_id": self.target_track_id,
            "frequency_hz": self.frequency_hz,
            "position_ecef_m": (
                list(self.position_ecef_m) if self.position_ecef_m is not None else None
            ),
            "model_id": self.model_id,
            "measurements": dict(self.measurements),
        }

    def to_json(self) -> str:
        return json.dumps(self.to_dict(), separators=(",", ":"), allow_nan=False)

    @classmethod
    def from_dict(cls, value: Mapping[str, Any]) -> "SensorObservation":
        item = dict(value)
        timestamp_raw = item.get("timestamp")
        if not isinstance(timestamp_raw, str):
            raise ValueError("timestamp must be an ISO-8601 string")
        normalized = timestamp_raw[:-1] + "+00:00" if timestamp_raw.endswith("Z") else timestamp_raw
        try:
            timestamp = datetime.fromisoformat(normalized)
        except ValueError as exc:
            raise ValueError("timestamp must be a valid ISO-8601 string") from exc
        required = (
            "observation_id",
            "node_id",
            "modality",
            "detection_probability",
            "confidence_level",
        )
        missing = [name for name in required if name not in item]
        if missing:
            raise ValueError(f"observation is missing required fields: {', '.join(missing)}")
        position = item.get("position_ecef_m")
        return cls(
            observation_id=item["observation_id"],
            timestamp=timestamp,
            node_id=item["node_id"],
            modality=SensorModality(item["modality"]),
            detection_probability=item["detection_probability"],
            confidence_level=item["confidence_level"],
            target_track_id=item.get("target_track_id"),
            frequency_hz=item.get("frequency_hz"),
            position_ecef_m=tuple(position) if position is not None else None,
            model_id=item.get("model_id"),
            measurements=item.get("measurements", {}),
            schema_version=item.get("schema_version", "1.0"),
        )
