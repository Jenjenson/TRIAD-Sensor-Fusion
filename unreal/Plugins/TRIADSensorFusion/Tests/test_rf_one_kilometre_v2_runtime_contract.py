import pathlib
import hashlib
import json
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
RUNTIME = ROOT / "Source" / "TRIADSensorFusion"
QUERY_H = (RUNTIME / "Public" / "TRIADRFIndexedGeometryQuery.h").read_text(encoding="utf-8")
QUERY_CPP = (RUNTIME / "Private" / "TRIADRFIndexedGeometryQuery.cpp").read_text(encoding="utf-8")
TYPES_H = (RUNTIME / "Public" / "TRIADSensorFusionTypes.h").read_text(encoding="utf-8")
MANAGER_H = (RUNTIME / "Public" / "TRIADSensorFusionScenarioManager.h").read_text(encoding="utf-8")
MANAGER_CPP = (RUNTIME / "Private" / "TRIADSensorFusionScenarioManager.cpp").read_text(encoding="utf-8")
NATIVE_TEST = (RUNTIME / "Private" / "Tests" / "TRIADRFIndexedGeometryQueryTests.cpp").read_text(encoding="utf-8")
GEODESY_CPP = (RUNTIME / "Private" / "TRIADGeodesy.cpp").read_text(encoding="utf-8")
GEODESY_TEST = (RUNTIME / "Private" / "Tests" / "TRIADGeodesyTests.cpp").read_text(encoding="utf-8")
EXAMPLE = ROOT / "Resources" / "IstanaOneKilometreV2RF.example.json"


class OneKilometreV2RuntimeContractTests(unittest.TestCase):
    def test_frozen_runtime_resources_and_example_are_exactly_hash_bound(self):
        config = json.loads(EXAMPLE.read_text(encoding="utf-8"))
        receipts = (
            ("RF/IstanaPublicViewRFOneKilometreV2.geometry.json", 12_506_346,
             "85e654fba602b2dbc51eb64c6b66ff234c8ea1f948152612c755edc22c65da51",
             "DedicatedRFExpectedGeometrySha256"),
            ("RF/istana_rf_materials_one_kilometre_v2.catalog.json", 28_674,
             "210cb26ddb9b531adeb3c917606a736aefc857eb6696da485a2e63dbb8b31662",
             "DedicatedRFExpectedMaterialCatalogSha256"),
            ("RF/istana_rf_scene_one_kilometre_v2.contract.json", 14_783,
             "fe509917ae59be0918bcd799f23dc981e00a394c6c328a7342f56b371c40bcc2",
             "DedicatedRFExpectedSceneContractSha256"),
        )
        for relative, expected_bytes, expected_hash, config_key in receipts:
            payload = (ROOT / "Resources" / relative).read_bytes()
            self.assertEqual(expected_bytes, len(payload))
            self.assertEqual(expected_hash, hashlib.sha256(payload).hexdigest())
            self.assertEqual(expected_hash, config[config_key])

    def test_tight_v1_defaults_and_paths_remain_unchanged(self):
        self.assertIn("IstanaPublicViewRFTightV1.geometry.json", TYPES_H)
        self.assertIn("a3705e22b47fbe4fd38936294890e1ad620bb3c618ac3c93ca6c4719c6edc62a", TYPES_H)
        self.assertIn("if (!Loaded.bHasClosedWgs84GeodesicCircleStudyDomain)", QUERY_CPP)

    def test_loader_retains_strict_circle_frame_and_truth_contract(self):
        for marker in (
            "CLOSED_WGS84_GEODESIC_CIRCLE",
            "CLOCKWISE_FROM_TRUE_NORTH",
            "EPSG:4326",
            "containedByLoaderCoverageAabb",
            "coversEntireRadiusCircle",
            "cornersOutsideCircleExcludedFromStudy",
            "StudyDomainPerimeterSampleCount",
        ):
            self.assertIn(marker, QUERY_H + QUERY_CPP)

    def test_scenario_hash_binding_includes_live_perimeter_and_georeference(self):
        for marker in (
            "DedicatedRFExpectedStudyDomainId",
            "DedicatedRFExpectedStudyCenterLongitudeDegrees",
            "DedicatedRFExpectedStudyRadiusMeters",
            "DedicatedRFExpectedStudyPerimeterSampleCount",
            "GetOriginLongitudeLatitudeHeight",
            "GeoreferenceCount != 1",
            "EOriginPlacement::CartographicOrigin",
            "GetActorTransform().Equals(FTransform::Identity",
            "DedicatedRFExpectedGeoreferenceOriginHeightMeters",
            "DedicatedRFExpectedGeoreferenceScaleCentimetersPerMeter",
            "Perimeter.ReferenceName != Metadata.StudyDomainId",
            "Perimeter.RadiusMeters != Metadata.StudyDomainRadiusMeters",
        ):
            self.assertIn(marker, TYPES_H + MANAGER_CPP)

    def test_last_mile_gate_has_no_direct_or_legacy_fallback(self):
        evaluate_start = MANAGER_CPP.index("bool ATRIADSensorFusionScenarioManager::EvaluateDedicatedRFPath")
        evaluate_end = MANAGER_CPP.index("void ATRIADSensorFusionScenarioManager::AddRFPropagationTelemetryFields")
        evaluate = MANAGER_CPP[evaluate_start:evaluate_end]
        self.assertLess(evaluate.index("TransformUnrealPositionToLongitudeLatitudeHeight"),
                        evaluate.index("BuildPathCandidates"))
        self.assertIn("IsFiniteSegmentWithinAdmittedStudyDomain", evaluate)
        self.assertIn("no direct or legacy fallback is allowed", evaluate)
        self.assertNotIn("ComputeFreeSpacePathLossDb", evaluate)
        self.assertIn("Wgs84ToSvy21Meters", evaluate)
        self.assertIn("GeometryTransmitterCentimeters", evaluate)
        self.assertNotIn("BuildPathCandidates(\n            TransmitterWorldCentimeters", evaluate)
        self.assertIn("OutStartSignedDistanceMeters > 0.0", QUERY_CPP)
        self.assertIn("OutEndSignedDistanceMeters > 0.0", QUERY_CPP)

    def test_telemetry_is_bounded_and_native_edge_cases_are_present(self):
        for marker in (
            "rfAoiAdmissionRequired",
            "rfAoiEndpointsAdmitted",
            "rfAoiAdmissionDomainId",
            "rfTransmitterAoiSignedDistanceMeters",
            "rfReceiverAoiSignedDistanceMeters",
            "AoiAdmissionDomainId = DedicatedRFMetadata.StudyDomainId.Left(256)",
            "AdmissionError.Left(512)",
        ):
            self.assertIn(marker, MANAGER_H + MANAGER_CPP)
        for marker in (
            "closed boundary is admitted",
            "epsilon outside fails closed",
            "Non-finite georeference transform evidence fails closed",
            "Vertical AABB overflow fails before circle admission",
        ):
            self.assertIn(marker, NATIVE_TEST)

    def test_epsg3414_projection_is_pinned_and_fixture_tested(self):
        for marker in (
            "constexpr double K0 = 1.0",
            "FalseEasting = 28001.642",
            "FalseNorthing = 38744.572",
            "1.3666666666666667",
            "Wgs84ToSvy21Meters",
            "29064.15860639389",
            "30064.158637627355",
            "28357.049127599217",
            "0.0005",
        ):
            self.assertIn(marker, GEODESY_CPP + GEODESY_TEST)
        for marker in (
            "DedicatedRFExpectedProjectionId",
            "DedicatedRFExpectedProjectedOriginEastingMeters",
            "DedicatedRFExpectedProjectedOriginNorthingMeters",
            "X_EASTING_DELTA_Y_NEGATED_NORTHING_DELTA",
            "ELLIPSOID_HEIGHT_MINUS_PINNED_ORIGIN_HEIGHT",
            "rfGeometryEndpointFrame",
        ):
            self.assertIn(marker, TYPES_H + MANAGER_CPP)


if __name__ == "__main__":
    unittest.main()
