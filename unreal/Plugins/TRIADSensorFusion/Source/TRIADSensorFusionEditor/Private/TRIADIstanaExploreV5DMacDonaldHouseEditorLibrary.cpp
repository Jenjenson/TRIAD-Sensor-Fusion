#include "TRIADIstanaExploreV5DMacDonaldHouseEditorLibrary.h"

#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif
#include "PackageTools.h"
#include "Misc/PackageName.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "TRIADIstanaExploreV5DMacDonaldHouseAssetFactory.h"
#include "TRIADIstanaExploreV5DHybridEditorLibrary.h"
#include "UObject/Package.h"

namespace
{
/** Spans every post-create exit through save, reload, and cold validation. */
class FMacDonaldFreshAssetTransactionGuard final
{
public:
    explicit FMacDonaldFreshAssetTransactionGuard(FString& InReport)
        : Report(InReport)
    {
    }

    ~FMacDonaldFreshAssetTransactionGuard()
    {
        if (bReleased)
        {
            return;
        }
        FString RollbackReport;
        const bool bRollbackSucceeded =
            TRIADIstanaExploreV5DMacDonaldHouseAssetFactory::
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

bool UTRIADIstanaExploreV5DMacDonaldHouseEditorLibrary::
    ImportIstanaExploreV5DMacDonaldHouseR24Assets(FString& OutMessage)
{
    FString ExistingReport;
    if (TRIADIstanaExploreV5DMacDonaldHouseAssetFactory::ValidateAssets(
            ExistingReport))
    {
        OutMessage =
            TEXT("IDEMPOTENT_EXPLORE_V5D_R24_MACDONALD_ASSETS_ALREADY_VALID: ") +
            ExistingReport;
        return true;
    }

    UEditorAssetSubsystem* AssetSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
        : nullptr;
    if (!AssetSubsystem)
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_MACDONALD_IMPORT_REFUSED: editor asset subsystem unavailable.");
        return false;
    }

    TArray<UObject*> FreshAssets;
    FString Error;
    if (!TRIADIstanaExploreV5DMacDonaldHouseAssetFactory::CreateFreshAssets(
            FreshAssets,
            Error))
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_MACDONALD_IMPORT_FAILED: ") +
            Error;
        return false;
    }
    FMacDonaldFreshAssetTransactionGuard TransactionGuard(OutMessage);
    if (FreshAssets.Num() != 10 || FreshAssets.Contains(nullptr))
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_MACDONALD_IMPORT_FAILED_FRESH_ROSTER: active exact-empty-predecessor transaction did not yield ten non-null assets.");
        return false;
    }

    if (!AssetSubsystem->SaveLoadedAssets(FreshAssets, false))
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_MACDONALD_IMPORT_FAILED_SAVE: only the exact one-mesh/nine-material roster was offered to save.");
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
    if (Packages.Num() != 10 || Packages.Contains(nullptr))
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_MACDONALD_IMPORT_FAILED_PACKAGE_ROSTER: expected ten unique packages.");
        return false;
    }
    FText ReloadError;
    if (!UPackageTools::ReloadPackages(
            Packages,
            ReloadError,
            EReloadPackagesInteractionMode::AssumePositive))
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_MACDONALD_IMPORT_FAILED_RELOAD: ") +
            ReloadError.ToString();
        return false;
    }

    FString ColdReport;
    if (!TRIADIstanaExploreV5DMacDonaldHouseAssetFactory::ValidateAssets(
            ColdReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_MACDONALD_IMPORT_FAILED_COLD_VALIDATION: ") +
            ColdReport;
        return false;
    }
    FString CommitReport;
    if (!TRIADIstanaExploreV5DMacDonaldHouseAssetFactory::
            CommitActiveFreshAssetTransaction(CommitReport))
    {
        OutMessage = TEXT("EXPLORE_V5D_R24_MACDONALD_IMPORT_FAILED_TRANSACTION_COMMIT: ") +
            CommitReport;
        return false;
    }
    TransactionGuard.Release();
    OutMessage = TEXT("EXPLORE_V5D_R24_MACDONALD_ASSET_IMPORT_PASS: one exact 7,080-triangle public-exterior mesh and nine deterministic texture-free procedural PBR materials were transactionally saved, reloaded, and cold-validated from an exact empty registry-and-disk predecessor. The CPU-readable LOD0 positions/indices/sections/material assignments are provenance-stamped and recomputed; this is an accidental-drift guard, not signed authority. The opaque roughness-controlled glass preserves Nanite; all materials remain uncalibrated visual priors. No actor, map, Cesium exclusion, collision, navigation, sensor, or RF authority was changed. ") +
        ColdReport + TEXT(" ") + CommitReport;
    return true;
}

bool UTRIADIstanaExploreV5DMacDonaldHouseEditorLibrary::
    ValidateIstanaExploreV5DMacDonaldHouseR24Assets(FString& OutReport)
{
    return TRIADIstanaExploreV5DMacDonaldHouseAssetFactory::ValidateAssets(
        OutReport);
}

bool UTRIADIstanaExploreV5DMacDonaldHouseEditorLibrary::
    ImportApplyAndValidateIstanaExploreV5DMacDonaldHouseR24(
        FString& OutReport)
{
    FString AssetReport;
    if (!ImportIstanaExploreV5DMacDonaldHouseR24Assets(AssetReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R24_MACDONALD_END_TO_END_FAILED_IMPORT: ") +
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
                OutReport = TEXT("EXPLORE_V5D_R24_MACDONALD_END_TO_END_FAILED_LOAD_MAP: ") +
                    DestinationFilename;
                return false;
            }
            BuildOrLoadReport =
                TEXT("loadedExistingDestination=") + DestinationFilename;
        }
        else if (!UTRIADIstanaExploreV5DHybridEditorLibrary::
                     BuildIstanaExploreV5DHybridMap(BuildOrLoadReport))
        {
            OutReport = TEXT("EXPLORE_V5D_R24_MACDONALD_END_TO_END_FAILED_BUILD_MAP: ") +
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
            ApplyIstanaExploreV5DMacDonaldHouseR24ToLoadedHybridMap(
                ApplyReport))
    {
        OutReport = TEXT("EXPLORE_V5D_R24_MACDONALD_END_TO_END_FAILED_APPLY: ") +
            ApplyReport;
        return false;
    }
    FString MapReport;
    FString FinalMapReport;
    const bool bExactMacOnlySuccessor =
        UTRIADIstanaExploreV5DHybridEditorLibrary::
            ValidateIstanaExploreV5DMacDonaldHouseR24SuccessorMap(MapReport);
    const bool bExactFinalTwoLandmarkMap = !bExactMacOnlySuccessor &&
        UTRIADIstanaExploreV5DHybridEditorLibrary::
            ValidateIstanaExploreV5DHybridMap(FinalMapReport);
    if (!bExactMacOnlySuccessor && !bExactFinalTwoLandmarkMap)
    {
        OutReport = TEXT("EXPLORE_V5D_R24_MACDONALD_END_TO_END_FAILED_VALIDATE: ") +
            MapReport + TEXT(" final={") + FinalMapReport + TEXT("}");
        return false;
    }
    if (bExactFinalTwoLandmarkMap)
    {
        MapReport = FinalMapReport;
    }
    OutReport =
        TEXT("EXPLORE_V5D_R24_MACDONALD_END_TO_END_PASS import={") +
        AssetReport + TEXT("} buildOrLoad={") + BuildOrLoadReport +
        TEXT("} apply={") + ApplyReport + TEXT("} map={") + MapReport +
        TEXT("}");
    return true;
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DMacDonaldHouseEndToEndContractTest,
    "TRIAD.Istana.ExploreV5D.MacDonaldHouse.EndToEndContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DMacDonaldHouseEndToEndContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    FString Report;
    if (!UTRIADIstanaExploreV5DMacDonaldHouseEditorLibrary::
            ImportApplyAndValidateIstanaExploreV5DMacDonaldHouseR24(Report))
    {
        AddError(Report);
        return false;
    }
    AddInfo(Report);
    return true;
}
#endif
