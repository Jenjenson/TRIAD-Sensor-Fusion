#include "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.h"

// Some Windows SDK configurations leave a function-like max macro active.
// Cesium's public headers correctly use numeric_limits::max.
#ifdef max
#undef max
#endif

#include "Cesium3DTileset.h"
#include "Cesium3DTilesetLoadFailureDetails.h"
#include "CesiumCartographicPolygon.h"
#include "CesiumGeoreference.h"
#include "CesiumIonServer.h"
#include "CesiumPolygonRasterOverlay.h"
#include "CesiumRasterOverlay.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"

DEFINE_LOG_CATEGORY_STATIC(
    LogTRIADR33CesiumWorldTerrainReference,
    Log,
    All);

namespace
{
constexpr int64 GooglePhotorealisticIonAssetId = 2275207;
constexpr int64 CesiumWorldTerrainIonAssetId = 1;
constexpr double GoogleMaximumScreenSpaceError = 1.0;
constexpr double CesiumWorldTerrainMaximumScreenSpaceError = 8.0;
constexpr float ReadinessSampleIntervalSeconds = 0.5f;
constexpr float PresentLoadProgressPercent = 98.0f;
constexpr float RestoreLoadProgressPercent = 90.0f;
constexpr int32 PresentRequiredConsecutiveSamples = 3;
constexpr int32 RestoreRequiredConsecutiveSamples = 2;
constexpr float R33CwtLoadFailureRetryBackoffSeconds = 10.0f;
constexpr float R33CwtWarmingTimeoutSeconds = 30.0f;
constexpr int64 CwtMaximumCachedBytes = 512LL * 1024LL * 1024LL;
constexpr int32 CwtMaximumSimultaneousTileLoads = 6;
constexpr int32 CwtLoadingDescendantLimit = 20;
constexpr double R33TransformTolerance = 0.001;
constexpr double LongitudeToleranceDegrees = 0.00000001;
constexpr double LatitudeToleranceDegrees = 0.00000001;
constexpr double HeightToleranceMeters = 0.000001;

const FName ControllerTag(
    TEXT("TRIADIstanaExploreV5DR33CesiumWorldTerrainReference"));
const FName GoogleRoleTag(TEXT("TRIADIstanaExploreV5DVisualTileset"));
const FName CwtRoleTag(
    TEXT("TRIADIstanaExploreV5DR33CesiumWorldTerrainTileset"));
const FName R33TerrainContextMemberTag(
    TEXT("TRIADIstanaExploreV5DR33CesiumContextMember"));
const FName R33GeoreferenceTag(TEXT("TRIADIstanaExploreV5DGeoreference"));
const FName R33DefaultGeoreferenceTag(TEXT("DEFAULT_GEOREFERENCE"));
const FName ProviderSiteClipTag(
    TEXT("TRIADIstanaExploreV5DProviderSiteClip"));
const FString R33TerrainClaim(
    TEXT("VISUAL_REFERENCE_ONLY_R33_CESIUM_WORLD_TERRAIN"));

bool IsFiniteLoadProgress(float Value)
{
    return FMath::IsFinite(Value) && Value >= 0.0f && Value <= 100.0f;
}

bool HasExpectedGeoreferenceValues(const ACesiumGeoreference* Georeference)
{
    if (!Georeference)
    {
        return false;
    }
    const FVector Origin =
        Georeference->GetOriginLongitudeLatitudeHeight();
    return Georeference->GetOriginPlacement() ==
            EOriginPlacement::CartographicOrigin &&
        FMath::IsNearlyEqual(
            Origin.X,
            103.84288055,
            LongitudeToleranceDegrees) &&
        FMath::IsNearlyEqual(
            Origin.Y,
            1.30709615,
            LatitudeToleranceDegrees) &&
        FMath::IsNearlyEqual(
            Origin.Z,
            47.0,
            HeightToleranceMeters) &&
        FMath::IsNearlyEqual(
            Georeference->GetScale(),
            100.0,
            R33TransformTolerance);
}

bool HasExpectedGoogleTuple(ACesium3DTileset* Tileset)
{
    if (!Tileset ||
        !IsValid(Tileset->GetCesiumIonServer()) ||
        Tileset->GetTilesetSource() != ETilesetSource::FromCesiumIon ||
        Tileset->GetIonAssetID() != GooglePhotorealisticIonAssetId)
    {
        return false;
    }
    return
        FMath::IsNearlyEqual(
            Tileset->GetMaximumScreenSpaceError(),
            GoogleMaximumScreenSpaceError,
            0.000001) &&
        Tileset->ApplyDpiScaling == EApplyDpiScaling::No &&
        !Tileset->GetCreatePhysicsMeshes() &&
        !Tileset->GetCreateNavCollision() &&
        !Tileset->GetActorEnableCollision() &&
        Tileset->ShowCreditsOnScreen;
}

bool HasExpectedCwtTuple(ACesium3DTileset* Tileset)
{
    if (!Tileset ||
        !IsValid(Tileset->GetCesiumIonServer()) ||
        Tileset->GetTilesetSource() != ETilesetSource::FromCesiumIon ||
        Tileset->GetIonAssetID() != CesiumWorldTerrainIonAssetId)
    {
        return false;
    }
    return
        FMath::IsNearlyEqual(
            Tileset->GetMaximumScreenSpaceError(),
            CesiumWorldTerrainMaximumScreenSpaceError,
            0.000001) &&
        Tileset->ApplyDpiScaling == EApplyDpiScaling::No &&
        !Tileset->GetCreatePhysicsMeshes() &&
        !Tileset->GetCreateNavCollision() &&
        !Tileset->GetActorEnableCollision() &&
        Tileset->ShowCreditsOnScreen && Tileset->PreloadAncestors &&
        !Tileset->PreloadSiblings && Tileset->ForbidHoles &&
        Tileset->MaximumCachedBytes == CwtMaximumCachedBytes &&
        Tileset->MaximumSimultaneousTileLoads ==
            CwtMaximumSimultaneousTileLoads &&
        Tileset->LoadingDescendantLimit == CwtLoadingDescendantLimit;
}

void ApplyR33SurvivingRolesSafeLocal(
    ACesium3DTileset* GoogleTileset,
    ACesium3DTileset* CwtTileset)
{
    if (IsValid(GoogleTileset))
    {
        GoogleTileset->SetActorHiddenInGame(true);
        GoogleTileset->SuspendUpdate = true;
    }
    if (IsValid(CwtTileset))
    {
        CwtTileset->SetActorHiddenInGame(true);
        CwtTileset->SuspendUpdate = true;
    }
}

bool ApplyExclusivePresentationState(
    ACesium3DTileset* GoogleTileset,
    ACesium3DTileset* CwtTileset,
    bool bShowGoogle,
    bool bSuspendGoogle,
    bool bShowCwt,
    bool bSuspendCwt,
    FString& OutError)
{
    if (!IsValid(GoogleTileset) || !IsValid(CwtTileset) ||
        GoogleTileset == CwtTileset ||
        (bShowGoogle && bShowCwt))
    {
        ApplyR33SurvivingRolesSafeLocal(GoogleTileset, CwtTileset);
        OutError = TEXT("R33 exclusive presentation refused missing, aliased, or double-visible streamed roles.");
        return false;
    }

    // Hide both roles first. Only after the non-overlap state is observable may
    // the admitted role for the destination state become visible.
    GoogleTileset->SetActorHiddenInGame(true);
    CwtTileset->SetActorHiddenInGame(true);
    GoogleTileset->SuspendUpdate = true;
    CwtTileset->SuspendUpdate = true;

    GoogleTileset->SuspendUpdate = bSuspendGoogle;
    CwtTileset->SuspendUpdate = bSuspendCwt;
    if (bShowGoogle)
    {
        GoogleTileset->SetActorHiddenInGame(false);
    }
    if (bShowCwt)
    {
        CwtTileset->SetActorHiddenInGame(false);
    }

    const bool bGoogleVisible = !GoogleTileset->IsHidden();
    const bool bCwtVisible = !CwtTileset->IsHidden();
    if ((bGoogleVisible && bCwtVisible) ||
        bGoogleVisible != bShowGoogle || bCwtVisible != bShowCwt ||
        GoogleTileset->SuspendUpdate != bSuspendGoogle ||
        CwtTileset->SuspendUpdate != bSuspendCwt)
    {
        GoogleTileset->SetActorHiddenInGame(true);
        CwtTileset->SetActorHiddenInGame(true);
        GoogleTileset->SuspendUpdate = true;
        CwtTileset->SuspendUpdate = true;
        OutError = TEXT("R33 streamed-role visibility/suspension readback failed; SafeLocal was applied.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool OverlayMatches(
    const UCesiumPolygonRasterOverlay* Overlay,
    const ACesium3DTileset* Owner,
    const ACesiumCartographicPolygon* Polygon)
{
    return Overlay && Overlay->GetOwner() == Owner && Overlay->IsActive() &&
        Overlay->MaterialLayerKey == TEXT("Clipping") &&
        !Overlay->InvertSelection && Overlay->ExcludeSelectedTiles &&
        Overlay->Polygons.Num() == 1 &&
        Overlay->Polygons[0].Get() == Polygon;
}
} // namespace

ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.TickInterval = ReadinessSampleIntervalSeconds;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);
    SceneRoot->SetMobility(EComponentMobility::Static);
    Tags.AddUnique(ControllerTag);
    ClaimLabel = R33TerrainClaim;
}

const FName&
ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    ExpectedControllerTag()
{
    return ControllerTag;
}

const FName&
ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    ExpectedGoogleRoleTag()
{
    return GoogleRoleTag;
}

const FName&
ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    ExpectedCwtRoleTag()
{
    return CwtRoleTag;
}

const FName&
ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    ExpectedContextMemberTag()
{
    return R33TerrainContextMemberTag;
}

int64 ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    ExpectedGoogleIonAssetId()
{
    return GooglePhotorealisticIonAssetId;
}

int64 ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    ExpectedCwtIonAssetId()
{
    return CesiumWorldTerrainIonAssetId;
}

float ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    ExpectedReadinessSampleIntervalSeconds()
{
    return ReadinessSampleIntervalSeconds;
}

bool ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    ResolveExactWorldRoster(
        ACesium3DTileset*& OutGoogleTileset,
        ACesium3DTileset*& OutCwtTileset,
        ACesiumGeoreference*& OutGeoreference,
        FString& OutError) const
{
    OutGoogleTileset = nullptr;
    OutCwtTileset = nullptr;
    OutGeoreference = nullptr;
    UWorld* World = GetWorld();
    if (!World)
    {
        OutError = TEXT("R33 terrain reference has no world.");
        return false;
    }

    int32 TotalTilesetCount = 0;
    int32 GoogleRoleCount = 0;
    int32 CwtRoleCount = 0;
    int32 ContextMemberCount = 0;
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        ACesium3DTileset* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }
        ++TotalTilesetCount;
        if (Candidate->Tags.Contains(R33TerrainContextMemberTag))
        {
            ++ContextMemberCount;
        }
        if (Candidate->Tags.Contains(GoogleRoleTag) &&
            Candidate->GetIonAssetID() == GooglePhotorealisticIonAssetId)
        {
            OutGoogleTileset = Candidate;
            ++GoogleRoleCount;
        }
        if (Candidate->Tags.Contains(CwtRoleTag) &&
            Candidate->GetIonAssetID() == CesiumWorldTerrainIonAssetId)
        {
            OutCwtTileset = Candidate;
            ++CwtRoleCount;
        }
    }

    int32 GeoreferenceCount = 0;
    for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            OutGeoreference = *It;
            ++GeoreferenceCount;
        }
    }

    if (TotalTilesetCount != 2 || ContextMemberCount != 2 ||
        GoogleRoleCount != 1 || CwtRoleCount != 1 ||
        GeoreferenceCount != 1 || !OutGoogleTileset || !OutCwtTileset ||
        !OutGeoreference || OutGoogleTileset == OutCwtTileset ||
        OutGoogleTileset->Tags.Contains(CwtRoleTag) ||
        OutCwtTileset->Tags.Contains(GoogleRoleTag) ||
        !IsValid(OutGoogleTileset->GetCesiumIonServer()) ||
        !IsValid(OutCwtTileset->GetCesiumIonServer()) ||
        OutGoogleTileset->GetCesiumIonServer() !=
            OutCwtTileset->GetCesiumIonServer())
    {
        OutError = FString::Printf(
            TEXT("R33 requires exactly two member tilesets with one exact Google role, one exact CWT role, and one georeference; total=%d members=%d google=%d cwt=%d georeference=%d."),
            TotalTilesetCount,
            ContextMemberCount,
            GoogleRoleCount,
            CwtRoleCount,
            GeoreferenceCount);
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    ValidateSharedProviderSiteClip(FString& OutError) const
{
    UWorld* World = GetWorld();
    if (!World || !GoogleTileset || !CwtTileset)
    {
        OutError = TEXT("R33 shared provider-site clipping has no exact world or roles.");
        return false;
    }

    ACesiumCartographicPolygon* Polygon = nullptr;
    int32 PolygonCount = 0;
    for (TActorIterator<ACesiumCartographicPolygon> It(World); It; ++It)
    {
        if (IsValid(*It) && It->Tags.Contains(ProviderSiteClipTag))
        {
            Polygon = *It;
            ++PolygonCount;
        }
    }
    TArray<UCesiumPolygonRasterOverlay*> GoogleOverlays;
    TArray<UCesiumPolygonRasterOverlay*> CwtOverlays;
    TArray<UCesiumRasterOverlay*> GoogleRasterOverlays;
    TArray<UCesiumRasterOverlay*> CwtRasterOverlays;
    GoogleTileset->GetComponents(GoogleOverlays);
    CwtTileset->GetComponents(CwtOverlays);
    GoogleTileset->GetComponents(GoogleRasterOverlays);
    CwtTileset->GetComponents(CwtRasterOverlays);
    if (PolygonCount != 1 || !Polygon || !Polygon->Polygon ||
        !Polygon->Polygon->IsClosedLoop() ||
        Polygon->Polygon->GetNumberOfSplinePoints() != 64 ||
        GoogleOverlays.Num() != 1 || CwtOverlays.Num() != 1 ||
        GoogleRasterOverlays.Num() != 1 ||
        CwtRasterOverlays.Num() != 1 ||
        GoogleRasterOverlays[0] != GoogleOverlays[0] ||
        CwtRasterOverlays[0] != CwtOverlays[0] ||
        !OverlayMatches(GoogleOverlays[0], GoogleTileset, Polygon) ||
        !OverlayMatches(CwtOverlays[0], CwtTileset, Polygon))
    {
        OutError = FString::Printf(
            TEXT("R33 requires one shared 64-point site clip and exactly one total raster overlay per tileset, pointer-identical to its active clipping overlay; polygon=%d googleClippingOverlays=%d cwtClippingOverlays=%d googleRasterOverlays=%d cwtRasterOverlays=%d."),
            PolygonCount,
            GoogleOverlays.Num(),
            CwtOverlays.Num(),
            GoogleRasterOverlays.Num(),
            CwtRasterOverlays.Num());
        return false;
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    ResetReadinessCounters()
{
    PresentReadySamples = 0;
    RestoreFailureSamples = 0;
    ReadinessAccumulatorSeconds = 0.0f;
    CwtWarmingElapsedSeconds = 0.0f;
}

void ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    ArmCwtRetryBackoff()
{
    if (!bCwtPresentationRequested)
    {
        bCwtLoadFailureRetryLatched = false;
        CwtRetryBackoffRemainingSeconds = 0.0f;
        return;
    }
    bCwtLoadFailureRetryLatched = true;
    CwtRetryBackoffRemainingSeconds =
        R33CwtLoadFailureRetryBackoffSeconds;
}

void ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    RegisterCwtLoadFailureHandler()
{
    if (CwtLoadFailureDelegateHandle.IsValid())
    {
        return;
    }
    CwtLoadFailureDelegateHandle = OnCesium3DTilesetLoadFailure.AddUObject(
        this,
        &ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
            HandleCwtTilesetLoadFailure);
}

void ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    UnregisterCwtLoadFailureHandler()
{
    if (!CwtLoadFailureDelegateHandle.IsValid())
    {
        return;
    }
    OnCesium3DTilesetLoadFailure.Remove(CwtLoadFailureDelegateHandle);
    CwtLoadFailureDelegateHandle.Reset();
}

void ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    HandleCwtTilesetLoadFailure(
        const FCesium3DTilesetLoadFailureDetails& Details)
{
    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld() || !HasActorBegunPlay() ||
        !bConfigured || !bCwtPresentationRequested ||
        !Details.Tileset.IsValid() || !IsValid(CwtTileset) ||
        CwtTileset->GetWorld() != World ||
        Details.Tileset.Get() != CwtTileset.Get() ||
        (PresentationState ==
             ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal &&
         bCwtLoadFailureRetryLatched))
    {
        return;
    }

    // Edge-latch before transitioning. Cesium can queue several failures for
    // one request burst, and late in-flight callbacks may arrive after CWT is
    // suspended.
    ArmCwtRetryBackoff();
    const bool bRequiresExplicitRetry =
        Details.HttpStatusCode == 401 || Details.HttpStatusCode == 403;
    FString SafeLocalError;
    const bool bSafeLocalApplied = EnterSafeLocal(SafeLocalError);
    if (bRequiresExplicitRetry)
    {
        // Authentication/authorization will not be repaired by a timed
        // refresh. Preserve SafeLocal and require a new operator request.
        bCwtPresentationRequested = false;
        bCwtLoadFailureRetryLatched = false;
        CwtRetryBackoffRemainingSeconds = 0.0f;
    }
    UE_LOG(
        LogTRIADR33CesiumWorldTerrainReference,
        Warning,
        TEXT("R33 CWT load failure type=%d httpStatus=%d SafeLocal=%s automaticRetry=%s retryBackoffSeconds=%.1f; provider message intentionally omitted."),
        static_cast<int32>(Details.Type),
        Details.HttpStatusCode,
        bSafeLocalApplied ? TEXT("true") : TEXT("false"),
        bRequiresExplicitRetry ? TEXT("false") : TEXT("true"),
        R33CwtLoadFailureRetryBackoffSeconds);
}

bool ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::ApplyState(
    ETRIADIstanaExploreV5DR33TerrainPresentationState NewState,
    FString& OutError)
{
    UWorld* World = GetWorld();
    const auto FailClosedToSafeLocal = [&]()
    {
        ApplyR33SurvivingRolesSafeLocal(GoogleTileset, CwtTileset);
        PresentationState =
            ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal;
        if (World && IsValid(ContextPolicy) &&
            ContextPolicy->GetWorld() == World)
        {
            FString IgnoredContextError;
            ContextPolicy->ApplyR33CesiumPresentationMode(
                this,
                false,
                true,
                IgnoredContextError);
        }
        ResetReadinessCounters();
        ArmCwtRetryBackoff();
    };
    if (!World || !IsValid(GoogleTileset) || !IsValid(CwtTileset) ||
        !IsValid(ContextPolicy) || GoogleTileset->GetWorld() != World ||
        CwtTileset->GetWorld() != World || ContextPolicy->GetWorld() != World ||
        !IsValid(GoogleTileset->GetCesiumIonServer()) ||
        !IsValid(CwtTileset->GetCesiumIonServer()) ||
        GoogleTileset->GetCesiumIonServer() !=
            CwtTileset->GetCesiumIonServer())
    {
        FailClosedToSafeLocal();
        OutError = TEXT("R33 cannot transition without both streamed roles and the context policy.");
        return false;
    }

    // Make the state transition itself visibly hide-before-show even if its
    // reusable readback helper is changed later.
    GoogleTileset->SetActorHiddenInGame(true);
    CwtTileset->SetActorHiddenInGame(true);
    bool bApplied = false;
    switch (NewState)
    {
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary:
        bApplied = ApplyExclusivePresentationState(
            GoogleTileset,
            CwtTileset,
            true,
            false,
            false,
            true,
            OutError);
        break;
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtWarming:
        bApplied = ApplyExclusivePresentationState(
            GoogleTileset,
            CwtTileset,
            true,
            false,
            false,
            false,
            OutError);
        break;
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented:
        bApplied = ApplyExclusivePresentationState(
            GoogleTileset,
            CwtTileset,
            false,
            true,
            true,
            false,
            OutError);
        break;
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal:
        bApplied = ApplyExclusivePresentationState(
            GoogleTileset,
            CwtTileset,
            false,
            true,
            false,
            true,
            OutError);
        break;
    default:
        OutError = TEXT("R33 refused an unknown presentation state.");
        FailClosedToSafeLocal();
        return false;
    }

    if (bApplied &&
        (NewState ==
             ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary ||
         NewState ==
             ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtWarming))
    {
        GoogleTileset->SetActorHiddenInGame(false);
    }
    if (bApplied && NewState ==
        ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented)
    {
        CwtTileset->SetActorHiddenInGame(false);
    }

    if (!bApplied)
    {
        FailClosedToSafeLocal();
        return false;
    }

    const bool bCwtPresented =
        NewState ==
        ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented;
    const bool bSafeLocal =
        NewState ==
        ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal;
    if (!ContextPolicy->ApplyR33CesiumPresentationMode(
            this,
            bCwtPresented,
            bSafeLocal,
            OutError))
    {
        FString Ignored;
        ApplyExclusivePresentationState(
            GoogleTileset,
            CwtTileset,
            false,
            true,
            false,
            true,
            Ignored);
        FailClosedToSafeLocal();
        return false;
    }

    PresentationState = NewState;
    ResetReadinessCounters();
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    EnterGooglePrimary(FString& OutError)
{
    return ApplyState(
        ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary,
        OutError);
}

bool ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    EnterCwtWarming(FString& OutError)
{
    if (bCwtLoadFailureRetryLatched)
    {
        OutError = TEXT("R33 CWT warming remains blocked by the active load-failure retry backoff.");
        return false;
    }
    return ApplyState(
        ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtWarming,
        OutError);
}

bool ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    EnterCwtPresented(FString& OutError)
{
    return ApplyState(
        ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented,
        OutError);
}

bool ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    EnterSafeLocal(FString& OutError)
{
    // Any fail-closed transition while CWT remains requested must not bounce
    // straight back into warming on the next readiness sample.
    ArmCwtRetryBackoff();
    return ApplyState(
        ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal,
        OutError);
}

bool ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::ConfigureR33CesiumWorldTerrainReference(
        ACesium3DTileset* InGoogleTileset,
        ACesium3DTileset* InCwtTileset,
        ACesiumGeoreference* InGeoreference,
        ATRIADIstanaExploreV5DContextPolicyActor* InContextPolicy,
        FString& OutError)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        OutError = TEXT("R33 configuration requires an editor world.");
        return false;
    }
    if (World->IsGameWorld() || World->WorldType != EWorldType::Editor ||
        HasActorBegunPlay() || CwtLoadFailureDelegateHandle.IsValid())
    {
        OutError = TEXT("R33 configuration is an editor-world-only map-build transaction; runtime worlds must use serialized bindings and BeginPlay registration.");
        return false;
    }
    if (bConfigured || !IsValid(InGoogleTileset) ||
        !IsValid(InCwtTileset) || !IsValid(InGeoreference) ||
        !IsValid(InContextPolicy) ||
        InGoogleTileset->GetWorld() != World ||
        InCwtTileset->GetWorld() != World ||
        InGeoreference->GetWorld() != World ||
        InContextPolicy->GetWorld() != World ||
        InGoogleTileset == InCwtTileset ||
        !GetActorTransform().Equals(FTransform::Identity, R33TransformTolerance) ||
        !InGoogleTileset->GetActorTransform().Equals(
            FTransform::Identity,
            R33TransformTolerance) ||
        !InCwtTileset->GetActorTransform().Equals(
            FTransform::Identity,
            R33TransformTolerance) ||
        !InGoogleTileset->Tags.Contains(GoogleRoleTag) ||
        InCwtTileset->Tags.Contains(GoogleRoleTag) ||
        !IsValid(InGoogleTileset->GetCesiumIonServer()) ||
        !HasExpectedGoogleTuple(InGoogleTileset) ||
        !InGeoreference->Tags.Contains(R33GeoreferenceTag) ||
        !InGeoreference->Tags.Contains(R33DefaultGeoreferenceTag) ||
        !HasExpectedGeoreferenceValues(InGeoreference))
    {
        OutError = TEXT("R33 configuration requires exact same-world identity Google/CWT roles, the admitted Google tuple with a valid opaque Cesium Ion server, one identity controller, and the exact Istana georeference.");
        return false;
    }

    int32 TotalTilesetCount = 0;
    int32 GeoreferenceCount = 0;
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        TotalTilesetCount += IsValid(*It) ? 1 : 0;
    }
    for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
    {
        GeoreferenceCount += IsValid(*It) ? 1 : 0;
    }
    if (TotalTilesetCount != 2 || GeoreferenceCount != 1)
    {
        OutError = TEXT("R33 configuration refuses anything except the exact two-tileset/one-georeference world roster.");
        return false;
    }

    const auto GoogleTilesetBindingBefore = GoogleTileset;
    const auto CwtTilesetBindingBefore = CwtTileset;
    const auto SharedGeoreferenceBindingBefore = SharedGeoreference;
    const auto ContextPolicyBindingBefore = ContextPolicy;
    const bool bControllerCollisionBefore = GetActorEnableCollision();
    const FString ClaimLabelBefore = ClaimLabel;
    const bool bConfiguredBefore = bConfigured;
    const bool bVisualReferenceOnlyBefore = bVisualReferenceOnly;
    const bool bHeightSamplesPersistedBefore = bHeightSamplesPersisted;
    const bool bSurveyAccuracyClaimedBefore = bSurveyAccuracyClaimed;
    const bool bVerticalDatumResolvedBefore = bVerticalDatumResolved;
    const bool bCollisionAuthorityBefore =
        bCollisionNavigationLineOfSightSensorOrRfAuthority;
    const bool bVisualAcceptanceBefore = bVisualAcceptance;
    const auto PresentationStateBefore = PresentationState;
    const int32 PresentReadySamplesBefore = PresentReadySamples;
    const int32 RestoreFailureSamplesBefore = RestoreFailureSamples;
    const bool bCwtPresentationRequestedBefore =
        bCwtPresentationRequested;
    const bool bCwtLoadFailureRetryLatchedBefore =
        bCwtLoadFailureRetryLatched;
    const float CwtRetryBackoffRemainingSecondsBefore =
        CwtRetryBackoffRemainingSeconds;
    const float CwtWarmingElapsedSecondsBefore =
        CwtWarmingElapsedSeconds;
    const float ReadinessAccumulatorSecondsBefore =
        ReadinessAccumulatorSeconds;
    const bool bLoggedRuntimeFailureBefore = bLoggedRuntimeFailure;
    const auto RestoreControllerState = [&]()
    {
        GoogleTileset = GoogleTilesetBindingBefore;
        CwtTileset = CwtTilesetBindingBefore;
        SharedGeoreference = SharedGeoreferenceBindingBefore;
        ContextPolicy = ContextPolicyBindingBefore;
        SetActorEnableCollision(bControllerCollisionBefore);
        ClaimLabel = ClaimLabelBefore;
        bConfigured = bConfiguredBefore;
        bVisualReferenceOnly = bVisualReferenceOnlyBefore;
        bHeightSamplesPersisted = bHeightSamplesPersistedBefore;
        bSurveyAccuracyClaimed = bSurveyAccuracyClaimedBefore;
        bVerticalDatumResolved = bVerticalDatumResolvedBefore;
        bCollisionNavigationLineOfSightSensorOrRfAuthority =
            bCollisionAuthorityBefore;
        bVisualAcceptance = bVisualAcceptanceBefore;
        PresentationState = PresentationStateBefore;
        PresentReadySamples = PresentReadySamplesBefore;
        RestoreFailureSamples = RestoreFailureSamplesBefore;
        bCwtPresentationRequested = bCwtPresentationRequestedBefore;
        bCwtLoadFailureRetryLatched =
            bCwtLoadFailureRetryLatchedBefore;
        CwtRetryBackoffRemainingSeconds =
            CwtRetryBackoffRemainingSecondsBefore;
        CwtWarmingElapsedSeconds = CwtWarmingElapsedSecondsBefore;
        ReadinessAccumulatorSeconds = ReadinessAccumulatorSecondsBefore;
        bLoggedRuntimeFailure = bLoggedRuntimeFailureBefore;
    };

    // The shared clipping topology is fully inspectable before any provider
    // property is changed. Bind temporarily for the member validator, then
    // restore the controller's prior binding on a failed preflight.
    GoogleTileset = InGoogleTileset;
    CwtTileset = InCwtTileset;
    SharedGeoreference = InGeoreference;
    ContextPolicy = InContextPolicy;
    if (!ValidateSharedProviderSiteClip(OutError))
    {
        RestoreControllerState();
        return false;
    }

    const TArray<FName> GoogleTagsBefore = InGoogleTileset->Tags;
    const TArray<FName> CwtTagsBefore = InCwtTileset->Tags;
    const bool bGoogleHiddenBefore = InGoogleTileset->IsHidden();
    const bool bGoogleSuspendUpdateBefore = InGoogleTileset->SuspendUpdate;
    const bool bGoogleEnableFogCullingBefore =
        InGoogleTileset->EnableFogCulling;
    const bool bCwtHiddenBefore = InCwtTileset->IsHidden();
    const bool bCwtSuspendUpdateBefore = InCwtTileset->SuspendUpdate;
    const ETilesetSource CwtSourceBefore = InCwtTileset->GetTilesetSource();
    const int64 CwtIonAssetIdBefore = InCwtTileset->GetIonAssetID();
    const auto CwtGeoreferenceBefore = InCwtTileset->GetGeoreference();
    const auto CwtIonServerBefore = InCwtTileset->GetCesiumIonServer();
    const double CwtMaximumSseBefore =
        InCwtTileset->GetMaximumScreenSpaceError();
    const EApplyDpiScaling CwtDpiScalingBefore =
        InCwtTileset->ApplyDpiScaling;
    const bool bCwtCreatePhysicsMeshesBefore =
        InCwtTileset->GetCreatePhysicsMeshes();
    const bool bCwtCreateNavCollisionBefore =
        InCwtTileset->GetCreateNavCollision();
    const bool bCwtActorCollisionBefore =
        InCwtTileset->GetActorEnableCollision();
    const bool bCwtShowCreditsBefore = InCwtTileset->ShowCreditsOnScreen;
    const bool bCwtPreloadAncestorsBefore = InCwtTileset->PreloadAncestors;
    const bool bCwtPreloadSiblingsBefore = InCwtTileset->PreloadSiblings;
    const bool bCwtForbidHolesBefore = InCwtTileset->ForbidHoles;
    const int64 CwtMaximumCachedBytesBefore =
        InCwtTileset->MaximumCachedBytes;
    const int32 CwtMaximumLoadsBefore =
        InCwtTileset->MaximumSimultaneousTileLoads;
    const int32 CwtLoadingDescendantLimitBefore =
        InCwtTileset->LoadingDescendantLimit;
    const auto RestoreInputState = [&]()
    {
        InGoogleTileset->Tags = GoogleTagsBefore;
        InCwtTileset->Tags = CwtTagsBefore;
        InGoogleTileset->SetActorHiddenInGame(bGoogleHiddenBefore);
        InGoogleTileset->SuspendUpdate = bGoogleSuspendUpdateBefore;
        InGoogleTileset->EnableFogCulling = bGoogleEnableFogCullingBefore;
        InCwtTileset->SetActorHiddenInGame(bCwtHiddenBefore);
        InCwtTileset->SuspendUpdate = bCwtSuspendUpdateBefore;
        InCwtTileset->SetTilesetSource(CwtSourceBefore);
        InCwtTileset->SetIonAssetID(CwtIonAssetIdBefore);
        InCwtTileset->SetGeoreference(CwtGeoreferenceBefore);
        InCwtTileset->SetCesiumIonServer(CwtIonServerBefore);
        InCwtTileset->SetMaximumScreenSpaceError(CwtMaximumSseBefore);
        InCwtTileset->ApplyDpiScaling = CwtDpiScalingBefore;
        InCwtTileset->SetCreatePhysicsMeshes(
            bCwtCreatePhysicsMeshesBefore);
        InCwtTileset->SetCreateNavCollision(bCwtCreateNavCollisionBefore);
        InCwtTileset->SetActorEnableCollision(bCwtActorCollisionBefore);
        InCwtTileset->ShowCreditsOnScreen = bCwtShowCreditsBefore;
        InCwtTileset->PreloadAncestors = bCwtPreloadAncestorsBefore;
        InCwtTileset->PreloadSiblings = bCwtPreloadSiblingsBefore;
        InCwtTileset->ForbidHoles = bCwtForbidHolesBefore;
        InCwtTileset->MaximumCachedBytes = CwtMaximumCachedBytesBefore;
        InCwtTileset->MaximumSimultaneousTileLoads =
            CwtMaximumLoadsBefore;
        InCwtTileset->LoadingDescendantLimit =
            CwtLoadingDescendantLimitBefore;
        InCwtTileset->RefreshTileset();
    };

    InGoogleTileset->Tags.AddUnique(R33TerrainContextMemberTag);
    InCwtTileset->Tags.AddUnique(CwtRoleTag);
    InCwtTileset->Tags.AddUnique(R33TerrainContextMemberTag);
    TSoftObjectPtr<ACesiumGeoreference> Georeference(InGeoreference);
    InCwtTileset->SetActorHiddenInGame(true);
    InCwtTileset->SuspendUpdate = true;
    InCwtTileset->SetTilesetSource(ETilesetSource::FromCesiumIon);
    InCwtTileset->SetIonAssetID(CesiumWorldTerrainIonAssetId);
    InCwtTileset->SetGeoreference(Georeference);
    InCwtTileset->SetCesiumIonServer(InGoogleTileset->GetCesiumIonServer());
    InCwtTileset->SetMaximumScreenSpaceError(CesiumWorldTerrainMaximumScreenSpaceError);
    InCwtTileset->ApplyDpiScaling = EApplyDpiScaling::No;
    InCwtTileset->SetCreatePhysicsMeshes(false);
    InCwtTileset->SetCreateNavCollision(false);
    InCwtTileset->SetActorEnableCollision(false);
    InCwtTileset->ShowCreditsOnScreen = true;
    InCwtTileset->PreloadAncestors = true;
    InCwtTileset->PreloadSiblings = false;
    InCwtTileset->ForbidHoles = true;
    InCwtTileset->MaximumCachedBytes = CwtMaximumCachedBytes;
    InCwtTileset->MaximumSimultaneousTileLoads =
        CwtMaximumSimultaneousTileLoads;
    InCwtTileset->LoadingDescendantLimit = CwtLoadingDescendantLimit;
    InCwtTileset->RefreshTileset();
    if (!IsValid(InCwtTileset->GetCesiumIonServer()) ||
        !IsValid(InGoogleTileset->GetCesiumIonServer()) ||
        InCwtTileset->GetCesiumIonServer() != InGoogleTileset->GetCesiumIonServer())
    {
        RestoreInputState();
        RestoreControllerState();
        OutError = TEXT("R33 CWT did not retain the exact Google-role Cesium Ion server binding.");
        return false;
    }
    if (!HasExpectedCwtTuple(InCwtTileset))
    {
        RestoreInputState();
        RestoreControllerState();
        OutError = TEXT("R33 CWT did not retain the complete visual-only presentation/loading tuple.");
        return false;
    }

    ClaimLabel = R33TerrainClaim;
    bConfigured = true;
    bVisualReferenceOnly = true;
    bHeightSamplesPersisted = false;
    bSurveyAccuracyClaimed = false;
    bVerticalDatumResolved = false;
    bCollisionNavigationLineOfSightSensorOrRfAuthority = false;
    bVisualAcceptance = false;
    bCwtPresentationRequested = false;
    bCwtLoadFailureRetryLatched = false;
    CwtRetryBackoffRemainingSeconds = 0.0f;
    CwtWarmingElapsedSeconds = 0.0f;
    bLoggedRuntimeFailure = false;
    SetActorEnableCollision(false);

    ACesium3DTileset* ResolvedGoogle = nullptr;
    ACesium3DTileset* ResolvedCwt = nullptr;
    ACesiumGeoreference* ResolvedGeoreference = nullptr;
    if (!ResolveExactWorldRoster(
            ResolvedGoogle,
            ResolvedCwt,
            ResolvedGeoreference,
            OutError) ||
        ResolvedGoogle != GoogleTileset || ResolvedCwt != CwtTileset ||
        ResolvedGeoreference != SharedGeoreference ||
        !ValidateSharedProviderSiteClip(OutError) ||
        !ContextPolicy->RegisterR33CesiumWorldTerrainController(
            this,
            GoogleTileset,
            CwtTileset,
            OutError) ||
        !EnterGooglePrimary(OutError))
    {
        FString Ignored;
        ApplyExclusivePresentationState(
            GoogleTileset,
            CwtTileset,
            false,
            true,
            false,
            true,
            Ignored);
        ContextPolicy->UnregisterR33CesiumWorldTerrainController(this);
        RestoreInputState();
        RestoreControllerState();
        return false;
    }

    FString Report;
    if (!ValidateR33CesiumWorldTerrainReference(Report))
    {
        FString Ignored;
        EnterSafeLocal(Ignored);
        ContextPolicy->UnregisterR33CesiumWorldTerrainController(this);
        RestoreInputState();
        RestoreControllerState();
        OutError = Report;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::ValidateR33CesiumWorldTerrainReference(
    FString& OutReport) const
{
    ACesium3DTileset* ResolvedGoogle = nullptr;
    ACesium3DTileset* ResolvedCwt = nullptr;
    ACesiumGeoreference* Georeference = nullptr;
    UWorld* World = GetWorld();
    FString Error;
    if (!World || !bConfigured || !ResolveExactWorldRoster(
            ResolvedGoogle,
            ResolvedCwt,
            Georeference,
            Error) ||
        ResolvedGoogle != GoogleTileset || ResolvedCwt != CwtTileset ||
        Georeference != SharedGeoreference || !IsValid(GoogleTileset) ||
        !IsValid(CwtTileset) || !IsValid(SharedGeoreference) ||
        !IsValid(ContextPolicy) ||
        !IsValid(GoogleTileset->GetCesiumIonServer()) ||
        !IsValid(CwtTileset->GetCesiumIonServer()) ||
        GoogleTileset->GetCesiumIonServer() !=
            CwtTileset->GetCesiumIonServer() ||
        ContextPolicy->GetWorld() != GetWorld() ||
        GoogleTileset->GetWorld() != World ||
        CwtTileset->GetWorld() != World ||
        Georeference->GetWorld() != World ||
        GoogleTileset == CwtTileset)
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R33_CWT_INVALID_ROSTER: ") +
            Error;
        return false;
    }

    if (!GetActorTransform().Equals(FTransform::Identity, R33TransformTolerance) ||
        GetActorEnableCollision() || Tags.Num() != 1 ||
        !Tags.Contains(ControllerTag) || ClaimLabel != R33TerrainClaim ||
        !bVisualReferenceOnly || bHeightSamplesPersisted ||
        bSurveyAccuracyClaimed || bVerticalDatumResolved ||
        bCollisionNavigationLineOfSightSensorOrRfAuthority ||
        bVisualAcceptance || !SceneRoot || GetRootComponent() != SceneRoot ||
        SceneRoot->GetOwner() != this || SceneRoot->GetAttachParent() ||
        SceneRoot->Mobility != EComponentMobility::Static ||
        !SceneRoot->GetRelativeTransform().Equals(
            FTransform::Identity,
            R33TransformTolerance) ||
        !HasExpectedGoogleTuple(GoogleTileset) ||
        !HasExpectedCwtTuple(CwtTileset) ||
        !HasExpectedGeoreferenceValues(Georeference) ||
        !GoogleTileset->GetMaterial() ||
        !GoogleTileset->GetTranslucentMaterial() ||
        CwtTileset->GetMaterial() != GoogleTileset->GetMaterial() ||
        CwtTileset->GetTranslucentMaterial() !=
            GoogleTileset->GetTranslucentMaterial() ||
        !GoogleTileset->GetActorTransform().Equals(
            FTransform::Identity,
            R33TransformTolerance) ||
        !CwtTileset->GetActorTransform().Equals(
            FTransform::Identity,
            R33TransformTolerance) ||
        GoogleTileset->GetGeoreference() != CwtTileset->GetGeoreference() ||
        GoogleTileset->GetGeoreference().Get() != Georeference ||
        GoogleTileset->ResolveGeoreference() != Georeference ||
        CwtTileset->ResolveGeoreference() != Georeference ||
        PresentReadySamples < 0 ||
        PresentReadySamples > PresentRequiredConsecutiveSamples ||
        RestoreFailureSamples < 0 ||
        RestoreFailureSamples > RestoreRequiredConsecutiveSamples ||
        !FMath::IsFinite(CwtRetryBackoffRemainingSeconds) ||
        CwtRetryBackoffRemainingSeconds < 0.0f ||
        CwtRetryBackoffRemainingSeconds >
            R33CwtLoadFailureRetryBackoffSeconds ||
        !FMath::IsFinite(CwtWarmingElapsedSeconds) ||
        CwtWarmingElapsedSeconds < 0.0f ||
        CwtWarmingElapsedSeconds > R33CwtWarmingTimeoutSeconds ||
        (PresentationState !=
             ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtWarming &&
         !FMath::IsNearlyZero(CwtWarmingElapsedSeconds)) ||
        (bCwtLoadFailureRetryLatched &&
         (!bCwtPresentationRequested ||
          PresentationState !=
              ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal)) ||
        (!bCwtLoadFailureRetryLatched &&
         !FMath::IsNearlyZero(CwtRetryBackoffRemainingSeconds)) ||
        (World->IsGameWorld() && HasActorBegunPlay() &&
         !CwtLoadFailureDelegateHandle.IsValid()) ||
        !FMath::IsNearlyEqual(
            PrimaryActorTick.TickInterval,
            ReadinessSampleIntervalSeconds) ||
        !ValidateSharedProviderSiteClip(Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R33_CWT_INVALID_TUPLE_OR_TRUTH_BOUNDARY: ") +
            Error;
        return false;
    }

    bool bExpectedGoogleVisible = false;
    bool bExpectedGoogleSuspended = true;
    bool bExpectedCwtVisible = false;
    bool bExpectedCwtSuspended = true;
    switch (PresentationState)
    {
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary:
        bExpectedGoogleVisible = true;
        bExpectedGoogleSuspended = false;
        break;
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtWarming:
        bExpectedGoogleVisible = true;
        bExpectedGoogleSuspended = false;
        bExpectedCwtSuspended = false;
        break;
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented:
        bExpectedCwtVisible = true;
        bExpectedCwtSuspended = false;
        break;
    case ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal:
        break;
    default:
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R33_CWT_INVALID_STATE");
        return false;
    }

    const bool bGoogleVisible = !GoogleTileset->IsHidden();
    const bool bCwtVisible = !CwtTileset->IsHidden();
    if ((bGoogleVisible && bCwtVisible) ||
        bGoogleVisible != bExpectedGoogleVisible ||
        bCwtVisible != bExpectedCwtVisible ||
        GoogleTileset->SuspendUpdate != bExpectedGoogleSuspended ||
        CwtTileset->SuspendUpdate != bExpectedCwtSuspended ||
        !ContextPolicy->ValidateR33CesiumPresentationBinding(this, Error))
    {
        OutReport = TEXT("ISTANA_EXPLORE_V5D_R33_CWT_INVALID_EXCLUSIVE_PRESENTATION: ") +
            Error;
        return false;
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R33_CWT_VALID state=%d googleIonAsset=%lld cwtIonAsset=%lld googleSse=%.1f cwtSse=%.1f readinessSampleSeconds=%.1f presentThresholdPercent=%.1f presentSamples=%d restoreThresholdPercent=%.1f restoreSamples=%d cwtLoadFailureRetryLatched=%s cwtRetryBackoffRemainingSeconds=%.1f cwtWarmingElapsedSeconds=%.1f cwtWarmingTimeoutSeconds=%.1f cwtFailureDelegateBound=%s doubleVisible=false hiddenStreamingVerifiedInPie=false sourceTerrainAuthorityUnchanged=true visualReferenceOnly=true heightSamplesPersisted=false surveyAccuracyClaimed=false verticalDatumResolved=false collisionAuthority=false navigationAuthority=false lineOfSightAuthority=false sensorAuthority=false rfGeometryAuthority=false visualAcceptance=false nativeMapApplicationClaimed=false"),
        static_cast<int32>(PresentationState),
        GooglePhotorealisticIonAssetId,
        CesiumWorldTerrainIonAssetId,
        GoogleMaximumScreenSpaceError,
        CesiumWorldTerrainMaximumScreenSpaceError,
        ReadinessSampleIntervalSeconds,
        PresentLoadProgressPercent,
        PresentRequiredConsecutiveSamples,
        RestoreLoadProgressPercent,
        RestoreRequiredConsecutiveSamples,
        bCwtLoadFailureRetryLatched ? TEXT("true") : TEXT("false"),
        CwtRetryBackoffRemainingSeconds,
        CwtWarmingElapsedSeconds,
        R33CwtWarmingTimeoutSeconds,
        CwtLoadFailureDelegateHandle.IsValid() ? TEXT("true") : TEXT("false"));
    return true;
}

bool ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    RequestCwtPresentation(bool bRequest, FString& OutError)
{
    if (!bConfigured || !IsValid(GoogleTileset) || !IsValid(CwtTileset) ||
        !IsValid(ContextPolicy) || !IsValid(SharedGeoreference) ||
        !IsValid(GoogleTileset->GetCesiumIonServer()) ||
        !IsValid(CwtTileset->GetCesiumIonServer()) ||
        GoogleTileset->GetCesiumIonServer() !=
            CwtTileset->GetCesiumIonServer())
    {
        ApplyR33SurvivingRolesSafeLocal(GoogleTileset, CwtTileset);
        PresentationState =
            ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal;
        ResetReadinessCounters();
        ArmCwtRetryBackoff();
        OutError = TEXT("R33 cannot change presentation before exact configuration.");
        return false;
    }
    bCwtPresentationRequested = bRequest;
    if (!bRequest)
    {
        bCwtLoadFailureRetryLatched = false;
        CwtRetryBackoffRemainingSeconds = 0.0f;
        return EnterGooglePrimary(OutError);
    }
    if (bCwtLoadFailureRetryLatched)
    {
        OutError.Reset();
        return true;
    }
    if (PresentationState ==
        ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented ||
        PresentationState ==
        ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtWarming)
    {
        OutError.Reset();
        return true;
    }
    // CWT may have failed its eager construction load before this controller
    // could subscribe. A fresh explicit request always starts one observable
    // load attempt whose asynchronous failure is covered by the delegate.
    CwtTileset->RefreshTileset();
    return EnterCwtWarming(OutError);
}

bool ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
    TickPresentationState(float DeltaSeconds, FString& OutError)
{
    UWorld* World = GetWorld();
    if (!bConfigured || !World || !IsValid(GoogleTileset) ||
        !IsValid(CwtTileset) || !IsValid(ContextPolicy) ||
        !IsValid(SharedGeoreference) || GoogleTileset->GetWorld() != World ||
        CwtTileset->GetWorld() != World || ContextPolicy->GetWorld() != World ||
        SharedGeoreference->GetWorld() != World ||
        !IsValid(GoogleTileset->GetCesiumIonServer()) ||
        !IsValid(CwtTileset->GetCesiumIonServer()) ||
        GoogleTileset->GetCesiumIonServer() !=
            CwtTileset->GetCesiumIonServer() ||
        !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0f ||
        !FMath::IsFinite(CwtRetryBackoffRemainingSeconds) ||
        CwtRetryBackoffRemainingSeconds < 0.0f ||
        CwtRetryBackoffRemainingSeconds >
            R33CwtLoadFailureRetryBackoffSeconds ||
        !FMath::IsFinite(CwtWarmingElapsedSeconds) ||
        CwtWarmingElapsedSeconds < 0.0f ||
        CwtWarmingElapsedSeconds > R33CwtWarmingTimeoutSeconds ||
        (World->IsGameWorld() && HasActorBegunPlay() &&
         !CwtLoadFailureDelegateHandle.IsValid()))
    {
        ApplyR33SurvivingRolesSafeLocal(GoogleTileset, CwtTileset);
        PresentationState =
            ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal;
        ResetReadinessCounters();
        ArmCwtRetryBackoff();
        OutError = TEXT("R33 readiness tick requires finite time and an exact configuration.");
        return false;
    }
    if (PresentationState ==
        ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtWarming)
    {
        CwtWarmingElapsedSeconds = FMath::Min(
            R33CwtWarmingTimeoutSeconds,
            CwtWarmingElapsedSeconds + DeltaSeconds);
    }
    if (bCwtLoadFailureRetryLatched)
    {
        CwtRetryBackoffRemainingSeconds = FMath::Max(
            0.0f,
            CwtRetryBackoffRemainingSeconds - DeltaSeconds);
    }
    ReadinessAccumulatorSeconds += DeltaSeconds;
    if (ReadinessAccumulatorSeconds + KINDA_SMALL_NUMBER <
        ReadinessSampleIntervalSeconds)
    {
        OutError.Reset();
        return true;
    }
    ReadinessAccumulatorSeconds = FMath::Fmod(
        ReadinessAccumulatorSeconds,
        ReadinessSampleIntervalSeconds);

    if (PresentationState !=
            ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtWarming &&
        PresentationState !=
            ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented)
    {
        PresentReadySamples = 0;
        RestoreFailureSamples = 0;
        if (PresentationState ==
                ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal &&
            bCwtPresentationRequested)
        {
            if (bCwtLoadFailureRetryLatched &&
                CwtRetryBackoffRemainingSeconds > 0.0f)
            {
                OutError.Reset();
                return true;
            }
            bCwtLoadFailureRetryLatched = false;
            CwtRetryBackoffRemainingSeconds = 0.0f;
            CwtTileset->RefreshTileset();
            return EnterCwtWarming(OutError);
        }
        OutError.Reset();
        return true;
    }

    const float LoadProgress = CwtTileset->GetLoadProgress();
    if (!IsFiniteLoadProgress(LoadProgress))
    {
        ArmCwtRetryBackoff();
        FString SafeLocalError;
        const bool bSafe = EnterSafeLocal(SafeLocalError);
        OutError = TEXT("R33 CWT returned non-finite or out-of-range current-view load progress; SafeLocal=") +
            (bSafe ? FString(TEXT("true")) : FString(TEXT("false"))) +
            TEXT(" ") + SafeLocalError;
        return false;
    }

    if (PresentationState ==
        ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtWarming)
    {
        RestoreFailureSamples = 0;
        PresentReadySamples = LoadProgress >= PresentLoadProgressPercent
            ? FMath::Min(
                  PresentReadySamples + 1,
                  PresentRequiredConsecutiveSamples)
            : 0;
        if (PresentReadySamples >= PresentRequiredConsecutiveSamples)
        {
            return EnterCwtPresented(OutError);
        }
        if (CwtWarmingElapsedSeconds >= R33CwtWarmingTimeoutSeconds)
        {
            ArmCwtRetryBackoff();
            FString SafeLocalError;
            const bool bSafe = EnterSafeLocal(SafeLocalError);
            OutError = FString::Printf(
                TEXT("R33 CWT warming exceeded %.1f seconds without readiness; SafeLocal=%s %s"),
                R33CwtWarmingTimeoutSeconds,
                bSafe ? TEXT("true") : TEXT("false"),
                *SafeLocalError);
            return false;
        }
        OutError.Reset();
        return true;
    }

    PresentReadySamples = 0;
    RestoreFailureSamples = LoadProgress < RestoreLoadProgressPercent
        ? FMath::Min(
              RestoreFailureSamples + 1,
              RestoreRequiredConsecutiveSamples)
        : 0;
    if (RestoreFailureSamples >= RestoreRequiredConsecutiveSamples)
    {
        // Load progress is scoped to the current view. A camera move can make
        // an otherwise healthy, already presented tileset temporarily fall
        // below the restore threshold while new tiles stream. Return to the
        // normal warming state, where Google remains visible, instead of
        // misclassifying view-dependent tile churn as a provider failure.
        // Explicit Cesium failures and the bounded warming timeout still own
        // the SafeLocal/backoff path.
        return EnterCwtWarming(OutError);
    }
    OutError.Reset();
    return true;
}

void ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::BeginPlay()
{
    Super::BeginPlay();
    if (!bConfigured)
    {
        return;
    }
    RegisterCwtLoadFailureHandler();
    FString Error;
    if (!CwtLoadFailureDelegateHandle.IsValid())
    {
        Error = TEXT("R33 could not subscribe to the Cesium tileset-load-failure delegate.");
    }
    if (!CwtLoadFailureDelegateHandle.IsValid() ||
        !IsValid(ContextPolicy) || !IsValid(GoogleTileset) ||
        !IsValid(CwtTileset) || !IsValid(SharedGeoreference) ||
        !IsValid(GoogleTileset->GetCesiumIonServer()) ||
        !IsValid(CwtTileset->GetCesiumIonServer()) ||
        GoogleTileset->GetCesiumIonServer() !=
            CwtTileset->GetCesiumIonServer() ||
        !ContextPolicy->RegisterR33CesiumWorldTerrainController(
            this,
            GoogleTileset,
            CwtTileset,
            Error) ||
        !ApplyState(PresentationState, Error))
    {
        FString Ignored;
        ApplyR33SurvivingRolesSafeLocal(GoogleTileset, CwtTileset);
        PresentationState =
            ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal;
        if (IsValid(ContextPolicy))
        {
            ContextPolicy->ApplyR33CesiumPresentationMode(
                this,
                false,
                true,
                Ignored);
        }
        ResetReadinessCounters();
        ArmCwtRetryBackoff();
        bLoggedRuntimeFailure = true;
        UE_LOG(
            LogTRIADR33CesiumWorldTerrainReference,
            Error,
            TEXT("%s"),
            *Error);
    }
}

void ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::Tick(
    float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bConfigured)
    {
        return;
    }
    FString Error;
    if (!TickPresentationState(DeltaSeconds, Error))
    {
        FString Ignored;
        ApplyR33SurvivingRolesSafeLocal(GoogleTileset, CwtTileset);
        PresentationState =
            ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal;
        if (IsValid(ContextPolicy))
        {
            ContextPolicy->ApplyR33CesiumPresentationMode(
                this,
                false,
                true,
                Ignored);
        }
        ResetReadinessCounters();
        ArmCwtRetryBackoff();
        if (!bLoggedRuntimeFailure)
        {
            UE_LOG(
                LogTRIADR33CesiumWorldTerrainReference,
                Warning,
                TEXT("%s"),
                *Error);
        }
        bLoggedRuntimeFailure = true;
        return;
    }
    bLoggedRuntimeFailure = false;
}

void ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    UnregisterCwtLoadFailureHandler();
    ApplyR33SurvivingRolesSafeLocal(GoogleTileset, CwtTileset);
    PresentationState =
        ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal;
    if (IsValid(ContextPolicy))
    {
        FString ContextError;
        ContextPolicy->ApplyR33CesiumPresentationMode(
            this,
            false,
            true,
            ContextError);
        ContextPolicy->UnregisterR33CesiumWorldTerrainController(this);
    }
    ResetReadinessCounters();
    bCwtPresentationRequested = false;
    bCwtLoadFailureRetryLatched = false;
    CwtRetryBackoffRemainingSeconds = 0.0f;
    Super::EndPlay(EndPlayReason);
}
