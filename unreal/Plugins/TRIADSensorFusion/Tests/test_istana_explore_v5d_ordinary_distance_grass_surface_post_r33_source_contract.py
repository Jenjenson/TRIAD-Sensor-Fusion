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
    / "SourceAssets/IstanaPublicViewExploreV5D/Vegetation/"
    "OrdinaryDistanceGrassSurfaceCandidate"
)
INTEGRATION_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/Vegetation/GrassSurfaceIntegration"
)
CONTRACT_PATH = (
    INTEGRATION_ROOT
    / "grass_surface_post_r33.source_contract.v1.json"
)
API_AUDIT_PATH = (
    INTEGRATION_ROOT / "grass_surface_ue55_api_audit.source.json"
)
README_PATH = INTEGRATION_ROOT / "README.md"
RUNTIME_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary.h"
)
RUNTIME_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibrary.cpp"
)
RUNTIME_TEST = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/Tests/"
    "TRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceLibraryTests.cpp"
)
EDITOR_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceEditorLibrary.h"
)
EDITOR_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DOrdinaryDistanceGrassSurfaceEditorLibrary.cpp"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def md5(path: Path) -> str:
    digest = hashlib.md5(usedforsecurity=False)
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def aggregate_tree(root: Path) -> dict[str, object]:
    files = sorted(path for path in root.rglob("*") if path.is_file())
    digest = hashlib.sha256()
    total = 0
    for path in files:
        relative = path.relative_to(REPO_ROOT).as_posix()
        size = path.stat().st_size
        file_hash = sha256(path)
        digest.update(
            relative.encode("utf-8")
            + b"\0"
            + str(size).encode("ascii")
            + b"\0"
            + file_hash.encode("ascii")
            + b"\n"
        )
        total += size
    return {
        "fileCount": len(files),
        "bytes": total,
        "aggregateSha256": digest.hexdigest().upper(),
    }


def normalized_cpp(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip()


class OrdinaryDistanceGrassPostR33SourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
        cls.audit = json.loads(API_AUDIT_PATH.read_text(encoding="utf-8"))
        cls.runtime_header = RUNTIME_HEADER.read_text(encoding="utf-8")
        cls.runtime_source = RUNTIME_SOURCE.read_text(encoding="utf-8")
        cls.runtime_test = RUNTIME_TEST.read_text(encoding="utf-8")
        cls.editor_header = EDITOR_HEADER.read_text(encoding="utf-8")
        cls.editor_source = EDITOR_SOURCE.read_text(encoding="utf-8")
        cls.readme = README_PATH.read_text(encoding="utf-8")

    def test_source_only_unnumbered_delivery_and_two_independent_gates(self):
        self.assertEqual(
            "triad.istana_public_view_explore_v5d."
            "ordinary_distance_grass_surface.post_r33_source_integration.v1",
            self.contract["schema"],
        )
        self.assertEqual("SOURCE_SCAFFOLD_NOT_EXECUTED", self.contract["status"])
        sequencing = self.contract["sequencing"]
        self.assertTrue(sequencing["acceptedR33ReceiptRequired"])
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
        self.assertFalse(sequencing["numberedWrappersInvoked"])
        state = dict(self.contract["deliveryState"])
        self.assertTrue(state.pop("sourceImplemented"))
        self.assertTrue(all(value is False for value in state.values()))
        self.assertTrue(
            all(value is False for value in self.contract["nativeClaims"].values())
        )

        gates = self.contract["executionGates"]
        predecessor = gates["acceptedPredecessorReceipt"]
        future = gates["futureExplicitTransaction"]
        self.assertEqual(
            "triad.istana_explore_v5d.r33_player0_capture.v1",
            predecessor["schema"],
        )
        self.assertEqual("COMMITTED", predecessor["status"])
        self.assertTrue(predecessor["humanVisualReviewAccepted"])
        self.assertFalse(predecessor["mapModifiedByCapture"])
        self.assertFalse(
            predecessor["simulationCollisionNavigationSensorRfModified"]
        )
        self.assertTrue(predecessor["callerSha256PinRequired"])
        self.assertFalse(predecessor["callerSha256PinAuthorizesExecution"])
        self.assertFalse(predecessor["compiledTrustAnchorPopulated"])
        self.assertEqual(
            "triad.istana_public_view_explore_v5d."
            "ordinary_distance_grass_surface.future_transaction_authority.v1",
            future["schema"],
        )
        self.assertEqual("AUTHORIZED_NOT_EXECUTED", future["status"])
        self.assertTrue(future["explicitExecutionAuthorized"])
        self.assertTrue(future["assetsOnlyEndpoint"])
        self.assertTrue(future["optionalPresentationMaterialOnly"])
        self.assertTrue(future["existingLawnFallbackPreserved"])
        self.assertFalse(future["mapMutationAuthorized"])
        self.assertFalse(future["sourceMaterialMutationAuthorized"])
        self.assertFalse(future["sourceTransformMutationAuthorized"])
        self.assertFalse(
            future[
                "geographyTerrainCollisionNavigationLosRfSensorSimulationMutationAuthorized"
            ]
        )
        self.assertTrue(future["callerSha256PinRequired"])
        self.assertFalse(future["callerSha256PinAuthorizesExecution"])
        self.assertFalse(future["compiledTrustAnchorPopulated"])
        self.assertTrue(future["exactFieldRosterRequired"])
        self.assertFalse(future["authorizationReceiptPresent"])

    def test_complete_immutable_source_closure_is_exact(self):
        expected = {
            "ordinary_distance_grass_surface.contract.json": (
                5345,
                "473BFA2D1AA3B5B2F5E8762E8FADF1D9681EE083BD7068FED1C919BEF5956357",
            ),
            "Generated/ordinary_distance_grass_material_intent.json": (
                3402,
                "93E0DF74EB12316559C5AEEC42FC79B879ACB8C61B7D0B878FFC141A0FBF79F6",
            ),
            "Provenance/grass001_local_reuse.provenance.json": (
                6134,
                "5F4B304157AF89EE6DB7BE9D86A7424D70B4F8D0AF55A8DA88205AAE0B1BB051",
            ),
            "source_inventory.json": (
                2449,
                "7213854045B88C91D9D17869CE6521ABEFE2A39FA930169A910BA8E8434FB67D",
            ),
            "OfflineAudit/ordinary_distance_grass_source_lookdev.json": (
                7399,
                "A19D45F727710C6C8CAFC98C5AA54A00D6742D353228271EBC7CDAA0B9B20D7E",
            ),
            "build_grass_surface_candidate.py": (
                55031,
                "976B49B3BA1DB37F6523417BA10DBCC7FEB9DA7E4AD8BBFF47D2017A91069FF6",
            ),
            "OfflineAudit/ordinary_distance_grass_fixed_views.png": (
                1485207,
                "843208465C89B0F7ED3A7785B631C9775F7A83C2148DA1D76A9512BEE3FB7375",
            ),
            "Provenance/upstream_grass001_manifest_extract.json": (
                9182,
                "817EDA4156196A1270FF852E1C2C3D54DB590931F78BB4830C2BF2CDEB50A6A7",
            ),
            "Provenance/Evidence/Grass001.source-page.html": (
                29112,
                "DBBCD614EA0334E05C705FFC9A1B92F39B5720F57DE52BAADCB03C99DAE9F36B",
            ),
            "Provenance/Evidence/Grass001.full.json": (
                13099,
                "5988A2CF736EB297FAAEFCE5F78266FBEC3882301F140DE8E88B2F5FB0529E74",
            ),
            "Provenance/Evidence/cc0-1.0-legalcode.html": (
                32451,
                "001E3D1C905C18B1D034B34200CC952026ABB38457C2294C23EAEF7F6BDA64DF",
            ),
            "Provenance/Evidence/cc0-1.0-deed.html": (
                30476,
                "4CEB8AE6835F2F5263CAA0E39C9E1ADCA9469686C267475049B33521DABBE339",
            ),
            "Provenance/Evidence/ambientcg-license.html": (
                34106,
                "60138E09B277C89977841462634B36E99D7BAE5B77174777813502069C59461C",
            ),
            "SourceTextures/Grass001_2K-JPG_Color.jpg": (
                6953345,
                "8A8BFCFFD087134CAAD2A7DEF216F3DD2D307FBE02340CA47E3C5C71F49D50B4",
            ),
            "SourceTextures/Grass001_2K-JPG_NormalDX.jpg": (
                9975086,
                "26343D7D725A2D5E3F82D0C3FBC9319F517715DF08297A4EE42C96DDE88815C7",
            ),
            "SourceTextures/Grass001_2K-JPG_Roughness.jpg": (
                3286058,
                "2A2A3F9220A351F14858075D4C6BA1FF7FDE8BEDD7009C94CB432BC2B9A362E4",
            ),
            "SourceTextures/Grass001_2K-JPG_AmbientOcclusion.jpg": (
                3644975,
                "7AA2C5AC4DB77005B92BED77399189C9219300ADA2F3D86C3C0FEA51BBA5A3F1",
            ),
            "SourceTextures/Grass001_2K-JPG_Displacement.jpg": (
                3460522,
                "1C6B8AD765F374958CC3FB35E4965830D3F967EC5764957D913C07494D8EA12D",
            ),
        }
        rows = self.contract["pinnedImmutableInputs"]
        self.assertEqual(
            expected,
            {row["relativeFile"]: (row["bytes"], row["sha256"]) for row in rows},
        )
        for relative, (size, digest) in expected.items():
            path = CANDIDATE_ROOT / relative
            self.assertTrue(path.is_file(), path)
            self.assertEqual(size, path.stat().st_size)
            self.assertEqual(digest, sha256(path))

    def test_all_five_jpegs_have_exact_payload_md5_sha_and_formats(self):
        expected = {
            "baseColor": (
                "SourceTextures/Grass001_2K-JPG_Color.jpg",
                6953345,
                "DC6B08F588B0581702296C9C58951B31",
                "8A8BFCFFD087134CAAD2A7DEF216F3DD2D307FBE02340CA47E3C5C71F49D50B4",
                "TSF_BGRA8",
            ),
            "normalDirectX": (
                "SourceTextures/Grass001_2K-JPG_NormalDX.jpg",
                9975086,
                "8A46197DD2E1530CCF52E691E0BD1078",
                "26343D7D725A2D5E3F82D0C3FBC9319F517715DF08297A4EE42C96DDE88815C7",
                "TSF_BGRA8",
            ),
            "roughness": (
                "SourceTextures/Grass001_2K-JPG_Roughness.jpg",
                3286058,
                "5007981963EC7E10686051486AFE6DB3",
                "2A2A3F9220A351F14858075D4C6BA1FF7FDE8BEDD7009C94CB432BC2B9A362E4",
                "TSF_G8",
            ),
            "ambientOcclusion": (
                "SourceTextures/Grass001_2K-JPG_AmbientOcclusion.jpg",
                3644975,
                "6D95D0CFB832ABD098CA1947A38421AE",
                "7AA2C5AC4DB77005B92BED77399189C9219300ADA2F3D86C3C0FEA51BBA5A3F1",
                "TSF_G8",
            ),
            "height": (
                "SourceTextures/Grass001_2K-JPG_Displacement.jpg",
                3460522,
                "2599A12F96EBD37F4188BEB37D74684E",
                "1C6B8AD765F374958CC3FB35E4965830D3F967EC5764957D913C07494D8EA12D",
                "TSF_G8",
            ),
        }
        rows = self.contract["implementation"]["sourceTextures"]
        actual = {
            row["role"]: (
                row["relativeFile"],
                row["bytes"],
                row["md5"],
                row["sha256"],
                row["textureSourceFormat"],
            )
            for row in rows
        }
        self.assertEqual(expected, actual)
        for relative, size, expected_md5, expected_sha, _ in expected.values():
            path = CANDIDATE_ROOT / relative
            self.assertEqual(size, path.stat().st_size)
            self.assertEqual(expected_md5, md5(path))
            self.assertEqual(expected_sha, sha256(path))
        for token in (
            "TextureMd5[]",
            "TextureSha256[]",
            "ExpectedMd5.GetBytes()",
            "SourceData.SourceFiles[0].FileHash != ExpectedMd5",
            "GetSourceCompression()",
            "TSCF_JPEG",
            "GetBulkDataPayload()",
            "FMemory::Memcmp",
            "SHA256(",
            "TextureSourceFormats[]",
            "TSF_BGRA8, TSF_BGRA8, TSF_G8, TSF_G8, TSF_G8",
            "GetNumBlocks() != 1",
            "GetNumLayers() != 1",
            "GetNumSlices() != 1",
            "GetNumMips() != 1",
        ):
            self.assertIn(token, self.editor_source)

    def test_exact_six_output_assets_and_only_six(self):
        root = (
            "/Game/TRIAD/IstanaPublicViewExploreV5D/Vegetation/"
            "OrdinaryDistanceGrassSurfaceIntegration"
        )
        expected = [
            f"{root}/Textures/T_IPV5D_Grass001_BaseColor.T_IPV5D_Grass001_BaseColor",
            f"{root}/Textures/T_IPV5D_Grass001_NormalDX.T_IPV5D_Grass001_NormalDX",
            f"{root}/Textures/T_IPV5D_Grass001_Roughness.T_IPV5D_Grass001_Roughness",
            f"{root}/Textures/T_IPV5D_Grass001_AmbientOcclusion.T_IPV5D_Grass001_AmbientOcclusion",
            f"{root}/Textures/T_IPV5D_Grass001_Height.T_IPV5D_Grass001_Height",
            f"{root}/Materials/M_IPV5D_OrdinaryDistanceGrassSurface.M_IPV5D_OrdinaryDistanceGrassSurface",
        ]
        implementation = self.contract["implementation"]
        self.assertEqual(6, implementation["exactOutputAssetCount"])
        self.assertEqual(5, implementation["exactTextureCount"])
        self.assertEqual(root, implementation["outputNamespace"])
        self.assertEqual(expected, implementation["exactOutputObjectPaths"])
        self.assertEqual(6, len(set(expected)))
        combined = self.runtime_source + self.runtime_test + self.editor_source
        for path in expected:
            self.assertIn(path, combined)
        self.assertIn("constexpr int32 OutputAssetCount = 6", combined)
        self.assertIn("constexpr int32 TextureCount = 5", combined)
        self.assertIn("ExistingCount != 0", self.editor_source)
        self.assertIn("expectedEither=0_or_6", self.editor_source)

    def test_material_graph_matches_scale_distance_and_rotated_normal_intent(self):
        implementation = self.contract["implementation"]
        self.assertEqual(25, implementation["exactMaterialExpressionCount"])
        self.assertEqual([1.4, 1.4], implementation["dualPhaseTileMetres"])
        self.assertEqual(90.0, implementation["phaseRotationDegrees"])
        self.assertEqual(["+Y", "-X", "+Z"], implementation["phaseNormalAxisMapping"])
        self.assertEqual([0.371, 0.613], implementation["phaseOffsetUv"])
        self.assertEqual(7.0, implementation["phaseBlendScaleMetres"])
        self.assertEqual(3.5, implementation["mesoScaleMetres"])
        self.assertEqual([8.4, 18.2], implementation["macroScalesMetres"])
        self.assertEqual(
            {"20m": 0.8, "35m": 0.62, "50m": 0.48},
            implementation["normalStrength"],
        )
        self.assertEqual([50.0, 70.0], implementation["responseFadeMetres"])
        self.assertTrue(implementation["heightRetainedAsInput"])
        self.assertFalse(implementation["heightDisplacementEnabled"])
        for token in (
            "ExpectedExpressionCount = 25",
            "AbsoluteWorldPosition.xy / 140.0",
            "float2(-worldM.y,worldM.x)/1.4+float2(0.371,0.613)",
            "TRIAD_OD_GRASS_INVERSE_ROTATE_PHASE_NORMAL_XY_90_BEFORE_BLEND",
            "float3(PhaseNormal.y,-PhaseNormal.x,PhaseNormal.z)",
            "NormalBlendIsExact",
            "CreateMaterialExpressionEx",
            "RemoveExpressionParameter",
            "UpdateParameterGuid(true, false)",
            "ValidateParameterName(false)",
            "AddExpressionParameter",
            "BuildEditorParameterList",
            "Expression->Material != Material",
            "MaterialExpressionGuid.IsValid()",
            "ExpressionGUID.IsValid()",
            "EditorParameters.Num() != TextureCount * 2",
            "7.0",
            "3.5",
            "8.4",
            "18.2",
            "0.82,1.18",
            "0.48,0.98",
            "20.0",
            "35.0",
            "50.0",
            "70.0",
            "RetainedHeightNoDisplacement",
            "MaxWorldPositionOffsetDisplacement = 0.0f",
        ):
            self.assertIn(token, self.editor_source)
        self.assertGreaterEqual(
            self.editor_source.count("BuildEditorParameterList()"), 2
        )

    def test_runtime_selection_requires_exact_fallback_and_zero_authority(self):
        source = self.runtime_header + self.runtime_source + self.runtime_test
        for token in (
            "ResolveOptionalPresentationMaterial",
            "The exact admitted lawn-surface fallback is mandatory",
            "bExistingLawnFallbackPreserved = true",
            "bMapGeographyTerrainOrSourceTransformModified = false",
            "bCollisionNavigationLosRfSensorOrSimulationAuthority = false",
            "HasExactPresentationPropertyConnections",
            "HasBaseColorConnected",
            "HasRoughnessConnected",
            "HasNormalConnected",
            "HasAmbientOcclusionConnected",
            "IsPropertyConnected",
            "MP_WorldPositionOffset",
            "MP_Displacement",
            "MP_PixelDepthOffset",
            "HasNoPhysicalOrNaniteAuthority",
            "PhysMaterialMask",
            "PhysicalMaterialMap",
            "RenderTracePhysicalMaterialOutputs",
            "NaniteOverrideMaterial.GetOverrideMaterial",
            "bUsedWithInstancedStaticMeshes",
            "bScreenSpaceReflections",
            "bEnableTessellation",
            "bEnableDisplacementFade",
            "HasExactEditorOnlyTexturePolicy",
            "#if WITH_EDITORONLY_DATA",
        ):
            self.assertIn(token, source)
        self.assertIn("Texture->Filter != TF_Default", self.runtime_source)
        for token in (
            "SpawnActor",
            "SetActorTransform",
            "SetWorldTransform",
            "SetRelativeTransform",
            "SetMaterial(",
            "SetStaticMesh(",
            "OpenLevel",
        ):
            self.assertNotIn(token, source)

    def test_runtime_editor_only_texture_policy_is_cross_target_guarded(self):
        helper = re.search(
            r"bool HasExactEditorOnlyTexturePolicy\([^}]+#if "
            r"WITH_EDITORONLY_DATA(?P<editor>.*?)#else(?P<runtime>.*?)#endif\s*}",
            self.runtime_source,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(helper)
        self.assertIn("MipGenSettings", helper.group("editor"))
        self.assertIn("bFlipGreenChannel", helper.group("editor"))
        self.assertNotIn("MipGenSettings", helper.group("runtime"))
        self.assertNotIn("bFlipGreenChannel", helper.group("runtime"))
        source_without_helper = (
            self.runtime_source[: helper.start()]
            + self.runtime_source[helper.end() :]
        )
        self.assertNotIn("MipGenSettings", source_without_helper)
        self.assertNotIn("bFlipGreenChannel", source_without_helper)

    def test_recursive_namespace_and_physical_rollback_fail_closed(self):
        source = self.editor_source
        for token in (
            "CollectNamespaceState(NamespaceRoot, Actual)",
            "GetAssetsByPath",
            "TObjectIterator<UObject>",
            "TObjectIterator<UPackage>",
            "TryConvertLongPackageNameToFilename",
            "TryConvertFilenameToLongPackageName",
            "FindFilesRecursive",
            "TEXT(\"*\")",
            "PhysicalFiles",
            "PhysicalDirectories",
            "GetAssetPackageExtension",
            "GetMapPackageExtension",
            "uncontracted physical file",
            "uncontracted physical directory",
            "bRequirePhysicalAssetFiles",
            "saved namespace lacks the exact six physical .uasset files",
            "DeleteLoadedAssets",
            "DeleteDirectory",
            "IFileManager::Get().DirectoryExists(*PhysicalRoot)",
            "physicalRootAbsent=true",
            "ROLLBACK_REFUSED",
            "ROLLBACK_FAILED",
            "ROLLBACK_COMPLETE",
        ):
            self.assertIn(token, source)
        self.assertGreaterEqual(source.count("FindFilesRecursive"), 2)
        self.assertIn("true,\n                Error", source)
        self.assertIn("false,\n            Error", source)

    def test_no_map_component_or_numbered_successor_mutation_surface(self):
        sources = {
            RUNTIME_HEADER: self.runtime_header,
            RUNTIME_SOURCE: self.runtime_source,
            RUNTIME_TEST: self.runtime_test,
            EDITOR_HEADER: self.editor_header,
            EDITOR_SOURCE: self.editor_source,
            API_AUDIT_PATH: API_AUDIT_PATH.read_text(encoding="utf-8"),
            README_PATH: self.readme,
        }
        forbidden_stage_token = "R" + str(33 + 1)
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
        for path, source in sources.items():
            self.assertNotIn(forbidden_stage_token, source, path)
            for token in forbidden_calls:
                self.assertNotIn(token, source, f"{path}: {token}")
        self.assertEqual(1, self.editor_header.count("UFUNCTION(BlueprintCallable"))
        public_text, private_text = self.editor_header.split("private:", 1)
        self.assertIn("InspectGrassSurfaceReceipts", public_text)
        self.assertNotIn("Materialize", public_text)
        self.assertIn(
            "static bool MaterializeTrustedGrassSurfaceAssetsInternal(",
            private_text,
        )

    def test_gate_and_provenance_drift_checks_are_literal_and_separate(self):
        source = self.editor_source
        for token in (
            "ValidateCandidateContract",
            "ValidateMaterialIntent",
            "ValidateProvenance",
            "ValidateInventory",
            "ValidateAudit",
            "ValidateAcceptedR33Receipt",
            "ValidateFutureAuthorization",
            "ExpectedAcceptedR33ReceiptSha256",
            "ExpectedFutureTransactionAuthorizationSha256",
            "ExactCoreCaptureCount\"), 8",
            "ExplicitHumanReviewAcceptance\"), true",
            "R33CesiumWorldTerrainVisualQaAccepted\"), true",
            "MapModifiedByCapture\"), false",
            "AcceptedR33ReceiptSha256",
            "ExactSourceTextureCount",
            "ExactOutputAssetCount",
            "OptionalPresentationMaterialOnly",
            "ExistingLawnFallbackPreserved",
            "MapMutationAuthorized\"), false",
            "SourceMaterialMutationAuthorized\"), false",
            "SourceTransformMutationAuthorized\"), false",
            "NativeWriteOrUnrealLaunchPerformed\"),\n            false",
            "TrustedAcceptedR33ReceiptSha256",
            "TrustedFutureAuthorizationSha256",
            "bRequireCompiledTrustAnchors",
            "FPaths::IsSamePath(FullR33, FullAuthorization)",
            "Root->Values.Num() != UE_ARRAY_COUNT(ExactFields)",
            "must contain exactly the narrow trusted field roster",
            "Caller-supplied grass-surface receipt hashes do not match",
        ):
            self.assertIn(token, source)
        self.assertNotEqual(
            "triad.istana_explore_v5d.r33_player0_capture.v1",
            self.contract["executionGates"]["futureExplicitTransaction"]["schema"],
        )

    def test_caller_cannot_self_authenticate_materialization(self):
        trust = self.contract["implementation"]["executionTrustBoundary"]
        self.assertTrue(trust["receiptInspectionBlueprintCallable"])
        self.assertFalse(trust["receiptInspectionAuthorizesExecution"])
        self.assertEqual("PRIVATE_CPP_ONLY", trust["materializerVisibility"])
        self.assertFalse(trust["compiledTrustAnchorsAreValidSha256"])
        self.assertFalse(trust["callerSuppliedHashesAloneSufficient"])
        self.assertTrue(trust["futureAuthorizationExactFieldRosterRequired"])
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
        self.assertIn("InspectGrassSurfaceReceipts", public_text)
        self.assertNotIn("Materialize", public_text)
        self.assertIn(
            "MaterializeTrustedGrassSurfaceAssetsInternal", private_text
        )
        self.assertEqual(1, self.editor_header.count("UFUNCTION(BlueprintCallable"))
        self.assertNotRegex(
            self.editor_header,
            r"UFUNCTION\(BlueprintCallable,[\s\S]{0,180}"
            r"MaterializeTrustedGrassSurfaceAssetsInternal",
        )
        for token in (
            "UNSET_ACCEPTED_R33_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "UNSET_FUTURE_AUTHORIZATION_TRUST_ANCHOR_REQUIRES_REVIEWED_SOURCE_CHANGE",
            "!IsSha256(TrustedAcceptedR33ReceiptSha256)",
            "!IsSha256(TrustedFutureAuthorizationSha256)",
            "compiled trusted receipt anchors are unset",
            "Root->Values.Num() != UE_ARRAY_COUNT(ExactFields)",
        ):
            self.assertIn(token, self.editor_source)
        audit_trust = self.audit["executionTrustBoundary"]
        self.assertFalse(audit_trust["publicMaterializationEndpointExposed"])
        self.assertFalse(audit_trust["privateMaterializerReflected"])
        self.assertFalse(audit_trust["callerSuppliedHashesAuthorizeExecution"])
        self.assertFalse(audit_trust["writePathReachableInCurrentSource"])
        self.assertTrue(audit_trust["activationRequiresReviewedSourceEditAndRecompile"])
        for phrase in (
            "read-only receipt inspection",
            "caller-provided paths and hashes",
            "private, non-reflected",
            "deliberately invalid sentinels",
            "write path is unreachable",
        ):
            self.assertIn(phrase, self.readme.lower())

    def test_protected_r30_through_r33_roots_and_r32_contract_remain_exact(self):
        expected_snapshots = {
            "unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R30FacadeLookdev": {
                "aggregateSha256": "DD3EB3473C524A88D1BEBF13CA2B563B9F389317CBC4589754BFE082785F5D09",
                "bytes": 355390,
                "fileCount": 10,
            },
            "unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R31BroadShellLookdev": {
                "aggregateSha256": "6AE7C358189F0EEAACBEF975406898E70679707F693B20079591A48D1F2B509D",
                "bytes": 1002873,
                "fileCount": 10,
            },
            "unreal/SourceAssets/IstanaPublicViewExploreV5D/Vegetation/R32MediumDistanceTurf": {
                "aggregateSha256": "2CAB9D63135BF914EA0E9EA4E930E04B5650AEA3A81179E48749FFFDA8E85ABA",
                "bytes": 587076,
                "fileCount": 8,
            },
            "unreal/SourceAssets/IstanaPublicViewExploreV5D/Terrain/R33CesiumWorldTerrainReference": {
                "aggregateSha256": "E2700F4E17D6FFE01E4B7B1FA23EFB07CA190BAFA1338FEE4E41D7BC9813B290",
                "bytes": 39496,
                "fileCount": 3,
            },
        }
        self.assertEqual(
            expected_snapshots,
            self.contract["preservedExternalPins"]["protectedR30ToR33SourceSnapshot"],
        )
        for relative, expected in expected_snapshots.items():
            if relative.endswith("/R30FacadeLookdev"):
                continue
            self.assertEqual(expected, aggregate_tree(REPO_ROOT / relative))
        receipt_bound_r30_closure = (
            REPO_ROOT
            / "unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
            "R30FacadeLookdev/r30_tree_material_response_v3.source_closure.json"
        )
        self.assertEqual(5171, receipt_bound_r30_closure.stat().st_size)
        self.assertEqual(
            "8DA0BCB32D4066036A985EF6F283E07F967B0AAD2C931C6A94E0C33925A82253",
            sha256(receipt_bound_r30_closure),
        )
        r32 = self.contract["preservedExternalPins"]["r32Contract"]
        r32_path = REPO_ROOT / r32["file"]
        self.assertEqual(26288, r32_path.stat().st_size)
        self.assertEqual(
            "8DED60CE751611306A29071B1E90AA47C17274EB601997C5BB0E420DA9413A89",
            sha256(r32_path),
        )

    def test_artifact_roster_and_hashes_are_exact(self):
        expected_paths = {
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
        self.assertEqual(expected_paths, {row["file"] for row in rows})
        for row in rows:
            path = REPO_ROOT / row["file"]
            self.assertEqual(row["bytes"], path.stat().st_size)
            self.assertEqual(row["sha256"], sha256(path))

    def test_installed_ue55_header_and_source_audit_is_exact(self):
        self.assertEqual(
            "STATIC_INSTALLED_HEADER_AUDIT_ONLY_NOT_COMPILED_OR_EXECUTED",
            self.audit["status"],
        )
        self.assertFalse(self.audit["launchOrBuildPerformed"])
        self.assertTrue(all(value is False for value in self.audit["nativeClaims"].values()))
        engine_root = Path("C:/Program Files/Epic Games/UE_5.5/Engine")
        source_root = engine_root / "Source"
        if not source_root.is_dir():
            self.skipTest("Pinned UE 5.5 install is not present on this test host")
        for row in self.audit["headers"] + self.audit["engineSources"]:
            path = source_root / row["pathBelowEngineSource"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(row["bytes"], path.stat().st_size)
            self.assertEqual(row["sha256"], sha256(path))
        for row in self.audit["configFiles"]:
            path = engine_root / row["pathBelowEngine"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(row["bytes"], path.stat().st_size)
            self.assertEqual(row["sha256"], sha256(path))

        exact_signatures = {
            "Runtime/Core/Public/HAL/FileManager.h": (
                "virtual bool DirectoryExists( const TCHAR* InDirectory )=0;",
                "virtual void FindFilesRecursive( TArray<FString>& FileNames, const TCHAR* StartDirectory, const TCHAR* Filename, bool Files, bool Directories, bool bClearFileNames=true) = 0;",
                "virtual int64 FileSize( const TCHAR* Filename )=0;",
            ),
            "Runtime/Core/Public/Misc/FileHelper.h": (
                "static CORE_API void BufferToString( FString& Result, const uint8* Buffer, int32 Size );",
                "static CORE_API bool LoadFileToArray( TArray<uint8>& Result, const TCHAR* Filename, uint32 Flags = 0 );",
            ),
            "Runtime/Core/Public/Misc/SecureHash.h": (
                "CORE_API void Update(const uint8* input, uint64 inputLen);",
                "void Set(FMD5& MD5)",
                "const uint8* GetBytes() const",
                "const int32 GetSize() const",
            ),
            "Runtime/Core/Public/Containers/Set.h": (
                "TSet Difference(const TSet& OtherSet) const",
            ),
            "Runtime/CoreUObject/Public/Misc/PackageName.h": (
                "TryConvertFilenameToLongPackageName(const FString& InFilename, FString& OutPackageName,",
                "TryConvertLongPackageNameToFilename(const FString& InLongPackageName, FString& OutFilename, const FString& InExtension = TEXT(\"\"));",
                "FString ObjectPathToPackageName(const FString& InObjectPath);",
                "bool DoesPackageExist(const FString& LongPackageName, FString* OutFilename = nullptr, bool InAllowTextFormats = true);",
                "const FString& GetAssetPackageExtension();",
                "const FString& GetMapPackageExtension();",
            ),
            "Runtime/AssetRegistry/Public/AssetRegistry/IAssetRegistry.h": (
                "virtual bool GetAssetsByPath(FName PackagePath, TArray<FAssetData>& OutAssetData, bool bRecursive = false, bool bIncludeOnlyOnDiskAssets = false) const = 0;",
            ),
            "Runtime/Engine/Classes/Engine/Texture.h": (
                "ETextureSourceCompressionFormat GetSourceCompression() const",
                "int64 GetSizeX() const",
                "int64 GetSizeY() const",
                "int32 GetNumSlices() const",
                "int32 GetNumMips() const",
                "int32 GetNumLayers() const",
                "int32 GetNumBlocks() const",
                "ETextureSourceFormat GetFormat(int32 LayerIndex = 0) const",
                "ENGINE_API FSharedBuffer GetBulkDataPayload();",
                "TEnumAsByte<enum TextureFilter> Filter;",
            ),
            "Runtime/Engine/Public/Materials/MaterialInterface.h": (
                "ENGINE_API TArrayView<const TObjectPtr<UObject>> GetReferencedTextures() const;",
            ),
            "Runtime/Engine/Public/Materials/Material.h": (
                "ENGINE_API bool IsPropertyConnected(EMaterialProperty Property) const;",
                "ENGINE_API bool HasBaseColorConnected() const;",
                "ENGINE_API bool HasRoughnessConnected() const;",
                "ENGINE_API bool HasAmbientOcclusionConnected() const;",
                "ENGINE_API bool HasNormalConnected() const;",
                "ENGINE_API void SetShadingModel(EMaterialShadingModel NewModel);",
                "ENGINE_API virtual bool AddExpressionParameter(UMaterialExpression* Expression, TMap<FName, TArray<UMaterialExpression*> >& ParameterTypeMap);",
                "ENGINE_API virtual bool RemoveExpressionParameter(UMaterialExpression* Expression);",
                "ENGINE_API virtual void BuildEditorParameterList();",
                "TMap<FName, TArray<UMaterialExpression*> > EditorParameters;",
            ),
            "Runtime/Engine/Public/Materials/MaterialExpression.h": (
                "TObjectPtr<class UMaterial> Material;",
                "FGuid MaterialExpressionGuid;",
                "ENGINE_API void UpdateParameterGuid(bool bForceGeneration, bool bAllowMarkingPackageDirty);",
                "ENGINE_API void UpdateMaterialExpressionGuid(bool bForceGeneration, bool bAllowMarkingPackageDirty);",
                "ENGINE_API virtual void ValidateParameterName(const bool bAllowDuplicateName = true);",
            ),
            "Runtime/Engine/Public/MaterialExpressionIO.h": (
                "ENGINE_API void Connect( int32 InOutputIndex, class UMaterialExpression* InExpression );",
            ),
            "Runtime/Engine/Classes/EditorFramework/AssetImportData.h": (
                "ENGINE_API FString GetFirstFilename() const;",
                "const FAssetImportInfo& GetSourceData() const",
                "int32 GetSourceFileCount() const",
                "static ENGINE_API FString ResolveImportFilename(const FString& InRelativePath, const UPackage* Outermost);",
            ),
            "Editor/UnrealEd/Public/AssetImportTask.h": (
                "UNREALED_API const TArray<UObject*>& GetObjects() const;",
                "TArray<FString> ImportedObjectPaths;",
            ),
            "Developer/AssetTools/Public/IAssetTools.h": (
                "virtual UObject* CreateAsset(const FString& AssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory, FName CallingContext = NAME_None) = 0;",
                "virtual void ImportAssetTasks(const TArray<UAssetImportTask*>& ImportTasks) = 0;",
            ),
            "Editor/UnrealEd/Public/Subsystems/EditorAssetSubsystem.h": (
                "UNREALED_API bool DeleteLoadedAssets(const TArray<UObject*>& AssetsToDelete);",
                "UNREALED_API bool DeleteDirectory(const FString& DirectoryPath);",
                "UNREALED_API bool SaveLoadedAssets(const TArray<UObject*>& AssetsToSave, bool bOnlyIfIsDirty = true);",
                "UNREALED_API bool DoesDirectoryExist(const FString& DirectoryPath);",
            ),
            "Editor/MaterialEditor/Public/MaterialEditingLibrary.h": (
                "static UMaterialExpression* CreateMaterialExpressionEx(UMaterial* Material, UMaterialFunction* MaterialFunction, TSubclassOf<UMaterialExpression> ExpressionClass, UObject* SelectedAsset = nullptr, int32 NodePosX = 0, int32 NodePosY = 0, bool bAllowMarkingPackageDirty = true);",
                "static void RecompileMaterial(UMaterial* Material);",
            ),
        }
        audited_paths = {row["pathBelowEngineSource"] for row in self.audit["headers"]}
        self.assertTrue(set(exact_signatures).issubset(audited_paths))
        for relative, signatures in exact_signatures.items():
            header = normalized_cpp((source_root / relative).read_text(encoding="utf-8"))
            for signature in signatures:
                self.assertIn(normalized_cpp(signature), header, f"{relative}: {signature}")

    def test_cpp_files_have_balanced_scaffolding_and_no_temp_artifacts(self):
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
        bytecode_root = PLUGIN_ROOT / "Tests/__pycache__"
        if bytecode_root.is_dir():
            leftovers.extend(
                bytecode_root.glob(f"{Path(__file__).stem}.*.pyc")
            )
        self.assertEqual([], leftovers)

    def test_preservation_and_remaining_native_proof_are_explicit(self):
        boundary = self.contract["preservationBoundary"]
        self.assertTrue(all(value is False for value in boundary.values()))
        expected = {
            "UE55_UBT_UHT_COMPILE_AND_LINK",
            "REVIEWED_SOURCE_CHANGE_TO_PIN_BOTH_TRUSTED_RECEIPT_HASHES_RECOMPILE_AND_EXPLICITLY_EXPOSE_INTERNAL_MATERIALIZER",
            "FIVE_TEXTURE_IMPORT_ASSET_IMPORT_DATA_MD5_AND_RETAINED_JPEG_SHA256_PROOF",
            "CUSTOM_HLSL_MATERIAL_COMPILE_AND_EXACT_FIVE_TEXTURE_REFERENCE_PROOF",
            "SAVE_AND_COLD_RELOAD_EXACT_SIX_ASSETS_AND_PHYSICAL_FILES",
            "INJECTED_FAILURE_RECURSIVE_REGISTRY_LIVE_PACKAGE_FILESYSTEM_ROLLBACK_PROOF",
            "REAL_FALLBACK_OPTIONAL_PRESENTATION_RESOLUTION",
            "EXCLUSION_CROSSFADE_MOIRE_AND_TEMPORAL_STABILITY_CAPTURE",
            "MATCHED_20_35_50_METRE_DEFINITION_CAPTURE",
            "COLLISION_NAVIGATION_LOS_RF_SENSOR_AND_GEOGRAPHY_REGRESSION_HASHES",
            "MEMORY_FRAME_TIME_AND_DRAW_CALL_ACCEPTANCE",
            "HUMAN_VISUAL_ACCEPTANCE",
        }
        self.assertEqual(expected, set(self.contract["remainingNativeProof"]))


if __name__ == "__main__":
    unittest.main()
