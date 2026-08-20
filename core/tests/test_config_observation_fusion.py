from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import tempfile
import unittest

from singapore_sensor_fusion.config import SimulationConfig, load_simulation_config
from singapore_sensor_fusion.fusion import (
    ProbabilityEvidence,
    conservative_confidence_fusion,
)
from singapore_sensor_fusion.observations import SensorModality, SensorObservation


def _config_mapping() -> dict:
    return {
        "random_seed": 42,
        "sensor_nodes": [
            {
                "node_id": "virtual-node-01",
                "location": {
                    "latitude_deg": 1.3521,
                    "longitude_deg": 103.8198,
                    "altitude_m": 40.0,
                },
                "rf_channels": [
                    {"center_frequency_hz": 2.4e9, "bandwidth_hz": 20e6},
                    {"center_frequency_hz": 5.8e9, "bandwidth_hz": 20e6},
                ],
            }
        ],
        "drone_emitters": [
            {
                "emitter_id": "sim-drone-01",
                "location": {
                    "latitude_deg": 1.36,
                    "longitude_deg": 103.83,
                    "altitude_m": 120.0,
                },
                "center_frequency_hz": 2.4e9,
                "transmit_power_dbm": 20.0,
                "bandwidth_hz": 20e6,
                "duty_cycle": 0.7,
            }
        ],
    }


class ConfigObservationFusionTests(unittest.TestCase):
    def test_config_mapping_and_json_loader(self) -> None:
        config = SimulationConfig.from_mapping(_config_mapping())
        self.assertEqual(config.random_seed, 42)
        self.assertEqual(config.sensor_nodes[0].rf_channels[1].center_frequency_hz, 5.8e9)
        self.assertAlmostEqual(config.drone_emitters[0].duty_cycle, 0.7)

        with tempfile.TemporaryDirectory() as temporary_directory:
            config_path = Path(temporary_directory) / "simulation.json"
            config_path.write_text(json.dumps(_config_mapping()), encoding="utf-8")
            loaded = load_simulation_config(config_path)
        self.assertEqual(loaded, config)
        self.assertEqual(loaded.to_dict()["sensor_nodes"][0]["node_id"], "virtual-node-01")

    def test_config_rejects_duplicate_node_ids(self) -> None:
        raw = _config_mapping()
        raw["sensor_nodes"].append(dict(raw["sensor_nodes"][0]))
        with self.assertRaisesRegex(ValueError, "node IDs must be unique"):
            SimulationConfig.from_mapping(raw)

    def test_observation_is_utc_json_round_trip(self) -> None:
        local_zone = timezone(timedelta(hours=8))
        observation = SensorObservation(
            observation_id="obs-0001",
            timestamp=datetime(2026, 8, 1, 10, 30, tzinfo=local_zone),
            node_id="virtual-node-01",
            modality=SensorModality.RF,
            detection_probability=0.84,
            confidence_level=0.91,
            target_track_id="sim-track-7",
            frequency_hz=2.4e9,
            position_ecef_m=(-1.5, 2.0, 3.25),
            model_id="analytic-fallback",
            measurements={"snr_db": -3.5},
        )
        payload = observation.to_dict()
        self.assertTrue(payload["timestamp"].startswith("2026-08-01T02:30:00"))
        self.assertTrue(payload["timestamp"].endswith("Z"))
        recovered = SensorObservation.from_dict(json.loads(observation.to_json()))
        self.assertEqual(recovered, observation)

    def test_observation_rejects_naive_timestamp(self) -> None:
        with self.assertRaisesRegex(ValueError, "timezone-aware"):
            SensorObservation(
                observation_id="obs-1",
                timestamp=datetime(2026, 8, 1, 10, 30),
                node_id="node-1",
                modality=SensorModality.RGBD,
                detection_probability=0.5,
                confidence_level=0.5,
            )

    def test_conservative_fusion_does_not_assume_independence(self) -> None:
        evidence = [
            ProbabilityEvidence("rf", probability=0.90, confidence_level=0.80),
            ProbabilityEvidence("rgbd", probability=0.75, confidence_level=0.99),
            ProbabilityEvidence("event", probability=0.70, confidence_level=0.95),
        ]
        result = conservative_confidence_fusion(evidence)
        self.assertAlmostEqual(result.fused_probability_lower_bound, 0.75 * 0.99)
        self.assertEqual(result.dominant_source_id, "rgbd")
        self.assertEqual(result.evidence_count, 3)
        self.assertLessEqual(
            result.fused_probability_lower_bound,
            max(item.probability for item in evidence),
        )


if __name__ == "__main__":
    unittest.main()
