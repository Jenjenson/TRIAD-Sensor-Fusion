from __future__ import annotations

import hashlib
import json
import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal/Plugins/TRIADSensorFusion"
EDITOR_H = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary.h"
)
EDITOR_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary.cpp"
)
ASSET_FACTORY_H = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory.h"
)
ASSET_FACTORY_CPP = ASSET_FACTORY_H.with_suffix(".cpp")
GROUND_H = (
    PLUGIN
    / "Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DGroundVegetationActor.h"
)
GROUND_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DGroundVegetationActor.cpp"
)
CONTRACT = (
    REPO
    / "unreal/SourceAssets/IstanaPublicViewExploreV5D/Vegetation/"
    "R32MediumDistanceTurf/r32_medium_distance_turf.contract.json"
)
README = CONTRACT.with_name("README.md")
WRAPPER = (
    REPO
    / "scripts/Invoke-IstanaExploreV5DMediumDistanceTurfR32NativeTransactionV1.ps1"
)
OVERLAY_ROOT = CONTRACT.parent / "NativeSourceClosure"
OVERLAY_GROUND_H = OVERLAY_ROOT / "TRIADIstanaExploreV5DGroundVegetationActor.h"
OVERLAY_GROUND_CPP = OVERLAY_ROOT / "TRIADIstanaExploreV5DGroundVegetationActor.cpp"

EXPECTED_OVERLAY_FILES = (
    {
        "repositoryPath": (
            "unreal/SourceAssets/IstanaPublicViewExploreV5D/Vegetation/"
            "R32MediumDistanceTurf/NativeSourceClosure/"
            "TRIADIstanaExploreV5DGroundVegetationActor.h"
        ),
        "nativeDestination": (
            "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/"
            "TRIADIstanaExploreV5DGroundVegetationActor.h"
        ),
        "bytes": 24_974,
        "sha256": "95122779D264BB0539AEC25A93D88C6485E5D4DB89E8F50B855D4EF1C82302E2",
    },
    {
        "repositoryPath": (
            "unreal/SourceAssets/IstanaPublicViewExploreV5D/Vegetation/"
            "R32MediumDistanceTurf/NativeSourceClosure/"
            "TRIADIstanaExploreV5DGroundVegetationActor.cpp"
        ),
        "nativeDestination": (
            "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
            "TRIADIstanaExploreV5DGroundVegetationActor.cpp"
        ),
        "bytes": 249_537,
        "sha256": "8B71713A4C134132BEB7ECE6DDCB93F86E690BB073F4508E325CD1BDAC5C520C",
    },
)

def region(source: str, start: str, end: str) -> str:
    start_index = source.find(start)
    if start_index < 0:
        raise AssertionError(f"missing region start: {start}")
    end_index = source.find(end, start_index)
    if end_index < 0:
        raise AssertionError(f"missing region end: {end}")
    return source[start_index:end_index]


class R32MediumDistanceTurfNativeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        for path in (
            EDITOR_H,
            EDITOR_CPP,
            ASSET_FACTORY_H,
            ASSET_FACTORY_CPP,
            GROUND_H,
            GROUND_CPP,
            CONTRACT,
            README,
            OVERLAY_GROUND_H,
            OVERLAY_GROUND_CPP,
            WRAPPER,
        ):
            if not path.is_file():
                raise AssertionError(f"missing R32 native source input: {path}")
        cls.header = EDITOR_H.read_text(encoding="utf-8")
        cls.source = EDITOR_CPP.read_text(encoding="utf-8")
        cls.asset_factory_header = ASSET_FACTORY_H.read_text(encoding="utf-8")
        cls.asset_factory_source = ASSET_FACTORY_CPP.read_text(encoding="utf-8")
        cls.ground_header_bytes = GROUND_H.read_bytes()
        cls.ground_header = GROUND_H.read_text(encoding="utf-8")
        cls.ground_source = GROUND_CPP.read_text(encoding="utf-8")
        cls.overlay_ground_header_bytes = OVERLAY_GROUND_H.read_bytes()
        cls.overlay_ground_source_bytes = OVERLAY_GROUND_CPP.read_bytes()
        cls.overlay_ground_header = cls.overlay_ground_header_bytes.decode("utf-8")
        cls.overlay_ground_source = cls.overlay_ground_source_bytes.decode("utf-8")
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
        cls.readme = README.read_text(encoding="utf-8")
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")

    def test_minimal_blueprint_editor_surface_is_exact(self) -> None:
        self.assertIn(
            "class TRIADSENSORFUSIONEDITOR_API\n"
            "    UTRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary",
            self.header,
        )
        endpoints = (
            "EnsureR32MediumDistanceTurfMaterials",
            "ValidateR32MediumDistanceTurfMaterials",
            "ApplyR32MediumDistanceTurfToLoadedV5DHybridMap",
            "CommitR32MediumDistanceTurfToLoadedV5DHybridMap",
            "ValidateR32MediumDistanceTurfInLoadedV5DHybridMap",
        )
        self.assertEqual(5, self.header.count("UFUNCTION(BlueprintCallable"))
        for endpoint in endpoints:
            self.assertIn(endpoint, self.header)
            self.assertIn(endpoint, self.source)
        for forbidden in ("BuildR32Assets", "CaptureR32"):
            self.assertNotIn(forbidden, self.header)

    def test_predecessor_is_exact_r31_r23_and_zero_r32(self) -> None:
        predecessor = region(
            self.source,
            "bool R32NativeValidateR31Predecessor(",
            "bool R32NativeValidateSuccessorWorld(",
        )
        for token in (
            "ValidateR31BroadShellInLoadedV5DHybridMap",
            "GroundClassCount != 1",
            "GroundTagCount != 1",
            "R32NativeGroundVegetationTag",
            "ExpectedGrassPresentationRevision()",
            "ValidateGroundVegetationRealism",
            "ValidateOwnedGrassInstanceTransforms",
            "R32ClassCount != 0",
            "R32TagCount != 0",
            "R32NativeLoadExactCleanAssetRoster",
            "exact R30 commit plus accepted five-pose capture",
            "exact R31 commit plus accepted native capture",
            "receiptGateOwnedByExternalWrapper=true",
        ):
            self.assertIn(token, predecessor)

        resolver = region(
            self.source,
            "bool R32NativeResolveWorldRoster(",
            "bool R32NativeLoadExactCleanAssetRoster(",
        )
        for token in (
            "R32NativeGroundVegetationTag",
            "TRIADIstanaExploreV5DGroundVegetationActor::StaticClass()",
            "ATRIADIstanaExploreV5DR32MediumDistanceTurfActor::StaticClass()",
            "bGroundFamily && !bExactGround",
            "bTaggedGround && !bExactGround",
            "bR32Family && !bExactR32",
            "bTaggedR32 && !bExactR32",
        ):
            self.assertIn(token, resolver)
        self.assertIn(
            'TEXT("TRIADIstanaExploreV5DGroundVegetation")', self.source
        )

    def test_canonical_factory_supplies_r29_meshes_and_isolated_r32_materials(self) -> None:
        loader = region(
            self.source,
            "bool R32NativeLoadExactCleanAssetRoster(",
            "bool R32NativeValidateR31Predecessor(",
        )
        for token in (
            "TRIADIstanaExploreV5DR29VegetationAssetFactory::",
            "TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory::",
            "LoadValidatedRuntimeContract",
            "GetExpectedAssetObjectPaths",
            "GetExpectedMaterialObjectPaths",
            "FinishAllCompilation",
            "OutAssets.GrassMeshVariants.Num() != 3",
            "OutAssets.GrassProfileMaterials.Num() != 4",
            "ExpectedR29MeshPaths.Remove(Path)",
            "UnmatchedR32MaterialPaths.Remove(Path)",
            "Package->IsDirty()",
            "FPackageName::DoesPackageExist(Package->GetName())",
            "ExactPackages.Contains(Package)",
            "ExpectedR29Paths.Num() != 7",
            "ExpectedR32Paths.Num() != 4",
            "!UnmatchedR32MaterialPaths.IsEmpty()",
            "ExactPackages.Num() != 7",
            "ValidateAssetRoster",
            "r29Meshes=3 isolatedR32Materials=4 persisted=true dirty=false",
        ):
            self.assertIn(token, loader)
        for forbidden in (
            "LoadObject<UStaticMesh>",
            "LoadObject<UMaterialInterface>",
            "R32NativeGrassMeshPaths",
            "R32NativeGrassMaterialPaths",
            "R32NativeGrassMeshPaths",
            "R32NativeGrassMaterialPaths",
        ):
            self.assertNotIn(forbidden, loader)

        for token in (
            "exactSavedPackages=4",
            "allowedGraphDeltas=baseColourBodyAndLabel,roughnessBodyAndLabel",
            "continuousBroadScaleMeters=11.0",
            "continuousMesoScaleMeters=3.4",
            "mediumResponseMeters=20,65",
            "opacityModified=false",
            "wpoModified=false",
            "normalResponseModified=false",
            "sourcePackagesModified=false",
        ):
            self.assertIn(token, self.asset_factory_source)

    def test_apply_is_add_only_transactional_and_undo_validated(self) -> None:
        apply = region(
            self.source,
            "ApplyR32MediumDistanceTurfToLoadedV5DHybridMap(FString& OutMessage)",
            "ValidateR32MediumDistanceTurfInLoadedV5DHybridMap",
        )
        for token in (
            "R32NativeValidateSuccessorWorld",
            "IDEMPOTENT_EXPLORE_V5D_R32_MEDIUM_DISTANCE_TURF_ALREADY_VALID",
            "transactionOpened=false",
            "actorSpawned=false",
            "R32NativeValidateR31Predecessor",
            "FScopedTransaction",
            "Transaction->IsOutstanding()",
            "World->Modify()",
            "SpawnParameters.Name = R32NativeActorObjectName",
            "SpawnParameters.OverrideLevel = World->PersistentLevel",
            "SpawnParameters.ObjectFlags |= RF_Transactional",
            "FTransform::Identity",
            "ConfigureR32MediumDistanceTurf",
            "R32NativeValidateSuccessorWorld",
            "R32NativeUndoAndValidateR31",
            "Transaction.Reset()",
            "mapSaved=false",
            "addOnly=true",
            "sourceActorModified=false",
            "sourceAssetsModified=false",
            "otherOwnersModified=false",
            "externalR30R31ReceiptChainRequiredBeforeInvocation=true",
        ):
            self.assertIn(token, apply)
        self.assertLess(
            apply.index("R32NativeValidateSuccessorWorld"),
            apply.index("R32NativeValidateR31Predecessor"),
        )
        self.assertLess(
            apply.index("IDEMPOTENT_EXPLORE_V5D_R32_MEDIUM_DISTANCE_TURF_ALREADY_VALID"),
            apply.index("FScopedTransaction"),
        )
        self.assertLess(
            apply.index("Transaction->IsOutstanding()"),
            apply.index("World->Modify()"),
        )
        for forbidden in (
            "DestroyActor(",
            "Predecessor.Ground->Modify()",
            "SaveMap(",
            "SetVisibility(",
            "SetHiddenInGame(",
            "ClearInstances(",
        ):
            self.assertNotIn(forbidden, apply)

        undo = region(
            self.source,
            "bool R32NativeUndoAndValidateR31(",
            "} // namespace",
        )
        for token in (
            "Transaction->IsOutstanding()",
            "GEditor->UndoTransaction(false)",
            "R32NativeValidateR31Predecessor",
            "Restored.Ground == ExpectedGround",
            "SetDirtyFlag(bPackageWasDirty)",
            "WorldPackage->IsDirty() == bPackageWasDirty",
            "!bPredecessorRestored || !bDirtyStateRestored",
        ):
            self.assertIn(token, undo)
        self.assertLess(
            undo.index("R32NativeValidateR31Predecessor"),
            undo.index("SetDirtyFlag(bPackageWasDirty)"),
        )
        self.assertLess(
            undo.index("SetDirtyFlag(bPackageWasDirty)"),
            undo.index("!bPredecessorRestored || !bDirtyStateRestored"),
        )
        self.assertNotIn("Transaction.Cancel", self.source)
        self.assertNotIn("Cancel()", self.source)

    def test_successor_has_one_exact_identity_persistent_level_owner(self) -> None:
        successor = region(
            self.source,
            "bool R32NativeValidateSuccessorWorld(",
            "bool R32NativeUndoAndValidateR31(",
        )
        for token in (
            "ValidateR31BroadShellInLoadedV5DHybridMap",
            "GroundClassCount != 1",
            "GroundTagCount != 1",
            "R32ClassCount != 1",
            "R32TagCount != 1",
            "GetLevel() != World->PersistentLevel",
            "GetFName() != R32NativeActorObjectName",
            "SourceGroundVegetation != OutRoster.Ground",
            "FTransform::Identity",
            "ValidateR32MediumDistanceTurf",
            "R32NativeLoadExactCleanAssetRoster",
            "ownedHisms=12 selectedTransforms=4608",
            "visualCaptureAccepted=false",
        ):
            self.assertIn(token, successor)

    def test_commit_has_two_pin_gates_one_save_and_cold_validation(self) -> None:
        commit = self.source.split(
            "CommitR32MediumDistanceTurfToLoadedV5DHybridMap(", 1
        )[1]
        for token in (
            "ExpectedPredecessorBytes <= 0",
            "R32NativeIsValidSha256(ExpectedSha256)",
            "World->GetOutermost()->IsDirty()",
            "R32NativeTransactionRelativeRoot",
            "FPaths::IsUnderDirectory(BackupFilename, TransactionRoot)",
            "MapBytes != ExpectedPredecessorBytes",
            "BackupBytes != ExpectedPredecessorBytes",
            "R32NativeValidateR31Predecessor",
            "ApplyR32MediumDistanceTurfToLoadedV5DHybridMap",
            "PreSaveMapBytes != ExpectedPredecessorBytes",
            "PreSaveBackupBytes != ExpectedPredecessorBytes",
            "UEditorLoadingAndSavingUtils::SaveMap",
            "UEditorLoadingAndSavingUtils::NewBlankMap(false)",
            "UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)",
            "ValidateR32MediumDistanceTurfInLoadedV5DHybridMap",
            "bSuccessorChanged",
            "bBackupPreserved",
            "oneMapSave=true materialPackageSaveCount=4 coldReload=true",
            "rollbackOwnedByWrapper=true",
        ):
            self.assertIn(token, commit)
        self.assertEqual(1, self.source.count("UEditorLoadingAndSavingUtils::SaveMap("))
        self.assertLess(
            commit.index("PreSaveMapBytes != ExpectedPredecessorBytes"),
            commit.index("UEditorLoadingAndSavingUtils::SaveMap"),
        )
        self.assertLess(
            commit.index("UEditorLoadingAndSavingUtils::SaveMap"),
            commit.index("UEditorLoadingAndSavingUtils::NewBlankMap(false)"),
        )

    def test_contract_and_readme_describe_implemented_but_unexecuted_wrapper(self) -> None:
        delivery = self.contract["deliveryState"]
        self.assertTrue(delivery["nativeEditorMapIntegrationImplemented"])
        self.assertTrue(delivery["nativeExecutionWrapperImplemented"])
        for field in (
            "nativeEditorMapIntegrationCompiled",
            "nativeProjectApplied",
            "targetMapMutated",
            "unrealEditorLaunchedForThisDelivery",
            "nativeCaptureProduced",
            "visualCaptureAccepted",
            "nativeExecutionEligibleNow",
        ):
            self.assertFalse(delivery[field], field)

        integration = self.contract["nativeEditorMapIntegration"]
        expected_true = (
            "exactTargetMapRequired",
            "nonPieEditorWorldRequired",
            "exactR31BroadShellPredecessorRequired",
            "exactTaggedR23GroundOwnerRequired",
            "groundOwnerClassExact",
            "addOnly",
            "applyUsesOutstandingScopedTransaction",
            "applyFailureUsesUndoAndRevalidatesR31",
            "applyFailureRestoresAndVerifiesOriginalPackageDirtyStateEvenIfPredecessorRevalidationFails",
            "commitRequiresCleanHashPinnedR31Preimage",
            "commitRequiresByteIdenticalExternalBackup",
            "commitColdReloadsAndRevalidates",
            "externalFileJournalRollbackStillRequired",
            "externalReceiptGateRequiredBeforeInvocation",
        )
        for field in expected_true:
            self.assertTrue(integration[field], field)
        self.assertEqual(7, integration["exactCleanPersistedR29PackageCount"])
        self.assertEqual(
            "TRIADIstanaExploreV5DR29VegetationAssetFactory",
            integration["sourceAssetAdmissionFactory"],
        )
        self.assertEqual(
            "TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory",
            integration["r32MaterialAssetFactory"],
        )
        self.assertEqual(
            "ValidateR32MediumDistanceTurfMaterials",
            integration["r32MaterialValidationMethod"],
        )
        self.assertEqual(
            "LoadValidatedRuntimeContract", integration["assetLoadMethod"]
        )
        self.assertTrue(integration["copiesOnlyOrderedR29GrassMeshArray"])
        self.assertTrue(integration["loadsOnlyOrderedR32MaterialDerivativeArray"])
        self.assertEqual(4, integration["exactCleanPersistedR32MaterialPackageCount"])
        self.assertEqual(4, integration["materialPackageSaveCount"])
        self.assertEqual(1, integration["mapCommitSaveCount"])
        self.assertFalse(integration["sourceActorModifyCalled"])
        self.assertFalse(integration["sourceAssetsSaved"])
        self.assertFalse(integration["otherOwnersMutated"])
        self.assertFalse(integration["applySavesMap"])
        self.assertTrue(integration["applySemanticIdempotence"])
        self.assertFalse(integration["idempotentSuccessOpensTransaction"])
        self.assertFalse(integration["idempotentSuccessSpawnsActor"])
        self.assertFalse(integration["idempotentSuccessSavesMap"])
        self.assertFalse(integration["editorLibraryEstablishesCaptureAcceptance"])
        self.assertTrue(integration["nativeExecutionWrapperImplemented"])

        order = self.contract["nativeOrdering"]
        self.assertTrue(order["nativeEditorMapIntegrationImplemented"])
        self.assertTrue(order["nativeTransactionImplemented"])
        self.assertTrue(order["nativeExecutionWrapperImplemented"])
        self.assertFalse(order["nativeTransactionExecuted"])
        self.assertFalse(order["r31PredecessorPromotionCurrentlyAvailable"])
        self.assertTrue(order["r31PreR33ContextPolicyOverlayReclosureRequired"])

        flat_readme = " ".join(self.readme.split())
        for text in (
            "minimal editor map-integration library",
            "exact valid R31 broad shell",
            "exact seven clean persisted R29 source packages",
            "exact four clean isolated R32 material packages",
            "semantic no-op",
            "before opening a transaction",
            "one `RF_Transactional` identity actor",
            "single map save",
            "source-only wrapper",
            "four hash-pinned predecessor receipts",
            "nativeTransactionImplemented",
            "R31 wrapper source is now reclosed",
            "recovered immutable pre-R33 context-policy overlay",
            "R30 -> R31 (pre-R33 closure) -> R32 -> R33",
        ):
            self.assertIn(text, flat_readme)

    def test_r32_requires_minimal_pre_r33_ground_source_overlay(self) -> None:
        overlay = self.contract["preR33GroundVegetationCompileOverlay"]
        self.assertTrue(overlay["required"])
        self.assertTrue(overlay["implemented"])
        self.assertFalse(overlay["repositoryCanDeriveExactNativeBaseWithoutReceipt"])
        self.assertTrue(overlay["exactNativeR29PreR33BaseReceiptRequired"])
        self.assertTrue(overlay["liveNativePreimageCannotBeDerivedFromRepository"])
        self.assertTrue(overlay["versionedAndHashPinnedBeforeExecution"])
        self.assertTrue(overlay["nativeDestinationNamesExact"])
        self.assertTrue(overlay["overlayHeaderEqualsCurrentRepositoryHeaderExactly"])
        self.assertEqual(
            list(EXPECTED_OVERLAY_FILES), overlay["repositoryNativeMappings"]
        )
        self.assertEqual(
            "GetMediumDistanceTurfSourceProfilesR32",
            overlay["soleR32ApiDelta"],
        )
        self.assertEqual(
            [
                "SavedGrassManicured",
                "SavedGrassHumid",
                "SavedGrassShade",
                "SavedGrassDryEdge",
            ],
            overlay["orderedCopyOnlyOutputs"],
        )
        self.assertEqual(
            [
                "bR33DualCesiumContextConfigured",
                "ShouldHideSourceTerrainRendererForVisualContext",
                "TRIADIstanaExploreV5DR33CesiumWorldTerrainReferenceActor",
            ],
            overlay["forbiddenR33Symbols"],
        )
        self.assertFalse(overlay["stageCurrentRepositoryGroundSourcePair"])
        self.assertTrue(overlay["stageImmutableOverlaySourcePair"])
        self.assertFalse(overlay["stageR33ActorSourcePair"])

        self.assertIn(
            "GetMediumDistanceTurfSourceProfilesR32(", self.ground_header
        )
        self.assertIn(
            "GetMediumDistanceTurfSourceProfilesR32(", self.ground_source
        )
        self.assertEqual(
            2, self.ground_source.count("bR33DualCesiumContextConfigured")
        )

        overlay_files = (
            (OVERLAY_GROUND_H, self.overlay_ground_header_bytes),
            (OVERLAY_GROUND_CPP, self.overlay_ground_source_bytes),
        )
        for expected, (path, contents) in zip(EXPECTED_OVERLAY_FILES, overlay_files):
            self.assertEqual(expected["bytes"], len(contents), path)
            self.assertEqual(
                expected["sha256"], hashlib.sha256(contents).hexdigest().upper(), path
            )
            self.assertEqual(REPO / expected["repositoryPath"], path)
            self.assertEqual(
                f"TRIADIstanaExploreV5DGroundVegetationActor{path.suffix}",
                Path(expected["nativeDestination"]).name,
            )

        self.assertEqual(
            self.ground_header_bytes, self.overlay_ground_header_bytes
        )
        for source in (self.overlay_ground_header, self.overlay_ground_source):
            self.assertIn("GetMediumDistanceTurfSourceProfilesR32(", source)
            for forbidden in overlay["forbiddenR33Symbols"]:
                self.assertNotIn(forbidden, source)
        self.assertEqual(
            2,
            self.overlay_ground_source.count(
                "Policy->bLocalBuildingFallbackCurrentlyHidden"
            ),
        )

        order = self.contract["nativeOrdering"]
        self.assertTrue(order["r32PreR33GroundVegetationOverlayRequired"])
        self.assertTrue(order["r32PreR33GroundVegetationOverlayAvailable"])

        flat_readme = " ".join(self.readme.split())
        for text in (
            "R32 ground-source compile closure is now frozen",
            "24,974-byte copy",
            "249,537-byte private source",
            "two R33-era policy ternaries",
            "GetMediumDistanceTurfSourceProfilesR32",
            "contain no R33 symbol or actor reference",
            "exact native destinations",
            "has not been staged, compiled, executed, or captured",
        ):
            self.assertIn(text, flat_readme)

    def test_wrapper_freezes_exact_eight_cpp_plus_one_contract_roster(self) -> None:
        self.assertIn("$sourcePins.Count -ne 8", self.wrapper)
        self.assertIn("$sourceAssetPins.Count -ne 1", self.wrapper)
        self.assertIn("$retainedSourcePins.Count -ne 11", self.wrapper)
        for expected in EXPECTED_OVERLAY_FILES:
            repository = expected["repositoryPath"].removeprefix("unreal/").replace(
                "/", "\\"
            )
            native = expected["nativeDestination"].replace("/", "\\")
            self.assertIn(f"RepositoryRelativePath = '{repository}'", self.wrapper)
            self.assertIn(f"NativeRelativePath = '{native}'", self.wrapper)
            self.assertIn(f"Bytes = {expected['bytes']}L", self.wrapper)
            self.assertIn(expected["sha256"], self.wrapper)
        expected_new = (
            ("TRIADIstanaExploreV5DR32MediumDistanceTurfActor.h", 6_436,
             "8862523D7B9CCB4D6891B6A8F23728C1B95D9E16513A1DA936565F30C3164ACE"),
            ("TRIADIstanaExploreV5DR32MediumDistanceTurfActor.cpp", 35_336,
             "1B51CECF08785C889B63E95BD97B681AA065CBB36C734BC8AA2247EF7F6BD1D4"),
            ("TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory.h", 966,
             "CCD2EE99EE42BD7D7B322423E5671E49D5246E445FB0DDC0103AAED10BBCD082"),
            ("TRIADIstanaExploreV5DR32MediumDistanceTurfAssetFactory.cpp", 22_691,
             "90FE9E9E3152BFEFE8585A0BB643F5AF3B068852C63C496AC253F8DBC829CB16"),
            ("TRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary.h", 2_280,
             "4C5B1014E2842F7DA27431260352ABFF91F7E7841D6D404D30B7C79E21FA13E5"),
            ("TRIADIstanaExploreV5DR32MediumDistanceTurfEditorLibrary.cpp", 31_204,
             "C85E270797F08267A67101C6B53A4E4F9383E90FBE366C90F230FF871BA1B31D"),
        )
        for name, size, sha256 in expected_new:
            self.assertIn(name, self.wrapper)
            self.assertIn(f"Bytes = {size}L", self.wrapper)
            self.assertIn(sha256, self.wrapper)
        contract_bytes = CONTRACT.read_bytes()
        self.assertIn(f"Bytes = {len(contract_bytes)}L", self.wrapper)
        self.assertIn(hashlib.sha256(contract_bytes).hexdigest().upper(), self.wrapper)
        source_block = self.wrapper.split("$sourcePins = @(", 1)[1].split(
            "$sourceAssetPins = @(", 1
        )[0]
        self.assertEqual(2, source_block.count("NativeBeforePresent = $true"))
        self.assertEqual(6, source_block.count("NativeBeforePresent = $false"))
        self.assertIn("NativeBeforeBytes = $ExpectedGroundHeaderBytes", source_block)
        self.assertIn("NativeBeforeBytes = $ExpectedGroundSourceBytes", source_block)

    def test_wrapper_strictly_parses_and_cross_binds_four_receipts(self) -> None:
        for fragment in (
            "function Read-HashPinnedDirectCommitReceipt",
            "direct safe-token child commit.json",
            "triad.istana_explore_v5d.context_facade_lookdev_r30.native_transaction.v1",
            "triad.istana_explore_v5d.r30_player0_capture.v1",
            "triad.istana_explore_v5d.broad_shell_r31.native_transaction.v1",
            "triad.istana_explore_v5d.r31_player0_capture.v2",
            "R30_COMMIT_THEN_R30_CAPTURE_BEFORE_R31",
            "R31_COMMIT_THEN_R31_CAPTURE_BEFORE_R32",
            "R31BroadShellVisualQaAccepted",
            "ExplicitHumanReviewAcceptance",
            "ConfirmedFiveImagesReviewed",
            "ConfirmedNaniteRasterPairReviewed",
            "HumanNaniteRasterComparisonAttested",
            "NaniteRasterAppearanceParityAccepted",
            "NaniteRasterHighOccupancyComparison",
            "ExactImageCount",
            "R32AdmissionAuthorized",
            "PendingReceiptHashPinned",
            "SixPngsRedecodedAndRehashed",
            "PENDING_VISUAL_REVIEW",
            "$expectedPoseIds=@('075m','020m','008m','002m','surroundings_oblique_macdonald')",
            "R31 accepted capture PNG",
            "R32DependencyAllowed",
            "GroundHeader",
            "GroundSource",
            "NativePluginSourceTree",
            "Test-ReceiptFileBinding $r30CaptureCommit $r30CommitAdmission",
            "Test-ReceiptFileBinding (Get-RequiredReceiptProperty $r31Commit 'R30TransactionAdmission'",
            "Test-ReceiptFileBinding (Get-RequiredReceiptProperty $r31Commit 'R30CaptureAdmission'",
            "Test-ReceiptFileBinding $r31CaptureCommit $r31CommitAdmission",
            "Test-ReceiptStateMatches $groundHeader $ExpectedGroundHeaderBytes",
            "Test-ReceiptStateMatches $groundSource $ExpectedGroundSourceBytes",
            "Assert-TreeReceipt $nativePluginSourceRoot $admission.NativePluginSourceTree",
            "function Get-ExpectedR32PluginSourceTreeReceipt",
            "AcceptedR31Baseline.Count + 6",
            "Assert-TreeReceipt $nativePluginSourceRoot $expectedR32PluginSourceTree 'postflight exact R32 native plugin source'",
            "NativePluginSourceTreeBefore=@($admission.NativePluginSourceTree)",
            "NativePluginSourceTreeAfter=@($expectedR32PluginSourceTree)",
        ):
            self.assertIn(fragment, self.wrapper)

        r30_capture_block = self.wrapper.split(
            "$r30Capture = $r30CaptureAdmission.Receipt", 1
        )[1].split("$r30CaptureCommit =", 1)[0]
        for field in (
            "MechanicalCaptureValidationPassed",
            "ExplicitHumanReviewAcceptance",
            "ConfirmedFiveImagesReviewed",
            "HumanVisualReviewAttested",
            "VisualReviewAccepted",
            "R31AdmissionAuthorized",
        ):
            self.assertIn(
                f"'{field}' 'R30 capture receipt') -ne $true",
                r30_capture_block,
            )
        for field in (
            "AutomaticVisualAcceptanceAllowed",
            "VisualReviewRequired",
        ):
            self.assertIn(
                f"'{field}' 'R30 capture receipt') -ne $false",
                r30_capture_block,
            )

        r31_capture_block = self.wrapper.split(
            "$r31Capture = $r31CaptureAdmission.Receipt", 1
        )[1].split("$r31CaptureCommit =", 1)[0]
        self.assertIn(
            "'HumanVisualReviewAttested' 'R31 capture receipt') -ne $true",
            r31_capture_block,
        )
        for field in (
            "ConfirmedNaniteRasterPairReviewed",
            "HumanNaniteRasterComparisonAttested",
            "NaniteRasterAppearanceParityAccepted",
        ):
            self.assertIn(
                f"'{field}' 'R31 capture receipt') -ne $true",
                r31_capture_block,
            )

    def test_wrapper_has_no_r33_build_and_prestate_escape_hatch(self) -> None:
        for fragment in (
            "$r33ForbiddenNativePins",
            "Assert-NoR33NativeState",
            "GetMediumDistanceTurfSourceProfilesR32",
            "Caller-pinned R31 Ground preimage already contains the R32 source handoff.",
            "R31 Ground preimage contains forbidden R33 symbol",
            "R33 marker contaminated R32 build",
            "Assert-NativePreState $sourcePins",
            "Assert-NativePreState $sourceAssetPins",
            "Assert-NativePreState $retainedSourcePins",
        ):
            self.assertIn(fragment, self.wrapper)
        self.assertNotIn("repositoryCanDeriveExactNativeBaseWithoutReceipt = $true", self.wrapper)

    def test_wrapper_guards_build_processes_and_rollback(self) -> None:
        for fragment in (
            "$minimumSystemFreeVirtualAtLaunchBytes = 10737418240L",
            "$minimumSystemFreeVirtualBytes = 6442450944L",
            "$privateMemoryCeilingBytes = 12884901888L",
            "$memoryWatchdogPollMilliseconds = 500",
            "$memoryWatchdogPersistentBreachMilliseconds = 2000",
            "ContinuousMemoryWatchdog",
            "Start-ContinuousMemoryWatchdog",
            "Assert-ContinuousMemoryWatchdogHealthy",
            "Stop-ContinuousMemoryWatchdog",
            "Protected UE5.4 CAPSTONE process set changed.",
            "CreationDate -cne",
            "$Handle.Kill($true)",
            "'-MaxParallelActions=1'",
            "'-NoUBA'",
            "'-NoUBALocal'",
            "New-FileJournal $sourceDestinations",
            "New-FileJournal @($mapFile)",
            "New-TreeJournal @($pluginBinaryRoot,$pluginIntermediateRoot)",
            "Wait-NativeMutationQuiescence 'before R32 filesystem rollback restoration'",
            "Restore-FileJournal $mapJournal",
            "Restore-TreeJournal $buildJournal",
            "Restore-FileJournal $sourceJournal",
            "Remove-IsolatedR32Content",
            "ROLLBACK_INCOMPLETE",
        ):
            self.assertIn(fragment, self.wrapper)

    def test_wrapper_calls_commit_once_with_cold_validation_and_truthful_receipt(self) -> None:
        self.assertEqual(
            1,
            self.wrapper.count("'CommitR32MediumDistanceTurfToLoadedV5DHybridMap'"),
        )
        self.assertNotIn("'ApplyR32MediumDistanceTurfToLoadedV5DHybridMap'", self.wrapper)
        before = self.wrapper.index(
            "'01_cold_validate_exact_captured_r31_predecessor'"
        )
        commit = self.wrapper.index(
            "'04_commit_r32_medium_distance_turf_exactly_once'"
        )
        after = self.wrapper.index(
            "'05_cold_validate_r32_medium_distance_turf_successor'"
        )
        ensure = self.wrapper.index(
            "'02_ensure_r32_medium_distance_turf_materials'"
        )
        material_validation = self.wrapper.index(
            "'03_cold_validate_r32_medium_distance_turf_materials'"
        )
        self.assertLess(before, commit)
        self.assertLess(before, ensure)
        self.assertLess(ensure, material_validation)
        self.assertLess(material_validation, commit)
        self.assertLess(commit, after)
        for fragment in (
            "CommitR32EndpointInvocationCount=1",
            "StandaloneApplyR32EndpointInvocationCount=0",
            "R32ContentPackageCount=4",
            "R32MaterialPackages=@($r32ContentReceipt)",
            "ReusedCleanR29PackageCount=7",
            "SelectedTransformCount=4608",
            "OwnedHismCount=12",
            "SimulationCollisionNavigationSensorRfAuthority=$false",
            "SensorPlacementModified=$false",
            "DetectionSimulationModified=$false",
            "RfTruthModified=$false",
            "HyperrealismClaimed=$false",
            "PhysicalPbrMaterialTruthClaimed=$false",
            "SiteMeasuredMaterialTruthClaimed=$false",
            "VisualCaptureAccepted=$false",
            "CaptureRevalidationRequired=$true",
            "NativeProjectAccessed=$false",
            "NativeTreeWritten=$false",
            "UnrealLaunched=$false",
        ):
            self.assertIn(fragment, self.wrapper)

        transaction = self.contract["nativeTransaction"]
        self.assertTrue(transaction["implemented"])
        self.assertFalse(transaction["executed"])
        self.assertEqual(8, transaction["promotionCodeFileCount"])
        self.assertEqual(4, transaction["newR32MaterialPackageCount"])
        self.assertTrue(transaction["r32MaterialContentTreeReceiptRequired"])
        self.assertEqual(1, transaction["promotionContractFileCount"])
        self.assertTrue(transaction["groundSourcePreimagesCallerPinned"])
        self.assertTrue(transaction["r31CaptureMustCrossBindMapDllAndGroundPreimages"])
        self.assertTrue(transaction["wholePluginSourceTreeBoundBeforeAndAfter"])
        self.assertTrue(transaction["rollbackRequiresOwnedProcessQuiescence"])
        self.assertEqual(1, transaction["commitEndpointInvocationCount"])
        self.assertEqual(0, transaction["standaloneApplyEndpointInvocationCount"])

    def test_anonymous_namespace_symbols_are_r32_specific_for_unity_builds(self) -> None:
        top_level_helpers = re.findall(
            r"^(?:bool|FString|UWorld\*|struct)\s+([A-Za-z0-9_]+)",
            self.source,
            re.MULTILINE,
        )
        for name in top_level_helpers:
            if name.startswith("UTRIAD"):
                continue
            self.assertTrue(
                name.startswith("R32Native") or name.startswith("FR32Native"),
                name,
            )


if __name__ == "__main__":
    unittest.main()
