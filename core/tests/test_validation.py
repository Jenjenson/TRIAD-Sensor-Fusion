from __future__ import annotations

import json
import math
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

from singapore_sensor_fusion.validation import (
    BOLTZMANN_J_K,
    REFERENCE_TEMPERATURE_K,
    SPEED_OF_LIGHT_M_S,
    run_validation_campaign,
    write_report_atomic,
)


def _jsonl(path: Path, rows: list[dict[str, object]]) -> None:
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def _chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)


def _write_png(path: Path, width: int, height: int, channels: int, pixels: bytes) -> None:
    color_type = {1: 0, 3: 2}[channels]
    stride = width * channels
    raw = b"".join(b"\x00" + pixels[offset : offset + stride] for offset in range(0, len(pixels), stride))
    ihdr = struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", ihdr) + _chunk(b"IDAT", zlib.compress(raw)) + _chunk(b"IEND", b""))


def _fspl(distance_m: float, frequency_ghz: float) -> float:
    return 20.0 * math.log10(4.0 * math.pi * distance_m * frequency_ghz * 1e9 / SPEED_OF_LIGHT_M_S)


def _noise(bandwidth_hz: float, noise_figure_db: float) -> float:
    return 10.0 * math.log10(BOLTZMANN_J_K * REFERENCE_TEMPERATURE_K * bandwidth_hz / 1e-3) + noise_figure_db


def _rf_row(node: str, frequency: float, *, detected: bool = True, truth: bool | None = True) -> dict[str, object]:
    distance = 1_000.0
    noise = _noise(20e6, 5.0)
    received = -70.0
    row: dict[str, object] = {
        "timestampUtc": "2026-08-01T00:00:01Z",
        "simulationSeconds": 1.0,
        "nodeId": node,
        "targetActor": "DemoSwarm_01",
        "emitterId": "swarm-rf",
        "frequencyGHz": frequency,
        "distanceMeters": distance,
        "lineOfSight": True,
        "freeSpacePathLossDb": _fspl(distance, frequency),
        "receivedPowerDbm": received,
        "noiseFloorDbm": noise,
        "snrDb": received - noise,
        "frequencySupported": True,
        "inRange": True,
        "aboveSensitivity": True,
        "detected": detected,
    }
    if truth is not None:
        row["hostileScenarioTruth"] = truth
    return row


def _make_frames(saved: Path) -> tuple[str, str]:
    directory = saved / "frames" / "City_Sector"
    directory.mkdir(parents=True)
    bases = [
        "frame_000000_20260801T000000000000Z",
        "frame_000001_20260801T000000033000Z",
    ]
    for index, base in enumerate(bases):
        rgb = directory / f"{base}_rgb.png"
        depth = directory / f"{base}_depth.png"
        metadata = directory / f"{base}_depth.json"
        if index == 0:
            rgb_pixels = bytes([10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120])
        else:
            rgb_pixels = bytes([30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140])
        _write_png(rgb, 2, 2, 3, rgb_pixels)
        _write_png(depth, 2, 2, 1, bytes([10, 50, 100, 200]))
        metadata.write_text(
            json.dumps(
                {
                    "timestampUtc": f"2026-08-01T00:00:0{index}Z",
                    "nodeId": "City_Sector",
                    "frameIndex": index,
                    "width": 2,
                    "height": 2,
                    "normalizationMaxMeters": 1000.0,
                    "minimumObservedMeters": 10.0,
                    "maximumObservedMeters": 900.0,
                    "targets": [
                        {
                            "actorName": "DemoSwarm_01",
                            "hostileScenarioTruth": True,
                            "bboxXyxyPixels": [0.25, 0.25, 1.75, 1.75],
                            "intersectsFrame": True,
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )
    return bases[0], bases[1]


class ValidationCampaignTests(unittest.TestCase):
    def _campaign(self, root: Path) -> tuple[Path, Path, Path, Path]:
        saved = root / "Saved" / "SingaporeSensorFusion"
        saved.mkdir(parents=True)
        config = root / "SingaporeSensorFusion.json"
        config.write_text(
            json.dumps(
                {
                    "bRequireLineOfSightForDetection": False,
                    "SensorNodes": [
                        {"NodeId": "Node-A", "BandwidthMHz": 20.0, "NoiseFigureDb": 5.0},
                        {"NodeId": "Node-B", "BandwidthMHz": 20.0, "NoiseFigureDb": 5.0},
                    ],
                }
            ),
            encoding="utf-8",
        )
        rf = saved / "rf_links_campaign.jsonl"
        _jsonl(rf, [_rf_row(node, band) for node in ("Node-A", "Node-B") for band in (2.4, 5.8)])
        alerts = saved / "alerts_campaign.jsonl"
        _jsonl(
            alerts,
            [
                {
                    "timestampUtc": "2026-08-01T00:00:01Z",
                    "simulationSeconds": 1.0,
                    "alertType": "confirmed_hostile",
                    "targetActor": "DemoSwarm_01",
                    "hostileScenarioTruth": True,
                    "confirmingNodeCount": 2,
                    "confirmingNodeIds": ["Node-A", "Node-B"],
                    "confirmingNodes": [
                        {"nodeId": "Node-A", "slantDistanceMeters": 820.0},
                        {"nodeId": "Node-B", "slantDistanceMeters": 1000.0},
                    ],
                    "minimumConfirmingNodes": 2,
                    "distanceValid": True,
                    "distanceSemantics": "sensor_to_target_3d_slant_range",
                    "nearestConfirmingNodeId": "Node-A",
                    "nearestConfirmingNodeDistanceMeters": 820.0,
                    "detectionOnly": True,
                    "actionsTaken": ["log", "visual_alert"],
                }
            ],
        )
        _, current = _make_frames(saved)
        event = root / "event_report.json"
        event.write_text(
            json.dumps(
                {
                    "event_camera": {
                        "source_encoding": "proxy_event_stack_v1",
                        "preprocessing_equivalent_to_training": False,
                        "event_observation_count": 1,
                        "observation_samples": [
                            {
                                "node_id": "City_Sector",
                                "measurements": {
                                    "frame_id": current + "_rgb",
                                    "bbox_xyxy_normalized": [0.1, 0.1, 0.9, 0.9],
                                },
                            }
                        ],
                    }
                }
            ),
            encoding="utf-8",
        )
        return saved, config, rf, event

    def test_valid_campaign_checks_math_frames_alerts_and_proxy_overlap(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            saved, config, rf, event = self._campaign(root)
            report = run_validation_campaign(
                saved_dir=saved,
                rf_jsonl=rf,
                alerts_jsonl=saved / "alerts_campaign.jsonl",
                scenario_config=config,
                event_report=event,
                max_rf_records=100,
                max_alert_records=100,
                max_frames=10,
                confirming_nodes_min=2,
                alert_cooldown_seconds=10.0,
            )
            statuses = {item["id"]: item["status"] for item in report["checks"]}
            self.assertEqual(statuses["rf.fspl_recomputation"], "pass")
            self.assertEqual(statuses["rf.ktb_noise_recomputation"], "pass")
            self.assertEqual(statuses["rf.detection_gate_consistency"], "pass")
            self.assertEqual(statuses["rf.dual_band_fspl_gap"], "pass")
            self.assertEqual(statuses["rf.multi_node_coverage"], "pass")
            self.assertEqual(statuses["frames.paired_files"], "pass")
            self.assertEqual(statuses["frames.projected_target_boxes"], "pass")
            self.assertEqual(statuses["alerts.hostile_only"], "pass")
            self.assertEqual(statuses["alerts.distance_fields"], "pass")
            self.assertEqual(statuses["alerts.scenario_truth_estimate"], "pass")
            self.assertTrue(report["event_camera"]["proxy_non_equivalence"])
            self.assertFalse(report["event_camera"]["candidate_activations_are_true_positives"])
            overlap = report["event_camera"]["projected_simulation_truth_overlap"]
            self.assertEqual(overlap["candidates_compared"], 1)
            self.assertTrue(overlap["not_precision_or_recall"])
            self.assertFalse(report["scope"]["engagement_logic_present"])
            output = write_report_atomic(report, root / "reports" / "validation.json")
            self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["schema_version"], "1.0")

    def test_inconsistent_gate_math_and_non_hostile_alert_fail(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            saved, config, _, _ = self._campaign(root)
            bad = _rf_row("Node-A", 2.4, detected=False)
            bad["freeSpacePathLossDb"] = float(bad["freeSpacePathLossDb"]) + 3.0
            rf = saved / "rf_links_bad.jsonl"
            _jsonl(rf, [bad])
            alerts = saved / "alerts_bad.jsonl"
            _jsonl(
                alerts,
                [
                    {
                        "simulationSeconds": 1.0,
                        "targetActor": "FriendlyDrone",
                        "hostileScenarioTruth": False,
                        "confirmingNodeCount": 1,
                    }
                ],
            )
            report = run_validation_campaign(
                saved_dir=saved,
                rf_jsonl=rf,
                alerts_jsonl=alerts,
                scenario_config=config,
                max_rf_records=10,
                max_alert_records=10,
                max_frames=10,
            )
            statuses = {item["id"]: item["status"] for item in report["checks"]}
            self.assertEqual(statuses["rf.fspl_recomputation"], "fail")
            self.assertEqual(statuses["rf.detection_gate_consistency"], "fail")
            self.assertEqual(statuses["alerts.hostile_only"], "fail")
            self.assertEqual(statuses["alerts.confirming_nodes"], "fail")
            self.assertEqual(report["overall_status"], "fail")

    def test_older_telemetry_without_truth_is_warned_not_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            saved = root / "Saved" / "SingaporeSensorFusion"
            saved.mkdir(parents=True)
            rf = saved / "rf_links_old.jsonl"
            _jsonl(rf, [_rf_row("Node-A", 2.4, truth=None)])
            report = run_validation_campaign(
                saved_dir=saved,
                rf_jsonl=rf,
                scenario_config=None,
                max_rf_records=10,
                max_alert_records=10,
                max_frames=10,
            )
            statuses = {item["id"]: item["status"] for item in report["checks"]}
            self.assertEqual(statuses["rf.scenario_truth_available"], "warn")
            self.assertEqual(report["rf"]["record_count"], 1)


if __name__ == "__main__":
    unittest.main()
