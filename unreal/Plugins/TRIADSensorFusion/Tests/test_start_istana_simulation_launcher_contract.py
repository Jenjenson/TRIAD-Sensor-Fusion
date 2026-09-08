from __future__ import annotations

import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
LAUNCHER = REPO / "scripts" / "Start-IstanaSimulation.ps1"
WORLD_SUBSYSTEM = (
    REPO
    / "unreal"
    / "Plugins"
    / "TRIADSensorFusion"
    / "Source"
    / "TRIADSensorFusion"
    / "Private"
    / "TRIADSensorFusionWorldSubsystem.cpp"
)
RF_EXAMPLE = (
    REPO
    / "unreal"
    / "Plugins"
    / "TRIADSensorFusion"
    / "Resources"
    / "IstanaOneKilometreV2RF.example.json"
)


class StartIstanaSimulationLauncherContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.launcher = LAUNCHER.read_text(encoding="utf-8")
        cls.world_subsystem = WORLD_SUBSYSTEM.read_text(encoding="utf-8")
        cls.rf_example = RF_EXAMPLE.read_text(encoding="utf-8")

    def test_default_remains_the_legacy_study_map(self) -> None:
        self.assertIn(
            "[string] $MapPackage = '/Game/Maps/Istana_1km'",
            self.launcher,
        )
        self.assertIn("'/Game/Maps/Istana_1km' { 'Istana_1km.umap' }", self.launcher)

    def test_exact_v5d_map_is_allowlisted_and_file_checked(self) -> None:
        for token in (
            "[ValidateSet(",
            "'/Game/Maps/Istana_PublicView_Explore_v5d_hybrid'",
            "'Istana_PublicView_Explore_v5d_hybrid.umap'",
            'Test-Path -LiteralPath $mapFile -PathType Leaf',
            "$MapPackage,",
            "Map = $MapPackage",
        ):
            self.assertIn(token, self.launcher)

    def test_dedicated_rf_config_must_match_selected_map(self) -> None:
        for token in (
            "$config.bUseDedicatedRFPropagation -eq $true",
            "$config.bRequireDedicatedRFReady -ne $true",
            "$config.bDedicatedRFFrameIsWorldOriginIdentity -ne $true",
            "[string] $config.DedicatedRFExpectedWorldPackageName -cne $MapPackage",
        ):
            self.assertIn(token, self.launcher)
        self.assertIn(
            '"DedicatedRFExpectedWorldPackageName": "/Game/Maps/Istana_PublicView_Explore_v5d_hybrid"',
            self.rf_example,
        )

    def test_scenario_subsystem_supports_any_pie_or_game_world(self) -> None:
        for token in (
            "WorldType == EWorldType::PIE || WorldType == EWorldType::Game",
            "LoadScenarioConfig(Config, Error) && Config.bEnabled",
            "SpawnActor<ATRIADSensorFusionScenarioManager>",
        ):
            self.assertIn(token, self.world_subsystem)


if __name__ == "__main__":
    unittest.main()
