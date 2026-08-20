#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TRIADLongRangeSensorModel.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADSearchRadarExactRangeEnvelopeTest,
    "TRIAD.SensorFusion.LongRange.SearchRadar.CloseThrough500Meters",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADSearchRadarExactRangeEnvelopeTest::RunTest(const FString& Parameters)
{
    const TArray<double> ValidationRangesMeters = {25.0, 100.0, 250.0, 500.0};
    for (int32 Index = 0; Index < ValidationRangesMeters.Num(); ++Index)
    {
        FTRIADSearchRadarModelInput Input;
        Input.TrueRangeMeters = ValidationRangesMeters[Index];
        Input.RangeEnvelopeMeters = 8000.0;
        Input.RadarCrossSectionSquareMeters = 0.03;
        Input.RangeNoiseSigmaMeters = 0.0;
        Input.BearingNoiseSigmaDegrees = 0.0;
        Input.ElevationNoiseSigmaDegrees = 0.0;
        Input.RadialVelocityNoiseSigmaMetersPerSecond = 0.0;
        Input.DeterministicSeed = 0x54524941u + Index;
        const FTRIADSearchRadarModelOutput Output = FTRIADLongRangeSensorModel::EvaluateSearchRadar(Input);
        TestTrue(FString::Printf(TEXT("radar detects at %.0f m"), Input.TrueRangeMeters), Output.bDetected);
        TestEqual(
            FString::Printf(TEXT("exact range retained at %.0f m with noise disabled"), Input.TrueRangeMeters),
            Output.MeasuredRangeMeters,
            Input.TrueRangeMeters);

        const FVector2D PixelExtent = FTRIADLongRangeSensorModel::ComputeProjectedPixelExtent(
            0.50,
            0.20,
            Input.TrueRangeMeters,
            4.0,
            960,
            540);
        TestTrue(
            FString::Printf(TEXT("EO cue has at least two horizontal pixels at %.0f m"), Input.TrueRangeMeters),
            PixelExtent.X >= 2.0);
    }
    return true;
}

#endif
