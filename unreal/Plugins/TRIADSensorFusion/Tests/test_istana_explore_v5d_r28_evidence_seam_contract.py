from __future__ import annotations

import json
import subprocess
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
HEADER = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/"
    "Public/TRIADIstanaExploreV5DHybridEditorLibrary.h"
)
SOURCE = REPO / (
    "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/"
    "Private/TRIADIstanaExploreV5DHybridEditorLibrary.cpp"
)
CAPTURE = REPO / "scripts/Capture-IstanaExploreV5DVegetationProviderEvidence.ps1"


def braced_block(text: str, marker: str) -> str:
    start = text.index(marker)
    brace = text.index("{", start)
    depth = 0
    for index in range(brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[start : index + 1]
    raise AssertionError(f"unterminated block after {marker}")


class IstanaExploreV5DR28EvidenceSeamContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.source = SOURCE.read_text(encoding="utf-8")
        cls.capture = CAPTURE.read_text(encoding="utf-8")

    def test_versioned_r28_endpoints_are_declared_and_defined_once(self) -> None:
        endpoints = (
            "ValidateIstanaExploreV5DR28VisualSuccessorPlayWorld",
            "GetIstanaExploreV5DR28VisualSuccessorPlayStateReport",
            "SetIstanaExploreV5DR28VisualSuccessorPlayViewPoseForQa",
            "CaptureIstanaExploreV5DR28VisualSuccessorPlayView",
            "CaptureIstanaExploreV5DR28VisualSuccessorDiagnosticPlayView",
        )
        for endpoint in endpoints:
            self.assertEqual(1, self.header.count(f"static bool {endpoint}("))
            self.assertEqual(1, self.source.count(f"    {endpoint}("))

    def test_internal_play_state_has_explicit_r28_contract_switch(self) -> None:
        helper = braced_block(self.source, "bool GetValidatedHybridPlayState(")
        for fragment in (
            "bool bRequireR28VisualSuccessor = false",
            "FString* OutWorldContractReport = nullptr",
            "bRequireR28VisualSuccessor",
            "ValidateR28VisualSuccessorWorld(OutWorld, WorldReport)",
            "ValidateHybridWorld(",
            "The active world is not the exact validated Explore V5D R28",
        ):
            self.assertIn(fragment, helper)

    def test_r28_endpoints_require_combined_world_and_player0_policy(self) -> None:
        markers = {
            "ValidateIstanaExploreV5DR28VisualSuccessorPlayWorld": (
                "true,",
                "&R28WorldReport",
                "ValidateR28Player0Presentation(",
                "ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_PIE_VALID",
                "r28ProviderNegativeAuthority=true",
            ),
            "GetIstanaExploreV5DR28VisualSuccessorPlayStateReport": (
                "GetIstanaExploreV5DHybridPlayStateReportForContract(",
                "true,",
            ),
            "SetIstanaExploreV5DR28VisualSuccessorPlayViewPoseForQa": (
                "true,",
                "&R28WorldReport",
                "ValidateR28Player0Presentation(",
                "EXPLORE_V5D_R28_VISUAL_SUCCESSOR_EXACT_QA_VIEW_POSE_PASS",
            ),
            "CaptureIstanaExploreV5DR28VisualSuccessorPlayView": (
                "true,",
                "&R28WorldReport",
                "r28EnvironmentPlayer0Visible=false",
                "r28EnvironmentSceneCaptureSensorExcluded=true",
            ),
            "CaptureIstanaExploreV5DR28VisualSuccessorDiagnosticPlayView": (
                "true,",
                "&R28WorldReport",
                "ValidateR28Player0Presentation(",
                "R28 VISUAL SUCCESSOR DIAGNOSTIC EVIDENCE:",
                "r28EnvironmentPlayer0Visible=true",
                "r28EnvironmentSceneCaptureSensorExcluded=true",
            ),
        }
        for endpoint, fragments in markers.items():
            block = braced_block(self.source, f"    {endpoint}(")
            for fragment in fragments:
                self.assertIn(fragment, block, endpoint)
            for forbidden in ("Modify(", "MarkPackageDirty", "SaveMap("):
                self.assertNotIn(forbidden, block, endpoint)

        presentation = braced_block(
            self.source, "bool ValidateR28Player0Presentation("
        )
        for fragment in (
            "EnvironmentCount != 1",
            "Component->bHiddenInSceneCapture",
            "!Component->bVisibleInSceneCaptureOnly",
            "Environment->bProviderReady !=",
            "Policy->bLocalBuildingFallbackCurrentlyHidden",
            "!Environment->bCollisionNavigationSensorOrRfAuthority",
            "!Environment->bExistingSimulationOrRfInputsModified",
            "!Environment->bRuntimeGeometryGenerated",
        ):
            self.assertIn(fragment, presentation)

    def test_legacy_public_endpoints_keep_legacy_contract(self) -> None:
        legacy = (
            "ValidateIstanaExploreV5DHybridPlayWorld",
            "GetIstanaExploreV5DHybridPlayStateReport",
            "SetIstanaExploreV5DHybridPlayViewPoseForQa",
            "CaptureIstanaExploreV5DHybridPlayView",
            "CaptureIstanaExploreV5DHybridDiagnosticPlayView",
        )
        for endpoint in legacy:
            block = braced_block(self.source, f"    {endpoint}(")
            self.assertNotIn("ValidateR28VisualSuccessorWorld", block)
            self.assertNotIn("R28_VISUAL_SUCCESSOR", block)

    def test_capture_wrapper_has_exact_r28_receipt_and_four_view_contract(self) -> None:
        for fragment in (
            "[switch] $RequireR28VisualSuccessor",
            "$R28CommitReceiptPath",
            "$ExpectedR28CommitReceiptSha256",
            "V5DVisualRealismR28V1",
            "triad.istana_explore_v5d.visual_realism_r28.native_transaction.v1",
            "PredecessorMap",
            "PredecessorRuntimeDll",
            "PredecessorEditorDll",
            "SuccessorMap",
            "SuccessorRuntimeDll",
            "SuccessorEditorDll",
            "ExactlyNewAssetCount",
            "CollisionNavigationSensorRfTerrainAuthority",
            "00_cold_validate_r27_pre_r28",
            "07_apply_combined_r28_visual_successor",
            "09_cold_validate_combined_r28_map",
            "10_idempotent_combined_r28_apply",
            "11_postvalidate_combined_r28_r25_provider_contract",
            "13_postvalidate_r28_grass_materials",
            "R28_CLOSE_NORMAL_LAWN_TREE",
            "R28_ISTANA_WIDE",
            "R28_TEMASEK_MACDONALD_STREETSCAPE_CONTEXT",
            "R28_BROADER_SURROUNDINGS",
            "VisualCaptureAccepted = [bool] $r28VisualCaptureAccepted",
            "ImpliedByVisualCaptureAccepted = $false",
        ):
            self.assertIn(fragment, self.capture)

    def test_r28_wrapper_routes_only_to_versioned_map_and_pie_endpoints(self) -> None:
        for endpoint in (
            "ValidateIstanaExploreV5DR28VisualSuccessorMap",
            "ValidateIstanaExploreV5DR28VisualSuccessorPlayWorld",
            "GetIstanaExploreV5DR28VisualSuccessorPlayStateReport",
            "SetIstanaExploreV5DR28VisualSuccessorPlayViewPoseForQa",
            "CaptureIstanaExploreV5DR28VisualSuccessorDiagnosticPlayView",
        ):
            self.assertIn(endpoint, self.capture)
        self.assertIn(
            "-RequireR28VisualSuccessor and -RequireLandmarkVegetationR27 are mutually exclusive.",
            self.capture.replace(
                "-RequireLandmarkVegetationR27 and -RequireR28VisualSuccessor",
                "-RequireR28VisualSuccessor and -RequireLandmarkVegetationR27",
            ),
        )
        self.assertIn("ProviderEvidenceMode -cne 'ProviderFallback'", self.capture)

    def test_r28_static_self_check_is_parseable_and_non_native(self) -> None:
        command = (
            "$ErrorActionPreference='Stop';"
            f"$p='{str(CAPTURE).replace(chr(39), chr(39) * 2)}';"
            "$t=$null;$e=$null;"
            "[void][Management.Automation.Language.Parser]::ParseFile($p,[ref]$t,[ref]$e);"
            "if($e.Count){throw ($e|Out-String)};"
            "& $p -RunToken r28_contract_test -StaticSelfCheck "
            "-RequireR28VisualSuccessor -ProviderEvidenceMode ProviderFallback "
            "-ExpectedMapBytes 1 -ExpectedMapSha256 ('A'*64) "
            "-ExpectedRuntimeDllBytes 1 -ExpectedRuntimeDllSha256 ('B'*64) "
            "-ExpectedEditorDllBytes 1 -ExpectedEditorDllSha256 ('C'*64) "
            "-R28CommitReceiptPath "
            "'D:\\triad\\TRIAD\\Saved\\TRIAD\\NativeTransactions\\"
            "V5DVisualRealismR28V1\\test\\commit.json' "
            "-ExpectedR28CommitReceiptSha256 ('D'*64)"
        )
        completed = subprocess.run(
            ["pwsh", "-NoProfile", "-Command", command],
            check=True,
            capture_output=True,
            text=True,
        )
        payload = json.loads(completed.stdout)
        self.assertEqual("STATIC_SELF_CHECK_PASS", payload["Status"])
        self.assertEqual("R28_VISUAL_SUCCESSOR_STRICT_CAPTURE", payload["PresentationRevision"])
        self.assertEqual(4, payload["PoseCount"])
        self.assertFalse(payload["R28VisualAcceptance"]["VisualCaptureAccepted"])
        self.assertFalse(payload["R28VisualAcceptance"]["ProviderReadinessImplied"])
        self.assertFalse(payload["LiveEditorLaunched"])
        self.assertFalse(payload["NativeTreeReadOrWritten"])

    def test_r28_mode_and_r27_mode_fail_closed_when_combined(self) -> None:
        completed = subprocess.run(
            [
                "pwsh",
                "-NoProfile",
                "-File",
                str(CAPTURE),
                "-RunToken",
                "r28_exclusive_contract",
                "-StaticSelfCheck",
                "-RequireR28VisualSuccessor",
                "-RequireLandmarkVegetationR27",
                "-ProviderEvidenceMode",
                "ProviderFallback",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertNotEqual(0, completed.returncode)
        self.assertIn("mutually exclusive", completed.stderr + completed.stdout)

        completed = subprocess.run(
            [
                "pwsh",
                "-NoProfile",
                "-File",
                str(CAPTURE),
                "-RunToken",
                "r28_fallback_contract",
                "-StaticSelfCheck",
                "-RequireR28VisualSuccessor",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertNotEqual(0, completed.returncode)
        self.assertIn(
            "requires -ProviderEvidenceMode ProviderFallback",
            completed.stderr + completed.stdout,
        )


if __name__ == "__main__":
    unittest.main()
