"""Validated, dependency-free configuration loading for the RF simulation."""

from __future__ import annotations

from dataclasses import asdict, dataclass, field
import json
import math
from pathlib import Path
import tomllib
from typing import Any, Mapping

from .geodesy import Geodetic


def _finite(name: str, value: Any) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise TypeError(f"{name} must be a number")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"{name} must be finite")
    return result


def _positive(name: str, value: Any) -> float:
    result = _finite(name, value)
    if result <= 0.0:
        raise ValueError(f"{name} must be > 0")
    return result


def _mapping(name: str, value: Any) -> Mapping[str, Any]:
    if not isinstance(value, Mapping):
        raise TypeError(f"{name} must be an object/table")
    return value


def _nonempty_id(name: str, value: Any) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{name} must be a non-empty string")
    return value.strip()


def _location(value: Any, owner: str) -> Geodetic:
    item = _mapping(f"{owner}.location", value)
    try:
        return Geodetic(
            latitude_deg=_finite("latitude_deg", item["latitude_deg"]),
            longitude_deg=_finite("longitude_deg", item["longitude_deg"]),
            altitude_m=_finite("altitude_m", item.get("altitude_m", 0.0)),
        )
    except KeyError as exc:
        raise ValueError(f"{owner}.location is missing {exc.args[0]!r}") from exc


@dataclass(frozen=True, slots=True)
class RFChannelConfig:
    """One simulated RF receiver channel on a sensor node."""

    center_frequency_hz: float
    bandwidth_hz: float
    noise_figure_db: float = 5.0
    receive_gain_dbi: float = 0.0

    def __post_init__(self) -> None:
        object.__setattr__(
            self,
            "center_frequency_hz",
            _positive("center_frequency_hz", self.center_frequency_hz),
        )
        object.__setattr__(self, "bandwidth_hz", _positive("bandwidth_hz", self.bandwidth_hz))
        object.__setattr__(
            self, "noise_figure_db", _finite("noise_figure_db", self.noise_figure_db)
        )
        object.__setattr__(
            self, "receive_gain_dbi", _finite("receive_gain_dbi", self.receive_gain_dbi)
        )

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "RFChannelConfig":
        item = _mapping("rf_channel", value)
        try:
            return cls(
                center_frequency_hz=item["center_frequency_hz"],
                bandwidth_hz=item["bandwidth_hz"],
                noise_figure_db=item.get("noise_figure_db", 5.0),
                receive_gain_dbi=item.get("receive_gain_dbi", 0.0),
            )
        except KeyError as exc:
            raise ValueError(f"rf_channel is missing {exc.args[0]!r}") from exc


@dataclass(frozen=True, slots=True)
class SensorNodeConfig:
    """Fixed simulated node containing RF and optional imaging modalities."""

    node_id: str
    location: Geodetic
    rf_channels: tuple[RFChannelConfig, ...]
    modalities: tuple[str, ...] = ("rf", "rgbd", "event_camera")
    metadata: Mapping[str, Any] = field(default_factory=dict)

    def __post_init__(self) -> None:
        object.__setattr__(self, "node_id", _nonempty_id("node_id", self.node_id))
        if not isinstance(self.location, Geodetic):
            raise TypeError("location must be Geodetic")
        channels = tuple(self.rf_channels)
        if not channels:
            raise ValueError("rf_channels must contain at least one channel")
        if not all(isinstance(channel, RFChannelConfig) for channel in channels):
            raise TypeError("rf_channels must contain RFChannelConfig values")
        object.__setattr__(self, "rf_channels", channels)
        modalities = tuple(_nonempty_id("modality", item) for item in self.modalities)
        if len(set(modalities)) != len(modalities):
            raise ValueError("modalities must be unique")
        object.__setattr__(self, "modalities", modalities)
        if not isinstance(self.metadata, Mapping):
            raise TypeError("metadata must be a mapping")

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "SensorNodeConfig":
        item = _mapping("sensor_node", value)
        try:
            raw_channels = item["rf_channels"]
            if isinstance(raw_channels, (str, bytes)) or not isinstance(raw_channels, list):
                raise TypeError("sensor_node.rf_channels must be an array")
            return cls(
                node_id=item["node_id"],
                location=_location(item["location"], "sensor_node"),
                rf_channels=tuple(RFChannelConfig.from_mapping(channel) for channel in raw_channels),
                modalities=tuple(item.get("modalities", ("rf", "rgbd", "event_camera"))),
                metadata=_mapping("sensor_node.metadata", item.get("metadata", {})),
            )
        except KeyError as exc:
            raise ValueError(f"sensor_node is missing {exc.args[0]!r}") from exc


@dataclass(frozen=True, slots=True)
class DroneRFEmitterConfig:
    """A non-physical simulated RF emitter attached to a drone actor."""

    emitter_id: str
    location: Geodetic
    center_frequency_hz: float
    transmit_power_dbm: float
    bandwidth_hz: float
    transmit_gain_dbi: float = 0.0
    duty_cycle: float = 1.0
    waveform: str = "qpsk_placeholder"
    metadata: Mapping[str, Any] = field(default_factory=dict)

    def __post_init__(self) -> None:
        object.__setattr__(self, "emitter_id", _nonempty_id("emitter_id", self.emitter_id))
        if not isinstance(self.location, Geodetic):
            raise TypeError("location must be Geodetic")
        object.__setattr__(
            self,
            "center_frequency_hz",
            _positive("center_frequency_hz", self.center_frequency_hz),
        )
        object.__setattr__(
            self, "transmit_power_dbm", _finite("transmit_power_dbm", self.transmit_power_dbm)
        )
        object.__setattr__(self, "bandwidth_hz", _positive("bandwidth_hz", self.bandwidth_hz))
        object.__setattr__(
            self, "transmit_gain_dbi", _finite("transmit_gain_dbi", self.transmit_gain_dbi)
        )
        duty_cycle = _finite("duty_cycle", self.duty_cycle)
        if not 0.0 <= duty_cycle <= 1.0:
            raise ValueError("duty_cycle must be in [0, 1]")
        object.__setattr__(self, "duty_cycle", duty_cycle)
        object.__setattr__(self, "waveform", _nonempty_id("waveform", self.waveform))
        if not isinstance(self.metadata, Mapping):
            raise TypeError("metadata must be a mapping")

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "DroneRFEmitterConfig":
        item = _mapping("drone_emitter", value)
        try:
            return cls(
                emitter_id=item["emitter_id"],
                location=_location(item["location"], "drone_emitter"),
                center_frequency_hz=item["center_frequency_hz"],
                transmit_power_dbm=item["transmit_power_dbm"],
                bandwidth_hz=item["bandwidth_hz"],
                transmit_gain_dbi=item.get("transmit_gain_dbi", 0.0),
                duty_cycle=item.get("duty_cycle", 1.0),
                waveform=item.get("waveform", "qpsk_placeholder"),
                metadata=_mapping("drone_emitter.metadata", item.get("metadata", {})),
            )
        except KeyError as exc:
            raise ValueError(f"drone_emitter is missing {exc.args[0]!r}") from exc


@dataclass(frozen=True, slots=True)
class SimulationConfig:
    """Top-level immutable configuration used by an integration adapter."""

    sensor_nodes: tuple[SensorNodeConfig, ...]
    drone_emitters: tuple[DroneRFEmitterConfig, ...]
    random_seed: int = 0

    def __post_init__(self) -> None:
        nodes = tuple(self.sensor_nodes)
        emitters = tuple(self.drone_emitters)
        if not all(isinstance(node, SensorNodeConfig) for node in nodes):
            raise TypeError("sensor_nodes must contain SensorNodeConfig values")
        if not all(isinstance(emitter, DroneRFEmitterConfig) for emitter in emitters):
            raise TypeError("drone_emitters must contain DroneRFEmitterConfig values")
        node_ids = [node.node_id for node in nodes]
        emitter_ids = [emitter.emitter_id for emitter in emitters]
        if len(set(node_ids)) != len(node_ids):
            raise ValueError("sensor node IDs must be unique")
        if len(set(emitter_ids)) != len(emitter_ids):
            raise ValueError("drone emitter IDs must be unique")
        if isinstance(self.random_seed, bool) or not isinstance(self.random_seed, int):
            raise TypeError("random_seed must be an integer")
        object.__setattr__(self, "sensor_nodes", nodes)
        object.__setattr__(self, "drone_emitters", emitters)

    @classmethod
    def from_mapping(cls, value: Mapping[str, Any]) -> "SimulationConfig":
        item = _mapping("simulation_config", value)
        raw_nodes = item.get("sensor_nodes", [])
        raw_emitters = item.get("drone_emitters", [])
        if not isinstance(raw_nodes, list):
            raise TypeError("sensor_nodes must be an array")
        if not isinstance(raw_emitters, list):
            raise TypeError("drone_emitters must be an array")
        return cls(
            sensor_nodes=tuple(SensorNodeConfig.from_mapping(node) for node in raw_nodes),
            drone_emitters=tuple(
                DroneRFEmitterConfig.from_mapping(emitter) for emitter in raw_emitters
            ),
            random_seed=item.get("random_seed", 0),
        )

    def to_dict(self) -> dict[str, Any]:
        """Return a JSON-serializable snapshot."""

        return asdict(self)


def load_simulation_config(path: str | Path) -> SimulationConfig:
    """Load a :class:`SimulationConfig` from UTF-8 JSON or binary TOML."""

    config_path = Path(path)
    suffix = config_path.suffix.lower()
    if suffix == ".json":
        raw = json.loads(config_path.read_text(encoding="utf-8"))
    elif suffix == ".toml":
        with config_path.open("rb") as handle:
            raw = tomllib.load(handle)
    else:
        raise ValueError("configuration file must use .json or .toml")
    return SimulationConfig.from_mapping(_mapping("simulation_config", raw))
