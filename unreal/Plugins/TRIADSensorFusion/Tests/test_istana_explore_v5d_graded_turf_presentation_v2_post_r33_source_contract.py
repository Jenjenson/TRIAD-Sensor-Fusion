import hashlib
import json
import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
PLUGIN_ROOT = REPO_ROOT / "unreal/Plugins/TRIADSensorFusion"
INTEGRATION_ROOT = (
    REPO_ROOT
    / "unreal/SourceAssets/IstanaPublicViewExploreV5D/Vegetation"
    / "GradedTurfPresentationIntegrationV2"
)
CONTRACT_PATH = INTEGRATION_ROOT / "graded_turf_presentation_v2_post_r33.source_contract.v1.json"
API_AUDIT_PATH = INTEGRATION_ROOT / "graded_turf_presentation_v2_ue55_api_audit.source.json"
README_PATH = INTEGRATION_ROOT / "README.md"
RUNTIME_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Public"
    / "TRIADIstanaExploreV5DGradedTurfPresentationActor.h"
)
RUNTIME_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private"
    / "TRIADIstanaExploreV5DGradedTurfPresentationActor.cpp"
)
RUNTIME_TEST = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/Tests"
    / "TRIADIstanaExploreV5DGradedTurfPresentationActorTests.cpp"
)
EDITOR_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Public"
    / "TRIADIstanaExploreV5DGradedTurfPresentationEditorLibrary.h"
)
EDITOR_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DGradedTurfPresentationEditorLibrary.cpp"
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


class GradedTurfPresentationV2SourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
        cls.audit = json.loads(API_AUDIT_PATH.read_text(encoding="utf-8"))
        cls.runtime_header = RUNTIME_HEADER.read_text(encoding="utf-8")
        cls.runtime_source = RUNTIME_SOURCE.read_text(encoding="utf-8")
        cls.runtime_test = RUNTIME_TEST.read_text(encoding="utf-8")
        cls.editor_header = EDITOR_HEADER.read_text(encoding="utf-8")
        cls.editor_source = EDITOR_SOURCE.read_text(encoding="utf-8")
        cls.readme = README_PATH.read_text(encoding="utf-8")

    def test_contract_is_dormant_source_only_and_unaccepted(self):
        self.assertEqual(
            "triad.istana_public_view_explore_v5d.graded_turf_presentation_v2.post_r33_source_contract.v1",
            self.contract["schema"],
        )
        self.assertEqual(
            "POST_R33_UNNUMBERED_DORMANT_FAIL_CLOSED_SOURCE_ONLY",
            self.contract["status"],
        )
        self.assertEqual(1, self.contract["output"]["exactAssetCount"])
        self.assertEqual(
            "/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/GradedTurfPresentationIntegrationV2",
            self.contract["output"]["namespace"],
        )
        self.assertFalse(any(self.contract["nativeClaims"].values()))

    def test_all_source_inputs_remain_exact_and_unmodified(self):
        rows = self.contract["sourceInputs"]
        self.assertEqual(7, len(rows))
        for row in rows:
            path = REPO_ROOT / row["file"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(row["bytes"], path.stat().st_size)
            self.assertEqual(row["sha256"], sha256(path))
            self.assertFalse(row["modifiedByIntegration"])

    def test_exact_masked_overlay_clone_and_four_output_graft(self):
        graph = self.contract["materialGraph"]
        self.assertEqual("BLEND_Masked", graph["blendMode"])
        self.assertEqual(0.5, graph["opacityMaskClipValue"])
        self.assertEqual(26, graph["acceptedOverlayExpressionCount"])
        self.assertEqual(25, graph["grass001ResponseExpressionCount"])
        self.assertEqual(
            ["BaseColor", "Roughness", "Normal", "AmbientOcclusion"],
            graph["onlyChangedOutputs"],
        )
        self.assertEqual(["OpacityMask", "Specular"], graph["preservedOutputs"])
        self.assertEqual(64, graph["estateBoundaryPointCount"])
        self.assertEqual(50.0, graph["opaqueCoreCollarMetres"])
        self.assertEqual(8.0, graph["outwardFeatherMetres"])
        self.assertEqual(0.25, graph["stableWorldDitherCellMetres"])
        self.assertTrue(graph["opacityMaskDependencyGraphComparedBeforeAndAfter"])
        self.assertEqual(
            ["ExpressionGraph", "OutputIndex", "Mask", "MaskR", "MaskG", "MaskB", "MaskA"],
            graph["preservedRootInputFieldsCompared"],
        )
        self.assertEqual(
            ["BaseColor", "Roughness", "Normal", "AmbientOcclusion", "OpacityMask", "Specular"],
            graph["connectedPropertyAllowlist"],
        )
        self.assertTrue(graph["allOtherMaterialPropertyInputsDisconnected"])
        self.assertTrue(graph["allExpressionsReachableOnlyFromAuthorizedRoots"])
        self.assertFalse(graph["customMaterialOutputsAllowed"])
        self.assertFalse(graph["worldPositionOffset"])
        self.assertFalse(graph["displacement"])
        self.assertFalse(graph["pixelDepthOffset"])
        for token in (
            "AcceptedOverlayExpressionCount = 26",
            "CandidateResponseExpressionCount = 25",
            "CoreMaskDescription",
            "const int edgeCount = 64;",
            "const float opaqueCollarCm = 5000.000000000;",
            "const float outwardFeatherCm = 800.000000000;",
            "const float ditherCellCm = 25.000000000;",
            "GraphEquivalent(SourceMask->Expression, ClonedMask->Expression)",
            "StripOnlyOldResponseGraph",
            "CopyExactResponseGraph",
            "MP_BaseColor, MP_Roughness, MP_Normal, MP_AmbientOcclusion",
            "MP_WorldPositionOffset",
            "MP_PixelDepthOffset",
            "MP_Displacement",
        ):
            self.assertIn(token, self.editor_source)
        input_comparator = self.editor_source.split(
            "bool InputAndGraphEquivalent(", 1
        )[1].split("bool HasOnlyAuthorizedFinalPropertyConnections", 1)[0]
        for token in (
            "Source->OutputIndex == Candidate->OutputIndex",
            "Source->Mask == Candidate->Mask",
            "Source->MaskR == Candidate->MaskR",
            "Source->MaskG == Candidate->MaskG",
            "Source->MaskB == Candidate->MaskB",
            "Source->MaskA == Candidate->MaskA",
            "GraphEquivalent(Source->Expression, Candidate->Expression)",
        ):
            self.assertIn(token, input_comparator)
        property_allowlist = self.editor_source.split(
            "bool HasOnlyAuthorizedFinalPropertyConnections", 1
        )[1].split("bool AllFinalExpressionsBelongToAuthorizedRootClosures", 1)[0]
        self.assertIn("Index < MP_MAX", property_allowlist)
        self.assertIn("GetExpressionInputForProperty(Property)", property_allowlist)
        for property_name in (
            "MP_BaseColor",
            "MP_Roughness",
            "MP_Normal",
            "MP_AmbientOcclusion",
            "MP_OpacityMask",
            "MP_Specular",
        ):
            self.assertEqual(1, property_allowlist.count(property_name), property_name)
        final_validator = self.editor_source.split(
            "bool ValidateFinalMaterial(", 1
        )[1].split("bool WriteExactMetadata", 1)[0]
        self.assertIn("InputAndGraphEquivalent(SourceMask, CandidateMask)", final_validator)
        self.assertIn(
            "InputAndGraphEquivalent(SourceSpecular, CandidateSpecular)",
            final_validator,
        )
        self.assertIn("HasOnlyAuthorizedFinalPropertyConnections(Candidate)", final_validator)
        self.assertIn("AllFinalExpressionsBelongToAuthorizedRootClosures", final_validator)
        self.assertNotIn("Candidate->IsPropertyConnected(", final_validator)

    def test_existing_grass001_dual_phase_assets_are_reused_not_modified(self):
        response = self.contract["grass001Response"]
        self.assertEqual(1.4, response["tileMetres"])
        self.assertEqual(90.0, response["phaseRotationDegrees"])
        self.assertEqual(7.0, response["phaseBlendMetres"])
        self.assertEqual(3.5, response["mesoMetres"])
        self.assertEqual([8.4, 18.2], response["macroMetres"])
        self.assertEqual([20.0, 35.0, 50.0], response["normalReviewMetres"])
        self.assertEqual([50.0, 70.0], response["sourceResponseFadeMetres"])
        self.assertEqual(5, response["exactTextureCount"])
        self.assertTrue(response["heightRetainedWithoutDisplacement"])
        self.assertTrue(response["cleanPersistedSourcePackageRequired"])
        self.assertEqual(-1, response["sourceMaterialPackageBytes"])
        self.assertEqual(
            "UNSET_GRADED_TURF_ACCEPTED_ORDINARY_GRASS_PACKAGE_SHA256_REQUIRES_REVIEWED_SOURCE_CHANGE",
            response["sourceMaterialPackageSha256"],
        )
        self.assertFalse(response["sourceMaterialPackagePinPopulated"])
        self.assertTrue(response["packageAndIndependentGraphAdmissionPrecedesCopy"])
        self.assertTrue(response["exactStaticTwentyFiveNodePayloadAndTopologyValidator"])
        self.assertFalse(response["liveTwentyFiveNodeGraphAloneSufficient"])
        self.assertTrue(
            response["futureReviewedSourceChangeMustPinAcceptedColdReloadedUpstreamPackage"]
        )
        self.assertFalse(response["sourceMaterialModified"])
        self.assertIn("DuplicateMaterialExpression", self.editor_source)
        self.assertIn("Remap.FindRef(SourceInput->Expression)", self.editor_source)
        source_validator = self.editor_source.split(
            "bool ValidateExactOrdinaryGrassSurface(", 1
        )[1].split("bool CopyExactResponseGraph", 1)[0]
        for token in (
            "OrdinaryGrassMaterialPackageBytes < 2",
            "!IsSha256(OrdinaryGrassMaterialPackageSha256)",
            "OutMaterial->GetOutermost()->IsDirty()",
            "OrdinaryGrassMaterialPackagePath",
            "FPackageName::DoesPackageExist(",
            "LoadPinnedBytes(",
            "ValidatePinnedOrdinaryGrassGraph(",
        ):
            self.assertIn(token, source_validator)
        self.assertLess(
            source_validator.index("OrdinaryGrassMaterialPackageBytes < 2"),
            source_validator.index("OutMaterial = LoadExact<UMaterial>"),
        )
        self.assertLess(
            source_validator.index("LoadPinnedBytes("),
            source_validator.index("ValidatePinnedOrdinaryGrassGraph("),
        )
        independent_graph = self.editor_source.split(
            "bool ValidatePinnedOrdinaryGrassGraph(", 1
        )[1].split("bool ValidateExactOrdinaryGrassSurface", 1)[0]
        for token in (
            "ODGrass.AbsoluteWorldPositionNoOffsets",
            "WPT_ExcludeAllShaderOffsets",
            "ODGrass.ProviderPrimaryUv",
            "ODGrass.ProviderPhaseUv",
            "ODGrass.PhaseBlendMask",
            "ODGrass.ColorRoughnessResponse",
            "ODGrass.NormalResponse",
            "ODGrass.AmbientOcclusionResponse",
            "ODGrass.BaseColorOutput",
            "ODGrass.RoughnessOutput",
            "CandidateResponseExpressionCount",
            "Material->EditorParameters.Num() != SourceTextureCount * 2",
        ):
            self.assertIn(token, independent_graph)

    def test_r29_r32_ownership_and_zero_to_95m_coverage_are_literal(self):
        ownership = self.contract["placementAndDistanceBoundary"]
        self.assertEqual(6144, ownership["r29OwnedPlacements"])
        self.assertEqual(4608, ownership["r32OwnedPlacements"])
        self.assertEqual(12, ownership["r29OwnedHismBuckets"])
        self.assertEqual(12, ownership["r32OwnedHismBuckets"])
        self.assertEqual([65.0, 90.0], ownership["modeledBladeFadeCullMetres"])
        self.assertEqual([0.0, 95.0], ownership["presentationProofMetres"])
        self.assertTrue(ownership["surfacePersistsBeyondBladeCull"])
        self.assertFalse(ownership["placementOwnerOrTransformChanged"])
        for token in (
            "R29OwnedPlacementCount = 6144",
            "R32OwnedPlacementCount = 4608",
            "PresentationProofEndDistanceCm = 9500",
            "BladeFadeStartDistanceCm = 6500",
            "BladeFadeEndDistanceCm = 9000",
            "SurfaceResponseFadeStartDistanceCm = 5000",
            "SurfaceResponseFadeEndDistanceCm = 7000",
            "UniqueComponents.Num() != 24",
            "R29->SavedLayout.GrassTotal()",
            "R32->SavedLayout.Total()",
        ):
            self.assertIn(token, self.runtime_source)

    def test_runtime_exact_snapshot_apply_restore_is_private_and_disabled(self):
        runtime = self.contract["runtimeBoundary"]
        self.assertEqual("V5DGroundMacroVariationOverlay", runtime["exactComponentName"])
        self.assertFalse(runtime["selectionCompiledAuthorized"])
        self.assertFalse(runtime["activationCompiledAuthorized"])
        self.assertFalse(runtime["productionCallSitePresent"])
        self.assertTrue(runtime["completeOverrideArraySnapshot"])
        self.assertTrue(runtime["exactFallbackRestore"])
        self.assertEqual("SLOT_ZERO_MATERIAL_OVERRIDE_ONLY", runtime["prospectiveMutation"])
        private = self.runtime_header.split("private:", 1)[1]
        for name in (
            "SnapshotGroundMacroVariationOverlayInternal",
            "ApplyGradedTurfMaterialInternal",
            "RestoreGroundMacroVariationOverlayInternal",
            "ValidateSnapshotBoundaryInternal",
        ):
            self.assertIn(name, private)
        for token in (
            "constexpr bool bRuntimeCandidateSelectionCompiledAuthorized = false",
            "constexpr bool bRuntimeCandidateActivationCompiledAuthorized = false",
            "if (!bRuntimeCandidateSelectionCompiledAuthorized)",
            "if (!bRuntimeCandidateActivationCompiledAuthorized)",
            "Component->SetMaterial(0, GradedTurfCandidate)",
            "Component->EmptyOverrideMaterials()",
            "GroundOverlaySnapshot.OverrideMaterials",
            "EXACT_V5D_GROUND_MACRO_VARIATION_OVERLAY_FALLBACK_RESTORED",
        ):
            self.assertIn(token, self.runtime_source)
        self.assertNotIn("UFUNCTION", private)

    def test_editor_materializer_is_private_unreflected_and_unreachable(self):
        trust = self.contract["editorTrustBoundary"]
        self.assertTrue(trust["inspectionBlueprintCallable"])
        self.assertFalse(trust["inspectionAuthorizesExecution"])
        self.assertEqual("PRIVATE_CPP_ONLY", trust["materializerVisibility"])
        self.assertFalse(trust["materializerHasCallSite"])
        self.assertFalse(trust["callerHashesAloneSufficient"])
        self.assertFalse(trust["writePathReachable"])
        self.assertEqual(1, self.editor_header.count("UFUNCTION(BlueprintCallable"))
        public, private = self.editor_header.split("private:", 1)
        self.assertIn("InspectGradedTurfPresentationReceipts", public)
        self.assertNotIn("MaterializeTrusted", public)
        self.assertIn("MaterializeTrustedGradedTurfPresentationInternal", private)
        self.assertEqual(
            1,
            self.editor_source.count(
                "MaterializeTrustedGradedTurfPresentationInternal("
            ),
        )
        for token in (
            "UNSET_GRADED_TURF_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "UNSET_GRADED_TURF_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "!IsSha256(TrustedAcceptedR33ReceiptSha256)",
            "!IsSha256(TrustedFutureAuthorizationSha256)",
            "caller-supplied paths and hashes cannot authorize materialization",
            "Root->Values.Num() != UE_ARRAY_COUNT(ExactFields)",
            "FPaths::IsSamePath(FullR33, FullAuthorization)",
        ):
            self.assertIn(token, self.editor_source)

    def test_fresh_namespace_inventory_and_rollback_are_recursive(self):
        boundary = self.contract["namespaceBoundary"]
        self.assertTrue(boundary["freshOnly"])
        self.assertEqual(1, boundary["exactOutputAssetCount"])
        self.assertEqual(
            ["ASSET_REGISTRY", "LIVE_OBJECTS", "LOADED_PACKAGES", "PHYSICAL_FILES", "PHYSICAL_DIRECTORIES"],
            boundary["inventoryLayers"],
        )
        self.assertTrue(boundary["liveObjectInventoryTopLevelAssetsOnly"])
        for token in (
            "GetAssetsByPath",
            "TObjectIterator<UObject>",
            "TObjectIterator<UPackage>",
            "FindFilesRecursive",
            "DeleteLoadedAssets",
            "DeleteDirectory",
            "ROLLBACK_REFUSED",
            "ROLLBACK_FAILED",
            "ROLLBACK_COMPLETE",
            "physicalRootAbsent=true",
        ):
            self.assertIn(token, self.editor_source)
        self.assertGreaterEqual(self.editor_source.count("FindFilesRecursive"), 2)
        collector = self.editor_source.split("void CollectNamespaceState(", 1)[1].split(
            "bool NamespaceIsEmpty", 1
        )[0]
        self.assertIn("!Object->IsAsset()", collector)
        self.assertEqual(1, collector.count("Object->IsAsset()"))

    def test_no_map_or_environment_mutation_exists_in_editor_scaffold(self):
        forbidden = (
            "SpawnActor",
            "LoadMap(",
            "OpenLevel(",
            "GetEditorWorld(",
            "SetActorLocation",
            "SetActorRotation",
            "SetActorTransform",
            "SetWorldTransform",
            "SetRelativeTransform",
            "SetMaterial(",
            "SetStaticMesh(",
            "SetCollision",
            "SetCanEverAffectNavigation",
            "AddInstance(",
            "RemoveInstance(",
            "DestroyActor",
        )
        for token in forbidden:
            self.assertNotIn(token, self.editor_source, token)
        self.assertNotRegex(
            self.editor_source,
            r"R(?:3[4-9]|[4-9][0-9])(?:Transaction|Capture|Successor)",
        )

    def test_preservation_boundary_is_all_false(self):
        self.assertTrue(
            all(value is False for value in self.contract["preservationBoundary"].values())
        )
        for phrase in (
            "appearance-only",
            "no native project was opened",
            "64-point irregular estate boundary",
            "r29 remains the exclusive owner of its exact 6,144 placements",
            "r32 remains the owner of its exact 4,608 placements",
            "caller-provided paths or hashes cannot replace those anchors",
            "remaining proof is explicit",
        ):
            self.assertIn(phrase, self.readme.lower())

    def test_api_audit_is_static_and_exact_on_installed_host(self):
        self.assertEqual(
            "STATIC_INSTALLED_HEADER_AUDIT_ONLY_NOT_COMPILED_OR_EXECUTED",
            self.audit["status"],
        )
        self.assertFalse(self.audit["launchOrBuildPerformed"])
        self.assertFalse(any(self.audit["nativeClaims"].values()))
        engine_source = Path("C:/Program Files/Epic Games/UE_5.5/Engine/Source")
        if not engine_source.is_dir():
            self.skipTest("Pinned UE 5.5 source headers are not installed")
        for row in self.audit["headers"]:
            path = engine_source / row["pathBelowEngineSource"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(row["bytes"], path.stat().st_size)
            self.assertEqual(row["sha256"], sha256(path))

    def test_artifact_roster_and_hashes_are_exact(self):
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
                Path(__file__).resolve(),
            )
        }
        rows = self.contract["implementation"]["artifacts"]
        self.assertEqual(expected, {row["file"] for row in rows})
        for row in rows:
            path = REPO_ROOT / row["file"]
            self.assertEqual(row["bytes"], path.stat().st_size)
            self.assertEqual(row["sha256"], sha256(path))

    def test_cpp_scaffolding_balanced_and_no_temporary_artifacts(self):
        for path, source in (
            (RUNTIME_SOURCE, self.runtime_source),
            (RUNTIME_TEST, self.runtime_test),
            (EDITOR_SOURCE, self.editor_source),
        ):
            self.assertEqual(source.count("{"), source.count("}"), path)
            self.assertEqual(source.count("#if"), source.count("#endif"), path)
        leftovers = [
            path
            for path in INTEGRATION_ROOT.rglob("*")
            if path.name == "__pycache__" or path.name.endswith((".tmp", ".part"))
        ]
        bytecode = PLUGIN_ROOT / "Tests/__pycache__"
        if bytecode.is_dir():
            leftovers.extend(bytecode.glob(f"{Path(__file__).stem}.*.pyc"))
        self.assertEqual([], leftovers)

    def test_remaining_native_proof_is_not_silently_claimed(self):
        expected = {
            "UE55_UBT_UHT_COMPILE_AND_LINK",
            "REVIEWED_DISTINCT_RECEIPT_PINS_RECOMPILE_AND_EXPLICIT_INVOCATION",
            "ACCEPTED_COLD_RELOADED_ORDINARY_GRASS_SOURCE_PACKAGE_BYTE_SHA256_PIN",
            "EXACT_ONE_MATERIAL_CREATE_SAVE_UNLOAD_COLD_RELOAD",
            "INJECTED_FAILURE_SLOT_ZERO_APPLY_AND_EXACT_FALLBACK_RESTORE",
            "MATCHED_MAIN_PASS_AND_RGB_0_12_20_35_50_65_70_90_95_METRE_CAPTURES",
            "NO_BALD_BAND_HARD_CUTOFF_MOIRE_SHIMMER_OR_PROVIDER_SEAM_REGRESSION",
            "R29_6144_AND_R32_4608_COLD_RELOAD_OWNERSHIP_RECENSUS",
            "MAP_GEOGRAPHY_TERRAIN_COLLISION_NAVIGATION_LOS_RF_SENSOR_SIMULATION_REGRESSION_HASHES",
            "TARGET_HARDWARE_MEMORY_FRAME_TIME_DRAW_CALL_AND_SHADER_COST_ACCEPTANCE",
            "EXPLICIT_HUMAN_VISUAL_ACCEPTANCE",
        }
        self.assertEqual(expected, set(self.contract["remainingNativeProof"]))


if __name__ == "__main__":
    unittest.main()
