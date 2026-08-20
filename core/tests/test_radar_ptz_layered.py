from __future__ import annotations

import json
from copy import deepcopy

from singapore_sensor_fusion.layered_runtime import build_layered_snapshot


TIMESTAMP = "2026-08-03T04:00:00Z"


def _node(index: int) -> dict:
    node_id = f"Node_{index}"
    return {
        "nodeId": node_id,
        "latitudeDegrees": 1.30 + index * 0.001,
        "longitudeDegrees": 103.85,
        "heightMeters": 40.0,
        "enabled": True,
        "spawned": True,
        "cameraCaptureConfigured": True,
        "runtimeStatus": "ONLINE",
        "detectionRangeMeters": 20_000.0,
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


def _snapshot(*, distance_m: float = 500.0) -> dict:
    node = _node(0)
    target_id = "Inbound_Track_01"
    radar_sensor_id = f"{node['nodeId']}:SEARCH_RADAR"
    latitude = node["latitudeDegrees"] + distance_m / 111_132.0
    common_ptz = {
        "kind": "SIMULATED_SENSOR_CONFIRMATION",
        "source": "SIMULATION_PROJECTION",
        "confirmationMethod": "SIMULATION_PROJECTION_TRUTH",
        "boxSource": "DEBUG_PROJECTION",
        "simulated": True,
        "calibratedDetector": False,
        "actionsTaken": "none",
        "timestampUtc": TIMESTAMP,
        "simulationSeconds": 1_000.0,
        "nodeId": node["nodeId"],
        "trackId": "RADAR-TRACK-01",
        "targetActor": target_id,
        "radarCueSensorId": radar_sensor_id,
        "cueAgeSeconds": 0.08,
        "slewState": "SETTLED",
        "confirmed": True,
        "lineOfSight": True,
        "hasFrame": True,
        "weatherProfile": "Clear",
        "weatherVisibilityMeters": 30_000.0,
        "weatherRainRateMillimetersPerHour": 0.0,
        "weatherConfidenceFactor": 1.0,
        "weatherConfidenceSemantics": "already_applied_to_confidence",
        "occlusionSemantics": "capture_origin_to_target_bounds",
        "rangeMeters": distance_m,
        "confidence": 0.92,
        "fovDegrees": 8.0,
        "imageWidthPixels": 1280,
        "imageHeightPixels": 720,
        "pixelExtentWidth": 28.0,
        "pixelExtentHeight": 20.0,
        "boundingBoxPixels": [626.0, 350.0, 654.0, 370.0],
        "reticle": {"x": 640.0, "y": 360.0, "coordinateSpace": "pixels"},
    }
    return {
        "schemaVersion": "triad.live_rf_snapshot.v3",
        "sampleComplete": True,
        "detectionOnly": True,
        "actionsTaken": "none",
        "timestampUtc": TIMESTAMP,
        "simulationSeconds": 1_000.0,
        "weather": {
            "profile": "Clear",
            "rainRateMillimetersPerHour": 0.0,
            "visibilityMeters": 30_000.0,
        },
        "simulationPerimeter": {
            "enabled": True,
            "minimumLongitudeDegrees": 103.70,
            "maximumLongitudeDegrees": 104.00,
            "minimumLatitudeDegrees": 1.20,
            "maximumLatitudeDegrees": 1.302,
            "phaseRateDeadbandMetersPerSecond": 0.25,
        },
        "sensorNodes": [_node(index) for index in range(8)],
        "scenarioTargets": [
            {
                "targetActor": target_id,
                "targetLatitudeDegrees": latitude,
                "targetLongitudeDegrees": node["longitudeDegrees"],
                "targetHeightMeters": 100.0,
                "speedMetersPerSecond": 20.0,
                "headingDegrees": 180.0,
                "ingressCorridorId": "NORTH_INBOUND",
            }
        ],
        "searchRadarDetections": [
            {
                "kind": "SIMULATED_SENSOR_DETECTION",
                "source": "ANALYTIC_SEARCH_RADAR",
                "simulated": True,
                "calibratedDetector": False,
                "detectionOnly": True,
                "id": f"{node['nodeId']}:SEARCH_RADAR|RADAR-TRACK-01",
                "timestampUtc": TIMESTAMP,
                "simulationSeconds": 1_000.0,
                "nodeId": node["nodeId"],
                "sensorId": radar_sensor_id,
                "sensorType": "SEARCH_RADAR",
                "trackId": "RADAR-TRACK-01",
                "targetActor": target_id,
                "rangeMeters": distance_m,
                "slantRangeMeters": distance_m,
                "rangeSemantics": "noisy_sensor_to_target_3d_slant_range",
                "bearingDegrees": 0.0,
                "azimuthDegrees": 0.0,
                "elevationDegrees": 6.84,
                "radialVelocityMetersPerSecond": -20.0,
                "confidence": 0.95,
                "confidenceSemantics": "uncalibrated_analytic_score_from_range_LOS_authored_RCS_and_weather",
                "radarCrossSectionSquareMeters": 0.03,
                "radarCrossSectionSemantics": "authored_demo_target_or_default_simulation_input_not_measured",
                "lineOfSight": True,
                "blockingActor": "",
                "weatherProfile": "Clear",
                "weatherVisibilityMeters": 30_000.0,
                "weatherRainRateMillimetersPerHour": 0.0,
                "rangeEnvelopeMeters": 5_000.0,
                "azimuthFieldOfRegardDegrees": 360.0,
                "elevationFieldOfRegardDegrees": 60.0,
                "measurementNoiseModel": "deterministic_seeded_gaussian_range_bearing_elevation_radial_velocity",
            }
        ],
        "ptzConfirmations": [
            {
                **common_ptz,
                "id": "EO-CONFIRM-01",
                "label": "UNREAL VISIBLE PIXELS + SIMULATED RADAR-CUED PTZ",
                "sensorId": f"{node['nodeId']}:EO_PTZ",
                "sensorType": "EO_PTZ",
                "modality": "EO_VISIBLE",
                "frameRelativePath": f"RadarPtzFrames/eo/{node['nodeId']}/latest.png",
                "metadataRelativePath": f"RadarPtzFrames/eo/{node['nodeId']}/latest.json",
                "syntheticThermal": False,
            },
            {
                **common_ptz,
                "id": "THERMAL-CONFIRM-01",
                "label": "SYNTHETIC THERMAL RADAR-CUED PTZ",
                "sensorId": f"{node['nodeId']}:THERMAL_PTZ",
                "sensorType": "THERMAL_PTZ",
                "modality": "THERMAL_SYNTHETIC",
                "frameRelativePath": f"RadarPtzFrames/thermal/{node['nodeId']}/latest.png",
                "metadataRelativePath": f"RadarPtzFrames/thermal/{node['nodeId']}/latest.json",
                "syntheticThermal": True,
                "thermalSemantics": "synthetic apparent-temperature palette",
            },
        ],
    }


def test_v3_registers_all_long_range_sensor_health_and_confirms_track() -> None:
    source = _snapshot()
    source["scenarioTargets"] = []
    result = build_layered_snapshot(source)

    health = [
        item
        for item in result["sensorNodes"]
        if item.get("sensorType") in {"SEARCH_RADAR", "EO_PTZ", "THERMAL_PTZ"}
    ]
    assert len(health) == 24
    assert {item["sensorType"] for item in health} == {
        "SEARCH_RADAR",
        "EO_PTZ",
        "THERMAL_PTZ",
    }
    assert all(item["status"] == "ONLINE" for item in health)

    track = result["tracks"][0]
    assert track["fusion"]["decision"] == "DETECTION_ALERT"
    assert set(track["fusion"]["activeModalityFamilies"]) == {
        "active_radar",
        "visual",
    }
    assert track["fusion"]["activeModalityFamilyCount"] == 2
    assert track["trackEstimate"]["measurementDerivedLocalization"] is True
    assert track["trackEstimate"]["searchRadarPolarGeometryConsumed"] is True
    assert track["scenarioTruth"]["available"] is False
    assert track["modalitySummary"]["searchRadar"]["nearestRangeMeters"] == 500.0
    assert track["modalitySummary"]["searchRadar"][
        "maximumClosingVelocityMetersPerSecond"
    ] == 20.0
    assert track["activeModalities"] == ["SEARCH_RADAR", "EO_PTZ", "THERMAL_PTZ"]
    assert track["visualEvidenceStatus"] == "RADAR_CUED_CONFIRMATION_AVAILABLE"
    assert track["visualFrame"]["transportStatus"] == "FRAME_AVAILABLE"
    assert len(track["visualFrame"]["confirmations"]) == 2
    box = track["visualFrame"]["confirmations"][0]["boxes"][0]
    assert box["kind"] == "SIMULATED_SENSOR_CONFIRMATION"
    assert box["source"] == "SIMULATION_PROJECTION"
    assert track["visualFrame"]["confirmations"][0]["reticle"][
        "coordinateSpace"
    ] == "PIXELS"
    assert track["modalityEvidence"][1]["modelId"] is None
    assert track["modalitySummary"]["eoPtz"][
        "rangeSemantics"
    ] == "radar-cued range, never EO box/depth stacking"
    assert result["ptzConfirmations"][0]["approachState"] == "APPROACHING"
    assert result["ptzConfirmations"][0]["frameRelativePath"].startswith(
        "RadarPtzFrames/eo/"
    )


def test_v3_fails_closed_on_stale_radar_and_unsafe_or_uncued_ptz() -> None:
    snapshot = _snapshot()
    snapshot["searchRadarDetections"][0]["timestampUtc"] = "2026-08-03T03:59:50Z"
    snapshot["ptzConfirmations"][0]["frameRelativePath"] = "../outside.png"
    result = build_layered_snapshot(snapshot)

    assert result["searchRadarDetections"] == []
    assert result["ptzConfirmations"] == []
    assert result["longRangeEvidenceValidation"][
        "rejectedSearchRadarDetectionCount"
    ] == 1
    assert result["longRangeEvidenceValidation"]["rejectedPtzConfirmationCount"] == 2
    assert result["tracks"] == []
    assert result["summary"]["observationContactCount"] == 0


def test_ptz_cannot_borrow_radar_cue_from_a_different_track() -> None:
    snapshot = _snapshot()
    for confirmation in snapshot["ptzConfirmations"]:
        confirmation["trackId"] = "MISMATCHED-RADAR-TRACK"
    result = build_layered_snapshot(snapshot)

    assert len(result["searchRadarDetections"]) == 1
    assert result["ptzConfirmations"] == []
    reasons = {
        item["reason"]
        for item in result["longRangeEvidenceValidation"]["rejectedPtzConfirmations"]
    }
    assert any("same targetActor, sensorId, and trackId" in reason for reason in reasons)


def test_unreal_contract_modality_spelling_is_strict_and_fails_loudly() -> None:
    snapshot = _snapshot()
    snapshot["ptzConfirmations"][0]["modality"] = "EO_PTZ"
    result = build_layered_snapshot(snapshot)

    accepted_types = {item["sensorType"] for item in result["ptzConfirmations"]}
    assert accepted_types == {"THERMAL_PTZ"}
    assert any(
        item["reason"] == "modality must be EO_VISIBLE"
        for item in result["longRangeEvidenceValidation"]["rejectedPtzConfirmations"]
    )


def test_radar_telemetry_cannot_expand_configured_range_or_invalid_for() -> None:
    expanded = _snapshot()
    expanded["searchRadarDetections"][0]["rangeEnvelopeMeters"] = 10_000.0
    result = build_layered_snapshot(expanded)
    assert result["searchRadarDetections"] == []
    assert result["longRangeEvidenceValidation"]["rejectedSearchRadarDetections"][0][
        "reason"
    ] == "declared rangeEnvelopeMeters exceeds the configured node envelope"

    invalid_for = _snapshot()
    invalid_for["searchRadarDetections"][0]["azimuthFieldOfRegardDegrees"] = 400.0
    result = build_layered_snapshot(invalid_for)
    assert result["searchRadarDetections"] == []
    assert result["longRangeEvidenceValidation"]["rejectedSearchRadarDetections"][0][
        "reason"
    ] == "azimuthFieldOfRegardDegrees must be in (0, 360]"


def test_degraded_nlos_search_radar_is_preserved_but_ptz_still_requires_los() -> None:
    snapshot = _snapshot()
    snapshot["searchRadarDetections"][0]["lineOfSight"] = False
    snapshot["searchRadarDetections"][0]["blockingActor"] = "Building_01"
    snapshot["searchRadarDetections"][0]["confidence"] = 0.60
    snapshot["ptzConfirmations"] = []
    result = build_layered_snapshot(snapshot)

    assert len(result["searchRadarDetections"]) == 1
    radar = result["searchRadarDetections"][0]
    assert radar["lineOfSight"] is False
    assert radar["blockingActor"] == "Building_01"
    assert radar["source"] == "ANALYTIC_SEARCH_RADAR"
    assert radar["modelOutputClaimed"] is False


def test_ptz_range_must_match_the_exact_associated_radar_cue() -> None:
    snapshot = _snapshot()
    snapshot["ptzConfirmations"][0]["rangeMeters"] = 700.0
    result = build_layered_snapshot(snapshot)

    assert {item["sensorType"] for item in result["ptzConfirmations"]} == {
        "THERMAL_PTZ"
    }
    assert any(
        item["reason"] == "PTZ rangeMeters does not match its associated radar cue"
        for item in result["longRangeEvidenceValidation"]["rejectedPtzConfirmations"]
    )


def test_unconfirmed_current_snapshot_cannot_retain_previous_confirmation_frame() -> None:
    confirmed_result = build_layered_snapshot(_snapshot())
    confirmed_track = confirmed_result["tracks"][0]
    assert confirmed_track["visualFrame"]["transportStatus"] == "FRAME_AVAILABLE"
    previous_paths = {
        item["relativePath"]
        for item in confirmed_track["visualFrame"]["confirmations"]
    }
    assert previous_paths

    current = _snapshot()
    for confirmation in current["ptzConfirmations"]:
        confirmation.update(
            {
                "confirmed": False,
                "hasFrame": False,
                "frameRelativePath": "",
                "relativePath": "",
                "metadataRelativePath": "",
                "boundingBoxPixels": [],
                "pixelExtentWidth": 0.0,
                "pixelExtentHeight": 0.0,
            }
        )
    current_result = build_layered_snapshot(current)
    current_track = current_result["tracks"][0]

    assert current_result["ptzConfirmations"] == []
    assert current_track["visualEvidenceStatus"] == "NO_RADAR_CUED_CONFIRMATION"
    assert current_track["visualFrame"]["transportStatus"] == "NO_FRAME"
    assert current_track["visualFrame"]["confirmations"] == []
    serialized = json.dumps(current_result)
    assert all(path not in serialized for path in previous_paths)


def test_low_score_single_radar_record_does_not_create_false_operator_cue() -> None:
    snapshot = _snapshot()
    snapshot["searchRadarDetections"][0]["confidence"] = 0.20
    snapshot["ptzConfirmations"] = []
    result = build_layered_snapshot(snapshot)

    fusion = result["tracks"][0]["fusion"]
    assert fusion["decision"] not in {"DETECTION_ALERT", "CONFIRMED_TRACK"}
    assert fusion["operatorCueActive"] is False


def test_v1_and_v2_remain_accepted_without_long_range_sensor_projection() -> None:
    for schema in ("triad.live_rf_snapshot.v1", "triad.live_rf_snapshot.v2"):
        snapshot = _snapshot()
        snapshot["schemaVersion"] = schema
        snapshot.pop("searchRadarDetections")
        snapshot.pop("ptzConfirmations")
        result = build_layered_snapshot(snapshot)
        assert result["sourceUnrealSchemaVersion"] == schema
        assert not any(
            item.get("sensorType") in {"SEARCH_RADAR", "EO_PTZ", "THERMAL_PTZ"}
            for item in result["sensorNodes"]
        )


def test_unreal_weather_adjusted_ptz_confidence_is_not_discounted_twice() -> None:
    snapshot = deepcopy(_snapshot())
    snapshot["weather"] = {
        "profile": "DenseFog",
        "rainRateMillimetersPerHour": 25.0,
        "visibilityMeters": 600.0,
    }
    result = build_layered_snapshot(snapshot)
    contributions = {
        item["modality"]: item for item in result["tracks"][0]["fusion"]["contributions"]
    }
    eo = contributions["EO_PTZ"]["selectedEvidence"]
    thermal = contributions["THERMAL_PTZ"]["selectedEvidence"]
    assert eo["weatherMultiplierAssumption"] == 1.0
    assert thermal["weatherMultiplierAssumption"] == 1.0
    assert "already contains Unreal weatherConfidenceFactor" in eo["scoreOrigin"]
    assert result["ptzConfirmations"][0]["weatherConfidenceSemantics"] == (
        "already_applied_to_confidence"
    )
