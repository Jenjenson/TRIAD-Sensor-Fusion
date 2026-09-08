#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TRIADIstanaExploreV5DR32MediumDistanceTurfActor.h"

namespace
{
void MakeProfile(int32 Count, int32 Profile, TArray<FTransform>& OutTransforms)
{
    OutTransforms.Reset(Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FVector Location(
            Profile * 100000.0 + Index * 11.0,
            Profile * 10000.0 + Index * 7.0,
            125.0 + Profile);
        const FRotator Rotation(
            (Index % 5) * 0.25,
            (Index * 13 + Profile * 17) % 360,
            (Index % 7) * -0.2);
        const FVector Scale(
            0.82 + (Index % 9) * 0.01,
            0.85 + (Index % 7) * 0.01,
            0.9);
        OutTransforms.Emplace(Rotation, Location, Scale);
    }
}

bool LayoutsEqual(
    const FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout& A,
    const FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout& B)
{
    if (A.GrassBuckets.Num() != B.GrassBuckets.Num())
    {
        return false;
    }
    for (int32 Bucket = 0; Bucket < A.GrassBuckets.Num(); ++Bucket)
    {
        const TArray<FTransform>& First =
            A.GrassBuckets[Bucket].WorldTransforms;
        const TArray<FTransform>& Second =
            B.GrassBuckets[Bucket].WorldTransforms;
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
    }
    return true;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DR32MediumDistanceTurfLayoutTest,
    "TRIAD.Istana.ExploreV5D.R32MediumDistanceTurf.DeterministicLayout",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DR32MediumDistanceTurfLayoutTest::RunTest(
    const FString& Parameters)
{
    TArray<FTransform> Profiles[4];
    const int32 SourceCounts[] = {12460, 3976, 1152, 844};
    for (int32 Profile = 0; Profile < 4; ++Profile)
    {
        MakeProfile(SourceCounts[Profile], Profile, Profiles[Profile]);
    }

    FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout First;
    FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout Second;
    FString FirstError;
    FString SecondError;
    TestTrue(
        TEXT("First R32 turf layout builds"),
        ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
            BuildDeterministicLayout(
                Profiles[0],
                Profiles[1],
                Profiles[2],
                Profiles[3],
                First,
                FirstError));
    TestTrue(
        TEXT("Second R32 turf layout builds"),
        ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
            BuildDeterministicLayout(
                Profiles[0],
                Profiles[1],
                Profiles[2],
                Profiles[3],
                Second,
                SecondError));
    TestTrue(TEXT("R32 turf layout is exactly replayable"), LayoutsEqual(First, Second));
    TestEqual(
        TEXT("R32 exact total"),
        First.Total(),
        ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
            ExpectedInstanceCount());
    TestEqual(
        TEXT("R32 exact bucket roster"),
        First.GrassBuckets.Num(),
        ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
            ExpectedBucketCount());

    const int32 ExpectedProfileVariantQuotas[4][3] = {
        {1560, 933, 622},
        {499, 297, 198},
        {145, 87, 56},
        {106, 63, 42}};
    for (int32 Profile = 0; Profile < 4; ++Profile)
    {
        TMap<FIntVector, const FTransform*> PresentedByLocation;
        int32 ProfileTotal = 0;
        int32 SelectedOldModuloCount = 0;
        int32 ChangedRotationCount = 0;
        for (int32 Variant = 0; Variant < 3; ++Variant)
        {
            const TArray<FTransform>& Bucket =
                First.GrassBuckets[Profile * 3 + Variant].WorldTransforms;
            TestEqual(
                *FString::Printf(
                    TEXT("Profile %d variant %d retains exact hash quota"),
                    Profile,
                    Variant),
                Bucket.Num(),
                ExpectedProfileVariantQuotas[Profile][Variant]);
            ProfileTotal += Bucket.Num();
            for (const FTransform& Transform : Bucket)
            {
                const FVector Location = Transform.GetTranslation();
                PresentedByLocation.Add(
                    FIntVector(
                        FMath::RoundToInt(Location.X),
                        FMath::RoundToInt(Location.Y),
                        FMath::RoundToInt(Location.Z)),
                    &Transform);
            }
        }
        TestEqual(
            *FString::Printf(TEXT("Profile %d exact quota"), Profile),
            ProfileTotal,
            ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
                ExpectedProfileQuota(Profile));

        for (int32 SourceIndex = 0;
             SourceIndex < Profiles[Profile].Num();
             ++SourceIndex)
        {
            const FTransform& Source = Profiles[Profile][SourceIndex];
            const FVector Location = Source.GetTranslation();
            const FIntVector Key(
                FMath::RoundToInt(Location.X),
                FMath::RoundToInt(Location.Y),
                FMath::RoundToInt(Location.Z));
            const FTransform* const* Presented = PresentedByLocation.Find(Key);
            if (Presented)
            {
                if (SourceIndex % 4 == 0)
                {
                    ++SelectedOldModuloCount;
                }
                if (!(*Presented)->GetRotation().Equals(
                        Source.GetRotation(), 0.000001))
                {
                    ++ChangedRotationCount;
                }
            }
        }
        TestTrue(
            *FString::Printf(
                TEXT("Profile %d hash selection is not every-fourth"),
                Profile),
            SelectedOldModuloCount > 0 &&
                SelectedOldModuloCount < ProfileTotal);
        TestTrue(
            *FString::Printf(
                TEXT("Profile %d receives widespread deterministic yaw breakup"),
                Profile),
            ChangedRotationCount > ProfileTotal * 9 / 10);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5DR32MediumDistanceTurfRejectsDriftTest,
    "TRIAD.Istana.ExploreV5D.R32MediumDistanceTurf.RejectsSourceCensusDrift",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5DR32MediumDistanceTurfRejectsDriftTest::RunTest(
    const FString& Parameters)
{
    TArray<FTransform> Profiles[4];
    MakeProfile(12459, 0, Profiles[0]);
    MakeProfile(3976, 1, Profiles[1]);
    MakeProfile(1152, 2, Profiles[2]);
    MakeProfile(844, 3, Profiles[3]);
    FTRIADIstanaExploreV5DR32MediumDistanceTurfLayout Layout;
    FString Error;
    TestFalse(
        TEXT("R32 rejects a one-transform source census drift"),
        ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::
            BuildDeterministicLayout(
                Profiles[0],
                Profiles[1],
                Profiles[2],
                Profiles[3],
                Layout,
                Error));
    TestTrue(TEXT("Rejected R32 layout is empty"), Layout.Total() == 0);
    TestFalse(TEXT("Rejected R32 layout explains the failure"), Error.IsEmpty());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
