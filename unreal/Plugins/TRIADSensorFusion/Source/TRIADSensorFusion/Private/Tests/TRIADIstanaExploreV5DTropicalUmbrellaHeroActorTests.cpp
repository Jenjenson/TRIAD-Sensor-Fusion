#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "TRIADIstanaExploreV5DTropicalUmbrellaHeroActor.h"

namespace
{
TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor> MakeAnchors()
{
    TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor> Anchors;
    const int32 Count =
        ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
            ExpectedSourceAnchorCount();
    Anchors.Reserve(Count);
    for (int32 Ordinal = 0; Ordinal < Count; ++Ordinal)
    {
        FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor Anchor;
        Anchor.SourceInstanceKey =
            ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
                ExpectedSourceInstanceKey(Ordinal);
        Anchor.SourceDomain =
            ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
                ExpectedSourceDomain(Ordinal);
        Anchor.SourceIndex =
            ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
                ExpectedSourceIndex(Ordinal);
        Anchor.SourceWorldTransform = FTransform(
            FQuat(FVector::UpVector, 0.07 * (Ordinal + 1)),
            FVector(
                1000.125 + Ordinal * 73.25,
                -800.5 + Ordinal * 31.75,
                42.0 + Ordinal),
            FVector(
                0.85 + Ordinal * 0.01,
                0.90 + Ordinal * 0.02,
                0.95 + Ordinal * 0.03));
        Anchors.Add(Anchor);
    }
    return Anchors;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaTropicalUmbrellaDormantDefaultsTest,
    "TRIAD.Istana.ExploreV5D.TropicalUmbrellaHero.DormantDefaults",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaTropicalUmbrellaDormantDefaultsTest::RunTest(
    const FString& Parameters)
{
    const ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor* Defaults =
        GetDefault<ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor>();
    TestNotNull(TEXT("Dormant actor CDO exists"), Defaults);
    TestFalse(
        TEXT("Runtime selection is independently compiled false"),
        ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
            RuntimeSelectionCompiled());
    TestFalse(
        TEXT("Runtime activation is independently compiled false"),
        ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
            RuntimeActivationCompiled());
    TestFalse(
        TEXT("Distinct runtime trust anchors remain invalid"),
        ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
            RuntimeTrustAnchorsConfigured());
    FString Report;
    TestTrue(
        TEXT("CDO is an empty hidden render-only scaffold"),
        Defaults && Defaults->ValidateDormantScaffold(Report));
    TestTrue(
        TEXT("Dormant report preserves authority boundary"),
        Report.Contains(TEXT("newPlacements=0")) &&
            Report.Contains(TEXT("rfAuthority=false")) &&
            Report.Contains(TEXT("fallbackPreserved=true")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaTropicalUmbrellaExactSelectorTest,
    "TRIAD.Istana.ExploreV5D.TropicalUmbrellaHero.ExactSelectorAndTransformCopy",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaTropicalUmbrellaExactSelectorTest::RunTest(
    const FString& Parameters)
{
    const TCHAR ExpectedSelectors[] = {
        TEXT('C'), TEXT('B'), TEXT('B'), TEXT('B'), TEXT('A'), TEXT('B')};
    const TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor> Anchors =
        MakeAnchors();
    FTRIADIstanaExploreV5DTropicalUmbrellaLayout Layout;
    FString Error;
    TestTrue(
        TEXT("Six exact existing identities route deterministically"),
        ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
            BuildDeterministicLayout(Anchors, Layout, Error));
    TestEqual(TEXT("Exact six-instance census"), Layout.TotalInstances(), 6);
    TestEqual(TEXT("Variant A count"), Layout.VariantBuckets[0].WorldTransforms.Num(), 1);
    TestEqual(TEXT("Variant B count"), Layout.VariantBuckets[1].WorldTransforms.Num(), 4);
    TestEqual(TEXT("Variant C count"), Layout.VariantBuckets[2].WorldTransforms.Num(), 1);
    for (int32 Ordinal = 0; Ordinal < Anchors.Num(); ++Ordinal)
    {
        TestEqual(
            TEXT("Source identity remains ordered"),
            Layout.OrderedSourceInstanceKeys[Ordinal],
            Anchors[Ordinal].SourceInstanceKey);
        TestEqual(
            TEXT("Selector exactly mirrors GV4"),
            Layout.VariantIndexBySourceOrdinal[Ordinal],
            static_cast<int32>(ExpectedSelectors[Ordinal] - TEXT('A')));
        TestTrue(
            TEXT("World FTransform value is copied bit-for-bit"),
            ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
                ExactSourceTransformBitsMatch(
                    Layout.OrderedSourceWorldTransforms[Ordinal],
                    Anchors[Ordinal].SourceWorldTransform));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaTropicalUmbrellaSelectorMutationTest,
    "TRIAD.Istana.ExploreV5D.TropicalUmbrellaHero.SelectorMutationFailsClosed",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaTropicalUmbrellaSelectorMutationTest::RunTest(
    const FString& Parameters)
{
    TArray<FTRIADIstanaExploreV5DTropicalUmbrellaSourceAnchor> Anchors =
        MakeAnchors();
    Anchors[2].SourceInstanceKey = TEXT("v4.heritage.UNAUTHORIZED");
    FTRIADIstanaExploreV5DTropicalUmbrellaLayout Layout;
    FString Error;
    TestFalse(
        TEXT("Unknown or reordered source identity is denied"),
        ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
            BuildDeterministicLayout(Anchors, Layout, Error));
    TestEqual(TEXT("Failure clears every candidate instance"), Layout.TotalInstances(), 0);
    TestTrue(TEXT("Failure identifies exact source-row drift"), Error.Contains(TEXT("source row 2")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaTropicalUmbrellaFallbackPathsTest,
    "TRIAD.Istana.ExploreV5D.TropicalUmbrellaHero.ExactFallbackAndNamespace",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaTropicalUmbrellaFallbackPathsTest::RunTest(
    const FString& Parameters)
{
    TestEqual(
        TEXT("Exact existing umbrella fallback mesh"),
        ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
            ExistingFallbackMeshObjectPath(),
        FString(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Umbrella_NearLOD0.SM_IPV5D_Tree_Umbrella_NearLOD0")));
    UStaticMesh* FallbackMesh = LoadObject<UStaticMesh>(
        nullptr,
        *ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
            ExistingFallbackMeshObjectPath());
    TestNotNull(TEXT("Exact existing fallback mesh loads"), FallbackMesh);
    for (int32 Slot = 0; Slot < 3; ++Slot)
    {
        UMaterialInterface* ExpectedMaterial = LoadObject<UMaterialInterface>(
            nullptr,
            *ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
                ExistingFallbackMaterialObjectPath(Slot));
        TestNotNull(TEXT("Exact existing fallback material loads"), ExpectedMaterial);
        TestTrue(
            TEXT("Fallback mesh slot actually binds the exact response material"),
            FallbackMesh && ExpectedMaterial &&
                FallbackMesh->GetMaterial(Slot) == ExpectedMaterial);
    }
    for (int32 Variant = 0; Variant < 3; ++Variant)
    {
        TestTrue(
            TEXT("Candidate mesh remains in isolated namespace"),
            ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
                CandidateMeshObjectPath(Variant)
                .StartsWith(TEXT("/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroIntegration/")));
    }
    FTRIADIstanaExploreV5DTropicalUmbrellaAssets EmptyAssets;
    FString Error;
    TestFalse(
        TEXT("Missing fallback and candidate roster fails closed"),
        ATRIADIstanaExploreV5DTropicalUmbrellaHeroActor::
            ValidateAssetRoster(EmptyAssets, Error));
    return true;
}

#endif
