"""Lazy, hash-pinned adapter for the supplied event-image YOLO checkpoint.

Only an explicitly trusted ``.pt`` checkpoint whose SHA-256 matches the
configured value is passed to Ultralytics.  RF checkpoints and TensorRT plans
are never deserialized by this module.  Input may come from a real, correctly
preprocessed event frame or from ``proxy_event_stack_v1``; the latter is always
marked as non-equivalent to the checkpoint's unavailable training pipeline.
"""

from __future__ import annotations

from datetime import datetime, timezone
import math
from pathlib import Path
from typing import Any
from uuid import uuid4

import numpy as np
from numpy.typing import ArrayLike, NDArray

from .event_camera import PROXY_EVENT_ENCODING
from .model_doctor import EVENT_YOLO_SHA256, sha256_file
from .observations import SensorModality, SensorObservation


class ModelBlockedError(RuntimeError):
    """A model cannot be used safely in the current runtime."""

    def __init__(self, code: str, message: str) -> None:
        super().__init__(message)
        self.code = code


def _as_numpy(value: Any) -> NDArray[np.generic]:
    """Convert a NumPy/torch-like output without importing torch."""

    if hasattr(value, "detach"):
        value = value.detach()
    if hasattr(value, "cpu"):
        value = value.cpu()
    if hasattr(value, "numpy"):
        value = value.numpy()
    return np.asarray(value)


def _model_input(projected_rgb: ArrayLike) -> NDArray[np.uint8]:
    value = np.asarray(projected_rgb)
    if value.ndim != 3 or value.shape[2] != 3 or value.shape[0] < 1 or value.shape[1] < 1:
        raise ValueError("projected_rgb must have non-empty shape (height, width, 3)")
    if np.issubdtype(value.dtype, np.integer):
        if value.min() < 0 or value.max() > 255:
            raise ValueError("integer projected_rgb values must be in [0, 255]")
        result = value.astype(np.uint8, copy=False)
    elif np.issubdtype(value.dtype, np.floating):
        if not np.isfinite(value).all() or value.min() < 0.0 or value.max() > 1.0:
            raise ValueError("floating-point projected_rgb values must be finite and in [0, 1]")
        result = np.rint(value * 255.0).astype(np.uint8)
    else:
        raise TypeError("projected_rgb must use an integer or floating-point dtype")
    return np.ascontiguousarray(result)


class EventYOLOAdapter:
    """Bounded Event-YOLO inference producing shared sensor observations.

    Loading is delayed until the first call to :meth:`load` or :meth:`infer`.
    The adapter retains one model object and no frame history.  At most
    ``max_detections`` observations are returned per inference call.
    """

    def __init__(
        self,
        checkpoint_path: str | Path,
        *,
        trusted_sha256: str = EVENT_YOLO_SHA256,
        confidence_threshold: float = 0.25,
        max_detections: int = 100,
        device: str = "cpu",
    ) -> None:
        self.checkpoint_path = Path(checkpoint_path).expanduser().resolve()
        self.trusted_sha256 = trusted_sha256.upper()
        if len(self.trusted_sha256) != 64 or any(
            character not in "0123456789ABCDEF" for character in self.trusted_sha256
        ):
            raise ValueError("trusted_sha256 must be a 64-character hexadecimal digest")
        if (
            not math.isfinite(confidence_threshold)
            or not 0.0 <= confidence_threshold <= 1.0
        ):
            raise ValueError("confidence_threshold must be finite and in [0, 1]")
        if not isinstance(max_detections, int) or max_detections < 1 or max_detections > 1000:
            raise ValueError("max_detections must be an integer in [1, 1000]")
        if not isinstance(device, str) or not device.strip():
            raise ValueError("device must be a non-empty string")
        if self.checkpoint_path.suffix.lower() != ".pt":
            raise ModelBlockedError(
                "blocked_untrusted_artifact_type",
                "EventYOLOAdapter accepts only a hash-pinned .pt event checkpoint.",
            )
        self.confidence_threshold = float(confidence_threshold)
        self.max_detections = max_detections
        self.device = device.strip()
        self._model: Any | None = None
        self._verified_sha256: str | None = None

    @property
    def loaded(self) -> bool:
        return self._model is not None

    @property
    def verified_sha256(self) -> str | None:
        return self._verified_sha256

    def load(self) -> None:
        """Verify and lazily load only the hash-pinned event checkpoint."""

        if self._model is not None:
            return
        if not self.checkpoint_path.is_file():
            raise ModelBlockedError(
                "blocked_missing_file", f"Event checkpoint not found: {self.checkpoint_path}"
            )
        actual = sha256_file(self.checkpoint_path)
        if actual != self.trusted_sha256:
            raise ModelBlockedError(
                "blocked_integrity_mismatch",
                "Event checkpoint SHA-256 does not match the trusted manifest; it was not loaded.",
            )
        try:
            from ultralytics import YOLO
        except (ImportError, ModuleNotFoundError) as exc:
            raise ModelBlockedError(
                "blocked_missing_ultralytics",
                "Ultralytics is required to load the trusted event checkpoint.",
            ) from exc
        try:
            model = YOLO(str(self.checkpoint_path))
        except Exception as exc:
            raise ModelBlockedError(
                "blocked_model_load_failure",
                f"Ultralytics could not load the trusted event checkpoint: {exc}",
            ) from exc
        self._verified_sha256 = actual
        self._model = model

    def infer(
        self,
        projected_rgb: ArrayLike,
        *,
        node_id: str,
        timestamp: datetime | None = None,
        frame_id: str | None = None,
        source_encoding: str = PROXY_EVENT_ENCODING,
    ) -> tuple[SensorObservation, ...]:
        """Run inference and normalize boxes into shared observations.

        Bounding boxes are stored as ``[x1, y1, x2, y2]`` fractions in [0, 1].
        A proxy source is explicitly tagged as not equivalent to training
        preprocessing.  No claim of physical-sensor performance is made.
        """

        if not isinstance(node_id, str) or not node_id.strip():
            raise ValueError("node_id must be a non-empty string")
        if timestamp is None:
            timestamp = datetime.now(timezone.utc)
        if timestamp.tzinfo is None or timestamp.utcoffset() is None:
            raise ValueError("timestamp must be timezone-aware")
        if not isinstance(source_encoding, str) or not source_encoding.strip():
            raise ValueError("source_encoding must be a non-empty string")
        image = _model_input(projected_rgb)
        self.load()
        assert self._model is not None
        try:
            raw_results = self._model.predict(
                source=image,
                conf=self.confidence_threshold,
                max_det=self.max_detections,
                device=self.device,
                verbose=False,
            )
        except Exception as exc:
            raise ModelBlockedError(
                "blocked_inference_failure", f"Event-YOLO inference failed: {exc}"
            ) from exc

        height, width = image.shape[:2]
        candidates: list[tuple[float, int, list[float], str]] = []
        for result in raw_results or ():
            boxes = getattr(result, "boxes", None)
            if boxes is None:
                continue
            xyxy = _as_numpy(getattr(boxes, "xyxy", np.empty((0, 4)))).reshape(-1, 4)
            confidence = _as_numpy(getattr(boxes, "conf", np.empty(0))).reshape(-1)
            classes = _as_numpy(getattr(boxes, "cls", np.zeros(len(confidence)))).reshape(-1)
            count = min(len(xyxy), len(confidence), len(classes))
            names = getattr(result, "names", getattr(self._model, "names", {}))
            for index in range(count):
                score = float(confidence[index])
                if not math.isfinite(score) or score < self.confidence_threshold:
                    continue
                class_id = int(classes[index])
                if isinstance(names, dict):
                    class_name = str(names.get(class_id, class_id))
                elif isinstance(names, (list, tuple)) and 0 <= class_id < len(names):
                    class_name = str(names[class_id])
                else:
                    class_name = str(class_id)
                raw_box = xyxy[index].astype(float)
                normalized = [
                    float(np.clip(raw_box[0] / width, 0.0, 1.0)),
                    float(np.clip(raw_box[1] / height, 0.0, 1.0)),
                    float(np.clip(raw_box[2] / width, 0.0, 1.0)),
                    float(np.clip(raw_box[3] / height, 0.0, 1.0)),
                ]
                if normalized[2] < normalized[0] or normalized[3] < normalized[1]:
                    continue
                candidates.append((min(max(score, 0.0), 1.0), class_id, normalized, class_name))

        candidates.sort(key=lambda item: item[0], reverse=True)
        observations: list[SensorObservation] = []
        is_proxy = source_encoding == PROXY_EVENT_ENCODING
        for score, class_id, normalized, class_name in candidates[: self.max_detections]:
            measurements: dict[str, object] = {
                "bbox_xyxy_normalized": normalized,
                "class_id": class_id,
                "class_name": class_name,
                "frame_width": width,
                "frame_height": height,
                "source_encoding": source_encoding,
                "preprocessing_equivalent_to_training": False if is_proxy else "unverified",
                "detection_only": True,
            }
            if frame_id is not None:
                measurements["frame_id"] = str(frame_id)
            observations.append(
                SensorObservation(
                    observation_id=f"event-{uuid4()}",
                    timestamp=timestamp,
                    node_id=node_id,
                    modality=SensorModality.EVENT_CAMERA,
                    detection_probability=score,
                    confidence_level=score,
                    model_id=f"fred_event_yolo26s_p2:{self.trusted_sha256[:12].lower()}",
                    measurements=measurements,
                )
            )
        return tuple(observations)
