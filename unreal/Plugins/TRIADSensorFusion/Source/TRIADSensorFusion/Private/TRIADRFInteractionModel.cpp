#include "TRIADRFInteractionModel.h"

namespace
{
constexpr double SpeedOfLightMetersPerSecond = 299792458.0;
constexpr double MinimumIncidenceCosine = 0.000001;
constexpr int32 HardMaximumCandidatePaths = 4096;
constexpr int32 HardMaximumVerticesPerPath = 3;
constexpr int32 HardMaximumInteractionsPerPath = 32;
constexpr int32 HardMaximumContributorsPerInteraction = 32;
constexpr int32 HardMaximumReceivedPowerCount = 4096;
constexpr int32 MaximumIdentifierCharacters = 256;
constexpr double HardMaximumFrequencyGHz = 100.0;
constexpr double HardMaximumPathLengthMeters = 5000.0;
constexpr double HardMaximumPositionToleranceCentimeters = 1.0;
constexpr double HardMaximumSpecularDirectionTolerance = 0.01;
const TCHAR* const ModelSemantics =
    TEXT("PARAMETRIC_LOOKDEV_DIRECT_STRAIGHT_TRANSMISSION_SINGLE_SPECULAR_REFLECTION_V2_INCOHERENT_ONLY");
const TCHAR* const ReadinessSemantics =
    TEXT("NOT_SURVEY_READY_EXTERNAL_IMMUTABLE_ACCEPTANCE_CONTEXT_NOT_IMPLEMENTED");

bool IsFiniteNonNegative(double Value)
{
    return FMath::IsFinite(Value) && Value >= 0.0;
}

bool IsFinitePoint(const FVector& Point)
{
    return FMath::IsFinite(Point.X) &&
        FMath::IsFinite(Point.Y) &&
        FMath::IsFinite(Point.Z);
}

bool IsBoundedIdentifier(const FString& Value)
{
    return !Value.IsEmpty() && Value.Len() <= MaximumIdentifierCharacters;
}

bool IsBoundedOptionalIdentifier(const FString& Value)
{
    return Value.Len() <= MaximumIdentifierCharacters;
}

/**
 * Reject every unbounded candidate-owned array and identifier before any of
 * those values can be copied into output telemetry. This is deliberately a
 * resource-envelope preflight; the later geometry/material checks still own
 * the semantic validation.
 */
bool PreflightCandidateResourceEnvelope(
    const FTRIADRFPathCandidate& Candidate,
    const FTRIADRFModelLimits& Limits,
    FString& OutError)
{
    if (Candidate.VerticesCentimeters.Num() < 2 ||
        Candidate.VerticesCentimeters.Num() > Limits.MaximumVerticesPerPath)
    {
        OutError = TEXT("RF path vertex count is outside the configured bound.");
        return false;
    }
    if (Candidate.Interactions.Num() > Limits.MaximumInteractionsPerPath)
    {
        OutError = TEXT("RF path interaction count exceeds the configured bound.");
        return false;
    }
    const int32 SegmentCount = Candidate.VerticesCentimeters.Num() - 1;
    if (Candidate.ClearSegmentWitnesses.Num() != SegmentCount)
    {
        OutError = TEXT("RF path requires exactly one clear witness for every bounded segment.");
        return false;
    }
    if (!IsBoundedIdentifier(Candidate.PathId) ||
        !IsBoundedIdentifier(Candidate.GeometryQueryId))
    {
        OutError = TEXT("RF path and geometry-query identifiers must contain 1..256 characters.");
        return false;
    }
    for (const FTRIADRFClearSegmentWitness& Witness :
         Candidate.ClearSegmentWitnesses)
    {
        if (!IsBoundedIdentifier(Witness.WitnessId))
        {
            OutError = TEXT("RF clear-segment witness identifiers must contain 1..256 characters.");
            return false;
        }
    }
    for (const FTRIADRFPathInteraction& Interaction : Candidate.Interactions)
    {
        if (Interaction.Surface.Contributors.Num() >
            Limits.MaximumContributorsPerInteraction)
        {
            OutError = TEXT("RF interaction contributor count exceeds the configured bound.");
            return false;
        }
        if (!IsBoundedIdentifier(Interaction.Surface.SurfaceId) ||
            !IsBoundedIdentifier(Interaction.Surface.MaterialId) ||
            !IsBoundedOptionalIdentifier(Interaction.Surface.SolidId) ||
            !IsBoundedOptionalIdentifier(Interaction.Surface.ProfileId) ||
            !IsBoundedOptionalIdentifier(Interaction.Surface.SourceClass) ||
            !IsBoundedOptionalIdentifier(
                Interaction.Surface.UncertaintyClass) ||
            !IsBoundedOptionalIdentifier(
                Interaction.Surface.CoefficientSelectionSemantics) ||
            !IsBoundedOptionalIdentifier(
                Interaction.Surface.CalibrationProvenanceId))
        {
            OutError = TEXT("RF surface/material identifiers and optional solid/profile/source/uncertainty/selection/calibration provenance must be bounded to 256 characters.");
            return false;
        }
        for (const FTRIADRFInteractionContributor& Contributor :
             Interaction.Surface.Contributors)
        {
            if (!IsBoundedIdentifier(Contributor.SolidId) ||
                !IsBoundedIdentifier(Contributor.EntrySurfaceId) ||
                !IsBoundedIdentifier(Contributor.ExitSurfaceId) ||
                !IsBoundedIdentifier(Contributor.SourceClass) ||
                !IsBoundedIdentifier(Contributor.UncertaintyClass) ||
                !IsFinitePoint(Contributor.EntryPointCentimeters) ||
                !IsFinitePoint(Contributor.ExitPointCentimeters))
            {
                OutError = TEXT("RF interaction contributors require finite points and non-empty identifiers bounded to 256 characters.");
                return false;
            }
        }
    }
    OutError.Reset();
    return true;
}

bool ValidateUnitNormal(
    const FVector& Normal,
    const FTRIADRFModelLimits& Limits,
    FVector& OutUnitNormal,
    FString& OutError)
{
    OutUnitNormal = FVector::ZeroVector;
    if (Normal.ContainsNaN())
    {
        OutError = TEXT("RF interaction surface normal must be finite.");
        return false;
    }
    const double SizeSquared = Normal.SizeSquared();
    if (!FMath::IsFinite(SizeSquared) ||
        FMath::Abs(SizeSquared - 1.0) > Limits.SpecularDirectionTolerance)
    {
        OutError = TEXT("RF interaction surface normal must be unit length within the configured tolerance.");
        return false;
    }
    OutUnitNormal = Normal.GetSafeNormal();
    return true;
}

bool ComputePointParameterOnSegment(
    const FVector& Point,
    const FVector& Start,
    const FVector& End,
    double PositionToleranceCentimeters,
    double& OutParameter,
    FString& OutError)
{
    OutParameter = 0.0;
    if (Point.ContainsNaN())
    {
        OutError = TEXT("RF interaction point must be finite.");
        return false;
    }
    const FVector Segment = End - Start;
    const double SegmentLengthSquared = Segment.SizeSquared();
    if (!FMath::IsFinite(SegmentLengthSquared) || SegmentLengthSquared <= 0.0)
    {
        OutError = TEXT("RF interaction cannot bind to a zero-length segment.");
        return false;
    }
    const double SegmentLength = FMath::Sqrt(SegmentLengthSquared);
    const double ParameterTolerance =
        PositionToleranceCentimeters / SegmentLength;
    OutParameter = FVector::DotProduct(Point - Start, Segment) /
        SegmentLengthSquared;
    if (!FMath::IsFinite(OutParameter) ||
        OutParameter < -ParameterTolerance ||
        OutParameter > 1.0 + ParameterTolerance)
    {
        OutError = TEXT("RF interaction point lies outside its declared path segment.");
        return false;
    }
    const FVector ClosestPoint = Start + Segment * OutParameter;
    if (FVector::Distance(Point, ClosestPoint) > PositionToleranceCentimeters)
    {
        OutError = TEXT("RF interaction point does not lie on its declared path segment.");
        return false;
    }
    return true;
}

bool ValidateSurfaceProfile(
    const FTRIADRFSurfaceProfile& Surface,
    double FrequencyGHz,
    double IncidenceCosine,
    FString& OutError)
{
    if (!IsBoundedIdentifier(Surface.SurfaceId) ||
        !IsBoundedIdentifier(Surface.MaterialId))
    {
        OutError = TEXT("RF surface and material identifiers must contain 1..256 characters.");
        return false;
    }
    if (!FMath::IsFinite(Surface.MinimumFrequencyGHz) ||
        !FMath::IsFinite(Surface.MaximumFrequencyGHz) ||
        Surface.MinimumFrequencyGHz <= 0.0 ||
        Surface.MaximumFrequencyGHz < Surface.MinimumFrequencyGHz)
    {
        OutError = TEXT("RF surface frequency bounds must be finite, positive, and ordered.");
        return false;
    }
    if (FrequencyGHz < Surface.MinimumFrequencyGHz ||
        FrequencyGHz > Surface.MaximumFrequencyGHz)
    {
        OutError = FString::Printf(
            TEXT("RF surface '%s' has no coefficients for %.6f GHz."),
            *Surface.SurfaceId,
            FrequencyGHz);
        return false;
    }
    if (!FMath::IsFinite(Surface.MinimumIncidenceCosine) ||
        !FMath::IsFinite(Surface.MaximumIncidenceCosine) ||
        Surface.MinimumIncidenceCosine < MinimumIncidenceCosine ||
        Surface.MaximumIncidenceCosine < Surface.MinimumIncidenceCosine ||
        Surface.MaximumIncidenceCosine > 1.0)
    {
        OutError = TEXT("RF surface incidence-cosine bounds must be finite and ordered within (0, 1].");
        return false;
    }
    if (IncidenceCosine < Surface.MinimumIncidenceCosine ||
        IncidenceCosine > Surface.MaximumIncidenceCosine)
    {
        OutError = FString::Printf(
            TEXT("RF surface '%s' has no coefficients for incidence cosine %.9f."),
            *Surface.SurfaceId,
            IncidenceCosine);
        return false;
    }
    if (!IsFiniteNonNegative(Surface.PairedBoundaryTransmissionLossDb) ||
        !IsFiniteNonNegative(Surface.BulkAttenuationDbPerMeter) ||
        !IsFiniteNonNegative(Surface.ReflectionLossDb) ||
        !IsFiniteNonNegative(Surface.EmpiricalGrazingReflectionLossDb))
    {
        OutError = TEXT("RF interaction coefficients must be finite and non-negative.");
        return false;
    }
    switch (Surface.CalibrationState)
    {
    case ETRIADRFMaterialCalibrationState::Uncalibrated:
        break;
    case ETRIADRFMaterialCalibrationState::Calibrated:
        if (!IsBoundedIdentifier(Surface.CalibrationProvenanceId))
        {
            OutError = TEXT("A calibrated RF surface requires a 1..256 character calibration provenance identifier.");
            return false;
        }
        break;
    case ETRIADRFMaterialCalibrationState::NotApplicable:
        OutError = TEXT("A material interaction cannot use the NotApplicable calibration state.");
        return false;
    default:
        OutError = TEXT("Unknown RF material calibration state.");
        return false;
    }
    return true;
}

bool ValidateTransmissionContributors(
    const FTRIADRFPathInteraction& Interaction,
    const FVector& Start,
    const FVector& End,
    const FTRIADRFModelLimits& Limits,
    double InteractionEntryParameter,
    double InteractionExitParameter,
    FString& OutError)
{
    const TArray<FTRIADRFInteractionContributor>& Contributors =
        Interaction.Surface.Contributors;
    if (Contributors.IsEmpty() ||
        Contributors.Num() > Limits.MaximumContributorsPerInteraction)
    {
        OutError = TEXT("RF transmission requires 1..MaximumContributorsPerInteraction ordered physical contributors.");
        return false;
    }
    const FTRIADRFInteractionContributor& First = Contributors[0];
    if (Interaction.Surface.SolidId != First.SolidId ||
        Interaction.Surface.SurfaceId != First.EntrySurfaceId ||
        Interaction.Surface.SourceClass != First.SourceClass ||
        Interaction.Surface.UncertaintyClass != First.UncertaintyClass)
    {
        OutError = TEXT("RF transmission singular provenance fields must identify the authoritative contributor list's first physical entry.");
        return false;
    }

    const double SegmentLengthCentimeters = FVector::Distance(Start, End);
    const double ParameterTolerance =
        Limits.GeometryPositionToleranceCentimeters /
        SegmentLengthCentimeters;
    TSet<FString> SolidIds;
    double PreviousExitParameter = InteractionEntryParameter;
    for (int32 ContributorIndex = 0;
         ContributorIndex < Contributors.Num();
         ++ContributorIndex)
    {
        const FTRIADRFInteractionContributor& Contributor =
            Contributors[ContributorIndex];
        if (SolidIds.Contains(Contributor.SolidId))
        {
            OutError = TEXT("RF transmission contributor solid identifiers must be unique within one material span.");
            return false;
        }
        SolidIds.Add(Contributor.SolidId);
        double EntryParameter = 0.0;
        double ExitParameter = 0.0;
        if (!ComputePointParameterOnSegment(
                Contributor.EntryPointCentimeters,
                Start,
                End,
                Limits.GeometryPositionToleranceCentimeters,
                EntryParameter,
                OutError) ||
            !ComputePointParameterOnSegment(
                Contributor.ExitPointCentimeters,
                Start,
                End,
                Limits.GeometryPositionToleranceCentimeters,
                ExitParameter,
                OutError))
        {
            return false;
        }
        if (ExitParameter - EntryParameter <= ParameterTolerance ||
            (ContributorIndex == 0 &&
                FMath::Abs(EntryParameter - InteractionEntryParameter) >
                    ParameterTolerance) ||
            (ContributorIndex > 0 &&
                FMath::Abs(EntryParameter - PreviousExitParameter) >
                    ParameterTolerance))
        {
            OutError = TEXT("RF transmission contributors must be positive, contiguous, and ordered from the interaction entry.");
            return false;
        }
        PreviousExitParameter = ExitParameter;
    }
    if (FMath::Abs(PreviousExitParameter - InteractionExitParameter) >
        ParameterTolerance)
    {
        OutError = TEXT("RF transmission contributor chain must end at the interaction exit.");
        return false;
    }
    return true;
}

bool ValidateClearSegmentWitnesses(
    const FTRIADRFPathCandidate& Candidate,
    FString& OutError)
{
    const int32 SegmentCount = Candidate.VerticesCentimeters.Num() - 1;
    if (!IsBoundedIdentifier(Candidate.GeometryQueryId))
    {
        OutError = TEXT("RF path requires a bounded geometry-query identifier.");
        return false;
    }
    if (Candidate.ClearSegmentWitnesses.Num() != SegmentCount)
    {
        OutError = TEXT("RF path requires exactly one clear witness for every segment.");
        return false;
    }
    TSet<FString> WitnessIds;
    for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
    {
        const FTRIADRFClearSegmentWitness& Witness =
            Candidate.ClearSegmentWitnesses[SegmentIndex];
        if (Witness.SegmentIndex != SegmentIndex ||
            !IsBoundedIdentifier(Witness.WitnessId) ||
            WitnessIds.Contains(Witness.WitnessId))
        {
            OutError = TEXT("RF clear-segment witnesses must be ordered, uniquely identified, and segment-bound.");
            return false;
        }
        if (!Witness.bNoUnmodelledBlockingHit)
        {
            OutError = TEXT("RF path segment has no fail-closed clear-leg witness.");
            return false;
        }
        WitnessIds.Add(Witness.WitnessId);
    }
    return true;
}

bool ValidatePathShape(
    const FTRIADRFPathCandidate& Candidate,
    int32 TransmissionCount,
    int32 ReflectionCount,
    FString& OutError)
{
    switch (Candidate.Kind)
    {
    case ETRIADRFPathKind::Direct:
        if (Candidate.VerticesCentimeters.Num() != 2 ||
            !Candidate.Interactions.IsEmpty())
        {
            OutError = TEXT("A direct/open-aperture RF path requires exactly two vertices and no interactions.");
            return false;
        }
        break;
    case ETRIADRFPathKind::Transmitted:
        if (Candidate.VerticesCentimeters.Num() != 2 ||
            TransmissionCount < 1 ||
            ReflectionCount != 0 ||
            TransmissionCount != Candidate.Interactions.Num())
        {
            OutError = TEXT("A transmitted RF path must be one straight segment with one or more transmissions and no reflections.");
            return false;
        }
        break;
    case ETRIADRFPathKind::SingleReflection:
        if (Candidate.VerticesCentimeters.Num() != 3 ||
            Candidate.Interactions.Num() != 1 ||
            ReflectionCount != 1 ||
            TransmissionCount != 0)
        {
            OutError = TEXT("A single-reflection RF path requires exactly three vertices and exactly one reflection interaction.");
            return false;
        }
        break;
    default:
        OutError = TEXT("Unknown RF path kind.");
        return false;
    }
    return true;
}
}

FTRIADDeterministicRFInteractionModel::FTRIADDeterministicRFInteractionModel(
    const FTRIADRFModelLimits& InLimits)
    : Limits(InLimits)
{
}

bool FTRIADDeterministicRFInteractionModel::ValidateLimits(
    FString& OutError) const
{
    if (Limits.MaximumCandidatePaths < 1 ||
        Limits.MaximumCandidatePaths > HardMaximumCandidatePaths ||
        Limits.MaximumVerticesPerPath < 2 ||
        Limits.MaximumVerticesPerPath > HardMaximumVerticesPerPath ||
        Limits.MaximumInteractionsPerPath < 1 ||
        Limits.MaximumInteractionsPerPath > HardMaximumInteractionsPerPath ||
        Limits.MaximumContributorsPerInteraction < 1 ||
        Limits.MaximumContributorsPerInteraction >
            HardMaximumContributorsPerInteraction ||
        Limits.MaximumReceivedPowerCount < 1 ||
        Limits.MaximumReceivedPowerCount > HardMaximumReceivedPowerCount)
    {
        OutError = TEXT("RF integer limits must be positive and within the compiled safety ceilings.");
        return false;
    }
    if (!FMath::IsFinite(Limits.MinimumFrequencyGHz) ||
        !FMath::IsFinite(Limits.MaximumFrequencyGHz) ||
        Limits.MinimumFrequencyGHz <= 0.0 ||
        Limits.MaximumFrequencyGHz < Limits.MinimumFrequencyGHz ||
        Limits.MaximumFrequencyGHz > HardMaximumFrequencyGHz ||
        !FMath::IsFinite(Limits.MinimumPathLengthMeters) ||
        !FMath::IsFinite(Limits.MaximumPathLengthMeters) ||
        Limits.MinimumPathLengthMeters <= 0.0 ||
        Limits.MaximumPathLengthMeters < Limits.MinimumPathLengthMeters ||
        Limits.MaximumPathLengthMeters > HardMaximumPathLengthMeters)
    {
        OutError = TEXT("RF frequency and path-length limits must be finite, positive, ordered, and within compiled safety ceilings.");
        return false;
    }
    if (!FMath::IsFinite(Limits.GeometryPositionToleranceCentimeters) ||
        Limits.GeometryPositionToleranceCentimeters <= 0.0 ||
        Limits.GeometryPositionToleranceCentimeters >
            HardMaximumPositionToleranceCentimeters ||
        !FMath::IsFinite(Limits.SpecularDirectionTolerance) ||
        Limits.SpecularDirectionTolerance <= 0.0 ||
        Limits.SpecularDirectionTolerance >
            HardMaximumSpecularDirectionTolerance)
    {
        OutError = TEXT("RF geometry tolerances must be positive and within compiled safety ceilings.");
        return false;
    }
    const double MinimumFriisRatio =
        4.0 * UE_DOUBLE_PI * Limits.MinimumPathLengthMeters *
        Limits.MinimumFrequencyGHz * 1.0e9 /
        SpeedOfLightMetersPerSecond;
    const double MaximumFriisRatio =
        4.0 * UE_DOUBLE_PI * Limits.MaximumPathLengthMeters *
        Limits.MaximumFrequencyGHz * 1.0e9 /
        SpeedOfLightMetersPerSecond;
    if (!FMath::IsFinite(MinimumFriisRatio) || MinimumFriisRatio < 1.0 ||
        !FMath::IsFinite(MaximumFriisRatio))
    {
        OutError = TEXT("RF configured Friis envelope is non-finite or admits a negative path-loss region.");
        return false;
    }
    OutError.Reset();
    return true;
}

bool FTRIADDeterministicRFInteractionModel::ComputeFreeSpacePathLossDb(
    double DistanceMeters,
    double FrequencyGHz,
    double& OutLossDb,
    FString& OutError)
{
    OutLossDb = 0.0;
    if (!FMath::IsFinite(DistanceMeters) || DistanceMeters <= 0.0 ||
        !FMath::IsFinite(FrequencyGHz) || FrequencyGHz <= 0.0)
    {
        OutError = TEXT("RF distance and frequency must be finite and positive.");
        return false;
    }
    const double FrequencyHz = FrequencyGHz * 1.0e9;
    const double LinearRatio =
        4.0 * UE_DOUBLE_PI * DistanceMeters * FrequencyHz /
        SpeedOfLightMetersPerSecond;
    if (!FMath::IsFinite(LinearRatio) || LinearRatio < 1.0)
    {
        OutError = TEXT("RF free-space path loss is outside the admitted Friis region.");
        return false;
    }
    OutLossDb = 20.0 * FMath::LogX(10.0, LinearRatio);
    if (!FMath::IsFinite(OutLossDb) || OutLossDb < 0.0)
    {
        OutError = TEXT("RF free-space path-loss calculation was non-finite or negative.");
        OutLossDb = 0.0;
        return false;
    }
    OutError.Reset();
    return true;
}

bool FTRIADDeterministicRFInteractionModel::EvaluatePath(
    const FTRIADRFPathCandidate& Candidate,
    double FrequencyGHz,
    FTRIADRFPathEvaluation& OutEvaluation) const
{
    OutEvaluation = FTRIADRFPathEvaluation();
    FString PreflightError;
    if (!ValidateLimits(PreflightError) ||
        !PreflightCandidateResourceEnvelope(
            Candidate,
            Limits,
            PreflightError))
    {
        OutEvaluation.Error = MoveTemp(PreflightError);
        return false;
    }

    OutEvaluation.PathId = Candidate.PathId;
    OutEvaluation.GeometryQueryId = Candidate.GeometryQueryId;
    OutEvaluation.PathKind = Candidate.Kind;
    OutEvaluation.FrequencyGHz = FrequencyGHz;
    OutEvaluation.ModelSemantics = ModelSemantics;
    OutEvaluation.ReadinessSemantics = ReadinessSemantics;
    OutEvaluation.bExternalAcceptanceContextBound = false;
    OutEvaluation.bFriisFarFieldApplicabilityValidated = false;
    OutEvaluation.bReadyForSurveyTruth = false;

    if (!FMath::IsFinite(FrequencyGHz) ||
        FrequencyGHz < Limits.MinimumFrequencyGHz ||
        FrequencyGHz > Limits.MaximumFrequencyGHz)
    {
        OutEvaluation.Error = TEXT("RF frequency is outside the configured finite operating interval.");
        return false;
    }
    double PathLengthCentimeters = 0.0;
    for (int32 Index = 1; Index < Candidate.VerticesCentimeters.Num(); ++Index)
    {
        const FVector& Start = Candidate.VerticesCentimeters[Index - 1];
        const FVector& End = Candidate.VerticesCentimeters[Index];
        if (Start.ContainsNaN() || End.ContainsNaN())
        {
            OutEvaluation.Error = TEXT("RF path contains a non-finite vertex.");
            return false;
        }
        const double SegmentCentimeters = FVector::Distance(Start, End);
        if (!FMath::IsFinite(SegmentCentimeters) || SegmentCentimeters <= 0.0)
        {
            OutEvaluation.Error = TEXT("RF path contains a zero-length or non-finite segment.");
            return false;
        }
        PathLengthCentimeters += SegmentCentimeters;
        if (!FMath::IsFinite(PathLengthCentimeters))
        {
            OutEvaluation.Error = TEXT("RF path length accumulation was non-finite.");
            return false;
        }
    }
    OutEvaluation.PathLengthMeters = PathLengthCentimeters / 100.0;
    if (OutEvaluation.PathLengthMeters < Limits.MinimumPathLengthMeters ||
        OutEvaluation.PathLengthMeters > Limits.MaximumPathLengthMeters)
    {
        OutEvaluation.Error = TEXT("RF path length is outside the configured operating interval.");
        return false;
    }
    if (!ValidateClearSegmentWitnesses(Candidate, OutEvaluation.Error))
    {
        return false;
    }
    for (const FTRIADRFClearSegmentWitness& Witness :
         Candidate.ClearSegmentWitnesses)
    {
        OutEvaluation.ClearSegmentWitnessIds.Add(Witness.WitnessId);
    }

    int32 TransmissionCount = 0;
    int32 ReflectionCount = 0;
    for (const FTRIADRFPathInteraction& Interaction : Candidate.Interactions)
    {
        switch (Interaction.Kind)
        {
        case ETRIADRFInteractionKind::Transmission:
            ++TransmissionCount;
            break;
        case ETRIADRFInteractionKind::Reflection:
            ++ReflectionCount;
            break;
        default:
            OutEvaluation.Error = TEXT("Unknown RF interaction kind.");
            return false;
        }
    }
    if (!ValidatePathShape(
            Candidate,
            TransmissionCount,
            ReflectionCount,
            OutEvaluation.Error))
    {
        return false;
    }
    if (!ComputeFreeSpacePathLossDb(
            OutEvaluation.PathLengthMeters,
            FrequencyGHz,
            OutEvaluation.FreeSpacePathLossDb,
            OutEvaluation.Error))
    {
        return false;
    }

    ETRIADRFMaterialCalibrationState AggregateCalibration =
        Candidate.Interactions.IsEmpty()
            ? ETRIADRFMaterialCalibrationState::NotApplicable
            : ETRIADRFMaterialCalibrationState::Calibrated;
    double InteractionLossDb = 0.0;
    double PreviousTransmissionExitParameter = -1.0;
    for (const FTRIADRFPathInteraction& Interaction : Candidate.Interactions)
    {
        FVector UnitNormal;
        FString GeometryError;
        if (!ValidateUnitNormal(
                Interaction.SurfaceNormal,
                Limits,
                UnitNormal,
                GeometryError))
        {
            OutEvaluation.Error = GeometryError;
            return false;
        }

        FTRIADRFInteractionEvaluation InteractionEvaluation;
        InteractionEvaluation.Kind = Interaction.Kind;
        InteractionEvaluation.SurfaceId = Interaction.Surface.SurfaceId;
        InteractionEvaluation.SolidId = Interaction.Surface.SolidId;
        InteractionEvaluation.MaterialId = Interaction.Surface.MaterialId;
        InteractionEvaluation.ProfileId = Interaction.Surface.ProfileId;
        InteractionEvaluation.SourceClass = Interaction.Surface.SourceClass;
        InteractionEvaluation.UncertaintyClass =
            Interaction.Surface.UncertaintyClass;
        InteractionEvaluation.CoefficientSelectionSemantics =
            Interaction.Surface.CoefficientSelectionSemantics;
        InteractionEvaluation.Contributors = Interaction.Surface.Contributors;
        InteractionEvaluation.CalibrationProvenanceId =
            Interaction.Surface.CalibrationProvenanceId;
        InteractionEvaluation.CalibrationState =
            Interaction.Surface.CalibrationState;
        InteractionEvaluation.MinimumFrequencyGHz =
            Interaction.Surface.MinimumFrequencyGHz;
        InteractionEvaluation.MaximumFrequencyGHz =
            Interaction.Surface.MaximumFrequencyGHz;
        InteractionEvaluation.MinimumIncidenceCosine =
            Interaction.Surface.MinimumIncidenceCosine;
        InteractionEvaluation.MaximumIncidenceCosine =
            Interaction.Surface.MaximumIncidenceCosine;
        InteractionEvaluation.PairedBoundaryTransmissionLossDb =
            Interaction.Surface.PairedBoundaryTransmissionLossDb;
        InteractionEvaluation.BulkAttenuationDbPerMeter =
            Interaction.Surface.BulkAttenuationDbPerMeter;
        InteractionEvaluation.ReflectionLossDb =
            Interaction.Surface.ReflectionLossDb;
        InteractionEvaluation.EmpiricalGrazingReflectionLossDb =
            Interaction.Surface.EmpiricalGrazingReflectionLossDb;
        double IncidenceCosine = 0.0;
        double LossDb = 0.0;

        if (Interaction.Kind == ETRIADRFInteractionKind::Transmission)
        {
            if (!Interaction.Surface.bAllowsTransmission ||
                Interaction.SegmentIndex != 0 ||
                Interaction.VertexIndex != INDEX_NONE)
            {
                OutEvaluation.Error = TEXT("RF transmission requires an enabled surface bound to the sole straight segment.");
                return false;
            }
            const FVector& Start = Candidate.VerticesCentimeters[0];
            const FVector& End = Candidate.VerticesCentimeters[1];
            double EntryParameter = 0.0;
            double ExitParameter = 0.0;
            if (!ComputePointParameterOnSegment(
                    Interaction.EntryPointCentimeters,
                    Start,
                    End,
                    Limits.GeometryPositionToleranceCentimeters,
                    EntryParameter,
                    GeometryError) ||
                !ComputePointParameterOnSegment(
                    Interaction.ExitPointCentimeters,
                    Start,
                    End,
                    Limits.GeometryPositionToleranceCentimeters,
                    ExitParameter,
                    GeometryError))
            {
                OutEvaluation.Error = GeometryError;
                return false;
            }
            const double SegmentLengthCentimeters = FVector::Distance(Start, End);
            const double ParameterTolerance =
                Limits.GeometryPositionToleranceCentimeters /
                SegmentLengthCentimeters;
            if (ExitParameter - EntryParameter <= ParameterTolerance)
            {
                OutEvaluation.Error = TEXT("RF transmission entry and exit points must be distinct and ordered along the ray.");
                return false;
            }
            if (!ValidateTransmissionContributors(
                    Interaction,
                    Start,
                    End,
                    Limits,
                    EntryParameter,
                    ExitParameter,
                    OutEvaluation.Error))
            {
                return false;
            }
            if (EntryParameter < PreviousTransmissionExitParameter -
                    ParameterTolerance)
            {
                OutEvaluation.Error = TEXT("RF transmission solids must be deterministically ordered and non-overlapping.");
                return false;
            }
            PreviousTransmissionExitParameter = ExitParameter;

            const FVector UnitDirection = (End - Start).GetSafeNormal();
            IncidenceCosine = FMath::Abs(
                FVector::DotProduct(UnitDirection, UnitNormal));
            const FVector TraversalVector =
                Interaction.ExitPointCentimeters -
                Interaction.EntryPointCentimeters;
            const double TraversalDistanceMeters =
                TraversalVector.Size() / 100.0;
            const double NormalThicknessMeters = FMath::Abs(
                FVector::DotProduct(TraversalVector, UnitNormal)) / 100.0;
            if (!FMath::IsFinite(IncidenceCosine) ||
                IncidenceCosine < MinimumIncidenceCosine ||
                !FMath::IsFinite(TraversalDistanceMeters) ||
                TraversalDistanceMeters <= 0.0 ||
                !FMath::IsFinite(NormalThicknessMeters) ||
                NormalThicknessMeters <= 0.0)
            {
                OutEvaluation.Error = TEXT("RF transmission geometry has invalid incidence, traversal, or paired-solid thickness.");
                return false;
            }
            if (!ValidateSurfaceProfile(
                    Interaction.Surface,
                    FrequencyGHz,
                    IncidenceCosine,
                    OutEvaluation.Error))
            {
                return false;
            }
            LossDb =
                Interaction.Surface.PairedBoundaryTransmissionLossDb +
                Interaction.Surface.BulkAttenuationDbPerMeter *
                    TraversalDistanceMeters;
            InteractionEvaluation.SegmentIndex = Interaction.SegmentIndex;
            InteractionEvaluation.NormalThicknessMeters =
                NormalThicknessMeters;
            InteractionEvaluation.TraversalDistanceMeters =
                TraversalDistanceMeters;
        }
        else
        {
            if (!Interaction.Surface.Contributors.IsEmpty())
            {
                OutEvaluation.Error = TEXT("RF reflection cannot declare entry/exit contributor spans.");
                return false;
            }
            if (!Interaction.Surface.bAllowsReflection ||
                Interaction.SegmentIndex != INDEX_NONE ||
                Interaction.VertexIndex != 1)
            {
                OutEvaluation.Error = TEXT("RF reflection requires an enabled surface bound to the sole interior vertex.");
                return false;
            }
            const FVector& Previous = Candidate.VerticesCentimeters[0];
            const FVector& Bounce = Candidate.VerticesCentimeters[1];
            const FVector& Next = Candidate.VerticesCentimeters[2];
            if (Interaction.SurfacePointCentimeters.ContainsNaN() ||
                FVector::Distance(
                    Interaction.SurfacePointCentimeters,
                    Bounce) > Limits.GeometryPositionToleranceCentimeters)
            {
                OutEvaluation.Error = TEXT("RF reflection surface point is not bound to the declared bounce vertex.");
                return false;
            }
            const FVector IncidentDirection =
                (Bounce - Previous).GetSafeNormal();
            const FVector OutgoingDirection =
                (Next - Bounce).GetSafeNormal();
            const FVector ReflectedDirection =
                IncidentDirection -
                2.0 * FVector::DotProduct(IncidentDirection, UnitNormal) *
                    UnitNormal;
            if (FVector::Distance(
                    ReflectedDirection,
                    OutgoingDirection) > Limits.SpecularDirectionTolerance)
            {
                OutEvaluation.Error = TEXT("RF reflection path does not satisfy the configured specular-direction tolerance.");
                return false;
            }
            IncidenceCosine = FMath::Abs(
                FVector::DotProduct(IncidentDirection, UnitNormal));
            if (!FMath::IsFinite(IncidenceCosine) ||
                IncidenceCosine < MinimumIncidenceCosine)
            {
                OutEvaluation.Error = TEXT("RF reflection incidence cosine is invalid.");
                return false;
            }
            if (!ValidateSurfaceProfile(
                    Interaction.Surface,
                    FrequencyGHz,
                    IncidenceCosine,
                    OutEvaluation.Error))
            {
                return false;
            }
            LossDb =
                Interaction.Surface.ReflectionLossDb +
                Interaction.Surface.EmpiricalGrazingReflectionLossDb *
                    (1.0 - IncidenceCosine);
            InteractionEvaluation.VertexIndex = Interaction.VertexIndex;
        }

        if (!FMath::IsFinite(LossDb) || LossDb < 0.0)
        {
            OutEvaluation.Error = TEXT("RF interaction loss was non-finite or negative.");
            return false;
        }
        if (Interaction.Surface.CalibrationState !=
            ETRIADRFMaterialCalibrationState::Calibrated)
        {
            AggregateCalibration =
                ETRIADRFMaterialCalibrationState::Uncalibrated;
        }
        else if (!OutEvaluation.CalibrationProvenanceIds.Contains(
                     Interaction.Surface.CalibrationProvenanceId))
        {
            OutEvaluation.CalibrationProvenanceIds.Add(
                Interaction.Surface.CalibrationProvenanceId);
        }
        InteractionEvaluation.IncidenceCosine = IncidenceCosine;
        InteractionEvaluation.LossDb = LossDb;
        OutEvaluation.InteractionEvaluations.Add(InteractionEvaluation);
        InteractionLossDb += LossDb;
        if (!FMath::IsFinite(InteractionLossDb))
        {
            OutEvaluation.Error = TEXT("RF interaction-loss accumulation was non-finite.");
            return false;
        }
    }

    OutEvaluation.MaterialCalibrationState = AggregateCalibration;
    OutEvaluation.InteractionLossDb = InteractionLossDb;
    OutEvaluation.TotalPropagationLossDb =
        OutEvaluation.FreeSpacePathLossDb + InteractionLossDb;
    if (!FMath::IsFinite(OutEvaluation.TotalPropagationLossDb) ||
        OutEvaluation.TotalPropagationLossDb < 0.0)
    {
        OutEvaluation.Error = TEXT("RF total propagation loss was non-finite or negative.");
        return false;
    }
    OutEvaluation.bExternalAcceptanceContextBound = false;
    OutEvaluation.bFriisFarFieldApplicabilityValidated = false;
    OutEvaluation.bReadyForSurveyTruth = false;
    OutEvaluation.bValid = true;
    OutEvaluation.Error.Reset();
    return true;
}

bool FTRIADDeterministicRFInteractionModel::EvaluatePaths(
    const TArray<FTRIADRFPathCandidate>& Candidates,
    double FrequencyGHz,
    TArray<FTRIADRFPathEvaluation>& OutEvaluations,
    FString& OutError) const
{
    OutEvaluations.Reset();
    if (!ValidateLimits(OutError))
    {
        return false;
    }
    if (!FMath::IsFinite(FrequencyGHz) ||
        FrequencyGHz < Limits.MinimumFrequencyGHz ||
        FrequencyGHz > Limits.MaximumFrequencyGHz)
    {
        OutError = TEXT("RF batch frequency is outside the configured finite operating interval.");
        return false;
    }
    if (Candidates.Num() > Limits.MaximumCandidatePaths)
    {
        OutError = TEXT("RF candidate batch exceeds the configured path-count bound.");
        return false;
    }

    TSet<FString> PathIds;
    for (const FTRIADRFPathCandidate& Candidate : Candidates)
    {
        FString CandidatePreflightError;
        if (!PreflightCandidateResourceEnvelope(
                Candidate,
                Limits,
                CandidatePreflightError))
        {
            OutError = MoveTemp(CandidatePreflightError);
            return false;
        }
        if (PathIds.Contains(Candidate.PathId))
        {
            OutError = TEXT("RF candidate batch path identifiers must be unique.");
            return false;
        }
        PathIds.Add(Candidate.PathId);
    }

    OutEvaluations.Reserve(Candidates.Num());
    for (const FTRIADRFPathCandidate& Candidate : Candidates)
    {
        FTRIADRFPathEvaluation Evaluation;
        if (!EvaluatePath(Candidate, FrequencyGHz, Evaluation))
        {
            OutError = FString::Printf(
                TEXT("RF candidate '%s' failed: %s"),
                *Candidate.PathId,
                *Evaluation.Error);
            OutEvaluations.Reset();
            return false;
        }
        OutEvaluations.Add(MoveTemp(Evaluation));
    }
    OutError.Reset();
    return true;
}

bool FTRIADDeterministicRFInteractionModel::CombineIncoherentReceivedPowersDbm(
    const TArray<double>& ReceivedPowersDbm,
    double& OutCombinedPowerDbm,
    FString& OutError) const
{
    OutCombinedPowerDbm = 0.0;
    if (!ValidateLimits(OutError))
    {
        return false;
    }
    if (ReceivedPowersDbm.IsEmpty())
    {
        OutError = TEXT("At least one received RF path power is required.");
        return false;
    }
    if (ReceivedPowersDbm.Num() > Limits.MaximumReceivedPowerCount)
    {
        OutError = TEXT("Received RF path-power count exceeds the configured bound.");
        return false;
    }

    double MaximumPowerDbm = -TNumericLimits<double>::Max();
    for (const double PowerDbm : ReceivedPowersDbm)
    {
        if (!FMath::IsFinite(PowerDbm))
        {
            OutError = TEXT("Received RF path powers must be finite.");
            return false;
        }
        MaximumPowerDbm = FMath::Max(MaximumPowerDbm, PowerDbm);
    }

    double NormalizedLinearSum = 0.0;
    for (const double PowerDbm : ReceivedPowersDbm)
    {
        NormalizedLinearSum +=
            FMath::Pow(10.0, (PowerDbm - MaximumPowerDbm) / 10.0);
    }
    OutCombinedPowerDbm = MaximumPowerDbm +
        10.0 * FMath::LogX(10.0, NormalizedLinearSum);
    if (!FMath::IsFinite(OutCombinedPowerDbm))
    {
        OutError = TEXT("Incoherent RF power combination was non-finite.");
        OutCombinedPowerDbm = 0.0;
        return false;
    }
    OutError.Reset();
    return true;
}
