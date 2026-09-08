"""Engine-independent source contracts for the narrow Istana editor tooling."""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


PLUGIN = Path(__file__).resolve().parents[1]
RUNTIME = PLUGIN / "Source" / "TRIADSensorFusion"
EDITOR = PLUGIN / "Source" / "TRIADSensorFusionEditor"
UPLUGIN = json.loads((PLUGIN / "TRIADSensorFusion.uplugin").read_text(encoding="utf-8"))
EDITOR_H = (EDITOR / "Public" / "TRIADIstanaEditorLibrary.h").read_text(encoding="utf-8")
EDITOR_CPP = (EDITOR / "Private" / "TRIADIstanaEditorLibrary.cpp").read_text(encoding="utf-8")
BUILDING_H = (RUNTIME / "Public" / "TRIADIstanaBuildingActor.h").read_text(encoding="utf-8")
BUILDING_CPP = (RUNTIME / "Private" / "TRIADIstanaBuildingActor.cpp").read_text(encoding="utf-8")
STUDY_CPP = (RUNTIME / "Private" / "TRIADIstanaStudyAreaActor.cpp").read_text(encoding="utf-8")
GEODESY_CPP = (RUNTIME / "Private" / "TRIADGeodesy.cpp").read_text(encoding="utf-8")
MANAGER_CPP = (RUNTIME / "Private" / "TRIADSensorFusionScenarioManager.cpp").read_text(encoding="utf-8")
TYPES_H = (RUNTIME / "Public" / "TRIADSensorFusionTypes.h").read_text(encoding="utf-8")
REQUEST = json.loads(
    (PLUGIN / "Resources" / "IstanaSensorPlacementRequest.example.json").read_text(
        encoding="utf-8"
    )
)


class IstanaMapBuilderContractTests(unittest.TestCase):
    def test_runtime_and_editor_modules_are_separated(self) -> None:
        modules = {module["Name"]: module["Type"] for module in UPLUGIN["Modules"]}
        self.assertEqual(modules["TRIADSensorFusion"], "Runtime")
        self.assertEqual(modules["TRIADSensorFusionEditor"], "Editor")
        editor_build = (EDITOR / "TRIADSensorFusionEditor.Build.cs").read_text(
            encoding="utf-8"
        )
        self.assertIn('"UnrealEd"', editor_build)
        self.assertIn('"CesiumRuntime"', editor_build)

    def test_builder_is_template_copy_non_overwriting_and_dirty_safe(self) -> None:
        self.assertIn('TEXT("/Game/SDTH")', EDITOR_CPP)
        self.assertIn('TEXT("/Game/Maps/Istana_1km")', EDITOR_CPP)
        self.assertIn("GetDirtyMapPackages", EDITOR_CPP)
        self.assertIn("GetDirtyContentPackages", EDITOR_CPP)
        self.assertIn("FPackageName::DoesPackageExist(DestinationMapPackage)", EDITOR_CPP)
        self.assertIn("NewMapFromTemplate(SourceFilename, false)", EDITOR_CPP)
        self.assertIn("SaveMap(World, DestinationMapPackage)", EDITOR_CPP)
        self.assertNotIn("DeleteAsset", EDITOR_CPP)
        self.assertNotIn("SaveDirtyPackages", EDITOR_CPP)
        self.assertLess(
            EDITOR_CPP.index("NewMapFromTemplate(SourceFilename, false)"),
            EDITOR_CPP.index("SaveMap(World, DestinationMapPackage)"),
        )

    def test_exact_geodesic_circle_and_tileset_clip_contract(self) -> None:
        self.assertIn("constexpr int32 PolygonPointCount = 64", EDITOR_CPP)
        self.assertIn("constexpr double StudyRadiusMeters = 1000.0", EDITOR_CPP)
        self.assertIn("Wgs84DestinationDegrees", EDITOR_CPP)
        self.assertIn("Overlay->InvertSelection = true", EDITOR_CPP)
        self.assertIn("Overlay->ExcludeSelectedTiles = true", EDITOR_CPP)
        self.assertIn("Tileset->SetCreatePhysicsMeshes(true)", EDITOR_CPP)
        self.assertIn("Tileset->EnableFrustumCulling = false", EDITOR_CPP)
        self.assertIn("Tileset->EnableFogCulling = false", EDITOR_CPP)
        self.assertIn("Tileset->EnforceCulledScreenSpaceError = true", EDITOR_CPP)
        self.assertIn("Tileset->CulledScreenSpaceError = 32.0", EDITOR_CPP)

    def test_public_exterior_is_readable_and_physics_enabled(self) -> None:
        surface = BUILDING_H + BUILDING_CPP
        for feature in (
            "Cross-shaped two-storey body",
            "Open front/rear veranda rhythm",
            "Shuttered/louvred windows",
            "Central south-facing portico",
            "Three-storey tower",
            "mansard",
            "dormers",
            "cupola",
            "Circular fountain",
        ):
            self.assertIn(feature, surface)
        self.assertIn("ECollisionEnabled::QueryAndPhysics", BUILDING_CPP)
        self.assertIn("M_Basic_Wall", BUILDING_CPP)
        self.assertIn("M_Ground_Grass", BUILDING_CPP)
        self.assertIn("M_Glass", BUILDING_CPP)
        self.assertIn("M_Water_Lake", BUILDING_CPP)
        self.assertIn("MasonryAccent->SetMaterial(0, Masonry->GetMaterial(0))", BUILDING_CPP)
        self.assertIn("ConfigureInstances(Foliage, Sphere, false)", BUILDING_CPP)
        self.assertIn("AddSphere(Foliage", BUILDING_CPP)
        self.assertNotIn("AddSphere(Domes, FVector(X, Y", BUILDING_CPP)

    def test_boundary_is_visible_but_does_not_occlude_sensors(self) -> None:
        self.assertIn("ECollisionEnabled::NoCollision", STUDY_CPP)
        self.assertIn('TEXT("TRIADHumanOnlyOverlay")', STUDY_CPP)
        self.assertIn("BoundarySegments = FMath::Clamp", STUDY_CPP)
        self.assertIn("SafeRadiusMeters", STUDY_CPP)

    def test_flat_terrain_fallback_is_colliding_hidden_and_explicitly_approximate(self) -> None:
        self.assertIn('TEXT("ApproximateTerrainFallbackCollision")', STUDY_CPP)
        self.assertIn('TEXT("TRIADApproximateTerrainFallback")', STUDY_CPP)
        self.assertIn("ECollisionEnabled::QueryAndPhysics", STUDY_CPP)
        self.assertIn("SetVisibility(false, true)", STUDY_CPP)
        self.assertIn("SetHiddenInGame(true)", STUDY_CPP)
        self.assertIn('TEXT("LOADED_SIMULATION_COLLISION_ONLY")', EDITOR_CPP)
        self.assertIn('TEXT("surveyGrade")', EDITOR_CPP)
        self.assertIn('TEXT("approximateTerrainFallbackEnabled")', EDITOR_CPP)

    def test_circle_perimeter_is_additive_and_rectangle_default_survives(self) -> None:
        self.assertIn('FString Shape = TEXT("Rectangle")', TYPES_H)
        for field in (
            "CenterLongitudeDegrees",
            "CenterLatitudeDegrees",
            "RadiusMeters",
        ):
            self.assertIn(field, TYPES_H)
        self.assertIn("if (IsCircle(Perimeter))", GEODESY_CPP)
        self.assertIn("CenterDistanceMeters - Perimeter.RadiusMeters", GEODESY_CPP)
        self.assertIn("MinimumLongitudeDegrees", GEODESY_CPP)
        self.assertIn("ValidatePerimeter(OutConfig.SimulationPerimeter", MANAGER_CPP)
        self.assertIn('TEXT("wgs84_geodesic_circle")', MANAGER_CPP)

    def test_survey_parses_allowlisted_request_and_is_core_compatible(self) -> None:
        signature = re.compile(
            r"GenerateIstanaPlacementSurvey\(\s*const FString& RequestJsonPath,\s*"
            r"const FString& OutputFileName,\s*FString& OutMessage\)",
            re.S,
        )
        self.assertRegex(EDITOR_H, signature)
        self.assertRegex(EDITOR_CPP, signature)
        self.assertIn("ResolveSafePlacementRequestPath", EDITOR_CPP)
        self.assertIn("ParseSurveyRequest", EDITOR_CPP)
        self.assertIn('TEXT("triad.placement_request.v1")', EDITOR_CPP)
        self.assertIn('TEXT("triad.placement_survey.v1")', EDITOR_CPP)
        self.assertIn("Request.Scenarios", EDITOR_CPP)
        self.assertIn("Request.SearchRadarRangeMeters", EDITOR_CPP)
        self.assertIn("Request.CameraFieldOfViewDegrees", EDITOR_CPP)
        self.assertIn('TEXT("site-r950"), 950.0', EDITOR_CPP)
        self.assertIn("LineTraceSingleByChannel", EDITOR_CPP)
        self.assertIn("Refusing to overwrite existing file", EDITOR_CPP)
        self.assertNotIn("bSearchRadarRequireLineOfSight", EDITOR_CPP)
        self.assertNotIn("bCameraTraceComplex", EDITOR_CPP)

        self.assertEqual(REQUEST["schemaVersion"], "triad.placement_request.v1")
        self.assertEqual(REQUEST["requestId"], "istana-1km-mvp")
        package = REQUEST["sensorPackages"][0]
        self.assertEqual(package["packageId"], "full-stack")
        self.assertFalse(package["unrealNodeTemplate"]["bTrackNearestTarget"])
        self.assertEqual(package["unrealNodeTemplate"]["SearchRadarRangeMeters"], 5000.0)
        self.assertEqual(
            [scenario["scenarioId"] for scenario in REQUEST["scenarios"]],
            ["clear-rf", "monsoon-rf", "haze-rf", "clear-rf-silent"],
        )

    def test_preview_presets_are_deterministic_and_non_overwriting(self) -> None:
        preview = EDITOR_CPP[
            EDITOR_CPP.index("CaptureIstanaPreview(") :
            EDITOR_CPP.index("CaptureIstanaPlaySpawnPreview(")
        ]
        for preset in ("FRONT", "OBLIQUE", "SIDE"):
            self.assertIn(f'TEXT("{preset}")', preview)
        self.assertIn("GCurrentLevelEditingViewportClient", preview)
        self.assertIn("FScreenshotRequest::RequestScreenshot", preview)
        self.assertIn("ResolveNewSavedFile", preview)
        self.assertIn('TEXT(".png")', preview)
        self.assertIn("ValidateIstanaRuntimeMapV2Readiness", preview)
        self.assertIn("ATRIADIstanaExteriorMeshActor", preview)
        self.assertIn("V2_IMPORTED_MESH", preview)
        self.assertIn("v2_editor_", preview)
        self.assertNotIn("not yet implemented", preview)


if __name__ == "__main__":
    unittest.main()
