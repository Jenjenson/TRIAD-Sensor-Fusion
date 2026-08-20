"""Deterministic placeholder IQ and PSD generation for RF adapter testing."""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime, timezone
import math

import numpy as np
from numpy.typing import NDArray

from .rf import dbm_to_mw

BAND_2_4_GHZ = 2.4e9
BAND_5_8_GHZ = 5.8e9
SUPPORTED_PLACEHOLDER_CENTERS_HZ = (BAND_2_4_GHZ, BAND_5_8_GHZ)
PLACEHOLDER_PREPROCESSING = "synthetic_iq_placeholder_not_sirfnet_preprocessing_v1"
PLACEHOLDER_PSD_METHOD = "welch_hann_placeholder_256_bin_v1"


def _positive(name: str, value: float) -> float:
    result = float(value)
    if not math.isfinite(result) or result <= 0.0:
        raise ValueError(f"{name} must be finite and > 0")
    return result


def _utc_timestamp(value: datetime | None) -> datetime:
    timestamp = datetime.now(timezone.utc) if value is None else value
    if timestamp.tzinfo is None or timestamp.utcoffset() is None:
        raise ValueError("timestamp must be timezone-aware")
    return timestamp.astimezone(timezone.utc)


@dataclass(frozen=True, slots=True)
class IQFrame:
    timestamp: datetime
    center_frequency_hz: float
    sample_rate_hz: float
    samples: NDArray[np.complex64]
    preprocessing: str = PLACEHOLDER_PREPROCESSING
    amplitude_units: str = "sqrt_mW"

    def __post_init__(self) -> None:
        object.__setattr__(self, "timestamp", _utc_timestamp(self.timestamp))
        object.__setattr__(
            self, "center_frequency_hz", _positive("center_frequency_hz", self.center_frequency_hz)
        )
        object.__setattr__(self, "sample_rate_hz", _positive("sample_rate_hz", self.sample_rate_hz))
        samples = np.asarray(self.samples, dtype=np.complex64)
        if samples.ndim != 1 or samples.size == 0:
            raise ValueError("samples must be a non-empty one-dimensional complex array")
        if not np.all(np.isfinite(samples.real)) or not np.all(np.isfinite(samples.imag)):
            raise ValueError("samples must be finite")
        immutable_samples = np.array(samples, dtype=np.complex64, copy=True)
        immutable_samples.setflags(write=False)
        object.__setattr__(self, "samples", immutable_samples)


@dataclass(frozen=True, slots=True)
class PSDFrame:
    timestamp: datetime
    center_frequency_hz: float
    frequency_hz: NDArray[np.float64]
    power_dbm_per_hz: NDArray[np.float64]
    method: str = PLACEHOLDER_PSD_METHOD

    def __post_init__(self) -> None:
        object.__setattr__(self, "timestamp", _utc_timestamp(self.timestamp))
        object.__setattr__(
            self, "center_frequency_hz", _positive("center_frequency_hz", self.center_frequency_hz)
        )
        frequencies = np.asarray(self.frequency_hz, dtype=np.float64)
        powers = np.asarray(self.power_dbm_per_hz, dtype=np.float64)
        if frequencies.ndim != 1 or powers.ndim != 1 or frequencies.shape != powers.shape:
            raise ValueError("frequency_hz and power_dbm_per_hz must be equal one-dimensional arrays")
        if frequencies.size == 0 or not np.all(np.isfinite(frequencies)):
            raise ValueError("frequency_hz must be non-empty and finite")
        if np.any(np.isnan(powers)) or np.any(np.isposinf(powers)):
            raise ValueError("power_dbm_per_hz must not contain NaN or positive infinity")
        frequency_copy = np.array(frequencies, copy=True)
        power_copy = np.array(powers, copy=True)
        frequency_copy.setflags(write=False)
        power_copy.setflags(write=False)
        object.__setattr__(self, "frequency_hz", frequency_copy)
        object.__setattr__(self, "power_dbm_per_hz", power_copy)


def _normalize_complex_power(samples: NDArray[np.complex128], target_power_mw: float) -> None:
    measured = float(np.mean(np.abs(samples) ** 2))
    if measured <= 0.0:
        raise RuntimeError("cannot normalize a zero-power generated signal")
    samples *= math.sqrt(target_power_mw / measured)


def generate_synthetic_iq(
    *,
    center_frequency_hz: float,
    sample_rate_hz: float,
    num_samples: int,
    signal_power_dbm: float | None,
    noise_power_dbm: float,
    signal_offset_hz: float = 0.0,
    symbol_rate_hz: float = 1e6,
    waveform: str = "qpsk",
    seed: int = 0,
    timestamp: datetime | None = None,
) -> IQFrame:
    """Create deterministic baseband IQ in sqrt(mW) units.

    This generator supports adapter/integration tests at 2.4 and 5.8 GHz. It is
    not the recovered preprocessing pipeline for any uploaded RF model.
    """

    center = _positive("center_frequency_hz", center_frequency_hz)
    sample_rate = _positive("sample_rate_hz", sample_rate_hz)
    symbol_rate = _positive("symbol_rate_hz", symbol_rate_hz)
    if isinstance(num_samples, bool) or not isinstance(num_samples, int) or num_samples <= 0:
        raise ValueError("num_samples must be a positive integer")
    if not math.isfinite(float(signal_offset_hz)):
        raise ValueError("signal_offset_hz must be finite")
    if abs(float(signal_offset_hz)) >= sample_rate / 2.0:
        raise ValueError("signal_offset_hz must be inside the sampled Nyquist band")
    if not isinstance(seed, int) or isinstance(seed, bool):
        raise TypeError("seed must be an integer")
    if waveform not in {"qpsk", "tone"}:
        raise ValueError("waveform must be 'qpsk' or 'tone'")

    noise_mw = dbm_to_mw(noise_power_dbm)
    if not math.isfinite(noise_mw) or noise_mw <= 0.0:
        raise ValueError("noise_power_dbm must represent finite positive power")
    generator = np.random.default_rng(seed)
    noise = generator.standard_normal(num_samples) + 1j * generator.standard_normal(num_samples)
    noise = noise.astype(np.complex128, copy=False)
    _normalize_complex_power(noise, noise_mw)

    combined = noise
    if signal_power_dbm is not None:
        signal_mw = dbm_to_mw(signal_power_dbm)
        if not math.isfinite(signal_mw) or signal_mw <= 0.0:
            raise ValueError("signal_power_dbm must represent finite positive power")
        if waveform == "tone":
            signal = np.ones(num_samples, dtype=np.complex128)
        else:
            samples_per_symbol = max(1, int(round(sample_rate / symbol_rate)))
            symbol_count = math.ceil(num_samples / samples_per_symbol)
            indices = generator.integers(0, 4, size=symbol_count)
            constellation = np.exp(1j * (math.pi / 4.0 + indices * math.pi / 2.0))
            signal = np.repeat(constellation, samples_per_symbol)[:num_samples]
        sample_indices = np.arange(num_samples, dtype=np.float64)
        signal = signal * np.exp(
            2j * math.pi * float(signal_offset_hz) * sample_indices / sample_rate
        )
        _normalize_complex_power(signal, signal_mw)
        combined = signal + noise

    return IQFrame(
        timestamp=_utc_timestamp(timestamp),
        center_frequency_hz=center,
        sample_rate_hz=sample_rate,
        samples=np.asarray(combined, dtype=np.complex64),
    )


def welch_psd(frame: IQFrame, *, bins: int = 256, overlap_fraction: float = 0.5) -> PSDFrame:
    """Return a centered Welch PSD in dBm/Hz, normally with 256 bins."""

    if not isinstance(frame, IQFrame):
        raise TypeError("frame must be IQFrame")
    if isinstance(bins, bool) or not isinstance(bins, int) or bins < 2:
        raise ValueError("bins must be an integer >= 2")
    if frame.samples.size < bins:
        raise ValueError("IQ frame must contain at least bins samples")
    overlap = float(overlap_fraction)
    if not math.isfinite(overlap) or not 0.0 <= overlap < 1.0:
        raise ValueError("overlap_fraction must be in [0, 1)")
    hop = max(1, int(round(bins * (1.0 - overlap))))
    starts = range(0, frame.samples.size - bins + 1, hop)
    window = np.hanning(bins)
    normalization = frame.sample_rate_hz * float(np.sum(window**2))
    accumulated = np.zeros(bins, dtype=np.float64)
    segment_count = 0
    for start in starts:
        segment = frame.samples[start : start + bins].astype(np.complex128) * window
        spectrum = np.fft.fft(segment, n=bins)
        accumulated += np.abs(spectrum) ** 2 / normalization
        segment_count += 1
    if segment_count == 0:
        raise RuntimeError("no Welch segments were generated")
    psd_mw_per_hz = np.fft.fftshift(accumulated / segment_count)
    frequency_offsets = np.fft.fftshift(np.fft.fftfreq(bins, d=1.0 / frame.sample_rate_hz))
    frequencies = frame.center_frequency_hz + frequency_offsets
    with np.errstate(divide="ignore"):
        power_dbm_per_hz = 10.0 * np.log10(psd_mw_per_hz)
    return PSDFrame(
        timestamp=frame.timestamp,
        center_frequency_hz=frame.center_frequency_hz,
        frequency_hz=frequencies,
        power_dbm_per_hz=power_dbm_per_hz,
    )
