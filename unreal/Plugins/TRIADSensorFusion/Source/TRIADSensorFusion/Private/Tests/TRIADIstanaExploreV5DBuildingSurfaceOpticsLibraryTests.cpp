#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TRIADIstanaExploreV5DBuildingSurfaceOpticsLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DBuildingSurfaceOpticsContractTest,
    "TRIAD.Istana.ExploreV5D.BuildingSurfaceOptics.SourceBoundary",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DBuildingSurfaceOpticsContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    TestEqual(
        TEXT("Exact retained material-slot count"),
        UTRIADIstanaExploreV5DBuildingSurfaceOpticsLibrary::
            ExpectedMaterialSlotCount(),
        17);
    TestEqual(
        TEXT("One master plus seventeen slot instances"),
        UTRIADIstanaExploreV5DBuildingSurfaceOpticsLibrary::
            ExpectedOutputAssetCount(),
        18);
    TestEqual(
        TEXT("Exact isolated master path"),
        UTRIADIstanaExploreV5DBuildingSurfaceOpticsLibrary::
            MasterMaterialObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingSurfaceOpticsIntegration/Materials/M_IPV5D_BuildingSurfaceOptics_Master.M_IPV5D_BuildingSurfaceOptics_Master")));

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
    static_assert(UE_ARRAY_COUNT(ExpectedSlots) == 17);
    TSet<FString> CandidatePaths;
    for (int32 Index = 0; Index < 17; ++Index)
    {
        TestEqual(
            FString::Printf(TEXT("Slot name %d"), Index),
            UTRIADIstanaExploreV5DBuildingSurfaceOpticsLibrary::SlotName(Index),
            FString(ExpectedSlots[Index]));
        const FString CandidatePath =
            UTRIADIstanaExploreV5DBuildingSurfaceOpticsLibrary::
                CandidateMaterialObjectPath(Index);
        TestTrue(
            FString::Printf(TEXT("Candidate path %d is isolated and named"), Index),
            CandidatePath.StartsWith(
                TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingSurfaceOpticsIntegration/Materials/MI_IPV5D_BSO_")) &&
                CandidatePath.Contains(ExpectedSlots[Index]) &&
                !CandidatePaths.Contains(CandidatePath));
        CandidatePaths.Add(CandidatePath);
        TestTrue(
            FString::Printf(TEXT("Fallback path %d remains exact R31"), Index),
            UTRIADIstanaExploreV5DBuildingSurfaceOpticsLibrary::
                ExistingR31FallbackObjectPath(Index)
                .StartsWith(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_")));
    }
    TestEqual(TEXT("Seventeen unique candidate paths"), CandidatePaths.Num(), 17);
    TestTrue(
        TEXT("Negative candidate index denied"),
        UTRIADIstanaExploreV5DBuildingSurfaceOpticsLibrary::
            CandidateMaterialObjectPath(-1)
            .IsEmpty());
    TestTrue(
        TEXT("Out-of-range fallback index denied"),
        UTRIADIstanaExploreV5DBuildingSurfaceOpticsLibrary::
            ExistingR31FallbackObjectPath(17)
            .IsEmpty());

    FTRIADIstanaExploreV5DBuildingSurfaceOpticsAssets EmptyCandidate;
    FString Error;
    TestFalse(
        TEXT("Empty candidate roster fails closed"),
        UTRIADIstanaExploreV5DBuildingSurfaceOpticsLibrary::
            ValidateCandidateMaterialRoster(EmptyCandidate, Error));
    TestTrue(TEXT("Candidate denial explains exact count"), !Error.IsEmpty());
    TArray<UMaterialInterface*> EmptyFallbacks;
    FTRIADIstanaExploreV5DOptionalBuildingSurfaceOptics Selection;
    TestFalse(
        TEXT("Optional resolver refuses missing exact R31 fallbacks"),
        UTRIADIstanaExploreV5DBuildingSurfaceOpticsLibrary::
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
