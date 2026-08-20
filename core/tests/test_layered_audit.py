from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from singapore_sensor_fusion.layered_audit import (
    MMWAVE_DISTANCES_M,
    RAIN_RATES_MM_H,
    RF_DISTANCES_KM,
    SPEEDS_M_S,
    SWARM_SIZES,
    render_markdown,
    run_audit,
    write_reports,
)


class StepClock:
    def __init__(self, increment_seconds: float = 0.001) -> None:
        self.value = 0.0
        self.increment_seconds = increment_seconds

    def __call__(self) -> float:
        self.value += self.increment_seconds
        return self.value


def keyed_values(value: object, key: str) -> list[object]:
    results: list[object] = []
    if isinstance(value, dict):
        for candidate, item in value.items():
            if candidate == key:
                results.append(item)
            results.extend(keyed_values(item, key))
    elif isinstance(value, list):
        for item in value:
            results.extend(keyed_values(item, key))
    return results


class LayeredAuditTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.report = run_audit(clock=StepClock())

    def test_every_required_axis_is_present_exactly(self) -> None:
        required = self.report["requiredAxes"]
        self.assertEqual(required["rfDistancesKilometers"], list(RF_DISTANCES_KM))
        self.assertEqual(required["mmWaveDistancesMeters"], list(MMWAVE_DISTANCES_M))
        self.assertEqual(required["speedsMetersPerSecond"], list(SPEEDS_M_S))
        self.assertEqual(required["rainRatesMillimetersPerHour"], list(RAIN_RATES_MM_H))
        self.assertEqual(required["swarmSizes"], list(SWARM_SIZES))
        self.assertEqual(len(self.report["modalityFailureSweep"]), 8)
        self.assertEqual(len(self.report["runtimeSiteFailureSweep"]), 4)

    def test_rf_sweep_detects_non_24_58_channels_without_supplied_models(self) -> None:
        rows = self.report["rfDistanceSweep"]
        self.assertTrue(all(row["operatorCueCount"] == 1 for row in rows))
        self.assertTrue(
            all(row["detectedBeyondPrimary24And58GHzChannelCount"] >= 3 for row in rows)
        )
        at_30km = next(row for row in rows if row["distanceKilometers"] == 30.0)
        self.assertEqual(at_30km["detectedChannelsAtOneReceiver"], 4)
        self.assertEqual(at_30km["fusionDecision"], "PRELIMINARY_RF_CUE")
        self.assertTrue(all(row["suppliedRFModelsUsed"] is False for row in rows))

    def test_mmwave_and_rain_sweeps_expose_detection_boundary(self) -> None:
        distance_states = {
            row["distanceMeters"]: bool(row["mmWaveDetectionCount"])
            for row in self.report["mmWaveDistanceSweep"]
        }
        for distance in (50.0, 100.0, 200.0, 300.0, 400.0, 500.0):
            self.assertTrue(distance_states[distance])
        self.assertFalse(distance_states[700.0])
        self.assertFalse(distance_states[1000.0])

        rain_states = {
            row["rainRateMillimetersPerHour"]: bool(
                row["mmWaveDetectionCountAt400Meters"]
            )
            for row in self.report["rainSweep"]
        }
        self.assertTrue(rain_states[0.0])
        self.assertTrue(rain_states[10.0])
        self.assertFalse(rain_states[25.0])
        self.assertFalse(rain_states[50.0])
        self.assertFalse(rain_states[100.0])

    def test_speed_sweep_measures_closing_velocity_and_time_to_perimeter(self) -> None:
        rows = self.report["speedSweep"]
        for row in rows:
            self.assertAlmostEqual(
                row["speedMetersPerSecond"],
                row["measuredClosingVelocityMetersPerSecond"],
                places=9,
            )
            self.assertEqual(row["operatorCueCount"], 1)
        stationary = next(row for row in rows if row["speedMetersPerSecond"] == 0.0)
        fastest = next(row for row in rows if row["speedMetersPerSecond"] == 300.0)
        self.assertIsNone(stationary["timeToPerimeterSeconds"])
        self.assertAlmostEqual(fastest["timeToPerimeterSeconds"], 16.667, places=3)

    def test_failure_matrix_degrades_to_preliminary_then_no_cue(self) -> None:
        modality = {row["scenarioId"]: row for row in self.report["modalityFailureSweep"]}
        self.assertEqual(modality["nominal"]["fusionDecision"], "CONFIRMED_TRACK")
        self.assertEqual(
            modality["all_but_one_rf_offline"]["fusionDecision"],
            "HOLD_FOR_CORROBORATION",
        )
        self.assertEqual(
            modality["total_sensor_outage"]["fusionDecision"],
            "NO_CURRENT_EVIDENCE",
        )

        sites = {row["scenarioId"]: row for row in self.report["runtimeSiteFailureSweep"]}
        self.assertEqual(sites["nominal"]["fusionDecision"], "DETECTION_ALERT")
        self.assertEqual(
            sites["both_near_sites_offline"]["fusionDecision"],
            "PRELIMINARY_RF_CUE",
        )
        self.assertEqual(
            sites["all_sites_offline"]["fusionDecision"],
            "NO_CURRENT_EVIDENCE",
        )

    def test_rf_silent_target_requires_non_rf_corroboration(self) -> None:
        result = self.report["rfSilentTarget"]
        self.assertEqual(result["widebandRFDetectedLinkCount"], 0)
        self.assertEqual(result["mmWaveDetectedLinkCount"], 0)
        self.assertGreater(result["searchRadarDetectionCount"], 0)
        self.assertEqual(
            result["passiveOnly"]["fusionDecision"], "HOLD_FOR_CORROBORATION"
        )
        self.assertEqual(
            result["withPostAssociationVisualFixtures"]["fusionDecision"],
            "DETECTION_ALERT",
        )
        self.assertEqual(
            result["withPostAssociationVisualFixtures"]["activeModalityFamilies"],
            ["active_radar", "visual"],
        )
        self.assertEqual(
            result["withPostAssociationVisualFixtures"]["activeModalityFamilyCount"],
            2,
        )
        self.assertIn("cannot corroborate each other", result["visualFixtureSemantics"])
        self.assertIn("does not run or validate", result["visualFixtureSemantics"])

    def test_swarm_sweep_processes_and_cues_every_requested_size(self) -> None:
        rows = self.report["swarmSweep"]
        self.assertEqual([row["requestedSwarmSize"] for row in rows], list(SWARM_SIZES))
        for row in rows:
            self.assertEqual(row["processedTrackCount"], row["requestedSwarmSize"])
            self.assertEqual(row["detectedTrackCount"], row["requestedSwarmSize"])
            self.assertEqual(row["activeOperatorCueCount"], row["requestedSwarmSize"])
            self.assertEqual(row["outsideDetectedTrackCount"], row["requestedSwarmSize"])
            self.assertTrue(row["legacyUnrealDetectedRFLinksConsumed"])
            self.assertFalse(row["suppliedRFModelsUsed"])

    def test_latency_percentiles_and_claim_boundaries_are_explicit(self) -> None:
        latency = self.report["modeledDecisionLatencySummaryMilliseconds"]
        runtime = self.report["processingRuntimeSummaryMilliseconds"]
        self.assertGreater(latency["sampleCount"], 100)
        self.assertIsNotNone(latency["p50"])
        self.assertIsNotNone(latency["p95"])
        self.assertIsNotNone(latency["p99"])
        self.assertGreater(runtime["sampleCount"], 40)
        self.assertEqual(
            self.report["claimBoundary"]["mathematicalSimulationCoverage"],
            "EXECUTED",
        )
        self.assertEqual(self.report["claimBoundary"]["fieldCalibration"], "NOT_PERFORMED")
        self.assertFalse(self.report["calibratedOperationalPerformanceClaimed"])
        self.assertEqual(set(keyed_values(self.report, "suppliedRFModelsUsed")), {False})
        self.assertEqual(set(keyed_values(self.report, "engagementLogicPresent")), {False})

    def test_deterministic_digest_excludes_host_runtime_measurements(self) -> None:
        slower_clock_report = run_audit(clock=StepClock(increment_seconds=0.123))
        self.assertNotEqual(
            self.report["processingRuntimeSummaryMilliseconds"]["p50"],
            slower_clock_report["processingRuntimeSummaryMilliseconds"]["p50"],
        )
        self.assertEqual(
            self.report["determinism"]["deterministicOutcomeSha256"],
            slower_clock_report["determinism"]["deterministicOutcomeSha256"],
        )

    def test_json_and_markdown_reports_round_trip(self) -> None:
        markdown = render_markdown(self.report)
        self.assertIn("Mathematical simulation coverage: executed.", markdown)
        self.assertIn("Field calibration and hardware characterization: not performed.", markdown)
        self.assertIn("RF-silent fallback", markdown)
        with tempfile.TemporaryDirectory() as directory:
            json_path, markdown_path = write_reports(
                self.report, output_directory=Path(directory)
            )
            reloaded = json.loads(json_path.read_text(encoding="utf-8"))
            rendered = markdown_path.read_text(encoding="utf-8")
        self.assertEqual(
            reloaded["determinism"]["deterministicOutcomeSha256"],
            self.report["determinism"]["deterministicOutcomeSha256"],
        )
        self.assertIn("Swarm scale", rendered)


if __name__ == "__main__":
    unittest.main()
