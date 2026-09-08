from __future__ import annotations

import hashlib
import json
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal/Plugins/TRIADSensorFusion"
SOURCE = REPO / "unreal/SourceAssets/IstanaPublicViewExploreV5C/GroundContext"
GENERATED = SOURCE / "Generated"
CONTRACT = SOURCE / "istana_public_view_v5c_ground_context.contract.json"
OBJ = GENERATED / "SM_IPV5C_OfficialPlanningGroundContext.obj"
MTL = GENERATED / "SM_IPV5C_OfficialPlanningGroundContext.mtl"
FEATURES = GENERATED / "IstanaPublicViewV5CGroundContext.features.json"
MANIFEST = GENERATED / "IstanaPublicViewV5CGroundContext.manifest.json"
FACTORY_H = PLUGIN / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5CGroundContextAssetFactory.h"
FACTORY_CPP = PLUGIN / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5CGroundContextAssetFactory.cpp"
EDITOR_H = PLUGIN / "Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5CGroundContextEditorLibrary.h"
EDITOR_CPP = PLUGIN / "Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5CGroundContextEditorLibrary.cpp"


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


class IstanaExploreV5CGroundContextAssetContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.factory_h = read(FACTORY_H)
        cls.factory_cpp = read(FACTORY_CPP)
        cls.editor_h = read(EDITOR_H)
        cls.editor_cpp = read(EDITOR_CPP)
        cls.contract = json.loads(read(CONTRACT))

    def test_factory_pins_exact_source_controlled_artifacts(self) -> None:
        expected = {
            OBJ: (11_861_751, "49334DD702E12E82EEAD1C70C4C8BB5984D6F2C375262CF368102E363677D12C"),
            MTL: (473, "9F1ED025A49BB0DBBF28BDAE9E12010B4600842BF847FDC1552D1F1351BF467B"),
            FEATURES: (181_449, "D79B7D7B602AEA593DE65E8C5368CFC04967AC02C024ABBF375D95724C1153A9"),
            MANIFEST: (13_093, "FC1F00F91B6CCAD820A3D3B2D354AAD691E08FE4F9BD70F7AA28E45EFBAF501F"),
            CONTRACT: (8_514, "7E642EC331F013CECB5251012592CA5509814B9F71E2CD989134719B8981E06E"),
        }
        for path, (size, digest) in expected.items():
            with self.subTest(path=path.name):
                self.assertEqual(path.stat().st_size, size)
                self.assertEqual(sha256(path), digest)
                self.assertIn(digest, self.factory_cpp)
        self.assertIn("ValidateAllSourceHashes", self.factory_cpp)

    def test_factory_owns_exact_three_asset_namespace_and_public_api(self) -> None:
        combined = self.factory_h + self.factory_cpp + self.editor_h + self.editor_cpp
        for marker in (
            "/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext",
            "ExpectedAssetCount = 3",
            "ExpectedSourceTriangleCount = 30408",
            "ExpectedImportedTriangleCount = 30403",
            "ExpectedMaterialCount = 2",
            "FreshAssets.Num() != 3",
            "SaveLoadedAssets(FreshAssets, false)",
            "GetGroundContextMeshObjectPath",
            "GetOrderedMaterialObjectPaths",
            "GetExpectedImportedTriangleCount",
            "ImportIstanaExploreV5CGroundContextAssets",
            "ValidateIstanaExploreV5CGroundContextAssets",
        ):
            self.assertIn(marker, combined)
        self.assertNotIn("ApplyIstanaExplore", self.editor_h)
        self.assertNotIn("CurrentWorld", self.editor_h)

    def test_import_policy_is_identity_render_only_and_texture_free(self) -> None:
        task = function_body(self.factory_cpp, "UAssetImportTask* MakeImportTask()")
        for marker in (
            "bImportMaterials = false",
            "bImportTextures = false",
            "bConvertScene = false",
            "bConvertSceneUnit = false",
            "ImportUniformScale = 1.0f",
            "bCombineMeshes = true",
            "bReorderMaterialToFbxOrder = true",
            "bImportMeshLODs = false",
            "bTransformVertexToAbsolute = true",
            "bAutoGenerateCollision = false",
            "bGenerateLightmapUVs = true",
            "FBXNIM_ImportNormals",
            "bBuildNanite = true",
            "bRemoveDegenerates = false",
            "bReplaceExisting = false",
            "bSave = false",
        ):
            self.assertIn(marker, task)
        mesh = function_body(self.factory_cpp, "bool ValidateMesh(")
        for marker in (
            "BuildScale3D !=",
            "FVector::OneVector",
            "bUseFullPrecisionUVs",
            "ExpectedImportedTriangleCount",
            "LODResources.Num() != 1",
            "Sections.Num() != ExpectedMaterialCount",
            "AggGeom.GetElementCount() != 0",
            "CTF_UseComplexAsSimple",
            "FVector(-99838.5847, -99957.6768, -128.003865)",
            "FVector(99892.9611, 99966.3390, 191.882529)",
            "ValidateUv0",
            "HasValidNaniteData()",
        ):
            self.assertIn(marker, mesh)
        for marker in (
            "KeepPercentTriangles = 1.0f",
            "TrimRelativeError = 0.0f",
            "FallbackTarget =\n        ENaniteFallbackTarget::PercentTriangles",
            "FallbackPercentTriangles = 1.0f",
            "FallbackRelativeError = 0.0f",
            "RemoveSimpleCollision",
        ):
            self.assertIn(marker, self.factory_cpp)

    def test_two_exact_child_instances_reuse_parent_with_only_listed_overrides(self) -> None:
        parent = "/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/MI_IPV5_HardscapeStone.MI_IPV5_HardscapeStone"
        self.assertEqual(
            self.contract["unrealAssets"]["parentMaterialObjectPath"], parent
        )
        self.assertIn(
            "/Game/TRIAD/IstanaPublicViewExploreV5/Appearance/Materials/",
            self.factory_cpp,
        )
        self.assertIn(
            "MI_IPV5_HardscapeStone.MI_IPV5_HardscapeStone", self.factory_cpp
        )
        ordered = (
            ("MI_IPV5C_OfficialPlanningRoadZone", 23_338),
            ("MI_IPV5C_OfficialPlanningRoadGraphic", 7_070),
        )
        positions = []
        for name, triangles in ordered:
            positions.append(self.factory_cpp.index(f'TEXT("{name}")'))
            self.assertIn(str(triangles), self.factory_cpp)
        self.assertEqual(positions, sorted(positions))
        validate = function_body(self.factory_cpp, "bool ValidateMaterialInstance(")
        for marker in (
            "TextureParameterValues.IsEmpty()",
            "StaticSwitchParameters.IsEmpty()",
            "BaseOverrides.IsEmpty()",
            "SameNames(ExpectedScalarNames, ActualScalarNames)",
            "SameNames(ExpectedVectorNames, ActualVectorNames)",
            "NaniteOverrideMaterial.GetOverrideMaterial()",
        ):
            self.assertIn(marker, validate)
        for parameter in (
            "LookdevTint",
            "TileMeters",
            "DetailTileMeters",
            "MacroTileMeters",
            "NormalStrength",
            "DetailNormalStrength",
            "RoughnessBias",
            "MacroAlbedoStrength",
            "MacroRoughnessStrength",
            "BumpOffsetStrength",
            "ExposedMetalMaskStrength",
        ):
            self.assertIn(parameter, self.factory_cpp)
        parent_validation = function_body(self.factory_cpp, "bool ValidateParentMaterial(")
        for marker in (
            "BlendMode != BLEND_Opaque",
            "Base->TwoSided",
            "EditorOnly->WorldPositionOffset.Expression",
            "EditorOnly->Displacement.Expression",
            "MaxWorldPositionOffsetDisplacement",
            "WeatheringStrength",
            "GroundContactDampStrength",
        ):
            self.assertIn(marker, parent_validation)

    def test_material_slot_permutation_is_normalized_by_name_and_triangle_census(self) -> None:
        normalize = function_body(self.factory_cpp, "bool NormalizeAndBindMaterials(")
        for marker in (
            "CanonicalOrder.Find(Imported.MaterialSlotName)",
            "Imported.MaterialSlotName != Imported.ImportedMaterialSlotName",
            "ImportedToCanonical[ImportedIndex]",
            "RenderSection.NumTriangles !=",
            "GetSectionInfoMap().Set",
            "GetOriginalSectionInfoMap().Set",
            "SetStaticMaterials(OrderedMaterials)",
        ):
            self.assertIn(marker, normalize)
        self.assertIn("23338", self.factory_cpp)
        self.assertIn("23336", self.factory_cpp)
        self.assertIn("7070", self.factory_cpp)
        self.assertIn("7067", self.factory_cpp)
        self.assertIn("int32 SourceTriangles;", self.factory_cpp)
        self.assertIn("int32 ImportedTriangles;", self.factory_cpp)
        self.assertIn("MaterialSpecs[CanonicalIndex].ImportedTriangles", normalize)
        self.assertIn("importerCanonicalizedNarrowFaces=5", self.factory_cpp)

    def test_editor_import_is_idempotent_and_saves_only_fresh_three(self) -> None:
        import_body = function_body(
            self.editor_cpp,
            "ImportIstanaExploreV5CGroundContextAssets(FString& OutMessage)",
        )
        validate_at = import_body.index("ValidateAssets")
        create_at = import_body.index("CreateFreshAssets")
        save_at = import_body.index("SaveLoadedAssets(FreshAssets, false)")
        cold_validate_at = import_body.rindex("ValidateAssets")
        self.assertLess(validate_at, create_at)
        self.assertLess(create_at, save_at)
        self.assertLess(save_at, cold_validate_at)
        self.assertIn("FreshAssets.Num() != 3", import_body)
        self.assertIn("FreshAssets.Contains(nullptr)", import_body)
        for denial in (
            "not a physical road width",
            "no textures",
            "collision",
            "navigation",
            "survey",
            "elevation/Z",
            "sensor",
            "RF authority",
        ):
            self.assertIn(denial, self.editor_cpp)


if __name__ == "__main__":
    unittest.main()
