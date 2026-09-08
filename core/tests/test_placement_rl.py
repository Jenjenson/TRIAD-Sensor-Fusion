from __future__ import annotations

from dataclasses import replace
import unittest

from singapore_sensor_fusion.placement.contracts import (
    AreaOfInterest,
    CandidateOption,
    CoverageEvaluation,
    CoverageSample,
    GeoPoint,
    PlacementConstraints,
    PlacementRequest,
    PlacementScenario,
    PlacementSurvey,
    SamplingPlan,
    SensorPackage,
)
from singapore_sensor_fusion.placement.rl import (
    DEFAULT_SINGAPORE_WEATHER_PROFILES,
    RLPlacementConfig,
    WeatherAwarePlacementEnv,
    train_weather_robust_q_learning,
)
from singapore_sensor_fusion.placement.optimizer import solve_placement


def _fixture(*, weather_sensitive: bool = True) -> tuple[PlacementRequest, PlacementSurvey]:
    center = GeoPoint(1.307, 103.843, 10.0)
    request = PlacementRequest(
        request_id="rl-test",
        aoi=AreaOfInterest("aoi", center, 500.0),
        sampling=SamplingPlan(50.0, (30.0,)),
        sensor_packages=(
            SensorPackage(
                package_id="radar",
                modalities=("SEARCH_RADAR",),
                families=("active_radar",),
                cost_units=1.0,
            ),
        ),
        scenarios=(
            PlacementScenario("clear", "Clear", True, 0.03, 0.5),
            PlacementScenario("monsoon", "Monsoon", True, 0.03, 0.5),
        ),
        constraints=PlacementConstraints(
            maximum_sites=2,
            minimum_family_count=1,
            minimum_site_redundancy=1,
            minimum_failure_domain_redundancy=1,
            minimum_coverage_fraction=1.0,
            budget_units=2.0,
        ),
        random_seed=17,
    )
    options = (
        CandidateOption("a-east", "site-a", "radar", "east", center, "domain-a", camera_yaw_degrees=90.0),
        CandidateOption("a-west", "site-a", "radar", "west", center, "domain-a", camera_yaw_degrees=270.0),
        CandidateOption("b-east", "site-b", "radar", "east", center, "domain-b", camera_yaw_degrees=90.0),
        CandidateOption("b-west", "site-b", "radar", "west", center, "domain-b", camera_yaw_degrees=270.0),
    )
    samples = (
        CoverageSample("east", center, 100.0, 0.0, 30.0),
        CoverageSample("west", center, -100.0, 0.0, 30.0),
    )
    covered = {
        ("a-east", "east"),
        ("b-east", "east"),
        ("b-west", "west"),
    }
    evaluations = []
    for scenario_id in ("clear", "monsoon"):
        for option in options:
            for sample in samples:
                eligible = (option.option_id, sample.sample_id) in covered
                if scenario_id == "clear" or not weather_sensitive:
                    quality = 0.90 if eligible else 0.10
                else:
                    quality = 0.72 if eligible else 0.08
                evaluations.append(
                    CoverageEvaluation(
                        option_id=option.option_id,
                        sample_id=sample.sample_id,
                        scenario_id=scenario_id,
                        modality="SEARCH_RADAR",
                        family="active_radar",
                        quality=quality,
                        eligible=eligible,
                        range_meters=100.0,
                        maximum_range_meters=1_000.0,
                        bearing_degrees=90.0 if sample.sample_id == "east" else 270.0,
                        within_range=True,
                        within_fov=eligible,
                        line_of_sight=True,
                        line_of_sight_required=False,
                        source="SYNTHETIC_TEST_SIMULATOR",
                    )
                )
    survey = PlacementSurvey(
        request_id=request.request_id,
        options=options,
        samples=samples,
        evaluations=tuple(evaluations),
        world={
            "rlPlacementReadiness": {
                "rfGeometryReady": True,
                "sensorModelsCalibrated": True,
                "weatherDifferentiated": True,
                "orientationSweepEvaluated": True,
                "exactSimulatorReplayRequired": True,
                "source": "synthetic-unit-test-only",
            }
        },
    )
    return request, survey


class PlacementRLTests(unittest.TestCase):
    def test_singapore_weather_profiles_keep_humidity_separate(self) -> None:
        clear = DEFAULT_SINGAPORE_WEATHER_PROFILES["Clear"]
        monsoon = DEFAULT_SINGAPORE_WEATHER_PROFILES["Monsoon"]
        self.assertGreater(monsoon.relative_humidity_percent, clear.relative_humidity_percent)
        self.assertGreater(monsoon.rain_rate_mm_h, clear.rain_rate_mm_h)
        self.assertLess(monsoon.visibility_meters, clear.visibility_meters)
        self.assertGreater(monsoon.water_vapour_density_g_m3, 0.0)
        self.assertGreater(monsoon.optical_extinction_per_meter, clear.optical_extinction_per_meter)
        self.assertTrue(monsoon.to_dict()["notAWeatherForecast"])

    def test_action_mask_enforces_one_orientation_per_site_and_budget(self) -> None:
        request, survey = _fixture()
        env = WeatherAwarePlacementEnv(request, survey)
        env.reset()
        a_east = next(index for index, item in enumerate(env.options) if item.option_id == "a-east")
        a_west = next(index for index, item in enumerate(env.options) if item.option_id == "a-west")
        env.step(a_east)
        self.assertFalse(env.valid_action_mask()[a_west])
        with self.assertRaisesRegex(ValueError, "invalid"):
            env.step(a_west)

    def test_weather_insensitive_survey_fails_closed(self) -> None:
        request, survey = _fixture(weather_sensitive=False)
        with self.assertRaisesRegex(ValueError, "weather-insensitive"):
            WeatherAwarePlacementEnv(request, survey)

    def test_missing_readiness_attestation_fails_closed(self) -> None:
        request, survey = _fixture()
        with self.assertRaisesRegex(ValueError, "readiness attestation"):
            WeatherAwarePlacementEnv(request, replace(survey, world={}))

    def test_q_learning_selects_positions_and_orientations(self) -> None:
        request, survey = _fixture()
        result = train_weather_robust_q_learning(
            request,
            survey,
            config=RLPlacementConfig(episodes=2_000, random_seed=9),
        )
        self.assertEqual(result.status, "FEASIBLE")
        self.assertEqual(result.selected_option_ids, ("a-east", "b-west"))
        self.assertEqual(result.metrics["worstScenarioCoverageFraction"], 1.0)
        payload = result.to_dict()
        self.assertFalse(payload["optimalityCertified"])
        self.assertFalse(payload["physicalDeploymentAuthorized"])
        self.assertTrue(payload["exactSimulatorReplayRequired"])
        self.assertEqual(len(payload["resultDigest"]), 64)

    def test_small_problem_matches_exhaustive_solver_and_is_deterministic(self) -> None:
        request, survey = _fixture()
        config = RLPlacementConfig(episodes=2_000, random_seed=9)
        first = train_weather_robust_q_learning(request, survey, config=config)
        second = train_weather_robust_q_learning(request, survey, config=config)
        exact = solve_placement(request, survey, exact_option_limit=8)

        self.assertEqual(first.selected_option_ids, exact.selected_option_ids)
        self.assertEqual(first.to_dict(), second.to_dict())


if __name__ == "__main__":
    unittest.main()
