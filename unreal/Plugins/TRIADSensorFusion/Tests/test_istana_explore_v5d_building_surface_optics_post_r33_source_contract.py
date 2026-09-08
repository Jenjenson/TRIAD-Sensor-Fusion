import hashlib
import json
import re
import unittest
from pathlib import Path


UNREAL_ROOT = Path(__file__).absolute().parents[3]
REPO_ROOT = UNREAL_ROOT.parent
PLUGIN_ROOT = UNREAL_ROOT / "Plugins/TRIADSensorFusion"
CANDIDATE_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
    "BuildingSurfaceOpticsCandidate"
)
INTEGRATION_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
    "BuildingSurfaceOpticsIntegration"
)
CONTRACT_PATH = (
    INTEGRATION_ROOT
    / "building_surface_optics_post_r33.source_contract.v1.json"
)
API_AUDIT_PATH = (
    INTEGRATION_ROOT / "building_surface_optics_ue55_api_audit.source.json"
)
README_PATH = INTEGRATION_ROOT / "README.md"
RUNTIME_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DBuildingSurfaceOpticsLibrary.h"
)
RUNTIME_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DBuildingSurfaceOpticsLibrary.cpp"
)
RUNTIME_TEST = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/Tests/"
    "TRIADIstanaExploreV5DBuildingSurfaceOpticsLibraryTests.cpp"
)
EDITOR_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DBuildingSurfaceOpticsEditorLibrary.h"
)
EDITOR_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DBuildingSurfaceOpticsEditorLibrary.cpp"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def normalized_cpp(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


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


class BuildingSurfaceOpticsPostR33SourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
        cls.audit = json.loads(API_AUDIT_PATH.read_text(encoding="utf-8"))
        cls.candidate_contract = json.loads(
            (
                CANDIDATE_ROOT
                / "building_surface_optics_candidate.contract.json"
            ).read_text(encoding="utf-8")
        )
        cls.candidate_audit = json.loads(
            (
                CANDIDATE_ROOT
                / "OfflineAudit/building_surface_optics_audit.json"
            ).read_text(encoding="utf-8")
        )
        cls.runtime_header = RUNTIME_HEADER.read_text(encoding="utf-8")
        cls.runtime_source = RUNTIME_SOURCE.read_text(encoding="utf-8")
        cls.runtime_test = RUNTIME_TEST.read_text(encoding="utf-8")
        cls.editor_header = EDITOR_HEADER.read_text(encoding="utf-8")
        cls.editor_source = EDITOR_SOURCE.read_text(encoding="utf-8")

    def test_source_only_unnumbered_delivery_has_two_independent_gates(self):
        self.assertEqual(
            "triad.istana_public_view_explore_v5d."
            "building_surface_optics.post_r33_source_integration.v1",
            self.contract["schema"],
        )
        self.assertEqual(
            "SOURCE_SCAFFOLD_NOT_EXECUTED", self.contract["status"]
        )
        sequencing = self.contract["sequencing"]
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
        self.assertFalse(sequencing["numberedWrappersModified"])
        self.assertFalse(sequencing["nativeProjectWritten"])
        self.assertFalse(sequencing["unrealLaunchedOrBuilt"])

    def test_candidate_closure_is_byte_and_hash_exact(self):
        for row in self.contract["sourcePins"].values():
            path = REPO_ROOT / row["file"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(row["bytes"], path.stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)
        self.assertEqual("SOURCE_ONLY_UNADMITTED", self.candidate_contract["status"])
        self.assertFalse(self.candidate_contract["claimBoundary"]["nativeIntegrated"])
        self.assertFalse(
            self.candidate_contract["claimBoundary"]["humanVisualAcceptance"]
        )
        self.assertEqual("PASS_SOURCE_LOOKDEV_ONLY", self.candidate_audit["status"])
        self.assertFalse(self.candidate_audit["nativeIntegrated"])
        self.assertFalse(self.candidate_audit["visualAcceptance"])

    def test_exact_r31_geometry_and_authority_boundary_are_preserved(self):
        geometry = self.contract["geometryInvariant"]
        self.assertEqual(43448, geometry["sourceTriangles"])
        self.assertEqual(17, geometry["materialSlots"])
        self.assertEqual(1391, geometry["canonicalSourceGroups"])
        self.assertEqual(1388, geometry["retainedGroups"])
        self.assertEqual("identity", geometry["componentTransform"])
        for key, value in geometry.items():
            if isinstance(value, bool):
                self.assertFalse(value, key)
        self.assertTrue(
            all(value is False for value in self.contract["preservationBoundary"].values())
        )
        candidate_geometry = self.candidate_audit["geometryEvidence"]
        self.assertEqual(43448, candidate_geometry["sourceTriangleCount"])
        self.assertEqual(17, candidate_geometry["materialSlotCount"])
        self.assertEqual(1388, candidate_geometry["sourceGroupCount"])
        self.assertEqual("identity", candidate_geometry["componentTransform"])

    def test_exact_eighteen_asset_and_seventeen_fallback_mapping(self):
        implementation = self.contract["implementation"]
        self.assertEqual(18, implementation["exactOutputAssetCount"])
        slots = implementation["slotMaterials"]
        self.assertEqual(17, len(slots))
        self.assertEqual(17, len({row[0] for row in slots}))
        self.assertEqual(17, len({row[1] for row in slots}))
        self.assertEqual(
            {"MI_IPV5D_R31_OfficialWall", "MI_IPV5D_R31_OfficialRoof",
             "MI_IPV5D_R31_FallbackWall", "MI_IPV5D_R31_FallbackRoof"},
            {row[2] for row in slots},
        )
        for index, (slot, candidate, fallback) in enumerate(slots):
            self.assertIn(f'TEXT("{slot}")', self.runtime_source)
            self.assertIn(f'TEXT("{candidate}")', self.runtime_source)
            self.assertIn(f"{fallback}.{fallback}", self.runtime_source)
            self.assertIn(f'TEXT("{candidate}")', self.editor_source)
            self.assertIn(f'TEXT("{slot}")', self.editor_source)
            self.assertIn("CandidateMaterialObjectPath", self.runtime_test)

    def test_graph_is_real_clear_coat_logic_not_parameter_only_mids(self):
        master = self.contract["implementation"]["master"]
        self.assertTrue(master["exactR31GraphClone"])
        self.assertEqual(42, master["expressionCountPreserved"])
        self.assertEqual(23, master["r31InputCountPreserved"])
        self.assertEqual(26, master["candidateInputCount"])
        self.assertEqual("MSM_ClearCoat", master["shadingModel"])
        self.assertEqual("MP_CustomData0", master["clearCoatProperty"])
        self.assertEqual("MP_CustomData1", master["clearCoatRoughnessProperty"])
        self.assertTrue(master["r31MasksAndSignalsRemainInsideClonedCustomNode"])
        self.assertFalse(master["fixedIndependentPaneGrid"])
        source = self.editor_source
        for token in (
            "AssetTools.DuplicateAsset(",
            "R31.TexturedWeatheredR25DepthSurface",
            "BSO.ExactR31SurfacePlusCandidateOptics",
            "ComposeCandidateSurfaceCode",
            "float2 R31PaneCellIndex = cellIndex;",
            "R31TangentNormalInput",
            "R31MetallicInput",
            "R31AmbientOcclusionInput",
            "CandidateTangentNormal",
            "CandidateRoughness",
            "CandidateMetallic",
            "CandidateAmbientOcclusion",
            "CandidateClearCoat",
            "CandidateClearCoatRoughness",
            "Material->SetShadingModel(MSM_ClearCoat)",
            "MP_CustomData0",
            "MP_CustomData1",
            "EditorOnly->ClearCoat",
            "EditorOnly->ClearCoatRoughness",
            "Surface->Outputs.Add(FExpressionOutput(TEXT(\"return\")))",
            "Surface->Outputs.Add(FExpressionOutput(Output.OutputName))",
        ):
            self.assertIn(token, source)
        self.assertNotIn("Surface->RebuildOutputs()", source)
        self.assertNotIn("PhysicalMaterialMap.IsEmpty()", source)

    def test_custom_output_order_and_property_edges_are_exact(self):
        expected = [
            "CandidateTangentNormal",
            "CandidateRoughness",
            "CandidateMetallic",
            "CandidateAmbientOcclusion",
            "CandidateClearCoat",
            "CandidateClearCoatRoughness",
        ]
        self.assertEqual(
            expected,
            self.contract["implementation"]["master"]["additionalOutputs"],
        )
        for index, property_name in enumerate(
            (
                "BaseColor",
                "Normal",
                "Roughness",
                "Metallic",
                "AmbientOcclusion",
                "ClearCoat",
                "ClearCoatRoughness",
            )
        ):
            self.assertIn(
                f"InputMatches(EditorOnly->{property_name}, Surface, {index})",
                normalized_cpp(self.editor_source),
            )

    def test_instances_clone_exact_r31_overrides_and_keep_direct_parent(self):
        source = self.editor_source
        for member in self.contract["implementation"]["slotCloneEquality"]:
            if member == "StaticParameters":
                self.assertIn("GetStaticParameters()", source)
            elif member == "ReferencedTextures":
                self.assertIn("SameReferencedTextures", source)
            else:
                self.assertIn(member, source)
        for token in (
            "Candidate->SetParentEditorOnly(CandidateMaster, false)",
            "Instance->Parent != CandidateMaster",
            "Instance->GetShadingModels().HasOnlyShadingModel(MSM_ClearCoat)",
            "ExactR31Fallback",
            "ValidateExactR31FallbackRoster",
            "ResolveOptionalPresentationMaterials",
        ):
            self.assertIn(token, self.editor_source + self.runtime_source)

    def test_authorization_is_literal_narrow_and_separate(self):
        source = self.editor_source
        for token in (
            "ValidateAcceptedR33Receipt",
            "ValidateFutureAuthorization",
            "ExplicitHumanReviewAcceptance\"), true",
            "ExactCoreCaptureCount\"), 8",
            "AcceptedR33ReceiptSha256",
            "ExpectedAcceptedR33ReceiptSha256",
            "ExpectedFutureTransactionAuthorizationSha256",
            "FPaths::IsSamePath(FullR33, FullAuthorization)",
            "ExactMaterialSlotCount\"), SlotCount",
            "ExactOutputAssetCount\"), OutputAssetCount",
            "OptionalPresentationMaterialSetOnly\"), true",
            "ExactR31FallbacksPreserved\"), true",
            "MapMutationAuthorized\"), false",
            "SourceMeshMutationAuthorized\"), false",
            "SourceTransformMutationAuthorized\"), false",
            "NativeWriteOrUnrealLaunchPerformed\"), false",
            "Root->Values.Num() != UE_ARRAY_COUNT(ExactFields)",
            "must contain exactly the narrow trusted field roster",
            "TrustedAcceptedR33ReceiptSha256",
            "TrustedFutureAuthorizationSha256",
            "bRequireCompiledTrustAnchors",
            "Caller-supplied building-surface receipt hashes do not match",
        ):
            self.assertIn(token, source)

    def test_caller_cannot_self_authenticate_materialization(self):
        trust = self.contract["implementation"]["executionTrustBoundary"]
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
        self.assertIn("InspectBuildingSurfaceOpticsReceipts", public_text)
        self.assertNotIn("Materialize", public_text)
        self.assertIn(
            "MaterializeTrustedBuildingSurfaceOpticsAssetsInternal",
            private_text,
        )
        self.assertEqual(1, self.editor_header.count("UFUNCTION(BlueprintCallable"))
        self.assertNotRegex(
            self.editor_header,
            r"UFUNCTION\(BlueprintCallable,[\s\S]{0,180}"
            r"MaterializeTrustedBuildingSurfaceOpticsAssetsInternal",
        )
        self.assertIn(
            "UNSET_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            self.editor_source,
        )
        self.assertIn(
            "UNSET_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            self.editor_source,
        )
        self.assertIn(
            "!IsSha256(TrustedAcceptedR33ReceiptSha256)", self.editor_source
        )
        self.assertIn(
            "!IsSha256(TrustedFutureAuthorizationSha256)", self.editor_source
        )
        self.assertIn(
            "compiled trusted receipt anchors are unset", self.editor_source
        )
        self.assertIn(
            "Root->Values.Num() != UE_ARRAY_COUNT(ExactFields)",
            self.editor_source,
        )

    def test_recursive_namespace_and_rollback_are_fail_closed(self):
        source = self.editor_source
        for token in (
            "CollectNamespaceState",
            "GetAssetsByPath",
            "TObjectIterator<UObject>",
            "TObjectIterator<UPackage>",
            "TryConvertLongPackageNameToFilename",
            "TryConvertFilenameToLongPackageName",
            "FindFilesRecursive",
            "TEXT(\"*\")",
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
        ):
            self.assertIn(token, source)
        self.assertGreaterEqual(source.count("FindFilesRecursive"), 2)
        namespace_contract = self.contract["namespaceAndRollback"]
        self.assertTrue(all(namespace_contract.values()))

    def test_no_map_component_mesh_or_numbered_successor_mutation_surface(self):
        files = {
            RUNTIME_HEADER: self.runtime_header,
            RUNTIME_SOURCE: self.runtime_source,
            RUNTIME_TEST: self.runtime_test,
            EDITOR_HEADER: self.editor_header,
            EDITOR_SOURCE: self.editor_source,
            API_AUDIT_PATH: API_AUDIT_PATH.read_text(encoding="utf-8"),
            README_PATH: README_PATH.read_text(encoding="utf-8"),
        }
        forbidden_stage = "R" + str(33 + 1)
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
            "SetMaterial(",
            "SetStaticMesh(",
            "SetCollision",
            "SetCanEverAffectNavigation",
            "AddInstance(",
            "RemoveInstance(",
            "DestroyActor",
        )
        for path, source in files.items():
            self.assertNotIn(forbidden_stage, source, path)
            for call in forbidden_calls:
                self.assertNotIn(call, source, f"{path}: {call}")
        self.assertEqual(
            1,
            len(
                re.findall(
                    r"UFUNCTION\(BlueprintCallable,[\s\S]{0,180}"
                    r"static bool InspectBuildingSurfaceOpticsReceipts\(",
                    self.editor_header,
                )
            ),
        )
        self.assertIn(
            "static bool MaterializeTrustedBuildingSurfaceOpticsAssetsInternal(",
            self.editor_header,
        )

    def test_installed_ue55_api_audit_is_exact(self):
        self.assertEqual(
            "STATIC_INSTALLED_HEADER_AUDIT_ONLY_NOT_COMPILED_OR_EXECUTED",
            self.audit["status"],
        )
        self.assertFalse(self.audit["launchOrBuildPerformed"])
        self.assertTrue(all(value is False for value in self.audit["nativeClaims"].values()))
        engine_source = Path("C:/Program Files/Epic Games/UE_5.5/Engine/Source")
        if not engine_source.is_dir():
            self.skipTest("Pinned UE 5.5 install is not present on this host")
        for row in self.audit["headersAndSources"]:
            path = engine_source / row["pathBelowEngineSource"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(row["bytes"], path.stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)

        signatures = {
            "Runtime/Engine/Public/Materials/MaterialExpressionCustom.h": (
                "TArray<struct FCustomOutput> AdditionalOutputs;",
                "TEnumAsByte<enum ECustomMaterialOutputType> OutputType",
            ),
            "Runtime/Engine/Private/Materials/MaterialExpressions.cpp": (
                'Outputs.Add(FExpressionOutput(TEXT("return")));',
                "Outputs.Add(FExpressionOutput(CustomOutput.OutputName));",
                "Type = AdditionalOutputs[OutputIndex - 1].OutputType;",
            ),
            "Editor/MaterialEditor/Public/MaterialEditingLibrary.h": (
                "static bool ConnectMaterialProperty(UMaterialExpression* FromExpression, FString FromOutputName, EMaterialProperty Property);",
                "static void RecompileMaterial(UMaterial* Material);",
            ),
            "Runtime/Engine/Public/Materials/Material.h": (
                "ENGINE_API void SetShadingModel(EMaterialShadingModel NewModel);",
                "ENGINE_API bool IsPropertyConnected(EMaterialProperty Property) const;",
                "TObjectPtr<class UPhysicalMaterial> PhysicalMaterialMap[EPhysicalMaterialMaskColor::MAX];",
            ),
            "Runtime/Engine/Public/Materials/MaterialInstance.h": (
                "TObjectPtr<class UPhysicalMaterial> PhysicalMaterialMap[EPhysicalMaterialMaskColor::MAX];",
                "TArray<struct FScalarParameterValue> ScalarParameterValues;",
            ),
            "Runtime/Engine/Public/Materials/MaterialInstanceConstant.h": (
                "ENGINE_API void SetParentEditorOnly(class UMaterialInterface* NewParent, bool RecacheShader = true);",
            ),
            "Developer/AssetTools/Public/IAssetTools.h": (
                "virtual UObject* DuplicateAsset(const FString& AssetName, const FString& PackagePath, UObject* OriginalObject) = 0;",
            ),
            "Editor/UnrealEd/Public/Subsystems/EditorAssetSubsystem.h": (
                "UNREALED_API bool DeleteLoadedAssets(const TArray<UObject*>& AssetsToDelete);",
                "UNREALED_API bool DeleteDirectory(const FString& DirectoryPath);",
                "UNREALED_API bool SaveLoadedAssets(const TArray<UObject*>& AssetsToSave, bool bOnlyIfIsDirty = true);",
            ),
            "Runtime/AssetRegistry/Public/AssetRegistry/IAssetRegistry.h": (
                "virtual bool GetAssetsByPath(FName PackagePath, TArray<FAssetData>& OutAssetData, bool bRecursive = false, bool bIncludeOnlyOnDiskAssets = false) const = 0;",
                "virtual void ScanPathsSynchronous(const TArray<FString>& InPaths, bool bForceRescan = false, bool bIgnoreDenyListScanFilters = false) = 0;",
            ),
        }
        audited = {
            row["pathBelowEngineSource"] for row in self.audit["headersAndSources"]
        }
        self.assertTrue(set(signatures).issubset(audited))
        for relative, expected_signatures in signatures.items():
            text = normalized_cpp((engine_source / relative).read_text(encoding="utf-8"))
            for signature in expected_signatures:
                self.assertIn(normalized_cpp(signature), text, f"{relative}: {signature}")

    def test_numbered_wrapper_source_and_historical_receipt_pins_are_exact(self):
        for row in self.contract["preservedExternalPins"]["numberedWrappers"]:
            path = REPO_ROOT / row["file"]
            self.assertTrue(path.is_file(), path)
            historical = HISTORICAL_WRAPPER_RECEIPT_PINS.get(row["file"])
            if historical is not None:
                self.assertEqual(historical, (row["bytes"], row["sha256"]), path)
                continue
            self.assertEqual(row["bytes"], path.stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)

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
                Path(__file__).absolute(),
            )
        }
        rows = self.contract["implementation"]["artifacts"]
        self.assertEqual(expected, {row["file"] for row in rows})
        for row in rows:
            path = REPO_ROOT / row["file"]
            self.assertEqual(row["bytes"], path.stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)

    def test_scaffolding_balanced_and_remaining_proof_explicit(self):
        for path, source in (
            (RUNTIME_SOURCE, self.runtime_source),
            (RUNTIME_TEST, self.runtime_test),
            (EDITOR_SOURCE, self.editor_source),
        ):
            self.assertEqual(source.count("{"), source.count("}"), path)
            self.assertEqual(source.count("#if"), source.count("#endif"), path)
        remaining = set(self.contract["remainingNativeProof"])
        self.assertIn("UE55_UBT_UHT_COMPILE_AND_LINK", remaining)
        self.assertIn("CUSTOM_HLSL_CLEAR_COAT_SHADER_COMPILE", remaining)
        self.assertIn("EXPLICIT_HUMAN_VISUAL_ACCEPTANCE", remaining)
        self.assertGreaterEqual(len(remaining), 12)


if __name__ == "__main__":
    unittest.main()
