"""Deterministic audit harness for the simulation-only layered detector.

The harness exercises the classical wideband RF and mmWave equations, the
track-centric fusion policy, and the end-to-end layered snapshot builder.  It
does not load supplied RF checkpoints, run engagement logic, or claim that a
mathematical result is calibrated field performance.

Running this module writes a JSON evidence record and a human-readable
Markdown summary.  Importing it has no filesystem side effects.
"""

from __future__ import annotations

import argparse
from collections.abc import Callable, Iterable, Mapping, Sequence
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import time
from typing import Any

from .fusion_v3 import LayeredEvidence, LayeredModality, fuse_layered_evidence
from .geodesy import ENU, Geodetic, enu_to_geodetic, geodetic_to_enu
from .layered_runtime import (
    WIDEBAND_CHANNELS,
    build_layered_snapshot,
)
from .mmwave import MmWaveDetection, MmWaveRadar, simulate_mmwave_detection
from .paths import DEFAULT_OUTPUT_DIRECTORY as PORTABLE_OUTPUT_DIRECTORY
from .wideband_rf import (
    RFEmitterProfile,
    WidebandRFDetection,
    WidebandReceiver,
    simulate_wideband_link,
)


AUDIT_SCHEMA = "triad.layered_sensor_audit.v1"
AUDIT_TIME = datetime(2026, 8, 3, 4, 0, tzinfo=timezone.utc)
AUDIT_TIME_TEXT = "2026-08-03T04:00:00.000Z"
DEFAULT_OUTPUT_DIRECTORY = PORTABLE_OUTPUT_DIRECTORY
DEFAULT_JSON_NAME = "layered_sensor_audit.json"
DEFAULT_MARKDOWN_NAME = "layered_sensor_audit.md"

RF_DISTANCES_KM = (0.1, 0.3, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 30.0)
MMWAVE_DISTANCES_M = (50.0, 100.0, 200.0, 300.0, 400.0, 500.0, 700.0, 1000.0)
SPEEDS_M_S = (0.0, 5.0, 20.0, 50.0, 100.0, 300.0)
RAIN_RATES_MM_H = (0.0, 10.0, 25.0, 50.0, 100.0)
SWARM_SIZES = (1, 8, 24, 64, 100)
TIME_TO_PERIMETER_DISTANCE_M = 5_000.0

RF_AUDIT_FREQUENCIES_HZ = (
    433.92e6,
    915.0e6,
    1.28e9,
    2.437e9,
    5.795e9,
    6.2e9,
)
PRIMARY_24_58_CHANNELS_HZ = frozenset((2.437e9, 5.795e9))


def _round(value: float, digits: int = 6) -> float:
    return round(float(value), digits)


def _nearest_rank(values: Iterable[float], quantile: float) -> float | None:
    ordered = sorted(float(value) for value in values if math.isfinite(float(value)))
    if not ordered:
        return None
    index = max(0, min(len(ordered) - 1, math.ceil(quantile * len(ordered)) - 1))
    return _round(ordered[index], 3)


def _distribution(values: Iterable[float]) -> dict[str, float | int | None]:
    materialized = tuple(float(value) for value in values if math.isfinite(float(value)))
    return {
        "sampleCount": len(materialized),
        "minimum": _round(min(materialized), 3) if materialized else None,
        "p50": _nearest_rank(materialized, 0.50),
        "p95": _nearest_rank(materialized, 0.95),
        "p99": _nearest_rank(materialized, 0.99),
        "maximum": _round(max(materialized), 3) if materialized else None,
    }


def _timed(clock: Callable[[], float], operation: Callable[[], Any]) -> tuple[Any, float]:
    started = clock()
    value = operation()
    elapsed_ms = max(0.0, (clock() - started) * 1000.0)
    return value, _round(elapsed_ms, 3)


def _wideband_receiver(receiver_id: str) -> WidebandReceiver:
    return WidebandReceiver(
        receiver_id=receiver_id,
        channels=WIDEBAND_CHANNELS,
        maximum_range_m=30_000.0,
    )


def _audit_emitter(target_id: str = "audit-emitter") -> RFEmitterProfile:
    return RFEmitterProfile(
        emitter_id=target_id,
        center_frequencies_hz=RF_AUDIT_FREQUENCIES_HZ,
        transmit_power_dbm=23.0,
        duty_cycle=1.0,
    )


def _rf_results(
    *,
    target_id: str,
    receiver_id: str,
    distance_m: float,
    rain_rate_mm_h: float = 0.0,
) -> tuple[WidebandRFDetection, ...]:
    return simulate_wideband_link(
        target_id=target_id,
        receiver=_wideband_receiver(receiver_id),
        emitter=_audit_emitter(target_id + ":emitter"),
        range_m=distance_m,
        timestamp_s=AUDIT_TIME.timestamp(),
        rain_rate_mm_h=rain_rate_mm_h,
        line_of_sight=True,
        shadowing=None,
    )


def _rf_evidence(
    results: Iterable[WidebandRFDetection],
    *,
    target_id: str,
    node_id: str,
) -> LayeredEvidence | None:
    detected = [item for item in results if item.detected]
    if not detected:
        return None
    strongest = max(detected, key=lambda item: (item.probability_detection, item.snr_db))
    return LayeredEvidence(
        evidence_id=f"audit-rf:{node_id}:{target_id}",
        target_track_id=target_id,
        node_id=node_id,
        modality=LayeredModality.WIDEBAND_RF,
        timestamp=AUDIT_TIME,
        raw_score=strongest.probability_detection,
        source_reliability=0.82,
        weather_multiplier=1.0,
        latency_ms=strongest.end_to_end_latency_ms,
        range_m=strongest.range_m,
        correlation_group=f"rf:{node_id}",
        score_origin=strongest.method,
    )


def _mmwave_result(
    *,
    target_id: str,
    distance_m: float,
    speed_m_s: float = 20.0,
    rain_rate_mm_h: float = 0.0,
    radar_id: str = "audit-mmwave",
) -> MmWaveDetection:
    return simulate_mmwave_detection(
        target_id=target_id,
        radar=MmWaveRadar(radar_id),
        east_m=distance_m,
        north_m=0.0,
        up_m=0.0,
        target_speed_m_s=speed_m_s,
        target_heading_deg=270.0,
        radar_cross_section_m2=0.02,
        rain_rate_mm_h=rain_rate_mm_h,
        line_of_sight=True,
    )


def _mmwave_evidence(result: MmWaveDetection, *, target_id: str) -> LayeredEvidence | None:
    if not result.detected:
        return None
    return LayeredEvidence(
        evidence_id=f"audit-mmwave:{result.radar_id}:{target_id}",
        target_track_id=target_id,
        node_id=result.radar_id,
        modality=LayeredModality.MMWAVE,
        timestamp=AUDIT_TIME,
        raw_score=result.probability_detection_index,
        source_reliability=0.88,
        weather_multiplier=1.0,
        latency_ms=result.end_to_end_latency_ms,
        range_m=result.range_m,
        correlation_group=f"mmwave:{result.radar_id}",
        score_origin=result.method,
    )


def _visual_evidence(
    target_id: str,
    modality: LayeredModality,
    *,
    node_id: str,
) -> LayeredEvidence:
    if modality not in (LayeredModality.RGB, LayeredModality.EVENT_CAMERA):
        raise ValueError("visual audit evidence must be RGB or event-camera")
    latency = 95.0 if modality is LayeredModality.RGB else 45.0
    reliability = 0.90 if modality is LayeredModality.RGB else 0.84
    return LayeredEvidence(
        evidence_id=f"audit-{modality.value}:{node_id}:{target_id}",
        target_track_id=target_id,
        node_id=node_id,
        modality=modality,
        timestamp=AUDIT_TIME,
        raw_score=0.92,
        source_reliability=reliability,
        weather_multiplier=1.0,
        latency_ms=latency,
        range_m=400.0 if modality is LayeredModality.RGB else None,
        correlation_group=f"{modality.value}:{node_id}",
        score_origin="deterministic_post_association_audit_fixture",
    )


def _no_evidence_decision() -> dict[str, object]:
    return {
        "decision": "NO_CURRENT_EVIDENCE",
        "detectionAlert": False,
        "preliminaryCue": False,
        "operatorCueActive": False,
        "confirmationTier": "UNCONFIRMED",
        "activeModalityFamilies": [],
        "activeModalityFamilyCount": 0,
        "decisionLatencyMilliseconds": None,
        "detectionOnly": True,
        "engagementLogicPresent": False,
    }


def _fuse(evidence: Iterable[LayeredEvidence]) -> dict[str, object]:
    items = tuple(item for item in evidence if item is not None)
    return fuse_layered_evidence(items, as_of=AUDIT_TIME) if items else _no_evidence_decision()


def audit_rf_distance_sweep(clock: Callable[[], float]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for distance_km in RF_DISTANCES_KM:
        target_id = f"rf-distance-{distance_km:g}km"

        def evaluate() -> tuple[tuple[WidebandRFDetection, ...], tuple[WidebandRFDetection, ...]]:
            return (
                _rf_results(
                    target_id=target_id,
                    receiver_id="rf-west",
                    distance_m=distance_km * 1000.0,
                ),
                _rf_results(
                    target_id=target_id,
                    receiver_id="rf-east",
                    distance_m=distance_km * 1000.0,
                ),
            )

        (west, east), elapsed_ms = _timed(clock, evaluate)
        west_detected = [item for item in west if item.detected]
        east_detected = [item for item in east if item.detected]
        fusion = _fuse(
            item
            for item in (
                _rf_evidence(west, target_id=target_id, node_id="rf-west"),
                _rf_evidence(east, target_id=target_id, node_id="rf-east"),
            )
            if item is not None
        )
        beyond_primary = [
            item
            for item in west_detected
            if item.center_frequency_hz not in PRIMARY_24_58_CHANNELS_HZ
        ]
        rows.append(
            {
                "distanceKilometers": distance_km,
                "channelsEvaluatedPerReceiver": len(west),
                "detectedChannelsAtOneReceiver": len(west_detected),
                "detectedLinksAcrossTwoReceivers": len(west_detected) + len(east_detected),
                "detectingReceiverCount": int(bool(west_detected)) + int(bool(east_detected)),
                "detectedBeyondPrimary24And58GHzChannelCount": len(beyond_primary),
                "detectedFrequenciesGHz": [
                    _round(item.center_frequency_hz / 1e9, 6) for item in west_detected
                ],
                "minimumDetectedSnrDb": _round(
                    min((item.snr_db for item in west_detected), default=-120.0), 3
                ),
                "maximumEvaluatedSnrDb": _round(max(item.snr_db for item in west), 3),
                "preliminaryCueCount": int(bool(fusion.get("preliminaryCue"))),
                "operatorCueCount": int(bool(fusion.get("operatorCueActive"))),
                "fusionDecision": fusion["decision"],
                "modeledDecisionLatencyMilliseconds": fusion.get(
                    "decisionLatencyMilliseconds"
                ),
                "processingRuntimeMilliseconds": elapsed_ms,
                "suppliedRFModelsUsed": False,
            }
        )
    return rows


def audit_mmwave_distance_sweep(clock: Callable[[], float]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for distance_m in MMWAVE_DISTANCES_M:
        target_id = f"mmwave-distance-{distance_m:g}m"

        def evaluate() -> tuple[MmWaveDetection, tuple[WidebandRFDetection, ...], tuple[WidebandRFDetection, ...]]:
            return (
                _mmwave_result(target_id=target_id, distance_m=distance_m),
                _rf_results(
                    target_id=target_id,
                    receiver_id="rf-west",
                    distance_m=distance_m,
                ),
                _rf_results(
                    target_id=target_id,
                    receiver_id="rf-east",
                    distance_m=distance_m,
                ),
            )

        (radar, west, east), elapsed_ms = _timed(clock, evaluate)
        fusion = _fuse(
            item
            for item in (
                _rf_evidence(west, target_id=target_id, node_id="rf-west"),
                _rf_evidence(east, target_id=target_id, node_id="rf-east"),
                _mmwave_evidence(radar, target_id=target_id),
            )
            if item is not None
        )
        rows.append(
            {
                "distanceMeters": distance_m,
                "mmWaveDetectionCount": int(radar.detected),
                "mmWaveSnrDb": _round(radar.snr_db, 3),
                "mmWaveEvidenceIndex": _round(radar.probability_detection_index, 6),
                "rangeResolutionMeters": _round(radar.range_resolution_m, 6),
                "mmWaveModeledLatencyMilliseconds": radar.end_to_end_latency_ms,
                "corroboratedCueCountWithWidebandRF": int(
                    bool(fusion.get("detectionAlert"))
                ),
                "preliminaryRFCueCountWhenRadarMisses": int(
                    not radar.detected and bool(fusion.get("preliminaryCue"))
                ),
                "operatorCueCount": int(bool(fusion.get("operatorCueActive"))),
                "fusionDecision": fusion["decision"],
                "modeledDecisionLatencyMilliseconds": fusion.get(
                    "decisionLatencyMilliseconds"
                ),
                "processingRuntimeMilliseconds": elapsed_ms,
            }
        )
    return rows


def audit_speed_sweep(clock: Callable[[], float]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for speed_m_s in SPEEDS_M_S:
        target_id = f"speed-{speed_m_s:g}mps"

        def evaluate() -> tuple[MmWaveDetection, tuple[WidebandRFDetection, ...], tuple[WidebandRFDetection, ...]]:
            return (
                _mmwave_result(
                    target_id=target_id,
                    distance_m=300.0,
                    speed_m_s=speed_m_s,
                ),
                _rf_results(
                    target_id=target_id,
                    receiver_id="rf-west",
                    distance_m=300.0,
                ),
                _rf_results(
                    target_id=target_id,
                    receiver_id="rf-east",
                    distance_m=300.0,
                ),
            )

        (radar, west, east), elapsed_ms = _timed(clock, evaluate)
        fusion = _fuse(
            item
            for item in (
                _rf_evidence(west, target_id=target_id, node_id="rf-west"),
                _rf_evidence(east, target_id=target_id, node_id="rf-east"),
                _mmwave_evidence(radar, target_id=target_id),
            )
            if item is not None
        )
        time_to_perimeter = (
            None
            if speed_m_s == 0.0
            else _round(TIME_TO_PERIMETER_DISTANCE_M / speed_m_s, 3)
        )
        rows.append(
            {
                "speedMetersPerSecond": speed_m_s,
                "mmWaveDetectionCount": int(radar.detected),
                "measuredClosingVelocityMetersPerSecond": _round(
                    -radar.radial_velocity_m_s, 3
                ),
                "closingVelocityAbsoluteErrorMetersPerSecond": _round(
                    abs((-radar.radial_velocity_m_s) - speed_m_s), 9
                ),
                "operatorCueCount": int(bool(fusion.get("operatorCueActive"))),
                "fusionDecision": fusion["decision"],
                "modeledDecisionLatencyMilliseconds": fusion.get(
                    "decisionLatencyMilliseconds"
                ),
                "distanceToPerimeterMeters": TIME_TO_PERIMETER_DISTANCE_M,
                "timeToPerimeterSeconds": time_to_perimeter,
                "timeToPerimeterState": "STATIONARY_OR_TANGENTIAL"
                if speed_m_s == 0.0
                else "APPROACHING",
                "processingRuntimeMilliseconds": elapsed_ms,
            }
        )
    return rows


def audit_rain_sweep(clock: Callable[[], float]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for rain_rate in RAIN_RATES_MM_H:
        target_id = f"rain-{rain_rate:g}mmh"

        def evaluate() -> tuple[MmWaveDetection, tuple[WidebandRFDetection, ...], tuple[WidebandRFDetection, ...]]:
            return (
                _mmwave_result(
                    target_id=target_id,
                    distance_m=400.0,
                    rain_rate_mm_h=rain_rate,
                ),
                _rf_results(
                    target_id=target_id,
                    receiver_id="rf-west",
                    distance_m=20_000.0,
                    rain_rate_mm_h=rain_rate,
                ),
                _rf_results(
                    target_id=target_id,
                    receiver_id="rf-east",
                    distance_m=20_000.0,
                    rain_rate_mm_h=rain_rate,
                ),
            )

        (radar, west, east), elapsed_ms = _timed(clock, evaluate)
        fusion = _fuse(
            item
            for item in (
                _rf_evidence(west, target_id=target_id, node_id="rf-west"),
                _rf_evidence(east, target_id=target_id, node_id="rf-east"),
                _mmwave_evidence(radar, target_id=target_id),
            )
            if item is not None
        )
        rows.append(
            {
                "rainRateMillimetersPerHour": rain_rate,
                "widebandDetectedChannelCountAt20Kilometers": sum(
                    item.detected for item in west
                ),
                "widebandWeakestDetectedSnrDb": _round(
                    min((item.snr_db for item in west if item.detected), default=-120.0),
                    3,
                ),
                "mmWaveDetectionCountAt400Meters": int(radar.detected),
                "mmWaveSnrDb": _round(radar.snr_db, 3),
                "mmWaveTwoWayRainLossDb": _round(radar.rain_loss_db, 3),
                "operatorCueCount": int(bool(fusion.get("operatorCueActive"))),
                "corroboratedCueCount": int(bool(fusion.get("detectionAlert"))),
                "preliminaryCueCount": int(bool(fusion.get("preliminaryCue"))),
                "fusionDecision": fusion["decision"],
                "modeledDecisionLatencyMilliseconds": fusion.get(
                    "decisionLatencyMilliseconds"
                ),
                "processingRuntimeMilliseconds": elapsed_ms,
            }
        )
    return rows


def audit_modality_failures(clock: Callable[[], float]) -> list[dict[str, Any]]:
    target_id = "modality-failure-track"
    evidence_by_sensor = {
        "rf-west": LayeredEvidence(
            evidence_id="failure-rf-west",
            target_track_id=target_id,
            node_id="rf-west",
            modality=LayeredModality.WIDEBAND_RF,
            timestamp=AUDIT_TIME,
            raw_score=0.99,
            source_reliability=0.82,
            weather_multiplier=1.0,
            latency_ms=268.0,
            range_m=5_000.0,
            correlation_group="rf:west",
            score_origin="failure_fixture",
        ),
        "rf-east": LayeredEvidence(
            evidence_id="failure-rf-east",
            target_track_id=target_id,
            node_id="rf-east",
            modality=LayeredModality.WIDEBAND_RF,
            timestamp=AUDIT_TIME,
            raw_score=0.99,
            source_reliability=0.82,
            weather_multiplier=1.0,
            latency_ms=268.0,
            range_m=5_100.0,
            correlation_group="rf:east",
            score_origin="failure_fixture",
        ),
        "mmwave": LayeredEvidence(
            evidence_id="failure-mmwave",
            target_track_id=target_id,
            node_id="radar-east",
            modality=LayeredModality.MMWAVE,
            timestamp=AUDIT_TIME,
            raw_score=0.95,
            source_reliability=0.88,
            weather_multiplier=1.0,
            latency_ms=74.0,
            range_m=400.0,
            correlation_group="mmwave:east",
            score_origin="failure_fixture",
        ),
        "rgb": _visual_evidence(target_id, LayeredModality.RGB, node_id="eo-east"),
        "event": _visual_evidence(
            target_id, LayeredModality.EVENT_CAMERA, node_id="event-east"
        ),
    }
    scenarios = (
        ("nominal", ()),
        ("event_camera_offline", ("event",)),
        ("rgb_and_event_offline", ("rgb", "event")),
        ("mmwave_offline", ("mmwave",)),
        ("one_rf_site_offline", ("rf-west",)),
        ("both_rf_sites_offline", ("rf-west", "rf-east")),
        ("all_but_one_rf_offline", ("rf-east", "mmwave", "rgb", "event")),
        ("total_sensor_outage", tuple(evidence_by_sensor)),
    )
    rows: list[dict[str, Any]] = []
    for scenario_id, failed in scenarios:
        available = tuple(
            evidence
            for sensor_id, evidence in evidence_by_sensor.items()
            if sensor_id not in failed
        )
        fusion, elapsed_ms = _timed(clock, lambda: _fuse(available))
        rows.append(
            {
                "scenarioId": scenario_id,
                "failedSensors": list(failed),
                "availableEvidenceCount": len(available),
                "activeModalityFamilyCount": fusion.get("activeModalityFamilyCount", 0),
                "detectionAlertCount": int(bool(fusion.get("detectionAlert"))),
                "preliminaryCueCount": int(bool(fusion.get("preliminaryCue"))),
                "operatorCueCount": int(bool(fusion.get("operatorCueActive"))),
                "fusionDecision": fusion["decision"],
                "modeledDecisionLatencyMilliseconds": fusion.get(
                    "decisionLatencyMilliseconds"
                ),
                "processingRuntimeMilliseconds": elapsed_ms,
            }
        )
    return rows


_PERIMETER_EAST_ORIGIN = Geodetic(1.35, 104.02, 100.0)


def _geodetic_fields(point: Geodetic) -> dict[str, float]:
    return {
        "latitudeDegrees": point.latitude_deg,
        "longitudeDegrees": point.longitude_deg,
        "heightMeters": point.altitude_m,
    }


def _runtime_nodes(disabled_ids: Iterable[str] = ()) -> list[dict[str, Any]]:
    disabled = frozenset(disabled_ids)
    specifications = (
        ("East_A", 0.0, 0.0, True),
        ("East_B", 0.0, 200.0, True),
        ("North_East", -8_000.0, 8_000.0, False),
        ("South_East", -8_000.0, -8_000.0, False),
        ("Central", -15_000.0, 0.0, False),
        ("North", -15_000.0, 10_000.0, False),
        ("South", -15_000.0, -10_000.0, False),
        ("West", -25_000.0, 0.0, False),
    )
    rows = []
    for node_id, east_m, north_m, camera in specifications:
        point = enu_to_geodetic(ENU(east_m, north_m, 0.0), _PERIMETER_EAST_ORIGIN)
        rows.append(
            {
                "nodeId": node_id,
                **_geodetic_fields(point),
                "enabled": node_id not in disabled,
                "spawned": True,
                "cameraCaptureConfigured": camera,
                "runtimeStatus": "OFFLINE" if node_id in disabled else "ONLINE",
                "detectionRangeMeters": 30_000.0,
                "supportedFrequenciesGHz": [2.412],
                "receiverSensitivityDbm": -96.0,
                "searchRadarConfigured": True,
                "searchRadarRangeMeters": 5_000.0,
                "searchRadarRuntimeStatus": (
                    "OFFLINE" if node_id in disabled else "ONLINE"
                ),
            }
        )
    return rows


def _runtime_targets(
    size: int,
    *,
    prefix: str = "EastSwarm",
    speed_m_s: float = 20.0,
) -> list[dict[str, Any]]:
    if size < 1:
        raise ValueError("size must be >= 1")
    columns = max(1, math.ceil(math.sqrt(size)))
    rows: list[dict[str, Any]] = []
    for index in range(size):
        column = index % columns
        row = index // columns
        north_offset = (column - (columns - 1) / 2.0) * 8.0
        up_offset = (row % 3) * 3.0 + 20.0
        point = enu_to_geodetic(
            ENU(400.0 + (row % 2) * 4.0, north_offset, up_offset),
            _PERIMETER_EAST_ORIGIN,
        )
        target_id = f"{prefix}_{index + 1:03d}"
        rows.append(
            {
                "targetActor": target_id,
                "targetLatitudeDegrees": point.latitude_deg,
                "targetLongitudeDegrees": point.longitude_deg,
                "targetHeightMeters": point.altitude_m,
                "speedMetersPerSecond": speed_m_s,
                "headingDegrees": 270.0,
                "airspaceState": "APPROACHING",
                "outsideSimulationPerimeter": True,
                "distanceToPerimeterMeters": 400.0,
                "approachRateMetersPerSecond": speed_m_s,
                "timeToPerimeterSeconds": 400.0 / speed_m_s if speed_m_s > 0.0 else None,
                "ingressCorridorId": "AUDIT_EAST_INBOUND",
                "hostileScenarioTruth": True,
                "kinematicsAvailable": True,
            }
        )
    return rows


def _runtime_snapshot(
    size: int,
    *,
    disabled_ids: Iterable[str] = (),
    prefix: str = "EastSwarm",
    speed_m_s: float = 20.0,
) -> dict[str, Any]:
    nodes = _runtime_nodes(disabled_ids)
    targets = _runtime_targets(size, prefix=prefix, speed_m_s=speed_m_s)
    rf_silent = "rf_silent" in prefix.casefold()
    detected_rf_links: list[dict[str, Any]] = []
    if not rf_silent:
        for node in nodes:
            if node["enabled"] is not True:
                continue
            node_position = Geodetic(
                node["latitudeDegrees"],
                node["longitudeDegrees"],
                node["heightMeters"],
            )
            for target in targets:
                target_position = Geodetic(
                    target["targetLatitudeDegrees"],
                    target["targetLongitudeDegrees"],
                    target["targetHeightMeters"],
                )
                relative = geodetic_to_enu(target_position, node_position)
                range_m = math.sqrt(
                    relative.east_m**2 + relative.north_m**2 + relative.up_m**2
                )
                if range_m > float(node["detectionRangeMeters"]):
                    continue
                detected_rf_links.append(
                    {
                        "timestampUtc": AUDIT_TIME_TEXT,
                        "linkId": (
                            f"{node['nodeId']}|{target['targetActor']}|2.412000"
                        ),
                        "nodeId": node["nodeId"],
                        "targetActor": target["targetActor"],
                        "emitterId": f"{target['targetActor']}:audit-rf",
                        "detected": True,
                        "frequencyGHz": 2.412,
                        "frequencyBandLabel": "2.4 GHz",
                        "slantRangeMeters": range_m,
                        "rangeSemantics": "sensor_to_target_3d_slant_range",
                        "azimuthDegrees": (
                            math.degrees(math.atan2(relative.east_m, relative.north_m))
                            % 360.0
                        ),
                        "elevationDegrees": math.degrees(
                            math.atan2(
                                relative.up_m,
                                math.hypot(relative.east_m, relative.north_m),
                            )
                        ),
                        "lineOfSight": True,
                        "blockingActor": "",
                        "receivedPowerDbm": -60.0,
                        "noiseFloorDbm": -96.0,
                        "snrDb": 36.0,
                        "weatherRFLossDb": 0.0,
                    }
                )

    near_radar_node = next(
        (
            node
            for node in nodes
            if node["nodeId"] in {"East_A", "East_B"} and node["enabled"] is True
        ),
        None,
    )
    search_radar_detections: list[dict[str, Any]] = []
    if near_radar_node is not None:
        radar_origin = Geodetic(
            near_radar_node["latitudeDegrees"],
            near_radar_node["longitudeDegrees"],
            near_radar_node["heightMeters"],
        )
        for target in targets:
            target_position = Geodetic(
                target["targetLatitudeDegrees"],
                target["targetLongitudeDegrees"],
                target["targetHeightMeters"],
            )
            relative = geodetic_to_enu(target_position, radar_origin)
            horizontal_m = math.hypot(relative.east_m, relative.north_m)
            range_m = math.sqrt(horizontal_m**2 + relative.up_m**2)
            bearing_deg = (
                math.degrees(math.atan2(relative.east_m, relative.north_m)) % 360.0
            )
            elevation_deg = math.degrees(math.atan2(relative.up_m, horizontal_m))
            sensor_id = f"{near_radar_node['nodeId']}:SEARCH_RADAR"
            track_id = f"RADAR-{target['targetActor']}"
            search_radar_detections.append(
                {
                    "kind": "SIMULATED_SENSOR_DETECTION",
                    "source": "ANALYTIC_SEARCH_RADAR",
                    "simulated": True,
                    "calibratedDetector": False,
                    "detectionOnly": True,
                    "id": f"{sensor_id}|{track_id}",
                    "timestampUtc": AUDIT_TIME_TEXT,
                    "simulationSeconds": AUDIT_TIME.timestamp(),
                    "nodeId": near_radar_node["nodeId"],
                    "sensorId": sensor_id,
                    "sensorType": "SEARCH_RADAR",
                    "trackId": track_id,
                    "targetActor": target["targetActor"],
                    "rangeMeters": range_m,
                    "bearingDegrees": bearing_deg,
                    "elevationDegrees": elevation_deg,
                    "radialVelocityMetersPerSecond": -speed_m_s,
                    "confidence": 0.95,
                    "radarCrossSectionSquareMeters": 0.03,
                    "lineOfSight": True,
                    "blockingActor": "",
                    "weatherProfile": "AuditClear",
                    "weatherVisibilityMeters": 30_000.0,
                    "weatherRainRateMillimetersPerHour": 0.0,
                    "rangeEnvelopeMeters": 5_000.0,
                    "azimuthFieldOfRegardDegrees": 360.0,
                    "elevationFieldOfRegardDegrees": 60.0,
                    "measurementNoiseModel": "deterministic_audit_fixture",
                }
            )

    return {
        "schemaVersion": "triad.live_rf_snapshot.v3",
        "timestampUtc": AUDIT_TIME_TEXT,
        "simulationSeconds": AUDIT_TIME.timestamp(),
        "sampleComplete": True,
        "detectionOnly": True,
        "actionsTaken": "none",
        "weather": {
            "profile": "AuditClear",
            "rainRateMillimetersPerHour": 0.0,
            "visibilityMeters": 30_000.0,
        },
        "simulationPerimeter": {
            "geometryType": "axis_aligned_wgs84_rectangle",
            "minimumLongitudeDegrees": 103.62,
            "maximumLongitudeDegrees": 104.02,
            "minimumLatitudeDegrees": 1.22,
            "maximumLatitudeDegrees": 1.47,
            "boundaryInclusive": True,
            "legalOrNationalBoundary": False,
        },
        "sensorNodes": nodes,
        "scenarioTargets": targets,
        "detectedRFLinks": detected_rf_links,
        "searchRadarDetections": search_radar_detections,
        "ptzConfirmations": [],
    }


def _snapshot_latency_values(snapshot: Mapping[str, Any]) -> list[float]:
    values: list[float] = []
    for track in snapshot.get("tracks", []):
        if not isinstance(track, Mapping):
            continue
        fusion = track.get("fusion")
        if not isinstance(fusion, Mapping):
            continue
        value = fusion.get("decisionLatencyMilliseconds")
        if isinstance(value, (int, float)) and math.isfinite(float(value)):
            values.append(float(value))
    return values


def _snapshot_time_to_perimeter_values(snapshot: Mapping[str, Any]) -> list[float]:
    values: list[float] = []
    for track in snapshot.get("tracks", []):
        if not isinstance(track, Mapping):
            continue
        value = track.get("timeToSimulationPerimeterSeconds")
        if isinstance(value, (int, float)) and float(value) >= 0.0 and math.isfinite(float(value)):
            values.append(float(value))
    return values


def audit_swarm_sweep(clock: Callable[[], float]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for size in SWARM_SIZES:
        snapshot, elapsed_ms = _timed(
            clock,
            lambda size=size: build_layered_snapshot(_runtime_snapshot(size)),
        )
        summary = snapshot["summary"]
        latency = _distribution(_snapshot_latency_values(snapshot))
        time_to_perimeter = _distribution(_snapshot_time_to_perimeter_values(snapshot))
        rows.append(
            {
                "requestedSwarmSize": size,
                "processedTrackCount": summary["observationContactCount"],
                "detectedTrackCount": summary["detectedTrackCount"],
                "activeOperatorCueCount": summary["activeOperatorCueCount"],
                "preliminaryCueCount": summary["preliminaryCueCount"],
                "corroboratedDetectionCount": summary["corroboratedDetectionCount"],
                "confirmedTrackCount": summary["confirmedTrackCount"],
                "outsideDetectedTrackCount": summary["outsideDetectedTrackCount"],
                "widebandRFDetectedLinkCount": summary["widebandRFDetectedLinkCount"],
                "mmWaveDetectedLinkCount": summary["mmWaveDetectedLinkCount"],
                "detectionRate": _round(
                    summary["detectedTrackCount"] / summary["observationContactCount"], 6
                ),
                "operatorCueRate": _round(
                    summary["activeOperatorCueCount"] / summary["observationContactCount"], 6
                ),
                "modeledDecisionLatencyMilliseconds": latency,
                "estimatedTimeToPerimeterSeconds": time_to_perimeter,
                "processingRuntimeMilliseconds": elapsed_ms,
                "legacyUnrealDetectedRFLinksConsumed": snapshot[
                    "legacyUnrealDetectedRFLinksConsumed"
                ],
                "suppliedRFModelsUsed": snapshot["suppliedRFModelsUsed"],
            }
        )
    return rows


def audit_runtime_site_failures(clock: Callable[[], float]) -> list[dict[str, Any]]:
    all_nodes = tuple(item["nodeId"] for item in _runtime_nodes())
    cases = (
        ("nominal", ()),
        ("nearest_site_offline", ("East_A",)),
        ("both_near_sites_offline", ("East_A", "East_B")),
        ("all_sites_offline", all_nodes),
    )
    rows: list[dict[str, Any]] = []
    for scenario_id, disabled in cases:
        snapshot, elapsed_ms = _timed(
            clock,
            lambda disabled=disabled: build_layered_snapshot(
                _runtime_snapshot(1, disabled_ids=disabled, prefix="EastFailureSwarm")
            ),
        )
        summary = snapshot["summary"]
        track = snapshot["tracks"][0] if snapshot["tracks"] else None
        fusion = (
            track["fusion"]
            if track is not None
            else {
                "decision": "NO_CURRENT_EVIDENCE",
                "decisionLatencyMilliseconds": None,
            }
        )
        rows.append(
            {
                "scenarioId": scenario_id,
                "disabledSiteIds": list(disabled),
                "onlineSiteCount": summary["sensorSiteCount"],
                "detectedTrackCount": summary["detectedTrackCount"],
                "operatorCueCount": summary["activeOperatorCueCount"],
                "preliminaryCueCount": summary["preliminaryCueCount"],
                "corroboratedDetectionCount": summary["corroboratedDetectionCount"],
                "fusionDecision": fusion["decision"],
                "widebandDetectingNodeCount": (
                    track["modalitySummary"]["widebandRF"]["detectingNodeCount"]
                    if track is not None
                    else 0
                ),
                "mmWaveDetectingNodeCount": 0,
                "modeledDecisionLatencyMilliseconds": fusion.get(
                    "decisionLatencyMilliseconds"
                ),
                "processingRuntimeMilliseconds": elapsed_ms,
            }
        )
    return rows


def audit_rf_silent_target(clock: Callable[[], float]) -> dict[str, Any]:
    snapshot_input = _runtime_snapshot(
        1,
        prefix="HighSpeedAerialCandidate_RF_Silent",
        speed_m_s=100.0,
    )
    target_id = snapshot_input["scenarioTargets"][0]["targetActor"]
    passive_snapshot, passive_ms = _timed(
        clock, lambda: build_layered_snapshot(snapshot_input)
    )
    rgb_row = {
        "evidenceId": "audit-rgb-silent-target",
        "targetId": target_id,
        "nodeId": "East_A",
        "timestampUtc": AUDIT_TIME_TEXT,
        "score": 0.92,
        "rangeMeters": 400.0,
        "modelId": "deterministic_post_association_audit_fixture",
        "frameId": "audit-rgb-frame",
        "frameFileName": "not-generated.png",
        "bboxXyxyPixels": [100.0, 100.0, 120.0, 120.0],
        "className": "aerial candidate",
        "visualSemantics": "post-association audit fixture; no model inference performed",
        "associationIoU": 1.0,
        "associationProvenance": "audit_hypothetical_estimated_track_association",
        "fusionEligible": True,
        "fusionExclusionReason": None,
    }
    event_row = {
        "evidenceId": "audit-event-silent-target",
        "targetId": target_id,
        "nodeId": "East_A",
        "timestampUtc": AUDIT_TIME_TEXT,
        "score": 0.92,
        "modelId": "deterministic_post_association_audit_fixture",
        "frameId": "audit-event-frame",
        "frameFileName": "not-generated.png",
        "bboxXyxyPixels": [100.0, 100.0, 120.0, 120.0],
        "className": "aerial candidate",
        "visualSemantics": "post-association audit fixture; no model inference performed",
        "associationIoU": 1.0,
        "proxyInput": False,
        # This is an explicitly hypothetical native-event representation test
        # double, not the RGB-derived proxy path and not evidence of model
        # accuracy. It shares the visual fusion family with RGB.
        "sourceEncoding": "audit_native_event_family_fixture_v1",
        "physicalNeuromorphicSensorData": True,
        "trainingPreprocessingEquivalent": True,
        "fusionEligible": True,
        "fusionExclusionReason": None,
    }
    corroborated_snapshot, corroborated_ms = _timed(
        clock,
        lambda: build_layered_snapshot(
            snapshot_input,
            rgb_rows=(rgb_row,),
            event_rows=(event_row,),
        ),
    )
    passive_track = passive_snapshot["tracks"][0]
    corroborated_track = corroborated_snapshot["tracks"][0]
    return {
        "targetId": target_id,
        "targetType": corroborated_track["targetType"],
        "rfSilentByScenarioDefinition": True,
        "widebandRFDetectedLinkCount": passive_snapshot["summary"][
            "widebandRFDetectedLinkCount"
        ],
        "mmWaveDetectedLinkCount": passive_snapshot["summary"][
            "mmWaveDetectedLinkCount"
        ],
        "searchRadarDetectionCount": passive_snapshot["summary"][
            "searchRadarDetectionCount"
        ],
        "passiveOnly": {
            "fusionDecision": passive_track["fusion"]["decision"],
            "operatorCueCount": int(passive_track["alert"]["active"]),
            "processingRuntimeMilliseconds": passive_ms,
        },
        "withPostAssociationVisualFixtures": {
            "fusionDecision": corroborated_track["fusion"]["decision"],
            "activeModalityFamilies": corroborated_track["fusion"][
                "activeModalityFamilies"
            ],
            "activeModalityFamilyCount": corroborated_track["fusion"][
                "activeModalityFamilyCount"
            ],
            "operatorCueCount": int(corroborated_track["alert"]["active"]),
            "detectionAlertCount": int(
                corroborated_track["fusion"]["detectionAlert"]
            ),
            "modeledDecisionLatencyMilliseconds": corroborated_track["fusion"].get(
                "decisionLatencyMilliseconds"
            ),
            "processingRuntimeMilliseconds": corroborated_ms,
        },
        "visualFixtureSemantics": (
            "Deterministic positive evidence injected after target association. This exercises "
            "fusion and RF-silent fallback, but does not run or validate either visual model. "
            "RGB and event rows share one visual family and cannot corroborate each other. "
            "Together with search radar, the fixtures therefore exercise a two-family corroborated "
            "alert, not a three-family confirmation. The real RGB-derived proxy event path is "
            "diagnostic-only and excluded from fusion-family counting."
        ),
        "suppliedRFModelsUsed": False,
        "engagementLogicPresent": False,
    }


def _all_modeled_latencies(report: Mapping[str, Any]) -> list[float]:
    values: list[float] = []
    for section_name in (
        "rfDistanceSweep",
        "mmWaveDistanceSweep",
        "speedSweep",
        "rainSweep",
        "modalityFailureSweep",
        "runtimeSiteFailureSweep",
    ):
        for row in report.get(section_name, []):
            if not isinstance(row, Mapping):
                continue
            value = row.get("modeledDecisionLatencyMilliseconds")
            if isinstance(value, (int, float)) and math.isfinite(float(value)):
                values.append(float(value))
    silent = report.get("rfSilentTarget")
    if isinstance(silent, Mapping):
        visual = silent.get("withPostAssociationVisualFixtures")
        if isinstance(visual, Mapping):
            value = visual.get("modeledDecisionLatencyMilliseconds")
            if isinstance(value, (int, float)) and math.isfinite(float(value)):
                values.append(float(value))
    for row in report.get("swarmSweep", []):
        if not isinstance(row, Mapping):
            continue
        distribution = row.get("modeledDecisionLatencyMilliseconds")
        if isinstance(distribution, Mapping):
            value = distribution.get("p50")
            count = distribution.get("sampleCount")
            if isinstance(value, (int, float)) and isinstance(count, int):
                values.extend([float(value)] * count)
    return values


def _all_processing_runtimes(report: Mapping[str, Any]) -> list[float]:
    values: list[float] = []
    for section_name in (
        "rfDistanceSweep",
        "mmWaveDistanceSweep",
        "speedSweep",
        "rainSweep",
        "modalityFailureSweep",
        "runtimeSiteFailureSweep",
        "swarmSweep",
    ):
        for row in report.get(section_name, []):
            if isinstance(row, Mapping):
                value = row.get("processingRuntimeMilliseconds")
                if isinstance(value, (int, float)) and math.isfinite(float(value)):
                    values.append(float(value))
    silent = report.get("rfSilentTarget")
    if isinstance(silent, Mapping):
        for key in ("passiveOnly", "withPostAssociationVisualFixtures"):
            row = silent.get(key)
            if isinstance(row, Mapping):
                value = row.get("processingRuntimeMilliseconds")
                if isinstance(value, (int, float)) and math.isfinite(float(value)):
                    values.append(float(value))
    return values


def _canonical_detection_digest(report: Mapping[str, Any]) -> str:
    excluded = {
        "processingRuntimeMilliseconds",
        "processingRuntimeSummaryMilliseconds",
    }

    def strip_runtime(value: Any) -> Any:
        if isinstance(value, Mapping):
            return {
                key: strip_runtime(item)
                for key, item in value.items()
                if key not in excluded and key != "deterministicOutcomeSha256"
            }
        if isinstance(value, list):
            return [strip_runtime(item) for item in value]
        return value

    payload = json.dumps(
        strip_runtime(report),
        sort_keys=True,
        separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def run_audit(*, clock: Callable[[], float] = time.perf_counter) -> dict[str, Any]:
    """Execute every required audit axis and return a serializable report."""

    report: dict[str, Any] = {
        "schemaVersion": AUDIT_SCHEMA,
        "auditScenarioTimestampUtc": AUDIT_TIME_TEXT,
        "simulationOnly": True,
        "detectionOnly": True,
        "engagementLogicPresent": False,
        "suppliedRFModelsUsed": False,
        "calibratedOperationalPerformanceClaimed": False,
        "requiredAxes": {
            "rfDistancesKilometers": list(RF_DISTANCES_KM),
            "mmWaveDistancesMeters": list(MMWAVE_DISTANCES_M),
            "speedsMetersPerSecond": list(SPEEDS_M_S),
            "rainRatesMillimetersPerHour": list(RAIN_RATES_MM_H),
            "swarmSizes": list(SWARM_SIZES),
            "sensorFailures": "site-level end-to-end plus modality-level fusion",
            "rfSilentTarget": True,
        },
        "rfDistanceSweep": audit_rf_distance_sweep(clock),
        "mmWaveDistanceSweep": audit_mmwave_distance_sweep(clock),
        "speedSweep": audit_speed_sweep(clock),
        "rainSweep": audit_rain_sweep(clock),
        "modalityFailureSweep": audit_modality_failures(clock),
        "runtimeSiteFailureSweep": audit_runtime_site_failures(clock),
        "rfSilentTarget": audit_rf_silent_target(clock),
        "swarmSweep": audit_swarm_sweep(clock),
        "claimBoundary": {
            "mathematicalSimulationCoverage": "EXECUTED",
            "fieldCalibration": "NOT_PERFORMED",
            "hardwareCharacterization": "NOT_PERFORMED",
            "visualModelInferenceInThisHarness": "NOT_PERFORMED",
            "visualFixtureBoundary": (
                "The RF-silent fallback injects deterministic positive evidence after "
                "association; it validates fusion handling only."
            ),
            "hostilityInference": "NOT_PERFORMED; scenario truth is evaluator-only",
            "engagementOrEffectorControl": "ABSENT",
        },
        "limitations": [
            "RF results use free-space propagation plus declared analytic losses; clutter, multipath, antenna patterns, interference occupancy, receiver saturation, and calibration error are not field-calibrated.",
            "mmWave results use a monostatic radar equation, a deterministic CFAR-style SNR gate, a declared RCS, and a bounded rain term; they are not vendor hardware range claims.",
            "Modeled latency is configured scan/update plus processing/network delay, not a measured camera-to-C2 wall-clock latency.",
            "Processing runtime is host- and load-dependent and is excluded from the deterministic outcome digest.",
            "Swarm members are simulated independently; mutual RF interference, radar resolution-cell merging, occlusion, and network queue saturation are not yet modeled.",
            "The audit contains detection and operator-cue logic only. It contains no engagement, targeting, or effector control.",
        ],
    }
    report["modeledDecisionLatencySummaryMilliseconds"] = _distribution(
        _all_modeled_latencies(report)
    )
    report["processingRuntimeSummaryMilliseconds"] = _distribution(
        _all_processing_runtimes(report)
    )
    report["timeToPerimeterSummarySeconds"] = _distribution(
        row["timeToPerimeterSeconds"]
        for row in report["speedSweep"]
        if row["timeToPerimeterSeconds"] is not None
    )
    report["determinism"] = {
        "scenarioInputsFixed": True,
        "detectionAndCueOutcomesDeterministic": True,
        "processingRuntimeHostDependent": True,
        "processingRuntimeExcludedFromOutcomeDigest": True,
    }
    report["determinism"]["deterministicOutcomeSha256"] = _canonical_detection_digest(
        report
    )
    return report


def _status(value: int | bool) -> str:
    return "yes" if bool(value) else "no"


def render_markdown(report: Mapping[str, Any]) -> str:
    """Render a compact evidence-led Markdown summary."""

    lines = [
        "# TRIAD layered sensor audit",
        "",
        "This report is a deterministic mathematical simulation audit. It is not field calibration, hardware acceptance, or an operational performance claim.",
        "",
        f"- Scenario timestamp: `{report['auditScenarioTimestampUtc']}`",
        f"- Supplied RF models used: `{str(report['suppliedRFModelsUsed']).lower()}`",
        f"- Engagement logic present: `{str(report['engagementLogicPresent']).lower()}`",
        f"- Deterministic outcome SHA-256: `{report['determinism']['deterministicOutcomeSha256']}`",
        "",
        "## RF distance sweep",
        "",
        "| Distance (km) | Channels detected | Beyond primary 2.4/5.8 channels | Receiver detections | Operator cue | Latency (ms) |",
        "|---:|---:|---:|---:|:---:|---:|",
    ]
    for row in report["rfDistanceSweep"]:
        lines.append(
            f"| {row['distanceKilometers']:g} | {row['detectedChannelsAtOneReceiver']} | "
            f"{row['detectedBeyondPrimary24And58GHzChannelCount']} | "
            f"{row['detectingReceiverCount']} | {_status(row['operatorCueCount'])} | "
            f"{row['modeledDecisionLatencyMilliseconds'] or '-'} |"
        )
    lines.extend(
        (
            "",
            "## mmWave distance sweep",
            "",
            "| Distance (m) | mmWave detected | SNR (dB) | Cue with RF | Fusion decision |",
            "|---:|:---:|---:|:---:|---|",
        )
    )
    for row in report["mmWaveDistanceSweep"]:
        lines.append(
            f"| {row['distanceMeters']:g} | {_status(row['mmWaveDetectionCount'])} | "
            f"{row['mmWaveSnrDb']:.3f} | {_status(row['operatorCueCount'])} | "
            f"{row['fusionDecision']} |"
        )
    lines.extend(
        (
            "",
            "## Speed and time-to-perimeter sweep",
            "",
            "| Speed (m/s) | Closing velocity (m/s) | mmWave detected | Operator cue | Time to 5 km perimeter (s) |",
            "|---:|---:|:---:|:---:|---:|",
        )
    )
    for row in report["speedSweep"]:
        ttp = row["timeToPerimeterSeconds"]
        lines.append(
            f"| {row['speedMetersPerSecond']:g} | {row['measuredClosingVelocityMetersPerSecond']:g} | "
            f"{_status(row['mmWaveDetectionCount'])} | {_status(row['operatorCueCount'])} | "
            f"{ttp if ttp is not None else '-'} |"
        )
    lines.extend(
        (
            "",
            "## Rain stress sweep",
            "",
            "| Rain (mm/h) | RF channels at 20 km | mmWave at 400 m | mmWave rain loss (dB) | Cue tier |",
            "|---:|---:|:---:|---:|---|",
        )
    )
    for row in report["rainSweep"]:
        lines.append(
            f"| {row['rainRateMillimetersPerHour']:g} | "
            f"{row['widebandDetectedChannelCountAt20Kilometers']} | "
            f"{_status(row['mmWaveDetectionCountAt400Meters'])} | "
            f"{row['mmWaveTwoWayRainLossDb']:.3f} | {row['fusionDecision']} |"
        )
    lines.extend(
        (
            "",
            "## Sensor failure behavior",
            "",
            "### Modality-level fusion failures",
            "",
            "| Scenario | Evidence | Independent families | Operator cue | Decision |",
            "|---|---:|---:|:---:|---|",
        )
    )
    for row in report["modalityFailureSweep"]:
        lines.append(
            f"| {row['scenarioId']} | {row['availableEvidenceCount']} | "
            f"{row['activeModalityFamilyCount']} | {_status(row['operatorCueCount'])} | "
            f"{row['fusionDecision']} |"
        )
    lines.extend(
        (
            "",
            "### End-to-end site failures",
            "",
            "| Scenario | Online sites | RF sites detecting | mmWave sites detecting | Operator cue | Decision |",
            "|---|---:|---:|---:|:---:|---|",
        )
    )
    for row in report["runtimeSiteFailureSweep"]:
        lines.append(
            f"| {row['scenarioId']} | {row['onlineSiteCount']} | "
            f"{row['widebandDetectingNodeCount']} | {row['mmWaveDetectingNodeCount']} | "
            f"{_status(row['operatorCueCount'])} | {row['fusionDecision']} |"
        )
    silent = report["rfSilentTarget"]
    lines.extend(
        (
            "",
            "## RF-silent fallback",
            "",
            f"The RF-silent high-speed candidate produced `{silent['widebandRFDetectedLinkCount']}` RF links and `{silent['mmWaveDetectedLinkCount']}` mmWave links. Radar-only fusion returned `{silent['passiveOnly']['fusionDecision']}`. After deterministic post-association RGB and event evidence was injected as one shared visual family, fusion returned `{silent['withPostAssociationVisualFixtures']['fusionDecision']}` with `{silent['withPostAssociationVisualFixtures']['activeModalityFamilyCount']}` independent families.",
            "",
            f"> {silent['visualFixtureSemantics']}",
            "",
            "## Swarm scale",
            "",
            "| Requested | Processed | Detected | Operator cues | Outside detected | Runtime (ms) | Modeled p95 latency (ms) |",
            "|---:|---:|---:|---:|---:|---:|---:|",
        )
    )
    for row in report["swarmSweep"]:
        lines.append(
            f"| {row['requestedSwarmSize']} | {row['processedTrackCount']} | "
            f"{row['detectedTrackCount']} | {row['activeOperatorCueCount']} | "
            f"{row['outsideDetectedTrackCount']} | {row['processingRuntimeMilliseconds']:.3f} | "
            f"{row['modeledDecisionLatencyMilliseconds']['p95']} |"
        )
    latency = report["modeledDecisionLatencySummaryMilliseconds"]
    runtime = report["processingRuntimeSummaryMilliseconds"]
    ttp = report["timeToPerimeterSummarySeconds"]
    lines.extend(
        (
            "",
            "## Aggregate measurements",
            "",
            f"- Modeled decision latency: p50 `{latency['p50']}` ms, p95 `{latency['p95']}` ms, p99 `{latency['p99']}` ms across `{latency['sampleCount']}` cue samples.",
            f"- Host processing runtime: p50 `{runtime['p50']}` ms, p95 `{runtime['p95']}` ms, p99 `{runtime['p99']}` ms across `{runtime['sampleCount']}` timed operations.",
            f"- Time to the simulated perimeter: p50 `{ttp['p50']}` s, p95 `{ttp['p95']}` s, p99 `{ttp['p99']}` s for positive approach speeds.",
            "",
            "Processing runtime is host- and load-dependent. It is excluded from the deterministic outcome digest.",
            "",
            "## Claim boundary",
            "",
            "- Mathematical simulation coverage: executed.",
            "- Field calibration and hardware characterization: not performed.",
            "- Visual model inference in this harness: not performed.",
            "- Hostility inference: not performed; authored truth remains evaluator-only.",
            "- Engagement or effector control: absent.",
            "",
            "## Limitations",
            "",
        )
    )
    lines.extend(f"- {item}" for item in report["limitations"])
    lines.append("")
    return "\n".join(lines)


def write_reports(
    report: Mapping[str, Any],
    *,
    output_directory: Path = DEFAULT_OUTPUT_DIRECTORY,
    json_name: str = DEFAULT_JSON_NAME,
    markdown_name: str = DEFAULT_MARKDOWN_NAME,
) -> tuple[Path, Path]:
    output_directory.mkdir(parents=True, exist_ok=True)
    json_path = output_directory / json_name
    markdown_path = output_directory / markdown_name
    json_temporary = json_path.with_suffix(json_path.suffix + ".tmp")
    markdown_temporary = markdown_path.with_suffix(markdown_path.suffix + ".tmp")
    json_temporary.write_text(
        json.dumps(report, indent=2, sort_keys=True, allow_nan=False) + "\n",
        encoding="utf-8",
    )
    markdown_temporary.write_text(render_markdown(report), encoding="utf-8")
    json_temporary.replace(json_path)
    markdown_temporary.replace(markdown_path)
    return json_path, markdown_path


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run the deterministic TRIAD layered sensor audit"
    )
    parser.add_argument("--output-directory", type=Path, default=DEFAULT_OUTPUT_DIRECTORY)
    parser.add_argument("--json-name", default=DEFAULT_JSON_NAME)
    parser.add_argument("--markdown-name", default=DEFAULT_MARKDOWN_NAME)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    report = run_audit()
    json_path, markdown_path = write_reports(
        report,
        output_directory=args.output_directory,
        json_name=args.json_name,
        markdown_name=args.markdown_name,
    )
    print(
        json.dumps(
            {
                "status": "complete",
                "json": str(json_path),
                "markdown": str(markdown_path),
                "deterministicOutcomeSha256": report["determinism"][
                    "deterministicOutcomeSha256"
                ],
                "calibratedOperationalPerformanceClaimed": False,
            },
            separators=(",", ":"),
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
