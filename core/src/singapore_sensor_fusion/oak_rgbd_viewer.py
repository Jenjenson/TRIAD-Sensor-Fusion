"""Live and offline visualization for the Unreal OAK RGB-D adapter.

Two bounded modes are provided:

``watch``
    Poll one Unreal sensor node for newly completed RGB/depth/metadata
    triplets, run the hash-pinned OAK RGB detector, and display annotated
    frames in an OpenCV window.  ``q`` or Escape closes the window.

``video``
    Run the detector over the newest complete capture session and atomically
    publish an annotated MP4 plus a JSON manifest.

The RGB model boxes and their confidence are always labelled ``OAK RGB
MODEL``.  Optional projected scenario boxes are labelled ``SIM TRUTH
(DEBUG ONLY)`` and are never included in detector counts.  Ranging prefers a
validated uint32-millimetre Unreal SceneDepth sidecar and falls back to the
normalized eight-bit preview.  The selected source and its quantization are
shown explicitly; neither source is presented as calibrated physical OAK
range.

Neither Ultralytics nor OpenCV is imported at module import time.  Model
deserialization remains lazy inside :class:`~.oak_rgbd.OAKRGBDAdapter` and is
permitted only after the caller supplies an exact checkpoint SHA-256.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import time
from typing import Any, Callable, Mapping, Protocol, Sequence
from uuid import uuid4

import numpy as np
from numpy.typing import NDArray

from .oak_rgbd import (
    DEFAULT_SIMULATION_MAX_RANGE_METERS,
    DEPTH_ENCODING,
    DEPTH_RAW_BYTE_ORDER,
    DEPTH_RAW_OBSERVATION_ENCODING,
    DEPTH_RAW_QUANTIZATION_METERS,
    DEPTH_RAW_SEMANTICS,
    DEPTH_RAW_UNITS,
    MAX_CLI_FRAMES,
    OAKRGBDAdapter,
    RANGE_MODE_PHYSICAL_OAK,
    RANGE_MODE_SIMULATION_ADAPTED,
    LoadedUnrealRGBDFrame,
    UnrealRGBDFramePaths,
    load_unreal_rgbd_frame,
    select_latest_unreal_rgbd_frames,
    write_json_atomic,
)


MAX_WATCH_SECONDS = 86_400.0
MAX_WATCH_OBSERVATIONS = 10_000
MAX_POLL_INTERVAL_SECONDS = 30.0
DEFAULT_WATCH_SECONDS = 300.0
DEFAULT_MAX_FRAME_AGE_SECONDS = 10.0
DETECTOR_COLOR_BGR = (255, 220, 0)
HOSTILE_TRUTH_COLOR_BGR = (20, 20, 255)
FRIENDLY_TRUTH_COLOR_BGR = (20, 220, 20)
STATUS_COLOR_BGR = (255, 255, 255)
QUANTIZATION_COLOR_BGR = (0, 190, 255)
DEPTH_PREVIEW_SEMANTICS = "normalized_scene_depth_png_preview"


class RGBDAdapterProtocol(Protocol):
    """Small adapter surface used by the viewer and its model-free tests."""

    verified_sha256: str | None

    def infer_unreal_frame(
        self,
        frame: LoadedUnrealRGBDFrame,
        *,
        include_simulation_iou_diagnostic: bool = False,
    ) -> Sequence[Any]: ...


class DrawingBackend(Protocol):
    """Rendering/window surface; the production implementation is OpenCV."""

    name: str

    def prepare(self, rgb: NDArray[np.uint8]) -> NDArray[np.uint8]: ...

    def resize(
        self, image_bgr: NDArray[np.uint8], width: int, height: int
    ) -> NDArray[np.uint8]: ...

    def rectangle(
        self,
        image_bgr: NDArray[np.uint8],
        bounds: tuple[int, int, int, int],
        color_bgr: tuple[int, int, int],
        *,
        thickness: int,
    ) -> None: ...

    def text(
        self,
        image_bgr: NDArray[np.uint8],
        value: str,
        origin: tuple[int, int],
        color_bgr: tuple[int, int, int],
        *,
        scale: float = 0.5,
    ) -> None: ...

    def show(self, window_name: str, image_bgr: NDArray[np.uint8]) -> None: ...

    def wait_key(self, delay_ms: int) -> int: ...

    def close_window(self, window_name: str) -> None: ...


class VideoEncoder(Protocol):
    name: str

    def write(self, frame_bgr: NDArray[np.uint8]) -> None: ...

    def close(self) -> None: ...


EncoderFactory = Callable[[Path, float, tuple[int, int]], VideoEncoder]
FrameLoader = Callable[[UnrealRGBDFramePaths], LoadedUnrealRGBDFrame]
FrameSelector = Callable[..., Sequence[UnrealRGBDFramePaths]]


class _OpenCVBackend:
    name = "opencv"

    def __init__(self) -> None:
        try:
            import cv2
        except (ImportError, ModuleNotFoundError) as exc:
            raise RuntimeError(
                "OpenCV is required for the OAK RGB-D viewer. Install opencv-python."
            ) from exc
        self.cv2 = cv2

    def prepare(self, rgb: NDArray[np.uint8]) -> NDArray[np.uint8]:
        return self.cv2.cvtColor(np.ascontiguousarray(rgb), self.cv2.COLOR_RGB2BGR)

    def resize(
        self, image_bgr: NDArray[np.uint8], width: int, height: int
    ) -> NDArray[np.uint8]:
        return self.cv2.resize(image_bgr, (width, height), interpolation=self.cv2.INTER_AREA)

    def rectangle(
        self,
        image_bgr: NDArray[np.uint8],
        bounds: tuple[int, int, int, int],
        color_bgr: tuple[int, int, int],
        *,
        thickness: int,
    ) -> None:
        self.cv2.rectangle(
            image_bgr,
            (bounds[0], bounds[1]),
            (bounds[2], bounds[3]),
            color_bgr,
            thickness,
            self.cv2.LINE_AA,
        )

    def text(
        self,
        image_bgr: NDArray[np.uint8],
        value: str,
        origin: tuple[int, int],
        color_bgr: tuple[int, int, int],
        *,
        scale: float = 0.5,
    ) -> None:
        font = self.cv2.FONT_HERSHEY_SIMPLEX
        thickness = 1
        (width, height), baseline = self.cv2.getTextSize(value, font, scale, thickness)
        x = max(0, min(origin[0], max(0, image_bgr.shape[1] - width - 1)))
        y = max(height + 2, min(origin[1], image_bgr.shape[0] - baseline - 1))
        self.cv2.rectangle(
            image_bgr,
            (max(0, x - 2), max(0, y - height - 3)),
            (min(image_bgr.shape[1] - 1, x + width + 2), min(image_bgr.shape[0] - 1, y + baseline + 2)),
            (0, 0, 0),
            -1,
        )
        self.cv2.putText(
            image_bgr,
            value,
            (x, y),
            font,
            scale,
            color_bgr,
            thickness,
            self.cv2.LINE_AA,
        )

    def show(self, window_name: str, image_bgr: NDArray[np.uint8]) -> None:
        self.cv2.imshow(window_name, image_bgr)

    def wait_key(self, delay_ms: int) -> int:
        return int(self.cv2.waitKey(delay_ms) & 0xFF)

    def close_window(self, window_name: str) -> None:
        try:
            self.cv2.destroyWindow(window_name)
        except self.cv2.error:
            # A window may never have been created if no fresh frame arrived.
            pass


class _OpenCVMP4Encoder:
    name = "opencv-mp4v"

    def __init__(self, path: Path, fps: float, size: tuple[int, int]) -> None:
        try:
            import cv2
        except (ImportError, ModuleNotFoundError) as exc:
            raise RuntimeError("OpenCV is required to encode the annotated MP4") from exc
        self.path = path
        self.writer = cv2.VideoWriter(
            str(path), cv2.VideoWriter_fourcc(*"mp4v"), fps, size
        )
        if not self.writer.isOpened():
            self.writer.release()
            raise RuntimeError(f"OpenCV could not open an MP4 writer for {path}")

    def write(self, frame_bgr: NDArray[np.uint8]) -> None:
        self.writer.write(np.ascontiguousarray(frame_bgr, dtype=np.uint8))

    def close(self) -> None:
        self.writer.release()
        if not self.path.is_file() or self.path.stat().st_size < 1:
            raise RuntimeError("OpenCV closed without producing a non-empty MP4")


@dataclass(frozen=True, slots=True)
class DetectorOverlay:
    bounds: tuple[int, int, int, int]
    class_name: str
    confidence: float
    depth_z_m: float | None
    slant_range_m: float | None
    quantization_step_m: float | None
    used_legacy_distance: bool
    depth_selected_encoding: str
    depth_source_semantics: str
    depth_source_units: str | None
    depth_source_byte_order: str | None
    depth_source_file: str | None
    depth_source_fallback_reason: str | None
    range_semantics: str
    legacy_depth_source_metadata_inferred: bool

    def manifest_record(self) -> dict[str, Any]:
        return {
            "detectorSource": "OAK RGB model",
            "bboxXyxyPixels": list(self.bounds),
            "className": self.class_name,
            "confidence": self.confidence,
            "depthZMeters": self.depth_z_m,
            "slantRangeMeters": self.slant_range_m,
            "depthQuantizationStepMeters": self.quantization_step_m,
            "legacyEstimatedDistanceFallbackUsed": self.used_legacy_distance,
            "depthSelectedEncoding": self.depth_selected_encoding,
            "depthSourceSemantics": self.depth_source_semantics,
            "depthSourceUnits": self.depth_source_units,
            "depthSourceByteOrder": self.depth_source_byte_order,
            "depthSourceFile": self.depth_source_file,
            "depthSourceFallbackReason": self.depth_source_fallback_reason,
            "depthSourceDisplay": _depth_source_display_name(self),
            "legacyDepthSourceMetadataInferred": self.legacy_depth_source_metadata_inferred,
            "rangeSemantics": self.range_semantics,
        }


@dataclass(frozen=True, slots=True)
class TruthOverlay:
    bounds: tuple[int, int, int, int]
    actor_name: str
    hostile: bool
    distance_m: float | None


@dataclass(frozen=True, slots=True)
class WatchConfig:
    frames_root: Path
    checkpoint_path: Path
    trusted_sha256: str
    node_id: str
    report_path: Path | None = None
    duration_seconds: float = DEFAULT_WATCH_SECONDS
    max_frames: int = 500
    max_observations: int = 5_000
    max_frame_age_seconds: float = DEFAULT_MAX_FRAME_AGE_SECONDS
    poll_interval_seconds: float = 0.25
    confidence_threshold: float = 0.25
    max_detections_per_frame: int = 10
    device: str = "cpu"
    max_range_meters: float = DEFAULT_SIMULATION_MAX_RANGE_METERS
    range_mode: str = RANGE_MODE_SIMULATION_ADAPTED
    show_simulation_truth: bool = False
    display_window: bool = True
    window_name: str = "Singapore OAK RGB-D Drone Detection"


@dataclass(frozen=True, slots=True)
class VideoConfig:
    frames_root: Path
    checkpoint_path: Path
    trusted_sha256: str
    output_path: Path
    node_id: str | None = None
    manifest_path: Path | None = None
    max_frames: int = 180
    session_gap_seconds: float = 30.0
    fps: float = 6.0
    confidence_threshold: float = 0.25
    max_detections_per_frame: int = 10
    device: str = "cpu"
    max_range_meters: float = DEFAULT_SIMULATION_MAX_RANGE_METERS
    range_mode: str = RANGE_MODE_SIMULATION_ADAPTED
    show_simulation_truth: bool = False


def _finite_number(value: object) -> float | None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    result = float(value)
    return result if math.isfinite(result) else None


def _optional_text(value: object) -> str | None:
    if not isinstance(value, str):
        return None
    result = value.strip()
    return result or None


def _depth_source_from_measurements(
    measurements: Mapping[str, Any], quantization_step_m: float | None
) -> tuple[str, str, str | None, str | None, str | None, str | None, str, bool]:
    """Return source-aware depth metadata with an explicit legacy fallback.

    Observations created before raw sidecars existed did not carry the source
    fields.  Those records retain their historical normalized-preview
    semantics.  Partial records that identify the raw SceneDepth semantics (or
    its millimetre storage contract) are recognized as raw rather than being
    mislabeled as eight-bit.
    """

    selected_encoding = _optional_text(measurements.get("depth_selected_encoding"))
    source_semantics = _optional_text(measurements.get("depth_source_semantics"))
    source_units = _optional_text(measurements.get("depth_source_units"))
    source_byte_order = _optional_text(measurements.get("depth_source_byte_order"))
    source_file = _optional_text(measurements.get("depth_source_file"))
    fallback_reason = _optional_text(measurements.get("depth_source_fallback_reason"))
    range_semantics = _optional_text(measurements.get("range_semantics"))
    source_metadata_inferred = selected_encoding is None

    raw_evidence = (
        source_semantics == DEPTH_RAW_SEMANTICS
        or (
            source_units is not None
            and source_units.casefold() in {"millimeter", "millimeters", "millimetre", "millimetres", "mm"}
            and source_byte_order is not None
            and source_byte_order.casefold() == DEPTH_RAW_BYTE_ORDER.casefold()
            and quantization_step_m is not None
            and math.isclose(
                quantization_step_m,
                DEPTH_RAW_QUANTIZATION_METERS,
                rel_tol=0.0,
                abs_tol=1e-12,
            )
        )
    )
    if selected_encoding is None:
        if raw_evidence:
            selected_encoding = DEPTH_RAW_OBSERVATION_ENCODING
        elif source_semantics in (None, DEPTH_PREVIEW_SEMANTICS):
            # A completely source-less observation predates raw sidecars and
            # therefore has the historical preview contract.
            selected_encoding = DEPTH_ENCODING
        else:
            # Do not relabel an unfamiliar future source as eight-bit merely
            # because it omitted the encoding field.
            selected_encoding = "unspecified_depth_encoding"

    raw_selected = (
        selected_encoding.casefold() == DEPTH_RAW_OBSERVATION_ENCODING.casefold()
        or source_semantics == DEPTH_RAW_SEMANTICS
    )
    preview_selected = selected_encoding.casefold() == DEPTH_ENCODING.casefold()
    if source_semantics is None:
        source_semantics = (
            DEPTH_RAW_SEMANTICS
            if raw_selected
            else DEPTH_PREVIEW_SEMANTICS
            if preview_selected
            else "unspecified_unreal_depth_source"
        )
    if source_units is None:
        source_units = DEPTH_RAW_UNITS if raw_selected else "meters" if preview_selected else None
    if source_byte_order is None and raw_selected:
        source_byte_order = DEPTH_RAW_BYTE_ORDER
    if range_semantics is None:
        # Preserve the value emitted by older viewer manifests when the whole
        # source contract is absent.  Current observations carry the adapter's
        # exact machine-readable range semantics.
        range_semantics = (
            f"{source_semantics}_estimate"
            if raw_selected or not source_metadata_inferred
            else "Unreal normalized depth-preview simulation estimate"
        )
    return (
        selected_encoding,
        source_semantics,
        source_units,
        source_byte_order,
        source_file,
        fallback_reason,
        range_semantics,
        source_metadata_inferred,
    )


def _validate_hash(value: str) -> str:
    digest = value.strip().upper() if isinstance(value, str) else ""
    if len(digest) != 64 or any(character not in "0123456789ABCDEF" for character in digest):
        raise ValueError("trusted_sha256 must be an explicit 64-character hexadecimal digest")
    return digest


def _validate_common(
    *,
    checkpoint_path: Path,
    trusted_sha256: str,
    confidence_threshold: float,
    max_detections_per_frame: int,
    max_range_meters: float,
    range_mode: str,
) -> None:
    if checkpoint_path.expanduser().suffix.casefold() != ".pt":
        raise ValueError("checkpoint_path must select the trusted OAK .pt file")
    _validate_hash(trusted_sha256)
    if not math.isfinite(confidence_threshold) or not 0.0 <= confidence_threshold <= 1.0:
        raise ValueError("confidence_threshold must be finite and in [0, 1]")
    if (
        isinstance(max_detections_per_frame, bool)
        or not isinstance(max_detections_per_frame, int)
        or not 1 <= max_detections_per_frame <= 1_000
    ):
        raise ValueError("max_detections_per_frame must be an integer in [1, 1000]")
    if not math.isfinite(max_range_meters) or max_range_meters <= 0.0:
        raise ValueError("max_range_meters must be finite and > 0")
    if range_mode not in (RANGE_MODE_SIMULATION_ADAPTED, RANGE_MODE_PHYSICAL_OAK):
        raise ValueError("range_mode is unsupported")


def _make_adapter(
    *,
    checkpoint_path: Path,
    trusted_sha256: str,
    confidence_threshold: float,
    max_detections_per_frame: int,
    device: str,
    max_range_meters: float,
    range_mode: str,
) -> OAKRGBDAdapter:
    return OAKRGBDAdapter(
        checkpoint_path,
        trusted_sha256=trusted_sha256,
        confidence_threshold=confidence_threshold,
        max_detections=max_detections_per_frame,
        device=device,
        max_range_meters=max_range_meters,
        range_mode=range_mode,
    )


def _measurements(observation: object) -> Mapping[str, Any]:
    if isinstance(observation, Mapping):
        candidate = observation.get("measurements", {})
    else:
        candidate = getattr(observation, "measurements", {})
    return candidate if isinstance(candidate, Mapping) else {}


def _confidence(observation: object) -> float:
    if isinstance(observation, Mapping):
        candidate = observation.get(
            "detection_probability", observation.get("confidence_level", 0.0)
        )
    else:
        candidate = getattr(
            observation,
            "detection_probability",
            getattr(observation, "confidence_level", 0.0),
        )
    value = _finite_number(candidate)
    return min(max(value or 0.0, 0.0), 1.0)


def _box_values(value: object) -> tuple[float, float, float, float] | None:
    if not isinstance(value, Sequence) or isinstance(value, (str, bytes)) or len(value) != 4:
        return None
    numbers = tuple(_finite_number(item) for item in value)
    if any(item is None for item in numbers):
        return None
    return tuple(float(item) for item in numbers)  # type: ignore[arg-type]


def _pixel_bounds(
    value: object,
    width: int,
    height: int,
    *,
    normalized: bool,
) -> tuple[int, int, int, int] | None:
    numbers = _box_values(value)
    if numbers is None:
        return None
    x1, y1, x2, y2 = numbers
    if normalized:
        x1, x2 = x1 * width, x2 * width
        y1, y2 = y1 * height, y2 * height
    bounds = (
        max(0, min(width - 1, int(round(x1)))),
        max(0, min(height - 1, int(round(y1)))),
        max(0, min(width - 1, int(round(x2)))),
        max(0, min(height - 1, int(round(y2)))),
    )
    return bounds if bounds[2] > bounds[0] and bounds[3] > bounds[1] else None


def extract_detector_overlays(
    observations: Sequence[object], width: int, height: int
) -> tuple[DetectorOverlay, ...]:
    """Convert actual adapter observations into bounded display overlays."""

    overlays: list[DetectorOverlay] = []
    for observation in observations:
        measurements = _measurements(observation)
        bounds = _pixel_bounds(
            measurements.get("bbox_xyxy_pixels"), width, height, normalized=False
        )
        if bounds is None:
            bounds = _pixel_bounds(
                measurements.get("bbox_xyxy_normalized"), width, height, normalized=True
            )
        if bounds is None:
            continue
        class_name = str(measurements.get("class_name", "drone"))
        depth_z = _finite_number(measurements.get("depth_z_m"))
        slant = _finite_number(measurements.get("slant_range_m"))
        legacy = False
        if slant is None:
            # Backward compatibility for observations produced before explicit
            # depth-Z and bbox-ray slant fields were added.
            slant = _finite_number(measurements.get("estimated_distance_meters"))
            legacy = slant is not None
        quantization = _finite_number(measurements.get("depth_quantization_step_meters"))
        (
            selected_encoding,
            source_semantics,
            source_units,
            source_byte_order,
            source_file,
            fallback_reason,
            range_semantics,
            source_metadata_inferred,
        ) = _depth_source_from_measurements(measurements, quantization)
        overlays.append(
            DetectorOverlay(
                bounds=bounds,
                class_name=class_name,
                confidence=_confidence(observation),
                depth_z_m=depth_z,
                slant_range_m=slant,
                quantization_step_m=quantization,
                used_legacy_distance=legacy,
                depth_selected_encoding=selected_encoding,
                depth_source_semantics=source_semantics,
                depth_source_units=source_units,
                depth_source_byte_order=source_byte_order,
                depth_source_file=source_file,
                depth_source_fallback_reason=fallback_reason,
                range_semantics=range_semantics,
                legacy_depth_source_metadata_inferred=source_metadata_inferred,
            )
        )
    return tuple(overlays)


def _truth_box(target: Mapping[str, Any], width: int, height: int) -> tuple[int, int, int, int] | None:
    for key in ("bboxXyxyPixels", "bbox_xyxy_pixels", "bbox_xyxy", "bbox"):
        value = target.get(key)
        if isinstance(value, Mapping):
            value = [
                value.get("left", value.get("x1")),
                value.get("top", value.get("y1")),
                value.get("right", value.get("x2")),
                value.get("bottom", value.get("y2")),
            ]
        result = _pixel_bounds(value, width, height, normalized=False)
        if result is not None:
            return result
    for key in ("bbox_xyxy_normalized", "bboxNormalized", "bbox_normalized"):
        result = _pixel_bounds(target.get(key), width, height, normalized=True)
        if result is not None:
            return result
    value = target.get("bboxPixels")
    if isinstance(value, Mapping):
        return _pixel_bounds(
            [
                value.get("left", value.get("x1")),
                value.get("top", value.get("y1")),
                value.get("right", value.get("x2")),
                value.get("bottom", value.get("y2")),
            ],
            width,
            height,
            normalized=False,
        )
    return None


def extract_simulation_truth_overlays(
    metadata: Mapping[str, Any], width: int, height: int
) -> tuple[TruthOverlay, ...]:
    """Extract debug-only projected truth without treating it as detection."""

    targets = metadata.get("targets", ())
    if not isinstance(targets, list):
        return ()
    overlays: list[TruthOverlay] = []
    for index, item in enumerate(targets):
        if not isinstance(item, Mapping):
            continue
        if item.get("inFront") is False or item.get("intersectsFrame") is False:
            continue
        bounds = _truth_box(item, width, height)
        if bounds is None:
            continue
        hostile_raw = item.get(
            "hostileScenarioTruth", item.get("hostile", item.get("isHostile", False))
        )
        affiliation = str(item.get("affiliation", "")).casefold()
        hostile = bool(hostile_raw) or affiliation in {"hostile", "enemy", "red"}
        actor = item.get("actorName", item.get("targetId", item.get("id", f"target-{index + 1}")))
        overlays.append(
            TruthOverlay(
                bounds=bounds,
                actor_name=str(actor),
                hostile=hostile,
                distance_m=_finite_number(
                    item.get("distanceMeters", item.get("distance_meters"))
                ),
            )
        )
    return tuple(overlays)


def _distance_text(value: float | None) -> str:
    if value is None:
        return "unavailable"
    if value >= 1_000.0:
        return f"{value / 1_000.0:.2f} km"
    if value >= 100.0:
        return f"{value:.0f} m"
    return f"{value:.1f} m"


def _quantization_text(value: float) -> str:
    if value < 0.1:
        millimeters = value * 1_000.0
        return f"{millimeters:.3g} mm"
    return _distance_text(value)


def _uses_raw_depth(overlay: DetectorOverlay) -> bool:
    return (
        overlay.depth_selected_encoding.casefold()
        == DEPTH_RAW_OBSERVATION_ENCODING.casefold()
        or overlay.depth_source_semantics == DEPTH_RAW_SEMANTICS
    )


def _uses_preview_depth(overlay: DetectorOverlay) -> bool:
    return (
        overlay.depth_selected_encoding.casefold() == DEPTH_ENCODING.casefold()
        or overlay.depth_source_semantics == DEPTH_PREVIEW_SEMANTICS
    )


def _depth_source_display_name(overlay: DetectorOverlay) -> str:
    if _uses_raw_depth(overlay):
        return "UINT32-MM SCENEDEPTH SIDECAR"
    if _uses_preview_depth(overlay):
        return "NORMALIZED 8-BIT DEPTH PREVIEW"
    compact = " ".join(overlay.depth_selected_encoding.split())
    if len(compact) > 48:
        compact = compact[:45] + "..."
    return f"DEPTH SOURCE {compact.upper()}"


def _detector_range_label(overlay: DetectorOverlay) -> str:
    pieces: list[str] = []
    if overlay.depth_z_m is not None:
        pieces.append(f"depth-Z {_distance_text(overlay.depth_z_m)}")
    if overlay.slant_range_m is not None:
        prefix = "legacy range" if overlay.used_legacy_distance else "slant"
        pieces.append(f"{prefix} {_distance_text(overlay.slant_range_m)}")
    if not pieces:
        pieces.append("range unavailable")
    if overlay.quantization_step_m is not None:
        if _uses_raw_depth(overlay):
            source = "uint32-mm"
            unit = "step"
        elif _uses_preview_depth(overlay):
            source = "8-bit"
            unit = "level"
        else:
            source = "depth"
            unit = "step"
        pieces.append(
            f"{source} q~{_quantization_text(overlay.quantization_step_m)}/{unit}"
        )
    return " | ".join(pieces)


def _frame_depth_display_name(
    frame: LoadedUnrealRGBDFrame, detections: Sequence[DetectorOverlay]
) -> str:
    labels = sorted({_depth_source_display_name(item) for item in detections})
    if labels:
        return labels[0] if len(labels) == 1 else "MIXED: " + " + ".join(labels)
    if frame.depth_raw_u32_mm is not None:
        return "UINT32-MM SCENEDEPTH SIDECAR"
    return "NORMALIZED 8-BIT DEPTH PREVIEW"


def annotate_oak_frame(
    frame: LoadedUnrealRGBDFrame,
    observations: Sequence[object],
    *,
    backend: DrawingBackend,
    show_simulation_truth: bool = False,
) -> tuple[NDArray[np.uint8], tuple[DetectorOverlay, ...], tuple[TruthOverlay, ...]]:
    """Render detector outputs and optional, clearly separate debug truth."""

    image = backend.prepare(frame.rgb)
    height, width = image.shape[:2]
    detections = extract_detector_overlays(observations, width, height)
    truth = (
        extract_simulation_truth_overlays(frame.metadata, width, height)
        if show_simulation_truth
        else ()
    )

    for item in truth:
        color = HOSTILE_TRUTH_COLOR_BGR if item.hostile else FRIENDLY_TRUTH_COLOR_BGR
        backend.rectangle(image, item.bounds, color, thickness=1)
        affiliation = "HOSTILE" if item.hostile else "FRIENDLY"
        backend.text(
            image,
            f"SIM TRUTH (DEBUG ONLY) | {affiliation} | {item.actor_name} | {_distance_text(item.distance_m)}",
            (item.bounds[0], max(14, item.bounds[1] - 5)),
            color,
            scale=0.42,
        )

    for item in detections:
        backend.rectangle(image, item.bounds, DETECTOR_COLOR_BGR, thickness=3)
        backend.text(
            image,
            f"OAK RGB MODEL | {item.class_name} | {item.confidence:.0%}",
            (item.bounds[0], max(16, item.bounds[1] - 22)),
            DETECTOR_COLOR_BGR,
            scale=0.5,
        )
        backend.text(
            image,
            _detector_range_label(item),
            (item.bounds[0], min(height - 4, item.bounds[3] + 16)),
            QUANTIZATION_COLOR_BGR,
            scale=0.43,
        )

    timestamp = frame.paths.timestamp.isoformat().replace("+00:00", "Z")
    backend.text(
        image,
        f"OAK RGB-D AUTO DETECTION | NODE {frame.paths.node_id} | MODEL BOXES {len(detections)}",
        (8, 20),
        STATUS_COLOR_BGR,
        scale=0.5,
    )
    backend.text(
        image,
        f"RANGE: UNREAL {_frame_depth_display_name(frame, detections)} | SIMULATION ESTIMATE, NOT CALIBRATED OAK RANGE",
        (8, height - 26),
        QUANTIZATION_COLOR_BGR,
        scale=0.4,
    )
    backend.text(
        image,
        f"{timestamp} | DETECTION ONLY | q/Esc exits live view",
        (8, height - 7),
        STATUS_COLOR_BGR,
        scale=0.4,
    )
    return image, detections, truth


def _observation_record(observation: object) -> dict[str, Any]:
    if hasattr(observation, "to_dict"):
        value = observation.to_dict()
        if isinstance(value, dict):
            return value
    if isinstance(observation, Mapping):
        return dict(observation)
    measurements = _measurements(observation)
    return {
        "detection_probability": _confidence(observation),
        "measurements": dict(measurements),
    }


def _frame_manifest_record(
    frame: LoadedUnrealRGBDFrame,
    detections: Sequence[DetectorOverlay],
    truth_count: int,
) -> dict[str, Any]:
    detection_sources = [item.manifest_record() for item in detections]
    if detections:
        selected_encodings = sorted({item.depth_selected_encoding for item in detections})
        source_semantics_values = sorted({item.depth_source_semantics for item in detections})
        source_units_values = sorted(
            {item.depth_source_units for item in detections if item.depth_source_units is not None}
        )
        source_byte_orders = sorted(
            {item.depth_source_byte_order for item in detections if item.depth_source_byte_order is not None}
        )
        source_files = sorted(
            {item.depth_source_file for item in detections if item.depth_source_file is not None}
        )
        quantization_values = sorted(
            {item.quantization_step_m for item in detections if item.quantization_step_m is not None}
        )
        selected_encoding = selected_encodings[0] if len(selected_encodings) == 1 else None
        source_semantics = (
            source_semantics_values[0] if len(source_semantics_values) == 1 else None
        )
        source_units = source_units_values[0] if len(source_units_values) == 1 else None
        source_byte_order = source_byte_orders[0] if len(source_byte_orders) == 1 else None
        selected_source_path = source_files[0] if len(source_files) == 1 else None
        quantization = quantization_values[0] if len(quantization_values) == 1 else None
        metadata_origin = "detector_observations"
        fallback_reasons = sorted(
            {
                item.depth_source_fallback_reason
                for item in detections
                if item.depth_source_fallback_reason is not None
            }
        )
        fallback_reason = fallback_reasons[0] if len(fallback_reasons) == 1 else None
    else:
        raw_selected = frame.depth_raw_u32_mm is not None
        selected_encoding = DEPTH_RAW_OBSERVATION_ENCODING if raw_selected else DEPTH_ENCODING
        selected_encodings = [selected_encoding]
        source_semantics = DEPTH_RAW_SEMANTICS if raw_selected else DEPTH_PREVIEW_SEMANTICS
        source_semantics_values = [source_semantics]
        source_units = DEPTH_RAW_UNITS if raw_selected else "meters"
        source_units_values = [source_units]
        source_byte_order = DEPTH_RAW_BYTE_ORDER if raw_selected else None
        source_byte_orders = [source_byte_order] if source_byte_order is not None else []
        selected_source_path = str(
            frame.depth_source_path
            or (frame.paths.raw_depth_path if raw_selected else frame.paths.depth_path)
        )
        source_files = [selected_source_path]
        normalization = _finite_number(frame.metadata.get("normalizationMaxMeters"))
        quantization = (
            DEPTH_RAW_QUANTIZATION_METERS
            if raw_selected
            else normalization / 255.0
            if normalization is not None and normalization > 0.0
            else None
        )
        quantization_values = [quantization] if quantization is not None else []
        metadata_origin = "loaded_frame"
        fallback_reason = frame.depth_fallback_reason
    return {
        "nodeId": frame.paths.node_id,
        "frameId": frame.paths.frame_id,
        "frameIndex": frame.paths.frame_index,
        "timestampUtc": frame.paths.timestamp.isoformat().replace("+00:00", "Z"),
        "rgbPath": str(frame.paths.rgb_path),
        # depthPath remains the preview path for backward compatibility.
        "depthPath": str(frame.paths.depth_path),
        "depthPreviewPath": str(frame.paths.depth_path),
        "depthRawPath": (
            str(frame.paths.raw_depth_path) if frame.paths.raw_depth_path is not None else None
        ),
        "depthSelectedSourcePath": selected_source_path,
        "depthSelectedEncoding": selected_encoding,
        "depthSelectedEncodings": selected_encodings,
        "depthSourceSemantics": source_semantics,
        "depthSourceSemanticsValues": source_semantics_values,
        "depthSourceUnits": source_units,
        "depthSourceUnitsValues": source_units_values,
        "depthSourceByteOrder": source_byte_order,
        "depthSourceByteOrderValues": source_byte_orders,
        "depthSourceFiles": source_files,
        "depthQuantizationStepMeters": quantization,
        "depthQuantizationStepValuesMeters": quantization_values,
        "depthSourceFallbackReason": fallback_reason,
        "depthSourceMetadataOrigin": metadata_origin,
        "metadataPath": str(frame.paths.metadata_path),
        "oakModelDetectionCount": len(detections),
        "simulationTruthDebugBoxCount": truth_count,
        "detections": detection_sources,
    }


def _semantics(show_simulation_truth: bool) -> dict[str, Any]:
    return {
        "detectionOnly": True,
        "detectorBoxes": "OAK RGB MODEL",
        "depthRole": "post-detection ranging; depth is not a fourth model input channel",
        "rangeFields": {
            "depth_z_m": "optical-axis depth from the detection ROI",
            "slant_range_m": "bbox-center camera-ray range derived from depth-Z and horizontal FOV",
            "estimated_distance_meters": "legacy fallback accepted only for old observations",
        },
        "depthSources": {
            "preferred": "validated little-endian uint32-millimetre Unreal SceneDepth sidecar",
            "fallback": "normalized uint8 Unreal SceneDepth preview",
            "selectedSourceRecordedPerFrameAndDetection": True,
        },
        "depthCaveat": "Both depth sources are Unreal simulation SceneDepth, not calibrated physical OAK measurements. Invalid and saturated values are excluded; the selected source and quantization are recorded per frame and detection.",
        "physicalOakAccuracyClaimed": False,
        "simulationTruthDebugEnabled": show_simulation_truth,
        "simulationTruthLabel": "SIM TRUTH (DEBUG ONLY)",
        "simulationTruthIsDetectorOutput": False,
        "engagementOrEffectorActions": "none",
    }


def _verified_digest(adapter: RGBDAdapterProtocol, _declared: str) -> str | None:
    value = getattr(adapter, "verified_sha256", None)
    if isinstance(value, str) and value:
        return value
    # A fake adapter or an adapter that produced no candidates may expose no
    # property.  Do not call the declared digest verified in that case.
    return None


def _watch_report(
    *,
    config: WatchConfig,
    adapter: RGBDAdapterProtocol,
    started_utc: datetime,
    stop_reason: str,
    processed: Sequence[dict[str, Any]],
    observations: Sequence[dict[str, Any]],
    stale_skipped: int,
) -> dict[str, Any]:
    running = stop_reason == "running"
    updated_at = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
    return {
        "schemaVersion": "1.0",
        "mode": "bounded_live_unreal_oak_rgbd_watch",
        "startedAtUtc": started_utc.isoformat().replace("+00:00", "Z"),
        "updatedAtUtc": updated_at,
        "finishedAtUtc": None if running else updated_at,
        "reportState": "running" if running else "complete",
        "stopReason": stop_reason,
        "inputs": {
            "framesRoot": str(config.frames_root.expanduser().resolve()),
            "nodeId": config.node_id,
            "checkpointPath": str(config.checkpoint_path.expanduser().resolve()),
            "declaredTrustedSha256": _validate_hash(config.trusted_sha256),
            "verifiedCheckpointSha256": _verified_digest(adapter, config.trusted_sha256),
        },
        "limits": {
            "durationSeconds": config.duration_seconds,
            "maxFrames": config.max_frames,
            "maxObservations": config.max_observations,
            "maxFrameAgeSeconds": config.max_frame_age_seconds,
        },
        "summary": {
            "processedFrameCount": len(processed),
            "oakModelDetectionCount": len(observations),
            "staleFrameCountSkipped": stale_skipped,
        },
        "processedFrames": list(processed),
        "observations": list(observations),
        "semantics": _semantics(config.show_simulation_truth),
    }


def watch_unreal_oak_rgbd(
    config: WatchConfig,
    *,
    adapter: RGBDAdapterProtocol | None = None,
    backend: DrawingBackend | None = None,
    frame_loader: FrameLoader = load_unreal_rgbd_frame,
    frame_selector: FrameSelector = select_latest_unreal_rgbd_frames,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
    now_utc: Callable[[], datetime] = lambda: datetime.now(timezone.utc),
) -> dict[str, Any]:
    """Watch a node for fresh complete frames until a hard bound or user exit.

    Files older than ``max_frame_age_seconds`` and paths already observed in
    this invocation are skipped before model inference.  ``display_window``
    may be disabled for servers and tests; all detection/ranging still runs.
    """

    _validate_common(
        checkpoint_path=config.checkpoint_path,
        trusted_sha256=config.trusted_sha256,
        confidence_threshold=config.confidence_threshold,
        max_detections_per_frame=config.max_detections_per_frame,
        max_range_meters=config.max_range_meters,
        range_mode=config.range_mode,
    )
    if not isinstance(config.node_id, str) or not config.node_id.strip():
        raise ValueError("node_id must be a non-empty string in watch mode")
    if not math.isfinite(config.duration_seconds) or not 0.0 < config.duration_seconds <= MAX_WATCH_SECONDS:
        raise ValueError(f"duration_seconds must be in (0, {MAX_WATCH_SECONDS:g}]")
    if isinstance(config.max_frames, bool) or not isinstance(config.max_frames, int) or not 1 <= config.max_frames <= MAX_CLI_FRAMES:
        raise ValueError(f"max_frames must be an integer in [1, {MAX_CLI_FRAMES}]")
    if isinstance(config.max_observations, bool) or not isinstance(config.max_observations, int) or not 1 <= config.max_observations <= MAX_WATCH_OBSERVATIONS:
        raise ValueError(f"max_observations must be an integer in [1, {MAX_WATCH_OBSERVATIONS}]")
    if not math.isfinite(config.max_frame_age_seconds) or config.max_frame_age_seconds <= 0.0:
        raise ValueError("max_frame_age_seconds must be finite and > 0")
    if not math.isfinite(config.poll_interval_seconds) or not 0.01 <= config.poll_interval_seconds <= MAX_POLL_INTERVAL_SECONDS:
        raise ValueError(f"poll_interval_seconds must be in [0.01, {MAX_POLL_INTERVAL_SECONDS:g}]")
    root = config.frames_root.expanduser().resolve()
    if not root.is_dir():
        raise FileNotFoundError(f"frames root was not found: {root}")

    active_adapter = adapter or _make_adapter(
        checkpoint_path=config.checkpoint_path,
        trusted_sha256=config.trusted_sha256,
        confidence_threshold=config.confidence_threshold,
        max_detections_per_frame=config.max_detections_per_frame,
        device=config.device,
        max_range_meters=config.max_range_meters,
        range_mode=config.range_mode,
    )
    active_backend = backend or _OpenCVBackend()
    started_utc = now_utc().astimezone(timezone.utc)
    started_clock = monotonic()
    seen: set[str] = set()
    failed_attempts: dict[str, int] = {}
    processed: list[dict[str, Any]] = []
    observation_records: list[dict[str, Any]] = []
    stale_skipped = 0
    stop_reason = "duration_limit"

    try:
        while monotonic() - started_clock < config.duration_seconds:
            try:
                selected = frame_selector(
                    root,
                    node_id=config.node_id.strip(),
                    max_frames=MAX_CLI_FRAMES,
                )
            except FileNotFoundError:
                selected = ()
            for paths in selected:
                key = str(paths.metadata_path)
                if key in seen:
                    continue
                age = (now_utc().astimezone(timezone.utc) - paths.timestamp).total_seconds()
                if age > config.max_frame_age_seconds:
                    seen.add(key)
                    stale_skipped += 1
                    continue
                try:
                    frame = frame_loader(paths)
                except (OSError, ValueError, RuntimeError):
                    failed_attempts[key] = failed_attempts.get(key, 0) + 1
                    if failed_attempts[key] >= 3:
                        seen.add(key)
                    continue
                seen.add(key)
                detected = tuple(
                    active_adapter.infer_unreal_frame(
                        frame,
                        include_simulation_iou_diagnostic=False,
                    )
                )
                remaining = config.max_observations - len(observation_records)
                kept = detected[:remaining]
                annotated, overlays, truth = annotate_oak_frame(
                    frame,
                    kept,
                    backend=active_backend,
                    show_simulation_truth=config.show_simulation_truth,
                )
                processed.append(_frame_manifest_record(frame, overlays, len(truth)))
                observation_records.extend(_observation_record(item) for item in kept)
                # Publish the bounded cumulative report immediately after each
                # complete inference result.  Atomic replacement means the C3
                # bridge can never observe a partially written detection list.
                if config.report_path is not None:
                    write_json_atomic(
                        _watch_report(
                            config=config,
                            adapter=active_adapter,
                            started_utc=started_utc,
                            stop_reason="running",
                            processed=processed,
                            observations=observation_records,
                            stale_skipped=stale_skipped,
                        ),
                        config.report_path,
                    )
                if config.display_window:
                    active_backend.show(config.window_name, annotated)
                    key_code = active_backend.wait_key(1)
                    if key_code in (27, ord("q"), ord("Q")):
                        stop_reason = "user_exit"
                        break
                if len(processed) >= config.max_frames:
                    stop_reason = "frame_limit"
                    break
                if len(observation_records) >= config.max_observations:
                    stop_reason = "observation_limit"
                    break
            if stop_reason != "duration_limit":
                break
            if monotonic() - started_clock >= config.duration_seconds:
                break
            # Even when a frame was processed, polling immediately can spin on
            # the same directory; use the configured bounded cadence.
            sleep(config.poll_interval_seconds)
        report = _watch_report(
            config=config,
            adapter=active_adapter,
            started_utc=started_utc,
            stop_reason=stop_reason,
            processed=processed,
            observations=observation_records,
            stale_skipped=stale_skipped,
        )
        if config.report_path is not None:
            write_json_atomic(report, config.report_path)
        return report
    finally:
        if config.display_window:
            active_backend.close_window(config.window_name)


def _video_manifest(
    *,
    config: VideoConfig,
    adapter: RGBDAdapterProtocol,
    encoder_name: str,
    backend_name: str,
    frame_records: Sequence[dict[str, Any]],
    output: Path,
) -> dict[str, Any]:
    detections = sum(int(item["oakModelDetectionCount"]) for item in frame_records)
    truth = sum(int(item["simulationTruthDebugBoxCount"]) for item in frame_records)
    ranged = sum(
        1
        for frame in frame_records
        for item in frame["detections"]
        if item["depthZMeters"] is not None or item["slantRangeMeters"] is not None
    )
    return {
        "schemaVersion": "1.0",
        "mode": "offline_latest_unreal_oak_rgbd_annotated_video",
        "createdAtUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "outputPath": str(output),
        "inputs": {
            "framesRoot": str(config.frames_root.expanduser().resolve()),
            "nodeId": config.node_id,
            "checkpointPath": str(config.checkpoint_path.expanduser().resolve()),
            "declaredTrustedSha256": _validate_hash(config.trusted_sha256),
            "verifiedCheckpointSha256": _verified_digest(adapter, config.trusted_sha256),
        },
        "video": {
            "encoder": encoder_name,
            "drawingBackend": backend_name,
            "fps": config.fps,
            "frameCount": len(frame_records),
            "durationSeconds": len(frame_records) / config.fps,
        },
        "counts": {
            "oakModelDetections": detections,
            "detectionsWithSimulationDepthRange": ranged,
            "simulationTruthDebugBoxes": truth,
        },
        "frames": list(frame_records),
        "semantics": _semantics(config.show_simulation_truth),
    }


def render_latest_oak_rgbd_video(
    config: VideoConfig,
    *,
    adapter: RGBDAdapterProtocol | None = None,
    backend: DrawingBackend | None = None,
    encoder_factory: EncoderFactory | None = None,
    frame_loader: FrameLoader = load_unreal_rgbd_frame,
    frame_selector: FrameSelector = select_latest_unreal_rgbd_frames,
) -> dict[str, Any]:
    """Atomically render an annotated MP4 and latest-session manifest."""

    _validate_common(
        checkpoint_path=config.checkpoint_path,
        trusted_sha256=config.trusted_sha256,
        confidence_threshold=config.confidence_threshold,
        max_detections_per_frame=config.max_detections_per_frame,
        max_range_meters=config.max_range_meters,
        range_mode=config.range_mode,
    )
    if isinstance(config.max_frames, bool) or not isinstance(config.max_frames, int) or not 1 <= config.max_frames <= MAX_CLI_FRAMES:
        raise ValueError(f"max_frames must be an integer in [1, {MAX_CLI_FRAMES}]")
    if not math.isfinite(config.fps) or config.fps <= 0.0:
        raise ValueError("fps must be finite and > 0")
    if not math.isfinite(config.session_gap_seconds) or config.session_gap_seconds <= 0.0:
        raise ValueError("session_gap_seconds must be finite and > 0")
    output = config.output_path.expanduser().resolve()
    if output.suffix.casefold() != ".mp4":
        raise ValueError("output_path must use an .mp4 suffix")
    manifest_path = (
        config.manifest_path.expanduser().resolve()
        if config.manifest_path is not None
        else output.with_suffix(".manifest.json")
    )
    if output == manifest_path:
        raise ValueError("manifest_path must differ from output_path")

    frames = tuple(
        frame_selector(
            config.frames_root.expanduser().resolve(),
            node_id=config.node_id,
            max_frames=config.max_frames,
            session_gap_seconds=config.session_gap_seconds,
        )
    )
    if not frames:
        raise FileNotFoundError("no complete latest-session RGB-D frames were selected")
    active_adapter = adapter or _make_adapter(
        checkpoint_path=config.checkpoint_path,
        trusted_sha256=config.trusted_sha256,
        confidence_threshold=config.confidence_threshold,
        max_detections_per_frame=config.max_detections_per_frame,
        device=config.device,
        max_range_meters=config.max_range_meters,
        range_mode=config.range_mode,
    )
    active_backend = backend or _OpenCVBackend()
    factory = encoder_factory or (
        lambda path, fps, size: _OpenCVMP4Encoder(path, fps, size)
    )

    output.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    token = uuid4().hex
    temporary_video = output.with_name(f".{output.stem}.{token}.tmp.mp4")
    temporary_manifest = manifest_path.with_name(f".{manifest_path.name}.{token}.tmp")
    encoder: VideoEncoder | None = None
    closed = False
    frame_records: list[dict[str, Any]] = []
    target_width: int | None = None
    target_height: int | None = None
    try:
        for paths in frames:
            frame = frame_loader(paths)
            observations = tuple(
                active_adapter.infer_unreal_frame(
                    frame,
                    include_simulation_iou_diagnostic=False,
                )
            )[: config.max_detections_per_frame]
            annotated, overlays, truth = annotate_oak_frame(
                frame,
                observations,
                backend=active_backend,
                show_simulation_truth=config.show_simulation_truth,
            )
            if encoder is None:
                target_height, target_width = annotated.shape[:2]
                # Common MP4 codecs require even dimensions.
                target_width -= target_width % 2
                target_height -= target_height % 2
                if target_width < 2 or target_height < 2:
                    raise ValueError("annotated frame is too small to encode")
                encoder = factory(
                    temporary_video, config.fps, (target_width, target_height)
                )
            assert target_width is not None and target_height is not None
            if annotated.shape[:2] != (target_height, target_width):
                annotated = active_backend.resize(annotated, target_width, target_height)
            encoder.write(np.ascontiguousarray(annotated, dtype=np.uint8))
            frame_records.append(_frame_manifest_record(frame, overlays, len(truth)))
        assert encoder is not None
        encoder.close()
        closed = True
        manifest = _video_manifest(
            config=config,
            adapter=active_adapter,
            encoder_name=encoder.name,
            backend_name=active_backend.name,
            frame_records=frame_records,
            output=output,
        )
        with temporary_manifest.open("w", encoding="utf-8") as handle:
            json.dump(manifest, handle, indent=2, sort_keys=True, allow_nan=False)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary_video, output)
        os.replace(temporary_manifest, manifest_path)
        return manifest
    except BaseException:
        if encoder is not None and not closed:
            try:
                encoder.close()
            except Exception:
                pass
        temporary_video.unlink(missing_ok=True)
        temporary_manifest.unlink(missing_ok=True)
        raise


def _add_model_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--frames-root", type=Path, required=True)
    parser.add_argument("--checkpoint", dest="checkpoint_path", type=Path, required=True)
    parser.add_argument(
        "--trusted-sha256",
        required=True,
        help="Exact SHA-256 for the selected .pt checkpoint; required before loading.",
    )
    parser.add_argument("--node", dest="node_id")
    parser.add_argument("--confidence", dest="confidence_threshold", type=float, default=0.25)
    parser.add_argument("--max-detections", dest="max_detections_per_frame", type=int, default=10)
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--max-range-meters", type=float, default=DEFAULT_SIMULATION_MAX_RANGE_METERS)
    parser.add_argument(
        "--range-mode",
        choices=(RANGE_MODE_SIMULATION_ADAPTED, RANGE_MODE_PHYSICAL_OAK),
        default=RANGE_MODE_SIMULATION_ADAPTED,
    )
    parser.add_argument(
        "--show-sim-truth",
        action="store_true",
        help="Draw debug-only projected scenario boxes, explicitly labelled SIM TRUTH.",
    )


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="View live or render offline OAK RGB-D drone detections from Unreal."
    )
    subparsers = parser.add_subparsers(dest="mode", required=True)
    watch = subparsers.add_parser("watch", help="Watch fresh frames from one Unreal node.")
    _add_model_arguments(watch)
    watch.add_argument("--report", dest="report_path", type=Path)
    watch.add_argument("--duration-seconds", type=float, default=DEFAULT_WATCH_SECONDS)
    watch.add_argument("--max-frames", type=int, default=500)
    watch.add_argument("--max-observations", type=int, default=5_000)
    watch.add_argument("--max-frame-age-seconds", type=float, default=DEFAULT_MAX_FRAME_AGE_SECONDS)
    watch.add_argument("--poll-interval-seconds", type=float, default=0.25)
    watch.add_argument("--headless", action="store_true", help="Run inference without opening a window.")

    video = subparsers.add_parser("video", help="Render the newest complete session to MP4.")
    _add_model_arguments(video)
    video.add_argument("--output", dest="output_path", type=Path, required=True)
    video.add_argument("--manifest", dest="manifest_path", type=Path)
    video.add_argument("--max-frames", type=int, default=180)
    video.add_argument("--session-gap-seconds", type=float, default=30.0)
    video.add_argument("--fps", type=float, default=6.0)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    common = {
        "frames_root": args.frames_root,
        "checkpoint_path": args.checkpoint_path,
        "trusted_sha256": args.trusted_sha256,
        "node_id": args.node_id,
        "confidence_threshold": args.confidence_threshold,
        "max_detections_per_frame": args.max_detections_per_frame,
        "device": args.device,
        "max_range_meters": args.max_range_meters,
        "range_mode": args.range_mode,
        "show_simulation_truth": args.show_sim_truth,
    }
    if args.mode == "watch":
        if not args.node_id:
            raise SystemExit("watch mode requires --node")
        report = watch_unreal_oak_rgbd(
            WatchConfig(
                **common,
                report_path=args.report_path,
                duration_seconds=args.duration_seconds,
                max_frames=args.max_frames,
                max_observations=args.max_observations,
                max_frame_age_seconds=args.max_frame_age_seconds,
                poll_interval_seconds=args.poll_interval_seconds,
                display_window=not args.headless,
            )
        )
        print(json.dumps({"mode": "watch", **report["summary"], "stopReason": report["stopReason"]}))
    else:
        manifest = render_latest_oak_rgbd_video(
            VideoConfig(
                **common,
                output_path=args.output_path,
                manifest_path=args.manifest_path,
                max_frames=args.max_frames,
                session_gap_seconds=args.session_gap_seconds,
                fps=args.fps,
            )
        )
        print(
            json.dumps(
                {
                    "mode": "video",
                    "output": str(args.output_path.expanduser().resolve()),
                    **manifest["counts"],
                }
            )
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
