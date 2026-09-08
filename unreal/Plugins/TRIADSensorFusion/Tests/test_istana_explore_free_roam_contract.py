from __future__ import annotations

import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal" / "Plugins" / "TRIADSensorFusion"
RUNTIME = PLUGIN / "Source" / "TRIADSensorFusion"
PAWN_HEADER = RUNTIME / "Public" / "TRIADIstanaFreeRoamPawn.h"
PAWN_SOURCE = RUNTIME / "Private" / "TRIADIstanaFreeRoamPawn.cpp"
GAME_MODE_HEADER = RUNTIME / "Private" / "TRIADIstanaExploreGameMode.h"
GAME_MODE_SOURCE = RUNTIME / "Private" / "TRIADIstanaExploreGameMode.cpp"
BUILD_RULES = RUNTIME / "TRIADSensorFusion.Build.cs"
EDITOR_SOURCE = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreEditorLibrary.cpp"
)


class IstanaExploreFreeRoamContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        required = (
            PAWN_HEADER,
            PAWN_SOURCE,
            GAME_MODE_HEADER,
            GAME_MODE_SOURCE,
            BUILD_RULES,
            EDITOR_SOURCE,
        )
        missing = [path for path in required if not path.is_file()]
        if missing:
            raise AssertionError(
                "Istana Explore runtime source set is incomplete: "
                + ", ".join(str(path) for path in missing)
            )
        cls.pawn_header = PAWN_HEADER.read_text(encoding="utf-8")
        cls.pawn_source = PAWN_SOURCE.read_text(encoding="utf-8")
        cls.game_mode_header = GAME_MODE_HEADER.read_text(encoding="utf-8")
        cls.game_mode_source = GAME_MODE_SOURCE.read_text(encoding="utf-8")
        cls.build_rules = BUILD_RULES.read_text(encoding="utf-8")
        cls.editor_source = EDITOR_SOURCE.read_text(encoding="utf-8")

    def test_01_pawn_owns_collision_camera_and_explicit_controls(self) -> None:
        self.assertRegex(
            self.pawn_header,
            r"ATRIADIstanaFreeRoamPawn\s*:\s*public\s+APawn",
        )
        self.assertIn("TObjectPtr<UCapsuleComponent> CollisionCapsule", self.pawn_header)
        self.assertIn("TObjectPtr<UCameraComponent> ExploreCamera", self.pawn_header)
        for control in ("W/S", "A/D", "E/Q", "Mouse", "Shift", "Ctrl"):
            self.assertIn(control, self.pawn_header)
        self.assertIn("Escape and Shift+F1 are intentionally not bound", self.pawn_header)
        self.assertIn("UCollisionProfile::Pawn_ProfileName", self.pawn_source)
        self.assertIn("SetActorEnableCollision(true)", self.pawn_source)

    def test_02_input_is_polled_without_project_mapping_mutation(self) -> None:
        for key in (
            "EKeys::W",
            "EKeys::S",
            "EKeys::A",
            "EKeys::D",
            "EKeys::E",
            "EKeys::Q",
            "EKeys::LeftShift",
            "EKeys::RightShift",
            "EKeys::LeftControl",
            "EKeys::RightControl",
        ):
            self.assertIn(key, self.pawn_source)
        self.assertIn("IsInputKeyDown", self.pawn_source)
        self.assertIn("GetInputMouseDelta", self.pawn_source)
        for forbidden in (
            "BindAxis",
            "BindAction",
            "UInputSettings",
            "AddAxisMapping",
            "AddActionMapping",
            "EKeys::Escape",
            "EKeys::F1",
        ):
            self.assertNotIn(forbidden, self.pawn_source)
        self.assertIn('"InputCore"', self.build_rules)

    def test_03_local_player_uses_pawn_camera_and_game_only_input(self) -> None:
        self.assertIn("FInputModeGameOnly InputMode", self.pawn_source)
        self.assertIn("PlayerController->SetInputMode(InputMode)", self.pawn_source)
        self.assertIn("PlayerController->bShowMouseCursor = false", self.pawn_source)
        self.assertIn("PlayerController->SetViewTarget(this)", self.pawn_source)
        self.assertIn("PlayerController->GetViewTarget() != this", self.pawn_source)
        self.assertIn("ExploreCamera->SetRelativeRotation", self.pawn_source)
        self.assertNotIn("FInputModeUIOnly", self.pawn_source)

    def test_04_motion_is_swept_sliding_and_bounded(self) -> None:
        self.assertRegex(
            self.pawn_header,
            r"MaximumHorizontalTravelMeters\s*=\s*950\.0f",
        )
        self.assertRegex(
            self.pawn_header,
            r"MinimumLocalAltitudeMeters\s*=\s*1\.5f",
        )
        self.assertRegex(
            self.pawn_header,
            r"MaximumLocalAltitudeMeters\s*=\s*300\.0f",
        )
        self.assertIn("HorizontalDistanceCentimeters > MaximumRadiusCentimeters", self.pawn_source)
        self.assertIn("MaximumRadiusCentimeters / HorizontalDistanceCentimeters", self.pawn_source)
        self.assertIn("ClampedLocation.Z = FMath::Clamp", self.pawn_source)
        self.assertGreaterEqual(
            len(re.findall(r"AddActorWorldOffset\s*\(", self.pawn_source)),
            2,
        )
        self.assertIn("FirstHit.bBlockingHit", self.pawn_source)
        self.assertIn("FVector::VectorPlaneProject", self.pawn_source)

    def test_05_game_mode_is_additive_and_keeps_airsim_base(self) -> None:
        self.assertRegex(
            self.game_mode_header,
            r"ATRIADIstanaExploreGameMode\s*\n?\s*:\s*public\s+"
            r"ATRIADIstanaAirSimGameMode",
        )
        self.assertIn(
            "DefaultPawnClass = ATRIADIstanaFreeRoamPawn::StaticClass()",
            self.game_mode_source,
        )
        self.assertIn("HUDClass = nullptr", self.game_mode_source)
        self.assertIn(
            'ExploreStartTag(TEXT("TRIADIstanaExplorePlayerStartV1"))',
            self.game_mode_source,
        )
        self.assertIn("ChoosePlayerStart_Implementation", self.game_mode_source)
        self.assertNotIn("ASimHUD::StaticClass", self.game_mode_source)
        combined = self.game_mode_header + self.game_mode_source
        for frozen_map_name in (
            "Istana_PublicView_Exterior_v1",
            "Istana_PublicView_Exterior_v2",
            "Istana_PublicView_Exterior_v3",
            "Istana_PublicView_Exterior_v4",
            "Istana_PublicView_Exterior_v5",
            "Istana_PublicView_Exterior_v6",
        ):
            self.assertNotIn(frozen_map_name, combined)

    def test_06_camera_has_deterministic_manual_exposure_and_keeps_spawn_pitch(self) -> None:
        for token in (
            "ExploreCamera->bAutoActivate = true",
            "ExploreCamera->Activate(true)",
            "PostProcessSettings = FPostProcessSettings()",
            "AutoExposureMethod = AEM_Manual",
            "AutoExposureApplyPhysicalCameraExposure = true",
            "CameraShutterSpeed = ExploreCameraShutterSpeed",
            "CameraISO = ExploreCameraIso",
            "DepthOfFieldFstop = ExploreCameraFStop",
            "DepthOfFieldScale = 0.0f",
            "WhiteTemp = ExploreCameraWhiteTemperature",
            "BloomIntensity = 0.0f",
            "VignetteIntensity = 0.0f",
            "MotionBlurAmount = 0.0f",
            "SceneFringeIntensity = 0.0f",
            "const FRotator SpawnRotation = GetActorRotation()",
            "FRotator::NormalizeAxis(SpawnRotation.Pitch)",
        ):
            self.assertIn(token, self.pawn_source)
        self.assertNotIn("ViewPitchDegrees = 0.0f;", self.pawn_source)
        for token in (
            "ExplorePawn->GetExploreCameraComponent()",
            "ExploreCamera->IsActive()",
            "ExplorePawn->HasExpectedExploreCameraProfile()",
            "manual exposure",
        ):
            self.assertIn(token, self.editor_source)


if __name__ == "__main__":
    unittest.main()
