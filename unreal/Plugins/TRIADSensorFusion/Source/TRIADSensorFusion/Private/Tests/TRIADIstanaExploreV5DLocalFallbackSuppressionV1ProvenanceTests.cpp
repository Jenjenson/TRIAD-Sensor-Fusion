#if WITH_DEV_AUTOMATION_TESTS

#include "TRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DLocalFallbackSuppressionV1ProvenanceTest,
    "TRIAD.Istana.ExploreV5D.LocalFallbackSuppressionV1.Provenance",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DLocalFallbackSuppressionV1ProvenanceTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance* Provenance =
        NewObject<
            UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance>();
    TestNotNull(TEXT("Cooked suppression provenance allocates"), Provenance);
    if (!Provenance)
    {
        return false;
    }

    TestTrue(TEXT("Default provenance is the exact admitted contract"),
        Provenance->IsCanonicalContract());
    TestEqual(TEXT("Only two source keys are suppressed"),
        Provenance->SuppressedSourceKeys.Num(), 2);
    if (Provenance->SuppressedSourceKeys.Num() == 2)
    {
        TestEqual(TEXT("MacDonald source key is first and exact"),
            Provenance->SuppressedSourceKeys[0],
            FString(TEXT("OSM:way:46521250")));
        TestEqual(TEXT("Temasek source key is second and exact"),
            Provenance->SuppressedSourceKeys[1],
            FString(TEXT("OSM:way:1551538490")));
    }
    TestEqual(TEXT("Filtered triangle census is exact"),
        Provenance->RenderTriangles, 43492);
    TestEqual(TEXT("Suppressed triangle delta is exact"),
        Provenance->SuppressedTriangles, 52);
    TestTrue(TEXT("Nanite keeps the complete admitted mesh"),
        Provenance->bNaniteFullMesh);
    TestTrue(TEXT("Raster fallback keeps the complete admitted mesh"),
        Provenance->bRasterFallbackFullMesh);
    TestTrue(TEXT("Canonical render and RF inputs remain hash-pinned"),
        Provenance->bCanonicalRenderAndRfInputsHashPinnedUnchanged);
    TestFalse(TEXT("Provider overlap remains unresolved"),
        Provenance->bProviderOverlapResolved);
    TestFalse(TEXT("Derivative has no collision/navigation/sensor/RF authority"),
        Provenance->bCollisionNavigationSensorRfAuthority);
    TestFalse(TEXT("Derivative makes no survey/as-built/hyperreal claim"),
        Provenance->bMeasuredSurveyAsBuiltHyperreal);

    Provenance->SuppressedSourceKeys.Add(TEXT("OSM:way:unexpected"));
    TestFalse(TEXT("Expanded suppression scope invalidates provenance"),
        Provenance->IsCanonicalContract());
    Provenance->SetCanonicalContract();
    Provenance->bProviderOverlapResolved = true;
    TestFalse(TEXT("Claiming provider resolution invalidates provenance"),
        Provenance->IsCanonicalContract());
    Provenance->SetCanonicalContract();
    Provenance->RenderTriangles = 43544;
    TestFalse(TEXT("Unsuppressed triangle census invalidates provenance"),
        Provenance->IsCanonicalContract());
    Provenance->SetCanonicalContract();
    TestTrue(TEXT("Canonical reset is deterministic"),
        Provenance->IsCanonicalContract());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
