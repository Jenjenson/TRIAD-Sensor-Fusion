from __future__ import annotations

import hashlib
import json
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal/Plugins/TRIADSensorFusion"
RUNTIME = PLUGIN / "Source/TRIADSensorFusion"
EDITOR = PLUGIN / "Source/TRIADSensorFusionEditor"
PACKAGE = PLUGIN / "Tools/IstanaExploreV5D/OuterGroundFallback"
GENERATED = PACKAGE / "Generated"
RIM_CANDIDATE_CONTRACT = (
    REPO
    / "unreal/SourceAssets/IstanaPublicViewExploreV5D/Terrain/"
    "OuterContextRimSeamCandidate/outer_context_rim_seam_candidate.v1.json"
)

PROVENANCE_H = (
    RUNTIME
    / "Public/TRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance.h"
)
PROVENANCE_CPP = (
    RUNTIME
    / "Private/TRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance.cpp"
)
CPP_TEST = (
    RUNTIME
    / "Private/Tests/TRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenanceTests.cpp"
)
FACTORY_H = (
    EDITOR
    / "Private/TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory.h"
)
FACTORY_CPP = (
    EDITOR
    / "Private/TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory.cpp"
)
EDITOR_H = (
    EDITOR
    / "Public/TRIADIstanaExploreV5DOuterGroundLoadingFallbackEditorLibrary.h"
)
EDITOR_CPP = (
    EDITOR
    / "Private/TRIADIstanaExploreV5DOuterGroundLoadingFallbackEditorLibrary.cpp"
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


class OuterGroundLoadingFallbackAssetContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.provenance_h = read(PROVENANCE_H)
        cls.provenance = read(PROVENANCE_CPP)
        cls.cpp_test = read(CPP_TEST)
        cls.factory_h = read(FACTORY_H)
        cls.factory = read(FACTORY_CPP)
        cls.editor_h = read(EDITOR_H)
        cls.editor = read(EDITOR_CPP)
        cls.lock = json.loads(
            read(
                GENERATED
                / "IstanaPublicViewV5DOuterGroundLoadingFallback.acceptance.lock.json"
            )
        )
        cls.rim_candidate = json.loads(read(RIM_CANDIDATE_CONTRACT))

    def test_cooked_provenance_is_exact_and_negative_authority(self) -> None:
        combined = self.provenance_h + self.provenance
        for marker in (
            "UTRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance",
            'ContractVersion = TEXT("outer_ground_loading_fallback_v1")',
            "023068051DBFC7E146EB07548ABD322BC42F7AED439A9717FAFD7829B8805054",
            "SourceCornerVertices = 3840",
            "SourceTextureCoordinates = 3840",
            "SourceNormals = 3840",
            "Triangles = 1280",
            "AngularSectors = 128",
            "RadialBands = 5",
            "InnerRadiusMetres = 1000.0",
            "OuterRadiusMetres = 1250.0",
            'MaterialSlot = TEXT("M_IPV5D_OuterGroundLoadingFallback")',
            "bExactInheritedInnerSeam = true",
            "bPhysicalMetreUv0 = true",
            "bIdentityImportScale = true",
            "bOneLod = true",
            "bNaniteFullMesh = true",
            "bRasterFallbackFullMesh = true",
            "bCollisionAuthority = false",
            "bNavigationAuthority = false",
            "bShadowAuthority = false",
            "bDistanceFieldAuthority = false",
            "bTerrainSurveyOrAsBuiltAuthority = false",
            "bSensorOcclusionOrRfAuthority = false",
            "bLiveMapOrProviderPolicyIntegrationIncluded = false",
            "IsCanonicalContract",
        ):
            self.assertIn(marker, combined)
        self.assertNotIn("#if WITH_EDITOR", combined)
        self.assertNotIn("WITH_EDITORONLY_DATA", combined)
        for marker in (
            "Source hash drift is rejected",
            "Topology drift is rejected",
            "Material drift is rejected",
            "Collision authority escalation is rejected",
            "Live-integration claim escalation is rejected",
            "Canonical reset is deterministic",
        ):
            self.assertIn(marker, self.cpp_test)

    def test_direct_source_is_current_while_factory_keeps_native_replay_closure(self) -> None:
        records = list(self.lock["files"])
        lock_path = (
            GENERATED
            / "IstanaPublicViewV5DOuterGroundLoadingFallback.acceptance.lock.json"
        )
        records.append(
            {
                "path": lock_path.name,
                "scope": "GENERATED",
                "bytes": lock_path.stat().st_size,
                "sha256": sha256(lock_path),
            }
        )
        self.assertEqual(len(records), 7)
        changed_source_only = {
            "outer_ground_loading_fallback_v1.contract.json",
            "IstanaPublicViewV5DOuterGroundLoadingFallback.manifest.json",
            "IstanaPublicViewV5DOuterGroundLoadingFallback.acceptance.lock.json",
        }
        for record in records:
            root = PACKAGE if record["scope"] == "PACKAGE" else GENERATED
            path = root / record["path"]
            self.assertEqual(path.stat().st_size, record["bytes"], path)
            digest = sha256(path)
            self.assertEqual(digest, record["sha256"].upper(), path)
            self.assertIn(str(record["bytes"]), self.factory)
            if record["path"] in changed_source_only:
                self.assertNotIn(digest, self.factory)
            else:
                self.assertIn(digest, self.factory)

        drift = self.rim_candidate["outerGroundCookedProvenanceDrift"]
        historical = drift["historicalNativeFactoryClosure"]
        current = drift["directSourceExpected"]
        for key, digest in historical.items():
            self.assertIn(digest, self.factory, key)
            self.assertNotEqual(digest, current[key], key)
        for digest in current.values():
            self.assertNotIn(digest, self.factory)

    def test_factory_is_isolated_and_targets_the_exact_two_assets(self) -> None:
        combined = self.factory_h + self.factory
        for marker in (
            "/Game/TRIAD/IstanaPublicViewExploreV5D/OuterGroundLoadingFallback",
            "SM_IPV5D_OuterGroundLoadingFallback_Render",
            "M_IPV5D_OuterGroundLoadingFallback",
            "ExpectedAssetCount = 2",
            "GetMeshObjectPath",
            "GetMaterialObjectPath",
            "CreateFreshAssets",
            "ValidateAssets",
            "Actual != Expected",
            "FScopedFreshRollback",
            "PKG_NewlyCreated",
            "DeleteObjectsUnchecked",
            "noActorOrMapChange=true",
            "liveMapOrProviderPolicyIntegrationIncluded=false",
        ):
            self.assertIn(marker, combined)
        for forbidden in (
            "ContextPolicyActor",
            "HybridEditorLibrary",
            "SourceAssets",
            "D:/",
            "D:\\",
        ):
            self.assertNotIn(forbidden, combined + self.editor)

    def test_factory_rollback_rediscovers_every_registered_root_asset(self) -> None:
        rollback = function_body(self.factory, "~FScopedFreshRollback()")
        create = function_body(self.factory, "bool CreateFreshAssets(")
        create_material = function_body(
            self.factory, "UMaterial* CreateMaterial("
        )

        # The rollback owns the transaction's previously-proven-empty root,
        # not just successful return pointers. Two recursive scans prove both
        # discovery and post-delete emptiness.
        self.assertEqual(rollback.count("GatherRootAssets"), 2)
        self.assertIn("TArray<FAssetData> Registered", rollback)
        self.assertIn("for (const FAssetData& Asset : Registered)", rollback)
        self.assertIn("UObject* Object = Asset.GetAsset()", rollback)
        self.assertIn("PKG_NewlyCreated", rollback)
        self.assertIn("!FPackageName::DoesPackageExist", rollback)
        self.assertIn("ObjectTools::DeleteObjectsUnchecked", rollback)
        self.assertIn("TArray<FAssetData> Remaining", rollback)
        self.assertIn("!Remaining.IsEmpty()", rollback)
        self.assertIn("ROLLBACK_DISCOVERY_FAILED", rollback)
        self.assertIn("ROLLBACK_ROOT_NOT_EMPTY", rollback)
        self.assertNotIn("for (UObject* Object : Created)", rollback)
        self.assertNotIn("TArray<UObject*>&", rollback)
        self.assertNotIn("Task->GetObjects", rollback)
        self.assertNotIn("MeshObjectPath", rollback)
        self.assertNotIn("MaterialObjectPath", rollback)

        first_scan = rollback.index("GatherRootAssets")
        iteration = rollback.index("for (const FAssetData& Asset : Registered)")
        deletion = rollback.index("DeleteObjectsUnchecked")
        verification_scan = rollback.rindex("GatherRootAssets")
        verification = rollback.index("!Remaining.IsEmpty()")
        self.assertLess(first_scan, iteration)
        self.assertLess(iteration, deletion)
        self.assertLess(deletion, verification_scan)
        self.assertLess(verification_scan, verification)

        # The guard begins before either registration-capable operation. A
        # material can be registered before graph validation returns null;
        # importer outputs are likewise covered without result filtering.
        guard = create.index("FScopedFreshRollback Rollback(OutError)")
        material_call = create.index("CreateMaterial(AssetTools, OutError)")
        import_call = create.index("AssetTools.ImportAssetTasks({Task})")
        commit = create.index("Rollback.Commit()")
        self.assertLess(guard, material_call)
        self.assertLess(material_call, import_call)
        self.assertLess(import_call, commit)
        self.assertNotIn("Created.Add", create)
        self.assertNotIn("TArray<UObject*> Created", create)
        self.assertLess(
            create_material.index("AssetTools.CreateAsset"),
            create_material.index("if (!Material || !Data"),
        )
        self.assertIn("return nullptr", create_material)

    def test_import_mesh_contract_is_identity_exact_and_render_only(self) -> None:
        task = function_body(self.factory, "UAssetImportTask* MakeImportTask()")
        for marker in (
            "bImportMaterials = false",
            "bImportTextures = false",
            "ImportUniformScale = 1.0f",
            "bCombineMeshes = true",
            "bImportMeshLODs = false",
            "bAutoGenerateCollision = false",
            "bGenerateLightmapUVs = false",
            "FBXNIM_ImportNormals",
            "bBuildNanite = true",
            "bRemoveDegenerates = false",
            "bReplaceExisting = false",
            "bSave = false",
        ):
            self.assertIn(marker, task)
        normalize = function_body(self.factory, "bool NormalizeMesh(")
        for marker in (
            "const FMeshDescription* Description",
            "Description->Vertices().Num() != ExpectedSourceCornerCount",
            "Description->VertexInstances().Num() != ExpectedSourceCornerCount",
            "Description->Triangles().Num() != ExpectedTriangleCount",
            "Description->Polygons().Num() != ExpectedTriangleCount",
            "Description->PolygonGroups().Num() != ExpectedMaterialCount",
            "BuildScale3D = FVector::OneVector",
            "bUseFullPrecisionUVs = true",
            "bRecomputeNormals = false",
            "bGenerateMeshDistanceField = false",
            "KeepPercentTriangles = 1.0f",
            "FallbackPercentTriangles = 1.0f",
            "FallbackRelativeError = 0.0f",
            "bEnableCollision = false",
            "bCastShadow = false",
            "bAffectDistanceFieldLighting = false",
            "RemoveSimpleCollision",
            "CTF_UseSimpleAsComplex",
            "MarkAsNotHavingNavigationData",
        ):
            self.assertIn(marker, normalize)
        # Fresh Nanite imports may expose a reduced default raster fallback
        # until NormalizeMesh applies the canonical 100% fallback settings and
        # rebuilds. Source topology is authoritative at this pre-build gate.
        pre_rebuild = normalize[: normalize.index("Mesh->NaniteSettings.bEnabled")]
        self.assertNotIn("NumTriangles != ExpectedTriangleCount", pre_rebuild)
        self.assertLess(
            normalize.index("Description->Triangles().Num()"),
            normalize.index("FallbackPercentTriangles = 1.0f"),
        )
        self.assertLess(
            normalize.index("FallbackPercentTriangles = 1.0f"),
            normalize.index("Mesh->PostEditChange()"),
        )
        validate = function_body(self.factory, "bool ValidateMesh(")
        for marker in (
            "ExpectedSourceCornerCount = 3840",
            "ExpectedRenderVertexCount = 768",
            "ExpectedTriangleCount",
            "ExpectedBoundsMin",
            "ExpectedBoundsMax",
            "ValidateDescription",
            "ValidateUv0",
            "HasValidNaniteData",
            "GetNavCollision",
            "bGenerateMeshDistanceField",
            "bCastShadow",
            "bAffectDistanceFieldLighting",
            "ValidateProvenance",
        ):
            self.assertIn(marker, self.factory if marker.startswith("Expected") else validate)
        uv0 = function_body(self.factory, "bool ValidateUv0(")
        self.assertIn(
            "Buffer.GetNumVertices() != ExpectedRenderVertexCount", uv0
        )
        self.assertIn("vertices=%u expected=%d", uv0)
        self.assertIn("renderVertices=%d", self.factory)

    def test_material_is_exact_owned_macro_varied_and_non_authoritative(self) -> None:
        create = function_body(self.factory, "UMaterial* CreateMaterial(")
        validate = function_body(self.factory, "bool ValidateMaterial(")
        combined = self.factory
        for marker in (
            "MaterialRoot",
            "OuterGround.AbsoluteWorldPosition",
            "OuterGround.MacroNeutralResponse",
            "WorldPosition.xy / 3700.0",
            "float3(0.19, 0.23, 0.13)",
            "Expressions.Num() != 6",
            "MD_Surface",
            "BLEND_Opaque",
            "MSM_DefaultLit",
            "bUsedWithNanite = true",
            "bCastRayTracedShadows = false",
            "MaxWorldPositionOffsetDisplacement = 0.0f",
            "PhysMaterial = nullptr",
            "PhysMaterialMask = nullptr",
            "PhysicalMaterialMap",
            "RenderTracePhysicalMaterialOutputs.Reset()",
            "HasExplicitPhysicalMaterialBinding",
            "WorldPositionOffset.Expression",
            "Displacement.Expression",
            "PixelDepthOffset.Expression",
        ):
            self.assertIn(marker, combined)
        # GetPhysicalMaterial() resolves the engine's default physical material
        # when no explicit binding exists, so it cannot prove this negative
        # authority contract. Inspect every explicit binding instead.
        self.assertNotIn("GetPhysicalMaterial()", validate)
        for candidate in (
            "WorldPositionCandidate",
            "CustomCandidate",
            "MaskCandidate",
            "ConstantCandidate",
        ):
            self.assertIn(candidate, validate)
        self.assertNotIn("const auto* Candidate", validate)

    def test_endpoint_is_cold_idempotent_and_rolls_back_exact_assets(self) -> None:
        combined = self.editor_h + self.editor
        for marker in (
            "ImportIstanaExploreV5DOuterGroundLoadingFallbackAssets",
            "ValidateIstanaExploreV5DOuterGroundLoadingFallbackAssets",
            "IDEMPOTENT_EXPLORE_V5D_OUTER_GROUND_LOADING_FALLBACK_ASSETS_ALREADY_VALID",
            "ExactSaveTargets = {FreshMaterial, FreshMesh}",
            "SaveLoadedAssets(ExactSaveTargets, false)",
            "UPackageTools::ReloadPackages",
            "ValidateAssets(ColdReport)",
            "FScopedSavedImportRollback",
            "DeleteAsset",
            "Rollback.Commit",
            "changes no actor, map, provider policy, sensor or RF input",
        ):
            self.assertIn(marker, combined)
        import_body = function_body(
            self.editor,
            "ImportIstanaExploreV5DOuterGroundLoadingFallbackAssets",
        )
        self.assertLess(
            import_body.index("ValidateAssets(ExistingReport)"),
            import_body.index("CreateFreshAssets"),
        )
        self.assertLess(
            import_body.index("UPackageTools::ReloadPackages"),
            import_body.index("ValidateAssets(ColdReport)"),
        )


if __name__ == "__main__":
    unittest.main()
