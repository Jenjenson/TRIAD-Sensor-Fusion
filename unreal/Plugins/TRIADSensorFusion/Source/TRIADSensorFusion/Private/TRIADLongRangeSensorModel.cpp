#include "TRIADLongRangeSensorModel.h"

namespace
{
double UnitGaussian(FRandomStream& Stream)
{
    // Box-Muller with a deterministic Unreal random stream. Clamp avoids log(0).
    const double U1 = FMath::Max(static_cast<double>(Stream.GetFraction()), 1.0e-9);
    const double U2 = static_cast<double>(Stream.GetFraction());
    return FMath::Sqrt(-2.0 * FMath::Loge(U1)) * FMath::Cos(2.0 * PI * U2);
}

double WrapDegrees360(double Degrees)
{
    return FMath::Fmod(FMath::Fmod(Degrees, 360.0) + 360.0, 360.0);
}
}

FTRIADSearchRadarModelOutput FTRIADLongRangeSensorModel::EvaluateSearchRadar(
    const FTRIADSearchRadarModelInput& Input)
{
    FTRIADSearchRadarModelOutput Output;
    const double EnvelopeMeters = FMath::Max(Input.RangeEnvelopeMeters, 5000.0);
    const double RangeMeters = FMath::Max(Input.TrueRangeMeters, 0.0);
    const double HalfElevationFieldOfRegard = FMath::Clamp(
        Input.ElevationFieldOfRegardDegrees * 0.5,
        0.5,
        90.0);
    Output.bInRange = RangeMeters <= EnvelopeMeters;
    Output.bInElevationFieldOfRegard = FMath::Abs(Input.TrueElevationDegrees) <= HalfElevationFieldOfRegard;

    const double NormalizedRange = FMath::Clamp(RangeMeters / EnvelopeMeters, 0.0, 1.0);
    const double RangeFactor = FMath::Clamp(1.0 - 0.55 * FMath::Square(NormalizedRange), 0.0, 1.0);
    const double RcsRatioLog10 = FMath::LogX(
        10.0,
        FMath::Max(Input.RadarCrossSectionSquareMeters, 0.0001) / 0.01);
    const double RcsFactor = 0.55 + 0.45 * FMath::Clamp(RcsRatioLog10 / 2.0, 0.0, 1.0);
    const double LosFactor = Input.bLineOfSight ? 1.0 : 0.45;
    const double VisibilityRatio = FMath::Clamp(
        FMath::Max(Input.WeatherVisibilityMeters, 1.0) / FMath::Max(RangeMeters, 1.0),
        0.0,
        1.0);
    const double WeatherFactor =
        (0.85 + 0.15 * VisibilityRatio) *
        FMath::Clamp(1.0 - FMath::Max(Input.RainRateMillimetersPerHour, 0.0) * 0.0015, 0.80, 1.0);
    Output.Confidence = FMath::Clamp(
        RangeFactor * RcsFactor * LosFactor * WeatherFactor,
        0.0,
        1.0);
    Output.bDetected = Output.bInRange && Output.bInElevationFieldOfRegard &&
        Output.Confidence >= FMath::Clamp(Input.DetectionThreshold, 0.0, 1.0);

    FRandomStream Stream(static_cast<int32>(Input.DeterministicSeed == 0 ? 1 : Input.DeterministicSeed));
    Output.MeasuredRangeMeters = FMath::Max(
        RangeMeters + UnitGaussian(Stream) * FMath::Max(Input.RangeNoiseSigmaMeters, 0.0),
        0.0);
    Output.MeasuredBearingDegrees = WrapDegrees360(
        Input.TrueBearingDegrees + UnitGaussian(Stream) * FMath::Max(Input.BearingNoiseSigmaDegrees, 0.0));
    Output.MeasuredElevationDegrees = Input.TrueElevationDegrees +
        UnitGaussian(Stream) * FMath::Max(Input.ElevationNoiseSigmaDegrees, 0.0);
    Output.MeasuredRadialVelocityMetersPerSecond = Input.TrueRadialVelocityMetersPerSecond +
        UnitGaussian(Stream) * FMath::Max(Input.RadialVelocityNoiseSigmaMetersPerSecond, 0.0);
    return Output;
}

FVector2D FTRIADLongRangeSensorModel::ComputeProjectedPixelExtent(
    double TargetWidthMeters,
    double TargetHeightMeters,
    double RangeMeters,
    double HorizontalFieldOfViewDegrees,
    int32 ImageWidthPixels,
    int32 ImageHeightPixels)
{
    const double SafeRangeMeters = FMath::Max(RangeMeters, 0.01);
    const double HorizontalFovRadians = FMath::DegreesToRadians(
        FMath::Clamp(HorizontalFieldOfViewDegrees, 0.1, 179.0));
    const double AspectRatio = static_cast<double>(FMath::Max(ImageWidthPixels, 1)) /
        static_cast<double>(FMath::Max(ImageHeightPixels, 1));
    const double VerticalFovRadians = 2.0 * FMath::Atan(FMath::Tan(HorizontalFovRadians * 0.5) /
        FMath::Max(AspectRatio, 0.000001));
    const double TargetHorizontalAngle = 2.0 * FMath::Atan(FMath::Max(TargetWidthMeters, 0.0) /
        (2.0 * SafeRangeMeters));
    const double TargetVerticalAngle = 2.0 * FMath::Atan(FMath::Max(TargetHeightMeters, 0.0) /
        (2.0 * SafeRangeMeters));
    return FVector2D(
        TargetHorizontalAngle / HorizontalFovRadians * FMath::Max(ImageWidthPixels, 1),
        TargetVerticalAngle / VerticalFovRadians * FMath::Max(ImageHeightPixels, 1));
}
