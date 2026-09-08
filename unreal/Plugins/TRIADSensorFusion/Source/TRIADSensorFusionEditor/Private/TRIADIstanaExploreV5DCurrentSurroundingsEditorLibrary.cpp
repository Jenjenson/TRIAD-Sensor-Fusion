#include "TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h"

#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "PackageTools.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DContextFacadeR25AssetFactory.h"
#include "TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.h"
#include "UObject/Package.h"

namespace
{
class FScopedSuppressedSavedImportRollback final
{
public:
    FScopedSuppressedSavedImportRollback(
        UEditorAssetSubsystem* InSubsystem,
        FString& InMessage)
        : Subsystem(InSubsystem), Message(InMessage)
    {
    }

    ~FScopedSuppressedSavedImportRollback()
    {
        if (!bArmed || bCommitted || !Subsystem)
        {
            return;
        }
        if (!Subsystem->DeleteAsset(
                TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
                    GetLocalFallbackSuppressedMeshObjectPath()))
        {
            Message += TEXT(" LOCAL_FALLBACK_SUPPRESSION_V1_SAVED_ROLLBACK_INCOMPLETE");
        }
    }

    void Arm() { bArmed = true; }
    void Commit() { bCommitted = true; }

private:
    UEditorAssetSubsystem* Subsystem = nullptr;
    FString& Message;
    bool bArmed = false;
    bool bCommitted = false;
};

class FScopedSuppressedV2SavedImportRollback final
{
public:
    FScopedSuppressedV2SavedImportRollback(
        UEditorAssetSubsystem* InSubsystem,
        FString& InMessage)
        : Subsystem(InSubsystem), Message(InMessage)
    {
    }

    ~FScopedSuppressedV2SavedImportRollback()
    {
        if (!bArmed || bCommitted || !Subsystem)
        {
            return;
        }
        if (!Subsystem->DeleteAsset(
                TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
                    GetLocalFallbackSuppressedV2MeshObjectPath()))
        {
            Message += TEXT(" LOCAL_FALLBACK_SUPPRESSION_V2_SAVED_ROLLBACK_INCOMPLETE");
        }
    }

    void Arm() { bArmed = true; }
    void Commit() { bCommitted = true; }

private:
    UEditorAssetSubsystem* Subsystem = nullptr;
    FString& Message;
    bool bArmed = false;
    bool bCommitted = false;
};

class FScopedContextFacadeR25SavedImportRollback final
{
public:
    FScopedContextFacadeR25SavedImportRollback(
        UEditorAssetSubsystem* InSubsystem,
        FString& InMessage)
        : Subsystem(InSubsystem), Message(InMessage)
    {
    }

    ~FScopedContextFacadeR25SavedImportRollback()
    {
        if (!bArmed || bCommitted || !Subsystem)
        {
            return;
        }
        TArray<FString> ExactPaths =
            TRIADIstanaExploreV5DContextFacadeR25AssetFactory::
                GetOrderedMaterialObjectPaths();
        ExactPaths.Add(
            TRIADIstanaExploreV5DContextFacadeR25AssetFactory::
                GetMasterMaterialObjectPath());
        for (const FString& Path : ExactPaths)
        {
            if (Subsystem->DoesAssetExist(Path) &&
                !Subsystem->DeleteAsset(Path))
            {
                Message +=
                    TEXT(" R25_CONTEXT_FACADE_SAVED_ROLLBACK_INCOMPLETE");
                break;
            }
        }
    }

    void Arm() { bArmed = true; }
    void Commit() { bCommitted = true; }

private:
    UEditorAssetSubsystem* Subsystem = nullptr;
    FString& Message;
    bool bArmed = false;
    bool bCommitted = false;
};
}

bool UTRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary::
    ImportIstanaExploreV5DCurrentSurroundingsAsset(FString& OutMessage)
{
    FString ExistingReport;
    if (TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::ValidateAsset(
            ExistingReport))
    {
        OutMessage =
            TEXT("IDEMPOTENT_EXPLORE_V5D_CURRENT_SURROUNDINGS_ASSET_ALREADY_VALID: ") +
            ExistingReport;
        return true;
    }

    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5D_CURRENT_SURROUNDINGS_IMPORT_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    UStaticMesh* FreshAsset = nullptr;
    FString Error;
    if (!TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
            CreateFreshAsset(FreshAsset, Error) ||
        !FreshAsset)
    {
        OutMessage = TEXT("EXPLORE_V5D_CURRENT_SURROUNDINGS_IMPORT_FAILED: ") +
            Error;
        return false;
    }

    UPackage* FreshPackage = FreshAsset->GetOutermost();
    TArray<UObject*> ExactSaveTargets = {FreshAsset};
    if (!FreshPackage || ExactSaveTargets.Num() != 1 ||
        ExactSaveTargets.Contains(nullptr) ||
        !AssetSubsystem->SaveLoadedAssets(ExactSaveTargets, false))
    {
        OutMessage = TEXT("EXPLORE_V5D_CURRENT_SURROUNDINGS_IMPORT_FAILED_SAVE: only the exact new render-mesh package was offered to save.");
        return false;
    }

    FreshPackage->SetDirtyFlag(false);
    FreshAsset = nullptr;
    ExactSaveTargets.Reset();
    const TArray<UPackage*> ReloadPackages = {FreshPackage};
    FText ReloadError;
    if (ReloadPackages.Num() != 1 || ReloadPackages.Contains(nullptr) ||
        !UPackageTools::ReloadPackages(
            ReloadPackages,
            ReloadError,
            EReloadPackagesInteractionMode::AssumePositive))
    {
        OutMessage = TEXT("EXPLORE_V5D_CURRENT_SURROUNDINGS_IMPORT_FAILED_RELOAD: ") +
            ReloadError.ToString();
        return false;
    }

    FString ColdReport;
    if (!TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::ValidateAsset(
            ColdReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_CURRENT_SURROUNDINGS_IMPORT_FAILED_COLD_VALIDATION: ") +
            ColdReport;
        return false;
    }

    OutMessage = TEXT("EXPLORE_V5D_CURRENT_SURROUNDINGS_ASSET_IMPORT_PASS: one exact hash-pinned 2026-08-31 public-OSM derivative was saved, reloaded, and cold-validated as a full-fidelity Nanite render mesh with preserved source UV0, 17 semantic slots, V5C presentation-material bindings, no collision, and no navigation data. It remains a volunteered public approximation: not survey/as-built/hyperreal truth and not sensor, propagation, RF-material, or RF-occlusion authority. No actor or map was changed. ") +
        ColdReport;
    return true;
}

bool UTRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary::
    ValidateIstanaExploreV5DCurrentSurroundingsAsset(FString& OutReport)
{
    return TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::ValidateAsset(
        OutReport);
}

bool UTRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary::
    ImportIstanaExploreV5DLocalFallbackSuppressedAsset(FString& OutMessage)
{
    FString ExistingReport;
    if (TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
            ValidateLocalFallbackSuppressedAsset(ExistingReport))
    {
        OutMessage =
            TEXT("IDEMPOTENT_EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V1_ASSET_ALREADY_VALID: ") +
            ExistingReport;
        return true;
    }

    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V1_IMPORT_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    UStaticMesh* FreshAsset = nullptr;
    FString Error;
    if (!TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
            CreateFreshLocalFallbackSuppressedAsset(FreshAsset, Error) ||
        !FreshAsset)
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V1_IMPORT_FAILED: ") +
            Error;
        return false;
    }

    FScopedSuppressedSavedImportRollback Rollback(AssetSubsystem, OutMessage);
    Rollback.Arm();
    UPackage* FreshPackage = FreshAsset->GetOutermost();
    TArray<UObject*> ExactSaveTargets = {FreshAsset};
    if (!FreshPackage || ExactSaveTargets.Num() != 1 ||
        ExactSaveTargets.Contains(nullptr) ||
        !AssetSubsystem->SaveLoadedAssets(ExactSaveTargets, false))
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V1_IMPORT_FAILED_SAVE: only the exact additive render-mesh package was offered to save.");
        return false;
    }

    FreshPackage->SetDirtyFlag(false);
    FreshAsset = nullptr;
    ExactSaveTargets.Reset();
    const TArray<UPackage*> ReloadPackages = {FreshPackage};
    FText ReloadError;
    if (ReloadPackages.Num() != 1 || ReloadPackages.Contains(nullptr) ||
        !UPackageTools::ReloadPackages(
            ReloadPackages,
            ReloadError,
            EReloadPackagesInteractionMode::AssumePositive))
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V1_IMPORT_FAILED_RELOAD: ") +
            ReloadError.ToString();
        return false;
    }

    FString ColdReport;
    if (!TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
            ValidateLocalFallbackSuppressedAsset(ColdReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V1_IMPORT_FAILED_COLD_VALIDATION: ") +
            ColdReport;
        return false;
    }

    Rollback.Commit();
    OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V1_ASSET_IMPORT_PASS: one additive 43,492-triangle hash-pinned local-fallback render mesh was saved, reloaded, and cold-validated. The original 43,544-triangle canonical mesh remains intact. Exactly OSM:way:46521250 and OSM:way:1551538490 are absent only from this local fallback; provider overlap remains unresolved. The derivative has full Nanite and raster fallback, zero collision/navigation, and no sensor, propagation, RF, survey, as-built, or hyperreal authority. No actor or map was changed. ") +
        ColdReport;
    return true;
}

bool UTRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary::
    ValidateIstanaExploreV5DLocalFallbackSuppressedAsset(FString& OutReport)
{
    return TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
        ValidateLocalFallbackSuppressedAsset(OutReport);
}

bool UTRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary::
    ImportIstanaExploreV5DLocalFallbackSuppressionV2(FString& OutMessage)
{
    FString ExistingReport;
    if (TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
            ValidateLocalFallbackSuppressedV2Asset(ExistingReport))
    {
        OutMessage =
            TEXT("IDEMPOTENT_EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_ASSET_ALREADY_VALID: ") +
            ExistingReport;
        return true;
    }

    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_IMPORT_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    UStaticMesh* FreshAsset = nullptr;
    FString Error;
    if (!TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
            CreateFreshLocalFallbackSuppressedV2Asset(FreshAsset, Error) ||
        !FreshAsset)
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_IMPORT_FAILED: ") +
            Error;
        return false;
    }

    FScopedSuppressedV2SavedImportRollback Rollback(
        AssetSubsystem, OutMessage);
    Rollback.Arm();
    UPackage* FreshPackage = FreshAsset->GetOutermost();
    TArray<UObject*> ExactSaveTargets = {FreshAsset};
    if (!FreshPackage || ExactSaveTargets.Num() != 1 ||
        ExactSaveTargets.Contains(nullptr) ||
        !AssetSubsystem->SaveLoadedAssets(ExactSaveTargets, false))
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_IMPORT_FAILED_SAVE: only the exact additive V2 render-mesh package was offered to save.");
        return false;
    }

    FreshPackage->SetDirtyFlag(false);
    FreshAsset = nullptr;
    ExactSaveTargets.Reset();
    const TArray<UPackage*> ReloadPackages = {FreshPackage};
    FText ReloadError;
    if (ReloadPackages.Num() != 1 || ReloadPackages.Contains(nullptr) ||
        !UPackageTools::ReloadPackages(
            ReloadPackages,
            ReloadError,
            EReloadPackagesInteractionMode::AssumePositive))
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_IMPORT_FAILED_RELOAD: ") +
            ReloadError.ToString();
        return false;
    }

    FString ColdReport;
    if (!TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
            ValidateLocalFallbackSuppressedV2Asset(ColdReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_IMPORT_FAILED_COLD_VALIDATION: ") +
            ColdReport;
        return false;
    }

    Rollback.Commit();
    OutMessage = TEXT("EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_ASSET_IMPORT_PASS: one additive 43,448-triangle hash-pinned local-fallback render mesh was saved, reloaded, and cold-validated. V1 and the original 43,544-triangle canonical mesh remain intact. Exactly OSM:way:46521250, OSM:way:1551538490, and OSM:way:429681826 are absent only from this V2 local fallback; provider overlap remains unresolved. The derivative has full Nanite and raster fallback, zero collision/navigation, and no sensor, propagation, RF, survey, as-built, or hyperreal authority. No actor or map was changed. ") +
        ColdReport;
    return true;
}

bool UTRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary::
    ValidateIstanaExploreV5DLocalFallbackSuppressionV2(FString& OutReport)
{
    return TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory::
        ValidateLocalFallbackSuppressedV2Asset(OutReport);
}

bool UTRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary::
    ImportIstanaExploreV5DContextFacadeR25Assets(FString& OutMessage)
{
    FString ExistingReport;
    if (TRIADIstanaExploreV5DContextFacadeR25AssetFactory::ValidateAssets(
            ExistingReport))
    {
        OutMessage =
            TEXT("IDEMPOTENT_EXPLORE_V5D_CONTEXT_FACADE_R25_ASSETS_ALREADY_VALID: ") +
            ExistingReport;
        return true;
    }

    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_IMPORT_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UObject*> FreshAssets;
    FString Error;
    if (!TRIADIstanaExploreV5DContextFacadeR25AssetFactory::
            CreateFreshAssets(FreshAssets, Error) ||
        FreshAssets.Num() != 5 || FreshAssets.Contains(nullptr))
    {
        OutMessage =
            TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_IMPORT_FAILED: ") + Error;
        return false;
    }

    FScopedContextFacadeR25SavedImportRollback Rollback(
        AssetSubsystem, OutMessage);
    Rollback.Arm();
    TArray<UPackage*> ReloadPackages;
    for (UObject* Asset : FreshAssets)
    {
        if (!Asset || !Asset->GetOutermost())
        {
            OutMessage = TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_IMPORT_FAILED_SCOPE: exact five-asset package roster was unavailable.");
            return false;
        }
        ReloadPackages.AddUnique(Asset->GetOutermost());
    }
    if (ReloadPackages.Num() != 5 ||
        !AssetSubsystem->SaveLoadedAssets(FreshAssets, false))
    {
        OutMessage = TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_IMPORT_FAILED_SAVE: only the exact five new material packages were offered to save.");
        return false;
    }

    for (UPackage* Package : ReloadPackages)
    {
        Package->SetDirtyFlag(false);
    }
    FreshAssets.Reset();
    FText ReloadError;
    if (!UPackageTools::ReloadPackages(
            ReloadPackages,
            ReloadError,
            EReloadPackagesInteractionMode::AssumePositive))
    {
        OutMessage = TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_IMPORT_FAILED_RELOAD: ") +
            ReloadError.ToString();
        return false;
    }

    FString ColdReport;
    if (!TRIADIstanaExploreV5DContextFacadeR25AssetFactory::ValidateAssets(
            ColdReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_IMPORT_FAILED_COLD_VALIDATION: ") +
            ColdReport;
        return false;
    }

    Rollback.Commit();
    OutMessage = TEXT("EXPLORE_V5D_CONTEXT_FACADE_R25_ASSET_IMPORT_PASS: five additive texture-free materials were saved, reloaded, and cold-validated. The existing V2 mesh and shared V5C assets were not changed; no actor or map was changed. ") +
        ColdReport;
    return true;
}

bool UTRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary::
    ValidateIstanaExploreV5DContextFacadeR25Assets(FString& OutReport)
{
    return TRIADIstanaExploreV5DContextFacadeR25AssetFactory::ValidateAssets(
        OutReport);
}
