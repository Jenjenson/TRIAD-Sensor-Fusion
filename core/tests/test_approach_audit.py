from __future__ import annotations

import unittest

from singapore_sensor_fusion.approach_audit import (
    Perimeter,
    _corridor_transition_capture_complete,
    audit_config,
    audit_snapshot_series,
    run_audit,
    signed_distance_to_perimeter_m,
)


def _config() -> dict:
    return {
        "SimulationPerimeter": {
            "bEnabled": True,
            "MinimumLongitudeDegrees": 103.62,
            "MaximumLongitudeDegrees": 104.02,
            "MinimumLatitudeDegrees": 1.22,
            "MaximumLatitudeDegrees": 1.47,
            "PhaseRateDeadbandMetersPerSecond": 0.25,
        },
        "SensorNodes": [{
            "NodeId": "East_Sector",
            "LongitudeDegrees": 103.988,
            "LatitudeDegrees": 1.357,
            "HeightMeters": 145,
            "DetectionRangeMeters": 20_000,
            "bEnabled": True,
        }],
        "DemoTargets": [{
            "ActorName": "EastIngress",
            "SpawnCount": 4,
            "FormationColumns": 2,
            "FormationSpacingMeters": 30,
            "StartLongitudeDegrees": 104.033,
            "StartLatitudeDegrees": 1.352,
            "StartHeightMeters": 320,
            "Trajectory": "Linear",
            "LinearDirectionEnu": {"X": -1.0, "Y": 0.0, "Z": 0.0},
            "LinearDistanceMeters": 47_500,
            "LinearSpeedMetersPerSecond": 28,
            "bPingPongLinearPath": True,
            "bInboundApproachScenario": True,
            "IngressCorridorId": "EAST_INBOUND",
            "RFEmitter": {
                "bHostileScenarioTruth": True,
                "CenterFrequenciesGHz": [2.4, 5.8],
            },
            "bEnabled": True,
        }],
    }


def _snapshot(
    *,
    seconds: float,
    lon: float,
    state: str,
    outside: bool,
    schema: str = "triad.live_rf_snapshot.v1",
) -> dict:
    perimeter = Perimeter(103.62, 104.02, 1.22, 1.47)
    distance = signed_distance_to_perimeter_m(lon, 1.352, perimeter)
    target = {
        "targetActor": "EastIngress_01",
        "hostileScenarioTruth": True,
        "inboundApproachScenario": True,
        "kinematicsAvailable": True,
        "detectedThisSample": True,
        "detectedLinkCount": 1,
        "targetLongitudeDegrees": lon,
        "targetLatitudeDegrees": 1.352,
        "targetHeightMeters": 320,
        "airspaceState": state,
        "distanceToPerimeterMeters": distance,
        "approachRateMetersPerSecond": 28,
        "headingDegrees": 270,
        "speedMetersPerSecond": 28,
        "outsideSimulationPerimeter": outside,
        "ingressCorridorId": "EAST_INBOUND",
    }
    return {
        "schemaVersion": schema,
        "sampleComplete": True,
        "detectionOnly": True,
        "actionsTaken": "none",
        "simulationSeconds": seconds,
        "simulationPerimeter": {
            "geometryType": "axis_aligned_wgs84_rectangle",
            "minimumLongitudeDegrees": 103.62,
            "maximumLongitudeDegrees": 104.02,
            "minimumLatitudeDegrees": 1.22,
            "maximumLatitudeDegrees": 1.47,
            "boundaryInclusive": True,
            "legalOrNationalBoundary": False,
        },
        "sensorNodes": [{"nodeId": "East_Sector", "detectionRangeMeters": 20_000}],
        "scenarioTargets": [target],
        "tracks": [target],
        "detectedRFLinks": [{
            "nodeId": "East_Sector",
            "targetActor": "EastIngress_01",
            "slantRangeMeters": 5_000,
            "detected": True,
            **{key: target[key] for key in (
                "airspaceState",
                "distanceToPerimeterMeters",
                "approachRateMetersPerSecond",
                "headingDegrees",
                "speedMetersPerSecond",
                "outsideSimulationPerimeter",
                "ingressCorridorId",
            )},
        }],
    }


class ApproachAuditTests(unittest.TestCase):
    def test_signed_distance_is_positive_outside_zero_edge_negative_inside(self) -> None:
        perimeter = Perimeter(103.62, 104.02, 1.22, 1.47)
        self.assertGreater(signed_distance_to_perimeter_m(104.03, 1.35, perimeter), 1_000)
        self.assertAlmostEqual(signed_distance_to_perimeter_m(104.02, 1.35, perimeter), 0.0, places=6)
        self.assertLess(signed_distance_to_perimeter_m(104.0, 1.35, perimeter), -2_000)

    def test_static_route_proves_external_origin_inbound_crossing_and_range_envelope(self) -> None:
        audit = audit_config(_config())
        statuses = {item["id"]: item["status"] for item in audit["checks"]}
        self.assertEqual(audit["status"], "pass")
        self.assertEqual(statuses["config.external_origins"], "pass")
        self.assertEqual(statuses["config.inbound_motion"], "pass")
        self.assertEqual(statuses["config.boundary_crossing"], "pass")
        self.assertEqual(statuses["config.outside_detection_envelope"], "pass")
        route = audit["routes"][0]
        self.assertGreater(route["minimum_start_distance_to_perimeter_m"], 1_000)
        self.assertGreater(route["initial_approach_rate_mps"], 27)
        self.assertLess(route["estimated_entry_seconds"], 60)

    def test_runtime_distinguishes_outside_detection_from_inside_transition(self) -> None:
        outside = _snapshot(seconds=1, lon=104.03, state="APPROACHING", outside=True)
        inside = _snapshot(seconds=50, lon=104.019, state="INSIDE", outside=False)
        audit = audit_snapshot_series([outside, inside])
        statuses = {item["id"]: item["status"] for item in audit["checks"]}
        self.assertEqual(audit["status"], "pass")
        self.assertEqual(statuses["runtime.outside_rf_detection"], "pass")
        self.assertEqual(statuses["runtime.approaching_rf_detection"], "pass")
        self.assertEqual(statuses["runtime.distance_decreases"], "pass")
        self.assertEqual(statuses["runtime.outside_to_inside_transition"], "pass")
        self.assertEqual(statuses["runtime.corridor_transition_evidence"], "pass")
        milestone = audit["corridor_milestones"]["EAST_INBOUND"]
        self.assertEqual(milestone["firstOutsideDetected"]["airspaceState"], "APPROACHING")
        self.assertEqual(milestone["firstInsideAfterOutsideDetection"]["airspaceState"], "INSIDE")
        self.assertEqual(milestone["secondsFromOutsideDetectionToInside"], 49.0)

    def test_runtime_accepts_mixed_complete_v1_and_v2_samples(self) -> None:
        outside_v1 = _snapshot(seconds=1, lon=104.03, state="APPROACHING", outside=True)
        inside_v2 = _snapshot(
            seconds=50,
            lon=104.019,
            state="INSIDE",
            outside=False,
            schema="triad.live_rf_snapshot.v2",
        )
        audit = audit_snapshot_series([outside_v1, inside_v2])
        complete = next(item for item in audit["checks"] if item["id"] == "runtime.complete_samples")

        self.assertEqual(audit["status"], "pass")
        self.assertEqual(complete["status"], "pass")
        self.assertIn("v1/v2", complete["message"])

    def test_corridor_early_stop_requires_inside_after_outside_in_same_order(self) -> None:
        old_inside = _snapshot(seconds=300, lon=104.019, state="INSIDE", outside=False)
        new_outside = _snapshot(seconds=1, lon=104.03, state="APPROACHING", outside=True)
        new_inside = _snapshot(seconds=50, lon=104.019, state="INSIDE", outside=False)
        self.assertFalse(_corridor_transition_capture_complete([old_inside, new_outside]))
        self.assertTrue(_corridor_transition_capture_complete([new_outside, new_inside]))

    def test_range_ceiling_violation_fails_runtime_geometry(self) -> None:
        snapshot = _snapshot(seconds=1, lon=104.03, state="APPROACHING", outside=True)
        snapshot["detectedRFLinks"][0]["slantRangeMeters"] = 20_001
        audit = audit_snapshot_series([snapshot])
        statuses = {item["id"]: item["status"] for item in audit["checks"]}
        self.assertEqual(statuses["runtime.signed_distance"], "fail")
        self.assertEqual(audit["status"], "fail")

    def test_end_to_end_report_remains_detection_only(self) -> None:
        report = run_audit(
            _config(),
            [_snapshot(seconds=1, lon=104.03, state="APPROACHING", outside=True)],
        )
        self.assertTrue(report["scope"]["detection_only"])
        self.assertEqual(report["scope"]["engagement_actions"], "none")
        self.assertFalse(report["scope"]["scenario_truth_is_sensor_detection"])


if __name__ == "__main__":
    unittest.main()
