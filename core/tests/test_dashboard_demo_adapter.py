from __future__ import annotations

from copy import deepcopy
import json
from pathlib import Path

import pytest

from singapore_sensor_fusion.placement.contracts import stable_digest
from singapore_sensor_fusion.placement.dashboard_demo_adapter import (
    DEFAULT_ACQUISITION_INTERVAL_SECONDS,
    EXPECTED_SELECTED_CANDIDATE_IDS,
    EXPECTED_TRAJECTORY_IDS,
    build_dashboard_replay_from_demo_study,
    main,
)
from singapore_sensor_fusion.placement.dashboard_replay import (
    DASHBOARD_REPLAY_SCHEMA,
    validate_dashboard_replay,
)
from singapore_sensor_fusion.placement.demo_study import build_four_class_demo_study


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


def _study() -> dict[str, object]:
    return build_four_class_demo_study(REPOSITORY_ROOT)


def _rehash(value: dict[str, object], digest_key: str) -> None:
    value.pop(digest_key, None)
    value[digest_key] = stable_digest(value)


def test_adapter_is_deterministic_viewable_and_maps_only_exact_selection() -> None:
    study = _study()
    first = build_dashboard_replay_from_demo_study(study)
    second = build_dashboard_replay_from_demo_study(study)

    assert first == second
    validate_dashboard_replay(first)
    assert first["schemaVersion"] == DASHBOARD_REPLAY_SCHEMA
    assert first["simulationOnly"] is True
    assert first["detectionOnly"] is True
    assert first["operationalUseAuthorized"] is False
    assert first["siteAuthorizationInferred"] is False
    assert first["calibratedProbabilities"] is False
    assert tuple(
        sorted(item["optionId"] for item in first["sensors"])
    ) == EXPECTED_SELECTED_CANDIDATE_IDS
    assert len(first["sensors"]) == 2
    assert all(len(item["modalities"]) == 4 for item in first["sensors"])
    assert {
        modality["sensorClass"]
        for sensor in first["sensors"]
        for modality in sensor["modalities"]
    } == {"radar", "passive_rf", "rgb", "event_camera"}
    assert tuple(
        sorted(track["displayName"] for track in first["timeline"]["tracks"])
    ) == EXPECTED_TRAJECTORY_IDS
    assert first["timeline"]["successSummary"]["simulatedTrackCount"] == 4
    assert first["timeline"]["successSummary"]["detectedTrackCount"] == 4
    assert first["timeline"]["successSummary"]["allTracksDetected"] is True
    assert first["timeline"]["successSummary"]["corroboratedTrackCount"] == 0
    assert first["recommendation"]["selectedCandidateIds"] == list(
        EXPECTED_SELECTED_CANDIDATE_IDS
    )
    assert first["recommendation"]["optimalityProvenForPrecomputedBinaryModel"] is True
    assert first["recommendation"]["solverProof"] == {
        "schemaVersion": "triad.robust_static_solver_result.v2",
        "terminationReason": "SEARCH_EXHAUSTED_OPTIMAL",
        "incumbentObjectiveCostUnits": 4.4,
        "objectiveCostLowerBoundUnits": 4.4,
        "absoluteCostOptimalityGapUnits": 0.0,
        "relativeCostOptimalityGap": 0.0,
        "nodeLimitReached": False,
        "exploredNodeCount": 17,
        "nodeLimit": 100_000,
        "boundSemantics": "MATCHED_INCUMBENT_BY_EXHAUSTIVE_SEARCH",
    }
    assert first["recommendation"]["coverageMetricLabel"] == (
        "Precomputed-model worst coverage"
    )
    assert first["stage0Scope"]["executionClass"] == "DEMO_STUDY_ONLY"
    assert first["stage0Scope"]["productionOptimizationAuthorized"] is False
    assert first["stage0Scope"]["physicalDeploymentAuthorized"] is False
    assert first["stage0Scope"]["blockers"]


def test_event_timing_and_scores_are_derived_only_from_bound_precompute_rows() -> None:
    study = _study()
    replay = build_dashboard_replay_from_demo_study(study)
    rows = {
        (row["candidateId"], row["trajectoryId"], row["sensorClass"]): row
        for row in study["candidateTrajectoryPrecompute"]["results"]
    }
    trajectory_by_track = {
        track["trackId"]: track["displayName"]
        for track in replay["timeline"]["tracks"]
    }
    candidate_by_node = {
        sensor["nodeId"]: sensor["optionId"] for sensor in replay["sensors"]
    }
    events = replay["timeline"]["detections"]

    assert events
    assert min(event["tSeconds"] for event in events) == (
        DEFAULT_ACQUISITION_INTERVAL_SECONDS
    )
    assert not any(
        event["tSeconds"] < DEFAULT_ACQUISITION_INTERVAL_SECONDS
        for event in events
    )
    assert {event["sensorClass"] for event in events} == {
        "radar",
        "passive_rf",
        "rgb",
        "event_camera",
    }
    for event in events:
        key = (
            candidate_by_node[event["nodeId"]],
            trajectory_by_track[event["trackId"]],
            event["sensorClass"],
        )
        row = rows[key]
        assert row["firstDetectionSeconds"] is not None
        assert event["tSeconds"] == (
            DEFAULT_ACQUISITION_INTERVAL_SECONDS
            + row["firstDetectionSeconds"]
        )
        assert event["sourcePrecomputeTimeSeconds"] == row["firstDetectionSeconds"]
        assert event["confidence"] == row["meanQuality"]
        assert event["detectionFraction"] == row["detectionFraction"]
        assert event["sourcePrecomputeRowSha256"] == stable_digest(row)
        assert event["source"] == "SYNTHETIC_DEMO_KINEMATIC_SURROGATE_V1"
        assert event["exactSimulatorReplay"] is False
        assert event["measuredCalibration"] is False
        assert event["rfGeometryQueried"] is False

    silent_track_id = next(
        track["trackId"]
        for track in replay["timeline"]["tracks"]
        if track["displayName"] == "demo-ingress-west-rf-silent"
    )
    assert all(
        not (
            event["trackId"] == silent_track_id
            and event["sensorClass"] == "passive_rf"
        )
        for event in events
    )
    assert all(
        track["outcome"]["firstCorroboratedDetectionSeconds"] is None
        for track in replay["timeline"]["tracks"]
    )


def test_adapter_rejects_a_different_or_unproven_static_selection() -> None:
    changed_selection = deepcopy(_study())
    exact = changed_selection["staticSolverComparison"]["robustBranchAndBound"]
    exact["selectedCandidateIds"] = ["demo-east", "demo-west"]
    _rehash(exact, "resultSha256")
    _rehash(changed_selection, "artifactSha256")

    with pytest.raises(ValueError, match="bounded adapter expected"):
        build_dashboard_replay_from_demo_study(changed_selection)

    unfinished = deepcopy(_study())
    exact = unfinished["staticSolverComparison"]["robustBranchAndBound"]
    exact["status"] = "FEASIBLE_NODE_LIMIT"
    exact["optimalityProven"] = False
    _rehash(exact, "resultSha256")
    _rehash(unfinished, "artifactSha256")

    with pytest.raises(ValueError, match="completed optimal static solve"):
        build_dashboard_replay_from_demo_study(unfinished)


@pytest.mark.parametrize(
    ("field", "value", "message"),
    (
        ("schemaVersion", "triad.robust_static_solver_result.v1", "v2 robust"),
        ("nodeLimitReached", True, "node-limit flag contradicts"),
        ("objectiveCostLowerBoundUnits", 4.3, "absolute cost gap contradicts"),
        ("absoluteCostOptimalityGapUnits", 0.1, "absolute cost gap contradicts"),
        ("relativeCostOptimalityGap", 0.1, "relative cost gap contradicts"),
    ),
)
def test_adapter_rejects_tampered_v2_solver_proof(
    field: str, value: object, message: str
) -> None:
    study = deepcopy(_study())
    exact = study["staticSolverComparison"]["robustBranchAndBound"]
    exact[field] = value
    _rehash(exact, "resultSha256")
    _rehash(study, "artifactSha256")

    with pytest.raises(ValueError, match=message):
        build_dashboard_replay_from_demo_study(study)


def test_replay_validation_rejects_tampered_displayed_solver_proof() -> None:
    replay = build_dashboard_replay_from_demo_study(_study())
    replay["recommendation"]["solverProof"]["relativeCostOptimalityGap"] = 0.01
    _rehash(replay, "replayDigest")

    with pytest.raises(ValueError, match="zero cost gaps"):
        validate_dashboard_replay(replay)


def test_replay_validation_rejects_proof_with_weakened_demo_scope() -> None:
    replay = build_dashboard_replay_from_demo_study(_study())
    replay["stage0Scope"]["physicalDeploymentAuthorized"] = True
    _rehash(replay, "replayDigest")

    with pytest.raises(ValueError, match="physicalDeploymentAuthorized"):
        validate_dashboard_replay(replay)


def test_preproof_four_class_replay_remains_loadable() -> None:
    replay = json.loads(
        (
            REPOSITORY_ROOT
            / "core"
            / "reports"
            / "istana_four_class_dashboard_replay.v1.json"
        ).read_text(encoding="utf-8")
    )

    assert replay["recommendation"].get("solverProof") is None
    validate_dashboard_replay(replay)


def test_cli_writes_create_new_replay_and_refuses_overwrite(tmp_path: Path) -> None:
    study_path = tmp_path / "study.json"
    output_path = tmp_path / "replay.json"
    study_path.write_text(
        json.dumps(_study(), ensure_ascii=False, sort_keys=True), encoding="utf-8"
    )
    arguments = [str(study_path), "--output", str(output_path)]

    assert main(arguments) == 0
    replay = json.loads(output_path.read_text(encoding="utf-8"))
    validate_dashboard_replay(replay)
    assert replay["recommendation"]["selectedCandidateIds"] == [
        "demo-south",
        "demo-west",
    ]
    with pytest.raises(FileExistsError, match="refusing to overwrite"):
        main(arguments)
