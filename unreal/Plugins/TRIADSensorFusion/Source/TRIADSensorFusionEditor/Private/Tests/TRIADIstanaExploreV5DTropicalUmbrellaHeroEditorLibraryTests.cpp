#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "TRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary.h"

namespace
{
constexpr const TCHAR* OutputRoot =
    TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration");
constexpr const TCHAR* StagingRoot =
    TEXT("/Temp/TRIAD/TropicalUmbrellaHeroIntegrationSourceStaging");

int32 RecursiveRegistryAssetCount(const TCHAR* Root)
{
    TArray<FAssetData> Assets;
    FAssetRegistryModule::GetRegistry().GetAssetsByPath(
        FName(Root),
        Assets,
        true,
        false);
    return Assets.Num();
}

struct FNamespaceObservation
{
    int32 RegistryAssets = 0;
    int32 PhysicalFiles = 0;
    int32 PhysicalDirectories = 0;
    bool bPhysicalRootExists = false;
};

FNamespaceObservation ObserveNamespace(const TCHAR* Root)
{
    FNamespaceObservation Result;
    Result.RegistryAssets = RecursiveRegistryAssetCount(Root);
    FString PhysicalRoot;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            Root,
            PhysicalRoot,
            FString()))
    {
        return Result;
    }
    FPaths::NormalizeDirectoryName(PhysicalRoot);
    Result.bPhysicalRootExists =
        IFileManager::Get().DirectoryExists(*PhysicalRoot);
    TArray<FString> Files;
    TArray<FString> Directories;
    IFileManager::Get().FindFilesRecursive(
        Files, *PhysicalRoot, TEXT("*"), true, false, true);
    IFileManager::Get().FindFilesRecursive(
        Directories, *PhysicalRoot, TEXT("*"), false, true, true);
    Result.PhysicalFiles = Files.Num();
    Result.PhysicalDirectories =
        Directories.Num() + (Result.bPhysicalRootExists ? 1 : 0);
    return Result;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaTropicalUmbrellaInspectorReadOnlyTest,
    "TRIAD.Istana.ExploreV5D.TropicalUmbrellaHero.Editor.InspectorFailsClosedReadOnly",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaTropicalUmbrellaInspectorReadOnlyTest::RunTest(
    const FString& Parameters)
{
    const FNamespaceObservation OutputBefore = ObserveNamespace(OutputRoot);
    const FNamespaceObservation StagingBefore = ObserveNamespace(StagingRoot);
    FString Report;
    TestFalse(
        TEXT("Empty untrusted inputs are denied"),
        UTRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary::
            InspectTropicalUmbrellaHeroReceipts(
                FString(),
                FString(),
                FString(),
                FString(),
                FString(),
                Report));
    TestTrue(
        TEXT("Denial is explicit"),
        Report.StartsWith(
            TEXT("ISTANA_TROPICAL_UMBRELLA_HERO_RECEIPT_INSPECTION_DENIED:")));
    TestEqual(
        TEXT("Inspection writes no output assets"),
        RecursiveRegistryAssetCount(OutputRoot),
        OutputBefore.RegistryAssets);
    TestEqual(
        TEXT("Inspection writes no staging assets"),
        RecursiveRegistryAssetCount(StagingRoot),
        StagingBefore.RegistryAssets);
    const FNamespaceObservation OutputAfter = ObserveNamespace(OutputRoot);
    const FNamespaceObservation StagingAfter = ObserveNamespace(StagingRoot);
    TestEqual(TEXT("Inspection writes no output files"), OutputAfter.PhysicalFiles, OutputBefore.PhysicalFiles);
    TestEqual(TEXT("Inspection writes no output directories"), OutputAfter.PhysicalDirectories, OutputBefore.PhysicalDirectories);
    TestEqual(TEXT("Inspection does not create output root"), OutputAfter.bPhysicalRootExists, OutputBefore.bPhysicalRootExists);
    TestEqual(TEXT("Inspection writes no staging files"), StagingAfter.PhysicalFiles, StagingBefore.PhysicalFiles);
    TestEqual(TEXT("Inspection writes no staging directories"), StagingAfter.PhysicalDirectories, StagingBefore.PhysicalDirectories);
    TestEqual(TEXT("Inspection does not create staging root"), StagingAfter.bPhysicalRootExists, StagingBefore.bPhysicalRootExists);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaTropicalUmbrellaReceiptSeparationTest,
    "TRIAD.Istana.ExploreV5D.TropicalUmbrellaHero.Editor.DistinctReceiptBoundary",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaTropicalUmbrellaReceiptSeparationTest::RunTest(
    const FString& Parameters)
{
    const FString CandidateRoot = FPaths::ConvertRelativePathToFull(
        TEXT("TropicalUmbrellaCandidate"));
    const FString SameReceipt = FPaths::ConvertRelativePathToFull(
        TEXT("SameReceipt.json"));
    const FString SameSha(
        TEXT("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"));
    const FNamespaceObservation OutputBefore = ObserveNamespace(OutputRoot);
    const FNamespaceObservation StagingBefore = ObserveNamespace(StagingRoot);
    FString Report;
    TestFalse(
        TEXT("One receipt cannot satisfy current and future trust"),
        UTRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary::
            InspectTropicalUmbrellaHeroReceipts(
                CandidateRoot,
                SameReceipt,
                SameSha,
                SameReceipt,
                SameSha,
                Report));
    TestTrue(
        TEXT("Receipt separation fails before file admission"),
        Report.Contains(
            TEXT("different files with different hashes")));
    TestEqual(
        TEXT("Separation check writes no output assets"),
        RecursiveRegistryAssetCount(OutputRoot),
        OutputBefore.RegistryAssets);
    TestEqual(
        TEXT("Separation check writes no staging assets"),
        RecursiveRegistryAssetCount(StagingRoot),
        StagingBefore.RegistryAssets);
    const FNamespaceObservation OutputAfter = ObserveNamespace(OutputRoot);
    const FNamespaceObservation StagingAfter = ObserveNamespace(StagingRoot);
    TestEqual(TEXT("Separation writes no output files"), OutputAfter.PhysicalFiles, OutputBefore.PhysicalFiles);
    TestEqual(TEXT("Separation writes no output directories"), OutputAfter.PhysicalDirectories, OutputBefore.PhysicalDirectories);
    TestEqual(TEXT("Separation does not create output root"), OutputAfter.bPhysicalRootExists, OutputBefore.bPhysicalRootExists);
    TestEqual(TEXT("Separation writes no staging files"), StagingAfter.PhysicalFiles, StagingBefore.PhysicalFiles);
    TestEqual(TEXT("Separation writes no staging directories"), StagingAfter.PhysicalDirectories, StagingBefore.PhysicalDirectories);
    TestEqual(TEXT("Separation does not create staging root"), StagingAfter.bPhysicalRootExists, StagingBefore.bPhysicalRootExists);
    return true;
}

#endif
