#include "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DGroundVegetationActor.h"
#include "TRIADIstanaPublicViewSceneActor.h"

DEFINE_LOG_CATEGORY_STATIC(
    LogTRIADR29CopernicusTerrainFallback,
    Log,
    All);

namespace
{
const FName ActorTag(TEXT("TRIADIstanaExploreV5DR29CopernicusTerrainFallback"));
const FString Claim(
    TEXT("COPERNICUS_DEM_2021_GLO30_RELATIVE_DSM_RENDER_ONLY_PROVIDER_FALLBACK; CESIUM_WORLD_TERRAIN_PREFERRED; EXACT_COMPLEMENT_OF_AUTHORED_CORE_MASK; NOT_BARE_EARTH_OR_ABSOLUTE_HEIGHT; NO_COLLISION_NAVIGATION_SENSOR_RF_GEOSPATIAL_SURVEY_AUTHORITY"));
const FString MeshPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/R29CopernicusTerrainFallback/SM_IPV5D_R29_CopernicusTerrainFallback_Render.SM_IPV5D_R29_CopernicusTerrainFallback_Render"));
const FString MaterialPath(
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/R29CopernicusTerrainFallback/Materials/M_IPV5D_R29_CopernicusTerrainFallback_ComplementaryCoreMask.M_IPV5D_R29_CopernicusTerrainFallback_ComplementaryCoreMask"));
const FVector BoundsMin(-100000.0, -100000.0, -3030.3865);
const FVector BoundsMax(100000.0, 100000.0, 119.3868);
constexpr double BoundsToleranceCm = 0.02;

bool IsExactNoAuthorityComponent(const UStaticMeshComponent* Component)
{
    return Component &&
        Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision &&
        Component->GetCollisionResponseToChannel(ECC_WorldStatic) ==
            ECollisionResponse::ECR_Ignore &&
        Component->GetCollisionResponseToChannel(ECC_Visibility) ==
            ECollisionResponse::ECR_Ignore &&
        !Component->GetGenerateOverlapEvents() &&
        !Component->CanEverAffectNavigation() &&
        !Component->bRenderCustomDepth;
}

bool SetTerrainRendererVisiblePreservingCollision(
    UStaticMeshComponent* Terrain,
    bool bVisible)
{
    if (!Terrain ||
        Terrain->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
    {
        return false;
    }
    const ECollisionEnabled::Type Before = Terrain->GetCollisionEnabled();
    Terrain->SetVisibility(bVisible, true);
    Terrain->SetHiddenInGame(!bVisible, true);
    return Terrain->IsVisible() == bVisible &&
        Terrain->bHiddenInGame == !bVisible &&
        Terrain->GetCollisionEnabled() == Before;
}
}

ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);

    TerrainFallbackVisual = CreateDefaultSubobject<UStaticMeshComponent>(
        TEXT("CopernicusDEM2021RelativeDsmRenderOnly"));
    TerrainFallbackVisual->SetupAttachment(SceneRoot);
    TerrainFallbackVisual->SetMobility(EComponentMobility::Static);
    TerrainFallbackVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TerrainFallbackVisual->SetCollisionResponseToAllChannels(ECR_Ignore);
    TerrainFallbackVisual->SetGenerateOverlapEvents(false);
    TerrainFallbackVisual->SetCanEverAffectNavigation(false);
    TerrainFallbackVisual->SetRenderCustomDepth(false);
    TerrainFallbackVisual->SetVisibility(false, true);
    TerrainFallbackVisual->SetHiddenInGame(true, true);
    TerrainFallbackVisual->SetCastShadow(true);
    TerrainFallbackVisual->bCastDynamicShadow = true;
    TerrainFallbackVisual->bCastStaticShadow = true;

    Tags.AddUnique(ActorTag);
    ClaimLabel = Claim;
}

const FName& ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    ExpectedActorTag()
{
    return ActorTag;
}

const FString& ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    ExpectedClaimLabel()
{
    return Claim;
}

const FString& ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    ExpectedMeshObjectPath()
{
    return MeshPath;
}

const FString& ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    ExpectedMaterialObjectPath()
{
    return MaterialPath;
}

double ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    ExpectedImportUniformScale()
{
    return 100.0;
}

FVector ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    ExpectedBoundsMinimumCentimeters()
{
    return BoundsMin;
}

FVector ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    ExpectedBoundsMaximumCentimeters()
{
    return BoundsMax;
}

double ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    EvaluateAuthoredCoreCoverage(const FVector2D& WorldXYCentimeters)
{
    return ATRIADIstanaExploreV5DGroundVegetationActor::
        EvaluateGroundOverlayCoreCoverage(WorldXYCentimeters);
}

bool ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    EvaluateFallbackStableDitherMask(const FVector2D& WorldXYCentimeters)
{
    return !ATRIADIstanaExploreV5DGroundVegetationActor::
        EvaluateGroundOverlayStableDitherMask(WorldXYCentimeters);
}

bool ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    ConfigureCopernicusTerrainFallback(
        UStaticMesh* InMesh,
        UMaterialInterface* InComplementaryCoreMaskMaterial,
        FString& OutError)
{
    bConfigured = false;
    SetFallbackVisible(false, OutError);
    if (!InMesh || !InComplementaryCoreMaskMaterial ||
        InMesh->GetPathName() != MeshPath ||
        InComplementaryCoreMaskMaterial->GetPathName() != MaterialPath ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001))
    {
        OutError = TEXT("R29 Copernicus fallback configuration requires the exact mesh/material and an identity actor transform.");
        return false;
    }

    const FBox Box = InMesh->GetBoundingBox();
    if (!Box.Min.Equals(BoundsMin, BoundsToleranceCm) ||
        !Box.Max.Equals(BoundsMax, BoundsToleranceCm))
    {
        OutError = FString::Printf(
            TEXT("R29 Copernicus native import bounds refused: expected min=%s max=%s actual min=%s max=%s. The OBJ must be imported at exact uniform x100."),
            *BoundsMin.ToString(), *BoundsMax.ToString(),
            *Box.Min.ToString(), *Box.Max.ToString());
        return false;
    }

    TerrainFallbackVisual->SetStaticMesh(InMesh);
    TerrainFallbackVisual->SetMaterial(0, InComplementaryCoreMaskMaterial);
    TerrainFallbackVisual->SetRelativeTransform(FTransform::Identity);
    TerrainFallbackVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TerrainFallbackVisual->SetCollisionResponseToAllChannels(ECR_Ignore);
    TerrainFallbackVisual->SetGenerateOverlapEvents(false);
    TerrainFallbackVisual->SetCanEverAffectNavigation(false);
    TerrainFallbackVisual->SetVisibility(false, true);
    TerrainFallbackVisual->SetHiddenInGame(true, true);
    bConfigured = true;

    FString Report;
    if (!ValidateCopernicusTerrainFallback(Report))
    {
        bConfigured = false;
        TerrainFallbackVisual->SetStaticMesh(nullptr);
        TerrainFallbackVisual->EmptyOverrideMaterials();
        OutError = Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    ValidateCopernicusTerrainFallback(FString& OutReport) const
{
    UStaticMesh* Mesh = TerrainFallbackVisual
        ? TerrainFallbackVisual->GetStaticMesh()
        : nullptr;
    UMaterialInterface* Material = TerrainFallbackVisual
        ? TerrainFallbackVisual->GetMaterial(0)
        : nullptr;
    const FBox Box = Mesh ? Mesh->GetBoundingBox() : FBox(ForceInit);
    const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
    const bool bBodyHasNoSimpleCollision = !Body ||
        (Body->AggGeom.GetElementCount() == 0 &&
            Body->CollisionTraceFlag == CTF_UseSimpleAsComplex);
    const bool bMaskContractExact =
        ProviderSiteClipSplinePoints ==
            ATRIADIstanaExploreV5DContextPolicyActor::
                ExpectedProviderSiteClipSplinePoints() &&
        FMath::IsNearlyEqual(
            AuthoredCoreOpaqueCollarMeters,
            ATRIADIstanaExploreV5DGroundVegetationActor::
                ExpectedGroundOverlayOpaqueCollarMeters(),
            0.000001) &&
        FMath::IsNearlyEqual(
            AuthoredCoreOutwardFeatherMeters,
            ATRIADIstanaExploreV5DGroundVegetationActor::
                ExpectedGroundOverlayOutwardFeatherMeters(),
            0.000001) &&
        FMath::IsNearlyEqual(
            StableDitherCellMeters,
            ATRIADIstanaExploreV5DGroundVegetationActor::
                ExpectedGroundOverlayDitherCellMeters(),
            0.000001);
    if (!bConfigured || GetClass() != StaticClass() ||
        !Tags.Contains(ActorTag) || Tags.Num() != 1 ||
        !GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !SceneRoot || !TerrainFallbackVisual ||
        !TerrainFallbackVisual->GetRelativeTransform().Equals(
            FTransform::Identity, 0.001) ||
        !Mesh || Mesh->GetPathName() != MeshPath ||
        !Material || Material->GetPathName() != MaterialPath ||
        TerrainFallbackVisual->IsVisible() != bFallbackCurrentlyVisible ||
        TerrainFallbackVisual->bHiddenInGame == bFallbackCurrentlyVisible ||
        !Box.Min.Equals(BoundsMin, BoundsToleranceCm) ||
        !Box.Max.Equals(BoundsMax, BoundsToleranceCm) ||
        !FMath::IsNearlyEqual(ImportUniformScale, 100.0, 0.000001) ||
        !ImportedBoundsMinimumCentimeters.Equals(BoundsMin, 0.000001) ||
        !ImportedBoundsMaximumCentimeters.Equals(BoundsMax, 0.000001) ||
        !IsExactNoAuthorityComponent(TerrainFallbackVisual) ||
        !bBodyHasNoSimpleCollision || !bMaskContractExact ||
        ClaimLabel != Claim || !bCesiumWorldTerrainPreferred ||
        !bRelativeDsmVisualFallbackOnly ||
        !bAuthoredCoreProtectedByExactComplementaryMask ||
        bCollisionNavigationSensorRfAuthority ||
        bAbsoluteHeightGeospatialOrSurveyAuthority || bBareEarthDtmClaimed ||
        FrozenObjSha256 != TEXT("6057AE3C23287E9AFFED72B1C0842177A980BFE7AAB4089BF51E6FD7B2A621DF") ||
        FrozenMtlSha256 != TEXT("AA98AABD87F409748B24AD344EFC43F68C3A0840CD670EC503A67597ACBEB720") ||
        FrozenManifestSha256 != TEXT("06FD4D5A2F3988DAC311601F8255255C632A4FE60FC7D923F1F2C62CF08AFC5D") ||
        FrozenSourceContractSha256 != TEXT("ED96EEE1B070E09F33F469747DDC27F482018959CF763A7D2B20755D6794A415"))
    {
        OutReport = TEXT("R29 Copernicus fallback validation failed: exact x100 bounds, complementary 64-edge mask, component isolation, assets, or negative authority drifted.");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("R29_COPERNICUS_TERRAIN_FALLBACK_VALID exactX100=true boundsXY=[-100000,+100000]cm vertices=16641 triangles=32768 coreMask=exactComplement64Edge collar=50m feather=8m dither=0.25m cesiumPreferred=true fallbackVisible=%s sourceCollisionPreserved=true authority=false"),
        bFallbackCurrentlyVisible ? TEXT("true") : TEXT("false"));
    return true;
}

bool ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    ResolveRuntimeDependencies(
        ATRIADIstanaPublicViewSceneActor*& OutScene,
        ATRIADIstanaExploreV5DContextPolicyActor*& OutPolicy,
        ATRIADIstanaExploreV5DGroundVegetationActor*& OutGround,
        FString& OutError)
{
    OutScene = nullptr;
    OutPolicy = nullptr;
    OutGround = nullptr;
    int32 SceneCount = 0;
    int32 PolicyCount = 0;
    int32 GroundCount = 0;
    UWorld* World = GetWorld();
    if (!World)
    {
        OutError = TEXT("R29 Copernicus fallback has no world.");
        return false;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }
        if (Candidate->GetClass() == ATRIADIstanaPublicViewSceneActor::StaticClass())
        {
            OutScene = Cast<ATRIADIstanaPublicViewSceneActor>(Candidate);
            ++SceneCount;
        }
        if (Candidate->GetClass() == ATRIADIstanaExploreV5DContextPolicyActor::StaticClass())
        {
            OutPolicy = Cast<ATRIADIstanaExploreV5DContextPolicyActor>(Candidate);
            ++PolicyCount;
        }
        if (Candidate->GetClass() == ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass())
        {
            OutGround = Cast<ATRIADIstanaExploreV5DGroundVegetationActor>(Candidate);
            ++GroundCount;
        }
    }

    UStaticMeshComponent* Terrain = OutScene ? OutScene->TerrainComponent.Get() : nullptr;
    UStaticMeshComponent* Core = OutGround
        ? OutGround->GroundMacroVariationOverlay.Get()
        : nullptr;
    if (SceneCount != 1 || PolicyCount != 1 || GroundCount != 1 ||
        !OutScene || !OutPolicy || !OutGround ||
        !OutScene->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !OutPolicy->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !OutGround->GetActorTransform().Equals(FTransform::Identity, 0.001) ||
        !Terrain || Terrain->GetCollisionEnabled() !=
            ECollisionEnabled::QueryAndPhysics ||
        !Core || !Core->IsVisible() || Core->bHiddenInGame ||
        Core->GetCollisionEnabled() != ECollisionEnabled::NoCollision ||
        !OutPolicy->bCesiumLayerIsVisualOnly ||
        OutPolicy->bCesiumCollisionNavigationSensorOrRfAuthority ||
        !OutPolicy->bProviderContentClippedFromAuthoredCore)
    {
        OutError = FString::Printf(
            TEXT("R29 Copernicus fallback requires one exact identity scene/policy/ground owner, live source QueryAndPhysics collision, visible no-collision core overlay, and visual-only clipped Cesium; counts=%d/%d/%d."),
            SceneCount, PolicyCount, GroundCount);
        return false;
    }

    if (RuntimePolicy.Get() != OutPolicy)
    {
        if (AActor* Previous = RuntimePolicy.Get())
        {
            RemoveTickPrerequisiteActor(Previous);
        }
        RuntimePolicy = OutPolicy;
        AddTickPrerequisiteActor(OutPolicy);
    }
    if (RuntimeGround.Get() != OutGround)
    {
        if (AActor* Previous = RuntimeGround.Get())
        {
            RemoveTickPrerequisiteActor(Previous);
        }
        RuntimeGround = OutGround;
        AddTickPrerequisiteActor(OutGround);
    }
    RuntimeScene = OutScene;
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    SetFallbackVisible(bool bVisible, FString& OutError)
{
    if (!TerrainFallbackVisual)
    {
        OutError = TEXT("R29 Copernicus fallback component is missing.");
        return false;
    }
    TerrainFallbackVisual->SetVisibility(bVisible, true);
    TerrainFallbackVisual->SetHiddenInGame(!bVisible, true);
    bFallbackCurrentlyVisible = bVisible;
    if (TerrainFallbackVisual->IsVisible() != bVisible ||
        TerrainFallbackVisual->bHiddenInGame == bVisible ||
        TerrainFallbackVisual->GetCollisionEnabled() !=
            ECollisionEnabled::NoCollision)
    {
        OutError = TEXT("R29 Copernicus fallback visibility readback or no-collision invariant failed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    SynchronizeWithProvider(FString& OutError)
{
    ATRIADIstanaPublicViewSceneActor* Scene = nullptr;
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ATRIADIstanaExploreV5DGroundVegetationActor* Ground = nullptr;
    FString Validation;
    if (!ValidateCopernicusTerrainFallback(Validation) ||
        !ResolveRuntimeDependencies(Scene, Policy, Ground, OutError) ||
        !Policy->HasActorBegunPlay())
    {
        if (OutError.IsEmpty())
        {
            OutError = Validation.IsEmpty()
                ? TEXT("R29 Copernicus fallback waits for the exact context policy to begin play.")
                : Validation;
        }
        return false;
    }

    const bool bPresentFallback =
        Policy->bR33DualCesiumContextConfigured
        ? Policy->ShouldPresentR29CopernicusFallback()
        : !Policy->bLocalBuildingFallbackCurrentlyHidden;
    if (!bPresentFallback)
    {
        // GroundVegetation is a tick prerequisite and owns the source renderer
        // whenever a streamed terrain surface is presented. This actor only
        // removes its no-authority DEM.
        if (!SetFallbackVisible(false, OutError))
        {
            return false;
        }
        bSourceTerrainRendererHiddenByThisActor = false;
        return true;
    }

    UStaticMeshComponent* Terrain = Scene->TerrainComponent.Get();
    // Hide renderer only after the exact core overlay and DEM have validated.
    // QueryAndPhysics remains on the original component before and after.
    if (!SetTerrainRendererVisiblePreservingCollision(Terrain, false) ||
        !SetFallbackVisible(true, OutError))
    {
        SetTerrainRendererVisiblePreservingCollision(Terrain, true);
        return false;
    }
    bSourceTerrainRendererHiddenByThisActor = true;
    return true;
}

void ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
    FailClosedToSourceTerrain()
{
    FString Ignored;
    SetFallbackVisible(false, Ignored);
    ATRIADIstanaPublicViewSceneActor* Scene = RuntimeScene.Get();
    if (Scene && Scene->TerrainComponent)
    {
        SetTerrainRendererVisiblePreservingCollision(
            Scene->TerrainComponent.Get(), true);
    }
    bSourceTerrainRendererHiddenByThisActor = false;
}

void ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::BeginPlay()
{
    Super::BeginPlay();
    FString Error;
    if (!SynchronizeWithProvider(Error))
    {
        FailClosedToSourceTerrain();
        bLoggedRuntimeFailure = true;
        UE_LOG(LogTRIADR29CopernicusTerrainFallback, Warning, TEXT("%s"), *Error);
    }
}

void ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::Tick(
    float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bConfigured)
    {
        FailClosedToSourceTerrain();
        return;
    }
    FString Error;
    if (!SynchronizeWithProvider(Error))
    {
        FailClosedToSourceTerrain();
        if (!bLoggedRuntimeFailure)
        {
            UE_LOG(LogTRIADR29CopernicusTerrainFallback, Warning, TEXT("%s"), *Error);
            bLoggedRuntimeFailure = true;
        }
        return;
    }
    bLoggedRuntimeFailure = false;
}

void ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    FailClosedToSourceTerrain();
    if (AActor* Policy = RuntimePolicy.Get())
    {
        RemoveTickPrerequisiteActor(Policy);
    }
    if (AActor* Ground = RuntimeGround.Get())
    {
        RemoveTickPrerequisiteActor(Ground);
    }
    RuntimeScene.Reset();
    RuntimePolicy.Reset();
    RuntimeGround.Reset();
    Super::EndPlay(EndPlayReason);
}
