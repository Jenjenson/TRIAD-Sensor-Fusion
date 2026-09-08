import hashlib
import json
import re
import unittest
from collections import Counter
from pathlib import Path


UNREAL_ROOT = Path(__file__).absolute().parents[3]
REPO_ROOT = UNREAL_ROOT.parent
PLUGIN_ROOT = UNREAL_ROOT / "Plugins/TRIADSensorFusion"
CANDIDATE_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/"
    "GeometryVariationCandidateV4"
)
CANDIDATE_PATH = CANDIDATE_ROOT / "tree_geometry_variation_candidate.v4.json"
SELECTOR_PATH = CANDIDATE_ROOT / "tree_geometry_instance_selector.v4.json"
INTEGRATION_PATH = (
    CANDIDATE_ROOT
    / "tree_geometry_variation_post_r33_integration.source_contract.v1.json"
)
API_AUDIT_PATH = (
    CANDIDATE_ROOT / "tree_geometry_variation_ue55_api_audit.source.json"
)
README_PATH = CANDIDATE_ROOT / "README.md"
RUNTIME_PUBLIC = PLUGIN_ROOT / "Source/TRIADSensorFusion/Public"
RUNTIME_PRIVATE = PLUGIN_ROOT / "Source/TRIADSensorFusion/Private"
EDITOR_PUBLIC = PLUGIN_ROOT / "Source/TRIADSensorFusionEditor/Public"
EDITOR_PRIVATE = PLUGIN_ROOT / "Source/TRIADSensorFusionEditor/Private"
RECIPES_PATH = (
    RUNTIME_PUBLIC / "TRIADIstanaExploreV5DTreeGeometryVariationRecipes.inl"
)
SELECTORS_PATH = (
    RUNTIME_PUBLIC / "TRIADIstanaExploreV5DTreeGeometryVariationSelectors.inl"
)
ACTOR_HEADER = (
    RUNTIME_PUBLIC / "TRIADIstanaExploreV5DTreeGeometryVariationActor.h"
)
ACTOR_SOURCE = (
    RUNTIME_PRIVATE / "TRIADIstanaExploreV5DTreeGeometryVariationActor.cpp"
)
ACTOR_TEST = (
    RUNTIME_PRIVATE
    / "Tests/TRIADIstanaExploreV5DTreeGeometryVariationActorTests.cpp"
)
EDITOR_HEADER = (
    EDITOR_PUBLIC
    / "TRIADIstanaExploreV5DTreeGeometryVariationEditorLibrary.h"
)
EDITOR_SOURCE = (
    EDITOR_PRIVATE
    / "TRIADIstanaExploreV5DTreeGeometryVariationEditorLibrary.cpp"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def as_float(token: str) -> float:
    return float(token.strip().removesuffix("f"))


class ExploreV5DTreeGeometryPostR33SourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.candidate = json.loads(CANDIDATE_PATH.read_text(encoding="utf-8"))
        cls.selector = json.loads(SELECTOR_PATH.read_text(encoding="utf-8"))
        cls.integration = json.loads(INTEGRATION_PATH.read_text(encoding="utf-8"))
        cls.audit = json.loads(API_AUDIT_PATH.read_text(encoding="utf-8"))
        cls.readme = README_PATH.read_text(encoding="utf-8")
        cls.actor_header = ACTOR_HEADER.read_text(encoding="utf-8")
        cls.actor_source = ACTOR_SOURCE.read_text(encoding="utf-8")
        cls.actor_test = ACTOR_TEST.read_text(encoding="utf-8")
        cls.editor_header = EDITOR_HEADER.read_text(encoding="utf-8")
        cls.editor_source = EDITOR_SOURCE.read_text(encoding="utf-8")

    def test_delivery_is_unnumbered_source_only_and_every_native_claim_is_false(self):
        self.assertEqual(
            "triad.istana_public_view_explore_v5d_tree_realism."
            "geometry_variation.post_r33_source_integration.v1",
            self.integration["schema"],
        )
        self.assertEqual(
            "SOURCE_SCAFFOLD_NOT_EXECUTED", self.integration["status"]
        )
        sequencing = self.integration["sequencing"]
        self.assertTrue(sequencing["acceptedR33ReceiptRequired"])
        self.assertTrue(sequencing["acceptedR33ReceiptMustBeHumanAccepted"])
        self.assertTrue(sequencing["futureExplicitTransactionRequired"])
        self.assertTrue(sequencing["twoReceiptHashesMustDiffer"])
        self.assertFalse(sequencing["callerSuppliedHashesAuthorizeExecution"])
        self.assertFalse(sequencing["blueprintMaterializationEndpointExposed"])
        self.assertFalse(sequencing["compiledTrustAnchorsPopulated"])
        self.assertFalse(sequencing["materializationReachable"])
        self.assertTrue(
            sequencing["futureReviewedSourceChangeAndRecompileRequired"]
        )
        self.assertFalse(sequencing["numberedSuccessorAssigned"])
        self.assertFalse(sequencing["numberedTransactionOrCaptureWrappersModified"])
        self.assertFalse(sequencing["numberedTransactionOrCaptureWrappersInvoked"])
        state = dict(self.integration["deliveryState"])
        self.assertTrue(state.pop("sourceImplemented"))
        self.assertTrue(all(value is False for value in state.values()))
        self.assertTrue(
            all(value is False for value in self.integration["nativeClaims"].values())
        )

    def test_pinned_candidate_inputs_and_implementation_artifacts_are_exact(self):
        inputs = self.integration["pinnedInputs"]
        for key, path in (
            ("candidateContract", CANDIDATE_PATH),
            ("selectorManifest", SELECTOR_PATH),
        ):
            receipt = inputs[key]
            self.assertEqual(path.stat().st_size, receipt["bytes"])
            self.assertEqual(sha256(path), receipt["sha256"])
            self.assertEqual(
                path.relative_to(REPO_ROOT).as_posix(), receipt["file"]
            )
        expected_artifacts = {
            path.relative_to(REPO_ROOT).as_posix()
            for path in (
                RECIPES_PATH,
                SELECTORS_PATH,
                ACTOR_HEADER,
                ACTOR_SOURCE,
                ACTOR_TEST,
                EDITOR_HEADER,
                EDITOR_SOURCE,
                API_AUDIT_PATH,
                README_PATH,
                Path(__file__).absolute(),
            )
        }
        artifacts = self.integration["implementation"]["artifacts"]
        self.assertEqual(expected_artifacts, {row["file"] for row in artifacts})
        for row in artifacts:
            path = REPO_ROOT / row["file"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(path.stat().st_size, row["bytes"])
            self.assertEqual(sha256(path), row["sha256"])

    def test_all_fifteen_compiled_recipes_exactly_mirror_candidate_v4(self):
        rows = []
        pattern = re.compile(
            r"^TRIAD_TREE_GEOMETRY_VARIATION_RECIPE\((.*)\)$", re.MULTILINE
        )
        for match in pattern.finditer(RECIPES_PATH.read_text(encoding="utf-8")):
            values = [value.strip() for value in match.group(1).split(",")]
            self.assertEqual(17, len(values))
            rows.append(
                {
                    "form": {
                        "Umbrella": "umbrella",
                        "Dome": "dome",
                        "HighForkRounded": "highForkRounded",
                        "Columnar": "columnar",
                        "Palm": "palm",
                    }[values[0]],
                    "selector": values[1].strip('"'),
                    "asset": values[2].strip('"'),
                    "parameters": {
                        "rootHoldNormalizedHeight": as_float(values[3]),
                        "crownStartNormalizedHeight": as_float(values[4]),
                        "crownFullNormalizedHeight": as_float(values[5]),
                        "trunkScaleXY": [as_float(values[6]), as_float(values[7])],
                        "crownScaleXY": [as_float(values[8]), as_float(values[9])],
                        "heightScale": as_float(values[10]),
                        "bendBySourceHeightXY": [
                            as_float(values[11]),
                            as_float(values[12]),
                        ],
                        "twistDegrees": as_float(values[13]),
                        "radialRippleFraction": as_float(values[14]),
                        "radialLobes": int(values[15]),
                        "radialPhaseDegrees": as_float(values[16]),
                    },
                }
            )
        expected = []
        for candidate in self.candidate["candidateVariants"]:
            expected.append(
                {
                    "form": candidate["form"],
                    "selector": candidate["selector"],
                    "asset": candidate["candidatePackagePath"].rsplit("/", 1)[-1],
                    "parameters": candidate["parameters"],
                }
            )
        self.assertEqual(15, len(rows))
        self.assertEqual(expected, rows)

    def test_compiled_selector_is_exact_736_row_manifest(self):
        text = SELECTORS_PATH.read_text(encoding="utf-8")
        compiled = "".join(re.findall(r'TEXT\("([ABC]+)"\)', text))
        expected = "".join(
            row["variantSelector"] for row in self.selector["rows"]
        )
        self.assertEqual(736, len(compiled))
        self.assertEqual(expected, compiled)
        self.assertEqual(Counter({"A": 223, "B": 266, "C": 247}), Counter(compiled))
        self.assertIn("static_assert(UE_ARRAY_COUNT(SelectorText) - 1 == SourceAnchorCount)", self.actor_source)
        self.assertIn("constexpr int32 SourceAnchorCount =", self.actor_source)
        self.assertIn("V4MainCount + V4HeritageCount + R29LandmarkCount", self.actor_source)

    def test_runtime_route_is_render_only_exact_and_fail_closed(self):
        source = self.actor_header + self.actor_source + self.actor_test
        for token in (
            "VariantMeshCount = FormCount * VariantsPerForm",
            "requires exactly 736 ordered source anchors",
            "const FTransform ExactCopy = Anchor.SourceWorldTransform",
            "ExactTransformValue(ExactCopy, Anchor.SourceWorldTransform)",
            "FMemory::Memcmp(&A, &B, sizeof(T)) == 0",
            "ExactQuatBits(A.GetRotation(), B.GetRotation())",
            "ExactSourceTransformBitsMatch(\n        const FTransform& Actual",
            "Component->AddInstances(Transforms, false, true, false)",
            "SetCollisionEnabled(ECollisionEnabled::NoCollision)",
            "SetCanEverAffectNavigation(false)",
            "SetForcedLodModel(0)",
            "SetLODDistanceScale(TreeLodDistanceScale)",
            "GetLODDistanceScale() == TreeLodDistanceScale",
            "Component->SetVisibility(false, true)",
            "Component->SetHiddenInGame(true, true)",
            "InAcceptedR33ReceiptSha256",
            "InFutureTransactionReceiptSha256",
            "bInExactSourcePresentationSuppressionProven",
            "Selector A count",
            "Selector B count",
            "Selector C count",
            "Manifest-key drift fails closed",
            "NativeInstanceTranslationToleranceCm = 0.02",
            "NativeInstanceRotationTolerance = 0.00001",
            "NativeInstanceScaleTolerance = 0.00001",
            "NativeInstanceTransformValue(Actual, Expected[Index])",
            "Probe->AddInstance(ExpectedRoundTrip, true)",
            "Probe->GetInstanceTransform(0, ActualRoundTrip, true)",
            "Meaningful translation drift is rejected",
            "Meaningful scale drift is rejected",
            "Bit-exact source comparison rejects a sign-flipped quaternion",
            "Bit-exact source comparison rejects signed-zero drift",
            "ExactSourceTransformBitsMatch(SignFlippedProbe, ExactProbe)",
            "ExactSourceTransformBitsMatch(SignedZeroProbe, ExactProbe)",
        ):
            self.assertIn(token, source)
        for authority in (
            "bNativeVisualAcceptanceProvided = false",
            "bSourceTransformsOrGeographyModified = false",
            "bCollisionNavigationLosRfSensorOrTerrainAuthority = false",
        ):
            self.assertIn(authority, source)
        for forbidden_private_member in (
            "bEnableDensityScaling",
            "bAutoRebuildTreeOnInstanceChanges",
            "bOverrideMinLOD",
            "->ForcedLodModel",
            "->InstanceLODDistanceScale",
        ):
            self.assertNotIn(forbidden_private_member, self.actor_source)

        layout_validation = self.actor_source.split(
            "bool ValidateLayoutInternal", 1
        )[1].split("bool ComponentMatches", 1)[0]
        self.assertIn("ExactTransformValue", layout_validation)
        component_validation = self.actor_source.split(
            "bool ComponentMatches", 1
        )[1].split("} // namespace", 1)[0]
        self.assertNotIn("ExactTransformValue", component_validation)

    def test_runtime_configuration_and_activation_cannot_self_authenticate(self):
        public_text, private_text = self.actor_header.split("private:", 1)
        self.assertNotIn("ConfigureGeometryVariation", public_text)
        self.assertNotIn(
            "ActivatePresentationAfterExactSourceSuppression", public_text
        )
        self.assertIn("ConfigureGeometryVariation", private_text)
        self.assertIn(
            "ActivatePresentationAfterExactSourceSuppression", private_text
        )
        self.assertNotIn("UFUNCTION(BlueprintCallable", self.actor_header)
        self.assertNotIn("friend ", self.actor_header)
        self.assertEqual(
            2,
            (self.actor_header + self.actor_source).count(
                "ConfigureGeometryVariation"
            ),
        )
        self.assertEqual(
            2,
            (self.actor_header + self.actor_source).count(
                "ActivatePresentationAfterExactSourceSuppression"
            ),
        )
        for token in (
            "UNSET_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "UNSET_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "RuntimeCompiledTrustAnchorsConfigured()",
            "!RuntimeCompiledTrustAnchorsConfigured()",
            "TrustedRuntimeAcceptedR33ReceiptSha256",
            "TrustedRuntimeFutureAuthorizationSha256",
            "ESearchCase::IgnoreCase",
            "Runtime compiled trust anchors remain deliberately unset",
        ):
            self.assertIn(token, self.actor_header + self.actor_source + self.actor_test)

        trust = self.integration["implementation"][
            "runtimePresentationTrustBoundary"
        ]
        self.assertEqual("PRIVATE_CPP_ONLY", trust["configurationVisibility"])
        self.assertEqual("PRIVATE_CPP_ONLY", trust["activationVisibility"])
        self.assertFalse(trust["configurationHasCallSite"])
        self.assertFalse(trust["activationHasCallSite"])
        self.assertFalse(trust["compiledTrustAnchorsAreValidSha256"])
        self.assertFalse(trust["callerSuppliedHashesAloneSufficient"])
        self.assertFalse(trust["runtimeConfigurationReachable"])
        self.assertFalse(trust["runtimeVisibilityReachable"])
        self.assertTrue(trust["reviewedSourceEditAndRecompileRequired"])
        self.assertEqual(
            trust,
            self.audit["runtimePresentationTrustBoundary"],
        )
        expected_tolerances = {
            "rotationQuaternionComponent": 0.00001,
            "scale": 0.00001,
            "translationCm": 0.02,
        }
        self.assertEqual(
            expected_tolerances,
            self.integration["implementation"][
                "nativeInstanceReadbackTolerance"
            ],
        )
        self.assertEqual(
            expected_tolerances,
            self.audit["nativeInstanceReadbackTolerance"],
        )

    def test_editor_materializer_warps_every_lod_and_preserves_mesh_semantics(self):
        source = self.editor_header + self.editor_source
        for token in (
            "CloneMeshDescription",
            "Source->GetNumSourceModels()",
            "Candidate->CreateMeshDescription",
            "Candidate->CommitMeshDescription",
            "FStaticMeshOperations::ComputeTriangleTangentsAndNormals",
            "FStaticMeshOperations::ComputeTangentsAndNormals",
            "GetVertexInstanceUVs",
            "GetVertexInstanceColors",
            "GetPolygonGroupMaterialSlotNames",
            "Source.Edges().Num()",
            "Source.Polygons().Num()",
            "Candidate->SetStaticMaterials(Source->GetStaticMaterials())",
            "Candidate->GetSectionInfoMap().CopyFrom",
            "Candidate->GetOriginalSectionInfoMap().CopyFrom",
            "PARTIAL_NAMESPACE_DENIED",
            "SaveLoadedAssets(AssetsToSave, false)",
        ):
            self.assertIn(token, source)
        self.assertEqual(1, self.editor_source.count("LoadFileToArray"))
        self.assertNotIn("LoadFileToString", self.editor_source)
        self.assertIn("FFileHelper::BufferToString", self.editor_source)

    def test_receipt_inspection_is_narrow_and_no_numbered_successor_is_named(self):
        source = self.editor_header + self.editor_source
        for token in (
            "triad.istana_explore_v5d.r33_player0_capture.v1",
            "triad.istana_explore_v5d.r33_cesium_world_terrain_reference."
            "native_transaction.v1",
            "ExplicitHumanReviewAcceptance",
            "ConfirmedEightImagesReviewed",
            "MapModifiedByCapture",
            "SimulationCollisionNavigationSensorRfModified",
            "future_transaction_authority.v1",
            "AUTHORIZED_NOT_EXECUTED",
            "ExplicitExecutionAuthorized",
            "AssetsOnlyEndpoint",
            "MapMutationAuthorized",
            "SourceTransformMutationAuthorized",
        ):
            self.assertIn(token, source)
        watched = [
            RECIPES_PATH,
            SELECTORS_PATH,
            ACTOR_HEADER,
            ACTOR_SOURCE,
            ACTOR_TEST,
            EDITOR_HEADER,
            EDITOR_SOURCE,
            API_AUDIT_PATH,
            README_PATH,
            INTEGRATION_PATH,
            Path(__file__).absolute(),
        ]
        forbidden_successor = "R" + str(34)
        for path in watched:
            self.assertNotIn(forbidden_successor, path.read_text(encoding="utf-8"), path)
        self.assertNotRegex(
            source,
            r"TRIADIstanaExploreV5DR3[0-3](?:Player0Capture|FacadeLookdev|BroadShell|MediumDistance|Cesium)",
        )

    def test_remaining_native_proof_is_complete_and_explicit(self):
        remaining = set(self.integration["remainingNativeProof"])
        required = {
            "UE55_COMPILE_AND_LINK",
            "REVIEWED_COMPILED_TRUST_ANCHOR_ACTIVATION",
            "EVERY_SOURCE_LOD_MESHDESCRIPTION_AVAILABLE",
            "MATERIALIZE_SAVE_COLD_RELOAD_EXACT_15_ASSETS",
            "EXTRACT_AND_CLASSIFY_ORDERED_736_SOURCE_ANCHORS",
            "CANONICAL_BEFORE_AFTER_TRANSFORM_HASH_EQUALITY",
            "ATOMIC_SOURCE_PRESENTATION_SUPPRESSION_AND_ROLLBACK",
            "MAP_SAVE_RELOAD_AND_PIE_RUNTIME_VALIDATION",
            "FIXED_VIEW_LOD_SILHOUETTE_CAPTURE",
            "HUMAN_VISUAL_ACCEPTANCE",
            "MEMORY_AND_FRAME_TIME_ACCEPTANCE",
        }
        self.assertEqual(required, remaining)

    def test_caller_cannot_self_authenticate_materialization(self):
        trust = self.integration["implementation"]["executionTrustBoundary"]
        self.assertTrue(trust["receiptInspectionBlueprintCallable"])
        self.assertFalse(trust["receiptInspectionAuthorizesExecution"])
        self.assertEqual("PRIVATE_CPP_ONLY", trust["materializerVisibility"])
        self.assertFalse(trust["compiledTrustAnchorsAreValidSha256"])
        self.assertFalse(trust["callerSuppliedHashesAloneSufficient"])
        self.assertFalse(trust["nativeWriteReachable"])
        self.assertTrue(trust["activationRequiresReviewedSourceEditAndRecompile"])
        self.assertEqual(
            "UNSET_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            trust["acceptedR33CompiledTrustAnchor"],
        )
        self.assertEqual(
            "UNSET_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            trust["futureAuthorizationCompiledTrustAnchor"],
        )

        self.assertIn("private:", self.editor_header)
        public_text, private_text = self.editor_header.split("private:", 1)
        self.assertIn("InspectTreeGeometryVariationReceipts", public_text)
        self.assertNotIn("Materialize", public_text)
        self.assertIn(
            "MaterializeTrustedPostR33CandidateMeshesInternal", private_text
        )
        self.assertEqual(1, self.editor_header.count("UFUNCTION(BlueprintCallable"))
        self.assertNotRegex(
            self.editor_header,
            r"UFUNCTION\(BlueprintCallable,[\s\S]{0,180}"
            r"MaterializeTrustedPostR33CandidateMeshesInternal",
        )
        self.assertEqual(
            2,
            (self.editor_header + self.editor_source).count(
                "MaterializeTrustedPostR33CandidateMeshesInternal"
            ),
        )
        for token in (
            "UNSET_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "UNSET_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "!IsSha256(TrustedAcceptedR33ReceiptSha256)",
            "!IsSha256(TrustedFutureAuthorizationSha256)",
            "bRequireCompiledTrustAnchors",
            "Caller-supplied tree-geometry receipt hashes do not match",
            "callerSuppliedHashesNeverAuthorizeExecution=true",
            "privateMaterializerInvoked=false",
        ):
            self.assertIn(token, self.editor_source)

        inspection = self.editor_source.index(
            "InspectTreeGeometryVariationReceipts("
        )
        materializer = self.editor_source.index(
            "MaterializeTrustedPostR33CandidateMeshesInternal("
        )
        load_sources = self.editor_source.index("LoadBaseMeshes(", materializer)
        self.assertIn(
            "ExpectedFutureTransactionAuthorizationSha256,\n            false,",
            self.editor_source[inspection:materializer],
        )
        self.assertIn(
            "ExpectedFutureTransactionAuthorizationSha256,\n            true,",
            self.editor_source[materializer:load_sources],
        )

    def test_static_installed_ue55_api_audit_is_exact(self):
        self.assertEqual(
            "STATIC_INSTALLED_HEADER_AUDIT_ONLY_NOT_COMPILED_OR_EXECUTED",
            self.audit["status"],
        )
        self.assertFalse(self.audit["launchOrBuildPerformed"])
        self.assertTrue(
            all(value is False for value in self.audit["nativeClaims"].values())
        )
        self.assertEqual(
            self.integration["implementation"]["executionTrustBoundary"],
            {
                **self.audit["sourceTrustBoundary"],
                "acceptedR33CompiledTrustAnchor": (
                    "UNSET_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"
                ),
                "futureAuthorizationCompiledTrustAnchor": (
                    "UNSET_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"
                ),
            },
        )
        engine_source = Path("C:/Program Files/Epic Games/UE_5.5/Engine/Source")
        if not engine_source.is_dir():
            self.skipTest("Pinned UE 5.5 install is not present on this host")
        for row in self.audit["headers"]:
            path = engine_source / row["pathBelowEngineSource"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(row["bytes"], path.stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)


if __name__ == "__main__":
    unittest.main()
