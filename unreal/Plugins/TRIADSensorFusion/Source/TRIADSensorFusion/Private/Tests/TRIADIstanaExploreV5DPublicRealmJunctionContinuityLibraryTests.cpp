#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DPublicRealmJunctionContinuityContractTest,
    "TRIAD.Istana.ExploreV5D.PublicRealm.JunctionContinuity.SourceBoundary",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DPublicRealmJunctionContinuityContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    using FLibrary =
        UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary;
    TestEqual(
        TEXT("Exactly two isolated candidate assets"),
        FLibrary::ExpectedCandidateAssetCount(),
        2);
    TestEqual(
        TEXT("Exact normal override row count"),
        FLibrary::ExpectedNormalOverrideCount(),
        355);
    TestFalse(
        TEXT("Dormant source has no runtime candidate-selection authority"),
        FLibrary::IsCandidateRuntimeSelectionCompiledAuthorized());
    TestFalse(
        TEXT("Dormant source has no runtime candidate-activation authority"),
        FLibrary::IsCandidateRuntimeActivationCompiledAuthorized());
    TestEqual(
        TEXT("Exact existing Core fallback path"),
        FLibrary::ExistingCoreMeshObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm/SM_IPV5D_PublicRealm_Core_Render.SM_IPV5D_PublicRealm_Core_Render")));
    TestEqual(
        TEXT("Exact existing Fallback path"),
        FLibrary::ExistingFallbackMeshObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm/SM_IPV5D_PublicRealm_Fallback_Render.SM_IPV5D_PublicRealm_Fallback_Render")));
    TestEqual(
        TEXT("Fresh isolated candidate namespace"),
        FLibrary::OutputNamespace(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealmJunctionContinuityIntegration")));
    TestNotEqual(
        TEXT("Candidate Core keeps a distinct identity"),
        FLibrary::CandidateCoreMeshObjectPath(),
        FLibrary::ExistingCoreMeshObjectPath());
    TestNotEqual(
        TEXT("Candidate Fallback keeps a distinct identity"),
        FLibrary::CandidateFallbackMeshObjectPath(),
        FLibrary::ExistingFallbackMeshObjectPath());

    FTRIADIstanaExploreV5DPublicRealmJunctionContinuityAssets EmptyCandidate;
    FTRIADIstanaExploreV5DOptionalPublicRealmJunctionContinuity Selection;
    FString Error;
    TestFalse(
        TEXT("Missing exact Core/Fallback pair fails closed"),
        FLibrary::ResolveOptionalRenderMeshes(
            nullptr,
            nullptr,
            EmptyCandidate,
            false,
            Selection,
            Error));
    TestTrue(TEXT("Fallback denial is explained"), !Error.IsEmpty());
    TestFalse(
        TEXT("Denied selection never selects the candidate"),
        Selection.bCandidateSelected);
    TestFalse(
        TEXT("Denied selection never claims fallback preservation"),
        Selection.bExactCoreAndFallbackPreserved);
    TestFalse(
        TEXT("Denied selection never claims geometry mutation"),
        Selection.bPositionUvTopologyGroupSlotTransformOrIdentityModified);
    TestFalse(
        TEXT("Denied selection never claims simulation authority"),
        Selection.bTerrainCollisionNavigationLosRfSensorOrSimulationAuthority);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
