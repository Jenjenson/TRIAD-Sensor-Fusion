#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TRIADIstanaExploreV5DGradedTurfPresentationActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DGradedTurfPresentationDormantBoundaryTest,
    "TRIAD.Istana.ExploreV5D.GradedTurfPresentationV2.DormantBoundary",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DGradedTurfPresentationDormantBoundaryTest::RunTest(
    const FString& Parameters)
{
    TestFalse(
        TEXT("Runtime candidate selection stays compiled false"),
        ATRIADIstanaExploreV5DGradedTurfPresentationActor::
            IsRuntimeCandidateSelectionCompiledAuthorized());
    TestFalse(
        TEXT("Runtime candidate activation stays independently compiled false"),
        ATRIADIstanaExploreV5DGradedTurfPresentationActor::
            IsRuntimeCandidateActivationCompiledAuthorized());
    TestEqual(
        TEXT("R29 placement ownership remains exact"),
        ATRIADIstanaExploreV5DGradedTurfPresentationActor::
            ExpectedR29OwnedPlacementCount(),
        6144);
    TestEqual(
        TEXT("R32 placement ownership remains exact"),
        ATRIADIstanaExploreV5DGradedTurfPresentationActor::
            ExpectedR32OwnedPlacementCount(),
        4608);
    TestEqual(
        TEXT("Presentation proof starts at camera"),
        ATRIADIstanaExploreV5DGradedTurfPresentationActor::
            ExpectedPresentationStartDistanceCm(),
        0);
    TestEqual(
        TEXT("Presentation proof reaches 95 metres"),
        ATRIADIstanaExploreV5DGradedTurfPresentationActor::
            ExpectedPresentationProofEndDistanceCm(),
        9500);
    TestEqual(
        TEXT("Modeled turf starts fading at 65 metres"),
        ATRIADIstanaExploreV5DGradedTurfPresentationActor::
            ExpectedBladeFadeStartDistanceCm(),
        6500);
    TestEqual(
        TEXT("Modeled turf finishes fading at 90 metres"),
        ATRIADIstanaExploreV5DGradedTurfPresentationActor::
            ExpectedBladeFadeEndDistanceCm(),
        9000);
    TestEqual(
        TEXT("Grass001 source response starts its base transition at 50 metres"),
        ATRIADIstanaExploreV5DGradedTurfPresentationActor::
            ExpectedSurfaceResponseFadeStartDistanceCm(),
        5000);
    TestEqual(
        TEXT("Grass001 source response reaches its stable far base at 70 metres"),
        ATRIADIstanaExploreV5DGradedTurfPresentationActor::
            ExpectedSurfaceResponseFadeEndDistanceCm(),
        7000);
    TestEqual(
        TEXT("Exact overlay name stays pinned"),
        ATRIADIstanaExploreV5DGradedTurfPresentationActor::
            ExactOverlayComponentName(),
        FString(TEXT("V5DGroundMacroVariationOverlay")));
    TestEqual(
        TEXT("Exact accepted masked-lawn fallback stays pinned"),
        ATRIADIstanaExploreV5DGradedTurfPresentationActor::
            ExactAcceptedLawnMaterialObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/"
                     "GroundVegetation/Materials/"
                     "M_IPV5D_LawnMacroVariation."
                     "M_IPV5D_LawnMacroVariation")));
    TestEqual(
        TEXT("Candidate lives in its fresh V2 namespace"),
        ATRIADIstanaExploreV5DGradedTurfPresentationActor::
            CandidateMaterialObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
                     "GradedTurfPresentationIntegrationV2/Materials/"
                     "M_IPV5D_GradedTurfPresentationV2."
                     "M_IPV5D_GradedTurfPresentationV2")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
