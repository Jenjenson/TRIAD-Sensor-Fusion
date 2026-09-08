"""Train the quarantined RL placer on an explicit synthetic Istana demo.

This adapter is deliberately outside the production placement workflow.  It
adds a small orientation lattice to the four-class demo candidates, evaluates
that lattice with the existing synthetic kinematic surrogate, and compares
tabular Q-learning against the exact legacy placement solver.  It never edits
Unreal, asserts Stage-0 readiness, or authorizes a physical placement.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
from typing import Sequence


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
CORE_SOURCE = REPOSITORY_ROOT / "core" / "src"
if str(CORE_SOURCE) not in sys.path:
    sys.path.insert(0, str(CORE_SOURCE))

from singapore_sensor_fusion.geodesy import ENU, Geodetic, enu_to_geodetic
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
    stable_digest,
)
from singapore_sensor_fusion.placement.demo_study import (
    build_four_class_demo_study,
)
from singapore_sensor_fusion.placement.optimizer import solve_placement
from singapore_sensor_fusion.placement.planning_pipeline import (
    CandidateSensorPose,
    TargetTrajectory,
    TrajectoryPoint,
    default_synthetic_demo_calibrations,
    precompute_demo_candidate_trajectory_results,
)
from singapore_sensor_fusion.placement.rl import (
    RLPlacementConfig,
    train_weather_robust_q_learning,
)


SCHEMA_VERSION = "triad.rl_demo_training.v1"
SOURCE_CLASS = "SYNTHETIC_DEMO_RL_ORIENTATION_LATTICE_V1"
ORIENTATION_OFFSETS_DEGREES = (-15.0, 0.0, 15.0)
MODALITY_BY_SENSOR_CLASS = {
    "radar": "SEARCH_RADAR",
    "passive_rf": "wideband_rf",
    "rgb": "rgb",
    "event_camera": "event_camera",
}
WEATHER_BY_SCENARIO = {
    "demo-clear-emitting": "Clear",
    "demo-clear-rf-silent": "Clear",
    "demo-haze-emitting": "Haze",
    "demo-monsoon-emitting": "Monsoon",
}


def _orientation_suffix(offset_degrees: float) -> str:
    if offset_degrees < 0.0:
        return f"m{abs(int(offset_degrees)):02d}"
    return f"p{int(offset_degrees):02d}"


def _trajectories(precompute: dict[str, object]) -> tuple[TargetTrajectory, ...]:
    rows: list[TargetTrajectory] = []
    for item in precompute["trajectories"]:
        points = tuple(
            TrajectoryPoint(
                float(point["timeSeconds"]),
                float(point["enuMeters"][0]),
                float(point["enuMeters"][1]),
                float(point["enuMeters"][2]),
            )
            for point in item["points"]
        )
        rows.append(
            TargetTrajectory(
                trajectory_id=str(item["trajectoryId"]),
                scenario_id=str(item["scenarioId"]),
                partition_id="DEMO_RL_TRAINING_ONLY_NOT_BENCHMARK_PARTITION",
                points=points,
                rf_emitting=bool(item["rfEmitting"]),
                sensor_quality_factors=dict(item["sensorQualityFactors"]),
                critical=True,
            )
        )
    return tuple(rows)


def _candidate_lattice(
    precompute: dict[str, object],
) -> tuple[CandidateSensorPose, ...]:
    rows: list[CandidateSensorPose] = []
    for item in precompute["candidates"]:
        x_meters, y_meters, z_meters = item["poseEnuMeters"]
        for offset_degrees in ORIENTATION_OFFSETS_DEGREES:
            suffix = _orientation_suffix(offset_degrees)
            rows.append(
                CandidateSensorPose(
                    candidate_id=f"{item['candidateId']}-yaw-{suffix}",
                    site_id=str(item["siteId"]),
                    failure_domain_id=str(item["failureDomainId"]),
                    x_meters=float(x_meters),
                    y_meters=float(y_meters),
                    z_meters=float(z_meters),
                    yaw_degrees=float(item["yawDegrees"]) + offset_degrees,
                    sensor_classes=tuple(item["sensorClasses"]),
                    cost_units=float(item["costUnits"]),
                )
            )
    return tuple(rows)


def _build_legacy_rl_inputs(
    precompute: dict[str, object],
) -> tuple[PlacementRequest, PlacementSurvey]:
    center = GeoPoint(
        1.30709615,
        103.84288055,
        0.0,
        "SYNTHETIC_LOCAL_UP_NOT_SURVEY_DATUM",
    )
    request = PlacementRequest(
        request_id="istana-four-class-synthetic-rl-demo-v1",
        aoi=AreaOfInterest(
            "istana-1km-synthetic-rl-demo",
            center,
            1000.0,
            center_provenance=(
                "Provisional public-reference centre used only for a synthetic "
                "RL software exercise."
            ),
            surface_height_source="SYNTHETIC_LOCAL_UP_NOT_SURVEY_DATUM",
        ),
        sampling=SamplingPlan(200.0, (60.0,)),
        sensor_packages=(
            SensorPackage(
                package_id="four-class-synthetic-demo-package",
                modalities=("SEARCH_RADAR", "wideband_rf", "rgb", "event_camera"),
                families=("active_radar", "passive_rf", "visual"),
                cost_units=0.0,
                cost_semantics="SYNTHETIC_DEMO_UNITS_ONLY",
            ),
        ),
        scenarios=tuple(
            PlacementScenario(
                scenario_id=str(item["scenarioId"]),
                weather_profile=WEATHER_BY_SCENARIO[str(item["scenarioId"])],
                rf_emitting=bool(item["rfEmitting"]),
                radar_cross_section_square_meters=0.03,
                weight=1.0,
            )
            for item in precompute["trajectories"]
        ),
        constraints=PlacementConstraints(
            maximum_sites=4,
            minimum_family_count=2,
            minimum_site_redundancy=2,
            minimum_failure_domain_redundancy=2,
            minimum_coverage_fraction=1.0,
            budget_units=10.0,
            critical_sample_ids=("complete-trajectory-demand",),
        ),
        random_seed=17,
    )

    origin = Geodetic(
        latitude_deg=center.latitude_degrees,
        longitude_deg=center.longitude_degrees,
        altitude_m=center.height_meters,
    )
    calibration_by_class = {
        item.sensor_class: item for item in default_synthetic_demo_calibrations()
    }
    candidate_by_id = {
        str(item["candidateId"]): item for item in precompute["candidates"]
    }
    options: list[CandidateOption] = []
    for candidate_id, item in sorted(candidate_by_id.items()):
        x_meters, y_meters, z_meters = item["poseEnuMeters"]
        location = enu_to_geodetic(
            ENU(float(x_meters), float(y_meters), float(z_meters)), origin
        )
        options.append(
            CandidateOption(
                option_id=candidate_id,
                site_id=str(item["siteId"]),
                package_id="four-class-synthetic-demo-package",
                orientation_id=candidate_id.rsplit("-yaw-", 1)[-1],
                location=GeoPoint(
                    location.latitude_deg,
                    location.longitude_deg,
                    location.altitude_m,
                    "SYNTHETIC_LOCAL_UP_NOT_SURVEY_DATUM",
                ),
                failure_domain_id=str(item["failureDomainId"]),
                site_cost_units=float(item["costUnits"]),
                camera_yaw_degrees=float(item["yawDegrees"]),
            )
        )

    sample = CoverageSample(
        "complete-trajectory-demand",
        GeoPoint(
            center.latitude_degrees,
            center.longitude_degrees,
            60.0,
            "SYNTHETIC_LOCAL_UP_NOT_SURVEY_DATUM",
        ),
        0.0,
        0.0,
        60.0,
        critical=True,
    )
    evaluations: list[CoverageEvaluation] = []
    for row in precompute["results"]:
        sensor_class = str(row["sensorClass"])
        model = calibration_by_class[sensor_class]
        detection_fraction = float(row["detectionFraction"])
        eligible = detection_fraction + 1e-12 >= 0.4
        evaluations.append(
            CoverageEvaluation(
                option_id=str(row["candidateId"]),
                sample_id=sample.sample_id,
                scenario_id=str(row["scenarioId"]),
                modality=MODALITY_BY_SENSOR_CLASS[sensor_class],
                family=str(row["evidenceFamily"]),
                quality=float(row["meanQuality"]),
                eligible=eligible,
                range_meters=model.maximum_range_meters,
                maximum_range_meters=model.maximum_range_meters,
                within_range=True,
                within_fov=True,
                line_of_sight=True,
                line_of_sight_required=False,
                source=SOURCE_CLASS,
            )
        )

    survey = PlacementSurvey(
        request_id=request.request_id,
        options=tuple(options),
        samples=(sample,),
        evaluations=tuple(evaluations),
        world={
            "executionClass": "DEMO_STUDY_ONLY",
            "sourceClass": SOURCE_CLASS,
            "sourcePrecomputeArtifactSha256": precompute["artifactSha256"],
            "rlPlacementReadiness": {
                "rfGeometryReady": False,
                "sensorModelsCalibrated": False,
                "weatherDifferentiated": True,
                "orientationSweepEvaluated": True,
                "exactSimulatorReplayRequired": True,
                "attestationSemantics": (
                    "NOT_USED; readiness enforcement is explicitly bypassed "
                    "for this synthetic demo training run."
                ),
            },
            "physicalDeploymentAuthorized": False,
        },
    )
    return request, survey


def train_demo(
    *, episodes: int, seeds: tuple[int, ...]
) -> dict[str, object]:
    base_study = build_four_class_demo_study(REPOSITORY_ROOT)
    base_precompute = base_study["candidateTrajectoryPrecompute"]
    lattice_candidates = _candidate_lattice(base_precompute)
    trajectories = _trajectories(base_precompute)
    calibrations = default_synthetic_demo_calibrations()
    lattice_precompute = precompute_demo_candidate_trajectory_results(
        lattice_candidates,
        trajectories,
        calibrations,
        rf_geometry_receipt_sha256=base_study["rfActualFileLoad"]["receiptSha256"],
    )
    request, survey = _build_legacy_rl_inputs(lattice_precompute)
    exact = solve_placement(request, survey, exact_option_limit=20)
    exact_payload = exact.to_dict()

    training_runs = []
    for seed in seeds:
        config = RLPlacementConfig(
            episodes=episodes,
            random_seed=seed,
            require_readiness_attestation=False,
            require_weather_differentiation=True,
            require_orientation_search=True,
        )
        result = train_weather_robust_q_learning(
            request,
            survey,
            config=config,
        )
        result_payload = result.to_dict()
        matches_exact_objective = (
            result_payload["status"] == exact_payload["status"]
            and abs(
                float(result_payload["totalCostUnits"])
                - float(exact_payload["totalCostUnits"])
            )
            <= 1e-9
            and stable_digest(result_payload["metrics"])
            == stable_digest(exact_payload["metrics"])
        )
        training_runs.append(
            {
                "seed": seed,
                "readinessAttestationEnforced": False,
                "syntheticDemoBypassReason": (
                    "The real Stage-0 contract is blocked; no readiness flag is "
                    "promoted to true for this software-only exercise."
                ),
                "matchesExactSelection": (
                    tuple(result.selected_option_ids)
                    == tuple(exact.selected_option_ids)
                ),
                "matchesExactObjective": matches_exact_objective,
                "result": result_payload,
            }
        )

    payload: dict[str, object] = {
        "schemaVersion": SCHEMA_VERSION,
        "executionClass": "DEMO_STUDY_ONLY",
        "algorithm": "masked_weather_robust_tabular_q_learning_v1",
        "episodesPerSeed": episodes,
        "seeds": list(seeds),
        "orientationOffsetsDegrees": list(ORIENTATION_OFFSETS_DEGREES),
        "sourceClass": SOURCE_CLASS,
        "sourceFourClassStudySha256": base_study["artifactSha256"],
        "sourceRfActualFileLoadReceiptSha256": base_study["rfActualFileLoad"]["receiptSha256"],
        "orientationLatticePrecomputeSha256": lattice_precompute["artifactSha256"],
        "request": request.to_dict(),
        "survey": survey.to_dict(),
        "exactReference": exact_payload,
        "trainingRuns": training_runs,
        "allRunsFeasible": all(
            run["result"]["status"] == "FEASIBLE" for run in training_runs
        ),
        "allRunsMatchExactSelection": all(
            run["matchesExactSelection"] for run in training_runs
        ),
        "allRunsMatchExactObjective": all(
            run["matchesExactObjective"] for run in training_runs
        ),
        "stage0ReadinessBypassed": True,
        "exactSimulatorReplayComplete": False,
        "measuredCalibrationUsed": False,
        "fieldPerformanceClaimed": False,
        "physicalDeploymentAuthorized": False,
        "limitations": [
            "Training uses deterministic synthetic kinematic evidence, not measured calibration.",
            "The one-kilometre RF artifact is hash-loaded by the source study, but this RL adapter does not query it for propagation.",
            "The orientation lattice is limited to the declared -15/0/+15 degree offsets.",
            "A matching exact result validates this small software fixture only; it does not validate field performance.",
        ],
    }
    payload["artifactSha256"] = stable_digest(payload)
    return payload


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Train the RL placer on an explicitly synthetic Istana demo."
    )
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--episodes", type=int, default=30_000)
    parser.add_argument("--seeds", type=int, nargs="+", default=[17, 5011, 6011])
    args = parser.parse_args(argv)
    if args.episodes <= 0:
        parser.error("--episodes must be positive")
    if not args.seeds:
        parser.error("--seeds requires at least one integer")

    payload = train_demo(episodes=args.episodes, seeds=tuple(args.seeds))
    destination = args.output.resolve(strict=False)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with destination.open("x", encoding="utf-8", newline="\n") as handle:
        json.dump(payload, handle, ensure_ascii=False, sort_keys=True, indent=2)
        handle.write("\n")
    print(
        json.dumps(
            {
                "output": str(destination),
                "artifactSha256": payload["artifactSha256"],
                "episodesPerSeed": payload["episodesPerSeed"],
                "seeds": payload["seeds"],
                "allRunsFeasible": payload["allRunsFeasible"],
                "allRunsMatchExactSelection": payload[
                    "allRunsMatchExactSelection"
                ],
                "allRunsMatchExactObjective": payload[
                    "allRunsMatchExactObjective"
                ],
                "exactSelectedOptionIds": payload["exactReference"][
                    "selectedOptions"
                ],
            },
            separators=(",", ":"),
        )
    )
    return 0 if payload["allRunsFeasible"] else 2


if __name__ == "__main__":
    raise SystemExit(main())
