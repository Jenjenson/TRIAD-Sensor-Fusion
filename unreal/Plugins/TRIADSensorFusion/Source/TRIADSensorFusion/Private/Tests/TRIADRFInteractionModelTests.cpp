#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "Misc/AutomationTest.h"
#include "TRIADRFInteractionModel.h"

namespace
{
FTRIADRFSurfaceProfile MakeSurface(bool bCalibrated = true)
{
    FTRIADRFSurfaceProfile Surface;
    Surface.SurfaceId = TEXT("TestWall_01");
    Surface.SolidId = TEXT("TestSolid_01");
    Surface.MaterialId = TEXT("TEST_CONCRETE_PARAMETRIC");
    Surface.ProfileId = TEXT("TEST_CONCRETE_2_TO_6_GHZ");
    Surface.SourceClass = TEXT("SYNTHETIC_NATIVE_TEST");
    Surface.UncertaintyClass = TEXT("TEST_ONLY_HIGH_UNCERTAINTY");
    Surface.CoefficientSelectionSemantics =
        TEXT("SINGLE_EXPLICIT_PROFILE_CLOSED_INTERVAL_NO_INTERPOLATION");
    Surface.MinimumFrequencyGHz = 2.0;
    Surface.MaximumFrequencyGHz = 6.0;
    Surface.MinimumIncidenceCosine = 0.05;
    Surface.MaximumIncidenceCosine = 1.0;
    Surface.PairedBoundaryTransmissionLossDb = 5.0;
    Surface.BulkAttenuationDbPerMeter = 10.0;
    Surface.ReflectionLossDb = 6.0;
    Surface.EmpiricalGrazingReflectionLossDb = 2.0;
    Surface.bAllowsTransmission = true;
    Surface.bAllowsReflection = true;
    Surface.CalibrationState = bCalibrated
        ? ETRIADRFMaterialCalibrationState::Calibrated
        : ETRIADRFMaterialCalibrationState::Uncalibrated;
    Surface.CalibrationProvenanceId = bCalibrated
        ? TEXT("TEST_CALIBRATION_RECEIPT")
        : TEXT("");
    return Surface;
}

FTRIADRFClearSegmentWitness MakeWitness(int32 SegmentIndex)
{
    FTRIADRFClearSegmentWitness Witness;
    Witness.WitnessId = FString::Printf(
        TEXT("GeometryQuery_Test_Segment_%d"),
        SegmentIndex);
    Witness.SegmentIndex = SegmentIndex;
    Witness.bNoUnmodelledBlockingHit = true;
    return Witness;
}

FTRIADRFPathCandidate MakeDirectPath()
{
    FTRIADRFPathCandidate Path;
    Path.PathId = TEXT("OpenApertureWitness");
    Path.GeometryQueryId = TEXT("GeometryQuery_Test_001");
    Path.Kind = ETRIADRFPathKind::Direct;
    Path.VerticesCentimeters = {
        FVector(0.0, 0.0, 0.0),
        FVector(100000.0, 0.0, 0.0)};
    Path.ClearSegmentWitnesses.Add(MakeWitness(0));
    return Path;
}

FTRIADRFPathCandidate MakeTransmissionPath(bool bCalibrated = true)
{
    FTRIADRFPathCandidate Path = MakeDirectPath();
    Path.PathId = TEXT("WallTransmission");
    Path.Kind = ETRIADRFPathKind::Transmitted;
    FTRIADRFPathInteraction Transmission;
    Transmission.Kind = ETRIADRFInteractionKind::Transmission;
    Transmission.Surface = MakeSurface(bCalibrated);
    Transmission.SegmentIndex = 0;
    Transmission.EntryPointCentimeters = FVector(49990.0, 0.0, 0.0);
    Transmission.ExitPointCentimeters = FVector(50010.0, 0.0, 0.0);
    FTRIADRFInteractionContributor Contributor;
    Contributor.SolidId = Transmission.Surface.SolidId;
    Contributor.EntrySurfaceId = Transmission.Surface.SurfaceId;
    Contributor.ExitSurfaceId = TEXT("TestWall_01_Exit");
    Contributor.SourceClass = Transmission.Surface.SourceClass;
    Contributor.UncertaintyClass = Transmission.Surface.UncertaintyClass;
    Contributor.EntryPointCentimeters = Transmission.EntryPointCentimeters;
    Contributor.ExitPointCentimeters = Transmission.ExitPointCentimeters;
    Transmission.Surface.Contributors.Add(MoveTemp(Contributor));
    Transmission.SurfaceNormal = FVector(1.0, 0.0, 0.0);
    Path.Interactions.Add(Transmission);
    return Path;
}

FTRIADRFPathCandidate MakeReflectionPath()
{
    FTRIADRFPathCandidate Path;
    Path.PathId = TEXT("OneBounce");
    Path.GeometryQueryId = TEXT("GeometryQuery_Test_Reflection_001");
    Path.Kind = ETRIADRFPathKind::SingleReflection;
    const double BounceY = 50000.0 / FMath::Sqrt(3.0);
    Path.VerticesCentimeters = {
        FVector(0.0, 0.0, 0.0),
        FVector(50000.0, BounceY, 0.0),
        FVector(100000.0, 0.0, 0.0)};
    Path.ClearSegmentWitnesses = {MakeWitness(0), MakeWitness(1)};
    FTRIADRFPathInteraction Reflection;
    Reflection.Kind = ETRIADRFInteractionKind::Reflection;
    Reflection.Surface = MakeSurface();
    Reflection.VertexIndex = 1;
    Reflection.SurfacePointCentimeters = Path.VerticesCentimeters[1];
    Reflection.SurfaceNormal = FVector(0.0, 1.0, 0.0);
    Path.Interactions.Add(Reflection);
    return Path;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADRFInteractionModelContractTest,
    "TRIAD.SensorFusion.RF.GeometryInteractionModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADRFInteractionModelContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FTRIADDeterministicRFInteractionModel Model;
    FString Error;

    FTRIADRFPathEvaluation Direct;
    const FTRIADRFPathCandidate DirectPath = MakeDirectPath();
    TestTrue(TEXT("Bounded direct open-aperture path evaluates"), Model.EvaluatePath(DirectPath, 2.4, Direct));
    TestTrue(TEXT("Direct path result valid"), Direct.bValid);
    TestTrue(TEXT("Direct telemetry keeps the path id"), Direct.PathId == DirectPath.PathId);
    TestTrue(TEXT("Direct telemetry keeps the geometry-query id"), Direct.GeometryQueryId == DirectPath.GeometryQueryId);
    TestEqual(TEXT("Direct telemetry keeps one clear witness"), Direct.ClearSegmentWitnessIds.Num(), 1);
    TestEqual(TEXT("Direct path is exactly 1 km"), Direct.PathLengthMeters, 1000.0);
    TestEqual(TEXT("Direct path has no material loss"), Direct.InteractionLossDb, 0.0);
    TestEqual(TEXT("2.4 GHz 1 km FSPL reference"), Direct.FreeSpacePathLossDb, 100.0520080561155, 1e-10);
    TestTrue(
        TEXT("Direct material calibration is not applicable"),
        Direct.MaterialCalibrationState ==
            ETRIADRFMaterialCalibrationState::NotApplicable);
    TestFalse(TEXT("Parametric model never binds an external acceptance context"), Direct.bExternalAcceptanceContextBound);
    TestFalse(TEXT("Antenna far-field applicability remains unvalidated"), Direct.bFriisFarFieldApplicabilityValidated);
    TestFalse(TEXT("Parametric direct path cannot self-promote to survey truth"), Direct.bReadyForSurveyTruth);
    TestTrue(TEXT("Readiness reason is explicit"), Direct.ReadinessSemantics.Contains(TEXT("NOT_SURVEY_READY")));

    const FTRIADRFPathCandidate TransmissionPath = MakeTransmissionPath();
    FTRIADRFPathEvaluation NormalTransmission;
    TestTrue(TEXT("Normal-incidence paired transmission evaluates"), Model.EvaluatePath(TransmissionPath, 2.4, NormalTransmission));
    TestEqual(TEXT("Paired boundary plus 20 cm bulk loss"), NormalTransmission.InteractionLossDb, 7.0, 1e-10);
    TestEqual(TEXT("One interaction telemetry row emitted"), NormalTransmission.InteractionEvaluations.Num(), 1);
    if (NormalTransmission.InteractionEvaluations.Num() == 1)
    {
        TestEqual(TEXT("Normal thickness is computed from paired points"), NormalTransmission.InteractionEvaluations[0].NormalThicknessMeters, 0.2, 1e-10);
        TestEqual(TEXT("Normal traversal is 20 cm"), NormalTransmission.InteractionEvaluations[0].TraversalDistanceMeters, 0.2, 1e-10);
        TestEqual(TEXT("Normal incidence cosine is computed"), NormalTransmission.InteractionEvaluations[0].IncidenceCosine, 1.0, 1e-10);
        TestEqual(TEXT("Solid provenance is preserved"), NormalTransmission.InteractionEvaluations[0].SolidId, FString(TEXT("TestSolid_01")));
        TestEqual(TEXT("Profile provenance is preserved"), NormalTransmission.InteractionEvaluations[0].ProfileId, FString(TEXT("TEST_CONCRETE_2_TO_6_GHZ")));
        TestEqual(TEXT("Source class is preserved"), NormalTransmission.InteractionEvaluations[0].SourceClass, FString(TEXT("SYNTHETIC_NATIVE_TEST")));
        TestEqual(TEXT("Uncertainty class is preserved"), NormalTransmission.InteractionEvaluations[0].UncertaintyClass, FString(TEXT("TEST_ONLY_HIGH_UNCERTAINTY")));
        TestEqual(TEXT("Coefficient-selection semantics are preserved"), NormalTransmission.InteractionEvaluations[0].CoefficientSelectionSemantics, FString(TEXT("SINGLE_EXPLICIT_PROFILE_CLOSED_INTERVAL_NO_INTERPOLATION")));
        TestEqual(TEXT("Physical contributor provenance is copied into evaluation"), NormalTransmission.InteractionEvaluations[0].Contributors.Num(), 1);
        if (NormalTransmission.InteractionEvaluations[0].Contributors.Num() == 1)
        {
            TestEqual(TEXT("Contributor solid is preserved"), NormalTransmission.InteractionEvaluations[0].Contributors[0].SolidId, FString(TEXT("TestSolid_01")));
            TestEqual(TEXT("Contributor entry surface is preserved"), NormalTransmission.InteractionEvaluations[0].Contributors[0].EntrySurfaceId, FString(TEXT("TestWall_01")));
        }
        TestEqual(TEXT("Minimum profile frequency is preserved"), NormalTransmission.InteractionEvaluations[0].MinimumFrequencyGHz, 2.0, 1e-10);
        TestEqual(TEXT("Maximum profile frequency is preserved"), NormalTransmission.InteractionEvaluations[0].MaximumFrequencyGHz, 6.0, 1e-10);
        TestEqual(TEXT("Paired-boundary coefficient is preserved"), NormalTransmission.InteractionEvaluations[0].PairedBoundaryTransmissionLossDb, 5.0, 1e-10);
        TestEqual(TEXT("Bulk coefficient is preserved"), NormalTransmission.InteractionEvaluations[0].BulkAttenuationDbPerMeter, 10.0, 1e-10);
        TestEqual(TEXT("Reflection coefficient is preserved"), NormalTransmission.InteractionEvaluations[0].ReflectionLossDb, 6.0, 1e-10);
        TestEqual(TEXT("Grazing coefficient is preserved"), NormalTransmission.InteractionEvaluations[0].EmpiricalGrazingReflectionLossDb, 2.0, 1e-10);
        TestEqual(TEXT("Per-interaction calibration provenance is preserved"), NormalTransmission.InteractionEvaluations[0].CalibrationProvenanceId, FString(TEXT("TEST_CALIBRATION_RECEIPT")));
        TestTrue(TEXT("Per-interaction calibration state is preserved"), NormalTransmission.InteractionEvaluations[0].CalibrationState == ETRIADRFMaterialCalibrationState::Calibrated);
    }
    TestTrue(
        TEXT("Calibrated coefficient provenance is reported as calibrated"),
        NormalTransmission.MaterialCalibrationState ==
            ETRIADRFMaterialCalibrationState::Calibrated);
    TestEqual(TEXT("Calibrated provenance is emitted once"), NormalTransmission.CalibrationProvenanceIds.Num(), 1);
    TestFalse(TEXT("Calibrated coefficients alone cannot create survey readiness"), NormalTransmission.bReadyForSurveyTruth);

    FTRIADRFPathCandidate ObliquePath = MakeTransmissionPath();
    ObliquePath.PathId = TEXT("ObliqueWallTransmission");
    ObliquePath.Interactions[0].EntryPointCentimeters = FVector(49980.0, 0.0, 0.0);
    ObliquePath.Interactions[0].ExitPointCentimeters = FVector(50020.0, 0.0, 0.0);
    ObliquePath.Interactions[0].Surface.Contributors[0].EntryPointCentimeters =
        ObliquePath.Interactions[0].EntryPointCentimeters;
    ObliquePath.Interactions[0].Surface.Contributors[0].ExitPointCentimeters =
        ObliquePath.Interactions[0].ExitPointCentimeters;
    ObliquePath.Interactions[0].SurfaceNormal =
        FVector(0.5, FMath::Sqrt(0.75), 0.0);
    FTRIADRFPathEvaluation ObliqueTransmission;
    TestTrue(TEXT("Oblique paired transmission evaluates"), Model.EvaluatePath(ObliquePath, 2.4, ObliqueTransmission));
    if (ObliqueTransmission.InteractionEvaluations.Num() == 1)
    {
        TestEqual(TEXT("Oblique pair yields 20 cm normal thickness"), ObliqueTransmission.InteractionEvaluations[0].NormalThicknessMeters, 0.2, 1e-10);
        TestEqual(TEXT("Oblique pair yields 40 cm ray traversal"), ObliqueTransmission.InteractionEvaluations[0].TraversalDistanceMeters, 0.4, 1e-10);
        TestEqual(TEXT("Oblique incidence is geometry-derived"), ObliqueTransmission.InteractionEvaluations[0].IncidenceCosine, 0.5, 1e-10);
    }
    TestEqual(TEXT("Oblique path uses actual paired chord"), ObliqueTransmission.InteractionLossDb, 9.0, 1e-10);

    FTRIADRFPathCandidate ReverseTransmission = ObliquePath;
    ReverseTransmission.PathId = TEXT("ReverseObliqueWallTransmission");
    Algo::Reverse(ReverseTransmission.VerticesCentimeters);
    Swap(
        ReverseTransmission.Interactions[0].EntryPointCentimeters,
        ReverseTransmission.Interactions[0].ExitPointCentimeters);
    Swap(
        ReverseTransmission.Interactions[0].Surface.Contributors[0].EntryPointCentimeters,
        ReverseTransmission.Interactions[0].Surface.Contributors[0].ExitPointCentimeters);
    FTRIADRFPathEvaluation ReverseEvaluation;
    TestTrue(TEXT("Reverse paired transmission evaluates"), Model.EvaluatePath(ReverseTransmission, 2.4, ReverseEvaluation));
    TestEqual(TEXT("Bidirectional material loss is symmetric"), ReverseEvaluation.InteractionLossDb, ObliqueTransmission.InteractionLossDb, 1e-10);
    TestEqual(TEXT("Bidirectional path length is symmetric"), ReverseEvaluation.PathLengthMeters, ObliqueTransmission.PathLengthMeters, 1e-10);

    const FTRIADRFPathCandidate ReflectionPath = MakeReflectionPath();
    FTRIADRFPathEvaluation ReflectionEvaluation;
    TestTrue(TEXT("Geometry-bound specular reflection evaluates"), Model.EvaluatePath(ReflectionPath, 5.8, ReflectionEvaluation));
    TestEqual(TEXT("Reflection plus empirical grazing control"), ReflectionEvaluation.InteractionLossDb, 7.0, 1e-9);
    TestTrue(TEXT("Bounce path is longer than direct path"), ReflectionEvaluation.PathLengthMeters > 1000.0);
    if (ReflectionEvaluation.InteractionEvaluations.Num() == 1)
    {
        TestEqual(TEXT("Reflection is bound to vertex one"), ReflectionEvaluation.InteractionEvaluations[0].VertexIndex, 1);
    }

    FTRIADRFPathCandidate FalseOpenPath = MakeDirectPath();
    FalseOpenPath.Interactions.Add(MakeTransmissionPath().Interactions[0]);
    FTRIADRFPathEvaluation RejectedOpen;
    TestFalse(TEXT("Open aperture cannot hide a material interaction"), Model.EvaluatePath(FalseOpenPath, 2.4, RejectedOpen));

    FTRIADRFPathCandidate BentTransmission = MakeTransmissionPath();
    BentTransmission.PathId = TEXT("BentTransmissionMustFail");
    BentTransmission.VerticesCentimeters.Insert(FVector(50000.0, 1000.0, 0.0), 1);
    BentTransmission.ClearSegmentWitnesses = {MakeWitness(0), MakeWitness(1)};
    FTRIADRFPathEvaluation RejectedBentTransmission;
    TestFalse(TEXT("Transmission cannot hide an unmodelled direction change"), Model.EvaluatePath(BentTransmission, 2.4, RejectedBentTransmission));

    FTRIADRFPathCandidate ExtraBounce = MakeReflectionPath();
    ExtraBounce.PathId = TEXT("ExtraBounceMustFail");
    ExtraBounce.VerticesCentimeters.Insert(FVector(75000.0, 10000.0, 0.0), 2);
    ExtraBounce.ClearSegmentWitnesses = {MakeWitness(0), MakeWitness(1), MakeWitness(2)};
    FTRIADRFPathEvaluation RejectedExtraBounce;
    TestFalse(TEXT("One-reflection path cannot contain an extra bend"), Model.EvaluatePath(ExtraBounce, 5.8, RejectedExtraBounce));

    FTRIADRFPathCandidate WrongBouncePoint = MakeReflectionPath();
    WrongBouncePoint.PathId = TEXT("WrongBouncePointMustFail");
    WrongBouncePoint.Interactions[0].SurfacePointCentimeters.X += 10.0;
    FTRIADRFPathEvaluation RejectedWrongBouncePoint;
    TestFalse(TEXT("Reflection hit must bind to the bounce vertex"), Model.EvaluatePath(WrongBouncePoint, 5.8, RejectedWrongBouncePoint));

    FTRIADRFPathCandidate NonSpecular = MakeReflectionPath();
    NonSpecular.PathId = TEXT("NonSpecularMustFail");
    NonSpecular.VerticesCentimeters[2].Y =
        NonSpecular.VerticesCentimeters[1].Y;
    FTRIADRFPathEvaluation RejectedNonSpecular;
    TestFalse(TEXT("Reflection must satisfy the reflection law"), Model.EvaluatePath(NonSpecular, 5.8, RejectedNonSpecular));

    FTRIADRFPathCandidate OffSegmentTransmission = MakeTransmissionPath();
    OffSegmentTransmission.PathId = TEXT("OffSegmentTransmissionMustFail");
    OffSegmentTransmission.Interactions[0].EntryPointCentimeters.Y = 10.0;
    FTRIADRFPathEvaluation RejectedOffSegment;
    TestFalse(TEXT("Transmission entry must lie on its declared segment"), Model.EvaluatePath(OffSegmentTransmission, 2.4, RejectedOffSegment));

    FTRIADRFPathCandidate MissingClearWitness = MakeDirectPath();
    MissingClearWitness.PathId = TEXT("MissingClearWitnessMustFail");
    MissingClearWitness.ClearSegmentWitnesses.Reset();
    FTRIADRFPathEvaluation RejectedMissingWitness;
    TestFalse(TEXT("Every leg requires one clear witness"), Model.EvaluatePath(MissingClearWitness, 2.4, RejectedMissingWitness));

    FTRIADRFPathCandidate BlockedClearWitness = MakeDirectPath();
    BlockedClearWitness.PathId = TEXT("BlockedClearWitnessMustFail");
    BlockedClearWitness.ClearSegmentWitnesses[0].bNoUnmodelledBlockingHit = false;
    FTRIADRFPathEvaluation RejectedBlockedWitness;
    TestFalse(TEXT("A failed clear-leg witness fails closed"), Model.EvaluatePath(BlockedClearWitness, 2.4, RejectedBlockedWitness));

    FTRIADRFPathCandidate UncalibratedPath = MakeTransmissionPath(false);
    UncalibratedPath.PathId = TEXT("UncalibratedLookdevPath");
    FTRIADRFPathEvaluation Uncalibrated;
    TestTrue(TEXT("Explicit uncalibrated coefficients remain usable for lookdev"), Model.EvaluatePath(UncalibratedPath, 2.4, Uncalibrated));
    TestTrue(
        TEXT("Uncalibrated state is preserved"),
        Uncalibrated.MaterialCalibrationState ==
            ETRIADRFMaterialCalibrationState::Uncalibrated);
    TestFalse(TEXT("Uncalibrated coefficients cannot claim survey readiness"), Uncalibrated.bReadyForSurveyTruth);

    FTRIADRFPathCandidate MissingCalibrationReceipt = MakeTransmissionPath();
    MissingCalibrationReceipt.PathId = TEXT("MissingCalibrationReceiptMustFail");
    MissingCalibrationReceipt.Interactions[0].Surface.CalibrationProvenanceId.Reset();
    FTRIADRFPathEvaluation RejectedMissingReceipt;
    TestFalse(TEXT("Calibrated state requires provenance"), Model.EvaluatePath(MissingCalibrationReceipt, 2.4, RejectedMissingReceipt));

    FTRIADRFPathCandidate NotApplicableInteraction = MakeTransmissionPath();
    NotApplicableInteraction.PathId = TEXT("NotApplicableInteractionMustFail");
    NotApplicableInteraction.Interactions[0].Surface.CalibrationState =
        ETRIADRFMaterialCalibrationState::NotApplicable;
    FTRIADRFPathEvaluation RejectedNotApplicable;
    TestFalse(TEXT("A real interaction cannot claim calibration is not applicable"), Model.EvaluatePath(NotApplicableInteraction, 2.4, RejectedNotApplicable));

    FTRIADRFPathEvaluation OutsideMaterialBand;
    TestFalse(TEXT("Out-of-band material coefficient fails closed"), Model.EvaluatePath(TransmissionPath, 8.0, OutsideMaterialBand));
    FTRIADRFPathEvaluation OutsideGlobalBand;
    TestFalse(TEXT("Frequency below configured global interval fails closed"), Model.EvaluatePath(DirectPath, 0.05, OutsideGlobalBand));
    FTRIADRFPathEvaluation AboveGlobalBand;
    TestFalse(TEXT("Frequency above configured global interval fails closed"), Model.EvaluatePath(DirectPath, 101.0, AboveGlobalBand));

    FTRIADRFPathCandidate TooShort = MakeDirectPath();
    TooShort.PathId = TEXT("TooShortForFriisEnvelope");
    TooShort.VerticesCentimeters[1] = FVector(50.0, 0.0, 0.0);
    FTRIADRFPathEvaluation RejectedTooShort;
    TestFalse(TEXT("Path below configured minimum distance fails closed"), Model.EvaluatePath(TooShort, 2.4, RejectedTooShort));
    FTRIADRFPathCandidate TooLong = MakeDirectPath();
    TooLong.PathId = TEXT("TooLongForConfiguredEnvelope");
    TooLong.VerticesCentimeters[1] = FVector(600000.0, 0.0, 0.0);
    FTRIADRFPathEvaluation RejectedTooLong;
    TestFalse(TEXT("Path above configured maximum distance fails closed"), Model.EvaluatePath(TooLong, 2.4, RejectedTooLong));

    FTRIADRFPathCandidate AtCompiledPathCeiling = MakeDirectPath();
    AtCompiledPathCeiling.PathId = TEXT("AtCompiledPathCeiling");
    AtCompiledPathCeiling.VerticesCentimeters[1] =
        FVector(500000.0, 0.0, 0.0);
    FTRIADRFPathEvaluation AtCompiledCeilingEvaluation;
    TestTrue(
        TEXT("The inclusive compiled 5 km and 100 GHz ceilings evaluate"),
        Model.EvaluatePath(
            AtCompiledPathCeiling,
            100.0,
            AtCompiledCeilingEvaluation));

    FTRIADRFPathCandidate TooManyInteractions = MakeTransmissionPath();
    TooManyInteractions.PathId = TEXT("TooManyInteractionsMustFail");
    const FTRIADRFPathInteraction InteractionTemplate =
        TooManyInteractions.Interactions[0];
    while (TooManyInteractions.Interactions.Num() <= Model.GetLimits().MaximumInteractionsPerPath)
    {
        TooManyInteractions.Interactions.Add(InteractionTemplate);
    }
    FTRIADRFPathEvaluation RejectedTooManyInteractions;
    TestFalse(TEXT("Interaction count above configured maximum fails before evaluation"), Model.EvaluatePath(TooManyInteractions, 2.4, RejectedTooManyInteractions));

    const FString OversizedIdentifier = FString::ChrN(257, TEXT('X'));
    const auto TestPreflightRejectsWithoutCandidateTelemetry =
        [this, &Model](
            const TCHAR* Description,
            const FTRIADRFPathCandidate& Candidate)
        {
            FTRIADRFPathEvaluation Evaluation;
            TestFalse(
                Description,
                Model.EvaluatePath(Candidate, 2.4, Evaluation));
            TestTrue(
                TEXT("Rejected resource envelope copies no candidate identifiers or arrays into telemetry"),
                Evaluation.PathId.IsEmpty() &&
                    Evaluation.GeometryQueryId.IsEmpty() &&
                    Evaluation.ClearSegmentWitnessIds.IsEmpty() &&
                    Evaluation.CalibrationProvenanceIds.IsEmpty() &&
                    Evaluation.InteractionEvaluations.IsEmpty());
        };

    FTRIADRFPathCandidate OversizedPathId = MakeDirectPath();
    OversizedPathId.PathId = OversizedIdentifier;
    TestPreflightRejectsWithoutCandidateTelemetry(
        TEXT("Oversized path identifier fails in preflight"),
        OversizedPathId);

    FTRIADRFPathCandidate OversizedGeometryQueryId = MakeDirectPath();
    OversizedGeometryQueryId.GeometryQueryId = OversizedIdentifier;
    TestPreflightRejectsWithoutCandidateTelemetry(
        TEXT("Oversized geometry-query identifier fails in preflight"),
        OversizedGeometryQueryId);

    FTRIADRFPathCandidate OversizedWitnessId = MakeDirectPath();
    OversizedWitnessId.ClearSegmentWitnesses[0].WitnessId =
        OversizedIdentifier;
    TestPreflightRejectsWithoutCandidateTelemetry(
        TEXT("Oversized clear-witness identifier fails in preflight"),
        OversizedWitnessId);

    FTRIADRFPathCandidate OversizedSurfaceId = MakeTransmissionPath();
    OversizedSurfaceId.Interactions[0].Surface.SurfaceId =
        OversizedIdentifier;
    TestPreflightRejectsWithoutCandidateTelemetry(
        TEXT("Oversized surface identifier fails in preflight"),
        OversizedSurfaceId);

    FTRIADRFPathCandidate OversizedContributorId = MakeTransmissionPath();
    OversizedContributorId.Interactions[0].Surface.Contributors[0].SolidId =
        OversizedIdentifier;
    TestPreflightRejectsWithoutCandidateTelemetry(
        TEXT("Oversized contributor identifier fails in preflight"),
        OversizedContributorId);

    FTRIADRFPathCandidate MissingContributors = MakeTransmissionPath();
    MissingContributors.Interactions[0].Surface.Contributors.Reset();
    FTRIADRFPathEvaluation RejectedMissingContributors;
    TestFalse(
        TEXT("Transmission without authoritative contributors fails closed"),
        Model.EvaluatePath(
            MissingContributors,
            2.4,
            RejectedMissingContributors));

    FTRIADRFPathCandidate OversizedMaterialId = MakeTransmissionPath();
    OversizedMaterialId.Interactions[0].Surface.MaterialId =
        OversizedIdentifier;
    TestPreflightRejectsWithoutCandidateTelemetry(
        TEXT("Oversized material identifier fails in preflight"),
        OversizedMaterialId);

    FTRIADRFPathCandidate OversizedCalibrationId = MakeTransmissionPath();
    OversizedCalibrationId.Interactions[0].Surface.CalibrationProvenanceId =
        OversizedIdentifier;
    TestPreflightRejectsWithoutCandidateTelemetry(
        TEXT("Oversized calibration identifier fails in preflight"),
        OversizedCalibrationId);

    FTRIADRFPathCandidate TooManyVerticesForPreflight = MakeReflectionPath();
    TooManyVerticesForPreflight.VerticesCentimeters.Add(
        FVector(110000.0, 0.0, 0.0));
    TooManyVerticesForPreflight.ClearSegmentWitnesses.Add(MakeWitness(2));
    TestPreflightRejectsWithoutCandidateTelemetry(
        TEXT("Over-limit vertex array fails before telemetry copies"),
        TooManyVerticesForPreflight);

    FTRIADRFPathCandidate TooManyWitnessesForPreflight = MakeDirectPath();
    TooManyWitnessesForPreflight.ClearSegmentWitnesses.Add(MakeWitness(1));
    TestPreflightRejectsWithoutCandidateTelemetry(
        TEXT("Over-limit witness array fails before telemetry copies"),
        TooManyWitnessesForPreflight);

    TArray<FTRIADRFPathCandidate> CandidateBatch;
    for (int32 Index = 0; Index < Model.GetLimits().MaximumCandidatePaths; ++Index)
    {
        FTRIADRFPathCandidate Candidate = MakeDirectPath();
        Candidate.PathId = FString::Printf(TEXT("BoundedCandidate_%d"), Index);
        CandidateBatch.Add(MoveTemp(Candidate));
    }
    TArray<FTRIADRFPathEvaluation> BatchEvaluations;
    TestTrue(TEXT("Candidate count exactly at configured maximum evaluates"), Model.EvaluatePaths(CandidateBatch, 2.4, BatchEvaluations, Error));
    TestEqual(TEXT("Bounded batch returns every result"), BatchEvaluations.Num(), Model.GetLimits().MaximumCandidatePaths);
    FTRIADRFPathCandidate ExcessCandidate = MakeDirectPath();
    ExcessCandidate.PathId = TEXT("ExcessCandidate");
    CandidateBatch.Add(MoveTemp(ExcessCandidate));
    TestFalse(TEXT("Candidate count above configured maximum fails closed"), Model.EvaluatePaths(CandidateBatch, 2.4, BatchEvaluations, Error));
    TestEqual(TEXT("Failed candidate batch leaves no partial output"), BatchEvaluations.Num(), 0);

    TArray<FTRIADRFPathCandidate> InvalidIdentifierBatch;
    InvalidIdentifierBatch.Add(MakeDirectPath());
    FTRIADRFPathCandidate InvalidSecondCandidate = MakeDirectPath();
    InvalidSecondCandidate.PathId = TEXT("InvalidSecondCandidate");
    InvalidSecondCandidate.GeometryQueryId = OversizedIdentifier;
    InvalidIdentifierBatch.Add(MoveTemp(InvalidSecondCandidate));
    TestFalse(
        TEXT("A later oversized identifier rejects the whole batch in preflight"),
        Model.EvaluatePaths(
            InvalidIdentifierBatch,
            2.4,
            BatchEvaluations,
            Error));
    TestEqual(
        TEXT("Batch preflight emits no earlier candidate telemetry"),
        BatchEvaluations.Num(),
        0);

    TArray<double> BoundedPowers;
    BoundedPowers.Init(-30.0, Model.GetLimits().MaximumReceivedPowerCount);
    double CombinedPowerDbm = 0.0;
    TestTrue(TEXT("Power count exactly at configured maximum combines"), Model.CombineIncoherentReceivedPowersDbm(BoundedPowers, CombinedPowerDbm, Error));
    BoundedPowers.Add(-30.0);
    TestFalse(TEXT("Power count above configured maximum fails closed"), Model.CombineIncoherentReceivedPowersDbm(BoundedPowers, CombinedPowerDbm, Error));

    TestTrue(
        TEXT("Two finite path powers combine incoherently"),
        Model.CombineIncoherentReceivedPowersDbm(
            {-30.0, -30.0},
            CombinedPowerDbm,
            Error));
    TestEqual(TEXT("Equal powers add three dB"), CombinedPowerDbm, -26.989700043360187, 1e-10);
    TestFalse(
        TEXT("Empty multipath set fails closed"),
        Model.CombineIncoherentReceivedPowersDbm(
            {},
            CombinedPowerDbm,
            Error));

    FTRIADRFModelLimits InvalidLimits;
    InvalidLimits.MaximumCandidatePaths = 5000;
    const FTRIADDeterministicRFInteractionModel InvalidModel(InvalidLimits);
    FTRIADRFPathEvaluation InvalidLimitEvaluation;
    TestFalse(TEXT("Configured limits cannot exceed compiled safety ceilings"), InvalidModel.EvaluatePath(DirectPath, 2.4, InvalidLimitEvaluation));

    FTRIADRFModelLimits ExcessFrequencyLimits;
    ExcessFrequencyLimits.MaximumFrequencyGHz = 100.000001;
    const FTRIADDeterministicRFInteractionModel ExcessFrequencyModel(
        ExcessFrequencyLimits);
    FTRIADRFPathEvaluation ExcessFrequencyEvaluation;
    TestFalse(
        TEXT("Configured frequency cannot exceed the compiled 100 GHz ceiling"),
        ExcessFrequencyModel.EvaluatePath(
            DirectPath,
            2.4,
            ExcessFrequencyEvaluation));

    FTRIADRFModelLimits ExcessPathLengthLimits;
    ExcessPathLengthLimits.MaximumPathLengthMeters = 5000.000001;
    const FTRIADDeterministicRFInteractionModel ExcessPathLengthModel(
        ExcessPathLengthLimits);
    FTRIADRFPathEvaluation ExcessPathLengthEvaluation;
    TestFalse(
        TEXT("Configured path length cannot exceed the compiled 5 km ceiling"),
        ExcessPathLengthModel.EvaluatePath(
            DirectPath,
            2.4,
            ExcessPathLengthEvaluation));
    return true;
}

#endif
