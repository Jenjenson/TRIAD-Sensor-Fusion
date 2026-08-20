"""Read-only readiness checks for the supplied visual detection artifacts.

The doctor hashes files and inspects Python dependency availability.  It never
deserializes a PyTorch checkpoint or TensorRT plan.  User-supplied RF
checkpoints are deliberately excluded from the default manifest and runtime.

Run with::

    python -m singapore_sensor_fusion.model_doctor --json
"""

from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import hashlib
import importlib.util
import json
import os
from pathlib import Path
from typing import Iterable, Sequence

from .paths import DEFAULT_MODEL_ROOT

RGBD_ENGINE_SHA256 = "5F16C8B49C9E2C619A1F9E3F28B5541736479B1513D32C08A50984721B0A2DA9"
EVENT_YOLO_SHA256 = "22AE317D35F209A00DE0B9DED20DD215121A9B0012A8993DBDDB7703DBB2C35D"


@dataclass(frozen=True, slots=True)
class ModelSpec:
    model_id: str
    modality: str
    path: Path
    expected_sha256: str
    artifact_type: str


@dataclass(frozen=True, slots=True)
class ModelReadiness:
    model_id: str
    modality: str
    path: str
    artifact_type: str
    exists: bool
    size_bytes: int | None
    expected_sha256: str
    actual_sha256: str | None
    integrity_ok: bool
    status: str
    reason: str
    details: dict[str, object]

    @property
    def ready(self) -> bool:
        return self.status == "ready_trusted_checkpoint"

    def to_dict(self) -> dict[str, object]:
        value = asdict(self)
        value["ready"] = self.ready
        return value


def default_model_specs(model_root: str | Path | None = None) -> tuple[ModelSpec, ...]:
    """Return the visual-model manifest; supplied RF artifacts are excluded."""

    root = Path(model_root) if model_root is not None else DEFAULT_MODEL_ROOT
    return (
        ModelSpec(
            model_id="rgbd_fp8_detector",
            modality="rgbd",
            path=root / "model_fp8.engine",
            expected_sha256=RGBD_ENGINE_SHA256,
            artifact_type="tensorrt_serialized_engine",
        ),
        ModelSpec(
            model_id="fred_event_yolo26s_p2",
            modality="event_camera",
            path=root / "fred_yolo26s_p2_eventstack_activegate_best.pt",
            expected_sha256=EVENT_YOLO_SHA256,
            artifact_type="ultralytics_pytorch_checkpoint",
        ),
    )


def sha256_file(path: str | Path, *, chunk_bytes: int = 1024 * 1024) -> str:
    """Hash a file using bounded memory."""

    if chunk_bytes < 1:
        raise ValueError("chunk_bytes must be >= 1")
    digest = hashlib.sha256()
    with Path(path).open("rb") as handle:
        while chunk := handle.read(chunk_bytes):
            digest.update(chunk)
    return digest.hexdigest().upper()


def _module_available(name: str) -> bool:
    try:
        return importlib.util.find_spec(name) is not None
    except (ImportError, ValueError):
        return False


def inspect_model(
    spec: ModelSpec,
    *,
    local_sm: str = "sm70",
    ultralytics_available: bool | None = None,
    tensorrt_available: bool | None = None,
) -> ModelReadiness:
    """Inspect one artifact without loading or deserializing its contents."""

    path = spec.path.expanduser().resolve()
    expected = spec.expected_sha256.upper()
    exists = path.is_file()
    size = path.stat().st_size if exists else None
    actual = sha256_file(path) if exists else None
    integrity_ok = bool(actual and actual == expected)

    base = dict(
        model_id=spec.model_id,
        modality=spec.modality,
        path=str(path),
        artifact_type=spec.artifact_type,
        exists=exists,
        size_bytes=size,
        expected_sha256=expected,
        actual_sha256=actual,
        integrity_ok=integrity_ok,
    )
    if not exists:
        return ModelReadiness(
            **base,
            status="blocked_missing_file",
            reason="The configured artifact path does not exist.",
            details={},
        )
    if not integrity_ok:
        return ModelReadiness(
            **base,
            status="blocked_integrity_mismatch",
            reason="The artifact hash differs from the trusted deployment manifest.",
            details={},
        )

    if spec.modality == "rf":
        return ModelReadiness(
            **base,
            status="blocked_missing_architecture_preprocessing",
            reason=(
                "Checkpoint weights are present, but the exact SiRFNet class, input-window "
                "shape, PSD/IQ preprocessing, normalization, and calibration are unavailable."
            ),
            details={
                "deserialized": False,
                "fallback_allowed": "analytic_energy_detector_only_and_must_be_labeled",
            },
        )

    if spec.artifact_type == "tensorrt_serialized_engine":
        runtime_available = (
            _module_available("tensorrt")
            if tensorrt_available is None
            else bool(tensorrt_available)
        )
        return ModelReadiness(
            **base,
            status="blocked_incompatible_runtime_hardware",
            reason=(
                "The serialized plan targets sm120 with hardware/version compatibility "
                "disabled; this deployment host is sm70, and the plan must not be deserialized."
            ),
            details={
                "plan_compute_capability": "sm120",
                "local_compute_capability": local_sm,
                "tensorrt_python_available": runtime_available,
                "hardware_compatible": local_sm.lower() == "sm120",
                "deserialized": False,
                "required_action": (
                    "Re-export from the original trusted ONNX/model source for sm70 and "
                    "provide class-map, box convention, normalization, and depth encoding."
                ),
            },
        )

    if spec.artifact_type == "ultralytics_pytorch_checkpoint":
        dependency_available = (
            _module_available("ultralytics")
            if ultralytics_available is None
            else bool(ultralytics_available)
        )
        if dependency_available:
            return ModelReadiness(
                **base,
                status="ready_trusted_checkpoint",
                reason=(
                    "Trusted checkpoint hash matches and Ultralytics is importable; loading "
                    "remains lazy until inference is explicitly requested."
                ),
                details={
                    "deserialized": False,
                    "input_contract": "3-channel projected event image",
                    "proxy_preprocessing_equivalent_to_training": False,
                },
            )
        return ModelReadiness(
            **base,
            status="blocked_missing_ultralytics",
            reason="Trusted checkpoint is present, but the Ultralytics package is unavailable.",
            details={"deserialized": False},
        )

    return ModelReadiness(
        **base,
        status="blocked_unknown_artifact_contract",
        reason="No safe adapter is registered for this artifact contract.",
        details={"deserialized": False},
    )


def doctor_models(
    specs: Iterable[ModelSpec] | None = None,
    *,
    local_sm: str = "sm70",
    ultralytics_available: bool | None = None,
    tensorrt_available: bool | None = None,
) -> list[ModelReadiness]:
    """Inspect a model manifest in stable order."""

    selected = tuple(specs) if specs is not None else default_model_specs()
    return [
        inspect_model(
            spec,
            local_sm=local_sm,
            ultralytics_available=ultralytics_available,
            tensorrt_available=tensorrt_available,
        )
        for spec in selected
    ]


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--model-root",
        type=Path,
        default=DEFAULT_MODEL_ROOT,
        help="Directory containing the supplied RGB-D and event artifacts.",
    )
    parser.add_argument(
        "--local-sm",
        default=os.environ.get("SINGAPORE_FUSION_LOCAL_SM", "sm70"),
        help="Local CUDA compute capability label (deployment default: sm70).",
    )
    parser.add_argument("--json", action="store_true", help="Emit machine-readable JSON.")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)
    reports = doctor_models(default_model_specs(args.model_root), local_sm=args.local_sm)
    if args.json:
        print(json.dumps([report.to_dict() for report in reports], indent=2))
    else:
        for report in reports:
            print(f"{report.model_id}: {report.status}")
            print(f"  path: {report.path}")
            print(f"  sha256: {report.actual_sha256 or 'unavailable'}")
            print(f"  reason: {report.reason}")
    return 0 if all(report.exists and report.integrity_ok for report in reports) else 2


if __name__ == "__main__":  # pragma: no cover - exercised via ``python -m``
    raise SystemExit(main())
