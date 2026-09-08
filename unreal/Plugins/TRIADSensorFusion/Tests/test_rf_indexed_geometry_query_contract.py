from __future__ import annotations

from collections import Counter, defaultdict
import hashlib
import json
import math
import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PUBLIC_HEADER = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADRFIndexedGeometryQuery.h"
)
IMPLEMENTATION = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADRFIndexedGeometryQuery.cpp"
)
NATIVE_TEST = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/Tests/TRIADRFIndexedGeometryQueryTests.cpp"
)
ACTUAL_FILE_NATIVE_TEST = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/Tests/TRIADRFOneKilometreActualFileTests.cpp"
)
TIGHT_ROOT = REPO / "unreal/SourceAssets/IstanaPublicViewRF/TightV1"
GEOMETRY = TIGHT_ROOT / "Generated/IstanaPublicViewRFTightV1.geometry.json"
CATALOG = TIGHT_ROOT / "istana_rf_materials_tight_v1.catalog.json"
ONE_KILOMETRE_GEOMETRY = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Resources/RF"
    / "IstanaPublicViewRFOneKilometreV2.geometry.json"
)


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def cross(
    first: tuple[float, float, float], second: tuple[float, float, float]
) -> tuple[float, float, float]:
    return (
        first[1] * second[2] - first[2] * second[1],
        first[2] * second[0] - first[0] * second[2],
        first[0] * second[1] - first[1] * second[0],
    )


def subtract(
    first: tuple[float, float, float], second: tuple[float, float, float]
) -> tuple[float, float, float]:
    return tuple(first[index] - second[index] for index in range(3))


def dot(first: tuple[float, float, float], second: tuple[float, float, float]) -> float:
    return sum(first[index] * second[index] for index in range(3))


class RFIndexedGeometryQueryContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = text(PUBLIC_HEADER)
        cls.cpp = text(IMPLEMENTATION)
        cls.native_test = text(NATIVE_TEST)
        cls.actual_file_native_test = text(ACTUAL_FILE_NATIVE_TEST)
        cls.geometry = json.loads(text(GEOMETRY))
        cls.catalog = json.loads(text(CATALOG))

    def test_query_package_is_additive_and_readiness_closed(self) -> None:
        self.assertIn("public ITRIADRFGeometryQuery", self.header)
        self.assertIn("LoadFromJsonFiles", self.header)
        self.assertIn("LoadFromJsonStrings", self.header)
        self.assertIn("TraceSegment", self.header)
        self.assertIn("IsFiniteSegmentWithinModeledCoverage", self.header)
        self.assertIn("GeometrySha256", self.header)
        self.assertIn("MaximumWitnesses", self.header)
        self.assertIn("BuildPathCandidates", self.header)
        self.assertIn("TryGetMaterialMetadata", self.header)
        self.assertIn("TryGetSolidMetadata", self.header)
        self.assertIn("TryGetSurfaceMetadata", self.header)
        self.assertNotIn("TRIADSensorFusionScenarioManager", self.header + self.cpp)
        self.assertNotRegex(
            self.header + self.cpp,
            r"bReadyForSurveyTruth\s*=\s*true",
        )
        self.assertIn("Reflection candidate\n * enumeration remains", self.header)

    def test_loader_has_real_resource_and_schema_guards(self) -> None:
        for marker in (
            "HardMaximumJsonBytesPerDocument",
            "HardMaximumVertices",
            "HardMaximumTriangles",
            "HardMaximumSolids",
            "HardMaximumSurfaces",
            "HardMaximumWitnesses",
            "HardMaximumMaterials",
            "HardMaximumProfilesPerMaterial",
            "HardMaximumBVHDepth",
            "HardMaximumOverlapPairChecks",
            "HardMaximumSelfIntersectionCandidateChecks",
            "HardMaximumCrossSolidTriangleCandidateChecks",
            "HardMaximumContainmentTriangleChecks",
            'triad.istana_public_view.rf_geometry.v1',
            'triad.rf_material_catalog.v1',
            'SIMULATION_READY_ASSUMPTION_BOUND',
            'UNCALIBRATED_ASSUMPTION_PRIORS',
            'runtimeInteractionProfileV2',
            'coefficients will not be invented',
            "FPortableSHA256",
            "strict, round-trippable UTF-8",
            "Coverage identity and claim flags are scenario-pinned metadata",
            "REJECT_QUERY_NEVER_EMIT_CLEAR_OR_DIRECT_PATH",
            "Canonical RF vertex lies outside the declared modeled coverage envelope",
            "Canonical RF witness count is empty or exceeds the configured bound",
            "exact minimum closed AABB containing canonical vertices and witness endpoints",
            "RF single-read buffer hashes differ from the exact buffers parsed",
        ):
            self.assertIn(marker, self.cpp)
        self.assertNotRegex(
            self.cpp,
            r"FPlatformMisc::GetSHA256Signature\s*\(",
        )
        self.assertIn("Reset();", self.cpp)
        self.assertIn("Candidate->Parse", self.cpp)
        self.assertIn("Impl = MoveTemp(Candidate)", self.cpp)
        self.assertNotIn("ExpectedModeledCoverageScope", self.cpp)
        self.assertNotRegex(
            self.cpp,
            r"ModeledCoverageScope\s*!=\s*",
        )

    def test_coordinate_transform_winding_and_bvh_are_explicit(self) -> None:
        for marker in (
            "LogicalMeters.X * 100.0",
            "-LogicalMeters.Y * 100.0",
            "LogicalMeters.Z * 100.0",
            "Triangle.VertexIndices[0] = LogicalIndices[0]",
            "Triangle.VertexIndices[1] = LogicalIndices[2]",
            "Triangle.VertexIndices[2] = LogicalIndices[1]",
            "SignedSixVolume",
            "VolumeOrigin",
            "VolumeCompensation",
            "non-finite signed-volume term",
            "Edge.DirectionBalance",
            "VisitedCount != Solid.TriangleCount",
            "ValidateAxisAlignedBoxMesh",
            "exactly 8 vertices and 12 triangles",
            "eight unique min/max corners",
            "two complete triangles on every face plane",
            "contains an unused vertex",
            "BuildBVHNode",
            "TrianglePermutation",
            "IntersectFiniteSegmentAabb",
            "IntersectTriangle",
        ):
            self.assertIn(marker, self.cpp)

    def test_segment_semantics_are_ordered_or_fail_closed(self) -> None:
        for marker in (
            "CoplanarAmbiguous",
            "EndpointAmbiguous",
            "IncidenceTangentEpsilon",
            "sharp edge/corner ambiguously",
            "unpaired triangle-edge contact",
            "non-alternating oriented crossings",
            "segment starts inside solid",
            "segment ends inside solid",
            "ETRIADRFBoundaryCrossing::Entry",
            "ETRIADRFBoundaryCrossing::Exit",
            "RawHits.Sort",
            "Pairs.Sort",
            "IsFiniteSegmentWithinModeledCoverage(Start, End, OutError)",
            "never interpreted as clear/direct paths",
        ):
            self.assertIn(marker, self.cpp)

    def test_shared_boundary_candidate_semantics_are_conservative(self) -> None:
        for marker in (
            "Same-material intervals are collapsed into one continuous material span",
            "not a sound material-to-material transition coefficient",
        ):
            self.assertIn(marker, self.header)
        for marker in (
            "TransmissionSpans",
            "SeamParameterTolerance",
            "bCoincidentWithinTolerance",
            "Same-material continuity",
            "applying one paired-boundary prior",
            "Previous.Exit = Current.Exit",
            "Previous.Contributors.Append(Current.Contributors)",
            "Interaction.Surface.Contributors = Pair.Contributors",
            "Mixed-material shared RF interface",
            "no material-transition prior is available",
            "transmission ordering is ambiguous",
            "RF material spans exceed the interaction bound",
        ):
            self.assertIn(marker, self.cpp)
        self.assertRegex(
            self.cpp,
            r"Previous\.Exit->MaterialId\s*!=\s*Current\.Entry->MaterialId",
        )
        self.assertRegex(
            self.cpp,
            r"Previous\.Exit->OutwardNormal[\s\S]{0,160}"
            r"Current\.Entry->OutwardNormal[\s\S]{0,160}"
            r">\s*-NormalAgreementCosine",
        )

    def test_review_regressions_have_native_coverage(self) -> None:
        for marker in (
            "A tetrahedron relabeled as an axis-aligned box fails closed",
            "Malformed indexed boundary cannot retain AXIS_ALIGNED_BOX semantics",
            "Unused vertex inside a solid range fails closed",
            "Positive volume remains stable far from the world origin",
            "Conflicting preferred and legacy coverage aliases fail closed",
            "Collapsed material span retains both physical contributors",
            "Evaluation retains both contributors without changing physics",
        ):
            self.assertIn(marker, self.native_test)
        self.assertRegex(
            self.cpp,
            r"SecondTriangle\.Bounds\.Min\.X[\s\S]{0,500}"
            r"\+\+CandidateChecks[\s\S]{0,500}"
            r"BoxesOverlapInclusive",
        )
        self.assertRegex(
            self.cpp,
            r"SecondSolid\.TriangleStart\s*\+\s*SecondOffset[\s\S]{0,300}"
            r"\+\+InOutTriangleCandidateChecks[\s\S]{0,500}"
            r"BoxesOverlapInclusive",
        )
        self.assertIn("conflicts with legacy alias", self.cpp)
        self.assertIn(
            "for (const FPair& Pair : TransmissionSpans)",
            self.cpp,
        )
        self.assertNotIn(
            "Pairs.Num() > Limits.MaximumInteractionsPerPath",
            self.cpp,
        )

    def test_exact_p0_catalog_is_hash_bound_and_runtime_compatible(self) -> None:
        self.assertEqual(self.catalog["schemaVersion"], "triad.rf_material_catalog.v1")
        self.assertEqual(self.catalog["status"], "UNCALIBRATED_ASSUMPTION_PRIORS")
        actual_hash = hashlib.sha256(CATALOG.read_bytes()).hexdigest()
        self.assertEqual(self.geometry["materialCatalogSha256"], actual_hash)
        self.assertRegex(self.geometry["contractSha256"], r"^[0-9a-f]{64}$")
        material_ids: set[str] = set()
        for material in self.catalog["materials"]:
            material_id = material["materialId"]
            self.assertNotIn(material_id, material_ids)
            material_ids.add(material_id)
            self.assertEqual(material["calibrationState"], "UNCALIBRATED_ASSUMPTION")
            runtime = material["runtimeInteractionProfileV2"]
            self.assertEqual(runtime["calibrationState"], "UNCALIBRATED")
            self.assertEqual(
                runtime["modelSemantics"],
                "PARAMETRIC_LOOKDEV_DIRECT_STRAIGHT_TRANSMISSION_"
                "SINGLE_SPECULAR_REFLECTION_V2_INCOHERENT_ONLY",
            )
            self.assertTrue(runtime["calibrationProvenanceId"])
            self.assertTrue(runtime["profiles"])
            for profile in runtime["profiles"]:
                self.assertGreater(profile["minimumFrequencyGHz"], 0.0)
                self.assertGreaterEqual(
                    profile["maximumFrequencyGHz"], profile["minimumFrequencyGHz"]
                )
                self.assertGreaterEqual(profile["minimumIncidenceCosine"], 1e-8)
                self.assertLessEqual(profile["maximumIncidenceCosine"], 1.0)
                for field in (
                    "pairedBoundaryTransmissionLossDb",
                    "bulkAttenuationDbPerMeter",
                    "reflectionLossDb",
                    "empiricalGrazingReflectionLossDb",
                ):
                    self.assertTrue(math.isfinite(profile[field]))
                    self.assertGreaterEqual(profile[field], 0.0)
                self.assertIsInstance(profile["allowsTransmission"], bool)
                self.assertIsInstance(profile["allowsReflection"], bool)

    def test_exact_p0_index_ranges_and_stable_ids_are_total(self) -> None:
        geometry = self.geometry
        self.assertEqual(
            geometry["schemaVersion"], "triad.istana_public_view.rf_geometry.v1"
        )
        self.assertEqual(geometry["status"], "SIMULATION_READY_ASSUMPTION_BOUND")
        self.assertEqual(
            geometry["coordinateContract"]["logicalSystem"],
            "RIGHT_HANDED_Z_UP_LOCAL_METRES_APPROACH_PLUS_Y",
        )
        coverage = geometry["modeledCoverageEnvelope"]
        self.assertEqual(coverage["scope"], "MAIN_HERO_ONLY_ASSUMPTION_BOUND")
        self.assertEqual(coverage["shape"], "AXIS_ALIGNED_BOX")
        self.assertEqual(coverage["boundaryInclusion"], "CLOSED")
        self.assertEqual(
            coverage["outsideDomainPolicy"],
            "REJECT_QUERY_NEVER_EMIT_CLEAR_OR_DIRECT_PATH",
        )
        self.assertEqual(
            coverage["boundsMeters"],
            {"min": [-57.5, -56.0, 0.0], "max": [57.5, 23.3, 36.0]},
        )
        for claim in (
            "coversOneKilometreAoi",
            "coversSurroundings",
            "surveyControlled",
            "fieldValidated",
        ):
            self.assertFalse(coverage[claim], claim)
        self.assertLessEqual(len(geometry["verticesMeters"]), 1_000_000)
        self.assertLessEqual(len(geometry["triangles"]), 1_000_000)
        self.assertLessEqual(len(geometry["solids"]), 100_000)
        self.assertLessEqual(len(geometry["surfaces"]), 200_000)
        material_ids = {item["materialId"] for item in self.catalog["materials"]}
        solid_ids = [item["solidId"] for item in geometry["solids"]]
        surface_ids = [item["surfaceId"] for item in geometry["surfaces"]]
        self.assertEqual(len(solid_ids), len(set(solid_ids)))
        self.assertEqual(len(surface_ids), len(set(surface_ids)))

        vertex_owner = [-1] * len(geometry["verticesMeters"])
        triangle_solid_owner = [-1] * len(geometry["triangles"])
        solid_by_id: dict[str, tuple[int, dict[str, object]]] = {}
        for solid_index, solid in enumerate(geometry["solids"]):
            self.assertIn(solid["materialId"], material_ids)
            self.assertEqual(solid["primitive"]["type"], "AXIS_ALIGNED_BOX")
            solid_by_id[solid["solidId"]] = solid_index, solid
            vertex_range = range(
                solid["vertexStart"], solid["vertexStart"] + solid["vertexCount"]
            )
            triangle_range = range(
                solid["triangleStart"],
                solid["triangleStart"] + solid["triangleCount"],
            )
            for vertex_index in vertex_range:
                self.assertEqual(vertex_owner[vertex_index], -1)
                vertex_owner[vertex_index] = solid_index
            for triangle_index in triangle_range:
                self.assertEqual(triangle_solid_owner[triangle_index], -1)
                triangle_solid_owner[triangle_index] = solid_index
                self.assertTrue(
                    all(index in vertex_range for index in geometry["triangles"][triangle_index])
                )
        self.assertNotIn(-1, vertex_owner)
        self.assertNotIn(-1, triangle_solid_owner)

        triangle_surface_owner = [-1] * len(geometry["triangles"])
        for surface_index, surface in enumerate(geometry["surfaces"]):
            self.assertIn(surface["solidId"], solid_by_id)
            solid_index, solid = solid_by_id[surface["solidId"]]
            self.assertEqual(surface["materialId"], solid["materialId"])
            for triangle_index in range(
                surface["triangleStart"],
                surface["triangleStart"] + surface["triangleCount"],
            ):
                self.assertEqual(triangle_surface_owner[triangle_index], -1)
                self.assertEqual(triangle_solid_owner[triangle_index], solid_index)
                triangle_surface_owner[triangle_index] = surface_index
        self.assertNotIn(-1, triangle_surface_owner)

    def test_exact_p0_solids_are_connected_closed_outward_manifolds(self) -> None:
        vertices = [tuple(map(float, item)) for item in self.geometry["verticesMeters"]]
        triangles = [tuple(map(int, item)) for item in self.geometry["triangles"]]
        for solid in self.geometry["solids"]:
            first = solid["triangleStart"]
            count = solid["triangleCount"]
            edges: Counter[tuple[int, int]] = Counter()
            directed: Counter[tuple[int, int]] = Counter()
            neighbours: dict[int, set[int]] = defaultdict(set)
            edge_triangle: dict[tuple[int, int], int] = {}
            signed_six_volume = 0.0
            for local_index, triangle in enumerate(triangles[first : first + count]):
                a, b, c = (vertices[index] for index in triangle)
                signed_six_volume += dot(a, cross(b, c))
                for start, end in (
                    (triangle[0], triangle[1]),
                    (triangle[1], triangle[2]),
                    (triangle[2], triangle[0]),
                ):
                    key = tuple(sorted((start, end)))
                    edges[key] += 1
                    directed[(start, end)] += 1
                    if key in edge_triangle:
                        other = edge_triangle[key]
                        neighbours[local_index].add(other)
                        neighbours[other].add(local_index)
                    else:
                        edge_triangle[key] = local_index
            self.assertTrue(all(value == 2 for value in edges.values()), solid["solidId"])
            for start, end in edges:
                self.assertEqual(directed[(start, end)], 1, solid["solidId"])
                self.assertEqual(directed[(end, start)], 1, solid["solidId"])
            self.assertGreater(signed_six_volume / 6.0, 0.0, solid["solidId"])
            seen = {0}
            stack = [0]
            while stack:
                current = stack.pop()
                for other in neighbours[current]:
                    if other not in seen:
                        seen.add(other)
                        stack.append(other)
            self.assertEqual(len(seen), count, solid["solidId"])

    def test_generic_indexed_polyhedron_foundation_is_bounded_and_fail_closed(self) -> None:
        for marker in (
            "MaximumSelfIntersectionCandidateChecks",
            "MaximumCrossSolidTriangleCandidateChecks",
            "MaximumContainmentTriangleChecks",
            "PrimitiveType",
            "INDEXED_CLOSED_POLYHEDRON",
            "ValidateTriangleSelfIntersections",
            "ClassifyTriangleContact",
            "TrianglePlaneParallelCrossSquaredTolerance",
            "IsTriangleWithinPlaneTolerance",
            "bMutuallyWithinPlaneTolerance",
            "ClassifyPointAgainstSolid",
            "GenericSolidsHavePositiveVolumeOverlap",
            "triangle self-intersection",
            "positive-volume overlap or containment",
            "deterministic candidate-check ceiling",
        ):
            self.assertIn(marker, self.header + self.cpp)
        for marker in (
            "Sloped indexed closed polyhedron loads",
            "Generic primitive type metadata is retained",
            "Finite segment traces through the sloped indexed face",
            "Convex prism with near-collinear adjacent walls loads",
            "Near-collinear convex indexed query is ready",
            "Convex prism with sub-tolerance near-coplanar kink loads",
            "Sub-tolerance kink convex indexed query is ready",
            "Self-intersecting indexed closed polyhedron fails closed",
            "Partially overlapping indexed polyhedra fail closed",
            "Contained indexed polyhedron fails closed",
            "Zero-volume shared-face boundary touch remains valid",
            "TightV1-style axis-aligned primitive metadata is retained",
        ):
            self.assertIn(marker, self.native_test)
        self.assertIn(
            'PrimitiveType == TEXT("AXIS_ALIGNED_BOX")',
            self.cpp,
        )
        self.assertIn(
            "First.bAxisAlignedBoxPrimitive && Second.bAxisAlignedBoxPrimitive",
            self.cpp,
        )
        self.assertRegex(
            self.cpp,
            r"PlaneLineDirection\.SizeSquared\(\)\s*<=\s*"
            r"TrianglePlaneParallelCrossSquaredTolerance",
        )
        self.assertRegex(
            self.cpp,
            r"PlaneLineDirection\.GetSafeNormal\(\s*"
            r"TrianglePlaneParallelCrossSquaredTolerance\s*\)",
        )
        self.assertNotIn("PlaneLineDirection.GetSafeNormal();", self.cpp)
        self.assertRegex(
            self.cpp,
            r"bMutuallyWithinPlaneTolerance\s*=\s*"
            r"IsTriangleWithinPlaneTolerance\(\s*"
            r"First,\s*Second\[0\],\s*SecondNormal\s*\)\s*&&\s*"
            r"IsTriangleWithinPlaneTolerance\(\s*"
            r"Second,\s*First\[0\],\s*FirstNormal\s*\)",
        )
        self.assertRegex(
            self.cpp,
            r"PlaneLineDirection\.SizeSquared\(\)\s*<=\s*"
            r"TrianglePlaneParallelCrossSquaredTolerance\s*&&\s*"
            r"!bMutuallyWithinPlaneTolerance",
        )
        self.assertRegex(
            self.cpp,
            r"if\s*\(bMutuallyWithinPlaneTolerance\)\s*\{\s*"
            r"const FVector AbsoluteNormal",
        )

    def test_containment_ceiling_has_a_pinned_large_file_opt_in_and_exact_boundary(self) -> None:
        default_match = re.search(
            r"MaximumContainmentTriangleChecks\s*=\s*(\d+)\s*;",
            self.header,
        )
        hard_match = re.search(
            r"HardMaximumContainmentTriangleChecks\s*=\s*(\d+)\s*;",
            self.cpp,
        )
        replay_match = re.search(
            r"PinnedContainmentTriangleChecks\s*=\s*(\d+)\s*;",
            self.actual_file_native_test,
        )
        actual_limit_match = re.search(
            r"ActualFileContainmentTriangleCheckLimit\s*=\s*(\d+)\s*;",
            self.actual_file_native_test,
        )
        self.assertIsNotNone(default_match)
        self.assertIsNotNone(hard_match)
        self.assertIsNotNone(replay_match)
        self.assertIsNotNone(actual_limit_match)
        default_limit = int(default_match.group(1))
        hard_limit = int(hard_match.group(1))
        replay_checks = int(replay_match.group(1))
        actual_limit = int(actual_limit_match.group(1))
        self.assertEqual(20_000_000, default_limit)
        self.assertEqual(29_904_936, replay_checks)
        self.assertEqual(30_000_000, actual_limit)
        self.assertEqual(actual_limit, hard_limit)
        self.assertEqual(95_064, actual_limit - replay_checks)
        self.assertIn(
            "LoadLimits.MaximumContainmentTriangleChecks =",
            self.actual_file_native_test,
        )
        self.assertIn(
            "FTRIADRFIndexedGeometryQuery Query(LoadLimits)",
            self.actual_file_native_test,
        )
        for marker in (
            "positive AABB overlap",
            "exactly 52 triangle checks",
            "BelowContainmentBudgetLimits.MaximumContainmentTriangleChecks = 51",
            "ExactContainmentBudgetLimits.MaximumContainmentTriangleChecks = 52",
            "Containment validation fails at one check below its exact bounded workload",
            "Containment validation succeeds at its exact bounded workload",
        ):
            self.assertIn(marker, self.native_test)

        geometry = json.loads(text(ONE_KILOMETRE_GEOMETRY))
        pending = [(len(geometry["triangles"]), 0)]
        node_count = 0
        maximum_depth = 0
        while pending:
            triangle_count, depth = pending.pop()
            node_count += 1
            maximum_depth = max(maximum_depth, depth)
            if triangle_count > 8:
                left_count = triangle_count // 2
                pending.append((left_count, depth + 1))
                pending.append((triangle_count - left_count, depth + 1))
        self.assertEqual(42_700, len(geometry["triangles"]))
        self.assertEqual(16_383, node_count)
        self.assertEqual(13, maximum_depth)
        self.assertIn(
            "Pinned full-file deterministic BVH node count",
            self.actual_file_native_test,
        )
        self.assertRegex(
            self.actual_file_native_test,
            r"Query\.GetBVHNodeCount\(\),\s*16383",
        )

    def test_actual_file_shallow_kink_is_mutually_within_plane_tolerance(self) -> None:
        geometry = json.loads(text(ONE_KILOMETRE_GEOMETRY))
        triangle_indices = (5434, 5436)
        logical_triangles = [geometry["triangles"][index] for index in triangle_indices]
        self.assertEqual(logical_triangles, [[3484, 3485, 3494], [3485, 3489, 3498]])
        self.assertEqual(
            set(logical_triangles[0]).intersection(logical_triangles[1]),
            {3485},
        )

        def unreal_vertex(index: int) -> tuple[float, float, float]:
            x, y, z = geometry["verticesMeters"][index]
            return (x * 100.0, -y * 100.0, z * 100.0)

        runtime_triangles = [
            [unreal_vertex(row[index]) for index in (0, 2, 1)]
            for row in logical_triangles
        ]

        def normal(
            triangle: list[tuple[float, float, float]],
        ) -> tuple[float, float, float]:
            value = cross(
                subtract(triangle[1], triangle[0]),
                subtract(triangle[2], triangle[0]),
            )
            length = math.sqrt(dot(value, value))
            return tuple(component / length for component in value)

        normals = [normal(triangle) for triangle in runtime_triangles]
        plane_cross = cross(normals[0], normals[1])
        plane_cross_squared = dot(plane_cross, plane_cross)
        self.assertGreater(plane_cross_squared, 1.0e-16)
        self.assertLess(plane_cross_squared, 1.1e-16)
        maximum_mutual_distance = max(
            abs(dot(subtract(point, runtime_triangles[1][0]), normals[1]))
            for point in runtime_triangles[0]
        )
        maximum_mutual_distance = max(
            maximum_mutual_distance,
            max(
                abs(dot(subtract(point, runtime_triangles[0][0]), normals[0]))
                for point in runtime_triangles[1]
            ),
        )
        self.assertGreater(maximum_mutual_distance, 0.0)
        self.assertLessEqual(maximum_mutual_distance, 0.0001)

    def test_native_contract_exercises_success_reverse_and_adversarial_paths(self) -> None:
        for marker in (
            "Closed hash-bound indexed cube loads",
            "Forward segment has one entry/exit pair",
            "Reverse segment has one entry/exit pair",
            "Geometry-produced candidate satisfies the interaction seam",
            "Stable surface metadata is queryable",
            "Profile provenance binds the full catalog hash",
            "Coplanar boundary travel fails closed",
            "Opaque straight path admits no candidate",
            "Shared frequency boundary is ambiguous and fails closed",
            "Catalog hash mismatch rejects loading",
            "Failed reload clears a formerly valid query",
            "Open or unbound manifold fails closed",
            "Missing canonical finite primitive fails closed",
            "Endpoint outside the modeled coverage envelope fails closed",
            "Outside-coverage path candidate query fails instead of reporting Direct",
            "Scenario-pinned generalized modeled-coverage metadata loads",
            "Generalized modeled-coverage scope is retained verbatim",
            "One-kilometre modeled-coverage claim is retained for scenario validation",
            "Surroundings modeled-coverage claim is retained for scenario validation",
            "Survey-control claim is retained for scenario validation",
            "Field-validation claim is retained for scenario validation",
            "Empty modeled-coverage scope fails closed",
            "Expanded modeled-coverage AABB fails exact derived-bounds validation",
            "Missing coverage witnesses fail closed",
            "Malformed coverage witness endpoint fails closed",
            "Closed modeled-coverage face",
            "Epsilon outside modeled-coverage face",
            "Geometry hash is retained from the exact parsed string buffer",
            "Exact TightV1 consumed geometry hash retained",
            "Production file API rejects a configured hash before committing parsed state",
            "Exact TightV1 hash-bound artifact loads through the production file API",
            "Adjacent same-material trace preserves both solid pairs",
            "Adjacent same-material solids collapse to one material span",
            "Two metres of one material applies one paired-boundary loss",
            "Reverse adjacent same-material ray also collapses to one span",
            "Sub-tolerance same-material seam remains one material span",
            "Mixed-material shared interface fails closed",
            "Mixed-material shared interface emits no candidate",
            "Mixed-material rejection identifies the missing transition prior",
            "Transmission telemetry retains the stable solid ID",
            "Transmission telemetry retains the selected profile ID",
            "Transmission telemetry retains source provenance",
            "Transmission telemetry retains uncertainty class",
            "Transmission telemetry names the coefficient-selection rule",
        ):
            self.assertIn(marker, self.native_test)


if __name__ == "__main__":
    unittest.main()
