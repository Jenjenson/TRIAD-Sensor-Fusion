from hashlib import sha256
from pathlib import Path
import tempfile
import unittest

from singapore_sensor_fusion.model_doctor import (
    ModelSpec,
    default_model_specs,
    inspect_model,
)


class ModelDoctorTests(unittest.TestCase):
    def test_default_manifest_excludes_all_supplied_rf_artifacts(self) -> None:
        specs = default_model_specs(Path("C:/models"))
        self.assertEqual({spec.modality for spec in specs}, {"rgbd", "event_camera"})
        self.assertFalse(any("final_all_data" in spec.path.name for spec in specs))

    def _artifact(self, root: str, name: str, content: bytes) -> tuple[Path, str]:
        path = Path(root) / name
        path.write_bytes(content)
        return path, sha256(content).hexdigest().upper()

    def test_rf_checkpoint_stays_blocked_without_contract(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path, digest = self._artifact(directory, "rf.zip", b"trusted rf fixture")
            report = inspect_model(
                ModelSpec("rf-test", "rf", path, digest, "pytorch_checkpoint_zip")
            )
        self.assertTrue(report.integrity_ok)
        self.assertEqual(report.status, "blocked_missing_architecture_preprocessing")
        self.assertFalse(report.ready)
        self.assertFalse(report.details["deserialized"])

    def test_engine_records_hardware_and_runtime_block(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path, digest = self._artifact(directory, "model.engine", b"trusted engine fixture")
            report = inspect_model(
                ModelSpec(
                    "rgbd-test", "rgbd", path, digest, "tensorrt_serialized_engine"
                ),
                local_sm="sm70",
                tensorrt_available=False,
            )
        self.assertEqual(report.status, "blocked_incompatible_runtime_hardware")
        self.assertEqual(report.details["plan_compute_capability"], "sm120")
        self.assertEqual(report.details["local_compute_capability"], "sm70")
        self.assertFalse(report.details["tensorrt_python_available"])
        self.assertFalse(report.details["deserialized"])

    def test_event_checkpoint_ready_only_when_hash_and_dependency_match(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path, digest = self._artifact(directory, "event.pt", b"trusted event fixture")
            spec = ModelSpec(
                "event-test", "event_camera", path, digest, "ultralytics_pytorch_checkpoint"
            )
            ready = inspect_model(spec, ultralytics_available=True)
            blocked = inspect_model(spec, ultralytics_available=False)
        self.assertEqual(ready.status, "ready_trusted_checkpoint")
        self.assertTrue(ready.ready)
        self.assertEqual(blocked.status, "blocked_missing_ultralytics")

    def test_missing_and_hash_mismatch_are_reported_before_adapter_status(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            missing = inspect_model(
                ModelSpec(
                    "missing", "rf", Path(directory) / "none.zip", "0" * 64, "zip"
                )
            )
            path, _ = self._artifact(directory, "changed.pt", b"different")
            mismatch = inspect_model(
                ModelSpec(
                    "changed", "event_camera", path, "A" * 64, "ultralytics_pytorch_checkpoint"
                ),
                ultralytics_available=True,
            )
        self.assertEqual(missing.status, "blocked_missing_file")
        self.assertEqual(mismatch.status, "blocked_integrity_mismatch")


if __name__ == "__main__":
    unittest.main()
