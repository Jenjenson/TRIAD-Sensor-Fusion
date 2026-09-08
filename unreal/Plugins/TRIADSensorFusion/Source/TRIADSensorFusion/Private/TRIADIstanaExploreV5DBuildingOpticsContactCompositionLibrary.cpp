#include "TRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary.h"

#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"

namespace
{
constexpr int32 SlotCount = 17;
constexpr int32 OutputAssetCount = 18;
constexpr bool bRuntimeCandidateSelectionCompiledAuthorized = false;
constexpr bool bRuntimeCandidateActivationCompiledAuthorized = false;

const TCHAR* const SlotNames[] = {
    TEXT("MAT_BOTTOM_HIDDEN"), TEXT("MAT_COMMERCIAL_HINT"),
    TEXT("MAT_GENERIC_BUILDING_HINT"), TEXT("MAT_HEALTHCARE_HINT"),
    TEXT("MAT_HOTEL_HINT"), TEXT("MAT_INDUSTRIAL_HINT"),
    TEXT("MAT_RELIGIOUS_HINT"), TEXT("MAT_RESIDENTIAL_HINT"),
    TEXT("MAT_ROOF_COMMERCIAL_HINT"),
    TEXT("MAT_ROOF_GENERIC_BUILDING_HINT"),
    TEXT("MAT_ROOF_HEALTHCARE_HINT"), TEXT("MAT_ROOF_HOTEL_HINT"),
    TEXT("MAT_ROOF_INDUSTRIAL_HINT"), TEXT("MAT_ROOF_RELIGIOUS_HINT"),
    TEXT("MAT_ROOF_RESIDENTIAL_HINT"),
    TEXT("MAT_ROOF_TRANSPORT_HINT"), TEXT("MAT_TRANSPORT_HINT")};

const TCHAR* const CandidateNames[] = {
    TEXT("MI_IPV5D_BOC_00_MAT_BOTTOM_HIDDEN"),
    TEXT("MI_IPV5D_BOC_01_MAT_COMMERCIAL_HINT"),
    TEXT("MI_IPV5D_BOC_02_MAT_GENERIC_BUILDING_HINT"),
    TEXT("MI_IPV5D_BOC_03_MAT_HEALTHCARE_HINT"),
    TEXT("MI_IPV5D_BOC_04_MAT_HOTEL_HINT"),
    TEXT("MI_IPV5D_BOC_05_MAT_INDUSTRIAL_HINT"),
    TEXT("MI_IPV5D_BOC_06_MAT_RELIGIOUS_HINT"),
    TEXT("MI_IPV5D_BOC_07_MAT_RESIDENTIAL_HINT"),
    TEXT("MI_IPV5D_BOC_08_MAT_ROOF_COMMERCIAL_HINT"),
    TEXT("MI_IPV5D_BOC_09_MAT_ROOF_GENERIC_BUILDING_HINT"),
    TEXT("MI_IPV5D_BOC_10_MAT_ROOF_HEALTHCARE_HINT"),
    TEXT("MI_IPV5D_BOC_11_MAT_ROOF_HOTEL_HINT"),
    TEXT("MI_IPV5D_BOC_12_MAT_ROOF_INDUSTRIAL_HINT"),
    TEXT("MI_IPV5D_BOC_13_MAT_ROOF_RELIGIOUS_HINT"),
    TEXT("MI_IPV5D_BOC_14_MAT_ROOF_RESIDENTIAL_HINT"),
    TEXT("MI_IPV5D_BOC_15_MAT_ROOF_TRANSPORT_HINT"),
    TEXT("MI_IPV5D_BOC_16_MAT_TRANSPORT_HINT")};

const TCHAR* const OpticsFallbackNames[] = {
    TEXT("MI_IPV5D_BSO_00_MAT_BOTTOM_HIDDEN"),
    TEXT("MI_IPV5D_BSO_01_MAT_COMMERCIAL_HINT"),
    TEXT("MI_IPV5D_BSO_02_MAT_GENERIC_BUILDING_HINT"),
    TEXT("MI_IPV5D_BSO_03_MAT_HEALTHCARE_HINT"),
    TEXT("MI_IPV5D_BSO_04_MAT_HOTEL_HINT"),
    TEXT("MI_IPV5D_BSO_05_MAT_INDUSTRIAL_HINT"),
    TEXT("MI_IPV5D_BSO_06_MAT_RELIGIOUS_HINT"),
    TEXT("MI_IPV5D_BSO_07_MAT_RESIDENTIAL_HINT"),
    TEXT("MI_IPV5D_BSO_08_MAT_ROOF_COMMERCIAL_HINT"),
    TEXT("MI_IPV5D_BSO_09_MAT_ROOF_GENERIC_BUILDING_HINT"),
    TEXT("MI_IPV5D_BSO_10_MAT_ROOF_HEALTHCARE_HINT"),
    TEXT("MI_IPV5D_BSO_11_MAT_ROOF_HOTEL_HINT"),
    TEXT("MI_IPV5D_BSO_12_MAT_ROOF_INDUSTRIAL_HINT"),
    TEXT("MI_IPV5D_BSO_13_MAT_ROOF_RELIGIOUS_HINT"),
    TEXT("MI_IPV5D_BSO_14_MAT_ROOF_RESIDENTIAL_HINT"),
    TEXT("MI_IPV5D_BSO_15_MAT_ROOF_TRANSPORT_HINT"),
    TEXT("MI_IPV5D_BSO_16_MAT_TRANSPORT_HINT")};

const TCHAR* const R31OfficialWall =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_OfficialWall.MI_IPV5D_R31_OfficialWall");
const TCHAR* const R31OfficialRoof =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_OfficialRoof.MI_IPV5D_R31_OfficialRoof");
const TCHAR* const R31FallbackWall =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackWall.MI_IPV5D_R31_FallbackWall");
const TCHAR* const R31FallbackRoof =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackRoof.MI_IPV5D_R31_FallbackRoof");

const TCHAR* const R31FallbackPaths[] = {
    R31FallbackRoof, R31FallbackWall, R31FallbackWall, R31FallbackWall,
    R31OfficialWall, R31FallbackWall, R31OfficialWall, R31FallbackWall,
    R31FallbackRoof, R31FallbackRoof, R31FallbackRoof, R31OfficialRoof,
    R31FallbackRoof, R31OfficialRoof, R31FallbackRoof, R31FallbackRoof,
    R31FallbackWall};

const FString OutputRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingOpticsContactComposition"));
const FString MaterialRoot(OutputRoot + TEXT("/Materials"));
const FString MasterName(TEXT("M_IPV5D_BuildingOpticsContact_Master"));
const FString MasterPath(
    MaterialRoot + TEXT("/") + MasterName + TEXT(".") + MasterName);
const FString OpticsRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingSurfaceOpticsIntegration/Materials"));
const FString OpticsMasterPath(
    OpticsRoot + TEXT("/M_IPV5D_BuildingSurfaceOptics_Master.M_IPV5D_BuildingSurfaceOptics_Master"));

static_assert(UE_ARRAY_COUNT(SlotNames) == SlotCount);
static_assert(UE_ARRAY_COUNT(CandidateNames) == SlotCount);
static_assert(UE_ARRAY_COUNT(OpticsFallbackNames) == SlotCount);
static_assert(UE_ARRAY_COUNT(R31FallbackPaths) == SlotCount);

FString IndexedPath(
    const FString& Root,
    const TCHAR* const* Names,
    int32 SlotIndex)
{
    return SlotIndex >= 0 && SlotIndex < SlotCount
        ? Root + TEXT("/") + Names[SlotIndex] + TEXT(".") +
              Names[SlotIndex]
        : FString();
}

FString CandidatePath(int32 SlotIndex)
{
    return IndexedPath(MaterialRoot, CandidateNames, SlotIndex);
}

FString OpticsFallbackPath(int32 SlotIndex)
{
    return IndexedPath(OpticsRoot, OpticsFallbackNames, SlotIndex);
}

bool HasNoDirectPhysicalAuthority(const UMaterialInterface* Material)
{
    const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material);
    const UMaterialInstanceConstant* ConstantInstance =
        Cast<UMaterialInstanceConstant>(Material);
    if (!Instance || !ConstantInstance || Instance->PhysMaterial ||
        ConstantInstance->PhysMaterialMask)
    {
        return false;
    }
    for (const TObjectPtr<UPhysicalMaterial>& PhysicalMaterial :
         Instance->PhysicalMaterialMap)
    {
        if (PhysicalMaterial)
        {
            return false;
        }
    }
    return true;
}

bool HasNoPhysicalAuthority(const UMaterialInterface* Material)
{
    return HasNoDirectPhysicalAuthority(Material) &&
        !Material->GetPhysicalMaterial() &&
        !Material->GetPhysicalMaterialMask();
}

bool ValidateExactClearCoatRoster(
    const TArray<TObjectPtr<UMaterialInterface>>& Materials,
    const FString& ExpectedMaster,
    const FString& ExpectedRoot,
    const TCHAR* const* ExpectedNames,
    FString& OutError)
{
    if (Materials.Num() != SlotCount || Materials.Contains(nullptr))
    {
        OutError = TEXT("Exactly seventeen ordered Clear Coat materials are required.");
        return false;
    }
    TSet<const UObject*> Unique;
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        UMaterialInstanceConstant* Instance =
            Cast<UMaterialInstanceConstant>(Materials[Index]);
        UMaterial* Master = Instance ? Instance->GetMaterial() : nullptr;
        if (!Instance ||
            Instance->GetClass() != UMaterialInstanceConstant::StaticClass() ||
            Instance->GetPathName() !=
                IndexedPath(ExpectedRoot, ExpectedNames, Index) ||
            !Instance->Parent || Instance->Parent->GetPathName() != ExpectedMaster ||
            !Master || Master->GetPathName() != ExpectedMaster ||
            Master->MaterialDomain != MD_Surface ||
            Master->BlendMode != BLEND_Opaque || Master->TwoSided ||
            !Master->bTangentSpaceNormal ||
            !Master->GetShadingModels().HasOnlyShadingModel(MSM_ClearCoat) ||
            !HasNoPhysicalAuthority(Instance) || Unique.Contains(Instance))
        {
            OutError = FString::Printf(
                TEXT("Clear Coat roster slot %d lost its exact path, direct master, opaque one-sided tangent-normal state, uniqueness, or no-physical-authority boundary."),
                Index);
            return false;
        }
        Unique.Add(Instance);
    }
    OutError.Reset();
    return true;
}
} // namespace

#if WITH_DEV_AUTOMATION_TESTS
bool TRIADBuildingOpticsContactCompositionTestHasNoDirectPhysicalAuthority(
    const UMaterialInterface* Material)
{
    return HasNoDirectPhysicalAuthority(Material);
}

bool TRIADBuildingOpticsContactCompositionTestHasNoPhysicalAuthority(
    const UMaterialInterface* Material)
{
    return HasNoPhysicalAuthority(Material);
}
#endif

bool UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
    ValidateCandidateMaterialRoster(
        const FTRIADIstanaExploreV5DBuildingOpticsContactCompositionAssets& Assets,
        FString& OutError)
{
    if (!ValidateExactClearCoatRoster(
            Assets.OrderedSlotMaterials,
            MasterPath,
            MaterialRoot,
            CandidateNames,
            OutError))
    {
        return false;
    }
    if (!bRuntimeCandidateSelectionCompiledAuthorized ||
        !bRuntimeCandidateActivationCompiledAuthorized)
    {
        OutError = TEXT("Building optics/contact composition selection and activation are intentionally unreachable in dormant source; reviewed provenance gates, source changes, and a recompile are required.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
    ValidateExactOpticsFallbackRoster(
        const TArray<TObjectPtr<UMaterialInterface>>& ExistingOpticsFallbacks,
        FString& OutError)
{
    if (!ValidateExactClearCoatRoster(
            ExistingOpticsFallbacks,
            OpticsMasterPath,
            OpticsRoot,
            OpticsFallbackNames,
            OutError))
    {
        OutError = TEXT("The exact seventeen-entry BuildingSurfaceOptics roster is a mandatory preferred fallback: ") + OutError;
        return false;
    }
    return true;
}

bool UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
    ValidateExactR31FallbackRoster(
        const TArray<TObjectPtr<UMaterialInterface>>& ExistingR31Fallbacks,
        FString& OutError)
{
    if (ExistingR31Fallbacks.Num() != SlotCount ||
        ExistingR31Fallbacks.Contains(nullptr))
    {
        OutError = TEXT("The exact seventeen-entry R31 roster is a mandatory ultimate fallback.");
        return false;
    }
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        if (!ExistingR31Fallbacks[Index] ||
            ExistingR31Fallbacks[Index]->GetPathName() != R31FallbackPaths[Index])
        {
            OutError = FString::Printf(
                TEXT("R31 ultimate-fallback slot %d lost its exact retained identity."),
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
    ResolveOptionalPresentationMaterials(
        const TArray<UMaterialInterface*>& ExistingOpticsFallbacks,
        const TArray<UMaterialInterface*>& ExistingR31Fallbacks,
        const FTRIADIstanaExploreV5DBuildingOpticsContactCompositionAssets& CandidateAssets,
        bool bExplicitlySelectCompositionCandidate,
        bool bPreferOpticsFallback,
        FTRIADIstanaExploreV5DOptionalBuildingOpticsContactComposition& OutSelection,
        FString& OutError)
{
    OutSelection =
        FTRIADIstanaExploreV5DOptionalBuildingOpticsContactComposition{};
    TArray<TObjectPtr<UMaterialInterface>> TypedOptics;
    TArray<TObjectPtr<UMaterialInterface>> TypedR31;
    TypedOptics.Reserve(ExistingOpticsFallbacks.Num());
    TypedR31.Reserve(ExistingR31Fallbacks.Num());
    for (UMaterialInterface* Material : ExistingOpticsFallbacks)
    {
        TypedOptics.Add(Material);
    }
    for (UMaterialInterface* Material : ExistingR31Fallbacks)
    {
        TypedR31.Add(Material);
    }
    if (!ValidateExactOpticsFallbackRoster(TypedOptics, OutError) ||
        !ValidateExactR31FallbackRoster(TypedR31, OutError) ||
        (bExplicitlySelectCompositionCandidate &&
         !ValidateCandidateMaterialRoster(CandidateAssets, OutError)))
    {
        return false;
    }

    OutSelection.SelectedSlotMaterials = bExplicitlySelectCompositionCandidate
        ? CandidateAssets.OrderedSlotMaterials
        : (bPreferOpticsFallback ? TypedOptics : TypedR31);
    OutSelection.bCompositionCandidateSelected =
        bExplicitlySelectCompositionCandidate;
    OutSelection.bOpticsFallbackSelected =
        !bExplicitlySelectCompositionCandidate && bPreferOpticsFallback;
    OutSelection.bR31FallbackSelected =
        !bExplicitlySelectCompositionCandidate && !bPreferOpticsFallback;
    OutSelection.bExactOpticsAndR31FallbacksPreserved = true;
    OutSelection.bMeshTopologyUvSlotRosterFootprintMassingOrTransformModified =
        false;
    OutSelection.bGeographyTerrainCollisionNavigationLosRfSensorOrSimulationAuthority =
        false;
    OutError.Reset();
    return true;
}

int32 UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
    ExpectedMaterialSlotCount()
{
    return SlotCount;
}

int32 UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
    ExpectedOutputAssetCount()
{
    return OutputAssetCount;
}

bool UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
    IsCandidateRuntimeSelectionCompiledAuthorized()
{
    return bRuntimeCandidateSelectionCompiledAuthorized;
}

bool UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
    IsCandidateRuntimeActivationCompiledAuthorized()
{
    return bRuntimeCandidateActivationCompiledAuthorized;
}

FString UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
    OutputNamespace()
{
    return OutputRoot;
}

FString UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
    MasterMaterialObjectPath()
{
    return MasterPath;
}

FString UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
    CandidateMaterialObjectPath(int32 SlotIndex)
{
    return CandidatePath(SlotIndex);
}

FString UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
    ExistingOpticsFallbackObjectPath(int32 SlotIndex)
{
    return OpticsFallbackPath(SlotIndex);
}

FString UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
    ExistingR31FallbackObjectPath(int32 SlotIndex)
{
    return SlotIndex >= 0 && SlotIndex < SlotCount
        ? R31FallbackPaths[SlotIndex]
        : FString();
}

FString UTRIADIstanaExploreV5DBuildingOpticsContactCompositionLibrary::
    SlotName(int32 SlotIndex)
{
    return SlotIndex >= 0 && SlotIndex < SlotCount
        ? SlotNames[SlotIndex]
        : FString();
}
