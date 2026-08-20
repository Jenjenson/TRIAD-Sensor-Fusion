"""Deterministic simulation audit for the radar-cued PTZ fusion path.

This is an integration-contract audit, not a physical sensor validation and
not a learned-model evaluation.  It exercises the accepted Unreal v3 evidence
schema through the production layered-runtime builder at eight Singapore
simulation sites, several slant ranges, adverse weather assumptions, and
negative fail-closed cases.
"""

from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import json
import math
from pathlib import Path
from typing import Any, Mapping, Sequence

from .geodesy import ENU, Geodetic, enu_to_geodetic
from .layered_runtime import build_layered_snapshot, write_json_atomic


AUDIT_SCHEMA = "triad.radar_ptz_multirange_audit.v1"
AS_OF = datetime(2026, 8, 3, 4, 0, tzinfo=timezone.utc)
TIMESTAMP = AS_OF.isoformat(timespec="seconds").replace("+00:00", "Z")
CLEAR_RANGES_METERS = (25.0, 100.0, 250.0, 500.0, 1_000.0)

SITE_REFERENCE: tuple[tuple[str, float, float, float], ...] = (
    ("West_Sector", 1.321, 103.650, 45.0),
    ("Jurong_Sector", 1.335, 103.705, 45.0),
    ("North_Sector", 1.435, 103.786, 45.0),
    ("NorthEast_Sector", 1.405, 103.902, 45.0),
    ("East_Sector", 1.357, 103.988, 45.0),
    ("Central_Sector", 1.345, 103.780, 70.0),
    ("City_Sector", 1.286, 103.860, 95.0),
    ("South_Sector", 1.254, 103.823, 45.0),
)

WEATHER_CASES: tuple[dict[str, Any], ...] = (
    {
        "id": "CLEAR",
        "profile": "Clear",
        "rainRateMillimetersPerHour": 0.0,
        "visibilityMeters": 30_000.0,
        "radarConfidence": 0.94,
        "eoConfidence": 0.92,
        "thermalConfidence": 0.90,
        "eoWeatherFactor": 1.0,
        "thermalWeatherFactor": 1.0,
    },
    {
        "id": "MONSOON",
        "profile": "Monsoon",
        "rainRateMillimetersPerHour": 50.0,
        "visibilityMeters": 1_500.0,
        "radarConfidence": 0.84,
        "eoConfidence": 0.68,
        "thermalConfidence": 0.82,
        "eoWeatherFactor": 1.0,
        "thermalWeatherFactor": 0.8,
    },
    {
        "id": "DENSE_FOG",
        "profile": "DenseFog",
        "rainRateMillimetersPerHour": 5.0,
        "visibilityMeters": 500.0,
        "radarConfidence": 0.86,
        "eoConfidence": 0.58,
        "thermalConfidence": 0.84,
        "eoWeatherFactor": 1.0,
        "thermalWeatherFactor": 0.98,
    },
)


def _sensor_nodes() -> list[dict[str, Any]]:
    return [
        {
            "nodeId": node_id,
            "latitudeDegrees": latitude,
            "longitudeDegrees": longitude,
            "heightMeters": height,
            "enabled": True,
            "spawned": True,
            "cameraCaptureConfigured": True,
            "runtimeStatus": "ONLINE",
            "detectionRangeMeters": 20_000.0,
            "supportedFrequenciesGHz": [2.412],
            "receiverSensitivityDbm": -96.0,
            "searchRadarConfigured": True,
            "searchRadarRangeMeters": 5_000.0,
            "searchRadarRuntimeStatus": "ONLINE",
            "eoPtzConfigured": True,
            "eoPtzConfirmationRangeMeters": 2_000.0,
            "eoPtzRuntimeStatus": "ONLINE",
            "thermalPtzConfigured": True,
            "thermalPtzConfirmationRangeMeters": 2_000.0,
            "thermalPtzRuntimeStatus": "ONLINE",
        }
        for node_id, latitude, longitude, height in SITE_REFERENCE
    ]


def _trial_snapshot(
    *,
    site_index: int,
    range_m: float,
    weather: Mapping[str, Any],
    radar_confidence: float | None = None,
    include_ptz: bool = True,
    stale_radar: bool = False,
    ptz_line_of_sight: bool = True,
    silent_emitter: bool = False,
) -> dict[str, Any]:
    nodes = _sensor_nodes()
    node = nodes[site_index]
    node_id = str(node["nodeId"])
    bearing_deg = float((site_index * 45) % 360)
    elevation_deg = 5.0
    horizontal_m = range_m * math.cos(math.radians(elevation_deg))
    target = enu_to_geodetic(
        ENU(
            horizontal_m * math.sin(math.radians(bearing_deg)),
            horizontal_m * math.cos(math.radians(bearing_deg)),
            range_m * math.sin(math.radians(elevation_deg)),
        ),
        Geodetic(
            float(node["latitudeDegrees"]),
            float(node["longitudeDegrees"]),
            float(node["heightMeters"]),
        ),
    )
    emitter_label = "MissileSilent" if silent_emitter else "EastWideband"
    target_id = (
        f"Audit{emitter_label}_{weather['id']}_{site_index}_{int(range_m)}m"
    )
    track_id = f"AUDIT-RADAR-{site_index}-{int(range_m)}"
    radar_sensor_id = f"{node_id}:SEARCH_RADAR"
    timestamp = "2026-08-03T03:59:50Z" if stale_radar else TIMESTAMP
    projected_width = max(4.0, min(180.0, 18_000.0 / max(range_m, 1.0)))
    projected_height = max(4.0, projected_width * 0.72)
    x1 = 640.0 - projected_width * 0.5
    y1 = 360.0 - projected_height * 0.5
    x2 = 640.0 + projected_width * 0.5
    y2 = 360.0 + projected_height * 0.5
    snapshot: dict[str, Any] = {
        "schemaVersion": "triad.live_rf_snapshot.v3",
        "sampleComplete": True,
        "detectionOnly": True,
        "actionsTaken": "none",
        "timestampUtc": TIMESTAMP,
        "simulationSeconds": 1_000.0,
        "weather": {
            "profile": weather["profile"],
            "rainRateMillimetersPerHour": weather["rainRateMillimetersPerHour"],
            "visibilityMeters": weather["visibilityMeters"],
        },
        "simulationPerimeter": {
            "enabled": True,
            "minimumLongitudeDegrees": float(node["longitudeDegrees"]) - 0.00005,
            "maximumLongitudeDegrees": float(node["longitudeDegrees"]) + 0.00005,
            "minimumLatitudeDegrees": float(node["latitudeDegrees"]) - 0.00005,
            "maximumLatitudeDegrees": float(node["latitudeDegrees"]) + 0.00005,
            "phaseRateDeadbandMetersPerSecond": 0.25,
            "scopeNotice": "deterministic audit geometry; not a legal boundary",
        },
        "sensorNodes": nodes,
        "scenarioTargets": [
            {
                "targetActor": target_id,
                "targetLatitudeDegrees": target.latitude_deg,
                "targetLongitudeDegrees": target.longitude_deg,
                "targetHeightMeters": target.altitude_m,
                "speedMetersPerSecond": 20.0,
                "headingDegrees": (bearing_deg + 180.0) % 360.0,
                "ingressCorridorId": "AUDIT_INBOUND",
            }
        ],
        "detectedRFLinks": (
            []
            if silent_emitter
            else [
                {
                    "timestampUtc": TIMESTAMP,
                    "linkId": f"{node_id}|{target_id}|2.412000",
                    "nodeId": node_id,
                    "targetActor": target_id,
                    "emitterId": f"{target_id}:audit-rf",
                    "detected": True,
                    "frequencyGHz": 2.412,
                    "frequencyBandLabel": "2.4 GHz",
                    "slantRangeMeters": range_m,
                    "rangeSemantics": "sensor_to_target_3d_slant_range",
                    "azimuthDegrees": bearing_deg,
                    "elevationDegrees": elevation_deg,
                    "lineOfSight": True,
                    "blockingActor": "",
                    "receivedPowerDbm": -60.0,
                    "noiseFloorDbm": -96.0,
                    "snrDb": 36.0,
                    "weatherRFLossDb": 0.0,
                }
            ]
        ),
        "searchRadarDetections": [
            {
                "kind": "SIMULATED_SENSOR_DETECTION",
                "source": "ANALYTIC_SEARCH_RADAR",
                "simulated": True,
                "calibratedDetector": False,
                "detectionOnly": True,
                "timestampUtc": timestamp,
                "simulationSeconds": 1_000.0,
                "nodeId": node_id,
                "sensorId": radar_sensor_id,
                "sensorType": "SEARCH_RADAR",
                "trackId": track_id,
                "targetActor": target_id,
                "rangeMeters": range_m,
                "bearingDegrees": bearing_deg,
                "elevationDegrees": elevation_deg,
                "radialVelocityMetersPerSecond": -20.0,
                "confidence": (
                    float(radar_confidence)
                    if radar_confidence is not None
                    else float(weather["radarConfidence"])
                ),
                "radarCrossSectionSquareMeters": 0.03,
                "lineOfSight": True,
                "weatherProfile": weather["profile"],
                "weatherVisibilityMeters": weather["visibilityMeters"],
                "rangeEnvelopeMeters": 5_000.0,
                "azimuthFieldOfRegardDegrees": 360.0,
                "elevationFieldOfRegardDegrees": 60.0,
            }
        ],
        "ptzConfirmations": [],
    }
    if include_ptz:
        base = {
            "kind": "SIMULATED_SENSOR_CONFIRMATION",
            "source": "SIMULATION_PROJECTION",
            "confirmationMethod": "SIMULATION_PROJECTION_TRUTH",
            "boxSource": "DEBUG_PROJECTION",
            "simulated": True,
            "calibratedDetector": False,
            "actionsTaken": "none",
            "timestampUtc": TIMESTAMP,
            "simulationSeconds": 1_000.0,
            "nodeId": node_id,
            "trackId": track_id,
            "targetActor": target_id,
            "radarCueSensorId": radar_sensor_id,
            "cueAgeSeconds": 0.08,
            "slewState": "SETTLED",
            "confirmed": True,
            "lineOfSight": ptz_line_of_sight,
            "hasFrame": ptz_line_of_sight,
            "blockingActor": None if ptz_line_of_sight else "AuditOccluder",
            "rangeMeters": range_m,
            "fovDegrees": 8.0,
            "imageWidthPixels": 1280,
            "imageHeightPixels": 720,
            "pixelExtentWidth": projected_width,
            "pixelExtentHeight": projected_height,
            "boundingBoxPixels": [x1, y1, x2, y2],
            "reticle": {"x": 640.0, "y": 360.0, "coordinateSpace": "PIXELS"},
            "weatherProfile": weather["profile"],
            "weatherVisibilityMeters": weather["visibilityMeters"],
            "weatherRainRateMillimetersPerHour": weather[
                "rainRateMillimetersPerHour"
            ],
            "weatherConfidenceSemantics": "already_applied_to_confidence",
            "occlusionSemantics": "capture_origin_to_target_bounds",
        }
        snapshot["ptzConfirmations"] = [
            {
                **base,
                "id": f"EO-{track_id}",
                "sensorId": f"{node_id}:EO_PTZ",
                "sensorType": "EO_PTZ",
                "modality": "EO_VISIBLE",
                "confidence": float(weather["eoConfidence"]),
                "weatherConfidenceFactor": float(weather["eoWeatherFactor"]),
                "frameRelativePath": f"RadarPtzFrames/eo/{node_id}/latest.png",
                "metadataRelativePath": f"RadarPtzFrames/eo/{node_id}/latest.json",
                "syntheticThermal": False,
            },
            {
                **base,
                "id": f"THERMAL-{track_id}",
                "sensorId": f"{node_id}:THERMAL_PTZ",
                "sensorType": "THERMAL_PTZ",
                "modality": "THERMAL_SYNTHETIC",
                "confidence": float(weather["thermalConfidence"]),
                "weatherConfidenceFactor": float(weather["thermalWeatherFactor"]),
                "frameRelativePath": f"RadarPtzFrames/thermal/{node_id}/latest.png",
                "metadataRelativePath": f"RadarPtzFrames/thermal/{node_id}/latest.json",
                "syntheticThermal": True,
                "thermalSemantics": "synthetic apparent-temperature palette",
            },
        ]
    return snapshot


def _evaluate_trial(snapshot: Mapping[str, Any]) -> dict[str, Any]:
    result = build_layered_snapshot(snapshot)
    track = result["tracks"][0] if result["tracks"] else {}
    fusion = track.get(
        "fusion",
        {
            "decision": "NO_CURRENT_EVIDENCE",
            "confirmationTier": "UNCONFIRMED",
            "operatorCueActive": False,
            "activeModalityFamilies": [],
            "fusedEvidenceScore": 0.0,
        },
    )
    modalities = set(track.get("activeModalities", []))
    reliable_pass = bool(
        fusion.get("operatorCueActive")
        and fusion.get("decision") == "CONFIRMED_TRACK"
        and {"SEARCH_RADAR", "EO_PTZ", "THERMAL_PTZ"}.issubset(modalities)
        and track.get("trackEstimate", {}).get("searchRadarPolarGeometryConsumed") is True
        and len(track.get("visualFrame", {}).get("confirmations", [])) == 2
    )
    return {
        "siteId": snapshot["searchRadarDetections"][0]["nodeId"],
        "rangeMeters": snapshot["searchRadarDetections"][0]["rangeMeters"],
        "weatherProfile": snapshot["weather"]["profile"],
        "pass": reliable_pass,
        "decision": fusion.get("decision"),
        "confirmationTier": fusion.get("confirmationTier"),
        "operatorCueActive": fusion.get("operatorCueActive"),
        "activeModalities": sorted(modalities),
        "activeModalityFamilies": fusion.get("activeModalityFamilies", []),
        "fusedEvidenceScore": fusion.get("fusedEvidenceScore"),
        "searchRadarAccepted": len(result.get("searchRadarDetections", [])) == 1,
        "ptzConfirmationsAccepted": len(result.get("ptzConfirmations", [])),
        "rejectedEvidenceCount": result.get("summary", {}).get(
            "longRangeEvidenceRejectedCount"
        ),
        "positionMeasurementDerived": track.get("trackEstimate", {}).get(
            "measurementDerivedLocalization"
        ),
        "approachState": track.get("airspaceState"),
        "distanceToPerimeterMeters": track.get("distanceToPerimeterMeters"),
        "nearestSensorDistanceMeters": track.get("nearestSensorDistanceMeters"),
        "visualBoxProvenance": [
            {
                "kind": item["boxes"][0]["kind"],
                "source": item["boxes"][0]["source"],
                "modelId": None,
            }
            for item in track.get("visualFrame", {}).get("confirmations", [])
        ],
    }


def run_audit() -> dict[str, Any]:
    clear_weather = WEATHER_CASES[0]
    range_results: list[dict[str, Any]] = []
    all_clear_trials: list[dict[str, Any]] = []
    for range_m in CLEAR_RANGES_METERS:
        trials = [
            _evaluate_trial(
                _trial_snapshot(
                    site_index=site_index,
                    range_m=range_m,
                    weather=clear_weather,
                )
            )
            for site_index in range(len(SITE_REFERENCE))
        ]
        all_clear_trials.extend(trials)
        passes = sum(item["pass"] is True for item in trials)
        range_results.append(
            {
                "rangeMeters": range_m,
                "trialCount": len(trials),
                "passCount": passes,
                "passRate": round(passes / len(trials), 6),
                "decisionCounts": dict(Counter(item["decision"] for item in trials)),
                "trials": trials,
            }
        )

    weather_results = []
    for weather in WEATHER_CASES:
        trials = [
            _evaluate_trial(
                _trial_snapshot(
                    site_index=site_index,
                    range_m=500.0,
                    weather=weather,
                )
            )
            for site_index in range(len(SITE_REFERENCE))
        ]
        weather_results.append(
            {
                "weatherId": weather["id"],
                "profile": weather["profile"],
                "rainRateMillimetersPerHour": weather[
                    "rainRateMillimetersPerHour"
                ],
                "visibilityMeters": weather["visibilityMeters"],
                "trialCount": len(trials),
                "confirmedTrackCount": sum(
                    item["decision"] == "CONFIRMED_TRACK" for item in trials
                ),
                "operatorCueCount": sum(item["operatorCueActive"] is True for item in trials),
                "decisionCounts": dict(Counter(item["decision"] for item in trials)),
                "trials": trials,
            }
        )

    false_alarm_snapshot = _trial_snapshot(
        site_index=0,
        range_m=1_000.0,
        weather=clear_weather,
        radar_confidence=0.20,
        include_ptz=False,
        silent_emitter=True,
    )
    false_alarm_result = _evaluate_trial(false_alarm_snapshot)
    false_alarm_pass = false_alarm_result["operatorCueActive"] is False

    stale_snapshot = _trial_snapshot(
        site_index=1,
        range_m=1_000.0,
        weather=clear_weather,
        include_ptz=True,
        stale_radar=True,
        silent_emitter=True,
    )
    stale_result = _evaluate_trial(stale_snapshot)
    stale_fail_closed_pass = bool(
        stale_result["searchRadarAccepted"] is False
        and stale_result["ptzConfirmationsAccepted"] == 0
        and stale_result["operatorCueActive"] is False
    )

    occluded_snapshot = _trial_snapshot(
        site_index=2,
        range_m=1_000.0,
        weather=clear_weather,
        include_ptz=True,
        ptz_line_of_sight=False,
        silent_emitter=True,
    )
    occluded_result = _evaluate_trial(occluded_snapshot)
    occlusion_fail_closed_pass = bool(
        occluded_result["ptzConfirmationsAccepted"] == 0
        and occluded_result["operatorCueActive"] is False
    )

    required_clear = [
        item for item in range_results if float(item["rangeMeters"]) <= 500.0
    ]
    required_clear_pass = all(item["passRate"] == 1.0 for item in required_clear)
    preferred_1000_pass = next(
        item["passRate"] == 1.0
        for item in range_results
        if item["rangeMeters"] == 1_000.0
    )
    overall_pass = bool(
        required_clear_pass
        and false_alarm_pass
        and stale_fail_closed_pass
        and occlusion_fail_closed_pass
    )
    return {
        "schemaVersion": AUDIT_SCHEMA,
        "generatedAtUtc": TIMESTAMP,
        "result": "PASS" if overall_pass else "FAIL",
        "simulationOnly": True,
        "physicalSensorValidation": False,
        "learnedModelEvaluation": False,
        "modelOutputClaimed": False,
        "scope": (
            "Deterministic Unreal-v3 contract/fusion audit at eight Singapore simulation "
            "sites; not evidence of physical radar, optics, thermal, or field range."
        ),
        "clearConditionReliabilityRequirement": {
            "rangeMetersInclusive": [25.0, 500.0],
            "requiredPassRate": 1.0,
            "siteCountPerRange": len(SITE_REFERENCE),
            "definition": (
                "fresh wideband RF plus accepted search-radar polar measurement and fresh "
                "LOS settled radar-cued visual confirmation produces CONFIRMED_TRACK, a map "
                "position, and two bridge-ready EO/thermal confirmation frames. EO and "
                "thermal remain one VISUAL family; search radar and mmWave remain one "
                "ACTIVE_RADAR family."
            ),
            "passed": required_clear_pass,
        },
        "preferredOneKilometerSimulationCheckPassed": preferred_1000_pass,
        "clearRangeResults": range_results,
        "weatherAt500Meters": weather_results,
        "negativeCases": {
            "lowScoreFalseAlarm": {
                "passed": false_alarm_pass,
                "result": false_alarm_result,
            },
            "staleRadarRejectsDependentPtz": {
                "passed": stale_fail_closed_pass,
                "result": stale_result,
            },
            "occludedPtzCannotConfirm": {
                "passed": occlusion_fail_closed_pass,
                "result": occluded_result,
            },
        },
        "provenanceInvariant": {
            "boxKind": "SIMULATED_SENSOR_CONFIRMATION",
            "boxSource": "SIMULATION_PROJECTION",
            "learnedModelOutput": False,
            "eoFrame": "Unreal rendered RGB pixels; simulated projection confirmation",
            "thermalFrame": "synthetic Unreal thermal-like pixels; simulated projection confirmation",
            "eoBoxAndRadarRangeCountedAsSeparateEvidence": False,
        },
        "limitations": [
            "Confidence values are declared deterministic audit inputs, not calibrated probabilities.",
            "The audit verifies software contracts, fusion gates, geometry conversion, provenance, and fail-closed behavior.",
            "It does not establish physical probability of detection, false-alarm rate, recognition range, atmospheric transfer accuracy, or hardware performance.",
            "Real acceptance requires measured datasets, calibrated sensors, surveyed geometry, representative backgrounds, and statistically powered trials.",
        ],
    }


def _markdown(report: Mapping[str, Any]) -> str:
    lines = [
        "# TRIAD radar-cued PTZ deterministic audit",
        "",
        f"**Result:** {report['result']}",
        "",
        "> Simulation-only software evidence. This is not physical sensor validation and not a learned-model evaluation.",
        "",
        "## Clear-weather range matrix",
        "",
        "| Slant range | Sites | Passes | Pass rate | Decisions |",
        "|---:|---:|---:|---:|---|",
    ]
    for item in report["clearRangeResults"]:
        lines.append(
            "| {range:.0f} m | {trials} | {passes} | {rate:.0%} | {decisions} |".format(
                range=item["rangeMeters"],
                trials=item["trialCount"],
                passes=item["passCount"],
                rate=item["passRate"],
                decisions=", ".join(
                    f"{key}: {value}" for key, value in item["decisionCounts"].items()
                ),
            )
        )
    lines.extend(
        (
            "",
            "## Weather at 500 m",
            "",
            "| Weather | Rain | Visibility | Confirmed | Operator cues | Decisions |",
            "|---|---:|---:|---:|---:|---|",
        )
    )
    for item in report["weatherAt500Meters"]:
        lines.append(
            "| {profile} | {rain:.1f} mm/h | {visibility:.0f} m | {confirmed}/{trials} | {cues}/{trials} | {decisions} |".format(
                profile=item["profile"],
                rain=item["rainRateMillimetersPerHour"],
                visibility=item["visibilityMeters"],
                confirmed=item["confirmedTrackCount"],
                cues=item["operatorCueCount"],
                trials=item["trialCount"],
                decisions=", ".join(
                    f"{key}: {value}" for key, value in item["decisionCounts"].items()
                ),
            )
        )
    lines.extend(("", "## Negative gates", ""))
    for key, item in report["negativeCases"].items():
        lines.append(f"- {key}: {'PASS' if item['passed'] else 'FAIL'}")
    lines.extend(("", "## Interpretation", ""))
    lines.extend(f"- {item}" for item in report["limitations"])
    lines.append("")
    return "\n".join(lines)


def write_reports(report: Mapping[str, Any], json_path: Path, markdown_path: Path) -> None:
    write_json_atomic(report, json_path)
    markdown_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = markdown_path.with_suffix(markdown_path.suffix + ".tmp")
    temporary.write_text(_markdown(report), encoding="utf-8")
    temporary.replace(markdown_path)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run the deterministic TRIAD radar-cued PTZ multi-range audit"
    )
    parser.add_argument(
        "--json-output",
        type=Path,
        default=Path("reports/radar_ptz_multirange_audit.json"),
    )
    parser.add_argument(
        "--markdown-output",
        type=Path,
        default=Path("reports/radar_ptz_multirange_audit.md"),
    )
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    report = run_audit()
    write_reports(report, args.json_output, args.markdown_output)
    print(json.dumps({"result": report["result"], "json": str(args.json_output)}))
    return 0 if report["result"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
