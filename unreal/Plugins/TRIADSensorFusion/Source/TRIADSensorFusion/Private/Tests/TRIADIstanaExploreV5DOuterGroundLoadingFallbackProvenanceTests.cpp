#if WITH_DEV_AUTOMATION_TESTS

#include "TRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenanceTest,
    "TRIAD.Istana.ExploreV5D.OuterGroundLoadingFallback.Provenance",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenanceTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    auto* Provenance = NewObject<
        UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance>();
    TestNotNull(TEXT("Cooked outer-ground provenance allocates"), Provenance);
    if (!Provenance)
    {
        return false;
    }

    TestTrue(TEXT("Default receipt is canonical"),
        Provenance->IsCanonicalContract());
    TestEqual(TEXT("Exact triangle topology is cooked"),
        Provenance->Triangles, 1280);
    TestEqual(TEXT("Exact material slot is cooked"),
        Provenance->MaterialSlot,
        FString(TEXT("M_IPV5D_OuterGroundLoadingFallback")));
    TestTrue(TEXT("UV0 is admitted in source metres"),
        Provenance->bPhysicalMetreUv0);
    TestTrue(TEXT("Nanite retains all triangles"),
        Provenance->bNaniteFullMesh);
    TestTrue(TEXT("Raster fallback retains all triangles"),
        Provenance->bRasterFallbackFullMesh);
    TestFalse(TEXT("No terrain/survey/as-built authority"),
        Provenance->bTerrainSurveyOrAsBuiltAuthority);
    TestFalse(TEXT("No sensor/RF authority"),
        Provenance->bSensorOcclusionOrRfAuthority);
    TestFalse(TEXT("No map/provider-policy integration is claimed"),
        Provenance->bLiveMapOrProviderPolicyIntegrationIncluded);

    Provenance->ObjSha256 = TEXT("00");
    TestFalse(TEXT("Source hash drift is rejected"),
        Provenance->IsCanonicalContract());
    Provenance->SetCanonicalContract();
    Provenance->Triangles = 1279;
    TestFalse(TEXT("Topology drift is rejected"),
        Provenance->IsCanonicalContract());
    Provenance->SetCanonicalContract();
    Provenance->MaterialSlot = TEXT("Unexpected");
    TestFalse(TEXT("Material drift is rejected"),
        Provenance->IsCanonicalContract());
    Provenance->SetCanonicalContract();
    Provenance->bCollisionAuthority = true;
    TestFalse(TEXT("Collision authority escalation is rejected"),
        Provenance->IsCanonicalContract());
    Provenance->SetCanonicalContract();
    Provenance->bLiveMapOrProviderPolicyIntegrationIncluded = true;
    TestFalse(TEXT("Live-integration claim escalation is rejected"),
        Provenance->IsCanonicalContract());
    Provenance->SetCanonicalContract();
    TestTrue(TEXT("Canonical reset is deterministic"),
        Provenance->IsCanonicalContract());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
