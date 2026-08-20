from datetime import datetime, timezone
from hashlib import sha256
from pathlib import Path
import sys
import tempfile
from types import ModuleType, SimpleNamespace
import unittest
from unittest.mock import patch

import numpy as np

from singapore_sensor_fusion.event_camera import PROXY_EVENT_ENCODING
from singapore_sensor_fusion.event_yolo import EventYOLOAdapter, ModelBlockedError


class _FakeYOLOModel:
    names = {0: "item"}

    def __init__(self, checkpoint: str) -> None:
        self.checkpoint = checkpoint
        self.predict_kwargs = None

    def predict(self, **kwargs):
        self.predict_kwargs = kwargs
        boxes = SimpleNamespace(
            xyxy=np.array([[10.0, 5.0, 90.0, 45.0], [-5.0, 0.0, 120.0, 50.0]]),
            conf=np.array([0.91, 0.40]),
            cls=np.array([0.0, 0.0]),
        )
        return [SimpleNamespace(boxes=boxes, names=self.names)]


class EventYOLOAdapterTests(unittest.TestCase):
    def _checkpoint(self, directory: str) -> tuple[Path, str]:
        content = b"trusted event checkpoint fixture"
        path = Path(directory) / "event.pt"
        path.write_bytes(content)
        return path, sha256(content).hexdigest().upper()

    def test_hash_mismatch_blocks_before_deserialization(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path, _ = self._checkpoint(directory)
            adapter = EventYOLOAdapter(path, trusted_sha256="0" * 64)
            with self.assertRaises(ModelBlockedError) as caught:
                adapter.load()
        self.assertEqual(caught.exception.code, "blocked_integrity_mismatch")
        self.assertFalse(adapter.loaded)

    def test_fake_runtime_produces_bounded_normalized_observations(self) -> None:
        module = ModuleType("ultralytics")
        created = []

        def fake_yolo(checkpoint: str) -> _FakeYOLOModel:
            model = _FakeYOLOModel(checkpoint)
            created.append(model)
            return model

        module.YOLO = fake_yolo
        with tempfile.TemporaryDirectory() as directory:
            path, digest = self._checkpoint(directory)
            adapter = EventYOLOAdapter(
                path,
                trusted_sha256=digest,
                confidence_threshold=0.5,
                max_detections=1,
            )
            image = np.zeros((50, 100, 3), dtype=np.uint8)
            with patch.dict(sys.modules, {"ultralytics": module}):
                observations = adapter.infer(
                    image,
                    node_id="sector-east-01",
                    timestamp=datetime(2026, 8, 1, tzinfo=timezone.utc),
                    frame_id="frame-7",
                    source_encoding=PROXY_EVENT_ENCODING,
                )

        self.assertTrue(adapter.loaded)
        self.assertEqual(len(created), 1)
        self.assertEqual(len(observations), 1)
        observation = observations[0]
        self.assertAlmostEqual(observation.detection_probability, 0.91)
        self.assertEqual(
            observation.measurements["bbox_xyxy_normalized"], [0.1, 0.1, 0.9, 0.9]
        )
        self.assertEqual(observation.measurements["source_encoding"], PROXY_EVENT_ENCODING)
        self.assertFalse(observation.measurements["preprocessing_equivalent_to_training"])
        self.assertTrue(observation.measurements["detection_only"])
        self.assertEqual(observation.measurements["frame_id"], "frame-7")
        self.assertEqual(created[0].predict_kwargs["max_det"], 1)

    def test_rejects_non_rgb_and_untrusted_artifact_types(self) -> None:
        with self.assertRaises(ModelBlockedError) as caught:
            EventYOLOAdapter(Path("model.engine"))
        self.assertEqual(caught.exception.code, "blocked_untrusted_artifact_type")

        with tempfile.TemporaryDirectory() as directory:
            path, digest = self._checkpoint(directory)
            adapter = EventYOLOAdapter(path, trusted_sha256=digest)
            with self.assertRaisesRegex(ValueError, "shape"):
                adapter.infer(np.zeros((10, 10), dtype=np.uint8), node_id="node")


if __name__ == "__main__":
    unittest.main()
