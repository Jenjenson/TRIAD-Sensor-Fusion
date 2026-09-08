#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TRIADIstanaExploreV5DR29VegetationActor.h"

namespace
{
bool LayoutsEqual(
    const FTRIADIstanaExploreV5DR29VegetationLayout& A,
    const FTRIADIstanaExploreV5DR29VegetationLayout& B)
{
    if (A.GrassBuckets.Num() != B.GrassBuckets.Num() ||
        A.TreeBuckets.Num() != B.TreeBuckets.Num() ||
        A.Shrubs.Num() != B.Shrubs.Num() ||
        A.Understorey.Num() != B.Understorey.Num() ||
        A.Flowers.Num() != B.Flowers.Num())
    {
        return false;
    }
    auto TransformArraysEqual = [](
        const TArray<FTransform>& First,
        const TArray<FTransform>& Second)
    {
        if (First.Num() != Second.Num())
        {
            return false;
        }
        for (int32 Index = 0; Index < First.Num(); ++Index)
        {
            if (!First[Index].Equals(Second[Index], 0.001))
            {
                return false;
            }
        }
        return true;
    };
    for (int32 Index = 0; Index < A.GrassBuckets.Num(); ++Index)
    {
        if (!TransformArraysEqual(
                A.GrassBuckets[Index].WorldTransforms,
                B.GrassBuckets[Index].WorldTransforms))
        {
            return false;
        }
    }
    for (int32 Index = 0; Index < A.TreeBuckets.Num(); ++Index)
    {
        if (!TransformArraysEqual(
                A.TreeBuckets[Index].WorldTransforms,
                B.TreeBuckets[Index].WorldTransforms))
        {
            return false;
        }
    }
    return TransformArraysEqual(A.Shrubs, B.Shrubs) &&
        TransformArraysEqual(A.Understorey, B.Understorey) &&
        TransformArraysEqual(A.Flowers, B.Flowers);
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DR29VegetationLayoutTest,
    "TRIAD.Istana.ExploreV5D.R29Vegetation.DeterministicLayout",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DR29VegetationLayoutTest::RunTest(
    const FString& Parameters)
{
    FTRIADIstanaExploreV5DR29VegetationLayout A;
    FTRIADIstanaExploreV5DR29VegetationLayout B;
    FString ErrorA;
    FString ErrorB;
    TestTrue(
        TEXT("First R29 layout builds"),
        ATRIADIstanaExploreV5DR29VegetationActor::
            BuildDeterministicLayout(A, ErrorA));
    TestTrue(
        TEXT("Second R29 layout builds"),
        ATRIADIstanaExploreV5DR29VegetationActor::
            BuildDeterministicLayout(B, ErrorB));
    TestTrue(TEXT("R29 layout is exactly replayable"), LayoutsEqual(A, B));
    TestEqual(
        TEXT("R29 grass census"),
        A.GrassTotal(),
        ATRIADIstanaExploreV5DR29VegetationActor::
            ExpectedGrassInstanceCount());
    TestEqual(
        TEXT("R29 grass bucket roster"),
        A.GrassBuckets.Num(),
        ATRIADIstanaExploreV5DR29VegetationActor::
            ExpectedGrassBucketCount());
    TestEqual(
        TEXT("R29 tree census"),
        A.TreeTotal(),
        ATRIADIstanaExploreV5DR29VegetationActor::
            ExpectedTreeInstanceCount());
    TestEqual(
        TEXT("R29 tree morphology roster"),
        A.TreeBuckets.Num(),
        ATRIADIstanaExploreV5DR29VegetationActor::
            ExpectedTreeMorphologyCount());
    for (int32 Index = 0; Index < A.GrassBuckets.Num(); ++Index)
    {
        TestTrue(
            *FString::Printf(TEXT("Grass bucket %d is populated"), Index),
            !A.GrassBuckets[Index].WorldTransforms.IsEmpty());
    }
    for (int32 Index = 0; Index < A.TreeBuckets.Num(); ++Index)
    {
        TestTrue(
            *FString::Printf(TEXT("Tree morphology %d is populated"), Index),
            !A.TreeBuckets[Index].WorldTransforms.IsEmpty());
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
