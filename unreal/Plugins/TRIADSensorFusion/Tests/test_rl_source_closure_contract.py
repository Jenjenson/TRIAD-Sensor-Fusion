from __future__ import annotations

import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal/Plugins/TRIADSensorFusion"
SOURCE = PLUGIN / "Source"
RUNTIME = SOURCE / "TRIADSensorFusion"
EDITOR = SOURCE / "TRIADSensorFusionEditor"
WORLD_CPP = RUNTIME / "Private/TRIADSensorFusionWorldSubsystem.cpp"
WORLD_H = RUNTIME / "Public/TRIADSensorFusionWorldSubsystem.h"
MANAGER_CPP = RUNTIME / "Private/TRIADAdversarialTrainingManager.cpp"
TYPES_H = RUNTIME / "Public/TRIADRLTrainingTypes.h"
COMMANDLET_CPP = EDITOR / "Private/TRIADRLAssetBootstrapCommandlet.cpp"
RUNTIME_BUILD = RUNTIME / "TRIADSensorFusion.Build.cs"
EDITOR_BUILD = EDITOR / "TRIADSensorFusionEditor.Build.cs"

REQUIRED_RL_SOURCES = (
    "TRIADSensorFusion/Private/TRIADAdversarialTrainingManager.cpp",
    "TRIADSensorFusion/Private/TRIADProtectedZoneComponent.cpp",
    "TRIADSensorFusion/Private/TRIADRLTrainingModel.cpp",
    "TRIADSensorFusion/Private/TRIADSwarmControllerComponent.cpp",
    "TRIADSensorFusion/Private/Tests/TRIADRLTrainingModelTests.cpp",
    "TRIADSensorFusion/Public/TRIADAdversarialTrainingManager.h",
    "TRIADSensorFusion/Public/TRIADProtectedZoneComponent.h",
    "TRIADSensorFusion/Public/TRIADRLTrainingModel.h",
    "TRIADSensorFusion/Public/TRIADRLTrainingTypes.h",
    "TRIADSensorFusion/Public/TRIADSwarmControllerComponent.h",
    "TRIADSensorFusionEditor/Private/TRIADRLAssetBootstrapCommandlet.cpp",
    "TRIADSensorFusionEditor/Private/TRIADRLAssetBootstrapCommandlet.h",
)


def text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def function_body(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start : index + 1]
    raise AssertionError(f"unclosed function: {signature}")


class RLSourceClosureContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.world_cpp = text(WORLD_CPP)
        cls.world_h = text(WORLD_H)
        cls.manager_cpp = text(MANAGER_CPP)
        cls.types_h = text(TYPES_H)
        cls.commandlet_cpp = text(COMMANDLET_CPP)

    def test_complete_runtime_editor_and_native_test_source_set_is_present(self) -> None:
        missing = [relative for relative in REQUIRED_RL_SOURCES if not (SOURCE / relative).is_file()]
        self.assertEqual([], missing)

    def test_training_definition_restores_the_class_referenced_by_the_rl_asset(self) -> None:
        for marker in (
            "UCLASS(BlueprintType)",
            "UTRIADRLTrainingDefinition : public UPrimaryDataAsset",
            "FTRIADRLTrainingConfig Config",
            "FTRIADRLBlueAction",
            "FTRIADRLRedAction",
            "FTRIADRLStepResult",
        ):
            self.assertIn(marker, self.types_h)

    def test_world_bootstrap_is_enabled_by_scenario_config_or_explicit_rl_flag(self) -> None:
        should_create = function_body(
            self.world_cpp,
            "bool UTRIADSensorFusionWorldSubsystem::ShouldCreateSubsystem(",
        )
        self.assertIn("bSensorFusionEnabled || ATRIADAdversarialTrainingManager::IsTrainingRequested()", should_create)

        begin_play = function_body(
            self.world_cpp,
            "void UTRIADSensorFusionWorldSubsystem::OnWorldBeginPlay(",
        )
        self.assertIn("if (bSensorFusionEnabled)", begin_play)
        self.assertIn("TRIAD_SingaporeSensorFusion_Manager", begin_play)
        self.assertIn("if (ATRIADAdversarialTrainingManager::IsTrainingRequested())", begin_play)
        self.assertIn("TRIAD_AdversarialTraining_Manager", begin_play)
        self.assertNotIn("return;", begin_play)
        self.assertIn("TObjectPtr<ATRIADAdversarialTrainingManager> AdversarialTrainingManager", self.world_h)
        self.assertIn("AdversarialTrainingManager = nullptr", self.world_cpp)

    def test_training_manager_remains_simulation_only_and_explicitly_opt_in(self) -> None:
        requested = function_body(
            self.manager_cpp,
            "bool ATRIADAdversarialTrainingManager::IsTrainingRequested()",
        )
        self.assertIn('FParse::Param(FCommandLine::Get(), TEXT("TRIADRLTraining"))', requested)
        self.assertIn('Tags.AddUnique(TEXT("TRIADSimulationOnlyRL"))', self.manager_cpp)
        self.assertIn("DefaultTrainingAssetPath", self.manager_cpp)
        self.assertIn("DefaultTrainingConfig.json", self.manager_cpp)
        self.assertIn("FTRIADRLTrainingModel::ValidateConfig", self.manager_cpp)

    def test_editor_commandlet_validates_and_creates_the_default_training_asset(self) -> None:
        for marker in (
            'TEXT("/Game/TRIAD/RL/DA_TRIADRLDefault")',
            "UTRIADRLTrainingDefinition::StaticClass()",
            "FTRIADRLTrainingModel::ValidateConfig",
            "FAssetRegistryModule::AssetCreated",
            "UPackage::SavePackage",
        ):
            self.assertIn(marker, self.commandlet_cpp)

    def test_existing_module_dependencies_cover_the_restored_sources(self) -> None:
        runtime_build = text(RUNTIME_BUILD)
        editor_build = text(EDITOR_BUILD)
        for dependency in ('"CesiumRuntime"', '"Json"', '"JsonUtilities"'):
            self.assertIn(dependency, runtime_build)
        for dependency in ('"TRIADSensorFusion"', '"UnrealEd"', '"AssetRegistry"'):
            self.assertIn(dependency, editor_build)


if __name__ == "__main__":
    unittest.main()
