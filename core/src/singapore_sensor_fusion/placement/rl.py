"""Weather-robust, research-only reinforcement learning for sensor placement.

The environment consumes a versioned :class:`PlacementSurvey` exported by the
simulator.  Every candidate option is already a complete position, package,
height, and orientation choice.  An episode builds one layout by selecting
options until it emits ``STOP`` or reaches the declared site limit.

This module deliberately does not invent detection probabilities from humidity
or rain.  The simulator must evaluate every candidate under each declared
weather profile.  Meteorological fields are retained as domain-randomization
inputs and provenance.  Final layouts remain planning evidence and must be
replayed in the exact simulator; no physical deployment is authorized here.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import math
from pathlib import Path
import random
from types import MappingProxyType
from typing import Any, Mapping, Sequence

from .contracts import (
    CandidateOption,
    PlacementRequest,
    PlacementSurvey,
    stable_digest,
)
from .coverage import build_eligible_evaluation_index, evaluate_selection
from .optimizer import option_installed_cost, selection_cost


RL_ALGORITHM = "masked_weather_robust_tabular_q_learning_v1"
RL_READINESS_KEY = "rlPlacementReadiness"


def _finite(name: str, value: object) -> float:
    if isinstance(value, bool):
        raise TypeError(f"{name} must be numeric")
    result = float(value)
    if not math.isfinite(result):
        raise ValueError(f"{name} must be finite")
    return result


def _unit(name: str, value: object) -> float:
    result = _finite(name, value)
    if not 0.0 <= result <= 1.0:
        raise ValueError(f"{name} must be in [0, 1]")
    return result


def _positive_integer(name: str, value: object) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value <= 0:
        raise ValueError(f"{name} must be a positive integer")
    return value


@dataclass(frozen=True, slots=True)
class SingaporeWeatherProfile:
    """One explicit simulator stress condition, not a climatological forecast."""

    profile_id: str
    display_name: str
    temperature_celsius: float
    relative_humidity_percent: float
    rain_rate_mm_h: float
    visibility_meters: float
    source_semantics: str = "DECLARED_SIMULATION_STRESS_PROFILE"

    def __post_init__(self) -> None:
        for name in ("profile_id", "display_name", "source_semantics"):
            value = getattr(self, name)
            if not isinstance(value, str) or not value.strip():
                raise ValueError(f"{name} must be non-empty text")
            object.__setattr__(self, name, value.strip())
        temperature = _finite("temperature_celsius", self.temperature_celsius)
        if not -20.0 <= temperature <= 60.0:
            raise ValueError("temperature_celsius must be in [-20, 60]")
        object.__setattr__(self, "temperature_celsius", temperature)
        humidity = _finite(
            "relative_humidity_percent", self.relative_humidity_percent
        )
        if not 0.0 <= humidity <= 100.0:
            raise ValueError("relative_humidity_percent must be in [0, 100]")
        object.__setattr__(self, "relative_humidity_percent", humidity)
        rain = _finite("rain_rate_mm_h", self.rain_rate_mm_h)
        if rain < 0.0:
            raise ValueError("rain_rate_mm_h must be >= 0")
        object.__setattr__(self, "rain_rate_mm_h", rain)
        visibility = _finite("visibility_meters", self.visibility_meters)
        if visibility <= 0.0:
            raise ValueError("visibility_meters must be > 0")
        object.__setattr__(self, "visibility_meters", visibility)

    @property
    def water_vapour_density_g_m3(self) -> float:
        """Approximate surface water-vapour density for simulator input.

        This uses the Buck saturation-vapour-pressure approximation.  It is an
        atmospheric input, not an RF attenuation model.  RF propagation should
        use the applicable ITU-R P.676, P.838, and P.840 procedures.
        """

        t = self.temperature_celsius
        saturation_hpa = 6.1121 * math.exp(
            (18.678 - t / 234.5) * (t / (257.14 + t))
        )
        vapour_pressure_hpa = saturation_hpa * (
            self.relative_humidity_percent / 100.0
        )
        return 216.7 * vapour_pressure_hpa / (t + 273.15)

    @property
    def optical_extinction_per_meter(self) -> float:
        """Koschmieder 2%-contrast extinction input for visual simulation."""

        return 3.912 / self.visibility_meters

    def to_dict(self) -> dict[str, object]:
        return {
            "profileId": self.profile_id,
            "displayName": self.display_name,
            "temperatureCelsius": self.temperature_celsius,
            "relativeHumidityPercent": self.relative_humidity_percent,
            "rainRateMmH": self.rain_rate_mm_h,
            "visibilityMeters": self.visibility_meters,
            "waterVapourDensityGM3": round(
                self.water_vapour_density_g_m3, 6
            ),
            "opticalExtinctionPerMeter": round(
                self.optical_extinction_per_meter, 12
            ),
            "rfPropagationReferences": [
                "ITU-R P.676-13 atmospheric gases",
                "ITU-R P.838-3 rain",
                "ITU-R P.840-9 cloud and fog",
            ],
            "sourceSemantics": self.source_semantics,
            "notAWeatherForecast": True,
        }


# These are deterministic stress-test inputs within Singapore's broad climate
# envelope.  They are not occurrence probabilities and must not be described as
# calibrated sensor effects.  MSS reports a 1991-2020 annual mean RH near 82%,
# >90% before sunrise, around 60% on dry afternoons, and frequent heavy rain.
DEFAULT_SINGAPORE_WEATHER_PROFILES: Mapping[
    str, SingaporeWeatherProfile
] = MappingProxyType(
    {
        profile.profile_id: profile
        for profile in (
            SingaporeWeatherProfile(
                profile_id="Clear",
                display_name="Tropical daylight",
                temperature_celsius=31.0,
                relative_humidity_percent=70.0,
                rain_rate_mm_h=0.0,
                visibility_meters=30_000.0,
            ),
            SingaporeWeatherProfile(
                profile_id="HumidMorningMist",
                display_name="Humid pre-sunrise mist stress",
                temperature_celsius=24.5,
                relative_humidity_percent=95.0,
                rain_rate_mm_h=0.0,
                visibility_meters=5_000.0,
            ),
            SingaporeWeatherProfile(
                profile_id="Monsoon",
                display_name="Heavy tropical rain stress",
                temperature_celsius=26.0,
                relative_humidity_percent=98.0,
                rain_rate_mm_h=50.0,
                visibility_meters=1_500.0,
            ),
            SingaporeWeatherProfile(
                profile_id="Haze",
                display_name="Smoke-haze visibility stress",
                temperature_celsius=30.0,
                relative_humidity_percent=75.0,
                rain_rate_mm_h=0.0,
                visibility_meters=3_000.0,
            ),
        )
    }
)


@dataclass(frozen=True, slots=True)
class RLPlacementConfig:
    episodes: int = 8_000
    learning_rate: float = 0.25
    discount_factor: float = 1.0
    epsilon_start: float = 0.35
    epsilon_end: float = 0.02
    random_seed: int = 0
    require_readiness_attestation: bool = True
    require_weather_differentiation: bool = True
    require_orientation_search: bool = True

    def __post_init__(self) -> None:
        object.__setattr__(self, "episodes", _positive_integer("episodes", self.episodes))
        object.__setattr__(
            self, "learning_rate", _unit("learning_rate", self.learning_rate)
        )
        object.__setattr__(
            self, "discount_factor", _unit("discount_factor", self.discount_factor)
        )
        object.__setattr__(
            self, "epsilon_start", _unit("epsilon_start", self.epsilon_start)
        )
        object.__setattr__(
            self, "epsilon_end", _unit("epsilon_end", self.epsilon_end)
        )
        if self.epsilon_end > self.epsilon_start:
            raise ValueError("epsilon_end must be <= epsilon_start")
        if isinstance(self.random_seed, bool) or not isinstance(self.random_seed, int):
            raise TypeError("random_seed must be an integer")
        for name in (
            "require_readiness_attestation",
            "require_weather_differentiation",
            "require_orientation_search",
        ):
            if not isinstance(getattr(self, name), bool):
                raise TypeError(f"{name} must be boolean")


@dataclass(frozen=True, slots=True)
class RLPlacementResult:
    request_id: str
    status: str
    selected_option_ids: tuple[str, ...]
    selected_options: tuple[Mapping[str, object], ...]
    total_cost_units: float
    metrics: Mapping[str, object]
    episodes: int
    q_state_count: int
    terminal_layout_count: int
    weather_profiles: tuple[Mapping[str, object], ...]
    input_digest: str
    result_digest: str

    def to_dict(self) -> dict[str, object]:
        return {
            "schemaVersion": "triad.rl_placement_result.v1",
            "requestId": self.request_id,
            "status": self.status,
            "algorithm": RL_ALGORITHM,
            "selectedOptionIds": list(self.selected_option_ids),
            "selectedOptions": [dict(item) for item in self.selected_options],
            "totalCostUnits": self.total_cost_units,
            "metrics": dict(self.metrics),
            "training": {
                "episodes": self.episodes,
                "qStateCount": self.q_state_count,
                "terminalLayoutCount": self.terminal_layout_count,
                "selectionSemantics": (
                    "best terminal layout observed during masked Q-learning"
                ),
            },
            "weatherProfiles": [dict(item) for item in self.weather_profiles],
            "inputDigest": self.input_digest,
            "resultDigest": self.result_digest,
            "planningEvidenceOnly": True,
            "physicalDeploymentAuthorized": False,
            "optimalityCertified": False,
            "exactSimulatorReplayRequired": True,
            "scoreSemantics": (
                "simulation evidence indexes, not calibrated probabilities"
            ),
        }


class WeatherAwarePlacementEnv:
    """Finite, action-masked MDP for sequential construction of one layout."""

    def __init__(
        self,
        request: PlacementRequest,
        survey: PlacementSurvey,
        *,
        weather_profiles: Mapping[str, SingaporeWeatherProfile] = (
            DEFAULT_SINGAPORE_WEATHER_PROFILES
        ),
        config: RLPlacementConfig | None = None,
    ) -> None:
        self.request = request
        self.survey = survey
        self.config = config or RLPlacementConfig(random_seed=request.random_seed)
        self._profiles = {
            key.casefold(): value for key, value in weather_profiles.items()
        }
        if len(self._profiles) != len(weather_profiles):
            raise ValueError("weather profile IDs must be unique case-insensitively")
        self._validate_problem()
        self.options = tuple(
            sorted(
                (item for item in survey.options if item.feasible),
                key=lambda item: item.option_id,
            )
        )
        if not self.options:
            raise ValueError("survey contains no feasible candidate options")
        self.options_by_id = {item.option_id: item for item in self.options}
        self.stop_action = len(self.options)
        self._eligible_index = build_eligible_evaluation_index(survey)
        self._metric_cache: dict[tuple[str, ...], dict[str, object]] = {}
        self._state: tuple[int, ...] = ()
        self._done = False

    def _profile_for(self, profile_id: str) -> SingaporeWeatherProfile:
        profile = self._profiles.get(profile_id.casefold())
        if profile is None:
            raise ValueError(
                f"weather profile {profile_id!r} has no quantitative Singapore stress specification"
            )
        return profile

    def _validate_problem(self) -> None:
        if self.request.request_id != self.survey.request_id:
            raise ValueError("request and survey request IDs do not match")
        package_by_id = {
            item.package_id: item for item in self.request.sensor_packages
        }
        scenario_by_id = {
            item.scenario_id: item for item in self.request.scenarios
        }
        for scenario in self.request.scenarios:
            self._profile_for(scenario.weather_profile)
        option_by_id = {item.option_id: item for item in self.survey.options}
        for option in self.survey.options:
            if option.package_id not in package_by_id:
                raise ValueError(
                    f"option {option.option_id!r} references unknown package {option.package_id!r}"
                )
        for row in self.survey.evaluations:
            scenario = scenario_by_id.get(row.scenario_id)
            if scenario is None:
                raise ValueError(
                    f"evaluation references unknown scenario {row.scenario_id!r}"
                )
            option = option_by_id[row.option_id]
            package = package_by_id[option.package_id]
            if row.modality not in package.modalities:
                raise ValueError(
                    f"evaluation modality {row.modality!r} is not in package {package.package_id!r}"
                )
            if not scenario.rf_emitting and row.family == "passive_rf" and row.eligible:
                raise ValueError(
                    f"eligible passive-RF row contradicts RF-silent scenario {row.scenario_id!r}"
                )

        if self.config.require_readiness_attestation:
            readiness = self.survey.world.get(RL_READINESS_KEY)
            if not isinstance(readiness, Mapping):
                raise ValueError(
                    f"survey.world.{RL_READINESS_KEY} readiness attestation is required"
                )
            required_flags = (
                "rfGeometryReady",
                "sensorModelsCalibrated",
                "weatherDifferentiated",
                "orientationSweepEvaluated",
                "exactSimulatorReplayRequired",
            )
            failed = [name for name in required_flags if readiness.get(name) is not True]
            if failed:
                raise ValueError(
                    "RL placement readiness attestation is blocked: "
                    + ", ".join(failed)
                )

        if self.config.require_orientation_search:
            orientations_by_site: dict[str, set[tuple[object, ...]]] = {}
            for option in self.survey.options:
                if not option.feasible:
                    continue
                orientations_by_site.setdefault(option.site_id, set()).add(
                    (
                        option.orientation_id,
                        round(option.camera_pitch_degrees, 6),
                        round(option.camera_yaw_degrees, 6),
                        round(option.camera_roll_degrees, 6),
                    )
                )
            if not any(len(items) > 1 for items in orientations_by_site.values()):
                raise ValueError(
                    "orientation optimization requires multiple evaluated orientations at at least one site"
                )

        if self.config.require_weather_differentiation:
            self._assert_weather_differentiated(scenario_by_id)

    def _assert_weather_differentiated(
        self, scenario_by_id: Mapping[str, object]
    ) -> None:
        signatures: dict[str, tuple[tuple[object, ...], ...]] = {}
        for scenario_id in scenario_by_id:
            signatures[scenario_id] = tuple(
                sorted(
                    (
                        row.option_id,
                        row.sample_id,
                        row.modality,
                        round(row.quality, 9),
                        row.eligible,
                        row.line_of_sight,
                        row.within_fov,
                        row.within_range,
                    )
                    for row in self.survey.evaluations
                    if row.scenario_id == scenario_id
                )
            )
        comparable_pair_found = False
        differentiated_pair_found = False
        scenarios = tuple(self.request.scenarios)
        for left_index, left in enumerate(scenarios):
            for right in scenarios[left_index + 1 :]:
                if left.weather_profile.casefold() == right.weather_profile.casefold():
                    continue
                if left.rf_emitting != right.rf_emitting:
                    continue
                if not math.isclose(
                    left.radar_cross_section_square_meters,
                    right.radar_cross_section_square_meters,
                    rel_tol=0.0,
                    abs_tol=1e-12,
                ):
                    continue
                comparable_pair_found = True
                if signatures[left.scenario_id] != signatures[right.scenario_id]:
                    differentiated_pair_found = True
                    break
            if differentiated_pair_found:
                break
        if not comparable_pair_found:
            raise ValueError(
                "weather-robust RL requires at least two weather scenarios with matching RF-emission and target assumptions"
            )
        if not differentiated_pair_found:
            raise ValueError(
                "survey is weather-insensitive: comparable Clear/Monsoon/Haze evaluations are identical"
            )

    @property
    def state(self) -> tuple[int, ...]:
        return self._state

    @property
    def done(self) -> bool:
        return self._done

    def _selected_ids(self, state: tuple[int, ...] | None = None) -> tuple[str, ...]:
        selected = self._state if state is None else state
        return tuple(self.options[index].option_id for index in selected)

    def _metrics(self, selected_ids: tuple[str, ...]) -> dict[str, object]:
        key = tuple(sorted(selected_ids))
        cached = self._metric_cache.get(key)
        if cached is None:
            cached = evaluate_selection(
                self.request,
                self.survey,
                key,
                eligible_evaluations_by_option=self._eligible_index,
            )
            self._metric_cache[key] = cached
        return cached

    def metrics(self) -> dict[str, object]:
        return dict(self._metrics(self._selected_ids()))

    def cost(self, selected_ids: tuple[str, ...] | None = None) -> float:
        ids = self._selected_ids() if selected_ids is None else selected_ids
        return selection_cost(self.request, self.options_by_id, ids)

    def _cost_normalizer(self) -> float:
        budget = self.request.constraints.budget_units
        if budget is not None and budget > 0.0:
            return budget
        costs = sorted(
            (
                option_installed_cost(self.request, option)
                for option in self.options
            ),
            reverse=True,
        )
        maximum = min(self.request.constraints.maximum_sites, len(costs))
        return max(1.0, sum(costs[:maximum]))

    def potential(self, selected_ids: tuple[str, ...] | None = None) -> float:
        ids = self._selected_ids() if selected_ids is None else selected_ids
        metrics = self._metrics(ids)
        cost_ratio = min(2.0, self.cost(ids) / self._cost_normalizer())
        count_ratio = len(ids) / max(1, self.request.constraints.maximum_sites)
        if metrics["coverageFeasible"] is True:
            # Every feasible layout dominates every non-feasible layout.  Cost
            # then matters, while small robust-coverage terms break ties.
            return (
                2.0
                + 0.25 * float(metrics["worstScenarioCoverageFraction"])
                + 0.10 * float(metrics["weightedCoverageFraction"])
                - 0.20 * cost_ratio
                - 0.02 * count_ratio
            )
        progress = (
            0.28 * float(metrics["worstScenarioComponentProgressFraction"])
            + 0.20 * float(metrics["weightedComponentProgressFraction"])
            + 0.16 * float(metrics["worstScenarioProgressFraction"])
            + 0.12 * float(metrics["weightedProgressFraction"])
            + 0.12 * float(metrics["criticalCoverageFraction"])
            + 0.06 * float(metrics["worstScenarioCoverageFraction"])
            + 0.06 * float(metrics["weightedCoverageFraction"])
        )
        return progress - 0.04 * cost_ratio - 0.01 * count_ratio

    def valid_action_mask(self) -> tuple[bool, ...]:
        mask = [False] * (len(self.options) + 1)
        mask[self.stop_action] = True
        if self._done or len(self._state) >= self.request.constraints.maximum_sites:
            return tuple(mask)
        selected_set = set(self._state)
        selected_options = [self.options[index] for index in self._state]
        selected_sites = {item.site_id for item in selected_options}
        selected_ids = self._selected_ids()
        budget = self.request.constraints.budget_units
        for index, option in enumerate(self.options):
            if index in selected_set or option.site_id in selected_sites:
                continue
            proposed_ids = (*selected_ids, option.option_id)
            if budget is not None and self.cost(proposed_ids) > budget + 1e-9:
                continue
            mask[index] = True
        return tuple(mask)

    def observation(self) -> dict[str, object]:
        metrics = self._metrics(self._selected_ids())
        scenario_features = []
        coverage_by_scenario = metrics["coverageByScenario"]
        for scenario in self.request.scenarios:
            profile = self._profile_for(scenario.weather_profile)
            scenario_metrics = coverage_by_scenario[scenario.scenario_id]
            scenario_features.append(
                {
                    "scenarioId": scenario.scenario_id,
                    "weatherProfile": scenario.weather_profile,
                    "weather": profile.to_dict(),
                    "coverageFraction": scenario_metrics["coverageFraction"],
                    "progressFraction": scenario_metrics["progressFraction"],
                    "componentProgressFraction": scenario_metrics[
                        "componentProgressFraction"
                    ],
                }
            )
        return {
            "selectedOptionIds": list(self._selected_ids()),
            "actionMask": list(self.valid_action_mask()),
            "stopAction": self.stop_action,
            "remainingSiteCapacity": max(
                0,
                self.request.constraints.maximum_sites - len(self._state),
            ),
            "remainingBudgetUnits": (
                None
                if self.request.constraints.budget_units is None
                else max(
                    0.0,
                    self.request.constraints.budget_units - self.cost(),
                )
            ),
            "scenarioFeatures": scenario_features,
            "potential": self.potential(),
        }

    def reset(self) -> dict[str, object]:
        self._state = ()
        self._done = False
        return self.observation()

    def step(self, action: int) -> tuple[dict[str, object], float, bool]:
        if self._done:
            raise RuntimeError("episode is already complete")
        if isinstance(action, bool) or not isinstance(action, int):
            raise TypeError("action must be an integer")
        mask = self.valid_action_mask()
        if action < 0 or action >= len(mask) or not mask[action]:
            raise ValueError(f"action {action} is invalid for the current layout")
        if action == self.stop_action:
            self._done = True
            return self.observation(), 0.0, True

        before = self.potential()
        self._state = tuple(sorted((*self._state, action)))
        after = self.potential()
        next_mask = self.valid_action_mask()
        if (
            len(self._state) >= self.request.constraints.maximum_sites
            or not any(next_mask[:-1])
        ):
            self._done = True
        return self.observation(), after - before, self._done


def _layout_rank(
    env: WeatherAwarePlacementEnv, state: tuple[int, ...]
) -> tuple[float, ...]:
    ids = env._selected_ids(state)
    metrics = env._metrics(ids)
    cost = env.cost(ids)
    if metrics["coverageFeasible"] is True:
        return (
            1.0,
            -round(cost, 9),
            -float(len(ids)),
            float(metrics["worstScenarioCoverageFraction"]),
            float(metrics["weightedCoverageFraction"]),
        )
    return (
        0.0,
        float(metrics["worstScenarioComponentProgressFraction"]),
        float(metrics["weightedComponentProgressFraction"]),
        float(metrics["worstScenarioProgressFraction"]),
        float(metrics["weightedProgressFraction"]),
        float(metrics["criticalCoverageFraction"]),
        float(metrics["worstScenarioCoverageFraction"]),
        float(metrics["weightedCoverageFraction"]),
        -round(cost, 9),
        -float(len(ids)),
    )


def train_weather_robust_q_learning(
    request: PlacementRequest,
    survey: PlacementSurvey,
    *,
    weather_profiles: Mapping[str, SingaporeWeatherProfile] = (
        DEFAULT_SINGAPORE_WEATHER_PROFILES
    ),
    config: RLPlacementConfig | None = None,
) -> RLPlacementResult:
    """Train masked tabular Q-learning and return its best observed layout.

    This is an auditable research baseline for finite candidate sets.  It does
    not certify global optimality.  Large candidate graphs should use a masked
    graph/pointer policy and must still be compared with exact or bounded
    deterministic optimization on identical simulator evidence.
    """

    selected_config = config or RLPlacementConfig(random_seed=request.random_seed)
    env = WeatherAwarePlacementEnv(
        request,
        survey,
        weather_profiles=weather_profiles,
        config=selected_config,
    )
    rng = random.Random(selected_config.random_seed)
    q_values: dict[tuple[tuple[int, ...], int], float] = {}
    visited_states: set[tuple[int, ...]] = set()
    terminal_states: set[tuple[int, ...]] = set()
    best_state: tuple[int, ...] = ()
    best_rank = _layout_rank(env, best_state)

    for episode in range(selected_config.episodes):
        env.reset()
        fraction = episode / max(1, selected_config.episodes - 1)
        epsilon = (
            selected_config.epsilon_start
            + fraction
            * (selected_config.epsilon_end - selected_config.epsilon_start)
        )
        while not env.done:
            state = env.state
            visited_states.add(state)
            mask = env.valid_action_mask()
            valid_actions = [index for index, allowed in enumerate(mask) if allowed]
            if rng.random() < epsilon:
                action = rng.choice(valid_actions)
            else:
                # On an unseen tie, prefer an addition to immediate STOP, then
                # use stable action order for reproducibility.
                action = max(
                    valid_actions,
                    key=lambda item: (
                        q_values.get((state, item), 0.0),
                        item != env.stop_action,
                        -item,
                    ),
                )
            _, reward, done = env.step(action)
            next_state = env.state
            if done:
                target = reward
            else:
                next_mask = env.valid_action_mask()
                next_actions = [
                    index for index, allowed in enumerate(next_mask) if allowed
                ]
                target = reward + selected_config.discount_factor * max(
                    q_values.get((next_state, item), 0.0)
                    for item in next_actions
                )
            key = (state, action)
            old_value = q_values.get(key, 0.0)
            q_values[key] = old_value + selected_config.learning_rate * (
                target - old_value
            )
        terminal_states.add(env.state)
        rank = _layout_rank(env, env.state)
        if rank > best_rank or (rank == best_rank and env.state < best_state):
            best_state = env.state
            best_rank = rank

    selected_ids = env._selected_ids(best_state)
    metrics = env._metrics(selected_ids)
    total_cost = env.cost(selected_ids)
    option_payloads = tuple(
        env.options_by_id[item].to_dict() for item in selected_ids
    )
    requested_profile_ids = tuple(
        dict.fromkeys(item.weather_profile for item in request.scenarios)
    )
    weather_payloads = tuple(
        env._profile_for(item).to_dict() for item in requested_profile_ids
    )
    input_payload = {
        "request": request.to_dict(),
        "survey": survey.to_dict(),
        "weatherProfiles": list(weather_payloads),
        "algorithm": RL_ALGORITHM,
        "config": {
            "episodes": selected_config.episodes,
            "learningRate": selected_config.learning_rate,
            "discountFactor": selected_config.discount_factor,
            "epsilonStart": selected_config.epsilon_start,
            "epsilonEnd": selected_config.epsilon_end,
            "randomSeed": selected_config.random_seed,
        },
    }
    input_digest = stable_digest(input_payload)
    result_payload = {
        "requestId": request.request_id,
        "selectedOptionIds": list(selected_ids),
        "totalCostUnits": total_cost,
        "metrics": metrics,
        "inputDigest": input_digest,
        "algorithm": RL_ALGORITHM,
    }
    result_digest = stable_digest(result_payload)
    return RLPlacementResult(
        request_id=request.request_id,
        status="FEASIBLE" if metrics["coverageFeasible"] is True else "UNRESOLVED",
        selected_option_ids=selected_ids,
        selected_options=option_payloads,
        total_cost_units=total_cost,
        metrics=metrics,
        episodes=selected_config.episodes,
        q_state_count=len(visited_states),
        terminal_layout_count=len(terminal_states),
        weather_profiles=weather_payloads,
        input_digest=input_digest,
        result_digest=result_digest,
    )


def _write_new_json(path: Path, payload: Mapping[str, Any]) -> Path:
    destination = path.expanduser().resolve()
    destination.parent.mkdir(parents=True, exist_ok=True)
    encoded = json.dumps(payload, indent=2, sort_keys=True, allow_nan=False) + "\n"
    with destination.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(encoded)
    return destination


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Train research-only weather-robust masked Q-learning on a "
            "simulation placement survey."
        )
    )
    parser.add_argument("request", type=Path)
    parser.add_argument("survey", type=Path)
    parser.add_argument("--episodes", type=int, default=8_000)
    parser.add_argument("--seed", type=int)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)

    request = PlacementRequest.load(args.request)
    survey = PlacementSurvey.load(args.survey)
    result = train_weather_robust_q_learning(
        request,
        survey,
        config=RLPlacementConfig(
            episodes=args.episodes,
            random_seed=request.random_seed if args.seed is None else args.seed,
        ),
    )
    payload = result.to_dict()
    if args.output is None:
        print(json.dumps(payload, indent=2, sort_keys=True, allow_nan=False))
    else:
        written = _write_new_json(args.output, payload)
        print(written)
    return 0 if result.status == "FEASIBLE" else 2


if __name__ == "__main__":
    raise SystemExit(main())
