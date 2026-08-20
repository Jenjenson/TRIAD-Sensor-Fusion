from __future__ import annotations

from datetime import datetime, timezone
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

import numpy as np

from singapore_sensor_fusion.event_camera import PROXY_EVENT_ENCODING
from singapore_sensor_fusion.experiment import (
    MissingOptionalDependencyError,
    _load_rgb_image,
    find_newest_rf_telemetry,
    run_experiment,
    write_report_atomic,
)
from singapore_sensor_fusion.observations import SensorModality, SensorObservation


def _rf_record(
    *,
    node: str,
    target: str,
    frequency_ghz: float,
    snr_db: float,
    detected: bool,
) -> dict[str, object]:
    return {
        "timestampUtc": "2026-08-01T00:00:00Z",
        "simulationSeconds": 1.0,
        "nodeId": node,
        "targetActor": target,
        "emitterId": f"{target}-rf",
        "frequencyGHz": frequency_ghz,
        "distanceMeters": 1000.0,
        "lineOfSight": True,
        "freeSpacePathLossDb": 100.0,
        "obstructionLossDb": 0.0,
        "receivedPowerDbm": -70.0,
        "noiseFloorDbm": -90.0,
        "snrDb": snr_db,
        "frequencySupported": True,
        "inRange": True,
        "aboveSensitivity": True,
        "detected": detected,
    }


def _write_jsonl(path: Path, records: list[dict[str, object]]) -> None:
    path.write_text(
        "".join(json.dumps(record) + "\n" for record in records),
        encoding="utf-8",
    )


def _make_frame_pair(saved_dir: Path, node: str = "Node-A") -> tuple[Path, Path]:
    directory = saved_dir / "frames" / node
    directory.mkdir(parents=True)
    previous = directory / "frame_000000_20260801T000000000000Z_rgb.png"
    current = directory / "frame_000001_20260801T000000033000Z_rgb.png"
    previous.touch()
    current.touch()
    return previous, current


class ExperimentTests(unittest.TestCase):
    def test_newest_telemetry_is_bounded_and_never_claims_sirfnet(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            saved = root / "Saved" / "SingaporeSensorFusion"
            saved.mkdir(parents=True)
            old = saved / "rf_links_20260731T000000Z_old.jsonl"
            new = saved / "rf_links_20260801T000000Z_new.jsonl"
            _write_jsonl(old, [_rf_record(
                node="Old", target="Drone-Old", frequency_ghz=5.8,
                snr_db=1.0, detected=True,
            )])
            _write_jsonl(
                new,
                [
                    _rf_record(
                        node="Node-A", target="Drone-1", frequency_ghz=2.4,
                        snr_db=10.0, detected=True,
                    ),
                    _rf_record(
                        node="Node-A", target="Drone-2", frequency_ghz=5.8,
                        snr_db=-20.0, detected=False,
                    ),
                    _rf_record(
                        node="Node-B", target="Drone-3", frequency_ghz=2.4,
                        snr_db=5.0, detected=True,
                    ),
                ],
            )
            os.utime(old, (1, 1))
            os.utime(new, (2, 2))

            self.assertEqual(find_newest_rf_telemetry(saved), new.resolve())
            report = run_experiment(
                saved_dir=saved,
                model_root=root / "missing-models",
                max_records=2,
                max_event_pairs=1,
                skip_event_inference=True,
            )

            self.assertEqual(report["rf"]["summary"]["observation_count"], 2)
            self.assertTrue(report["rf"]["telemetry"]["truncated_by_record_limit"])
            samples = report["rf"]["observation_samples"]
            self.assertTrue(samples)
            self.assertTrue(
                all(sample["model_id"].startswith("analytic_") for sample in samples)
            )
            self.assertTrue(
                all(sample["measurements"]["sirfnet_inference_performed"] is False
                    for sample in samples)
            )
            self.assertEqual(samples[1]["detection_probability"], 0.0)
            self.assertEqual(report["fusion"]["node_result_count"], 1)
            self.assertFalse(report["scope"]["engagement_logic_present"])

    def test_proxy_pairs_are_created_while_inference_is_skipped(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            saved = root / "SingaporeSensorFusion"
            previous, current = _make_frame_pair(saved)

            def fake_load(path: Path) -> np.ndarray:
                value = 0 if path == previous else 255
                return np.full((6, 8, 3), value, dtype=np.uint8)

            with patch(
                "singapore_sensor_fusion.experiment._load_rgb_image",
                side_effect=fake_load,
            ), patch(
                "singapore_sensor_fusion.experiment.EventYOLOAdapter"
            ) as adapter_class:
                report = run_experiment(
                    saved_dir=saved,
                    model_root=root / "missing-models",
                    max_records=1,
                    max_event_pairs=1,
                    skip_event_inference=True,
                )

            adapter_class.assert_not_called()
            event = report["event_camera"]
            self.assertEqual(event["source_encoding"], PROXY_EVENT_ENCODING)
            self.assertFalse(event["preprocessing_equivalent_to_training"])
            self.assertEqual(event["proxy_stacks_created"], 1)
            self.assertEqual(event["event_model_inference_status"], "skipped_by_user")
            self.assertEqual(event["event_observation_count"], 0)

    def test_mock_event_adapter_produces_observation_without_gpu(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            saved = root / "SingaporeSensorFusion"
            previous, current = _make_frame_pair(saved, "Node-Event")

            def fake_load(path: Path) -> np.ndarray:
                value = 32 if path == previous else 192
                return np.full((5, 7, 3), value, dtype=np.uint8)

            class FakeAdapter:
                def __init__(self, *args: object, **kwargs: object) -> None:
                    pass

                def infer(
                    self,
                    projected_rgb: np.ndarray,
                    *,
                    node_id: str,
                    timestamp: datetime,
                    frame_id: str,
                    source_encoding: str,
                ) -> tuple[SensorObservation, ...]:
                    self.assert_proxy(projected_rgb, source_encoding)
                    return (
                        SensorObservation(
                            observation_id="mock-event-1",
                            timestamp=timestamp,
                            node_id=node_id,
                            modality=SensorModality.EVENT_CAMERA,
                            detection_probability=0.8,
                            confidence_level=0.8,
                            model_id="mock-event-yolo",
                            measurements={
                                "source_encoding": source_encoding,
                                "preprocessing_equivalent_to_training": False,
                            },
                        ),
                    )

                @staticmethod
                def assert_proxy(image: np.ndarray, encoding: str) -> None:
                    if image.shape != (5, 7, 3) or encoding != PROXY_EVENT_ENCODING:
                        raise AssertionError("adapter did not receive a proxy event image")

            with patch(
                "singapore_sensor_fusion.experiment._load_rgb_image",
                side_effect=fake_load,
            ), patch(
                "singapore_sensor_fusion.experiment.EventYOLOAdapter",
                FakeAdapter,
            ):
                report = run_experiment(
                    saved_dir=saved,
                    model_root=root / "missing-models",
                    max_records=1,
                    max_event_pairs=1,
                    skip_event_inference=False,
                )

            self.assertEqual(
                report["event_camera"]["event_model_inference_status"], "completed"
            )
            self.assertEqual(report["event_camera"]["event_observation_count"], 1)
            self.assertEqual(report["fusion"]["node_result_count"], 1)
            fused = report["fusion"]["nodes"][0]
            self.assertAlmostEqual(fused["fused_probability_lower_bound"], 0.064)
            self.assertFalse(report["fusion"]["independence_assumed"])

    def test_missing_pillow_error_is_explicit(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "frame.png"
            path.touch()
            with patch.dict(sys.modules, {"PIL": None}):
                with self.assertRaisesRegex(
                    MissingOptionalDependencyError, "Pillow is required"
                ):
                    _load_rgb_image(path)

    def test_report_write_is_atomic_and_json_safe(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "nested" / "report.json"
            written = write_report_atomic(
                {"generated": datetime.now(timezone.utc).isoformat(), "value": 1},
                destination,
            )
            self.assertEqual(written, destination.resolve())
            self.assertEqual(json.loads(destination.read_text(encoding="utf-8"))["value"], 1)
            self.assertEqual(list(destination.parent.glob("*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
