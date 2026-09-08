#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TRIADIstanaExploreV5DPalmHeroSourceLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DPalmHeroSourceContractTest,
    "TRIAD.Istana.ExploreV5D.PalmHero.SourceContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DPalmHeroSourceContractTest::RunTest(
    const FString& Parameters)
{
    TestEqual(
        TEXT("Exact LOD count"),
        UTRIADIstanaExploreV5DPalmHeroSourceLibrary::ExpectedLodCount(),
        3);
    TestEqual(
        TEXT("Exact material count"),
        UTRIADIstanaExploreV5DPalmHeroSourceLibrary::
            ExpectedMaterialCount(),
        3);
    TestEqual(
        TEXT("Exact texture count"),
        UTRIADIstanaExploreV5DPalmHeroSourceLibrary::ExpectedTextureCount(),
        4);
    TestEqual(
        TEXT("Stable Bark route"),
        UTRIADIstanaExploreV5DPalmHeroSourceLibrary::MaterialObjectPath(0),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/PalmHeroIntegration/Materials/M_IPV5D_PalmHero_Bark.M_IPV5D_PalmHero_Bark")));
    TestEqual(
        TEXT("Stable existing-palm fallback"),
        UTRIADIstanaExploreV5DPalmHeroSourceLibrary::
            ExistingPalmFallbackObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Palm_NearLOD0.SM_IPV5D_Tree_Palm_NearLOD0")));
    TestTrue(
        TEXT("Out-of-range material route fails closed"),
        UTRIADIstanaExploreV5DPalmHeroSourceLibrary::MaterialObjectPath(3)
            .IsEmpty());
    TestTrue(
        TEXT("Out-of-range texture route fails closed"),
        UTRIADIstanaExploreV5DPalmHeroSourceLibrary::TextureObjectPath(-1)
            .IsEmpty());

    FTRIADIstanaExploreV5DPalmHeroAssets EmptyAssets;
    FTRIADIstanaExploreV5DOptionalPalmSource Selection;
    FString Error;
    TestFalse(
        TEXT("Missing admitted fallback denies optional-source resolution"),
        UTRIADIstanaExploreV5DPalmHeroSourceLibrary::
            ResolveOptionalGeometryVariationPalmSource(
                nullptr,
                EmptyAssets,
                true,
                Selection,
                Error));
    TestTrue(TEXT("Denied resolution explains failure"), !Error.IsEmpty());
    TestNull(
        TEXT("Denied resolution selects no mesh"),
        Selection.SelectedSourceMesh.Get());
    TestFalse(
        TEXT("Denied resolution provides no map/source-transform mutation"),
        Selection.bMapOrSourceTransformModified);
    TestFalse(
        TEXT("Denied resolution provides no simulation authority"),
        Selection.bCollisionNavigationLosRfSensorOrTerrainAuthority);
    return true;
}

#endif
