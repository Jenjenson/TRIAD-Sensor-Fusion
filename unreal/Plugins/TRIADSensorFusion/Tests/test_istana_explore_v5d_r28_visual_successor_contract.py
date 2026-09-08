from __future__ import annotations

import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal" / "Plugins" / "TRIADSensorFusion"
HYBRID_H = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Public/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.h"
)
HYBRID_CPP = (
    PLUGIN
    / "Source/TRIADSensorFusionEditor/Private/"
    "TRIADIstanaExploreV5DHybridEditorLibrary.cpp"
)


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


class IstanaExploreV5DR28VisualSuccessorContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for path in (HYBRID_H, HYBRID_CPP):
            if not path.is_file():
                raise AssertionError(f"missing R28 visual-successor source: {path}")
        cls.header = HYBRID_H.read_text(encoding="utf-8")
        cls.source = HYBRID_CPP.read_text(encoding="utf-8")
        cls.world_validator = between(
            cls.source,
            "bool ValidateR28VisualSuccessorWorld(",
            "bool HasStableHybridVisualPolicy(",
        )
        cls.apply = between(
            cls.source,
            "ApplyIstanaExploreV5DR28VisualSuccessorToLoadedHybridMap(",
            "ValidateIstanaExploreV5DR28VisualSuccessorMap(",
        )
        cls.map_validator = between(
            cls.source,
            "ValidateIstanaExploreV5DR28VisualSuccessorMap(",
            "ValidateIstanaExploreV5DHybridPlayWorld(",
        )

    def test_public_reflected_endpoints_and_environment_seams_are_exact(self):
        combined = self.header + self.source
        for token in (
            "ApplyIstanaExploreV5DR28VisualSuccessorToLoadedHybridMap(",
            "int64 ExpectedPredecessorBytes",
            "const FString& ExpectedPredecessorSha256",
            "const FString& VerifiedExternalBackupFilename",
            "FString& OutMessage",
            "ValidateIstanaExploreV5DR28VisualSuccessorMap(",
            "FString& OutReport",
            '#include "TRIADIstanaExploreV5DR28EnvironmentActor.h"',
            '#include "TRIADIstanaExploreV5DR28EnvironmentEditorLibrary.h"',
        ):
            self.assertIn(token, combined)
        self.assertEqual(
            2,
            combined.count(
                "ApplyIstanaExploreV5DR28VisualSuccessorToLoadedHybridMap("
            ),
        )
        self.assertEqual(
            2,
            combined.count("ValidateIstanaExploreV5DR28VisualSuccessorMap("),
        )

    def test_combined_world_validation_defers_only_historical_landmark_check(self):
        for token in (
            "ValidateR28EnvironmentAssets(EnvironmentAssetReport)",
            "ValidateReusableLandmarkVegetationAssetsR28(",
            "TRIADIstanaExploreV5DTemasekShophouseAssetFactory::ValidateAssets(",
            "TRIADIstanaExploreV5DContextFacadeR25AssetFactory::ValidateAssets(",
            "ELandmarkPresencePolicy::Required,\n"
            "            ELandmarkPresencePolicy::Required,\n"
            "            ELandmarkPresencePolicy::Required,\n"
            "            false",
            "Environment->ValidateR28Environment(EnvironmentReport)",
            "LandmarkVegetation->ValidateLandmarkVegetationR28(",
            "ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene(",
            "ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_WORLD_VALID",
        ):
            self.assertIn(token, self.world_validator)
        self.assertNotIn(
            "ValidateLandmarkVegetation(LandmarkReport)", self.world_validator
        )

    def test_combined_world_validation_retains_exact_roster_and_negative_authority(self):
        for token in (
            "SceneCount != 1",
            "PolicyCount != 1",
            "EnvironmentCount != 1",
            "LandmarkVegetationCount != 1",
            "ATRIADIstanaExploreV5DR28EnvironmentActor::StaticClass()",
            "ATRIADIstanaExploreV5DLandmarkVegetationActor::StaticClass()",
            "Environment->bProviderReady !=",
            "Policy->bLocalBuildingFallbackCurrentlyHidden",
            "Policy->bCesiumLayerIsVisualOnly",
            "!Policy->bCesiumCollisionNavigationSensorOrRfAuthority",
            "!Policy->bTriadReadSerializedOrLoggedProviderToken",
            "!Policy->bTriadExportedGeometricallyTracedAnalysedDerivedOrBakedProviderContent",
            "providerVisualOnly=true",
            "providerCollisionNavigationSensorRfAuthority=false",
            "providerTokenReadSerializedOrLogged=false",
            "providerContentExportedGeometricallyTracedAnalysedDerivedOrBaked=false",
        ):
            self.assertIn(token, self.world_validator)

    def test_idempotence_precedes_every_predecessor_pin_gate(self):
        idempotence = self.apply.index(
            "ValidateR28VisualSuccessorWorld(World, ExistingR28Report)"
        )
        pins = self.apply.index("ExpectedPredecessorSha256.ToUpper()")
        backup = self.apply.index(
            "TRIAD/NativeTransactions/V5DVisualRealismR28V1"
        )
        predecessor = self.apply.index(
            "ValidateLandmarkVegetationR27World("
        )
        mutation_gate = self.apply.index(
            "EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_REFUSED_FINAL_MUTATION_GATE"
        )
        self.assertLess(idempotence, pins)
        self.assertLess(pins, backup)
        self.assertLess(backup, predecessor)
        self.assertLess(predecessor, mutation_gate)
        self.assertIn(
            "IDEMPOTENT_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_ALREADY_VALID",
            self.apply,
        )

    def test_exact_r27_map_and_external_backup_are_required_before_mutation(self):
        for token in (
            "LogicalPackage != DestinationMapPackage",
            "Package->IsDirty()",
            "CurrentBytes != ExpectedPredecessorBytes",
            "CurrentSha256 != ExpectedSha256",
            "FPaths::IsSamePath(BackupFilename, DestinationFilename)",
            "FPaths::IsUnderDirectory(BackupFilename, AllowedBackupRoot)",
            "BackupBytes != ExpectedPredecessorBytes",
            "BackupSha256 != ExpectedSha256",
            "ValidateLandmarkVegetationR27World(",
            "EnvironmentCount != 0",
            "VegetationCount != 1",
            "MutationGateMapBytes != ExpectedPredecessorBytes",
            "MutationGateMapSha256 != ExpectedSha256",
            "MutationGateBackupBytes != ExpectedPredecessorBytes",
            "MutationGateBackupSha256 != ExpectedSha256",
        ):
            self.assertIn(token, self.apply)
        self.assertEqual(
            2,
            self.apply.count("V5DVisualRealismR28V1"),
            "backup root should occur once in the path gate and once in refusal telemetry",
        )

    def test_no_save_environment_seam_then_landmark_configuration_then_one_save(self):
        environment = self.apply.index(
            "ApplyR28EnvironmentToLoadedV5DHybridMap("
        )
        landmark = self.apply.index("ConfigureLandmarkVegetationActorR28(")
        pre_save_validation = self.apply.index(
            "ValidateR28VisualSuccessorWorld(World, PreSaveReport)"
        )
        save = self.apply.index("UEditorLoadingAndSavingUtils::SaveMap(")
        self.assertLess(environment, landmark)
        self.assertLess(landmark, pre_save_validation)
        self.assertLess(pre_save_validation, save)
        self.assertEqual(
            1, self.apply.count("UEditorLoadingAndSavingUtils::SaveMap(")
        )
        self.assertNotIn("SavePackage", self.apply)
        self.assertNotIn(
            "SpawnActor<ATRIADIstanaExploreV5DR28EnvironmentActor>", self.apply
        )
        self.assertNotIn("EnsureR28EnvironmentAssets", self.apply)

    def test_save_is_followed_by_true_unload_reload_and_combined_cold_validation(self):
        save = self.apply.index("UEditorLoadingAndSavingUtils::SaveMap(")
        unload = self.apply.index(
            "UEditorLoadingAndSavingUtils::LoadMap(SourceFilename)", save
        )
        blank = self.apply.index(
            "UEditorLoadingAndSavingUtils::NewBlankMap(false)", unload
        )
        reload_target = self.apply.index(
            "UEditorLoadingAndSavingUtils::LoadMap(DestinationFilename)", unload
        )
        cold = self.apply.index(
            "ValidateR28VisualSuccessorWorld(ReloadedTarget, ColdReport)",
            reload_target,
        )
        receipt = self.apply.index("bSuccessorValid", cold)
        self.assertLess(save, unload)
        self.assertLess(unload, blank)
        self.assertLess(blank, reload_target)
        self.assertLess(reload_target, cold)
        self.assertLess(cold, receipt)
        for token in (
            "RecheckedBackupBytes == ExpectedPredecessorBytes",
            "RecheckedBackupSha256 == ExpectedSha256",
            "EXPLORE_V5D_R28_VISUAL_SUCCESSOR_APPLY_PASS",
            "oneSave=true",
            "visualCaptureAccepted=false",
            "captureRevalidationRequired=true",
        ):
            self.assertIn(token, self.apply)

    def test_public_validator_requires_exact_clean_target_and_disk_receipt(self):
        for token in (
            "LogicalPackage != DestinationMapPackage",
            "Package->IsDirty()",
            "ValidateR28VisualSuccessorWorld(World, SemanticReport)",
            "FPackageName::DoesPackageExist(",
            "HashFileSha256(",
            "ISTANA_EXPLORE_V5D_R28_VISUAL_SUCCESSOR_MAP_VALID",
            "cleanSavedMap=true",
            "visualCaptureAccepted=false",
            "captureRevalidationRequired=true",
        ):
            self.assertIn(token, self.map_validator)


if __name__ == "__main__":
    unittest.main()
