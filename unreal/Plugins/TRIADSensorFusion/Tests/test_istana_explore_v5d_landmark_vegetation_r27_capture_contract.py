from pathlib import Path
import json
import shutil
import subprocess
import unittest


REPO = Path(__file__).resolve().parents[4]
WRAPPER = REPO / "scripts/Capture-IstanaExploreV5DVegetationProviderEvidence.ps1"


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    return text[begin : text.index(end, begin)]


class IstanaExploreV5DLandmarkVegetationR27CaptureContractTests(
    unittest.TestCase
):
    @classmethod
    def setUpClass(cls) -> None:
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")
        cls.pwsh = shutil.which("pwsh") or shutil.which("pwsh.exe")

    def test_r27_live_mode_requires_six_explicit_native_pins_and_receipt_pin(self):
        for parameter in (
            "ExpectedMapBytes",
            "ExpectedMapSha256",
            "ExpectedRuntimeDllBytes",
            "ExpectedRuntimeDllSha256",
            "ExpectedEditorDllBytes",
            "ExpectedEditorDllSha256",
        ):
            self.assertIn(
                f"explicit caller-supplied -$requiredIdentityParameter",
                self.wrapper,
            )
            self.assertIn(f"'{parameter}'", self.wrapper)
        self.assertIn("[switch] $RequireLandmarkVegetationR27", self.wrapper)
        self.assertIn("[string] $R27CommitReceiptPath = ''", self.wrapper)
        self.assertIn("$ExpectedR27CommitReceiptSha256", self.wrapper)
        self.assertIn(
            "Strict R27 live capture requires an explicit commit.json path",
            self.wrapper,
        )

    def test_r27_receipt_is_bounded_and_exactly_bound_to_live_successor(self):
        receipt = between(
            self.wrapper,
            "function Get-ValidatedR27CommitReceipt {",
            "function New-VegetationRangePose {",
        )
        self.assertIn("V5DLandmarkVegetationR27V1", self.wrapper)
        for fragment in (
            "commit.json",
            "landmark_vegetation_r27.native_transaction.v1",
            "'Status' 'R27 receipt'",
            "'PASS'",
            "'VisualCaptureAccepted'",
            "'CaptureRevalidationRequired'",
            "'CollisionNavigationSensorRfAuthority'",
            "'SourceCount'",
            "'Phase2CompiledSourceCount'",
            "'RenderingCapableFreshEditorProcesses'",
            "'SuccessorMap'",
            "'RuntimeDll'",
            "'EditorDll'",
            "'R27MaterialPackages'",
        ):
            self.assertIn(fragment, receipt)
        for stage in (
            "00_cold_validate_phase2_pre_r27",
            "01_build_r27_grass_materials",
            "02_validate_reusable_assets",
            "03_apply_r27_visual_correction",
            "04_cold_validate_r27_map",
            "05_idempotent_r27_apply",
            "06_cold_validate_phase2_post_r27",
        ):
            self.assertIn(stage, receipt)

    def test_exact_four_package_r27_material_roster_is_pinned_at_boundaries(self):
        for leaf in (
            "M_IPV5D_LandmarkTurf_R27_Manicured.uasset",
            "M_IPV5D_LandmarkTurf_R27_Humid.uasset",
            "M_IPV5D_LandmarkTurf_R27_Shade.uasset",
            "M_IPV5D_LandmarkTurf_R27_DryEdge.uasset",
        ):
            self.assertEqual(1, self.wrapper.count(f"'{leaf}'"))
        roster = between(
            self.wrapper,
            "function Get-ExactR27GrassMaterialPins {",
            "function Get-ValidatedR27CommitReceipt {",
        )
        self.assertIn("$actualPaths.Count -ne 4", roster)
        self.assertIn("$expectedPaths.Count -ne 4", roster)
        self.assertIn("-File -Recurse -Force", roster)
        self.assertIn("reparse", roster.lower())
        boundary = between(
            self.wrapper,
            "$script:boundaryPins = @(",
            "Assert-FilePins -Pins $script:boundaryPins -Checkpoint 'preflight'",
        )
        self.assertIn("$r27GrassMaterialPins", boundary)
        self.assertIn("$r27CommitReceiptEvidence.Pin", boundary)
        self.assertIn("$script:r27MaterialBoundaryArmed = $true", boundary)

    def test_only_read_only_r27_phase2_validators_are_called_by_capture(self):
        live = between(
            self.wrapper,
            "if ($RequireLandmarkVegetationR27) {\n        Assert-FilePins",
            "$initialPie = Invoke-OwnedRcCall",
        )
        for endpoint in (
            "ValidateLandmarkGrassMaterialsR27",
            "ValidateIstanaExploreV5DTemasekShophouseR24Assets",
            "ValidateIstanaExploreV5DLandmarkVegetationR27SuccessorMap",
        ):
            self.assertIn(f"'{endpoint}'", live)
        self.assertNotIn(
            "-FunctionName 'BuildOrValidateLandmarkGrassMaterialsR27'", live
        )
        self.assertNotIn(
            "-FunctionName 'ApplyIstanaExploreV5DLandmarkVegetationR27VisualCorrectionToLoadedHybridMap'",
            live,
        )

    def test_r27_semantics_and_phase2_handoff_are_fail_closed(self):
        for marker in (
            "landmarkVegetationR27=true",
            "isolatedR27GrassMaterials=4",
            "sourceStableVisibilityMeters=20,28",
            "targetStableVisibilityMeters=65,90",
            "visibilityGateCount=1",
            "componentFadeIntegrated=true",
            "grassCullCm=6500,9000",
            "grassInstances=3072",
            "legacyTemasekBakedFoliageRemoved=true",
            "assets=16",
            "meshTriangles=15760",
            "materialSlots=15",
            "foliageOwner=ATRIADIstanaExploreV5DLandmarkVegetationActor",
            "bakedFoliageRenderComponents=0",
            "foliageTreeAnchors=3",
            "visualCaptureAccepted=false",
            "captureRevalidationRequired=true",
        ):
            self.assertIn(f"'{marker}'", self.wrapper)
        self.assertGreaterEqual(
            self.wrapper.count("Assert-ReportMetricMinimum"), 5
        )
        self.assertIn("-Minimum 0.75", self.wrapper)
        self.assertIn("-Minimum 1.5", self.wrapper)

    def test_r27_adds_one_12m_low_ground_grazing_pose_without_losing_context(self):
        pose = between(
            self.wrapper,
            "New-VegetationRangePose -Label '012m_ground_grazing_r27'",
            "New-VegetationRangePose -Label '008m'",
        )
        self.assertIn("-DistanceMeters 12.0", pose)
        self.assertIn("-WorldXCentimeters 35325.4728501344", pose)
        self.assertIn("-WorldYCentimeters 90196.7700142270", pose)
        self.assertIn("-WorldZCentimeters 182.041815825093", pose)
        self.assertIn("-CameraHeightAboveGroundCentimeters 85.0", pose)
        self.assertIn("-PitchDegrees -4.32374585326185", pose)
        self.assertIn("-YawDegrees -71.8858221227921", pose)
        self.assertIn("R27_MACDONALD_LANDMARK_GRASS_PATCH", pose)
        self.assertIn("35698.5668011438", pose)
        self.assertIn("89056.2434236907", pose)
        self.assertIn(
            "PRIMARY_R27_GRASS_READABILITY_MANUAL_REVIEW", pose
        )
        for context_label in ("075m", "065m", "050m", "020m", "008m", "002m"):
            self.assertIn(f"-Label '{context_label}'", self.wrapper)

    def test_manifest_cannot_turn_a_technical_capture_into_hyperreal_acceptance(self):
        for fragment in (
            "Status = 'PENDING_MANUAL_REVIEW'",
            "AutomatedHyperrealismClaimed = $false",
            "TechnicalCapturePassIsVisualAcceptance = $false",
            "R27_TECHNICAL_RASTER_CAPTURE_COMPLETE_MANUAL_VISUAL_ACCEPTANCE_PENDING",
            "AutomatedPhotorealismOrHyperrealismClaim = $false",
            "TechnicalCapturePassIsManualVisualAcceptance = $false",
        ):
            self.assertIn(fragment, self.wrapper)
        self.assertIn(
            "semantic reports and a PASS capture manifest do not prove photorealism or hyperrealism",
            self.wrapper,
        )

    def test_strict_static_self_check_has_seven_poses_and_touches_no_native_tree(self):
        if self.pwsh is None:
            self.skipTest("PowerShell 7 is unavailable")
        completed = subprocess.run(
            [
                self.pwsh,
                "-NoProfile",
                "-NonInteractive",
                "-File",
                str(WRAPPER),
                "-RunToken",
                "r27_capture_contract",
                "-RequireLandmarkVegetationR27",
                "-R27CommitReceiptPath",
                r"D:\triad\TRIAD\Saved\TRIAD\NativeTransactions\V5DLandmarkVegetationR27V1\placeholder\commit.json",
                "-ExpectedR27CommitReceiptSha256",
                "A" * 64,
                "-ExpectedMapBytes",
                "1",
                "-ExpectedMapSha256",
                "B" * 64,
                "-ExpectedRuntimeDllBytes",
                "2",
                "-ExpectedRuntimeDllSha256",
                "C" * 64,
                "-ExpectedEditorDllBytes",
                "3",
                "-ExpectedEditorDllSha256",
                "D" * 64,
                "-StaticSelfCheck",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(0, completed.returncode, completed.stderr)
        report = json.loads(completed.stdout)
        self.assertEqual("STATIC_SELF_CHECK_PASS", report["Status"])
        self.assertEqual(
            "LANDMARK_VEGETATION_R27_STRICT_CAPTURE",
            report["PresentationRevision"],
        )
        self.assertEqual(7, report["PoseCount"])
        self.assertEqual(
            "012m_ground_grazing_r27",
            report["ManualVisualAcceptance"]["PrimaryPose"],
        )
        self.assertTrue(report["ManualVisualAcceptance"]["ReviewRequired"])
        self.assertFalse(
            report["ManualVisualAcceptance"]["AutomatedHyperrealismClaimed"]
        )
        self.assertEqual(7, len(report["RuntimeContract"]["BoundaryAssetPaths"]))
        self.assertTrue(report["R27CommitReceiptExpectation"]["Required"])
        self.assertFalse(report["LiveEditorLaunched"])
        self.assertFalse(report["NativeTreeReadOrWritten"])

    def test_missing_explicit_pins_fails_before_any_native_preflight(self):
        if self.pwsh is None:
            self.skipTest("PowerShell 7 is unavailable")
        completed = subprocess.run(
            [
                self.pwsh,
                "-NoProfile",
                "-NonInteractive",
                "-File",
                str(WRAPPER),
                "-RunToken",
                "r27_missing_pins",
                "-RequireLandmarkVegetationR27",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertNotEqual(0, completed.returncode)
        self.assertIn(
            "Strict R27 live capture requires explicit caller-supplied",
            completed.stderr,
        )
        self.assertNotIn("The exact output, log, or DDC directory is absent", completed.stderr)

    def test_capstone_and_rc_safety_contract_remains_present(self):
        for fragment in (
            r"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe",
            r"C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject",
            "CreationUtcTicks",
            "Assert-ProtectedUE54Unchanged",
            "Get-NativeTRIADUnrealProcesses",
            "Get-RcListeners",
            "Test-RcOwnership",
            "RemoteControlEndpointAllowlist = @($rcCallUri)",
            "Stop-ExactHelperForContainment",
        ):
            self.assertIn(fragment, self.wrapper)


if __name__ == "__main__":
    unittest.main()
