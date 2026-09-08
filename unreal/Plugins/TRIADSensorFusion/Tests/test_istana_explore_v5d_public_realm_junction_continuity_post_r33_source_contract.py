import hashlib
import json
import math
import re
import unittest
from pathlib import Path


UNREAL_ROOT = Path(__file__).absolute().parents[3]
REPO_ROOT = UNREAL_ROOT.parent
PLUGIN_ROOT = UNREAL_ROOT / "Plugins/TRIADSensorFusion"
CANDIDATE_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/"
    "JunctionContinuityCandidate"
)
INTEGRATION_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/"
    "JunctionContinuityIntegration"
)
CONTRACT_PATH = (
    INTEGRATION_ROOT
    / "public_realm_junction_continuity_post_r33.source_contract.v1.json"
)
API_AUDIT_PATH = (
    INTEGRATION_ROOT
    / "public_realm_junction_continuity_ue55_api_audit.source.json"
)
README_PATH = INTEGRATION_ROOT / "README.md"
CANDIDATE_CONTRACT_PATH = (
    CANDIDATE_ROOT
    / "public_realm_junction_continuity_candidate.contract.v1.json"
)
OVERRIDE_PATH = (
    CANDIDATE_ROOT
    / "Generated/public_realm_junction_normal_overrides.v1.json"
)
CANDIDATE_AUDIT_PATH = (
    CANDIDATE_ROOT
    / "Generated/public_realm_junction_continuity_source_audit.v1.json"
)
CORE_OBJ_PATH = (
    CANDIDATE_ROOT.parent / "Generated/SM_IPV5D_PublicRealm_Core_Render.obj"
)
FALLBACK_OBJ_PATH = (
    CANDIDATE_ROOT.parent
    / "Generated/SM_IPV5D_PublicRealm_Fallback_Render.obj"
)
RUNTIME_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary.h"
)
RUNTIME_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DPublicRealmJunctionContinuityLibrary.cpp"
)
RUNTIME_TEST = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/Tests/"
    "TRIADIstanaExploreV5DPublicRealmJunctionContinuityLibraryTests.cpp"
)
EDITOR_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DPublicRealmJunctionContinuityEditorLibrary.h"
)
EDITOR_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DPublicRealmJunctionContinuityEditorLibrary.cpp"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def reject_duplicate_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_strict_json(path: Path):
    return json.loads(
        path.read_text(encoding="utf-8"),
        object_pairs_hook=reject_duplicate_keys,
        parse_constant=lambda value: (_ for _ in ()).throw(
            ValueError(f"non-finite JSON number: {value}")
        ),
    )


def normalized_cpp(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


def parse_obj(path: Path):
    positions = []
    normals = []
    faces = []
    group = None
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("v "):
            positions.append(tuple(float(value) for value in line.split()[1:]))
        elif line.startswith("vn "):
            normals.append(tuple(float(value) for value in line.split()[1:]))
        elif line.startswith("g "):
            group = line[2:].strip()
        elif line.startswith("f "):
            faces.append(
                (
                    group,
                    tuple(
                        tuple(int(value) for value in token.split("/"))
                        for token in line.split()[1:]
                    ),
                )
            )
    return positions, normals, faces


HISTORICAL_WRAPPER_RECEIPT_PINS = {
    "scripts/Invoke-IstanaExploreV5DContextFacadeR30NativeTransactionV1.ps1": (
        92139,
        "2445F9BBD96E39AC8C92C8AA3C279D9CDB88226EFABFC9AAF2F2351A767EC148",
    ),
    "scripts/Capture-IstanaExploreV5DR30Player0Evidence.ps1": (
        152520,
        "1C2427A5A15463D250A19E17466831B1862B13D40F864B706D60E74E689E5524",
    ),
}


class PublicRealmJunctionContinuityPostR33SourceContractTests(
    unittest.TestCase
):
    @classmethod
    def setUpClass(cls):
        cls.contract = load_strict_json(CONTRACT_PATH)
        cls.api_audit = load_strict_json(API_AUDIT_PATH)
        cls.candidate = load_strict_json(CANDIDATE_CONTRACT_PATH)
        cls.overrides = load_strict_json(OVERRIDE_PATH)
        cls.candidate_audit = load_strict_json(CANDIDATE_AUDIT_PATH)
        cls.runtime_header = RUNTIME_HEADER.read_text(encoding="utf-8")
        cls.runtime_source = RUNTIME_SOURCE.read_text(encoding="utf-8")
        cls.runtime_test = RUNTIME_TEST.read_text(encoding="utf-8")
        cls.editor_header = EDITOR_HEADER.read_text(encoding="utf-8")
        cls.editor_source = EDITOR_SOURCE.read_text(encoding="utf-8")

    def test_source_only_delivery_has_independent_dormant_gates(self):
        self.assertEqual(
            "triad.istana_public_view_explore_v5d.public_realm."
            "junction_continuity.post_r33_source_integration.v1",
            self.contract["schema"],
        )
        self.assertEqual("SOURCE_SCAFFOLD_NOT_EXECUTED", self.contract["status"])
        sequencing = self.contract["sequencing"]
        self.assertTrue(sequencing["acceptedR33ReceiptRequired"])
        self.assertTrue(sequencing["acceptedR33ReceiptMustBeHumanAccepted"])
        self.assertTrue(sequencing["futureExplicitTransactionRequired"])
        self.assertTrue(sequencing["twoReceiptHashesMustDiffer"])
        self.assertFalse(sequencing["callerSuppliedHashesAuthorizeExecution"])
        self.assertFalse(sequencing["blueprintMaterializationEndpointExposed"])
        self.assertFalse(sequencing["compiledTrustAnchorsPopulated"])
        self.assertFalse(sequencing["materializationReachable"])
        self.assertTrue(sequencing["futureReviewedSourceChangeAndRecompileRequired"])
        self.assertFalse(sequencing["numberedSuccessorAssigned"])
        self.assertFalse(sequencing["numberedWrappersModified"])
        self.assertFalse(sequencing["nativeProjectWritten"])
        self.assertFalse(sequencing["unrealLaunchedOrBuilt"])
        self.assertTrue(
            all(value is False for value in self.contract["nativeClaims"].values())
        )

    def test_candidate_closure_and_source_inputs_are_hash_exact(self):
        for row in self.contract["sourcePins"].values():
            path = REPO_ROOT / row["file"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(row["bytes"], path.stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)
            self.assertIn(row["sha256"], self.editor_source)
        self.assertEqual(
            "POST_R33_UNNUMBERED_UNADMITTED_SOURCE_ONLY_CANDIDATE",
            self.candidate["status"],
        )
        self.assertTrue(self.candidate["sourceOnly"])
        self.assertFalse(self.candidate["nativeProjectApplied"])
        self.assertFalse(self.candidate["unrealLaunchedOrBuilt"])
        self.assertFalse(self.candidate["newNumberedStageAuthorized"])
        self.assertFalse(self.candidate_audit["nativeProjectApplied"])
        self.assertFalse(self.candidate_audit["visualAcceptance"])

    def test_exact_355_row_table_references_the_frozen_obj_corners(self):
        summary = self.overrides["summary"]
        self.assertEqual(60, summary["clusterCount"])
        self.assertEqual(355, summary["normalRecordCount"])
        self.assertEqual(174, summary["coreNormalRecordCount"])
        self.assertEqual(181, summary["fallbackNormalRecordCount"])
        self.assertEqual(60, summary["crossCoreFallbackClusterCount"])
        self.assertEqual(0, summary["coreOnlyClusterCount"])
        self.assertEqual(0, summary["fallbackOnlyClusterCount"])
        self.assertEqual(0.044152281, summary["maximumCandidateAdjustmentDegrees"])
        self.assertLessEqual(
            summary["maximumCandidateAdjustmentDegrees"],
            summary["maximumCandidateAdjustmentAllowedDegrees"],
        )

        parsed = {
            "CORE": parse_obj(CORE_OBJ_PATH),
            "FALLBACK": parse_obj(FALLBACK_OBJ_PATH),
        }
        keys = set()
        counts = {"CORE": 0, "FALLBACK": 0}
        for cluster in self.overrides["clusters"]:
            self.assertEqual(["CORE", "FALLBACK"], cluster["sourceRoles"])
            candidate = cluster["candidateNormal"]
            self.assertAlmostEqual(
                1.0,
                math.sqrt(sum(value * value for value in candidate)),
                places=7,
            )
            self.assertLessEqual(cluster["maximumCandidateAdjustmentDegrees"], 0.05)
            self.assertEqual(cluster["memberCount"], len(cluster["members"]))
            for row in cluster["members"]:
                role = row["sourceRole"]
                positions, normals, faces = parsed[role]
                index = row["normalRecordIndex"]
                self.assertEqual(index, row["vertexRecordIndex"])
                self.assertEqual(index, row["uvRecordIndex"])
                self.assertEqual(
                    tuple(float(value) for value in row["sourcePositionCentimetres"]),
                    positions[index - 1],
                )
                self.assertEqual(tuple(row["sourceNormal"]), normals[index - 1])
                group, tokens = faces[row["faceRecordIndex"] - 1]
                self.assertEqual("V5D_PUBLIC_ROAD_BASE_VISUAL_ASSUMPTION", group)
                self.assertIn((index, index, index), tokens)
                key = (role, index)
                self.assertNotIn(key, keys)
                keys.add(key)
                counts[role] += 1
        self.assertEqual(355, len(keys))
        self.assertEqual({"CORE": 174, "FALLBACK": 181}, counts)

    def test_native_mapping_explicitly_applies_legacy_obj_conversion(self):
        source = self.editor_source
        for token in (
            "FVector3f(ObjPosition.X, -ObjPosition.Y, ObjPosition.Z)",
            "FVector3f(ObjNormal.X, -ObjNormal.Y, ObjNormal.Z)",
            "FVector2f(ObjPosition.X * 0.01f, -ObjPosition.Y * 0.01f)",
            "Row.VertexRecordIndex - 1",
            "Row.NormalRecordIndex - 1",
            "Row.FaceRecordIndex - 1",
            "TriangleContainsVertexInstance",
            "CandidateNormals.Set(InstanceId, ExpectedCandidateNormal)",
            "ExpectedNativeNormals.Num() != Spec.Overrides",
            "SourceGroups.Get(Source.GetTrianglePolygonGroup(TriangleId))",
            "RoadMaterialSlotName",
        ):
            self.assertIn(token, source)
        self.assertNotIn("ComputeTriangleTangentsAndNormals", source)
        self.assertNotIn("ComputeTangentsAndNormals", source)

    def test_only_listed_normals_may_change_and_all_other_mesh_state_is_protected(self):
        source = self.editor_source
        for token in (
            "ProtectedDescriptionEqual",
            "SourcePositions.Get(Id) != CandidatePositions.Get(Id)",
            "SourceTangents.Get(Id) != CandidateTangents.Get(Id)",
            "SourceSigns.Get(Id) != CandidateSigns.Get(Id)",
            "SourceColors.Get(Id) != CandidateColors.Get(Id)",
            "SourceUvs.Get(Id, Channel)",
            "Source.GetTrianglePolygonGroup(Id)",
            "Source.GetTriangleVertexInstances(Id)",
            "SourceGroups.Get(Id) != CandidateGroups.Get(Id)",
            "An unlisted junction normal row changed",
            "Candidate->SetStaticMaterials(Source->GetStaticMaterials())",
            "Candidate->GetSectionInfoMap().CopyFrom",
            "Candidate->GetOriginalSectionInfoMap().CopyFrom",
            "StaticMeshAssetStateEqual",
            "BuildSettings.bRecomputeNormals",
            "BuildSettings.bRecomputeTangents",
            "NaniteSettings.FallbackPercentTriangles",
            "Body->AggGeom.GetElementCount()",
            "Mesh->bHasNavigationData",
            "StaticDuplicateObject",
            "CloneMeshDescription",
            "CreateMeshDescription",
            "CommitMeshDescription",
        ):
            self.assertIn(token, source)
        self.assertTrue(
            all(value is False for value in self.contract["preservationBoundary"].values())
        )

    def test_runtime_is_pointer_only_compiled_off_and_exact_fallback_is_mandatory(self):
        source = self.runtime_header + self.runtime_source + self.runtime_test
        for token in (
            "bRuntimeCandidateSelectionCompiledAuthorized = false",
            "bRuntimeCandidateActivationCompiledAuthorized = false",
            "ValidateExactFallbackMeshRoster",
            "ResolveOptionalRenderMeshes",
            "ExistingCoreMesh",
            "ExistingFallbackMesh",
            "The exact existing Core/Fallback public-realm pair is mandatory",
            "Missing exact Core/Fallback pair fails closed",
        ):
            self.assertIn(token, source)
        self.assertIn(
            "/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealmJunctionContinuityIntegration",
            self.runtime_source,
        )
        self.assertNotIn(
            "/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm/JunctionContinuityIntegration",
            self.runtime_source,
        )
        trust = self.contract["implementation"]["runtimeTrustBoundary"]
        self.assertTrue(trust["pointerSelectionOnly"])
        self.assertTrue(trust["exactCoreFallbackPairRequired"])
        self.assertFalse(trust["candidateSelectionCompiledAuthorized"])
        self.assertFalse(trust["candidateActivationCompiledAuthorized"])
        self.assertFalse(trust["runtimeCandidateSelectionReachable"])
        self.assertFalse(trust["runtimeCandidateActivationReachable"])

    def test_public_editor_surface_is_inspection_only_and_materializer_has_no_call_site(self):
        public_text, private_text = self.editor_header.split("private:", 1)
        symbol = "MaterializeTrustedPublicRealmJunctionContinuityAssetsInternal"
        self.assertIn("InspectPublicRealmJunctionContinuityReceipts", public_text)
        self.assertNotIn("Materialize", public_text)
        self.assertIn(symbol, private_text)
        self.assertEqual(1, self.editor_header.count("UFUNCTION(BlueprintCallable"))
        source_files = list((PLUGIN_ROOT / "Source").rglob("*.h")) + list(
            (PLUGIN_ROOT / "Source").rglob("*.cpp")
        )
        hits = {
            path.relative_to(REPO_ROOT).as_posix(): path.read_text(
                encoding="utf-8"
            ).count(symbol)
            for path in source_files
            if symbol in path.read_text(encoding="utf-8")
        }
        self.assertEqual(
            {
                EDITOR_HEADER.relative_to(REPO_ROOT).as_posix(): 1,
                EDITOR_SOURCE.relative_to(REPO_ROOT).as_posix(): 1,
            },
            hits,
        )
        inspection = self.editor_source.index(
            "InspectPublicRealmJunctionContinuityReceipts("
        )
        materializer = self.editor_source.index(symbol + "(")
        native_load = self.editor_source.index("LoadAndValidateSources(", materializer)
        self.assertIn(
            "ExpectedFutureTransactionAuthorizationSha256,\n            false,",
            self.editor_source[inspection:materializer],
        )
        self.assertIn(
            "ExpectedFutureTransactionAuthorizationSha256,\n            true,",
            self.editor_source[materializer:native_load],
        )
        for token in (
            "UNSET_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "UNSET_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "!IsSha256(TrustedAcceptedR33ReceiptSha256)",
            "!IsSha256(TrustedFutureAuthorizationSha256)",
            "bRequireCompiledTrustAnchors",
            "callerSuppliedHashesNeverAuthorizeExecution=true",
            "privateMaterializerInvoked=false",
        ):
            self.assertIn(token, self.editor_source)

    def test_authorization_is_exact_narrow_and_distinct(self):
        source = self.editor_source
        for token in (
            "ValidateAcceptedR33Receipt",
            "ValidateFutureAuthorization",
            "triad.istana_explore_v5d.r33_player0_capture.v1",
            "triad.istana_explore_v5d.r33_cesium_world_terrain_reference.native_transaction.v1",
            "ExactCoreCaptureCount" ,
            "ExplicitHumanReviewAcceptance",
            "MATERIALIZE_PUBLIC_REALM_JUNCTION_CONTINUITY_CANDIDATE_ASSETS_ONLY",
            "NormalOnlyMeshDescriptionMutation",
            "ExactCoreFallbackSourceAssetsPreserved",
            "MapOrComponentBindingAuthorized",
            "SourceObjMutationAuthorized",
            "TerrainCollisionNavigationLosRfSensorSimulationMutationAuthorized",
            "Root->Values.Num() != UE_ARRAY_COUNT(ExactFields)",
            "FPaths::IsSamePath(FullR33, FullAuthorization)",
        ):
            self.assertIn(token, source)
        trust = self.contract["implementation"]["executionTrustBoundary"]
        accepted = trust["acceptedR33CompiledTrustAnchor"]
        future = trust["futureAuthorizationCompiledTrustAnchor"]
        self.assertNotEqual(accepted, future)
        self.assertIsNone(re.fullmatch(r"[0-9A-F]{64}", accepted))
        self.assertIsNone(re.fullmatch(r"[0-9A-F]{64}", future))
        self.assertFalse(trust["compiledTrustAnchorsAreValidSha256"])
        self.assertFalse(trust["callerSuppliedHashesAloneSufficient"])
        self.assertFalse(trust["nativeWriteReachable"])

    def test_recursive_fresh_namespace_and_rollback_fail_closed(self):
        source = self.editor_source
        for token in (
            "CollectNamespaceState",
            "GetAssetsByPath",
            "TObjectIterator<UObject>",
            "TObjectIterator<UPackage>",
            "TryConvertLongPackageNameToFilename",
            "TryConvertFilenameToLongPackageName",
            "FindFilesRecursive",
            "PhysicalFiles",
            "PhysicalDirectories",
            "uncontracted physical file",
            "uncontracted physical directory",
            "DeleteLoadedAssets",
            "DeleteDirectory",
            "DirectoryExists(*After.PhysicalRoot)",
            "ROLLBACK_REFUSED",
            "ROLLBACK_FAILED",
            "ROLLBACK_COMPLETE",
            "EXISTING_NAMESPACE_REUSE_DENIED",
            "PARTIAL_NAMESPACE_DENIED",
        ):
            self.assertIn(token, source)
        self.assertGreaterEqual(source.count("FindFilesRecursive"), 2)
        self.assertTrue(all(self.contract["namespaceAndRollback"].values()))

    def test_no_map_component_actor_or_numbered_wrapper_mutation_surface(self):
        watched = {
            RUNTIME_HEADER: self.runtime_header,
            RUNTIME_SOURCE: self.runtime_source,
            RUNTIME_TEST: self.runtime_test,
            EDITOR_HEADER: self.editor_header,
            EDITOR_SOURCE: self.editor_source,
            API_AUDIT_PATH: API_AUDIT_PATH.read_text(encoding="utf-8"),
        }
        forbidden_stage = "R" + str(34)
        forbidden_calls = (
            "SpawnActor",
            "LoadMap(",
            "OpenLevel(",
            "GetEditorWorld(",
            "SetActorLocation",
            "SetActorRotation",
            "SetActorTransform",
            "SetWorldTransform",
            "SetRelativeTransform",
            "SetStaticMesh(",
            "SetMaterial(",
            "SetCollisionEnabled",
            "SetCanEverAffectNavigation",
            "AddInstance(",
            "RemoveInstance(",
            "DestroyActor",
        )
        for path, text in watched.items():
            self.assertNotIn(forbidden_stage, text, path)
            for call in forbidden_calls:
                self.assertNotIn(call, text, f"{path}: {call}")
        for row in self.contract["preservedExternalPins"]["numberedWrappers"]:
            path = REPO_ROOT / row["file"]
            self.assertTrue(path.is_file(), path)
            historical = HISTORICAL_WRAPPER_RECEIPT_PINS.get(row["file"])
            if historical is not None:
                self.assertEqual(historical, (row["bytes"], row["sha256"]), path)
                continue
            self.assertEqual(row["bytes"], path.stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)

    def test_api_audit_is_static_and_exact_when_ue55_is_present(self):
        self.assertEqual(
            "STATIC_INSTALLED_HEADER_AUDIT_ONLY_NOT_COMPILED_OR_EXECUTED",
            self.api_audit["status"],
        )
        self.assertFalse(self.api_audit["launchOrBuildPerformed"])
        self.assertTrue(
            all(value is False for value in self.api_audit["nativeClaims"].values())
        )
        self.assertEqual(
            self.contract["implementation"]["executionTrustBoundary"],
            {
                **self.api_audit["sourceTrustBoundary"],
                "acceptedR33CompiledTrustAnchor": (
                    "UNSET_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"
                ),
                "futureAuthorizationCompiledTrustAnchor": (
                    "UNSET_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE"
                ),
            },
        )
        self.assertEqual(
            self.contract["implementation"]["runtimeTrustBoundary"],
            self.api_audit["runtimeTrustBoundary"],
        )
        engine_source = Path("C:/Program Files/Epic Games/UE_5.5/Engine/Source")
        if not engine_source.is_dir():
            self.skipTest("Pinned UE 5.5 install is not present on this host")
        for row in self.api_audit["headers"]:
            path = engine_source / row["pathBelowEngineSource"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(row["bytes"], path.stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)

    def test_artifact_roster_hashes_and_remaining_native_proof_are_exact(self):
        expected = {
            path.relative_to(REPO_ROOT).as_posix()
            for path in (
                RUNTIME_HEADER,
                RUNTIME_SOURCE,
                RUNTIME_TEST,
                EDITOR_HEADER,
                EDITOR_SOURCE,
                API_AUDIT_PATH,
                README_PATH,
                Path(__file__).absolute(),
            )
        }
        rows = self.contract["implementation"]["artifacts"]
        self.assertEqual(expected, {row["file"] for row in rows})
        for row in rows:
            path = REPO_ROOT / row["file"]
            self.assertEqual(row["bytes"], path.stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)
        required = {
            "UE55_UBT_UHT_COMPILE_AND_LINK",
            "REVIEWED_COMPILED_TRUST_ANCHOR_ACTIVATION",
            "EXACT_OBJ_RECORD_TO_MESHDESCRIPTION_MAPPING",
            "MATERIALIZE_SAVE_UNLOAD_COLD_RELOAD_EXACT_2_ASSETS",
            "EXACT_355_NORMAL_ONLY_BEFORE_AFTER_NATIVE_PROOF",
            "SHARP_BEND_AND_FOUR_WAY_JUNCTION_MATCHED_CAPTURES",
            "NANITE_AND_RASTER_PARITY",
            "COLLISION_NAVIGATION_LOS_RF_SENSOR_SIMULATION_TERRAIN_REGRESSION",
            "TARGET_HARDWARE_MEMORY_FRAME_TIME_AND_DRAW_CALL_ACCEPTANCE",
            "EXPLICIT_HUMAN_VISUAL_ACCEPTANCE",
        }
        self.assertEqual(required, set(self.contract["remainingNativeProof"]))

    def test_scoped_readme_states_dormant_boundary_and_serial_checks(self):
        readme = README_PATH.read_text(encoding="utf-8")
        for token in (
            "dormant, fail-closed post-R33 source scaffold",
            "private, non-reflected materializer has no call site",
            "Runtime candidate selection and activation are independently compiled off",
            "exact 355 listed render-normal records: 174 on Core and 181 on Fallback",
            "/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealmJunctionContinuityIntegration/Meshes",
            "The exact existing Core and Fallback meshes remain mandatory",
            "does not bind a map, actor, component, or numbered wrapper",
            "Run these checks serially",
            "do not establish compilation",
        ):
            self.assertIn(token, readme)

    def test_cpp_scaffolding_is_lexically_balanced(self):
        for path, source in (
            (RUNTIME_SOURCE, self.runtime_source),
            (RUNTIME_TEST, self.runtime_test),
            (EDITOR_SOURCE, self.editor_source),
        ):
            self.assertEqual(source.count("{"), source.count("}"), path)
            self.assertEqual(source.count("#if"), source.count("#endif"), path)
            self.assertNotRegex(source, r"\bTODO\b|\bFIXME\b")


if __name__ == "__main__":
    unittest.main(verbosity=2)
