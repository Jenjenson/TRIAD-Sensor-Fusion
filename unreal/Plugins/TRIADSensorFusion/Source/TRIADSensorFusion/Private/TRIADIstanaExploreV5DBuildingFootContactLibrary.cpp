#include "TRIADIstanaExploreV5DBuildingFootContactLibrary.h"

#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"

namespace
{
constexpr int32 SlotCount = 17;
constexpr int32 OutputAssetCount = 18;
constexpr bool bRuntimeCandidateSelectionCompiledAuthorized = false;

const TCHAR* const SlotNames[] = {
    TEXT("MAT_BOTTOM_HIDDEN"),
    TEXT("MAT_COMMERCIAL_HINT"),
    TEXT("MAT_GENERIC_BUILDING_HINT"),
    TEXT("MAT_HEALTHCARE_HINT"),
    TEXT("MAT_HOTEL_HINT"),
    TEXT("MAT_INDUSTRIAL_HINT"),
    TEXT("MAT_RELIGIOUS_HINT"),
    TEXT("MAT_RESIDENTIAL_HINT"),
    TEXT("MAT_ROOF_COMMERCIAL_HINT"),
    TEXT("MAT_ROOF_GENERIC_BUILDING_HINT"),
    TEXT("MAT_ROOF_HEALTHCARE_HINT"),
    TEXT("MAT_ROOF_HOTEL_HINT"),
    TEXT("MAT_ROOF_INDUSTRIAL_HINT"),
    TEXT("MAT_ROOF_RELIGIOUS_HINT"),
    TEXT("MAT_ROOF_RESIDENTIAL_HINT"),
    TEXT("MAT_ROOF_TRANSPORT_HINT"),
    TEXT("MAT_TRANSPORT_HINT")};

const TCHAR* const CandidateNames[] = {
    TEXT("MI_IPV5D_BFC_00_MAT_BOTTOM_HIDDEN"),
    TEXT("MI_IPV5D_BFC_01_MAT_COMMERCIAL_HINT"),
    TEXT("MI_IPV5D_BFC_02_MAT_GENERIC_BUILDING_HINT"),
    TEXT("MI_IPV5D_BFC_03_MAT_HEALTHCARE_HINT"),
    TEXT("MI_IPV5D_BFC_04_MAT_HOTEL_HINT"),
    TEXT("MI_IPV5D_BFC_05_MAT_INDUSTRIAL_HINT"),
    TEXT("MI_IPV5D_BFC_06_MAT_RELIGIOUS_HINT"),
    TEXT("MI_IPV5D_BFC_07_MAT_RESIDENTIAL_HINT"),
    TEXT("MI_IPV5D_BFC_08_MAT_ROOF_COMMERCIAL_HINT"),
    TEXT("MI_IPV5D_BFC_09_MAT_ROOF_GENERIC_BUILDING_HINT"),
    TEXT("MI_IPV5D_BFC_10_MAT_ROOF_HEALTHCARE_HINT"),
    TEXT("MI_IPV5D_BFC_11_MAT_ROOF_HOTEL_HINT"),
    TEXT("MI_IPV5D_BFC_12_MAT_ROOF_INDUSTRIAL_HINT"),
    TEXT("MI_IPV5D_BFC_13_MAT_ROOF_RELIGIOUS_HINT"),
    TEXT("MI_IPV5D_BFC_14_MAT_ROOF_RESIDENTIAL_HINT"),
    TEXT("MI_IPV5D_BFC_15_MAT_ROOF_TRANSPORT_HINT"),
    TEXT("MI_IPV5D_BFC_16_MAT_TRANSPORT_HINT")};

const TCHAR* const R31OfficialWall =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_OfficialWall.MI_IPV5D_R31_OfficialWall");
const TCHAR* const R31OfficialRoof =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_OfficialRoof.MI_IPV5D_R31_OfficialRoof");
const TCHAR* const R31FallbackWall =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackWall.MI_IPV5D_R31_FallbackWall");
const TCHAR* const R31FallbackRoof =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31/Materials/MI_IPV5D_R31_FallbackRoof.MI_IPV5D_R31_FallbackRoof");

const TCHAR* const R31Fallbacks[] = {
    R31FallbackRoof,
    R31FallbackWall,
    R31FallbackWall,
    R31FallbackWall,
    R31OfficialWall,
    R31FallbackWall,
    R31OfficialWall,
    R31FallbackWall,
    R31FallbackRoof,
    R31FallbackRoof,
    R31FallbackRoof,
    R31OfficialRoof,
    R31FallbackRoof,
    R31OfficialRoof,
    R31FallbackRoof,
    R31FallbackRoof,
    R31FallbackWall};

const FString OutputRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/BuildingFootContactIntegration"));
const FString MaterialRoot(OutputRoot + TEXT("/Materials"));
const FString MasterName(TEXT("M_IPV5D_BuildingFootContact_Master"));
const FString MasterPath(
    MaterialRoot + TEXT("/") + MasterName + TEXT(".") + MasterName);

static_assert(UE_ARRAY_COUNT(SlotNames) == SlotCount);
static_assert(UE_ARRAY_COUNT(CandidateNames) == SlotCount);
static_assert(UE_ARRAY_COUNT(R31Fallbacks) == SlotCount);

FString CandidatePath(int32 SlotIndex)
{
    return SlotIndex >= 0 && SlotIndex < SlotCount
        ? MaterialRoot + TEXT("/") + CandidateNames[SlotIndex] +
              TEXT(".") + CandidateNames[SlotIndex]
        : FString();
}
bool HasNoPhysicalAuthority(const UMaterialInterface* Material)
{
    const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material);
    if (Instance)
    {
        if (Instance->PhysMaterial)
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
    }
    return Material && !Material->GetPhysicalMaterial();
}
} // namespace

bool UTRIADIstanaExploreV5DBuildingFootContactLibrary::
    ValidateCandidateMaterialRoster(
        const FTRIADIstanaExploreV5DBuildingFootContactAssets& Assets,
        FString& OutError)
{
    if (Assets.OrderedSlotMaterials.Num() != SlotCount ||
        Assets.OrderedSlotMaterials.Contains(nullptr))
    {
        OutError = TEXT("Building-foot contact requires exactly seventeen ordered candidate materials.");
        return false;
    }
    TSet<const UObject*> UniqueMaterials;
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        UMaterialInterface* Interface = Assets.OrderedSlotMaterials[Index];
        UMaterialInstanceConstant* Instance =
            Cast<UMaterialInstanceConstant>(Interface);
        UMaterial* Master = Interface ? Interface->GetMaterial() : nullptr;
        if (!Instance ||
            Instance->GetClass() != UMaterialInstanceConstant::StaticClass() ||
            Instance->GetPathName() != CandidatePath(Index) ||
            !Instance->Parent || Instance->Parent->GetPathName() != MasterPath ||
            !Master || Master->GetPathName() != MasterPath ||
            Master->MaterialDomain != MD_Surface ||
            Master->BlendMode != BLEND_Opaque || Master->TwoSided ||
            !Master->bTangentSpaceNormal ||
            !Master->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
            !HasNoPhysicalAuthority(Instance) ||
            UniqueMaterials.Contains(Instance))
        {
            OutError = FString::Printf(
                TEXT("Building-foot contact slot %d lost its exact path, direct master, opaque Default Lit, one-sided, tangent-normal, unique, or no-physical-authority contract."),
                Index);
            return false;
        }
        UniqueMaterials.Add(Instance);
    }
    if (!bRuntimeCandidateSelectionCompiledAuthorized)
    {
        OutError = TEXT("Building-foot contact candidate selection is intentionally unreachable in this dormant source; a reviewed source change, stronger runtime provenance validator, and recompile are required.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool UTRIADIstanaExploreV5DBuildingFootContactLibrary::
    ValidateExactR31FallbackRoster(
        const TArray<TObjectPtr<UMaterialInterface>>& ExistingR31Fallbacks,
        FString& OutError)
{
    if (ExistingR31Fallbacks.Num() != SlotCount ||
        ExistingR31Fallbacks.Contains(nullptr))
    {
        OutError = TEXT("The exact seventeen-entry R31 fallback roster is mandatory.");
        return false;
    }
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        UMaterialInterface* Material = ExistingR31Fallbacks[Index];
        if (!Material || Material->GetPathName() != R31Fallbacks[Index])
        {
            OutError = FString::Printf(
                TEXT("R31 fallback material %d does not match the exact retained slot-role mapping."),
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool UTRIADIstanaExploreV5DBuildingFootContactLibrary::
    ResolveOptionalPresentationMaterials(
        const TArray<UMaterialInterface*>& ExistingR31Fallbacks,
        const FTRIADIstanaExploreV5DBuildingFootContactAssets& CandidateAssets,
        bool bExplicitlySelectCandidate,
        FTRIADIstanaExploreV5DOptionalBuildingFootContact& OutSelection,
        FString& OutError)
{
    OutSelection = FTRIADIstanaExploreV5DOptionalBuildingFootContact{};
    TArray<TObjectPtr<UMaterialInterface>> TypedFallbacks;
    TypedFallbacks.Reserve(ExistingR31Fallbacks.Num());
    for (UMaterialInterface* Material : ExistingR31Fallbacks)
    {
        TypedFallbacks.Add(Material);
    }
    if (!ValidateExactR31FallbackRoster(TypedFallbacks, OutError) ||
        (bExplicitlySelectCandidate &&
         !ValidateCandidateMaterialRoster(CandidateAssets, OutError)))
    {
        return false;
    }

    OutSelection.SelectedSlotMaterials = bExplicitlySelectCandidate
        ? CandidateAssets.OrderedSlotMaterials
        : TypedFallbacks;
    OutSelection.bCandidateSelected = bExplicitlySelectCandidate;
    OutSelection.bExactR31FallbacksPreserved = true;
    OutSelection.bMeshTopologyUvSlotRosterOrTransformModified = false;
    OutSelection.bGeographyTerrainCollisionNavigationLosRfSensorOrSimulationAuthority = false;
    OutError.Reset();
    return true;
}

int32 UTRIADIstanaExploreV5DBuildingFootContactLibrary::
    ExpectedMaterialSlotCount()
{
    return SlotCount;
}

int32 UTRIADIstanaExploreV5DBuildingFootContactLibrary::
    ExpectedOutputAssetCount()
{
    return OutputAssetCount;
}

bool UTRIADIstanaExploreV5DBuildingFootContactLibrary::
    IsCandidateRuntimeSelectionCompiledAuthorized()
{
    return bRuntimeCandidateSelectionCompiledAuthorized;
}

FString UTRIADIstanaExploreV5DBuildingFootContactLibrary::
    MasterMaterialObjectPath()
{
    return MasterPath;
}

FString UTRIADIstanaExploreV5DBuildingFootContactLibrary::
    CandidateMaterialObjectPath(int32 SlotIndex)
{
    return CandidatePath(SlotIndex);
}

FString UTRIADIstanaExploreV5DBuildingFootContactLibrary::
    ExistingR31FallbackObjectPath(int32 SlotIndex)
{
    return SlotIndex >= 0 && SlotIndex < SlotCount
        ? R31Fallbacks[SlotIndex]
        : FString();
}

FString UTRIADIstanaExploreV5DBuildingFootContactLibrary::
    SlotName(int32 SlotIndex)
{
    return SlotIndex >= 0 && SlotIndex < SlotCount
        ? SlotNames[SlotIndex]
        : FString();
}
