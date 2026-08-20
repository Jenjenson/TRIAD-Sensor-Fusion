import math
import unittest

from singapore_sensor_fusion.geodesy import (
    ECEF,
    ENU,
    Geodetic,
    WGS84_A_M,
    WGS84_B_M,
    ecef_to_geodetic,
    enu_to_ecef,
    enu_to_geodetic,
    geodetic_to_ecef,
    geodetic_to_enu,
)


class GeodesyReferenceTests(unittest.TestCase):
    def test_wgs84_equator_and_pole_reference_points(self) -> None:
        equator = geodetic_to_ecef(Geodetic(0.0, 0.0, 0.0))
        self.assertEqual(equator, ECEF(WGS84_A_M, 0.0, 0.0))

        north_pole = geodetic_to_ecef(Geodetic(90.0, 0.0, 0.0))
        self.assertAlmostEqual(north_pole.x_m, 0.0, delta=1e-8)
        self.assertAlmostEqual(north_pole.y_m, 0.0, delta=1e-8)
        self.assertAlmostEqual(north_pole.z_m, WGS84_B_M, delta=1e-8)

    def test_singapore_geodetic_ecef_round_trip(self) -> None:
        original = Geodetic(1.3521, 103.8198, 37.25)
        recovered = ecef_to_geodetic(geodetic_to_ecef(original))
        self.assertAlmostEqual(recovered.latitude_deg, original.latitude_deg, delta=1e-10)
        self.assertAlmostEqual(recovered.longitude_deg, original.longitude_deg, delta=1e-10)
        self.assertAlmostEqual(recovered.altitude_m, original.altitude_m, delta=1e-5)

    def test_local_enu_round_trip_at_singapore_origin(self) -> None:
        origin = Geodetic(1.290270, 103.851959, 15.0)
        local = ENU(1_234.5, -678.25, 85.0)
        ecef = enu_to_ecef(local, origin)
        recovered_local = geodetic_to_enu(ecef_to_geodetic(ecef), origin)
        self.assertAlmostEqual(recovered_local.east_m, local.east_m, delta=1e-5)
        self.assertAlmostEqual(recovered_local.north_m, local.north_m, delta=1e-5)
        self.assertAlmostEqual(recovered_local.up_m, local.up_m, delta=1e-5)

        recovered_geodetic = enu_to_geodetic(local, origin)
        recovered_ecef = geodetic_to_ecef(recovered_geodetic)
        ecef_error = math.sqrt(
            (recovered_ecef.x_m - ecef.x_m) ** 2
            + (recovered_ecef.y_m - ecef.y_m) ** 2
            + (recovered_ecef.z_m - ecef.z_m) ** 2
        )
        self.assertLess(ecef_error, 1e-5)

    def test_earth_centre_has_no_unique_geodetic_coordinate(self) -> None:
        with self.assertRaisesRegex(ValueError, "no unique"):
            ecef_to_geodetic(ECEF(0.0, 0.0, 0.0))


if __name__ == "__main__":
    unittest.main()
