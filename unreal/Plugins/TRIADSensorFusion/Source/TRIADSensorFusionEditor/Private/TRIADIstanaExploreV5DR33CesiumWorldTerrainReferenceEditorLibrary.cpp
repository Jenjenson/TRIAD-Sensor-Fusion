#include "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceEditorLibrary.h"

// Cesium public headers use numeric_limits::max; some Windows SDK layouts
// leave a function-like max macro active.
#ifdef max
#undef max
#endif

#include "AssetCompilingManager.h"
#include "Cesium3DTileset.h"
#include "CesiumCartographicPolygon.h"
#include "CesiumGeoreference.h"
#include "CesiumIonServer.h"
#include "CesiumPolygonRasterOverlay.h"
#include "CesiumRasterOverlay.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "ScopedTransaction.h"
#include "Ssl.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary.h"
#include "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.h"
#include "UObject/Package.h"

#if WITH_SSL
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
#include <openssl/sha.h>
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#endif

namespace
{
const FString R33NativeTargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString R33NativeTransactionRelativeRoot(
    TEXT("TRIAD/NativeTransactions/V5DCesiumWorldTerrainReferenceR33V1"));
const FName R33NativeContextPolicyTag(
    TEXT("TRIADIstanaExploreV5DContextPolicy"));
const FName R33NativeGoogleRoleTag(
    TEXT("TRIADIstanaExploreV5DVisualTileset"));
const FName R33NativeCwtRoleTag(
    TEXT("TRIADIstanaExploreV5DR33CesiumWorldTerrainTileset"));
const FName R33NativeContextMemberTag(
    TEXT("TRIADIstanaExploreV5DR33CesiumContextMember"));
const FName R33NativeGeoreferenceTag(
    TEXT("TRIADIstanaExploreV5DGeoreference"));
const FName R33NativeDefaultGeoreferenceTag(TEXT("DEFAULT_GEOREFERENCE"));
const FName R33NativeSiteClipTag(
    TEXT("TRIADIstanaExploreV5DProviderSiteClip"));
const FName R33NativeCwtActorName(
    TEXT("CesiumWorldTerrain_IstanaV5D_R33"));
const FName R33NativeControllerActorName(
    TEXT("TRIAD_IPV5D_R33_CesiumWorldTerrainReference"));
constexpr double R33NativeTransformTolerance = 0.001;

bool R33NativeIsValidSha256(const FString& Value)
{
    if (Value.Len() != 64)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!FChar::IsHexDigit(Character))
        {
            return false;
        }
    }
    return true;
}

FString R33NativeBytesToHex(const uint8* Bytes, int32 Count)
{
    FString Result;
    Result.Reserve(Count * 2);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Result += FString::Printf(TEXT("%02X"), Bytes[Index]);
    }
    return Result;
}

bool R33NativeHashFileSha256(
    const FString& Filename,
    FString& OutSha256,
    int64& OutBytes,
    FString& OutError)
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
    {
        OutError = TEXT("Could not read guarded R33 file: ") + Filename;
        return false;
    }
    OutBytes = Bytes.Num();
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(Bytes.GetData(), static_cast<size_t>(Bytes.Num()), Digest) ==
        nullptr)
    {
        OutError = TEXT("SHA-256 failed for guarded R33 file: ") + Filename;
        return false;
    }
    OutSha256 = R33NativeBytesToHex(Digest, SHA256_DIGEST_LENGTH);
#else
    OutError = TEXT("R33 map commit requires WITH_SSL SHA-256 support.");
    return false;
#endif
    OutError.Reset();
    return true;
}

UWorld* R33NativeGetExactLoadedTargetWorld(FString& OutError)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const FString PackageName = World && World->GetOutermost()
        ? World->GetOutermost()->GetName()
        : FString();
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress() || !World ||
        World->WorldType != EWorldType::Editor || !World->PersistentLevel ||
        PackageName != R33NativeTargetMapPackage)
    {
        OutError = FString::Printf(
            TEXT("R33 operation requires exact loaded non-PIE editor map '%s'; actual='%s'."),
            *R33NativeTargetMapPackage,
            *PackageName);
        return nullptr;
    }
    OutError.Reset();
    return World;
}

struct FR33NativeWorldRoster
{
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor* Controller =
        nullptr;
    ACesium3DTileset* Google = nullptr;
    ACesium3DTileset* Cwt = nullptr;
    ACesiumGeoreference* Georeference = nullptr;
    ACesiumCartographicPolygon* SiteClip = nullptr;
    int32 PolicyCount = 0;
    int32 ControllerClassCount = 0;
    int32 ControllerTagCount = 0;
    int32 TotalTilesetCount = 0;
    int32 GoogleRoleCount = 0;
    int32 CwtRoleCount = 0;
    int32 ContextMemberCount = 0;
    int32 GeoreferenceCount = 0;
    int32 SiteClipCount = 0;
};

bool R33NativeResolveWorldRoster(
    UWorld* World,
    FR33NativeWorldRoster& OutRoster,
    FString& OutError)
{
    OutRoster = FR33NativeWorldRoster{};
    if (!World || !World->PersistentLevel)
    {
        OutError = TEXT("R33 roster resolution requires a persistent editor world.");
        return false;
    }

    for (TActorIterator<ATRIADIstanaExploreV5DContextPolicyActor> It(World);
         It;
         ++It)
    {
        if (IsValid(*It))
        {
            OutRoster.Policy = *It;
            ++OutRoster.PolicyCount;
        }
    }
    for (TActorIterator<ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor>
             It(World);
         It;
         ++It)
    {
        ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor* Candidate =
            *It;
        if (IsValid(Candidate))
        {
            OutRoster.Controller = Candidate;
            ++OutRoster.ControllerClassCount;
        }
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (IsValid(*It) && It->Tags.Contains(
                ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
                    ExpectedControllerTag()))
        {
            ++OutRoster.ControllerTagCount;
            if (It->GetClass() !=
                ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
                    StaticClass())
            {
                OutError = TEXT("R33 controller tag is owned by a non-exact class.");
                return false;
            }
        }
    }
    for (TActorIterator<ACesium3DTileset> It(World); It; ++It)
    {
        ACesium3DTileset* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }
        ++OutRoster.TotalTilesetCount;
        if (Candidate->Tags.Contains(R33NativeGoogleRoleTag) &&
            Candidate->GetIonAssetID() ==
                ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
                    ExpectedGoogleIonAssetId())
        {
            OutRoster.Google = Candidate;
            ++OutRoster.GoogleRoleCount;
        }
        if (Candidate->Tags.Contains(R33NativeCwtRoleTag) &&
            Candidate->GetIonAssetID() ==
                ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
                    ExpectedCwtIonAssetId())
        {
            OutRoster.Cwt = Candidate;
            ++OutRoster.CwtRoleCount;
        }
        OutRoster.ContextMemberCount +=
            Candidate->Tags.Contains(R33NativeContextMemberTag) ? 1 : 0;
    }
    for (TActorIterator<ACesiumGeoreference> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            OutRoster.Georeference = *It;
            ++OutRoster.GeoreferenceCount;
        }
    }
    for (TActorIterator<ACesiumCartographicPolygon> It(World); It; ++It)
    {
        if (IsValid(*It) && It->Tags.Contains(R33NativeSiteClipTag))
        {
            OutRoster.SiteClip = *It;
            ++OutRoster.SiteClipCount;
        }
    }

    if (OutRoster.PolicyCount != 1 || !OutRoster.Policy ||
        OutRoster.GeoreferenceCount != 1 || !OutRoster.Georeference ||
        OutRoster.SiteClipCount != 1 || !OutRoster.SiteClip ||
        (OutRoster.Google &&
         !IsValid(OutRoster.Google->GetCesiumIonServer())) ||
        (OutRoster.Cwt &&
         !IsValid(OutRoster.Cwt->GetCesiumIonServer())) ||
        (OutRoster.Google && OutRoster.Cwt &&
         OutRoster.Google->GetCesiumIonServer() !=
             OutRoster.Cwt->GetCesiumIonServer()))
    {
        OutError = FString::Printf(
            TEXT("R33 requires one policy, one georeference and one tagged site clip; policy=%d georeference=%d clip=%d."),
            OutRoster.PolicyCount,
            OutRoster.GeoreferenceCount,
            OutRoster.SiteClipCount);
        return false;
    }
    OutError.Reset();
    return true;
}

bool R33NativeOverlayMatches(
    const UCesiumPolygonRasterOverlay* Overlay,
    const ACesium3DTileset* Owner,
    const ACesiumCartographicPolygon* Polygon)
{
    return Overlay && Overlay->GetOwner() == Owner && Overlay->IsActive() &&
        Overlay->MaterialLayerKey == TEXT("Clipping") &&
        !Overlay->InvertSelection && Overlay->ExcludeSelectedTiles &&
        Overlay->Polygons.Num() == 1 && Overlay->Polygons[0].Get() == Polygon;
}

bool R33NativeValidateSharedBase(
    const FR33NativeWorldRoster& Roster,
    FString& OutError)
{
    if (!Roster.Policy || !Roster.Georeference || !Roster.SiteClip ||
        Roster.Policy->GetLevel() != Roster.Policy->GetWorld()->PersistentLevel ||
        !Roster.Policy->Tags.Contains(R33NativeContextPolicyTag) ||
        !Roster.Georeference->Tags.Contains(R33NativeGeoreferenceTag) ||
        !Roster.Georeference->Tags.Contains(R33NativeDefaultGeoreferenceTag) ||
        !Roster.Georeference->GetActorTransform().Equals(
            FTransform::Identity, R33NativeTransformTolerance) ||
        !Roster.SiteClip->GetActorTransform().Equals(
            FTransform::Identity, R33NativeTransformTolerance))
    {
        OutError = TEXT("R33 shared policy/georeference/site-clip identity changed.");
        return false;
    }
    const FVector Origin =
        Roster.Georeference->GetOriginLongitudeLatitudeHeight();
    if (Roster.Georeference->GetOriginPlacement() !=
            EOriginPlacement::CartographicOrigin ||
        !FMath::IsNearlyEqual(Origin.X, 103.84288055, 0.00000001) ||
        !FMath::IsNearlyEqual(Origin.Y, 1.30709615, 0.00000001) ||
        !FMath::IsNearlyEqual(Origin.Z, 47.0, 0.000001) ||
        !FMath::IsNearlyEqual(Roster.Georeference->GetScale(), 100.0, 0.001) ||
        !Roster.SiteClip->Polygon || !Roster.SiteClip->Polygon->IsClosedLoop() ||
        Roster.SiteClip->Polygon->GetNumberOfSplinePoints() != 64)
    {
        OutError = TEXT("R33 exact Istana georeference or 64-point site clip changed.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool R33NativeValidateR32Predecessor(
    UWorld* World,
    FR33NativeWorldRoster& OutRoster,
    FString& OutReport)
{
    FString R32Report;
    FString RosterError;
    if (!World ||
        !UTRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary::
            ValidateR32MediumDistanceTurfInLoadedV5DHybridMap(R32Report) ||
        !R33NativeResolveWorldRoster(World, OutRoster, RosterError) ||
        !R33NativeValidateSharedBase(OutRoster, RosterError) ||
        OutRoster.TotalTilesetCount != 1 ||
        OutRoster.GoogleRoleCount != 1 || !OutRoster.Google ||
        OutRoster.CwtRoleCount != 0 || OutRoster.Cwt ||
        OutRoster.ContextMemberCount != 0 ||
        OutRoster.ControllerClassCount != 0 || OutRoster.Controller ||
        OutRoster.ControllerTagCount != 0 ||
        OutRoster.Google->GetLevel() != World->PersistentLevel ||
        !OutRoster.Google->GetActorTransform().Equals(
            FTransform::Identity, R33NativeTransformTolerance) ||
        OutRoster.Google->GetGeoreference().Get() != OutRoster.Georeference ||
        OutRoster.Google->ResolveGeoreference() != OutRoster.Georeference ||
        !IsValid(OutRoster.Google->GetCesiumIonServer()) ||
        OutRoster.Google->GetTilesetSource() != ETilesetSource::FromCesiumIon ||
        !FMath::IsNearlyEqual(
            OutRoster.Google->GetMaximumScreenSpaceError(), 1.0, 0.000001) ||
        OutRoster.Google->ApplyDpiScaling != EApplyDpiScaling::No ||
        OutRoster.Google->GetCreatePhysicsMeshes() ||
        OutRoster.Google->GetCreateNavCollision() ||
        OutRoster.Google->GetActorEnableCollision() ||
        !OutRoster.Google->ShowCreditsOnScreen)
    {
        OutReport = TEXT("R33 predecessor validation failed; exact committed and human-accepted R30/R31/R32 chain must precede this endpoint. r32={") +
            R32Report + TEXT("} roster={") + RosterError + TEXT("}");
        return false;
    }
    TArray<UCesiumPolygonRasterOverlay*> GoogleOverlays;
    OutRoster.Google->GetComponents(GoogleOverlays);
    if (GoogleOverlays.Num() != 1 ||
        !R33NativeOverlayMatches(
            GoogleOverlays[0], OutRoster.Google, OutRoster.SiteClip))
    {
        OutReport = TEXT("R33 predecessor has no exact active Google site-clipping overlay.");
        return false;
    }
    OutReport = TEXT("R33_R32_PREDECESSOR_VALID exactR32Turf=true tilesets=1 googleAsset=2275207 cwtOwners=0 r33Controllers=0 georeferences=1 sharedSiteClip=true simulationAuthorityUnchanged=true receiptGateOwnedByExternalWrapper=true.");
    return true;
}

bool R33NativeValidateSuccessorWorld(
    UWorld* World,
    FR33NativeWorldRoster& OutRoster,
    FString& OutReport)
{
    FString R32Report;
    FString RosterError;
    FString BindingError;
    FString ControllerReport;
    if (!World ||
        !R33NativeResolveWorldRoster(World, OutRoster, RosterError) ||
        !R33NativeValidateSharedBase(OutRoster, RosterError) ||
        OutRoster.TotalTilesetCount != 2 ||
        OutRoster.GoogleRoleCount != 1 || !OutRoster.Google ||
        OutRoster.CwtRoleCount != 1 || !OutRoster.Cwt ||
        OutRoster.ContextMemberCount != 2 ||
        OutRoster.ControllerClassCount != 1 || !OutRoster.Controller ||
        OutRoster.ControllerTagCount != 1 ||
        OutRoster.Controller->GetFName() != R33NativeControllerActorName ||
        OutRoster.Cwt->GetFName() != R33NativeCwtActorName ||
        OutRoster.Google == OutRoster.Cwt ||
        OutRoster.Controller->GetLevel() != World->PersistentLevel ||
        OutRoster.Cwt->GetLevel() != World->PersistentLevel ||
        !OutRoster.Controller->GetActorTransform().Equals(
            FTransform::Identity, R33NativeTransformTolerance) ||
        !OutRoster.Cwt->GetActorTransform().Equals(
            FTransform::Identity, R33NativeTransformTolerance) ||
        OutRoster.Google->GetGeoreference().Get() != OutRoster.Georeference ||
        OutRoster.Cwt->GetGeoreference().Get() != OutRoster.Georeference ||
        !IsValid(OutRoster.Google->GetCesiumIonServer()) ||
        !IsValid(OutRoster.Cwt->GetCesiumIonServer()) ||
        OutRoster.Google->GetCesiumIonServer() !=
            OutRoster.Cwt->GetCesiumIonServer() ||
        !OutRoster.Google->GetMaterial() ||
        !OutRoster.Google->GetTranslucentMaterial() ||
        OutRoster.Cwt->GetMaterial() != OutRoster.Google->GetMaterial() ||
        OutRoster.Cwt->GetTranslucentMaterial() !=
            OutRoster.Google->GetTranslucentMaterial())
    {
        OutReport = TEXT("R33 successor validation failed: r32={") +
            R32Report + TEXT("} roster={") + RosterError +
            TEXT("} controller={") + ControllerReport + TEXT("}");
        return false;
    }

    // The policy/controller association is intentionally transient: it must
    // never be serialized as simulation or provider authority. Reconstruct
    // that exact runtime-only binding after a cold editor reload before asking
    // the controller to validate it. Without this step, the post-save cold
    // validation always sees empty weak pointers and rejects a valid map.
    if (!OutRoster.Policy->RegisterR33CesiumWorldTerrainController(
            OutRoster.Controller,
            OutRoster.Google,
            OutRoster.Cwt,
            BindingError))
    {
        OutReport = TEXT("R33 successor transient binding reconstruction failed: binding={") +
            BindingError + TEXT("}");
        return false;
    }

    // Registration above deliberately reconstructs transient state that is not
    // part of the map transaction. If any subsequent validation fails, clear
    // that state before returning so a read-only validation cannot mutate an
    // invalid world and an apply failure can still validate the restored R32
    // predecessor after UndoTransaction. A successful validation keeps the
    // binding for the loaded R33 world.
    bool bKeepReconstructedBinding = false;
    ON_SCOPE_EXIT
    {
        if (!bKeepReconstructedBinding && IsValid(OutRoster.Policy))
        {
            OutRoster.Policy->UnregisterR33CesiumWorldTerrainController(
                OutRoster.Controller);
        }
    };

    // The inherited R32 validator descends through ContextPolicy's hybrid
    // validation. It must run after the transient dual-provider binding has
    // been rebuilt, otherwise a cold-loaded two-tileset R33 map is mistaken
    // for a legacy one-tileset predecessor and rejected before controller
    // validation can run.
    if (!UTRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary::
            ValidateR32MediumDistanceTurfInLoadedV5DHybridMap(R32Report) ||
        !OutRoster.Controller->ValidateR33CesiumWorldTerrainReference(
            ControllerReport))
    {
        OutReport = TEXT("R33 successor validation failed after transient binding reconstruction: r32={") +
            R32Report + TEXT("} controller={") + ControllerReport + TEXT("}");
        return false;
    }
    TArray<UCesiumPolygonRasterOverlay*> GoogleOverlays;
    TArray<UCesiumPolygonRasterOverlay*> CwtOverlays;
    TArray<UCesiumRasterOverlay*> GoogleRasterOverlays;
    TArray<UCesiumRasterOverlay*> CwtRasterOverlays;
    OutRoster.Google->GetComponents(GoogleOverlays);
    OutRoster.Cwt->GetComponents(CwtOverlays);
    OutRoster.Google->GetComponents(GoogleRasterOverlays);
    OutRoster.Cwt->GetComponents(CwtRasterOverlays);
    if (GoogleOverlays.Num() != 1 || CwtOverlays.Num() != 1 ||
        GoogleRasterOverlays.Num() != 1 ||
        CwtRasterOverlays.Num() != 1 ||
        GoogleRasterOverlays[0] != GoogleOverlays[0] ||
        CwtRasterOverlays[0] != CwtOverlays[0] ||
        !R33NativeOverlayMatches(
            GoogleOverlays[0], OutRoster.Google, OutRoster.SiteClip) ||
        !R33NativeOverlayMatches(
            CwtOverlays[0], OutRoster.Cwt, OutRoster.SiteClip))
    {
        OutReport = TEXT("R33 successor must have exactly one total raster overlay per tileset, pointer-identical to its sole shared site-clipping overlay; imagery and undeclared raster overlays are forbidden.");
        return false;
    }
    bKeepReconstructedBinding = true;
    OutReport = TEXT("ISTANA_EXPLORE_V5D_R33_CESIUM_WORLD_TERRAIN_REFERENCE_MAP_VALID mapIntegrated=true addOnly=true exactR32Turf=true exactTilesets=2 googleIonAsset=2275207 cwtIonAsset=1 sharedGeoreference=true sharedIonServer=true sharedSiteClip=true initialState=GooglePrimary cwtHidden=true cwtSuspended=true providerCreditsVisible=true visualReferenceOnly=true sourceTerrainAuthorityUnchanged=true collision=false navigation=false lineOfSightAuthority=false sensorAuthority=false rfGeometryAuthority=false verticalDatumResolved=false surveyAccuracyClaimed=false visualCaptureAccepted=false captureRevalidationRequired=true. ") +
        ControllerReport;
    return true;
}

bool R33NativeUndoAndValidateR32(
    UWorld* World,
    TUniquePtr<FScopedTransaction>& Transaction,
    bool bPackageWasDirty,
    FString& OutReport)
{
    if (!GEditor || !World || !Transaction || !Transaction->IsOutstanding())
    {
        OutReport = TEXT("R33 in-memory rollback could not close an outstanding editor transaction.");
        return false;
    }
    Transaction.Reset();
    const bool bUndoSucceeded = GEditor->UndoTransaction(false);
    FR33NativeWorldRoster Restored;
    FString PredecessorReport;
    const bool bPredecessorRestored = bUndoSucceeded &&
        R33NativeValidateR32Predecessor(World, Restored, PredecessorReport);
    UPackage* Package = World->GetOutermost();
    if (Package)
    {
        Package->SetDirtyFlag(bPackageWasDirty);
    }
    const bool bDirtyStateRestored =
        Package && Package->IsDirty() == bPackageWasDirty;
    if (!bPredecessorRestored || !bDirtyStateRestored)
    {
        OutReport = TEXT("R33 undo failed to restore the exact R32 predecessor; external wrapper rollback is required. predecessor={") +
            PredecessorReport + TEXT("}");
        return false;
    }
    OutReport = TEXT("R33_IN_MEMORY_ROLLBACK_VALID undoTransaction=true exactR32Turf=true tilesets=1 cwtOwners=0 r33Controllers=0 packageDirtyStateRestored=true.");
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceEditorLibrary::
    ApplyR33CesiumWorldTerrainReferenceToLoadedV5DHybridMap(
        FString& OutMessage)
{
    FString Error;
    UWorld* World = R33NativeGetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutMessage = TEXT("EXPLORE_V5D_R33_CWT_APPLY_REFUSED_WORLD: ") + Error;
        return false;
    }

    FR33NativeWorldRoster ExistingSuccessor;
    FString ExistingSuccessorReport;
    if (R33NativeValidateSuccessorWorld(
            World, ExistingSuccessor, ExistingSuccessorReport))
    {
        OutMessage = TEXT("IDEMPOTENT_EXPLORE_V5D_R33_CWT_ALREADY_VALID mapSaved=false transactionOpened=false actorSpawned=false exactTilesets=2 exactR33Controller=1 sourceTerrainAuthorityUnchanged=true visualCaptureAccepted=false captureRevalidationRequired=true. ") +
            ExistingSuccessorReport;
        return true;
    }

    FR33NativeWorldRoster Predecessor;
    FString PredecessorReport;
    if (!R33NativeValidateR32Predecessor(
            World, Predecessor, PredecessorReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R33_CWT_APPLY_REFUSED_PREDECESSOR: ") +
            PredecessorReport;
        return false;
    }
    if (!IsValid(Predecessor.Google->GetCesiumIonServer()))
    {
        OutMessage = TEXT("EXPLORE_V5D_R33_CWT_APPLY_REFUSED_OPAQUE_SERVER: predecessor Google role has no valid Cesium Ion server.");
        return false;
    }

    const bool bPackageWasDirty = World->GetOutermost()->IsDirty();
    TUniquePtr<FScopedTransaction> Transaction =
        MakeUnique<FScopedTransaction>(NSLOCTEXT(
            "TRIAD",
            "ApplyIstanaExploreV5DR33CesiumWorldTerrainReference",
            "Apply Istana Explore V5D R33 Cesium World Terrain Reference"));
    if (!Transaction || !Transaction->IsOutstanding())
    {
        OutMessage = TEXT("EXPLORE_V5D_R33_CWT_APPLY_REFUSED_NO_UNDO_TRANSACTION: no map mutation was attempted.");
        return false;
    }

    World->Modify();
    Predecessor.Google->Modify();
    Predecessor.Policy->Modify();
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.OverrideLevel = World->PersistentLevel;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParameters.ObjectFlags |= RF_Transactional;

    SpawnParameters.Name = R33NativeCwtActorName;
    ACesium3DTileset* Cwt = World->SpawnActor<ACesium3DTileset>(
        ACesium3DTileset::StaticClass(),
        FTransform::Identity,
        SpawnParameters);
    if (!Cwt || Cwt->GetLevel() != World->PersistentLevel ||
        Cwt->GetFName() != R33NativeCwtActorName)
    {
        FString RollbackReport;
        const bool bRolledBack = R33NativeUndoAndValidateR32(
            World, Transaction, bPackageWasDirty, RollbackReport);
        OutMessage = FString(TEXT("EXPLORE_V5D_R33_CWT_APPLY_FAILED_CWT_SPAWN inMemoryRollback=")) +
            (bRolledBack ? TEXT("true ") : TEXT("false ")) + RollbackReport;
        return false;
    }
    Cwt->Modify();
#if WITH_EDITOR
    Cwt->SetActorLabel(
        TEXT("Cesium World Terrain - Istana V5D R33 Visual Reference Only"));
#endif
    Cwt->SetMaterial(Predecessor.Google->GetMaterial());
    Cwt->SetTranslucentMaterial(
        Predecessor.Google->GetTranslucentMaterial());
    UCesiumPolygonRasterOverlay* CwtSiteClip =
        NewObject<UCesiumPolygonRasterOverlay>(
            Cwt,
            TEXT("IstanaV5DR33AuthoredCoreClippingOverlay"),
            RF_Transactional);
    if (!CwtSiteClip)
    {
        FString RollbackReport;
        const bool bRolledBack = R33NativeUndoAndValidateR32(
            World, Transaction, bPackageWasDirty, RollbackReport);
        OutMessage = FString(TEXT("EXPLORE_V5D_R33_CWT_APPLY_FAILED_CLIP_CREATE inMemoryRollback=")) +
            (bRolledBack ? TEXT("true ") : TEXT("false ")) + RollbackReport;
        return false;
    }
    CwtSiteClip->MaterialLayerKey = TEXT("Clipping");
    CwtSiteClip->Polygons.Add(
        TSoftObjectPtr<ACesiumCartographicPolygon>(Predecessor.SiteClip));
    CwtSiteClip->InvertSelection = false;
    CwtSiteClip->ExcludeSelectedTiles = true;
    Cwt->AddInstanceComponent(CwtSiteClip);
    CwtSiteClip->SetAutoActivate(true);
    CwtSiteClip->RegisterComponent();

    SpawnParameters.Name = R33NativeControllerActorName;
    ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor* Controller =
        World->SpawnActor<
            ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor>(
            ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor::
                StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    if (!Controller || Controller->GetLevel() != World->PersistentLevel ||
        Controller->GetFName() != R33NativeControllerActorName)
    {
        FString RollbackReport;
        const bool bRolledBack = R33NativeUndoAndValidateR32(
            World, Transaction, bPackageWasDirty, RollbackReport);
        OutMessage = FString(TEXT("EXPLORE_V5D_R33_CWT_APPLY_FAILED_CONTROLLER_SPAWN inMemoryRollback=")) +
            (bRolledBack ? TEXT("true ") : TEXT("false ")) + RollbackReport;
        return false;
    }
    Controller->Modify();
#if WITH_EDITOR
    Controller->SetActorLabel(
        TEXT("TRIAD Istana V5D R33 CWT Visual Reference Controller"));
#endif

    FString ConfigureError;
    if (!Controller->ConfigureR33CesiumWorldTerrainReference(
            Predecessor.Google,
            Cwt,
            Predecessor.Georeference,
            Predecessor.Policy,
            ConfigureError))
    {
        FString RollbackReport;
        const bool bRolledBack = R33NativeUndoAndValidateR32(
            World, Transaction, bPackageWasDirty, RollbackReport);
        OutMessage = TEXT("EXPLORE_V5D_R33_CWT_APPLY_FAILED_CONFIGURE: ") +
            ConfigureError + TEXT(" inMemoryRollback=") +
            (bRolledBack ? TEXT("true ") : TEXT("false ")) + RollbackReport;
        return false;
    }

    FR33NativeWorldRoster Successor;
    FString SuccessorReport;
    if (!R33NativeValidateSuccessorWorld(World, Successor, SuccessorReport))
    {
        FString RollbackReport;
        const bool bRolledBack = R33NativeUndoAndValidateR32(
            World, Transaction, bPackageWasDirty, RollbackReport);
        OutMessage = FString(TEXT("EXPLORE_V5D_R33_CWT_APPLY_FAILED_VALIDATION inMemoryRollback=")) +
            (bRolledBack ? TEXT("true") : TEXT("false")) +
            TEXT(" successor={") + SuccessorReport + TEXT("} rollback={") +
            RollbackReport + TEXT("}");
        return false;
    }

    Transaction.Reset();
    OutMessage = TEXT("ISTANA_EXPLORE_V5D_R33_CESIUM_WORLD_TERRAIN_REFERENCE_APPLY_PASS mapSaved=false callerOwnsSingleCommit=true addOnly=true actorsAdded=2 actorsRemoved=0 exactR32Turf=true exactTilesets=2 googleIonAsset=2275207 cwtIonAsset=1 sharedGeoreference=true sharedIonServer=true sharedSiteClip=true initialState=GooglePrimary cwtHidden=true cwtSuspended=true visualReferenceOnly=true sourceTerrainAuthorityUnchanged=true collision=false navigation=false lineOfSightAuthority=false sensorAuthority=false rfGeometryAuthority=false verticalDatumResolved=false surveyAccuracyClaimed=false externalR30R31R32CommittedAndHumanAcceptedReceiptChainRequiredBeforeInvocation=true receiptGateOwnedByExternalWrapper=true visualCaptureAccepted=false captureRevalidationRequired=true. ") +
        SuccessorReport;
    return true;
}

bool UTRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceEditorLibrary::
    ValidateR33CesiumWorldTerrainReferenceInLoadedV5DHybridMap(
        FString& OutReport)
{
    FString Error;
    UWorld* World = R33NativeGetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutReport = Error;
        return false;
    }
    FR33NativeWorldRoster Roster;
    return R33NativeValidateSuccessorWorld(World, Roster, OutReport);
}

bool UTRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceEditorLibrary::
    CommitR33CesiumWorldTerrainReferenceToLoadedV5DHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& VerifiedExternalBackupFilename,
        FString& OutReport)
{
    FString Error;
    UWorld* World = R33NativeGetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutReport = TEXT("EXPLORE_V5D_R33_CWT_COMMIT_REFUSED_WORLD: ") + Error;
        return false;
    }

    FString DestinationFilename;
    const FString ExpectedSha256 = ExpectedPredecessorSha256.ToUpper();
    FString BackupFilename = FPaths::ConvertRelativePathToFull(
        VerifiedExternalBackupFilename);
    FString TransactionRoot = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(), R33NativeTransactionRelativeRoot));
    FPaths::NormalizeFilename(BackupFilename);
    FPaths::NormalizeDirectoryName(TransactionRoot);
    if (ExpectedPredecessorBytes <= 0 ||
        !R33NativeIsValidSha256(ExpectedSha256) ||
        !FPackageName::DoesPackageExist(
            R33NativeTargetMapPackage, &DestinationFilename) ||
        DestinationFilename.IsEmpty() || BackupFilename.IsEmpty() ||
        FPaths::IsSamePath(DestinationFilename, BackupFilename) ||
        !FPaths::IsUnderDirectory(BackupFilename, TransactionRoot) ||
        !World->GetOutermost() || World->GetOutermost()->IsDirty())
    {
        OutReport = TEXT("EXPLORE_V5D_R33_CWT_COMMIT_REFUSED_INPUT: exact clean R32 map, positive preimage bytes/SHA-256, and an external backup below the bounded R33 transaction root are required; external R30/R31/R32 committed-and-human-accepted receipt admission must already have passed.");
        return false;
    }
    DestinationFilename = FPaths::ConvertRelativePathToFull(
        DestinationFilename);
    FPaths::NormalizeFilename(DestinationFilename);

    FString MapSha256;
    FString BackupSha256;
    int64 MapBytes = INDEX_NONE;
    int64 BackupBytes = INDEX_NONE;
    FString MapError;
    FString BackupError;
    if (!R33NativeHashFileSha256(
            DestinationFilename, MapSha256, MapBytes, MapError) ||
        !R33NativeHashFileSha256(
            BackupFilename, BackupSha256, BackupBytes, BackupError) ||
        MapBytes != ExpectedPredecessorBytes ||
        MapSha256 != ExpectedSha256 ||
        BackupBytes != ExpectedPredecessorBytes ||
        BackupSha256 != ExpectedSha256)
    {
        OutReport = FString::Printf(
            TEXT("EXPLORE_V5D_R33_CWT_COMMIT_REFUSED_PREIMAGE mapBytes=%lld mapSha256=%s backupBytes=%lld backupSha256=%s mapError={%s} backupError={%s}"),
            MapBytes,
            *MapSha256,
            BackupBytes,
            *BackupSha256,
            *MapError,
            *BackupError);
        return false;
    }

    FR33NativeWorldRoster Predecessor;
    FString PredecessorReport;
    if (!R33NativeValidateR32Predecessor(
            World, Predecessor, PredecessorReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R33_CWT_COMMIT_REFUSED_PREDECESSOR: ") +
            PredecessorReport;
        return false;
    }

    FString ApplyReport;
    if (!ApplyR33CesiumWorldTerrainReferenceToLoadedV5DHybridMap(
            ApplyReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R33_CWT_COMMIT_FAILED_NO_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            ApplyReport;
        return false;
    }

    FString PreSaveMapSha256;
    FString PreSaveBackupSha256;
    int64 PreSaveMapBytes = INDEX_NONE;
    int64 PreSaveBackupBytes = INDEX_NONE;
    FString PreSaveMapError;
    FString PreSaveBackupError;
    if (!R33NativeHashFileSha256(
            DestinationFilename,
            PreSaveMapSha256,
            PreSaveMapBytes,
            PreSaveMapError) ||
        !R33NativeHashFileSha256(
            BackupFilename,
            PreSaveBackupSha256,
            PreSaveBackupBytes,
            PreSaveBackupError) ||
        PreSaveMapBytes != ExpectedPredecessorBytes ||
        PreSaveMapSha256 != ExpectedSha256 ||
        PreSaveBackupBytes != ExpectedPredecessorBytes ||
        PreSaveBackupSha256 != ExpectedSha256)
    {
        OutReport = TEXT("EXPLORE_V5D_R33_CWT_COMMIT_FAILED_PRE_SAVE_PIN_GATE externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            World, R33NativeTargetMapPackage))
    {
        OutReport = TEXT("EXPLORE_V5D_R33_CWT_COMMIT_FAILED_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }

    UWorld* UnloadWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
    UPackage* UnloadPackage = UnloadWorld
        ? UnloadWorld->GetOutermost()
        : nullptr;
    if (!UnloadWorld || !UnloadPackage ||
        UWorld::RemovePIEPrefix(UnloadPackage->GetName()) ==
            R33NativeTargetMapPackage)
    {
        OutReport = TEXT("EXPLORE_V5D_R33_CWT_COMMIT_FAILED_COLD_UNLOAD externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }
    UWorld* ReloadedWorld =
        UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename);
    if (ReloadedWorld)
    {
        FAssetCompilingManager::Get().FinishAllCompilation();
    }
    FString ColdReport;
    if (!ReloadedWorld || !ReloadedWorld->GetOutermost() ||
        ReloadedWorld->GetOutermost()->IsDirty() ||
        !ValidateR33CesiumWorldTerrainReferenceInLoadedV5DHybridMap(
            ColdReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R33_CWT_COMMIT_FAILED_COLD_VALIDATION externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            ColdReport;
        return false;
    }

    FString SuccessorSha256;
    FString FinalBackupSha256;
    int64 SuccessorBytes = INDEX_NONE;
    int64 FinalBackupBytes = INDEX_NONE;
    FString SuccessorError;
    FString FinalBackupError;
    const bool bSuccessorChanged = R33NativeHashFileSha256(
        DestinationFilename,
        SuccessorSha256,
        SuccessorBytes,
        SuccessorError) &&
        SuccessorBytes > 0 && R33NativeIsValidSha256(SuccessorSha256) &&
        (SuccessorBytes != ExpectedPredecessorBytes ||
         SuccessorSha256 != ExpectedSha256);
    const bool bBackupPreserved = R33NativeHashFileSha256(
        BackupFilename,
        FinalBackupSha256,
        FinalBackupBytes,
        FinalBackupError) &&
        FinalBackupBytes == ExpectedPredecessorBytes &&
        FinalBackupSha256 == ExpectedSha256;
    if (!bSuccessorChanged || !bBackupPreserved)
    {
        OutReport = TEXT("EXPLORE_V5D_R33_CWT_COMMIT_FAILED_FINAL_RECEIPT externalBackupVerified=true rollbackOwnedByWrapper=true successorError={") +
            SuccessorError + TEXT("} backupError={") +
            FinalBackupError + TEXT("}");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R33_CESIUM_WORLD_TERRAIN_REFERENCE_COMMIT_PASS oneSave=true coldReload=true predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s externalBackupBytes=%lld externalBackupSha256=%s exactR32Turf=true exactTilesets=2 googleIonAsset=2275207 cwtIonAsset=1 sharedGeoreference=true sharedIonServer=true sharedSiteClip=true initialState=GooglePrimary cwtHidden=true cwtSuspended=true visualReferenceOnly=true sourceTerrainAuthorityUnchanged=true collision=false navigation=false lineOfSightAuthority=false sensorAuthority=false rfGeometryAuthority=false verticalDatumResolved=false surveyAccuracyClaimed=false externalR30R31R32CommittedAndHumanAcceptedReceiptChainRequiredBeforeInvocation=true receiptGateOwnedByExternalWrapper=true visualCaptureAccepted=false captureRevalidationRequired=true. %s %s"),
        ExpectedPredecessorBytes,
        *ExpectedSha256,
        SuccessorBytes,
        *SuccessorSha256,
        FinalBackupBytes,
        *FinalBackupSha256,
        *ApplyReport,
        *ColdReport);
    return true;
}
