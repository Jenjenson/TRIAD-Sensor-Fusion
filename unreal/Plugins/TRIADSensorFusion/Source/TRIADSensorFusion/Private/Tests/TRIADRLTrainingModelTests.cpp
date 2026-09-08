#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TRIADRLTrainingModel.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADRLTrainingModelDeterminismTest,
    "TRIAD.SensorFusion.RL.DeterministicMath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADRLTrainingModelDeterminismTest::RunTest(const FString& Parameters)
{
    const double A = FTRIADRLTrainingModel::GreatCircleDistanceMeters(103.842, 1.307, 103.848, 1.307);
    const double B = FTRIADRLTrainingModel::GreatCircleDistanceMeters(103.842, 1.307, 103.848, 1.307);
    TestEqual(TEXT("Same geodesic inputs are deterministic"), A, B);
    TestTrue(TEXT("Distance is finite and positive"), FMath::IsFinite(A) && A > 0.0);
    TestEqual(TEXT("Non-finite action fails closed"),
        FTRIADRLTrainingModel::ClampNormalizedAction(FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0)),
        FVector::ZeroVector);
    TestEqual(TEXT("Action X is clamped"),
        FTRIADRLTrainingModel::ClampNormalizedAction(FVector(2.0, -2.0, 0.5)),
        FVector(1.0, -1.0, 0.5));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADRLTrainingRewardTest,
    "TRIAD.SensorFusion.RL.RewardContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADRLTrainingRewardTest::RunTest(const FString& Parameters)
{
    FTRIADRLRewardWeights Weights;
    double Blue = 0.0;
    double Red = 0.0;
    FTRIADRLTrainingModel::ComputeStepRewards(Weights, 1000.0, 900.0, 1, false, false, Blue, Red);
    TestTrue(TEXT("A new detection benefits Blue"), Blue > 0.0);
    TestTrue(TEXT("Detection outweighs small Red progress"), Red < 0.0);
    FTRIADRLTrainingModel::ComputeStepRewards(Weights, 100.0, 0.0, 0, true, false, Blue, Red);
    TestTrue(TEXT("Geofence entry penalizes Blue"), Blue < 0.0);
    TestTrue(TEXT("Geofence entry rewards Red"), Red > 0.0);
    return true;
}

#endif
