from __future__ import annotations

import hashlib
import shutil
import subprocess
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
UNREAL = REPO / "unreal"
WRAPPER = (
    REPO
    / "scripts"
    / "Invoke-IstanaExploreV5DLandmarkVegetationR27NativeTransactionV1.ps1"
)

SOURCES = {
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DLandmarkVegetationActor.h": (
        11_493,
        "1B73DECAEB572876817F63B3B4EA8C6362F0695C9F2468137EDA92D148107D08",
        10_946,
        "1CBB2B107D00949BE0EB33028E2BE80C3F4A889796D0BE682412374FACC764F6",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DLandmarkVegetationActor.cpp": (
        55_817,
        "537C35F16F10C0E3BDFD4E25F41028E5FE2875134B08F0D3C92805A25BA00BEF",
        50_398,
        "FE7910E7C1B4A60BB8FE70A088A0F7C002925477842398500471883F56BC735E",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h": (
        1_619,
        "4718F81CDE54D999E6559382278C9EBBFE426936931F7B5669A84997B23F1A01",
        940,
        "026A7F9BA45C53A5AC7608D7B7BE117D4E5B847C57518F768339AA8CFABD6844",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp": (
        28_044,
        "7926155C2AEEDABD19BC1C56534AEC3DE3C83E41D9382E5219B576E2D6EDA7F1",
        6_167,
        "0110187660AE630858D774F3747923653DF4D9F40760DCFD6D7C86102CDCCEBD",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.h": (
        11_566,
        "17419FF25CD3BFECCFF082635F8ED1C9668BC4EA4D4BCA7E5D5EAEDAF7EBAD06",
        10_845,
        "9E66E7007BE43FCCD0A228342809C4281B9C5C7C9B5B145A7EDB1C0568C9309F",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp": (
        722_720,
        "B4371454C084CBE47D52E31421808C867004AE0418A66D560973C5C850AC713F",
        719_616,
        "EA0D9E665571089B5B2C7E00E44B4E31C0B0E8C0ADDA9790D21780CEFB074EF0",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.h": (
        11_654,
        "EE0A2CE3FAC359B7DDDF808775E748D7E29214437EFD438456D7333819F31661",
        10_522,
        "E0EA591E419A1D023F8BD047429A3EF64D194FBAAEB73F1B989BADB7FA18C994",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.cpp": (
        452_992,
        "D82148FCB9E9B647B1D3DFD1C5E68582052EF6175160214771CF6398E6705317",
        432_233,
        "1AB587FC5C2DC07827351E56537057BDD54C89341B44A872BFEA3D60C2A95C0C",
    ),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


class LandmarkVegetationR27NativeTransactionContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not WRAPPER.is_file():
            raise AssertionError(f"missing R27 native wrapper: {WRAPPER}")
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")

    def test_default_is_read_only_and_live_receipts_are_explicit(self):
        for token in (
            "triad.istana_explore_v5d.landmark_vegetation_r27.native_transaction.v1",
            "[switch] $Execute",
            "[switch] $StaticSelfCheck",
            "[switch] $RequireTemasekPhase2",
            "$PSBoundParameters.ContainsKey($name)",
            "Live R27 execution requires explicit caller-supplied",
            "if (-not $Execute)",
            "READ_ONLY_PREFLIGHT_PASS",
            "NativeTreeWritten = $false",
            "UnrealBuildOrEditorLaunched = $false",
            "36335002L",
            "0CDA45D7390A92885A911C1CD3404B0B59E889CE54A5879B8F1234197F73D498",
            "4672000L",
            "0B90C971AA385244FD20D2916BA87B7AE013C0C40F5B1153FB237E4CCF156191",
            "7725056L",
            "7F5D8596FD032DFAC335D7DC9AD2EC9DC7B72029144D36A40292CB8C19510F5A",
        ):
            self.assertIn(token, self.wrapper)
        self.assertLess(
            self.wrapper.index("if (-not $Execute)"),
            self.wrapper.index("[IO.Directory]::CreateDirectory($transactionRoot)"),
        )

    def test_exact_eight_repo_sources_and_native_predecessors_are_pinned(self):
        source_section = between(
            self.wrapper, "$sourcePins = @(", "$immutableContentPins = @(")
        self.assertEqual(8, source_section.count("RelativePath ="))
        self.assertEqual(8, source_section.count("NativeBeforePresent = $true"))
        for relative, (size, digest, before_size, before_digest) in SOURCES.items():
            native = relative.replace("/", "\\")
            self.assertIn(native, source_section)
            path = UNREAL / relative
            self.assertTrue(path.is_file(), relative)
            self.assertEqual(size, path.stat().st_size, relative)
            self.assertEqual(digest, sha256(path), relative)
            entry_start = source_section.index(native)
            next_entry = source_section.find("[pscustomobject]", entry_start + 1)
            entry = source_section[
                entry_start : next_entry if next_entry >= 0 else None
            ]
            self.assertIn(f"Bytes = {size}L", entry)
            self.assertIn(digest, entry)
            self.assertIn(f"NativeBeforeBytes = {before_size}L", entry)
            self.assertIn(before_digest, entry)

    def test_phase2_compiled_source_closure_and_reparse_boundaries_are_pinned(self):
        closure = between(
            self.wrapper,
            "$phase2CompiledSourcePins = @(",
            "$immutableContentPins = @(",
        )
        self.assertEqual(9, closure.count("RelativePath="))
        for token in (
            "TRIADIstanaExploreV5DTemasekShophouseActor.cpp",
            "TRIADIstanaExploreV5DTemasekShophouseProvenance.cpp",
            "TRIADIstanaExploreV5DTemasekShophouseRuntimeTests.cpp",
            "TRIADIstanaExploreV5DTemasekShophouseAssetFactory.cpp",
            "TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp",
            "Assert-Phase2CompiledSourcePins -Native",
            "Assert-NoReparseAncestor $transactionRoot $nativeProjectRoot",
            "Assert-NoReparseAncestor $journalRoot $nativeProjectRoot",
            "Assert-NoReparseAncestor $r27MaterialRoot $nativeProjectRoot",
        ):
            self.assertIn(token, self.wrapper)

    def test_four_material_packages_are_fresh_exact_and_rollback_journalled(self):
        material_section = between(
            self.wrapper,
            "$r27MaterialRelativePaths = @(",
            "$globalBuildRelativePaths = @(",
        )
        self.assertEqual(4, material_section.count(".uasset'"))
        for name in (
            "M_IPV5D_LandmarkTurf_R27_Manicured.uasset",
            "M_IPV5D_LandmarkTurf_R27_Humid.uasset",
            "M_IPV5D_LandmarkTurf_R27_Shade.uasset",
            "M_IPV5D_LandmarkTurf_R27_DryEdge.uasset",
        ):
            self.assertIn(name, material_section)
        for token in (
            "Assert-R27MaterialPredecessor",
            "Get-R27MaterialSuccessorPins",
            "Assert-R27MaterialPins",
            "$contentPaths = @($immutableContentPins",
            "+ @($r27MaterialRelativePaths",
            "Restore-FileJournal $contentJournal",
        ):
            self.assertIn(token, self.wrapper)

    def test_forced_build_and_fresh_reflection_cover_every_r27_endpoint(self):
        build = between(
            self.wrapper,
            "$forcedDirectCompileObjects = @(",
            "$mapBeforeMaterialBuild =",
        )
        for token in (
            "'-Module=TRIADSensorFusion'",
            "'-Module=TRIADSensorFusionEditor'",
            "'-ForceHeaderGeneration'",
            "'-NoUBTMakefiles'",
            "'-MaxParallelActions=1'",
            "TRIADIstanaExploreV5DLandmarkVegetationActor.gen.cpp",
            "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.gen.cpp",
            "TRIADIstanaExploreV5DHybridEditorLibrary.gen.cpp",
            "BuildOrValidateLandmarkGrassMaterialsR27",
            "ValidateLandmarkGrassMaterialsR27",
            "ApplyIstanaExploreV5DLandmarkVegetationR27VisualCorrectionToLoadedHybridMap",
            "ValidateIstanaExploreV5DLandmarkVegetationR27SuccessorMap",
            "Remove-R27DirectCompileObjectsForForcedRebuild",
            "R27 build log lacks forced direct compile action",
            "VISUAL_ASSUMPTION_BOUND_R27_LANDMARK_VEGETATION_CORRECTION",
            "SINGLE_GATE_VISIBILITY_65M_90M",
            "V5D_R23B_DERIVATIVE_MATERIAL_VALID",
        ):
            self.assertIn(token, build)

    def test_fresh_binary_markers_accept_wide_pooled_literals_and_reject_stale_objects(
        self,
    ):
        marker_gate = between(
            self.wrapper,
            "function Assert-BinaryContainsEncodedMarkers",
            "function Assert-SourcePins",
        )
        for token in (
            "[IO.File]::ReadAllBytes",
            "[Convert]::ToHexString",
            "[Text.Encoding]::UTF8.GetBytes",
            "[Text.Encoding]::Unicode.GetBytes",
            "[StringComparison]::Ordinal",
            "alignment agnostic",
            "linker pools a literal as part of a longer one",
            "UTF-8 or UTF-16LE",
        ):
            self.assertIn(token, marker_gate)

        # The failed first live attempt proved that ASCII-decoding a Windows
        # Unreal TCHAR literal inserts NULs and causes a false negative.  A
        # marker embedded at an odd byte offset in a larger pooled wide literal
        # must still be found as its exact UTF-16LE byte subsequence.
        marker = "VISUAL_ASSUMPTION_BOUND_R27_LANDMARK_VEGETATION_CORRECTION"
        pooled_wide = b"\xA5" + ("prefix::" + marker + "::suffix").encode(
            "utf-16le"
        )
        self.assertNotIn(marker.encode("ascii"), pooled_wide)
        self.assertIn(marker.encode("utf-16le"), pooled_wide)

        build = between(
            self.wrapper,
            "$forcedDirectCompileObjects = @(",
            "$mapBeforeMaterialBuild =",
        )
        for token in (
            "Remove-R27DirectCompileObjectsForForcedRebuild",
            "R27 build log lacks forced direct compile action",
            "$rebuilt.Sha256 -ceq $row.Before.Sha256",
            "byte-identical to its predecessor after forced rebuild",
        ):
            self.assertIn(token, build)
        self.assertNotIn("$runtimeDllText", self.wrapper)
        self.assertNotIn("$editorDllText.Contains($marker", self.wrapper)

    def test_seven_cold_editor_stages_are_ordered_and_strict(self):
        stages = (
            "00_cold_validate_phase2_pre_r27",
            "01_build_r27_grass_materials",
            "02_validate_reusable_assets",
            "03_apply_r27_visual_correction",
            "04_cold_validate_r27_map",
            "05_idempotent_r27_apply",
            "06_cold_validate_phase2_post_r27",
        )
        positions = [self.wrapper.index(stage) for stage in stages]
        self.assertEqual(positions, sorted(positions))
        self.assertEqual(7, self.wrapper.count("$stageResults.Add((Invoke-ColdStage `"))
        for endpoint in (
            "BuildOrValidateLandmarkGrassMaterialsR27",
            "ValidateReusableLandmarkVegetationAssets",
            "ApplyIstanaExploreV5DLandmarkVegetationR27VisualCorrectionToLoadedHybridMap",
            "ValidateIstanaExploreV5DLandmarkVegetationR27SuccessorMap",
            "IDEMPOTENT_EXPLORE_V5D_LANDMARK_VEGETATION_R27_ALREADY_VALID",
            "ValidateIstanaExploreV5DTemasekShophouseR24Assets",
            "ISTANA_EXPLORE_V5D_R24_TEMASEK_ASSETS_VALID",
        ):
            self.assertIn(endpoint, self.wrapper)
        for marker in (
            "'sourceStableVisibilityMeters=20,28'",
            "'targetStableVisibilityMeters=65,90'",
            "'visibilityGateCount=1'",
            "'componentFadeIntegrated=true'",
            "'projectionAssumption=perpendicularPinholeMaxSourceTip'",
            "'legacyTemasekBakedFoliageRemoved=true'",
            "'grassInstances=3072'",
            "'maximumGrassPerSite=2048'",
            "'visualCaptureAccepted=false'",
            "'captureRevalidationRequired=true'",
        ):
            self.assertIn(marker, self.wrapper)

    def test_map_write_requires_the_exact_journal_backup(self):
        stage = between(
            self.wrapper,
            "'03_apply_r27_visual_correction'",
            "$successorPin = Get-FileState $mapFile",
        )
        for token in (
            "ExpectedPredecessorBytes = [int64] $expectedMapPin.Bytes",
            "ExpectedPredecessorSha256 = [string] $expectedMapPin.Sha256",
            "VerifiedExternalBackupFilename = [string] $mapJournal[0].Backup",
        ):
            self.assertIn(token, stage)
        self.assertLess(
            self.wrapper.index("$mapJournal = @(New-FileJournal"),
            self.wrapper.index("'03_apply_r27_visual_correction'"),
        )

    def test_rollback_is_bounded_and_protects_capstone(self):
        for token in (
            "New-FileJournal",
            "$sourceJournal",
            "$mapJournal",
            "$contentJournal",
            "$buildJournal",
            "Restore-MapFromJournal",
            "Restore-BuildSurface",
            "Restore-FileJournal $contentJournal",
            "Restore-FileJournal $sourceJournal",
            "TRIAD_R27_Wrapper_Restore_",
            "ROLLBACK_INCOMPLETE",
            "C:\\Program Files\\Epic Games\\UE_5.4\\Engine\\Binaries\\Win64\\UnrealEditor.exe",
            "C:\\Users\\Lyz\\Desktop\\CAPSTONE\\Capstone.uproject",
            "Assert-ProtectedUnchanged",
            "StartTimeUtcTicks",
            "-WindowStyle Hidden",
            "$owners.Count -eq 1",
        ):
            self.assertIn(token, self.wrapper)
        for forbidden in (
            "Remove-Item -Recurse",
            "Remove-Item -Force -Recurse",
            "rm -rf",
            "rmdir /s",
            "git reset --hard",
        ):
            self.assertNotIn(forbidden, self.wrapper)

    def test_cold_stage_cannot_escape_before_or_after_identity_proof(self):
        lifecycle = between(
            self.wrapper,
            "function Get-ProcessRecord",
            "function Invoke-ColdStage",
        )
        for token in (
            "$cim.CreationDate",
            "function Test-Win32ProcessPresent",
            "function Test-ProcessRecordEqual",
            "function Wait-ExpectedHelperIdentity",
            "function Wait-ExactHelperExit",
            "function Wait-LaunchedHelperBoundaryReleased",
            "function Stop-LaunchedHelperBeforeIdentity",
            "PID reuse or exact helper identity drift",
            "Forced containment is permitted only after re-proving PID, creation",
            "ticks, executable, and full command line",
            "$Handle.Kill()",
            "Get-CimInstance Win32_Process -ErrorAction Stop",
            "Get-NetTCPConnection -State Listen -ErrorAction Stop",
            "exe=<identity-incomplete> cmd=<identity-incomplete>",
            "RC listener remained after exact helper graceful exit",
            "RC listener remained after exact helper containment",
        ):
            self.assertIn(token, lifecycle)
        self.assertNotIn("-ErrorAction SilentlyContinue", lifecycle)
        self.assertNotIn("Stop-Process -Id", lifecycle)
        self.assertNotIn("$Handle.HasExited", lifecycle)
        self.assertNotIn("$Handle.WaitForExit", lifecycle)

        stage = between(
            self.wrapper,
            "function Invoke-ColdStage",
            "function Get-BuildSurfacePaths",
        )
        for token in (
            "$script:activeHelperHandle = $handle",
            "$script:activeHelperIdentity = $null",
            "$script:activeHelperLaunchStartTimeUtcTicks",
            "Wait-ExpectedHelperIdentity",
            "Could not recover exact helper identity during cleanup",
            "Stop-LaunchedHelperBeforeIdentity",
            "launch-handle fallback containment error",
            "Exact launched helper/RC boundary remains after stage cleanup",
            "Stage $Stage cleanup boundary failed",
        ):
            self.assertIn(token, stage)
        self.assertLess(
            stage.index("$script:activeHelperHandle = $handle"),
            stage.index("$identity = Wait-ExpectedHelperIdentity"),
        )
        self.assertLess(
            stage.index("try { Stop-OwnedHelper $identity $handle }"),
            stage.index("if (Test-Win32ProcessPresent $launchProcessId)"),
        )

        rollback = self.wrapper[self.wrapper.index("finally {", self.wrapper.index("$commitPath")) :]
        for token in (
            "if ($null -ne $script:activeHelperHandle)",
            "Wait-ExpectedHelperIdentity",
            "Stop-OwnedHelper $script:activeHelperIdentity",
            "Stop-LaunchedHelperBeforeIdentity",
            "$ownedStopError = $null",
            "launch-handle fallback error",
            "Test-Win32ProcessPresent",
            "Launched helper remains after outer containment",
            "RC listener remains after outer helper containment",
        ):
            self.assertIn(token, rollback)
        self.assertLess(
            rollback.index("if ($null -ne $script:activeHelperHandle)"),
            rollback.index("Assert-NativeIdle 'rollback entry'"),
        )
        self.assertLess(
            rollback.index("Assert-NativeIdle 'rollback entry'"),
            rollback.index("Restore-BuildSurface $buildJournal"),
        )

    def test_powershell_parser_and_repo_only_static_selfcheck_pass(self):
        pwsh = shutil.which("pwsh")
        if pwsh is None:
            self.skipTest("PowerShell 7 is not available")
        parser = subprocess.run(
            [
                pwsh,
                "-NoProfile",
                "-NonInteractive",
                "-Command",
                "$t=$null;$e=$null;"
                f"[void][System.Management.Automation.Language.Parser]::ParseFile('{WRAPPER}',[ref]$t,[ref]$e);"
                "if($e.Count){$e|ForEach-Object{$_.Message};exit 1}",
            ],
            cwd=REPO,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(0, parser.returncode, parser.stdout + parser.stderr)
        check = subprocess.run(
            [
                pwsh,
                "-NoProfile",
                "-NonInteractive",
                "-File",
                str(WRAPPER),
                "-RunToken",
                "contract_r27",
                "-StaticSelfCheck",
            ],
            cwd=REPO,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(0, check.returncode, check.stdout + check.stderr)
        self.assertIn('"Status": "STATIC_SELF_CHECK_PASS"', check.stdout)
        self.assertIn('"NativeTreeReadOrWritten": false', check.stdout)
        self.assertIn('"VisualCaptureAccepted": false', check.stdout)


if __name__ == "__main__":
    unittest.main()
