#if WITH_DEV_AUTOMATION_TESTS

#include "TRIADIstanaExploreV5BVisualActor.h"
#include "TRIADIstanaExploreV5GameMode.h"
#include "TRIADIstanaPublicViewRuntimePolicyActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
bool SelectLegacyCanonicalMaximinCandidateIndices(
    const TArray<FVector2D>& CanonicalCandidateRoots,
    int32 SelectionCount,
    TArray<int32>& OutBestSelection,
    double& OutBestMinimumDistanceSquared)
{
    OutBestSelection.Reset();
    OutBestMinimumDistanceSquared = -1.0;
    if (SelectionCount <= 0 ||
        CanonicalCandidateRoots.Num() < SelectionCount)
    {
        return false;
    }

    for (int32 SeedIndex = 0;
         SeedIndex < CanonicalCandidateRoots.Num();
         ++SeedIndex)
    {
        TArray<int32> TrialSelection;
        TrialSelection.Reserve(SelectionCount);
        TArray<bool> bChosen;
        bChosen.Init(false, CanonicalCandidateRoots.Num());
        TrialSelection.Add(SeedIndex);
        bChosen[SeedIndex] = true;
        double TrialMinimumDistanceSquared =
            TNumericLimits<double>::Max();

        while (TrialSelection.Num() < SelectionCount)
        {
            int32 FarthestIndex = INDEX_NONE;
            double FarthestNearestDistanceSquared = -1.0;
            for (int32 CandidateIndex = 0;
                 CandidateIndex < CanonicalCandidateRoots.Num();
                 ++CandidateIndex)
            {
                if (bChosen[CandidateIndex])
                {
                    continue;
                }
                double NearestDistanceSquared =
                    TNumericLimits<double>::Max();
                for (const int32 ExistingIndex : TrialSelection)
                {
                    NearestDistanceSquared = FMath::Min(
                        NearestDistanceSquared,
                        FVector2D(
                            CanonicalCandidateRoots[CandidateIndex].X -
                                CanonicalCandidateRoots[ExistingIndex].X,
                            CanonicalCandidateRoots[CandidateIndex].Y -
                                CanonicalCandidateRoots[ExistingIndex].Y).
                            SizeSquared());
                }
                if (NearestDistanceSquared >
                    FarthestNearestDistanceSquared)
                {
                    FarthestIndex = CandidateIndex;
                    FarthestNearestDistanceSquared =
                        NearestDistanceSquared;
                }
            }
            if (FarthestIndex == INDEX_NONE)
            {
                OutBestSelection.Reset();
                OutBestMinimumDistanceSquared = -1.0;
                return false;
            }
            TrialSelection.Add(FarthestIndex);
            bChosen[FarthestIndex] = true;
            TrialMinimumDistanceSquared = FMath::Min(
                TrialMinimumDistanceSquared,
                FarthestNearestDistanceSquared);
        }

        if (TrialMinimumDistanceSquared >
            OutBestMinimumDistanceSquared)
        {
            OutBestMinimumDistanceSquared = TrialMinimumDistanceSquared;
            OutBestSelection = MoveTemp(TrialSelection);
        }
    }
    return OutBestSelection.Num() == SelectionCount;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5BMaximinRegressionTest,
    "TRIAD.Istana.ExploreV5B.MaximinExactOutput",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5BMaximinRegressionTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    const auto CompareWithLegacy = [this](
        const FString& Label,
        const TArray<FVector2D>& CanonicalCandidateRoots,
        int32 SelectionCount,
        const TArray<int32>* ExpectedSelection = nullptr)
    {
        TArray<int32> LegacySelection;
        TArray<int32> OptimizedSelection;
        double LegacyMinimumDistanceSquared = -1.0;
        double OptimizedMinimumDistanceSquared = -1.0;
        const bool bLegacySelected =
            SelectLegacyCanonicalMaximinCandidateIndices(
                CanonicalCandidateRoots,
                SelectionCount,
                LegacySelection,
                LegacyMinimumDistanceSquared);
        const bool bOptimizedSelected =
            ATRIADIstanaExploreV5BVisualActor::
                SelectCanonicalMaximinCandidateIndices(
                    CanonicalCandidateRoots,
                    SelectionCount,
                    OptimizedSelection,
                    OptimizedMinimumDistanceSquared);

        TestTrue(Label + TEXT(" legacy selector succeeds"), bLegacySelected);
        TestEqual(
            Label + TEXT(" legacy and optimized success agree"),
            bOptimizedSelected,
            bLegacySelected);
        TestTrue(
            Label + TEXT(" ordered selection is byte-identical"),
            OptimizedSelection == LegacySelection);
        TestTrue(
            Label + TEXT(" minimum squared distance is bit-identical"),
            OptimizedMinimumDistanceSquared ==
                LegacyMinimumDistanceSquared);
        if (ExpectedSelection)
        {
            TestTrue(
                Label + TEXT(" preserves the expected canonical tie order"),
                OptimizedSelection == *ExpectedSelection);
        }
    };

    TArray<FVector2D> RepresentativeRoots;
    RepresentativeRoots.Reserve(64);
    for (int32 Index = 0; Index < 64; ++Index)
    {
        RepresentativeRoots.Emplace(
            static_cast<double>((Index * 37) % 101) * 13.0 +
                static_cast<double>(Index / 7),
            static_cast<double>((Index * 53) % 103) * 11.0 -
                static_cast<double>(Index % 5) * 0.25);
    }
    CompareWithLegacy(
        TEXT("Representative irregular roster"),
        RepresentativeRoots,
        12);

    const TArray<FVector2D> SquareTieRoots = {
        FVector2D(-1.0, -1.0),
        FVector2D(1.0, -1.0),
        FVector2D(1.0, 1.0),
        FVector2D(-1.0, 1.0)};
    const TArray<int32> ExpectedSquareTieSelection = {0, 2, 1, 3};
    CompareWithLegacy(
        TEXT("Symmetric square tie"),
        SquareTieRoots,
        4,
        &ExpectedSquareTieSelection);

    const TArray<FVector2D> CollinearTieRoots = {
        FVector2D(0.0, 0.0),
        FVector2D(1.0, 0.0),
        FVector2D(-1.0, 0.0)};
    const TArray<int32> ExpectedCollinearTieSelection = {0, 1, 2};
    CompareWithLegacy(
        TEXT("Equal-distance candidate tie"),
        CollinearTieRoots,
        3,
        &ExpectedCollinearTieSelection);

    const TArray<int32> ExpectedSingleSelection = {0};
    CompareWithLegacy(
        TEXT("Single-selection boundary"),
        RepresentativeRoots,
        1,
        &ExpectedSingleSelection);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV5BRuntimeVisualContractTest,
    "TRIAD.Istana.ExploreV5B.RuntimeVisualContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV5BRuntimeVisualContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    const FString ExactV5BGameModeClassPath(
        TEXT("/Script/TRIADSensorFusion.TRIADIstanaExploreV5GameMode"));
    TestEqual(
        TEXT("Exact V5B GameMode class path remains load-stable"),
        ATRIADIstanaExploreV5GameMode::StaticClass()->GetPathName(),
        ExactV5BGameModeClassPath);
    const ATRIADIstanaPublicViewRuntimePolicyActor* RuntimePolicyDefaults =
        GetDefault<ATRIADIstanaPublicViewRuntimePolicyActor>();
    TestNotNull(
        TEXT("Runtime-policy class default object remains available"),
        RuntimePolicyDefaults);
    if (RuntimePolicyDefaults)
    {
        TestTrue(
            TEXT("Runtime policy supports an exact required GameMode"),
            RuntimePolicyDefaults->bRequireIstanaAirSimGameMode);
        TestTrue(
            TEXT("Runtime policy class default keeps the public-view fixed camera; Explore maps must serialize their explicit false override"),
            RuntimePolicyDefaults->bEnforceFixedPrimaryCamera);
        TestFalse(
            TEXT("Runtime policy is not pre-settled on its class default"),
            RuntimePolicyDefaults->bRuntimePolicySettledAtRuntime);
        TestFalse(
            TEXT("GameMode verification is runtime-derived, not pre-seeded"),
            RuntimePolicyDefaults->bGameModeOverrideVerifiedAtRuntime);
    }

    const ATRIADIstanaExploreV5BVisualActor* Defaults =
        GetDefault<ATRIADIstanaExploreV5BVisualActor>();
    TestNotNull(TEXT("V5B visual actor class default object"), Defaults);
    if (Defaults)
    {
        TestEqual(
            TEXT("Exact V5B truth label"),
            Defaults->ClaimLabel,
            ATRIADIstanaExploreV5BVisualActor::ExpectedClaimLabel());
        TestTrue(TEXT("V5B is appearance-only"), Defaults->bAppearanceOnly);
        TestTrue(TEXT("V5B geometry is render-only"), Defaults->bRenderOnlyGeometry);
        TestFalse(
            TEXT("V5B has no collision/navigation authority"),
            Defaults->bCollisionOrNavigationAuthority);
        TestFalse(
            TEXT("V5B has no sensor/RF material authority"),
            Defaults->bSensorOrRfMaterialAuthority);
        TestFalse(
            TEXT("V5B makes no survey/as-built claim"),
            Defaults->bSurveyOrAsBuiltClaimed);
        TestFalse(
            TEXT("V5B makes no botanical-inventory claim"),
            Defaults->bBotanicalInventoryClaimed);
        TestTrue(
            TEXT("Pachira asset is explicitly a synthetic morphology proxy"),
            Defaults->bPachiraUsedOnlyAsSyntheticMorphologyProxy);
        TestFalse(
            TEXT("Synthetic bed positions are not claimed observed"),
            Defaults->bSyntheticBedLocationsClaimedObserved);
        TestFalse(
            TEXT("Fountain envelopes are not a hydraulic simulation"),
            Defaults->bFountainHydraulicSimulationClaimed);
        TestFalse(
            TEXT("No Google or OneMap content is admitted"),
            Defaults->bGoogleOrOneMapContentUsed);
        TestEqual(
            TEXT("Exact non-primitive root"),
            Defaults->GetRootComponent(),
            Defaults->SceneRoot.Get());
        TestEqual(
            TEXT("Actor owns root, exact 24 primitives, and one facade fill"),
            Defaults->GetComponents().Num(),
            26);

        TestNotNull(
            TEXT("Facade fill component exists"),
            Defaults->FacadeFillLightComponent.Get());
        if (Defaults->FacadeFillLightComponent)
        {
            TestEqual(TEXT("Facade fill is 2,500 lux"), Defaults->FacadeFillLightComponent->Intensity, 2500.0f);
            TestFalse(TEXT("Cold facade fill is disabled"), Defaults->FacadeFillLightComponent->IsVisible());
            TestFalse(TEXT("Facade fill casts no shadows"), Defaults->FacadeFillLightComponent->CastShadows);
            TestFalse(TEXT("Facade fill casts no volumetric shadow"), Defaults->FacadeFillLightComponent->bCastVolumetricShadow);
            TestFalse(TEXT("Facade fill affects no reflections"), Defaults->FacadeFillLightComponent->bAffectReflection);
            TestFalse(TEXT("Facade fill affects no GI"), Defaults->FacadeFillLightComponent->bAffectGlobalIllumination);
            TestEqual(TEXT("Facade fill has zero indirect intensity"), Defaults->FacadeFillLightComponent->IndirectLightingIntensity, 0.0f);
            TestEqual(TEXT("Facade fill affects no volumetrics"), Defaults->FacadeFillLightComponent->VolumetricScatteringIntensity, 0.0f);
            TestFalse(TEXT("Facade fill excludes channel 0"), Defaults->FacadeFillLightComponent->LightingChannels.bChannel0);
            TestTrue(TEXT("Facade fill uses channel 1"), Defaults->FacadeFillLightComponent->LightingChannels.bChannel1);
            TestFalse(TEXT("Facade fill excludes channel 2"), Defaults->FacadeFillLightComponent->LightingChannels.bChannel2);
        }

        TInlineComponentArray<UPrimitiveComponent*> Primitives(Defaults);
        TestEqual(TEXT("Exact V5B primitive census"), Primitives.Num(), 24);
        TSet<FName> PrimitiveNames;
        for (const UPrimitiveComponent* Primitive : Primitives)
        {
            TestNotNull(TEXT("V5B primitive exists"), Primitive);
            if (!Primitive)
            {
                continue;
            }
            PrimitiveNames.Add(Primitive->GetFName());
            TestEqual(
                *FString::Printf(
                    TEXT("%s is static"),
                    *Primitive->GetName()),
                Primitive->Mobility,
                EComponentMobility::Static);
            TestEqual(
                *FString::Printf(
                    TEXT("%s has no collision"),
                    *Primitive->GetName()),
                Primitive->GetCollisionEnabled(),
                ECollisionEnabled::NoCollision);
            TestFalse(
                *FString::Printf(
                    TEXT("%s generates no overlaps"),
                    *Primitive->GetName()),
                Primitive->GetGenerateOverlapEvents());
            TestFalse(
                *FString::Printf(
                    TEXT("%s never affects navigation"),
                    *Primitive->GetName()),
                Primitive->CanEverAffectNavigation());
            TestTrue(
                *FString::Printf(
                    TEXT("%s ignores every collision channel"),
                    *Primitive->GetName()),
                Primitive->GetCollisionResponseToChannels() ==
                    FCollisionResponseContainer(ECR_Ignore));
            if (const UHierarchicalInstancedStaticMeshComponent* Hism =
                    Cast<UHierarchicalInstancedStaticMeshComponent>(Primitive))
            {
                TestEqual(
                    *FString::Printf(
                        TEXT("Unconfigured %s has no instances"),
                        *Primitive->GetName()),
                    Hism->GetInstanceCount(),
                    0);
                TestFalse(
                    *FString::Printf(
                        TEXT("%s cannot density-scale away"),
                        *Primitive->GetName()),
                    Hism->bEnableDensityScaling);
            }
        }
        const TArray<FName> ExpectedPrimitiveNames = {
            FName(TEXT("V5BHardscapeNoLegacyBedsRenderSuccessor")),
            FName(TEXT("V5BAccentTurfInstances")),
            FName(TEXT("V5BFormalBedsOrganicVeneer")),
            FName(TEXT("V5BFormalBedShrubCorrections")),
            FName(TEXT("V5BFormalBedFlowerCorrections")),
            FName(TEXT("V5BFormalBedUnderstoreyCorrections")),
            FName(TEXT("V5BTreeBaseMulchInstances")),
            FName(TEXT("V5BTreeBaseShrubInstances")),
            FName(TEXT("V5BTreeBaseUnderstoreyInstances")),
            FName(TEXT("V5BFountainSurface")),
            FName(TEXT("V5BFountainEdgeFoam")),
            FName(TEXT("V5BOuterPlumeInstances")),
            FName(TEXT("V5BImpactRingInstances")),
            FName(TEXT("V5BCentralPlume")),
            FName(TEXT("V5BInnerPaverInstances")),
            FName(TEXT("V5BOuterPaverInstances")),
            FName(TEXT("V5BPachiraBarkAInstances")),
            FName(TEXT("V5BPachiraBarkBInstances")),
            FName(TEXT("V5BPachiraBarkCInstances")),
            FName(TEXT("V5BPachiraBarkDInstances")),
            FName(TEXT("V5BPachiraLeavesAInstances")),
            FName(TEXT("V5BPachiraLeavesBInstances")),
            FName(TEXT("V5BPachiraLeavesCInstances")),
            FName(TEXT("V5BPachiraLeavesDInstances"))};
        for (const FName ExpectedName : ExpectedPrimitiveNames)
        {
            TestTrue(
                *FString::Printf(
                    TEXT("Exact primitive '%s' exists"),
                    *ExpectedName.ToString()),
                PrimitiveNames.Contains(ExpectedName));
        }

        int32 AccentStartCull = 0;
        int32 AccentEndCull = 0;
        Defaults->AccentTurfInstances->GetCullDistances(
            AccentStartCull,
            AccentEndCull);
        TestEqual(TEXT("Accent turf starts fading at 3,000 cm"), AccentStartCull, 3000);
        TestEqual(TEXT("Accent turf ends fading at 5,200 cm"), AccentEndCull, 5200);
        TestEqual(
            TEXT("Accent turf WPO disables at 3,200 cm"),
            Defaults->AccentTurfInstances->WorldPositionOffsetDisableDistance,
            3200);
        TestEqual(
            TEXT("Only tree-base shrubs force zero-based source LOD1"),
            Defaults->TreeBaseShrubInstances->ForcedLodModel,
            1);
        TestEqual(
            TEXT("Only tree-base understorey forces zero-based source LOD1"),
            Defaults->TreeBaseUnderstoreyInstances->ForcedLodModel,
            1);
        TestEqual(
            TEXT("Formal-bed shrubs retain automatic LOD selection"),
            Defaults->FormalBedShrubCorrections->ForcedLodModel,
            0);
        TestEqual(
            TEXT("Formal-bed understorey retains automatic LOD selection"),
            Defaults->FormalBedUnderstoreyCorrections->ForcedLodModel,
            0);

        FString UnconfiguredReport;
        TestFalse(
            TEXT("Unconfigured V5B actor fails closed"),
            Defaults->ValidateExploreV5BVisuals(UnconfiguredReport));
        TestTrue(
            TEXT("Unconfigured V5B failure has exact prefix"),
            UnconfiguredReport.StartsWith(
                TEXT("ISTANA_EXPLORE_V5B_VISUALS_INVALID:")));
    }

    TestEqual(
        TEXT("Exact inherited close-turf source census"),
        ATRIADIstanaExploreV5BVisualActor::ExpectedCloseTurfSourceCount(),
        18432);
    TestEqual(
        TEXT("Exact accent turf census"),
        ATRIADIstanaExploreV5BVisualActor::ExpectedAccentTurfCount(),
        18432);
    TestEqual(
        TEXT("Exact formal-bed shrub correction census"),
        ATRIADIstanaExploreV5BVisualActor::ExpectedFormalBedShrubCorrectionCount(),
        752);
    TestEqual(
        TEXT("Exact formal-bed flower correction census"),
        ATRIADIstanaExploreV5BVisualActor::ExpectedFormalBedFlowerCorrectionCount(),
        912);
    TestEqual(
        TEXT("Exact formal-bed understorey correction census"),
        ATRIADIstanaExploreV5BVisualActor::ExpectedFormalBedUnderstoreyCorrectionCount(),
        2304);
    TestEqual(
        TEXT("Exact tree-base mulch census"),
        ATRIADIstanaExploreV5BVisualActor::ExpectedTreeBaseMulchCount(),
        64);
    TestEqual(
        TEXT("Exact tree-base shrub census"),
        ATRIADIstanaExploreV5BVisualActor::ExpectedTreeBaseShrubCount(),
        48);
    TestEqual(
        TEXT("Exact tree-base understorey census"),
        ATRIADIstanaExploreV5BVisualActor::ExpectedTreeBaseUnderstoreyCount(),
        144);
    TestEqual(
        TEXT("Exact inner paver census"),
        ATRIADIstanaExploreV5BVisualActor::ExpectedInnerPaverCount(),
        384);
    TestEqual(
        TEXT("Exact outer paver census"),
        ATRIADIstanaExploreV5BVisualActor::ExpectedOuterPaverCount(),
        256);
    TestEqual(
        TEXT("Exact outer plume census"),
        ATRIADIstanaExploreV5BVisualActor::ExpectedOuterPlumeCount(),
        12);
    TestEqual(
        TEXT("Exact impact ring census"),
        ATRIADIstanaExploreV5BVisualActor::ExpectedImpactRingCount(),
        12);
    TestEqual(
        TEXT("Exact logical Pachira morphology-proxy census"),
        ATRIADIstanaExploreV5BVisualActor::ExpectedLogicalPachiraCount(),
        48);
    TestEqual(
        TEXT("Exact Pachira instances per variant"),
        ATRIADIstanaExploreV5BVisualActor::ExpectedPachiraInstancesPerVariant(),
        12);
    TestEqual(
        TEXT("First accent source index"),
        ATRIADIstanaExploreV5BVisualActor::AccentSourceIndexForOrdinal(0),
        37);
    TestEqual(
        TEXT("Out-of-range negative accent ordinal is rejected"),
        ATRIADIstanaExploreV5BVisualActor::AccentSourceIndexForOrdinal(-1),
        INDEX_NONE);
    TestEqual(
        TEXT("Out-of-range high accent ordinal is rejected"),
        ATRIADIstanaExploreV5BVisualActor::AccentSourceIndexForOrdinal(18432),
        INDEX_NONE);

    TArray<FTransform> SourceCloseTurfTransforms;
    SourceCloseTurfTransforms.Reserve(18432);
    for (int32 Index = 0; Index < 18432; ++Index)
    {
        SourceCloseTurfTransforms.Add(FTransform(
            FRotator(0.0, static_cast<double>(Index % 360), 0.0),
            FVector(
                static_cast<double>((Index % 192) * 17 - 1600),
                static_cast<double>((Index / 192) * 19 + 3000),
                static_cast<double>(Index % 11) * 0.125),
            FVector::OneVector));
    }
    FTRIADIstanaExploreV5BDeterministicLayout LayoutA;
    FString LayoutError;
    TestTrue(
        TEXT("Exact close-turf roster builds the deterministic V5B layout"),
        ATRIADIstanaExploreV5BVisualActor::BuildDeterministicLayout(
            SourceCloseTurfTransforms,
            LayoutA,
            LayoutError));
    TestTrue(TEXT("Successful layout clears error"), LayoutError.IsEmpty());

    FTRIADIstanaExploreV5BDeterministicLayout LayoutB;
    TestTrue(
        TEXT("Second build of identical input succeeds"),
        ATRIADIstanaExploreV5BVisualActor::BuildDeterministicLayout(
            SourceCloseTurfTransforms,
            LayoutB,
            LayoutError));
    TestTrue(
        TEXT("Accent source order is bitwise deterministic"),
        LayoutA.AccentTurfSourceIndices == LayoutB.AccentTurfSourceIndices);
    TestEqual(
        TEXT("Exact generated accent count"),
        LayoutA.AccentTurfWorldTransforms.Num(),
        18432);
    TestEqual(
        TEXT("Exact generated source-index count"),
        LayoutA.AccentTurfSourceIndices.Num(),
        18432);

    const auto FractionalPart = [](double Value)
    {
        return Value - FMath::FloorToDouble(Value);
    };
    const auto AnalyticTerrainHeightCm = [](double XCm, double YCm)
    {
        const double XMeters = XCm / 100.0;
        const double YMeters = YCm / 100.0;
        const double RadiusMeters = FMath::Sqrt(
            XMeters * XMeters + YMeters * YMeters);
        return 100.0 * (
            0.72 * FMath::Sin(XMeters / 185.0) +
            0.48 * FMath::Cos(YMeters / 230.0) +
            0.22 * FMath::Sin((XMeters + YMeters) / 97.0) +
            0.0000011 * RadiusMeters * RadiusMeters - 0.48);
    };
    const auto IsLawnExclusion = [](double XCm, double YCm)
    {
        return (YCm < 1400.0 && FMath::Abs(XCm) < 8200.0) ||
            (FMath::Abs(XCm) < 320.0 && YCm < 7000.0) ||
            FVector2D(XCm, YCm - 9500.0).SizeSquared() <
                FMath::Square(2500.0);
    };
    const auto IsOrganicBedSuppression = [](double XCm, double YCm)
    {
        const double BedCenterXCm = XCm < 0.0 ? -3400.0 : 3400.0;
        const double U = (XCm - BedCenterXCm) / 1610.0;
        const double V = (YCm - 8800.0) / 1110.0;
        const double Angle = FMath::Atan2(V, U);
        const double Boundary = FMath::Max(
            0.82,
            1.0 +
                0.055 * FMath::Sin(5.0 * Angle + 0.63) +
                0.025 * FMath::Cos(8.0 * Angle - 0.21) +
                0.012 * FMath::Sin(13.0 * Angle + 1.17));
        constexpr double Exponent = 2.65;
        return FMath::Pow(FMath::Abs(U), Exponent) +
                FMath::Pow(FMath::Abs(V), Exponent) <=
            FMath::Pow(Boundary, Exponent);
    };

    TSet<int32> UniqueAccentIndices;
    TSet<FVector2D> UniqueAccentXY;
    int32 LowDiscrepancyCandidateIndex = 0;
    int32 LeftAccentCount = 0;
    int32 RightAccentCount = 0;
    int32 SuppressedAccentCount = 0;
    int32 AnisotropicAccentCount = 0;
    for (int32 Ordinal = 0;
         Ordinal < LayoutA.AccentTurfWorldTransforms.Num();
         ++Ordinal)
    {
        double ExpectedX = 0.0;
        double ExpectedY = 0.0;
        do
        {
            const double SequenceIndex = static_cast<double>(
                ++LowDiscrepancyCandidateIndex);
            ExpectedX = FMath::Lerp(
                -9600.0,
                9600.0,
                FractionalPart(
                    0.5 + SequenceIndex * 0.7548776662466927));
            ExpectedY = FMath::Lerp(
                800.0,
                22000.0,
                FractionalPart(
                    0.5 + SequenceIndex * 0.5698402909980532));
        }
        while (IsLawnExclusion(ExpectedX, ExpectedY));

        const int32 SourceIndex = LayoutA.AccentTurfSourceIndices[Ordinal];
        const FTransform& Source = SourceCloseTurfTransforms[SourceIndex];
        const FTransform& Accent = LayoutA.AccentTurfWorldTransforms[Ordinal];
        const FVector AccentLocation = Accent.GetTranslation();
        UniqueAccentIndices.Add(SourceIndex);
        UniqueAccentXY.Add(FVector2D(AccentLocation.X, AccentLocation.Y));
        if (AccentLocation.X < 0.0) { ++LeftAccentCount; }
        else { ++RightAccentCount; }
        TestEqual(
            *FString::Printf(TEXT("Accent %d exact source formula"), Ordinal),
            SourceIndex,
            (101 * Ordinal + 37) % 18432);
        TestEqual(
            *FString::Printf(TEXT("Accent %d follows exact R2 X sequence"), Ordinal),
            AccentLocation.X,
            ExpectedX,
            0.000001);
        TestEqual(
            *FString::Printf(TEXT("Accent %d follows exact R2 Y sequence"), Ordinal),
            AccentLocation.Y,
            ExpectedY,
            0.000001);
        TestTrue(
            *FString::Printf(TEXT("Accent %d remains outside all lawn exclusions"), Ordinal),
            !IsLawnExclusion(AccentLocation.X, AccentLocation.Y));
        const bool bUnderFormalBed = IsOrganicBedSuppression(
            AccentLocation.X,
            AccentLocation.Y);
        if (bUnderFormalBed) { ++SuppressedAccentCount; }
        TestEqual(
            *FString::Printf(TEXT("Accent %d has analytic terrain Z and organic bed suppression"), Ordinal),
            AccentLocation.Z,
            AnalyticTerrainHeightCm(AccentLocation.X, AccentLocation.Y) + 0.15 -
                (bUnderFormalBed ? 10.0 : 0.0),
            0.000001);
        const FVector AccentScale = Accent.GetScale3D();
        if (!FMath::IsNearlyEqual(
                AccentScale.X,
                AccentScale.Y,
                0.000001))
        {
            ++AnisotropicAccentCount;
        }
        TestTrue(
            *FString::Printf(TEXT("Accent %d anisotropic XY scale is bounded"), Ordinal),
            AccentScale.X >= 0.86 && AccentScale.X <= 1.14 &&
                AccentScale.Y >= 0.82 && AccentScale.Y <= 1.18);
        TestTrue(
            *FString::Printf(TEXT("Accent %d keeps the frozen serialized-map transform denominator"), Ordinal),
            AccentScale.Z * 4.8 >= 2.0 &&
                AccentScale.Z * 4.8 <= 3.2);
        TestTrue(
            *FString::Printf(TEXT("Accent %d reports the R10 4.4 cm source-tip placement truthfully"), Ordinal),
            AccentScale.Z * 4.4 >= 1.833333 - 0.000001 &&
                AccentScale.Z * 4.4 <= 2.933333 + 0.000001);
        const double YawJitter = FRotator::NormalizeAxis(
            Accent.Rotator().Yaw - Source.Rotator().Yaw - 47.0);
        TestTrue(
            *FString::Printf(TEXT("Accent %d yaw jitter is bounded"), Ordinal),
            YawJitter >= -33.000001 && YawJitter <= 33.000001);
        TestTrue(
            *FString::Printf(TEXT("Accent %d repeats deterministically"), Ordinal),
            Accent.Equals(LayoutB.AccentTurfWorldTransforms[Ordinal], 0.0));
    }
    TestEqual(
        TEXT("All 18,432 accent source rows are unique"),
        UniqueAccentIndices.Num(),
        18432);
    TestEqual(
        TEXT("All 18,432 low-discrepancy accent XY locations are unique"),
        UniqueAccentXY.Num(),
        18432);
    TestTrue(
        TEXT("Low-discrepancy turf remains bilaterally balanced"),
        FMath::Abs(LeftAccentCount - RightAccentCount) <= 4);
    TestTrue(
        TEXT("Organic formal-bed suppression is exercised by the turf roster"),
        SuppressedAccentCount >= 300);
    TestTrue(
        TEXT("Independent scale hashes produce materially anisotropic turf"),
        AnisotropicAccentCount >= 18000);

    TestEqual(
        TEXT("Fountain surface exact X"),
        LayoutA.FountainSurfaceWorldTransform.GetTranslation().X,
        0.0,
        0.000001);
    TestEqual(
        TEXT("Fountain surface exact Y"),
        LayoutA.FountainSurfaceWorldTransform.GetTranslation().Y,
        9500.0,
        0.000001);
    TestEqual(
        TEXT("Fountain surface exact Z"),
        LayoutA.FountainSurfaceWorldTransform.GetTranslation().Z,
        ATRIADIstanaExploreV5BVisualActor::ExpectedFountainSurfaceZCm(),
        0.000001);
    TestEqual(
        TEXT("Exact outer plume transform count"),
        LayoutA.OuterPlumeWorldTransforms.Num(),
        12);
    TestEqual(
        TEXT("Exact impact-ring transform count"),
        LayoutA.ImpactRingWorldTransforms.Num(),
        12);
    for (int32 Index = 0; Index < 12; ++Index)
    {
        const FVector PlumeLocation =
            LayoutA.OuterPlumeWorldTransforms[Index].GetTranslation();
        const FVector RingLocation =
            LayoutA.ImpactRingWorldTransforms[Index].GetTranslation();
        TestEqual(
            *FString::Printf(TEXT("Outer plume %d exact radial origin"), Index),
            FVector2D(PlumeLocation.X, PlumeLocation.Y - 9500.0).Size(),
            560.0,
            0.0001);
        TestEqual(
            *FString::Printf(TEXT("Outer plume %d exact Z"), Index),
            PlumeLocation.Z,
            ATRIADIstanaExploreV5BVisualActor::ExpectedOuterPlumeOriginZCm(),
            0.000001);
        TestTrue(
            *FString::Printf(TEXT("Impact ring %d is paired with plume"), Index),
            PlumeLocation.Equals(RingLocation, 0.000001));
    }
    TestEqual(
        TEXT("Central plume exact X"),
        LayoutA.CentralPlumeWorldTransform.GetTranslation().X,
        0.0,
        0.000001);
    TestEqual(
        TEXT("Central plume exact Y"),
        LayoutA.CentralPlumeWorldTransform.GetTranslation().Y,
        9500.0,
        0.000001);
    TestEqual(
        TEXT("Central plume exact Z"),
        LayoutA.CentralPlumeWorldTransform.GetTranslation().Z,
        ATRIADIstanaExploreV5BVisualActor::ExpectedCentralPlumeOriginZCm(),
        0.000001);

    TestEqual(
        TEXT("Exact three-ring inner-mesh paver transforms"),
        LayoutA.InnerPaverWorldTransforms.Num(),
        384);
    TestEqual(
        TEXT("Exact two-ring outer-mesh paver transforms"),
        LayoutA.OuterPaverWorldTransforms.Num(),
        256);
    int32 InnerRingCounts[3] = {0, 0, 0};
    int32 OuterRingCounts[2] = {0, 0};
    const auto ValidatePaver = [this](
        const FTransform& Paver,
        const TCHAR* Label,
        int32 Index)
    {
        const FVector Location = Paver.GetTranslation();
        const double Radius = FVector2D(Location.X, Location.Y - 9500.0).Size();
        const double RadialYaw = FRotator::NormalizeAxis(
            FMath::RadiansToDegrees(FMath::Atan2(Location.Y - 9500.0, Location.X)));
        TestEqual(
            *FString::Printf(TEXT("%s paver %d faces radially"), Label, Index),
            Paver.Rotator().Yaw,
            RadialYaw,
            0.0001);
        TestTrue(
            *FString::Printf(TEXT("%s paver %d bounded Z variation"), Label, Index),
            Location.Z >= 37.7194726 && Location.Z <= 37.9194726);
        const FVector Scale = Paver.GetScale3D();
        TestTrue(
            *FString::Printf(TEXT("%s paver %d bounded per-piece scale"), Label, Index),
            Scale.X >= 0.985 && Scale.X <= 1.015 &&
                Scale.Y >= 0.99 && Scale.Y <= 1.08 &&
                FMath::IsNearlyEqual(Scale.Z, 1.0, 0.000001));
        return Radius;
    };
    for (int32 Index = 0; Index < LayoutA.InnerPaverWorldTransforms.Num(); ++Index)
    {
        const double Radius = ValidatePaver(
            LayoutA.InnerPaverWorldTransforms[Index], TEXT("Inner-mesh"), Index);
        if (FMath::IsNearlyEqual(Radius, 1065.0, 0.0001)) { ++InnerRingCounts[0]; }
        else if (FMath::IsNearlyEqual(Radius, 1179.0, 0.0001)) { ++InnerRingCounts[1]; }
        else if (FMath::IsNearlyEqual(Radius, 1293.0, 0.0001)) { ++InnerRingCounts[2]; }
        else { AddError(TEXT("Inner-mesh paver used an unadmitted radius.")); }
    }
    for (int32 Index = 0; Index < LayoutA.OuterPaverWorldTransforms.Num(); ++Index)
    {
        const double Radius = ValidatePaver(
            LayoutA.OuterPaverWorldTransforms[Index], TEXT("Outer-mesh"), Index);
        if (FMath::IsNearlyEqual(Radius, 1122.0, 0.0001)) { ++OuterRingCounts[0]; }
        else if (FMath::IsNearlyEqual(Radius, 1236.0, 0.0001)) { ++OuterRingCounts[1]; }
        else { AddError(TEXT("Outer-mesh paver used an unadmitted radius.")); }
    }
    TestEqual(TEXT("Innermost paver ring census"), InnerRingCounts[0], 112);
    TestEqual(TEXT("Middle paver ring census"), InnerRingCounts[1], 128);
    TestEqual(TEXT("Outermost paver ring census"), InnerRingCounts[2], 144);
    TestEqual(TEXT("Second paver ring census"), OuterRingCounts[0], 120);
    TestEqual(TEXT("Fourth paver ring census"), OuterRingCounts[1], 136);

    const auto SmoothStep01 = [](double Value)
    {
        const double Unit = FMath::Clamp(Value, 0.0, 1.0);
        return Unit * Unit * (3.0 - 2.0 * Unit);
    };
    const auto FormalBedReliefCm = [&SmoothStep01](double U, double V)
    {
        const double Signal =
            0.38 * FMath::Sin(7.31 * U + 5.17 * V + 0.37) +
            0.27 * FMath::Sin(15.13 * U - 9.71 * V + 1.11) +
            0.21 * FMath::Cos(24.97 * U + 17.39 * V - 0.73) +
            0.14 * FMath::Sin(39.17 * U - 31.07 * V + 2.03);
        const double Unit = FMath::Clamp(0.5 * (Signal + 1.0), 0.0, 1.0);
        const double EdgeDistance =
            1.0 - FMath::Max(FMath::Abs(U), FMath::Abs(V));
        return 4.8 * Unit * SmoothStep01(EdgeDistance / 0.24);
    };
    constexpr double FormalBedCoreDatumZCm = 39.7194726;
    TArray<FVector> LogicalPachiraLocations;
    TSet<FVector2D> UniquePachiraXY;
    TSet<int64> UniquePachiraYQuantized;
    int32 LeftPachiraCount = 0;
    int32 RightPachiraCount = 0;
    for (int32 Variant = 0; Variant < 4; ++Variant)
    {
        TestEqual(
            *FString::Printf(TEXT("Pachira variant %d exact count"), Variant),
            LayoutA.PachiraWorldTransformsByVariant[Variant].Num(),
            12);
        for (const FTransform& Plant :
             LayoutA.PachiraWorldTransformsByVariant[Variant])
        {
            const FVector Location = Plant.GetTranslation();
            LogicalPachiraLocations.Add(Location);
            UniquePachiraXY.Add(FVector2D(Location.X, Location.Y));
            UniquePachiraYQuantized.Add(FMath::RoundToInt64(
                Location.Y * 1000.0));
            if (Location.X < 0.0) { ++LeftPachiraCount; }
            else { ++RightPachiraCount; }
            TestTrue(
                TEXT("Pachira X remains inside the admitted synthetic beds"),
                FMath::Abs(Location.X) >= 2200.0 &&
                    FMath::Abs(Location.X) <= 4600.0);
            TestTrue(
                TEXT("Pachira Y remains inside the admitted synthetic beds"),
                Location.Y >= 8000.0 && Location.Y <= 9600.0);
            TestEqual(
                TEXT("Pachira is rooted 1.5 cm into the raised formal-bed mound"),
                Location.Z,
                FormalBedCoreDatumZCm + FormalBedReliefCm(
                    (3400.0 - FMath::Abs(Location.X)) / 1400.0,
                    (Location.Y - 8800.0) / 900.0) - 1.5,
                0.000001);
            TestTrue(
                TEXT("Pachira has bounded deterministic uniform variation"),
                FMath::IsNearlyEqual(
                    Plant.GetScale3D().X,
                    Plant.GetScale3D().Y,
                    0.000001) &&
                    FMath::IsNearlyEqual(
                        Plant.GetScale3D().X,
                        Plant.GetScale3D().Z,
                        0.000001) &&
                    Plant.GetScale3D().X >= 0.78 &&
                    Plant.GetScale3D().X <= 1.18);
        }
    }
    TestEqual(
        TEXT("All 48 Pachira morphology-proxy locations are unique"),
        UniquePachiraXY.Num(),
        48);
    TestEqual(
        TEXT("Natural Pachira groves have no repeated row coordinates"),
        UniquePachiraYQuantized.Num(),
        48);
    TestEqual(TEXT("Left Pachira grove census"), LeftPachiraCount, 24);
    TestEqual(TEXT("Right Pachira grove census"), RightPachiraCount, 24);
    int32 ExactMirroredPachiraCount = 0;
    for (const FVector& Location : LogicalPachiraLocations)
    {
        if (LogicalPachiraLocations.ContainsByPredicate(
                [&Location](const FVector& Candidate)
                {
                    return FMath::IsNearlyEqual(
                               Candidate.X, -Location.X, 0.000001) &&
                        FMath::IsNearlyEqual(
                            Candidate.Y, Location.Y, 0.000001);
                }))
        {
            ++ExactMirroredPachiraCount;
        }
    }
    TestEqual(
        TEXT("Natural Pachira groves contain no exact mirror clones"),
        ExactMirroredPachiraCount,
        0);

    const auto MakeFormalSourceRoster = [](int32 Count, double BaseScale)
    {
        TArray<FTransform> Result;
        Result.Reserve(Count);
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const double Sign = Index % 2 == 0 ? -1.0 : 1.0;
            const double X = Sign * (2150.0 + (Index % 23) * 105.0);
            const double Y = 8000.0 + (Index % 17) * 100.0;
            Result.Add(FTransform(
                FRotator(0.0, static_cast<double>(Index % 360), 0.0),
                FVector(X, Y, -8.0 + (Index % 9)),
                FVector(BaseScale)));
        }
        return Result;
    };
    const TArray<FTransform> FormalShrubs = MakeFormalSourceRoster(512, 5.0);
    const TArray<FTransform> FormalFlowers = MakeFormalSourceRoster(192, 3.5);
    const TArray<FTransform> FormalUnderstorey = MakeFormalSourceRoster(384, 5.5);
    TestTrue(
        TEXT("Exact full V4 planting rosters build deterministic V5B formal masses"),
        ATRIADIstanaExploreV5BVisualActor::BuildFormalBedCorrections(
            FormalShrubs,
            FormalFlowers,
            FormalUnderstorey,
            LayoutA,
            LayoutError));
    TestTrue(
        TEXT("Second full-roster build is deterministic"),
        ATRIADIstanaExploreV5BVisualActor::BuildFormalBedCorrections(
            FormalShrubs,
            FormalFlowers,
            FormalUnderstorey,
            LayoutB,
            LayoutError));
    TestEqual(
        TEXT("Exact corrected shrub count"),
        LayoutA.FormalBedShrubCorrectionWorldTransforms.Num(),
        752);
    TestEqual(
        TEXT("Exact corrected flower count"),
        LayoutA.FormalBedFlowerCorrectionWorldTransforms.Num(),
        912);
    TestEqual(
        TEXT("Exact corrected understorey count"),
        LayoutA.FormalBedUnderstoreyCorrectionWorldTransforms.Num(),
        2304);
    const auto ValidateFormalCategory = [this, FormalBedCoreDatumZCm](
        const TArray<FTransform>& Actual,
        const TArray<FTransform>& Repeated,
        const TArray<FTransform>& Source,
        int32 RelocatedSourceCount,
        int32 ExpectedInfillCount,
        double MinimumExpectedScale,
        double MaximumExpectedScale,
        double HorizontalScaleMultiplier,
        double MaximumPitchDegrees,
        double MaximumRollDegrees,
        double MinimumSameSideSeparationCm,
        double MinimumOuterBandFraction,
        double MaximumOuterBandFraction,
        double MinimumCoreSpillFraction,
        double MaximumCoreSpillFraction,
        const TCHAR* Label)
    {
        constexpr double BedCenterAbsXCm = 3400.0;
        constexpr double BedCenterYCm = 8800.0;
        constexpr double PlantHalfSizeXCm = 1540.0;
        constexpr double PlantHalfSizeYCm = 1000.0;
        constexpr double CoreHalfSizeXCm = 1400.0;
        constexpr double CoreHalfSizeYCm = 900.0;
        constexpr double MinimumApronWidthCm = 304.0;
        constexpr double ConservativeVeneerExponent = 2.65;
        // The squircle mapping pulls diagonal points inward, so 0.58 in
        // physical plant-envelope space is the conservative observable form
        // of the builder's outer 0.66..0.98 radial band.
        constexpr double PhysicalOuterBandMinimumRadius = 0.58;
        TestEqual(
            *FString::Printf(TEXT("%s exact full correction census"), Label),
            Actual.Num(),
            Source.Num() + ExpectedInfillCount);
        bool bRepeatsExactly = Actual.Num() == Repeated.Num();
        for (int32 Index = 0;
             bRepeatsExactly && Index < Actual.Num();
             ++Index)
        {
            bRepeatsExactly = Actual[Index].Equals(Repeated[Index], 0.0);
        }
        TestTrue(
            *FString::Printf(TEXT("%s complete correction rebuild repeats exactly"), Label),
            bRepeatsExactly);

        TArray<FVector> LeftPlacedLocations;
        TArray<FVector> RightPlacedLocations;
        const int32 ExpectedPlacedCount =
            RelocatedSourceCount + ExpectedInfillCount;
        LeftPlacedLocations.Reserve(ExpectedPlacedCount / 2);
        RightPlacedLocations.Reserve(ExpectedPlacedCount / 2);
        double MinimumSeenScale = MaximumExpectedScale;
        double MaximumSeenScale = MinimumExpectedScale;
        double MaximumSeenPitch = 0.0;
        double MaximumSeenRoll = 0.0;
        double MaximumSeenYawDelta = 0.0;
        int32 OuterBandCountBySide[2] = {0, 0};
        int32 CoreSpillCountBySide[2] = {0, 0};
        uint8 OuterSectorMaskBySide[2] = {0, 0};
        for (int32 Index = 0; Index < Actual.Num(); ++Index)
        {
            const FTransform& Correction = Actual[Index];
            const FVector Location = Correction.GetTranslation();
            const FVector Scale = Correction.GetScale3D();
            TestTrue(
                *FString::Printf(TEXT("%s correction %d is finite and positive"), Label, Index),
                !Correction.ContainsNaN() &&
                    FMath::IsFinite(Location.X) &&
                    FMath::IsFinite(Location.Y) &&
                    FMath::IsFinite(Location.Z) &&
                    FMath::IsFinite(Scale.X) && Scale.X > 0.0001 &&
                    FMath::IsFinite(Scale.Y) && Scale.Y > 0.0001 &&
                    FMath::IsFinite(Scale.Z) && Scale.Z > 0.0001);
            const bool bIsPlacedCorrection =
                Index < RelocatedSourceCount || Index >= Source.Num();
            if (!bIsPlacedCorrection)
            {
                TestTrue(
                    *FString::Printf(TEXT("%s flank suffix %d remains exact"), Label, Index),
                    Source.IsValidIndex(Index) &&
                        Correction.Equals(Source[Index], 0.0));
                continue;
            }

            TestTrue(
                *FString::Printf(TEXT("%s placed correction %d stays inside an organic bed"), Label, Index),
                FMath::Abs(Location.X) >= 1700.0 &&
                    FMath::Abs(Location.X) <= 5100.0 &&
                    Location.Y >= 7650.0 && Location.Y <= 9950.0);
            const int32 SideIndex = Location.X < 0.0 ? 0 : 1;
            const double LocalX = SideIndex == 0
                ? Location.X + BedCenterAbsXCm
                : BedCenterAbsXCm - Location.X;
            const double LocalY = Location.Y - BedCenterYCm;
            const double PhysicalRadius = FMath::Max(
                FMath::Abs(LocalX) / PlantHalfSizeXCm,
                FMath::Abs(LocalY) / PlantHalfSizeYCm);
            const double ConservativeVeneerRadius =
                FMath::Pow(
                    FMath::Abs(LocalX) /
                        (CoreHalfSizeXCm + MinimumApronWidthCm),
                    ConservativeVeneerExponent) +
                FMath::Pow(
                    FMath::Abs(LocalY) /
                        (CoreHalfSizeYCm + MinimumApronWidthCm),
                    ConservativeVeneerExponent);
            TestTrue(
                *FString::Printf(TEXT("%s placed correction %d remains inside the minimum-width organic veneer"), Label, Index),
                ConservativeVeneerRadius <= 1.000001);
            if (PhysicalRadius >= PhysicalOuterBandMinimumRadius)
            {
                ++OuterBandCountBySide[SideIndex];
                const double NormalizedLocalX =
                    FMath::Abs(LocalX) / PlantHalfSizeXCm;
                const double NormalizedLocalY =
                    FMath::Abs(LocalY) / PlantHalfSizeYCm;
                uint8 Sector = 0;
                if (NormalizedLocalX >= NormalizedLocalY)
                {
                    Sector = LocalX < 0.0 ? 0 : 1;
                }
                else
                {
                    Sector = LocalY < 0.0 ? 2 : 3;
                }
                OuterSectorMaskBySide[SideIndex] |=
                    static_cast<uint8>(1u << Sector);
            }
            if (FMath::Abs(LocalX) > CoreHalfSizeXCm ||
                FMath::Abs(LocalY) > CoreHalfSizeYCm)
            {
                ++CoreSpillCountBySide[SideIndex];
            }
            TestTrue(
                *FString::Printf(TEXT("%s placed correction %d follows the 4.8 cm relief contract"), Label, Index),
                Location.Z >= FormalBedCoreDatumZCm - 2.001 &&
                    Location.Z <= FormalBedCoreDatumZCm + 4.8 - 1.999);
            const FRotator Rotation = Correction.Rotator();
            TestTrue(
                *FString::Printf(TEXT("%s placed correction %d has bounded deterministic lean"), Label, Index),
                FMath::IsFinite(Rotation.Pitch) &&
                    FMath::IsFinite(Rotation.Roll) &&
                    FMath::Abs(Rotation.Pitch) <=
                        MaximumPitchDegrees + 0.000001 &&
                    FMath::Abs(Rotation.Roll) <=
                        MaximumRollDegrees + 0.000001);
            TestTrue(
                *FString::Printf(TEXT("%s placed correction %d stays inside its height band"), Label, Index),
                Scale.Z >= MinimumExpectedScale - 0.000001 &&
                    Scale.Z <= MaximumExpectedScale + 0.000001 &&
                    FMath::IsNearlyEqual(
                        Scale.X,
                        Scale.Z * HorizontalScaleMultiplier,
                        0.000001) &&
                    FMath::IsNearlyEqual(
                        Scale.Y,
                        Scale.Z * HorizontalScaleMultiplier,
                        0.000001));
            MinimumSeenScale = FMath::Min(MinimumSeenScale, Scale.Z);
            MaximumSeenScale = FMath::Max(MaximumSeenScale, Scale.Z);
            MaximumSeenPitch = FMath::Max(
                MaximumSeenPitch,
                FMath::Abs(Rotation.Pitch));
            MaximumSeenRoll = FMath::Max(
                MaximumSeenRoll,
                FMath::Abs(Rotation.Roll));
            if (Index < RelocatedSourceCount)
            {
                MaximumSeenYawDelta = FMath::Max(
                    MaximumSeenYawDelta,
                    FMath::Abs(FRotator::NormalizeAxis(
                        Rotation.Yaw - Source[Index].Rotator().Yaw)));
            }
            if (Location.X < 0.0)
            {
                LeftPlacedLocations.Add(Location);
            }
            else
            {
                RightPlacedLocations.Add(Location);
            }
        }
        TestEqual(
            *FString::Printf(TEXT("%s exact left-bed placed census"), Label),
            LeftPlacedLocations.Num(),
            ExpectedPlacedCount / 2);
        TestEqual(
            *FString::Printf(TEXT("%s exact right-bed placed census"), Label),
            RightPlacedLocations.Num(),
            ExpectedPlacedCount / 2);
        const int32 ExpectedPlacedCountPerSide = ExpectedPlacedCount / 2;
        for (int32 SideIndex = 0; SideIndex < 2; ++SideIndex)
        {
            const double OuterBandFraction =
                static_cast<double>(OuterBandCountBySide[SideIndex]) /
                static_cast<double>(ExpectedPlacedCountPerSide);
            const double CoreSpillFraction =
                static_cast<double>(CoreSpillCountBySide[SideIndex]) /
                static_cast<double>(ExpectedPlacedCountPerSide);
            TestTrue(
                *FString::Printf(
                    TEXT("%s side %d has category-specific outer-band coverage %.6f in [%.6f,%.6f]"),
                    Label,
                    SideIndex,
                    OuterBandFraction,
                    MinimumOuterBandFraction,
                    MaximumOuterBandFraction),
                OuterBandFraction >= MinimumOuterBandFraction &&
                    OuterBandFraction <= MaximumOuterBandFraction);
            TestTrue(
                *FString::Printf(
                    TEXT("%s side %d has bounded outward core spill %.6f in [%.6f,%.6f]"),
                    Label,
                    SideIndex,
                    CoreSpillFraction,
                    MinimumCoreSpillFraction,
                    MaximumCoreSpillFraction),
                CoreSpillFraction >= MinimumCoreSpillFraction &&
                    CoreSpillFraction <= MaximumCoreSpillFraction);
            if (MinimumOuterBandFraction >= 0.50)
            {
                TestEqual(
                    *FString::Printf(TEXT("%s side %d covers all four organic edge sectors"), Label, SideIndex),
                    OuterSectorMaskBySide[SideIndex],
                    static_cast<uint8>(0x0F));
            }
        }
        int32 ExactMirrorCloneCount = 0;
        for (const FVector& Left : LeftPlacedLocations)
        {
            if (RightPlacedLocations.ContainsByPredicate(
                    [&Left](const FVector& Right)
                    {
                        return FMath::IsNearlyEqual(
                                   Left.X, -Right.X, 0.000001) &&
                            FMath::IsNearlyEqual(
                                Left.Y, Right.Y, 0.000001);
                    }))
            {
                ++ExactMirrorCloneCount;
            }
        }
        TestEqual(
            *FString::Printf(TEXT("%s has no exact bilateral mirror clones"), Label),
            ExactMirrorCloneCount,
            0);
        double MinimumSameSideSeparationSquared =
            TNumericLimits<double>::Max();
        const TArray<FVector>* SideLocations[] = {
            &LeftPlacedLocations,
            &RightPlacedLocations};
        for (const TArray<FVector>* Locations : SideLocations)
        {
            for (int32 First = 0; First < Locations->Num(); ++First)
            {
                for (int32 Second = First + 1;
                     Second < Locations->Num();
                     ++Second)
                {
                    MinimumSameSideSeparationSquared = FMath::Min(
                        MinimumSameSideSeparationSquared,
                        FVector::DistSquared2D(
                            (*Locations)[First],
                            (*Locations)[Second]));
                }
            }
        }
        TestTrue(
            *FString::Printf(TEXT("%s best-choice placement preserves blue-noise spacing"), Label),
            MinimumSameSideSeparationSquared >=
                FMath::Square(MinimumSameSideSeparationCm));
        TestTrue(
            *FString::Printf(TEXT("%s exercises most of its scale range"), Label),
            MaximumSeenScale - MinimumSeenScale >=
                0.80 * (MaximumExpectedScale - MinimumExpectedScale));
        TestTrue(
            *FString::Printf(TEXT("%s exercises bounded pitch variation"), Label),
            MaximumSeenPitch >= 0.75 * MaximumPitchDegrees);
        TestTrue(
            *FString::Printf(TEXT("%s exercises bounded roll variation"), Label),
            MaximumSeenRoll >= 0.75 * MaximumRollDegrees);
        TestTrue(
            *FString::Printf(TEXT("%s uses substantially wider yaw variation"), Label),
            MaximumSeenYawDelta >= 120.0 &&
                MaximumSeenYawDelta <= 178.000001);
    };
    ValidateFormalCategory(
        LayoutA.FormalBedShrubCorrectionWorldTransforms,
        LayoutB.FormalBedShrubCorrectionWorldTransforms,
        FormalShrubs,
        512,
        240,
        5.2,
        7.8,
        1.10,
        4.0,
        5.0,
        20.0,
        0.03,
        0.15,
        0.0,
        0.0,
        TEXT("Shrub"));
    ValidateFormalCategory(
        LayoutA.FormalBedFlowerCorrectionWorldTransforms,
        LayoutB.FormalBedFlowerCorrectionWorldTransforms,
        FormalFlowers,
        192,
        720,
        3.8,
        5.8,
        1.35,
        6.0,
        7.0,
        18.0,
        0.40,
        0.58,
        0.04,
        0.14,
        TEXT("Flower"));
    ValidateFormalCategory(
        LayoutA.FormalBedUnderstoreyCorrectionWorldTransforms,
        LayoutB.FormalBedUnderstoreyCorrectionWorldTransforms,
        FormalUnderstorey,
        384,
        1920,
        2.6,
        4.4,
        1.60,
        8.0,
        10.0,
        12.0,
        0.45,
        0.66,
        0.05,
        0.16,
        TEXT("Understorey"));

    const TArray<int32> AccentSourceIndicesBeforeGrounding =
        LayoutA.AccentTurfSourceIndices;
    const TArray<FTransform> AccentTransformsBeforeGrounding =
        LayoutA.AccentTurfWorldTransforms;
    TArray<FTRIADIstanaExploreV5BTreeSource> TreeSources;
    TreeSources.Reserve(729);
    for (int32 Index = 0; Index < 720; ++Index)
    {
        FVector Location;
        if (Index < 128)
        {
            const int32 SideLocalIndex = Index % 64;
            const double Side = Index < 64 ? -1.0 : 1.0;
            Location = FVector(
                Side * (6000.0 + (SideLocalIndex % 8) * 1100.0),
                4000.0 + (SideLocalIndex / 8) * 1600.0,
                32.0 + static_cast<double>(SideLocalIndex % 3));
        }
        else
        {
            Location = FVector(
                (Index & 1) == 0 ? -30000.0 : 30000.0,
                25000.0 + static_cast<double>(Index),
                25.0);
        }
        FTRIADIstanaExploreV5BTreeSource Source;
        Source.WorldTransform = FTransform(
            FRotator(0.0, static_cast<double>(Index % 360), 0.0),
            Location,
            FVector::OneVector);
        Source.SourceMeshHeightCm = 3000.0;
        Source.SourceComponentIndex = 0;
        Source.SourceInstanceIndex = Index;
        TreeSources.Add(Source);
    }
    for (int32 Index = 0; Index < 9; ++Index)
    {
        FTRIADIstanaExploreV5BTreeSource Source;
        Source.WorldTransform = FTransform(
            FQuat::Identity,
            FVector(40000.0 + Index * 100.0, 26000.0, 20.0),
            FVector::OneVector);
        Source.SourceMeshHeightCm = 3200.0;
        Source.SourceComponentIndex = 5;
        Source.SourceInstanceIndex = Index;
        TreeSources.Add(Source);
    }
    TestTrue(
        TEXT("Exact synthetic 720+9 tree census builds bounded grounding"),
        ATRIADIstanaExploreV5BVisualActor::BuildTreeGrounding(
            TreeSources,
            LayoutA,
            LayoutError));
    TestTrue(
        TEXT("Second identical tree grounding build succeeds"),
        ATRIADIstanaExploreV5BVisualActor::BuildTreeGrounding(
            TreeSources,
            LayoutB,
            LayoutError));
    TArray<FTRIADIstanaExploreV5BTreeSource> ReverseOrderedTreeSources;
    ReverseOrderedTreeSources.Reserve(TreeSources.Num());
    for (int32 Index = TreeSources.Num() - 1; Index >= 0; --Index)
    {
        ReverseOrderedTreeSources.Add(TreeSources[Index]);
    }
    FTRIADIstanaExploreV5BDeterministicLayout ReverseOrderedLayout = LayoutA;
    TestTrue(
        TEXT("Reverse source enumeration builds the same canonical maximin grounding"),
        ATRIADIstanaExploreV5BVisualActor::BuildTreeGrounding(
            ReverseOrderedTreeSources,
            ReverseOrderedLayout,
            LayoutError));
    TestTrue(
        TEXT("Maximin grounding is independent of source enumeration order"),
        LayoutA.TreeBaseSourceComponentIndices ==
                ReverseOrderedLayout.TreeBaseSourceComponentIndices &&
            LayoutA.TreeBaseSourceInstanceIndices ==
                ReverseOrderedLayout.TreeBaseSourceInstanceIndices);
    TestEqual(TEXT("Exact tree-base mulch output"), LayoutA.TreeBaseMulchWorldTransforms.Num(), 64);
    TestEqual(TEXT("Exact tree-base shrub output"), LayoutA.TreeBaseShrubWorldTransforms.Num(), 48);
    TestEqual(TEXT("Exact tree-base understorey output"), LayoutA.TreeBaseUnderstoreyWorldTransforms.Num(), 144);
    const auto MoundSurfaceZCm = [](double RadialFraction)
    {
        if (RadialFraction <= 0.30)
        {
            return FMath::Lerp(0.70, 1.80, RadialFraction / 0.30);
        }
        if (RadialFraction <= 0.65)
        {
            return FMath::Lerp(
                1.80,
                0.90,
                (RadialFraction - 0.30) / 0.35);
        }
        if (RadialFraction <= 0.88)
        {
            return FMath::Lerp(
                0.90,
                0.20,
                (RadialFraction - 0.65) / 0.23);
        }
        return FMath::Lerp(
            0.20,
            -0.45,
            (RadialFraction - 0.88) / 0.12);
    };
    const auto ValidateGroundedPlant = [this, &LayoutA, &MoundSurfaceZCm](
        const FTransform& Plant,
        bool bShrub)
    {
        const FVector PlantLocation = Plant.GetTranslation();
        int32 NearestPatchIndex = INDEX_NONE;
        double NearestDistanceCm = TNumericLimits<double>::Max();
        for (int32 PatchIndex = 0;
             PatchIndex < LayoutA.TreeBaseMulchWorldTransforms.Num();
             ++PatchIndex)
        {
            const FVector PatchLocation =
                LayoutA.TreeBaseMulchWorldTransforms[PatchIndex]
                    .GetTranslation();
            const double DistanceCm = FVector2D(
                PlantLocation.X - PatchLocation.X,
                PlantLocation.Y - PatchLocation.Y).Size();
            if (DistanceCm < NearestDistanceCm)
            {
                NearestDistanceCm = DistanceCm;
                NearestPatchIndex = PatchIndex;
            }
        }
        TestTrue(
            TEXT("Every tree-base plant resolves to a host mulch patch"),
            NearestPatchIndex != INDEX_NONE);
        if (NearestPatchIndex == INDEX_NONE)
        {
            return;
        }
        const FTransform& Patch =
            LayoutA.TreeBaseMulchWorldTransforms[NearestPatchIndex];
        const double PatchRadiusCm = Patch.GetScale3D().X * 100.0;
        const double RadialFraction = NearestDistanceCm / PatchRadiusCm;
        const FVector Scale = Plant.GetScale3D();
        const FRotator Rotation = Plant.GetRotation().Rotator();
        const double MinimumRadialFraction = bShrub ? 0.42 : 0.44;
        const double MaximumRadialFraction = bShrub ? 0.56 : 0.58;
        const double MinimumScale = bShrub ? 1.35 : 1.15;
        const double MaximumScale = bShrub ? 1.85 : 1.60;
        const double HorizontalMultiplier = bShrub ? 0.72 : 0.60;
        TestTrue(
            TEXT("Tree-base plant center remains inside its host mulch silhouette"),
            FMath::IsWithinInclusive(
                RadialFraction,
                MinimumRadialFraction - 0.000001,
                MaximumRadialFraction + 0.000001));
        TestTrue(
            TEXT("Tree-base plant vertical scale remains in the realistic range"),
            FMath::IsWithinInclusive(
                Scale.Z,
                MinimumScale - 0.000001,
                MaximumScale + 0.000001));
        TestTrue(
            TEXT("Tree-base plant uses the pinned restrained horizontal spread"),
            FMath::IsNearlyEqual(
                Scale.X,
                Scale.Z * HorizontalMultiplier,
                0.000001) &&
                FMath::IsNearlyEqual(
                    Scale.Y,
                    Scale.Z * HorizontalMultiplier,
                    0.000001));
        TestTrue(
            TEXT("Tree-base plant keeps an upright grounded rotation"),
            FMath::IsNearlyZero(Rotation.Pitch, 0.000001) &&
                FMath::IsNearlyZero(Rotation.Roll, 0.000001));
        const double RootInsetCm = bShrub ? 3.0 : 4.5;
        const double ExpectedPlantZCm = Patch.GetTranslation().Z +
            MoundSurfaceZCm(RadialFraction) - RootInsetCm;
        TestTrue(
            bShrub
                ? TEXT("Tree-base shrub root is inset 3.0 cm into the mound profile")
                : TEXT("Tree-base understorey root is inset 4.5 cm into the mound profile"),
            FMath::IsNearlyEqual(
                PlantLocation.Z,
                ExpectedPlantZCm,
                0.0001));
    };
    for (const FTransform& Shrub : LayoutA.TreeBaseShrubWorldTransforms)
    {
        ValidateGroundedPlant(Shrub, true);
    }
    for (const FTransform& Understorey :
         LayoutA.TreeBaseUnderstoreyWorldTransforms)
    {
        ValidateGroundedPlant(Understorey, false);
    }
    for (int32 PlantedIndex = 0;
         PlantedIndex < 24;
         ++PlantedIndex)
    {
        const int32 PatchIndex = PlantedIndex < 12
            ? PlantedIndex
            : 32 + (PlantedIndex - 12);
        const FVector PatchLocation =
            LayoutA.TreeBaseMulchWorldTransforms[PatchIndex].GetTranslation();
        for (int32 ClusterIndex = 0; ClusterIndex < 2; ++ClusterIndex)
        {
            const FVector ShrubOffset =
                LayoutA.TreeBaseShrubWorldTransforms[
                    PlantedIndex * 2 + ClusterIndex].GetTranslation() -
                PatchLocation;
            const double ShrubAngle = FMath::Atan2(
                ShrubOffset.Y,
                ShrubOffset.X);
            for (int32 MemberIndex = 0; MemberIndex < 3; ++MemberIndex)
            {
                const FVector UnderstoreyOffset =
                    LayoutA.TreeBaseUnderstoreyWorldTransforms[
                        PlantedIndex * 6 + ClusterIndex * 3 + MemberIndex]
                            .GetTranslation() -
                    PatchLocation;
                const double UnderstoreyAngle = FMath::Atan2(
                    UnderstoreyOffset.Y,
                    UnderstoreyOffset.X);
                const double ExpectedOffset =
                    static_cast<double>(MemberIndex - 1) * 0.14;
                TestTrue(
                    TEXT("Tree-base understorey forms two coherent triads"),
                    FMath::IsNearlyEqual(
                        FMath::FindDeltaAngleRadians(
                            ShrubAngle,
                            UnderstoreyAngle),
                        ExpectedOffset,
                        0.000001));
            }
        }
    }
    TestTrue(
        TEXT("Tree grounding preserves the exact accent source-index roster"),
        LayoutA.AccentTurfSourceIndices == AccentSourceIndicesBeforeGrounding);
    TestTrue(
        TEXT("Tree grounding source selections are deterministic"),
        LayoutA.TreeBaseSourceComponentIndices ==
                LayoutB.TreeBaseSourceComponentIndices &&
            LayoutA.TreeBaseSourceInstanceIndices ==
                LayoutB.TreeBaseSourceInstanceIndices);
    int32 LeftGroundingCount = 0;
    int32 RightGroundingCount = 0;
    for (int32 Index = 0;
         Index < LayoutA.TreeBaseMulchWorldTransforms.Num();
         ++Index)
    {
        const FVector Location =
            LayoutA.TreeBaseMulchWorldTransforms[Index].GetTranslation();
        TestTrue(TEXT("Grounding excludes the central lawn"), FMath::Abs(Location.X) >= 5200.0);
        TestTrue(TEXT("Grounding remains inside the bounded flank envelope"), FMath::Abs(Location.X) <= 17000.0 && Location.Y >= 3500.0 && Location.Y <= 22000.0);
        LeftGroundingCount += Location.X < 0.0 ? 1 : 0;
        RightGroundingCount += Location.X > 0.0 ? 1 : 0;
        for (int32 Other = 0; Other < Index; ++Other)
        {
            const FVector OtherLocation =
                LayoutA.TreeBaseMulchWorldTransforms[Other].GetTranslation();
            TestTrue(
                TEXT("Selected tree-base mounds keep the derived 550 cm root spacing"),
                FVector2D(
                    Location.X - OtherLocation.X,
                    Location.Y - OtherLocation.Y).Size() >= 549.999);
        }
    }
    TestEqual(TEXT("Exact left tree-base quota"), LeftGroundingCount, 32);
    TestEqual(TEXT("Exact right tree-base quota"), RightGroundingCount, 32);
    int32 TreeSuppressedAccentCount = 0;
    for (int32 Index = 0; Index < LayoutA.AccentTurfWorldTransforms.Num(); ++Index)
    {
        const FTransform& Before = AccentTransformsBeforeGrounding[Index];
        const FTransform& After = LayoutA.AccentTurfWorldTransforms[Index];
        TestTrue(TEXT("Tree suppression preserves accent rotation"), Before.GetRotation().Equals(After.GetRotation(), 0.000001));
        TestTrue(TEXT("Tree suppression preserves accent scale"), Before.GetScale3D().Equals(After.GetScale3D(), 0.000001));
        TestTrue(TEXT("Tree suppression preserves accent XY"), FMath::IsNearlyEqual(Before.GetTranslation().X, After.GetTranslation().X, 0.000001) && FMath::IsNearlyEqual(Before.GetTranslation().Y, After.GetTranslation().Y, 0.000001));
        const double DipCm = Before.GetTranslation().Z - After.GetTranslation().Z;
        TestTrue(TEXT("Tree suppression changes accent Z by only zero or ten cm"), FMath::IsNearlyZero(DipCm, 0.0001) || FMath::IsNearlyEqual(DipCm, 10.0, 0.0001));
        TreeSuppressedAccentCount += FMath::IsNearlyEqual(DipCm, 10.0, 0.0001) ? 1 : 0;
    }
    TestTrue(TEXT("At least one accent instance is hidden beneath mulch"), TreeSuppressedAccentCount > 0);
    TArray<FTRIADIstanaExploreV5BTreeSource> ShortTreeSources = TreeSources;
    ShortTreeSources.RemoveAt(ShortTreeSources.Num() - 1);
    FTRIADIstanaExploreV5BDeterministicLayout RejectedTreeLayout = LayoutB;
    TestFalse(
        TEXT("A short V4 tree census fails closed"),
        ATRIADIstanaExploreV5BVisualActor::BuildTreeGrounding(
            ShortTreeSources,
            RejectedTreeLayout,
            LayoutError));
    TestEqual(TEXT("Rejected tree census leaves no mulch output"), RejectedTreeLayout.TreeBaseMulchWorldTransforms.Num(), 0);

    TArray<FTransform> ShortSource = SourceCloseTurfTransforms;
    ShortSource.RemoveAt(ShortSource.Num() - 1);
    FTRIADIstanaExploreV5BDeterministicLayout RejectedLayout = LayoutA;
    TestFalse(
        TEXT("Short close-turf source roster is rejected"),
        ATRIADIstanaExploreV5BVisualActor::BuildDeterministicLayout(
            ShortSource,
            RejectedLayout,
            LayoutError));
    TestEqual(
        TEXT("Rejected source roster leaves no accent output"),
        RejectedLayout.AccentTurfWorldTransforms.Num(),
        0);

    TArray<FTransform> InvalidSource = SourceCloseTurfTransforms;
    InvalidSource[37].SetScale3D(FVector(-1.0, 1.0, 1.0));
    TestFalse(
        TEXT("Invalid selected transform is rejected before layout generation"),
        ATRIADIstanaExploreV5BVisualActor::BuildDeterministicLayout(
            InvalidSource,
            RejectedLayout,
            LayoutError));
    TestEqual(
        TEXT("Invalid transform leaves no partial accent output"),
        RejectedLayout.AccentTurfWorldTransforms.Num(),
        0);

    FTRIADIstanaExploreV5BAssetRoster InvalidAssets;
    FString AssetError;
    TestFalse(
        TEXT("Empty V5B asset roster fails preflight"),
        ATRIADIstanaExploreV5BVisualActor::ValidateAssetRoster(
            InvalidAssets,
            AssetError));
    TestTrue(
        TEXT("Empty-roster rejection is explicit"),
        !AssetError.IsEmpty());

    UMaterialInterface* DefaultSurfaceMaterial =
        UMaterial::GetDefaultMaterial(MD_Surface);
    const auto NewTransientMesh = []() -> UStaticMesh*
    {
        return NewObject<UStaticMesh>(
            GetTransientPackage(),
            NAME_None,
            RF_Transient);
    };
    FTRIADIstanaExploreV5BAssetRoster ValidAssets;
    ValidAssets.HardscapeRenderSuccessorMesh = NewTransientMesh();
    ValidAssets.HardscapeRenderSuccessorMaterials = {
        DefaultSurfaceMaterial,
        DefaultSurfaceMaterial,
        DefaultSurfaceMaterial};
    ValidAssets.AccentTurfMesh = NewTransientMesh();
    ValidAssets.AccentTurfMaterial = DefaultSurfaceMaterial;
    ValidAssets.FormalBedVeneerMesh = NewTransientMesh();
    ValidAssets.FormalBedVeneerMaterial = DefaultSurfaceMaterial;
    ValidAssets.FormalBedShrubMesh = NewTransientMesh();
    ValidAssets.FormalBedShrubMaterial = DefaultSurfaceMaterial;
    ValidAssets.FormalBedFlowerMesh = NewTransientMesh();
    ValidAssets.FormalBedFlowerMaterial = DefaultSurfaceMaterial;
    ValidAssets.FormalBedUnderstoreyMesh = NewTransientMesh();
    ValidAssets.FormalBedUnderstoreyMaterial = DefaultSurfaceMaterial;
    ValidAssets.TreeBaseMulchMesh = NewTransientMesh();
    ValidAssets.FountainSurfaceMesh = NewTransientMesh();
    ValidAssets.FountainSurfaceMaterial = DefaultSurfaceMaterial;
    ValidAssets.FountainEdgeFoamMesh = NewTransientMesh();
    ValidAssets.FountainEdgeFoamMaterial = DefaultSurfaceMaterial;
    ValidAssets.OuterPlumeMesh = NewTransientMesh();
    ValidAssets.OuterPlumeMaterial = DefaultSurfaceMaterial;
    ValidAssets.ImpactRingMesh = NewTransientMesh();
    ValidAssets.ImpactRingMaterial = DefaultSurfaceMaterial;
    ValidAssets.CentralPlumeMesh = NewTransientMesh();
    ValidAssets.CentralPlumeMaterial = DefaultSurfaceMaterial;
    ValidAssets.InnerPaverWedgeMesh = NewTransientMesh();
    ValidAssets.OuterPaverWedgeMesh = NewTransientMesh();
    ValidAssets.PaverMaterial = DefaultSurfaceMaterial;
    for (int32 Variant = 0; Variant < 4; ++Variant)
    {
        ValidAssets.PachiraBarkMeshes.Add(NewTransientMesh());
        ValidAssets.PachiraLeavesMeshes.Add(NewTransientMesh());
        ValidAssets.PachiraBarkMaterials.Add(DefaultSurfaceMaterial);
        ValidAssets.PachiraLeavesMaterials.Add(DefaultSurfaceMaterial);
    }
    TestTrue(
        TEXT("Complete exact V5B asset roster passes pure preflight"),
        ATRIADIstanaExploreV5BVisualActor::ValidateAssetRoster(
            ValidAssets,
            AssetError));
    TestTrue(TEXT("Valid asset preflight clears error"), AssetError.IsEmpty());

    ValidAssets.OuterPaverWedgeMesh = ValidAssets.InnerPaverWedgeMesh;
    TestFalse(
        TEXT("Aliased/combined mesh import is rejected"),
        ATRIADIstanaExploreV5BVisualActor::ValidateAssetRoster(
            ValidAssets,
            AssetError));

    return true;
}

#endif
