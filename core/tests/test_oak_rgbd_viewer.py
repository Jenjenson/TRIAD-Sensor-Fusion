from __future__ import annotations

from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import tempfile
import unittest

import numpy as np

from singapore_sensor_fusion.oak_rgbd import (
    DEPTH_ENCODING,
    DEPTH_RAW_OBSERVATION_ENCODING,
    DEPTH_RAW_SEMANTICS,
    LoadedUnrealRGBDFrame,
    UnrealRGBDFramePaths,
)
from singapore_sensor_fusion.oak_rgbd_viewer import (
    VideoConfig,
    WatchConfig,
    annotate_oak_frame,
    extract_detector_overlays,
    extract_simulation_truth_overlays,
    render_latest_oak_rgbd_video,
    watch_unreal_oak_rgbd,
)


_DIGEST = "A" * 64


def _paths(root: Path, index: int, timestamp: datetime) -> UnrealRGBDFramePaths:
    node = root / "City_Sector"
    node.mkdir(parents=True, exist_ok=True)
    stem = f"frame_{index:06d}"
    return UnrealRGBDFramePaths(
        rgb_path=node / f"{stem}_rgb.png",
        depth_path=node / f"{stem}_depth.png",
        metadata_path=node / f"{stem}_depth.json",
        timestamp=timestamp,
        node_id="City_Sector",
        frame_index=index,
        frame_id=stem,
    )


def _frame(paths: UnrealRGBDFramePaths, *, truth: bool = False) -> LoadedUnrealRGBDFrame:
    metadata = {
        "timestampUtc": paths.timestamp.isoformat().replace("+00:00", "Z"),
        "nodeId": paths.node_id,
        "frameIndex": paths.frame_index,
        "width": 80,
        "height": 60,
        "normalizationMaxMeters": 10_000.0,
        "fovDegrees": 90.0,
    }
    if truth:
        metadata["targets"] = [
            {
                "actorName": "HostileSwarm_03",
                "hostileScenarioTruth": True,
                "bboxXyxyPixels": [12, 10, 35, 31],
                "distanceMeters": 407.0,
                "inFront": True,
                "intersectsFrame": True,
            }
        ]
    return LoadedUnrealRGBDFrame(
        paths=paths,
        rgb=np.zeros((60, 80, 3), dtype=np.uint8),
        depth_preview_gray=np.full((60, 80), 10, dtype=np.uint8),
        metadata=metadata,
    )


def _observation(
    *, legacy: bool = False, raw_depth: bool = False, include_depth_source: bool = True
) -> dict:
    measurements = {
        "bbox_xyxy_pixels": [10.0, 8.0, 38.0, 34.0],
        "class_name": "drone",
        "depth_quantization_step_meters": 0.001 if raw_depth else 39.215686,
    }
    if include_depth_source:
        if raw_depth:
            measurements.update(
                {
                    "depth_selected_encoding": DEPTH_RAW_OBSERVATION_ENCODING,
                    "depth_source_semantics": DEPTH_RAW_SEMANTICS,
                    "depth_source_units": "millimeters",
                    "depth_source_byte_order": "little-endian",
                    "depth_source_file": "C:/frames/frame_000001_depth_u32_mm.bin",
                    "depth_source_fallback_reason": None,
                    "range_semantics": f"{DEPTH_RAW_SEMANTICS}_estimate",
                }
            )
        else:
            measurements.update(
                {
                    "depth_selected_encoding": DEPTH_ENCODING,
                    "depth_source_semantics": "normalized_scene_depth_png_preview",
                    "depth_source_units": "meters",
                    "depth_source_byte_order": None,
                    "depth_source_file": "C:/frames/frame_000001_depth.png",
                    "depth_source_fallback_reason": "raw_sidecar_not_declared",
                    "range_semantics": "normalized_scene_depth_png_preview_estimate",
                }
            )
    if legacy:
        measurements["estimated_distance_meters"] = 325.0
    else:
        measurements["depth_z_m"] = 320.0
        measurements["slant_range_m"] = 327.4
    return {
        "observation_id": "rgbd-test",
        "detection_probability": 0.74,
        "measurements": measurements,
    }


class _FakeAdapter:
    verified_sha256 = _DIGEST

    def __init__(self, observations=None) -> None:
        self.observations = tuple(observations or (_observation(),))
        self.calls: list[str] = []

    def infer_unreal_frame(self, frame, *, include_simulation_iou_diagnostic=False):
        self.calls.append(frame.paths.frame_id)
        if include_simulation_iou_diagnostic:
            raise AssertionError("viewer must not mix truth diagnostics into inference output")
        return self.observations


class _FakeBackend:
    name = "fake-drawing"

    def __init__(self, keys=()) -> None:
        self.keys = list(keys)
        self.rectangles: list[tuple[tuple[int, int, int, int], tuple[int, int, int], int]] = []
        self.labels: list[str] = []
        self.shown = 0
        self.closed = 0

    def prepare(self, rgb):
        return rgb[..., ::-1].copy()

    def resize(self, image_bgr, width, height):
        return np.zeros((height, width, 3), dtype=np.uint8)

    def rectangle(self, image_bgr, bounds, color_bgr, *, thickness):
        del image_bgr
        self.rectangles.append((bounds, color_bgr, thickness))

    def text(self, image_bgr, value, origin, color_bgr, *, scale=0.5):
        del image_bgr, origin, color_bgr, scale
        self.labels.append(value)

    def show(self, window_name, image_bgr):
        del window_name, image_bgr
        self.shown += 1

    def wait_key(self, delay_ms):
        del delay_ms
        return self.keys.pop(0) if self.keys else -1

    def close_window(self, window_name):
        del window_name
        self.closed += 1


class _FakeEncoder:
    name = "fake-mp4"

    def __init__(self, path: Path, fps: float, size: tuple[int, int]) -> None:
        self.path = path
        self.fps = fps
        self.size = size
        self.frames: list[np.ndarray] = []

    def write(self, frame_bgr):
        self.frames.append(frame_bgr.copy())

    def close(self):
        self.path.write_bytes(b"fake-mp4")


class OAKRGBDViewerTests(unittest.TestCase):
    def test_extracts_new_distance_fields_and_legacy_fallback(self) -> None:
        current, legacy = extract_detector_overlays(
            [_observation(), _observation(legacy=True)], 80, 60
        )
        self.assertEqual(current.depth_z_m, 320.0)
        self.assertEqual(current.slant_range_m, 327.4)
        self.assertFalse(current.used_legacy_distance)
        self.assertIsNone(legacy.depth_z_m)
        self.assertEqual(legacy.slant_range_m, 325.0)
        self.assertTrue(legacy.used_legacy_distance)
        self.assertAlmostEqual(current.quantization_step_m, 39.215686)
        self.assertEqual(current.depth_selected_encoding, DEPTH_ENCODING)
        self.assertFalse(current.legacy_depth_source_metadata_inferred)

    def test_legacy_source_metadata_remains_preview_compatible(self) -> None:
        overlay = extract_detector_overlays(
            [_observation(include_depth_source=False)], 80, 60
        )[0]

        self.assertEqual(overlay.depth_selected_encoding, DEPTH_ENCODING)
        self.assertEqual(
            overlay.depth_source_semantics, "normalized_scene_depth_png_preview"
        )
        self.assertTrue(overlay.legacy_depth_source_metadata_inferred)
        record = overlay.manifest_record()
        self.assertEqual(
            record["rangeSemantics"],
            "Unreal normalized depth-preview simulation estimate",
        )
        self.assertTrue(record["legacyDepthSourceMetadataInferred"])

    def test_raw_sidecar_source_controls_overlay_and_manifest_labels(self) -> None:
        now = datetime(2026, 1, 1, tzinfo=timezone.utc)
        raw_observation = _observation(raw_depth=True)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths = _paths(root, 1, now)
            frame = _frame(paths)
            backend = _FakeBackend()
            _, detections, _ = annotate_oak_frame(
                frame,
                [raw_observation],
                backend=backend,
            )

            self.assertEqual(len(detections), 1)
            raw = detections[0]
            self.assertEqual(raw.depth_selected_encoding, DEPTH_RAW_OBSERVATION_ENCODING)
            self.assertEqual(raw.depth_source_semantics, DEPTH_RAW_SEMANTICS)
            self.assertEqual(raw.depth_source_units, "millimeters")
            self.assertEqual(raw.depth_source_byte_order, "little-endian")
            self.assertFalse(raw.legacy_depth_source_metadata_inferred)
            self.assertTrue(any("uint32-mm q~1 mm/step" in label for label in backend.labels))
            self.assertTrue(
                any("RANGE: UNREAL UINT32-MM SCENEDEPTH SIDECAR" in label for label in backend.labels)
            )
            self.assertFalse(any("8-bit q~" in label for label in backend.labels))

            encoders: list[_FakeEncoder] = []

            def factory(path, fps, size):
                encoder = _FakeEncoder(path, fps, size)
                encoders.append(encoder)
                return encoder

            output = root / "raw_depth_detected.mp4"
            manifest = render_latest_oak_rgbd_video(
                VideoConfig(
                    frames_root=root,
                    checkpoint_path=root / "trusted.pt",
                    trusted_sha256=_DIGEST,
                    output_path=output,
                    node_id="City_Sector",
                ),
                adapter=_FakeAdapter((raw_observation,)),
                backend=_FakeBackend(),
                encoder_factory=factory,
                frame_selector=lambda *args, **kwargs: (paths,),
                frame_loader=lambda selected: frame,
            )

            frame_record = manifest["frames"][0]
            self.assertEqual(
                frame_record["depthSelectedEncoding"], DEPTH_RAW_OBSERVATION_ENCODING
            )
            self.assertEqual(frame_record["depthSourceSemantics"], DEPTH_RAW_SEMANTICS)
            self.assertEqual(frame_record["depthQuantizationStepMeters"], 0.001)
            self.assertEqual(frame_record["depthSourceMetadataOrigin"], "detector_observations")
            detection_record = frame_record["detections"][0]
            self.assertEqual(
                detection_record["depthSelectedEncoding"],
                DEPTH_RAW_OBSERVATION_ENCODING,
            )
            self.assertEqual(
                detection_record["depthSourceDisplay"],
                "UINT32-MM SCENEDEPTH SIDECAR",
            )
            self.assertEqual(
                detection_record["rangeSemantics"],
                f"{DEPTH_RAW_SEMANTICS}_estimate",
            )

    def test_truth_is_explicitly_debug_only_and_not_detector_output(self) -> None:
        now = datetime(2026, 1, 1, tzinfo=timezone.utc)
        with tempfile.TemporaryDirectory() as temporary:
            frame = _frame(_paths(Path(temporary), 1, now), truth=True)
            truth = extract_simulation_truth_overlays(frame.metadata, 80, 60)
            self.assertEqual(len(truth), 1)
            self.assertTrue(truth[0].hostile)
            backend = _FakeBackend()
            _, detections, rendered_truth = annotate_oak_frame(
                frame,
                [_observation()],
                backend=backend,
                show_simulation_truth=True,
            )
            self.assertEqual(len(detections), 1)
            self.assertEqual(len(rendered_truth), 1)
            self.assertTrue(any(label.startswith("OAK RGB MODEL") for label in backend.labels))
            self.assertTrue(any(label.startswith("SIM TRUTH (DEBUG ONLY)") for label in backend.labels))
            self.assertTrue(any("depth-Z 320 m" in label for label in backend.labels))
            self.assertTrue(any("slant 327 m" in label for label in backend.labels))
            self.assertTrue(any("8-bit q~39.2 m/level" in label for label in backend.labels))
            self.assertTrue(any("SIMULATION ESTIMATE" in label for label in backend.labels))
            self.assertEqual(sorted(item[2] for item in backend.rectangles), [1, 3])

    def test_watch_skips_stale_frame_and_is_headless_and_bounded(self) -> None:
        now = datetime(2026, 1, 1, 12, 0, tzinfo=timezone.utc)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            stale = _paths(root, 1, now - timedelta(seconds=20))
            fresh = _paths(root, 2, now - timedelta(seconds=1))
            frames = {stale.frame_id: _frame(stale), fresh.frame_id: _frame(fresh)}
            adapter = _FakeAdapter()
            backend = _FakeBackend()
            report_path = root / "watch.json"

            report = watch_unreal_oak_rgbd(
                WatchConfig(
                    frames_root=root,
                    checkpoint_path=root / "trusted.pt",
                    trusted_sha256=_DIGEST,
                    node_id="City_Sector",
                    report_path=report_path,
                    duration_seconds=5.0,
                    max_frames=1,
                    max_frame_age_seconds=5.0,
                    display_window=False,
                ),
                adapter=adapter,
                backend=backend,
                frame_selector=lambda *args, **kwargs: (stale, fresh),
                frame_loader=lambda paths: frames[paths.frame_id],
                monotonic=lambda: 0.0,
                sleep=lambda seconds: None,
                now_utc=lambda: now,
            )

            self.assertEqual(adapter.calls, [fresh.frame_id])
            self.assertEqual(report["stopReason"], "frame_limit")
            self.assertEqual(report["summary"]["staleFrameCountSkipped"], 1)
            self.assertEqual(report["summary"]["oakModelDetectionCount"], 1)
            self.assertEqual(backend.shown, 0)
            self.assertEqual(backend.closed, 0)
            self.assertEqual(json.loads(report_path.read_text())["stopReason"], "frame_limit")

    def test_watch_window_q_exits_and_closes(self) -> None:
        now = datetime(2026, 1, 1, tzinfo=timezone.utc)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths = _paths(root, 1, now)
            backend = _FakeBackend(keys=[ord("q")])
            report = watch_unreal_oak_rgbd(
                WatchConfig(
                    frames_root=root,
                    checkpoint_path=root / "trusted.pt",
                    trusted_sha256=_DIGEST,
                    node_id="City_Sector",
                    duration_seconds=5.0,
                    max_frames=10,
                    max_frame_age_seconds=5.0,
                ),
                adapter=_FakeAdapter(),
                backend=backend,
                frame_selector=lambda *args, **kwargs: (paths,),
                frame_loader=lambda selected: _frame(selected),
                monotonic=lambda: 0.0,
                sleep=lambda seconds: None,
                now_utc=lambda: now,
            )
            self.assertEqual(report["stopReason"], "user_exit")
            self.assertEqual(backend.shown, 1)
            self.assertEqual(backend.closed, 1)

    def test_watch_atomically_refreshes_running_report_before_display(self) -> None:
        now = datetime(2026, 1, 1, tzinfo=timezone.utc)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            paths = _paths(root, 7, now)
            report_path = root / "oak_rgbd_live_watch_latest.json"

            class InspectingBackend(_FakeBackend):
                def show(self, window_name, image_bgr):
                    del window_name, image_bgr
                    live = json.loads(report_path.read_text(encoding="utf-8"))
                    self.assertions = {
                        "state": live["reportState"],
                        "stop": live["stopReason"],
                        "frame": live["processedFrames"][-1]["frameId"],
                    }
                    self.shown += 1

            backend = InspectingBackend(keys=[ord("q")])
            final = watch_unreal_oak_rgbd(
                WatchConfig(
                    frames_root=root,
                    checkpoint_path=root / "trusted.pt",
                    trusted_sha256=_DIGEST,
                    node_id="City_Sector",
                    report_path=report_path,
                    duration_seconds=5.0,
                    max_frames=10,
                    max_frame_age_seconds=5.0,
                ),
                adapter=_FakeAdapter(),
                backend=backend,
                frame_selector=lambda *args, **kwargs: (paths,),
                frame_loader=lambda selected: _frame(selected),
                monotonic=lambda: 0.0,
                sleep=lambda seconds: None,
                now_utc=lambda: now,
            )

            self.assertEqual(
                backend.assertions,
                {"state": "running", "stop": "running", "frame": paths.frame_id},
            )
            self.assertEqual(final["reportState"], "complete")
            self.assertEqual(json.loads(report_path.read_text())["stopReason"], "user_exit")

    def test_offline_video_and_manifest_are_published_from_fake_adapter(self) -> None:
        now = datetime(2026, 1, 1, tzinfo=timezone.utc)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            selected = (_paths(root, 1, now), _paths(root, 2, now + timedelta(seconds=1)))
            adapter = _FakeAdapter()
            backend = _FakeBackend()
            encoders: list[_FakeEncoder] = []

            def factory(path, fps, size):
                encoder = _FakeEncoder(path, fps, size)
                encoders.append(encoder)
                return encoder

            output = root / "oak_detected.mp4"
            manifest = render_latest_oak_rgbd_video(
                VideoConfig(
                    frames_root=root,
                    checkpoint_path=root / "trusted.pt",
                    trusted_sha256=_DIGEST,
                    output_path=output,
                    node_id="City_Sector",
                    fps=4.0,
                    show_simulation_truth=True,
                ),
                adapter=adapter,
                backend=backend,
                encoder_factory=factory,
                frame_selector=lambda *args, **kwargs: selected,
                frame_loader=lambda paths: _frame(paths, truth=True),
            )

            manifest_path = output.with_suffix(".manifest.json")
            self.assertEqual(output.read_bytes(), b"fake-mp4")
            self.assertTrue(manifest_path.is_file())
            self.assertEqual(len(encoders[0].frames), 2)
            self.assertEqual(encoders[0].size, (80, 60))
            self.assertEqual(manifest["counts"]["oakModelDetections"], 2)
            self.assertEqual(manifest["counts"]["simulationTruthDebugBoxes"], 2)
            self.assertFalse(manifest["semantics"]["simulationTruthIsDetectorOutput"])
            self.assertEqual(manifest["video"]["durationSeconds"], 0.5)
            self.assertEqual(
                json.loads(manifest_path.read_text())["inputs"]["verifiedCheckpointSha256"],
                _DIGEST,
            )


if __name__ == "__main__":
    unittest.main()
