import re
import shutil
import subprocess
import unittest
from pathlib import Path


PLUGIN_ROOT = Path(__file__).resolve().parents[1]
UNREAL_ROOT = PLUGIN_ROOT.parent.parent
REPOSITORY_ROOT = UNREAL_ROOT.parent
HEADER_PATH = (
    PLUGIN_ROOT
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Public"
    / "TRIADIstanaPublicViewEditorLibrary.h"
)
CPP_PATH = (
    PLUGIN_ROOT
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaPublicViewEditorLibrary.cpp"
)
START_SCRIPT_PATH = (
    REPOSITORY_ROOT / "scripts" / "Set-IstanaPublicViewHeroV2PlayInEditor.ps1"
)
CAPTURE_SCRIPT_PATH = (
    REPOSITORY_ROOT
    / "scripts"
    / "Capture-IstanaPublicViewHeroV2PlayPreviews.ps1"
)
LEGACY_START_PATH = (
    REPOSITORY_ROOT / "scripts" / "Set-IstanaPublicViewPlayInEditor.ps1"
)
LEGACY_CAPTURE_PATH = (
    REPOSITORY_ROOT / "scripts" / "Capture-IstanaPublicViewPlayPreviews.ps1"
)


class IstanaPublicViewHeroV2LiveAcceptanceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = HEADER_PATH.read_text(encoding="utf-8")
        cls.cpp = CPP_PATH.read_text(encoding="utf-8")
        cls.start_script = START_SCRIPT_PATH.read_text(encoding="utf-8")
        cls.capture_script = CAPTURE_SCRIPT_PATH.read_text(encoding="utf-8")
        cls.legacy_start = LEGACY_START_PATH.read_text(encoding="utf-8")
        cls.legacy_capture = LEGACY_CAPTURE_PATH.read_text(encoding="utf-8")

    def test_01_v2_runtime_surface_is_explicit_and_v1_remains_separate(self) -> None:
        for function_name in (
            "ValidateIstanaPublicViewHeroV2PlayWorldReadiness",
            "CaptureIstanaPublicViewHeroV2PlayCamera",
            "QuiesceIstanaPublicViewHeroV2PlayWorldForStop",
        ):
            self.assertIn(function_name, self.header)
            self.assertIn(function_name, self.cpp)
        for preset_name in (
            "HERO_FRONT_CLOSE",
            "HERO_FRONT_OBLIQUE_CLOSE",
            "HERO_FACADE_MACRO",
        ):
            self.assertIn(preset_name, self.header)
        self.assertIn("intentional detail-only façade crop", self.header)
        self.assertIn(
            'TEXT("/Game/Maps/Istana_PublicView_Exterior_v2")', self.cpp
        )
        self.assertIn(
            'TEXT("/Game/Maps/Istana_PublicView_Exterior_v1")', self.cpp
        )
        self.assertNotIn("HeroV2", self.legacy_start)
        self.assertNotIn("HeroV2", self.legacy_capture)
        self.assertIn(
            "/Game/Maps/Istana_PublicView_Exterior_v1", self.legacy_start
        )
        self.assertIn(
            "/Game/Maps/Istana_PublicView_Exterior_v1", self.legacy_capture
        )

    def test_02_v2_readiness_is_map_and_preservation_gated(self) -> None:
        start = self.cpp.index(
            "ValidateIstanaPublicViewHeroV2PlayWorldReadiness(FString& OutReport)"
        )
        end = self.cpp.index(
            "::CaptureIstanaPublicViewPlayCamera", start
        )
        body = self.cpp[start:end]
        for marker in (
            "ValidateIstanaPublicViewHeroV2Map",
            "UWorld::RemovePIEPrefix",
            "SourcePackage != HeroV2DestinationMapPackage",
            "PlayWorld->WorldType != EWorldType::PIE",
            "PlayWorld->IsGameWorld()",
            "PlayWorld->HasBegunPlay()",
            "IstanaAirSimGameModeClassPath",
            "SceneActorTag",
            "RuntimePolicyTag",
            "IsFogSuppressionActive",
            "IsFixedPrimaryCameraProfileActive",
            "GetCameraSpecs()",
            "PlayerController->GetViewTarget() != PrimaryCamera",
        ):
            self.assertIn(marker, body)
        self.assertNotIn("ValidatePublicViewWorld(PlayWorld", body)
        self.assertNotIn("Istana_PublicView_Exterior_v1", body)

    def test_03_close_captures_are_transient_4k_and_restore_runtime_state(self) -> None:
        start = self.cpp.index("CaptureIstanaPublicViewHeroV2PlayCamera(")
        end = self.cpp.index(
            "::QuiesceIstanaPublicViewPlayWorldForStop", start
        )
        body = self.cpp[start:end]
        for marker in (
            "ValidateIstanaPublicViewHeroV2PlayWorldReadiness",
            "HERO_FRONT_CLOSE",
            "HERO_FRONT_OBLIQUE_CLOSE",
            "HERO_FACADE_MACRO",
            "RF_Transient",
            "TRIADIstanaPublicViewHeroV2CaptureOnly",
            "SetFixedViewportSize(3840, 2160)",
            "TRIAD/IstanaPreviews/PublicViewHeroV2",
            "ipv_v2_play_",
            "RequestScreenshot(DestinationPath, false, false, false)",
            "RestorePrimaryAndDestroyCapture",
            "CaptureCamera->Destroy()",
            "SetViewTargetWithBlend(PrimaryCamera, 0.0f)",
            "no map actor or surrounding asset was changed",
        ):
            self.assertIn(marker, body)
        for forbidden in (
            "SaveMap",
            "SaveAsset",
            "DeleteFile",
            "GEngine->Exec",
            "HighResShot",
        ):
            self.assertNotIn(forbidden, body)

    def test_04_v2_start_stop_wrapper_is_fail_closed(self) -> None:
        script = self.start_script
        for marker in (
            "ValidateIstanaPublicViewHeroV2RemoteControlProject",
            "ValidateIstanaPublicViewHeroV2Map",
            "ValidateIstanaPublicViewHeroV2PlayWorldReadiness",
            "QuiesceIstanaPublicViewHeroV2PlayWorldForStop",
            "EditorRequestBeginPlay",
            "EditorRequestEndPlay",
            "Start-Sleep -Seconds $QuiesceDrainSeconds",
            "$requiredStableReadinessPolls = 3",
            "/Game/Maps/Istana_PublicView_Exterior_v2",
        ):
            self.assertIn(marker, script)
        stop_body = script[script.index("if ($Stop)") :]
        self.assertLess(
            stop_body.index(
                "-FunctionName 'QuiesceIstanaPublicViewHeroV2PlayWorldForStop'"
            ),
            stop_body.index("-FunctionName 'EditorRequestEndPlay'"),
        )
        for forbidden in ("Stop-Process", "taskkill", "SaveMap", "SaveAsset"):
            self.assertNotIn(forbidden, script)

    def test_05_capture_wrapper_proves_both_maps_unchanged(self) -> None:
        script = self.capture_script
        for marker in (
            "ValidateIstanaPublicViewHeroV2RemoteControlProject",
            "ValidateIstanaPublicViewHeroV2PlayWorldReadiness",
            "CaptureIstanaPublicViewHeroV2PlayCamera",
            "HERO_FRONT_CLOSE",
            "HERO_FRONT_OBLIQUE_CLOSE",
            "HERO_FACADE_MACRO",
            "Get-PngDimensions",
            "3840",
            "2160",
            "Istana_PublicView_Exterior_v1.umap",
            "Istana_PublicView_Exterior_v2.umap",
            "$v1HashBefore",
            "$v1HashAfter",
            "$v2HashBefore",
            "$v2HashAfter",
            "MapsUnchanged = $true",
            "SurroundingsModified = $false",
        ):
            self.assertIn(marker, script)
        self.assertEqual(3, len(re.findall(r"Preset = 'HERO_", script)))
        for forbidden in (
            "Remove-Item",
            "Move-Item",
            "Set-Content",
            "Copy-Item",
            "SaveMap",
            "SaveAsset",
        ):
            self.assertNotIn(forbidden, script)

    def test_06_wrapper_powershell_syntax_is_valid(self) -> None:
        powershell = shutil.which("powershell") or shutil.which("pwsh")
        if not powershell:
            self.skipTest("PowerShell parser is unavailable on this host.")
        for path in (START_SCRIPT_PATH, CAPTURE_SCRIPT_PATH):
            escaped_path = str(path).replace("'", "''")
            parser = (
                "$tokens=$null; $errors=$null; "
                "[System.Management.Automation.Language.Parser]::ParseFile("
                f"'{escaped_path}',[ref]$tokens,[ref]$errors) | Out-Null; "
                "if($errors.Count -ne 0){$errors | ForEach-Object {Write-Error $_}; exit 1}"
            )
            completed = subprocess.run(
                [powershell, "-NoProfile", "-NonInteractive", "-Command", parser],
                check=False,
                capture_output=True,
                text=True,
                timeout=30,
            )
            self.assertEqual(
                0,
                completed.returncode,
                f"{path.name}: {completed.stdout}\n{completed.stderr}",
            )

    def test_07_close_captures_follow_rebased_scene_frame_and_finish_before_restore(
        self,
    ) -> None:
        start = self.cpp.index("CaptureIstanaPublicViewHeroV2PlayCamera(")
        end = self.cpp.index(
            "::QuiesceIstanaPublicViewPlayWorldForStop", start
        )
        body = self.cpp[start:end]
        for marker in (
            "LocalCaptureLocation = FVector(0.0, 29200.0, 5000.0)",
            "LocalCaptureLookAt = FVector(0.0, 500.0, 1350.0)",
            "LocalCaptureLocation = FVector(17500.0, 26500.0, 4500.0)",
            "LocalCaptureLookAt = FVector(0.0, 500.0, 1400.0)",
            "LocalCaptureLocation = FVector(0.0, 10000.0, 1800.0)",
            "LocalCaptureLookAt = FVector(0.0, 5000.0, 600.0)",
            "CaptureFieldOfView = 32.0f",
            "CaptureFieldOfView = 28.0f",
            "CaptureFieldOfView = 58.0f",
            "TActorIterator<ATRIADIstanaPublicViewSceneActor>",
            "Candidate->ActorHasTag(SceneActorTag)",
            "SceneActorCount != 1",
            "SceneActor->HasActorBegunPlay()",
            "SceneFrame.IsValid()",
            "SceneFrame.TransformPosition(LocalCaptureLocation)",
            "SceneFrame.TransformPosition(LocalCaptureLookAt)",
            "FMath::IsFinite(Value.X)",
            "FMath::IsFinite(Value.Y)",
            "FMath::IsFinite(Value.Z)",
            "FScreenshotRequest::IsScreenshotRequested()",
            "IFileManager::Get().FileExists(*DestinationPath)",
            "FScreenshotRequest::Reset()",
        ):
            self.assertIn(marker, body)

        transform_index = body.index(
            "SceneFrame.TransformPosition(LocalCaptureLocation)"
        )
        spawn_index = body.index("PlayWorld->SpawnActor<ACameraActor>")
        request_index = body.index(
            "FScreenshotRequest::RequestScreenshot(DestinationPath"
        )
        capture_draw_index = body.index(
            "GameViewport->Draw(false);", request_index
        )
        processed_index = body.index(
            "const bool bScreenshotRequestProcessed", capture_draw_index
        )
        reset_index = body.index("FScreenshotRequest::Reset()", processed_index)
        restore_index = body.index(
            "RestorePrimaryAndDestroyCapture();", reset_index
        )
        self.assertLess(transform_index, spawn_index)
        self.assertLess(request_index, capture_draw_index)
        self.assertLess(capture_draw_index, processed_index)
        self.assertLess(processed_index, reset_index)
        self.assertLess(reset_index, restore_index)

        # A representative AirSim origin shift must move both endpoints by the
        # same amount without changing the scene-relative look vector.
        local_camera = (0.0, 29200.0, 5000.0)
        local_target = (0.0, 500.0, 1350.0)
        scene_translation = (0.0, -8000.0, 0.0)
        world_camera = tuple(
            value + offset
            for value, offset in zip(local_camera, scene_translation)
        )
        world_target = tuple(
            value + offset
            for value, offset in zip(local_target, scene_translation)
        )
        self.assertEqual((0.0, 21200.0, 5000.0), world_camera)
        self.assertEqual((0.0, -7500.0, 1350.0), world_target)
        self.assertEqual(
            tuple(target - camera for camera, target in zip(local_camera, local_target)),
            tuple(target - camera for camera, target in zip(world_camera, world_target)),
        )


if __name__ == "__main__":
    unittest.main()
