from __future__ import annotations

import hashlib
import json
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
RUNTIME_H = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV3SupplementActor.h"
)
RUNTIME_CPP = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV3SupplementActor.cpp"
)
EDITOR_H = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV3EditorLibrary.h"
)
EDITOR_CPP = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV3EditorLibrary.cpp"
)
SCENE_H = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaPublicViewSceneActor.h"
)
SCENE_CPP = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaPublicViewSceneActor.cpp"
)

V3_SOURCE_ROOT = REPO / "unreal/SourceAssets/IstanaPublicViewExploreV3"
PORTICO_SOURCE_ROOT = REPO / "unreal/SourceAssets/IstanaPublicViewV7Portico"
PREPARED_MANIFEST = (
    V3_SOURCE_ROOT / "Prepared/ExploreV3.prepared-render-assets.manifest.json"
)
V3_INTEGRATION_CONTRACT = REPO / (
    "unreal/SourceAssets/IstanaPublicViewExploreV3Unreal/"
    "explore_v3_unreal_integration.contract.json"
)


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


class IstanaExploreV3UnrealContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.runtime_h = text(RUNTIME_H)
        cls.runtime_cpp = text(RUNTIME_CPP)
        cls.editor_h = text(EDITOR_H)
        cls.editor_cpp = text(EDITOR_CPP)
        cls.scene_h = text(SCENE_H)
        cls.scene_cpp = text(SCENE_CPP)
        cls.all_source = "\n".join(
            (cls.runtime_h, cls.runtime_cpp, cls.editor_h, cls.editor_cpp)
        )
        cls.prepared_manifest = json.loads(text(PREPARED_MANIFEST))
        cls.integration_contract = json.loads(text(V3_INTEGRATION_CONTRACT))

    def test_exact_v2_to_v3_map_chain_is_additive_and_non_overwriting(self) -> None:
        self.assertIn(
            'SourceMapPackage(TEXT("/Game/Maps/Istana_PublicView_Explore_v2"))',
            self.editor_cpp,
        )
        self.assertIn(
            'DestinationMapPackage(TEXT("/Game/Maps/Istana_PublicView_Explore_v3"))',
            self.editor_cpp,
        )
        self.assertIn(
            "/Game/Maps/Istana_PublicView_Explore_v3.Istana_PublicView_Explore_v3",
            self.editor_cpp,
        )
        self.assertIn(
            '/Game/TRIAD/IstanaPublicViewExploreV3', self.editor_cpp
        )
        self.assertIn("DuplicateLoadedAsset", self.editor_cpp)
        self.assertIn("bReplaceExisting = false", self.editor_cpp)
        self.assertNotIn("bReplaceExisting = true", self.editor_cpp)
        self.assertIn("BuildIstanaExploreV3Map", self.editor_h)
        self.assertIn("ValidateIstanaExploreV3Map", self.editor_h)

    def test_frozen_inputs_are_bound_by_exact_sha256_and_byte_counts(self) -> None:
        expected = {
            V3_SOURCE_ROOT / "Sources/source-manifest.json": (
                36624,
                "CA26023F45BAA1344ED0DC031766355D734EC9B89C8B25F089ADFA18FEA12290",
            ),
            REPO
            / "unreal/SourceAssets/IstanaPublicViewExploreV3Unreal/"
            "explore_v3_unreal_integration.contract.json": (
                11892,
                "8F287CFF182A121D62CF617AD4E3BA758092D00F3F07A6D0C342007793216250",
            ),
            PREPARED_MANIFEST: (
                4584,
                "46CA095EC6B200DD2C3E7A0BB89C377968FEF17C7A4C8EF07C0681366BCBE82D",
            ),
            V3_SOURCE_ROOT
            / "Generated/GeospatialContext/geospatial_context.manifest.json": (
                7830,
                "FBC1032E60591AA8CEA0C1B3E16BDB9E9776C84692533CABC11D8EB7A79D6A56",
            ),
            V3_SOURCE_ROOT / "geospatial_context.acceptance.lock.json": (
                4334,
                "7A9A78F4E0E1AA6FF2A9FC9E7623B4E27C23DC57C706B8D5EFBB464A4D17BCAD",
            ),
            PORTICO_SOURCE_ROOT / "v5_live_receiver_binding_v7.json": (
                4613,
                "5955C9194B5A342D11054996BD759D54212A95FBEF8042F5D5111CF2ADC8AC46",
            ),
            PORTICO_SOURCE_ROOT
            / "GeneratedV5Live/"
            "IstanaPublicViewV7CentralPorticoRefinementV5Live.manifest.json": (
                7599,
                "10EE96E2A2D59F510102F9854A5079A2E51D6E79C6A795C524608AC86799685A",
            ),
            PORTICO_SOURCE_ROOT
            / "GeneratedV5Live/"
            "SM_IstanaPublicViewV7_CentralPorticoRefinement_V5Live.obj": (
                1220068,
                "164624CABCF0B093A7D53F87F0A3ACA5B009A001ABCA9CBD3950ECFC6336FFFD",
            ),
            PORTICO_SOURCE_ROOT
            / "GeneratedV5Live/"
            "SM_IstanaPublicViewV7_CentralPorticoRefinement_V5Live.mtl": (
                1372,
                "F58223E1AB2676FE7EEC10D4C07BC3CB75D5FADDBC215D7B7F7F50EA2B9479A7",
            ),
        }
        for path, (byte_count, digest) in expected.items():
            with self.subTest(path=path.name):
                self.assertEqual(path.stat().st_size, byte_count)
                self.assertEqual(sha256(path), digest)
                self.assertIn(digest, self.all_source)
        self.assertIn("ValidateExactFile", self.editor_cpp)
        for marker in (
            "namespace TriadExploreV3Sha256",
            "class FStreamingSha256 final",
            "VerifyKnownVectors()",
            "Sha256ImplementationIsValid()",
            "Candidate.ToUpperInline();",
            "constexpr int64 ReadChunkBytes = 1024 * 1024;",
            "CreateFileReader(*Filename, FILEREAD_Silent)",
            "Reader->Serialize(Buffer.GetData(), ThisChunk);",
            "OutSha256 = MoveTemp(Digest);",
            "OutBytes = TotalBytes;",
            "e3b0c44298fc1c149afbf4c8996fb924",
            "ba7816bf8f01cfea414140de5dae2223",
            "248d6a61d20638b8e5c026930c3e6039",
            "c26032d5154f96bd29c799447d715ab6",
        ):
            self.assertIn(marker, self.editor_cpp)
        self.assertNotIn("FPlatformMisc::GetSHA256Signature", self.editor_cpp)
        self.assertNotIn("FSHA256Signature", self.editor_cpp)
        hash_file = self.editor_cpp[
            self.editor_cpp.index("bool HashFileSha256(") :
            self.editor_cpp.index("bool ValidateExactFile(")
        ]
        self.assertNotIn("FFileHelper::LoadFileToArray", hash_file)
        self.assertNotIn("MAX_uint32", hash_file)
        self.assertIn("OutSha256.Reset();\n    OutBytes = -1;", hash_file)
        self.assertLess(
            hash_file.index("if (!FinishSha256(Hasher, Digest))"),
            hash_file.index("OutSha256 = MoveTemp(Digest);"),
        )
        constants = re.search(
            r"std::array<std::uint32_t, 64> Constants = \{(.*?)\};",
            self.editor_cpp,
            re.DOTALL,
        )
        self.assertIsNotNone(constants)
        self.assertEqual(
            len(re.findall(r"0x[0-9a-f]{8}u", constants.group(1))),
            64,
        )
        self.assertIn(
            "for (std::size_t ChunkIndex = 0u; ChunkIndex < 1024u; ++ChunkIndex)",
            self.editor_cpp,
        )
        self.assertIn(
            "std::array<std::uint8_t, 17> LargeTail{};", self.editor_cpp
        )
        self.assertEqual(
            hashlib.sha256(b"a" * (1024 * 1024 + 17)).hexdigest(),
            "c26032d5154f96bd29c799447d715ab681d8d0aa308ecc6f321a35d98f0672da",
        )
        portico_obj = next(
            path for path in expected if path.suffix.lower() == ".obj"
        )
        self.assertGreater(portico_obj.stat().st_size, 1024 * 1024)

        runtime_frozen_paths = set(expected)
        runtime_frozen_paths.update(
            {
                V3_SOURCE_ROOT / "explore_v3.contract.json",
                REPO
                / "unreal/SourceAssets/IstanaPublicViewExploreV3Unreal/"
                "explore_v3_unreal_integration.contract.json",
            }
        )

        island_lock_path = V3_SOURCE_ROOT / (
            "Sources/polyhaven-assets/island_tree_01/"
            "island_tree_01_1k.lock.json"
        )
        additional_lock_path = V3_SOURCE_ROOT / (
            "Sources/polyhaven-assets/polyhaven_additional_1k.lock.json"
        )
        runtime_frozen_paths.update({island_lock_path, additional_lock_path})
        island_lock = json.loads(text(island_lock_path))
        runtime_frozen_paths.update(
            island_lock_path.parent / row["path"] for row in island_lock["files"]
        )
        additional_lock = json.loads(text(additional_lock_path))
        runtime_frozen_paths.update(
            additional_lock_path.parent / row["path"]
            for asset in additional_lock["assets"]
            for row in asset["files"]
        )
        runtime_frozen_paths.update(
            PREPARED_MANIFEST.parent / row["path"]
            for row in self.prepared_manifest["files"]
        )
        context_manifest_path = V3_SOURCE_ROOT / (
            "Generated/GeospatialContext/geospatial_context.manifest.json"
        )
        context_manifest = json.loads(text(context_manifest_path))
        runtime_frozen_paths.update(
            context_manifest_path.parent / row["path"]
            for row in context_manifest["outputs"]
        )
        context_lock_path = (
            V3_SOURCE_ROOT / "geospatial_context.acceptance.lock.json"
        )
        context_lock = json.loads(text(context_lock_path))
        runtime_frozen_paths.update(
            context_manifest_path.parent / row["path"]
            for row in context_lock["acceptedOutputs"]
        )
        portico_manifest_path = PORTICO_SOURCE_ROOT / (
            "GeneratedV5Live/"
            "IstanaPublicViewV7CentralPorticoRefinementV5Live.manifest.json"
        )
        portico_manifest = json.loads(text(portico_manifest_path))
        runtime_frozen_paths.update(
            portico_manifest_path.parent / row["path"]
            for row in portico_manifest["files"]
        )
        self.assertGreaterEqual(len(runtime_frozen_paths), 60)
        for path in runtime_frozen_paths:
            self.assertTrue(path.is_file(), path)

        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("A standalone C++ compiler is unavailable on this host.")
        begin_marker = "// TRIAD_EXPLORE_V3_SHA256_HOST_BEGIN"
        end_marker = "// TRIAD_EXPLORE_V3_SHA256_HOST_END"
        begin = self.editor_cpp.index(begin_marker) + len(begin_marker)
        end = self.editor_cpp.index(end_marker, begin)
        helper = self.editor_cpp[begin:end]
        harness = """
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
""" + helper + r"""
int main(int ArgumentCount, char** Arguments)
{
    using namespace TriadExploreV3Sha256;
    if (!VerifyKnownVectors())
    {
        return 1;
    }
    for (int ArgumentIndex = 1;
         ArgumentIndex < ArgumentCount;
         ++ArgumentIndex)
    {
        std::ifstream Input(Arguments[ArgumentIndex], std::ios::binary);
        if (!Input)
        {
            return 2;
        }
        FStreamingSha256 Hasher;
        std::array<std::uint8_t, 64 * 1024> Buffer{};
        while (Input)
        {
            Input.read(
                reinterpret_cast<char*>(Buffer.data()),
                static_cast<std::streamsize>(Buffer.size()));
            const std::streamsize ByteCount = Input.gcount();
            if (ByteCount > 0 &&
                !Hasher.Update(
                    Buffer.data(), static_cast<std::size_t>(ByteCount)))
            {
                return 3;
            }
        }
        if (!Input.eof())
        {
            return 4;
        }
        FDigest Digest{};
        if (!Hasher.Finalize(Digest))
        {
            return 5;
        }
        const FLowerHexDigest Hex = ToLowerHex(Digest);
        std::cout.write(Hex.data(), 64);
        std::cout.put('\n');
    }
    return 0;
}
"""
        ordered_frozen_paths = sorted(runtime_frozen_paths)
        with tempfile.TemporaryDirectory(prefix="triad_explore_v3_sha256_") as temp:
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
            exercised = subprocess.run(
                [str(executable), *map(str, ordered_frozen_paths)],
                check=False,
                capture_output=True,
                text=True,
                timeout=120,
            )
            self.assertEqual(
                0,
                exercised.returncode,
                f"{exercised.stdout}\n{exercised.stderr}",
            )
            self.assertEqual(
                [sha256(path).lower() for path in ordered_frozen_paths],
                exercised.stdout.splitlines(),
            )

        prepared_files = {
            row["path"]: row for row in self.prepared_manifest["files"]
        }
        turf = prepared_files["SM_IPVExploreV3_CloseTurfCards.obj"]
        self.assertEqual(turf["bytes"], 3057)
        self.assertEqual(
            turf["sha256"].upper(),
            "D97FE4289F90B924E575D8D7C5C263021BDC447DA87141962570ED115BD1429D",
        )

    def test_prepared_texture_colour_space_and_normal_green_flip_are_exact(self) -> None:
        texture_policy = self.editor_cpp[
            self.editor_cpp.index("FTextureSourceSpec MakeTextureSpec(") :
            self.editor_cpp.index("bool AddLockedFileRoster(")
        ]
        self.assertIn('Lower.Contains(TEXT("diff"))', texture_policy)
        self.assertIn('Lower.EndsWith(TEXT("_color"))', texture_policy)
        self.assertIn('Lower.Contains(TEXT("nor_gl"))', texture_policy)
        self.assertIn('Lower.Contains(TEXT("normalgl"))', texture_policy)
        self.assertIn("Spec.Compression = TC_Normalmap", texture_policy)
        self.assertIn("Spec.bFlipGreen = true", texture_policy)
        self.assertGreaterEqual(texture_policy.count("Spec.bSrgb = false"), 2)
        self.assertIn("Spec.Compression = TC_Masks", texture_policy)
        for linear_map in (
            "rough",
            "alpha",
            "mask",
            "opacity",
            "ambientocclusion",
            "displacement",
            "scattering",
        ):
            self.assertIn(f'Lower.Contains(TEXT("{linear_map}"))', texture_policy)
        for marker in (
            "Texture->SRGB != Spec.bSrgb",
            "Texture->CompressionSettings != Spec.Compression",
            "Texture->bFlipGreenChannel != Spec.bFlipGreen",
            "Factory->bFlipNormalMapGreenChannel = Spec.bFlipGreen",
        ):
            self.assertIn(marker, self.editor_cpp)
        self.assertEqual(len(self.prepared_manifest["files"]), 13)
        self.assertEqual(
            sum(
                row["path"].endswith(("_NormalGL.jpg", "_NormalGL.png"))
                for row in self.prepared_manifest["files"]
            ),
            2,
        )

    def test_ue55_import_census_is_distinct_from_catalog_polycount(self) -> None:
        integration_contract_path = REPO / (
            "unreal/SourceAssets/IstanaPublicViewExploreV3Unreal/"
            "explore_v3_unreal_integration.contract.json"
        )
        integration = json.loads(text(integration_contract_path))
        census = integration["ue55MeshImportCensus"]
        self.assertEqual(
            "UStaticMesh::GetNumTriangles(0)",
            census["triangleMeasurementApi"],
        )
        expected = {
            "SM_IPVExploreV3_IslandTree01": (3_729_692, 812_532, 1_599_403),
            "SM_IPVExploreV3_Shrub02": (52_317, 13_834, 27_254),
            "SM_IPVExploreV3_Fern02": (6_232, 3_116, 6_232),
            "SM_IPVExploreV3_Moss01": (246_170, 138, 204),
            "SM_IPVExploreV3_BermudaGrass": (223_596, 552, 941),
        }
        self.assertEqual(
            expected,
            {
                row["assetName"]: (
                    row["catalogPolycount"],
                    row["binaryFbxPolygonCount"],
                    row["importedLod0Triangles"],
                )
                for row in census["fbxMeshes"]
            },
        )
        spec_block = self.editor_cpp[
            self.editor_cpp.index("struct FMeshImportSpec") :
            self.editor_cpp.index("bool ValidateMeshSourceAndCollision(")
        ]
        for catalog, polygon_count, imported_triangles in expected.values():
            self.assertRegex(
                spec_block,
                rf"{catalog},\s*{polygon_count},\s*{imported_triangles}",
            )
        for marker in (
            "int32 ExpectedImportedLod0Triangles;",
            "Mesh->GetNumTriangles(LodIndex)",
            "ValidateUe55MeshImportCensus",
            "ENGINE_MAJOR_VERSION != 5",
            "ENGINE_MINOR_VERSION != 5",
            "ValidateMeshImportTaskPolicy(Task, Spec, OutError)",
            "Spec.ExpectedImportedLod0Triangles",
            "expected %d from the UE5.5 contract, actual %d",
        ):
            self.assertIn(marker, self.editor_cpp)
        self.assertNotIn("int32 Lod0Triangles;", self.editor_cpp)
        self.assertNotIn("Spec.Lod0Triangles", self.editor_cpp)
        for label, imported_triangles in (
            ("Island", 1_599_403),
            ("Shrub", 27_254),
            ("Fern", 6_232),
            ("Moss", 204),
            ("Bermuda", 941),
        ):
            self.assertIn(
                f"Expected{label}ImportedLod0Triangles = {imported_triangles}",
                self.runtime_cpp,
            )

    def test_raw_high_polygon_lod_zero_is_never_runtime_selectable(self) -> None:
        for marker in (
            "The upstream 3,729,692 catalog polycount is provenance only",
            "UE5.5 imported Island Tree LOD0 is 1,599,403 render triangles",
            "IslandTree01Instances->bOverrideMinLOD = true",
            "IslandTree01Instances->MinLOD = 1",
            "Moss01Instances->bOverrideMinLOD = true",
            "Moss01Instances->MinLOD = 1",
            "BermudaGrassInstances->bOverrideMinLOD = true",
            "BermudaGrassInstances->MinLOD = 1",
            "IslandMesh->GetMinLODIdx() < 1",
            "MossMesh->GetMinLODIdx() < 1",
            "BermudaMesh->GetMinLODIdx() < 1",
        ):
            self.assertIn(marker, self.runtime_cpp)
        for lod_index, cap in ((1, 150000), (2, 60000), (3, 20000)):
            self.assertRegex(
                self.runtime_cpp,
                rf"CountTriangles\(IslandMesh, {lod_index}\).*?> {cap}",
            )
        for mesh, caps in (("MossMesh", (20000, 6000)), ("BermudaMesh", (12000, 3000))):
            for lod_index, cap in enumerate(caps, start=1):
                self.assertRegex(
                    self.runtime_cpp,
                    rf"CountTriangles\({mesh}, {lod_index}\).*?> {cap}",
                )
        for marker in (
            "ConfigureRuntimeLods",
            "Reduction.MaxNumOfTriangles = Spec.MaximumTriangles",
            "Mesh->SetMinLODIdx(RuntimeMinLod)",
            "Triangles >= PreviousTriangles",
            "Options->StaticMeshImportData->bBuildNanite = false",
        ):
            self.assertIn(marker, self.editor_cpp)

    def test_visual_vegetation_is_no_collision_and_blockers_are_pawn_only(self) -> None:
        visual_configuration = self.runtime_cpp[
            self.runtime_cpp.index("void ATRIADIstanaExploreV3SupplementActor::ConfigureVisualHism(") :
            self.runtime_cpp.index("double ATRIADIstanaExploreV3SupplementActor::TerrainHeightMeters(")
        ]
        self.assertGreaterEqual(
            visual_configuration.count(
                "SetCollisionEnabled(ECollisionEnabled::NoCollision)"
            ),
            2,
        )
        self.assertIn("SetCollisionResponseToAllChannels(ECR_Ignore)", visual_configuration)
        blocker_configuration = self.runtime_cpp[
            self.runtime_cpp.index(
                "SupplementalTreePawnBlockers->SetCollisionEnabled("
            ) : self.runtime_cpp.index(
                "PorticoV7RenderOnlyComponent = CreateDefaultSubobject"
            )
        ]
        for marker in (
            "ECollisionEnabled::QueryOnly",
            "SetCollisionResponseToAllChannels(ECR_Ignore)",
            "SetCollisionResponseToChannel(ECC_Pawn, ECR_Block)",
            "SetVisibility(false, true)",
            "SetHiddenInGame(true, true)",
        ):
            self.assertIn(marker, blocker_configuration)
        blocker_validation = self.runtime_cpp[
            self.runtime_cpp.index(
                "FCollisionResponseContainer ExpectedPawnOnlyResponses(ECR_Ignore)"
            ) : self.runtime_cpp.index(
                "if (!HasExactMeshPath(PorticoV7RenderOnlyComponent"
            )
        ]
        for marker in (
            "ExpectedPawnOnlyResponses.SetResponse(ECC_Pawn, ECR_Block)",
            "SupplementalTreePawnBlockers->GetCollisionResponseToChannels() !=",
            "ExpectedPawnOnlyResponses",
        ):
            self.assertIn(marker, blocker_validation)
        self.assertIn("Body->RemoveSimpleCollision()", self.editor_cpp)
        self.assertIn("bAutoGenerateCollision = false", self.editor_cpp)

    def test_dark_columnar_replacement_is_exactly_232_preserved_transforms(self) -> None:
        for marker in (
            "DarkColumnarBroadleafInstances",
            "InheritedV2ColumnarWorldTransforms",
            "PreservedV2ColumnarWorldTransforms",
            "bV2ColumnarVisualsReplacedWithExactTransforms",
            "ColumnarBroadleafInstances",
            "M_IPVExploreV3_ColumnarLeavesDark_Wind",
            "M_IPVExploreV2_BroadleafTrunk_Wind",
            "M_IPVExploreV2_BroadleafBranches_Wind",
            "SM_IstanaPublicViewExploreV1_Broadleaf_A",
        ):
            self.assertIn(marker, self.all_source)
        self.assertRegex(
            self.runtime_cpp,
            r"constexpr int32 Expected(?:Dark)?Columnar(?:Broadleaf)?Count = 232;",
        )
        self.assertRegex(
            self.runtime_cpp,
            r"InheritedV2ColumnarWorldTransforms\.Num\(\)\s*!=\s*"
            r"Expected(?:Dark)?Columnar(?:Broadleaf)?Count",
        )
        self.assertIn("AddInstance", self.runtime_cpp)
        self.assertIn("GetInstanceTransform", self.runtime_cpp)
        self.assertIn("Equals", self.runtime_cpp)

        # The V2 HISM remains present in the duplicated map: only its target-map
        # visibility is changed, while the replacement receives its world transforms.
        self.assertRegex(
            self.editor_cpp,
            r"ColumnarBroadleafInstances->SetVisibility\(false, true\)",
        )
        self.assertRegex(
            self.editor_cpp,
            r"ColumnarBroadleafInstances->SetHiddenInGame\(true, true\)",
        )
        self.assertRegex(
            self.editor_cpp,
            r"PopulateDeterministicSupplement\([^;]*Columnar[^;]*,\s*[^;]*\)",
        )
        self.assertIn("NoCollision", self.runtime_cpp)
        self.assertIn("Pawn-only", self.runtime_cpp)

    def test_dark_columnar_material_starts_clean_and_has_exactly_one_v3_wpo(self) -> None:
        dark_builder = self.editor_cpp[
            self.editor_cpp.index("UMaterial* CreateDarkColumnarLeafMaterial(") :
            self.editor_cpp.index("bool CreateAllMaterials(")
        ]
        source_validator = self.editor_cpp[
            self.editor_cpp.index("bool ValidateDarkColumnarSourceTextures(") :
            self.editor_cpp.index("FString TextureObjectPath(")
        ]
        dark_validator = self.editor_cpp[
            self.editor_cpp.index("bool ValidateDarkColumnarLeafMaterial(") :
            self.editor_cpp.index("UMaterial* CreateVegetationMaterial(")
        ]
        wind_builder = self.editor_cpp[
            self.editor_cpp.index("bool AddInstanceLocalWindGraph(") :
            self.editor_cpp.index("bool ValidateWindMaterial(")
        ]

        def construction_errors(dark: str) -> list[str]:
            errors: list[str] = []
            if "AssetTools.CreateAsset(" not in dark:
                errors.append("not-created-from-fresh-v3-material")
            if any(
                forbidden in dark
                for forbidden in (
                    "DuplicateAsset(",
                    "V2LeafWindMaterialPath",
                    "ProtectedV2Leaf",
                    "LoadExact<UMaterial>(",
                )
            ):
                errors.append("duplicate-or-second-wpo-path")
            if "!EditorOnly->ExpressionCollection.Expressions.IsEmpty()" not in dark:
                errors.append("fresh-graph-precondition-absent")
            if dark.count("AddInstanceLocalWindGraph(") != 1:
                errors.append("fresh-builder-does-not-add-exactly-one-wpo")
            return errors

        self.assertEqual([], construction_errors(dark_builder))
        self.assertEqual(source_validator.count("LoadExact<UTexture2D>("), 4)

        # Mutation checks make this gate non-vacuous: both the original P0
        # duplicate path and a second WPO insertion must be detected.
        duplicate_mutation = dark_builder.replace(
            "UMaterialFactoryNew* Factory",
            "UMaterial* Old = LoadExact<UMaterial>(V2LeafWindMaterialPath);\n"
            "    AssetTools.DuplicateAsset(/* inherited WPO */);\n"
            "    UMaterialFactoryNew* Factory",
            1,
        )
        self.assertIn(
            "duplicate-or-second-wpo-path",
            construction_errors(duplicate_mutation),
        )
        double_wpo_mutation = dark_builder + (
            "\nif (!AddInstanceLocalWindGraph(Material, Spec, OutError)) "
            "{ return nullptr; }\n"
        )
        self.assertIn(
            "fresh-builder-does-not-add-exactly-one-wpo",
            construction_errors(double_wpo_mutation),
        )

        for marker in (
            "V1LeafDiffuseTexturePath",
            "V1LeafNormalTexturePath",
            "V1LeafRoughnessTexturePath",
            "V1LeafOpacityTexturePath",
            "ValidateDarkColumnarSourceTextures",
            "ValidateDarkColumnarLeafMaterial",
            "InheritedV2WindNodes != 0",
            "V3WindNodes != 1",
            "Expressions.Num() != 16",
            "ConnectedWind != ExactWindNode",
            "IsCompilingOrHadCompileError(ERHIFeatureLevel::SM5)",
        ):
            self.assertIn(marker, self.editor_cpp)
        self.assertIn("EditorOnly->WorldPositionOffset.Expression", wind_builder)
        self.assertIn(
            'TEXT("V3 wind graph requires one material with no prior WPO.")',
            wind_builder,
        )

    def test_generated_context_is_hidden_diagnostic_and_v2_context_remains_authority(self) -> None:
        constructor = self.runtime_cpp[
            self.runtime_cpp.index("ATRIADIstanaExploreV3SupplementActor::ATRIADIstanaExploreV3SupplementActor()") :
            self.runtime_cpp.index("void ATRIADIstanaExploreV3SupplementActor::ConfigureVisualHism(")
        ]
        self.assertIn("OsmPublicRoadsComponent", constructor)
        self.assertIn("UraIndicativeRoadsComponent", constructor)
        self.assertIn("LowFrequencyTerrainComponent.Get()", constructor)
        self.assertIn("OsmPublicRoadsComponent.Get()", constructor)
        self.assertIn("UraIndicativeRoadsComponent.Get()", constructor)
        self.assertIn("OsmWaterComponent.Get()", constructor)
        self.assertIn("Component->SetVisibility(false, true)", constructor)
        self.assertIn("Component->SetHiddenInGame(true, true)", constructor)
        for marker in (
            "LowFrequencyTerrainComponent->IsVisible()",
            "!LowFrequencyTerrainComponent->bHiddenInGame",
            "OsmPublicRoadsComponent->IsVisible()",
            "!OsmPublicRoadsComponent->bHiddenInGame",
            "UraIndicativeRoadsComponent->IsVisible()",
            "!UraIndicativeRoadsComponent->bHiddenInGame",
            "OsmWaterComponent->IsVisible()",
            "!OsmWaterComponent->bHiddenInGame",
            "hidden diagnostics",
            "V2 free-roam, terrain/collision",
        ):
            self.assertIn(marker, self.runtime_cpp)

    def test_target_only_lawn_upgrade_is_visible_bounded_and_preserves_collision(self) -> None:
        for marker in (
            "ExpectedNearTurfCount = 18432",
            "ExpectedNearTurfCount * 8 != 147456",
            "Random.FRandRange(0.50f, 1.00f)",
            "bBuildingOrApron",
            "bEntranceWalk",
            "bFountainOrDrive",
            "TurfEndCull != 10000",
        ):
            self.assertIn(marker, self.runtime_cpp)
        for marker in (
            "FormalLawnMaterialPath",
            "Scene->TerrainComponent->SetMaterial(0, FormalLawn)",
            "V2Landscape->NearTurfInstances->SetVisibility(false, true)",
            "V2Landscape->NearTurfInstances->SetHiddenInGame(true, true)",
            "V2Landscape->MeadowSedgeInstances->SetVisibility(false, true)",
            "V2Landscape->MeadowSedgeInstances->SetHiddenInGame(true, true)",
            "SourceSnapshot.Terrain",
            "ExpectedTerrain.MaterialPaths[0] = FormalLawnMaterialPath",
            "terrain mesh/transform/QueryAndPhysics collision",
        ):
            self.assertIn(marker, self.editor_cpp)

    def test_reload_recomputes_bindings_and_pie_accepts_only_exact_mid_parents(self) -> None:
        for marker in (
            "virtual void PostLoad() override",
            "RefreshSavedAssetRuntimeBindings",
            "ExpectedRuntimeWindMidCount",
            "ColumnarTrunkMaterialSlot",
            "ColumnarBranchMaterialSlot",
            "ColumnarLeafMaterialSlot",
        ):
            self.assertIn(marker, self.runtime_h + self.runtime_cpp)
        self.assertIn("HasExactMaterialOrRuntimeMidParent", self.runtime_cpp)
        self.assertIn("Mid->Parent->GetPathName() == ExactPath", self.runtime_cpp)
        self.assertIn("ImportedSlotHasExactMaterialOrMidParent", self.runtime_cpp)

    def test_wind_defaults_and_hism_rebuild_fail_closed(self) -> None:
        for marker in (
            "BaseWindStrengthCm, 5.0f",
            "GustPeakStrengthCm, 46.0f",
            "WindSpeed, 1.42f",
            "RecoveryFrequencyHz, 0.62f",
            "RecoveryDampingRatio, 0.30f",
            "WindStrengthVelocityCmPerSecond = 0.0f",
            "WindDirectionVelocityPerSecond = FVector2D::ZeroVector",
            "BuildTreeIfOutdated(false, true)",
        ):
            self.assertIn(marker, self.runtime_cpp)
        self.assertIn("Scene->OSMContextBuildingsComponent", self.editor_cpp)
        self.assertIn("MatchesComponentState", self.editor_cpp)

    def test_unexported_per_instance_random_uses_v2_proven_custom_hlsl(self) -> None:
        self.assertIn(
            'TEXT("float InstanceRandom = GetPerInstanceRandom(Parameters);\\n")',
            self.editor_cpp,
        )
        for unsupported_link_reference in (
            '#include "Materials/MaterialExpressionPerInstanceRandom.h"',
            "UMaterialExpressionPerInstanceRandom",
            "AddMaterialExpression<UMaterialExpressionPerInstanceRandom>",
            "Cast<UMaterialExpressionPerInstanceRandom>",
            "PerInstanceRandomExpressionClassPath",
            "AddPerInstanceRandomExpression",
        ):
            self.assertNotIn(unsupported_link_reference, self.editor_cpp)

    def test_every_v3_material_graph_is_exact_and_shader_ready(self) -> None:
        for marker in (
            "ValidateVegetationMaterial",
            "ExpectedExpressionCount = 13",
            "Wind->Inputs.Num() == 9",
            "ScalarNodes != 5",
            "VectorNodes != 1",
            "Strength->DefaultValue, 5.0f",
            "Speed->DefaultValue, 1.42f",
            "Height->DefaultValue, Spec.HeightCm",
            "Response->DefaultValue, Spec.ResponseScale",
            "MaximumWpo->DefaultValue, Spec.MaximumWpoCm",
            "EditorOnly->WorldPositionOffset.Expression != Wind",
            "ValidateFormalLawnMaterial",
            "EditorOnly->ExpressionCollection.Expressions.Num() != 5",
            "ValidateSimpleMaterial",
            "EditorOnly->ExpressionCollection.Expressions.Num() != 3",
            "IsCompilingOrHadCompileError(ERHIFeatureLevel::SM5)",
        ):
            self.assertIn(marker, self.editor_cpp)
        validate_assets = self.editor_cpp[
            self.editor_cpp.index("bool ValidateAllV3AssetsInternal(") :
            self.editor_cpp.index("bool ImportAllMeshes(")
        ]
        self.assertIn("ValidateVegetationMaterial(", validate_assets)
        self.assertIn("ValidateFormalLawnMaterial(", validate_assets)
        self.assertIn("ValidateSimpleMaterial(", validate_assets)

    def test_leaf_sources_and_imported_wind_pivot_bounds_are_exact(self) -> None:
        for digest in (
            "879EF43ACB3443E247A2D32713FEC86C",
            "C968C79960303470E124875A111BD5A4",
            "4CBFFD1596F7AD63DE8E170AC54FB907",
            "740214DE7B900E30737009FF4EE2C01A",
            "B0F96EEEB90824FBA7594EC3185A2EE2094290D13C3F9497E85FAA085BCB8CC2",
        ):
            self.assertIn(digest, self.editor_cpp)
        for package in (
            "jacaranda_tree_leaves_diff_1k",
            "jacaranda_tree_leaves_nor_gl_1k",
            "jacaranda_tree_leaves_rough_1k",
            "jacaranda_tree_leaves_alpha_1k",
        ):
            self.assertGreaterEqual(self.editor_cpp.count(package), 3)
        for marker in (
            "CurrentHash == *ImportedHash",
            "LexToString(CurrentHash).Equals",
            "FPaths::IsSamePath(ImportedSource, ExpectedSource)",
            "IslandImportedUpAxisSpanCm = 502.744640f",
            "ShrubImportedUpAxisSpanCm = 138.346186f",
            "FernImportedUpAxisSpanCm = 42.769471f",
            "MossImportedUpAxisSpanCm = 4.036798f",
            "BermudaImportedUpAxisSpanCm = 15.825569f",
            "Binding.Mesh->GetBounds().BoxExtent.Z * 2.0",
            "WindSpec->HeightCm",
            "MossWindSpec->MaximumWpoCm > 0.8f",
            "BermudaWindSpec->MaximumWpoCm > 1.2f",
            "ValidateVegetationWindPivotBoundsContract",
            "FROZEN_BINARY_FBX_ABSOLUTE_MODEL_TRANSFORMS_PLUS_UE55_SCENE_AXIS_CONVERSION",
            "geometryToModelOneToOneOoLinks",
            "modelTransformPivotDeclarationsPresent",
            "TARGET_HEIGHT_CM / EXPECTED_IMPORTED_UP_AXIS_SPAN_CM",
            "bTransformVertexToAbsolute = true",
            "bBakePivotInVertex = false",
        ):
            self.assertIn(marker, self.editor_cpp)
        for marker in (
            "ExpectedBermudaImportedUpAxisSpanCm = 15.825569f",
            "MinimumBermudaPlacedHeightCm = 3.0f",
            "MaximumBermudaPlacedHeightCm = 6.0f",
            "TargetHeightCm /",
            "ExpectedBermudaImportedUpAxisSpanCm",
            "PlacedHeightCm = Transform.GetScale3D().Z *",
            "a Bermuda accent does not read back at the exact derived 3-6 cm placed height",
            "BermudaMesh->GetBounds().BoxExtent.Z * 2.0",
        ):
            self.assertIn(marker, self.runtime_cpp)
        self.assertNotIn("105.992782", self.all_source)
        self.assertNotIn("Random.FRandRange(0.032f, 0.058f)", self.runtime_cpp)
        self.assertNotIn("CurrentHash.ToString()", self.editor_cpp)

    def test_portico_is_identity_5816_triangle_render_only_sibling(self) -> None:
        for marker in (
            'GetIntegerField(TEXT("triangleCount")) != 5816',
            "!IsTriple(Location, 0.0)",
            "!IsTriple(Rotation, 0.0)",
            "!IsTriple(Scale, 1.0)",
            'GetBoolField(TEXT("collisionEnabled"))',
            'GetBoolField(TEXT("addAsSiblingOfFrozenV5Hero"))',
            'GetBoolField(TEXT("replaceFrozenV5Hero"))',
        ):
            self.assertIn(marker, self.editor_cpp)
        for marker in (
            "PorticoV7RenderOnlyComponent",
            "GetRelativeTransform().Equals(FTransform::Identity",
            "GetComponentTransform().Equals(FTransform::Identity",
            "GetCollisionEnabled() != ECollisionEnabled::NoCollision",
            "CountTriangles(PorticoV7RenderOnlyComponent->GetStaticMesh(), 0) != 5816",
            "PorticoImportedMaterialSpecs",
            "Component->GetNumMaterials() != Specs.Num()",
            "Mesh->GetStaticMaterials().Num() != Specs.Num()",
            "RenderData->LODResources[0].Sections.Num() != Specs.Num()",
            "Section.NumTriangles != Specs[MaterialIndex].TriangleFaces",
            'Mesh->GetMaterialIndex(TEXT("M_IPV7_Portico_Render")) != INDEX_NONE',
            'Mesh->GetMaterialIndex(TEXT("M_IPV7_Portico_Stone")) != INDEX_NONE',
        ):
            self.assertIn(marker, self.runtime_cpp)
        for marker in (
            "ValidatePorticoMaterialTopologyContract",
            "PorticoUe55ImportedSlotSpecs",
            "PorticoObjFaceAssignmentSpecs",
            "PorticoMtlDeclaredDefinitions",
            "PorticoStandaloneUnusedDefinitions",
            "ValidatePorticoMeshMaterialTopology",
            "if (!bImportedNameExact || !bDisplayNameExact)",
            "Mesh->GetStaticMaterials().Num() != Specs.Num()",
            "RenderData->LODResources[0].Sections.Num() != Specs.Num()",
            "Section.NumTriangles != Specs[MaterialIndex].TriangleFaces",
            "FStaticMeshCompilingManager::Get().FinishCompilation({Mesh})",
            "PorticoMtlSha",
        ):
            self.assertIn(marker, self.editor_cpp)
        self.assertNotIn(
            "V7 portico import must retain exactly eight material slots",
            self.editor_cpp,
        )
        self.assertNotIn(
            "V7 portico must retain exactly eight material slots",
            self.runtime_cpp,
        )

        topology = self.integration_contract["porticoAuthorization"][
            "materialTopology"
        ]
        self.assertEqual(8, topology["sourceMtlDeclaredDefinitionCount"])
        self.assertEqual(6, topology["objFaceAssignedMaterialCount"])
        self.assertEqual(6, topology["ue55ImportedSlotCount"])
        self.assertEqual(6, topology["ue55Lod0SectionCount"])
        self.assertEqual(2, topology["standaloneUnusedDefinitionCount"])
        self.assertEqual(
            [
                "M_IPV7_Portico_Soffit",
                "M_IPV7_Portico_Trim",
                "M_IPV7_Portico_Recess",
                "M_IPV7_Portico_Metal",
                "M_IPV7_Portico_Glass",
                "M_IPV7_Portico_Louvre",
            ],
            topology["ue55ImportedSlotOrder"],
        )
        self.assertEqual(
            ["M_IPV7_Portico_Render", "M_IPV7_Portico_Stone"],
            topology["standaloneUnusedDefinitions"],
        )
        self.assertFalse(topology["zeroFaceDefinitionsFabricatedAsSlots"])
        self.assertTrue(topology["allEightStandaloneMaterialAssetsRequired"])
        for material_name in topology["sourceMtlDeclaredDefinitions"]:
            self.assertIn(
                f'{{TEXT("{material_name}"), FLinearColor(',
                self.editor_cpp,
            )
        self.assertFalse(self.prepared_manifest["claimBoundary"]["oneToOneOneKilometreClaimed"])
        self.assertFalse(self.prepared_manifest["claimBoundary"]["sensorTruthAuthority"])

    def test_partial_outputs_fail_closed_and_prior_maps_are_byte_protected(self) -> None:
        for version in range(1, 7):
            self.assertIn(
                f'/Game/Maps/Istana_PublicView_Exterior_v{version}',
                self.editor_cpp,
            )
        self.assertIn('/Game/Maps/Istana_PublicView_Explore_v1', self.editor_cpp)
        self.assertIn('/Game/Maps/Istana_PublicView_Explore_v2', self.editor_cpp)
        for marker in (
            "CaptureProtectedPackages",
            "ValidateProtectedPackages",
            "FFileHelper::LoadFileToArray",
            "CurrentBytes != Record.Bytes",
            "GetAssetsByPath",
        ):
            self.assertIn(marker, self.editor_cpp)
        self.assertRegex(
            self.editor_cpp.lower(),
            r"(?:partial[^\n;]*(?:refus|fail)|(?:refus|fail)[^\n;]*partial)",
        )
        self.assertRegex(
            self.editor_cpp,
            r"(?:Expected\w*(?:Asset|Package)\w*\s*=\s*67|"
            r"(?:asset|package)[^\n]*exactly 67|exactly 67[^\n]*(?:asset|package))",
            msg="The complete 36-texture + 20-material + 11-mesh roster must be gated.",
        )
        mandatory_start = self.editor_cpp.index(
            "const TArray<FString>& ProtectedMapPackages()"
        )
        mandatory_end = self.editor_cpp.index(
            "const TArray<FString>& ProtectedV2AssetPackages()",
            mandatory_start,
        )
        mandatory = self.editor_cpp[mandatory_start:mandatory_end]
        self.assertNotIn("Istana_PublicView_Exterior_v6", mandatory)
        self.assertIn(
            'const FString OptionalV6(TEXT("/Game/Maps/Istana_PublicView_Exterior_v6"))',
            self.editor_cpp,
        )
        self.assertIn("FPackageName::DoesPackageExist(OptionalV6", self.editor_cpp)

    def test_claim_boundary_says_not_one_to_one_hyperreal_or_sensor_truth(self) -> None:
        for marker in (
            "bOneToOneOneKilometerClaimed = false",
            "bSurveyAccurateTerrainClaimed = false",
            "bExactBotanicalInventoryClaimed = false",
            "bSensorTruthAuthority = false",
            "bGoogleOrOneMapPixelsUsed = false",
            "bPorticoCollisionAuthority = false",
        ):
            self.assertIn(marker, self.runtime_h)
        self.assertIn("NOT_ONE_TO_ONE", self.runtime_h)
        self.assertIn("NOT_SENSOR_TRUTH", self.runtime_h)
        self.assertRegex(
            self.all_source.lower(),
            r"(?:not[_ -]?hyperreal|non[_ -]?hyperreal)",
        )
        self.assertIn(
            "SOURCE_ONLY_VISUAL_PROTOTYPE_NOT_SURVEY_OR_SENSOR_TRUTH",
            self.editor_cpp,
        )
        for false_claim in (
            "oneToOneOneKilometerClaimed",
            "sensorTruthAuthority",
            "surveyAccurateTerrainClaimed",
            "googleOrOneMapPixelsUsed",
        ):
            self.assertIn(f'GetBoolField(TEXT("{false_claim}"))', self.editor_cpp)

    def test_pie_gate_preserves_free_roam_and_live_wind(self) -> None:
        for marker in (
            "ValidateIstanaExploreV3PlayWorld",
            "ATRIADIstanaFreeRoamPawn",
            "UGameplayStatics::GetPlayerController",
            "GetViewTarget()",
            "HasExpectedExploreCameraProfile()",
            "IsWindRuntimeActive()",
        ):
            self.assertIn(marker, self.editor_cpp)
        self.assertIn(
            "/Script/TRIADSensorFusion.TRIADIstanaExploreGameMode",
            self.editor_cpp,
        )
        self.assertEqual(
            self.editor_cpp.count(
                "ValidatePublicViewScene(SceneReport, false)"
            ),
            2,
        )
        self.assertIn(
            "bool bRequireLegacyTreeInstances = true",
            self.scene_h,
        )
        self.assertIn(
            "bool bRequireLegacyTreeInstances) const",
            self.scene_cpp,
        )
        self.assertEqual(
            self.scene_cpp.count("if (bRequireLegacyTreeInstances &&"),
            2,
        )


if __name__ == "__main__":
    unittest.main()
