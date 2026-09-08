import hashlib
import json
import re
import shutil
import subprocess
import unittest
from pathlib import Path


PLUGIN_ROOT = Path(__file__).resolve().parents[1]
UNREAL_ROOT = PLUGIN_ROOT.parent.parent
REPOSITORY_ROOT = UNREAL_ROOT.parent
GEOMETRY_ROOT = UNREAL_ROOT / "SourceAssets" / "IstanaPublicViewV3"
MATERIAL_ROOT = (
    UNREAL_ROOT / "SourceAssets" / "IstanaPublicView" / "HeroMaterialsV3"
)
CPP_PATH = (
    PLUGIN_ROOT
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaPublicViewHeroV3EditorLibrary.cpp"
)
HEADER_PATH = (
    PLUGIN_ROOT
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Public"
    / "TRIADIstanaPublicViewHeroV3EditorLibrary.h"
)
BUILD_CS_PATH = (
    PLUGIN_ROOT
    / "Source"
    / "TRIADSensorFusionEditor"
    / "TRIADSensorFusionEditor.Build.cs"
)
INTEGRATION_FREEZE_PATH = (
    PLUGIN_ROOT
    / "Resources"
    / "IstanaPublicViewHeroV3.integration.freeze.json"
)
GITIGNORE_PATH = REPOSITORY_ROOT / ".gitignore"
LEGACY_VISUAL_SETTINGS_PATH = (
    UNREAL_ROOT / "Config" / "IstanaVisualAcceptance.settings.json"
)
HERO_V3_VISUAL_SETTINGS_PATH = (
    UNREAL_ROOT
    / "Config"
    / "IstanaPublicViewHeroV3VisualAcceptance.settings.json"
)
SCRIPT_PATHS = {
    "install": REPOSITORY_ROOT
    / "scripts"
    / "Install-IstanaPublicViewHeroV3DevelopmentAssets.ps1",
    "import": REPOSITORY_ROOT
    / "scripts"
    / "Import-IstanaPublicViewHeroV3Assets.ps1",
    "migrate": REPOSITORY_ROOT
    / "scripts"
    / "Migrate-IstanaPublicViewHeroV3Map.ps1",
    "pie": REPOSITORY_ROOT
    / "scripts"
    / "Set-IstanaPublicViewHeroV3PlayInEditor.ps1",
    "capture": REPOSITORY_ROOT
    / "scripts"
    / "Capture-IstanaPublicViewHeroV3QualityPreviews.ps1",
    "start": REPOSITORY_ROOT
    / "scripts"
    / "Start-IstanaPublicViewHeroV3Editor.ps1",
}

ORDERED_SLOTS = [
    "M_IPV3_DarkTimber",
    "M_IPV3_Door",
    "M_IPV3_Glass",
    "M_IPV3_Metal",
    "M_IPV3_Opaline",
    "M_IPV3_Recess",
    "M_IPV3_Render",
    "M_IPV3_Shutter",
    "M_IPV3_Slate",
    "M_IPV3_Stone",
    "M_IPV3_Trim",
]

# Exact UE5.5 legacy FBX/OBJ witness captured from the first fail-closed live
# import. It is a permutation of the frozen OBJ first-use order, not a reversal
# or another stable ordering rule, so integration must normalize semantically.
LEGACY_OBJ_UE55_OBSERVED_ORDER = [
    "M_IPV3_Trim",
    "M_IPV3_Render",
    "M_IPV3_Glass",
    "M_IPV3_Door",
    "M_IPV3_Recess",
    "M_IPV3_Stone",
    "M_IPV3_Shutter",
    "M_IPV3_Slate",
    "M_IPV3_Metal",
    "M_IPV3_DarkTimber",
    "M_IPV3_Opaline",
]

V2_SOURCE_SEMANTIC_SLOTS = [
    "M_IPV2_Render",
    "M_IPV2_Trim",
    "M_IPV2_Slate",
    "M_IPV2_Shutter",
    "M_IPV2_Stone",
    "M_IPV2_Door",
    "M_IPV2_DarkTimber",
    "M_IPV2_Metal",
    "M_IPV2_Glass",
    "M_IPV2_Opaline",
    "M_IPV2_Recess",
]
V2_SOURCE_SLOT_INSTANCES = {
    "M_IPV2_Render": "MI_IPV_Hero_Render_V2",
    "M_IPV2_Trim": "MI_IPV_Hero_Trim_V2",
    "M_IPV2_Slate": "MI_IPV_Hero_Slate_V2",
    "M_IPV2_Shutter": "MI_IPV_Hero_Louvre_V2",
    "M_IPV2_Stone": "MI_IPV_Hero_Stone_V2",
    "M_IPV2_Door": "MI_IPV_Hero_Timber_V2",
    "M_IPV2_DarkTimber": "MI_IPV_Hero_Timber_V2",
    "M_IPV2_Metal": "MI_IPV_Hero_PaintedMetal_V2",
    "M_IPV2_Glass": "MI_IPV_Hero_Glass_V2",
    "M_IPV2_Opaline": "MI_IPV_Hero_Opaline_V2",
    "M_IPV2_Recess": "MI_IPV_Hero_Recess_V2",
}
# Exact read-only live witness from the protected V2 map. Like the V3 import,
# UE5.5's legacy OBJ route exposes a non-source-order material permutation.
LEGACY_V2_UE55_OBSERVED_ORDER = [
    "M_IPV2_Stone",
    "M_IPV2_Trim",
    "M_IPV2_Render",
    "M_IPV2_Glass",
    "M_IPV2_DarkTimber",
    "M_IPV2_Recess",
    "M_IPV2_Shutter",
    "M_IPV2_Door",
    "M_IPV2_Metal",
    "M_IPV2_Opaline",
    "M_IPV2_Slate",
]
OPAQUE_TEXTURES = {
    "Tex_BaseColor",
    "Tex_Normal",
    "Tex_ORM",
    "Tex_Height",
    "Tex_DetailNormal",
    "Tex_MacroVariation",
    "Tex_SurfaceMasks",
    "Tex_ArchitecturalDirtMasks",
}
OPAQUE_SCALARS = {
    "TileMeters",
    "DetailTileMeters",
    "MacroTileMeters",
    "NormalStrength",
    "DetailNormalStrength",
    "RoughnessBias",
    "MacroAlbedoStrength",
    "MacroRoughnessStrength",
    "WeatheringStrength",
    "SillDirtStrength",
    "CorniceRunoffStrength",
    "GroundContactDampStrength",
    "CavityDirtStrength",
    "HeightMillimetres",
    "BumpOffsetStrength",
    "ExposedMetalMaskStrength",
}
GLASS_TEXTURES = {
    "Tex_BaseColor",
    "Tex_Normal",
    "Tex_ORM",
    "Tex_DetailNormal",
    "Tex_MacroVariation",
    "Tex_SurfaceMasks",
}
GLASS_SCALARS = {
    "TileMeters",
    "DetailTileMeters",
    "MacroTileMeters",
    "NormalStrength",
    "DetailNormalStrength",
    "RoughnessBias",
    "IOR",
    "Opacity",
    "DustStrength",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


class IstanaPublicViewHeroV3IntegrationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.cpp = CPP_PATH.read_text(encoding="utf-8")
        cls.header = HEADER_PATH.read_text(encoding="utf-8")
        cls.build_cs = BUILD_CS_PATH.read_text(encoding="utf-8")
        cls.geometry_contract = load_json(
            GEOMETRY_ROOT / "istana_public_view_hero_v3.contract.json"
        )
        cls.geometry_freeze = load_json(GEOMETRY_ROOT / "hero_v3.freeze.json")
        cls.geometry_manifest = load_json(
            GEOMETRY_ROOT
            / "Generated"
            / "IstanaPublicViewV3Building.manifest.json"
        )
        cls.material_contract = load_json(
            MATERIAL_ROOT / "hero_materials_v3.contract.json"
        )
        cls.material_interface = load_json(
            MATERIAL_ROOT / "unreal_asset_interface.v3.json"
        )
        cls.material_manifest = load_json(MATERIAL_ROOT / "Generated" / "manifest.json")
        cls.integration_freeze = load_json(INTEGRATION_FREEZE_PATH)
        cls.gitignore = GITIGNORE_PATH.read_text(encoding="utf-8")
        cls.scripts = {
            name: path.read_text(encoding="utf-8")
            for name, path in SCRIPT_PATHS.items()
        }

    def test_01_integration_freeze_binds_exact_inputs_and_baselines(self) -> None:
        freeze = self.integration_freeze
        self.assertEqual(
            "triad.istana_public_view_hero_v3.unreal_freeze.v1",
            freeze["schema"],
        )
        self.assertEqual("/Game/Maps/Istana_PublicView_Exterior_v2", freeze["sourceMap"])
        self.assertEqual(
            "/Game/Maps/Istana_PublicView_Exterior_v3", freeze["destinationMap"]
        )
        self.assertFalse(freeze["naniteEnabled"])
        self.assertFalse(freeze["sm6Required"])
        self.assertTrue(freeze["reusesV1Collision"])
        self.assertFalse(freeze["changesSurroundings"])

        expected_roles = {
            "GEOMETRY_FREEZE",
            "GEOMETRY_CONTRACT",
            "GEOMETRY_GENERATOR",
            "GEOMETRY_VALIDATOR",
            "GEOMETRY_CLOSURE_AUDIT",
            "GEOMETRY_DENSE_CLOSURE_AUDIT",
            "GEOMETRY_WINDING_AUDIT",
            "GEOMETRY_MANIFEST",
            "BUILDING_HERO_VISUAL_V3",
            "HERO_MATERIAL_CONTRACT",
            "HERO_MATERIAL_UNREAL_INTERFACE",
            "HERO_MATERIAL_SOURCE_MANIFEST",
            "HERO_MATERIAL_GENERATED_MANIFEST",
            "HERO_MATERIAL_BUILDER",
            "HERO_V3_VISUAL_ACCEPTANCE_SETTINGS",
            "PUBLIC_REFERENCE_MANIFEST",
            "PROTECTED_V2_GEOMETRY_FREEZE",
            "PROTECTED_V2_INTEGRATION_FREEZE",
        }
        self.assertEqual(expected_roles, {row["role"] for row in freeze["files"]})
        self.assertEqual(4, len(freeze["protectedV1Files"]))
        for record in freeze["files"] + freeze["protectedV1Files"]:
            path = UNREAL_ROOT / Path(record["relativePath"])
            self.assertTrue(path.is_file(), record["relativePath"])
            self.assertEqual(record["bytes"], path.stat().st_size)
            self.assertEqual(record["sha256"], sha256(path))

        match = re.search(
            r'FrozenFreezeSha256\(\s*TEXT\("([0-9a-f]{64})"\)\)',
            self.cpp,
        )
        self.assertIsNotNone(match)
        self.assertEqual(sha256(INTEGRATION_FREEZE_PATH), match.group(1))

    def test_02_geometry_is_the_exact_frozen_v3_contract(self) -> None:
        manifest_file = self.geometry_manifest["files"][0]
        obj = GEOMETRY_ROOT / "Generated" / manifest_file["path"]
        self.assertEqual(ORDERED_SLOTS, self.geometry_contract["expectedMaterialSlots"])
        self.assertEqual(ORDERED_SLOTS, self.geometry_freeze["materialSlots"])
        self.assertEqual(ORDERED_SLOTS, self.geometry_manifest["materialSlots"])
        self.assertEqual(ORDERED_SLOTS, self.geometry_manifest["objFirstUseMaterialOrder"])
        self.assertEqual(ORDERED_SLOTS, manifest_file["materialSlots"])
        self.assertEqual(self.geometry_freeze["heroTriangles"], manifest_file["triangles"])
        self.assertEqual(self.geometry_freeze["heroVertices"], manifest_file["vertices"])
        self.assertEqual(self.geometry_freeze["heroBytes"], obj.stat().st_size)
        self.assertEqual(self.geometry_freeze["heroSha256"], sha256(obj))
        closure_audit = GEOMETRY_ROOT / "facade_closure_audit.py"
        dense_closure_audit = GEOMETRY_ROOT / "dense_facade_closure_audit.py"
        winding_audit = GEOMETRY_ROOT / "slate_winding_audit.py"
        self.assertEqual(
            self.geometry_freeze["closureAuditSha256"], sha256(closure_audit)
        )
        self.assertEqual(
            self.geometry_freeze["denseClosureAuditSha256"],
            sha256(dense_closure_audit),
        )
        self.assertEqual(
            self.geometry_manifest["denseClosureAuditSha256"],
            sha256(dense_closure_audit),
        )
        self.assertEqual(
            self.geometry_freeze["slateWindingAuditSha256"],
            sha256(winding_audit),
        )
        self.assertEqual(
            self.geometry_manifest["slateWindingAuditSha256"],
            sha256(winding_audit),
        )
        self.assertTrue(
            self.geometry_manifest["detailMetrics"]
            ["systematicFacadeClosureRayAuditPassed"]
        )
        self.assertEqual(
            1_690,
            self.geometry_manifest["detailMetrics"]
            ["systematicFacadeClosureRayProbeCount"],
        )
        detail = self.geometry_manifest["detailMetrics"]
        self.assertTrue(detail["denseJitteredFacadeClosureRayAuditPassed"])
        self.assertEqual(
            24_078,
            detail["denseJitteredFacadeClosureRayReviewProbeCount"],
        )
        self.assertEqual(
            18_232,
            detail["denseJitteredFacadeClosureRayPhaseOffsetProbeCount"],
        )
        self.assertEqual(
            42_310,
            detail["denseJitteredFacadeClosureRayProbeCount"],
        )
        self.assertEqual(0, detail["denseJitteredFacadeClosureRayMissCount"])
        self.assertEqual(
            {
                "reviewProbeCount": 24_078,
                "phaseOffsetProbeCount": 18_232,
                "totalProbeCount": 42_310,
                "missCount": 0,
            },
            self.geometry_freeze["denseClosureEvidence"],
        )
        self.assertTrue(detail["slateExteriorWindingAuditPassed"])
        self.assertEqual(
            4,
            self.geometry_manifest["featureCounts"]["outwardWoundSlateSlopes"],
        )
        self.assertEqual(
            3_744,
            self.geometry_manifest["featureCounts"]
            ["systematicSlateWindingTrianglesAudited"],
        )
        logical_normals = detail["slateExteriorWindingLogicalAverageNormals"]
        disk_normals = detail["slateExteriorWindingUE55DiskAverageNormals"]
        normal_expectations = {
            "FrontSlateTile_": (1, 1.0, -1.0),
            "RearSlateTile_": (1, -1.0, 1.0),
            "EastSlateTile_": (0, 1.0, 1.0),
            "WestSlateTile_": (0, -1.0, -1.0),
        }
        for slope, (axis, logical_sign, disk_sign) in normal_expectations.items():
            self.assertGreater(logical_normals[slope][axis] * logical_sign, 0.5)
            self.assertGreater(logical_normals[slope][2], 0.5)
            self.assertGreater(disk_normals[slope][axis] * disk_sign, 0.5)
            self.assertGreater(disk_normals[slope][2], 0.5)
        self.assertEqual([-63.6, -56.0, 0.0], manifest_file["boundsMinMeters"])
        self.assertEqual([63.6, 68.905, 36.0], manifest_file["boundsMaxMeters"])
        self.assertIn(
            f'constexpr int32 ExpectedHeroTriangles = {self.geometry_freeze["heroTriangles"]}',
            self.cpp,
        )
        self.assertIn('TEXT("ImportNormals")', self.cpp)
        self.assertIn("bRecomputeNormals = false", self.cpp)
        self.assertIn("bRecomputeTangents = true", self.cpp)
        self.assertIn("Hero->NaniteSettings.bEnabled = false", self.cpp)
        self.assertIn("systematicFacadeClosureRayAuditPassed", self.cpp)
        self.assertIn("denseJitteredFacadeClosureRayAuditPassed", self.cpp)
        self.assertIn("denseJitteredFacadeClosureRayProbeCount", self.cpp)
        self.assertIn("slateExteriorWindingAuditPassed", self.cpp)
        self.assertIn("slateExteriorWindingUE55DiskAverageNormals", self.cpp)
        self.assertNotIn(
            "112db3c045d6c0176839535da6bbecedb4034ff8d2338c277e89e04802635a12",
            self.cpp,
        )
        self.assertNotIn(
            "cbd98178bc27e3769b309a2f2d389291a015dd35a5fc7c9a7b173c78e9afbb23",
            self.cpp,
        )
        self.assertNotIn(
            "91ee3c151276f0dc04fdc71daf153eac3067556c2abffd3399722a92296200c1",
            self.cpp,
        )

    def test_03_material_interface_and_generated_pack_are_exact(self) -> None:
        contract = self.material_contract
        interface = self.material_interface
        manifest = self.material_manifest
        source_manifest_path = MATERIAL_ROOT / "Source" / "source_manifest.json"
        source_manifest = load_json(source_manifest_path)
        builder_path = MATERIAL_ROOT / "build_hero_materials_v3.py"
        self.assertEqual("3.2.0", contract["version"])
        self.assertEqual("3.1.0", interface["version"])
        self.assertEqual("3.1.0", source_manifest["version"])
        self.assertEqual("3.2.0", manifest["version"])
        self.assertEqual(
            "triad.istana_hero_pbr_builder.v3.4.0",
            manifest["algorithmVersion"],
        )
        native_contract = contract["sourceInterface"]["nativeSampleAudit"]
        self.assertTrue(native_contract["preserveNativeScalarBitDepth"])
        self.assertEqual(
            "FLOAT64", native_contract["statisticsReductionPrecision"]
        )
        self.assertTrue(native_contract["failUnlessEveryMeanWithinNativeExtrema"])
        self.assertEqual(
            {
                "originalMode": "I;16",
                "sampleBits": 16,
                "nativeMinimumSample": 17112,
                "nativeMaximumSample": 36510,
                "generatedHeightMustRespondToSourceHeightPerturbation": True,
            },
            native_contract["slateHeightInvariant"],
        )
        slate_height_audits = []
        for source_asset in source_manifest["assets"]:
            for channel in source_asset["channels"]:
                audit = channel["audit"]
                self.assertEqual("FLOAT64", audit["statisticsReductionPrecision"])
                self.assertLessEqual(
                    audit["nativeMinimumSample"], audit["nativeMeanSample"]
                )
                self.assertLessEqual(
                    audit["nativeMeanSample"], audit["nativeMaximumSample"]
                )
                if source_asset["id"] == "Slate" and channel["role"] == "Height":
                    slate_height_audits.append(audit)
        self.assertEqual(1, len(slate_height_audits))
        slate_height = slate_height_audits[0]
        self.assertEqual("I;16", slate_height["originalMode"])
        self.assertEqual("I;16", slate_height["decodedMode"])
        self.assertEqual("uint16", slate_height["nativeDtype"])
        self.assertEqual(16, slate_height["sampleBits"])
        self.assertEqual(17112, slate_height["nativeMinimumSample"])
        self.assertEqual(36510, slate_height["nativeMaximumSample"])
        expected_normal_ownership = {
            "mapGenerationFields": ["sourceNormalMix", "detailSourceMix"],
            "runtimeAmplitudeFields": [
                "NormalStrength",
                "DetailNormalStrength",
            ],
            "sameFieldMayDriveGenerationAndRuntime": False,
        }
        for key, value in expected_normal_ownership.items():
            self.assertEqual(value, contract["normalStrengthOwnership"][key])
            self.assertEqual(value, interface["normalStrengthOwnership"][key])
            self.assertEqual(value, manifest["normalStrengthOwnership"][key])
        self.assertEqual(
            1,
            interface["normalStrengthOwnership"]["runtimeApplicationCount"],
        )
        for material in contract["materials"]:
            self.assertEqual(1.0, material["sourceNormalMix"])
            self.assertEqual(1.0, material["detailSourceMix"])
        for material in manifest["materials"]:
            self.assertEqual(
                {
                    "sourceNormalMix": 1.0,
                    "detailSourceMix": 1.0,
                    "runtimeNormalStrengthAppliedDuringGeneration": False,
                    "runtimeDetailNormalStrengthAppliedDuringGeneration": False,
                },
                material["normalGeneration"],
            )
        self.assertIn("ValidateNormalStrengthOwnership", self.cpp)
        self.assertIn("RuntimeApplicationCount != 1", self.cpp)
        self.assertIn("ValidateNativeSourceChannelAudit", self.cpp)
        self.assertIn('TEXT("I;16")', self.cpp)
        self.assertIn("NativeMinimum != 17112.0", self.cpp)
        self.assertIn("NativeMaximum != 36510.0", self.cpp)
        frozen_cpp_digests = {
            "FrozenMaterialSourceManifestSha256": sha256(source_manifest_path),
            "FrozenMaterialContractSha256": sha256(
                MATERIAL_ROOT / "hero_materials_v3.contract.json"
            ),
            "FrozenMaterialInterfaceSha256": sha256(
                MATERIAL_ROOT / "unreal_asset_interface.v3.json"
            ),
            "FrozenMaterialBuilderSha256": sha256(builder_path),
        }
        for constant, expected_digest in frozen_cpp_digests.items():
            match = re.search(
                rf'{constant}\(\s*TEXT\("([0-9a-f]{{64}})"\)\)',
                self.cpp,
            )
            self.assertIsNotNone(match, constant)
            self.assertEqual(expected_digest, match.group(1), constant)
        for revoked_digest in (
            "ffd858bca5aae934cdcee30760bdc412ccaf3dfa512c26f1d4f53af55a871f9f",
            "06a79adfc30457dfa47acab73f352ecae8bf9609386d354a37e77aaff91d73be",
            "72115400bd2a75a5b773847260833c59a612af32747f31784511818207562b1b",
            "848b6ff91619177c10f565366789a83d1bfb5e007ec98637c147544caa44b5d9",
            "bb5677aa8e3f4e09699d0b0e34be0eeef1bffa27f0b13a9693320cfb7c936852",
            "24e0d155d98b197041fe7484d34366b3d35276b59501159c877f1a1139a8ade3",
            "3840fbdcc0e7efa56457190b895caa92a719173d24e9c5b1c026b2599eb19819",
            "28460d0587fc1f41dce6ce76c3382033fd17ec9fc1861bb625fb100a1aa1eb84",
        ):
            self.assertNotIn(revoked_digest, self.cpp)
            self.assertNotEqual(revoked_digest, sha256(MATERIAL_ROOT / "Generated" / "manifest.json"))
        self.assertEqual(ORDERED_SLOTS, contract["orderedHeroSlotsV3"])
        self.assertEqual(ORDERED_SLOTS, contract["scope"]["requiredV3SlotOrder"])
        self.assertEqual(
            [slot for slot in ORDERED_SLOTS if slot not in {"M_IPV3_Opaline", "M_IPV3_Recess"}],
            contract["scope"]["requiredTexturedV3Slots"],
        )
        self.assertEqual(
            ["M_IPV3_Opaline", "M_IPV3_Recess"],
            contract["scope"]["requiredSpecialV3Slots"],
        )
        self.assertNotIn("requiredV3SpecialSlots", contract["scope"])
        self.assertIn('TEXT("requiredSpecialV3Slots")', self.cpp)
        self.assertNotIn('TEXT("requiredV3SpecialSlots")', self.cpp)
        self.assertEqual(ORDERED_SLOTS, interface["orderedHeroSlotsV3"])
        self.assertEqual(ORDERED_SLOTS, interface["effectiveV3SlotOrder"])
        self.assertEqual(ORDERED_SLOTS, manifest["orderedHeroSlotsV3"])

        opaque = interface["masterMaterials"]["opaque"]
        glass = interface["masterMaterials"]["glass"]
        self.assertEqual(OPAQUE_TEXTURES, set(opaque["textureParameters"]))
        self.assertEqual(OPAQUE_SCALARS, set(opaque["scalarParameters"]))
        self.assertEqual({"LookdevTint"}, set(opaque["vectorParameters"]))
        self.assertEqual(
            {
                "UseTextureSet": True,
                "UseBumpOffset": False,
                "UseArchitecturalDirtMasks": False,
                "UseExposedMetalMask": False,
            },
            opaque["staticSwitches"],
        )
        self.assertEqual(GLASS_TEXTURES, set(glass["textureParameters"]))
        self.assertEqual(GLASS_SCALARS, set(glass["scalarParameters"]))
        self.assertEqual({"LookdevTint"}, set(glass["vectorParameters"]))
        runtime = interface["runtimeRequirements"]
        self.assertEqual("5.5", runtime["minimumEngine"])
        for key in (
            "requiresShaderModel6",
            "requiresNanite",
            "requiresVirtualTextures",
            "changesCollisionOrSensorTruth",
            "changesWorldLighting",
            "changesSurroundings",
        ):
            self.assertFalse(runtime[key], key)

        outputs = []
        for material in manifest["materials"]:
            self.assertEqual(
                {
                    "BaseColor",
                    "Normal",
                    "ORM",
                    "Height",
                    "DetailNormal",
                    "MacroVariation",
                    "SurfaceMasks",
                },
                set(material["outputs"]),
            )
            outputs.extend(material["outputs"].values())
        self.assertEqual(
            {"ArchitecturalDirtMasks"}, set(manifest["sharedOutputs"])
        )
        outputs.extend(manifest["sharedOutputs"].values())
        self.assertEqual(57, manifest["outputCount"])
        self.assertEqual(57, len(outputs))
        self.assertEqual(551260402, sum(row["bytes"] for row in outputs))
        self.assertEqual(57, len({row["file"] for row in outputs}))
        for row in outputs:
            path = MATERIAL_ROOT / "Generated" / row["file"]
            self.assertEqual(row["bytes"], path.stat().st_size)
            self.assertEqual(row["sha256"], sha256(path))

        self.assertFalse(contract["architecturalSurfaceRules"]["exposedBrickAllowed"])
        self.assertFalse(
            contract["architecturalSurfaceRules"]
            ["brickCoursingInBaseColorNormalHeightOrMasksAllowed"]
        )

    def test_04_unreal_material_graph_uses_only_the_frozen_v3_semantics(self) -> None:
        for parameter in OPAQUE_TEXTURES | OPAQUE_SCALARS | GLASS_SCALARS | {
            "LookdevTint",
            "UseTextureSet",
            "UseBumpOffset",
            "UseArchitecturalDirtMasks",
            "UseExposedMetalMask",
        }:
            self.assertIn(f'TEXT("{parameter}")', self.cpp, parameter)
        for marker in (
            "TRIAD_IPV3_HeightRatio",
            "TRIAD_IPV3_OpaqueAlbedo",
            "TRIAD_IPV3_OpaqueRoughness",
            "TRIAD_IPV3_OpaqueMetallic",
            "TRIAD_IPV3_NormalRNM",
            "TRIAD_IPV3_GlassNormalRNM",
            "SealHeroMaterialGraph",
            "ValidateHeroMaterialGraphSeal",
            "ValidateCustomExpressionTopology",
            "ValidateExactMaterialOverrideNames",
        ):
            self.assertIn(marker, self.cpp)
        for forbidden in (
            "SharedWeatheringDecalMasks",
            "UseGenericWeatheringDecals",
            "GenericDecalStrength",
            "Tex_SharedWeatheringDecalMasks",
        ):
            self.assertNotIn(forbidden, self.cpp)
        self.assertIn("DirtRgb.r * max(SillDirtStrength", self.cpp)
        self.assertIn("DirtRgb.g * max(CorniceRunoffStrength", self.cpp)
        self.assertIn("DirtRgb.b * max(GroundContactDampStrength", self.cpp)
        self.assertIn("DirtA * max(CavityDirtStrength", self.cpp)
        self.assertIn(
            'TEXT("triad.istana_public_view_hero_v3.material_graph.v1")',
            self.cpp,
        )
        self.assertNotIn("bScreenSpaceReflections", self.cpp)

    def test_05_import_and_map_migration_are_additive_and_fail_closed(self) -> None:
        for required in (
            'SourceMapPackage(TEXT("/Game/Maps/Istana_PublicView_Exterior_v2"))',
            'DestinationMapPackage(TEXT("/Game/Maps/Istana_PublicView_Exterior_v3"))',
            "ValidateIstanaPublicViewHeroV2Map",
            "ValidateIstanaPublicViewHeroV2Assets",
            "Refusing to overwrite existing v3 destination map",
            "DoesObjectOrPackageExist",
            "CaptureSurroundings",
            "ValidateSurroundingsUnchanged",
            "CaptureEditableCameraComponentDigest",
            "ProtectedV1ShaBefore",
            "SourceV2ShaBefore",
            "ProtectedV1ShaAfter",
            "SourceV2ShaAfter",
            "onlyHeroVisualComponentChanged",
            "heroVisualNonAssetStateExact",
            "componentsExact",
            "camerasExact",
        ):
            self.assertIn(required, self.cpp)
        mutation = re.search(
            r"TargetScene->BuildingHeroVisualComponent->Modify\(\);(?P<body>[\s\S]{0,500})FPublicViewSurroundingsSnapshot AfterSwap",
            self.cpp,
        )
        self.assertIsNotNone(mutation)
        body = mutation.group("body")
        self.assertIn("SetStaticMesh(Hero)", body)
        self.assertIn("EmptyOverrideMaterials()", body)
        self.assertNotIn("SetCollisionEnabled", body)
        self.assertNotIn("SetMaterial(", body)
        self.assertIn("HeroVisualComponent", self.cpp)
        self.assertIn(
            "property other than its mesh/effective materials",
            self.cpp,
        )
        self.assertIn("$report.heroVisualNonAssetStateExact", self.scripts["migrate"])

        for forbidden in (
            "PCD3D_SM6",
            "D3D12TargetedShaderFormats",
            "r.ShaderModel",
            "r.Nanite",
        ):
            self.assertNotIn(forbidden, self.cpp)

        import_start = self.cpp.index(
            "ImportIstanaPublicViewHeroV3Assets(FString& OutMessage)"
        )
        import_end = self.cpp.index(
            "ValidateIstanaPublicViewHeroV3Assets(FString& OutReport)",
            import_start,
        )
        import_body = self.cpp[import_start:import_end]
        source_gate = import_body.index("ValidateCompleteV3SourceSet")
        mutation_handoff = import_body.index("ImportHeroV3AssetSet")
        self.assertLess(source_gate, mutation_handoff)
        self.assertNotIn("CreateAsset(", import_body[:mutation_handoff])
        self.assertNotIn("ImportAssetTasks", import_body[:mutation_handoff])

        # Migration consumes the already frozen and validated V2 asset. Its
        # legacy OBJ slot order is not a contract: the exact semantic names,
        # one-to-one roster, and effective material bound at each imported
        # index are. Prove arbitrary permutations are accepted without any V2
        # mutation while malformed or ambiguous bindings fail closed.
        def validate_v2_source_slot_model(
            material_slots: list[str],
            effective_instances: list[str],
            has_overrides: bool = False,
            has_overlay: bool = False,
        ) -> None:
            if has_overrides or has_overlay:
                raise ValueError("component overrides")
            if (
                len(material_slots) != len(V2_SOURCE_SEMANTIC_SLOTS)
                or len(effective_instances) != len(V2_SOURCE_SEMANTIC_SLOTS)
            ):
                raise ValueError("slot count")
            seen: set[str] = set()
            for material_slot, effective in zip(
                material_slots, effective_instances
            ):
                expected = V2_SOURCE_SLOT_INSTANCES.get(material_slot)
                if (
                    material_slot in seen
                    or expected is None
                    or effective != expected
                ):
                    raise ValueError("semantic binding")
                seen.add(material_slot)
            if seen != set(V2_SOURCE_SEMANTIC_SLOTS):
                raise ValueError("incomplete roster")

        self.assertEqual(
            set(V2_SOURCE_SEMANTIC_SLOTS),
            set(LEGACY_V2_UE55_OBSERVED_ORDER),
        )
        self.assertNotEqual(
            V2_SOURCE_SEMANTIC_SLOTS,
            LEGACY_V2_UE55_OBSERVED_ORDER,
        )
        v2_permutations = (
            V2_SOURCE_SEMANTIC_SLOTS,
            list(reversed(V2_SOURCE_SEMANTIC_SLOTS)),
            V2_SOURCE_SEMANTIC_SLOTS[4:] + V2_SOURCE_SEMANTIC_SLOTS[:4],
            [
                V2_SOURCE_SEMANTIC_SLOTS[index]
                for index in (8, 1, 10, 3, 6, 0, 9, 4, 2, 7, 5)
            ],
            LEGACY_V2_UE55_OBSERVED_ORDER,
        )
        for permutation in v2_permutations:
            validate_v2_source_slot_model(
                permutation,
                [V2_SOURCE_SLOT_INSTANCES[slot] for slot in permutation],
            )

        valid_v2_effective = [
            V2_SOURCE_SLOT_INSTANCES[slot]
            for slot in V2_SOURCE_SEMANTIC_SLOTS
        ]
        invalid_v2_vectors = (
            (V2_SOURCE_SEMANTIC_SLOTS[:-1], valid_v2_effective[:-1]),
            (
                V2_SOURCE_SEMANTIC_SLOTS[:-1]
                + [V2_SOURCE_SEMANTIC_SLOTS[0]],
                valid_v2_effective[:-1] + [valid_v2_effective[0]],
            ),
            (
                V2_SOURCE_SEMANTIC_SLOTS[:-1] + ["M_IPV2_Unknown"],
                valid_v2_effective[:-1] + ["MI_IPV_Hero_Unknown_V2"],
            ),
            (
                V2_SOURCE_SEMANTIC_SLOTS,
                [V2_SOURCE_SLOT_INSTANCES[V2_SOURCE_SEMANTIC_SLOTS[1]]]
                + valid_v2_effective[1:],
            ),
        )
        for material_slots, effective_instances in invalid_v2_vectors:
            with self.assertRaises(ValueError):
                validate_v2_source_slot_model(
                    material_slots, effective_instances
                )
        with self.assertRaises(ValueError):
            validate_v2_source_slot_model(
                V2_SOURCE_SEMANTIC_SLOTS,
                valid_v2_effective,
                has_overrides=True,
            )
        with self.assertRaises(ValueError):
            validate_v2_source_slot_model(
                V2_SOURCE_SEMANTIC_SLOTS,
                valid_v2_effective,
                has_overlay=True,
            )

        v2_source_validation_start = self.cpp.index(
            "bool ValidateExactV2SourceHeroComponent("
        )
        v2_source_validation_end = self.cpp.index(
            "bool ValidateWorldForV3(", v2_source_validation_start
        )
        v2_source_validation = self.cpp[
            v2_source_validation_start:v2_source_validation_end
        ]
        for marker in (
            "ExpectedV2SourceSlots().Contains(Slot)",
            "SeenSlots.Contains(Slot)",
            "ExpectedV2SourceSlotMaterials().FindRef(Slot)",
            "HeroComponent->GetMaterial(Index)",
            "SeenSlots.Num() != ExpectedV2SourceSlots().Num()",
            "HeroComponent->HasOverrideMaterials()",
            "HeroComponent->GetNumOverrideMaterials() != 0",
            "HeroComponent->GetOverlayMaterial() != nullptr",
        ):
            self.assertIn(marker, v2_source_validation)
        self.assertNotIn(
            "Slot != ExpectedV2SourceSlots()[Index]", v2_source_validation
        )
        self.assertNotIn(
            "ImportedMaterialSlotName", v2_source_validation
        )
        for forbidden_mutation in (
            "SetStaticMesh(",
            "SetMaterial(",
            "SetMaterialByName(",
            "EmptyOverrideMaterials(",
            "SetCollisionEnabled(",
            "Modify(",
            "PostEditChange",
            "SaveLoadedAsset(",
        ):
            self.assertNotIn(forbidden_mutation, v2_source_validation)

        migrate_start = self.cpp.index("bool MigrateMapToV3(")
        migrate_end = self.cpp.index(
            "ValidateIstanaPublicViewHeroV3RemoteControlProject(",
            migrate_start,
        )
        migrate_body = self.cpp[migrate_start:migrate_end]
        self.assertLess(
            migrate_body.index("ValidateSourceWorldForMigration("),
            migrate_body.index("DuplicateLoadedAsset("),
        )

        self.assertEqual(len(ORDERED_SLOTS), len(LEGACY_OBJ_UE55_OBSERVED_ORDER))
        self.assertEqual(set(ORDERED_SLOTS), set(LEGACY_OBJ_UE55_OBSERVED_ORDER))
        self.assertNotEqual(ORDERED_SLOTS, LEGACY_OBJ_UE55_OBSERVED_ORDER)
        def normalize_slot_model(
            slot_pairs: list[tuple[str, str]],
            render_indices: list[int],
            current_indices: list[int],
            original_indices: list[int],
        ) -> tuple[list[str], list[int]]:
            if len(slot_pairs) != len(ORDERED_SLOTS):
                raise ValueError("slot count")
            ordered: list[str | None] = [None] * len(ORDERED_SLOTS)
            imported_to_ordered: list[int] = []
            for material_slot, imported_slot in slot_pairs:
                if material_slot != imported_slot:
                    raise ValueError("name fields")
                if material_slot not in ORDERED_SLOTS:
                    raise ValueError("unknown slot")
                ordered_index = ORDERED_SLOTS.index(material_slot)
                if ordered[ordered_index] is not None:
                    raise ValueError("duplicate slot")
                ordered[ordered_index] = material_slot
                imported_to_ordered.append(ordered_index)
            if any(slot is None for slot in ordered):
                raise ValueError("missing slot")
            if not (
                len(render_indices)
                == len(current_indices)
                == len(original_indices)
                == len(ORDERED_SLOTS)
            ):
                raise ValueError("section count")
            remapped_sections: list[int] = []
            for render, current, original in zip(
                render_indices, current_indices, original_indices
            ):
                if render != current or render != original:
                    raise ValueError("section disagreement")
                if render < 0 or render >= len(imported_to_ordered):
                    raise ValueError("section range")
                remapped_sections.append(imported_to_ordered[render])
            return [str(slot) for slot in ordered], remapped_sections

        observed_to_ordered = [
            ORDERED_SLOTS.index(slot)
            for slot in LEGACY_OBJ_UE55_OBSERVED_ORDER
        ]
        self.assertEqual(
            [10, 6, 2, 1, 5, 9, 7, 8, 3, 0, 4],
            observed_to_ordered,
        )
        permutations = (
            ORDERED_SLOTS,
            list(reversed(ORDERED_SLOTS)),
            ORDERED_SLOTS[3:] + ORDERED_SLOTS[:3],
            [ORDERED_SLOTS[i] for i in (4, 0, 10, 2, 7, 1, 9, 3, 6, 5, 8)],
            LEGACY_OBJ_UE55_OBSERVED_ORDER,
        )
        for permutation in permutations:
            expected_remap = [ORDERED_SLOTS.index(slot) for slot in permutation]
            canonicalized, remapped_sections = normalize_slot_model(
                [(slot, slot) for slot in permutation],
                list(range(len(ORDERED_SLOTS))),
                list(range(len(ORDERED_SLOTS))),
                list(range(len(ORDERED_SLOTS))),
            )
            self.assertEqual(ORDERED_SLOTS, canonicalized)
            self.assertEqual(expected_remap, remapped_sections)

        valid_pairs = [(slot, slot) for slot in ORDERED_SLOTS]
        negative_vectors = (
            (valid_pairs[:-1], list(range(10)), list(range(10)), list(range(10))),
            (valid_pairs[:-1] + [valid_pairs[0]], list(range(11)), list(range(11)), list(range(11))),
            (valid_pairs[:-1] + [("M_IPV3_Unknown", "M_IPV3_Unknown")], list(range(11)), list(range(11)), list(range(11))),
            ([(ORDERED_SLOTS[0], ORDERED_SLOTS[1])] + valid_pairs[1:], list(range(11)), list(range(11)), list(range(11))),
            (valid_pairs, list(range(11)), [1] + list(range(1, 11)), list(range(11))),
            (valid_pairs, list(range(11)), list(range(11)), [1] + list(range(1, 11))),
            (valid_pairs, list(range(10)) + [11], list(range(10)) + [11], list(range(10)) + [11]),
        )
        for vector in negative_vectors:
            with self.assertRaises(ValueError):
                normalize_slot_model(*vector)

        for marker in (
            "NormalizeImportedHeroMaterialSlots",
            "Imported.MaterialSlotName.ToString()",
            "Imported.ImportedMaterialSlotName.ToString()",
            "ImportedToOrdered",
            "SeenOrderedSlots",
            "RenderMaterialIndex != SectionInfo.MaterialIndex",
            "RenderMaterialIndex != OriginalSectionInfo.MaterialIndex",
            "GetSectionInfoMap().Set(0, SectionIndex, SectionInfo)",
            "GetOriginalSectionInfoMap().Set(",
            "Mesh->SetStaticMaterials(OrderedMaterials)",
            "GetPolygonGroupMaterialSlotNames",
            "Lod.Sections[SectionIndex].MaterialIndex",
            "SeenSectionSlots",
        ):
            self.assertIn(marker, self.cpp)
        asset_import_start = self.cpp.index("bool ImportHeroV3AssetSet(")
        asset_import_end = self.cpp.index(
            "bool CaptureEditableCameraComponentDigest(", asset_import_start
        )
        asset_import_body = self.cpp[asset_import_start:asset_import_end]
        normalize_call = asset_import_body.index(
            "NormalizeImportedHeroMaterialSlots(Hero, Error)"
        )
        semantic_bind = asset_import_body.index(
            "Hero->SetMaterial", normalize_call
        )
        rebuild = asset_import_body.index(
            "Hero->PostEditChange()", semantic_bind
        )
        validate = asset_import_body.index("ValidateHeroMesh", rebuild)
        save = asset_import_body.index("SaveLoadedAssets", validate)
        self.assertLess(normalize_call, semantic_bind)
        self.assertLess(semantic_bind, rebuild)
        self.assertLess(rebuild, validate)
        self.assertLess(validate, save)

    def test_06_camera_local_d65_tsr_capture_is_transient_and_restored(self) -> None:
        for marker in (
            "ConfigureHeroV3D65CaptureCamera",
            "FScopedHeroV3TsrCaptureQuality",
            "AEM_Manual",
            "WhiteTemp = 6500.0f",
            "CameraISO = 100.0f",
            "CameraShutterSpeed = 125.0f",
            "AutoExposureBias = 0.0f",
            "bOverride_DynamicGlobalIlluminationMethod = true",
            "EDynamicGlobalIlluminationMethod::ScreenSpace",
            "bOverride_ReflectionMethod = true",
            "EReflectionMethod::ScreenSpace",
            "TemperatureType = TEMP_WhiteBalance",
            "ColorGradingLUT = nullptr",
            "Viewport->SetFixedViewportSize(3840, 2160)",
            'TEXT("r.AntiAliasingMethod")',
            'TEXT("r.ScreenPercentage")',
            'TEXT("r.TSR.History.ScreenPercentage")',
            'TEXT("r.SSGI.Quality")',
            'TEXT("r.SSR.Quality")',
            "OriginalAntiAliasing_",
            "OriginalScreenPercentage_",
            "OriginalTsrHistoryPercentage_",
            "OriginalSsgiQuality_",
            "OriginalSsrQuality_",
            "Quality.Restore",
            "RF_Transient",
        ):
            self.assertIn(marker, self.cpp)
        quality_start = self.cpp.index("class FScopedHeroV3TsrCaptureQuality")
        quality_end = self.cpp.index("bool MigrateMapToV3(", quality_start)
        quality_body = self.cpp[quality_start:quality_end]
        for apply_marker, restore_marker in (
            (
                "SsgiQuality_->SetWithCurrentPriority(4)",
                "SsgiQuality_->SetWithCurrentPriority(OriginalSsgiQuality_)",
            ),
            (
                "SsrQuality_->SetWithCurrentPriority(4)",
                "SsrQuality_->SetWithCurrentPriority(OriginalSsrQuality_)",
            ),
        ):
            self.assertIn(apply_marker, quality_body)
            self.assertIn(restore_marker, quality_body)
            self.assertLess(
                quality_body.index(apply_marker), quality_body.index(restore_marker)
            )
        self.assertNotIn("r.GenerateMeshDistanceFields", quality_body)
        self.assertNotIn("r.Lumen", quality_body)
        self.assertNotIn("r.DynamicGlobalIlluminationMethod", quality_body)
        self.assertNotIn("r.ReflectionMethod", quality_body)
        success_message_start = self.cpp.index(
            'TEXT("Captured 3840x2160 Hero-V3 QA preset='
        )
        success_message_end = self.cpp.index('")', success_message_start)
        success_message = self.cpp[success_message_start:success_message_end]
        for exact_diagnostic in (
            "manual ISO100 1/125s",
            "exposure bias 0 EV",
            "ScreenSpace GI/reflection",
            "r.SSGI.Quality=4",
            "r.SSR.Quality=4",
            "every CVar was restored exactly to its pre-capture value",
        ):
            self.assertIn(exact_diagnostic, success_message)
        self.assertNotIn("1/200s", success_message)
        self.assertNotIn("exposure bias -0.65 EV", success_message)
        for preset in (
            "HERO_FRONT",
            "HERO_OBLIQUE_RIGHT",
            "HERO_OBLIQUE_LEFT",
            "HERO_FACADE_MACRO",
            "HERO_ORBIT_RIGHT",
            "HERO_ORBIT_LEFT",
        ):
            self.assertIn(preset, self.cpp)
            self.assertIn(preset, self.scripts["capture"])
        capture = self.scripts["capture"]
        for marker in (
            "$v1HashBefore",
            "$v2HashBefore",
            "$v3HashBefore",
            "$v1HashAfter",
            "$v2HashAfter",
            "$v3HashAfter",
            "$dimensions.Width -ne 3840",
            "$dimensions.Height -ne 2160",
            "Refusing to overwrite existing hero-v3 preview",
        ):
            self.assertIn(marker, capture)

    def test_07_scripts_parse_and_do_not_write_live_or_renderer_defaults(self) -> None:
        powershell = shutil.which("pwsh") or shutil.which("powershell")
        if powershell is None:
            self.skipTest("PowerShell parser is unavailable")
        for name, path in SCRIPT_PATHS.items():
            parse = subprocess.run(
                [
                    powershell,
                    "-NoProfile",
                    "-NonInteractive",
                    "-Command",
                    (
                        "$tokens=$null;$errors=$null;"
                        "[void][System.Management.Automation.Language.Parser]::"
                        f"ParseFile('{path}',[ref]$tokens,[ref]$errors);"
                        "if($errors.Count){$errors|% Message;exit 1}"
                    ),
                ],
                capture_output=True,
                text=True,
                timeout=60,
            )
            self.assertEqual(0, parse.returncode, f"{name}: {parse.stdout}{parse.stderr}")

        combined = "\n".join(self.scripts.values())
        self.assertNotRegex(combined, r"(?i)D:\\triad")
        self.assertNotRegex(
            combined,
            r"(?i)(Set|Add)-Content[^\n]*DefaultEngine\.ini|Copy-Item[^\n]*DefaultEngine\.ini",
        )
        for name, script in self.scripts.items():
            self.assertRegex(
                script,
                r"\[Parameter\(Mandatory\s*=\s*\$true\)\][\s\S]{0,140}\[string\]\s+\$ProjectPath",
                name,
            )
        self.assertIn("Assert-NonOverwritingFileCopy", self.scripts["install"])
        self.assertIn("Copy-NewOrEqualHashFile", self.scripts["install"])
        self.assertIn(
            "Assert-NoHeroMaterialsV3DeterminismTemporaryTree",
            self.scripts["install"],
        )
        self.assertIn(
            "HeroMaterialsV3_Determinism_[a-z0-9_]+",
            self.scripts["install"],
        )
        for marker in (
            "Assert-NoGeneratedPythonCache",
            "Assert-NoHeroV3GeneratedPythonCache",
            "$sourcePluginRoot",
            "$targetPluginRoot",
            "$pluginFiles",
            "repository plugin source after controlled installation",
            "target project plugin after controlled installation",
            "Get-RecognizedPluginUpdateDisposition",
            "MISSING_ADDITIVE",
            "CURRENT_EQUAL",
            "RECOGNIZED_PRIOR",
            "RECOGNIZED_PRIOR_REPLACE",
            "584b43bd1c3fdfa0f5cf3d00167f7688c8fd01b4745505d05917d7a0b09ee8d4",
            "b79da60b0c793094b8e753bb7903af52623858b1f190cdb5cc3e79dbbbf9f53e",
            "aab3b1de39ac2536fb1886f76cb1e59b0ff8da3d94126e17ae8bada4b80463b8",
            "9273f8067378893658338ef4bd50363b3d1fc68a753ad33616b0dc88c9c6ce08",
            "Source\\TRIADSensorFusionEditor\\TRIADSensorFusionEditor.Build.cs",
            "1df3a2115cd414072d5f9f895b52719cd0c69e23d3bc84582190bf8abb50dc84",
            "34ba790e50b87d495c2eff41aa4afbc2798af8524dd891740dccab466df973f5",
            "Copy-RecognizedPriorHashUpgrade",
            "RecognizedPriorPluginFilesPreserved",
            "RecognizedPriorPluginFilesReplaced",
            "BroadLegacyForceInstallerCalled = $false",
        ):
            self.assertIn(marker, self.scripts["install"])
        self.assertIn(
            "159b7ded558197544d72c44af86cb5209594f57c89f0e947e719b5617aab69bc",
            self.scripts["install"],
        )
        self.assertIn(
            "c06d7eee5a8251a03ff9384cb5d2d41b677f32a5939781d8ee5e7fe6cd16186c",
            self.scripts["install"],
        )
        self.assertIn(
            "f64bcd9b71bf106044fd62aca6d7f245038d498427c955e82d9c90a3c96651f0",
            self.scripts["install"],
        )
        self.assertIn(
            "f1ff3b9057db2992e05f30d4a70507019deb7fca08aacbcc232178a5affa020f",
            self.scripts["install"],
        )
        self.assertIn(sha256(CPP_PATH), self.scripts["install"])
        self.assertIn(sha256(Path(__file__).resolve()), self.scripts["install"])
        cache_preflight = self.scripts["install"].find(
            "Assert-NoGeneratedPythonCache `\n"
            "    -SourceRoot $sourcePluginRoot"
        )
        controlled_install = self.scripts["install"].find(
            "# Install each reviewed plugin file independently"
        )
        self.assertGreaterEqual(cache_preflight, 0)
        self.assertGreater(controlled_install, cache_preflight)
        self.assertNotIn("$baseInstallResult", self.scripts["install"])
        self.assertNotIn("& $baseInstaller", self.scripts["install"])
        self.assertIn("[System.IO.File]::Move($stageFile, $DestinationFile)", self.scripts["install"])
        self.assertNotIn("Copy-Item -LiteralPath $SourceFile -Destination $DestinationFile", self.scripts["install"])
        self.assertRegex(
            self.scripts["install"],
            r"Assert-NoHeroMaterialsV3DeterminismTemporaryTree\s+"
            r"-SourceRoot\s+\$sourceMaterialRoot",
        )
        preflight_call = self.scripts["install"].rfind(
            "Assert-NoHeroMaterialsV3DeterminismTemporaryTree "
            "-SourceRoot $sourceMaterialRoot"
        )
        copy_set_enumeration = self.scripts["install"].find(
            "$geometryFiles = Get-InstallSourceFiles"
        )
        self.assertGreaterEqual(preflight_call, 0)
        self.assertGreater(copy_set_enumeration, preflight_call)
        for name in ("install", "import", "migrate"):
            script = self.scripts[name]
            self.assertIn("Resolve-HeroMaterialPython", script, name)
            self.assertIn("MaterialPythonExecutable", script, name)
            self.assertIn(
                "HeroMaterialsV2\\.venv\\Scripts\\python.exe",
                script,
                name,
            )
            self.assertIn("12.3.0|2.3.5", script, name)
            self.assertIn("system-Python fallback is forbidden", script, name)
            self.assertNotIn("Get-Command python", script, name)
            self.assertRegex(
                script,
                r"&\s+\$candidate\s+-B\s+-c\s+",
                f"{name}: dependency/version probe must suppress bytecode",
            )
            self.assertEqual(
                2,
                len(re.findall(r"&\s+\$PythonExecutable\s+-B\s+", script)),
                f"{name}: both geometry and material validation must suppress bytecode",
            )
            resolver_call = script.rfind(
                "$pythonExecutable = Resolve-HeroMaterialPython"
            )
            source_validation = script.find(
                "Invoke-FailClosedSourceValidation `",
                resolver_call,
            )
            self.assertGreaterEqual(resolver_call, 0, name)
            self.assertGreater(source_validation, resolver_call, name)
        self.assertIn("**/HeroMaterialsV3_Determinism_*/", self.gitignore)
        self.assertIn("Refusing to overwrite existing v3 destination map", self.scripts["migrate"])

        # Exercise the extracted prior-baseline decision function without a
        # live project. Mocked file hashes isolate its four control outcomes.
        install_path = str(SCRIPT_PATHS["install"]).replace("'", "''")
        disposition_test = (
            f"$path='{install_path}';"
            "$tokens=$null;$errors=$null;"
            "$ast=[System.Management.Automation.Language.Parser]::"
            "ParseFile($path,[ref]$tokens,[ref]$errors);"
            "$fn=$ast.Find({param($node) $node -is "
            "[System.Management.Automation.Language.FunctionDefinitionAst] "
            "-and $node.Name -eq 'Get-RecognizedPluginUpdateDisposition'},$true);"
            "Invoke-Expression $fn.Extent.Text;"
            "$root=[IO.Path]::Combine([IO.Path]::GetTempPath(),"
            "'HeroV3Disposition_'+[Guid]::NewGuid().ToString('N'));"
            "[IO.Directory]::CreateDirectory($root)|Out-Null;"
            "$source=[IO.Path]::Combine($root,'source');"
            "$dest=[IO.Path]::Combine($root,'dest');"
            "[IO.File]::WriteAllBytes($source,[byte[]](1));"
            "$script:sourceHash='aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa';"
            "$script:destHash=$script:sourceHash;"
            "function Get-FileHash{param($LiteralPath,$Algorithm)"
            "$hash=if($LiteralPath -eq $source){$script:sourceHash}else{$script:destHash};"
            "[pscustomobject]@{Hash=$hash}};"
            "$map=@{'README.md'='584b43bd1c3fdfa0f5cf3d00167f7688c8fd01b4745505d05917d7a0b09ee8d4'};"
            "$replacePath='Source\\TRIADSensorFusionEditor\\Private\\TRIADIstanaPublicViewHeroV3EditorLibrary.cpp';"
            "$replaceMap=@{$replacePath='cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc'};"
            "$replacementSourceMap=@{$replacePath=$script:sourceHash};"
            "try{"
            "$a=Get-RecognizedPluginUpdateDisposition -SourceFile $source "
            "-DestinationFile $dest -RelativePath 'README.md' -RecognizedPriorHashes $map "
            "-RecognizedPriorReplacementHashes $replaceMap "
            "-RecognizedReplacementSourceHashes $replacementSourceMap;"
            "if($a -cne 'MISSING_ADDITIVE'){exit 1};"
            "[IO.File]::WriteAllBytes($dest,[byte[]](2));"
            "$b=Get-RecognizedPluginUpdateDisposition -SourceFile $source "
            "-DestinationFile $dest -RelativePath 'README.md' -RecognizedPriorHashes $map "
            "-RecognizedPriorReplacementHashes $replaceMap "
            "-RecognizedReplacementSourceHashes $replacementSourceMap;"
            "if($b -cne 'CURRENT_EQUAL'){exit 2};"
            "$script:destHash=$map['README.md'];"
            "$c=Get-RecognizedPluginUpdateDisposition -SourceFile $source "
            "-DestinationFile $dest -RelativePath 'README.md' -RecognizedPriorHashes $map "
            "-RecognizedPriorReplacementHashes $replaceMap "
            "-RecognizedReplacementSourceHashes $replacementSourceMap;"
            "if($c -cne 'RECOGNIZED_PRIOR'){exit 3};"
            "$script:destHash=$replaceMap[$replacePath];"
            "$d=Get-RecognizedPluginUpdateDisposition -SourceFile $source "
            "-DestinationFile $dest -RelativePath $replacePath -RecognizedPriorHashes $map "
            "-RecognizedPriorReplacementHashes $replaceMap "
            "-RecognizedReplacementSourceHashes $replacementSourceMap;"
            "if($d -cne 'RECOGNIZED_PRIOR_REPLACE'){exit 4};"
            "$replaceMap[$replacePath]=@($replaceMap[$replacePath],('e'*64));"
            "$script:destHash=('e'*64);"
            "$d2=Get-RecognizedPluginUpdateDisposition -SourceFile $source "
            "-DestinationFile $dest -RelativePath $replacePath -RecognizedPriorHashes $map "
            "-RecognizedPriorReplacementHashes $replaceMap "
            "-RecognizedReplacementSourceHashes $replacementSourceMap;"
            "if($d2 -cne 'RECOGNIZED_PRIOR_REPLACE'){exit 7};"
            "$replacementSourceMap[$replacePath]=('d'*64);"
            "$wrongSourceRejected=$false;try{Get-RecognizedPluginUpdateDisposition "
            "-SourceFile $source -DestinationFile $dest -RelativePath $replacePath "
            "-RecognizedPriorHashes $map -RecognizedPriorReplacementHashes $replaceMap "
            "-RecognizedReplacementSourceHashes $replacementSourceMap|Out-Null}catch{$wrongSourceRejected=$true};"
            "if(-not $wrongSourceRejected){exit 5};"
            "$replacementSourceMap[$replacePath]=$script:sourceHash;"
            "$script:destHash='bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb';"
            "$rejected=$false;try{Get-RecognizedPluginUpdateDisposition "
            "-SourceFile $source -DestinationFile $dest -RelativePath 'README.md' "
            "-RecognizedPriorHashes $map -RecognizedPriorReplacementHashes $replaceMap "
            "-RecognizedReplacementSourceHashes $replacementSourceMap|Out-Null}catch{$rejected=$true};"
            "if(-not $rejected){exit 6}"
            "}finally{[IO.Directory]::Delete($root,$true)}"
        )
        disposition_result = subprocess.run(
            [
                powershell,
                "-NoProfile",
                "-NonInteractive",
                "-Command",
                disposition_test,
            ],
            capture_output=True,
            text=True,
            timeout=60,
        )
        self.assertEqual(
            0,
            disposition_result.returncode,
            disposition_result.stdout + disposition_result.stderr,
        )

        # The additive publisher must never overwrite a destination that
        # appears after its absence check. Publish through a verified,
        # same-directory stage and let atomic no-overwrite Move arbitrate.
        additive_test = (
            f"$path='{install_path}';"
            "$tokens=$null;$errors=$null;"
            "$ast=[System.Management.Automation.Language.Parser]::"
            "ParseFile($path,[ref]$tokens,[ref]$errors);"
            "$fn=$ast.Find({param($node) $node -is "
            "[System.Management.Automation.Language.FunctionDefinitionAst] "
            "-and $node.Name -eq 'Copy-NewOrEqualHashFile'},$true);"
            "Invoke-Expression $fn.Extent.Text;"
            "$root=[IO.Path]::Combine([IO.Path]::GetTempPath(),"
            "'HeroV3Additive_'+[Guid]::NewGuid().ToString('N'));"
            "[IO.Directory]::CreateDirectory($root)|Out-Null;"
            "$source=[IO.Path]::Combine($root,'source.bin');"
            "$dest=[IO.Path]::Combine($root,'dest.bin');"
            "try{"
            "[IO.File]::WriteAllText($source,'reviewed-source');"
            "Copy-NewOrEqualHashFile -SourceFile $source -DestinationFile $dest;"
            "if([IO.File]::ReadAllText($dest) -cne 'reviewed-source'){exit 1};"
            "if(@(Get-ChildItem -LiteralPath $root -Filter '*.hero_v3_*').Count -ne 0){exit 2};"
            "[IO.File]::WriteAllText($dest,'unknown');"
            "$rejected=$false;try{Copy-NewOrEqualHashFile -SourceFile $source "
            "-DestinationFile $dest}catch{$rejected=$true};"
            "if(-not $rejected -or [IO.File]::ReadAllText($dest) -cne 'unknown'){exit 3};"
            "[IO.File]::Delete($dest);$script:destChecks=0;$script:injectCollision=$true;"
            "function Test-Path{param($LiteralPath,$PathType)"
            "$real=Microsoft.PowerShell.Management\\Test-Path -LiteralPath $LiteralPath -PathType $PathType;"
            "if($LiteralPath -eq $dest){$script:destChecks++;"
            "if($script:injectCollision -and $script:destChecks -eq 1){"
            "[IO.File]::WriteAllText($dest,'intervening');$script:injectCollision=$false;return $false}};"
            "return $real};"
            "$rejected=$false;try{Copy-NewOrEqualHashFile -SourceFile $source "
            "-DestinationFile $dest}catch{$rejected=$true};"
            "$residue=@(Get-ChildItem -LiteralPath $root -Filter '*.hero_v3_*').Count;"
            "if(-not $rejected -or [IO.File]::ReadAllText($dest) -cne 'intervening' -or "
            "$residue -ne 0){Write-Output ('ADDITIVE_COLLISION rejected='+$rejected+"
            "' content='+[IO.File]::ReadAllText($dest)+' residue='+$residue);exit 4}"
            "}finally{[IO.Directory]::Delete($root,$true)}"
        )
        additive_result = subprocess.run(
            [
                powershell,
                "-NoProfile",
                "-NonInteractive",
                "-Command",
                additive_test,
            ],
            capture_output=True,
            text=True,
            timeout=60,
        )
        self.assertEqual(
            0,
            additive_result.returncode,
            additive_result.stdout + additive_result.stderr,
        )

        # Exercise the exact-prior atomic replacement independently with real
        # file hashes. The same function must preserve an unknown destination.
        upgrade_test = (
            f"$path='{install_path}';"
            "$tokens=$null;$errors=$null;"
            "$ast=[System.Management.Automation.Language.Parser]::"
            "ParseFile($path,[ref]$tokens,[ref]$errors);"
            "$fn=$ast.Find({param($node) $node -is "
            "[System.Management.Automation.Language.FunctionDefinitionAst] "
            "-and $node.Name -eq 'Copy-RecognizedPriorHashUpgrade'},$true);"
            "Invoke-Expression $fn.Extent.Text;"
            "$root=[IO.Path]::Combine([IO.Path]::GetTempPath(),"
            "'HeroV3Upgrade_'+[Guid]::NewGuid().ToString('N'));"
            "[IO.Directory]::CreateDirectory($root)|Out-Null;"
            "$source=[IO.Path]::Combine($root,'source.cpp');"
            "$dest=[IO.Path]::Combine($root,'dest.cpp');"
            "$relative='Source\\TRIADSensorFusionEditor\\Private\\TRIADIstanaPublicViewHeroV3EditorLibrary.cpp';"
            "try{"
            "[IO.File]::WriteAllText($source,'fixed');"
            "[IO.File]::WriteAllText($dest,'prior');"
            "$prior=(Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash.ToLowerInvariant();"
            "$replacement=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant();"
            "$map=@{$relative=@($prior,('f'*64))};"
            "$sourceMap=@{$relative=$replacement};"
            "Copy-RecognizedPriorHashUpgrade -SourceFile $source -DestinationFile $dest "
            "-RelativePath $relative -RecognizedPriorReplacementHashes $map "
            "-RecognizedReplacementSourceHashes $sourceMap;"
            "if([IO.File]::ReadAllText($dest) -cne 'fixed'){exit 1};"
            "if(@(Get-ChildItem -LiteralPath $root -Filter '*.hero_v3_*').Count -ne 0){exit 2};"
            "[IO.File]::WriteAllText($dest,'unknown');"
            "$before=(Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash;"
            "$rejected=$false;try{Copy-RecognizedPriorHashUpgrade -SourceFile $source "
            "-DestinationFile $dest -RelativePath $relative "
            "-RecognizedPriorReplacementHashes $map "
            "-RecognizedReplacementSourceHashes $sourceMap}catch{$rejected=$true};"
            "$after=(Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash;"
            "if(-not $rejected -or $before -cne $after){exit 3};"
            "[IO.File]::WriteAllText($dest,'prior');"
            "$badSourceMap=@{$relative=('0'*64)};"
            "$rejected=$false;try{Copy-RecognizedPriorHashUpgrade -SourceFile $source "
            "-DestinationFile $dest -RelativePath $relative "
            "-RecognizedPriorReplacementHashes $map "
            "-RecognizedReplacementSourceHashes $badSourceMap}catch{$rejected=$true};"
            "if(-not $rejected -or [IO.File]::ReadAllText($dest) -cne 'prior'){exit 4};"
            "$script:mode='';$script:destReads=0;"
            "function Get-FileHash{param($LiteralPath,$Algorithm)"
            "$real=Microsoft.PowerShell.Utility\\Get-FileHash -LiteralPath $LiteralPath -Algorithm $Algorithm;"
            "if($LiteralPath -eq $dest){$script:destReads++;"
            "if($script:mode -ceq 'late_target_toctou' -and $script:destReads -eq 2){"
            "[IO.File]::WriteAllText($dest,'changed-after-recheck');"
            "$script:mode='late_target_toctou_done';return $real};"
            "if($script:mode -ceq 'post_verify' -and $script:destReads -eq 3){"
            "return [pscustomobject]@{Hash=('0'*64)}};"
            "if($script:mode -ceq 'rollback_toctou' -and $script:destReads -eq 3){"
            "return [pscustomobject]@{Hash=('0'*64)}};"
            "if($script:mode -ceq 'rollback_toctou' -and $script:destReads -eq 4){"
            "[IO.File]::WriteAllText($dest,'changed-during-rollback');"
            "$script:mode='rollback_toctou_done';return $real}};"
            "if($LiteralPath -like '*.hero_v3_upgrade_*.tmp'){"
            "if($script:mode -ceq 'stage_mismatch'){return [pscustomobject]@{Hash=('0'*64)}};"
            "if($script:mode -ceq 'target_toctou'){[IO.File]::WriteAllText($dest,'changed-during-stage');"
            "$script:mode='target_toctou_done'}};"
            "if($LiteralPath -like '*.hero_v3_backup_*.tmp' -and "
            "$script:mode -ceq 'post_replace_dest_change'){"
            "[IO.File]::WriteAllText($dest,'post-replace-intervening-change');"
            "$script:mode='post_replace_dest_change_done'};return $real};"
            "[IO.File]::WriteAllText($dest,'prior');$script:mode='stage_mismatch';$script:destReads=0;"
            "$rejected=$false;try{Copy-RecognizedPriorHashUpgrade -SourceFile $source "
            "-DestinationFile $dest -RelativePath $relative "
            "-RecognizedPriorReplacementHashes $map "
            "-RecognizedReplacementSourceHashes $sourceMap}catch{$rejected=$true};"
            "if(-not $rejected -or [IO.File]::ReadAllText($dest) -cne 'prior' -or "
            "@(Get-ChildItem -LiteralPath $root -Filter '*.hero_v3_*').Count -ne 0){exit 5};"
            "[IO.File]::WriteAllText($dest,'prior');$script:mode='target_toctou';$script:destReads=0;"
            "$rejected=$false;try{Copy-RecognizedPriorHashUpgrade -SourceFile $source "
            "-DestinationFile $dest -RelativePath $relative "
            "-RecognizedPriorReplacementHashes $map "
            "-RecognizedReplacementSourceHashes $sourceMap}catch{$rejected=$true};"
            "if(-not $rejected -or [IO.File]::ReadAllText($dest) -cne 'changed-during-stage' -or "
            "@(Get-ChildItem -LiteralPath $root -Filter '*.hero_v3_*').Count -ne 0){exit 6};"
            "[IO.File]::WriteAllText($dest,'prior');$script:mode='late_target_toctou';$script:destReads=0;"
            "$rejected=$false;try{Copy-RecognizedPriorHashUpgrade -SourceFile $source "
            "-DestinationFile $dest -RelativePath $relative "
            "-RecognizedPriorReplacementHashes $map "
            "-RecognizedReplacementSourceHashes $sourceMap}catch{$rejected=$true};"
            "$residue=@(Get-ChildItem -LiteralPath $root -Filter '*.hero_v3_*').Count;"
            "if(-not $rejected -or [IO.File]::ReadAllText($dest) -cne 'changed-after-recheck' -or "
            "$residue -ne 0){Write-Output ('LATE_TOCTOU rejected='+$rejected+' content='+"
            "[IO.File]::ReadAllText($dest)+' residue='+$residue+' reads='+$script:destReads);exit 7};"
            "[IO.File]::WriteAllText($dest,'prior');$script:mode='post_replace_dest_change';$script:destReads=0;"
            "$message='';$rejected=$false;try{Copy-RecognizedPriorHashUpgrade -SourceFile $source "
            "-DestinationFile $dest -RelativePath $relative "
            "-RecognizedPriorReplacementHashes $map "
            "-RecognizedReplacementSourceHashes $sourceMap}catch{$rejected=$true;$message=$_.Exception.Message};"
            "$backups=@(Get-ChildItem -LiteralPath $root -Filter '*.hero_v3_backup_*.tmp');"
            "$otherResidue=@(Get-ChildItem -LiteralPath $root -Filter '*.hero_v3_*'|"
            "Where-Object{$_.Name -notlike '*.hero_v3_backup_*.tmp'}).Count;"
            "if(-not $rejected -or [IO.File]::ReadAllText($dest) -cne 'post-replace-intervening-change' -or "
            "$backups.Count -ne 1 -or [IO.File]::ReadAllText($backups[0].FullName) -cne 'prior' -or "
            "-not $message.Contains($backups[0].FullName) -or $otherResidue -ne 0){exit 8};"
            "[IO.File]::Delete($backups[0].FullName);"
            "[IO.File]::WriteAllText($dest,'prior');$script:mode='rollback_toctou';$script:destReads=0;"
            "$message='';$rejected=$false;try{Copy-RecognizedPriorHashUpgrade -SourceFile $source "
            "-DestinationFile $dest -RelativePath $relative "
            "-RecognizedPriorReplacementHashes $map "
            "-RecognizedReplacementSourceHashes $sourceMap}catch{$rejected=$true;$message=$_.Exception.Message};"
            "$backups=@(Get-ChildItem -LiteralPath $root -Filter '*.hero_v3_backup_*.tmp');"
            "$otherResidue=@(Get-ChildItem -LiteralPath $root -Filter '*.hero_v3_*'|"
            "Where-Object{$_.Name -notlike '*.hero_v3_backup_*.tmp'}).Count;"
            "if(-not $rejected -or [IO.File]::ReadAllText($dest) -cne 'changed-during-rollback' -or "
            "$backups.Count -ne 1 -or [IO.File]::ReadAllText($backups[0].FullName) -cne 'prior' -or "
            "-not $message.Contains($backups[0].FullName) -or $otherResidue -ne 0){exit 9};"
            "[IO.File]::Delete($backups[0].FullName);"
            "[IO.File]::WriteAllText($dest,'prior');$script:mode='post_verify';$script:destReads=0;"
            "$rejected=$false;try{Copy-RecognizedPriorHashUpgrade -SourceFile $source "
            "-DestinationFile $dest -RelativePath $relative "
            "-RecognizedPriorReplacementHashes $map "
            "-RecognizedReplacementSourceHashes $sourceMap}catch{$rejected=$true};"
            "$restored=(Microsoft.PowerShell.Utility\\Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash.ToLowerInvariant();"
            "$residue=@(Get-ChildItem -LiteralPath $root -Filter '*.hero_v3_*').Count;"
            "if(-not $rejected -or $restored -cne $prior -or $residue -ne 0){"
            "Write-Output ('POST_VERIFY rejected='+$rejected+' restored='+$restored+"
            "' prior='+$prior+' residue='+$residue+' reads='+$script:destReads);exit 10}"
            "}finally{[IO.Directory]::Delete($root,$true)}"
        )
        upgrade_result = subprocess.run(
            [
                powershell,
                "-NoProfile",
                "-NonInteractive",
                "-Command",
                upgrade_test,
            ],
            capture_output=True,
            text=True,
            timeout=60,
        )
        self.assertEqual(
            0,
            upgrade_result.returncode,
            upgrade_result.stdout + upgrade_result.stderr,
        )

    def test_08_public_api_and_ue55_regular_compile_surface_are_present(self) -> None:
        for function in (
            "ValidateIstanaPublicViewHeroV3RemoteControlProject",
            "ImportIstanaPublicViewHeroV3Assets",
            "ValidateIstanaPublicViewHeroV3Assets",
            "MigrateIstanaPublicViewExteriorMapToHeroV3",
            "ValidateIstanaPublicViewHeroV3Map",
            "ValidateIstanaPublicViewHeroV3PlayWorldReadiness",
            "CaptureIstanaPublicViewHeroV3QualityFrame",
            "QuiesceIstanaPublicViewHeroV3PlayWorldForStop",
        ):
            self.assertIn(function, self.header)
            self.assertIn(function, self.cpp)
        self.assertIn("ERHIFeatureLevel::SM5", self.cpp)
        self.assertNotIn("ERHIFeatureLevel::SM6", self.cpp)
        self.assertIn("UE5.5", self.cpp)
        self.assertIn('#include "StaticMeshAttributes.h"', self.cpp)

        # ImportedMaterialSlotName is declared with STATICMESHDESCRIPTION_API;
        # a header-only TU compile can succeed through transitive include paths
        # while the modular editor link still fails without its owning import
        # library. Require one direct private dependency on that exact module.
        private_dependencies = re.search(
            r"PrivateDependencyModuleNames\.AddRange\(new\[\]\s*"
            r"\{(?P<body>.*?)\}\);",
            self.build_cs,
            re.DOTALL,
        )
        self.assertIsNotNone(private_dependencies)
        self.assertEqual(
            1,
            len(
                re.findall(
                    r'"StaticMeshDescription"',
                    private_dependencies.group("body"),
                )
            ),
        )
        self.assertEqual(1, self.build_cs.count('"StaticMeshDescription"'))

    def test_09_editor_launch_and_pie_are_exact_no_lidar_and_fail_closed(self) -> None:
        self.assertEqual(910, LEGACY_VISUAL_SETTINGS_PATH.stat().st_size)
        self.assertEqual(
            "1158e33c61660b7e3d6ad64f5d3c86d60e2c7865c6b59c9d454938cc802c1ee0",
            sha256(LEGACY_VISUAL_SETTINGS_PATH),
        )
        self.assertEqual(1022, HERO_V3_VISUAL_SETTINGS_PATH.stat().st_size)
        self.assertEqual(
            "6120769876168fe972f11950b72550539ee1fa2ca3cd7e0fab2d0d1a2eabe64c",
            sha256(HERO_V3_VISUAL_SETTINGS_PATH),
        )
        v3_profile_text = HERO_V3_VISUAL_SETTINGS_PATH.read_text(encoding="utf-8")
        v3_profile = json.loads(v3_profile_text)
        self.assertEqual(
            "ISTANA_PUBLIC_VIEW_HERO_V3_VISUAL_ACCEPTANCE_ONLY",
            v3_profile["TRIADProfile"],
        )
        self.assertIs(v3_profile["TRIADProductionSensorProfile"], False)
        self.assertEqual("ComputerVision", v3_profile["SimMode"])
        self.assertIs(v3_profile["RpcEnabled"], False)
        self.assertIs(v3_profile["EnableRpc"], False)
        self.assertEqual("127.0.0.1", v3_profile["LocalHostIp"])
        self.assertEqual(41451, v3_profile["ApiServerPort"])
        self.assertEqual(1, v3_profile_text.count('"RpcEnabled"'))
        self.assertEqual(1, v3_profile_text.count('"EnableRpc"'))

        launcher = self.scripts["start"]
        for marker in (
            "StartIstanaPublicViewHeroV3Editor",
            "6120769876168fe972f11950b72550539ee1fa2ca3cd7e0fab2d0d1a2eabe64c",
            "IstanaPublicViewHeroV3VisualAcceptance.settings.json",
            "ISTANA_PUBLIC_VIEW_HERO_V3_VISUAL_ACCEPTANCE_ONLY",
            "RpcEnabled",
            "EnableRpc",
            "Close every Unreal Editor before the guarded Hero-V3 launch",
            "BOOTSTRAP_V2_FOR_HERO_V3_IMPORT_AND_MIGRATION",
            "VALIDATE_EXISTING_HERO_V3",
            "Istana_PublicView_Exterior_v2.umap",
            "Istana_PublicView_Exterior_v3.umap",
            "-TRIADIstanaVisualAcceptance",
            "-RCWebControlEnable",
            "WebControl.StartServer",
            "http://127.0.0.1:30010",
            "/remote/info",
            "ValidateIstanaPublicViewHeroV3RemoteControlProject",
            "ValidateIstanaPublicViewHeroV2Map",
            "ValidateIstanaPublicViewHeroV3Map",
            "$protectedHashesBefore",
            "$protectedHashesAfter",
            "ProtectedDiskBytesUnchanged",
            "ActualCommandLineVerified",
            "Assert-ExactHeroV3EditorCommandLine",
        ):
            self.assertIn(marker, launcher)
        self.assertRegex(
            launcher,
            r"\$v3MapPresent\s*=\s*Test-Path[\s\S]{0,500}"
            r"if\s*\(\$v3MapPresent\)[\s\S]{0,200}"
            r"Istana_PublicView_Exterior_v3",
        )
        self.assertNotIn("Copy-Item", launcher)
        self.assertNotIn("Set-Content", launcher)

        pie = self.scripts["pie"]
        for marker in (
            "Assert-ExactHeroV3VisualAcceptanceProfile",
            "6120769876168fe972f11950b72550539ee1fa2ca3cd7e0fab2d0d1a2eabe64c",
            "IstanaPublicViewHeroV3VisualAcceptance.settings.json",
            "ISTANA_PUBLIC_VIEW_HERO_V3_VISUAL_ACCEPTANCE_ONLY",
            "TRIADProductionSensorProfile",
            "RpcEnabled",
            "EnableRpc",
            "DefaultSensors",
            "SensorType",
            "lidar",
            "Assert-ExactHeroV3EditorCommandLine",
            "$anySettingsPattern",
            "$anyProjectPattern",
            "$visualFlagPattern",
            "VisualAcceptanceProfileVerified",
        ):
            self.assertIn(marker, pie)
        profile_call = pie.find(
            "$visualProfile = Assert-ExactHeroV3VisualAcceptanceProfile"
        )
        identity_call = pie.find("$identity = Invoke-RemoteObjectCall")
        stop_branch = pie.find("if ($Stop)")
        begin_play = pie.find("EditorRequestBeginPlay")
        self.assertGreaterEqual(profile_call, 0)
        self.assertGreater(identity_call, profile_call)
        self.assertGreater(stop_branch, identity_call)
        self.assertGreater(begin_play, stop_branch)

        for marker in (
            "ValidateHeroV3VisualAcceptanceProcessProfile",
            "FrozenVisualAcceptanceSettingsSha256",
            "TRIADIstanaVisualAcceptance",
            "IstanaPublicViewHeroV3VisualAcceptance.settings.json",
            "Bytes != 1022",
            "RpcEnabled",
            "EnableRpc",
            "Direct AirSim RPC state is not introspected across the plugin module boundary",
            "DefaultSensors",
            "Sensors",
            "arbitrary sensor process profile",
        ):
            self.assertIn(marker, self.cpp)

        powershell = shutil.which("pwsh") or shutil.which("powershell")
        if powershell is not None:
            for script_name in ("start", "pie"):
                script_path = str(SCRIPT_PATHS[script_name]).replace("'", "''")
                command = (
                    f"$path='{script_path}';"
                    "$tokens=$null;$errors=$null;"
                    "$ast=[System.Management.Automation.Language.Parser]::"
                    "ParseFile($path,[ref]$tokens,[ref]$errors);"
                    "$fn=$ast.Find({param($node) $node -is "
                    "[System.Management.Automation.Language.FunctionDefinitionAst] "
                    "-and $node.Name -eq 'Assert-ExactHeroV3EditorCommandLine'},$true);"
                    "Invoke-Expression $fn.Extent.Text;"
                    "$project='C:\\TRIAD Project\\TRIAD.uproject';"
                    "$settings='C:\\TRIAD Project\\Config\\IstanaPublicViewHeroV3VisualAcceptance.settings.json';"
                    "$safe='\"{0}\" \"{1}\" -settings=\"{2}\" "
                    "-TRIADIstanaVisualAcceptance' -f "
                    "'C:\\UE\\UnrealEditor.exe',$project,$settings;"
                    "Assert-ExactHeroV3EditorCommandLine -CommandLine $safe "
                    "-ProjectFile $project -SettingsPath $settings|Out-Null;"
                    "$bad=@("
                    "$safe+' -settings=\"C:\\ProductionLidar.json\"',"
                    "'\"C:\\UE\\UnrealEditor.exe\" --settings=\"C:\\ProductionLidar.json\" '+$safe.Substring($safe.IndexOf('\"'+$project+'\"')),"
                    "'\"C:\\UE\\UnrealEditor.exe\" -foo-settings=\"C:\\ProductionLidar.json\" '+$safe.Substring($safe.IndexOf('\"'+$project+'\"')),"
                    "$safe+' -TRIADIstanaVisualAcceptance',"
                    "$safe+' \"C:\\Other\\Other.uproject\"');"
                    "foreach($candidate in $bad){$rejected=$false;try{"
                    "Assert-ExactHeroV3EditorCommandLine -CommandLine $candidate "
                    "-ProjectFile $project -SettingsPath $settings|Out-Null"
                    "}catch{$rejected=$true};if(-not $rejected){exit 1}}"
                )
                result = subprocess.run(
                    [
                        powershell,
                        "-NoProfile",
                        "-NonInteractive",
                        "-Command",
                        command,
                    ],
                    capture_output=True,
                    text=True,
                    timeout=60,
                )
                self.assertEqual(
                    0,
                    result.returncode,
                    script_name + ": " + result.stdout + result.stderr,
                )

        install = self.scripts["install"]
        for marker in (
            "IstanaPublicViewHeroV3VisualAcceptance.settings.json",
            "Assert-ExactHeroV3VisualAcceptanceSettings",
            "6120769876168fe972f11950b72550539ee1fa2ca3cd7e0fab2d0d1a2eabe64c",
            "Copy-NewOrEqualHashFile",
            "$sourceVisualSettings",
            "$targetVisualSettings",
        ):
            self.assertIn(marker, install)


if __name__ == "__main__":
    unittest.main()
