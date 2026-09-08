from __future__ import annotations

import hashlib
import json
import re
import unittest
from collections import Counter
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal/Plugins/TRIADSensorFusion"
PACKAGE = REPO / "unreal/SourceAssets/IstanaPublicViewExploreV5D/PublicRealm"
GENERATED = PACKAGE / "Generated"
CONTRACT = PACKAGE / "istana_public_view_v5d_public_realm.contract.json"
GENERATOR = PACKAGE / "build_public_realm_v5d.py"
CORE_OBJ = GENERATED / "SM_IPV5D_PublicRealm_Core_Render.obj"
FALLBACK_OBJ = GENERATED / "SM_IPV5D_PublicRealm_Fallback_Render.obj"
MTL = GENERATED / "SM_IPV5D_PublicRealm_Render.mtl"
FEATURES = GENERATED / "IstanaPublicViewV5DPublicRealm.features.json"
MANIFEST = GENERATED / "IstanaPublicViewV5DPublicRealm.manifest.json"
LOCK = GENERATED / "IstanaPublicViewV5DPublicRealm.acceptance.lock.json"
FACTORY_H = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DPublicRealmAssetFactory.h"
)
FACTORY_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DPublicRealmAssetFactory.cpp"
)
EDITOR_H = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Public"
    / "TRIADIstanaExploreV5DPublicRealmEditorLibrary.h"
)
EDITOR_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DPublicRealmEditorLibrary.cpp"
)
ACTOR_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusion/Private"
    / "TRIADIstanaExploreV5DPublicRealmActor.cpp"
)

ORDERED_MATERIALS = (
    "MI_IPV5C_OfficialPlanningRoadZone",
    "MI_IPV5C_OfficialPlanningRoadGraphic",
    "MI_IPV5D_PublicRealmConcrete",
)
LEGACY_ROAD_BASE_PATH = (
    "/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/Materials/"
    "MI_IPV5C_OfficialPlanningRoadZone.MI_IPV5C_OfficialPlanningRoadZone"
)
LEGACY_ROAD_GRAPHIC_PATH = (
    "/Game/TRIAD/IstanaPublicViewExploreV5C/GroundContext/Materials/"
    "MI_IPV5C_OfficialPlanningRoadGraphic."
    "MI_IPV5C_OfficialPlanningRoadGraphic"
)
ASPHALT_ROOT = (
    "/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealmVisualR2/Materials"
)
ASPHALT_NAME = "M_IPV5D_PublicRealm_AsphaltDry_R2"
ASPHALT_PATH = f"{ASPHALT_ROOT}/{ASPHALT_NAME}.{ASPHALT_NAME}"
ROAD_GRAPHIC_SUPPRESSION_NAME = (
    "M_IPV5D_PublicRealm_RoadGraphicFullyClipped_R2"
)
ROAD_GRAPHIC_SUPPRESSION_PATH = (
    f"{ASPHALT_ROOT}/{ROAD_GRAPHIC_SUPPRESSION_NAME}."
    f"{ROAD_GRAPHIC_SUPPRESSION_NAME}"
)
ASPHALT_TEXTURE_PATHS = {
    "const FString AsphaltBaseColorTextureObjectPath(": (
        "/Game/Scene_RoadsideConstruction/Assets/MS/Surfaces/"
        "Ground_Asphalt_Fresh_01/T_Ground_Asphalt_Fresh_01_D."
        "T_Ground_Asphalt_Fresh_01_D"
    ),
    "const FString AsphaltNormalTextureObjectPath(": (
        "/Game/Scene_RoadsideConstruction/Assets/MS/Surfaces/"
        "Ground_Asphalt_Fresh_01/T_Ground_Asphalt_Fresh_01_N."
        "T_Ground_Asphalt_Fresh_01_N"
    ),
    "const FString AsphaltOrdpTextureObjectPath(": (
        "/Game/Scene_RoadsideConstruction/Assets/MS/Surfaces/"
        "Ground_Asphalt_Fresh_01/T_Ground_Asphalt_Fresh_01_ORDp."
        "T_Ground_Asphalt_Fresh_01_ORDp"
    ),
}
EXPECTED_GROUPS = {
    CORE_OBJ: {
        "V5D_PUBLIC_ROAD_BASE_VISUAL_ASSUMPTION": 444,
    },
    FALLBACK_OBJ: {
        "V5D_PUBLIC_ROAD_BASE_VISUAL_ASSUMPTION": 697,
        "V5D_OFFICIAL_ROAD_GRAPHIC_VISUAL_REFERENCE": 329,
        "V5D_EXPLICITLY_TAGGED_SIDEWALK_VISUAL_ASSUMPTION": 27,
        "V5D_EXPLICITLY_TAGGED_LOW_KERB_TOP_VISUAL_ASSUMPTION": 24,
        "V5D_PUBLIC_KERB_VISUAL_WALL": 24,
    },
}
EXPECTED_FILES = {
    CORE_OBJ: (
        168_139,
        "6418A023D64FA0A0F6C4CA14C79195BF96C02818B49C2AC03E4C81A61ECE9438",
    ),
    FALLBACK_OBJ: (
        420_002,
        "EFB1E7FE2371D5C522297698647DFC01C30240BE5A8A54945A7BCFDFA7C488F9",
    ),
    MTL: (
        707,
        "F03216B73AB47ACCBC7FC8BE2DADD1B6067AF2DA39F6A037CEDC107515FA18E8",
    ),
    FEATURES: (
        19_432,
        "DBBC471315517DBC0B6AB69C1F4169FCAA9CD475F708009580C1C25A4CF3155A",
    ),
    MANIFEST: (
        21_577,
        "FD5448DEA86724EB1AF611394829B72C543ABAA8D1DF6A4BD275A509F5B70018",
    ),
    LOCK: (
        7_607,
        "3EF1EA38422505920F920E06B3447D99135A56BC82144CE64946BC025EDBD4FF",
    ),
    CONTRACT: (
        17_548,
        "C5B4BFFF1FD90D5A0E7F056CE510CFC76BF3E7914C008F4D0CBCA4852B853C28",
    ),
    GENERATOR: (
        68_011,
        "E5DF1DADB4DE6DD96CBE3A74CCE815055373F16A18E99F9F1A0F77D27762B547",
    ),
}


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


def adjacent_cpp_string(source: str, declaration: str) -> str:
    start = source.index(declaration)
    end = source.index(");", start)
    return "".join(re.findall(r'"([^"\\]*)"', source[start:end]))


def obj_census(path: Path) -> tuple[int, int, int, Counter[str], Counter[str], list[str]]:
    vertices = 0
    normals = 0
    uvs = 0
    group = ""
    material = ""
    groups: Counter[str] = Counter()
    materials: Counter[str] = Counter()
    encounter_order: list[str] = []
    for line in read(path).splitlines():
        if line.startswith("v "):
            vertices += 1
        elif line.startswith("vn "):
            normals += 1
        elif line.startswith("vt "):
            uvs += 1
        elif line.startswith("g "):
            group = line[2:].strip()
        elif line.startswith("usemtl "):
            material = line[7:].strip()
            if material not in encounter_order:
                encounter_order.append(material)
        elif line.startswith("f "):
            groups[group] += 1
            materials[material] += 1
    return vertices, normals, uvs, groups, materials, encounter_order


class IstanaExploreV5DPublicRealmAssetFactoryContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.factory_h = read(FACTORY_H)
        cls.factory_cpp = read(FACTORY_CPP)
        cls.editor_h = read(EDITOR_H)
        cls.editor_cpp = read(EDITOR_CPP)
        cls.actor_cpp = read(ACTOR_CPP)
        cls.combined = (
            cls.factory_h
            + cls.factory_cpp
            + cls.editor_h
            + cls.editor_cpp
            + cls.actor_cpp
        )
        cls.contract = json.loads(read(CONTRACT))
        cls.manifest = json.loads(read(MANIFEST))
        cls.lock = json.loads(read(LOCK))

    def test_final_source_freeze_and_lock_are_pinned(self) -> None:
        for path, (size, digest) in EXPECTED_FILES.items():
            with self.subTest(path=path.name):
                self.assertEqual(path.stat().st_size, size)
                self.assertEqual(sha256(path), digest)
                self.assertIn(str(size), self.factory_cpp)
                self.assertIn(digest, self.factory_cpp)

        lock_rows = {row["path"]: row for row in self.lock["files"]}
        for path in (CONTRACT, GENERATOR, CORE_OBJ, FALLBACK_OBJ, MTL, FEATURES, MANIFEST):
            row = lock_rows[path.name]
            self.assertEqual(row["bytes"], path.stat().st_size)
            self.assertEqual(row["sha256"].upper(), sha256(path))
        self.assertEqual(
            self.lock["lockedSetSha256"].upper(),
            "C99E09A59E6E7BB41127815C8D73A74937E231B2946B7EB53EE9F8DD122F0287",
        )
        self.assertEqual(
            self.manifest["primaryOutputSetSha256"].upper(),
            "C0AC6D6D2141E764160BB0BCFB9E879603A26CB2953FDB5587C178081F909EA4",
        )
        self.assertEqual(self.manifest["contract"]["bytes"], CONTRACT.stat().st_size)
        self.assertEqual(
            self.manifest["contract"]["sha256"].upper(), sha256(CONTRACT)
        )

    def test_source_obj_semantics_match_the_topology_contract(self) -> None:
        expected_material_counts = {
            CORE_OBJ: {ORDERED_MATERIALS[0]: 444},
            FALLBACK_OBJ: {
                ORDERED_MATERIALS[0]: 697,
                ORDERED_MATERIALS[1]: 329,
                ORDERED_MATERIALS[2]: 75,
            },
        }
        expected_corners = {CORE_OBJ: 1_332, FALLBACK_OBJ: 3_303}
        for path in (CORE_OBJ, FALLBACK_OBJ):
            vertices, normals, uvs, groups, materials, order = obj_census(path)
            with self.subTest(path=path.name):
                self.assertEqual(vertices, expected_corners[path])
                self.assertEqual(normals, expected_corners[path])
                self.assertEqual(uvs, expected_corners[path])
                self.assertEqual(groups, Counter(EXPECTED_GROUPS[path]))
                self.assertEqual(materials, Counter(expected_material_counts[path]))
                expected_order = [
                    name for name in ORDERED_MATERIALS if name in materials
                ]
                self.assertEqual(order, expected_order)

        self.assertEqual(
            self.contract["materialPlan"]["orderedSlots"],
            list(ORDERED_MATERIALS),
        )
        topology = self.contract["expectedTopology"]
        self.assertEqual(topology["core"]["triangleCount"], 444)
        self.assertEqual(topology["fallback"]["triangleCount"], 1_101)
        self.assertEqual(topology["totalTriangleCount"], 1_545)
        self.assertEqual(topology["totalDuplicatedCornerCount"], 4_635)

    def test_import_policy_is_identity_and_legacy_y_preconditioned(self) -> None:
        task = function_body(self.factory_cpp, "UAssetImportTask* MakeImportTask(")
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
            "bGenerateLightmapUVs = false",
            "FBXNIM_ImportNormals",
            "MikkTSpace",
            "bBuildNanite = true",
            "bRemoveDegenerates = false",
            "bReplaceExisting = false",
            "bSave = false",
        ):
            self.assertIn(marker, task)
        render_only = function_body(self.factory_cpp, "void MakeRenderOnly(")
        for marker in (
            "BuildScale3D = FVector::OneVector",
            "bRecomputeNormals = false",
            "bRecomputeTangents = true",
            "bUseMikkTSpace = true",
            "RemoveSimpleCollision",
            "CTF_UseSimpleAsComplex",
            "MarkAsNotHavingNavigationData",
        ):
            self.assertIn(marker, render_only)
        self.assertNotIn("FStaticMeshOperations::ApplyTransform", self.factory_cpp)
        self.assertNotIn("FVector(1.0, -1.0, 1.0)", self.factory_cpp)
        self.assertNotIn("ImportUniformScale = 100.0f", self.factory_cpp)

    def test_exact_assets_material_bindings_and_topology_are_validated(self) -> None:
        for marker in (
            "/Game/TRIAD/IstanaPublicViewExploreV5D/PublicRealm",
            "SM_IPV5D_PublicRealm_Core_Render",
            "SM_IPV5D_PublicRealm_Fallback_Render",
            "MI_IPV5D_PublicRealmConcrete",
            "ExpectedAssetCount = 5",
            "ExpectedCoreTriangleCount = 444",
            "ExpectedFallbackTriangleCount = 1101",
            "ExpectedFallbackRoadBaseTriangles = 697",
            "ExpectedFallbackRoadGraphicTriangles = 329",
            "ExpectedFallbackSidewalkTriangles = 27",
            "ExpectedFallbackKerbTopTriangles = 24",
            "ExpectedFallbackKerbWallTriangles = 24",
            "ExpectedCoreVertexCount = 1332",
            "ExpectedFallbackVertexCount = 3303",
            "GetSectionInfoMap().Set",
            "GetOriginalSectionInfoMap().Set",
            "SetStaticMaterials(OrderedMaterials)",
            "ValidateMeshDescriptionCensus",
            "ValidateUv0",
            "AggGeom.GetElementCount() != 0",
            "Mesh->bHasNavigationData",
            "Mesh->GetNavCollision()",
        ):
            self.assertIn(marker, self.factory_cpp)

        slots = self.factory_cpp[
            self.factory_cpp.index("MaterialSlotNames[]") :
            self.factory_cpp.index(
                "static_assert(UE_ARRAY_COUNT(MaterialSlotNames)"
            )
        ]
        road_zone = slots.index('TEXT("MI_IPV5C_OfficialPlanningRoadZone")')
        road_graphic = slots.index(
            'TEXT("MI_IPV5C_OfficialPlanningRoadGraphic")'
        )
        concrete = slots.index('TEXT("MI_IPV5D_PublicRealmConcrete")')
        self.assertLess(road_zone, road_graphic)
        self.assertLess(road_graphic, concrete)
        fallback_sections = self.factory_cpp[
            self.factory_cpp.index("const FImportedSectionSpec FallbackSections[]") :
            self.factory_cpp.index("struct FMeshSpec")
        ]
        self.assertIn(
            "UE5.5's legacy OBJ importer emits render sections in reverse usemtl order",
            self.factory_cpp,
        )
        imported_concrete = fallback_sections.index("ConcreteMaterialIndex")
        imported_graphic = fallback_sections.index("RoadGraphicMaterialIndex")
        imported_road_base = fallback_sections.index("RoadBaseMaterialIndex")
        self.assertLess(imported_concrete, imported_graphic)
        self.assertLess(imported_graphic, imported_road_base)
        normalize = function_body(
            self.factory_cpp, "bool NormalizeAndBindMaterials("
        )
        for marker in (
            "CanonicalOrder.Find(Imported.MaterialSlotName)",
            "Imported.MaterialSlotName != Imported.ImportedMaterialSlotName",
            "ImportedToCanonical[ImportedIndex]",
            "RenderSection.NumTriangles != Expected.Triangles",
            "Materials[*CanonicalIndex]",
        ):
            self.assertIn(marker, normalize)

    def test_concrete_and_baked_v5c_mesh_bindings_remain_immutable(self) -> None:
        for marker in (
            "MI_IPV5_HardscapeStone.MI_IPV5_HardscapeStone",
            "CreateConcreteMaterial",
            "UMaterialInstanceConstantFactoryNew",
            "ConcreteLookdevTint(0.48f, 0.49f, 0.47f, 1.0f)",
            "TextureParameterValues.IsEmpty()",
            "StaticSwitchParameters.IsEmpty()",
            "SameNames(ExpectedScalarNames, ActualScalarNames)",
            "SameNames(ExpectedVectorNames, ActualVectorNames)",
            "ExpectedV5CPaths",
            "TRIADIstanaExploreV5CGroundContextAssetFactory::",
            "GetOrderedMaterialObjectPaths",
        ):
            self.assertIn(marker, self.factory_cpp)
        self.assertEqual(self.factory_cpp.count("CreateConcreteMaterial("), 2)
        self.assertNotIn("UTextureFactory", self.factory_cpp)

        self.assertEqual(
            adjacent_cpp_string(
                self.factory_cpp,
                "const FString LegacyRoadBaseMaterialObjectPath(",
            ),
            LEGACY_ROAD_BASE_PATH,
        )
        self.assertEqual(
            adjacent_cpp_string(
                self.factory_cpp,
                "const FString LegacyRoadGraphicMaterialObjectPath(",
            ),
            LEGACY_ROAD_GRAPHIC_PATH,
        )
        baked_paths = function_body(
            self.factory_cpp,
            "const TArray<FString>& OrderedSemanticMaterialPaths()",
        )
        road_base = baked_paths.index("LegacyRoadBaseMaterialObjectPath")
        road_graphic = baked_paths.index("LegacyRoadGraphicMaterialObjectPath")
        concrete = baked_paths.index("ConcreteMaterialObjectPath")
        self.assertLess(road_base, road_graphic)
        self.assertLess(road_graphic, concrete)
        self.assertNotIn("AsphaltMaterialObjectPath", baked_paths)
        self.assertNotIn(
            "RoadGraphicSuppressionMaterialObjectPath", baked_paths
        )

        validate_mesh = function_body(self.factory_cpp, "bool ValidateMesh(")
        for marker in (
            "StaticMaterial.MaterialInterface != Materials[Slot]",
            "OrderedSemanticMaterialPaths()[Slot]",
            "ordered semantic material binding drifted",
        ):
            self.assertIn(marker, validate_mesh)
        dependencies = function_body(
            self.factory_cpp, "bool ValidateLegacyMaterialDependencies("
        )
        self.assertRegex(
            dependencies,
            r"ExpectedRoadBaseMaterialObjectPath\(\) !=\s*"
            r"AsphaltMaterialObjectPath",
        )
        self.assertRegex(
            dependencies,
            r"ExpectedRoadGraphicMaterialObjectPath\(\) !=\s*"
            r"RoadGraphicSuppressionMaterialObjectPath",
        )
        self.assertNotRegex(
            dependencies,
            r"ExpectedRoadGraphicMaterialObjectPath\(\) !=\s*"
            r"AsphaltMaterialObjectPath",
        )
        validate_final = function_body(
            self.factory_cpp, "bool ValidateInternal("
        )
        self.assertIn(
            "ValidateLegacyInternal(bRequireSaved, LegacyReport)",
            validate_final,
        )
        self.assertIn("immutableMeshPackages=true", validate_final)
        self.assertIn("immutableV5CSourceBindings=true", validate_final)

    def test_v5d_owned_asphalt_has_exact_dependencies_and_graph(self) -> None:
        self.assertEqual(
            adjacent_cpp_string(
                self.factory_cpp, "const FString VisualMaterialRoot("
            ),
            ASPHALT_ROOT,
        )
        self.assertEqual(
            adjacent_cpp_string(
                self.factory_cpp, "const FString AsphaltMaterialName("
            ),
            ASPHALT_NAME,
        )
        self.assertIn(
            "AsphaltMaterialObjectPath(\n    ObjectPath(VisualMaterialRoot, AsphaltMaterialName))",
            self.factory_cpp,
        )
        self.assertEqual(
            adjacent_cpp_string(
                self.factory_cpp,
                "const FString RoadGraphicSuppressionMaterialName(",
            ),
            ROAD_GRAPHIC_SUPPRESSION_NAME,
        )
        self.assertIn(
            "RoadGraphicSuppressionMaterialObjectPath(\n"
            "    ObjectPath(VisualMaterialRoot, "
            "RoadGraphicSuppressionMaterialName))",
            self.factory_cpp,
        )
        for declaration, expected_path in ASPHALT_TEXTURE_PATHS.items():
            with self.subTest(texture=expected_path):
                self.assertEqual(
                    adjacent_cpp_string(self.factory_cpp, declaration),
                    expected_path,
                )

        dependencies = function_body(
            self.factory_cpp, "bool ValidateAsphaltTextureDependencies("
        )
        for marker in (
            "AsphaltBaseColorTexturePackageBytes = 23508700",
            "AsphaltNormalTexturePackageBytes = 18074043",
            "AsphaltOrdpTexturePackageBytes = 16481152",
            "8BB620174317FDF2AE97909CBF08E11E6C3AFCCEAE40B1C7D399A3D42CE05C1E",
            "8E166FB343C8E1F80E41675FDEC80FE00F3506A6D5E68EBD46C80AE579826280",
            "E66E2C80B3D1D8CDB8516131B646A496CC9F857D49ACA98BED966115C6AC4B7D",
        ):
            self.assertIn(marker, self.factory_cpp)
        for marker in (
            "LoadExact<UTexture2D>(AsphaltBaseColorTextureObjectPath)",
            "LoadExact<UTexture2D>(AsphaltNormalTextureObjectPath)",
            "LoadExact<UTexture2D>(AsphaltOrdpTextureObjectPath)",
            "FPackageName::ObjectPathToPackageName(*Paths[Index])",
            "FPackageName::LongPackageNameToFilename(",
            "FPackageName::GetAssetPackageExtension()",
            "ValidateSourceHash(",
            "PackageBytes[Index]",
            "*PackageSha256[Index]",
            "Texture->GetSizeX() != Texture->GetSizeY()",
            "Texture->GetSizeX() < 2048",
            "Texture->AddressX != TA_Wrap",
            "Texture->AddressY != TA_Wrap",
            "!OutTextures.BaseColor->SRGB",
            "OutTextures.BaseColor->CompressionSettings != TC_Default",
            "OutTextures.Normal->SRGB",
            "OutTextures.Normal->CompressionSettings != TC_Normalmap",
            "OutTextures.Ordp->SRGB",
            "OutTextures.Ordp->CompressionSettings != TC_Masks",
        ):
            self.assertIn(marker, dependencies)

        create = function_body(self.factory_cpp, "UMaterial* CreateAsphaltMaterial(")
        for marker in (
            "UMaterialFactoryNew",
            "VisualMaterialRoot",
            "Asphalt.UV0_Metres",
            "Asphalt.PrimaryUV",
            "Asphalt.SecondaryUnrotatedUV",
            "Asphalt.RotateSecondaryUV",
            "Asphalt.SecondaryStaticOffset",
            "Asphalt.SecondaryUV",
            "Asphalt.MacroUV",
            "Asphalt.BaseColorPrimary",
            "Asphalt.BaseColorSecondary",
            "Asphalt.NormalPrimary",
            "Asphalt.NormalSecondary",
            "Asphalt.ORDpPrimary",
            "Asphalt.ORDpSecondary",
            "TRIAD_IPV5D_ASPHALT_MACRO_VALUE_NOISE_V1",
            "TRIAD_IPV5D_ASPHALT_REORIENT_ROTATED_NORMAL_37_DEGREES_V1",
            "Could not allocate the exact 39-node",
            "RoughnessBlend->A.Connect(2, OrdpPrimary)",
            "RoughnessBlend->B.Connect(2, OrdpSecondary)",
            "AoBlend->A.Connect(1, OrdpPrimary)",
            "AoBlend->B.Connect(1, OrdpSecondary)",
            "Data->BaseColor.Connect(0, FinalBase)",
            "Data->Normal.Connect(0, FinalNormal)",
            "Data->Roughness.Connect(0, FinalRoughness)",
            "Data->Specular.Connect(0, Specular)",
            "Data->Metallic.Connect(0, Metallic)",
            "Data->AmbientOcclusion.Connect(0, AoBlend)",
            "Material->bUsedWithNanite = true",
        ):
            self.assertIn(marker, create)
        for prohibited_connection in (
            "Data->WorldPositionOffset.Connect",
            "Data->Displacement.Connect",
            "Data->PixelDepthOffset.Connect",
        ):
            self.assertNotIn(prohibited_connection, create)
        add_custom = function_body(
            self.factory_cpp, "UMaterialExpressionCustom* AddAsphaltCustom("
        )
        self.assertIn("Custom->Inputs.Reset(1)", add_custom)

        for marker in (
            "AsphaltPrimaryTileMeters = 2.0f",
            "AsphaltSecondaryTileMeters = 3.37f",
            "AsphaltMacroTileMeters = 37.0f",
            "FMath::DegreesToRadians(37.0f)",
            "AsphaltSecondaryOffsetU = 13.17f",
            "AsphaltSecondaryOffsetV = -7.43f",
            "AsphaltSecondaryBlend = 0.34f",
            "AsphaltNormalStrength = 0.42f",
            "AsphaltRoughnessBias = 0.04f",
            "AsphaltSpecular = 0.25f",
        ):
            self.assertIn(marker, self.factory_cpp)

        validate = function_body(
            self.factory_cpp, "bool ValidateAsphaltMaterial("
        )
        asphalt_nodes = (
            "Asphalt.UV0_Metres",
            "Asphalt.PrimaryTileMeters",
            "Asphalt.SecondaryTileMeters",
            "Asphalt.MacroTileMeters",
            "Asphalt.PrimaryUV",
            "Asphalt.SecondaryUnrotatedUV",
            "Asphalt.StaticRotation37Degrees",
            "Asphalt.RotateSecondaryUV",
            "Asphalt.SecondaryStaticOffset",
            "Asphalt.SecondaryUV",
            "Asphalt.MacroUV",
            "Asphalt.BaseColorPrimary",
            "Asphalt.BaseColorSecondary",
            "Asphalt.NormalPrimary",
            "Asphalt.NormalSecondary",
            "Asphalt.ORDpPrimary",
            "Asphalt.ORDpSecondary",
            "Asphalt.SecondaryBlend",
            "Asphalt.BaseColorAntiTileBlend",
            "Asphalt.AlbedoTint",
            "Asphalt.NeutralAlbedoTint",
            "Asphalt.ProceduralMacroField",
            "Asphalt.MacroTintLow",
            "Asphalt.MacroTintHigh",
            "Asphalt.LowAmplitudeMacroTint",
            "Asphalt.ReorientRotatedNormal37Degrees",
            "Asphalt.FinalBaseColor",
            "Asphalt.RoughnessAntiTileBlend",
            "Asphalt.RoughnessBias",
            "Asphalt.DryRoughnessBias",
            "Asphalt.FinalRoughness",
            "Asphalt.AOAntiTileBlend",
            "Asphalt.NormalAntiTileBlend",
            "Asphalt.FlatTangentNormal",
            "Asphalt.NormalStrength",
            "Asphalt.ControlledNormalStrength",
            "Asphalt.FinalNormalizedNormal",
            "Asphalt.DielectricSpecular",
            "Asphalt.NonMetal",
        )
        self.assertEqual(len(asphalt_nodes), 39)
        for node in asphalt_nodes:
            with self.subTest(asphalt_node=node):
                self.assertIn(f'TEXT("{node}")', validate)
        for marker in (
            "Material->GetPathName() != AsphaltMaterialObjectPath",
            "Nodes.Num() != 39",
            "Material->EnsureIsComplete()",
            "FeatureLevel = GMaxRHIFeatureLevel",
            "GetMaterialResource(FeatureLevel)",
            "Resource->SubmitCompileJobs_GameThread",
            "Resource->FinishCompilation()",
            "Resource->IsGameThreadShaderMapComplete()",
            "ShaderMap->IsValidForRendering()",
            "Resource->IsDefaultMaterial()",
            "Resource->GetCompileErrors()",
            "ExactScalar",
            "ExactVector",
            "ExactTexture",
            "ExactCustom",
            'Parameter->Group == TEXT("Istana Public Realm Visual R2")',
            "Parameter->SortPriority == 32",
            "!Parameter->bUseCustomPrimitiveData",
            "Parameter->PrimitiveDataIndex == 0",
            "Sample->ConstMipValue == 0",
            "Custom->Inputs.Num() == 1",
            "Custom->AdditionalOutputs.IsEmpty()",
            "Custom->AdditionalDefines.IsEmpty()",
            "Custom->IncludeFilePaths.IsEmpty()",
            "Material->GetUsageByFlag(MATUSAGE_Nanite)",
            "Material->bEnableTessellation",
            "Material->bEnableDisplacementFade",
            "Material->MaxWorldPositionOffsetDisplacement",
            "Data->WorldPositionOffset.Expression",
            "Data->Displacement.Expression",
            "Data->PixelDepthOffset.Expression",
            "Data->MaterialAttributes.Expression",
            "Data->ClearCoat.Expression",
            "Data->ClearCoatRoughness.Expression",
            "Data->ShadingModelFromMaterialExpression.Expression",
            "Data->SurfaceThickness.Expression",
            "bCustomizedUvConnected",
            "Uv0->UnMirrorU",
            "Uv0->UnMirrorV",
            "RotatedSecondary->CenterX",
            "RotatedSecondary->CenterY",
            "RotatedSecondary->Speed",
            "RotatedSecondary->ConstCoordinate",
            "InputIs(PrimaryUv->A, Uv0)",
            "InputIs(PrimaryUv->B, PrimaryMeters)",
            "InputIs(SecondaryUnrotated->A, Uv0)",
            "InputIs(SecondaryUnrotated->B, SecondaryMeters)",
            "InputIs(RotatedSecondary->Coordinate, SecondaryUnrotated)",
            "InputIs(RotatedSecondary->Time, Rotation)",
            "InputIs(SecondaryUv->A, RotatedSecondary)",
            "InputIs(SecondaryUv->B, SecondaryOffset)",
            "InputIs(MacroUv->A, Uv0)",
            "InputIs(MacroUv->B, MacroMeters)",
            "InputIs(BasePrimary->Coordinates, PrimaryUv)",
            "InputIs(BaseSecondary->Coordinates, SecondaryUv)",
            "InputIs(NormalPrimary->Coordinates, PrimaryUv)",
            "InputIs(NormalSecondary->Coordinates, SecondaryUv)",
            "InputIs(OrdpPrimary->Coordinates, PrimaryUv)",
            "InputIs(OrdpSecondary->Coordinates, SecondaryUv)",
            "InputIs(BaseBlend->A, BasePrimary)",
            "InputIs(BaseBlend->B, BaseSecondary)",
            "InputIs(BaseBlend->Alpha, SecondaryBlend)",
            "InputIs(TintedBase->A, BaseBlend)",
            "InputIs(TintedBase->B, AlbedoTint)",
            "AsphaltMacroFieldCode",
            "AsphaltRotatedNormalCode",
            "InputIs(MacroTint->A, MacroTintLow)",
            "InputIs(MacroTint->B, MacroTintHigh)",
            "InputIs(MacroTint->Alpha, MacroField)",
            "InputIs(FinalBase->A, TintedBase)",
            "InputIs(FinalBase->B, MacroTint)",
            "InputIs(RoughnessBlend->A, OrdpPrimary, 2)",
            "InputIs(RoughnessBlend->B, OrdpSecondary, 2)",
            "InputIs(RoughnessBlend->Alpha, SecondaryBlend)",
            "InputIs(RoughnessAdd->A, RoughnessBlend)",
            "InputIs(RoughnessAdd->B, RoughnessBias)",
            "InputIs(FinalRoughness->Input, RoughnessAdd)",
            "InputIs(AoBlend->A, OrdpPrimary, 1)",
            "InputIs(AoBlend->B, OrdpSecondary, 1)",
            "InputIs(AoBlend->Alpha, SecondaryBlend)",
            "InputIs(NormalBlend->A, NormalPrimary)",
            "InputIs(NormalBlend->B, RotatedNormal)",
            "InputIs(NormalBlend->Alpha, SecondaryBlend)",
            "InputIs(StrengthenedNormal->A, FlatNormal)",
            "InputIs(StrengthenedNormal->B, NormalBlend)",
            "InputIs(StrengthenedNormal->Alpha, NormalStrength)",
            "InputIs(FinalNormal->VectorInput, StrengthenedNormal)",
            "InputIs(Data->BaseColor, FinalBase)",
            "InputIs(Data->Roughness, FinalRoughness)",
            "InputIs(Data->AmbientOcclusion, AoBlend)",
            "InputIs(Data->Normal, FinalNormal)",
            "InputIs(Data->Specular, Specular)",
            "InputIs(Data->Metallic, Metallic)",
        ):
            self.assertIn(marker, validate)

        input_is = function_body(self.factory_cpp, "bool InputIs(")
        for marker in (
            "Expression->Outputs.IsValidIndex(OutputIndex)",
            "Input.Mask == Output.Mask",
            "Input.MaskR == Output.MaskR",
            "Input.MaskG == Output.MaskG",
            "Input.MaskB == Output.MaskB",
            "Input.MaskA == Output.MaskA",
        ):
            self.assertIn(marker, input_is)

        final_validation = function_body(
            self.factory_cpp, "bool ValidateInternal("
        )
        for field in (
            "assets=5",
            "legacyMeshes=2",
            "concreteMics=1",
            "isolatedVisualMaterials=2",
            "pbrAsphaltMaterials=1",
            "fullyClippedRoadGraphicMaterials=1",
            "metricUvAntiTiling=true",
            "primaryTileMeters=2.0",
            "secondaryTileMeters=3.37",
            "secondaryRotationDegrees=37",
            "macroTileMeters=37.0",
            "pbrTextures=D,N,ORDp",
            "texturePackagesSha256Pinned=true",
            "ordpChannels=ao:R,roughness:G",
            "displacementConsumed=false",
            "normalStrength=0.42",
            "roughnessBias=0.04",
            "specular=0.25",
            "nanite=true",
            "wpo=false",
            "displacement=false",
            "pdo=false",
            "immutableMeshPackages=true",
            "immutableV5CSourceBindings=true",
            "componentOverridePresentation=true",
            "fullyClippedPlanningGraphic=true",
            "laneOrCrossingPaintAuthored=false",
        ):
            self.assertIn(field, final_validation)

    def test_road_graphic_is_an_exact_fully_clipped_one_node_material(self) -> None:
        self.assertEqual(
            adjacent_cpp_string(
                self.factory_cpp,
                "const FString RoadGraphicSuppressionMaterialName(",
            ),
            ROAD_GRAPHIC_SUPPRESSION_NAME,
        )
        self.assertEqual(
            f"{ASPHALT_ROOT}/{ROAD_GRAPHIC_SUPPRESSION_NAME}."
            f"{ROAD_GRAPHIC_SUPPRESSION_NAME}",
            ROAD_GRAPHIC_SUPPRESSION_PATH,
        )
        self.assertIn(
            "const FString& GetRoadGraphicSuppressionMaterialObjectPath()",
            self.factory_h,
        )

        create = function_body(
            self.factory_cpp,
            "UMaterial* CreateRoadGraphicSuppressionMaterial(",
        )
        for marker in (
            "UMaterialFactoryNew",
            "RoadGraphicSuppressionMaterialName",
            "VisualMaterialRoot",
            "UMaterialExpressionConstant",
            'TEXT("RoadGraphic.FullyClippedOpacityMask")',
            "FullyClipped->R = 0.0f",
            "Material->MaterialDomain = MD_Surface",
            "Material->BlendMode = BLEND_Masked",
            "Material->OpacityMaskClipValue = 0.5f",
            "Material->SetShadingModel(MSM_Unlit)",
            "Material->TwoSided = false",
            "Material->bCastRayTracedShadows = false",
            "Material->NaniteOverrideMaterial.bEnableOverride = false",
            "Material->NaniteOverrideMaterial.OverrideMaterialEditor = nullptr",
            "Data->OpacityMask.Connect(0, FullyClipped)",
            "UMaterialEditingLibrary::RecompileMaterial(Material)",
            "FAssetCompilingManager::Get().FinishAllCompilation()",
        ):
            self.assertIn(marker, create)
        for prohibited in (
            "UMaterialExpressionTexture",
            "UMaterialExpressionScalarParameter",
            "UMaterialExpressionVectorParameter",
            "UMaterialExpressionCustom",
            "Data->BaseColor.Connect",
            "Data->EmissiveColor.Connect",
            "Data->Opacity.Connect",
            "Data->WorldPositionOffset.Connect",
            "Data->Displacement.Connect",
            "Data->PixelDepthOffset.Connect",
            "Data->MaterialAttributes.Connect",
        ):
            self.assertNotIn(prohibited, create)

        validate = function_body(
            self.factory_cpp,
            "bool ValidateRoadGraphicSuppressionMaterial(",
        )
        for marker in (
            "Material->EnsureIsComplete()",
            "FeatureLevel = GMaxRHIFeatureLevel",
            "GetMaterialResource(FeatureLevel)",
            "Resource->SubmitCompileJobs_GameThread",
            "Resource->FinishCompilation()",
            "Resource->IsGameThreadShaderMapComplete()",
            "ShaderMap->IsValidForRendering()",
            "Resource->IsDefaultMaterial()",
            "Resource->GetCompileErrors()",
            "Material->GetClass() != UMaterial::StaticClass()",
            "RoadGraphicSuppressionMaterialObjectPath",
            "Data->ExpressionCollection.Expressions.Num() != 1",
            "UMaterialExpressionConstant::StaticClass()",
            'TEXT("RoadGraphic.FullyClippedOpacityMask")',
            "FullyClipped->MaterialExpressionEditorX != -300",
            "FullyClipped->MaterialExpressionEditorY != 0",
            "FMath::IsNearlyZero(FullyClipped->R)",
            "Material->MaterialDomain != MD_Surface",
            "Material->BlendMode != BLEND_Masked",
            "Material->OpacityMaskClipValue, 0.5f",
            "HasOnlyShadingModel(MSM_Unlit)",
            "Material->TwoSided",
            "Material->bCastRayTracedShadows",
            "Material->NaniteOverrideMaterial.bEnableOverride",
            "Material->NaniteOverrideMaterial.GetOverrideMaterial()",
            "Material->GetNaniteOverride()",
            "InputIs(Data->OpacityMask, FullyClipped)",
            "Data->BaseColor.Expression",
            "Data->EmissiveColor.Expression",
            "Data->Opacity.Expression",
            "Data->WorldPositionOffset.Expression",
            "Data->Displacement.Expression",
            "Data->PixelDepthOffset.Expression",
            "Data->MaterialAttributes.Expression",
            "bCustomizedUvConnected",
        ):
            self.assertIn(marker, validate)

    def test_editor_api_is_remote_callable_idempotent_and_atomic(self) -> None:
        for marker in (
            "UBlueprintFunctionLibrary",
            "UFUNCTION(BlueprintCallable",
            "ImportIstanaExploreV5DPublicRealmAssets",
            "ValidateIstanaExploreV5DPublicRealmAssets",
        ):
            self.assertIn(marker, self.editor_h)
        body = function_body(
            self.editor_cpp,
            "ImportIstanaExploreV5DPublicRealmAssets(FString& OutMessage)",
        )
        for marker in (
            "ValidateAssets",
            "IDEMPOTENT_EXPLORE_V5D_PUBLIC_REALM_ASSETS_ALREADY_VALID",
            "ValidateLegacyAssets",
            "CreateFreshVisualMaterialUpgrade",
            "CreateFreshAssets",
            "bVisualUpgrade",
            "HasExactFreshRoster",
            "SaveLoadedAssets(FreshAssets, false)",
            "UPackageTools::ReloadPackages",
            "DeleteExactTransactionAssets",
            "IMPORT_FAILED_ATOMIC_SAVE",
            "IMPORT_FAILED_COLD_VALIDATION",
            "No actor or map was changed",
        ):
            self.assertIn(marker, body)
        self.assertNotIn("SaveDirectory", body)
        self.assertNotIn("SaveAsset(", body)

        visual_roster = function_body(
            self.editor_cpp, "TArray<FString> ExpectedVisualUpgradeObjectPaths()"
        )
        self.assertEqual(visual_roster.count("GetAsphaltMaterialObjectPath()"), 1)
        self.assertEqual(
            visual_roster.count(
                "GetRoadGraphicSuppressionMaterialObjectPath()"
            ),
            1,
        )
        for prohibited in (
            "GetConcreteMaterialObjectPath()",
            "GetCoreMeshObjectPath()",
            "GetFallbackMeshObjectPath()",
        ):
            self.assertNotIn(prohibited, visual_roster)
        full_roster = function_body(
            self.editor_cpp, "TArray<FString> ExpectedAllFreshObjectPaths()"
        )
        for marker in (
            "GetAsphaltMaterialObjectPath()",
            "GetRoadGraphicSuppressionMaterialObjectPath()",
            "GetConcreteMaterialObjectPath()",
            "GetCoreMeshObjectPath()",
            "GetFallbackMeshObjectPath()",
        ):
            self.assertEqual(full_roster.count(marker), 1)
        self.assertIn(
            "neither the exact five fresh assets nor the exact two-asset Visual R2 upgrade",
            self.editor_cpp,
        )
        self.assertIn('TEXT("additive-two-material-visual-r2")', body)
        self.assertIn('TEXT("fresh-five-assets")', body)

        fresh = function_body(self.factory_cpp, "bool CreateFreshAssets(")
        for marker in (
            "CreateAsphaltMaterial(",
            "ValidateAsphaltMaterial(",
            "CreateRoadGraphicSuppressionMaterial(",
            "ValidateRoadGraphicSuppressionMaterial(",
            "OutAssets.Num() != ExpectedAssetCount",
            "ValidateInternal(false, Validation)",
        ):
            self.assertIn(marker, fresh)
        self.assertEqual(fresh.count("OutAssets.Add("), 5)

        upgrade = function_body(
            self.factory_cpp, "bool CreateFreshVisualMaterialUpgrade("
        )
        for marker in (
            "ValidateLegacyInternal(true, LegacyReport)",
            "ValidateAsphaltTextureDependencies(Textures, OutError)",
            "GatherVisualMaterialAssets(ExistingVisual, OutError)",
            "ExistingVisual.IsEmpty()",
            "CreateAsphaltMaterial(",
            "ValidateAsphaltMaterial(Asphalt, Textures, OutError)",
            "CreateRoadGraphicSuppressionMaterial(",
            "ValidateRoadGraphicSuppressionMaterial(",
            "ValidateInternal(false, Validation)",
        ):
            self.assertIn(marker, upgrade)
        self.assertEqual(upgrade.count("OutAssets.Add("), 2)
        self.assertIn(
            "FScopedFreshRollback Rollback(OutAssets, OutError, true)",
            upgrade,
        )
        for prohibited in (
            "CreateConcreteMaterial(",
            "MakeImportTask(",
            "ImportAssetTasks(",
            "NormalizeAndBindMaterials(",
            "SetStaticMaterials(",
        ):
            self.assertNotIn(prohibited, upgrade)

        rollback = self.factory_cpp[
            self.factory_cpp.index("class FScopedFreshRollback final") :
            self.factory_cpp.index("bool ValidateParentMaterial(")
        ]
        self.assertIn("if (!bVisualUpgradeOnly)", rollback)
        self.assertIn("GatherRootAssets(Data, Ignored)", rollback)
        self.assertIn("GatherVisualMaterialAssets(VisualData, Ignored)", rollback)
        self.assertLess(
            rollback.index("if (!bVisualUpgradeOnly)"),
            rollback.index("GatherRootAssets(Data, Ignored)"),
        )

    def test_runtime_contract_has_one_validated_provenance_source(self) -> None:
        self.assertIn("LoadValidatedRuntimeContract", self.factory_h)
        body = function_body(
            self.factory_cpp, "bool LoadValidatedRuntimeContract("
        )
        for marker in (
            "ValidateAssets(Report)",
            "OutAssets.CoreMesh",
            "OutAssets.FallbackMesh",
            "OutAssets.RoadBaseMaterial",
            "OutAssets.RoadGraphicMaterial",
            "OutAssets.ConcreteMaterial",
            'SourceEpoch = TEXT("2026-08-25")',
            "CoreSourceIdentifier",
            "ExpectedCoreObjSha256",
            "FallbackSourceIdentifier",
            "ExpectedFallbackObjSha256",
            "bRenderOnly = true",
            "bMeasuredSurveyOrAsBuiltClaimed = false",
            "bCollisionNavigationSensorOrRfAuthority = false",
            "bProviderContentBakedCachedTracedOrAnalysed = false",
            "bR15GroundMaterialPackagesUntouched = true",
            "bExistingRfInputsUntouched = true",
            "ValidateProvenanceContract",
        ):
            self.assertIn(marker, body)
        self.assertEqual(
            body.count(
                "LoadExact<UMaterialInterface>(AsphaltMaterialObjectPath)"
            ),
            1,
        )
        self.assertIn(
            "OutAssets.RoadBaseMaterial =\n"
            "        LoadExact<UMaterialInterface>(AsphaltMaterialObjectPath)",
            body,
        )
        self.assertIn(
            "OutAssets.RoadGraphicMaterial =\n"
            "        LoadExact<UMaterialInterface>(\n"
            "            RoadGraphicSuppressionMaterialObjectPath)",
            body,
        )
        self.assertEqual(
            f"{ASPHALT_ROOT}/{ASPHALT_NAME}.{ASPHALT_NAME}",
            ASPHALT_PATH,
        )

    def test_actor_correction_uses_exact_overrides_and_atomic_cleanup(self) -> None:
        validate_assets = function_body(
            self.actor_cpp, "bool ValidatePresentationAssets("
        )
        for marker in (
            "RoadAsphaltMaterialObjectPath",
            "RoadGraphicSuppressionMaterialObjectPath",
            "LegacyRoadBaseMaterialObjectPath",
            "LegacyRoadGraphicMaterialObjectPath",
            "ValidateExactMeshSourceBindings",
        ):
            self.assertIn(marker, validate_assets)

        configure = function_body(self.actor_cpp, "ConfigurePublicRealm(")
        core_override = (
            "CorePublicRealmRenderOnly->SetMaterial(0, "
            "SavedAssets.RoadBaseMaterial)"
        )
        fallback_road = (
            "FallbackPublicRealmRenderOnly->SetMaterial(\n"
            "        0,\n"
            "        SavedAssets.RoadBaseMaterial)"
        )
        fallback_graphic = (
            "FallbackPublicRealmRenderOnly->SetMaterial(\n"
            "        1,\n"
            "        SavedAssets.RoadGraphicMaterial)"
        )
        for marker in (
            core_override,
            fallback_road,
            fallback_graphic,
            "AppearanceRevision = PbrAppearanceRevision",
            "bProviderReady = false",
        ):
            self.assertIn(marker, configure)
        failure = configure[configure.index("if (!ValidatePublicRealm(Report))") :]
        for component in (
            "CorePublicRealmRenderOnly",
            "FallbackPublicRealmRenderOnly",
        ):
            clear = f"{component}->EmptyOverrideMaterials()"
            detach = f"{component}->SetStaticMesh(nullptr)"
            self.assertIn(clear, failure)
            self.assertIn(detach, failure)
            self.assertLess(failure.index(clear), failure.index(detach))
        for marker in (
            "SavedAssets = FTRIADIstanaExploreV5DPublicRealmAssets()",
            "SavedProvenance = FTRIADIstanaExploreV5DPublicRealmProvenance()",
            "AppearanceRevision = LegacyAppearanceRevision",
            "bConfigured = false",
        ):
            self.assertIn(marker, failure)

        upgrade = function_body(self.actor_cpp, "UpgradeVisualMaterials(")
        self.assertIn("OutError.Reset()", upgrade)
        self.assertIn("ValidateLegacyVisualMaterialContract", upgrade)
        legacy = function_body(
            self.actor_cpp,
            "ValidateLegacyVisualMaterialContract(FString& OutReport) const",
        )
        for marker in (
            "AppearanceRevision != LegacyAppearanceRevision",
            "Tags.Contains(ExpectedActorTag())",
            "Tags.Contains(HumanOnlyOverlayTag)",
            "ClaimLabel != ExpectedClaimLabel()",
            "bCollisionOrNavigationAuthority",
            "bSensorOrRfMaterialAuthority",
            "bRuntimeGeometryGenerated",
            "bR15GroundMaterialPackagesModified",
            "bExistingRfInputsModified",
            "PrimaryActorTick.bCanEverTick",
            "GetActorEnableCollision()",
            "GetActorTransform().Equals(FTransform::Identity, 0.001)",
            "SceneRoot != RootComponent",
            "LegacyRoadBaseMaterialObjectPath",
            "LegacyRoadGraphicMaterialObjectPath",
            "NoOverrides",
        ):
            self.assertIn(marker, legacy)
        self.assertIn("appearanceRevision=1 overrides=0", legacy)


if __name__ == "__main__":
    unittest.main()
