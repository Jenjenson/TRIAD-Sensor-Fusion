"""OAK-style RGB detection with post-detection Unreal depth ranging.

This adapter follows the supplied OAK pipeline contract: an Ultralytics YOLO
checkpoint receives a three-channel RGB-camera image, while depth is sampled
*after* a two-dimensional detection.  Depth is never appended as a fourth
model channel.  New Unreal captures include a lossless little-endian uint32
millimetre SceneDepth sidecar; the normalized 8-bit PNG remains a diagnostic
fallback.  Both sources exclude invalid/saturated pixels and are labelled as
simulation estimates rather than physical-camera measurements.

Example::

    python -m singapore_sensor_fusion.oak_rgbd \
        --frames-root <TRIAD_PROJECT>/Saved/SingaporeSensorFusion/frames \
        --checkpoint D:/models/best.pt --trusted-sha256 <64 hex chars> \
        --output rgbd_observations.json --max-frames 24

Only an explicitly selected ``.pt`` file whose SHA-256 equals
``--trusted-sha256`` is passed to Ultralytics.  Loading is lazy and occurs on
the first inference call.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import tempfile
from typing import Any, Mapping, Sequence
from uuid import uuid4

import numpy as np
from numpy.typing import ArrayLike, NDArray

from .model_doctor import sha256_file
from .observations import SensorModality, SensorObservation


MAX_FRAME_CANDIDATES = 10_000
MAX_CLI_FRAMES = 500
MAX_CLI_OBSERVATIONS = 10_000
MAX_METADATA_BYTES = 4 * 1024 * 1024
MAX_IMAGE_PIXELS = 32_000_000
DEFAULT_SIMULATION_MAX_RANGE_METERS = 10_000.0
DEPTH_ENCODING = "gray/255 * normalizationMaxMeters"
DEPTH_RAW_ENCODING = "uint32_millimeters"
DEPTH_RAW_OBSERVATION_ENCODING = "uint32_little_endian_millimeters"
DEPTH_RAW_BYTE_ORDER = "little-endian"
DEPTH_RAW_UNITS = "millimeters"
DEPTH_RAW_QUANTIZATION_METERS = 0.001
DEPTH_RAW_INVALID_VALUE = 0
DEPTH_RAW_SATURATED_VALUE = 0xFFFFFFFF
DEPTH_RAW_MAXIMUM_METERS = (DEPTH_RAW_SATURATED_VALUE - 1) * DEPTH_RAW_QUANTIZATION_METERS
DEPTH_RAW_MAXIMUM_VALID_METERS = DEPTH_RAW_MAXIMUM_METERS
DEPTH_RAW_SEMANTICS = "simulation_scene_depth_z_not_physical_oak_accuracy"
PHYSICAL_OAK_MIN_RANGE_METERS = 0.2
PHYSICAL_OAK_MAX_RANGE_METERS = 30.0
RANGE_MODE_PHYSICAL_OAK = "physical_oak_contract_0_2_to_30m"
RANGE_MODE_SIMULATION_ADAPTED = "simulation_depth_preview_adapted"


class RGBDModelBlockedError(RuntimeError):
    """The selected RGB detector cannot be loaded or executed safely."""

    def __init__(self, code: str, message: str) -> None:
        super().__init__(message)
        self.code = code


def _finite_positive(name: str, value: float, *, maximum: float | None = None) -> float:
    result = float(value)
    if not math.isfinite(result) or result <= 0.0:
        raise ValueError(f"{name} must be finite and > 0")
    if maximum is not None and result > maximum:
        raise ValueError(f"{name} must be <= {maximum:g}")
    return result


def _parse_timestamp(value: object) -> datetime:
    if not isinstance(value, str) or not value.strip():
        raise ValueError("timestampUtc must be a non-empty ISO-8601 string")
    normalized = value.strip()
    if normalized.endswith("Z"):
        normalized = normalized[:-1] + "+00:00"
    try:
        parsed = datetime.fromisoformat(normalized)
    except ValueError as exc:
        raise ValueError("timestampUtc must be valid ISO-8601") from exc
    if parsed.tzinfo is None or parsed.utcoffset() is None:
        raise ValueError("timestampUtc must include a timezone")
    return parsed.astimezone(timezone.utc)


def _json_object(path: Path) -> dict[str, Any]:
    size = path.stat().st_size
    if size > MAX_METADATA_BYTES:
        raise ValueError(f"metadata exceeds {MAX_METADATA_BYTES} bytes: {path}")
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"metadata root must be an object: {path}")
    return value


@dataclass(frozen=True, slots=True)
class UnrealRGBDFramePaths:
    """One complete Unreal RGB/depth/metadata triplet."""

    rgb_path: Path
    depth_path: Path
    metadata_path: Path
    timestamp: datetime
    node_id: str
    frame_index: int
    frame_id: str
    raw_depth_path: Path | None = None


@dataclass(frozen=True, slots=True)
class LoadedUnrealRGBDFrame:
    """Decoded frame data with a required preview and optional validated raw depth."""

    paths: UnrealRGBDFramePaths
    rgb: NDArray[np.uint8]
    depth_preview_gray: NDArray[np.uint8]
    metadata: Mapping[str, Any]
    depth_raw_u32_mm: NDArray[np.uint32] | None = None
    depth_source_path: Path | None = None
    depth_fallback_reason: str | None = None


def _frame_paths(rgb_path: Path, fallback_node: str) -> UnrealRGBDFramePaths | None:
    suffix = "_rgb.png"
    if not rgb_path.name.lower().endswith(suffix):
        return None
    stem = rgb_path.name[: -len(suffix)]
    depth_path = rgb_path.with_name(stem + "_depth.png")
    metadata_path = rgb_path.with_name(stem + "_depth.json")
    if not depth_path.is_file() or not metadata_path.is_file():
        return None
    try:
        metadata = _json_object(metadata_path)
        timestamp = _parse_timestamp(metadata.get("timestampUtc", metadata.get("timestamp")))
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError):
        return None
    raw_node = metadata.get("nodeId", fallback_node)
    node_id = raw_node.strip() if isinstance(raw_node, str) and raw_node.strip() else fallback_node
    raw_index = metadata.get("frameIndex", -1)
    if isinstance(raw_index, bool) or not isinstance(raw_index, (int, float)):
        return None
    frame_index = int(raw_index)
    return UnrealRGBDFramePaths(
        rgb_path=rgb_path.resolve(),
        depth_path=depth_path.resolve(),
        metadata_path=metadata_path.resolve(),
        timestamp=timestamp,
        node_id=node_id,
        frame_index=frame_index,
        frame_id=stem,
        raw_depth_path=(
            candidate
            if (candidate := rgb_path.with_name(stem + "_depth_u32_mm.bin")).is_file()
            else None
        ),
    )


def _uniform_bound(records: Sequence[UnrealRGBDFramePaths], maximum: int) -> list[UnrealRGBDFramePaths]:
    if len(records) <= maximum:
        return list(records)
    if maximum == 1:
        return [records[-1]]
    indexes = [round(i * (len(records) - 1) / (maximum - 1)) for i in range(maximum)]
    return [records[index] for index in indexes]


def select_latest_unreal_rgbd_frames(
    frames_root: str | Path,
    *,
    node_id: str | None = None,
    max_frames: int = 24,
    session_gap_seconds: float = 30.0,
) -> tuple[UnrealRGBDFramePaths, ...]:
    """Return a bounded sample from the most recent complete capture session.

    A timestamp gap or non-increasing frame index starts a new session.  With
    no node filter, latest per-node sessions that overlap the newest campaign
    are combined.  Incomplete RGB/depth/JSON triplets are ignored.
    """

    if isinstance(max_frames, bool) or not isinstance(max_frames, int) or not 1 <= max_frames <= MAX_CLI_FRAMES:
        raise ValueError(f"max_frames must be an integer in [1, {MAX_CLI_FRAMES}]")
    gap_limit = _finite_positive("session_gap_seconds", session_gap_seconds, maximum=86_400.0)
    root = Path(frames_root).expanduser().resolve()
    if not root.is_dir():
        raise FileNotFoundError(f"frames root was not found: {root}")
    if node_id is not None and (not isinstance(node_id, str) or not node_id.strip()):
        raise ValueError("node_id must be a non-empty string when supplied")

    if node_id:
        direct = root / node_id
        directories = [direct] if direct.is_dir() else ([root] if root.name.casefold() == node_id.casefold() else [])
    elif any(root.glob("*_rgb.png")):
        directories = [root]
    else:
        directories = sorted((item for item in root.iterdir() if item.is_dir()), key=lambda p: p.name)
    if not directories:
        raise FileNotFoundError(f"no frame directory matched node {node_id!r} under {root}")

    candidate_count = 0
    latest_sessions: list[list[UnrealRGBDFramePaths]] = []
    for directory in directories:
        candidates = list(directory.glob("*_rgb.png"))
        candidate_count += len(candidates)
        if candidate_count > MAX_FRAME_CANDIDATES:
            raise ValueError(f"frame scan exceeds the hard limit of {MAX_FRAME_CANDIDATES} RGB files")
        records = [record for path in candidates if (record := _frame_paths(path, directory.name)) is not None]
        records.sort(key=lambda item: (item.timestamp, item.frame_index, item.rgb_path.name))
        if not records:
            continue
        start = 0
        for index in range(1, len(records)):
            gap = (records[index].timestamp - records[index - 1].timestamp).total_seconds()
            if gap > gap_limit or records[index].frame_index <= records[index - 1].frame_index:
                start = index
        latest_sessions.append(records[start:])
    if not latest_sessions:
        raise FileNotFoundError(f"no complete timestamped *_rgb/_depth PNG and JSON triplets were found under {root}")

    newest = max(session[-1].timestamp for session in latest_sessions)
    active = [
        record
        for session in latest_sessions
        if abs((newest - session[-1].timestamp).total_seconds()) <= gap_limit
        for record in session
    ]
    active.sort(key=lambda item: (item.timestamp, item.node_id, item.frame_index))
    # Persistence is defined over consecutive recent frames, so keep a tail
    # instead of uniformly subsampling the session timeline.
    return tuple(active[-max_frames:])


def load_unreal_rgbd_frame(paths: UnrealRGBDFramePaths) -> LoadedUnrealRGBDFrame:
    """Load RGB/preview metadata and prefer a complete validated raw sidecar."""

    try:
        from PIL import Image
    except (ImportError, ModuleNotFoundError) as exc:
        raise RuntimeError("Pillow is required to read Unreal RGB-D PNG files") from exc

    metadata = _json_object(paths.metadata_path)
    with Image.open(paths.rgb_path) as source:
        if source.width * source.height > MAX_IMAGE_PIXELS:
            raise ValueError(f"RGB image exceeds {MAX_IMAGE_PIXELS} pixels")
        rgb = np.asarray(source.convert("RGB"), dtype=np.uint8).copy()
    with Image.open(paths.depth_path) as source:
        if source.width * source.height > MAX_IMAGE_PIXELS:
            raise ValueError(f"depth image exceeds {MAX_IMAGE_PIXELS} pixels")
        depth_channels = np.asarray(source, dtype=np.uint8)
    if depth_channels.ndim == 2:
        depth_gray = depth_channels.copy()
    elif depth_channels.ndim == 3 and depth_channels.shape[2] >= 3:
        if not (
            np.array_equal(depth_channels[..., 0], depth_channels[..., 1])
            and np.array_equal(depth_channels[..., 0], depth_channels[..., 2])
        ):
            raise ValueError("depth preview RGB channels must encode the same grayscale value")
        depth_gray = depth_channels[..., 0].copy()
    else:
        raise ValueError("depth preview must be grayscale, RGB, or RGBA")
    if rgb.shape[:2] != depth_gray.shape:
        raise ValueError("RGB and depth preview dimensions do not match")
    declared_width = metadata.get("width")
    declared_height = metadata.get("height")
    if declared_width != rgb.shape[1] or declared_height != rgb.shape[0]:
        raise ValueError("metadata width/height do not match the paired images")
    if metadata.get("depthPngEncoding") not in (None, DEPTH_ENCODING):
        raise ValueError(f"unsupported Unreal depthPngEncoding: {metadata.get('depthPngEncoding')!r}")

    raw_depth: NDArray[np.uint32] | None = None
    selected_depth_path = paths.depth_path
    fallback_reason: str | None = None
    if paths.raw_depth_path is None or not paths.raw_depth_path.is_file():
        fallback_reason = (
            "declared_raw_sidecar_absent"
            if metadata.get("depthRawFileName") is not None
            else "raw_sidecar_not_declared"
        )
    else:
        expected_name = f"{paths.frame_id}_depth_u32_mm.bin"
        required_text = {
            "depthRawFileName": expected_name,
            "depthRawEncoding": DEPTH_RAW_ENCODING,
            "depthRawUnits": DEPTH_RAW_UNITS,
            "depthRawByteOrder": DEPTH_RAW_BYTE_ORDER,
            "depthRawSemantics": DEPTH_RAW_SEMANTICS,
        }
        for field, expected in required_text.items():
            if metadata.get(field) != expected:
                raise ValueError(f"raw depth metadata {field} must equal {expected!r}")
        required_numbers = {
            "depthRawWidth": rgb.shape[1],
            "depthRawHeight": rgb.shape[0],
            "depthRawInvalidValue": DEPTH_RAW_INVALID_VALUE,
            "depthRawSaturatedValue": DEPTH_RAW_SATURATED_VALUE,
        }
        for field, expected in required_numbers.items():
            actual = metadata.get(field)
            if isinstance(actual, bool) or not isinstance(actual, (int, float)) or float(actual) != float(expected):
                raise ValueError(f"raw depth metadata {field} must equal {expected}")
        quantization = metadata.get("depthRawQuantizationMeters")
        if (
            isinstance(quantization, bool)
            or not isinstance(quantization, (int, float))
            or not math.isclose(float(quantization), DEPTH_RAW_QUANTIZATION_METERS, rel_tol=0.0, abs_tol=1e-12)
        ):
            raise ValueError(
                f"raw depth metadata depthRawQuantizationMeters must equal {DEPTH_RAW_QUANTIZATION_METERS}"
            )
        if metadata.get("depthRawSimulationOnly") is not True:
            raise ValueError("raw depth metadata must declare depthRawSimulationOnly=true")
        maximum_representable = metadata.get("depthRawMaximumRepresentableMeters")
        if (
            isinstance(maximum_representable, bool)
            or not isinstance(maximum_representable, (int, float))
            or not math.isclose(float(maximum_representable), DEPTH_RAW_MAXIMUM_METERS, rel_tol=0.0, abs_tol=1e-12)
        ):
            raise ValueError(
                f"raw depth metadata depthRawMaximumRepresentableMeters must equal {DEPTH_RAW_MAXIMUM_METERS}"
            )

        expected_bytes = rgb.shape[0] * rgb.shape[1] * np.dtype("<u4").itemsize
        declared_bytes = metadata.get("depthRawFileByteCount")
        if (
            isinstance(declared_bytes, bool)
            or not isinstance(declared_bytes, (int, float))
            or float(declared_bytes) != float(expected_bytes)
        ):
            raise ValueError(f"raw depth metadata depthRawFileByteCount must equal {expected_bytes}")
        actual_bytes = paths.raw_depth_path.stat().st_size
        if actual_bytes != expected_bytes:
            raise ValueError(
                f"raw depth byte length {actual_bytes} does not match {rgb.shape[1]}x{rgb.shape[0]} uint32 ({expected_bytes})"
            )
        raw_depth = np.frombuffer(paths.raw_depth_path.read_bytes(), dtype="<u4").copy().reshape(rgb.shape[:2])
        selected_depth_path = paths.raw_depth_path

    return LoadedUnrealRGBDFrame(
        paths,
        rgb,
        depth_gray,
        metadata,
        depth_raw_u32_mm=raw_depth,
        depth_source_path=selected_depth_path,
        depth_fallback_reason=fallback_reason,
    )


@dataclass(frozen=True, slots=True)
class DecodedDepthPreview:
    distance_meters: NDArray[np.float32]
    valid_mask: NDArray[np.bool_]
    normalization_max_meters: float
    configured_max_range_meters: float
    quantization_step_meters: float
    encoding: str = DEPTH_ENCODING
    source_semantics: str = "normalized_scene_depth_png_preview"
    units: str = "meters"
    byte_order: str | None = None
    requested_max_range_meters: float | None = None


def decode_unreal_depth_preview(
    depth_preview_gray: ArrayLike,
    *,
    normalization_max_meters: float,
    max_range_meters: float = DEFAULT_SIMULATION_MAX_RANGE_METERS,
    min_range_meters: float = 0.0,
) -> DecodedDepthPreview:
    """Decode Unreal's normalized preview and exclude zero/saturated/clipped pixels."""

    value = np.asarray(depth_preview_gray)
    if value.ndim != 2 or value.shape[0] < 1 or value.shape[1] < 1:
        raise ValueError("depth_preview_gray must have non-empty shape (height, width)")
    if value.dtype != np.uint8:
        raise TypeError("depth_preview_gray must use uint8 gray values")
    normalization = _finite_positive("normalization_max_meters", normalization_max_meters, maximum=1_000_000.0)
    maximum = _finite_positive("max_range_meters", max_range_meters, maximum=1_000_000.0)
    minimum = float(min_range_meters)
    if not math.isfinite(minimum) or minimum < 0.0:
        raise ValueError("min_range_meters must be finite and >= 0")
    if maximum > normalization:
        raise ValueError("max_range_meters must be <= normalization_max_meters for a normalized preview")
    if minimum >= maximum:
        raise ValueError("min_range_meters must be < max_range_meters")
    distance = value.astype(np.float32) * np.float32(normalization / 255.0)
    # 255 represents the normalization ceiling and also receives any clipped
    # scene depth.  Zero has no useful positive range.  Neither is ranged.
    valid = (
        (value > 0)
        & (value < 255)
        & np.isfinite(distance)
        & (distance >= minimum)
        & (distance <= maximum)
    )
    return DecodedDepthPreview(
        distance_meters=distance,
        valid_mask=valid,
        normalization_max_meters=normalization,
        configured_max_range_meters=maximum,
        quantization_step_meters=normalization / 255.0,
        requested_max_range_meters=maximum,
    )


def decode_unreal_depth_u32_mm(
    depth_u32_mm: ArrayLike,
    *,
    max_range_meters: float = DEFAULT_SIMULATION_MAX_RANGE_METERS,
    min_range_meters: float = 0.0,
) -> DecodedDepthPreview:
    """Decode validated uint32 millimetres and exclude invalid/saturated values.

    ``0`` is invalid. ``0xFFFFFFFF`` is a clamped/saturated sentinel and is
    also excluded, leaving more than 4,294 km of unambiguous encoded range. The
    result is Unreal SceneDepth simulation geometry, not calibrated OAK depth.
    """

    value = np.asarray(depth_u32_mm)
    if value.ndim != 2 or value.shape[0] < 1 or value.shape[1] < 1:
        raise ValueError("depth_u32_mm must have non-empty shape (height, width)")
    if value.dtype != np.dtype(np.uint32):
        raise TypeError("depth_u32_mm must use uint32 millimetre values")
    requested_maximum = _finite_positive("max_range_meters", max_range_meters, maximum=1_000_000.0)
    maximum = min(requested_maximum, DEPTH_RAW_MAXIMUM_VALID_METERS)
    minimum = float(min_range_meters)
    if not math.isfinite(minimum) or minimum < 0.0:
        raise ValueError("min_range_meters must be finite and >= 0")
    if minimum >= maximum:
        raise ValueError("min_range_meters must be < the uint32 millimetre storage/range limit")
    distance = value.astype(np.float32) * np.float32(DEPTH_RAW_QUANTIZATION_METERS)
    valid = (
        (value > DEPTH_RAW_INVALID_VALUE)
        & (value < DEPTH_RAW_SATURATED_VALUE)
        & np.isfinite(distance)
        & (distance >= minimum)
        & (distance <= maximum)
    )
    return DecodedDepthPreview(
        distance_meters=distance,
        valid_mask=valid,
        normalization_max_meters=DEPTH_RAW_MAXIMUM_METERS,
        configured_max_range_meters=maximum,
        quantization_step_meters=DEPTH_RAW_QUANTIZATION_METERS,
        encoding=DEPTH_RAW_OBSERVATION_ENCODING,
        source_semantics=DEPTH_RAW_SEMANTICS,
        units=DEPTH_RAW_UNITS,
        byte_order=DEPTH_RAW_BYTE_ORDER,
        requested_max_range_meters=requested_maximum,
    )


@dataclass(frozen=True, slots=True)
class BoxDepthEstimate:
    estimated_distance_meters: float | None
    valid_pixel_count: int
    roi_semantics: str
    percentile: float
    bbox_xyxy_pixels: tuple[int, int, int, int]


def depth_z_to_slant_range(
    depth_z_meters: float,
    bbox_xyxy_pixels: Sequence[float],
    *,
    frame_width: int,
    frame_height: int,
    horizontal_fov_degrees: float,
) -> float:
    """Convert optical-axis depth Z to a bbox-centre camera-ray slant range.

    Unreal metadata currently supplies a horizontal field of view.  Square
    pixels are assumed, so ``fy == fx`` and the vertical field follows from
    frame dimensions.  This is a geometric simulation estimate, not calibrated
    OAK-camera range.
    """

    depth_z = _finite_positive("depth_z_meters", depth_z_meters, maximum=1_000_000.0)
    if isinstance(frame_width, bool) or not isinstance(frame_width, int) or frame_width < 1:
        raise ValueError("frame_width must be a positive integer")
    if isinstance(frame_height, bool) or not isinstance(frame_height, int) or frame_height < 1:
        raise ValueError("frame_height must be a positive integer")
    if len(bbox_xyxy_pixels) != 4:
        raise ValueError("bbox_xyxy_pixels must contain x1, y1, x2, y2")
    box = tuple(float(item) for item in bbox_xyxy_pixels)
    if not all(math.isfinite(item) for item in box):
        raise ValueError("bbox coordinates must be finite")
    fov = float(horizontal_fov_degrees)
    if not math.isfinite(fov) or not 0.0 < fov < 180.0:
        raise ValueError("horizontal_fov_degrees must be finite and in (0, 180)")
    focal_pixels = frame_width / (2.0 * math.tan(math.radians(fov) / 2.0))
    center_x = (box[0] + box[2]) / 2.0
    center_y = (box[1] + box[3]) / 2.0
    normalized_x = (center_x - frame_width / 2.0) / focal_pixels
    normalized_y = (center_y - frame_height / 2.0) / focal_pixels
    return depth_z * math.sqrt(1.0 + normalized_x * normalized_x + normalized_y * normalized_y)


def estimate_box_depth(
    decoded: DecodedDepthPreview,
    bbox_xyxy_pixels: Sequence[float],
    *,
    inset_fraction: float = 0.15,
    minimum_valid_pixels: int = 20,
    foreground_percentile: float = 35.0,
) -> BoxDepthEstimate:
    """Range a detection using 15% inset, then full-box fallback, at P35."""

    if len(bbox_xyxy_pixels) != 4:
        raise ValueError("bbox_xyxy_pixels must contain x1, y1, x2, y2")
    numbers = tuple(float(item) for item in bbox_xyxy_pixels)
    if not all(math.isfinite(item) for item in numbers):
        raise ValueError("bbox coordinates must be finite")
    if not math.isfinite(inset_fraction) or not 0.0 <= inset_fraction < 0.5:
        raise ValueError("inset_fraction must be finite and in [0, 0.5)")
    if isinstance(minimum_valid_pixels, bool) or not isinstance(minimum_valid_pixels, int) or minimum_valid_pixels < 1:
        raise ValueError("minimum_valid_pixels must be an integer >= 1")
    if not math.isfinite(foreground_percentile) or not 0.0 <= foreground_percentile <= 100.0:
        raise ValueError("foreground_percentile must be finite and in [0, 100]")
    height, width = decoded.distance_meters.shape
    x1 = min(max(int(math.floor(numbers[0])), 0), width)
    y1 = min(max(int(math.floor(numbers[1])), 0), height)
    x2 = min(max(int(math.ceil(numbers[2])), 0), width)
    y2 = min(max(int(math.ceil(numbers[3])), 0), height)
    if x2 <= x1 or y2 <= y1:
        return BoxDepthEstimate(None, 0, "empty_box", foreground_percentile, (x1, y1, x2, y2))

    inset_x = (x2 - x1) * inset_fraction
    inset_y = (y2 - y1) * inset_fraction
    ix1 = min(max(int(math.floor(x1 + inset_x)), x1), x2)
    iy1 = min(max(int(math.floor(y1 + inset_y)), y1), y2)
    ix2 = min(max(int(math.ceil(x2 - inset_x)), x1), x2)
    iy2 = min(max(int(math.ceil(y2 - inset_y)), y1), y2)

    def values(left: int, top: int, right: int, bottom: int) -> NDArray[np.float32]:
        if right <= left or bottom <= top:
            return np.empty(0, dtype=np.float32)
        roi_depth = decoded.distance_meters[top:bottom, left:right]
        roi_valid = decoded.valid_mask[top:bottom, left:right]
        return roi_depth[roi_valid]

    selected = values(ix1, iy1, ix2, iy2)
    semantics = "inset_15_percent"
    if selected.size < minimum_valid_pixels:
        selected = values(x1, y1, x2, y2)
        semantics = "full_box_fallback"
    if selected.size < minimum_valid_pixels:
        return BoxDepthEstimate(None, int(selected.size), semantics + "_insufficient_valid_pixels", foreground_percentile, (x1, y1, x2, y2))
    estimate = float(np.percentile(selected, foreground_percentile))
    return BoxDepthEstimate(estimate, int(selected.size), semantics, foreground_percentile, (x1, y1, x2, y2))


def _as_numpy(value: Any) -> NDArray[np.generic]:
    if hasattr(value, "detach"):
        value = value.detach()
    if hasattr(value, "cpu"):
        value = value.cpu()
    if hasattr(value, "numpy"):
        value = value.numpy()
    return np.asarray(value)


def _rgb_to_ultralytics_bgr(image: ArrayLike, input_color_space: str) -> NDArray[np.uint8]:
    value = np.asarray(image)
    if value.dtype != np.uint8 or value.ndim != 3 or value.shape[2] != 3 or value.shape[0] < 1 or value.shape[1] < 1:
        raise ValueError("image must be a non-empty uint8 array with shape (height, width, 3)")
    color = input_color_space.strip().upper() if isinstance(input_color_space, str) else ""
    if color == "RGB":
        value = value[..., ::-1]
    elif color != "BGR":
        raise ValueError("input_color_space must be exactly 'RGB' or 'BGR'")
    # Ultralytics' NumPy/OpenCV source contract is BGR; it performs its own
    # BGR-to-RGB tensor conversion during preprocessing.
    return np.ascontiguousarray(value)


def _box_iou(left: Sequence[float], right: Sequence[float]) -> float:
    ix1, iy1 = max(left[0], right[0]), max(left[1], right[1])
    ix2, iy2 = min(left[2], right[2]), min(left[3], right[3])
    intersection = max(0.0, ix2 - ix1) * max(0.0, iy2 - iy1)
    left_area = max(0.0, left[2] - left[0]) * max(0.0, left[3] - left[1])
    right_area = max(0.0, right[2] - right[0]) * max(0.0, right[3] - right[1])
    union = left_area + right_area - intersection
    return intersection / union if union > 0.0 else 0.0


def _simulation_iou_diagnostic(box: Sequence[float], metadata: Mapping[str, Any]) -> dict[str, Any] | None:
    targets = metadata.get("targets")
    if not isinstance(targets, list):
        return None
    matches: list[tuple[float, Mapping[str, Any]]] = []
    for target in targets:
        if not isinstance(target, dict) or target.get("intersectsFrame") is False:
            continue
        truth_box = target.get("bboxXyxyPixels")
        if not isinstance(truth_box, list) or len(truth_box) != 4:
            continue
        try:
            numeric = tuple(float(item) for item in truth_box)
        except (TypeError, ValueError):
            continue
        if not all(math.isfinite(item) for item in numeric):
            continue
        matches.append((_box_iou(box, numeric), target))
    if not matches:
        return None
    best_iou, best = max(matches, key=lambda item: item[0])
    result: dict[str, Any] = {
        "best_iou": best_iou,
        "diagnostic_scope": "projected_simulation_truth_only",
        "physical_accuracy_claimed": False,
        "not_precision_or_recall": True,
    }
    actor = best.get("actorName")
    if isinstance(actor, str):
        result["matched_actor_name"] = actor
    if isinstance(best.get("hostileScenarioTruth"), bool):
        result["matched_hostile_scenario_truth"] = best["hostileScenarioTruth"]
    return result


class OAKRGBDAdapter:
    """Hash-pinned RGB YOLO adapter with bounded post-detection depth ranging."""

    def __init__(
        self,
        checkpoint_path: str | Path,
        *,
        trusted_sha256: str,
        confidence_threshold: float = 0.25,
        max_detections: int = 10,
        device: str = "cpu",
        max_range_meters: float = DEFAULT_SIMULATION_MAX_RANGE_METERS,
        minimum_valid_depth_pixels: int = 20,
        range_mode: str = RANGE_MODE_SIMULATION_ADAPTED,
    ) -> None:
        self.checkpoint_path = Path(checkpoint_path).expanduser().resolve()
        digest = trusted_sha256.strip().upper() if isinstance(trusted_sha256, str) else ""
        if len(digest) != 64 or any(character not in "0123456789ABCDEF" for character in digest):
            raise ValueError("trusted_sha256 must be an explicit 64-character hexadecimal digest")
        if not math.isfinite(confidence_threshold) or not 0.0 <= confidence_threshold <= 1.0:
            raise ValueError("confidence_threshold must be finite and in [0, 1]")
        if isinstance(max_detections, bool) or not isinstance(max_detections, int) or not 1 <= max_detections <= 1000:
            raise ValueError("max_detections must be an integer in [1, 1000]")
        if not isinstance(device, str) or not device.strip():
            raise ValueError("device must be a non-empty string")
        if isinstance(minimum_valid_depth_pixels, bool) or not isinstance(minimum_valid_depth_pixels, int) or minimum_valid_depth_pixels < 1:
            raise ValueError("minimum_valid_depth_pixels must be an integer >= 1")
        if range_mode not in (RANGE_MODE_PHYSICAL_OAK, RANGE_MODE_SIMULATION_ADAPTED):
            raise ValueError(
                f"range_mode must be {RANGE_MODE_PHYSICAL_OAK!r} or {RANGE_MODE_SIMULATION_ADAPTED!r}"
            )
        if self.checkpoint_path.suffix.lower() != ".pt":
            raise RGBDModelBlockedError(
                "blocked_untrusted_artifact_type",
                "OAKRGBDAdapter accepts only an explicitly hash-pinned .pt checkpoint.",
            )
        self.trusted_sha256 = digest
        self.confidence_threshold = float(confidence_threshold)
        self.max_detections = max_detections
        self.device = device.strip()
        self.max_range_meters = _finite_positive("max_range_meters", max_range_meters, maximum=1_000_000.0)
        self.minimum_valid_depth_pixels = minimum_valid_depth_pixels
        self.range_mode = range_mode
        self._model: Any | None = None
        self._verified_sha256: str | None = None

    @property
    def loaded(self) -> bool:
        return self._model is not None

    @property
    def verified_sha256(self) -> str | None:
        return self._verified_sha256

    def load(self) -> None:
        """Verify the explicit checkpoint before any Ultralytics deserialization."""

        if self._model is not None:
            return
        if not self.checkpoint_path.is_file():
            raise RGBDModelBlockedError("blocked_missing_file", f"RGB checkpoint not found: {self.checkpoint_path}")
        actual = sha256_file(self.checkpoint_path)
        if actual != self.trusted_sha256:
            raise RGBDModelBlockedError(
                "blocked_integrity_mismatch",
                "RGB checkpoint SHA-256 does not match the explicit trusted digest; it was not loaded.",
            )
        try:
            from ultralytics import YOLO
        except (ImportError, ModuleNotFoundError) as exc:
            raise RGBDModelBlockedError("blocked_missing_ultralytics", "Ultralytics is required to load the trusted RGB checkpoint.") from exc
        try:
            model = YOLO(str(self.checkpoint_path))
        except Exception as exc:
            raise RGBDModelBlockedError("blocked_model_load_failure", f"Ultralytics could not load the trusted RGB checkpoint: {exc}") from exc
        self._verified_sha256 = actual
        self._model = model

    def infer(
        self,
        image: ArrayLike,
        depth_preview_gray: ArrayLike,
        depth_metadata: Mapping[str, Any],
        *,
        node_id: str,
        timestamp: datetime | None = None,
        frame_id: str | None = None,
        input_color_space: str = "RGB",
        include_simulation_iou_diagnostic: bool = False,
        depth_raw_u32_mm: ArrayLike | None = None,
        depth_source_path: str | Path | None = None,
        depth_fallback_reason: str | None = None,
    ) -> tuple[SensorObservation, ...]:
        """Detect in RGB, then estimate depth independently inside each box."""

        if not isinstance(node_id, str) or not node_id.strip():
            raise ValueError("node_id must be a non-empty string")
        if not isinstance(depth_metadata, Mapping):
            raise TypeError("depth_metadata must be a mapping")
        if timestamp is None:
            timestamp = datetime.now(timezone.utc)
        if timestamp.tzinfo is None or timestamp.utcoffset() is None:
            raise ValueError("timestamp must be timezone-aware")
        bgr = _rgb_to_ultralytics_bgr(image, input_color_space)
        if depth_raw_u32_mm is not None:
            if self.range_mode == RANGE_MODE_PHYSICAL_OAK:
                applied_minimum = PHYSICAL_OAK_MIN_RANGE_METERS
                applied_maximum = min(PHYSICAL_OAK_MAX_RANGE_METERS, DEPTH_RAW_MAXIMUM_VALID_METERS)
            else:
                applied_minimum = 0.0
                applied_maximum = min(self.max_range_meters, DEPTH_RAW_MAXIMUM_VALID_METERS)
            decoded = decode_unreal_depth_u32_mm(
                depth_raw_u32_mm,
                max_range_meters=applied_maximum,
                min_range_meters=applied_minimum,
            )
        else:
            normalization_raw = depth_metadata.get("normalizationMaxMeters")
            if isinstance(normalization_raw, bool) or not isinstance(normalization_raw, (int, float)):
                raise ValueError("depth metadata must contain numeric normalizationMaxMeters")
            normalization = float(normalization_raw)
            if self.range_mode == RANGE_MODE_PHYSICAL_OAK:
                applied_minimum = PHYSICAL_OAK_MIN_RANGE_METERS
                applied_maximum = min(PHYSICAL_OAK_MAX_RANGE_METERS, normalization)
            else:
                applied_minimum = 0.0
                applied_maximum = self.max_range_meters
            decoded = decode_unreal_depth_preview(
                depth_preview_gray,
                normalization_max_meters=normalization,
                max_range_meters=applied_maximum,
                min_range_meters=applied_minimum,
            )
        if bgr.shape[:2] != decoded.distance_meters.shape:
            raise ValueError("RGB and selected depth source dimensions do not match")
        self.load()
        assert self._model is not None
        try:
            raw_results = self._model.predict(
                source=bgr,
                conf=self.confidence_threshold,
                max_det=self.max_detections,
                imgsz=832,
                iou=0.5,
                device=self.device,
                verbose=False,
            )
        except Exception as exc:
            raise RGBDModelBlockedError("blocked_inference_failure", f"RGB YOLO inference failed: {exc}") from exc

        height, width = bgr.shape[:2]
        candidates: list[tuple[float, int, str, tuple[float, float, float, float]]] = []
        for result in raw_results or ():
            boxes = getattr(result, "boxes", None)
            if boxes is None:
                continue
            xyxy = _as_numpy(getattr(boxes, "xyxy", np.empty((0, 4)))).reshape(-1, 4)
            confidence = _as_numpy(getattr(boxes, "conf", np.empty(0))).reshape(-1)
            classes = _as_numpy(getattr(boxes, "cls", np.zeros(len(confidence)))).reshape(-1)
            names = getattr(result, "names", getattr(self._model, "names", {}))
            for index in range(min(len(xyxy), len(confidence), len(classes))):
                score = float(confidence[index])
                raw_box = tuple(float(item) for item in xyxy[index])
                if not math.isfinite(score) or score < self.confidence_threshold or not all(math.isfinite(item) for item in raw_box):
                    continue
                x1 = float(np.clip(raw_box[0], 0.0, width))
                y1 = float(np.clip(raw_box[1], 0.0, height))
                x2 = float(np.clip(raw_box[2], 0.0, width))
                y2 = float(np.clip(raw_box[3], 0.0, height))
                if x2 <= x1 or y2 <= y1:
                    continue
                class_id = int(classes[index])
                if isinstance(names, dict):
                    class_name = str(names.get(class_id, class_id))
                elif isinstance(names, (list, tuple)) and 0 <= class_id < len(names):
                    class_name = str(names[class_id])
                else:
                    class_name = str(class_id)
                candidates.append((min(max(score, 0.0), 1.0), class_id, class_name, (x1, y1, x2, y2)))
        candidates.sort(key=lambda item: item[0], reverse=True)

        observations: list[SensorObservation] = []
        for score, class_id, class_name, box in candidates[: self.max_detections]:
            ranging = estimate_box_depth(
                decoded,
                box,
                inset_fraction=0.15,
                minimum_valid_pixels=self.minimum_valid_depth_pixels,
                foreground_percentile=35.0,
            )
            raw_fov = depth_metadata.get("fovDegrees")
            if isinstance(raw_fov, bool) or not isinstance(raw_fov, (int, float)):
                raise ValueError("depth metadata must contain numeric horizontal fovDegrees")
            slant_range = (
                depth_z_to_slant_range(
                    ranging.estimated_distance_meters,
                    box,
                    frame_width=width,
                    frame_height=height,
                    horizontal_fov_degrees=float(raw_fov),
                )
                if ranging.estimated_distance_meters is not None
                else None
            )
            normalized_box = [box[0] / width, box[1] / height, box[2] / width, box[3] / height]
            measurements: dict[str, Any] = {
                "bbox_xyxy_normalized": normalized_box,
                "bbox_xyxy_pixels": list(box),
                "class_id": class_id,
                "class_name": class_name,
                "frame_width": width,
                "frame_height": height,
                "depth_z_m": ranging.estimated_distance_meters,
                "slant_range_m": slant_range,
                "estimated_distance_meters": slant_range,
                "depth_valid_pixel_count": ranging.valid_pixel_count,
                "depth_roi_semantics": ranging.roi_semantics,
                "depth_foreground_percentile": ranging.percentile,
                "depth_inset_fraction": 0.15,
                "depth_minimum_valid_pixels": self.minimum_valid_depth_pixels,
                "depth_png_encoding": DEPTH_ENCODING,
                "depth_selected_encoding": decoded.encoding,
                "depth_source_semantics": decoded.source_semantics,
                "depth_source_units": decoded.units,
                "depth_source_byte_order": decoded.byte_order,
                "depth_source_file": str(depth_source_path) if depth_source_path is not None else None,
                "depth_source_fallback_reason": depth_fallback_reason,
                "depth_normalization_max_meters": decoded.normalization_max_meters,
                "depth_configured_max_range_meters": decoded.configured_max_range_meters,
                "depth_requested_max_range_meters": decoded.requested_max_range_meters,
                "range_mode": self.range_mode,
                "physical_oak_contract_range_gate_meters": [
                    PHYSICAL_OAK_MIN_RANGE_METERS,
                    PHYSICAL_OAK_MAX_RANGE_METERS,
                ],
                "applied_range_gate_meters": [applied_minimum, applied_maximum],
                "depth_quantization_step_meters": decoded.quantization_step_meters,
                "saturated_or_clipped_depth_pixels_excluded": True,
                "model_input_channels": 3,
                "depth_used_as_model_input": False,
                "depth_role": "post_detection_ranging",
                "caller_image_color_space": input_color_space.upper(),
                "ultralytics_numpy_input_color_space": "BGR",
                "source_semantics": f"unreal_rgb_plus_{decoded.source_semantics}",
                "range_semantics": (
                    f"{decoded.source_semantics}_estimate"
                    if self.range_mode == RANGE_MODE_SIMULATION_ADAPTED
                    else f"physical_oak_gate_applied_to_{decoded.source_semantics}"
                ),
                "slant_range_geometry": "bbox_center_ray_from_horizontal_fov_square_pixel_assumption",
                "physical_camera_range_claimed": False,
                "physical_accuracy_claimed": False,
                "detection_only": True,
            }
            if frame_id is not None:
                measurements["frame_id"] = str(frame_id)
            if include_simulation_iou_diagnostic:
                diagnostic = _simulation_iou_diagnostic(box, depth_metadata)
                if diagnostic is not None:
                    measurements["simulation_ground_truth_iou_diagnostic"] = diagnostic
            observations.append(
                SensorObservation(
                    observation_id=f"rgbd-{uuid4()}",
                    timestamp=timestamp,
                    node_id=node_id.strip(),
                    modality=SensorModality.RGBD,
                    detection_probability=score,
                    confidence_level=score,
                    model_id=f"oak_rgb_yolo:{self.trusted_sha256[:12].lower()}",
                    measurements=measurements,
                )
            )
        return tuple(observations)

    def infer_unreal_frame(
        self,
        frame: LoadedUnrealRGBDFrame,
        *,
        include_simulation_iou_diagnostic: bool = False,
    ) -> tuple[SensorObservation, ...]:
        return self.infer(
            frame.rgb,
            frame.depth_preview_gray,
            frame.metadata,
            node_id=frame.paths.node_id,
            timestamp=frame.paths.timestamp,
            frame_id=frame.paths.frame_id,
            input_color_space="RGB",
            include_simulation_iou_diagnostic=include_simulation_iou_diagnostic,
            depth_raw_u32_mm=frame.depth_raw_u32_mm,
            depth_source_path=frame.depth_source_path,
            depth_fallback_reason=frame.depth_fallback_reason,
        )


def write_json_atomic(value: Mapping[str, Any], output_path: str | Path) -> Path:
    """Write strict JSON through a same-directory temporary file and replace."""

    destination = Path(output_path).expanduser().resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            dir=destination.parent,
            prefix=f".{destination.name}.",
            suffix=".tmp",
            delete=False,
        ) as handle:
            temporary = Path(handle.name)
            json.dump(value, handle, indent=2, allow_nan=False)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, destination)
    except Exception:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
        raise
    return destination


def _persistence_descriptor(observation: Mapping[str, Any]) -> tuple[str, tuple[float, float, float, float]] | None:
    measurements = observation.get("measurements")
    if not isinstance(measurements, dict):
        return None
    class_name = measurements.get("class_name")
    box = measurements.get("bbox_xyxy_pixels")
    if not isinstance(class_name, str) or not isinstance(box, list) or len(box) != 4:
        return None
    try:
        numeric = tuple(float(item) for item in box)
    except (TypeError, ValueError):
        return None
    if not all(math.isfinite(item) for item in numeric):
        return None
    return class_name, numeric  # type: ignore[return-value]


def apply_2_of_3_persistence(
    observations: Sequence[dict[str, Any]],
    prior_frame_detections: Sequence[Sequence[tuple[str, tuple[float, float, float, float]]]],
    *,
    iou_threshold: float = 0.3,
) -> tuple[int, list[tuple[str, tuple[float, float, float, float]]]]:
    """Annotate one frame's detections with archive-compatible persistence.

    A current detection is persistent when a same-class box overlaps at least
    one of the two prior frames at IoU >= 0.3, i.e. it appears in two of the
    last three frames.  Raw confidence values are not changed.
    """

    if not math.isfinite(iou_threshold) or not 0.0 <= iou_threshold <= 1.0:
        raise ValueError("iou_threshold must be finite and in [0, 1]")
    current = [descriptor for item in observations if (descriptor := _persistence_descriptor(item)) is not None]
    persistent_count = 0
    for observation in observations:
        descriptor = _persistence_descriptor(observation)
        matches = 0
        if descriptor is not None:
            class_name, box = descriptor
            for prior_frame in prior_frame_detections[-2:]:
                if any(
                    prior_class == class_name and _box_iou(box, prior_box) >= iou_threshold
                    for prior_class, prior_box in prior_frame
                ):
                    matches += 1
        confirmed = descriptor is not None and (1 + matches) >= 2
        measurements = observation.get("measurements")
        if isinstance(measurements, dict):
            measurements["temporal_persistence"] = {
                "rule": "2_of_last_3_frames",
                "iou_threshold": iou_threshold,
                "same_class_required": True,
                "matched_prior_frame_count": matches,
                "persistent_detection": confirmed,
                "raw_confidence_preserved": True,
            }
        persistent_count += int(confirmed)
    return persistent_count, current


def run_latest_frame_session(
    *,
    frames_root: str | Path,
    checkpoint_path: str | Path,
    trusted_sha256: str,
    output_path: str | Path,
    node_id: str | None = None,
    max_frames: int = 24,
    max_observations: int = 1_000,
    confidence_threshold: float = 0.25,
    max_detections_per_frame: int = 10,
    device: str = "cpu",
    max_range_meters: float = DEFAULT_SIMULATION_MAX_RANGE_METERS,
    range_mode: str = RANGE_MODE_SIMULATION_ADAPTED,
    include_simulation_iou_diagnostic: bool = False,
) -> dict[str, Any]:
    """Run a bounded latest-session campaign and atomically write observations."""

    if isinstance(max_observations, bool) or not isinstance(max_observations, int) or not 1 <= max_observations <= MAX_CLI_OBSERVATIONS:
        raise ValueError(f"max_observations must be an integer in [1, {MAX_CLI_OBSERVATIONS}]")
    frames = select_latest_unreal_rgbd_frames(frames_root, node_id=node_id, max_frames=max_frames)
    adapter = OAKRGBDAdapter(
        checkpoint_path,
        trusted_sha256=trusted_sha256,
        confidence_threshold=confidence_threshold,
        max_detections=max_detections_per_frame,
        device=device,
        max_range_meters=max_range_meters,
        range_mode=range_mode,
    )
    observations: list[dict[str, Any]] = []
    processed: list[dict[str, Any]] = []
    ranged_count = 0
    persistent_count = 0
    raw_u32_depth_frame_count = 0
    history_by_node: dict[str, list[list[tuple[str, tuple[float, float, float, float]]]]] = {}
    for selected in frames:
        if len(observations) >= max_observations:
            break
        frame = load_unreal_rgbd_frame(selected)
        raw_u32_depth_frame_count += int(frame.depth_raw_u32_mm is not None)
        detected = adapter.infer_unreal_frame(
            frame,
            include_simulation_iou_diagnostic=include_simulation_iou_diagnostic,
        )
        remaining = max_observations - len(observations)
        kept = detected[:remaining]
        rows = [item.to_dict() for item in kept]
        history = history_by_node.setdefault(selected.node_id, [])
        frame_persistent, current_descriptors = apply_2_of_3_persistence(
            rows,
            history,
            iou_threshold=0.3,
        )
        history.append(current_descriptors)
        del history[:-2]
        observations.extend(rows)
        persistent_count += frame_persistent
        ranged_count += sum(item.measurements.get("slant_range_m") is not None for item in kept)
        processed.append(
            {
                "node_id": selected.node_id,
                "frame_id": selected.frame_id,
                "timestamp": selected.timestamp.isoformat().replace("+00:00", "Z"),
                "rgb_path": str(selected.rgb_path),
                "depth_path": str(selected.depth_path),
                "depth_preview_path": str(selected.depth_path),
                "depth_raw_path": str(selected.raw_depth_path) if selected.raw_depth_path is not None else None,
                "depth_selected_source_path": str(frame.depth_source_path) if frame.depth_source_path is not None else None,
                "depth_selected_encoding": (
                    DEPTH_RAW_OBSERVATION_ENCODING if frame.depth_raw_u32_mm is not None else DEPTH_ENCODING
                ),
                "depth_quantization_step_meters": (
                    DEPTH_RAW_QUANTIZATION_METERS
                    if frame.depth_raw_u32_mm is not None
                    else float(frame.metadata["normalizationMaxMeters"]) / 255.0
                ),
                "depth_fallback_reason": frame.depth_fallback_reason,
                "metadata_path": str(selected.metadata_path),
                "detections_kept": len(kept),
                "persistent_detections": frame_persistent,
            }
        )
    report: dict[str, Any] = {
        "schema_version": "1.0",
        "campaign_kind": "bounded_unreal_oak_rgbd_post_detection_ranging",
        "generated_at_utc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "inputs": {
            "frames_root": str(Path(frames_root).expanduser().resolve()),
            "node_id": node_id,
            "checkpoint_path": str(Path(checkpoint_path).expanduser().resolve()),
            "verified_checkpoint_sha256": adapter.verified_sha256,
            "caller_png_color_space": "RGB",
            "ultralytics_numpy_input_color_space": "BGR",
        },
        "limits": {
            "max_frames": max_frames,
            "max_observations": max_observations,
            "max_detections_per_frame": max_detections_per_frame,
            "max_range_meters": max_range_meters,
            "range_mode": range_mode,
        },
        "summary": {
            "selected_frame_count": len(frames),
            "processed_frame_count": len(processed),
            "observation_count": len(observations),
            "raw_detection_count": len(observations),
            "persistent_detection_count": persistent_count,
            "observations_with_depth_range": ranged_count,
            "raw_u32_depth_frame_count": raw_u32_depth_frame_count,
            "preview_fallback_frame_count": len(processed) - raw_u32_depth_frame_count,
        },
        "processed_frames": processed,
        "observations": observations,
        "semantics": {
            "rgb_yolo_input_only": True,
            "depth_is_post_detection_ranging": True,
            "depth_is_fourth_model_channel": False,
            "depth_algorithm": "15_percent_inset_then_full_box_fallback_foreground_biased_p35",
            "temporal_persistence": "2_of_last_3_frames_same_class_iou_gte_0_3",
            "raw_confidences_preserved": True,
            "invalid_and_saturated_depth_pixels_excluded": True,
            "raw_u32_depth_preferred_when_complete": True,
            "preview_depth_fallback_supported": True,
            "simulation_ground_truth_iou_diagnostic_enabled": include_simulation_iou_diagnostic,
            "physical_accuracy_claimed": False,
            "detection_only": True,
        },
        "limitations": [
            "The preferred uint32 millimetre sidecar is Unreal SceneDepth simulation geometry, not a raw physical OAK depth frame.",
            "The uint32 millimetre format excludes its saturated 0xFFFFFFFF sentinel and retains 0.001 m quantization across the 10 km simulation range.",
            "When the raw sidecar is absent, the normalized 8-bit PNG is used; its 255 clipped/ambiguous value is excluded.",
            "Any projected simulation-truth IoU is a diagnostic only, not physical accuracy, precision, or recall.",
            "This adapter performs detection and ranging only; it contains no engagement or effector behavior.",
        ],
    }
    write_json_atomic(report, output_path)
    return report


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run hash-pinned OAK RGB detection and post-detection depth ranging on the latest Unreal frame session.")
    parser.add_argument("--frames-root", required=True)
    parser.add_argument("--checkpoint", required=True, help="Explicit Ultralytics .pt RGB detector path.")
    parser.add_argument("--trusted-sha256", required=True, help="Expected SHA-256 of the selected .pt checkpoint.")
    parser.add_argument("--output", required=True, help="Atomic JSON output path.")
    parser.add_argument("--node", help="Optional Unreal sensor node ID.")
    parser.add_argument("--max-frames", type=int, default=24)
    parser.add_argument("--max-observations", type=int, default=1_000)
    parser.add_argument("--confidence", type=float, default=0.25)
    parser.add_argument("--max-detections-per-frame", type=int, default=10)
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--max-range-meters", type=float, default=DEFAULT_SIMULATION_MAX_RANGE_METERS)
    parser.add_argument(
        "--range-mode",
        choices=(RANGE_MODE_SIMULATION_ADAPTED, RANGE_MODE_PHYSICAL_OAK),
        default=RANGE_MODE_SIMULATION_ADAPTED,
        help="Use explicit simulation-adapted preview range or the archive's physical OAK 0.2-30 m gate.",
    )
    parser.add_argument("--simulation-iou-diagnostic", action="store_true", help="Add descriptive IoU against Unreal projected scenario truth; never physical accuracy.")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    report = run_latest_frame_session(
        frames_root=args.frames_root,
        checkpoint_path=args.checkpoint,
        trusted_sha256=args.trusted_sha256,
        output_path=args.output,
        node_id=args.node,
        max_frames=args.max_frames,
        max_observations=args.max_observations,
        confidence_threshold=args.confidence,
        max_detections_per_frame=args.max_detections_per_frame,
        device=args.device,
        max_range_meters=args.max_range_meters,
        range_mode=args.range_mode,
        include_simulation_iou_diagnostic=args.simulation_iou_diagnostic,
    )
    print(json.dumps({"output": str(Path(args.output).expanduser().resolve()), **report["summary"]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
