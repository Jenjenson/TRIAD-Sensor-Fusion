#include "TRIADIstanaExploreV5DR30FacadeLookdevActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"
#include "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.h"

namespace
{
const FString R29AssetRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsRealismR29"));
const FString R29MeshRoot(R29AssetRoot + TEXT("/Meshes"));
const FString R29MaterialRoot(R29AssetRoot + TEXT("/Materials"));
const FString R30AssetRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsLookdevR30"));
const FString R30MaterialRoot(R30AssetRoot + TEXT("/Materials"));
const FString R28AssetRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsRealismR28"));
const FString R28MeshRoot(R28AssetRoot + TEXT("/Meshes"));
const FString R28MaterialRoot(R28AssetRoot + TEXT("/Materials"));

FString ObjectPath(const FString& Root, const TCHAR* Name)
{
    return Root + TEXT("/") + Name + TEXT(".") + Name;
}

const FString R29FacadeMeshObjectPath(ObjectPath(
    R29MeshRoot, TEXT("SM_IPV5D_R29_ContextFacadeCoverage_Render")));
const FString R28PublicRealmMeshObjectPath(ObjectPath(
    R28MeshRoot, TEXT("SM_IPV5D_R28_ConnectivePublicRealm_Render")));
const FString OuterGroundMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/OuterGroundLoadingFallback/"
         "SM_IPV5D_OuterGroundLoadingFallback_Render."
         "SM_IPV5D_OuterGroundLoadingFallback_Render"));
const FString R28OuterGroundMaterialObjectPath(ObjectPath(
    R28MaterialRoot, TEXT("MI_IPV5D_R28_OuterGround")));
const FString R30ClaimLabel(
    TEXT("PUBLIC_REFERENCE_RENDER_ONLY_R30_TEXTURED_CONTEXT_FACADE_LOOKDEV_NOT_SURVEY_AS_BUILT_CURRENT_COMPLETE_PHYSICAL_MATERIAL_SIMULATION_SENSOR_RF_OR_GEOSPATIAL_AUTHORITY"));

const FName ActorTag(TEXT("TRIADIstanaExploreV5DR30FacadeLookdev"));
const FName PublicRealmTag(TEXT("TRIADV5DR30RetainedR28PublicRealmRenderOnly"));
const FName FacadeTag(TEXT("TRIADV5DR30ContextFacadeLookdevRenderOnly"));
const FName OuterGroundTag(TEXT("TRIADV5DR30RetainedR28OuterGroundRenderOnly"));
constexpr float OuterGroundRenderOffsetCentimetres = 0.5f;
constexpr float ProviderMirrorTickSeconds = 0.5f;

const TCHAR* const R29MaterialNames[] = {
    TEXT("MI_IPV5D_R29_GlassCool"),
    TEXT("MI_IPV5D_R29_GlassWarm"),
    TEXT("MI_IPV5D_R29_GlassNeutral"),
    TEXT("MI_IPV5D_R29_FrameLight"),
    TEXT("MI_IPV5D_R29_FrameDark"),
    TEXT("MI_IPV5D_R29_FrameBronze"),
    TEXT("MI_IPV5D_R29_SillLight"),
    TEXT("MI_IPV5D_R29_SillDark"),
    TEXT("MI_IPV5D_R29_RoofTrim"),
    TEXT("MI_IPV5D_R29_Canopy"),
    TEXT("MI_IPV5D_R29_BalconyRail")};

const TCHAR* const R30MaterialNames[] = {
    TEXT("MI_IPV5D_R30_GlassCool"),
    TEXT("MI_IPV5D_R30_GlassWarm"),
    TEXT("MI_IPV5D_R30_GlassNeutral"),
    TEXT("MI_IPV5D_R30_FrameLight"),
    TEXT("MI_IPV5D_R30_FrameDark"),
    TEXT("MI_IPV5D_R30_FrameBronze"),
    TEXT("MI_IPV5D_R30_SillLight"),
    TEXT("MI_IPV5D_R30_SillDark"),
    TEXT("MI_IPV5D_R30_RoofTrim"),
    TEXT("MI_IPV5D_R30_Canopy"),
    TEXT("MI_IPV5D_R30_BalconyRail")};

const TCHAR* const R28PublicRealmMaterialNames[] = {
    TEXT("MI_IPV5D_R28_Asphalt"),
    TEXT("MI_IPV5D_R28_Curb"),
    TEXT("MI_IPV5D_R28_Sidewalk"),
    TEXT("MI_IPV5D_R28_Verge")};

static_assert(UE_ARRAY_COUNT(R29MaterialNames) == 11);
static_assert(UE_ARRAY_COUNT(R30MaterialNames) == 11);
static_assert(UE_ARRAY_COUNT(R29MaterialNames) == UE_ARRAY_COUNT(R30MaterialNames));

template <typename ElementType>
bool SameSet(const TSet<ElementType>& A, const TSet<ElementType>& B)
{
    if (A.Num() != B.Num())
    {
        return false;
    }
    for (const ElementType& Value : A)
    {
        if (!B.Contains(Value))
        {
            return false;
        }
    }
    return true;
}

void ConfigureRenderer(
    UStaticMeshComponent* Component,
    const FName& ComponentTag,
    bool bCastShadow,
    const FVector& RelativeLocation = FVector::ZeroVector)
{
    if (!Component)
    {
        return;
    }
    Component->SetRelativeTransform(
        FTransform(FQuat::Identity, RelativeLocation));
    Component->SetMobility(EComponentMobility::Static);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(bCastShadow);
    Component->SetCastContactShadow(false);
    Component->SetAffectDistanceFieldLighting(false);
    Component->SetRenderInMainPass(true);
    Component->SetRenderCustomDepth(false);
    Component->bHiddenInSceneCapture = true;
    Component->EmptyOverrideMaterials();
    Component->SetOverlayMaterial(nullptr);
    Component->ComponentTags.AddUnique(ComponentTag);
}

void SetOwnedPresentationVisible(
    ATRIADIstanaExploreV5DR30FacadeLookdevActor* Actor,
    bool bVisible)
{
    if (!Actor)
    {
        return;
    }
    for (UStaticMeshComponent* Component : {
             Actor->RetainedR28ConnectivePublicRealmRenderOnly.Get(),
             Actor->R30ContextFacadeLookdevRenderOnly.Get(),
             Actor->RetainedR28OuterGroundOverlayRenderOnly.Get()})
    {
        if (Component)
        {
            Component->SetVisibility(bVisible, true);
            Component->SetHiddenInGame(!bVisible, true);
        }
    }
}

bool ValidateMeshMaterials(
    const UStaticMesh* Mesh,
    const FString& ExpectedPath,
    const FString& MaterialRoot,
    const TCHAR* const* ExpectedNames,
    int32 ExpectedCount,
    FString& OutError)
{
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
    if (!Mesh || Mesh->GetPathName() != ExpectedPath ||
        Mesh->GetStaticMaterials().Num() != ExpectedCount ||
        (Body && (Body->AggGeom.GetElementCount() != 0 ||
                  Body->CollisionTraceFlag == CTF_UseComplexAsSimple)))
    {
        OutError = TEXT("An R30-retained mesh lost its exact path, material-count, or no-collision contract.");
        return false;
    }
    TSet<FName> ExpectedSlots;
    TSet<FString> ExpectedPaths;
    for (int32 Index = 0; Index < ExpectedCount; ++Index)
    {
        ExpectedSlots.Add(FName(ExpectedNames[Index]));
        ExpectedPaths.Add(ObjectPath(MaterialRoot, ExpectedNames[Index]));
    }
    TSet<FName> ActualSlots;
    TSet<FString> ActualPaths;
    for (const FStaticMaterial& StaticMaterial : Mesh->GetStaticMaterials())
    {
        ActualSlots.Add(StaticMaterial.MaterialSlotName);
        if (StaticMaterial.MaterialInterface)
        {
            ActualPaths.Add(StaticMaterial.MaterialInterface->GetPathName());
        }
    }
    if (!SameSet(ActualSlots, ExpectedSlots) ||
        !SameSet(ActualPaths, ExpectedPaths))
    {
        OutError = TEXT("An R30-retained mesh lost its predecessor semantic material bindings.");
        return false;
    }
    return true;
}

bool ValidateFacadeOverrides(
    const UStaticMesh* Mesh,
    const TArray<TObjectPtr<UMaterialInterface>>& Overrides,
    FString& OutError)
{
    if (!Mesh || Overrides.Num() != UE_ARRAY_COUNT(R30MaterialNames))
    {
        OutError = TEXT("R30 requires exactly eleven facade lookdev overrides.");
        return false;
    }
    TSet<int32> SeenSlotIndices;
    for (int32 SpecIndex = 0;
         SpecIndex < UE_ARRAY_COUNT(R30MaterialNames);
         ++SpecIndex)
    {
        const UMaterialInterface* Override = Overrides[SpecIndex];
        const FString ExpectedPath = ObjectPath(
            R30MaterialRoot, R30MaterialNames[SpecIndex]);
        const int32 SlotIndex = Mesh->GetMaterialIndex(
            FName(R29MaterialNames[SpecIndex]));
        if (!Override || Override->GetPathName() != ExpectedPath ||
            SlotIndex == INDEX_NONE || SeenSlotIndices.Contains(SlotIndex))
        {
            OutError = TEXT("R30 facade override roster lost an exact material path or one-to-one semantic slot binding.");
            return false;
        }
        SeenSlotIndices.Add(SlotIndex);
    }
    return SeenSlotIndices.Num() == UE_ARRAY_COUNT(R30MaterialNames);
}

bool ValidateCommonRenderer(
    const UStaticMeshComponent* Component,
    const USceneComponent* Parent,
    const UStaticMesh* Mesh,
    const FName& RequiredTag,
    const FVector& RelativeLocation,
    bool bExpectedCastShadow,
    bool bExpectedVisible,
    int32 ExpectedOverrideCount,
    FString& OutError)
{
    if (!Component || Component->GetAttachParent() != Parent ||
        Component->GetStaticMesh() != Mesh ||
        !Component->GetRelativeTransform().Equals(
            FTransform(FQuat::Identity, RelativeLocation), 0.001) ||
        Component->Mobility != EComponentMobility::Static ||
        Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        Component->GetCollisionResponseToChannels() !=
            FCollisionResponseContainer(ECR_Ignore) ||
        Component->GetGenerateOverlapEvents() ||
        Component->CanEverAffectNavigation() ||
        Component->CastShadow != bExpectedCastShadow ||
        Component->bCastContactShadow || Component->bAffectDistanceFieldLighting ||
        !Component->bRenderInMainPass || Component->bRenderCustomDepth ||
        !Component->bHiddenInSceneCapture ||
        Component->GetNumOverrideMaterials() != ExpectedOverrideCount ||
        Component->GetOverlayMaterial() ||
        !Component->ComponentTags.Contains(RequiredTag) ||
        Component->IsVisible() != bExpectedVisible ||
        Component->bHiddenInGame == bExpectedVisible)
    {
        OutError = TEXT("An R30 facade-lookdev renderer lost its exact static, render-only, scene-capture-excluded contract.");
        return false;
    }
    return true;
}

bool ValidateFacadeRenderer(
    const UStaticMeshComponent* Component,
    const USceneComponent* Parent,
    const UStaticMesh* Mesh,
    const TArray<TObjectPtr<UMaterialInterface>>& Overrides,
    bool bExpectedVisible,
    FString& OutError)
{
    if (!ValidateCommonRenderer(
            Component, Parent, Mesh, FacadeTag, FVector::ZeroVector,
            true, bExpectedVisible, UE_ARRAY_COUNT(R30MaterialNames), OutError))
    {
        return false;
    }
    for (int32 SpecIndex = 0;
         SpecIndex < UE_ARRAY_COUNT(R30MaterialNames);
         ++SpecIndex)
    {
        const int32 SlotIndex = Mesh->GetMaterialIndex(
            FName(R29MaterialNames[SpecIndex]));
        if (SlotIndex == INDEX_NONE ||
            Component->GetMaterial(SlotIndex) != Overrides[SpecIndex])
        {
            OutError = TEXT("R30 facade renderer lost an exact slot-ordered material override.");
            return false;
        }
    }
    return true;
}

bool ResolveOwnerRosters(
    UWorld* World,
    const ATRIADIstanaExploreV5DR30FacadeLookdevActor* ExpectedR30,
    int32& OutR28ClassCount,
    int32& OutR28TagCount,
    int32& OutR29ClassCount,
    int32& OutR29TagCount,
    int32& OutR30ClassCount,
    int32& OutR30TagCount,
    int32& OutActiveR29FacadeRenderers,
    FString& OutError)
{
    OutR28ClassCount = 0;
    OutR28TagCount = 0;
    OutR29ClassCount = 0;
    OutR29TagCount = 0;
    OutR30ClassCount = 0;
    OutR30TagCount = 0;
    OutActiveR29FacadeRenderers = 0;
    if (!World)
    {
        OutError = TEXT("R30 facade-lookdev owner resolution requires a world.");
        return false;
    }
    const FName R29FacadeTag(TEXT("TRIADV5DR29ContextFacadeCoverageRenderOnly"));
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        const bool bExactR28 = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR28EnvironmentActor::StaticClass();
        const bool bTaggedR28 = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR28EnvironmentActor::ExpectedActorTag());
        const bool bExactR29 = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::StaticClass();
        const bool bTaggedR29 = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::ExpectedActorTag());
        const bool bExactR30 = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR30FacadeLookdevActor::StaticClass();
        const bool bTaggedR30 = Candidate && Candidate->Tags.Contains(ActorTag);
        OutR28ClassCount += bExactR28 ? 1 : 0;
        OutR28TagCount += bTaggedR28 ? 1 : 0;
        OutR29ClassCount += bExactR29 ? 1 : 0;
        OutR29TagCount += bTaggedR29 ? 1 : 0;
        OutR30ClassCount += bExactR30 ? 1 : 0;
        OutR30TagCount += bTaggedR30 ? 1 : 0;
        if ((bTaggedR28 && !bExactR28) || (bTaggedR29 && !bExactR29) ||
            (bTaggedR30 && !bExactR30))
        {
            OutError = TEXT("An R28/R29/R30 environment-owner tag is held by a non-exact class.");
            return false;
        }
        if (bExactR30 && Candidate != ExpectedR30)
        {
            OutError = TEXT("A second exact R30 facade-lookdev owner exists.");
            return false;
        }
        if (bExactR29)
        {
            TArray<UStaticMeshComponent*> Components;
            Candidate->GetComponents<UStaticMeshComponent>(Components);
            for (const UStaticMeshComponent* Component : Components)
            {
                if (Component && Component->ComponentTags.Contains(R29FacadeTag) &&
                    Component->IsVisible() && !Component->bHiddenInGame)
                {
                    ++OutActiveR29FacadeRenderers;
                }
            }
        }
    }
    return true;
}

bool ValidateState(
    const ATRIADIstanaExploreV5DR30FacadeLookdevActor* Actor,
    bool bPrepared,
    FString& OutReport)
{
    FString Error;
    int32 R28ClassCount = 0;
    int32 R28TagCount = 0;
    int32 R29ClassCount = 0;
    int32 R29TagCount = 0;
    int32 R30ClassCount = 0;
    int32 R30TagCount = 0;
    int32 ActiveR29FacadeRenderers = 0;
    const bool bExpectedVisible = !bPrepared && !Actor->bProviderReady;
    const bool bRosterValid = ResolveOwnerRosters(
        Actor ? Actor->GetWorld() : nullptr, Actor,
        R28ClassCount, R28TagCount,
        R29ClassCount, R29TagCount,
        R30ClassCount, R30TagCount,
        ActiveR29FacadeRenderers, Error);
    const bool bCountsValid = bPrepared
        ? (R28ClassCount == 0 && R28TagCount == 0 &&
           R29ClassCount == 1 && R29TagCount == 1 &&
           R30ClassCount == 1 && R30TagCount == 1 &&
           ActiveR29FacadeRenderers == (Actor && Actor->bProviderReady ? 0 : 1))
        : (R28ClassCount == 0 && R28TagCount == 0 &&
           R29ClassCount == 0 && R29TagCount == 0 &&
           R30ClassCount == 1 && R30TagCount == 1 &&
           ActiveR29FacadeRenderers == 0);
    if (!Actor || Actor->GetClass() !=
            ATRIADIstanaExploreV5DR30FacadeLookdevActor::StaticClass() ||
        !Actor->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        Actor->ClaimLabel != R30ClaimLabel || !Actor->bRenderOnly ||
        !Actor->bPredecessorMeshesAndTransformsRetainedUnmodified ||
        Actor->bR28OrR29ArchitectureRetainedOrCoRendered ||
        Actor->bCollisionNavigationSimulationSensorOrRfAuthority ||
        Actor->bSurveyAsBuiltCurrentCompleteOrPhysicalMaterialClaimed ||
        Actor->bExistingSimulationRfProviderCesiumOrGeospatialInputsModified ||
        Actor->bRuntimeGeometryGenerated ||
        !Actor->bProviderReadinessObservedWithoutPolicyMutation ||
        !Actor->PrimaryActorTick.bCanEverTick ||
        !Actor->PrimaryActorTick.bStartWithTickEnabled ||
        !FMath::IsNearlyEqual(
            Actor->PrimaryActorTick.TickInterval,
            ProviderMirrorTickSeconds,
            0.000001f) ||
        !Actor->bConfigured ||
        Actor->bReplacementActivated == bPrepared || !bRosterValid ||
        !bCountsValid || Actor->Tags.Num() != 1 ||
        !Actor->Tags.Contains(ActorTag) || !Actor->SceneRoot ||
        Actor->SceneRoot != Actor->GetRootComponent() ||
        Actor->SceneRoot->Mobility != EComponentMobility::Static ||
        !Actor->SceneRoot->GetRelativeTransform().Equals(
            FTransform::Identity, 0.001) ||
        !ATRIADIstanaExploreV5DR30FacadeLookdevActor::ValidateAssetRoster(
            Actor->SavedAssets, Error) ||
        !ValidateCommonRenderer(
            Actor->RetainedR28ConnectivePublicRealmRenderOnly,
            Actor->SceneRoot,
            Actor->SavedAssets.RetainedR28ConnectivePublicRealmMesh,
            PublicRealmTag,
            FVector::ZeroVector,
            false,
            bExpectedVisible,
            0,
            Error) ||
        !ValidateFacadeRenderer(
            Actor->R30ContextFacadeLookdevRenderOnly,
            Actor->SceneRoot,
            Actor->SavedAssets.RetainedR29ContextFacadeCoverageMesh,
            Actor->SavedAssets.ContextFacadeMaterialOverrides,
            bExpectedVisible,
            Error) ||
        !ValidateCommonRenderer(
            Actor->RetainedR28OuterGroundOverlayRenderOnly,
            Actor->SceneRoot,
            Actor->SavedAssets.RetainedOuterGroundMesh,
            OuterGroundTag,
            FVector(0.0, 0.0, OuterGroundRenderOffsetCentimetres),
            false,
            bExpectedVisible,
            1,
            Error) ||
        Actor->RetainedR28OuterGroundOverlayRenderOnly->GetMaterial(0) !=
            Actor->SavedAssets.RetainedR28OuterGroundMaterial)
    {
        OutReport = Error.IsEmpty()
            ? TEXT("R30 facade lookdev lost its exact handoff, material binding, component, asset, or negative-authority contract.")
            : Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R30_FACADE_LOOKDEV_%s providerReady=%s visible=%s ownedComponents=3 facadeMaterialOverrides=11 retainedR29FacadeMesh=true retainedR28PublicRealm=true retainedR28OuterGround=true r28Owners=0 r29Owners=%d r30Owners=1 mutuallyExclusiveArchitecture=true identity=true static=true renderOnly=true sceneCapture=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false cesiumModified=false geospatialInputsModified=false providerInputsModified=false providerReadinessObservedOnly=true providerMirrorHz=2 runtimeGeometry=false surveyClaim=false asBuiltClaim=false currentCompleteClaim=false physicalMaterialClaim=false visualCaptureAccepted=false captureRevalidationRequired=true."),
        bPrepared ? TEXT("HANDOFF_PREPARED") : TEXT("VALID"),
        Actor->bProviderReady ? TEXT("true") : TEXT("false"),
        bExpectedVisible ? TEXT("true") : TEXT("false"),
        R29ClassCount);
    return true;
}
} // namespace

ATRIADIstanaExploreV5DR30FacadeLookdevActor::
    ATRIADIstanaExploreV5DR30FacadeLookdevActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.TickInterval = ProviderMirrorTickSeconds;
    SetActorEnableCollision(false);
    ClaimLabel = ExpectedClaimLabel();

    SceneRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("R30FacadeLookdevRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);
    SceneRoot->SetRelativeTransform(FTransform::Identity);

    RetainedR28ConnectivePublicRealmRenderOnly =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("RetainedR28ConnectivePublicRealmRenderOnly"));
    R30ContextFacadeLookdevRenderOnly =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("R30ContextFacadeLookdevRenderOnly"));
    RetainedR28OuterGroundOverlayRenderOnly =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("RetainedR28OuterGroundOverlayRenderOnly"));
    RetainedR28ConnectivePublicRealmRenderOnly->SetupAttachment(SceneRoot);
    R30ContextFacadeLookdevRenderOnly->SetupAttachment(SceneRoot);
    RetainedR28OuterGroundOverlayRenderOnly->SetupAttachment(SceneRoot);
    ConfigureRenderer(
        RetainedR28ConnectivePublicRealmRenderOnly, PublicRealmTag, false);
    ConfigureRenderer(
        R30ContextFacadeLookdevRenderOnly, FacadeTag, true);
    ConfigureRenderer(
        RetainedR28OuterGroundOverlayRenderOnly,
        OuterGroundTag,
        false,
        FVector(0.0, 0.0, OuterGroundRenderOffsetCentimetres));
    SetOwnedPresentationVisible(this, false);
    Tags.AddUnique(ExpectedActorTag());
}

void ATRIADIstanaExploreV5DR30FacadeLookdevActor::BeginPlay()
{
    Super::BeginPlay();
    FString Error;
    if (!SynchronizeOwnedVisibilityWithContextPolicy(Error) &&
        !bLoggedRuntimeProviderMirrorFailure)
    {
        bLoggedRuntimeProviderMirrorFailure = true;
        UE_LOG(LogTemp, Error, TEXT("R30 facade provider mirror failed closed: %s"), *Error);
    }
}

void ATRIADIstanaExploreV5DR30FacadeLookdevActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FString Error;
    if (!SynchronizeOwnedVisibilityWithContextPolicy(Error))
    {
        if (!bLoggedRuntimeProviderMirrorFailure)
        {
            bLoggedRuntimeProviderMirrorFailure = true;
            UE_LOG(LogTemp, Error, TEXT("R30 facade provider mirror failed closed: %s"), *Error);
        }
        return;
    }
    bLoggedRuntimeProviderMirrorFailure = false;
}

void ATRIADIstanaExploreV5DR30FacadeLookdevActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (ATRIADIstanaExploreV5DContextPolicyActor* Policy =
            RuntimeContextPolicyActor.Get())
    {
        RemoveTickPrerequisiteActor(Policy);
    }
    RuntimeContextPolicyActor.Reset();
    Super::EndPlay(EndPlayReason);
}

bool ATRIADIstanaExploreV5DR30FacadeLookdevActor::
    SynchronizeOwnedVisibilityWithContextPolicy(FString& OutError)
{
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    int32 PolicyCount = 0;
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<ATRIADIstanaExploreV5DContextPolicyActor> It(World);
             It; ++It)
        {
            if ((*It)->GetClass() ==
                ATRIADIstanaExploreV5DContextPolicyActor::StaticClass())
            {
                ++PolicyCount;
                Policy = *It;
            }
        }
    }
    if (PolicyCount != 1 || !Policy || !bConfigured ||
        !bReplacementActivated)
    {
        if (ATRIADIstanaExploreV5DContextPolicyActor* Previous =
                RuntimeContextPolicyActor.Get())
        {
            RemoveTickPrerequisiteActor(Previous);
        }
        RuntimeContextPolicyActor.Reset();
        FString LocalFallbackReport;
        const bool bLocalFallbackRestored = bConfigured &&
            bReplacementActivated &&
            SetProviderReady(false, LocalFallbackReport);
        OutError = FString::Printf(
            TEXT("R30 provider mirror requires one exact context policy and one active configured successor; policies=%d configured=%s active=%s ownedLocalFallbackRestored=%s fallback={%s}."),
            PolicyCount,
            bConfigured ? TEXT("true") : TEXT("false"),
            bReplacementActivated ? TEXT("true") : TEXT("false"),
            bLocalFallbackRestored ? TEXT("true") : TEXT("false"),
            *LocalFallbackReport);
        return false;
    }
    if (RuntimeContextPolicyActor.Get() != Policy)
    {
        if (ATRIADIstanaExploreV5DContextPolicyActor* Previous =
                RuntimeContextPolicyActor.Get())
        {
            RemoveTickPrerequisiteActor(Previous);
        }
        RuntimeContextPolicyActor = Policy;
        AddTickPrerequisiteActor(Policy);
    }
    const bool bExpectedProviderReady =
        Policy->bLocalBuildingFallbackCurrentlyHidden;
    if (bProviderReady != bExpectedProviderReady &&
        !SetProviderReady(bExpectedProviderReady, OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR30FacadeLookdevActor::ValidateAssetRoster(
    const FTRIADIstanaExploreV5DR30FacadeLookdevAssets& Assets,
    FString& OutError)
{
    const UBodySetup* OuterBody = Assets.RetainedOuterGroundMesh
        ? Assets.RetainedOuterGroundMesh->GetBodySetup()
        : nullptr;
    if (!ValidateMeshMaterials(
            Assets.RetainedR28ConnectivePublicRealmMesh,
            R28PublicRealmMeshObjectPath,
            R28MaterialRoot,
            R28PublicRealmMaterialNames,
            UE_ARRAY_COUNT(R28PublicRealmMaterialNames),
            OutError) ||
        !ValidateMeshMaterials(
            Assets.RetainedR29ContextFacadeCoverageMesh,
            R29FacadeMeshObjectPath,
            R29MaterialRoot,
            R29MaterialNames,
            UE_ARRAY_COUNT(R29MaterialNames),
            OutError) ||
        !ValidateFacadeOverrides(
            Assets.RetainedR29ContextFacadeCoverageMesh,
            Assets.ContextFacadeMaterialOverrides,
            OutError) ||
        !Assets.RetainedOuterGroundMesh ||
        Assets.RetainedOuterGroundMesh->GetPathName() !=
            OuterGroundMeshObjectPath ||
        (OuterBody &&
         (OuterBody->AggGeom.GetElementCount() != 0 ||
          OuterBody->CollisionTraceFlag == CTF_UseComplexAsSimple)) ||
        !Assets.RetainedR28OuterGroundMaterial ||
        Assets.RetainedR28OuterGroundMaterial->GetPathName() !=
            R28OuterGroundMaterialObjectPath)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("R30 requires exact retained R29/R28 meshes plus exactly eleven isolated lookdev materials.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR30FacadeLookdevActor::
    ConfigurePreparedR30FacadeLookdevHandoff(
        const FTRIADIstanaExploreV5DR30FacadeLookdevAssets& InAssets,
        bool bInPredecessorProviderReady,
        FString& OutError)
{
    if (bConfigured || !GetWorld() ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !ValidateAssetRoster(InAssets, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("R30 facade-lookdev preparation requires one fresh identity actor and exact admitted assets.");
        }
        return false;
    }
    int32 R28ClassCount = 0;
    int32 R28TagCount = 0;
    int32 R29ClassCount = 0;
    int32 R29TagCount = 0;
    int32 R30ClassCount = 0;
    int32 R30TagCount = 0;
    int32 ActiveR29FacadeRenderers = 0;
    if (!ResolveOwnerRosters(
            GetWorld(), this,
            R28ClassCount, R28TagCount,
            R29ClassCount, R29TagCount,
            R30ClassCount, R30TagCount,
            ActiveR29FacadeRenderers, OutError) ||
        R28ClassCount != 0 || R28TagCount != 0 ||
        R29ClassCount != 1 || R29TagCount != 1 ||
        R30ClassCount != 1 || R30TagCount != 1 ||
        ActiveR29FacadeRenderers !=
            (bInPredecessorProviderReady ? 0 : 1))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("R30 preparation requires exactly one coherent R29 predecessor, no R28 owner, and this sole hidden R30 successor.");
        }
        return false;
    }
    for (TActorIterator<ATRIADIstanaExploreV5DR29FacadeEnvironmentActor> It(
             GetWorld()); It; ++It)
    {
        FString R29Report;
        if ((*It)->GetClass() !=
                ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::StaticClass() ||
            !(*It)->ValidateR29FacadeEnvironment(R29Report) ||
            (*It)->bProviderReady != bInPredecessorProviderReady)
        {
            OutError = TEXT("R30 preparation refused an invalid or provider-incoherent R29 predecessor: ") +
                R29Report;
            return false;
        }
    }

    SavedAssets = InAssets;
    RetainedR28ConnectivePublicRealmRenderOnly->SetStaticMesh(
        InAssets.RetainedR28ConnectivePublicRealmMesh);
    R30ContextFacadeLookdevRenderOnly->SetStaticMesh(
        InAssets.RetainedR29ContextFacadeCoverageMesh);
    RetainedR28OuterGroundOverlayRenderOnly->SetStaticMesh(
        InAssets.RetainedOuterGroundMesh);
    R30ContextFacadeLookdevRenderOnly->EmptyOverrideMaterials();
    for (int32 SpecIndex = 0;
         SpecIndex < UE_ARRAY_COUNT(R30MaterialNames);
         ++SpecIndex)
    {
        const int32 SlotIndex =
            InAssets.RetainedR29ContextFacadeCoverageMesh->GetMaterialIndex(
                FName(R29MaterialNames[SpecIndex]));
        R30ContextFacadeLookdevRenderOnly->SetMaterial(
            SlotIndex, InAssets.ContextFacadeMaterialOverrides[SpecIndex]);
    }
    RetainedR28OuterGroundOverlayRenderOnly->SetMaterial(
        0, InAssets.RetainedR28OuterGroundMaterial);
    bConfigured = true;
    bReplacementActivated = false;
    bProviderReady = bInPredecessorProviderReady;
    SetOwnedPresentationVisible(this, false);

    FString Report;
    if (!ValidatePreparedR30FacadeLookdevHandoff(Report))
    {
        RetainedR28ConnectivePublicRealmRenderOnly->SetStaticMesh(nullptr);
        R30ContextFacadeLookdevRenderOnly->SetStaticMesh(nullptr);
        R30ContextFacadeLookdevRenderOnly->EmptyOverrideMaterials();
        RetainedR28OuterGroundOverlayRenderOnly->SetStaticMesh(nullptr);
        RetainedR28OuterGroundOverlayRenderOnly->EmptyOverrideMaterials();
        SavedAssets = FTRIADIstanaExploreV5DR30FacadeLookdevAssets{};
        bConfigured = false;
        bProviderReady = false;
        OutError = TEXT("R30 facade-lookdev handoff preparation failed closed and cleared its hidden successor: ") +
            Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR30FacadeLookdevActor::
    ActivateAfterR29FacadeRemoval(FString& OutError)
{
    if (!bConfigured || bReplacementActivated)
    {
        OutError = TEXT("R30 activation requires one prepared, inactive successor.");
        return false;
    }
    int32 R28ClassCount = 0;
    int32 R28TagCount = 0;
    int32 R29ClassCount = 0;
    int32 R29TagCount = 0;
    int32 R30ClassCount = 0;
    int32 R30TagCount = 0;
    int32 ActiveR29FacadeRenderers = 0;
    if (!ResolveOwnerRosters(
            GetWorld(), this,
            R28ClassCount, R28TagCount,
            R29ClassCount, R29TagCount,
            R30ClassCount, R30TagCount,
            ActiveR29FacadeRenderers, OutError) ||
        R28ClassCount != 0 || R28TagCount != 0 ||
        R29ClassCount != 0 || R29TagCount != 0 ||
        R30ClassCount != 1 || R30TagCount != 1 ||
        ActiveR29FacadeRenderers != 0)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("R30 activation refused because an R28/R29 owner or active predecessor facade remains.");
        }
        return false;
    }
    bReplacementActivated = true;
    SetOwnedPresentationVisible(this, !bProviderReady);
    FString Report;
    if (!ValidateR30FacadeLookdev(Report))
    {
        SetOwnedPresentationVisible(this, false);
        bReplacementActivated = false;
        OutError = TEXT("R30 activation failed closed and returned to an all-hidden state: ") +
            Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR30FacadeLookdevActor::SetProviderReady(
    bool bInProviderReady,
    FString& OutError)
{
    if (!bConfigured || !bReplacementActivated)
    {
        OutError = TEXT("R30 provider readiness cannot change before completed predecessor replacement.");
        return false;
    }
    const bool bBefore = bProviderReady;
    bProviderReady = bInProviderReady;
    SetOwnedPresentationVisible(this, !bProviderReady);
    FString Report;
    if (!ValidateR30FacadeLookdev(Report))
    {
        bProviderReady = bBefore;
        SetOwnedPresentationVisible(this, !bProviderReady);
        OutError = TEXT("R30 provider visibility transition failed and was restored: ") +
            Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR30FacadeLookdevActor::
    ValidatePreparedR30FacadeLookdevHandoff(FString& OutReport) const
{
    return ValidateState(this, true, OutReport);
}

bool ATRIADIstanaExploreV5DR30FacadeLookdevActor::
    ValidateR30FacadeLookdev(FString& OutReport) const
{
    return ValidateState(this, false, OutReport);
}

const FString& ATRIADIstanaExploreV5DR30FacadeLookdevActor::
    ExpectedR29FacadeMeshObjectPath()
{
    return R29FacadeMeshObjectPath;
}

const FString& ATRIADIstanaExploreV5DR30FacadeLookdevActor::
    ExpectedR28PublicRealmMeshObjectPath()
{
    return R28PublicRealmMeshObjectPath;
}

const FString& ATRIADIstanaExploreV5DR30FacadeLookdevActor::
    ExpectedOuterGroundMeshObjectPath()
{
    return OuterGroundMeshObjectPath;
}

const FString& ATRIADIstanaExploreV5DR30FacadeLookdevActor::
    ExpectedR28OuterGroundMaterialObjectPath()
{
    return R28OuterGroundMaterialObjectPath;
}

const TArray<FName>& ATRIADIstanaExploreV5DR30FacadeLookdevActor::
    ExpectedR29MaterialSlotNames()
{
    static const TArray<FName> Names = []
    {
        TArray<FName> Result;
        for (const TCHAR* Name : R29MaterialNames)
        {
            Result.Add(FName(Name));
        }
        return Result;
    }();
    return Names;
}

const TArray<FString>& ATRIADIstanaExploreV5DR30FacadeLookdevActor::
    ExpectedR30MaterialObjectPaths()
{
    static const TArray<FString> Paths = []
    {
        TArray<FString> Result;
        for (const TCHAR* Name : R30MaterialNames)
        {
            Result.Add(ObjectPath(R30MaterialRoot, Name));
        }
        return Result;
    }();
    return Paths;
}

const FString& ATRIADIstanaExploreV5DR30FacadeLookdevActor::ExpectedClaimLabel()
{
    return R30ClaimLabel;
}

const FName& ATRIADIstanaExploreV5DR30FacadeLookdevActor::ExpectedActorTag()
{
    return ActorTag;
}

int32 ATRIADIstanaExploreV5DR30FacadeLookdevActor::ExpectedOwnedRendererCount()
{
    return 3;
}

int32 ATRIADIstanaExploreV5DR30FacadeLookdevActor::
    ExpectedFacadeMaterialOverrideCount()
{
    return UE_ARRAY_COUNT(R30MaterialNames);
}
