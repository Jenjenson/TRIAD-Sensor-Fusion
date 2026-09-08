#include "TRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Ssl.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DLandmarkVegetationActor.h"
#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"
#include "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.h"
#include "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.h"
#include "TRIADIstanaExploreV5DR30FacadeLookdevActor.h"
#include "TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory.h"
#include "TRIADIstanaExploreV5DContextPolicyActor.h"
#include "TRIADIstanaExploreV5DTreeRealismActor.h"
#include "TRIADIstanaPublicViewSceneActor.h"
#include "UObject/Package.h"

// A clean R28-only predecessor does not yet have the separately promoted R29
// vegetation source. Keep that build valid, while enabling the full typed
// validator automatically after the R29 vegetation transaction has landed.
#if __has_include("TRIADIstanaExploreV5DR29VegetationActor.h")
#include "TRIADIstanaExploreV5DR29VegetationActor.h"
#define TRIAD_HAS_R29_VEGETATION_ACTOR 1
#else
#define TRIAD_HAS_R29_VEGETATION_ACTOR 0
#endif

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
const FString TargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString R29VegetationClassPath(
    TEXT("/Script/TRIADSensorFusion.TRIADIstanaExploreV5DR29VegetationActor"));
const FName R29VegetationActorTag(
    TEXT("TRIAD_IstanaExploreV5D_R29Vegetation_RenderOnly"));

struct FOwnerRoster
{
    ATRIADIstanaExploreV5DR28EnvironmentActor* R28 = nullptr;
    ATRIADIstanaExploreV5DR29FacadeEnvironmentActor* R29 = nullptr;
    ATRIADIstanaExploreV5DR30FacadeLookdevActor* R30 = nullptr;
    int32 R28ClassCount = 0;
    int32 R28TagCount = 0;
    int32 R29ClassCount = 0;
    int32 R29TagCount = 0;
    int32 R30ClassCount = 0;
    int32 R30TagCount = 0;
    ATRIADIstanaExploreV5DLandmarkVegetationActor* R28Vegetation = nullptr;
    AActor* R29Vegetation = nullptr;
    int32 R28VegetationClassCount = 0;
    int32 R28VegetationTagCount = 0;
    int32 R29VegetationClassCount = 0;
    int32 R29VegetationTagCount = 0;
};

bool IsValidSha256(const FString& Value)
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

FString BytesToHex(const uint8* Bytes, int32 Count)
{
    FString Result;
    Result.Reserve(Count * 2);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Result += FString::Printf(TEXT("%02X"), Bytes[Index]);
    }
    return Result;
}

bool HashFileSha256(
    const FString& Filename,
    FString& OutSha256,
    int64& OutBytes,
    FString& OutError)
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
    {
        OutError = TEXT("Could not read guarded file: ") + Filename;
        return false;
    }
    OutBytes = Bytes.Num();
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(
            Bytes.GetData(),
            static_cast<size_t>(Bytes.Num()),
            Digest) == nullptr)
    {
        OutError = TEXT("SHA-256 failed for guarded file: ") + Filename;
        return false;
    }
    OutSha256 = BytesToHex(Digest, SHA256_DIGEST_LENGTH);
#else
    OutError = TEXT("R30 facade lookdev commit requires WITH_SSL SHA-256 support.");
    return false;
#endif
    OutError.Reset();
    return true;
}

UWorld* GetExactLoadedTargetWorld(FString& OutError)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const FString PackageName = World && World->GetOutermost()
        ? World->GetOutermost()->GetName()
        : FString();
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress() || !World ||
        World->WorldType != EWorldType::Editor || !World->PersistentLevel ||
        PackageName != TargetMapPackage)
    {
        OutError = FString::Printf(
            TEXT("R30 facade lookdev map operation requires exact loaded non-PIE editor map '%s'; actual='%s'."),
            *TargetMapPackage,
            *PackageName);
        return nullptr;
    }
    OutError.Reset();
    return World;
}

bool ResolveOwnerRoster(
    UWorld* World,
    FOwnerRoster& OutRoster,
    FString& OutError)
{
    OutRoster = FOwnerRoster{};
    if (!World)
    {
        OutError = TEXT("R30 facade lookdev owner resolution requires a world.");
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
        const bool bTaggedR29 = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::ExpectedActorTag());
        const bool bExactR30 = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR30FacadeLookdevActor::StaticClass();
        const bool bTaggedR30 = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR30FacadeLookdevActor::ExpectedActorTag());
        const bool bExactR28Vegetation = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DLandmarkVegetationActor::StaticClass();
        const bool bTaggedR28Vegetation = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DLandmarkVegetationActor::ExpectedActorTag());
        const bool bExactR29Vegetation = Candidate && Candidate->GetClass() &&
            Candidate->GetClass()->GetPathName() == R29VegetationClassPath;
        const bool bTaggedR29Vegetation = Candidate && Candidate->Tags.Contains(
            R29VegetationActorTag);
        if (bExactR28)
        {
            ++OutRoster.R28ClassCount;
            OutRoster.R28 =
                Cast<ATRIADIstanaExploreV5DR28EnvironmentActor>(Candidate);
        }
        if (bTaggedR28)
        {
            ++OutRoster.R28TagCount;
        }
        if (bExactR29)
        {
            ++OutRoster.R29ClassCount;
            OutRoster.R29 =
                Cast<ATRIADIstanaExploreV5DR29FacadeEnvironmentActor>(Candidate);
        }
        if (bTaggedR29)
        {
            ++OutRoster.R29TagCount;
        }
        if (bExactR30)
        {
            ++OutRoster.R30ClassCount;
            OutRoster.R30 =
                Cast<ATRIADIstanaExploreV5DR30FacadeLookdevActor>(Candidate);
        }
        if (bTaggedR30)
        {
            ++OutRoster.R30TagCount;
        }
        if (bExactR28Vegetation)
        {
            ++OutRoster.R28VegetationClassCount;
            OutRoster.R28Vegetation =
                Cast<ATRIADIstanaExploreV5DLandmarkVegetationActor>(Candidate);
        }
        if (bTaggedR28Vegetation)
        {
            ++OutRoster.R28VegetationTagCount;
        }
        if (bExactR29Vegetation)
        {
            ++OutRoster.R29VegetationClassCount;
            OutRoster.R29Vegetation = Candidate;
        }
        if (bTaggedR29Vegetation)
        {
            ++OutRoster.R29VegetationTagCount;
        }
        if ((bTaggedR28 && !bExactR28) || (bTaggedR29 && !bExactR29) ||
            (bTaggedR30 && !bExactR30))
        {
            OutError = TEXT("An R28/R29/R30 environment owner tag is held by a non-exact actor class.");
            return false;
        }
        if ((bTaggedR28Vegetation && !bExactR28Vegetation) ||
            (bTaggedR29Vegetation && !bExactR29Vegetation) ||
            (bTaggedR28Vegetation && bTaggedR29Vegetation))
        {
            OutError = TEXT("An R28/R29 vegetation owner tag is impersonated or dual-owned.");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateComposableVegetationOwner(
    const FOwnerRoster& Roster,
    FString& OutOwnerLabel,
    FString& OutReport)
{
    const bool bExactR28 =
        Roster.R28VegetationClassCount == 1 &&
        Roster.R28VegetationTagCount == 1 &&
        Roster.R28Vegetation &&
        Roster.R29VegetationClassCount == 0 &&
        Roster.R29VegetationTagCount == 0 &&
        !Roster.R29Vegetation;
    const bool bExactR29 =
        Roster.R29VegetationClassCount == 1 &&
        Roster.R29VegetationTagCount == 1 &&
        Roster.R29Vegetation &&
        Roster.R28VegetationClassCount == 0 &&
        Roster.R28VegetationTagCount == 0 &&
        !Roster.R28Vegetation;
    if (bExactR28)
    {
        OutOwnerLabel = TEXT("R28");
        if (!Roster.R28Vegetation->ValidateLandmarkVegetationR28(OutReport))
        {
            OutReport = TEXT("Retained R28 vegetation owner is invalid: ") +
                OutReport;
            return false;
        }
        return true;
    }
    if (bExactR29)
    {
        OutOwnerLabel = TEXT("R29");
#if TRIAD_HAS_R29_VEGETATION_ACTOR
        ATRIADIstanaExploreV5DR29VegetationActor* TypedR29Vegetation =
            Cast<ATRIADIstanaExploreV5DR29VegetationActor>(
                Roster.R29Vegetation);
        if (!TypedR29Vegetation ||
            !TypedR29Vegetation->ValidateR29Vegetation(OutReport))
        {
            OutReport = TEXT("R29 vegetation successor owner is invalid: ") +
                OutReport;
            return false;
        }
        return true;
#else
        OutReport = TEXT("R29 vegetation successor is present, but its typed validation contract is not available in this native source tree; run the guarded R29 vegetation transaction first.");
        return false;
#endif
    }
    OutOwnerLabel = TEXT("INVALID");
    OutReport = FString::Printf(
        TEXT("Composable vegetation owner requires exact R28 xor R29: r28Class=%d r28Tag=%d r29Class=%d r29Tag=%d."),
        Roster.R28VegetationClassCount,
        Roster.R28VegetationTagCount,
        Roster.R29VegetationClassCount,
        Roster.R29VegetationTagCount);
    return false;
}

bool HasExactFreshSaveRoster(
    const TArray<UObject*>& Assets,
    FString& OutError)
{
    TArray<FString> Actual;
    TSet<UPackage*> Packages;
    for (UObject* Asset : Assets)
    {
        UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
        if (!Asset || !Package ||
            !Package->HasAnyPackageFlags(PKG_NewlyCreated) ||
            FPackageName::DoesPackageExist(Package->GetName()))
        {
            OutError = TEXT("An R30 facade lookdev save target is null, not newly created, or already persisted.");
            return false;
        }
        Actual.Add(Asset->GetPathName());
        Packages.Add(Package);
    }
    Actual.Sort();
    if (Actual !=
            TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory::
                GetExpectedAssetObjectPaths() ||
        Assets.Num() != 12 || Packages.Num() != 12)
    {
        OutError = TEXT("The R30 facade lookdev fresh save roster is not the exact twelve-package isolated namespace.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool ValidateContextPolicyShell(
    UWorld* World,
    bool bExpectedProviderReady,
    FString& OutReport)
{
    ATRIADIstanaExploreV5DContextPolicyActor* Policy = nullptr;
    ATRIADIstanaPublicViewSceneActor* Scene = nullptr;
    ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor* Terrain = nullptr;
    ATRIADIstanaExploreV5DTreeRealismActor* Trees = nullptr;
    int32 PolicyCount = 0;
    int32 SceneCount = 0;
    int32 TerrainClassCount = 0;
    int32 TerrainTagCount = 0;
    int32 TreeClassCount = 0;
    int32 TreeTagCount = 0;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (Candidate && Candidate->GetClass() ==
                ATRIADIstanaExploreV5DContextPolicyActor::StaticClass())
        {
            ++PolicyCount;
            Policy = Cast<ATRIADIstanaExploreV5DContextPolicyActor>(Candidate);
        }
        if (Candidate && Candidate->GetClass() ==
                ATRIADIstanaPublicViewSceneActor::StaticClass())
        {
            ++SceneCount;
            Scene = Cast<ATRIADIstanaPublicViewSceneActor>(Candidate);
        }
        const bool bExactTerrain = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
                StaticClass();
        const bool bTaggedTerrain = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor::
                ExpectedActorTag());
        TerrainClassCount += bExactTerrain ? 1 : 0;
        TerrainTagCount += bTaggedTerrain ? 1 : 0;
        if (bExactTerrain)
        {
            Terrain = Cast<
                ATRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor>(
                    Candidate);
        }
        if (bTaggedTerrain && !bExactTerrain)
        {
            OutReport = TEXT("R30 refused an R29 terrain tag held by a non-exact actor class.");
            return false;
        }
        const bool bExactTrees = Candidate && Candidate->GetClass() ==
            ATRIADIstanaExploreV5DTreeRealismActor::StaticClass();
        const bool bTaggedTrees = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DTreeRealismActor::ExpectedActorTag());
        TreeClassCount += bExactTrees ? 1 : 0;
        TreeTagCount += bTaggedTrees ? 1 : 0;
        if (bExactTrees)
        {
            Trees = Cast<ATRIADIstanaExploreV5DTreeRealismActor>(Candidate);
        }
        if (bTaggedTrees && !bExactTrees)
        {
            OutReport = TEXT("R30 refused a tree-realism tag held by a non-exact actor class.");
            return false;
        }
    }
    FString HybridReport;
    FString ShellReport;
    FString TerrainReport;
    FString TreeReport;
    if (PolicyCount != 1 || SceneCount != 1 || !Policy || !Scene ||
        TerrainClassCount != 1 || TerrainTagCount != 1 || !Terrain ||
        TreeClassCount != 1 || TreeTagCount != 1 || !Trees ||
        Policy->bLocalBuildingFallbackCurrentlyHidden !=
            bExpectedProviderReady ||
        !Policy->ValidateHybridContext(HybridReport) ||
        !Policy->ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene(
            Scene, ShellReport) ||
        !Terrain->ValidateCopernicusTerrainFallback(TerrainReport) ||
        !Trees->ValidateTreeRealism(TreeReport))
    {
        OutReport = FString::Printf(
            TEXT("R30 requires one valid current-surroundings/context-policy shell, one exact R29 terrain fallback, and one exact tree-realism owner synchronized with provider readiness: policies=%d scenes=%d terrainClasses=%d terrainTags=%d treeClasses=%d treeTags=%d expectedProviderReady=%s hybrid={%s} shell={%s} terrain={%s} trees={%s}."),
            PolicyCount,
            SceneCount,
            TerrainClassCount,
            TerrainTagCount,
            TreeClassCount,
            TreeTagCount,
            bExpectedProviderReady ? TEXT("true") : TEXT("false"),
            *HybridReport,
            *ShellReport,
            *TerrainReport,
            *TreeReport);
        return false;
    }
    OutReport = FString(TEXT("R30_CONTEXT_POLICY_SHELL_VALID exactPolicies=1 exactScenes=1 currentSurroundingsContextFacadeR25=true sourceShellVisibleWithLocalFacade=")) +
        (bExpectedProviderReady ? TEXT("false") : TEXT("true")) +
        TEXT(" providerReady=") +
        (bExpectedProviderReady ? TEXT("true") : TEXT("false")) +
        TEXT(" currentSurroundingsGeometryModified=false providerPolicyModified=false r29TerrainActors=1 r29TerrainTags=1 terrain64EdgeMaskValidated=true terrainCollisionNavigationSensorRfGeospatialHeightAuthority=false treeRealismActors=1 treeRealismTags=1 treeSourceCensus=729 treeDerivativeMeshes=5 treeMaterialResponseV3=true treeResponseMaterials=13 treeRuntimeResponseMids=26 treeAutoLod=true treeWindMaterials=true treePlacementGeometryOpacityWindAuthorityModified=false treeCollisionNavigationSensorRfAuthority=false terrainModified=false vegetationModified=false.");
    return true;
}

bool UndoR30HandoffAndValidateR29(
    UWorld* World,
    TUniquePtr<FScopedTransaction>& Transaction,
    bool bExpectedProviderReady,
    bool bPackageWasDirty,
    FString& OutReport)
{
    if (!GEditor || !World || !Transaction || !Transaction->IsOutstanding())
    {
        OutReport = TEXT("R30 in-memory rollback could not close an outstanding editor transaction.");
        return false;
    }

    // Closing records the actor spawn/removal as one atomic transaction; Undo
    // then restores the exact serialized predecessor instead of rebuilding it.
    Transaction.Reset();
    const bool bUndoSucceeded = GEditor->UndoTransaction(false);
    FOwnerRoster Restored;
    FString RosterError;
    FString R29Report;
    FString ShellReport;
    FString VegetationOwner;
    FString VegetationReport;
    const bool bRestored = bUndoSucceeded &&
        ResolveOwnerRoster(World, Restored, RosterError) &&
        Restored.R28ClassCount == 0 && Restored.R28TagCount == 0 &&
        Restored.R29ClassCount == 1 && Restored.R29TagCount == 1 &&
        Restored.R29 && Restored.R30ClassCount == 0 &&
        Restored.R30TagCount == 0 && !Restored.R30 &&
        Restored.R29->bProviderReady == bExpectedProviderReady &&
        Restored.R29->ValidateR29FacadeEnvironment(R29Report) &&
        ValidateContextPolicyShell(
            World, bExpectedProviderReady, ShellReport) &&
        ValidateComposableVegetationOwner(
            Restored, VegetationOwner, VegetationReport) &&
        VegetationOwner == TEXT("R29");
    if (bRestored && World->GetOutermost())
    {
        World->GetOutermost()->SetDirtyFlag(bPackageWasDirty);
    }
    if (!bRestored)
    {
        OutReport = TEXT("R30 in-memory transaction undo failed to restore the exact validated R29 predecessor; external backup rollback is required. roster={") +
            RosterError + TEXT("} actor={") + R29Report +
            TEXT("} shell={") + ShellReport + TEXT("} vegetationOwner={") +
            VegetationOwner + TEXT("} vegetation={") + VegetationReport +
            TEXT("}");
        return false;
    }
    OutReport = TEXT("R30_IN_MEMORY_ROLLBACK_VALID undoTransaction=true r28Owners=0 r29Owners=1 r30Owners=0 providerReadyRestored=true contextPolicyShellRestored=true vegetationOwner=R29 vegetationValidated=true packageDirtyStateRestored=true.");
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary::
    EnsureR30FacadeLookdevAssets(FString& OutMessage)
{
    FString ExistingReport;
    if (TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory::ValidateAssets(
            ExistingReport))
    {
        OutMessage = TEXT("IDEMPOTENT_EXPLORE_V5D_R30_FACADE_LOOKDEV_ASSETS_ALREADY_VALID: ") +
            ExistingReport;
        return true;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_ASSET_BUILD_REFUSED: editor asset subsystem unavailable.");
        return false;
    }
    TArray<UObject*> FreshAssets;
    FString Error;
    if (!TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory::
            CreateFreshAssets(FreshAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_ASSET_BUILD_FAILED: ") +
            Error;
        return false;
    }
    if (!HasExactFreshSaveRoster(FreshAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_ASSET_BUILD_FAILED_ROSTER: ") +
            Error;
        return false;
    }
    if (!AssetSubsystem->SaveLoadedAssets(FreshAssets, false))
    {
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_ASSET_BUILD_FAILED_ATOMIC_SAVE: exact twelve-asset save request failed; guarded native transaction must restore the initially absent R30 facade lookdev namespace.");
        return false;
    }
    for (UObject* Asset : FreshAssets)
    {
        if (UPackage* Package = Asset ? Asset->GetOutermost() : nullptr)
        {
            Package->SetDirtyFlag(false);
        }
    }
    FString SavedReport;
    if (!TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory::ValidateAssets(
            SavedReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_ASSET_BUILD_FAILED_POST_SAVE_VALIDATION: ") +
            SavedReport;
        return false;
    }
    OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_ASSET_BUILD_PASS exactAssets=12 exactNewPackages=12 mapMutation=false retainedR28PackagesModified=false vegetationPackagesModified=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false visualCaptureAccepted=false captureRevalidationRequired=true. ") +
        SavedReport;
    return true;
}

bool UTRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary::
    ValidateR30FacadeLookdevAssets(FString& OutReport)
{
    return TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory::
        ValidateAssets(OutReport);
}

bool UTRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary::
    ApplyR30FacadeLookdevReplacementToLoadedV5DHybridMap(FString& OutMessage)
{
    FString Error;
    UWorld* World = GetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_REFUSED: ") + Error;
        return false;
    }
    FOwnerRoster Roster;
    if (!ResolveOwnerRoster(World, Roster, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_REFUSED_ROSTER: ") +
            Error;
        return false;
    }
    FString VegetationOwner;
    FString VegetationReport;
    if (!ValidateComposableVegetationOwner(
            Roster, VegetationOwner, VegetationReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_REFUSED_VEGETATION_OWNER: ") +
            VegetationReport;
        return false;
    }
    if (Roster.R28ClassCount == 0 && Roster.R28TagCount == 0 &&
        Roster.R29ClassCount == 0 && Roster.R29TagCount == 0 &&
        Roster.R30ClassCount == 1 && Roster.R30TagCount == 1 && Roster.R30)
    {
        FString ExistingReport;
        if (!ValidateR30FacadeLookdevReplacementInLoadedV5DHybridMap(ExistingReport))
        {
            OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_IDEMPOTENT_VALIDATION_FAILED: ") +
                ExistingReport;
            return false;
        }
        OutMessage = TEXT("IDEMPOTENT_EXPLORE_V5D_R30_FACADE_LOOKDEV_REPLACEMENT_ALREADY_VALID oneSave=false mapSaved=false r28Owners=0 r29Owners=0 vegetationOwner=") +
            VegetationOwner + TEXT(" vegetationMutated=false. ") +
            ExistingReport;
        return true;
    }
    if (Roster.R28ClassCount != 0 || Roster.R28TagCount != 0 ||
        Roster.R29ClassCount != 1 || Roster.R29TagCount != 1 ||
        !Roster.R29 || Roster.R30ClassCount != 0 || Roster.R30TagCount != 0)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_REFUSED_PREDECESSOR_ROSTER r28Class=%d r28Tag=%d r29Class=%d r29Tag=%d r30Class=%d r30Tag=%d; exact 0/0/1/1/0/0 is required."),
            Roster.R28ClassCount,
            Roster.R28TagCount,
            Roster.R29ClassCount,
            Roster.R29TagCount,
            Roster.R30ClassCount,
            Roster.R30TagCount);
        return false;
    }
    FString R29Report;
    if (!Roster.R29->ValidateR29FacadeEnvironment(R29Report))
    {
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_REFUSED_INVALID_R29_PREDECESSOR: ") +
            R29Report;
        return false;
    }
    FString PredecessorShellReport;
    if (!ValidateContextPolicyShell(
            World, Roster.R29->bProviderReady, PredecessorShellReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_REFUSED_CONTEXT_SHELL: ") +
            PredecessorShellReport;
        return false;
    }
    FString AssetMessage;
    if (!EnsureR30FacadeLookdevAssets(AssetMessage))
    {
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_FAILED_ASSETS: ") +
            AssetMessage;
        return false;
    }
    FTRIADIstanaExploreV5DR30FacadeLookdevAssets RuntimeAssets;
    if (!TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory::
            LoadValidatedRuntimeContract(RuntimeAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_FAILED_RUNTIME_CONTRACT: ") +
            Error;
        return false;
    }
    if (RuntimeAssets.RetainedR28ConnectivePublicRealmMesh !=
            Roster.R29->SavedAssets.RetainedR28ConnectivePublicRealmMesh ||
        RuntimeAssets.RetainedR29ContextFacadeCoverageMesh !=
            Roster.R29->SavedAssets.ContextFacadeCoverageMesh ||
        RuntimeAssets.RetainedOuterGroundMesh !=
            Roster.R29->SavedAssets.RetainedOuterGroundMesh ||
        RuntimeAssets.RetainedR28OuterGroundMaterial !=
            Roster.R29->SavedAssets.RetainedR28OuterGroundMaterial)
    {
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_REFUSED_ASSET_HANDOFF: retained R29 facade/R28 public realm/outer ground identity is incoherent.");
        return false;
    }

    const bool bPredecessorProviderReady = Roster.R29->bProviderReady;
    const bool bPackageWasDirty =
        World->GetOutermost() && World->GetOutermost()->IsDirty();
    TUniquePtr<FScopedTransaction> Transaction =
        MakeUnique<FScopedTransaction>(NSLOCTEXT(
            "TRIAD",
            "IstanaExploreV5DR30FacadeLookdevReplacement",
            "Replace R29 facade materials with R30 context-safe lookdev"));
    if (!Transaction || !Transaction->IsOutstanding())
    {
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_REFUSED_NO_UNDO_TRANSACTION: no map mutation was attempted.");
        return false;
    }
    World->Modify();
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = TEXT("TRIAD_IPV5D_R30_FacadeEnvironment");
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParameters.ObjectFlags |= RF_Transactional;
    ATRIADIstanaExploreV5DR30FacadeLookdevActor* Successor =
        World->SpawnActor<ATRIADIstanaExploreV5DR30FacadeLookdevActor>(
            ATRIADIstanaExploreV5DR30FacadeLookdevActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    if (!Successor)
    {
        FString RollbackReport;
        const bool bRolledBack = UndoR30HandoffAndValidateR29(
            World, Transaction, bPredecessorProviderReady,
            bPackageWasDirty, RollbackReport);
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_FAILED: hidden successor spawn returned null.");
        OutMessage += TEXT(" inMemoryRollbackSucceeded=") +
            FString(bRolledBack ? TEXT("true ") : TEXT("false ")) +
            RollbackReport;
        return false;
    }
#if WITH_EDITOR
    Successor->SetActorLabel(TEXT("TRIAD V5D R30 Facade Lookdev Environment (Render Only)"));
#endif
    FString PreparedReport;
    if (!Successor->ConfigurePreparedR30FacadeLookdevHandoff(
            RuntimeAssets, bPredecessorProviderReady, Error) ||
        !Successor->ValidatePreparedR30FacadeLookdevHandoff(PreparedReport))
    {
        FString RollbackReport;
        const bool bRolledBack = UndoR30HandoffAndValidateR29(
            World, Transaction, bPredecessorProviderReady,
            bPackageWasDirty, RollbackReport);
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_FAILED_PREPARE: ") +
            Error + TEXT(" ") + PreparedReport +
            TEXT(" inMemoryRollbackSucceeded=") +
            (bRolledBack ? TEXT("true ") : TEXT("false ")) +
            RollbackReport;
        return false;
    }

    Roster.R29->Modify();
    if (!World->DestroyActor(Roster.R29))
    {
        FString RollbackReport;
        const bool bRolledBack = UndoR30HandoffAndValidateR29(
            World, Transaction, bPredecessorProviderReady,
            bPackageWasDirty, RollbackReport);
        OutMessage = FString(TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_FAILED_REMOVE_R29: inMemoryRollbackSucceeded=")) +
            (bRolledBack ? TEXT("true ") : TEXT("false ")) +
            RollbackReport;
        return false;
    }
    if (!Successor->ActivateAfterR29FacadeRemoval(Error))
    {
        FString RollbackReport;
        const bool bRolledBack = UndoR30HandoffAndValidateR29(
            World, Transaction, bPredecessorProviderReady,
            bPackageWasDirty, RollbackReport);
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_FAILED_ACTIVATE: ") +
            Error + TEXT(" inMemoryRollbackSucceeded=") +
            (bRolledBack ? TEXT("true ") : TEXT("false ")) +
            RollbackReport;
        return false;
    }
    FString FinalReport;
    if (!ValidateR30FacadeLookdevReplacementInLoadedV5DHybridMap(FinalReport))
    {
        FString RollbackReport;
        const bool bRolledBack = UndoR30HandoffAndValidateR29(
            World, Transaction, bPredecessorProviderReady,
            bPackageWasDirty, RollbackReport);
        OutMessage = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_APPLY_FAILED_FINAL_VALIDATION: ") +
            FinalReport + TEXT(" inMemoryRollbackSucceeded=") +
            (bRolledBack ? TEXT("true ") : TEXT("false ")) +
            RollbackReport;
        return false;
    }
    Transaction.Reset();
    OutMessage = TEXT("ISTANA_EXPLORE_V5D_R30_FACADE_LOOKDEV_REPLACEMENT_APPLY_PASS mapSaved=false callerOwnsSingleCommit=true preparedHiddenBeforeR29Removal=true r28EnvironmentActors=0 r29FacadeEnvironmentActors=0 r30FacadeLookdevActors=1 retainedR29FacadeMesh=true exactMaterialOverrides=11 retainedR28PublicRealm=true retainedR28OuterGround=true vegetationOwner=") +
        VegetationOwner +
        TEXT(" vegetationActorsOrAssetsModified=false providerSettingsModified=false geospatialInputsModified=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false visualCaptureAccepted=false captureRevalidationRequired=true. ") +
        PreparedReport + TEXT(" ") + FinalReport + TEXT(" ") +
        VegetationReport + TEXT(" ") + PredecessorShellReport + TEXT(" ") +
        AssetMessage;
    return true;
}

bool UTRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary::
    CommitR30FacadeLookdevReplacementToLoadedV5DHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& ExpectedVegetationOwner,
        const FString& VerifiedExternalBackupFilename,
        FString& OutReport)
{
    FString Error;
    UWorld* World = GetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutReport = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_COMMIT_REFUSED_WORLD: ") +
            Error;
        return false;
    }
    FString DestinationFilename;
    const FString ExpectedSha256 =
        ExpectedPredecessorSha256.ToUpper();
    const FString NormalizedExpectedVegetationOwner =
        ExpectedVegetationOwner.ToUpper();
    const FString BackupFilename = FPaths::ConvertRelativePathToFull(
        VerifiedExternalBackupFilename);
    const FString TransactionRoot = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("TRIAD/NativeTransactions/V5DContextFacadeR30V1")));
    if (ExpectedPredecessorBytes <= 0 || !IsValidSha256(ExpectedSha256) ||
        NormalizedExpectedVegetationOwner != TEXT("R29") ||
        !FPackageName::DoesPackageExist(
            TargetMapPackage, &DestinationFilename) ||
        DestinationFilename.IsEmpty() || BackupFilename.IsEmpty() ||
        FPaths::IsSamePath(DestinationFilename, BackupFilename) ||
        !FPaths::IsUnderDirectory(BackupFilename, TransactionRoot) ||
        !World->GetOutermost() || World->GetOutermost()->IsDirty())
    {
        OutReport = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_COMMIT_REFUSED_INPUT: exact clean map, positive byte count, SHA-256, expected vegetation owner R29, and external backup below the bounded transaction root are required.");
        return false;
    }

    FString MapSha256;
    FString BackupSha256;
    int64 MapBytes = INDEX_NONE;
    int64 BackupBytes = INDEX_NONE;
    FString MapError;
    FString BackupError;
    if (!HashFileSha256(
            DestinationFilename,
            MapSha256,
            MapBytes,
            MapError) ||
        !HashFileSha256(
            BackupFilename,
            BackupSha256,
            BackupBytes,
            BackupError) ||
        MapBytes != ExpectedPredecessorBytes ||
        MapSha256 != ExpectedSha256 ||
        BackupBytes != ExpectedPredecessorBytes ||
        BackupSha256 != ExpectedSha256)
    {
        OutReport = FString::Printf(
            TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_COMMIT_REFUSED_PREIMAGE mapBytes=%lld mapSha256=%s backupBytes=%lld backupSha256=%s mapError={%s} backupError={%s}"),
            MapBytes,
            *MapSha256,
            BackupBytes,
            *BackupSha256,
            *MapError,
            *BackupError);
        return false;
    }

    FOwnerRoster PredecessorRoster;
    FString PredecessorRosterError;
    FString PredecessorVegetationOwner;
    FString PredecessorVegetationReport;
    FString PredecessorActorReport;
    FString PredecessorShellReport;
    if (!ResolveOwnerRoster(
            World, PredecessorRoster, PredecessorRosterError) ||
        PredecessorRoster.R28ClassCount != 0 ||
        PredecessorRoster.R28TagCount != 0 || PredecessorRoster.R28 ||
        PredecessorRoster.R29ClassCount != 1 ||
        PredecessorRoster.R29TagCount != 1 || !PredecessorRoster.R29 ||
        PredecessorRoster.R30ClassCount != 0 ||
        PredecessorRoster.R30TagCount != 0 || PredecessorRoster.R30 ||
        !PredecessorRoster.R29->ValidateR29FacadeEnvironment(
            PredecessorActorReport) ||
        !ValidateContextPolicyShell(
            World,
            PredecessorRoster.R29->bProviderReady,
            PredecessorShellReport) ||
        !ValidateComposableVegetationOwner(
            PredecessorRoster,
            PredecessorVegetationOwner,
            PredecessorVegetationReport) ||
        PredecessorVegetationOwner != NormalizedExpectedVegetationOwner)
    {
        OutReport = FString::Printf(
            TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_COMMIT_REFUSED_PREDECESSOR_WORLD r28Class=%d r28Tag=%d r29Class=%d r29Tag=%d r30Class=%d r30Tag=%d expectedVegetationOwner=%s actualVegetationOwner=%s rosterError={%s} actor={%s} shell={%s} vegetation={%s}"),
            PredecessorRoster.R28ClassCount,
            PredecessorRoster.R28TagCount,
            PredecessorRoster.R29ClassCount,
            PredecessorRoster.R29TagCount,
            PredecessorRoster.R30ClassCount,
            PredecessorRoster.R30TagCount,
            *NormalizedExpectedVegetationOwner,
            *PredecessorVegetationOwner,
            *PredecessorRosterError,
            *PredecessorActorReport,
            *PredecessorShellReport,
            *PredecessorVegetationReport);
        return false;
    }

    FString ApplyReport;
    if (!ApplyR30FacadeLookdevReplacementToLoadedV5DHybridMap(ApplyReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_COMMIT_FAILED_NO_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            ApplyReport;
        return false;
    }

    FString PreSaveMapSha256;
    FString PreSaveBackupSha256;
    int64 PreSaveMapBytes = INDEX_NONE;
    int64 PreSaveBackupBytes = INDEX_NONE;
    FString PreSaveMapError;
    FString PreSaveBackupError;
    if (!HashFileSha256(
            DestinationFilename,
            PreSaveMapSha256,
            PreSaveMapBytes,
            PreSaveMapError) ||
        !HashFileSha256(
            BackupFilename,
            PreSaveBackupSha256,
            PreSaveBackupBytes,
            PreSaveBackupError) ||
        PreSaveMapBytes != ExpectedPredecessorBytes ||
        PreSaveMapSha256 != ExpectedSha256 ||
        PreSaveBackupBytes != ExpectedPredecessorBytes ||
        PreSaveBackupSha256 != ExpectedSha256)
    {
        OutReport = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_COMMIT_FAILED_PRE_SAVE_PIN_GATE externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(World, TargetMapPackage))
    {
        OutReport = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_COMMIT_FAILED_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }
    UWorld* UnloadWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
    UPackage* UnloadPackage = UnloadWorld ? UnloadWorld->GetOutermost() : nullptr;
    if (!UnloadWorld || !UnloadPackage ||
        UWorld::RemovePIEPrefix(UnloadPackage->GetName()) == TargetMapPackage)
    {
        OutReport = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_COMMIT_FAILED_COLD_UNLOAD externalBackupVerified=true rollbackOwnedByWrapper=true");
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
        !ValidateR30FacadeLookdevReplacementInLoadedV5DHybridMap(ColdReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_COMMIT_FAILED_COLD_VALIDATION externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            ColdReport;
        return false;
    }

    FString SuccessorSha256;
    FString FinalBackupSha256;
    int64 SuccessorBytes = INDEX_NONE;
    int64 FinalBackupBytes = INDEX_NONE;
    FString SuccessorError;
    FString FinalBackupError;
    const bool bSuccessorChanged = HashFileSha256(
        DestinationFilename,
        SuccessorSha256,
        SuccessorBytes,
        SuccessorError) &&
        SuccessorBytes > 0 && IsValidSha256(SuccessorSha256) &&
        (SuccessorBytes != ExpectedPredecessorBytes ||
         SuccessorSha256 != ExpectedSha256);
    const bool bBackupPreserved = HashFileSha256(
        BackupFilename,
        FinalBackupSha256,
        FinalBackupBytes,
        FinalBackupError) &&
        FinalBackupBytes == ExpectedPredecessorBytes &&
        FinalBackupSha256 == ExpectedSha256;
    if (!bSuccessorChanged || !bBackupPreserved)
    {
        OutReport = TEXT("EXPLORE_V5D_R30_FACADE_LOOKDEV_COMMIT_FAILED_FINAL_RECEIPT externalBackupVerified=true rollbackOwnedByWrapper=true successorError={") +
            SuccessorError + TEXT("} backupError={") + FinalBackupError +
            TEXT("}");
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R30_FACADE_LOOKDEV_COMMIT_PASS oneSave=true coldReload=true predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s externalBackupBytes=%lld externalBackupSha256=%s r28Owners=0 r29Owners=0 r30Owners=1 retainedR29FacadeMesh=true exactMaterialOverrides=11 retainedR28PublicRealm=true contextPolicyShellValidatedBeforeAndAfter=true predecessorVegetationOwner=%s vegetationActorsOrAssetsModified=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false cesiumModified=false geographyModified=false visualCaptureAccepted=false captureRevalidationRequired=true. %s %s"),
        ExpectedPredecessorBytes,
        *ExpectedSha256,
        SuccessorBytes,
        *SuccessorSha256,
        FinalBackupBytes,
        *FinalBackupSha256,
        *PredecessorVegetationOwner,
        *ApplyReport,
        *ColdReport);
    return true;
}

bool UTRIADIstanaExploreV5DR30FacadeLookdevEditorLibrary::
    ValidateR30FacadeLookdevReplacementInLoadedV5DHybridMap(FString& OutReport)
{
    FString Error;
    UWorld* World = GetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutReport = Error;
        return false;
    }
    FOwnerRoster Roster;
    if (!ResolveOwnerRoster(World, Roster, Error) ||
        Roster.R28ClassCount != 0 || Roster.R28TagCount != 0 ||
        Roster.R29ClassCount != 0 || Roster.R29TagCount != 0 ||
        Roster.R30ClassCount != 1 || Roster.R30TagCount != 1 || !Roster.R30)
    {
        OutReport = FString::Printf(
            TEXT("R30 facade lookdev loaded-map owner roster is not exact: r28Class=%d r28Tag=%d r29Class=%d r29Tag=%d r30Class=%d r30Tag=%d %s"),
            Roster.R28ClassCount,
            Roster.R28TagCount,
            Roster.R29ClassCount,
            Roster.R29TagCount,
            Roster.R30ClassCount,
            Roster.R30TagCount,
            *Error);
        return false;
    }
    FString VegetationOwner;
    FString VegetationReport;
    if (!ValidateComposableVegetationOwner(
            Roster, VegetationOwner, VegetationReport) ||
        VegetationOwner != TEXT("R29"))
    {
        OutReport = TEXT("R30 facade lookdev loaded-map vegetation owner is not exact R29: ") +
            VegetationReport;
        return false;
    }
    FString AssetReport;
    FString ActorReport;
    FString ShellReport;
    if (!TRIADIstanaExploreV5DR30FacadeLookdevAssetFactory::
            ValidateAssets(AssetReport) ||
        !Roster.R30->ValidateR30FacadeLookdev(ActorReport) ||
        !ValidateContextPolicyShell(
            World, Roster.R30->bProviderReady, ShellReport))
    {
        OutReport = AssetReport + TEXT(" ") + ActorReport + TEXT(" ") +
            ShellReport;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_R30_FACADE_LOOKDEV_REPLACEMENT_MAP_VALID r28EnvironmentActors=0 r28EnvironmentTags=0 r29FacadeEnvironmentActors=0 r29FacadeEnvironmentTags=0 r30FacadeLookdevActors=1 r30FacadeLookdevTags=1 retainedR29FacadeMesh=true exactMaterialOverrides=11 retainedR28PublicRealmComponentsOwned=1 retainedR28OuterGroundComponentsOwned=1 mutuallyExclusiveEnvironmentOwners=true providerVisibilityMirrored=true currentSurroundingsContextFacadeR25=true sourceShellAndFacadeLayeringValidated=true mapSavedByThisValidator=false vegetationOwner=") +
        VegetationOwner +
        TEXT(" vegetationOwnerMode=EXACT_R29 vegetationActorsOrAssetsModified=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false cesiumModified=false geographyModified=false visualCaptureAccepted=false captureRevalidationRequired=true. ") +
        ActorReport + TEXT(" ") + ShellReport + TEXT(" ") +
        VegetationReport + TEXT(" ") + AssetReport;
    return true;
}
