"""Geometry gates and family-aware coverage accounting."""

from __future__ import annotations

import math
from typing import Iterable, Mapping

from .contracts import (
    CoverageEvaluation,
    PlacementRequest,
    PlacementSurvey,
    family_for_modality,
)


def angular_difference_degrees(left: float, right: float) -> float:
    """Return the smallest absolute separation between two headings."""

    left_value = float(left)
    right_value = float(right)
    if not math.isfinite(left_value) or not math.isfinite(right_value):
        raise ValueError("angles must be finite")
    return abs((left_value - right_value + 180.0) % 360.0 - 180.0)


def within_horizontal_fov(
    *, bearing_degrees: float, boresight_degrees: float, horizontal_fov_degrees: float
) -> bool:
    fov = float(horizontal_fov_degrees)
    if not math.isfinite(fov) or not 0.0 < fov <= 360.0:
        raise ValueError("horizontal_fov_degrees must be in (0, 360]")
    return angular_difference_degrees(bearing_degrees, boresight_degrees) <= fov * 0.5 + 1e-9


def build_coverage_evaluation(
    *,
    option_id: str,
    sample_id: str,
    scenario_id: str,
    modality: str,
    quality: float,
    range_meters: float,
    maximum_range_meters: float,
    bearing_degrees: float = 0.0,
    elevation_degrees: float = 0.0,
    boresight_degrees: float | None = None,
    horizontal_fov_degrees: float | None = None,
    line_of_sight: bool = True,
    line_of_sight_required: bool | None = None,
    minimum_quality: float = 0.35,
    source: str = "UNREAL_SURVEY",
) -> CoverageEvaluation:
    """Build one fail-closed coverage row from explicit geometry/model inputs."""

    distance = float(range_meters)
    maximum = float(maximum_range_meters)
    score = float(quality)
    threshold = float(minimum_quality)
    if not all(math.isfinite(item) for item in (distance, maximum, score, threshold)):
        raise ValueError("range, maximum range, quality, and threshold must be finite")
    if distance < 0.0 or maximum <= 0.0:
        raise ValueError("range_meters must be >= 0 and maximum_range_meters must be > 0")
    if not 0.0 <= score <= 1.0 or not 0.0 <= threshold <= 1.0:
        raise ValueError("quality and minimum_quality must be in [0, 1]")
    if not isinstance(line_of_sight, bool):
        raise TypeError("line_of_sight must be boolean")
    family = family_for_modality(modality)
    requires_los = family == "visual" if line_of_sight_required is None else line_of_sight_required
    if not isinstance(requires_los, bool):
        raise TypeError("line_of_sight_required must be boolean")
    in_range = distance <= maximum + 1e-9
    if (boresight_degrees is None) != (horizontal_fov_degrees is None):
        raise ValueError("boresight_degrees and horizontal_fov_degrees must be supplied together")
    in_fov = (
        True
        if boresight_degrees is None
        else within_horizontal_fov(
            bearing_degrees=bearing_degrees,
            boresight_degrees=boresight_degrees,
            horizontal_fov_degrees=horizontal_fov_degrees,
        )
    )
    eligible = in_range and in_fov and score >= threshold and (line_of_sight or not requires_los)
    return CoverageEvaluation(
        option_id=option_id,
        sample_id=sample_id,
        scenario_id=scenario_id,
        modality=modality,
        family=family,
        quality=score,
        eligible=eligible,
        range_meters=distance,
        maximum_range_meters=maximum,
        bearing_degrees=bearing_degrees,
        elevation_degrees=elevation_degrees,
        within_range=in_range,
        within_fov=in_fov,
        line_of_sight=line_of_sight,
        line_of_sight_required=requires_los,
        source=source,
    )


def active_families(evaluations: Iterable[CoverageEvaluation]) -> tuple[str, ...]:
    return tuple(sorted({item.family for item in evaluations if item.eligible}))


def build_eligible_evaluation_index(
    survey: PlacementSurvey,
) -> dict[str, tuple[CoverageEvaluation, ...]]:
    """Index eligible rows once for repeated optimization evaluations."""

    rows: dict[str, list[CoverageEvaluation]] = {}
    for item in survey.evaluations:
        if item.eligible:
            rows.setdefault(item.option_id, []).append(item)
    return {
        option_id: tuple(
            sorted(
                values,
                key=lambda item: (
                    item.sample_id,
                    item.scenario_id,
                    item.family,
                    item.modality,
                ),
            )
        )
        for option_id, values in rows.items()
    }


def evaluate_selection(
    request: PlacementRequest,
    survey: PlacementSurvey,
    selected_option_ids: Iterable[str],
    *,
    eligible_evaluations_by_option: Mapping[
        str, tuple[CoverageEvaluation, ...]
    ]
    | None = None,
) -> dict[str, object]:
    """Evaluate fusion-family coverage and independent-site resilience."""

    if request.request_id != survey.request_id:
        raise ValueError("request and survey request IDs do not match")
    selected_ids = tuple(sorted(set(selected_option_ids)))
    option_by_id = {item.option_id: item for item in survey.options}
    missing = [item for item in selected_ids if item not in option_by_id]
    if missing:
        raise ValueError(f"unknown selected option IDs: {missing}")
    selected_options = [option_by_id[item] for item in selected_ids]
    selected_sites = [item.site_id for item in selected_options]
    if len(set(selected_sites)) != len(selected_sites):
        raise ValueError("at most one selected option is allowed per physical site")

    eligible_by_pair: dict[tuple[str, str], list[CoverageEvaluation]] = {}
    scenario_by_id = {item.scenario_id: item for item in request.scenarios}
    index = (
        build_eligible_evaluation_index(survey)
        if eligible_evaluations_by_option is None
        else eligible_evaluations_by_option
    )
    for option_id in selected_ids:
        for item in index.get(option_id, ()):
            if not item.eligible:
                continue
            scenario = scenario_by_id.get(item.scenario_id)
            if (
                scenario is not None
                and not scenario.rf_emitting
                and item.family == "passive_rf"
            ):
                raise ValueError(
                    f"eligible passive-RF evaluation {item.option_id!r}/"
                    f"{item.sample_id!r} contradicts RF-silent scenario "
                    f"{item.scenario_id!r}"
                )
            eligible_by_pair.setdefault((item.sample_id, item.scenario_id), []).append(item)

    critical_ids = set(request.constraints.critical_sample_ids)
    critical_ids.update(item.sample_id for item in survey.samples if item.critical)
    required_families = request.constraints.minimum_family_count
    required_sites = request.constraints.minimum_site_redundancy
    required_domains = request.constraints.minimum_failure_domain_redundancy

    total_scenario_weight = sum(item.weight for item in request.scenarios)
    overall_total_weight = 0.0
    overall_covered_weight = 0.0
    overall_progress_weight = 0.0
    overall_component_progress_weight = 0.0
    covered_count = 0
    pair_count = 0
    critical_count = 0
    covered_critical_count = 0
    uncovered_pairs: list[str] = []
    uncovered_critical: list[str] = []
    scenario_metrics: dict[str, dict[str, object]] = {}
    minimum_contributing_sites: int | None = None
    minimum_contributing_domains: int | None = None

    for scenario in request.scenarios:
        scenario_total = sum(item.weight for item in survey.samples)
        scenario_covered = 0.0
        scenario_progress = 0.0
        scenario_component_progress = 0.0
        scenario_count = 0
        for sample in survey.samples:
            pair_count += 1
            scenario_count += 1
            pair_key = (sample.sample_id, scenario.scenario_id)
            rows = eligible_by_pair.get(pair_key, [])
            families = {item.family for item in rows}
            contributing_option_ids = {item.option_id for item in rows}
            contributing_sites = {
                option_by_id[item].site_id for item in contributing_option_ids
            }
            contributing_domains = {
                option_by_id[item].failure_domain_id for item in contributing_option_ids
            }
            family_ratio = min(1.0, len(families) / required_families)
            site_ratio = min(1.0, len(contributing_sites) / required_sites)
            domain_ratio = min(1.0, len(contributing_domains) / required_domains)
            progress = min(family_ratio, site_ratio, domain_ratio)
            component_progress = (family_ratio + site_ratio + domain_ratio) / 3.0
            scenario_progress += sample.weight * progress
            scenario_component_progress += sample.weight * component_progress
            covered = (
                len(families) >= required_families
                and len(contributing_sites) >= required_sites
                and len(contributing_domains) >= required_domains
            )
            label = f"{scenario.scenario_id}/{sample.sample_id}"
            if covered:
                covered_count += 1
                scenario_covered += sample.weight
                minimum_contributing_sites = (
                    len(contributing_sites)
                    if minimum_contributing_sites is None
                    else min(minimum_contributing_sites, len(contributing_sites))
                )
                minimum_contributing_domains = (
                    len(contributing_domains)
                    if minimum_contributing_domains is None
                    else min(minimum_contributing_domains, len(contributing_domains))
                )
            else:
                uncovered_pairs.append(label)
            if sample.sample_id in critical_ids:
                critical_count += 1
                if covered:
                    covered_critical_count += 1
                else:
                    uncovered_critical.append(label)

        fraction = scenario_covered / scenario_total if scenario_total else 0.0
        progress_fraction = scenario_progress / scenario_total if scenario_total else 0.0
        component_progress_fraction = (
            scenario_component_progress / scenario_total if scenario_total else 0.0
        )
        scenario_metrics[scenario.scenario_id] = {
            "coverageFraction": round(fraction, 9),
            "progressFraction": round(progress_fraction, 9),
            "componentProgressFraction": round(component_progress_fraction, 9),
            "coveredSampleCount": sum(
                1
                for sample in survey.samples
                if f"{scenario.scenario_id}/{sample.sample_id}" not in uncovered_pairs
            ),
            "sampleCount": scenario_count,
        }
        weighted_factor = scenario.weight / total_scenario_weight
        overall_total_weight += scenario_total * weighted_factor
        overall_covered_weight += scenario_covered * weighted_factor
        overall_progress_weight += scenario_progress * weighted_factor
        overall_component_progress_weight += scenario_component_progress * weighted_factor

    coverage_values = [float(item["coverageFraction"]) for item in scenario_metrics.values()]
    progress_values = [float(item["progressFraction"]) for item in scenario_metrics.values()]
    component_progress_values = [
        float(item["componentProgressFraction"]) for item in scenario_metrics.values()
    ]
    weighted_fraction = overall_covered_weight / overall_total_weight if overall_total_weight else 0.0
    progress_fraction = overall_progress_weight / overall_total_weight if overall_total_weight else 0.0
    component_progress_fraction = (
        overall_component_progress_weight / overall_total_weight
        if overall_total_weight
        else 0.0
    )
    worst_fraction = min(coverage_values, default=0.0)
    worst_progress = min(progress_values, default=0.0)
    worst_component_progress = min(component_progress_values, default=0.0)
    required_fraction = request.constraints.minimum_coverage_fraction
    coverage_feasible = worst_fraction + 1e-12 >= required_fraction and not uncovered_critical
    critical_fraction = covered_critical_count / critical_count if critical_count else 1.0

    return {
        "coverageFeasible": coverage_feasible,
        "weightedCoverageFraction": round(weighted_fraction, 9),
        "worstScenarioCoverageFraction": round(worst_fraction, 9),
        "weightedProgressFraction": round(progress_fraction, 9),
        "worstScenarioProgressFraction": round(worst_progress, 9),
        "weightedComponentProgressFraction": round(component_progress_fraction, 9),
        "worstScenarioComponentProgressFraction": round(worst_component_progress, 9),
        "criticalCoverageFraction": round(critical_fraction, 9),
        "coveredSampleScenarioCount": covered_count,
        "sampleScenarioCount": pair_count,
        "coveredCriticalSampleScenarioCount": covered_critical_count,
        "criticalSampleScenarioCount": critical_count,
        "minimumContributingSiteCountAcrossCoveredPairs": minimum_contributing_sites or 0,
        "minimumContributingFailureDomainCountAcrossCoveredPairs": minimum_contributing_domains or 0,
        "selectedPhysicalSiteCount": len(selected_sites),
        "selectedFailureDomainCount": len(
            {item.failure_domain_id for item in selected_options}
        ),
        "coverageByScenario": scenario_metrics,
        "uncoveredSampleScenarios": sorted(uncovered_pairs),
        "uncoveredCriticalSampleScenarios": sorted(uncovered_critical),
        "policy": {
            "minimumFamilyCount": required_families,
            "minimumSiteRedundancy": required_sites,
            "minimumFailureDomainRedundancy": required_domains,
            "minimumCoverageFraction": required_fraction,
            "coFamilyModalitiesCountOnce": True,
        },
    }
