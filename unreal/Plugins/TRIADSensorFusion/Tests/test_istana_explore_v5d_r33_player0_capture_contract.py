import hashlib
import json
import pathlib
import re
import shutil
import subprocess
import unittest


REPO = pathlib.Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal" / "Plugins" / "TRIADSensorFusion"
HEADER = (
    PLUGIN
    / "Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DR33Player0CaptureLibrary.h"
)
CPP = (
    PLUGIN
    / "Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DR33Player0CaptureLibrary.cpp"
)
WRAPPER = REPO / "scripts/Capture-IstanaExploreV5DR33Player0Evidence.ps1"
ASSET_ROOT = (
    REPO
    / "unreal/SourceAssets/IstanaPublicViewExploreV5D/Terrain/"
    "R33CesiumWorldTerrainReference"
)
CONTRACT = ASSET_ROOT / "r33_player0_visual_review.contract.json"
README = ASSET_ROOT / "README.md"
GUIDE = REPO / "docs/ISTANA_R30_R33_NATIVE_EXECUTION_GUIDE.md"


class IstanaExploreV5DR33Player0CaptureContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.cpp = CPP.read_text(encoding="utf-8")
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
        cls.readme = README.read_text(encoding="utf-8")
        cls.guide = GUIDE.read_text(encoding="utf-8")

    def test_contract_is_unexecuted_two_phase_visual_only_gate(self):
        gate = self.contract["nativePlayer0CaptureGate"]
        self.assertEqual(
            self.contract["schema"],
            "triad.istana_explore_v5d.r33_player0_visual_review.v1",
        )
        self.assertEqual(
            gate["schema"], "triad.istana_explore_v5d.r33_player0_capture.v1"
        )
        self.assertTrue(gate["implemented"])
        self.assertFalse(gate["executed"])
        self.assertFalse(gate["accepted"])
        self.assertTrue(gate["twoPhaseHumanVisualReviewRequired"])
        self.assertFalse(gate["executeCanAcceptVisualQuality"])
        self.assertFalse(gate["acceptanceLaunchesUnreal"])
        self.assertFalse(gate["acceptanceWritesNativeProject"])
        self.assertFalse(gate["r34SourceAllowed"])
        self.assertFalse(gate["r34AdmissionAuthorized"])

    def test_two_capture_source_pins_are_exact(self):
        gate = self.contract["nativePlayer0CaptureGate"]
        self.assertEqual(gate["exactSourceTreeAdditionCount"], 2)
        self.assertEqual(gate["nativeContentPackageCount"], 0)
        self.assertEqual(len(gate["captureSourceFiles"]), 2)
        for row in gate["captureSourceFiles"]:
            payload = (REPO / row["path"]).read_bytes()
            digest = hashlib.sha256(payload).hexdigest().upper()
            self.assertEqual(len(payload), row["bytes"])
            self.assertEqual(digest, row["sha256"])
            self.assertIn(f"Bytes = {row['bytes']}L", self.wrapper)
            self.assertIn(f"Sha256 = '{row['sha256']}'", self.wrapper)

    def test_runtime_library_is_r33_only_and_never_forces_presented(self):
        for endpoint in (
            "GetIstanaExploreV5DR33Player0CaptureState",
            "SetIstanaExploreV5DR33Player0Presentation",
            "SetIstanaExploreV5DR33Player0CapturePose",
            "CaptureIstanaExploreV5DR33Player0ComparisonView",
            "FinishIstanaExploreV5DR33Player0CaptureRun",
        ):
            self.assertIn(endpoint, self.header)
            self.assertIn(endpoint, self.cpp)
        for forbidden in ("GEditor", "UnrealEd", "LevelEditor", "R34"):
            self.assertNotIn(forbidden, self.header)
            self.assertNotIn(forbidden, self.cpp)
        self.assertNotIn("->EnterCwtPresented(", self.cpp)
        self.assertIn("RequestCwtPresentation", self.cpp)

    def test_exact_four_by_two_capture_roster(self):
        gate = self.contract["nativePlayer0CaptureGate"]
        poses = [
            "075m",
            "020m",
            "095m",
            "surroundings_oblique_macdonald",
        ]
        states = ["GooglePrimary", "CwtPresented"]
        expected = [f"{state}__{pose}" for state in states for pose in poses]
        self.assertEqual(gate["exactPoseCount"], 4)
        self.assertEqual(gate["exactPoseIds"], poses)
        self.assertEqual(gate["exactPresentationStates"], states)
        self.assertEqual(gate["exactCoreCaptureCount"], 8)
        self.assertEqual(gate["exactCaptureOrder"], expected)
        self.assertIn("static_assert(UE_ARRAY_COUNT(ReviewedPoses) == 4)", self.cpp)
        for pose in poses:
            self.assertIn(pose, self.cpp)
            self.assertIn(f"Id='{pose}'", self.wrapper)

    def test_cesium_state_machine_and_truth_boundaries_are_explicit(self):
        gate = self.contract["nativePlayer0CaptureGate"]
        self.assertEqual(gate["googlePhotorealisticIonAssetId"], 2275207)
        self.assertEqual(gate["cesiumWorldTerrainIonAssetId"], 1)
        self.assertTrue(gate["sameGeoreferenceAndIonServerRequired"])
        self.assertTrue(gate["exactNonNullOpaqueIonServerRequired"])
        self.assertFalse(gate["serverObjectValidityProvesTokenPresence"])
        self.assertFalse(gate["serverObjectValidityProvesAssetEntitlement"])
        self.assertFalse(gate["ionTokenValueOrFingerprintInspectedByTriad"])
        self.assertTrue(gate["naturalCwtWarmingTransitionRequired"])
        self.assertFalse(gate["directEnterCwtPresentedAllowed"])
        self.assertTrue(gate["restoreGooglePrimaryBeforeExitRequired"])
        self.assertFalse(gate["safeLocalDiagnostic"]["included"])
        for marker in (
            "GooglePhotorealistic3DTilesIonAssetId = 2275207",
            "CesiumWorldTerrainIonAssetId = 1",
            "GetGeoreference().Get() != OutState.Georeference",
            "presentation=CwtWarming",
            "doubleVisible=false",
            "providerReadyProofClaimed=false",
            "accurateRealWorldTerrainClaimed=false",
            "surveyAccuracyClaimed=false",
            "verticalDatumResolved=false",
            "heightSamplesPersisted=false",
            "restoredPresentation=GooglePrimary",
        ):
            self.assertIn(marker, self.cpp + self.wrapper)

    def test_capture_resolver_rejects_equal_null_opaque_server_pointers(self):
        self.assertIn('#include "CesiumIonServer.h"', self.cpp)
        self.assertNotIn("GetIonAccessToken(", self.cpp)
        self.assertNotIn("DefaultIonAccessToken", self.cpp)
        resolver = self.cpp.split("bool ResolveValidatedCaptureState(", 1)[1].split(
            "FString CaptureFilename(", 1
        )[0]
        compact = re.sub(r"\s+", "", resolver)
        required = (
            "!IsValid(OutState.GoogleTileset->GetCesiumIonServer())",
            "!IsValid(OutState.CwtTileset->GetCesiumIonServer())",
            "OutState.GoogleTileset->GetCesiumIonServer()!=OutState.CwtTileset->GetCesiumIonServer()",
        )
        for marker in required:
            self.assertIn(marker, compact)
        equality_only_mutation = compact.replace(required[0] + "||", "", 1).replace(
            required[1] + "||", "", 1
        )
        self.assertIn(required[2], equality_only_mutation)
        self.assertNotIn(required[0], equality_only_mutation)
        self.assertNotIn(required[1], equality_only_mutation)

    def test_full_chain_and_no_mutation_are_replayed(self):
        for marker in (
            "ExpectedR33CommitReceiptSha256",
            "R30TransactionAdmission",
            "R30CaptureAdmission",
            "R31TransactionAdmission",
            "R31CaptureAdmission",
            "R32TransactionAdmission",
            "R32CaptureAdmission",
            "MechanicalCaptureValidationPassed",
            "ExplicitHumanReviewAcceptance",
            "HumanVisualReviewAttested",
            "NativePluginSourceTreeBefore",
            "NativePluginSourceTreeAfter",
            "MapCaptureBinding",
            "GroundHeaderCaptureBinding",
            "GroundSourceCaptureBinding",
            "NativePluginSourceTreeCaptureBinding",
        ):
            self.assertIn(marker, self.wrapper)
        chain = self.wrapper.split("function Assert-R33CommitReceipt", 1)[1].split(
            "function Assert-CaptureSourcePins", 1
        )[0]
        self.assertIn("'R30 capture'", chain)
        self.assertIn("'R31 capture'", chain)
        self.assertIn("'R32 capture'", chain)

    def test_predecessor_png_rehash_fields_include_r31_six_image_gate(self):
        predecessor_gate = self.wrapper.split(
            "function Assert-AcceptedCaptureTruth", 1
        )[1].split("function Assert-R33CommitReceipt", 1)[0]
        self.assertIn("FivePngsRedecodedAndRehashed", self.wrapper)
        self.assertIn("SixPngsRedecodedAndRehashed", self.wrapper)
        self.assertIn("TenPngsRedecodedAndRehashed", self.wrapper)
        self.assertIn("Assert-R31V2NaniteRasterAcceptedReceipt", predecessor_gate)
        for token in (
            "ExactImageCount",
            "ConfirmedNaniteRasterPairReviewed",
            "HumanNaniteRasterComparisonAttested",
            "NaniteRasterAppearanceParityAccepted",
            "NaniteConsoleStateRestored",
            "NaniteRasterHighOccupancyComparison",
        ):
            self.assertIn(token, predecessor_gate)
        self.assertNotIn("EightPngsRedecodedAndRehashed", predecessor_gate)

    def test_live_capture_changes_presentation_once_per_group(self):
        live = self.wrapper.split("$captureReceipts =", 1)[1].split(
            "$finishTransportError", 1
        )[0]
        self.assertIn("foreach ($plan in $capturePlan)", live)
        self.assertIn("$currentPresentation -cne $plan.Presentation", live)
        self.assertEqual(live.count("Request-PresentationAndWait"), 1)
        self.assertIn("$presentationTransitions['CwtPresented'].WarmingObserved", live)
        self.assertNotIn("foreach ($pose in $poses)", live)

    def test_safe_local_and_transition_replay_fail_closed(self):
        readiness = self.wrapper.split("function Wait-RuntimeCaptureReady", 1)[
            1
        ].split("function Request-PresentationAndWait", 1)[0]
        wait = self.wrapper.split("function Request-PresentationAndWait", 1)[1].split(
            "function Wait-StablePose", 1
        )[0]
        pose_wait = self.wrapper.split("function Wait-StablePose", 1)[1].split(
            "function Get-PngReceipt", 1
        )[0]
        self.assertIn("R33_CAPTURE_SAFE_LOCAL_OBSERVED", readiness)
        self.assertIn("presentation=SafeLocal", wait)
        self.assertIn("this run cannot claim no failover/recovery diagnostic", wait)
        self.assertIn("R33_CAPTURE_SAFE_LOCAL_OBSERVED", pose_wait)
        pending_parser = self.wrapper.split(
            "function Assert-PendingCaptureReceipt", 1
        )[1].split("function Invoke-VisualReviewAcceptance", 1)[0]
        for marker in (
            "'AutomaticVisualAcceptanceAllowed' 'pending R33 receipt') -ne $false",
            "'PresentationTransition'",
            "'RequestedPresentation'",
            "'RequestAcknowledgement'",
            "'WarmingObserved'",
            "'StableSeconds'",
            "'FinalReport'",
            "one grouped transition",
            "presentation=SafeLocal",
        ):
            self.assertIn(marker, pending_parser)

    def test_partial_source_promotion_has_internal_rollback(self):
        install = self.wrapper.split("function Install-CaptureSourceClosure", 1)[
            1
        ].split("function Restore-CaptureSourceClosure", 1)[0]
        self.assertLess(install.index("$journal.Add"), install.index("[IO.File]::Copy"))
        for marker in (
            "catch {",
            "$promotionFailure",
            "$internalRollbackErrors",
            "[IO.File]::Delete($entry.Path)",
            "internally rolled-back partial R33 capture source promotion",
            "internal rollback was incomplete",
        ):
            self.assertIn(marker, install)

    def test_no_stale_r32_remote_endpoint_or_turf_capture_marker(self):
        for forbidden in (
            "SetIstanaExploreV5DR32Player0CapturePose",
            "GetIstanaExploreV5DR32Player0CaptureState",
            "CaptureIstanaExploreV5DR32Player0TurfView",
            "FinishIstanaExploreV5DR32Player0CaptureRun",
            "TurfProbe",
            "TurfCullIntersection",
            "TurfIntersectionDistance",
        ):
            self.assertNotIn(forbidden, self.wrapper)

    def test_pending_and_acceptance_keep_r34_closed(self):
        acceptance = self.wrapper.split("function Invoke-VisualReviewAcceptance", 1)[
            1
        ].split("$modeCount", 1)[0]
        pending = self.wrapper.split("$pending = [pscustomobject] [ordered] @{", 1)[
            1
        ].split("$captureCompleted = $true", 1)[0]
        self.assertIn("R34AdmissionAuthorized=$false", acceptance)
        self.assertNotIn("R34AdmissionAuthorized=$true", acceptance)
        self.assertIn("R34AdmissionAuthorized=$false", pending)
        for marker in (
            "ExplicitHumanReviewAcceptance=$true",
            "ConfirmedEightImagesReviewed=$true",
            "HumanVisualReviewAttested=$true",
            "AutomaticVisualAcceptanceAllowed=$false",
            "VisualReviewRequired=$false",
            "VisualReviewAccepted=$true",
            "EightPngsRedecodedAndRehashed=$true",
            "NativeProjectWriteAllowed=$false",
            "UnrealLaunchAllowed=$false",
        ):
            self.assertIn(marker, acceptance)

    def test_acceptance_cannot_launch_or_write_native_project(self):
        acceptance = self.wrapper.split("function Invoke-VisualReviewAcceptance", 1)[
            1
        ].split("$modeCount", 1)[0]
        for forbidden in (
            "Start-GuardedOwnedProcess",
            "Invoke-GuardedCommand",
            "Install-CaptureSourceClosure",
            "Restore-CaptureSourceClosure",
            "UnrealEditor-Cmd.exe",
        ):
            self.assertNotIn(forbidden, acceptance)

    def test_fixed_memory_serial_build_fresh_cook_and_airsim(self):
        for marker in (
            "10737418240L # fixed 10 GiB",
            "12884901888L # fixed 12 GiB owned tree",
            "6442450944L # fixed continuous 6 GiB",
            "-ForceHeaderGeneration",
            "-NoUBTMakefiles",
            "-MaxParallelActions=1",
            "-NoUBA",
            "-NoUBALocal",
            "-run=Cook",
            "-OutputDir=$cookOutputPattern",
            "-COOKDIR=$airSimRuntimeContentRoot",
            "AirSimTriadRuntime",
        ):
            self.assertIn(marker, self.wrapper + self.cpp)
        self.assertNotIn("'-iterate'", self.wrapper.lower())
        self.assertIn("R33_CAPTURE_LAUNCH_HEADROOM_REFUSED", self.wrapper)
        self.assertNotIn("R32_CAPTURE_LAUNCH_HEADROOM_REFUSED", self.wrapper)

    def test_static_self_check_and_explicit_confirmation_gate(self):
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
        self.assertEqual(report["ExactPoseCount"], 4)
        self.assertEqual(report["ExactPresentationStateCount"], 2)
        self.assertEqual(report["ExactCoreCaptureCount"], 8)
        self.assertTrue(report["RepositoryOnly"])
        self.assertFalse(report["NativeAccess"])
        self.assertFalse(report["UnrealLaunched"])
        self.assertFalse(report["R34AdmissionAuthorized"])

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
                r"D:\triad\TRIAD_R33Evidence\review-required\pending-visual-review.json",
                "-ExpectedPendingCaptureReceiptSha256",
                "0" * 64,
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("ConfirmEightImagesReviewed", rejected.stdout + rejected.stderr)

    def test_documentation_preserves_visual_only_boundary(self):
        combined = self.readme + self.guide
        for marker in (
            "PENDING_VISUAL_REVIEW",
            "ConfirmEightImagesReviewed",
            "R34AdmissionAuthorized=false",
            "all eight PNGs",
            "SafeLocal",
            "survey accuracy",
            "vertical datum",
            "provider readiness",
        ):
            self.assertIn(marker, combined)


if __name__ == "__main__":
    unittest.main()
