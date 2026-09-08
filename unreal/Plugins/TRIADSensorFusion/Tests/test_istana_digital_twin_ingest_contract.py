from __future__ import annotations

import json
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
SOURCE_ROOT = REPO / "unreal" / "SourceAssets" / "IstanaDigitalTwin"
SCHEMA_PATH = SOURCE_ROOT / "Schemas" / "triad.istana_digital_twin_release.v1.schema.json"
EXAMPLE_PATH = SOURCE_ROOT / "Examples" / "IstanaDigitalTwinRelease.example.json"
VALIDATOR_PATH = SOURCE_ROOT / "validate_release.py"
GEOSPATIAL_VALIDATOR_PATH = SOURCE_ROOT / "validate_geospatial_authority.py"
GEOSPATIAL_SCHEMA_PATH = SOURCE_ROOT / "Schemas" / "triad.istana_geospatial_authority.v1.schema.json"
SCRIPT_PATH = REPO / "scripts" / "Validate-IstanaDigitalTwinRelease.ps1"
GEOSPATIAL_SCRIPT_PATH = REPO / "scripts" / "Validate-IstanaGeospatialAuthorityV1.ps1"
GEOSPATIAL_DOC_PATH = REPO / "docs" / "ISTANA_GEOSPATIAL_AUTHORITY_V1.md"
SOURCE_README_PATH = SOURCE_ROOT / "README.md"
RELEASE_IGNORE_PATH = SOURCE_ROOT / ".gitignore"
EDITOR_ROOT = REPO / "unreal" / "Plugins" / "TRIADSensorFusion" / "Source" / "TRIADSensorFusionEditor"
EDITOR_HEADER = (EDITOR_ROOT / "Public" / "TRIADIstanaEditorLibrary.h").read_text(encoding="utf-8")
EDITOR_CPP = (EDITOR_ROOT / "Private" / "TRIADIstanaEditorLibrary.cpp").read_text(encoding="utf-8")


class IstanaDigitalTwinIngestContractTests(unittest.TestCase):
    def test_schema_and_empty_example_are_explicitly_versioned(self) -> None:
        schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
        example = json.loads(EXAMPLE_PATH.read_text(encoding="utf-8"))
        self.assertEqual(schema["$schema"], "https://json-schema.org/draft/2020-12/schema")
        self.assertEqual(example["schemaVersion"], "triad.istana_digital_twin_release.v1")
        self.assertEqual(example["claimStatus"], "SCHEMA_ONLY")
        self.assertEqual(example["releaseState"], "DRAFT")
        self.assertIn(
            "APPROVED_FOR_STAGE0_INTEGRITY_REVIEW",
            schema["properties"]["releaseState"]["enum"],
        )
        self.assertNotIn(
            "APPROVED_FOR_" + "UNREAL_IMPORT",
            schema["properties"]["releaseState"]["enum"],
        )
        self.assertEqual(example["assets"], [])
        self.assertEqual(example["groundControlPoints"], [])
        self.assertIsNone(example["declaredAccuracy"])
        self.assertIsNone(example["qaResults"])

    def test_strict_validator_requires_authority_and_real_geometry_roles(self) -> None:
        validator = VALIDATOR_PATH.read_text(encoding="utf-8")
        for contract in (
            "APPROVED_FOR_STAGE0_INTEGRITY_REVIEW",
            "SURVEY_CONTROLLED",
            "AS_BUILT_AUTHORIZED",
            "BUILDING_RENDER_MESH",
            "BUILDING_COLLISION_MESH",
            "SOURCE_POINT_CLOUD",
            "SOURCE_POINT_CLOUD_ARCHIVE",
            "TERRAIN_DTM_SOURCE",
            "TERRAIN_RENDER_MESH",
            "VEGETATION_INVENTORY",
            "VEGETATION_GEOMETRY_LIBRARY",
            "CONTEXT_GEOMETRY",
            "REFERENCE_PHOTO",
            "REFERENCE_PHOTO_RAW",
            "MATERIAL_TEXTURE",
            "COLOR_CHART_CAPTURE",
            "CAMERA_COLOR_PROFILE",
            "LIGHTING_MEASUREMENT",
            "SURVEY_AUTHORITY",
            "DATA_RIGHTS",
            "SECURITY_REVIEW",
            "PROJECT_ACCEPTANCE",
            "sourceToUnrealMatrix4x4",
            "transformationToWgs84Ellipsoid",
            "materialColorDeltaE2000Median",
            "materialColorDeltaE2000P95",
            "materialCalibration",
            "compute_acceptance_payload_sha256",
            "COLOR_QA_EVIDENCE",
            "COLOR_QA_RESULT",
            "contextAoiBoundsSourceCrs",
        ):
            self.assertIn(contract, validator)

    def test_strict_validator_rejects_shortcuts_and_verifies_bytes(self) -> None:
        validator = VALIDATOR_PATH.read_text(encoding="utf-8")
        for forbidden in (
            "SM_IstanaExterior",
            "GENERATED_REFERENCE_MESH",
            "BILLBOARD",
            "IMAGE_PLANE",
            "STREAMED_UNVERIFIED",
            "SRTM_FALLBACK",
        ):
            self.assertIn(forbidden, validator)
        self.assertIn("hashlib.sha256", validator)
        self.assertIn("REJECTED_REFERENCE_HASHES", validator)
        self.assertIn("4c822bb2c85451136c41fab0a362c7d2f0ca6663c0ae44cfecbe8f73e6a83b91", validator)
        self.assertIn("byte hash mismatch", validator)
        self.assertIn("renaming cannot create survey authority", validator)
        self.assertIn("at least four surveyed CONTROL", validator)
        self.assertIn("at least two independent withheld CHECK", validator)
        self.assertIn("calibrated reference views", validator)

    def test_powershell_wrapper_separates_schema_only_from_strict_releases(self) -> None:
        script = SCRIPT_PATH.read_text(encoding="utf-8")
        self.assertIn("[switch] $SchemaOnly", script)
        self.assertIn("IstanaDigitalTwin", script)
        self.assertIn("$resolvedReleaseRoot", script)
        self.assertIn("$resolvedExampleRoot", script)
        self.assertIn("--schema-only", script)
        self.assertIn("not an import-ready or fidelity-authority result", script)
        self.assertIn("remains NON_IMPORTABLE", script)
        self.assertNotIn("Invoke-RestMethod", script)

    def test_geospatial_authority_v1_is_downstream_hash_bound_and_non_importable(self) -> None:
        validator = GEOSPATIAL_VALIDATOR_PATH.read_text(encoding="utf-8")
        schema = json.loads(GEOSPATIAL_SCHEMA_PATH.read_text(encoding="utf-8"))
        wrapper = GEOSPATIAL_SCRIPT_PATH.read_text(encoding="utf-8")
        documentation = GEOSPATIAL_DOC_PATH.read_text(encoding="utf-8")
        readme = SOURCE_README_PATH.read_text(encoding="utf-8")

        self.assertEqual(
            schema["properties"]["schemaVersion"]["const"],
            "triad.istana_geospatial_authority.v1",
        )
        for contract in (
            "validate_release.validate_manifest",
            "parse_geotiff",
            "analyze_composite_coverage",
            "analyze_seam",
            "analyze_glb",
            "receiptPayloadSha256",
            "GEOSPATIAL_AUTHORITY_V1_ADMITTED_NON_IMPORTABLE",
            "placeholder/development markers cannot become authority",
            "changed during semantic admission",
        ):
            self.assertIn(contract, validator)
        self.assertIn("will not be overwritten", wrapper)
        self.assertIn("same controlled release directory", wrapper)
        self.assertNotIn("Invoke-RestMethod", wrapper)
        self.assertIn("unlock Unreal\nimport", documentation)
        self.assertIn("Passing remains `NON_IMPORTABLE`", readme)

    def test_release_payloads_are_quarantined_and_stage0_never_claims_semantic_importability(self) -> None:
        ignore = RELEASE_IGNORE_PATH.read_text(encoding="utf-8")
        readme = SOURCE_README_PATH.read_text(encoding="utf-8")
        validator = VALIDATOR_PATH.read_text(encoding="utf-8")
        self.assertIn("Releases/*", ignore)
        self.assertIn("!Releases/README.md", ignore)
        self.assertIn("STAGE0_RELEASE_INTEGRITY_VALIDATED_NON_IMPORTABLE", validator)
        self.assertIn("does **not** prove that a GLB", readme)
        self.assertIn("cryptographically verify", readme)
        self.assertIn("ImportIstanaDigitalTwinRelease", readme)
        self.assertIn("remain hard locked", readme)

    def test_unreal_hooks_are_metadata_only_and_import_build_are_locked(self) -> None:
        for function_name in (
            "ValidateIstanaDigitalTwinReleaseMetadata",
            "ImportIstanaDigitalTwinRelease",
            "BuildIstanaDigitalTwinMapV3",
        ):
            self.assertIn(function_name, EDITOR_HEADER)
            self.assertIn(function_name, EDITOR_CPP)

        import_start = EDITOR_CPP.index("bool UTRIADIstanaEditorLibrary::ImportIstanaDigitalTwinRelease(")
        build_start = EDITOR_CPP.index("bool UTRIADIstanaEditorLibrary::BuildIstanaDigitalTwinMapV3(")
        legacy_build_start = EDITOR_CPP.index("bool UTRIADIstanaEditorLibrary::BuildIstanaStudyMap(")
        import_body = EDITOR_CPP[import_start:build_start]
        build_body = EDITOR_CPP[build_start:legacy_build_start]
        self.assertIn("DIGITAL_TWIN_IMPORT_LOCKED", import_body)
        self.assertIn("return false", import_body)
        self.assertIn("DIGITAL_TWIN_V3_BUILD_LOCKED", build_body)
        self.assertIn("return false", build_body)
        for mutator in (
            "CreateAsset",
            "ImportAssetTasks",
            "SaveMap",
            "LoadMap",
            "NewMapFromTemplate",
            "SpawnActor",
            "Delete",
        ):
            self.assertNotIn(mutator, import_body)
            self.assertNotIn(mutator, build_body)

    def test_v2_contract_is_not_a_digital_twin_fallback(self) -> None:
        metadata_start = EDITOR_CPP.index("bool UTRIADIstanaEditorLibrary::ValidateIstanaDigitalTwinReleaseMetadata(")
        legacy_build_start = EDITOR_CPP.index("bool UTRIADIstanaEditorLibrary::BuildIstanaStudyMap(")
        hooks = EDITOR_CPP[metadata_start:legacy_build_start]
        self.assertIn("current generated/refined mesh is not a permitted substitute", hooks)
        self.assertIn("never opens, duplicates, saves, or modifies", hooks)
        self.assertNotIn("BuildIstanaRuntimeMapV2(", hooks)
        self.assertNotIn("RepairIstanaRuntimeMapV2", hooks)


if __name__ == "__main__":
    unittest.main()
