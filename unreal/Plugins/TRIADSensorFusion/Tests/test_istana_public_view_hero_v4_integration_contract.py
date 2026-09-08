from __future__ import annotations

import json
import hashlib
import re
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal" / "Plugins" / "TRIADSensorFusion"
HEADER = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Public"
    / "TRIADIstanaPublicViewHeroV4EditorLibrary.h"
)
SOURCE = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaPublicViewHeroV4EditorLibrary.cpp"
)
MATERIAL_ROOT = (
    REPO / "unreal" / "SourceAssets" / "IstanaPublicView" / "HeroMaterialsV4"
)
GEOMETRY_ROOT = REPO / "unreal" / "SourceAssets" / "IstanaPublicViewV4"
SCRIPTS_ROOT = REPO / "scripts"
INTEGRATION_FREEZE = (
    PLUGIN / "Resources" / "IstanaPublicViewHeroV4.integration.freeze.json"
)
ZERO_SHA256 = "0" * 64


def function_body(source: str, name: str) -> str:
    start = source.index(name)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"unterminated C++ function: {name}")


class HeroV4IntegrationCandidateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.source = SOURCE.read_text(encoding="utf-8")
        cls.contract = json.loads(
            (MATERIAL_ROOT / "hero_materials_v4.contract.json").read_text(
                encoding="utf-8"
            )
        )
        cls.interface = json.loads(
            (MATERIAL_ROOT / "unreal_asset_interface.v4.json").read_text(
                encoding="utf-8"
            )
        )
        cls.material_freeze = json.loads(
            (MATERIAL_ROOT / "hero_materials_v4.freeze.json").read_text(
                encoding="utf-8"
            )
        )
        cls.geometry_contract = json.loads(
            (GEOMETRY_ROOT / "istana_public_view_hero_v4.contract.json").read_text(
                encoding="utf-8"
            )
        )
        cls.geometry_manifest = json.loads(
            (
                GEOMETRY_ROOT
                / "Generated"
                / "IstanaPublicViewV4Building.manifest.json"
            ).read_text(encoding="utf-8")
        )
        cls.integration_freeze = json.loads(
            INTEGRATION_FREEZE.read_text(encoding="utf-8")
        )

    def test_01_additive_v4_editor_entry_points_exist(self) -> None:
        expected = [
            "ValidateIstanaPublicViewHeroV4RemoteControlProject",
            "ImportIstanaPublicViewHeroV4Assets",
            "ValidateIstanaPublicViewHeroV4Assets",
            "MigrateIstanaPublicViewExteriorMapToHeroV4",
            "ValidateIstanaPublicViewHeroV4Map",
            "ValidateIstanaPublicViewHeroV4PlayWorldReadiness",
            "CaptureIstanaPublicViewHeroV4QualityFrame",
            "QuiesceIstanaPublicViewHeroV4PlayWorldForStop",
        ]
        for name in expected:
            self.assertIn(name, self.header)
            self.assertIn(name, self.source)
        self.assertIn("UTRIADIstanaPublicViewHeroV4EditorLibrary", self.header)
        self.assertIn("TRIADSENSORFUSIONEDITOR_API", self.header)

    def test_02_namespaces_and_source_map_are_exactly_additive(self) -> None:
        exact_literals = [
            "/Game/Maps/Istana_PublicView_Exterior_v1",
            "/Game/Maps/Istana_PublicView_Exterior_v2",
            "/Game/Maps/Istana_PublicView_Exterior_v3",
            "/Game/Maps/Istana_PublicView_Exterior_v4",
            "/Game/TRIAD/IstanaPublicViewV4",
            "/Game/TRIAD/IstanaPublicView/HeroMaterialsV4",
            "/Game/TRIAD/IstanaPublicView/HeroMaterialsV3/Textures",
            "SourceAssets/IstanaPublicViewV4",
            "SourceAssets/IstanaPublicView/HeroMaterialsV4",
        ]
        for value in exact_literals:
            self.assertIn(value, self.source)
        self.assertNotIn("D:\\\\", self.source)
        self.assertNotIn("D:/", self.source)

    def test_03_final_v4_bindings_are_exact_and_nonzero(self) -> None:
        exact_bindings = {
            "FrozenFreezeSha256": "c6afedad2ca868be455a5425ab02114fb1584d237700c9fba489b595be650f12",
            "FrozenMaterialFreezeSha256": "2459254b4835c0aff123028feb4ed09fe6282bf8cbe07901b4813e89d85300a7",
            "FrozenMaterialContractSha256": "3251b58a8d3c87cf7c41e520a3bc331ef5e04cd399b327bc35c9c9ba376f96cb",
            "FrozenMaterialInterfaceSha256": "80dfc9bf36cf6419e0512c3cf49b0f310669a94c6325389a1ba971f22329493e",
            "FrozenGeometryFreezeSha256": "c31cc1b81d4a7008f69ad84c72795244ef9bc4daf12e29d4ee181bde3f1f75cd",
            "FrozenGeometryContractSha256": "2578edcb30c122f15a357ae604fa9ab6d8a95c457255cfafe27dd05c69827822",
            "FrozenGeometryGeneratorSha256": "65911bd06fa4d09331c048f9c778c1650d6f32736f94981f36dd6974d1498b6d",
            "FrozenGeometryManifestSha256": "7277aad39e33208ea4ccd5dd848ac8f2ddffef5ab903a44f22854dee77e77b51",
            "FrozenGeometryManifestSemanticSha256": "e56c627bf40588ebcef74fb17d4a9a8c35339c8bd2addcc3bac4959f91c0b842",
            "FrozenGeometryObjSha256": "6bd4caba97ae9b559303c5cf0192ecfd4962921acfe72c106a2e304fb37bd076",
            "FrozenGeometryClosureAuditSha256": "691c25144f501bde381ff052afe188aeca44c878df389f8eac6ae5c9f09fecd3",
            "FrozenGeometryDenseClosureAuditSha256": "70d31d09d4532a81a7da70f95b7fac31abff1d5e315915846e52d0028cba58f0",
            "FrozenGeometryWindingAuditSha256": "063be95d22f540e67923ed72794a3463e2c7ffece853cfbeea089e69a58f87c3",
        }
        for constant, digest in exact_bindings.items():
            self.assertRegex(
                self.source,
                rf'const FString {constant}\(\s*TEXT\("{digest}"\)\s*\);',
            )
        self.assertIn(ZERO_SHA256, self.source)
        self.assertRegex(
            self.source,
            r"constexpr int32 ExpectedHeroTriangles = 958836;",
        )
        self.assertRegex(
            self.source,
            r"constexpr int32 ExpectedHeroVertices = 2876508;",
        )
        self.assertIn("V4_FREEZE_NOT_FROZEN", self.source)
        self.assertTrue((MATERIAL_ROOT / "hero_materials_v4.freeze.json").exists())
        self.assertTrue(INTEGRATION_FREEZE.exists())

        for public_name in (
            "ImportIstanaPublicViewHeroV4Assets",
            "MigrateIstanaPublicViewExteriorMapToHeroV4",
            "ValidateIstanaPublicViewHeroV4Assets",
            "ValidateIstanaPublicViewHeroV4Map",
        ):
            definition_start = self.source.rfind(f"\n    {public_name}(")
            self.assertGreater(definition_start, 0)
            body = function_body(self.source[definition_start:], public_name)
            self.assertIn("ValidateCompleteV4SourceSet", body)

    def test_04_v3_textures_are_read_only_references_not_v4_payloads(self) -> None:
        import_body = function_body(self.source, "ImportHeroV4AssetSet")
        self.assertIn("LoadObject<UTexture2D>", import_body)
        self.assertIn("ValidateHeroTexture", import_body)
        self.assertIn("V4_IMPORT_REFUSED_FROZEN_V3_TEXTURE_DEPENDENCY", import_body)
        self.assertNotIn("AssetsToSave.Add(Texture)", import_body)
        self.assertNotIn("Texture->Modify", import_body)
        self.assertNotIn("Texture->PostEditChange", import_body)
        self.assertNotIn("Texture->MarkPackageDirty", import_body)
        self.assertNotIn("TextureTasks", import_body)
        self.assertEqual(1, import_body.count("ImportAssetTasks("))
        self.assertIn("HeroTasks", import_body)

        expected_paths_body = function_body(self.source, "BuildExpectedAssetPaths")
        self.assertNotIn("TextureObjectPath", expected_paths_body)
        inspect_body = function_body(self.source, "InspectHeroV4Assets")
        self.assertIn("ForbiddenV4TextureObjectPath", inspect_body)
        self.assertIn("AssetRegistry.ScanPathsSynchronous", inspect_body)
        self.assertIn("AssetRegistry.GetAssetsByPaths", inspect_body)
        self.assertIn("HeroAssetRoot", inspect_body)
        self.assertIn("MaterialAssetRoot", inspect_body)
        self.assertIn("ActualPaths.Num() == 0", inspect_body)
        self.assertIn("ActualPaths.Num() != ExpectedPaths.Num()", inspect_body)
        self.assertIn("!ExpectedPaths.Contains(ActualPath)", inspect_body)
        self.assertLess(
            inspect_body.index("ActualPaths.Num() == 0"),
            inspect_body.index("LoadObject<UTexture2D>"),
        )
        self.assertIn("Pack.Instances.Num()", inspect_body)
        self.assertNotIn("ten instances", inspect_body)
        self.assertIn("ValidateFrozenV3TexturePayloadSeal", self.source)

        # Mirror the production exact-set classification with explicit hostile
        # paths. This is deliberately semantic and does not bless any one
        # Asset Registry enumeration order.
        material_root = self.interface["contentRoot"]
        expected = {
            "/Game/TRIAD/IstanaPublicViewV4/Building/"
            "SM_IstanaPublicViewV4_Building_Hero."
            "SM_IstanaPublicViewV4_Building_Hero",
            *(
                f"{material_root}/{row['asset']}.{row['asset']}"
                for row in self.interface["masterMaterials"].values()
            ),
            *(
                f"{material_root}/{row['asset']}.{row['asset']}"
                for row in self.interface["instances"]
            ),
            *(
                f"{material_root}/{row['asset']}.{row['asset']}"
                for row in self.interface["specialSlotBindingsV4"].values()
            ),
        }
        self.assertEqual(13, len(expected))

        def classify(actual: set[str]) -> str:
            if not actual:
                return "Absent"
            return "CompleteValid" if actual == expected else "PartialOrInvalid"

        self.assertEqual("Absent", classify(set()))
        self.assertEqual("CompleteValid", classify(expected.copy()))
        for binding in self.interface["specialSlotBindingsV4"].values():
            special_path = (
                f"{material_root}/{binding['asset']}.{binding['asset']}"
            )
            self.assertEqual(
                "PartialOrInvalid",
                classify(expected - {special_path}),
                special_path,
            )
        self.assertEqual(
            "PartialOrInvalid",
            classify(
                expected
                | {
                    f"{material_root}/Textures/T_Bogus.T_Bogus",
                }
            ),
        )
        self.assertEqual(
            "PartialOrInvalid",
            classify(
                expected
                | {
                    "/Game/TRIAD/IstanaPublicViewV4/Bogus/"
                    "SM_Bogus.SM_Bogus",
                }
            ),
        )

    def test_05_material_local_glass_ssr_is_created_validated_and_sealed(self) -> None:
        glass_body = function_body(self.source, "CreateGlassHeroMaster")
        opaque_body = function_body(self.source, "CreateOpaqueHeroMaster")
        graph_body = function_body(self.source, "ComputeHeroMaterialGraphSha256")
        validator_body = function_body(self.source, "ValidateHeroMaster")
        self.assertIn("Material->bScreenSpaceReflections = true", glass_body)
        self.assertIn("Material->bScreenSpaceReflections = false", opaque_body)
        self.assertIn("Material->bScreenSpaceReflections", graph_body)
        self.assertIn("Material->bScreenSpaceReflections != bGlass", validator_body)
        self.assertIn("triad.istana_hero_surface_graph.v4.1", self.source)
        self.assertIn("triad.istana_hero_glass_graph.v4.2", self.source)
        self.assertNotRegex(
            glass_body,
            r"r\.SSR|IConsoleManager|DefaultEngine\.ini|ShaderModel6",
        )

    def test_06_material_contract_is_v3_texture_reference_only(self) -> None:
        scope = self.contract["scope"]
        texture_refs = self.interface["textureReferences"]
        self.assertEqual("triad.istana_hero_materials.v4", self.contract["schema"])
        self.assertEqual("4.0.0", self.contract["version"])
        self.assertEqual(
            "triad.istana_hero_material_unreal_interface.v4",
            self.interface["schema"],
        )
        self.assertEqual(0, scope["generatedOrCopiedTextureCountV4"])
        self.assertEqual(57, scope["referencedFrozenTextureCountV3"])
        self.assertFalse(scope["changesFrozenV3TexturePixels"])
        self.assertFalse(scope["changesFrozenV3TexturePackages"])
        self.assertEqual(0, texture_refs["v4TexturePackageCount"])
        self.assertEqual(57, texture_refs["v3TextureReferenceCount"])
        self.assertIsNone(texture_refs["destinationTextureRoot"])
        self.assertTrue(texture_refs["noV4TextureCopy"])
        self.assertTrue(
            self.interface["masterMaterials"]["glass"]["screenSpaceReflections"]
        )
        self.assertFalse(
            self.interface["masterMaterials"]["glass"]
            ["requiresGlobalRendererChange"]
        )
        self.assertEqual(
            1, self.interface["normalStrengthOwnership"]["runtimeApplicationCount"]
        )
        self.assertFalse(
            self.interface["normalStrengthOwnership"]
            ["inheritedGeneratedMapsHaveRuntimeStrengthApplied"]
        )

    def test_07_slot_normalization_is_semantic_and_precedes_binding(self) -> None:
        normalize = function_body(self.source, "NormalizeImportedHeroMaterialSlots")
        self.assertIn("MaterialSlotName", normalize)
        self.assertIn("ImportedMaterialSlotName", normalize)
        self.assertIn("ImportedToOrdered", normalize)
        self.assertIn("GetSectionInfoMap", normalize)
        self.assertIn("GetOriginalSectionInfoMap", normalize)
        self.assertIn("RenderMaterialIndex != SectionInfo.MaterialIndex", normalize)
        self.assertIn("RenderMaterialIndex != OriginalSectionInfo.MaterialIndex", normalize)
        importer = function_body(self.source, "ImportHeroV4AssetSet")
        self.assertLess(
            importer.index("NormalizeImportedHeroMaterialSlots"),
            importer.index("Hero->SetMaterial"),
        )
        self.assertLess(
            importer.index("NormalizeImportedHeroMaterialSlots"),
            importer.index("SaveLoadedAssets"),
        )

    def test_08_migration_preserves_v1_v2_v3_and_swaps_only_hero(self) -> None:
        migrate = function_body(self.source, "MigrateMapToV4")
        self.assertIn("ProtectedV1MapPackage", migrate)
        self.assertIn("ProtectedV2MapPackage", migrate)
        self.assertIn("SourceMapPackage", migrate)
        self.assertIn("ProtectedV1ShaBefore", migrate)
        self.assertIn("ProtectedV2ShaBefore", migrate)
        self.assertIn("SourceV3ShaBefore", migrate)
        self.assertIn("ProtectedV1ShaAfter", migrate)
        self.assertIn("ProtectedV2ShaAfter", migrate)
        self.assertIn("SourceV3ShaAfter", migrate)
        self.assertEqual(1, migrate.count("SetStaticMesh(Hero)"))
        self.assertEqual(1, migrate.count("EmptyOverrideMaterials"))
        self.assertIn("ValidateSurroundingsUnchanged", migrate)
        self.assertIn("ValidateWorldForV4", migrate)
        self.assertLess(migrate.index("ValidateWorldForV4"), migrate.index("SaveLoadedAsset"))

        source_validator = function_body(
            self.source, "ValidateExactV3SourceHeroComponent"
        )
        self.assertNotRegex(
            source_validator,
            r"Modify\(|PostEditChange|SetMaterial|SetStaticMesh|MarkPackageDirty",
        )

    def test_09_neutral_capture_and_quality_overrides_restore_exactly(self) -> None:
        configure = function_body(self.source, "ConfigureHeroV4D65CaptureCamera")
        camera_gate = function_body(self.source, "HasHeroV4D65CaptureCamera")
        quality = function_body(self.source, "FScopedHeroV4TsrCaptureQuality final")
        for body in (configure, camera_gate):
            self.assertIn("AutoExposureBias", body)
            self.assertIn("CameraShutterSpeed", body)
            self.assertIn("ScreenSpace", body)
        self.assertIn("Settings.AutoExposureBias = 0.0f", configure)
        self.assertIn("Settings.CameraShutterSpeed = 125.0f", configure)
        for cvar in (
            "r.AntiAliasingMethod",
            "r.ScreenPercentage",
            "r.TSR.History.ScreenPercentage",
            "r.SSGI.Quality",
            "r.SSR.Quality",
        ):
            self.assertIn(cvar, quality)
        self.assertIn("OriginalSsgiQuality_", quality)
        self.assertIn("OriginalSsrQuality_", quality)
        self.assertIn("Restore(Ignored)", quality)

    def test_10_recess_policy_is_narrow_and_geometry_binding_is_held(self) -> None:
        recess = self.interface["specialSlotBindingsV4"]["M_IPV4_Recess"]
        self.assertEqual(
            "NEAR_BLACK_ONLY_FOR_NARROW_TERMINAL_JOINERY_SEAMS_NEVER_BROAD_OPENING_BACKDROPS",
            recess["surfaceIntent"],
        )
        scope = self.geometry_contract["scope"]
        self.assertTrue(
            scope["reclassifiesPreservedV3BroadRecessSupportToV4Render"]
        )
        self.assertTrue(
            scope["limitsNearBlackRecessToV4AuthoredTerminalSeamsAcrossHero"]
        )
        self.assertEqual(
            "M_IPV4_Render",
            self.geometry_contract["materialInterface"]
            ["preservedBroadV3RecessSupportTarget"],
        )
        self.assertEqual(
            531,
            self.geometry_manifest["featureCounts"]
            ["preservedV3BroadRecessGroupsReclassified"],
        )
        self.assertEqual(
            6372,
            self.geometry_manifest["featureCounts"]
            ["preservedV3BroadRecessTrianglesReclassified"],
        )
        self.assertEqual(
            39,
            self.geometry_manifest["featureCounts"]["v4TerminalRecessSeams"],
        )
        self.assertEqual(
            468,
            self.geometry_manifest["featureCounts"]
            ["v4RecessTrianglesAreaAndExtentAudited"],
        )

    def test_11_freeze_role_roster_has_no_v2_v3_clone_path_residue(self) -> None:
        self.assertEqual(
            "/Game/Maps/Istana_PublicView_Exterior_v3",
            self.integration_freeze["sourceMap"],
        )
        roles = {
            record["role"]: record["relativePath"]
            for array_name in ("files", "protectedV1Files")
            for record in self.integration_freeze[array_name]
        }
        self.assertEqual(
            "SourceAssets/IstanaPublicViewV4/central_closure_audit.py",
            roles["GEOMETRY_CLOSURE_AUDIT"],
        )
        self.assertEqual(
            "SourceAssets/IstanaPublicViewV3/dense_facade_closure_audit.py",
            roles["GEOMETRY_DENSE_CLOSURE_AUDIT"],
        )
        self.assertEqual(
            "SourceAssets/IstanaPublicViewV4/v4_winding_audit.py",
            roles["GEOMETRY_WINDING_AUDIT"],
        )
        self.assertEqual(
            "SourceAssets/IstanaPublicView/HeroMaterialsV3/Source/source_manifest.json",
            roles["HERO_MATERIAL_SOURCE_MANIFEST_V3"],
        )
        self.assertEqual(
            "SourceAssets/IstanaPublicView/HeroMaterialsV3/Generated/manifest.json",
            roles["HERO_MATERIAL_GENERATED_MANIFEST_V3"],
        )
        self.assertEqual(
            "SourceAssets/IstanaPublicView/HeroMaterialsV3/build_hero_materials_v3.py",
            roles["HERO_MATERIAL_BUILDER_V3"],
        )
        forbidden = (
            "SourceAssets/IstanaPublicViewV4/facade_closure_audit.py",
            "SourceAssets/IstanaPublicViewV4/dense_facade_closure_audit.py",
            "SourceAssets/IstanaPublicViewV4/slate_winding_audit.py",
            "SourceAssets/IstanaPublicView/HeroMaterialsV4/Source/source_manifest.json",
            "SourceAssets/IstanaPublicView/HeroMaterialsV4/Generated/manifest.json",
            "SourceAssets/IstanaPublicView/HeroMaterialsV4/build_hero_materials_v4.py",
        )
        self.assertFalse(set(forbidden) & set(roles.values()))
        validate_geometry = function_body(self.source, "ValidateGeometrySource")
        for stale_field in (
            "systematicFacadeClosureRayProbeCount",
            "denseJitteredFacadeClosureRayReviewProbeCount",
            "denseJitteredFacadeClosureRayPhaseOffsetProbeCount",
            "denseJitteredFacadeClosureRayMissCount",
            "systematicSlateWindingTrianglesAudited",
            "outwardWoundSlateSlopes",
        ):
            self.assertNotIn(stale_field, validate_geometry)
        for exact_field in (
            "systematicFacadeClosureRayProbes",
            "independentDenseReviewFacadeClosureRayProbes",
            "phaseOffsetDenseFacadeClosureRayProbes",
            "v3DenseClosureProbeCount",
            "v4CentralDenseClosureProbeCount",
            "v4RoofWindingTrianglesAudited",
            "v4VisibleFrontWindingTrianglesAudited",
        ):
            self.assertIn(exact_field, validate_geometry)

    def test_12_final_material_freeze_is_geometry_bound_and_reviewed(self) -> None:
        self.assertEqual(
            "FROZEN_AFTER_EXACT_V3_TEXTURE_PAYLOAD_VALIDATION_GEOMETRY_SEMANTIC_BINDING_AND_INDEPENDENT_REVIEW",
            self.material_freeze["freezeStatus"],
        )
        binding = self.material_freeze["geometryBinding"]
        self.assertEqual(
            self.interface["geometryBinding"]["geometryFreezeSha256"],
            binding["geometryFreezeSha256"],
        )
        self.assertEqual("M_IPV4_Render", binding["preservedBroadV3RecessTarget"])
        self.assertEqual("M_IPV4_Recess", binding["v4AuthoredTerminalRecessSlot"])
        self.assertFalse(binding["openingSizedNearBlackBackingPresent"])

    def test_13_integration_freeze_records_are_exact_unique_and_complete(self) -> None:
        freeze_bytes = INTEGRATION_FREEZE.read_bytes()
        self.assertEqual(7279, len(freeze_bytes))
        self.assertEqual(
            "c6afedad2ca868be455a5425ab02114fb1584d237700c9fba489b595be650f12",
            hashlib.sha256(freeze_bytes).hexdigest(),
        )
        records = [
            record
            for array_name in ("files", "protectedV1Files")
            for record in self.integration_freeze[array_name]
        ]
        self.assertEqual(len(records), len({record["role"] for record in records}))
        self.assertEqual(
            len(records), len({record["relativePath"] for record in records})
        )
        expected_roles = {
            "GEOMETRY_FREEZE",
            "GEOMETRY_CONTRACT",
            "GEOMETRY_GENERATOR",
            "GEOMETRY_CLOSURE_AUDIT",
            "GEOMETRY_DENSE_CLOSURE_AUDIT",
            "GEOMETRY_WINDING_AUDIT",
            "GEOMETRY_MANIFEST",
            "BUILDING_HERO_VISUAL_V4",
            "HERO_MATERIAL_FREEZE",
            "HERO_MATERIAL_CONTRACT",
            "HERO_MATERIAL_UNREAL_INTERFACE",
            "HERO_MATERIAL_SOURCE_MANIFEST_V3",
            "HERO_MATERIAL_GENERATED_MANIFEST_V3",
            "HERO_MATERIAL_BUILDER_V3",
            "PROTECTED_V3_MATERIAL_CONTRACT",
            "PROTECTED_V3_MATERIAL_UNREAL_INTERFACE",
            "HERO_V3_V4_VISUAL_ACCEPTANCE_SETTINGS",
            "PUBLIC_REFERENCE_MANIFEST",
            "PROTECTED_V3_GEOMETRY_FREEZE",
            "PROTECTED_V3_INTEGRATION_FREEZE",
            "PROTECTED_V2_GEOMETRY_FREEZE",
            "PROTECTED_V2_INTEGRATION_FREEZE",
            "PROTECTED_V1_GEOMETRY_KERNEL",
            "PROTECTED_V1_BUILDING_GENERATOR",
            "PROTECTED_V1_CONTEXT_GENERATOR",
            "PROTECTED_V1_CONTRACT",
        }
        self.assertEqual(expected_roles, {record["role"] for record in records})
        unreal_root = REPO / "unreal"
        for record in records:
            path = unreal_root / Path(record["relativePath"])
            payload = path.read_bytes()
            self.assertEqual(record["bytes"], len(payload), record["role"])
            self.assertEqual(
                record["sha256"],
                hashlib.sha256(payload).hexdigest(),
                record["role"],
            )

    def test_14_v4_scripts_reuse_v3_profile_and_publish_additively(self) -> None:
        names = (
            "Install-IstanaPublicViewHeroV4DevelopmentAssets.ps1",
            "Import-IstanaPublicViewHeroV4Assets.ps1",
            "Migrate-IstanaPublicViewHeroV4Map.ps1",
            "Start-IstanaPublicViewHeroV4Editor.ps1",
            "Set-IstanaPublicViewHeroV4PlayInEditor.ps1",
            "Capture-IstanaPublicViewHeroV4QualityPreviews.ps1",
        )
        scripts = {
            name: (SCRIPTS_ROOT / name).read_text(encoding="utf-8")
            for name in names
        }
        combined = "\n".join(scripts.values())
        self.assertNotIn("D:\\", combined)
        self.assertNotIn("D:/", combined)
        self.assertNotIn(
            "IstanaPublicViewHeroV4VisualAcceptance.settings.json", combined
        )
        self.assertNotIn("ISTANA_PUBLIC_VIEW_HERO_V4_VISUAL_ACCEPTANCE_ONLY", combined)
        self.assertNotIn("V4-only no-lidar/RPC-off process file", self.source)
        self.assertIn(
            "IstanaPublicViewHeroV3VisualAcceptance.settings.json", combined
        )
        self.assertIn("ISTANA_PUBLIC_VIEW_HERO_V3_VISUAL_ACCEPTANCE_ONLY", combined)

        installer = scripts[
            "Install-IstanaPublicViewHeroV4DevelopmentAssets.ps1"
        ]
        self.assertIn("Install-IstanaPublicViewHeroV3DevelopmentAssets.ps1", installer)
        self.assertIn(
            "764089445b03dcecd5c706370301d4d82646f258ce90183a5ce52d6904808e15",
            installer,
        )
        self.assertIn("Copy-NewOrEqualHashFile", installer)
        self.assertIn("[System.IO.File]::Move($stageFile, $DestinationFile)", installer)
        self.assertNotIn("@(& $baseInstaller", installer)
        self.assertNotRegex(installer, r"(?m)^\s*&\s*\$baseInstaller\b")
        self.assertIn("V3BaseInstallerCalled = $false", installer)
        self.assertIn(
            "V3SourceAndProfilePrerequisitesByteExact = $true", installer
        )
        self.assertIn(
            "V3PluginPrerequisitesCurrentOrReviewedCompatible = $true",
            installer,
        )
        self.assertIn(
            "ReviewedCompatibleV3PluginPrerequisiteCount = ", installer
        )
        self.assertIn(
            "V3PluginPrerequisitesOverwritten = $false", installer
        )
        self.assertIn(
            "Source\\TRIADSensorFusionEditor\\Private\\"
            "TRIADIstanaPublicViewHeroV3EditorLibrary.cpp",
            installer,
        )
        self.assertIn(
            "3c2f916845c421070e7a64c25ea95cd8de2fbea95800f4db166edd3a7177fdee",
            installer,
        )
        self.assertIn(
            "f48379a6c236b65553d2ef41fcea2c8932b53cc7e138da5daa667aa2a92857a9",
            installer,
        )
        self.assertIn(
            "Tests\\test_istana_public_view_hero_v3_integration_contract.py",
            installer,
        )
        self.assertIn(
            "d040466ec9eda50afb94e2b210cd2ae6e964bfc72a91012df78c5b6388988746",
            installer,
        )
        self.assertIn(
            "59543e0254b44d5baa6ca548e58767ddee64a54b4a53065653317aa289f577ac",
            installer,
        )
        self.assertIn("REVIEWED_COMPATIBLE_READ_ONLY", installer)
        self.assertIn("ImportedV3PackageRosterPresent = $true", installer)
        self.assertIn(
            "ImportedV3SemanticValidationDeferredToV4EditorGate = $true",
            installer,
        )
        self.assertIn("FreshProjectRequiresSeparateV3Install = $true", installer)
        self.assertIn("Assert-ExactInstalledPrerequisiteTree", installer)
        self.assertIn("Assert-ExactInstalledPrerequisiteFile", installer)
        self.assertIn("Assert-ExactInstalledHeroV3TextureRoster", installer)
        self.assertIn("$targetV3Map", installer)
        self.assertIn("$targetV3Mesh", installer)
        self.assertIn("$targetV3TextureRoot", installer)
        self.assertIn("exact 57-name frozen-contract Hero-V3 texture", installer)
        self.assertIn(
            "first run Install-IstanaPublicViewHeroV3DevelopmentAssets.ps1",
            installer,
        )
        self.assertIn("RecognizedPriorHashReplacementUsed = $false", installer)
        self.assertIn("-B (Join-Path $GeometryRoot 'validate_hero_v4.py')", installer)
        self.assertIn("validate_hero_materials_v4.py", installer)
        self.assertNotIn("build_hero_materials_v4.py", installer)

        publish = installer[installer.index("$protectedBefore =") :]
        geometry_copy = publish.index("foreach ($sourceFile in $geometryFiles)")
        material_copy = publish.index("foreach ($sourceFile in $materialFiles)")
        installed_validation = publish.index(
            "# Validate the exact installed V4 source trees"
        )
        plugin_copy = publish.index("foreach ($sourceFile in $pluginFiles)")
        self.assertLess(geometry_copy, material_copy)
        self.assertLess(material_copy, installed_validation)
        self.assertLess(installed_validation, plugin_copy)
        self.assertLess(
            installer.rindex("Assert-NewOrEqualHashFile", 0, installer.index("$protectedBefore =")),
            installer.index("$protectedBefore ="),
        )
        self.assertEqual(
            4,
            len(
                re.findall(
                    r"'(?:Source|Tests|Resources)\\[^']+(?:\.h|\.cpp|\.py|\.json)'",
                    installer[installer.index("$v4PluginRelativePaths = @(") : installer.index(")\n$requiredSourcePaths")],
                )
            ),
        )

        start = scripts["Start-IstanaPublicViewHeroV4Editor.ps1"]
        self.assertIn("Istana_PublicView_Exterior_v3.umap", start)
        self.assertIn("Istana_PublicView_Exterior_v4.umap", start)
        self.assertNotIn("$v2MapFile", start)

        migrate = scripts["Migrate-IstanaPublicViewHeroV4Map.ps1"]
        self.assertIn("$report.sourceMapPackage", migrate)
        self.assertIn("/Game/Maps/Istana_PublicView_Exterior_v3", migrate)
        self.assertIn("/Game/Maps/Istana_PublicView_Exterior_v4", migrate)

    def test_15_installer_prerequisites_and_publication_are_fail_closed(self) -> None:
        installer_path = (
            SCRIPTS_ROOT / "Install-IstanaPublicViewHeroV4DevelopmentAssets.ps1"
        )
        installer = installer_path.read_text(encoding="utf-8")
        function_names = (
            "Get-InstallSourceFiles",
            "Assert-NewOrEqualHashFile",
            "Copy-NewOrEqualHashFile",
            "Get-RelativePath",
            "Assert-NoUnexpectedTargetFiles",
            "Assert-ExactInstalledPrerequisiteFile",
            "Assert-ExactInstalledPrerequisiteTree",
        )
        functions = {
            name: function_body(installer, f"function {name}")
            for name in function_names
        }

        move_line = "[System.IO.File]::Move($stageFile, $DestinationFile)"
        self.assertEqual(1, functions["Copy-NewOrEqualHashFile"].count(move_line))
        race_copy = functions["Copy-NewOrEqualHashFile"].replace(
            move_line,
            "[System.IO.File]::WriteAllText($DestinationFile, "
            "'intervening-unknown')\n            " + move_line,
        )

        powershell = (
            "param([string] $FixtureRoot)\n"
            "Import-Module Microsoft.PowerShell.Utility\n\n"
            + "\n\n".join(
            functions[name]
            if name != "Copy-NewOrEqualHashFile"
            else race_copy
            for name in function_names
            )
        )
        powershell += r'''
$ErrorActionPreference = 'Stop'
$sourceRoot = Join-Path $FixtureRoot 'source'
$targetRoot = Join-Path $FixtureRoot 'target'
New-Item -ItemType Directory -Force -Path $sourceRoot, $targetRoot | Out-Null
[System.IO.File]::WriteAllText((Join-Path $sourceRoot 'a.txt'), 'reviewed')
[System.IO.File]::WriteAllText((Join-Path $targetRoot 'a.txt'), 'reviewed')

if ((Assert-ExactInstalledPrerequisiteTree -SourceRoot $sourceRoot -TargetRoot $targetRoot -Label 'fixture') -ne 1) {
    throw 'Exact prerequisite tree did not accept the identical fixture.'
}

[System.IO.File]::Delete((Join-Path $targetRoot 'a.txt'))
$missingRejected = $false
try { Assert-ExactInstalledPrerequisiteTree -SourceRoot $sourceRoot -TargetRoot $targetRoot -Label 'fixture' | Out-Null }
catch { $missingRejected = $true }
if (-not $missingRejected) { throw 'Missing prerequisite was accepted.' }

[System.IO.File]::WriteAllText((Join-Path $targetRoot 'a.txt'), 'stale')
$staleRejected = $false
try { Assert-ExactInstalledPrerequisiteTree -SourceRoot $sourceRoot -TargetRoot $targetRoot -Label 'fixture' | Out-Null }
catch { $staleRejected = $true }
if (-not $staleRejected) { throw 'Stale prerequisite was accepted.' }

[System.IO.File]::WriteAllText((Join-Path $targetRoot 'a.txt'), 'reviewed-compatible-prior')
$compatibleHash = (Get-FileHash -LiteralPath (Join-Path $targetRoot 'a.txt') -Algorithm SHA256).Hash.ToLowerInvariant()
$acceptedHash = Assert-ExactInstalledPrerequisiteFile `
    -SourceFile (Join-Path $sourceRoot 'a.txt') `
    -DestinationFile (Join-Path $targetRoot 'a.txt') `
    -Label 'compatible fixture' `
    -AdditionalAcceptedDestinationSha256 @($compatibleHash)
if ($acceptedHash -cne $compatibleHash -or
    [System.IO.File]::ReadAllText((Join-Path $targetRoot 'a.txt')) -cne 'reviewed-compatible-prior') {
    throw 'Explicit compatible prerequisite was not accepted read-only.'
}

[System.IO.File]::WriteAllText((Join-Path $targetRoot 'a.txt'), 'reviewed')
[System.IO.File]::WriteAllText((Join-Path $targetRoot 'unexpected.txt'), 'unknown')
$unexpectedRejected = $false
try { Assert-ExactInstalledPrerequisiteTree -SourceRoot $sourceRoot -TargetRoot $targetRoot -Label 'fixture' | Out-Null }
catch { $unexpectedRejected = $true }
if (-not $unexpectedRejected) { throw 'Unexpected prerequisite-tree file was accepted.' }
[System.IO.File]::Delete((Join-Path $targetRoot 'unexpected.txt'))

$pluginSource = Join-Path $FixtureRoot 'plugin-source.txt'
$pluginTarget = Join-Path $FixtureRoot 'plugin-target.txt'
[System.IO.File]::WriteAllText($pluginSource, 'reviewed-plugin')
[System.IO.File]::WriteAllText($pluginTarget, 'unknown-plugin')
$pluginCollisionRejected = $false
try { Assert-NewOrEqualHashFile -SourceFile $pluginSource -DestinationFile $pluginTarget }
catch { $pluginCollisionRejected = $true }
if (-not $pluginCollisionRejected -or [System.IO.File]::ReadAllText($pluginTarget) -cne 'unknown-plugin') {
    throw 'Unknown plugin preflight collision was not rejected and preserved.'
}

$raceTarget = Join-Path $FixtureRoot 'race-target.txt'
$raceRejected = $false
try { Copy-NewOrEqualHashFile -SourceFile $pluginSource -DestinationFile $raceTarget | Out-Null }
catch { $raceRejected = $true }
if (-not $raceRejected -or [System.IO.File]::ReadAllText($raceTarget) -cne 'intervening-unknown') {
    throw 'Post-absence publication collision was not rejected and preserved.'
}
if (@(Get-ChildItem -LiteralPath $FixtureRoot -Recurse -File -Filter '*.hero_v4_add_*.tmp').Count -ne 0) {
    throw 'Owned Hero-V4 publication stage residue remained.'
}
'HERO_V4_INSTALLER_FAIL_CLOSED_FIXTURES_PASS'
'''

        with tempfile.TemporaryDirectory(prefix="hero_v4_installer_contract_") as tmp:
            script_path = Path(tmp) / "installer_fixture.ps1"
            script_path.write_text(powershell, encoding="utf-8")
            result = subprocess.run(
                [
                    "powershell.exe",
                    "-NoLogo",
                    "-NoProfile",
                    "-NonInteractive",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(script_path),
                    "-FixtureRoot",
                    str(Path(tmp) / "fixture"),
                ],
                check=False,
                capture_output=True,
                text=True,
                timeout=30,
            )
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn("HERO_V4_INSTALLER_FAIL_CLOSED_FIXTURES_PASS", result.stdout)

    def test_16_reviewed_v3_diagnostic_compatibility_is_reconstructable(self) -> None:
        v3_source_path = (
            PLUGIN
            / "Source"
            / "TRIADSensorFusionEditor"
            / "Private"
            / "TRIADIstanaPublicViewHeroV3EditorLibrary.cpp"
        )
        current_payload = v3_source_path.read_bytes()
        self.assertEqual(329041, len(current_payload))
        self.assertEqual(
            "3c2f916845c421070e7a64c25ea95cd8de2fbea95800f4db166edd3a7177fdee",
            hashlib.sha256(current_payload).hexdigest(),
        )
        current_text = current_payload.decode("utf-8")
        reviewed_current_diagnostic = (
            'TEXT("Captured 3840x2160 Hero-V3 QA preset=%s path=\'%s\' '
            "bytes=%lld after %d settled TSR frames. Camera-local profile: "
            "D65 6500K, manual ISO100 1/125s, exposure bias 0 EV, ScreenSpace "
            "GI/reflection, no LUT/bloom/vignette/motion blur/grain; UE project "
            "filmic tonemapper remains unmodified and external display/OCIO is "
            "not certified. Scoped capture overrides r.AntiAliasingMethod=TSR, "
            "r.ScreenPercentage=100, r.TSR.History.ScreenPercentage=200, "
            "r.SSGI.Quality=4, and r.SSR.Quality=4 were applied, then every CVar "
            "was restored exactly to its pre-capture value. The scene-local "
            "transient camera followed world-origin rebasing; V1/V2/V3 maps, "
            "surroundings, project settings, and renderer config were not saved "
            'or mutated. %s")'
        )
        reviewed_compatible_diagnostic = (
            'TEXT("Captured 3840x2160 Hero-V3 QA preset=%s path=\'%s\' '
            "bytes=%lld after %d settled TSR frames. Camera-local profile: "
            "D65 6500K, manual ISO100 1/200s, exposure bias -0.65 EV, no "
            "LUT/bloom/vignette/motion blur/grain; UE project filmic tonemapper "
            "remains unmodified and external display/OCIO is not certified. "
            "Scoped r.AntiAliasingMethod=TSR, r.ScreenPercentage=100, and "
            "r.TSR.History.ScreenPercentage=200 were restored exactly. The "
            "scene-local transient camera followed world-origin rebasing; "
            "V1/V2/V3 maps, surroundings, project settings, and renderer config "
            'were not saved or mutated. %s")'
        )
        self.assertEqual(1, current_text.count(reviewed_current_diagnostic))
        self.assertEqual(0, current_text.count(reviewed_compatible_diagnostic))
        reconstructed = current_text.replace(
            reviewed_current_diagnostic,
            reviewed_compatible_diagnostic,
        ).encode("utf-8")
        self.assertEqual(328911, len(reconstructed))
        self.assertEqual(
            "f48379a6c236b65553d2ef41fcea2c8932b53cc7e138da5daa667aa2a92857a9",
            hashlib.sha256(reconstructed).hexdigest(),
        )
        # A single terminal success-report literal accounts for every byte of
        # the reviewed prior. No declaration, branch, call, validation gate,
        # parameter, capture setting, restoration action, or API differs.
        self.assertEqual(
            current_payload,
            reconstructed.replace(
                reviewed_compatible_diagnostic.encode("utf-8"),
                reviewed_current_diagnostic.encode("utf-8"),
            ),
        )

    def test_17_reviewed_v3_test_compatibility_is_reconstructable(self) -> None:
        v3_test_path = (
            PLUGIN
            / "Tests"
            / "test_istana_public_view_hero_v3_integration_contract.py"
        )
        current_payload = v3_test_path.read_bytes()
        self.assertEqual(78613, len(current_payload))
        self.assertEqual(
            "d040466ec9eda50afb94e2b210cd2ae6e964bfc72a91012df78c5b6388988746",
            hashlib.sha256(current_payload).hexdigest(),
        )
        current_text = current_payload.decode("utf-8")
        insertion_anchor = (
            '        self.assertNotIn("r.ReflectionMethod", quality_body)\n'
        )
        diagnostic_regression_block = (
            "        success_message_start = self.cpp.index(\n"
            "            'TEXT(\"Captured 3840x2160 Hero-V3 QA preset='\n"
            "        )\n"
            "        success_message_end = self.cpp.index('\")', success_message_start)\n"
            "        success_message = self.cpp[success_message_start:success_message_end]\n"
            "        for exact_diagnostic in (\n"
            '            "manual ISO100 1/125s",\n'
            '            "exposure bias 0 EV",\n'
            '            "ScreenSpace GI/reflection",\n'
            '            "r.SSGI.Quality=4",\n'
            '            "r.SSR.Quality=4",\n'
            '            "every CVar was restored exactly to its pre-capture value",\n'
            "        ):\n"
            "            self.assertIn(exact_diagnostic, success_message)\n"
            '        self.assertNotIn("1/200s", success_message)\n'
            '        self.assertNotIn("exposure bias -0.65 EV", success_message)\n'
        )
        self.assertEqual(1, current_text.count(insertion_anchor))
        self.assertEqual(1, current_text.count(diagnostic_regression_block))
        prior_text = current_text.replace(diagnostic_regression_block, "")
        prior_payload = prior_text.encode("utf-8")
        self.assertEqual(77873, len(prior_payload))
        self.assertEqual(
            "59543e0254b44d5baa6ca548e58767ddee64a54b4a53065653317aa289f577ac",
            hashlib.sha256(prior_payload).hexdigest(),
        )
        # Reinserting only the diagnostic assertion block after its unique
        # quality-method anchor reconstructs the current test byte-for-byte.
        self.assertEqual(
            current_payload,
            prior_text.replace(
                insertion_anchor,
                insertion_anchor + diagnostic_regression_block,
            ).encode("utf-8"),
        )


if __name__ == "__main__":
    unittest.main()
