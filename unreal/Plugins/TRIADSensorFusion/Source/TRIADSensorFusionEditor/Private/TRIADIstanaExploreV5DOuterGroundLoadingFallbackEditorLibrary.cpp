#include "TRIADIstanaExploreV5DOuterGroundLoadingFallbackEditorLibrary.h"

#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "PackageTools.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory.h"
#include "UObject/Package.h"

namespace
{
class FScopedSavedImportRollback final
{
public:
    FScopedSavedImportRollback(
        UEditorAssetSubsystem* InSubsystem,
        FString& InMessage)
        : Subsystem(InSubsystem), Message(InMessage)
    {
    }

    ~FScopedSavedImportRollback()
    {
        if (!bArmed || bCommitted || !Subsystem)
        {
            return;
        }
        const bool bMeshDeleted = Subsystem->DeleteAsset(
            TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory::
                GetMeshObjectPath());
        const bool bMaterialDeleted = Subsystem->DeleteAsset(
            TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory::
                GetMaterialObjectPath());
        if (!bMeshDeleted || !bMaterialDeleted)
        {
            Message += TEXT(" OUTER_GROUND_LOADING_FALLBACK_SAVED_ROLLBACK_INCOMPLETE");
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

bool UTRIADIstanaExploreV5DOuterGroundLoadingFallbackEditorLibrary::
    ImportIstanaExploreV5DOuterGroundLoadingFallbackAssets(
        FString& OutMessage)
{
    FString ExistingReport;
    if (TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory::
            ValidateAssets(ExistingReport))
    {
        OutMessage =
            TEXT("IDEMPOTENT_EXPLORE_V5D_OUTER_GROUND_LOADING_FALLBACK_ASSETS_ALREADY_VALID: ") +
            ExistingReport;
        return true;
    }

    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5D_OUTER_GROUND_LOADING_FALLBACK_IMPORT_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    UStaticMesh* FreshMesh = nullptr;
    UMaterial* FreshMaterial = nullptr;
    FString Error;
    if (!TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory::
            CreateFreshAssets(FreshMesh, FreshMaterial, Error) ||
        !FreshMesh || !FreshMaterial)
    {
        OutMessage = TEXT("EXPLORE_V5D_OUTER_GROUND_LOADING_FALLBACK_IMPORT_FAILED: ") +
            Error;
        return false;
    }

    FScopedSavedImportRollback Rollback(AssetSubsystem, OutMessage);
    Rollback.Arm();
    UPackage* MaterialPackage = FreshMaterial->GetOutermost();
    UPackage* MeshPackage = FreshMesh->GetOutermost();
    TArray<UObject*> ExactSaveTargets = {FreshMaterial, FreshMesh};
    if (!MaterialPackage || !MeshPackage || MaterialPackage == MeshPackage ||
        ExactSaveTargets.Num() != 2 || ExactSaveTargets.Contains(nullptr) ||
        !AssetSubsystem->SaveLoadedAssets(ExactSaveTargets, false))
    {
        OutMessage = TEXT("EXPLORE_V5D_OUTER_GROUND_LOADING_FALLBACK_IMPORT_FAILED_SAVE: only the exact new material and render-mesh packages were offered to save.");
        return false;
    }

    MaterialPackage->SetDirtyFlag(false);
    MeshPackage->SetDirtyFlag(false);
    FreshMaterial = nullptr;
    FreshMesh = nullptr;
    ExactSaveTargets.Reset();
    const TArray<UPackage*> ReloadPackages = {MaterialPackage, MeshPackage};
    FText ReloadError;
    if (ReloadPackages.Num() != 2 || ReloadPackages.Contains(nullptr) ||
        !UPackageTools::ReloadPackages(
            ReloadPackages,
            ReloadError,
            EReloadPackagesInteractionMode::AssumePositive))
    {
        OutMessage = TEXT("EXPLORE_V5D_OUTER_GROUND_LOADING_FALLBACK_IMPORT_FAILED_RELOAD: ") +
            ReloadError.ToString();
        return false;
    }

    FString ColdReport;
    if (!TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory::
            ValidateAssets(ColdReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_OUTER_GROUND_LOADING_FALLBACK_IMPORT_FAILED_COLD_VALIDATION: ") +
            ColdReport;
        return false;
    }

    Rollback.Commit();
    OutMessage = TEXT("EXPLORE_V5D_OUTER_GROUND_LOADING_FALLBACK_ASSET_IMPORT_PASS: the exact V5D-owned opaque macro material and 1,280-triangle outer-ground render mesh were saved, reloaded, and cold-validated with physical-metre UV0, identity import scale, one LOD, full Nanite and raster fallback, zero collision/navigation/shadow/distance-field participation, and one cooked negative-authority receipt. This endpoint changes no actor, map, provider policy, sensor or RF input. ") +
        ColdReport;
    return true;
}

bool UTRIADIstanaExploreV5DOuterGroundLoadingFallbackEditorLibrary::
    ValidateIstanaExploreV5DOuterGroundLoadingFallbackAssets(
        FString& OutReport)
{
    return TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory::
        ValidateAssets(OutReport);
}
