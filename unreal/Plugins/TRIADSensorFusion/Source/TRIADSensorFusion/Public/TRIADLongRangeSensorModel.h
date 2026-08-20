#pragma once

#include "CoreMinimal.h"

/** Inputs to the deterministic, explicitly simulated geometry-only search radar. */
struct TRIADSENSORFUSION_API FTRIADSearchRadarModelInput
{
    double TrueRangeMeters = 0.0;
    double TrueBearingDegrees = 0.0;
    double TrueElevationDegrees = 0.0;
    double TrueRadialVelocityMetersPerSecond = 0.0;
    double RadarCrossSectionSquareMeters = 0.03;
    double RangeEnvelopeMeters = 8000.0;
    double ElevationFieldOfRegardDegrees = 120.0;
    double WeatherVisibilityMeters = 30000.0;
    double RainRateMillimetersPerHour = 0.0;
    double DetectionThreshold = 0.18;
    double RangeNoiseSigmaMeters = 2.5;
    double BearingNoiseSigmaDegrees = 0.20;
    double ElevationNoiseSigmaDegrees = 0.15;
    double RadialVelocityNoiseSigmaMetersPerSecond = 0.35;
    bool bLineOfSight = true;
    uint32 DeterministicSeed = 1;
};

struct TRIADSENSORFUSION_API FTRIADSearchRadarModelOutput
{
    double MeasuredRangeMeters = 0.0;
    double MeasuredBearingDegrees = 0.0;
    double MeasuredElevationDegrees = 0.0;
    double MeasuredRadialVelocityMetersPerSecond = 0.0;
    double Confidence = 0.0;
    bool bInRange = false;
    bool bInElevationFieldOfRegard = false;
    bool bDetected = false;
};

/** Pure math kept separate so close/100/250/500 m validation is repeatable without changing mesh scale. */
class TRIADSENSORFUSION_API FTRIADLongRangeSensorModel
{
public:
    static FTRIADSearchRadarModelOutput EvaluateSearchRadar(const FTRIADSearchRadarModelInput& Input);

    static FVector2D ComputeProjectedPixelExtent(
        double TargetWidthMeters,
        double TargetHeightMeters,
        double RangeMeters,
        double HorizontalFieldOfViewDegrees,
        int32 ImageWidthPixels,
        int32 ImageHeightPixels);
};
