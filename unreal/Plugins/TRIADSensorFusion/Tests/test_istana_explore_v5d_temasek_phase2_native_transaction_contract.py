import hashlib
import importlib.util
import json
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


PLUGIN = Path(__file__).resolve().parents[1]
UNREAL = PLUGIN.parents[1]
REPOSITORY = UNREAL.parent
WRAPPER = (
    REPOSITORY
    / "scripts/Invoke-IstanaExploreV5DTemasekPhase2NativeTransactionV1.ps1"
)
R24 = (
    UNREAL
    / "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R24TemasekShophouse"
)
BUILDER = R24 / "build_temasek_shophouse_r24.py"
EDITOR_LIBRARY = (
    UNREAL
    / "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private"
    / "TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp"
)


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def load_builder():
    spec = importlib.util.spec_from_file_location("temasek_phase2_tx_builder", BUILDER)
    if spec is None or spec.loader is None:
        raise AssertionError(f"cannot load {BUILDER}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def powershell() -> str:
    executable = shutil.which("pwsh")
    if executable is None:
        raise unittest.SkipTest("PowerShell 7 is not installed")
    return executable


def section(source: str, start: str, end: str) -> str:
    begin = source.index(start)
    finish = source.index(end, begin)
    return source[begin:finish]


class TemasekPhase2NativeTransactionContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = text(WRAPPER)
        cls.builder = load_builder()

    def test_wrapper_has_no_powershell_parse_errors(self) -> None:
        wrapper_ps = str(WRAPPER).replace("'", "''")
        command = (
            "$tokens=$null;$errors=$null;"
            "[void][System.Management.Automation.Language.Parser]::ParseFile("
            f"'{wrapper_ps}',"
            "[ref]$tokens,[ref]$errors);"
            "if($errors.Count){$errors|%{$_.Message};exit 1};"
            "'PARSE_PASS'"
        )
        result = subprocess.run(
            [powershell(), "-NoProfile", "-Command", command],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("PARSE_PASS", result.stdout)

    def test_static_self_check_is_repo_only_and_reports_exact_scope(self) -> None:
        result = subprocess.run(
            [
                powershell(),
                "-NoProfile",
                "-File",
                str(WRAPPER),
                "-RunToken",
                "contract_static",
                "-StaticSelfCheck",
            ],
            check=False,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual("STATIC_SELF_CHECK_PASS", report["Status"])
        self.assertFalse(report["NativeTreeReadOrWritten"])
        self.assertFalse(report["UnrealBuildOrEditorLaunched"])
        self.assertEqual("R25_DOCUMENTATION_ONLY", report["EmbeddedDefaultPredecessor"])
        self.assertTrue(report["LiveExecuteRequiresLandmarkVegetationR26"])
        self.assertEqual(14, report["PromotionCount"])
        self.assertEqual(20, report["LegacyAssetCount"])
        self.assertEqual(16, report["SuccessorAssetCount"])
        rollback = report["BuildRollbackContract"]
        self.assertEqual(7, rollback["BinaryProductCount"])
        self.assertEqual(2, rollback["ExplicitCallerPinnedBinaryCount"])
        self.assertEqual(5, rollback["DynamicallyCapturedBinaryProductCount"])
        self.assertEqual(165, rollback["ScopedBuildAndUhtCount"])
        self.assertEqual(30, rollback["R26SuccessorBuildAndUhtAdditionCount"])
        self.assertEqual(172, rollback["TotalBuildProductCount"])
        self.assertFalse(report["WholeNativeTreeRollbackClaimed"])
        self.assertFalse(report["RecursiveDeleteUsed"])
        self.assertEqual(
            {
                "SourceVertices": 9536,
                "Triangles": 15760,
                "Components": 828,
                "Materials": 15,
                "BakedFoliageRenderComponents": 0,
                "TreeAnchorsHandedToR26": 3,
            },
            report["Phase2Census"],
        )

    def test_live_execution_requires_explicit_successor_pins_and_r26(self) -> None:
        self.assertIn("$PSBoundParameters.ContainsKey($name)", self.source)
        for parameter in (
            "ExpectedMapBytes",
            "ExpectedMapSha256",
            "ExpectedRuntimeDllBytes",
            "ExpectedRuntimeDllSha256",
            "ExpectedEditorDllBytes",
            "ExpectedEditorDllSha256",
        ):
            self.assertIn(f"'{parameter}'", self.source)
        for marker in (
            "Live Phase-2 execution requires -RequireLandmarkVegetationR26",
            "R25 defaults are documentation/read-only convenience only",
            "CallerSuppliedPredecessorPins",
            "R25DefaultsAreDocumentationOnly",
            "Assert-LandmarkR26NativeSourceReady",
        ):
            self.assertIn(marker, self.source)

    def test_embedded_defaults_are_exact_r25_receipts_not_live_authority(self) -> None:
        pins = (
            ("34993427L", "38114240B7A0C673B2492B74DE7AA2EB22E9E5E87349E448310FC001688D89D9"),
            ("4585984L", "31B6EF8FE3044176AE311D6665B822713483D92C293056FDF8507E3087F0ABC4"),
            ("7671296L", "471DBFE1F3BA1CB54D346CCFE96747A30F11CE089EDC83D1BC4BE909ED4D1E34"),
        )
        for byte_count, sha256 in pins:
            self.assertIn(byte_count, self.source)
            self.assertIn(sha256, self.source)
        self.assertIn("AllSixExplicitForExecute", self.source)

    def test_fresh_builder_outputs_match_all_seven_staged_pins(self) -> None:
        with tempfile.TemporaryDirectory(prefix="triad_phase2_tx_contract_") as temp:
            output = Path(temp)
            manifest = self.builder.build(output)
            self.assertEqual(9536, manifest["counts"]["vertices"])
            self.assertEqual(15760, manifest["counts"]["triangles"])
            self.assertEqual(828, manifest["counts"]["components"])
            self.assertEqual(15, manifest["counts"]["materials"])
            self.assertEqual(
                0, manifest["foliageLayout"]["bakedRenderComponentCount"]
            )
            self.assertEqual(3, len(manifest["foliageLayout"]["treeAnchors"]))
            files = sorted(path for path in output.iterdir() if path.is_file())
            self.assertEqual(7, len(files))
            for path in files:
                digest = hashlib.sha256(path.read_bytes()).hexdigest().upper()
                self.assertIn(path.name, self.source)
                self.assertIn(f"Bytes = {path.stat().st_size}L", self.source)
                self.assertIn(digest, self.source)

    def test_promotion_is_exactly_seven_repo_plus_seven_staged_files(self) -> None:
        repo_block = section(
            self.source, "$repositorySourcePins = @(", "$generatedPins = @(")
        generated_block = section(
            self.source, "$generatedPins = @(", "$generatorAdmissionPins = @(")
        self.assertEqual(7, repo_block.count("RelativePath = '"))
        self.assertEqual(7, generated_block.count("Name = '"))
        for excluded in (
            "TRIADIstanaExploreV5DLandmarkVegetationActor.h",
            "TRIADIstanaExploreV5DLandmarkVegetationActor.cpp",
            "TRIADIstanaExploreV5DHybridEditorLibrary.h",
            "TRIADIstanaExploreV5DHybridEditorLibrary.cpp",
        ):
            self.assertNotIn(excluded, repo_block)
        self.assertIn("Origin = 'STAGED_DETERMINISTIC'", self.source)
        self.assertIn("Invoke-Phase2StagingBuild", self.source)
        self.assertIn("deterministicRebuild", self.source)

    def test_exact_20_to_16_asset_replacement_and_removed_foliage(self) -> None:
        legacy = section(
            self.source,
            "$legacyTemasekAssetPins = @(",
            "$phase2PrimaryAssetRelativePaths = @(",
        )
        successor = section(
            self.source,
            "$phase2PrimaryAssetRelativePaths = @(",
            "$removedLegacyFoliageAssetRelativePaths = @(",
        )
        removed = section(
            self.source,
            "$removedLegacyFoliageAssetRelativePaths = @(",
            "$immutableContentPins = @(",
        )
        self.assertEqual(20, legacy.count("RelativePath='"))
        self.assertEqual(16, successor.count(".uasset'"))
        self.assertEqual(4, removed.count(".uasset'"))
        for material in ("Bark", "LeafDeep", "LeafLight", "PollinatorBloom"):
            self.assertIn(material, removed)
            self.assertNotIn(material, successor)
        self.assertIn("Remove-Item -LiteralPath $row.Path -Force", self.source)
        self.assertNotRegex(self.source, r"(?i)Remove-Item\s+[^\r\n]*-Recurse")
        self.assertNotRegex(self.source, r"(?im)^\s*(rm|rmdir|del)\b")

    def test_backups_precede_mutation_and_rollback_is_reverse_scoped(self) -> None:
        main = self.source.index("$backups = New-VerifiedTransactionBackups")
        validate_r26 = self.source.index(
            "-Stage '00_validate_r26_predecessor'", main
        )
        stage = self.source.index("$staging = Invoke-Phase2StagingBuild", main)
        publish = self.source.index("Publish-NativeSources", stage)
        build = self.source.index("Invoke-Phase2Build", publish)
        evacuate = self.source.index("Remove-ExactLegacyTemasekAssetPredecessor", build)
        import_assets = self.source.index("-Stage '01_import_phase2_assets'", evacuate)
        self.assertLess(main, validate_r26)
        self.assertLess(validate_r26, stage)
        self.assertLess(stage, publish)
        self.assertLess(publish, build)
        self.assertLess(build, evacuate)
        self.assertLess(evacuate, import_assets)
        catch = self.source.index("catch {\n    $transactionError")
        content = self.source.index("Restore-Phase2Content", catch)
        products = self.source.index("Restore-BuildProducts", content)
        sources = self.source.index("Restore-NativeSources", products)
        self.assertLess(content, products)
        self.assertLess(products, sources)
        for marker in (
            "BackupsVerifiedBeforeFunctionalNativeMutation",
            "[IO.File]::Replace",
            "Copy-NewPinnedFile",
            "ONE_HUNDRED_SEVENTY_TWO_BUILD_PRODUCT_STATES",
            "FOURTEEN_SOURCE_DESTINATIONS",
        ):
            self.assertIn(marker, self.source)

    def test_file_replace_always_has_a_nonempty_displaced_audit_path(self) -> None:
        calls = re.findall(r"\[IO\.File\]::Replace\(([^\r\n]+)\)", self.source)
        self.assertEqual(
            ["$Source, $Destination, $displaced, $true"], calls
        )
        self.assertNotRegex(
            self.source,
            r"\[IO\.File\]::Replace\([^\r\n]*(?:\$null|''|\"\")",
        )
        for marker in (
            "function Replace-ExistingPinnedFile",
            "did not produce a non-empty displaced audit path",
            "ExpectedDestination $current",
            "Test-EquivalentFileState -Left $current -Right $Expected",
            "[string]::IsNullOrWhiteSpace([string] $Backup.Path)",
            "replace_displaced",
        ):
            self.assertIn(marker, self.source)

    def test_nonempty_file_replace_backup_path_works_on_host(self) -> None:
        with tempfile.TemporaryDirectory(prefix="triad_replace_contract_") as temp:
            root = Path(temp)
            source = root / "transaction_stage/successor.bin"
            destination = root / "native_source/destination.bin"
            displaced = root / "transaction_audit/displaced.bin"
            source.parent.mkdir()
            destination.parent.mkdir()
            displaced.parent.mkdir()
            source.write_bytes(b"phase2-successor")
            destination.write_bytes(b"r26-predecessor")
            source_ps = str(source).replace("'", "''")
            destination_ps = str(destination).replace("'", "''")
            displaced_ps = str(displaced).replace("'", "''")
            command = (
                f"[IO.File]::Replace('{source_ps}','{destination_ps}',"
                f"'{displaced_ps}',$true);"
                f"if([IO.File]::Exists('{source_ps}')){{exit 11}};"
                f"if([Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes('{destination_ps}'))"
                " -cne 'phase2-successor'){exit 12};"
                f"if([Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes('{displaced_ps}'))"
                " -cne 'r26-predecessor'){exit 13};"
                "'NONEMPTY_REPLACE_PASS'"
            )
            result = subprocess.run(
                [
                    powershell(),
                    "-NoProfile",
                    "-Command",
                    command,
                ],
                check=False,
                capture_output=True,
                text=True,
                timeout=30,
            )
            self.assertEqual(0, result.returncode, result.stdout + result.stderr)
            self.assertIn("NONEMPTY_REPLACE_PASS", result.stdout)

    def test_r26_is_cold_validated_before_and_twice_after_replacement(self) -> None:
        endpoint = "ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap"
        self.assertGreaterEqual(self.source.count(endpoint), 7)
        before = self.source.index("-Stage '00_validate_r26_predecessor'")
        evacuate = self.source.index("$evacuation = Remove-ExactLegacy")
        after = self.source.index("-Stage '04_cold_validate_r26_successor'")
        final = self.source.index("-Stage '07_final_cold_validate_r26'")
        self.assertLess(before, evacuate)
        self.assertLess(evacuate, after)
        self.assertLess(after, final)
        self.assertIn("landmarkVegetationR26=true", self.source)
        self.assertIn("LandmarkVegetationR26MapIntegrated = $true", self.source)
        self.assertIn("LandmarkVegetationR26OwnedByThisTransaction = $false", self.source)

    def test_capstone_and_remote_control_identity_are_exact(self) -> None:
        for marker in (
            "C:\\Program Files\\Epic Games\\UE_5.4\\Engine\\Binaries\\Win64\\UnrealEditor.exe",
            "C:\\Users\\Lyz\\Desktop\\CAPSTONE\\Capstone.uproject",
            "Get-ProtectedUE54Identity",
            "Assert-ProtectedUE54Unchanged",
            "CreationUtcTicks",
            "Start-Process -FilePath $editor",
            "-WindowStyle Hidden",
            "Get-RemoteControlListeners",
            "listeners.Count -ne 1",
            "Test-ExpectedHelperLaunchIdentity",
            "Stop-ExactHelperForContainment",
        ):
            self.assertIn(marker, self.source)

    def test_build_is_module_scoped_forced_uht_and_fully_journalled(self) -> None:
        for marker in (
            "-Module=TRIADSensorFusion",
            "-Module=TRIADSensorFusionEditor",
            "-NoUBTMakefiles",
            "-ForceHeaderGeneration",
            "-MaxParallelActions=1",
            "compileArtifactSpecs.Count -ne 26",
            "r26UhtOutputRelativePaths.Count -ne 5",
            "buildAuxiliaryRelativePaths.Count -ne 165",
            "relativePaths.Count -ne 172",
            "R26_SUCCESSOR_BOUNDARY",
            "TRIADIstanaExploreV5DLandmarkVegetationActor.gen.cpp",
            "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.gen.cpp",
            "TRIADSensorFusion.init.gen.cpp",
            "Remove-ExactPhase2CompileObjectsForForcedRebuild",
            "Forced Phase-2 compile-object count drifted",
            "Compile [x64] $leaf",
            "ForcedCompileObjectCount = $forcedCompileObjects.Count",
            "TRIAD_TEMASEK_PHASE2_FRESH_ROSTER_EXPECTED=16",
            "sixteen non-null assets",
            "twenty non-null assets",
            "StaleTwentyAssetInvariantAbsent = $true",
            "EXACT_9_PHASE2_DEPENDENCY_CLOSURE_COVERED",
            "FRESH_PHASE2_UHT_AND_DLL_REFLECTION_VALID",
        ):
            self.assertIn(marker, self.source)

        r26_sources = section(
            self.source,
            "$landmarkR26NativeSourceRelativePaths = @(",
            "$compileArtifactSuffixes = @(",
        )
        self.assertEqual(6, r26_sources.count("'Plugins\\TRIADSensorFusion"))
        self.assertIn("AllSixSourceFilesPresent", self.source)

    def test_editor_import_uses_one_explicit_phase2_16_asset_contract(self) -> None:
        source = text(EDITOR_LIBRARY)
        self.assertIn("constexpr int32 Phase2ExpectedFreshAssetCount = 16;", source)
        self.assertIn("TRIAD_TEMASEK_PHASE2_FRESH_ROSTER_EXPECTED=16", source)
        self.assertEqual(2, source.count("!= Phase2ExpectedFreshAssetCount"))
        self.assertNotRegex(source, r"(?:FreshAssets|Packages)\.Num\(\)\s*!=\s*20")
        self.assertNotIn("twenty non-null assets", source)
        editor_bytes = EDITOR_LIBRARY.read_bytes()
        editor_hash = hashlib.sha256(editor_bytes).hexdigest().upper()
        self.assertIn(f"Bytes = {len(editor_bytes)}L", self.source)
        self.assertIn(editor_hash, self.source)

    def test_forced_compile_roster_is_exactly_the_nine_journalled_objects(self) -> None:
        function = section(
            self.source,
            "function Remove-ExactPhase2CompileObjectsForForcedRebuild",
            "function Get-LandmarkR26NativeReadiness",
        )
        self.assertIn("$expectedDependencyClosureKeys", function)
        self.assertIn("$matches.Count -ne 1", function)
        self.assertIn("Remove-Item -LiteralPath $row.Path -Force", function)
        self.assertIn("$removed.Count -ne 9", function)
        self.assertNotRegex(function, r"(?i)Remove-Item\s+[^\r\n]*-Recurse")

    def test_r25_facade_v5c_and_v2_assets_are_immutable(self) -> None:
        immutable = section(
            self.source,
            "$immutableContentPins = @(",
            "$nonAuthoritativeEphemeralStateExclusions = @(",
        )
        self.assertEqual(11, immutable.count("Role='"))
        for marker in (
            "M_IPV5D_ContextFacadeR25_Master.uasset",
            "MI_IPV5D_ContextFacadeR25_OfficialWall.uasset",
            "MI_IPV5D_ContextFacadeR25_FallbackRoof.uasset",
            "M_IPV5C_ContextMassing_Master.uasset",
            "SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.uasset",
            "Assert-ImmutableContentPins",
        ):
            self.assertIn(marker, self.source)

    def test_commit_is_honest_about_authority_and_remaining_ownership(self) -> None:
        for marker in (
            "BakedFoliageRenderComponents = 0",
            "FoliageTreeAnchors = 3",
            "CollisionNavigationSensorRfAuthorityChanged = $false",
            "WholeNativeTreeRollbackClaimed = $false",
            "ForcedContainmentUsed = $false",
            "RecursiveDeleteUsed = $false",
            "Phase 2 deliberately contains zero baked foliage",
            "does not promote or place that separately committed R26 actor",
        ):
            self.assertIn(marker, self.source)


if __name__ == "__main__":
    unittest.main()
