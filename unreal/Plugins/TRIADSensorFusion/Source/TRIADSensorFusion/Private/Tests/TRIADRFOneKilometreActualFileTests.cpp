#if WITH_DEV_AUTOMATION_TESTS

#include "TRIADRFIndexedGeometryQuery.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

namespace
{
// An exact replay of the hash-pinned R24C inputs consumes 29,904,936
// containment triangle checks. Keep ordinary query defaults unchanged and
// opt this fixed large-file workload into the smallest clean bounded budget.
constexpr int64 PinnedContainmentTriangleChecks = 29904936;
constexpr int64 ActualFileContainmentTriangleCheckLimit = 30000000;
static_assert(
    PinnedContainmentTriangleChecks < ActualFileContainmentTriangleCheckLimit,
    "The actual-file containment budget must retain a finite positive margin.");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADRFOneKilometreActualFileTest,
    "TRIAD.RF.IndexedGeometryQuery.OneKilometreV2ActualFile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADRFOneKilometreActualFileTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const TSharedPtr<IPlugin> Plugin =
        IPluginManager::Get().FindPlugin(TEXT("TRIADSensorFusion"));
    if (!TestTrue(TEXT("TRIADSensorFusion plugin is registered"), Plugin.IsValid()))
    {
        return false;
    }

    const FString RFDirectory =
        FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/RF"));
    const FString GeometryPath = FPaths::Combine(
        RFDirectory, TEXT("IstanaPublicViewRFOneKilometreV2.geometry.json"));
    const FString CatalogPath = FPaths::Combine(
        RFDirectory, TEXT("istana_rf_materials_one_kilometre_v2.catalog.json"));

    TestTrue(TEXT("Full OneKilometreV2 geometry file exists"), FPaths::FileExists(GeometryPath));
    TestTrue(TEXT("Full OneKilometreV2 catalog file exists"), FPaths::FileExists(CatalogPath));

    const FString ExpectedGeometrySha256 =
        TEXT("85e654fba602b2dbc51eb64c6b66ff234c8ea1f948152612c755edc22c65da51");
    const FString ExpectedCatalogSha256 =
        TEXT("210cb26ddb9b531adeb3c917606a736aefc857eb6696da485a2e63dbb8b31662");
    const FString ExpectedContractSha256 =
        TEXT("fe509917ae59be0918bcd799f23dc981e00a394c6c328a7342f56b371c40bcc2");

    FString Error;
    FTRIADRFIndexedGeometryLoadLimits LoadLimits;
    LoadLimits.MaximumContainmentTriangleChecks =
        ActualFileContainmentTriangleCheckLimit;
    FTRIADRFIndexedGeometryQuery Query(LoadLimits);
    const bool bLoaded = Query.LoadFromJsonFiles(
        GeometryPath,
        CatalogPath,
        ExpectedGeometrySha256,
        ExpectedCatalogSha256,
        Error);
    TestTrue(
        *FString::Printf(TEXT("Full OneKilometreV2 files load: %s"), *Error),
        bLoaded);
    if (!bLoaded)
    {
        return false;
    }

    TestTrue(TEXT("Actual-file query is ready"), Query.IsReady());
    TestEqual(TEXT("Pinned vertex count"), Query.GetVertexCount(), 23496);
    TestEqual(TEXT("Pinned triangle count"), Query.GetTriangleCount(), 42700);
    TestEqual(TEXT("Pinned solid count"), Query.GetSolidCount(), 1094);
    TestEqual(TEXT("Pinned surface count"), Query.GetSurfaceCount(), 13936);
    TestEqual(TEXT("Pinned material count"), Query.GetMaterialCount(), 9);
    TestEqual(
        TEXT("Pinned full-file deterministic BVH node count"),
        Query.GetBVHNodeCount(),
        16383);

    const FTRIADRFIndexedGeometryMetadata& Metadata = Query.GetMetadata();
    TestEqual(TEXT("Pinned geometry hash retained"), Metadata.GeometrySha256, ExpectedGeometrySha256);
    TestEqual(TEXT("Pinned catalog hash retained"), Metadata.MaterialCatalogSha256, ExpectedCatalogSha256);
    TestEqual(TEXT("Pinned scene-contract hash retained"), Metadata.ContractSha256, ExpectedContractSha256);
    TestEqual(
        TEXT("Pinned revision retained"),
        Metadata.GeometryRevision,
        FString(TEXT("OneKilometreV2-R24C")));
    TestTrue(
        TEXT("Declared loader coverage contains the one-kilometre AOI"),
        Metadata.bModeledCoverageCoversOneKilometreAoi);
    TestTrue(
        TEXT("Declared loader coverage includes admitted surroundings"),
        Metadata.bModeledCoverageCoversSurroundings);
    TestFalse(
        TEXT("Public-reference geometry does not claim survey control"),
        Metadata.bModeledCoverageSurveyControlled);
    TestFalse(
        TEXT("Public-reference geometry does not claim field validation"),
        Metadata.bModeledCoverageFieldValidated);

    FTRIADRFIndexedMaterialMetadata Material;
    TestTrue(
        TEXT("Pinned masonry material metadata is present"),
        Query.TryGetMaterialMetadata(
            TEXT("MAT_RF_RENDERED_MASONRY_ASSUMED"), Material));
    TestEqual(
        TEXT("Pinned masonry material remains explicitly uncalibrated"),
        Material.CalibrationState,
        FString(TEXT("UNCALIBRATED_ASSUMPTION")));

    return true;
}

#endif
