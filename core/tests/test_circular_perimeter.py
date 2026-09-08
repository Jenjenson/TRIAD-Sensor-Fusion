from __future__ import annotations

import unittest

from singapore_sensor_fusion.approach_audit import (
    _perimeter_from_config,
    _perimeter_from_snapshot,
    signed_distance_to_perimeter_m,
)
from singapore_sensor_fusion.c3_bridge import _perimeter_from_live_snapshot
from singapore_sensor_fusion.geodesy import (
    ENU,
    Geodetic,
    enu_to_geodetic,
    wgs84_surface_distance_m,
)
from singapore_sensor_fusion.layered_runtime import _estimated_perimeter_state


CENTER = Geodetic(1.30709615, 103.84288055, 47.0)


def _live_circle() -> dict[str, object]:
    return {
        "enabled": True,
        "referenceName": "Istana_1km_Simulation_AOI",
        "geometryType": "wgs84_geodesic_circle",
        "shape": "Circle",
        "centerLongitudeDegrees": CENTER.longitude_deg,
        "centerLatitudeDegrees": CENTER.latitude_deg,
        "radiusMeters": 1000.0,
        "phaseRateDeadbandMetersPerSecond": 0.25,
        "boundaryInclusive": True,
        "legalOrNationalBoundary": False,
        "distanceMethod": "signed WGS84 Vincenty geodesic center distance minus radius",
    }


class CircularPerimeterTests(unittest.TestCase):
    def test_vincenty_surface_distance_matches_local_one_kilometre_fixture(self) -> None:
        east = enu_to_geodetic(ENU(1000.0, 0.0, 0.0), CENTER)
        self.assertAlmostEqual(wgs84_surface_distance_m(CENTER, east), 1000.0, delta=0.1)

    def test_layered_runtime_classifies_circle_inside_and_approaching(self) -> None:
        perimeter = _live_circle()
        inside = _estimated_perimeter_state(CENTER, 1.0, 0.0, perimeter)
        self.assertEqual(inside["state"], "INSIDE")
        self.assertFalse(inside["outside"])
        self.assertAlmostEqual(float(inside["distanceMeters"]), -1000.0, delta=1e-6)

        east_outside = enu_to_geodetic(ENU(1100.0, 0.0, 0.0), CENTER)
        approaching = _estimated_perimeter_state(east_outside, 10.0, 270.0, perimeter)
        self.assertEqual(approaching["state"], "APPROACHING")
        self.assertTrue(approaching["outside"])
        self.assertAlmostEqual(float(approaching["distanceMeters"]), 100.0, delta=0.15)
        self.assertAlmostEqual(float(approaching["approachRateMetersPerSecond"]), 10.0, delta=0.01)
        self.assertAlmostEqual(float(approaching["timeToPerimeterSeconds"]), 10.0, delta=0.02)

    def test_invalid_circle_fails_closed(self) -> None:
        invalid = _live_circle()
        invalid["radiusMeters"] = 0.0
        state = _estimated_perimeter_state(CENTER, 10.0, 0.0, invalid)
        self.assertEqual(state["state"], "UNAVAILABLE")
        self.assertIsNone(state["distanceMeters"])

    def test_approach_audit_loads_config_and_snapshot_circle(self) -> None:
        config = {
            "SimulationPerimeter": {
                "bEnabled": True,
                "Shape": "Circle",
                "CenterLongitudeDegrees": CENTER.longitude_deg,
                "CenterLatitudeDegrees": CENTER.latitude_deg,
                "RadiusMeters": 1000.0,
                "PhaseRateDeadbandMetersPerSecond": 0.25,
            }
        }
        from_config = _perimeter_from_config(config)
        from_snapshot = _perimeter_from_snapshot({"simulationPerimeter": _live_circle()})
        self.assertTrue(from_config.is_circle)
        self.assertTrue(from_snapshot.is_circle)
        east_outside = enu_to_geodetic(ENU(1100.0, 0.0, 0.0), CENTER)
        self.assertAlmostEqual(
            signed_distance_to_perimeter_m(
                east_outside.longitude_deg,
                east_outside.latitude_deg,
                from_snapshot,
            ),
            100.0,
            delta=0.15,
        )

    def test_c3_projection_preserves_circle_instead_of_inventing_bounds(self) -> None:
        projected = _perimeter_from_live_snapshot({"simulationPerimeter": _live_circle()})
        assert projected is not None
        self.assertEqual(projected["geometryType"], "wgs84_geodesic_circle")
        self.assertEqual(projected["radiusMeters"], 1000.0)
        self.assertNotIn("minLongitudeDegrees", projected)
        self.assertFalse(projected["legalOrNationalBoundary"])


if __name__ == "__main__":
    unittest.main()
