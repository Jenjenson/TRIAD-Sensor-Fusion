from __future__ import annotations

import hashlib
import json
import shutil
import subprocess
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
UNREAL = REPO / "unreal"
WRAPPER = (
    REPO
    / "scripts"
    / "Invoke-IstanaExploreV5DVisualRealismR28NativeTransactionV1.ps1"
)

SOURCE_NATIVE_BEFORE = {
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DLandmarkVegetationActor.h": (
        True,
        11_493,
        "1B73DECAEB572876817F63B3B4EA8C6362F0695C9F2468137EDA92D148107D08",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DLandmarkVegetationActor.cpp": (
        True,
        55_817,
        "537C35F16F10C0E3BDFD4E25F41028E5FE2875134B08F0D3C92805A25BA00BEF",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.h": (
        True,
        11_566,
        "17419FF25CD3BFECCFF082635F8ED1C9668BC4EA4D4BCA7E5D5EAEDAF7EBAD06",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp": (
        True,
        722_720,
        "B4371454C084CBE47D52E31421808C867004AE0418A66D560973C5C850AC713F",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h": (
        True,
        1_619,
        "4718F81CDE54D999E6559382278C9EBBFE426936931F7B5669A84997B23F1A01",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp": (
        True,
        28_044,
        "7926155C2AEEDABD19BC1C56534AEC3DE3C83E41D9382E5219B576E2D6EDA7F1",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.h": (
        True,
        11_654,
        "EE0A2CE3FAC359B7DDDF808775E748D7E29214437EFD438456D7333819F31661",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.cpp": (
        True,
        452_992,
        "D82148FCB9E9B647B1D3DFD1C5E68582052EF6175160214771CF6398E6705317",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DR28EnvironmentActor.h": (False, 0, "ABSENT"),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DR28EnvironmentActor.cpp": (False, 0, "ABSENT"),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DR28EnvironmentAssetFactory.h": (False, 0, "ABSENT"),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DR28EnvironmentAssetFactory.cpp": (False, 0, "ABSENT"),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.h": (False, 0, "ABSENT"),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.cpp": (False, 0, "ABSENT"),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DContextPolicyActor.h": (
        True,
        9_003,
        "07858E83E7E73E103922A0FB3EA52FA76F30A9D96F1A402DA6631F75E9A73774",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DContextPolicyActor.cpp": (
        True,
        93_879,
        "6E2C5697AE3949CFD009D4762A334E71A09DFAEA60F53823C670D19137C082EA",
    ),
}

SOURCE_REPO_PINS = {
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DLandmarkVegetationActor.h": (
        12_748,
        "1040C15E3B7274E8706B9F4A13A4C6FA3B67A234678771F4638CE01E06406BB3",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DLandmarkVegetationActor.cpp": (
        84_297,
        "7F9B3F52286C2C02D4C1CB9993499BE7478D44BAB8DF7DA125941C53F476EE43",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.h": (
        12_448,
        "EF2D6A32781462565FAA568B51F3FE0B322623CF82090E399EF2311196D90553",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp": (
        726_040,
        "1BDABF90F5E04BE0487DA93C0806A9E89DCAA05B1B35A77FA6C10857D6CE15B4",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h": (
        2_777,
        "6337874A05B8CBDF321C81B7F18B217B30788B63757FB797F0BAA8E06900F430",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp": (
        53_669,
        "6449EF8F534E2EAAB501063A5A0E6253E1715655C4A53E4A9020CD98B455032E",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.h": (
        14_928,
        "52DE5BE0B7228669242C2620DFF2191004AE630520C9073729E3FA9543B26F01",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.cpp": (
        505_613,
        "FACFF7B8757DF68B824937CA04AE79F4B2B2A55D08B32711424E717BD0FE7DCD",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DR28EnvironmentActor.h": (
        4_648,
        "D38DAD76CEA609F032BA250E6648B3F66FA964DE7E6E3CC9BF4E692A57C7B703",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DR28EnvironmentActor.cpp": (
        17_090,
        "2BBA03A7AF2559D2D0D312B5A875D99439D9545F3EDE327EAEEECDBDCBB38781",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DR28EnvironmentAssetFactory.h": (
        950,
        "CC916ADEFF7F60D24EEE6D8FA5220FF79A6139E7D8FBCDEE9E62DE0CAA53B3FD",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DR28EnvironmentAssetFactory.cpp": (
        48_142,
        "DE744EE356EBBF4E5CAD18F41E9B0D915EDCD541AF9481B113E84338FE110EEE",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.h": (
        1_234,
        "D979B3B833F85E4B2F1B7B7041742878C07A8B024066E473FCDD3144F6AA7284",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.cpp": (
        9_898,
        "737190BE5C6A9B0A412224447A5929C237CC22651637483B545C553118D3923B",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DContextPolicyActor.h": (
        10_340,
        "B81DE0307CC50F8FB89373900D0C7B63776C41419935FAB5F0EF64DB60E8894E",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DContextPolicyActor.cpp": (
        125_137,
        "BF3750FD6A4C65885DC8E4D94BB97A2464F83CE0B9B5119830E1A0D17F1FF841",
    ),
}

SOURCE_ASSETS = {
    "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
    "R28EnvironmentalDressing/Generated/"
    "SM_IPV5D_R28_ContextArchitecturalDressing_Render.obj": (
        59_534_571,
        "77E17482E73AB65222D5B0B0C70A407DB898F360FE1051BB338FDF31C0B88096",
    ),
    "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
    "R28EnvironmentalDressing/Generated/"
    "SM_IPV5D_R28_ConnectivePublicRealm_Render.obj": (
        12_484_549,
        "C8CD38ACB1C1730EA928FC31FC825C8803DF7B8F7E13878254776714AFE26842",
    ),
    "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
    "R28EnvironmentalDressing/Generated/"
    "IstanaPublicViewV5DR28EnvironmentalDressing.mtl": (
        1_233,
        "C237F52740740E95F510502F037D8C4CA9A3AA735C8EC47848DCB7B0D97CA946",
    ),
    "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
    "R28EnvironmentalDressing/Generated/"
    "IstanaPublicViewV5DR28EnvironmentalDressing.manifest.json": (
        20_596,
        "3366F0B1D6C7A5801538B897488DCF506D1A1014AFB4E8676B64223420C8C641",
    ),
    "SourceAssets/IstanaPublicViewExploreV5D/Surroundings/"
    "R28EnvironmentalDressing/r28_environmental_dressing.contract.json": (
        9_317,
        "0CA25D72C3FAAEA2AF445E74ED374AD9AAA149DDC4BDF0691DC235435D9C8DF8",
    ),
}

STAGES = (
    "00_cold_validate_r27_pre_r28",
    "01_cold_validate_phase2_pre_r28",
    "02_build_r28_grass_materials",
    "03_validate_r28_grass_materials",
    "04_validate_r28_vegetation_assets",
    "05_ensure_r28_environment_assets",
    "06_validate_r28_environment_assets",
    "07_apply_combined_r28_visual_successor",
    "08_cold_validate_r28_environment_map",
    "09_cold_validate_combined_r28_map",
    "10_idempotent_combined_r28_apply",
    "11_postvalidate_combined_r28_r25_provider_contract",
    "12_cold_validate_phase2_post_r28",
    "13_postvalidate_r28_grass_materials",
)

R28_GRASS_ASSETS = (
    "Content\\TRIAD\\IstanaPublicViewExploreV5D\\LandmarkVegetationR28\\Materials\\"
    "M_IPV5D_LandmarkTurf_R28_Manicured.uasset",
    "Content\\TRIAD\\IstanaPublicViewExploreV5D\\LandmarkVegetationR28\\Materials\\"
    "M_IPV5D_LandmarkTurf_R28_Humid.uasset",
    "Content\\TRIAD\\IstanaPublicViewExploreV5D\\LandmarkVegetationR28\\Materials\\"
    "M_IPV5D_LandmarkTurf_R28_Shade.uasset",
    "Content\\TRIAD\\IstanaPublicViewExploreV5D\\LandmarkVegetationR28\\Materials\\"
    "M_IPV5D_LandmarkTurf_R28_DryEdge.uasset",
)

R28_ENVIRONMENT_ASSETS = tuple(
    "Content\\TRIAD\\IstanaPublicViewExploreV5D\\SurroundingsRealismR28\\"
    + relative
    for relative in (
        "Materials\\M_IPV5D_R28_Surface_Master.uasset",
        "Materials\\MI_IPV5D_R28_GlassCool.uasset",
        "Materials\\MI_IPV5D_R28_GlassWarm.uasset",
        "Materials\\MI_IPV5D_R28_FrameLight.uasset",
        "Materials\\MI_IPV5D_R28_FrameDark.uasset",
        "Materials\\MI_IPV5D_R28_RoofTrim.uasset",
        "Materials\\MI_IPV5D_R28_Asphalt.uasset",
        "Materials\\MI_IPV5D_R28_Curb.uasset",
        "Materials\\MI_IPV5D_R28_Sidewalk.uasset",
        "Materials\\MI_IPV5D_R28_Verge.uasset",
        "Materials\\MI_IPV5D_R28_OuterGround.uasset",
        "Meshes\\SM_IPV5D_R28_ConnectivePublicRealm_Render.uasset",
        "Meshes\\SM_IPV5D_R28_ContextArchitecturalDressing_Render.uasset",
    )
)

R28_DIRECT_OBJECTS = tuple(
    "Plugins\\TRIADSensorFusion\\Intermediate\\Build\\Win64\\x64\\"
    "UnrealEditor\\Development\\"
    + relative
    for relative in (
        "TRIADSensorFusion\\TRIADIstanaExploreV5DLandmarkVegetationActor.cpp.obj",
        "TRIADSensorFusion\\TRIADIstanaExploreV5DR28EnvironmentActor.cpp.obj",
        "TRIADSensorFusion\\TRIADIstanaExploreV5DContextPolicyActor.cpp.obj",
        "TRIADSensorFusionEditor\\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp.obj",
        "TRIADSensorFusionEditor\\TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp.obj",
        "TRIADSensorFusionEditor\\TRIADIstanaExploreV5DR28EnvironmentAssetFactory.cpp.obj",
        "TRIADSensorFusionEditor\\TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.cpp.obj",
        "TRIADSensorFusionEditor\\TRIADIstanaExploreV5DHybridEditorLibrary.cpp.obj",
    )
)

R28_GENERATED_OBJECTS = tuple(
    "Plugins\\TRIADSensorFusion\\Intermediate\\Build\\Win64\\x64\\"
    "UnrealEditor\\Development\\"
    + relative
    for relative in (
        "TRIADSensorFusion\\TRIADIstanaExploreV5DLandmarkVegetationActor.gen.cpp.obj",
        "TRIADSensorFusion\\TRIADIstanaExploreV5DR28EnvironmentActor.gen.cpp.obj",
        "TRIADSensorFusion\\TRIADIstanaExploreV5DContextPolicyActor.gen.cpp.obj",
        "TRIADSensorFusionEditor\\TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.gen.cpp.obj",
        "TRIADSensorFusionEditor\\TRIADIstanaExploreV5DGroundVegetationEditorLibrary.gen.cpp.obj",
        "TRIADSensorFusionEditor\\TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.gen.cpp.obj",
        "TRIADSensorFusionEditor\\TRIADIstanaExploreV5DHybridEditorLibrary.gen.cpp.obj",
    )
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


class VisualRealismR28NativeTransactionContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not WRAPPER.is_file():
            raise AssertionError(f"missing R28 native wrapper: {WRAPPER}")
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")

    def test_default_is_read_only_and_live_requires_exact_r27_receipts(self):
        for token in (
            "triad.istana_explore_v5d.visual_realism_r28.native_transaction.v1",
            "[switch] $Execute",
            "[switch] $StaticSelfCheck",
            "[switch] $RequireR27Predecessor",
            "$PSBoundParameters.ContainsKey($name)",
            "Live R28 execution requires explicit caller-supplied",
            "if (-not $Execute)",
            "READ_ONLY_PREFLIGHT_PASS",
            "NativeTreeWritten = $false",
            "UnrealBuildOrEditorLaunched = $false",
            "36335930L",
            "9C9660B02F3B9FBF8C679182E8EB39B8FECD0A618AECA6ED546AAD39E0C895E5",
            "4676096L",
            "C341A9F2583248824911EBB5904EC357E0DCB840304C637D87FD0CE87E67B910",
            "7781888L",
            "30569F04A956FDCC24F5AE8C0A99D1B70D86C03208E9497EE9BBD8480ABCC2B6",
            "Saved\\TRIAD\\NativeTransactions\\V5DVisualRealismR28V1",
        ):
            self.assertIn(token, self.wrapper)
        self.assertLess(
            self.wrapper.index("if (-not $Execute)"),
            self.wrapper.index("$prewriteAdmission = Assert-LaunchAdmission"),
        )

    def test_exact_sixteen_repo_sources_and_native_before_states_are_pinned(self):
        section = between(self.wrapper, "$sourcePins = @(", "$sourceAssetPins = @(")
        self.assertEqual(set(SOURCE_NATIVE_BEFORE), set(SOURCE_REPO_PINS))
        self.assertEqual(16, section.count("RelativePath ="))
        self.assertEqual(10, section.count("NativeBeforePresent = $true"))
        self.assertEqual(6, section.count("NativeBeforePresent = $false"))
        for relative, (present, before_size, before_digest) in SOURCE_NATIVE_BEFORE.items():
            path = UNREAL / relative
            self.assertTrue(path.is_file(), relative)
            entry = pin_entry(section, relative)
            repo_size, repo_digest = SOURCE_REPO_PINS[relative]
            self.assertEqual(repo_size, path.stat().st_size, relative)
            self.assertEqual(repo_digest, sha256(path), relative)
            self.assertIn(f"Bytes = {repo_size}L", entry, relative)
            self.assertIn(repo_digest, entry, relative)
            literal = "$true" if present else "$false"
            self.assertIn(f"NativeBeforePresent = {literal}", entry, relative)
            self.assertIn(f"NativeBeforeBytes = {before_size}L", entry, relative)
            self.assertIn(before_digest, entry, relative)

    def test_exact_five_source_asset_specs_are_hash_pinned_and_absent_before(self):
        section = between(
            self.wrapper, "$sourceAssetPins = @(", "$r28SourceAssetRoot ="
        )
        self.assertEqual(5, section.count("RelativePath ="))
        self.assertEqual(5, section.count("NativeBeforePresent = $false"))
        for relative, (expected_size, expected_digest) in SOURCE_ASSETS.items():
            path = UNREAL / relative
            self.assertTrue(path.is_file(), relative)
            self.assertEqual(expected_size, path.stat().st_size, relative)
            self.assertEqual(expected_digest, sha256(path), relative)
            entry = pin_entry(section, relative)
            self.assertIn(f"Bytes = {expected_size}L", entry, relative)
            self.assertIn(expected_digest, entry, relative)
            self.assertIn("NativeBeforeBytes = 0L", entry, relative)
            self.assertIn("NativeBeforeSha256 = 'ABSENT'", entry, relative)

    def test_exact_seventeen_new_assets_and_immutable_ancestor_closure(self):
        new_assets = between(
            self.wrapper,
            "$r28GrassMaterialRelativePaths = @(",
            "$globalBuildRelativePaths = @(",
        )
        grass = between(
            new_assets,
            "$r28GrassMaterialRelativePaths = @(",
            "$r28EnvironmentRoot =",
        )
        environment = between(
            new_assets,
            "$r28EnvironmentRelativePaths = @(",
            "$r28NewAssetRelativePaths =",
        )
        self.assertEqual(4, grass.count(".uasset'"))
        self.assertEqual(13, environment.count(".uasset'"))
        for relative in R28_GRASS_ASSETS:
            self.assertIn(relative, grass)
        for relative in R28_ENVIRONMENT_ASSETS:
            self.assertIn(relative, environment)
        self.assertIn(
            "$r28NewAssetRelativePaths = @(\n"
            "    @($r28GrassMaterialRelativePaths) + @($r28EnvironmentRelativePaths))",
            new_assets,
        )
        immutable = between(
            self.wrapper, "$immutableContentPins = @(", "$r28GrassMaterialRoot ="
        )
        self.assertEqual(15, immutable.count("RelativePath ="))
        self.assertEqual(4, immutable.count("LandmarkVegetationR27"))
        closure = between(
            self.wrapper,
            "$immutableAncestorSourcePins = @(",
            "$immutableContentPins = @(",
        )
        self.assertEqual(6, closure.count("RelativePath="))
        for token in (
            "ContextFacadeR25AssetFactory.h",
            "ContextFacadeR25AssetFactory.cpp",
            "LocalFallbackSuppressionV2Provenance.h",
            "LocalFallbackSuppressionV2Provenance.cpp",
            "LocalFallbackSuppressionV2ProvenanceTests.cpp",
            "SourceAssets\\IstanaPublicView\\Generated\\SM_IstanaPublicView_Terrain.obj",
            "3909911L",
            "78AF53572EF53BACB684B5B2103D7BA427999C8223BDDD5C0DE76DEB8B16DD23",
            "Assert-ImmutableAncestorSourcePins -Native",
            "Assert-ImmutableContent",
        ):
            self.assertIn(token, self.wrapper)

    def test_fixed_memory_gate_precedes_every_authorized_launch_and_first_write(self):
        for token in (
            "$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L",
            "Get-CimInstance Win32_OperatingSystem",
            "FreeVirtualMemory",
            "Fixed 10 GiB FreeVirtualMemory launch admission refused",
            "PrewriteLaunchAdmission = $prewriteAdmission",
            "Status = 'PREWRITE_ADMISSION_PASS'",
            "PrewriteAdmissionReceipt = $prewriteAdmissionReceiptPin",
            "BuildLaunchAdmission = $buildAdmission",
            "LaunchAdmission = $launchAdmission",
        ):
            self.assertIn(token, self.wrapper)
        self.assertLess(
            self.wrapper.index("$prewriteAdmission = Assert-LaunchAdmission"),
            self.wrapper.index("[IO.Directory]::CreateDirectory($transactionRoot)"),
        )
        self.assertLess(
            self.wrapper.index("[IO.Directory]::CreateDirectory($transactionRoot)"),
            self.wrapper.index("'prewrite-admission.json'"),
        )
        self.assertLess(
            self.wrapper.index("'prewrite-admission.json'"),
            self.wrapper.index("$sourceJournal = @(New-FileJournal"),
        )
        build = between(
            self.wrapper,
            "$buildArguments = @(",
            "$actorGenerated = Join-Path",
        )
        self.assertLess(
            build.index("$buildAdmission = Assert-LaunchAdmission"),
            build.index("& $buildTool @buildArguments"),
        )
        helper = between(
            self.wrapper, "function Invoke-ColdStage", "function Get-BuildSurfacePaths"
        )
        self.assertLess(
            helper.index('$launchAdmission = Assert-LaunchAdmission "before helper $Stage"'),
            helper.index("Start-Process -FilePath $editor"),
        )

    def test_protected_capstone_and_owned_helper_boundaries_are_fail_closed(self):
        lifecycle = between(
            self.wrapper, "function Get-ProcessRecord", "function Invoke-ColdStage"
        )
        for token in (
            "C:\\Program Files\\Epic Games\\UE_5.4\\Engine\\Binaries\\Win64\\UnrealEditor.exe",
            "C:\\Users\\Lyz\\Desktop\\CAPSTONE\\Capstone.uproject",
            "C:\\Program Files\\Epic Games\\UE_5.5",
            "Assert-ProtectedUnchanged",
            "Get-CimInstance Win32_Process -ErrorAction Stop",
            "StartTimeUtcTicks",
            "A foreign RC owner appeared",
            "Forced containment is permitted only after re-proving PID, creation",
            "$Handle.Kill()",
            "RC listener remained after exact helper containment",
        ):
            self.assertIn(token, self.wrapper)
        for forbidden in (
            "Stop-Process",
            "Remove-Item -Recurse",
            "Remove-Item -Force -Recurse",
            "rm -rf",
            "rmdir /s",
            "git reset --hard",
        ):
            self.assertNotIn(forbidden, self.wrapper)
        self.assertNotIn("-ErrorAction SilentlyContinue", lifecycle)

    def test_all_mutated_surfaces_and_created_directories_are_rollback_owned(self):
        for token in (
            "$r28PromotionDirectoriesBefore = @(Get-R28PromotionDirectoryInventory)",
            "$sourceJournal = @(New-FileJournal",
            "$sourceAssetJournal = @(New-FileJournal",
            "$mapJournal = @(New-FileJournal",
            "$contentJournal = @(New-FileJournal",
            "$buildJournal = @(New-FileJournal",
            "Restore-MapFromJournal $mapJournal[0]",
            "Restore-BuildSurface $buildJournal",
            "Restore-FileJournal $contentJournal",
            "Restore-FileJournal $sourceAssetJournal",
            "Restore-FileJournal $sourceJournal",
            "Restore-R28PromotionDirectories",
            "[IO.Directory]::Delete($path, $false)",
            "ROLLBACK_INCOMPLETE",
        ):
            self.assertIn(token, self.wrapper)
        inventory = between(
            self.wrapper,
            "function Get-R28PromotionDirectoryInventory",
            "function Restore-R28PromotionDirectories",
        )
        for token in (
            "$cursor = $fullRoot",
            "while ($true)",
            "$nativeProjectRoot.TrimEnd('\\')",
            "Test-ContainedPath $cursor $nativeProjectRoot",
            "$parent = [IO.Path]::GetDirectoryName($cursor)",
            "if ($atNativeRoot) { break }",
            "Get-ChildItem -LiteralPath $fullRoot",
        ):
            self.assertIn(token, inventory)
        rollback = self.wrapper[self.wrapper.rindex("finally {") :]
        self.assertLess(
            rollback.index("Restore-MapFromJournal"),
            rollback.index("Restore-R28PromotionDirectories"),
        )

    def test_forced_build_uht_reflection_and_binary_gates_cover_all_endpoints(self):
        build = between(
            self.wrapper,
            "$r28DirectCompileObjectRelativePaths = @(",
            "$stageResults.Add((Invoke-ColdStage `\n        '00_cold_validate_r27_pre_r28'",
        )
        direct_objects = between(
            self.wrapper,
            "$r28DirectCompileObjectRelativePaths = @(",
            "$r28GeneratedCompileObjectRelativePaths = @(",
        )
        self.assertEqual(8, direct_objects.count(".cpp.obj'"))
        for relative in R28_DIRECT_OBJECTS:
            self.assertIn(relative, direct_objects)
        generated_objects = between(
            self.wrapper,
            "$r28GeneratedCompileObjectRelativePaths = @(",
            "function Test-ContainedPath",
        )
        self.assertEqual(7, generated_objects.count(".gen.cpp.obj'"))
        for relative in R28_GENERATED_OBJECTS:
            self.assertIn(relative, generated_objects)
        for token in (
            "'-Module=TRIADSensorFusion'",
            "'-Module=TRIADSensorFusionEditor'",
            "'-ForceHeaderGeneration'",
            "'-NoUBTMakefiles'",
            "'-MaxParallelActions=1'",
            "TRIADIstanaExploreV5DLandmarkVegetationActor.gen.cpp",
            "TRIADIstanaExploreV5DR28EnvironmentActor.gen.cpp",
            "TRIADIstanaExploreV5DContextPolicyActor.gen.cpp",
            "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.gen.cpp",
            "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.gen.cpp",
            "TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.gen.cpp",
            "TRIADIstanaExploreV5DHybridEditorLibrary.gen.cpp",
            "ApplyIstanaExploreV5DR28VisualSuccessorToLoadedHybridMap",
            "ValidateIstanaExploreV5DR28VisualSuccessorMap",
            "ValidateIstanaExploreV5DR28VisualSuccessorPlayWorld",
            "GetIstanaExploreV5DR28VisualSuccessorPlayStateReport",
            "SetIstanaExploreV5DR28VisualSuccessorPlayViewPoseForQa",
            "CaptureIstanaExploreV5DR28VisualSuccessorPlayView",
            "CaptureIstanaExploreV5DR28VisualSuccessorDiagnosticPlayView",
            "BuildOrValidateLandmarkGrassMaterialsR28",
            "ConfigureLandmarkVegetationActorR28",
            "ApplyR28EnvironmentToLoadedV5DHybridMap",
            "ValidateR28EnvironmentInLoadedV5DHybridMap",
            "SetProviderReady",
            "ValidateR28Environment",
            "R28 build log lacks forced direct compile action",
            "Remove-R28GeneratedCompileObjectsForForcedRebuild",
            "R28 forced generated compile-object invalidation",
            "Total of 14 written",
            "$expectedUhtInvocation",
            "$uhtInvocationIndexes.Count -ne 1",
            "$uhtWriteIndexes.Count -ne 1",
            "$uhtCompletionIndexes.Count -ne 1",
            "$linkMatchIndexes = @()",
            "Reflection code generated for UnrealEditor in ",
            "Forced-UHT output is absent or empty",
            "R28 generated compile object was not recreated",
            "R28 build log lacks one ordered exact generated compile action",
            "R28 build log lacks one ordered exact module link action",
            "UnrealEditor-TRIADSensorFusion.dll",
            "UnrealEditor-TRIADSensorFusionEditor.dll",
            "stable generated reflection source read",
            "post-marker generated source",
            "post-marker generated object",
            "post-marker runtime DLL",
            "post-marker editor DLL",
            "Z_Construct_UClass_ATRIADIstanaExploreV5DLandmarkVegetationActor",
            "Z_Construct_UClass_ATRIADIstanaExploreV5DR28EnvironmentActor",
            "Z_Construct_UClass_ATRIADIstanaExploreV5DContextPolicyActor",
            "Z_Construct_UClass_UTRIADIstanaExploreV5DLandmarkVegetationEditorLibrary",
            "Z_Construct_UClass_UTRIADIstanaExploreV5DGroundVegetationEditorLibrary",
            "Z_Construct_UClass_UTRIADIstanaExploreV5DR28EnvironmentEditorLibrary",
            "Z_Construct_UClass_UTRIADIstanaExploreV5DHybridEditorLibrary",
            "execUpgradeGroundVegetationRealismGrassSystemToR23B",
            "execValidateGroundVegetationRealismPassInLoadedV5DHybridMap",
            "byte-identical to its predecessor after forced rebuild",
            "EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_REFUSED_FINAL_MUTATION_GATE",
            "V5D_R23B_DERIVATIVE_MATERIAL_VALID",
        ):
            self.assertIn(token, build)
        outer_ground_authority_marker = (
            "'outerGroundCollisionNavigationShadowDistanceFieldSensorRfTerrainAuthority=false'"
        )
        runtime_binary_gate = between(
            build,
            "Assert-BinaryContainsEncodedMarkers $runtimeDll @(",
            "'Fresh runtime DLL'",
        )
        editor_binary_gate = between(
            build,
            "Assert-BinaryContainsEncodedMarkers $editorDll @(",
            "'Fresh editor DLL'",
        )
        self.assertIn(outer_ground_authority_marker, runtime_binary_gate)
        self.assertNotIn(outer_ground_authority_marker, editor_binary_gate)
        self.assertNotIn("LastWriteTimeUtc -lt", build)
        self.assertNotIn("(Get-Item $buildLog).CreationTimeUtc", build)
        self.assertNotIn("timestamp tolerance", build.lower())
        self.assertNotIn("Forced-UHT output is absent or stale", build)
        self.assertNotIn("$matches += $index", build)
        self.assertIn("GeneratedReflectionProofs", self.wrapper)
        self.assertIn("GeneratedCompileActionCount", self.wrapper)
        self.assertIn("ModuleLinkActionCount", self.wrapper)
        self.assertIn("ForcedGeneratedCompileObjectCount", self.wrapper)

    def test_exact_fourteen_cold_stages_are_ordered_and_no_r27_validator_follows_apply(self):
        positions = [self.wrapper.index(stage) for stage in STAGES]
        self.assertEqual(positions, sorted(positions))
        self.assertEqual(14, self.wrapper.count("$stageResults.Add((Invoke-ColdStage `"))
        successor = self.wrapper[self.wrapper.index(STAGES[7]) :]
        self.assertNotIn(
            "ValidateIstanaExploreV5DLandmarkVegetationR27SuccessorMap", successor
        )
        self.assertNotIn("GetValidatedHybridPlayState", successor)
        for token in (
            "ExpectedPredecessorBytes = [int64] $expectedMapPin.Bytes",
            "ExpectedPredecessorSha256 = [string] $expectedMapPin.Sha256",
            "VerifiedExternalBackupFilename = [string] $mapJournal[0].Backup",
            "EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_PASS",
            "ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_MAP_VALID",
            "ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_MAP_VALID",
            "IDEMPOTENT_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_ALREADY_VALID",
            "'oneSave=true'",
            "'oneSave=false'",
        ):
            self.assertIn(token, successor)

    def test_stage_markers_prove_layout_assets_and_negative_authority(self):
        for marker in (
            "'exactSavedPackages=4'",
            "'exactR27Sources=4'",
            "'completeCanonicalR23BGraph=true'",
            "'exactDerivativeGraph=true'",
            "'minimumNominalCarrierCoverage=0.75'",
            "'priorityBuildingParts=40'",
            "'priorityBuildingCoverageComplete=true'",
            "'baselineBuildingParts=85'",
            "'retainedBaselineBuildingParts=41'",
            "'selectedBuildingParts=128'",
            "'newlySelectedBuildingParts=87'",
            "'architectureTriangles=149758'",
            "'architectureSourceCorners=449274'",
            "'architectureWindows=14786'",
            "'evidenceCameraCount=4'",
            "'priorityStreetscapeCameraCount=2'",
            "'omnidirectionalSectorsMeetingQuota=24'",
            "'architecturePartCeiling=128'",
            "'architectureWindowCeiling=15000'",
            "'architectureTriangleCeiling=150000'",
            "'architectureSourceCornerCeiling=450000'",
            "'publicRoadSegments=1923'",
            "'terrainDrapeSourcePinned=true'",
            "'terrainSamplesResolved=64620'",
            "'terrainSamplesUnresolved=0'",
            "'proceduralMicroSurface=true'",
            "'sidewalkJointCues=true'",
            "'roughnessVariation=true'",
            "'viewDependentGlazingCue=true'",
            "'opaqueGlazing=true'",
            "'glazingTransparencyClaimed=false'",
            "'screenSpaceReflections=false'",
            "'textureInputs=0'",
            "'identity=true'",
            "'mapSavedByThisValidator=false'",
            "'temasekTreeRoster=umbrella,dome,umbrella'",
            "'legacyTemasekBakedFoliageRemoved=true'",
            "'landmarkGrassInstances=6144'",
            "'landmarkGrassMaximumPerSite=4096'",
            "'providerReady=false'",
            "'providerVisualOnly=true'",
            "'providerCollisionNavigationSensorRfAuthority=false'",
            "'r28EnvironmentCollisionNavigationSensorRfTerrainAuthority=false'",
            "'outerGroundCollisionNavigationShadowDistanceFieldSensorRfTerrainAuthority=false'",
            "'providerTokenReadSerializedOrLogged=false'",
            "'providerContentExportedGeometricallyTracedAnalysedDerivedOrBaked=false'",
            "'visualCaptureAccepted=false'",
            "'captureRevalidationRequired=true'",
        ):
            self.assertIn(marker, self.wrapper)

    def test_commit_receipt_has_exact_capture_schema_and_does_not_claim_transition(self):
        commit = between(
            self.wrapper,
            "$commit = [pscustomobject] [ordered] @{",
            "$commitPath = Join-Path $transactionRoot 'commit.json'",
        )
        for token in (
            "Schema = $schema",
            "Status = 'PASS'",
            "PredecessorMap = $expectedMapPin",
            "PredecessorRuntimeDll = $expectedRuntimeDllPin",
            "PredecessorEditorDll = $expectedEditorDllPin",
            "SuccessorMap = $successorPin",
            "SuccessorRuntimeDll = $runtimeAfter",
            "SuccessorEditorDll = $editorAfter",
            "SourceAssetSpecCount = $sourceAssetPins.Count",
            "ExactlyNewAssetCount = $script:r28NewAssetPins.Count",
            "R28GrassPackages = @($script:r28GrassPins)",
            "R28EnvironmentPackages = @($environmentSuccessorPins)",
            "SerializedProviderReady = $false",
            "SerializedLocalFallbackVisible = $true",
            "SerializedProviderCoherenceValidated = $true",
            "ProviderFalseTrueFalseRuntimeTransitionProven = $false",
            "CollisionNavigationSensorRfTerrainAuthority = $false",
            "VisualCaptureAccepted = $false",
            "CaptureRevalidationRequired = $true",
        ):
            self.assertIn(token, commit)
        self.assertNotIn("ProviderFalseTrueFalseRuntimeTransitionProven = $true", self.wrapper)
        self.assertNotIn("R27MaterialPackages", self.wrapper)
        self.assertNotIn("$materialSuccessorPins", self.wrapper)

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
                "contract_r28",
                "-StaticSelfCheck",
            ],
            cwd=REPO,
            capture_output=True,
            text=True,
            timeout=60,
        )
        self.assertEqual(0, check.returncode, check.stdout + check.stderr)
        receipt = json.loads(check.stdout)
        self.assertEqual("STATIC_SELF_CHECK_PASS", receipt["Status"])
        self.assertEqual(16, receipt["CodeSourceCount"])
        self.assertEqual(5, receipt["SourceAssetSpecCount"])
        self.assertEqual(6, receipt["ImmutableAncestorSourceCount"])
        self.assertEqual(8, receipt["DirectCompileObjectCount"])
        self.assertEqual(7, receipt["GeneratedCompileObjectCount"])
        self.assertEqual(17, receipt["ExactlyNewAssetCount"])
        self.assertEqual(4, receipt["R28GrassPackageCount"])
        self.assertEqual(13, receipt["R28EnvironmentPackageCount"])
        self.assertEqual(10_737_418_240, receipt["MinimumSystemFreeVirtualAtLaunchBytes"])
        self.assertFalse(receipt["NativeTreeReadOrWritten"])
        self.assertFalse(receipt["UnrealBuildOrEditorLaunched"])
        self.assertFalse(receipt["CollisionNavigationSensorRfTerrainAuthority"])
        self.assertFalse(receipt["VisualCaptureAccepted"])
        self.assertTrue(receipt["CaptureRevalidationRequired"])


if __name__ == "__main__":
    unittest.main()
