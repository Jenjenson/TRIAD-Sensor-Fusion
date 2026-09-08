from __future__ import annotations

from copy import deepcopy
import hashlib
import json
from pathlib import Path

import pytest

from singapore_sensor_fusion.placement.study_contract import (
    ALLOWED_SENSOR_CLASSES,
    PROHIBITED_DEPLOYED_MODALITIES,
    STAGE0_STUDY_SCHEMA,
    Stage0StudyContract,
    validate_stage0_study_contract,
)
from singapore_sensor_fusion.placement.validate_study_contract import (
    main as validate_main,
)

EXAMPLE_PATH = (
    Path(__file__).parents[1]
    / "examples"
    / "istana_stage0_sensor_placement_study_contract.v1.json"
)


def _payload() -> dict[str, object]:
    return json.loads(EXAMPLE_PATH.read_text(encoding="utf-8"))


def _hardware(payload: dict[str, object], sensor_class: str) -> dict[str, object]:
    inventory = payload["hardwareInventory"]
    assert isinstance(inventory, list)
    return next(item for item in inventory if item["sensorClass"] == sensor_class)


def _artifact(payload: dict[str, object], artifact_id: str) -> dict[str, object]:
    bindings = payload["artifactBindings"]
    assert isinstance(bindings, list)
    return next(item for item in bindings if item["artifactId"] == artifact_id)


def _materialize_verified_artifact(
    payload: dict[str, object], artifact_id: str, directory: Path
) -> Path:
    artifact_path = directory / f"{artifact_id}.bin"
    contents = f"verified bytes for {artifact_id}\n".encode()
    artifact_path.write_bytes(contents)
    binding = _artifact(payload, artifact_id)
    binding["uri"] = artifact_path.as_uri()
    binding["sha256"] = hashlib.sha256(contents).hexdigest()
    binding["readiness"] = "VERIFIED"
    return artifact_path


def _add_artifact(
    payload: dict[str, object], artifact_id: str, role: str
) -> dict[str, object]:
    binding = {
        "artifactId": artifact_id,
        "role": role,
        "sha256": "0" * 64,
        "uri": f"unresolved://{artifact_id}",
        "provenance": "Test-only unresolved evidence.",
        "readiness": "UNVERIFIED",
    }
    bindings = payload["artifactBindings"]
    assert isinstance(bindings, list)
    bindings.append(binding)
    return binding


def test_example_is_valid_immutable_and_emits_planning_only_receipt(
    capsys: pytest.CaptureFixture[str],
) -> None:
    contract = Stage0StudyContract.load(EXAMPLE_PATH)

    assert contract.schema_version == STAGE0_STUDY_SCHEMA
    assert set(contract.payload["scope"]["allowedSensorClasses"]) == set(
        ALLOWED_SENSOR_CLASSES
    )
    assert set(contract.payload["scope"]["prohibitedDeployedModalities"]) == set(
        PROHIBITED_DEPLOYED_MODALITIES
    )
    assert len(contract.digest) == 64
    assert contract.digest == Stage0StudyContract.load(EXAMPLE_PATH).digest
    assert (
        contract.validation_receipt()["productionOptimizerImplementationAuthorized"]
        is False
    )
    assert contract.validation_receipt()["physicalDeploymentAuthorized"] is False
    with pytest.raises(TypeError):
        contract.payload["studyId"] = "scope-drift"

    assert validate_main([str(EXAMPLE_PATH)]) == 0
    receipt = json.loads(capsys.readouterr().out)
    assert receipt["valid"] is True
    assert receipt["contractSha256"] == contract.digest


@pytest.mark.parametrize(
    "missing", ["requirements", "criticalZoneCatalog", "artifactBindings", "rlGoNoGo"]
)
def test_missing_root_sections_fail_closed(missing: str) -> None:
    payload = _payload()
    del payload[missing]

    with pytest.raises(ValueError, match="missing required fields"):
        validate_stage0_study_contract(payload)


def test_unknown_root_or_nested_fields_are_scope_drift() -> None:
    root_drift = _payload()
    root_drift["productionOptimizer"] = {"implementation": "requested"}
    with pytest.raises(ValueError, match="scope drift"):
        validate_stage0_study_contract(root_drift)

    nested_drift = _payload()
    event_config = _hardware(nested_drift, "event_camera")["configuration"]
    event_config["deployedDepthStream"] = True
    with pytest.raises(ValueError, match="scope drift"):
        validate_stage0_study_contract(nested_drift)


@pytest.mark.parametrize(
    "mutation",
    (
        "missing_event",
        "allowed_thermal",
        "hardware_thermal",
        "missing_depth_prohibition",
        "thermal_policy_weakened",
        "depth_deployed",
    ),
)
def test_scope_admits_exactly_four_classes_and_rejects_thermal_and_depth(
    mutation: str,
) -> None:
    payload = _payload()
    scope = payload["scope"]
    inventory = payload["hardwareInventory"]
    if mutation == "missing_event":
        scope["allowedSensorClasses"].remove("event_camera")
    elif mutation == "allowed_thermal":
        scope["allowedSensorClasses"].append("thermal")
    elif mutation == "hardware_thermal":
        inventory[0]["sensorClass"] = "thermal"
    elif mutation == "missing_depth_prohibition":
        scope["prohibitedDeployedModalities"].remove("depth")
    elif mutation == "thermal_policy_weakened":
        scope["thermalPolicy"] = "OPTIONAL"
    else:
        scope["depthPolicy"] = "DEPLOYED_DEPTH_ALLOWED"

    with pytest.raises((TypeError, ValueError)):
        validate_stage0_study_contract(payload)


@pytest.mark.parametrize(
    ("sensor_class", "field"),
    (
        ("radar", "frequencyBands"),
        ("passive_rf", "frequencyBands"),
        ("rgb", "lensOptions"),
        ("event_camera", "lensOptions"),
        ("event_camera", "temporalParameters"),
    ),
)
def test_band_lens_and_event_temporal_parameters_are_required(
    sensor_class: str, field: str
) -> None:
    payload = _payload()
    del _hardware(payload, sensor_class)["configuration"][field]

    with pytest.raises(ValueError, match="missing required fields"):
        validate_stage0_study_contract(payload)


def test_target_envelope_requires_emitting_and_silent_cases_and_known_rf_bands() -> (
    None
):
    payload = _payload()
    target = payload["targetAndTrajectoryEnvelope"]["targetClasses"][0]
    target["rfEmissionStates"] = ["EMITTING"]
    with pytest.raises(ValueError, match="must contain exactly"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    target = payload["targetAndTrajectoryEnvelope"]["targetClasses"][0]
    target["rfBandIds"].append("undeclared-band")
    with pytest.raises(ValueError, match="unknown passive-RF bands"):
        validate_stage0_study_contract(payload)


def test_train_validation_test_partitions_are_frozen_disjoint_and_hash_bound() -> None:
    payload = _payload()
    partitions = payload["scenarioCorpus"]["partitions"]
    partitions["test"]["scenarioIds"][0] = partitions["train"]["scenarioIds"][0]
    with pytest.raises(ValueError, match="must be disjoint"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    payload["scenarioCorpus"]["partitions"]["validation"]["frozen"] = False
    with pytest.raises(ValueError, match="must be True"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    payload["scenarioCorpus"]["partitions"]["test"][
        "scenarioSetArtifactId"
    ] = "artifact-train-scenarios"
    with pytest.raises(ValueError, match="must reference role"):
        validate_stage0_study_contract(payload)


def test_rgb_and_event_camera_are_one_correlated_visual_family() -> None:
    payload = _payload()
    payload["fusionPolicy"]["familyAssignments"]["event"] = ["event_camera"]
    with pytest.raises(ValueError, match="scope drift"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    payload["fusionPolicy"]["rgbEventCorrelationTreatment"] = "INDEPENDENT_FAMILIES"
    with pytest.raises(ValueError, match="CORRELATED_SINGLE_VISUAL_FAMILY"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    payload["fusionPolicy"]["sameFamilyEvidenceMayCorroborate"] = True
    with pytest.raises(ValueError, match="must be False"):
        validate_stage0_study_contract(payload)


def test_mount_region_permission_resource_and_privacy_checks_fail_closed() -> None:
    payload = _payload()
    region = payload["mountRegionPolicy"]["regions"][0]
    region["eligibleForCandidateGeneration"] = True
    with pytest.raises(ValueError, match="cannot be eligible"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    payload["mountRegionPolicy"]["regions"][0]["allowedSensorClasses"].append("thermal")
    with pytest.raises(ValueError, match="prohibited/unknown"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    del payload["mountRegionPolicy"]["regions"][0]["privacy"]
    with pytest.raises(ValueError, match="missing required fields"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    del payload["mountRegionPolicy"]["regions"][0]["resources"]["availablePowerWatts"]
    with pytest.raises(ValueError, match="missing required fields"):
        validate_stage0_study_contract(payload)


def test_hash_and_readiness_gates_cannot_claim_unverified_evidence() -> None:
    payload = _payload()
    payload["artifactBindings"][0]["sha256"] = "NOT_A_DIGEST"
    with pytest.raises(ValueError, match="SHA-256"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    environment = next(
        gate
        for gate in payload["readiness"]["gates"]
        if gate["gateId"] == "ENVIRONMENT"
    )
    environment["status"] = "PASSED"
    environment["blockingReasons"] = []
    with pytest.raises(ValueError, match="unverified evidence"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    payload["readiness"]["productionOptimizerImplementationGate"] = "AUTHORIZED"
    with pytest.raises(ValueError, match="must be 'BLOCKED'"):
        validate_stage0_study_contract(payload)


def test_verified_artifacts_are_mechanically_resolved_and_hashed(
    tmp_path: Path,
) -> None:
    payload = _payload()
    binding = _artifact(payload, "artifact-map")
    binding["readiness"] = "VERIFIED"
    binding["uri"] = "https://example.invalid/map.bin"
    binding["sha256"] = "0" * 64
    with pytest.raises(ValueError, match="must resolve to a local file"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    artifact_path = tmp_path / "map.bin"
    artifact_path.write_bytes(b"actual-map-bytes")
    binding = _artifact(payload, "artifact-map")
    binding["readiness"] = "VERIFIED"
    binding["uri"] = artifact_path.as_uri()
    binding["sha256"] = "0" * 64
    with pytest.raises(ValueError, match="does not match local artifact bytes"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    artifact_path = _materialize_verified_artifact(payload, "artifact-map", tmp_path)
    _artifact(payload, "artifact-map")["uri"] = artifact_path.name
    contract = validate_stage0_study_contract(payload, artifact_base_directory=tmp_path)
    assert contract.payload["artifactBindings"][0]["readiness"] == "VERIFIED"


def test_gate_evidence_must_exactly_match_bound_artifacts_and_roles(
    tmp_path: Path,
) -> None:
    payload = _payload()
    _materialize_verified_artifact(payload, "artifact-rgb-detector", tmp_path)
    environment = next(
        gate
        for gate in payload["readiness"]["gates"]
        if gate["gateId"] == "ENVIRONMENT"
    )
    environment["evidenceArtifactIds"] = ["artifact-rgb-detector"]
    environment["status"] = "PASSED"
    environment["blockingReasons"] = []

    with pytest.raises(ValueError, match="must exactly match"):
        validate_stage0_study_contract(payload)


def test_mount_eligibility_requires_verified_permission_and_privacy_evidence(
    tmp_path: Path,
) -> None:
    payload = _payload()
    _add_artifact(payload, "artifact-mount-permission-example", "MOUNT_PERMISSION")
    region = payload["mountRegionPolicy"]["regions"][0]
    region["permissions"].update(
        {
            "status": "APPROVED",
            "evidenceArtifactId": "artifact-mount-permission-example",
            "accessApproved": True,
            "structuralApproved": True,
            "radarEmissionApproved": True,
        }
    )
    region["privacy"]["reviewStatus"] = "APPROVED"
    region["eligibleForCandidateGeneration"] = True
    with pytest.raises(ValueError, match="must reference a VERIFIED artifact"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    _add_artifact(payload, "artifact-mount-permission-example", "MOUNT_PERMISSION")
    for artifact_id in (
        "artifact-mount-permission-example",
        "artifact-privacy-mask",
        "artifact-prohibited-view-geometry",
    ):
        _materialize_verified_artifact(payload, artifact_id, tmp_path)
    region = payload["mountRegionPolicy"]["regions"][0]
    region["permissions"].update(
        {
            "status": "APPROVED",
            "evidenceArtifactId": "artifact-mount-permission-example",
            "accessApproved": True,
            "structuralApproved": True,
            "radarEmissionApproved": True,
        }
    )
    region["privacy"]["reviewStatus"] = "APPROVED"
    region["eligibleForCandidateGeneration"] = True
    mount_gate = next(
        gate
        for gate in payload["readiness"]["gates"]
        if gate["gateId"] == "MOUNT_PERMISSIONS_AND_RESOURCES"
    )
    mount_gate["evidenceArtifactIds"].append("artifact-mount-permission-example")
    with pytest.raises(ValueError, match="require matching PASSED readiness gates"):
        validate_stage0_study_contract(payload)


def test_sequential_rl_experiment_requires_verified_environment_contract() -> None:
    payload = _payload()
    _add_artifact(payload, "artifact-rl-environment", "RL_ENVIRONMENT_CONTRACT")
    payload["rlGoNoGo"].update(
        {
            "decision": "GO_SEQUENTIAL_EXPERIMENT_ONLY",
            "problemClass": "SEQUENTIAL_DEPLOYMENT_OR_SENSOR_TASKING",
            "sequentialUseCaseId": "test-sequential-use-case",
            "environmentContractArtifactId": "artifact-rl-environment",
            "experimentAuthorized": True,
        }
    )

    with pytest.raises(ValueError, match="must reference a VERIFIED artifact"):
        validate_stage0_study_contract(payload)


def test_fusion_and_hard_constraint_family_minima_cannot_drift() -> None:
    payload = _payload()
    payload["requirements"]["hardConstraints"]["minimumIndependentEvidenceFamilies"] = 3
    payload["fusionPolicy"]["minimumIndependentFamilies"] = 1

    with pytest.raises(ValueError, match="must equal"):
        validate_stage0_study_contract(payload)


def test_critical_zone_ids_must_exist_in_hash_bound_catalog() -> None:
    payload = _payload()
    payload["requirements"]["criticalZoneIds"] = ["nonexistent-zone"]

    with pytest.raises(ValueError, match="absent from the hash-bound"):
        validate_stage0_study_contract(payload)


def test_equal_budget_benchmark_and_rl_no_go_are_bound_to_held_out_partitions() -> None:
    payload = _payload()
    payload["solverBenchmark"]["sameBudgetForAllTracks"] = False
    with pytest.raises(ValueError, match="must be True"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    payload["solverBenchmark"]["solverTracks"].pop()
    with pytest.raises(ValueError, match="exactly methods"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    payload["solverBenchmark"][
        "finalEvaluationPartitionId"
    ] = "scenario-partition-validation-v1"
    with pytest.raises(ValueError, match="scenario-partition-test-v1"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    payload["rlGoNoGo"]["experimentAuthorized"] = True
    with pytest.raises(ValueError, match="must be False"):
        validate_stage0_study_contract(payload)

    payload = _payload()
    payload["rlGoNoGo"]["deterministicBaselineSolverId"] = "greedy-baseline"
    with pytest.raises(ValueError, match="exact static solver"):
        validate_stage0_study_contract(payload)


def test_duplicate_json_keys_are_rejected(tmp_path: Path) -> None:
    duplicate = tmp_path / "duplicate.json"
    duplicate.write_text(
        '{"schemaVersion":"one","schemaVersion":"two"}', encoding="utf-8"
    )

    with pytest.raises(ValueError, match="duplicate JSON object key"):
        Stage0StudyContract.load(duplicate)


def test_cli_returns_nonzero_for_invalid_contract(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    invalid = deepcopy(_payload())
    del invalid["fusionPolicy"]
    path = tmp_path / "invalid.json"
    path.write_text(json.dumps(invalid), encoding="utf-8")

    assert validate_main([str(path)]) == 2
    assert "INVALID:" in capsys.readouterr().err
