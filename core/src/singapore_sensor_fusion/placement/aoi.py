"""Deterministic AOI sampling in a local WGS84 ENU frame."""

from __future__ import annotations

import math
from typing import Callable

from ..geodesy import ENU, Geodetic, enu_to_geodetic
from .contracts import AreaOfInterest, CoverageSample, GeoPoint, SamplingPlan


SurfaceHeightProvider = Callable[[float, float], float]


def horizontal_distance_meters(sample: CoverageSample) -> float:
    return math.hypot(sample.east_meters, sample.north_meters)


def generate_circular_samples(
    aoi: AreaOfInterest,
    sampling: SamplingPlan,
    *,
    surface_height_provider: SurfaceHeightProvider | None = None,
) -> tuple[CoverageSample, ...]:
    """Generate a stable square-lattice sampling clipped to a circular AOI.

    ``surface_height_provider`` returns WGS84 ellipsoid surface height for one
    local east/north coordinate.  The default uses the AOI centre height and is
    suitable only for tests or coarse planning; an Unreal collision survey is
    required before a physical placement recommendation.
    """

    if not isinstance(aoi, AreaOfInterest) or not isinstance(sampling, SamplingPlan):
        raise TypeError("aoi and sampling must use placement contract types")
    provider = surface_height_provider or (lambda _east, _north: aoi.center.height_meters)
    origin = Geodetic(
        latitude_deg=aoi.center.latitude_degrees,
        longitude_deg=aoi.center.longitude_degrees,
        altitude_m=aoi.center.height_meters,
    )
    spacing = sampling.horizontal_spacing_meters
    steps = math.ceil(aoi.radius_meters / spacing)
    horizontal_points: list[tuple[float, float]] = []
    tolerance = max(1e-9, aoi.radius_meters * 1e-12)
    for north_index in range(-steps, steps + 1):
        north = north_index * spacing
        for east_index in range(-steps, steps + 1):
            east = east_index * spacing
            if math.hypot(east, north) <= aoi.radius_meters + tolerance:
                horizontal_points.append((east, north))

    projected_count = len(horizontal_points) * len(sampling.target_altitude_bands_agl_meters)
    if projected_count > sampling.maximum_sample_count:
        raise ValueError(
            f"AOI sampling would create {projected_count} records, exceeding "
            f"maximum_sample_count={sampling.maximum_sample_count}"
        )

    results: list[CoverageSample] = []
    ordinal = 0
    for east, north in horizontal_points:
        surface_height = float(provider(east, north))
        if not math.isfinite(surface_height):
            raise ValueError("surface_height_provider must return finite WGS84 heights")
        horizontal = enu_to_geodetic(ENU(east_m=east, north_m=north, up_m=0.0), origin)
        for altitude_agl in sampling.target_altitude_bands_agl_meters:
            results.append(
                CoverageSample(
                    sample_id=f"sample-{ordinal:06d}",
                    location=GeoPoint(
                        latitude_degrees=horizontal.latitude_deg,
                        longitude_degrees=horizontal.longitude_deg,
                        height_meters=surface_height + altitude_agl,
                    ),
                    east_meters=east,
                    north_meters=north,
                    altitude_agl_meters=altitude_agl,
                )
            )
            ordinal += 1
    return tuple(results)
