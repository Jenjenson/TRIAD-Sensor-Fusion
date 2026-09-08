#if WITH_DEV_AUTOMATION_TESTS

#include "TRIADIstanaExploreV4LandscapeActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADIstanaExploreV4RuntimeSourceContractTest,
    "TRIAD.Istana.ExploreV4.RuntimeSourceContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTRIADIstanaExploreV4RuntimeSourceContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    const ATRIADIstanaExploreV4LandscapeActor* Defaults =
        GetDefault<ATRIADIstanaExploreV4LandscapeActor>();
    TestNotNull(TEXT("V4 class default object"), Defaults);
    if (!Defaults)
    {
        return false;
    }

    TestFalse(TEXT("No one-to-one claim"), Defaults->bOneToOneOneKilometerClaimed);
    TestFalse(TEXT("No survey claim"), Defaults->bSurveyAccuracyClaimed);
    TestFalse(TEXT("No botanical-inventory claim"), Defaults->bExactBotanicalInventoryClaimed);
    TestFalse(TEXT("No individual-raster-tree claim"), Defaults->bIndividualRasterTreeTruthClaimed);
    TestFalse(TEXT("No collision/sensor authority"), Defaults->bCollisionOrSensorTruthAuthority);
    TestFalse(TEXT("No Google or OneMap content"), Defaults->bGoogleOrOneMapContentUsed);
    TestFalse(TEXT("No portico collision authority"), Defaults->bPorticoCollisionAuthority);
    TestTrue(TEXT("Heritage visuals remain labelled silhouette proxies"), Defaults->bHeritageVisualsAreSilhouetteProxies);
    TestEqual(
        TEXT("External static lawn surface is named explicitly"),
        Defaults->ExternalStaticLawnSurfaceMaterialName,
        FString(TEXT("MI_IPV4_AmbientCG_Grass001_Lawn")));
    TestFalse(
        TEXT("External static lawn surface has no wind WPO"),
        Defaults->bExternalStaticLawnSurfaceUsesWindWpo);
    TestNotNull(TEXT("Umbrella HISM"), Defaults->UmbrellaTreeInstances.Get());
    TestNotNull(TEXT("Dome HISM"), Defaults->DomeTreeInstances.Get());
    TestNotNull(TEXT("High-fork HISM"), Defaults->HighForkRoundedTreeInstances.Get());
    TestNotNull(TEXT("Columnar HISM"), Defaults->ColumnarNarrowTreeInstances.Get());
    TestNotNull(TEXT("Palm HISM"), Defaults->PalmTreeInstances.Get());
    TestNotNull(TEXT("Heritage umbrella HISM"), Defaults->HeritageUmbrellaInstances.Get());
    TestNotNull(TEXT("Heritage dome HISM"), Defaults->HeritageDomeInstances.Get());
    TestNotNull(TEXT("Heritage high-fork HISM"), Defaults->HeritageHighForkRoundedInstances.Get());
    TestNotNull(TEXT("Heritage columnar HISM"), Defaults->HeritageColumnarNarrowInstances.Get());
    TestNotNull(TEXT("Heritage palm HISM"), Defaults->HeritagePalmInstances.Get());
    TestNotNull(TEXT("Heritage blocker HISM"), Defaults->HeritageAnchorPawnBlockers.Get());
    TestNotNull(TEXT("V8 render-only component"), Defaults->PorticoV8RenderOnlyComponent.Get());
    TestNotNull(TEXT("V5C render-only sibling component"), Defaults->PorticoV5CRenderOnlyComponent.Get());
    TestFalse(TEXT("V5C presentation defaults inactive"), Defaults->IsPorticoV5CPresentationActive());
    if (Defaults->PorticoV5CRenderOnlyComponent)
    {
        TestNull(
            TEXT("V5C sibling has no default mesh"),
            Defaults->PorticoV5CRenderOnlyComponent->GetStaticMesh());
        TestFalse(
            TEXT("V5C sibling defaults hidden"),
            Defaults->PorticoV5CRenderOnlyComponent->IsVisible());
        TestTrue(
            TEXT("V5C sibling defaults hidden in game"),
            Defaults->PorticoV5CRenderOnlyComponent->bHiddenInGame);
        TestEqual(
            TEXT("V5C sibling defaults NoCollision"),
            Defaults->PorticoV5CRenderOnlyComponent->GetCollisionEnabled(),
            ECollisionEnabled::NoCollision);
        TestFalse(
            TEXT("V5C sibling defaults no navigation"),
            Defaults->PorticoV5CRenderOnlyComponent->CanEverAffectNavigation());
        TestTrue(
            TEXT("V5C sibling defaults identity"),
            Defaults->PorticoV5CRenderOnlyComponent->GetRelativeTransform().Equals(
                FTransform::Identity, 0.0));
    }
    TestTrue(TEXT("V5C is explicitly render-only"), Defaults->bPorticoV5CRenderOnly);
    TestFalse(
        TEXT("V5C has no collision/navigation/RF authority"),
        Defaults->bPorticoV5CCollisionNavigationOrRfAuthority);
    TestFalse(TEXT("V5C first-stage materials claim no texture maps"), Defaults->bPorticoV5CUsesTextureMaps);
    TestEqual(
        TEXT("Frozen V5C OBJ digest"),
        Defaults->FrozenPorticoV5CObjSha256,
        FString(TEXT("883EAC65449A54D284D1E0E15B42F134F73C509A5ACE95BFD6DDD1DD326A3EC5")));
    TestEqual(
        TEXT("Frozen V5C MTL digest"),
        Defaults->FrozenPorticoV5CMtlSha256,
        FString(TEXT("DC38D80587AEDFD31143A755388D4248A7C6E2029C4C9FD92794DDE06F2E339C")));
    TestEqual(
        TEXT("Frozen V5C manifest digest"),
        Defaults->FrozenPorticoV5CManifestSha256,
        FString(TEXT("380A2DE1204029C26FA06A7BBFD99105AA9D7979625575B28FF3E45A4B0337A1")));
    TestEqual(
        TEXT("Frozen V5C semantic digest"),
        Defaults->FrozenPorticoV5CSemanticSha256,
        FString(TEXT("84E99970FEAF2F9ACCE9D7BE07C320C4A7E8AAA342F5C30463F0ADE311958567")));
    const UHierarchicalInstancedStaticMeshComponent* BulkPopulationHisMs[] = {
        Defaults->UmbrellaTreeInstances,
        Defaults->DomeTreeInstances,
        Defaults->HighForkRoundedTreeInstances,
        Defaults->ColumnarNarrowTreeInstances,
        Defaults->PalmTreeInstances,
        Defaults->HeritageUmbrellaInstances,
        Defaults->HeritageDomeInstances,
        Defaults->HeritageHighForkRoundedInstances,
        Defaults->HeritageColumnarNarrowInstances,
        Defaults->HeritagePalmInstances,
        Defaults->HeritageAnchorPawnBlockers,
        Defaults->ShrubInstances,
        Defaults->FlowerInstances,
        Defaults->UnderstoreyInstances,
        Defaults->GeometryGrassInstances,
        Defaults->CloseTurfInstances};
    TestEqual(
        TEXT("Exact bulk-population HISM roster count"),
        static_cast<int32>(UE_ARRAY_COUNT(BulkPopulationHisMs)),
        16);
    for (const UHierarchicalInstancedStaticMeshComponent* Component :
         BulkPopulationHisMs)
    {
        TestNotNull(TEXT("Bulk-population HISM exists"), Component);
        if (Component)
        {
            TestEqual(
                TEXT("Bulk-population HISM uses the exact synchronous V4 subclass"),
                Component->GetClass(),
                UTRIADIstanaExploreV4SynchronousHismComponent::StaticClass());
            TestFalse(
                TEXT("Bulk-population HISM default disables density scaling"),
                Component->bEnableDensityScaling);
            TestEqual(
                TEXT("Bulk-population HISM default density is exactly one"),
                Component->CurrentDensityScaling,
                1.0f);
#if WITH_EDITOR
            TestFalse(
                TEXT("Bulk-population HISM cannot enable density scaling in editor"),
                Component->bCanEnableDensityScaling);
#endif
            TestTrue(
                TEXT("Bulk-population HISM default restores auto rebuild"),
                Component->bAutoRebuildTreeOnInstanceChanges);
            TestFalse(
                TEXT("Bulk-population HISM default is not async building"),
                Component->IsAsyncBuilding());
            TestTrue(
                TEXT("Bulk-population HISM default tree is fully built"),
                Component->IsTreeFullyBuilt());
        }
    }
    UHierarchicalInstancedStaticMeshComponent* OutdatedTreeProbe =
        NewObject<UHierarchicalInstancedStaticMeshComponent>(
            GetTransientPackage());
    TestNotNull(TEXT("Outdated-tree negative probe exists"), OutdatedTreeProbe);
    if (OutdatedTreeProbe)
    {
        TestTrue(
            TEXT("Fresh negative probe starts fully built"),
            OutdatedTreeProbe->IsTreeFullyBuilt());
        OutdatedTreeProbe->ClearInstances();
        TestFalse(
            TEXT("Outdated HISM tree fails persisted-readiness gate"),
            OutdatedTreeProbe->IsTreeFullyBuilt());
    }

    UStaticMesh* ProbeMesh = LoadObject<UStaticMesh>(
        nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    TestNotNull(TEXT("Transient synchronous-HISM probe mesh exists"), ProbeMesh);
    if (ProbeMesh)
    {
        UTRIADIstanaExploreV4SynchronousHismComponent* EmptyProbe =
            NewObject<UTRIADIstanaExploreV4SynchronousHismComponent>(
                GetTransientPackage());
        TestNotNull(TEXT("Transient empty synchronous-HISM probe exists"), EmptyProbe);
        if (EmptyProbe)
        {
            EmptyProbe->SetCanEverAffectNavigation(false);
            EmptyProbe->SetStaticMesh(ProbeMesh);
            EmptyProbe->bAutoRebuildTreeOnInstanceChanges = false;
            EmptyProbe->bEnableDensityScaling = true;
            EmptyProbe->CurrentDensityScaling = 0.0f;
            EmptyProbe->OnPostPopulatePerInstanceData();
            EmptyProbe->bAutoRebuildTreeOnInstanceChanges = true;
            TestEqual(
                TEXT("Transient empty probe retains zero instances"),
                EmptyProbe->GetInstanceCount(),
                0);
            TestEqual(
                TEXT("Transient empty probe retains zero render instances"),
                EmptyProbe->GetNumRenderInstances(),
                0);
            TestFalse(
                TEXT("Transient empty probe is not async building"),
                EmptyProbe->IsAsyncBuilding());
            TestTrue(
                TEXT("Transient empty probe tree is fully built"),
                EmptyProbe->IsTreeFullyBuilt());
            TestFalse(
                TEXT("Transient empty probe disables density scaling"),
                EmptyProbe->bEnableDensityScaling);
            TestEqual(
                TEXT("Transient empty probe restores unit density"),
                EmptyProbe->CurrentDensityScaling,
                1.0f);
        }

        UTRIADIstanaExploreV4SynchronousHismComponent* PopulatedProbe =
            NewObject<UTRIADIstanaExploreV4SynchronousHismComponent>(
                GetTransientPackage());
        TestNotNull(
            TEXT("Transient populated synchronous-HISM probe exists"),
            PopulatedProbe);
        if (PopulatedProbe)
        {
            PopulatedProbe->SetCanEverAffectNavigation(false);
            PopulatedProbe->SetStaticMesh(ProbeMesh);
            PopulatedProbe->bAutoRebuildTreeOnInstanceChanges = false;
            TestEqual(
                TEXT("Transient populated probe adds one ordered instance"),
                PopulatedProbe->AddInstance(FTransform::Identity),
                0);
            TestFalse(
                TEXT("Transient populated probe is outdated before finalization"),
                PopulatedProbe->IsTreeFullyBuilt());
            PopulatedProbe->bEnableDensityScaling = true;
            PopulatedProbe->CurrentDensityScaling = 0.0f;
            PopulatedProbe->OnPostPopulatePerInstanceData();
            PopulatedProbe->bAutoRebuildTreeOnInstanceChanges = true;
            TestEqual(
                TEXT("Transient populated probe retains one instance"),
                PopulatedProbe->GetInstanceCount(),
                1);
            TestEqual(
                TEXT("Transient populated probe has one render instance"),
                PopulatedProbe->GetNumRenderInstances(),
                1);
            TestFalse(
                TEXT("Transient populated probe is not async building"),
                PopulatedProbe->IsAsyncBuilding());
            TestTrue(
                TEXT("Transient populated probe tree is fully built"),
                PopulatedProbe->IsTreeFullyBuilt());
            TestFalse(
                TEXT("Transient populated probe disables density scaling"),
                PopulatedProbe->bEnableDensityScaling);
            TestEqual(
                TEXT("Transient populated probe restores unit density"),
                PopulatedProbe->CurrentDensityScaling,
                1.0f);
        }
    }
    TestEqual(
        TEXT("Exact persistent wind role count"),
        static_cast<int32>(ETRIADIstanaExploreV4WindRole::Count),
        18);
    TestEqual(TEXT("Fixed 120 Hz wind step"), Defaults->FixedWindStepSeconds, 1.0f / 120.0f, 0.0000001f);
    TestTrue(TEXT("Recovery is explicitly underdamped"), Defaults->RecoveryDampingRatio > 0.0f && Defaults->RecoveryDampingRatio < 1.0f);
    TestEqual(TEXT("Natural recovery damping"), Defaults->RecoveryDampingRatio, 0.55f, 0.0001f);
    TestFalse(TEXT("Low-poly palm is not a free-roam placement"), Defaults->bLowPolyPalmRuntimePlacementAllowed);
    TestEqual(
        TEXT("Frozen vegetation contract schema is v2"),
        Defaults->FrozenVegetationContractSchema,
        FString(TEXT("triad.istana_explore_v4_vegetation_contract.v2")));
    TestEqual(
        TEXT("Frozen vegetation contract v2 digest"),
        Defaults->FrozenVegetationContractSha256,
        FString(TEXT("ABDC65AA14DA9AE89736BBE2D75E6212FA38B76B44DBA491C9266DEBC247E104")));

    TArray<FTransform> InheritedTransforms;
    TArray<FTRIADIstanaExploreV4RasterCue> RasterCues;
    InheritedTransforms.Reserve(720);
    RasterCues.Reserve(720);
    const uint8 SupportedClasses[] = {0, 10, 30, 50, 60, 80};
    for (int32 Index = 0; Index < 720; ++Index)
    {
        const double X = static_cast<double>((Index % 30) - 15) * 725.0;
        const double Y = static_cast<double>((Index / 30) - 12) * 825.0;
        const double Z = static_cast<double>(Index % 11) * 7.0;
        const double Pitch = static_cast<double>((Index % 7) - 3);
        const double Yaw = static_cast<double>((Index * 37) % 360);
        const double Roll = static_cast<double>((Index % 5) - 2);
        InheritedTransforms.Add(FTransform(
            FRotator(Pitch, Yaw, Roll),
            FVector(X, Y, Z),
            FVector(0.8, 0.8, 0.8)));

        FTRIADIstanaExploreV4RasterCue Cue;
        Cue.BroadRasterClass = SupportedClasses[Index % 6];
        Cue.HeightBand = static_cast<ETRIADIstanaExploreV4CanopyHeightBand>(
            Index % 4);
        RasterCues.Add(Cue);
    }
    const TArray<float> NativeHeightsCm = {
        1840.0f, 1620.0f, 2010.0f, 1760.0f, 1330.0f};

    TArray<FTRIADIstanaExploreV4ClassifiedTreeRow> Rows;
    FString Error;
    TestTrue(
        TEXT("Exact 720-row deterministic classification succeeds"),
        ATRIADIstanaExploreV4LandscapeActor::BuildDeterministicReclassifiedRows(
            InheritedTransforms,
            RasterCues,
            NativeHeightsCm,
            0x4757A6A5,
            Rows,
            Error));
    TestEqual(TEXT("Exactly 720 output rows"), Rows.Num(), 720);

    int32 FormCounts[5] = {0, 0, 0, 0, 0};
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        const FTRIADIstanaExploreV4ClassifiedTreeRow& Row = Rows[Index];
        TestEqual(TEXT("Source index remains ordered"), Row.SourceIndex, Index);
        TestTrue(
            TEXT("Translation remains exact"),
            Row.WorldTransform.GetTranslation().Equals(
                InheritedTransforms[Index].GetTranslation(), 0.0001));
        TestTrue(
            TEXT("Full source rotation remains exact"),
            ATRIADIstanaExploreV4LandscapeActor::
                PreservesSourceTranslationAndRotation(
                    Row.WorldTransform, InheritedTransforms[Index]));
        TestTrue(
            TEXT("Rescaled tree remains finite/positive"),
            !Row.WorldTransform.ContainsNaN() &&
                Row.WorldTransform.GetScale3D().GetMin() > 0.0);
        const int32 FormIndex = static_cast<int32>(Row.Form);
        TestTrue(TEXT("Form index is one of five"), FormIndex >= 0 && FormIndex < 5);
        if (FormIndex >= 0 && FormIndex < 5)
        {
            ++FormCounts[FormIndex];
        }
        const FVector Scale = Row.WorldTransform.GetScale3D();
        const bool bFrozenColumnarIndex = Index >= 272 && Index < 504;
        if (bFrozenColumnarIndex)
        {
            TestEqual(
                TEXT("Frozen V2 columnar range retains columnar form"),
                static_cast<uint8>(Row.Form),
                static_cast<uint8>(
                    ETRIADIstanaExploreV4TreeForm::ColumnarNarrow));
            TestTrue(
                TEXT("Frozen V2 columnar range retains the entire source transform"),
                Row.WorldTransform.Equals(InheritedTransforms[Index], 0.0f));
        }
        else
        {
            const ETRIADIstanaExploreV4CanopyHeightBand Band =
                RasterCues[Index].HeightBand;
            if (Band == ETRIADIstanaExploreV4CanopyHeightBand::Low18To23Meters)
            {
                TestEqual(TEXT("Low band routes to umbrella"), static_cast<uint8>(Row.Form), static_cast<uint8>(ETRIADIstanaExploreV4TreeForm::Umbrella));
            }
            else if (Band == ETRIADIstanaExploreV4CanopyHeightBand::Mid23To29Meters)
            {
                TestEqual(TEXT("Mid band routes to high-fork"), static_cast<uint8>(Row.Form), static_cast<uint8>(ETRIADIstanaExploreV4TreeForm::HighForkRounded));
            }
            else if (Band == ETRIADIstanaExploreV4CanopyHeightBand::High29To37Meters)
            {
                TestEqual(TEXT("High band routes to dome"), static_cast<uint8>(Row.Form), static_cast<uint8>(ETRIADIstanaExploreV4TreeForm::Dome));
            }
            else
            {
                TestTrue(TEXT("NoData hashes only across three admitted forms"), FormIndex >= 0 && FormIndex < 3);
            }
        }
        if (Row.Form == ETRIADIstanaExploreV4TreeForm::Umbrella)
        {
            const double HeightCm = Scale.Z * NativeHeightsCm[FormIndex];
            const double MaximumHeightCm =
                RasterCues[Index].HeightBand ==
                        ETRIADIstanaExploreV4CanopyHeightBand::Low18To23Meters
                    ? 2300.0
                    : 2428.810830713951;
            TestTrue(TEXT("Umbrella scale remains uniform"), Scale.Equals(FVector(Scale.Z), 0.0001));
            TestTrue(TEXT("Umbrella height remains in admitted range"), HeightCm >= 2000.0 - 0.001 && HeightCm <= MaximumHeightCm + 0.001);
        }
        else if (Row.Form == ETRIADIstanaExploreV4TreeForm::Dome)
        {
            TestTrue(TEXT("Dome scale remains uniform"), Scale.Equals(FVector(Scale.Z), 0.0001));
            TestTrue(TEXT("Dome height is exact admitted anchor"), FMath::IsNearlyEqual(Scale.Z * NativeHeightsCm[FormIndex], 3650.0, 0.001));
        }
        else if (Row.Form == ETRIADIstanaExploreV4TreeForm::HighForkRounded)
        {
            TestTrue(TEXT("High-fork scale remains uniform"), Scale.Equals(FVector(Scale.Z), 0.0001));
            TestTrue(TEXT("High-fork height is exact admitted anchor"), FMath::IsNearlyEqual(Scale.Z * NativeHeightsCm[FormIndex], 2830.0, 0.001));
        }
        else if (!bFrozenColumnarIndex)
        {
            AddError(TEXT("A non-columnar source row used the frozen columnar or low-poly palm form."));
        }
    }
    TestEqual(TEXT("Synthetic umbrella census is deterministic"), FormCounts[0], 162);
    TestEqual(TEXT("Synthetic dome census is deterministic"), FormCounts[1], 174);
    TestEqual(TEXT("Synthetic high-fork census is deterministic"), FormCounts[2], 152);
    TestEqual(TEXT("Frozen V2 columnar census is exact"), FormCounts[3], 232);
    TestEqual(TEXT("Palm runtime census is exactly zero"), FormCounts[4], 0);

    FTransform PitchRollDrift = Rows[0].WorldTransform;
    const FRotator SourceRotation = InheritedTransforms[0].Rotator();
    PitchRollDrift.SetRotation(FRotator(
        SourceRotation.Pitch + 4.0,
        SourceRotation.Yaw,
        SourceRotation.Roll - 5.0).Quaternion());
    TestTrue(
        TEXT("Negative pose keeps yaw unchanged"),
        FMath::Abs(FMath::FindDeltaAngleDegrees(
            PitchRollDrift.Rotator().Yaw,
            SourceRotation.Yaw)) <= 0.0001);
    TestFalse(
        TEXT("Pitch/roll drift with unchanged yaw is rejected"),
        ATRIADIstanaExploreV4LandscapeActor::
            PreservesSourceTranslationAndRotation(
                PitchRollDrift, InheritedTransforms[0]));

    FTransform CloseTurfScaleDrift = InheritedTransforms[1];
    CloseTurfScaleDrift.SetScale3D(
        CloseTurfScaleDrift.GetScale3D() + FVector(0.0, 0.0, 0.25));
    TestFalse(
        TEXT("Full close-turf replacement gate rejects scale drift"),
        CloseTurfScaleDrift.Equals(InheritedTransforms[1], 0.001f));
    FTransform CloseTurfPitchRollDrift = InheritedTransforms[1];
    const FRotator CloseTurfSourceRotation = InheritedTransforms[1].Rotator();
    CloseTurfPitchRollDrift.SetRotation(FRotator(
        CloseTurfSourceRotation.Pitch + 3.0,
        CloseTurfSourceRotation.Yaw,
        CloseTurfSourceRotation.Roll - 4.0).Quaternion());
    TestFalse(
        TEXT("Full close-turf replacement gate rejects pitch/roll drift"),
        CloseTurfPitchRollDrift.Equals(InheritedTransforms[1], 0.001f));

    const float PulseDurationsSeconds[] = {1.6f, 2.4f, 3.2f};
    for (float PulseDurationSeconds : PulseDurationsSeconds)
    {
        const float StepSeconds = Defaults->FixedWindStepSeconds;
        const float AngularFrequency =
            2.0f * PI * Defaults->RecoveryFrequencyHz;
        float Strength = Defaults->BaseWindStrengthCm;
        float Velocity = 0.0f;
        float MinimumStrength = Strength;
        float MaximumStrength = Strength;
        bool bSawPositiveReleaseVelocity = false;
        bool bSawNegativeReleaseVelocity = false;
        const float EndSeconds = PulseDurationSeconds + 12.0f;
        const int32 TotalSteps = FMath::RoundToInt(
            EndSeconds / StepSeconds);
        for (int32 StepIndex = 1; StepIndex <= TotalSteps; ++StepIndex)
        {
            const float TimeSeconds = StepIndex * StepSeconds;
            const float Pulse = TimeSeconds <= PulseDurationSeconds
                ? FMath::Sin(PI * TimeSeconds / PulseDurationSeconds)
                : 0.0f;
            const float Target = FMath::Lerp(
                Defaults->BaseWindStrengthCm,
                Defaults->GustPeakStrengthCm,
                Pulse);
            const float Acceleration =
                AngularFrequency * AngularFrequency * (Target - Strength) -
                2.0f * Defaults->RecoveryDampingRatio * AngularFrequency *
                    Velocity;
            Velocity += Acceleration * StepSeconds;
            Strength += Velocity * StepSeconds;
            MinimumStrength = FMath::Min(MinimumStrength, Strength);
            MaximumStrength = FMath::Max(MaximumStrength, Strength);
            if (TimeSeconds > PulseDurationSeconds)
            {
                bSawPositiveReleaseVelocity |= Velocity > 0.0001f;
                bSawNegativeReleaseVelocity |= Velocity < -0.0001f;
            }
        }
        TestTrue(TEXT("Pulse/release remains finite"), FMath::IsFinite(Strength) && FMath::IsFinite(Velocity));
        TestTrue(TEXT("Pulse/release never reaches the zero-strength clamp"), MinimumStrength > 0.01f);
        TestTrue(TEXT("Underdamped response retains gentle overshoot"), MaximumStrength > Defaults->GustPeakStrengthCm);
        TestTrue(TEXT("Release recovery has signed after-sway"), bSawPositiveReleaseVelocity && bSawNegativeReleaseVelocity);
        TestTrue(TEXT("Pulse/release settles to base strength"), FMath::IsNearlyEqual(Strength, Defaults->BaseWindStrengthCm, 0.01f));
        TestTrue(TEXT("Pulse/release settles velocity"), FMath::IsNearlyZero(Velocity, 0.01f));
    }

    TArray<FTRIADIstanaExploreV4ClassifiedTreeRow> RepeatRows;
    TestTrue(
        TEXT("Repeat classification succeeds"),
        ATRIADIstanaExploreV4LandscapeActor::BuildDeterministicReclassifiedRows(
            InheritedTransforms,
            RasterCues,
            NativeHeightsCm,
            0x4757A6A5,
            RepeatRows,
            Error));
    TestEqual(TEXT("Repeat row count"), RepeatRows.Num(), Rows.Num());
    for (int32 Index = 0; Index < FMath::Min(Rows.Num(), RepeatRows.Num()); ++Index)
    {
        TestEqual(
            TEXT("Repeat form is deterministic"),
            static_cast<uint8>(RepeatRows[Index].Form),
            static_cast<uint8>(Rows[Index].Form));
        TestTrue(
            TEXT("Repeat transform is deterministic"),
            RepeatRows[Index].WorldTransform.Equals(
                Rows[Index].WorldTransform, 0.0001f));
    }

    TArray<FTransform> TooFewTransforms = InheritedTransforms;
    TooFewTransforms.Pop();
    TArray<FTRIADIstanaExploreV4ClassifiedTreeRow> RejectedRows;
    TestFalse(
        TEXT("719 transforms fail closed"),
        ATRIADIstanaExploreV4LandscapeActor::BuildDeterministicReclassifiedRows(
            TooFewTransforms,
            RasterCues,
            NativeHeightsCm,
            0x4757A6A5,
            RejectedRows,
            Error));
    TestEqual(TEXT("Rejected row output is empty"), RejectedRows.Num(), 0);

    TArray<FTRIADIstanaExploreV4RasterCue> InvalidCues = RasterCues;
    InvalidCues[17].BroadRasterClass = 42;
    TestFalse(
        TEXT("Unsupported raster class fails closed"),
        ATRIADIstanaExploreV4LandscapeActor::BuildDeterministicReclassifiedRows(
            InheritedTransforms,
            InvalidCues,
            NativeHeightsCm,
            0x4757A6A5,
            RejectedRows,
            Error));

    TArray<FTransform> NonFiniteTransforms = InheritedTransforms;
    FTransform NonFinite = NonFiniteTransforms[9];
    NonFinite.SetTranslation(FVector(
        std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0));
    NonFiniteTransforms[9] = NonFinite;
    TestFalse(
        TEXT("Non-finite transform fails closed"),
        ATRIADIstanaExploreV4LandscapeActor::BuildDeterministicReclassifiedRows(
            NonFiniteTransforms,
            RasterCues,
            NativeHeightsCm,
            0x4757A6A5,
            RejectedRows,
            Error));

    TArray<float> InvalidNativeHeights = NativeHeightsCm;
    InvalidNativeHeights[3] = 0.0f;
    TestFalse(
        TEXT("Non-positive native height fails closed"),
        ATRIADIstanaExploreV4LandscapeActor::BuildDeterministicReclassifiedRows(
            InheritedTransforms,
            RasterCues,
            InvalidNativeHeights,
            0x4757A6A5,
            RejectedRows,
            Error));

    return true;
}

#endif
