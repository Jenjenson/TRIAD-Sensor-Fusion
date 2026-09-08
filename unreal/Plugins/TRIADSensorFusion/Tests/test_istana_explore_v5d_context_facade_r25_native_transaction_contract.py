from __future__ import annotations

import hashlib
import re
import shutil
import subprocess
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
WRAPPER = (
    REPO
    / "scripts/Invoke-IstanaExploreV5DContextFacadeR25NativeTransactionV1.ps1"
)

SOURCES = {
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DContextFacadeR25AssetFactory.h": (
        709,
        "A3D6114BA9450E9A32B0CAE51713B9A56C31AA08EAAD665A0132F6C9CF227CF0",
        False,
        0,
        "ABSENT",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DContextFacadeR25AssetFactory.cpp": (
        41_282,
        "82376BEFB4261283466226A019C751E35B0FC59DA42FE5D9DEA0842A54ADD298",
        False,
        0,
        "ABSENT",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h": (
        2_490,
        "160EDC5EDBE67AF6D48B3FA34C7C9998B5F693EE4186C9D97E06F11A7B42C9C0",
        True,
        1_722,
        "E752E5767ABBBA900E2957231D8E1FE3CD4E03AAE8180FF08EE85346DE06243F",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp": (
        16_747,
        "289C996F67ADD3FBAD26073083BDC83F8D1CB2117E316A94E3D5F5E6686EC06E",
        True,
        12_313,
        "2E52D294F856C00B17ACAE0299447CBC11BA4E423F3D37A25CCC7E2C8300923D",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DContextPolicyActor.h": (
        9_003,
        "07858E83E7E73E103922A0FB3EA52FA76F30A9D96F1A402DA6631F75E9A73774",
        True,
        8_694,
        "88A4000BA015308E50C909B333E8337649409179165138E092CA8DE63393C981",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DContextPolicyActor.cpp": (
        93_879,
        "6E2C5697AE3949CFD009D4762A334E71A09DFAEA60F53823C670D19137C082EA",
        True,
        86_305,
        "75A753072A8914D505314A6AF395AFAD89AA919A0E1C8B6253EEF67B072338D1",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.h": (
        9_553,
        "762F0EB8436844912D203D6E14E962AB3BA7331F300DE19EA323096D45C11AEF",
        True,
        9_021,
        "950ACEFDAF9EB1B3B21C535DB7E5482C217F1D72C23FF9995CAC9DA1758DFE47",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.cpp": (
        403_489,
        "C28D6D9408C98469CF21C423A0D568ACBC2F00BDA61BF1AAB60F476285311E26",
        True,
        377_184,
        "9490D09F9FC453777F5187C2440F144F01227B5D7E9F079B22D710B403B4F75D",
    ),
    "Plugins/TRIADSensorFusion/Tests/"
    "test_istana_explore_v5d_context_facade_r25_contract.py": (
        13_305,
        "F04B2A4529F5D257CB6E4CAF01B25759F76CE4FC54B71365A6C1F8F02FBE1D2F",
        False,
        0,
        "ABSENT",
    ),
}

BUILD_PRODUCTS = {
    "UnrealEditor-TRIADSensorFusion.dll": (
        4_577_280,
        "C7A80FD955ED831A7148068444079E54D7D9847759D7DAAC24272022CC8A695A",
    ),
    "UnrealEditor-TRIADSensorFusion.exp": (
        565_592,
        "7B3BA3AD58D287846ED93FECAE3939B78050439254BE397385DD4C7D9F4223D1",
    ),
    "UnrealEditor-TRIADSensorFusion.pdb": (
        99_340_288,
        "54B6ABA6F5573B0EBCAD89AFCDEC3DD176C265995BF0AF950CB007F6A2D484A1",
    ),
    "UnrealEditor-TRIADSensorFusionEditor.dll": (
        7_588_352,
        "EA930B29A95E341E5EB3667C3C3F796345F6A538AB75FFC80977C36CC8610028",
    ),
    "UnrealEditor-TRIADSensorFusionEditor.exp": (
        309_114,
        "D6CEB13EFEF5C940DD1B3FD906B44EDBEDF092E94033C73DBC02B4C870BFAA5D",
    ),
    "UnrealEditor-TRIADSensorFusionEditor.pdb": (
        106_328_064,
        "70A38FD67543E577C008DC8233E331775265FA4EC980035EDB0812FCDB64BECC",
    ),
    "UnrealEditor.modules": (
        186,
        "E9AB983A61BF374D9AE1EBB0E4DB44936A08E7EB103BF52A2973E158BF83C238",
    ),
}

# The committed R25 transaction remains an immutable historical receipt. These
# integration sources below legitimately advance in additive successors, so the
# test verifies their frozen R25 values inside the wrapper rather than falsely
# requiring the live worktree to remain byte-identical forever.
SUCCESSOR_MUTABLE_SOURCE_MARKERS = {
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.h": "LandmarkVegetationR26",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.cpp": "LandmarkVegetationR26",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DContextPolicyActor.h": "BroadShellR31",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DContextPolicyActor.cpp": "BroadShellR31",
    "Plugins/TRIADSensorFusion/Tests/"
    "test_istana_explore_v5d_context_facade_r25_contract.py": "LandmarkVegetationR26",
}

OBSERVED_DEPENDENCY_COMPILE_BASES = (
    "TRIADIstanaExploreV5AppearanceActor.cpp",
    "TRIADIstanaExploreV5DGroundVegetationActor.cpp",
    "TRIADIstanaExploreV5DMacDonaldHouseActor.cpp",
    "TRIADIstanaExploreV5DTemasekShophouseActor.cpp",
    "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp",
    "TRIADIstanaExploreV5DMacDonaldHouseEditorLibrary.cpp",
    "TRIADIstanaExploreV5DTemasekShophouseEditorLibrary.cpp",
    "TRIADSensorFusionEditor.init.gen.cpp",
)

OBSERVED_UHT_MUTATIONS = (
    r"TRIADSensorFusion\UHT\Timestamp",
    r"TRIADSensorFusionEditor\UHT\Timestamp",
    r"TRIADSensorFusionEditor\UHT\TRIADSensorFusionEditor.init.gen.cpp",
)

OBSERVED_CROSS_PLUGIN_UHT_MUTATION = (
    r"Plugins\AirSimTriadRuntime\Intermediate\Build\Win64\UnrealEditor"
    r"\Inc\AirSimTriadRuntime\UHT\Timestamp"
)

OBSERVED_GLOBAL_BUILD_MUTATIONS = (
    r"Intermediate\Build\SourceFileCache.bin",
    r"Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.deps",
    r"Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtmanifest",
    r"Intermediate\Build\Win64\UnrealEditor\Development\UnrealEditor.uhtpath",
    r"Intermediate\Build\Win64\x64\UnrealEditor\ActionHistory.bin",
    r"Intermediate\Build\Win64\x64\UnrealEditor\Development\DependencyCache.bin",
)

IMMUTABLE_CONTENT = {
    "M_IPV5C_ContextMassing_Master.uasset": (
        29_065,
        "47B0F9F804858280B53D4D03CEEA8E078F01740920AF589EFEA676EBC4D057B4",
    ),
    "M_IPV5C_OfficialContextRender.uasset": (
        15_159,
        "113CE217898FD5A65603493FFF82B4B0FC16C6AD3C0339097F61E78B911E326F",
    ),
    "M_IPV5C_OfficialContextRoof.uasset": (
        14_843,
        "4534461C849FD80A1D1BA34FE68239414B25F1FF31F4790BD857C9F828A8C943",
    ),
    "M_IPV5C_OsmFallbackContextRender.uasset": (
        15_170,
        "227415242B7F0D0E61F3474C57D605848D24EF7574CD67C2242DED75B22F8079",
    ),
    "M_IPV5C_OsmFallbackContextRoof.uasset": (
        14_693,
        "BF11B2C73A066E03923B83AADD9C1222CD80377B44A90EBFAE868454C05898EB",
    ),
    "SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.uasset": (
        761_010,
        "A2062A8A90CB5FFA4D0B6B1117E775230B39AF58CB57F3C26C792DD4861429EE",
    ),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def between(text: str, start: str, end: str) -> str:
    start_index = text.index(start)
    return text[start_index : text.index(end, start_index)]


class IstanaExploreV5DContextFacadeR25NativeTransactionContractTests(
    unittest.TestCase
):
    @classmethod
    def setUpClass(cls) -> None:
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")

    def test_wrapper_is_fresh_versioned_and_repo_only(self):
        self.assertTrue(WRAPPER.name.endswith("NativeTransactionV1.ps1"))
        self.assertIn(
            "triad.istana_explore_v5d.context_facade_r25.native_transaction.v1",
            self.wrapper,
        )
        self.assertIn("[switch] $Execute", self.wrapper)
        self.assertIn("[switch] $StaticSelfCheck", self.wrapper)
        self.assertIn("if (-not $Execute)", self.wrapper)
        self.assertIn("NativeTreeReadOrWritten = $false", self.wrapper)
        self.assertIn("UnrealBuildOrEditorLaunched = $false", self.wrapper)
        for forbidden in (
            "Capture-IstanaExploreV5DVegetationProviderEvidence.ps1",
            "Invoke-IstanaExploreV5DVegetationProviderReadyRetry.ps1",
        ):
            self.assertNotIn(forbidden, self.wrapper)

    def test_exact_nine_source_allowlist_is_byte_hash_pinned(self):
        section = between(
            self.wrapper,
            "$sourcePins = @(",
            "# These seven exact binary products",
        )
        self.assertEqual(9, section.count("RelativePath ="))
        self.assertEqual(9, section.count("NativeBeforePresent ="))
        for forbidden_source in (
            "TRIADIstanaExploreV5DMacDonaldHouseActor",
            "TRIADIstanaExploreV5DTemasekShophouseActor",
            "TRIADIstanaExploreV5DLandmarkVegetationActor",
        ):
            self.assertNotIn(forbidden_source, section)
        for relative, (
            size,
            digest,
            before_present,
            before_size,
            before_digest,
        ) in SOURCES.items():
            native = relative.replace("/", "\\")
            self.assertIn(native, section)
            path = REPO / "unreal" / Path(relative)
            self.assertTrue(path.is_file(), relative)
            if relative not in SUCCESSOR_MUTABLE_SOURCE_MARKERS:
                self.assertEqual(size, path.stat().st_size, relative)
                self.assertEqual(digest, sha256(path), relative)
            else:
                self.assertIn(
                    SUCCESSOR_MUTABLE_SOURCE_MARKERS[relative],
                    path.read_text(encoding="utf-8"),
                    relative,
                )
            entry_start = section.index(native)
            next_entry = section.find("[pscustomobject]", entry_start + 1)
            entry = section[entry_start : next_entry if next_entry >= 0 else None]
            self.assertIn(f"Bytes = {size}L", entry)
            self.assertIn(digest, entry)
            self.assertIn(
                f"NativeBeforePresent = ${str(before_present).lower()}", entry
            )
            self.assertIn(f"NativeBeforeBytes = {before_size}L", entry)
            self.assertIn(before_digest, entry)
        self.assertIn("$rows.Count -ne 9", self.wrapper)
        self.assertIn("$seen.Count -ne 9", self.wrapper)

    def test_current_project_map_and_dll_pins_are_exact(self):
        expected = (
            (1_298, "42114E7A55BAC2974ECAB36B16E19EAAE013B7D8B3E2354CE93E15933A0AAFC3"),
            (
                34_992_354,
                "4A5F5514C7C3B508567465BA1F3B2FE8F31F4DAAAC5E317C8C57F1C30B50FD08",
            ),
            (
                4_577_280,
                "C7A80FD955ED831A7148068444079E54D7D9847759D7DAAC24272022CC8A695A",
            ),
            (
                7_588_352,
                "EA930B29A95E341E5EB3667C3C3F796345F6A538AB75FFC80977C36CC8610028",
            ),
        )
        for size, digest in expected:
            self.assertIn(f"Bytes = {size}L", self.wrapper)
            self.assertIn(digest, self.wrapper)
        self.assertIn("Assert-InitialNativePins", self.wrapper)
        self.assertIn("predecessor hybrid map", self.wrapper)
        self.assertIn("predecessor runtime DLL", self.wrapper)
        self.assertIn("predecessor editor DLL", self.wrapper)

    def test_v5c_material_and_v2_mesh_pins_are_immutable(self):
        section = between(
            self.wrapper,
            "$immutableContentPins = @(",
            "$r25PrimaryAssetRelativePaths = @(",
        )
        self.assertEqual(6, section.count("RelativePath ="))
        for name, (size, digest) in IMMUTABLE_CONTENT.items():
            self.assertIn(name, section)
            self.assertIn(f"Bytes = {size}L", section)
            self.assertIn(digest, section)
        self.assertIn("Assert-ImmutableContentPins", self.wrapper)
        self.assertGreaterEqual(
            self.wrapper.count("Assert-ImmutableContentPins"), 8
        )

    def test_exact_five_additive_asset_roster_starts_absent(self):
        section = between(
            self.wrapper,
            "$r25PrimaryAssetRelativePaths = @(",
            "$script:protectedUE54Before",
        )
        expected = (
            "M_IPV5D_ContextFacadeR25_Master.uasset",
            "MI_IPV5D_ContextFacadeR25_OfficialWall.uasset",
            "MI_IPV5D_ContextFacadeR25_OfficialRoof.uasset",
            "MI_IPV5D_ContextFacadeR25_FallbackWall.uasset",
            "MI_IPV5D_ContextFacadeR25_FallbackRoof.uasset",
        )
        self.assertEqual(5, section.count(".uasset"))
        for name in expected:
            self.assertIn(name, section)
        inventory = between(
            self.wrapper,
            "function Get-R25AssetInventory {",
            "function Test-ExactCommandLineToken {",
        )
        self.assertIn("$RequireAbsent", inventory)
        self.assertIn("$RequireComplete", inventory)
        self.assertIn("@('.uasset', '.uexp', '.ubulk', '.uptnl')", inventory)
        self.assertIn("Unexpected file in exact R25 asset root", inventory)
        self.assertIn("Get-R25AssetInventory -RequireAbsent", self.wrapper)

    def test_110_state_build_and_uht_rollback_roster_is_exact(self):
        section = between(
            self.wrapper,
            "$buildProductPins = @(",
            "$compileArtifactSuffixes = @(",
        )
        self.assertEqual(7, section.count("RelativePath ="))
        for name, (size, digest) in BUILD_PRODUCTS.items():
            self.assertIn(name, section)
            self.assertIn(f"Bytes = {size}L", section)
            self.assertIn(digest, section)
        auxiliary = between(
            self.wrapper,
            "$compileArtifactSuffixes = @(",
            "$immutableContentPins = @(",
        )
        for suffix in (
            ".dep.json",
            ".obj",
            ".obj.rsp",
            ".obj.rsp.old",
            ".sarif",
        ):
            self.assertIn(f"'{suffix}'", auxiliary)
        self.assertEqual(15, auxiliary.count("Base ="))
        self.assertEqual(7, auxiliary.count("Admission = 'R25_DIRECT'"))
        self.assertEqual(
            8,
            auxiliary.count(
                "Admission = 'OBSERVED_20260905_DEPENDENCY_REBUILD'"
            ),
        )
        for base in OBSERVED_DEPENDENCY_COMPILE_BASES:
            self.assertIn(f"Base = '{base}'", auxiliary)
        self.assertIn("@('TRIADSensorFusion', 'TRIADSensorFusionEditor')", auxiliary)
        self.assertEqual(10, auxiliary.count("\\UHT\\"))
        for suffix in OBSERVED_UHT_MUTATIONS:
            self.assertIn(suffix, auxiliary)
        self.assertIn(OBSERVED_CROSS_PLUGIN_UHT_MUTATION, auxiliary)
        for path in OBSERVED_GLOBAL_BUILD_MUTATIONS:
            self.assertIn(path, auxiliary)
        definition = between(
            self.wrapper,
            "function Assert-BuildRollbackRosterDefinition {",
            "function Get-PinnedBuildProductRows {",
        )
        self.assertIn("$compileArtifactSpecs.Count -ne 15", definition)
        self.assertIn("$r25UhtOutputRelativePaths.Count -ne 6", definition)
        self.assertIn(
            "$observedUhtMutationRelativePaths.Count -ne 3", definition
        )
        self.assertIn(
            "$observedCrossPluginUhtMutationRelativePaths.Count -ne 1",
            definition,
        )
        self.assertIn(
            "$observedGlobalBuildMutationRelativePaths.Count -ne 6",
            definition,
        )
        self.assertIn("$buildAuxiliaryRelativePaths.Count -ne 103", definition)
        self.assertIn("$relativePaths.Count -ne 110", definition)
        self.assertIn("$seen.Count -ne 110", definition)
        self.assertIn("[IO.Path]::IsPathRooted", definition)
        self.assertIn("Invalid or duplicate R25 build rollback path", definition)
        roster = between(
            self.wrapper,
            "function Get-PinnedBuildProductRows {",
            "function Get-R25AssetInventory {",
        )
        self.assertIn("Assert-BuildRollbackRosterDefinition", roster)
        self.assertIn("$buildAuxiliaryRelativePaths.Count -ne 103", roster)
        self.assertIn("$rows.Count -ne 110", roster)
        self.assertIn("$seen.Count -ne 110", roster)
        self.assertIn("PinnedBinary = $false", roster)
        backup = between(
            self.wrapper,
            "function New-VerifiedTransactionBackups {",
            "function Publish-NativeSources {",
        )
        self.assertIn("source_before", backup)
        self.assertIn("build_before", backup)
        self.assertIn("content_before", backup)
        self.assertIn("Copy-NewPinnedFile", backup)
        self.assertIn("$BuildRows.Count", backup)
        self.assertIn("if ($row.Before.Present)", backup)
        self.assertIn("PinnedBinaryProductCount = 7", self.wrapper)
        self.assertIn("ScopedBuildAndUhtProductCount = 103", self.wrapper)
        self.assertIn(
            "ObservedOutOfRosterMutationClosureCount = 34", self.wrapper
        )
        self.assertIn(
            "ConservativeObservedCompileSidecarCount = 16", self.wrapper
        )
        self.assertIn("TotalBuildProductCount = 110", self.wrapper)
        self.assertIn("TotalBuildProductRollbackCount = 110", self.wrapper)
        self.assertIn(
            "$script:buildRollbackContract = "
            "Assert-BuildRollbackRosterDefinition",
            self.wrapper,
        )
        self.assertIn(
            "BuildRollbackContract = $script:buildRollbackContract",
            self.wrapper,
        )
        self.assertIn("WholeNativeTreeRollbackClaimed = $false", self.wrapper)
        for ephemeral in (
            r"Intermediate\CachedAssetRegistry_0.bin",
            r"Intermediate\Config\CoalescedSourceConfigs\*.ini",
            r"Intermediate\PipInstall\**",
            r"Saved\Autosaves\PackageRestoreData.json",
            r"Saved\Config\WindowsEditor\EditorPerProjectUserSettings.ini",
            r"Saved\ShaderDebugInfo\**\DDCKey-Editor.txt",
            r"Saved\DerivedDataCache\**",
        ):
            self.assertIn(ephemeral, auxiliary)

    def test_every_backup_and_receipt_is_create_new(self):
        copy = between(
            self.wrapper,
            "function Copy-NewPinnedFile {",
            "function Write-NewJsonReceipt {",
        )
        self.assertIn("Assert-NewPath", copy)
        self.assertIn("[IO.File]::Copy($Source, $Destination, $false)", copy)
        receipt = between(
            self.wrapper,
            "function Write-NewJsonReceipt {",
            "function Get-WorkspaceSourceRows {",
        )
        self.assertIn("[IO.FileMode]::CreateNew", receipt)
        self.assertIn("$stream.Flush($true)", receipt)
        self.assertNotIn("Copy-Item", self.wrapper)
        self.assertNotIn("Remove-Item", self.wrapper)
        self.assertIn("BackupWritesWereNonOverwriting = $true", self.wrapper)

    def test_dependency_closure_is_discovered_and_covered_before_writes(self):
        section = between(
            self.wrapper,
            "function Get-ReviewedBuildDependencyClosure {",
            "function Get-PinnedBuildProductRows {",
        )
        self.assertIn("$expectedDependencyClosureKeys.Count -ne 13", section)
        self.assertIn("-Filter '*.dep.json'", section)
        self.assertIn("ConvertFrom-Json -Depth 20", section)
        self.assertIn("$document.Data.Includes", section)
        self.assertIn("Unjournalled R25 dependency-rebuild translation unit", section)
        self.assertIn("$actual.Count -ne 13", section)
        self.assertIn("EXACT_13_DEPENDENCY_CLOSURE_COVERED", section)
        for base in OBSERVED_DEPENDENCY_COMPILE_BASES[:-1]:
            self.assertIn(base, self.wrapper)
        gate = between(
            self.wrapper,
            "# Final gate immediately before the first D: write.",
            "[void] [IO.Directory]::CreateDirectory($transactionRoot)",
        )
        self.assertIn("$finalDependencyClosure", gate)
        self.assertIn("ConvertTo-Json -Compress -Depth 20", gate)
        self.assertIn(
            "dependency closure changed before the first native write", gate
        )

    def test_build_is_two_module_scoped_and_source_discovery_forced(self):
        build = between(
            self.wrapper,
            "function Invoke-R25Build {",
            "function Assert-InitialNativePins {",
        )
        self.assertIn("'UnrealEditor'", build)
        self.assertIn("'-Module=TRIADSensorFusion'", build)
        self.assertIn("'-Module=TRIADSensorFusionEditor'", build)
        self.assertIn("'-NoUBTMakefiles'", build)
        self.assertIn("'-ForceHeaderGeneration'", build)
        self.assertIn("'-MaxParallelActions=1'", build)
        self.assertIn("'-NoUBA'", build)
        self.assertIn("'-NoUBALocal'", build)
        self.assertIn("& $buildTool @buildArguments *> $buildLog", build)
        self.assertIn("did not produce distinct runtime and editor DLL", build)
        self.assertIn("$script:builtProductPins", build)
        boundary = between(
            self.wrapper,
            "function Assert-WorkspaceAndNativeCodeBoundary {",
            "function Assert-OwnedBoundary {",
        )
        self.assertIn("foreach ($pin in $script:builtProductPins)", boundary)

    def test_forced_uht_and_fresh_reflection_gate_precede_remote_control(self):
        gate = between(
            self.wrapper,
            "function Assert-FreshGeneratedR25Reflection {",
            "function Invoke-R25Build {",
        )
        endpoints = (
            "ImportIstanaExploreV5DContextFacadeR25Assets",
            "ValidateIstanaExploreV5DContextFacadeR25Assets",
            "ApplyIstanaExploreV5DContextFacadeR25ToLoadedHybridMap",
            "ValidateIstanaExploreV5DContextFacadeR25SuccessorMap",
        )
        for endpoint in endpoints:
            self.assertIn(endpoint, gate)
        self.assertIn("$row.After.Sha256 -ceq $row.Before.Sha256", gate)
        self.assertIn("Forced UHT did not replace stale generated reflection", gate)
        self.assertIn("[IO.File]::ReadAllText($row.Path)", gate)
        self.assertIn("[IO.File]::ReadAllBytes($editorDll)", gate)
        self.assertIn("Built editor DLL lacks reflected R25 endpoint", gate)
        self.assertIn(
            "FRESH_GENERATED_R25_REFLECTION_AND_DLL_TEXT_VALID", gate
        )
        build = between(
            self.wrapper,
            "function Invoke-R25Build {",
            "function Assert-InitialNativePins {",
        )
        self.assertEqual(1, build.count("'-ForceHeaderGeneration'"))
        self.assertIn("Assert-FreshGeneratedR25Reflection", build)
        self.assertIn("ReflectionApi = $reflectionApi", build)
        main = self.wrapper[
            self.wrapper.index("$preparedReceipt = Write-NewJsonReceipt") :
        ]
        self.assertLess(
            main.index("Invoke-R25Build"),
            main.index("-Stage '01_import_assets'"),
        )

    def test_native_stage_order_is_exact_and_five_processes_are_fresh(self):
        main = self.wrapper[
            self.wrapper.index("$preparedReceipt = Write-NewJsonReceipt") :
        ]
        ordered = (
            "Publish-NativeSources",
            "Invoke-R25Build",
            "-Stage '01_import_assets'",
            "-Stage '02_cold_validate_assets'",
            "-Stage '03_apply_map'",
            "-Stage '04_cold_validate_map'",
            "-Stage '05_idempotent_apply'",
        )
        cursor = -1
        for marker in ordered:
            current = main.index(marker, cursor + 1)
            self.assertGreater(current, cursor)
            cursor = current
        self.assertEqual(5, main.count("Invoke-StrictColdEditorStage `"))
        helper = between(
            self.wrapper,
            "function Invoke-StrictColdEditorStage {",
            "function New-VerifiedTransactionBackups {",
        )
        self.assertIn("Start-Process -FilePath $editor", helper)
        self.assertIn("-PassThru -WindowStyle Hidden", helper)
        self.assertIn("RenderingCapable = $true", helper)
        self.assertIn("FreshProcess = $true", helper)

    def test_remote_calls_and_success_prefixes_are_exact(self):
        expected = (
            "ImportIstanaExploreV5DContextFacadeR25Assets",
            "EXPLORE_V5D_CONTEXT_FACADE_R25_ASSET_IMPORT_PASS:",
            "ValidateIstanaExploreV5DContextFacadeR25Assets",
            "ISTANA_EXPLORE_V5D_CONTEXT_FACADE_R25_ASSETS_VALID",
            "ApplyIstanaExploreV5DContextFacadeR25ToLoadedHybridMap",
            "EXPLORE_V5D_CONTEXT_FACADE_R25_APPLY_PASS:",
            "ValidateIstanaExploreV5DContextFacadeR25SuccessorMap",
            "ISTANA_EXPLORE_V5D_CONTEXT_FACADE_R25_MAP_VALID:",
            "IDEMPOTENT_EXPLORE_V5D_CONTEXT_FACADE_R25_ALREADY_VALID:",
        )
        for marker in expected:
            self.assertIn(marker, self.wrapper)
        main = self.wrapper[
            self.wrapper.index("$preparedReceipt = Write-NewJsonReceipt") :
        ]
        self.assertEqual(
            2,
            main.count(
                "'ApplyIstanaExploreV5DContextFacadeR25ToLoadedHybridMap'"
            ),
        )
        self.assertIn("$assetStageMapPackage = $hybridMapPackage", self.wrapper)
        self.assertIn("$hybridMapPackage", self.wrapper)

    def test_apply_requires_one_save_17_overrides_and_no_authority_drift(self):
        for marker in (
            "oneSave=true",
            "materialOverrides=17",
            "V2MeshUnchanged=true",
            "landmarksUnchanged=true",
            "providerSettingsUnchanged=true",
            "collisionNavigationSensorRfAuthority=false",
            "sharedV5CAssetsMutated=false",
        ):
            self.assertIn(marker, self.wrapper)
        self.assertIn("$successorMapPin.Sha256 -ceq $expectedMapPin.Sha256", self.wrapper)
        self.assertIn(
            "idempotent R25 second apply map receipt", self.wrapper
        )
        self.assertIn("IdempotentMapBytesAndHashUnchanged = $true", self.wrapper)

    def test_capstone_is_an_exact_immutable_session_set(self):
        protected = between(
            self.wrapper,
            "function Get-ProtectedUE54Identity {",
            "function Get-NativeTRIADUnrealProcesses {",
        )
        self.assertIn(r"C:\Program Files\Epic Games\UE_5.4", self.wrapper)
        self.assertIn(
            r"C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject",
            self.wrapper,
        )
        self.assertIn("Test-ExactCommandLineToken", protected)
        self.assertIn("CreationUtcTicks", protected)
        self.assertIn("ExecutablePath", protected)
        self.assertIn("ProjectPath", protected)
        self.assertIn("CommandLine", protected)
        self.assertIn("Sort-Object ProcessId", protected)
        self.assertIn("SessionCount = $identities.Count", protected)
        self.assertIn("Sessions = @($identities)", protected)
        self.assertIn("unexpected process", protected)
        self.assertIn("$beforeCanonical", protected)
        self.assertIn("$afterCanonical", protected)
        self.assertIn("ConvertTo-Json -Compress -Depth 8", protected)
        self.assertNotIn("More than one", protected)

    def test_only_ue55_or_exact_native_project_helpers_are_blocked(self):
        selector = between(
            self.wrapper,
            "function Get-NativeTRIADUnrealProcesses {",
            "function Get-RemoteControlListeners {",
        )
        self.assertIn("$enginePrefix", selector)
        self.assertIn("$isUE55", selector)
        self.assertIn("$isNativeTRIAD", selector)
        self.assertIn("-Token $nativeProjectFile", selector)
        self.assertIn("$isUE55 -or $isNativeTRIAD", selector)
        idle = between(
            self.wrapper,
            "function Assert-NativeProjectIdle {",
            "function Assert-LaunchedProcessIdentity {",
        )
        self.assertIn("Get-NativeTRIADUnrealProcesses", idle)
        self.assertIn("RC port 30010 must be unowned", idle)

    def test_rc_is_idle_or_solely_owned_by_exact_started_helper(self):
        owned = between(
            self.wrapper,
            "function Assert-OwnedBoundary {",
            "function Invoke-RcCall {",
        )
        self.assertIn("$nativeEditors.Count -ne 1", owned)
        self.assertIn("$ExpectedProcess.ProcessId", owned)
        self.assertIn("$listeners.Count -ne 1", owned)
        self.assertIn("$foreignListeners", owned)
        identity = between(
            self.wrapper,
            "function Test-ExpectedHelperLaunchIdentity {",
            "function Assert-WorkspaceAndNativeCodeBoundary {",
        )
        for marker in (
            "$Handle.Id",
            "$nativeProjectFile",
            "$MapPackage",
            "$LogFile",
            "-NoAutoSave",
            "-RemoteControlHttpServer",
            "-RCWebControlEnable",
            "WebControl.StartServer",
            "FromSeconds(2)",
        ):
            self.assertIn(marker, identity)
        self.assertIn("Stop-ExactHelperForContainment", self.wrapper)
        self.assertIn("Process handle does not match", self.wrapper)

    def test_failure_rolls_back_content_build_then_sources(self):
        catch_marker = "catch {\n    $transactionError = $_.Exception.Message"
        catch = self.wrapper[self.wrapper.index(catch_marker) :]
        ordered = (
            "Restore-R25Content",
            "Restore-BuildProducts",
            "Restore-NativeSources",
        )
        cursor = -1
        for marker in ordered:
            current = catch.index(marker, cursor + 1)
            self.assertGreater(current, cursor)
            cursor = current
        self.assertIn("FAILED_AND_ROLLED_BACK", catch)
        self.assertIn("FAILED_ROLLBACK_INCOMPLETE", catch)
        self.assertIn("stopped fail-closed", catch)
        self.assertIn("BackupsRetained = $true", catch)
        self.assertIn("RecursiveDeleteUsed = $false", catch)
        self.assertIn("ONE_HUNDRED_TEN_BUILD_PRODUCT_STATES", catch)
        self.assertIn("[IO.Directory]::Delete($materialsRoot, $false)", self.wrapper)
        self.assertIn("[IO.Directory]::Delete($r25AssetRoot, $false)", self.wrapper)
        self.assertNotIn("[IO.Directory]::Delete($r25AssetRoot, $true)", self.wrapper)
        restore = between(
            self.wrapper,
            "function Restore-OneFileFromBackup {",
            "function Restore-NativeSources {",
        )
        self.assertIn("[IO.File]::Replace", restore)
        self.assertIn("[IO.File]::Delete($Destination)", restore)
        self.assertIn("has backup material for an absent predecessor", restore)

    def test_final_prewrite_gate_revalidates_every_external_pin(self):
        gate = between(
            self.wrapper,
            "# Final gate immediately before the first D: write.",
            "[void] [IO.Directory]::CreateDirectory($transactionRoot)",
        )
        self.assertIn("Assert-NativeProjectIdle", gate)
        self.assertIn("Assert-SameState -Expected $script:selfPin", gate)
        self.assertIn("Assert-WorkspaceSourcesUnchanged", gate)
        self.assertIn("Assert-NativeSourcePredecessors", gate)
        self.assertIn("Assert-InitialNativePins", gate)
        self.assertIn("Assert-SameState -Expected $row.Before", gate)
        self.assertIn("Assert-NewPath -Path $transactionRoot", gate)

    def test_powershell_parser_accepts_wrapper_without_executing_it(self):
        pwsh = shutil.which("pwsh") or shutil.which("pwsh.exe")
        if pwsh is None:
            self.skipTest("PowerShell 7 is unavailable")
        wrapper_literal = str(WRAPPER).replace("'", "''")
        command = (
            "$tokens=$null;$errors=$null;"
            "[void][Management.Automation.Language.Parser]::ParseFile("
            f"'{wrapper_literal}',[ref]$tokens,[ref]$errors);"
            "if($errors.Count -ne 0){$errors | ForEach-Object {"
            "[Console]::Error.WriteLine($_.ToString())};exit 1}"
        )
        completed = subprocess.run(
            [
                pwsh,
                "-NoLogo",
                "-NoProfile",
                "-NonInteractive",
                "-Command",
                command,
            ],
            cwd=REPO,
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(0, completed.returncode, completed.stderr)


if __name__ == "__main__":
    unittest.main()
