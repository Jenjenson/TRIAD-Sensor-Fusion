from __future__ import annotations

from datetime import datetime, timezone
from http.client import HTTPConnection
import importlib.util
import json
import os
from pathlib import Path
import sys
from tempfile import TemporaryDirectory
import threading
import unittest


MODULE_PATH = (
    Path(__file__).parents[1]
    / "src"
    / "singapore_sensor_fusion"
    / "c3_bridge.py"
)
SPEC = importlib.util.spec_from_file_location("triad_c3_bridge_test_module", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
bridge = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = bridge
SPEC.loader.exec_module(bridge)


NOW = datetime(2026, 8, 1, 3, 4, 10, tzinfo=timezone.utc)


def _write_jsonl(path: Path, records: list[dict[str, object]], *, partial: bool = False) -> None:
    body = "\n".join(json.dumps(record) for record in records) + "\n"
    if partial:
        body += '{"timestampUtc":"unfinished"'
    path.write_text(body, encoding="utf-8")


def _rf(
    *,
    timestamp: str,
    target: str,
    node: str,
    frequency: float,
    snr: float,
    detected: bool,
) -> dict[str, object]:
    return {
        "timestampUtc": timestamp,
        "simulationSeconds": 2.0,
        "nodeId": node,
        "targetActor": target,
        "emitterId": f"{target}_RF",
        "hostileScenarioTruth": not target.startswith("Friendly"),
        "frequencyGHz": frequency,
        "nodeLongitudeDegrees": 103.86,
        "nodeLatitudeDegrees": 1.286,
        "nodeHeightMeters": 150,
        "targetLongitudeDegrees": 103.865,
        "targetLatitudeDegrees": 1.29,
        "targetHeightMeters": 320,
        "distanceMeters": 640,
        "lineOfSight": True,
        "weatherProfile": "Light Rain",
        "airSimVisualWeatherApplied": True,
        "weatherRainRateMillimetersPerHour": 5,
        "weatherVisibilityMeters": 12000,
        "weatherRFSpecificAttenuationDbPerKm": 0.01 if frequency < 3 else 0.08,
        "weatherRFLossDb": 0.01,
        "receivedPowerDbm": -60,
        "noiseFloorDbm": -96,
        "snrDb": snr,
        "detected": detected,
        "preliminaryCueEvidence": detected,
        "alertEvidence": detected,
    }


class SnapshotBuilderTests(unittest.TestCase):
    def setUp(self) -> None:
        self._original_now = bridge._utc_now
        bridge._utc_now = lambda: NOW

    def tearDown(self) -> None:
        bridge._utc_now = self._original_now

    def _fixture(self, root: Path) -> None:
        _write_jsonl(
            root / "rf_links_old.jsonl",
            [_rf(timestamp="2026-08-01T03:03:00.000Z", target="Old", node="City_Sector", frequency=2.4, snr=1, detected=False)],
        )
        latest_rf = root / "rf_links_latest.jsonl"
        _write_jsonl(
            latest_rf,
            [
                _rf(timestamp="2026-08-01T03:04:08.000Z", target="Hostile_0", node="City_Sector", frequency=2.4, snr=24, detected=True),
                _rf(timestamp="2026-08-01T03:04:08.100Z", target="Hostile_0", node="South_Sector", frequency=5.8, snr=14, detected=True),
                _rf(timestamp="2026-08-01T03:04:08.100Z", target="FriendlyPatrol_01", node="City_Sector", frequency=2.4, snr=18, detected=True),
            ],
            partial=True,
        )
        old = root / "rf_links_old.jsonl"
        # Use explicit nanosecond mtimes so newest-file discovery is deterministic.
        base_ns = 1_775_000_000_000_000_000
        os.utime(old, ns=(base_ns, base_ns))
        os.utime(latest_rf, ns=(base_ns + 1_000_000_000, base_ns + 1_000_000_000))

        _write_jsonl(
            root / "alerts_latest.jsonl",
            [{
                "timestampUtc": "2026-08-01T03:04:08.200Z",
                "alertType": "rf_multinode_preliminary_cue",
                "cueLevel": "preliminary",
                "classification": "unclassified_emitter",
                "hostilityAssessment": "not_inferred",
                "trackId": bridge._operator_rf_track_id("Hostile_0", "Hostile_0_RF"),
                "confirmingNodeIds": ["City_Sector", "South_Sector"],
                "confirmingNodeCount": 2,
                "nearestConfirmingNodeId": "City_Sector",
                "nearestConfirmingNodeDistanceMeters": 640,
                "maxSnrDb": 24,
                "targetLongitudeDegrees": 103.865,
                "targetLatitudeDegrees": 1.29,
                "targetHeightMeters": 320,
                "weatherProfile": "Light Rain",
                "airSimVisualWeatherApplied": True,
                "weatherRainRateMillimetersPerHour": 5,
                "weatherVisibilityMeters": 12000,
                "detectionOnly": True,
                "actionsTaken": "none",
            }],
        )

        frame_dir = root / "frames" / "City_Sector"
        frame_dir.mkdir(parents=True)
        (frame_dir / "frame_000001_demo_rgb.png").write_bytes(b"\x89PNG\r\n\x1a\nfixture")
        (frame_dir / "frame_000001_demo_depth.json").write_text(
            json.dumps({
                "timestampUtc": "2026-08-01T03:04:08.150Z",
                "nodeId": "City_Sector",
                "frameIndex": 1,
                "width": 640,
                "height": 360,
                "weatherProfile": "Light Rain",
                "airSimVisualWeatherApplied": True,
                "targetProjectionSemantics": "component_bounds_frustum_only_no_occlusion_test",
                "targets": [{
                    "actorName": "Hostile_0",
                    "hostileScenarioTruth": True,
                    "bboxXyxyPixels": [10, 20, 30, 40],
                    "distanceMeters": 640,
                    "intersectsFrame": True,
                }],
            }),
            encoding="utf-8",
        )

    def test_snapshot_is_bounded_explicit_and_does_not_invent_fusion_score(self) -> None:
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            self._fixture(root)
            snapshot = bridge.SnapshotBuilder(saved_dir=root, cache_seconds=0).snapshot(force=True)

            self.assertEqual(snapshot["mode"], "live-local")
            self.assertTrue(snapshot["detectionOnly"])
            self.assertEqual(snapshot["actionsTaken"], "none")
            self.assertEqual(snapshot["sources"]["rfLinks"]["invalidLines"], 1)
            self.assertEqual(snapshot["weather"]["visualWeatherVerification"], "verified")
            self.assertEqual(len(snapshot["nodes"]), 8)
            self.assertTrue(all(item["nominalRadiusKm"] == 20.0 for item in snapshot["nodes"]))
            self.assertLessEqual(len(snapshot["tracks"]), bridge.MAX_TRACKS)

            expected_track_id = bridge._operator_rf_track_id("Hostile_0", "Hostile_0_RF")
            cue_track = next(item for item in snapshot["tracks"] if item["id"] == expected_track_id)
            self.assertEqual(cue_track["targetActor"], expected_track_id)
            self.assertEqual(cue_track["disposition"], "UNKNOWN")
            self.assertEqual(cue_track["hostilityAssessment"], "not_inferred")
            self.assertEqual(cue_track["ueRfDecision"], "ALERT")
            self.assertEqual(cue_track["bands"]["rf24"]["maxSnrDb"], 24.0)
            self.assertEqual(cue_track["bands"]["rf58"]["maxSnrDb"], 14.0)
            self.assertEqual(cue_track["fusionV2"]["decision"], "NOT_EVALUATED")
            self.assertNotIn("confidence", cue_track)
            self.assertEqual(cue_track["visualProjectionObservations"][0]["nodeId"], "City_Sector")
            self.assertEqual(snapshot["counts"]["rfPreliminaryCues"], 1)
            self.assertIn("not RGB-D model detections", snapshot["sources"]["frames"][0]["modelDetectionNotice"])
            operator_json = json.dumps(snapshot)
            self.assertNotIn("Hostile_0", operator_json)
            self.assertNotIn("FriendlyPatrol_01", operator_json)

    def test_empty_saved_directory_is_reported_as_unavailable(self) -> None:
        with TemporaryDirectory() as temporary:
            snapshot = bridge.SnapshotBuilder(saved_dir=Path(temporary), cache_seconds=0).snapshot(force=True)
        self.assertEqual(snapshot["mode"], "unavailable")
        self.assertEqual(snapshot["sourceFreshness"]["state"], "unavailable")
        self.assertEqual(snapshot["counts"]["tracks"], 0)

    def test_fresh_atomic_snapshot_is_preferred_over_jsonl(self) -> None:
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            self._fixture(root)
            (root / "latest_rf_snapshot.json").write_text(
                json.dumps({
                    "schemaVersion": "triad.live_rf_snapshot.v2",
                    "timestampUtc": "2026-08-01T03:04:09.500Z",
                    "sampleCadenceSeconds": 0.25,
                    "sampleComplete": True,
                    "detectionOnly": True,
                    "actionsTaken": "none",
                    "calibratedOperationalSystem": False,
                    "calibrationStatus": "uncalibrated_deterministic_simulation_model",
                    "detectedLinksTruncated": False,
                    "rfCueMinimumConfirmingNodes": 1,
                    "simulationPerimeter": {
                        "geometryType": "axis_aligned_wgs84_rectangle",
                        "minimumLongitudeDegrees": 103.62,
                        "maximumLongitudeDegrees": 104.02,
                        "minimumLatitudeDegrees": 1.22,
                        "maximumLatitudeDegrees": 1.47,
                        "boundaryInclusive": True,
                        "legalOrNationalBoundary": False,
                        "distanceMethod": "local_equirectangular_wgs84",
                    },
                    "weather": {
                        "profile": "Monsoon",
                        "airSimVisualWeatherApplied": True,
                        "visualVerificationStatus": "VERIFIED",
                        "rainRateMillimetersPerHour": 80,
                        "visibilityMeters": 3000,
                        "rfSpecificAttenuationDbPerKmAt2_4GHz": 0.01,
                        "rfSpecificAttenuationDbPerKmAt5_8GHz": 0.08,
                    },
                    "sensorNodes": [{
                        "nodeId": "City_Sector",
                        "enabled": True,
                        "latitudeDegrees": 1.286,
                        "longitudeDegrees": 103.86,
                        "heightMeters": 185,
                        "detectionRangeMeters": 20000,
                    }],
                    "detectedRFLinks": [{
                        "timestampUtc": "2026-08-01T03:04:09.500Z",
                        "nodeId": "City_Sector",
                        "targetActor": "Current_Target",
                        "emitterId": "Current_RF",
                        "hostileScenarioTruth": False,
                        "preliminaryCueEvidence": True,
                        "alertEvidence": True,
                        "detected": True,
                        "frequencyGHz": 5.8,
                        "nodeLongitudeDegrees": 103.86,
                        "nodeLatitudeDegrees": 1.286,
                        "nodeHeightMeters": 185,
                        "targetLongitudeDegrees": 103.866,
                        "targetLatitudeDegrees": 1.291,
                        "targetHeightMeters": 350,
                        "slantRangeMeters": 700,
                        "lineOfSight": True,
                        "receivedPowerDbm": -58,
                        "noiseFloorDbm": -96,
                        "snrDb": 38,
                        "weatherRFLossDb": 0.05,
                        "airspaceState": "APPROACHING",
                        "distanceToPerimeterMeters": 8500,
                        "approachRateMetersPerSecond": 80,
                        "headingDegrees": 270,
                        "speedMetersPerSecond": 80,
                        "outsideSimulationPerimeter": True,
                        "ingressCorridorId": "EAST",
                    }],
                    "tracks": [{
                        "targetActor": "Current_Target",
                        "emitterId": "Current_RF",
                        "hostileScenarioTruth": False,
                        "targetLongitudeDegrees": 103.866,
                        "targetLatitudeDegrees": 1.291,
                        "targetHeightMeters": 350,
                        "strongestSnrDb": 38,
                        "nearestSlantRangeMeters": 700,
                        "confirmingNodeIds": ["City_Sector"],
                        "preliminaryCueEvidenceNodeIds": ["City_Sector"],
                        "detectedFrequenciesGHz": [5.8],
                        "rfMultinodePreliminaryCueRuleSatisfied": True,
                        "airspaceState": "APPROACHING",
                        "distanceToPerimeterMeters": 8500,
                        "approachRateMetersPerSecond": 80,
                        "headingDegrees": 270,
                        "speedMetersPerSecond": 80,
                        "outsideSimulationPerimeter": True,
                        "ingressCorridorId": "EAST",
                    }],
                    "scenarioTargets": [{
                        "targetActor": "Current_Target",
                        "hostileScenarioTruth": False,
                        "inboundApproachScenario": True,
                        "kinematicsAvailable": True,
                        "detectedThisSample": True,
                        "detectedLinkCount": 1,
                        "targetLongitudeDegrees": 104.025,
                        "targetLatitudeDegrees": 1.291,
                        "targetHeightMeters": 350,
                        "airspaceState": "APPROACHING",
                        "distanceToPerimeterMeters": 8500,
                        "approachRateMetersPerSecond": 80,
                        "headingDegrees": 270,
                        "speedMetersPerSecond": 80,
                        "outsideSimulationPerimeter": True,
                        "ingressCorridorId": "EAST",
                    }],
                }),
                encoding="utf-8",
            )
            snapshot = bridge.SnapshotBuilder(saved_dir=root, cache_seconds=0).snapshot(force=True)

        self.assertEqual(snapshot["sources"]["selectedRFSource"], "atomic-live-snapshot")
        self.assertTrue(snapshot["sources"]["latestRFSnapshot"]["selected"])
        expected_track_id = bridge._operator_rf_track_id("Current_Target", "Current_RF")
        self.assertEqual(snapshot["tracks"][0]["id"], expected_track_id)
        self.assertEqual(snapshot["tracks"][0]["targetActor"], expected_track_id)
        self.assertEqual(snapshot["tracks"][0]["disposition"], "UNKNOWN")
        self.assertEqual(snapshot["tracks"][0]["hostilityAssessment"], "not_inferred")
        self.assertEqual(snapshot["tracks"][0]["rfCueType"], "rf_multinode_preliminary_cue")
        self.assertEqual(snapshot["tracks"][0]["bands"]["rf58"]["maxSnrDb"], 38.0)
        self.assertEqual(snapshot["tracks"][0]["airspaceState"], "APPROACHING")
        self.assertEqual(snapshot["tracks"][0]["distanceToPerimeterMeters"], 8500.0)
        self.assertEqual(snapshot["tracks"][0]["headingDegrees"], 270.0)
        self.assertTrue(snapshot["tracks"][0]["outsideSimulationPerimeter"])
        self.assertEqual(snapshot["simulationPerimeter"]["minLongitudeDegrees"], 103.62)
        self.assertEqual(snapshot["scenarioTruthSummary"]["approachingTargetCount"], 1)
        self.assertEqual(snapshot["counts"]["outsideDetectedTracks"], 1)
        self.assertEqual(snapshot["weather"]["profile"], "Monsoon")

    def test_v1_migration_recomputes_generic_cue_without_scenario_truth(self) -> None:
        target = "FriendlyLegacyTarget"
        emitter = "FriendlyLegacyEmitter"
        payload = {
            "schemaVersion": "triad.live_rf_snapshot.v1",
            "timestampUtc": "2026-08-01T03:04:09.500Z",
            "alertMinimumConfirmingNodes": 2,
            "weather": {"profile": "Clear"},
            "detectedRFLinks": [
                {
                    "nodeId": node,
                    "targetActor": target,
                    "emitterId": emitter,
                    "hostileScenarioTruth": False,
                    "detected": True,
                    # The old field was truth-gated and must not be trusted.
                    "alertEvidence": False,
                    "frequencyGHz": 2.4,
                    "slantRangeMeters": 900 + index,
                    "snrDb": 12 - index,
                }
                for index, node in enumerate(("West_Sector", "Jurong_Sector"))
            ],
            "tracks": [{
                "targetActor": target,
                "emitterId": emitter,
                "hostileScenarioTruth": False,
                "confirmingNodeIds": ["West_Sector", "Jurong_Sector"],
                "alertEvidenceNodeIds": [],
                "detectionOnlyMultinodeRuleSatisfiedUsingScenarioTruth": False,
                "nearestSlantRangeMeters": 900,
                "strongestSnrDb": 12,
            }],
        }

        links, cues = bridge._records_from_live_snapshot(payload)

        self.assertEqual(len(cues), 1)
        self.assertTrue(all(item["preliminaryCueEvidence"] for item in links))
        cue = cues[0]
        self.assertEqual(cue["alertType"], "rf_multinode_preliminary_cue")
        self.assertEqual(cue["hostilityAssessment"], "not_inferred")
        self.assertEqual(cue["trackId"], bridge._operator_rf_track_id(target, emitter))
        self.assertNotIn("targetActor", cue)
        self.assertNotIn("emitterId", cue)
        self.assertNotIn("hostileScenarioTruth", cue)
        operator_payload = json.dumps({key: value for key, value in cue.items() if not key.startswith("_")}).casefold()
        self.assertNotIn("friendly", operator_payload)
        self.assertNotIn("hostile", operator_payload)

    def test_operator_track_id_is_deterministic_and_opaque(self) -> None:
        first = bridge._operator_rf_track_id("HostileEastSwarm_01", "FriendlyEmitter_5")
        second = bridge._operator_rf_track_id("HostileEastSwarm_01", "FriendlyEmitter_5")
        self.assertEqual(first, second)
        self.assertTrue(bridge._is_operator_rf_track_id(first))
        self.assertNotIn("HOSTILE", first)
        self.assertNotIn("FRIENDLY", first)

    def test_operator_corridor_keeps_geometry_but_drops_identity_label(self) -> None:
        approach = bridge._approach_fields({"ingressCorridorId": "SOUTH_INBOUND_FRIENDLY"})
        self.assertEqual(approach["ingressCorridorId"], "SOUTH_INBOUND")
        self.assertNotIn("FRIENDLY", json.dumps(approach))

    def test_oak_model_boxes_require_exact_node_and_frame_match(self) -> None:
        with TemporaryDirectory() as temporary:
            root = Path(temporary)
            self._fixture(root)
            rgb_path = root / "frames" / "City_Sector" / "frame_000001_demo_rgb.png"
            report_path = root / "oak_rgbd_live_watch_latest.json"
            report_path.write_text(
                json.dumps({
                    "schemaVersion": "1.0",
                    "mode": "bounded_live_unreal_oak_rgbd_watch",
                    "reportState": "running",
                    "stopReason": "running",
                    "processedFrames": [{
                        "nodeId": "City_Sector",
                        "frameId": "frame_000001_demo",
                        "frameIndex": 1,
                        "timestampUtc": "2026-08-01T03:04:08.150Z",
                        "rgbPath": str(rgb_path),
                        "detections": [{
                            "detectorSource": "OAK RGB model",
                            "bboxXyxyPixels": [12, 20, 44, 70],
                            "className": "drone",
                            "confidence": 0.81,
                            "slantRangeMeters": 642.5,
                            "rangeSemantics": "Unreal uint32 SceneDepth simulation estimate",
                        }],
                    }],
                }),
                encoding="utf-8",
            )
            snapshot = bridge.SnapshotBuilder(
                saved_dir=root,
                oak_report_path=report_path,
                cache_seconds=0,
            ).snapshot(force=True)

        frame = next(item for item in snapshot["sources"]["frames"] if item["nodeId"] == "City_Sector")
        self.assertTrue(frame["modelDetectionFrameMatched"])
        self.assertEqual(frame["modelDetectionCount"], 1)
        self.assertEqual(frame["modelDetections"][0]["label"], "drone")
        self.assertEqual(frame["modelDetections"][0]["confidence"], 0.81)
        self.assertEqual(frame["modelDetections"][0]["distanceMeters"], 642.5)
        self.assertIsNone(frame["modelDetections"][0]["targetActor"])
        self.assertTrue(snapshot["sources"]["oakRgbdModel"]["exactFrameMatchRequired"])


class HttpBridgeTests(unittest.TestCase):
    def setUp(self) -> None:
        self._original_now = bridge._utc_now
        bridge._utc_now = lambda: NOW
        self.temporary = TemporaryDirectory()
        self.root = Path(self.temporary.name)
        SnapshotBuilderTests._fixture(self, self.root)
        builder = bridge.SnapshotBuilder(saved_dir=self.root, host="127.0.0.1", port=0, cache_seconds=0)
        self.server = bridge.C3BridgeServer(("127.0.0.1", 0), builder)
        self.port = self.server.server_address[1]
        builder.port = self.port
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self) -> None:
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=2)
        self.temporary.cleanup()
        bridge._utc_now = self._original_now

    def _get(self, path: str) -> tuple[int, dict[str, str], bytes]:
        connection = HTTPConnection("127.0.0.1", self.port, timeout=2)
        connection.request("GET", path)
        response = connection.getresponse()
        result = (response.status, dict(response.getheaders()), response.read())
        connection.close()
        return result

    def test_snapshot_and_frame_endpoints_have_cors_and_no_store(self) -> None:
        status, headers, body = self._get("/api/snapshot")
        self.assertEqual(status, 200)
        self.assertEqual(headers["Access-Control-Allow-Origin"], "*")
        self.assertEqual(headers["Cache-Control"], "no-store")
        payload = json.loads(body)
        self.assertEqual(payload["schemaVersion"], bridge.SNAPSHOT_SCHEMA_VERSION)

        status, headers, body = self._get("/frames/City_Sector")
        self.assertEqual(status, 200)
        self.assertEqual(headers["Content-Type"], "image/png")
        self.assertTrue(body.startswith(b"\x89PNG"))

        status, _, _ = self._get("/frames/../alerts_latest.jsonl")
        self.assertEqual(status, 404)


if __name__ == "__main__":
    unittest.main()
