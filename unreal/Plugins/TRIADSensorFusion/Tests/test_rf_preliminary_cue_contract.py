"""Source-contract regression tests for the Unreal RF preliminary cue path.

These tests are intentionally engine-independent so they can run while an editor
session is open. Unreal compilation remains the authoritative C++ integration
check; this suite prevents scenario-truth gating or hostile operator wording from
being accidentally reintroduced between editor builds.
"""

from __future__ import annotations

import re
import unittest
from pathlib import Path


PLUGIN_ROOT = Path(__file__).resolve().parents[1]
SOURCE_ROOT = PLUGIN_ROOT / "Source" / "TRIADSensorFusion"
MANAGER_CPP = (
    SOURCE_ROOT / "Private" / "TRIADSensorFusionScenarioManager.cpp"
).read_text(encoding="utf-8")
NODE_CPP = (SOURCE_ROOT / "Private" / "TRIADSensorNodeActor.cpp").read_text(
    encoding="utf-8"
)
NODE_H = (SOURCE_ROOT / "Public" / "TRIADSensorNodeActor.h").read_text(
    encoding="utf-8"
)


def function_body(source: str, signature: str, next_signature: str) -> str:
    start = source.index(signature)
    end = source.index(next_signature, start)
    return source[start:end]


class RFPreliminaryCueContractTests(unittest.TestCase):
    def test_all_detected_emitters_can_contribute_nodes(self) -> None:
        self.assertIn("if (bDetectedByNode)", MANAGER_CPP)
        self.assertNotRegex(
            MANAGER_CPP,
            re.compile(
                r"if\s*\(\s*bDetectedByNode\s*&&[^)]*bHostileScenarioTruth"
            ),
        )

    def test_multinode_threshold_does_not_consult_scenario_truth(self) -> None:
        sample_body = function_body(
            MANAGER_CPP,
            "void ATRIADSensorFusionScenarioManager::SampleScenarioNow()",
            "double ATRIADSensorFusionScenarioManager::ComputeSignedDistanceToSimulationPerimeterMeters(",
        )
        condition = re.search(
            r"if\s*\(Emitter\s*&&\s*ConfirmingNodeDistances\s*&&\s*MaximumSnrDb\s*&&\s*"
            r"ConfirmingNodeDistances->Num\(\)\s*>=\s*MinimumConfirmingNodes\)",
            sample_body,
        )
        self.assertIsNotNone(condition)
        self.assertNotIn("bHostileScenarioTruth", condition.group(0))
        self.assertIn("EmitRFEarlyWarningCue", sample_body)

    def test_operator_cue_payload_is_generic_and_truth_free(self) -> None:
        cue_body = function_body(
            MANAGER_CPP,
            "void ATRIADSensorFusionScenarioManager::EmitRFEarlyWarningCue(",
            "void ATRIADSensorFusionScenarioManager::AppendAlertRecord(",
        )
        self.assertIn("PRELIMINARY RF EARLY WARNING", cue_body)
        self.assertIn('TEXT("rf_multinode_preliminary_cue")', cue_body)
        self.assertIn('TEXT("unclassified_emitter")', cue_body)
        self.assertIn('TEXT("not_inferred")', cue_body)
        self.assertIn('TEXT("none")', cue_body)
        self.assertNotIn('TEXT("simulated_hostile_rf_multinode")', cue_body)
        self.assertNotIn('TEXT("hostileScenarioTruth")', cue_body)
        self.assertNotIn("Definition.bHostileScenarioTruth", cue_body)
        self.assertNotIn('SetStringField(TEXT("targetActor")', cue_body)
        self.assertNotIn('SetStringField(TEXT("emitterId")', cue_body)
        self.assertNotIn('SetStringField(TEXT("ingressCorridorId")', cue_body)
        self.assertIn('SetStringField(TEXT("trackId"), OperatorTrackId)', cue_body)
        self.assertIn("*OperatorTrackId", cue_body)
        self.assertIn('SetBoolField(TEXT("positionEstimateAvailable"), false)', cue_body)
        self.assertIn('SetBoolField(TEXT("approachEstimateAvailable"), false)', cue_body)
        self.assertIn('SetBoolField(TEXT("bearingEstimateAvailable"), false)', cue_body)
        self.assertIn('TEXT("unavailable_at_raw_rf_cue_stage")', cue_body)
        for evaluator_field in (
            "TargetLongitudeLatitudeHeight",
            "CurrentTargetApproachStatusByActor",
            "ApproachText",
            'TEXT("targetLongitudeDegrees")',
            'TEXT("targetLatitudeDegrees")',
            'TEXT("targetHeightMeters")',
            'TEXT("airspaceState")',
            'TEXT("distanceToPerimeterMeters")',
            'TEXT("approachRateMetersPerSecond")',
            'TEXT("headingDegrees")',
            'TEXT("speedMetersPerSecond")',
            'TEXT("outsideSimulationPerimeter")',
        ):
            self.assertNotIn(evaluator_field, cue_body)

        # Scan emitted constant values (not field names) for authored
        # hostile/friendly/threat wording.
        emitted_values = re.findall(
            r'SetStringField\(TEXT\("[^"]+"\),\s*TEXT\("([^"]*)"\)\)',
            cue_body,
        )
        emitted_values += re.findall(
            r'const FString Message = FString::Printf\(\s*TEXT\("([^"]*)"\)',
            cue_body,
        )
        self.assertTrue(emitted_values)
        for value in emitted_values:
            self.assertNotRegex(value.casefold(), r"\b(?:hostile|friendly|threat)\b")

    def test_operator_identifier_is_a_deterministic_opaque_pseudonym(self) -> None:
        helper_body = function_body(
            MANAGER_CPP,
            "FString MakeOperatorRFCueTrackId(",
            "struct FResolvedSingaporeWeather",
        )
        self.assertIn("FCrc::StrCrc32", helper_body)
        self.assertIn('TEXT("RF-CUE-%08X-%08X")', helper_body)
        self.assertNotRegex(helper_body, r"return\s+(?:TargetSeed|EmitterSeed|Seed)\s*;")

    def test_record_cap_does_not_suppress_operator_display(self) -> None:
        cue_body = function_body(
            MANAGER_CPP,
            "void ATRIADSensorFusionScenarioManager::EmitRFEarlyWarningCue(",
            "void ATRIADSensorFusionScenarioManager::AppendAlertRecord(",
        )
        self.assertNotIn("AlertRecordsWritten", cue_body)
        self.assertIn("GEngine->AddOnScreenDebugMessage", cue_body)
        self.assertIn("AppendAlertRecord(Alert)", cue_body)

    def test_camera_slew_is_downstream_of_this_nodes_rf_detection(self) -> None:
        sample_body = function_body(
            MANAGER_CPP,
            "void ATRIADSensorFusionScenarioManager::SampleScenarioNow()",
            "double ATRIADSensorFusionScenarioManager::ComputeSignedDistanceToSimulationPerimeterMeters(",
        )
        detection_index = sample_body.index("bDetectedByNode = SampleNodeTargetLink(")
        detected_branch_index = sample_body.index("if (bDetectedByNode)", detection_index)
        selection_index = sample_body.index("NearestRFDetectedTarget = Target", detected_branch_index)
        aim_index = sample_body.index("Node->AimCamerasAtWorldLocation", selection_index)
        self.assertLess(detection_index, detected_branch_index)
        self.assertLess(detected_branch_index, selection_index)
        self.assertLess(selection_index, aim_index)
        self.assertNotIn("AimCamerasAtWorldLocation", sample_body[:detection_index])
        self.assertIn("if (NearestRFDetectedTarget)", sample_body[selection_index:aim_index])

    def test_node_status_tracks_all_detected_frequencies(self) -> None:
        operator_surface = NODE_H + "\n" + NODE_CPP
        self.assertIn("SampleDetectedFrequenciesGHz.AddUnique(FrequencyGHz)", operator_surface)
        self.assertIn("for (const double FrequencyGHz : SampleDetectedFrequenciesGHz)", operator_surface)
        self.assertIn("RF WIDEBAND %d CH", operator_surface)
        self.assertNotIn("bSampleDetected2_4GHz", operator_surface)
        self.assertNotIn("bSampleDetected5_8GHz", operator_surface)

    def test_live_snapshot_exposes_truth_independent_cue_rule(self) -> None:
        self.assertIn('TEXT("rfMultinodePreliminaryCueRuleSatisfied")', MANAGER_CPP)
        self.assertNotIn(
            'TEXT("detectionOnlyMultinodeRuleSatisfiedUsingScenarioTruth")',
            MANAGER_CPP,
        )
        rule = re.search(
            r'TEXT\("rfMultinodePreliminaryCueRuleSatisfied"\),\s*([^;]+)\);',
            MANAGER_CPP,
        )
        self.assertIsNotNone(rule)
        self.assertIn("PreliminaryCueEvidenceNodeIds.Num()", rule.group(1))
        self.assertNotIn("Hostile", rule.group(1))

    def test_node_operator_labels_do_not_claim_hostility(self) -> None:
        operator_surface = NODE_H + "\n" + NODE_CPP
        self.assertIn("RF MULTI-NODE CUE", operator_surface)
        self.assertNotIn("RF MULTI-NODE HOSTILE", operator_surface)
        self.assertNotIn("hostile evidence", operator_surface.lower())


if __name__ == "__main__":
    unittest.main()
