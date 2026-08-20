"""Engine-independent contract tests for simulated search radar and radar-cued PTZ.

These checks run while Unreal Editor is open. The Unreal automation test remains
the exact C++ math check once a coordinated editor build is available.
"""

from __future__ import annotations

import json
import math
import unittest
from pathlib import Path


PLUGIN_ROOT = Path(__file__).resolve().parents[1]
PROJECT_ROOT = PLUGIN_ROOT.parents[1]
SOURCE_ROOT = PLUGIN_ROOT / "Source" / "TRIADSensorFusion"
MANAGER_CPP = (SOURCE_ROOT / "Private" / "TRIADSensorFusionScenarioManager.cpp").read_text(encoding="utf-8")
NODE_CPP = (SOURCE_ROOT / "Private" / "TRIADSensorNodeActor.cpp").read_text(encoding="utf-8")
NODE_H = (SOURCE_ROOT / "Public" / "TRIADSensorNodeActor.h").read_text(encoding="utf-8")
MODEL_CPP = (SOURCE_ROOT / "Private" / "TRIADLongRangeSensorModel.cpp").read_text(encoding="utf-8")
DEMO_CPP = (SOURCE_ROOT / "Private" / "TRIADDemoDroneActor.cpp").read_text(encoding="utf-8")
MODEL_TEST_CPP = (
    SOURCE_ROOT / "Private" / "Tests" / "TRIADLongRangeSensorModelTests.cpp"
).read_text(encoding="utf-8")
TYPES_H = (SOURCE_ROOT / "Public" / "TRIADSensorFusionTypes.h").read_text(encoding="utf-8")
CONFIG = json.loads((PROJECT_ROOT / "Config" / "SingaporeSensorFusion.json").read_text(encoding="utf-8"))


def function_body(source: str, signature: str, next_signature: str) -> str:
    start = source.index(signature)
    end = source.index(next_signature, start)
    return source[start:end]


class LongRangeRadarPtzContractTests(unittest.TestCase):
    def test_every_active_node_has_explicit_long_range_stack(self) -> None:
        nodes = CONFIG["SensorNodes"]
        self.assertGreaterEqual(len(nodes), 1)
        for node in nodes:
            with self.subTest(node=node["NodeId"]):
                self.assertTrue(node["bEnableSearchRadar"])
                self.assertGreaterEqual(node["SearchRadarRangeMeters"], 5000.0)
                self.assertTrue(node["bEnableEOPTZ"])
                self.assertGreaterEqual(node["EOPTZConfirmationRangeMeters"], 500.0)
                self.assertLessEqual(node["EOPTZFieldOfViewDegrees"], 12.0)
                self.assertTrue(node["bEnableThermalPTZ"])
                self.assertGreaterEqual(node["ThermalPTZConfirmationRangeMeters"], 500.0)
                self.assertTrue(node["bPTZTraceComplex"])
                self.assertTrue(node["bVisualizeLongRangeSensorCue"])

    def test_search_radar_is_sampled_before_rf_emitter_gate(self) -> None:
        sample = function_body(
            MANAGER_CPP,
            "void ATRIADSensorFusionScenarioManager::SampleScenarioNow()",
            "double ATRIADSensorFusionScenarioManager::ComputeSignedDistanceToSimulationPerimeterMeters(",
        )
        radar_index = sample.index("SampleNodeTargetSearchRadar(")
        emitter_gate_index = sample.index("if (Emitter)", radar_index)
        self.assertLess(radar_index, emitter_gate_index)
        radar_function = function_body(
            MANAGER_CPP,
            "bool ATRIADSensorFusionScenarioManager::SampleNodeTargetSearchRadar(",
            "bool ATRIADSensorFusionScenarioManager::ComputeLineOfSight(",
        )
        self.assertNotIn("UTRIADRFEmitterComponent", radar_function)
        self.assertNotIn("CenterFrequenciesGHz", radar_function)
        self.assertIn("RadarCrossSectionSquareMeters", radar_function)
        self.assertIn("bLineOfSight", radar_function)
        self.assertIn("WeatherVisibilityMeters", radar_function)
        self.assertIn("MeasuredRadialVelocityMetersPerSecond", radar_function)

    def test_v3_snapshot_is_additive_and_bounded(self) -> None:
        self.assertIn('TEXT("triad.live_rf_snapshot.v3")', MANAGER_CPP)
        self.assertIn('SetArrayField(TEXT("detectedRFLinks"), LinkValues)', MANAGER_CPP)
        self.assertIn('SetArrayField(TEXT("tracks"), TrackValues)', MANAGER_CPP)
        self.assertIn('SetArrayField(TEXT("scenarioTargets"), ScenarioTargetValues)', MANAGER_CPP)
        self.assertIn('SetArrayField(TEXT("searchRadarDetections"), RadarDetectionValues)', MANAGER_CPP)
        self.assertIn('SetArrayField(TEXT("ptzConfirmations"), PTZConfirmationValues)', MANAGER_CPP)
        self.assertLessEqual(CONFIG["MaxLiveSnapshotSearchRadarDetections"], 4096)
        self.assertLessEqual(CONFIG["MaxLiveSnapshotPTZConfirmations"], 256)

    def test_ptz_has_real_slew_settle_and_radar_cue_linkage(self) -> None:
        surface = NODE_H + "\n" + NODE_CPP
        for state in ("SLEWING", "SETTLING", "SETTLED"):
            self.assertIn(f'TEXT("{state}")', surface)
        self.assertIn("RInterpConstantTo", NODE_CPP)
        self.assertIn("PTZSettleSeconds", NODE_CPP)
        self.assertIn("RadarCueSensorId", NODE_CPP)
        self.assertIn("CueLongRangeSensors", MANAGER_CPP)

    def test_continuous_radar_cue_freshness_is_not_acquisition_age(self) -> None:
        cue = function_body(
            NODE_CPP,
            "void ATRIADSensorNodeActor::CueLongRangeSensors(",
            "void ATRIADSensorNodeActor::ClearLongRangeCue()",
        )
        exported = function_body(
            NODE_CPP,
            "void ATRIADSensorNodeActor::GetLatestPTZConfirmations(",
            "void ATRIADSensorNodeActor::UpdateLongRangePTZ(",
        )
        self.assertIn("LastRadarCueSimulationSeconds = SimulationSeconds", cue)
        self.assertIn(
            "SimulationSeconds - LastRadarCueSimulationSeconds", cue
        )
        self.assertIn("NowSeconds - LastRadarCueSimulationSeconds", exported)
        self.assertNotIn("NowSeconds - RadarCueStartSimulationSeconds", exported)
        self.assertIn("RadarCueStartSimulationSeconds = SimulationSeconds", cue)

    def test_moving_target_cue_geometry_refreshes_between_ptz_frames(self) -> None:
        cue = function_body(
            NODE_CPP,
            "void ATRIADSensorNodeActor::CueLongRangeSensors(",
            "void ATRIADSensorNodeActor::ClearLongRangeCue()",
        )
        refresh_loop = cue[cue.index("for (FTRIADPTZConfirmationResult& Pending") :]
        self.assertNotIn("continue;", refresh_loop)
        for field in (
            "Pending.RangeMeters = CurrentRadarMeasuredRangeMeters",
            "Pending.AzimuthDegrees = CurrentRadarMeasuredBearingDegrees",
            "Pending.ElevationDegrees = CurrentRadarMeasuredElevationDegrees",
        ):
            self.assertIn(field, refresh_loop)

    def test_unreal_and_fusion_require_two_dimensional_pixel_support(self) -> None:
        capture = function_body(
            NODE_CPP,
            "void ATRIADSensorNodeActor::CaptureLongRangeConfirmation()",
            "bool ATRIADSensorNodeActor::PublishLongRangeFrame(",
        )
        self.assertIn(
            "FMath::Min(Result.PixelExtentWidth, Result.PixelExtentHeight) >= 2.0",
            capture,
        )

    def test_symmetric_swarm_cue_selection_uses_stable_track_id(self) -> None:
        self.assertIn("bStableEqualConfidenceTieBreak", MANAGER_CPP)
        self.assertIn(
            "RadarTrackId.Compare(SelectedRadarTrackId, ESearchCase::CaseSensitive) < 0",
            MANAGER_CPP,
        )
        self.assertNotIn("RadarRangeMeters < SelectedRadarRangeMeters", MANAGER_CPP)

    def test_demo_drone_visual_mesh_is_normalized_to_one_meter(self) -> None:
        self.assertIn("DemoDroneMaximumVisualDimensionCentimeters = 100.0", DEMO_CPP)
        self.assertIn("GetStaticMesh()->GetBoundingBox().GetSize()", DEMO_CPP)
        self.assertIn("NormalizeDemoDroneVisualScale(VisualMesh)", DEMO_CPP)
        self.assertIn("VisualMesh->SetRelativeScale3D(FVector(UniformScale))", DEMO_CPP)
        self.assertIn(
            "VisualMesh->SetRelativeRotation(FRotator(0.0, 0.0, 90.0))",
            DEMO_CPP,
        )
        raw_x, raw_y, raw_z = 1589.531982, 299.100220, 1336.495239
        scale = 100.0 / max(raw_x, raw_y, raw_z)
        horizontal_x = raw_x * scale
        horizontal_y = raw_z * scale
        vertical_z = raw_y * scale
        self.assertGreaterEqual(min(horizontal_x, horizontal_y), 80.0)
        self.assertLessEqual(vertical_z, 20.0)

    def test_ptz_recues_and_blocks_capture_when_off_boresight(self) -> None:
        update = function_body(
            NODE_CPP,
            "void ATRIADSensorNodeActor::UpdateLongRangePTZ(",
            "void ATRIADSensorNodeActor::InitializeLongRangeCaptureTargets()",
        )
        self.assertIn("PointingErrorDegrees > PTZPointingToleranceDegrees", update)
        self.assertIn("PTZSlewState = EPTZSlewState::Slewing", update)
        capture_gate = update.index(
            "PointingErrorDegrees <= PTZPointingToleranceDegrees &&"
        )
        capture_call = update.index("CaptureLongRangeConfirmation()")
        self.assertLess(capture_gate, capture_call)

    def test_far_radar_cues_do_not_trigger_costly_ptz_capture(self) -> None:
        capture = function_body(
            NODE_CPP,
            "void ATRIADSensorNodeActor::CaptureLongRangeConfirmation()",
            "bool ATRIADSensorNodeActor::PublishLongRangeFrame(",
        )
        range_gate = capture.index(
            "Result.RangeMeters > FMath::Max(ConfirmationRangeMeters, 500.0)"
        )
        capture_scene = capture.index("Capture->CaptureScene()")
        readback = capture.index("Resource->ReadPixels(Pixels)")
        publish = capture.index("PublishLongRangeFrame(")
        self.assertLess(range_gate, capture_scene)
        self.assertLess(range_gate, readback)
        self.assertLess(range_gate, publish)
        self.assertIn("no_capture_or_readback", capture[range_gate:capture_scene])

    def test_confirmation_fails_closed_on_occlusion(self) -> None:
        capture = function_body(
            NODE_CPP,
            "void ATRIADSensorNodeActor::CaptureLongRangeConfirmation()",
            "bool ATRIADSensorNodeActor::PublishLongRangeFrame(",
        )
        los = function_body(
            NODE_CPP,
            "bool ATRIADSensorNodeActor::ComputePTZLineOfSight(",
            "void ATRIADSensorNodeActor::CaptureLongRangeConfirmation()",
        )
        self.assertIn("LineTraceSingleByChannel", los)
        self.assertIn("Bounds.GetCenter()", los)
        self.assertIn("CornerIndex < 8", los)
        self.assertIn("QueryParameters.AddIgnoredActor(this)", los)
        self.assertIn("Result.bLineOfSight", capture)
        self.assertIn("bIntersectsFrame && Result.bLineOfSight", capture)
        self.assertIn('TEXT("blockingActor")', NODE_CPP)
        self.assertIn('TEXT("occlusionSemantics")', NODE_CPP)

    def test_latest_frame_paths_are_allowlisted_and_atomically_replaced(self) -> None:
        self.assertIn('TEXT("RadarPtzFrames")', NODE_CPP)
        self.assertIn('TEXT("eo")', NODE_CPP)
        self.assertIn('TEXT("thermal")', NODE_CPP)
        self.assertIn('TEXT("/latest.png")', NODE_CPP)
        self.assertIn('TEXT("/latest.json")', NODE_CPP)
        self.assertIn("MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH", NODE_CPP)
        self.assertIn('StartsWith(TEXT("RadarPtzFrames/eo/"))', MANAGER_CPP)
        self.assertIn('StartsWith(TEXT("RadarPtzFrames/thermal/"))', MANAGER_CPP)
        self.assertIn('Contains(TEXT(".."))', MANAGER_CPP)

    def test_only_confirmed_ptz_captures_publish_and_expose_latest_frames(self) -> None:
        capture = function_body(
            NODE_CPP,
            "void ATRIADSensorNodeActor::CaptureLongRangeConfirmation()",
            "bool ATRIADSensorNodeActor::PublishLongRangeFrame(",
        )
        publisher = function_body(
            NODE_CPP,
            "bool ATRIADSensorNodeActor::PublishLongRangeFrame(",
            "void ATRIADSensorNodeActor::EndPlay(",
        )
        confirmation_gate = capture.index("if (Result.bConfirmed)")
        publish_call = capture.index("PublishLongRangeFrame(", confirmation_gate)
        current_record = capture.index("LatestPTZConfirmations.Add", publish_call)
        self.assertLess(confirmation_gate, publish_call)
        self.assertLess(publish_call, current_record)
        self.assertEqual(capture.count("PublishLongRangeFrame("), 1)
        self.assertIn("Result.FrameRelativePath.Reset()", capture[publish_call:current_record])
        self.assertIn("Result.MetadataRelativePath.Reset()", capture[publish_call:current_record])
        self.assertIn("if (!Result.bConfirmed ||", publisher)

        self.assertIn(
            "bExposeConfirmedFrameArtifacts = Confirmation.bConfirmed &&",
            MANAGER_CPP,
        )
        self.assertIn(
            'SetBoolField(TEXT("hasFrame"), bExposeConfirmedFrameArtifacts)',
            MANAGER_CPP,
        )
        self.assertIn(
            "const FString FrameRelativePath = bExposeConfirmedFrameArtifacts",
            MANAGER_CPP,
        )
        self.assertIn(
            "const FString MetadataRelativePath = bExposeConfirmedFrameArtifacts",
            MANAGER_CPP,
        )

    def test_projection_boxes_never_claim_learned_model_output(self) -> None:
        surface = MANAGER_CPP + "\n" + NODE_CPP
        self.assertIn('TEXT("SIMULATED_SENSOR_CONFIRMATION")', surface)
        self.assertIn('TEXT("SIMULATION_PROJECTION")', surface)
        self.assertIn('TEXT("SIMULATION_PROJECTION_TRUTH")', surface)
        self.assertIn('TEXT("DEBUG_PROJECTION")', surface)
        self.assertIn('SetBoolField(TEXT("calibratedDetector"), false)', surface)
        self.assertNotIn('TEXT("MODEL_OUTPUT")', surface)

    def test_synthetic_thermal_is_visible_and_unambiguous(self) -> None:
        self.assertIn('TEXT("SIM THERMAL SYNTHETIC")', NODE_CPP)
        self.assertIn("DrawSyntheticThermalLabel", NODE_CPP)
        self.assertIn("SimulatedThermalPalette", NODE_CPP)
        self.assertIn('SetBoolField(TEXT("syntheticThermal")', NODE_CPP)
        self.assertIn("not a physical radiometric camera", MANAGER_CPP)

    def test_ptz_modalities_match_fusion_contract(self) -> None:
        self.assertRegex(
            NODE_CPP,
            r'TEXT\("EO_PTZ"\),\s*TEXT\("EO_VISIBLE"\)',
        )
        self.assertRegex(
            NODE_CPP,
            r'TEXT\("THERMAL_PTZ"\),\s*TEXT\("THERMAL_SYNTHETIC"\)',
        )
        self.assertGreaterEqual(NODE_CPP.count('TEXT("EO_VISIBLE")'), 2)
        self.assertGreaterEqual(NODE_CPP.count('TEXT("THERMAL_SYNTHETIC")'), 2)

    def test_ptz_weather_context_explains_confirmation_confidence(self) -> None:
        for field in (
            'TEXT("weatherProfile")',
            'TEXT("weatherVisibilityMeters")',
            'TEXT("weatherRainRateMillimetersPerHour")',
            'TEXT("weatherConfidenceFactor")',
            'TEXT("weatherConfidenceSemantics")',
        ):
            self.assertIn(field, NODE_CPP)
            self.assertIn(field, MANAGER_CPP)

    def test_acceptance_corridor_reaches_east_node_close_range_in_three_minutes(self) -> None:
        target = next(
            item
            for item in CONFIG["DemoTargets"]
            if item["IngressCorridorId"] == "RADAR_PTZ_ACCEPTANCE_INBOUND"
        )
        east = next(node for node in CONFIG["SensorNodes"] if node["NodeId"] == "East_Sector")
        self.assertEqual(target["SpawnCount"], 4)
        live_target_count = sum(item["SpawnCount"] for item in CONFIG["DemoTargets"])
        self.assertEqual(live_target_count, 29)
        self.assertLessEqual(
            len(CONFIG["SensorNodes"]) * live_target_count,
            CONFIG["MaxLiveSnapshotSearchRadarDetections"],
        )
        self.assertGreater(
            target["StartLongitudeDegrees"],
            CONFIG["SimulationPerimeter"]["MaximumLongitudeDegrees"],
        )
        self.assertEqual(target["RFEmitter"]["CenterFrequenciesGHz"], [2.437, 5.795])
        self.assertGreaterEqual(target["LinearSpeedMetersPerSecond"], 28.0)
        self.assertLessEqual(target["LinearSpeedMetersPerSecond"], 35.0)

        meters_per_degree = math.pi * 6_378_137.0 / 180.0
        east_start_offset = (
            target["StartLongitudeDegrees"] - east["LongitudeDegrees"]
        ) * meters_per_degree * math.cos(math.radians(east["LatitudeDegrees"]))
        vertical_offset = target["StartHeightMeters"] - east["HeightMeters"]
        maximum_north_offset = target["FormationSpacingMeters"] * 1.5
        closest_slant = math.hypot(vertical_offset, maximum_north_offset)
        self.assertGreaterEqual(closest_slant, 100.0)
        self.assertLessEqual(closest_slant, 250.0)
        horizontal_at_500 = math.sqrt(500.0**2 - vertical_offset**2)
        seconds_to_500 = (east_start_offset - horizontal_at_500) / target["LinearSpeedMetersPerSecond"]
        self.assertGreater(seconds_to_500, 120.0)
        self.assertLess(seconds_to_500, 180.0)

    def test_live_close_range_corridor_covers_about_25_through_500_meters(self) -> None:
        target = next(
            item
            for item in CONFIG["DemoTargets"]
            if item["IngressCorridorId"] == "RADAR_PTZ_CLOSE_RANGE_ACCEPTANCE"
        )
        east = next(node for node in CONFIG["SensorNodes"] if node["NodeId"] == "East_Sector")
        meters_per_degree = math.pi * 6_378_137.0 / 180.0
        east_start_offset = (
            target["StartLongitudeDegrees"] - east["LongitudeDegrees"]
        ) * meters_per_degree * math.cos(math.radians(east["LatitudeDegrees"]))
        north_offset = (
            target["StartLatitudeDegrees"] - east["LatitudeDegrees"]
        ) * meters_per_degree
        vertical_offset = target["StartHeightMeters"] - east["HeightMeters"]
        closest_slant = math.hypot(north_offset, vertical_offset)
        self.assertGreaterEqual(closest_slant, 20.0)
        self.assertLessEqual(closest_slant, 35.0)
        self.assertEqual(target["SpawnCount"], 1)
        self.assertEqual(target["RFEmitter"]["CenterFrequenciesGHz"], [2.437, 5.795])
        seconds_to_500 = (east_start_offset - math.sqrt(500.0**2 - closest_slant**2)) / target[
            "LinearSpeedMetersPerSecond"
        ]
        self.assertGreater(seconds_to_500, 25.0)
        self.assertLess(seconds_to_500, 50.0)
        for node in CONFIG["SensorNodes"]:
            self.assertGreater(node["PTZSettleSeconds"], 0.0)
            self.assertLess(node["PTZSettleSeconds"], CONFIG["SampleCadenceSeconds"])

    def test_human_debug_aids_are_bounded_and_excluded_from_captures(self) -> None:
        self.assertIn("[SIM DEBUG RADAR CUE]", NODE_CPP)
        self.assertIn("[SIM DEBUG PTZ CONFIRMED]", NODE_CPP)
        self.assertIn("SelectedRadarCueTarget", MANAGER_CPP)
        self.assertIn("HideFromSensorCaptures(LineBatch)", NODE_CPP)
        self.assertIn("EOPTZCapture->HideComponent", NODE_CPP)
        self.assertIn("ThermalPTZCapture->HideComponent", NODE_CPP)

    def test_close_100_250_500_meter_deterministic_validation_contract(self) -> None:
        for literal in ("25.0", "100.0", "250.0", "500.0"):
            self.assertIn(literal, MODEL_TEST_CPP)
        self.assertIn("RangeNoiseSigmaMeters = 0.0", MODEL_TEST_CPP)
        self.assertIn("Output.MeasuredRangeMeters", MODEL_TEST_CPP)
        self.assertIn("PixelExtent.X >= 2.0", MODEL_TEST_CPP)

        # Mirror the documented pinhole check: a 0.5 m target remains well over
        # the two-pixel simulation-confirmation floor at 500 m and 4 degrees.
        width_pixels = 960
        fov_radians = math.radians(4.0)
        target_angle = 2.0 * math.atan(0.5 / (2.0 * 500.0))
        self.assertGreaterEqual(target_angle / fov_radians * width_pixels, 2.0)
        self.assertIn("FMath::Max(Input.RangeEnvelopeMeters, 5000.0)", MODEL_CPP)

    def test_drone_mesh_uses_uniform_component_scale_only(self) -> None:
        self.assertIn("VisualMesh->SetRelativeScale3D(FVector(UniformScale))", DEMO_CPP)
        self.assertNotIn("FVector(1.0, 1.0,", DEMO_CPP)
        self.assertNotIn("SetWorldScale3D", NODE_CPP + MANAGER_CPP + MODEL_CPP)

    def test_projection_label_explains_the_box(self) -> None:
        self.assertIn('TEXT("SIMULATED SENSOR CONFIRMATION")', NODE_CPP)
        self.assertIn('TEXT("SIMULATED SENSOR CONFIRMATION")', MANAGER_CPP)
        self.assertNotIn("SIM GROUND-TRUTH BOX", NODE_CPP + MANAGER_CPP)


if __name__ == "__main__":
    unittest.main()
