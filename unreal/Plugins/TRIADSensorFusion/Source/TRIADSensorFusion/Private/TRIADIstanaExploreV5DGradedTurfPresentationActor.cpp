#include "TRIADIstanaExploreV5DGradedTurfPresentationActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "TRIADIstanaExploreV5DGroundVegetationActor.h"
#include "TRIADIstanaExploreV5DR29VegetationActor.h"
#include "TRIADIstanaExploreV5DR32MediumDistanceTurfActor.h"

namespace
{
constexpr bool bRuntimeCandidateSelectionCompiledAuthorized = false;
constexpr bool bRuntimeCandidateActivationCompiledAuthorized = false;
constexpr int32 R29OwnedPlacementCount = 6144;
constexpr int32 R32OwnedPlacementCount = 4608;
constexpr int32 PresentationStartDistanceCm = 0;
constexpr int32 PresentationProofEndDistanceCm = 9500;
constexpr int32 BladeFadeStartDistanceCm = 6500;
constexpr int32 BladeFadeEndDistanceCm = 9000;
constexpr int32 SurfaceResponseFadeStartDistanceCm = 5000;
constexpr int32 SurfaceResponseFadeEndDistanceCm = 7000;

static_assert(!bRuntimeCandidateSelectionCompiledAuthorized);
static_assert(!bRuntimeCandidateActivationCompiledAuthorized);
static_assert(R29OwnedPlacementCount == 3072 * 2);
static_assert(R32OwnedPlacementCount == 18432 / 4);
static_assert(PresentationStartDistanceCm == 0);
static_assert(SurfaceResponseFadeStartDistanceCm < BladeFadeStartDistanceCm);
static_assert(BladeFadeStartDistanceCm < SurfaceResponseFadeEndDistanceCm);
static_assert(SurfaceResponseFadeEndDistanceCm < BladeFadeEndDistanceCm);
static_assert(BladeFadeEndDistanceCm < PresentationProofEndDistanceCm);

const FString OverlayComponentName(TEXT("V5DGroundMacroVariationOverlay"));
const FString OverlayMeshPath(
    TEXT("/Game/TRIAD/IstanaPublicView/Ground/"
         "SM_IstanaPublicView_Terrain.SM_IstanaPublicView_Terrain"));
const FString AcceptedLawnPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/GroundVegetation/Materials/"
         "M_IPV5D_LawnMacroVariation.M_IPV5D_LawnMacroVariation"));
const FString OutputRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
         "GradedTurfPresentationIntegrationV2"));
const FString CandidateName(TEXT("M_IPV5D_GradedTurfPresentationV2"));
const FString CandidatePath(
    OutputRoot + TEXT("/Materials/") + CandidateName + TEXT(".") +
    CandidateName);

const TCHAR* const Grass001TexturePaths[] = {
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
         "OrdinaryDistanceGrassSurfaceIntegration/Textures/"
         "T_IPV5D_Grass001_BaseColor.T_IPV5D_Grass001_BaseColor"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
         "OrdinaryDistanceGrassSurfaceIntegration/Textures/"
         "T_IPV5D_Grass001_NormalDX.T_IPV5D_Grass001_NormalDX"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
         "OrdinaryDistanceGrassSurfaceIntegration/Textures/"
         "T_IPV5D_Grass001_Roughness.T_IPV5D_Grass001_Roughness"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
         "OrdinaryDistanceGrassSurfaceIntegration/Textures/"
         "T_IPV5D_Grass001_AmbientOcclusion."
         "T_IPV5D_Grass001_AmbientOcclusion"),
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
         "OrdinaryDistanceGrassSurfaceIntegration/Textures/"
         "T_IPV5D_Grass001_Height.T_IPV5D_Grass001_Height")};

bool IsExactMaterialPath(
    const UMaterialInterface* Material,
    const FString& ExpectedPath)
{
    return IsValid(Material) && Material->GetPathName() == ExpectedPath;
}

bool HasExactGrass001TextureRoster(const UMaterial* Material)
{
    if (!Material)
    {
        return false;
    }
    TSet<FString> Actual;
    for (const TObjectPtr<UObject>& Object : Material->GetReferencedTextures())
    {
        if (Object)
        {
            Actual.Add(Object->GetPathName());
        }
    }
    if (Actual.Num() != UE_ARRAY_COUNT(Grass001TexturePaths))
    {
        return false;
    }
    for (const TCHAR* Expected : Grass001TexturePaths)
    {
        if (!Actual.Contains(Expected))
        {
            return false;
        }
    }
    return true;
}

bool ValidateCandidateMaterialRuntime(
    UMaterialInterface* Candidate,
    FString& OutError)
{
    UMaterial* Material = Cast<UMaterial>(Candidate);
    if (!Material || Material->GetClass() != UMaterial::StaticClass() ||
        Material->GetPathName() != CandidatePath ||
        Material->MaterialDomain != MD_Surface ||
        Material->BlendMode != BLEND_Masked || Material->TwoSided ||
        !Material->bTangentSpaceNormal || Material->bUseMaterialAttributes ||
        Material->bEnableTessellation || Material->bEnableDisplacementFade ||
        Material->MaxWorldPositionOffsetDisplacement != 0.0f ||
        !Material->GetShadingModels().HasOnlyShadingModel(MSM_DefaultLit) ||
        !Material->HasBaseColorConnected() ||
        !Material->HasRoughnessConnected() || !Material->HasNormalConnected() ||
        !Material->HasAmbientOcclusionConnected() ||
        !Material->IsPropertyConnected(MP_OpacityMask) ||
        Material->IsPropertyConnected(MP_WorldPositionOffset) ||
        Material->IsPropertyConnected(MP_PixelDepthOffset) ||
        Material->IsPropertyConnected(MP_Displacement) ||
        !HasExactGrass001TextureRoster(Material))
    {
        OutError = TEXT("The graded-turf candidate lost its exact Masked, five-texture, four-response, no-WPO/no-displacement/no-PDO runtime boundary.");
        return false;
    }
    OutError.Reset();
    return true;
}

int32 CountInstances(
    const TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>&
        Components)
{
    int32 Total = 0;
    for (const UHierarchicalInstancedStaticMeshComponent* Component : Components)
    {
        if (!Component)
        {
            return INDEX_NONE;
        }
        Total += Component->GetInstanceCount();
    }
    return Total;
}

bool ValidatePresentationOwners(
    const ATRIADIstanaExploreV5DR29VegetationActor* R29,
    const ATRIADIstanaExploreV5DR32MediumDistanceTurfActor* R32,
    const ATRIADIstanaExploreV5DGroundVegetationActor* Ground,
    FString& OutError)
{
    FString R29Report;
    FString R32Report;
    if (!R29 || R29->GetClass() !=
            ATRIADIstanaExploreV5DR29VegetationActor::StaticClass() ||
        !R29->ValidateR29Vegetation(R29Report) ||
        R29->SavedLayout.GrassTotal() != R29OwnedPlacementCount ||
        R29->GrassComponents.Num() != 12 ||
        CountInstances(R29->GrassComponents) != R29OwnedPlacementCount)
    {
        OutError = TEXT("The exact R29 twelve-bucket/6,144-placement presentation owner is mandatory: ") + R29Report;
        return false;
    }
    if (!R32 || R32->GetClass() !=
            ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::StaticClass() ||
        R32->SourceGroundVegetation != Ground ||
        !R32->ValidateR32MediumDistanceTurf(R32Report) ||
        R32->SavedLayout.Total() != R32OwnedPlacementCount ||
        R32->GrassComponents.Num() != 12 ||
        CountInstances(R32->GrassComponents) != R32OwnedPlacementCount ||
        ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
                ExpectedCullStartDistanceCm() != BladeFadeStartDistanceCm ||
        ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
                ExpectedCullEndDistanceCm() != BladeFadeEndDistanceCm)
    {
        OutError = TEXT("The exact R32 twelve-bucket/4,608-placement 65-90 m presentation owner is mandatory: ") + R32Report;
        return false;
    }
    TSet<const UActorComponent*> UniqueComponents;
    for (const UHierarchicalInstancedStaticMeshComponent* Component :
         R29->GrassComponents)
    {
        UniqueComponents.Add(Component);
    }
    for (const UHierarchicalInstancedStaticMeshComponent* Component :
         R32->GrassComponents)
    {
        UniqueComponents.Add(Component);
    }
    if (UniqueComponents.Num() != 24)
    {
        OutError = TEXT("R29 and R32 must retain 24 distinct owned grass components; graded turf cannot merge, move, or steal placement ownership.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool SnapshotArraysEqual(
    const TArray<TWeakObjectPtr<UMaterialInterface>>& Expected,
    const TArray<TObjectPtr<UMaterialInterface>>& Actual)
{
    if (Expected.Num() != Actual.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < Expected.Num(); ++Index)
    {
        if (Expected[Index].Get() != Actual[Index])
        {
            return false;
        }
    }
    return true;
}
} // namespace

ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ATRIADIstanaExploreV5DGradedTurfPresentationActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;
    SetActorEnableCollision(false);
    Tags.AddUnique(TEXT("TRIAD_IstanaExploreV5D_GradedTurfPresentationV2_Dormant"));
}

bool ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ValidateDormantPresentationInputs(
        const ATRIADIstanaExploreV5DGroundVegetationActor* GroundVegetation,
        const ATRIADIstanaExploreV5DR29VegetationActor* R29Vegetation,
        const ATRIADIstanaExploreV5DR32MediumDistanceTurfActor* R32Turf,
        UMaterialInterface* ExactAcceptedLawnFallback,
        UMaterialInterface* GradedTurfCandidate,
        FString& OutReport)
{
    FString Error;
    const UStaticMeshComponent* Overlay = GroundVegetation
        ? GroundVegetation->GroundMacroVariationOverlay.Get()
        : nullptr;
    if (!GroundVegetation || GroundVegetation->GetClass() !=
            ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass() ||
        !Overlay || Overlay->GetOwner() != GroundVegetation ||
        Overlay->GetName() != OverlayComponentName ||
        !Overlay->GetStaticMesh() ||
        Overlay->GetStaticMesh()->GetPathName() != OverlayMeshPath ||
        Overlay->GetMaterial(0) != ExactAcceptedLawnFallback ||
        !IsExactMaterialPath(ExactAcceptedLawnFallback, AcceptedLawnPath) ||
        Overlay->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        Overlay->CanEverAffectNavigation() || !Overlay->IsVisible() ||
        Overlay->bHiddenInGame || Overlay->CastShadow ||
        Overlay->bCastContactShadow ||
        Overlay->bAffectDistanceFieldLighting)
    {
        OutReport = TEXT("The exact visible render-only V5DGroundMacroVariationOverlay and accepted R23B masked-lawn fallback are mandatory.");
        return false;
    }
    if (!ValidateCandidateMaterialRuntime(GradedTurfCandidate, Error) ||
        !ValidatePresentationOwners(
            R29Vegetation, R32Turf, GroundVegetation, Error))
    {
        OutReport = Error;
        return false;
    }
    if (bRuntimeCandidateSelectionCompiledAuthorized ||
        bRuntimeCandidateActivationCompiledAuthorized)
    {
        OutReport = TEXT("Dormant graded-turf source unexpectedly enabled runtime selection or activation.");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_GRADED_TURF_PRESENTATION_V2_DORMANT_VALID exactOverlay=V5DGroundMacroVariationOverlay acceptedFallback=R23B_Masked mask=64PointEstateBoundary_50mCore_8mFeather candidateResponse=Grass001DualPhase r29OwnedPlacements=%d r32OwnedPlacements=%d presentationProofMeters=0-95 bladeFadeMeters=65-90 surfaceResponseFadeMeters=50-70 fallbackRestoration=exactFullOverrideArray runtimeSelectionCompiled=false runtimeActivationCompiled=false appearanceOnly=true mapGeographyTerrainPlacementCollisionNavigationLosRfSensorSimulationModified=false"),
        R29OwnedPlacementCount,
        R32OwnedPlacementCount);
    return true;
}

bool ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    SnapshotGroundMacroVariationOverlayInternal(
        ATRIADIstanaExploreV5DGroundVegetationActor* GroundVegetation,
        FString& OutError)
{
    if (GroundOverlaySnapshot.bValid)
    {
        OutError = TEXT("A graded-turf overlay snapshot is already active; nested ownership is refused.");
        return false;
    }
    UStaticMeshComponent* Component = GroundVegetation
        ? GroundVegetation->GroundMacroVariationOverlay.Get()
        : nullptr;
    if (!Component || Component->GetOwner() != GroundVegetation ||
        Component->GetName() != OverlayComponentName ||
        !Component->GetStaticMesh() ||
        Component->GetStaticMesh()->GetPathName() != OverlayMeshPath ||
        !IsExactMaterialPath(Component->GetMaterial(0), AcceptedLawnPath))
    {
        OutError = TEXT("Snapshot refused anything except the exact accepted V5D ground-macro overlay and fallback binding.");
        return false;
    }

    FGroundOverlaySnapshot Snapshot;
    Snapshot.Owner = GroundVegetation;
    Snapshot.Component = Component;
    Snapshot.AttachParent = Component->GetAttachParent();
    Snapshot.StaticMesh = Component->GetStaticMesh();
    Snapshot.ResolvedSlotZeroMaterial = Component->GetMaterial(0);
    for (UMaterialInterface* Material : Component->OverrideMaterials)
    {
        Snapshot.OverrideMaterials.Add(Material);
    }
    Snapshot.ComponentTags = Component->ComponentTags;
    Snapshot.AttachSocketName = Component->GetAttachSocketName();
    Snapshot.RelativeTransform = Component->GetRelativeTransform();
    Snapshot.WorldTransform = Component->GetComponentTransform();
    Snapshot.Mobility = Component->Mobility;
    Snapshot.CollisionEnabled = Component->GetCollisionEnabled();
    Snapshot.bCanEverAffectNavigation = Component->CanEverAffectNavigation();
    Snapshot.bVisible = Component->IsVisible();
    Snapshot.bHiddenInGame = Component->bHiddenInGame;
    Snapshot.bActive = Component->IsActive();
    Snapshot.bCastShadow = Component->CastShadow;
    Snapshot.bCastContactShadow = Component->bCastContactShadow;
    Snapshot.bAffectDistanceFieldLighting =
        Component->bAffectDistanceFieldLighting;
    Snapshot.bRenderInMainPass = Component->bRenderInMainPass;
    Snapshot.bRenderCustomDepth = Component->bRenderCustomDepth;
    Snapshot.bValid = true;
    GroundOverlaySnapshot = MoveTemp(Snapshot);
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ApplyGradedTurfMaterialInternal(
        UMaterialInterface* GradedTurfCandidate,
        FString& OutError)
{
    if (!bRuntimeCandidateSelectionCompiledAuthorized)
    {
        OutError = TEXT("Graded-turf runtime candidate selection is compiled false.");
        return false;
    }
    if (!bRuntimeCandidateActivationCompiledAuthorized)
    {
        OutError = TEXT("Graded-turf runtime candidate activation is independently compiled false.");
        return false;
    }
    if (!GroundOverlaySnapshot.bValid ||
        !ValidateCandidateMaterialRuntime(GradedTurfCandidate, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("A complete exact fallback snapshot is mandatory before graded-turf apply.");
        }
        return false;
    }
    UStaticMeshComponent* Component = GroundOverlaySnapshot.Component.Get();
    if (!Component || Component->GetMaterial(0) !=
            GroundOverlaySnapshot.ResolvedSlotZeroMaterial.Get() ||
        !ValidateSnapshotBoundaryInternal(false, nullptr, OutError))
    {
        return false;
    }

    // This is the sole prospective environment mutation in the entire
    // runtime scaffold, unreachable while either independent gate is false.
    Component->SetMaterial(0, GradedTurfCandidate);
    if (!ValidateSnapshotBoundaryInternal(
            true, GradedTurfCandidate, OutError))
    {
        FString RestoreError;
        RestoreGroundMacroVariationOverlayInternal(RestoreError);
        OutError += TEXT(" restore=") + RestoreError;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ValidateSnapshotBoundaryInternal(
        bool bExpectCandidateInSlotZero,
        UMaterialInterface* GradedTurfCandidate,
        FString& OutError) const
{
    const FGroundOverlaySnapshot& Snapshot = GroundOverlaySnapshot;
    UStaticMeshComponent* Component = Snapshot.Component.Get();
    if (!Snapshot.bValid || !Component ||
        Component->GetOwner() != Snapshot.Owner.Get() ||
        Component->GetAttachParent() != Snapshot.AttachParent.Get() ||
        Component->GetAttachSocketName() != Snapshot.AttachSocketName ||
        Component->GetStaticMesh() != Snapshot.StaticMesh.Get() ||
        Component->GetRelativeTransform().Equals(
            Snapshot.RelativeTransform, 0.0f) == false ||
        Component->GetComponentTransform().Equals(
            Snapshot.WorldTransform, 0.0f) == false ||
        Component->Mobility != Snapshot.Mobility ||
        Component->GetCollisionEnabled() != Snapshot.CollisionEnabled ||
        Component->CanEverAffectNavigation() !=
            Snapshot.bCanEverAffectNavigation ||
        Component->IsVisible() != Snapshot.bVisible ||
        Component->bHiddenInGame != Snapshot.bHiddenInGame ||
        Component->IsActive() != Snapshot.bActive ||
        Component->CastShadow != Snapshot.bCastShadow ||
        Component->bCastContactShadow != Snapshot.bCastContactShadow ||
        Component->bAffectDistanceFieldLighting !=
            Snapshot.bAffectDistanceFieldLighting ||
        Component->bRenderInMainPass != Snapshot.bRenderInMainPass ||
        Component->bRenderCustomDepth != Snapshot.bRenderCustomDepth ||
        Component->ComponentTags != Snapshot.ComponentTags)
    {
        OutError = TEXT("V5DGroundMacroVariationOverlay changed outside its single prospective slot-0 material override.");
        return false;
    }

    if (bExpectCandidateInSlotZero)
    {
        if (!GradedTurfCandidate ||
            Component->GetMaterial(0) != GradedTurfCandidate ||
            Component->OverrideMaterials.Num() !=
                FMath::Max(1, Snapshot.OverrideMaterials.Num()))
        {
            OutError = TEXT("The active graded-turf boundary lost its exact sole slot-0 candidate override.");
            return false;
        }
        for (int32 Index = 1; Index < Snapshot.OverrideMaterials.Num(); ++Index)
        {
            if (Component->OverrideMaterials[Index] !=
                Snapshot.OverrideMaterials[Index].Get())
            {
                OutError = TEXT("A non-zero ground-overlay material slot changed during graded-turf apply.");
                return false;
            }
        }
    }
    else if (!SnapshotArraysEqual(
                 Snapshot.OverrideMaterials, Component->OverrideMaterials) ||
             Component->GetMaterial(0) !=
                 Snapshot.ResolvedSlotZeroMaterial.Get())
    {
        OutError = TEXT("The exact accepted lawn fallback override array was not preserved or restored.");
        return false;
    }

    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    RestoreGroundMacroVariationOverlayInternal(FString& OutError)
{
    if (!GroundOverlaySnapshot.bValid)
    {
        OutError = TEXT("No graded-turf snapshot exists to restore.");
        return false;
    }
    UStaticMeshComponent* Component = GroundOverlaySnapshot.Component.Get();
    if (!Component || Component->GetOwner() != GroundOverlaySnapshot.Owner.Get())
    {
        OutError = TEXT("Exact ground-overlay object identity was lost; restoration refused.");
        return false;
    }

    Component->EmptyOverrideMaterials();
    for (int32 Index = 0;
         Index < GroundOverlaySnapshot.OverrideMaterials.Num();
         ++Index)
    {
        Component->SetMaterial(
            Index, GroundOverlaySnapshot.OverrideMaterials[Index].Get());
    }
    if (!ValidateSnapshotBoundaryInternal(false, nullptr, OutError))
    {
        return false;
    }
    GroundOverlaySnapshot = FGroundOverlaySnapshot{};
    OutError = TEXT("EXACT_V5D_GROUND_MACRO_VARIATION_OVERLAY_FALLBACK_RESTORED");
    return true;
}

bool ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    IsRuntimeCandidateSelectionCompiledAuthorized()
{
    return bRuntimeCandidateSelectionCompiledAuthorized;
}

bool ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    IsRuntimeCandidateActivationCompiledAuthorized()
{
    return bRuntimeCandidateActivationCompiledAuthorized;
}

int32 ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ExpectedR29OwnedPlacementCount()
{
    return R29OwnedPlacementCount;
}

int32 ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ExpectedR32OwnedPlacementCount()
{
    return R32OwnedPlacementCount;
}

int32 ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ExpectedPresentationStartDistanceCm()
{
    return PresentationStartDistanceCm;
}

int32 ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ExpectedPresentationProofEndDistanceCm()
{
    return PresentationProofEndDistanceCm;
}

int32 ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ExpectedBladeFadeStartDistanceCm()
{
    return BladeFadeStartDistanceCm;
}

int32 ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ExpectedBladeFadeEndDistanceCm()
{
    return BladeFadeEndDistanceCm;
}

int32 ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ExpectedSurfaceResponseFadeStartDistanceCm()
{
    return SurfaceResponseFadeStartDistanceCm;
}

int32 ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ExpectedSurfaceResponseFadeEndDistanceCm()
{
    return SurfaceResponseFadeEndDistanceCm;
}

FString ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ExactOverlayComponentName()
{
    return OverlayComponentName;
}

FString ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ExactOverlayMeshObjectPath()
{
    return OverlayMeshPath;
}

FString ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    ExactAcceptedLawnMaterialObjectPath()
{
    return AcceptedLawnPath;
}

FString ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    CandidateMaterialObjectPath()
{
    return CandidatePath;
}

FString ATRIADIstanaExploreV5DGradedTurfPresentationActor::
    CandidateOutputNamespace()
{
    return OutputRoot;
}
