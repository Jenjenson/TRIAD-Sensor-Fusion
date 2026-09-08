#include "TRIADIstanaExploreV5DOuterContextVegetationActor.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DOuterContextVegetationContractTest,
    "TRIAD.Istana.ExploreV5D.OuterContextVegetation.SourceContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DOuterContextVegetationContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    using FActor = ATRIADIstanaExploreV5DOuterContextVegetationActor;

    TestEqual(TEXT("Exact placement census"), FActor::ExpectedPlacementCount(), 145);
    TestEqual(TEXT("Exact variant bucket census"), FActor::ExpectedVariantMeshCount(), 12);
    TestEqual(
        TEXT("Candidate contract pin"),
        FActor::ExpectedCandidateContractSha256(),
        FString(TEXT("FEEAD01475A9A2F6105F9BEC616BA71053FCFCFFA1DEB146112589E0BC162560")));
    TestEqual(
        TEXT("Placement dataset pin"),
        FActor::ExpectedPlacementDatasetSha256(),
        FString(TEXT("60BC748B0536BFF1E7C1ADF2AFB86307A96D96E3D779D827393A8275C3717984")));
    TestEqual(
        TEXT("Attribution text"),
        FActor::ExpectedAttributionText(),
        FString(TEXT("\u00a9 OpenStreetMap contributors; https://www.openstreetmap.org/copyright")));

    TestFalse(
        TEXT("Compiled receipt trust anchors remain deliberately unset"),
        FActor::CompiledTrustAnchorsConfigured());
    TestFalse(
        TEXT("Candidate distribution gates remain deliberately unsatisfied"),
        FActor::CandidateDistributionGatesDeclaredSatisfied());
    TestFalse(
        TEXT("GooglePrimary never renders outer candidate trees"),
        FActor::CanRenderInPresentationState(
            ETRIADIstanaExploreV5DR33TerrainPresentationState::GooglePrimary));
    TestFalse(
        TEXT("CwtWarming never renders outer candidate trees"),
        FActor::CanRenderInPresentationState(
            ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtWarming));
    TestTrue(
        TEXT("CwtPresented is the first eligible future state"),
        FActor::CanRenderInPresentationState(
            ETRIADIstanaExploreV5DR33TerrainPresentationState::CwtPresented));
    TestTrue(
        TEXT("SafeLocal is an eligible future state"),
        FActor::CanRenderInPresentationState(
            ETRIADIstanaExploreV5DR33TerrainPresentationState::SafeLocal));

    FTRIADIstanaExploreV5DOuterContextVegetationExpectedPlacement First;
    FTRIADIstanaExploreV5DOuterContextVegetationExpectedPlacement Last;
    TestTrue(TEXT("First placement exists"), FActor::GetExpectedPlacement(0, First));
    TestTrue(TEXT("Last placement exists"), FActor::GetExpectedPlacement(144, Last));
    TestFalse(TEXT("Negative placement ordinal rejected"), FActor::GetExpectedPlacement(-1, First));
    TestFalse(TEXT("Past-end placement ordinal rejected"), FActor::GetExpectedPlacement(145, Last));
    TestEqual(TEXT("First key"), FString(First.PlacementKey), FString(TEXT("node/6378240212")));
    TestEqual(TEXT("First variant"), First.VariantIndex, 1);
    TestEqual(
        TEXT("First X micrometres"),
        First.XEastMicrometers,
        static_cast<int64>(33659819));
    TestEqual(
        TEXT("First Y micrometres"),
        First.YSouthMicrometers,
        static_cast<int64>(779843127));
    TestEqual(TEXT("First yaw millidegrees"), First.YawMillidegrees, 326596);
    TestEqual(TEXT("First scale millionths"), First.UniformScaleMillionths, 1021682);
    TestEqual(
        TEXT("Last key"),
        FString(Last.PlacementKey),
        FString(TEXT("way/1551564404/sample/2")));
    TestEqual(TEXT("Last variant"), Last.VariantIndex, 2);
    TestEqual(
        TEXT("Last X micrometres"),
        Last.XEastMicrometers,
        static_cast<int64>(466162837));
    TestEqual(
        TEXT("Last Y micrometres"),
        Last.YSouthMicrometers,
        static_cast<int64>(872734524));
    TestEqual(TEXT("Last yaw millidegrees"), Last.YawMillidegrees, 34926);
    TestEqual(TEXT("Last scale millionths"), Last.UniformScaleMillionths, 1002199);

    TArray<FTRIADIstanaExploreV5DOuterContextTerrainContact> Contacts;
    Contacts.Reserve(FActor::ExpectedPlacementCount());
    for (int32 Ordinal = 0; Ordinal < FActor::ExpectedPlacementCount(); ++Ordinal)
    {
        FTRIADIstanaExploreV5DOuterContextVegetationExpectedPlacement Expected;
        TestTrue(TEXT("Placement row available"), FActor::GetExpectedPlacement(Ordinal, Expected));
        FTRIADIstanaExploreV5DOuterContextTerrainContact Contact;
        Contact.PlacementKey = Expected.PlacementKey;
        Contact.ZMeters = 10.0 + static_cast<double>(Ordinal) / 1000.0;
        Contacts.Add(MoveTemp(Contact));
    }

    FTRIADIstanaExploreV5DOuterContextVegetationLayout Layout;
    FString Error;
    TestTrue(
        TEXT("Exact ordered external contacts build a deterministic layout"),
        FActor::BuildDeterministicLayout(Contacts, Layout, Error));
    TestTrue(TEXT("Exact layout emits no error"), Error.IsEmpty());
    TestEqual(TEXT("Exact layout total"), Layout.TotalInstances(), 145);
    TestEqual(TEXT("Exact layout rows"), Layout.OrderedPlacementKeys.Num(), 145);
    TestEqual(TEXT("Exact layout buckets"), Layout.VariantBuckets.Num(), 12);
    TestEqual(
        TEXT("First X remains exact"),
        Layout.XEastMicrometers[0],
        static_cast<int64>(33659819));
    TestEqual(
        TEXT("First Y remains exact"),
        Layout.YSouthMicrometers[0],
        static_cast<int64>(779843127));
    TestEqual(
        TEXT("First Z is the supplied contact"),
        Layout.TerrainContactZMeters[0],
        10.0);
    for (int32 BucketIndex = 0; BucketIndex < Layout.VariantBuckets.Num();
         ++BucketIndex)
    {
        TestTrue(
            *FString::Printf(TEXT("Bucket %d is populated"), BucketIndex),
            Layout.VariantBuckets[BucketIndex].PlacementOrdinals.Num() > 0);
    }

    UStaticMesh* ProbeMesh = LoadObject<UStaticMesh>(
        nullptr,
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    TestNotNull(TEXT("Native HISM round-trip probe mesh exists"), ProbeMesh);
    if (ProbeMesh)
    {
        UHierarchicalInstancedStaticMeshComponent* Probe =
            NewObject<UHierarchicalInstancedStaticMeshComponent>(
                GetTransientPackage());
        TestNotNull(TEXT("Native HISM round-trip probe exists"), Probe);
        if (Probe)
        {
            Probe->SetCanEverAffectNavigation(false);
            Probe->SetStaticMesh(ProbeMesh);
            const FTransform ExpectedRoundTrip(
                FRotator(0.0, 326.596, 0.0).Quaternion(),
                FVector(3365.9819, 77984.3127, 1012.3456),
                FVector(1.021682));
            TestEqual(
                TEXT("Native HISM accepts the nontrivial transform"),
                Probe->AddInstance(ExpectedRoundTrip, false),
                0);
            FTransform ActualRoundTrip;
            TestTrue(
                TEXT("Native HISM returns the nontrivial transform"),
                Probe->GetInstanceTransform(0, ActualRoundTrip, false));
            TestTrue(
                TEXT("Native matrix round trip stays within narrow tolerances"),
                FActor::NativeInstanceTransformMatches(
                    ActualRoundTrip,
                    ExpectedRoundTrip));

            FTransform MeaningfulTranslationDrift = ExpectedRoundTrip;
            MeaningfulTranslationDrift.AddToTranslation(FVector(0.1, 0.0, 0.0));
            TestFalse(
                TEXT("Meaningful translation drift is rejected"),
                FActor::NativeInstanceTransformMatches(
                    MeaningfulTranslationDrift,
                    ExpectedRoundTrip));
            FTransform MeaningfulScaleDrift = ExpectedRoundTrip;
            MeaningfulScaleDrift.SetScale3D(
                ExpectedRoundTrip.GetScale3D() + FVector(0.001));
            TestFalse(
                TEXT("Meaningful scale drift is rejected"),
                FActor::NativeInstanceTransformMatches(
                    MeaningfulScaleDrift,
                    ExpectedRoundTrip));
        }
    }

    auto Reordered = Contacts;
    Swap(Reordered[0], Reordered[1]);
    FTRIADIstanaExploreV5DOuterContextVegetationLayout RejectedLayout;
    TestFalse(
        TEXT("Reordered contacts fail closed"),
        FActor::BuildDeterministicLayout(Reordered, RejectedLayout, Error));
    TestEqual(TEXT("Rejected layout is empty"), RejectedLayout.TotalInstances(), 0);

    auto MissingZ = Contacts;
    MissingZ[0].ZMeters.Reset();
    TestFalse(
        TEXT("Missing Z fails closed"),
        FActor::BuildDeterministicLayout(MissingZ, RejectedLayout, Error));

    auto NonFiniteZ = Contacts;
    NonFiniteZ[0].ZMeters = std::numeric_limits<double>::quiet_NaN();
    TestFalse(
        TEXT("Non-finite Z fails closed"),
        FActor::BuildDeterministicLayout(NonFiniteZ, RejectedLayout, Error));

    FTRIADIstanaExploreV5DOuterContextVegetationAssets EmptyAssets;
    TestFalse(
        TEXT("Missing mandatory fallback asset roster fails closed"),
        FActor::ValidateAssetRoster(EmptyAssets, Error));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
