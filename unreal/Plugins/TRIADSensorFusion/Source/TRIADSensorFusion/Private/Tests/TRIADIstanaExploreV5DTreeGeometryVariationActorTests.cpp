#if WITH_DEV_AUTOMATION_TESTS

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "TRIADIstanaExploreV5DTreeGeometryVariationActor.h"
#include "UObject/Package.h"

namespace
{
using FForm = ETRIADIstanaExploreV5DTreeGeometryForm;
using FDomain = ETRIADIstanaExploreV5DTreeGeometrySourceDomain;

bool ExactTransform(const FTransform& A, const FTransform& B)
{
    return ATRIADIstanaExploreV5DTreeGeometryVariationActor::
        ExactSourceTransformBitsMatch(A, B);
}

FForm FirstAllowedForm(int32 Ordinal)
{
    for (int32 FormIndex = 0;
         FormIndex < static_cast<int32>(FForm::Count);
         ++FormIndex)
    {
        const FForm Form = static_cast<FForm>(FormIndex);
        if (ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                IsResolvedFormAllowed(Ordinal, Form))
        {
            return Form;
        }
    }
    return FForm::Count;
}

TArray<FTRIADIstanaExploreV5DTreeGeometrySourceAnchor> MakeExactRoster()
{
    TArray<FTRIADIstanaExploreV5DTreeGeometrySourceAnchor> Anchors;
    const int32 Count =
        ATRIADIstanaExploreV5DTreeGeometryVariationActor::
            ExpectedSourceAnchorCount();
    Anchors.Reserve(Count);
    for (int32 Ordinal = 0; Ordinal < Count; ++Ordinal)
    {
        FTRIADIstanaExploreV5DTreeGeometrySourceAnchor Anchor;
        Anchor.SourceInstanceKey =
            ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                ExpectedSourceInstanceKey(Ordinal);
        if (Ordinal < 720)
        {
            Anchor.SourceDomain = FDomain::V4Main;
            Anchor.SourceIndex = Ordinal;
            Anchor.ResolvedForm = Ordinal >= 272 && Ordinal < 504
                ? FForm::Columnar
                : static_cast<FForm>(Ordinal % 3);
        }
        else if (Ordinal < 729)
        {
            Anchor.SourceDomain = FDomain::V4Heritage;
            Anchor.SourceIndex = Ordinal - 720;
            Anchor.ResolvedForm = FirstAllowedForm(Ordinal);
        }
        else
        {
            Anchor.SourceDomain = FDomain::R29Landmark;
            Anchor.SourceIndex = Ordinal - 729;
            Anchor.ResolvedForm = FirstAllowedForm(Ordinal);
        }
        Anchor.SourceWorldTransform = FTransform(
            FQuat::Identity,
            FVector(
                static_cast<double>(Ordinal * 16),
                static_cast<double>((Ordinal % 17) * 32),
                static_cast<double>(1000 + (Ordinal % 13) * 8)),
            FVector::OneVector);
        Anchors.Add(Anchor);
    }
    return Anchors;
}

bool ExactLayoutsEqual(
    const FTRIADIstanaExploreV5DTreeGeometryVariationLayout& A,
    const FTRIADIstanaExploreV5DTreeGeometryVariationLayout& B)
{
    if (A.VariantBuckets.Num() != B.VariantBuckets.Num() ||
        A.OrderedSourceWorldTransforms.Num() !=
            B.OrderedSourceWorldTransforms.Num() ||
        A.RecipeIndexBySourceOrdinal != B.RecipeIndexBySourceOrdinal)
    {
        return false;
    }
    for (int32 Ordinal = 0;
         Ordinal < A.OrderedSourceWorldTransforms.Num();
         ++Ordinal)
    {
        if (!ExactTransform(
                A.OrderedSourceWorldTransforms[Ordinal],
                B.OrderedSourceWorldTransforms[Ordinal]))
        {
            return false;
        }
    }
    for (int32 BucketIndex = 0;
         BucketIndex < A.VariantBuckets.Num();
         ++BucketIndex)
    {
        const auto& First = A.VariantBuckets[BucketIndex];
        const auto& Second = B.VariantBuckets[BucketIndex];
        if (First.SourceOrdinals != Second.SourceOrdinals ||
            First.WorldTransforms.Num() != Second.WorldTransforms.Num())
        {
            return false;
        }
        for (int32 Index = 0; Index < First.WorldTransforms.Num(); ++Index)
        {
            if (!ExactTransform(
                    First.WorldTransforms[Index],
                    Second.WorldTransforms[Index]))
            {
                return false;
            }
        }
    }
    return true;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DTreeGeometryVariationLayoutTest,
    "TRIAD.Istana.ExploreV5D.TreeGeometryVariation.ExactSelectorLayout",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DTreeGeometryVariationLayoutTest::RunTest(
    const FString& Parameters)
{
    const auto Anchors = MakeExactRoster();
    FTRIADIstanaExploreV5DTreeGeometryVariationLayout First;
    FTRIADIstanaExploreV5DTreeGeometryVariationLayout Second;
    FString Error;
    TestEqual(TEXT("Exact source anchor census"), Anchors.Num(), 736);
    TestTrue(
        TEXT("First exact selector layout builds"),
        ATRIADIstanaExploreV5DTreeGeometryVariationActor::
            BuildDeterministicLayout(Anchors, First, Error));
    TestTrue(
        TEXT("Second exact selector layout builds"),
        ATRIADIstanaExploreV5DTreeGeometryVariationActor::
            BuildDeterministicLayout(Anchors, Second, Error));
    TestEqual(TEXT("Exact variant bucket census"), First.VariantBuckets.Num(), 15);
    TestEqual(TEXT("Exact routed instance census"), First.TotalInstances(), 736);
    TestTrue(TEXT("Layout replays bit-exactly"), ExactLayoutsEqual(First, Second));

    int32 SelectorCounts[3] = {0, 0, 0};
    bool bEveryTransformExact = true;
    bool bEveryOrdinalRoutedOnce = true;
    TBitArray<> Seen(false, Anchors.Num());
    for (int32 Ordinal = 0; Ordinal < Anchors.Num(); ++Ordinal)
    {
        const TCHAR Selector =
            ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                ExpectedSelector(Ordinal);
        const int32 SelectorIndex = Selector - TEXT('A');
        if (SelectorIndex >= 0 && SelectorIndex < 3)
        {
            ++SelectorCounts[SelectorIndex];
        }
        else
        {
            bEveryOrdinalRoutedOnce = false;
        }
        bEveryTransformExact = bEveryTransformExact && ExactTransform(
            Anchors[Ordinal].SourceWorldTransform,
            First.OrderedSourceWorldTransforms[Ordinal]);
    }
    for (const auto& Bucket : First.VariantBuckets)
    {
        for (const int32 Ordinal : Bucket.SourceOrdinals)
        {
            if (!Seen.IsValidIndex(Ordinal) || Seen[Ordinal])
            {
                bEveryOrdinalRoutedOnce = false;
            }
            else
            {
                Seen[Ordinal] = true;
            }
        }
    }
    bEveryOrdinalRoutedOnce =
        bEveryOrdinalRoutedOnce && !Seen.Contains(false);
    TestEqual(TEXT("Selector A count"), SelectorCounts[0], 223);
    TestEqual(TEXT("Selector B count"), SelectorCounts[1], 266);
    TestEqual(TEXT("Selector C count"), SelectorCounts[2], 247);
    TestTrue(TEXT("Every saved source transform is exact"), bEveryTransformExact);
    TestTrue(TEXT("Every ordinal routes exactly once"), bEveryOrdinalRoutedOnce);
    TestFalse(
        TEXT("Runtime compiled trust anchors remain deliberately unset"),
        ATRIADIstanaExploreV5DTreeGeometryVariationActor::
            RuntimeCompiledTrustAnchorsConfigured());

    const FQuat ExactProbeRotation = FRotator(7.0, 287.341, -3.0).Quaternion();
    const FTransform ExactProbe(
        ExactProbeRotation,
        FVector(88416.2734, -73328.6172, 0.0),
        FVector(1.073125, 0.982375, 1.041625));
    const FTransform SignFlippedProbe(
        FQuat(
            -ExactProbeRotation.X,
            -ExactProbeRotation.Y,
            -ExactProbeRotation.Z,
            -ExactProbeRotation.W),
        ExactProbe.GetTranslation(),
        ExactProbe.GetScale3D());
    TestFalse(
        TEXT("Bit-exact source comparison rejects a sign-flipped quaternion"),
        ATRIADIstanaExploreV5DTreeGeometryVariationActor::
            ExactSourceTransformBitsMatch(SignFlippedProbe, ExactProbe));
    const FTransform SignedZeroProbe(
        ExactProbeRotation,
        FVector(88416.2734, -73328.6172, -0.0),
        ExactProbe.GetScale3D());
    TestFalse(
        TEXT("Bit-exact source comparison rejects signed-zero drift"),
        ATRIADIstanaExploreV5DTreeGeometryVariationActor::
            ExactSourceTransformBitsMatch(SignedZeroProbe, ExactProbe));

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
                FRotator(0.0, 287.341, 0.0).Quaternion(),
                FVector(88416.2734, -73328.6172, 2941.1250),
                FVector(1.073125, 0.982375, 1.041625));
            TestEqual(
                TEXT("Native HISM accepts the nontrivial world transform"),
                Probe->AddInstance(ExpectedRoundTrip, true),
                0);
            FTransform ActualRoundTrip;
            TestTrue(
                TEXT("Native HISM returns the nontrivial world transform"),
                Probe->GetInstanceTransform(0, ActualRoundTrip, true));
            TestTrue(
                TEXT("Native matrix round trip stays within narrow tolerances"),
                ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                    NativeInstanceTransformMatches(
                        ActualRoundTrip,
                        ExpectedRoundTrip));

            FTransform MeaningfulTranslationDrift = ExpectedRoundTrip;
            MeaningfulTranslationDrift.AddToTranslation(
                FVector(0.1, 0.0, 0.0));
            TestFalse(
                TEXT("Meaningful translation drift is rejected"),
                ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                    NativeInstanceTransformMatches(
                        MeaningfulTranslationDrift,
                        ExpectedRoundTrip));
            FTransform MeaningfulScaleDrift = ExpectedRoundTrip;
            MeaningfulScaleDrift.SetScale3D(
                ExpectedRoundTrip.GetScale3D() + FVector(0.001));
            TestFalse(
                TEXT("Meaningful scale drift is rejected"),
                ATRIADIstanaExploreV5DTreeGeometryVariationActor::
                    NativeInstanceTransformMatches(
                        MeaningfulScaleDrift,
                        ExpectedRoundTrip));
        }
    }

    auto Tampered = Anchors;
    Tampered[0].SourceInstanceKey = TEXT("v4.main.001");
    FTRIADIstanaExploreV5DTreeGeometryVariationLayout Rejected;
    TestFalse(
        TEXT("Manifest-key drift fails closed"),
        ATRIADIstanaExploreV5DTreeGeometryVariationActor::
            BuildDeterministicLayout(Tampered, Rejected, Error));
    TestEqual(TEXT("Rejected layout is cleared"), Rejected.TotalInstances(), 0);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
