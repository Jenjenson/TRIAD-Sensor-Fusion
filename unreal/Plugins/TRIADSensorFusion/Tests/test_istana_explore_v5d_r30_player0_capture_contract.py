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
HEADER = PUBLIC / "TRIADIstanaExploreV5DR30Player0CaptureLibrary.h"
CPP = PRIVATE / "TRIADIstanaExploreV5DR30Player0CaptureLibrary.cpp"
WRAPPER = REPO / "scripts" / "Capture-IstanaExploreV5DR30Player0Evidence.ps1"
VISUAL_REVIEW_CONTRACT = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Surroundings"
    / "R30FacadeLookdev"
    / "r30_player0_visual_review.contract.json"
)
README = VISUAL_REVIEW_CONTRACT.with_name("README.md")


class IstanaExploreV5DR30Player0CaptureContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.cpp = CPP.read_text(encoding="utf-8")
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")
        cls.visual_review_contract = json.loads(
            VISUAL_REVIEW_CONTRACT.read_text(encoding="utf-8")
        )
        cls.readme = README.read_text(encoding="utf-8")

    def test_runtime_blueprint_library_exposes_only_bounded_capture_seam(self):
        self.assertIn("UBlueprintFunctionLibrary", self.header)
        for endpoint in (
            "GetIstanaExploreV5DR30Player0CaptureState",
            "SetIstanaExploreV5DR30Player0CapturePose",
            "CaptureIstanaExploreV5DR30Player0FallbackView",
            "FinishIstanaExploreV5DR30Player0CaptureRun",
        ):
            self.assertIn(endpoint, self.header)
            self.assertIn(endpoint, self.cpp)
        for forbidden in ("GEditor", "UnrealEd", "LevelEditor", "R31"):
            self.assertNotIn(forbidden, self.header)
            self.assertNotIn(forbidden, self.cpp)

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
            "contextPolicyShell=R25Inherited",
            "r29VegetationOwner=1",
            "r29TerrainOwner=1",
            "treeRealismOwner=1",
            "providerReadyForProof=%s",
            "providerFallbackVisualQa=%s",
            "providerReadyProofClaimed=false",
            "localFallbackHidden=false",
            "r30Visible=true",
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

    def test_r30_r29_r25_and_cesium_runtime_validation_is_explicit(self):
        for marker in (
            "ValidateR30FacadeLookdev",
            "ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene",
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
                2046,
                "B120B71629CB59BEC40C2E05DC18601F864CDFDA7DF45CCBAAB0B9700A920ADD",
            ),
            (
                CPP,
                26848,
                "0953E5D29CCD64FC7691FFA64A0FC68CC7377AEF10BD1C641A7C5FFAF6779878",
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
            "$identityDeadline = [DateTime]::UtcNow.AddSeconds(5)",
            "Start-Sleep -Milliseconds 100",
            "[string] $candidateIdentity.ExecutablePath",
            "[string] $candidateIdentity.CommandLine",
            "Restore-TreeJournal @($buildJournal)",
            "Restore-FileJournal @($sourceJournal)",
            "ExactOwnedProcessContainmentOnly=$true",
        ):
            self.assertIn(marker, self.wrapper)

    def test_committed_r30_receipt_is_hash_bound_before_capture(self):
        for marker in (
            "ExpectedR30CommitReceiptSha256",
            "triad.istana_explore_v5d.context_facade_lookdev_r30.native_transaction.v1",
            "'COMMITTED'",
            "R30ContentPackageCount",
            "PredecessorVegetationOwner",
            "ContextPolicyShellValidatedBeforeAndAfter",
            "VisualCaptureAccepted",
            "CaptureRevalidationRequired",
            "current R30 successor map",
            "current cooked-runtime hotfix runtime DLL",
            "current R30 editor DLL",
            "V5DCookedRuntimeUE55V1",
            "94A94303468D6045137CC74241117CC8C8047C9D9388E5672955B138745453EE",
            "function Assert-CookedRuntimeHotfixCommitReceipt",
            "CookedRuntimeHotfixAdmission",
        ):
            self.assertIn(marker, self.wrapper)
        self.assertIn("R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31", self.wrapper)
        self.assertIn("R31DependencyAllowed=$false", self.wrapper)

    def test_user_authorized_ram_policy_and_exact_process_identity_are_preserved(self):
        for marker in (
            "2147483648L # 2 GiB emergency commit reserve",
            "9223372036854775807L # no fixed process-RAM ceiling",
            "FixedPrivateMemoryCeilingDisabled=$true",
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
            "standalone R30 Development Game executable",
            "-ForceHeaderGeneration",
            "-NoUBTMakefiles",
            "-run=Cook",
            "-TargetPlatform=Windows",
            "-OutputDir=$cookOutputPattern",
            "Cooked\\[Platform]",
            "Cooked\\Windows",
            "-Sandbox=$sandboxRoot",
            "-TRIADR30CaptureRun=$RunToken",
            "-TRIADR30CaptureOutputRoot=$evidenceRoot",
        ):
            self.assertIn(marker, self.wrapper)
        self.assertNotIn("-iterate", self.wrapper.lower())

    def test_cold_cook_shader_compile_policy_is_narrow_and_executable(self):
        self.assertIn("function Assert-NoFatalRuntimeLogText", self.wrapper)
        self.assertIn("AllowExpectedColdCookShaderCompile", self.wrapper)
        self.assertIn("LogMaterial: Display:", self.wrapper)
        self.assertIn("[0-9A-Fa-f]{40}", self.wrapper)
        self.assertIn("(?: Is special engine material\\.)?", self.wrapper)

        cook_call = re.search(
            r"Assert-NoFatalRuntimeLog\s+`\s*\n"
            r"\s*\$cookRuntimeLog \$fullCookLabel\s+`\s*\n"
            r"\s*-AllowExpectedColdCookShaderCompile",
            self.wrapper,
        )
        self.assertIsNotNone(cook_call)
        game_call = re.search(
            r"Assert-NoFatalRuntimeLog\s+\$gameRuntimeLog\s+"
            r"'standalone R30 capture Game'",
            self.wrapper,
        )
        self.assertIsNotNone(game_call)
        self.assertNotIn(
            "AllowExpectedColdCookShaderCompile", game_call.group(0)
        )

        for marker in (
            "Fatal error:",
            "LogWindows: Error:",
            "Failed to compile Material",
            "ShaderCompileWorker terminated unexpectedly",
            "Falling back to directly compiling",
            "Missing shader map",
            "Failed to load package",
            "Can't find file for asset",
            "R30 facade provider mirror failed closed",
            "ISTANA_EXPLORE_V5_BEGIN_PLAY_REAPPLY_FAILED",
            "ISTANA_EXPLORE_V5D_HYBRID_INVALID",
            "ISTANA_EXPLORE_V5D_FOUNTAIN_INVALID",
            "ISTANA_EXPLORE_V5D_R24_MACDONALD_ASSET_FAIL_CLOSED",
            "ISTANA_EXPLORE_V5D_R24_MACDONALD_INVALID",
            "ISTANA_EXPLORE_V5D_R24_TEMASEK_ASSET_FAIL_CLOSED",
            "ISTANA_EXPLORE_V5D_R24_TEMASEK_INVALID",
            "ISTANA_EXPLORE_V5D_TREE_REALISM_INVALID",
            "ISTANA_EXPLORE_V4_INVALID",
            "V5D ground/vegetation runtime failed closed",
            "V5D terrain/provider/turf/edge presentation failed closed",
            "V5D tree realism runtime failed closed",
            "V5D tree material response failed closed",
            "R30_CAPTURE_STATE_INVALID",
        ):
            self.assertIn(marker, self.wrapper)

        for required_marker in (
            "ISTANA_EXPLORE_V5D_FOUNTAIN_VALID",
            "ISTANA_EXPLORE_V5D_TREE_REALISM_VALID",
            "ISTANA_EXPLORE_V5D_GROUND_VEGETATION_VALID",
        ):
            self.assertIn(required_marker, self.wrapper)
        self.assertIn(
            "function Assert-RequiredPlayer0RuntimeContractMarkersText",
            self.wrapper,
        )
        self.assertIn(
            "function Assert-RequiredPlayer0RuntimeContractMarkers",
            self.wrapper,
        )
        startup_gate = self.wrapper.index("$gameStartupRuntimeContract =")
        pose_loop = self.wrapper.index("foreach ($pose in $poses)", startup_gate)
        self.assertLess(startup_gate, pose_loop)
        self.assertIn("GameStartupRuntimeContract=$gameStartupRuntimeContract", self.wrapper)
        self.assertIn("GameStartupHybridContract=$gameStartupHybridContract", self.wrapper)
        self.assertIn("GameFinalRuntimeContract=$gameFinalRuntimeContract", self.wrapper)

    def test_hybrid_and_landmark_runtime_proofs_follow_actual_runtime_paths(self):
        for marker in (
            "function Assert-HybridCaptureStateResponse",
            "ValidateHybridContextExecutedTransitively=$true",
            "contextPolicyOwner=1",
            "contextPolicyShell=R25Inherited",
            "function Invoke-RcPropertyRead",
            "function Assert-HybridRuntimeProofReceipt",
            "function Assert-LandmarkBooleanPropertyReceipt",
            "function Assert-LandmarkRuntimeContractsReceipt",
            "function Assert-RuntimeEvidenceAcceptancePolicyContract",
            "http://127.0.0.1:30010/remote/object/property",
            "access='READ_ACCESS'",
            "function Wait-LandmarkRuntimeContracts",
            "bRuntimeProviderTelemetryBindingValid",
            "bRuntimeAssetContractFailClosed",
            "bDedicatedOverlayVisible",
            "TRIADIstanaExploreV5DR24MacDonaldHouse",
            "TRIADIstanaExploreV5DR24TemasekShophouse",
            "MutationAllowed=$false",
            "LandmarkRuntimeContractsPreCapture=$landmarkRuntimeContractsPreCapture",
            "LandmarkRuntimeContractsPostCapture=$landmarkRuntimeContractsPostCapture",
            "HybridRuntimeProofRevalidated=$true",
            "LandmarkRuntimePropertyReceiptsRevalidated=$true",
        ):
            self.assertIn(marker, self.wrapper)

        startup = self.wrapper.index("$gameStartupState = Wait-RuntimeCaptureReady")
        preflight = self.wrapper.index(
            "$landmarkRuntimeContractsPreCapture = Wait-LandmarkRuntimeContracts",
            startup,
        )
        pose_loop = self.wrapper.index("foreach ($pose in $poses)", preflight)
        postflight = self.wrapper.index(
            "$landmarkRuntimeContractsPostCapture = Wait-LandmarkRuntimeContracts",
            pose_loop,
        )
        finish = self.wrapper.index("FinishIstanaExploreV5DR30Player0CaptureRun", postflight)
        self.assertLess(startup, preflight)
        self.assertLess(preflight, pose_loop)
        self.assertLess(pose_loop, postflight)
        self.assertLess(postflight, finish)

        pending_start = self.wrapper.index(
            "function Assert-R30PendingCaptureReceipt"
        )
        pending_end = self.wrapper.index(
            "function Get-R30AcceptanceNativeBoundary", pending_start
        )
        pending_validator = self.wrapper[pending_start:pending_end]
        for marker in (
            "'GameStartupHybridContract'",
            "Assert-HybridRuntimeProofReceipt",
            "'LandmarkRuntimeContractsPreCapture'",
            "'LandmarkRuntimeContractsPostCapture'",
            "Assert-LandmarkRuntimeContractsReceipt",
            "'PRE_CAPTURE'",
            "'POST_FIFTH_CAPTURE'",
            "Assert-JsonIdentity",
            "GameStartupHybridContract=$hybridRuntimeProof",
            "LandmarkRuntimeContractsPreCapture=$landmarkRuntimePre",
            "LandmarkRuntimeContractsPostCapture=$landmarkRuntimePost",
        ):
            self.assertIn(marker, pending_validator)
        hybrid_validation = pending_validator.index(
            "Assert-HybridRuntimeProofReceipt"
        )
        capture_replay = pending_validator.index("$captures = @(")
        self.assertLess(hybrid_validation, capture_replay)

    def test_runtime_logs_use_bounded_shared_reads_and_ready_wait_fails_fast(self):
        for marker in (
            "function Read-RuntimeLogTextShared",
            "[IO.FileShare]::ReadWrite",
            "[IO.FileShare]::Delete",
            "[ValidateRange(1, 400)] [int] $RetryCount = 40",
            "[ValidateRange(0, 1000)] [int] $RetryDelayMilliseconds = 100",
            "[IO.StreamReader]::new",
            "catch [IO.IOException]",
            "catch [System.UnauthorizedAccessException]",
        ):
            self.assertIn(marker, self.wrapper)

        liveness_start = self.wrapper.index(
            "function Assert-LiveRuntimeCaptureCanContinue"
        )
        wait_start = self.wrapper.index("function Wait-RuntimeCaptureReady")
        stable_pose_start = self.wrapper.index("function Wait-StableFallbackPose")
        liveness = self.wrapper[liveness_start:wait_start]
        wait = self.wrapper[wait_start:stable_pose_start]
        for marker in (
            "$owned.Handle.HasExited",
            "Assert-ExactOwnedProcessIdentity",
            "Assert-WatchdogHealthy",
            "Read-RuntimeLogTextShared",
            "Assert-NoFatalRuntimeLogText",
        ):
            self.assertIn(marker, liveness)
        for marker in (
            "param([string] $RuntimeLogPath, [int] $TimeoutSeconds)",
            "Assert-LiveRuntimeCaptureCanContinue",
            "Assert-NoFatalRuntimeLogText",
            "R30 runtime capture-state response",
        ):
            self.assertIn(marker, wait)
        self.assertIn(
            "Wait-RuntimeCaptureReady `\n        $gameRuntimeLog $CaptureTimeoutSeconds",
            self.wrapper,
        )
        self.assertNotIn("-Text ([IO.File]::ReadAllText($Path))", self.wrapper)
        self.assertNotIn(
            "$gameLogText = [IO.File]::ReadAllText($gameRuntimeLog)", self.wrapper
        )

    def test_child_only_ddc_and_paired_external_shader_worker_are_fail_closed(self):
        for marker in (
            "'UE-LocalDataCachePath'=$isolatedDdcRoot",
            "-Environment $validatedChildEnvironment",
            "ParentProcessEnvironmentMutated=$false",
            "function Assert-IsolatedDdcRuntimeLogText",
            "function Assert-IsolatedDdcRuntimeLog",
            "function Assert-DedicatedShaderWorkerRuntimeLogText",
            "function Assert-DedicatedShaderWorkerRuntimeLog",
            "function Assert-DedicatedShaderWorkerLogPolicyContract",
            "LogDerivedDataCache: (?:Display: )?",
            "Local: Using data cache path (?<CachePath>.+): Writable",
            "=== FShaderJobCache stats",
            "-AllowNoShaderJobs",
            "-AllowCleanBoundedShaderBatchWithoutJobCacheStats",
            "CleanBoundedShaderBatchWithoutStatsExceptionNarrow=$true",
            "CookRuntimeIsolation=$cookRuntimeIsolation",
            "CookShaderWorkerMode=$cookShaderWorkerMode",
            "CookTreeDerivativeMeshDdcHits=$cookTreeDerivativeMeshDdcHits",
            "GameRuntimeIsolation=$gameRuntimeIsolation",
        ):
            self.assertIn(marker, self.wrapper)
        self.assertNotIn("-LocalDataCachePath=", self.wrapper)
        self.assertNotRegex(
            self.wrapper,
            r"\$\{?env:UE-LocalDataCachePath\}?\s*=",
        )
        self.assertNotIn("SetEnvironmentVariable(", self.wrapper)

        cook_start = self.wrapper.index("$cookArguments = @(")
        cook_end = self.wrapper.index("$cookProcessReceipt", cook_start)
        cook_arguments = self.wrapper[cook_start:cook_end]
        for required in (
            "'-DDC=InstalledNoZenLocalFallback'",
            "'-ini:Engine:[DevOptions.Shaders]:bAllowCompilingThroughWorkers=true'",
            "'-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=8'",
            "'-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreadsDuringGame=8'",
            "'-ini:Editor:[CookSettings]:PackagesPerGC=32'",
            "'-LogCmds=LogStaticMesh Verbose'",
        ):
            self.assertIn(required, cook_arguments)
        self.assertNotIn("'-NoShaderWorker'", cook_arguments)
        self.assertNotIn("r.ShaderCompiler.JobCache=0", cook_arguments)
        cook_call_end = self.wrapper.index(
            "Assert-NoFatalRuntimeLog", cook_end
        )
        cook_call = self.wrapper[cook_end:cook_call_end]
        for required in (
            "'-DDC=InstalledNoZenLocalFallback'",
            "'-ini:Engine:[DevOptions.Shaders]:bAllowCompilingThroughWorkers=true'",
            "'-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=8'",
            "'-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreadsDuringGame=8'",
            "'-ini:Editor:[CookSettings]:PackagesPerGC=32'",
            "-ChildEnvironment $isolatedDdcChildEnvironment",
        ):
            self.assertIn(required, cook_call)
        self.assertNotIn("'-NoShaderWorker'", cook_call)
        self.assertNotIn("r.ShaderCompiler.JobCache=0", cook_call)
        cook_worker_start = self.wrapper.index("$cookShaderWorkerMode =")
        cook_worker_end = self.wrapper.index(
            "$cookTreeDerivativeMeshDdcHits", cook_worker_start
        )
        self.assertIn(
            "-AllowNoShaderJobs",
            self.wrapper[cook_worker_start:cook_worker_end],
        )

    def test_large_tree_ddc_prewarm_is_per_mesh_guarded_and_cache_proved(self):
        for marker in (
            "$treeDerivativeMeshPrewarmPackages = @(",
            "SM_IPV5D_Tree_ColumnarNarrow_NearLOD0'",
            "SM_IPV5D_Tree_HighForkRounded_NearLOD0'",
            "function Assert-TreeDerivativeMeshPrewarmBuildLogText",
            "function Assert-TreeDerivativeMeshPrewarmResultLog",
            "function Assert-TreeDerivativeMeshDdcHitLogText",
            "function Assert-TreeDerivativeMeshDdcPrewarmPolicyContract",
            "-Package=$prewarmPackage",
            "'-cooksinglepackage'",
            "PER_MESH_ISOLATED_PROCESS_LARGEST_FIRST",
            "DdcScope='CURRENT_EVIDENCE_RUN_ONLY'",
            "VisualAssetsModified=$false",
            "AccelerationSeedKind='HASH_PINNED_SOURCE_STATIC_MESH_PAIR'",
            "746286AA5339611E695ECEC63F44A4E12038F9C70529FB7FBF82F1F95979BEF3",
            "CBC819BB03B671A17BB0CF6443F1D98472C8B844B4D981C665349F85856D19A1",
            "TreeDdcPrewarmCookedRemoved=$true",
            "TreeDdcProbeCookedRemoved=$true",
            "ForbiddenRuntimeLogPattern $forbiddenPrewarmBuildPattern",
            "The seeded largest TreeRealism mesh was not an exact cache-only DDC hit.",
            "'TreeDdcPrewarmCooked'",
            "'TreeDdcProbeCooked'",
        ):
            self.assertIn(marker, self.wrapper)

        prewarm_start = self.wrapper.index("$prewarmArguments = @(")
        prewarm_end = self.wrapper.index(
            "$prewarmProcessReceipt", prewarm_start
        )
        prewarm_arguments = self.wrapper[prewarm_start:prewarm_end]
        self.assertIn("'-LogCmds=LogStaticMesh Verbose'", prewarm_arguments)
        self.assertIn("'-DDC=InstalledNoZenLocalFallback'", prewarm_arguments)
        self.assertNotIn("-COOKDIR=", prewarm_arguments)
        self.assertNotIn("-NoShaderWorker", prewarm_arguments)
        prewarm_worker_start = self.wrapper.index(
            "$treePrewarmShaderWorkerReceipts.Add"
        )
        prewarm_worker_end = self.wrapper.index(
            "$prewarmResult", prewarm_worker_start
        )
        self.assertIn(
            "-AllowCleanBoundedShaderBatchWithoutJobCacheStats:"
            "($prewarmIndex -gt 0)",
            self.wrapper[prewarm_worker_start:prewarm_worker_end],
        )

        probe_start = self.wrapper.index("$treeDdcProbeArguments = @(")
        probe_end = self.wrapper.index(
            "$treeDdcProbeProcessReceipt", probe_start
        )
        probe_arguments = self.wrapper[probe_start:probe_end]
        self.assertIn("-Package=$treeDdcProbePackageArgument", probe_arguments)
        self.assertIn("'-cooksinglepackage'", probe_arguments)
        self.assertIn("'-LogCmds=LogStaticMesh Verbose'", probe_arguments)
        self.assertNotIn("-COOKDIR=", probe_arguments)

        game_start = self.wrapper.index("$gameArguments = @(")
        game_end = self.wrapper.index("$gameStartParameters", game_start)
        game_arguments = self.wrapper[game_start:game_end]
        self.assertIn("'-DDC=InstalledNoZenLocalFallback'", game_arguments)
        self.assertNotIn("ShaderCompiler.JobCache", game_arguments)
        game_parameters_end = self.wrapper.index(
            "$gameOwned = Start-GuardedOwnedProcess", game_end
        )
        game_parameters = self.wrapper[game_end:game_parameters_end]
        self.assertIn(
            "ChildEnvironment=$isolatedDdcChildEnvironment", game_parameters
        )
        self.assertIn(
            "'-DDC=InstalledNoZenLocalFallback'", game_parameters
        )

    def test_process_log_release_and_idempotent_bounded_restore_are_explicit(self):
        for marker in (
            "function Test-ExactFileState",
            "function Test-ExactFileContentState",
            "if (Test-ExactFileState $row.Before $current)",
            "if (-not (Test-ExactFileContentState $row.Before $current))",
            "$restoreRetryCount = 8",
            "$restoreRetryDelayMilliseconds = 250",
            "Capture journal restore failed after $restoreRetryCount bounded attempts",
            "Remove-JournalAdditionBounded",
            "function Wait-RedirectedLogRelease",
            "$Owned.Handle.WaitForExit()",
            "$Owned.Handle.Dispose()",
            "[IO.FileShare]::None",
            "ProcessHandlesDisposedBeforeLogInspection=$true",
            "IdempotentBoundedJournalRestore=$true",
        ):
            self.assertIn(marker, self.wrapper)
        close_start = self.wrapper.index("function Close-GuardedOwnedProcess")
        close_end = self.wrapper.index(
            "function Wait-NativeMutationQuiescence", close_start
        )
        close = self.wrapper[close_start:close_end]
        self.assertLess(
            close.index("$Owned.Handle.Dispose()"),
            close.index("Get-FileState $Owned.StandardOutputLog"),
        )

    def test_cooked_and_png_actual_file_closure_is_hash_checked(self):
        for marker in (
            "Istana_PublicView_Explore_v5d_hybrid.umap",
            "cooked R30 successor map export",
            "Cooked R30 namespace is not the exact twelve-package roster",
            "requiredCookedDependencyRoots",
            "ContextFacadeR25",
            "AssetRegistry.bin",
            "*.ushaderbytecode",
            "DecodedBgraSha256",
            "Present=$true",
            "Assert-CookedClosureUnchanged",
            "WidthPixels=2560",
            "HeightPixels=1440",
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

    def test_execute_is_pending_only_and_cannot_self_accept_visual_quality(self):
        pending_start = self.wrapper.index(
            "$pendingCapture = [pscustomobject] [ordered] @{"
        )
        pending_end = self.wrapper.index("$captureCompleted = $true", pending_start)
        pending = self.wrapper[pending_start:pending_end]
        for marker in (
            "Status='PENDING_VISUAL_REVIEW'",
            "MechanicalCaptureValidationPassed=$true",
            "ExplicitHumanReviewAcceptance=$false",
            "HumanVisualReviewAttested=$false",
            "ConfirmedFiveImagesReviewed=$false",
            "AutomaticVisualAcceptanceAllowed=$false",
            "VisualReviewRequired=$true",
            "VisualReviewAccepted=$false",
            "ProviderFallbackVisualQaAccepted=$false",
            "TreeMaterialResponseV3Reviewed=$false",
            "R31AdmissionAuthorized=$false",
            "pending-visual-review.json",
        ):
            self.assertIn(marker, pending)
        self.assertNotIn("Status='COMMITTED'", pending)
        self.assertNotIn("(Join-Path $evidenceRoot 'commit.json')", pending)

    def test_explicit_acceptance_replays_five_images_and_native_boundary(self):
        start = self.wrapper.index("function Assert-R30PendingCaptureReceipt")
        end = self.wrapper.index("$staticReceipt = Assert-StaticContract", start)
        acceptance = self.wrapper[start:end]
        for marker in (
            "-AcceptVisualReview",
            "ConfirmFiveImagesReviewed",
            "ExpectedPendingCaptureReceiptSha256",
            "Assert-R30PendingCaptureReceipt",
            "Get-PngReceipt $expectedImagePath",
            "Assert-CaptureSourceTreeSnapshot",
            "Get-R30AcceptanceNativeBoundary",
            "Assert-CaptureImmutableState",
            "GroundHeaderCaptureBinding",
            "GroundSourceCaptureBinding",
            "NativePluginSourceTreeCaptureBinding",
            "Status='COMMITTED'",
            "MechanicalCaptureValidationPassed=$true",
            "ExplicitHumanReviewAcceptance=$true",
            "HumanVisualReviewAttested=$true",
            "ConfirmedFiveImagesReviewed=$true",
            "AutomaticVisualAcceptanceAllowed=$false",
            "VisualReviewRequired=$false",
            "VisualReviewAccepted=$true",
            "ProviderFallbackVisualQaAccepted=$true",
            "TreeMaterialResponseV3Reviewed=$true",
            "R31AdmissionAuthorized=$true",
            "MapModifiedByCapture=$false",
            "SimulationCollisionNavigationSensorRfModified=$false",
            "NativeStateMutatedByAcceptance=$false",
            "UnrealLaunchedByAcceptance=$false",
            "FivePngsRedecodedAndRehashed=$true",
            "FullNativePluginSourceTreeRevalidated=$true",
            "TreeMaterialResponseV3SourceAndContentClosureRevalidated=$true",
            "TreeResponseMaterials13AndRuntimeMids26Revalidated=$true",
            "GameStartupHybridContract=",
            "LandmarkRuntimeContractsPreCapture=",
            "LandmarkRuntimeContractsPostCapture=",
            "HybridTransitiveRuntimeProofRevalidated=$true",
            "LandmarkRuntimePropertyReceiptsRevalidated=$true",
            "LandmarkRuntimePropertyReceiptsCarriedForward=$true",
        ):
            parameter_markers = {
                "-AcceptVisualReview",
                "ConfirmFiveImagesReviewed",
                "ExpectedPendingCaptureReceiptSha256",
            }
            self.assertIn(marker, self.wrapper if marker in parameter_markers else acceptance)
        for forbidden_native_mutation_or_launch in (
            "Start-GuardedOwnedProcess",
            "Invoke-GuardedCommand",
            "$unrealEditorCmd",
            "Copy-Item",
            "SetLastWriteTimeUtc",
            "Restore-FileJournal",
            "Restore-TreeJournal",
        ):
            self.assertNotIn(forbidden_native_mutation_or_launch, acceptance)

        accepted_start = self.wrapper.index(
            "$acceptedReceipt = [pscustomobject] [ordered] @{"
        )
        accepted_end = self.wrapper.index(
            "$candidatePath = Join-Path", accepted_start
        )
        accepted_receipt = self.wrapper[accepted_start:accepted_end]
        for carried_runtime_proof in (
            "GameStartupHybridContract=",
            "$pendingAdmission.GameStartupHybridContract",
            "LandmarkRuntimeContractsPreCapture=",
            "$pendingAdmission.LandmarkRuntimeContractsPreCapture",
            "LandmarkRuntimeContractsPostCapture=",
            "$pendingAdmission.LandmarkRuntimeContractsPostCapture",
            "HybridTransitiveRuntimeProofRevalidated=$true",
            "LandmarkRuntimePropertyReceiptsRevalidated=$true",
            "LandmarkRuntimePropertyReceiptsCarriedForward=$true",
        ):
            self.assertIn(carried_runtime_proof, accepted_receipt)

    def test_machine_readable_two_phase_visual_review_contract_is_exact(self):
        contract = self.visual_review_contract
        self.assertEqual(
            "triad.istana_explore_v5d.r30_player0_visual_review.v1",
            contract["schema"],
        )
        self.assertEqual(
            "R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31",
            contract["strictNativeOrder"],
        )
        self.assertEqual("PENDING_VISUAL_REVIEW", contract["executeStatus"])
        self.assertEqual("COMMITTED", contract["acceptedStatus"])
        self.assertFalse(contract["automaticVisualAcceptanceAllowed"])
        self.assertEqual(
            [
                "075m",
                "020m",
                "008m",
                "002m",
                "surroundings_oblique_macdonald",
            ],
            contract["exactOrderedPoseIds"],
        )
        self.assertFalse(contract["pendingTruth"]["humanVisualReviewAttested"])
        self.assertFalse(contract["pendingTruth"]["treeMaterialResponseV3Reviewed"])
        self.assertFalse(contract["pendingTruth"]["r31AdmissionAuthorized"])
        self.assertTrue(contract["acceptedTruth"]["humanVisualReviewAttested"])
        self.assertTrue(contract["acceptedTruth"]["treeMaterialResponseV3Reviewed"])
        self.assertTrue(contract["acceptedTruth"]["r31AdmissionAuthorized"])
        self.assertTrue(
            contract["acceptanceRevalidation"][
                "treeMaterialResponseV3SourceAndContentClosure"
            ]
        )
        self.assertTrue(
            contract["acceptanceRevalidation"][
                "treeResponseMaterials13AndRuntimeMids26"
            ]
        )
        self.assertEqual(contract["treeVisualReview"]["primaryPoseIds"], ["008m", "002m"])
        self.assertFalse(
            contract["treeVisualReview"]["sourceOrCpuAuditMayAuthorizeAcceptance"]
        )
        self.assertIn("-ConfirmFiveImagesReviewed", self.readme)

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
        self.assertFalse(report["NativeProjectAccessed"])
        self.assertFalse(report["NativeTreeWritten"])
        self.assertFalse(report["UnrealLaunched"])
        self.assertFalse(report["ProviderReadyProofClaimed"])
        self.assertFalse(report["R31DependencyAllowed"])
        self.assertTrue(report["MemoryWatchdogTypeCompiled"])
        self.assertTrue(report["BinaryMarkerScannerTypeCompiled"])
        self.assertEqual(
            report["HybridRuntimeProof"],
            "SUCCESSFUL_RC_CAPTURE_STATE_TRANSITIVE_VALIDATE_HYBRID",
        )
        runtime_evidence_policy = report["RuntimeEvidenceAcceptancePolicy"]
        self.assertTrue(runtime_evidence_policy["PositiveHybridProofAccepted"])
        self.assertTrue(runtime_evidence_policy["PositiveLandmarkProofAccepted"])
        self.assertEqual(runtime_evidence_policy["TamperedFormsRejected"], 2)
        self.assertTrue(
            runtime_evidence_policy["PendingReceiptRevalidationRequired"]
        )
        self.assertTrue(
            runtime_evidence_policy["CommittedReceiptCarryForwardRequired"]
        )
        landmark_audit = report["LandmarkRuntimePropertyAudit"]
        self.assertEqual(landmark_audit["ExactActorCount"], 2)
        self.assertTrue(landmark_audit["ReadOnly"])
        self.assertTrue(landmark_audit["PreCaptureRequired"])
        self.assertTrue(landmark_audit["PostFifthCaptureRequired"])
        self.assertTrue(landmark_audit["PendingReceiptRevalidationRequired"])
        self.assertTrue(landmark_audit["CommittedReceiptCarryForwardRequired"])
        log_policy = report["RuntimeLogPolicy"]
        self.assertEqual(log_policy["ExpectedColdCookFormsAccepted"], 2)
        self.assertEqual(log_policy["ForbiddenFormsRejected"], 26)
        self.assertTrue(log_policy["RuntimeCachedShaderMapCompileStillRejected"])
        self.assertTrue(log_policy["MissingShaderMapAlwaysRejected"])
        self.assertTrue(log_policy["OtherFatalMarkersStillRejected"])
        self.assertTrue(log_policy["MissingRequiredPackagedRuntimeMarkerRejected"])
        required_runtime = log_policy["RequiredPackagedRuntimeMarkers"]
        self.assertEqual(required_runtime["RequiredMarkerCount"], 3)
        self.assertEqual(
            required_runtime["RequiredMarkers"],
            [
                "ISTANA_EXPLORE_V5D_FOUNTAIN_VALID",
                "ISTANA_EXPLORE_V5D_TREE_REALISM_VALID",
                "ISTANA_EXPLORE_V5D_GROUND_VEGETATION_VALID",
            ],
        )
        self.assertTrue(required_runtime["AllRequiredMarkersPresent"])
        isolation_policy = report["RuntimeIsolationLogPolicy"]
        self.assertEqual(isolation_policy["PositiveFormsAccepted"], 2)
        self.assertEqual(isolation_policy["ForbiddenFormsRejected"], 3)
        self.assertTrue(isolation_policy["ChildOnlyDdcSelectionRequired"])
        self.assertTrue(
            isolation_policy[
                "ShaderJobCachePolicyDelegatedToDedicatedWorkerContract"
            ]
        )
        worker_policy = report["DedicatedShaderWorkerLogPolicy"]
        self.assertEqual(worker_policy["PositiveFormsAccepted"], 3)
        self.assertEqual(worker_policy["ForbiddenFormsRejected"], 12)
        self.assertTrue(worker_policy["InProcessNoShaderWorkerForbidden"])
        self.assertTrue(
            worker_policy["ShaderWorkerCrashAndDirectFallbackForbidden"]
        )
        self.assertTrue(worker_policy["ShaderJobCacheDisableOverrideForbidden"])
        self.assertTrue(worker_policy["EnabledShaderJobCacheRuntimeStatsRequired"])
        self.assertTrue(worker_policy["CacheOnlyNoShaderJobsExceptionNarrow"])
        self.assertTrue(
            worker_policy[
                "CleanBoundedShaderBatchWithoutStatsExceptionNarrow"
            ]
        )
        self.assertTrue(worker_policy["CompilingThroughWorkersExplicitlyEnabled"])
        self.assertEqual(worker_policy["ExactLocalShaderWorkerCount"], 8)
        prewarm_policy = report["TreeDerivativeMeshDdcPrewarmPolicy"]
        self.assertTrue(prewarm_policy["IsolatedPerMeshProcessRequired"])
        self.assertTrue(prewarm_policy["LargestMeshFirst"])
        self.assertEqual(prewarm_policy["ExactPackageCount"], 5)
        self.assertEqual(prewarm_policy["HashPinnedAccelerationSeedFileCount"], 2)
        self.assertTrue(prewarm_policy["BoundedPopulationProofRequired"])
        self.assertTrue(prewarm_policy["SeededLargestMeshColdBuildForbidden"])
        self.assertEqual(
            prewarm_policy["LiveBuildMissSentinelPollMilliseconds"], 100
        )
        self.assertTrue(prewarm_policy["CacheOnlyProbeRequired"])
        self.assertTrue(prewarm_policy["FullCookCacheOnlyReuseRequired"])
        self.assertEqual(prewarm_policy["NegativeFormsRejected"], 2)
        self.assertFalse(prewarm_policy["VisualAssetMutationAllowed"])
        self.assertTrue(report["ProcessHandlesDisposedBeforeLogInspection"])
        self.assertTrue(report["IdempotentBoundedJournalRestore"])
        self.assertTrue(report["TwoPhaseVisualReviewRequired"])
        self.assertFalse(report["AutomaticVisualAcceptanceAllowed"])
        self.assertEqual(report["PendingCaptureStatus"], "PENDING_VISUAL_REVIEW")
        self.assertEqual(report["AcceptedCaptureStatus"], "COMMITTED")
        self.assertTrue(report["NativePluginSourceTreeRequired"])
        self.assertTrue(report["GroundHeaderAndSourceRequired"])
        self.assertEqual(report["TreeResponseMaterialPackageCount"], 13)
        self.assertEqual(report["TreeDerivativeMeshPackageCount"], 5)
        self.assertEqual(report["TreeRuntimeResponseMidCount"], 26)
        self.assertTrue(report["TreeHumanVisualReviewRequired"])


if __name__ == "__main__":
    unittest.main()
