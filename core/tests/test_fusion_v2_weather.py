from __future__ import annotations

from datetime import datetime, timedelta, timezone
import json
from pathlib import Path
import tempfile
import unittest

from singapore_sensor_fusion.fusion_v2 import (
    FusionEvidenceV2,
    FusionModality,
    FusionPolicyV2,
    REFERENCE_AS_OF,
    SCENARIO_ASSUMPTION_NOTICE,
    WEATHER_PROFILES,
    fuse_track_evidence_v2,
    reference_scenario_evidence,
    run_reference_weather_sweep,
)
from singapore_sensor_fusion.observations import SensorModality, SensorObservation
from singapore_sensor_fusion.weather_sweep import write_json_atomic


class FusionV2WeatherTests(unittest.TestCase):
    def test_raw_score_and_weather_adjustment_are_separate(self) -> None:
        rgbd = next(
            item
            for item in reference_scenario_evidence()
            if item.modality is FusionModality.RGBD
        )
        result = fuse_track_evidence_v2(
            reference_scenario_evidence(),
            weather=WEATHER_PROFILES["monsoon_heavy_rain"],
            as_of=REFERENCE_AS_OF,
        )
        contribution = next(
            item for item in result.contributions if item.modality is FusionModality.RGBD
        )
        self.assertEqual(contribution.raw_model_confidence, rgbd.raw_model_confidence)
        self.assertEqual(contribution.source_reliability_assumption, 0.90)
        self.assertEqual(contribution.weather_reliability_multiplier, 0.62)
        self.assertAlmostEqual(contribution.pre_weather_evidence_score, 0.88 * 0.90)
        self.assertAlmostEqual(
            contribution.weather_adjusted_evidence_score, 0.88 * 0.90 * 0.62
        )
        payload = result.to_dict()
        self.assertIn("not a calibrated probability", payload["score_semantics"])
        self.assertTrue(payload["reliability_values_are_scenario_assumptions"])

    def test_correlated_repeats_never_inflate_score(self) -> None:
        timestamp = REFERENCE_AS_OF - timedelta(milliseconds=50)
        first = FusionEvidenceV2(
            evidence_id="rgbd-1",
            timestamp=timestamp,
            target_track_id="T-1",
            node_id="City",
            modality=FusionModality.RGBD,
            raw_model_confidence=0.80,
            source_reliability_assumption=0.90,
            score_origin="unit-test",
            correlation_group="same-frame-window",
        )
        second = FusionEvidenceV2(
            evidence_id="rgbd-2",
            timestamp=timestamp,
            target_track_id="T-1",
            node_id="City",
            modality=FusionModality.RGBD,
            raw_model_confidence=0.70,
            source_reliability_assumption=0.90,
            score_origin="unit-test",
            correlation_group="same-frame-window",
        )
        policy = FusionPolicyV2(minimum_modality_families=1)
        one = fuse_track_evidence_v2(
            [first], weather=WEATHER_PROFILES["clear"], as_of=REFERENCE_AS_OF, policy=policy
        )
        repeated = fuse_track_evidence_v2(
            [first, second],
            weather=WEATHER_PROFILES["clear"],
            as_of=REFERENCE_AS_OF,
            policy=policy,
        )
        self.assertEqual(repeated.fused_evidence_score, one.fused_evidence_score)
        contribution = repeated.contributions[0]
        self.assertEqual(contribution.input_evidence_count, 2)
        self.assertEqual(contribution.correlation_group_count, 1)
        self.assertEqual(contribution.correlated_samples_not_stacked, 1)
        self.assertEqual(contribution.non_selected_correlation_groups_not_stacked, 0)

    def test_two_rf_bands_count_as_one_corroboration_family(self) -> None:
        rf_only = [item for item in reference_scenario_evidence() if item.modality.family == "rf"]
        result = fuse_track_evidence_v2(
            rf_only, weather=WEATHER_PROFILES["clear"], as_of=REFERENCE_AS_OF
        )
        self.assertEqual(result.active_modality_families, ("rf",))
        self.assertEqual(result.decision, "hold_for_corroboration")
        self.assertFalse(result.detection_alert)

    def test_separate_correlation_groups_are_also_not_stacked(self) -> None:
        timestamp = REFERENCE_AS_OF - timedelta(milliseconds=50)
        items = [
            FusionEvidenceV2(
                evidence_id=f"rgbd-{index}",
                timestamp=timestamp,
                target_track_id="T-2",
                node_id=f"Node-{index}",
                modality=FusionModality.RGBD,
                raw_model_confidence=raw_score,
                source_reliability_assumption=0.90,
                score_origin="unit-test",
                correlation_group=f"independent-not-proven-{index}",
            )
            for index, raw_score in ((1, 0.80), (2, 0.85))
        ]
        result = fuse_track_evidence_v2(
            items,
            weather=WEATHER_PROFILES["clear"],
            as_of=REFERENCE_AS_OF,
            policy=FusionPolicyV2(minimum_modality_families=1),
        )
        self.assertAlmostEqual(result.fused_evidence_score, 0.85 * 0.90)
        contribution = result.contributions[0]
        self.assertEqual(contribution.correlation_group_count, 2)
        self.assertEqual(contribution.non_selected_correlation_groups_not_stacked, 1)
        self.assertLess(result.fused_evidence_score, sum(item.pre_weather_evidence_score for item in items))

    def test_cross_track_fusion_is_refused(self) -> None:
        items = list(reference_scenario_evidence())
        first = items[0]
        items[0] = FusionEvidenceV2(
            evidence_id=first.evidence_id,
            timestamp=first.timestamp,
            target_track_id="DIFFERENT-TRACK",
            node_id=first.node_id,
            modality=first.modality,
            raw_model_confidence=first.raw_model_confidence,
            source_reliability_assumption=first.source_reliability_assumption,
            score_origin=first.score_origin,
            model_id=first.model_id,
            frequency_hz=first.frequency_hz,
            correlation_group=first.correlation_group,
        )
        with self.assertRaisesRegex(ValueError, "different target tracks"):
            fuse_track_evidence_v2(
                items, weather=WEATHER_PROFILES["clear"], as_of=REFERENCE_AS_OF
            )

    def test_stale_and_future_inputs_are_visible_but_excluded(self) -> None:
        base = reference_scenario_evidence()[0]
        stale = FusionEvidenceV2(
            evidence_id="stale",
            timestamp=REFERENCE_AS_OF - timedelta(seconds=2),
            target_track_id=base.target_track_id,
            node_id=base.node_id,
            modality=base.modality,
            raw_model_confidence=base.raw_model_confidence,
            source_reliability_assumption=base.source_reliability_assumption,
            score_origin=base.score_origin,
            frequency_hz=base.frequency_hz,
        )
        future = FusionEvidenceV2(
            evidence_id="future",
            timestamp=REFERENCE_AS_OF + timedelta(seconds=1),
            target_track_id=base.target_track_id,
            node_id=base.node_id,
            modality=base.modality,
            raw_model_confidence=base.raw_model_confidence,
            source_reliability_assumption=base.source_reliability_assumption,
            score_origin=base.score_origin,
            frequency_hz=base.frequency_hz,
        )
        result = fuse_track_evidence_v2(
            [stale, future], weather=WEATHER_PROFILES["clear"], as_of=REFERENCE_AS_OF
        )
        self.assertEqual(result.decision, "no_current_evidence")
        self.assertEqual(result.stale_evidence_count, 1)
        self.assertEqual(result.future_evidence_count, 1)
        self.assertEqual(result.eligible_evidence_count, 0)

    def test_conversion_does_not_square_legacy_confidence(self) -> None:
        observation = SensorObservation(
            observation_id="obs-rf",
            timestamp=datetime(2026, 8, 1, tzinfo=timezone.utc),
            node_id="City",
            modality=SensorModality.RF,
            detection_probability=0.8,
            confidence_level=0.8,
            target_track_id="T-9",
            frequency_hz=5.8e9,
            model_id="analytic-fallback",
        )
        converted = FusionEvidenceV2.from_sensor_observation(
            observation, source_reliability_assumption=0.6
        )
        self.assertEqual(converted.modality, FusionModality.RF_5_8_GHZ)
        self.assertEqual(converted.raw_model_confidence, 0.8)
        self.assertEqual(converted.source_reliability_assumption, 0.6)
        self.assertEqual(
            converted.metadata["legacy_observation_confidence_level_not_reused"], 0.8
        )
        self.assertAlmostEqual(converted.pre_weather_evidence_score, 0.48)

    def test_conversion_requires_target_association(self) -> None:
        observation = SensorObservation(
            observation_id="obs-unassociated",
            timestamp=datetime(2026, 8, 1, tzinfo=timezone.utc),
            node_id="City",
            modality=SensorModality.RGBD,
            detection_probability=0.8,
            confidence_level=0.8,
        )
        with self.assertRaisesRegex(ValueError, "target_track_id"):
            FusionEvidenceV2.from_sensor_observation(
                observation, source_reliability_assumption=0.7
            )

    def test_weather_sweep_is_deterministic_and_json_safe(self) -> None:
        first = run_reference_weather_sweep()
        second = run_reference_weather_sweep()
        self.assertEqual(first, second)
        encoded = json.dumps(first, sort_keys=True, allow_nan=False)
        self.assertIn(SCENARIO_ASSUMPTION_NOTICE, encoded)
        results = {item["weather"]["profile_id"]: item for item in first["results"]}
        self.assertEqual(
            tuple(results), ("clear", "light_rain", "monsoon_heavy_rain", "haze_fog")
        )
        clear_rgbd = next(
            item for item in results["clear"]["contributions"] if item["modality"] == "rgbd"
        )
        monsoon_rgbd = next(
            item
            for item in results["monsoon_heavy_rain"]["contributions"]
            if item["modality"] == "rgbd"
        )
        self.assertLess(
            monsoon_rgbd["weather_adjusted_evidence_score"],
            clear_rgbd["weather_adjusted_evidence_score"],
        )
        self.assertEqual(results["clear"]["dominant_modality"], "rgbd")
        self.assertEqual(results["monsoon_heavy_rain"]["dominant_modality"], "rf_2_4_ghz")

    def test_sweep_writer_is_atomic(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "nested" / "sweep.json"
            written = write_json_atomic(run_reference_weather_sweep(), output)
            self.assertEqual(written, output.resolve())
            self.assertTrue(json.loads(output.read_text(encoding="utf-8"))["deterministic_fixture"])
            self.assertEqual(list(output.parent.glob("*.tmp")), [])


if __name__ == "__main__":
    unittest.main()
