from __future__ import annotations

from datetime import datetime, timezone
import json
from pathlib import Path
import tempfile
import unittest

from singapore_sensor_fusion.fusion_v3 import (
    LayeredEvidence,
    LayeredModality,
    fuse_layered_evidence,
)
from singapore_sensor_fusion.layered_runtime import (
    build_layered_snapshot,
    load_event_evidence,
    load_rgb_evidence,
    load_rgb_evidence_reports,
    load_rgb_sensor_health_reports,
)
from singapore_sensor_fusion.mmwave import MmWaveRadar, simulate_mmwave_detection
from singapore_sensor_fusion.wideband_rf import (
    RFEmitterProfile,
    WidebandChannel,
    WidebandReceiver,
    simulate_wideband_link,
)


AS_OF = datetime(2026, 8, 3, 4, 0, tzinfo=timezone.utc)


def evidence(
    evidence_id: str,
    node_id: str,
    modality: LayeredModality,
    *,
    score: float = 0.9,
) -> LayeredEvidence:
    return LayeredEvidence(
        evidence_id=evidence_id,
        target_track_id="track-1",
        node_id=node_id,
        modality=modality,
        timestamp=AS_OF,
        raw_score=score,
        source_reliability=1.0,
        weather_multiplier=1.0,
        latency_ms=100.0,
        range_m=1_000.0,
        correlation_group=f"{modality.value}:{node_id}",
        score_origin="test",
    )


class LayeredFusionTests(unittest.TestCase):
    def test_two_rf_nodes_create_only_a_preliminary_cue(self) -> None:
        result = fuse_layered_evidence(
            (
                evidence("rf-1", "west", LayeredModality.WIDEBAND_RF),
                evidence("rf-2", "east", LayeredModality.WIDEBAND_RF),
            ),
            as_of=AS_OF,
        )
        self.assertEqual(result["decision"], "PRELIMINARY_RF_CUE")
        self.assertTrue(result["preliminaryCue"])
        self.assertTrue(result["operatorCueActive"])
        self.assertFalse(result["detectionAlert"])
        self.assertEqual(result["confirmationTier"], "PRELIMINARY")

    def test_two_independent_modalities_create_a_corroborated_detection(self) -> None:
        result = fuse_layered_evidence(
            (
                evidence("rf-1", "west", LayeredModality.WIDEBAND_RF),
                evidence("radar-1", "west", LayeredModality.MMWAVE),
            ),
            as_of=AS_OF,
        )
        self.assertEqual(result["decision"], "DETECTION_ALERT")
        self.assertFalse(result["preliminaryCue"])
        self.assertTrue(result["detectionAlert"])
        self.assertTrue(result["operatorCueActive"])
        self.assertEqual(
            result["activeModalityFamilies"],
            ["active_radar", "passive_rf"],
        )

    def test_rgb_and_event_share_one_visual_family(self) -> None:
        result = fuse_layered_evidence(
            (
                evidence("rgb-1", "city", LayeredModality.RGB),
                evidence("event-1", "city", LayeredModality.EVENT_CAMERA),
            ),
            as_of=AS_OF,
        )
        self.assertEqual(result["activeModalities"], ["event_camera", "rgb"])
        self.assertEqual(result["activeModalityFamilies"], ["visual"])
        self.assertEqual(result["activeModalityFamilyCount"], 1)
        self.assertEqual(result["decision"], "HOLD_FOR_CORROBORATION")
        self.assertFalse(result["operatorCueActive"])

    def test_radar_cued_eo_and_thermal_remain_two_independent_families(self) -> None:
        result = fuse_layered_evidence(
            (
                evidence("search-radar", "north", LayeredModality.SEARCH_RADAR),
                evidence("eo-ptz", "north", LayeredModality.EO_PTZ),
                evidence("thermal-ptz", "north", LayeredModality.THERMAL_PTZ),
            ),
            as_of=AS_OF,
        )
        self.assertEqual(result["activeModalityFamilyCount"], 2)
        self.assertEqual(result["activeModalityFamilies"], ["active_radar", "visual"])
        self.assertEqual(result["decision"], "DETECTION_ALERT")

    def test_passive_rf_plus_search_radar_plus_ptz_can_confirm(self) -> None:
        result = fuse_layered_evidence(
            (
                evidence("rf", "west", LayeredModality.WIDEBAND_RF),
                evidence("search-radar", "north", LayeredModality.SEARCH_RADAR),
                evidence("eo-ptz", "north", LayeredModality.EO_PTZ),
                evidence("thermal-ptz", "north", LayeredModality.THERMAL_PTZ),
            ),
            as_of=AS_OF,
        )
        self.assertEqual(result["activeModalityFamilyCount"], 3)
        self.assertEqual(result["decision"], "CONFIRMED_TRACK")

    def test_one_rf_node_is_held_for_corroboration(self) -> None:
        result = fuse_layered_evidence(
            (evidence("rf-1", "west", LayeredModality.WIDEBAND_RF),),
            as_of=AS_OF,
        )
        self.assertEqual(result["decision"], "HOLD_FOR_CORROBORATION")
        self.assertFalse(result["operatorCueActive"])


class PhysicsLayerTests(unittest.TestCase):
    def test_wideband_non_24_58_channel_detects_without_supplied_model(self) -> None:
        receiver = WidebandReceiver(
            receiver_id="west",
            channels=(WidebandChannel(433.92e6, false_alarm_probability=1e-7),),
        )
        result = simulate_wideband_link(
            target_id="target",
            receiver=receiver,
            emitter=RFEmitterProfile("emitter", (433.92e6,), duty_cycle=1.0),
            range_m=10_000.0,
            timestamp_s=1.0,
        )[0]
        self.assertTrue(result.detected)
        self.assertFalse(result.supplied_rf_models_used)
        self.assertEqual(result.active_epoch_count, 5)
        self.assertLessEqual(result.end_to_end_latency_ms, 500.0)

    def test_monsoon_stress_reduces_mmwave_confirmation_range(self) -> None:
        radar = MmWaveRadar("east")
        clear = simulate_mmwave_detection(
            target_id="target",
            radar=radar,
            east_m=400.0,
            north_m=0.0,
            up_m=0.0,
            target_speed_m_s=20.0,
            target_heading_deg=270.0,
            radar_cross_section_m2=0.03,
            rain_rate_mm_h=0.0,
        )
        monsoon = simulate_mmwave_detection(
            target_id="target",
            radar=radar,
            east_m=400.0,
            north_m=0.0,
            up_m=0.0,
            target_speed_m_s=20.0,
            target_heading_deg=270.0,
            radar_cross_section_m2=0.03,
            rain_rate_mm_h=50.0,
        )
        self.assertTrue(clear.detected)
        self.assertFalse(monsoon.detected)
        self.assertGreater(monsoon.rain_loss_db, 10.0)


class VisualEvidenceGateTests(unittest.TestCase):
    def _write_fixture(self, root: Path, *, capture_clean: bool) -> Path:
        metadata_path = root / "frame_depth.json"
        metadata_path.write_text(
            json.dumps(
                {
                    "debugVisualsExcludedFromSensorCapture": capture_clean,
                    "targets": [
                        {
                            "actorName": "track-1",
                            "intersectsFrame": True,
                            "bboxXyxyPixels": [100, 100, 120, 120],
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )
        report_path = root / "report.json"
        report_path.write_text(
            json.dumps(
                {
                    "processedFrames": [
                        {
                            "nodeId": "city",
                            "frameId": "frame-1",
                            "timestampUtc": "2026-08-03T04:00:00Z",
                            "rgbPath": str(root / "frame_rgb.png"),
                            "metadataPath": str(metadata_path),
                            "detections": [
                                {
                                    "bboxXyxyPixels": [101, 101, 119, 119],
                                    "className": "drone",
                                    "confidence": 0.8,
                                    "slantRangeMeters": 500.0,
                                }
                            ],
                        }
                    ]
                }
            ),
            encoding="utf-8",
        )
        return report_path

    def test_legacy_contaminated_frames_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            report = self._write_fixture(Path(directory), capture_clean=False)
            self.assertEqual(load_rgb_evidence(report), [])

    def test_debug_clean_frame_preserves_detector_box_semantics(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            report = self._write_fixture(Path(directory), capture_clean=True)
            rows = load_rgb_evidence(report)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["bboxXyxyPixels"], [101.0, 101.0, 119.0, 119.0])
        self.assertIn("model_detector_bounding_box", rows[0]["visualSemantics"])
        self.assertFalse(rows[0]["fusionEligible"])
        self.assertEqual(
            rows[0]["associationProvenance"],
            "evaluator_truth_projection_debug_only",
        )
        self.assertIn("estimated-track", rows[0]["fusionExclusionReason"])

    def test_per_node_oak_reports_merge_for_all_site_runtime(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            city_root = root / "city"
            south_root = root / "south"
            city_root.mkdir()
            south_root.mkdir()
            city_report = self._write_fixture(city_root, capture_clean=True)
            south_report = self._write_fixture(south_root, capture_clean=True)
            south_payload = json.loads(south_report.read_text(encoding="utf-8"))
            south_payload["processedFrames"][0]["nodeId"] = "south"
            south_payload["processedFrames"][0]["frameId"] = "frame-2"
            south_report.write_text(json.dumps(south_payload), encoding="utf-8")
            rows = load_rgb_evidence_reports((city_report, south_report, city_report))

        self.assertEqual(len(rows), 2)
        self.assertEqual({row["nodeId"] for row in rows}, {"city", "south"})

    def test_rgb_derived_event_proxy_is_diagnostic_only_and_cannot_raise_fusion_tier(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            frames_root = root / "frames"
            node_root = frames_root / "City"
            node_root.mkdir(parents=True)
            (node_root / "frame-1_depth.json").write_text(
                json.dumps(
                    {
                        "debugVisualsExcludedFromSensorCapture": True,
                        "targets": [
                            {
                                "actorName": "track-1",
                                "intersectsFrame": True,
                                "bboxXyxyPixels": [100, 100, 200, 200],
                            }
                        ],
                    }
                ),
                encoding="utf-8",
            )
            event_report = root / "event.json"
            event_report.write_text(
                json.dumps(
                    {
                        "event_camera": {
                            "frames_root": str(frames_root),
                            "source_encoding": "proxy_event_stack_v1",
                            "preprocessing_equivalent_to_training": False,
                            "observation_samples": [
                                {
                                    "timestamp": "2026-08-03T04:00:00Z",
                                    "node_id": "City",
                                    "detection_probability": 0.95,
                                    "model_id": "fred-event-test",
                                    "measurements": {
                                        "frame_id": "frame-1_rgb",
                                        "frame_width": 1_000,
                                        "frame_height": 1_000,
                                        "bbox_xyxy_normalized": [0.1, 0.1, 0.2, 0.2],
                                        "source_encoding": "proxy_event_stack_v1",
                                        "preprocessing_equivalent_to_training": False,
                                    },
                                }
                            ],
                        }
                    }
                ),
                encoding="utf-8",
            )
            event_rows = load_event_evidence(event_report)
            native_payload = json.loads(event_report.read_text(encoding="utf-8"))
            native_event = native_payload["event_camera"]
            native_event["source_encoding"] = "native_event_stream_v1"
            native_event["physical_neuromorphic_sensor_data"] = True
            native_event["preprocessing_equivalent_to_training"] = True
            native_measurements = native_event["observation_samples"][0]["measurements"]
            native_measurements["source_encoding"] = "native_event_stream_v1"
            native_measurements["physical_neuromorphic_sensor_data"] = True
            native_measurements["preprocessing_equivalent_to_training"] = True
            event_report.write_text(json.dumps(native_payload), encoding="utf-8")
            native_rows = load_event_evidence(event_report)

        self.assertEqual(len(event_rows), 1)
        self.assertTrue(event_rows[0]["proxyInput"])
        self.assertFalse(event_rows[0]["fusionEligible"])
        self.assertIn("cannot count as a separate corroborating family", event_rows[0]["fusionExclusionReason"])
        self.assertTrue(native_rows[0]["sensorHealthEligible"])
        self.assertFalse(native_rows[0]["fusionEligible"])
        self.assertEqual(
            native_rows[0]["associationProvenance"],
            "evaluator_truth_projection_debug_only",
        )
        self.assertIn("estimated-track", native_rows[0]["fusionExclusionReason"])
        self.assertIn("native-event-frame", native_rows[0]["visualSemantics"])

        snapshot = build_layered_snapshot(
            {
                "schemaVersion": "triad.live_rf_snapshot.v1",
                "sampleComplete": True,
                "timestampUtc": "2026-08-03T04:00:00Z",
                "simulationSeconds": AS_OF.timestamp(),
                "weather": {
                    "profile": "Clear",
                    "rainRateMillimetersPerHour": 0.0,
                    "visibilityMeters": 30_000.0,
                },
                "sensorNodes": [
                    {
                        "nodeId": "City",
                        "latitudeDegrees": 1.29,
                        "longitudeDegrees": 103.86,
                        "heightMeters": 100.0,
                        "enabled": True,
                        "spawned": True,
                        "cameraCaptureConfigured": True,
                        "runtimeStatus": "ONLINE",
                    }
                ],
                "scenarioTargets": [],
            },
            rgb_rows=(
                {
                    "evidenceId": "rgb:City:frame-1:0",
                    "targetId": "track-1",
                    "nodeId": "City",
                    "timestampUtc": "2026-08-03T04:00:00Z",
                    "score": 0.90,
                    "rangeMeters": 500.0,
                    "modelId": "oak-rgb-test",
                    "associationProvenance": "test_estimated_track_association",
                    "fusionEligible": True,
                },
            ),
            event_rows=event_rows,
        )
        track = snapshot["tracks"][0]
        self.assertEqual(track["fusion"]["activeModalityFamilyCount"], 1)
        self.assertEqual(track["fusion"]["decision"], "HOLD_FOR_CORROBORATION")
        self.assertFalse(track["fusion"]["operatorCueActive"])
        self.assertFalse(track["positionAvailable"])
        self.assertFalse(track["scenarioTruth"]["available"])
        event_summary = track["modalitySummary"]["eventCamera"]
        self.assertFalse(event_summary["detected"])
        self.assertTrue(event_summary["candidateObserved"])
        self.assertEqual(event_summary["fusionEligibleObservationCount"], 0)
        self.assertEqual(event_summary["diagnosticOnlyObservationCount"], 1)
        registry = {row["modality"]: row for row in snapshot["sensorNodes"]}
        self.assertEqual(registry["rgb"]["status"], "DEGRADED")
        self.assertEqual(registry["event_camera"]["status"], "DEGRADED")


class SensorHealthProjectionTests(unittest.TestCase):
    @staticmethod
    def _snapshot(
        nodes: list[dict],
        *,
        schema: str = "triad.live_rf_snapshot.v1",
        targets: list[dict] | None = None,
        detected_rf_links: list[dict] | None = None,
    ) -> dict:
        return {
            "schemaVersion": schema,
            "sampleComplete": True,
            "timestampUtc": "2026-08-03T04:00:00Z",
            "simulationSeconds": AS_OF.timestamp(),
            "weather": {
                "profile": "Clear",
                "rainRateMillimetersPerHour": 0.0,
                "visibilityMeters": 30_000.0,
            },
            "sensorNodes": nodes,
            "scenarioTargets": targets or [],
            "detectedRFLinks": detected_rf_links or [],
            "detectionOnly": True,
            "actionsTaken": "none",
        }

    @staticmethod
    def _node(node_id: str, **overrides: object) -> dict:
        node = {
            "nodeId": node_id,
            "latitudeDegrees": 1.29,
            "longitudeDegrees": 103.85,
            "heightMeters": 100.0,
            "enabled": True,
            "spawned": True,
            "cameraCaptureConfigured": True,
            "runtimeStatus": "ONLINE",
            "detectionRangeMeters": 20_000.0,
        }
        node.update(overrides)
        return node

    def test_fresh_unreal_status_alone_preserves_32_records_but_visuals_fail_closed(self) -> None:
        nodes = [self._node(f"Node_{index}") for index in range(8)]
        snapshot = build_layered_snapshot(self._snapshot(nodes))
        sensors = snapshot["sensorNodes"]
        by_modality = {
            modality: [row for row in sensors if row["modality"] == modality]
            for modality in ("wideband_rf", "mmwave", "rgb", "event_camera")
        }

        self.assertEqual(snapshot["summary"]["sensorSiteCount"], 8)
        self.assertEqual(snapshot["summary"]["registeredSensorCount"], 32)
        self.assertEqual({key: len(value) for key, value in by_modality.items()}, {
            "wideband_rf": 8,
            "mmwave": 8,
            "rgb": 8,
            "event_camera": 8,
        })
        self.assertTrue(all(row["status"] == "ONLINE" for row in by_modality["wideband_rf"]))
        self.assertTrue(all(row["status"] == "OFFLINE" for row in by_modality["mmwave"]))
        self.assertTrue(
            all("excluded from fusion" in row["method"] for row in by_modality["mmwave"])
        )
        self.assertTrue(all(row["status"] == "OFFLINE" for row in by_modality["rgb"]))
        self.assertTrue(all(row["status"] == "OFFLINE" for row in by_modality["event_camera"]))
        self.assertTrue(all("not inherited" in row["healthMethod"] for row in by_modality["rgb"]))
        self.assertTrue(all("not inherited" in row["healthMethod"] for row in by_modality["event_camera"]))

    def test_fresh_oak_heartbeat_without_a_detection_can_mark_only_rgb_online(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            metadata = root / "frame_depth.json"
            metadata.write_text(
                json.dumps({"debugVisualsExcludedFromSensorCapture": True, "targets": []}),
                encoding="utf-8",
            )
            report = root / "oak.json"
            report.write_text(
                json.dumps({
                    "schemaVersion": "1.0",
                    "mode": "bounded_live_unreal_oak_rgbd_watch",
                    "updatedAtUtc": "2026-08-03T04:00:00Z",
                    "reportState": "running",
                    "inputs": {
                        "nodeId": "City",
                        "declaredTrustedSha256": "a" * 64,
                        "verifiedCheckpointSha256": "A" * 64,
                    },
                    "processedFrames": [{
                        "nodeId": "City",
                        "frameId": "frame-1",
                        "timestampUtc": "2026-08-03T04:00:00Z",
                        "metadataPath": str(metadata),
                        "detections": [],
                    }],
                }),
                encoding="utf-8",
            )
            health = load_rgb_sensor_health_reports((report,))

        snapshot = build_layered_snapshot(
            self._snapshot([self._node("City")]),
            rgb_health_rows=health,
        )
        sensors = {row["modality"]: row for row in snapshot["sensorNodes"]}
        self.assertEqual(health[0]["detectorCandidateRequiredForHealth"], False)
        self.assertEqual(sensors["rgb"]["status"], "ONLINE")
        self.assertIn("detections are not required", sensors["rgb"]["healthMethod"])
        self.assertEqual(sensors["event_camera"]["status"], "OFFLINE")

    def test_fresh_rgb_difference_event_proxy_is_degraded_never_online(self) -> None:
        snapshot = build_layered_snapshot(
            self._snapshot([self._node("City")]),
            event_rows=({
                "nodeId": "City",
                "timestampUtc": "2026-08-03T04:00:00Z",
                "proxyInput": True,
                "physicalNeuromorphicSensorData": False,
                "trainingPreprocessingEquivalent": False,
                "fusionEligible": False,
            },),
        )
        event = next(row for row in snapshot["sensorNodes"] if row["modality"] == "event_camera")
        self.assertEqual(event["status"], "DEGRADED")
        self.assertIn("not validated physical neuromorphic sensing", event["validationState"])
        self.assertIn("never treated as ONLINE", event["healthMethod"])

    def test_v2_uses_configured_rf_range_and_rejects_out_of_envelope_observation(self) -> None:
        node = self._node(
            "City",
            detectionRangeMeters=12_345.0,
            supportedFrequenciesGHz=[2.412],
            receiverSensitivityDbm=-96.0,
        )
        snapshot = build_layered_snapshot(
            self._snapshot(
                [node],
                schema="triad.live_rf_snapshot.v2",
                detected_rf_links=[
                    {
                        "timestampUtc": "2026-08-03T04:00:00Z",
                        "linkId": "City|track-1|2.412000",
                        "nodeId": "City",
                        "targetActor": "track-1",
                        "detected": True,
                        "frequencyGHz": 2.412,
                        "slantRangeMeters": 12_346.0,
                        "receivedPowerDbm": -60.0,
                        "noiseFloorDbm": -96.0,
                        "snrDb": 36.0,
                        "lineOfSight": True,
                    }
                ],
            )
        )

        rf = next(row for row in snapshot["sensorNodes"] if row["modality"] == "wideband_rf")
        self.assertEqual(snapshot["sourceUnrealSchemaVersion"], "triad.live_rf_snapshot.v2")
        self.assertEqual(rf["rangeMeters"], 12_345.0)
        self.assertEqual(rf["rangeSource"], "unreal_node_detectionRangeMeters")
        self.assertEqual(snapshot["tracks"], [])
        self.assertEqual(
            snapshot["longRangeEvidenceValidation"]["rejectedRFDetections"][0]["reason"],
            "slantRangeMeters exceeds the configured node RF envelope",
        )

    def test_scenario_truth_alone_cannot_create_a_contact_or_operator_cue(self) -> None:
        target = {
            "targetActor": "track-1",
            "targetLatitudeDegrees": 1.20,
            "targetLongitudeDegrees": 103.85,
            "targetHeightMeters": 120.0,
            "speedMetersPerSecond": 20.0,
            "headingDegrees": 0.0,
        }
        snapshot = build_layered_snapshot(
            self._snapshot([self._node("City")], targets=[target])
        )
        self.assertEqual(snapshot["tracks"], [])
        self.assertEqual(snapshot["summary"]["scenarioTargetCount"], 1)
        self.assertEqual(snapshot["summary"]["observationContactCount"], 0)
        self.assertEqual(snapshot["summary"]["activeOperatorCueCount"], 0)
        self.assertEqual(snapshot["observations"]["widebandRFLinks"], [])
        self.assertFalse(
            snapshot["observationAuthority"]["scenarioTargetsUsedAsDetectionSeeds"]
        )

    def test_rf_observations_without_scenario_truth_create_an_unlocated_contact(self) -> None:
        nodes = [
            self._node(
                node_id,
                supportedFrequenciesGHz=[2.412],
                receiverSensitivityDbm=-96.0,
            )
            for node_id in ("West", "East")
        ]
        links = [
            {
                "timestampUtc": "2026-08-03T04:00:00Z",
                "linkId": f"{node['nodeId']}|rf-contact|2.412000",
                "nodeId": node["nodeId"],
                "targetActor": "rf-contact",
                "detected": True,
                "frequencyGHz": 2.412,
                "frequencyBandLabel": "2.4 GHz",
                "slantRangeMeters": 1_000.0,
                "receivedPowerDbm": -60.0,
                "noiseFloorDbm": -96.0,
                "snrDb": 36.0,
                "lineOfSight": True,
                "targetLatitudeDegrees": 88.0,
                "targetLongitudeDegrees": -170.0,
                "targetHeightMeters": 9_999.0,
                "hostileScenarioTruth": True,
            }
            for node in nodes
        ]
        snapshot = build_layered_snapshot(
            self._snapshot(
                nodes,
                schema="triad.live_rf_snapshot.v3",
                detected_rf_links=links,
            )
        )

        self.assertEqual(snapshot["summary"]["scenarioTargetCount"], 0)
        self.assertEqual(snapshot["summary"]["observationContactCount"], 1)
        self.assertEqual(len(snapshot["observations"]["widebandRFLinks"]), 2)
        track = snapshot["tracks"][0]
        self.assertEqual(track["fusion"]["decision"], "PRELIMINARY_RF_CUE")
        self.assertTrue(track["fusion"]["operatorCueActive"])
        self.assertFalse(track["positionAvailable"])
        self.assertIsNone(track["latitudeDegrees"])
        self.assertIsNone(track["longitudeDegrees"])
        self.assertIsNone(track["heightMeters"])
        self.assertFalse(track["trackEstimate"]["truthSeededSimulationSurrogate"])
        self.assertFalse(track["scenarioTruth"]["available"])
        self.assertTrue(snapshot["rawUnrealDetectedRFLinksConsumed"])
        for observation in snapshot["observations"]["widebandRFLinks"]:
            self.assertNotIn("targetLatitudeDegrees", observation)
            self.assertNotIn("targetLongitudeDegrees", observation)
            self.assertNotIn("hostileScenarioTruth", observation)


class SensorRegistryEvidenceWordingTests(unittest.TestCase):
    def test_visual_registry_separates_offline_evidence_from_live_range(self) -> None:
        snapshot = build_layered_snapshot(
            {
                "schemaVersion": "triad.live_rf_snapshot.v1",
                "sampleComplete": True,
                "timestampUtc": "2026-08-03T04:00:00Z",
                "simulationSeconds": AS_OF.timestamp(),
                "weather": {
                    "profile": "Clear",
                    "rainRateMillimetersPerHour": 0.0,
                    "visibilityMeters": 30_000.0,
                },
                "sensorNodes": [
                    {
                        "nodeId": "City",
                        "latitudeDegrees": 1.29,
                        "longitudeDegrees": 103.85,
                        "heightMeters": 100.0,
                        "enabled": True,
                        "spawned": True,
                        "cameraCaptureConfigured": True,
                        "runtimeStatus": "ONLINE",
                    }
                ],
                "scenarioTargets": [],
            }
        )
        sensors = {item["modality"]: item for item in snapshot["sensorNodes"]}

        rgb = sensors["rgb"]
        self.assertEqual(rgb["rangeMeters"], 0.0)
        self.assertIn("one-class RGB detector", rgb["method"])
        self.assertIn("raw uint32-mm simulation depth", rgb["method"])
        self.assertIn("City 48/60 frames", rgb["method"])
        self.assertIn("South 37/60 frames", rgb["method"])
        self.assertIn("15.8-339.0 m", rgb["method"])
        self.assertIn("field/physical range unvalidated", rgb["method"])
        self.assertIn("OFFLINE ACCEPTANCE EVIDENCE ONLY", rgb["validationState"])

        event = sensors["event_camera"]
        self.assertEqual(event["rangeMeters"], 0.0)
        self.assertIn("RGB-derived proxy", event["method"])
        self.assertIn("non-physical", event["method"])
        self.assertIn("not training-equivalent", event["method"])
        self.assertIn("candidates inconclusive", event["method"])
        self.assertIn("native event-camera range unvalidated", event["method"])


if __name__ == "__main__":
    unittest.main()
