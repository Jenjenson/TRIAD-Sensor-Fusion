#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TRIADIstanaExploreV5DBuildingFootContactLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DBuildingFootContactContractTest,
    "TRIAD.Istana.ExploreV5D.BuildingFootContact.SourceBoundary",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DBuildingFootContactContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    TestEqual(
        TEXT("Exact retained material-slot count"),
        UTRIADIstanaExploreV5DBuildingFootContactLibrary::
            ExpectedMaterialSlotCount(),
        17);
    TestEqual(
        TEXT("One master plus seventeen slot instances"),
        UTRIADIstanaExploreV5DBuildingFootContactLibrary::
            ExpectedOutputAssetCount(),
        18);
    TestFalse(
        TEXT("Dormant source has no compiled runtime candidate-selection authority"),
        UTRIADIstanaExploreV5DBuildingFootContactLibrary::
            IsCandidateRuntimeSelectionCompiledAuthorized());
    TestEqual(
        TEXT("Exact isolated master path"),
        UTRIADIstanaExploreV5DBuildingFootContactLibrary::
            MasterMaterialObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingFootContactIntegration/Materials/M_IPV5D_BuildingFootContact_Master.M_IPV5D_BuildingFootContact_Master")));

    const TCHAR* const ExpectedSlots[] = {
        TEXT("MAT_BOTTOM_HIDDEN"), TEXT("MAT_COMMERCIAL_HINT"),
        TEXT("MAT_GENERIC_BUILDING_HINT"), TEXT("MAT_HEALTHCARE_HINT"),
        TEXT("MAT_HOTEL_HINT"), TEXT("MAT_INDUSTRIAL_HINT"),
        TEXT("MAT_RELIGIOUS_HINT"), TEXT("MAT_RESIDENTIAL_HINT"),
        TEXT("MAT_ROOF_COMMERCIAL_HINT"),
        TEXT("MAT_ROOF_GENERIC_BUILDING_HINT"),
        TEXT("MAT_ROOF_HEALTHCARE_HINT"), TEXT("MAT_ROOF_HOTEL_HINT"),
        TEXT("MAT_ROOF_INDUSTRIAL_HINT"),
        TEXT("MAT_ROOF_RELIGIOUS_HINT"),
        TEXT("MAT_ROOF_RESIDENTIAL_HINT"),
        TEXT("MAT_ROOF_TRANSPORT_HINT"), TEXT("MAT_TRANSPORT_HINT")};
    const TCHAR* const ExpectedFallbacks[] = {
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackRoof.MI_IPV5D_R31_FallbackRoof"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackWall.MI_IPV5D_R31_FallbackWall"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackWall.MI_IPV5D_R31_FallbackWall"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackWall.MI_IPV5D_R31_FallbackWall"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_OfficialWall.MI_IPV5D_R31_OfficialWall"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackWall.MI_IPV5D_R31_FallbackWall"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_OfficialWall.MI_IPV5D_R31_OfficialWall"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackWall.MI_IPV5D_R31_FallbackWall"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackRoof.MI_IPV5D_R31_FallbackRoof"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackRoof.MI_IPV5D_R31_FallbackRoof"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackRoof.MI_IPV5D_R31_FallbackRoof"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_OfficialRoof.MI_IPV5D_R31_OfficialRoof"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackRoof.MI_IPV5D_R31_FallbackRoof"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_OfficialRoof.MI_IPV5D_R31_OfficialRoof"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackRoof.MI_IPV5D_R31_FallbackRoof"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackRoof.MI_IPV5D_R31_FallbackRoof"),
        TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackWall.MI_IPV5D_R31_FallbackWall")};
    static_assert(UE_ARRAY_COUNT(ExpectedSlots) == 17);
    static_assert(UE_ARRAY_COUNT(ExpectedFallbacks) == 17);
    TSet<FString> CandidatePaths;
    for (int32 Index = 0; Index < 17; ++Index)
    {
        TestEqual(
            FString::Printf(TEXT("Slot name %d"), Index),
            UTRIADIstanaExploreV5DBuildingFootContactLibrary::SlotName(Index),
            FString(ExpectedSlots[Index]));
        const FString CandidatePath =
            UTRIADIstanaExploreV5DBuildingFootContactLibrary::
                CandidateMaterialObjectPath(Index);
        TestTrue(
            FString::Printf(TEXT("Candidate path %d is isolated and named"), Index),
            CandidatePath.StartsWith(
                TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingFootContactIntegration/Materials/MI_IPV5D_BFC_")) &&
                CandidatePath.Contains(ExpectedSlots[Index]) &&
                !CandidatePaths.Contains(CandidatePath));
        CandidatePaths.Add(CandidatePath);
        TestEqual(
            FString::Printf(TEXT("Fallback path %d remains exact R31"), Index),
            UTRIADIstanaExploreV5DBuildingFootContactLibrary::
                ExistingR31FallbackObjectPath(Index),
            FString(ExpectedFallbacks[Index]));
    }
    TestEqual(TEXT("Seventeen unique candidate paths"), CandidatePaths.Num(), 17);
    TestTrue(
        TEXT("Negative candidate index denied"),
        UTRIADIstanaExploreV5DBuildingFootContactLibrary::
            CandidateMaterialObjectPath(-1)
            .IsEmpty());
    TestTrue(
        TEXT("Out-of-range fallback index denied"),
        UTRIADIstanaExploreV5DBuildingFootContactLibrary::
            ExistingR31FallbackObjectPath(17)
            .IsEmpty());
    TestTrue(
        TEXT("Out-of-range slot name denied"),
        UTRIADIstanaExploreV5DBuildingFootContactLibrary::SlotName(17)
            .IsEmpty());

    FTRIADIstanaExploreV5DBuildingFootContactAssets EmptyCandidate;
    FString Error;
    TestFalse(
        TEXT("Empty candidate roster fails closed"),
        UTRIADIstanaExploreV5DBuildingFootContactLibrary::
            ValidateCandidateMaterialRoster(EmptyCandidate, Error));
    TestTrue(TEXT("Candidate denial explains exact count"), !Error.IsEmpty());
    TArray<UMaterialInterface*> EmptyFallbacks;
    FTRIADIstanaExploreV5DOptionalBuildingFootContact Selection;
    TestFalse(
        TEXT("Optional resolver refuses missing exact R31 fallbacks"),
        UTRIADIstanaExploreV5DBuildingFootContactLibrary::
            ResolveOptionalPresentationMaterials(
                EmptyFallbacks,
                EmptyCandidate,
                false,
                Selection,
                Error));
    TestFalse(TEXT("Denied resolver never selects candidate"), Selection.bCandidateSelected);
    TestFalse(
        TEXT("Denied resolver does not claim fallback preservation"),
        Selection.bExactR31FallbacksPreserved);
    TestFalse(
        TEXT("Denied resolver does not claim mesh mutation"),
        Selection.bMeshTopologyUvSlotRosterOrTransformModified);
    TestFalse(
        TEXT("Denied resolver does not claim simulation authority"),
        Selection.bGeographyTerrainCollisionNavigationLosRfSensorOrSimulationAuthority);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
