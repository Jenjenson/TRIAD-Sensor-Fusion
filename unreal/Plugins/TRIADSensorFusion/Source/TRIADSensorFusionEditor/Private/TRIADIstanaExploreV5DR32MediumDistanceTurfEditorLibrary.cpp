#include "TRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary.h"

#include "AssetCompilingManager.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ScopedTransaction.h"
#include "Ssl.h"
#include "TRIADIstanaExploreV5DGroundVegetationActor.h"
#include "TRIADIstanaExploreV5DR29VegetationActor.h"
#include "TRIADIstanaExploreV5DR29VegetationAssetFactory.h"
#include "TRIADIstanaExploreV5DR31BroadShellEditorLibrary.h"
#include "TRIADIstanaExploreV5DR32MediumDistanceTurfActor.h"
#include "TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory.h"
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
const FString R32NativeTargetMapPackage(
    TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
const FString R32NativeTransactionRelativeRoot(
    TEXT("TRIAD/NativeTransactions/V5DMediumDistanceTurfR32V1"));
const FName R32NativeGroundVegetationTag(
    TEXT("TRIADIstanaExploreV5DGroundVegetation"));
const FName R32NativeActorObjectName(
    TEXT("TRIAD_IPV5D_R32_MediumDistanceTurf"));

bool R32NativeIsValidSha256(const FString& Value)
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

FString R32NativeBytesToHex(const uint8* Bytes, int32 Count)
{
    FString Result;
    Result.Reserve(Count * 2);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Result += FString::Printf(TEXT("%02X"), Bytes[Index]);
    }
    return Result;
}

bool R32NativeHashFileSha256(
    const FString& Filename,
    FString& OutSha256,
    int64& OutBytes,
    FString& OutError)
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
    {
        OutError = TEXT("Could not read guarded R32 file: ") + Filename;
        return false;
    }
    OutBytes = Bytes.Num();
#if WITH_SSL
    uint8 Digest[SHA256_DIGEST_LENGTH] = {};
    if (SHA256(Bytes.GetData(), static_cast<size_t>(Bytes.Num()), Digest) ==
        nullptr)
    {
        OutError = TEXT("SHA-256 failed for guarded R32 file: ") + Filename;
        return false;
    }
    OutSha256 = R32NativeBytesToHex(Digest, SHA256_DIGEST_LENGTH);
#else
    OutError = TEXT("R32 turf map commit requires WITH_SSL SHA-256 support.");
    return false;
#endif
    OutError.Reset();
    return true;
}

UWorld* R32NativeGetExactLoadedTargetWorld(FString& OutError)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    const FString PackageName = World && World->GetOutermost()
        ? World->GetOutermost()->GetName()
        : FString();
    if (!GIsEditor || !GEditor || GEditor->PlayWorld ||
        GEditor->IsPlaySessionInProgress() || !World ||
        World->WorldType != EWorldType::Editor || !World->PersistentLevel ||
        PackageName != R32NativeTargetMapPackage)
    {
        OutError = FString::Printf(
            TEXT("R32 turf operation requires exact loaded non-PIE editor map '%s'; actual='%s'."),
            *R32NativeTargetMapPackage,
            *PackageName);
        return nullptr;
    }
    OutError.Reset();
    return World;
}

struct FR32NativeWorldRoster
{
    ATRIADIstanaExploreV5DGroundVegetationActor* Ground = nullptr;
    ATRIADIstanaExploreV5DR32MediumDistanceTurfActor* R32 = nullptr;
    int32 GroundClassCount = 0;
    int32 GroundTagCount = 0;
    int32 R32ClassCount = 0;
    int32 R32TagCount = 0;
};

bool R32NativeResolveWorldRoster(
    UWorld* World,
    FR32NativeWorldRoster& OutRoster,
    FString& OutError)
{
    OutRoster = FR32NativeWorldRoster{};
    if (!World || !World->PersistentLevel)
    {
        OutError = TEXT("R32 turf roster resolution requires an exact persistent editor world.");
        return false;
    }

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (!IsValid(Candidate))
        {
            continue;
        }

        const bool bGroundFamily = Candidate->IsA<
            ATRIADIstanaExploreV5DGroundVegetationActor>();
        const bool bExactGround = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DGroundVegetationActor::StaticClass();
        const bool bTaggedGround = Candidate->Tags.Contains(
            R32NativeGroundVegetationTag);
        if ((bGroundFamily && !bExactGround) ||
            (bTaggedGround && !bExactGround))
        {
            OutError = TEXT("R32 turf refused a ground-owner subclass or a ground tag on a non-exact class.");
            return false;
        }
        if (bExactGround)
        {
            ++OutRoster.GroundClassCount;
            OutRoster.Ground = Cast<
                ATRIADIstanaExploreV5DGroundVegetationActor>(Candidate);
        }
        OutRoster.GroundTagCount += bTaggedGround ? 1 : 0;

        const bool bR32Family = Candidate->IsA<
            ATRIADIstanaExploreV5DR32MediumDistanceTurfActor>();
        const bool bExactR32 = Candidate->GetClass() ==
            ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::StaticClass();
        const bool bTaggedR32 = Candidate->Tags.Contains(
            ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
                ExpectedActorTag());
        if ((bR32Family && !bExactR32) || (bTaggedR32 && !bExactR32))
        {
            OutError = TEXT("R32 turf refused an R32 subclass or an R32 owner tag on a non-exact class.");
            return false;
        }
        if (bExactR32)
        {
            ++OutRoster.R32ClassCount;
            OutRoster.R32 = Cast<
                ATRIADIstanaExploreV5DR32MediumDistanceTurfActor>(Candidate);
        }
        OutRoster.R32TagCount += bTaggedR32 ? 1 : 0;
    }

    OutError.Reset();
    return true;
}

bool R32NativeLoadExactCleanAssetRoster(
    FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets& OutAssets,
    FString& OutReport)
{
    OutAssets = FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets{};

    FAssetCompilingManager::Get().FinishAllCompilation();
    FString RuntimeContractError;
    if (!TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory::
            LoadValidatedRuntimeContract(OutAssets, RuntimeContractError) ||
        OutAssets.GrassMeshVariants.Num() != 3 ||
        OutAssets.GrassProfileMaterials.Num() != 4)
    {
        OutReport = TEXT("R32 could not load the exact validated R29-mesh/R32-material runtime contract: ") +
            RuntimeContractError;
        return false;
    }

    const TArray<FString>& ExpectedR29Paths =
        TRIADIstanaExploreV5DR29VegetationAssetFactory::
            GetExpectedAssetObjectPaths();
    const TArray<FString>& ExpectedR32Paths =
        TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory::
            GetExpectedMaterialObjectPaths();
    TSet<FString> ExpectedR29MeshPaths;
    for (const FString& Path : ExpectedR29Paths)
    {
        if (Path.Contains(TEXT("/Meshes/"), ESearchCase::CaseSensitive))
        {
            ExpectedR29MeshPaths.Add(Path);
        }
    }
    TSet<FString> UnmatchedR32MaterialPaths;
    for (const FString& Path : ExpectedR32Paths)
    {
        UnmatchedR32MaterialPaths.Add(Path);
    }
    TSet<const UPackage*> ExactPackages;
    for (UStaticMesh* Mesh : OutAssets.GrassMeshVariants)
    {
        UPackage* Package = Mesh ? Mesh->GetOutermost() : nullptr;
        const FString Path = Mesh ? Mesh->GetPathName() : FString();
        if (!IsValid(Mesh) || !ExpectedR29MeshPaths.Remove(Path) ||
            !Package || Package->IsDirty() ||
            !FPackageName::DoesPackageExist(Package->GetName()) ||
            ExactPackages.Contains(Package))
        {
            OutReport = TEXT("R32 refused a missing, aliased, duplicate, unsaved, or dirty R29 grass-mesh package: ") +
                Path;
            return false;
        }
        ExactPackages.Add(Package);
    }
    for (UMaterialInterface* Material : OutAssets.GrassProfileMaterials)
    {
        UPackage* Package = Material ? Material->GetOutermost() : nullptr;
        const FString Path = Material
            ? Material->GetPathName()
            : FString();
        if (!IsValid(Material) || !UnmatchedR32MaterialPaths.Remove(Path) ||
            !Package || Package->IsDirty() ||
            !FPackageName::DoesPackageExist(Package->GetName()) ||
            ExactPackages.Contains(Package))
        {
            OutReport = TEXT("R32 refused a missing, aliased, duplicate, unsaved, or dirty isolated R32 grass-material package: ") +
                Path;
            return false;
        }
        ExactPackages.Add(Package);
    }

    FString RuntimeRosterError;
    if (ExpectedR29Paths.Num() != 7 ||
        !ExpectedR29MeshPaths.IsEmpty() || ExpectedR32Paths.Num() != 4 ||
        !UnmatchedR32MaterialPaths.IsEmpty() || ExactPackages.Num() != 7 ||
        !ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
            ValidateAssetRoster(OutAssets, RuntimeRosterError))
    {
        OutReport = TEXT("R32 exact seven-package runtime asset admission failed: ") +
            RuntimeRosterError;
        return false;
    }
    OutReport = TEXT("R32_EXACT_CLEAN_RUNTIME_ASSET_ROSTER_VALID loadedPackages=7 r29Meshes=3 isolatedR32Materials=4 persisted=true dirty=false exactCleanR29SourcePackages=7 sourcePackagesModified=false.");
    return true;
}

bool R32NativeValidateR31Predecessor(
    UWorld* World,
    FR32NativeWorldRoster& OutRoster,
    FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets& OutAssets,
    FString& OutReport)
{
    FString R31Report;
    FString RosterError;
    FString GroundReport;
    FString GroundTransformReport;
    FString AssetReport;
    if (!World ||
        !UTRIADIstanaExploreV5DR31BroadShellEditorLibrary::
            ValidateR31BroadShellInLoadedV5DHybridMap(R31Report) ||
        !R32NativeResolveWorldRoster(World, OutRoster, RosterError) ||
        OutRoster.GroundClassCount != 1 || OutRoster.GroundTagCount != 1 ||
        !OutRoster.Ground ||
        OutRoster.Ground->GetLevel() != World->PersistentLevel ||
        !OutRoster.Ground->Tags.Contains(R32NativeGroundVegetationTag) ||
        OutRoster.Ground->GrassPresentationRevision !=
            ATRIADIstanaExploreV5DGroundVegetationActor::
                ExpectedGrassPresentationRevision() ||
        !OutRoster.Ground->ValidateGroundVegetationRealism(GroundReport) ||
        !OutRoster.Ground->ValidateOwnedGrassInstanceTransforms(
            GroundTransformReport) ||
        OutRoster.R32ClassCount != 0 || OutRoster.R32TagCount != 0 ||
        OutRoster.R32 ||
        !R32NativeLoadExactCleanAssetRoster(OutAssets, AssetReport))
    {
        OutReport = TEXT("R32 predecessor validation failed; exact R30 commit plus accepted five-pose capture and exact R31 commit plus accepted native capture must precede R32. r31={") +
            R31Report + TEXT("} roster={") + RosterError +
            TEXT("} ground={") + GroundReport + TEXT("} transforms={") +
            GroundTransformReport + TEXT("} assets={") + AssetReport +
            TEXT("}");
        return false;
    }

    OutReport = TEXT("R32_R31_PREDECESSOR_VALID exactR31BroadShell=true exactTaggedR23GroundOwner=1 r32Owners=0 exactCleanR29SourcePackages=7 exactIsolatedR32MaterialPackages=4 r30CommitAndFivePoseAcceptanceReceiptGateRequired=true r31CommitAndNativeCaptureAcceptanceReceiptGateRequired=true receiptGateOwnedByExternalWrapper=true.");
    return true;
}

bool R32NativeValidateSuccessorWorld(
    UWorld* World,
    FR32NativeWorldRoster& OutRoster,
    FString& OutReport)
{
    FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets ExactAssets;
    FString R31Report;
    FString RosterError;
    FString GroundReport;
    FString GroundTransformReport;
    FString AssetReport;
    FString R32Report;
    if (!World ||
        !UTRIADIstanaExploreV5DR31BroadShellEditorLibrary::
            ValidateR31BroadShellInLoadedV5DHybridMap(R31Report) ||
        !R32NativeResolveWorldRoster(World, OutRoster, RosterError) ||
        OutRoster.GroundClassCount != 1 || OutRoster.GroundTagCount != 1 ||
        !OutRoster.Ground ||
        OutRoster.Ground->GetLevel() != World->PersistentLevel ||
        !OutRoster.Ground->Tags.Contains(R32NativeGroundVegetationTag) ||
        OutRoster.Ground->GrassPresentationRevision !=
            ATRIADIstanaExploreV5DGroundVegetationActor::
                ExpectedGrassPresentationRevision() ||
        !OutRoster.Ground->ValidateGroundVegetationRealism(GroundReport) ||
        !OutRoster.Ground->ValidateOwnedGrassInstanceTransforms(
            GroundTransformReport) ||
        OutRoster.R32ClassCount != 1 || OutRoster.R32TagCount != 1 ||
        !OutRoster.R32 ||
        OutRoster.R32->GetLevel() != World->PersistentLevel ||
        OutRoster.R32->GetFName() != R32NativeActorObjectName ||
        OutRoster.R32->SourceGroundVegetation != OutRoster.Ground ||
        !OutRoster.R32->GetActorTransform().Equals(
            FTransform::Identity, 0.0001) ||
        !OutRoster.R32->ValidateR32MediumDistanceTurf(R32Report) ||
        !R32NativeLoadExactCleanAssetRoster(ExactAssets, AssetReport))
    {
        OutReport = TEXT("R32 loaded-world validation failed: r31={") +
            R31Report + TEXT("} roster={") + RosterError +
            TEXT("} ground={") + GroundReport + TEXT("} transforms={") +
            GroundTransformReport + TEXT("} r32={") + R32Report +
            TEXT("} assets={") + AssetReport + TEXT("}");
        return false;
    }

    OutReport = TEXT("ISTANA_EXPLORE_V5D_R32_MEDIUM_DISTANCE_TURF_MAP_VALID mapIntegrated=true addOnly=true exactR31BroadShell=true exactTaggedR23GroundOwner=1 exactR32Owners=1 exactR32Tags=1 persistentLevel=true identityTransform=true exactCleanR29SourcePackages=7 exactIsolatedR32MaterialPackages=4 ownedHisms=12 selectedTransforms=4608 sourceActorModified=false sourceAssetsModified=false otherOwnersModified=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false geospatialAuthority=false visualCaptureAccepted=false captureRevalidationRequired=true. ") +
        R32Report;
    return true;
}

bool R32NativeUndoAndValidateR31(
    UWorld* World,
    TUniquePtr<FScopedTransaction>& Transaction,
    ATRIADIstanaExploreV5DGroundVegetationActor* ExpectedGround,
    bool bPackageWasDirty,
    FString& OutReport)
{
    if (!GEditor || !World || !Transaction || !Transaction->IsOutstanding())
    {
        OutReport = TEXT("R32 in-memory rollback could not close an outstanding editor transaction.");
        return false;
    }

    Transaction.Reset();
    const bool bUndoSucceeded = GEditor->UndoTransaction(false);
    FR32NativeWorldRoster Restored;
    FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets RestoredAssets;
    FString PredecessorReport;
    const bool bPredecessorRestored = bUndoSucceeded &&
        R32NativeValidateR31Predecessor(
            World, Restored, RestoredAssets, PredecessorReport) &&
        Restored.Ground == ExpectedGround;
    UPackage* WorldPackage = World->GetOutermost();
    if (WorldPackage)
    {
        WorldPackage->SetDirtyFlag(bPackageWasDirty);
    }
    const bool bDirtyStateRestored = WorldPackage &&
        WorldPackage->IsDirty() == bPackageWasDirty;
    if (!bPredecessorRestored || !bDirtyStateRestored)
    {
        OutReport = TEXT("R32 undo failed to restore the exact R31 predecessor, tagged R23 ground owner, zero-R32 roster, and seven clean R29 packages; external wrapper rollback is required. predecessor={") +
            PredecessorReport + TEXT("}");
        return false;
    }

    OutReport = TEXT("R32_IN_MEMORY_ROLLBACK_VALID undoTransaction=true exactR31BroadShell=true exactTaggedR23GroundOwner=1 r32Owners=0 exactCleanR29SourcePackages=7 exactIsolatedR32MaterialPackages=4 packageDirtyStateRestored=true.");
    return true;
}
} // namespace

bool UTRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary::
    EnsureR32MediumDistanceTurfMaterials(FString& OutMessage)
{
    TArray<UObject*> Assets;
    FString Error;
    if (!TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory::
            CreateFreshAssets(Assets, Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R32_TURF_MATERIAL_BUILD_FAILED: ") +
            Error;
        return false;
    }
    FString ValidationReport;
    if (Assets.Num() != 4 ||
        !TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory::
            ValidateAssets(ValidationReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R32_TURF_MATERIAL_BUILD_FAILED_POST_VALIDATION: ") +
            ValidationReport;
        return false;
    }
    OutMessage = TEXT("EXPLORE_V5D_R32_TURF_MATERIAL_BUILD_PASS exactSavedPackages=4 sourceR29PackagesModified=false mapsSaved=0 visualCaptureAccepted=false captureRevalidationRequired=true. ") +
        ValidationReport;
    return true;
}

bool UTRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary::
    ValidateR32MediumDistanceTurfMaterials(FString& OutReport)
{
    return TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory::
        ValidateAssets(OutReport);
}

bool UTRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary::
    ApplyR32MediumDistanceTurfToLoadedV5DHybridMap(FString& OutMessage)
{
    FString Error;
    UWorld* World = R32NativeGetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutMessage = TEXT("EXPLORE_V5D_R32_TURF_APPLY_REFUSED_WORLD: ") +
            Error;
        return false;
    }

    FR32NativeWorldRoster ExistingSuccessor;
    FString ExistingSuccessorReport;
    if (R32NativeValidateSuccessorWorld(
            World, ExistingSuccessor, ExistingSuccessorReport))
    {
        OutMessage = TEXT("IDEMPOTENT_EXPLORE_V5D_R32_MEDIUM_DISTANCE_TURF_ALREADY_VALID mapSaved=false transactionOpened=false actorSpawned=false exactR32Owner=1 sourceActorModified=false sourceAssetsModified=false otherOwnersModified=false visualCaptureAccepted=false captureRevalidationRequired=true. ") +
            ExistingSuccessorReport;
        return true;
    }

    FR32NativeWorldRoster Predecessor;
    FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets Assets;
    FString PredecessorReport;
    if (!R32NativeValidateR31Predecessor(
            World, Predecessor, Assets, PredecessorReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R32_TURF_APPLY_REFUSED_PREDECESSOR: ") +
            PredecessorReport;
        return false;
    }

    const bool bPackageWasDirty = World->GetOutermost()->IsDirty();
    TUniquePtr<FScopedTransaction> Transaction =
        MakeUnique<FScopedTransaction>(
            NSLOCTEXT(
                "TRIAD",
                "ApplyIstanaExploreV5DR32MediumDistanceTurf",
                "Apply Istana Explore V5D R32 Medium-Distance Turf"));
    if (!Transaction || !Transaction->IsOutstanding())
    {
        OutMessage = TEXT("EXPLORE_V5D_R32_TURF_APPLY_REFUSED_NO_UNDO_TRANSACTION: no map mutation was attempted.");
        return false;
    }

    World->Modify();
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = R32NativeActorObjectName;
    SpawnParameters.OverrideLevel = World->PersistentLevel;
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParameters.ObjectFlags |= RF_Transactional;
    ATRIADIstanaExploreV5DR32MediumDistanceTurfActor* R32Actor =
        World->SpawnActor<ATRIADIstanaExploreV5DR32MediumDistanceTurfActor>(
            ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);
    if (!R32Actor || R32Actor->GetLevel() != World->PersistentLevel ||
        R32Actor->GetFName() != R32NativeActorObjectName)
    {
        FString RollbackReport;
        const bool bRolledBack = R32NativeUndoAndValidateR31(
            World,
            Transaction,
            Predecessor.Ground,
            bPackageWasDirty,
            RollbackReport);
        OutMessage = FString(TEXT("EXPLORE_V5D_R32_TURF_APPLY_FAILED_SPAWN inMemoryRollback=")) +
            (bRolledBack ? TEXT("true ") : TEXT("false ")) +
            RollbackReport;
        return false;
    }

    R32Actor->Modify();
#if WITH_EDITOR
    R32Actor->SetActorLabel(
        TEXT("TRIAD Istana Explore V5D R32 Medium-Distance Turf - Render Only"));
#endif
    FString ConfigureError;
    if (!R32Actor->ConfigureR32MediumDistanceTurf(
            Predecessor.Ground, Assets, ConfigureError))
    {
        FString RollbackReport;
        const bool bRolledBack = R32NativeUndoAndValidateR31(
            World,
            Transaction,
            Predecessor.Ground,
            bPackageWasDirty,
            RollbackReport);
        OutMessage = TEXT("EXPLORE_V5D_R32_TURF_APPLY_FAILED_CONFIGURE: ") +
            ConfigureError + TEXT(" inMemoryRollback=") +
            (bRolledBack ? TEXT("true ") : TEXT("false ")) +
            RollbackReport;
        return false;
    }

    FR32NativeWorldRoster Successor;
    FString SuccessorReport;
    if (!R32NativeValidateSuccessorWorld(
            World, Successor, SuccessorReport))
    {
        FString RollbackReport;
        const bool bRolledBack = R32NativeUndoAndValidateR31(
            World,
            Transaction,
            Predecessor.Ground,
            bPackageWasDirty,
            RollbackReport);
        OutMessage = FString(TEXT("EXPLORE_V5D_R32_TURF_APPLY_FAILED_VALIDATION inMemoryRollback=")) +
            (bRolledBack ? TEXT("true") : TEXT("false")) +
            TEXT(" successor={") + SuccessorReport +
            TEXT("} rollback={") + RollbackReport + TEXT("}");
        return false;
    }

    Transaction.Reset();
    OutMessage = TEXT("ISTANA_EXPLORE_V5D_R32_MEDIUM_DISTANCE_TURF_APPLY_PASS mapSaved=false callerOwnsSingleCommit=true addOnly=true exactR31BroadShell=true exactTaggedR23GroundOwner=1 r32ActorsAdded=1 r32ActorsRemoved=0 persistentLevel=true identityTransform=true exactCleanR29SourcePackages=7 exactIsolatedR32MaterialPackages=4 ownedHisms=12 selectedTransforms=4608 sourceActorModified=false sourceAssetsModified=false otherOwnersModified=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false geospatialAuthority=false externalR30R31ReceiptChainRequiredBeforeInvocation=true receiptGateOwnedByExternalWrapper=true visualCaptureAccepted=false captureRevalidationRequired=true. ") +
        SuccessorReport;
    return true;
}

bool UTRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary::
    ValidateR32MediumDistanceTurfInLoadedV5DHybridMap(FString& OutReport)
{
    FString Error;
    UWorld* World = R32NativeGetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutReport = Error;
        return false;
    }
    FR32NativeWorldRoster Roster;
    return R32NativeValidateSuccessorWorld(World, Roster, OutReport);
}

bool UTRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary::
    CommitR32MediumDistanceTurfToLoadedV5DHybridMap(
        int64 ExpectedPredecessorBytes,
        const FString& ExpectedPredecessorSha256,
        const FString& VerifiedExternalBackupFilename,
        FString& OutReport)
{
    FString Error;
    UWorld* World = R32NativeGetExactLoadedTargetWorld(Error);
    if (!World)
    {
        OutReport = TEXT("EXPLORE_V5D_R32_TURF_COMMIT_REFUSED_WORLD: ") +
            Error;
        return false;
    }

    FString DestinationFilename;
    const FString ExpectedSha256 = ExpectedPredecessorSha256.ToUpper();
    FString BackupFilename = FPaths::ConvertRelativePathToFull(
        VerifiedExternalBackupFilename);
    FString TransactionRoot = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            R32NativeTransactionRelativeRoot));
    FPaths::NormalizeFilename(BackupFilename);
    FPaths::NormalizeDirectoryName(TransactionRoot);
    if (ExpectedPredecessorBytes <= 0 ||
        !R32NativeIsValidSha256(ExpectedSha256) ||
        !FPackageName::DoesPackageExist(
            R32NativeTargetMapPackage, &DestinationFilename) ||
        DestinationFilename.IsEmpty() || BackupFilename.IsEmpty() ||
        FPaths::IsSamePath(DestinationFilename, BackupFilename) ||
        !FPaths::IsUnderDirectory(BackupFilename, TransactionRoot) ||
        !World->GetOutermost() || World->GetOutermost()->IsDirty())
    {
        OutReport = TEXT("EXPLORE_V5D_R32_TURF_COMMIT_REFUSED_INPUT: exact clean R31 map, positive preimage bytes/SHA-256, and an external backup below the bounded R32 transaction root are required; external R30/R31 commit-and-capture receipt admission must already have passed.");
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
    if (!R32NativeHashFileSha256(
            DestinationFilename, MapSha256, MapBytes, MapError) ||
        !R32NativeHashFileSha256(
            BackupFilename, BackupSha256, BackupBytes, BackupError) ||
        MapBytes != ExpectedPredecessorBytes ||
        MapSha256 != ExpectedSha256 ||
        BackupBytes != ExpectedPredecessorBytes ||
        BackupSha256 != ExpectedSha256)
    {
        OutReport = FString::Printf(
            TEXT("EXPLORE_V5D_R32_TURF_COMMIT_REFUSED_PREIMAGE mapBytes=%lld mapSha256=%s backupBytes=%lld backupSha256=%s mapError={%s} backupError={%s}"),
            MapBytes,
            *MapSha256,
            BackupBytes,
            *BackupSha256,
            *MapError,
            *BackupError);
        return false;
    }

    FR32NativeWorldRoster Predecessor;
    FTRIADIstanaExploreV5DR32MediumDistanceTurfAssets Assets;
    FString PredecessorReport;
    if (!R32NativeValidateR31Predecessor(
            World, Predecessor, Assets, PredecessorReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R32_TURF_COMMIT_REFUSED_PREDECESSOR: ") +
            PredecessorReport;
        return false;
    }

    FString ApplyReport;
    if (!ApplyR32MediumDistanceTurfToLoadedV5DHybridMap(ApplyReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R32_TURF_COMMIT_FAILED_NO_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            ApplyReport;
        return false;
    }

    FString PreSaveMapSha256;
    FString PreSaveBackupSha256;
    int64 PreSaveMapBytes = INDEX_NONE;
    int64 PreSaveBackupBytes = INDEX_NONE;
    FString PreSaveMapError;
    FString PreSaveBackupError;
    if (!R32NativeHashFileSha256(
            DestinationFilename,
            PreSaveMapSha256,
            PreSaveMapBytes,
            PreSaveMapError) ||
        !R32NativeHashFileSha256(
            BackupFilename,
            PreSaveBackupSha256,
            PreSaveBackupBytes,
            PreSaveBackupError) ||
        PreSaveMapBytes != ExpectedPredecessorBytes ||
        PreSaveMapSha256 != ExpectedSha256 ||
        PreSaveBackupBytes != ExpectedPredecessorBytes ||
        PreSaveBackupSha256 != ExpectedSha256)
    {
        OutReport = TEXT("EXPLORE_V5D_R32_TURF_COMMIT_FAILED_PRE_SAVE_PIN_GATE externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }

    World->MarkPackageDirty();
    if (!UEditorLoadingAndSavingUtils::SaveMap(
            World, R32NativeTargetMapPackage))
    {
        OutReport = TEXT("EXPLORE_V5D_R32_TURF_COMMIT_FAILED_SAVE externalBackupVerified=true rollbackOwnedByWrapper=true");
        return false;
    }

    UWorld* UnloadWorld = UEditorLoadingAndSavingUtils::NewBlankMap(false);
    UPackage* UnloadPackage = UnloadWorld
        ? UnloadWorld->GetOutermost()
        : nullptr;
    if (!UnloadWorld || !UnloadPackage ||
        UWorld::RemovePIEPrefix(UnloadPackage->GetName()) ==
            R32NativeTargetMapPackage)
    {
        OutReport = TEXT("EXPLORE_V5D_R32_TURF_COMMIT_FAILED_COLD_UNLOAD externalBackupVerified=true rollbackOwnedByWrapper=true");
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
        !ValidateR32MediumDistanceTurfInLoadedV5DHybridMap(ColdReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R32_TURF_COMMIT_FAILED_COLD_VALIDATION externalBackupVerified=true rollbackOwnedByWrapper=true ") +
            ColdReport;
        return false;
    }

    FString SuccessorSha256;
    FString FinalBackupSha256;
    int64 SuccessorBytes = INDEX_NONE;
    int64 FinalBackupBytes = INDEX_NONE;
    FString SuccessorError;
    FString FinalBackupError;
    const bool bSuccessorChanged = R32NativeHashFileSha256(
        DestinationFilename,
        SuccessorSha256,
        SuccessorBytes,
        SuccessorError) &&
        SuccessorBytes > 0 && R32NativeIsValidSha256(SuccessorSha256) &&
        (SuccessorBytes != ExpectedPredecessorBytes ||
         SuccessorSha256 != ExpectedSha256);
    const bool bBackupPreserved = R32NativeHashFileSha256(
        BackupFilename,
        FinalBackupSha256,
        FinalBackupBytes,
        FinalBackupError) &&
        FinalBackupBytes == ExpectedPredecessorBytes &&
        FinalBackupSha256 == ExpectedSha256;
    if (!bSuccessorChanged || !bBackupPreserved)
    {
        OutReport = TEXT("EXPLORE_V5D_R32_TURF_COMMIT_FAILED_FINAL_RECEIPT externalBackupVerified=true rollbackOwnedByWrapper=true successorError={") +
            SuccessorError + TEXT("} backupError={") +
            FinalBackupError + TEXT("}");
        return false;
    }

    OutReport = FString::Printf(
        TEXT("ISTANA_EXPLORE_V5D_R32_MEDIUM_DISTANCE_TURF_COMMIT_PASS oneMapSave=true materialPackageSaveCount=4 coldReload=true predecessorBytes=%lld predecessorSha256=%s successorBytes=%lld successorSha256=%s externalBackupBytes=%lld externalBackupSha256=%s exactR31BroadShell=true exactTaggedR23GroundOwner=1 exactR32Owner=1 persistentLevel=true identityTransform=true exactCleanR29SourcePackages=7 exactIsolatedR32MaterialPackages=4 ownedHisms=12 selectedTransforms=4608 sourceActorModified=false sourceAssetsModified=false otherOwnersModified=false collision=false navigation=false simulationAuthority=false sensorAuthority=false rfAuthority=false geospatialAuthority=false externalR30R31ReceiptChainRequiredBeforeInvocation=true receiptGateOwnedByExternalWrapper=true visualCaptureAccepted=false captureRevalidationRequired=true. %s %s"),
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
