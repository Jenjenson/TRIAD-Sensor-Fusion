"""Model-free wideband passive-RF detection for the TRIAD simulation.

The detector is deliberately classical and auditable: channelized received
power, a kTB noise floor, an analytic radiometer threshold, deterministic
shadowing, and a persistence gate.  It never opens or scores either of the
user-supplied RF checkpoints.

This module models detection only.  It does not decode, interfere with, or
transmit on any radio service.
"""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import math
from typing import Iterable

from .rf import (
    DeterministicShadowing,
    LinkContext,
    analytic_energy_detector_probability,
    fspl_db,
    received_power_dbm,
    thermal_noise_dbm,
)


WIDEBAND_RF_METHOD = "channelized_radiometer_adaptive_threshold_v1"


def _finite(name: str, value: float) -> float:
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"{name} must be finite")
    return result


def _positive(name: str, value: float) -> float:
    result = _finite(name, value)
    if result <= 0.0:
        raise ValueError(f"{name} must be > 0")
    return result


def _unit(name: str, value: float) -> float:
    result = _finite(name, value)
    if not 0.0 <= result <= 1.0:
        raise ValueError(f"{name} must be in [0, 1]")
    return result


@dataclass(frozen=True, slots=True)
class WidebandChannel:
    center_frequency_hz: float
    bandwidth_hz: float = 2_000_000.0
    dwell_time_ms: float = 8.0
    false_alarm_probability: float = 1e-3

    def __post_init__(self) -> None:
        object.__setattr__(
            self, "center_frequency_hz", _positive("center_frequency_hz", self.center_frequency_hz)
        )
        object.__setattr__(self, "bandwidth_hz", _positive("bandwidth_hz", self.bandwidth_hz))
        object.__setattr__(self, "dwell_time_ms", _positive("dwell_time_ms", self.dwell_time_ms))
        pfa = _finite("false_alarm_probability", self.false_alarm_probability)
        if not 0.0 < pfa < 1.0:
            raise ValueError("false_alarm_probability must be in (0, 1)")
        object.__setattr__(self, "false_alarm_probability", pfa)


@dataclass(frozen=True, slots=True)
class WidebandReceiver:
    receiver_id: str
    channels: tuple[WidebandChannel, ...]
    receive_gain_dbi: float = 14.0
    noise_figure_db: float = 5.0
    system_loss_db: float = 2.0
    minimum_snr_db: float = 6.0
    minimum_probability_detection: float = 0.90
    maximum_range_m: float = 30_000.0
    temporal_window_ms: float = 250.0
    temporal_epoch_count: int = 5
    minimum_active_epochs: int = 2
    processing_latency_ms: float = 6.0
    network_latency_ms: float = 12.0

    def __post_init__(self) -> None:
        if not isinstance(self.receiver_id, str) or not self.receiver_id.strip():
            raise ValueError("receiver_id must be non-empty")
        if not self.channels or not all(isinstance(item, WidebandChannel) for item in self.channels):
            raise ValueError("channels must contain at least one WidebandChannel")
        object.__setattr__(self, "receive_gain_dbi", _finite("receive_gain_dbi", self.receive_gain_dbi))
        object.__setattr__(self, "noise_figure_db", _finite("noise_figure_db", self.noise_figure_db))
        object.__setattr__(self, "system_loss_db", _positive("system_loss_db", self.system_loss_db))
        object.__setattr__(self, "minimum_snr_db", _finite("minimum_snr_db", self.minimum_snr_db))
        object.__setattr__(
            self,
            "minimum_probability_detection",
            _unit("minimum_probability_detection", self.minimum_probability_detection),
        )
        object.__setattr__(self, "maximum_range_m", _positive("maximum_range_m", self.maximum_range_m))
        object.__setattr__(
            self, "temporal_window_ms", _positive("temporal_window_ms", self.temporal_window_ms)
        )
        if not 1 <= self.temporal_epoch_count <= 32:
            raise ValueError("temporal_epoch_count must be in [1, 32]")
        if not 1 <= self.minimum_active_epochs <= self.temporal_epoch_count:
            raise ValueError("minimum_active_epochs must be between 1 and temporal_epoch_count")
        object.__setattr__(
            self, "processing_latency_ms", _positive("processing_latency_ms", self.processing_latency_ms)
        )
        object.__setattr__(
            self, "network_latency_ms", _positive("network_latency_ms", self.network_latency_ms)
        )

    @property
    def scan_latency_ms(self) -> float:
        return max(
            sum(item.dwell_time_ms for item in self.channels),
            self.temporal_window_ms,
        )


@dataclass(frozen=True, slots=True)
class RFEmitterProfile:
    emitter_id: str
    center_frequencies_hz: tuple[float, ...]
    transmit_power_dbm: float = 23.0
    transmit_gain_dbi: float = 2.0
    duty_cycle: float = 0.65
    channel_tolerance_hz: float = 10_000_000.0

    def __post_init__(self) -> None:
        if not isinstance(self.emitter_id, str) or not self.emitter_id.strip():
            raise ValueError("emitter_id must be non-empty")
        frequencies = tuple(_positive("center_frequency_hz", item) for item in self.center_frequencies_hz)
        object.__setattr__(self, "center_frequencies_hz", frequencies)
        object.__setattr__(self, "transmit_power_dbm", _finite("transmit_power_dbm", self.transmit_power_dbm))
        object.__setattr__(self, "transmit_gain_dbi", _finite("transmit_gain_dbi", self.transmit_gain_dbi))
        object.__setattr__(self, "duty_cycle", _unit("duty_cycle", self.duty_cycle))
        object.__setattr__(
            self, "channel_tolerance_hz", _positive("channel_tolerance_hz", self.channel_tolerance_hz)
        )


@dataclass(frozen=True, slots=True)
class WidebandRFDetection:
    target_id: str
    receiver_id: str
    center_frequency_hz: float
    range_m: float
    received_power_dbm: float
    noise_floor_dbm: float
    snr_db: float
    probability_detection: float
    detected: bool
    emitting_during_scan: bool
    path_loss_db: float
    weather_loss_db: float
    scan_latency_ms: float
    end_to_end_latency_ms: float
    temporal_epoch_count: int
    active_epoch_count: int
    minimum_active_epochs: int
    method: str = WIDEBAND_RF_METHOD
    supplied_rf_models_used: bool = False

    def to_dict(self) -> dict[str, object]:
        return {
            "targetId": self.target_id,
            "receiverId": self.receiver_id,
            "centerFrequencyHz": round(self.center_frequency_hz, 3),
            "centerFrequencyGHz": round(self.center_frequency_hz / 1e9, 6),
            "rangeMeters": round(self.range_m, 3),
            "receivedPowerDbm": round(self.received_power_dbm, 3),
            "noiseFloorDbm": round(self.noise_floor_dbm, 3),
            "snrDb": round(self.snr_db, 3),
            "probabilityDetectionAnalytic": round(self.probability_detection, 6),
            "detected": self.detected,
            "emittingDuringScan": self.emitting_during_scan,
            "freeSpacePathLossDb": round(self.path_loss_db, 3),
            "weatherLossDb": round(self.weather_loss_db, 4),
            "scanLatencyMilliseconds": round(self.scan_latency_ms, 3),
            "endToEndLatencyMilliseconds": round(self.end_to_end_latency_ms, 3),
            "temporalEpochCount": self.temporal_epoch_count,
            "activeEpochCount": self.active_epoch_count,
            "minimumActiveEpochs": self.minimum_active_epochs,
            "method": self.method,
            "suppliedRFModelsUsed": self.supplied_rf_models_used,
            "scoreSemantics": "analytic radiometer detection estimate; not a learned or calibrated probability",
        }


def _stable_uniform(*values: object) -> float:
    payload = "\0".join(str(value) for value in values).encode("utf-8")
    digest = hashlib.blake2b(payload, digest_size=8, person=b"triad-rf-v1").digest()
    return (int.from_bytes(digest, "big") + 0.5) / float(1 << 64)


def _active_epoch_count(
    profile: RFEmitterProfile,
    timestamp_s: float,
    channel_hz: float,
    epoch_count: int,
) -> int:
    if profile.duty_cycle <= 0.0:
        return 0
    if profile.duty_cycle >= 1.0:
        return epoch_count
    # Five 50 ms epochs form the default 250 ms persistence window. The result
    # is deterministic and call-order independent for repeatable audits.
    bucket = math.floor(timestamp_s * 20.0)
    return sum(
        _stable_uniform(profile.emitter_id, round(channel_hz), bucket - offset)
        < profile.duty_cycle
        for offset in range(epoch_count)
    )


def rf_rain_specific_attenuation_db_per_km(frequency_hz: float, rain_rate_mm_h: float) -> float:
    """Conservative bounded RF weather stress term for 0.3-6 GHz simulation.

    This is an explicit scenario approximation, not an implementation of an
    ITU-R recommendation.  Its purpose is to avoid claiming that rain is zero
    while keeping low-GHz loss appropriately secondary to geometry/obstruction.
    """

    frequency_ghz = _positive("frequency_hz", frequency_hz) / 1e9
    rain_rate = max(0.0, _finite("rain_rate_mm_h", rain_rate_mm_h))
    return min(0.12, 0.00005 * frequency_ghz**1.35 * rain_rate**0.9)


def simulate_wideband_link(
    *,
    target_id: str,
    receiver: WidebandReceiver,
    emitter: RFEmitterProfile,
    range_m: float,
    timestamp_s: float,
    rain_rate_mm_h: float = 0.0,
    line_of_sight: bool = True,
    shadowing: DeterministicShadowing | None = None,
    num_complex_samples: int = 256,
) -> tuple[WidebandRFDetection, ...]:
    """Evaluate matching emitter/receiver channels for one target-node link."""

    distance = _positive("range_m", range_m)
    timestamp = _finite("timestamp_s", timestamp_s)
    results: list[WidebandRFDetection] = []
    if not emitter.center_frequencies_hz:
        return ()

    for emitter_frequency in emitter.center_frequencies_hz:
        matching = min(
            receiver.channels,
            key=lambda item: abs(item.center_frequency_hz - emitter_frequency),
        )
        if abs(matching.center_frequency_hz - emitter_frequency) > emitter.channel_tolerance_hz:
            continue

        active_epoch_count = _active_epoch_count(
            emitter,
            timestamp,
            emitter_frequency,
            receiver.temporal_epoch_count,
        )
        emitting = active_epoch_count >= receiver.minimum_active_epochs
        context = LinkContext(
            transmitter_id=target_id,
            receiver_id=receiver.receiver_id,
            distance_m=distance,
            frequency_hz=emitter_frequency,
            timestamp_s=timestamp,
            line_of_sight=line_of_sight,
        )
        path_loss = fspl_db(distance, emitter_frequency)
        weather_specific = rf_rain_specific_attenuation_db_per_km(
            emitter_frequency, rain_rate_mm_h
        )
        weather_loss = weather_specific * (distance / 1000.0)
        obstruction_loss = 0.0 if line_of_sight else 18.0
        power = received_power_dbm(
            emitter.transmit_power_dbm,
            context,
            transmit_gain_dbi=emitter.transmit_gain_dbi,
            receive_gain_dbi=receiver.receive_gain_dbi,
            path_loss_db=path_loss + weather_loss + obstruction_loss,
            shadowing=shadowing,
            system_loss_db=receiver.system_loss_db,
        )
        noise = thermal_noise_dbm(
            matching.bandwidth_hz, noise_figure_db=receiver.noise_figure_db
        )
        snr = power - noise if emitting else -120.0
        estimate = analytic_energy_detector_probability(
            snr,
            num_complex_samples,
            false_alarm_probability=matching.false_alarm_probability,
        )
        detected = (
            emitting
            and distance <= receiver.maximum_range_m
            and snr >= receiver.minimum_snr_db
            and estimate.probability_detection >= receiver.minimum_probability_detection
        )
        scan_latency = receiver.scan_latency_ms
        results.append(
            WidebandRFDetection(
                target_id=target_id,
                receiver_id=receiver.receiver_id,
                center_frequency_hz=emitter_frequency,
                range_m=distance,
                received_power_dbm=power,
                noise_floor_dbm=noise,
                snr_db=snr,
                probability_detection=estimate.probability_detection,
                detected=detected,
                emitting_during_scan=emitting,
                path_loss_db=path_loss,
                weather_loss_db=weather_loss,
                scan_latency_ms=scan_latency,
                end_to_end_latency_ms=(
                    scan_latency + receiver.processing_latency_ms + receiver.network_latency_ms
                ),
                temporal_epoch_count=receiver.temporal_epoch_count,
                active_epoch_count=active_epoch_count,
                minimum_active_epochs=receiver.minimum_active_epochs,
            )
        )
    return tuple(results)


def detected_frequency_labels(detections: Iterable[WidebandRFDetection]) -> tuple[str, ...]:
    frequencies = sorted({item.center_frequency_hz for item in detections if item.detected})
    return tuple(
        f"{frequency / 1e9:.3f} GHz" if frequency >= 1e9 else f"{frequency / 1e6:.2f} MHz"
        for frequency in frequencies
    )
