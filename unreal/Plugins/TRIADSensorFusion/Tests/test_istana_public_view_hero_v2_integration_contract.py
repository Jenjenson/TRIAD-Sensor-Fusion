import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


PLUGIN_ROOT = Path(__file__).resolve().parents[1]
UNREAL_ROOT = PLUGIN_ROOT.parent.parent
REPOSITORY_ROOT = UNREAL_ROOT.parent
GEOMETRY_ROOT = UNREAL_ROOT / "SourceAssets" / "IstanaPublicViewV2"
MATERIAL_ROOT = (
    UNREAL_ROOT / "SourceAssets" / "IstanaPublicView" / "HeroMaterialsV2"
)
CPP_PATH = (
    PLUGIN_ROOT
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaPublicViewHeroV2EditorLibrary.cpp"
)
HEADER_PATH = (
    PLUGIN_ROOT
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Public"
    / "TRIADIstanaPublicViewHeroV2EditorLibrary.h"
)
INTEGRATION_FREEZE_PATH = (
    PLUGIN_ROOT
    / "Resources"
    / "IstanaPublicViewHeroV2.integration.freeze.json"
)
SCRIPT_PATHS = {
    "install": REPOSITORY_ROOT
    / "scripts"
    / "Install-IstanaPublicViewHeroV2DevelopmentAssets.ps1",
    "import": REPOSITORY_ROOT
    / "scripts"
    / "Import-IstanaPublicViewHeroV2Assets.ps1",
    "migrate": REPOSITORY_ROOT
    / "scripts"
    / "Migrate-IstanaPublicViewHeroV2Map.ps1",
}

EXPECTED_SLOTS = {
    "M_IPV2_DarkTimber",
    "M_IPV2_Door",
    "M_IPV2_Glass",
    "M_IPV2_Metal",
    "M_IPV2_Opaline",
    "M_IPV2_Recess",
    "M_IPV2_Render",
    "M_IPV2_Shutter",
    "M_IPV2_Slate",
    "M_IPV2_Stone",
    "M_IPV2_Trim",
}
EXPECTED_V1_HASHES = {
    "SourceAssets/IstanaPublicView/public_view_geometry.py":
        "f46b05c5e5a53a3fcb4392735a9cab15fef9dc8f6e8e0d7fb30395b3f8b82023",
    "SourceAssets/IstanaPublicView/generate_public_view_building.py":
        "fbc11e95d42232d3b7a57fa8e6e5b79248c30e168668ffa10146dad76b309985",
    "SourceAssets/IstanaPublicView/generate_public_view_context.py":
        "21681279cb303af143b6e28bb17906becdceda7c4c7d1d154cf8964c11ac0e79",
    "SourceAssets/IstanaPublicView/istana_public_view.contract.json":
        "c068cf3ceb731c85b28e3c8e6292fe92bb5463e792742dd4a0b81525bd84baee",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


class IstanaPublicViewHeroV2IntegrationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.cpp = CPP_PATH.read_text(encoding="utf-8")
        cls.header = HEADER_PATH.read_text(encoding="utf-8")
        cls.integration_freeze = load_json(INTEGRATION_FREEZE_PATH)
        cls.geometry_contract = load_json(
            GEOMETRY_ROOT / "istana_public_view_hero_v2.contract.json"
        )
        cls.geometry_freeze = load_json(GEOMETRY_ROOT / "hero_v2.freeze.json")
        cls.material_interface = load_json(
            MATERIAL_ROOT / "unreal_asset_interface.v2.json"
        )
        cls.scripts = {
            name: path.read_text(encoding="utf-8")
            for name, path in SCRIPT_PATHS.items()
        }

    def test_01_integration_freeze_binds_every_exact_source_and_v1_byte(self) -> None:
        freeze = self.integration_freeze
        self.assertEqual(
            "triad.istana_public_view_hero_v2.unreal_freeze.v1",
            freeze["schema"],
        )
        self.assertEqual("/Game/Maps/Istana_PublicView_Exterior_v1", freeze["sourceMap"])
        self.assertEqual("/Game/Maps/Istana_PublicView_Exterior_v2", freeze["destinationMap"])
        self.assertFalse(freeze["naniteEnabled"])
        self.assertFalse(freeze["sm6Required"])
        self.assertTrue(freeze["reusesV1Collision"])
        self.assertFalse(freeze["changesSurroundings"])

        roles = {record["role"] for record in freeze["files"]}
        self.assertEqual(
            {
                "GEOMETRY_FREEZE",
                "GEOMETRY_CONTRACT",
                "GEOMETRY_GENERATOR",
                "GEOMETRY_MANIFEST",
                "BUILDING_HERO_VISUAL_V2",
                "HERO_MATERIAL_CONTRACT",
                "HERO_MATERIAL_UNREAL_INTERFACE",
                "HERO_MATERIAL_SOURCE_MANIFEST",
                "HERO_MATERIAL_GENERATED_MANIFEST",
                "HERO_MATERIAL_BUILDER",
            },
            roles,
        )
        for record in freeze["files"] + freeze["protectedV1Files"]:
            path = UNREAL_ROOT / Path(record["relativePath"])
            self.assertTrue(path.is_file(), record["relativePath"])
            self.assertEqual(record["bytes"], path.stat().st_size)
            self.assertEqual(record["sha256"], sha256(path))

        actual_v1 = {
            record["relativePath"]: record["sha256"]
            for record in freeze["protectedV1Files"]
        }
        self.assertEqual(EXPECTED_V1_HASHES, actual_v1)

        match = re.search(
            r'FrozenFreezeSha256\(\s*TEXT\("([0-9a-f]{64})"\)\)',
            self.cpp,
        )
        self.assertIsNotNone(match)
        self.assertEqual(sha256(INTEGRATION_FREEZE_PATH), match.group(1))

    def test_02_geometry_and_material_slot_contracts_are_exact_v2_only(self) -> None:
        self.assertEqual(EXPECTED_SLOTS, set(self.geometry_contract["expectedMaterialSlots"]))
        self.assertEqual(EXPECTED_SLOTS, set(self.geometry_freeze["materialSlots"]))
        self.assertEqual(496_644, self.geometry_freeze["heroTriangles"])
        self.assertEqual(198_314_153, self.geometry_freeze["heroBytes"])
        self.assertEqual(
            "b4d2dee9fe267314f31f72585287a2e66f523adbbe664d5f521917375fbe755f",
            self.geometry_freeze["heroSha256"],
        )

        interface_slots = set()
        for instance in self.material_interface["instances"]:
            interface_slots.update(instance["replaceSlotsV2"])
        interface_slots.update(self.material_interface["v2SpecialSlotBindings"])
        self.assertEqual(EXPECTED_SLOTS, interface_slots)
        for slot in EXPECTED_SLOTS:
            self.assertIn(f'TEXT("{slot}")', self.cpp)
            self.assertFalse(slot.startswith("M_IPV_"))
        self.assertNotIn('TEXT("M_IPV_Glass")', self.cpp)
        self.assertIn("constexpr int32 ExpectedHeroTriangles = 496644", self.cpp)
        self.assertIn("constexpr int32 ExpectedMaterialTextureCount = 57", self.cpp)

    def test_03_regular_sm5_path_is_explicit_and_global_renderer_changes_are_absent(self) -> None:
        unreal_import = self.geometry_contract["surfaceContract"]["unrealImport"]
        self.assertFalse(unreal_import["naniteEnabled"])
        self.assertFalse(unreal_import["requiresShaderModel6Change"])
        self.assertFalse(self.geometry_freeze["renderingDefaults"]["naniteEnabled"])
        self.assertFalse(
            self.geometry_freeze["renderingDefaults"]["requiresShaderModel6Change"]
        )
        for required in (
            "Hero->NaniteSettings.bEnabled = false",
            "Mesh->NaniteSettings.bEnabled",
            "ERHIFeatureLevel::SM5",
            "requiresShaderModel6",
        ):
            self.assertIn(required, self.cpp)
        for forbidden in (
            "PCD3D_SM6",
            "D3D12TargetedShaderFormats",
            "r.ShaderModel",
            "r.Nanite",
            "DefaultEngine.ini\"",
        ):
            self.assertNotIn(forbidden, self.cpp)
        for script in self.scripts.values():
            self.assertNotRegex(
                script,
                r"(?i)(Set|Add)-Content[^\n]*DefaultEngine\.ini|Copy-Item[^\n]*DefaultEngine\.ini",
            )

    def test_04_import_is_non_overwriting_and_retains_frozen_provenance(self) -> None:
        for required in (
            "ValidateSingleSourceProvenance",
            "FMD5Hash::HashFile",
            "GetSourceData().SourceFiles",
            "FileHash",
            "EHeroV2AssetState::PartialOrInvalid",
            "Refusing to overwrite existing v2 destination map",
            "DoesObjectOrPackageExist",
            "ImportNormals",
            "bRecomputeNormals = false",
            "bRecomputeTangents = true",
            "bUseMikkTSpace = true",
        ):
            self.assertIn(required, self.cpp)
        for name, script in self.scripts.items():
            self.assertNotIn("D:\\triad", script)
            self.assertRegex(
                script,
                r"\[Parameter\(Mandatory\s*=\s*\$true\)\][\s\S]{0,100}\[string\]\s+\$ProjectPath",
                name,
            )
        self.assertIn("Assert-NonOverwritingFileCopy", self.scripts["install"])
        self.assertIn("Copy-NewOrEqualHashFile", self.scripts["install"])
        self.assertIn("Refusing to overwrite existing v2 destination map", self.scripts["migrate"])

    def test_05_material_graph_and_exact_mic_overrides_are_fail_closed(self) -> None:
        for required in (
            "CreateStaticBoolParameter",
            "SealHeroMaterialGraph",
            "ValidateHeroMaterialGraphSeal",
            "ComputeHeroMaterialGraphSha256",
            "GatherReachableMaterialExpressions",
            "ValidateCustomExpressionTopology",
            "FinishAndValidateMaterialCompilation",
            "TRIAD_IPV2_NormalRNM",
            "TRIAD_IPV2_GlassNormalRNM",
            "UMaterialExpressionBumpOffset",
            "forbidden duplicate-prone StaticSwitchParameter",
        ):
            self.assertIn(required, self.cpp)

        opaque = self.material_interface["masterMaterials"]["opaque"]
        glass = self.material_interface["masterMaterials"]["glass"]
        parameter_names = (
            set(opaque["textureParameters"])
            | set(opaque["scalarParameters"])
            | set(opaque["vectorParameters"])
            | set(opaque["staticSwitches"])
            | set(glass["textureParameters"])
            | set(glass["scalarParameters"])
            | set(glass["vectorParameters"])
        )
        for parameter in parameter_names:
            self.assertIn(f'TEXT("{parameter}")', self.cpp, parameter)
        for required in (
            "ValidateHeroMaterialInstance",
            "ScalarParameterValues",
            "VectorParameterValues",
            "TextureParameterValues",
            "StaticParameters.StaticSwitchParameters",
        ):
            self.assertIn(required, self.cpp)

    def test_06_only_the_visual_hero_changes_and_effective_materials_are_verified(self) -> None:
        for required in (
            "ValidateEffectiveHeroV2Materials",
            "BuildingHeroVisualComponent->SetStaticMesh(Hero)",
            "BuildingHeroVisualComponent->SetCollisionEnabled",
            "V1CollisionObjectPath",
            "ValidateKnownV1Surroundings",
            "ValidateSurroundingsUnchanged",
        ):
            self.assertIn(required, self.cpp)
        self.assertEqual(
            1,
            self.cpp.count("BuildingHeroVisualComponent->SetStaticMesh(Hero)"),
        )
        for component in (
            "BuildingCollisionComponent",
            "TerrainComponent",
            "TerrainSkirtComponent",
            "HardscapeComponent",
            "ContextBuildingsComponent",
            "OSMContextBuildingsComponent",
            "RainTreeInstances",
            "PalmTreeInstances",
            "FramingTreeInstances",
        ):
            self.assertIn(component, self.cpp)
            self.assertNotIn(f"{component}->SetStaticMesh", self.cpp)
        for forbidden_slot in self.material_interface["forbiddenReplacements"]:
            self.assertNotIn(f'TEXT("{forbidden_slot}")', self.cpp)

    def test_07_full_v1_component_camera_and_umap_sha_baseline_is_reported(self) -> None:
        for required in (
            "CaptureSurroundings",
            "CapturePreservedComponent",
            "SameCameraState",
            "EffectiveMaterialPaths",
            "OverrideMaterialPaths",
            "InstanceTransforms",
            "PerInstanceCustomData",
            "CollisionResponses",
            "WriteMigrationInvariantReport",
            "sourceMapSha256Before",
            "sourceMapSha256After",
            "sourceMapByteIdentical",
            "componentsExact",
            "camerasExact",
            "Istana_PublicView_Exterior_v2.invariants.json",
        ):
            self.assertIn(required, self.cpp)
        self.assertIn("TActorIterator<ACameraActor>", self.cpp)
        self.assertIn("SourceShaBefore != SourceShaAfter", self.cpp)
        migrate_script = self.scripts["migrate"]
        for required in (
            "Get-FileHash -LiteralPath $sourceMap -Algorithm SHA256",
            "sourceMapSha256Before",
            "sourceMapSha256After",
            "componentsExact",
            "camerasExact",
            "changesSurroundings",
            "requiresNanite",
            "requiresShaderModel6",
        ):
            self.assertIn(required, migrate_script)

    def test_08_wrappers_validate_before_mutation_and_protect_v1_context(self) -> None:
        for script in self.scripts.values():
            self.assertIn("validate_hero_v2.py", script)
            self.assertIn("build_hero_materials_v2.py", script)
            self.assertIn("--validate-only", script)
            self.assertIn("DefaultEngine.ini", script)
            self.assertIn("Istana_PublicView_Exterior_v1.umap", script)
            self.assertIn("Content\\TRIAD\\Istana", script)
            self.assertIn("Content\\TRIAD\\IstanaDigitalTwin", script)
            self.assertIn("Get-ProtectedSnapshot", script)
            self.assertIn("Assert-SameSnapshot", script)
        self.assertIn("Install-IstanaDevelopmentAssets.ps1", self.scripts["install"])
        for required in (
            "sourceReferenceRoot",
            "targetReferenceRoot",
            "referenceFiles",
            "IstanaDigitalTwin\\IstanaPublicView",
            "LicensedReferenceFileCount",
        ):
            self.assertIn(required, self.scripts["install"])
        self.assertIn("ImportIstanaPublicViewHeroV2Assets", self.scripts["import"])
        self.assertIn("ValidateIstanaPublicViewHeroV2Assets", self.scripts["import"])
        self.assertIn("MigrateIstanaPublicViewExteriorMapToHeroV2", self.scripts["migrate"])
        self.assertIn("ValidateIstanaPublicViewHeroV2Map", self.scripts["migrate"])

    def test_09_wrapper_powershell_syntax_is_valid(self) -> None:
        powershell = shutil.which("powershell") or shutil.which("pwsh")
        if not powershell:
            self.skipTest("PowerShell parser is unavailable on this host.")
        for name, path in SCRIPT_PATHS.items():
            escaped_path = str(path).replace("'", "''")
            parser = (
                "$tokens=$null; $errors=$null; "
                "[System.Management.Automation.Language.Parser]::ParseFile("
                f"'{escaped_path}',[ref]$tokens,[ref]$errors) | Out-Null; "
                "if($errors.Count -ne 0){$errors | ForEach-Object {Write-Error $_}; exit 1}"
            )
            completed = subprocess.run(
                [powershell, "-NoProfile", "-NonInteractive", "-Command", parser],
                check=False,
                capture_output=True,
                text=True,
                timeout=30,
            )
            self.assertEqual(
                0,
                completed.returncode,
                f"{name}: {completed.stdout}\n{completed.stderr}",
            )

    def test_10_embedded_texture_and_mesh_payloads_are_stale_edit_sealed(self) -> None:
        for required in (
            "SealEmbeddedPayload",
            "ValidateEmbeddedPayloadSeal",
            "texture_source_id",
            "mesh_render_ddc_and_lighting",
            "DerivedDataKey",
            "LightingGuid",
        ):
            self.assertIn(required, self.cpp)
        self.assertRegex(self.cpp, r"Source\s*\.\s*GetId(?:String)?\s*\(")
        self.assertGreaterEqual(self.cpp.count("ValidateEmbeddedPayloadSeal("), 3)
        self.assertGreaterEqual(self.cpp.count("SealEmbeddedPayload("), 3)

    def test_11_ue55_compile_surface_uses_supported_set_filter_and_mobility_apis(self) -> None:
        self.assertIn("AreSetsEqual", self.cpp)
        self.assertIn("Left.Includes(Right)", self.cpp)
        for invalid_comparison in (
            "ActualSet == ExpectedSet",
            "DiskPngSet != SeenFiles",
            "ExpectedScalarNames != ActualScalarNames",
            "ExpectedVectorNames != ActualVectorNames",
            "ExpectedTextureNames != ActualTextureNames",
            "ExpectedStaticNames != ActualStaticNames",
        ):
            self.assertNotIn(invalid_comparison, self.cpp)
        self.assertNotRegex(
            self.cpp,
            r"Texture->Filter\s*(?:=|!=)\s*TF_Anisotropic",
        )
        self.assertGreaterEqual(self.cpp.count("Texture->Filter"), 2)
        self.assertGreaterEqual(self.cpp.count("TF_Default"), 2)
        self.assertNotIn("Component->GetMobility()", self.cpp)
        self.assertIn("Component->Mobility", self.cpp)
        for binding in (
            "StaticBoolParameter",
            "TextureParameter",
            "ScalarParameter",
            "VectorParameter",
        ):
            self.assertIn(binding, self.cpp)

    def test_12_sha256_is_streaming_nontrapping_and_matches_standard_vectors(self) -> None:
        for forbidden in (
            "GetSHA256Signature",
            "FSHA256Signature",
            "LoadFileToArray",
        ):
            self.assertNotIn(forbidden, self.cpp)
        for required in (
            "FStreamingSha256",
            "VerifyKnownVectors",
            "CalculateSha256Bytes",
            "CreateFileReader",
            "FILEREAD_Silent",
            "Reader->Serialize",
            "Reader->IsError()",
            "OriginalBitCount",
            '"0123456789abcdef"',
            "TotalBytes <= 0",
        ):
            self.assertIn(required, self.cpp)
        serialize_at = self.cpp.index("Reader->Serialize")
        read_error_at = self.cpp.index("Reader->IsError()", serialize_at)
        update_at = self.cpp.index("Hasher.Update", serialize_at)
        self.assertLess(serialize_at, read_error_at)
        self.assertLess(read_error_at, update_at)

        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("A standalone C++ compiler is unavailable on this host.")
        begin_marker = "// TRIAD_HERO_SHA256_HOST_BEGIN"
        end_marker = "// TRIAD_HERO_SHA256_HOST_END"
        begin = self.cpp.index(begin_marker) + len(begin_marker)
        end = self.cpp.index(end_marker, begin)
        helper = self.cpp[begin:end]
        harness = """
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
""" + helper + r"""
int main(int ArgumentCount, char** Arguments)
{
    using namespace TriadHeroSha256;
    if (!VerifyKnownVectors())
    {
        return 1;
    }
    std::array<std::uint8_t, 1000> Chunk{};
    Chunk.fill(static_cast<std::uint8_t>('a'));
    FStreamingSha256 MillionA;
    for (int Index = 0; Index < 1000; ++Index)
    {
        if (!MillionA.Update(Chunk.data(), Chunk.size()))
        {
            return 2;
        }
    }
    if (!HasDigest(
            MillionA,
            "cdc76e5c9914fb9281a1c7e284d73e67"
            "f1809a48a497200e046d39ccc7112cd0"))
    {
        return 3;
    }
    if (ArgumentCount == 2 &&
        std::strcmp(Arguments[1], "--extended-4gib") == 0)
    {
        static const std::array<std::uint8_t, 1024 * 1024> ZeroChunk{};
        FStreamingSha256 OverFourGib;
        for (int Index = 0; Index < 4097; ++Index)
        {
            if (!OverFourGib.Update(ZeroChunk.data(), ZeroChunk.size()))
            {
                return 4;
            }
        }
        if (!HasDigest(
                OverFourGib,
                "829816e339ff597ec3ada4c30fc840d3f"
                "2298444169d242952a54bcf3fcd7747"))
        {
            return 5;
        }
    }
    return 0;
}
"""
        with tempfile.TemporaryDirectory(prefix="triad_hero_sha256_") as temp:
            temp_root = Path(temp)
            source = temp_root / "sha256_vectors.cpp"
            executable = temp_root / "sha256_vectors.exe"
            source.write_text(harness, encoding="utf-8")
            compiled = subprocess.run(
                [
                    compiler,
                    "-std=c++20",
                    "-O2",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    str(source),
                    "-o",
                    str(executable),
                ],
                check=False,
                capture_output=True,
                text=True,
                timeout=60,
            )
            self.assertEqual(
                0,
                compiled.returncode,
                f"{compiled.stdout}\n{compiled.stderr}",
            )
            exercise_command = [str(executable)]
            extended = os.environ.get("TRIAD_SHA256_EXTENDED_VECTOR") == "1"
            if extended:
                exercise_command.append("--extended-4gib")
            exercised = subprocess.run(
                exercise_command,
                check=False,
                capture_output=True,
                text=True,
                timeout=300 if extended else 60,
            )
            self.assertEqual(
                0,
                exercised.returncode,
                f"{exercised.stdout}\n{exercised.stderr}",
            )

    def test_13_custom_nodes_replace_ue55_placeholder_and_diagnose_each_pin(self) -> None:
        create_start = self.cpp.index("UMaterialExpressionCustom* CreateCustomExpression(")
        create_end = self.cpp.index(
            "UMaterialExpressionTextureSampleParameter2D* CreateTextureParameter(",
            create_start,
        )
        create_body = self.cpp[create_start:create_end]
        reset = create_body.index("Expression->Inputs.Reset(InputNames.Num())")
        append_loop = create_body.index("for (const FName& InputName : InputNames)")
        append = create_body.index("Inputs.AddDefaulted_GetRef()")
        self.assertLess(reset, append_loop)
        self.assertLess(append_loop, append)

        validate_start = self.cpp.index("bool ValidateCustomExpressionTopology(")
        validate_end = self.cpp.index(
            "bool FinishAndValidateMaterialCompilation(",
            validate_start,
        )
        validate_body = self.cpp[validate_start:validate_end]
        for required in (
            "for (int32 InputIndex = 0;",
            "input[%d] of %d is unnamed",
            "duplicates an earlier input name",
            "is directly disconnected",
            "traces to no source expression",
            "is not referenced as an exact HLSL identifier",
            "GetTracedInput()",
            "CustomCodeUsesIdentifier",
        ):
            self.assertIn(required, validate_body)
        self.assertNotIn(
            "has an empty, duplicate, disconnected, or code-unused input",
            validate_body,
        )
        self.assertEqual(9, self.cpp.count("CreateCustomExpression("))
        for description in (
            "TRIAD_IPV2_HeightRatio",
            "TRIAD_IPV2_OpaqueAlbedo",
            "TRIAD_IPV2_OpaqueRoughness",
            "TRIAD_IPV2_OpaqueMetallic",
            "TRIAD_IPV2_NormalRNM",
            "TRIAD_IPV2_GlassAlbedo",
            "TRIAD_IPV2_GlassRoughness",
            "TRIAD_IPV2_GlassNormalRNM",
        ):
            self.assertIn(description, self.cpp)

    def test_14_mic_roster_handles_ue55_inactive_base_property_cache_exactly(self) -> None:
        validate_start = self.cpp.index("bool ValidateHeroMaterialInstance(")
        validate_end = self.cpp.index(
            "bool ComputeHeroMeshPayloadDigest(",
            validate_start,
        )
        validate_body = self.cpp[validate_start:validate_end]
        helper_start = self.cpp.index("FString EnabledBasePropertyOverrideNames(")
        helper_end = validate_start
        helper_body = self.cpp[helper_start:helper_end]

        self.assertNotIn(
            "BasePropertyOverrides !=",
            validate_body,
        )
        self.assertNotIn("HasOverridenBaseProperties", validate_body)
        for override_flag in (
            "bOverride_OpacityMaskClipValue",
            "bOverride_BlendMode",
            "bOverride_ShadingModel",
            "bOverride_DitheredLODTransition",
            "bOverride_CastDynamicShadowAsMasked",
            "bOverride_TwoSided",
            "bOverride_bIsThinSurface",
            "bOverride_OutputTranslucentVelocity",
            "bOverride_bHasPixelAnimation",
            "bOverride_bEnableTessellation",
            "bOverride_DisplacementScaling",
            "bOverride_bEnableDisplacementFade",
            "bOverride_DisplacementFadeRange",
            "bOverride_MaxWorldPositionOffsetDisplacement",
        ):
            self.assertIn(f"Overrides.{override_flag}", helper_body)
            self.assertIn(f'TEXT("{override_flag}")', helper_body)
        for override_kind in (
            "scalar",
            "vector",
            "texture",
            "static-switch",
        ):
            self.assertIn(
                f'Spec.AssetName, TEXT("{override_kind}")',
                validate_body,
            )
        for diagnostic in (
            "missing=[%s]; unexpected=[%s]; expected=[%s]; actual=[%s]",
            "unexpected %s override(s)",
            "unexpected material-layer override",
            "enables forbidden base-property override flag(s): [%s]",
            "0.3333f versus 0.333333f opacity clip",
        ):
            self.assertIn(diagnostic, self.cpp)

    def test_15_mic_static_permutation_is_submitted_finished_and_diagnosed(self) -> None:
        create_start = self.cpp.index("UMaterialInstanceConstant* CreateHeroMaterialInstance(")
        create_end = self.cpp.index("bool SetEquals(", create_start)
        create_body = self.cpp[create_start:create_end]
        set_static = create_body.index("SetStaticSwitchParameterValueEditorOnly")
        update_static = create_body.index("Instance->UpdateStaticPermutation()", set_static)
        post_edit = create_body.index("Instance->PostEditChange()", update_static)
        ensure_complete = create_body.index("Instance->EnsureIsComplete()", post_edit)
        mark_dirty = create_body.index("Instance->MarkPackageDirty()", ensure_complete)
        self.assertLess(set_static, update_static)
        self.assertLess(update_static, post_edit)
        self.assertLess(post_edit, ensure_complete)
        self.assertLess(ensure_complete, mark_dirty)
        self.assertNotIn("CacheShaders(", create_body)

        validate_start = self.cpp.index("bool ValidateHeroMaterialInstance(")
        validate_end = self.cpp.index(
            "bool ComputeHeroMeshPayloadDigest(",
            validate_start,
        )
        validate_body = self.cpp[validate_start:validate_end]
        validator_ensure = validate_body.index("Instance->EnsureIsComplete()")
        get_resource = validate_body.index(
            "Instance->GetMaterialResource(ERHIFeatureLevel::SM5)",
            validator_ensure,
        )
        self.assertLess(validator_ensure, get_resource)
        self.assertNotIn("InstanceResource->FinishCompilation()", validate_body)
        self.assertIn("InstanceResource->GetMaterialInstance() == Instance", validate_body)
        for required in (
            "bRequiresMicOwnedStaticResource",
            "bResourcePresent",
            "bOwnedByInstance",
            "bInstanceIsCompiling",
            "bInstanceIsComplete",
            "bResourceCompilationFinished",
            "bShaderMapPresent",
            "bShaderMapComplete",
            "bValidGameThreadShaderMap",
            "InstanceResource->GetCompileErrors()",
            "resourcePresent=%s",
            "requiresMicOwnedStaticResource=%s",
            "ownedByInstance=%s",
            "instanceIsCompiling=%s",
            "instanceIsComplete=%s",
            "resourceCompilationFinished=%s",
            "shaderMapPresent=%s",
            "shaderMapComplete=%s",
            "validGameThreadShaderMap=%s",
            "compileErrors=[%s]",
        ):
            self.assertIn(required, validate_body)
        self.assertNotIn(
            "did not produce a valid SM5 static permutation",
            validate_body,
        )

    def test_16_odsc_unfinalized_complete_shader_map_is_not_a_false_failure(self) -> None:
        validate_start = self.cpp.index("bool ValidateHeroMaterialInstance(")
        validate_end = self.cpp.index(
            "bool ComputeHeroMeshPayloadDigest(",
            validate_start,
        )
        validate_body = self.cpp[validate_start:validate_end]
        failure_start = validate_body.index("if (!bResourcePresent ||")
        failure_end = validate_body.index("\n    {", failure_start)
        failure_gate = validate_body[failure_start:failure_end]

        for required_gate in (
            "!bResourcePresent",
            "bRequiresMicOwnedStaticResource && !bOwnedByInstance",
            "bInstanceIsCompiling",
            "!bInstanceIsComplete",
            "!bResourceCompilationFinished",
            "!bShaderMapPresent",
            "!bShaderMapComplete",
            "CompileErrors.Num() != 0",
        ):
            self.assertIn(required_gate, failure_gate)
        self.assertNotIn("bValidGameThreadShaderMap", failure_gate)
        self.assertIn(
            "InstanceResource->HasValidGameThreadShaderMap()",
            validate_body,
        )
        self.assertIn("validGameThreadShaderMap=%s", validate_body)
        for engine_evidence_marker in (
            "bCompilationFinalized",
            "ODSC/material-map-DDC-disabled",
            "IsRenderingThreadShaderMapComplete",
            "do not reject that supported",
        ):
            self.assertIn(engine_evidence_marker, validate_body)

    def test_17_import_wrapper_exact_path_set_is_strict_mode_null_safe(self) -> None:
        import_script = self.scripts["import"]
        self.assertIn(
            "$differences = @(Compare-Object -ReferenceObject $expectedSorted "
            "-DifferenceObject $actualSorted)",
            import_script,
        )
        self.assertIn("$differences.Count -ne 0", import_script)
        self.assertNotIn(
            "(Compare-Object -ReferenceObject $expectedSorted "
            "-DifferenceObject $actualSorted).Count",
            import_script,
        )

        powershell = shutil.which("powershell") or shutil.which("pwsh")
        if not powershell:
            self.skipTest("PowerShell is unavailable on this host.")
        probe = (
            "Set-StrictMode -Version Latest; $ErrorActionPreference='Stop'; "
            "$expectedSorted=@('Building\\hero.uasset'); "
            "$actualSorted=@('Building\\hero.uasset'); "
            "$differences=@(Compare-Object -ReferenceObject $expectedSorted "
            "-DifferenceObject $actualSorted); "
            "if($actualSorted.Count -ne $expectedSorted.Count -or "
            "$differences.Count -ne 0){throw 'unexpected difference'}; "
            "if($differences.Count -ne 0){exit 2}"
        )
        completed = subprocess.run(
            [powershell, "-NoProfile", "-NonInteractive", "-Command", probe],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(
            0,
            completed.returncode,
            f"{completed.stdout}\n{completed.stderr}",
        )

    def test_18_camera_digest_unconditionally_exports_zero_default_properties(self) -> None:
        capture_start = self.cpp.index(
            "bool CaptureEditableCameraComponentDigest("
        )
        capture_end = self.cpp.index(
            "bool CapturePreservedComponent(", capture_start
        )
        capture_body = self.cpp[capture_start:capture_end]

        self.assertIn("CPF_Edit", capture_body)
        self.assertIn("Property->ExportTextItem_Direct(", capture_body)
        self.assertIn(
            "Property->ContainerPtrToValuePtr<void>(Camera, Index)",
            capture_body,
        )
        self.assertIn("FScriptArrayHelper ArrayHelper(", capture_body)
        self.assertIn("dynamic-array:hard-object-references", capture_body)
        self.assertIn("dynamic-array-count:%d", capture_body)
        self.assertIn(
            "bHardObjectReferenceCollection && DynamicArrayNum > 0",
            capture_body,
        )
        self.assertIn(
            "deep referenced-object state is not supported by the exact migration invariant",
            capture_body,
        )
        self.assertIn("DynamicArrayNum > 0 && Value.IsEmpty()", capture_body)
        self.assertIn(
            "Could not canonicalize non-empty camera array property",
            capture_body,
        )
        self.assertIn("AppendGraphToken(Canonical, Value)", capture_body)
        self.assertIn("ActorComponent::AssetUserData", capture_body)
        self.assertIn(
            'ExportText_InContainer\'s bool means "different from Delta"',
            capture_body,
        )
        self.assertNotIn("if (!Property->ExportText_InContainer(", capture_body)
        self.assertNotIn("Could not serialize camera property", capture_body)


if __name__ == "__main__":
    unittest.main()
