from __future__ import annotations

import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal/Plugins/TRIADSensorFusion"
RUNTIME = PLUGIN / "Source/TRIADSensorFusion"
EDITOR = PLUGIN / "Source/TRIADSensorFusionEditor"
FACTORY_H = (
    EDITOR
    / "Private/TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.h"
)
CANONICAL_FACTORY_CPP = (
    EDITOR
    / "Private/TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.cpp"
)
SUPPRESSED_FACTORY_CPP = (
    EDITOR
    / "Private/TRIADIstanaExploreV5DCurrentSurroundingsSuppressedAssetFactory.cpp"
)
EDITOR_H = (
    EDITOR
    / "Public/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h"
)
EDITOR_CPP = (
    EDITOR
    / "Private/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp"
)
CANONICAL_PROVENANCE_CPP = (
    RUNTIME
    / "Private/TRIADIstanaExploreV5DCurrentSurroundingsProvenance.cpp"
)
PROVENANCE_H = (
    RUNTIME
    / "Public/TRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance.h"
)
PROVENANCE_CPP = (
    RUNTIME
    / "Private/TRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance.cpp"
)
CPP_TEST = (
    RUNTIME
    / "Private/Tests/TRIADIstanaExploreV5DLocalFallbackSuppressionV1ProvenanceTests.cpp"
)


EXPECTED_MATERIALS = (
    ("MAT_BOTTOM_HIDDEN", 9446, "FallbackRoof"),
    ("MAT_COMMERCIAL_HINT", 2166, "FallbackWall"),
    ("MAT_GENERIC_BUILDING_HINT", 14696, "FallbackWall"),
    ("MAT_HEALTHCARE_HINT", 278, "FallbackWall"),
    ("MAT_HOTEL_HINT", 414, "OfficialWall"),
    ("MAT_INDUSTRIAL_HINT", 8, "FallbackWall"),
    ("MAT_RELIGIOUS_HINT", 150, "OfficialWall"),
    ("MAT_RESIDENTIAL_HINT", 6340, "FallbackWall"),
    ("MAT_ROOF_COMMERCIAL_HINT", 1029, "FallbackRoof"),
    ("MAT_ROOF_GENERIC_BUILDING_HINT", 5452, "FallbackRoof"),
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


class IstanaExploreV5DLocalFallbackSuppressionAssetContractTests(
    unittest.TestCase
):
    @classmethod
    def setUpClass(cls) -> None:
        cls.factory_h = read(FACTORY_H)
        cls.canonical_factory = read(CANONICAL_FACTORY_CPP)
        cls.factory = read(SUPPRESSED_FACTORY_CPP)
        cls.editor_h = read(EDITOR_H)
        cls.editor = read(EDITOR_CPP)
        cls.canonical_provenance = read(CANONICAL_PROVENANCE_CPP)
        cls.provenance_h = read(PROVENANCE_H)
        cls.provenance = read(PROVENANCE_CPP)
        cls.cpp_test = read(CPP_TEST)

    def test_provenance_is_additive_versioned_and_negative_authority(self) -> None:
        canonical_markers = (
            "Triangles = 43544",
            "1612461DBC3FE8C7C760517C0A743B631CDFE23307792AEB8BB04E15855B59A3",
            "UTRIADIstanaExploreV5DCurrentSurroundingsProvenance",
        )
        for marker in canonical_markers:
            self.assertIn(marker, self.canonical_provenance)
        combined = self.provenance_h + self.provenance
        for marker in (
            "UTRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance",
            'ContractVersion = TEXT("local_fallback_suppression_v1")',
            'SourceEpoch = TEXT("2026-08-31")',
            "CanonicalOutputSetSha256",
            "27EF5679CA0010AB646C3DA542B1F0EC6EA163504845BD446FA5D578095AC4FD",
            "CanonicalRenderObjSha256",
            "1612461DBC3FE8C7C760517C0A743B631CDFE23307792AEB8BB04E15855B59A3",
            "FilteredRenderObjSha256",
            "C4781C95EBE88387A57260BD2D8BBC4CD132BA38D26AE007F862BA81FD9F31E9",
            "154BE543F3E7398900F39658FDE41604ABE4642A11EFE097DF1C9C3F8BF1CC61",
            "D8627FECAC184B9B658E8C36048544A026811D56E130336441792FA997A9B88D",
            "DD61A0746D68899217E87470F3C07BA29FB37334DC11120833E15A59732AE56C",
            'TEXT("OSM:way:46521250")',
            'TEXT("OSM:way:1551538490")',
            "CanonicalTriangles = 43544",
            "RenderTriangles = 43492",
            "SuppressedTriangles = 52",
            "ImportedVertices = 24492",
            "ImportedVertices == 24492",
            "ImportedVertexInstances = 130476",
            "bNaniteFullMesh = true",
            "bRasterFallbackFullMesh = true",
            "bCanonicalRenderAndRfInputsHashPinnedUnchanged = true",
            "bProviderOverlapResolved = false",
            "bCollisionNavigationSensorRfAuthority = false",
            "bMeasuredSurveyAsBuiltHyperreal = false",
            "IsCanonicalContract",
        ):
            self.assertIn(marker, combined)
        self.assertNotIn("WITH_EDITORONLY_DATA", combined)
        self.assertNotIn("#if WITH_EDITOR", combined)
        for marker in (
            "Expanded suppression scope invalidates provenance",
            "Claiming provider resolution invalidates provenance",
            "Unsuppressed triangle census invalidates provenance",
            "Canonical reset is deterministic",
        ):
            self.assertIn(marker, self.cpp_test)

    def test_factory_uses_an_independent_one_asset_namespace(self) -> None:
        for marker in (
            "GetLocalFallbackSuppressedAssetRootPath",
            "GetLocalFallbackSuppressedMeshObjectPath",
            "CreateFreshLocalFallbackSuppressedAsset",
            "ValidateLocalFallbackSuppressedAsset",
        ):
            self.assertIn(marker, self.factory_h)
        for marker in (
            "/Game/TRIAD/IstanaPublicViewExploreV5D/LocalFallbackSuppressionV1",
            "SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v1",
            "SuppressedExpectedAssetCount = 1",
            "LocalFallbackSuppressionV1",
            "Actual != Expected",
            "ValidateAsset(CanonicalReport)",
            "canonicalMeshStillValid=true",
            "noActorOrMapChange=true",
        ):
            self.assertIn(marker, self.factory)
        self.assertIn(
            "/Game/TRIAD/IstanaPublicViewExploreV5D/Surroundings",
            self.canonical_factory,
        )
        self.assertNotIn("ContextPolicyActor", self.factory)
        self.assertNotIn("HybridEditorLibrary", self.factory)

    def test_every_generated_source_and_exact_roster_are_fail_closed(self) -> None:
        for size, digest in (
            (
                6356433,
                "C4781C95EBE88387A57260BD2D8BBC4CD132BA38D26AE007F862BA81FD9F31E9",
            ),
            (
                2645,
                "751DE195642892F781731EBD0F9EB3C05F23731CC674F76E7EF335E4F909342A",
            ),
            (
                93879,
                "154BE543F3E7398900F39658FDE41604ABE4642A11EFE097DF1C9C3F8BF1CC61",
            ),
            (
                1761,
                "D8627FECAC184B9B658E8C36048544A026811D56E130336441792FA997A9B88D",
            ),
            (
                6284,
                "68D68B4D906076A49AA070C7341D38245100A7577C4AF7C66C2D62C9A12D0BB5",
            ),
            (
                1796000,
                "2B329516E24C081C7773DB984510EBD1E0C87CDB522CB490170B702D5F564324",
            ),
            (
                216,
                "107E25A2EEB7DFF92356CFBF8E1DA99C3329CE66F75CD1AE15A6EDD8D74F1A47",
            ),
        ):
            self.assertIn(str(size), self.factory)
            self.assertIn(digest, self.factory)
        json_validate = function_body(
            self.factory, "bool ValidateSuppressedJsonContracts("
        )
        for marker in (
            "local_fallback_suppression_metadata.v1",
            "local_fallback_suppression_manifest.v1",
            "outputSetSha256",
            "sourceInputsMatchedBeforeAndAfter",
            "allRemainingFaceLinesRetainedByteForByteAndInSourceOrder",
            'TEXT("OSM:way:46521250")',
            'TEXT("OSM:way:1551538490")',
            'TEXT("OSM_way_46521250_P00")',
            'TEXT("OSM_way_1551538490_P00")',
            "SourceKeys->Num() != 2",
            "Outputs->Num() != 3",
        ):
            self.assertIn(marker, json_validate)

    def test_exact_import_census_materials_and_render_only_policy(self) -> None:
        for marker in (
            "SuppressedExpectedSourceVertexLines = 24522",
            "SuppressedExpectedSourceTextureCoordinateLines = 130632",
            "SuppressedExpectedImportedVertexCount = 24492",
            "SuppressedExpectedVertexInstanceCount = 130476",
            "SuppressedExpectedTriangleCount = 43492",
            "SuppressedExpectedMaterialCount = 17",
        ):
            self.assertIn(marker, self.factory)
        positions: list[int] = []
        for name, triangles, role in EXPECTED_MATERIALS:
            literal = (
                f'{{TEXT("{name}"), {triangles}, '
                f"ESuppressedV5CMaterialRole::{role}}}"
            )
            self.assertIn(literal, self.factory)
            positions.append(self.factory.index(literal))
        self.assertEqual(positions, sorted(positions))
        self.assertEqual(43492, sum(row[1] for row in EXPECTED_MATERIALS))

        task = function_body(self.factory, "UAssetImportTask* MakeSuppressedImportTask()")
        for marker in (
            "bImportMaterials = false",
            "bImportTextures = false",
            "ImportUniformScale = 100.0f",
            "bCombineMeshes = true",
            "bReorderMaterialToFbxOrder = true",
            "bAutoGenerateCollision = false",
            "bGenerateLightmapUVs = false",
            "FBXNIM_ComputeNormals",
            "bBuildNanite = true",
            "bReplaceExisting = false",
            "bSave = false",
        ):
            self.assertIn(marker, task)
        render_only = function_body(
            self.factory, "void MakeSuppressedRenderOnly("
        )
        for marker in (
            "RestoreIstanaLocalHandedness",
            "FVector(1.0, -1.0, 1.0)",
            "FallbackPercentTriangles = 1.0f",
            "FallbackRelativeError = 0.0f",
            "RemoveSimpleCollision",
            "CTF_UseSimpleAsComplex",
            "MarkAsNotHavingNavigationData",
        ):
            self.assertIn(marker, render_only)
        mesh_validate = function_body(
            self.factory, "bool ValidateSuppressedMesh("
        )
        for marker in (
            "HasValidNaniteData",
            "GetNavCollision",
            "ValidateSuppressedMeshDescription",
            "ValidateSuppressedUv0",
            "ValidateSuppressedProvenance",
            "SuppressedMaterialSpecs[Section.MaterialIndex].Triangles",
        ):
            self.assertIn(marker, mesh_validate)

    def test_import_is_idempotent_saved_reloaded_and_cold_validated(self) -> None:
        create = function_body(
            self.factory, "bool CreateFreshLocalFallbackSuppressedAsset("
        )
        self.assertLess(
            create.index("ValidateLocalFallbackSuppressedAsset"),
            create.index("AssetTools.ImportAssetTasks"),
        )
        for marker in (
            "ValidateSuppressedSourceHashes",
            "ValidateSuppressedJsonContracts",
            "FScopedSuppressedFreshRollback",
            "NormalizeSuppressedMaterials",
            "StampSuppressedProvenance",
            "ValidateSuppressedInternal(false",
        ):
            self.assertIn(marker, create)

        import_body = function_body(
            self.editor,
            "ImportIstanaExploreV5DLocalFallbackSuppressedAsset",
        )
        for marker in (
            "IDEMPOTENT_EXPLORE_V5D_LOCAL_FALLBACK_SUPPRESSION_V1_ASSET_ALREADY_VALID",
            "CreateFreshLocalFallbackSuppressedAsset",
            "FScopedSuppressedSavedImportRollback Rollback",
            "Rollback.Arm()",
            "ExactSaveTargets = {FreshAsset}",
            "SaveLoadedAssets(ExactSaveTargets, false)",
            "UPackageTools::ReloadPackages",
            "ValidateLocalFallbackSuppressedAsset(ColdReport)",
            "Rollback.Commit()",
            "provider overlap remains unresolved",
            "No actor or map was changed",
        ):
            self.assertIn(marker, import_body)
        rollback = function_body(
            self.editor, "class FScopedSuppressedSavedImportRollback final"
        )
        for marker in (
            "if (!bArmed || bCommitted || !Subsystem)",
            "Subsystem->DeleteAsset(",
            "GetLocalFallbackSuppressedMeshObjectPath()",
            "LOCAL_FALLBACK_SUPPRESSION_V1_SAVED_ROLLBACK_INCOMPLETE",
        ):
            self.assertIn(marker, rollback)
        self.assertLess(
            import_body.index("Rollback.Arm()"),
            import_body.index("SaveLoadedAssets(ExactSaveTargets, false)"),
        )
        self.assertLess(
            import_body.index("ValidateLocalFallbackSuppressedAsset(ColdReport)"),
            import_body.index("Rollback.Commit()"),
        )
        for marker in (
            "ImportIstanaExploreV5DLocalFallbackSuppressedAsset",
            "ValidateIstanaExploreV5DLocalFallbackSuppressedAsset",
        ):
            self.assertIn(marker, self.editor_h)


if __name__ == "__main__":
    unittest.main()
