from __future__ import annotations

from datetime import datetime, timezone
import json
from pathlib import Path

from singapore_sensor_fusion.c3_bridge import SnapshotBuilder


def test_bridge_projects_layered_radar_ptz_confirmation_without_model_claim(
    tmp_path: Path,
) -> None:
    timestamp = datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace(
        "+00:00", "Z"
    )
    node_id = "North_Sector"
    target_id = "InboundScenarioActor"
    raw_snapshot = {
        "schemaVersion": "triad.live_rf_snapshot.v3",
        "sampleComplete": True,
        "detectionOnly": True,
        "actionsTaken": "none",
        "timestampUtc": timestamp,
        "sampleCadenceSeconds": 0.25,
        "weather": {
            "profile": "Clear",
            "rainRateMillimetersPerHour": 0.0,
            "visibilityMeters": 30_000.0,
        },
        "sensorNodes": [
            {
                "nodeId": node_id,
                "latitudeDegrees": 1.435,
                "longitudeDegrees": 103.786,
                "heightMeters": 45.0,
                "detectionRangeMeters": 20_000.0,
                "enabled": True,
            }
        ],
        "scenarioTargets": [
            {
                "targetActor": target_id,
                "targetLatitudeDegrees": 1.44,
                "targetLongitudeDegrees": 103.786,
                "targetHeightMeters": 100.0,
                "airspaceState": "APPROACHING",
                "outsideSimulationPerimeter": True,
                "distanceToPerimeterMeters": 500.0,
            }
        ],
        "detectedRFLinks": [],
        "tracks": [],
        "searchRadarDetections": [],
        "ptzConfirmations": [],
    }
    confirmation = {
        "id": "PTZ-CONFIRM-1",
        "modality": "EO_PTZ",
        "label": "SIM EO RADAR-CUED CONFIRMATION",
        "simulated": True,
        "timestampUtc": timestamp,
        "nodeId": node_id,
        "sensorId": f"{node_id}:EO_PTZ",
        "relativePath": f"RadarPtzFrames/eo/{node_id}/latest.png",
        "frameRelativePath": f"RadarPtzFrames/eo/{node_id}/latest.png",
        "width": 1280,
        "height": 720,
        "reticle": {"x": 640, "y": 360, "coordinateSpace": "PIXELS"},
        "boxes": [
            {
                "id": "box-1",
                "kind": "SIMULATED_SENSOR_CONFIRMATION",
                "sensorType": "EO_PTZ",
                "label": "SIMULATED SENSOR CONFIRMATION",
                "bbox": [620, 350, 660, 370],
                "coordinateSpace": "PIXELS",
                "source": "SIMULATION_PROJECTION",
            }
        ],
        "confidence": 0.9,
        "lineOfSight": True,
        "blockingActor": None,
        "rangeM": 500.0,
        "rangeSemantics": "radar-cued range; not stereo depth",
        "approachState": "APPROACHING",
        "distanceToPerimeterMeters": 500.0,
    }
    layered = {
        "schemaVersion": "triad.layered_detection_snapshot.v1",
        "timestampUtc": timestamp,
        "sourceUnrealSchemaVersion": "triad.live_rf_snapshot.v3",
        "simulationOnly": True,
        "detectionOnly": True,
        "actionsTaken": "none",
        "fusionPolicy": {
            "method": "max_discounted_evidence_independent_family_corroboration_v3"
        },
        "sensorNodes": [
            {
                "nodeId": node_id,
                "sensorId": f"{node_id}:{sensor_type}",
                "sensorType": sensor_type,
                "modality": sensor_type,
                "status": "ONLINE",
                "runtimeStatus": "ONLINE",
                "configured": True,
                "enabled": True,
                "simulated": True,
                "rangeMeters": 5_000.0 if sensor_type == "SEARCH_RADAR" else 2_000.0,
                "detectedObservationCount": 1,
            }
            for sensor_type in ("SEARCH_RADAR", "EO_PTZ", "THERMAL_PTZ")
        ],
        "tracks": [
            {
                "trackId": target_id,
                "latitudeDegrees": 1.44,
                "longitudeDegrees": 103.786,
                "heightMeters": 100.0,
                "airspaceState": "APPROACHING",
                "outsideSimulationPerimeter": True,
                "distanceToPerimeterMeters": 500.0,
                "nearestSensorDistanceMeters": 500.0,
                "activeModalities": ["SEARCH_RADAR", "EO_PTZ", "THERMAL_PTZ"],
                "modalityEvidence": [
                    {
                        "modality": "SEARCH_RADAR",
                        "family": "ACTIVE_RADAR",
                        "fusionFamily": "ACTIVE_RADAR",
                        "evidenceFamily": "SEARCH_RADAR",
                        "detected": True,
                        "sensorIds": [f"{node_id}:SEARCH_RADAR"],
                        "nodeCount": 1,
                        "rangeM": 500.0,
                        "radialSpeedMps": -20.0,
                        "closingSpeedMps": 20.0,
                        "score": 0.9,
                    },
                    {
                        "modality": "EO_PTZ",
                        "family": "VISUAL",
                        "fusionFamily": "VISUAL",
                        "evidenceFamily": "EO_PTZ",
                        "detected": True,
                        "sensorIds": [f"{node_id}:EO_PTZ"],
                        "nodeCount": 1,
                        "score": 0.9,
                        "modelId": None,
                    },
                ],
                "visualFrame": {
                    "transportStatus": "FRAME_AVAILABLE",
                    "confirmations": [confirmation],
                },
                "fusion": {
                    "method": "max_discounted_evidence_independent_family_corroboration_v3",
                    "decision": "CONFIRMED_TRACK",
                    "confirmationTier": "CONFIRMED",
                    "operatorCueActive": True,
                    "fusedEvidenceScore": 0.9,
                    "activeModalityFamilies": [
                        "active_radar",
                        "passive_rf",
                        "visual",
                    ],
                },
            }
        ],
    }
    (tmp_path / "latest_rf_snapshot.json").write_text(
        json.dumps(raw_snapshot), encoding="utf-8"
    )
    (tmp_path / "latest_layered_snapshot.json").write_text(
        json.dumps(layered), encoding="utf-8"
    )

    snapshot = SnapshotBuilder(saved_dir=tmp_path).snapshot(force=True)

    assert snapshot["fusion"]["liveEvaluationAvailable"] is True
    assert snapshot["fusion"]["status"] == "EVALUATED"
    assert len(snapshot["sensorHealth"]) == 3
    assert snapshot["nodes"][0]["state"] == "tracking"
    track = snapshot["tracks"][0]
    assert track["targetActor"].startswith("RF-CUE-")
    assert target_id not in json.dumps(track)
    assert track["activeModalities"] == ["SEARCH_RADAR", "EO_PTZ", "THERMAL_PTZ"]
    assert track["fusionV3"]["decision"] == "CONFIRMED_TRACK"
    assert track["visualEvidenceStatus"] == "RADAR_CUED_CONFIRMATION_AVAILABLE"
    assert track["visualFrame"]["transportStatus"] == "FRAME_AVAILABLE"
    box = track["visualFrame"]["confirmations"][0]["boxes"][0]
    assert box["kind"] == "SIMULATED_SENSOR_CONFIRMATION"
    assert box["source"] == "SIMULATION_PROJECTION"
