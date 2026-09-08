#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"

namespace
{
const FString AssetRoot(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsRealismR28"));
const FString MeshRoot(AssetRoot + TEXT("/Meshes"));
const FString MaterialRoot(AssetRoot + TEXT("/Materials"));

FString ObjectPath(const FString& Root, const TCHAR* Name)
{
    return Root + TEXT("/") + Name + TEXT(".") + Name;
}

const FString ConnectivePublicRealmMeshObjectPath(ObjectPath(
    MeshRoot, TEXT("SM_IPV5D_R28_ConnectivePublicRealm_Render")));
const FString ContextArchitecturalDressingMeshObjectPath(ObjectPath(
    MeshRoot, TEXT("SM_IPV5D_R28_ContextArchitecturalDressing_Render")));
const FString OuterGroundMeshObjectPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/OuterGroundLoadingFallback/"
         "SM_IPV5D_OuterGroundLoadingFallback_Render."
         "SM_IPV5D_OuterGroundLoadingFallback_Render"));
const FString OuterGroundMaterialObjectPath(ObjectPath(
    MaterialRoot, TEXT("MI_IPV5D_R28_OuterGround")));
const FString ExpectedEnvironmentClaimLabel(
    TEXT("PUBLIC_SOURCE_ALIGNED_RENDER_ONLY_VISUAL_DRESSING_NOT_SURVEY_AS_BUILT_CURRENT_COMPLETE_SENSOR_OR_RF_AUTHORITY"));

const FName ActorTag(TEXT("TRIADIstanaExploreV5DR28Environment"));
const FName PublicRealmTag(TEXT("TRIADV5DR28ConnectivePublicRealmRenderOnly"));
const FName ArchitectureTag(TEXT("TRIADV5DR28ContextArchitectureRenderOnly"));
const FName OuterGroundTag(TEXT("TRIADV5DR28OuterGroundOverlayRenderOnly"));
const FName HumanOnlyOverlayTag(TEXT("TRIADHumanOnlyOverlay"));
constexpr float OuterGroundRenderOffsetCentimetres = 0.5f;

const TCHAR* const ArchitectureMaterialNames[] = {
    TEXT("MI_IPV5D_R28_GlassCool"),
    TEXT("MI_IPV5D_R28_GlassWarm"),
    TEXT("MI_IPV5D_R28_FrameLight"),
    TEXT("MI_IPV5D_R28_FrameDark"),
    TEXT("MI_IPV5D_R28_RoofTrim")};
const TCHAR* const PublicRealmMaterialNames[] = {
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
    Component->SetRelativeTransform(FTransform(FQuat::Identity, RelativeLocation));
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
    // Manual/Player0 proof capture must retain this presentation.  EO/event
    // sensor SceneCapture feeds are excluded by bHiddenInSceneCapture above.
    Component->ComponentTags.Remove(HumanOnlyOverlayTag);
}

bool ValidateMeshMaterials(
    const UStaticMesh* Mesh,
    const FString& ExpectedPath,
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
        OutError = TEXT("An R28 mesh lost its exact object-path, material-count, or no-collision source contract.");
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
        OutError = TEXT("An R28 mesh lost its exact isolated semantic material bindings.");
        return false;
    }
    return true;
}

bool ValidateAssets(
    const FTRIADIstanaExploreV5DR28EnvironmentAssets& Assets,
    FString& OutError)
{
    const UBodySetup* OuterBody = Assets.OuterGroundMesh
        ? Assets.OuterGroundMesh->GetBodySetup()
        : nullptr;
    if (!ValidateMeshMaterials(
            Assets.ConnectivePublicRealmMesh,
            ConnectivePublicRealmMeshObjectPath,
            PublicRealmMaterialNames,
            UE_ARRAY_COUNT(PublicRealmMaterialNames),
            OutError) ||
        !ValidateMeshMaterials(
            Assets.ContextArchitecturalDressingMesh,
            ContextArchitecturalDressingMeshObjectPath,
            ArchitectureMaterialNames,
            UE_ARRAY_COUNT(ArchitectureMaterialNames),
            OutError) ||
        !Assets.OuterGroundMesh ||
        Assets.OuterGroundMesh->GetPathName() != OuterGroundMeshObjectPath ||
        (OuterBody &&
         (OuterBody->AggGeom.GetElementCount() != 0 ||
          OuterBody->CollisionTraceFlag == CTF_UseComplexAsSimple)) ||
        !Assets.OuterGroundMaterial ||
        Assets.OuterGroundMaterial->GetPathName() != OuterGroundMaterialObjectPath)
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("R28 requires its two isolated meshes/materials and the exact immutable outer-ground annulus.");
        }
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
        Component->ComponentTags.Contains(HumanOnlyOverlayTag) ||
        Component->IsVisible() != bExpectedVisible ||
        Component->bHiddenInGame == bExpectedVisible ||
        (ExpectedOverride && Component->GetMaterial(0) != ExpectedOverride))
    {
        OutError = TEXT("An R28 renderer lost its exact static, visual-only, scene-capture-excluded contract.");
        return false;
    }
    return true;
}
} // namespace

ATRIADIstanaExploreV5DR28EnvironmentActor::
    ATRIADIstanaExploreV5DR28EnvironmentActor()
{
    PrimaryActorTick.bCanEverTick = false;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetActorEnableCollision(false);
    ClaimLabel = ExpectedClaimLabel();

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("R28EnvironmentRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);
    SceneRoot->SetRelativeTransform(FTransform::Identity);

    ConnectivePublicRealmRenderOnly =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConnectivePublicRealmRenderOnly"));
    ContextArchitecturalDressingRenderOnly =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ContextArchitecturalDressingRenderOnly"));
    OuterGroundColourReliefOverlayRenderOnly =
        CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OuterGroundColourReliefOverlayRenderOnly"));
    ConnectivePublicRealmRenderOnly->SetupAttachment(SceneRoot);
    ContextArchitecturalDressingRenderOnly->SetupAttachment(SceneRoot);
    OuterGroundColourReliefOverlayRenderOnly->SetupAttachment(SceneRoot);

    ConfigureRenderer(ConnectivePublicRealmRenderOnly, PublicRealmTag, false);
    ConfigureRenderer(ContextArchitecturalDressingRenderOnly, ArchitectureTag, true);
    ConfigureRenderer(
        OuterGroundColourReliefOverlayRenderOnly,
        OuterGroundTag,
        false,
        FVector(0.0, 0.0, OuterGroundRenderOffsetCentimetres));
    for (UStaticMeshComponent* Component : {
             ConnectivePublicRealmRenderOnly.Get(),
             ContextArchitecturalDressingRenderOnly.Get(),
             OuterGroundColourReliefOverlayRenderOnly.Get()})
    {
        Component->SetVisibility(false, true);
        Component->SetHiddenInGame(true, true);
    }
    Tags.AddUnique(ExpectedActorTag());
}

bool ATRIADIstanaExploreV5DR28EnvironmentActor::ConfigureR28Environment(
    const FTRIADIstanaExploreV5DR28EnvironmentAssets& InAssets,
    bool bInitialProviderReady,
    FString& OutError)
{
    if (bConfigured || !GetWorld() ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !ValidateAssets(InAssets, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("R28 configuration requires one fresh identity actor and exact admitted assets.");
        }
        return false;
    }

    SavedAssets = InAssets;
    ConnectivePublicRealmRenderOnly->SetStaticMesh(InAssets.ConnectivePublicRealmMesh);
    ContextArchitecturalDressingRenderOnly->SetStaticMesh(
        InAssets.ContextArchitecturalDressingMesh);
    OuterGroundColourReliefOverlayRenderOnly->SetStaticMesh(InAssets.OuterGroundMesh);
    OuterGroundColourReliefOverlayRenderOnly->SetMaterial(0, InAssets.OuterGroundMaterial);
    bConfigured = true;
    bProviderReady = bInitialProviderReady;
    const bool bVisible = !bProviderReady;
    for (UStaticMeshComponent* Component : {
             ConnectivePublicRealmRenderOnly.Get(),
             ContextArchitecturalDressingRenderOnly.Get(),
             OuterGroundColourReliefOverlayRenderOnly.Get()})
    {
        Component->SetVisibility(bVisible, true);
        Component->SetHiddenInGame(!bVisible, true);
    }

    FString Report;
    if (!ValidateR28Environment(Report))
    {
        ConnectivePublicRealmRenderOnly->SetStaticMesh(nullptr);
        ContextArchitecturalDressingRenderOnly->SetStaticMesh(nullptr);
        OuterGroundColourReliefOverlayRenderOnly->SetStaticMesh(nullptr);
        OuterGroundColourReliefOverlayRenderOnly->EmptyOverrideMaterials();
        SavedAssets = FTRIADIstanaExploreV5DR28EnvironmentAssets{};
        bConfigured = false;
        bProviderReady = false;
        OutError = TEXT("R28 configuration failed and was restored: ") + Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR28EnvironmentActor::SetProviderReady(
    bool bInProviderReady,
    FString& OutError)
{
    if (!bConfigured)
    {
        OutError = TEXT("R28 provider readiness cannot change before configuration.");
        return false;
    }
    const bool bBefore = bProviderReady;
    bProviderReady = bInProviderReady;
    const bool bVisible = !bProviderReady;
    for (UStaticMeshComponent* Component : {
             ConnectivePublicRealmRenderOnly.Get(),
             ContextArchitecturalDressingRenderOnly.Get(),
             OuterGroundColourReliefOverlayRenderOnly.Get()})
    {
        Component->SetVisibility(bVisible, true);
        Component->SetHiddenInGame(!bVisible, true);
    }
    FString Report;
    if (!ValidateR28Environment(Report))
    {
        bProviderReady = bBefore;
        const bool bRestoreVisible = !bProviderReady;
        for (UStaticMeshComponent* Component : {
                 ConnectivePublicRealmRenderOnly.Get(),
                 ContextArchitecturalDressingRenderOnly.Get(),
                 OuterGroundColourReliefOverlayRenderOnly.Get()})
        {
            Component->SetVisibility(bRestoreVisible, true);
            Component->SetHiddenInGame(!bRestoreVisible, true);
        }
        OutError = TEXT("R28 provider visibility transition failed and was restored: ") + Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR28EnvironmentActor::ValidateR28Environment(
    FString& OutReport) const
{
    FString Error;
    const bool bVisible = !bProviderReady;
    if (!bConfigured || !GetWorld() || !Tags.Contains(ExpectedActorTag()) ||
        ClaimLabel != ExpectedClaimLabel() || !bRenderOnly ||
        bCollisionNavigationSensorOrRfAuthority ||
        bMeasuredSurveyAsBuiltOrCurrentCompleteClaimed ||
        bExistingSimulationOrRfInputsModified || bRuntimeGeometryGenerated ||
        PrimaryActorTick.bCanEverTick || GetActorEnableCollision() ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !SceneRoot || SceneRoot != RootComponent ||
        SceneRoot->Mobility != EComponentMobility::Static ||
        !SceneRoot->GetRelativeTransform().Equals(FTransform::Identity, 0.001) ||
        !ValidateAssets(SavedAssets, Error) ||
        !ValidateRenderer(
            ConnectivePublicRealmRenderOnly,
            SceneRoot,
            SavedAssets.ConnectivePublicRealmMesh,
            PublicRealmTag,
            FVector::ZeroVector,
            false,
            bVisible,
            nullptr,
            Error) ||
        !ValidateRenderer(
            ContextArchitecturalDressingRenderOnly,
            SceneRoot,
            SavedAssets.ContextArchitecturalDressingMesh,
            ArchitectureTag,
            FVector::ZeroVector,
            true,
            bVisible,
            nullptr,
            Error) ||
        !ValidateRenderer(
            OuterGroundColourReliefOverlayRenderOnly,
            SceneRoot,
            SavedAssets.OuterGroundMesh,
            OuterGroundTag,
            FVector(0.0, 0.0, OuterGroundRenderOffsetCentimetres),
            false,
            bVisible,
            SavedAssets.OuterGroundMaterial,
            Error))
    {
        OutReport = Error.IsEmpty()
            ? TEXT("R28 environment lost its identity, renderer, asset, visibility, or negative-authority contract.")
            : Error;
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_VALID providerReady=%s visible=%s components=3 identity=true static=true renderOnly=true sceneCapture=false collision=false navigation=false sensorRfAuthority=false runtimeGeometry=false existingSimulationRfInputsModified=false outerGroundRenderOffsetCm=0.5."),
        bProviderReady ? TEXT("true") : TEXT("false"),
        bVisible ? TEXT("true") : TEXT("false"));
    return true;
}

const FString& ATRIADIstanaExploreV5DR28EnvironmentActor::
    ExpectedConnectivePublicRealmMeshObjectPath()
{
    return ConnectivePublicRealmMeshObjectPath;
}

const FString& ATRIADIstanaExploreV5DR28EnvironmentActor::
    ExpectedContextArchitecturalDressingMeshObjectPath()
{
    return ContextArchitecturalDressingMeshObjectPath;
}

const FString& ATRIADIstanaExploreV5DR28EnvironmentActor::
    ExpectedOuterGroundMeshObjectPath()
{
    return OuterGroundMeshObjectPath;
}

const FString& ATRIADIstanaExploreV5DR28EnvironmentActor::
    ExpectedOuterGroundMaterialObjectPath()
{
    return OuterGroundMaterialObjectPath;
}

const FString& ATRIADIstanaExploreV5DR28EnvironmentActor::ExpectedClaimLabel()
{
    return ExpectedEnvironmentClaimLabel;
}

const FName& ATRIADIstanaExploreV5DR28EnvironmentActor::ExpectedActorTag()
{
    return ActorTag;
}
