from __future__ import annotations

from singapore_sensor_fusion.radar_ptz_audit import run_audit


def test_deterministic_audit_passes_all_required_clear_ranges_and_negative_gates() -> None:
    report = run_audit()

    assert report["result"] == "PASS"
    assert report["clearConditionReliabilityRequirement"]["passed"] is True
    required = [
        item for item in report["clearRangeResults"] if item["rangeMeters"] <= 500.0
    ]
    assert [item["rangeMeters"] for item in required] == [25.0, 100.0, 250.0, 500.0]
    assert all(item["trialCount"] == 8 for item in required)
    assert all(item["passRate"] == 1.0 for item in required)
    assert report["preferredOneKilometerSimulationCheckPassed"] is True
    assert all(item["passed"] is True for item in report["negativeCases"].values())


def test_audit_covers_weather_and_preserves_non_model_provenance() -> None:
    report = run_audit()

    assert {item["weatherId"] for item in report["weatherAt500Meters"]} == {
        "CLEAR",
        "MONSOON",
        "DENSE_FOG",
    }
    assert all(item["operatorCueCount"] == 8 for item in report["weatherAt500Meters"])
    assert report["provenanceInvariant"]["boxKind"] == "SIMULATED_SENSOR_CONFIRMATION"
    assert report["provenanceInvariant"]["boxSource"] == "SIMULATION_PROJECTION"
    assert report["provenanceInvariant"]["learnedModelOutput"] is False
    assert report["physicalSensorValidation"] is False
    assert report["learnedModelEvaluation"] is False
