from __future__ import annotations

from dataclasses import replace
import json
import math
from pathlib import Path

import pytest

from singapore_sensor_fusion.placement import (
    EXACT_ALGORITHM,
    AreaOfInterest,
    CandidateOption,
    CoverageEvaluation,
    CoverageSample,
    GeoPoint,
    PlacementConstraints,
    PlacementRecommendation,
    PlacementRequest,
    PlacementScenario,
    PlacementSurvey,
    SamplingPlan,
    SensorPackage,
    active_families,
    build_coverage_evaluation,
    build_unreal_sensor_nodes_patch,
    evaluate_selection,
    generate_circular_samples,
    horizontal_distance_meters,
    render_markdown,
    solve_placement,
    stable_digest,
)
from singapore_sensor_fusion.placement.unreal_config import write_new_json_file
from singapore_sensor_fusion.placement.recommend import (
    _preflight_output_paths,
    main as placement_main,
)


def _point(*, latitude: float = 1.307, longitude: float = 103.843, height: float = 20.0) -> GeoPoint:
    return GeoPoint(
        latitude_degrees=latitude,
        longitude_degrees=longitude,
        height_meters=height,
    )


def _sample(sample_id: str = "sample-0", *, critical: bool = False) -> CoverageSample:
    return CoverageSample(
        sample_id=sample_id,
        location=_point(height=60.0),
        east_meters=0.0,
        north_meters=0.0,
        altitude_agl_meters=40.0,
        critical=critical,
    )


def _package(
    package_id: str,
    modality: str,
    family: str,
    *,
    cost: float = 0.0,
    unreal_node_template: dict[str, object] | None = None,
) -> SensorPackage:
    return SensorPackage(
        package_id=package_id,
        modalities=(modality,),
        families=(family,),
        cost_units=cost,
        unreal_node_template=unreal_node_template or {},
    )


def _request(
    packages: tuple[SensorPackage, ...],
    *,
    constraints: PlacementConstraints | None = None,
) -> PlacementRequest:
    return PlacementRequest(
        request_id="fixture",
        aoi=AreaOfInterest(
            aoi_id="fixture-aoi",
            center=_point(height=0.0),
            radius_meters=1000.0,
            center_provenance="test fixture",
        ),
        sampling=SamplingPlan(
            horizontal_spacing_meters=100.0,
            target_altitude_bands_agl_meters=(40.0,),
        ),
        sensor_packages=packages,
        scenarios=(PlacementScenario(scenario_id="clear-rf"),),
        constraints=constraints
        or PlacementConstraints(
            maximum_sites=2,
            minimum_family_count=2,
            minimum_site_redundancy=2,
            minimum_failure_domain_redundancy=2,
            minimum_coverage_fraction=1.0,
        ),
    )


def _option(
    option_id: str,
    site_id: str,
    package_id: str,
    failure_domain_id: str,
    *,
    cost: float,
    longitude: float = 103.843,
    overrides: dict[str, object] | None = None,
) -> CandidateOption:
    return CandidateOption(
        option_id=option_id,
        site_id=site_id,
        package_id=package_id,
        orientation_id="default",
        location=_point(longitude=longitude),
        failure_domain_id=failure_domain_id,
        site_cost_units=cost,
        camera_pitch_degrees=-5.0,
        camera_yaw_degrees=45.0,
        unreal_node_overrides=overrides or {},
    )


def _evaluation(option_id: str, modality: str, *, sample_id: str = "sample-0"):
    return build_coverage_evaluation(
        option_id=option_id,
        sample_id=sample_id,
        scenario_id="clear-rf",
        modality=modality,
        quality=0.9,
        range_meters=100.0,
        maximum_range_meters=1000.0,
    )


def _exact_fixture(
    *,
    first_template: dict[str, object] | None = None,
    first_overrides: dict[str, object] | None = None,
) -> tuple[PlacementRequest, PlacementSurvey]:
    request = _request(
        (
            _package(
                "rf",
                "wideband_rf",
                "passive_rf",
                unreal_node_template=first_template,
            ),
            _package("radar", "SEARCH_RADAR", "active_radar"),
        )
    )
    options = (
        _option(
            "option-a",
            "site-a",
            "rf",
            "power-a",
            cost=1.0,
            overrides=first_overrides,
        ),
        _option("option-b", "site-b", "radar", "power-b", cost=2.0, longitude=103.844),
        _option("option-c", "site-c", "radar", "power-c", cost=5.0, longitude=103.845),
    )
    survey = PlacementSurvey(
        request_id=request.request_id,
        options=options,
        samples=(_sample(),),
        evaluations=(
            _evaluation("option-a", "wideband_rf"),
            _evaluation("option-b", "SEARCH_RADAR"),
            _evaluation("option-c", "SEARCH_RADAR"),
        ),
        world={"map": "test"},
    )
    return request, survey


def test_circular_aoi_samples_never_exceed_declared_radius() -> None:
    aoi = AreaOfInterest(
        aoi_id="istana-test",
        center=_point(height=10.0),
        radius_meters=1000.0,
        center_provenance="test fixture",
    )
    plan = SamplingPlan(
        horizontal_spacing_meters=500.0,
        target_altitude_bands_agl_meters=(30.0, 60.0),
    )

    samples = generate_circular_samples(aoi, plan)

    assert samples
    assert any(sample.east_meters == 0.0 and sample.north_meters == 0.0 for sample in samples)
    assert max(horizontal_distance_meters(sample) for sample in samples) <= 1000.0 + 1e-9
    assert {sample.altitude_agl_meters for sample in samples} == {30.0, 60.0}
    for sample in samples:
        assert math.hypot(sample.east_meters, sample.north_meters) <= aoi.radius_meters


def test_range_fov_los_and_cofamily_modalities_are_fail_closed() -> None:
    wrapped_heading = build_coverage_evaluation(
        option_id="visual-option",
        sample_id="sample-0",
        scenario_id="clear-rf",
        modality="rgb",
        quality=0.9,
        range_meters=100.0,
        maximum_range_meters=200.0,
        bearing_degrees=355.0,
        boresight_degrees=5.0,
        horizontal_fov_degrees=30.0,
        line_of_sight=True,
    )
    out_of_range = build_coverage_evaluation(
        option_id="visual-option",
        sample_id="sample-0",
        scenario_id="clear-rf",
        modality="EO_PTZ",
        quality=0.9,
        range_meters=201.0,
        maximum_range_meters=200.0,
    )
    occluded = build_coverage_evaluation(
        option_id="visual-option",
        sample_id="sample-0",
        scenario_id="clear-rf",
        modality="THERMAL_PTZ",
        quality=0.9,
        range_meters=100.0,
        maximum_range_meters=200.0,
        line_of_sight=False,
    )

    assert wrapped_heading.eligible is True
    assert out_of_range.within_range is False
    assert out_of_range.eligible is False
    assert occluded.line_of_sight_required is True
    assert occluded.eligible is False
    assert active_families((wrapped_heading, out_of_range, occluded)) == ("visual",)

    request = _request(
        (_package("visual", "rgb", "visual"),),
        constraints=PlacementConstraints(
            maximum_sites=1,
            minimum_family_count=2,
            minimum_site_redundancy=1,
            minimum_failure_domain_redundancy=1,
        ),
    )
    option = _option("visual-option", "site-v", "visual", "power-v", cost=1.0)
    survey = PlacementSurvey(
        request_id=request.request_id,
        options=(option,),
        samples=(_sample(),),
        evaluations=(wrapped_heading,),
    )
    metrics = evaluate_selection(request, survey, (option.option_id,))
    assert metrics["coverageFeasible"] is False
    assert metrics["policy"]["coFamilyModalitiesCountOnce"] is True

    with pytest.raises(ValueError, match="within_range must agree"):
        CoverageEvaluation(
            option_id="imported",
            sample_id="sample-0",
            scenario_id="clear-rf",
            modality="wideband_rf",
            family="passive_rf",
            quality=0.9,
            eligible=True,
            range_meters=2000.0,
            maximum_range_meters=1000.0,
            within_range=True,
        )


def test_redundancy_requires_distinct_physical_sites_and_failure_domains() -> None:
    request, survey = _exact_fixture()
    same_domain_options = (
        survey.options[0],
        replace(survey.options[1], failure_domain_id="power-a"),
    )
    same_domain_survey = PlacementSurvey(
        request_id=survey.request_id,
        options=same_domain_options,
        samples=survey.samples,
        evaluations=survey.evaluations[:2],
    )

    shared_metrics = evaluate_selection(
        request, same_domain_survey, ("option-a", "option-b")
    )
    independent_metrics = evaluate_selection(request, survey, ("option-a", "option-b"))

    assert shared_metrics["coverageFeasible"] is False
    assert shared_metrics["selectedPhysicalSiteCount"] == 2
    assert shared_metrics["selectedFailureDomainCount"] == 1
    assert independent_metrics["coverageFeasible"] is True
    assert independent_metrics["minimumContributingFailureDomainCountAcrossCoveredPairs"] == 2


def test_exact_small_solver_returns_minimum_cost_feasible_layout() -> None:
    request, survey = _exact_fixture()

    result = solve_placement(request, survey)

    assert result.status == "FEASIBLE"
    assert result.algorithm == EXACT_ALGORITHM
    assert result.selected_option_ids == ("option-a", "option-b")
    assert result.total_cost_units == 3.0
    assert result.metrics["worstScenarioCoverageFraction"] == 1.0


def test_solver_reports_infeasibility_without_weakening_family_policy() -> None:
    request = _request(
        (_package("rf", "wideband_rf", "passive_rf"),),
        constraints=PlacementConstraints(
            maximum_sites=1,
            minimum_family_count=2,
            minimum_site_redundancy=1,
            minimum_failure_domain_redundancy=1,
            minimum_coverage_fraction=1.0,
        ),
    )
    option = _option("only-rf", "site-rf", "rf", "power-rf", cost=1.0)
    survey = PlacementSurvey(
        request_id=request.request_id,
        options=(option,),
        samples=(_sample(critical=True),),
        evaluations=(_evaluation("only-rf", "wideband_rf"),),
    )

    result = solve_placement(request, survey)

    assert result.status == "INFEASIBLE"
    assert result.metrics["coverageFeasible"] is False
    assert result.infeasibility_reasons
    assert "critical" in " ".join(result.infeasibility_reasons).lower()
    assert "INFEASIBLE" in render_markdown(request, result)


def test_rf_silent_scenario_rejects_eligible_passive_rf_evidence() -> None:
    request = _request(
        (_package("rf", "wideband_rf", "passive_rf"),),
        constraints=PlacementConstraints(
            maximum_sites=1,
            minimum_family_count=1,
            minimum_site_redundancy=1,
            minimum_failure_domain_redundancy=1,
        ),
    )
    request = replace(
        request,
        scenarios=(PlacementScenario(scenario_id="clear-rf", rf_emitting=False),),
    )
    option = _option("only-rf", "site-rf", "rf", "power-rf", cost=1.0)
    survey = PlacementSurvey(
        request_id=request.request_id,
        options=(option,),
        samples=(_sample(),),
        evaluations=(_evaluation("only-rf", "wideband_rf"),),
    )

    with pytest.raises(ValueError, match="RF-silent scenario"):
        solve_placement(request, survey)


def test_recommendation_and_digests_are_deterministic() -> None:
    request, survey = _exact_fixture()

    first = solve_placement(request, survey)
    second = solve_placement(request, survey)

    assert first.input_digest == second.input_digest
    assert first.recommendation_digest == second.recommendation_digest
    assert first.to_dict() == second.to_dict()
    assert len(first.recommendation_digest) == 64
    with pytest.raises(TypeError):
        first.selected_options[0]["optionId"] = "mutated"
    with pytest.raises(TypeError):
        first.metrics["coverageFeasible"] = False


def test_greedy_dead_end_is_unresolved_not_false_infeasibility() -> None:
    request = _request(
        (
            _package("rf", "wideband_rf", "passive_rf"),
            _package("radar", "SEARCH_RADAR", "active_radar"),
        ),
        constraints=PlacementConstraints(
            maximum_sites=2,
            minimum_family_count=2,
            minimum_site_redundancy=2,
            minimum_failure_domain_redundancy=2,
        ),
    )
    options = (
        _option("option-a", "site-a", "rf", "domain-a", cost=1.0),
        _option("option-b", "site-b", "radar", "domain-b", cost=2.0),
        _option("option-c", "site-c", "radar", "domain-a", cost=0.1),
    )
    survey = PlacementSurvey(
        request_id=request.request_id,
        options=options,
        samples=(_sample(),),
        evaluations=(
            _evaluation("option-a", "wideband_rf"),
            _evaluation("option-b", "SEARCH_RADAR"),
            _evaluation("option-c", "SEARCH_RADAR"),
        ),
    )

    exact = solve_placement(request, survey)
    greedy = solve_placement(request, survey, exact_option_limit=0)

    assert exact.status == "FEASIBLE"
    assert exact.selected_option_ids == ("option-a", "option-b")
    assert greedy.status == "UNRESOLVED"
    assert "not proof" in " ".join(greedy.infeasibility_reasons)
    assert "Why no feasible layout was returned" in render_markdown(request, greedy)


def test_unreal_patch_forces_survey_identity_and_refuses_overwrite(tmp_path: Path) -> None:
    request, survey = _exact_fixture(
        first_template={
            "NodeId": "template-can-not-win",
            "LongitudeDegrees": 0.0,
            "LatitudeDegrees": 0.0,
            "HeightMeters": -1.0,
            "bEnabled": False,
        },
        first_overrides={
            "NodeId": "override-can-not-win",
            "LongitudeDegrees": 1.0,
            "bEnabled": False,
            "bCaptureCameraFrames": True,
        },
    )
    request_before = json.dumps(request.to_dict(), sort_keys=True)
    survey_before = json.dumps(survey.to_dict(), sort_keys=True)
    recommendation = solve_placement(request, survey)

    patch = build_unreal_sensor_nodes_patch(request, survey, recommendation)

    assert patch["applySemantics"].startswith("review_required_separate_patch")
    assert patch["siteAuthorizationInferred"] is False
    first_node = next(node for node in patch["SensorNodes"] if node["NodeId"] == "PLACEMENT_site_a")
    assert first_node["LongitudeDegrees"] == survey.options[0].location.longitude_degrees
    assert first_node["LatitudeDegrees"] == survey.options[0].location.latitude_degrees
    assert first_node["HeightMeters"] == survey.options[0].location.height_meters
    assert first_node["bEnabled"] is True
    assert first_node["CameraRelativeRotation"] == {
        "Pitch": -5.0,
        "Yaw": 45.0,
        "Roll": 0.0,
    }
    assert json.dumps(request.to_dict(), sort_keys=True) == request_before
    assert json.dumps(survey.to_dict(), sort_keys=True) == survey_before

    output = tmp_path / "review-only-patch.json"
    write_new_json_file(patch, output)
    written = output.read_text(encoding="utf-8")
    with pytest.raises(FileExistsError, match="refusing to overwrite"):
        write_new_json_file({"replacement": True}, output)
    assert output.read_text(encoding="utf-8") == written


def test_unreal_patch_rejects_unsafe_fields_and_mismatched_provenance() -> None:
    request, survey = _exact_fixture()
    recommendation = solve_placement(request, survey)
    unsafe_request = replace(
        request,
        sensor_packages=(
            replace(request.sensor_packages[0], unreal_node_template={"ExecCommand": "quit"}),
            request.sensor_packages[1],
        ),
    )

    with pytest.raises(ValueError, match="input digest"):
        build_unreal_sensor_nodes_patch(unsafe_request, survey, recommendation)

    unsafe_recommendation = solve_placement(unsafe_request, survey)
    with pytest.raises(ValueError, match="unsupported SensorNode fields"):
        build_unreal_sensor_nodes_patch(unsafe_request, survey, unsafe_recommendation)

    wrong_type_request = replace(
        request,
        sensor_packages=(
            replace(
                request.sensor_packages[0],
                unreal_node_template={"bEnableSearchRadar": "false"},
            ),
            request.sensor_packages[1],
        ),
    )
    wrong_type_recommendation = solve_placement(wrong_type_request, survey)
    with pytest.raises(TypeError, match="must be boolean"):
        build_unreal_sensor_nodes_patch(
            wrong_type_request, survey, wrong_type_recommendation
        )


def test_unreal_patch_cost_check_uses_absolute_not_relative_tolerance() -> None:
    request, survey = _exact_fixture()
    expensive_survey = replace(
        survey,
        options=(
            replace(survey.options[0], site_cost_units=1_000_000_000_000.0),
            *survey.options[1:],
        ),
    )
    recommendation = solve_placement(request, expensive_survey)
    wrong_total = recommendation.total_cost_units + 100.0
    digest_payload = recommendation.digest_payload()
    digest_payload["totalCostUnits"] = wrong_total
    forged = PlacementRecommendation(
        request_id=recommendation.request_id,
        status=recommendation.status,
        algorithm=recommendation.algorithm,
        selected_options=recommendation.to_dict()["selectedOptions"],
        total_cost_units=wrong_total,
        metrics=recommendation.to_dict()["metrics"],
        infeasibility_reasons=recommendation.infeasibility_reasons,
        input_digest=recommendation.input_digest,
        recommendation_digest=stable_digest(digest_payload),
    )

    with pytest.raises(ValueError, match="total cost"):
        build_unreal_sensor_nodes_patch(request, expensive_survey, forged)


def test_cli_output_preflight_rejects_aliases_and_existing_files(tmp_path: Path) -> None:
    shared = tmp_path / "shared.json"
    with pytest.raises(ValueError, match="distinct paths"):
        _preflight_output_paths((shared, shared))

    existing = tmp_path / "existing.md"
    existing.write_text("keep me", encoding="utf-8")
    with pytest.raises(FileExistsError, match="refusing to overwrite"):
        _preflight_output_paths((shared, existing))
    assert not shared.exists()
    assert existing.read_text(encoding="utf-8") == "keep me"


def test_installed_workflow_generates_review_patch_and_plain_language_report(
    tmp_path: Path,
) -> None:
    request, survey = _exact_fixture()
    request_path = tmp_path / "request.json"
    survey_path = tmp_path / "survey.json"
    output_path = tmp_path / "recommendation.json"
    request_path.write_text(json.dumps(request.to_dict()), encoding="utf-8")
    survey_path.write_text(json.dumps(survey.to_dict()), encoding="utf-8")

    exit_code = placement_main(
        [str(request_path), str(survey_path), "--output", str(output_path)]
    )

    report_path = tmp_path / "recommendation.md"
    patch_path = tmp_path / "recommendation_unreal_sensor_nodes_patch.json"
    assert exit_code == 0
    assert json.loads(output_path.read_text(encoding="utf-8"))["status"] == "FEASIBLE"
    report = report_path.read_text(encoding="utf-8")
    assert "planning evidence only" in report
    assert "does not establish physical sensor performance" in report
    patch = json.loads(patch_path.read_text(encoding="utf-8"))
    assert patch["applySemantics"].startswith("review_required_separate_patch")
    assert patch["siteAuthorizationInferred"] is False


def test_istana_example_contract_uses_interoperable_ids_and_provenance() -> None:
    example_path = Path(__file__).parents[1] / "examples" / "istana_1km_placement_request.json"

    request = PlacementRequest.load(example_path)

    assert request.request_id == "istana-1km-mvp"
    assert request.aoi.radius_meters == 1000.0
    assert request.aoi.center.latitude_degrees == pytest.approx(1.30709615)
    assert request.aoi.center.longitude_degrees == pytest.approx(103.84288055)
    assert "41895536" in request.aoi.center_provenance
    assert [item.package_id for item in request.sensor_packages] == ["full-stack"]
    assert [item.scenario_id for item in request.scenarios] == [
        "clear-rf",
        "monsoon-rf",
        "haze-rf",
        "clear-rf-silent",
    ]
    assert request.aoi.candidate_buffer_meters == 0.0
    assert request.sensor_packages[0].families == (
        "active_radar",
        "passive_rf",
        "visual",
    )
    template = request.sensor_packages[0].unreal_node_template
    assert template["bTrackNearestTarget"] is False
    assert template["SearchRadarRangeMeters"] == 5000.0
