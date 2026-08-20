"""Auditable fixed-site FMCW mmWave radar simulation primitives.

The implementation uses the monostatic radar equation, an explicit coherent
processing gain, bounded weather attenuation, and a deterministic SNR gate.  It
is intended for range/velocity confirmation in simulation, not as a hardware
performance claim or a missile fire-control model.
"""

from __future__ import annotations

from dataclasses import dataclass
import math

from .rf import SPEED_OF_LIGHT_M_S, thermal_noise_dbm


MMWAVE_METHOD = "fmcw_77ghz_range_doppler_cfar_simulation_v1"


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


def _db_to_linear(value_db: float) -> float:
    return 10.0 ** (_finite("value_db", value_db) / 10.0)


@dataclass(frozen=True, slots=True)
class MmWaveRadar:
    radar_id: str
    center_frequency_hz: float = 77e9
    sweep_bandwidth_hz: float = 150e6
    transmit_power_dbm: float = 22.0
    transmit_gain_dbi: float = 33.0
    receive_gain_dbi: float = 33.0
    receiver_noise_bandwidth_hz: float = 1_000_000.0
    noise_figure_db: float = 10.0
    system_loss_db: float = 10.0
    coherent_processing_gain_db: float = 35.0
    minimum_snr_db: float = 10.0
    maximum_range_m: float = 1_500.0
    update_period_ms: float = 50.0
    processing_latency_ms: float = 12.0
    network_latency_ms: float = 12.0

    def __post_init__(self) -> None:
        if not isinstance(self.radar_id, str) or not self.radar_id.strip():
            raise ValueError("radar_id must be non-empty")
        for name in (
            "center_frequency_hz",
            "sweep_bandwidth_hz",
            "receiver_noise_bandwidth_hz",
            "maximum_range_m",
            "update_period_ms",
            "processing_latency_ms",
            "network_latency_ms",
        ):
            object.__setattr__(self, name, _positive(name, getattr(self, name)))
        for name in (
            "transmit_power_dbm",
            "transmit_gain_dbi",
            "receive_gain_dbi",
            "noise_figure_db",
            "system_loss_db",
            "coherent_processing_gain_db",
            "minimum_snr_db",
        ):
            object.__setattr__(self, name, _finite(name, getattr(self, name)))

    @property
    def range_resolution_m(self) -> float:
        return SPEED_OF_LIGHT_M_S / (2.0 * self.sweep_bandwidth_hz)

    @property
    def end_to_end_latency_ms(self) -> float:
        return self.update_period_ms + self.processing_latency_ms + self.network_latency_ms


@dataclass(frozen=True, slots=True)
class MmWaveDetection:
    target_id: str
    radar_id: str
    range_m: float
    azimuth_deg: float
    elevation_deg: float
    radial_velocity_m_s: float
    radar_cross_section_m2: float
    received_power_dbm: float
    noise_floor_dbm: float
    snr_db: float
    probability_detection_index: float
    rain_loss_db: float
    detected: bool
    range_resolution_m: float
    end_to_end_latency_ms: float
    method: str = MMWAVE_METHOD

    def to_dict(self) -> dict[str, object]:
        return {
            "targetId": self.target_id,
            "radarId": self.radar_id,
            "rangeMeters": round(self.range_m, 3),
            "azimuthDegrees": round(self.azimuth_deg, 3),
            "elevationDegrees": round(self.elevation_deg, 3),
            "radialVelocityMetersPerSecond": round(self.radial_velocity_m_s, 3),
            "closingVelocityMetersPerSecond": round(-self.radial_velocity_m_s, 3),
            "radarCrossSectionSquareMeters": round(self.radar_cross_section_m2, 6),
            "receivedPowerDbm": round(self.received_power_dbm, 3),
            "noiseFloorDbm": round(self.noise_floor_dbm, 3),
            "snrDb": round(self.snr_db, 3),
            "probabilityDetectionIndex": round(self.probability_detection_index, 6),
            "rainLossDb": round(self.rain_loss_db, 4),
            "detected": self.detected,
            "rangeResolutionMeters": round(self.range_resolution_m, 4),
            "endToEndLatencyMilliseconds": round(self.end_to_end_latency_ms, 3),
            "method": self.method,
            "scoreSemantics": "deterministic SNR-to-index mapping; not a calibrated probability",
            "physicalPerformanceClaimed": False,
        }


def mmwave_rain_specific_attenuation_db_per_km(rain_rate_mm_h: float) -> float:
    """Representative 77 GHz horizontal-polarization rain stress term.

    ``1.132 * R**0.7177`` follows a published P.838-compatible 77 GHz
    measurement fit.  It remains a declared simulation assumption because the
    current scene does not model polarization or spatially varying rain cells.
    """

    rain_rate = max(0.0, _finite("rain_rate_mm_h", rain_rate_mm_h))
    return min(35.0, 1.132 * rain_rate**0.7177)


def monostatic_received_power_dbm(
    radar: MmWaveRadar,
    *,
    range_m: float,
    radar_cross_section_m2: float,
    rain_rate_mm_h: float = 0.0,
) -> tuple[float, float]:
    """Return received power and two-way rain loss for a monostatic radar."""

    distance = _positive("range_m", range_m)
    rcs = _positive("radar_cross_section_m2", radar_cross_section_m2)
    wavelength = SPEED_OF_LIGHT_M_S / radar.center_frequency_hz
    transmit_w = 10.0 ** ((radar.transmit_power_dbm - 30.0) / 10.0)
    tx_gain = _db_to_linear(radar.transmit_gain_dbi)
    rx_gain = _db_to_linear(radar.receive_gain_dbi)
    system_loss = _db_to_linear(radar.system_loss_db)
    rain_loss_db = (
        2.0
        * mmwave_rain_specific_attenuation_db_per_km(rain_rate_mm_h)
        * (distance / 1000.0)
    )
    rain_loss = _db_to_linear(rain_loss_db)
    received_w = (
        transmit_w
        * tx_gain
        * rx_gain
        * wavelength**2
        * rcs
        / (((4.0 * math.pi) ** 3) * distance**4 * system_loss * rain_loss)
    )
    return 10.0 * math.log10(received_w * 1000.0), rain_loss_db


def simulate_mmwave_detection(
    *,
    target_id: str,
    radar: MmWaveRadar,
    east_m: float,
    north_m: float,
    up_m: float,
    target_speed_m_s: float,
    target_heading_deg: float,
    radar_cross_section_m2: float,
    rain_rate_mm_h: float = 0.0,
    line_of_sight: bool = True,
) -> MmWaveDetection:
    """Simulate one range-Doppler cell for a target in radar-local ENU."""

    east = _finite("east_m", east_m)
    north = _finite("north_m", north_m)
    up = _finite("up_m", up_m)
    distance = math.sqrt(east**2 + north**2 + up**2)
    if distance <= 0.0:
        raise ValueError("target and radar positions must differ")
    horizontal = math.hypot(east, north)
    azimuth = math.degrees(math.atan2(east, north)) % 360.0
    elevation = math.degrees(math.atan2(up, horizontal))
    speed = max(0.0, _finite("target_speed_m_s", target_speed_m_s))
    heading = math.radians(_finite("target_heading_deg", target_heading_deg))
    velocity_east = speed * math.sin(heading)
    velocity_north = speed * math.cos(heading)
    radial_velocity = (velocity_east * east + velocity_north * north) / distance

    power, rain_loss = monostatic_received_power_dbm(
        radar,
        range_m=distance,
        radar_cross_section_m2=radar_cross_section_m2,
        rain_rate_mm_h=rain_rate_mm_h,
    )
    if not line_of_sight:
        power -= 35.0
    noise = thermal_noise_dbm(
        radar.receiver_noise_bandwidth_hz,
        noise_figure_db=radar.noise_figure_db,
    )
    snr = power - noise + radar.coherent_processing_gain_db
    # A smooth, stable evidence index around the CFAR SNR threshold.  It is not
    # labeled as a calibrated probability.
    sigmoid_argument = 0.55 * (snr - radar.minimum_snr_db)
    if sigmoid_argument >= 0.0:
        probability_index = 1.0 / (1.0 + math.exp(-sigmoid_argument))
    else:
        exponential = math.exp(sigmoid_argument)
        probability_index = exponential / (1.0 + exponential)
    detected = (
        line_of_sight
        and distance <= radar.maximum_range_m
        and snr >= radar.minimum_snr_db
    )
    return MmWaveDetection(
        target_id=target_id,
        radar_id=radar.radar_id,
        range_m=distance,
        azimuth_deg=azimuth,
        elevation_deg=elevation,
        radial_velocity_m_s=radial_velocity,
        radar_cross_section_m2=radar_cross_section_m2,
        received_power_dbm=power,
        noise_floor_dbm=noise,
        snr_db=snr,
        probability_detection_index=probability_index,
        rain_loss_db=rain_loss,
        detected=detected,
        range_resolution_m=radar.range_resolution_m,
        end_to_end_latency_ms=radar.end_to_end_latency_ms,
    )
