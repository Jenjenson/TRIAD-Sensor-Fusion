#if WITH_DEV_AUTOMATION_TESTS

#include "TRIADRFIndexedGeometryQuery.h"

#include <limits>
#include "TRIADGeodesy.h"

#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
FString Sha256Utf8(const FString& Text)
{
    FString Hash;
    FString Error;
    return FTRIADRFIndexedGeometryQuery::ComputeCanonicalJsonSha256(
        Text, Hash, Error)
        ? Hash
        : FString();
}

FString MakeProfiles(bool bAmbiguousBoundary, bool bAllowsTransmission)
{
    return bAmbiguousBoundary
        ? TEXT(
            "[{\"profileId\":\"PROFILE_LOW\",\"minimumFrequencyGHz\":0.1,\"maximumFrequencyGHz\":2.4,"
            "\"minimumIncidenceCosine\":0.000001,\"maximumIncidenceCosine\":1.0,"
            "\"pairedBoundaryTransmissionLossDb\":5.0,\"bulkAttenuationDbPerMeter\":10.0,"
            "\"reflectionLossDb\":6.0,\"empiricalGrazingReflectionLossDb\":2.0,"
            "\"allowsTransmission\":true,\"allowsReflection\":true},"
            "{\"profileId\":\"PROFILE_HIGH\",\"minimumFrequencyGHz\":2.4,\"maximumFrequencyGHz\":10.0,"
            "\"minimumIncidenceCosine\":0.000001,\"maximumIncidenceCosine\":1.0,"
            "\"pairedBoundaryTransmissionLossDb\":7.0,\"bulkAttenuationDbPerMeter\":12.0,"
            "\"reflectionLossDb\":8.0,\"empiricalGrazingReflectionLossDb\":3.0,"
            "\"allowsTransmission\":true,\"allowsReflection\":true}]")
        : FString::Printf(
            TEXT(
                "[{\"profileId\":\"PROFILE_ALL\",\"minimumFrequencyGHz\":0.1,\"maximumFrequencyGHz\":10.0,"
                "\"minimumIncidenceCosine\":0.000001,\"maximumIncidenceCosine\":1.0,"
                "\"pairedBoundaryTransmissionLossDb\":5.0,\"bulkAttenuationDbPerMeter\":10.0,"
                "\"reflectionLossDb\":6.0,\"empiricalGrazingReflectionLossDb\":2.0,"
                "\"allowsTransmission\":%s,\"allowsReflection\":true}]"),
            bAllowsTransmission ? TEXT("true") : TEXT("false"));
}

FString MakeMaterial(const FString& MaterialId, const FString& Profiles)
{
    return FString::Printf(
        TEXT(
            "{\"materialId\":\"%s\",\"modelClass\":\"FINITE_SLAB_DIELECTRIC\","
            "\"calibrationState\":\"UNCALIBRATED_ASSUMPTION\",\"provenanceId\":\"TEST_PRIOR\","
            "\"uncertaintyClass\":\"HIGH\",\"frequencyResponse\":{"
            "\"frequencyGHz\":[1.0,6.0],\"relativePermittivityReal\":[4.0,3.5],"
            "\"lossTangent\":[0.04,0.06],\"conductivitySiemensPerMeter\":[0.01,0.02]},"
            "\"runtimeInteractionProfileV2\":{"
            "\"modelSemantics\":\"PARAMETRIC_LOOKDEV_DIRECT_STRAIGHT_TRANSMISSION_SINGLE_SPECULAR_REFLECTION_V2_INCOHERENT_ONLY\","
            "\"calibrationState\":\"UNCALIBRATED\",\"calibrationProvenanceId\":\"TEST_RUNTIME_PRIOR\","
            "\"profiles\":%s}}"),
        *MaterialId,
        *Profiles);
}

FString MakeCatalog(bool bAmbiguousBoundary = false, bool bAllowsTransmission = true)
{
    const FString Material = MakeMaterial(
        TEXT("MAT_TEST"),
        MakeProfiles(bAmbiguousBoundary, bAllowsTransmission));
    return FString::Printf(
        TEXT(
            "{\"schemaVersion\":\"triad.rf_material_catalog.v1\","
            "\"catalogId\":\"rf-test-catalog\",\"status\":\"UNCALIBRATED_ASSUMPTION_PRIORS\","
            "\"materials\":[%s]}"),
        *Material);
}

FString MakeMixedMaterialCatalog()
{
    const FString Profiles = MakeProfiles(false, true);
    const FString First = MakeMaterial(TEXT("MAT_TEST"), Profiles);
    const FString Second = MakeMaterial(TEXT("MAT_OTHER"), Profiles);
    return FString::Printf(
        TEXT(
            "{\"schemaVersion\":\"triad.rf_material_catalog.v1\","
            "\"catalogId\":\"rf-test-catalog-mixed\",\"status\":\"UNCALIBRATED_ASSUMPTION_PRIORS\","
            "\"materials\":[%s,%s]}"),
        *First,
        *Second);
}

FString MakeGeometry(const FString& CatalogSha256)
{
    return FString::Printf(
        TEXT(
            "{\"schemaVersion\":\"triad.istana_public_view.rf_geometry.v1\","
            "\"assetId\":\"rf-test-cube\",\"revision\":\"TEST_V1\","
            "\"status\":\"SIMULATION_READY_ASSUMPTION_BOUND\","
            "\"contractSha256\":\"0000000000000000000000000000000000000000000000000000000000000000\","
            "\"materialCatalogSha256\":\"%s\","
            "\"coordinateContract\":{\"logicalSystem\":\"RIGHT_HANDED_Z_UP_LOCAL_METRES_APPROACH_PLUS_Y\"},"
            "\"modeledCoverageEnvelope\":{"
            "\"coverageId\":\"RF_COVERAGE_TEST_HERO\",\"scope\":\"MAIN_HERO_ONLY_ASSUMPTION_BOUND\","
            "\"shape\":\"AXIS_ALIGNED_BOX\",\"boundsMeters\":{\"min\":[-1,-1,-1],\"max\":[2,1,2]},"
            "\"boundaryInclusion\":\"CLOSED\","
            "\"finiteSegmentPolicy\":\"BOTH_ENDPOINTS_AND_ENTIRE_FINITE_SEGMENT_MUST_BE_INSIDE_CLOSED_ENVELOPE\","
            "\"outsideDomainPolicy\":\"REJECT_QUERY_NEVER_EMIT_CLEAR_OR_DIRECT_PATH\","
            "\"derivation\":\"MINIMUM_CLOSED_AABB_CONTAINING_CANONICAL_VERTICES_AND_WITNESS_ENDPOINTS\","
            "\"coversCanonicalHeroGeometry\":true,\"coversOfflineWitnessEndpoints\":true,"
            "\"coversOneKilometreAoi\":false,\"coversSurroundings\":false,"
            "\"surveyControlled\":false,\"fieldValidated\":false,"
            "\"explicitExclusions\":[\"TEST_FIXTURE_OUTSIDE_DOMAIN\"]},"
            "\"witnesses\":[{\"witnessId\":\"WIT_TEST_COVERAGE_EXTENTS\","
            "\"startMeters\":[-1,-1,-1],\"endMeters\":[2,1,2]}],"
            "\"verticesMeters\":[[0,0,0],[1,0,0],[1,1,0],[0,1,0],[0,0,1],[1,0,1],[1,1,1],[0,1,1]],"
            "\"triangles\":[[0,2,1],[0,3,2],[4,5,6],[4,6,7],[0,1,5],[0,5,4],"
            "[3,7,6],[3,6,2],[0,4,7],[0,7,3],[1,2,6],[1,6,5]],"
            "\"solids\":[{\"solidId\":\"SOLID_CUBE\",\"role\":\"TEST_CUBE\",\"materialId\":\"MAT_TEST\","
            "\"boundaryRole\":\"OPAQUE_ENVELOPE\",\"sourceClass\":\"TEST_FIXTURE\",\"uncertaintyClass\":\"HIGH\","
            "\"apertureId\":null,\"apertureState\":null,"
            "\"vertexStart\":0,\"vertexCount\":8,\"triangleStart\":0,\"triangleCount\":12,"
            "\"boundsMeters\":{\"min\":[0,0,0],\"max\":[1,1,1]},"
            "\"primitive\":{\"type\":\"AXIS_ALIGNED_BOX\",\"minMeters\":[0,0,0],\"maxMeters\":[1,1,1]}}],"
            "\"surfaces\":["
            "{\"surfaceId\":\"SURFACE_BOTTOM\",\"solidId\":\"SOLID_CUBE\",\"materialId\":\"MAT_TEST\",\"boundaryRole\":\"OPAQUE_ENVELOPE\",\"sourceClass\":\"TEST_FIXTURE\",\"uncertaintyClass\":\"HIGH\",\"apertureId\":null,\"apertureState\":null,\"outwardNormal\":[0,0,-1],\"triangleStart\":0,\"triangleCount\":2},"
            "{\"surfaceId\":\"SURFACE_TOP\",\"solidId\":\"SOLID_CUBE\",\"materialId\":\"MAT_TEST\",\"boundaryRole\":\"OPAQUE_ENVELOPE\",\"sourceClass\":\"TEST_FIXTURE\",\"uncertaintyClass\":\"HIGH\",\"apertureId\":null,\"apertureState\":null,\"outwardNormal\":[0,0,1],\"triangleStart\":2,\"triangleCount\":2},"
            "{\"surfaceId\":\"SURFACE_FRONT\",\"solidId\":\"SOLID_CUBE\",\"materialId\":\"MAT_TEST\",\"boundaryRole\":\"OPAQUE_ENVELOPE\",\"sourceClass\":\"TEST_FIXTURE\",\"uncertaintyClass\":\"HIGH\",\"apertureId\":null,\"apertureState\":null,\"outwardNormal\":[0,-1,0],\"triangleStart\":4,\"triangleCount\":2},"
            "{\"surfaceId\":\"SURFACE_BACK\",\"solidId\":\"SOLID_CUBE\",\"materialId\":\"MAT_TEST\",\"boundaryRole\":\"OPAQUE_ENVELOPE\",\"sourceClass\":\"TEST_FIXTURE\",\"uncertaintyClass\":\"HIGH\",\"apertureId\":null,\"apertureState\":null,\"outwardNormal\":[0,1,0],\"triangleStart\":6,\"triangleCount\":2},"
            "{\"surfaceId\":\"SURFACE_LEFT\",\"solidId\":\"SOLID_CUBE\",\"materialId\":\"MAT_TEST\",\"boundaryRole\":\"OPAQUE_ENVELOPE\",\"sourceClass\":\"TEST_FIXTURE\",\"uncertaintyClass\":\"HIGH\",\"apertureId\":null,\"apertureState\":null,\"outwardNormal\":[-1,0,0],\"triangleStart\":8,\"triangleCount\":2},"
            "{\"surfaceId\":\"SURFACE_RIGHT\",\"solidId\":\"SOLID_CUBE\",\"materialId\":\"MAT_TEST\",\"boundaryRole\":\"OPAQUE_ENVELOPE\",\"sourceClass\":\"TEST_FIXTURE\",\"uncertaintyClass\":\"HIGH\",\"apertureId\":null,\"apertureState\":null,\"outwardNormal\":[1,0,0],\"triangleStart\":10,\"triangleCount\":2}]}"),
        *CatalogSha256);
}

FString MakeCubeVertices(const FString& MinimumX, const FString& MaximumX)
{
    return FString::Printf(
        TEXT(
            "[%s,0,0],[%s,0,0],[%s,1,0],[%s,1,0],"
            "[%s,0,1],[%s,0,1],[%s,1,1],[%s,1,1]"),
        *MinimumX, *MaximumX, *MaximumX, *MinimumX,
        *MinimumX, *MaximumX, *MaximumX, *MinimumX);
}

FString MakeCubeTriangles(int32 VertexStart)
{
    const int32 A = VertexStart;
    const int32 B = VertexStart + 1;
    const int32 C = VertexStart + 2;
    const int32 D = VertexStart + 3;
    const int32 E = VertexStart + 4;
    const int32 F = VertexStart + 5;
    const int32 G = VertexStart + 6;
    const int32 H = VertexStart + 7;
    return FString::Printf(
        TEXT(
            "[%d,%d,%d],[%d,%d,%d],[%d,%d,%d],[%d,%d,%d],"
            "[%d,%d,%d],[%d,%d,%d],[%d,%d,%d],[%d,%d,%d],"
            "[%d,%d,%d],[%d,%d,%d],[%d,%d,%d],[%d,%d,%d]"),
        A, C, B, A, D, C, E, F, G, E, G, H,
        A, B, F, A, F, E, D, H, G, D, G, C,
        A, E, H, A, H, D, B, C, G, B, G, F);
}

FString MakeCubeSolid(
    const FString& Suffix,
    const FString& MaterialId,
    const FString& MinimumX,
    const FString& MaximumX,
    int32 VertexStart,
    int32 TriangleStart)
{
    return FString::Printf(
        TEXT(
            "{\"solidId\":\"SOLID_%s\",\"role\":\"TEST_CUBE_%s\",\"materialId\":\"%s\","
            "\"boundaryRole\":\"OPAQUE_ENVELOPE\",\"sourceClass\":\"TEST_FIXTURE\",\"uncertaintyClass\":\"HIGH\","
            "\"apertureId\":null,\"apertureState\":null,"
            "\"vertexStart\":%d,\"vertexCount\":8,\"triangleStart\":%d,\"triangleCount\":12,"
            "\"boundsMeters\":{\"min\":[%s,0,0],\"max\":[%s,1,1]},"
            "\"primitive\":{\"type\":\"AXIS_ALIGNED_BOX\",\"minMeters\":[%s,0,0],\"maxMeters\":[%s,1,1]}}"),
        *Suffix, *Suffix, *MaterialId,
        VertexStart, TriangleStart,
        *MinimumX, *MaximumX, *MinimumX, *MaximumX);
}

FString MakeCubeSurfaces(
    const FString& Suffix,
    const FString& MaterialId,
    int32 TriangleStart)
{
    const FString Prefix = FString::Printf(
        TEXT(
            "\"solidId\":\"SOLID_%s\",\"materialId\":\"%s\","
            "\"boundaryRole\":\"OPAQUE_ENVELOPE\",\"sourceClass\":\"TEST_FIXTURE\","
            "\"uncertaintyClass\":\"HIGH\",\"apertureId\":null,\"apertureState\":null"),
        *Suffix,
        *MaterialId);
    return FString::Printf(
        TEXT(
            "{\"surfaceId\":\"SURFACE_%s_BOTTOM\",%s,\"outwardNormal\":[0,0,-1],\"triangleStart\":%d,\"triangleCount\":2},"
            "{\"surfaceId\":\"SURFACE_%s_TOP\",%s,\"outwardNormal\":[0,0,1],\"triangleStart\":%d,\"triangleCount\":2},"
            "{\"surfaceId\":\"SURFACE_%s_FRONT\",%s,\"outwardNormal\":[0,-1,0],\"triangleStart\":%d,\"triangleCount\":2},"
            "{\"surfaceId\":\"SURFACE_%s_BACK\",%s,\"outwardNormal\":[0,1,0],\"triangleStart\":%d,\"triangleCount\":2},"
            "{\"surfaceId\":\"SURFACE_%s_LEFT\",%s,\"outwardNormal\":[-1,0,0],\"triangleStart\":%d,\"triangleCount\":2},"
            "{\"surfaceId\":\"SURFACE_%s_RIGHT\",%s,\"outwardNormal\":[1,0,0],\"triangleStart\":%d,\"triangleCount\":2}"),
        *Suffix, *Prefix, TriangleStart,
        *Suffix, *Prefix, TriangleStart + 2,
        *Suffix, *Prefix, TriangleStart + 4,
        *Suffix, *Prefix, TriangleStart + 6,
        *Suffix, *Prefix, TriangleStart + 8,
        *Suffix, *Prefix, TriangleStart + 10);
}

FString MakeAdjacentGeometry(
    const FString& CatalogSha256,
    const FString& SecondMaterialId,
    const FString& SecondMinimumX = TEXT("1"),
    const FString& SecondMaximumX = TEXT("2"))
{
    const FString FirstVertices = MakeCubeVertices(TEXT("0"), TEXT("1"));
    const FString SecondVertices = MakeCubeVertices(SecondMinimumX, SecondMaximumX);
    const FString FirstTriangles = MakeCubeTriangles(0);
    const FString SecondTriangles = MakeCubeTriangles(8);
    const FString FirstSolid = MakeCubeSolid(
        TEXT("A"), TEXT("MAT_TEST"), TEXT("0"), TEXT("1"), 0, 0);
    FString SecondSolid = MakeCubeSolid(
        TEXT("B"), SecondMaterialId, SecondMinimumX, SecondMaximumX, 8, 12);
    const FString FirstSurfaces = MakeCubeSurfaces(TEXT("A"), TEXT("MAT_TEST"), 0);
    FString SecondSurfaces = MakeCubeSurfaces(TEXT("B"), SecondMaterialId, 12);
    SecondSolid = SecondSolid.Replace(
        TEXT("\"sourceClass\":\"TEST_FIXTURE\",\"uncertaintyClass\":\"HIGH\""),
        TEXT("\"sourceClass\":\"TEST_FIXTURE_SECOND\",\"uncertaintyClass\":\"MEDIUM\""));
    SecondSurfaces = SecondSurfaces.Replace(
        TEXT("\"sourceClass\":\"TEST_FIXTURE\",\"uncertaintyClass\":\"HIGH\""),
        TEXT("\"sourceClass\":\"TEST_FIXTURE_SECOND\",\"uncertaintyClass\":\"MEDIUM\""));
    return FString::Printf(
        TEXT(
            "{\"schemaVersion\":\"triad.istana_public_view.rf_geometry.v1\","
            "\"assetId\":\"rf-test-adjacent-cubes\",\"revision\":\"TEST_ADJACENT_V1\","
            "\"status\":\"SIMULATION_READY_ASSUMPTION_BOUND\","
            "\"contractSha256\":\"0000000000000000000000000000000000000000000000000000000000000000\","
            "\"materialCatalogSha256\":\"%s\","
            "\"coordinateContract\":{\"logicalSystem\":\"RIGHT_HANDED_Z_UP_LOCAL_METRES_APPROACH_PLUS_Y\"},"
            "\"modeledCoverageEnvelope\":{"
            "\"coverageId\":\"RF_COVERAGE_TEST_ADJACENT\",\"scope\":\"MAIN_HERO_ONLY_ASSUMPTION_BOUND\","
            "\"shape\":\"AXIS_ALIGNED_BOX\",\"boundsMeters\":{\"min\":[-1,-1,-1],\"max\":[4,1,2]},"
            "\"boundaryInclusion\":\"CLOSED\","
            "\"finiteSegmentPolicy\":\"BOTH_ENDPOINTS_AND_ENTIRE_FINITE_SEGMENT_MUST_BE_INSIDE_CLOSED_ENVELOPE\","
            "\"outsideDomainPolicy\":\"REJECT_QUERY_NEVER_EMIT_CLEAR_OR_DIRECT_PATH\","
            "\"derivation\":\"MINIMUM_CLOSED_AABB_CONTAINING_CANONICAL_VERTICES_AND_WITNESS_ENDPOINTS\","
            "\"coversCanonicalHeroGeometry\":true,\"coversOfflineWitnessEndpoints\":true,"
            "\"coversOneKilometreAoi\":false,\"coversSurroundings\":false,"
            "\"surveyControlled\":false,\"fieldValidated\":false,"
            "\"explicitExclusions\":[\"TEST_FIXTURE_OUTSIDE_DOMAIN\"]},"
            "\"witnesses\":[{\"witnessId\":\"WIT_TEST_ADJACENT_EXTENTS\","
            "\"startMeters\":[-1,-1,-1],\"endMeters\":[4,1,2]}],"
            "\"verticesMeters\":[%s,%s],"
            "\"triangles\":[%s,%s],"
            "\"solids\":[%s,%s],"
            "\"surfaces\":[%s,%s]}"),
        *CatalogSha256,
        *FirstVertices,
        *SecondVertices,
        *FirstTriangles,
        *SecondTriangles,
        *FirstSolid,
        *SecondSolid,
        *FirstSurfaces,
        *SecondSurfaces);
}

FString JsonNumber(double Value)
{
    return FString::Printf(TEXT("%.17g"), Value);
}

FString JsonVector(const FVector& Value)
{
    return FString::Printf(
        TEXT("[%s,%s,%s]"),
        *JsonNumber(Value.X),
        *JsonNumber(Value.Y),
        *JsonNumber(Value.Z));
}

struct FIndexedPolyhedronFixture
{
    FString Suffix;
    TArray<FVector> Vertices;
    TArray<FIntVector> Triangles;
};

FIndexedPolyhedronFixture MakeTetrahedronFixture(
    const FString& Suffix,
    const TArray<FVector>& Vertices)
{
    check(Vertices.Num() == 4);
    FIndexedPolyhedronFixture Fixture;
    Fixture.Suffix = Suffix;
    Fixture.Vertices = Vertices;
    const FIntVector CandidateFaces[4] = {
        FIntVector(1, 2, 3),
        FIntVector(0, 3, 2),
        FIntVector(0, 1, 3),
        FIntVector(0, 2, 1)};
    const int32 OppositeVertices[4] = {0, 1, 2, 3};
    for (int32 FaceIndex = 0; FaceIndex < 4; ++FaceIndex)
    {
        FIntVector Face = CandidateFaces[FaceIndex];
        const FVector Normal = FVector::CrossProduct(
            Vertices[Face.Y] - Vertices[Face.X],
            Vertices[Face.Z] - Vertices[Face.X]);
        if (FVector::DotProduct(
                Normal,
                Vertices[OppositeVertices[FaceIndex]] -
                    Vertices[Face.X]) > 0.0)
        {
            Swap(Face.Y, Face.Z);
        }
        Fixture.Triangles.Add(Face);
    }
    return Fixture;
}

FString MakeIndexedPolyhedraGeometry(
    const FString& CatalogSha256,
    const FString& Revision,
    const TArray<FIndexedPolyhedronFixture>& Fixtures)
{
    TArray<FString> VertexJson;
    TArray<FString> TriangleJson;
    TArray<FString> SolidJson;
    TArray<FString> SurfaceJson;
    int32 VertexStart = 0;
    int32 TriangleStart = 0;
    for (const FIndexedPolyhedronFixture& Fixture : Fixtures)
    {
        FBox Bounds(ForceInit);
        for (const FVector& Vertex : Fixture.Vertices)
        {
            Bounds += Vertex;
            VertexJson.Add(JsonVector(Vertex));
        }
        for (int32 LocalTriangleIndex = 0;
             LocalTriangleIndex < Fixture.Triangles.Num();
             ++LocalTriangleIndex)
        {
            const FIntVector& Triangle =
                Fixture.Triangles[LocalTriangleIndex];
            TriangleJson.Add(FString::Printf(
                TEXT("[%d,%d,%d]"),
                VertexStart + Triangle.X,
                VertexStart + Triangle.Y,
                VertexStart + Triangle.Z));
            const FVector Normal = FVector::CrossProduct(
                Fixture.Vertices[Triangle.Y] - Fixture.Vertices[Triangle.X],
                Fixture.Vertices[Triangle.Z] - Fixture.Vertices[Triangle.X]).GetSafeNormal();
            SurfaceJson.Add(FString::Printf(
                TEXT(
                    "{\"surfaceId\":\"SURFACE_%s_%d\",\"solidId\":\"SOLID_%s\","
                    "\"materialId\":\"MAT_TEST\",\"boundaryRole\":\"OPAQUE_ENVELOPE\","
                    "\"sourceClass\":\"TEST_FIXTURE\",\"uncertaintyClass\":\"HIGH\","
                    "\"apertureId\":null,\"apertureState\":null,\"outwardNormal\":%s,"
                    "\"triangleStart\":%d,\"triangleCount\":1}"),
                *Fixture.Suffix,
                LocalTriangleIndex,
                *Fixture.Suffix,
                *JsonVector(Normal),
                TriangleStart + LocalTriangleIndex));
        }
        SolidJson.Add(FString::Printf(
            TEXT(
                "{\"solidId\":\"SOLID_%s\",\"role\":\"TEST_INDEXED_POLYHEDRON_%s\","
                "\"materialId\":\"MAT_TEST\",\"boundaryRole\":\"OPAQUE_ENVELOPE\","
                "\"sourceClass\":\"TEST_FIXTURE\",\"uncertaintyClass\":\"HIGH\","
                "\"apertureId\":null,\"apertureState\":null,"
                "\"vertexStart\":%d,\"vertexCount\":%d,\"triangleStart\":%d,\"triangleCount\":%d,"
                "\"boundsMeters\":{\"min\":%s,\"max\":%s},"
                "\"primitive\":{\"type\":\"INDEXED_CLOSED_POLYHEDRON\"}}"),
            *Fixture.Suffix,
            *Fixture.Suffix,
            VertexStart,
            Fixture.Vertices.Num(),
            TriangleStart,
            Fixture.Triangles.Num(),
            *JsonVector(Bounds.Min),
            *JsonVector(Bounds.Max)));
        VertexStart += Fixture.Vertices.Num();
        TriangleStart += Fixture.Triangles.Num();
    }
    return FString::Printf(
        TEXT(
            "{\"schemaVersion\":\"triad.istana_public_view.rf_geometry.v1\","
            "\"assetId\":\"rf-test-indexed-polyhedron\",\"revision\":\"%s\","
            "\"status\":\"SIMULATION_READY_ASSUMPTION_BOUND\","
            "\"contractSha256\":\"0000000000000000000000000000000000000000000000000000000000000000\","
            "\"materialCatalogSha256\":\"%s\","
            "\"coordinateContract\":{\"logicalSystem\":\"RIGHT_HANDED_Z_UP_LOCAL_METRES_APPROACH_PLUS_Y\"},"
            "\"modeledCoverageEnvelope\":{"
            "\"coverageId\":\"RF_COVERAGE_TEST_INDEXED\",\"scope\":\"MAIN_HERO_ONLY_ASSUMPTION_BOUND\","
            "\"shape\":\"AXIS_ALIGNED_BOX\",\"boundsMeters\":{\"min\":[-1,-1,-1],\"max\":[4,4,4]},"
            "\"boundaryInclusion\":\"CLOSED\","
            "\"finiteSegmentPolicy\":\"BOTH_ENDPOINTS_AND_ENTIRE_FINITE_SEGMENT_MUST_BE_INSIDE_CLOSED_ENVELOPE\","
            "\"outsideDomainPolicy\":\"REJECT_QUERY_NEVER_EMIT_CLEAR_OR_DIRECT_PATH\","
            "\"derivation\":\"MINIMUM_CLOSED_AABB_CONTAINING_CANONICAL_VERTICES_AND_WITNESS_ENDPOINTS\","
            "\"coversCanonicalHeroGeometry\":true,\"coversOfflineWitnessEndpoints\":true,"
            "\"coversOneKilometreAoi\":false,\"coversSurroundings\":false,"
            "\"surveyControlled\":false,\"fieldValidated\":false,"
            "\"explicitExclusions\":[\"TEST_FIXTURE_OUTSIDE_DOMAIN\"]},"
            "\"witnesses\":[{\"witnessId\":\"WIT_TEST_INDEXED_EXTENTS\","
            "\"startMeters\":[-1,-1,-1],\"endMeters\":[4,4,4]}],"
            "\"verticesMeters\":[%s],\"triangles\":[%s],\"solids\":[%s],\"surfaces\":[%s]}"),
        *Revision,
        *CatalogSha256,
        *FString::Join(VertexJson, TEXT(",")),
        *FString::Join(TriangleJson, TEXT(",")),
        *FString::Join(SolidJson, TEXT(",")),
        *FString::Join(SurfaceJson, TEXT(",")));
}

FIndexedPolyhedronFixture MakeSelfIntersectingPrismFixture()
{
    FIndexedPolyhedronFixture Fixture;
    Fixture.Suffix = TEXT("SELF_INTERSECTING");
    Fixture.Vertices = {
        FVector(0.0, 0.0, 0.0), FVector(2.0, 2.0, 0.0),
        FVector(0.0, 2.0, 0.0), FVector(3.0, 0.0, 0.0),
        FVector(0.0, 0.0, 1.0), FVector(2.0, 2.0, 1.0),
        FVector(0.0, 2.0, 1.0), FVector(3.0, 0.0, 1.0)};
    const FIntVector CubeTriangles[12] = {
        FIntVector(0, 2, 1), FIntVector(0, 3, 2),
        FIntVector(4, 5, 6), FIntVector(4, 6, 7),
        FIntVector(0, 1, 5), FIntVector(0, 5, 4),
        FIntVector(3, 7, 6), FIntVector(3, 6, 2),
        FIntVector(0, 4, 7), FIntVector(0, 7, 3),
        FIntVector(1, 2, 6), FIntVector(1, 6, 5)};
    for (FIntVector Triangle : CubeTriangles)
    {
        Swap(Triangle.Y, Triangle.Z);
        Fixture.Triangles.Add(Triangle);
    }
    return Fixture;
}

FIndexedPolyhedronFixture MakeKinkedConvexPrismFixture(
    const TCHAR* Suffix,
    double KinkMeters)
{
    FIndexedPolyhedronFixture Fixture;
    Fixture.Suffix = Suffix;
    Fixture.Vertices = {
        FVector(0.0, 0.0, 0.0), FVector(1.0, 0.0, 0.0),
        FVector(2.0, KinkMeters, 0.0), FVector(2.0, 1.0, 0.0),
        FVector(0.0, 1.0, 0.0), FVector(0.0, 0.0, 1.0),
        FVector(1.0, 0.0, 1.0), FVector(2.0, KinkMeters, 1.0),
        FVector(2.0, 1.0, 1.0), FVector(0.0, 1.0, 1.0)};
    Fixture.Triangles = {
        FIntVector(0, 2, 1), FIntVector(0, 3, 2),
        FIntVector(0, 4, 3), FIntVector(5, 6, 7),
        FIntVector(5, 7, 8), FIntVector(5, 8, 9),
        FIntVector(0, 1, 6), FIntVector(0, 6, 5),
        FIntVector(1, 2, 7), FIntVector(1, 7, 6),
        FIntVector(2, 3, 8), FIntVector(2, 8, 7),
        FIntVector(3, 4, 9), FIntVector(3, 9, 8),
        FIntVector(4, 0, 5), FIntVector(4, 5, 9)};
    return Fixture;
}

FIndexedPolyhedronFixture MakeNearCollinearConvexPrismFixture()
{
    return MakeKinkedConvexPrismFixture(
        TEXT("NEAR_COLLINEAR_CONVEX"), 0.00005);
}

FIndexedPolyhedronFixture MakeSubToleranceKinkConvexPrismFixture()
{
    return MakeKinkedConvexPrismFixture(
        TEXT("SUB_TOLERANCE_KINK_CONVEX"), 0.00000005);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTRIADRFIndexedGeometryQueryContractTest,
    "TRIAD.RF.IndexedGeometryQuery.Contract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTRIADRFIndexedGeometryQueryContractTest::RunTest(const FString& Parameters)
{
    FString KnownHash;
    FString KnownHashError;
    TestTrue(
        TEXT("Portable SHA-256 accepts the standard abc vector"),
        FTRIADRFIndexedGeometryQuery::ComputeCanonicalJsonSha256(
            TEXT("abc"), KnownHash, KnownHashError));
    TestEqual(
        TEXT("Portable SHA-256 matches the standard abc vector"),
        KnownHash,
        FString(TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")));
    const FString Catalog = MakeCatalog();
    const FString CatalogSha256 = Sha256Utf8(Catalog);
    TestEqual(TEXT("Test catalog has a SHA-256"), CatalogSha256.Len(), 64);
    const FString Geometry = MakeGeometry(CatalogSha256);
    FString Error;
    FTRIADRFIndexedGeometryQuery Query;
    TestTrue(TEXT("Closed hash-bound indexed cube loads"), Query.LoadFromJsonStrings(Geometry, Catalog, Error));
    TestTrue(TEXT("Loaded query is ready"), Query.IsReady());
    TestEqual(TEXT("Stable vertex count retained"), Query.GetVertexCount(), 8);
    TestEqual(TEXT("Stable triangle count retained"), Query.GetTriangleCount(), 12);
    TestEqual(TEXT("Stable solid count retained"), Query.GetSolidCount(), 1);
    TestEqual(TEXT("Stable surface count retained"), Query.GetSurfaceCount(), 6);
    TestEqual(TEXT("Stable material count retained"), Query.GetMaterialCount(), 1);
    TestTrue(TEXT("Deterministic BVH is populated"), Query.GetBVHNodeCount() > 1);
    TestEqual(
        TEXT("Geometry hash is retained from the exact parsed string buffer"),
        Query.GetMetadata().GeometrySha256,
        Sha256Utf8(Geometry));
    TestEqual(TEXT("Exact catalog hash retained"), Query.GetMetadata().MaterialCatalogSha256, CatalogSha256);
    TestTrue(TEXT("Geometry query ID binds the catalog hash"), Query.GetMetadata().GeometryQueryId.Contains(CatalogSha256));
    TestEqual(
        TEXT("Modeled coverage ID retained"),
        Query.GetMetadata().ModeledCoverageId,
        FString(TEXT("RF_COVERAGE_TEST_HERO")));
    TestEqual(
        TEXT("Modeled coverage remains hero-only"),
        Query.GetMetadata().ModeledCoverageScope,
        FString(TEXT("MAIN_HERO_ONLY_ASSUMPTION_BOUND")));
    TestEqual(
        TEXT("Modeled coverage minimum is converted to Unreal centimetres"),
        Query.GetMetadata().ModeledCoverageMinimumCentimeters,
        FVector(-100.0, -100.0, -100.0));
    TestEqual(
        TEXT("Modeled coverage maximum is converted to Unreal centimetres"),
        Query.GetMetadata().ModeledCoverageMaximumCentimeters,
        FVector(200.0, 100.0, 200.0));
    TestFalse(
        TEXT("Modeled coverage never claims the one-kilometre AOI"),
        Query.GetMetadata().bModeledCoverageCoversOneKilometreAoi);

    const FString CircleStudyDomain =
        TEXT("\"studyDomain\":{\"domainId\":\"RF_STUDY_DOMAIN_TEST_RADIUS_1000M\","
             "\"domainType\":\"CLOSED_WGS84_GEODESIC_CIRCLE\",\"shape\":\"CIRCLE\","
             "\"distanceMethod\":\"SIGNED_WGS84_VINCENTY_INVERSE_GEODESIC_CENTER_DISTANCE_MINUS_RADIUS_METERS\","
             "\"boundaryInclusion\":\"CLOSED\",\"radiusMeters\":1000.0,"
             "\"pointMembershipPolicy\":\"WGS84_VINCENTY_INVERSE_SURFACE_DISTANCE_METERS_FROM_CENTER_TO_ENDPOINT_LESS_THAN_OR_EQUAL_TO_RADIUS_METERS\","
             "\"finiteSegmentPolicy\":\"BOTH_ENDPOINTS_MUST_HAVE_SIGNED_WGS84_VINCENTY_DISTANCE_LESS_THAN_OR_EQUAL_TO_ZERO; THE CLOSED 1000_M_GEODESIC_DISK_IS CONVEX FOR THE ADMITTED MINIMIZING SURFACE SEGMENT\","
             "\"containsEntireFiniteSegmentWhenBothEndpointsInside\":true,"
             "\"centerWgs84Degrees\":{\"longitude\":103.84288055,\"latitude\":1.30709615},"
             "\"configurationBinding\":{\"enabled\":true,"
             "\"referenceName\":\"RF_STUDY_DOMAIN_TEST_RADIUS_1000M\",\"shape\":\"Circle\"},"
             "\"perimeterSampling\":{\"count\":64,\"startAzimuthDegrees\":0.0,"
             "\"stepDegrees\":5.625,\"azimuthConvention\":\"CLOCKWISE_FROM_TRUE_NORTH\"},"
             "\"frame\":{\"sourceGeodeticCrs\":\"EPSG:4326\","
             "\"projectedConstructionCrs\":\"EPSG:3414\","
             "\"logicalSystem\":\"RIGHT_HANDED_Z_UP_LOCAL_METRES_APPROACH_PLUS_Y\","
             "\"studyMembershipFrame\":\"WGS84_GEODETIC_SURFACE_HORIZONTAL_ALTITUDE_IGNORED\","
             "\"surveyRegistered\":false},\"truthFlags\":{"
             "\"containedByLoaderCoverageAabb\":true,\"coversEntireRadiusCircle\":true,"
             "\"cornersOutsideCircleExcludedFromStudy\":true}},");
    const FString CircleGeometry = Geometry.Replace(
        TEXT("\"explicitExclusions\":"),
        *(CircleStudyDomain + TEXT("\"explicitExclusions\":")));
    FTRIADRFIndexedGeometryQuery CircleQuery;
    TestTrue(TEXT("Additive closed WGS84 circle geometry loads"),
        CircleQuery.LoadFromJsonStrings(CircleGeometry, Catalog, Error));
    TestTrue(TEXT("Circle study-domain metadata is retained"),
        CircleQuery.GetMetadata().bHasClosedWgs84GeodesicCircleStudyDomain);
    FTRIADRFIndexedGeometryQuery MismatchedDomainQuery;
    TestFalse(TEXT("Mismatched circle domain type fails closed"),
        MismatchedDomainQuery.LoadFromJsonStrings(
            CircleGeometry.Replace(
                TEXT("CLOSED_WGS84_GEODESIC_CIRCLE"),
                TEXT("LOCAL_EUCLIDEAN_CIRCLE")),
            Catalog,
            Error));
    FTRIADRFIndexedGeometryQuery MismatchedPerimeterQuery;
    TestFalse(TEXT("Mismatched perimeter count and step fail closed"),
        MismatchedPerimeterQuery.LoadFromJsonStrings(
            CircleGeometry.Replace(TEXT("\"count\":64"), TEXT("\"count\":63")),
            Catalog,
            Error));
    FTRIADRFIndexedGeometryQuery MismatchedFrameQuery;
    TestFalse(TEXT("Mismatched geodetic frame fails closed"),
        MismatchedFrameQuery.LoadFromJsonStrings(
            CircleGeometry.Replace(TEXT("EPSG:4326"), TEXT("EPSG:3857")),
            Catalog,
            Error));
    const FVector2D Center = CircleQuery.GetMetadata().StudyDomainCenterWgs84Degrees;
    const FVector LocalStart(0.0, 0.0, 50.0);
    const FVector LocalEnd(50.0, 0.0, 50.0);
    double StartSigned = 0.0;
    double EndSigned = 0.0;
    for (const double Bearing : {0.0, 90.0, 180.0, 270.0})
    {
        const FVector2D Boundary = TRIAD::Geodesy::Wgs84DestinationDegrees(
            Center.X, Center.Y, Bearing, 1000.0);
        TestTrue(FString::Printf(TEXT("Cardinal %.0f-degree closed boundary is admitted"), Bearing),
            CircleQuery.IsFiniteSegmentWithinAdmittedStudyDomain(
                LocalStart, LocalEnd, FVector(Center.X, Center.Y, 0.0),
                FVector(Boundary.X, Boundary.Y, 0.0), StartSigned, EndSigned, Error));
        const FVector2D Outside = TRIAD::Geodesy::Wgs84DestinationDegrees(
            Center.X, Center.Y, Bearing, 1000.001);
        TestFalse(FString::Printf(TEXT("Cardinal %.0f-degree epsilon outside fails closed"), Bearing),
            CircleQuery.IsFiniteSegmentWithinAdmittedStudyDomain(
                LocalStart, LocalEnd, FVector(Center.X, Center.Y, 0.0),
                FVector(Outside.X, Outside.Y, 0.0), StartSigned, EndSigned, Error));
    }
    TestFalse(TEXT("Non-finite georeference transform evidence fails closed"),
        CircleQuery.IsFiniteSegmentWithinAdmittedStudyDomain(
            LocalStart, LocalEnd,
            FVector(std::numeric_limits<double>::quiet_NaN(), Center.Y, 0.0),
            FVector(Center.X, Center.Y, 0.0), StartSigned, EndSigned, Error));
    TestFalse(TEXT("Vertical AABB overflow fails before circle admission"),
        CircleQuery.IsFiniteSegmentWithinAdmittedStudyDomain(
            LocalStart, FVector(0.0, 0.0, 200.001),
            FVector(Center.X, Center.Y, 0.0), FVector(Center.X, Center.Y, 0.0),
            StartSigned, EndSigned, Error));
    FTRIADRFIndexedSolidMetadata SolidMetadata;
    TestTrue(
        TEXT("Stable solid metadata is queryable"),
        Query.TryGetSolidMetadata(TEXT("SOLID_CUBE"), SolidMetadata));
    TestEqual(TEXT("Solid role metadata retained"), SolidMetadata.Role, FString(TEXT("TEST_CUBE")));
    TestEqual(
        TEXT("TightV1-style axis-aligned primitive metadata is retained"),
        SolidMetadata.PrimitiveType,
        FString(TEXT("AXIS_ALIGNED_BOX")));
    TestEqual(
        TEXT("Solid provenance metadata retained"),
        SolidMetadata.SourceClass,
        FString(TEXT("TEST_FIXTURE")));
    FTRIADRFIndexedSurfaceMetadata SurfaceMetadata;
    TestTrue(
        TEXT("Stable surface metadata is queryable"),
        Query.TryGetSurfaceMetadata(TEXT("SURFACE_LEFT"), SurfaceMetadata));
    TestEqual(
        TEXT("Surface boundary metadata retained"),
        SurfaceMetadata.BoundaryRole,
        FString(TEXT("OPAQUE_ENVELOPE")));
    FTRIADRFIndexedMaterialMetadata MaterialMetadata;
    TestTrue(
        TEXT("Stable material metadata is queryable"),
        Query.TryGetMaterialMetadata(TEXT("MAT_TEST"), MaterialMetadata));
    TestEqual(
        TEXT("Material provenance metadata retained"),
        MaterialMetadata.ProvenanceId,
        FString(TEXT("TEST_PRIOR")));

    const FVector Left(-100.0, -40.0, 60.0);
    const FVector Right(200.0, -40.0, 60.0);
    TArray<FTRIADRFIndexedSegmentHit> Hits;
    TestTrue(TEXT("Forward finite segment traces"), Query.TraceSegment(Left, Right, 0.01, Hits, Error));
    TestEqual(TEXT("Forward segment has one entry/exit pair"), Hits.Num(), 2);
    if (Hits.Num() == 2)
    {
        TestTrue(TEXT("First crossing is entry"), Hits[0].Crossing == ETRIADRFBoundaryCrossing::Entry);
        TestTrue(TEXT("Second crossing is exit"), Hits[1].Crossing == ETRIADRFBoundaryCrossing::Exit);
        TestEqual(TEXT("Entry point is x=0"), Hits[0].PointCentimeters.X, 0.0, 1.0e-9);
        TestEqual(TEXT("Exit point is x=100"), Hits[1].PointCentimeters.X, 100.0, 1.0e-9);
        TestEqual(TEXT("Entry stable surface ID retained"), Hits[0].SurfaceId, FString(TEXT("SURFACE_LEFT")));
        TestEqual(TEXT("Exit stable surface ID retained"), Hits[1].SurfaceId, FString(TEXT("SURFACE_RIGHT")));
        TestEqual(TEXT("Stable solid ID retained"), Hits[0].SolidId, FString(TEXT("SOLID_CUBE")));
        TestEqual(TEXT("Stable material ID retained"), Hits[0].MaterialId, FString(TEXT("MAT_TEST")));
        TestEqual(TEXT("Hit solid role retained"), Hits[0].SolidRole, FString(TEXT("TEST_CUBE")));
        TestEqual(TEXT("Hit source provenance retained"), Hits[0].SourceClass, FString(TEXT("TEST_FIXTURE")));
        TestTrue(TEXT("Derived triangle ID is deterministic"), Hits[0].TriangleId.StartsWith(TEXT("TEST_V1:triangle:")));
    }

    TArray<FTRIADRFIndexedSegmentHit> ReverseHits;
    TestTrue(TEXT("Reverse finite segment traces"), Query.TraceSegment(Right, Left, 0.01, ReverseHits, Error));
    TestEqual(TEXT("Reverse segment has one entry/exit pair"), ReverseHits.Num(), 2);
    if (ReverseHits.Num() == 2)
    {
        TestEqual(TEXT("Reverse entry is right face"), ReverseHits[0].SurfaceId, FString(TEXT("SURFACE_RIGHT")));
        TestTrue(TEXT("Reverse classification begins with entry"), ReverseHits[0].Crossing == ETRIADRFBoundaryCrossing::Entry);
        TestEqual(TEXT("Bidirectional chord distance agrees"), ReverseHits[1].DistanceCentimeters - ReverseHits[0].DistanceCentimeters, Hits[1].DistanceCentimeters - Hits[0].DistanceCentimeters, 1.0e-9);
    }

    FTRIADRFModelLimits ModelLimits;
    TArray<FTRIADRFPathCandidate> Candidates;
    TestTrue(TEXT("Straight transmission candidate builds"), Query.BuildPathCandidates(Left, Right, 2.4, ModelLimits, Candidates, Error));
    TestEqual(TEXT("Exactly one straight candidate is admitted"), Candidates.Num(), 1);
    if (Candidates.Num() == 1)
    {
        TestTrue(TEXT("Candidate kind is transmitted"), Candidates[0].Kind == ETRIADRFPathKind::Transmitted);
        TestEqual(TEXT("One finite solid becomes one interaction"), Candidates[0].Interactions.Num(), 1);
        if (Candidates[0].Interactions.Num() == 1)
        {
            const FTRIADRFPathInteraction& Interaction = Candidates[0].Interactions[0];
            TestEqual(TEXT("Entry/exit chord is one metre"), FVector::Distance(Interaction.EntryPointCentimeters, Interaction.ExitPointCentimeters), 100.0, 1.0e-9);
            TestEqual(TEXT("Explicit paired boundary prior retained"), Interaction.Surface.PairedBoundaryTransmissionLossDb, 5.0, 1.0e-12);
            TestEqual(TEXT("Explicit bulk prior retained"), Interaction.Surface.BulkAttenuationDbPerMeter, 10.0, 1.0e-12);
            TestTrue(TEXT("Assumption prior remains uncalibrated"), Interaction.Surface.CalibrationState == ETRIADRFMaterialCalibrationState::Uncalibrated);
            TestTrue(
                TEXT("Profile provenance binds the full catalog hash"),
                Interaction.Surface.CalibrationProvenanceId.Contains(CatalogSha256));
            TestTrue(
                TEXT("Profile provenance retains the exact profile ID"),
                Interaction.Surface.CalibrationProvenanceId.Contains(TEXT("PROFILE_ALL")));
            TestEqual(
                TEXT("Transmission telemetry retains the stable solid ID"),
                Interaction.Surface.SolidId,
                FString(TEXT("SOLID_CUBE")));
            TestEqual(
                TEXT("Transmission telemetry retains the selected profile ID"),
                Interaction.Surface.ProfileId,
                FString(TEXT("PROFILE_ALL")));
            TestEqual(
                TEXT("Transmission telemetry retains source provenance"),
                Interaction.Surface.SourceClass,
                FString(TEXT("TEST_FIXTURE")));
            TestEqual(
                TEXT("Transmission telemetry retains uncertainty class"),
                Interaction.Surface.UncertaintyClass,
                FString(TEXT("HIGH")));
            TestEqual(
                TEXT("Transmission telemetry names the coefficient-selection rule"),
                Interaction.Surface.CoefficientSelectionSemantics,
                FString(TEXT("SINGLE_EXPLICIT_PROFILE_CLOSED_INTERVAL_NO_INTERPOLATION")));
        }
        FTRIADDeterministicRFInteractionModel InteractionModel(ModelLimits);
        FTRIADRFPathEvaluation Evaluation;
        TestTrue(TEXT("Geometry-produced candidate satisfies the interaction seam"), InteractionModel.EvaluatePath(Candidates[0], 2.4, Evaluation));
        TestEqual(TEXT("One metre slab applies boundary plus bulk loss"), Evaluation.InteractionLossDb, 15.0, 1.0e-9);
        TestFalse(TEXT("Assumption-bound query cannot become survey truth"), Evaluation.bReadyForSurveyTruth);
    }

    const TArray<FVector> PrimaryTetraVertices = {
        FVector(0.0, 0.0, 0.0),
        FVector(2.0, 0.0, 0.0),
        FVector(0.0, 2.0, 0.0),
        FVector(0.0, 0.0, 2.0)};
    const FString SlopedGeometry = MakeIndexedPolyhedraGeometry(
        CatalogSha256,
        TEXT("TEST_INDEXED_SLOPED_V1"),
        {MakeTetrahedronFixture(TEXT("SLOPED"), PrimaryTetraVertices)});
    FTRIADRFIndexedGeometryQuery SlopedQuery;
    TestTrue(
        TEXT("Sloped indexed closed polyhedron loads"),
        SlopedQuery.LoadFromJsonStrings(SlopedGeometry, Catalog, Error));
    TestTrue(TEXT("Sloped indexed query is ready"), SlopedQuery.IsReady());
    FTRIADRFIndexedSolidMetadata SlopedMetadata;
    TestTrue(
        TEXT("Sloped indexed solid metadata is queryable"),
        SlopedQuery.TryGetSolidMetadata(TEXT("SOLID_SLOPED"), SlopedMetadata));
    TestEqual(
        TEXT("Generic primitive type metadata is retained"),
        SlopedMetadata.PrimitiveType,
        FString(TEXT("INDEXED_CLOSED_POLYHEDRON")));
    TArray<FTRIADRFIndexedSegmentHit> SlopedHits;
    TestTrue(
        TEXT("Finite segment traces through the sloped indexed face"),
        SlopedQuery.TraceSegment(
            FVector(-50.0, -25.0, 25.0),
            FVector(250.0, -25.0, 25.0),
            0.01,
            SlopedHits,
            Error));
    TestEqual(
        TEXT("Sloped indexed tetrahedron has one entry/exit pair"),
        SlopedHits.Num(),
        2);
    if (SlopedHits.Num() == 2)
    {
        TestEqual(
            TEXT("Sloped indexed exit is resolved at x=1.5m"),
            SlopedHits[1].PointCentimeters.X,
            150.0,
            1.0e-8);
    }

    const FString RelabeledTetraGeometry = SlopedGeometry.Replace(
        TEXT("{\"type\":\"INDEXED_CLOSED_POLYHEDRON\"}"),
        TEXT("{\"type\":\"AXIS_ALIGNED_BOX\",\"minMeters\":[0,0,0],\"maxMeters\":[2,2,2]}"));
    FTRIADRFIndexedGeometryQuery RelabeledTetraQuery;
    TestFalse(
        TEXT("A tetrahedron relabeled as an axis-aligned box fails closed"),
        RelabeledTetraQuery.LoadFromJsonStrings(
            RelabeledTetraGeometry,
            Catalog,
            Error));
    TestTrue(
        TEXT("Relabeled tetrahedron rejection cites exact box mesh shape"),
        Error.Contains(TEXT("exactly 8 vertices and 12 triangles")));

    const FString MalformedBoxGeometry = Geometry.Replace(
        TEXT("[1,1,1],[0,1,1]"),
        TEXT("[0.8,1,1],[0,1,1]"));
    FTRIADRFIndexedGeometryQuery MalformedBoxQuery;
    TestFalse(
        TEXT("Malformed indexed boundary cannot retain AXIS_ALIGNED_BOX semantics"),
        MalformedBoxQuery.LoadFromJsonStrings(
            MalformedBoxGeometry,
            Catalog,
            Error));

    FString UnusedVertexGeometry = Geometry.Replace(
        TEXT("[1,1,1],[0,1,1]],\"triangles\""),
        TEXT("[1,1,1],[0,1,1],[0.5,0.5,0.5]],\"triangles\""));
    UnusedVertexGeometry = UnusedVertexGeometry.Replace(
        TEXT("\"vertexStart\":0,\"vertexCount\":8"),
        TEXT("\"vertexStart\":0,\"vertexCount\":9"));
    FTRIADRFIndexedGeometryQuery UnusedVertexQuery;
    TestFalse(
        TEXT("Unused vertex inside a solid range fails closed"),
        UnusedVertexQuery.LoadFromJsonStrings(
            UnusedVertexGeometry,
            Catalog,
            Error));
    TestTrue(
        TEXT("Unused vertex rejection is explicit"),
        Error.Contains(TEXT("unused vertex")));

    const double Translation = 10000000.0;
    const TArray<FVector> TranslatedTetraVertices = {
        FVector(Translation, Translation, Translation),
        FVector(Translation + 2.0, Translation, Translation),
        FVector(Translation, Translation + 2.0, Translation),
        FVector(Translation, Translation, Translation + 2.0)};
    FString TranslatedGeometry = MakeIndexedPolyhedraGeometry(
        CatalogSha256,
        TEXT("TEST_INDEXED_TRANSLATED_VOLUME_V1"),
        {MakeTetrahedronFixture(
            TEXT("TRANSLATED"),
            TranslatedTetraVertices)});
    TranslatedGeometry = TranslatedGeometry.Replace(
        TEXT("[-1,-1,-1]"),
        *JsonVector(FVector(
            Translation - 1.0,
            Translation - 1.0,
            Translation - 1.0)));
    TranslatedGeometry = TranslatedGeometry.Replace(
        TEXT("[4,4,4]"),
        *JsonVector(FVector(
            Translation + 4.0,
            Translation + 4.0,
            Translation + 4.0)));
    FTRIADRFIndexedGeometryQuery TranslatedQuery;
    TestTrue(
        TEXT("Positive volume remains stable far from the world origin"),
        TranslatedQuery.LoadFromJsonStrings(
            TranslatedGeometry,
            Catalog,
            Error));

    const FString ConflictingCoverageAliasGeometry = Geometry.Replace(
        TEXT("\"coversCanonicalHeroGeometry\":true"),
        TEXT("\"coversCanonicalGeometry\":false,\"coversCanonicalHeroGeometry\":true"));
    FTRIADRFIndexedGeometryQuery ConflictingCoverageAliasQuery;
    TestFalse(
        TEXT("Conflicting preferred and legacy coverage aliases fail closed"),
        ConflictingCoverageAliasQuery.LoadFromJsonStrings(
            ConflictingCoverageAliasGeometry,
            Catalog,
            Error));
    TestTrue(
        TEXT("Coverage alias conflict is explicit"),
        Error.Contains(TEXT("conflicts with legacy alias")));

    const FString NearCollinearConvexGeometry =
        MakeIndexedPolyhedraGeometry(
            CatalogSha256,
            TEXT("TEST_INDEXED_NEAR_COLLINEAR_CONVEX_V1"),
            {MakeNearCollinearConvexPrismFixture()});
    FTRIADRFIndexedGeometryQuery NearCollinearConvexQuery;
    TestTrue(
        TEXT("Convex prism with near-collinear adjacent walls loads"),
        NearCollinearConvexQuery.LoadFromJsonStrings(
            NearCollinearConvexGeometry,
            Catalog,
            Error));
    TestTrue(
        TEXT("Near-collinear convex indexed query is ready"),
        NearCollinearConvexQuery.IsReady());

    const FString SubToleranceKinkConvexGeometry =
        MakeIndexedPolyhedraGeometry(
            CatalogSha256,
            TEXT("TEST_INDEXED_SUB_TOLERANCE_KINK_CONVEX_V1"),
            {MakeSubToleranceKinkConvexPrismFixture()});
    FTRIADRFIndexedGeometryQuery SubToleranceKinkConvexQuery;
    TestTrue(
        TEXT("Convex prism with sub-tolerance near-coplanar kink loads"),
        SubToleranceKinkConvexQuery.LoadFromJsonStrings(
            SubToleranceKinkConvexGeometry,
            Catalog,
            Error));
    TestTrue(
        TEXT("Sub-tolerance kink convex indexed query is ready"),
        SubToleranceKinkConvexQuery.IsReady());

    const FString SelfIntersectingGeometry = MakeIndexedPolyhedraGeometry(
        CatalogSha256,
        TEXT("TEST_INDEXED_SELF_INTERSECTION_V1"),
        {MakeSelfIntersectingPrismFixture()});
    FTRIADRFIndexedGeometryQuery SelfIntersectingQuery;
    TestFalse(
        TEXT("Self-intersecting indexed closed polyhedron fails closed"),
        SelfIntersectingQuery.LoadFromJsonStrings(
            SelfIntersectingGeometry,
            Catalog,
            Error));
    TestTrue(
        TEXT("Self-intersection rejection identifies triangle self-intersection"),
        Error.Contains(TEXT("triangle self-intersection")));
    TestFalse(
        TEXT("Self-intersecting indexed query remains unavailable"),
        SelfIntersectingQuery.IsReady());

    const TArray<FVector> PartiallyOverlappingTetraVertices = {
        FVector(0.8, 0.1, 0.1),
        FVector(2.8, 0.1, 0.1),
        FVector(0.8, 2.1, 0.1),
        FVector(0.8, 0.1, 2.1)};
    const FString OverlappingGeometry = MakeIndexedPolyhedraGeometry(
        CatalogSha256,
        TEXT("TEST_INDEXED_OVERLAP_V1"),
        {
            MakeTetrahedronFixture(TEXT("OVERLAP_A"), PrimaryTetraVertices),
            MakeTetrahedronFixture(
                TEXT("OVERLAP_B"), PartiallyOverlappingTetraVertices)
        });
    FTRIADRFIndexedGeometryQuery OverlappingQuery;
    TestFalse(
        TEXT("Partially overlapping indexed polyhedra fail closed"),
        OverlappingQuery.LoadFromJsonStrings(
            OverlappingGeometry,
            Catalog,
            Error));
    TestTrue(
        TEXT("Partial-overlap rejection identifies positive-volume overlap"),
        Error.Contains(TEXT("positive-volume overlap or containment")));

    const TArray<FVector> ContainedTetraVertices = {
        FVector(0.2, 0.2, 0.2),
        FVector(0.6, 0.2, 0.2),
        FVector(0.2, 0.6, 0.2),
        FVector(0.2, 0.2, 0.6)};
    const FString ContainedGeometry = MakeIndexedPolyhedraGeometry(
        CatalogSha256,
        TEXT("TEST_INDEXED_CONTAINMENT_V1"),
        {
            MakeTetrahedronFixture(TEXT("CONTAINMENT_OUTER"), PrimaryTetraVertices),
            MakeTetrahedronFixture(TEXT("CONTAINMENT_INNER"), ContainedTetraVertices)
        });
    FTRIADRFIndexedGeometryQuery ContainedQuery;
    TestFalse(
        TEXT("Contained indexed polyhedron fails closed"),
        ContainedQuery.LoadFromJsonStrings(
            ContainedGeometry,
            Catalog,
            Error));
    TestTrue(
        TEXT("Containment rejection identifies positive-volume containment"),
        Error.Contains(TEXT("positive-volume overlap or containment")));

    const TArray<FVector> FaceTouchingTetraVertices = {
        FVector(2.0, 0.0, 0.0),
        FVector(0.0, 2.0, 0.0),
        FVector(0.0, 0.0, 2.0),
        FVector(2.0, 2.0, 2.0)};
    const FString FaceTouchingGeometry = MakeIndexedPolyhedraGeometry(
        CatalogSha256,
        TEXT("TEST_INDEXED_BOUNDARY_TOUCH_V1"),
        {
            MakeTetrahedronFixture(TEXT("TOUCH_A"), PrimaryTetraVertices),
            MakeTetrahedronFixture(TEXT("TOUCH_B"), FaceTouchingTetraVertices)
        });
    FTRIADRFIndexedGeometryQuery FaceTouchingQuery;
    TestTrue(
        TEXT("Zero-volume shared-face boundary touch remains valid"),
        FaceTouchingQuery.LoadFromJsonStrings(
            FaceTouchingGeometry,
            Catalog,
            Error));
    TestTrue(
        TEXT("Boundary-touching indexed query is ready"),
        FaceTouchingQuery.IsReady());

    // These tetrahedra have positive AABB overlap but are separated by the
    // parallel planes x+y+z=2 and x+y+z=3. With no triangle contacts, the
    // deterministic validation workload is exactly 52 triangle checks:
    // four for the one in-bounds vertex and 48 for inward face samples.
    const TArray<FVector> DiagonallySeparatedTetraVertices = {
        FVector(1.0, 1.0, 1.0),
        FVector(3.0, 1.0, 1.0),
        FVector(1.0, 3.0, 1.0),
        FVector(1.0, 1.0, 3.0)};
    const FString BoundedContainmentGeometry =
        MakeIndexedPolyhedraGeometry(
            CatalogSha256,
            TEXT("TEST_INDEXED_CONTAINMENT_BUDGET_V1"),
            {
                MakeTetrahedronFixture(
                    TEXT("CONTAINMENT_BUDGET_A"), PrimaryTetraVertices),
                MakeTetrahedronFixture(
                    TEXT("CONTAINMENT_BUDGET_B"),
                    DiagonallySeparatedTetraVertices)
            });
    FTRIADRFIndexedGeometryLoadLimits BelowContainmentBudgetLimits;
    BelowContainmentBudgetLimits.MaximumContainmentTriangleChecks = 51;
    FTRIADRFIndexedGeometryQuery BelowContainmentBudgetQuery(
        BelowContainmentBudgetLimits);
    TestFalse(
        TEXT("Containment validation fails at one check below its exact bounded workload"),
        BelowContainmentBudgetQuery.LoadFromJsonStrings(
            BoundedContainmentGeometry,
            Catalog,
            Error));
    TestTrue(
        TEXT("Containment ceiling rejection is explicit"),
        Error.Contains(TEXT("containment validation exceeded its configured deterministic triangle-check ceiling")));
    TestFalse(
        TEXT("Ceiling-rejected containment query remains unavailable"),
        BelowContainmentBudgetQuery.IsReady());

    FTRIADRFIndexedGeometryLoadLimits ExactContainmentBudgetLimits;
    ExactContainmentBudgetLimits.MaximumContainmentTriangleChecks = 52;
    FTRIADRFIndexedGeometryQuery ExactContainmentBudgetQuery(
        ExactContainmentBudgetLimits);
    TestTrue(
        TEXT("Containment validation succeeds at its exact bounded workload"),
        ExactContainmentBudgetQuery.LoadFromJsonStrings(
            BoundedContainmentGeometry,
            Catalog,
            Error));
    TestTrue(
        TEXT("Exact-budget containment query is ready"),
        ExactContainmentBudgetQuery.IsReady());

    const FVector AdjacentLeft(-50.0, -40.0, 60.0);
    const FVector AdjacentRight(300.0, -40.0, 60.0);
    FTRIADRFModelLimits AdjacentLimits = ModelLimits;
    AdjacentLimits.GeometryPositionToleranceCentimeters = 0.01;
    AdjacentLimits.MaximumInteractionsPerPath = 1;
    const FString AdjacentGeometry = MakeAdjacentGeometry(
        CatalogSha256,
        TEXT("MAT_TEST"));
    FTRIADRFIndexedGeometryQuery AdjacentQuery;
    TestTrue(
        TEXT("Adjacent same-material closed solids load"),
        AdjacentQuery.LoadFromJsonStrings(AdjacentGeometry, Catalog, Error));
    TArray<FTRIADRFIndexedSegmentHit> AdjacentHits;
    TestTrue(
        TEXT("Adjacent same-material raw boundaries remain traceable"),
        AdjacentQuery.TraceSegment(
            AdjacentLeft,
            AdjacentRight,
            AdjacentLimits.GeometryPositionToleranceCentimeters,
            AdjacentHits,
            Error));
    TestEqual(
        TEXT("Adjacent same-material trace preserves both solid pairs"),
        AdjacentHits.Num(),
        4);
    TestTrue(
        TEXT("Adjacent same-material transmission candidate builds"),
        AdjacentQuery.BuildPathCandidates(
            AdjacentLeft,
            AdjacentRight,
            2.4,
            AdjacentLimits,
            Candidates,
            Error));
    TestEqual(
        TEXT("Adjacent same-material solids collapse to one material span"),
        Candidates.Num() == 1 ? Candidates[0].Interactions.Num() : 0,
        1);
    if (Candidates.Num() == 1 && Candidates[0].Interactions.Num() == 1)
    {
        const FTRIADRFPathInteraction& Interaction = Candidates[0].Interactions[0];
        TestEqual(
            TEXT("Same-material continuity spans the full two-metre chord"),
            FVector::Distance(
                Interaction.EntryPointCentimeters,
                Interaction.ExitPointCentimeters),
            200.0,
            1.0e-9);
        TestEqual(
            TEXT("Collapsed material span retains both physical contributors"),
            Interaction.Surface.Contributors.Num(),
            2);
        if (Interaction.Surface.Contributors.Num() == 2)
        {
            TestEqual(TEXT("First contributor retains first solid"), Interaction.Surface.Contributors[0].SolidId, FString(TEXT("SOLID_A")));
            TestEqual(TEXT("Second contributor retains second solid"), Interaction.Surface.Contributors[1].SolidId, FString(TEXT("SOLID_B")));
            TestEqual(TEXT("First contributor retains entry surface"), Interaction.Surface.Contributors[0].EntrySurfaceId, FString(TEXT("SURFACE_A_LEFT")));
            TestEqual(TEXT("First contributor retains exit surface"), Interaction.Surface.Contributors[0].ExitSurfaceId, FString(TEXT("SURFACE_A_RIGHT")));
            TestEqual(TEXT("Second contributor retains entry surface"), Interaction.Surface.Contributors[1].EntrySurfaceId, FString(TEXT("SURFACE_B_LEFT")));
            TestEqual(TEXT("Second contributor retains exit surface"), Interaction.Surface.Contributors[1].ExitSurfaceId, FString(TEXT("SURFACE_B_RIGHT")));
            TestEqual(TEXT("First contributor retains source class"), Interaction.Surface.Contributors[0].SourceClass, FString(TEXT("TEST_FIXTURE")));
            TestEqual(TEXT("Second contributor retains distinct source class"), Interaction.Surface.Contributors[1].SourceClass, FString(TEXT("TEST_FIXTURE_SECOND")));
            TestEqual(TEXT("First contributor retains uncertainty class"), Interaction.Surface.Contributors[0].UncertaintyClass, FString(TEXT("HIGH")));
            TestEqual(TEXT("Second contributor retains distinct uncertainty class"), Interaction.Surface.Contributors[1].UncertaintyClass, FString(TEXT("MEDIUM")));
        }
        FTRIADDeterministicRFInteractionModel AdjacentModel(AdjacentLimits);
        FTRIADRFPathEvaluation AdjacentEvaluation;
        TestTrue(
            TEXT("Collapsed same-material span satisfies the interaction model"),
            AdjacentModel.EvaluatePath(Candidates[0], 2.4, AdjacentEvaluation));
        TestEqual(
            TEXT("Two metres of one material applies one paired-boundary loss"),
            AdjacentEvaluation.InteractionLossDb,
            25.0,
            1.0e-9);
        TestEqual(
            TEXT("Evaluation retains both contributors without changing physics"),
            AdjacentEvaluation.InteractionEvaluations.Num() == 1
                ? AdjacentEvaluation.InteractionEvaluations[0].Contributors.Num()
                : 0,
            2);
    }

    TestTrue(
        TEXT("Reverse adjacent same-material transmission candidate builds"),
        AdjacentQuery.BuildPathCandidates(
            AdjacentRight,
            AdjacentLeft,
            2.4,
            AdjacentLimits,
            Candidates,
            Error));
    TestEqual(
        TEXT("Reverse adjacent same-material ray also collapses to one span"),
        Candidates.Num() == 1 ? Candidates[0].Interactions.Num() : 0,
        1);
    if (Candidates.Num() == 1 && Candidates[0].Interactions.Num() == 1)
    {
        TestEqual(
            TEXT("Reverse same-material span retains the same chord length"),
            FVector::Distance(
                Candidates[0].Interactions[0].EntryPointCentimeters,
                Candidates[0].Interactions[0].ExitPointCentimeters),
            200.0,
            1.0e-9);
        TestEqual(
            TEXT("Reverse contributor order follows physical ray order"),
            Candidates[0].Interactions[0].Surface.Contributors.Num() == 2
                ? Candidates[0].Interactions[0].Surface.Contributors[0].SolidId
                : FString(),
            FString(TEXT("SOLID_B")));
    }

    const FString PerturbedAdjacentGeometry = MakeAdjacentGeometry(
        CatalogSha256,
        TEXT("MAT_TEST"),
        TEXT("1.00005"),
        TEXT("2.00005"));
    FTRIADRFIndexedGeometryQuery PerturbedAdjacentQuery;
    TestTrue(
        TEXT("Sub-tolerance same-material seam perturbation loads"),
        PerturbedAdjacentQuery.LoadFromJsonStrings(
            PerturbedAdjacentGeometry,
            Catalog,
            Error));
    TestTrue(
        TEXT("Sub-tolerance same-material seam perturbation builds"),
        PerturbedAdjacentQuery.BuildPathCandidates(
            AdjacentLeft,
            AdjacentRight,
            2.4,
            AdjacentLimits,
            Candidates,
            Error));
    TestEqual(
        TEXT("Sub-tolerance same-material seam remains one material span"),
        Candidates.Num() == 1 ? Candidates[0].Interactions.Num() : 0,
        1);
    if (Candidates.Num() == 1 && Candidates[0].Interactions.Num() == 1)
    {
        TestEqual(
            TEXT("Sub-tolerance seam is conservatively absorbed into the material chord"),
            FVector::Distance(
                Candidates[0].Interactions[0].EntryPointCentimeters,
                Candidates[0].Interactions[0].ExitPointCentimeters),
            200.005,
            1.0e-6);
    }

    const FString MixedCatalog = MakeMixedMaterialCatalog();
    const FString MixedGeometry = MakeAdjacentGeometry(
        Sha256Utf8(MixedCatalog),
        TEXT("MAT_OTHER"));
    FTRIADRFIndexedGeometryQuery MixedInterfaceQuery;
    TestTrue(
        TEXT("Adjacent mixed-material closed solids load"),
        MixedInterfaceQuery.LoadFromJsonStrings(
            MixedGeometry,
            MixedCatalog,
            Error));
    TestFalse(
        TEXT("Mixed-material shared interface fails closed"),
        MixedInterfaceQuery.BuildPathCandidates(
            AdjacentLeft,
            AdjacentRight,
            2.4,
            AdjacentLimits,
            Candidates,
            Error));
    TestEqual(
        TEXT("Mixed-material shared interface emits no candidate"),
        Candidates.Num(),
        0);
    TestTrue(
        TEXT("Mixed-material rejection identifies the missing transition prior"),
        Error.Contains(TEXT("Mixed-material shared RF interface")) &&
            Error.Contains(TEXT("no material-transition prior")));

    const FVector ClearStart(-100.0, 50.0, 60.0);
    const FVector ClearEnd(200.0, 50.0, 60.0);
    TestTrue(
        TEXT("Clear segment is inside the closed modeled coverage envelope"),
        Query.IsFiniteSegmentWithinModeledCoverage(ClearStart, ClearEnd, Error));
    TestTrue(TEXT("Clear segment candidate builds"), Query.BuildPathCandidates(ClearStart, ClearEnd, 2.4, ModelLimits, Candidates, Error));
    TestEqual(TEXT("Clear segment admits exactly one direct candidate"), Candidates.Num(), 1);
    if (Candidates.Num() == 1)
    {
        TestTrue(TEXT("Clear candidate is direct"), Candidates[0].Kind == ETRIADRFPathKind::Direct);
        TestEqual(TEXT("Direct candidate has no interactions"), Candidates[0].Interactions.Num(), 0);
    }

    const FVector OutsideCoverageStart(-100.01, 50.0, 60.0);
    TestFalse(
        TEXT("Endpoint outside the modeled coverage envelope fails closed"),
        Query.IsFiniteSegmentWithinModeledCoverage(
            OutsideCoverageStart,
            ClearEnd,
            Error));
    TestTrue(
        TEXT("Outside-coverage failure explains that clear/direct fallback is forbidden"),
        Error.Contains(TEXT("never interpreted as clear/direct")));
    TestFalse(
        TEXT("Outside-coverage path candidate query fails instead of reporting Direct"),
        Query.BuildPathCandidates(
            OutsideCoverageStart,
            ClearEnd,
            2.4,
            ModelLimits,
            Candidates,
            Error));
    TestEqual(
        TEXT("Outside-coverage path candidate query emits no candidate"),
        Candidates.Num(),
        0);
    TestFalse(
        TEXT("Outside-coverage trace fails instead of reporting a clear segment"),
        Query.TraceSegment(
            OutsideCoverageStart,
            ClearEnd,
            0.01,
            Hits,
            Error));
    TestEqual(TEXT("Outside-coverage trace emits no hits"), Hits.Num(), 0);

    const FVector CoverageCenter(50.0, 0.0, 50.0);
    const TArray<FVector> ClosedBoundaryFacePoints = {
        FVector(-100.0, 0.0, 50.0),
        FVector(200.0, 0.0, 50.0),
        FVector(50.0, -100.0, 50.0),
        FVector(50.0, 100.0, 50.0),
        FVector(50.0, 0.0, -100.0),
        FVector(50.0, 0.0, 200.0)};
    const TArray<FVector> EpsilonOutsideFacePoints = {
        FVector(-100.001, 0.0, 50.0),
        FVector(200.001, 0.0, 50.0),
        FVector(50.0, -100.001, 50.0),
        FVector(50.0, 100.001, 50.0),
        FVector(50.0, 0.0, -100.001),
        FVector(50.0, 0.0, 200.001)};
    for (int32 FaceIndex = 0; FaceIndex < ClosedBoundaryFacePoints.Num(); ++FaceIndex)
    {
        TestTrue(
            FString::Printf(TEXT("Closed modeled-coverage face %d is included"), FaceIndex),
            Query.IsFiniteSegmentWithinModeledCoverage(
                CoverageCenter,
                ClosedBoundaryFacePoints[FaceIndex],
                Error));
        TestFalse(
            FString::Printf(TEXT("Epsilon outside modeled-coverage face %d fails closed"), FaceIndex),
            Query.IsFiniteSegmentWithinModeledCoverage(
                CoverageCenter,
                EpsilonOutsideFacePoints[FaceIndex],
                Error));
    }

    TArray<FTRIADRFIndexedSegmentHit> AmbiguousHits;
    TestFalse(TEXT("Coplanar boundary travel fails closed"), Query.TraceSegment(FVector(-100.0, 0.0, 60.0), FVector(200.0, 0.0, 60.0), 0.01, AmbiguousHits, Error));
    TestEqual(TEXT("Ambiguous trace leaves no hits"), AmbiguousHits.Num(), 0);

    const FString OpaqueCatalog = MakeCatalog(false, false);
    const FString OpaqueGeometry = MakeGeometry(Sha256Utf8(OpaqueCatalog));
    FTRIADRFIndexedGeometryQuery OpaqueQuery;
    TestTrue(TEXT("Explicit opaque catalog loads"), OpaqueQuery.LoadFromJsonStrings(OpaqueGeometry, OpaqueCatalog, Error));
    TestTrue(TEXT("Opaque material query succeeds"), OpaqueQuery.BuildPathCandidates(Left, Right, 2.4, ModelLimits, Candidates, Error));
    TestEqual(TEXT("Opaque straight path admits no candidate"), Candidates.Num(), 0);

    const FString BoundaryCatalog = MakeCatalog(true, true);
    const FString BoundaryGeometry = MakeGeometry(Sha256Utf8(BoundaryCatalog));
    FTRIADRFIndexedGeometryQuery BoundaryQuery;
    TestTrue(TEXT("Adjacent runtime profiles load"), BoundaryQuery.LoadFromJsonStrings(BoundaryGeometry, BoundaryCatalog, Error));
    TestFalse(TEXT("Shared frequency boundary is ambiguous and fails closed"), BoundaryQuery.BuildPathCandidates(Left, Right, 2.4, ModelLimits, Candidates, Error));
    TestEqual(TEXT("Ambiguous profile emits no candidate"), Candidates.Num(), 0);

    FString WrongHashGeometry = MakeGeometry(FString::ChrN(64, TEXT('f')));
    TestFalse(TEXT("Catalog hash mismatch rejects loading"), Query.LoadFromJsonStrings(WrongHashGeometry, Catalog, Error));
    TestFalse(TEXT("Failed reload clears a formerly valid query"), Query.IsReady());
    TestEqual(TEXT("Failed reload exposes no stale triangles"), Query.GetTriangleCount(), 0);

    FString OpenGeometry = Geometry;
    OpenGeometry.ReplaceInline(TEXT("\"triangleCount\":12"), TEXT("\"triangleCount\":11"), ESearchCase::CaseSensitive);
    TestFalse(TEXT("Open or unbound manifold fails closed"), Query.LoadFromJsonStrings(OpenGeometry, Catalog, Error));
    TestFalse(TEXT("Invalid manifold remains unavailable"), Query.IsReady());

    FString PrimitiveFreeGeometry = Geometry;
    PrimitiveFreeGeometry.ReplaceInline(
        TEXT(",\"primitive\":{\"type\":\"AXIS_ALIGNED_BOX\",\"minMeters\":[0,0,0],\"maxMeters\":[1,1,1]}"),
        TEXT(""),
        ESearchCase::CaseSensitive);
    TestFalse(
        TEXT("Missing canonical finite primitive fails closed"),
        Query.LoadFromJsonStrings(PrimitiveFreeGeometry, Catalog, Error));
    TestFalse(TEXT("Primitive-free geometry remains unavailable"), Query.IsReady());

    FString GeneralizedCoverageGeometry = Geometry;
    GeneralizedCoverageGeometry.ReplaceInline(
        TEXT("\"scope\":\"MAIN_HERO_ONLY_ASSUMPTION_BOUND\""),
        TEXT("\"scope\":\"ONE_KILOMETRE_CONTEXT_ASSUMPTION_BOUND\""),
        ESearchCase::CaseSensitive);
    GeneralizedCoverageGeometry.ReplaceInline(
        TEXT("\"coversOneKilometreAoi\":false"),
        TEXT("\"coversOneKilometreAoi\":true"),
        ESearchCase::CaseSensitive);
    GeneralizedCoverageGeometry.ReplaceInline(
        TEXT("\"coversSurroundings\":false"),
        TEXT("\"coversSurroundings\":true"),
        ESearchCase::CaseSensitive);
    GeneralizedCoverageGeometry.ReplaceInline(
        TEXT("\"surveyControlled\":false"),
        TEXT("\"surveyControlled\":true"),
        ESearchCase::CaseSensitive);
    GeneralizedCoverageGeometry.ReplaceInline(
        TEXT("\"fieldValidated\":false"),
        TEXT("\"fieldValidated\":true"),
        ESearchCase::CaseSensitive);
    GeneralizedCoverageGeometry.ReplaceInline(
        TEXT("\"coversCanonicalHeroGeometry\":true"),
        TEXT("\"coversCanonicalGeometry\":true"),
        ESearchCase::CaseSensitive);
    TestTrue(
        TEXT("Scenario-pinned generalized modeled-coverage metadata loads"),
        Query.LoadFromJsonStrings(GeneralizedCoverageGeometry, Catalog, Error));
    TestEqual(
        TEXT("Generalized modeled-coverage scope is retained verbatim"),
        Query.GetMetadata().ModeledCoverageScope,
        FString(TEXT("ONE_KILOMETRE_CONTEXT_ASSUMPTION_BOUND")));
    TestTrue(
        TEXT("One-kilometre modeled-coverage claim is retained for scenario validation"),
        Query.GetMetadata().bModeledCoverageCoversOneKilometreAoi);
    TestTrue(
        TEXT("Surroundings modeled-coverage claim is retained for scenario validation"),
        Query.GetMetadata().bModeledCoverageCoversSurroundings);
    TestTrue(
        TEXT("Survey-control claim is retained for scenario validation"),
        Query.GetMetadata().bModeledCoverageSurveyControlled);
    TestTrue(
        TEXT("Field-validation claim is retained for scenario validation"),
        Query.GetMetadata().bModeledCoverageFieldValidated);

    FString EmptyCoverageScopeGeometry = Geometry;
    EmptyCoverageScopeGeometry.ReplaceInline(
        TEXT("\"scope\":\"MAIN_HERO_ONLY_ASSUMPTION_BOUND\""),
        TEXT("\"scope\":\"\""),
        ESearchCase::CaseSensitive);
    TestFalse(
        TEXT("Empty modeled-coverage scope fails closed"),
        Query.LoadFromJsonStrings(EmptyCoverageScopeGeometry, Catalog, Error));
    TestFalse(TEXT("Empty coverage scope leaves query unavailable"), Query.IsReady());

    FString ExpandedCoverageGeometry = Geometry;
    ExpandedCoverageGeometry.ReplaceInline(
        TEXT("\"boundsMeters\":{\"min\":[-1,-1,-1],\"max\":[2,1,2]}"),
        TEXT("\"boundsMeters\":{\"min\":[-1,-1,-1],\"max\":[2000,1,2]}"),
        ESearchCase::CaseSensitive);
    TestFalse(
        TEXT("Expanded modeled-coverage AABB fails exact derived-bounds validation"),
        Query.LoadFromJsonStrings(ExpandedCoverageGeometry, Catalog, Error));
    TestFalse(TEXT("Expanded coverage leaves query unavailable"), Query.IsReady());

    FString MissingWitnessGeometry = Geometry;
    MissingWitnessGeometry.ReplaceInline(
        TEXT("\"witnesses\":[{\"witnessId\":\"WIT_TEST_COVERAGE_EXTENTS\",\"startMeters\":[-1,-1,-1],\"endMeters\":[2,1,2]}],"),
        TEXT(""),
        ESearchCase::CaseSensitive);
    TestFalse(
        TEXT("Missing coverage witnesses fail closed"),
        Query.LoadFromJsonStrings(MissingWitnessGeometry, Catalog, Error));
    TestFalse(TEXT("Missing witnesses leave query unavailable"), Query.IsReady());

    FString MalformedWitnessGeometry = Geometry;
    MalformedWitnessGeometry.ReplaceInline(
        TEXT("\"endMeters\":[2,1,2]}]"),
        TEXT("\"endMeters\":[2,1]}]"),
        ESearchCase::CaseSensitive);
    TestFalse(
        TEXT("Malformed coverage witness endpoint fails closed"),
        Query.LoadFromJsonStrings(MalformedWitnessGeometry, Catalog, Error));
    TestFalse(TEXT("Malformed witness leaves query unavailable"), Query.IsReady());

    FString TightGeometryPath;
    FString TightCatalogPath;
    const bool bHasTightGeometry = FParse::Value(
        FCommandLine::Get(), TEXT("TRIADRFTightGeometry="), TightGeometryPath);
    const bool bHasTightCatalog = FParse::Value(
        FCommandLine::Get(), TEXT("TRIADRFTightCatalog="), TightCatalogPath);
    TestTrue(
        TEXT("TightV1 optional geometry/catalog arguments are supplied as a pair"),
        bHasTightGeometry == bHasTightCatalog);
    if (bHasTightGeometry && bHasTightCatalog)
    {
        FTRIADRFIndexedGeometryQuery WrongExpectedFileHashQuery;
        TestFalse(
            TEXT("Production file API rejects a configured hash before committing parsed state"),
            WrongExpectedFileHashQuery.LoadFromJsonFiles(
                TightGeometryPath,
                TightCatalogPath,
                FString::ChrN(64, TEXT('f')),
                TEXT("0089fed936494ecd6938dc14d34ea8d5540e0896c2dc814414e071b6d1081a63"),
                Error));
        TestFalse(
            TEXT("Expected-hash mismatch leaves production file query unavailable"),
            WrongExpectedFileHashQuery.IsReady());

        FTRIADRFIndexedGeometryQuery TightQuery;
        TestTrue(
            TEXT("Exact TightV1 hash-bound artifact loads through the production file API"),
            TightQuery.LoadFromJsonFiles(
                TightGeometryPath,
                TightCatalogPath,
                TEXT("a3705e22b47fbe4fd38936294890e1ad620bb3c618ac3c93ca6c4719c6edc62a"),
                TEXT("0089fed936494ecd6938dc14d34ea8d5540e0896c2dc814414e071b6d1081a63"),
                Error));
        TestEqual(TEXT("Exact TightV1 vertex census"), TightQuery.GetVertexCount(), 2976);
        TestEqual(TEXT("Exact TightV1 triangle census"), TightQuery.GetTriangleCount(), 4464);
        TestEqual(TEXT("Exact TightV1 solid census"), TightQuery.GetSolidCount(), 372);
        TestEqual(TEXT("Exact TightV1 surface census"), TightQuery.GetSurfaceCount(), 2232);
        TestEqual(TEXT("Exact TightV1 material census"), TightQuery.GetMaterialCount(), 7);
        TestEqual(
            TEXT("Exact TightV1 consumed geometry hash retained"),
            TightQuery.GetMetadata().GeometrySha256,
            FString(TEXT("a3705e22b47fbe4fd38936294890e1ad620bb3c618ac3c93ca6c4719c6edc62a")));
        TestEqual(
            TEXT("Exact TightV1 material catalog hash retained"),
            TightQuery.GetMetadata().MaterialCatalogSha256,
            FString(TEXT("0089fed936494ecd6938dc14d34ea8d5540e0896c2dc814414e071b6d1081a63")));
        TestEqual(
            TEXT("Exact TightV1 contract hash retained"),
            TightQuery.GetMetadata().ContractSha256,
            FString(TEXT("19002f898c34c23793038c42892f122642f112bd514a0ebb05e4969dcb9ae591")));
        TestEqual(
            TEXT("Exact TightV1 modeled coverage ID retained"),
            TightQuery.GetMetadata().ModeledCoverageId,
            FString(TEXT("RF_COVERAGE_ISTANA_MAIN_HERO_TIGHT_V1")));
        TestFalse(
            TEXT("Exact TightV1 modeled coverage is not the one-kilometre AOI"),
            TightQuery.GetMetadata().bModeledCoverageCoversOneKilometreAoi);

        TArray<FTRIADRFIndexedSegmentHit> TightHits;
        TestTrue(
            TEXT("Exact TightV1 assumed-open west portal traces"),
            TightQuery.TraceSegment(
                FVector(-720.0, -2330.0, 100.0),
                FVector(-720.0, -2170.0, 100.0),
                0.1,
                TightHits,
                Error));
        TestEqual(TEXT("Exact TightV1 open portal has no blocking crossings"), TightHits.Num(), 0);

        TestTrue(
            TEXT("Exact TightV1 assumed-closed centre door traces"),
            TightQuery.TraceSegment(
                FVector(-120.0, -2330.0, 100.0),
                FVector(-120.0, -2170.0, 100.0),
                0.1,
                TightHits,
                Error));
        TestEqual(TEXT("Exact TightV1 centre door has one entry/exit pair"), TightHits.Num(), 2);
        if (TightHits.Num() == 2)
        {
            TestEqual(
                TEXT("Exact TightV1 centre-door material ID retained"),
                TightHits[0].MaterialId,
                FString(TEXT("MAT_RF_TIMBER_DOOR_ASSUMED")));
            TestEqual(
                TEXT("Exact TightV1 centre-door solid ID retained"),
                TightHits[0].SolidId,
                FString(TEXT("SOL_AP_PORTAL_CENTRE_DOOR_LEAF")));
            TestEqual(
                TEXT("Exact TightV1 aperture ID retained"),
                TightHits[0].ApertureId,
                FString(TEXT("AP_PORTAL_CENTRE_DOOR")));
            TestEqual(
                TEXT("Exact TightV1 aperture state retained"),
                TightHits[0].ApertureState,
                FString(TEXT("CLOSED")));
        }
        TestTrue(
            TEXT("Exact TightV1 centre-door transmission candidate builds"),
            TightQuery.BuildPathCandidates(
                FVector(-120.0, -2330.0, 100.0),
                FVector(-120.0, -2170.0, 100.0),
                2.4,
                ModelLimits,
                Candidates,
                Error));
        TestEqual(TEXT("Exact TightV1 door admits one assumption-bound transmission"), Candidates.Num(), 1);
    }
    else
    {
        AddInfo(TEXT("Exact TightV1 file load was not requested; pass -TRIADRFTightGeometry and -TRIADRFTightCatalog to exercise it."));
    }
    return true;
}

#endif
