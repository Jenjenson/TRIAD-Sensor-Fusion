"""Deterministic four-class study-mode runner for dashboard integration.

The runner is intentionally self-contained and visibly synthetic.  It loads
the repository's actual OneKilometreV2 RF files, but its trajectory detections
remain a kinematic UI/solver surrogate until measured calibration and exact
Unreal replays are supplied.  It never edits the legacy recommendation.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Sequence

from .contracts import stable_digest
from .planning_pipeline import (
    CandidateSensorPose,
    PoseBounds,
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
from .study_contract import Stage0StudyContract


DEMO_STUDY_SCHEMA = "triad.four_class_placement_demo_study.v1"


def _trajectory(
    trajectory_id: str,
    scenario_id: str,
    coordinates: tuple[tuple[float, float], ...],
    *,
    rf_emitting: bool,
    factors: dict[str, float],
) -> TargetTrajectory:
    return TargetTrajectory(
        trajectory_id=trajectory_id,
        scenario_id=scenario_id,
        partition_id="DEMO_ONLY_NOT_BENCHMARK_PARTITION",
        points=tuple(
            TrajectoryPoint(index * 1.5, x, y, 60.0)
            for index, (x, y) in enumerate(coordinates)
        ),
        rf_emitting=rf_emitting,
        sensor_quality_factors=factors,
        critical=True,
    )


def build_four_class_demo_study(
    repository_root: str | Path | None = None,
) -> dict[str, object]:
    """Build one stable cache and equal-input solver/refinement comparison."""

    root = (
        Path(repository_root).resolve(strict=True)
        if repository_root is not None
        else Path(__file__).resolve().parents[4]
    )
    core_root = root / "core"
    rf_root = (
        root
        / "unreal"
        / "Plugins"
        / "TRIADSensorFusion"
        / "Resources"
        / "RF"
    )
    stage0 = Stage0StudyContract.load(
        core_root
        / "examples"
        / "istana_stage0_sensor_placement_study_contract.v1.json"
    )
    stage0_receipt = assess_stage0_readiness(stage0)
    rf_receipt = load_and_validate_rf_geometry(
        rf_root / "IstanaPublicViewRFOneKilometreV2.geometry.json",
        rf_root / "istana_rf_materials_one_kilometre_v2.catalog.json",
        rf_root / "istana_rf_scene_one_kilometre_v2.contract.json",
    ).to_dict()
    calibrations = default_synthetic_demo_calibrations()
    calibration_bundle = build_calibration_bundle(calibrations)

    candidates = (
        CandidateSensorPose(
            "demo-north", "demo-site-north", "demo-domain-north", 0, 600, 18, -90, cost_units=2.4
        ),
        CandidateSensorPose(
            "demo-east", "demo-site-east", "demo-domain-east", 600, 0, 18, 180, cost_units=2.2
        ),
        CandidateSensorPose(
            "demo-south", "demo-site-south", "demo-domain-south", 0, -600, 18, 90, cost_units=2.3
        ),
        CandidateSensorPose(
            "demo-west", "demo-site-west", "demo-domain-west", -600, 0, 18, 0, cost_units=2.1
        ),
        CandidateSensorPose(
            "demo-centre", "demo-site-centre", "demo-domain-centre", 0, 0, 28, 0, cost_units=3.0
        ),
    )
    trajectories = (
        _trajectory(
            "demo-ingress-north",
            "demo-clear-emitting",
            ((0, 900), (0, 700), (0, 500), (0, 300), (0, 100)),
            rf_emitting=True,
            factors={"radar": 1.0, "passive_rf": 1.0, "rgb": 0.95, "event_camera": 0.95},
        ),
        _trajectory(
            "demo-ingress-east",
            "demo-haze-emitting",
            ((900, 0), (700, 0), (500, 0), (300, 0), (100, 0)),
            rf_emitting=True,
            factors={"radar": 0.92, "passive_rf": 0.94, "rgb": 0.58, "event_camera": 0.68},
        ),
        _trajectory(
            "demo-ingress-south",
            "demo-monsoon-emitting",
            ((0, -900), (0, -700), (0, -500), (0, -300), (0, -100)),
            rf_emitting=True,
            factors={"radar": 0.82, "passive_rf": 0.88, "rgb": 0.48, "event_camera": 0.62},
        ),
        _trajectory(
            "demo-ingress-west-rf-silent",
            "demo-clear-rf-silent",
            ((-900, 0), (-700, 0), (-500, 0), (-300, 0), (-100, 0)),
            rf_emitting=False,
            factors={"radar": 1.0, "passive_rf": 1.0, "rgb": 0.92, "event_camera": 0.92},
        ),
    )
    precompute = precompute_demo_candidate_trajectory_results(
        candidates,
        trajectories,
        calibrations,
        rf_geometry_receipt_sha256=str(rf_receipt["receiptSha256"]),
    )
    problem = static_problem_from_precompute(
        precompute,
        minimum_detection_fraction=0.4,
        maximum_sites=4,
        minimum_evidence_families=2,
        minimum_site_redundancy=2,
        minimum_failure_domain_redundancy=2,
        minimum_scenario_coverage_fraction=1.0,
        budget_units=10.0,
    )
    greedy = solve_greedy_local_search(problem)
    exact = solve_robust_branch_and_bound(problem, node_limit=100_000)

    def pose_demo_score(pose: tuple[float, ...]) -> float:
        # A deterministic UI/contract objective only. Exact simulator replay
        # remains mandatory before accepting any physical pose.
        return -(
            ((pose[0] - 12.0) / 25.0) ** 2
            + ((pose[1] + 8.0) / 25.0) ** 2
            + ((pose[2] - 4.0) / 30.0) ** 2
        )

    refinement = compare_pose_refiners(
        (0.0, 0.0, 0.0),
        PoseBounds((-25.0, -25.0, -30.0), (25.0, 25.0, 30.0)),
        pose_demo_score,
        evaluation_budget_per_method=48,
        evolutionary_seed=17,
    )
    payload: dict[str, object] = {
        "schemaVersion": DEMO_STUDY_SCHEMA,
        "executionClass": "DEMO_STUDY_ONLY",
        "stage0Readiness": stage0_receipt,
        "rfActualFileLoad": rf_receipt,
        "sensorModelCalibration": calibration_bundle,
        "candidateTrajectoryPrecompute": precompute,
        "staticSolverComparison": {
            "samePrecomputeArtifactSha256": precompute["artifactSha256"],
            "greedyLocalBaseline": greedy.to_dict(),
            "robustBranchAndBound": exact.to_dict(),
        },
        "continuousPoseComparison": refinement,
        "limitations": [
            "All candidate permissions and physical mount suitability remain unresolved.",
            "Per-trajectory detections are a synthetic kinematic demo surrogate, not Unreal replay or measured performance.",
            "OneKilometreV2 geometry loads and hash-checks, but this demo surrogate does not query it for propagation.",
            "Final candidates require exact simulator replay, measured calibration, privacy/RF approval, and independent review.",
        ],
        "fieldPerformanceClaimed": False,
        "physicalDeploymentAuthorized": False,
    }
    payload["artifactSha256"] = stable_digest(payload)
    return payload


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Generate a deterministic four-class study-mode artifact. "
            "The output is synthetic, review-only, and never changes Unreal."
        )
    )
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--repository-root", type=Path)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    artifact = build_four_class_demo_study(args.repository_root)
    output = args.output.resolve(strict=False)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8", newline="\n") as handle:
        json.dump(artifact, handle, ensure_ascii=False, sort_keys=True, indent=2)
        handle.write("\n")
    print(
        json.dumps(
            {
                "artifact": str(output),
                "artifactSha256": artifact["artifactSha256"],
                "executionClass": artifact["executionClass"],
                "physicalDeploymentAuthorized": False,
            },
            separators=(",", ":"),
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


__all__ = ["DEMO_STUDY_SCHEMA", "build_four_class_demo_study", "main"]
