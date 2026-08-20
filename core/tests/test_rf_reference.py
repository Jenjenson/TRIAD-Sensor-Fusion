import math
import unittest

from singapore_sensor_fusion.rf import (
    ANALYTIC_FALLBACK_METHOD,
    DeterministicShadowing,
    LinkContext,
    analytic_energy_detector_probability,
    fixed_los_nlos_loss_hook,
    fspl_db,
    log_distance_path_loss_db,
    received_power_dbm,
    sum_powers_dbm,
    thermal_noise_dbm,
)


class RFReferenceTests(unittest.TestCase):
    def test_fspl_reference_at_one_kilometre(self) -> None:
        cases = (
            (2.4e9, 100.0520080561155),
            (5.8e9, 107.71634309314211),
        )
        for frequency_hz, expected_db in cases:
            with self.subTest(frequency_hz=frequency_hz):
                self.assertAlmostEqual(fspl_db(1_000.0, frequency_hz), expected_db, places=11)

    def test_log_distance_matches_fspl_for_exponent_two(self) -> None:
        self.assertAlmostEqual(
            log_distance_path_loss_db(1234.5, 2.4e9, path_loss_exponent=2.0),
            fspl_db(1234.5, 2.4e9),
            places=11,
        )

    def test_thermal_noise_reference_cases(self) -> None:
        # Exact kTB with the defined Boltzmann constant, not rounded -174 dBm/Hz.
        self.assertAlmostEqual(
            thermal_noise_dbm(1.0, temperature_k=290.0),
            -173.97518719422808,
            places=11,
        )
        self.assertAlmostEqual(
            thermal_noise_dbm(20e6, temperature_k=290.0, noise_figure_db=5.0),
            -95.9648872375883,
            places=11,
        )

    def test_dbm_interference_sum_is_linear(self) -> None:
        self.assertAlmostEqual(sum_powers_dbm((-30.0, -30.0)), -26.989700043360187, places=11)
        self.assertAlmostEqual(sum_powers_dbm((-math.inf, -50.0)), -50.0)
        self.assertEqual(sum_powers_dbm(()), -math.inf)

    def test_scene_hook_adds_only_configured_nlos_loss(self) -> None:
        los = LinkContext("drone-1", "node-1", 1_000.0, 2.4e9, 10.0, line_of_sight=True)
        nlos = LinkContext("drone-1", "node-1", 1_000.0, 2.4e9, 10.0, line_of_sight=False)
        hook = fixed_los_nlos_loss_hook(nlos_excess_loss_db=18.0)
        los_power = received_power_dbm(20.0, los, additional_loss_hook=hook)
        nlos_power = received_power_dbm(20.0, nlos, additional_loss_hook=hook)
        self.assertAlmostEqual(los_power - nlos_power, 18.0)

    def test_shadowing_is_stable_symmetric_and_time_correlated(self) -> None:
        shadow = DeterministicShadowing(seed=712, sigma_db=5.0, coherence_time_s=1.0)
        first = shadow.sample_db("drone-1", "node-1", 12.25)
        self.assertEqual(first, shadow.sample_db("drone-1", "node-1", 12.25))
        self.assertEqual(first, shadow.sample_db("node-1", "drone-1", 12.25))
        self.assertLess(
            abs(
                shadow.sample_db("drone-1", "node-1", 12.999)
                - shadow.sample_db("drone-1", "node-1", 13.001)
            ),
            0.01,
        )
        self.assertNotEqual(first, shadow.sample_db("drone-2", "node-1", 12.25))

    def test_analytic_fallback_is_labeled_and_monotonic(self) -> None:
        low = analytic_energy_detector_probability(-20.0, 256, false_alarm_probability=1e-3)
        high = analytic_energy_detector_probability(-5.0, 256, false_alarm_probability=1e-3)
        self.assertGreater(high.probability_detection, low.probability_detection)
        self.assertEqual(low.method, ANALYTIC_FALLBACK_METHOD)
        self.assertFalse(low.is_learned_model_score)
        self.assertAlmostEqual(low.false_alarm_probability, 1e-3)


if __name__ == "__main__":
    unittest.main()
