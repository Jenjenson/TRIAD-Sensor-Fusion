from __future__ import annotations

from copy import deepcopy
from datetime import datetime, timedelta, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
import threading

import pytest

from singapore_sensor_fusion.placement.contracts import stable_digest
from singapore_sensor_fusion.placement.dashboard_app import (
    LiveSnapshotClient,
    LiveSnapshotMalformedError,
    LiveSnapshotModel,
    LiveSnapshotUnavailableError,
    MapProjector,
    ReplayModel,
    _mode_banner_text,
    _solver_proof_text,
    build_default_replay,
    location_to_enu,
    main,
    validate_live_snapshot_url,
)
from singapore_sensor_fusion.placement.dashboard_demo_adapter import (
    build_dashboard_replay_from_demo_study,
)
from singapore_sensor_fusion.placement.demo_study import build_four_class_demo_study


def _model() -> ReplayModel:
    return ReplayModel(
        build_default_replay(
            scenario_id="clear-rf",
            duration_seconds=4.0,
            cadence_seconds=1.0,
        )
    )


def _live_snapshot(
    model: ReplayModel,
    *,
    freshness: str = "fresh",
    mode: str = "live-local",
) -> dict[str, object]:
    nodes = []
    for sensor in model.replay["sensors"]:
        location = sensor["location"]
        nodes.append(
            {
                "id": sensor["nodeId"],
                "short": sensor["siteId"],
                "latitudeDegrees": location["latitudeDegrees"],
                "longitudeDegrees": location["longitudeDegrees"],
                "heightMeters": location["heightMeters"],
                "state": "tracking",
                "longRangeSensors": [
                    {
                        "sensorId": f"{sensor['nodeId']}:SEARCH_RADAR",
                        "sensorType": "SEARCH_RADAR",
                        "status": "ONLINE",
                    }
                ],
            }
        )
    first = nodes[0]
    tracks = [] if mode == "unavailable" else [
        {
            "id": "RF-CUE-test",
            "latitudeDegrees": float(first["latitudeDegrees"]) + 0.0001,
            "longitudeDegrees": first["longitudeDegrees"],
            "altitudeMeters": 100.0,
            "ageSeconds": 0.1,
            "freshness": "fresh",
            "ueRfDecision": "ALERT",
            "confirmingNodeIds": [first["id"]],
            "activeModalities": ["SEARCH_RADAR"],
            "rfLinks": [
                {
                    "nodeId": first["id"],
                    "detected": True,
                    "rangeMeters": 120.0,
                }
            ],
            "fusionV3": {
                "decision": "CONFIRMED_TRACK",
                "operatorCueActive": True,
            },
            "distanceMeters": 120.0,
        }
    ]
    return {
        "schemaVersion": "1.0",
        "generatedAtUtc": datetime.now(timezone.utc).isoformat(
            timespec="milliseconds"
        ).replace("+00:00", "Z"),
        "mode": mode,
        "detectionOnly": True,
        "actionsTaken": "none",
        "sourceFreshness": {
            "state": freshness,
            "newestTelemetryAgeSeconds": {
                "fresh": 0.1,
                "delayed": 10.0,
                "stale": 75.0,
                "unavailable": None,
            }[freshness],
            "freshThresholdSeconds": 5.0,
            "staleThresholdSeconds": 60.0,
            "notice": f"fixture is {freshness}",
        },
        "sources": {
            "selectedRFSource": "atomic-live-snapshot",
            "nodeConfiguration": "fixture",
        },
        "nodes": nodes,
        "tracks": tracks,
    }


def test_local_projection_preserves_cardinal_direction_and_scale() -> None:
    projector = MapProjector(800.0, 600.0, 1000.0, padding_pixels=50.0)
    center = projector.project(0.0, 0.0)
    east = projector.project(1000.0, 0.0)
    north = projector.project(0.0, 1000.0)

    assert center == (400.0, 300.0)
    assert east[0] > center[0]
    assert north[1] < center[1]
    assert math.isclose(east[0] - center[0], center[1] - north[1])


def test_wgs84_offset_projects_to_finite_local_enu() -> None:
    origin = {
        "latitudeDegrees": 1.30709615,
        "longitudeDegrees": 103.84288055,
        "heightMeters": 47.0,
    }
    location = {
        "latitudeDegrees": 1.30809615,
        "longitudeDegrees": 103.84388055,
        "heightMeters": 67.0,
    }

    east, north, up = location_to_enu(location, origin)

    assert 110.0 < east < 112.0
    assert 111.0 < north < 112.0
    assert up == 20.0


def test_playback_interpolates_tracks_and_stops_at_the_end() -> None:
    model = _model()
    first_track = model.tracks[0]
    first = model.track_state(first_track)
    model.seek(0.5)
    halfway = model.track_state(first_track)
    model.seek(1.0)
    second = model.track_state(first_track)

    first_lon = float(first["location"]["longitudeDegrees"])
    second_lon = float(second["location"]["longitudeDegrees"])
    assert math.isclose(
        float(halfway["location"]["longitudeDegrees"]),
        (first_lon + second_lon) * 0.5,
        rel_tol=0.0,
        abs_tol=1e-10,
    )

    model.seek(3.5)
    model.speed = 2.0
    model.playing = True
    assert model.advance(1.0) is True
    assert model.current_seconds == model.duration_seconds
    assert model.playing is False


def test_detection_windows_and_recent_feed_are_bounded() -> None:
    model = _model()
    model.seek(0.6)
    assert model.active_detections() == ()
    model.seek(2.0)

    assert all(
        2.0 - 1.05 - 1e-9 <= float(item["tSeconds"]) <= 2.0 + 1e-9
        for item in model.active_detections()
    )
    recent = model.recent_detections(3)
    assert len(recent) <= 3
    assert all(float(item["tSeconds"]) <= 2.0 + 1e-9 for item in recent)
    assert model.detected_track_ids().issubset(
        {str(item["trackId"]) for item in model.tracks}
    )


def test_default_dashboard_replay_keeps_fail_closed_evidence_labels() -> None:
    model = _model()

    assert model.replay["simulationOnly"] is True
    assert model.replay["detectionOnly"] is True
    assert model.replay["operationalUseAuthorized"] is False
    assert model.replay["calibratedProbabilities"] is False
    assert model.replay["timeline"]["successSummary"]["allTracksDetected"] is True


def test_demo_banner_and_solver_proof_text_are_explicit_and_complete() -> None:
    replay = {
        "stage0Scope": {"executionClass": "DEMO_STUDY_ONLY"},
    }
    recommendation = {
        "solverProof": {
            "terminationReason": "SEARCH_EXHAUSTED_OPTIMAL",
            "incumbentObjectiveCostUnits": 4.4,
            "objectiveCostLowerBoundUnits": 4.4,
            "absoluteCostOptimalityGapUnits": 0.0,
            "relativeCostOptimalityGap": 0.0,
            "nodeLimitReached": False,
            "exploredNodeCount": 17,
            "nodeLimit": 100_000,
        }
    }

    banner = _mode_banner_text(replay, live=False)
    proof = _solver_proof_text(recommendation)

    assert "SYNTHETIC DEMO_STUDY_ONLY" in banner
    assert "NO DEPLOYMENT" in banner
    assert proof is not None
    assert "Termination: SEARCH_EXHAUSTED_OPTIMAL" in proof
    assert "Incumbent: 4.400 cost units" in proof
    assert "conservative lower bound: 4.400" in proof
    assert "Absolute gap: 0.000" in proof
    assert "relative gap: 0.000%" in proof
    assert "Node limit: NOT REACHED · 17 explored / 100,000 limit" in proof


def test_validate_only_cli_checks_replay_without_opening_tk(capsys) -> None:
    assert main(
        [
            "--scenario-id",
            "clear-rf",
            "--duration-seconds",
            "2",
            "--cadence-seconds",
            "1",
            "--validate-only",
        ]
    ) == 0
    summary = capsys.readouterr().out
    assert '"schemaVersion":"triad.dashboard_replay.v1"' in summary
    assert '"allTracksDetected":true' in summary
    assert '"simulationOnly":true' in summary


def test_validate_only_cli_exposes_exact_solver_proof(tmp_path, capsys) -> None:
    replay = build_dashboard_replay_from_demo_study(build_four_class_demo_study())
    replay_path = tmp_path / "proof-replay.json"
    replay_path.write_text(json.dumps(replay), encoding="utf-8")

    assert main(["--replay", str(replay_path), "--validate-only"]) == 0
    summary = json.loads(capsys.readouterr().out)

    assert summary["solverProof"] == replay["recommendation"]["solverProof"]
    assert summary["solverProof"]["terminationReason"] == "SEARCH_EXHAUSTED_OPTIMAL"
    assert summary["solverProof"]["nodeLimitReached"] is False


@pytest.mark.parametrize(
    "url",
    (
        "https://127.0.0.1:8765/api/snapshot",
        "http://192.0.2.10:8765/api/snapshot",
        "http://example.test/api/snapshot",
        "http://user:secret@127.0.0.1:8765/api/snapshot",
        "http://127.0.0.1:8765/health",
        "http://127.0.0.1:8765/api/snapshot?unsafe=true",
    ),
)
def test_live_url_rejects_every_non_exact_loopback_endpoint(url: str) -> None:
    with pytest.raises(ValueError):
        validate_live_snapshot_url(url)


@pytest.mark.parametrize(
    "url",
    (
        "http://127.0.0.1:8765/api/snapshot",
        "http://127.9.8.7:8765/api/snapshot",
        "http://localhost:8765/api/snapshot",
        "http://[::1]:8765/api/snapshot",
    ),
)
def test_live_url_accepts_only_explicit_loopback_snapshot_routes(url: str) -> None:
    assert validate_live_snapshot_url(url) == url


def test_live_time_and_poll_bounds_fail_before_any_request() -> None:
    with pytest.raises(ValueError, match="live timeout"):
        LiveSnapshotClient(
            "http://127.0.0.1:8765/api/snapshot",
            timeout_seconds=5.01,
        )
    with pytest.raises(ValueError, match="live-poll-seconds"):
        main(
            [
                "--live-url",
                "http://127.0.0.1:8765/api/snapshot",
                "--live-poll-seconds",
                "0.01",
                "--validate-only",
            ]
        )


def test_fresh_live_snapshot_matches_reference_and_exposes_current_cues() -> None:
    replay = _model()
    live = LiveSnapshotModel(replay.replay)

    assert live.accept_snapshot(_live_snapshot(replay)) is True
    assert live.source_state == "fresh"
    assert live.comparison.status == "matched"
    assert live.reference_deployment_claim_authorized is True
    assert live.detection_rendering_enabled is True
    assert len(live.tracks_for_display) == 1
    assert live.cue_links() == (
        (str(replay.replay["sensors"][0]["nodeId"]), "RF-CUE-test"),
    )


def test_local_monotonic_age_expires_tracks_cues_and_reference_authorization() -> None:
    replay = _model()
    clock = [100.0]
    wall_clock = datetime(2026, 9, 7, 4, 0, tzinfo=timezone.utc)
    snapshot = _live_snapshot(replay)
    snapshot["generatedAtUtc"] = wall_clock.isoformat().replace("+00:00", "Z")
    snapshot["sourceFreshness"]["newestTelemetryAgeSeconds"] = 0.0
    snapshot["sourceFreshness"]["freshThresholdSeconds"] = 0.05
    snapshot["sourceFreshness"]["staleThresholdSeconds"] = 0.20
    snapshot["tracks"][0]["ageSeconds"] = 0.0
    live = LiveSnapshotModel(
        replay.replay,
        monotonic_clock=lambda: clock[0],
        utc_clock=lambda: wall_clock,
    )

    assert live.accept_snapshot(snapshot) is True
    assert live.reference_deployment_claim_authorized is True
    assert len(live.tracks_for_display) == 1
    clock[0] += 0.051
    assert live.effective_source_state == "delayed"
    assert live.tracks_for_display == ()
    assert live.cue_links() == ()
    assert live.effective_comparison.status == "not-comparable"
    assert live.reference_deployment_claim_authorized is False

    # Polling the exact same cached body cannot reset its monotonic receipt age.
    assert live.accept_snapshot(snapshot) is True
    assert live.effective_source_state == "delayed"
    assert live.reference_deployment_claim_authorized is False
    clock[0] += 0.150
    assert live.effective_source_state == "stale"


def test_generated_timestamp_replay_retrogression_and_old_envelope_fail_closed() -> None:
    replay = _model()
    clock = [20.0]
    wall_clock = datetime(2026, 9, 7, 4, 0, tzinfo=timezone.utc)
    snapshot = _live_snapshot(replay)
    snapshot["generatedAtUtc"] = wall_clock.isoformat().replace("+00:00", "Z")
    live = LiveSnapshotModel(
        replay.replay,
        monotonic_clock=lambda: clock[0],
        utc_clock=lambda: wall_clock,
    )
    assert live.accept_snapshot(snapshot) is True

    changed_replay = deepcopy(snapshot)
    changed_replay["tracks"][0]["ueRfDecision"] = "NO_DETECTION"
    assert live.accept_snapshot(changed_replay) is False
    assert live.source_state == "malformed"
    assert live.tracks_for_display == ()

    recovered = deepcopy(snapshot)
    recovered["generatedAtUtc"] = (
        wall_clock + timedelta(seconds=1)
    ).isoformat().replace("+00:00", "Z")
    wall_clock += timedelta(seconds=1)
    assert live.accept_snapshot(recovered) is True

    retrogressed = deepcopy(snapshot)
    retrogressed["generatedAtUtc"] = (
        wall_clock - timedelta(milliseconds=500)
    ).isoformat().replace("+00:00", "Z")
    assert live.accept_snapshot(retrogressed) is False
    assert live.source_state == "malformed"

    old_model = LiveSnapshotModel(
        replay.replay,
        monotonic_clock=lambda: clock[0],
        utc_clock=lambda: wall_clock,
    )
    old_snapshot = deepcopy(snapshot)
    old_snapshot["generatedAtUtc"] = (
        wall_clock - timedelta(seconds=10.01)
    ).isoformat().replace("+00:00", "Z")
    assert old_model.accept_snapshot(old_snapshot) is False
    assert old_model.source_state == "malformed"


def test_live_coordinate_mismatch_never_authorizes_reference_claim() -> None:
    replay = _model()
    snapshot = _live_snapshot(replay)
    snapshot["nodes"][0]["longitudeDegrees"] += 0.001
    live = LiveSnapshotModel(replay.replay)

    assert live.accept_snapshot(snapshot) is True
    assert live.source_state == "fresh"
    assert live.comparison.status == "mismatched"
    assert live.comparison.coordinate_mismatches
    assert live.reference_deployment_claim_authorized is False
    # Current telemetry remains viewable, but it is never attributed to the reference.
    assert live.detection_rendering_enabled is True


def test_live_node_roster_and_height_datum_must_both_match() -> None:
    replay = _model()
    missing_node_snapshot = _live_snapshot(replay)
    removed = missing_node_snapshot["nodes"].pop()
    live = LiveSnapshotModel(replay.replay)
    assert live.accept_snapshot(missing_node_snapshot) is True
    assert live.comparison.status == "mismatched"
    assert live.comparison.missing_node_ids == (removed["id"],)
    assert live.reference_deployment_claim_authorized is False

    incompatible_reference = deepcopy(replay.replay)
    incompatible_reference["sensors"][0]["location"]["heightReference"] = (
        "SYNTHETIC_LOCAL_UP_NOT_SURVEY_DATUM"
    )
    incompatible_reference.pop("replayDigest")
    incompatible_reference["replayDigest"] = stable_digest(incompatible_reference)
    datum_model = LiveSnapshotModel(incompatible_reference)
    assert datum_model.accept_snapshot(_live_snapshot(replay)) is True
    assert datum_model.comparison.status == "mismatched"
    assert any(
        "height datum" in item
        for item in datum_model.comparison.coordinate_mismatches
    )
    assert datum_model.reference_deployment_claim_authorized is False


@pytest.mark.parametrize("freshness", ("delayed", "stale"))
def test_nonfresh_live_snapshot_suppresses_tracks_cues_and_match_claim(
    freshness: str,
) -> None:
    replay = _model()
    live = LiveSnapshotModel(replay.replay)

    assert live.accept_snapshot(_live_snapshot(replay, freshness=freshness)) is True
    assert live.source_state == freshness
    assert live.nodes_for_display
    assert live.tracks_for_display == ()
    assert live.cue_links() == ()
    assert live.comparison.status == "not-comparable"
    assert live.reference_deployment_claim_authorized is False


def test_unavailable_and_malformed_live_states_fail_closed() -> None:
    replay = _model()
    live = LiveSnapshotModel(replay.replay)
    unavailable = _live_snapshot(
        replay,
        freshness="unavailable",
        mode="unavailable",
    )
    assert live.accept_snapshot(unavailable) is True
    assert live.source_state == "unavailable"
    assert live.nodes_for_display == ()
    assert live.tracks_for_display == ()

    malformed = _live_snapshot(replay)
    malformed["actionsTaken"] = "engage"
    assert live.accept_snapshot(malformed) is False
    assert live.source_state == "malformed"
    assert live.snapshot is None
    assert live.reference_deployment_claim_authorized is False

    inconsistent_age = _live_snapshot(replay)
    inconsistent_age["sourceFreshness"]["newestTelemetryAgeSeconds"] = 75.0
    assert live.accept_snapshot(inconsistent_age) is False
    assert live.source_state == "malformed"
    assert live.tracks_for_display == ()


def test_loopback_client_ignores_proxy_env_and_is_get_only_byte_bounded(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    replay = _model()

    class Handler(BaseHTTPRequestHandler):
        body = json.dumps(_live_snapshot(replay)).encode("utf-8")
        observed_method = ""
        redirect = False

        def do_GET(self) -> None:  # noqa: N802 - stdlib handler API
            type(self).observed_method = self.command
            if type(self).redirect:
                self.send_response(302)
                self.send_header("Location", "/api/snapshot")
                self.end_headers()
                return
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(type(self).body)))
            self.end_headers()
            self.wfile.write(type(self).body)

        def log_message(self, format: str, *args: object) -> None:
            return

    server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    url = f"http://127.0.0.1:{server.server_port}/api/snapshot"
    try:
        for name in ("HTTP_PROXY", "HTTPS_PROXY", "http_proxy", "https_proxy"):
            monkeypatch.setenv(name, "http://127.0.0.1:1")
        for name in ("NO_PROXY", "no_proxy"):
            monkeypatch.setenv(name, "")
        client = LiveSnapshotClient(url, timeout_seconds=0.5)
        assert client.fetch()["mode"] == "live-local"
        assert Handler.observed_method == "GET"

        Handler.body = b"{" + (b" " * 2_048) + b"}"
        bounded = LiveSnapshotClient(url, timeout_seconds=0.5, max_bytes=1_024)
        with pytest.raises(LiveSnapshotMalformedError, match="byte limit"):
            bounded.fetch()

        Handler.redirect = True
        with pytest.raises(LiveSnapshotUnavailableError, match="unavailable"):
            client.fetch()
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2.0)


def test_validate_only_live_cli_reports_match_without_opening_tk(
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    replay = _model()
    snapshot = _live_snapshot(replay)
    monkeypatch.setattr(LiveSnapshotClient, "fetch", lambda _self: snapshot)

    assert main(
        [
            "--scenario-id",
            "clear-rf",
            "--duration-seconds",
            "4",
            "--cadence-seconds",
            "1",
            "--live-url",
            "http://127.0.0.1:8765/api/snapshot",
            "--validate-only",
        ]
    ) == 0
    summary = json.loads(capsys.readouterr().out)
    assert summary["liveState"] == "fresh"
    assert summary["placementComparison"] == "matched"
    assert summary["referenceDeploymentClaimAuthorized"] is True
    assert summary["detectionRenderingEnabled"] is True
