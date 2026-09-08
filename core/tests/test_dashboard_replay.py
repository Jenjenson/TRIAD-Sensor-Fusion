from __future__ import annotations

from copy import deepcopy
import json
from pathlib import Path

import pytest

from singapore_sensor_fusion.placement.contracts import PlacementRequest, stable_digest
from singapore_sensor_fusion.placement.dashboard_replay import (
    DASHBOARD_REPLAY_SCHEMA,
    DEFAULT_MAP_PACKAGE,
    build_dashboard_replay,
    load_recommendation,
    main,
    validate_dashboard_replay,
)


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
REQUEST_PATH = REPOSITORY_ROOT / "core" / "examples" / "istana_1km_placement_request.json"
RECOMMENDATION_PATH = REPOSITORY_ROOT / "core" / "reports" / "istana_1km_recommendation.json"
SCENARIO_PATH = REPOSITORY_ROOT / "unreal" / "Config" / "IstanaSensorPlacement.base.json"


def _inputs():
    return (
        PlacementRequest.load(REQUEST_PATH),
        load_recommendation(RECOMMENDATION_PATH),
        json.loads(SCENARIO_PATH.read_text(encoding="utf-8")),
    )


def test_replay_is_deterministic_and_self_validating() -> None:
    request, recommendation, scenario = _inputs()
    first = build_dashboard_replay(
        request,
        recommendation,
        scenario,
        duration_seconds=4.0,
        cadence_seconds=1.0,
    )
    second = build_dashboard_replay(
        request,
        recommendation,
        scenario,
        duration_seconds=4.0,
        cadence_seconds=1.0,
    )

    assert first == second
    assert first["schemaVersion"] == DASHBOARD_REPLAY_SCHEMA
    assert first["map"]["unrealPackage"] == DEFAULT_MAP_PACKAGE
    assert first["simulationOnly"] is True
    assert first["detectionOnly"] is True
    assert first["operationalUseAuthorized"] is False
    assert first["siteAuthorizationInferred"] is False
    assert first["calibratedProbabilities"] is False
    assert first["scenario"]["runtimeConfigEnabledInSource"] is False
    assert first["scenario"]["stage0Authorization"] == "NOT_INFERRED"
    assert len(first["sensors"]) == 4
    assert first["sensors"][0]["nodeId"] == "PLACEMENT_site_center"
    assert [item["tSeconds"] for item in first["timeline"]["tracks"][0]["samples"]] == [
        0.0,
        1.0,
        2.0,
        3.0,
        4.0,
    ]
    assert first["timeline"]["successSummary"]["allTracksDetected"] is True
    assert first["timeline"]["detections"]
    assert not any(
        item["tSeconds"] == 0.0 for item in first["timeline"]["detections"]
    )
    outcome = first["timeline"]["tracks"][0]["outcome"]
    assert outcome["firstDetectionSeconds"] == 1.0
    assert outcome["firstCorroboratedDetectionSeconds"] == 2.0
    assert all(
        0.0 <= item["confidence"] <= 1.0
        for item in first["timeline"]["detections"]
    )
    validate_dashboard_replay(first)


def test_rf_silent_partition_never_manufactures_passive_rf_detection() -> None:
    request, recommendation, scenario = _inputs()
    replay = build_dashboard_replay(
        request,
        recommendation,
        scenario,
        scenario_id="clear-rf-silent",
        duration_seconds=4.0,
        cadence_seconds=1.0,
    )

    assert replay["scenario"]["rfEmitting"] is False
    assert all(
        item["modality"] != "wideband_rf"
        for item in replay["timeline"]["detections"]
    )
    assert any(
        item["modality"] == "SEARCH_RADAR"
        for item in replay["timeline"]["detections"]
    )


def test_legacy_thermal_is_filtered_and_missing_event_camera_is_explicit() -> None:
    request, recommendation, scenario = _inputs()
    replay = build_dashboard_replay(
        request,
        recommendation,
        scenario,
        duration_seconds=2.0,
        cadence_seconds=1.0,
    )

    scope = replay["stage0Scope"]
    assert scope["legacyRecommendationScopeComplete"] is False
    assert scope["filteredRecommendationModalities"] == ["THERMAL_PTZ"]
    assert scope["missingSensorClasses"] == ["event_camera"]
    assert "event_camera" not in scope["representedSensorClasses"]
    assert any("THERMAL_PTZ" in warning for warning in scope["warnings"])
    assert any("event_camera" in warning for warning in scope["warnings"])
    assert all(
        modality["modality"] != "THERMAL_PTZ"
        for sensor in replay["sensors"]
        for modality in sensor["modalities"]
    )
    assert all(
        detection["modality"] != "THERMAL_PTZ"
        for detection in replay["timeline"]["detections"]
    )
    assert not any(
        detection["modality"] == "event_camera"
        for detection in replay["timeline"]["detections"]
    )


def test_replay_digest_rejects_tampering() -> None:
    request, recommendation, scenario = _inputs()
    replay = build_dashboard_replay(
        request,
        recommendation,
        scenario,
        duration_seconds=2.0,
        cadence_seconds=1.0,
    )
    tampered = deepcopy(replay)
    tampered["sensors"][0]["siteId"] = "silently-edited"

    with pytest.raises(ValueError, match="replayDigest does not match"):
        validate_dashboard_replay(tampered)


def test_validator_rejects_prohibited_sensor_even_with_recomputed_digest() -> None:
    request, recommendation, scenario = _inputs()
    replay = build_dashboard_replay(
        request,
        recommendation,
        scenario,
        duration_seconds=2.0,
        cadence_seconds=1.0,
    )
    tampered = deepcopy(replay)
    tampered["sensors"][0]["modalities"][0]["modality"] = "THERMAL_PTZ"
    tampered.pop("replayDigest")
    tampered["replayDigest"] = stable_digest(tampered)

    with pytest.raises(ValueError, match="prohibited deployed modality"):
        validate_dashboard_replay(tampered)


def test_unsupported_trajectory_fails_closed() -> None:
    request, recommendation, scenario = _inputs()
    scenario["DemoTargets"][0]["Trajectory"] = "Teleport"

    with pytest.raises(ValueError, match="unsupported demo trajectory"):
        build_dashboard_replay(
            request,
            recommendation,
            scenario,
            duration_seconds=2.0,
            cadence_seconds=1.0,
        )


def test_cli_creates_new_artifact_and_refuses_overwrite(tmp_path: Path) -> None:
    output = tmp_path / "replay.json"
    arguments = [
        str(REQUEST_PATH),
        str(RECOMMENDATION_PATH),
        str(SCENARIO_PATH),
        "--output",
        str(output),
        "--duration-seconds",
        "2",
        "--cadence-seconds",
        "1",
    ]

    assert main(arguments) == 0
    replay = json.loads(output.read_text(encoding="utf-8"))
    validate_dashboard_replay(replay)
    with pytest.raises(FileExistsError, match="refusing to overwrite"):
        main(arguments)
