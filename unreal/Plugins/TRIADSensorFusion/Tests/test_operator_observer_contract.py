"""Static contracts for the Play-time sensor-gated operator observer.

These tests intentionally keep scenario truth separate from sensor evidence.
They run without Unreal Editor and complement the compiled/runtime check.
"""

from __future__ import annotations

import json
import unittest
from pathlib import Path


PLUGIN_ROOT = Path(__file__).resolve().parents[1]
PROJECT_ROOT = PLUGIN_ROOT.parents[1]
SOURCE_ROOT = PLUGIN_ROOT / "Source" / "TRIADSensorFusion"
MANAGER = (SOURCE_ROOT / "Private" / "TRIADSensorFusionScenarioManager.cpp").read_text(encoding="utf-8")
OBSERVER = (SOURCE_ROOT / "Private" / "TRIADOperatorObserverActor.cpp").read_text(encoding="utf-8")
WIDGET = (SOURCE_ROOT / "Private" / "TRIADOperatorSlateWidget.cpp").read_text(encoding="utf-8")
NODE = (SOURCE_ROOT / "Private" / "TRIADSensorNodeActor.cpp").read_text(encoding="utf-8")
TYPES = (SOURCE_ROOT / "Public" / "TRIADSensorFusionTypes.h").read_text(encoding="utf-8")
CONFIG = json.loads((PROJECT_ROOT / "Config" / "SingaporeSensorFusion.json").read_text(encoding="utf-8"))


def function_body(source: str, signature: str, next_signature: str) -> str:
    start = source.index(signature)
    end = source.index(next_signature, start)
    return source[start:end]


class OperatorObserverContractTests(unittest.TestCase):
    def test_observer_auto_starts_after_nodes_and_targets(self) -> None:
        begin_play = function_body(
            MANAGER,
            "void ATRIADSensorFusionScenarioManager::BeginPlay()",
            "void ATRIADSensorFusionScenarioManager::EndPlay(",
        )
        self.assertLess(begin_play.index("SpawnSensorNodes()"), begin_play.index("SpawnOperatorObserver()"))
        self.assertLess(begin_play.index("SpawnDemoTargets()"), begin_play.index("SpawnOperatorObserver()"))
        self.assertTrue(CONFIG["OperatorObserver"]["bEnabled"])

    def test_contacts_require_positive_current_sensor_evidence(self) -> None:
        update = function_body(
            OBSERVER,
            "void ATRIADOperatorObserverActor::UpdateDetectedContacts(",
            "void ATRIADOperatorObserverActor::SelectBestOrRetainedContact(",
        )
        self.assertIn("Contact.TargetActor.IsValid()", update)
        self.assertIn("Contact.ReportingNode.IsValid()", update)
        self.assertIn("!Contact.ContactId.IsEmpty()", update)
        self.assertIn("Contact.bDetectedBySearchRadar || Contact.bDetectedByRF", update)
        self.assertIn("CurrentContacts.Reset()", update)

        sample = function_body(
            MANAGER,
            "void ATRIADSensorFusionScenarioManager::SampleScenarioNow()",
            "double ATRIADSensorFusionScenarioManager::ComputeSignedDistanceToSimulationPerimeterMeters(",
        )
        self.assertIn("if (bDetectedBySearchRadar || bDetectedByNode)", sample)
        self.assertIn("OperatorObserver->UpdateDetectedContacts", sample)

    def test_contact_ranking_cannot_use_authored_truth_or_route(self) -> None:
        score = function_body(
            OBSERVER,
            "int32 ATRIADOperatorObserverActor::ScoreContact(",
            "void ATRIADOperatorObserverActor::UpdateDetectedContacts(",
        )
        for forbidden in (
            "bHostileScenarioTruth",
            "bAuthoredAttackScenario",
            "bInboundApproachScenario",
            "AirspaceState",
            "bOutsideSimulationPerimeter",
        ):
            self.assertNotIn(forbidden, score)
        self.assertIn("bDetectedBySearchRadar", score)
        self.assertIn("bDetectedByRF", score)

    def test_precise_minimap_position_requires_radar_measurement(self) -> None:
        self.assertIn("MeasuredRangeMeters", MANAGER)
        self.assertIn("MeasuredBearingDegrees", MANAGER)
        self.assertIn("MeasuredElevationDegrees", MANAGER)
        self.assertIn("Contact.bHasRadarPositionEstimate = true", MANAGER)
        self.assertIn("if (Contact->bHasRadarPositionEstimate)", WIDGET)
        self.assertIn("RF-ONLY CONTACT - PRECISE MAP POSITION UNAVAILABLE", WIDGET)
        self.assertIn("WEATHER  %s | SIMULATED ENVIRONMENT", WIDGET)

    def test_sensor_image_is_current_confirmed_eo_only(self) -> None:
        fresh_view = function_body(
            NODE,
            "bool ATRIADSensorNodeActor::TryGetFreshOperatorEOView(",
            "void ATRIADSensorNodeActor::UpdateLongRangePTZ(",
        )
        for requirement in (
            "Confirmation.TrackId == TrackId",
            "Confirmation.Modality == TEXT(\"EO_VISIBLE\")",
            "Confirmation.bConfirmed",
            "Confirmation.bLineOfSight",
            "Confirmation.CueAgeSeconds <= FreshnessSeconds",
            "FrameAgeSeconds <= FreshnessSeconds",
        ):
            self.assertIn(requirement, fresh_view)
        self.assertIn("NO FRESH EO CONFIRMATION", WIDGET)

    def test_truth_camera_is_explicitly_labelled_and_ui_fails_closed(self) -> None:
        self.assertIn("SIMULATION OBSERVER VIEW - NOT SENSOR EVIDENCE", WIDGET)
        self.assertIn("PRESENTATION RETICLE - NOT A DETECTOR BOX", WIDGET)
        self.assertIn("YELLOW OUTLINE = SIM PROJECTION, NOT MODEL OUTPUT", WIDGET)
        self.assertIn("WAITING FOR CURRENT SENSOR CONTACT", WIDGET)
        self.assertIn("No drone view or map position is shown without fresh RF/radar evidence.", WIDGET)
        self.assertIn("NO CURRENT SENSOR DETECTION", WIDGET)

    def test_observer_feed_excludes_operator_debug_geometry(self) -> None:
        self.assertIn("ObserverCapture->HideComponent(World->LineBatcher.Get())", OBSERVER)
        self.assertIn("ObserverCapture->HideComponent(World->PersistentLineBatcher.Get())", OBSERVER)
        self.assertIn('ComponentHasTag(TEXT("TRIADHumanOnlyOverlay"))', OBSERVER)
        self.assertIn("GEngine->bEnableOnScreenDebugMessages = false", OBSERVER)
        self.assertIn("FlushDebugStrings(GetWorld())", OBSERVER)

    def test_one_opaque_contact_id_joins_all_modalities(self) -> None:
        self.assertIn("FString ContactId;", TYPES)
        self.assertGreaterEqual(MANAGER.count('SetStringField(TEXT("contactId")'), 4)
        self.assertIn("Pending.ContactId = ContactId", NODE)
        self.assertIn("Result.ContactId = CurrentSensorContactId", NODE)
        self.assertIn('Metadata->SetStringField(TEXT("contactId"), Result.ContactId)', NODE)

    def test_many_contacts_cycle_and_overlay_cleanup(self) -> None:
        self.assertIn("ContactCycleSeconds", OBSERVER)
        self.assertIn("SelectBestOrRetainedContact(true)", OBSERVER)
        self.assertIn("EKeys::N", OBSERVER)
        self.assertIn("RemoveViewportWidgetContent", OBSERVER)
        self.assertIn("CurrentContacts.Reset()", OBSERVER)

    def test_runtime_validation_capture_is_explicitly_opt_in(self) -> None:
        self.assertIn('FParse::Param(FCommandLine::Get(), TEXT("TRIADCaptureObserver"))', OBSERVER)
        self.assertIn("FScreenshotRequest::RequestScreenshot", OBSERVER)
        self.assertIn("bValidationScreenshotRequested", OBSERVER)


if __name__ == "__main__":
    unittest.main()
