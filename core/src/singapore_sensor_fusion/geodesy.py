"""WGS84 geodetic, Earth-centered, and local tangent-plane transforms."""

from __future__ import annotations

from dataclasses import dataclass
import math

WGS84_A_M = 6_378_137.0
WGS84_F = 1.0 / 298.257_223_563
WGS84_B_M = WGS84_A_M * (1.0 - WGS84_F)
WGS84_E2 = WGS84_F * (2.0 - WGS84_F)
WGS84_EP2 = (WGS84_A_M**2 - WGS84_B_M**2) / WGS84_B_M**2
MEAN_EARTH_RADIUS_M = 6_371_008.8


def _finite(name: str, value: float) -> float:
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"{name} must be finite")
    return result


@dataclass(frozen=True, slots=True)
class Geodetic:
    latitude_deg: float
    longitude_deg: float
    altitude_m: float = 0.0

    def __post_init__(self) -> None:
        latitude = _finite("latitude_deg", self.latitude_deg)
        longitude = _finite("longitude_deg", self.longitude_deg)
        altitude = _finite("altitude_m", self.altitude_m)
        if not -90.0 <= latitude <= 90.0:
            raise ValueError("latitude_deg must be in [-90, 90]")
        if not -180.0 <= longitude <= 180.0:
            raise ValueError("longitude_deg must be in [-180, 180]")
        object.__setattr__(self, "latitude_deg", latitude)
        object.__setattr__(self, "longitude_deg", longitude)
        object.__setattr__(self, "altitude_m", altitude)


@dataclass(frozen=True, slots=True)
class ECEF:
    x_m: float
    y_m: float
    z_m: float

    def __post_init__(self) -> None:
        object.__setattr__(self, "x_m", _finite("x_m", self.x_m))
        object.__setattr__(self, "y_m", _finite("y_m", self.y_m))
        object.__setattr__(self, "z_m", _finite("z_m", self.z_m))


@dataclass(frozen=True, slots=True)
class ENU:
    east_m: float
    north_m: float
    up_m: float

    def __post_init__(self) -> None:
        object.__setattr__(self, "east_m", _finite("east_m", self.east_m))
        object.__setattr__(self, "north_m", _finite("north_m", self.north_m))
        object.__setattr__(self, "up_m", _finite("up_m", self.up_m))


def geodetic_to_ecef(point: Geodetic) -> ECEF:
    """Convert WGS84 latitude/longitude/ellipsoidal altitude to ECEF metres."""

    if not isinstance(point, Geodetic):
        raise TypeError("point must be Geodetic")
    latitude = math.radians(point.latitude_deg)
    longitude = math.radians(point.longitude_deg)
    sin_latitude = math.sin(latitude)
    cos_latitude = math.cos(latitude)
    radius = WGS84_A_M / math.sqrt(1.0 - WGS84_E2 * sin_latitude**2)
    return ECEF(
        x_m=(radius + point.altitude_m) * cos_latitude * math.cos(longitude),
        y_m=(radius + point.altitude_m) * cos_latitude * math.sin(longitude),
        z_m=(radius * (1.0 - WGS84_E2) + point.altitude_m) * sin_latitude,
    )


def ecef_to_geodetic(point: ECEF) -> Geodetic:
    """Convert ECEF metres to WGS84 coordinates using Bowring's solution."""

    if not isinstance(point, ECEF):
        raise TypeError("point must be ECEF")
    horizontal = math.hypot(point.x_m, point.y_m)
    if horizontal < 1e-9:
        if abs(point.z_m) < 1e-9:
            raise ValueError("Earth-centre ECEF has no unique geodetic coordinate")
        latitude_deg = 90.0 if point.z_m > 0.0 else -90.0
        return Geodetic(latitude_deg, 0.0, abs(point.z_m) - WGS84_B_M)

    longitude = math.atan2(point.y_m, point.x_m)
    theta = math.atan2(point.z_m * WGS84_A_M, horizontal * WGS84_B_M)
    sin_theta = math.sin(theta)
    cos_theta = math.cos(theta)
    latitude = math.atan2(
        point.z_m + WGS84_EP2 * WGS84_B_M * sin_theta**3,
        horizontal - WGS84_E2 * WGS84_A_M * cos_theta**3,
    )
    sin_latitude = math.sin(latitude)
    radius = WGS84_A_M / math.sqrt(1.0 - WGS84_E2 * sin_latitude**2)
    altitude = horizontal / math.cos(latitude) - radius
    return Geodetic(math.degrees(latitude), math.degrees(longitude), altitude)


def ecef_to_enu(point: ECEF, origin: Geodetic) -> ENU:
    """Express an ECEF point in the east/north/up frame at ``origin``."""

    if not isinstance(point, ECEF) or not isinstance(origin, Geodetic):
        raise TypeError("point must be ECEF and origin must be Geodetic")
    base = geodetic_to_ecef(origin)
    dx = point.x_m - base.x_m
    dy = point.y_m - base.y_m
    dz = point.z_m - base.z_m
    latitude = math.radians(origin.latitude_deg)
    longitude = math.radians(origin.longitude_deg)
    sin_latitude, cos_latitude = math.sin(latitude), math.cos(latitude)
    sin_longitude, cos_longitude = math.sin(longitude), math.cos(longitude)
    return ENU(
        east_m=-sin_longitude * dx + cos_longitude * dy,
        north_m=(
            -sin_latitude * cos_longitude * dx
            - sin_latitude * sin_longitude * dy
            + cos_latitude * dz
        ),
        up_m=(
            cos_latitude * cos_longitude * dx
            + cos_latitude * sin_longitude * dy
            + sin_latitude * dz
        ),
    )


def enu_to_ecef(point: ENU, origin: Geodetic) -> ECEF:
    """Convert local east/north/up metres at ``origin`` to ECEF."""

    if not isinstance(point, ENU) or not isinstance(origin, Geodetic):
        raise TypeError("point must be ENU and origin must be Geodetic")
    base = geodetic_to_ecef(origin)
    latitude = math.radians(origin.latitude_deg)
    longitude = math.radians(origin.longitude_deg)
    sin_latitude, cos_latitude = math.sin(latitude), math.cos(latitude)
    sin_longitude, cos_longitude = math.sin(longitude), math.cos(longitude)
    dx = (
        -sin_longitude * point.east_m
        - sin_latitude * cos_longitude * point.north_m
        + cos_latitude * cos_longitude * point.up_m
    )
    dy = (
        cos_longitude * point.east_m
        - sin_latitude * sin_longitude * point.north_m
        + cos_latitude * sin_longitude * point.up_m
    )
    dz = cos_latitude * point.north_m + sin_latitude * point.up_m
    return ECEF(base.x_m + dx, base.y_m + dy, base.z_m + dz)


def geodetic_to_enu(point: Geodetic, origin: Geodetic) -> ENU:
    """Convenience composition from WGS84 geodetic to local ENU."""

    return ecef_to_enu(geodetic_to_ecef(point), origin)


def enu_to_geodetic(point: ENU, origin: Geodetic) -> Geodetic:
    """Convenience composition from local ENU to WGS84 geodetic."""

    return ecef_to_geodetic(enu_to_ecef(point, origin))


def wgs84_surface_distance_m(left: Geodetic, right: Geodetic) -> float:
    """Return the WGS84 ellipsoidal surface distance using Vincenty's inverse.

    The rare antipodal non-convergence case falls back to a mean-Earth-radius
    haversine distance. Altitudes are deliberately ignored.
    """

    if not isinstance(left, Geodetic) or not isinstance(right, Geodetic):
        raise TypeError("left and right must be Geodetic")
    latitude_left = math.radians(left.latitude_deg)
    latitude_right = math.radians(right.latitude_deg)
    longitude_delta = math.radians(right.longitude_deg - left.longitude_deg)
    longitude_delta = (longitude_delta + math.pi) % (2.0 * math.pi) - math.pi
    reduced_left = math.atan((1.0 - WGS84_F) * math.tan(latitude_left))
    reduced_right = math.atan((1.0 - WGS84_F) * math.tan(latitude_right))
    sin_left, cos_left = math.sin(reduced_left), math.cos(reduced_left)
    sin_right, cos_right = math.sin(reduced_right), math.cos(reduced_right)
    lambda_value = longitude_delta

    for _ in range(100):
        sin_lambda = math.sin(lambda_value)
        cos_lambda = math.cos(lambda_value)
        term_left = cos_right * sin_lambda
        term_right = cos_left * sin_right - sin_left * cos_right * cos_lambda
        sin_sigma = math.hypot(term_left, term_right)
        if sin_sigma <= 1e-16:
            return 0.0
        cos_sigma = sin_left * sin_right + cos_left * cos_right * cos_lambda
        sigma = math.atan2(sin_sigma, cos_sigma)
        sin_alpha = cos_left * cos_right * sin_lambda / sin_sigma
        cos_squared_alpha = 1.0 - sin_alpha * sin_alpha
        cos_two_sigma_mid = (
            cos_sigma - 2.0 * sin_left * sin_right / cos_squared_alpha
            if cos_squared_alpha > 1e-16
            else 0.0
        )
        correction = WGS84_F / 16.0 * cos_squared_alpha * (
            4.0 + WGS84_F * (4.0 - 3.0 * cos_squared_alpha)
        )
        previous = lambda_value
        lambda_value = longitude_delta + (1.0 - correction) * WGS84_F * sin_alpha * (
            sigma
            + correction
            * sin_sigma
            * (
                cos_two_sigma_mid
                + correction
                * cos_sigma
                * (-1.0 + 2.0 * cos_two_sigma_mid * cos_two_sigma_mid)
            )
        )
        if abs(lambda_value - previous) < 1e-12:
            u_squared = cos_squared_alpha * (
                WGS84_A_M * WGS84_A_M - WGS84_B_M * WGS84_B_M
            ) / (WGS84_B_M * WGS84_B_M)
            coefficient_a = 1.0 + u_squared / 16384.0 * (
                4096.0
                + u_squared * (-768.0 + u_squared * (320.0 - 175.0 * u_squared))
            )
            coefficient_b = u_squared / 1024.0 * (
                256.0
                + u_squared * (-128.0 + u_squared * (74.0 - 47.0 * u_squared))
            )
            delta_sigma = coefficient_b * sin_sigma * (
                cos_two_sigma_mid
                + coefficient_b
                / 4.0
                * (
                    cos_sigma * (-1.0 + 2.0 * cos_two_sigma_mid**2)
                    - coefficient_b
                    / 6.0
                    * cos_two_sigma_mid
                    * (-3.0 + 4.0 * sin_sigma**2)
                    * (-3.0 + 4.0 * cos_two_sigma_mid**2)
                )
            )
            return WGS84_B_M * coefficient_a * (sigma - delta_sigma)

    sin_half_latitude = math.sin((latitude_right - latitude_left) * 0.5)
    sin_half_longitude = math.sin(longitude_delta * 0.5)
    haversine = sin_half_latitude**2 + (
        math.cos(latitude_left) * math.cos(latitude_right) * sin_half_longitude**2
    )
    return 2.0 * MEAN_EARTH_RADIUS_M * math.asin(
        math.sqrt(min(1.0, max(0.0, haversine)))
    )
