from __future__ import annotations

import hashlib
import re
import unittest
from collections import Counter
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal/Plugins/TRIADSensorFusion"
SOURCE = (
    Path("D:/triad/TRIAD/Saved/TRIAD/IstanaReferences/V5D/Current20260831")
    / "GeneratedCurrent"
)
CONTRACT = (
    REPO
    / "unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings"
    / "istana_public_view_v5d_surroundings.contract.json"
)
OBJ = SOURCE / "SM_IPV5D_OSMCurrentSurroundings_Render.obj"
MTL = SOURCE / "SM_IPV5D_OSMCurrentSurroundings_Render.mtl"
MANIFEST = SOURCE / "IstanaPublicViewV5DSurroundings.manifest.json"
FEATURES = SOURCE / "IstanaPublicViewV5DSurroundings.features.json"
GEOMETRY = SOURCE / "IstanaPublicViewV5DSurroundings.geometry.json"
FACTORY_H = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.h"
)
FACTORY_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.cpp"
)
EDITOR_H = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Public"
    / "TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h"
)
EDITOR_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp"
)
PROVENANCE_H = (
    PLUGIN
    / "Source/TRIADSensorFusion/Public"
    / "TRIADIstanaExploreV5DCurrentSurroundingsProvenance.h"
)
PROVENANCE_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusion/Private"
    / "TRIADIstanaExploreV5DCurrentSurroundingsProvenance.cpp"
)


EXPECTED_MATERIALS = (
    ("MAT_BOTTOM_HIDDEN", 9457, "FallbackRoof"),
    ("MAT_COMMERCIAL_HINT", 2188, "FallbackWall"),
    ("MAT_GENERIC_BUILDING_HINT", 14704, "FallbackWall"),
    ("MAT_HEALTHCARE_HINT", 278, "FallbackWall"),
    ("MAT_HOTEL_HINT", 414, "OfficialWall"),
    ("MAT_INDUSTRIAL_HINT", 8, "FallbackWall"),
    ("MAT_RELIGIOUS_HINT", 150, "OfficialWall"),
    ("MAT_RESIDENTIAL_HINT", 6340, "FallbackWall"),
    ("MAT_ROOF_COMMERCIAL_HINT", 1038, "FallbackRoof"),
    ("MAT_ROOF_GENERIC_BUILDING_HINT", 5454, "FallbackRoof"),
    ("MAT_ROOF_HEALTHCARE_HINT", 131, "FallbackRoof"),
    ("MAT_ROOF_HOTEL_HINT", 169, "OfficialRoof"),
    ("MAT_ROOF_INDUSTRIAL_HINT", 2, "FallbackRoof"),
    ("MAT_ROOF_RELIGIOUS_HINT", 53, "OfficialRoof"),
    ("MAT_ROOF_RESIDENTIAL_HINT", 2694, "FallbackRoof"),
    ("MAT_ROOF_TRANSPORT_HINT", 132, "FallbackRoof"),
    ("MAT_TRANSPORT_HINT", 332, "FallbackWall"),
)


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


class IstanaExploreV5DCurrentSurroundingsAssetContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.factory_h = read(FACTORY_H)
        cls.factory_cpp = read(FACTORY_CPP)
        cls.editor_h = read(EDITOR_H)
        cls.editor_cpp = read(EDITOR_CPP)
        cls.provenance_h = read(PROVENANCE_H)
        cls.provenance_cpp = read(PROVENANCE_CPP)
        cls.combined = (
            cls.factory_h + cls.factory_cpp + cls.editor_h + cls.editor_cpp
        )

    def test_isolated_remote_callable_one_asset_boundary(self) -> None:
        for marker in (
            "ImportIstanaExploreV5DCurrentSurroundingsAsset",
            "ValidateIstanaExploreV5DCurrentSurroundingsAsset",
            "/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings",
            "SM_IPV5D_OSMCurrentSurroundings_Render",
            "ExpectedAssetCount = 1",
            "Actual != Expected",
            "SaveLoadedAssets(ExactSaveTargets, false)",
            "UPackageTools::ReloadPackages",
            "No actor or map was changed",
        ):
            self.assertIn(marker, self.combined)
        self.assertNotIn("TRIADIstanaPublicViewSceneActor", self.combined)
        self.assertNotIn("IstanaExploreV5DHybrid", self.combined)

    def test_canonical_root_cardinality_counts_direct_assets_only(self) -> None:
        gather = function_body(self.factory_cpp, "bool GatherRootAssets(")
        self.assertIn(
            "GetAssetsByPath(FName(*AssetRoot), OutAssets, false, false)",
            gather,
        )
        self.assertNotIn(
            "GetAssetsByPath(FName(*AssetRoot), OutAssets, true, false)",
            gather,
        )
        self.assertIn("Independently owned landmark packages", gather)
        for consumer in (
            "bool ValidateExactRootRoster(",
            "~FScopedFreshRollback()",
            "bool CreateFreshAsset(",
        ):
            self.assertIn("GatherRootAssets", function_body(self.factory_cpp, consumer))

    def test_cooked_provenance_is_exact_and_not_editor_only(self) -> None:
        runtime = self.provenance_h + self.provenance_cpp
        for marker in (
            "UAssetUserData",
            "TRIADSENSORFUSION_API",
            "SourceEpoch",
            'TEXT("2026-08-31")',
            "OutputSetSha256",
            "27EF5679CA0010AB646C3DA542B1F0EC6EA163504845BD446FA5D578095AC4FD",
            "RenderObjSha256",
            "1612461DBC3FE8C7C760517C0A743B631CDFE23307792AEB8BB04E15855B59A3",
            "SelectedFeatures = 1389",
            "PolygonParts = 1391",
            "SourceVertices = 24522",
            "SourceVertexInstances = 130632",
            "Triangles = 43544",
            "bPhysicalMetreUv0Validated = true",
            "bRenderOnly = true",
            "bCollisionNavigationSensorRfAuthority = false",
            "bMeasuredSurveyAsBuiltHyperreal = false",
            "IsCanonicalContract",
        ):
            self.assertIn(marker, runtime)
        self.assertNotIn("WITH_EDITORONLY_DATA", runtime)
        self.assertNotIn("#if WITH_EDITOR", runtime)

        stamp = function_body(
            self.factory_cpp, "bool StampCanonicalProvenance("
        )
        for marker in (
            "CountProvenanceObjects",
            "RemoveUserDataOfClass",
            "NewObject<UTRIADIstanaExploreV5DCurrentSurroundingsProvenance>",
            "SetCanonicalContract",
            "AddAssetUserData",
            "ValidateProvenance",
        ):
            self.assertIn(marker, stamp)
        validate = function_body(self.factory_cpp, "bool ValidateProvenance(")
        for marker in (
            "GetAssetUserDataArray",
            "MatchingCount != 1",
            "GetClass() !=",
            "GetOuter() != Mesh",
            "RF_Transient",
            "IsCanonicalContract",
        ):
            self.assertIn(marker, validate)
        create = function_body(self.factory_cpp, "bool CreateFreshAsset(")
        self.assertIn("StampCanonicalProvenance", create)
        mesh_validate = function_body(self.factory_cpp, "bool ValidateMesh(")
        self.assertIn("ValidateProvenance", mesh_validate)
        self.assertEqual(self.provenance_h.count("UPROPERTY("), 12)

    def test_exact_source_guards_and_mesh_policy_are_static_contracts(self) -> None:
        expected = {
            OBJ: (
                6359246,
                "1612461DBC3FE8C7C760517C0A743B631CDFE23307792AEB8BB04E15855B59A3",
            ),
            MTL: (
                2645,
                "751DE195642892F781731EBD0F9EB3C05F23731CC674F76E7EF335E4F909342A",
            ),
            MANIFEST: (
                4533,
                "974583C86473D55CFFE3BF8A57660E787A946CD13E688D5DE5779A962DDC40F7",
            ),
            FEATURES: (
                3248669,
                "6EE2FAC283BC036F1FB67055D7CB9D2613D00C6B6F267D3329C88CCC5A5D7AE6",
            ),
            GEOMETRY: (
                4899394,
                "E1C4432097C4360676E96C4B1ED4672FB5C11E37306544697A4576EC01256D62",
            ),
            CONTRACT: (
                6328,
                "69C53E4E3E772A2DABE4BAF1A5097222B5CA6A7ADE11FBE3CE3544F4F9D61088",
            ),
        }
        for _, (size, digest) in expected.items():
            self.assertIn(str(size), self.factory_cpp)
            self.assertIn(digest, self.factory_cpp)

        task = function_body(self.factory_cpp, "UAssetImportTask* MakeImportTask()")
        for marker in (
            "bImportMaterials = false",
            "bImportTextures = false",
            "bConvertScene = false",
            "bConvertSceneUnit = false",
            "ImportUniformScale = 100.0f",
            "bCombineMeshes = true",
            "bReorderMaterialToFbxOrder = true",
            "bTransformVertexToAbsolute = true",
            "bAutoGenerateCollision = false",
            "bGenerateLightmapUVs = false",
            "FBXNIM_ComputeNormals",
            "bBuildNanite = true",
            "bReplaceExisting = false",
            "bSave = false",
        ):
            self.assertIn(marker, task)

        render_only = function_body(self.factory_cpp, "void MakeRenderOnly(")
        for marker in (
            "RestoreIstanaLocalHandedness",
            "FVector(1.0, -1.0, 1.0)",
            "FStaticMeshOperations::ApplyTransform",
            "Mesh->CommitMeshDescription(0)",
        ):
            self.assertIn(marker, render_only)

        validate = function_body(self.factory_cpp, "bool ValidateMesh(")
        for marker in (
            "ExpectedSourceVertexCount = 24522",
            "ExpectedTriangleCount = 43544",
            "ExpectedMaterialCount = 17",
            "BuildScale3D !=",
            "bGenerateLightmapUVs",
            "bUseFullPrecisionUVs",
            "HasValidNaniteData",
            "CTF_UseSimpleAsComplex",
            "bHasNavigationData",
            "GetNavCollision",
            "FVector(-98438.3058, -99831.9761, 0.0)",
            "FVector(99875.1074, 99339.8856, 15200.0)",
        ):
            self.assertIn(marker, self.factory_cpp if marker.startswith("Expected") else validate)

    def test_canonical_lexical_material_roster_and_v5c_bindings(self) -> None:
        positions: list[int] = []
        for name, triangles, role in EXPECTED_MATERIALS:
            literal = (
                f'{{TEXT("{name}"), {triangles}, '
                f"EV5CMaterialRole::{role}}}"
            )
            self.assertIn(literal, self.factory_cpp)
            positions.append(self.factory_cpp.index(literal))
        self.assertEqual(positions, sorted(positions))
        self.assertEqual(
            [name for name, _, _ in EXPECTED_MATERIALS],
            sorted(name for name, _, _ in EXPECTED_MATERIALS),
        )
        self.assertEqual(sum(count for _, count, _ in EXPECTED_MATERIALS), 43544)
        for material in (
            "M_IPV5C_OfficialContextRender",
            "M_IPV5C_OfficialContextRoof",
            "M_IPV5C_OsmFallbackContextRender",
            "M_IPV5C_OsmFallbackContextRoof",
        ):
            self.assertIn(material, self.factory_cpp)
        self.assertIn(
            "TRIADIstanaExploreV5CSurroundingsAssetFactory::\n"
            "            GetOrderedMaterialObjectPaths()",
            self.factory_cpp,
        )
        self.assertNotIn(
            "TRIADIstanaExploreV5CSurroundingsAssetFactory::ValidateAssets",
            self.factory_cpp,
        )
        self.assertIn("SM6-only editor session", self.factory_cpp)

    @unittest.skipUnless(OBJ.is_file(), "external current derivative is not mounted")
    def test_mounted_current_derivative_matches_the_admission_contract(self) -> None:
        expected_files = {
            OBJ: (
                6359246,
                "1612461DBC3FE8C7C760517C0A743B631CDFE23307792AEB8BB04E15855B59A3",
            ),
            MTL: (
                2645,
                "751DE195642892F781731EBD0F9EB3C05F23731CC674F76E7EF335E4F909342A",
            ),
            MANIFEST: (
                4533,
                "974583C86473D55CFFE3BF8A57660E787A946CD13E688D5DE5779A962DDC40F7",
            ),
            FEATURES: (
                3248669,
                "6EE2FAC283BC036F1FB67055D7CB9D2613D00C6B6F267D3329C88CCC5A5D7AE6",
            ),
            GEOMETRY: (
                4899394,
                "E1C4432097C4360676E96C4B1ED4672FB5C11E37306544697A4576EC01256D62",
            ),
            CONTRACT: (
                6328,
                "69C53E4E3E772A2DABE4BAF1A5097222B5CA6A7ADE11FBE3CE3544F4F9D61088",
            ),
        }
        for path, (size, digest) in expected_files.items():
            with self.subTest(path=path.name):
                self.assertEqual(path.stat().st_size, size)
                self.assertEqual(sha256(path), digest)

        vertices = 0
        texture_vertices = 0
        faces = 0
        current_material: str | None = None
        triangles: Counter[str] = Counter()
        with OBJ.open(encoding="utf-8") as stream:
            for line in stream:
                if line.startswith("v "):
                    vertices += 1
                elif line.startswith("vt "):
                    texture_vertices += 1
                elif line.startswith("usemtl "):
                    current_material = line.split(maxsplit=1)[1].strip()
                elif line.startswith("f "):
                    self.assertIsNotNone(current_material)
                    corners = len(re.split(r"\s+", line.strip())) - 1
                    count = corners - 2
                    triangles[current_material] += count
                    faces += count
        self.assertEqual(vertices, 24522)
        self.assertEqual(texture_vertices, 130632)
        self.assertEqual(faces, 43544)
        self.assertEqual(
            triangles,
            Counter({name: count for name, count, _ in EXPECTED_MATERIALS}),
        )


if __name__ == "__main__":
    unittest.main()
