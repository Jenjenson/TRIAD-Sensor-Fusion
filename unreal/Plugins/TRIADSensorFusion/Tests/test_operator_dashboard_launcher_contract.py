from __future__ import annotations

import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
DASHBOARD_LAUNCHER = REPO / "scripts" / "Show-IstanaPlacementDashboard.ps1"
SCENARIO_BUILDER = REPO / "scripts" / "New-IstanaScenarioConfig.ps1"


class OperatorDashboardLauncherContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.dashboard = DASHBOARD_LAUNCHER.read_text(encoding="utf-8")
        cls.scenario = SCENARIO_BUILDER.read_text(encoding="utf-8")

    def test_python_candidates_are_probed_before_selection(self) -> None:
        for token in (
            "function Test-DashboardPython",
            "import tkinter; import numpy",
            "foreach ($candidatePath in $candidatePaths)",
            "Test-DashboardPython -Executable $candidatePath",
            "No usable Python was found",
        ):
            self.assertIn(token, self.dashboard)
        self.assertNotIn("$resolvedPython = $candidatePaths[0]", self.dashboard)

    def test_replay_mode_does_not_forward_ignored_generation_arguments(self) -> None:
        replay_branch = self.dashboard.split(
            "if (-not [string]::IsNullOrWhiteSpace($ReplayPath))", 1
        )[1].split("if ($Autoplay)", 1)[0]
        self.assertLess(replay_branch.index("--replay"), replay_branch.index("else"))
        generation_branch = replay_branch.split("else", 1)[1]
        for token in ("--scenario-id", "--duration-seconds", "--cadence-seconds"):
            self.assertIn(token, generation_branch)

    def test_headless_validation_is_exposed_by_the_launcher(self) -> None:
        self.assertIn("[switch] $ValidateOnly", self.dashboard)
        self.assertIn("$arguments += '--validate-only'", self.dashboard)

    def test_live_overlay_forwards_only_bounded_client_controls(self) -> None:
        for token in (
            "[string] $LiveUrl",
            "[double] $LivePollSeconds = 0.25",
            "[double] $LiveTimeoutSeconds = 0.75",
            "'--live-url', $LiveUrl",
            "'--live-poll-seconds'",
            "'--live-timeout-seconds'",
        ):
            self.assertIn(token, self.dashboard)
        self.assertIn("[ValidateRange(0.1, 10.0)]", self.dashboard)
        self.assertIn("[ValidateRange(0.1, 5.0)]", self.dashboard)

    def test_new_scenario_can_enable_the_evidence_gated_operator_overlay(self) -> None:
        self.assertIn("[switch] $EnableOperatorObserver", self.scenario)
        self.assertIn("$base.OperatorObserver.bEnabled = $true", self.scenario)
        self.assertIn("OperatorObserverEnabled =", self.scenario)


if __name__ == "__main__":
    unittest.main()
