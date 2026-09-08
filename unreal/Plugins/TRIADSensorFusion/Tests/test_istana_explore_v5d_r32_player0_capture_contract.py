import hashlib
import json
import math
import pathlib
import shutil
import subprocess
import unittest


REPO = pathlib.Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal" / "Plugins" / "TRIADSensorFusion"
PUBLIC = PLUGIN / "Source" / "TRIADSensorFusion" / "Public"
PRIVATE = PLUGIN / "Source" / "TRIADSensorFusion" / "Private"
HEADER = PUBLIC / "TRIADIstanaExploreV5DR32Player0CaptureLibrary.h"
CPP = PRIVATE / "TRIADIstanaExploreV5DR32Player0CaptureLibrary.cpp"
WRAPPER = REPO / "scripts" / "Capture-IstanaExploreV5DR32Player0Evidence.ps1"
CONTRACT = (
    REPO
    / "unreal/SourceAssets/IstanaPublicViewExploreV5D/Vegetation/"
    "R32MediumDistanceTurf/r32_medium_distance_turf.contract.json"
)
README = CONTRACT.with_name("README.md")


class IstanaExploreV5DR32Player0CaptureContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.cpp = CPP.read_text(encoding="utf-8")
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
        cls.readme = README.read_text(encoding="utf-8")

    def test_contract_declares_unexecuted_two_phase_gate(self):
        gate = self.contract["nativePlayer0CaptureGate"]
        self.assertEqual(
            gate["schema"], "triad.istana_explore_v5d.r32_player0_capture.v1"
        )
        self.assertEqual(
            gate["wrapper"],
            "scripts/Capture-IstanaExploreV5DR32Player0Evidence.ps1",
        )
        self.assertTrue(gate["implemented"])
        self.assertFalse(gate["executed"])
        self.assertFalse(gate["accepted"])
        self.assertTrue(gate["twoPhaseHumanVisualReviewRequired"])
        self.assertFalse(gate["executeCanAcceptVisualQuality"])
        self.assertFalse(gate["acceptanceLaunchesUnreal"])
        self.assertFalse(gate["acceptanceWritesNativeProject"])
        self.assertTrue(gate["r32ReceiptMustBindFullAcceptedR30R31Chain"])
        self.assertTrue(gate["fullNativePluginSourceTreeReceiptRequired"])
        self.assertFalse(gate["r33SourceAllowed"])
        self.assertFalse(gate["performanceAcceptanceClaimed"])
        self.assertFalse(gate["hyperrealismClaimed"])
        self.assertFalse(gate["botanicalSurveyOrCurrentConditionClaimed"])

    def test_source_pins_are_exact(self):
        gate = self.contract["nativePlayer0CaptureGate"]
        for row in gate["captureSourceFiles"]:
            path = REPO / row["path"]
            payload = path.read_bytes()
            digest = hashlib.sha256(payload).hexdigest().upper()
            self.assertEqual(len(payload), row["bytes"])
            self.assertEqual(digest, row["sha256"])
            self.assertIn(f"Bytes = {row['bytes']}L", self.wrapper)
            self.assertIn(f"Sha256 = '{row['sha256']}'", self.wrapper)

    def test_runtime_library_is_bounded_and_has_no_successor_source(self):
        self.assertIn("UBlueprintFunctionLibrary", self.header)
        for endpoint in (
            "GetIstanaExploreV5DR32Player0CaptureState",
            "SetIstanaExploreV5DR32Player0CapturePose",
            "CaptureIstanaExploreV5DR32Player0TurfView",
            "FinishIstanaExploreV5DR32Player0CaptureRun",
        ):
            self.assertIn(endpoint, self.header)
            self.assertIn(endpoint, self.cpp)
        for forbidden in ("GEditor", "UnrealEd", "LevelEditor", "R33"):
            self.assertNotIn(forbidden, self.header)
            self.assertNotIn(forbidden, self.cpp)

    def test_exact_ten_pose_roster_and_turf_probe_geometry(self):
        expected_ids = [
            "075m",
            "020m",
            "008m",
            "002m",
            "surroundings_oblique_macdonald",
            "012m",
            "050m",
            "065m",
            "090m",
            "095m",
        ]
        gate = self.contract["nativePlayer0CaptureGate"]
        self.assertEqual(gate["exactPoseCount"], 10)
        self.assertEqual(gate["exactPoseIds"], expected_ids)
        self.assertIn("static_assert(UE_ARRAY_COUNT(ReviewedPoses) == 10)", self.cpp)
        self.assertEqual(
            gate["turfReadabilityReviewPoseIds"],
            ["012m", "020m", "050m"],
        )
        self.assertEqual(
            gate["turfIntersectionDistanceProbeIds"],
            ["012m", "050m", "065m", "090m", "095m"],
        )
        for distance, pitch in zip(
            gate["turfIntersectionDistancesMeters"],
            gate["turfProbePitchDegrees"],
        ):
            expected = -math.degrees(math.atan2(1.64, distance))
            self.assertAlmostEqual(pitch, expected, places=9)
            self.assertIn(f"DistanceMeters={distance:.1f}", self.wrapper)
            self.assertIn(f"{pitch:.9f}", self.cpp)
        self.assertEqual(
            gate["turfProbeCameraXyzCentimeters"], [3500.0, 15000.0, 164.0]
        )
        self.assertIn("turf-readability probes", self.header)
        self.assertIn("intersection-distance probes", self.header)

    def test_r32_and_all_predecessor_runtime_owners_are_fail_closed(self):
        for marker in (
            "ATRIADIstanaExploreV5DR32MediumDistanceTurfActor",
            "ValidateR32MediumDistanceTurf",
            "ExpectedBucketCount()",
            "ExpectedInstanceCount()",
            "SourceGroundVegetation != Ground",
            "ValidateGroundVegetationRealism",
            "ValidateCurrentSurroundingsBroadShellR31ForInheritedScene",
            "ValidateR30FacadeLookdev",
            "ValidateR29Vegetation",
            "ValidateCopernicusTerrainFallback",
            "ValidateTreeRealism",
            "r32MediumDistanceTurfOwner=1",
            "r32SelectedTransforms=4608",
            "r32OwnedHismCount=12",
            "r32CullMeters=65-90",
        ):
            self.assertIn(marker, self.cpp)

    def test_retired_owner_tags_and_exact_v2_shell_identity_are_preserved(self):
        for marker in (
            '#include "Components/StaticMeshComponent.h"',
            "R28FacadeCount != 0",
            "R28FacadeTagCount != 0",
            "R29FacadeCount != 0",
            "R29FacadeTagCount != 0",
            "LandmarkVegetationCount != 0",
            "LandmarkVegetationTagCount != 0",
            "ExactV2ComponentCount != 1",
            "ExpectedCurrentSurroundingsV2MeshObjectPath()",
            "ExactV2Component !=",
            "OutState.Policy->CurrentSurroundingsRenderOnlyComponent",
        ):
            self.assertIn(marker, self.cpp)
        gate = self.contract["nativePlayer0CaptureGate"]
        self.assertTrue(gate["retiredFacadeAndLandmarkOwnersAbsentByClassAndTag"])
        self.assertTrue(gate["retainedV2ShellComponentUniqueAndPolicyOwned"])

    def test_cesium_airsim_sdr_and_truth_boundaries_are_explicit(self):
        for marker in (
            "GooglePhotorealistic3DTilesIonAssetId = 2275207",
            "IstanaLongitudeDegrees = 103.84288055",
            "IstanaLatitudeDegrees = 1.30709615",
            "IstanaFallbackEllipsoidHeightMetres = 47.0",
            "AirSimTriadRuntime",
            "WeatherActor",
            "GetLoadProgress()",
            "GetSceneHDREnabled()",
            "SetResolution(2560, 1440, 1.0f)",
            "performanceAccepted=false",
            "surveyClaim=false",
            "botanicalClaim=false",
            "currentConditionClaim=false",
        ):
            self.assertIn(marker, self.cpp)
        self.assertIn("-COOKDIR=$airSimRuntimeContentRoot", self.wrapper)
        self.assertIn("DecodedBgraSha256", self.wrapper)
        self.assertIn("Sort-Object -Unique).Count -ne 10", self.wrapper)

    def test_r32_commit_and_full_accepted_r31_chain_are_revalidated(self):
        for marker in (
            "ExpectedR32CommitReceiptSha256",
            "triad.istana_explore_v5d.r32_medium_distance_turf.native_transaction.v1",
            "R30TransactionAdmission",
            "R30CaptureAdmission",
            "R31TransactionAdmission",
            "R31CaptureAdmission",
            "R31BroadShellVisualQaAccepted",
            "ConfirmedFiveImagesReviewed",
            "FivePngsRedecodedAndRehashed",
            "R32AdmissionAuthorized",
            "CommitR32EndpointInvocationCount",
            "StandaloneApplyR32EndpointInvocationCount",
            "NativePluginSourceTreeAfter",
            "R33SourceOrDeclarationAllowed",
        ):
            self.assertIn(marker, self.wrapper)

        chain = self.wrapper.split("function Assert-R32CommitReceipt", 1)[1].split(
            "function Assert-CaptureSourcePins", 1
        )[0]
        for field in (
            "MechanicalCaptureValidationPassed",
            "ExplicitHumanReviewAcceptance",
            "ConfirmedFiveImagesReviewed",
            "HumanVisualReviewAttested",
            "VisualReviewAccepted",
            "R31AdmissionAuthorized",
        ):
            self.assertIn(f"'{field}' 'R30 capture') -ne $true", chain)
        for field in (
            "AutomaticVisualAcceptanceAllowed",
            "VisualReviewRequired",
        ):
            self.assertIn(f"'{field}' 'R30 capture') -ne $false", chain)
        self.assertIn(
            "'HumanVisualReviewAttested' 'R31 capture') -ne $true", chain
        )
        self.assertIn("'R30CommitAdmission' 'R30 capture'", chain)
        self.assertIn("'PendingCaptureAdmission' 'R30 capture'", chain)
        self.assertIn("'AcceptanceRevalidation' 'R30 capture'", chain)

    def test_fixed_memory_serial_build_and_fresh_cook_guards(self):
        for marker in (
            "10737418240L # fixed 10 GiB",
            "12884901888L # fixed 12 GiB owned tree",
            "6442450944L # fixed continuous 6 GiB",
            "$memoryWatchdogPollMilliseconds = 500",
            "$memoryWatchdogPersistentBreachMilliseconds = 2000",
            "Get-OwnedProcessTree",
            "PeakOwnedTreePrivateBytes",
            "MEMORY_GUARD_OWNED_TREE_PRIVATE_BYTES",
            "MEMORY_GUARD_SYSTEM_FREE_VIRTUAL",
            "$Owned.Handle.Kill($true)",
            "-ForceHeaderGeneration",
            "-NoUBTMakefiles",
            "-MaxParallelActions=1",
            "-NoUBA",
            "-NoUBALocal",
            "-run=Cook",
            "-OutputDir=$cookOutputPattern",
            "Cooked\\[Platform]",
            "-Sandbox=$sandboxRoot",
            "turfIntersectionDistanceMeters={0:F3}",
        ):
            self.assertIn(marker, self.wrapper)

        self.assertNotIn("'-iterate'", self.wrapper.lower())

    def test_pending_and_accepted_receipts_expose_exact_successor_fields(self):
        for marker in (
            "Status='PENDING_VISUAL_REVIEW'",
            "Status='COMMITTED'",
            "NativeOrder='R32_COMMIT_THEN_R32_CAPTURE_BEFORE_R33'",
            "R33DependencyAllowed=$false",
            "PendingVisualReviewReceipt=[pscustomobject] [ordered]",
            "ExplicitHumanReviewAcceptance=$true",
            "ConfirmedTenImagesReviewed=$true",
            "HumanVisualReviewAttested=$true",
            "R32MediumDistanceTurfVisualQaAccepted=$true",
            "R33AdmissionAuthorized=$true",
            "MapModifiedByCapture=$false",
            "SimulationCollisionNavigationSensorRfModified=$false",
            "GroundHeader=(Get-RequiredPropertyValue",
            "GroundSource=(Get-RequiredPropertyValue",
            "NativePluginSourceTree=(Get-RequiredPropertyValue",
            "TenPngsRedecodedAndRehashed=$true",
            "NativeProjectWriteAllowed=$false",
            "UnrealLaunchAllowed=$false",
        ):
            self.assertIn(marker, self.wrapper)

        pending = self.wrapper.split("$pending = [pscustomobject] [ordered] @{", 1)[
            1
        ].split("$captureCompleted = $true", 1)[0]
        for marker in (
            "Status='PENDING_VISUAL_REVIEW'",
            "ExplicitHumanReviewAcceptance=$false",
            "ConfirmedTenImagesReviewed=$false",
            "HumanVisualReviewAttested=$false",
            "VisualReviewAccepted=$false",
            "R32MediumDistanceTurfVisualQaAccepted=$false",
            "R33AdmissionAuthorized=$false",
        ):
            self.assertIn(marker, pending)

        pending_parser = self.wrapper.split(
            "function Assert-PendingCaptureReceipt", 1
        )[1].split("function Invoke-VisualReviewAcceptance", 1)[0]
        for marker in (
            "'CallerSha256' 'R32 commit admission'",
            "Name='SuccessorMap'",
            "Name='RuntimeDllAfter'",
            "Name='EditorDllAfter'",
            "Name='GroundHeaderAfter'",
            "Name='GroundSourceAfter'",
            "'NativePluginSourceTreeAfter' 'R32 commit admission'",
            "Name='R30TransactionAdmission'",
            "Name='R30CaptureAdmission'",
            "Name='R31TransactionAdmission'",
            "Name='R31CaptureAdmission'",
        ):
            self.assertIn(marker, pending_parser)

    def test_acceptance_function_has_no_native_launch_or_source_write(self):
        start = self.wrapper.index("function Invoke-VisualReviewAcceptance")
        end = self.wrapper.index("$modeCount", start)
        acceptance = self.wrapper[start:end]
        for marker in (
            "Assert-PendingCaptureReceipt",
            "Assert-CaptureBindings",
            "Assert-ImmutableContentSnapshot $immutable",
            "PendingReceiptHashPinned=$true",
            "NativeStateMutatedByAcceptance=$false",
            "UnrealLaunchedByAcceptance=$false",
            "Write-JsonAtomic",
        ):
            self.assertIn(marker, acceptance)
        self.assertEqual(
            2, acceptance.count("Assert-ImmutableContentSnapshot $immutable")
        )
        for forbidden in (
            "Start-GuardedOwnedProcess",
            "Invoke-GuardedCommand",
            "Install-CaptureSourceClosure",
            "Restore-CaptureSourceClosure",
            "UnrealEditor-Cmd.exe",
        ):
            self.assertNotIn(forbidden, acceptance)

    def test_parser_static_self_check_and_confirmation_gate(self):
        pwsh = shutil.which("pwsh")
        if pwsh is None:
            self.skipTest("pwsh is unavailable")
        check = subprocess.run(
            [pwsh, "-NoProfile", "-File", str(WRAPPER), "-StaticSelfCheck"],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(check.returncode, 0, check.stdout + check.stderr)
        report = json.loads(check.stdout)
        self.assertEqual(report["Status"], "STATIC_SELF_CHECK_PASS")
        self.assertEqual(report["ExactPoseCount"], 10)
        self.assertTrue(report["RepositoryOnly"])
        self.assertFalse(report["NativeAccess"])
        self.assertFalse(report["UnrealLaunched"])
        self.assertFalse(report["R33DependencyAllowed"])
        self.assertTrue(report["TwoPhaseHumanVisualReview"])
        self.assertFalse(report["ExecuteCanAcceptVisualQuality"])

        rejected = subprocess.run(
            [
                pwsh,
                "-NoProfile",
                "-File",
                str(WRAPPER),
                "-AcceptVisualReview",
                "-RunToken",
                "review-required",
                "-PendingCaptureReceipt",
                r"D:\triad\TRIAD_R32Evidence\review-required\pending-visual-review.json",
                "-ExpectedPendingCaptureReceiptSha256",
                "0" * 64,
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("ConfirmTenImagesReviewed", rejected.stdout + rejected.stderr)

    def test_readme_is_truthful_about_source_only_state(self):
        for marker in (
            "promoted, compiled, executed, or accepted in the native project",
            "PENDING_VISUAL_REVIEW",
            "R32MediumDistanceTurfVisualQaAccepted=true",
            "do not establish target-hardware performance acceptance",
            "visual probes, not",
            "distance-survey evidence",
        ):
            self.assertIn(marker, self.readme)


if __name__ == "__main__":
    unittest.main()
