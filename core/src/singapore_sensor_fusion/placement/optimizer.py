"""Deterministic exact-small and greedy sensor-site selection."""

from __future__ import annotations

from itertools import combinations
from typing import Iterable, Mapping

from .contracts import (
    CandidateOption,
    PlacementRecommendation,
    PlacementRequest,
    PlacementSurvey,
    stable_digest,
)
from .coverage import build_eligible_evaluation_index, evaluate_selection


EXACT_ALGORITHM = "deterministic_exact_site_enumeration_v1"
GREEDY_ALGORITHM = "deterministic_greedy_site_selection_v1"


def _package_map(request: PlacementRequest) -> dict[str, object]:
    return {item.package_id: item for item in request.sensor_packages}


def _validate_problem(request: PlacementRequest, survey: PlacementSurvey) -> None:
    if request.request_id != survey.request_id:
        raise ValueError("request and survey request IDs do not match")
    packages = _package_map(request)
    scenarios = {item.scenario_id: item for item in request.scenarios}
    scenario_ids = set(scenarios)
    sample_ids = {item.sample_id for item in survey.samples}
    for sample_id in request.constraints.critical_sample_ids:
        if sample_id not in sample_ids:
            raise ValueError(f"critical sample {sample_id!r} is not present in the survey")
    option_by_id = {item.option_id: item for item in survey.options}
    for option in survey.options:
        if option.package_id not in packages:
            raise ValueError(
                f"option {option.option_id!r} references unknown package {option.package_id!r}"
            )
    for evaluation in survey.evaluations:
        if evaluation.scenario_id not in scenario_ids:
            raise ValueError(
                f"evaluation references unknown scenario {evaluation.scenario_id!r}"
            )
        if (
            not scenarios[evaluation.scenario_id].rf_emitting
            and evaluation.family == "passive_rf"
            and evaluation.eligible
        ):
            raise ValueError(
                f"eligible passive-RF evaluation {evaluation.option_id!r}/"
                f"{evaluation.sample_id!r} contradicts RF-silent scenario "
                f"{evaluation.scenario_id!r}"
            )
        option = option_by_id[evaluation.option_id]
        package = packages[option.package_id]
        if evaluation.modality not in package.modalities:
            raise ValueError(
                f"evaluation modality {evaluation.modality!r} is not in package "
                f"{package.package_id!r}"
            )


def option_installed_cost(request: PlacementRequest, option: CandidateOption) -> float:
    packages = {item.package_id: item for item in request.sensor_packages}
    package = packages.get(option.package_id)
    if package is None:
        raise ValueError(f"unknown package {option.package_id!r}")
    return option.site_cost_units + package.cost_units


def selection_cost(
    request: PlacementRequest,
    options_by_id: Mapping[str, CandidateOption],
    selected_ids: Iterable[str],
) -> float:
    return sum(option_installed_cost(request, options_by_id[item]) for item in selected_ids)


def _selection_allowed(
    request: PlacementRequest,
    options_by_id: Mapping[str, CandidateOption],
    selected_ids: tuple[str, ...],
) -> bool:
    if len(selected_ids) > request.constraints.maximum_sites:
        return False
    options = [options_by_id[item] for item in selected_ids]
    if len({item.site_id for item in options}) != len(options):
        return False
    if any(not item.feasible for item in options):
        return False
    budget = request.constraints.budget_units
    return budget is None or selection_cost(request, options_by_id, selected_ids) <= budget + 1e-9


def _objective_key(
    *, metrics: Mapping[str, object], cost: float, count: int
) -> tuple[float, ...]:
    feasible = metrics["coverageFeasible"] is True
    if feasible:
        # Once the declared coverage contract is satisfied, solve minimum-cost
        # set cover. Extra coverage and deterministic IDs only break ties.
        return (
            1.0,
            -round(cost, 9),
            -float(count),
            float(metrics["worstScenarioCoverageFraction"]),
            float(metrics["weightedCoverageFraction"]),
            float(metrics["minimumContributingFailureDomainCountAcrossCoveredPairs"]),
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
        -float(count),
    )


def _is_better(
    candidate_key: tuple[float, ...],
    candidate_ids: tuple[str, ...],
    incumbent_key: tuple[float, ...] | None,
    incumbent_ids: tuple[str, ...] | None,
) -> bool:
    if incumbent_key is None or candidate_key > incumbent_key:
        return True
    return candidate_key == incumbent_key and (
        incumbent_ids is None or candidate_ids < incumbent_ids
    )


def _exact_selection(
    request: PlacementRequest,
    survey: PlacementSurvey,
    available: tuple[CandidateOption, ...],
    eligible_index: Mapping[str, tuple[object, ...]],
) -> tuple[tuple[str, ...], dict[str, object]]:
    options_by_id = {item.option_id: item for item in survey.options}
    best_ids: tuple[str, ...] | None = None
    best_metrics: dict[str, object] | None = None
    best_key: tuple[float, ...] | None = None
    maximum = min(request.constraints.maximum_sites, len(available))
    ordered_ids = tuple(item.option_id for item in available)
    for count in range(maximum + 1):
        for raw_ids in combinations(ordered_ids, count):
            selected_ids = tuple(sorted(raw_ids))
            if not _selection_allowed(request, options_by_id, selected_ids):
                continue
            metrics = evaluate_selection(
                request,
                survey,
                selected_ids,
                eligible_evaluations_by_option=eligible_index,
            )
            cost = selection_cost(request, options_by_id, selected_ids)
            key = _objective_key(metrics=metrics, cost=cost, count=count)
            if _is_better(key, selected_ids, best_key, best_ids):
                best_ids, best_metrics, best_key = selected_ids, metrics, key
    assert best_ids is not None and best_metrics is not None
    return best_ids, best_metrics


def _greedy_selection(
    request: PlacementRequest,
    survey: PlacementSurvey,
    available: tuple[CandidateOption, ...],
    eligible_index: Mapping[str, tuple[object, ...]],
) -> tuple[tuple[str, ...], dict[str, object]]:
    options_by_id = {item.option_id: item for item in survey.options}
    selected: tuple[str, ...] = ()
    metrics = evaluate_selection(
        request,
        survey,
        selected,
        eligible_evaluations_by_option=eligible_index,
    )
    while len(selected) < request.constraints.maximum_sites and not metrics["coverageFeasible"]:
        incumbent_key = _objective_key(
            metrics=metrics,
            cost=selection_cost(request, options_by_id, selected),
            count=len(selected),
        )
        best_ids: tuple[str, ...] | None = None
        best_metrics: dict[str, object] | None = None
        best_key: tuple[float, ...] | None = None
        selected_sites = {options_by_id[item].site_id for item in selected}
        for option in available:
            if option.option_id in selected or option.site_id in selected_sites:
                continue
            candidate_ids = tuple(sorted((*selected, option.option_id)))
            if not _selection_allowed(request, options_by_id, candidate_ids):
                continue
            candidate_metrics = evaluate_selection(
                request,
                survey,
                candidate_ids,
                eligible_evaluations_by_option=eligible_index,
            )
            candidate_key = _objective_key(
                metrics=candidate_metrics,
                cost=selection_cost(request, options_by_id, candidate_ids),
                count=len(candidate_ids),
            )
            if _is_better(candidate_key, candidate_ids, best_key, best_ids):
                best_ids, best_metrics, best_key = candidate_ids, candidate_metrics, candidate_key
        if best_ids is None or best_key is None or best_key <= incumbent_key:
            break
        selected, metrics = best_ids, best_metrics or metrics

    # Deterministically remove any now-redundant expensive selections.
    changed = True
    while metrics["coverageFeasible"] and changed:
        changed = False
        removal_order = sorted(
            selected,
            key=lambda item: (option_installed_cost(request, options_by_id[item]), item),
            reverse=True,
        )
        for option_id in removal_order:
            candidate_ids = tuple(item for item in selected if item != option_id)
            candidate_metrics = evaluate_selection(
                request,
                survey,
                candidate_ids,
                eligible_evaluations_by_option=eligible_index,
            )
            if candidate_metrics["coverageFeasible"]:
                selected, metrics, changed = candidate_ids, candidate_metrics, True
                break
    return selected, metrics


def _infeasibility_reasons(
    request: PlacementRequest,
    metrics: Mapping[str, object],
    available: tuple[CandidateOption, ...],
) -> tuple[str, ...]:
    reasons: list[str] = []
    if not available:
        reasons.append("No feasible candidate options were supplied by the placement survey.")
    uncovered_critical = list(metrics["uncoveredCriticalSampleScenarios"])
    if uncovered_critical:
        reasons.append(
            f"{len(uncovered_critical)} critical sample/scenario pairs do not satisfy the family and redundancy policy."
        )
    worst = float(metrics["worstScenarioCoverageFraction"])
    required = request.constraints.minimum_coverage_fraction
    if worst + 1e-12 < required:
        reasons.append(
            f"Worst-scenario coverage is {worst:.6f}, below the required {required:.6f}."
        )
    if request.constraints.minimum_failure_domain_redundancy > 1:
        reasons.append(
            "The declared distinct failure-domain redundancy may be unattainable with the supplied candidates."
        )
    if request.constraints.budget_units is not None:
        reasons.append(
            "The budget and maximum-site constraints may exclude otherwise covering layouts."
        )
    if not reasons:
        reasons.append("No layout satisfies all declared placement constraints.")
    return tuple(reasons)


def solve_placement(
    request: PlacementRequest,
    survey: PlacementSurvey,
    *,
    exact_option_limit: int = 8,
) -> PlacementRecommendation:
    """Solve one deterministic placement problem.

    Exact enumeration is used for small surveyed option sets. Larger sets use
    a deterministic family-progress greedy method with removal pruning and no
    optional solver dependency.
    """

    if isinstance(exact_option_limit, bool) or not isinstance(exact_option_limit, int) or exact_option_limit < 0:
        raise ValueError("exact_option_limit must be an integer >= 0")
    _validate_problem(request, survey)
    available = tuple(sorted((item for item in survey.options if item.feasible), key=lambda item: item.option_id))
    eligible_index = build_eligible_evaluation_index(survey)
    if len(available) <= exact_option_limit:
        selected_ids, metrics = _exact_selection(
            request, survey, available, eligible_index
        )
        algorithm = EXACT_ALGORITHM
    else:
        selected_ids, metrics = _greedy_selection(
            request, survey, available, eligible_index
        )
        algorithm = GREEDY_ALGORITHM

    option_by_id = {item.option_id: item for item in survey.options}
    total_cost = selection_cost(request, option_by_id, selected_ids)
    packages = {item.package_id: item for item in request.sensor_packages}
    selected_rows = tuple(
        {
            "optionId": option.option_id,
            "siteId": option.site_id,
            "packageId": option.package_id,
            "orientationId": option.orientation_id,
            "failureDomainId": option.failure_domain_id,
            "installedCostUnits": round(
                option.site_cost_units + packages[option.package_id].cost_units, 9
            ),
            "location": option.location.to_dict(),
        }
        for option in (option_by_id[item] for item in selected_ids)
    )
    feasible = metrics["coverageFeasible"] is True
    if feasible:
        status = "FEASIBLE"
        reasons: tuple[str, ...] = ()
    elif algorithm == EXACT_ALGORITHM:
        status = "INFEASIBLE"
        reasons = _infeasibility_reasons(request, metrics, available)
    else:
        status = "UNRESOLVED"
        reasons = (
            "The deterministic greedy search did not find a feasible layout; this is not proof that none exists.",
            *_infeasibility_reasons(request, metrics, available),
        )
    input_digest = stable_digest({"request": request.to_dict(), "survey": survey.to_dict()})
    digest_payload = {
        "schemaVersion": "triad.placement_recommendation.v1",
        "requestId": request.request_id,
        "status": status,
        "algorithm": algorithm,
        "selectedOptions": list(selected_rows),
        "totalCostUnits": round(total_cost, 9),
        "metrics": metrics,
        "infeasibilityReasons": list(reasons),
        "inputDigest": input_digest,
    }
    recommendation_digest = stable_digest(digest_payload)
    return PlacementRecommendation(
        request_id=request.request_id,
        status=status,
        algorithm=algorithm,
        selected_options=selected_rows,
        total_cost_units=round(total_cost, 9),
        metrics=metrics,
        infeasibility_reasons=reasons,
        input_digest=input_digest,
        recommendation_digest=recommendation_digest,
    )
