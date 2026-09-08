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
    "BuildingFootContactCandidate"
)
INTEGRATION_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
    "BuildingFootContactIntegration"
)
CONTRACT_PATH = (
    INTEGRATION_ROOT
    / "building_foot_contact_post_r33.source_contract.v1.json"
)
API_AUDIT_PATH = (
    INTEGRATION_ROOT / "building_foot_contact_ue55_api_audit.source.json"
)
README_PATH = INTEGRATION_ROOT / "README.md"
RUNTIME_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DBuildingFootContactLibrary.h"
)
RUNTIME_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DBuildingFootContactLibrary.cpp"
)
RUNTIME_TEST = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/Tests/"
    "TRIADIstanaExploreV5DBuildingFootContactLibraryTests.cpp"
)
EDITOR_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DBuildingFootContactEditorLibrary.h"
)
EDITOR_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DBuildingFootContactEditorLibrary.cpp"
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


class BuildingFootContactPostR33SourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = load_strict_json(CONTRACT_PATH)
        cls.audit = load_strict_json(API_AUDIT_PATH)
        cls.candidate_contract = load_strict_json(
            CANDIDATE_ROOT
            / "building_foot_contact_candidate.contract.v1.json"
        )
        cls.candidate_audit = load_strict_json(
            CANDIDATE_ROOT
            / "Generated/building_foot_contact_source_audit.v1.json"
        )
        cls.runtime_header = RUNTIME_HEADER.read_text(encoding="utf-8")
        cls.runtime_source = RUNTIME_SOURCE.read_text(encoding="utf-8")
        cls.runtime_test = RUNTIME_TEST.read_text(encoding="utf-8")
        cls.editor_header = EDITOR_HEADER.read_text(encoding="utf-8")
        cls.editor_source = EDITOR_SOURCE.read_text(encoding="utf-8")

    def test_source_only_delivery_has_two_independent_unset_gates(self):
        self.assertEqual(
            "triad.istana_public_view_explore_v5d."
            "building_foot_contact.post_r33_source_integration.v1",
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

    def test_candidate_closure_is_exact_and_strict_json(self):
        for row in self.contract["sourcePins"].values():
            path = REPO_ROOT / row["file"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(row["bytes"], path.stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)
        for key in (
            "candidateContract",
            "materialIntent",
            "generatedAudit",
            "builder",
        ):
            row = self.contract["sourcePins"][key]
            relative = (
                (REPO_ROOT / row["file"])
                .relative_to(CANDIDATE_ROOT)
                .as_posix()
            )
            self.assertIn(f'TEXT("{relative}")', self.editor_source)
            self.assertIn(row["sha256"], self.editor_source)
        self.assertIn(
            self.contract["sourcePins"]["r31Contract"]["sha256"],
            self.editor_source,
        )
        self.assertEqual(
            "POST_R33_UNNUMBERED_UNADMITTED_SOURCE_ONLY_CANDIDATE",
            self.candidate_contract["status"],
        )
        self.assertTrue(self.candidate_contract["sourceOnly"])
        self.assertFalse(self.candidate_contract["nativeProjectApplied"])
        self.assertFalse(self.candidate_contract["targetMapMutated"])
        self.assertFalse(self.candidate_contract["unrealLaunched"])
        self.assertFalse(self.candidate_audit["nativeProjectApplied"])
        self.assertFalse(self.candidate_audit["visualAcceptance"])

    def test_geometry_response_and_authority_are_exact(self):
        geometry = self.contract["geometryInvariant"]
        self.assertEqual(24522, geometry["sourceVertices"])
        self.assertEqual(130632, geometry["sourceUvRows"])
        self.assertEqual(43448, geometry["sourceTriangles"])
        self.assertEqual(24360, geometry["wallTriangles"])
        self.assertEqual(19088, geometry["roofOrHiddenBottomTriangles"])
        self.assertEqual(17, geometry["materialSlots"])
        self.assertEqual(1388, geometry["retainedGroups"])
        self.assertEqual(1370, geometry["groundPlaneGroups"])
        self.assertEqual(18, geometry["elevatedGroupsExcluded"])
        self.assertEqual("identity", geometry["componentTransform"])
        self.assertTrue(geometry["wallUvVEqualsSourceAbsoluteZExactly"])
        response = self.contract["materialResponse"]
        self.assertEqual(0.1, response["fullWeightThroughSourceZMetres"])
        self.assertEqual(1.25, response["zeroWeightAtAndAboveSourceZMetres"])
        self.assertEqual(0.08, response["maximumBaseColorDarkening"])
        self.assertEqual(0.05, response["maximumRoughnessDelta"])
        self.assertEqual(0.07, response["maximumAoDarkening"])
        self.assertTrue(response["existingGlassMaskExcludesGlazing"])
        self.assertTrue(response["continuousWorldXyBreakup"])
        self.assertTrue(
            all(
                value is False
                for value in self.contract["preservationBoundary"].values()
            )
        )

    def test_exact_eighteen_assets_and_ordered_r31_fallbacks(self):
        implementation = self.contract["implementation"]
        self.assertEqual(18, implementation["exactOutputAssetCount"])
        slots = implementation["slotMaterials"]
        self.assertEqual(17, len(slots))
        self.assertEqual(17, len({row[0] for row in slots}))
        self.assertEqual(17, len({row[1] for row in slots}))
        self.assertEqual(
            {
                "MI_IPV5D_R31_OfficialWall",
                "MI_IPV5D_R31_OfficialRoof",
                "MI_IPV5D_R31_FallbackWall",
                "MI_IPV5D_R31_FallbackRoof",
            },
            {row[2] for row in slots},
        )
        for slot, candidate, fallback in slots:
            self.assertIn(f'TEXT("{slot}")', self.runtime_source)
            self.assertIn(f'TEXT("{candidate}")', self.runtime_source)
            self.assertIn(f"{fallback}.{fallback}", self.runtime_source)
            self.assertIn(f'TEXT("{candidate}")', self.editor_source)
            self.assertIn(f'TEXT("{slot}")', self.editor_source)
        self.assertIn("CandidateMaterialObjectPath", self.runtime_test)
        self.assertIn("ExistingR31FallbackObjectPath", self.runtime_test)

    def test_graph_splice_is_minimal_default_lit_contact_logic(self):
        master = self.contract["implementation"]["master"]
        self.assertTrue(master["exactR31GraphClone"])
        self.assertEqual(42, master["expressionCountPreserved"])
        self.assertEqual(23, master["r31InputCountPreserved"])
        self.assertEqual(24, master["candidateInputCount"])
        self.assertEqual("CMOT_Float4", master["primaryOutputType"])
        self.assertEqual(
            ["ContactAmbientOcclusionOutput"], master["additionalOutputs"]
        )
        self.assertEqual("MSM_DefaultLit", master["shadingModel"])
        self.assertEqual("MP_AmbientOcclusion", master["onlyReroutedProperty"])
        self.assertFalse(master["clearCoatIntroduced"])
        source = self.editor_source
        for token in (
            "AssetTools.DuplicateAsset(",
            "R31.TexturedWeatheredR25DepthSurface",
            "BFC.ExactR31SurfacePlusSourcePlaneContact",
            "ComposeCandidateSurfaceCode",
            "float ExistingBaseColor = finalColor;".replace("float ", "float3 "),
            "float ExistingRoughness = saturate(finalRoughness);",
            "float ExistingAmbientOcclusion = saturate(R31AmbientOcclusionInput);",
            "float ExistingGlassMask = visibleGlass;",
            "float R31WallVerticalWeatherMask = wallMask;",
            "float SourceAbsoluteZMetres = sourceAbsoluteZMetres;",
            "float2 AbsoluteWorldPositionMetresXY = WorldPositionCm.xy * 0.01;",
            "ContactAmbientOcclusionOutput = ContactAmbientOcclusion;",
            "return float4(ContactBaseColor, ContactRoughness);",
            "Surface->OutputType = CMOT_Float4;",
            "R31AmbientOcclusionInput",
            "ContactAmbientOcclusionOutput",
            "MP_AmbientOcclusion",
            "Surface->Outputs.Add(FExpressionOutput(TEXT(\"return\")))",
            "Surface->Outputs.Add(FExpressionOutput(Output.OutputName))",
        ):
            self.assertIn(token, source)
        self.assertNotIn("Surface->RebuildOutputs()", source)
        self.assertNotIn("Material->SetShadingModel", source)
        self.assertNotIn("MSM_ClearCoat", source)
        self.assertNotIn("EditorOnly->ClearCoat", source)
        self.assertNotIn("PhysicalMaterialMap.IsEmpty()", source)

    def test_inherited_r31_edges_and_instance_clones_are_exact(self):
        source = normalized_cpp(self.editor_source)
        for edge in (
            "InputMatches(EditorOnly->BaseColor, Surface, 0)",
            "InputMatches(EditorOnly->Normal, Normal, 0)",
            "InputMatches(EditorOnly->Roughness, RoughnessOutput, 0)",
            "InputMatches(RoughnessOutput->Input, Surface, 0)",
            "InputMatches(EditorOnly->Metallic, Metallic, 0)",
            "InputMatches(EditorOnly->AmbientOcclusion, Surface, 1)",
            "InputMatches(EditorOnly->Specular, Specular, 0)",
        ):
            self.assertIn(normalized_cpp(edge), source)
        for member in self.contract["implementation"]["slotCloneEquality"]:
            if member == "StaticParameters":
                self.assertIn("GetStaticParameters()", self.editor_source)
            elif member == "ReferencedTextures":
                self.assertIn("SameReferencedTextures", self.editor_source)
            else:
                self.assertIn(member, self.editor_source)
        for token in (
            "Candidate->SetParentEditorOnly(CandidateMaster, false)",
            "Instance->Parent != CandidateMaster",
            "MSM_DefaultLit",
            "ExactR31Fallback",
            "ValidateExactR31FallbackRoster",
            "ResolveOptionalPresentationMaterials",
        ):
            self.assertIn(token, self.editor_source + self.runtime_source)

    def test_authorization_is_narrow_literal_and_unreachable(self):
        source = self.editor_source
        for token in (
            "ValidateAcceptedR33Receipt",
            "ValidateFutureAuthorization",
            'ExplicitHumanReviewAcceptance"), true',
            'ExactCoreCaptureCount"), 8',
            "ExpectedAcceptedR33ReceiptSha256",
            "ExpectedFutureTransactionAuthorizationSha256",
            "FPaths::IsSamePath(FullR33, FullAuthorization)",
            'ExactMaterialSlotCount"), SlotCount',
            'ExactOutputAssetCount"), OutputAssetCount',
            'OptionalPresentationMaterialSetOnly"), true',
            'ExactR31FallbacksPreserved"), true',
            'MapMutationAuthorized"), false',
            'SourceMeshMutationAuthorized"), false',
            'SourceTransformMutationAuthorized"), false',
            'NativeWriteOrUnrealLaunchPerformed"), false',
            "Root->Values.Num() != UE_ARRAY_COUNT(ExactFields)",
            "must contain exactly the narrow trusted field roster",
            "TrustedAcceptedR33ReceiptSha256",
            "TrustedFutureAuthorizationSha256",
            "bRequireCompiledTrustAnchors",
            "Caller-supplied building-foot-contact receipt hashes do not match",
        ):
            self.assertIn(token, source)
        trust = self.contract["implementation"]["executionTrustBoundary"]
        self.assertTrue(trust["receiptInspectionBlueprintCallable"])
        self.assertFalse(trust["receiptInspectionAuthorizesExecution"])
        self.assertEqual("PRIVATE_CPP_ONLY", trust["materializerVisibility"])
        self.assertFalse(trust["compiledTrustAnchorsAreValidSha256"])
        self.assertFalse(trust["callerSuppliedHashesAloneSufficient"])
        self.assertFalse(trust["nativeWriteReachable"])
        self.assertFalse(trust["runtimeCandidateSelectionCompiledAuthorized"])
        self.assertFalse(trust["runtimeCandidateSelectionReachable"])
        self.assertNotEqual(
            trust["acceptedR33CompiledTrustAnchor"],
            trust["futureAuthorizationCompiledTrustAnchor"],
        )
        for anchor in (
            "UNSET_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "UNSET_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
        ):
            self.assertIn(anchor, source)
            self.assertFalse(re.fullmatch(r"[0-9A-F]{64}", anchor))
        self.assertIn("private:", self.editor_header)
        public_text, private_text = self.editor_header.split("private:", 1)
        self.assertIn("InspectBuildingFootContactReceipts", public_text)
        self.assertNotIn("Materialize", public_text)
        self.assertIn(
            "MaterializeTrustedBuildingFootContactAssetsInternal",
            private_text,
        )
        self.assertEqual(1, self.editor_header.count("UFUNCTION(BlueprintCallable"))
        self.assertIn(
            "bRuntimeCandidateSelectionCompiledAuthorized = false",
            self.runtime_source,
        )
        self.assertIn(
            "stronger runtime provenance validator",
            self.runtime_source,
        )

    def test_private_materializer_has_no_production_call_site(self):
        symbol = "MaterializeTrustedBuildingFootContactAssetsInternal"
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

    def test_recursive_namespace_and_rollback_fail_closed(self):
        source = self.editor_source
        for token in (
            "CollectNamespaceState",
            "GetAssetsByPath",
            "TObjectIterator<UObject>",
            "TObjectIterator<UPackage>",
            "TryConvertLongPackageNameToFilename",
            "TryConvertFilenameToLongPackageName",
            "FindFilesRecursive",
            'TEXT("*")',
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
        self.assertTrue(all(self.contract["namespaceAndRollback"].values()))
        self.assertIn(
            "ISTANA_BUILDING_FOOT_CONTACT_EXISTING_NAMESPACE_REUSE_DENIED",
            source,
        )
        self.assertNotIn(
            "ISTANA_BUILDING_FOOT_CONTACT_ASSETS_VALID existing=18",
            source,
        )

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
        self.assertFalse(
            self.contract["implementation"]["combinationBoundary"][
                "silentlyCumulativeWithBuildingSurfaceOptics"
            ]
        )

    def test_installed_ue55_api_audit_is_exact_when_present(self):
        self.assertEqual(
            "STATIC_INSTALLED_HEADER_AUDIT_ONLY_NOT_COMPILED_OR_EXECUTED",
            self.audit["status"],
        )
        self.assertFalse(self.audit["launchOrBuildPerformed"])
        self.assertTrue(
            all(value is False for value in self.audit["nativeClaims"].values())
        )
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
            ),
            "Editor/MaterialEditor/Public/MaterialEditingLibrary.h": (
                "static bool ConnectMaterialProperty(UMaterialExpression* FromExpression, FString FromOutputName, EMaterialProperty Property);",
                "static void RecompileMaterial(UMaterial* Material);",
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
        }
        audited = {
            row["pathBelowEngineSource"]
            for row in self.audit["headersAndSources"]
        }
        self.assertTrue(set(signatures).issubset(audited))
        for relative, expected_signatures in signatures.items():
            text = normalized_cpp(
                (engine_source / relative).read_text(encoding="utf-8")
            )
            for signature in expected_signatures:
                self.assertIn(normalized_cpp(signature), text, relative)

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
        self.assertIn("CUSTOM_HLSL_DEFAULT_LIT_SHADER_COMPILE", remaining)
        self.assertIn("VISIBLE_FOOT_SOURCE_PLANE_ALIGNMENT_REVIEW", remaining)
        self.assertIn("EXPLICIT_HUMAN_VISUAL_ACCEPTANCE", remaining)
        self.assertGreaterEqual(len(remaining), 12)


if __name__ == "__main__":
    unittest.main()
