#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DGroundVegetationActor.h"
#include "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADR29CopernicusTerrainFallbackDefaultContractTest,
    "TRIAD.Istana.ExploreV5D.R29CopernicusTerrainFallback.DefaultContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADR29CopernicusTerrainFallbackDefaultContractTest::RunTest(
    const FString& Parameters)
{
    const auto* Defaults = GetDefault<
        ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor>();
    TestNotNull(TEXT("Class default exists"), Defaults);
    if (!Defaults)
    {
        return false;
    }
    TestTrue(TEXT("Cesium stays preferred"),
        Defaults->bCesiumWorldTerrainPreferred);
    TestTrue(TEXT("DEM is relative DSM visual fallback only"),
        Defaults->bRelativeDsmVisualFallbackOnly);
    TestTrue(TEXT("Occupied core is protected"),
        Defaults->bAuthoredCoreProtectedByExactComplementaryMask);
    TestFalse(TEXT("No collision/navigation/sensor/RF authority"),
        Defaults->bCollisionNavigationSensorRfAuthority);
    TestFalse(TEXT("No absolute/geospatial/survey authority"),
        Defaults->bAbsoluteHeightGeospatialOrSurveyAuthority);
    TestFalse(TEXT("No bare-earth DTM claim"),
        Defaults->bBareEarthDtmClaimed);
    TestEqual(TEXT("Exact import scale"),
        ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
            ExpectedImportUniformScale(),
        100.0);
    TestTrue(TEXT("Exact minimum native bounds"),
        ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
            ExpectedBoundsMinimumCentimeters().Equals(
                FVector(-100000.0, -100000.0, -3030.3865), 0.000001));
    TestTrue(TEXT("Exact maximum native bounds"),
        ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
            ExpectedBoundsMaximumCentimeters().Equals(
                FVector(100000.0, 100000.0, 119.3868), 0.000001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADR29CopernicusTerrainFallbackComplementTest,
    "TRIAD.Istana.ExploreV5D.R29CopernicusTerrainFallback.ExactCoreComplement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADR29CopernicusTerrainFallbackComplementTest::RunTest(
    const FString& Parameters)
{
    const FVector2D Center =
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedProviderSiteClipCenterCentimeters();
    TestEqual(TEXT("64-edge provider clip"),
        ATRIADIstanaExploreV5DContextPolicyActor::
            ExpectedProviderSiteClipSplinePoints(),
        64);
    TestEqual(TEXT("50 m opaque collar"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedGroundOverlayOpaqueCollarMeters(),
        50.0);
    TestEqual(TEXT("8 m feather"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedGroundOverlayOutwardFeatherMeters(),
        8.0);
    TestEqual(TEXT("0.25 m stable dither cell"),
        ATRIADIstanaExploreV5DGroundVegetationActor::
            ExpectedGroundOverlayDitherCellMeters(),
        0.25);
    TestTrue(TEXT("Core coverage is opaque at centre"),
        FMath::IsNearlyEqual(
            ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
                EvaluateAuthoredCoreCoverage(Center),
            1.0,
            0.000001));
    TestFalse(TEXT("DEM is absent at occupied-core centre"),
        ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
            EvaluateFallbackStableDitherMask(Center));
    TestTrue(TEXT("DEM is present well outside protected core"),
        ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
            EvaluateFallbackStableDitherMask(
                FVector2D(90000.0, 90000.0)));

    int32 ComplementChecks = 0;
    for (int32 Y = -90000; Y <= 90000; Y += 7500)
    {
        for (int32 X = -90000; X <= 90000; X += 7500)
        {
            const FVector2D Sample(X, Y);
            const bool bCore =
                ATRIADIstanaExploreV5DGroundVegetationActor::
                    EvaluateGroundOverlayStableDitherMask(Sample);
            const bool bFallback =
                ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
                    EvaluateFallbackStableDitherMask(Sample);
            TestTrue(TEXT("Every sample has exactly one visual owner"),
                bCore != bFallback);
            ++ComplementChecks;
        }
    }
    TestEqual(TEXT("Grid complement samples"), ComplementChecks, 625);
    return true;
}

#endif
