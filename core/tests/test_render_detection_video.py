from __future__ import annotations

from datetime import datetime, timezone
import json
from pathlib import Path
import tempfile
import unittest

import numpy as np

from singapore_sensor_fusion.render_detection_video import (
    AlertRecord,
    FrameRecord,
    RenderConfig,
    build_video_manifest,
    extract_ground_truth_overlays,
    load_alert_records,
    recent_matching_alert,
    render_detection_video,
    select_frame_records,
)


def _write_frame(directory: Path, index: int, timestamp: str, targets=None) -> Path:
    directory.mkdir(parents=True, exist_ok=True)
    name = f"frame_{index:06d}_20260101T000000fZ"
    rgb = directory / f"{name}_rgb.png"
    rgb.write_bytes(b"not-read-by-selection-tests")
    metadata = {
        "timestampUtc": timestamp,
        "nodeId": directory.name,
        "frameIndex": index,
        "width": 80,
        "height": 60,
    }
    if targets is not None:
        metadata["targets"] = targets
    (directory / f"{name}_depth.json").write_text(json.dumps(metadata), encoding="utf-8")
    return rgb


class _FakeImageOperations:
    name = "fake-images"

    def __init__(self) -> None:
        self.box_labels: list[str] = []
        self.banners: list[str] = []
        self.text_labels: list[str] = []

    def read_rgb(self, path: Path) -> np.ndarray:
        del path
        return np.zeros((60, 80, 3), dtype=np.uint8)

    def resize(self, image: np.ndarray, width: int, height: int) -> np.ndarray:
        del image
        return np.zeros((height, width, 3), dtype=np.uint8)

    def box(self, image, bounds, color, label) -> None:
        del image, bounds, color
        self.box_labels.append(label)

    def text(self, image, text, origin, color, *, scale=0.55) -> None:
        del image, origin, color, scale
        self.text_labels.append(text)

    def banner(self, image, text) -> None:
        del image
        self.banners.append(text)


class _FakeEncoder:
    name = "mock-encoder"

    def __init__(self, path: Path, fps: float, size: tuple[int, int]) -> None:
        self.path = path
        self.fps = fps
        self.size = size
        self.frames: list[np.ndarray] = []

    def write(self, frame_rgb: np.ndarray) -> None:
        self.frames.append(frame_rgb.copy())

    def close(self) -> None:
        self.path.write_bytes(b"mock-mp4")


class RenderDetectionVideoTests(unittest.TestCase):
    def test_loads_unreal_multinode_alert_schema(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "alerts.jsonl"
            path.write_text(
                json.dumps(
                    {
                        "timestampUtc": "2026-01-01T00:00:00Z",
                        "alertType": "simulated_hostile_rf_multinode",
                        "targetActor": "drone-7",
                        "hostileScenarioTruth": True,
                        "confirmingNodeIds": ["Node_A", "Node_C"],
                        "nearestConfirmingNodeId": "Node_A",
                        "nearestConfirmingNodeDistanceMeters": 347.6,
                        "distanceSemantics": "sensor_to_target_3d_slant_range",
                        "detectionOnly": True,
                        "actionsTaken": "none",
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            alerts = load_alert_records(path)
            self.assertEqual(len(alerts), 1)
            self.assertTrue(alerts[0].hostile)
            self.assertEqual(alerts[0].target_id, "drone-7")
            self.assertEqual(alerts[0].confirming_node_ids, ("Node_A", "Node_C"))
            self.assertEqual(alerts[0].nearest_node_id, "Node_A")
            self.assertEqual(alerts[0].nearest_node_distance_meters, 347.6)
            self.assertTrue(alerts[0].scenario_red_team)

    def test_alert_range_supports_nested_and_legacy_schemas(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "alerts.jsonl"
            values = [
                {
                    "timestampUtc": "2026-01-01T00:00:00Z",
                    "hostile": True,
                    "nearestNode": {"nodeId": "Node_B", "distanceMeters": 1_250.0},
                },
                {
                    "timestampUtc": "2026-01-01T00:00:01Z",
                    "hostile": True,
                    "confirmingNodes": [
                        {"nodeId": "Node_C", "slantDistanceMeters": 700.0},
                        {"nodeId": "Node_A", "slantDistanceMeters": 325.0},
                    ],
                },
                {
                    "timestampUtc": "2026-01-01T00:00:02Z",
                    "hostile": True,
                    "confirmingNodeIds": ["Node_A", "Node_B"],
                },
            ]
            path.write_text(
                "".join(json.dumps(value) + "\n" for value in values),
                encoding="utf-8",
            )
            alerts = load_alert_records(path)
            self.assertEqual(alerts[0].nearest_node_id, "Node_B")
            self.assertEqual(alerts[0].nearest_node_distance_meters, 1_250.0)
            self.assertEqual(alerts[1].nearest_node_id, "Node_A")
            self.assertEqual(alerts[1].nearest_node_distance_meters, 325.0)
            self.assertIsNone(alerts[2].nearest_node_distance_meters)

    def test_generic_alert_status_is_not_treated_as_hostility_or_red_team(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "alerts.jsonl"
            path.write_text(
                json.dumps(
                    {
                        "timestampUtc": "2026-01-01T00:00:00Z",
                        "status": "alert",
                        "targetId": "drone-7",
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            alert = load_alert_records(path)[0]
            self.assertFalse(alert.hostile)
            self.assertFalse(alert.scenario_red_team)

    def test_selects_latest_session_then_applies_cadence_and_bound(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            node = Path(temporary) / "Node_A"
            for index, second in enumerate((0, 2, 4, 70, 72, 74)):
                _write_frame(node, index, f"2026-01-01T00:01:{second - 60:02d}Z" if second >= 60 else f"2026-01-01T00:00:{second:02d}Z")
            selected = select_frame_records(
                Path(temporary),
                node_id="Node_A",
                max_frames=2,
                cadence_seconds=2.0,
                session_gap_seconds=30.0,
            )
            self.assertEqual([record.frame_index for record in selected], [3, 5])
            self.assertTrue(all(record.node_id == "Node_A" for record in selected))

    def test_extracts_hostile_and_friendly_ground_truth_boxes(self) -> None:
        metadata = {
            "targets": [
                {
                    "actorName": "red-1",
                    "hostileScenarioTruth": True,
                    "bboxXyxyPixels": [10, 10, 50, 40],
                    "distanceMeters": 438.4,
                    "inFront": True,
                    "intersectsFrame": True,
                },
                {
                    "targetId": "blue-1",
                    "affiliation": "friendly",
                    "bboxPixels": {"left": 5, "top": 6, "right": 20, "bottom": 30},
                    "distanceMeters": 1_250.0,
                },
            ]
        }
        overlays = extract_ground_truth_overlays(metadata, 100, 50)
        self.assertEqual(len(overlays), 2)
        self.assertEqual(overlays[0].box_xyxy, (10, 10, 50, 40))
        self.assertTrue(overlays[0].hostile)
        self.assertIn("SIM GROUND TRUTH | HOSTILE | RANGE 438 m | red-1", overlays[0].label)
        self.assertEqual(overlays[0].distance_meters, 438.4)
        self.assertFalse(overlays[1].hostile)
        self.assertIn(
            "SIM GROUND TRUTH | FRIENDLY | RANGE 1.25 km | blue-1",
            overlays[1].label,
        )

    def test_recent_alert_requires_node_and_target_match_not_hostility_inference(self) -> None:
        timestamp = datetime(2026, 1, 1, tzinfo=timezone.utc)
        record = FrameRecord(
            rgb_path=Path("frame.png"),
            metadata_path=None,
            timestamp=timestamp,
            node_id="Node_A",
            frame_index=1,
            metadata={"targets": [{"targetId": "drone-7"}]},
        )
        alerts = [
            AlertRecord(timestamp, "Node_B", "drone-7", True, {}),
            AlertRecord(timestamp, "Node_A", "drone-8", True, {}),
            AlertRecord(
                timestamp,
                None,
                "drone-7",
                False,
                {"alertType": "rf_multinode_detection"},
                ("Node_A", "Node_C"),
            ),
        ]
        matched = recent_matching_alert(record, alerts)
        self.assertIsNotNone(matched)
        self.assertEqual(
            matched.payload, {"alertType": "rf_multinode_detection"}
        )

    def test_manifest_records_provenance_counts_and_disclaimers(self) -> None:
        timestamp = datetime(2026, 1, 1, tzinfo=timezone.utc)
        records = [
            FrameRecord(Path("a.png"), None, timestamp, "Node_A", 1, {}),
            FrameRecord(Path("b.png"), None, timestamp, "Node_A", 2, {}),
        ]
        config = RenderConfig(
            frames_root=Path("frames"),
            output_path=Path("demo.mp4"),
            fps=4.0,
            event_model_path=Path("trusted.pt"),
        )
        manifest = build_video_manifest(
            config=config,
            records=records,
            encoder_name="mock",
            image_backend="mock-images",
            output_path=Path("demo.mp4"),
            alert_frame_count=1,
            candidate_count=3,
            ground_truth_box_count=2,
            event_enabled=True,
            created_utc=timestamp,
        )
        self.assertEqual(manifest["frameCount"], 2)
        self.assertEqual(manifest["durationSeconds"], 0.5)
        self.assertEqual(manifest["counts"]["eventYoloCandidates"], 3)
        self.assertIn("no true-positive claim", manifest["event"]["disclaimer"])
        self.assertFalse(
            manifest["event"]["inputProvenance"]["physicalNeuromorphicSensorData"]
        )
        self.assertFalse(
            manifest["event"]["inputProvenance"]["trainingPreprocessingEquivalent"]
        )
        self.assertFalse(manifest["groundTruth"]["enabled"])
        self.assertEqual(manifest["alertLabel"], "RF MULTI-NODE DETECTION ALERT")
        self.assertFalse(manifest["hostilityInferredFromSensorEvidence"])
        self.assertTrue(manifest["detectionOnly"])

    def test_render_uses_mock_encoder_and_publishes_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            node = root / "frames" / "Node_A"
            target = {
                "targetId": "hostile-1",
                "hostile": True,
                "bbox_xyxy_normalized": [0.1, 0.1, 0.3, 0.4],
                "distanceMeters": 1_234.0,
            }
            _write_frame(node, 1, "2026-01-01T00:00:00Z", [target])
            _write_frame(node, 2, "2026-01-01T00:00:01Z", [target])
            output = root / "rendered.mp4"
            operations = _FakeImageOperations()
            encoders: list[_FakeEncoder] = []

            def factory(path: Path, fps: float, size: tuple[int, int]) -> _FakeEncoder:
                encoder = _FakeEncoder(path, fps, size)
                encoders.append(encoder)
                return encoder

            manifest = render_detection_video(
                RenderConfig(
                    frames_root=root / "frames",
                    node_id="Node_A",
                    output_path=output,
                    fps=5.0,
                ),
                image_operations=operations,
                encoder_factory=factory,
            )
            self.assertTrue(output.is_file())
            self.assertTrue(output.with_suffix(".manifest.json").is_file())
            self.assertEqual(len(encoders[0].frames), 2)
            self.assertEqual(encoders[0].size, (80, 60))
            self.assertEqual(manifest["counts"]["simulationGroundTruthBoxes"], 0)
            self.assertFalse(manifest["groundTruth"]["enabled"])
            self.assertEqual(operations.box_labels, [])

    def test_render_draws_simulation_truth_only_when_explicitly_enabled(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            node = root / "frames" / "Node_A"
            target = {
                "targetId": "hostile-1",
                "hostile": True,
                "bbox_xyxy_normalized": [0.1, 0.1, 0.3, 0.4],
                "distanceMeters": 1_234.0,
            }
            _write_frame(node, 1, "2026-01-01T00:00:00Z", [target])
            operations = _FakeImageOperations()

            manifest = render_detection_video(
                RenderConfig(
                    frames_root=root / "frames",
                    node_id="Node_A",
                    output_path=root / "rendered.mp4",
                    show_simulation_truth=True,
                ),
                image_operations=operations,
                encoder_factory=lambda path, fps, size: _FakeEncoder(path, fps, size),
            )

            self.assertEqual(manifest["counts"]["simulationGroundTruthBoxes"], 1)
            self.assertTrue(manifest["groundTruth"]["enabled"])
            self.assertEqual(len(operations.box_labels), 1)
            self.assertIn("SIM GROUND TRUTH", operations.box_labels[0])
            self.assertIn("RANGE 1.23 km", operations.box_labels[0])

    def test_render_labels_event_input_as_proxy_and_not_training_equivalent(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            node = root / "frames" / "Node_A"
            _write_frame(node, 1, "2026-01-01T00:00:00Z")
            operations = _FakeImageOperations()

            manifest = render_detection_video(
                RenderConfig(
                    frames_root=root / "frames",
                    node_id="Node_A",
                    output_path=root / "rendered.mp4",
                ),
                image_operations=operations,
                encoder_factory=lambda path, fps, size: _FakeEncoder(path, fps, size),
                event_adapter=object(),
            )

            self.assertTrue(manifest["event"]["enabled"])
            self.assertIn(
                "EVENT INPUT: RGB-DERIVED PROXY | NOT A PHYSICAL EVENT CAMERA "
                "| NOT TRAINING-EQUIVALENT",
                operations.text_labels,
            )
            self.assertIn(
                "EVENT-YOLO OUTPUTS: PROXY CANDIDATES ONLY | NOT CONFIRMED DETECTIONS",
                operations.text_labels,
            )

    def test_render_includes_nearest_rf_node_range_in_alert_overlay(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            node = root / "frames" / "Node_A"
            target = {
                "targetId": "hostile-1",
                "hostile": True,
                "bbox_xyxy_normalized": [0.1, 0.1, 0.3, 0.4],
                "distanceMeters": 460.0,
            }
            _write_frame(node, 1, "2026-01-01T00:00:01Z", [target])
            alerts_path = root / "alerts.jsonl"
            alerts_path.write_text(
                json.dumps(
                    {
                        "timestampUtc": "2026-01-01T00:00:00Z",
                        "targetId": "hostile-1",
                        "hostile": True,
                        "confirmingNodeIds": ["Node_A", "Node_B"],
                        "nearestNodeDistanceMeters": 432.1,
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            operations = _FakeImageOperations()

            render_detection_video(
                RenderConfig(
                    frames_root=root / "frames",
                    node_id="Node_A",
                    output_path=root / "rendered.mp4",
                    alerts_path=alerts_path,
                ),
                image_operations=operations,
                encoder_factory=lambda path, fps, size: _FakeEncoder(path, fps, size),
            )

            self.assertEqual(
                operations.banners,
                ["RF MULTI-NODE DETECTION ALERT | NEAREST RF NODE 432 m"],
            )
            self.assertIn(
                "RF/FUSION STATUS: DETECTION ALERT ACTIVE | NEAREST RF NODE 432 m",
                operations.text_labels,
            )

    def test_render_uses_red_team_wording_only_for_explicit_scenario_truth(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            node = root / "frames" / "Node_A"
            target = {"targetId": "red-1"}
            _write_frame(node, 1, "2026-01-01T00:00:01Z", [target])
            alerts_path = root / "alerts.jsonl"
            alerts_path.write_text(
                json.dumps(
                    {
                        "timestampUtc": "2026-01-01T00:00:00Z",
                        "targetId": "red-1",
                        "hostileScenarioTruth": True,
                        "confirmingNodeIds": ["Node_A", "Node_B"],
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            operations = _FakeImageOperations()

            render_detection_video(
                RenderConfig(
                    frames_root=root / "frames",
                    node_id="Node_A",
                    output_path=root / "rendered.mp4",
                    alerts_path=alerts_path,
                ),
                image_operations=operations,
                encoder_factory=lambda path, fps, size: _FakeEncoder(path, fps, size),
            )

            self.assertEqual(
                operations.banners,
                ["RF MULTI-NODE DETECTION ALERT | SCENARIO RED-TEAM"],
            )
            self.assertIn(
                "RF/FUSION STATUS: DETECTION ALERT ACTIVE | SCENARIO RED-TEAM",
                operations.text_labels,
            )


if __name__ == "__main__":
    unittest.main()
