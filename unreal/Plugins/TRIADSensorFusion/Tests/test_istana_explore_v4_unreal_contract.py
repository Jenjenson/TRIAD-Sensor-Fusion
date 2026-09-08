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
EDITOR_H = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV4EditorLibrary.h"
)
EDITOR_CPP = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV4EditorLibrary.cpp"
)
EDITOR_BUILD = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/"
    "TRIADSensorFusionEditor.Build.cs"
)
RUNTIME_H = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV4LandscapeActor.h"
)
RUNTIME_CPP = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV4LandscapeActor.cpp"
)
V4_ROOT = REPO / "unreal/SourceAssets/IstanaPublicViewExploreV4"
V8_ROOT = REPO / "unreal/SourceAssets/IstanaPublicViewV8Portico"
V7_PORTICO_OBJ = REPO / (
    "unreal/SourceAssets/IstanaPublicViewV7Portico/GeneratedV5Live/"
    "SM_IstanaPublicViewV7_CentralPorticoRefinement_V5Live.obj"
)
VEGETATION_CONTRACT = V4_ROOT / "explore_v4_vegetation.contract.json"


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def ue55_legacy_obj_material_census(path: Path) -> tuple[list[str], list[int]]:
    """Mirror the accepted legacy OBJ group traversal used by UE5.5."""
    current_group: str | None = None
    current_material: str | None = None
    group_materials: dict[str, str] = {}
    triangles_by_material: dict[str, int] = {}
    for line in text(path).splitlines():
        if line.startswith("g "):
            current_group = line[2:]
        elif line.startswith("usemtl "):
            current_material = line[7:]
        elif line.startswith("f "):
            if current_group is None or current_material is None:
                raise AssertionError("OBJ face lacks a group or material.")
            previous = group_materials.setdefault(current_group, current_material)
            if previous != current_material:
                raise AssertionError("OBJ group changes material within one node.")
            triangles_by_material[current_material] = (
                triangles_by_material.get(current_material, 0) + 1
            )
    ordered_materials = list(
        dict.fromkeys(group_materials[group] for group in sorted(group_materials))
    )
    return ordered_materials, [
        triangles_by_material[material] for material in ordered_materials
    ]


class IstanaExploreV4UnrealEditorContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.editor_h = text(EDITOR_H)
        cls.editor_cpp = text(EDITOR_CPP)
        cls.editor_build = text(EDITOR_BUILD)
        cls.runtime_h = text(RUNTIME_H)
        cls.runtime_cpp = text(RUNTIME_CPP)
        cls.all_source = "\n".join(
            (cls.editor_h, cls.editor_cpp, cls.runtime_h, cls.runtime_cpp)
        )
        cls.vegetation_contract = json.loads(text(VEGETATION_CONTRACT))

    def test_editor_public_surface_is_complete_and_each_body_lands_once(self) -> None:
        functions = (
            "PrewarmIstanaExploreV4ProtectedReferencesForImport",
            "ImportIstanaExploreV4Assets",
            "BuildIstanaExploreV4Map",
            "ValidateIstanaExploreV4Assets",
            "ValidateIstanaExploreV4Map",
            "ValidateIstanaExploreV4PlayWorld",
            "CaptureIstanaExploreV4PlayView",
            "TriggerIstanaExploreV4PlayWindGust",
            "MoveIstanaExploreV4PlayPawnForQa",
            "TeleportIstanaExploreV4PlayPawnForQa",
            "GetIstanaExploreV4PlayStateReport",
            "QuiesceIstanaExploreV4PlayWorldForStop",
        )
        for name in functions:
            with self.subTest(name=name):
                self.assertIn(name, self.editor_h)
                self.assertEqual(
                    len(
                        re.findall(
                            rf"UTRIADIstanaExploreV4EditorLibrary::\s*{name}\s*\(",
                            self.editor_cpp,
                        )
                    ),
                    1,
                )
        self.assertEqual(self.editor_h.count("UFUNCTION("), len(functions))

    def test_project_source_helper_signature_cannot_degrade_to_naked_body(self) -> None:
        exact = "FString ProjectSourcePath(const TCHAR* Relative)\n{"
        self.assertEqual(self.editor_cpp.count(exact), 1)
        object_path_end = self.editor_cpp.index(
            "FString ObjectPath(const FString& PackagePath,"
        )
        helper = self.editor_cpp.index(exact, object_path_end)
        self.assertLess(object_path_end, helper)
        between = self.editor_cpp[
            self.editor_cpp.index("}\n", object_path_end) + 2 : helper
        ]
        self.assertNotRegex(between, r"\n\s*\{\s*\n")
        first_public = self.editor_cpp.index(
            "bool UTRIADIstanaExploreV4EditorLibrary::"
            "ImportIstanaExploreV4Assets("
        )
        last_internal = self.editor_cpp.index("bool GetValidatedV4PlayState(")
        self.assertGreater(first_public, last_internal)

    def test_ue55_editor_api_boundaries_are_explicit_and_const_correct(self) -> None:
        self.assertIn('#include "MaterialDomain.h"', self.editor_cpp)
        inheritance = self.editor_cpp[
            self.editor_cpp.index("bool ValidateTargetInheritance(") :
            self.editor_cpp.index("struct FFrozenRasterGrid")
        ]
        for marker in (
            "V3->DarkColumnarBroadleafInstances->GetInstanceCount()",
            "V3->AmbientCgNearTurfInstances->GetInstanceCount()",
        ):
            self.assertIn(marker, inheritance)
        self.assertNotRegex(
            inheritance,
            r"V3Live\[(?:1|6)\]->GetInstanceCount\(\)",
        )
        self.assertNotIn("const UMetaData*", self.editor_cpp)
        self.assertGreaterEqual(self.editor_cpp.count("UMetaData*"), 4)

    def test_final_frozen_contracts_and_mesh_inputs_are_byte_exact(self) -> None:
        frozen = {
            V4_ROOT / "explore_v4_geospatial.contract.json": (
                16706,
                "CCD9B5200E03602EC6FA6986F4D3AB70920806C63D718B5B1A943CC29F30D8E7",
            ),
            V4_ROOT / "Prepared/v4_raster_sample_grid.json": (
                170691,
                "262C88795C3C5FB1BB92B2D1CD2D1886B25D5E3885E60F34E407082E4C81C447",
            ),
            V4_ROOT / "Sources/Geospatial/v4_public_vegetation_zoning.json": (
                11673,
                "3AA50AECDA04DDC68BC548EC17FAB0CC2292D05532390E51A523F2817E91645E",
            ),
            VEGETATION_CONTRACT: (
                117730,
                "ABDC65AA14DA9AE89736BBE2D75E6212FA38B76B44DBA491C9266DEBC247E104",
            ),
            V4_ROOT / "Sources/Vegetation/manifest.json": (
                329362,
                "87A48A09EE6C9C977C5865D9D6B821581A654E30F5095C52C2901E48D6E5B3C6",
            ),
            V8_ROOT
            / "Generated/IstanaPublicViewV8CentralPorticoDepthOverlayV5Live.manifest.json": (
                6310,
                "3E88B8C4AEF5A499696436920972301EFBDA242BAAA3F64928D1A8B072557943",
            ),
            V8_ROOT
            / "Generated/SM_IstanaPublicViewV8_CentralPorticoDepthOverlay_V5Live.obj": (
                1383806,
                "330B20E8F58289C84EF96CB031D374ADB3AAF52711E5247D1F167C7E94DC4526",
            ),
            V8_ROOT
            / "Generated/SM_IstanaPublicViewV8_CentralPorticoDepthOverlay_V5Live.mtl": (
                1218,
                "593E5721C2968236142E6C7C23E5AC7757722D2DF1E8D540C7E86A96D1369600",
            ),
        }
        for path, (size, digest) in frozen.items():
            with self.subTest(path=path.name):
                self.assertEqual(path.stat().st_size, size)
                self.assertEqual(sha256(path), digest)
                self.assertIn(digest, self.all_source)
        self.assertEqual(
            self.vegetation_contract["schema"],
            "triad.istana_explore_v4_vegetation_contract.v2",
        )
        freeze = self.vegetation_contract["sourceFreeze"]
        self.assertEqual(freeze["packageFileCount"], 103)
        self.assertEqual(freeze["packageBytes"], 406188006)
        self.assertEqual(len(freeze["packagedFiles"]), 103)

    def test_portable_streaming_sha256_has_vectors_and_file_regression(self) -> None:
        for marker in (
            "namespace TriadExploreV4Sha256",
            "class FPortableSha256 final",
            "VerifyKnownVectors()",
            "Sha256ImplementationIsValid()",
            "constexpr int64 ReadChunkBytes = 1024 * 1024;",
            "CreateFileReader(*Filename, FILEREAD_Silent)",
            "Reader->Serialize(Buffer.GetData(), ThisChunk);",
            "e3b0c44298fc1c149afbf4c8996fb924",
            "ba7816bf8f01cfea414140de5dae2223",
            "248d6a61d20638b8e5c026930c3e6039",
            "c26032d5154f96bd29c799447d715ab6",
        ):
            self.assertIn(marker, self.editor_cpp)
        self.assertNotIn("FPlatformMisc::GetSHA256Signature", self.editor_cpp)
        self.assertNotIn("FSHA256Signature", self.editor_cpp)
        file_wrapper = self.editor_cpp[
            self.editor_cpp.index("bool HashFileSha256(") :
            self.editor_cpp.index("bool ValidateExactFile(")
        ]
        self.assertNotIn("LoadFileToArray", file_wrapper)
        self.assertIn("OutSha.Reset();\n    OutBytes = -1;", file_wrapper)

        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("A standalone C++ compiler is unavailable.")
        begin = "// TRIAD_EXPLORE_V4_SHA256_HOST_BEGIN"
        end = "// TRIAD_EXPLORE_V4_SHA256_HOST_END"
        helper = self.editor_cpp[
            self.editor_cpp.index(begin) + len(begin) :
            self.editor_cpp.index(end)
        ]
        harness = r"""
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
""" + helper + r"""
int main(int argc, char** argv)
{
    using namespace TriadExploreV4Sha256;
    if (!VerifyKnownVectors() || argc != 2) return 1;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 2;
    FPortableSha256 hasher;
    std::array<std::uint8_t, 100003> buffer{};
    while (input)
    {
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        const auto count = input.gcount();
        if (count > 0 && !hasher.Update(
                buffer.data(), static_cast<std::size_t>(count))) return 3;
    }
    FDigest digest{};
    if (!hasher.Finalize(digest)) return 4;
    std::cout << ToLowerHex(digest).data();
    return 0;
}
"""
        payload = bytes(range(256)) * 4097 + b"V4-file-regression"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "sha.cpp"
            executable = root / "sha-test"
            fixture = root / "fixture.bin"
            source.write_text(harness, encoding="utf-8")
            fixture.write_bytes(payload)
            compile_result = subprocess.run(
                [compiler, "-std=c++17", str(source), "-o", str(executable)],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(compile_result.returncode, 0, compile_result.stderr)
            run = subprocess.run(
                [str(executable), str(fixture)],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertEqual(run.stdout, hashlib.sha256(payload).hexdigest())

    def test_additive_paths_and_protected_roots_are_exact(self) -> None:
        for marker in (
            'SourceMapPackage(TEXT("/Game/Maps/Istana_PublicView_Explore_v3"))',
            'DestinationMapPackage(\n    TEXT("/Game/Maps/Istana_PublicView_Explore_v4"))',
            'AssetRoot(TEXT("/Game/TRIAD/IstanaPublicViewExploreV4"))',
            'TEXT("/Game/Maps/Istana_PublicView_Exterior_v1")',
            'TEXT("/Game/Maps/Istana_PublicView_Exterior_v5")',
            'TEXT("/Game/Maps/Istana_PublicView_Explore_v1")',
            'TEXT("/Game/TRIAD/IstanaPublicViewExploreV1")',
            'TEXT("/Game/TRIAD/IstanaPublicViewExploreV2")',
            'TEXT("/Game/TRIAD/IstanaPublicViewExploreV3")',
            "FEditorFileUtils::LoadMap(",
            "FPackageName::IsTempPackage",
            "UEditorLoadingAndSavingUtils::SaveMap(",
            "CaptureProtectedPackages",
            "ValidateProtectedPackages",
            "Protected V1-V5/Explore V1-V3/HDB/OSM size/SHA changed",
        ):
            self.assertIn(marker, self.editor_cpp)
        self.assertNotIn("bReplaceExisting = true", self.editor_cpp)
        self.assertNotIn("DeleteDirectory", self.editor_cpp)

    def test_asset_import_is_all_or_none_idempotent_and_never_repairs_partial(self) -> None:
        for marker in (
            "CountAssetsUnderV4Root() != 85",
            "Existing == 85 &&",
            "ValidateAllV4AssetsInternal(false, ExistingError)",
            "IDEMPOTENT_EXPLORE_V4_ASSETS_ALREADY_VALID",
            "EXPLORE_V4_IMPORT_REFUSED_PARTIAL_DESTINATION",
            "No overwrite or partial repair is authorized",
            "RollBackNewV4Namespace",
            "EXPLORE_V4_SAVE_FAILED_AND_ROLLED_BACK",
            "AssetsToSave.Num() != 78",
            "LoadedObjectPaths.Num() != 78",
            "SealedStagingObjectPaths.Num() != 7",
            "CombinedOwnedObjectPaths.Num() != 85",
            "SaveLoadedAssets(AssetsToSave, false)",
            "UPackageTools::UnloadPackages",
            "PersistedObjectPaths.Num() != 85",
            "UniquePackagesToReload.Num() != 78",
            "CombinedOwnedPackageNames.Num() != 85",
        ):
            self.assertIn(marker, self.editor_cpp)
        rollback = self.editor_cpp[
            self.editor_cpp.index("bool RollBackNewV4Namespace(") :
            self.editor_cpp.index("bool BuildRuntimeAssetRosterAndWindBindings(")
        ]
        self.assertIn("StartsWith(AssetRoot + TEXT(\"/\"))", rollback)
        self.assertIn("DeleteObjectsUnchecked", rollback)
        loaded_delete = rollback.index(
            "DeleteObjectsUnchecked(AlreadyLoadedObjects)"
        )
        unloaded_loop = rollback.index(
            "for (const FAssetData& Data : UnloadedAssetData)"
        )
        one_asset_load = rollback.index("Data.GetAsset()")
        self.assertLess(loaded_delete, unloaded_loop)
        self.assertLess(unloaded_loop, one_asset_load)
        self.assertNotIn("Data.GetAsset()", rollback[:unloaded_loop])
        self.assertEqual(1, rollback.count("Data.GetAsset()"))
        self.assertNotIn("DeleteDirectory", rollback)
        self.assertNotIn("LoadAsset(", rollback)

    def test_protected_prewarm_is_exact_clean_streamed_and_precedes_creation(self) -> None:
        prewarm = self.editor_cpp[
            self.editor_cpp.index(
                "bool PrewarmAndUnloadProtectedMeshReferences("
            ) : self.editor_cpp.index(
                "bool IsProtectedRuntimeMeshReference("
            )
        ]
        for marker in (
            "ProtectedDerivativeSpecs().Num() != 2",
            "int32 PrewarmedCount = 0",
            "for (const FProtectedDerivativeSpec& Spec : ",
            "ProtectedDerivativeSpecs())",
            "FindObject<UStaticMesh>",
            "FindPackage(nullptr, *PackageName)",
            "ResolvePackageFileAndValidate(",
            "FinishCompilation({Source})",
            "FAssetCompilingManager::Get().FinishAllCompilation()",
            "GDistanceFieldAsyncQueue->BlockUntilBuildComplete(Source, true)",
            "GCardRepresentationAsyncQueue->BlockUntilBuildComplete(",
            "GetDerivedDataCacheRef().WaitForQuiescence(false)",
            "ValidateProtectedMeshTopology(Source, Spec, OutError)",
            "BuildProtectedSecondaryDerivedDataProof(",
            "ValidateProtectedSecondaryDerivedDataProof(",
            "Source->ClearMeshDescriptions()",
            "const auto ReleaseAndRevalidate =",
            "UnloadOneExactCleanPackage(",
            "FindPackage(nullptr, *PackageName)",
            "PrewarmedCount != 2",
            "ValidateProtectedPackages(ProtectedSnapshot, OutError)",
        ):
            self.assertIn(marker, prewarm)
        self.assertLess(
            prewarm.index("ResolvePackageFileAndValidate("),
            prewarm.index("LoadExact<UStaticMesh>(Spec.SourceObjectPath)"),
        )
        self.assertLess(
            prewarm.index("Source->ClearMeshDescriptions()"),
            prewarm.index("UnloadOneExactCleanPackage("),
        )
        self.assertGreaterEqual(
            prewarm.count("ResolvePackageFileAndValidate("), 2
        )
        prewarm_function = prewarm[
            : prewarm.index("struct FProtectedPrewarmReceiptFile")
        ]
        self.assertEqual(
            2,
            prewarm_function.count(
                "ValidateProtectedSecondaryDerivedDataProof("
            ),
        )
        self.assertLess(
            prewarm_function.index("FinishCompilation({Source})"),
            prewarm_function.index(
                "FAssetCompilingManager::Get().FinishAllCompilation()"
            ),
        )
        self.assertLess(
            prewarm_function.index(
                "GDistanceFieldAsyncQueue->BlockUntilBuildComplete"
            ),
            prewarm_function.index(
                "GCardRepresentationAsyncQueue->BlockUntilBuildComplete"
            ),
        )
        self.assertLess(
            prewarm_function.index(
                "GetDerivedDataCacheRef().WaitForQuiescence(false)"
            ),
            prewarm_function.index(
                "BuildProtectedSecondaryDerivedDataProof("
            ),
        )
        self.assertLess(
            prewarm_function.index(
                "BuildProtectedSecondaryDerivedDataProof("
            ),
            prewarm_function.index(
                "ReleaseAndRevalidate(FString(), OutError)"
            ),
        )
        for forbidden in (
            "AssetTools",
            "CreatePackage(",
            "DuplicateAsset(",
            "MarkPackageDirty(",
            "SaveLoadedAssets(",
            "bUnloadDirtyPackages = true",
        ):
            self.assertNotIn(forbidden, prewarm)
        self.assertIn(
            "return ReleaseAndRevalidate(Failure, OutError)", prewarm
        )
        self.assertIn(
            "ReleaseAndRevalidate(FString(), OutError)", prewarm
        )

        one_package_unload = self.editor_cpp[
            self.editor_cpp.index("bool UnloadOneExactCleanPackage(") :
            self.editor_cpp.index("bool ValidateProtectedMeshTopology(")
        ]
        for marker in (
            "Package->IsDirty()",
            "TArray<UPackage*> OnePackage = {Package}",
            "bUnloadDirtyPackages = false",
            "bResetTransBuffer = bResetTransBuffer",
            "UPackageTools::UnloadPackages(UnloadParams)",
            "FindObject<UObject>(nullptr, *ObjectPath)",
            "FindPackage(nullptr, *PackageName)",
            "FMemory::Trim(true)",
        ):
            self.assertIn(marker, one_package_unload)
        self.assertNotIn(
            "bUnloadDirtyPackages = true", one_package_unload
        )

        public_prewarm = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::\n"
                "    PrewarmIstanaExploreV4ProtectedReferencesForImport("
            ) : self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ImportIstanaExploreV4Assets("
            )
        ]
        for marker in (
            "HasDisallowedDirtyPackages(",
            "ValidateCleanEntryProcessBoundary(",
            "CountAssetsUnderV4Root() != 0",
            "ValidateFrozenPublicDataContracts(",
            "CaptureProtectedPackages(",
            "ValidateProtectedPrewarmSourcesUnloaded(",
            "PrewarmAndUnloadProtectedMeshReferences(",
            "CaptureCurrentProtectedPrewarmReceiptState(",
            "WriteProtectedPrewarmReceiptAtomically(",
            "ValidateProtectedPrewarmReceipt(",
            "FApp::GetInstanceId()",
            "FPlatformProcess::GetCurrentProcessId()",
        ):
            self.assertIn(marker, public_prewarm)
        self.assertEqual(
            1,
            public_prewarm.count(
                "PrewarmAndUnloadProtectedMeshReferences("
            ),
        )
        self.assertNotIn("ImportExternalMeshDerivatives(", public_prewarm)

        public_import = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ImportIstanaExploreV4Assets("
            ) : self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ValidateIstanaExploreV4Assets("
            )
        ]
        capture = public_import.index("CaptureProtectedPackages(")
        receipt = public_import.index("ValidateProtectedPrewarmReceipt(")
        asset_tools = public_import.index("IAssetTools& AssetTools")
        external = public_import.index("ImportExternalMeshDerivatives(")
        seal = public_import.index(
            "SealAndUnloadExactExternalStagingPackages("
        )
        textures = public_import.index("ImportRequiredTextures(")
        owned_small_meshes = public_import.index(
            "DuplicateCloseTurfAndBlocker("
        )
        collect = public_import.index(
            "CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS)"
        )
        cold_facts = public_import.index(
            "ValidateProtectedReferenceFactsWithoutLoading(true, Error)"
        )
        self.assertLess(capture, receipt)
        self.assertLess(receipt, asset_tools)
        self.assertLess(asset_tools, external)
        self.assertLess(external, seal)
        self.assertLess(seal, textures)
        self.assertLess(textures, owned_small_meshes)
        self.assertLess(owned_small_meshes, collect)
        self.assertLess(collect, cold_facts)
        self.assertNotIn("ResolveProtectedMeshReferences(", public_import)
        self.assertNotIn("RuntimeMeshes.Num() != 11", public_import)
        self.assertNotIn(
            "PrewarmAndUnloadProtectedMeshReferences(", public_import
        )

    def test_asset_registry_discovery_is_quiesced_before_census_and_staging_rename(self) -> None:
        helper = self.editor_cpp[
            self.editor_cpp.index(
                "bool EnsureAssetRegistryDiscoveryComplete("
            ) : self.editor_cpp.index("int32 CountAssetsUnderV4Root()")
        ]
        self.assertIn("IsLoadingAssets()", helper)
        self.assertIn("WaitForCompletion()", helper)
        self.assertLess(
            helper.index("WaitForCompletion()"),
            helper.rindex("IsLoadingAssets()"),
        )

        public_prewarm = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::\n"
                "    PrewarmIstanaExploreV4ProtectedReferencesForImport("
            ) : self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ImportIstanaExploreV4Assets("
            )
        ]
        self.assertLess(
            public_prewarm.index(
                "EnsureAssetRegistryDiscoveryComplete("
            ),
            public_prewarm.index("CountAssetsUnderV4Root()"),
        )

        public_import = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ImportIstanaExploreV4Assets("
            ) : self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ValidateIstanaExploreV4Assets("
            )
        ]
        discovery = public_import.index(
            "EnsureAssetRegistryDiscoveryComplete("
        )
        census = public_import.index("CountAssetsUnderV4Root()")
        capture = public_import.index("CaptureProtectedPackages(")
        external = public_import.index("ImportExternalMeshDerivatives(")
        self.assertLess(discovery, census)
        self.assertLess(census, capture)
        self.assertLess(capture, external)

        derivative_import = self.editor_cpp[
            self.editor_cpp.index("bool ImportExternalMeshDerivatives(") :
            self.editor_cpp.index("struct FProtectedDerivativeSpec")
        ]
        normalization = derivative_import[
            derivative_import.index(
                "if (Staging->GetPathName() != ExactStagePath)"
            ) : derivative_import.index("if (!Unselected.IsEmpty())")
        ]
        self.assertIn(
            "RegistryModule.Get().IsLoadingAssets()", normalization
        )
        self.assertNotIn("WaitForCompletion()", normalization)
        self.assertLess(
            normalization.index("IsLoadingAssets()"),
            normalization.index("AssetTools.RenameAssets(Rename)"),
        )
        self.assertLess(
            derivative_import.index("AssetTools.RenameAssets(Rename)"),
            derivative_import.index(
                "ObjectTools::DeleteObjectsUnchecked(Unselected)"
            ),
        )
        self.assertNotIn("Staging->Rename(", derivative_import)

        for public_body in (public_prewarm, public_import):
            self.assertIn(
                "TStrongObjectPtr<UCesiumIonServer> "
                "CesiumIonServerKeepAlive",
                public_body,
            )
            self.assertIn(
                "UCesiumIonServer::GetServerForNewObjects()",
                public_body,
            )
            self.assertIn(
                "CesiumIonServerKeepAlive->GetOutermost()->IsDirty()",
                public_body,
            )

    def test_heavy_runtime_filters_before_first_meshdescription_commit(self) -> None:
        unbuilt = self.editor_cpp[
            self.editor_cpp.index(
                "UStaticMesh* CreateUnbuiltTopologyRuntimeDerivative("
            ) : self.editor_cpp.index("bool ImportExternalMeshDerivatives(")
        ]
        self.assertIn(
            "CreateMeshDescription(\n        0, MoveTemp(SourceDescription))",
            unbuilt,
        )
        self.assertIn("ConfigureTopologyPreservingRuntimeLods", unbuilt)
        self.assertNotIn("CommitMeshDescription", unbuilt)

        configure = self.editor_cpp[
            self.editor_cpp.index(
                "bool ConfigureTopologyPreservingRuntimeLods("
            ) : self.editor_cpp.index(
                "bool StampExactSourceRuntimeBasePolicy("
            )
        ]
        self.assertLess(
            configure.index("FilterWholeTopologyComponents("),
            configure.index("CommitMeshDescription(Lod, CommitParams)"),
        )
        self.assertEqual(
            1,
            configure.count("CommitMeshDescription(Lod, CommitParams)"),
        )

    def test_process_separated_prewarm_receipt_is_exact_and_fail_closed(self) -> None:
        receipt = self.editor_cpp[
            self.editor_cpp.index("struct FProtectedPrewarmReceiptFile") :
            self.editor_cpp.index("bool IsProtectedRuntimeMeshReference(")
        ]
        self.assertIn(
            "TRIAD_ISTANA_EXPLORE_V4_PROTECTED_PREWARM_RECEIPT_V1",
            self.editor_cpp,
        )
        self.assertIn('"DerivedDataCache"', self.editor_build)
        self.assertIn('"MeshBuilder"', self.editor_build)
        self.assertIn('"MeshUtilities"', self.editor_build)
        self.assertIn('"NaniteBuilder"', self.editor_build)
        self.assertIn('"TargetPlatform"', self.editor_build)
        for marker in (
            "ProtectedPrewarmReceipt.v1.json",
            "FPaths::ProjectSavedDir()",
            "ProducerProcessId",
            "ProducerAppInstanceId",
            "EngineBuildVersionFile",
            'TEXT("Build/Build.version")',
            "ProjectFile",
            "EditorModule",
            "VegetationContract",
            "ProtectedSnapshotSha256",
            "DdcGraphName",
            "DdcDirectories",
            "DdcZenIdentitySha256",
            "StaticMeshKeyEnvironmentRecordCount",
            "StaticMeshKeyEnvironmentSha256",
            "RenderDerivedDataKey",
            "MeshDataRawHash",
            "MeshDataRawSize",
            "SecondaryMode",
            "CardDerivedDataKey",
            "CardRawHash",
            "CardRawSize",
            "BindingSha256",
            "BuildProtectedPrewarmReceiptCanonical(",
            "HashUtf8StringSha256(",
            "ValidateExactJsonFields(",
            "ParseProtectedPrewarmReceipt(",
            "CaptureCurrentProtectedPrewarmReceiptState(",
            "ParsedCanonical != CurrentCanonical",
            "RemoveProtectedPrewarmReceiptExact(",
        ):
            self.assertIn(marker, receipt)
        for json_field in (
            "schema",
            "receiptId",
            "createdUtc",
            "producerProcessId",
            "producerAppInstanceId",
            "projectFile",
            "editorModule",
            "engineBuildVersionFile",
            "engineVersion",
            "engineChangelist",
            "vegetationContract",
            "protectedSnapshot",
            "ddcZen",
            "staticMeshKeyEnvironment",
            "protectedMeshes",
            "v4AssetCountAtIssue",
            "bindingSha256",
            "recordBucket",
            "StaticMesh",
            "meshDataRawHash",
            "meshDataRawSize",
            "secondaryMode",
            "cardDerivedDataKey",
            "cardRawHash",
            "cardRawSize",
        ):
            self.assertIn(f'TEXT("{json_field}")', receipt)
        json_writer = receipt[
            receipt.index("ProtectedPrewarmReceiptToJson(") :
            receipt.index("bool WriteProtectedPrewarmReceiptAtomically(")
        ]
        self.assertEqual(17, json_writer.count("Root->Set"))
        root_parser_gate = receipt[
            receipt.index(
                "!ValidateExactJsonFields(\n            *Root,"
            ) : receipt.index("TSharedPtr<FJsonObject> ProjectFile;")
        ]
        parsed_root_fields = re.findall(
            r'TEXT\("([A-Za-z][A-Za-z0-9]+)"\)', root_parser_gate
        )
        self.assertEqual(
            [
                "schema",
                "receiptId",
                "createdUtc",
                "producerProcessId",
                "producerAppInstanceId",
                "projectFile",
                "editorModule",
                "engineBuildVersionFile",
                "engineVersion",
                "engineChangelist",
                "vegetationContract",
                "protectedSnapshot",
                "ddcZen",
                "staticMeshKeyEnvironment",
                "protectedMeshes",
                "v4AssetCountAtIssue",
                "bindingSha256",
            ],
            parsed_root_fields,
        )

        ddc = self.editor_cpp[
            self.editor_cpp.index("bool ValidateStaticMeshDerivedDataRecord(") :
            self.editor_cpp.index("bool ValidateProtectedMeshTopology(")
        ]
        for marker in (
            'FCacheBucket(TEXT("StaticMesh"))',
            "ECachePolicy::QueryLocal",
            "ECachePolicy::SkipData",
            'FValueId::FromName("MeshData")',
            "Response.Status == EStatus::Ok",
            "MeshData.GetRawHash()",
            "MeshData.GetRawSize()",
            "ConvertLegacyCacheKey(LegacyDerivedDataKey)",
            "GetCache().GetValue(",
            "UE::FSharedString(ObjectPath)",
            'TEXT("Card")',
            "CardRawHash",
            "CardRawSize",
        ):
            self.assertIn(marker, ddc)
        self.assertNotIn("ECachePolicy::QueryRemote", ddc)
        self.assertNotIn("Request.Name = FSharedString", ddc)
        self.assertIn("ECachePolicy::KeepAlive", ddc)
        for marker in (
            'TEXT("r.GenerateMeshDistanceFields")',
            'TEXT("r.Nanite.ForceEnableMeshes")',
            'TEXT("r.MeshCardRepresentation")',
            'TEXT("r.MeshCardRepresentation.MinDensity")',
            'TEXT("r.MeshCardRepresentation.NormalTreshold")',
            'TEXT("r.MeshCardRepresentation.Debug")',
            'TEXT("r.DistanceFields.MaxPerMeshResolution")',
            'TEXT("r.DistanceFields.DefaultVoxelDensity")',
        ):
            self.assertIn(marker, receipt)
        for marker in (
            "7DD7930F-6ED7-4CF1-BE60-E9819779DBAF",
            "Source->bGenerateMeshDistanceField",
            "MeshCardRepresentation::IsDebugMode()",
            "!Source->LODGroup.IsNone()",
            "Source->IsNaniteEnabled()",
            "ForceNaniteMeshes->GetInt() != 0",
            "GenerateCards->GetInt() == 1",
        ):
            self.assertIn(marker, ddc)
        self.assertIn(
            "IMeshBuilderModule::GetForPlatform(RunningTargetPlatform)",
            receipt,
        )
        self.assertIn("GetMeshBuilderModuleName()", receipt)
        self.assertIn("MeshBuilderModule.AppendToDDCKey(", receipt)
        self.assertIn(
            'TEXT("target.meshBuilderModule.staticMeshDdcContribution")',
            receipt,
        )
        for marker in (
            "FDevSystemGuids::GetSystemGuid(",
            "STATICMESH_DERIVEDDATA_VER",
            'LoadModuleChecked<IMeshUtilities>(',
            "MeshUtilities.GetVersionString()",
            "Nanite::IBuilderModule::Get()",
            "NaniteBuilder.GetVersionString()",
            'TEXT("binary.engine")',
            'TEXT("binary.meshUtilities")',
            'TEXT("binary.naniteBuilder")',
            'TEXT("binary.editorExecutable")',
            "GIsAutomationTesting",
            'TEXT("process.isAutomationTesting")',
            "GetLODGroup(\n            NAME_None)",
            'TEXT("target.staticMeshLodGroup.None")',
        ):
            self.assertIn(marker, receipt)
        self.assertNotIn("GetLODGroupNames(", receipt)

        publication = receipt[
            receipt.index("bool WriteProtectedPrewarmReceiptAtomically(") :
            receipt.index("bool ValidateExactJsonFields(")
        ]
        for marker in (
            "FileExists(*DestinationPath)",
            "FGuid::NewGuid()",
            'TEXT(".tmp")',
            "PlatformFile.OpenWrite(",
            "TemporaryHandle->Write(",
            "TemporaryHandle->Flush(true)",
            "HashFileSha256(",
            "FJsonSerializer::Deserialize(",
            "IFileManager::Get().Move(",
            "DeleteTemporary();",
        ):
            self.assertIn(marker, publication)
        move = publication[publication.index("IFileManager::Get().Move(") :]
        self.assertRegex(
            move,
            r"IFileManager::Get\(\)\.Move\(\s*\*DestinationPath,"
            r"\s*\*TemporaryPath,\s*false,\s*false,\s*false,\s*true\)",
        )
        self.assertNotIn("SaveStringToFile", publication)

        validation = receipt[
            receipt.index("bool ValidateProtectedPrewarmReceipt(") :
        ]
        for marker in (
            "bRequireDifferentExitedProcess",
            "ProducerProcessId == FPlatformProcess::GetCurrentProcessId()",
            "Parsed.ProducerAppInstanceId == CurrentAppInstanceId",
            "FPlatformProcess::IsApplicationRunning(ProducerProcessId)",
            "ValidateProtectedPrewarmSourcesUnloaded(",
            "valid receipt was preserved",
            "bInvalidateOnDrift",
            "ValidateCleanEntryProcessBoundary(",
            "GEditor->PlayWorld",
            "KnownAutoDirtyMaterialCount != 0",
            "PrewarmEntryMapPackage",
            "RevalidateProtectedPrewarmReceiptAfterExternalSeal(",
        ):
            self.assertIn(marker, validation)
        self.assertIn("FindObject<UStaticMesh>", receipt)
        self.assertIn("FindPackage(nullptr, *PackageName)", receipt)
        post_seal = receipt[
            receipt.index(
                "bool RevalidateProtectedPrewarmReceiptAfterExternalSeal("
            ) :
        ]
        for marker in (
            "CaptureDdcZenIdentity(",
            "CaptureStaticMeshKeyEnvironmentBinding(",
            "ValidateStaticMeshDerivedDataRecord(",
            "ValidateProtectedSecondaryDerivedDataProof(",
            "MeshDataRawHash != Mesh.MeshDataRawHash",
            "MeshDataRawSize != Mesh.MeshDataRawSize",
            "ValidateProtectedPrewarmSourcesUnloaded(",
            "ValidateProtectedPackages(ProtectedSnapshot, ProtectedError)",
            "receiptInvalidation=complete",
        ):
            self.assertIn(marker, post_seal)
        self.assertNotIn(
            "CaptureCurrentProtectedPrewarmReceiptState(", post_seal
        )
        self.assertNotIn("CountAssetsUnderV4Root()", post_seal)
        public_prewarm = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::\n"
                "    PrewarmIstanaExploreV4ProtectedReferencesForImport("
            ) : self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ImportIstanaExploreV4Assets("
            )
        ]
        self.assertIn("WaitForExit/Wait-Process", public_prewarm)
        self.assertIn("producer process handle", public_prewarm)

    def test_prewarm_environment_binding_is_deterministic_across_process_lifecycles(self) -> None:
        environment = self.editor_cpp[
            self.editor_cpp.index(
                "bool CaptureStaticMeshKeyEnvironmentBinding("
            ) : self.editor_cpp.index(
                "bool CaptureCurrentProtectedPrewarmReceiptState("
            )
        ]
        for marker in (
            "FCommandLine::GetOriginal()",
            'TEXT("process.originalCommandLineUtf8Bytes")',
            'TEXT("process.originalCommandLineSha256")',
            "GetDefault<UMeshBudgetProjectSettings>()",
            "MeshBudgetProjectSettings->bEnableStaticMeshBudget",
            'TEXT("project.meshBudget.enableStaticMeshBudget")',
            "CaptureConfigRouting",
            "FProtectedPrewarmReceiptEnvironmentRow",
            "IsSafeStaticMeshEnvironmentRowName(",
            "HashUtf8StringSha256(\n                Value, DiagnosticRow.ValueSha256)",
            "OutRows.Sort(",
            "OutRows.Num() == OutRecordCount",
            "OutRows[Index - 1].Name.Compare(OutRows[Index].Name) < 0",
        ):
            self.assertIn(marker, environment)
        self.assertNotIn("FCommandLine::Get()", environment)
        self.assertIn('#include "MeshBudgetProjectSettings.h"', self.editor_cpp)
        mesh_budget_gate = environment.index(
            "MeshBudgetProjectSettings->bEnableStaticMeshBudget"
        )
        mesh_budget_row = environment.index(
            'TEXT("project.meshBudget.enableStaticMeshBudget")'
        )
        self.assertLess(mesh_budget_gate, mesh_budget_row)

        config_routing = environment[
            environment.index("const auto CaptureConfigRouting =") :
            environment.index("const auto AddReductionSettings =")
        ]
        for marker in (
            "GetConfigFilename(BaseName)",
            "ConfigSystem->FindConfigFile(Filename)",
            'RecordPrefix + TEXT(".filename")',
            'RecordPrefix + TEXT(".present")',
            "GConfig, TEXT(\"hostConfig\")",
            "RunningTargetPlatform->GetConfigSystem()",
            'TEXT("targetConfig")',
        ):
            self.assertIn(marker, config_routing)
        for forbidden in (
            "WriteToString",
            "EffectiveConfigCanonical",
            "ConfigFile->GetKeys",
            "FindSection",
            "MultiFind",
            'TEXT(".utf8Bytes")',
            'TEXT(".sha256")',
        ):
            self.assertNotIn(forbidden, config_routing)

        receipt = self.editor_cpp[
            self.editor_cpp.index("struct FProtectedPrewarmReceiptFile") :
            self.editor_cpp.index("bool IsProtectedRuntimeMeshReference(")
        ]
        for marker in (
            "StaticMeshKeyEnvironmentRows",
            "CompareStaticMeshKeyEnvironmentRows(",
            "MaxReportedDifferences = 8",
            'TEXT("currentOnly[%s]=%s")',
            'TEXT("producerOnly[%s]=%s")',
            'TEXT("changed[%s]{producerSha=%s,currentSha=%s}")',
            'TEXT("staticMeshKeyEnvironment.rowDigestCount")',
            'TEXT("staticMeshKeyEnvironment.rowDigest.%d")',
            'TEXT("valueSha256")',
            "StaticMeshKeyEnvironment->SetArrayField(\n        TEXT(\"rows\")",
            '{TEXT("recordCount"), TEXT("sha256"), TEXT("rows")}',
            "EnvironmentRows->Num() != StaticMeshKeyEnvironmentRecordCount",
            "PreviousEnvironmentRowName.Compare(Row.Name) >= 0",
            "!IsSafeStaticMeshEnvironmentRowName(Row.Name)",
            "IsExactUpperHex(Row.ValueSha256, 64)",
        ):
            self.assertIn(marker, receipt)
        environment_writer = receipt[
            receipt.index(
                "TArray<TSharedPtr<FJsonValue>> EnvironmentRows;"
            ) : receipt.index(
                "TArray<TSharedPtr<FJsonValue>> Meshes;"
            )
        ]
        self.assertNotIn('SetStringField(TEXT("value")', environment_writer)

        public_prewarm = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::\n"
                "    PrewarmIstanaExploreV4ProtectedReferencesForImport("
            ) : self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ImportIstanaExploreV4Assets("
            )
        ]
        clean_capture = public_prewarm.index(
            "CleanStaticMeshKeyEnvironmentRecordCount"
        )
        protected_load = public_prewarm.index(
            "PrewarmAndUnloadProtectedMeshReferences("
        )
        post_capture = public_prewarm.index(
            "CaptureCurrentProtectedPrewarmReceiptState("
        )
        phase_equality = public_prewarm.index(
            "EXPLORE_V4_PREWARM_FAILED_ENVIRONMENT_DRIFT"
        )
        publication = public_prewarm.index(
            "WriteProtectedPrewarmReceiptAtomically("
        )
        self.assertLess(clean_capture, protected_load)
        self.assertLess(protected_load, post_capture)
        self.assertLess(post_capture, phase_equality)
        self.assertLess(phase_equality, publication)
        for marker in (
            "CleanStaticMeshKeyEnvironmentSha",
            "CleanStaticMeshKeyEnvironmentRows",
            "Receipt.StaticMeshKeyEnvironmentRecordCount !=",
            "Receipt.StaticMeshKeyEnvironmentSha256 !=",
            "CompareStaticMeshKeyEnvironmentRows(",
            "cleanRows=%d cleanSha=%s postRows=%d postSha=%s rowDiffs=%s",
        ):
            self.assertIn(marker, public_prewarm)

        validation = self.editor_cpp[
            self.editor_cpp.index(
                "bool ValidateProtectedPrewarmReceipt("
            ) : self.editor_cpp.index(
                "bool ValidateCleanEntryProcessBoundary("
            )
        ]
        for marker in (
            "StaticMesh key environment drifted: producerRows=%d",
            "currentSha=%s rowDiffs=%s",
            "CompareStaticMeshKeyEnvironmentRows(",
            "NONE_AGGREGATE_MISMATCH",
            "DDC/Zen identity drifted: producerGraph=%s",
            "protected snapshot drifted: producerRows=%d",
            "engine/project/module/contract file binding drifted",
            "mesh/DDC proof drifted at index %d",
            "V4 asset-count boundary drifted",
            "after all named state groups matched",
        ):
            self.assertIn(marker, validation)

        post_seal = self.editor_cpp[
            self.editor_cpp.index(
                "bool RevalidateProtectedPrewarmReceiptAfterExternalSeal("
            ) : self.editor_cpp.index(
                "bool UnloadResolvedProtectedMeshReferences("
            )
        ]
        for marker in (
            "CurrentStaticMeshKeyEnvironmentRows",
            "CompareStaticMeshKeyEnvironmentRows(",
            "post-seal protected-prewarm StaticMesh key environment drifted",
            "currentSha=%s rowDiffs=%s",
        ):
            self.assertIn(marker, post_seal)

    def test_receipt_lifecycle_preserves_retry_and_deletes_only_after_success_or_drift(self) -> None:
        import_body = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ImportIstanaExploreV4Assets("
            ) : self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ValidateIstanaExploreV4Assets("
            )
        ]
        rollback = import_body[
            import_body.index("const auto RollBackFailure =") :
            import_body.index("IAssetTools& AssetTools")
        ]
        self.assertNotIn("RemoveProtectedPrewarmReceiptExact(", rollback)
        clean_consumer = import_body.index(
            "ValidateCleanEntryProcessBoundary("
        )
        receipt_gate = import_body.index("ValidateProtectedPrewarmReceipt(")
        external = import_body.index("ImportExternalMeshDerivatives(")
        stage_seal = import_body.index(
            "SealAndUnloadExactExternalStagingPackages("
        )
        post_seal_receipt = import_body.index(
            "RevalidateProtectedPrewarmReceiptAfterExternalSeal("
        )
        protected_no_load = import_body.index(
            "ValidateProtectedReferenceFactsWithoutLoading(true, Error)",
            post_seal_receipt,
        )
        persistence = import_body.index(
            "ValidateAllV4AssetsInternal(false, PersistedValidation)"
        )
        final_cleanup = import_body.rindex(
            "RemoveProtectedPrewarmReceiptExact(ReceiptCleanupError)"
        )
        self.assertLess(clean_consumer, receipt_gate)
        self.assertLess(receipt_gate, external)
        self.assertLess(external, stage_seal)
        self.assertLess(stage_seal, post_seal_receipt)
        self.assertLess(post_seal_receipt, protected_no_load)
        self.assertLess(external, persistence)
        self.assertLess(persistence, final_cleanup)
        self.assertIn("Protected, true, true, Error", import_body)
        self.assertIn("AssetsToSave.Num() != 78", import_body)
        self.assertIn("SealedStagingObjectPaths.Num() != 7", import_body)
        self.assertIn("CombinedOwnedObjectPaths.Num() != 85", import_body)
        self.assertNotIn("ResolveProtectedMeshReferences(", import_body)

    def test_external_fourteen_are_validated_then_exact_seven_are_sealed(self) -> None:
        exact_external = self.editor_cpp[
            self.editor_cpp.index(
                "bool FinishAndValidateExactExternalMeshRoster("
            ) : self.editor_cpp.index(
                "bool SealAndUnloadExactExternalStagingPackages("
            )
        ]
        for marker in (
            "RuntimeMeshes.Num() != 7",
            "ExternalMeshSpecs().Num() != 7",
            "ExactExternalMeshes.Num() != 14",
            "ExpectedObjectPaths.Num() != 14",
            "AssetsToSave.Num() != 14",
            "FinishCompilation(",
            "ValidateStagingMesh(",
            "ValidateRuntimeDerivative(",
        ):
            self.assertIn(marker, exact_external)

        seal = self.editor_cpp[
            self.editor_cpp.index(
                "bool SealAndUnloadExactExternalStagingPackages("
            ) : self.editor_cpp.index("struct FProtectedDerivativeSpec")
        ]
        for marker in (
            "OutSealedObjectPaths = ExactExternalStagingObjectPaths()",
            "OutSealedObjectPaths.Num() != 7",
            "StagingAssets.Num() != 7",
            "StagingPackages.Num() != 7",
            "StagingPackageNames.Num() != 7",
            "SaveLoadedAssets(StagingAssets, false)",
            "Package->IsDirty()",
            "CaptureExactPackageFileDigests(",
            "ValidateExactPackageFileDigests(",
            "Removed != 7",
            "InOutAssetsToSave.Num() != 7",
            "bUnloadDirtyPackages = false",
            "bResetTransBuffer = true",
            "UPackageTools::UnloadPackages(UnloadParams)",
            "FindObject<UObject>(nullptr, *ObjectPathToSeal)",
            "FindPackage(nullptr, *PackageName)",
            "FMemory::Trim(true)",
        ):
            self.assertIn(marker, seal)
        self.assertEqual(2, seal.count("ValidateExactPackageFileDigests("))
        self.assertLess(
            seal.index("SaveLoadedAssets(StagingAssets, false)"),
            seal.index("CaptureExactPackageFileDigests("),
        )
        self.assertLess(
            seal.index("CaptureExactPackageFileDigests("),
            seal.index("InOutAssetsToSave.RemoveAll("),
        )
        self.assertLess(
            seal.index("InOutAssetsToSave.RemoveAll("),
            seal.index("UPackageTools::UnloadPackages(UnloadParams)"),
        )
        for forbidden in (
            "RuntimeMeshObjectPath(",
            "bUnloadDirtyPackages = true",
            "LoadAsset(",
            "DeleteObjects",
        ):
            self.assertNotIn(forbidden, seal)

    def test_protected_tree_meshes_are_exact_read_only_references(self) -> None:
        for marker in (
            "PROTECTED_INHERITED_UASSET_EXACT_READ_ONLY_REFERENCE",
            "PROTECTED_UASSET_DIRECT_REFERENCE_NO_IMPORT_OR_DUPLICATION",
            "DIRECT_READ_ONLY_USTATICMESH_REFERENCE_WITH_COMPONENT_MATERIAL_OVERRIDES",
            "protectedExactRuntimeReferenceExceptions",
            "protectedReferencesAreReadOnly",
            "protectedReferenceMaterialOverridesAreComponentLocal",
            "protectedReferencePackagesMayBeSavedByV4Workflow",
            "ResolveProtectedMeshReferences",
            "ValidateProtectedRuntimeMeshReference",
            "IsExactProtectedReferenceForWindRole",
            "nine V4-owned meshes plus two exact protected read-only references",
        ):
            self.assertIn(marker, self.editor_cpp)
        resolver = self.editor_cpp[
            self.editor_cpp.index("bool ResolveProtectedMeshReferences(") :
            self.editor_cpp.index("bool DuplicateCloseTurfAndBlocker(")
        ]
        for forbidden in (
            "DuplicateAsset(",
            "RemoveVisualCollision(",
            "StampDerivativeProvenance(",
            "MarkPackageDirty(",
            "OutAssetsToSave",
        ):
            self.assertNotIn(forbidden, resolver)
        binder = self.editor_cpp[
            self.editor_cpp.index("bool BindWindMaterialsToRuntimeMeshes(") :
            self.editor_cpp.index("FString LawnBaseMaterialObjectPath()")
        ]
        self.assertIn("if (IsProtectedWindRole(Spec.Role))", binder)
        self.assertIn("ValidateFrozenProtectedWindBinding", binder)
        self.assertIn(
            "MeshForWindRole(Meshes, Spec.Role) != nullptr", binder
        )
        self.assertIn("BoundOwnedSlots.Num() != 12", binder)
        self.assertIn("MutatedOwnedMeshes", binder)
        self.assertNotIn("Mesh->GetOutermost()->IsDirty()", binder)

    def test_idempotent_successes_repeat_the_strict_dirty_gate(self) -> None:
        import_body = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::ImportIstanaExploreV4Assets("
            ) :
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::ValidateIstanaExploreV4Assets("
            )
        ]
        build_body = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::BuildIstanaExploreV4Map("
            ) :
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::ValidateIstanaExploreV4Map("
            )
        ]
        self.assertGreaterEqual(import_body.count("HasDisallowedDirtyPackages("), 3)
        self.assertGreaterEqual(build_body.count("HasDisallowedDirtyPackages("), 3)
        self.assertIn(
            "EXPLORE_V4_IMPORT_REFUSED_AFTER_IDEMPOTENT_VALIDATION",
            import_body,
        )
        self.assertIn(
            "EXPLORE_V4_BUILD_REFUSED_AFTER_IDEMPOTENT_VALIDATION",
            build_body,
        )

    def test_exact_source_topology_slots_lods_and_no_raw_lod0_selection(self) -> None:
        expected = {
            "umbrella_tree": (1072213, [27298, 714744, 330171]),
            "dense_dome_tree": (2062487, [94814, 1939380, 28293]),
            "palm_accent": (3208, [3208]),
            "high_fork_tree": (1599403, [34787, 1060032, 504584]),
            "columnar_tree": (3863832, [1231286, 230112, 2402434]),
        }
        rows = {
            row["selectionId"]: row
            for row in self.vegetation_contract["selectedMeshDerivatives"]
        }
        for selection, (triangles, per_slot) in expected.items():
            with self.subTest(selection=selection):
                self.assertEqual(rows[selection]["sourceTriangles"], triangles)
                by_slot = rows[selection]["sectionTriangleCountsBySlot"]
                self.assertEqual(
                    [by_slot[str(index)] for index in range(len(per_slot))],
                    per_slot,
                )
                self.assertEqual(rows[selection]["runtimeRequiredMinLOD"], 1)
                self.assertFalse(rows[selection]["rawSourceRuntimeSelected"])
                self.assertIn(f'TEXT("{selection}")', self.editor_cpp)
        for marker in (
            "GetMinLODIdx() != 1",
            "GetMinLODIdx() < 1",
            "ForcedLodModel",
            "rawLod0MayBeSelectedByMapOrRuntimeActor",
            "rawSourceAssetsAreEditorOnlyStaging",
            "runtimeReferencesMustResolveOnlyUnder",
            "GetReferencers",
            "A runtime/map package references editor-only raw LOD0 staging",
        ):
            self.assertIn(marker, self.all_source)

    def test_raw_mesh_import_policy_is_format_exact_and_memory_bounded(self) -> None:
        derivatives = {
            row["selectionId"]: row
            for row in self.vegetation_contract["selectedMeshDerivatives"]
        }
        for selection in (
            "umbrella_tree",
            "dense_dome_tree",
            "shrub_04_a",
            "periwinkle_06_f",
            "calathea_d",
            "grass_medium_small_a",
        ):
            policy = derivatives[selection]["sourceImportOptions"]
            self.assertTrue(policy["ConvertScene"])
            self.assertTrue(policy["ConvertSceneUnit"])
            self.assertEqual(policy["ImportUniformScale"], 1.0)
            self.assertTrue(policy["bTransformVertexToAbsolute"])
            self.assertFalse(policy["bBakePivotInVertex"])
        palm_policy = derivatives["palm_accent"]["sourceImportOptions"]
        self.assertFalse(palm_policy["ConvertScene"])
        self.assertFalse(palm_policy["ConvertSceneUnit"])
        self.assertEqual(palm_policy["ImportUniformScale"], 100.0)
        self.assertTrue(palm_policy["bTransformVertexToAbsolute"])
        self.assertFalse(palm_policy["bBakePivotInVertex"])

        importer = self.editor_cpp[
            self.editor_cpp.index("UAssetImportTask* MakeRawMeshImportTask(") :
            self.editor_cpp.index("bool ValidateRawMeshImportTask(")
        ]
        validator = self.editor_cpp[
            self.editor_cpp.index("bool ValidateRawMeshImportTask(") :
            self.editor_cpp.index("bool HasExactRawMeshTopology(")
        ]
        for source in (importer, validator):
            self.assertIn("Spec.bObjMetresZUp", source)
            self.assertIn("bConvertScene", source)
            self.assertIn("bConvertSceneUnit", source)
            self.assertIn("100.0f", source)
            self.assertIn("bTransformVertexToAbsolute", source)
            self.assertIn("bBakePivotInVertex", source)
        self.assertIn(
            "bTransformVertexToAbsolute = true",
            importer,
        )
        self.assertIn("bBakePivotInVertex = false", importer)
        self.assertIn(
            "Spec.bObjMetresZUp ? 100.0f : 1.0f",
            importer,
        )
        self.assertNotIn("ImportMismatchDiagnostics", self.editor_cpp)
        self.assertIn("exact UE5.5 imported topology tuple", self.editor_cpp)
        for marker in (
            "triangles=%d",
            "slots=[%s]",
            "sections=[%s]",
            "zSpanCm=%.9f",
        ):
            self.assertIn(marker, self.editor_cpp)

        specs = self.editor_cpp[
            self.editor_cpp.index("const TArray<FExternalMeshSpec>& ExternalMeshSpecs()") :
            self.editor_cpp.index("FString StagingMeshObjectPath(")
        ]
        self.assertLess(
            specs.index('TEXT("dense_dome_tree")'),
            specs.index('TEXT("umbrella_tree")'),
        )
        public_import = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ImportIstanaExploreV4Assets("
            ) :
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ValidateIstanaExploreV4Assets("
            )
        ]
        self.assertLess(
            public_import.index("ImportExternalMeshDerivatives("),
            public_import.index("ImportRequiredTextures("),
        )
        for marker in (
            '#include "MeshDescription.h"',
            '#include "StaticMeshAttributes.h"',
            "TOPOLOGY_PRESERVING_MATERIAL_EDGE_COMPONENTS_V1",
            "EXACT_SOURCE_NO_PRE_REDUCTION_V1",
            "CUSTOM_TOPOLOGY_PRESERVING_SELF_BASED_SOURCE_LODS_V1",
            "GetEdgeConnectedPolygons",
            "DeleteBatchSize",
            "Description.DeletePolygons(Batch)",
            "Description.Compact(Remappings)",
            "CloneMeshDescription",
            "CreateMeshDescription",
            "CommitMeshDescription(Lod, CommitParams)",
            "ResetReductionSetting",
            "Mesh->IsReductionActive(Lod)",
            "EnsureRawTopologyIdentityAttributes",
            "TRIAD_IPV4_RawTopologyPolygonId",
            "TRIAD_IPV4_RawTopologyVertexId",
            "CreateUnbuiltTopologyRuntimeDerivative",
            "NewObject<UStaticMesh>",
            "FAssetRegistryModule::AssetCreated(Runtime)",
            "LodPositionBufferBounds",
            "Lod0PositionZSpanCm",
            "Staging->ClearMeshDescriptions()",
            "FMemory::Trim(true)",
            "HasRuntimeBasePolicy",
            "{94814, 176890, 28293}",
            "{27298, 142512, 130190}",
        ):
            self.assertIn(marker, self.editor_cpp)
        for obsolete in (
            "PrepareMemoryBoundedRuntimeBase",
            "DETERMINISTIC_MATERIAL_AWARE_POLYGON_THINNING_V1",
            "EXACT_IMPORTED_SOURCE_WITHIN_CAP_V1",
            "DeletePolygons(PolygonsToDelete)",
        ):
            self.assertNotIn(obsolete, self.editor_cpp)
        build_cs = text(
            REPO
            / "unreal/Plugins/TRIADSensorFusion/Source/"
            "TRIADSensorFusionEditor/TRIADSensorFusionEditor.Build.cs"
        )
        self.assertIn('"MeshDescription"', build_cs)
        derivative_import = self.editor_cpp[
            self.editor_cpp.index("bool ImportExternalMeshDerivatives(") :
            self.editor_cpp.index("struct FProtectedDerivativeSpec")
        ]
        self.assertLess(
            derivative_import.index("Staging->ClearMeshDescriptions()"),
            derivative_import.index("ConfigureTopologyPreservingRuntimeLods("),
        )
        self.assertIn(
            "bTopologyPreservingHeavyTree\n"
            "            ? CreateUnbuiltTopologyRuntimeDerivative",
            derivative_import,
        )
        self.assertLess(
            derivative_import.index("StampExactSourceRuntimeBasePolicy("),
            derivative_import.index("ConfigureRuntimeDerivativeLods("),
        )
        raw_identity = self.editor_cpp[
            self.editor_cpp.index("bool HasExactRawMeshTopology(") :
            self.editor_cpp.index("void ApplyVisualCollisionPolicyWithoutBuild(")
        ]
        runtime_identity = self.editor_cpp[
            self.editor_cpp.index("bool ValidateRuntimeDerivative(") :
            self.editor_cpp.index("bool ImportExternalMeshDerivatives(")
        ]
        for identity in (raw_identity, runtime_identity):
            self.assertIn("Lod0PositionZSpanCm", identity)
            self.assertNotIn("Mesh->GetBounds()", identity)

    def test_external_mesh_import_extends_preseeded_protected_reference_roster(self) -> None:
        derivative_import = self.editor_cpp[
            self.editor_cpp.index("bool ImportExternalMeshDerivatives(") :
            self.editor_cpp.index("struct FProtectedDerivativeSpec")
        ]
        for marker in (
            "const int32 InitialCount = OutRuntimeMeshes.Num();",
            "ExternalMeshSpecs().Num() != 7",
            "OutRuntimeMeshes.Contains(Spec.RuntimeAssetName)",
            "OutRuntimeMeshes.Num() != InitialCount + 7",
            "collides with a preexisting protected/runtime reference",
        ):
            self.assertIn(marker, derivative_import)
        self.assertNotIn("OutRuntimeMeshes.Num() != 7", derivative_import)
        self.assertLess(
            derivative_import.index(
                "OutRuntimeMeshes.Contains(Spec.RuntimeAssetName)"
            ),
            derivative_import.index("AssetTools.ImportAssetTasks({Task})"),
        )

        preseeded_protected_keys = {
            "SM_IPV4_IslandTree01_HighForkProxy",
            "SM_IPV4_Broadleaf_ColumnarProxy",
        }
        external_keys = {
            "SM_IPV4_UmbrellaTree",
            "SM_IPV4_DenseDomeTree",
            "SM_IPV4_PalmAccent",
            "SM_IPV4_Shrub04_A",
            "SM_IPV4_Periwinkle06_F",
            "SM_IPV4_Calathea_D",
            "SM_IPV4_GrassMedium_SmallA",
        }
        self.assertTrue(preseeded_protected_keys.isdisjoint(external_keys))
        self.assertEqual(9, len(preseeded_protected_keys | external_keys))

    def test_heavy_external_and_serial_textures_keep_protected_meshes_unloaded(self) -> None:
        public_import = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ImportIstanaExploreV4Assets("
            ) :
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ValidateIstanaExploreV4Assets("
            )
        ]
        phase = public_import[
            public_import.index("TMap<FString, UStaticMesh*> RuntimeMeshes;") :
            public_import.index("if (!CreateAndBindAllMaterials(")
        ]
        empty = phase.index("RuntimeMeshes.IsEmpty()")
        external = phase.index("ImportExternalMeshDerivatives(")
        seven = phase.index("RuntimeMeshes.Num() != 7")
        exact_fourteen = phase.index(
            "FinishAndValidateExactExternalMeshRoster("
        )
        seal = phase.index("SealAndUnloadExactExternalStagingPackages(")
        sealed_live_seven = phase.index("AssetsToSave.Num() != 7")
        textures = phase.index("ImportRequiredTextures(")
        close_turf = phase.index("DuplicateCloseTurfAndBlocker(")
        nine = phase.index("RuntimeMeshes.Num() != 9")
        finish_owned = phase.index("FinishCompilation(CompileMeshes)")
        collect = phase.index(
            "CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS)"
        )
        trim = phase.index("FMemory::Trim(true)")
        protected_facts = phase.index(
            "ValidateProtectedReferenceFactsWithoutLoading(true, Error)"
        )
        self.assertLess(empty, external)
        self.assertLess(external, seven)
        self.assertLess(seven, exact_fourteen)
        self.assertLess(exact_fourteen, seal)
        self.assertLess(seal, sealed_live_seven)
        self.assertLess(sealed_live_seven, textures)
        self.assertLess(textures, close_turf)
        self.assertLess(close_turf, nine)
        self.assertLess(nine, finish_owned)
        self.assertLess(finish_owned, collect)
        self.assertLess(collect, trim)
        self.assertLess(trim, protected_facts)
        self.assertEqual(
            1,
            phase.count(
                "ValidateProtectedReferenceFactsWithoutLoading(true, Error)"
            ),
        )
        self.assertNotIn("ResolveProtectedMeshReferences(", phase)
        self.assertNotIn("RuntimeMeshes.Num() != 11", phase)

    def test_texture_import_is_serial_compiled_and_memory_bounded(self) -> None:
        helper = self.editor_cpp[
            self.editor_cpp.index("bool ImportRequiredTextures(") :
            self.editor_cpp.index("int32 CountTriangles(")
        ]
        for marker in (
            '#include "TextureCompiler.h"',
            "Specs.Num() != 41",
            "AssetTools.ImportAssetTasks({Task})",
            "Texture->PostEditChange()",
            "FTextureCompilingManager::Get().FinishCompilation({Texture})",
            "FTextureCompilingManager::Get().IsCompilingTexture(Texture)",
            "ImportedNames.Num() != 41",
            "OutTextures.Num() != 41",
            "FMemory::Trim(true)",
        ):
            self.assertIn(marker, self.editor_cpp if marker.startswith("#include") else helper)
        self.assertLess(
            helper.index("Texture->PostEditChange()"),
            helper.index(
                "FTextureCompilingManager::Get().FinishCompilation({Texture})"
            ),
        )
        for obsolete in (
            "TArray<UAssetImportTask*> Tasks",
            "TasksByName",
            "ImportAssetTasks(Tasks)",
            "Texture->UpdateResource()",
        ):
            self.assertNotIn(obsolete, helper)

    def test_sealed_seven_plus_loaded_seventy_eight_is_the_only_commit_ledger(self) -> None:
        public_import = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ImportIstanaExploreV4Assets("
            ) : self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::"
                "ValidateIstanaExploreV4Assets("
            )
        ]
        for marker in (
            "AssetsToSave.Num() != 78",
            "LoadedObjectPaths.Num() != 78",
            "SealedStagingObjectPaths.Num() != 7",
            "CombinedOwnedObjectPaths.Num() != 85",
            "CountAssetsUnderV4Root() != 85",
            "PersistedObjectPaths.Num() != 85",
            "UniquePackagesToReload.Num() != 78",
            "CombinedOwnedPackageNames.Num() != 85",
            "SaveLoadedAssets(AssetsToSave, false)",
            "ValidateAllV4NonStagingAssetsInternal(InMemoryValidation)",
            "ValidateProtectedReferenceFactsWithoutLoading(true, Error)",
            "ValidateAllV4AssetsInternal(false, PersistedValidation)",
        ):
            self.assertIn(marker, public_import)
        self.assertGreaterEqual(
            public_import.count("ValidateExactPackageFileDigests("), 3
        )
        self.assertLess(
            public_import.index("SealAndUnloadExactExternalStagingPackages("),
            public_import.index("ImportRequiredTextures("),
        )
        self.assertNotIn("ResolveProtectedMeshReferences(", public_import)
        hot_gate = public_import[
            public_import.index("TSet<FString> CombinedOwnedObjectPaths") :
            public_import.index("TArray<FString> PersistedObjectPaths")
        ]
        self.assertIn("ValidateAllV4NonStagingAssetsInternal(", hot_gate)
        self.assertNotIn("ValidateAllV4AssetsInternal(", hot_gate)
        self.assertNotIn("AssetSubsystem->LoadAsset(", public_import)
        self.assertNotIn("UniquePackagesToReload.Num() != 85", public_import)
        self.assertNotIn("AssetsToSave.Num() != 85", public_import)

        final_unload = public_import.index(
            "UPackageTools::UnloadPackages(\n"
            "            PackagesToReload, UnloadError, false)"
        )
        absence_gate = public_import.index(
            "for (const FString& ObjectPathToReload : PersistedObjectPaths)"
        )
        final_validation = public_import.index(
            "ValidateAllV4AssetsInternal(false, PersistedValidation)"
        )
        protected_absence = public_import.index(
            "ValidateProtectedReferenceFactsWithoutLoading(true, Error)",
            absence_gate,
        )
        self.assertLess(final_unload, absence_gate)
        self.assertLess(absence_gate, protected_absence)
        self.assertLess(protected_absence, final_validation)
        final_gate = public_import[
            public_import.index("FString PersistedValidation;") :
            public_import.index("if (HasDisallowedDirtyPackages(", final_validation)
        ]
        self.assertEqual(2, final_gate.count("ValidateExactPackageFileDigests("))
        self.assertLess(
            final_gate.index("ValidateExactPackageFileDigests("),
            final_gate.index(
                "ValidateAllV4AssetsInternal(false, PersistedValidation)"
            ),
        )
        self.assertLess(
            final_gate.index(
                "ValidateAllV4AssetsInternal(false, PersistedValidation)"
            ),
            final_gate.rindex("ValidateExactPackageFileDigests("),
        )

    def test_protected_live_validation_is_one_at_a_time_or_map_resident(self) -> None:
        exact_wait = self.editor_cpp[
            self.editor_cpp.index("bool FinishExactStaticMeshRosterMemoryBounded(") :
            self.editor_cpp.index(
                "bool ValidateProtectedRuntimeMeshReferencesMemoryBounded("
            )
        ]
        for marker in (
            "for (UStaticMesh* Mesh : Meshes)",
            "UniqueMeshes.Contains(Mesh)",
            "Mesh->GetOutermost()->IsDirty()",
            "if (Mesh->IsCompiling())",
            "FinishCompilation({Mesh})",
            "Mesh->GetRenderData()",
            "RenderData->LODResources.IsEmpty()",
            "Mesh->GetNumLODs() <= 0",
            "Mesh->ClearMeshDescriptions()",
            "FMemory::Trim(true)",
        ):
            self.assertIn(marker, exact_wait)
        self.assertNotIn("FinishAllCompilation", exact_wait)
        self.assertNotIn("FinishCompilation(Meshes)", exact_wait)

        protected_validation = self.editor_cpp[
            self.editor_cpp.index(
                "bool ValidateProtectedRuntimeMeshReferencesMemoryBounded("
            ) : self.editor_cpp.index(
                "bool ValidateV4StagingAssetsMemoryBounded("
            )
        ]
        for marker in (
            "ProtectedDerivativeSpecs().Num() != 2",
            "LoadedObjects == 2 && LoadedPackages == 2",
            "LoadedObjects != 0 || LoadedPackages != 0",
            "int32 Validated = 0",
            "for (const FProtectedDerivativeSpec& Spec :",
            "for (const FProtectedDerivativeSpec& Other :",
            "FindObject<UStaticMesh>(",
            "LoadExact<UStaticMesh>(",
            "FinishExactStaticMeshRosterMemoryBounded(",
            "ValidateProtectedRuntimeMeshReference(",
            "Reference->ClearMeshDescriptions()",
            "ReferencePackage->IsDirty()",
            "UnloadOneExactCleanPackage(",
            "ResolvePackageFileAndValidate(",
            "Validated != 2",
            "ValidateProtectedPrewarmSourcesUnloaded(OutError)",
        ):
            self.assertIn(marker, protected_validation)
        streamed_start = protected_validation.index(
            "UStaticMesh* Reference = LoadExact<UStaticMesh>("
        )
        self.assertLess(
            streamed_start,
            protected_validation.index(
                "FinishExactStaticMeshRosterMemoryBounded(", streamed_start
            ),
        )
        self.assertLess(
            protected_validation.index(
                "FinishExactStaticMeshRosterMemoryBounded(", streamed_start
            ),
            protected_validation.index(
                "ValidateProtectedRuntimeMeshReference(", streamed_start
            ),
        )
        self.assertLess(
            protected_validation.index(
                "Reference->ClearMeshDescriptions()", streamed_start
            ),
            protected_validation.index(
                "UnloadOneExactCleanPackage(", streamed_start
            ),
        )
        for forbidden in (
            "FinishAllCompilation",
            "SaveLoadedAssets(",
            "CreatePackage(",
            "DuplicateAsset(",
            "bUnloadDirtyPackages = true",
        ):
            self.assertNotIn(forbidden, protected_validation)

    def test_cold_staging_validation_streams_and_unloads_exactly_one_at_a_time(self) -> None:
        staging = self.editor_cpp[
            self.editor_cpp.index(
                "bool ValidateV4StagingAssetsMemoryBounded("
            ) : self.editor_cpp.index(
                "bool ValidateAllV4NonStagingAssetsInternal("
            )
        ]
        for marker in (
            "ExactStagingPaths.Num() != 7",
            "for (const FExternalMeshSpec& Spec : ExternalMeshSpecs())",
            "LoadExact<UStaticMesh>(ExactObjectPath)",
            "FinishExactStaticMeshRosterMemoryBounded(",
            "ValidateStagingMesh(Staging, Spec, OutError)",
            "Staging->ClearMeshDescriptions()",
            "StagingPackage->IsDirty()",
            "const auto FailAndRelease =",
            "UnloadOneExactCleanPackage(",
            "ValidatedCount != 7",
        ):
            self.assertIn(marker, staging)
        ordered_markers = (
            "FinishExactStaticMeshRosterMemoryBounded(",
            "ValidateStagingMesh(Staging, Spec, OutError)",
            "Staging->ClearMeshDescriptions()",
            "StagingPackage->IsDirty()",
            "UnloadOneExactCleanPackage(",
        )
        success_start = staging.index("FinishExactStaticMeshRosterMemoryBounded(")
        self.assertLess(
            staging.index("LoadExact<UStaticMesh>(ExactObjectPath)"),
            success_start,
        )
        offsets = [
            staging.index(marker, success_start) for marker in ordered_markers
        ]
        self.assertEqual(offsets, sorted(offsets))
        for forbidden in (
            "FinishAllCompilation",
            "bWasAlreadyLoaded",
            "bUnloadDirtyPackages = true",
            "LoadRuntimeMeshRoster",
        ):
            self.assertNotIn(forbidden, staging)
        self.assertEqual(2, staging.count("UnloadOneExactCleanPackage("))
        self.assertEqual(3, staging.count("FailAndRelease("))
        self.assertIn("return FailAndRelease(Failure, OutError)", staging)

        non_staging = self.editor_cpp[
            self.editor_cpp.index(
                "bool ValidateAllV4NonStagingAssetsInternal("
            ) : self.editor_cpp.index("bool ValidateAllV4AssetsInternal(")
        ]
        self.assertIn(
            "LoadOwnedRuntimeMeshRoster(Meshes, OutError)", non_staging
        )
        self.assertIn(
            "ValidateProtectedReferenceFactsWithoutLoading(false, OutError)",
            non_staging,
        )
        self.assertNotIn("LoadRuntimeMeshRoster(Meshes, OutError)", non_staging)
        self.assertNotIn(
            "LoadExact<UStaticMesh>(StagingMeshObjectPath", non_staging
        )
        self.assertLess(
            non_staging.index(
                "LoadOwnedRuntimeMeshRoster(Meshes, OutError)"
            ),
            non_staging.index(
                "LoadExact<UTexture2D>(TextureObjectPath(Spec.AssetName))"
            ),
        )
        wrapper = self.editor_cpp[
            self.editor_cpp.index("bool ValidateAllV4AssetsInternal(") :
            self.editor_cpp.index("bool RollBackNewV4Namespace(")
        ]
        self.assertLess(
            wrapper.index(
                "ValidateProtectedRuntimeMeshReferencesMemoryBounded(OutError)"
            ),
            wrapper.index("ValidateV4StagingAssetsMemoryBounded(OutError)"),
        )
        self.assertLess(
            wrapper.index("ValidateV4StagingAssetsMemoryBounded(OutError)"),
            wrapper.index("ValidateAllV4NonStagingAssetsInternal(OutError)"),
        )

    def test_portico_is_exact_identity_successor_with_collision_disabled(self) -> None:
        for marker in (
            "CountTriangles(Mesh, 0) != 6592",
            "PorticoTrianglesBySlot",
            "3100, 1704, 48, 1092, 36, 144, 468",
            "PorticoSlotOrder",
            "ValidatePorticoImportTask",
            "bConvertScene = false",
            "bConvertSceneUnit = false",
            "bTransformVertexToAbsolute = true",
            "bAutoGenerateCollision = false",
            "bCombineMeshes = true",
            "bReorderMaterialToFbxOrder = true",
            "!Data->bReorderMaterialToFbxOrder",
            "PorticoV8RenderOnlyComponent->GetComponentTransform().Equals(\n"
            "            FTransform::Identity",
            "V3Live[8]",
            "HideInheritedVisual(V3Live[8])",
        ):
            self.assertIn(marker, self.editor_cpp)
        self.assertIn("presentationSuccessorOfFrozenV7", self.editor_cpp)
        self.assertIn("v7AndV8ConcurrentRenderingAuthorized", self.editor_cpp)

    def test_portico_ue55_slot_order_is_calibrated_and_mirrored(self) -> None:
        v7_materials, v7_triangles = ue55_legacy_obj_material_census(
            V7_PORTICO_OBJ
        )
        self.assertEqual(
            v7_materials,
            [
                "M_IPV7_Portico_Soffit",
                "M_IPV7_Portico_Trim",
                "M_IPV7_Portico_Recess",
                "M_IPV7_Portico_Metal",
                "M_IPV7_Portico_Glass",
                "M_IPV7_Portico_Louvre",
            ],
        )
        self.assertEqual(v7_triangles, [948, 3176, 72, 252, 576, 792])

        v8_obj = V8_ROOT / (
            "Generated/SM_IstanaPublicViewV8_"
            "CentralPorticoDepthOverlay_V5Live.obj"
        )
        expected_materials, expected_triangles = (
            ue55_legacy_obj_material_census(v8_obj)
        )
        self.assertEqual(
            expected_materials,
            [
                "M_IPV8_Portico_Trim",
                "M_IPV8_Portico_Stone",
                "M_IPV8_Portico_Render",
                "M_IPV8_Portico_Soffit",
                "M_IPV8_Portico_Recess",
                "M_IPV8_Portico_Metal",
                "M_IPV8_Portico_Louvre",
            ],
        )
        self.assertEqual(
            expected_triangles, [3100, 1704, 48, 1092, 36, 144, 468]
        )

        editor_slots = self.editor_cpp[
            self.editor_cpp.index("const TArray<FName>& PorticoSlotOrder()") :
            self.editor_cpp.index("FString PorticoMeshObjectPath()")
        ]
        runtime_slots = self.runtime_cpp[
            self.runtime_cpp.index("const FName ExpectedPorticoMaterials[]") :
            self.runtime_cpp.index("static_assert(UE_ARRAY_COUNT(ExpectedHeritageAnchors)")
        ]
        for source in (editor_slots, runtime_slots):
            previous = -1
            for material in expected_materials:
                current = source.index(f'TEXT("{material}")')
                self.assertGreater(current, previous)
                previous = current
            self.assertIn(
                ", ".join(str(value) for value in expected_triangles), source
            )
        self.assertIn(
            'MaterialAssetPath(VegetationAssetPath + TEXT("/Materials"))',
            self.editor_cpp,
        )
        self.assertIn(
            'V4VegetationRuntimeObjectRoot + TEXT("Materials/")',
            self.runtime_cpp,
        )
        self.assertIn("ExpectedPorticoMaterialObjectPath", self.runtime_cpp)
        self.assertIn(
            "BoundMaterial->GetPathName() !=\n"
            "                ExpectedPorticoMaterialObjectPath(",
            self.runtime_cpp,
        )

    def test_wind_roles_slots_and_static_lawn_animated_cards_are_separate(self) -> None:
        expected_slots = {
            "UmbrellaTrunk": 0,
            "UmbrellaBranch": 2,
            "UmbrellaLeaf": 1,
            "DomeTrunk": 2,
            "DomeBranch": 0,
            "DomeLeaf": 1,
            "HighForkTrunk": 0,
            "HighForkBranch": 2,
            "HighForkLeaf": 1,
            "ColumnarTrunk": 1,
            "ColumnarBranch": 0,
            "ColumnarLeaf": 2,
        }
        for role, slot in expected_slots.items():
            self.assertIn(
                f"ETRIADIstanaExploreV4WindRole::{role}: return {slot};",
                self.editor_cpp,
            )
        for role in (
            "PalmComposite",
            "Shrub",
            "Flower",
            "Understorey",
            "GeometryGrass",
            "CloseTurf",
        ):
            self.assertIn(f"ETRIADIstanaExploreV4WindRole::{role}", self.editor_cpp)
        self.assertIn("OutBindings.Num() != 18", self.editor_cpp)
        self.assertIn("Binding.bUsesPerInstanceLocalPosition = true", self.editor_cpp)
        self.assertIn('TEXT("M_IPV4_CloseTurf_Wind")', self.editor_cpp)
        self.assertIn('TEXT("MI_IPV4_AmbientCG_Grass001_Lawn")', self.all_source)
        self.assertIn("bExternalStaticLawnSurfaceUsesWindWpo = false", self.runtime_h)
        self.assertIn("staticLawnWpo=false animatedCloseTurf=true", self.editor_cpp)

    def test_static_lawn_has_exact_anti_tiling_graph_and_no_displacement(self) -> None:
        for marker in (
            "TileSizeCm->R = 140.0f",
            "MacroTileSizeCm->R = 3200.0f",
            "DetailOffset->R = 19.25f",
            "DetailOffset->G = -31.75f",
            'TEXT("BaseColorTextureRotated")',
            'TEXT("RoughnessTextureRotated")',
            'TEXT("NormalDXTextureRotated")',
            'TEXT("AmbientOcclusionTextureRotated")',
            'TEXT("MacroVariationTexture")',
            "DetailBlend->Alpha.Connect(1, MacroSample)",
            "TintBlend->Alpha.Connect(1, MacroSample)",
            "RoughDetailBlend->Alpha.Connect(1, MacroSample)",
            "RoughScaleBlend->Alpha.Connect(1, MacroSample)",
            "NormalDetailBlend->Alpha.Connect(1, MacroSample)",
            "AoDetailBlend->Alpha.Connect(1, MacroSample)",
            "ReorientedNormalXY->A.Connect(0, NegativeRotatedNormalY)",
            "ReorientedNormalXY->B.Connect(0, RotatedNormalX)",
            "ReorientedNormalXYZ->B.Connect(0, RotatedNormalZ)",
            "NormalizedDetailNormal->VectorInput.Connect(0, NormalDetailBlend)",
            "MaxWorldPositionOffsetDisplacement = 0.0f",
            "EditorOnly->WorldPositionOffset.Expression",
            "EditorOnly->PixelDepthOffset.Expression",
            "ExpressionCollection.Expressions.Num() != 41",
            "Samples.Num() != 9",
            "Normalizes.Num() != 1",
            "WPT_ExcludeAllShaderOffsets",
            "T_IPV4_Grass001_NormalDX",
            "T_IPV4_Grass001_NormalGL",
        ):
            self.assertIn(marker, self.editor_cpp)
        lawn_creation = self.editor_cpp[
            self.editor_cpp.index("UMaterialInstanceConstant* CreateStaticLawnMaterial(") :
            self.editor_cpp.index("bool ValidateStaticLawnMaterial(")
        ]
        self.assertNotIn("MaterialExpressionCustom", lawn_creation)
        self.assertNotIn("WorldPositionOffset.Connect", lawn_creation)
        self.assertNotIn("PixelDepthOffset.Connect", lawn_creation)

    def test_grid_is_exact_201_square_and_bands_are_frozen(self) -> None:
        grid = json.loads(
            text(V4_ROOT / "Prepared/v4_raster_sample_grid.json")
        )
        self.assertEqual(grid["grid"]["width"], 201)
        self.assertEqual(grid["grid"]["height"], 201)
        self.assertEqual(grid["grid"]["stepMeters"], [10.0, 10.0])
        self.assertEqual(
            grid["samplingPolicy"]["canopyHeightBandsMeters"],
            [[1, 16], [17, 22], [23, 32]],
        )
        for marker in (
            "constexpr int32 RasterGridWidth = 201",
            "constexpr int32 RasterGridHeight = 201",
            "RasterGridPixels = RasterGridWidth * RasterGridHeight",
            "HasExactBand(0, 1, 16)",
            "HasExactBand(1, 17, 22)",
            "HasExactBand(2, 23, 32)",
            "SampleRasterGrid",
            "OutBand.Num() != RasterGridPixels",
        ):
            self.assertIn(marker, self.editor_cpp)

    def test_strict_tree_union_order_and_pre_hide_readback_are_enforced(self) -> None:
        for marker in (
            "ExpectedV2UmbrellaCount = 272",
            "ExpectedV2ColumnarCount = 232",
            "ExpectedV2DomeCount = 182",
            "ExpectedV2PalmCount = 34",
            "ExpectedV2TreeUnionCount = 720",
            "V2->UmbrellaBroadleafInstances",
            "V2->ColumnarBroadleafInstances",
            "V2->DomeBroadleafInstances",
            "V2->PalmInstances",
            "V3->PreservedV2ColumnarWorldTransforms",
            "V3Dark.Equals(ExactColumnar[Index], 0.001f)",
            "ValidateExactReplacementBeforeHiding",
            "bExactColumnarSourceIndex",
            "Actual.Equals(SourceTransform, 0.001f)",
            "Actual.GetRotation().Equals(SourceTransform.GetRotation()",
            "Row.Form == ETRIADIstanaExploreV4TreeForm::Palm",
            "ExpectedV2ColumnarCount",
            "2428.810830713951",
            "3650.0",
            "2830.0",
        ):
            self.assertIn(marker, self.editor_cpp)
        prehide = self.editor_cpp.index("ValidateExactReplacementBeforeHiding(")
        first_hide = self.editor_cpp.index("HideInheritedVisual(V2Live[Index])")
        self.assertLess(prehide, first_hide)

    def test_formal_beds_are_authored_used_and_validated_after_reload(self) -> None:
        for marker in (
            "FormalBedShrubCount = 256",
            "FormalBedFlowerCount = 128",
            "FormalBedUnderstoreyCount = 192",
            "BuildPairedFormalBedTransforms",
            "bHighAbsoluteX ? 34.125f : 20.75f",
            "bHighAbsoluteX ? 47.25f : 33.875f",
            "bHighY ? 88.125f : 79.75f",
            "bHighY ? 96.25f : 87.875f",
            "Left != Count / 2",
            "Right != Count / 2",
            "OutTransforms[Pair * 2]",
            "OutTransforms[Pair * 2 + 1]",
            "Terrain, FormalBedShrubCount, 0x4757A601",
            "Terrain, FormalBedFlowerCount, 0x4757A602",
            "Terrain, FormalBedUnderstoreyCount, 0x4757A603",
            "ValidateFormalBedPopulation(V4, Error)",
            "256/128/192 mirrored planting-box",
        ):
            self.assertIn(marker, self.editor_cpp)
        self.assertGreaterEqual(
            self.editor_cpp.count("ValidateFormalBedPopulation(V4,"), 2
        )

    def test_close_turf_is_exact_replacement_not_double_layer(self) -> None:
        for marker in (
            "ExpectedCloseTurfCount = 18432",
            "V3->AmbientCgNearTurfInstances",
            "Source->GetInstanceCount() != 18432",
            "OutInput.CloseTurfTransforms",
            "V4->CloseTurfInstances->GetInstanceCount() != ExpectedCloseTurfCount",
            "Actual.Equals(Input.CloseTurfTransforms[Index], 0.001f)",
            "HideInheritedVisual(V3Live[6])",
            "bV3CloseTurfVisualsHidden",
            "M_IPV4_CloseTurf_Wind",
        ):
            self.assertIn(marker, self.all_source)
        populate = self.editor_cpp.index("PopulateDeterministicLandscape(Population")
        hide = self.editor_cpp.index("HideInheritedVisual(V3Live[6])")
        self.assertLess(populate, hide)

    def test_heritage_anchors_trace_only_terrain_and_keep_truth_boundary(self) -> None:
        ids = (
            "HT2018-295",
            "HT2020-313",
            "HT2003-108",
            "HT2003-87",
            "HT2018-292",
            "HT2021-319",
            "HT2008-169",
            "HT2018-298",
            "HT2019-306",
        )
        for record_id in ids:
            self.assertIn(record_id, self.editor_cpp)
        for marker in (
            "Terrain->LineTraceComponent",
            "Hit.Component.Get() != Terrain",
            "ExpectedHeritageAnchorCount = 9",
            "bPositionComesFromPublishedRecord = true",
            "bVisualIsSilhouetteProxy = true",
            "bFormComesFromPublishedQualitativeHint = Index >= 2",
            'TEXT("NOT_STATED_NO_FORM_CLAIM")',
            'TEXT("PUBLIC_PROFILE_QUALITATIVE_HINT")',
            "HeritageAnchorPawnBlockers",
        ):
            self.assertIn(marker, self.all_source)
        self.assertNotRegex(
            self.runtime_h,
            r"TObjectPtr<[^>]+>\s+\w*NativeGirth|float\s+\w*NativeGirth",
        )
        self.assertNotIn("OutRoster.UmbrellaNativeGirth", self.editor_cpp)
        self.assertIn(
            "Anchor.PublishedGirthMeters * CentimetersPerMeter / PI",
            self.runtime_cpp,
        )
        self.assertIn("FVector(UniformScale)", self.runtime_cpp)

        trace = self.editor_cpp[
            self.editor_cpp.index("bool TraceInheritedTerrainOnly(") :
            self.editor_cpp.index("struct FPlanarExclusionTriangle")
        ]
        for readiness_gate in (
            "Terrain->GetWorld()",
            "Terrain->IsRegistered()",
            "Terrain->IsPhysicsStateCreated()",
            "Terrain->GetBodyInstance()",
            "TerrainBody->IsValidBodyInstance()",
            "not registered with a valid physics body",
        ):
            self.assertIn(readiness_gate, trace)
        self.assertLess(
            trace.index("IsValidBodyInstance()"),
            trace.index("Terrain->LineTraceComponent"),
        )

        heritage = self.editor_cpp[
            self.editor_cpp.index("bool BuildSanitizedHeritageAnchors(") :
            self.editor_cpp.index("struct FTextureSourceSpec")
        ]
        for diagnostic in (
            "FString TraceError",
            "Heritage anchor terrain trace failed at index=%d id=%s",
            "localXYCm=(%.2f, %.2f)",
            "startZCm=100000.00 endZCm=-100000.00",
            "Index",
            "Exact.Id",
            "*TraceError",
        ):
            self.assertIn(diagnostic, heritage)

    def test_only_exact_inherited_visuals_and_terrain_slot_zero_change(self) -> None:
        build = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::BuildIstanaExploreV4Map("
            ) :
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::ValidateIstanaExploreV4Map("
            )
        ]
        self.assertIn("for (int32 Index = 0; Index < 4; ++Index)", build)
        self.assertIn("HideInheritedVisual(V2Live[Index])", build)
        self.assertIn("HideInheritedVisual(V3Live[1])", build)
        self.assertIn("HideInheritedVisual(V3Live[6])", build)
        self.assertIn("HideInheritedVisual(V3Live[8])", build)
        self.assertEqual(build.count("HideInheritedVisual("), 4)
        self.assertIn("Scene->TerrainComponent->SetMaterial(0, Lawn)", build)
        self.assertNotIn("OSMContextBuildingsComponent->Set", build)
        self.assertNotIn("TreeTrunkPawnBlockers->Set", build)
        for marker in (
            "ValidateTargetInheritance",
            "V2Live[10]->GetInstanceCount() != ExpectedV2TreeUnionCount",
            "Source.SceneComponents",
            "Source.V2Components",
            "Source.V3Components",
            "bDistantBuildingsPreserved",
        ):
            self.assertIn(marker, self.editor_cpp + self.runtime_h)

    def test_every_v4_hism_is_synchronously_built_before_save_and_cold_reload(self) -> None:
        for marker in (
            "Components.Num() != 16",
            "Component->IsAsyncBuilding()",
            "!Component->IsTreeFullyBuilt()",
            "ValidateV4HismTreesAfterPopulation(V4, Error)",
            "UEditorLoadingAndSavingUtils::SaveMap(",
            "UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)",
            "ValidateV4WorldAgainstPrecomputedPopulation(",
            "EXPLORE_V4_COLD_RELOAD_FAILED_AND_TARGET_DELETED",
        ):
            self.assertIn(marker, self.editor_cpp)
        self.assertIn("BuildTreeIfOutdated(false, true)", self.runtime_cpp)
        self.assertIn(
            "class TRIADSENSORFUSION_API "
            "UTRIADIstanaExploreV4SynchronousHismComponent final",
            self.runtime_h,
        )
        self.assertIn(
            "virtual void OnPostLoadPerInstanceData() override;",
            self.runtime_h,
        )
        self.assertEqual(
            self.runtime_cpp.count(
                "CreateDefaultSubobject<"
                "UTRIADIstanaExploreV4SynchronousHismComponent>"
            ),
            16,
        )
        self.assertNotIn(
            "CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>",
            self.runtime_cpp,
        )
        for marker in (
            "BuildTreeIfOutdated(false, bForceTreeBuild);",
            "bEnableDensityScaling = false;",
            "CurrentDensityScaling = 1.0f;",
            "ActualMesh == Row.ExpectedMesh",
            "ExpectedTotalHismInstanceCount == 21794",
            "ExpectedVisibleHismInstanceCount == 21785",
            "const int32 RequiredVisibleComponents = ExpectedVisibleHismComponentCount -",
            "bRenderVisualsHiddenByV5BSuccessor ? 5 : 0",
            "ExpectedUnderstoreyCount +\n                ExpectedGeometryGrassCount",
            "ISTANA_EXPLORE_V4_HISM_COLD_GATE_PASS components=%d visible=%d "
            "hiddenBlockers=%d nonEmpty=%d totalInstances=%d "
            "visibleInstances=%d autoRebuild=%d async=%d fullyBuilt=%d "
            "renderCountMatches=%d",
        ):
            self.assertIn(marker, self.runtime_cpp)
        editor_helper = self.editor_cpp[
            self.editor_cpp.index("bool ValidateV4HismTreesAfterPopulation(") :
            self.editor_cpp.index("UHierarchicalInstancedStaticMeshComponent* MainTreeComponentForForm(")
        ]
        self.assertNotIn("BuildTreeIfOutdated", editor_helper)
        build_tree = self.editor_cpp.index("ValidateV4HismTreesAfterPopulation(V4, Error)")
        save = self.editor_cpp.index("UEditorLoadingAndSavingUtils::SaveMap(")
        self.assertLess(build_tree, save)

    def test_geometry_backed_placement_exclusions_are_non_vacuous_and_persisted(self) -> None:
        for marker in (
            "struct FPlanarExclusionTriangle",
            "struct FPlanarExclusionIndex",
            "TriangleIndicesByCell",
            "PositionVertexBuffer",
            "IndexBuffer.GetArrayView()",
            "ProjectedDoubleArea > 0.001",
            "DistanceSquaredToSegment",
            "BuildingHeroVisualComponent",
            "BuildingCollisionComponent",
            "HardscapeComponent",
            "ContextBuildingsComponent",
            "OSMContextBuildingsComponent",
            "OsmPublicRoadsComponent",
            "UraIndicativeRoadsComponent",
            "OsmWaterComponent",
            "OutIndex.SourceComponentCount != 8",
            "OutIndex.FormalPlantingTriangleCount != 24",
            'TEXT("M_IPV_Planting")',
            "bAllowFormalPlantingBedSurface",
            "IsBroadFormalLawnClearance",
            "WorldCover != RequiredWorldCoverClass",
            "IsWithinAnyClearance",
            "InOutAcceptedNewCentersCm",
            "ExpectedPlacementAudit.BuildPersistedTag()",
            "MatchesExactWorldTransforms",
            "PlacementPolicyTag",
        ):
            self.assertIn(marker, self.editor_cpp)
        population = self.editor_cpp[
            self.editor_cpp.index("bool BuildV4PopulationInput(") :
            self.editor_cpp.index("bool ValidateFormalBedComponentPrefix(")
        ]
        self.assertGreaterEqual(population.count(", 10,"), 3)
        self.assertGreaterEqual(population.count(", 30,"), 1)
        self.assertNotIn("WorldCover == 10 || WorldCover == 30", population)

    def test_formal_prefixes_cover_only_the_exact_planting_box_insets(self) -> None:
        for marker in (
            "FormalPlantingTriangleCount +=",
            "static_cast<int32>(Section.NumTriangles)",
            "LeftCm, PlanarMarginCm, true",
            "RightCm, PlanarMarginCm, true",
            "CoverageQuadrant = Pair & 3",
            "bHighAbsoluteX ? 34.125f : 20.75f",
            "bHighY ? 88.125f : 79.75f",
            "OutAudit.Accepted += 2",
            "-LeftLocation.X, RightLocation.X",
            "ExpectedFormalPrefix / 2",
            "QuadrantCount != ExpectedFormalPrefix / 8",
        ):
            self.assertIn(marker, self.editor_cpp)
        formal = self.editor_cpp[
            self.editor_cpp.index("bool BuildPairedFormalBedTransforms(") :
            self.editor_cpp.index("bool GatherExactV3CloseTurfTransforms(")
        ]
        self.assertNotIn("22.0f", formal)
        self.assertNotIn("55.0f", formal)
        self.assertNotIn("40.0f", formal)
        self.assertNotIn("85.0f", formal)

    def test_public_zoning_rules_are_directly_pinned_and_consumed(self) -> None:
        for marker in (
            "v4_public_vegetation_zoning.json",
            "VegetationZoningSha",
            "11673",
            "triad.istana_explore_v4_public_vegetation_zoning.v1",
            "ExpectedPlacementRules",
            "reject any point outside class 10",
            "formal-lawn/ceremonial-clearance or existing-tree clearance masks",
        ):
            self.assertIn(marker, self.editor_cpp)

    def test_known_v1_hism_auto_dirty_materials_are_the_only_exception(self) -> None:
        for name in (
            "M_IPV_Bark",
            "M_IPV_LeafDark",
            "M_IPV_LeafMid",
            "M_IPV_LeafLight",
        ):
            self.assertIn(name, self.editor_cpp)
        dirty = self.editor_cpp[
            self.editor_cpp.index("bool HasDisallowedDirtyPackages(") :
            self.editor_cpp.index("struct FProtectedFileDigest")
        ]
        for marker in (
            "DirtyMaps.IsEmpty()",
            "ExactKnownAutoDirtyMaterials.Find(PackageName)",
            "Material->GetOutermost() != Package",
            "!Material->bUsedWithInstancedStaticMeshes",
            "ValidateIstanaPublicViewAssets(V1Report)",
            "ExactKnownAutoDirtyMaterials.Find(PackageName)",
            "RecheckedDirtyContent",
        ):
            self.assertIn(marker, dirty)
        self.assertNotIn("SetDirtyFlag", dirty)
        self.assertNotIn("MarkPackageDirty", dirty)
        self.assertNotIn("SaveLoaded", dirty)
        self.assertIn("do not use Save All", self.editor_cpp)

    def test_protected_snapshots_stream_size_sha_and_preserve_absence(self) -> None:
        snapshot = self.editor_cpp[
            self.editor_cpp.index("struct FProtectedFileDigest") :
            self.editor_cpp.index("int32 CountAssetsUnderV4Root()")
        ]
        for marker in (
            "bool bExisted = false",
            "int64 ByteCount = 0",
            "FString Sha256",
            'Base + TEXT(".uexp")',
            'Base + TEXT(".ubulk")',
            'Base + TEXT(".uptnl")',
            "Record.bExisted = Record.ByteCount >= 0",
            "HashFileSha256",
            "!Record.bExisted && CurrentBytes >= 0",
            "CurrentBytes != Record.ByteCount",
        ):
            self.assertIn(marker, snapshot)
        self.assertNotIn("TArray<uint8> Bytes", snapshot)
        self.assertNotIn("LoadFileToArray", snapshot)

    def test_map_failures_delete_only_the_exact_new_target_and_cold_reload(self) -> None:
        cleanup = self.editor_cpp[
            self.editor_cpp.index("bool DeleteExactV4TargetMapAfterFailure(") :
            self.editor_cpp.index("bool BuildRuntimeAssetRosterAndWindBindings(")
        ]
        for marker in (
            "LoadMap(SourceFilename)",
            "AssetSubsystem->DeleteAsset(DestinationMapPackage)",
            "CollectGarbage(RF_NoFlags)",
            "FindPackage(nullptr, *DestinationMapPackage)",
        ):
            self.assertIn(marker, cleanup)
        build = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::BuildIstanaExploreV4Map("
            ) :
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::ValidateIstanaExploreV4Map("
            )
        ]
        template_transition = build.index("if (!FEditorFileUtils::LoadMap(")
        after_transition = build[template_transition:]
        self.assertGreaterEqual(after_transition.count("FailAndDeleteExactTarget("), 10)
        self.assertIn("LoadMap(\n            SourceFilenameForColdReload)", build)
        self.assertIn("FindObject<UWorld>(nullptr, *DestinationMapObjectPath)", build)
        self.assertIn("FindPackage(nullptr, *DestinationMapPackage)", build)
        self.assertNotIn("UNSAVED_PARTIAL_EXPLORE_V4_MAP", after_transition)
        self.assertNotIn("SAVE_FAILED_PARTIAL_EXPLORE_V4_MAP", after_transition)

    def test_build_phases_entry_source_template_roster_save_and_cold_validation(self) -> None:
        build = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::BuildIstanaExploreV4Map("
            ) :
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::ValidateIstanaExploreV4Map("
            )
        ]
        for marker in (
            "TStrongObjectPtr<UCesiumIonServer> CesiumIonServerKeepAlive",
            "EXPLORE_V4_BUILD_REFUSED_CESIUM_EDITOR_LIFETIME",
            'V2ValidationMapPackage(\n    TEXT("/Game/Maps/Istana_PublicView_Explore_v2"))',
            'V2ValidationMapObjectPath(\n    TEXT("/Game/Maps/Istana_PublicView_Explore_v2.Istana_PublicView_Explore_v2"))',
            "V2ValidationWorld == CurrentEditorWorld",
            "EXPLORE_V4_BUILD_REFUSED_V2_VALIDATION_LIFETIME",
            "UnloadOneExactCleanPackage(",
            "Build-time public V3 validation source",
            "SourceScene->TerrainComponent->GetWorld() != SourceWorld",
            "EXPLORE_V4_BUILD_REFUSED_PREFLIGHT",
            "UnloadCleanV4ValidationPackagesAtEntry(Error)",
            "FEditorFileUtils::LoadMap(",
            "FPackageName::IsTempPackage(TemplatePackageName)",
            "UEditorLoadingAndSavingUtils::SaveMap(",
            "ValidateV4WorldAgainstPrecomputedPopulation(",
        ):
            self.assertIn(marker, self.editor_cpp)

        guard = build.index(
            "TStrongObjectPtr<UCesiumIonServer> CesiumIonServerKeepAlive"
        )
        dirty_gate = build.index("HasDisallowedDirtyPackages(")
        asset_validation = build.index("ValidateIstanaExploreV4Assets(AssetReport)")
        entry_release = build.index("UnloadCleanV4ValidationPackagesAtEntry(Error)")
        source_load = build.index(
            "UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)"
        )
        v3_validation = build.index("ValidateIstanaExploreV3Map(V3Report)")
        v2_release = build.index("UnloadOneExactCleanPackage(", v3_validation)
        exact_v3 = build.index("ValidateExactV3SourceWorld(SourceWorld", v2_release)
        snapshot = build.index("CaptureV3WorldSnapshot(SourceWorld")
        source_actors = build.index("FindV2Landscape(SourceWorld)")
        population = build.index("BuildV4PopulationInput(")
        template_transition = build.index("if (!FEditorFileUtils::LoadMap(")
        source_absence = build.index(
            "FindPackage(nullptr, *SourceMapPackage)", template_transition
        )
        roster = build.index("BuildRuntimeAssetRosterAndWindBindings(")
        compile_roster = build.index("TArray<UStaticMesh*> CompileMeshes")
        compile_gate = build.index("CompileMeshes.Contains(nullptr)")
        compile_finish = build.index(
            "FinishExactStaticMeshRosterMemoryBounded(", compile_roster
        )
        protected_recheck = build.index(
            "ValidateProtectedPackages(Protected, Error)", compile_finish
        )
        target_actors = build.index(
            "ATRIADIstanaExploreV2LandscapeActor* V2 = "
            "FindV2Landscape(TargetWorld)"
        )
        spawn = build.index("TargetWorld->SpawnActor")
        self.assertEqual(
            [
                guard,
                dirty_gate,
                asset_validation,
                entry_release,
                source_load,
                v3_validation,
                v2_release,
                exact_v3,
                snapshot,
                source_actors,
                population,
                template_transition,
                source_absence,
                roster,
                compile_roster,
                compile_gate,
                compile_finish,
                protected_recheck,
                target_actors,
                spawn,
            ],
            sorted(
                [
                    guard,
                    dirty_gate,
                    asset_validation,
                    entry_release,
                    source_load,
                    v3_validation,
                    v2_release,
                    exact_v3,
                    snapshot,
                    source_actors,
                    population,
                    template_transition,
                    source_absence,
                    roster,
                    compile_roster,
                    compile_gate,
                    compile_finish,
                    protected_recheck,
                    target_actors,
                    spawn,
                ]
            ),
        )
        self.assertEqual(1, build.count("UnloadOneExactCleanPackage("))
        self.assertEqual(1, build.count("UnloadCleanV4ValidationPackagesAtEntry("))
        self.assertEqual(1, build.count("BuildRuntimeAssetRosterAndWindBindings("))
        self.assertEqual(1, build.count("BuildV4PopulationInput("))
        self.assertEqual(1, build.count("FinishExactStaticMeshRosterMemoryBounded("))
        self.assertIn("SourceScene->TerrainComponent", build[:template_transition])
        self.assertNotIn("BuildV4PopulationInput(", build[template_transition:])
        self.assertNotIn("BuildRuntimeAssetRosterAndWindBindings(", build[:template_transition])
        self.assertNotIn("DuplicateLoadedAsset", build)
        self.assertNotIn("FinishAllCompilation", build)
        self.assertNotIn("FinishCompilation(CompileMeshes)", build)
        self.assertIn(
            "ValidateV4WorldAgainstPrecomputedPopulation(\n"
            "            TargetWorld,\n"
            "            TemplatePackageName,\n"
            "            SourceSnapshot,\n"
            "            Population,\n"
            "            PlacementAudit,\n"
            "            false,\n"
            "            BeforeSave)",
            build[template_transition:],
        )
        self.assertIn(
            "ValidateV4WorldAgainstPrecomputedPopulation(\n"
            "            Reloaded,\n"
            "            DestinationMapPackage,\n"
            "            SourceSnapshot,\n"
            "            Population,\n"
            "            PlacementAudit,\n"
            "            true,\n"
            "            Persisted)",
            build,
        )
        self.assertNotIn("ValidateV4World(TargetWorld, BeforeSave)", build)
        self.assertNotIn("CleanupWorld", build)

        phases = (
            "ENTRY_BASELINE",
            "ENTRY_ASSET_VALIDATION_COMPLETE",
            "ENTRY_VALIDATION_PACKAGES_RELEASED",
            "V3_SOURCE_LOADED",
            "V3_SNAPSHOT_AND_POPULATION_CAPTURED",
            "V3_TEMPLATE_LOADED_WITH_SOURCE_UNLOADED",
            "TEMPLATE_RUNTIME_ROSTER_READY",
            "TEMPLATE_POPULATION_AND_MUTATION_COMPLETE",
            "TEMPLATE_RENAMED_AND_SAVED_AS_V4",
            "SAVED_V4_UNLOADED_AT_EXACT_V3",
            "SAVED_V4_COLD_RELOADED",
            "SAVED_V4_FULL_COLD_VALIDATION_COMPLETE",
        )
        phase_offsets = [build.index(f'TEXT("{phase}")') for phase in phases]
        self.assertEqual(phase_offsets, sorted(phase_offsets))
        self.assertIn("ISTANA_EXPLORE_V4_BUILD_PHASE_MEMORY", self.editor_cpp)

    def test_entry_asset_release_is_exact_clean_and_proves_absence(self) -> None:
        release = self.editor_cpp[
            self.editor_cpp.index("bool UnloadCleanV4ValidationPackagesAtEntry(") :
            self.editor_cpp.index("bool RollBackNewV4Namespace(")
        ]
        for marker in (
            "ValidateCleanEntryProcessBoundary(",
            "CountAssetsUnderV4Root() != 85",
            "AssetData.Num() != 85",
            "GetAssetsByPath(",
            "TSet<UPackage*> UniqueLoadedPackages",
            "Package->IsDirty()",
            "UPackageTools::UnloadPackages(UnloadParams)",
            "UnloadParams.bUnloadDirtyPackages = false",
            "UnloadParams.bResetTransBuffer = true",
            "CollectGarbage(RF_NoFlags)",
            "FMemory::Trim(true)",
            "FindPackage(nullptr, *SourceMapPackage)",
            "FindObject<UWorld>(nullptr, *SourceMapObjectPath)",
            "FindPackage(nullptr, *DestinationMapPackage)",
            "FindObject<UWorld>(nullptr, *DestinationMapObjectPath)",
            "ValidateProtectedPrewarmSourcesUnloaded(OutError)",
        ):
            self.assertIn(marker, release)
        self.assertNotIn("Data.GetAsset()", release)
        self.assertNotIn("bUnloadDirtyPackages = true", release)
        self.assertLess(
            release.index("UPackageTools::UnloadPackages(UnloadParams)"),
            release.index("CollectGarbage(RF_NoFlags)"),
        )
        self.assertLess(
            release.index("CollectGarbage(RF_NoFlags)"),
            release.index("FindPackage(nullptr, *SourceMapPackage)"),
        )

    def test_map_validation_recomputes_only_when_active_and_unloads_inactive_v3(self) -> None:
        snapshot = self.editor_cpp[
            self.editor_cpp.index(
                "bool CaptureV3WorldSnapshotForV4Validation("
            ) : self.editor_cpp.index("bool ValidateStaticMeshDerivedDataRecord(")
        ]
        for marker in (
            "LoadExact<UWorld>(SourceMapObjectPath)",
            "SourceWorld == CurrentEditorWorld",
            "ValidateExactV3SourceWorld(SourceWorld, ValidationError)",
            "CaptureV3WorldSnapshot(SourceWorld, OutSnapshot, ValidationError)",
            "if (bSourceIsCurrent)",
            "SourceWorld = nullptr",
            "UnloadOneExactCleanPackage(",
            "V4 validation inactive V3 snapshot source",
            "sourceUnload=",
        ):
            self.assertIn(marker, snapshot)
        self.assertLess(
            snapshot.index("CaptureV3WorldSnapshot(SourceWorld"),
            snapshot.index("SourceWorld = nullptr"),
        )
        self.assertLess(
            snapshot.index("SourceWorld = nullptr"),
            snapshot.index("UnloadOneExactCleanPackage("),
        )

        validation = self.editor_cpp[
            self.editor_cpp.index("bool ValidateV4WorldInternal(") :
            self.editor_cpp.index("bool GetLightweightAcceptedV4PlayState(")
        ]
        guard = validation.index(
            "TStrongObjectPtr<UCesiumIonServer> CesiumIonServerKeepAlive"
        )
        snapshot_call = validation.index(
            "CaptureV3WorldSnapshotForV4Validation(\n"
            "                LoadedSourceSnapshot, Error)"
        )
        active_gate = validation.index("World != CurrentEditorWorld")
        recompute = validation.index("BuildV4PopulationInput(")
        self.assertLess(guard, snapshot_call)
        self.assertLess(active_gate, recompute)
        for marker in (
            "ExpectedWorldPackage",
            "PrecomputedSourceSnapshot",
            "PrecomputedPopulation",
            "PrecomputedPlacementAudit",
            "if (!ExpectedSourceSnapshot)",
            "if (!ExpectedPopulationPtr)",
            "inactive/template worlds require the precomputed source population",
            "if (bValidateAssets && !ValidateAllV4AssetsInternal(true, Error))",
            "&SourceSnapshot",
            "&Population",
            "&PlacementAudit",
            "DestinationMapPackage",
            "nullptr",
            "true",
        ):
            self.assertIn(marker, validation)

        precomputed_entry = validation[
            validation.index("bool ValidateV4WorldAgainstPrecomputedPopulation(") :
            validation.index("bool ValidateV4World(UWorld* World")
        ]
        self.assertNotIn("BuildV4PopulationInput(", precomputed_entry)
        self.assertNotIn("ValidateAllV4AssetsInternal(", precomputed_entry)
        self.assertIn("&SourceSnapshot", precomputed_entry)
        self.assertIn("&Population", precomputed_entry)
        self.assertIn("&PlacementAudit", precomputed_entry)
        self.assertIn("bValidateAssets", precomputed_entry)

        public_entry = validation[
            validation.index("bool ValidateV4World(UWorld* World") :
        ]
        self.assertIn("DestinationMapPackage", public_entry)
        self.assertEqual(3, public_entry.count("nullptr"))
        self.assertIn("true", public_entry)

    def test_pie_wrappers_validate_free_roam_manual_exposure_and_bounded_motion(self) -> None:
        for marker in (
            "GetValidatedV4PlayState",
            "ATRIADIstanaFreeRoamPawn",
            "HasExpectedExploreCameraProfile",
            "OutPlayer->GetViewTarget() != OutPawn",
            "OutV4->IsWindRuntimeActive()",
            "PeakStrengthCm < 6.0f",
            "PeakStrengthCm > 120.0f",
            "DeltaCentimeters.Size() > 5000.0",
            "SetActorLocation(\n        Requested, true",
            "SetActorLocationAndRotation",
            "ETeleportType::TeleportPhysics",
            "95000.0",
            "Requested.Z < 50.0",
            "WorldLocationCentimeters.Z > 20000.0",
            "explore_v4_",
            "FScreenshotRequest::RequestScreenshot",
            "QuiesceIstanaExploreV4PlayWorldForStop",
        ):
            self.assertIn(marker, self.editor_cpp)
        self.assertNotIn("ConsoleCommand", self.editor_cpp)
        self.assertNotIn("UEDPIE_", self.editor_cpp)

    def test_full_pie_acceptance_is_not_rehashed_for_each_bounded_qa_action(self) -> None:
        lightweight = self.editor_cpp[
            self.editor_cpp.index("bool GetLightweightAcceptedV4PlayState(") :
            self.editor_cpp.index("bool GetValidatedV4PlayState(")
        ]
        full = self.editor_cpp[
            self.editor_cpp.index("bool GetValidatedV4PlayState(") :
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::ImportIstanaExploreV4Assets("
            )
        ]
        self.assertNotIn("ValidateV4World", lightweight)
        self.assertNotIn("ValidateAllV4AssetsInternal", lightweight)
        self.assertIn("bRequirePriorFullAcceptance", lightweight)
        self.assertIn("FullyAcceptedQaEditorWorld.Get()", lightweight)
        self.assertIn("EditorWorld->GetOutermost()->IsDirty()", lightweight)
        self.assertIn("ValidateV4World(EditorWorld, EditorReport)", full)
        self.assertIn("ValidateExploreV4Landscape(RuntimeReport)", full)
        self.assertEqual(self.editor_cpp.count("GetValidatedV4PlayState("), 2)
        self.assertGreaterEqual(
            self.editor_cpp.count("GetLightweightAcceptedV4PlayState("), 7
        )

    def test_screenshot_wrapper_never_claims_async_completion_or_nonwhite_success(self) -> None:
        capture = self.editor_cpp[
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::CaptureIstanaExploreV4PlayView("
            ) :
            self.editor_cpp.index(
                "bool UTRIADIstanaExploreV4EditorLibrary::\n"
                "    TriggerIstanaExploreV4PlayWindGust("
            )
        ]
        for marker in (
            "WrittenBytes > 0",
            "Queued asynchronous Explore V4 Player0 screenshot",
            "not a capture-success claim",
            "nonwhite pixel-histogram check",
        ):
            self.assertIn(marker, capture)
        self.assertNotIn("Captured validated", capture)


if __name__ == "__main__":
    unittest.main()
