#include "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"

namespace
{
const FString R29AssetRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsRealismR29"));
const FString R29MeshRoot(R29AssetRoot + TEXT("/Meshes"));
const FString R29MaterialRoot(R29AssetRoot + TEXT("/Materials"));
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
const FString R28ArchitectureMeshObjectPath(ObjectPath(
    R28MeshRoot, TEXT("SM_IPV5D_R28_ContextArchitecturalDressing_Render")));
const FString R28PublicRealmMeshObjectPath(ObjectPath(
    R28MeshRoot, TEXT("SM_IPV5D_R28_ConnectivePublicRealm_Render")));
const FString OuterGroundMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/OuterGroundLoadingFallback/"
         "SM_IPV5D_OuterGroundLoadingFallback_Render."
         "SM_IPV5D_OuterGroundLoadingFallback_Render"));
const FString R28OuterGroundMaterialObjectPath(ObjectPath(
    R28MaterialRoot, TEXT("MI_IPV5D_R28_OuterGround")));
const FString R29FacadeClaimLabel(
    TEXT("PUBLIC_SOURCE_ALIGNED_RENDER_ONLY_R29_FACADE_CUES_NOT_SURVEY_AS_BUILT_CURRENT_COMPLETE_PHYSICAL_MATERIAL_SIMULATION_SENSOR_OR_RF_AUTHORITY"));

const FName ActorTag(TEXT("TRIADIstanaExploreV5DR29FacadeEnvironment"));
const FName PublicRealmTag(TEXT("TRIADV5DR29RetainedR28PublicRealmRenderOnly"));
const FName FacadeTag(TEXT("TRIADV5DR29ContextFacadeCoverageRenderOnly"));
const FName OuterGroundTag(TEXT("TRIADV5DR29RetainedR28OuterGroundRenderOnly"));
constexpr float OuterGroundRenderOffsetCentimetres = 0.5f;

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
const TCHAR* const R28PublicRealmMaterialNames[] = {
    TEXT("MI_IPV5D_R28_Asphalt"),
    TEXT("MI_IPV5D_R28_Curb"),
    TEXT("MI_IPV5D_R28_Sidewalk"),
    TEXT("MI_IPV5D_R28_Verge")};

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
    ATRIADIstanaExploreV5DR29FacadeEnvironmentActor* Actor,
    bool bVisible)
{
    if (!Actor)
    {
        return;
    }
    for (UStaticMeshComponent* Component : {
             Actor->RetainedR28ConnectivePublicRealmRenderOnly.Get(),
             Actor->R29ContextFacadeCoverageRenderOnly.Get(),
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
        OutError = TEXT("An R29 facade environment mesh lost its exact object-path, material-count, or no-collision contract.");
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
        OutError = TEXT("An R29 facade environment mesh lost its isolated semantic material bindings.");
        return false;
    }
    return true;
}

bool ValidateRenderer(
    const UStaticMeshComponent* Component,
    const USceneComponent* Parent,
    const UStaticMesh* Mesh,
    const FName& RequiredTag,
    const FVector& RelativeLocation,
    bool bExpectedCastShadow,
    bool bExpectedVisible,
    const UMaterialInterface* ExpectedOverride,
    FString& OutError)
{
    const int32 ExpectedOverrideCount = ExpectedOverride ? 1 : 0;
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
        Component->bHiddenInGame == bExpectedVisible ||
        (ExpectedOverride && Component->GetMaterial(0) != ExpectedOverride))
    {
        OutError = TEXT("An R29 facade renderer lost its exact static, render-only, scene-capture-excluded contract.");
        return false;
    }
    return true;
}

bool ResolveOwnerRosters(
    UWorld* World,
    const ATRIADIstanaExploreV5DR29FacadeEnvironmentActor* ExpectedR29,
    int32& OutR28ClassCount,
    int32& OutR28TagCount,
    int32& OutR29ClassCount,
    int32& OutR29TagCount,
    int32& OutActiveR28ArchitectureRenderers,
    FString& OutError)
{
    OutR28ClassCount = 0;
    OutR28TagCount = 0;
    OutR29ClassCount = 0;
    OutR29TagCount = 0;
    OutActiveR28ArchitectureRenderers = 0;
    if (!World)
    {
        OutError = TEXT("R29 facade owner resolution requires a world.");
        return false;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        const bool bExactR28 = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR28EnvironmentActor::StaticClass();
        const bool bTaggedR28 = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR28EnvironmentActor::ExpectedActorTag());
        const bool bExactR29 = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::StaticClass();
        const bool bTaggedR29 = Candidate && Candidate->Tags.Contains(ActorTag);
        OutR28ClassCount += bExactR28 ? 1 : 0;
        OutR28TagCount += bTaggedR28 ? 1 : 0;
        OutR29ClassCount += bExactR29 ? 1 : 0;
        OutR29TagCount += bTaggedR29 ? 1 : 0;
        if ((bTaggedR28 && !bExactR28) || (bTaggedR29 && !bExactR29))
        {
            OutError = TEXT("An R28/R29 facade owner tag is held by a non-exact class.");
            return false;
        }
        if (bExactR29 && Candidate != ExpectedR29)
        {
            OutError = TEXT("A second exact R29 facade environment owner exists.");
            return false;
        }
        TArray<UStaticMeshComponent*> Components;
        Candidate->GetComponents<UStaticMeshComponent>(Components);
        for (const UStaticMeshComponent* Component : Components)
        {
            if (Component && Component->GetStaticMesh() &&
                Component->GetStaticMesh()->GetPathName() ==
                    R28ArchitectureMeshObjectPath &&
                Component->IsVisible() && !Component->bHiddenInGame)
            {
                ++OutActiveR28ArchitectureRenderers;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateState(
    const ATRIADIstanaExploreV5DR29FacadeEnvironmentActor* Actor,
    bool bPrepared,
    FString& OutReport)
{
    FString Error;
    int32 R28ClassCount = 0;
    int32 R28TagCount = 0;
    int32 R29ClassCount = 0;
    int32 R29TagCount = 0;
    int32 ActiveR28ArchitectureRenderers = 0;
    const bool bExpectedVisible = !bPrepared && !Actor->bProviderReady;
    const int32 ExpectedR28Count = bPrepared ? 1 : 0;
    const int32 ExpectedActiveR28Architecture =
        bPrepared && !Actor->bProviderReady ? 1 : 0;
    if (!Actor->bConfigured || Actor->bReplacementActivated == bPrepared ||
        !Actor->GetWorld() || !Actor->Tags.Contains(ActorTag) ||
        Actor->ClaimLabel != R29FacadeClaimLabel || !Actor->bRenderOnly ||
        !Actor->bR28PublicRealmRetainedUnmodified ||
        Actor->bR28ArchitectureRetainedOrCoRendered ||
        Actor->bCollisionNavigationSimulationSensorOrRfAuthority ||
        Actor->bSurveyAsBuiltCurrentCompleteOrPhysicalMaterialClaimed ||
        Actor->bExistingSimulationRfProviderOrGeospatialInputsModified ||
        Actor->bRuntimeGeometryGenerated ||
        Actor->PrimaryActorTick.bCanEverTick ||
        Actor->GetActorEnableCollision() ||
        !Actor->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !Actor->SceneRoot || Actor->SceneRoot != Actor->GetRootComponent() ||
        Actor->SceneRoot->Mobility != EComponentMobility::Static ||
        !Actor->SceneRoot->GetRelativeTransform().Equals(
            FTransform::Identity, 0.001) ||
        !ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::ValidateAssetRoster(
            Actor->SavedAssets, Error) ||
        !ResolveOwnerRosters(
            Actor->GetWorld(), Actor,
            R28ClassCount, R28TagCount,
            R29ClassCount, R29TagCount,
            ActiveR28ArchitectureRenderers, Error) ||
        R28ClassCount != ExpectedR28Count ||
        R28TagCount != ExpectedR28Count ||
        R29ClassCount != 1 || R29TagCount != 1 ||
        ActiveR28ArchitectureRenderers != ExpectedActiveR28Architecture ||
        !ValidateRenderer(
            Actor->RetainedR28ConnectivePublicRealmRenderOnly,
            Actor->SceneRoot,
            Actor->SavedAssets.RetainedR28ConnectivePublicRealmMesh,
            PublicRealmTag,
            FVector::ZeroVector,
            false,
            bExpectedVisible,
            nullptr,
            Error) ||
        !ValidateRenderer(
            Actor->R29ContextFacadeCoverageRenderOnly,
            Actor->SceneRoot,
            Actor->SavedAssets.ContextFacadeCoverageMesh,
            FacadeTag,
            FVector::ZeroVector,
            true,
            bExpectedVisible,
            nullptr,
            Error) ||
        !ValidateRenderer(
            Actor->RetainedR28OuterGroundOverlayRenderOnly,
            Actor->SceneRoot,
            Actor->SavedAssets.RetainedOuterGroundMesh,
            OuterGroundTag,
            FVector(0.0, 0.0, OuterGroundRenderOffsetCentimetres),
            false,
            bExpectedVisible,
            Actor->SavedAssets.RetainedR28OuterGroundMaterial,
            Error))
    {
        OutReport = Error.IsEmpty()
            ? TEXT("R29 facade environment lost its exact handoff, owner-roster, component, asset, or negative-authority contract.")
            : Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R29_FACADE_%s providerReady=%s visible=%s ownedComponents=3 retainedR28PublicRealm=true retainedR28OuterGround=true r28ArchitectureComponentsOwned=0 r28ArchitectureActiveRenderers=%d r29FacadeMesh=true mutuallyExclusiveArchitecture=true identity=true static=true renderOnly=true sceneCapture=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false geospatialInputsModified=false providerInputsModified=false runtimeGeometry=false surveyClaim=false asBuiltClaim=false currentCompleteClaim=false physicalMaterialClaim=false visualCaptureAccepted=false captureRevalidationRequired=true."),
        bPrepared ? TEXT("HANDOFF_PREPARED") : TEXT("ENVIRONMENT_VALID"),
        Actor->bProviderReady ? TEXT("true") : TEXT("false"),
        bExpectedVisible ? TEXT("true") : TEXT("false"),
        ActiveR28ArchitectureRenderers);
    return true;
}
} // namespace

ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
    ATRIADIstanaExploreV5DR29FacadeEnvironmentActor()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetActorEnableCollision(false);
    ClaimLabel = ExpectedClaimLabel();

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("R29FacadeEnvironmentRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);
    SceneRoot->SetRelativeTransform(FTransform::Identity);

    RetainedR28ConnectivePublicRealmRenderOnly =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("RetainedR28ConnectivePublicRealmRenderOnly"));
    R29ContextFacadeCoverageRenderOnly =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("R29ContextFacadeCoverageRenderOnly"));
    RetainedR28OuterGroundOverlayRenderOnly =
        CreateDefaultSubobject<UStaticMeshComponent>(
            TEXT("RetainedR28OuterGroundOverlayRenderOnly"));
    RetainedR28ConnectivePublicRealmRenderOnly->SetupAttachment(SceneRoot);
    R29ContextFacadeCoverageRenderOnly->SetupAttachment(SceneRoot);
    RetainedR28OuterGroundOverlayRenderOnly->SetupAttachment(SceneRoot);
    ConfigureRenderer(
        RetainedR28ConnectivePublicRealmRenderOnly, PublicRealmTag, false);
    ConfigureRenderer(
        R29ContextFacadeCoverageRenderOnly, FacadeTag, true);
    ConfigureRenderer(
        RetainedR28OuterGroundOverlayRenderOnly,
        OuterGroundTag,
        false,
        FVector(0.0, 0.0, OuterGroundRenderOffsetCentimetres));
    SetOwnedPresentationVisible(this, false);
    Tags.AddUnique(ExpectedActorTag());
}

bool ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::ValidateAssetRoster(
    const FTRIADIstanaExploreV5DR29FacadeEnvironmentAssets& Assets,
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
            Assets.ContextFacadeCoverageMesh,
            R29FacadeMeshObjectPath,
            R29MaterialRoot,
            R29MaterialNames,
            UE_ARRAY_COUNT(R29MaterialNames),
            OutError) ||
        Assets.ContextFacadeCoverageMesh->GetPathName() ==
            R28ArchitectureMeshObjectPath ||
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
            OutError = TEXT("R29 requires its exact facade mesh plus unchanged R28 public-realm and outer-ground presentation assets.");
        }
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
    ConfigurePreparedR29FacadeHandoff(
        const FTRIADIstanaExploreV5DR29FacadeEnvironmentAssets& InAssets,
        bool bInPredecessorProviderReady,
        FString& OutError)
{
    if (bConfigured || !GetWorld() ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !ValidateAssetRoster(InAssets, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("R29 facade preparation requires one fresh identity actor and exact admitted assets.");
        }
        return false;
    }
    int32 R28ClassCount = 0;
    int32 R28TagCount = 0;
    int32 R29ClassCount = 0;
    int32 R29TagCount = 0;
    int32 ActiveR28ArchitectureRenderers = 0;
    if (!ResolveOwnerRosters(
            GetWorld(), this,
            R28ClassCount, R28TagCount,
            R29ClassCount, R29TagCount,
            ActiveR28ArchitectureRenderers, OutError) ||
        R28ClassCount != 1 || R28TagCount != 1 ||
        R29ClassCount != 1 || R29TagCount != 1 ||
        ActiveR28ArchitectureRenderers !=
            (bInPredecessorProviderReady ? 0 : 1))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("R29 facade preparation requires exactly one coherent R28 predecessor and this sole hidden R29 successor.");
        }
        return false;
    }
    for (TActorIterator<ATRIADIstanaExploreV5DR28EnvironmentActor> It(GetWorld()); It; ++It)
    {
        FString R28Report;
        if ((*It)->GetClass() !=
                ATRIADIstanaExploreV5DR28EnvironmentActor::StaticClass() ||
            !(*It)->ValidateR28Environment(R28Report) ||
            (*It)->bProviderReady != bInPredecessorProviderReady)
        {
            OutError = TEXT("R29 facade preparation refused an invalid or provider-incoherent R28 predecessor: ") +
                R28Report;
            return false;
        }
    }

    SavedAssets = InAssets;
    RetainedR28ConnectivePublicRealmRenderOnly->SetStaticMesh(
        InAssets.RetainedR28ConnectivePublicRealmMesh);
    R29ContextFacadeCoverageRenderOnly->SetStaticMesh(
        InAssets.ContextFacadeCoverageMesh);
    RetainedR28OuterGroundOverlayRenderOnly->SetStaticMesh(
        InAssets.RetainedOuterGroundMesh);
    RetainedR28OuterGroundOverlayRenderOnly->SetMaterial(
        0, InAssets.RetainedR28OuterGroundMaterial);
    bConfigured = true;
    bReplacementActivated = false;
    bProviderReady = bInPredecessorProviderReady;
    SetOwnedPresentationVisible(this, false);

    FString Report;
    if (!ValidatePreparedR29FacadeHandoff(Report))
    {
        RetainedR28ConnectivePublicRealmRenderOnly->SetStaticMesh(nullptr);
        R29ContextFacadeCoverageRenderOnly->SetStaticMesh(nullptr);
        RetainedR28OuterGroundOverlayRenderOnly->SetStaticMesh(nullptr);
        RetainedR28OuterGroundOverlayRenderOnly->EmptyOverrideMaterials();
        SavedAssets = FTRIADIstanaExploreV5DR29FacadeEnvironmentAssets{};
        bConfigured = false;
        bProviderReady = false;
        OutError = TEXT("R29 facade handoff preparation failed closed and cleared its hidden successor: ") +
            Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
    ActivateAfterR28EnvironmentRemoval(FString& OutError)
{
    if (!bConfigured || bReplacementActivated)
    {
        OutError = TEXT("R29 facade activation requires one prepared, inactive successor.");
        return false;
    }
    int32 R28ClassCount = 0;
    int32 R28TagCount = 0;
    int32 R29ClassCount = 0;
    int32 R29TagCount = 0;
    int32 ActiveR28ArchitectureRenderers = 0;
    if (!ResolveOwnerRosters(
            GetWorld(), this,
            R28ClassCount, R28TagCount,
            R29ClassCount, R29TagCount,
            ActiveR28ArchitectureRenderers, OutError) ||
        R28ClassCount != 0 || R28TagCount != 0 ||
        R29ClassCount != 1 || R29TagCount != 1 ||
        ActiveR28ArchitectureRenderers != 0)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("R29 facade activation refused because an R28 owner or active R28 architecture renderer remains.");
        }
        return false;
    }
    bReplacementActivated = true;
    SetOwnedPresentationVisible(this, !bProviderReady);
    FString Report;
    if (!ValidateR29FacadeEnvironment(Report))
    {
        SetOwnedPresentationVisible(this, false);
        bReplacementActivated = false;
        OutError = TEXT("R29 facade activation failed closed and returned to an all-hidden state: ") +
            Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::SetProviderReady(
    bool bInProviderReady,
    FString& OutError)
{
    if (!bConfigured || !bReplacementActivated)
    {
        OutError = TEXT("R29 facade provider readiness cannot change before completed predecessor replacement.");
        return false;
    }
    const bool bBefore = bProviderReady;
    bProviderReady = bInProviderReady;
    SetOwnedPresentationVisible(this, !bProviderReady);
    FString Report;
    if (!ValidateR29FacadeEnvironment(Report))
    {
        bProviderReady = bBefore;
        SetOwnedPresentationVisible(this, !bProviderReady);
        OutError = TEXT("R29 facade provider visibility transition failed and was restored: ") +
            Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
    ValidatePreparedR29FacadeHandoff(FString& OutReport) const
{
    return ValidateState(this, true, OutReport);
}

bool ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
    ValidateR29FacadeEnvironment(FString& OutReport) const
{
    return ValidateState(this, false, OutReport);
}

const FString& ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
    ExpectedR29FacadeMeshObjectPath()
{
    return R29FacadeMeshObjectPath;
}

const FString& ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
    ExpectedR28ArchitectureMeshObjectPath()
{
    return R28ArchitectureMeshObjectPath;
}

const FString& ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
    ExpectedR28PublicRealmMeshObjectPath()
{
    return R28PublicRealmMeshObjectPath;
}

const FString& ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
    ExpectedOuterGroundMeshObjectPath()
{
    return OuterGroundMeshObjectPath;
}

const FString& ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
    ExpectedR28OuterGroundMaterialObjectPath()
{
    return R28OuterGroundMaterialObjectPath;
}

const TArray<FName>& ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
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

const FString& ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
    ExpectedClaimLabel()
{
    return R29FacadeClaimLabel;
}

const FName& ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::ExpectedActorTag()
{
    return ActorTag;
}

int32 ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::
    ExpectedOwnedRendererCount()
{
    return 3;
}
