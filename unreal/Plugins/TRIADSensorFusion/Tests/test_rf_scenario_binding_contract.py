from __future__ import annotations

import hashlib
import json
import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal/Plugins/TRIADSensorFusion"
TYPES = PLUGIN / "Source/TRIADSensorFusion/Public/TRIADSensorFusionTypes.h"
MANAGER_H = PLUGIN / "Source/TRIADSensorFusion/Public/TRIADSensorFusionScenarioManager.h"
MANAGER_CPP = PLUGIN / "Source/TRIADSensorFusion/Private/TRIADSensorFusionScenarioManager.cpp"
BUILD_CS = PLUGIN / "Source/TRIADSensorFusion/TRIADSensorFusion.Build.cs"
EXAMPLE = PLUGIN / "Resources/IstanaHighFidelityRF.example.json"
GENERIC_EXAMPLE = PLUGIN / "Resources/SingaporeSensorFusion.example.json"
RUNTIME_GEOMETRY = PLUGIN / "Resources/RF/IstanaPublicViewRFTightV1.geometry.json"
RUNTIME_CATALOG = PLUGIN / "Resources/RF/istana_rf_materials_tight_v1.catalog.json"
RUNTIME_CONTRACT = PLUGIN / "Resources/RF/istana_rf_scene_tight_v1.contract.json"
SOURCE_GEOMETRY = REPO / "unreal/SourceAssets/IstanaPublicViewRF/TightV1/Generated/IstanaPublicViewRFTightV1.geometry.json"
SOURCE_CATALOG = REPO / "unreal/SourceAssets/IstanaPublicViewRF/TightV1/istana_rf_materials_tight_v1.catalog.json"
SOURCE_CONTRACT = REPO / "unreal/SourceAssets/IstanaPublicViewRF/TightV1/istana_rf_scene_tight_v1.contract.json"


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"unclosed function: {signature}")


class RFScenarioBindingContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.types = text(TYPES)
        cls.manager_h = text(MANAGER_H)
        cls.manager_cpp = text(MANAGER_CPP)
        cls.build_cs = text(BUILD_CS)
        cls.example = json.loads(text(EXAMPLE))
        cls.generic_example = json.loads(text(GENERIC_EXAMPLE))

    def test_generic_default_is_off_and_istana_example_is_required_hash_bound(self) -> None:
        self.assertIn("bUseDedicatedRFPropagation = false", self.types)
        self.assertIn("bRequireDedicatedRFReady = false", self.types)
        self.assertIn("bDedicatedRFFrameIsWorldOriginIdentity = false", self.types)
        self.assertFalse(self.generic_example["bUseDedicatedRFPropagation"])
        self.assertFalse(self.generic_example["bRequireDedicatedRFReady"])
        self.assertTrue(self.example["bUseDedicatedRFPropagation"])
        self.assertTrue(self.example["bRequireDedicatedRFReady"])
        self.assertTrue(self.example["bDedicatedRFFrameIsWorldOriginIdentity"])
        self.assertEqual(
            self.example["DedicatedRFExpectedWorldPackageName"],
            "/Game/Maps/Istana_PublicView_Explore_v5d_hybrid",
        )
        self.assertEqual(
            self.example["DedicatedRFExpectedGeometrySha256"],
            sha256(SOURCE_GEOMETRY),
        )
        self.assertIn(
            self.example["DedicatedRFExpectedGeometrySha256"],
            self.types,
        )
        self.assertEqual(
            self.example["DedicatedRFExpectedMaterialCatalogSha256"],
            sha256(SOURCE_CATALOG),
        )
        self.assertEqual(
            self.example["DedicatedRFExpectedSceneContractSha256"],
            sha256(SOURCE_CONTRACT),
        )
        geometry = json.loads(text(SOURCE_GEOMETRY))
        catalog = json.loads(text(SOURCE_CATALOG))
        self.assertEqual(
            self.example["DedicatedRFExpectedGeometryQueryId"],
            f"{geometry['assetId']}@catalog-sha256:{sha256(SOURCE_CATALOG)}",
        )
        self.assertEqual(
            self.example["DedicatedRFExpectedMaterialCatalogId"],
            catalog["catalogId"],
        )
        self.assertEqual(
            self.example["DedicatedRFExpectedModeledCoverageId"],
            geometry["modeledCoverageEnvelope"]["coverageId"],
        )
        self.assertEqual(
            self.example["DedicatedRFExpectedModeledCoverageScope"],
            geometry["modeledCoverageEnvelope"]["scope"],
        )
        self.assertEqual(
            self.example["bDedicatedRFExpectedCoverageCoversOneKilometreAoi"],
            geometry["modeledCoverageEnvelope"]["coversOneKilometreAoi"],
        )
        self.assertEqual(
            self.example["bDedicatedRFExpectedCoverageCoversSurroundings"],
            geometry["modeledCoverageEnvelope"]["coversSurroundings"],
        )

    def test_runtime_resources_are_exact_staged_copies(self) -> None:
        self.assertEqual(RUNTIME_GEOMETRY.read_bytes(), SOURCE_GEOMETRY.read_bytes())
        self.assertEqual(RUNTIME_CATALOG.read_bytes(), SOURCE_CATALOG.read_bytes())
        self.assertEqual(RUNTIME_CONTRACT.read_bytes(), SOURCE_CONTRACT.read_bytes())
        self.assertEqual(sha256(RUNTIME_GEOMETRY), self.example["DedicatedRFExpectedGeometrySha256"])
        self.assertEqual(sha256(RUNTIME_CATALOG), self.example["DedicatedRFExpectedMaterialCatalogSha256"])
        self.assertEqual(sha256(RUNTIME_CONTRACT), self.example["DedicatedRFExpectedSceneContractSha256"])
        self.assertIn("RuntimeDependencies.Add", self.build_cs)
        self.assertIn("IstanaPublicViewRFTightV1.geometry.json", self.build_cs)
        self.assertIn("istana_rf_materials_tight_v1.catalog.json", self.build_cs)
        self.assertIn("istana_rf_scene_tight_v1.contract.json", self.build_cs)

    def test_resource_resolution_and_begin_play_load_are_fail_closed(self) -> None:
        resolver = function_body(
            self.manager_cpp,
            "bool ATRIADSensorFusionScenarioManager::ResolveDedicatedRFResourcePath(",
        )
        for marker in (
            'TEXT("Plugin/")',
            'TEXT("Project/")',
            "IPluginManager::Get().FindPlugin",
            "FPaths::IsRelative",
            'Component == TEXT("..")',
            "Candidate.StartsWith(RootPrefix",
        ):
            self.assertIn(marker, resolver)
        initialize = function_body(
            self.manager_cpp,
            "bool ATRIADSensorFusionScenarioManager::InitializeDedicatedRFPropagation(",
        )
        for marker in (
            "bDedicatedRFFrameIsWorldOriginIdentity",
            "DedicatedRFExpectedWorldPackageName",
            "LoadFromJsonFiles",
            "ScenarioConfig.DedicatedRFExpectedGeometrySha256",
            "ScenarioConfig.DedicatedRFExpectedMaterialCatalogSha256",
            "ScenarioConfig.DedicatedRFExpectedSceneContractSha256",
            "ScenarioConfig.DedicatedRFExpectedModeledCoverageId",
            "ScenarioConfig.DedicatedRFExpectedModeledCoverageScope",
            "MaximumSceneContractBytes",
            "IFileManager::Get().FileSize(*ContractPath)",
            "FFileHelper::LoadFileToString(ContractJson",
            "ComputeCanonicalJsonSha256",
            "scene-contract hash mismatch: actual=%s expected=%s",
            "Metadata.ContractSha256.Equals",
            "IsReady()",
            "CandidateQuery->GetMetadata()",
            "Metadata.GeometrySha256",
            "DedicatedRFGeometryQuery = MoveTemp(CandidateQuery)",
            "DedicatedRFInteractionModel = MoveTemp(CandidateModel)",
        ):
            self.assertIn(marker, initialize)
        self.assertNotIn("ComputeFileSha256", self.manager_cpp)
        self.assertIn(
            "DedicatedRFGeometrySha256 = Metadata.GeometrySha256",
            initialize,
        )
        self.assertIn(
            "DedicatedRFSceneContractSha256 = ContractHash.ToLower()",
            initialize,
        )
        self.assertIn(
            "DedicatedRFMaterialCatalogSha256 = Metadata.MaterialCatalogSha256",
            initialize,
        )
        fail_block = initialize[
            initialize.index("const auto Fail") : initialize.index(
                "if (!ScenarioConfig.bUseDedicatedRFPropagation)"
            )
        ]
        for consumed_hash in (
            "DedicatedRFGeometrySha256.Reset()",
            "DedicatedRFMaterialCatalogSha256.Reset()",
            "DedicatedRFSceneContractSha256.Reset()",
        ):
            self.assertIn(consumed_hash, fail_block)
        self.assertNotIn("FPlatformMisc::GetSHA256Signature", self.manager_cpp)
        begin_play = function_body(
            self.manager_cpp,
            "void ATRIADSensorFusionScenarioManager::BeginPlay()",
        )
        self.assertLess(
            begin_play.index("InitializeDedicatedRFPropagation"),
            begin_play.index("InitializeTelemetry"),
        )
        self.assertRegex(
            begin_play,
            r"(?s)if \(ScenarioConfig\.bRequireDedicatedRFReady\).*?Destroy\(\);",
        )

    def test_only_direct_or_straight_transmission_is_selected_by_lowest_loss(self) -> None:
        evaluate = function_body(
            self.manager_cpp,
            "bool ATRIADSensorFusionScenarioManager::EvaluateDedicatedRFPath(",
        )
        self.assertIn("BuildPathCandidates", evaluate)
        self.assertIn("if (Candidates.IsEmpty())", evaluate)
        self.assertIn("OPAQUE_STRAIGHT_PATH_NO_ADMITTED", evaluate)
        self.assertIn("Candidate.Kind == ETRIADRFPathKind::Direct", evaluate)
        self.assertIn("Candidate.Kind == ETRIADRFPathKind::Transmitted", evaluate)
        self.assertNotIn("Candidate.Kind == ETRIADRFPathKind::SingleReflection", evaluate)
        self.assertIn("EvaluatePaths", evaluate)
        self.assertIn("Evaluation.TotalPropagationLossDb < Best->TotalPropagationLossDb", evaluate)
        self.assertIn("Evaluation.PathId < Best->PathId", evaluate)
        for unsupported in ("diffraction", "polarization", "phase synthesis"):
            self.assertNotIn(unsupported, evaluate.lower())

    def test_dedicated_detection_does_not_use_visibility_and_losses_stay_separate(self) -> None:
        sample = function_body(
            self.manager_cpp,
            "bool ATRIADSensorFusionScenarioManager::SampleNodeTargetLink(",
        )
        self.assertIn("EvaluateDedicatedRFPath", sample)
        self.assertIn("bLegacyLineOfSightGateSatisfied = Propagation.bDedicated ||", sample)
        self.assertRegex(
            sample,
            r"TotalPropagationLossDb\s*-\s*NodeDefinition\.SystemLossDb\s*-\s*WeatherLossDb",
        )
        self.assertNotRegex(
            sample,
            r"Propagation\.bDedicated\s*&&\s*bLineOfSight",
        )
        self.assertIn("DIAGNOSTIC_VISIBILITY_TRACE_ONLY_NOT_RF_LOSS_OR_DETECTION_GATE", sample)

    def test_json_and_csv_expose_provenance_path_loss_and_failure_semantics(self) -> None:
        telemetry = function_body(
            self.manager_cpp,
            "void ATRIADSensorFusionScenarioManager::AddRFPropagationTelemetryFields(",
        )
        for field in (
            "propagationMode",
            "rfPropagationReadiness",
            "rfGeometryQueryId",
            "rfGeometrySha256",
            "rfResourceHashSemantics",
            "rfGeometrySchemaVersion",
            "rfMaterialCatalogId",
            "rfMaterialCatalogSha256",
            "rfModeledCoverageId",
            "rfModeledCoverageScope",
            "rfModeledCoverageOutsideDomainPolicy",
            "rfModeledCoverageCoversOneKilometreAoi",
            "rfModeledCoverageCoversSurroundings",
            "pathId",
            "pathKind",
            "materialCalibrationState",
            "freeSpacePathLossDb",
            "interactionLossDb",
            "totalPropagationLossDb",
            "systemLossDb",
            "weatherRFLossDb",
            "totalLinkLossDb",
            "propagationFailureReason",
            "readyForSurveyTruth",
            "fieldValidated",
            "externalAcceptanceContextBound",
            "rfInteractionTraceSchemaVersion",
            "rfInteractions",
            "solidId",
            "profileId",
            "sourceClass",
            "uncertaintyClass",
            "coefficientSelectionSemantics",
            "contributors",
            "entrySurfaceId",
            "exitSurfaceId",
            "entryPointCentimeters",
            "exitPointCentimeters",
            "calibrationProvenanceId",
            "minimumFrequencyGHz",
            "maximumFrequencyGHz",
            "minimumIncidenceCosine",
            "maximumIncidenceCosine",
            "pairedBoundaryTransmissionLossDb",
            "bulkAttenuationDbPerMeter",
            "reflectionLossDb",
            "empiricalGrazingReflectionLossDb",
        ):
            self.assertIn(field, telemetry)
        self.assertIn("propagation_mode,rf_propagation_readiness", self.manager_cpp)
        self.assertIn("total_propagation_loss_db", self.manager_cpp)
        self.assertIn("propagation_failure_reason", self.manager_cpp)
        self.assertIn("ready_for_survey_truth", self.manager_cpp)
        self.assertIn("rf_interaction_trace_json", self.manager_cpp)
        self.assertIn("triad.rf_interaction_trace.v3", self.manager_cpp)
        self.assertNotIn("triad.rf_interaction_trace.v2", telemetry)
        self.assertNotRegex(
            self.manager_cpp,
            r'SetBoolField\(TEXT\("(?:readyForSurveyTruth|fieldValidated|externalAcceptanceContextBound)"\),\s*true',
        )
        self.assertNotRegex(
            self.manager_cpp,
            r'bModeledCoverage(?:CoversOneKilometreAoi|CoversSurroundings)\s*=\s*true',
        )

    def test_ready_log_reports_loaded_coverage_flags_without_tight_v1_assumption(self) -> None:
        initialize = function_body(
            self.manager_cpp,
            "bool ATRIADSensorFusionScenarioManager::InitializeDedicatedRFPropagation(",
        )
        self.assertIn("coversOneKilometreAoi=%s", initialize)
        self.assertIn("coversSurroundings=%s", initialize)
        self.assertIn(
            "DedicatedRFMetadata.bModeledCoverageCoversOneKilometreAoi",
            initialize,
        )
        self.assertIn(
            "DedicatedRFMetadata.bModeledCoverageCoversSurroundings",
            initialize,
        )
        self.assertNotIn("not 1km AOI", initialize)


if __name__ == "__main__":
    unittest.main()
