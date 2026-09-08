#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceSourceContractTest,
    "TRIAD.Istana.ExploreV5D.OrdinaryDistanceGrassSurface.SourceContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceSourceContractTest::
    RunTest(const FString& Parameters)
{
    const TCHAR* const ExpectedTexturePaths[] = {
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceIntegration/Textures/T_IPV5D_Grass001_BaseColor.T_IPV5D_Grass001_BaseColor"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceIntegration/Textures/T_IPV5D_Grass001_NormalDX.T_IPV5D_Grass001_NormalDX"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceIntegration/Textures/T_IPV5D_Grass001_Roughness.T_IPV5D_Grass001_Roughness"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceIntegration/Textures/T_IPV5D_Grass001_AmbientOcclusion.T_IPV5D_Grass001_AmbientOcclusion"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceIntegration/Textures/T_IPV5D_Grass001_Height.T_IPV5D_Grass001_Height")};
    TestEqual(
        TEXT("Exact five-texture source roster"),
        UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
            ExpectedTextureCount(),
        5);
    TestEqual(
        TEXT("Exact six-asset output roster"),
        UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
            ExpectedOutputAssetCount(),
        6);
    TestEqual(
        TEXT("Provider-published tile scale"),
        UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
            ProviderTileMetres(),
        1.4);
    TestEqual(
        TEXT("Stable appearance material route"),
        UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
            MaterialObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/OrdinaryDistanceGrassSurfaceIntegration/Materials/M_IPV5D_OrdinaryDistanceGrassSurface.M_IPV5D_OrdinaryDistanceGrassSurface")));
    TestEqual(
        TEXT("Stable admitted lawn fallback"),
        UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
            ExistingLawnFallbackObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/M_IPV5D_LawnMacroVariation.M_IPV5D_LawnMacroVariation")));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedTexturePaths); ++Index)
    {
        TestEqual(
            *FString::Printf(TEXT("Stable texture route %d"), Index),
            UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
                TextureObjectPath(Index),
            FString(ExpectedTexturePaths[Index]));
    }
    TestTrue(
        TEXT("Out-of-range texture route fails closed"),
        UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
            TextureObjectPath(5).IsEmpty());

    FTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceAssets EmptyAssets;
    FTRIADIstanaExploreV5DOptionalGrassSurfaceMaterial Selection;
    FString Error;
    TestFalse(
        TEXT("Missing exact fallback denies optional selection"),
        UTRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary::
            ResolveOptionalPresentationMaterial(
                nullptr,
                EmptyAssets,
                true,
                Selection,
                Error));
    TestTrue(TEXT("Denied selection explains failure"), !Error.IsEmpty());
    TestNull(
        TEXT("Denied selection returns no material"),
        Selection.SelectedMaterial.Get());
    TestFalse(
        TEXT("Denied selection never modifies map/geography/terrain/transforms"),
        Selection.bMapGeographyTerrainOrSourceTransformModified);
    TestFalse(
        TEXT("Denied selection grants no simulation authority"),
        Selection.bCollisionNavigationLosRfSensorOrSimulationAuthority);
    return true;
}

#endif
