from pathlib import Path
import json
import re
import shutil
import subprocess
import unittest


REPO = Path(__file__).resolve().parents[4]
LEAF = REPO / "scripts/Capture-IstanaExploreV5DVegetationProviderEvidence.ps1"
RETRY = (
    REPO
    / "scripts/Invoke-IstanaExploreV5DVegetationProviderReadyRetry.ps1"
)
DIAGNOSTIC = (
    REPO
    / "scripts/Measure-IstanaExploreV5DCesiumFirstPoseMemoryQueueDiagnosticV1.ps1"
)


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    return text[begin : text.index(end, begin)]


class IstanaExploreV5DProviderReadyRetryContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.leaf = LEAF.read_text(encoding="utf-8")
        cls.retry = RETRY.read_text(encoding="utf-8")
        cls.diagnostic = DIAGNOSTIC.read_text(encoding="utf-8")

    def test_leaf_rate_limit_abort_is_opt_in_and_provider_ready_only(self):
        self.assertIn(
            "[int] $ProviderReadyRateLimitAbortThreshold = 0", self.leaf
        )
        self.assertIn(
            "$ProviderEvidenceMode -cne 'ProviderReady' -and", self.leaf
        )
        self.assertIn(
            "-ProviderEvidenceMode ProviderReady", self.leaf
        )
        self.assertIn(
            "'Received status code 429 for tile content '", self.leaf
        )
        gate = between(
            self.leaf,
            "function Assert-NoProviderReadyRateLimitBurst {",
            "function Wait-StableExactPoseState {",
        )
        self.assertIn("PROVIDER_RATE_LIMIT_RETRYABLE", gate)
        self.assertIn(
            "$observed -ge $ProviderReadyRateLimitAbortThreshold", gate
        )

    def test_leaf_checks_429_before_accepting_or_capturing_a_pose(self):
        wait = between(
            self.leaf,
            "function Wait-StableExactPoseState {",
            "function Get-PngEvidence {",
        )
        self.assertLess(
            wait.index("Assert-NoProviderReadyRateLimitBurst"),
            wait.index("$accepted = Test-ExactPoseState"),
        )
        capture = self.leaf[self.leaf.index("foreach ($pose in $poses)") :]
        self.assertLess(
            capture.index(
                'Assert-NoProviderReadyRateLimitBurst `\n'
                '            -Checkpoint "pose-$($pose.Label)-immediate-pre-capture"'
            ),
            capture.index("$captureRequestedUtc = [DateTime]::UtcNow"),
        )
        postcheck = between(
            self.leaf,
            "function Assert-NoVegetationRuntimeFailure {",
            "if (-not (Test-Path -LiteralPath $outputRoot",
        )
        self.assertIn("Assert-NoProviderReadyRateLimitBurst", postcheck)

    def test_retry_is_bounded_and_only_explicit_429_is_retryable(self):
        self.assertIn("[ValidateRange(1, 3)]", self.retry)
        self.assertIn("[int] $MaximumAttempts = 2", self.retry)
        self.assertIn("[int] $CooldownSeconds = 180", self.retry)
        self.assertIn("[int] $RateLimitAbortThreshold = 1", self.retry)
        attempt = between(
            self.retry,
            "for ($attempt = 1;",
            "if ($null -eq $successfulLeaf",
        )
        self.assertIn("ProviderEvidenceMode = 'ProviderReady'", attempt)
        self.assertIn(
            "ProviderReadyRateLimitAbortThreshold =", attempt
        )
        self.assertIn("PROVIDER_RATE_LIMIT_RETRYABLE", attempt)
        self.assertIn(
            "$rateLimitCount -ge $RateLimitAbortThreshold", attempt
        )
        self.assertIn("$cleanRateLimitShutdown", attempt)
        self.assertIn("cleanup= postconditions=Capture count mismatch:", attempt)
        self.assertIn("if (-not $retryable)", attempt)
        self.assertIn("if ($attempt -eq $MaximumAttempts)", attempt)

    def test_retry_defaults_to_the_committed_r25_native_successor(self):
        for fragment in (
            "[int64] $ExpectedMapBytes = 34993427L",
            "38114240B7A0C673B2492B74DE7AA2EB22E9E5E87349E448310FC001688D89D9",
            "[int64] $ExpectedRuntimeDllBytes = 4585984L",
            "31B6EF8FE3044176AE311D6665B822713483D92C293056FDF8507E3087F0ABC4",
            "[int64] $ExpectedEditorDllBytes = 7671296L",
            "471DBFE1F3BA1CB54D346CCFE96747A30F11CE089EDC83D1BC4BE909ED4D1E34",
        ):
            self.assertIn(fragment, self.retry)
            self.assertIn(fragment, self.leaf)

    def test_retry_revalidates_the_r25_context_marker(self):
        self.assertIn(
            "$requiredContextFacadeR25Marker = 'contextFacadeR25=true'",
            self.retry,
        )
        acceptance = between(
            self.retry,
            "$manifest = Get-Content -LiteralPath $manifestPin.Path -Raw |",
            "foreach ($captureEvidence in @($manifest.Captures))",
        )
        self.assertIn("$manifest.MapValidationReport", acceptance)
        self.assertIn("$requiredContextFacadeR25Marker", acceptance)
        self.assertIn("[StringComparison]::Ordinal", acceptance)

    def test_retry_is_fully_r27_aware_and_preserves_legacy_capture_count(self):
        for fragment in (
            "[switch] $RequireLandmarkVegetationR27",
            "[string] $R27CommitReceiptPath = ''",
            "$ExpectedR27CommitReceiptSha256",
            "$expectedCaptureCount = if ($RequireLandmarkVegetationR27) { 7 } else { 6 }",
            "$leafArguments.RequireLandmarkVegetationR27 = $true",
            "$leafArguments.R27CommitReceiptPath = $R27CommitReceiptPath",
            "$leafArguments.ExpectedR27CommitReceiptSha256 =",
            "$leafResult.Captures.Count -ne $expectedCaptureCount",
            "$manifest.Captures.Count -ne $expectedCaptureCount",
            "LANDMARK_VEGETATION_R27_STRICT_CAPTURE",
            "012m_ground_grazing_r27",
        ):
            self.assertIn(fragment, self.retry)
        self.assertNotIn("$leafResult.Captures.Count -ne 6", self.retry)
        self.assertNotIn("$manifest.Captures.Count -ne 6", self.retry)

    def test_retry_validates_and_boundary_pins_the_exact_r27_receipt(self):
        receipt = between(
            self.retry,
            "function Get-ValidatedR27CommitReceipt {",
            "function Test-ExactCommandLineToken {",
        )
        self.assertIn("V5DLandmarkVegetationR27V1", self.retry)
        for fragment in (
            "commit.json",
            "landmark_vegetation_r27.native_transaction.v1",
            "VisualCaptureAccepted",
            "CaptureRevalidationRequired",
            "CollisionNavigationSensorRfAuthority",
            "SourceCount",
            "Phase2CompiledSourceCount",
            "RenderingCapableFreshEditorProcesses",
            "SuccessorMap",
            "RuntimeDll",
            "EditorDll",
            "R27MaterialPackages",
            "04_cold_validate_r27_map",
            "06_cold_validate_phase2_post_r27",
        ):
            self.assertIn(fragment, receipt)
        boundary = between(
            self.retry,
            "function Assert-RetryBoundary {",
            "function Wait-ExactCooldown {",
        )
        self.assertIn("$script:r27CommitReceiptEvidence.Pin", boundary)

    def test_retry_restarts_helper_and_revalidates_exact_pins(self):
        self.assertIn('$attemptToken = "${RunToken}_a${attempt}"', self.retry)
        self.assertIn("& $captureWrapper @leafArguments", self.retry)
        boundary = between(
            self.retry,
            "function Assert-RetryBoundary {",
            "function Wait-ExactCooldown {",
        )
        for pin in (
            "$script:selfPin",
            "$script:captureWrapperPin",
            "$script:mapPin",
            "$script:runtimeDllPin",
            "$script:editorDllPin",
        ):
            self.assertIn(pin, boundary)
        self.assertIn("Assert-NoNativeTRIADOrRemoteControlOwner", boundary)
        cooldown = between(
            self.retry,
            "function Wait-ExactCooldown {",
            "$script:selfPin =",
        )
        self.assertIn("[Math]::Min(30", cooldown)
        self.assertIn("Assert-RetryBoundary", cooldown)
        self.assertIn('Checkpoint "before-attempt-$attempt"', self.retry)

    def test_retry_preserves_the_exact_multi_session_capstone_snapshot(self):
        self.assertIn(
            r"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe",
            self.retry,
        )
        self.assertIn(
            r"C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject",
            self.retry,
        )
        snapshot = between(
            self.retry,
            "function Get-ProtectedUE54Identity {",
            "function Assert-ProtectedUE54Unchanged {",
        )
        self.assertIn("CreationUtcTicks", snapshot)
        self.assertIn("Sort-Object ProcessId", snapshot)
        self.assertIn("SessionCount = $identities.Count", snapshot)
        self.assertIn("Sessions = @($identities)", snapshot)
        self.assertNotIn("ExpectedState", snapshot)
        unchanged = between(
            self.retry,
            "function Assert-ProtectedUE54Unchanged {",
            "function Get-NativeTRIADUnrealProcesses {",
        )
        self.assertIn("ConvertTo-Json -Compress -Depth 8", unchanged)
        self.assertIn(
            "$script:protectedUE54Before = Get-ProtectedUE54Identity",
            self.retry,
        )
        self.assertIn("ProtectedUE54Before = $script:protectedUE54Before", self.retry)
        self.assertIn("ProtectedUE54After = $protectedUE54After", self.retry)
        self.assertNotIn("Assert-NoUnrealOrRemoteControlOwner", self.retry)

    def test_retry_rejects_only_native_ue55_or_exact_triad_helpers(self):
        scoped = between(
            self.retry,
            "function Get-NativeTRIADUnrealProcesses {",
            "function Get-RemoteControlListeners {",
        )
        self.assertIn("$nativeUE55EngineRoot", scoped)
        self.assertIn("-Token $projectFile", scoped)
        self.assertIn("$isUE55 -or $isNativeTRIAD", scoped)
        self.assertNotIn("CAPSTONE", scoped)

    def test_both_wrappers_accept_an_explicit_one_hour_provider_timeout(self):
        self.assertIn("[ValidateRange(60, 3600)]", self.retry)
        self.assertIn("[ValidateRange(60, 3600)]", self.leaf)
        pwsh = shutil.which("pwsh") or shutil.which("pwsh.exe")
        if pwsh is None:
            self.skipTest("PowerShell 7 is unavailable")
        for wrapper in (RETRY, LEAF):
            completed = subprocess.run(
                [
                    pwsh,
                    "-NoProfile",
                    "-NonInteractive",
                    "-File",
                    str(wrapper),
                    "-RunToken",
                    "contract_timeout",
                    "-ProviderTimeoutSeconds",
                    "3600",
                    "-StaticSelfCheck",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(0, completed.returncode, completed.stderr)
            self.assertEqual(3600, json.loads(completed.stdout)["ProviderTimeoutSeconds"])

    def test_retry_preserves_cache_and_does_not_weaken_ready_gate(self):
        self.assertIn(
            "PRESERVE_CESIUM_STANDARD_PERSISTENT_HTTP_CACHE_NO_CLEAR_NO_RELOCATION",
            self.retry,
        )
        self.assertNotIn("Remove-Item", self.retry)
        self.assertIn("ReadyThresholdPercent = 98.0", self.retry)
        self.assertIn("RequiredConsecutiveReadySamples = 3", self.retry)
        self.assertIn(
            "$manifest.ProviderHandling.RequiredProviderReadyForProof -ne",
            self.retry,
        )
        self.assertIn(
            "$manifest.ProviderHandling.RequiredLocalFallbackHidden -ne",
            self.retry,
        )
        self.assertIn(
            "$manifest.ProviderHandling.GlobalProviderReadyGateRequired -ne",
            self.retry,
        )
        self.assertIn("$readyReports.Count -lt 3", self.retry)
        self.assertIn("$readySamples.Count -lt 3", self.retry)
        self.assertIn("'providerReadyForProof=true'", self.retry)
        self.assertIn("'localFallbackHidden=true'", self.retry)
        self.assertIn(
            "$captureEvidence.ImmediatePreCaptureProviderTelemetry.ProviderReadyForProof",
            self.retry,
        )
        self.assertIn(
            "$captureEvidence.PostCaptureProviderTelemetry.ProviderReadyForProof",
            self.retry,
        )
        self.assertIn("PartialEvidencePublicationAllowed = $false", self.retry)

    def test_retry_static_self_check_is_side_effect_free(self):
        pwsh = shutil.which("pwsh") or shutil.which("pwsh.exe")
        if pwsh is None:
            self.skipTest("PowerShell 7 is unavailable")
        completed = subprocess.run(
            [
                pwsh,
                "-NoProfile",
                "-NonInteractive",
                "-File",
                str(RETRY),
                "-RunToken",
                "contract_retry",
                "-StaticSelfCheck",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(0, completed.returncode, completed.stderr)
        report = json.loads(completed.stdout)
        self.assertEqual("STATIC_SELF_CHECK_PASS", report["Status"])
        self.assertEqual("ProviderReady", report["ProviderEvidenceMode"])
        self.assertFalse(report["LandmarkVegetationR27Required"])
        self.assertEqual(6, report["ExpectedCaptureCount"])
        self.assertEqual(2, report["MaximumAttempts"])
        self.assertEqual(180, report["CooldownSeconds"])
        self.assertEqual(1, report["RateLimitAbortThreshold"])
        self.assertEqual(98.0, report["ReadyThresholdPercent"])
        self.assertEqual(3, report["RequiredConsecutiveReadySamples"])
        self.assertEqual(900, report["ProviderTimeoutSeconds"])
        self.assertEqual(
            "SNAPSHOT_EXACT_CAPSTONE_IDENTITY_SET_AND_REQUIRE_UNCHANGED_AT_EVERY_BOUNDARY",
            report["ProtectedUE54Policy"],
        )
        self.assertTrue(report["NativeUE55AndTRIADHelpersMustBeIdle"])
        self.assertFalse(report["PartialEvidencePublicationAllowed"])
        self.assertEqual(
            "contextFacadeR25=true",
            report["RequiredContextFacadeR25Marker"],
        )
        expected_identities = report["ExpectedNativeIdentities"]
        self.assertEqual(34993427, expected_identities["Map"]["Bytes"])
        self.assertEqual(
            "38114240B7A0C673B2492B74DE7AA2EB22E9E5E87349E448310FC001688D89D9",
            expected_identities["Map"]["Sha256"],
        )
        self.assertEqual(4585984, expected_identities["RuntimeDll"]["Bytes"])
        self.assertEqual(
            "31B6EF8FE3044176AE311D6665B822713483D92C293056FDF8507E3087F0ABC4",
            expected_identities["RuntimeDll"]["Sha256"],
        )
        self.assertEqual(7671296, expected_identities["EditorDll"]["Bytes"])
        self.assertEqual(
            "471DBFE1F3BA1CB54D346CCFE96747A30F11CE089EDC83D1BC4BE909ED4D1E34",
            expected_identities["EditorDll"]["Sha256"],
        )

    def test_retry_r27_static_self_check_is_seven_pose_and_side_effect_free(self):
        pwsh = shutil.which("pwsh") or shutil.which("pwsh.exe")
        if pwsh is None:
            self.skipTest("PowerShell 7 is unavailable")
        completed = subprocess.run(
            [
                pwsh,
                "-NoProfile",
                "-NonInteractive",
                "-File",
                str(RETRY),
                "-RunToken",
                "contract_retry_r27",
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
        self.assertTrue(report["LandmarkVegetationR27Required"])
        self.assertEqual(7, report["ExpectedCaptureCount"])
        expectation = report["R27CommitReceiptExpectation"]
        self.assertTrue(expectation["Required"])
        self.assertEqual("A" * 64, expectation["Sha256"])
        self.assertEqual(4, expectation["ExactMaterialPackageCount"])
        self.assertEqual(7, expectation["ExactFreshEditorStageCount"])

    def test_leaf_has_exact_continuous_watchdog_and_enables_provider_ready(self):
        for fragment in (
            "$privateMemoryCeilingBytes = 12884901888L",
            "$minimumSystemFreeVirtualBytes = 6442450944L",
            "$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L",
            "$memoryWatchdogPollMilliseconds = 100",
            "$memoryWatchdogPersistentBreachMilliseconds = 2000",
            "$continuousMemoryWatchdogIntegrated = $true",
        ):
            self.assertIn(fragment, self.leaf)

        startup_guard = between(
            self.leaf,
            "function Assert-SystemStartupHeadroom {",
            "function Get-FileStateIdentity {",
        )
        self.assertIn("$snapshot.FreeVirtualBytes -lt", startup_guard)
        self.assertIn("MEMORY_GUARD_STARTUP_HEADROOM", startup_guard)

        source_pattern = re.compile(
            r"\$source = @'\r?\n(?P<source>.*?)\r?\n'@", re.DOTALL
        )
        leaf_source = source_pattern.search(self.leaf)
        diagnostic_source = source_pattern.search(self.diagnostic)
        self.assertIsNotNone(leaf_source)
        self.assertIsNotNone(diagnostic_source)
        self.assertEqual(
            diagnostic_source.group("source"), leaf_source.group("source")
        )
        for marker in (
            "GlobalMemoryStatusEx",
            "new Thread(this.Run)",
            "this.thread.IsBackground = true",
            "process.PrivateMemorySize64",
            "memory.AvailablePageFile",
            "actualTicks != this.creationUtcTicks",
            "StringComparison.OrdinalIgnoreCase",
            "this.persistentBreachMilliseconds",
            "process.Kill()",
            "ExactIdentityVerifiedAtContainment",
            "ForceKillUsed",
        ):
            self.assertIn(marker, leaf_source.group("source"))

        launch = between(
            self.leaf,
            "Assert-FilePins -Pins $script:boundaryPins -Checkpoint 'preflight'",
            "$identityDeadline = [DateTime]::UtcNow.AddSeconds(20)",
        )
        compile_index = launch.index("Initialize-ContinuousMemoryWatchdogType")
        admission_index = launch.index("Assert-SystemStartupHeadroom")
        process_index = launch.index("$process = Start-Process")
        watchdog_index = launch.index(
            "$script:memoryWatchdog = Start-ContinuousMemoryWatchdog"
        )
        self.assertLess(compile_index, admission_index)
        self.assertLess(admission_index, process_index)
        self.assertLess(process_index, watchdog_index)
        self.assertIn("Checkpoint 'startup identity acquisition'", self.leaf)
        self.assertIn("Checkpoint 'editor readiness'", self.leaf)

        boundary = between(
            self.leaf,
            "function Assert-OwnedBoundary {",
            "function Invoke-RcCall {",
        )
        self.assertIn("[switch] $SkipMemoryGuard", boundary)
        self.assertGreaterEqual(
            boundary.count("Assert-ContinuousMemoryWatchdogHealthy"), 2
        )
        graceful = between(
            self.leaf,
            "function Invoke-GracefulQuit {",
            "function Stop-ExactHelperForContainment {",
        )
        self.assertIn("-SkipMemoryGuard", graceful)

        cleanup = between(
            self.leaf,
            "finally {\n    $preCleanupWatchdogSnapshot",
            "try {\n    Assert-ProtectedUE54Unchanged",
        )
        self.assertIn("Emergency QuitEditor", cleanup)
        self.assertIn("Stop-ContinuousMemoryWatchdog", cleanup)
        self.assertIn("nonProofAbort=true", cleanup)
        postflight = self.leaf[self.leaf.index("$r28VisualCaptureAccepted =") :]
        self.assertIn("proofPublished=false", postflight)
        self.assertIn("visualCaptureAccepted=false", postflight)
        self.assertIn("partialEvidencePublicationAllowed=false", postflight)
        self.assertLess(
            postflight.index("if ($watchdogMemoryAbort)"),
            postflight.index("$result = [pscustomobject]"),
        )

        pwsh = shutil.which("pwsh") or shutil.which("pwsh.exe")
        if pwsh is None:
            self.skipTest("PowerShell 7 is unavailable")
        static = subprocess.run(
            [
                pwsh,
                "-NoProfile",
                "-NonInteractive",
                "-File",
                str(LEAF),
                "-RunToken",
                "contract_leaf_memory_static",
                "-ProviderEvidenceMode",
                "ProviderReady",
                "-StaticSelfCheck",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(0, static.returncode, static.stderr)
        memory = json.loads(static.stdout)["MemorySafety"]
        self.assertEqual(12884901888, memory["PrivateMemoryCeilingBytes"])
        self.assertEqual(6442450944, memory["MinimumSystemFreeVirtualBytes"])
        self.assertEqual(
            10737418240,
            memory["MinimumSystemFreeVirtualAtLaunchBytes"],
        )
        self.assertEqual(100, memory["PollMilliseconds"])
        self.assertEqual(
            2000, memory["PersistentBreachContainmentMilliseconds"]
        )
        self.assertTrue(memory["PreLaunchAdmissionApplied"])
        self.assertTrue(memory["ContinuousMemoryWatchdogIntegrated"])
        self.assertTrue(memory["StrictProviderReadyLiveLaunchEnabled"])
        self.assertTrue(memory["IndependentClrThread"])
        self.assertTrue(
            memory["ExactPidCreationTimeAndExecutableRequiredForContainment"]
        )
        self.assertTrue(memory["MemoryAlertSuppressesProofPublication"])
        self.assertIsNone(memory["RequiredFutureHardening"])

    def test_leaf_rejects_rate_limit_abort_for_non_ready_static_mode(self):
        pwsh = shutil.which("pwsh") or shutil.which("pwsh.exe")
        if pwsh is None:
            self.skipTest("PowerShell 7 is unavailable")
        completed = subprocess.run(
            [
                pwsh,
                "-NoProfile",
                "-NonInteractive",
                "-File",
                str(LEAF),
                "-RunToken",
                "contract_invalid_retry",
                "-ProviderEvidenceMode",
                "ProviderFallback",
                "-ProviderReadyRateLimitAbortThreshold",
                "1",
                "-StaticSelfCheck",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertNotEqual(0, completed.returncode)
        self.assertIn(
            "valid only with -ProviderEvidenceMode ProviderReady",
            completed.stderr,
        )


if __name__ == "__main__":
    unittest.main()
