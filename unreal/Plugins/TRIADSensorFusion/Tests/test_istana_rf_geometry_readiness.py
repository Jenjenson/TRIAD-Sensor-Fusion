from __future__ import annotations

import ast
from collections import Counter, defaultdict
import json
import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
SCENARIO_H = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADSensorFusionTypes.h"
SCENARIO_CPP = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADSensorFusionScenarioManager.cpp"
PUBLIC_VIEW_SCENE_CPP = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaPublicViewSceneActor.cpp"
RF_MODEL_H = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADRFInteractionModel.h"
RF_MODEL_CPP = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADRFInteractionModel.cpp"
RF_MODEL_NATIVE_TEST = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/Tests/TRIADRFInteractionModelTests.cpp"
V1_GENERATOR = REPO / "unreal/SourceAssets/IstanaPublicView/generate_public_view_building.py"
V1_COLLISION_OBJ = REPO / "unreal/SourceAssets/IstanaPublicView/Generated/SM_IstanaPublicView_Building_Collision.obj"
V5_GENERATOR = REPO / "unreal/SourceAssets/IstanaPublicViewV5/generate_hero_v5.py"
V8_GENERATOR = REPO / "unreal/SourceAssets/IstanaPublicViewV8Portico/build_portico_depth_overlay_v8.py"
V4_RUNTIME_H = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADIstanaExploreV4LandscapeActor.h"
V4_RUNTIME_CPP = REPO / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreV4LandscapeActor.cpp"
READINESS = REPO / "unreal/SourceAssets/IstanaPublicViewRF/istana_public_exterior_rf_readiness.v1.json"
READINESS_DOC = REPO / "docs/ISTANA_RF_GEOMETRY_READINESS.md"
TIGHT_V1_MANIFEST = REPO / "unreal/SourceAssets/IstanaPublicViewRF/TightV1/Generated/IstanaPublicViewRFTightV1.manifest.json"
HIGH_FIDELITY_RF_EXAMPLE = REPO / "unreal/Plugins/TRIADSensorFusion/Resources/IstanaHighFidelityRF.example.json"


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def collision_hulls() -> tuple[tuple[tuple[float, float, float], tuple[float, float, float]], ...]:
    tree = ast.parse(text(V1_GENERATOR))
    for node in tree.body:
        if isinstance(node, ast.FunctionDef) and node.name == "build_collision":
            for statement in node.body:
                if (
                    isinstance(statement, ast.Assign)
                    and any(isinstance(target, ast.Name) and target.id == "hulls" for target in statement.targets)
                ):
                    value = ast.literal_eval(statement.value)
                    return tuple((tuple(center), tuple(size)) for center, size in value)
    raise AssertionError("Could not read the authoritative V1 collision hull tuple")


def ray_box_interval(
    start: tuple[float, float, float],
    end: tuple[float, float, float],
    center: tuple[float, float, float],
    size: tuple[float, float, float],
) -> tuple[float, float] | None:
    lower = tuple(center[index] - size[index] * 0.5 for index in range(3))
    upper = tuple(center[index] + size[index] * 0.5 for index in range(3))
    t_min = 0.0
    t_max = 1.0
    for axis in range(3):
        delta = end[axis] - start[axis]
        if abs(delta) <= 1e-12:
            if start[axis] < lower[axis] or start[axis] > upper[axis]:
                return None
            continue
        first = (lower[axis] - start[axis]) / delta
        second = (upper[axis] - start[axis]) / delta
        if first > second:
            first, second = second, first
        t_min = max(t_min, first)
        t_max = min(t_max, second)
        if t_min > t_max:
            return None
    return t_min, t_max


def hit_hulls(
    start: tuple[float, float, float],
    end: tuple[float, float, float],
) -> tuple[int, ...]:
    hits = []
    for index, (center, size) in enumerate(collision_hulls()):
        interval = ray_box_interval(start, end, center, size)
        if interval is not None:
            hits.append((interval[0], index))
    return tuple(index for _, index in sorted(hits))


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"Unclosed function body: {signature}")


class IstanaRFGeometryReadinessTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.scenario_h = text(SCENARIO_H)
        cls.scenario_cpp = text(SCENARIO_CPP)
        cls.public_view_scene_cpp = text(PUBLIC_VIEW_SCENE_CPP)
        cls.rf_model_h = text(RF_MODEL_H)
        cls.rf_model_cpp = text(RF_MODEL_CPP)
        cls.rf_model_native_test = text(RF_MODEL_NATIVE_TEST)
        cls.v5_generator = text(V5_GENERATOR)
        cls.v8_generator = text(V8_GENERATOR)
        cls.v4_runtime_h = text(V4_RUNTIME_H)
        cls.v4_runtime_cpp = text(V4_RUNTIME_CPP)
        cls.readiness = json.loads(text(READINESS))
        cls.readiness_doc = text(READINESS_DOC)
        cls.tight_v1_manifest = json.loads(text(TIGHT_V1_MANIFEST))
        cls.high_fidelity_rf_example = json.loads(text(HIGH_FIDELITY_RF_EXAMPLE))

    def test_visibility_trace_is_legacy_only_and_dedicated_rf_is_successor_wired(self) -> None:
        body = function_body(
            self.scenario_cpp,
            "bool ATRIADSensorFusionScenarioManager::ComputeLineOfSight(",
        )
        self.assertEqual(body.count("LineTraceSingleByChannel"), 1)
        self.assertNotIn("LineTraceMultiByChannel", body)
        self.assertIn("LineOfSightTraceChannel = 0", self.scenario_h)
        self.assertIn("NonLineOfSightAdditionalLossDb = 30.0", self.scenario_h)
        self.assertIn("bUseDedicatedRFPropagation = false", self.scenario_h)
        self.assertIn("bRequireDedicatedRFReady = false", self.scenario_h)
        self.assertIn("FTRIADRFIndexedGeometryQuery", self.scenario_cpp)
        self.assertIn("FTRIADDeterministicRFInteractionModel", self.scenario_cpp)
        self.assertIn("LoadFromJsonFiles", self.scenario_cpp)
        self.assertIn("const double LegacyObstructionLossDb = bLineOfSight", self.scenario_cpp)
        sample = function_body(
            self.scenario_cpp,
            "bool ATRIADSensorFusionScenarioManager::SampleNodeTargetLink(",
        )
        self.assertIn("EvaluateDedicatedRFPath", sample)
        self.assertIn("bLegacyLineOfSightGateSatisfied = Propagation.bDedicated ||", sample)
        self.assertIn("TotalPropagationLossDb - NodeDefinition.SystemLossDb - WeatherLossDb", sample)

    def test_v1_collision_is_seven_closed_outward_wound_box_solids(self) -> None:
        self.assertEqual(len(collision_hulls()), 7)
        vertices: list[tuple[float, float, float]] = []
        faces_by_group: dict[str, list[tuple[int, int, int]]] = defaultdict(list)
        group = ""
        for line in text(V1_COLLISION_OBJ).splitlines():
            fields = line.split()
            if not fields:
                continue
            if fields[0] == "v":
                vertices.append(tuple(float(value) for value in fields[1:4]))
            elif fields[0] == "g":
                group = fields[1]
            elif fields[0] == "f":
                faces_by_group[group].append(
                    tuple(int(token.split("/")[0]) - 1 for token in fields[1:])
                )
        self.assertEqual(len(vertices), 252)
        self.assertEqual(sum(map(len, faces_by_group.values())), 84)
        self.assertEqual(len(faces_by_group), 7)
        for name, faces in faces_by_group.items():
            self.assertRegex(name, r"^IPV_CollisionHull_[0-9]{2}$")
            self.assertEqual(len(faces), 12)
            edge_counts: Counter[
                tuple[tuple[float, float, float], tuple[float, float, float]]
            ] = Counter()
            directed_edges: Counter[
                tuple[tuple[float, float, float], tuple[float, float, float]]
            ] = Counter()
            signed_volume = 0.0
            for face in faces:
                a, b, c = (vertices[index] for index in face)
                signed_volume += (
                    a[0] * (b[1] * c[2] - b[2] * c[1])
                    + a[1] * (b[2] * c[0] - b[0] * c[2])
                    + a[2] * (b[0] * c[1] - b[1] * c[0])
                ) / 6.0
                for start, end in ((a, b), (b, c), (c, a)):
                    edge_counts[tuple(sorted((start, end)))] += 1
                    directed_edges[(start, end)] += 1
            self.assertTrue(all(count == 2 for count in edge_counts.values()))
            for start, end in edge_counts:
                self.assertEqual(directed_edges[(start, end)], 1)
                self.assertEqual(directed_edges[(end, start)], 1)
            self.assertGreater(signed_volume, 0.0)

    def test_v1_closed_hulls_overlap_and_are_not_one_boundary_manifold(self) -> None:
        hulls = collision_hulls()
        overlapping_pairs: list[tuple[int, int]] = []
        for first_index, (first_center, first_size) in enumerate(hulls):
            first_minimum = tuple(
                first_center[axis] - first_size[axis] * 0.5 for axis in range(3)
            )
            first_maximum = tuple(
                first_center[axis] + first_size[axis] * 0.5 for axis in range(3)
            )
            for second_index in range(first_index + 1, len(hulls)):
                second_center, second_size = hulls[second_index]
                second_minimum = tuple(
                    second_center[axis] - second_size[axis] * 0.5 for axis in range(3)
                )
                second_maximum = tuple(
                    second_center[axis] + second_size[axis] * 0.5 for axis in range(3)
                )
                overlap = tuple(
                    min(first_maximum[axis], second_maximum[axis])
                    - max(first_minimum[axis], second_minimum[axis])
                    for axis in range(3)
                )
                if min(overlap) > 0.0:
                    overlapping_pairs.append((first_index, second_index))
        self.assertEqual(
            overlapping_pairs,
            [(0, 1), (0, 2), (0, 3), (1, 4), (5, 6)],
        )

    def test_three_visual_portal_centerlines_are_solid_in_current_v1_collision(self) -> None:
        self.assertIn("PORTAL_CENTERS_X = (-6.0, 0.0, 6.0)", self.v8_generator)
        self.assertIn('return "LAYERED_OPEN_PORTAL_REVEAL"', self.v8_generator)
        for center_x in (-6.0, 0.0, 6.0):
            with self.subTest(center_x=center_x):
                front = (center_x, 60.0, 2.5)
                rear = (center_x, -60.0, 2.5)
                forward_hits = hit_hulls(front, rear)
                reverse_hits = hit_hulls(rear, front)
                self.assertTrue(forward_hits)
                self.assertEqual(set(forward_hits), set(reverse_hits))
                self.assertIn(1, forward_hits)  # 29 m-wide, 102 m-deep solid central box.

    def test_v1_gap_passes_bidirectionally_through_v5_opaque_plinth(self) -> None:
        self.assertIn("UPPER_TOWER_BODY_FRONT_Y = 12.22", self.v5_generator)
        self.assertIn('f"{GROUP_PREFIX}UpperTowerFoundationPlinth"', self.v5_generator)
        self.assertIn("14.475", self.v5_generator)
        self.assertIn("1.65", self.v5_generator)
        positive_y = (0.0, 100.0, 14.10)
        negative_y = (0.0, -100.0, 14.10)
        self.assertEqual(hit_hulls(positive_y, negative_y), ())
        self.assertEqual(hit_hulls(negative_y, positive_y), ())

    def test_v4_portico_and_vegetation_are_explicitly_not_rf_authority(self) -> None:
        self.assertIn("bool bCollisionOrSensorTruthAuthority = false", self.v4_runtime_h)
        self.assertIn("bool bPorticoCollisionAuthority = false", self.v4_runtime_h)
        configure_visual = function_body(
            self.v4_runtime_cpp,
            "void ATRIADIstanaExploreV4LandscapeActor::ConfigureVisualStaticMesh(",
        )
        self.assertIn("ECollisionEnabled::NoCollision", configure_visual)
        self.assertIn("SetCollisionResponseToAllChannels(ECR_Ignore)", configure_visual)

    def test_new_runtime_interface_is_bounded_and_fail_closed(self) -> None:
        for marker in (
            "class TRIADSENSORFUSION_API ITRIADRFInteractionModel",
            "class TRIADSENSORFUSION_API ITRIADRFGeometryQuery",
            "enum class ETRIADRFMaterialCalibrationState",
            "struct TRIADSENSORFUSION_API FTRIADRFModelLimits",
            "ETRIADRFPathKind::Direct",
            "ETRIADRFPathKind::Transmitted",
            "ETRIADRFPathKind::SingleReflection",
            "EntryPointCentimeters",
            "ExitPointCentimeters",
            "SurfacePointCentimeters",
            "SurfaceNormal",
            "ClearSegmentWitnesses",
            "ReflectedDirection",
            "SpecularDirectionTolerance",
            "MaximumCandidatePaths = 64",
            "MaximumVerticesPerPath = 3",
            "MaximumInteractionsPerPath = 32",
            "MaximumReceivedPowerCount = 64",
            "MinimumFrequencyGHz = 0.1",
            "MaximumFrequencyGHz = 100.0",
            "MinimumPathLengthMeters = 1.0",
            "MaximumPathLengthMeters = 5000.0",
            "HardMaximumFrequencyGHz = 100.0",
            "HardMaximumPathLengthMeters = 5000.0",
            "PreflightCandidateResourceEnvelope",
            "bExternalAcceptanceContextBound",
            "bFriisFarFieldApplicabilityValidated",
            "bReadyForSurveyTruth",
        ):
            self.assertIn(marker, self.rf_model_h + self.rf_model_cpp)
        self.assertNotIn("bGeometryAuthorityValidated", self.rf_model_h)
        self.assertNotIn("bApertureWitnessValidated", self.rf_model_h)
        self.assertNotIn("bMaterialCoefficientsCalibrated", self.rf_model_h)
        self.assertNotRegex(
            self.rf_model_h + self.rf_model_cpp,
            r"bReadyForSurveyTruth\s*=\s*true",
        )
        self.assertIn("Candidate.VerticesCentimeters.Num() != 2", self.rf_model_cpp)
        self.assertIn("Candidate.VerticesCentimeters.Num() != 3", self.rf_model_cpp)
        self.assertIn("Candidate.Interactions.Num() != 1", self.rf_model_cpp)
        self.assertIn("Candidate.ClearSegmentWitnesses.Num() != SegmentCount", self.rf_model_cpp)
        self.assertIn("Candidates.Num() > Limits.MaximumCandidatePaths", self.rf_model_cpp)
        self.assertIn("ReceivedPowersDbm.Num() > Limits.MaximumReceivedPowerCount", self.rf_model_cpp)
        self.assertIn("Interaction.SurfacePointCentimeters", self.rf_model_cpp)
        self.assertIn("FVector::Distance(\n                    ReflectedDirection", self.rf_model_cpp)
        self.assertIn("INCOHERENT_ONLY", self.rf_model_cpp)

    def test_candidate_resource_envelope_precedes_all_candidate_telemetry_copies(self) -> None:
        preflight = function_body(
            self.rf_model_cpp,
            "bool PreflightCandidateResourceEnvelope(",
        )
        for identifier in (
            "Candidate.PathId",
            "Candidate.GeometryQueryId",
            "Witness.WitnessId",
            "Interaction.Surface.SurfaceId",
            "Interaction.Surface.SolidId",
            "Interaction.Surface.MaterialId",
            "Interaction.Surface.ProfileId",
            "Interaction.Surface.SourceClass",
            "Interaction.Surface.UncertaintyClass",
            "Interaction.Surface.CoefficientSelectionSemantics",
            "Interaction.Surface.CalibrationProvenanceId",
            "Contributor.SolidId",
            "Contributor.EntrySurfaceId",
            "Contributor.ExitSurfaceId",
            "Contributor.SourceClass",
            "Contributor.UncertaintyClass",
        ):
            self.assertIn(identifier, preflight)
        for count in (
            "Candidate.VerticesCentimeters.Num()",
            "Candidate.Interactions.Num()",
            "Candidate.ClearSegmentWitnesses.Num()",
            "Interaction.Surface.Contributors.Num()",
        ):
            self.assertIn(count, preflight)

        evaluate_path = function_body(
            self.rf_model_cpp,
            "bool FTRIADDeterministicRFInteractionModel::EvaluatePath(",
        )
        preflight_call = evaluate_path.index("PreflightCandidateResourceEnvelope(")
        for telemetry_copy in (
            "OutEvaluation.PathId = Candidate.PathId",
            "OutEvaluation.GeometryQueryId = Candidate.GeometryQueryId",
            "OutEvaluation.ClearSegmentWitnessIds.Add(Witness.WitnessId)",
            "InteractionEvaluation.SurfaceId = Interaction.Surface.SurfaceId",
            "InteractionEvaluation.SolidId = Interaction.Surface.SolidId",
            "InteractionEvaluation.MaterialId = Interaction.Surface.MaterialId",
            "InteractionEvaluation.ProfileId = Interaction.Surface.ProfileId",
            "InteractionEvaluation.UncertaintyClass =",
            "InteractionEvaluation.CoefficientSelectionSemantics =",
            "InteractionEvaluation.PairedBoundaryTransmissionLossDb =",
            "InteractionEvaluation.Contributors = Interaction.Surface.Contributors",
            "OutEvaluation.CalibrationProvenanceIds.Add(",
        ):
            self.assertLess(preflight_call, evaluate_path.index(telemetry_copy))

        evaluate_paths = function_body(
            self.rf_model_cpp,
            "bool FTRIADDeterministicRFInteractionModel::EvaluatePaths(",
        )
        self.assertEqual(
            evaluate_paths.count(
                "for (const FTRIADRFPathCandidate& Candidate : Candidates)"
            ),
            2,
        )
        self.assertLess(
            evaluate_paths.index("PreflightCandidateResourceEnvelope("),
            evaluate_paths.index("OutEvaluations.Reserve(Candidates.Num())"),
        )
        self.assertLess(
            evaluate_paths.index("OutEvaluations.Reserve(Candidates.Num())"),
            evaluate_paths.index("EvaluatePath(Candidate, FrequencyGHz, Evaluation)"),
        )

        for adversarial_marker in (
            "FString::ChrN(257, TEXT('X'))",
            "OversizedPathId",
            "OversizedGeometryQueryId",
            "OversizedWitnessId",
            "OversizedSurfaceId",
            "OversizedMaterialId",
            "OversizedCalibrationId",
            "OversizedContributorId",
            "MissingContributors",
            "InvalidIdentifierBatch",
            "copies no candidate identifiers or arrays into telemetry",
        ):
            self.assertIn(adversarial_marker, self.rf_model_native_test)

    def test_frequency_and_path_length_have_real_compiled_ceilings(self) -> None:
        limits = function_body(
            self.rf_model_cpp,
            "bool FTRIADDeterministicRFInteractionModel::ValidateLimits(",
        )
        self.assertIn(
            "Limits.MaximumFrequencyGHz > HardMaximumFrequencyGHz",
            limits,
        )
        self.assertIn(
            "Limits.MaximumPathLengthMeters > HardMaximumPathLengthMeters",
            limits,
        )
        for adversarial_marker in (
            "MaximumFrequencyGHz = 100.000001",
            "MaximumPathLengthMeters = 5000.000001",
            "inclusive compiled 5 km and 100 GHz ceilings evaluate",
        ):
            self.assertIn(adversarial_marker, self.rf_model_native_test)

    def test_current_collision_policy_and_live_proof_remain_fail_closed(self) -> None:
        constructor = function_body(
            self.public_view_scene_cpp,
            "ATRIADIstanaPublicViewSceneActor::ATRIADIstanaPublicViewSceneActor()",
        )
        self.assertIn(
            "BuildingCollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics)",
            constructor,
        )
        self.assertNotIn("BuildingCollisionComponent->SetCollisionObjectType", constructor)
        self.assertNotIn("BuildingCollisionComponent->SetCollisionProfileName", constructor)
        geometry = self.readiness["currentGeometry"]
        self.assertNotIn("WorldStatic", geometry["componentTracePolicy"])
        self.assertEqual(
            geometry["collisionObjectType"],
            "NOT_LIVE_VERIFIED_SOURCE_DEFAULT_IS_WORLD_DYNAMIC_UNLESS_A_SERIALIZED_MAP_OVERRIDE_EXISTS",
        )
        proof = geometry["liveUnrealProof"]
        self.assertEqual(proof["status"], "MISSING_NO_LIVE_MAP_RECEIPT")
        for field in (
            "coldMapLoaded",
            "bodySetupInspectedInLoadedAsset",
            "componentTransformAndCollisionPolicyInspectedInLoadedMap",
            "worldSpaceBidirectionalWitnessTracesExecuted",
        ):
            self.assertFalse(proof[field])
        for field in ("mapSha256", "collisionAssetSha256", "automationReceipt"):
            self.assertIsNone(proof[field])
        self.assertNotIn("API/link audit passes", self.readiness_doc)
        self.assertNotIn("not yet wired into the\nscenario manager", self.readiness_doc)
        self.assertIn(
            "Guarded native evidence now establishes\n"
            "compilation, linking, and bounded contract-test execution",
            self.readiness_doc,
        )
        self.assertIn(
            "do not provide a deployed/live dedicated-RF scenario receipt, a cold packaged\n"
            "build receipt",
            self.readiness_doc,
        )
        self.assertIn(
            "now has its own passing, hash-bound native\n"
            "actual-file geometry-loader test",
            self.readiness_doc,
        )
        for marker in (
            "rf_actual_fix_20260905T184017Z\\receipt.json",
            "4,577,280 bytes",
            "C7A80FD955ED831A7148068444079E54D7D9847759D7DAAC24272022CC8A695A",
            "29,904,936 containment triangle checks",
            "30,000,000-check cap",
            "ordinary loader default remains\n20,000,000",
            "exactly 16,383 BVH\nnodes",
        ):
            self.assertIn(marker, self.readiness_doc)

    def test_readiness_contract_refuses_rf_tight_claim_and_names_acceptance(self) -> None:
        # Preserve the predecessor audit as historical fail-closed evidence,
        # then require the additive TightV1/runtime-binding successor explicitly.
        self.assertEqual(
            self.readiness["schemaVersion"],
            "triad.istana_public_exterior_rf_readiness.v1",
        )
        self.assertEqual(self.readiness["status"], "NOT_RF_READY")
        self.assertEqual(
            self.readiness["currentGeometry"]["positiveVolumeOverlappingHullPairCount"],
            5,
        )
        self.assertFalse(self.readiness["claimBoundary"]["surveyControlled"])
        self.assertFalse(
            self.readiness["claimBoundary"]["friisFarFieldApplicabilityValidated"]
        )
        self.assertFalse(
            self.readiness["claimBoundary"]["reflectionTransmissionDiffractionValidated"]
        )
        target = self.readiness["requiredDedicatedRfMeshContract"]
        for requirement in (
            "separateFromRenderAndPawnCollision",
            "dedicatedTraceChannel",
            "pairedEntryExitIntersectionsForTransmission",
            "openAperturesRepresentedByAbsentBlockingFaces",
            "doorsWindowsAndLouversHaveExplicitState",
            "bidirectionalRayWitnessesRequired",
            "independentSurveyCheckpointTracesRequired",
        ):
            self.assertTrue(target[requirement])
        modes = self.readiness["runtimeModeContract"]
        legacy = modes["legacyDefault"]
        dedicated = modes["dedicatedOptIn"]
        self.assertTrue(legacy["enabledByDefault"])
        self.assertEqual(
            legacy["configurationCondition"],
            "bUseDedicatedRFPropagation=false",
        )
        self.assertEqual(legacy["propagationMode"], "LEGACY_VISIBILITY_BINARY_NLOS")
        self.assertFalse(legacy["readyForSurveyTruth"])
        self.assertFalse(dedicated["enabledByDefault"])
        self.assertEqual(
            dedicated["configurationCondition"],
            "bUseDedicatedRFPropagation=true",
        )
        self.assertTrue(dedicated["sourceImplemented"])
        self.assertTrue(dedicated["scenarioManagerSourceWired"])
        self.assertTrue(dedicated["failClosedWhenRequired"])
        self.assertEqual(
            dedicated["admittedScenarioPathKinds"],
            ["Direct", "Transmitted"],
        )
        self.assertFalse(dedicated["reflectionCandidateEnumerationImplemented"])
        self.assertFalse(dedicated["readyForSurveyTruth"])

        deployment = self.readiness["deploymentEvidence"]
        self.assertEqual(
            deployment["status"],
            "UNPROVEN_NO_REPOSITORY_BUILD_OR_LIVE_EXECUTION_RECEIPT",
        )
        for field in (
            "nativeCompilationProvenByThisRecord",
            "nativeAutomationExecutionProvenByThisRecord",
            "coldLoadedDedicatedRuntimeProvenByThisRecord",
            "liveResourceHashBindingProvenByThisRecord",
            "deploymentReady",
        ):
            self.assertFalse(deployment[field], field)
        self.assertIsNone(deployment["liveTelemetryReceipt"])
        self.assertIsNone(deployment["fieldMeasurementReceipt"])

        runtime = self.readiness["newRuntimeInterface"]
        self.assertEqual(
            runtime["status"],
            "SOURCE_IMPLEMENTED_AND_SCENARIO_MANAGER_WIRED_OPT_IN_NATIVE_BUILD_AND_LIVE_DEPLOYMENT_UNPROVEN",
        )
        self.assertEqual(
            runtime["defaultActivation"],
            "DISABLED_LEGACY_MODE_REMAINS_DEFAULT",
        )
        self.assertTrue(runtime["optInConfigurationAvailable"])
        self.assertTrue(runtime["scenarioManagerSourceWired"])
        self.assertIsNone(runtime["nativeBuildReceipt"])
        self.assertIsNone(runtime["liveDedicatedRuntimeReceipt"])
        self.assertEqual(
            runtime["interfaceEvaluatorSupportedPathKinds"],
            ["Direct", "Transmitted", "SingleReflection"],
        )
        self.assertEqual(
            runtime["scenarioManagerAdmittedPathKinds"],
            ["Direct", "Transmitted"],
        )
        self.assertFalse(
            runtime["scenarioManagerReflectionCandidateEnumerationImplemented"]
        )
        self.assertEqual(
            runtime["surveyReadinessBehavior"],
            "HARD_FALSE_EXTERNAL_IMMUTABLE_ACCEPTANCE_CONTEXT_NOT_IMPLEMENTED",
        )
        self.assertFalse(runtime["externalAcceptanceContextImplemented"])
        self.assertEqual(
            runtime["materialCalibrationState"],
            "TRI_STATE_NOT_APPLICABLE_UNCALIBRATED_CALIBRATED",
        )
        self.assertFalse(runtime["geometryBinding"]["clearWitnessIsSurveyAuthority"])
        self.assertEqual(
            self.tight_v1_manifest["status"],
            "SIMULATION_READY_ASSUMPTION_BOUND",
        )
        self.assertFalse(self.tight_v1_manifest["acceptance"]["fieldValidated"])
        self.assertFalse(self.tight_v1_manifest["acceptance"]["readyForSurveyTruth"])
        self.assertTrue(self.high_fidelity_rf_example["bUseDedicatedRFPropagation"])
        self.assertTrue(self.high_fidelity_rf_example["bRequireDedicatedRFReady"])
        self.assertTrue(
            self.high_fidelity_rf_example["bDedicatedRFFrameIsWorldOriginIdentity"]
        )
        self.assertIn("InitializeDedicatedRFPropagation", self.scenario_cpp)
        self.assertIn("DEDICATED_RF_REQUIRED_LOAD_FAILED_MANAGER_STOPPED", self.scenario_cpp)
        self.assertIn("readyForSurveyTruth\"), false", self.scenario_cpp)
        self.assertEqual(
            runtime["defaultEnforcedLimits"],
            {
                "maximumCandidatePaths": 64,
                "maximumVerticesPerPath": 3,
                "maximumInteractionsPerPath": 32,
                "maximumReceivedPowerCount": 64,
                "minimumFrequencyGHz": 0.1,
                "maximumFrequencyGHz": 100.0,
                "minimumPathLengthMeters": 1.0,
                "maximumPathLengthMeters": 5000.0,
            },
        )
        self.assertEqual(
            runtime["compiledSafetyCeilings"],
            {
                "maximumCandidatePaths": 4096,
                "maximumVerticesPerPath": 3,
                "maximumInteractionsPerPath": 32,
                "maximumReceivedPowerCount": 4096,
                "maximumIdentifierCharacters": 256,
                "maximumFrequencyGHz": 100.0,
                "maximumPathLengthMeters": 5000.0,
                "maximumGeometryPositionToleranceCentimeters": 1.0,
                "maximumSpecularDirectionTolerance": 0.01,
            },
        )
        telemetry = set(self.readiness["minimumRuntimeTelemetry"])
        self.assertTrue(
            {
                "geometryQueryId",
                "frequencyGHz",
                "clearSegmentWitnessIds",
                "materialCalibrationState",
                "calibrationProvenanceIds",
                "externalAcceptanceContextBound",
                "friisFarFieldApplicabilityValidated",
                "readyForSurveyTruth",
                "readinessSemantics",
            }.issubset(telemetry)
        )
        self.assertTrue(
            any(
                "cold_load_the_exact_map" in step
                for step in self.readiness["acceptanceSequence"]
            )
        )


if __name__ == "__main__":
    unittest.main()
