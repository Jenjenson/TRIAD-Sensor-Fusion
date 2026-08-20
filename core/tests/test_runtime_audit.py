from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from singapore_sensor_fusion.runtime_audit import render_markdown, run_runtime_audit


def _jsonl(path: Path, rows: list[dict[str, object]]) -> None:
    path.write_text(
        "".join(json.dumps(row, separators=(",", ":")) + "\n" for row in rows),
        encoding="utf-8",
    )


def _rf_row(node: str, band: float, *, detected: bool) -> dict[str, object]:
    return {
        "timestampUtc": "2026-08-01T00:00:01.000Z",
        "simulationSeconds": 1.0,
        "nodeId": node,
        "targetActor": "Hostile_0",
        "emitterId": "Hostile_RF",
        "hostileScenarioTruth": True,
        "frequencyGHz": band,
        "nodeLongitudeDegrees": 103.86,
        "nodeLatitudeDegrees": 1.28,
        "nodeHeightMeters": 100.0,
        "targetLongitudeDegrees": 103.87,
        "targetLatitudeDegrees": 1.29,
        "targetHeightMeters": 300.0,
        "distanceMeters": 1000.0 if band == 2.4 else 1200.0,
        "azimuthDegrees": 90.0,
        "elevationDegrees": 2.0,
        "lineOfSight": True,
        "freeSpacePathLossDb": 100.0,
        "obstructionLossDb": 0.0,
        "weatherProfile": "Clear",
        "airSimVisualWeatherApplied": True,
        "weatherRainRateMillimetersPerHour": 0.0,
        "weatherVisibilityMeters": 30000.0,
        "weatherRFSpecificAttenuationDbPerKm": 0.0,
        "weatherRFLossDb": 0.0,
        "receivedPowerDbm": -70.0,
        "noiseFloorDbm": -96.0,
        "snrDb": 26.0,
        "frequencySupported": True,
        "inRange": True,
        "aboveSensitivity": True,
        "detected": detected,
        "preliminaryCueEvidence": detected,
        "alertEvidence": detected,
    }


class RuntimeAuditTests(unittest.TestCase):
    def _fixture(self, root: Path) -> tuple[Path, Path, Path]:
        saved = root / "Saved" / "SingaporeSensorFusion"
        saved.mkdir(parents=True)
        rf = saved / "rf_links_20260801T000000Z_A1.jsonl"
        _jsonl(
            rf,
            [
                _rf_row("Node-A", 2.4, detected=True),
                _rf_row("Node-A", 5.8, detected=False),
                _rf_row("Node-B", 2.4, detected=True),
                _rf_row("Node-B", 5.8, detected=True),
            ],
        )
        _jsonl(
            saved / "alerts_20260801T000000Z_A1.jsonl",
            [
                {
                    "timestampUtc": "2026-08-01T00:00:02.000Z",
                    "simulationSeconds": 2.0,
                    "alertType": "rf_multinode_preliminary_cue",
                    "cueLevel": "preliminary",
                    "classification": "unclassified_emitter",
                    "hostilityAssessment": "not_inferred",
                    "evidenceBasis": "rf_detection_by_distinct_nodes",
                    "trackId": "RF-CUE-0123ABCD-89ABCDEF",
                    "identifierSemantics": "deterministic_pseudonym_no_raw_actor_or_emitter_id",
                    "positionEstimateAvailable": False,
                    "approachEstimateAvailable": False,
                    "bearingEstimateAvailable": False,
                    "positionApproachSemantics": "unavailable_at_raw_rf_cue_stage",
                    "confirmingNodeCount": 2,
                    "confirmingNodeIds": ["Node-A", "Node-B"],
                    "confirmingNodes": [
                        {"nodeId": "Node-A", "slantDistanceMeters": 1000.0},
                        {"nodeId": "Node-B", "slantDistanceMeters": 1100.0},
                    ],
                    "minimumConfirmingNodes": 2,
                    "maxSnrDb": 26.0,
                    "distanceValid": True,
                    "distanceSemantics": "sensor_to_target_3d_slant_range",
                    "nearestConfirmingNodeId": "Node-A",
                    "nearestConfirmingNodeDistanceMeters": 1000.0,
                    "weatherProfile": "Clear",
                    "airSimVisualWeatherApplied": True,
                    "weatherRainRateMillimetersPerHour": 0.0,
                    "weatherVisibilityMeters": 30000.0,
                    "detectionOnly": True,
                    "actionsTaken": "none",
                },
                {
                    "timestampUtc": "2026-08-01T00:00:02.500Z",
                    "simulationSeconds": 2.5,
                    "alertType": "simulated_hostile_rf_multinode",
                    "targetActor": "LegacyEvaluatorTarget",
                    "emitterId": "LegacyEvaluatorEmitter",
                    "hostileScenarioTruth": True,
                    "confirmingNodeCount": 2,
                    "confirmingNodeIds": ["Node-A", "Node-B"],
                    "minimumConfirmingNodes": 2,
                    "weatherProfile": "Clear",
                    "detectionOnly": True,
                    "actionsTaken": "none",
                },
            ],
        )
        frames = saved / "frames" / "Node-A"
        frames.mkdir(parents=True)
        base = "frame_000000_20260801T000001fZ"
        (frames / f"{base}_rgb.png").write_bytes(b"rgb")
        (frames / f"{base}_depth.png").write_bytes(b"depth")
        (frames / f"{base}_depth.json").write_text(
            json.dumps(
                {
                    "timestampUtc": "2026-08-01T00:00:01.500Z",
                    "nodeId": "Node-A",
                    "frameIndex": 0,
                    "width": 640,
                    "height": 360,
                    "fovDegrees": 90.0,
                    "depthCaptureSource": "scene-depth",
                    "depthPngEncoding": "normalised",
                    "normalizationMaxMeters": 10000.0,
                    "minimumObservedMeters": 1.0,
                    "maximumObservedMeters": 10000.0,
                    "weatherProfile": "Clear",
                    "airSimVisualWeatherApplied": True,
                    "weatherRainRateMillimetersPerHour": 0.0,
                    "weatherVisibilityMeters": 30000.0,
                    "targetProjectionSemantics": "frustum-only",
                    "cameraAspectRatio": 16 / 9,
                    "taggedTargetCount": 1,
                    "visibleTargetCount": 1,
                    "targets": [],
                }
            ),
            encoding="utf-8",
        )
        config = root / "SingaporeSensorFusion.json"
        config.write_text(
            json.dumps(
                {
                    "SensorNodes": [
                        {"NodeId": "Node-A", "bEnabled": True},
                        {"NodeId": "Node-B", "bEnabled": True},
                    ],
                    "MaxTelemetryRecords": 100,
                    "MaxTelemetryFileBytes": rf.stat().st_size,
                    "MaxAlertRecords": 2,
                    "MaxFramesPerNode": 1,
                    "Weather": {
                        "bCycleProfiles": True,
                        "CycleProfiles": ["Clear", "Haze"],
                    },
                }
            ),
            encoding="utf-8",
        )
        log = root / "TRIAD.log"
        log.write_text(
            "\n".join(
                [
                    "[2026.08.01-00.00.00:000][  0]LogTemp: StartupModule: AirSim TRIAD runtime plugin",
                    "[2026.08.01-00.00.00:500][  0]LogTemp: Display: TRIAD WEATHER Clear | rain 0 mm/h | visibility 30.0 km | AirSim visual weather verified.",
                    "[2026.08.01-00.00.05:000][  0]LogTemp: Display: TRIAD JSONL telemetry stopped: JSONL byte limit reached.",
                    "[2026.08.01-00.00.10:000][  0]LogTemp: Display: TRIAD WEATHER Haze | rain 0 mm/h | visibility 2.5 km | AirSim visual weather verified.",
                ]
            )
            + "\n",
            encoding="utf-8",
        )
        return saved, config, log

    def test_streaming_groups_bands_nodes_caps_and_log_only_weather(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            saved, config, log = self._fixture(Path(temporary))
            report = run_runtime_audit(
                saved_dir=saved,
                log_path=log,
                config_path=config,
            )
            clear = report["rf"]["by_weather_and_band"]["Clear"]
            self.assertEqual(clear["2.4 GHz"]["sample_count"], 2)
            self.assertEqual(clear["5.8 GHz"]["detection_count"], 1)
            self.assertEqual(clear["2.4 GHz"]["preliminary_cue_evidence_sample_count"], 2)
            self.assertEqual(clear["2.4 GHz"]["legacy_truth_gated_alert_evidence_sample_count"], 0)
            self.assertEqual(clear["5.8 GHz"]["range_m"]["mean"], 1200.0)
            coverage = report["rf"]["node_coverage"]
            self.assertTrue(coverage["all_expected_nodes_observed"])
            self.assertEqual(coverage["missing_nodes"], [])
            haze = report["weather"]["profile_evidence"]["Haze"]
            self.assertTrue(haze["only_in_logs_overall"])
            self.assertTrue(haze["only_in_logs_because_telemetry_capped"])
            self.assertTrue(report["limits"]["rf_jsonl_cap_explicitly_logged"])
            self.assertTrue(report["limits"]["rf_cue_record_limit_reached_inferred"])
            self.assertEqual(report["frames"]["paired_rgb_depth_metadata_count"], 1)
            self.assertEqual(report["rf"]["schema"]["complete_record_percent"], 100.0)
            cues = report["rf_preliminary_cues"]
            self.assertEqual(cues["schema"]["complete_record_percent"], 100.0)
            self.assertEqual(cues["legacy_truth_gated_alert_schema"]["complete_record_percent"], 100.0)
            self.assertEqual(cues["by_weather_profile"]["Clear"]["generic_preliminary_cue_record_count"], 1)
            self.assertEqual(cues["by_weather_profile"]["Clear"]["legacy_truth_gated_alert_record_count"], 1)

    def test_markdown_names_self_report_boundary_and_missing_schema(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            saved, config, log = self._fixture(Path(temporary))
            rf = saved / "rf_links_20260801T000000Z_A1.jsonl"
            rows = [json.loads(line) for line in rf.read_text(encoding="utf-8").splitlines()]
            del rows[0]["weatherRFLossDb"]
            _jsonl(rf, rows)
            report = run_runtime_audit(
                saved_dir=saved,
                log_path=log,
                config_path=config,
            )
            self.assertEqual(
                report["rf"]["schema"]["missing_record_count_by_field"]["weatherRFLossDb"],
                1,
            )
            markdown = render_markdown(report)
            self.assertIn("2.4 GHz", markdown)
            self.assertIn("Haze", markdown)
            self.assertIn("runtime self-report", markdown)
            self.assertIn("weatherRFLossDb", markdown)
            self.assertIn("Emitted preliminary RF cues", markdown)
            self.assertNotIn("Emitted hostile alerts", markdown)


if __name__ == "__main__":
    unittest.main()
