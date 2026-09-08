import json
import os
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path
from unittest.mock import patch
from urllib.error import URLError

from bridge.triad_bridge import (
    BridgeError,
    C2Client,
    TriadBridge,
    build_detection_payloads,
    build_operating_context_payload,
    build_sensor_payloads,
    build_standard_detection_payloads,
    build_standard_sensor_payloads,
    read_snapshot,
    run_once,
    snapshot_age_seconds,
)


def layered_snapshot() -> dict:
    return {
        "schemaVersion": "triad.layered_detection_snapshot.v1",
        "timestampUtc": "2026-08-03T03:42:42.236Z",
        "sourceUnrealSnapshotTimestampUtc": "2026-08-03T03:42:42.236Z",
        "simulationOnly": True,
        "detectionOnly": True,
        "suppliedRFModelsUsed": False,
        "legacyUnrealDetectedRFLinksConsumed": True,
        "rawUnrealDetectedRFLinksConsumed": True,
        "calibratedOperationalSystem": False,
        "actionsTaken": "none",
        "observationAuthority": {
            "trackSeedPolicy": "current_validated_sensor_observations_only",
            "scenarioTargetsUsedAsDetectionSeeds": False,
            "scenarioTargetsUsedForOperationalState": False,
            "rawUnrealDetectedRFLinksConsumed": True,
            "truthRegeneratedRFEnabled": False,
            "truthRegeneratedMmWaveEnabled": False,
            "rfOnlyTruthPositionFallbackEnabled": False,
        },
        "weather": {
            "profile": "HEAVY_RAIN",
            "rainRateMillimetersPerHour": 30,
            "visibilityMeters": 3200,
        },
        "simulationPerimeter": {
            "enabled": True,
            "referenceName": "Singapore_Simulation_Perimeter",
            "minimumLongitudeDegrees": 103.62,
            "maximumLongitudeDegrees": 104.02,
            "minimumLatitudeDegrees": 1.22,
            "maximumLatitudeDegrees": 1.47,
            "legalOrNationalBoundary": False,
            "purpose": "Simulation classification only.",
        },
        "sensorNodes": [
            {
                "sensorId": "triad:West_Sector:rf-wideband",
                "nodeId": "West_Sector",
                "modality": "wideband_rf",
                "latitudeDegrees": 1.321,
                "longitudeDegrees": 103.65,
                "rangeMeters": 30000,
                "band": "0.3-6.0 GHz channel bank",
                "method": "channelized radiometer",
                "status": "ONLINE",
            },
            {
                "sensorId": "triad:West_Sector:mmwave",
                "nodeId": "West_Sector",
                "modality": "mmwave",
                "latitudeDegrees": 1.321,
                "longitudeDegrees": 103.65,
                "rangeMeters": 6000,
                "band": "76-81 GHz FMCW",
                "method": "range-Doppler simulation",
                "status": "ONLINE",
            },
            {
                "sensorId": "triad:West_Sector:rgb",
                "nodeId": "West_Sector",
                "modality": "rgb",
                "latitudeDegrees": 1.321,
                "longitudeDegrees": 103.65,
                "rangeMeters": 10000,
                "band": "visible RGB + depth",
                "method": "OAK detector",
                "status": "OFFLINE",
            },
            {
                "sensorId": "triad:West_Sector:event",
                "nodeId": "West_Sector",
                "modality": "event_camera",
                "latitudeDegrees": 1.321,
                "longitudeDegrees": 103.65,
                "rangeMeters": 10000,
                "band": "proxy event stack",
                "method": "FRED event detector",
                "status": "DEGRADED",
            },
        ],
        "observations": {
            "widebandRFLinks": [
                {
                    "targetId": "Inbound_01",
                    "receiverId": "triad:West_Sector:rf-wideband",
                    "centerFrequencyGHz": 2.412,
                    "rangeMeters": 24752.1,
                    "snrDb": 22.7,
                },
                {
                    "targetId": "Inbound_01",
                    "receiverId": "triad:South_Sector:rf-wideband",
                    "centerFrequencyGHz": 5.825,
                    "rangeMeters": 25500,
                    "snrDb": 18.1,
                },
            ],
            "mmWaveLinks": [],
        },
        "tracks": [
            {
                "trackId": "Inbound_01",
                "targetType": "UAS_SWARM_MEMBER",
                "positionAvailable": False,
                "latitudeDegrees": None,
                "longitudeDegrees": None,
                "heightMeters": None,
                "speedMetersPerSecond": None,
                "headingDegrees": None,
                "airspaceState": "APPROACHING",
                "distanceToPerimeterMeters": 2809.7,
                "approachRateMetersPerSecond": 25.4,
                "timeToSimulationPerimeterSeconds": 110.6,
                "outsideSimulationPerimeter": True,
                "ingressCorridorId": "SOUTH_INBOUND",
                "trackEstimate": {
                    "horizontalOneSigmaMeters": 150,
                    "speedOneSigmaMetersPerSecond": 2.5,
                    "headingOneSigmaDegrees": 5,
                    "method": "position unavailable from RF-only evidence",
                    "truthCoordinatesForwardedToC2": False,
                    "calibratedFieldAccuracyClaimed": False,
                    "truthSeededSimulationSurrogate": False,
                    "measurementDerivedLocalization": False,
                    "uncertaintyEmpiricallyCalibrated": False,
                    "sigmaSemantics": "fixed assumed simulation noise; not measured covariance",
                },
                "scenarioTruth": {
                    "hostile": True,
                    "latitudeDegrees": 9.9,
                    "longitudeDegrees": 9.9,
                    "semantics": "evaluator only",
                },
                "modalitySummary": {
                    "widebandRF": {
                        "detected": True,
                        "detectingNodeCount": 2,
                        "bands": ["2.412 GHz", "5.825 GHz"],
                        "strongestSnrDb": 22.7,
                        "method": "channelized radiometer",
                    },
                    "mmWave": {"detected": False, "detectingNodeCount": 0},
                    "rgb": {"detected": False, "observationCount": 0, "observations": []},
                    "eventCamera": {"detected": False, "observationCount": 0, "observations": []},
                },
                "fusion": {
                    "decision": "PRELIMINARY_RF_CUE",
                    "detectionAlert": False,
                    "preliminaryCue": True,
                    "operatorCueActive": True,
                    "confirmationTier": "PRELIMINARY",
                    "fusedEvidenceScore": 0.82,
                    "scoreSemantics": "evidence index; not probability",
                    "activeModalityFamilyCount": 1,
                    "nearestContributingRangeMeters": 24752.1,
                    "decisionLatencyMilliseconds": 268,
                    "contributions": [
                        {
                            "modality": "wideband_rf",
                            "selectedEvidence": {
                                "nodeId": "West_Sector",
                                "discountedEvidenceScore": 0.82,
                                "latencyMilliseconds": 268,
                                "rangeMeters": 24752.1,
                                "scoreOrigin": "radiometer-v1",
                            },
                        }
                    ],
                },
                "alert": {"active": True, "category": "APPROACH_EARLY_WARNING"},
            }
        ],
    }


def localized_snapshot() -> dict:
    snapshot = layered_snapshot()
    track = snapshot["tracks"][0]
    track.update(
        {
            "positionAvailable": True,
            "latitudeDegrees": 1.1947,
            "longitudeDegrees": 103.835,
            "heightMeters": 420.2,
            "speedMetersPerSecond": 25.4,
            "headingDegrees": 359.7,
        }
    )
    track["trackEstimate"].update(
        {
            "method": "measurement-derived search-radar polar localization",
            "truthSeededSimulationSurrogate": False,
            "measurementDerivedLocalization": True,
        }
    )
    return snapshot


def radar_ptz_snapshot() -> dict:
    snapshot = localized_snapshot()
    timestamp = snapshot["sourceUnrealSnapshotTimestampUtc"]
    node_id = "West_Sector"
    snapshot["sensorNodes"].extend(
        {
            "sensorId": f"{node_id}:{sensor_type}",
            "nodeId": node_id,
            "sensorType": sensor_type,
            "modality": sensor_type,
            "latitudeDegrees": 1.321,
            "longitudeDegrees": 103.65,
            "rangeMeters": 12_000 if sensor_type == "SEARCH_RADAR" else 4_000,
            "band": "simulated search volume" if sensor_type == "SEARCH_RADAR" else "simulated imagery",
            "method": "explicit Unreal v3 simulated sensor runtime",
            "status": "ONLINE",
            "runtimeStatus": "ONLINE",
            "configured": True,
            "enabled": True,
            "azimuthDeg": 85.0,
            "horizontalFovDeg": 100.0 if sensor_type == "SEARCH_RADAR" else 12.0,
        }
        for sensor_type in ("SEARCH_RADAR", "EO_PTZ", "THERMAL_PTZ")
    )
    track = snapshot["tracks"][0]
    track["activeModalities"] = ["SEARCH_RADAR", "EO_PTZ", "THERMAL_PTZ"]
    track["modalityEvidence"] = [
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
            "rangeM": 500.0,
            "score": 0.88,
            "modelId": "must-not-cross-bridge",
        },
        {
            "modality": "THERMAL_PTZ",
            # Deliberately wrong input family: the bridge must derive VISUAL.
            "family": "ACTIVE_RADAR",
            "fusionFamily": "ACTIVE_RADAR",
            "evidenceFamily": "THERMAL_PTZ",
            "detected": True,
            "sensorIds": [f"{node_id}:THERMAL_PTZ"],
            "nodeCount": 1,
            "rangeM": 500.0,
            "score": 0.86,
        },
    ]

    def confirmation(sensor_type: str, directory: str) -> dict:
        return {
            "id": f"ptz:{sensor_type}:Inbound_01:{timestamp}",
            "modality": sensor_type,
            "label": "input label must be normalized",
            "simulated": True,
            "timestampUtc": timestamp,
            "nodeId": node_id,
            "sensorId": f"{node_id}:{sensor_type}",
            "relativePath": f"RadarPtzFrames/{directory}/{node_id}/latest.png",
            "frameRelativePath": f"RadarPtzFrames/{directory}/{node_id}/latest.png",
            "width": 1280,
            "height": 720,
            "hasFrame": True,
            "lineOfSight": True,
            "reticle": {"x": 640, "y": 360, "coordinateSpace": "PIXELS"},
            "boxes": [
                {
                    "id": f"box:Inbound_01:{sensor_type}",
                    "kind": "SIMULATED_SENSOR_CONFIRMATION",
                    "sensorType": sensor_type,
                    "label": "projection truth",
                    "confidence": 0.9,
                    "bbox": [620, 350, 660, 370],
                    "coordinateSpace": "PIXELS",
                    "source": "SIMULATION_PROJECTION",
                }
            ],
        }

    track["visualFrame"] = {
        "transportStatus": "FRAME_AVAILABLE",
        "confirmations": [
            confirmation("EO_PTZ", "eo"),
            confirmation("THERMAL_PTZ", "thermal"),
        ],
    }
    track["fusion"].update(
        {
            "decision": "CONFIRMED_TRACK",
            "detectionAlert": True,
            "preliminaryCue": False,
            "confirmationTier": "CONFIRMED",
            "fusedEvidenceScore": 0.9,
            "activeModalityFamilyCount": 3,
            "nearestContributingRangeMeters": 500.0,
        }
    )
    return snapshot


class TriadMappingTests(unittest.TestCase):
    def test_operating_context_is_available_without_an_active_track(self):
        snapshot = layered_snapshot()
        snapshot["tracks"] = []
        context = build_operating_context_payload(snapshot)
        self.assertIsNotNone(context)
        assert context is not None
        self.assertEqual(context["weather"]["profile"], "HEAVY_RAIN")
        self.assertEqual(context["weather"]["rainRateMmH"], 30.0)
        self.assertFalse(context["simulationPerimeter"]["legalOrNationalBoundary"])
        self.assertTrue(context["detectionOnly"])
        self.assertEqual(
            context["sourceTimestampUtc"],
            snapshot["sourceUnrealSnapshotTimestampUtc"],
        )

    def test_sensor_heartbeat_publishes_operating_context(self):
        class RecordingClient:
            def __init__(self):
                self.posts = []

            def post(self, path, payload):
                self.posts.append((path, payload))
                return {}

            def delete(self, _path, _identifier):
                return None

        client = RecordingClient()
        bridge = TriadBridge(client, api_profile="globe")  # type: ignore[arg-type]
        self.assertEqual(bridge.send_sensor_heartbeats(layered_snapshot()), 4)
        context_posts = [payload for path, payload in client.posts if path == "/api/cuas/context"]
        self.assertEqual(len(context_posts), 1)
        self.assertEqual(context_posts[0]["weather"]["profile"], "HEAVY_RAIN")

    def test_registers_all_layered_sensor_modalities(self):
        payloads = build_sensor_payloads(layered_snapshot())
        self.assertEqual(
            [item["type"] for item in payloads],
            ["WIDEBAND_RF", "MMWAVE", "RGB", "EVENT_CAMERA"],
        )
        self.assertEqual(payloads[0]["id"], "triad:West_Sector:rf-wideband")
        self.assertEqual(payloads[0]["nodeId"], "West_Sector")
        self.assertEqual(payloads[1]["rangeM"], 6000.0)
        self.assertEqual(payloads[0]["observedAtUtc"], "2026-08-03T03:42:42.236Z")
        self.assertEqual(
            [item["reportedStatus"] for item in payloads],
            ["ONLINE", "ONLINE", "OFFLINE", "DEGRADED"],
        )

    def test_standard_sensor_profile_matches_attached_contract(self):
        payloads = build_standard_sensor_payloads(layered_snapshot())
        self.assertEqual([item["type"] for item in payloads], ["RF", "RADAR"])
        allowed = {"id", "type", "name", "rangeM", "band", "lat", "lon"}
        self.assertTrue(all(set(item) <= allowed for item in payloads))
        self.assertTrue(all("reportedStatus" not in item for item in payloads))

    def test_standard_profile_omits_context_and_unavailable_sensor_heartbeats(self):
        class RecordingClient:
            def __init__(self):
                self.posts = []
                self.deletes = []

            def post(self, path, payload):
                self.posts.append((path, payload))
                return {}

            def delete(self, path, identifier):
                self.deletes.append((path, identifier))

        client = RecordingClient()
        bridge = TriadBridge(client, api_profile="standard")  # type: ignore[arg-type]
        bridge.registered_sensor_ids = {
            "triad:West_Sector:rgb",
            "triad:West_Sector:event",
        }
        self.assertEqual(bridge.send_sensor_heartbeats(layered_snapshot()), 2)
        self.assertNotIn("/api/cuas/context", [path for path, _ in client.posts])
        self.assertEqual(
            {identifier for _, identifier in client.deletes},
            {"triad:West_Sector:rgb", "triad:West_Sector:event"},
        )

    def test_missing_or_invalid_modality_health_fails_closed(self):
        snapshot = layered_snapshot()
        snapshot["sensorNodes"][0].pop("status")
        snapshot["sensorNodes"][1]["status"] = "BROKEN"
        payloads = build_sensor_payloads(snapshot)
        self.assertEqual(payloads[0]["reportedStatus"], "OFFLINE")
        self.assertEqual(payloads[1]["reportedStatus"], "OFFLINE")

    def test_registers_radar_ptz_health_and_map_orientation(self):
        snapshot = radar_ptz_snapshot()
        payloads = build_sensor_payloads(snapshot)
        by_type = {item["type"]: item for item in payloads}
        self.assertEqual(len(payloads), 7)
        self.assertEqual(by_type["SEARCH_RADAR"]["id"], "triad:West_Sector:SEARCH_RADAR")
        self.assertEqual(by_type["SEARCH_RADAR"]["reportedStatus"], "ONLINE")
        self.assertEqual(by_type["SEARCH_RADAR"]["rangeM"], 12_000.0)
        self.assertEqual(by_type["SEARCH_RADAR"]["azimuthDeg"], 85.0)
        self.assertEqual(by_type["SEARCH_RADAR"]["horizontalFovDeg"], 100.0)
        self.assertEqual(by_type["EO_PTZ"]["reportedStatus"], "ONLINE")
        self.assertEqual(by_type["THERMAL_PTZ"]["reportedStatus"], "ONLINE")

        snapshot["sensorNodes"][-3]["configured"] = False
        self.assertEqual(
            {item["type"]: item for item in build_sensor_payloads(snapshot)}["SEARCH_RADAR"]["reportedStatus"],
            "OFFLINE",
        )

    def test_forwards_fusion_v3_radar_ptz_and_simulated_frames(self):
        payload = build_detection_payloads(radar_ptz_snapshot())[0]
        self.assertEqual(payload["fusionTier"], "CONFIRMED")
        self.assertEqual(payload["distanceM"], 500.0)
        self.assertEqual(
            payload["activeModalities"],
            ["WIDEBAND_RF", "SEARCH_RADAR", "EO_PTZ", "THERMAL_PTZ"],
        )
        evidence = {item["modality"]: item for item in payload["modalityEvidence"]}
        self.assertEqual(evidence["SEARCH_RADAR"]["family"], "ACTIVE_RADAR")
        self.assertEqual(evidence["EO_PTZ"]["family"], "VISUAL")
        self.assertEqual(evidence["THERMAL_PTZ"]["family"], "VISUAL")
        self.assertNotIn("modelId", evidence["EO_PTZ"])
        self.assertEqual(payload["visualEvidenceStatus"], "RADAR_CUED_CONFIRMATION_AVAILABLE")
        frame = payload["visualFrame"]
        self.assertEqual(frame["transportStatus"], "FRAME_AVAILABLE")
        self.assertEqual(len(frame["confirmations"]), 2)
        self.assertEqual(
            [item["relativePath"] for item in frame["confirmations"]],
            [
                "RadarPtzFrames/eo/West_Sector/latest.png",
                "RadarPtzFrames/thermal/West_Sector/latest.png",
            ],
        )
        box = frame["confirmations"][0]["boxes"][0]
        self.assertEqual(box["kind"], "SIMULATED_SENSOR_CONFIRMATION")
        self.assertEqual(box["source"], "SIMULATION_PROJECTION")
        serialized = json.dumps(payload)
        self.assertNotIn("Inbound_01", serialized)
        self.assertNotIn("must-not-cross-bridge", serialized)
        self.assertNotIn("MODEL_OUTPUT", serialized)

    def test_standard_detection_profile_matches_attached_contract(self):
        payload = build_standard_detection_payloads(radar_ptz_snapshot())[0]
        self.assertEqual(
            set(payload),
            {"id", "drone", "lat", "lon", "alt", "speed", "heading", "rf", "cls", "status", "det", "sources"},
        )
        self.assertEqual(payload["drone"], "UNKNOWN UAS")
        self.assertEqual(payload["det"], "RF+RADAR+EO_IR")
        self.assertEqual(payload["cls"], "SUSPECT")
        self.assertNotIn("distanceM", payload)
        self.assertNotIn("bearingDeg", payload)

    def test_unknown_optional_measurements_are_omitted_not_zeroed(self):
        snapshot = radar_ptz_snapshot()
        track = snapshot["tracks"][0]
        track["heightMeters"] = None
        track["speedMetersPerSecond"] = None
        track["headingDegrees"] = None
        payload = build_standard_detection_payloads(snapshot)[0]
        self.assertNotIn("alt", payload)
        self.assertNotIn("speed", payload)
        self.assertNotIn("heading", payload)

    def test_classification_is_invariant_to_scenario_truth(self):
        classifications = []
        identifiers = []
        for truth in ({"hostile": True}, {"hostile": False}, None):
            snapshot = radar_ptz_snapshot()
            if truth is None:
                snapshot["tracks"][0].pop("scenarioTruth", None)
            else:
                snapshot["tracks"][0]["scenarioTruth"] = truth
            payload = build_detection_payloads(snapshot)[0]
            classifications.append(payload["cls"])
            identifiers.append(payload["id"])
        self.assertEqual(classifications, ["SUSPECT", "SUSPECT", "SUSPECT"])
        self.assertEqual(len(set(identifiers)), 1)

    def test_alert_flag_cannot_promote_single_family_cue_to_suspect(self):
        snapshot = localized_snapshot()
        snapshot["tracks"][0]["fusion"]["detectionAlert"] = True
        payload = build_detection_payloads(snapshot)[0]
        self.assertEqual(payload["fusionTier"], "PRELIMINARY")
        self.assertEqual(payload["cls"], "UNKNOWN")

    def test_radar_ptz_frames_and_box_provenance_fail_closed(self):
        snapshot = radar_ptz_snapshot()
        track = snapshot["tracks"][0]
        track["visualFrame"]["confirmations"] = [
            track["visualFrame"]["confirmations"][0]
        ]
        track["visualFrame"]["confirmations"][0]["relativePath"] = "../outside.png"
        payload = build_detection_payloads(snapshot)[0]
        self.assertEqual(payload["visualEvidenceStatus"], "UNAVAILABLE")
        self.assertNotIn("visualFrame", payload)

        snapshot = radar_ptz_snapshot()
        confirmation = snapshot["tracks"][0]["visualFrame"]["confirmations"][0]
        confirmation["boxes"][0].update(
            {"kind": "MODEL_DETECTION", "source": "MODEL_OUTPUT"}
        )
        snapshot["tracks"][0]["visualFrame"]["confirmations"] = [confirmation]
        payload = build_detection_payloads(snapshot)[0]
        self.assertEqual(payload["visualFrame"]["confirmations"][0]["boxes"], [])
        self.assertNotIn("MODEL_OUTPUT", json.dumps(payload["visualFrame"]))

    def test_unconfirmed_current_snapshot_cannot_surface_previous_ptz_frame(self):
        previous_snapshot = radar_ptz_snapshot()
        previous_payload = build_detection_payloads(previous_snapshot)[0]
        previous_paths = {
            item["relativePath"]
            for item in previous_payload["visualFrame"]["confirmations"]
        }
        self.assertTrue(previous_paths)

        current_snapshot = radar_ptz_snapshot()
        current_confirmations = current_snapshot["tracks"][0]["visualFrame"][
            "confirmations"
        ]
        for confirmation in current_confirmations:
            confirmation.update(
                {
                    "hasFrame": False,
                    "relativePath": "",
                    "frameRelativePath": "",
                    "boxes": [],
                }
            )
        current_payload = build_detection_payloads(current_snapshot)[0]

        self.assertEqual(current_payload["visualEvidenceStatus"], "UNAVAILABLE")
        self.assertNotIn("visualFrame", current_payload)
        serialized = json.dumps(current_payload)
        self.assertTrue(all(path not in serialized for path in previous_paths))

    def test_eo_and_thermal_do_not_count_as_two_independent_families(self):
        snapshot = radar_ptz_snapshot()
        track = snapshot["tracks"][0]
        track["modalitySummary"]["widebandRF"]["detected"] = False
        snapshot["observations"]["widebandRFLinks"] = []
        track["fusion"]["contributions"] = [
            item
            for item in track["fusion"]["contributions"]
            if item.get("modality") != "wideband_rf"
        ]
        payload = build_detection_payloads(snapshot)[0]
        self.assertEqual(payload["fusionTier"], "CORROBORATED")
        families = {
            item["family"]
            for item in payload["modalityEvidence"]
            if item["detected"]
        }
        self.assertEqual(families, {"ACTIVE_RADAR", "VISUAL"})

    def test_rf_only_unlocated_cue_is_not_given_fabricated_position(self):
        snapshot = layered_snapshot()
        self.assertIsNone(snapshot["tracks"][0]["latitudeDegrees"])
        self.assertEqual(build_detection_payloads(snapshot), [])

        # Even poisoned coordinates remain unusable without measurement-derived
        # localization provenance and an explicit positionAvailable flag.
        snapshot["tracks"][0]["latitudeDegrees"] = 9.9
        snapshot["tracks"][0]["longitudeDegrees"] = 9.9
        self.assertEqual(build_detection_payloads(snapshot), [])

    def test_pseudonymizes_truth_bearing_actor_and_corridor_values(self):
        snapshot = localized_snapshot()
        raw_track_id = "HostileEastSwarm_01"
        track = snapshot["tracks"][0]
        old_track_id = track["trackId"]
        track["trackId"] = raw_track_id
        track["ingressCorridorId"] = "SOUTH_INBOUND_FRIENDLY"
        for row in snapshot["observations"]["widebandRFLinks"]:
            if row["targetId"] == old_track_id:
                row["targetId"] = raw_track_id

        payload = build_detection_payloads(snapshot)[0]
        serialized = json.dumps(payload).casefold()
        self.assertRegex(payload["id"], r"^triad:SG-UAS-[0-9A-F]{12}$")
        self.assertEqual(payload["drone"], "UNKNOWN UAS")
        self.assertEqual(payload["ingressCorridorId"], "SOUTH_INBOUND")
        self.assertNotIn(raw_track_id.casefold(), serialized)
        self.assertNotIn("hostile", serialized)
        self.assertNotIn("friendly", serialized)

    def test_labels_multi_node_single_family_rf_as_preliminary(self):
        payload = build_detection_payloads(localized_snapshot())[0]
        rf = next(item for item in payload["modalityEvidence"] if item["modality"] == "WIDEBAND_RF")
        self.assertEqual(rf["nodeCount"], 2)
        self.assertEqual(
            rf["sensorIds"],
            ["triad:South_Sector:rf-wideband", "triad:West_Sector:rf-wideband"],
        )
        self.assertEqual(rf["frequenciesGHz"], [2.412, 5.825])
        self.assertEqual(rf["strongestSnrDb"], 22.7)
        self.assertEqual(payload["rf"], "2.412 GHz / 5.825 GHz")

    def test_current_rf_frequency_fields_are_normalized(self):
        snapshot = localized_snapshot()
        first, second = snapshot["observations"]["widebandRFLinks"]
        first.pop("centerFrequencyGHz")
        first["frequencyGHz"] = 2.412
        second.pop("centerFrequencyGHz")
        second["centerFrequencyHz"] = 5.825e9
        payload = build_detection_payloads(snapshot)[0]
        rf = next(item for item in payload["modalityEvidence"] if item["modality"] == "WIDEBAND_RF")
        self.assertEqual(rf["frequenciesGHz"], [2.412, 5.825])

    def test_duplicate_track_ids_are_forwarded_once(self):
        snapshot = localized_snapshot()
        snapshot["tracks"].append(json.loads(json.dumps(snapshot["tracks"][0])))
        payloads = build_detection_payloads(snapshot)
        self.assertEqual(len(payloads), 1)

    def test_maps_corroborated_mmwave_evidence_without_changing_hostility(self):
        snapshot = localized_snapshot()
        track = snapshot["tracks"][0]
        track["fusion"].update(
            {
                "decision": "DETECTION_ALERT",
                "detectionAlert": True,
                "preliminaryCue": False,
                "confirmationTier": "CORROBORATED",
                "activeModalityFamilyCount": 2,
            }
        )
        track["fusion"]["contributions"].append(
            {
                "modality": "mmwave",
                "selectedEvidence": {
                    "nodeId": "West_Sector",
                    "discountedEvidenceScore": 0.47,
                    "latencyMilliseconds": 74,
                    "rangeMeters": 4803,
                    "scoreOrigin": "fmcw-v1",
                },
            }
        )
        track["modalitySummary"]["mmWave"] = {
            "detected": True,
            "detectingNodeCount": 1,
            "nearestRangeMeters": 4803,
            "maximumClosingVelocityMetersPerSecond": 27.8,
            "method": "77 GHz FMCW",
        }
        snapshot["observations"]["mmWaveLinks"] = [
            {
                "targetId": "Inbound_01",
                "radarId": "triad:West_Sector:mmwave",
                "rangeMeters": 4803,
                "radialVelocityMetersPerSecond": -27.5,
                "snrDb": 9.2,
            }
        ]
        payload = build_detection_payloads(snapshot)[0]
        self.assertEqual(payload["cls"], "SUSPECT")
        self.assertEqual(payload["fusionTier"], "CORROBORATED")
        self.assertFalse(payload["preliminaryCue"])
        mmwave = next(item for item in payload["modalityEvidence"] if item["modality"] == "MMWAVE")
        self.assertEqual(mmwave["rangeM"], 4803.0)
        self.assertEqual(mmwave["radialSpeedMps"], -27.5)
        self.assertEqual(mmwave["closingSpeedMps"], 27.8)

    def test_does_not_forward_inactive_scenario_track(self):
        snapshot = layered_snapshot()
        snapshot["tracks"][0]["fusion"]["operatorCueActive"] = False
        self.assertEqual(build_detection_payloads(snapshot), [])

    def test_forwards_only_actual_model_boxes_and_no_projection_box(self):
        snapshot = localized_snapshot()
        track = snapshot["tracks"][0]
        track["modalitySummary"]["rgb"] = {
            "detected": True,
            "observationCount": 1,
            "associationSemantics": "projection IoU debug association only",
            "observations": [
                {
                    "evidenceId": "rgb:West:frame-9:0",
                    "nodeId": "West_Sector",
                    "timestampUtc": "2026-08-03T03:42:42.200Z",
                    "score": 0.91,
                    "rangeMeters": 642.5,
                    "frameId": "frame-9",
                    "bboxXyxyPixels": [100, 40, 180, 120],
                    "className": "drone",
                    "visualSemantics": "model_detector_bounding_box_on_debug-clean_sensor_frame",
                }
            ],
        }
        track["fusion"]["contributions"].append(
            {
                "modality": "rgb",
                "selectedEvidence": {
                    "nodeId": "West_Sector",
                    "discountedEvidenceScore": 0.81,
                    "scoreOrigin": "oak-rgb-yolo",
                },
            }
        )
        payload = build_detection_payloads(snapshot)[0]
        self.assertEqual(payload["visualEvidenceStatus"], "MODEL_DETECTION_AVAILABLE")
        frame = payload["visualFrame"]
        self.assertEqual(len(frame["modelBoxes"]), 1)
        self.assertEqual(frame["modelBoxes"][0]["kind"], "MODEL_DETECTION")
        self.assertEqual(frame["modelBoxes"][0]["source"], "MODEL_OUTPUT")
        self.assertEqual(frame["modelBoxes"][0]["rangeM"], 642.5)
        self.assertEqual(
            frame["modelBoxes"][0]["rangeSemantics"],
            "SIMULATION_DEPTH_POST_DETECTION",
        )
        self.assertIn("not RF distance", frame["note"])
        self.assertEqual(frame["debugProjectionBoxes"], [])
        self.assertNotIn("imageUrl", frame)

    def test_proxy_event_box_is_diagnostic_only_and_not_an_active_modality(self):
        snapshot = localized_snapshot()
        track = snapshot["tracks"][0]
        track["modalitySummary"]["eventCamera"] = {
            "detected": False,
            "candidateObserved": True,
            "observationCount": 1,
            "fusionEligibleObservationCount": 0,
            "diagnosticOnlyObservationCount": 1,
            "proxyInputUsed": True,
            "observations": [
                {
                    "evidenceId": "event:West:frame-10:0",
                    "nodeId": "West_Sector",
                    "timestampUtc": "2026-08-03T03:42:42.210Z",
                    "score": 0.77,
                    "modelId": "fred-event-proxy-test",
                    "frameId": "frame-10",
                    "bboxXyxyPixels": [90, 30, 170, 110],
                    "className": "item",
                    "visualSemantics": "model_detector_bounding_box_on_debug-clean_proxy-event-frame",
                    "proxyInput": True,
                    "fusionEligible": False,
                    "fusionExclusionReason": "RGB-derived proxy cannot count independently",
                }
            ],
        }

        payload = build_detection_payloads(snapshot)[0]
        self.assertNotIn("EVENT_CAMERA", payload["activeModalities"])
        event = next(
            item for item in payload["modalityEvidence"]
            if item["modality"] == "EVENT_CAMERA"
        )
        self.assertFalse(event["detected"])
        self.assertTrue(event["proxyInput"])
        frame = payload["visualFrame"]
        self.assertEqual(frame["modelBoxes"][0]["sensorType"], "EVENT_CAMERA")
        self.assertTrue(frame["modelBoxes"][0]["label"].startswith("PROXY CANDIDATE |"))
        self.assertIn("excluded from fusion-family counting", frame["note"])

    def test_hard_excludes_supplied_debug_projection_truth_boxes(self):
        snapshot = localized_snapshot()
        snapshot["tracks"][0]["visualEvidence"] = {
            "frameId": "debug-frame",
            "nodeId": "West_Sector",
            "modelBoxes": [
                {
                    "id": "model-1",
                    "sensorType": "RGB",
                    "label": "drone",
                    "confidence": 0.88,
                    "rangeM": 321.25,
                    "bbox": [10, 20, 40, 60],
                    "coordinateSpace": "PIXELS",
                    "kind": "MODEL_DETECTION",
                    "source": "MODEL_OUTPUT",
                },
                {
                    "id": "truth-misbucketed",
                    "sensorType": "RGB",
                    "label": "authored truth",
                    "bbox": [1, 2, 50, 70],
                    "coordinateSpace": "PIXELS",
                    "kind": "DEBUG_PROJECTION",
                    "source": "SIMULATION_PROJECTION_TRUTH",
                }
            ],
            "debugProjectionBoxes": [
                {
                    "id": "truth-1",
                    "sensorType": "RGB",
                    "label": "authored truth",
                    "bbox": [1, 2, 50, 70],
                    "coordinateSpace": "PIXELS",
                }
            ],
        }
        frame = build_detection_payloads(snapshot)[0]["visualFrame"]
        self.assertEqual([box["id"] for box in frame["modelBoxes"]], ["model-1"])
        self.assertEqual(frame["modelBoxes"][0]["rangeM"], 321.25)
        self.assertEqual(
            frame["modelBoxes"][0]["rangeSemantics"],
            "SIMULATION_DEPTH_POST_DETECTION",
        )
        self.assertEqual(frame["debugProjectionBoxes"], [])
        self.assertNotIn("truth-1", json.dumps(frame))
        self.assertNotIn("truth-misbucketed", json.dumps(frame))

    def test_snapshot_contract_fails_closed(self):
        snapshot = layered_snapshot()
        snapshot["simulationOnly"] = False
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "snapshot.json"
            path.write_text(json.dumps(snapshot), encoding="utf-8")
            with self.assertRaises(BridgeError):
                read_snapshot(path)

    def test_snapshot_rejects_unsafe_runtime_flags(self):
        unsafe_values = {
            "suppliedRFModelsUsed": True,
            "legacyUnrealDetectedRFLinksConsumed": False,
            "rawUnrealDetectedRFLinksConsumed": False,
            "calibratedOperationalSystem": True,
            "actionsTaken": "engaged",
        }
        for key, unsafe in unsafe_values.items():
            with self.subTest(key=key), tempfile.TemporaryDirectory() as directory:
                snapshot = layered_snapshot()
                snapshot[key] = unsafe
                path = Path(directory) / "snapshot.json"
                path.write_text(json.dumps(snapshot), encoding="utf-8")
                with self.assertRaisesRegex(BridgeError, "snapshot safety field"):
                    read_snapshot(path)

    def test_snapshot_rejects_truth_authoritative_runtime_flags(self):
        unsafe_values = {
            "scenarioTargetsUsedAsDetectionSeeds": True,
            "scenarioTargetsUsedForOperationalState": True,
            "rawUnrealDetectedRFLinksConsumed": False,
            "truthRegeneratedRFEnabled": True,
            "truthRegeneratedMmWaveEnabled": True,
            "rfOnlyTruthPositionFallbackEnabled": True,
        }
        for key, unsafe in unsafe_values.items():
            with self.subTest(key=key), tempfile.TemporaryDirectory() as directory:
                snapshot = layered_snapshot()
                snapshot["observationAuthority"][key] = unsafe
                path = Path(directory) / "snapshot.json"
                path.write_text(json.dumps(snapshot), encoding="utf-8")
                with self.assertRaisesRegex(BridgeError, "observationAuthority"):
                    read_snapshot(path)

    def test_embedded_source_timestamp_controls_staleness_even_with_fresh_file(self):
        snapshot = layered_snapshot()
        now = 2_000_000_000.0
        snapshot["timestampUtc"] = datetime.fromtimestamp(now - 60, timezone.utc).isoformat().replace("+00:00", "Z")
        snapshot["sourceUnrealSnapshotTimestampUtc"] = datetime.fromtimestamp(now - 2, timezone.utc).isoformat().replace("+00:00", "Z")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "snapshot.json"
            path.write_text(json.dumps(snapshot), encoding="utf-8")
            os.utime(path, (now, now))
            self.assertAlmostEqual(snapshot_age_seconds(snapshot, path, now=now), 60.0, places=3)

    def test_once_mode_refuses_stale_snapshot_and_clears_existing_cues(self):
        snapshot = layered_snapshot()
        old = datetime.fromtimestamp(1_600_000_000, timezone.utc).isoformat().replace("+00:00", "Z")
        snapshot["timestampUtc"] = old
        snapshot["sourceUnrealSnapshotTimestampUtc"] = old

        class FakeBridge:
            active_detection_ids = {"triad:old-cue"}
            cleared = False
            sensors_registered = False

            def discover_existing(self):
                return None

            def clear_detections(self):
                self.cleared = True
                self.active_detection_ids.clear()

            def send_sensor_heartbeats(self, _snapshot, **_kwargs):
                self.sensors_registered = True
                return 4

            def send_detections(self, _snapshot):
                raise AssertionError("stale cues must not be forwarded")

        bridge = FakeBridge()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "snapshot.json"
            path.write_text(json.dumps(snapshot), encoding="utf-8")
            with self.assertRaisesRegex(BridgeError, "registered 4 stale/offline sensors but refused detections"):
                run_once(path, bridge, stale_after=5.0)  # type: ignore[arg-type]
        self.assertTrue(bridge.cleared)
        self.assertTrue(bridge.sensors_registered)

    def test_failed_stale_delete_remains_tracked_for_retry(self):
        snapshot = layered_snapshot()
        snapshot["tracks"][0]["fusion"]["operatorCueActive"] = False

        class DeleteFailingClient:
            def post(self, _path, _payload):
                return {}

            def delete(self, _path, _identifier):
                raise BridgeError("delete failed")

        bridge = TriadBridge(DeleteFailingClient())  # type: ignore[arg-type]
        bridge.active_detection_ids = {"triad:stale"}
        with self.assertRaisesRegex(BridgeError, "delete failed"):
            bridge.send_detections(snapshot)
        self.assertEqual(bridge.active_detection_ids, {"triad:stale"})

    def test_c2_client_retries_transient_connection_failure(self):
        class Response:
            def __enter__(self):
                return self

            def __exit__(self, *_args):
                return False

            def read(self, _limit):
                return b'{"detections":[]}'

        client = C2Client("http://127.0.0.1:3000", 0.1, attempts=2, retry_delay=0.0)
        with patch(
            "bridge.triad_bridge.urlopen",
            side_effect=[URLError("starting"), Response()],
        ) as mocked:
            self.assertEqual(client.get("/api/cuas"), {"detections": []})
        self.assertEqual(mocked.call_count, 2)

    def test_once_mode_clears_existing_cues_when_posting_fails(self):
        snapshot = layered_snapshot()
        now = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
        snapshot["timestampUtc"] = now
        snapshot["sourceUnrealSnapshotTimestampUtc"] = now

        class FailingPostBridge:
            active_detection_ids = {"triad:old-cue"}
            cleared = False

            def discover_existing(self):
                return None

            def send_sensor_heartbeats(self, _snapshot):
                return 4

            def send_detections(self, _snapshot):
                raise BridgeError("post failed")

            def clear_detections(self):
                self.cleared = True
                self.active_detection_ids.clear()

        bridge = FailingPostBridge()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "snapshot.json"
            path.write_text(json.dumps(snapshot), encoding="utf-8")
            with self.assertRaisesRegex(BridgeError, "post failed"):
                run_once(path, bridge, stale_after=5.0)  # type: ignore[arg-type]
        self.assertTrue(bridge.cleared)


class CircularOperatingContextTests(unittest.TestCase):
    def test_operating_context_preserves_geodesic_circle(self):
        snapshot = layered_snapshot()
        snapshot["simulationPerimeter"] = {
            "enabled": True,
            "referenceName": "Istana_1km_Simulation_AOI",
            "geometryType": "wgs84_geodesic_circle",
            "centerLongitudeDegrees": 103.84288055,
            "centerLatitudeDegrees": 1.30709615,
            "radiusMeters": 1000.0,
            "legalOrNationalBoundary": False,
            "purpose": "Simulation classification only.",
        }
        context = build_operating_context_payload(snapshot)
        assert context is not None
        perimeter = context["simulationPerimeter"]
        self.assertEqual(perimeter["geometryType"], "wgs84_geodesic_circle")
        self.assertEqual(perimeter["centerLat"], 1.30709615)
        self.assertEqual(perimeter["centerLon"], 103.84288055)
        self.assertEqual(perimeter["radiusM"], 1000.0)
        self.assertNotIn("minLat", perimeter)
        self.assertFalse(perimeter["legalOrNationalBoundary"])

    def test_invalid_circle_is_omitted_fail_closed(self):
        snapshot = layered_snapshot()
        snapshot["simulationPerimeter"] = {
            "enabled": True,
            "geometryType": "wgs84_geodesic_circle",
            "centerLongitudeDegrees": 103.84288055,
            "centerLatitudeDegrees": 1.30709615,
            "radiusMeters": 0.0,
            "legalOrNationalBoundary": False,
        }
        context = build_operating_context_payload(snapshot)
        assert context is not None
        self.assertNotIn("simulationPerimeter", context)


if __name__ == "__main__":
    unittest.main()
