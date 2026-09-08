from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
UNREAL = REPO / "unreal"
SCRIPT = (
    REPO
    / "scripts/Invoke-IstanaExploreV5DVisualQualitySuccessorPromotion.ps1"
)
MANIFEST = (
    REPO
    / "scripts/Promote-IstanaExploreV5DVisualQualitySuccessor.manifest.json"
)
HISTORICAL_PROMOTION_CLOSURE_ROOT = (
    UNREAL
    / "SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/NativeSourceClosure/"
    "VQSP20260905"
)
HISTORICAL_PROMOTION_PROVENANCE = HISTORICAL_PROMOTION_CLOSURE_ROOT.parent / (
    "README.md"
)
HISTORICAL_PROMOTION_OVERRIDE_SOURCES = {
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADIstanaExploreV5DContextPolicyActor.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5DContextPolicyActor.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DHybridEditorLibrary.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DHybridEditorLibrary.cpp",
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/outer_ground_loading_fallback_v1.contract.json",
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.acceptance.lock.json",
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.manifest.json",
    "Plugins/TRIADSensorFusion/Docs/IstanaExploreV5DProviderQuality.md",
}
HISTORICAL_PROMOTION_CLOSURE_FILES = {
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADIstanaExploreV5DContextPolicyActor.h": "00_ContextPolicy.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5DContextPolicyActor.cpp": "01_ContextPolicy.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h": "02_CurrentSurroundingsEditor.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp": "03_CurrentSurroundingsEditor.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DHybridEditorLibrary.h": "04_HybridEditor.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DHybridEditorLibrary.cpp": "05_HybridEditor.cpp",
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/outer_ground_loading_fallback_v1.contract.json": "06_OuterGround.contract.json",
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.acceptance.lock.json": "07_OuterGround.acceptance.lock.json",
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.manifest.json": "08_OuterGround.manifest.json",
    "Plugins/TRIADSensorFusion/Docs/IstanaExploreV5DProviderQuality.md": "09_ProviderQuality.md",
}

EXPECTED_PREDECESSOR_MAP = {
    "path": "Content/Maps/Istana_PublicView_Explore_v5d_hybrid.umap",
    "bytes": 34_992_354,
    "sha256": "859734CB9EFCB429AE7D863E677B7B370CC897EB9C100AACA7AAD6F227805815",
}

EXPECTED_CANONICAL_PARENTS = {
    (
        "Saved/TRIAD/IstanaReferences/V5D/Current20260831/GeneratedCurrent/"
        "SM_IPV5D_OSMCurrentSurroundings_Render.obj"
    ): (
        6_359_246,
        "1612461DBC3FE8C7C760517C0A743B631CDFE23307792AEB8BB04E15855B59A3",
    ),
    (
        "Saved/TRIAD/IstanaReferences/V5D/Current20260831/GeneratedCurrent/"
        "SM_IPV5D_OSMCurrentSurroundings_RFShell.obj"
    ): (
        1_796_000,
        "2B329516E24C081C7773DB984510EBD1E0C87CDB522CB490170B702D5F564324",
    ),
    (
        "Saved/TRIAD/IstanaReferences/V5D/Current20260831/GeneratedCurrent/"
        "SM_IPV5D_OSMCurrentSurroundings_RFShell.mtl"
    ): (
        216,
        "107E25A2EEB7DFF92356CFBF8E1DA99C3329CE66F75CD1AE15A6EDD8D74F1A47",
    ),
}

EXPECTED_SOURCES = {
    # Runtime policy and immutable cooked provenance.
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DContextPolicyActor.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DContextPolicyActor.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5AppearanceActor.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/Tests/"
    "TRIADIstanaExploreV5DLocalFallbackSuppressionV1ProvenanceTests.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DLocalFallbackSuppressionV2Provenance.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/Tests/"
    "TRIADIstanaExploreV5DLocalFallbackSuppressionV2ProvenanceTests.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenance.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/Tests/"
    "TRIADIstanaExploreV5DOuterGroundLoadingFallbackProvenanceTests.cpp",
    # Editor admission, creation, validation, and guarded map endpoint.
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DCurrentSurroundingsSuppressedAssetFactory.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DOuterGroundLoadingFallbackEditorLibrary.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DOuterGroundLoadingFallbackEditorLibrary.cpp",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.h",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.cpp",
    # Public-realm receipt lineage consumed by native Hybrid validation.
    "SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/"
    "istana_public_view_v5d_public_realm.contract.json",
    "SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/"
    "IstanaPublicViewV5DPublicRealm.manifest.json",
    "SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/"
    "IstanaPublicViewV5DPublicRealm.acceptance.lock.json",
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DPublicRealmAssetFactory.cpp",
    # Suppression tool and generated payload.
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/"
    "build_local_fallback_suppression_v1.py",
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/"
    "local_fallback_suppression_v1.contract.json",
    "Generated/IstanaExploreV5D/LocalFallbackSuppressionV1/"
    "SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v1.obj",
    "Generated/IstanaExploreV5D/LocalFallbackSuppressionV1/"
    "SM_IPV5D_OSMCurrentSurroundings_Render.mtl",
    "Generated/IstanaExploreV5D/LocalFallbackSuppressionV1/"
    "IstanaPublicViewV5DLocalFallbackSuppression.v1.metadata.json",
    "Generated/IstanaExploreV5D/LocalFallbackSuppressionV1/"
    "IstanaPublicViewV5DLocalFallbackSuppression.v1.manifest.json",
    # V2 suppression tool, exact generated payload, and focused contracts.
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/"
    "build_local_fallback_suppression_v2.py",
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/"
    "local_fallback_suppression_v2.contract.json",
    "Generated/IstanaExploreV5D/LocalFallbackSuppressionV2/"
    "SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.obj",
    "Generated/IstanaExploreV5D/LocalFallbackSuppressionV2/"
    "SM_IPV5D_OSMCurrentSurroundings_Render.mtl",
    "Generated/IstanaExploreV5D/LocalFallbackSuppressionV2/"
    "IstanaPublicViewV5DLocalFallbackSuppression.v2.metadata.json",
    "Generated/IstanaExploreV5D/LocalFallbackSuppressionV2/"
    "IstanaPublicViewV5DLocalFallbackSuppression.v2.manifest.json",
    "Plugins/TRIADSensorFusion/Tests/"
    "test_istana_explore_v5d_local_fallback_suppression_v2.py",
    "Plugins/TRIADSensorFusion/Tests/"
    "test_istana_explore_v5d_local_fallback_suppression_v2_integration.py",
    "Plugins/TRIADSensorFusion/Tests/"
    "test_istana_explore_v5d_context_policy_successor_contract.py",
    # Outer-ground tool and exact generated package.
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/"
    "build_outer_ground_loading_fallback_v1.py",
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/"
    "outer_ground_loading_fallback_v1.contract.json",
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/"
    "Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.acceptance.lock.json",
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/"
    "Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.audit.json",
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/"
    "Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.manifest.json",
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/"
    "Generated/SM_IPV5D_OuterGroundLoadingFallback_Render.mtl",
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/"
    "Generated/SM_IPV5D_OuterGroundLoadingFallback_Render.obj",
    # Truth-boundary documentation belongs to the promoted slice.
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/"
    "README.md",
    "Plugins/TRIADSensorFusion/Docs/IstanaExploreV5DProviderQuality.md",
}

VERIFIED_NOOP = (
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/"
    "TRIADSensorFusionEditor.Build.cs"
)

SUPPRESSION_ROOT_PAIRS = (
    (
        "Generated/IstanaExploreV5D/LocalFallbackSuppressionV1/",
        "Saved/TRIAD/IstanaReferences/V5D/Current20260831/GeneratedCurrent/"
        "LocalFallbackSuppressionV1/",
    ),
    (
        "Generated/IstanaExploreV5D/LocalFallbackSuppressionV2/",
        "Saved/TRIAD/IstanaReferences/V5D/Current20260831/GeneratedCurrent/"
        "LocalFallbackSuppressionV2/",
    ),
)

EXPECTED_OLD = {
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DLocalFallbackSuppressionV1Provenance.cpp": (
        3_640,
        "D8C6775CE125B989E7326721600577616C4A5B02295A2AA1045530D50C023D35",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DContextPolicyActor.h": (
        8_465,
        "830323932951AE67928388B5FBE2432C2E99582DDB6F9CE053EE9890E9D896F9",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DContextPolicyActor.cpp": (
        79_676,
        "36C70F7333965E89558A035D7A2483C02E95635A6C71D1737CCB6ADF7853413B",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5AppearanceActor.cpp": (
        73_679,
        "4E77FE69F1421DFFCD5F61674EEC6A028A6D472ABD85F21B21BADB3355BA9830",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.h": (
        1_310,
        "AC7E6C6E6B93DEC3F38638B724FFDF0A215CF765B095100F284AC82DF532C984",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DCurrentSurroundingsAssetFactory.cpp": (
        44_868,
        "4FDB6C570AA3EB7A4310821C22B429EE8B9FA339A2A27F144BF37AEEE210A9C2",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DCurrentSurroundingsSuppressedAssetFactory.cpp": (
        56_123,
        "BD089123A0E8A4B0D842E42C70062C73AB40F4104537A50718371A281070E4BF",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h": (
        1_327,
        "0E1EE7F401B80A64EEA0E16A6ADFA7362D10FF150AB8F054ACD29FD179FC4414",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp": (
        7_867,
        "DB6098B01FD648D8B5B389467391CECA1FF2172A64B43399B2217951098D1F68",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DOuterGroundLoadingFallbackAssetFactory.cpp": (
        44_956,
        "1E41C5D6DF08C7BC05873A740A4C06F89841D15905F3FA1F24BEB88BEF6E89CB",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.h": (
        8_245,
        "69B0E31D9A191279195FE982476D8F742DC1805302F3833ED5F146AF0B81674A",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.cpp": (
        351_574,
        "707A6A062078D6B2BE838183EB47BF707A740487D718D78F0289DF7894A148B5",
    ),
    "Plugins/TRIADSensorFusion/Docs/IstanaExploreV5DProviderQuality.md": (
        4_113,
        "84F98839D225CCD2AC06F04CF59974ECD25B8C7E5E802B2EC8127FEA648BBE99",
    ),
    "Plugins/TRIADSensorFusion/Tests/"
    "test_istana_explore_v5d_local_fallback_suppression_v2_integration.py": (
        9_462,
        "5A08A7F26613F8D4BAEF36E70C98210ACDA4EEA0F69500FF695F57A035DEB523",
    ),
    "SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/"
    "istana_public_view_v5d_public_realm.contract.json": (
        17_548,
        "77B437F32F537A0B98C62FBBC25124415C2A7764911D7C89532640494999AB63",
    ),
    "SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/"
    "IstanaPublicViewV5DPublicRealm.manifest.json": (
        21_577,
        "8497C2A9709ECDCE196CD71FD7FC8606D8E8D2BCD47D137E55EEF25092B9CBFD",
    ),
    "SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/"
    "IstanaPublicViewV5DPublicRealm.acceptance.lock.json": (
        7_607,
        "5D826DAE312644593AE4631DB6CD74AA9977A822734D84F8FAC3E884B8D08032",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DPublicRealmAssetFactory.cpp": (
        119_458,
        "70D3A2FD199B7EE50949B5F666D23B8CADAE77890EA30A48E23B45D04BBCC125",
    ),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def resolved_promotion_source(relative: str) -> Path:
    if relative in HISTORICAL_PROMOTION_OVERRIDE_SOURCES:
        return (
            HISTORICAL_PROMOTION_CLOSURE_ROOT
            / HISTORICAL_PROMOTION_CLOSURE_FILES[relative]
        )
    return UNREAL / relative


def powershell_executable() -> str | None:
    return shutil.which("pwsh") or shutil.which("powershell")


def ps_quote(path: Path) -> str:
    return "'" + str(path).replace("'", "''") + "'"


def function_prefix() -> str:
    return SCRIPT.read_text(encoding="utf-8").split(
        "# Read-only preflight begins here.", 1
    )[0]


def run_process_guard_harness(
    root: Path, body: str
) -> subprocess.CompletedProcess[str]:
    executable = powershell_executable()
    if executable is None:
        raise unittest.SkipTest("PowerShell is unavailable on this host.")
    harness = root / "process_guard_harness.ps1"
    harness.write_text(function_prefix() + body, encoding="utf-8")
    return subprocess.run(
        [
            executable,
            "-NoLogo",
            "-NoProfile",
            "-NonInteractive",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(harness),
        ],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )


def run_recovery_harness(fixture: dict[str, Path]) -> subprocess.CompletedProcess[str]:
    executable = powershell_executable()
    if executable is None:
        raise unittest.SkipTest("PowerShell is unavailable on this host.")
    harness = fixture["root"] / "recovery_harness.ps1"
    body = f"""
# Recovery semantics run only against this disposable fixture. Mock the
# low-level CIM enumeration to a stable empty set so recovery exercises the
# real production snapshot guard without coupling to the user's open editors.
function Get-CimInstance {{
    param(
        [string] $ClassName,
        [string] $Filter,
        [string[]] $Property,
        [object] $ErrorAction
    )
    return @()
}}
$script:ResolvedNativeRoot = {ps_quote(fixture['native'])}
Initialize-ReviewedUnrealEditorProcessSnapshot `
    -NativeRoot $script:ResolvedNativeRoot
$manifest = Get-Content -LiteralPath {ps_quote(fixture['manifest'])} -Raw | ConvertFrom-Json
$manifestReceipt = Get-FileReceipt -Path {ps_quote(fixture['manifest'])}
$result = Invoke-DurableRecovery `
    -TransactionRoot {ps_quote(fixture['transaction'])} `
    -BackupBase {ps_quote(fixture['backup_base'])} `
    -Manifest $manifest -ManifestReceipt $manifestReceipt `
    -WorkspaceRoot {ps_quote(fixture['workspace'])} `
    -NativeRoot {ps_quote(fixture['native'])}
$pendingAfter = @(Get-PendingTransactionRoots `
    -BackupBase {ps_quote(fixture['backup_base'])} `
    -NativeRoot {ps_quote(fixture['native'])})
if ($pendingAfter.Count -ne 0) {{
    throw "Recovered transaction still appears pending."
}}
$result | Add-Member -NotePropertyName PendingAfterRecovery `
    -NotePropertyValue $pendingAfter.Count
$result | ConvertTo-Json -Compress
"""
    harness.write_text(function_prefix() + body, encoding="utf-8")
    return subprocess.run(
        [
            executable,
            "-NoLogo",
            "-NoProfile",
            "-NonInteractive",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(harness),
        ],
        cwd=fixture["root"],
        text=True,
        capture_output=True,
        check=False,
    )


def build_recovery_fixture(
    root: Path,
    live_bytes: bytes,
    *,
    inject_reparse_parent: bool = False,
    transaction_published: bool = True,
) -> dict[str, Path]:
    workspace = root / "workspace"
    native = root / "native"
    backup_base = native / "backups"
    transaction_id = "20260905T120000000Z-p1234-abcdef12"
    transaction = backup_base / transaction_id
    destination_parent = native / "Plugin"
    destination = destination_parent / "file.txt"
    source = workspace / "Plugin/file.txt"
    old_bytes = b"reviewed-old-predecessor\n"
    target_bytes = b"reviewed-promoted-target\n"

    source.parent.mkdir(parents=True)
    source.write_bytes(target_bytes)
    (native / "Content/Maps").mkdir(parents=True)
    predecessor_map = native / "Content/Maps/predecessor.umap"
    predecessor_map.write_bytes(b"exact-predecessor-map\n")
    (transaction / "native_before").mkdir(parents=True)
    backup = transaction / "native_before/000.predecessor"
    backup.write_bytes(old_bytes)
    displaced = transaction / "native_before/000.predecessor.displaced"

    if inject_reparse_parent:
        outside = root / "outside"
        outside.mkdir()
        (outside / "file.txt").write_bytes(live_bytes)
        try:
            os.symlink(outside, destination_parent, target_is_directory=True)
        except OSError:
            executable = powershell_executable()
            if executable is None:
                raise unittest.SkipTest("Cannot create a reparse fixture.")
            created = subprocess.run(
                [
                    executable,
                    "-NoLogo",
                    "-NoProfile",
                    "-NonInteractive",
                    "-Command",
                    "New-Item -ItemType Junction -Path "
                    + ps_quote(destination_parent)
                    + " -Target "
                    + ps_quote(outside)
                    + " | Out-Null",
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            if created.returncode != 0:
                raise unittest.SkipTest(
                    f"Cannot create a reparse fixture: {created.stderr}"
                )
    else:
        destination_parent.mkdir(parents=True)
        destination.write_bytes(live_bytes)

    manifest_value = {
        "predecessorMap": {
            "path": "Content/Maps/predecessor.umap",
            "bytes": predecessor_map.stat().st_size,
            "sha256": sha256(predecessor_map),
        },
        "canonicalParentSavedInputs": [],
        "files": [
            {
                "area": "runtime",
                "source": "Plugin/file.txt",
                "destination": "Plugin/file.txt",
                "bytes": len(target_bytes),
                "sha256": hashlib.sha256(target_bytes).hexdigest().upper(),
                "destinationPolicy": "exact-old-hash-backup-and-replace",
                "expectedOld": {
                    "bytes": len(old_bytes),
                    "sha256": hashlib.sha256(old_bytes).hexdigest().upper(),
                },
            }
        ],
    }
    fixture_manifest = root / "fixture_manifest.json"
    fixture_manifest.write_text(json.dumps(manifest_value, indent=2), encoding="utf-8")

    stage = destination_parent / (
        f".triad-v5d-{transaction_id}-000-publish.tmp"
    )
    if transaction_published:
        displaced.write_bytes(old_bytes)
    elif not inject_reparse_parent:
        stage.write_bytes(target_bytes)
    journal_value = {
        "Schema": (
            "triad.istana_explore_v5d.visual_quality_successor_transaction.v1"
        ),
        "Version": 1,
        "State": "PREPARED",
        "TransactionId": transaction_id,
        "PromotionId": (
            "istana_explore_v5d_visual_quality_successor_2026-09-05"
        ),
        "CreatedUtc": "2026-09-05T12:00:00.0000000Z",
        "WorkspaceUnrealRoot": str(workspace.resolve()),
        "NativeProjectRoot": str(native.resolve()),
        "ManifestBytes": fixture_manifest.stat().st_size,
        "ManifestSha256": sha256(fixture_manifest),
        "Rows": [
            {
                "Ordinal": 0,
                "Area": "runtime",
                "RelativeSource": "Plugin/file.txt",
                "RelativeDestination": "Plugin/file.txt",
                "Action": "replace",
                "BeforeExists": True,
                "BeforeBytes": len(old_bytes),
                "BeforeSha256": hashlib.sha256(old_bytes).hexdigest().upper(),
                "TargetBytes": len(target_bytes),
                "TargetSha256": hashlib.sha256(target_bytes).hexdigest().upper(),
                "StageRelativePath": stage.relative_to(native).as_posix(),
                "BackupRelativePath": backup.relative_to(native).as_posix(),
                "DisplacedRelativePath": displaced.relative_to(native).as_posix(),
            }
        ],
    }
    (transaction / "prepared.json").write_text(
        json.dumps(journal_value, indent=2), encoding="utf-8"
    )
    (root / "old.expected").write_bytes(old_bytes)
    (root / "target.expected").write_bytes(target_bytes)
    return {
        "root": root,
        "workspace": workspace,
        "native": native,
        "backup_base": backup_base,
        "transaction": transaction,
        "destination": destination,
        "manifest": fixture_manifest,
        "old": root / "old.expected",
        "target": root / "target.expected",
    }


def add_second_published_replacement(fixture: dict[str, Path]) -> Path:
    target_bytes = b"second-reviewed-promoted-target\n"
    old_bytes = b"second-reviewed-old-predecessor\n"
    relative = "Plugin/file2.txt"
    source = fixture["workspace"] / relative
    destination = fixture["native"] / relative
    source.write_bytes(target_bytes)
    destination.write_bytes(target_bytes)

    manifest_value = json.loads(fixture["manifest"].read_text(encoding="utf-8"))
    manifest_value["files"].append(
        {
            "area": "runtime",
            "source": relative,
            "destination": relative,
            "bytes": len(target_bytes),
            "sha256": hashlib.sha256(target_bytes).hexdigest().upper(),
            "destinationPolicy": "exact-old-hash-backup-and-replace",
            "expectedOld": {
                "bytes": len(old_bytes),
                "sha256": hashlib.sha256(old_bytes).hexdigest().upper(),
            },
        }
    )
    fixture["manifest"].write_text(
        json.dumps(manifest_value, indent=2), encoding="utf-8"
    )

    backup = fixture["transaction"] / "native_before/001.predecessor"
    backup.write_bytes(old_bytes)
    displaced = backup.with_name(backup.name + ".displaced")
    displaced.write_bytes(old_bytes)
    transaction_id = fixture["transaction"].name
    stage = destination.with_name(
        f".triad-v5d-{transaction_id}-001-publish.tmp"
    )
    journal_path = fixture["transaction"] / "prepared.json"
    journal_value = json.loads(journal_path.read_text(encoding="utf-8"))
    journal_value["ManifestBytes"] = fixture["manifest"].stat().st_size
    journal_value["ManifestSha256"] = sha256(fixture["manifest"])
    journal_value["Rows"].append(
        {
            "Ordinal": 1,
            "Area": "runtime",
            "RelativeSource": relative,
            "RelativeDestination": relative,
            "Action": "replace",
            "BeforeExists": True,
            "BeforeBytes": len(old_bytes),
            "BeforeSha256": hashlib.sha256(old_bytes).hexdigest().upper(),
            "TargetBytes": len(target_bytes),
            "TargetSha256": hashlib.sha256(target_bytes).hexdigest().upper(),
            "StageRelativePath": stage.relative_to(fixture["native"]).as_posix(),
            "BackupRelativePath": backup.relative_to(fixture["native"]).as_posix(),
            "DisplacedRelativePath": displaced.relative_to(
                fixture["native"]
            ).as_posix(),
        }
    )
    journal_path.write_text(json.dumps(journal_value, indent=2), encoding="utf-8")
    return destination


class IstanaExploreV5DSuccessorPromotionContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.script = SCRIPT.read_text(encoding="utf-8")
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def test_authoritative_manifest_has_the_exact_closed_roster(self) -> None:
        self.assertEqual(
            set(self.manifest),
            {
                "schema",
                "version",
                "promotionId",
                "predecessorMap",
                "canonicalParentSavedInputs",
                "verifiedNoopFiles",
                "files",
            },
        )
        self.assertEqual(
            self.manifest["schema"],
            "triad.istana_explore_v5d.visual_quality_successor_promotion.v1",
        )
        self.assertEqual(self.manifest["version"], 1)
        self.assertEqual(
            self.manifest["promotionId"],
            "istana_explore_v5d_visual_quality_successor_2026-09-05",
        )
        self.assertEqual(self.manifest["predecessorMap"], EXPECTED_PREDECESSOR_MAP)

        parents = {
            row["path"]: (row["bytes"], row["sha256"])
            for row in self.manifest["canonicalParentSavedInputs"]
        }
        self.assertEqual(parents, EXPECTED_CANONICAL_PARENTS)

        rows = self.manifest["files"]
        self.assertEqual(len(rows), 51)
        self.assertEqual({row["source"] for row in rows}, EXPECTED_SOURCES)
        self.assertEqual(len({row["destination"] for row in rows}), 51)
        for row in rows:
            expected_destination = row["source"]
            for source_root, destination_root in SUPPRESSION_ROOT_PAIRS:
                if row["source"].startswith(source_root):
                    expected_destination = destination_root + row[
                        "source"
                    ].removeprefix(source_root)
                    break
            self.assertEqual(row["destination"], expected_destination)
            self.assertEqual(
                set(row),
                {
                    "area",
                    "source",
                    "destination",
                    "bytes",
                    "sha256",
                    "destinationPolicy",
                    "expectedOld",
                },
            )

        noops = self.manifest["verifiedNoopFiles"]
        self.assertEqual(
            noops,
            [
                {
                    "area": "editor",
                    "source": VERIFIED_NOOP,
                    "destination": VERIFIED_NOOP,
                    "bytes": 1229,
                    "sha256": (
                        "B7D10EA034A37CB15C9939DADFA1910B229ABA14098F096E21994F7A6D977C1B"
                    ),
                }
            ],
        )

    def test_every_resolved_source_and_manifest_receipt_is_exact(self) -> None:
        for row in self.manifest["files"] + self.manifest["verifiedNoopFiles"]:
            path = resolved_promotion_source(row["source"])
            self.assertTrue(path.is_file(), path)
            self.assertEqual(path.stat().st_size, row["bytes"], path)
            self.assertEqual(sha256(path), row["sha256"], path)

        manifest_bytes = MANIFEST.stat().st_size
        manifest_sha = sha256(MANIFEST)
        self.assertIn(f"$ExpectedManifestBytes = {manifest_bytes}", self.script)
        self.assertIn(
            f"$ExpectedManifestSha256 = '{manifest_sha}'", self.script
        )
        self.assertIn("$ExpectedFileCount = 51", self.script)
        self.assertIn("$ExpectedVerifiedNoopCount = 1", self.script)
        self.assertIn("Assert-ExactProperties", self.script)

    def test_historical_override_closure_is_exact_and_isolated_from_live(self) -> None:
        closure_files = {
            path.relative_to(HISTORICAL_PROMOTION_CLOSURE_ROOT).as_posix()
            for path in HISTORICAL_PROMOTION_CLOSURE_ROOT.rglob("*")
            if path.is_file()
        }
        self.assertEqual(
            closure_files, set(HISTORICAL_PROMOTION_CLOSURE_FILES.values())
        )
        self.assertEqual(
            set(HISTORICAL_PROMOTION_CLOSURE_FILES),
            HISTORICAL_PROMOTION_OVERRIDE_SOURCES,
        )
        manifest_rows = {
            row["source"]: row for row in self.manifest["files"]
        }
        provenance = HISTORICAL_PROMOTION_PROVENANCE.read_text(
            encoding="utf-8"
        )
        for canonical_source in HISTORICAL_PROMOTION_OVERRIDE_SOURCES:
            row = manifest_rows[canonical_source]
            closure_file = (
                HISTORICAL_PROMOTION_CLOSURE_ROOT
                / HISTORICAL_PROMOTION_CLOSURE_FILES[canonical_source]
            )
            live_file = UNREAL / canonical_source
            self.assertEqual(row["destination"], canonical_source)
            self.assertEqual(
                (closure_file.stat().st_size, sha256(closure_file)),
                (row["bytes"], row["sha256"]),
            )
            self.assertNotEqual(
                closure_file.read_bytes(), live_file.read_bytes()
            )
            self.assertNotIn("NativeSourceClosure", row["source"])
            self.assertNotIn("NativeSourceClosure", row["destination"])
            self.assertIn(row["sha256"], provenance)
            self.assertIn(sha256(live_file), provenance)

        for token in (
            "4268ED027D07B31CAC896AA8F4268038B227FE8E3AEC9268A7119A2621572AB4",
            "source_before/04.predecessor",
            "source_displaced/04.predecessor",
            "source_before/05.predecessor",
            "source_displaced/05.predecessor",
            "r25_native_20260905T2022Z",
            "r25_sm6_native_20260905T2045Z",
            "r25_forceuht_native_20260905T2053Z",
        ):
            self.assertIn(token, provenance)

    def test_destination_policies_pin_only_exact_old_or_absent(self) -> None:
        rows = {row["destination"]: row for row in self.manifest["files"]}
        self.assertEqual(
            {
                path
                for path, row in rows.items()
                if row["destinationPolicy"]
                == "exact-old-hash-backup-and-replace"
            },
            set(EXPECTED_OLD),
        )
        for path, row in rows.items():
            if path in EXPECTED_OLD:
                expected_bytes, expected_sha = EXPECTED_OLD[path]
                self.assertEqual(
                    row["expectedOld"],
                    {"bytes": expected_bytes, "sha256": expected_sha},
                )
            else:
                self.assertEqual(row["destinationPolicy"], "must-be-missing")
                self.assertIsNone(row["expectedOld"])

        self.assertIn(
            "Unexpected existing create-only destination; refusing overwrite",
            self.script,
        )
        self.assertIn("native exact-old predecessor", self.script)
        self.assertIn("$alreadyPromoted", self.script)
        self.assertNotIn("destinationPolicy", str(self.manifest["verifiedNoopFiles"]))

    def test_preflight_is_read_only_and_apply_is_explicit(self) -> None:
        preflight_marker = "# Read-only preflight begins here."
        write_marker = "# Native write boundary."
        self.assertIn(preflight_marker, self.script)
        self.assertIn(write_marker, self.script)
        preflight = self.script.split(preflight_marker, 1)[1].split(
            write_marker, 1
        )[0]
        self.assertIn("if (-not $Apply)", preflight)
        self.assertGreaterEqual(
            preflight.count("Assert-UnrealEditorProcessSnapshotUnchanged"), 2
        )
        self.assertIn("Assert-ProtectedNativeInputs", preflight)
        self.assertIn("Assert-PlanStable -Rows $allRows", preflight)
        for command in ("New-Item", "Copy-Item", "Move-Item", "Remove-Item"):
            self.assertIsNone(
                re.search(rf"(?m)^\s*{re.escape(command)}\b", preflight),
                command,
            )

        snapshot = re.search(
            r"function Get-ReviewedUnrealEditorProcessSnapshot "
            r"\{(?P<body>.*?)\n\}",
            self.script,
            re.DOTALL,
        )
        self.assertIsNotNone(snapshot)
        assert snapshot is not None
        snapshot_body = snapshot.group("body")
        self.assertIn("Get-CimInstance -ClassName Win32_Process", snapshot_body)
        for field in (
            "ProcessId",
            "Name",
            "ExecutablePath",
            "CreationDate",
            "CommandLine",
        ):
            self.assertIn(field, snapshot_body)
        self.assertIn("$resolvedNativeRoot", snapshot_body)
        self.assertIn("RegexOptions]::IgnoreCase", snapshot_body)
        self.assertNotIn("SilentlyContinue", snapshot_body)
        self.assertNotIn("6860", self.script)
        self.assertNotIn("CAPSTONE", self.script)
        self.assertNotIn("Assert-NoUnrealEditorProcess", self.script)
        checkpoints = re.findall(
            r"(?m)^\s*Assert-UnrealEditorProcessSnapshotUnchanged\s*$",
            self.script,
        )
        self.assertEqual(len(checkpoints), 10)
        self.assertEqual(
            self.script.count("Initialize-ReviewedUnrealEditorProcessSnapshot"),
            2,
        )
        self.assertIn("UnrelatedUnrealEditorProcesses", preflight)

    def test_process_guard_allows_only_an_unchanged_unrelated_snapshot(self) -> None:
        body = r'''
$script:MockCimFailure = $false
$script:MockProcesses = @(
    [pscustomobject] @{
        ProcessId = [uint32] 41001
        Name = 'UnrealEditor.exe'
        ExecutablePath = 'C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe'
        CreationDate = [datetime] '2026-09-06T08:00:00Z'
        CommandLine = '"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Unrelated\Example.uproject"'
    }
)
function Get-CimInstance {
    param(
        [string] $ClassName,
        [string] $Filter,
        [string[]] $Property,
        [object] $ErrorAction
    )
    if ($script:MockCimFailure) {
        throw 'synthetic CIM enumeration failure'
    }
    return @($script:MockProcesses)
}
$script:ResolvedNativeRoot = 'D:\triad\TRIAD'
Initialize-ReviewedUnrealEditorProcessSnapshot `
    -NativeRoot $script:ResolvedNativeRoot
Assert-UnrealEditorProcessSnapshotUnchanged
$identity = @($script:ReviewedUnrealEditorProcessSnapshot)[0]
if ($identity.ProcessId -ne 41001 -or
    $identity.Name -cne 'UnrealEditor.exe' -or
    [string]::IsNullOrWhiteSpace($identity.ExecutablePath) -or
    [string]::IsNullOrWhiteSpace($identity.CreationUtc) -or
    [string]::IsNullOrWhiteSpace($identity.CommandLine)) {
    throw 'The unrelated-editor snapshot omitted an exact identity field.'
}
$script:MockProcesses[0].CommandLine += ' -identity-drift'
try {
    Assert-UnrealEditorProcessSnapshotUnchanged
    throw 'Expected identity drift refusal was not raised.'
}
catch {
    if ($_.Exception.Message -cnotlike '*identity changed after preflight*') {
        throw
    }
}
$script:UnrealEditorProcessSnapshotInitialized = $false
$script:ReviewedUnrealEditorProcessSnapshot = @()
$script:MockProcesses = @(
    [pscustomobject] @{
        ProcessId = [uint32] 41003
        Name = 'UnrealEditor.exe'
        ExecutablePath = 'C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe'
        CreationDate = [datetime] '2026-09-06T08:00:30Z'
        CommandLine = '"C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe" "D:\triad\TRIAD_old\Unrelated.uproject"'
    }
)
Initialize-ReviewedUnrealEditorProcessSnapshot `
    -NativeRoot $script:ResolvedNativeRoot
Assert-UnrealEditorProcessSnapshotUnchanged
$script:UnrealEditorProcessSnapshotInitialized = $false
$script:ReviewedUnrealEditorProcessSnapshot = @()
$script:MockProcesses = @(
    [pscustomobject] @{
        ProcessId = [uint32] 41002
        Name = 'UnrealEditor-Cmd.exe'
        ExecutablePath = 'C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
        CreationDate = [datetime] '2026-09-06T08:01:00Z'
        CommandLine = '"C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "d:/TRIAD/triad/TRIAD.uproject" -run=pythonscript'
    }
)
try {
    Initialize-ReviewedUnrealEditorProcessSnapshot `
        -NativeRoot $script:ResolvedNativeRoot
    throw 'Expected native-project editor refusal was not raised.'
}
catch {
    if ($_.Exception.Message -cnotlike '*Native TRIAD Unreal editor/helper*') {
        throw
    }
}
$script:UnrealEditorProcessSnapshotInitialized = $false
$script:ReviewedUnrealEditorProcessSnapshot = @()
$script:MockProcesses = @()
$script:MockCimFailure = $true
try {
    Initialize-ReviewedUnrealEditorProcessSnapshot `
        -NativeRoot $script:ResolvedNativeRoot
    throw 'Expected fail-closed CIM refusal was not raised.'
}
catch {
    if ($_.Exception.Message -cnotlike '*synthetic CIM enumeration failure*') {
        throw
    }
}
'PROCESS_GUARD_PASS'
'''
        with tempfile.TemporaryDirectory() as temporary:
            result = run_process_guard_harness(Path(temporary), body)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("PROCESS_GUARD_PASS", result.stdout)

    def test_apply_stages_backs_up_publishes_and_rehashes_in_order(self) -> None:
        write_section = self.script.split("# Native write boundary.", 1)[1]
        stage = write_section.index("# Stage every source beside its destination")
        backup = write_section.index("# Authenticate every differing predecessor")
        journal = write_section.index(
            "# This flushed, atomically published journal is the durable ownership boundary."
        )
        publish = write_section.index("[IO.File]::Move($row.StagedPath")
        final = write_section.index("'final promoted destination'")
        commit = write_section.index("Write-TransactionCloseReceipt -Plan")
        self.assertLess(stage, backup)
        self.assertLess(backup, journal)
        self.assertLess(journal, publish)
        self.assertLess(publish, final)
        self.assertLess(final, commit)

        self.assertIn("[IO.File]::Copy($Source, $Destination, $false)", self.script)
        self.assertIn("[IO.FileMode]::CreateNew", self.script)
        self.assertIn("[IO.FileOptions]::WriteThrough", self.script)
        self.assertIn("$stream.Flush($true)", self.script)
        self.assertIn("'prepared.json'", self.script)
        self.assertIn("[IO.File]::Move($temporary, $fullPath)", self.script)
        self.assertIn("[IO.File]::Replace(", write_section)
        self.assertIn("Assert-SameReceipt -Expected $row.TargetReceipt", write_section)
        self.assertIn("EveryDestinationRehashed", write_section)
        self.assertNotIn("$published", self.script)
        self.assertNotIn("Copy-Item", self.script)
        self.assertNotIn("Remove-Item", self.script)

    def test_durable_recovery_authenticates_ownership_and_paths(self) -> None:
        body = self.script.split("function Invoke-DurableRecovery {", 1)[1].split(
            "# Read-only preflight begins here.", 1
        )[0]
        self.assertIn(
            "for ($index = $rows.Count - 1; $index -ge 0; --$index)",
            body,
        )
        self.assertIn("Read-PreparedTransactionPlan", body)
        self.assertIn("Recovery lost transaction ownership; refusing overwrite", body)
        self.assertIn("Assert-ExistingNonReparseDirectory", body)
        self.assertIn("Assert-NoReparseAncestor", body)
        self.assertIn("[IO.File]::Replace(", body)
        self.assertIn("[IO.File]::Move($row.Destination", body)
        self.assertIn("-Outcome 'RECOVERED'", body)
        close_body = self.script.split(
            "function Write-TransactionCloseReceipt {", 1
        )[1].split("function Invoke-DurableRecovery {", 1)[0]
        self.assertIn(
            "Assert-SameReceipt -Expected $Plan.JournalReceipt", close_body
        )
        self.assertIn("-Path $Plan.JournalPath", close_body)

        runtime = self.script.split("# Read-only preflight begins here.", 1)[1]
        pending = runtime.index("Get-PendingTransactionRoots")
        recovery = runtime.index("Invoke-DurableRecovery")
        plan = runtime.index("New-PromotionPlan")
        self.assertLess(pending, recovery)
        self.assertLess(recovery, plan)
        self.assertIn(
            r"'^\d{8}T\d{9}Z-p\d+-[0-9a-f]{8}$'", self.script
        )

        lowered = self.script.lower()
        for forbidden in (
            "robocopy",
            "xcopy",
            "cmd /c",
            "start-process",
            "invoke-expression",
        ):
            self.assertNotIn(forbidden, lowered)
        self.assertNotRegex(self.script, r"(?im)^\s*(?:&\s*)?cmd(?:\.exe)?\s")
        self.assertNotRegex(self.script, r"(?i)Remove-Item[^\r\n]*-Recurse")

    def test_recovery_restores_an_interrupted_atomic_replacement(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            fixture = build_recovery_fixture(
                Path(temporary), b"reviewed-promoted-target\n"
            )
            result = run_recovery_harness(fixture)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(
                fixture["destination"].read_bytes(),
                fixture["old"].read_bytes(),
            )
            recovered = fixture["transaction"] / "recovered.json"
            self.assertTrue(recovered.is_file())
            self.assertFalse((fixture["transaction"] / "committed.json").exists())
            payload = json.loads(result.stdout)
            self.assertEqual(payload["Status"], "RECOVERY_PASS")
            self.assertEqual(payload["PendingAfterRecovery"], 0)

    def test_recovery_refuses_torn_mixed_replacement_without_writing(self) -> None:
        mixed = b"reviewed-promoted-predecessor\n"
        with tempfile.TemporaryDirectory() as temporary:
            fixture = build_recovery_fixture(Path(temporary), mixed)
            before = fixture["destination"].read_bytes()
            result = run_recovery_harness(fixture)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "Recovery refuses foreign or mixed destination bytes",
                result.stderr,
            )
            self.assertEqual(fixture["destination"].read_bytes(), before)
            self.assertFalse((fixture["transaction"] / "recovered.json").exists())

    def test_recovery_refuses_foreign_post_publish_edit_without_writing(self) -> None:
        foreign = b"operator-edited-after-publish\n"
        with tempfile.TemporaryDirectory() as temporary:
            fixture = build_recovery_fixture(Path(temporary), foreign)
            result = run_recovery_harness(fixture)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "Recovery refuses foreign or mixed destination bytes",
                result.stderr,
            )
            self.assertEqual(fixture["destination"].read_bytes(), foreign)
            self.assertFalse((fixture["transaction"] / "recovered.json").exists())

    def test_foreign_row_blocks_all_recovery_writes_during_full_preflight(self) -> None:
        foreign = b"operator-edited-after-publish\n"
        with tempfile.TemporaryDirectory() as temporary:
            fixture = build_recovery_fixture(Path(temporary), foreign)
            eligible_later_row = add_second_published_replacement(fixture)
            eligible_before = eligible_later_row.read_bytes()
            result = run_recovery_harness(fixture)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "Recovery refuses foreign or mixed destination bytes",
                result.stderr,
            )
            self.assertEqual(eligible_later_row.read_bytes(), eligible_before)
            self.assertEqual(fixture["destination"].read_bytes(), foreign)
            self.assertFalse((fixture["transaction"] / "recovered.json").exists())

    def test_recovery_refuses_identical_target_without_publish_ownership(self) -> None:
        target = b"reviewed-promoted-target\n"
        with tempfile.TemporaryDirectory() as temporary:
            fixture = build_recovery_fixture(
                Path(temporary), target, transaction_published=False
            )
            result = run_recovery_harness(fixture)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "Target bytes lack transaction-owned publication evidence",
                result.stderr,
            )
            self.assertEqual(fixture["destination"].read_bytes(), target)
            self.assertTrue(
                fixture["destination"].with_name(
                    ".triad-v5d-20260905T120000000Z-p1234-abcdef12-000-publish.tmp"
                ).is_file()
            )
            self.assertFalse((fixture["transaction"] / "recovered.json").exists())

    def test_recovery_refuses_reparse_ancestor_without_outside_write(self) -> None:
        target = b"reviewed-promoted-target\n"
        with tempfile.TemporaryDirectory() as temporary:
            fixture = build_recovery_fixture(
                Path(temporary), target, inject_reparse_parent=True
            )
            outside_destination = Path(temporary) / "outside/file.txt"
            before = outside_destination.read_bytes()
            result = run_recovery_harness(fixture)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("reparse ancestor", result.stderr.lower())
            self.assertEqual(outside_destination.read_bytes(), before)
            self.assertFalse((fixture["transaction"] / "recovered.json").exists())


if __name__ == "__main__":
    unittest.main()
