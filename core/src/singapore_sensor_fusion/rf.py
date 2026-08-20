"""RF link-budget primitives and an explicitly analytic detector fallback."""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import math
from statistics import NormalDist
from typing import Callable

SPEED_OF_LIGHT_M_S = 299_792_458.0
BOLTZMANN_J_K = 1.380_649e-23
REFERENCE_TEMPERATURE_K = 290.0
ANALYTIC_FALLBACK_METHOD = "analytic_gaussian_energy_detector_fallback_v1"


def _finite(name: str, value: float) -> float:
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"{name} must be finite")
    return result


def _positive(name: str, value: float) -> float:
    result = _finite(name, value)
    if result <= 0.0:
        raise ValueError(f"{name} must be > 0")
    return result


def dbm_to_mw(power_dbm: float) -> float:
    """Convert dBm to milliwatts."""

    value = float(power_dbm)
    if math.isnan(value):
        raise ValueError("power_dbm must not be NaN")
    if value == -math.inf:
        return 0.0
    if value == math.inf:
        return math.inf
    return 10.0 ** (value / 10.0)


def mw_to_dbm(power_mw: float) -> float:
    """Convert milliwatts to dBm; zero power maps to negative infinity."""

    value = float(power_mw)
    if math.isnan(value) or value < 0.0:
        raise ValueError("power_mw must be >= 0 and not NaN")
    if value == 0.0:
        return -math.inf
    return 10.0 * math.log10(value)


def sum_powers_dbm(powers_dbm: list[float] | tuple[float, ...]) -> float:
    """Sum simultaneous uncorrelated powers in linear units, returning dBm."""

    total_mw = math.fsum(dbm_to_mw(power) for power in powers_dbm)
    return mw_to_dbm(total_mw)


def fspl_db(distance_m: float, frequency_hz: float) -> float:
    """Free-space path loss using exact SI units and the defined speed of light."""

    distance = _positive("distance_m", distance_m)
    frequency = _positive("frequency_hz", frequency_hz)
    return 20.0 * math.log10(4.0 * math.pi * distance * frequency / SPEED_OF_LIGHT_M_S)


def log_distance_path_loss_db(
    distance_m: float,
    frequency_hz: float,
    *,
    path_loss_exponent: float = 2.0,
    reference_distance_m: float = 1.0,
    reference_loss_db: float | None = None,
) -> float:
    """Log-distance loss referenced to FSPL (or a supplied calibrated loss)."""

    distance = _positive("distance_m", distance_m)
    frequency = _positive("frequency_hz", frequency_hz)
    exponent = _positive("path_loss_exponent", path_loss_exponent)
    reference_distance = _positive("reference_distance_m", reference_distance_m)
    reference_loss = (
        fspl_db(reference_distance, frequency)
        if reference_loss_db is None
        else _finite("reference_loss_db", reference_loss_db)
    )
    return reference_loss + 10.0 * exponent * math.log10(distance / reference_distance)


def thermal_noise_dbm(
    bandwidth_hz: float,
    *,
    temperature_k: float = REFERENCE_TEMPERATURE_K,
    noise_figure_db: float = 0.0,
) -> float:
    """Receiver noise power from ``k*T*B``, including receiver noise figure."""

    bandwidth = _positive("bandwidth_hz", bandwidth_hz)
    temperature = _positive("temperature_k", temperature_k)
    noise_figure = _finite("noise_figure_db", noise_figure_db)
    thermal_mw = BOLTZMANN_J_K * temperature * bandwidth * 1_000.0
    return mw_to_dbm(thermal_mw) + noise_figure


@dataclass(frozen=True, slots=True)
class LinkContext:
    """Geometry and identity passed to propagation loss hooks."""

    transmitter_id: str
    receiver_id: str
    distance_m: float
    frequency_hz: float
    timestamp_s: float
    line_of_sight: bool = True

    def __post_init__(self) -> None:
        if not isinstance(self.transmitter_id, str) or not self.transmitter_id.strip():
            raise ValueError("transmitter_id must be non-empty")
        if not isinstance(self.receiver_id, str) or not self.receiver_id.strip():
            raise ValueError("receiver_id must be non-empty")
        object.__setattr__(self, "distance_m", _positive("distance_m", self.distance_m))
        object.__setattr__(self, "frequency_hz", _positive("frequency_hz", self.frequency_hz))
        object.__setattr__(self, "timestamp_s", _finite("timestamp_s", self.timestamp_s))
        if not isinstance(self.line_of_sight, bool):
            raise TypeError("line_of_sight must be bool")


AdditionalLossHook = Callable[[LinkContext], float]


def fixed_los_nlos_loss_hook(
    *, los_excess_loss_db: float = 0.0, nlos_excess_loss_db: float = 18.0
) -> AdditionalLossHook:
    """Build a basic hook; replace it with an Unreal ray/terrain-derived hook."""

    los_loss = _finite("los_excess_loss_db", los_excess_loss_db)
    nlos_loss = _finite("nlos_excess_loss_db", nlos_excess_loss_db)
    if los_loss < 0.0 or nlos_loss < 0.0:
        raise ValueError("excess losses must be >= 0")

    def hook(context: LinkContext) -> float:
        return los_loss if context.line_of_sight else nlos_loss

    return hook


@dataclass(frozen=True, slots=True)
class DeterministicShadowing:
    """Reproducible, locally correlated zero-mean link shadowing.

    Independent Gaussian anchor values are derived from a stable hash of seed,
    unordered link identity, and time bucket. Smooth interpolation makes nearby
    timestamps correlated and continuous without relying on call order.
    """

    seed: int | str = 0
    sigma_db: float = 4.0
    coherence_time_s: float = 2.0

    def __post_init__(self) -> None:
        sigma = _finite("sigma_db", self.sigma_db)
        if sigma < 0.0:
            raise ValueError("sigma_db must be >= 0")
        object.__setattr__(self, "sigma_db", sigma)
        object.__setattr__(
            self,
            "coherence_time_s",
            _positive("coherence_time_s", self.coherence_time_s),
        )

    def _anchor(self, transmitter_id: str, receiver_id: str, bucket: int) -> float:
        endpoint_a, endpoint_b = sorted((transmitter_id, receiver_id))
        payload = f"{self.seed!s}\0{endpoint_a}\0{endpoint_b}\0{bucket}".encode("utf-8")
        digest = hashlib.blake2b(payload, digest_size=16, person=b"sg-rf-shadow-v1").digest()
        first = int.from_bytes(digest[:8], "big")
        second = int.from_bytes(digest[8:], "big")
        denominator = float(1 << 64)
        uniform_1 = (first + 0.5) / denominator
        uniform_2 = (second + 0.5) / denominator
        return math.sqrt(-2.0 * math.log(uniform_1)) * math.cos(2.0 * math.pi * uniform_2)

    def sample_db(self, transmitter_id: str, receiver_id: str, timestamp_s: float) -> float:
        """Return signed excess loss in dB; negative values are constructive."""

        timestamp = _finite("timestamp_s", timestamp_s)
        bucket_position = timestamp / self.coherence_time_s
        bucket = math.floor(bucket_position)
        fraction = bucket_position - bucket
        # Smoothstep avoids jumps while stable hash anchors preserve reproducibility.
        blend = fraction * fraction * (3.0 - 2.0 * fraction)
        left = self._anchor(transmitter_id, receiver_id, bucket)
        right = self._anchor(transmitter_id, receiver_id, bucket + 1)
        return self.sigma_db * ((1.0 - blend) * left + blend * right)

    def for_link(self, context: LinkContext) -> float:
        return self.sample_db(
            context.transmitter_id,
            context.receiver_id,
            context.timestamp_s,
        )


def received_power_dbm(
    transmit_power_dbm: float,
    context: LinkContext,
    *,
    transmit_gain_dbi: float = 0.0,
    receive_gain_dbi: float = 0.0,
    path_loss_db: float | None = None,
    additional_loss_hook: AdditionalLossHook | None = None,
    shadowing: DeterministicShadowing | None = None,
    system_loss_db: float = 0.0,
) -> float:
    """Compute a receive link budget with optional scene and shadowing hooks."""

    transmit_power = _finite("transmit_power_dbm", transmit_power_dbm)
    tx_gain = _finite("transmit_gain_dbi", transmit_gain_dbi)
    rx_gain = _finite("receive_gain_dbi", receive_gain_dbi)
    system_loss = _finite("system_loss_db", system_loss_db)
    if system_loss < 0.0:
        raise ValueError("system_loss_db must be >= 0")
    propagation_loss = (
        fspl_db(context.distance_m, context.frequency_hz)
        if path_loss_db is None
        else _finite("path_loss_db", path_loss_db)
    )
    scene_loss = 0.0
    if additional_loss_hook is not None:
        scene_loss = _finite("additional loss hook result", additional_loss_hook(context))
        if scene_loss < 0.0:
            raise ValueError("additional loss hook must return a non-negative loss")
    shadow_loss = 0.0 if shadowing is None else shadowing.for_link(context)
    return (
        transmit_power
        + tx_gain
        + rx_gain
        - propagation_loss
        - scene_loss
        - shadow_loss
        - system_loss
    )


@dataclass(frozen=True, slots=True)
class AnalyticDetectionEstimate:
    """Result from the analytic fallback, never a learned-model probability."""

    probability_detection: float
    false_alarm_probability: float
    normalized_energy_threshold: float
    calibrated_snr_db: float
    num_complex_samples: int
    method: str = ANALYTIC_FALLBACK_METHOD
    is_learned_model_score: bool = False


def analytic_energy_detector_probability(
    snr_db: float,
    num_complex_samples: int,
    *,
    false_alarm_probability: float = 1e-3,
    snr_calibration_offset_db: float = 0.0,
) -> AnalyticDetectionEstimate:
    """Estimate energy-detector Pd with a Gaussian/CLT approximation.

    The normalized threshold is analytically calibrated to the requested Pfa.
    ``snr_calibration_offset_db`` is an explicit lab/simulation correction, not
    learned calibration. This function is a transparent fallback until the
    exact RF model preprocessing and validated score calibration are available.
    """

    snr = _finite("snr_db", snr_db)
    offset = _finite("snr_calibration_offset_db", snr_calibration_offset_db)
    if isinstance(num_complex_samples, bool) or not isinstance(num_complex_samples, int):
        raise TypeError("num_complex_samples must be an integer")
    if num_complex_samples <= 0:
        raise ValueError("num_complex_samples must be > 0")
    pfa = _finite("false_alarm_probability", false_alarm_probability)
    if not 0.0 < pfa < 1.0:
        raise ValueError("false_alarm_probability must be in (0, 1)")

    calibrated_snr_db = snr + offset
    snr_linear = 10.0 ** (calibrated_snr_db / 10.0)
    sample_count = float(num_complex_samples)
    standard_normal = NormalDist()
    threshold = sample_count + math.sqrt(sample_count) * standard_normal.inv_cdf(1.0 - pfa)
    alternative_mean = sample_count * (1.0 + snr_linear)
    alternative_stddev = math.sqrt(sample_count * (1.0 + 2.0 * snr_linear))
    standardized_threshold = (threshold - alternative_mean) / alternative_stddev
    probability_detection = 1.0 - standard_normal.cdf(standardized_threshold)
    probability_detection = min(1.0, max(0.0, probability_detection))
    return AnalyticDetectionEstimate(
        probability_detection=probability_detection,
        false_alarm_probability=pfa,
        normalized_energy_threshold=threshold,
        calibrated_snr_db=calibrated_snr_db,
        num_complex_samples=num_complex_samples,
    )
