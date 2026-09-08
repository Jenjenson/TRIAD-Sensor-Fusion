from pathlib import Path
import json
import shutil
import subprocess
import unittest


REPO = Path(__file__).resolve().parents[4]
WRAPPER = (
    REPO
    / "scripts/Invoke-IstanaRFOneKilometreActualFileNativeTransaction.ps1"
)
PRIVATE = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private"
)
SOURCES = {
    "TRIADRFIndexedGeometryQuery.cpp": PRIVATE
    / "TRIADRFIndexedGeometryQuery.cpp",
    "TRIADRFIndexedGeometryQueryTests.cpp": PRIVATE
    / "Tests/TRIADRFIndexedGeometryQueryTests.cpp",
    "TRIADRFOneKilometreActualFileTests.cpp": PRIVATE
    / "Tests/TRIADRFOneKilometreActualFileTests.cpp",
}
FILTER = "TRIAD.RF.IndexedGeometryQuery.OneKilometreV2ActualFile"
INTERMEDIATE_ROOT = (
    r"Plugins\TRIADSensorFusion\Intermediate\Build\Win64\x64\UnrealEditor"
    r"\Development\TRIADSensorFusion"
)
MODULE_BUILD_PRODUCTS = (
    r"Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.dll",
    r"Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.pdb",
    r"Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor-TRIADSensorFusion.exp",
    r"Plugins\TRIADSensorFusion\Binaries\Win64\UnrealEditor.modules",
    INTERMEDIATE_ROOT + r"\UnrealEditor-TRIADSensorFusion.lib",
    INTERMEDIATE_ROOT + r"\UnrealEditor-TRIADSensorFusion.exp",
    INTERMEDIATE_ROOT + r"\UnrealEditor-TRIADSensorFusion.dll.rsp",
    INTERMEDIATE_ROOT + r"\UnrealEditor-TRIADSensorFusion.dll.rsp.old",
    INTERMEDIATE_ROOT + r"\UnrealEditor-TRIADSensorFusion.lib.rsp",
    INTERMEDIATE_ROOT + r"\UnrealEditor-TRIADSensorFusion.lib.rsp.old",
)
SOURCE_INTERMEDIATE_SUFFIXES = (
    ".obj",
    ".dep.json",
    ".obj.rsp",
    ".obj.rsp.old",
    ".sarif",
)
SOURCE_INTERMEDIATE_PATHS = tuple(
    INTERMEDIATE_ROOT + "\\" + source + suffix
    for source in SOURCES
    for suffix in SOURCE_INTERMEDIATE_SUFFIXES
)
BUILD_PRODUCTS = MODULE_BUILD_PRODUCTS + SOURCE_INTERMEDIATE_PATHS
PINNED_OBJECT_PRESTATES = {
    INTERMEDIATE_ROOT + r"\TRIADRFIndexedGeometryQuery.cpp.obj": (
        3380560,
        "A18231509929785E4BEC4ED26B15EADDFF9B2EDB798FD4A0425105C9C0906472",
    ),
    INTERMEDIATE_ROOT + r"\TRIADRFIndexedGeometryQueryTests.cpp.obj": (
        1583646,
        "C3FA184BBFC1364A6E58113A4B5FB9CA3D58C049F9ABEF3CCD6C3B4FA9B22762",
    ),
}


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    return text[begin : text.index(end, begin)]


class RFOneKilometreActualFileNativeTransactionContractTests(
    unittest.TestCase
):
    @classmethod
    def setUpClass(cls) -> None:
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")
        cls.sources = {
            name: path.read_text(encoding="utf-8")
            for name, path in SOURCES.items()
        }

    def test_all_reviewed_inputs_are_exactly_hash_and_byte_pinned(self):
        expected = (
            (
                "167670L",
                "606FD58A90D396D34F5648B78D26A6BDD7510662565DD095CABC84DC6800FCAB",
            ),
            (
                "73946L",
                "459831E13FC9793FDC966A3DF78876443BC078059B9D4A2D12E3AD7A99D5E823",
            ),
            (
                "4839L",
                "DA1CA1CA4BEF5E21AB47B1ECBF501CB7DBEFCC8219F5E6D5EC6CCB34CDAA0D71",
            ),
            (
                "12506346L",
                "85E654FBA602B2DBC51EB64C6B66FF234C8EA1F948152612C755EDC22C65DA51",
            ),
            (
                "28674L",
                "210CB26DDB9B531ADEB3C917606A736AEFC857EB6696DA485A2E63DBB8B31662",
            ),
            (
                "14783L",
                "FE509917AE59BE0918BCD799F23DC981E00A394C6C328A7342F56B371C40BCC2",
            ),
            (
                "1298L",
                "42114E7A55BAC2974ECAB36B16E19EAAE013B7D8B3E2354CE93E15933A0AAFC3",
            ),
        )
        for size, digest in expected:
            self.assertIn(size, self.wrapper)
            self.assertIn(digest, self.wrapper)
        input_gate = between(
            self.wrapper,
            "function Assert-WorkspaceAndNativeInputs {",
            "function Assert-NativeSourceAdmissible {",
        )
        self.assertIn("foreach ($source in $sourcePins)", input_gate)
        self.assertIn("$repositoryUnrealRoot", input_gate)
        self.assertIn("$nativeProjectRoot", input_gate)
        self.assertIn("$nativeProjectFile", input_gate)
        self.assertIn("$script:selfPin", input_gate)

    def test_native_sources_admit_only_each_exact_predecessor_or_target(self):
        predecessors = (
            (
                "166878L",
                "A56719963B602795F5B8A4C2715C51BA39650D9F124686C71FDE418BCF4D3C1D",
            ),
            (
                "69037L",
                "A17DF12D9B4B365E39290D80B4461F79B03315181E978E10184258D487805003",
            ),
        )
        for size, digest in predecessors:
            self.assertIn(size, self.wrapper)
            self.assertIn(digest, self.wrapper)
        self.assertEqual(
            2, self.wrapper.count("NativePolicy = 'EXACT_PREDECESSOR_OR_TARGET'")
        )
        self.assertEqual(
            1, self.wrapper.count("NativePolicy = 'MISSING_OR_TARGET'")
        )
        gate = between(
            self.wrapper,
            "function Assert-NativeSourceAdmissible {",
            "function New-NativeSourceJournal {",
        )
        self.assertIn("$isTarget = Test-StateIdentity", gate)
        self.assertIn("$isPredecessor = Test-StateIdentity", gate)
        self.assertIn("-Present ([bool] $Pin.PredecessorPresent)", gate)
        self.assertIn("if (-not $isTarget -and -not $isPredecessor)", gate)
        self.assertIn("neither its exact predecessor nor target", gate)
        self.assertIn("RequiresPromotion = -not $isTarget", gate)

    def test_source_backups_are_non_overwriting_and_precede_mutation(self):
        journal = between(
            self.wrapper,
            "function New-NativeSourceJournal {",
            "function Install-NativeSource {",
        )
        self.assertIn("for ($index = 0; $index -lt $sourcePins.Count", journal)
        self.assertIn("if ($admission.Before.Present)", journal)
        self.assertIn(
            "[IO.File]::Copy($admission.NativePath, $backupPath, $false)",
            journal,
        )
        self.assertIn("Assert-SameState -Expected $admission.Before", journal)
        self.assertIn("BackupPath = $backupPath", journal)
        self.assertIn("Backup = $backup", journal)
        self.assertIn("PromotionStagePath = $promotionStagePath", journal)
        self.assertIn(
            "PromotionDisplacedPath = $promotionDisplacedPath", journal
        )
        self.assertIn("PromotionDisplaced = $null", journal)
        self.assertIn("$promotionStagePath = Join-Path $sourceBackupRoot", journal)
        self.assertIn(
            "$promotionDisplacedPath = Join-Path $sourceBackupRoot", journal
        )
        install = between(
            self.wrapper,
            "function Install-NativeSource {",
            "function New-BuildProductJournal {",
        )
        self.assertIn("[IO.File]::Copy($JournalRow.WorkspacePath, $stage, $false)", install)
        self.assertIn("[IO.File]::Replace(", install)
        self.assertIn(
            "$stage, $JournalRow.NativePath, $displaced, $true", install
        )
        self.assertNotIn(
            "$stage, $JournalRow.NativePath, $null, $true", install
        )
        self.assertIn("Native RF source promotion paths must be non-empty", install)
        self.assertIn("PromotionDisplaced = Assert-SameState", install)
        self.assertIn("-Expected $JournalRow.Before -Path $displaced", install)
        self.assertIn("[IO.File]::Move($stage, $JournalRow.NativePath)", install)
        source_backup = self.wrapper.index("$sourceJournal = @(New-NativeSourceJournal)")
        build_backup = self.wrapper.index("$buildJournal = @(New-BuildProductJournal)")
        prepared = self.wrapper.index("Write-JsonAtomic -Path $preparedPath")
        promotion = self.wrapper.index("Install-NativeSource -JournalRow $_")
        build = self.wrapper.index("& $buildTool @buildArguments")
        self.assertLess(source_backup, build_backup)
        self.assertLess(build_backup, prepared)
        self.assertLess(prepared, promotion)
        self.assertLess(promotion, build)

    def test_exactly_twenty_five_build_product_states_are_journaled(self):
        self.assertEqual(25, len(BUILD_PRODUCTS))
        self.assertEqual(25, len(set(path.casefold() for path in BUILD_PRODUCTS)))
        self.assertEqual(15, len(SOURCE_INTERMEDIATE_PATHS))
        for path in BUILD_PRODUCTS:
            self.assertIn(path, self.wrapper)
        journal = between(
            self.wrapper,
            "function New-BuildProductJournal {",
            "function Restore-OneFileState {",
        )
        self.assertIn("$buildProductRelativePaths.Count", journal)
        self.assertIn("[IO.File]::Copy($nativePath, $backupPath, $false)", journal)
        self.assertIn("Assert-SameState -Expected $before", journal)
        self.assertIn("TRANSACTION_START_EXACT_SNAPSHOT", journal)
        self.assertIn("EXACT_CURRENT_POST_FAILURE_PRESTATE", self.wrapper)
        self.assertIn("BuildProducts = $buildJournal", self.wrapper)
        self.assertIn(
            "RESTORE_ALL_3_NATIVE_SOURCE_PREDECESSORS_AND_ALL_25_BUILD_PRODUCT_STATES",
            self.wrapper,
        )

    def test_post_failure_object_prestates_are_exactly_admitted(self):
        for path, (size, digest) in PINNED_OBJECT_PRESTATES.items():
            self.assertIn(path, self.wrapper)
            self.assertIn(f"Bytes = {size}L", self.wrapper)
            self.assertIn(digest, self.wrapper)
        self.assertEqual(
            2, self.wrapper.count("Admission = 'EXACT_CURRENT_POST_FAILURE_PRESTATE'")
        )
        journal = between(
            self.wrapper,
            "function New-BuildProductJournal {",
            "function Restore-OneFileState {",
        )
        identity_check = journal.index("Test-StateIdentity -State $before")
        backup_copy = journal.index("[IO.File]::Copy($nativePath, $backupPath, $false)")
        self.assertLess(identity_check, backup_copy)
        self.assertIn("$prestatePin.Present", journal)
        self.assertIn("$prestatePin.Bytes", journal)
        self.assertIn("$prestatePin.Sha256", journal)
        self.assertIn("BeforeAdmission = $beforeAdmission", journal)
        self.assertIn("PrestatePin = $prestatePin", journal)

    def test_build_is_runtime_module_scoped_and_forces_source_discovery(self):
        build = self.wrapper[self.wrapper.index("$buildArguments = @(") :]
        build = build[: build.index("$automationResult =")]
        self.assertIn("'-Module=TRIADSensorFusion'", build)
        self.assertNotIn("'-Module=TRIADSensorFusionEditor'", build)
        self.assertIn("'-NoUBTMakefiles'", build)
        self.assertIn("'-MaxParallelActions=1'", build)
        self.assertIn("'-NoUBA'", build)
        self.assertIn("'-NoUBALocal'", build)
        self.assertIn("& $buildTool @buildArguments *> $buildLog", build)
        self.assertIn("$buildExitCode -ne 0", build)

    def test_every_changed_source_requires_one_compile_marker_and_object(self):
        proof = between(
            self.wrapper,
            "$buildLogText = Get-Content -LiteralPath $buildLog -Raw",
            "Assert-NativeProjectIdle -Checkpoint 'after module-scoped RF build'",
        )
        for source_name in SOURCES:
            self.assertIn(source_name, self.wrapper)
            escaped = source_name.replace(".", r"\.")
            self.assertIn(escaped, self.wrapper)
        self.assertIn("$changedSourceCount", proof)
        self.assertIn("$targetUpToDateObserved", proof)
        self.assertIn("UBT reported Target is up to date", proof)
        self.assertIn("$compileProofs = @($sourceJournal | ForEach-Object", proof)
        self.assertIn("$markerCount = [regex]::Matches", proof)
        self.assertIn("if ($_.RequiresPromotion -and $markerCount -ne 1)", proof)
        self.assertIn("lacks exactly one Compile [x64] marker", proof)
        self.assertIn("if ($_.RequiresPromotion -and -not $compiledObject.Present)", proof)
        self.assertIn("CompileProofs = $compileProofs", self.wrapper)

    def test_runs_only_the_exact_filter_and_requires_one_clean_success(self):
        self.assertIn(f"'{FILTER}'", self.wrapper)
        self.assertNotIn("[string[]] $TestFilter", self.wrapper)
        automation = between(
            self.wrapper,
            "function Invoke-ExactAutomationTest {",
            "$script:selfPin =",
        )
        self.assertIn(
            "'-ExecCmds=\"Automation RunTests ' + $automationFilter + '\" '",
            automation,
        )
        self.assertIn("$allSuccesses -ne 1", automation)
        self.assertIn("$exactSuccesses -ne 1", automation)
        self.assertIn("$failures -ne 0", automation)
        self.assertIn("Automation Test Queue Empty", automation)
        self.assertIn("requires exactly one clean Success", automation)
        self.assertIn("RequiredSuccessfulCompletions = 1", self.wrapper)
        self.assertIn("ZeroSuccessRejected = $true", self.wrapper)

    def test_failure_restores_all_three_sources_and_all_build_products(self):
        restore = between(
            self.wrapper,
            "function Restore-NativeTransaction {",
            "function Test-ExactCommandLineToken {",
        )
        self.assertIn("$SourceJournal.Count -ne $sourcePins.Count", restore)
        self.assertIn(
            "$BuildJournal.Count -ne $expectedBuildProductCount", restore
        )
        self.assertIn("complete fixed journal", restore)
        self.assertIn("foreach ($row in $SourceJournal)", restore)
        self.assertIn("$matchesBefore = Test-StateIdentity", restore)
        self.assertIn("$matchesTarget = Test-StateIdentity", restore)
        self.assertIn("if (-not $matchesBefore -and -not $matchesTarget)", restore)
        self.assertIn("foreach ($row in $BuildJournal)", restore)
        self.assertEqual(2, restore.count("Restore-OneFileState"))
        receipt_write = self.wrapper.index(
            "Write-JsonAtomic -Path $receiptPath -Value $receipt"
        )
        catch = self.wrapper[receipt_write:]
        self.assertIn("Restore-NativeTransaction", catch)
        self.assertIn("-SourceJournal $sourceJournal", catch)
        self.assertIn("-BuildJournal $buildJournal", catch)
        self.assertIn("Write-JsonAtomic -Path $failurePath", catch)
        self.assertIn("failed and rolled back", catch)
        self.assertNotIn("Remove-Item", self.wrapper)

    def test_success_keeps_compiled_target_products(self):
        receipt_write = self.wrapper.index(
            "Write-JsonAtomic -Path $receiptPath -Value $receipt"
        )
        success_path = self.wrapper[
            self.wrapper.index("$automationResult = Invoke-ExactAutomationTest") :
            receipt_write
        ]
        self.assertNotIn("Restore-NativeTransaction", success_path)
        self.assertIn("FinalBuildProducts = $finalBuildProducts", success_path)
        self.assertIn("LEAVE_COMPILED_TARGET_PRODUCTS_IN_PLACE", success_path)

    def test_v3_receipts_publish_complete_product_counts(self):
        for receipt_kind in ("static_check", "prepared", "receipt", "failure"):
            self.assertIn(
                "triad.rf.one_kilometre_v2.actual_file_native_transaction."
                + receipt_kind
                + ".v3",
                self.wrapper,
            )
        self.assertNotIn("actual_file_native_transaction.prepared.v2", self.wrapper)
        self.assertNotIn("actual_file_native_transaction.receipt.v2", self.wrapper)
        self.assertNotIn("actual_file_native_transaction.failure.v2", self.wrapper)
        self.assertIn("NativeSourceCount = $SourceJournal.Count", self.wrapper)
        self.assertIn("ExpectedBuildProductCount = $expectedBuildProductCount", self.wrapper)
        self.assertIn("JournalledBuildProductCount = $buildJournal.Count", self.wrapper)

    def test_absent_predecessor_accepts_only_empty_backup_binding(self):
        restore_one = between(
            self.wrapper,
            "function Restore-OneFileState {",
            "function Restore-NativeTransaction {",
        )
        self.assertIn("[AllowNull()] [AllowEmptyString()]", restore_one)
        present_branch = between(restore_one, "if ($Before.Present) {", "    else {")
        self.assertIn("$null -eq $Backup", present_branch)
        self.assertIn("[string]::IsNullOrWhiteSpace($BackupPath)", present_branch)
        self.assertIn("Assert-SameState -Expected $Backup", present_branch)
        absent_branch = restore_one[restore_one.index("    else {") :]
        self.assertIn("$null -ne $Backup", absent_branch)
        self.assertIn("-not [string]::IsNullOrWhiteSpace($BackupPath)", absent_branch)
        self.assertIn("has backup material for an absent predecessor", absent_branch)
        self.assertIn("[IO.File]::Delete($Path)", absent_branch)

    def test_capstone_identity_set_is_snapshotted_and_unchanged_at_boundaries(self):
        protected = between(
            self.wrapper,
            "function Get-ProtectedUE54Identity {",
            "function Get-NativeTRIADUnrealProcesses {",
        )
        self.assertIn(r"C:\Program Files\Epic Games\UE_5.4", self.wrapper)
        self.assertIn(r"C:\Users\Lyz\Desktop\CAPSTONE\Capstone.uproject", self.wrapper)
        self.assertIn("Test-ExactCommandLineToken", protected)
        self.assertIn("CreationUtcTicks", protected)
        self.assertIn("ExecutablePath", protected)
        self.assertIn("CommandLine", protected)
        self.assertIn("SessionCount", protected)
        self.assertIn("Sessions = @($identities)", protected)
        self.assertIn("Sort-Object ProcessId", protected)
        self.assertNotIn("More than one UE5.4/Capstone session", protected)
        self.assertIn("function Assert-ProtectedUE54Unchanged", protected)
        self.assertIn("$beforeCanonical", protected)
        self.assertIn("$afterCanonical", protected)
        self.assertIn("Protected UE5.4/Capstone identity set changed", protected)
        self.assertIn(
            "$script:protectedUE54Before = Get-ProtectedUE54Identity",
            self.wrapper,
        )
        idle_calls = self.wrapper.count("Assert-NativeProjectIdle -Checkpoint")
        self.assertGreaterEqual(idle_calls, 8)
        idle = between(
            self.wrapper,
            "function Assert-NativeProjectIdle {",
            "function Assert-WorkspaceAndNativeInputs {",
        )
        self.assertIn("Assert-ProtectedUE54Unchanged", idle)
        owned = between(
            self.wrapper,
            "function Assert-OwnedAutomationBoundary {",
            "function Stop-OwnedAutomation {",
        )
        self.assertIn("Assert-ProtectedUE54Unchanged", owned)

    def test_only_ue55_or_native_triad_helpers_block_the_transaction(self):
        selector = between(
            self.wrapper,
            "function Get-NativeTRIADUnrealProcesses {",
            "function Assert-NativeProjectIdle {",
        )
        self.assertIn("$isUE55", selector)
        self.assertIn("$enginePrefix", selector)
        self.assertIn("$isNativeTRIAD", selector)
        self.assertIn("-Token $nativeProjectFile", selector)
        self.assertIn("$isUE55 -or $isNativeTRIAD", selector)
        idle = between(
            self.wrapper,
            "function Assert-NativeProjectIdle {",
            "function Assert-WorkspaceAndNativeInputs {",
        )
        self.assertIn("Get-NativeTRIADUnrealProcesses", idle)
        self.assertIn("UE5.5/native TRIAD Unreal helpers must be idle", idle)
        self.assertNotIn("Get-CimInstance Win32_Process", idle)

    def test_rc_30010_is_idle_or_owned_only_by_exact_automation(self):
        listener = between(
            self.wrapper,
            "function Get-RemoteControlListeners {",
            "function Get-ProtectedUE54Identity {",
        )
        self.assertIn(r"\S+:30010", listener)
        idle = between(
            self.wrapper,
            "function Assert-NativeProjectIdle {",
            "function Assert-WorkspaceAndNativeInputs {",
        )
        self.assertIn("Get-RemoteControlListeners", idle)
        self.assertIn("RC port 30010 must be idle", idle)
        owned = between(
            self.wrapper,
            "function Assert-OwnedAutomationBoundary {",
            "function Stop-OwnedAutomation {",
        )
        self.assertIn("Get-NativeTRIADUnrealProcesses", owned)
        self.assertIn("$nativeEditors.Count -ne 1", owned)
        self.assertIn("$Identity.ProcessId", owned)
        self.assertIn("$foreignListeners", owned)
        self.assertIn("RC port 30010 has a foreign owner", owned)
        automation = between(
            self.wrapper,
            "function Invoke-ExactAutomationTest {",
            "$script:selfPin =",
        )
        self.assertIn("Assert-OwnedAutomationBoundary", automation)
        self.assertIn("-WindowStyle Hidden", automation)

    def test_native_cpp_registers_the_exact_single_actual_file_test(self):
        source = self.sources["TRIADRFOneKilometreActualFileTests.cpp"]
        validator = self.sources["TRIADRFIndexedGeometryQuery.cpp"]
        regression = self.sources["TRIADRFIndexedGeometryQueryTests.cpp"]
        self.assertEqual(1, source.count(FILTER))
        self.assertIn("IMPLEMENT_SIMPLE_AUTOMATION_TEST", source)
        self.assertIn(
            "HardMaximumContainmentTriangleChecks = 30000000",
            validator,
        )
        self.assertIn("PinnedContainmentTriangleChecks = 29904936", source)
        self.assertIn(
            "ActualFileContainmentTriangleCheckLimit = 30000000",
            source,
        )
        self.assertIn(
            "LoadLimits.MaximumContainmentTriangleChecks =",
            source,
        )
        self.assertIn(
            "BelowContainmentBudgetLimits.MaximumContainmentTriangleChecks = 51",
            regression,
        )
        self.assertIn(
            "ExactContainmentBudgetLimits.MaximumContainmentTriangleChecks = 52",
            regression,
        )
        self.assertIn("Query.LoadFromJsonFiles", source)
        self.assertIn("Query.GetVertexCount(), 23496", source)
        self.assertIn("Query.GetTriangleCount(), 42700", source)
        self.assertIn("Query.GetSolidCount(), 1094", source)
        self.assertIn("Query.GetSurfaceCount(), 13936", source)
        self.assertRegex(source, r"Query\.GetBVHNodeCount\(\),\s*16383")

    def test_static_self_check_proves_three_source_contract(self):
        pwsh = shutil.which("pwsh") or shutil.which("pwsh.exe")
        if pwsh is None:
            self.skipTest("PowerShell 7 is unavailable")
        completed = subprocess.run(
            [
                pwsh,
                "-NoLogo",
                "-NoProfile",
                "-NonInteractive",
                "-File",
                str(WRAPPER),
                "-RunToken",
                "contract_rf_actual",
                "-StaticSelfCheck",
            ],
            check=False,
            capture_output=True,
            text=True,
        )
        self.assertEqual(0, completed.returncode, completed.stderr)
        report = json.loads(completed.stdout)
        self.assertEqual(
            "triad.rf.one_kilometre_v2.actual_file_native_transaction.static_check.v3",
            report["Schema"],
        )
        self.assertEqual("STATIC_SELF_CHECK_PASS", report["Status"])
        self.assertEqual(3, len(report["SourceTargets"]))
        self.assertEqual(3, len(report["NativeSourcePolicies"]))
        self.assertEqual(
            [
                "EXACT_PREDECESSOR_OR_TARGET",
                "EXACT_PREDECESSOR_OR_TARGET",
                "MISSING_OR_TARGET",
            ],
            report["NativeDestinationPolicies"],
        )
        self.assertEqual(3, len(report["PromotedRelativePaths"]))
        self.assertEqual(
            "FILE_REPLACE_WITH_NONEMPTY_TRANSACTION_DISPLACEMENT",
            report["ExistingSourcePromotionPolicy"],
        )
        self.assertEqual(
            "TRANSACTION_SOURCE_BACKUP_ROOT",
            report["PromotionTemporaryFilesRoot"],
        )
        self.assertEqual(["TRIADSensorFusion"], report["BuildModuleAllowlist"])
        self.assertEqual(1, report["MaximumParallelActions"])
        self.assertEqual("-NoUBTMakefiles", report["ForceSourceDiscoveryFlag"])
        self.assertTrue(report["ChangedOrAdmittedSourceCompileMarkersRequired"])
        self.assertEqual(list(SOURCES), report["ExpectedCompileSources"])
        self.assertEqual(3, len(report["CompileObjectLogPatterns"]))
        self.assertEqual(list(BUILD_PRODUCTS), report["BuildProductRelativePaths"])
        self.assertEqual(25, report["BuildProductCount"])
        self.assertEqual(15, report["SourceAssociatedIntermediateCount"])
        self.assertEqual(
            list(SOURCE_INTERMEDIATE_PATHS),
            report["SourceAssociatedIntermediatePaths"],
        )
        pinned = report["AdmittedBuildProductPrestatePins"]
        self.assertEqual(2, len(pinned))
        for row in pinned:
            self.assertEqual("EXACT_CURRENT_POST_FAILURE_PRESTATE", row["Admission"])
            expected_size, expected_digest = PINNED_OBJECT_PRESTATES[
                row["RelativePath"]
            ]
            self.assertTrue(row["Present"])
            self.assertEqual(expected_size, row["Bytes"])
            self.assertEqual(expected_digest, row["Sha256"])
        self.assertEqual(
            "PIN_2_CURRENT_POST_FAILURE_OBJECTS_AND_EXACTLY_SNAPSHOT_ALL_25",
            report["BuildProductPrestateAdmissionPolicy"],
        )
        self.assertEqual(FILTER, report["AutomationFilter"])
        self.assertEqual(1, report["RequiredSuccessfulCompletions"])
        self.assertFalse(report["ZeroSuccessfulCompletionsAccepted"])
        self.assertEqual(
            "RESTORE_ALL_3_NATIVE_SOURCE_PREDECESSORS_AND_ALL_25_BUILD_PRODUCT_STATES",
            report["FailureRollback"],
        )
        self.assertEqual(
            "LEAVE_COMPILED_TARGET_PRODUCTS_IN_PLACE",
            report["SuccessBuildProductPolicy"],
        )
        self.assertEqual(
            "SNAPSHOT_EXACT_CAPSTONE_IDENTITY_SET_AND_REQUIRE_UNCHANGED_AT_EVERY_BOUNDARY",
            report["ProtectedUE54Policy"],
        )
        self.assertTrue(report["NativeUE55AndTRIADHelpersMustBeIdle"])
        self.assertEqual(
            "IDLE_EXCEPT_OPTIONAL_SOLE_OWNED_AUTOMATION_LISTENER",
            report["RemoteControlPolicy"],
        )
        self.assertFalse(report["NativeFilesystemReadOrWritten"])


if __name__ == "__main__":
    unittest.main()
