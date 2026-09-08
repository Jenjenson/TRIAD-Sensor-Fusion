#include "TRIADRLTrainingModel.h"

bool FTRIADRLTrainingModel::ValidateConfig(const FTRIADRLTrainingConfig& Config, FString& OutError)
{
    if (Config.SchemaVersion != TEXT("triad.rl_training.v1"))
    {
        OutError = TEXT("RL definition requires schemaVersion triad.rl_training.v1.");
        return false;
    }
    if (!FMath::IsFinite(Config.FixedStepSeconds) || Config.FixedStepSeconds < 0.02 ||
        Config.EpisodeHorizonSteps < 1 || Config.EpisodeHorizonSteps > 100000)
    {
        OutError = TEXT("RL fixed step or episode horizon is outside the bounded contract.");
        return false;
    }
    if (!FMath::IsFinite(Config.ProtectedZone.LongitudeDegrees) ||
        !FMath::IsFinite(Config.ProtectedZone.LatitudeDegrees) ||
        !FMath::IsFinite(Config.ProtectedZone.RadiusMeters) || Config.ProtectedZone.RadiusMeters < 10.0)
    {
        OutError = TEXT("RL protected-zone definition is invalid.");
        return false;
    }
    if (Config.MaximumSensorSites < 1 || Config.MaximumSensorSites > 32 ||
        !FMath::IsFinite(Config.SensorBudgetUnits) || Config.SensorBudgetUnits < 0.0 ||
        !FMath::IsFinite(Config.BluePlacementRadiusMeters) || Config.BluePlacementRadiusMeters < 50.0 ||
        !FMath::IsFinite(Config.BlueMinimumObjectiveStandoffMeters) ||
        Config.BlueMinimumObjectiveStandoffMeters < 0.0 ||
        Config.BlueMinimumObjectiveStandoffMeters >= Config.BluePlacementRadiusMeters ||
        !FMath::IsFinite(Config.MinimumSensorSeparationMeters) || Config.MinimumSensorSeparationMeters < 0.0 ||
        !FMath::IsFinite(Config.SensorCostUnits) || Config.SensorCostUnits < 0.0 ||
        !FMath::IsFinite(Config.RedMinimumSpawnRadiusMeters) ||
        !FMath::IsFinite(Config.RedMaximumSpawnRadiusMeters) ||
        Config.RedMinimumSpawnRadiusMeters <= Config.BluePlacementRadiusMeters ||
        Config.RedMaximumSpawnRadiusMeters < Config.RedMinimumSpawnRadiusMeters ||
        !FMath::IsFinite(Config.RedMinimumAltitudeMeters) ||
        !FMath::IsFinite(Config.RedMaximumAltitudeMeters) ||
        Config.RedMinimumAltitudeMeters < 5.0 || Config.RedMaximumAltitudeMeters < Config.RedMinimumAltitudeMeters ||
        Config.RedMinimumSwarmSize < 1 || Config.RedMaximumSwarmSize > 64 ||
        Config.RedMaximumSwarmSize < Config.RedMinimumSwarmSize ||
        !FMath::IsFinite(Config.RedMinimumFormationSpacingMeters) ||
        !FMath::IsFinite(Config.RedMaximumFormationSpacingMeters) ||
        Config.RedMinimumFormationSpacingMeters < 1.0 ||
        Config.RedMaximumFormationSpacingMeters < Config.RedMinimumFormationSpacingMeters ||
        !FMath::IsFinite(Config.MaximumTargetSpeedMetersPerSecond) ||
        Config.MaximumTargetSpeedMetersPerSecond < 0.1 || Config.MaximumTargetSpeedMetersPerSecond > 50.0 ||
        !FMath::IsFinite(Config.MaximumDistanceFromZoneMeters) || Config.MaximumDistanceFromZoneMeters < 100.0)
    {
        OutError = TEXT("RL continuous placement, deployment, budget, speed, or AOI bound is invalid.");
        return false;
    }
    OutError.Reset();
    return true;
}

double FTRIADRLTrainingModel::GreatCircleDistanceMeters(
    double LongitudeA, double LatitudeA, double LongitudeB, double LatitudeB)
{
    constexpr double EarthRadiusMeters = 6378137.0;
    const double LatitudeARadians = FMath::DegreesToRadians(LatitudeA);
    const double LatitudeBRadians = FMath::DegreesToRadians(LatitudeB);
    const double DeltaLatitude = LatitudeBRadians - LatitudeARadians;
    const double DeltaLongitude = FMath::DegreesToRadians(LongitudeB - LongitudeA);
    const double Haversine = FMath::Square(FMath::Sin(DeltaLatitude * 0.5)) +
        FMath::Cos(LatitudeARadians) * FMath::Cos(LatitudeBRadians) *
        FMath::Square(FMath::Sin(DeltaLongitude * 0.5));
    return EarthRadiusMeters * 2.0 * FMath::Asin(FMath::Sqrt(FMath::Clamp(Haversine, 0.0, 1.0)));
}

FVector FTRIADRLTrainingModel::ClampNormalizedAction(const FVector& Action)
{
    if (!FMath::IsFinite(Action.X) || !FMath::IsFinite(Action.Y) || !FMath::IsFinite(Action.Z))
    {
        return FVector::ZeroVector;
    }
    return FVector(
        FMath::Clamp(Action.X, -1.0, 1.0),
        FMath::Clamp(Action.Y, -1.0, 1.0),
        FMath::Clamp(Action.Z, -1.0, 1.0));
}

void FTRIADRLTrainingModel::ComputeStepRewards(
    const FTRIADRLRewardWeights& Weights,
    double PreviousZoneDistanceMeters,
    double CurrentZoneDistanceMeters,
    int32 NewlyDetectedTargets,
    bool bZoneReached,
    bool bConstraintViolation,
    double& OutBlueReward,
    double& OutRedReward)
{
    const double SafePrevious = FMath::Max(PreviousZoneDistanceMeters, 1.0);
    const double ProgressFraction = FMath::Clamp(
        (PreviousZoneDistanceMeters - CurrentZoneDistanceMeters) / SafePrevious, -1.0, 1.0);
    OutBlueReward = NewlyDetectedTargets * Weights.BlueDetection;
    OutRedReward = ProgressFraction * Weights.RedProgress + NewlyDetectedTargets * Weights.RedDetected;
    if (bZoneReached)
    {
        OutBlueReward += Weights.BlueZoneMiss;
        OutRedReward += Weights.RedZoneReached;
    }
    if (bConstraintViolation)
    {
        OutRedReward += Weights.RedConstraintViolation;
    }
}
