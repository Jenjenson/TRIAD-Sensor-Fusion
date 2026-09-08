import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
SOURCE_ROOT = ROOT / "SourceAssets" / "IstanaPublicViewExploreV5D" / "DynamicRange"
RUNTIME_PUBLIC = ROOT / "Plugins" / "TRIADSensorFusion" / "Source" / "TRIADSensorFusion" / "Public"
RUNTIME_PRIVATE = ROOT / "Plugins" / "TRIADSensorFusion" / "Source" / "TRIADSensorFusion" / "Private"
EDITOR_PUBLIC = ROOT / "Plugins" / "TRIADSensorFusion" / "Source" / "TRIADSensorFusionEditor" / "Public"
EDITOR_PRIVATE = ROOT / "Plugins" / "TRIADSensorFusion" / "Source" / "TRIADSensorFusionEditor" / "Private"
DOCS = ROOT / "Plugins" / "TRIADSensorFusion" / "Docs"


class ExploreV5DDynamicRangeContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = json.loads(
            (SOURCE_ROOT / "istana_public_view_v5d_dynamic_range.contract.json").read_text(
                encoding="utf-8"
            )
        )
        cls.actor_h = (
            RUNTIME_PUBLIC / "TRIADIstanaExploreV5DDynamicRangeRealismActor.h"
        ).read_text(encoding="utf-8")
        cls.actor_cpp = (
            RUNTIME_PRIVATE / "TRIADIstanaExploreV5DDynamicRangeRealismActor.cpp"
        ).read_text(encoding="utf-8")
        cls.editor_h = (
            EDITOR_PUBLIC
            / "TRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary.h"
        ).read_text(encoding="utf-8")
        cls.editor_cpp = (
            EDITOR_PRIVATE
            / "TRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary.cpp"
        ).read_text(encoding="utf-8")
        cls.docs = (DOCS / "IstanaExploreV5DDynamicRangeRealism.md").read_text(
            encoding="utf-8"
        )

    def test_exact_map_and_single_component_are_frozen(self):
        self.assertEqual(
            "/Game/Maps/Istana_PublicView_Explore_v5d_hybrid",
            self.contract["targetMap"],
        )
        profile = self.contract["postProcess"]
        self.assertEqual(1, profile["ownedComponentCount"])
        self.assertTrue(profile["unbound"])
        self.assertEqual(
            1,
            self.actor_cpp.count("CreateDefaultSubobject<UPostProcessComponent>"),
        )
        for token in (
            "DynamicRangePostProcess->bUnbound = true",
            "RequiredPostProcessPriority = 6400.0f",
            "RequiredPostProcessBlendWeight = 1.0f",
            "RequiredPostProcessBlendRadius = 0.0f",
            "OwnedPostProcessComponents.Num() != 1",
        ):
            self.assertIn(token, self.actor_cpp)
        self.assertNotIn("APostProcessVolume", self.actor_cpp + self.editor_cpp)

    def test_local_exposure_and_color_values_are_exact_and_restrained(self):
        expected = {
            "highlightContrastScale": 0.66,
            "shadowContrastScale": 0.58,
            "detailStrength": 1.01,
            "blurredLuminanceBlend": 0.55,
            "blurredLuminanceKernelSizePercent": 50.0,
            "middleGreyBias": 0.0,
            "globalSaturation": 0.97,
            "globalContrast": 0.99,
        }
        for key, value in expected.items():
            self.assertEqual(value, self.contract["postProcess"][key], key)
        for token in (
            "ELocalExposureMethod::Bilateral",
            "HighlightContrastScale = 0.66f",
            "ShadowContrastScale = 0.58f",
            "DetailStrength = 1.01f",
            "BlurredLuminanceBlend = 0.55f",
            "BlurredLuminanceKernelSizePercent = 50.0f",
            "GlobalSaturation = 0.970f",
            "GlobalContrast = 0.990f",
        ):
            self.assertIn(token, self.actor_cpp)
        self.assertEqual(0, self.contract["postProcess"]["curveAssets"])
        self.assertEqual(0, self.contract["postProcess"]["lutAssets"])
        self.assertEqual(0, self.contract["postProcess"]["customBlendables"])

    def test_source_clear_day_lighting_is_snapshotted_not_mutated(self):
        source = self.contract["sourceLighting"]
        self.assertEqual(80000.0, source["directionalSunLux"])
        self.assertEqual(1.0, source["skyLightIntensity"])
        self.assertTrue(source["skyLightRealtimeCapture"])
        self.assertTrue(source["preAndPostSnapshotRequired"])
        self.assertFalse(source["actorsOrComponentsModified"])
        combined_cpp = self.actor_cpp + self.editor_cpp
        for token in (
            "SourceSunIntensityLux = 80000.0f",
            "SourceSunRotation(-32.0, -145.0, 0.0)",
            "SourceLightingTag(TEXT(\"TRIADIstanaPublicViewLighting_v1\"))",
            "BeforeLighting.Equals(AfterLighting, 0.0001)",
            "ValidateSourceLightingStillMatches",
        ):
            self.assertIn(token, combined_cpp)
        for forbidden in (
            "SetIntensity(",
            "SetLightColor(",
            "SetActorRotation(",
            "SetRealTimeCapture(",
        ):
            self.assertNotIn(forbidden, combined_cpp)

    def test_physical_camera_is_unchanged_and_final_composition_is_effective(self):
        camera = self.contract["cameraComposition"]
        self.assertFalse(camera["pawnPhysicalProfileModified"])
        self.assertFalse(camera["manualExposureModified"])
        self.assertFalse(camera["shutterIsoApertureWhiteBalanceModified"])
        self.assertTrue(camera["sameComponentSettingsResubmittedAtFinalCameraStage"])
        self.assertEqual("VTBlendOrder_Override", camera["cachedBlendOrder"])
        for token in (
            "HasExpectedExploreV5CameraProfile()",
            "CameraManager->AddCachedPPBlend(",
            "VTBlendOrder_Override",
            "(void)InOutPOV",
            "Settings.bOverride_CameraShutterSpeed",
            "Settings.bOverride_CameraISO",
            "Settings.bOverride_DepthOfFieldFstop",
            "Settings.bOverride_AutoExposureMethod",
        ):
            self.assertIn(token, self.actor_cpp)
        modifier_body = self.actor_cpp.split(
            "UTRIADIstanaExploreV5DDynamicRangeCameraModifier::ModifyCamera", 1
        )[1].split(
            "ATRIADIstanaExploreV5DDynamicRangeRealismActor::", 1
        )[0]
        self.assertNotRegex(modifier_body, r"InOutPOV\s*\.")
        self.assertNotIn("PostProcessSettings =", modifier_body)

    def test_truth_boundary_excludes_simulation_and_measurement_claims(self):
        boundary = self.contract["claimBoundary"]
        self.assertTrue(boundary["appearanceOnly"])
        for key, value in boundary.items():
            if key != "appearanceOnly":
                self.assertFalse(value, key)
        combined = self.actor_h + self.actor_cpp
        for token in (
            "bCollisionNavigationSensorOrRfAuthority = false",
            "bMaterialsOrEmissiveRetuned = false",
            "bSurveyDisplayCalibrationOrCurrentWeatherClaimed = false",
            "bPawnPhysicalCameraProfileModified = false",
        ):
            self.assertIn(token, combined)
        self.assertNotRegex(
            self.actor_cpp + self.editor_cpp,
            r"SetMaterial\(|CreateDynamicMaterialInstance|SetScalarParameterValue\([^\n]*Emissive|LineTrace|SweepSingle|SweepMulti|SetCollision",
        )

    def test_editor_hooks_fail_closed_and_keep_blueprint_map_exact(self):
        public_hook = "ApplyDynamicRangeRealismPassToLoadedV5DHybridMap"
        trusted_hook = (
            "ApplyDynamicRangeRealismPassToWorldForTrustedHybridBuilder"
        )
        self.assertIn(public_hook, self.editor_h)
        self.assertIn(trusted_hook, self.editor_h)
        self.assertIn("FPackageName::IsTempPackage", self.editor_cpp)
        self.assertIn("bTrustedUntitledHybridBuilder", self.editor_cpp)
        public_decl = self.editor_h.split(public_hook, 1)[0]
        self.assertIn("UFUNCTION(BlueprintCallable", public_decl)
        trusted_decl = self.editor_h.split(trusted_hook, 1)[1].split(";", 1)[0]
        self.assertNotIn("UFUNCTION", trusted_decl)
        self.assertNotRegex(
            self.editor_cpp,
            r"SaveMap|DeleteAsset|DeleteDirectory|ConsolidateAssets",
        )

    def test_runtime_is_bounded_revalidated_and_native_tested(self):
        for token in (
            "MaximumRuntimeActivationAttempts = 8",
            "RuntimeActivationAttempts >= MaximumRuntimeActivationAttempts",
            "DeactivateRuntimeFinalCameraOverride()",
            "ValidateDynamicRangeRealism(true, Report)",
            "TRIAD.Istana.ExploreV5D.DynamicRange.ExpectedSettings",
        ):
            self.assertIn(token, self.actor_h + self.actor_cpp)
        self.assertGreaterEqual(
            len(re.findall(r"TestTrue\(|TestFalse\(|TestEqual\(", self.actor_cpp)),
            10,
        )

    def test_documented_hybrid_include_and_hook_are_exact(self):
        self.assertIn(
            '#include "TRIADIstanaExploreV5DDynamicRangeRealismEditorLibrary.h"',
            self.docs,
        )
        self.assertIn(
            "ApplyDynamicRangeRealismPassToWorldForTrustedHybridBuilder",
            self.docs,
        )
        self.assertIn("Target,", self.docs)
        self.assertIn("true,", self.docs)


if __name__ == "__main__":
    unittest.main()
