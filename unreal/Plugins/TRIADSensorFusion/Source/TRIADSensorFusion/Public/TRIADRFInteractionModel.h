#pragma once

#include "CoreMinimal.h"

/** Path types admitted by the bounded parametric RF look-development model. */
enum class ETRIADRFPathKind : uint8
{
    Direct,
    Transmitted,
    SingleReflection
};

enum class ETRIADRFInteractionKind : uint8
{
    Transmission,
    Reflection
};

/**
 * Calibration applicability is tri-state. In particular, a direct path has no
 * material interaction, so calling its material coefficients "calibrated"
 * would be a misleading vacuous truth.
 */
enum class ETRIADRFMaterialCalibrationState : uint8
{
    NotApplicable,
    Uncalibrated,
    Calibrated
};

/**
 * Operational and resource limits are copied into the deterministic model at
 * construction. They are not survey acceptance criteria; they only bound the
 * parametric calculation and its admitted Friis operating envelope.
 */
struct TRIADSENSORFUSION_API FTRIADRFModelLimits
{
    int32 MaximumCandidatePaths = 64;
    int32 MaximumVerticesPerPath = 3;
    int32 MaximumInteractionsPerPath = 32;
    int32 MaximumContributorsPerInteraction = 32;
    int32 MaximumReceivedPowerCount = 64;
    double MinimumFrequencyGHz = 0.1;
    double MaximumFrequencyGHz = 100.0;
    double MinimumPathLengthMeters = 1.0;
    double MaximumPathLengthMeters = 5000.0;
    double GeometryPositionToleranceCentimeters = 0.1;
    double SpecularDirectionTolerance = 0.0001;
};

/**
 * Ordered physical-solid provenance for one evaluated material span.
 *
 * Same-material adjacent solids share one paired-boundary loss, but each
 * physical entry/exit pair remains independently attributable here.
 */
struct TRIADSENSORFUSION_API FTRIADRFInteractionContributor
{
    FString SolidId;
    FString EntrySurfaceId;
    FString ExitSurfaceId;
    FString SourceClass;
    FString UncertaintyClass;
    FVector EntryPointCentimeters = FVector::ZeroVector;
    FVector ExitPointCentimeters = FVector::ZeroVector;
};

/**
 * Explicit empirical coefficients for one separately authored RF surface.
 *
 * These scalar controls are intentionally a transparent look-development
 * model, not a Fresnel, layered-medium, polarization, or diffraction solver.
 * A calibrated state records provenance for the coefficients only; it cannot
 * make a path or mesh survey-authoritative.
 */
struct TRIADSENSORFUSION_API FTRIADRFSurfaceProfile
{
    FString SurfaceId;
    /** Compatibility view: physical first-entry owner; Contributors is authoritative. */
    FString SolidId;
    FString MaterialId;
    /** Exact catalog profile selected for this frequency/incidence interval. */
    FString ProfileId;
    FString SourceClass;
    FString UncertaintyClass;
    /** Explicit selection/interpolation rule used to obtain these coefficients. */
    FString CoefficientSelectionSemantics;
    /** Ordered, bounded provenance for every physical solid in this span. */
    TArray<FTRIADRFInteractionContributor> Contributors;
    double MinimumFrequencyGHz = 0.0;
    double MaximumFrequencyGHz = 0.0;
    double MinimumIncidenceCosine = 0.0;
    double MaximumIncidenceCosine = 0.0;
    /** Total empirical entry-plus-exit boundary loss for one paired solid. */
    double PairedBoundaryTransmissionLossDb = 0.0;
    double BulkAttenuationDbPerMeter = 0.0;
    double ReflectionLossDb = 0.0;
    /** Linear empirical grazing control; not a physical rough-surface model. */
    double EmpiricalGrazingReflectionLossDb = 0.0;
    bool bAllowsTransmission = false;
    bool bAllowsReflection = false;
    ETRIADRFMaterialCalibrationState CalibrationState =
        ETRIADRFMaterialCalibrationState::Uncalibrated;
    FString CalibrationProvenanceId;
};

/**
 * One geometry-bound material interaction.
 *
 * Transmission uses SegmentIndex plus ordered entry/exit points on that path
 * segment. Reflection uses VertexIndex plus an exact surface point and unit
 * normal. Unused indices must remain INDEX_NONE.
 */
struct TRIADSENSORFUSION_API FTRIADRFPathInteraction
{
    ETRIADRFInteractionKind Kind = ETRIADRFInteractionKind::Transmission;
    FTRIADRFSurfaceProfile Surface;
    int32 SegmentIndex = INDEX_NONE;
    int32 VertexIndex = INDEX_NONE;
    FVector EntryPointCentimeters = FVector::ZeroVector;
    FVector ExitPointCentimeters = FVector::ZeroVector;
    FVector SurfacePointCentimeters = FVector::ZeroVector;
    FVector SurfaceNormal = FVector::ZeroVector;
};

/**
 * Per-segment output of the geometry query. It is traceability evidence for
 * this parametric candidate, not an owner/survey acceptance receipt.
 */
struct TRIADSENSORFUSION_API FTRIADRFClearSegmentWitness
{
    FString WitnessId;
    int32 SegmentIndex = INDEX_NONE;
    bool bNoUnmodelledBlockingHit = false;
};

/**
 * One candidate polyline supplied by a dedicated RF geometry implementation.
 * Open apertures are represented by a Direct path with no interactions.
 */
struct TRIADSENSORFUSION_API FTRIADRFPathCandidate
{
    FString PathId;
    FString GeometryQueryId;
    ETRIADRFPathKind Kind = ETRIADRFPathKind::Direct;
    TArray<FVector> VerticesCentimeters;
    TArray<FTRIADRFPathInteraction> Interactions;
    TArray<FTRIADRFClearSegmentWitness> ClearSegmentWitnesses;
};

/** Computed telemetry for one geometry-bound material interaction. */
struct TRIADSENSORFUSION_API FTRIADRFInteractionEvaluation
{
    ETRIADRFInteractionKind Kind = ETRIADRFInteractionKind::Transmission;
    FString SurfaceId;
    FString SolidId;
    FString MaterialId;
    FString ProfileId;
    FString SourceClass;
    FString UncertaintyClass;
    FString CoefficientSelectionSemantics;
    /** Authoritative ordered physical-solid provenance copied from the candidate. */
    TArray<FTRIADRFInteractionContributor> Contributors;
    FString CalibrationProvenanceId;
    ETRIADRFMaterialCalibrationState CalibrationState =
        ETRIADRFMaterialCalibrationState::Uncalibrated;
    int32 SegmentIndex = INDEX_NONE;
    int32 VertexIndex = INDEX_NONE;
    double MinimumFrequencyGHz = 0.0;
    double MaximumFrequencyGHz = 0.0;
    double MinimumIncidenceCosine = 0.0;
    double MaximumIncidenceCosine = 0.0;
    double PairedBoundaryTransmissionLossDb = 0.0;
    double BulkAttenuationDbPerMeter = 0.0;
    double ReflectionLossDb = 0.0;
    double EmpiricalGrazingReflectionLossDb = 0.0;
    double NormalThicknessMeters = 0.0;
    double TraversalDistanceMeters = 0.0;
    double IncidenceCosine = 0.0;
    double LossDb = 0.0;
};

struct TRIADSENSORFUSION_API FTRIADRFPathEvaluation
{
    bool bValid = false;
    FString PathId;
    FString GeometryQueryId;
    ETRIADRFPathKind PathKind = ETRIADRFPathKind::Direct;
    ETRIADRFMaterialCalibrationState MaterialCalibrationState =
        ETRIADRFMaterialCalibrationState::NotApplicable;
    /** No acceptance-context type exists in V2, so this is fail-closed false. */
    bool bExternalAcceptanceContextBound = false;
    /** Antenna aperture data is absent, so true far-field validity is unknown. */
    bool bFriisFarFieldApplicabilityValidated = false;
    /** Parametric evaluation must never promote itself to survey truth. */
    bool bReadyForSurveyTruth = false;
    double FrequencyGHz = 0.0;
    double PathLengthMeters = 0.0;
    double FreeSpacePathLossDb = 0.0;
    double InteractionLossDb = 0.0;
    double TotalPropagationLossDb = 0.0;
    TArray<FString> ClearSegmentWitnessIds;
    TArray<FString> CalibrationProvenanceIds;
    TArray<FTRIADRFInteractionEvaluation> InteractionEvaluations;
    FString ModelSemantics;
    FString ReadinessSemantics;
    FString Error;
};

/** Runtime extension point: geometry produces paths; the model scores them. */
class TRIADSENSORFUSION_API ITRIADRFInteractionModel
{
public:
    virtual ~ITRIADRFInteractionModel() = default;

    virtual bool EvaluatePath(
        const FTRIADRFPathCandidate& Candidate,
        double FrequencyGHz,
        FTRIADRFPathEvaluation& OutEvaluation) const = 0;

    /** Batch entry point that enforces the candidate-count bound. */
    virtual bool EvaluatePaths(
        const TArray<FTRIADRFPathCandidate>& Candidates,
        double FrequencyGHz,
        TArray<FTRIADRFPathEvaluation>& OutEvaluations,
        FString& OutError) const = 0;
};

/**
 * Runtime extension point for a dedicated RF collision layer.
 *
 * Implementations must honor Limits, pair solid entry/exit intersections,
 * emit one clear witness for every path segment, preserve explicit apertures,
 * and return deterministic ordering. Visual/Nanite geometry and a generic
 * Visibility trace do not satisfy this interface by themselves.
 */
class TRIADSENSORFUSION_API ITRIADRFGeometryQuery
{
public:
    virtual ~ITRIADRFGeometryQuery() = default;

    virtual bool BuildPathCandidates(
        const FVector& TransmitterCentimeters,
        const FVector& ReceiverCentimeters,
        double FrequencyGHz,
        const FTRIADRFModelLimits& Limits,
        TArray<FTRIADRFPathCandidate>& OutCandidates,
        FString& OutError) const = 0;
};

/**
 * Bounded parametric implementation for direct, straight transmission, and
 * exactly one specular reflection. It is deliberately look-development only.
 */
class TRIADSENSORFUSION_API FTRIADDeterministicRFInteractionModel final
    : public ITRIADRFInteractionModel
{
public:
    explicit FTRIADDeterministicRFInteractionModel(
        const FTRIADRFModelLimits& InLimits = FTRIADRFModelLimits());

    virtual bool EvaluatePath(
        const FTRIADRFPathCandidate& Candidate,
        double FrequencyGHz,
        FTRIADRFPathEvaluation& OutEvaluation) const override;

    virtual bool EvaluatePaths(
        const TArray<FTRIADRFPathCandidate>& Candidates,
        double FrequencyGHz,
        TArray<FTRIADRFPathEvaluation>& OutEvaluations,
        FString& OutError) const override;

    /** Incoherent power sum only; bounded and not coherent phase synthesis. */
    bool CombineIncoherentReceivedPowersDbm(
        const TArray<double>& ReceivedPowersDbm,
        double& OutCombinedPowerDbm,
        FString& OutError) const;

    const FTRIADRFModelLimits& GetLimits() const
    {
        return Limits;
    }

private:
    static bool ComputeFreeSpacePathLossDb(
        double DistanceMeters,
        double FrequencyGHz,
        double& OutLossDb,
        FString& OutError);

    bool ValidateLimits(FString& OutError) const;

    FTRIADRFModelLimits Limits;
};
