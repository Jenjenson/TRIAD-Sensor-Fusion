#include "TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.h"

#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif
#include "PackageTools.h"
#include "Misc/PackageName.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DTemasekShophouseAssetFactory.h"
#include "TRIADIstanaExploreV5DHybridEditorLibrary.h"
#include "UObject/Package.h"

namespace
{
constexpr int32 Phase2ExpectedFreshAssetCount = 16;
const TCHAR* Phase2FreshRosterMarker =
    TEXT("TRIAD_TEMASEK_PHASE2_FRESH_ROSTER_EXPECTED=16");

/** Spans every post-create exit through save, reload, and cold validation. */
class FTemasekFreshAssetTransactionGuard final
{
public:
    explicit FTemasekFreshAssetTransactionGuard(FString& InReport)
        : Report(InReport)
    {
    }

    ~FTemasekFreshAssetTransactionGuard()
    {
        if (bReleased)
        {
            return;
        }
        FString RollbackReport;
        const bool bRollbackSucceeded =
            TRIADIstanaExploreV5DTemasekShophouseAssetFactory::
                RollbackActiveFreshAssetTransaction(RollbackReport);
        Report += FString::Printf(
            TEXT(" rollbackSucceeded=%s rollback={%s}"),
            bRollbackSucceeded ? TEXT("true") : TEXT("false"),
            *RollbackReport);
    }

    void Release() { bReleased = true; }

private:
    FString& Report;
    bool bReleased = false;
};
} // namespace

bool UTRIADIstanaExploreV5DTemasekShophouseEditorLibrary::
    ImportIstanaExploreV5DTemasekShophouseR24Assets(FString& OutMessage)
{
    FString ExistingReport;
    if (TRIADIstanaExploreV5DTemasekShophouseAssetFactory::ValidateAssets(
            ExistingReport))
    {
        OutMessage =
            TEXT("IDEMPOTENT_EXPLORE_V5D_R24_TEMASEK_ASSETS_ALREADY_VALID: ") +
            ExistingReport;
        return true;
    }

    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_TEMASEK_IMPORT_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UObject*> FreshAssets;
    FString Error;
    if (!TRIADIstanaExploreV5DTemasekShophouseAssetFactory::CreateFreshAssets(
            FreshAssets,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_TEMASEK_IMPORT_FAILED: ") +
            Error;
        return false;
    }
    FTemasekFreshAssetTransactionGuard TransactionGuard(OutMessage);
    if (FreshAssets.Num() != Phase2ExpectedFreshAssetCount ||
        FreshAssets.Contains(nullptr))
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R24_TEMASEK_IMPORT_FAILED_FRESH_ROSTER: active exact-empty-predecessor transaction did not yield sixteen non-null assets; expectedAssets=%d actualAssets=%d nullAsset=%s marker=%s."),
            Phase2ExpectedFreshAssetCount,
            FreshAssets.Num(),
            FreshAssets.Contains(nullptr) ? TEXT("true") : TEXT("false"),
            Phase2FreshRosterMarker);
        return false;
    }

    if (!AssetSubsystem->SaveLoadedAssets(FreshAssets, false))
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_TEMASEK_IMPORT_FAILED_SAVE: only the exact one-mesh/fifteen-non-foliage-material roster was offered to save.");
        return false;
    }

    TArray<UPackage*> Packages;
    for (UObject* Asset : FreshAssets)
    {
        if (UPackage* Package = Asset ? Asset->GetOutermost() : nullptr)
        {
            Package->SetDirtyFlag(false);
            Packages.AddUnique(Package);
        }
    }
    FreshAssets.Reset();
    if (Packages.Num() != Phase2ExpectedFreshAssetCount ||
        Packages.Contains(nullptr))
    {
        OutMessage = FString::Printf(
            TEXT("EXPLORE_V5D_R24_TEMASEK_IMPORT_FAILED_PACKAGE_ROSTER: expected sixteen unique packages; expectedPackages=%d actualPackages=%d marker=%s."),
            Phase2ExpectedFreshAssetCount,
            Packages.Num(),
            Phase2FreshRosterMarker);
        return false;
    }
    FText ReloadError;
    if (!UPackageTools::ReloadPackages(
            Packages,
            ReloadError,
            EReloadPackagesInteractionMode::AssumePositive))
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_TEMASEK_IMPORT_FAILED_RELOAD: ") +
            ReloadError.ToString();
        return false;
    }

    FString ColdReport;
    if (!TRIADIstanaExploreV5DTemasekShophouseAssetFactory::ValidateAssets(
            ColdReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_TEMASEK_IMPORT_FAILED_COLD_VALIDATION: ") +
            ColdReport;
        return false;
    }
    FString CommitReport;
    if (!TRIADIstanaExploreV5DTemasekShophouseAssetFactory::
            CommitActiveFreshAssetTransaction(CommitReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_TEMASEK_IMPORT_FAILED_TRANSACTION_COMMIT: ") +
            CommitReport;
        return false;
    }
    TransactionGuard.Release();
    OutMessage = TEXT("EXPLORE_V5D_R24_TEMASEK_ASSET_IMPORT_PASS: one exact 15,760-triangle zero-baked-foliage public-exterior mesh and fifteen deterministic texture-free procedural PBR materials were transactionally saved, reloaded, and cold-validated from an exact empty registry-and-disk predecessor. Architecture, planters, soil, pavers, swales and the source placement transform remain pinned; three source-local tree anchors are handed to ATRIADIstanaExploreV5DLandmarkVegetationActor. The CPU-readable LOD0 positions/indices/sections/material assignments are provenance-stamped and recomputed; this is an accidental-drift guard, not signed authority. Opaque roughness-controlled glass/water preserve Nanite; all materials remain uncalibrated visual priors. No map, provider exclusion, collision, navigation, sensor, or RF authority was changed. bakedFoliageRenderComponents=0 foliageTreeAnchors=3 ") +
        ColdReport + TEXT(" ") + CommitReport;
    return true;
}

bool UTRIADIstanaExploreV5DTemasekShophouseEditorLibrary::
    ValidateIstanaExploreV5DTemasekShophouseR24Assets(FString& OutReport)
{
    return TRIADIstanaExploreV5DTemasekShophouseAssetFactory::ValidateAssets(
        OutReport);
}

bool UTRIADIstanaExploreV5DTemasekShophouseEditorLibrary::
    ImportApplyAndValidateIstanaExploreV5DTemasekShophouseR24(
        FString& OutReport)
{
    FString AssetReport;
    if (!ImportIstanaExploreV5DTemasekShophouseR24Assets(AssetReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R24_TEMASEK_END_TO_END_FAILED_IMPORT: ") +
            AssetReport;
        return false;
    }
    const FString DestinationMapPackage(
        TEXT("/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"));
    UWorld* CurrentWorld = GEditor
        ? GEditor->GetEditorWorldContext().World()
        : nullptr;
    const FString CurrentLogicalPackage =
        CurrentWorld && CurrentWorld->GetOutermost()
        ? UWorld::RemovePIEPrefix(CurrentWorld->GetOutermost()->GetName())
        : FString();
    FString BuildOrLoadReport;
    if (CurrentLogicalPackage != DestinationMapPackage)
    {
        FString DestinationFilename;
        if (FPackageName::DoesPackageExist(
                DestinationMapPackage,
                &DestinationFilename))
        {
            if (!UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename))
            {
                OutReport = TEXT("EXPLORE_V5D_R24_TEMASEK_END_TO_END_FAILED_LOAD_MAP: ") +
                    DestinationFilename;
                return false;
            }
            BuildOrLoadReport =
                TEXT("loadedExistingDestination=") + DestinationFilename;
        }
        else if (!UTRIADIstanaExploreV5DHybridEditorLibrary::
                     BuildIstanaExploreV5DHybridMap(BuildOrLoadReport))
        {
            OutReport = TEXT("EXPLORE_V5D_R24_TEMASEK_END_TO_END_FAILED_BUILD_MAP: ") +
                BuildOrLoadReport;
            return false;
        }
    }
    else
    {
        BuildOrLoadReport = TEXT("destinationAlreadyLoaded=true");
    }
    FString ApplyReport;
    if (!UTRIADIstanaExploreV5DHybridEditorLibrary::
            ApplyIstanaExploreV5DTemasekShophouseR24ToLoadedHybridMap(
                ApplyReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R24_TEMASEK_END_TO_END_FAILED_APPLY: ") +
            ApplyReport;
        return false;
    }
    FString MapReport;
    if (!UTRIADIstanaExploreV5DHybridEditorLibrary::
            ValidateIstanaExploreV5DHybridMap(MapReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R24_TEMASEK_END_TO_END_FAILED_VALIDATE: ") +
            MapReport;
        return false;
    }
    OutReport =
        TEXT("EXPLORE_V5D_R24_TEMASEK_END_TO_END_PASS import={") +
        AssetReport + TEXT("} buildOrLoad={") + BuildOrLoadReport +
        TEXT("} apply={") + ApplyReport + TEXT("} map={") + MapReport +
        TEXT("}");
    return true;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DTemasekShophouseEndToEndContractTest,
    "TRIAD.Istana.ExploreV5D.TemasekShophouse.EndToEndContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DTemasekShophouseEndToEndContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    FString Report;
    if (!UTRIADIstanaExploreV5DTemasekShophouseEditorLibrary::
            ImportApplyAndValidateIstanaExploreV5DTemasekShophouseR24(Report))
    {
        AddError(Report);
        return false;
    }
    AddInfo(Report);
    return true;
}
#endif
