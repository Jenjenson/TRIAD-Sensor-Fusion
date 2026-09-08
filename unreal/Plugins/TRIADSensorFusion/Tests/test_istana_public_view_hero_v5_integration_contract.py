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
    / "TRIADIstanaPublicViewHeroV5EditorLibrary.h"
)
SOURCE = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaPublicViewHeroV5EditorLibrary.cpp"
)
MATERIAL_ROOT = (
    REPO / "unreal" / "SourceAssets" / "IstanaPublicView" / "HeroMaterialsV5"
)
GEOMETRY_ROOT = REPO / "unreal" / "SourceAssets" / "IstanaPublicViewV5"
SCRIPTS_ROOT = REPO / "scripts"
INTEGRATION_FREEZE = (
    PLUGIN / "Resources" / "IstanaPublicViewHeroV5.integration.freeze.json"
)
ZERO_SHA256 = "0" * 64
FINAL_INPUT_PATHS = (
    MATERIAL_ROOT / "hero_materials_v5.contract.json",
    MATERIAL_ROOT / "unreal_asset_interface.v5.json",
    MATERIAL_ROOT / "hero_materials_v5.freeze.json",
    GEOMETRY_ROOT / "istana_public_view_hero_v5.contract.json",
    GEOMETRY_ROOT / "Generated" / "IstanaPublicViewV5Building.manifest.json",
    GEOMETRY_ROOT / "hero_v5.freeze.json",
    INTEGRATION_FREEZE,
)
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


class HeroV5IntegrationFrozenTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.source = SOURCE.read_text(encoding="utf-8")
        missing = [path for path in FINAL_INPUT_PATHS if not path.is_file()]
        if missing:
            raise AssertionError(
                "final V5 geometry/material/integration authority is incomplete: "
                + ", ".join(str(path) for path in missing)
            )
        cls.contract = json.loads(
            (MATERIAL_ROOT / "hero_materials_v5.contract.json").read_text(
                encoding="utf-8"
            )
        )
        cls.interface = json.loads(
            (MATERIAL_ROOT / "unreal_asset_interface.v5.json").read_text(
                encoding="utf-8"
            )
        )
        cls.material_freeze = json.loads(
            (MATERIAL_ROOT / "hero_materials_v5.freeze.json").read_text(
                encoding="utf-8"
            )
        )
        cls.geometry_contract = json.loads(
            (GEOMETRY_ROOT / "istana_public_view_hero_v5.contract.json").read_text(
                encoding="utf-8"
            )
        )
        cls.geometry_manifest = json.loads(
            (
                GEOMETRY_ROOT
                / "Generated"
                / "IstanaPublicViewV5Building.manifest.json"
            ).read_text(encoding="utf-8")
        )
        cls.geometry_freeze = json.loads(
            (GEOMETRY_ROOT / "hero_v5.freeze.json").read_text(encoding="utf-8")
        )
        cls.integration_freeze = json.loads(
            INTEGRATION_FREEZE.read_text(encoding="utf-8")
        )

    def test_01_additive_v5_editor_entry_points_exist(self) -> None:
        expected = [
            "ValidateIstanaPublicViewHeroV5RemoteControlProject",
            "ImportIstanaPublicViewHeroV5Assets",
            "ValidateIstanaPublicViewHeroV5Assets",
            "MigrateIstanaPublicViewExteriorMapToHeroV5",
            "ValidateIstanaPublicViewHeroV5Map",
            "ValidateIstanaPublicViewHeroV5PlayWorldReadiness",
            "CaptureIstanaPublicViewHeroV5QualityFrame",
            "QuiesceIstanaPublicViewHeroV5PlayWorldForStop",
        ]
        for name in expected:
            self.assertIn(name, self.header)
            self.assertIn(name, self.source)
        self.assertIn("UTRIADIstanaPublicViewHeroV5EditorLibrary", self.header)
        self.assertIn("TRIADSENSORFUSIONEDITOR_API", self.header)

    def test_02_namespaces_and_source_map_are_exactly_additive(self) -> None:
        exact_literals = [
            "/Game/Maps/Istana_PublicView_Exterior_v1",
            "/Game/Maps/Istana_PublicView_Exterior_v2",
            "/Game/Maps/Istana_PublicView_Exterior_v3",
            "/Game/Maps/Istana_PublicView_Exterior_v4",
            "/Game/Maps/Istana_PublicView_Exterior_v5",
            "/Game/TRIAD/IstanaPublicViewV5",
            "/Game/TRIAD/IstanaPublicView/HeroMaterialsV5",
            "/Game/TRIAD/IstanaPublicView/HeroMaterialsV3/Textures",
            "SourceAssets/IstanaPublicViewV5",
            "SourceAssets/IstanaPublicView/HeroMaterialsV5",
        ]
        for value in exact_literals:
            self.assertIn(value, self.source)
        self.assertNotIn("D:\\\\", self.source)
        self.assertNotIn("D:/", self.source)

    def test_03_final_v5_bindings_are_exact_and_fail_closed(self) -> None:
        frozen_bindings = {
            "FrozenFreezeSha256":
                "459f60e21da82422aeedd88325157ffc61d12f69846144f3f8f295f0b609ce75",
            "FrozenMaterialFreezeSha256":
                "1a3ec7696e25eb7da83cd53e60eed67531f6e225c430f189eab51ca37d66d69a",
            "FrozenMaterialContractSha256":
                "8ba78534eb7c91ba66815e5456d6b23b138d10961e84a4b35d81fcd3545bcc20",
            "FrozenMaterialInterfaceSha256":
                "f4f606d00d4e2fbf9033117c9e34c768d73287f3dce55ba234b2d905b3a7bb44",
            "FrozenMaterialValidatorSha256":
                "db6590043aa192d420cdca80cce538cfe725b3a4be0c0b8d1cec70792587ee2d",
            "FrozenMaterialEvaluatorSha256":
                "c26e9eec7f8550185d5b70c8d3b07405f8fc66e9fcb62547a029b9be1cdc26ca",
            "FrozenGeometryFreezeSha256":
                "653b5f6f73f9b8b6cd277ba1ba42d7863f4616b24d8e7300883e96bc21514928",
            "FrozenGeometryContractSha256":
                "842f4184137eca2a5e515ea0f339411735c10258e49d6673a8799d3b9e2ab9e0",
            "FrozenGeometryGeneratorSha256":
                "6526e7c22e1c2e9323bfe942a45bda39761de81fd902bf605c3f7327431ab676",
            "FrozenGeometryManifestSha256":
                "6a21cc263303b3b415fd6071e3a597a627ee2958e4e61ab81e4701308db64570",
            "FrozenGeometryManifestSemanticSha256":
                "2a7fe5fe9fc4847317d9fa05358bf9aecb0b0966bd6942805bda7372956dbf83",
            "FrozenGeometryObjSha256":
                "d9b37092401a70855d045bf28153293cd876ba748bc6e874096748c78d00eb4c",
            "FrozenGeometryAuditSha256":
                "02fef93ba2b49dc5ad78900adf3be73564a01e0db901366407d1e8313642247b",
            "FrozenGeometryWholeShellAuditSha256":
                "085d0726e79ba8a12d46378b2628ddf79606578c3e30c37e5ce8f785057bb721",
            "FrozenGeometrySparseClosureAuditSha256":
                "6bbd4dfb6f7245f97577a28318c204c99aa85fc4fa2f18f736ac66f7ba268839",
            "FrozenGeometryDenseClosureAuditSha256":
                "70d31d09d4532a81a7da70f95b7fac31abff1d5e315915846e52d0028cba58f0",
        }
        for constant, digest in frozen_bindings.items():
            self.assertRegex(
                self.source,
                rf'const FString {constant}\(\s*TEXT\("{digest}"\)\s*\);',
            )
        self.assertRegex(
            self.source,
            r"constexpr int32 ExpectedHeroTriangles = 831720;",
        )
        self.assertRegex(
            self.source,
            r"constexpr int32 ExpectedHeroVertices = 2495160;",
        )
        self.assertIn("constexpr int32 ExpectedHeroGroups = 28505;", self.source)
        self.assertIn("constexpr int64 ExpectedHeroBytes = 334592878;", self.source)
        self.assertTrue((MATERIAL_ROOT / "hero_materials_v5.freeze.json").is_file())
        self.assertTrue(INTEGRATION_FREEZE.is_file())

        complete_source = function_body(self.source, "ValidateCompleteV5SourceSet")
        self.assertLess(
            complete_source.index("ValidateIntegrationFreeze"),
            complete_source.index("ValidateGeometrySource"),
        )
        self.assertLess(
            complete_source.index("ValidateIntegrationFreeze"),
            complete_source.index("ValidateMaterialSource"),
        )

        for public_name in (
            "ImportIstanaPublicViewHeroV5Assets",
            "MigrateIstanaPublicViewExteriorMapToHeroV5",
            "ValidateIstanaPublicViewHeroV5Assets",
            "ValidateIstanaPublicViewHeroV5Map",
        ):
            definition_start = self.source.rfind(f"\n    {public_name}(")
            self.assertGreater(definition_start, 0)
            body = function_body(self.source[definition_start:], public_name)
            self.assertIn("ValidateCompleteV5SourceSet", body)
        readiness = function_body(
            self.source, "ValidateIstanaPublicViewHeroV5PlayWorldReadiness"
        )
        capture = function_body(
            self.source, "CaptureIstanaPublicViewHeroV5QualityFrame"
        )
        quiesce = function_body(
            self.source, "QuiesceIstanaPublicViewHeroV5PlayWorldForStop"
        )
        self.assertIn("ValidateIstanaPublicViewHeroV5Map", readiness)
        self.assertIn("ValidateIstanaPublicViewHeroV5PlayWorldReadiness", capture)
        self.assertIn("ValidateIntegrationFreeze", quiesce)

    def test_04_v3_textures_are_read_only_references_not_v5_payloads(self) -> None:
        import_body = function_body(self.source, "ImportHeroV5AssetSet")
        self.assertIn("LoadObject<UTexture2D>", import_body)
        self.assertIn("ValidateHeroTexture", import_body)
        self.assertIn("V5_IMPORT_REFUSED_FROZEN_V3_TEXTURE_DEPENDENCY", import_body)
        self.assertNotIn("AssetsToSave.Add(Texture)", import_body)
        self.assertNotIn("Texture->Modify", import_body)
        self.assertNotIn("Texture->PostEditChange", import_body)
        self.assertNotIn("Texture->MarkPackageDirty", import_body)
        self.assertNotIn("TextureTasks", import_body)
        self.assertEqual(1, import_body.count("ImportAssetTasks("))
        self.assertIn("HeroTasks", import_body)

        expected_paths_body = function_body(self.source, "BuildExpectedAssetPaths")
        self.assertNotIn("TextureObjectPath", expected_paths_body)
        inspect_body = function_body(self.source, "InspectHeroV5Assets")
        self.assertIn("ForbiddenV5TextureObjectPath", inspect_body)
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
            "/Game/TRIAD/IstanaPublicViewV5/Building/"
            "SM_IstanaPublicViewV5_Building_Hero."
            "SM_IstanaPublicViewV5_Building_Hero",
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
                for row in self.interface["specialSlotBindingsV5"].values()
            ),
        }
        self.assertEqual(14, len(expected))

        def classify(actual: set[str]) -> str:
            if not actual:
                return "Absent"
            return "CompleteValid" if actual == expected else "PartialOrInvalid"

        self.assertEqual("Absent", classify(set()))
        self.assertEqual("CompleteValid", classify(expected.copy()))
        for binding in self.interface["specialSlotBindingsV5"].values():
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
                    "/Game/TRIAD/IstanaPublicViewV5/Bogus/"
                    "SM_Bogus.SM_Bogus",
                }
            ),
        )

    def test_04a_door_is_distinct_and_reuses_only_frozen_v3_trim(self) -> None:
        bindings = function_body(self.source, "ExpectedSlotInstances")
        parser = function_body(self.source, "ParseMaterialInterface")
        texture_binding = function_body(
            self.source, "HasExactV3ToV5TextureBinding"
        )
        create_instance = function_body(self.source, "CreateHeroMaterialInstance")
        validate_instance = function_body(self.source, "ValidateHeroMaterialInstance")
        validate_material_source = function_body(self.source, "ValidateMaterialSource")
        self.assertIn(
            '{TEXT("M_IPV5_Door"), TEXT("MI_IPV_Hero_Door_V5")}',
            bindings,
        )
        self.assertIn('{TEXT("Door"), TEXT("Trim")}', parser)
        self.assertIn('TextureSurfaceUseCounts.FindRef(TEXT("Trim")) != 2', parser)
        self.assertNotIn('TextureSurfaceUseCounts.FindRef(TEXT("Timber")) != 2', parser)
        self.assertIn('V3Slot != TEXT("M_IPV3_Door")', texture_binding)
        self.assertIn('ExpectedV5.Add(TEXT("M_IPV5_Door"))', texture_binding)
        for body in (create_instance, validate_instance):
            self.assertIn(
                "Spec.TextureSurfaceIdV3, Binding.Key, Pack, Textures", body
            )
            self.assertNotIn("Spec.Id, Binding.Key, Pack, Textures", body)
        self.assertIn("semanticV4ToV5SlotMapping", validate_material_source)
        self.assertNotIn("semanticV3ToV5SlotMapping", validate_material_source)
        self.assertNotIn(
            "preservedTriangleV3ToV5SlotMapping", validate_material_source
        )
        self.assertIn("freezeStatus", validate_material_source)
        self.assertIn("geometryBindingStatus", validate_material_source)
        self.assertIn("geometryAuditSha256", validate_material_source)
        self.assertIn("heroObjGroups", validate_material_source)

    def test_05_material_local_glass_ssr_is_created_validated_and_sealed(self) -> None:
        glass_body = function_body(self.source, "CreateGlassHeroMaster")
        opaque_body = function_body(self.source, "CreateOpaqueHeroMaster")
        graph_body = function_body(self.source, "ComputeHeroMaterialGraphSha256")
        validator_body = function_body(self.source, "ValidateHeroMaster")
        self.assertIn("Material->bScreenSpaceReflections = true", glass_body)
        self.assertIn("Material->bScreenSpaceReflections = false", opaque_body)
        self.assertIn(
            "Material->bUsedWithNanite = true;\n"
            "    UMaterialEditingLibrary::RecompileMaterial(Material);",
            opaque_body,
        )
        self.assertNotIn("bUsedWithNanite", glass_body)
        self.assertIn("Material->bScreenSpaceReflections", graph_body)
        self.assertIn("Material->bScreenSpaceReflections != bGlass", validator_body)
        self.assertIn(
            "(!bGlass && !Material->GetUsageByFlag(MATUSAGE_Nanite))",
            validator_body,
        )
        self.assertIn("triad.istana_hero_surface_graph.v5.1", self.source)
        self.assertIn("triad.istana_hero_glass_graph.v5.2", self.source)
        self.assertNotRegex(
            glass_body,
            r"r\.SSR|IConsoleManager|DefaultEngine\.ini|ShaderModel6",
        )

    def test_06_material_contract_is_v3_texture_reference_only(self) -> None:
        scope = self.contract["scope"]
        texture_refs = self.interface["textureReferences"]
        interface_parser = function_body(self.source, "ParseMaterialInterface")
        integration_status_match = re.search(
            r'TEXT\("integrationStatus"\),\s*TEXT\("([^"]+)"\)',
            interface_parser,
        )
        self.assertIsNotNone(integration_status_match)
        self.assertEqual(
            self.interface["integrationStatus"],
            integration_status_match.group(1),
        )
        self.assertEqual("triad.istana_hero_materials.v5", self.contract["schema"])
        self.assertEqual("5.0.0", self.contract["version"])
        self.assertEqual(
            "triad.istana_hero_material_unreal_interface.v5",
            self.interface["schema"],
        )
        self.assertEqual(0, scope["generatedOrCopiedTextureCountV5"])
        self.assertEqual(57, scope["referencedFrozenTextureCountV3"])
        self.assertFalse(scope["changesFrozenV3TexturePixels"])
        self.assertFalse(scope["changesFrozenV3TexturePackages"])
        self.assertEqual(0, texture_refs["v5TexturePackageCount"])
        self.assertEqual(57, texture_refs["v3TextureReferenceCount"])
        self.assertIsNone(texture_refs["destinationTextureRoot"])
        self.assertTrue(texture_refs["noV5TextureCopy"])
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
        importer = function_body(self.source, "ImportHeroV5AssetSet")
        self.assertLess(
            importer.index("NormalizeImportedHeroMaterialSlots"),
            importer.index("Hero->SetMaterial"),
        )
        self.assertLess(
            importer.index("NormalizeImportedHeroMaterialSlots"),
            importer.index("SaveLoadedAssets"),
        )

    def test_08_migration_preserves_v1_through_v4_and_swaps_only_hero(self) -> None:
        migrate = function_body(self.source, "MigrateMapToV5")
        self.assertIn("ProtectedV1MapPackage", migrate)
        self.assertIn("ProtectedV2MapPackage", migrate)
        self.assertIn("ProtectedV3MapPackage", migrate)
        self.assertIn("SourceMapPackage", migrate)
        self.assertIn("ProtectedV1ShaBefore", migrate)
        self.assertIn("ProtectedV2ShaBefore", migrate)
        self.assertIn("ProtectedV3ShaBefore", migrate)
        self.assertIn("SourceV4ShaBefore", migrate)
        self.assertIn("ProtectedV1ShaAfter", migrate)
        self.assertIn("ProtectedV2ShaAfter", migrate)
        self.assertIn("ProtectedV3ShaAfter", migrate)
        self.assertIn("SourceV4ShaAfter", migrate)
        self.assertEqual(1, migrate.count("SetStaticMesh(Hero)"))
        self.assertEqual(1, migrate.count("EmptyOverrideMaterials"))
        self.assertIn("ValidateSurroundingsUnchanged", migrate)
        self.assertIn("ExpectedPreservedTreeInstanceCount = 720", self.source)
        self.assertIn("preservedTreeInstanceCount", self.source)
        self.assertIn("ValidateWorldForV5", migrate)
        self.assertLess(migrate.index("ValidateWorldForV5"), migrate.index("SaveLoadedAsset"))
        self.assertIn("IDEMPOTENT_V5_MIGRATION_ALREADY_COMPLETE", migrate)
        self.assertIn("ValidateIstanaPublicViewHeroV5Map", migrate)
        self.assertLess(
            migrate.index("IDEMPOTENT_V5_MIGRATION_ALREADY_COMPLETE"),
            migrate.index("SetStaticMesh(Hero)"),
        )

        source_validator = function_body(
            self.source, "ValidateExactV4SourceHeroComponent"
        )
        self.assertNotRegex(
            source_validator,
            r"Modify\(|PostEditChange|SetMaterial|SetStaticMesh|MarkPackageDirty",
        )

    def test_09_neutral_capture_and_quality_overrides_restore_exactly(self) -> None:
        configure = function_body(self.source, "ConfigureHeroV5D65CaptureCamera")
        camera_gate = function_body(self.source, "HasHeroV5D65CaptureCamera")
        quality = function_body(self.source, "FScopedHeroV5TsrCaptureQuality final")
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
        capture = function_body(
            self.source, "CaptureIstanaPublicViewHeroV5QualityFrame"
        )
        self.assertIn('Preset == TEXT("HERO_GROUND_DETAIL")', capture)
        self.assertIn("FVector(0.0, 8200.0, 170.0)", capture)
        self.assertIn("FVector(0.0, 5200.0, 170.0)", capture)
        self.assertIn('Preset == TEXT("HERO_MATERIAL_DETAIL")', capture)
        self.assertIn("FVector(0.0, 4000.0, 170.0)", capture)
        self.assertIn("FVector(0.0, 2280.0, 420.0)", capture)
        self.assertIn("exactly 17.20 m from that face", capture)
        self.assertIn("RF_Transient", capture)
        self.assertIn("CaptureCamera->Destroy()", capture)

    def test_10_recess_policy_is_narrow_and_geometry_binding_is_held(self) -> None:
        recess = self.interface["specialSlotBindingsV5"]["M_IPV5_Recess"]
        self.assertEqual(
            "NEAR BLACK ONLY FOR NARROW TERMINAL JOINERY SEAMS; NEVER BROAD OPENING BACKDROPS",
            recess["surfaceIntent"],
        )
        scope = self.geometry_contract["scope"]
        self.assertTrue(scope["rebuildsAllElevationUpperTowerFromLawfulPublicProportions"])
        self.assertTrue(
            scope["rebuildsCentralRoofMansardDormersFromLawfulPublicProportions"]
        )
        self.assertFalse(scope["oneToOneDigitalTwinClaimed"])
        self.assertFalse(scope["surveyAccuracyClaimed"])
        semantic_policy = self.geometry_contract["materialSemanticPolicy"]
        self.assertEqual(
            "M_IPV5_Door",
            semantic_policy["retainedDeepDoorClosureFieldRole"],
        )
        self.assertEqual(
            "M_IPV5_Door",
            semantic_policy["authoredUpperTowerOpaqueScreenBackingRole"],
        )
        self.assertFalse(semantic_policy["broadAuthoredOpalineBackingAllowed"])
        self.assertEqual(
            93,
            self.geometry_manifest["baselinePreservation"]
            ["retainedDeepDoorClosureFieldGroupCount"],
        )
        self.assertEqual(
            1116,
            self.geometry_manifest["baselinePreservation"]
            ["retainedDeepDoorClosureFieldTriangleCount"],
        )
        self.assertEqual(
            0,
            self.geometry_manifest["featureCounts"]
            ["v5AuditAuthoredOpalineTriangleCount"],
        )
        self.assertEqual(
            15,
            self.geometry_manifest["featureCounts"]["v5TerminalRecessSeams"],
        )
        self.assertEqual(
            0,
            self.geometry_manifest["qualityAudit"]
            ["transparentFirstOpaqueMissingCount"],
        )
        validate_geometry = function_body(self.source, "ValidateGeometrySource")
        for stale in (
            "qualityGateSourceBytes",
            "detailMetrics",
            "requiredGeometry",
            "materialInterface",
            "preservedV3BroadRecess",
            "central_closure_audit.py",
            "v5_winding_audit.py",
        ):
            self.assertNotIn(stale, validate_geometry)

    def test_11_freeze_role_roster_has_no_v2_v3_clone_path_residue(self) -> None:
        self.assertEqual(
            "/Game/Maps/Istana_PublicView_Exterior_v4",
            self.integration_freeze["sourceMap"],
        )
        roles = {
            record["role"]: record["relativePath"]
            for array_name in ("files", "protectedV1Files", "repositoryScripts")
            for record in self.integration_freeze[array_name]
        }
        self.assertEqual(
            "SourceAssets/IstanaPublicViewV5/v5_geometry_audit.py",
            roles["GEOMETRY_AUDIT"],
        )
        self.assertEqual(
            "SourceAssets/IstanaPublicViewV5/v5_whole_shell_audit.py",
            roles["GEOMETRY_WHOLE_SHELL_AUDIT"],
        )
        self.assertEqual(
            "SourceAssets/IstanaPublicViewV5/validate_hero_v5.py",
            roles["GEOMETRY_VALIDATOR"],
        )
        self.assertEqual(
            "SourceAssets/IstanaPublicViewV3/facade_closure_audit.py",
            roles["PROTECTED_V3_SPARSE_CLOSURE_AUDIT"],
        )
        self.assertEqual(
            "SourceAssets/IstanaPublicViewV3/dense_facade_closure_audit.py",
            roles["PROTECTED_V3_DENSE_CLOSURE_AUDIT"],
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
            "SourceAssets/IstanaPublicViewV5/central_closure_audit.py",
            "SourceAssets/IstanaPublicViewV5/v5_winding_audit.py",
            "SourceAssets/IstanaPublicViewV5/facade_closure_audit.py",
            "SourceAssets/IstanaPublicViewV5/dense_facade_closure_audit.py",
            "SourceAssets/IstanaPublicViewV5/slate_winding_audit.py",
            "SourceAssets/IstanaPublicView/HeroMaterialsV5/Source/source_manifest.json",
            "SourceAssets/IstanaPublicView/HeroMaterialsV5/Generated/manifest.json",
            "SourceAssets/IstanaPublicView/HeroMaterialsV5/build_hero_materials_v5.py",
        )
        self.assertFalse(set(forbidden) & set(roles.values()))
        validate_geometry = function_body(self.source, "ValidateGeometrySource")
        for stale_field in (
            "qualityGateSourceBytes",
            "detailMetrics",
            "requiredGeometry",
            "v5CentralDenseClosureProbeCount",
            "v5RoofWindingTrianglesAudited",
            "preservedV3BroadRecessGroupsReclassified",
        ):
            self.assertNotIn(stale_field, validate_geometry)
        for exact_field in (
            "publicViewClosureNearestSurfaceSemanticDistanceSha256",
            "intentionalForegroundCanonicalFullPathSha256",
            "roofSilhouetteBoundarySemanticDistanceSha256",
            "dormerRaisedObliqueLayeringRawPathSha256",
            "roofDrainageLiteralPathSha256",
            "wholeShellEquivalentProbeCount",
            "rebuildsAllElevationUpperTowerFromLawfulPublicProportions",
        ):
            self.assertIn(exact_field, validate_geometry)

    def test_12_final_material_freeze_is_geometry_bound_and_reviewed(self) -> None:
        self.assertEqual(
            "FROZEN_AFTER_EXACT_V5_GEOMETRY_V3_TEXTURE_SEMANTIC_AND_NEGATIVE_CONTROL_VALIDATION",
            self.material_freeze["freezeStatus"],
        )
        binding = self.interface["geometryBinding"]
        dependencies = self.material_freeze["geometryDependencies"]
        self.assertEqual(
            self.interface["geometryBinding"]["geometryFreezeSha256"],
            dependencies["geometryFreeze"]["sha256"],
        )
        self.assertEqual(
            self.geometry_freeze["auditSha256"],
            binding["geometryAuditSha256"],
        )
        self.assertEqual(28505, binding["heroObjGroups"])
        self.assertEqual(
            self.geometry_manifest["sourceClass"],
            binding["sourceGeometryGeneration"],
        )
        self.assertEqual(
            "M_IPV5_Door", binding["retainedDeepDoorClosureFieldV5Material"]
        )
        self.assertEqual(
            "M_IPV5_Door", binding["authoredUpperTowerOpaqueScreenBackingRole"]
        )
        self.assertFalse(binding["broadAuthoredOpalineBackingAllowed"])
        self.assertEqual(15, binding["v5TerminalRecessSeamFeatureCount"])
        self.assertFalse(binding["nearBlackOpeningSizedCardsAllowed"])
        negative = self.material_freeze["neutralCaptureNegativeControl"]
        self.assertTrue(negative["baselineReproductionPassed"])
        self.assertFalse(negative["candidateAcceptancePassed"])
        self.assertEqual(7, len(negative["expectedFailedGates"]))

    def test_13_integration_freeze_records_are_exact_unique_and_complete(self) -> None:
        freeze_bytes = INTEGRATION_FREEZE.read_bytes()
        self.assertEqual(12456, len(freeze_bytes))
        self.assertEqual(
            "459f60e21da82422aeedd88325157ffc61d12f69846144f3f8f295f0b609ce75",
            hashlib.sha256(freeze_bytes).hexdigest(),
        )
        self.assertEqual(
            "/Game/Maps/Istana_PublicView_Exterior_v4",
            self.integration_freeze["sourceMap"],
        )
        self.assertEqual(
            "/Game/Maps/Istana_PublicView_Exterior_v5",
            self.integration_freeze["destinationMap"],
        )
        self.assertFalse(self.integration_freeze["oneToOneDigitalTwinClaimed"])
        self.assertFalse(self.integration_freeze["surveyAccuracyClaimed"])
        self.assertFalse(self.integration_freeze["photogrammetryClaimed"])
        self.assertEqual(
            [
                "HERO_FRONT",
                "HERO_OBLIQUE_RIGHT",
                "HERO_OBLIQUE_LEFT",
                "HERO_FACADE_MACRO",
                "HERO_GROUND_DETAIL",
                "HERO_MATERIAL_DETAIL",
                "HERO_ORBIT_RIGHT",
                "HERO_ORBIT_LEFT",
            ],
            self.integration_freeze["qualityCapturePresets"],
        )
        rooted_records = [
            (REPO if array_name == "repositoryScripts" else REPO / "unreal", record)
            for array_name in ("files", "protectedV1Files", "repositoryScripts")
            for record in self.integration_freeze[array_name]
        ]
        records = [record for _, record in rooted_records]
        self.assertEqual(len(records), len({record["role"] for record in records}))
        self.assertEqual(
            len(records), len({record["relativePath"] for record in records})
        )
        expected_roles = {
            "GEOMETRY_FREEZE",
            "GEOMETRY_CONTRACT",
            "GEOMETRY_GENERATOR",
            "GEOMETRY_AUDIT",
            "GEOMETRY_WHOLE_SHELL_AUDIT",
            "GEOMETRY_VALIDATOR",
            "GEOMETRY_README",
            "GEOMETRY_CONTRACT_TEST",
            "GEOMETRY_MUTATION_TEST",
            "PROTECTED_V3_SPARSE_CLOSURE_AUDIT",
            "PROTECTED_V3_DENSE_CLOSURE_AUDIT",
            "GEOMETRY_MANIFEST",
            "BUILDING_HERO_VISUAL_V5",
            "HERO_MATERIAL_FREEZE",
            "HERO_MATERIAL_CONTRACT",
            "HERO_MATERIAL_UNREAL_INTERFACE",
            "HERO_MATERIAL_VALIDATOR",
            "HERO_MATERIAL_EVALUATOR",
            "HERO_MATERIAL_README",
            "HERO_MATERIAL_CONTRACT_TEST",
            "HERO_MATERIAL_SOURCE_MANIFEST_V3",
            "HERO_MATERIAL_GENERATED_MANIFEST_V3",
            "HERO_MATERIAL_BUILDER_V3",
            "PROTECTED_V3_MATERIAL_CONTRACT",
            "PROTECTED_V3_MATERIAL_UNREAL_INTERFACE",
            "HERO_V3_V5_VISUAL_ACCEPTANCE_SETTINGS",
            "PUBLIC_REFERENCE_MANIFEST",
            "PROTECTED_V4_GEOMETRY_FREEZE",
            "PROTECTED_V4_MATERIAL_FREEZE",
            "PROTECTED_V4_INTEGRATION_FREEZE",
            "PROTECTED_V3_GEOMETRY_FREEZE",
            "PROTECTED_V3_INTEGRATION_FREEZE",
            "PROTECTED_V2_GEOMETRY_FREEZE",
            "PROTECTED_V2_INTEGRATION_FREEZE",
            "V5_QUALITY_CAPTURE_SCRIPT",
            "V5_ASSET_IMPORT_SCRIPT",
            "V5_DEVELOPMENT_ASSET_INSTALL_SCRIPT",
            "V5_MAP_MIGRATION_SCRIPT",
            "V5_PIE_SETUP_SCRIPT",
            "V5_EDITOR_START_SCRIPT",
            "PROTECTED_V1_GEOMETRY_KERNEL",
            "PROTECTED_V1_BUILDING_GENERATOR",
            "PROTECTED_V1_CONTEXT_GENERATOR",
            "PROTECTED_V1_CONTRACT",
        }
        self.assertEqual(expected_roles, {record["role"] for record in records})
        for root, record in rooted_records:
            path = root / Path(record["relativePath"])
            payload = path.read_bytes()
            self.assertEqual(record["bytes"], len(payload), record["role"])
            self.assertEqual(
                record["sha256"],
                hashlib.sha256(payload).hexdigest(),
                record["role"],
            )

    def test_14_v5_scripts_reuse_v3_profile_and_publish_additively(self) -> None:
        names = (
            "Install-IstanaPublicViewHeroV5DevelopmentAssets.ps1",
            "Import-IstanaPublicViewHeroV5Assets.ps1",
            "Migrate-IstanaPublicViewHeroV5Map.ps1",
            "Start-IstanaPublicViewHeroV5Editor.ps1",
            "Set-IstanaPublicViewHeroV5PlayInEditor.ps1",
            "Capture-IstanaPublicViewHeroV5QualityPreviews.ps1",
        )
        scripts = {
            name: (SCRIPTS_ROOT / name).read_text(encoding="utf-8")
            for name in names
        }
        combined = "\n".join(scripts.values())
        self.assertNotIn("D:\\", combined)
        self.assertNotIn("D:/", combined)
        self.assertNotIn(
            "IstanaPublicViewHeroV5VisualAcceptance.settings.json", combined
        )
        self.assertNotIn("ISTANA_PUBLIC_VIEW_HERO_V5_VISUAL_ACCEPTANCE_ONLY", combined)
        self.assertNotIn("V5-only no-lidar/RPC-off process file", self.source)
        self.assertIn(
            "IstanaPublicViewHeroV3VisualAcceptance.settings.json", combined
        )
        self.assertIn("ISTANA_PUBLIC_VIEW_HERO_V3_VISUAL_ACCEPTANCE_ONLY", combined)

        exact_geometry_validator_invocation = re.compile(
            r"&\s+\$PythonExecutable\s+-B\s+"
            r"\(Join-Path\s+\$GeometryRoot\s+'validate_hero_v5\.py'\)\s*`\s*"
            r"--generated\s+\(Join-Path\s+\$GeometryRoot\s+'Generated'\)\s*`\s*"
            r"--require-frozen"
        )
        for name in (
            "Install-IstanaPublicViewHeroV5DevelopmentAssets.ps1",
            "Import-IstanaPublicViewHeroV5Assets.ps1",
            "Migrate-IstanaPublicViewHeroV5Map.ps1",
        ):
            validation = function_body(
                scripts[name], "function Invoke-FailClosedSourceValidation"
            )
            self.assertRegex(validation, exact_geometry_validator_invocation, name)
            self.assertEqual(1, validation.count("validate_hero_v5.py"), name)
            for retired_flag in ("--root", "--output", "--freeze"):
                self.assertNotIn(retired_flag, validation, name)

        installer = scripts[
            "Install-IstanaPublicViewHeroV5DevelopmentAssets.ps1"
        ]
        self.assertIn("Install-IstanaPublicViewHeroV4DevelopmentAssets.ps1", installer)
        self.assertIn(
            "f042a5257c6759d22bca4092594c8e28a175347c39d09dba14ab36f77542391a",
            installer,
        )
        self.assertIn("Copy-NewOrEqualHashFile", installer)
        self.assertIn("[System.IO.File]::Move($stageFile, $DestinationFile)", installer)
        self.assertNotIn("@(& $baseInstaller", installer)
        self.assertNotRegex(installer, r"(?m)^\s*&\s*\$baseInstaller\b")
        self.assertIn("V4PrerequisiteInstallerCalled = $false", installer)
        self.assertIn(
            "V3V4SourceAndProfilePrerequisitesByteExact = $true", installer
        )
        self.assertIn(
            "V4PluginPrerequisitesCurrentOrReviewedCompatible = $true",
            installer,
        )
        self.assertIn(
            "ReviewedCompatibleV3PluginPrerequisiteCount = ", installer
        )
        self.assertIn(
            "V4PluginPrerequisitesOverwritten = $false", installer
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
        self.assertIn("ImportedV4PackageRosterPresent = $true", installer)
        self.assertIn("ImportedV3TexturePackageRosterPresent = $true", installer)
        self.assertIn(
            "ImportedV4SemanticValidationDeferredToV5EditorGate = $true",
            installer,
        )
        self.assertIn("FreshProjectRequiresSeparateV4Install = $true", installer)
        self.assertIn("Assert-ExactInstalledPrerequisiteTree", installer)
        self.assertIn("Assert-ExactInstalledPrerequisiteFile", installer)
        self.assertIn("Assert-ExactInstalledHeroV3TextureRoster", installer)
        self.assertIn("$targetV3Map", installer)
        self.assertIn("$targetV3Mesh", installer)
        self.assertIn("$targetV3TextureRoot", installer)
        self.assertIn("$targetV4Map", installer)
        self.assertIn("$targetV4Mesh", installer)
        self.assertIn("Assert-ExactInstalledHeroV4MaterialRoster", installer)
        for digest in (
            "34ba790e50b87d495c2eff41aa4afbc2798af8524dd891740dccab466df973f5",
            "bafd7fe66f97bdfa66a1595b462903f5ff943d4138dfecc92f6af355e65db497",
            "533c719b4ab5566bd10d0b8158ed05952b13f1d20ae2c7e8dbe2f705a8446678",
            "b609f062d570887257e5d8279ed2d2787ee2551d48c9ddd6a0690acb81735ad7",
            "c6afedad2ca868be455a5425ab02114fb1584d237700c9fba489b595be650f12",
        ):
            self.assertIn(digest, installer)
        self.assertIn("Invoke-FailClosedV4PrerequisiteValidation", installer)
        self.assertIn("ReviewedV4PluginPinCount = ", installer)
        self.assertIn("exact 57-name frozen-contract Hero-V3 texture", installer)
        self.assertIn(
            "first run Install-IstanaPublicViewHeroV4DevelopmentAssets.ps1",
            installer,
        )
        self.assertIn("RecognizedPriorHashReplacementUsed = $false", installer)
        self.assertIn("-B (Join-Path $GeometryRoot 'validate_hero_v5.py')", installer)
        self.assertIn("validate_hero_materials_v5.py", installer)
        self.assertNotIn("build_hero_materials_v5.py", installer)

        publish = installer[installer.index("$protectedBefore =") :]
        geometry_copy = publish.index("foreach ($sourceFile in $geometryFiles)")
        material_copy = publish.index("foreach ($sourceFile in $materialFiles)")
        installed_validation = publish.index(
            "# Validate the exact installed V5 source trees"
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
                    installer[installer.index("$v5PluginRelativePaths = @(") : installer.index(")\n$requiredSourcePaths")],
                )
            ),
        )

        start = scripts["Start-IstanaPublicViewHeroV5Editor.ps1"]
        self.assertIn("Istana_PublicView_Exterior_v4.umap", start)
        self.assertIn("Istana_PublicView_Exterior_v5.umap", start)
        self.assertIn("ValidateIstanaPublicViewHeroV4Map", start)
        self.assertNotIn("ValidateIstanaPublicViewHeroV3Map", start)

        migrate = scripts["Migrate-IstanaPublicViewHeroV5Map.ps1"]
        self.assertIn("$report.sourceMapPackage", migrate)
        self.assertIn("/Game/Maps/Istana_PublicView_Exterior_v4", migrate)
        self.assertIn("/Game/Maps/Istana_PublicView_Exterior_v5", migrate)
        self.assertIn("$report.protectedV3MapPackage", migrate)
        self.assertIn("$report.preservedTreeInstanceCount", migrate)
        self.assertIn("$destinationExistedBefore", migrate)
        self.assertIn("IdempotentExistingDestination", migrate)
        self.assertNotIn("Refusing to overwrite existing v5 destination map", migrate)

        capture = scripts["Capture-IstanaPublicViewHeroV5QualityPreviews.ps1"]
        self.assertEqual(8, capture.count("Preset = 'HERO_"))
        self.assertIn("Preset = 'HERO_GROUND_DETAIL'", capture)
        self.assertIn("Preset = 'HERO_MATERIAL_DETAIL'", capture)
        self.assertIn("17.20 m from the public face", capture)
        self.assertIn("V4MapSha256Before", capture)
        self.assertIn("V4MapSha256After", capture)
        self.assertIn("VisualProfileSha256Before", capture)
        self.assertIn("VisualProfileSha256After", capture)
        self.assertIn("RendererConfigSha256Before", capture)
        self.assertIn("RendererConfigSha256After", capture)
        self.assertIn("Config\\DefaultEngine.ini", capture)

    def test_15_installer_prerequisites_and_publication_are_fail_closed(self) -> None:
        installer_path = (
            SCRIPTS_ROOT / "Install-IstanaPublicViewHeroV5DevelopmentAssets.ps1"
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
if (@(Get-ChildItem -LiteralPath $FixtureRoot -Recurse -File -Filter '*.hero_v5_add_*.tmp').Count -ne 0) {
    throw 'Owned Hero-V5 publication stage residue remained.'
}
'HERO_V5_INSTALLER_FAIL_CLOSED_FIXTURES_PASS'
'''

        with tempfile.TemporaryDirectory(prefix="hero_v5_installer_contract_") as tmp:
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
        self.assertIn("HERO_V5_INSTALLER_FAIL_CLOSED_FIXTURES_PASS", result.stdout)

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
