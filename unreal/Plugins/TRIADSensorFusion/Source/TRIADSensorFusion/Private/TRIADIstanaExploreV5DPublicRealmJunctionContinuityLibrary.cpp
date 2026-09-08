#include "TRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
constexpr int32 CandidateAssetCount = 2;
constexpr int32 NormalOverrideCount = 355;
constexpr bool bRuntimeCandidateSelectionCompiledAuthorized = false;
constexpr bool bRuntimeCandidateActivationCompiledAuthorized = false;

const FString ExistingCorePath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm/"
         "SM_IPV5D_PublicRealm_Core_Render."
         "SM_IPV5D_PublicRealm_Core_Render"));
const FString ExistingFallbackPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm/"
         "SM_IPV5D_PublicRealm_Fallback_Render."
         "SM_IPV5D_PublicRealm_Fallback_Render"));
const FString OutputRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealmJunctionContinuityIntegration"));
const FString MeshRoot(OutputRoot + TEXT("/Meshes"));
const FString CandidateCoreName(
    TEXT("SM_IPV5D_PublicRealm_Core_Render_JunctionContinuity"));
const FString CandidateFallbackName(
    TEXT("SM_IPV5D_PublicRealm_Fallback_Render_JunctionContinuity"));
const FString CandidateCorePath(
    MeshRoot + TEXT("/") + CandidateCoreName + TEXT(".") +
    CandidateCoreName);
const FString CandidateFallbackPath(
    MeshRoot + TEXT("/") + CandidateFallbackName + TEXT(".") +
    CandidateFallbackName);

const TCHAR* const CoreMaterialPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/Materials/"
         "MI_IPV5C_OfficialPlanningRoadZone."
         "MI_IPV5C_OfficialPlanningRoadZone")};
const TCHAR* const FallbackMaterialPaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/Materials/"
         "MI_IPV5C_OfficialPlanningRoadZone."
         "MI_IPV5C_OfficialPlanningRoadZone"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/Materials/"
         "MI_IPV5C_OfficialPlanningRoadGraphic."
         "MI_IPV5C_OfficialPlanningRoadGraphic"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm/Materials/"
         "MI_IPV5D_PublicRealmConcrete."
         "MI_IPV5D_PublicRealmConcrete")};
const TCHAR* const CoreMaterialSlotNames[] = {
    TEXT("MI_IPV5C_OfficialPlanningRoadZone")};
const TCHAR* const FallbackMaterialSlotNames[] = {
    TEXT("MI_IPV5C_OfficialPlanningRoadZone"),
    TEXT("MI_IPV5C_OfficialPlanningRoadGraphic"),
    TEXT("MI_IPV5D_PublicRealmConcrete")};

static_assert(UE_ARRAY_COUNT(CoreMaterialPaths) == 1);
static_assert(UE_ARRAY_COUNT(CoreMaterialSlotNames) == 1);
static_assert(UE_ARRAY_COUNT(FallbackMaterialPaths) == 3);
static_assert(UE_ARRAY_COUNT(FallbackMaterialSlotNames) == 3);

bool HasExactRenderOnlyBoundary(const UStaticMesh* Mesh)
{
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
    return Mesh && Body && Body->AggGeom.GetElementCount() == 0 &&
        Body->CollisionTraceFlag == CTF_UseSimpleAsComplex &&
        !Mesh->bHasNavigationData && !Mesh->GetNavCollision();
}

bool ValidateMaterialRoster(
    const UStaticMesh* Mesh,
    const TCHAR* const* ExpectedPaths,
    const TCHAR* const* ExpectedSlotNames,
    int32 ExpectedCount,
    FString& OutError)
{
    if (!Mesh || Mesh->GetStaticMaterials().Num() != ExpectedCount)
    {
        OutError = TEXT("A public-realm junction mesh lost its exact material-slot count.");
        return false;
    }
    for (int32 Index = 0; Index < ExpectedCount; ++Index)
    {
        const FStaticMaterial& Slot = Mesh->GetStaticMaterials()[Index];
        if (!Slot.MaterialInterface ||
            Slot.MaterialInterface->GetPathName() != ExpectedPaths[Index] ||
            Slot.MaterialSlotName != FName(ExpectedSlotNames[Index]) ||
            Slot.ImportedMaterialSlotName != FName(ExpectedSlotNames[Index]))
        {
            OutError = FString::Printf(
                TEXT("Public-realm junction material slot %d lost its exact identity or binding."),
                Index);
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateMeshBoundary(
    const UStaticMesh* Mesh,
    const FString& ExpectedPath,
    bool bCore,
    FString& OutError)
{
    if (!Mesh || Mesh->GetClass() != UStaticMesh::StaticClass() ||
        Mesh->GetPathName() != ExpectedPath || Mesh->GetNumLODs() != 1 ||
        !HasExactRenderOnlyBoundary(Mesh))
    {
        OutError = TEXT("A public-realm junction mesh lost its exact path, one-LOD, no-collision, or no-navigation contract.");
        return false;
    }
    return bCore
        ? ValidateMaterialRoster(
              Mesh,
              CoreMaterialPaths,
              CoreMaterialSlotNames,
              UE_ARRAY_COUNT(CoreMaterialPaths),
              OutError)
        : ValidateMaterialRoster(
              Mesh,
              FallbackMaterialPaths,
              FallbackMaterialSlotNames,
              UE_ARRAY_COUNT(FallbackMaterialPaths),
              OutError);
}
} // namespace

bool UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
    ValidateCandidateMeshRoster(
        const FTRIADIstanaExploreV5DPublicRealmJunctionContinuityAssets& Assets,
        FString& OutError)
{
    if (Assets.CoreCandidateMesh == Assets.FallbackCandidateMesh ||
        !ValidateMeshBoundary(
            Assets.CoreCandidateMesh,
            CandidateCorePath,
            true,
            OutError) ||
        !ValidateMeshBoundary(
            Assets.FallbackCandidateMesh,
            CandidateFallbackPath,
            false,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("Public-realm junction candidates must be two distinct isolated render-only meshes.");
        }
        return false;
    }
    if (!bRuntimeCandidateSelectionCompiledAuthorized ||
        !bRuntimeCandidateActivationCompiledAuthorized)
    {
        OutError = TEXT("Public-realm junction candidate selection and activation are intentionally unreachable in dormant source; reviewed provenance gates, source changes, and a recompile are required.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
    ValidateExactFallbackMeshRoster(
        UStaticMesh* ExistingCoreMesh,
        UStaticMesh* ExistingFallbackMesh,
        FString& OutError)
{
    if (ExistingCoreMesh == ExistingFallbackMesh ||
        !ValidateMeshBoundary(
            ExistingCoreMesh, ExistingCorePath, true, OutError) ||
        !ValidateMeshBoundary(
            ExistingFallbackMesh, ExistingFallbackPath, false, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("The exact existing Core/Fallback public-realm pair is mandatory.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
    ResolveOptionalRenderMeshes(
        UStaticMesh* ExistingCoreMesh,
        UStaticMesh* ExistingFallbackMesh,
        const FTRIADIstanaExploreV5DPublicRealmJunctionContinuityAssets& CandidateAssets,
        bool bExplicitlySelectCandidate,
        FTRIADIstanaExploreV5DOptionalPublicRealmJunctionContinuity& OutSelection,
        FString& OutError)
{
    OutSelection =
        FTRIADIstanaExploreV5DOptionalPublicRealmJunctionContinuity{};
    if (!ValidateExactFallbackMeshRoster(
            ExistingCoreMesh, ExistingFallbackMesh, OutError) ||
        (bExplicitlySelectCandidate &&
         !ValidateCandidateMeshRoster(CandidateAssets, OutError)))
    {
        return false;
    }

    OutSelection.SelectedCoreMesh = bExplicitlySelectCandidate
        ? CandidateAssets.CoreCandidateMesh.Get()
        : ExistingCoreMesh;
    OutSelection.SelectedFallbackMesh = bExplicitlySelectCandidate
        ? CandidateAssets.FallbackCandidateMesh.Get()
        : ExistingFallbackMesh;
    OutSelection.bCandidateSelected = bExplicitlySelectCandidate;
    OutSelection.bExactCoreAndFallbackPreserved = true;
    OutSelection.bPositionUvTopologyGroupSlotTransformOrIdentityModified =
        false;
    OutSelection.bTerrainCollisionNavigationLosRfSensorOrSimulationAuthority =
        false;
    OutError.Reset();
    return true;
}

int32 UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
    ExpectedCandidateAssetCount()
{
    return CandidateAssetCount;
}

int32 UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
    ExpectedNormalOverrideCount()
{
    return NormalOverrideCount;
}

bool UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
    IsCandidateRuntimeSelectionCompiledAuthorized()
{
    return bRuntimeCandidateSelectionCompiledAuthorized;
}

bool UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
    IsCandidateRuntimeActivationCompiledAuthorized()
{
    return bRuntimeCandidateActivationCompiledAuthorized;
}

FString UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
    ExistingCoreMeshObjectPath()
{
    return ExistingCorePath;
}

FString UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
    ExistingFallbackMeshObjectPath()
{
    return ExistingFallbackPath;
}

FString UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
    CandidateCoreMeshObjectPath()
{
    return CandidateCorePath;
}

FString UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
    CandidateFallbackMeshObjectPath()
{
    return CandidateFallbackPath;
}

FString UTRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary::
    OutputNamespace()
{
    return OutputRoot;
}
