from __future__ import annotations

from datetime import datetime, timezone
import json
from pathlib import Path
import tempfile
import unittest

from singapore_sensor_fusion.config_topology_audit import (
    DEFAULT_CONFIG,
    audit_topology,
    render_markdown,
    write_reports,
)


def configured() -> dict:
    return json.loads(DEFAULT_CONFIG.read_text(encoding="utf-8"))


def legacy_snapshot() -> dict:
    nodes = [
        "West_Sector",
        "Jurong_Sector",
        "North_Sector",
        "NorthEast_Sector",
        "East_Sector",
        "Central_Sector",
        "City_Sector",
        "South_Sector",
    ]
    sensors = [
        {"nodeId": node_id, "modality": modality}
        for node_id in nodes
        for modality in ("wideband_rf", "mmwave")
    ]
    sensors.extend(
        {"nodeId": node_id, "modality": modality}
        for node_id in ("City_Sector", "South_Sector")
        for modality in ("rgb", "event_camera")
    )
    return {
        "schemaVersion": "triad.layered_detection_snapshot.v1",
        "timestampUtc": "2026-08-03T03:42:42.236Z",
        "sourceUnrealSnapshotTimestampUtc": "2026-08-03T03:42:42.236Z",
        "sensorNodes": sensors,
    }


class ConfigTopologyAuditTests(unittest.TestCase):
    def test_current_config_declares_eight_sites_and_four_modalities_each(self) -> None:
        report = audit_topology(configured())
        topology = report["configuredAfterRestart"]
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(topology["siteCount"], 8)
        self.assertEqual(topology["sensorRecordCount"], 32)
        self.assertEqual(
            topology["modalityRecordCounts"],
            {"wideband_rf": 8, "mmwave": 8, "rgb": 8, "event_camera": 8},
        )
        self.assertTrue(topology["allEnabledNodesHaveAllFourModalities"])
        self.assertTrue(topology["allNodesLearnedModelRuntimeCapable"])
        self.assertTrue(all(node["cameraCaptureConfigured"] for node in topology["nodes"]))

    def test_acceptance_is_evidenced_only_at_city_and_south(self) -> None:
        report = audit_topology(configured())
        acceptance = report["cleanLearnedModelAcceptance"]
        self.assertEqual(acceptance["evidencedNodeIds"], ["City_Sector", "South_Sector"])
        self.assertEqual(len(acceptance["notEvidencedNodeIds"]), 6)
        self.assertFalse(acceptance["nativeEventCameraAcceptanceEvidenced"])
        self.assertFalse(acceptance["fieldOrPhysicalRangeValidated"])

    def test_legacy_snapshot_inventory_is_separate_from_configured_topology(self) -> None:
        report = audit_topology(
            configured(),
            snapshot=legacy_snapshot(),
            as_of=datetime(2026, 8, 3, 10, 0, tzinfo=timezone.utc),
        )
        current = report["currentSnapshot"]
        self.assertEqual(current["state"], "CURRENT_STALE_LEGACY_SNAPSHOT")
        self.assertEqual(current["siteCount"], 8)
        self.assertEqual(current["sensorRecordCount"], 20)
        self.assertEqual(current["rgbNodeIds"], ["City_Sector", "South_Sector"])
        self.assertEqual(current["eventCameraNodeIds"], ["City_Sector", "South_Sector"])
        self.assertFalse(current["onlineStateInferred"])

    def test_report_is_deterministic_and_writes_json_and_markdown(self) -> None:
        first = audit_topology(configured(), snapshot=legacy_snapshot())
        second = audit_topology(configured(), snapshot=legacy_snapshot())
        self.assertEqual(first["deterministicAuditSha256"], second["deterministicAuditSha256"])
        markdown = render_markdown(first)
        self.assertIn("8 sites / 32 sensor records", markdown)
        self.assertIn("capability, not proof", markdown)
        with tempfile.TemporaryDirectory() as directory:
            json_path, markdown_path = write_reports(first, output_directory=Path(directory))
            reloaded = json.loads(json_path.read_text(encoding="utf-8"))
            rendered = markdown_path.read_text(encoding="utf-8")
        self.assertEqual(
            reloaded["deterministicAuditSha256"], first["deterministicAuditSha256"]
        )
        self.assertIn("CURRENT_STALE_LEGACY_SNAPSHOT", rendered)


if __name__ == "__main__":
    unittest.main()
