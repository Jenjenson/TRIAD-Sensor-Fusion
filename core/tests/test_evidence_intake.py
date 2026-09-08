from __future__ import annotations

from copy import deepcopy
import hashlib
import json
import os
from pathlib import Path

import pytest

import singapore_sensor_fusion.placement.evidence_intake as evidence_intake_module

from singapore_sensor_fusion.placement.contracts import stable_digest
from singapore_sensor_fusion.placement.evidence_intake import (
    CANDIDATE_CATALOG_SCHEMA,
    EVALUATOR_EVIDENCE_SCHEMA,
    EVIDENCE_INTAKE_MANIFEST_SCHEMA,
    EVIDENCE_INTAKE_RECEIPT_SCHEMA,
    NATIVE_RF_AUTOMATION_FILTER,
    NATIVE_RF_RECEIPT_SCHEMA,
    SCENARIO_PARTITION_SCHEMA,
    TRAJECTORY_CORPUS_SCHEMA,
    evaluator_result_rows_sha256,
    validate_evidence_intake,
)
from singapore_sensor_fusion.placement.planning_pipeline import (
    CANDIDATE_TRAJECTORY_PRECOMPUTE_SCHEMA,
    EXACT_SIMULATOR_REPLAY_SOURCE,
    MEASURED_CALIBRATION_EVIDENCE,
    SENSOR_CLASSES,
    SENSOR_FAMILIES,
    STRICT_EVIDENCE_EXECUTION_CLASS,
    SensorModelCalibration,
    build_calibration_bundle,
    static_problem_from_precompute,
)
from singapore_sensor_fusion.placement.study_contract import Stage0StudyContract
from singapore_sensor_fusion.placement.validate_evidence_intake import (
    main as intake_main,
)


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
STAGE0_EXAMPLE = (
    REPOSITORY_ROOT
    / "core"
    / "examples"
    / "istana_stage0_sensor_placement_study_contract.v1.json"
)
REAL_NATIVE_RF_RECEIPT = Path(
    r"D:\triad\TRIAD\Saved\TRIAD\RFActualFileNativeTransactions"
    r"\rf_actual_fix_20260905T184017Z\receipt.json"
)


def _write_json(path: Path, payload: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, ensure_ascii=False, sort_keys=True, indent=2) + "\n",
        encoding="utf-8",
    )


def _write_digest_artifact(path: Path, payload: dict[str, object], field: str) -> None:
    payload[field] = stable_digest(payload)
    _write_json(path, payload)


def _file_identity(path: Path, root: Path) -> dict[str, object]:
    data = path.read_bytes()
    return {
        "relativePath": path.relative_to(root).as_posix(),
        "bytes": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
    }


def _artifact(payload: dict[str, object], artifact_id: str) -> dict[str, object]:
    return next(
        item
        for item in payload["artifactBindings"]
        if item["artifactId"] == artifact_id
    )


def _replace_artifact_file(
    payload: dict[str, object], artifact_id: str, path: Path, root: Path
) -> None:
    identity = _file_identity(path, root)
    binding = _artifact(payload, artifact_id)
    binding.update(
        {
            "uri": identity["relativePath"],
            "sha256": identity["sha256"],
            "readiness": "VERIFIED",
            "provenance": "Test-only file-backed evidence fixture.",
        }
    )


def _build_package(root: Path, *, include_second_radar: bool = False) -> dict[str, object]:
    root.mkdir(parents=True, exist_ok=True)
    artifacts_root = root / "artifacts"
    artifacts_root.mkdir()
    stage0 = json.loads(STAGE0_EXAMPLE.read_text(encoding="utf-8"))

    stage0["targetAndTrajectoryEnvelope"]["trajectoryCorpus"][
        "timeHorizonSeconds"
    ] = {"minimum": 0.02, "maximum": 0.02}
    stage0["targetAndTrajectoryEnvelope"]["trajectoryCorpus"][
        "samplePeriodSeconds"
    ] = 0.01
    for index, hardware in enumerate(stage0["hardwareInventory"]):
        hardware["manufacturer"] = f"Fixture Manufacturer {index}"
        hardware["model"] = f"Fixture Model {index}"
        hardware["hardwareRevision"] = "fixture-r1"

    second_radar_hardware_id: str | None = None
    second_radar_calibration_artifact_id: str | None = None
    if include_second_radar:
        first_radar = next(
            item
            for item in stage0["hardwareInventory"]
            if item["sensorClass"] == "radar"
        )
        second_radar = deepcopy(first_radar)
        second_radar_hardware_id = "hardware-radar-fixture-b"
        second_radar_calibration_artifact_id = "artifact-radar-calibration-fixture-b"
        second_radar.update(
            {
                "hardwareId": second_radar_hardware_id,
                "model": "Fixture Radar Model B",
                "hardwareRevision": "fixture-r2",
                "calibrationArtifactId": second_radar_calibration_artifact_id,
            }
        )
        stage0["hardwareInventory"].append(second_radar)
        stage0["artifactBindings"].append(
            {
                "artifactId": second_radar_calibration_artifact_id,
                "role": "SENSOR_CALIBRATION",
                "sha256": "0" * 64,
                "uri": "unresolved://test-only-second-radar-calibration",
                "provenance": "Test-only unresolved evidence fixture.",
                "readiness": "UNVERIFIED",
            }
        )
        radar_gate = next(
            gate
            for gate in stage0["readiness"]["gates"]
            if gate["gateId"] == "RADAR_MODEL"
        )
        radar_gate["evidenceArtifactIds"].append(
            second_radar_calibration_artifact_id
        )

    region = stage0["mountRegionPolicy"]["regions"][0]
    region["permissions"].update(
        {
            "siteAuthority": "fixture-authority",
            "status": "APPROVED",
            "evidenceArtifactId": "artifact-mount-permission-fixture",
            "accessApproved": True,
            "structuralApproved": True,
            "radarEmissionApproved": True,
        }
    )
    region["privacy"]["reviewStatus"] = "APPROVED"
    region["eligibleForCandidateGeneration"] = True

    hardware_by_class: dict[str, dict[str, object]] = {}
    for item in stage0["hardwareInventory"]:
        hardware_by_class.setdefault(item["sensorClass"], item)
    candidate_rows = [
        {
            "candidateId": "candidate-fixture-a",
            "siteId": "site-fixture-a",
            "mountRegionId": region["regionId"],
            "failureDomainId": region["resources"]["failureDomainId"],
            "poseEnuMeters": [0.0, 0.0, 10.0],
            "heightAglMeters": 10.0,
            "yawDegrees": 90.0,
            "sensorClasses": list(SENSOR_CLASSES),
            "hardwareIdsBySensorClass": {
                sensor_class: hardware_by_class[sensor_class]["hardwareId"]
                for sensor_class in SENSOR_CLASSES
            },
            "hardwareRevisionsBySensorClass": {
                sensor_class: hardware_by_class[sensor_class]["hardwareRevision"]
                for sensor_class in SENSOR_CLASSES
            },
            "costUnits": 28.0,
        }
    ]
    if include_second_radar:
        assert second_radar_hardware_id is not None
        second_radar = next(
            item
            for item in stage0["hardwareInventory"]
            if item["hardwareId"] == second_radar_hardware_id
        )
        candidate_rows.append(
            {
                "candidateId": "candidate-fixture-b",
                "siteId": "site-fixture-b",
                "mountRegionId": region["regionId"],
                "failureDomainId": region["resources"]["failureDomainId"],
                "poseEnuMeters": [1.0, 0.0, 10.0],
                "heightAglMeters": 10.0,
                "yawDegrees": 90.0,
                "sensorClasses": ["radar"],
                "hardwareIdsBySensorClass": {
                    "radar": second_radar_hardware_id
                },
                "hardwareRevisionsBySensorClass": {
                    "radar": second_radar["hardwareRevision"]
                },
                "costUnits": 9.0,
            }
        )
    candidate_path = artifacts_root / "candidate_catalog.json"
    _write_digest_artifact(
        candidate_path,
        {
            "schemaVersion": CANDIDATE_CATALOG_SCHEMA,
            "candidates": candidate_rows,
        },
        "artifactSha256",
    )

    scenario_paths: dict[str, Path] = {}
    scenario_to_partition: dict[str, str] = {}
    dimensions = stage0["scenarioCorpus"]["requiredDimensions"]
    for partition_name, declaration in stage0["scenarioCorpus"]["partitions"].items():
        partition_id = declaration["partitionId"]
        path = artifacts_root / f"{partition_name}_scenarios.json"
        _write_digest_artifact(
            path,
            {
                "schemaVersion": SCENARIO_PARTITION_SCHEMA,
                "partitionId": partition_id,
                "purpose": declaration["purpose"],
                "commonRandomSeeds": stage0["scenarioCorpus"]["commonRandomSeeds"],
                "scenarios": [
                    {
                        "scenarioId": scenario_id,
                        "dimensionValues": {
                            dimension: f"fixture-{dimension.lower()}"
                            for dimension in dimensions
                        },
                    }
                    for scenario_id in declaration["scenarioIds"]
                ],
            },
            "artifactSha256",
        )
        scenario_paths[partition_id] = path
        for scenario_id in declaration["scenarioIds"]:
            scenario_to_partition[scenario_id] = partition_id

    trajectory_declaration = stage0["targetAndTrajectoryEnvelope"]["trajectoryCorpus"]
    scenario_ids = [
        "train-clear-emitting",
        "validation-haze-emitting",
        "test-monsoon-emitting",
        "train-rain-silent",
    ]
    trajectory_rows: list[dict[str, object]] = []
    for index, trajectory_id in enumerate(trajectory_declaration["trajectoryIds"]):
        scenario_id = scenario_ids[index]
        trajectory_rows.append(
            {
                "trajectoryId": trajectory_id,
                "scenarioId": scenario_id,
                "partitionId": scenario_to_partition[scenario_id],
                "targetClassId": stage0["targetAndTrajectoryEnvelope"]["targetClasses"][0]["targetClassId"],
                "critical": trajectory_id in stage0["requirements"]["criticalTrajectoryIds"],
                "rfEmitting": not scenario_id.endswith("silent"),
                "points": [
                    {"timeSeconds": 0.0, "enuMeters": [100.0, 0.0, 40.0]},
                    {"timeSeconds": 0.01, "enuMeters": [99.0, 0.0, 40.0]},
                    {"timeSeconds": 0.02, "enuMeters": [98.0, 0.0, 40.0]},
                ],
            }
        )
    trajectory_path = artifacts_root / "trajectories.json"
    _write_digest_artifact(
        trajectory_path,
        {
            "schemaVersion": TRAJECTORY_CORPUS_SCHEMA,
            "coordinateFrame": trajectory_declaration["coordinateFrame"],
            "samplePeriodSeconds": 0.01,
            "trajectories": trajectory_rows,
        },
        "artifactSha256",
    )

    calibration_paths: dict[str, Path] = {}
    for sensor_class in SENSOR_CLASSES:
        path = artifacts_root / f"{sensor_class}_measured_dataset.bin"
        path.write_bytes(f"test-only measured fixture for {sensor_class}\n".encode())
        calibration_paths[sensor_class] = path
    second_radar_dataset_path: Path | None = None
    if include_second_radar:
        assert second_radar_hardware_id is not None
        second_radar_dataset_path = (
            artifacts_root / "radar_fixture_b_measured_dataset.bin"
        )
        second_radar_dataset_path.write_bytes(
            b"test-only measured fixture for second radar hardware\n"
        )

    rf_geometry_path = artifacts_root / "fixture.geometry.json"
    rf_material_path = artifacts_root / "fixture.catalog.json"
    rf_scene_path = artifacts_root / "fixture.contract.json"
    _write_json(rf_geometry_path, {"testFixture": "geometry"})
    _write_json(rf_material_path, {"testFixture": "material"})
    _write_json(rf_scene_path, {"testFixture": "scene"})

    native_receipt_path = root / "native_rf_receipt.json"
    workspace_resources = []
    for path in (rf_geometry_path, rf_material_path, rf_scene_path):
        identity = _file_identity(path, root)
        workspace_resources.append(
            {
                "Path": identity["relativePath"],
                "Present": True,
                "Bytes": identity["bytes"],
                "Sha256": str(identity["sha256"]).upper(),
            }
        )
    _write_json(
        native_receipt_path,
        {
            "Schema": NATIVE_RF_RECEIPT_SCHEMA,
            "Status": "PASS",
            "RunToken": "test-only-native-rf-run",
            "WorkspaceResources": workspace_resources,
            "Automation": {
                "Status": "PASS",
                "Filter": NATIVE_RF_AUTOMATION_FILTER,
                "ExitCode": 0,
                "ExactFilterSuccessfulCompletions": 1,
                "FailureCompletions": 0,
            },
            "ExactAutomationSuccessCount": 1,
            "NativeProjectIdleAfter": True,
            "FailureRollbackArmed": True,
        },
    )

    special_paths = {
        "artifact-solver-candidates": candidate_path,
        "artifact-target-trajectories": trajectory_path,
        "artifact-train-scenarios": scenario_paths["scenario-partition-train-v1"],
        "artifact-validation-scenarios": scenario_paths["scenario-partition-validation-v1"],
        "artifact-test-scenarios": scenario_paths["scenario-partition-test-v1"],
        "artifact-radar-calibration": calibration_paths["radar"],
        "artifact-passive-rf-calibration": calibration_paths["passive_rf"],
        "artifact-rgb-calibration": calibration_paths["rgb"],
        "artifact-event-camera-calibration": calibration_paths["event_camera"],
        "artifact-rf-geometry": rf_geometry_path,
        "artifact-rf-materials": rf_material_path,
        "artifact-rf-native-test-receipt": native_receipt_path,
    }
    if include_second_radar:
        assert second_radar_calibration_artifact_id is not None
        assert second_radar_dataset_path is not None
        special_paths[second_radar_calibration_artifact_id] = (
            second_radar_dataset_path
        )
    for binding in stage0["artifactBindings"]:
        artifact_id = binding["artifactId"]
        path = special_paths.get(artifact_id)
        if path is None:
            path = artifacts_root / f"{artifact_id}.bin"
            path.write_bytes(f"test-only evidence for {artifact_id}\n".encode())
        _replace_artifact_file(stage0, artifact_id, path, root)

    extra_bindings: list[dict[str, object]] = []
    extra_ids = ["artifact-mount-permission-fixture"] + [
        f"artifact-approval-{role.lower()}"
        for role in stage0["governance"]["requiredApproverRoles"]
    ]
    for artifact_id in extra_ids:
        path = artifacts_root / f"{artifact_id}.bin"
        path.write_bytes(f"test-only evidence for {artifact_id}\n".encode())
        identity = _file_identity(path, root)
        extra_bindings.append(
            {
                "artifactId": artifact_id,
                "role": (
                    "MOUNT_PERMISSION"
                    if artifact_id == "artifact-mount-permission-fixture"
                    else "APPROVAL_EVIDENCE"
                ),
                "sha256": identity["sha256"],
                "uri": identity["relativePath"],
                "provenance": "Test-only file-backed evidence fixture.",
                "readiness": "VERIFIED",
            }
        )
    stage0["artifactBindings"].extend(extra_bindings)

    for gate in stage0["readiness"]["gates"]:
        gate["status"] = "PASSED"
        gate["blockingReasons"] = []
        if gate["gateId"] == "MOUNT_PERMISSIONS_AND_RESOURCES":
            gate["evidenceArtifactIds"].append("artifact-mount-permission-fixture")
    stage0["contractState"] = "FROZEN"
    stage0["solverBenchmark"]["status"] = "FROZEN"
    stage0["governance"]["approvalState"] = "APPROVED"
    for approval in stage0["governance"]["approvals"]:
        approval.update(
            {
                "decision": "APPROVED",
                "reviewerId": f"fixture-reviewer-{approval['role'].lower()}",
                "decidedAtUtc": "2026-09-07T00:00:00Z",
                "evidenceArtifactId": f"artifact-approval-{approval['role'].lower()}",
            }
        )
    stage0_path = root / "stage0.json"
    _write_json(stage0_path, stage0)
    stage0_contract = Stage0StudyContract.load(stage0_path)

    hardware_by_id = {
        item["hardwareId"]: item for item in stage0["hardwareInventory"]
    }
    candidate_hardware_ids = sorted(
        {
            hardware_id
            for candidate in candidate_rows
            for hardware_id in candidate["hardwareIdsBySensorClass"].values()
        }
    )
    calibration_path_by_hardware = {}
    for hardware_id in candidate_hardware_ids:
        if hardware_id == second_radar_hardware_id:
            assert second_radar_dataset_path is not None
            calibration_path_by_hardware[hardware_id] = second_radar_dataset_path
        else:
            calibration_path_by_hardware[hardware_id] = calibration_paths[
                hardware_by_id[hardware_id]["sensorClass"]
            ]
    calibrations = []
    for hardware_id in candidate_hardware_ids:
        hardware = hardware_by_id[hardware_id]
        sensor_class = hardware["sensorClass"]
        calibrations.append(
            SensorModelCalibration(
                calibration_id=f"fixture-{hardware_id}-calibration-v1",
                sensor_class=sensor_class,
                evidence_class=MEASURED_CALIBRATION_EVIDENCE,
                maximum_range_meters=1000.0,
                minimum_quality=0.2,
                horizontal_fov_degrees=360.0,
                range_exponent=1.0,
                dataset_sha256=_file_identity(
                    calibration_path_by_hardware[hardware_id], root
                )["sha256"],
                limitations=("Test-only measured-data fixture; no field claim.",),
                hardware_id=hardware_id,
                hardware_revision=hardware["hardwareRevision"],
            )
        )
    calibration_bundle = build_calibration_bundle(calibrations)
    calibration_bundle_path = root / "calibration_bundle.json"
    _write_json(calibration_bundle_path, calibration_bundle)
    calibration_ids = {
        item.hardware_id: item.calibration_id for item in calibrations
    }

    scenario_hashes = {
        partition_id: _file_identity(path, root)["sha256"]
        for partition_id, path in sorted(scenario_paths.items())
    }
    result_rows_without_evaluator = []
    for candidate in candidate_rows:
        for trajectory in trajectory_rows:
            for sensor_class in sorted(candidate["sensorClasses"]):
                silent_passive = (
                    sensor_class == "passive_rf" and not trajectory["rfEmitting"]
                )
                hardware_id = candidate["hardwareIdsBySensorClass"][sensor_class]
                result_rows_without_evaluator.append(
                    {
                        "candidateId": candidate["candidateId"],
                        "trajectoryId": trajectory["trajectoryId"],
                        "scenarioId": trajectory["scenarioId"],
                        "partitionId": trajectory["partitionId"],
                        "sensorClass": sensor_class,
                        "evidenceFamily": SENSOR_FAMILIES[sensor_class],
                        "detectionFraction": 0.0 if silent_passive else 1.0,
                        "firstDetectionSeconds": None if silent_passive else 0.0,
                        "meanQuality": 0.0 if silent_passive else 0.75,
                        "sourceClass": EXACT_SIMULATOR_REPLAY_SOURCE,
                        "exactSimulatorReplay": True,
                        "measuredCalibration": True,
                        "rfGeometryQueried": sensor_class
                        in ("radar", "passive_rf"),
                        "hardwareId": hardware_id,
                        "hardwareRevision": candidate[
                            "hardwareRevisionsBySensorClass"
                        ][sensor_class],
                        "calibrationId": calibration_ids[hardware_id],
                    }
                )
    result_rows_without_evaluator.sort(
        key=lambda row: (
            row["candidateId"],
            row["trajectoryId"],
            row["sensorClass"],
        )
    )

    evaluation_paths: dict[str, Path] = {}
    for hardware_id in candidate_hardware_ids:
        hardware = hardware_by_id[hardware_id]
        sensor_class = hardware["sensorClass"]
        bound_rows = [
            row
            for row in result_rows_without_evaluator
            if row["hardwareId"] == hardware_id
        ]
        path = root / f"evaluation_{hardware_id}.json"
        _write_digest_artifact(
            path,
            {
                "schemaVersion": EVALUATOR_EVIDENCE_SCHEMA,
                "sensorClass": sensor_class,
                "hardwareId": hardware_id,
                "hardwareRevision": hardware["hardwareRevision"],
                "calibrationId": calibration_ids[hardware_id],
                "evidenceClass": EXACT_SIMULATOR_REPLAY_SOURCE,
                "stage0ContractSha256": stage0_contract.digest,
                "nativeRfReceiptFileSha256": _file_identity(native_receipt_path, root)["sha256"],
                "calibrationBundleFileSha256": _file_identity(calibration_bundle_path, root)["sha256"],
                "calibrationDatasetFileSha256": _file_identity(
                    calibration_path_by_hardware[hardware_id], root
                )["sha256"],
                "candidateCatalogFileSha256": _file_identity(candidate_path, root)["sha256"],
                "trajectoryCorpusFileSha256": _file_identity(trajectory_path, root)["sha256"],
                "scenarioPartitionFileSha256ById": scenario_hashes,
                "exactSimulatorReplayComplete": True,
                "resultRowCount": len(bound_rows),
                "resultRowsSha256": evaluator_result_rows_sha256(bound_rows),
                "fieldPerformanceClaimed": False,
                "physicalDeploymentAuthorized": False,
            },
            "artifactSha256",
        )
        evaluation_paths[hardware_id] = path

    result_rows = [
        {
            **row,
            "evaluatorEvidenceFileSha256": _file_identity(
                evaluation_paths[row["hardwareId"]], root
            )["sha256"],
        }
        for row in result_rows_without_evaluator
    ]

    precompute_bindings = {
        "stage0ContractSha256": stage0_contract.digest,
        "stage0ContractFileSha256": _file_identity(stage0_path, root)["sha256"],
        "nativeRfReceiptFileSha256": _file_identity(native_receipt_path, root)["sha256"],
        "nativeRfRunToken": "test-only-native-rf-run",
        "calibrationBundleFileSha256": _file_identity(calibration_bundle_path, root)["sha256"],
        "calibrationDatasetFileSha256ByHardwareId": {
            hardware_id: _file_identity(
                calibration_path_by_hardware[hardware_id], root
            )["sha256"]
            for hardware_id in candidate_hardware_ids
        },
        "candidateCatalogFileSha256": _file_identity(candidate_path, root)["sha256"],
        "trajectoryCorpusFileSha256": _file_identity(trajectory_path, root)["sha256"],
        "scenarioPartitionFileSha256ById": scenario_hashes,
        "evaluationEvidenceFileSha256ByHardwareId": {
            hardware_id: _file_identity(evaluation_paths[hardware_id], root)["sha256"]
            for hardware_id in candidate_hardware_ids
        },
    }
    precompute_path = root / "precompute.json"
    precompute = {
        "schemaVersion": CANDIDATE_TRAJECTORY_PRECOMPUTE_SCHEMA,
        "revision": 1,
        "executionClass": STRICT_EVIDENCE_EXECUTION_CLASS,
        "sourceClass": EXACT_SIMULATOR_REPLAY_SOURCE,
        "scoreSemantics": "MEASURED_CALIBRATION_WITHIN_DECLARED_LIMITS",
        "bindings": precompute_bindings,
        "candidates": candidate_rows,
        "trajectories": trajectory_rows,
        "results": result_rows,
        "allCandidateTrajectorySensorRowsPresent": True,
        "exactSimulatorReplayComplete": True,
        "measuredCalibrationUsed": True,
        "rfGeometryQueried": True,
        "fieldPerformanceClaimed": False,
        "physicalDeploymentAuthorized": False,
    }
    _write_digest_artifact(precompute_path, precompute, "artifactSha256")

    manifest_path = root / "intake_manifest.json"
    manifest = {
        "schemaVersion": EVIDENCE_INTAKE_MANIFEST_SCHEMA,
        "intakeId": "test-only-strict-intake",
        "executionClass": STRICT_EVIDENCE_EXECUTION_CLASS,
        "stage0Contract": _file_identity(stage0_path, root),
        "nativeRfReceipt": _file_identity(native_receipt_path, root),
        "calibrationBundle": _file_identity(calibration_bundle_path, root),
        "calibrationDatasets": [
            {
                "sensorClass": hardware_by_id[hardware_id]["sensorClass"],
                "hardwareId": hardware_id,
                "hardwareRevision": hardware_by_id[hardware_id]["hardwareRevision"],
                "stage0ArtifactId": hardware_by_id[hardware_id][
                    "calibrationArtifactId"
                ],
                "file": _file_identity(
                    calibration_path_by_hardware[hardware_id], root
                ),
            }
            for hardware_id in candidate_hardware_ids
        ],
        "candidateCatalog": _file_identity(candidate_path, root),
        "trajectoryCorpus": _file_identity(trajectory_path, root),
        "scenarioPartitions": [
            {
                "partitionId": declaration["partitionId"],
                "stage0ArtifactId": declaration["scenarioSetArtifactId"],
                "file": _file_identity(scenario_paths[declaration["partitionId"]], root),
            }
            for declaration in stage0["scenarioCorpus"]["partitions"].values()
        ],
        "evaluationEvidence": [
            {
                "sensorClass": hardware_by_id[hardware_id]["sensorClass"],
                "hardwareId": hardware_id,
                "hardwareRevision": hardware_by_id[hardware_id]["hardwareRevision"],
                "file": _file_identity(evaluation_paths[hardware_id], root),
            }
            for hardware_id in candidate_hardware_ids
        ],
        "precompute": _file_identity(precompute_path, root),
        "stage1AuthorizationPresented": False,
    }
    _write_json(manifest_path, manifest)
    return {
        "root": root,
        "manifestPath": manifest_path,
        "manifest": manifest,
        "precomputePath": precompute_path,
        "precompute": precompute,
        "nativeReceiptPath": native_receipt_path,
        "stage0Path": stage0_path,
        "evaluationPaths": evaluation_paths,
    }


def _rewrite_manifest(package: dict[str, object]) -> None:
    _write_json(package["manifestPath"], package["manifest"])


def _rewrite_precompute(package: dict[str, object]) -> None:
    precompute = package["precompute"]
    precompute.pop("artifactSha256", None)
    precompute["artifactSha256"] = stable_digest(precompute)
    _write_json(package["precomputePath"], precompute)
    package["manifest"]["precompute"] = _file_identity(
        package["precomputePath"], package["root"]
    )
    _rewrite_manifest(package)


def test_strict_file_backed_intake_accepts_complete_fixture_and_never_authorizes(
    tmp_path: Path,
) -> None:
    package = _build_package(tmp_path)

    receipt = validate_evidence_intake(package["manifestPath"], tmp_path)

    assert receipt["schemaVersion"] == EVIDENCE_INTAKE_RECEIPT_SCHEMA
    assert receipt["valid"] is True
    assert receipt["candidateCount"] == 1
    assert receipt["trajectoryCount"] == 4
    assert receipt["resultRowCount"] == 16
    assert receipt["validationScope"] == "STRUCTURAL_EVIDENCE_PACKAGE_INTAKE_ONLY"
    assert receipt["structuralEvidencePackageValid"] is True
    assert receipt["stage0OutcomeEvidenceComplete"] is False
    assert receipt["stage1OutcomeReviewReady"] is False
    assert receipt["staticSolverInputAuthorized"] is False
    assert receipt["stage1AuthorizationPresented"] is False
    assert receipt["solverExecutionAuthorized"] is False
    assert receipt["productionOptimizationAuthorized"] is False
    assert receipt["physicalDeploymentAuthorized"] is False
    assert receipt["fieldPerformanceClaimed"] is False

    with pytest.raises(ValueError, match="not a static-solver authorization"):
        static_problem_from_precompute(
            package["precompute"],
            minimum_detection_fraction=0.5,
            maximum_sites=1,
        )


def test_cli_prints_a_review_only_receipt(tmp_path: Path, capsys: pytest.CaptureFixture[str]) -> None:
    package = _build_package(tmp_path)

    assert intake_main([str(package["manifestPath"]), "--approved-root", str(tmp_path)]) == 0
    receipt = json.loads(capsys.readouterr().out)
    assert receipt["valid"] is True
    assert receipt["solverExecutionAuthorized"] is False


@pytest.mark.parametrize("bad_path", ("../outside.json", "C:/outside.json", "folder\\file.json", "%2e%2e/file.json"))
def test_manifest_file_paths_must_be_normalized_and_contained(
    tmp_path: Path, bad_path: str
) -> None:
    package = _build_package(tmp_path)
    package["manifest"]["precompute"]["relativePath"] = bad_path
    _rewrite_manifest(package)

    with pytest.raises(
        ValueError,
        match="normalized relative POSIX path|does not resolve|parent components",
    ):
        validate_evidence_intake(package["manifestPath"], tmp_path)


def test_missing_calibration_dataset_file_is_rejected(tmp_path: Path) -> None:
    package = _build_package(tmp_path)
    package["manifest"]["calibrationDatasets"][0]["file"]["relativePath"] = "missing.bin"
    _rewrite_manifest(package)

    with pytest.raises(ValueError, match="does not resolve|unable to inspect"):
        validate_evidence_intake(package["manifestPath"], tmp_path)


def test_binary_binding_is_streamed_without_retaining_file_bytes(tmp_path: Path) -> None:
    dataset = tmp_path / "large-dataset.bin"
    dataset.write_bytes(b"0123456789abcdef" * 131_072)
    binding = _file_identity(dataset, tmp_path)

    admitted = evidence_intake_module._resolve_binding(
        tmp_path,
        "streamedDataset",
        binding,
        json_document=False,
    )

    assert admitted.byte_length == 2 * 1024 * 1024
    assert admitted.data is None
    assert admitted.sha256 == binding["sha256"]


def test_same_root_symlink_is_rejected(tmp_path: Path) -> None:
    package = _build_package(tmp_path)
    link = tmp_path / "precompute-link.json"
    try:
        os.symlink(package["precomputePath"], link)
    except OSError as exc:
        pytest.skip(f"file symlinks unavailable on this host: {exc}")
    package["manifest"]["precompute"] = _file_identity(link, tmp_path)
    _rewrite_manifest(package)

    with pytest.raises(ValueError, match="symlink or reparse point"):
        validate_evidence_intake(package["manifestPath"], tmp_path)


def test_opened_handle_alias_is_rejected_as_toctou(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    target = tmp_path / "target.json"
    alias = tmp_path / "alias.json"
    target.write_text("{}", encoding="utf-8")
    alias.write_text("{}", encoding="utf-8")
    binding = _file_identity(target, tmp_path)
    monkeypatch.setattr(
        evidence_intake_module,
        "_open_handle_path",
        lambda stream: alias,
    )

    with pytest.raises(ValueError, match="opened through a symlink, junction, or alias"):
        evidence_intake_module._resolve_binding(
            tmp_path,
            "raceTarget",
            binding,
            json_document=True,
        )


def test_hardlink_aliases_cannot_satisfy_distinct_manifest_roles(
    tmp_path: Path,
) -> None:
    package = _build_package(tmp_path)
    first, second = package["manifest"]["evaluationEvidence"][:2]
    first_path = tmp_path / first["file"]["relativePath"]
    second_path = tmp_path / second["file"]["relativePath"]
    second_path.unlink()
    try:
        os.link(first_path, second_path)
    except OSError as exc:
        pytest.skip(f"hardlinks unavailable on this host: {exc}")
    second["file"] = _file_identity(second_path, tmp_path)
    _rewrite_manifest(package)

    with pytest.raises(ValueError, match="hardlink aliases are prohibited"):
        validate_evidence_intake(package["manifestPath"], tmp_path)


def test_hardlink_cannot_alias_unrelated_manifest_and_stage0_roles(
    tmp_path: Path,
) -> None:
    package = _build_package(tmp_path)
    bundle_path = tmp_path / package["manifest"]["calibrationBundle"]["relativePath"]
    alias_path = tmp_path / "artifacts" / "map-cross-role-alias.json"
    try:
        os.link(bundle_path, alias_path)
    except OSError as exc:
        pytest.skip(f"hardlinks unavailable on this host: {exc}")

    stage0 = json.loads(package["stage0Path"].read_text(encoding="utf-8"))
    _replace_artifact_file(stage0, "artifact-map", alias_path, tmp_path)
    _write_json(package["stage0Path"], stage0)
    stage0_contract = Stage0StudyContract.load(package["stage0Path"])
    stage0_file_identity = _file_identity(package["stage0Path"], tmp_path)
    package["manifest"]["stage0Contract"] = stage0_file_identity

    evaluation_hashes: dict[str, str] = {}
    for hardware_id, evaluation_path in package["evaluationPaths"].items():
        evaluation = json.loads(evaluation_path.read_text(encoding="utf-8"))
        evaluation["stage0ContractSha256"] = stage0_contract.digest
        evaluation.pop("artifactSha256")
        _write_digest_artifact(evaluation_path, evaluation, "artifactSha256")
        identity = _file_identity(evaluation_path, tmp_path)
        evaluation_hashes[hardware_id] = identity["sha256"]
        manifest_row = next(
            row
            for row in package["manifest"]["evaluationEvidence"]
            if row["hardwareId"] == hardware_id
        )
        manifest_row["file"] = identity

    package["precompute"]["bindings"][
        "stage0ContractSha256"
    ] = stage0_contract.digest
    package["precompute"]["bindings"][
        "stage0ContractFileSha256"
    ] = stage0_file_identity["sha256"]
    package["precompute"]["bindings"][
        "evaluationEvidenceFileSha256ByHardwareId"
    ] = evaluation_hashes
    for row in package["precompute"]["results"]:
        row["evaluatorEvidenceFileSha256"] = evaluation_hashes[row["hardwareId"]]
    _rewrite_precompute(package)

    with pytest.raises(
        ValueError,
        match="alias of unrelated Stage-0 artifact",
    ):
        validate_evidence_intake(package["manifestPath"], tmp_path)


def test_stage0_absolute_uri_is_rejected_before_parser_can_open_it(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    package = _build_package(tmp_path)
    outside = tmp_path.parent / f"{tmp_path.name}-outside.bin"
    outside.write_bytes(b"outside approved root")
    stage0 = json.loads(package["stage0Path"].read_text(encoding="utf-8"))
    binding = stage0["artifactBindings"][0]
    binding["uri"] = str(outside.resolve())
    binding["sha256"] = hashlib.sha256(outside.read_bytes()).hexdigest()
    _write_json(package["stage0Path"], stage0)
    package["manifest"]["stage0Contract"] = _file_identity(
        package["stage0Path"], tmp_path
    )
    _rewrite_manifest(package)
    original_open = Path.open
    outside_normalized = os.path.normcase(os.path.abspath(outside))

    def guarded_open(self: Path, *args: object, **kwargs: object):
        if os.path.normcase(os.path.abspath(self)) == outside_normalized:
            raise AssertionError("outside-root artifact was opened before path audit")
        return original_open(self, *args, **kwargs)

    monkeypatch.setattr(Path, "open", guarded_open)
    try:
        with pytest.raises(ValueError, match="normalized relative POSIX path"):
            validate_evidence_intake(package["manifestPath"], tmp_path)
    finally:
        outside.unlink(missing_ok=True)


def test_stripped_precompute_truth_fields_are_rejected_even_after_rehash(
    tmp_path: Path,
) -> None:
    package = _build_package(tmp_path)
    del package["precompute"]["bindings"]
    _rewrite_precompute(package)

    with pytest.raises(ValueError, match="truth envelope has scope drift"):
        validate_evidence_intake(package["manifestPath"], tmp_path)


def test_precompute_stage0_cross_binding_mismatch_is_rejected(tmp_path: Path) -> None:
    package = _build_package(tmp_path)
    package["precompute"]["bindings"]["stage0ContractSha256"] = "0" * 64
    _rewrite_precompute(package)

    with pytest.raises(ValueError, match="bindings do not exactly match"):
        validate_evidence_intake(package["manifestPath"], tmp_path)


def test_empty_strict_bindings_are_rejected_even_after_rehash(tmp_path: Path) -> None:
    package = _build_package(tmp_path)
    package["precompute"]["bindings"] = {}
    _rewrite_precompute(package)

    with pytest.raises(ValueError, match="strict precompute bindings have scope drift"):
        validate_evidence_intake(package["manifestPath"], tmp_path)


def test_stripped_candidate_authority_is_rejected_even_after_rehash(
    tmp_path: Path,
) -> None:
    package = _build_package(tmp_path)
    del package["precompute"]["candidates"][0][
        "hardwareRevisionsBySensorClass"
    ]
    _rewrite_precompute(package)

    with pytest.raises(ValueError, match=r"artifact\.candidates\[0\].*scope drift"):
        validate_evidence_intake(package["manifestPath"], tmp_path)


@pytest.mark.parametrize(
    ("field", "replacement"),
    (
        ("detectionFraction", 0.5),
        ("meanQuality", 0.5),
        ("firstDetectionSeconds", 0.01),
    ),
)
def test_rehashed_score_change_is_rejected_by_evaluator_output_digest(
    tmp_path: Path,
    field: str,
    replacement: object,
) -> None:
    package = _build_package(tmp_path)
    row = next(
        item
        for item in package["precompute"]["results"]
        if item["sensorClass"] == "radar"
    )
    row[field] = replacement
    _rewrite_precompute(package)

    with pytest.raises(ValueError, match="evaluator-bound output digest"):
        validate_evidence_intake(package["manifestPath"], tmp_path)


def test_missing_precompute_grid_row_is_rejected_after_rehash(tmp_path: Path) -> None:
    package = _build_package(tmp_path)
    package["precompute"]["results"].pop()
    _rewrite_precompute(package)

    with pytest.raises(ValueError, match="row grid mismatch"):
        validate_evidence_intake(package["manifestPath"], tmp_path)


def test_multiple_hardware_revisions_in_one_sensor_class_remain_distinct(
    tmp_path: Path,
) -> None:
    package = _build_package(tmp_path, include_second_radar=True)

    receipt = validate_evidence_intake(package["manifestPath"], tmp_path)

    assert receipt["candidateCount"] == 2
    assert receipt["resultRowCount"] == 20
    radar_rows = [
        row
        for row in package["precompute"]["results"]
        if row["sensorClass"] == "radar"
    ]
    assert len({row["hardwareId"] for row in radar_rows}) == 2
    assert len({row["hardwareRevision"] for row in radar_rows}) == 2
    assert len({row["calibrationId"] for row in radar_rows}) == 2
    assert len({row["evaluatorEvidenceFileSha256"] for row in radar_rows}) == 2


def test_wrong_row_evaluator_binding_is_rejected(tmp_path: Path) -> None:
    package = _build_package(tmp_path)
    package["precompute"]["results"][0]["evaluatorEvidenceFileSha256"] = "0" * 64
    _rewrite_precompute(package)

    with pytest.raises(ValueError, match="evaluatorEvidenceFileSha256"):
        validate_evidence_intake(package["manifestPath"], tmp_path)


def test_rf_silent_passive_row_cannot_report_detection(tmp_path: Path) -> None:
    package = _build_package(tmp_path)
    row = next(
        item
        for item in package["precompute"]["results"]
        if item["sensorClass"] == "passive_rf"
        and item["trajectoryId"] == "trajectory-ingress-west"
    )
    row["detectionFraction"] = 1.0
    row["firstDetectionSeconds"] = 0.0
    _rewrite_precompute(package)

    with pytest.raises(ValueError, match="RF-silent trajectories"):
        validate_evidence_intake(package["manifestPath"], tmp_path)


def test_forged_native_rf_pass_is_rejected(tmp_path: Path) -> None:
    package = _build_package(tmp_path)
    receipt = json.loads(package["nativeReceiptPath"].read_text(encoding="utf-8"))
    receipt["Automation"]["ExactFilterSuccessfulCompletions"] = 0
    _write_json(package["nativeReceiptPath"], receipt)
    package["manifest"]["nativeRfReceipt"] = _file_identity(
        package["nativeReceiptPath"], package["root"]
    )
    _rewrite_manifest(package)

    with pytest.raises(ValueError, match="Stage-0 artifact.*changed|authenticated"):
        validate_evidence_intake(package["manifestPath"], tmp_path)


@pytest.mark.skipif(
    not REAL_NATIVE_RF_RECEIPT.is_file(),
    reason="the exact workstation native RF v3 receipt is not present",
)
def test_exact_real_native_rf_v3_receipt_and_uppercase_hashes_validate() -> None:
    receipt_data = REAL_NATIVE_RF_RECEIPT.read_bytes()
    receipt_stat = REAL_NATIVE_RF_RECEIPT.stat()
    receipt_sha = hashlib.sha256(receipt_data).hexdigest()
    assert receipt_sha == (
        "1e7856c2435f6eebbcb362b4b09c4df19e8964544528f6204cc64a8a6690f0e2"
    )
    parsed = json.loads(receipt_data)
    assert all(
        row["Sha256"] == row["Sha256"].upper()
        for row in parsed["WorkspaceResources"]
    )
    receipt_file = evidence_intake_module.BoundFile(
        role="nativeRfReceipt",
        relative_path="receipt.json",
        resolved_path=REAL_NATIVE_RF_RECEIPT.resolve(),
        byte_length=len(receipt_data),
        sha256=receipt_sha,
        stable_identity=(
            "FILE_ID",
            str(receipt_stat.st_dev),
            str(receipt_stat.st_ino),
        ),
        data=receipt_data,
    )
    workspace_rows = parsed["WorkspaceResources"]
    geometry_path = Path(workspace_rows[0]["Path"]).resolve()
    material_path = Path(workspace_rows[1]["Path"]).resolve()

    def artifact_record(path: Path, role: str) -> dict[str, object]:
        path_stat = path.stat()
        return {
            "role": role,
            "path": path,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "bytes": path_stat.st_size,
            "stableIdentity": (
                "FILE_ID",
                str(path_stat.st_dev),
                str(path_stat.st_ino),
            ),
        }

    artifacts = {
        "artifact-rf-native-test-receipt": {
            "role": "RF_NATIVE_TEST_RECEIPT",
            "path": receipt_file.resolved_path,
            "sha256": receipt_file.sha256,
            "bytes": receipt_file.byte_length,
            "stableIdentity": receipt_file.stable_identity,
        },
        "artifact-rf-geometry": artifact_record(geometry_path, "RF_GEOMETRY"),
        "artifact-rf-materials": artifact_record(
            material_path, "RF_MATERIAL_CATALOG"
        ),
    }

    run_token = evidence_intake_module._validate_native_rf_receipt(
        REPOSITORY_ROOT,
        receipt_file,
        {},
        artifacts,
    )

    assert run_token == "rf_actual_fix_20260905T184017Z"


def test_demo_precompute_still_passes_its_explicit_truth_contract() -> None:
    from singapore_sensor_fusion.placement.demo_study import build_four_class_demo_study

    demo = build_four_class_demo_study(REPOSITORY_ROOT)["candidateTrajectoryPrecompute"]
    problem = static_problem_from_precompute(
        demo,
        minimum_detection_fraction=0.4,
        maximum_sites=4,
        minimum_evidence_families=2,
        minimum_site_redundancy=2,
        minimum_failure_domain_redundancy=2,
        minimum_scenario_coverage_fraction=1.0,
        budget_units=10.0,
    )
    assert len(problem.candidates) == 5


def test_low_level_solver_rejects_stripped_rehashed_demo_provenance() -> None:
    from singapore_sensor_fusion.placement.demo_study import build_four_class_demo_study

    precompute = deepcopy(
        build_four_class_demo_study(REPOSITORY_ROOT)["candidateTrajectoryPrecompute"]
    )
    del precompute["bindings"]
    precompute.pop("artifactSha256")
    precompute["artifactSha256"] = stable_digest(precompute)

    with pytest.raises(ValueError, match="truth envelope has scope drift"):
        static_problem_from_precompute(
            precompute,
            minimum_detection_fraction=0.4,
            maximum_sites=4,
        )
