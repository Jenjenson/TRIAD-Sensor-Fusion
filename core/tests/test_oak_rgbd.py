from datetime import datetime, timezone
from hashlib import sha256
import json
from pathlib import Path
import sys
import tempfile
from types import ModuleType, SimpleNamespace
import unittest
from unittest.mock import patch

import numpy as np

from singapore_sensor_fusion.oak_rgbd import (
    DEPTH_RAW_OBSERVATION_ENCODING,
    DEPTH_RAW_QUANTIZATION_METERS,
    DEPTH_RAW_SEMANTICS,
    OAKRGBDAdapter,
    RANGE_MODE_SIMULATION_ADAPTED,
    RGBDModelBlockedError,
    apply_2_of_3_persistence,
    decode_unreal_depth_preview,
    decode_unreal_depth_u32_mm,
    depth_z_to_slant_range,
    estimate_box_depth,
    load_unreal_rgbd_frame,
    run_latest_frame_session,
    select_latest_unreal_rgbd_frames,
)
from singapore_sensor_fusion.observations import SensorModality


class _FakeYOLOModel:
    names = {0: "drone"}

    def __init__(self, checkpoint: str) -> None:
        self.checkpoint = checkpoint
        self.calls = []

    def predict(self, **kwargs):
        self.calls.append(kwargs)
        boxes = SimpleNamespace(
            xyxy=np.array([[2.0, 1.0, 18.0, 9.0]], dtype=float),
            conf=np.array([0.731], dtype=float),
            cls=np.array([0.0], dtype=float),
        )
        return [SimpleNamespace(boxes=boxes, names=self.names)]


class OAKRGBDTests(unittest.TestCase):
    def _checkpoint(self, directory: str) -> tuple[Path, str]:
        content = b"trusted oak RGB detector fixture"
        path = Path(directory) / "best.pt"
        path.write_bytes(content)
        return path, sha256(content).hexdigest().upper()

    def _fake_ultralytics(self):
        module = ModuleType("ultralytics")
        created = []

        def fake_yolo(checkpoint: str):
            model = _FakeYOLOModel(checkpoint)
            created.append(model)
            return model

        module.YOLO = fake_yolo
        return module, created

    def test_hash_is_checked_before_lazy_deserialization(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            checkpoint, _ = self._checkpoint(directory)
            adapter = OAKRGBDAdapter(checkpoint, trusted_sha256="0" * 64)
            self.assertFalse(adapter.loaded)
            with self.assertRaises(RGBDModelBlockedError) as caught:
                adapter.load()
        self.assertEqual(caught.exception.code, "blocked_integrity_mismatch")
        self.assertFalse(adapter.loaded)

        with self.assertRaises(RGBDModelBlockedError) as caught:
            OAKRGBDAdapter("detector.engine", trusted_sha256="0" * 64)
        self.assertEqual(caught.exception.code, "blocked_untrusted_artifact_type")

    def test_rgb_only_yolo_then_depth_z_and_slant_range(self) -> None:
        module, created = self._fake_ultralytics()
        with tempfile.TemporaryDirectory() as directory:
            checkpoint, digest = self._checkpoint(directory)
            adapter = OAKRGBDAdapter(
                checkpoint,
                trusted_sha256=digest,
                max_range_meters=255.0,
            )
            rgb = np.zeros((10, 20, 3), dtype=np.uint8)
            rgb[0, 0] = [10, 20, 30]
            depth = np.full((10, 20), 40, dtype=np.uint8)
            metadata = {
                "normalizationMaxMeters": 255.0,
                "fovDegrees": 90.0,
                "targets": [
                    {
                        "actorName": "sim-drone",
                        "intersectsFrame": True,
                        "hostileScenarioTruth": True,
                        "bboxXyxyPixels": [2, 1, 18, 9],
                        "distanceMeters": 999.0,
                    }
                ],
            }
            with patch.dict(sys.modules, {"ultralytics": module}):
                observations = adapter.infer(
                    rgb,
                    depth,
                    metadata,
                    node_id="City_Sector",
                    timestamp=datetime(2026, 8, 1, tzinfo=timezone.utc),
                    frame_id="frame_2",
                    input_color_space="RGB",
                    include_simulation_iou_diagnostic=True,
                )

        self.assertEqual(len(created), 1)
        self.assertEqual(len(observations), 1)
        observation = observations[0]
        self.assertEqual(observation.modality, SensorModality.RGBD)
        self.assertAlmostEqual(observation.detection_probability, 0.731)
        self.assertEqual(observation.measurements["model_input_channels"], 3)
        self.assertFalse(observation.measurements["depth_used_as_model_input"])
        self.assertEqual(observation.measurements["depth_role"], "post_detection_ranging")
        self.assertAlmostEqual(observation.measurements["depth_z_m"], 40.0)
        self.assertAlmostEqual(observation.measurements["slant_range_m"], 40.0)
        self.assertEqual(observation.measurements["range_mode"], RANGE_MODE_SIMULATION_ADAPTED)
        self.assertEqual(observation.measurements["physical_oak_contract_range_gate_meters"], [0.2, 30.0])
        diagnostic = observation.measurements["simulation_ground_truth_iou_diagnostic"]
        self.assertEqual(diagnostic["diagnostic_scope"], "projected_simulation_truth_only")
        self.assertFalse(diagnostic["physical_accuracy_claimed"])
        self.assertNotIn("simulation_truth_distance_meters", diagnostic)

        call = created[0].calls[0]
        self.assertEqual(call["imgsz"], 832)
        self.assertEqual(call["iou"], 0.5)
        self.assertEqual(call["max_det"], 10)
        self.assertEqual(call["device"], "cpu")
        np.testing.assert_array_equal(call["source"][0, 0], [30, 20, 10])

    def test_preview_clipping_range_gate_and_full_box_fallback(self) -> None:
        preview = np.array([[0, 1, 100, 101, 254, 255]], dtype=np.uint8)
        decoded = decode_unreal_depth_preview(
            preview,
            normalization_max_meters=255.0,
            max_range_meters=100.0,
        )
        np.testing.assert_array_equal(decoded.valid_mask, [[False, True, True, False, False, False]])
        self.assertEqual(decoded.quantization_step_meters, 1.0)
        with self.assertRaisesRegex(ValueError, "<= normalization"):
            decode_unreal_depth_preview(preview, normalization_max_meters=255.0, max_range_meters=256.0)

        box_preview = np.full((10, 10), 255, dtype=np.uint8)
        box_preview[0, 0:3] = [20, 30, 40]
        box_depth = decode_unreal_depth_preview(
            box_preview,
            normalization_max_meters=255.0,
            max_range_meters=255.0,
        )
        estimate = estimate_box_depth(
            box_depth,
            [0, 0, 10, 10],
            minimum_valid_pixels=3,
        )
        self.assertEqual(estimate.roi_semantics, "full_box_fallback")
        self.assertEqual(estimate.valid_pixel_count, 3)
        self.assertAlmostEqual(estimate.estimated_distance_meters, 27.0)

    def test_uint32_millimetre_decoder_preserves_10km_and_excludes_sentinels(self) -> None:
        raw = np.array([[0, 1, 1234, 10_000_000, 0xFFFFFFFE, 0xFFFFFFFF]], dtype=np.uint32)
        decoded = decode_unreal_depth_u32_mm(raw, max_range_meters=10_000.0)
        np.testing.assert_array_equal(decoded.valid_mask, [[False, True, True, True, False, False]])
        self.assertAlmostEqual(float(decoded.distance_meters[0, 2]), 1.234, places=6)
        self.assertAlmostEqual(float(decoded.distance_meters[0, 3]), 10_000.0, places=3)
        self.assertEqual(decoded.quantization_step_meters, DEPTH_RAW_QUANTIZATION_METERS)
        self.assertEqual(decoded.encoding, DEPTH_RAW_OBSERVATION_ENCODING)
        self.assertEqual(decoded.source_semantics, DEPTH_RAW_SEMANTICS)
        self.assertAlmostEqual(decoded.configured_max_range_meters, 10_000.0)

    def test_complete_uint32_sidecar_is_preferred_and_length_is_validated(self) -> None:
        try:
            from PIL import Image
        except (ImportError, ModuleNotFoundError):
            self.skipTest("Pillow is not installed")
        module, _ = self._fake_ultralytics()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            node = root / "City_Sector"
            node.mkdir()
            checkpoint, digest = self._checkpoint(directory)
            stem = "frame_000000_20260801T000000Z"
            rgb = np.zeros((10, 20, 3), dtype=np.uint8)
            preview = np.full((10, 20), 40, dtype=np.uint8)
            raw = np.full((10, 20), 10_000_000, dtype="<u4")
            Image.fromarray(rgb, mode="RGB").save(node / f"{stem}_rgb.png")
            Image.fromarray(preview, mode="L").save(node / f"{stem}_depth.png")
            raw_path = node / f"{stem}_depth_u32_mm.bin"
            raw_path.write_bytes(raw.tobytes(order="C"))
            metadata = {
                "timestampUtc": "2026-08-01T00:00:00Z",
                "nodeId": "City_Sector",
                "frameIndex": 0,
                "width": 20,
                "height": 10,
                "fovDegrees": 90,
                "depthPngEncoding": "gray/255 * normalizationMaxMeters",
                "normalizationMaxMeters": 255.0,
                "depthRawFileName": raw_path.name,
                "depthRawEncoding": "uint32_millimeters",
                "depthRawQuantizationMeters": 0.001,
                "depthRawUnits": "millimeters",
                "depthRawByteOrder": "little-endian",
                "depthRawWidth": 20,
                "depthRawHeight": 10,
                "depthRawInvalidValue": 0,
                "depthRawSaturatedValue": 0xFFFFFFFF,
                "depthRawMaximumRepresentableMeters": 4294967.294,
                "depthRawFileByteCount": 800,
                "depthRawSemantics": "simulation_scene_depth_z_not_physical_oak_accuracy",
                "depthRawSimulationOnly": True,
                "targets": [],
            }
            (node / f"{stem}_depth.json").write_text(json.dumps(metadata), encoding="utf-8")
            selected = select_latest_unreal_rgbd_frames(root, max_frames=1)[0]
            loaded = load_unreal_rgbd_frame(selected)
            self.assertIsNotNone(loaded.depth_raw_u32_mm)
            self.assertEqual(loaded.depth_source_path, raw_path.resolve())
            adapter = OAKRGBDAdapter(checkpoint, trusted_sha256=digest)
            with patch.dict(sys.modules, {"ultralytics": module}):
                observation = adapter.infer_unreal_frame(loaded)[0]
            self.assertAlmostEqual(observation.measurements["depth_z_m"], 10_000.0, places=3)
            self.assertEqual(observation.measurements["depth_selected_encoding"], DEPTH_RAW_OBSERVATION_ENCODING)
            self.assertEqual(observation.measurements["depth_quantization_step_meters"], 0.001)
            self.assertEqual(observation.measurements["depth_source_semantics"], DEPTH_RAW_SEMANTICS)
            self.assertFalse(observation.measurements["physical_accuracy_claimed"])

            raw_path.write_bytes(raw.tobytes(order="C")[:-2])
            with self.assertRaisesRegex(ValueError, "byte length"):
                load_unreal_rgbd_frame(selected)
            raw_path.unlink()
            fallback = load_unreal_rgbd_frame(selected)
            self.assertIsNone(fallback.depth_raw_u32_mm)
            self.assertEqual(fallback.depth_source_path, fallback.paths.depth_path)
            self.assertEqual(fallback.depth_fallback_reason, "declared_raw_sidecar_absent")

    def test_slant_geometry_and_2_of_3_persistence_preserve_confidence(self) -> None:
        centered = depth_z_to_slant_range(
            100.0,
            [40, 40, 60, 60],
            frame_width=100,
            frame_height=100,
            horizontal_fov_degrees=90.0,
        )
        off_axis = depth_z_to_slant_range(
            100.0,
            [80, 40, 100, 60],
            frame_width=100,
            frame_height=100,
            horizontal_fov_degrees=90.0,
        )
        self.assertAlmostEqual(centered, 100.0)
        self.assertGreater(off_axis, centered)

        def row(box, confidence=0.731):
            return {
                "detection_probability": confidence,
                "confidence_level": confidence,
                "measurements": {"class_name": "drone", "bbox_xyxy_pixels": list(box)},
            }

        first = [row((0, 0, 10, 10))]
        count, descriptors = apply_2_of_3_persistence(first, [])
        self.assertEqual(count, 0)
        second = [row((1, 0, 11, 10))]
        count, second_descriptors = apply_2_of_3_persistence(second, [descriptors])
        self.assertEqual(count, 1)
        self.assertTrue(second[0]["measurements"]["temporal_persistence"]["persistent_detection"])
        self.assertEqual(second[0]["detection_probability"], 0.731)
        third = [row((30, 30, 40, 40))]
        count, _ = apply_2_of_3_persistence(third, [descriptors, second_descriptors])
        self.assertEqual(count, 0)

    def test_bounded_latest_session_cli_writes_raw_and_persistent_detections(self) -> None:
        try:
            from PIL import Image
        except (ImportError, ModuleNotFoundError):
            self.skipTest("Pillow is not installed")
        module, created = self._fake_ultralytics()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            node = root / "City_Sector"
            node.mkdir()
            checkpoint, digest = self._checkpoint(directory)
            rgb = np.zeros((10, 20, 3), dtype=np.uint8)
            rgb[..., 0], rgb[..., 1], rgb[..., 2] = 10, 20, 30
            depth = np.zeros((10, 20, 4), dtype=np.uint8)
            depth[..., :3] = 40
            depth[..., 3] = 255
            for index in range(3):
                stem = f"frame_{index:06d}_20260801T00000{index}Z"
                Image.fromarray(rgb, mode="RGB").save(node / f"{stem}_rgb.png")
                Image.fromarray(depth, mode="RGBA").save(node / f"{stem}_depth.png")
                metadata = {
                    "timestampUtc": f"2026-08-01T00:00:0{index}Z",
                    "nodeId": "City_Sector",
                    "frameIndex": index,
                    "width": 20,
                    "height": 10,
                    "fovDegrees": 90,
                    "depthPngEncoding": "gray/255 * normalizationMaxMeters",
                    "normalizationMaxMeters": 255.0,
                    "targets": [],
                }
                (node / f"{stem}_depth.json").write_text(json.dumps(metadata), encoding="utf-8")
            output = root / "report.json"
            with patch.dict(sys.modules, {"ultralytics": module}):
                report = run_latest_frame_session(
                    frames_root=root,
                    checkpoint_path=checkpoint,
                    trusted_sha256=digest,
                    output_path=output,
                    max_frames=3,
                    max_range_meters=255.0,
                )
            saved = json.loads(output.read_text(encoding="utf-8"))
            temporary_files_remain = any(
                path.name.startswith(".report.json.") for path in root.iterdir()
            )

        self.assertEqual(len(created), 1)
        self.assertEqual(report["summary"]["raw_detection_count"], 3)
        self.assertEqual(report["summary"]["persistent_detection_count"], 2)
        self.assertEqual(report["summary"]["raw_u32_depth_frame_count"], 0)
        self.assertEqual(report["summary"]["preview_fallback_frame_count"], 3)
        self.assertTrue(all(item["depth_selected_encoding"] == "gray/255 * normalizationMaxMeters" for item in report["processed_frames"]))
        self.assertTrue(all(item["depth_fallback_reason"] == "raw_sidecar_not_declared" for item in report["processed_frames"]))
        self.assertEqual(saved["summary"], report["summary"])
        self.assertEqual([item["detection_probability"] for item in saved["observations"]], [0.731] * 3)
        flags = [item["measurements"]["temporal_persistence"]["persistent_detection"] for item in saved["observations"]]
        self.assertEqual(flags, [False, True, True])
        self.assertFalse(temporary_files_remain)


if __name__ == "__main__":
    unittest.main()
