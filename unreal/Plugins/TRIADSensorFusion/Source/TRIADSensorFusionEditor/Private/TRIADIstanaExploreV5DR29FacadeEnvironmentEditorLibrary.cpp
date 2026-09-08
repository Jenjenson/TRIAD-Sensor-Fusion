#include "TRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary.h"

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
#include "TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory.h"
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
    int32 R28ClassCount = 0;
    int32 R28TagCount = 0;
    int32 R29ClassCount = 0;
    int32 R29TagCount = 0;
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
    OutError = TEXT("R29 facade commit requires WITH_SSL SHA-256 support.");
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
            TEXT("R29 facade map operation requires exact loaded non-PIE editor map '%s'; actual='%s'."),
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
        const bool bTaggedR29 = Candidate && Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::ExpectedActorTag());
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
        if ((bTaggedR28 && !bExactR28) || (bTaggedR29 && !bExactR29))
        {
            OutError = TEXT("An R28/R29 facade owner tag is held by a non-exact actor class.");
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
            OutError = TEXT("An R29 facade save target is null, not newly created, or already persisted.");
            return false;
        }
        Actual.Add(Asset->GetPathName());
        Packages.Add(Package);
    }
    Actual.Sort();
    if (Actual !=
            TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory::
                GetExpectedAssetObjectPaths() ||
        Assets.Num() != 12 || Packages.Num() != 12)
    {
        OutError = TEXT("The R29 facade fresh save roster is not the exact twelve-package isolated namespace.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool RestoreR28Predecessor(
    UWorld* World,
    const FTRIADIstanaExploreV5DR28EnvironmentAssets& Assets,
    bool bProviderReady,
    FString& OutReport)
{
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = TEXT("TRIAD_IPV5D_R28_Environment_Restored");
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParameters.ObjectFlags |= RF_Transactional;
    ATRIADIstanaExploreV5DR28EnvironmentActor* Restored = World
        ? World->SpawnActor<ATRIADIstanaExploreV5DR28EnvironmentActor>(
              ATRIADIstanaExploreV5DR28EnvironmentActor::StaticClass(),
              FTransform::Identity,
              SpawnParameters)
        : nullptr;
    FString Error;
    if (!Restored ||
        !Restored->ConfigureR28Environment(
            Assets, bProviderReady, Error))
    {
        if (Restored && World)
        {
            World->DestroyActor(Restored);
        }
        OutReport = TEXT("R29 in-memory predecessor restoration failed; external transaction rollback is required: ") +
            Error;
        return false;
    }
#if WITH_EDITOR
    Restored->SetActorLabel(TEXT("TRIAD V5D R28 Environment (Render Only)"));
#endif
    OutReport = TEXT("R28 predecessor restored in-memory after refused R29 handoff.");
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary::
    EnsureR29FacadeEnvironmentAssets(FString& OutMessage)
{
    FString ExistingReport;
    if (TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory::ValidateAssets(
            ExistingReport))
    {
        OutMessage = TEXT("IDEMPOTENT_EXPLORE_V5D_R29_FACADE_ASSETS_ALREADY_VALID: ") +
            ExistingReport;
        return true;
    }
    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_ASSET_BUILD_REFUSED: editor asset subsystem unavailable.");
        return false;
    }
    TArray<UObject*> FreshAssets;
    FString Error;
    if (!TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory::
            CreateFreshAssets(FreshAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_ASSET_BUILD_FAILED: ") +
            Error;
        return false;
    }
    if (!HasExactFreshSaveRoster(FreshAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_ASSET_BUILD_FAILED_ROSTER: ") +
            Error;
        return false;
    }
    if (!AssetSubsystem->SaveLoadedAssets(FreshAssets, false))
    {
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_ASSET_BUILD_FAILED_ATOMIC_SAVE: exact twelve-asset save request failed; guarded native transaction must restore the initially absent R29 facade namespace.");
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
    if (!TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory::ValidateAssets(
            SavedReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_ASSET_BUILD_FAILED_POST_SAVE_VALIDATION: ") +
            SavedReport;
        return false;
    }
    OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_ASSET_BUILD_PASS exactAssets=12 exactNewPackages=12 mapMutation=false retainedR28PackagesModified=false vegetationPackagesModified=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false visualCaptureAccepted=false captureRevalidationRequired=true. ") +
        SavedReport;
    return true;
}

bool UTRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary::
    ValidateR29FacadeEnvironmentAssets(FString& OutReport)
{
    return TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory::
        ValidateAssets(OutReport);
}

bool UTRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary::
    ApplyR29FacadeReplacementToLoadedV5DHybridMap(FString& OutMessage)
{
    FString Error;
    UWorld* World = GetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_APPLY_REFUSED: ") + Error;
        return false;
    }
    FOwnerRoster Roster;
    if (!ResolveOwnerRoster(World, Roster, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_APPLY_REFUSED_ROSTER: ") +
            Error;
        return false;
    }
    FString VegetationOwner;
    FString VegetationReport;
    if (!ValidateComposableVegetationOwner(
            Roster, VegetationOwner, VegetationReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_APPLY_REFUSED_VEGETATION_OWNER: ") +
            VegetationReport;
        return false;
    }
    if (Roster.R28ClassCount == 0 && Roster.R28TagCount == 0 &&
        Roster.R29ClassCount == 1 && Roster.R29TagCount == 1 && Roster.R29)
    {
        FString ExistingReport;
        if (!ValidateR29FacadeReplacementInLoadedV5DHybridMap(ExistingReport))
        {
            OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_IDEMPOTENT_VALIDATION_FAILED: ") +
                ExistingReport;
            return false;
        }
        OutMessage = TEXT("IDEMPOTENT_EXPLORE_V5D_R29_FACADE_REPLACEMENT_ALREADY_VALID oneSave=false mapSaved=false r28ArchitectureActiveRenderers=0 vegetationOwner=") +
            VegetationOwner + TEXT(" vegetationMutated=false. ") +
            ExistingReport;
        return true;
    }
    if (Roster.R28ClassCount != 1 || Roster.R28TagCount != 1 ||
        !Roster.R28 || Roster.R29ClassCount != 0 || Roster.R29TagCount != 0)
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R29_FACADE_APPLY_REFUSED_PREDECESSOR_ROSTER r28Class=%d r28Tag=%d r29Class=%d r29Tag=%d; exact 1/1/0/0 is required."),
            Roster.R28ClassCount,
            Roster.R28TagCount,
            Roster.R29ClassCount,
            Roster.R29TagCount);
        return false;
    }
    FString R28Report;
    if (!Roster.R28->ValidateR28Environment(R28Report))
    {
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_APPLY_REFUSED_INVALID_R28_PREDECESSOR: ") +
            R28Report;
        return false;
    }
    FString AssetMessage;
    if (!EnsureR29FacadeEnvironmentAssets(AssetMessage))
    {
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_APPLY_FAILED_ASSETS: ") +
            AssetMessage;
        return false;
    }
    FTRIADIstanaExploreV5DR29FacadeEnvironmentAssets RuntimeAssets;
    if (!TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory::
            LoadValidatedRuntimeContract(RuntimeAssets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_APPLY_FAILED_RUNTIME_CONTRACT: ") +
            Error;
        return false;
    }
    if (RuntimeAssets.RetainedR28ConnectivePublicRealmMesh !=
            Roster.R28->SavedAssets.ConnectivePublicRealmMesh ||
        RuntimeAssets.RetainedOuterGroundMesh !=
            Roster.R28->SavedAssets.OuterGroundMesh ||
        RuntimeAssets.RetainedR28OuterGroundMaterial !=
            Roster.R28->SavedAssets.OuterGroundMaterial ||
        RuntimeAssets.ContextFacadeCoverageMesh ==
            Roster.R28->SavedAssets.ContextArchitecturalDressingMesh)
    {
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_APPLY_REFUSED_ASSET_HANDOFF: retained R28 public realm/outer ground or replacement mesh identity is incoherent.");
        return false;
    }

    const FTRIADIstanaExploreV5DR28EnvironmentAssets R28Assets =
        Roster.R28->SavedAssets;
    const bool bPredecessorProviderReady = Roster.R28->bProviderReady;
    FScopedTransaction Transaction(
        NSLOCTEXT(
            "TRIAD",
            "IstanaExploreV5DR29FacadeReplacement",
            "Replace R28 context facade with R29 render-only facade"));
    World->Modify();
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = TEXT("TRIAD_IPV5D_R29_FacadeEnvironment");
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParameters.ObjectFlags |= RF_Transactional;
    ATRIADIstanaExploreV5DR29FacadeEnvironmentActor* Successor =
        World->SpawnActor<ATRIADIstanaExploreV5DR29FacadeEnvironmentActor>(
            ATRIADIstanaExploreV5DR29FacadeEnvironmentActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    if (!Successor)
    {
        Transaction.Cancel();
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_APPLY_FAILED: hidden successor spawn returned null.");
        return false;
    }
#if WITH_EDITOR
    Successor->SetActorLabel(TEXT("TRIAD V5D R29 Facade Environment (Render Only)"));
#endif
    FString PreparedReport;
    if (!Successor->ConfigurePreparedR29FacadeHandoff(
            RuntimeAssets, bPredecessorProviderReady, Error) ||
        !Successor->ValidatePreparedR29FacadeHandoff(PreparedReport))
    {
        World->DestroyActor(Successor);
        Transaction.Cancel();
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_APPLY_FAILED_PREPARE: ") +
            Error + TEXT(" ") + PreparedReport;
        return false;
    }

    Roster.R28->Modify();
    if (!World->DestroyActor(Roster.R28))
    {
        World->DestroyActor(Successor);
        Transaction.Cancel();
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_APPLY_FAILED_REMOVE_R28: predecessor remained intact and hidden R29 successor was discarded.");
        return false;
    }
    if (!Successor->ActivateAfterR28EnvironmentRemoval(Error))
    {
        World->DestroyActor(Successor);
        FString RestoreReport;
        const bool bRestored = RestoreR28Predecessor(
            World, R28Assets, bPredecessorProviderReady, RestoreReport);
        Transaction.Cancel();
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_APPLY_FAILED_ACTIVATE: ") +
            Error + TEXT(" restorationSucceeded=") +
            (bRestored ? TEXT("true ") : TEXT("false ")) + RestoreReport;
        return false;
    }
    FString FinalReport;
    if (!ValidateR29FacadeReplacementInLoadedV5DHybridMap(FinalReport))
    {
        World->DestroyActor(Successor);
        FString RestoreReport;
        const bool bRestored = RestoreR28Predecessor(
            World, R28Assets, bPredecessorProviderReady, RestoreReport);
        Transaction.Cancel();
        OutMessage = TEXT("EXPLORE_V5D_R29_FACADE_APPLY_FAILED_FINAL_VALIDATION: ") +
            FinalReport + TEXT(" restorationSucceeded=") +
            (bRestored ? TEXT("true ") : TEXT("false ")) + RestoreReport;
        return false;
    }
    OutMessage = TEXT("ISTANA_EXPLORE_V5D_R29_FACADE_REPLACEMENT_APPLY_PASS mapSaved=false callerOwnsSingleCommit=true preparedHiddenBeforeR28Removal=true r28EnvironmentActors=0 r29FacadeEnvironmentActors=1 r28ArchitectureComponentsOwned=0 r28ArchitectureActiveRenderers=0 retainedR28PublicRealm=true retainedR28OuterGround=true vegetationOwner=") +
        VegetationOwner +
        TEXT(" vegetationActorsOrAssetsModified=false providerSettingsModified=false geospatialInputsModified=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false visualCaptureAccepted=false captureRevalidationRequired=true. ") +
        PreparedReport + TEXT(" ") + FinalReport + TEXT(" ") +
        VegetationReport + TEXT(" ") + AssetMessage;
    return true;
}

bool UTRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary::
    CommitR29FacadeReplacementToLoadedV5DHybridMap(
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
        OutReport = TEXT("EXPLORE_V5D_R29_FACADE_COMMIT_REFUSED_WORLD: ") +
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
            TEXT("TRIAD/NativeTransactions/V5DContextFacadeR29V1")));
    if (ExpectedPredecessorBytes <= 0 || !IsValidSha256(ExpectedSha256) ||
        (NormalizedExpectedVegetationOwner != TEXT("R28") &&
         NormalizedExpectedVegetationOwner != TEXT("R29")) ||
        !FPackageName::DoesPackageExist(
            TargetMapPackage, &DestinationFilename) ||
        DestinationFilename.IsEmpty() || BackupFilename.IsEmpty() ||
        FPaths::IsSamePath(DestinationFilename, BackupFilename) ||
        !FPaths::IsUnderDirectory(BackupFilename, TransactionRoot) ||
        !World->GetOutermost() || World->GetOutermost()->IsDirty())
    {
        OutReport = TEXT("EXPLORE_V5D_R29_FACADE_COMMIT_REFUSED_INPUT: exact clean map, positive byte count, SHA-256, expected vegetation owner R28 or R29, and external backup below the bounded transaction root are required.");
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
            TEXT("EXPLORE_V5D_R29_FACADE_COMMIT_REFUSED_PREIMAGE mapBytes=%lld mapSha256=%s backupBytes=%lld backupSha256=%s mapError={%s} backupError={%s}"),
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
    if (!ResolveOwnerRoster(
            World, PredecessorRoster, PredecessorRosterError) ||
        PredecessorRoster.R28ClassCount != 1 ||
        PredecessorRoster.R28TagCount != 1 || !PredecessorRoster.R28 ||
        PredecessorRoster.R29ClassCount != 0 ||
        PredecessorRoster.R29TagCount != 0 || PredecessorRoster.R29 ||
        !PredecessorRoster.R28->ValidateR28Environment(
            PredecessorActorReport) ||
        !ValidateComposableVegetationOwner(
            PredecessorRoster,
            PredecessorVegetationOwner,
            PredecessorVegetationReport) ||
        PredecessorVegetationOwner != NormalizedExpectedVegetationOwner)
    {
        OutReport = FString::Printf(
            TEXT("EXPLORE_V5D_R29_FACADE_COMMIT_REFUSED_PREDECESSOR_WORLD r28Class=%d r28Tag=%d r29Class=%d r29Tag=%d expectedVegetationOwner=%s actualVegetationOwner=%s rosterError={%s} actor={%s} vegetation={%s}"),
            PredecessorRoster.R28ClassCount,
            PredecessorRoster.R28TagCount,
            PredecessorRoster.R29ClassCount,
            PredecessorRoster.R29TagCount,
            *NormalizedExpectedVegetationOwner,
            *PredecessorVegetationOwner,
            *PredecessorRosterError,
            *PredecessorActorReport,
            *PredecessorVegetationReport);
        return false;
    }

    FString ApplyReport;
    if (!ApplyR29FacadeReplacementToLoadedV5DHybridMap(ApplyReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R29_FACADE_COMMIT_FAILED_NO_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true ") +
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
        OutReport = TEXT("EXPLORE_V5D_R29_FACADE_COMMIT_FAILED_PRE_SAVE_PIN_GATE externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(World, TargetMapPackage))
    {
        OutReport = TEXT("EXPLORE_V5D_R29_FACADE_COMMIT_FAILED_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }
    UWorld* UnloadWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
    UPackage* UnloadPackage = UnloadWorld ? UnloadWorld->GetOutermost() : nullptr;
    if (!UnloadWorld || !UnloadPackage ||
        UWorld::RemovePIEPrefix(UnloadPackage->GetName()) == TargetMapPackage)
    {
        OutReport = TEXT("EXPLORE_V5D_R29_FACADE_COMMIT_FAILED_COLD_UNLOAD externalBackupVerified=true rollbackOwnedByWrapper=true");
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
        !ValidateR29FacadeReplacementInLoadedV5DHybridMap(ColdReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R29_FACADE_COMMIT_FAILED_COLD_VALIDATION externalBackupVerified=true rollbackOwnedByWrapper=true ") +
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
        OutReport = TEXT("EXPLORE_V5D_R29_FACADE_COMMIT_FAILED_FINAL_RECEIPT externalBackupVerified=true rollbackOwnedByWrapper=true successorError={") +
            SuccessorError + TEXT("} backupError={") + FinalBackupError +
            TEXT("}");
        return false;
    }
    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R29_FACADE_COMMIT_PASS oneSave=true coldReload=true predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s externalBackupBytes=%lld externalBackupSha256=%s r28ArchitectureActiveRenderers=0 retainedR28PublicRealm=true predecessorVegetationOwner=%s vegetationActorsOrAssetsModified=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false visualCaptureAccepted=false captureRevalidationRequired=true. %s %s"),
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

bool UTRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary::
    ValidateR29FacadeReplacementInLoadedV5DHybridMap(FString& OutReport)
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
        Roster.R29ClassCount != 1 || Roster.R29TagCount != 1 || !Roster.R29)
    {
        OutReport = FString::Printf(
            TEXT("R29 facade loaded-map owner roster is not exact: r28Class=%d r28Tag=%d r29Class=%d r29Tag=%d %s"),
            Roster.R28ClassCount,
            Roster.R28TagCount,
            Roster.R29ClassCount,
            Roster.R29TagCount,
            *Error);
        return false;
    }
    FString VegetationOwner;
    FString VegetationReport;
    if (!ValidateComposableVegetationOwner(
            Roster, VegetationOwner, VegetationReport))
    {
        OutReport = TEXT("R29 facade loaded-map vegetation owner is not composable: ") +
            VegetationReport;
        return false;
    }
    FString AssetReport;
    FString ActorReport;
    if (!TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory::
            ValidateAssets(AssetReport) ||
        !Roster.R29->ValidateR29FacadeEnvironment(ActorReport))
    {
        OutReport = AssetReport + TEXT(" ") + ActorReport;
        return false;
    }
    OutReport = TEXT("ISTANA_EXPLORE_V5D_R29_FACADE_REPLACEMENT_MAP_VALID r28EnvironmentActors=0 r28EnvironmentTags=0 r29FacadeEnvironmentActors=1 r29FacadeEnvironmentTags=1 r28ArchitectureComponentsOwned=0 r28ArchitectureActiveRenderers=0 r29FacadeComponentsOwned=1 retainedR28PublicRealmComponentsOwned=1 retainedR28OuterGroundComponentsOwned=1 mutuallyExclusiveArchitecture=true providerVisibilityMirrored=true mapSavedByThisValidator=false vegetationOwner=") +
        VegetationOwner +
        TEXT(" vegetationOwnerMode=EXACT_R28_XOR_R29 vegetationActorsOrAssetsModified=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false visualCaptureAccepted=false captureRevalidationRequired=true. ") +
        ActorReport + TEXT(" ") + VegetationReport + TEXT(" ") + AssetReport;
    return true;
}
