"""Bounded proxy projection for simulated event-camera imagery.

This module does **not** reproduce the private preprocessing used to train the
supplied FRED-derived checkpoint.  That checkpoint's exact event accumulation,
polarity encoding, normalization, and tiling code were not supplied.  The
projector below is deliberately named and tagged ``proxy_event_stack_v1`` so a
simulation result cannot accidentally be reported as equivalent to the
training pipeline or to a physical neuromorphic sensor.

The proxy compares two RGB frames in log-luminance space.  Its output is an
RGB uint8 image with positive event counts in red, recent activity in green,
and negative event counts in blue.  State is bounded to one previous frame.
"""

from __future__ import annotations

from dataclasses import dataclass
import math

import numpy as np
from numpy.typing import ArrayLike, NDArray


PROXY_EVENT_ENCODING = "proxy_event_stack_v1"
"""Stable label embedded in observations produced from this approximation."""


@dataclass(frozen=True, slots=True)
class ProxyEventProjectorConfig:
    """Configuration for :func:`project_proxy_event_stack`.

    ``contrast_threshold`` is a log-luminance increment per proxy event.
    ``count_clip`` bounds the event count represented by one output channel.
    ``activity_decay_seconds`` controls the green recency channel when a real
    inter-frame interval is supplied.
    """

    contrast_threshold: float = 0.08
    count_clip: int = 8
    activity_decay_seconds: float = 0.033
    luminance_epsilon: float = 1.0 / 255.0

    def __post_init__(self) -> None:
        if not math.isfinite(self.contrast_threshold) or self.contrast_threshold <= 0.0:
            raise ValueError("contrast_threshold must be finite and > 0")
        if not isinstance(self.count_clip, int) or self.count_clip < 1 or self.count_clip > 255:
            raise ValueError("count_clip must be an integer in [1, 255]")
        if (
            not math.isfinite(self.activity_decay_seconds)
            or self.activity_decay_seconds <= 0.0
        ):
            raise ValueError("activity_decay_seconds must be finite and > 0")
        if not math.isfinite(self.luminance_epsilon) or self.luminance_epsilon <= 0.0:
            raise ValueError("luminance_epsilon must be finite and > 0")


def _normalized_rgb(frame: ArrayLike, name: str) -> NDArray[np.float32]:
    value = np.asarray(frame)
    if value.ndim != 3 or value.shape[2] not in (3, 4):
        raise ValueError(f"{name} must have shape (height, width, 3 or 4)")
    if value.shape[0] < 1 or value.shape[1] < 1:
        raise ValueError(f"{name} must not be empty")

    if np.issubdtype(value.dtype, np.integer):
        max_value = np.iinfo(value.dtype).max
        rgb = value[..., :3].astype(np.float32) / float(max_value)
    elif np.issubdtype(value.dtype, np.floating):
        rgb = value[..., :3].astype(np.float32, copy=False)
        if not np.isfinite(rgb).all():
            raise ValueError(f"{name} must contain only finite values")
        minimum = float(rgb.min())
        maximum = float(rgb.max())
        if minimum < 0.0 or maximum > 1.0:
            raise ValueError(f"floating-point {name} values must be in [0, 1]")
    else:
        raise TypeError(f"{name} must use an integer or floating-point dtype")
    return np.clip(rgb, 0.0, 1.0)


def _log_luminance(frame: ArrayLike, name: str, epsilon: float) -> NDArray[np.float32]:
    rgb = _normalized_rgb(frame, name)
    # ITU-R BT.709 coefficients are sufficient for this simulation proxy.
    luminance = (
        0.2126 * rgb[..., 0]
        + 0.7152 * rgb[..., 1]
        + 0.0722 * rgb[..., 2]
    )
    return np.log(luminance + np.float32(epsilon)).astype(np.float32, copy=False)


def project_proxy_event_stack(
    previous_rgb: ArrayLike,
    current_rgb: ArrayLike,
    *,
    delta_seconds: float = 1.0 / 30.0,
    config: ProxyEventProjectorConfig | None = None,
) -> NDArray[np.uint8]:
    """Project two RGB frames into a labeled three-channel event approximation.

    Channel order is RGB: positive count, recent activity, negative count.
    Counts estimate how many contrast thresholds were crossed and are clipped
    to a fixed bound.  The green channel is non-zero only where an event was
    estimated, with exponential decay based on the supplied frame interval.

    This is a deterministic simulation aid, not a calibrated sensor model and
    not the supplied checkpoint's unknown FRED custom preprocessing.
    """

    settings = config or ProxyEventProjectorConfig()
    if not math.isfinite(delta_seconds) or delta_seconds < 0.0:
        raise ValueError("delta_seconds must be finite and >= 0")

    previous_log = _log_luminance(
        previous_rgb, "previous_rgb", settings.luminance_epsilon
    )
    current_log = _log_luminance(current_rgb, "current_rgb", settings.luminance_epsilon)
    if previous_log.shape != current_log.shape:
        raise ValueError("previous_rgb and current_rgb must have identical dimensions")

    change = current_log - previous_log
    positive = np.floor(np.maximum(change, 0.0) / settings.contrast_threshold)
    negative = np.floor(np.maximum(-change, 0.0) / settings.contrast_threshold)
    positive = np.clip(positive, 0, settings.count_clip)
    negative = np.clip(negative, 0, settings.count_clip)

    count_scale = 255.0 / float(settings.count_clip)
    red = np.rint(positive * count_scale).astype(np.uint8)
    blue = np.rint(negative * count_scale).astype(np.uint8)
    active = np.logical_or(positive > 0, negative > 0)
    recency = 255.0 * math.exp(-delta_seconds / settings.activity_decay_seconds)
    green = np.where(active, np.rint(recency), 0.0).astype(np.uint8)
    return np.stack((red, green, blue), axis=-1)


class ProxyEventProjector:
    """One-frame-state wrapper for a live RGB stream.

    The first call seeds the previous frame and returns ``None``.  Every later
    call returns one ``proxy_event_stack_v1`` image.  No unbounded history or
    frame queue is retained.
    """

    encoding = PROXY_EVENT_ENCODING
    equivalent_to_training_preprocessing = False

    def __init__(self, config: ProxyEventProjectorConfig | None = None) -> None:
        self.config = config or ProxyEventProjectorConfig()
        self._previous: NDArray[np.generic] | None = None
        self._previous_timestamp_seconds: float | None = None

    @property
    def has_previous_frame(self) -> bool:
        return self._previous is not None

    def reset(self) -> None:
        self._previous = None
        self._previous_timestamp_seconds = None

    def push(
        self, frame_rgb: ArrayLike, *, timestamp_seconds: float
    ) -> NDArray[np.uint8] | None:
        if not math.isfinite(timestamp_seconds):
            raise ValueError("timestamp_seconds must be finite")
        # Validate before retaining a copy, keeping the same accepted formats as
        # the stateless function while preserving the original numeric scale.
        _normalized_rgb(frame_rgb, "frame_rgb")
        current = np.array(frame_rgb, copy=True)
        if self._previous is None:
            self._previous = current
            self._previous_timestamp_seconds = float(timestamp_seconds)
            return None

        assert self._previous_timestamp_seconds is not None
        delta_seconds = float(timestamp_seconds) - self._previous_timestamp_seconds
        if delta_seconds < 0.0:
            raise ValueError("timestamp_seconds must be monotonic")
        result = project_proxy_event_stack(
            self._previous,
            current,
            delta_seconds=delta_seconds,
            config=self.config,
        )
        self._previous = current
        self._previous_timestamp_seconds = float(timestamp_seconds)
        return result
