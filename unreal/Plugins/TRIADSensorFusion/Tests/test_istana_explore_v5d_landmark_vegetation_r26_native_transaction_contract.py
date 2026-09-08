from __future__ import annotations

import hashlib
import shutil
import subprocess
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
WRAPPER = (
    REPO
    / "scripts"
    / "Invoke-IstanaExploreV5DLandmarkVegetationR26NativeTransactionV1.ps1"
)
UNREAL = REPO / "unreal"

SOURCES = {
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DLandmarkVegetationActor.h": (
        10_946,
        "1CBB2B107D00949BE0EB33028E2BE80C3F4A889796D0BE682412374FACC764F6",
        False,
        0,
        "ABSENT",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DLandmarkVegetationActor.cpp": (
        50_398,
        "FE7910E7C1B4A60BB8FE70A088A0F7C002925477842398500471883F56BC735E",
        False,
        0,
        "ABSENT",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h": (
        940,
        "026A7F9BA45C53A5AC7608D7B7BE117D4E5B847C57518F768339AA8CFABD6844",
        False,
        0,
        "ABSENT",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp": (
        6_167,
        "0110187660AE630858D774F3747923653DF4D9F40760DCFD6D7C86102CDCCEBD",
        False,
        0,
        "ABSENT",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.h": (
        10_522,
        "E0EA591E419A1D023F8BD047429A3EF64D194FBAAEB73F1B989BADB7FA18C994",
        True,
        9_553,
        "762F0EB8436844912D203D6E14E962AB3BA7331F300DE19EA323096D45C11AEF",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.cpp": (
        432_233,
        "1AB587FC5C2DC07827351E56537057BDD54C89341B44A872BFEA3D60C2A95C0C",
        True,
        403_489,
        "C28D6D9408C98469CF21C423A0D568ACBC2F00BDA61BF1AAB60F476285311E26",
    ),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


class LandmarkVegetationR26NativeTransactionContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not WRAPPER.is_file():
            raise AssertionError(f"missing R26 transaction wrapper: {WRAPPER}")
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")

    def test_default_is_read_only_and_execute_is_explicit(self):
        for token in (
            "triad.istana_explore_v5d.landmark_vegetation_r26.native_transaction.v1",
            "[switch] $Execute",
            "[switch] $StaticSelfCheck",
            "if (-not $Execute)",
            "READ_ONLY_PREFLIGHT_PASS",
            "NativeTreeWritten = $false",
            "UnrealBuildOrEditorLaunched = $false",
        ):
            self.assertIn(token, self.wrapper)
        self.assertLess(
            self.wrapper.index("if (-not $Execute)"),
            self.wrapper.index("[IO.Directory]::CreateDirectory($transactionRoot)"),
        )

    def test_exact_six_source_pins_preserve_the_historical_r26_snapshot(self):
        section = between(
            self.wrapper, "$sourcePins = @(", "$immutableContentPins = @(")
        self.assertEqual(6, section.count("RelativePath ="))
        self.assertEqual(6, section.count("NativeBeforePresent ="))
        for relative, (size, digest, before, before_size, before_digest) in SOURCES.items():
            native = relative.replace("/", "\\")
            self.assertIn(native, section)
            path = UNREAL / relative
            self.assertTrue(path.is_file(), relative)
            entry_start = section.index(native)
            next_entry = section.find("[pscustomobject]", entry_start + 1)
            entry = section[entry_start : next_entry if next_entry >= 0 else None]
            self.assertIn(f"Bytes = {size}L", entry)
            self.assertIn(digest, entry)
            self.assertIn(
                f"NativeBeforePresent = ${str(before).lower()}", entry
            )
            self.assertIn(f"NativeBeforeBytes = {before_size}L", entry)
            self.assertIn(before_digest, entry)
        # R27 is a deliberate successor. The historical R26 wrapper must keep
        # its old immutable promotion pins and therefore refuse this checkout.
        self.assertTrue(
            any(
                (UNREAL / relative).stat().st_size != size
                or sha256(UNREAL / relative) != digest
                for relative, (size, digest, *_rest) in SOURCES.items()
            )
        )
        self.assertIn("$sourcePins.Count -ne 6", self.wrapper)
        self.assertIn("exactly two present/four absent", self.wrapper)

    def test_exact_r25_map_dll_project_and_content_are_pinned(self):
        for token in (
            "Bytes = 34993427L",
            "38114240B7A0C673B2492B74DE7AA2EB22E9E5E87349E448310FC001688D89D9",
            "Bytes = 4585984L",
            "31B6EF8FE3044176AE311D6665B822713483D92C293056FDF8507E3087F0ABC4",
            "Bytes = 7671296L",
            "471DBFE1F3BA1CB54D346CCFE96747A30F11CE089EDC83D1BC4BE909ED4D1E34",
            "Bytes = 1298L",
            "42114E7A55BAC2974ECAB36B16E19EAAE013B7D8B3E2354CE93E15933A0AAFC3",
            "A2062A8A90CB5FFA4D0B6B1117E775230B39AF58CB57F3C26C792DD4861429EE",
            "37C7305FC07DD076902A243410F7CB094356F8CBD333461E29EF3AC47694AC03",
            "2C783BA3D045FBC37432D117C372C1B6654B94ECDC3E9C8C051263E2492036DA",
            "98DE68DFB79083DF8D690C0222BAED926F414302E840A8C88950FD73DCE729D8",
            "CD27BC83E1876E53B485F055D5D1F2E48999EE8CEAC733D7AB55F06CC6E81C9B",
            "E8F78D776EB235F1F25304678CCAD7F64DA125376BA04126DE2CE3BABCA64A18",
        ):
            self.assertIn(token, self.wrapper)
        self.assertEqual(11, between(
            self.wrapper,
            "$immutableContentPins = @(",
            "$globalBuildRelativePaths = @(",
        ).count("RelativePath ="))

    def test_build_is_two_module_forced_uht_and_reflection_gated(self):
        build = between(
            self.wrapper,
            "$buildArguments = @(",
            "$mapBeforeAssetValidation = Get-FileState",
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
            "[Text.Encoding]::ASCII.GetString",
        ):
            self.assertIn(token, build)
        for endpoint in (
            "ApplyIstanaExploreV5DLandmarkVegetationR26ToLoadedHybridMap",
            "ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap",
            "ValidateReusableLandmarkVegetationAssets",
            "ConfigureLandmarkVegetationActor",
        ):
            self.assertIn(endpoint, build)

    def test_four_fresh_rendering_capable_editor_stages_are_ordered(self):
        ordered = (
            "01_validate_reusable_assets",
            "02_apply_map",
            "03_cold_validate_map",
            "04_idempotent_apply",
        )
        positions = [self.wrapper.index(name) for name in ordered]
        self.assertEqual(positions, sorted(positions))
        stages = self.wrapper[self.wrapper.index(ordered[0]) :]
        self.assertEqual(
            4,
            self.wrapper.count("$stageResults.Add((Invoke-ColdStage `"),
        )
        for function, prefix in (
            (
                "ValidateReusableLandmarkVegetationAssets",
                "ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_ASSETS_VALID",
            ),
            (
                "ApplyIstanaExploreV5DLandmarkVegetationR26ToLoadedHybridMap",
                "EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_PASS:",
            ),
            (
                "ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap",
                "ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R26_MAP_VALID:",
            ),
            (
                "ApplyIstanaExploreV5DLandmarkVegetationR26ToLoadedHybridMap",
                "IDEMPOTENT_EXPLORE_V5D_LANDMARK_VEGETATION_R26_ALREADY_VALID:",
            ),
        ):
            self.assertIn(function, stages)
            self.assertIn(prefix, stages)
        self.assertIn("Start-Process -FilePath $editor", self.wrapper)
        self.assertIn("-WindowStyle Hidden", self.wrapper)
        self.assertIn("-RemoteControlHttpServer", self.wrapper)
        self.assertIn("ValidateIstanaExploreRemoteControlProject", self.wrapper)

    def test_distance_readability_and_negative_authority_markers_are_required(self):
        for marker in (
            "'macDonaldGrass=1536'",
            "'temasekGrass=1536'",
            "'distanceReadableGrass=true'",
            "'grassCullCm=6500,9000'",
            "'evidenceRangeMeters=72.8'",
            "'wpoDisableCm=2400'",
            "'grassInstances=3072'",
            "'renderOnly=true'",
            "'sensorAuthority=false'",
            "'rfAuthority=false'",
        ):
            self.assertIn(marker, self.wrapper)
        self.assertIn("CollisionNavigationSensorRfAuthority = $false", self.wrapper)

    def test_rollback_journals_bounded_map_build_content_and_source_state(self):
        for token in (
            "New-FileJournal",
            "$sourceJournal",
            "$mapJournal",
            "$contentJournal",
            "$buildJournal",
            "Get-BuildSurfacePaths",
            "$pluginBinaryRoot",
            "$pluginIntermediateRoot",
            "Restore-MapFromJournal",
            "Restore-BuildSurface",
            "Restore-FileJournal $contentJournal",
            "Restore-FileJournal $sourceJournal",
            "TRIAD_R26_Wrapper_Restore_",
            "Assert-NativeIdle 'rollback entry'",
            "ROLLBACK_INCOMPLETE",
        ):
            self.assertIn(token, self.wrapper)
        rollback = self.wrapper[self.wrapper.index("if (-not $committed)") :]
        self.assertLess(
            rollback.index("Restore-MapFromJournal"),
            rollback.index("Restore-BuildSurface"),
        )
        self.assertLess(
            rollback.index("Restore-BuildSurface"),
            rollback.index("Restore-FileJournal $sourceJournal"),
        )
        for forbidden in ("Remove-Item -Recurse", "rm -rf", "rmdir /s"):
            self.assertNotIn(forbidden, self.wrapper)

    def test_capstone_and_rc_ownership_are_exact(self):
        for token in (
            "C:\\Program Files\\Epic Games\\UE_5.4\\Engine\\Binaries\\Win64\\UnrealEditor.exe",
            "C:\\Users\\Lyz\\Desktop\\CAPSTONE\\Capstone.uproject",
            "Get-ProtectedSnapshot",
            "Assert-ProtectedUnchanged",
            "StartTimeUtcTicks",
            "Get-NetTCPConnection -State Listen -LocalPort 30010",
            "$owners.Count -eq 1",
            "$owners[0] -eq [uint32] $identity.ProcessId",
            "Assert-HelperIdentity",
            "Stop-OwnedHelper",
        ):
            self.assertIn(token, self.wrapper)

    def test_powershell_parser_and_static_selfcheck_pass(self):
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
                "contract_static_r26",
                "-StaticSelfCheck",
            ],
            cwd=REPO,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertNotEqual(0, check.returncode, check.stdout + check.stderr)
        self.assertIn(
            "repository source pin mismatch", check.stdout + check.stderr
        )


if __name__ == "__main__":
    unittest.main()
