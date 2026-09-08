from __future__ import annotations

import hashlib
import json
import re
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
UNREAL = REPO / "unreal"
HELPER = (
    REPO
    / "scripts/New-IstanaExploreV5DVisualQualityPromotionInputSnapshot.ps1"
)
MANIFEST = (
    REPO
    / "scripts/Promote-IstanaExploreV5DVisualQualitySuccessor.manifest.json"
)
PRODUCTION_PROMOTION = (
    REPO
    / "scripts/Invoke-IstanaExploreV5DVisualQualitySuccessorPromotion.ps1"
)
SOURCE_ASSETS_PREFIX = "SourceAssets/"
PHYSICAL_SOURCE_ASSETS = {
    (
        "SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/"
        "istana_public_view_v5d_public_realm.contract.json"
    ),
    (
        "SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/"
        "IstanaPublicViewV5DPublicRealm.manifest.json"
    ),
    (
        "SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/Generated/"
        "IstanaPublicViewV5DPublicRealm.acceptance.lock.json"
    ),
}
HISTORICAL_PROMOTION_CLOSURE_RELATIVE_ROOT = (
    "IstanaPublicViewExploreV5D/PublicRealm/NativeSourceClosure/"
    "VQSP20260905"
)
HISTORICAL_PROMOTION_CLOSURE_ROOT = (
    UNREAL / "SourceAssets" / HISTORICAL_PROMOTION_CLOSURE_RELATIVE_ROOT
)
HISTORICAL_PROMOTION_OVERRIDES = {
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADIstanaExploreV5DContextPolicyActor.h": (
        8_694,
        "88A4000BA015308E50C909B333E8337649409179165138E092CA8DE63393C981",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5DContextPolicyActor.cpp": (
        86_305,
        "75A753072A8914D505314A6AF395AFAD89AA919A0E1C8B6253EEF67B072338D1",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h": (
        1_722,
        "E752E5767ABBBA900E2957231D8E1FE3CD4E03AAE8180FF08EE85346DE06243F",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp": (
        12_313,
        "2E52D294F856C00B17ACAE0299447CBC11BA4E423F3D37A25CCC7E2C8300923D",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DHybridEditorLibrary.h": (
        9_021,
        "950ACEFDAF9EB1B3B21C535DB7E5482C217F1D72C23FF9995CAC9DA1758DFE47",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DHybridEditorLibrary.cpp": (
        377_184,
        "9490D09F9FC453777F5187C2440F144F01227B5D7E9F079B22D710B403B4F75D",
    ),
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/outer_ground_loading_fallback_v1.contract.json": (
        9_600,
        "8815790E9BC0D7E4149DF67759F73A9F98738EACED490984B0F5519B883DDE83",
    ),
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.acceptance.lock.json": (
        3_478,
        "736785C3AEB8D7D2E2DDC4943314F450DD2DD548C6010E9FBC51D0CEF3C1EBDE",
    ),
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.manifest.json": (
        11_710,
        "9A9A12D127ED34D227F909F06DB55804B741F9ABBB22814BEF82DDC2301FB959",
    ),
    "Plugins/TRIADSensorFusion/Docs/IstanaExploreV5DProviderQuality.md": (
        4_661,
        "79A38816CAE908B21434F780DE883331D703BE5236E66729B0D8DBB1C903FB00",
    ),
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
LIVE_PROMOTION_OVERRIDE_RECEIPTS = {
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADIstanaExploreV5DContextPolicyActor.h": (
        13_025,
        "674134B2404364384A5760012114F19C19CCCF16A6E5D9552E6A4D20C396E875",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5DContextPolicyActor.cpp": (
        156_491,
        "7A75941BBD020CBCA68E49C86B5234748EBF96BCACB615BB4C14B6459E904AB7",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.h": (
        2_490,
        "160EDC5EDBE67AF6D48B3FA34C7C9998B5F693EE4186C9D97E06F11A7B42C9C0",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DCurrentSurroundingsEditorLibrary.cpp": (
        16_747,
        "289C996F67ADD3FBAD26073083BDC83F8D1CB2117E316A94E3D5F5E6686EC06E",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DHybridEditorLibrary.h": (
        14_928,
        "52DE5BE0B7228669242C2620DFF2191004AE630520C9073729E3FA9543B26F01",
    ),
    "Plugins/TRIADSensorFusion/Source/TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DHybridEditorLibrary.cpp": (
        505_613,
        "FACFF7B8757DF68B824937CA04AE79F4B2B2A55D08B32711424E717BD0FE7DCD",
    ),
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/outer_ground_loading_fallback_v1.contract.json": (
        9_600,
        "595DB092F24565FAA8B1850F3363F9C33FB3C6FD285705A501C41B7A34DFCAB6",
    ),
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.acceptance.lock.json": (
        3_478,
        "8C25E905EA482BCC6260F70F30B01A65B301E50F62E368246B4809E04CDF9E65",
    ),
    "Plugins/TRIADSensorFusion/Tools/IstanaExploreV5D/OuterGroundFallback/Generated/IstanaPublicViewV5DOuterGroundLoadingFallback.manifest.json": (
        11_710,
        "1ADB5810820B75B51C5E8DE4E5CD4A181160B0CA9BBD8C75B21A7354B9532320",
    ),
    "Plugins/TRIADSensorFusion/Docs/IstanaExploreV5DProviderQuality.md": (
        17_285,
        "3DC44C5F440EC31DF3ABDEF5E37B463C1719DDCEE2F101C75B3CA1986FF648AC",
    ),
}
PRODUCTION_PROMOTION_RECEIPT = (
    85_907,
    "7D9C2CDD1295921B3D0B007954B9BB0330A932974028ECB2EAC40BCBF666C9AB",
)
MANIFEST_RECEIPT = (
    29_317,
    "9DF232B3724A43C621DFAD03E611D6DC5B5C779135577CC1DDA375B293004072",
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def powershell_executable() -> str | None:
    return shutil.which("pwsh") or shutil.which("powershell")


def ps_quote(path: Path) -> str:
    return "'" + str(path).replace("'", "''") + "'"


def manifest_roster() -> list[dict[str, object]]:
    value = json.loads(MANIFEST.read_text(encoding="utf-8"))
    return [*value["files"], *value["verifiedNoopFiles"]]


def historical_closure_file(canonical_source: str) -> Path:
    return (
        HISTORICAL_PROMOTION_CLOSURE_ROOT
        / HISTORICAL_PROMOTION_CLOSURE_FILES[canonical_source]
    )


def build_source_fixture(root: Path) -> tuple[Path, Path]:
    workspace = root / "workspace-unreal"
    physical = root / "physical-source-assets"
    workspace.mkdir()
    physical.mkdir()
    for row in manifest_roster():
        relative = str(row["source"])
        source = UNREAL / Path(relative)
        if relative.startswith(SOURCE_ASSETS_PREFIX):
            destination = physical / Path(
                relative.removeprefix(SOURCE_ASSETS_PREFIX)
            )
        else:
            destination = workspace / Path(relative)
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
    for canonical_source in HISTORICAL_PROMOTION_OVERRIDES:
        source = historical_closure_file(canonical_source)
        destination = (
            physical
            / HISTORICAL_PROMOTION_CLOSURE_RELATIVE_ROOT
            / HISTORICAL_PROMOTION_CLOSURE_FILES[canonical_source]
        )
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
    (workspace / "not-in-manifest.txt").write_bytes(b"must-not-copy\n")
    (physical / "not-in-manifest.txt").write_bytes(b"must-not-copy\n")
    return workspace, physical


def run_helper(
    destination: Path,
    workspace: Path,
    physical: Path,
) -> subprocess.CompletedProcess[str]:
    executable = powershell_executable()
    if executable is None:
        raise unittest.SkipTest("PowerShell is unavailable on this host.")
    return subprocess.run(
        [
            executable,
            "-NoLogo",
            "-NoProfile",
            "-NonInteractive",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(HELPER),
            "-DestinationRoot",
            str(destination),
            "-WorkspaceUnrealRoot",
            str(workspace),
            "-WorkspaceSourceAssetsRoot",
            str(physical),
            "-NativeProjectRoot",
            r"D:\triad\TRIAD",
            "-ManifestPath",
            str(MANIFEST),
        ],
        cwd=REPO,
        text=True,
        capture_output=True,
        check=False,
    )


def create_junction(link: Path, target: Path) -> None:
    executable = powershell_executable()
    if executable is None:
        raise unittest.SkipTest("PowerShell is unavailable on this host.")
    command = (
        "$ErrorActionPreference='Stop'; "
        f"New-Item -ItemType Junction -Path {ps_quote(link)} "
        f"-Target {ps_quote(target)} "
        "| Out-Null"
    )
    result = subprocess.run(
        [
            executable,
            "-NoLogo",
            "-NoProfile",
            "-NonInteractive",
            "-Command",
            command,
        ],
        text=True,
        capture_output=True,
        check=False,
    )
    if result.returncode != 0:
        raise unittest.SkipTest(f"Cannot create junction fixture: {result.stderr}")


class IstanaExploreV5DPromotionInputSnapshotContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.script = HELPER.read_text(encoding="utf-8")
        cls.roster = manifest_roster()

    def test_helper_is_closed_fail_safe_and_keeps_production_inputs_exact(self) -> None:
        self.assertEqual(len(self.roster), 52)
        self.assertEqual(
            {
                str(row["source"])
                for row in self.roster
                if str(row["source"]).startswith(SOURCE_ASSETS_PREFIX)
            },
            PHYSICAL_SOURCE_ASSETS,
        )
        self.assertIn("$ExpectedSnapshotFileCount = 52", self.script)
        self.assertIn("$ExpectedPromotionFileCount = 51", self.script)
        self.assertIn("$ExpectedVerifiedNoopCount = 1", self.script)
        self.assertIn(
            r"D:\triad\TRIAD\Saved\TRIAD\CDriveRelief\20260830_workspace\TRIAD-Sensor-Fusion-Repo_unreal_SourceAssets",
            self.script,
        )
        for relative in PHYSICAL_SOURCE_ASSETS:
            self.assertIn(f"'{relative}'", self.script)
        self.assertIn(
            "$ExpectedHistoricalPromotionOverrideCount = 10",
            self.script,
        )
        self.assertIn(
            f"$HistoricalPromotionClosureRootRelativePath = "
            f"'{HISTORICAL_PROMOTION_CLOSURE_RELATIVE_ROOT}'",
            self.script,
        )
        script_overrides = re.findall(
            r"(?m)^\s+CanonicalSource = '([^']+)'$", self.script
        )
        script_closure_files = re.findall(
            r"(?m)^\s+ClosureFile = '([^']+)'$", self.script
        )
        self.assertEqual(len(script_overrides), 10)
        self.assertEqual(set(script_overrides), set(HISTORICAL_PROMOTION_OVERRIDES))
        self.assertEqual(len(script_closure_files), 10)
        self.assertEqual(
            set(script_closure_files),
            set(HISTORICAL_PROMOTION_CLOSURE_FILES.values()),
        )
        self.assertEqual(
            set(HISTORICAL_PROMOTION_CLOSURE_FILES),
            set(HISTORICAL_PROMOTION_OVERRIDES),
        )
        closure_files = {
            path.relative_to(HISTORICAL_PROMOTION_CLOSURE_ROOT).as_posix()
            for path in HISTORICAL_PROMOTION_CLOSURE_ROOT.rglob("*")
            if path.is_file()
        }
        self.assertEqual(
            closure_files, set(HISTORICAL_PROMOTION_CLOSURE_FILES.values())
        )
        manifest_by_source = {
            str(row["source"]): row for row in self.roster
        }
        for canonical_source, historical_receipt in (
            HISTORICAL_PROMOTION_OVERRIDES.items()
        ):
            row = manifest_by_source[canonical_source]
            self.assertEqual(
                (row["bytes"], row["sha256"]), historical_receipt
            )
            closure_file = historical_closure_file(canonical_source)
            self.assertEqual(
                (closure_file.stat().st_size, sha256(closure_file)),
                historical_receipt,
            )
            live_file = UNREAL / canonical_source
            self.assertEqual(
                (live_file.stat().st_size, sha256(live_file)),
                LIVE_PROMOTION_OVERRIDE_RECEIPTS[canonical_source],
            )
            self.assertNotEqual(
                closure_file.read_bytes(), live_file.read_bytes()
            )
        self.assertIn(
            "$kind = 'historical-promotion-source-closure'", self.script
        )
        self.assertIn("[IO.File]::Copy($row.Source, $row.Destination, $false)", self.script)
        self.assertIn("[IO.FileMode]::CreateNew", self.script)
        self.assertIn("[IO.FileOptions]::WriteThrough", self.script)
        self.assertIn("$stream.Flush($true)", self.script)
        self.assertIn("[IO.File]::Move($temporary, $fullPath)", self.script)
        self.assertNotIn("Copy-Item", self.script)
        self.assertNotIn("Remove-Item", self.script)

        first_write = self.script.index("# First write:")
        preflight = self.script.index("# Complete read-only preflight.")
        copy = self.script.index("[IO.File]::Copy($row.Source")
        postflight = self.script.index("$row.SourcePostReceipt")
        final_write = self.script.index("# Final write:")
        final_call = self.script.index("Write-DurableCompleteReceipt", final_write)
        self.assertLess(preflight, first_write)
        self.assertLess(first_write, copy)
        self.assertLess(copy, postflight)
        self.assertLess(postflight, final_write)
        self.assertLess(final_write, final_call)
        self.assertNotIn("New-Item", self.script[final_call:])
        self.assertNotIn("[IO.File]::Copy", self.script[final_call:])

        self.assertEqual(PRODUCTION_PROMOTION.stat().st_size, 85_907)
        self.assertEqual(sha256(PRODUCTION_PROMOTION), PRODUCTION_PROMOTION_RECEIPT[1])
        self.assertEqual(MANIFEST.stat().st_size, MANIFEST_RECEIPT[0])
        self.assertEqual(sha256(MANIFEST), MANIFEST_RECEIPT[1])

    def test_builds_exact_snapshot_and_durable_sibling_receipt(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            workspace, physical = build_source_fixture(root)
            destination = root / "fresh-composite-root"
            result = run_helper(destination, workspace, physical)
            self.assertEqual(result.returncode, 0, result.stderr)
            output = json.loads(result.stdout)
            self.assertEqual(output["Status"], "SNAPSHOT_PASS")
            self.assertEqual(output["SnapshotFileCount"], 52)
            self.assertEqual(output["PhysicalSourceAssetsFileCount"], 3)
            self.assertEqual(output["HistoricalSourceClosureFileCount"], 10)

            expected = {str(row["source"]) for row in self.roster}
            actual = {
                path.relative_to(destination).as_posix()
                for path in destination.rglob("*")
                if path.is_file()
            }
            self.assertEqual(actual, expected)
            self.assertFalse((destination / "not-in-manifest.txt").exists())
            receipt_path = Path(str(destination) + ".complete.json")
            self.assertTrue(receipt_path.is_file())
            receipt = json.loads(receipt_path.read_text(encoding="utf-8"))
            self.assertEqual(receipt["State"], "COMPLETE")
            self.assertEqual(receipt["SnapshotRoot"], str(destination.resolve()))
            self.assertEqual(receipt["SnapshotFileCount"], 52)
            self.assertEqual(receipt["PhysicalSourceAssetsFileCount"], 3)
            self.assertEqual(receipt["HistoricalSourceClosureFileCount"], 10)
            self.assertEqual(len(receipt["Files"]), 52)
            self.assertEqual(output["CompleteReceiptSha256"], sha256(receipt_path))
            for row in receipt["Files"]:
                self.assertEqual(row["SourcePreBytes"], row["SourcePostBytes"])
                self.assertEqual(row["SourcePreBytes"], row["SnapshotPostBytes"])
                self.assertEqual(row["SourcePreSha256"], row["SourcePostSha256"])
                self.assertEqual(row["SourcePreSha256"], row["SnapshotPostSha256"])
            historical_rows = {
                row["RelativePath"]: row
                for row in receipt["Files"]
                if row["SourceKind"]
                == "historical-promotion-source-closure"
            }
            self.assertEqual(
                set(historical_rows), set(HISTORICAL_PROMOTION_OVERRIDES)
            )
            for canonical_source, historical_receipt in (
                HISTORICAL_PROMOTION_OVERRIDES.items()
            ):
                row = historical_rows[canonical_source]
                self.assertEqual(
                    (row["SourcePreBytes"], row["SourcePreSha256"]),
                    historical_receipt,
                )
                snapshot_file = destination / canonical_source
                fixture_live_file = workspace / canonical_source
                self.assertEqual(
                    (snapshot_file.stat().st_size, sha256(snapshot_file)),
                    historical_receipt,
                )
                self.assertEqual(
                    (
                        fixture_live_file.stat().st_size,
                        sha256(fixture_live_file),
                    ),
                    LIVE_PROMOTION_OVERRIDE_RECEIPTS[canonical_source],
                )
                self.assertNotEqual(
                    snapshot_file.read_bytes(), fixture_live_file.read_bytes()
                )
            self.assertFalse(
                (
                    destination
                    / "SourceAssets/IstanaPublicViewExploreV5D/PublicRealm/"
                    "NativeSourceClosure"
                ).exists()
            )
            self.assertEqual(
                list(receipt_path.parent.glob(receipt_path.name + ".*.tmp")), []
            )

    def test_historical_override_closure_drift_fails_before_any_write(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            workspace, physical = build_source_fixture(root)
            canonical_source = next(iter(HISTORICAL_PROMOTION_OVERRIDES))
            closure = (
                physical
                / HISTORICAL_PROMOTION_CLOSURE_RELATIVE_ROOT
                / HISTORICAL_PROMOTION_CLOSURE_FILES[canonical_source]
            )
            closure.write_bytes(b"foreign historical-closure drift\n")
            destination = root / "must-stay-absent"
            result = run_helper(destination, workspace, physical)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("receipt mismatch", result.stderr.lower())
            self.assertIn(
                "historical-promotion-source-closure", result.stderr.lower()
            )
            self.assertFalse(destination.exists())
            self.assertFalse(Path(str(destination) + ".complete.json").exists())

    def test_source_drift_fails_before_creating_destination(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            workspace, physical = build_source_fixture(root)
            drifted = workspace / (
                "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
                "TRIADIstanaExploreV5AppearanceActor.cpp"
            )
            drifted.write_bytes(b"foreign drift\n")
            destination = root / "must-stay-absent"
            result = run_helper(destination, workspace, physical)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("receipt mismatch", result.stderr.lower())
            self.assertFalse(destination.exists())
            self.assertFalse(Path(str(destination) + ".complete.json").exists())

    def test_existing_destination_is_never_reused_or_modified(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            workspace, physical = build_source_fixture(root)
            destination = root / "existing-composite-root"
            destination.mkdir()
            sentinel = destination / "operator-owned.txt"
            sentinel.write_bytes(b"preserve exactly\n")
            result = run_helper(destination, workspace, physical)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("fresh and absent", result.stderr.lower())
            self.assertEqual(sentinel.read_bytes(), b"preserve exactly\n")
            self.assertEqual(
                {path.name for path in destination.iterdir()},
                {"operator-owned.txt"},
            )
            self.assertFalse(Path(str(destination) + ".complete.json").exists())

    def test_nested_source_junction_fails_before_creating_destination(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            workspace, physical = build_source_fixture(root)
            generated = (
                physical
                / "IstanaPublicViewExploreV5D/PublicRealm/Generated"
            )
            outside = root / "outside-generated"
            generated.rename(outside)
            create_junction(generated, outside)
            destination = root / "must-stay-absent"
            result = run_helper(destination, workspace, physical)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("reparse ancestor", result.stderr.lower())
            self.assertFalse(destination.exists())
            self.assertFalse(Path(str(destination) + ".complete.json").exists())

    def test_non_c_destination_is_refused_before_any_write(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            workspace, physical = build_source_fixture(root)
            destination_text = r"D:\triad-never-create\v5d-snapshot-test"
            result = run_helper(Path(destination_text), workspace, physical)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("must be a fresh c: path", result.stderr.lower())


if __name__ == "__main__":
    unittest.main()
