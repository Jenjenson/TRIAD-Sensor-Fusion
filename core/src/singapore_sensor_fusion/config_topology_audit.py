"""Audit configured sensor topology against the currently exported snapshot.

The report separates three facts that must not be conflated:

* the topology configured for the next Unreal restart;
* the topology present in the current layered snapshot; and
* the sites for which clean learned-model acceptance evidence exists.

No online state is inferred from configuration. Importing this module has no
filesystem side effects.
"""

from __future__ import annotations

import argparse
from collections import Counter
from collections.abc import Mapping, Sequence
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
from typing import Any

from .paths import (
    DEFAULT_OUTPUT_DIRECTORY as PORTABLE_OUTPUT_DIRECTORY,
    DEFAULT_TRIAD_CONFIG,
    DEFAULT_TRIAD_SENSOR_SAVED_DIR,
)

AUDIT_SCHEMA = "triad.config_topology_audit.v1"
EXPECTED_MODALITIES = ("wideband_rf", "mmwave", "rgb", "event_camera")
DEFAULT_CONFIG = DEFAULT_TRIAD_CONFIG
DEFAULT_SNAPSHOT = DEFAULT_TRIAD_SENSOR_SAVED_DIR / "latest_layered_snapshot.json"
DEFAULT_OUTPUT_DIRECTORY = PORTABLE_OUTPUT_DIRECTORY
DEFAULT_JSON_NAME = "CONFIGURED_SENSOR_TOPOLOGY_AUDIT.json"
DEFAULT_MARKDOWN_NAME = "CONFIGURED_SENSOR_TOPOLOGY_AUDIT.md"
AUDIT_REFERENCE_TIME = datetime(2026, 8, 3, 10, 0, tzinfo=timezone.utc)
STALE_AFTER_SECONDS = 5.0

CLEAN_ACCEPTANCE_BY_NODE: dict[str, dict[str, Any]] = {
    "City_Sector": {
        "scenario": "City four-drone formation",
        "acceptedFrames": 48,
        "evaluatedFrames": 60,
    },
    "South_Sector": {
        "scenario": "South single fast target",
        "acceptedFrames": 37,
        "evaluatedFrames": 60,
    },
}


class TopologyAuditError(ValueError):
    """Raised when an audit input violates the expected JSON contract."""


def _read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise TopologyAuditError(f"cannot read {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise TopologyAuditError(f"{path} must contain a JSON object")
    return value


def _finite(value: Any, *, name: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise TopologyAuditError(f"{name} must be numeric")
    result = float(value)
    if not math.isfinite(result):
        raise TopologyAuditError(f"{name} must be finite")
    return result


def _parse_utc(value: Any, *, name: str) -> datetime:
    if not isinstance(value, str) or not value.strip():
        raise TopologyAuditError(f"{name} must be an ISO-8601 timestamp")
    normalized = value[:-1] + "+00:00" if value.endswith("Z") else value
    try:
        result = datetime.fromisoformat(normalized)
    except ValueError as exc:
        raise TopologyAuditError(f"{name} must be an ISO-8601 timestamp") from exc
    if result.tzinfo is None or result.utcoffset() is None:
        raise TopologyAuditError(f"{name} must include a UTC offset")
    return result.astimezone(timezone.utc)


def _acceptance(node_id: str) -> dict[str, Any]:
    evidence = CLEAN_ACCEPTANCE_BY_NODE.get(node_id)
    if evidence is None:
        return {
            "cleanLearnedModelAcceptanceEvidenced": False,
            "state": "NOT_EVIDENCED_AT_THIS_NODE",
            "semantics": "runtime capability is not an acceptance result",
        }
    return {
        "cleanLearnedModelAcceptanceEvidenced": True,
        "state": "OFFLINE_CLEAN_ACCEPTANCE_EVIDENCED",
        **evidence,
        "observedSimulationSlantRangeMetersAcrossAcceptedRuns": {
            "minimum": 15.8,
            "maximum": 339.0,
        },
        "fieldOrPhysicalRangeValidated": False,
    }


def _configured_nodes(config: Mapping[str, Any]) -> list[dict[str, Any]]:
    raw_nodes = config.get("SensorNodes")
    if not isinstance(raw_nodes, list):
        raise TopologyAuditError("SensorNodes must be an array")
    sample_cadence = _finite(config.get("SampleCadenceSeconds"), name="SampleCadenceSeconds")
    nodes: list[dict[str, Any]] = []
    seen: set[str] = set()
    for index, raw in enumerate(raw_nodes):
        if not isinstance(raw, Mapping) or raw.get("bEnabled") is not True:
            continue
        node_id = raw.get("NodeId")
        if not isinstance(node_id, str) or not node_id.strip():
            raise TopologyAuditError(f"SensorNodes[{index}].NodeId must be non-empty")
        node_id = node_id.strip()
        if node_id in seen:
            raise TopologyAuditError(f"duplicate enabled node: {node_id}")
        seen.add(node_id)
        camera_enabled = raw.get("bCaptureCameraFrames") is True
        modalities = list(EXPECTED_MODALITIES[:2])
        if camera_enabled:
            modalities.extend(EXPECTED_MODALITIES[2:])
        nodes.append(
            {
                "nodeId": node_id,
                "longitudeDegrees": _finite(
                    raw.get("LongitudeDegrees"), name=f"{node_id}.LongitudeDegrees"
                ),
                "latitudeDegrees": _finite(
                    raw.get("LatitudeDegrees"), name=f"{node_id}.LatitudeDegrees"
                ),
                "heightMeters": _finite(
                    raw.get("HeightMeters"), name=f"{node_id}.HeightMeters"
                ),
                "configuredModalities": modalities,
                "configuredSensorRecordCount": len(modalities),
                "layeredSampleCadenceSeconds": sample_cadence,
                "cameraCaptureConfigured": camera_enabled,
                "cameraCaptureCadenceSeconds": _finite(
                    raw.get("CameraCaptureCadenceSeconds"),
                    name=f"{node_id}.CameraCaptureCadenceSeconds",
                )
                if camera_enabled
                else None,
                "cameraCaptureWidthPixels": int(
                    _finite(raw.get("CameraCaptureWidth"), name=f"{node_id}.CameraCaptureWidth")
                )
                if camera_enabled
                else None,
                "cameraCaptureHeightPixels": int(
                    _finite(raw.get("CameraCaptureHeight"), name=f"{node_id}.CameraCaptureHeight")
                )
                if camera_enabled
                else None,
                "modelExecution": {
                    "state": "RUNTIME_CAPABLE_AFTER_RESTART"
                    if camera_enabled
                    else "NOT_CONFIGURED",
                    "runtimeCapable": camera_enabled,
                    "basis": "node is configured to export RGB, raw uint32-mm depth, and metadata consumed by node-agnostic RGB/event adapters",
                    "semantics": "configuration/runtime-path capability; not proof of execution, online state, or acceptance at this node",
                },
                "learnedModelAcceptance": _acceptance(node_id),
            }
        )
    return nodes


def _snapshot_inventory(
    snapshot: Mapping[str, Any] | None,
    *,
    as_of: datetime,
) -> dict[str, Any]:
    if snapshot is None:
        return {
            "available": False,
            "onlineStateInferred": False,
            "state": "NO_SNAPSHOT_SUPPLIED",
        }
    sensors = snapshot.get("sensorNodes")
    if not isinstance(sensors, list):
        raise TopologyAuditError("snapshot sensorNodes must be an array")
    rows = [row for row in sensors if isinstance(row, Mapping)]
    modalities = Counter(str(row.get("modality", "unknown")) for row in rows)
    node_ids = sorted(
        {
            str(row.get("nodeId"))
            for row in rows
            if isinstance(row.get("nodeId"), str) and str(row.get("nodeId")).strip()
        }
    )
    timestamp = _parse_utc(snapshot.get("timestampUtc"), name="snapshot.timestampUtc")
    source_timestamp = _parse_utc(
        snapshot.get("sourceUnrealSnapshotTimestampUtc"),
        name="snapshot.sourceUnrealSnapshotTimestampUtc",
    )
    age_seconds = max(
        0.0,
        (as_of - timestamp).total_seconds(),
        (as_of - source_timestamp).total_seconds(),
    )
    rgb_nodes = sorted(
        str(row.get("nodeId")) for row in rows if row.get("modality") == "rgb"
    )
    event_nodes = sorted(
        str(row.get("nodeId")) for row in rows if row.get("modality") == "event_camera"
    )
    is_legacy_20 = (
        len(node_ids) == 8
        and len(rows) == 20
        and modalities
        == Counter({"wideband_rf": 8, "mmwave": 8, "rgb": 2, "event_camera": 2})
    )
    return {
        "available": True,
        "schemaVersion": snapshot.get("schemaVersion"),
        "timestampUtc": timestamp.isoformat(timespec="milliseconds").replace("+00:00", "Z"),
        "sourceUnrealSnapshotTimestampUtc": source_timestamp.isoformat(timespec="milliseconds").replace(
            "+00:00", "Z"
        ),
        "ageAtAuditReferenceSeconds": round(age_seconds, 3),
        "staleAfterSeconds": STALE_AFTER_SECONDS,
        "staleAtAuditReference": age_seconds > STALE_AFTER_SECONDS,
        "state": "CURRENT_STALE_LEGACY_SNAPSHOT"
        if is_legacy_20 and age_seconds > STALE_AFTER_SECONDS
        else "SNAPSHOT_TOPOLOGY_OBSERVED",
        "siteCount": len(node_ids),
        "sensorRecordCount": len(rows),
        "modalityRecordCounts": {
            modality: modalities.get(modality, 0) for modality in EXPECTED_MODALITIES
        },
        "rgbNodeIds": rgb_nodes,
        "eventCameraNodeIds": event_nodes,
        "legacyEightSiteTwentyRecordTopology": is_legacy_20,
        "onlineStateInferred": False,
        "semantics": "historical exported inventory; sensor online state is not inferred by this audit",
    }


def _digest(value: Mapping[str, Any]) -> str:
    payload = json.dumps(value, sort_keys=True, separators=(",", ":"), allow_nan=False).encode(
        "utf-8"
    )
    return hashlib.sha256(payload).hexdigest()


def audit_topology(
    config: Mapping[str, Any],
    *,
    snapshot: Mapping[str, Any] | None = None,
    as_of: datetime = AUDIT_REFERENCE_TIME,
) -> dict[str, Any]:
    if config.get("bEnabled") is not True:
        raise TopologyAuditError("configuration must be enabled")
    if as_of.tzinfo is None or as_of.utcoffset() is None:
        raise TopologyAuditError("as_of must be timezone-aware")
    effective_as_of = as_of.astimezone(timezone.utc)
    nodes = _configured_nodes(config)
    modality_counts = Counter(
        modality for node in nodes for modality in node["configuredModalities"]
    )
    all_four = all(node["configuredModalities"] == list(EXPECTED_MODALITIES) for node in nodes)
    all_runtime_capable = all(node["modelExecution"]["runtimeCapable"] for node in nodes)
    accepted_node_ids = sorted(
        node["nodeId"]
        for node in nodes
        if node["learnedModelAcceptance"]["cleanLearnedModelAcceptanceEvidenced"]
    )
    snapshot_inventory = _snapshot_inventory(snapshot, as_of=effective_as_of)
    configured_records = sum(node["configuredSensorRecordCount"] for node in nodes)
    checks = [
        {
            "id": "configured.enabled_site_count",
            "status": "PASS" if len(nodes) == 8 else "FAIL",
            "observed": len(nodes),
            "expected": 8,
        },
        {
            "id": "configured.all_four_modalities_per_site",
            "status": "PASS" if all_four else "FAIL",
            "observed": sum(
                node["configuredModalities"] == list(EXPECTED_MODALITIES) for node in nodes
            ),
            "expected": len(nodes),
        },
        {
            "id": "configured.sensor_record_count",
            "status": "PASS" if configured_records == 32 else "FAIL",
            "observed": configured_records,
            "expected": 32,
        },
        {
            "id": "configured.learned_model_runtime_capability",
            "status": "PASS" if all_runtime_capable else "FAIL",
            "observed": sum(node["modelExecution"]["runtimeCapable"] for node in nodes),
            "expected": len(nodes),
        },
        {
            "id": "acceptance.evidenced_nodes",
            "status": "PASS" if accepted_node_ids == ["City_Sector", "South_Sector"] else "FAIL",
            "observed": accepted_node_ids,
            "expected": ["City_Sector", "South_Sector"],
        },
    ]
    report: dict[str, Any] = {
        "schemaVersion": AUDIT_SCHEMA,
        "auditReferenceTimeUtc": effective_as_of.isoformat(timespec="milliseconds").replace(
            "+00:00", "Z"
        ),
        "simulationOnly": True,
        "onlineStateInferred": False,
        "status": "PASS" if all(item["status"] == "PASS" for item in checks) else "FAIL",
        "configuredAfterRestart": {
            "restartRequiredToReplaceLegacyLoadedTopology": True,
            "siteCount": len(nodes),
            "sensorRecordCount": configured_records,
            "modalitiesPerSite": list(EXPECTED_MODALITIES),
            "modalityRecordCounts": {
                modality: modality_counts.get(modality, 0) for modality in EXPECTED_MODALITIES
            },
            "allEnabledNodesHaveAllFourModalities": all_four,
            "allNodesLearnedModelRuntimeCapable": all_runtime_capable,
            "nodes": nodes,
        },
        "cleanLearnedModelAcceptance": {
            "evidencedNodeIds": accepted_node_ids,
            "notEvidencedNodeIds": sorted(
                node["nodeId"] for node in nodes if node["nodeId"] not in accepted_node_ids
            ),
            "rgbAcceptanceOnly": True,
            "nativeEventCameraAcceptanceEvidenced": False,
            "observedSimulationSlantRangeMetersAcrossCityAndSouth": {
                "minimum": 15.8,
                "maximum": 339.0,
            },
            "fieldOrPhysicalRangeValidated": False,
        },
        "currentSnapshot": snapshot_inventory,
        "distinction": {
            "configuredTopology": "post-restart capability declaration; does not imply online state",
            "currentSnapshot": "currently exported historical inventory; it may remain stale until Unreal restarts and Play produces a fresh snapshot",
            "acceptanceEvidence": "clean learned-model results exist only for City and South; runtime capability at other nodes is not acceptance",
        },
        "checks": checks,
    }
    report["deterministicAuditSha256"] = _digest(report)
    return report


def render_markdown(report: Mapping[str, Any]) -> str:
    configured = report["configuredAfterRestart"]
    current = report["currentSnapshot"]
    current_counts = current.get("modalityRecordCounts", {})
    current_counts_text = " / ".join(
        f"{modality} {current_counts.get(modality, 0)}" for modality in EXPECTED_MODALITIES
    ) if isinstance(current_counts, Mapping) else "not available"
    lines = [
        "# TRIAD configured sensor topology audit",
        "",
        "This audit separates configured post-restart capability, the current exported snapshot, and learned-model acceptance evidence. Configuration does not imply that a sensor is online.",
        "",
        f"- Audit status: `{report['status']}`",
        f"- Deterministic audit SHA-256: `{report['deterministicAuditSha256']}`",
        f"- Configured after restart: `{configured['siteCount']} sites / {configured['sensorRecordCount']} sensor records`",
        "- Modalities at every configured site: `wideband RF / mmWave / RGB / event camera`",
        f"- Current exported inventory: `{current.get('siteCount', 0)} sites / {current.get('sensorRecordCount', 0)} sensor records`",
        f"- Current inventory state: `{current.get('state', 'NO_SNAPSHOT')}`",
        "",
        "## Configured after restart",
        "",
        "| Node | Coordinates / height | Layered cadence | Camera cadence | Configured modalities | Learned-model acceptance |",
        "|---|---|---:|---:|---|---|",
    ]
    for node in configured["nodes"]:
        acceptance = node["learnedModelAcceptance"]
        acceptance_text = (
            f"EVIDENCED: {acceptance['acceptedFrames']}/{acceptance['evaluatedFrames']} frames"
            if acceptance["cleanLearnedModelAcceptanceEvidenced"]
            else "NOT EVIDENCED AT THIS NODE"
        )
        lines.append(
            f"| {node['nodeId']} | {node['latitudeDegrees']:.4f}, {node['longitudeDegrees']:.4f} / {node['heightMeters']:.0f} m | "
            f"{node['layeredSampleCadenceSeconds']:.2f} s | {node['cameraCaptureCadenceSeconds']:.2f} s | "
            f"{' / '.join(node['configuredModalities'])} | {acceptance_text} |"
        )
    lines.extend(
        (
            "",
            "Every configured node is runtime-capable after restart because it exports the same RGB, raw uint32-mm depth, and metadata contract consumed by node-agnostic model adapters. This is capability, not proof that a model ran successfully or passed acceptance at that node.",
            "",
            "## Learned-model acceptance boundary",
            "",
            "- Clean RGB acceptance is evidenced only at City: 48/60 frames, and South: 37/60 frames.",
            "- Observed simulation slant across those two accepted runs was 15.8-339.0 m.",
            "- Field/physical RGB range remains unvalidated.",
            "- The event path remains an RGB-derived, non-physical, non-training-equivalent proxy. Candidate activations are inconclusive and native event-camera range is unvalidated.",
            "",
            "## Current snapshot comparison",
            "",
            f"The currently exported snapshot contains {current.get('siteCount', 0)} sites and {current.get('sensorRecordCount', 0)} sensor records: "
            f"{current_counts_text}. It is classified as `{current.get('state', 'NO_SNAPSHOT')}` at the frozen audit reference time.",
            "",
            "The legacy snapshot has RGB/event records only at City and South. It does not override the eight-site, 32-record configuration that takes effect after Unreal is restarted and Play produces a fresh export.",
            "",
            "No online state is inferred by this audit.",
            "",
            "## Checks",
            "",
        )
    )
    for check in report["checks"]:
        lines.append(
            f"- `{check['status']}` {check['id']}: observed `{check['observed']}`, expected `{check['expected']}`"
        )
    lines.append("")
    return "\n".join(lines)


def write_reports(
    report: Mapping[str, Any],
    *,
    output_directory: Path = DEFAULT_OUTPUT_DIRECTORY,
) -> tuple[Path, Path]:
    output_directory.mkdir(parents=True, exist_ok=True)
    json_path = output_directory / DEFAULT_JSON_NAME
    markdown_path = output_directory / DEFAULT_MARKDOWN_NAME
    json_temp = json_path.with_suffix(json_path.suffix + ".tmp")
    markdown_temp = markdown_path.with_suffix(markdown_path.suffix + ".tmp")
    json_temp.write_text(
        json.dumps(report, indent=2, sort_keys=True, allow_nan=False) + "\n",
        encoding="utf-8",
    )
    markdown_temp.write_text(render_markdown(report), encoding="utf-8")
    json_temp.replace(json_path)
    markdown_temp.replace(markdown_path)
    return json_path, markdown_path


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Audit TRIAD configured sensor topology")
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--snapshot", type=Path, default=DEFAULT_SNAPSHOT)
    parser.add_argument("--output-directory", type=Path, default=DEFAULT_OUTPUT_DIRECTORY)
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    config = _read_json(args.config)
    snapshot = _read_json(args.snapshot) if args.snapshot.exists() else None
    report = audit_topology(config, snapshot=snapshot)
    json_path, markdown_path = write_reports(report, output_directory=args.output_directory)
    print(
        json.dumps(
            {
                "status": report["status"],
                "json": str(json_path),
                "markdown": str(markdown_path),
                "configuredSites": report["configuredAfterRestart"]["siteCount"],
                "configuredSensors": report["configuredAfterRestart"]["sensorRecordCount"],
                "currentSnapshotSensors": report["currentSnapshot"].get("sensorRecordCount"),
                "deterministicAuditSha256": report["deterministicAuditSha256"],
            },
            separators=(",", ":"),
        )
    )
    return 0 if report["status"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
