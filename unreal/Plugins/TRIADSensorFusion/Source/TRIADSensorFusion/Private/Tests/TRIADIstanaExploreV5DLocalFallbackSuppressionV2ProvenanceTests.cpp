#if WITH_DEV_AUTOMATION_TESTS

#include "TRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DLocalFallbackSuppressionV2ProvenanceTest,
    "TRIAD.Istana.ExploreV5D.LocalFallbackSuppressionV2.Provenance",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DLocalFallbackSuppressionV2ProvenanceTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance* Provenance =
        NewObject<
            UTRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance>();
    TestNotNull(TEXT("Cooked V2 suppression provenance allocates"), Provenance);
    if (!Provenance)
    {
        return false;
    }

    TestTrue(TEXT("Default V2 provenance is the exact admitted contract"),
        Provenance->IsCanonicalContract());
    TestEqual(TEXT("Exactly three source keys are suppressed"),
        Provenance->SuppressedSourceKeys.Num(), 3);
    if (Provenance->SuppressedSourceKeys.Num() == 3)
    {
        TestEqual(TEXT("MacDonald source key is first and exact"),
            Provenance->SuppressedSourceKeys[0],
            FString(TEXT("OSM:way:46521250")));
        TestEqual(TEXT("Temasek source key is second and exact"),
            Provenance->SuppressedSourceKeys[1],
            FString(TEXT("OSM:way:1551538490")));
        TestEqual(TEXT("Floating-shell source key is third and exact"),
            Provenance->SuppressedSourceKeys[2],
            FString(TEXT("OSM:way:429681826")));
    }
    TestEqual(TEXT("Filtered triangle census is exact"),
        Provenance->RenderTriangles, 43448);
    TestEqual(TEXT("Suppressed triangle delta is exact"),
        Provenance->SuppressedTriangles, 96);
    TestEqual(TEXT("Visible feature census is exact"),
        Provenance->VisibleFeatures, 1386);
    TestEqual(TEXT("Visible part census is exact"),
        Provenance->VisiblePolygonParts, 1388);
    TestTrue(TEXT("Canonical render and RF inputs remain hash-pinned"),
        Provenance->bCanonicalRenderAndRfInputsHashPinnedUnchanged);
    TestFalse(TEXT("Provider overlap remains unresolved"),
        Provenance->bProviderOverlapResolved);
    TestFalse(TEXT("Derivative has no collision/navigation/sensor/RF authority"),
        Provenance->bCollisionNavigationSensorRfAuthority);
    TestFalse(TEXT("Derivative makes no survey/as-built/hyperreal claim"),
        Provenance->bMeasuredSurveyAsBuiltHyperreal);

    Provenance->SuppressedSourceKeys.RemoveAt(2);
    TestFalse(TEXT("Reduced suppression scope invalidates provenance"),
        Provenance->IsCanonicalContract());
    Provenance->SetCanonicalContract();
    Provenance->SuppressedSourceKeys.Add(TEXT("OSM:way:unexpected"));
    TestFalse(TEXT("Expanded suppression scope invalidates provenance"),
        Provenance->IsCanonicalContract());
    Provenance->SetCanonicalContract();
    Provenance->bProviderOverlapResolved = true;
    TestFalse(TEXT("Claiming provider resolution invalidates provenance"),
        Provenance->IsCanonicalContract());
    Provenance->SetCanonicalContract();
    Provenance->RenderTriangles = 43492;
    TestFalse(TEXT("V1 triangle census invalidates V2 provenance"),
        Provenance->IsCanonicalContract());
    Provenance->SetCanonicalContract();
    TestTrue(TEXT("Canonical reset is deterministic"),
        Provenance->IsCanonicalContract());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
