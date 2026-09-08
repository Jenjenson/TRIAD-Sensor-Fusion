#if WITH_DEV_AUTOMATION_TESTS

#include "Materials/MaterialInstanceConstant.h"
#include "Misc/AutomationTest.h"
#include "PhysicalMaterials/PhysicalMaterialMask.h"
#include "TRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

bool TRIADBuildingOpticsContactCompositionTestHasNoDirectPhysicalAuthority(
    const UMaterialInterface* Material);
bool TRIADBuildingOpticsContactCompositionTestHasNoPhysicalAuthority(
    const UMaterialInterface* Material);

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DBuildingOpticsContactCompositionContractTest,
    "TRIAD.Istana.ExploreV5D.BuildingOpticsContactComposition.SourceBoundary",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DBuildingOpticsContactCompositionContractTest::
    RunTest(const FString& Parameters)
{
    (void)Parameters;
    using FLibrary =
        UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary;
    TestEqual(TEXT("Exact slot count"), FLibrary::ExpectedMaterialSlotCount(), 17);
    TestEqual(TEXT("Master plus slots"), FLibrary::ExpectedOutputAssetCount(), 18);
    TestFalse(
        TEXT("Dormant candidate selection is compiled off"),
        FLibrary::IsCandidateRuntimeSelectionCompiledAuthorized());
    TestFalse(
        TEXT("Dormant candidate activation is independently compiled off"),
        FLibrary::IsCandidateRuntimeActivationCompiledAuthorized());
    TestEqual(
        TEXT("Fresh isolated namespace"),
        FLibrary::OutputNamespace(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingOpticsContactComposition")));
    TestEqual(
        TEXT("Fresh isolated master"),
        FLibrary::MasterMaterialObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingOpticsContactComposition/Materials/M_IPV5D_BuildingOpticsContact_Master.M_IPV5D_BuildingOpticsContact_Master")));

    UMaterialInstanceConstant* PhysicalMaskProbe =
        NewObject<UMaterialInstanceConstant>(
            GetTransientPackage(), NAME_None, RF_Transient);
    TestNotNull(
        TEXT("Transient runtime physical-mask probe exists"),
        PhysicalMaskProbe);
    if (PhysicalMaskProbe)
    {
        TestTrue(
            TEXT("Unassigned MIC has no direct physical authority"),
            TRIADBuildingOpticsContactCompositionTestHasNoDirectPhysicalAuthority(
                PhysicalMaskProbe));
        UPhysicalMaterialMask* AdversarialMask =
            NewObject<UPhysicalMaterialMask>(
                PhysicalMaskProbe, NAME_None, RF_Transient);
        TestNotNull(
            TEXT("Transient adversarial physical-material mask exists"),
            AdversarialMask);
        PhysicalMaskProbe->PhysMaterialMask = AdversarialMask;
        TestTrue(
            TEXT("Effective UE5.5 mask getter exposes the direct MIC override"),
            PhysicalMaskProbe->GetPhysicalMaterialMask() ==
                AdversarialMask);
        TestFalse(
            TEXT("Direct runtime physical-mask mutation is denied"),
            TRIADBuildingOpticsContactCompositionTestHasNoDirectPhysicalAuthority(
                PhysicalMaskProbe));
        TestFalse(
            TEXT("Effective runtime physical-mask mutation is denied"),
            TRIADBuildingOpticsContactCompositionTestHasNoPhysicalAuthority(
                PhysicalMaskProbe));
        PhysicalMaskProbe->PhysMaterialMask = nullptr;
    }

    TSet<FString> CandidatePaths;
    TSet<FString> OpticsPaths;
    for (int32 Index = 0; Index < 17; ++Index)
    {
        const FString Candidate = FLibrary::CandidateMaterialObjectPath(Index);
        const FString Optics = FLibrary::ExistingOpticsFallbackObjectPath(Index);
        const FString R31 = FLibrary::ExistingR31FallbackObjectPath(Index);
        TestTrue(
            FString::Printf(TEXT("Candidate %d remains isolated"), Index),
            Candidate.StartsWith(
                TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingOpticsContactComposition/Materials/MI_IPV5D_BOC_")) &&
                !CandidatePaths.Contains(Candidate));
        TestTrue(
            FString::Printf(TEXT("Optics fallback %d remains exact"), Index),
            Optics.StartsWith(
                TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingSurfaceOpticsIntegration/Materials/MI_IPV5D_BSO_")) &&
                !OpticsPaths.Contains(Optics));
        TestTrue(
            FString::Printf(TEXT("R31 fallback %d remains exact"), Index),
            R31.StartsWith(
                TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_")));
        TestFalse(
            FString::Printf(TEXT("Slot %d has a name"), Index),
            FLibrary::SlotName(Index).IsEmpty());
        CandidatePaths.Add(Candidate);
        OpticsPaths.Add(Optics);
    }
    TestEqual(TEXT("Seventeen unique candidates"), CandidatePaths.Num(), 17);
    TestEqual(TEXT("Seventeen unique optics fallbacks"), OpticsPaths.Num(), 17);
    TestTrue(TEXT("Negative index denied"), FLibrary::CandidateMaterialObjectPath(-1).IsEmpty());
    TestTrue(TEXT("High index denied"), FLibrary::ExistingOpticsFallbackObjectPath(17).IsEmpty());

    FTRIADIstanaExploreV5DBuildingOpticsContactCompositionAssets EmptyCandidate;
    TArray<UMaterialInterface*> EmptyOptics;
    TArray<UMaterialInterface*> EmptyR31;
    FTRIADIstanaExploreV5DOptionalBuildingOpticsContactComposition Selection;
    FString Error;
    TestFalse(
        TEXT("Missing dual fallbacks fail closed"),
        FLibrary::ResolveOptionalPresentationMaterials(
            EmptyOptics,
            EmptyR31,
            EmptyCandidate,
            false,
            true,
            Selection,
            Error));
    TestFalse(
        TEXT("Denied resolver selects no candidate"),
        Selection.bCompositionCandidateSelected);
    TestFalse(
        TEXT("Denied resolver claims no fallback preservation"),
        Selection.bExactOpticsAndR31FallbacksPreserved);
    TestFalse(
        TEXT("Denied resolver claims no mesh change"),
        Selection.bMeshTopologyUvSlotRosterFootprintMassingOrTransformModified);
    TestFalse(
        TEXT("Denied resolver claims no authority"),
        Selection.bGeographyTerrainCollisionNavigationLosRfSensorOrSimulationAuthority);
    TestTrue(TEXT("Denial is explained"), !Error.IsEmpty());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
