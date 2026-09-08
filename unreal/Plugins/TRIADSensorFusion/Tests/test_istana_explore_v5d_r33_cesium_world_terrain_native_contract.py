from __future__ import annotations

import json
import re
import subprocess
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal" / "Plugins" / "TRIADSensorFusion"
R33_SOURCE = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Terrain"
    / "R33CesiumWorldTerrainReference"
)
CONTRACT_PATH = R33_SOURCE / "r33_cesium_world_terrain_reference.contract.json"
README_PATH = R33_SOURCE / "README.md"
WRAPPER_PATH = (
    REPO
    / "scripts"
    / "Invoke-IstanaExploreV5DR33CesiumWorldTerrainReferenceNativeTransactionV1.ps1"
)
EDITOR_H_PATH = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Public"
    / "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceEditorLibrary.h"
)
EDITOR_CPP_PATH = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceEditorLibrary.cpp"
)


class R33CesiumWorldTerrainNativeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        for path in (
            CONTRACT_PATH,
            README_PATH,
            WRAPPER_PATH,
            EDITOR_H_PATH,
            EDITOR_CPP_PATH,
        ):
            if not path.is_file():
                raise AssertionError(f"missing R33 native input: {path}")
        cls.contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
        cls.readme = README_PATH.read_text(encoding="utf-8")
        cls.wrapper = WRAPPER_PATH.read_text(encoding="utf-8")
        cls.editor_h = EDITOR_H_PATH.read_text(encoding="utf-8")
        cls.editor_cpp = EDITOR_CPP_PATH.read_text(encoding="utf-8")

    def test_contract_declares_source_only_unexecuted_native_wrapper(self) -> None:
        order = self.contract["nativeOrdering"]
        self.assertTrue(order["nativeTransactionImplemented"])
        self.assertFalse(order["nativeTransactionExecuted"])
        self.assertFalse(order["prerequisitesSatisfiedForThisDelivery"])
        for name in (
            "r30ExplicitHumanReviewReceiptRequired",
            "r31ExplicitHumanReviewReceiptRequired",
            "r32ExplicitHumanReviewReceiptRequired",
        ):
            self.assertTrue(order[name])

        native = self.contract["nativeTransaction"]
        self.assertEqual(
            native["schema"],
            "triad.istana_explore_v5d.r33_cesium_world_terrain_reference.native_transaction.v1",
        )
        promotion = native["exactPromotion"]
        self.assertEqual(promotion["cppFileCount"], 9)
        self.assertEqual(promotion["contractFileCount"], 1)
        self.assertEqual(promotion["contentPackageCount"], 0)
        self.assertEqual(len(promotion["files"]), 10)
        self.assertIn(
            "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
            "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp",
            promotion["files"],
        )
        self.assertTrue(promotion["replacesR31PreR33ContextPolicyPair"])
        self.assertTrue(promotion["replacesR32PreR33GroundVegetationPair"])
        self.assertFalse(promotion["otherSourceOrContractPromotionAllowed"])
        self.assertFalse(promotion["nativeContentCreationAllowed"])

    def test_cesium_218_descriptor_identity_is_fail_closed_and_receipted(self) -> None:
        compatibility = self.contract["cesiumPluginCompatibility"]
        self.assertEqual(78, compatibility["descriptorVersion"])
        self.assertEqual("2.18.0", compatibility["versionName"])
        self.assertEqual("5.5.0", compatibility["engineVersion"])
        self.assertEqual(1214, compatibility["descriptorBytes"])
        self.assertEqual(
            "77F60013ADAFAC1EADBC9364F7453E6CF0BD83AA824DFBE5291D5D0FF2F1D2A6",
            compatibility["descriptorSha256"],
        )
        self.assertTrue(compatibility["verifyBeforeNativeWrite"])
        self.assertTrue(compatibility["exactlyOneDescriptorRequired"])
        self.assertTrue(compatibility["receiptRecordsDescriptorIdentity"])
        self.assertFalse(compatibility["pluginVersionProvesProviderEntitlement"])
        for token in (
            "$expectedCesiumDescriptorVersion = 78",
            "$expectedCesiumDescriptorVersionName = '2.18.0'",
            "$expectedCesiumDescriptorEngineVersion = '5.5.0'",
            "$expectedCesiumDescriptorBytes = 1214L",
            "Get-VerifiedCesiumPluginDescriptor",
            "CesiumForUnreal.uplugin",
            "exactly one unambiguous CesiumForUnreal.uplugin descriptor",
            "$cesiumPlugin=Get-VerifiedCesiumPluginDescriptor",
            "CesiumPluginDescriptor=$cesiumPlugin",
            "CesiumDescriptorIdentityPinned=$true",
        ):
            self.assertIn(token, self.wrapper)

    def test_exact_six_receipt_chain_and_human_review_are_fail_closed(self) -> None:
        receipt = self.contract["nativeTransaction"]["predecessorReceipts"]
        self.assertEqual(receipt["exactCount"], 6)
        self.assertFalse(receipt["currentR30MechanicalOnlyReceiptAdmissible"])
        self.assertEqual(
            receipt["r31CaptureSchema"],
            "triad.istana_explore_v5d.r31_player0_capture.v2",
        )
        self.assertEqual(receipt["r31ExactPoseCount"], 5)
        self.assertEqual(receipt["r31ExactImageCount"], 6)
        self.assertTrue(receipt["r31NaniteRasterPairReviewRequired"])
        self.assertTrue(
            receipt["r31HumanNaniteRasterComparisonAttestationRequired"]
        )
        self.assertTrue(
            receipt["r31NaniteRasterAppearanceParityAcceptanceRequired"]
        )
        self.assertTrue(receipt["r31SixPngAcceptanceRevalidationRequired"])
        for token in (
            "$r30TransactionSchema",
            "$r30CaptureSchema",
            "$r31TransactionSchema",
            "$r31CaptureSchema",
            "$r32TransactionSchema",
            "$r32CaptureSchema",
            "Read-HashPinnedCommitReceipt",
            "Assert-SixPredecessorReceipts",
            "ExplicitHumanReviewAcceptance",
            "AutomaticVisualAcceptanceAllowed",
            "VisualReviewRequired",
            "VisualReviewAccepted",
            "ConfirmedFiveImagesReviewed",
            "ConfirmedNaniteRasterPairReviewed",
            "HumanNaniteRasterComparisonAttested",
            "NaniteRasterAppearanceParityAccepted",
            "SixPngsRedecodedAndRehashed",
            "Assert-R31V2NaniteRasterCaptureTruth",
            "ConfirmedTenImagesReviewed",
            "R31AdmissionAuthorized",
            "R32AdmissionAuthorized",
            "R33AdmissionAuthorized",
        ):
            self.assertIn(token, self.wrapper)
        self.assertIn(
            "R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31", self.wrapper
        )
        self.assertIn(
            "R31_COMMIT_THEN_R31_CAPTURE_BEFORE_R32", self.wrapper
        )
        self.assertIn(
            "R32_COMMIT_THEN_R32_CAPTURE_BEFORE_R33", self.wrapper
        )
        self.assertIn("CurrentMechanicalOnlyR30ReceiptAdmissible=$false", self.wrapper)

    def test_caller_pins_map_dll_ground_and_full_source_tree(self) -> None:
        for token in (
            "ExpectedMapBytes",
            "ExpectedMapSha256",
            "ExpectedRuntimeDllBytes",
            "ExpectedRuntimeDllSha256",
            "ExpectedEditorDllBytes",
            "ExpectedEditorDllSha256",
            "ExpectedGroundHeaderBytes",
            "ExpectedGroundHeaderSha256",
            "ExpectedGroundSourceBytes",
            "ExpectedGroundSourceSha256",
            "ExpectedNativePluginSourceTreeSha256",
            "Get-TreeIdentitySha256",
            "Assert-TreeIdentity",
            "NativePluginSourceTreeBefore",
            "NativePluginSourceTreeAfter",
        ):
            self.assertIn(token, self.wrapper)
        self.assertIn("249537L", self.wrapper)
        self.assertIn(
            "8B71713A4C134132BEB7ECE6DDCB93F86E690BB073F4508E325CD1BDAC5C520C",
            self.wrapper,
        )

    def test_promotion_roster_is_exact_and_contains_no_content_package(self) -> None:
        expected = (
            "TRIADIstanaExploreV5DContextPolicyActor.h",
            "TRIADIstanaExploreV5DContextPolicyActor.cpp",
            "TRIADIstanaExploreV5DGroundVegetationActor.h",
            "TRIADIstanaExploreV5DGroundVegetationActor.cpp",
            "TRIADIstanaExploreV5DR29CopernicusTerrainFallbackActor.cpp",
            "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.h",
            "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor.cpp",
            "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceEditorLibrary.h",
            "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceEditorLibrary.cpp",
            "r33_cesium_world_terrain_reference.contract.json",
        )
        for name in expected:
            self.assertIn(name, self.wrapper)
        self.assertIn("CodeSourceCount=9", self.wrapper)
        self.assertIn("SourceContractCount=1", self.wrapper)
        self.assertIn("NativeContentPackageCount=0", self.wrapper)
        self.assertIn("PromotedContentPackageCount=0", self.wrapper)
        self.assertIn(
            "F57C4C51F666290EF3C74CB03E4A536C476250CCC3AC3BD008D92456C824CA36",
            self.wrapper,
        )
        self.assertIn(
            "1F598910BC4D763BBBA04FD55DBE07D0F59B80078BB973924CDFBB40745E1DF7",
            self.wrapper,
        )

    def test_editor_library_adds_only_cwt_and_controller(self) -> None:
        for endpoint in (
            "ApplyR33CesiumWorldTerrainReferenceToLoadedV5DHybridMap",
            "CommitR33CesiumWorldTerrainReferenceToLoadedV5DHybridMap",
            "ValidateR33CesiumWorldTerrainReferenceInLoadedV5DHybridMap",
        ):
            self.assertIn(endpoint, self.editor_h)
            self.assertIn(endpoint, self.editor_cpp)
        for token in (
            "World->SpawnActor<ACesium3DTileset>",
            "ATRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor",
            "IstanaV5DR33AuthoredCoreClippingOverlay",
            "SetMaterial(Predecessor.Google->GetMaterial())",
            "SetTranslucentMaterial(",
            "ConfigureR33CesiumWorldTerrainReference(",
            "actorsAdded=2 actorsRemoved=0",
            "PromotedContentPackageCount",
        ):
            target = self.wrapper if token == "PromotedContentPackageCount" else self.editor_cpp
            self.assertIn(token, target)
        self.assertNotIn("NewObject<UStaticMesh", self.editor_cpp)
        self.assertNotIn("CreatePackage(", self.editor_cpp)

    def test_editor_predecessor_and_successor_validation_are_cold_safe(self) -> None:
        for token in (
            "ValidateR32MediumDistanceTurfInLoadedV5DHybridMap",
            "R33NativeValidateR32Predecessor",
            "R33NativeValidateSuccessorWorld",
            "TotalTilesetCount != 1",
            "TotalTilesetCount != 2",
            "GoogleRoleCount != 1",
            "CwtRoleCount != 1",
            "GeoreferenceCount != 1",
            "ContextMemberCount != 2",
            "ExpectedGoogleIonAssetId",
            "ExpectedCwtIonAssetId",
            "GetCesiumIonServer() !=",
            "!IsValid(OutRoster.Google->GetCesiumIonServer())",
            "!IsValid(OutRoster.Cwt->GetCesiumIonServer())",
            "Cwt->GetMaterial() != OutRoster.Google->GetMaterial()",
            "Cwt->GetTranslucentMaterial() !=",
            "TArray<UCesiumRasterOverlay*> GoogleRasterOverlays",
            "TArray<UCesiumRasterOverlay*> CwtRasterOverlays",
            "GoogleRasterOverlays.Num() != 1",
            "CwtRasterOverlays.Num() != 1",
            "GoogleRasterOverlays[0] != GoogleOverlays[0]",
            "CwtRasterOverlays[0] != CwtOverlays[0]",
            "ValidateR33CesiumWorldTerrainReference(",
        ):
            self.assertIn(token, self.editor_cpp)
        self.assertEqual(self.editor_cpp.count("UEditorLoadingAndSavingUtils::SaveMap("), 1)
        self.assertIn("UEditorLoadingAndSavingUtils::NewBlankMap(false)", self.editor_cpp)
        self.assertIn("UEditorLoadingAndSavingUtils::LoadMap", self.editor_cpp)

        successor_start = self.editor_cpp.index(
            "bool R33NativeValidateSuccessorWorld("
        )
        successor_end = self.editor_cpp.index(
            "bool R33NativeUndoAndValidateR32(", successor_start
        )
        successor = self.editor_cpp[successor_start:successor_end]
        register = successor.index("RegisterR33CesiumWorldTerrainController(")
        inherited = successor.index(
            "ValidateR32MediumDistanceTurfInLoadedV5DHybridMap"
        )
        validate = successor.index("ValidateR33CesiumWorldTerrainReference(")
        self.assertLess(register, inherited)
        self.assertLess(inherited, validate)
        self.assertLess(register, validate)
        self.assertIn("transient binding reconstruction failed", successor)
        scope_exit = successor.index("ON_SCOPE_EXIT", register)
        unregister = successor.index(
            "UnregisterR33CesiumWorldTerrainController(", scope_exit
        )
        keep = successor.index("bKeepReconstructedBinding = true", unregister)
        success_report = successor.index(
            "ISTANA_EXPLORE_V5D_R33_CESIUM_WORLD_TERRAIN_REFERENCE_MAP_VALID",
            keep,
        )
        self.assertLess(register, scope_exit)
        self.assertLess(scope_exit, unregister)
        self.assertLess(unregister, inherited)
        self.assertLess(validate, keep)
        self.assertLess(keep, success_report)

    def test_wrapper_invokes_exactly_one_commit_between_cold_validations(self) -> None:
        commit = "CommitR33CesiumWorldTerrainReferenceToLoadedV5DHybridMap"
        self.assertEqual(self.wrapper.count(commit), 1)
        first = self.wrapper.index("01_cold_validate_exact_captured_r32_predecessor")
        commit_offset = self.wrapper.index(commit)
        successor = self.wrapper.index(
            "03_cold_validate_r33_cesium_world_terrain_reference_successor"
        )
        self.assertLess(first, commit_offset)
        self.assertLess(commit_offset, successor)
        self.assertIn("CommitR33EndpointInvocationCount=1", self.wrapper)
        self.assertIn("StandaloneApplyR33EndpointInvocationCount=0", self.wrapper)

    def test_memory_process_quiescence_and_rollback_guards_are_fixed(self) -> None:
        for token in (
            "10737418240L # fixed 10 GiB",
            "12884901888L # fixed 12 GiB",
            "6442450944L # fixed continuous 6 GiB",
            "$memoryPollMilliseconds = 500",
            "$memoryPersistentBreachMilliseconds = 2000",
            "-MaxParallelActions=1",
            "-NoUBA",
            "-NoUBALocal",
            "Assert-ProtectedUnchanged",
            "Assert-NativeIdle",
            "Wait-NativeMutationQuiescence",
            "before R33 filesystem rollback restoration",
            "Restore-FileJournal $mapJournal",
            "Restore-TreeJournal $buildJournal",
            "Restore-FileJournal $sourceJournal",
        ):
            self.assertIn(token, self.wrapper)
        self.assertIn("-WindowStyle Hidden", self.wrapper)
        self.assertNotIn("Stop-Process -Name UnrealEditor", self.wrapper)

    def test_receipt_keeps_visual_and_accuracy_claims_false(self) -> None:
        for token in (
            "VisualReferenceOnly=$true",
            "AccurateRealWorldTerrainClaimed=$false",
            "SurveyAccuracyClaimed=$false",
            "VerticalDatumResolved=$false",
            "HeightCheckpointValidationComplete=$false",
            "SimulationCollisionNavigationSensorRfAuthority=$false",
            "VisualCaptureAccepted=$false",
            "CaptureRevalidationRequired=$true",
            "GooglePhotorealisticIonAssetId=2275207L",
            "CesiumWorldTerrainIonAssetId=1L",
            "NonNullOpaqueIonServerRequired=$true",
            "IonTokenValueOrFingerprintInspectedByTriad=$false",
            "NonNullServerProvesTokenOrEntitlement=$false",
        ):
            self.assertIn(token, self.wrapper)

    def test_static_self_check_is_repository_only(self) -> None:
        completed = subprocess.run(
            [
                "pwsh",
                "-NoProfile",
                "-File",
                str(WRAPPER_PATH),
                "-RunToken",
                "contract_test",
                "-StaticSelfCheck",
            ],
            cwd=REPO,
            check=True,
            capture_output=True,
            text=True,
        )
        receipt = json.loads(completed.stdout)
        self.assertEqual(receipt["Status"], "STATIC_SELF_CHECK_PASS")
        self.assertEqual(receipt["CodeSourceCount"], 9)
        self.assertEqual(receipt["SourceContractCount"], 1)
        self.assertEqual(receipt["PredecessorReceiptParserCount"], 6)
        self.assertFalse(receipt["CurrentMechanicalOnlyR30ReceiptAdmissible"])
        self.assertFalse(receipt["NativeProjectAccessed"])
        self.assertFalse(receipt["NativeTreeWritten"])
        self.assertFalse(receipt["UnrealLaunched"])
        self.assertTrue(receipt["NonNullOpaqueIonServerRequired"])
        self.assertFalse(receipt["IonTokenValueOrFingerprintInspected"])
        self.assertFalse(receipt["NonNullServerProvesTokenOrEntitlement"])

    def test_readme_discloses_current_gate_and_no_native_claim(self) -> None:
        for phrase in (
            "repository source only",
            "explicitly human-accept",
            "mechanically accepted R30 capture receipt is not enough",
            "exactly nine C++ files and this contract",
            "visualCaptureAccepted=false",
            "claims no survey-grade",
        ):
            self.assertIn(phrase, self.readme)
        self.assertRegex(self.readme, r"with no\s+content package")


if __name__ == "__main__":
    unittest.main()
