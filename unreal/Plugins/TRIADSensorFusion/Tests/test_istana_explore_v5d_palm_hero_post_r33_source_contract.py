import hashlib
import json
import re
import struct
import unittest
from pathlib import Path


UNREAL_ROOT = Path(__file__).absolute().parents[3]
REPO_ROOT = UNREAL_ROOT.parent
PLUGIN_ROOT = UNREAL_ROOT / "Plugins/TRIADSensorFusion"
CANDIDATE_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/PalmHeroCandidate"
)
INTEGRATION_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/PalmHeroIntegration"
)
GEOMETRY_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/"
    "GeometryVariationCandidateV4"
)
CONTRACT_PATH = (
    INTEGRATION_ROOT / "palm_hero_post_r33_integration.source_contract.v1.json"
)
API_AUDIT_PATH = INTEGRATION_ROOT / "palm_hero_ue55_api_audit.source.json"
README_PATH = INTEGRATION_ROOT / "README.md"
RUNTIME_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DPalmHeroSourceLibrary.h"
)
RUNTIME_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DPalmHeroSourceLibrary.cpp"
)
RUNTIME_TEST = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/Tests/"
    "TRIADIstanaExploreV5DPalmHeroSourceLibraryTests.cpp"
)
EDITOR_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DPalmHeroEditorLibrary.h"
)
EDITOR_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DPalmHeroEditorLibrary.cpp"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


class PalmHeroPostR33SourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
        cls.audit = json.loads(API_AUDIT_PATH.read_text(encoding="utf-8"))
        cls.integration_readme = README_PATH.read_text(encoding="utf-8")
        cls.runtime_header = RUNTIME_HEADER.read_text(encoding="utf-8")
        cls.runtime_source = RUNTIME_SOURCE.read_text(encoding="utf-8")
        cls.runtime_test = RUNTIME_TEST.read_text(encoding="utf-8")
        cls.editor_header = EDITOR_HEADER.read_text(encoding="utf-8")
        cls.editor_source = EDITOR_SOURCE.read_text(encoding="utf-8")

    def test_delivery_is_unnumbered_source_only_and_all_native_claims_false(self):
        self.assertEqual(
            "triad.istana_public_view_explore_v5d_tree_realism."
            "palm_hero.post_r33_source_integration.v1",
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

    def test_every_immutable_input_is_hash_and_byte_pinned(self):
        expected = {
            "palm_hero_candidate.contract.json": (
                8922,
                "A856DF661D8FD06B7F0BBF5AE55BF711FC4196EA3FBF97E5C68F1031F54CFFC4",
            ),
            "Provenance/palm_tree_bark.provenance.json": (
                4851,
                "DC7F1CF47E118D008FEF4A32BED721FA7518802860091871AB291DD661AFB504",
            ),
            "Generated/M_IPV5D_PalmHeroCandidate.mtl": (
                697,
                "57D3D23017EF5E8C2A70BA6E71509ED3D3B8C364B44034DF3D4FEF89589C4936",
            ),
            "Generated/SM_IPV5D_PalmHeroCandidate_LOD0.obj": (
                11120505,
                "119F1D8D9AD9E3A8F98BF3E94E32F4B88B221CA6F022DD899BDD276D4376BF97",
            ),
            "Generated/SM_IPV5D_PalmHeroCandidate_LOD1.obj": (
                4007254,
                "53DBD68149CE2E361EAF2E466B1A6B4CCCE5E9A4C294AD286FAC06E274720D57",
            ),
            "Generated/SM_IPV5D_PalmHeroCandidate_LOD2.obj": (
                1010979,
                "7E94F23E604B40346D9FE8DF91AB5F8A2651CFF13E37F2E1509F04A2A5082C5F",
            ),
            "SourceTextures/palm_tree_bark_diff_2k.jpg": (
                7622678,
                "808B70B2B1F94D292B689E71CA761B99E2C1593706883495C041B6BF36B6BAB4",
            ),
            "SourceTextures/palm_tree_bark_nor_dx_2k.jpg": (
                6833446,
                "CE3EECA1617E23851DFD86E541ED0F1FDF2D88BC526EFA7FD0F35CF8E9EF58F0",
            ),
            "SourceTextures/palm_tree_bark_rough_2k.jpg": (
                5158632,
                "25B7B82A0218DF0A3B778004855CF217665FC8B4E6874800819C296907FE7B23",
            ),
            "SourceTextures/palm_tree_bark_ao_2k.jpg": (
                5242101,
                "8B720C90A32BE2665295B3F9D250282108937B3B4F58BF07F7E607341B92F963",
            ),
            "Provenance/api/palm_tree_bark.files.json": (
                34767,
                "3ED4BA21C0D223B3370902536AA1389CDAE90062CEA125E6A5BDB8468C56AADF",
            ),
            "Provenance/api/palm_tree_bark.info.json": (
                1153,
                "D323726E1E96BDF6B6085B6D8912349E6B5992D890B770786084649FB18EC607",
            ),
            "Provenance/source-page/palm_tree_bark.html": (
                192582,
                "1D28E0B9FC07D0868C8257436158CA05B8BDBFDF50B1240B018FE16876F5CB8C",
            ),
            "Provenance/license/polyhaven-license.html": (
                72295,
                "6ED195C17E59E0404BCFC79AB1943C0B63307BB777D8A874B6650E048C6BCE80",
            ),
        }
        rows = self.contract["pinnedImmutableInputs"]
        self.assertEqual(expected, {r["relativeFile"]: (r["bytes"], r["sha256"]) for r in rows})
        for relative, (size, digest) in expected.items():
            path = CANDIDATE_ROOT / relative
            self.assertTrue(path.is_file(), path)
            self.assertEqual(size, path.stat().st_size)
            self.assertEqual(digest, sha256(path))

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

    def test_existing_geometry_candidate_and_selector_remain_exact(self):
        expected = {
            "tree_geometry_variation_candidate.v4.json": (
                28625,
                "3E8216CBB0D78E755C1597030C3C513ACE70E5412464620EEDAF5530AD54DF31",
            ),
            "tree_geometry_instance_selector.v4.json": (
                227267,
                "59BA4B21B81A3546CBE76E1108D6EB39C2DEA1DD0CBB309791515F9AC5B59E51",
            ),
        }
        rows = self.contract["preservedExternalPins"]
        self.assertEqual(
            expected,
            {row["relativeFile"]: (row["bytes"], row["sha256"]) for row in rows},
        )
        for relative, (size, digest) in expected.items():
            path = GEOMETRY_ROOT / relative
            self.assertEqual(size, path.stat().st_size)
            self.assertEqual(digest, sha256(path))

    def test_runtime_optional_source_keeps_fallback_and_zero_authority(self):
        source = self.runtime_header + self.runtime_source + self.runtime_test
        for token in (
            "ResolveOptionalGeometryVariationPalmSource",
            "The exact existing TreeRealism palm fallback is mandatory",
            "bExplicitlySelectPalmHero",
            "bExistingPalmFallbackPreserved = true",
            "bMapOrSourceTransformModified = false",
            "bCollisionNavigationLosRfSensorOrTerrainAuthority = false",
            "ExpectedTriangles[] = {104244, 37968, 9864}",
            "ExpectedTrianglesByMaterial",
            "Bark\"), TEXT(\"FrondLive\"), TEXT(\"FrondDry",
            "GetCollisionTraceFlag()",
            "#if WITH_EDITORONLY_DATA",
        ):
            self.assertIn(token, source)
        self.assertNotIn("SpawnActor", source)
        self.assertNotIn("SetActorTransform", source)

    def test_editor_validates_all_source_and_import_semantics(self):
        source = self.editor_header + self.editor_source
        for token in (
            "ValidateSourceRoster",
            "ValidateCandidateContract",
            "ValidateProvenanceManifest",
            "ValidateMtl",
            "ValidateObj",
            "ImportUniformScale = 100.0f",
            "bConvertScene = false",
            "bConvertSceneUnit = false",
            "bCombineMeshes = true",
            "bImportMaterials = false",
            "bImportTextures = false",
            "bAutoGenerateCollision = false",
            "bGenerateLightmapUVs = false",
            "bBuildNanite = false",
            "bRemoveDegenerates = false",
            "FBXNIM_ImportNormals",
            "CreateMeshDescription",
            "CommitMeshDescription",
            "CloneMeshDescription",
            "GetPolygonGroupMaterialSlotNames",
            "verticesExactlyOnZ0",
            "TrianglesByMaterial",
            "SaveLoadedAssets(AssetsToSave, false)",
        ):
            self.assertIn(token, source)
        self.assertEqual(1, self.editor_source.count("LoadFileToArray"))
        self.assertNotIn("LoadFileToString", self.editor_source)
        self.assertIn("BufferToString", self.editor_source)

    def test_every_pinned_bound_literal_survives_json_double_to_fvector3f(self):
        tolerance = self.contract["implementation"][
            "candidateBoundsJsonToFloat32ToleranceMeters"
        ]
        self.assertEqual(0.000001, tolerance)
        self.assertTrue(
            self.contract["implementation"]["candidateBoundsHashPinStillRequired"]
        )
        self.assertIn(
            "ContractBoundsFloat32ToleranceMeters = 0.000001",
            self.editor_source,
        )

        array_match = re.search(
            r"const FVector3f LodSourceBoundsMinMeters\[\] = \{(?P<minimum>.*?)\};"
            r"\s*const FVector3f LodSourceBoundsMaxMeters\[\] = \{(?P<maximum>.*?)\};",
            self.editor_source,
            re.DOTALL,
        )
        self.assertIsNotNone(array_match)

        vector_pattern = re.compile(
            r"FVector3f\(\s*([-+\d.]+)f,\s*([-+\d.]+)f,\s*([-+\d.]+)f\)"
        )
        source_minima = vector_pattern.findall(array_match.group("minimum"))
        source_maxima = vector_pattern.findall(array_match.group("maximum"))
        self.assertEqual(3, len(source_minima))
        self.assertEqual(3, len(source_maxima))

        candidate = json.loads(
            (CANDIDATE_ROOT / "palm_hero_candidate.contract.json").read_text(
                encoding="utf-8"
            )
        )
        deltas = []
        for lod, source_minimum, source_maximum in zip(
            candidate["lods"], source_minima, source_maxima
        ):
            for key, source_vector in (
                ("min", source_minimum),
                ("max", source_maximum),
            ):
                json_vector = lod["boundsMeters"][key]
                for json_double, source_literal in zip(json_vector, source_vector):
                    float32 = struct.unpack(
                        "<f", struct.pack("<f", float(source_literal))
                    )[0]
                    delta = abs(json_double - float32)
                    deltas.append(delta)
                    self.assertLessEqual(delta, tolerance)

        observed_maximum = max(deltas)
        self.assertGreater(observed_maximum, 0.0000005)
        self.assertAlmostEqual(
            self.contract["implementation"][
                "candidateBoundsMaximumPinnedRoundingDeltaMeters"
            ],
            observed_maximum,
            places=15,
        )

    def test_ue55_obj_y_flip_has_separate_explicit_imported_bounds(self):
        implementation = self.contract["implementation"]
        self.assertEqual(
            ["X", "-Y", "Z"], implementation["ue55LegacyObjConvertPos"]
        )
        candidate = json.loads(
            (CANDIDATE_ROOT / "palm_hero_candidate.contract.json").read_text(
                encoding="utf-8"
            )
        )
        expected_minima = []
        expected_maxima = []
        for lod in candidate["lods"]:
            source_minimum = lod["boundsMeters"]["min"]
            source_maximum = lod["boundsMeters"]["max"]
            expected_minima.append(
                [
                    source_minimum[0] * 100.0,
                    -source_maximum[1] * 100.0,
                    source_minimum[2] * 100.0,
                ]
            )
            expected_maxima.append(
                [
                    source_maximum[0] * 100.0,
                    -source_minimum[1] * 100.0,
                    source_maximum[2] * 100.0,
                ]
            )
        for actual, expected in zip(
            implementation["lodExpectedUnrealBoundsMinCentimetres"],
            expected_minima,
        ):
            for coordinate, expected_coordinate in zip(actual, expected):
                self.assertAlmostEqual(expected_coordinate, coordinate, places=9)
        for actual, expected in zip(
            implementation["lodExpectedUnrealBoundsMaxCentimetres"],
            expected_maxima,
        ):
            for coordinate, expected_coordinate in zip(actual, expected):
                self.assertAlmostEqual(expected_coordinate, coordinate, places=9)

        source = self.editor_source
        array_match = re.search(
            r"const FVector3f LodExpectedUnrealBoundsMinCentimetres\[\] = "
            r"\{(?P<minimum>.*?)\};\s*const FVector3f "
            r"LodExpectedUnrealBoundsMaxCentimetres\[\] = "
            r"\{(?P<maximum>.*?)\};",
            source,
            re.DOTALL,
        )
        self.assertIsNotNone(array_match)
        vector_pattern = re.compile(
            r"FVector3f\(\s*([-+\d.]+)f,\s*([-+\d.]+)f,\s*([-+\d.]+)f\)"
        )
        cpp_minima = [
            [float(value) for value in vector]
            for vector in vector_pattern.findall(array_match.group("minimum"))
        ]
        cpp_maxima = [
            [float(value) for value in vector]
            for vector in vector_pattern.findall(array_match.group("maximum"))
        ]
        self.assertEqual(
            implementation["lodExpectedUnrealBoundsMinCentimetres"],
            cpp_minima,
        )
        self.assertEqual(
            implementation["lodExpectedUnrealBoundsMaxCentimetres"],
            cpp_maxima,
        )
        for token in (
            "LodExpectedUnrealBoundsMinCentimetres",
            "LodExpectedUnrealBoundsMaxCentimetres",
            "Converter.ConvertPos",
            "maps (X,Y,Z) to (X,-Y,Z)",
            "sourceToUnrealCoordinateConversion=X,-Y,Z",
        ):
            self.assertIn(token, source)
        self.assertNotIn(
            "LodSourceBoundsMinMeters[Lod] * 100.0f",
            source,
        )

    def test_existing_outputs_require_exact_source_and_admission_metadata(self):
        source = self.editor_source
        for token in (
            "ExactAssetMetadata",
            "ValidateCommonOutputMetadata",
            "WriteCommonOutputMetadata",
            "Package->HasMetaData()",
            "TRIAD_PalmHeroCandidateContractSha256",
            "TRIAD_PalmHeroProvenanceSha256",
            "TRIAD_PalmHeroAcceptedR33ReceiptSha256",
            "TRIAD_PalmHeroFutureAuthorizationSha256",
            "TRIAD_PalmHeroSourceSha256",
            "TRIAD_PalmHeroMaterialSlot",
            "TRIAD_PalmHeroLod%dSourceSha256",
            "TRIAD_PalmHeroOptionalFallback",
            "LoadAndValidateOutputAssets(Admission, nullptr, Error)",
        ):
            self.assertIn(token, source)
        policy = self.contract["implementation"][
            "existingAssetMetadataPolicy"
        ]
        self.assertIn("Every one of the exact eight assets", policy)
        self.assertIn("textures additionally match their source hashes", policy)
        self.assertIn("mesh all three OBJ hashes", policy)

    def test_texture_acceptance_reads_real_import_data_and_source_payload(self):
        source = self.editor_source
        texture_validator = source[
            source.index("bool ValidateTexture("):
            source.index("bool ValidateMaterial(")
        ]
        self.assertIn("Texture->bFlipGreenChannel", texture_validator)
        self.assertIn(
            "ValidateTextureImportProvenance",
            texture_validator,
        )
        self.assertIn(
            "ValidateTextureSourcePayloadFingerprint",
            texture_validator,
        )

        provenance = source[
            source.index("bool ValidateTextureImportProvenance("):
            source.index("bool ValidateTextureSourcePayloadFingerprint(")
        ]
        for token in (
            "Texture->AssetImportData",
            "GetSourceFileCount() != 1",
            "GetSourceData()",
            "SourceData.SourceFiles.Num() != 1",
            "SourceData.SourceFiles[0].FileHash.IsValid()",
            "UAssetImportData::ResolveImportFilename",
            "FPaths::IsSamePath",
            "FMD5 ExpectedMd5Builder",
            "ExpectedMd5.Set(ExpectedMd5Builder)",
            "SourceData.SourceFiles[0].FileHash != ExpectedMd5",
            "Admission.TextureSourceBytes[Index]",
        ):
            self.assertIn(token, provenance)

        payload = source[
            source.index("bool ValidateTextureSourcePayloadFingerprint("):
            source.index("bool ValidateTexture(")
        ]
        for token in (
            "Texture->Source.IsValid()",
            "Texture->Source.GetNumBlocks() != 1",
            "Texture->Source.GetNumLayers() != 1",
            "Texture->Source.GetNumSlices() != 1",
            "Texture->Source.GetNumMips() != 1",
            "Texture->Source.GetFormat() != TextureSourceFormats[Index]",
            "ETextureSourceCompressionFormat::TSCF_JPEG",
            "Texture->Source.GetBulkDataPayload()",
            "SourcePayload.GetSize()",
            "FMemory::Memcmp",
            "SHA256(",
            "TextureSha256[Index]",
        ):
            self.assertIn(token, payload)
        self.assertNotIn("TRIAD_PalmHeroSourceSha256", payload)

        implementation = self.contract["implementation"]
        self.assertFalse(implementation["textureGreenChannelFlip"])
        self.assertEqual(
            ["TSF_BGRA8", "TSF_BGRA8", "TSF_BGRA8", "TSF_G8"],
            implementation["textureSourceFormats"],
        )
        self.assertTrue(implementation["ue55RetainJpegEngineDefaultPinned"])
        self.assertIn(
            "resolved source filename and MD5",
            implementation["textureAssetImportDataBinding"],
        )
        self.assertIn(
            "byte-for-byte plus SHA-256 identical",
            implementation["textureSourcePayloadBinding"],
        )

    def test_material_route_is_stable_and_no_wind_is_falsely_claimed(self):
        source = self.editor_source + self.runtime_source
        for token in (
            "M_IPV5D_PalmHero_Bark",
            "M_IPV5D_PalmHero_FrondLive",
            "M_IPV5D_PalmHero_FrondDry",
            "T_IPV5D_PalmHero_Bark_BaseColor",
            "T_IPV5D_PalmHero_Bark_NormalDX",
            "T_IPV5D_PalmHero_Bark_Roughness",
            "T_IPV5D_PalmHero_Bark_AmbientOcclusion",
            "MSM_TwoSidedFoliage",
            "EditorOnly->SubsurfaceColor.Connect",
            "EditorOnly->WorldPositionOffset.Expression != nullptr",
            "TC_Normalmap",
            "TC_Masks",
        ):
            self.assertIn(token, source)
        for forbidden in (
            "MaterialExpressionSimpleGrassWind",
            "MaterialExpressionWorldPositionOffset",
            "nativeVisualAcceptanceProvided=true",
        ):
            self.assertNotIn(forbidden, source)

    def test_two_receipt_gates_are_explicit_and_narrow(self):
        source = self.editor_source
        for token in (
            "triad.istana_explore_v5d.r33_player0_capture.v1",
            "ExplicitHumanReviewAcceptance",
            "ConfirmedEightImagesReviewed",
            "MapModifiedByCapture",
            "future_transaction_authority.v1",
            "AUTHORIZED_NOT_EXECUTED",
            "MATERIALIZE_PALM_HERO_CANDIDATE_ASSETS_ONLY_AND_EXPOSE_OPTIONAL_SOURCE",
            "AcceptedR33ReceiptSha256",
            "PalmHeroContractSha256",
            "PalmHeroProvenanceSha256",
            "ExplicitExecutionAuthorized",
            "AssetsOnlyEndpoint",
            "OptionalPalmSourceOnly",
            "ExistingPalmFallbackPreserved",
            "MapMutationAuthorized",
            "SourceTransformMutationAuthorized",
            "NativeWriteOrUnrealLaunchPerformed",
            "Root->Values.Num() != UE_ARRAY_COUNT(ExactFields)",
            "must contain exactly the narrow trusted field roster",
            "TrustedAcceptedR33ReceiptSha256",
            "TrustedFutureAuthorizationSha256",
            "bRequireCompiledTrustAnchors",
            "Caller-supplied PalmHero receipt hashes do not match",
        ):
            self.assertIn(token, source)

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
        self.assertIn("InspectPalmHeroReceipts", public_text)
        self.assertNotIn("Materialize", public_text)
        self.assertIn("MaterializeTrustedPalmHeroAssetsInternal", private_text)
        self.assertEqual(1, self.editor_header.count("UFUNCTION(BlueprintCallable"))
        self.assertNotIn("ValidateAuthorizedPalmHeroInputs", self.editor_header)
        self.assertNotIn("ValidateAuthorizedPalmHeroInputs", self.editor_source)
        self.assertNotRegex(
            self.editor_header,
            r"UFUNCTION\(BlueprintCallable,[\s\S]{0,180}"
            r"MaterializeTrustedPalmHeroAssetsInternal",
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
        materializer = self.editor_source[
            self.editor_source.index(
                "MaterializeTrustedPalmHeroAssetsInternal("
            ):
        ]
        pre_write_gate = materializer[: materializer.index("IAssetTools& AssetTools")]
        self.assertIn("BuildAdmission(", pre_write_gate)
        self.assertIn("true,\n            Admission", pre_write_gate)
        self.assertLess(
            materializer.index("BuildAdmission("),
            materializer.index("IAssetTools& AssetTools"),
        )
        self.assertEqual(8, self.contract["implementation"]["exactOutputAssets"])
        self.assertIn("constexpr int32 OutputAssetCount = 8", self.editor_source)
        self.assertIn("InspectPalmHeroReceipts", self.integration_readme)
        self.assertIn("private, non-reflected C++ method", self.integration_readme)
        self.assertIn("deliberately unset sentinels", self.integration_readme)
        self.assertNotIn("C:/Users/", self.integration_readme)

    def test_partial_namespace_and_fresh_asset_rollback_are_explicit(self):
        source = self.editor_source
        for token in (
            "PARTIAL_NAMESPACE_DENIED",
            "expectedEither=0_or_8",
            "ExistingOutputPackageCount",
            "HasUnexpectedOutputNamespaceAssets",
            "UNEXPECTED_NAMESPACE_ASSET_DENIED",
            "GetOnlyExactImportedObject",
            "Task->ImportedObjectPaths.Num() != 1",
            "ReturnedObjects.Num() != 1",
            "CollectNamespaceState",
            "NamespaceContainsOnlyExpectedAssets",
            "NamespaceMatchesExactAssets",
            "TObjectIterator<UObject>",
            "TObjectIterator<UPackage>",
            "DeleteExactFreshAssets",
            "DeleteLoadedAssets(Loaded)",
            "DeleteFreshNamespaceContents",
            "DeleteDirectory(NamespaceRoot)",
            "bOutputRootProvenEmptyAtEntry",
            "bStagingRootProvenEmptyAtEntry",
            "RollbackFreshMaterialization",
            "ROLLBACK_COMPLETE recursivelyDeletedFreshOutputAndStagingNamespaces=true",
            "RemoveFreshStagingAssets",
            "stagingNamespaceEmpty=true",
            "failureRollbackScope=bothIsolatedNamespacesProvenEmptyAtEntry",
        ):
            self.assertIn(token, source)
        self.assertGreaterEqual(
            source.count("RollbackFreshMaterialization(FreshBaseline, Error)"),
            6,
        )
        self.assertLess(
            source.index("RemoveFreshStagingAssets(Error)"),
            source.index("SaveLoadedAssets(AssetsToSave, false)"),
        )
        policy = self.contract["implementation"]["failureRollbackPolicy"]
        self.assertIn("both isolated output and staging namespaces", policy)
        self.assertIn("require both DeleteLoadedAssets and DeleteDirectory", policy)
        cleanup = source[
            source.index("bool DeleteFreshNamespaceContents("):
            source.index("bool RollbackFreshMaterialization(")
        ]
        self.assertIn("!bLoadedDeleteReportedSuccess ||", cleanup)
        self.assertIn("!bDirectoryDeleteReportedSuccess ||", cleanup)
        self.assertIn("!bNamespaceProvenEmpty", cleanup)
        self.assertIn("could not prove cleanup of namespace", cleanup)
        self.assertIn("unexpected", self.contract["implementation"][
            "namespaceValidationPolicy"
        ])

    def test_new_files_do_not_name_or_call_numbered_successor_wrappers(self):
        watched = (
            RUNTIME_HEADER,
            RUNTIME_SOURCE,
            RUNTIME_TEST,
            EDITOR_HEADER,
            EDITOR_SOURCE,
            API_AUDIT_PATH,
            README_PATH,
            CONTRACT_PATH,
            Path(__file__).absolute(),
        )
        forbidden_successor = "R" + str(34)
        for path in watched:
            text = path.read_text(encoding="utf-8")
            self.assertNotIn(forbidden_successor, text, path)
            for stage in range(30, 34):
                wrapper = (
                    "TRIADIstanaExploreV5D" + "R" + str(stage)
                    + "Player0CaptureLibrary"
                )
                self.assertNotIn(wrapper, text, path)

    def test_static_ue55_api_audit_is_truthful(self):
        self.assertEqual(
            "STATIC_HEADER_AUDIT_ONLY_NOT_COMPILED_OR_EXECUTED",
            self.audit["status"],
        )
        self.assertFalse(self.audit["launchOrBuildPerformed"])
        self.assertTrue(all(v is False for v in self.audit["nativeClaims"].values()))
        surface = self.audit["scaffoldExecutionSurface"]
        self.assertEqual(1, surface["publicReflectedMethodCount"])
        self.assertEqual("InspectPalmHeroReceipts", surface["publicReflectedMethod"])
        self.assertFalse(surface["publicInspectionWrites"])
        self.assertFalse(surface["publicInspectionAuthorizesExecution"])
        self.assertFalse(surface["publicMaterializerExposed"])
        self.assertEqual(
            "MaterializeTrustedPalmHeroAssetsInternal",
            surface["privateNonReflectedMaterializer"],
        )
        self.assertFalse(surface["compiledTrustAnchorsAreValidSha256"])
        self.assertFalse(surface["callerSuppliedHashesAloneSufficient"])
        self.assertTrue(surface["futureAuthorizationExactFieldRosterRequired"])
        self.assertFalse(surface["nativeWriteReachable"])
        self.assertTrue(surface["activationRequiresReviewedSourceEditAndRecompile"])
        unresolved = set(self.audit["unresolvedNativeProof"])
        self.assertIn("UnrealBuildTool compile and link of both modules", unresolved)
        self.assertIn(
            "Fresh recursive namespace rollback removes expected and unexpected assets after injected failures before save, during save, and after save",
            unresolved,
        )
        engine_root = Path("C:/Program Files/Epic Games/UE_5.5/Engine")
        header_root = engine_root / "Source"
        if header_root.is_dir():
            for row in self.audit["headers"] + self.audit["engineSources"]:
                path = header_root / row["pathBelowEngineSource"]
                self.assertTrue(path.is_file(), path)
                self.assertEqual(row["bytes"], path.stat().st_size)
                self.assertEqual(row["sha256"], sha256(path))
            for row in self.audit["configFiles"]:
                path = engine_root / row["pathBelowEngine"]
                self.assertTrue(path.is_file(), path)
                self.assertEqual(row["bytes"], path.stat().st_size)
                self.assertEqual(row["sha256"], sha256(path))

    def test_preservation_and_remaining_proof_boundaries_are_complete(self):
        self.assertTrue(
            all(
                value is False
                for value in self.contract["preservationBoundary"].values()
            )
        )
        expected = {
            "UE55_COMPILE_AND_LINK",
            "REVIEWED_SOURCE_CHANGE_TO_PIN_BOTH_TRUSTED_RECEIPT_HASHES_RECOMPILE_AND_EXPLICITLY_CALL_OR_EXPOSE_PRIVATE_MATERIALIZER",
            "TEXTURE_IMPORT_ASSET_IMPORT_DATA_AND_RAW_SOURCE_PAYLOAD_PROOF",
            "OBJ_IMPORT_EXACT_SLOT_VERTEX_TRIANGLE_ROOT_AND_BOUNDS_PROOF",
            "SAVE_AND_COLD_RELOAD_EXACT_EIGHT_ASSETS",
            "INJECTED_FAILURE_AND_UNEXPECTED_IMPORT_OUTPUT_RECURSIVE_ROLLBACK_PROOF",
            "REAL_ASSET_OPTIONAL_SOURCE_RESOLUTION",
            "FIXED_VIEW_LOD_SILHOUETTE_AND_ALIASING_CAPTURE",
            "FROND_TRANSMISSION_SUBSURFACE_AND_WIND_REVIEW",
            "MEMORY_AND_FRAME_TIME_ACCEPTANCE",
            "HUMAN_VISUAL_ACCEPTANCE",
        }
        self.assertEqual(expected, set(self.contract["remainingNativeProof"]))


if __name__ == "__main__":
    unittest.main()
