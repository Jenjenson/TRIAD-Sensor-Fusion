"""Contract tests for the TRIAD fusion API.

Written as ``unittest.TestCase`` so the repository's documented
``python -m unittest`` command collects them.
"""

from __future__ import annotations

from copy import deepcopy
from datetime import datetime, timedelta, timezone
import unittest

from fastapi.testclient import TestClient

from service.app import Settings, create_app
from service.demo_swarm import InProcessTransport, run_scenario, scenarios

AS_OF = datetime(2026, 8, 3, 4, 0, tzinfo=timezone.utc)
TIMESTAMP = "2026-08-03T04:00:00Z"
PERIMETER = {
    "enabled": True,
    "minimumLongitudeDegrees": 103.70,
    "maximumLongitudeDegrees": 104.00,
    "minimumLatitudeDegrees": 1.20,
    "maximumLatitudeDegrees": 1.3020,
    "phaseRateDeadbandMetersPerSecond": 0.25,
}


def node_payload(node_id: str, *, index: int = 0) -> dict:
    return {
        "nodeId": node_id,
        "latitudeDegrees": 1.30,
        "longitudeDegrees": 103.85 + index * 0.002,
        "heightMeters": 40.0,
        "runtimeStatus": "ONLINE",
        "detectionRangeMeters": 20_000.0,
        "supportedFrequenciesGHz": [2.437],
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


def rf_payload(node_id: str, target: str, *, timestamp: str = TIMESTAMP) -> dict:
    return {
        "nodeId": node_id,
        "targetActor": target,
        "timestampUtc": timestamp,
        "frequencyGHz": 2.437,
        "slantRangeMeters": 500.0,
        "receivedPowerDbm": -80.0,
        "noiseFloorDbm": -96.0,
        "snrDb": 16.0,
        "lineOfSight": True,
        "detected": True,
    }


def radar_payload(node_id: str, target: str, *, timestamp: str = TIMESTAMP, confidence: float = 0.95) -> dict:
    return {
        "nodeId": node_id,
        "targetActor": target,
        "trackId": f"RT-{target}",
        "timestampUtc": timestamp,
        "rangeMeters": 500.0,
        "bearingDegrees": 0.0,
        "elevationDegrees": 6.84,
        "radialVelocityMetersPerSecond": -20.0,
        "confidence": confidence,
        "lineOfSight": True,
        "radarCrossSectionSquareMeters": 0.03,
        "rangeEnvelopeMeters": 5_000.0,
        "elevationFieldOfRegardDegrees": 60.0,
    }


def ptz_payload(node_id: str, target: str, sensor_type: str, *, timestamp: str = TIMESTAMP) -> dict:
    return {
        "nodeId": node_id,
        "targetActor": target,
        "trackId": f"RT-{target}",
        "sensorType": sensor_type,
        "timestampUtc": timestamp,
        "rangeMeters": 500.0,
        "confidence": 0.92,
        "cueAgeSeconds": 0.08,
        "confirmed": True,
        "lineOfSight": True,
        "boundingBoxPixels": [626.0, 350.0, 654.0, 370.0],
        "weatherConfidenceFactor": 1.0,
    }


def producer_snapshot(targets: tuple[str, ...] = ("Inbound_01",)) -> dict:
    nodes = [node_payload("Node_A"), node_payload("Node_B", index=1)]
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
        "simulationPerimeter": PERIMETER,
        "sensorNodes": nodes,
        "scenarioTargets": [],
        "detectedRFLinks": [
            rf_payload(node["nodeId"], target) for node in nodes for target in targets
        ],
        "searchRadarDetections": [
            {
                **radar_payload("Node_A", target),
                "kind": "SIMULATED_SENSOR_DETECTION",
                "source": "ANALYTIC_SEARCH_RADAR",
                "sensorId": "Node_A:SEARCH_RADAR",
                "sensorType": "SEARCH_RADAR",
                "simulated": True,
                "calibratedDetector": False,
                "detectionOnly": True,
            }
            for target in targets
        ],
        "ptzConfirmations": [
            {
                **ptz_payload("Node_A", target, sensor_type),
                "kind": "SIMULATED_SENSOR_CONFIRMATION",
                "source": "SIMULATION_PROJECTION",
                "confirmationMethod": "SIMULATION_PROJECTION_TRUTH",
                "boxSource": "DEBUG_PROJECTION",
                "simulated": True,
                "calibratedDetector": False,
                "actionsTaken": "none",
                "sensorId": f"Node_A:{sensor_type}",
                "modality": "EO_VISIBLE" if sensor_type == "EO_PTZ" else "THERMAL_SYNTHETIC",
                "radarCueSensorId": "Node_A:SEARCH_RADAR",
                "slewState": "SETTLED",
                "hasFrame": True,
                "syntheticThermal": sensor_type == "THERMAL_PTZ",
                "imageWidthPixels": 1280,
                "imageHeightPixels": 720,
                "fovDegrees": 8.0,
                "pixelExtentWidth": 28.0,
                "pixelExtentHeight": 20.0,
                "frameRelativePath": (
                    f"RadarPtzFrames/{'eo' if sensor_type == 'EO_PTZ' else 'thermal'}/Node_A/latest.png"
                ),
                "metadataRelativePath": (
                    f"RadarPtzFrames/{'eo' if sensor_type == 'EO_PTZ' else 'thermal'}/Node_A/latest.json"
                ),
            }
            for target in targets
            for sensor_type in ("EO_PTZ", "THERMAL_PTZ")
        ],
    }


class FusionAPITestBase(unittest.TestCase):
    def setUp(self) -> None:
        self.client = TestClient(create_app(Settings(stale_after_s=1e9)))

    def ingest_full_stack(self, targets: tuple[str, ...]) -> None:
        nodes = [node_payload("Node_A"), node_payload("Node_B", index=1)]
        self.assertEqual(
            self.client.post("/v1/observations/nodes", json={"nodes": nodes}).status_code, 200
        )
        self.assertEqual(
            self.client.put(
                "/v1/environment", json={"simulationPerimeter": PERIMETER}
            ).status_code,
            200,
        )
        links = [rf_payload(node["nodeId"], target) for node in nodes for target in targets]
        self.assertEqual(
            self.client.post("/v1/observations/rf", json={"links": links}).status_code, 200
        )
        self.assertEqual(
            self.client.post(
                "/v1/observations/search-radar",
                json={"detections": [radar_payload("Node_A", target) for target in targets]},
            ).status_code,
            200,
        )
        self.assertEqual(
            self.client.post(
                "/v1/observations/ptz",
                json={
                    "confirmations": [
                        ptz_payload("Node_A", target, sensor_type)
                        for target in targets
                        for sensor_type in ("EO_PTZ", "THERMAL_PTZ")
                    ]
                },
            ).status_code,
            200,
        )

    def run_buffer(self, as_of: datetime = AS_OF) -> dict:
        response = self.client.post(
            "/v1/fusion/run", params={"as_of": as_of.isoformat().replace("+00:00", "Z")}
        )
        self.assertEqual(response.status_code, 200, response.text)
        return response.json()


class SnapshotIngestTests(FusionAPITestBase):
    def test_three_families_confirm_one_track_and_one_c2_detection(self) -> None:
        response = self.client.post("/v1/fusion/snapshot", json=producer_snapshot())
        self.assertEqual(response.status_code, 200, response.text)
        body = response.json()
        self.assertEqual(body["trackCount"], 1)
        track = body["tracks"][0]
        self.assertEqual(track["trackId"], "Inbound_01")
        self.assertEqual(track["confirmationTier"], "CONFIRMED")
        self.assertEqual(track["activeModalityFamilyCount"], 3)
        self.assertTrue(track["positionAvailable"])
        self.assertEqual(body["c2DetectionCount"], 1)
        self.assertEqual(body["detections"][0]["fusionTier"], "CONFIRMED")
        self.assertTrue(body["simulationOnly"])
        self.assertTrue(body["detectionOnly"])
        self.assertEqual(body["actionsTaken"], "none")
        self.assertFalse(body["numericScoreStacking"])

    def test_c2_detection_id_is_a_pseudonym_not_the_actor_name(self) -> None:
        body = self.client.post("/v1/fusion/snapshot", json=producer_snapshot()).json()
        detection_id = body["detections"][0]["id"]
        self.assertTrue(detection_id.startswith("triad:"))
        self.assertNotIn("Inbound_01", detection_id)

    def test_swarm_emits_exactly_one_track_and_cue_per_drone(self) -> None:
        targets = tuple(f"Swarm_{index:02d}" for index in range(1, 13))
        body = self.client.post(
            "/v1/fusion/snapshot", json=producer_snapshot(targets)
        ).json()
        self.assertEqual(body["trackCount"], len(targets))
        self.assertEqual(body["c2DetectionCount"], len(targets))
        self.assertEqual({track["trackId"] for track in body["tracks"]}, set(targets))
        identifiers = [detection["id"] for detection in body["detections"]]
        self.assertEqual(len(identifiers), len(set(identifiers)))
        self.assertTrue(
            all(track["confirmationTier"] == "CONFIRMED" for track in body["tracks"])
        )

    def test_unsupported_schema_fails_closed_with_the_runtime_reason(self) -> None:
        snapshot = producer_snapshot()
        snapshot["schemaVersion"] = "triad.live_rf_snapshot.v9"
        response = self.client.post("/v1/fusion/snapshot", json=snapshot)
        self.assertEqual(response.status_code, 422)
        detail = response.json()["detail"]
        self.assertTrue(detail["failedClosed"])
        self.assertIn("unsupported Unreal snapshot schema", detail["reason"])

    def test_incomplete_sample_is_refused(self) -> None:
        snapshot = producer_snapshot()
        snapshot["sampleComplete"] = False
        response = self.client.post("/v1/fusion/snapshot", json=snapshot)
        self.assertEqual(response.status_code, 422)
        self.assertIn("incomplete", response.json()["detail"]["reason"])

    def test_stale_radar_cue_rejects_dependent_ptz_with_a_reason(self) -> None:
        snapshot = deepcopy(producer_snapshot())
        stale = (AS_OF - timedelta(seconds=30)).isoformat().replace("+00:00", "Z")
        for record in snapshot["searchRadarDetections"]:
            record["timestampUtc"] = stale
        body = self.client.post("/v1/fusion/snapshot", json=snapshot).json()
        self.assertEqual(len(body["rejectedEvidence"]["searchRadar"]), 1)
        self.assertEqual(len(body["rejectedEvidence"]["ptz"]), 2)
        reasons = {row["reason"] for row in body["rejectedEvidence"]["ptz"]}
        self.assertTrue(any("radar cue" in reason for reason in reasons))
        track = body["tracks"][0]
        self.assertEqual(track["confirmationTier"], "PRELIMINARY")
        self.assertFalse(track["positionAvailable"])
        self.assertEqual(body["c2DetectionCount"], 0)


class ObservationIngestTests(FusionAPITestBase):
    def test_per_sensor_ingest_matches_whole_snapshot_ingest(self) -> None:
        self.ingest_full_stack(("Inbound_01",))
        buffered = self.run_buffer()
        direct = self.client.post("/v1/fusion/snapshot", json=producer_snapshot()).json()
        for body in (buffered, direct):
            self.assertEqual(body["trackCount"], 1)
            self.assertEqual(body["tracks"][0]["confirmationTier"], "CONFIRMED")
            self.assertEqual(body["c2DetectionCount"], 1)
        self.assertEqual(
            buffered["tracks"][0]["fusedEvidenceScore"],
            direct["tracks"][0]["fusedEvidenceScore"],
        )

    def test_derived_identifiers_and_frame_paths_satisfy_the_allow_list(self) -> None:
        self.ingest_full_stack(("Inbound_01",))
        body = self.run_buffer()
        self.assertEqual(body["rejectedEvidence"]["ptz"], [])
        confirmations = body["tracks"][0]["visualFrame"]["confirmations"]
        self.assertTrue(confirmations)
        for confirmation in confirmations:
            self.assertTrue(
                str(confirmation["frameRelativePath"]).startswith("RadarPtzFrames/")
            )

    def test_expired_observations_are_reported_not_silently_dropped(self) -> None:
        self.ingest_full_stack(("Inbound_01",))
        body = self.run_buffer(AS_OF + timedelta(seconds=5))
        self.assertEqual(body["trackCount"], 0)
        self.assertEqual(body["c2DetectionCount"], 0)
        self.assertEqual(len(body["rejectedEvidence"]["rf"]), 2)
        self.assertTrue(
            all("stale" in row["reason"] for row in body["rejectedEvidence"]["rf"])
        )

    def test_observation_endpoints_cannot_inject_scenario_truth(self) -> None:
        self.ingest_full_stack(("Inbound_01",))
        hostile = {**rf_payload("Node_A", "Ghost_Target"), "hostileScenarioTruth": True}
        self.client.post("/v1/observations/rf", json={"links": [hostile]})
        body = self.run_buffer()
        snapshot = self.client.get("/v1/snapshot").json()
        self.assertEqual(snapshot["observationAuthority"]["scenarioTargetsUsedAsDetectionSeeds"], False)
        self.assertEqual(snapshot["summary"]["scenarioTargetCount"], 0)
        for track in snapshot["tracks"]:
            self.assertFalse(track["scenarioTruth"]["available"])
            self.assertIsNone(track["scenarioTruth"]["hostile"])
            self.assertFalse(track["alert"]["hostilityInferredBySensors"])
        ghost = next(track for track in body["tracks"] if track["trackId"] == "Ghost_Target")
        self.assertEqual(ghost["confirmationTier"], "UNCONFIRMED")

    def test_rgb_derived_proxy_event_cannot_raise_a_fusion_tier(self) -> None:
        self.ingest_full_stack(("Inbound_01",))
        response = self.client.post(
            "/v1/observations/visual-model",
            json={
                "detections": [
                    {
                        "modality": "EVENT_CAMERA",
                        "evidenceId": "event:Node_A:1",
                        "targetId": "Inbound_01",
                        "nodeId": "Node_A",
                        "timestampUtc": TIMESTAMP,
                        "score": 0.99,
                        "modelId": "fred_event_yolo",
                        "proxyInput": True,
                        "debugVisualsExcludedFromSensorCapture": True,
                        "fusionEligible": True,
                    }
                ]
            },
        )
        self.assertEqual(response.status_code, 200, response.text)
        body = self.run_buffer()
        track = body["tracks"][0]
        self.assertNotIn("event_camera", track["activeModalities"])
        self.assertEqual(track["activeModalityFamilyCount"], 3)

    def test_clearing_the_buffer_yields_an_empty_but_valid_sample(self) -> None:
        self.ingest_full_stack(("Inbound_01",))
        self.assertEqual(self.client.delete("/v1/observations").status_code, 200)
        body = self.run_buffer()
        self.assertEqual(body["trackCount"], 0)
        self.assertEqual(body["c2DetectionCount"], 0)
        self.assertEqual(body["summary"]["sensorSiteCount"], 0)

    def test_transport_validation_rejects_naive_timestamps(self) -> None:
        payload = {**rf_payload("Node_A", "Inbound_01"), "timestampUtc": "2026-08-03T04:00:00"}
        response = self.client.post("/v1/observations/rf", json={"links": [payload]})
        self.assertEqual(response.status_code, 422)


class DashboardProjectionTests(FusionAPITestBase):
    def test_c2_reads_fail_closed_before_any_fusion_has_run(self) -> None:
        for path in ("/v1/dashboard", "/v1/tracks", "/v1/c2/detections", "/v1/c2/sensors", "/v1/snapshot"):
            with self.subTest(path=path):
                response = self.client.get(path)
                self.assertEqual(response.status_code, 503)
                self.assertIn("reason", response.json()["detail"])

    def test_stale_last_sample_is_refused_rather_than_served(self) -> None:
        client = TestClient(create_app(Settings(stale_after_s=1.0)))
        self.assertEqual(
            client.post("/v1/fusion/snapshot", json=producer_snapshot()).status_code, 200
        )
        response = client.get("/v1/c2/detections")
        self.assertEqual(response.status_code, 503)
        self.assertEqual(response.json()["detail"]["error"], "fused sample is stale")

    def test_sensor_and_context_projections_are_served_after_fusion(self) -> None:
        self.client.post("/v1/fusion/snapshot", json=producer_snapshot())
        sensors = self.client.get("/v1/c2/sensors").json()
        self.assertGreater(sensors["c2SensorCount"], 0)
        modalities = {sensor["type"] for sensor in sensors["sensors"]}
        self.assertIn("SEARCH_RADAR", modalities)
        self.assertIn("WIDEBAND_RF", modalities)
        context = self.client.get("/v1/c2/context").json()
        self.assertIsNotNone(context["operatingContext"])

    def test_standard_profile_emits_only_the_basic_contract(self) -> None:
        response = self.client.post(
            "/v1/fusion/snapshot", json=producer_snapshot(), params={"api_profile": "standard"}
        )
        self.assertEqual(response.status_code, 200, response.text)
        body = response.json()
        self.assertEqual(body["apiProfile"], "standard")
        self.assertIsNone(body["operatingContext"])
        self.assertNotIn("modalityEvidence", body["detections"][0])

    def test_health_reports_buffer_state_and_safety_declarations(self) -> None:
        body = self.client.get("/healthz").json()
        self.assertEqual(body["status"], "ok")
        self.assertTrue(body["simulationOnly"])
        self.assertTrue(body["detectionOnly"])
        self.assertFalse(body["engagementLogicPresent"])
        self.assertFalse(body["lastFusion"]["available"])
        self.ingest_full_stack(("Inbound_01",))
        body = self.client.get("/healthz").json()
        self.assertEqual(body["buffer"]["sensorNodes"], 2)
        self.assertEqual(body["buffer"]["rfLinks"], 2)
        self.assertEqual(body["buffer"]["ptzConfirmations"], 2)

    def test_publish_is_disabled_until_a_c2_url_is_configured(self) -> None:
        self.client.post("/v1/fusion/snapshot", json=producer_snapshot())
        response = self.client.post("/v1/c2/publish")
        self.assertEqual(response.status_code, 409)
        self.assertIn("TRIAD_C2_URL", response.json()["detail"]["reason"])


class SwarmDemoOracleTests(unittest.TestCase):
    """Guard the demo harness itself so its verdicts stay trustworthy."""

    @classmethod
    def setUpClass(cls) -> None:
        transport = InProcessTransport()
        as_of = datetime.now(timezone.utc)
        cls.results = {
            scenario.name: run_scenario(transport, scenario, as_of) for scenario in scenarios()
        }

    def test_scenarios_without_a_known_gap_are_all_correct(self) -> None:
        for name, result in self.results.items():
            if result.scenario.known_gap:
                continue
            with self.subTest(scenario=name):
                self.assertTrue(
                    result.passed,
                    f"{name} regressed: {[finding.detail for finding in result.findings]}",
                )

    def test_one_drone_under_two_labels_is_reported_as_a_duplicate(self) -> None:
        result = self.results["label_disagreement"]
        self.assertFalse(result.passed)
        self.assertIn("duplicate", {finding.kind for finding in result.findings})
        self.assertEqual(len(result.tracks), 4)

    def test_two_drones_under_one_label_are_reported_as_merged(self) -> None:
        result = self.results["shared_label_two_drones"]
        self.assertFalse(result.passed)
        self.assertIn("merged", {finding.kind for finding in result.findings})
        self.assertEqual(len(result.tracks), 1)


if __name__ == "__main__":
    unittest.main()
