import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
PLUGIN = ROOT / "Plugins" / "TRIADSensorFusion"
RUNTIME = PLUGIN / "Source" / "TRIADSensorFusion"
EDITOR = PLUGIN / "Source" / "TRIADSensorFusionEditor"
ACTOR_CPP = (
    RUNTIME / "Private" / "TRIADIstanaExploreV5DLandmarkVegetationActor.cpp"
)
HYBRID_H = (
    EDITOR / "Public" / "TRIADIstanaExploreV5DHybridEditorLibrary.h"
)
HYBRID_CPP = (
    EDITOR / "Private" / "TRIADIstanaExploreV5DHybridEditorLibrary.cpp"
)


def function_body(source: str, name: str) -> str:
    name_index = source.find(name)
    if name_index < 0:
        raise AssertionError(f"missing function name: {name}")
    open_brace = source.find("{", name_index)
    if open_brace < 0:
        raise AssertionError(f"missing function body: {name}")
    depth = 0
    for index in range(open_brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[open_brace : index + 1]
    raise AssertionError(f"unterminated function body: {name}")


class LandmarkVegetationR26IntegrationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for path in (ACTOR_CPP, HYBRID_H, HYBRID_CPP):
            if not path.is_file():
                raise AssertionError(f"missing R26 integration source: {path}")
        cls.actor_cpp = ACTOR_CPP.read_text(encoding="utf-8")
        cls.hybrid_h = HYBRID_H.read_text(encoding="utf-8")
        cls.hybrid_cpp = HYBRID_CPP.read_text(encoding="utf-8")
        cls.apply_body = function_body(
            cls.hybrid_cpp,
            "ApplyIstanaExploreV5DLandmarkVegetationR26ToLoadedHybridMap",
        )
        cls.validate_body = function_body(
            cls.hybrid_cpp,
            "ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap",
        )

    def test_blueprint_endpoints_are_explicit_and_reflectable(self):
        for name, display_name in (
            (
                "ApplyIstanaExploreV5DLandmarkVegetationR26ToLoadedHybridMap",
                "Apply Istana Explore V5D Landmark Vegetation R26 To Loaded Hybrid Map",
            ),
            (
                "ValidateIstanaExploreV5DLandmarkVegetationR26SuccessorMap",
                "Validate Istana Explore V5D Landmark Vegetation R26 Successor Map",
            ),
        ):
            self.assertEqual(1, self.hybrid_h.count(name), name)
            self.assertEqual(1, self.hybrid_cpp.count(f"    {name}("), name)
            self.assertIn(f'meta = (DisplayName = "{display_name}")', self.hybrid_h)
        self.assertGreaterEqual(self.hybrid_h.count("UFUNCTION(BlueprintCallable"), 2)

    def test_r26_admits_only_the_exact_committed_r25_map(self):
        for token in (
            "constexpr int64 PreLandmarkVegetationR26Bytes = 34993427;",
            "38114240B7A0C673B2492B74DE7AA2EB22E9E5E87349E448310FC001688D89D9",
            "V5DLandmarkVegetationR26_20260906",
            "Istana_PublicView_Explore_v5d_hybrid_38114240B7A0.umap",
            "CreateVerifiedPreLandmarkVegetationR26MapBackup",
            "RestoreVerifiedPreLandmarkVegetationR26MapViaSiblingTemp",
        ):
            self.assertIn(token, self.hybrid_cpp)
        # The R25 predecessor and its immutable rollback receipt remain intact.
        for token in (
            "constexpr int64 PreContextFacadeR25Bytes = 34992354;",
            "4A5F5514C7C3B508567465BA1F3B2FE8F31F4DAAAC5E317C8C57F1C30B50FD08",
            "V5DContextFacadeR25_20260906",
        ):
            self.assertIn(token, self.hybrid_cpp)

    def test_shared_world_contract_accepts_but_validates_downstream_layer(self):
        signature = re.search(
            r"bool ValidateHybridWorld\(.*?\)\s*\{",
            self.hybrid_cpp,
            flags=re.DOTALL,
        )
        self.assertIsNotNone(signature)
        self.assertIn(
            "ELandmarkPresencePolicy LandmarkVegetationPolicy =\n"
            "        ELandmarkPresencePolicy::Optional",
            signature.group(0),
        )
        body = function_body(self.hybrid_cpp, "bool ValidateHybridWorld(")
        for token in (
            "FindExactlyOne<ATRIADIstanaExploreV5DLandmarkVegetationActor>",
            "MatchesLandmarkPresence(\n            LandmarkVegetationCount",
            "ATRIADIstanaExploreV5DLandmarkVegetationActor::\n"
            "                 ExpectedActorTag()",
            "ValidateLandmarkVegetation",
            "landmarkVegetationCount=%d",
            "landmarkVegetationMapIntegrated=%s",
        ):
            self.assertIn(token, body)

    def test_apply_is_one_save_additive_and_fail_closed(self):
        body = self.apply_body
        for token in (
            "ValidateReusableLandmarkVegetationAssets",
            "ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene",
            "ELandmarkPresencePolicy::Forbidden",
            "ELandmarkPresencePolicy::Required",
            "PreLandmarkVegetationR26Bytes",
            "PreLandmarkVegetationR26Sha256",
            "Package->IsDirty()",
            "RecheckedVegetationCount != 0",
            "TRIADIstanaExploreV5DR26LandmarkVegetation",
            "World->SpawnActor<ATRIADIstanaExploreV5DLandmarkVegetationActor>",
            "FTransform::Identity",
            "ConfigureLandmarkVegetationActor",
            "World->MarkPackageDirty()",
            "FAssetCompilingManager::Get().FinishAllCompilation()",
            "IDEMPOTENT_EXPLORE_V5D_LANDMARK_VEGETATION_R26_ALREADY_VALID",
            "EXPLORE_V5D_LANDMARK_VEGETATION_R26_APPLY_PASS",
        ):
            self.assertIn(token, body)
        self.assertEqual(
            1,
            body.count("UEditorLoadingAndSavingUtils::SaveMap("),
            "the forward migration must contain exactly one save site",
        )
        self.assertEqual(
            1,
            body.count(
                "World->SpawnActor<ATRIADIstanaExploreV5DLandmarkVegetationActor>"
            ),
        )
        self.assertNotIn("SavePackage", body)
        self.assertNotIn("DuplicateObject", body)
        self.assertNotIn("CreateFreshAssets", body)

    def test_rollback_is_cold_hash_verified_and_preserves_backup(self):
        helper = function_body(
            self.hybrid_cpp,
            "RestoreVerifiedPreLandmarkVegetationR26MapViaSiblingTemp",
        )
        for token in (
            "FPaths::CreateTempFilename",
            "TRIAD_LandmarkVegetationR26_Restore_",
            "FPaths::IsSamePath",
            "IFileManager::Get().Copy",
            "IFileManager::Get().Move",
            "PreLandmarkVegetationR26Bytes",
            "PreLandmarkVegetationR26Sha256",
            "bDestinationRestored",
            "bBackupPreserved",
        ):
            self.assertIn(token, helper)
        for failure in (
            "APPLY_FAILED_CONFIGURE",
            "APPLY_FAILED_PRE_SAVE_VALIDATION",
            "APPLY_FAILED_SAVE",
            "APPLY_FAILED_COLD_UNLOAD",
            "APPLY_FAILED_COLD_VALIDATION",
            "APPLY_FAILED_FINAL_RECEIPT",
        ):
            failure_index = self.apply_body.find(failure)
            self.assertGreaterEqual(failure_index, 0, failure)
            preceding = self.apply_body[max(0, failure_index - 550) : failure_index]
            self.assertIn("RestoreVerifiedPredecessor", preceding, failure)
        self.assertIn("UEditorLoadingAndSavingUtils::NewBlankMap(false)", self.apply_body)
        self.assertIn("successorReceiptRequiresPinning=true", self.apply_body)

    def test_successor_validator_requires_r26_and_retained_r25(self):
        body = self.validate_body
        for token in (
            "ValidateReusableLandmarkVegetationAssets",
            "TRIADIstanaExploreV5DContextFacadeR25AssetFactory::ValidateAssets",
            "ELandmarkPresencePolicy::Required",
            "ValidateCurrentSurroundingsContextFacadeR25ForInheritedScene",
            "ISTANA_EXPLORE_V5D_LANDMARK_VEGETATION_R26_MAP_VALID",
            "landmarkVegetationR26HistoricalNameOnly=true",
            "mapIntegrated=true",
            "contextFacadeR25=true",
            "exactlyOneActor=true",
            "collision=false",
            "navigation=false",
            "sensorAuthority=false",
            "rfAuthority=false",
            "providerSettingsUnchanged=true",
            "providerClipUnchanged=true",
        ):
            self.assertIn(token, body)

    def test_grass_is_bounded_but_readable_at_the_long_evidence_pose(self):
        for token in (
            "constexpr int32 MaximumGrassInstancesPerLandmark = 2048;",
            "constexpr int32 MacDonaldGrassCount = 1536;",
            "constexpr int32 TemasekGrassCount = 1536;",
            "constexpr int32 GrassCullStartDistanceCm = 6500;",
            "constexpr int32 GrassCullEndDistanceCm = 9000;",
            "constexpr int32 GrassWpoDisableDistanceCm = 2400;",
            "r26VisualAcceptanceFailed=true",
            "r27FarCameraCorrectionConfigured=true",
            "evidenceRangeMeters=72.8",
            "visualCaptureAccepted=false",
            "captureRevalidationRequired=true",
        ):
            self.assertIn(token, self.actor_cpp + self.validate_body)
        self.assertNotIn("mapIntegrated=false", self.actor_cpp + self.hybrid_cpp)
        self.assertIn("actorMapIntegrationAuthority=false", self.actor_cpp)
        self.assertIn("grassInstances=3072", self.validate_body)


if __name__ == "__main__":
    unittest.main()
