from __future__ import annotations

import hashlib
import json
import re
import shutil
import subprocess
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
UNREAL = REPO / "unreal"
WRAPPER = (
    REPO
    / "scripts"
    / "Invoke-IstanaExploreV5DContextFacadeR29NativeTransactionV1.ps1"
)

CODE_FILES = (
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/Tests/"
    "TRIADIstanaExploreV5DR29FacadeEnvironmentActorTests.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DR29FacadeEnvironmentAssetFactory.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DR29FacadeEnvironmentEditorLibrary.cpp",
)
SOURCE_ASSETS = (
    "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
    "R29ContextFacadeCoverage/Generated/"
    "SM_IPV5D_R29_ContextFacadeCoverage_Render.obj",
    "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
    "R29ContextFacadeCoverage/Generated/"
    "IstanaPublicViewV5DR29ContextFacadeCoverage.mtl",
    "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
    "R29ContextFacadeCoverage/Generated/"
    "IstanaPublicViewV5DR29ContextFacadeCoverage.manifest.json",
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


def pin_entry(section: str, relative: str) -> str:
    native = relative.replace("/", "\\")
    begin = section.index(native)
    finish = section.find("[pscustomobject]", begin + 1)
    return section[begin : finish if finish >= 0 else None]


class R29ContextFacadeNativeTransactionContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if not WRAPPER.is_file():
            raise AssertionError(f"missing R29 facade wrapper: {WRAPPER}")
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")

    def test_default_and_static_self_check_are_repository_only(self) -> None:
        for token in (
            "[switch] $Execute",
            "[switch] $StaticSelfCheck",
            "[switch] $RequireR28FacadePredecessor",
            "[Alias('RequireR28Predecessor')]",
            "[ValidateSet('R28', 'R29')]",
            "[string] $ExpectedVegetationOwner = 'R28'",
            "if ($StaticSelfCheck)",
            "if (-not $Execute)",
            "READ_ONLY_REPOSITORY_PREFLIGHT_PASS",
            "NativeTreeWritten = $false",
            "UnrealBuildOrEditorLaunched = $false",
            "$PSBoundParameters.ContainsKey($name)",
            "Live R29 facade execution requires explicit caller-supplied",
        ):
            self.assertIn(token, self.wrapper)
        self.assertLess(
            self.wrapper.index("if ($StaticSelfCheck)"),
            self.wrapper.index("[IO.Directory]::CreateDirectory($transactionRoot)"),
        )

    def test_exact_seven_code_and_three_source_assets_are_current_hash_pinned(self) -> None:
        code_section = between(
            self.wrapper, "$sourcePins = @(", "$sourceAssetPins = @("
        )
        asset_section = between(
            self.wrapper, "$sourceAssetPins = @(", "$r29ContentRelativePaths = @("
        )
        self.assertEqual(7, code_section.count("RelativePath ="))
        self.assertEqual(3, asset_section.count("RelativePath ="))
        self.assertEqual(7, code_section.count("NativeBeforePresent = $false"))
        self.assertEqual(3, asset_section.count("NativeBeforePresent = $false"))
        for relative in CODE_FILES:
            path = UNREAL / relative
            self.assertTrue(path.is_file(), relative)
            entry = pin_entry(code_section, relative)
            self.assertIn(f"Bytes = {path.stat().st_size}L", entry, relative)
            self.assertIn(sha256(path), entry, relative)
        for relative in SOURCE_ASSETS:
            path = UNREAL / relative
            self.assertTrue(path.is_file(), relative)
            entry = pin_entry(asset_section, relative)
            self.assertIn(f"Bytes = {path.stat().st_size}L", entry, relative)
            self.assertIn(sha256(path), entry, relative)

    def test_exact_twelve_content_packages_and_architecture_only_mutation(self) -> None:
        content = between(
            self.wrapper,
            "$r29ContentRelativePaths = @(",
            "function Get-FileState",
        )
        self.assertEqual(12, content.count(".uasset'"))
        self.assertEqual(11, content.count("\\Materials\\"))
        self.assertEqual(1, content.count("\\Meshes\\"))
        for forbidden in (
            "R29VegetationActor.h",
            "R29VegetationActor.cpp",
            "LandmarkVegetationActor.h",
            "LandmarkVegetationActor.cpp",
        ):
            self.assertNotIn(forbidden, content)
        for token in (
            "R28Environment = @(Get-TreeReceipt $r28ContentRoot)",
            "VegetationR29 = @(Get-TreeReceipt $vegetationR29ContentRoot)",
            "LandmarkVegetationR28 = @(Get-TreeReceipt $landmarkVegetationR28ContentRoot)",
            "TreeRealism = @(Get-TreeReceipt $treeRealismContentRoot)",
            "Assert-TreeReceipt $r28ContentRoot",
            "Assert-TreeReceipt $vegetationR29ContentRoot",
            "Assert-TreeReceipt $landmarkVegetationR28ContentRoot",
            "Assert-TreeReceipt $treeRealismContentRoot",
            "R28PublicRealmRetained = $true",
            "R28ArchitectureConcurrentRenderingAllowed = $false",
            "VegetationMutationAllowed = $false",
        ):
            self.assertIn(token, self.wrapper)

    def test_live_path_requires_exact_map_and_dll_preimages_before_first_write(self) -> None:
        for token in (
            "ExpectedMapBytes",
            "ExpectedMapSha256",
            "ExpectedVegetationOwner",
            "ExpectedRuntimeDllBytes",
            "ExpectedRuntimeDllSha256",
            "ExpectedEditorDllBytes",
            "ExpectedEditorDllSha256",
            "Assert-State $expectedMap $mapFile 'explicit facade-predecessor map'",
            "Assert-State $expectedRuntime $runtimeDll 'runtime DLL predecessor'",
            "Assert-State $expectedEditor $editorDll 'editor DLL predecessor'",
            "Assert-NativePredecessorAbsent $sourcePins",
            "Assert-NativePredecessorAbsent $sourceAssetPins",
            "$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L",
            "Assert-LaunchAdmission 'before first native write'",
        ):
            self.assertIn(token, self.wrapper)
        first_write = self.wrapper.index(
            "[IO.Directory]::CreateDirectory($transactionRoot)"
        )
        self.assertLess(
            self.wrapper.index("Assert-State $expectedMap $mapFile"), first_write
        )
        self.assertLess(
            self.wrapper.index("Assert-LaunchAdmission 'before first native write'"),
            first_write,
        )
        self.assertIn(
            "ExpectedVegetationOwner = $ExpectedVegetationOwner.ToUpperInvariant()",
            self.wrapper,
        )
        self.assertIn(
            "outputs of the R29 vegetation transaction", self.wrapper
        )

    def test_forced_build_and_four_cold_stages_are_ordered(self) -> None:
        for token in (
            "'UnrealEditor', 'Win64', 'Development'",
            "-Module=TRIADSensorFusion",
            "-Module=TRIADSensorFusionEditor",
            "-ForceHeaderGeneration",
            "-NoUBTMakefiles",
            "-NoHotReload",
            "-MaxParallelActions=1",
            "ATRIADIstanaExploreV5DR29FacadeEnvironmentActor",
            "ActivateAfterR28EnvironmentRemoval",
            "CommitR29FacadeReplacementToLoadedV5DHybridMap",
            "ValidateR29FacadeReplacementInLoadedV5DHybridMap",
        ):
            self.assertIn(token, self.wrapper)
        stages = (
            "01_ensure_r29_facade_assets",
            "02_validate_r29_facade_assets",
            "03_commit_r29_facade_replacement",
            "04_cold_validate_r29_facade_replacement",
        )
        positions = [self.wrapper.index(stage) for stage in stages]
        self.assertEqual(sorted(positions), positions)
        self.assertIn("Start-Process", self.wrapper)
        self.assertIn("-WindowStyle Hidden", self.wrapper)
        self.assertIn("Assert-NativeIdle \"before $Stage\"", self.wrapper)
        self.assertIn("Assert-NativeIdle \"after $Stage\"", self.wrapper)

    def test_rollback_is_bounded_and_no_broad_process_or_recursive_delete_exists(self) -> None:
        for token in (
            "New-FileJournal",
            "Restore-FileJournal",
            "New-TreeJournal",
            "Restore-TreeJournal",
            "Remove-IsolatedR29Content",
            "Test-ContainedPath",
            "Get-OwnedHelperIdentity",
            "Stop-OwnedHelper $handle $identity $log",
            "Protected UE5.4 CAPSTONE process set changed",
        ):
            self.assertIn(token, self.wrapper)
        for forbidden in (
            "taskkill",
            "Remove-Item -Recurse",
            "Remove-Item -LiteralPath $nativeProjectRoot",
            "Remove-Item -LiteralPath $r29ContentRoot -Recurse",
        ):
            self.assertNotIn(forbidden, self.wrapper)
        self.assertIsNone(re.search(r"(?mi)^\s*Stop-Process\b", self.wrapper))

    def test_repository_static_self_check_executes_without_native_access(self) -> None:
        pwsh = shutil.which("pwsh")
        if not pwsh:
            self.skipTest("PowerShell 7 is not available")
        result = subprocess.run(
            [
                pwsh,
                "-NoProfile",
                "-File",
                str(WRAPPER),
                "-RunToken",
                "python_static_contract",
                "-StaticSelfCheck",
            ],
            cwd=REPO,
            check=True,
            capture_output=True,
            text=True,
        )
        receipt = json.loads(result.stdout)
        self.assertEqual("STATIC_SELF_CHECK_PASS", receipt["Status"])
        self.assertEqual(7, receipt["CodeSourceCount"])
        self.assertEqual(3, receipt["SourceAssetCount"])
        self.assertEqual(12, receipt["NewContentPackageCount"])
        self.assertFalse(receipt["NativeTreeWritten"])
        self.assertFalse(receipt["UnrealBuildOrEditorLaunched"])


if __name__ == "__main__":
    unittest.main()
