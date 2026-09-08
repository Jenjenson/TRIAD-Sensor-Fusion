#pragma once

#include "CoreMinimal.h"

struct FTRIADSimulationPerimeter;

namespace TRIAD::Geodesy
{
/** True only for the case-insensitive authored shape name "Circle". */
TRIADSENSORFUSION_API bool IsCircle(const FTRIADSimulationPerimeter& Perimeter);

/** Validate the enabled perimeter contract without changing backwards-compatible defaults. */
TRIADSENSORFUSION_API bool ValidatePerimeter(
    const FTRIADSimulationPerimeter& Perimeter,
    FString& OutError);

/** WGS84 Vincenty inverse distance with a deterministic spherical fallback. */
TRIADSENSORFUSION_API double Wgs84DistanceMeters(
    double LongitudeADegrees,
    double LatitudeADegrees,
    double LongitudeBDegrees,
    double LatitudeBDegrees);

/** WGS84 Vincenty direct solution. X is longitude, Y is latitude. */
TRIADSENSORFUSION_API FVector2D Wgs84DestinationDegrees(
    double StartLongitudeDegrees,
    double StartLatitudeDegrees,
    double BearingDegrees,
    double DistanceMeters);

/** Forward EPSG:3414 (SVY21 / Singapore TM) projection. */
TRIADSENSORFUSION_API bool Wgs84ToSvy21Meters(
    double LongitudeDegrees,
    double LatitudeDegrees,
    FVector2D& OutEastingNorthingMeters,
    FString& OutError);

/**
 * Positive outside, zero on the inclusive boundary, negative inside.
 * Circle uses WGS84 geodesic distance; Rectangle preserves the existing local
 * equirectangular calculation.
 */
TRIADSENSORFUSION_API double SignedDistanceToPerimeterMeters(
    const FTRIADSimulationPerimeter& Perimeter,
    double LongitudeDegrees,
    double LatitudeDegrees);
}
