#pragma once

#include "CoreMinimal.h"
#include "TRIADRFInteractionModel.h"

/**
 * Classification of an oriented, non-tangent boundary crossing.
 *
 * The canonical RF meshes use outward winding.  Entry therefore means that
 * the finite segment direction has a negative dot product with the outward
 * normal; Exit means that the dot product is positive.
 */
enum class ETRIADRFBoundaryCrossing : uint8
{
    Entry,
    Exit
};

/** Resource ceilings applied before canonical JSON arrays are retained. */
struct TRIADSENSORFUSION_API FTRIADRFIndexedGeometryLoadLimits
{
    int32 MaximumJsonBytesPerDocument = 64 * 1024 * 1024;
    int32 MaximumVertices = 1000000;
    int32 MaximumTriangles = 1000000;
    int32 MaximumSolids = 100000;
    int32 MaximumSurfaces = 200000;
    int32 MaximumWitnesses = 100000;
    int32 MaximumMaterials = 4096;
    int32 MaximumProfilesPerMaterial = 64;
    int32 MaximumBVHDepth = 64;
    int32 MaximumTrianglesPerLeaf = 8;
    int64 MaximumSelfIntersectionCandidateChecks = 10000000;
    int64 MaximumCrossSolidTriangleCandidateChecks = 10000000;
    int64 MaximumContainmentTriangleChecks = 20000000;
};

/** Immutable provenance retained from the two hash-bound source documents. */
struct TRIADSENSORFUSION_API FTRIADRFIndexedGeometryMetadata
{
    FString GeometrySchemaVersion;
    FString GeometryQueryId;
    FString GeometryRevision;
    FString GeometryStatus;
    FString GeometrySha256;
    FString ContractSha256;
    FString MaterialCatalogSchemaVersion;
    FString MaterialCatalogId;
    FString MaterialCatalogSha256;
    FString CoordinateSemantics;
    FString RuntimeSemantics;
    FString ModeledCoverageId;
    FString ModeledCoverageScope;
    FString ModeledCoverageShape;
    FString ModeledCoverageFiniteSegmentPolicy;
    FString ModeledCoverageOutsideDomainPolicy;
    FVector ModeledCoverageMinimumCentimeters = FVector::ZeroVector;
    FVector ModeledCoverageMaximumCentimeters = FVector::ZeroVector;
    bool bModeledCoverageCoversOneKilometreAoi = false;
    bool bModeledCoverageCoversSurroundings = false;
    bool bModeledCoverageSurveyControlled = false;
    bool bModeledCoverageFieldValidated = false;
    bool bHasClosedWgs84GeodesicCircleStudyDomain = false;
    FString StudyDomainId;
    FString StudyDomainType;
    FString StudyDomainDistanceMethod;
    FString StudyDomainConfigurationReferenceName;
    FString StudyDomainConfigurationShape;
    FVector2D StudyDomainCenterWgs84Degrees = FVector2D::ZeroVector;
    double StudyDomainRadiusMeters = 0.0;
    int32 StudyDomainPerimeterSampleCount = 0;
    double StudyDomainPerimeterStartAzimuthDegrees = 0.0;
    double StudyDomainPerimeterStepDegrees = 0.0;
    FString StudyDomainPerimeterAzimuthConvention;
    FString StudyDomainSourceGeodeticCrs;
    FString StudyDomainProjectedConstructionCrs;
    FString StudyDomainLogicalSystem;
    FString StudyDomainMembershipFrame;
    bool bStudyDomainConfigurationEnabled = false;
    bool bStudyDomainSurveyRegistered = false;
    bool bStudyDomainContainedByLoaderCoverageAabb = false;
    bool bStudyDomainCoversEntireRadiusCircle = false;
    bool bStudyDomainCornersOutsideCircleExcluded = false;
};

/** Stable catalog metadata for one loaded material. */
struct TRIADSENSORFUSION_API FTRIADRFIndexedMaterialMetadata
{
    FString MaterialId;
    FString ProvenanceId;
    FString CalibrationState;
    FString UncertaintyClass;
    int32 RuntimeProfileCount = 0;
};

/** Stable finite-solid metadata, with canonical bounds converted to Unreal. */
struct TRIADSENSORFUSION_API FTRIADRFIndexedSolidMetadata
{
    FString SolidId;
    FString PrimitiveType;
    FString Role;
    FString MaterialId;
    FString BoundaryRole;
    FString SourceClass;
    FString UncertaintyClass;
    FString ApertureId;
    FString ApertureState;
    FVector BoundsMinimumCentimeters = FVector::ZeroVector;
    FVector BoundsMaximumCentimeters = FVector::ZeroVector;
    int32 VertexStart = INDEX_NONE;
    int32 VertexCount = 0;
    int32 TriangleStart = INDEX_NONE;
    int32 TriangleCount = 0;
};

/** Stable surface metadata, including optional aperture provenance. */
struct TRIADSENSORFUSION_API FTRIADRFIndexedSurfaceMetadata
{
    FString SurfaceId;
    FString SolidId;
    FString MaterialId;
    FString BoundaryRole;
    FString SourceClass;
    FString UncertaintyClass;
    FString ApertureId;
    FString ApertureState;
    FVector OutwardNormal = FVector::ZeroVector;
    int32 TriangleStart = INDEX_NONE;
    int32 TriangleCount = 0;
};

/** One deterministic boundary hit returned in increasing segment order. */
struct TRIADSENSORFUSION_API FTRIADRFIndexedSegmentHit
{
    double SegmentParameter = 0.0;
    double DistanceCentimeters = 0.0;
    FVector PointCentimeters = FVector::ZeroVector;
    FVector OutwardNormal = FVector::ZeroVector;
    ETRIADRFBoundaryCrossing Crossing = ETRIADRFBoundaryCrossing::Entry;
    int32 CanonicalTriangleIndex = INDEX_NONE;
    FString TriangleId;
    FString SolidId;
    FString SurfaceId;
    FString MaterialId;
    FString SolidRole;
    FString BoundaryRole;
    FString SourceClass;
    FString UncertaintyClass;
    FString ApertureId;
    FString ApertureState;
};

/**
 * Deterministic CPU geometry query for canonical indexed RF meshes.
 *
 * Loading is transactional and fail-closed: every load attempt first clears
 * the previous scene, and an invalid document leaves the query unavailable.
 * The canonical logical frame is right-handed Z-up metres; vertices are
 * converted to Unreal X/+east, Y/-approach, Z/+up centimetres and triangle
 * winding is reversed exactly once so outward normals remain outward.
 * Finite solids may retain the TightV1 AXIS_ALIGNED_BOX primitive or declare
 * INDEXED_CLOSED_POLYHEDRON, whose indexed boundary is additionally checked
 * for self-intersection and positive-volume contact with other solids.
 *
 * After loading, TraceSegment is read-only and returns ordered finite-segment
 * entry/exit crossings.  BuildPathCandidates currently emits the bounded
 * direct or straight-transmission candidate only.  Reflection candidate
 * enumeration remains a separate later integration stage.
 */
class TRIADSENSORFUSION_API FTRIADRFIndexedGeometryQuery final
    : public ITRIADRFGeometryQuery
{
public:
    explicit FTRIADRFIndexedGeometryQuery(
        const FTRIADRFIndexedGeometryLoadLimits& InLoadLimits =
            FTRIADRFIndexedGeometryLoadLimits());
    virtual ~FTRIADRFIndexedGeometryQuery();

    FTRIADRFIndexedGeometryQuery(const FTRIADRFIndexedGeometryQuery&) = delete;
    FTRIADRFIndexedGeometryQuery& operator=(
        const FTRIADRFIndexedGeometryQuery&) = delete;

    /** Load exact LF/UTF-8 source files and verify the catalog hash binding. */
    bool LoadFromJsonFiles(
        const FString& GeometryJsonPath,
        const FString& MaterialCatalogJsonPath,
        FString& OutError);

    /**
     * Load each exact LF/UTF-8 source file once, hash and parse those same
     * buffers, and require both configured SHA-256 values before commit.
     */
    bool LoadFromJsonFiles(
        const FString& GeometryJsonPath,
        const FString& MaterialCatalogJsonPath,
        const FString& ExpectedGeometrySha256,
        const FString& ExpectedMaterialCatalogSha256,
        FString& OutError);

    /** Load UTF-16 Unreal strings after canonical UTF-8 re-encoding. */
    bool LoadFromJsonStrings(
        const FString& GeometryJson,
        const FString& MaterialCatalogJson,
        FString& OutError);

    /** Portable SHA-256 used for exact UTF-8 catalog binding and diagnostics. */
    static bool ComputeCanonicalJsonSha256(
        const FString& JsonText,
        FString& OutSha256,
        FString& OutError);

    void Reset();
    bool IsReady() const;

    int32 GetVertexCount() const;
    int32 GetTriangleCount() const;
    int32 GetSolidCount() const;
    int32 GetSurfaceCount() const;
    int32 GetMaterialCount() const;
    int32 GetBVHNodeCount() const;
    const FTRIADRFIndexedGeometryMetadata& GetMetadata() const;

    bool TryGetMaterialMetadata(
        const FString& MaterialId,
        FTRIADRFIndexedMaterialMetadata& OutMetadata) const;
    bool TryGetSolidMetadata(
        const FString& SolidId,
        FTRIADRFIndexedSolidMetadata& OutMetadata) const;
    bool TryGetSurfaceMetadata(
        const FString& SurfaceId,
        FTRIADRFIndexedSurfaceMetadata& OutMetadata) const;

    /**
     * Test the exact closed, hash-bound modeled-coverage envelope.
     *
     * Canonical geometry declares an axis-aligned convex domain. Both finite
     * segment endpoints must be inside or on that closed box; for a convex
     * box this also proves the complete finite segment is contained. Coverage
     * identity, scope, and claim flags are retained as metadata for scenario
     * pinning. Outside queries fail and are never clear/direct paths.
     */
    bool IsFiniteSegmentWithinModeledCoverage(
        const FVector& StartCentimeters,
        const FVector& EndCentimeters,
        FString& OutError) const;

    /** Apply the optional closed WGS84 circle in addition to the closed AABB. */
    bool IsFiniteSegmentWithinAdmittedStudyDomain(
        const FVector& StartCentimeters,
        const FVector& EndCentimeters,
        const FVector& StartLongitudeLatitudeHeight,
        const FVector& EndLongitudeLatitudeHeight,
        double& OutStartSignedDistanceMeters,
        double& OutEndSignedDistanceMeters,
        FString& OutError) const;

    /**
     * Return every non-tangent, non-endpoint boundary crossing in increasing
     * segment parameter order. The complete finite segment must first pass
     * the modeled-coverage check. Coplanar and edge/corner-ambiguous contact
     * is rejected instead of being interpreted as clear air.
     */
    bool TraceSegment(
        const FVector& StartCentimeters,
        const FVector& EndCentimeters,
        double PositionToleranceCentimeters,
        TArray<FTRIADRFIndexedSegmentHit>& OutHits,
        FString& OutError) const;

    /**
     * Build the bounded direct or straight-transmission candidate.
     *
     * Consecutive solid intervals whose opposed boundaries coincide within
     * GeometryPositionToleranceCentimeters are one unresolved interface.
     * Same-material intervals are collapsed into one continuous material span
     * so the paired-boundary prior is applied exactly once. A coincident
     * mixed-material interface is rejected: this catalog has per-material slab
     * priors, not a sound material-to-material transition coefficient.
     */
    virtual bool BuildPathCandidates(
        const FVector& TransmitterCentimeters,
        const FVector& ReceiverCentimeters,
        double FrequencyGHz,
        const FTRIADRFModelLimits& Limits,
        TArray<FTRIADRFPathCandidate>& OutCandidates,
        FString& OutError) const override;

private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
    FTRIADRFIndexedGeometryLoadLimits LoadLimits;
};
