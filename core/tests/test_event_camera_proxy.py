import unittest

import numpy as np

from singapore_sensor_fusion.event_camera import (
    PROXY_EVENT_ENCODING,
    ProxyEventProjector,
    ProxyEventProjectorConfig,
    project_proxy_event_stack,
)


class ProxyEventCameraTests(unittest.TestCase):
    def test_static_frame_has_no_proxy_events(self) -> None:
        frame = np.full((5, 7, 3), 120, dtype=np.uint8)
        projected = project_proxy_event_stack(frame, frame)
        self.assertEqual(projected.shape, (5, 7, 3))
        self.assertEqual(projected.dtype, np.uint8)
        self.assertFalse(np.any(projected))

    def test_channel_mapping_separates_positive_and_negative_polarity(self) -> None:
        settings = ProxyEventProjectorConfig(contrast_threshold=0.05, count_clip=4)
        dark = np.full((2, 2, 3), 16, dtype=np.uint8)
        bright = np.full((2, 2, 3), 240, dtype=np.uint8)

        positive = project_proxy_event_stack(dark, bright, config=settings)
        negative = project_proxy_event_stack(bright, dark, config=settings)

        self.assertTrue(np.all(positive[..., 0] == 255))
        self.assertTrue(np.all(positive[..., 1] > 0))
        self.assertTrue(np.all(positive[..., 2] == 0))
        self.assertTrue(np.all(negative[..., 0] == 0))
        self.assertTrue(np.all(negative[..., 1] > 0))
        self.assertTrue(np.all(negative[..., 2] == 255))

    def test_streaming_projector_retains_only_previous_frame(self) -> None:
        projector = ProxyEventProjector()
        first = np.zeros((3, 4, 3), dtype=np.uint8)
        second = np.full((3, 4, 3), 255, dtype=np.uint8)
        self.assertEqual(projector.encoding, PROXY_EVENT_ENCODING)
        self.assertFalse(projector.equivalent_to_training_preprocessing)
        self.assertIsNone(projector.push(first, timestamp_seconds=1.0))
        output = projector.push(second, timestamp_seconds=1.02)
        self.assertIsNotNone(output)
        self.assertEqual(output.shape, second.shape)
        with self.assertRaisesRegex(ValueError, "monotonic"):
            projector.push(first, timestamp_seconds=0.5)
        projector.reset()
        self.assertFalse(projector.has_previous_frame)

    def test_rejects_shape_and_range_mismatch(self) -> None:
        with self.assertRaisesRegex(ValueError, "identical"):
            project_proxy_event_stack(
                np.zeros((2, 2, 3), dtype=np.uint8),
                np.zeros((3, 2, 3), dtype=np.uint8),
            )
        with self.assertRaisesRegex(ValueError, r"\[0, 1\]"):
            project_proxy_event_stack(
                np.zeros((2, 2, 3), dtype=np.float32),
                np.full((2, 2, 3), 2.0, dtype=np.float32),
            )


if __name__ == "__main__":
    unittest.main()
