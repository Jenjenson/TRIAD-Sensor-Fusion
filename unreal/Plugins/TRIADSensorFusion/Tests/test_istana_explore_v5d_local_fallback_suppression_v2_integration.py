from __future__ import annotations

import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal/Plugins/TRIADSensorFusion"
RUNTIME = PLUGIN / "Source/TRIADSensorFusion"
EDITOR = PLUGIN / "Source/TRIADSensorFusionEditor"
FACTORY_H = EDITOR / "Private/TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.h"
FACTORY_CPP = EDITOR / "Private/TRIADIstanaExploreV5DCurrentSurroundingsSuppressedAssetFactory.cpp"
ASSET_EDITOR_H = EDITOR / "Public/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h"
ASSET_EDITOR_CPP = EDITOR / "Private/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp"
POLICY_H = RUNTIME / "Public/TRIADIstanaExploreV5DContextPolicyActor.h"
POLICY_CPP = RUNTIME / "Private/TRIADIstanaExploreV5DContextPolicyActor.cpp"
HYBRID_H = EDITOR / "Public/TRIADIstanaExploreV5DHybridEditorLibrary.h"
HYBRID_CPP = EDITOR / "Private/TRIADIstanaExploreV5DHybridEditorLibrary.cpp"
V5_APPEARANCE_CPP = RUNTIME / "Private/TRIADIstanaExploreV5AppearanceActor.cpp"


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


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


class IstanaExploreV5DLocalFallbackSuppressionV2IntegrationTests(
    unittest.TestCase
):
    @classmethod
    def setUpClass(cls) -> None:
        cls.factory_h = read(FACTORY_H)
        cls.factory = read(FACTORY_CPP)
        cls.asset_editor_h = read(ASSET_EDITOR_H)
        cls.asset_editor = read(ASSET_EDITOR_CPP)
        cls.policy_h = read(POLICY_H)
        cls.policy = read(POLICY_CPP)
        cls.hybrid_h = read(HYBRID_H)
        cls.hybrid = read(HYBRID_CPP)
        cls.v5_appearance = read(V5_APPEARANCE_CPP)

    def test_v2_factory_is_independent_hash_pinned_and_three_key_exact(self) -> None:
        for marker in (
            "GetLocalFallbackSuppressedV2AssetRootPath",
            "GetLocalFallbackSuppressedV2MeshObjectPath",
            "CreateFreshLocalFallbackSuppressedV2Asset",
            "ValidateLocalFallbackSuppressedV2Asset",
        ):
            self.assertIn(marker, self.factory_h)
        for marker in (
            "/Game/TRIAD/IstanaPublicViewExploreV5D/LocalFallbackSuppressionV2",
            "SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2",
            "SuppressedV2ExpectedImportedVertexCount = 24468",
            "SuppressedV2ExpectedVertexInstanceCount = 130344",
            "SuppressedV2ExpectedTriangleCount = 43448",
            "SuppressedV2ExpectedObjBytes = 6354063",
            "99175681A1F307D02D8FD01E09850AD017B782BCD4A56043F64B0EA285703110",
            "31A32BCB8DAED756E0B8D90D0EE795A43B389BFB3148322A0FAC761A9BD73477",
            "7D455FE8C1E057F2380BE4941026F511AE5D5A1817795F238496FCEAD77EDE04",
            "EAA570EC3F6DCA0B47CD4F346E73EE879E9C94AC951CE5FE456E3C4DFCFAB6A6",
            "FBE8F7D0C8BB935A3DFC2AE2953B9480902B4CCBFBE7765170FC09AF49081120",
            'TEXT("OSM:way:429681826")',
            'TEXT("OSM_way_429681826_P00")',
            "SourceKeys->Num() != 3",
            "ExpectedTriangleCounts[] = {12, 40, 44}",
        ):
            self.assertIn(marker, self.factory)

    def test_v2_mesh_census_materials_and_provenance_are_fail_closed(self) -> None:
        roster = self.factory[
            self.factory.index("SuppressedV2MaterialSpecs[]") :
            self.factory.index("FString CurrentSourcePath")
        ]
        counts = [
            int(value)
            for value in re.findall(r'\{TEXT\("MAT_[A-Z_]+"\),\s*(\d+),', roster)
        ]
        self.assertEqual(len(counts), 17)
        self.assertEqual(sum(counts), 43448)
        validate = function_body(self.factory, "bool ValidateSuppressedV2Mesh(")
        for marker in (
            "SuppressedV2MeshObjectPath",
            "SuppressedV2ExpectedTriangleCount",
            "ValidateSuppressedV2MeshDescription",
            "ValidateSuppressedV2Uv0",
            "ValidateSuppressedV2Provenance",
            "HasValidNaniteData",
            "GetNavCollision",
        ):
            self.assertIn(marker, validate)
        provenance = function_body(
            self.factory, "bool ValidateSuppressedV2Provenance("
        )
        for marker in (
            "MatchingCount != 1",
            "V1Count != 0",
            "LegacyCanonicalCount != 0",
            "IsCanonicalContract",
        ):
            self.assertIn(marker, provenance)

    def test_v2_editor_import_is_saved_reloaded_cold_validated_and_rollbackable(self) -> None:
        for marker in (
            "ImportIstanaExploreV5DLocalFallbackSuppressionV2",
            "ValidateIstanaExploreV5DLocalFallbackSuppressionV2",
        ):
            self.assertIn(marker, self.asset_editor_h)
        body = function_body(
            self.asset_editor,
            "ImportIstanaExploreV5DLocalFallbackSuppressionV2",
        )
        for marker in (
            "ValidateLocalFallbackSuppressedV2Asset",
            "CreateFreshLocalFallbackSuppressedV2Asset",
            "FScopedSuppressedV2SavedImportRollback",
            "Rollback.Arm()",
            "SaveLoadedAssets(ExactSaveTargets, false)",
            "UPackageTools::ReloadPackages",
            "Rollback.Commit()",
        ):
            self.assertIn(marker, body)
        self.assertLess(body.index("Rollback.Arm()"), body.index("SaveLoadedAssets"))
        self.assertLess(body.index("ValidateLocalFallbackSuppressedV2Asset(ColdReport)"), body.index("Rollback.Commit()"))

    def test_runtime_dispatch_is_exact_for_v1_and_v2(self) -> None:
        for marker in (
            "ExpectedCurrentSurroundingsV2MeshObjectPath",
            "ValidateCurrentSurroundingsV2SuccessorForInheritedScene",
        ):
            self.assertIn(marker, self.policy_h)
        validate = function_body(self.policy, "bool ValidateCurrentSurroundingsMesh(")
        for marker in (
            "CurrentSurroundingsMeshObjectPath",
            "CurrentSurroundingsV2MeshObjectPath",
            "(!bSuppressionV1 && !bSuppressionV2)",
            "GetNumTriangles() != 43492",
            "GetNumTriangles() != 43448",
            "V1ProvenanceCount == 1",
            "V2ProvenanceCount == 1",
            "LegacyProvenanceCount != 0",
        ):
            self.assertIn(marker, validate)
        self.assertNotIn("GetNumTriangles() != 43544", validate)
        for marker in (
            "currentContextSuppressionContract=%s",
            'TEXT("local_fallback_suppression_v2")',
            'TEXT("local_fallback_suppression_v1")',
            "bUsingSuppressionV2 ? 43448 : 43492",
            "bUsingSuppressionV2 ? 96 : 52",
        ):
            self.assertIn(marker, self.policy)

        appearance_dispatch = function_body(
            self.v5_appearance,
            "bool ValidateInheritedSceneOrExactV5DCurrentSuccessor(",
        )
        v1_call = "Policy->ValidateCurrentSurroundingsSuccessorForInheritedScene("
        v2_call = (
            "Policy->ValidateCurrentSurroundingsV2SuccessorForInheritedScene("
        )
        self.assertIn(v1_call, appearance_dispatch)
        self.assertIn(v2_call, appearance_dispatch)
        self.assertLess(
            appearance_dispatch.index(v1_call),
            appearance_dispatch.index(v2_call),
        )
        self.assertNotIn(
            "Policy->ValidateCurrentSurroundingsPresentation(",
            appearance_dispatch,
        )

        v1_validator = function_body(
            self.policy,
            "bool ATRIADIstanaExploreV5DContextPolicyActor::\n"
            "    ValidateCurrentSurroundingsSuccessorForInheritedScene(",
        )
        v2_validator = function_body(
            self.policy,
            "bool ATRIADIstanaExploreV5DContextPolicyActor::\n"
            "    ValidateCurrentSurroundingsV2SuccessorForInheritedScene(",
        )
        self.assertIn(
            "GetPathName() !=\n            CurrentSurroundingsMeshObjectPath",
            v1_validator,
        )
        self.assertIn(
            "GetPathName() !=\n            CurrentSurroundingsV2MeshObjectPath",
            v2_validator,
        )
        self.assertNotIn(
            "/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings/",
            self.policy,
        )

    def test_map_migration_is_predecessor_pinned_single_delta_and_cold_rollbackable(self) -> None:
        endpoint = "ApplyIstanaExploreV5DLocalFallbackSuppressionV2ToLoadedHybridMap"
        validator = "ValidateIstanaExploreV5DLocalFallbackSuppressionV2SuccessorMap"
        self.assertIn(endpoint, self.hybrid_h)
        self.assertIn(validator, self.hybrid_h)
        for marker in (
            "PreLocalFallbackSuppressionV2Bytes = 34992354",
            "859734CB9EFCB429AE7D863E677B7B370CC897EB9C100AACA7AAD6F227805815",
            "V5DLocalFallbackSuppressionV2_20260906",
            "CreateVerifiedPreLocalFallbackSuppressionV2MapBackup",
            "RestoreVerifiedPreLocalFallbackSuppressionV2MapViaSiblingTemp",
        ):
            self.assertIn(marker, self.hybrid)
        body = function_body(self.hybrid, endpoint)
        for marker in (
            "ValidateLocalFallbackSuppressedV2Asset",
            "ValidateExactSuppressionVersion(World, false",
            "CreateVerifiedPreLocalFallbackSuppressionV2MapBackup",
            "Component->SetStaticMesh(V2Mesh)",
            "ValidateExactSuppressionVersion(World, true",
            "UEditorLoadingAndSavingUtils::SaveMap",
            "UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)",
            "IDEMPOTENT_EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V2_ALREADY_VALID",
            "successorReceiptRequiresPinning=true",
            "soleMapDelta=CurrentSurroundingsRenderOnly.StaticMesh",
        ):
            self.assertIn(marker, body)
        self.assertLess(
            body.index("CreateVerifiedPreLocalFallbackSuppressionV2MapBackup"),
            body.index("Component->SetStaticMesh(V2Mesh)"),
        )
        self.assertLess(
            body.index("Component->SetStaticMesh(V2Mesh)"),
            body.index("UEditorLoadingAndSavingUtils::SaveMap"),
        )
        self.assertEqual(body.count("UEditorLoadingAndSavingUtils::SaveMap"), 1)
        self.assertNotIn("MaximumSimultaneousTileLoads =", body)
        self.assertNotIn("PreloadSiblings =", body)

    def test_native_automation_covers_v2_path_census_and_provenance(self) -> None:
        for marker in (
            "Runtime pins the independent suppression-V2 successor path",
            "Suppression V2 successor exact section total",
            "Suppression V2 successor provenance is canonical",
            "Suppression V2 omits exactly 96 triangles",
        ):
            self.assertIn(marker, self.policy)


if __name__ == "__main__":
    unittest.main()
