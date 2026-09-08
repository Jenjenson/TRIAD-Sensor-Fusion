from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
import shutil

import pytest

import singapore_sensor_fusion.placement.planning_pipeline as planning_pipeline_module

from singapore_sensor_fusion.placement.planning_pipeline import (
    BRANCH_AND_BOUND_ALGORITHM,
    CANDIDATE_TRAJECTORY_PRECOMPUTE_SCHEMA,
    DEMO_SURROGATE_SOURCE,
    GREEDY_LOCAL_ALGORITHM,
    MEASURED_CALIBRATION_EVIDENCE,
    SYNTHETIC_DEMO_EVIDENCE,
    CandidateSensorPose,
    PoseBounds,
    RobustDemand,
    RobustStaticProblem,
    SensorModelCalibration,
    StaticCandidate,
    TargetTrajectory,
    TrajectoryPoint,
    assess_stage0_readiness,
    build_calibration_bundle,
    compare_pose_refiners,
    default_synthetic_demo_calibrations,
    load_and_validate_rf_geometry,
    precompute_demo_candidate_trajectory_results,
    solve_greedy_local_search,
    solve_robust_branch_and_bound,
    static_problem_from_precompute,
)
from singapore_sensor_fusion.placement.demo_study import (
    DEMO_STUDY_SCHEMA,
    build_four_class_demo_study,
)
from singapore_sensor_fusion.placement.study_contract import Stage0StudyContract


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
RF_RESOURCE_ROOT = (
    REPOSITORY_ROOT
    / "unreal"
    / "Plugins"
    / "TRIADSensorFusion"
    / "Resources"
    / "RF"
)


def _load_full_rf_receipt():
    return load_and_validate_rf_geometry(
        RF_RESOURCE_ROOT / "IstanaPublicViewRFOneKilometreV2.geometry.json",
        RF_RESOURCE_ROOT / "istana_rf_materials_one_kilometre_v2.catalog.json",
        RF_RESOURCE_ROOT / "istana_rf_scene_one_kilometre_v2.contract.json",
    )


def test_actual_full_one_kilometre_rf_files_load_and_match_pinned_contract() -> None:
    receipt = _load_full_rf_receipt()
    payload = receipt.to_dict()

    assert receipt.geometry_sha256 == (
        "85e654fba602b2dbc51eb64c6b66ff234c8ea1f948152612c755edc22c65da51"
    )
    assert receipt.material_catalog_sha256 == (
        "210cb26ddb9b531adeb3c917606a736aefc857eb6696da485a2e63dbb8b31662"
    )
    assert receipt.scene_contract_sha256 == (
        "fe509917ae59be0918bcd799f23dc981e00a394c6c328a7342f56b371c40bcc2"
    )
    assert receipt.revision == "OneKilometreV2-R24C"
    assert (
        receipt.solid_count,
        receipt.surface_count,
        receipt.vertex_count,
        receipt.triangle_count,
        receipt.partition_cell_count,
        receipt.positive_volume_overlap_pair_count,
    ) == (1094, 13936, 23496, 42700, 721, 0)
    assert receipt.covers_one_kilometre_aoi is True
    assert receipt.field_validated is False
    assert receipt.survey_controlled is False
    assert receipt.survey_truth_ready is False
    assert receipt.material_calibration_states == ("UNCALIBRATED_ASSUMPTION",)
    assert payload["authority"] == "SIMULATION_READY_ASSUMPTION_BOUND"
    assert payload["physicalDeploymentAuthorized"] is False
    assert len(payload["receiptSha256"]) == 64


def test_rf_loader_parses_the_same_stable_bytes_that_it_hashes(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    geometry_path = tmp_path / "IstanaPublicViewRFOneKilometreV2.geometry.json"
    catalog_path = tmp_path / "istana_rf_materials_one_kilometre_v2.catalog.json"
    scene_path = tmp_path / "istana_rf_scene_one_kilometre_v2.contract.json"
    for source, target in (
        (RF_RESOURCE_ROOT / geometry_path.name, geometry_path),
        (RF_RESOURCE_ROOT / catalog_path.name, catalog_path),
        (RF_RESOURCE_ROOT / scene_path.name, scene_path),
    ):
        shutil.copyfile(source, target)
    expected_geometry_sha = hashlib.sha256(geometry_path.read_bytes()).hexdigest()
    original_decoder = planning_pipeline_module._load_json_bytes_without_duplicate_keys
    changed = False

    def replace_after_snapshot(name: str, data: bytes):
        nonlocal changed
        if name == geometry_path.name and not changed:
            geometry_path.write_text('{"tamperedAfterSnapshot":true}', encoding="utf-8")
            changed = True
        return original_decoder(name, data)

    monkeypatch.setattr(
        planning_pipeline_module,
        "_load_json_bytes_without_duplicate_keys",
        replace_after_snapshot,
    )

    receipt = load_and_validate_rf_geometry(
        geometry_path,
        catalog_path,
        scene_path,
    )

    assert changed is True
    assert receipt.geometry_sha256 == expected_geometry_sha


def test_actual_file_rf_loader_rejects_non_numeric_vertex_coordinates(
    tmp_path: Path,
) -> None:
    geometry_path = RF_RESOURCE_ROOT / "IstanaPublicViewRFOneKilometreV2.geometry.json"
    geometry = json.loads(geometry_path.read_text(encoding="utf-8"))
    geometry["verticesMeters"][0][0] = "not-a-number"
    malformed_path = tmp_path / "malformed.geometry.json"
    malformed_path.write_text(
        json.dumps(geometry, separators=(",", ":")),
        encoding="utf-8",
    )

    with pytest.raises(
        TypeError,
        match=r"geometry\.verticesMeters\[0\]\.x must be numeric",
    ):
        load_and_validate_rf_geometry(
            malformed_path,
            RF_RESOURCE_ROOT / "istana_rf_materials_one_kilometre_v2.catalog.json",
            RF_RESOURCE_ROOT / "istana_rf_scene_one_kilometre_v2.contract.json",
        )


def test_actual_file_rf_loader_rejects_cross_file_asset_id_mismatch(
    tmp_path: Path,
) -> None:
    geometry_path = RF_RESOURCE_ROOT / "IstanaPublicViewRFOneKilometreV2.geometry.json"
    scene_path = RF_RESOURCE_ROOT / "istana_rf_scene_one_kilometre_v2.contract.json"
    scene = json.loads(scene_path.read_text(encoding="utf-8"))
    scene["assetId"] = "different-rf-scene"
    scene_text = json.dumps(scene, separators=(",", ":"))
    malformed_scene_path = tmp_path / scene_path.name
    malformed_scene_path.write_text(scene_text, encoding="utf-8")

    geometry = json.loads(geometry_path.read_text(encoding="utf-8"))
    geometry["contractSha256"] = hashlib.sha256(scene_text.encode("utf-8")).hexdigest()
    malformed_geometry_path = tmp_path / "malformed.geometry.json"
    malformed_geometry_path.write_text(
        json.dumps(geometry, separators=(",", ":")),
        encoding="utf-8",
    )

    with pytest.raises(
        ValueError,
        match="geometry and scene-contract asset IDs do not match",
    ):
        load_and_validate_rf_geometry(
            malformed_geometry_path,
            RF_RESOURCE_ROOT / "istana_rf_materials_one_kilometre_v2.catalog.json",
            malformed_scene_path,
        )


def test_stage0_example_is_reported_as_demo_only_without_self_approval() -> None:
    contract = Stage0StudyContract.load(
        REPOSITORY_ROOT
        / "core"
        / "examples"
        / "istana_stage0_sensor_placement_study_contract.v1.json"
    )

    receipt = assess_stage0_readiness(contract)

    assert receipt["executionClass"] == "DEMO_STUDY_ONLY"
    assert receipt["demoStudyExecutionAllowed"] is True
    assert receipt["productionOptimizationAuthorized"] is False
    assert receipt["physicalDeploymentAuthorized"] is False
    assert len(receipt["unverifiedArtifactIds"]) == 29
    assert len(receipt["blockedGateIds"]) == 10
    assert receipt["ineligibleMountRegionIds"] == ["example-region-pending-survey"]
    assert len(receipt["incompleteApprovalRoles"]) == 5
    assert "NO_ELIGIBLE_MOUNT_REGION" in receipt["blockers"]
    assert "PHYSICAL_DEPLOYMENT_NOT_AUTHORIZED" in receipt["blockers"]


def test_synthetic_calibration_and_precompute_are_complete_and_visibly_labelled() -> None:
    calibrations = default_synthetic_demo_calibrations()
    calibration_bundle = build_calibration_bundle(calibrations)
    assert calibration_bundle["allModelsMeasured"] is False
    assert (
        calibration_bundle["scoreSemantics"]
        == "SYNTHETIC_DEMO_EVIDENCE_INDEX_NOT_A_PROBABILITY"
    )
    assert all(
        item["evidenceClass"] == SYNTHETIC_DEMO_EVIDENCE
        for item in calibration_bundle["calibrations"]
    )

    candidates = (
        CandidateSensorPose("candidate-a", "site-a", "domain-a", 0, 0, 10, 0),
        CandidateSensorPose("candidate-b", "site-b", "domain-b", 250, 80, 12, 180),
    )
    trajectories = (
        TargetTrajectory(
            "trajectory-emitting",
            "clear-emitting",
            "train",
            (
                TrajectoryPoint(0, 100, 0, 40),
                TrajectoryPoint(1, 150, 0, 40),
                TrajectoryPoint(2, 200, 0, 40),
            ),
            True,
            {"rgb": 0.9, "event_camera": 0.9},
            True,
        ),
        TargetTrajectory(
            "trajectory-silent",
            "clear-silent",
            "validation",
            (
                TrajectoryPoint(0, 100, 10, 30),
                TrajectoryPoint(1, 125, 10, 30),
            ),
            False,
        ),
    )
    receipt_sha = _load_full_rf_receipt().to_dict()["receiptSha256"]

    first = precompute_demo_candidate_trajectory_results(
        candidates,
        trajectories,
        calibrations,
        rf_geometry_receipt_sha256=receipt_sha,
    )
    second = precompute_demo_candidate_trajectory_results(
        reversed(candidates),
        reversed(trajectories),
        reversed(calibrations),
        rf_geometry_receipt_sha256=receipt_sha,
    )

    assert first == second
    assert first["schemaVersion"] == CANDIDATE_TRAJECTORY_PRECOMPUTE_SCHEMA
    assert first["sourceClass"] == DEMO_SURROGATE_SOURCE
    assert len(first["results"]) == 2 * 2 * 4
    assert first["allCandidateTrajectorySensorRowsPresent"] is True
    assert first["exactSimulatorReplayComplete"] is False
    assert first["measuredCalibrationUsed"] is False
    assert first["rfGeometryQueried"] is False
    silent_rf_rows = [
        item
        for item in first["results"]
        if item["trajectoryId"] == "trajectory-silent"
        and item["sensorClass"] == "passive_rf"
    ]
    assert silent_rf_rows
    assert {item["detectionFraction"] for item in silent_rf_rows} == {0.0}
    assert {item["firstDetectionSeconds"] for item in silent_rf_rows} == {None}

    problem = static_problem_from_precompute(
        first,
        minimum_detection_fraction=0.5,
        maximum_sites=2,
        minimum_evidence_families=1,
        minimum_scenario_coverage_fraction=0.0,
    )
    assert len(problem.demands) == 2
    assert len(problem.candidates) == 2


def test_measured_calibration_cannot_be_declared_without_dataset_hash() -> None:
    with pytest.raises(ValueError, match="dataset_sha256"):
        SensorModelCalibration(
            "bad-measured-record",
            "radar",
            MEASURED_CALIBRATION_EVIDENCE,
            1000,
            0.2,
            360,
            limitations=("test",),
        )


def _robust_problem() -> RobustStaticProblem:
    demands = (
        RobustDemand("clear-north", "clear", critical=True),
        RobustDemand("clear-south", "clear"),
        RobustDemand("rain-east", "rain", critical=True),
        RobustDemand("rain-west", "rain"),
    )
    return RobustStaticProblem(
        "small-robust-fixture",
        demands,
        (
            StaticCandidate(
                "candidate-a",
                "site-a",
                "domain-a",
                2.0,
                {
                    "clear-north": ("active_radar",),
                    "rain-east": ("active_radar",),
                },
            ),
            StaticCandidate(
                "candidate-b",
                "site-b",
                "domain-b",
                2.0,
                {
                    "clear-south": ("passive_rf",),
                    "rain-west": ("passive_rf",),
                },
            ),
            StaticCandidate(
                "candidate-c",
                "site-c",
                "domain-c",
                3.0,
                {
                    "clear-north": ("visual",),
                    "clear-south": ("visual",),
                },
            ),
            StaticCandidate(
                "candidate-d",
                "site-d",
                "domain-d",
                3.0,
                {
                    "rain-east": ("visual",),
                    "rain-west": ("visual",),
                },
            ),
        ),
        maximum_sites=2,
        minimum_evidence_families=1,
        minimum_site_redundancy=1,
        minimum_failure_domain_redundancy=1,
        minimum_scenario_coverage_fraction=1.0,
        budget_units=8.0,
    )


def test_greedy_local_baseline_and_branch_and_bound_are_deterministic() -> None:
    problem = _robust_problem()

    greedy = solve_greedy_local_search(problem)
    exact = solve_robust_branch_and_bound(problem, node_limit=10_000)
    again = solve_robust_branch_and_bound(problem, node_limit=10_000)

    assert greedy.algorithm == GREEDY_LOCAL_ALGORITHM
    assert greedy.status == "FEASIBLE"
    assert exact.algorithm == BRANCH_AND_BOUND_ALGORITHM
    assert exact.status == "OPTIMAL"
    assert exact.optimality_proven is True
    assert exact.infeasibility_proven is False
    assert exact.selected_candidate_ids == ("candidate-a", "candidate-b")
    assert exact.metrics["worstScenarioCoverageFraction"] == 1.0
    assert exact.metrics["totalCostUnits"] == 4.0
    exact_payload = exact.to_dict()
    assert exact_payload["schemaVersion"] == "triad.robust_static_solver_result.v2"
    assert exact_payload["terminationReason"] == "SEARCH_EXHAUSTED_OPTIMAL"
    assert exact_payload["nodeLimitReached"] is False
    assert exact_payload["incumbentObjectiveCostUnits"] == 4.0
    assert exact_payload["objectiveCostLowerBoundUnits"] == 4.0
    assert exact_payload["absoluteCostOptimalityGapUnits"] == 0.0
    assert exact_payload["relativeCostOptimalityGap"] == 0.0
    assert exact_payload["boundSemantics"] == "MATCHED_INCUMBENT_BY_EXHAUSTIVE_SEARCH"
    assert exact.to_dict() == again.to_dict()

    bounded = solve_robust_branch_and_bound(problem, node_limit=1)
    assert bounded.status == "FEASIBLE_NODE_LIMIT"
    assert bounded.optimality_proven is False
    assert bounded.infeasibility_proven is False
    bounded_payload = bounded.to_dict()
    assert bounded_payload["terminationReason"] == "NODE_LIMIT_WITH_FEASIBLE_INCUMBENT"
    assert bounded_payload["nodeLimitReached"] is True
    assert bounded_payload["incumbentObjectiveCostUnits"] == 4.0
    assert bounded_payload["objectiveCostLowerBoundUnits"] == 2.0
    assert bounded_payload["absoluteCostOptimalityGapUnits"] == 2.0
    assert bounded_payload["relativeCostOptimalityGap"] == 0.5
    assert bounded_payload["boundSemantics"] == "GLOBAL_STRUCTURAL_MINIMUM_SELECTION_COST"


def test_branch_and_bound_reports_proven_infeasibility_only_after_complete_search() -> None:
    impossible = RobustStaticProblem(
        "impossible",
        (RobustDemand("critical", "clear", critical=True),),
        (
            StaticCandidate(
                "candidate",
                "site",
                "domain",
                1.0,
                {"critical": ("visual",)},
            ),
        ),
        maximum_sites=1,
        minimum_evidence_families=2,
    )

    result = solve_robust_branch_and_bound(impossible, node_limit=100)

    assert result.status == "INFEASIBLE_PROVEN"
    assert result.infeasibility_proven is True
    assert result.optimality_proven is False
    payload = result.to_dict()
    assert payload["terminationReason"] == "SEARCH_EXHAUSTED_INFEASIBLE"
    assert payload["nodeLimitReached"] is False
    assert payload["incumbentObjectiveCostUnits"] is None
    assert payload["objectiveCostLowerBoundUnits"] is None
    assert payload["absoluteCostOptimalityGapUnits"] is None
    assert payload["relativeCostOptimalityGap"] is None

    # The optimistic bound deliberately ignores same-site exclusivity, so this
    # variant needs search to prove that its two evidence families cannot be
    # selected together.  A one-node budget must therefore remain unresolved.
    node_limited_impossible = RobustStaticProblem(
        "node-limited-impossible",
        (RobustDemand("critical", "clear", critical=True),),
        (
            StaticCandidate(
                "candidate-a",
                "shared-site",
                "domain-a",
                1.0,
                {"critical": ("visual",)},
            ),
            StaticCandidate(
                "candidate-b",
                "shared-site",
                "domain-b",
                2.0,
                {"critical": ("active_radar",)},
            ),
        ),
        maximum_sites=2,
        minimum_evidence_families=2,
    )
    bounded = solve_robust_branch_and_bound(node_limited_impossible, node_limit=1)
    bounded_payload = bounded.to_dict()
    assert bounded.status == "UNRESOLVED_NODE_LIMIT"
    assert bounded_payload["terminationReason"] == "NODE_LIMIT_WITHOUT_FEASIBLE_INCUMBENT"
    assert bounded_payload["nodeLimitReached"] is True
    assert bounded_payload["incumbentObjectiveCostUnits"] is None
    assert bounded_payload["objectiveCostLowerBoundUnits"] == 1.0
    assert bounded_payload["absoluteCostOptimalityGapUnits"] is None
    assert bounded_payload["relativeCostOptimalityGap"] is None


def test_continuous_refiners_use_exactly_equal_budgets_and_are_repeatable() -> None:
    bounds = PoseBounds((-1.0, -1.0, -45.0), (1.0, 1.0, 45.0))

    def objective(pose: tuple[float, ...]) -> float:
        return -(
            (pose[0] - 0.2) ** 2
            + (pose[1] + 0.3) ** 2
            + ((pose[2] - 5.0) / 45.0) ** 2
        )

    initial = (0.8, 0.8, 25.0)
    initial_score = objective(initial)
    first = compare_pose_refiners(
        initial,
        bounds,
        objective,
        evaluation_budget_per_method=48,
        evolutionary_seed=17,
    )
    second = compare_pose_refiners(
        initial,
        bounds,
        objective,
        evaluation_budget_per_method=48,
        evolutionary_seed=17,
    )

    assert first == second
    assert first["equalEvaluationBudget"] is True
    assert {item["evaluationCount"] for item in first["results"]} == {48}
    assert {item["evaluationBudget"] for item in first["results"]} == {48}
    assert all(item["bestScore"] >= initial_score for item in first["results"])
    assert all(math.isfinite(item["bestScore"]) for item in first["results"])
    assert first["exactSimulatorReplayRequiredForAcceptance"] is True
    assert first["physicalDeploymentAuthorized"] is False


def test_four_class_demo_runner_binds_cache_and_solver_comparison() -> None:
    first = build_four_class_demo_study(REPOSITORY_ROOT)
    second = build_four_class_demo_study(REPOSITORY_ROOT)

    assert first == second
    assert first["schemaVersion"] == DEMO_STUDY_SCHEMA
    assert first["executionClass"] == "DEMO_STUDY_ONLY"
    assert first["stage0Readiness"]["productionOptimizationAuthorized"] is False
    assert first["rfActualFileLoad"]["counts"]["solids"] == 1094
    assert len(first["sensorModelCalibration"]["calibrations"]) == 4
    cache = first["candidateTrajectoryPrecompute"]
    assert len(cache["candidates"]) == 5
    assert len(cache["trajectories"]) == 4
    assert len(cache["results"]) == 5 * 4 * 4
    comparison = first["staticSolverComparison"]
    assert comparison["samePrecomputeArtifactSha256"] == cache["artifactSha256"]
    assert comparison["greedyLocalBaseline"]["status"] == "FEASIBLE"
    assert comparison["robustBranchAndBound"]["status"] == "OPTIMAL"
    assert comparison["robustBranchAndBound"]["optimalityProven"] is True
    assert first["continuousPoseComparison"]["equalEvaluationBudget"] is True
    assert first["fieldPerformanceClaimed"] is False
    assert first["physicalDeploymentAuthorized"] is False
