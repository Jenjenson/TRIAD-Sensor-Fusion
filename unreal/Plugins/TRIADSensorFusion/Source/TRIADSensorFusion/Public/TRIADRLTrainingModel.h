#pragma once

#include "CoreMinimal.h"
#include "TRIADRLTrainingTypes.h"

/** Pure deterministic math for the simulation-only RL episode layer. */
class TRIADSENSORFUSION_API FTRIADRLTrainingModel
{
public:
    static bool ValidateConfig(const FTRIADRLTrainingConfig& Config, FString& OutError);
    static double GreatCircleDistanceMeters(double LongitudeA, double LatitudeA, double LongitudeB, double LatitudeB);
    static FVector ClampNormalizedAction(const FVector& Action);
    static void ComputeStepRewards(
        const FTRIADRLRewardWeights& Weights,
        double PreviousZoneDistanceMeters,
        double CurrentZoneDistanceMeters,
        int32 NewlyDetectedTargets,
        bool bZoneReached,
        bool bConstraintViolation,
        double& OutBlueReward,
        double& OutRedReward);
};
