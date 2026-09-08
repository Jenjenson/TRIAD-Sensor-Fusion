import hashlib
import json
import pathlib
import re
import shutil
import subprocess
import unittest


REPO = pathlib.Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal" / "Plugins" / "TRIADSensorFusion"
PUBLIC = PLUGIN / "Source" / "TRIADSensorFusion" / "Public"
PRIVATE = PLUGIN / "Source" / "TRIADSensorFusion" / "Private"
HEADER = PUBLIC / "TRIADIstanaExploreV5DR31Player0CaptureLibrary.h"
CPP = PRIVATE / "TRIADIstanaExploreV5DR31Player0CaptureLibrary.cpp"
WRAPPER = REPO / "scripts" / "Capture-IstanaExploreV5DR31Player0Evidence.ps1"
CONTRACT = (
    REPO
    / "unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
    "R31BroadShellLookdev/r31_broad_shell_lookdev.contract.json"
)
VISUAL_REVIEW_CONTRACT = CONTRACT.with_name(
    "r31_player0_visual_review.contract.json"
)
VISUAL_REVIEW_CONTRACT_BYTES = 1749
VISUAL_REVIEW_CONTRACT_SHA256 = (
    "5C07E6B0129AD67ED2999E23D1B2C38A23422097A7096718CE3FCB2FE9D5AAAC"
)
README = CONTRACT.with_name("README.md")


class IstanaExploreV5DR31Player0CaptureContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.cpp = CPP.read_text(encoding="utf-8")
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
        cls.visual_review_contract = json.loads(
            VISUAL_REVIEW_CONTRACT.read_text(encoding="utf-8")
        )
        cls.readme = README.read_text(encoding="utf-8")

    def test_contract_declares_source_complete_but_unexecuted_capture_gate(self):
        gate = self.contract["nativeCaptureGate"]
        self.assertEqual(
            gate["schema"], "triad.istana_explore_v5d.r31_player0_capture.v2"
        )
        self.assertTrue(gate["implemented"])
        self.assertFalse(gate["executed"])
        self.assertFalse(gate["accepted"])
        self.assertTrue(gate["twoPhaseVisualReviewRequired"])
        self.assertFalse(gate["automaticVisualAcceptanceAllowed"])
        self.assertEqual(
            gate["pendingReceipt"]["status"], "PENDING_VISUAL_REVIEW"
        )
        self.assertTrue(gate["pendingReceipt"]["mechanicalValidationOnly"])
        self.assertFalse(gate["pendingReceipt"]["visualReviewAccepted"])
        self.assertTrue(
            gate["pendingReceipt"]["naniteRasterHighOccupancyComparisonRequired"]
        )
        self.assertFalse(
            gate["pendingReceipt"]["humanNaniteRasterComparisonAttested"]
        )
        self.assertFalse(
            gate["pendingReceipt"]["naniteRasterAppearanceParityAccepted"]
        )
        self.assertFalse(gate["pendingReceipt"]["r32AdmissionAuthorized"])
        self.assertEqual(gate["acceptanceMode"]["switch"], "AcceptVisualReview")
        self.assertEqual(
            gate["acceptanceMode"]["fiveImageReviewConfirmationSwitch"],
            "ConfirmFiveImagesReviewed",
        )
        self.assertEqual(
            gate["acceptanceMode"]["naniteRasterPairReviewConfirmationSwitch"],
            "ConfirmNaniteRasterPairReviewedAndAccepted",
        )
        self.assertTrue(
            gate["acceptanceMode"]["callerPendingReceiptSha256Required"]
        )
        self.assertFalse(gate["acceptanceMode"]["relaunchesUnreal"])
        self.assertFalse(gate["acceptanceMode"]["writesNativeState"])
        self.assertEqual(
            gate["acceptanceMode"]["acceptedReceiptStatus"], "COMMITTED"
        )
        self.assertEqual(
            gate["nativeOrder"], "R31_COMMIT_THEN_R31_CAPTURE_BEFORE_R32"
        )
        self.assertFalse(gate["r32DependencyAllowed"])
        self.assertEqual(gate["exactPoseCount"], 5)
        self.assertEqual(gate["exactImageCount"], 6)
        self.assertEqual(gate["contextPolicyShell"], "R31_BROAD_SHELL")
        self.assertEqual(
            gate["poses"],
            [
                "075m",
                "020m",
                "008m",
                "002m",
                "surroundings_oblique_macdonald",
            ],
        )
        self.assertTrue(gate["image"]["decodeRequired"])
        self.assertTrue(gate["image"]["nonBlankRequired"])
        self.assertTrue(
            gate["image"]["baselineFiveDistinctFileAndDecodedPixelIdentityRequired"]
        )
        self.assertTrue(gate["image"]["naniteRasterPairDistinctFilesRequired"])
        self.assertFalse(
            gate["image"]["naniteRasterPairDecodedPixelDifferenceRequired"]
        )
        self.assertTrue(gate["fullNativePluginSourceTreeReceiptRequired"])
        self.assertTrue(gate["mapDllGroundAndSourceTreeNoMutationRequired"])
        self.assertTrue(
            gate["downstreamReceiptFieldsAuthoritativeOnlyAfterExplicitAcceptance"]
        )
        for row in gate["sourcePins"]:
            path = REPO / row["file"]
            payload = path.read_bytes()
            self.assertEqual(len(payload), row["bytes"])
            self.assertEqual(
                hashlib.sha256(payload).hexdigest().upper(), row["sha256"]
            )

    def test_runtime_blueprint_library_exposes_only_bounded_capture_seam(self):
        self.assertIn("UBlueprintFunctionLibrary", self.header)
        for endpoint in (
            "GetIstanaExploreV5DR31Player0CaptureState",
            "SetIstanaExploreV5DR31Player0CapturePose",
            "SetIstanaExploreV5DR31Player0CaptureRenderPath",
            "CaptureIstanaExploreV5DR31Player0FallbackView",
            "FinishIstanaExploreV5DR31Player0CaptureRun",
        ):
            self.assertIn(endpoint, self.header)
            self.assertIn(endpoint, self.cpp)
        for forbidden in ("GEditor", "UnrealEd", "LevelEditor", "R32", "R33"):
            self.assertNotIn(forbidden, self.header)
            self.assertNotIn(forbidden, self.cpp)

    def test_separate_hash_pinned_contract_mirrors_human_attestation_truth(self):
        payload = VISUAL_REVIEW_CONTRACT.read_bytes()
        self.assertEqual(len(payload), VISUAL_REVIEW_CONTRACT_BYTES)
        self.assertEqual(
            hashlib.sha256(payload).hexdigest().upper(),
            VISUAL_REVIEW_CONTRACT_SHA256,
        )
        contract = self.visual_review_contract
        self.assertEqual(
            contract["schema"],
            "triad.istana_explore_v5d.r31_player0_visual_review.v2",
        )
        self.assertFalse(
            contract["pendingReceipt"]["humanVisualReviewAttested"]
        )
        self.assertTrue(
            contract["acceptedReceipt"]["humanVisualReviewAttested"]
        )
        self.assertTrue(
            contract["acceptedReceipt"]["r31BroadShellVisualQaAccepted"]
        )
        self.assertFalse(
            contract["pendingReceipt"]["humanNaniteRasterComparisonAttested"]
        )
        self.assertFalse(
            contract["pendingReceipt"]["naniteRasterAppearanceParityAccepted"]
        )
        self.assertTrue(
            contract["acceptedReceipt"]["confirmedNaniteRasterPairReviewed"]
        )
        self.assertTrue(
            contract["acceptedReceipt"]["humanNaniteRasterComparisonAttested"]
        )
        self.assertTrue(
            contract["acceptedReceipt"]["naniteRasterAppearanceParityAccepted"]
        )
        self.assertEqual(contract["acceptedReceipt"]["exactImageCount"], 6)
        self.assertIn(f"Bytes={VISUAL_REVIEW_CONTRACT_BYTES}L", self.wrapper)
        self.assertIn(
            f"Sha256='{VISUAL_REVIEW_CONTRACT_SHA256}'", self.wrapper
        )
        self.assertIn("`HumanVisualReviewAttested=false`", self.readme)
        self.assertIn("`HumanVisualReviewAttested=true`", self.readme)

    def test_exact_game_world_player0_and_five_pose_roster(self):
        for marker in (
            "EWorldType::Game",
            "/Game/Maps/Istana_PublicView_Explore_v5d_hybrid",
            "GetPlayerController(\n        OutState.World, 0)",
            "ATRIADIstanaExploreV5Pawn",
            "ATRIADIstanaExploreV5GameMode::StaticClass()",
            "static_assert(UE_ARRAY_COUNT(ReviewedPoses) == 5)",
            "LocationToleranceCentimetres = 0.1f",
            "RotationToleranceDegrees = 0.05f",
        ):
            self.assertIn(marker, self.cpp)
        expected = {
            "075m": (3500.0, 15000.0, 164.0, -1.252968, -90.0, 0.0),
            "020m": (3500.0, 15000.0, 164.0, -4.703535, -90.0, 0.0),
            "008m": (3500.0, 15000.0, 164.0, -11.829499, -90.0, 0.0),
            "002m": (3500.0, 15000.0, 164.0, -55.084794, -90.0, 0.0),
            "surroundings_oblique_macdonald": (
                40226.9640238642,
                106152.591966384,
                3900.0,
                -5.74137954693854,
                -101.620267003074,
                0.0,
            ),
        }
        for pose_id, values in expected.items():
            self.assertIn(f'TEXT("{pose_id}")', self.cpp)
            self.assertIn(f"Id='{pose_id}'", self.wrapper)
            for value in values:
                self.assertIn(str(value), self.wrapper)

    def test_provider_fallback_truth_is_fail_closed(self):
        for marker in (
            "contextPolicyShell=R31BroadShell",
            "r30Owner=1",
            "r29VegetationOwner=1",
            "r29TerrainOwner=1",
            "treeRealismOwner=1",
            "providerReadyForProof=%s",
            "providerFallbackVisualQa=%s",
            "providerReadyProofClaimed=false",
            "localFallbackHidden=false",
            "r30Visible=true",
            "r31BroadShellVisible=true",
            "State.Policy->bLocalBuildingFallbackCurrentlyHidden",
            "State.R30->bProviderReady",
            "visualCaptureAccepted=false",
            "captureRevalidationRequired=true",
        ):
            self.assertIn(marker, self.cpp)
        self.assertNotIn("SetProviderReady", self.cpp)
        self.assertNotIn("ProviderReadyProofClaimed=$true", self.wrapper)
        self.assertIn("ProviderReadyCaptureAccepted=$false", self.wrapper)
        self.assertIn("HyperrealismClaimed=$false", self.wrapper)

    def test_r31_r30_r29_and_cesium_runtime_validation_is_explicit(self):
        for marker in (
            "ValidateR30FacadeLookdev",
            "ValidateCurrentSurroundingsBroadShellR31ForInheritedScene",
            "ValidateR29Vegetation",
            "ValidateCopernicusTerrainFallback",
            "ValidateTreeRealism",
            "GooglePhotorealistic3DTilesIonAssetId = 2275207",
            "IstanaLongitudeDegrees = 103.84288055",
            "IstanaLatitudeDegrees = 1.30709615",
            "IstanaFallbackEllipsoidHeightMetres = 47.0",
            "GetMaximumScreenSpaceError()",
            "GetLoadProgress()",
        ):
            self.assertIn(marker, self.cpp)

    def test_airsim_is_retained_in_project_cook_binary_and_runtime_contract(self):
        for marker in (
            "AirSimTriadRuntime",
            "/AirSimTriadRuntime/Weather/WeatherFX/WeatherActor.WeatherActor_C",
            "IsModuleLoaded(FName(*AirSimRuntimeModule))",
            "weatherActorResolved=true",
        ):
            self.assertIn(marker, self.cpp)
        for marker in (
            "AirSimTriadRuntime=$true",
            "AirSim=$false",
            "-COOKDIR=$airSimRuntimeContentRoot",
            "WeatherActor.uasset",
            "AirSimDisableArgumentsUsed=$false",
        ):
            self.assertIn(marker, self.wrapper)
        for forbidden in (
            "-DisablePlugin=AirSim",
            "-DisablePlugins=AirSim",
            "-DisablePlugin=AirSimTriadRuntime",
            "-DisablePlugins=AirSimTriadRuntime",
            "-NoAirSim",
        ):
            declarations = re.findall(
                rf"^\s*'{re.escape(forbidden)}',?\s*$",
                self.wrapper,
                flags=re.MULTILINE,
            )
            self.assertEqual(len(declarations), 1)

    def test_hash_pinned_source_promotion_and_exact_rollback_are_wired(self):
        for path, expected_bytes, expected_sha in (
            (
                HEADER,
                2629,
                "779B7974B1C70CD15334F47A68BEEFD338CAE9AD53F9F0049B101FFA0BA973B9",
            ),
            (
                CPP,
                40565,
                "F41E6E4651B647C51B789C949B73119DEE688249CBA509436DD63D278C2A70C7",
            ),
        ):
            data = path.read_bytes()
            self.assertEqual(len(data), expected_bytes)
            self.assertEqual(hashlib.sha256(data).hexdigest().upper(), expected_sha)
            self.assertIn(f"Bytes = {expected_bytes}L", self.wrapper)
            self.assertIn(f"Sha256 = '{expected_sha}'", self.wrapper)
        for marker in (
            "Assert-CaptureSourcePins $repositoryUnrealRoot",
            "New-FileJournal",
            "New-TreeJournal",
            "Copy-Item -LiteralPath $source -Destination $destination -Force",
            "Assert-CaptureSourcePins $nativeProjectRoot",
            "Wait-NativeMutationQuiescence",
            "Restore-TreeJournal @($buildJournal)",
            "Restore-FileJournal @($sourceJournal)",
            "ExactOwnedProcessContainmentOnly=$true",
        ):
            self.assertIn(marker, self.wrapper)

    def test_committed_r31_receipt_is_hash_bound_before_capture(self):
        for marker in (
            "ExpectedR31CommitReceiptSha256",
            "triad.istana_explore_v5d.broad_shell_r31.native_transaction.v1",
            "'COMMITTED'",
            "R31ContentPackageCount",
            "R30FacadeCueCoexists",
            "V2BroadShellMeshRetained",
            "ContextPolicyShellValidatedBeforeAndAfter",
            "NativeSourceClosureFiles",
            "R33SourceOrDeclarationAllowed",
            "VisualCaptureAccepted",
            "CaptureRevalidationRequired",
            "current R31 successor map",
            "current R31 runtime editor DLL",
            "current R31 editor DLL",
        ):
            self.assertIn(marker, self.wrapper)
        self.assertIn("R31_COMMIT_THEN_R31_CAPTURE_BEFORE_R32", self.wrapper)
        self.assertIn("R32DependencyAllowed=$false", self.wrapper)

    def test_ground_and_full_native_source_tree_are_cross_bound(self):
        for marker in (
            "GroundHeaderCaptureBinding=[pscustomobject] [ordered]",
            "GroundSourceCaptureBinding=[pscustomobject] [ordered]",
            "NativePluginSourceTreeCaptureBinding=[pscustomobject] [ordered]",
            "BaselineAfterCaptureSourcePromotion",
            "ImmediatePreCapture",
            "ImmediatePostCapture",
            "Unchanged=$true",
            "24562L",
            "599358BC0E6290DAB276890BF118A6E5EE59AD62888B847330A9A991B6E2BDE3",
            "246667L",
            "3F00D319112C3E7C7F1172FD128C71360C025B43B6BE2FB138A63DE07C1FDB17",
            "Assert-CaptureSourceTreeSnapshot",
        ):
            self.assertIn(marker, self.wrapper)
        for marker in (
            "PolicyTagCount != 1",
            "R30TagCount != 1",
            "VegetationTagCount != 1",
            "TerrainTagCount != 1",
            "TreeTagCount != 1",
            "R28FacadeClassCount != 0",
            "R29FacadeClassCount != 0",
            "LandmarkVegetationClassCount != 0",
            "ExactV2ComponentCount != 1",
            "ExpectedCurrentSurroundingsV2MeshObjectPath()",
        ):
            self.assertIn(marker, self.cpp)

    def test_fixed_memory_guards_and_exact_process_identity_are_preserved(self):
        for marker in (
            "10737418240L # fixed 10 GiB",
            "12884901888L # fixed 12 GiB",
            "6442450944L # fixed continuous 6 GiB",
            "$memoryWatchdogPollMilliseconds = 500",
            "$memoryWatchdogPersistentBreachMilliseconds = 2000",
            "process.Kill(true)",
            "creationUtcTicks",
            "executablePath",
            "Assert-ExactOwnedProcessIdentity",
            "Assert-ProtectedUnchanged",
            "-MaxParallelActions=1",
            "-NoUBA",
            "-NoUBALocal",
        ):
            self.assertIn(marker, self.wrapper)

    def test_standalone_game_build_fresh_cook_and_sandbox_are_explicit(self):
        for marker in (
            "'TRIAD',\n        'Win64',\n        'Development'",
            "standalone R31 Development Game executable",
            "-ForceHeaderGeneration",
            "-NoUBTMakefiles",
            "-run=Cook",
            "-TargetPlatform=Windows",
            "-OutputDir=$cookOutputPattern",
            "Cooked\\[Platform]",
            "Cooked\\Windows",
            "-Sandbox=$sandboxRoot",
            "-TRIADR31CaptureRun=$RunToken",
            "-TRIADR31CaptureOutputRoot=$evidenceRoot",
        ):
            self.assertIn(marker, self.wrapper)
        self.assertNotIn("-iterate", self.wrapper.lower())

    def test_cooked_and_png_actual_file_closure_is_hash_checked(self):
        for marker in (
            "Istana_PublicView_Explore_v5d_hybrid.umap",
            "cooked R31 successor map export",
            "Cooked R31 namespace is not the exact five-package roster",
            "requiredCookedDependencyRoots",
            "SurroundingsShellLookdevR31",
            "SurroundingsLookdevR30",
            "LocalFallbackSuppressionV2",
            "AssetRegistry.bin",
            "*.ushaderbytecode",
            "DecodedBgraSha256",
            "Present=$true",
            "Assert-CookedClosureUnchanged",
            "WidthPixels=2560",
            "HeightPixels=1440",
            "NonBlank=$true",
            "SampledDistinctColorCount",
            "SampledLuminanceRange",
            "blank/near-uniform",
            "stable -ge 3",
            "Sort-Object -Unique).Count -ne 5",
        ):
            self.assertIn(marker, self.wrapper)

    def test_atomic_receipts_and_bounded_cleanup_are_explicit(self):
        for marker in (
            "function Write-JsonAtomic",
            "Move-Item -LiteralPath $temporary -Destination $Path",
            "admission.json",
            "pending-visual-review.json",
            "commit.json",
            "rollback.json",
            "Remove-ContainedDirectory",
            "ROLLBACK_INCOMPLETE",
            "ROLLED_BACK",
        ):
            self.assertIn(marker, self.wrapper)
        self.assertNotIn("Remove-Item -LiteralPath $nativeProjectRoot", self.wrapper)

    def test_powershell_parser_and_static_self_check_pass_without_native_access(self):
        pwsh = shutil.which("pwsh")
        if pwsh is None:
            self.skipTest("pwsh is unavailable")
        parser = subprocess.run(
            [
                pwsh,
                "-NoProfile",
                "-Command",
                (
                    "$t=$null;$e=$null;"
                    "[void][System.Management.Automation.Language.Parser]::"
                    f"ParseFile('{WRAPPER}',[ref]$t,[ref]$e);"
                    "if($e.Count){$e|% Message;exit 1}"
                ),
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(parser.returncode, 0, parser.stdout + parser.stderr)
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
        self.assertEqual(report["ExactPoseCount"], 5)
        self.assertEqual(report["ExactImageCount"], 6)
        self.assertFalse(report["NativeProjectAccessed"])
        self.assertFalse(report["NativeTreeWritten"])
        self.assertFalse(report["UnrealLaunched"])
        self.assertFalse(report["ProviderReadyProofClaimed"])
        self.assertFalse(report["R32DependencyAllowed"])
        self.assertTrue(report["TwoPhaseVisualReviewRequired"])
        self.assertFalse(report["AutomaticVisualAcceptanceAllowed"])
        self.assertTrue(report["ExplicitFiveImageReviewConfirmationRequired"])
        self.assertTrue(report["ExplicitNaniteRasterPairReviewRequired"])
        self.assertTrue(report["HumanVisualReviewAttestationRequired"])
        self.assertEqual(report["PendingCaptureStatus"], "PENDING_VISUAL_REVIEW")
        self.assertEqual(report["R31ContentPackageCount"], 5)
        self.assertEqual(report["R30Owner"], "EXACT_R30_FACADE_LOOKDEV")
        self.assertEqual(report["ContextPolicyShell"], "R31_BROAD_SHELL")
        self.assertTrue(report["NativePluginSourceTreeRequired"])
        self.assertTrue(report["MemoryWatchdogTypeCompiled"])

    def test_accepted_receipt_exposes_exact_downstream_gate_fields(self):
        for marker in (
            "R31BroadShellVisualQaAccepted=$true",
            "MapModifiedByCapture=$false",
            "SimulationCollisionNavigationSensorRfModified=$false",
            "ProviderFallbackVisualQaAccepted=$true",
            "ExactPoseCount=5",
            "ExactImageCount=6",
            "GroundHeader=(Get-RequiredPropertyValue",
            "GroundSource=(Get-RequiredPropertyValue",
            "NativePluginSourceTree=(Get-RequiredPropertyValue",
            "PendingCaptureAdmission=[pscustomobject] [ordered]",
            "ExplicitHumanReviewAcceptance=$true",
            "HumanVisualReviewAttested=$true",
            "ConfirmedNaniteRasterPairReviewed=$true",
            "HumanNaniteRasterComparisonAttested=$true",
            "NaniteRasterAppearanceParityAccepted=$true",
            "NaniteRasterHighOccupancyComparison=(Get-RequiredPropertyValue",
            "R32AdmissionAuthorized=$true",
            "ImmediatePostAcceptance=$nativeBoundaryAfter",
        ):
            self.assertIn(marker, self.wrapper)

    def test_capture_stops_at_pending_and_cannot_automatically_accept(self):
        start = self.wrapper.index("$pendingCapture = [pscustomobject] [ordered] @{")
        end = self.wrapper.index(
            "$pendingCapture | ConvertTo-Json -Depth 24", start
        )
        pending_path = self.wrapper.index("'pending-visual-review.json'", start, end)
        pending_block = self.wrapper[start:end]
        self.assertGreater(pending_path, start)
        for marker in (
            "Status='PENDING_VISUAL_REVIEW'",
            "MechanicalCaptureValidationPassed=$true",
            "HumanVisualReviewAttested=$false",
            "ConfirmedNaniteRasterPairReviewed=$false",
            "HumanNaniteRasterComparisonAttested=$false",
            "NaniteRasterAppearanceParityAccepted=$false",
            "VisualReviewRequired=$true",
            "VisualReviewAccepted=$false",
            "AutomaticVisualAcceptanceAllowed=$false",
            "R32AdmissionAuthorized=$false",
            "$captureCompleted = $true",
        ):
            self.assertIn(marker, pending_block)
        for accepted_only in (
            "Status='COMMITTED'",
            "R31BroadShellVisualQaAccepted=",
            "ProviderFallbackVisualQaAccepted=",
            "MapModifiedByCapture=",
            "SimulationCollisionNavigationSensorRfModified=",
        ):
            self.assertNotIn(accepted_only, pending_block)
        self.assertNotIn("(Join-Path $evidenceRoot 'commit.json')", pending_block)
        pending_validator_start = self.wrapper.index(
            "function Assert-R31PendingCaptureReceipt"
        )
        pending_validator_end = self.wrapper.index(
            "function Get-R31AcceptanceNativeBoundary", pending_validator_start
        )
        pending_validator = self.wrapper[
            pending_validator_start:pending_validator_end
        ]
        self.assertIn(
            "'HumanVisualReviewAttested' 'pending R31 capture receipt'",
            pending_validator,
        )
        self.assertIn("-ne $false", pending_validator)

    def test_explicit_acceptance_is_hash_pinned_and_native_read_only(self):
        start = self.wrapper.index("function Invoke-R31VisualReviewAcceptance")
        end = self.wrapper.index("$staticReceipt = Assert-StaticContract", start)
        acceptance = self.wrapper[start:end]
        for marker in (
            "Assert-R31PendingCaptureReceipt",
            "ExpectedPendingCaptureReceiptSha256",
            "ConfirmFiveImagesReviewed",
            "ConfirmNaniteRasterPairReviewedAndAccepted",
            "SixPngsRedecodedAndRehashed=$true",
            "NativeStateMutatedByAcceptance=$false",
            "UnrealLaunchedByAcceptance=$false",
            "R31BroadShellVisualQaAccepted=$true",
            "HumanVisualReviewAttested=$true",
            "ConfirmedNaniteRasterPairReviewed=$true",
            "HumanNaniteRasterComparisonAttested=$true",
            "NaniteRasterAppearanceParityAccepted=$true",
            "ProviderFallbackVisualQaAccepted=$true",
            "MapModifiedByCapture=$false",
            "SimulationCollisionNavigationSensorRfModified=$false",
            'Move-Item -LiteralPath $candidatePath -Destination $commitPath',
        ):
            self.assertIn(marker, self.wrapper)
        for native_mutation_or_launch in (
            "Start-GuardedOwnedProcess",
            "Invoke-GuardedCommand",
            "Copy-Item -LiteralPath $source",
            "Restore-TreeJournal",
            "Restore-FileJournal",
            "UnrealEditor-Cmd.exe",
        ):
            self.assertNotIn(native_mutation_or_launch, acceptance)

    def test_acceptance_refuses_to_start_without_five_image_confirmation(self):
        pwsh = shutil.which("pwsh")
        if pwsh is None:
            self.skipTest("pwsh is unavailable")
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
                r"D:\triad\TRIAD_R31Evidence\review-required\pending-visual-review.json",
                "-ExpectedPendingCaptureReceiptSha256",
                "0" * 64,
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn(
            "ConfirmFiveImagesReviewed", rejected.stdout + rejected.stderr
        )


if __name__ == "__main__":
    unittest.main()
