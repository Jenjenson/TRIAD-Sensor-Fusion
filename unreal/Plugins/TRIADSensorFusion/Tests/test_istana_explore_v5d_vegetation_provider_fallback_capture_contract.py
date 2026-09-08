from pathlib import Path
import json
import shutil
import subprocess
import unittest


REPO = Path(__file__).resolve().parents[4]
WRAPPER = (
    REPO
    / "scripts/Capture-IstanaExploreV5DVegetationProviderEvidence.ps1"
)


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    return text[begin : text.index(end, begin)]


class IstanaExploreV5DVegetationProviderFallbackCaptureContractTests(
    unittest.TestCase
):
    @classmethod
    def setUpClass(cls) -> None:
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")

    def test_capture_process_pins_provider_safe_http_concurrency(self) -> None:
        self.assertIn("HttpMaxConnectionsPerServer=12", self.wrapper)
        self.assertNotIn("HttpMaxConnectionsPerServer=16", self.wrapper)
        self.assertIn("MaxCacheItems=32768", self.wrapper)
        self.assertIn("$captureTextureStreamingPoolMiB = 768", self.wrapper)
        self.assertIn(
            "-ini:Engine:[ConsoleVariables]:r.Streaming.PoolSize="
            "$captureTextureStreamingPoolMiB",
            self.wrapper,
        )
        argument_line = between(
            self.wrapper, '$argumentLine = "', "\n$oldLocalDdc"
        )
        self.assertEqual(
            argument_line.count("$captureTextureStreamingPoolOverrideToken"),
            1,
        )
        self.assertEqual(
            argument_line.count('-ExecCmds=`"WebControl.StartServer`"'),
            1,
        )
        self.assertLess(
            argument_line.index("$captureTextureStreamingPoolOverrideToken"),
            argument_line.index("-ExecCmds="),
        )
        self.assertNotIn("r.Streaming.PoolSize ", argument_line)
        self.assertNotIn("r.Streaming.PoolSize=", argument_line)
        self.assertNotIn(
            '-ExecCmds=`"r.Streaming.PoolSize', self.wrapper
        )

    def test_texture_pool_application_is_proven_before_map_load(self) -> None:
        verifier = between(
            self.wrapper,
            "function Get-TextureStreamingPoolApplicationEvidence {",
            "function Wait-EarlyTextureStreamingPoolApplication {",
        )
        self.assertIn(
            "LogConfig: Applying CVar settings from Section "
            "[ConsoleVariables] File [Engine]",
            verifier,
        )
        self.assertIn("Cmd: MAP LOAD FILE=", verifier)
        self.assertIn("$lastPreMapAssignment", verifier)
        self.assertIn("$captureTextureStreamingPoolMiB", verifier)
        self.assertIn("$postMapAssignments.Count -ne 0", verifier)
        self.assertIn("VerifiedBeforeExactMapLoad = $true", verifier)
        self.assertIn("NoPostMapReassignment = $true", verifier)

        wait_gate = between(
            self.wrapper,
            "function Wait-EarlyTextureStreamingPoolApplication {",
            "function Wait-OwnedRuntimeLogReport {",
        )
        self.assertIn("Assert-ContinuousMemoryWatchdogHealthy", wait_gate)
        self.assertIn("Assert-ProtectedUE54Unchanged", wait_gate)
        self.assertIn("Assert-LaunchedProcessIdentity", wait_gate)

        launch_to_readiness = between(
            self.wrapper,
            "$process = Start-Process",
            "$editorDeadline =",
        )
        self.assertIn(
            "Wait-EarlyTextureStreamingPoolApplication",
            launch_to_readiness,
        )
        self.assertIn(
            "TextureStreamingPoolApplicationEvidence =",
            self.wrapper,
        )

    def test_texture_pool_log_verifier_accepts_only_early_final_value(self):
        pwsh = shutil.which("pwsh") or shutil.which("pwsh.exe")
        if pwsh is None:
            self.skipTest("PowerShell 7 is unavailable")
        wrapper_literal = str(WRAPPER).replace("'", "''")
        harness = f"""
$tokens = $null
$parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    '{wrapper_literal}', [ref] $tokens, [ref] $parseErrors)
if ($parseErrors.Count -ne 0) {{ throw 'wrapper parse failed' }}
$functionAst = $ast.Find({{
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -ceq 'Get-TextureStreamingPoolApplicationEvidence'
}}, $true)
if ($null -eq $functionAst) {{ throw 'verifier function missing' }}
Invoke-Expression $functionAst.Extent.Text
$mapFile = 'D:\\triad\\TRIAD\\Content\\Maps\\Istana_PublicView_Explore_v5d_hybrid.umap'
$captureTextureStreamingPoolMiB = 768
$captureTextureStreamingPoolOverrideToken =
    '-ini:Engine:[ConsoleVariables]:r.Streaming.PoolSize=768'
$engine = 'LogConfig: Applying CVar settings from Section [ConsoleVariables] File [Engine]'
$oldPool = 'LogConfig: Set CVar [[r.Streaming.PoolSize:1000]]'
$newPool = 'LogConfig: Set CVar [[r.Streaming.PoolSize:768]]'
$map = 'Cmd: MAP LOAD FILE="D:/triad/TRIAD/Content/Maps/Istana_PublicView_Explore_v5d_hybrid.umap"'
$good = [string]::Join([Environment]::NewLine,
    @($oldPool, $engine, $newPool, $map))
$evidence = Get-TextureStreamingPoolApplicationEvidence -LogText $good
if (-not $evidence.Applied -or
    -not $evidence.VerifiedBeforeExactMapLoad -or
    -not $evidence.NoPostMapReassignment -or
    $evidence.LastPreMapAssignmentValue -cne '768') {{
    throw 'valid early application was rejected'
}}
foreach ($invalid in @(
    [string]::Join([Environment]::NewLine, @($oldPool, $engine, $map)),
    [string]::Join([Environment]::NewLine, @($oldPool, $newPool, $map)),
    [string]::Join([Environment]::NewLine,
        @($oldPool, $engine, $newPool, $map, $oldPool))
)) {{
    $rejected = $false
    try {{
        Get-TextureStreamingPoolApplicationEvidence -LogText $invalid |
            Out-Null
    }}
    catch {{ $rejected = $true }}
    if (-not $rejected) {{ throw 'invalid pool ordering was accepted' }}
}}
'PASS'
"""
        completed = subprocess.run(
            [pwsh, "-NoProfile", "-NonInteractive", "-Command", harness],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(0, completed.returncode, completed.stderr)
        self.assertEqual("PASS", completed.stdout.strip())

    def test_capture_omits_unused_authoring_plugins_without_disabling_scene_plugins(self):
        self.assertIn(
            "-DisablePlugins=ModelingToolsEditorMode,GeometryScripting,"
            "MeshModelingToolset,MeshModelingToolsetExp,MeshLODToolset,"
            "ToolPresets,StylusInput,PlanarCut,NNEDenoiser,"
            "SkeletalMeshModelingTools,EditorDataStorage,StudioTelemetry,"
            "EditorTelemetry,EditorPerformance,AnimationData,ControlRig,"
            "ControlRigModules,ControlRigSpline,DeformerGraph,FullBodyIK,"
            "IKRig,MetaHumanSDK,RigLogic,RigVM,SequencerAnimTools,"
            "Bridge,HairStrands,AndroidPermission,OnlineSubsystemGooglePlay,"
            "CLionSourceCodeAccess,CodeLiteSourceCodeAccess,GitSourceControl,"
            "KDevelopSourceCodeAccess,N10XSourceCodeAccess,"
            "NullSourceCodeAccess,PerforceSourceControl,PlasticSourceControl,"
            "RiderSourceCodeAccess,SubversionSourceControl,"
            "VisualStudioSourceCodeAccess,VisualStudioCodeSourceCodeAccess,"
            "PluginBrowser,PluginUtils,ChangelistReview,"
            "MobileLauncherProfileWizard",
            self.wrapper,
        )
        disabled = (
            self.wrapper.split("-DisablePlugins=", 1)[1]
            .split(" ", 1)[0]
            .split(",")
        )
        for required_plugin in (
            "CesiumForUnreal",
            "PCG",
            "RemoteControl",
            "GeometryProcessing",
            "PythonScriptPlugin",
            "Niagara",
            "Water",
        ):
            self.assertNotIn(required_plugin, disabled)

    def test_continuous_watchdog_handles_a_transient_null_main_module(self):
        self.assertIn(
            "ProcessModule mainModule = candidate.MainModule;", self.wrapper
        )
        self.assertIn(
            "mainModule == null ||\n"
            "                    String.IsNullOrWhiteSpace(mainModule.FileName)",
            self.wrapper,
        )
        self.assertIn(
            "string actualPath = Path.GetFullPath(mainModule.FileName);",
            self.wrapper,
        )
        self.assertIn("Process candidate = null;", self.wrapper)
        self.assertIn("candidate = null;\n                return true;", self.wrapper)
        self.assertIn(
            "catch (System.ComponentModel.Win32Exception)", self.wrapper
        )
        self.assertIn("finally\n            {", self.wrapper)
        self.assertIn("candidate.Dispose();", self.wrapper)
        self.assertNotIn("candidate.MainModule.FileName", self.wrapper)

    def test_provider_fallback_is_additive_and_explicitly_not_ready_proof(self):
        self.assertIn(
            "[ValidateSet('TelemetryOnly', 'ProviderFallback', 'ProviderReady')]",
            self.wrapper,
        )
        self.assertIn(
            "[string] $ProviderEvidenceMode = 'TelemetryOnly'", self.wrapper
        )
        self.assertIn(
            "STRICT_LOCAL_FALLBACK_RASTER_VISUAL_EVIDENCE_NOT_PROVIDER_READY_PROOF",
            self.wrapper,
        )
        self.assertIn(
            "PROOF_CANDIDATE_STRICT_GLOBAL_PROVIDER_READY_FAIL_CLOSED",
            self.wrapper,
        )
        self.assertIn(
            "EXPLICIT_NON_PROOF_TELEMETRY_DIAGNOSTIC", self.wrapper
        )

    def test_strict_fallback_state_is_required_during_stabilization(self):
        state = between(
            self.wrapper,
            "function Test-ExactPoseState {",
            "function Get-ProviderTelemetrySample {",
        )
        self.assertIn("$EvidenceMode -ceq 'ProviderFallback'", state)
        self.assertIn("'providerReadyForProof=false'", state)
        self.assertIn("'localFallbackHidden=false'", state)

        wait = between(
            self.wrapper,
            "function Wait-StableExactPoseState {",
            "function Get-PngEvidence {",
        )
        self.assertIn(
            "$requireFallback = $ProviderEvidenceMode -ceq 'ProviderFallback'",
            wait,
        )
        self.assertIn("-EvidenceMode $ProviderEvidenceMode", wait)
        self.assertIn("ProviderFallbackRequired = $requireFallback", wait)
        self.assertIn(
            "providerReadyForProof=false and localFallbackHidden=false readbacks",
            wait,
        )

    def test_request_is_preceded_by_a_fresh_strict_state_readback(self):
        stable = self.wrapper.index(
            "$stateWindow = Wait-StableExactPoseState"
        )
        immediate = self.wrapper.index("$preCaptureState = Invoke-OwnedRcCall", stable)
        immediate_gate = self.wrapper.index(
            "-Pose $pose -EvidenceMode $ProviderEvidenceMode", immediate
        )
        capture_time = self.wrapper.index(
            "$captureRequestedUtc = [DateTime]::UtcNow", immediate_gate
        )
        capture_call = self.wrapper.index(
            "$capture = Invoke-RequiredOwnedRcCall", capture_time
        )
        self.assertLess(stable, immediate)
        self.assertLess(immediate, immediate_gate)
        self.assertLess(immediate_gate, capture_time)
        self.assertLess(capture_time, capture_call)
        self.assertIn(
            "ImmediatePreCaptureStateReport", self.wrapper[capture_call:]
        )
        self.assertIn(
            "ImmediatePreCaptureProviderTelemetry", self.wrapper[capture_call:]
        )

    def test_all_non_ready_modes_use_only_the_diagnostic_capture_path(self):
        self.assertNotIn(
            "$ProviderEvidenceMode -ceq 'TelemetryOnly'", self.wrapper
        )
        self.assertGreaterEqual(
            self.wrapper.count("$ProviderEvidenceMode -cne 'ProviderReady'"),
            5,
        )
        capture = between(
            self.wrapper,
            "$captureFunction = if",
            "$png = Wait-StablePng",
        )
        self.assertIn("'CaptureIstanaExploreV5DHybridDiagnosticPlayView'", capture)
        self.assertIn("'CaptureIstanaExploreV5DHybridPlayView'", capture)
        self.assertIn("'NON-PROOF DIAGNOSTIC EVIDENCE:'", capture)
        self.assertIn(
            "'Accepted exact 2560x1440 HDR-off V5D Player0 proof capture'",
            capture,
        )

    def test_capture_acknowledgement_proves_visible_not_hidden_fallback(self):
        self.assertIn(
            "$strictFallbackCaptureAcknowledgementMarker =", self.wrapper
        )
        self.assertIn(
            "'localFallbackVisible=true localFallbackHidden=false'",
            self.wrapper,
        )
        capture = between(
            self.wrapper,
            "$captureFunction = if",
            "$png = Wait-StablePng",
        )
        fallback_gate = capture.index(
            "$ProviderEvidenceMode -ceq 'ProviderFallback'"
        )
        acknowledgement = capture.index(
            "$strictFallbackCaptureAcknowledgementMarker", fallback_gate
        )
        self.assertLess(fallback_gate, acknowledgement)

    def test_post_capture_reuses_the_exact_mode_specific_state_gate(self):
        post = between(
            self.wrapper,
            "function Assert-PostCaptureWorld {",
            "function Wait-ExactProcessExit {",
        )
        self.assertIn("GetIstanaExploreV5DHybridPlayStateReport", post)
        self.assertIn("-EvidenceMode $ProviderEvidenceMode", post)
        self.assertIn(
            "$providerTelemetry = Get-ProviderTelemetrySample -Response $state",
            post,
        )
        self.assertIn("ProviderTelemetry = $providerTelemetry", post)
        self.assertIn("ValidateIstanaExploreV5DHybridPlayWorld", post)

    def test_map_validation_requires_the_successor_quality_receipts(self):
        for marker in (
            "applyDpiScaling=false",
            "forbidHoles=true",
            "loadingDescendantLimit=20",
            "r24CoarseLocalLandmarkShellsSuppressed=true",
            "currentContextSuppressionContract=local_fallback_suppression_v2",
            "currentContextTriangles=43448",
            "currentContextSuppressedTriangles=96",
            "contextFacadeR25=true",
            "outerGroundLoadingFallbackTriangles=1280",
            "outerGroundLoadingFallbackSourceCorners=3840",
            "outerGroundLoadingFallbackRenderVertices=768",
            "outerGroundLoadingFallbackRenderOnly=true",
            "outerGroundCollisionNavigationShadowDistanceFieldSensorRfTerrainAuthority=false",
            "inheritedV5CPlanningGroundHidden=true",
        ):
            self.assertIn(f"'{marker}'", self.wrapper)

    def test_committed_r25_native_successor_is_the_default_identity(self):
        for fragment in (
            "[int64] $ExpectedMapBytes = 34993427L",
            "38114240B7A0C673B2492B74DE7AA2EB22E9E5E87349E448310FC001688D89D9",
            "[int64] $ExpectedRuntimeDllBytes = 4585984L",
            "31B6EF8FE3044176AE311D6665B822713483D92C293056FDF8507E3087F0ABC4",
            "[int64] $ExpectedEditorDllBytes = 7671296L",
            "471DBFE1F3BA1CB54D346CCFE96747A30F11CE089EDC83D1BC4BE909ED4D1E34",
        ):
            self.assertIn(fragment, self.wrapper)

    def test_v2_suppression_and_outer_ground_assets_are_pinned_across_the_live_run(self):
        expected_paths = (
            "LocalFallbackSuppressionV2\\SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.uasset",
            "OuterGroundLoadingFallback\\SM_IPV5D_OuterGroundLoadingFallback_Render.uasset",
            "OuterGroundLoadingFallback\\Materials\\M_IPV5D_OuterGroundLoadingFallback.uasset",
        )
        for path in expected_paths:
            self.assertIn(path, self.wrapper)
        self.assertNotIn("LocalFallbackSuppressionV1", self.wrapper)
        self.assertNotIn("LocalFallbackSuppressed_v1.uasset", self.wrapper)
        self.assertNotIn("local_fallback_suppression_v1", self.wrapper)
        for variable in (
            "$suppressedFallbackMeshPin",
            "$outerGroundMeshPin",
            "$outerGroundMaterialPin",
        ):
            self.assertIn(f"{variable} =", self.wrapper)
            self.assertIn(variable, between(
                self.wrapper,
                "$script:boundaryPins = @(",
                "Assert-FilePins -Pins $script:boundaryPins -Checkpoint 'preflight'",
            ))
            self.assertIn(variable, self.wrapper[self.wrapper.index("$result ="):])

    def test_live_capture_preserves_the_exact_multi_session_capstone_snapshot(self):
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
        self.assertIn("Get-NativeTRIADUnrealProcesses", self.wrapper)
        self.assertIn(
            "the protected UE5.4/CAPSTONE set may remain open", self.wrapper
        )
        self.assertNotIn("Get-UnprotectedUnrealEditors", self.wrapper)
        self.assertNotIn("protectedUE54ExpectedState", self.wrapper)
        self.assertNotIn("protectedUE54ExpectedPid", self.wrapper)

    def test_manifest_records_the_strict_gate_without_claiming_ready_proof(self):
        manifest = self.wrapper[self.wrapper.index("$result =") :]
        for marker in (
            "StrictLocalFallbackGateRequired",
            "RequiredProviderReadyForProof",
            "RequiredLocalFallbackHidden",
            "StrictDiagnosticFallbackAcknowledgementRequired",
            "StrictDiagnosticFallbackAcknowledgementMarker",
            "ProviderReadyProofClaimed",
        ):
            self.assertIn(marker, manifest)
        self.assertIn(
            "$ProviderEvidenceMode -ceq 'ProviderReady'", manifest
        )
        self.assertIn(
            "$ProviderEvidenceMode -ceq 'ProviderFallback'", manifest
        )

    def test_static_self_check_exercises_all_three_modes(self):
        pwsh = shutil.which("pwsh") or shutil.which("pwsh.exe")
        if pwsh is None:
            self.skipTest("PowerShell 7 is unavailable")
        expected = {
            "TelemetryOnly": (
                "EXPLICIT_NON_PROOF_TELEMETRY_DIAGNOSTIC",
                "explore_v5d_diagnostic_",
            ),
            "ProviderFallback": (
                "STRICT_LOCAL_FALLBACK_RASTER_VISUAL_EVIDENCE_NOT_PROVIDER_READY_PROOF",
                "explore_v5d_diagnostic_",
            ),
            "ProviderReady": (
                "PROOF_CANDIDATE_STRICT_GLOBAL_PROVIDER_READY_FAIL_CLOSED",
                "explore_v5d_vegetation_range_",
            ),
        }
        for mode, (classification, filename_prefix) in expected.items():
            completed = subprocess.run(
                [
                    pwsh,
                    "-NoProfile",
                    "-NonInteractive",
                    "-File",
                    str(WRAPPER),
                    "-RunToken",
                    f"contract_{mode}",
                    "-ProviderEvidenceMode",
                    mode,
                    "-StaticSelfCheck",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(0, completed.returncode, completed.stderr)
            report = json.loads(completed.stdout)
            self.assertEqual("STATIC_SELF_CHECK_PASS", report["Status"])
            self.assertEqual(mode, report["ProviderEvidenceMode"])
            self.assertEqual(classification, report["EvidenceClassification"])
            self.assertEqual(900, report["ProviderTimeoutSeconds"])
            self.assertEqual(
                768,
                report["MemorySafety"]["CaptureTextureStreamingPoolMiB"],
            )
            self.assertEqual(
                "ENGINE_INI_CONSOLE_VARIABLES_PRE_MAP_LOAD_REQUIRED",
                report["MemorySafety"][
                    "CaptureTextureStreamingPoolApplication"
                ],
            )
            self.assertFalse(
                report["MemorySafety"]["RuntimeApplicationEvidenceAvailable"]
            )
            self.assertEqual(
                "SNAPSHOT_EXACT_CAPSTONE_IDENTITY_SET_AND_REQUIRE_UNCHANGED_AT_EVERY_BOUNDARY",
                report["ProtectedUE54Interaction"],
            )
            self.assertTrue(report["NativeUE55AndTRIADHelpersMustBeIdle"])
            self.assertEqual(6, report["PoseCount"])
            self.assertTrue(
                all(
                    pose["OutputFileName"].startswith(filename_prefix)
                    for pose in report["Poses"]
                )
            )
            self.assertEqual(
                3, len(report["RuntimeContract"]["BoundaryAssetPaths"])
            )
            self.assertIn(
                "contextFacadeR25=true",
                report["RuntimeContract"]["MapValidationMarkers"],
            )
            expected_identities = report["ExpectedNativeIdentities"]
            self.assertEqual(34993427, expected_identities["Map"]["Bytes"])
            self.assertEqual(
                "38114240B7A0C673B2492B74DE7AA2EB22E9E5E87349E448310FC001688D89D9",
                expected_identities["Map"]["Sha256"],
            )
            self.assertEqual(
                4585984, expected_identities["RuntimeDll"]["Bytes"]
            )
            self.assertEqual(
                "31B6EF8FE3044176AE311D6665B822713483D92C293056FDF8507E3087F0ABC4",
                expected_identities["RuntimeDll"]["Sha256"],
            )
            self.assertEqual(
                7671296, expected_identities["EditorDll"]["Bytes"]
            )
            self.assertEqual(
                "471DBFE1F3BA1CB54D346CCFE96747A30F11CE089EDC83D1BC4BE909ED4D1E34",
                expected_identities["EditorDll"]["Sha256"],
            )


if __name__ == "__main__":
    unittest.main()
