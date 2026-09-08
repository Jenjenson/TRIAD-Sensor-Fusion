import hashlib
import json
import os
import re
import unittest
from collections import Counter
from pathlib import Path


UNREAL_ROOT = Path(__file__).absolute().parents[3]
REPO_ROOT = UNREAL_ROOT.parent
PLUGIN_ROOT = UNREAL_ROOT / "Plugins/TRIADSensorFusion"
TREE_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/TreeRealism"
)
CANDIDATE_ROOT = TREE_ROOT / "TropicalUmbrellaHeroCandidate"
INTEGRATION_ROOT = TREE_ROOT / "TropicalUmbrellaHeroIntegration"
CONTRACT_PATH = (
    INTEGRATION_ROOT
    / "tropical_umbrella_hero_post_r33_integration.source_contract.v1.json"
)
IDENTITY_PATH = (
    INTEGRATION_ROOT / "tropical_umbrella_hero_source_instances.v1.json"
)
API_AUDIT_PATH = (
    INTEGRATION_ROOT / "tropical_umbrella_hero_ue55_api_audit.source.json"
)
README_PATH = INTEGRATION_ROOT / "README.md"
CANDIDATE_CONTRACT_PATH = (
    CANDIDATE_ROOT / "tropical_umbrella_hero_candidate.contract.json"
)
CANDIDATE_TEST_PATH = (
    PLUGIN_ROOT
    / "Tests/test_istana_explore_v5d_tropical_umbrella_hero_candidate.py"
)
SELECTOR_PATH = (
    TREE_ROOT
    / "GeometryVariationCandidateV4/tree_geometry_instance_selector.v4.json"
)
RUNTIME_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DTropicalUmbrellaHeroActor.h"
)
RUNTIME_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DTropicalUmbrellaHeroActor.cpp"
)
RUNTIME_TEST = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/Tests/"
    "TRIADIstanaExploreV5DTropicalUmbrellaHeroActorTests.cpp"
)
EDITOR_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary.h"
)
EDITOR_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary.cpp"
)
EDITOR_TEST = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Private/Tests/"
    "TRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibraryTests.cpp"
)


def io_path(path: Path) -> Path:
    if os.name == "nt" and not str(path).startswith("\\\\?\\"):
        return Path("\\\\?\\" + str(path.absolute()))
    return path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with io_path(path).open("rb") as handle:
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
        io_path(path).read_text(encoding="utf-8"),
        object_pairs_hook=reject_duplicate_keys,
        parse_constant=lambda value: (_ for _ in ()).throw(
            ValueError(f"non-finite JSON number: {value}")
        ),
    )


def cpp_function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1 : index]
    raise AssertionError(f"unbalanced C++ function: {signature}")


class TropicalUmbrellaHeroPostR33SourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = load_strict_json(CONTRACT_PATH)
        cls.candidate = load_strict_json(CANDIDATE_CONTRACT_PATH)
        cls.selector = load_strict_json(SELECTOR_PATH)
        cls.identities = load_strict_json(IDENTITY_PATH)
        cls.audit = load_strict_json(API_AUDIT_PATH)
        cls.readme = io_path(README_PATH).read_text(encoding="utf-8")
        cls.runtime_header = io_path(RUNTIME_HEADER).read_text(encoding="utf-8")
        cls.runtime_source = io_path(RUNTIME_SOURCE).read_text(encoding="utf-8")
        cls.runtime_test = io_path(RUNTIME_TEST).read_text(encoding="utf-8")
        cls.editor_header = io_path(EDITOR_HEADER).read_text(encoding="utf-8")
        cls.editor_source = io_path(EDITOR_SOURCE).read_text(encoding="utf-8")
        cls.editor_test = io_path(EDITOR_TEST).read_text(encoding="utf-8")

    def test_delivery_is_source_only_and_every_native_claim_is_false(self):
        self.assertEqual(
            "triad.istana_public_view_explore_v5d.tree_realism."
            "tropical_umbrella_hero.post_r33_source_integration.v1",
            self.contract["schema"],
        )
        self.assertEqual("SOURCE_SCAFFOLD_NOT_EXECUTED", self.contract["status"])
        state = dict(self.contract["deliveryState"])
        self.assertTrue(state.pop("sourceImplemented"))
        self.assertTrue(all(value is False for value in state.values()))
        self.assertTrue(
            all(value is False for value in self.contract["nativeClaims"].values())
        )
        sequencing = self.contract["sequencing"]
        for key in (
            "acceptedR33ReceiptRequired",
            "futureExplicitTransactionRequired",
            "distinctCurrentAndFutureReceiptsRequired",
            "futureReviewedSourceChangesAndRecompileRequired",
        ):
            self.assertTrue(sequencing[key])
        for key in (
            "callerHashesAuthorizeExecution",
            "materializationReachable",
            "runtimeSelectionReachable",
            "runtimeActivationReachable",
            "mapSaveAuthorized",
            "numberedSuccessorAssigned",
        ):
            self.assertFalse(sequencing[key])

    def test_all_inputs_have_exact_byte_and_sha256_pins(self):
        inputs = self.contract["pinnedInputs"]
        simple = {
            "candidateContract": CANDIDATE_CONTRACT_PATH,
            "materialLibrary": CANDIDATE_ROOT
            / "Generated/M_IPV5D_TropicalUmbrellaHeroCandidate.mtl",
            "offlineAudit": CANDIDATE_ROOT
            / "OfflineAudit/tropical_umbrella_hero_candidate.audit.json",
            "offlineAuditImage": CANDIDATE_ROOT
            / "OfflineAudit/tropical_umbrella_hero_candidate_fixed_views.png",
            "treeRealismContract": TREE_ROOT
            / "istana_public_view_v5d_tree_realism.contract.json",
            "geometrySelector": SELECTOR_PATH,
            "sourceIdentityManifest": IDENTITY_PATH,
        }
        for key, path in simple.items():
            receipt = inputs[key]
            self.assertEqual(path.relative_to(REPO_ROOT).as_posix(), receipt["file"])
            self.assertEqual(io_path(path).stat().st_size, receipt["bytes"])
            self.assertEqual(sha256(path), receipt["sha256"])
        expected_objs = []
        for variant in "ABC":
            for lod in range(3):
                expected_objs.append(
                    CANDIDATE_ROOT
                    / f"Generated/SM_IPV5D_TropicalUmbrellaHero_{variant}_LOD{lod}.obj"
                )
        self.assertEqual(9, len(inputs["candidateObjs"]))
        self.assertEqual(
            [path.relative_to(REPO_ROOT).as_posix() for path in expected_objs],
            [row["file"] for row in inputs["candidateObjs"]],
        )
        for row, path in zip(inputs["candidateObjs"], expected_objs):
            self.assertEqual(io_path(path).stat().st_size, row["bytes"])
            self.assertEqual(sha256(path), row["sha256"])
        self.assertEqual(31330, inputs["candidateContract"]["bytes"])
        self.assertEqual(
            "504E94CCC21E5C671E290181A14DDA6339A0D7FED365EE822ABD54E90CC63130",
            inputs["candidateContract"]["sha256"],
        )
        self.assertEqual(
            "8083F401DAFBE2AFF1B5234ECA7A07BE2E540C04FD113DDEC15343554A2DF980",
            inputs["offlineAudit"]["sha256"],
        )

    def test_exact_six_native_classified_umbrella_identities(self):
        selected = [
            row for row in self.selector["rows"] if row["sourceForm"] == "umbrella"
        ]
        expected = [
            (722, "v4Heritage", 2, "v4.heritage.HT2003-108", "C"),
            (726, "v4Heritage", 6, "v4.heritage.HT2008-169", "B"),
            (727, "v4Heritage", 7, "v4.heritage.HT2018-298", "B"),
            (728, "v4Heritage", 8, "v4.heritage.HT2019-306", "B"),
            (729, "r29Landmark", 0, "r29.landmark.00", "A"),
            (734, "r29Landmark", 5, "r29.landmark.05", "B"),
        ]
        self.assertEqual(
            expected,
            [
                (
                    row["ordinal"],
                    row["sourceDomain"],
                    row["sourceIndex"],
                    row["sourceInstanceKey"],
                    row["variantSelector"],
                )
                for row in selected
            ],
        )
        manifest_rows = self.identities["rows"]
        self.assertEqual(6, len(manifest_rows))
        self.assertEqual(
            expected,
            [
                (
                    row["selectorManifestOrdinal"],
                    row["sourceDomain"],
                    row["sourceIndex"],
                    row["sourceInstanceKey"],
                    row["candidateVariant"],
                )
                for row in manifest_rows
            ],
        )
        self.assertEqual(Counter("ABBBBC"), Counter(row[-1] for row in expected))
        self.assertFalse(
            self.identities["transformBoundary"][
                "numericWorldTransformsInventedOrSerialized"
            ]
        )

    def test_three_variants_have_true_exact_lod_geometry(self):
        geometry = self.contract["candidateGeometry"]
        self.assertEqual(["A", "B", "C"], geometry["variants"])
        self.assertEqual([0, 1, 2], geometry["trueLods"])
        self.assertEqual([37136, 14372, 3156], geometry["verticesPerLod"])
        self.assertEqual([54828, 21352, 4776], geometry["trianglesPerLod"])
        self.assertEqual([61, 47, 33], geometry["rootVerticesPerLod"])
        self.assertEqual(["Bark", "LeafLive", "LeafDry"], geometry["materials"])
        self.assertEqual(
            self.candidate["geometry"]["variants"],
            geometry["exactVariantLodEvidence"],
        )
        for token in (
            "ImportUniformScale = 100.0f",
            "bConvertScene = false",
            "bConvertSceneUnit = false",
            "bImportMaterials = false",
            "bImportTextures = false",
            "bAutoGenerateCollision = false",
            "bBuildNanite = false",
            "bRemoveDegenerates = false",
            "SetNumSourceModels(LodCount)",
            "CreateMeshDescription",
            "CommitMeshDescription",
            "const float ScreenSizes[LodCount] = {1.0f, 0.42f, 0.16f}",
            "LodTrianglesByMaterial",
            "ExpectedUnrealBoundsMinCm",
            "ExpectedUnrealBoundsMaxCm",
        ):
            self.assertIn(token, self.editor_source)

    def test_runtime_fallback_checks_actual_mesh_bindings_and_gates(self):
        source = self.runtime_header + self.runtime_source + self.runtime_test
        for token in (
            "constexpr bool bRuntimeSelectionCompiled = false",
            "constexpr bool bRuntimeActivationCompiled = false",
            "static_assert(!bRuntimeSelectionCompiled)",
            "static_assert(!bRuntimeActivationCompiled)",
            "UNSET_RUNTIME_CURRENT_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "UNSET_RUNTIME_FUTURE_TRUST_ANCHOR_REQUIRES_SEPARATE_REVIEWED_SOURCE_CHANGE",
            "Mesh->GetMaterial(Slot) != Material",
            "Exact TreeRealism umbrella mesh/material fallback is mandatory",
            "const FTransform ExactCopy = Anchor.SourceWorldTransform",
            "FMemory::Memcmp(&A, &B, sizeof(T)) == 0",
            "SetCollisionEnabled(ECollisionEnabled::NoCollision)",
            "SetCanEverAffectNavigation(false)",
            "Component->AddInstances(",
            "false,\n            true,\n            false",
        ):
            self.assertIn(token, source)
        self.assertNotIn("SpawnActor", source)
        self.assertNotIn("SetActorTransform", source)
        self.assertNotIn("SetWorldTransform", source)

    def test_leafdry_is_exactly_distinct_and_leaf_graphs_are_inherited(self):
        policy = self.contract["materialPolicy"]
        self.assertEqual([], policy["barkOverrides"])
        self.assertEqual([], policy["leafLiveOverrides"])
        self.assertEqual(
            {
                "TRIAD_TreeResponseLumaLow": 0.82,
                "TRIAD_TreeResponseLumaHigh": 0.96,
                "TRIAD_TreeResponseDesaturationFraction": 0.28,
                "TRIAD_TreeResponseRoughnessMin": 0.72,
                "TRIAD_TreeResponseRoughnessMax": 0.90,
                "TRIAD_TreeResponseBaseColorMax": 0.72,
            },
            policy["leafDryScalarOverrides"],
        )
        self.assertEqual(
            {
                "TRIAD_TreeResponseTintLow": [1.30, 0.79, 0.38, 1.0],
                "TRIAD_TreeResponseTintHigh": [1.12, 0.70, 0.34, 1.0],
                "TRIAD_TreeResponseSubsurfaceTint": [0.84, 0.58, 0.26, 1.0],
            },
            policy["leafDryVectorOverrides"],
        )
        self.assertEqual(0.333, policy["inheritedOpacityMaskClip"])
        self.assertEqual(6, len(policy["inheritedWindParameters"]))
        self.assertFalse(policy["nativeClosedLeafletOpacityWindAccepted"])
        for token in (
            "ValidateCandidateMaterial(",
            "ValidateLeafOpacityWindInheritance(",
            "LeafDryScalarOverrides",
            "LeafDryVectorOverrides",
            "only LeafDry may own the exact six-scalar/three-vector set",
            "LeafLive->GetBlendMode() != BLEND_Masked",
            "LeafDry->GetBlendMode() != BLEND_Masked",
            "MSM_TwoSidedFoliage",
            "InheritedLeafOpacityMaskClip",
            "TRIAD_WindHeightCm",
            "TRIAD_WindDirection",
            "NATIVE_LOD_WIND_AND_ALPHA_ACCEPTANCE_REQUIRED",
        ):
            self.assertIn(token, self.editor_source)
        self.assertIn("native fixed-view wind/alpha review", self.readme)

    def test_only_public_reflected_surface_is_read_only_inspection(self):
        self.assertEqual(1, self.editor_header.count("UFUNCTION("))
        public_prefix, private_suffix = self.editor_header.split("private:", 1)
        self.assertIn("InspectTropicalUmbrellaHeroReceipts", public_prefix)
        self.assertNotIn("MaterializeAndStageTrusted", public_prefix)
        self.assertIn(
            "MaterializeAndStageTrustedTropicalUmbrellaHeroInternal",
            private_suffix,
        )
        self.assertNotIn("UFUNCTION", private_suffix)
        self.assertEqual(
            1,
            self.editor_source.count(
                "MaterializeAndStageTrustedTropicalUmbrellaHeroInternal"
            ),
        )
        self.assertEqual(
            1,
            self.editor_header.count(
                "MaterializeAndStageTrustedTropicalUmbrellaHeroInternal"
            ),
        )
        cpp_roster = [
            RUNTIME_HEADER,
            RUNTIME_SOURCE,
            RUNTIME_TEST,
            EDITOR_HEADER,
            EDITOR_SOURCE,
            EDITOR_TEST,
        ]
        occurrences = sum(
            io_path(path)
            .read_text(encoding="utf-8")
            .count("MaterializeAndStageTrustedTropicalUmbrellaHeroInternal")
            for path in cpp_roster
        )
        self.assertEqual(2, occurrences)  # one private declaration, one definition
        inspection_body = cpp_function_body(
            self.editor_source,
            "InspectTropicalUmbrellaHeroReceipts(",
        )
        self.assertIn("false,\n            Admission", inspection_body)
        for forbidden in (
            "CreateAsset",
            "ImportAssetTasks",
            "SaveLoadedAssets",
            "SetVisibility",
            "SetHiddenInGame",
        ):
            self.assertNotIn(forbidden, inspection_body)

    def test_editor_materializer_and_runtime_gates_are_independent_false(self):
        for token in (
            "constexpr bool bMaterializationAndSwapCompiled = false",
            "static_assert(!bMaterializationAndSwapCompiled)",
            "UNSET_EDITOR_CURRENT_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "UNSET_EDITOR_FUTURE_TRUST_ANCHOR_REQUIRES_SEPARATE_REVIEWED_SOURCE_CHANGE",
            "!bMaterializationAndSwapCompiled",
            "bRuntimeSelectionCompiled = false",
            "bRuntimeActivationCompiled = false",
        ):
            self.assertIn(token, self.editor_source + self.runtime_source)
        materializer = cpp_function_body(
            self.editor_source,
            "MaterializeAndStageTrustedTropicalUmbrellaHeroInternal(",
        )
        self.assertIn("true,\n            Admission", materializer)
        self.assertFalse(self.contract["sequencing"]["compiledTrustAnchorsPopulated"])
        self.assertEqual(
            3, self.contract["sequencing"]["independentCompiledFalseGateCount"]
        )

    def test_exact_components_counts_and_atomic_restore_are_concrete(self):
        materializer = cpp_function_body(
            self.editor_source,
            "MaterializeAndStageTrustedTropicalUmbrellaHeroInternal(",
        )
        suppress = cpp_function_body(
            self.editor_source,
            "bool SuppressExactSourcePresentation(",
        )
        restore = cpp_function_body(
            self.editor_source,
            "bool RestoreExactSourcePresentation(",
        )
        for token in (
            'FName(TEXT("V4HeritageUmbrellaSilhouetteProxies"))',
            'FName(TEXT("V5DLandmarkTreesUmbrella"))',
            "EExactSourceOwnerKind::V4Landscape",
            "EExactSourceOwnerKind::V5DLandmarkVegetation",
            "4,\n            World,\n            V4Heritage",
            "2,\n            World,\n            R29Landmark",
            "BuildExactNativeAnchorCopy(",
            "BeforeTransformBitHash",
            "AfterTransformBitHash",
            "BeforeTransformBitHash != AfterTransformBitHash",
        ):
            self.assertIn(token, materializer)
        for component in ("V4Heritage", "R29Landmark"):
            self.assertIn(f"{component}.Component->SetVisibility(false, false)", suppress)
            self.assertIn(f"{component}.Component->SetHiddenInGame(true, false)", suppress)
            self.assertNotIn(f"{component}.Component->SetVisibility(false, true)", suppress)
            self.assertNotIn(f"{component}.Component->SetHiddenInGame(true, true)", suppress)
        self.assertIn("RestoreExactSourcePresentation(", suppress)
        self.assertIn("ValidateExactSourceComponentUnchanged(", suppress)
        self.assertIn("for (const FExactSourceComponentSnapshot* Snapshot", restore)
        self.assertIn("Snapshot->bVisible", restore)
        self.assertIn("Snapshot->bHiddenInGame", restore)
        self.assertGreaterEqual(restore.count("false);"), 2)
        rollback_lambda = materializer[
            materializer.index("const auto FailAndRollback") :
            materializer.index("IAssetTools& AssetTools")
        ]
        for token in (
            "RollbackPresentationToMandatoryFallbackInternal",
            "RestoreExactSourcePresentation(",
            "RollbackFreshNamespaces(",
            "sourcePresentationRestored",
            "namespacesRolledBack",
        ):
            self.assertIn(token, rollback_lambda)
        self.assertIn('FailAndRollback(TEXT("SOURCE_SUPPRESSION"))', materializer)
        self.assertIn('FailAndRollback(TEXT("RUNTIME_ACTIVATION"))', materializer)
        self.assertEqual(
            ["V4HeritageUmbrellaSilhouetteProxies", "V5DLandmarkTreesUmbrella"],
            self.contract["sourceSwap"]["exactComponentNames"],
        )
        self.assertEqual([4, 2], self.contract["sourceSwap"]["exactComponentCounts"])

    def test_exact_owner_child_and_target_world_boundaries_are_concrete(self):
        owner = cpp_function_body(
            self.editor_source,
            "bool ValidateExactSourceOwnerBinding(",
        )
        for token in (
            "Owner->GetWorld() != ExpectedWorld",
            "Owner->GetLevel() != ExpectedWorld->PersistentLevel",
            "Component->GetAttachChildren().IsEmpty()",
            "ATRIADIstanaExploreV4LandscapeActor::StaticClass()",
            "Typed->HeritageUmbrellaInstances.Get() != Component",
            "ATRIADIstanaExploreV5DLandmarkVegetationActor::StaticClass()",
            "Typed->UmbrellaTreeInstances.Get() != Component",
        ):
            self.assertIn(token, owner)
        world = cpp_function_body(
            self.editor_source,
            "bool ValidateExactTargetPersistentEditorWorld(",
        )
        for token in (
            "OutWorld->WorldType != EWorldType::Editor",
            "OutWorld->HasBegunPlay()",
            "WorldPackage->GetName() != TargetMapPackage",
            "OutWorld->PersistentLevel->GetOutermost() != WorldPackage",
            "CandidateActor->GetLevel() != OutWorld->PersistentLevel",
        ):
            self.assertIn(token, world)
        materializer = cpp_function_body(
            self.editor_source,
            "MaterializeAndStageTrustedTropicalUmbrellaHeroInternal(",
        )
        self.assertLess(
            materializer.index("ValidateExactTargetPersistentEditorWorld("),
            materializer.index("NamespaceMatchesExactAssets("),
        )
        self.assertEqual(
            "/Game/Maps/Istana_PublicView_Explore_v5d_hybrid",
            self.contract["sequencing"]["exactTargetPersistentEditorWorldPackage"],
        )
        self.assertTrue(self.contract["sourceSwap"]["exactOwnerClassAndPropertyRequired"])
        self.assertTrue(self.contract["sourceSwap"]["zeroAttachedChildrenRequired"])
        self.assertFalse(self.contract["sourceSwap"]["visibilityPropagationToChildren"])

    def test_source_instances_are_never_added_removed_or_transformed(self):
        source_helpers = "\n".join(
            cpp_function_body(self.editor_source, signature)
            for signature in (
                "bool CaptureExactSourceComponent(",
                "bool ValidateExactSourceComponentUnchanged(",
                "bool RestoreExactSourcePresentation(",
                "bool SuppressExactSourcePresentation(",
            )
        )
        for forbidden in (
            "AddInstance(",
            "AddInstances(",
            "RemoveInstance(",
            "RemoveInstances(",
            "ClearInstances(",
            "UpdateInstanceTransform(",
            "BatchUpdateInstancesTransform",
            "SetStaticMesh(",
            "SetMaterial(",
            "SetCollision",
            "SetCanEverAffectNavigation",
        ):
            self.assertNotIn(forbidden, source_helpers)
        self.assertIn("GetInstanceTransform(", source_helpers)
        self.assertFalse(self.contract["sourceSwap"]["newPlacements"])
        self.assertFalse(self.contract["sourceSwap"]["deletedPlacements"])
        self.assertFalse(self.contract["sourceSwap"]["sourceTransformsModified"])

    def test_namespace_rollback_always_attempts_output_and_staging(self):
        rollback = cpp_function_body(
            self.editor_source,
            "bool RollbackFreshNamespaces(",
        )
        self.assertIn("DeleteFreshNamespaceContents(\n        OutputRoot", rollback)
        self.assertIn("DeleteFreshNamespaceContents(\n        StagingRoot", rollback)
        self.assertIn("Both scopes are always attempted", rollback)
        self.assertNotIn("&& DeleteFreshNamespaceContents", rollback)
        delete = cpp_function_body(
            self.editor_source,
            "bool DeleteFreshNamespaceContents(",
        )
        for token in (
            "ScanPathsSynchronous(",
            "PhysicalFiles.IsEmpty()",
            "PhysicalDirectories.IsEmpty()",
            "bPhysicalRootAbsent",
            "DirectoryExists(*After.PhysicalRoot)",
        ):
            self.assertIn(token, delete)
        collect = cpp_function_body(
            self.editor_source,
            "void CollectNamespaceState(",
        )
        for token in (
            "TryConvertLongPackageNameToFilename(",
            "FindFilesRecursive(",
            "PhysicalFiles.Add",
            "PhysicalDirectories.Add",
            "TryConvertFilenameToLongPackageName(",
        ):
            self.assertIn(token, collect)
        matcher = cpp_function_body(
            self.editor_source,
            "bool NamespaceMatchesExactAssets(",
        )
        self.assertIn("ExpectedPhysicalFiles", matcher)
        self.assertIn("ExpectedPhysicalDirectories", matcher)
        materializer = cpp_function_body(
            self.editor_source,
            "MaterializeAndStageTrustedTropicalUmbrellaHeroInternal(",
        )
        self.assertIn(
            "Baseline.bOutputProvenEmptyAtEntry = true", materializer
        )
        self.assertIn(
            "Baseline.bStagingProvenEmptyAtEntry = true", materializer
        )
        self.assertEqual(6, self.contract["namespaces"]["outputAssetCount"])
        self.assertEqual(9, self.contract["namespaces"]["stagingAssetCount"])
        self.assertTrue(self.contract["namespaces"]["recursivePhysicalFilesystemInventoryRequired"])
        self.assertTrue(self.contract["namespaces"]["registryRescanBeforeAndAfterRollback"])
        self.assertTrue(self.contract["namespaces"]["rollbackRequiresPhysicalRootAbsent"])

    def test_obj_and_mtl_pins_are_rehashed_immediately_after_import(self):
        import_body = cpp_function_body(
            self.editor_source,
            "bool ImportStagingLods(",
        )
        self.assertLess(
            import_body.index("AssetTools.ImportAssetTasks(Tasks)"),
            import_body.index("RevalidateImporterSourcePinsImmediatelyAfterImport("),
        )
        self.assertLess(
            import_body.index("RevalidateImporterSourcePinsImmediatelyAfterImport("),
            import_body.index("FinishAllCompilation()"),
        )
        rehash = cpp_function_body(
            self.editor_source,
            "bool RevalidateImporterSourcePinsImmediatelyAfterImport(",
        )
        self.assertIn("ObjPins[Variant][Lod]", rehash)
        self.assertIn("MaterialLibrarySha256", rehash)
        self.assertIn("POST_IMPORT_OBJ_TOCTOU_PIN_CLOSED", rehash)
        self.assertIn("POST_IMPORT_MTL_TOCTOU_WINDOW_CLOSED", rehash)
        self.assertTrue(self.contract["pinnedInputs"]["postImportObjAndMtlRehashRequired"])

    def test_api_audit_is_exact_static_only_and_native_proof_remains(self):
        self.assertEqual(
            "STATIC_HEADER_AUDIT_ONLY_NOT_COMPILED_OR_EXECUTED",
            self.audit["status"],
        )
        self.assertFalse(self.audit["launchOrBuildPerformed"])
        self.assertTrue(
            all(value is False for value in self.audit["nativeClaims"].values())
        )
        engine_root = Path("C:/Program Files/Epic Games/UE_5.5/Engine/Source")
        if io_path(engine_root).is_dir():
            for row in self.audit["headers"] + self.audit["engineSources"]:
                path = engine_root / row["pathBelowEngineSource"]
                self.assertTrue(io_path(path).is_file(), path)
                self.assertEqual(row["bytes"], io_path(path).stat().st_size, path)
                self.assertEqual(row["sha256"], sha256(path), path)
        unresolved = " ".join(self.audit["unresolvedNativeProof"])
        for phrase in (
            "compile and link",
            "cold-reload",
            "both suppression branches",
            "LOD transition",
            "human visual",
        ):
            self.assertIn(phrase, unresolved)

    def test_artifact_roster_and_hashes_are_exact(self):
        expected = {
            path.relative_to(REPO_ROOT).as_posix()
            for path in (
                RUNTIME_HEADER,
                RUNTIME_SOURCE,
                RUNTIME_TEST,
                EDITOR_HEADER,
                EDITOR_SOURCE,
                EDITOR_TEST,
                API_AUDIT_PATH,
                README_PATH,
                IDENTITY_PATH,
                CANDIDATE_TEST_PATH,
                Path(__file__).absolute(),
            )
        }
        rows = self.contract["implementation"]["artifacts"]
        self.assertEqual(expected, {row["file"] for row in rows})
        for row in rows:
            path = REPO_ROOT / row["file"]
            self.assertTrue(io_path(path).is_file(), path)
            self.assertEqual(row["bytes"], io_path(path).stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)

    def test_no_numbered_or_native_artifact_was_touched_by_integration(self):
        forbidden = self.contract["preservation"]
        for key in (
            "numberedTransactionScriptsModified",
            "numberedCaptureScriptsModified",
            "nativeMapModified",
            "nativeDllModified",
            "collisionModified",
            "navigationModified",
            "geospatialLayoutModified",
            "losAuthorityModified",
            "sensorAuthorityModified",
            "rfAuthorityModified",
            "terrainAuthorityModified",
        ):
            self.assertFalse(forbidden[key])
        self.assertNotIn("Istana_PublicView_Explore_v5d_hybrid.umap", self.editor_source)
        self.assertNotIn("SaveMap", self.editor_source)
        self.assertNotIn("EditorLoadingAndSavingUtils", self.editor_source)
        self.assertFalse(self.contract["officialReferenceBoundary"]["speciesClaimed"])
        self.assertFalse(
            self.contract["officialReferenceBoundary"]["unsurveyedPlacementClaimed"]
        )


if __name__ == "__main__":
    unittest.main()
