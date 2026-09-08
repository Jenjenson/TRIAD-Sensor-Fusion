from pathlib import Path
import json
import re
import shutil
import subprocess
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[4]
WRAPPER = (
    REPO
    / "scripts/Measure-IstanaExploreV5DCesiumFirstPoseMemoryQueueDiagnosticV1.ps1"
)


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    return text[begin : text.index(end, begin)]


class IstanaExploreV5DCesiumFirstPoseDiagnosticContractTests(
    unittest.TestCase
):
    @classmethod
    def setUpClass(cls) -> None:
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")

    def test_window_is_strictly_bounded_to_five_through_ten_minutes(self):
        self.assertIn("[ValidateRange(300, 600)]", self.wrapper)
        self.assertIn("[int] $DiagnosticDurationSeconds = 600", self.wrapper)
        self.assertIn(
            "$diagnosticStartedUtc.AddSeconds($DiagnosticDurationSeconds)",
            self.wrapper,
        )
        self.assertIn(
            "[DateTime]::UtcNow -lt $diagnosticDeadline", self.wrapper
        )

    def test_memory_guards_are_fixed_and_checked_during_the_window(self):
        self.assertIn(
            "$privateMemoryCeilingBytes = 12884901888L", self.wrapper
        )
        self.assertIn(
            "$minimumSystemFreeVirtualBytes = 6442450944L", self.wrapper
        )
        self.assertIn(
            "$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L",
            self.wrapper,
        )
        startup_guard = between(
            self.wrapper,
            "function Assert-SystemStartupHeadroom {",
            "function Assert-MemoryBudget {",
        )
        self.assertIn("$snapshot.FreeVirtualBytes -lt", startup_guard)
        self.assertIn("MEMORY_GUARD_STARTUP_HEADROOM", startup_guard)
        preflight = between(
            self.wrapper,
            "$cacheBefore = Get-GrowingFileState",
            "$script:protectedUE54Before = Get-ProtectedUE54Identity",
        )
        self.assertIn("Assert-SystemStartupHeadroom", preflight)
        self.assertLess(
            preflight.index("Assert-SystemStartupHeadroom"),
            preflight.index("Assert-SystemMemoryBudget"),
        )
        guard = between(
            self.wrapper,
            "function Assert-MemoryBudget {",
            "function Invoke-GuardedRcJson {",
        )
        self.assertIn("$snapshot.ProcessPrivateBytes -ge", guard)
        self.assertIn("$snapshot.SystemFreeVirtualBytes -lt", guard)
        self.assertIn("MEMORY_GUARD_PRIVATE_BYTES", guard)
        self.assertIn("MEMORY_GUARD_SYSTEM_FREE_VIRTUAL", guard)
        rc = between(
            self.wrapper,
            "function Invoke-GuardedRcJson {",
            "function Invoke-OwnedRcCall {",
        )
        self.assertIn('Checkpoint "during RC $Operation"', rc)
        measurement = between(
            self.wrapper,
            "$diagnosticStartedUtc = [DateTime]::UtcNow",
            "catch {\n    $workflowError",
        )
        self.assertIn('Checkpoint "between diagnostic samples', measurement)

    def test_independent_watchdog_covers_startup_map_and_pie_readiness(self):
        self.assertIn(
            "$memoryWatchdogPollMilliseconds = 100", self.wrapper
        )
        self.assertIn(
            "$memoryWatchdogPersistentBreachMilliseconds = 2000",
            self.wrapper,
        )
        watchdog = between(
            self.wrapper,
            "function Initialize-ContinuousMemoryWatchdogType {",
            "function Get-SystemMemorySnapshot {",
        )
        for marker in (
            "GlobalMemoryStatusEx",
            "new Thread(this.Run)",
            "this.thread.IsBackground = true",
            "process.PrivateMemorySize64",
            "memory.AvailablePageFile",
            "actualTicks != this.creationUtcTicks",
            "ProcessModule mainModule = candidate.MainModule;",
            "mainModule == null ||",
            "String.IsNullOrWhiteSpace(mainModule.FileName)",
            "Path.GetFullPath(mainModule.FileName)",
            "Process candidate = null;",
            "candidate = null;",
            "catch (System.ComponentModel.Win32Exception)",
            "candidate.Dispose();",
            "StringComparison.OrdinalIgnoreCase",
            "this.persistentBreachMilliseconds",
            "process.Kill()",
            "ExactIdentityVerifiedAtContainment",
            "ForceKillUsed",
        ):
            self.assertIn(marker, watchdog)
        self.assertNotIn("candidate.MainModule.FileName", watchdog)

        live = self.wrapper[
            self.wrapper.index("$argumentLine =") :
            self.wrapper.index("$protectedUE54After =")
        ]
        compile_index = live.index(
            "Initialize-ContinuousMemoryWatchdogType"
        )
        launch_index = live.index("$process = Start-Process")
        start_index = live.index(
            "$script:memoryWatchdog = Start-ContinuousMemoryWatchdog"
        )
        identity_index = live.index(
            "$identityDeadline = [DateTime]::UtcNow.AddSeconds(20)"
        )
        self.assertLess(compile_index, launch_index)
        self.assertLess(launch_index, start_index)
        self.assertLess(start_index, identity_index)
        self.assertIn("Checkpoint 'startup identity acquisition'", live)
        self.assertIn("Checkpoint 'editor readiness'", live)

        guarded = between(
            self.wrapper,
            "function Invoke-GuardedRcJson {",
            "function Invoke-OwnedRcCall {",
        )
        self.assertIn('Checkpoint "during RC $Operation"', guarded)
        self.assertIn("Assert-MemoryBudget", guarded)

    def test_watchdog_csharp_source_compiles(self):
        pwsh = shutil.which("pwsh") or shutil.which("pwsh.exe")
        if pwsh is None:
            self.skipTest("PowerShell 7 is unavailable")
        match = re.search(
            r"\$source = @'\r?\n(?P<source>.*?)\r?\n'@",
            self.wrapper,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(match)
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "ContinuousMemoryWatchdog.cs"
            source.write_text(match.group("source"), encoding="utf-8")
            completed = subprocess.run(
                [
                    pwsh,
                    "-NoProfile",
                    "-NonInteractive",
                    "-Command",
                    "Add-Type -Path '"
                    + str(source).replace("'", "''")
                    + "' -ErrorAction Stop; "
                    "if ($null -eq ('Triad.CesiumDiagnostics."
                    "ContinuousMemoryWatchdog' -as [type])) { exit 7 }",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
        self.assertEqual(0, completed.returncode, completed.stderr)

    def test_memory_abort_remains_non_proof_but_publishes_safety_receipt(self):
        postflight = self.wrapper[self.wrapper.index("$protectedUE54After =") :]
        self.assertIn("$watchdogMemoryAbort", postflight)
        self.assertIn("$memoryAbortCleanupObservations", postflight)
        self.assertIn("ABORTED_BY_MEMORY_GUARD_NON_PROOF", postflight)
        self.assertIn("ContinuousWatchdog = [pscustomobject]", postflight)
        self.assertIn("EmergencyCleanupObservations", postflight)
        self.assertIn("ProviderReadyProofClaimed = $false", postflight)
        self.assertIn("PublicationPerformed = $false", postflight)

    def test_staged_selector_profile_reduces_duplicate_first_pose_load(self):
        self.assertIn(
            "[ValidateSet('EditorWorldSuspendedPoseBeforeResumeV1')]",
            self.wrapper,
        )
        self.assertIn("$editorTilesetObjectPath", self.wrapper)
        self.assertIn("$pieTilesetObjectPath", self.wrapper)
        setter = between(
            self.wrapper,
            "function Set-OwnedTilesetSuspendUpdate {",
            "function Set-OwnedLogSelectionStats {",
        )
        self.assertIn("propertyName = 'SuspendUpdate'", setter)
        self.assertIn("generateTransaction = $false", setter)
        for forbidden in (
            "MaximumScreenSpaceError",
            "MaximumCachedBytes",
            "MaximumSimultaneousTileLoads",
            "LoadingDescendantLimit",
            "PreloadAncestors",
            "PreloadSiblings",
        ):
            self.assertNotIn(forbidden, setter)

        live = self.wrapper[
            self.wrapper.index("$argumentLine =") :
            self.wrapper.index("# Start at byte zero")
        ]
        editor_suspend = live.index(
            "-ObjectPath $editorTilesetObjectPath -Suspended $true"
        )
        map_validation = live.index(
            "-FunctionName 'ValidateIstanaExploreV5DHybridMap'"
        )
        begin_pie = live.index("-FunctionName 'EditorRequestBeginPlay'")
        inherited = live.index(
            "$pieInheritedSuspendedState =", begin_pie
        )
        set_pose = live.index(
            "-FunctionName 'SetIstanaExploreV5DHybridPlayViewPoseForQa'"
        )
        pie_resume = live.index(
            "-ObjectPath $pieTilesetObjectPath -Suspended $false"
        )
        editor_still_suspended = live.index(
            "$editorWorldRemainedSuspendedDuringPie =", pie_resume
        )
        self.assertLess(editor_suspend, map_validation)
        self.assertLess(map_validation, begin_pie)
        self.assertLess(begin_pie, inherited)
        self.assertLess(inherited, set_pose)
        self.assertLess(set_pose, pie_resume)
        self.assertLess(pie_resume, editor_still_suspended)
        self.assertNotIn("SavePackage", self.wrapper)
        self.assertIn(
            "SerializedOrProofTimeQualityTupleMutated = $false",
            self.wrapper,
        )

    def test_uses_bounded_existing_cesium_selection_stats_telemetry(self):
        self.assertIn("$selectionStatsPulseMilliseconds = 250", self.wrapper)
        setter = between(
            self.wrapper,
            "function Set-OwnedLogSelectionStats {",
            "function Wait-OwnedPieState {",
        )
        self.assertIn("$rcPropertyUri", setter)
        self.assertIn("propertyName = 'LogSelectionStats'", setter)
        self.assertIn("generateTransaction = $false", setter)
        self.assertIn("access = 'READ_ACCESS'", setter)
        self.assertIn("LogSelectionStats readback", setter)
        for field in (
            "Visited",
            "CulledVisited",
            "Rendered",
            "WaitingForOcclusionResults",
            "MaxDepthVisited",
            "LoadingWorker",
            "LoadingMain",
            "LoadedTilesPercent",
        ):
            self.assertIn(field, self.wrapper)
        self.assertIn("SanitizedAggregateOnly = $true", self.wrapper)
        self.assertIn("RawLogLinesCopiedToReceipt = $false", self.wrapper)

    def test_first_pose_is_the_exact_existing_far_rig_pose(self):
        pose = between(
            self.wrapper,
            "$firstPose = [pscustomobject] [ordered] @{",
            "$requiredMapValidationMarkers = @(",
        )
        for marker in (
            "Label = '075m'",
            "X = 3500.0",
            "Y = 15000.0",
            "Z = 164.0",
            "Pitch = -1.252968",
            "Yaw = -90.0",
            "Roll = 0.0",
        ):
            self.assertIn(marker, pose)
        live = self.wrapper[
            self.wrapper.index("$setPose = Invoke-RequiredOwnedRcCall") :
        ]
        self.assertIn(
            "SetIstanaExploreV5DHybridPlayViewPoseForQa", live
        )
        self.assertNotIn("foreach ($pose", live)

    def test_no_capture_screenshot_or_proof_publication_api_is_called(self):
        for forbidden in (
            "CaptureIstanaExploreV5DHybridPlayView",
            "CaptureIstanaExploreV5DHybridDiagnosticPlayView",
            "TakeHighResScreenShot",
            "HighResShot",
            ".png",
        ):
            self.assertNotIn(forbidden, self.wrapper)
        self.assertIn("CaptureCount = 0", self.wrapper)
        self.assertIn("PublicationPerformed = $false", self.wrapper)
        self.assertIn("ProofEligible = $false", self.wrapper)
        self.assertIn("ProviderReadyProofClaimed = $false", self.wrapper)
        self.assertIn("GlobalProviderReadyGateRequired = $false", self.wrapper)

    def test_exact_map_dll_and_current_v2_contract_are_pinned(self):
        for pin in (
            "$mapPin",
            "$runtimeDllPin",
            "$editorDllPin",
        ):
            self.assertIn(f"{pin} = Get-FileIdentity", self.wrapper)
            self.assertIn(pin, between(
                self.wrapper,
                "$script:boundaryPins = @(",
                "Assert-FilePins -Pins $script:boundaryPins -Checkpoint 'preflight'",
            ))
        for marker in (
            "maximumSse=1.0",
            "simultaneousLoads=12",
            "currentContextTriangles=43448",
            "currentContextSuppressedTriangles=96",
            "contextFacadeR25=true",
        ):
            self.assertIn(f"'{marker}'", self.wrapper)

    def test_preserves_exact_multi_session_capstone_snapshot(self):
        self.assertIn(
            r"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe",
            self.wrapper,
        )
        self.assertIn(
            r"C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject",
            self.wrapper,
        )
        snapshot = between(
            self.wrapper,
            "function Get-ProtectedUE54Identity {",
            "function Assert-ProtectedUE54Unchanged {",
        )
        self.assertIn("CreationUtcTicks", snapshot)
        self.assertIn("Sort-Object ProcessId", snapshot)
        self.assertIn("SessionCount = $identities.Count", snapshot)
        self.assertIn("Sessions = @($identities)", snapshot)
        self.assertNotIn("ExpectedState", snapshot)
        self.assertIn(
            "$script:protectedUE54Before = Get-ProtectedUE54Identity",
            self.wrapper,
        )
        self.assertIn(
            "the protected UE5.4/CAPSTONE set may remain open",
            self.wrapper,
        )

    def test_rc_is_loopback_allowlisted_owned_and_released(self):
        self.assertIn(
            "http://127.0.0.1:30010/remote/object/call", self.wrapper
        )
        self.assertIn(
            "http://127.0.0.1:30010/remote/object/property", self.wrapper
        )
        guarded = between(
            self.wrapper,
            "function Invoke-GuardedRcJson {",
            "function Invoke-OwnedRcCall {",
        )
        self.assertIn("$remoteControlEndpointAllowlist -cnotcontains", guarded)
        self.assertIn("Assert-OwnedBoundary", guarded)
        self.assertIn("before RC $Operation", guarded)
        self.assertIn("after RC $Operation", guarded)
        postflight = self.wrapper[self.wrapper.index("$protectedUE54After =") :]
        self.assertIn("Get-RemoteControlListeners", postflight)
        self.assertIn("RC port 30010 still has a listener", postflight)

    def test_persistent_cache_is_observed_but_never_cleared_or_relocated(self):
        self.assertIn(
            "PRESERVE_CESIUM_STANDARD_PERSISTENT_HTTP_CACHE_NO_CLEAR_NO_RELOCATION",
            self.wrapper,
        )
        self.assertIn(
            "$persistentCachePath = 'D:\\triad\\TRIAD\\cesium-request-cache.sqlite'",
            self.wrapper,
        )
        self.assertIn("$cacheBefore = Get-GrowingFileState", self.wrapper)
        self.assertIn("$cacheAfter = Get-GrowingFileState", self.wrapper)
        self.assertIn("RemovedClearedOrRelocatedByWrapper = $false", self.wrapper)
        self.assertNotIn("Remove-Item", self.wrapper)
        self.assertNotIn("Clear-Content", self.wrapper)

    def test_receipt_is_versioned_atomic_and_explicitly_non_proof(self):
        self.assertIn(
            "cesium_first_pose_memory_queue_diagnostic.v1", self.wrapper
        )
        self.assertIn("DiagnosticReceiptIsProofPublication = $false", self.wrapper)
        self.assertIn("COMPLETED_NON_PROOF_DIAGNOSTIC", self.wrapper)
        self.assertIn("ABORTED_BY_MEMORY_GUARD_NON_PROOF", self.wrapper)
        self.assertIn("[IO.File]::WriteAllText", self.wrapper)
        self.assertIn("[IO.File]::Move", self.wrapper)
        self.assertIn("Refusing to overwrite", self.wrapper)
        self.assertIn(
            "ProviderUrlOrCredentialCopiedToTelemetryOrReceipt = $false",
            self.wrapper,
        )

    def test_static_self_check_is_side_effect_free_and_complete(self):
        pwsh = shutil.which("pwsh") or shutil.which("pwsh.exe")
        if pwsh is None:
            self.skipTest("PowerShell 7 is unavailable")
        completed = subprocess.run(
            [
                pwsh,
                "-NoProfile",
                "-NonInteractive",
                "-File",
                str(WRAPPER),
                "-RunToken",
                "contract_diag_v1",
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
            "triad.istana_explore_v5d.cesium_first_pose_memory_queue_diagnostic.v1",
            report["Schema"],
        )
        self.assertEqual(600, report["DiagnosticDurationSeconds"])
        self.assertEqual(5, report["TelemetryIntervalSeconds"])
        self.assertEqual(
            12884901888,
            report["MemoryGuard"]["PrivateMemoryCeilingBytes"],
        )
        self.assertEqual(
            6442450944,
            report["MemoryGuard"]["MinimumSystemFreeVirtualBytes"],
        )
        self.assertEqual(
            10737418240,
            report["MemoryGuard"][
                "MinimumSystemFreeVirtualAtLaunchBytes"
            ],
        )
        self.assertEqual(
            100,
            report["MemoryGuard"]["ContinuousWatchdog"][
                "PollMilliseconds"
            ],
        )
        self.assertEqual(
            "EditorWorldSuspendedPoseBeforeResumeV1",
            report["CesiumStreamingIsolation"]["Profile"],
        )
        self.assertFalse(
            report["CesiumStreamingIsolation"]
            ["SerializedOrProofTimeQualityTupleMutated"]
        )
        self.assertEqual("075m", report["FirstPose"]["Label"])
        self.assertFalse(report["ProofEligible"])
        self.assertFalse(report["ProviderReadyProofClaimed"])
        self.assertFalse(report["PublicationPerformed"])
        self.assertEqual(0, report["CaptureCount"])
        self.assertFalse(report["LiveEditorLaunched"])
        self.assertIn(
            "contextFacadeR25=true",
            report["RequiredMapValidationMarkers"],
        )


if __name__ == "__main__":
    unittest.main()
