#include "TRIADIstanaExploreV5DPublicRealmEditorLibrary.h"

#include "Editor.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "PackageTools.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DPublicRealmAssetFactory.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
TArray<FString> ExpectedAllFreshObjectPaths()
{
    TArray<FString> Paths = {
        TRIADIstanaExploreV5DPublicRealmAssetFactory::
            GetAsphaltMaterialObjectPath(),
        TRIADIstanaExploreV5DPublicRealmAssetFactory::
            GetRoadGraphicSuppressionMaterialObjectPath(),
        TRIADIstanaExploreV5DPublicRealmAssetFactory::
            GetConcreteMaterialObjectPath(),
        TRIADIstanaExploreV5DPublicRealmAssetFactory::
            GetCoreMeshObjectPath(),
        TRIADIstanaExploreV5DPublicRealmAssetFactory::
            GetFallbackMeshObjectPath(),
    };
    Paths.Sort();
    return Paths;
}

TArray<FString> ExpectedVisualUpgradeObjectPaths()
{
    TArray<FString> Paths = {
        TRIADIstanaExploreV5DPublicRealmAssetFactory::
            GetAsphaltMaterialObjectPath(),
        TRIADIstanaExploreV5DPublicRealmAssetFactory::
            GetRoadGraphicSuppressionMaterialObjectPath(),
    };
    Paths.Sort();
    return Paths;
}

bool HasExactFreshRoster(
    const TArray<UObject*>& Assets,
    TArray<UPackage*>& OutPackages,
    FString& OutError)
{
    OutPackages.Reset();
    TArray<FString> ActualPaths;
    for (UObject* Asset : Assets)
    {
        UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
        if (!Asset || !Package ||
            !Package->HasAnyPackageFlags(PKG_NewlyCreated) ||
            FPackageName::DoesPackageExist(Package->GetName()))
        {
            OutError = TEXT("A public-realm save target is null, not newly created, or already persisted.");
            OutPackages.Reset();
            return false;
        }
        ActualPaths.Add(Asset->GetPathName());
        OutPackages.AddUnique(Package);
    }
    ActualPaths.Sort();
    const bool bFullFresh = ActualPaths == ExpectedAllFreshObjectPaths();
    const bool bVisualUpgrade =
        ActualPaths == ExpectedVisualUpgradeObjectPaths();
    if ((!bFullFresh && !bVisualUpgrade) ||
        OutPackages.Num() != Assets.Num())
    {
        OutError = FString::Printf(
            TEXT("Public-realm save roster is neither the exact five fresh assets nor the exact two-asset Visual R2 upgrade: [%s]."),
            *FString::Join(ActualPaths, TEXT(", ")));
        OutPackages.Reset();
        return false;
    }
    OutError.Reset();
    return true;
}

TArray<FString> ExactPaths(const TArray<UObject*>& Assets)
{
    TArray<FString> Paths;
    for (const UObject* Asset : Assets)
    {
        if (Asset)
        {
            Paths.Add(Asset->GetPathName());
        }
    }
    Paths.Sort();
    return Paths;
}

bool DeleteExactTransactionAssets(
    const TArray<FString>& TransactionPaths,
    FString& OutDetail)
{
    TArray<UObject*> Objects;
    for (const FString& Path : TransactionPaths)
    {
        if (UObject* Object = LoadObject<UObject>(nullptr, *Path))
        {
            if (Object->GetPathName() != Path)
            {
                OutDetail = TEXT("rollback encountered a non-exact object path");
                return false;
            }
            Objects.Add(Object);
        }
    }
    if (Objects.IsEmpty())
    {
        OutDetail = TEXT("rollback found no transaction assets");
        return true;
    }
    const int32 Requested = Objects.Num();
    const int32 Deleted = ObjectTools::DeleteObjectsUnchecked(Objects);
    const bool bComplete = Deleted == Requested;
    OutDetail = FString::Printf(
        TEXT("rollback deleted %d/%d exact transaction assets"),
        Deleted,
        Requested);
    return bComplete;
}
} // namespace

bool UTRIADIstanaExploreV5DPublicRealmEditorLibrary::
    ImportIstanaExploreV5DPublicRealmAssets(FString& OutMessage)
{
    FString ExistingReport;
    if (TRIADIstanaExploreV5DPublicRealmAssetFactory::ValidateAssets(
            ExistingReport))
    {
        OutMessage =
            TEXT("IDEMPOTENT_EXPLORE_V5D_PUBLIC_REALM_ASSETS_ALREADY_VALID: ") +
            ExistingReport;
        return true;
    }

    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_IMPORT_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UObject*> FreshAssets;
    FString Error;
    FString LegacyReport;
    const bool bVisualUpgrade =
        TRIADIstanaExploreV5DPublicRealmAssetFactory::ValidateLegacyAssets(
            LegacyReport);
    const bool bCreated = bVisualUpgrade
        ? TRIADIstanaExploreV5DPublicRealmAssetFactory::
              CreateFreshVisualMaterialUpgrade(FreshAssets, Error)
        : TRIADIstanaExploreV5DPublicRealmAssetFactory::CreateFreshAssets(
              FreshAssets, Error);
    if (!bCreated)
    {
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_IMPORT_FAILED: ") + Error;
        return false;
    }
    const TArray<FString> TransactionPaths = ExactPaths(FreshAssets);

    TArray<UPackage*> FreshPackages;
    if (!HasExactFreshRoster(FreshAssets, FreshPackages, Error))
    {
        FString Rollback;
        DeleteExactTransactionAssets(TransactionPaths, Rollback);
        OutMessage = TEXT("EXPLORE_V5D_PUBLIC_REALM_IMPORT_FAILED_ROSTER: ") +
            Error + TEXT("; ") + Rollback;
        return false;
    }

    // One exact save request is the commit point. A failed or invalid commit
    // deletes only this transaction's exact known assets, leaving no partial
    // public-realm namespace behind when rollback succeeds.
    if (!AssetSubsystem->SaveLoadedAssets(FreshAssets, false))
    {
        FString Rollback;
        const bool bRolledBack = DeleteExactTransactionAssets(
            TransactionPaths,
            Rollback);
        OutMessage =
            TEXT("EXPLORE_V5D_PUBLIC_REALM_IMPORT_FAILED_ATOMIC_SAVE: ") +
            Rollback +
            (bRolledBack ? TEXT(".") : TEXT("; ROLLBACK_INCOMPLETE."));
        return false;
    }

    for (UPackage* Package : FreshPackages)
    {
        if (Package)
        {
            Package->SetDirtyFlag(false);
        }
    }
    FreshAssets.Reset();
    FText ReloadError;
    if (!UPackageTools::ReloadPackages(
            FreshPackages,
            ReloadError,
            EReloadPackagesInteractionMode::AssumePositive))
    {
        FString Rollback;
        const bool bRolledBack = DeleteExactTransactionAssets(
            TransactionPaths,
            Rollback);
        OutMessage =
            TEXT("EXPLORE_V5D_PUBLIC_REALM_IMPORT_FAILED_RELOAD: ") +
            ReloadError.ToString() + TEXT("; ") + Rollback +
            (bRolledBack ? TEXT(".") : TEXT("; ROLLBACK_INCOMPLETE."));
        return false;
    }
    FreshPackages.Reset();

    FString ColdReport;
    if (!TRIADIstanaExploreV5DPublicRealmAssetFactory::ValidateAssets(
            ColdReport))
    {
        FString Rollback;
        const bool bRolledBack = DeleteExactTransactionAssets(
            TransactionPaths,
            Rollback);
        OutMessage =
            TEXT("EXPLORE_V5D_PUBLIC_REALM_IMPORT_FAILED_COLD_VALIDATION: ") +
            ColdReport + TEXT("; ") + Rollback +
            (bRolledBack ? TEXT(".") : TEXT("; ROLLBACK_INCOMPLETE."));
        return false;
    }

    OutMessage = FString::Printf(
        TEXT("EXPLORE_V5D_PUBLIC_REALM_ASSET_IMPORT_PASS: transaction=%s; exact immutable core/fallback meshes and concrete MIC plus isolated V5D-owned dual-scale PBR asphalt and fully clipped RoadGraphic materials were saved, reloaded, and cold-validated; baked V5C planning bindings and topology remain unchanged, the official planning graphic is fully clipped rather than mislabeled as road paint, and collision/navigation remain disabled. No actor or map was changed. "),
        bVisualUpgrade ? TEXT("additive-two-material-visual-r2") : TEXT("fresh-five-assets")) +
        ColdReport;
    return true;
}

bool UTRIADIstanaExploreV5DPublicRealmEditorLibrary::
    ValidateIstanaExploreV5DPublicRealmAssets(FString& OutReport)
{
    return TRIADIstanaExploreV5DPublicRealmAssetFactory::ValidateAssets(
        OutReport);
}
