from __future__ import annotations

import hashlib
import json
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal" / "Plugins" / "TRIADSensorFusion"
SOURCE = REPO / "unreal" / "SourceAssets" / "IstanaPublicViewExploreV5C" / "Portico"
RUNTIME_H = PLUGIN / "Source/TRIADSensorFusion/Public/TRIADIstanaExploreV4LandscapeActor.h"
RUNTIME_CPP = PLUGIN / "Source/TRIADSensorFusion/Private/TRIADIstanaExploreV4LandscapeActor.cpp"
APPEARANCE_CPP = PLUGIN / "Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5AppearanceActor.cpp"
FACTORY_H = PLUGIN / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5CPorticoAssetFactory.h"
FACTORY_CPP = PLUGIN / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5CPorticoAssetFactory.cpp"
EDITOR_BUILD = PLUGIN / "Source/TRIADSensorFusionEditor/TRIADSensorFusionEditor.Build.cs"
EDITOR_H = PLUGIN / "Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5CPorticoEditorLibrary.h"
EDITOR_CPP = PLUGIN / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5CPorticoEditorLibrary.cpp"
OBJ = SOURCE / "Generated/SM_IPV5C_CentralPorticoFidelityOverlay.obj"
MTL = SOURCE / "Generated/SM_IPV5C_CentralPorticoFidelityOverlay.mtl"
MANIFEST = SOURCE / "Generated/IstanaPublicViewV5CPorticoFidelityOverlay.manifest.json"
V8_OBJ = REPO / "unreal/SourceAssets/IstanaPublicViewV8Portico/Generated/SM_IstanaPublicViewV8_CentralPorticoDepthOverlay_V5Live.obj"
V5B_OBJ = REPO / "unreal/SourceAssets/IstanaPublicViewExploreV5B/Portico/Generated/SM_IPV5B_BuildingHero_CrossTrimmed.obj"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"unterminated function: {signature}")


class IstanaExploreV5CPorticoIntegrationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.runtime_h = read(RUNTIME_H)
        cls.runtime_cpp = read(RUNTIME_CPP)
        cls.appearance_cpp = read(APPEARANCE_CPP)
        cls.factory_h = read(FACTORY_H)
        cls.factory_cpp = read(FACTORY_CPP)
        cls.editor_build = read(EDITOR_BUILD)
        cls.editor_h = read(EDITOR_H)
        cls.editor_cpp = read(EDITOR_CPP)
        cls.manifest = json.loads(read(MANIFEST))

    def test_exact_source_identities_and_frozen_anchors(self) -> None:
        expected = {
            OBJ: (2913389, "883EAC65449A54D284D1E0E15B42F134F73C509A5ACE95BFD6DDD1DD326A3EC5"),
            MTL: (1432, "DC38D80587AEDFD31143A755388D4248A7C6E2029C4C9FD92794DDE06F2E339C"),
            MANIFEST: (8252, "380A2DE1204029C26FA06A7BBFD99105AA9D7979625575B28FF3E45A4B0337A1"),
            V8_OBJ: (1383806, "330B20E8F58289C84EF96CB031D374ADB3AAF52711E5247D1F167C7E94DC4526"),
            V5B_OBJ: (330419440, "20387D2F55F5F87B2C285AC3AAD3C8D8364CF909E68E408B5DDBCB8CA43B08AD"),
        }
        combined = self.runtime_h + self.runtime_cpp + self.factory_cpp
        for path, (size, digest) in expected.items():
            with self.subTest(path=path.name):
                self.assertEqual(path.stat().st_size, size)
                self.assertEqual(sha256(path), digest)
                if path in (OBJ, MTL, MANIFEST):
                    self.assertIn(digest, combined)
        self.assertEqual(
            self.manifest["semanticSha256"].upper(),
            "84E99970FEAF2F9ACCE9D7BE07C320C4A7E8AAA342F5C30463F0ADE311958567",
        )
        self.assertIn(self.manifest["semanticSha256"].upper(), combined)

    def test_factory_isolated_exact_nine_asset_namespace(self) -> None:
        for marker in (
            "/Game/TRIAD/IstanaPublicViewExploreV5C/Portico",
            "SM_IPV5C_CentralPorticoFidelityOverlay",
            "ExpectedAssetCount = 9",
            "ExpectedTriangleCount = 13772",
            "ExpectedMaterialCount = 8",
            "Actual != Expected",
            "FreshAssets.Num() != 9",
            "SaveLoadedAssets(FreshAssets, false)",
        ):
            self.assertIn(marker, self.factory_cpp + self.editor_cpp)
        self.assertNotIn("IstanaPublicViewExploreV5BAssetFactory", self.factory_cpp)

    def test_import_is_exact_hash_gated_and_render_only(self) -> None:
        validate_hash = function_body(self.factory_cpp, "bool ValidateSourceHash(")
        for marker in (
            "FFileHelper::LoadFileToArray",
            "Bytes.Num() != ExpectedBytes",
            "SHA256_DIGEST_LENGTH",
            "BytesToHex(Digest, SHA256_DIGEST_LENGTH)",
            "ActualSha256 != ExpectedSha256",
        ):
            self.assertIn(marker, validate_hash)
        self.assertNotIn("FPlatformMisc::GetSHA256Signature", validate_hash)
        self.assertIn('"SSL"', self.editor_build)
        self.assertIn(
            'AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL")',
            self.editor_build,
        )
        task = function_body(self.factory_cpp, "UAssetImportTask* MakeImportTask()")
        for marker in (
            "bImportMaterials = false",
            "bImportTextures = false",
            "bConvertScene = false",
            "bConvertSceneUnit = false",
            "ImportUniformScale = 1.0f",
            "bCombineMeshes = true",
            "bReorderMaterialToFbxOrder = true",
            "bTransformVertexToAbsolute = true",
            "bAutoGenerateCollision = false",
            "bGenerateLightmapUVs = true",
            "FBXNIM_ImportNormals",
            "bBuildNanite = false",
            "bReplaceExisting = false",
            "bSave = false",
        ):
            self.assertIn(marker, task)
        mesh = function_body(self.factory_cpp, "bool ValidateMesh(")
        for marker in (
            "GetNumSourceModels() != 1",
            "BuildScale3D != FVector::OneVector",
            "TriangleCount(Mesh) != ExpectedTriangleCount",
            "AggGeom.GetElementCount() != 0",
            "CTF_UseComplexAsSimple",
            "FVector(-1350.0, 1250.0, 64.0)",
            "FVector(1350.0, 2592.0, 2118.0)",
        ):
            self.assertIn(marker, mesh)

    def test_material_order_values_and_procedural_no_texture_claim_are_exact(self) -> None:
        ordered = (
            ("StoneWarm", "0.4452f, 0.4564f, 0.4452f", "0.66f", "0.016f", "0.028f", "0.35f", "false", 2616),
            ("TrimIvory", "0.7157f, 0.7157f, 0.6724f", "0.54f", "0.010f", "0.020f", "0.35f", "false", 5504),
            ("RenderWarm", "0.5029f, 0.5029f, 0.4793f", "0.72f", "0.024f", "0.050f", "0.30f", "false", 180),
            ("RecessWarmShadow", "0.0160f, 0.0144f, 0.0116f", "0.84f", "0.004f", "0.012f", "0.15f", "false", 240),
            ("Soffit", "0.2831f, 0.2747f, 0.2462f", "0.72f", "0.012f", "0.020f", "0.25f", "false", 1632),
            ("DoorTimber", "0.0648f, 0.0319f, 0.0160f", "0.56f", "0.014f", "0.024f", "0.40f", "false", 432),
            ("FanlightGlass", "0.0296f, 0.0343f, 0.0369f", "0.14f", "0.003f", "0.018f", "0.55f", "true", 288),
            ("LouvreWarmIvory", "0.3916f, 0.3712f, 0.3278f", "0.58f", "0.008f", "0.018f", "0.28f", "false", 2880),
        )
        positions = []
        for (
            name,
            color,
            roughness,
            albedo_breakup,
            roughness_breakup,
            specular,
            glazing,
            triangles,
        ) in ordered:
            marker = f'M_IPV5C_Portico_{name}"'
            positions.append(self.factory_cpp.index(marker))
            expected_row = (
                f'{{TEXT("M_IPV5C_Portico_{name}"), FLinearColor({color}), '
                f"{roughness}, {albedo_breakup}, {roughness_breakup}, 0.0f, "
                f"{specular}, {glazing}, {triangles}}}"
            )
            self.assertIn(expected_row, self.factory_cpp)
        self.assertEqual(positions, sorted(positions))
        expected_names = [f"M_IPV5C_Portico_{row[0]}" for row in ordered]
        self.assertEqual(
            self.manifest["mesh"]["materialUseOrder"], expected_names
        )
        self.assertIn(
            "ExpectedImportedToCanonical[] = {5, 6, 1, 0, 7, 3, 2, 4}",
            self.factory_cpp,
        )
        normalize = function_body(
            self.factory_cpp, "bool NormalizeAndBindMaterials("
        )
        for marker in (
            "Imported.MaterialSlotName != Imported.ImportedMaterialSlotName",
            "*CanonicalIndex != ExpectedImportedToCanonical[ImportedIndex]",
            "RenderSection.NumTriangles !=",
            "GetSectionInfoMap().Set",
            "GetOriginalSectionInfoMap().Set",
            "Mesh->SetStaticMaterials(OrderedMaterials)",
        ):
            self.assertIn(marker, normalize)
        create = function_body(self.factory_cpp, "UMaterial* CreateMaterial(")
        validate = function_body(self.factory_cpp, "bool ValidateMaterial(")
        response = function_body(
            self.factory_cpp, "FString MakeSurfaceResponseCode("
        )
        for marker in (
            "BLEND_Opaque",
            "MSM_DefaultLit",
            "WPT_ExcludeAllShaderOffsets",
            "CMOT_Float4",
            'InputName = TEXT("WorldPosition")',
            'InputName = TEXT("Fresnel")',
            "GlassFresnel->Exponent = 5.0f",
            "GlassFresnel->BaseReflectFraction = 0.04f",
            "Data->BaseColor.Connect",
            "Data->Roughness.Connect",
            "Data->Metallic.Connect",
            "Data->Specular.Connect",
        ):
            self.assertIn(marker, create)
        for marker in (
            "ExpectedExpressionCount = Spec.bGlazing ? 7 : 6",
            "WorldPositionCount != 1",
            "CustomCount != 1",
            "ComponentMaskCount != 2",
            "ConstantCount != 2",
            "FresnelCount != ExpectedFresnelCount",
            "SurfaceResponse->Code != MakeSurfaceResponseCode(Spec)",
            "SurfaceResponse->OutputType != CMOT_Float4",
            "!SurfaceResponse->AdditionalOutputs.IsEmpty()",
            "!SurfaceResponse->AdditionalDefines.IsEmpty()",
            "!SurfaceResponse->IncludeFilePaths.IsEmpty()",
            "GlassFresnel->ExponentIn.Expression",
            "GlassFresnel->BaseReflectFractionIn.Expression",
            "GlassFresnel->Normal.Expression",
            "Data->Opacity.Expression",
            "Data->Refraction.Expression",
            "Data->WorldPositionOffset.Expression",
        ):
            self.assertIn(marker, validate)
        for marker in (
            "float3 p = WorldPosition * 0.01",
            "0.72 * macro + 0.28 * detail",
            "Spec.AlbedoBreakup",
            "Spec.RoughnessBreakup",
            "saturate((Fresnel - 0.04) / 0.96)",
            "edge * 0.45",
            "max(0.10, roughness - edge * 0.04)",
        ):
            self.assertIn(marker, response)
        self.assertIn(
            "materials=proceduralNoTexturePBR opaqueGlass=true",
            self.factory_cpp,
        )
        compiled = function_body(
            self.factory_cpp, "bool ValidateCompiledMaterial("
        )
        for marker in (
            "EnsureIsComplete",
            "GetMaterialResource(ERHIFeatureLevel::SM5)",
            "FinishCompilation",
            "GetCompileErrors",
            "IsCompilingOrHadCompileError",
            "IsCompilationFinished",
            "GetGameThreadShaderMap",
            "IsGameThreadShaderMapComplete",
        ):
            self.assertIn(marker, compiled)
        validate_internal = function_body(
            self.factory_cpp, "bool ValidateInternal("
        )
        self.assertIn(
            "!ValidateCompiledMaterial(Material, Spec, OutReport)",
            validate_internal,
        )
        self.assertIn("compiledSM5=true", validate_internal)
        self.assertNotIn("BLEND_Translucent", self.factory_cpp)
        self.assertNotIn("Data->WorldPositionOffset.Connect", self.factory_cpp)
        self.assertNotIn("MaterialExpressionTexture", self.factory_cpp)
        self.assertIn("bPorticoV5CUsesTextureMaps = false", self.runtime_h)

    def test_fail_safe_switch_never_hides_v8_before_mesh_validation(self) -> None:
        configure = function_body(
            self.runtime_cpp,
            "bool ATRIADIstanaExploreV4LandscapeActor::ConfigurePorticoV5CPresentation(",
        )
        restore_at = configure.index("RestorePorticoV8PresentationFailSafe();")
        validate_at = configure.index("ValidateExactPorticoV5CMesh")
        hide_at = configure.index("PorticoV8RenderOnlyComponent->SetVisibility(false")
        self.assertLess(restore_at, validate_at)
        self.assertLess(validate_at, hide_at)
        self.assertGreaterEqual(
            configure.count("RestorePorticoV8PresentationFailSafe();"), 2
        )
        restore = function_body(
            self.runtime_cpp,
            "void ATRIADIstanaExploreV4LandscapeActor::\n    RestorePorticoV8PresentationFailSafe()",
        )
        self.assertIn("SetVisibility(true, true)", restore)
        self.assertIn("SetStaticMesh(nullptr)", restore)
        self.assertIn("SetVisibility(false, true)", restore)

    def test_exact_two_state_runtime_and_v5_contact_shadow_contract(self) -> None:
        validate = function_body(
            self.runtime_cpp,
            "bool ATRIADIstanaExploreV4LandscapeActor::ValidatePorticoV5CPresentation(",
        )
        for marker in (
            "if (!bPorticoV5CPresentationActive)",
            "failSafeV8Visible=true",
            "Active V5C state requires exact visible V5C and hidden V8",
            "CanEverAffectNavigation()",
            "IgnoresAllChannels(Component)",
        ):
            self.assertIn(marker, validate)
        self.assertIn("ContactShadowEnabledComponentCount = 17", self.appearance_cpp)
        self.assertIn("ContactShadowDisabledComponentCount = 11", self.appearance_cpp)
        self.assertIn("V5CCentralPorticoRenderOnlySuccessor", self.appearance_cpp)
        self.assertIn("bHiddenByExactV5CPorticoPresentation", self.appearance_cpp)

    def test_editor_apply_is_explicit_and_recoverable(self) -> None:
        apply = function_body(
            self.editor_cpp,
            "ApplyIstanaExploreV5CPorticoToCurrentWorld(FString& OutMessage)",
        )
        for marker in (
            "ValidateAssets(AssetReport)",
            "ResolveCurrentV4Owner",
            "ConfigurePorticoV5CPresentation",
            "ValidatePorticoV5CPresentation",
            "RestorePorticoV8PresentationFailSafe",
            "MarkPackageDirty",
        ):
            self.assertIn(marker, apply)
        self.assertNotIn("SaveMap", self.editor_cpp)
        self.assertNotIn("SaveCurrentLevel", self.editor_cpp)
        self.assertIn("RestoreIstanaExploreV8PorticoInCurrentWorld", self.editor_h)


if __name__ == "__main__":
    unittest.main()
