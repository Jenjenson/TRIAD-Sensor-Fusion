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
    / "Invoke-IstanaExploreV5DR29TropicalVegetationNativeTransactionV1.ps1"
)
EDITOR_H = (
    UNREAL
    / "Plugins"
    / "TRIADSensorFusion"
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Public"
    / "TRIADIstanaExploreV5DR29VegetationEditorLibrary.h"
)
EDITOR_CPP = (
    UNREAL
    / "Plugins"
    / "TRIADSensorFusion"
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5DR29VegetationEditorLibrary.cpp"
)
FACTORY_CPP = (
    UNREAL
    / "Plugins"
    / "TRIADSensorFusion"
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5DR29VegetationAssetFactory.cpp"
)
CONTRACT = (
    UNREAL
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Vegetation"
    / "R29TropicalDetail"
    / "r29_tropical_vegetation.contract.json"
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


def parse_pins(section: str) -> list[tuple[str, int, str]]:
    return [
        (match.group(1), int(match.group(2)), match.group(3))
        for match in re.finditer(
            r"RelativePath='([^']+)'; Bytes=(\d+)L; Sha256='([0-9A-F]{64})'",
            section,
        )
    ]


class R29TropicalVegetationNativeTransactionContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        for path in (WRAPPER, EDITOR_H, EDITOR_CPP, FACTORY_CPP, CONTRACT):
            if not path.is_file():
                raise AssertionError(f"missing R29 transaction source: {path}")
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")
        cls.editor_h = EDITOR_H.read_text(encoding="utf-8")
        cls.editor = EDITOR_CPP.read_text(encoding="utf-8")
        cls.factory = FACTORY_CPP.read_text(encoding="utf-8")
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))

    def test_default_and_static_selfcheck_are_repo_only(self) -> None:
        for token in (
            "triad.istana_explore_v5d.r29_tropical_vegetation.native_transaction.v1",
            "[switch] $Execute",
            "[switch] $StaticSelfCheck",
            "if ($StaticSelfCheck -or -not $Execute)",
            "READ_ONLY_PREFLIGHT_PASS",
            "STATIC_SELF_CHECK_PASS",
            "UnrealBuildOrEditorLaunched=$false",
            "NativeTreeReadOrWritten=$false",
            "TargetMapMutated=$false",
            "CollisionNavigationSensorRfGeospatialAuthority=$false",
            "Live R29 execution requires explicit caller-supplied",
        ):
            self.assertIn(token, self.wrapper)
        self.assertLess(
            self.wrapper.index("if ($StaticSelfCheck -or -not $Execute)"),
            self.wrapper.index("$nativeProjectRoot ="),
        )
        self.assertLess(
            self.wrapper.index("if ($StaticSelfCheck -or -not $Execute)"),
            self.wrapper.index("[IO.Directory]::CreateDirectory($transactionRoot)"),
        )

    def test_exact_repo_pin_rosters_match_current_bytes(self) -> None:
        code_section = between(
            self.wrapper, "$codeSourcePins = @(", "$sourceAssetPins = @(",
        )
        asset_section = between(
            self.wrapper,
            "$sourceAssetPins = @(",
            "$expectedNativeInputRelativePaths = @(",
        )
        code_pins = parse_pins(code_section)
        asset_pins = parse_pins(asset_section)
        self.assertEqual(7, len(code_pins))
        self.assertEqual(7, len(asset_pins))
        self.assertEqual(14, len({row[0].lower() for row in code_pins + asset_pins}))
        for relative, expected_bytes, expected_sha in code_pins + asset_pins:
            path = UNREAL / Path(relative.replace("\\", "/"))
            self.assertTrue(path.is_file(), relative)
            self.assertEqual(expected_bytes, path.stat().st_size, relative)
            self.assertEqual(expected_sha, sha256(path), relative)
        for required in (
            "TRIADIstanaExploreV5DR29VegetationActor.cpp",
            "TRIADIstanaExploreV5DR29VegetationAssetFactory.cpp",
            "TRIADIstanaExploreV5DR29VegetationEditorLibrary.cpp",
            "build_r29_tropical_vegetation.py",
            "r29_tropical_vegetation.contract.json",
            "SM_IPV5D_R29_GrassFineCluster_Render.obj",
            "SM_IPV5D_R29_GrassBroadCluster_Render.obj",
            "SM_IPV5D_R29_GrassMixedCluster_Render.obj",
        ):
            self.assertIn(required, code_section + asset_section)

    def test_fresh_factory_does_not_probe_missing_assets_by_loading_them(self) -> None:
        create = self.factory[self.factory.index("bool CreateFreshAssets(") :]
        existing_count = create.index("int32 ExistingCount = 0;")
        validation = create.index("if (ExistingCount == 7)")
        duplication = create.index("DuplicateMaterial(Spec, Source, OutError)")
        self.assertLess(existing_count, validation)
        self.assertLess(validation, duplication)
        self.assertNotIn("if (ValidateInternal(ExistingReport))", create[:existing_count])
        self.assertIn("FindObject<UObject>(nullptr, *Path)", create[existing_count:validation])

    def test_editor_entrypoint_does_not_validate_missing_assets_before_creation(self) -> None:
        signature = "BuildOrValidateR29VegetationAssets(FString& OutReport)"
        begin = self.editor.index(signature)
        finish = self.editor.index(
            "ValidateR29VegetationAssets(FString& OutReport)",
            begin + len(signature),
        )
        entrypoint = self.editor[begin:finish]
        create = entrypoint.index("CreateFreshAssets(")
        validate = entrypoint.index("ValidateAssets(ExistingReport)")
        self.assertLess(create, validate)
        self.assertNotIn("ValidateAssets(", entrypoint[:create])
        self.assertIn("GetExpectedAssetObjectPaths()", entrypoint[:create])
        self.assertIn("FPackageName::DoesPackageExist", entrypoint[:create])
        self.assertIn("ExistingPackageCount == 7", entrypoint[validate:])

    def test_native_admission_is_frozen_and_exhaustive(self) -> None:
        native_inputs = between(
            self.wrapper,
            "$expectedNativeInputRelativePaths = @(",
            "$r29AssetRelativePaths = @(",
        )
        outputs = between(
            self.wrapper, "$r29AssetRelativePaths = @(", "function Test-Sha256",
        )
        self.assertEqual(15, native_inputs.count(".uasset'"))
        self.assertEqual(7, outputs.count(".uasset'"))
        for token in (
            "triad.istana_explore_v5d.r29_tropical_vegetation.native_admission.v1",
            "$admission.Status -cne 'FROZEN'",
            "$admission.Project",
            "$admission.PredecessorMap",
            "$admission.RuntimeDll",
            "$admission.EditorDll",
            "$admission.R28CommitReceipt",
            "$admission.ImmutableNativeInputs",
            "@(Compare-Object $expectedSorted $actualSorted).Count",
            "V5DVisualRealismR28V1",
            "CollisionNavigationSensorRfTerrainAuthority -ne $false",
            "@($receipt.R28GrassPackages).Count -ne 4",
            "@($receipt.R28EnvironmentPackages).Count -ne 13",
        ):
            self.assertIn(token, self.wrapper)

    def test_ue55_build_and_six_cold_stages_are_forced_and_ordered(self) -> None:
        for token in (
            "MajorVersion -ne 5",
            "MinorVersion -ne 5",
            "'-Module=TRIADSensorFusion'",
            "'-Module=TRIADSensorFusionEditor'",
            "'-ForceHeaderGeneration'",
            "'-NoUBTMakefiles'",
            "'-MaxParallelActions=1'",
            "TRIADIstanaExploreV5DR29VegetationActor.cpp.obj",
            "TRIADIstanaExploreV5DR29VegetationEditorLibrary.gen.cpp.obj",
            "exactlyOneEnvironmentOwner=true",
            "publicRealmInvariantPreserved=true",
            "Wait-OwnedHelperFullyReleased",
            "Require a second stable observation",
        ):
            self.assertIn(token, self.wrapper)
        stages = (
            "00_cold_validate_r28_predecessor",
            "01_build_r29_assets",
            "02_cold_validate_r29_assets",
            "03_apply_r29_swap_once",
            "04_cold_validate_r29_successor",
            "05_idempotent_r29_apply",
        )
        positions = [self.wrapper.index(stage) for stage in stages]
        self.assertEqual(positions, sorted(positions))
        self.assertEqual(
            6, self.wrapper.count("$stageResults.Add((Invoke-ColdStage `")
        )

    def test_map_swap_is_validated_before_mutation_and_saved_once(self) -> None:
        transaction = self.editor[
            self.editor.index("ApplyR29VegetationSuccessorToLoadedHybridMap") :
        ]
        for token in (
            "ExpectedPredecessorBytes",
            "ExpectedPredecessorSha256",
            "VerifiedExternalBackupFilename",
            "ValidateIstanaExploreV5DR28VisualSuccessorMap",
            "ValidateAssets",
            "BuildDeterministicLayout",
            "ExpectedGrassInstanceCount",
            "ExpectedTreeInstanceCount",
            "EXPLORE_V5D_R29_VEGETATION_APPLY_REFUSED_FINAL_MUTATION_GATE",
            "World->DestroyActor(Roster.R28, true, true)",
            "World->SpawnActor<ATRIADIstanaExploreV5DR29VegetationActor>",
            "SaveMap(World, TargetMapPackage)",
            "NewBlankMap(false)",
            "coexistenceObserved=false",
            "grassGeographyExact=true",
            "treeGeographyExact=true",
            "sensorAuthority=false",
            "rfAuthority=false",
            "geospatialAuthority=false",
        ):
            self.assertIn(token, self.editor_h + transaction)
        validate = transaction.index("BuildDeterministicLayout")
        remove = transaction.index("World->DestroyActor(Roster.R28")
        spawn = transaction.index(
            "World->SpawnActor<ATRIADIstanaExploreV5DR29VegetationActor>"
        )
        save = transaction.index("SaveMap(World, TargetMapPackage)")
        self.assertLess(validate, remove)
        self.assertLess(remove, spawn)
        self.assertLess(spawn, save)
        self.assertEqual(1, transaction.count("SaveMap(World, TargetMapPackage)"))

    def test_combined_facade_world_validation_is_fail_closed(self) -> None:
        for token in (
            "R29FacadeEnvironmentClassPath",
            "ResolveEnvironmentOwnerRoster",
            "ResolvePublicRealmRoster",
            "ValidateCombinedWorldR29FacadeOwner",
            "ReadCombinedWorldR29FacadeProviderReady",
            "Actor->FindFunction(R29FacadeValidationFunction)",
            "Actor->ProcessEvent(Function, ParameterMemory)",
            "bR28EnvironmentOwner == bR29FacadeEnvironmentOwner",
            "EnvironmentRoster.R28->ValidateR28Environment",
            "PublicRealm->ValidatePublicRealm",
            "impersonated or dual-owned environment-owner tag",
            "impersonated public-realm owner tag",
            "environmentOwner=%s",
            "exactlyOneEnvironmentOwner=true",
            "publicRealmInvariantPreserved=true",
        ):
            self.assertIn(token, self.editor)
        self.assertNotIn(
            '#include "TRIADIstanaExploreV5DR29FacadeEnvironmentActor.h"',
            self.editor,
        )

    def test_journals_and_helper_containment_bound_rollback(self) -> None:
        for token in (
            "Assert-NoReparseAncestor",
            "$sourceJournal",
            "$contentJournal",
            "$mapJournal",
            "$buildJournal",
            "Restore-MapFromJournal $mapJournal",
            "Restore-BuildSurface $buildJournal",
            "Restore-FileJournal $contentJournal",
            "Restore-FileJournal $sourceJournal",
            "atomic map rollback staging",
            "[IO.File]::Move($temporary, $row.Path, $true)",
            "Start-Process -FilePath $editor",
            "-WindowStyle Hidden",
            "Wait-ExpectedHelperIdentity",
            "Test-ProcessRecordEqual",
            "Stop-OwnedHelper",
            "Stop-LaunchedHelperBeforeIdentity",
            "$Handle.Kill()",
            "Assert-NativeIdle 'R29 rollback entry'",
            "ROLLBACK_INCOMPLETE",
        ):
            self.assertIn(token, self.wrapper)
        for forbidden in (
            "Remove-Item -Recurse",
            "Remove-Item -Force -Recurse",
            "Stop-Process",
            "git reset --hard",
            "rm -rf",
            "D:\\triad",
            "CAPSTONE",
        ):
            self.assertNotIn(forbidden, self.wrapper)

    def test_contract_keeps_native_execution_and_authority_false(self) -> None:
        transaction = self.contract["nativeTransaction"]
        self.assertTrue(transaction["sourceImplemented"])
        self.assertFalse(transaction["executedForThisDelivery"])
        self.assertEqual("5.5", transaction["unrealEngineVersionRequired"])
        self.assertEqual(14, transaction["hashPinnedRepositoryInputCount"])
        self.assertEqual(15, transaction["hashPinnedNativeDependencyCount"])
        self.assertEqual(7, transaction["createdAssetPackageCount"])
        self.assertEqual(6, transaction["coldEditorStageCount"])
        self.assertTrue(transaction["grassAndTreeGeographyValidatedBeforeMutation"])
        self.assertTrue(transaction["externalByteIdenticalMapBackupRequired"])
        self.assertTrue(transaction["singleMapSaveRequired"])
        self.assertTrue(transaction["coldReloadValidationRequired"])
        self.assertTrue(transaction["idempotentReplayRequired"])
        truth = self.contract["truthBoundary"]
        for key in (
            "collisionAuthority",
            "navigationAuthority",
            "sensorAuthority",
            "rfMaterialAuthority",
            "geospatialAuthority",
        ):
            self.assertFalse(truth[key])

    def test_powershell_parser_and_repo_only_static_selfcheck_pass(self) -> None:
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
                "contract_r29_vegetation",
                "-StaticSelfCheck",
            ],
            cwd=REPO,
            capture_output=True,
            text=True,
            timeout=30,
        )
        self.assertEqual(0, check.returncode, check.stdout + check.stderr)
        receipt = json.loads(check.stdout)
        self.assertEqual("STATIC_SELF_CHECK_PASS", receipt["Status"])
        self.assertEqual(7, receipt["CodeSourceCount"])
        self.assertEqual(7, receipt["SourceAssetInputCount"])
        self.assertEqual(14, receipt["HashPinnedRepoInputCount"])
        self.assertEqual(15, receipt["RequiredHashPinnedNativeInputCount"])
        self.assertEqual(7, receipt["ExactlyNewAssetCount"])
        self.assertFalse(receipt["UnrealBuildOrEditorLaunched"])
        self.assertFalse(receipt["NativeTreeReadOrWritten"])
        self.assertFalse(receipt["TargetMapMutated"])


if __name__ == "__main__":
    unittest.main()
