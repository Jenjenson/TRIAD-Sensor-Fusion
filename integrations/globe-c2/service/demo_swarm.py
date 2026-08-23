"""Drone-swarm fusion demo with a ground-truth oracle.

Each scenario declares the *physical* drones that exist and how each sensor
reports them.  Because a sensor supplies the label it uses, a scenario can make
two sensors disagree about one drone, or make two drones share one label.  The
demo posts those observations to the fusion API, then grades the emitted tracks
against the physical truth and prints a per-scenario verdict:

    correct    exactly one track per physical drone, at the expected tier
    duplicate  one physical drone produced more than one track
    merged     one track covers more than one physical drone
    missing    a physical drone produced no track
    ghost      a track maps to no physical drone
    tier       the track exists but its confirmation tier is wrong

Scenarios marked ``known_gap`` fail today because the pipeline has no
association layer: identity arrives with the observation, so nothing merges or
splits tracks.  They are reported separately from regressions, and the demo
tells you when one starts passing.

Run in-process (no server needed)::

    cd integrations/globe-c2
    python -m service.demo_swarm

Run against a live server::

    python -m service.demo_swarm --base-url http://127.0.0.1:8100
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass, field
from datetime import datetime, timedelta, timezone
import json
import sys
from typing import Any, Iterable, Sequence
import urllib.error
import urllib.request

NODES = (
    ("City_Sector", 1.3000, 103.8500),
    ("South_Sector", 1.2960, 103.8520),
    ("East_Sector", 1.3010, 103.8560),
)
PERIMETER = {
    "enabled": True,
    "minimumLongitudeDegrees": 103.70,
    "maximumLongitudeDegrees": 104.00,
    "minimumLatitudeDegrees": 1.20,
    "maximumLatitudeDegrees": 1.3020,
    "phaseRateDeadbandMetersPerSecond": 0.25,
}
CLEAR_WEATHER = {
    "profile": "Clear",
    "rainRateMillimetersPerHour": 0.0,
    "visibilityMeters": 30_000.0,
}


# --------------------------------------------------------------------------- #
# Scenario description
# --------------------------------------------------------------------------- #


@dataclass(frozen=True, slots=True)
class Observation:
    """One sensor's report about one physical drone.

    ``label`` is the identity that sensor emits.  Two observations of the same
    physical drone carrying different labels is exactly the association problem
    this demo is built to expose.
    """

    modality: str  # rf | radar | eo | thermal
    node: str
    label: str
    track: str = "RT-1"
    age_s: float = 0.0
    confidence: float = 0.95


@dataclass(frozen=True, slots=True)
class PhysicalDrone:
    """One drone that actually exists in the simulated scene."""

    physical_id: str
    range_m: float
    bearing_deg: float
    observations: tuple[Observation, ...]
    expected_tier: str = "CONFIRMED"
    expected_tracks: int = 1
    note: str = ""


@dataclass(frozen=True, slots=True)
class Scenario:
    name: str
    description: str
    drones: tuple[PhysicalDrone, ...]
    known_gap: str | None = None


def _rf(node: str, label: str, *, age_s: float = 0.0) -> Observation:
    return Observation("rf", node, label, age_s=age_s)


def _radar(node: str, label: str, *, track: str = "RT-1", age_s: float = 0.0, confidence: float = 0.95) -> Observation:
    return Observation("radar", node, label, track=track, age_s=age_s, confidence=confidence)


def _eo(node: str, label: str, *, track: str = "RT-1") -> Observation:
    return Observation("eo", node, label, track=track, confidence=0.92)


def _thermal(node: str, label: str, *, track: str = "RT-1") -> Observation:
    return Observation("thermal", node, label, track=track, confidence=0.90)


def full_stack(label: str, *, node: str = "City_Sector", second: str = "South_Sector") -> tuple[Observation, ...]:
    """RF from two nodes plus radar, EO, and thermal from one node."""

    return (
        _rf(node, label),
        _rf(second, label),
        _radar(node, label),
        _eo(node, label),
        _thermal(node, label),
    )


def scenarios() -> tuple[Scenario, ...]:
    swarm = tuple(
        PhysicalDrone(
            physical_id=f"P{index:02d}",
            range_m=420.0 + index * 30.0,
            bearing_deg=(index * 17.0) % 360.0,
            observations=full_stack(f"Swarm_{index:02d}"),
        )
        for index in range(1, 9)
    )

    label_disagreement = (
        PhysicalDrone("P01", 480.0, 10.0, full_stack("Alpha")),
        PhysicalDrone(
            "P02",
            520.0,
            40.0,
            observations=(
                # RF calls it Bravo; radar and PTZ call the same drone Bravo_alt.
                _rf("City_Sector", "Bravo"),
                _rf("South_Sector", "Bravo"),
                _radar("City_Sector", "Bravo_alt"),
                _eo("City_Sector", "Bravo_alt"),
                _thermal("City_Sector", "Bravo_alt"),
            ),
            note="one drone, two labels across sensor families",
        ),
        PhysicalDrone("P03", 560.0, 70.0, full_stack("Charlie")),
    )

    trackid_churn = (
        PhysicalDrone(
            "P01",
            500.0,
            20.0,
            observations=(
                _rf("City_Sector", "Delta"),
                _rf("South_Sector", "Delta"),
                # Two radar sites, consistent label, different internal track ids.
                _radar("City_Sector", "Delta", track="RT-CITY-9"),
                _radar("East_Sector", "Delta", track="RT-EAST-4"),
                _eo("City_Sector", "Delta", track="RT-CITY-9"),
            ),
            note="same drone, same label, different radar track ids",
        ),
        PhysicalDrone(
            "P02",
            700.0,
            200.0,
            observations=(
                _rf("City_Sector", "Echo"),
                _rf("East_Sector", "Echo"),
                _radar("East_Sector", "Echo", track="RT-EAST-7"),
                _eo("East_Sector", "Echo", track="RT-EAST-7"),
            ),
            note="single radar site, RF plus visual still reaches three families",
        ),
    )

    shared_label = (
        PhysicalDrone(
            "P01",
            500.00,
            30.0,
            observations=(
                _rf("City_Sector", "Foxtrot"),
                _rf("South_Sector", "Foxtrot"),
                _radar("City_Sector", "Foxtrot"),
                _eo("City_Sector", "Foxtrot"),
                _thermal("City_Sector", "Foxtrot"),
            ),
            note="two drones share one upstream label",
        ),
        PhysicalDrone(
            "P02",
            500.10,
            30.0,
            observations=(
                _rf("East_Sector", "Foxtrot"),
                _radar("East_Sector", "Foxtrot", track="RT-EAST-2"),
            ),
            note="two drones share one upstream label",
        ),
    )

    evidence_gates = (
        PhysicalDrone(
            "P01",
            600.0,
            15.0,
            observations=(_rf("City_Sector", "Golf"), _rf("South_Sector", "Golf")),
            expected_tier="PRELIMINARY",
            note="two RF receivers, one family only",
        ),
        PhysicalDrone(
            "P02",
            640.0,
            45.0,
            observations=(_rf("City_Sector", "Hotel"),),
            expected_tier="UNCONFIRMED",
            note="single RF receiver cannot cue",
        ),
        PhysicalDrone(
            "P03",
            680.0,
            75.0,
            observations=(
                _rf("City_Sector", "India"),
                _rf("South_Sector", "India"),
                _radar("City_Sector", "India"),
            ),
            expected_tier="CORROBORATED",
            note="RF plus active radar, two families",
        ),
    )

    stale_cue = (
        PhysicalDrone(
            "P01",
            500.0,
            25.0,
            observations=(
                _rf("City_Sector", "Juliet"),
                _rf("South_Sector", "Juliet"),
                _radar("City_Sector", "Juliet", age_s=5.0),
                _eo("City_Sector", "Juliet"),
                _thermal("City_Sector", "Juliet"),
            ),
            expected_tier="PRELIMINARY",
            note="stale radar cue must invalidate its dependent PTZ confirmations",
        ),
    )

    return (
        Scenario("clean_swarm", "8 drones, every sensor agrees on the label", swarm),
        Scenario(
            "label_disagreement",
            "3 drones; RF and radar/PTZ disagree about one drone's label",
            label_disagreement,
            known_gap="no association layer: differing labels cannot be merged into one track",
        ),
        Scenario(
            "radar_trackid_churn",
            "2 drones reported by two radar sites with different internal track ids",
            trackid_churn,
        ),
        Scenario(
            "shared_label_two_drones",
            "2 distinct drones arrive under one upstream label",
            shared_label,
            known_gap="no association layer: one label cannot be split back into two tracks",
        ),
        Scenario("evidence_gates", "Degraded evidence must land on the right tier", evidence_gates),
        Scenario("stale_cue_fails_closed", "A stale radar cue must invalidate PTZ", stale_cue),
    )


# --------------------------------------------------------------------------- #
# Payload construction
# --------------------------------------------------------------------------- #


def _stamp(as_of: datetime, age_s: float) -> str:
    return (as_of - timedelta(seconds=age_s)).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def node_payloads() -> list[dict[str, Any]]:
    return [
        {
            "nodeId": node_id,
            "latitudeDegrees": latitude,
            "longitudeDegrees": longitude,
            "heightMeters": 40.0,
            "runtimeStatus": "ONLINE",
            "detectionRangeMeters": 20_000.0,
            "supportedFrequenciesGHz": [2.437],
            "receiverSensitivityDbm": -96.0,
            "searchRadarConfigured": True,
            "searchRadarRangeMeters": 5_000.0,
            "searchRadarRuntimeStatus": "ONLINE",
            "eoPtzConfigured": True,
            "eoPtzConfirmationRangeMeters": 2_000.0,
            "eoPtzRuntimeStatus": "ONLINE",
            "thermalPtzConfigured": True,
            "thermalPtzConfirmationRangeMeters": 2_000.0,
            "thermalPtzRuntimeStatus": "ONLINE",
        }
        for node_id, latitude, longitude in NODES
    ]


def observation_payloads(
    scenario: Scenario, as_of: datetime
) -> tuple[list[dict[str, Any]], list[dict[str, Any]], list[dict[str, Any]]]:
    rf: list[dict[str, Any]] = []
    radar: list[dict[str, Any]] = []
    ptz: list[dict[str, Any]] = []
    for drone in scenario.drones:
        for observation in drone.observations:
            timestamp = _stamp(as_of, observation.age_s)
            if observation.modality == "rf":
                rf.append(
                    {
                        "nodeId": observation.node,
                        "targetActor": observation.label,
                        "timestampUtc": timestamp,
                        "frequencyGHz": 2.437,
                        "slantRangeMeters": drone.range_m,
                        "receivedPowerDbm": -80.0,
                        "noiseFloorDbm": -96.0,
                        "lineOfSight": True,
                    }
                )
            elif observation.modality == "radar":
                radar.append(
                    {
                        "nodeId": observation.node,
                        "targetActor": observation.label,
                        "trackId": observation.track,
                        "timestampUtc": timestamp,
                        "rangeMeters": drone.range_m,
                        "bearingDegrees": drone.bearing_deg,
                        "elevationDegrees": 6.5,
                        "radialVelocityMetersPerSecond": -20.0,
                        "confidence": observation.confidence,
                        "lineOfSight": True,
                        "rangeEnvelopeMeters": 5_000.0,
                    }
                )
            else:
                ptz.append(
                    {
                        "nodeId": observation.node,
                        "targetActor": observation.label,
                        "trackId": observation.track,
                        "sensorType": "EO_PTZ" if observation.modality == "eo" else "THERMAL_PTZ",
                        "timestampUtc": timestamp,
                        "rangeMeters": drone.range_m,
                        "confidence": observation.confidence,
                        "cueAgeSeconds": 0.08,
                        "confirmed": True,
                        "lineOfSight": True,
                        "boundingBoxPixels": [626.0, 350.0, 654.0, 370.0],
                        "weatherConfidenceFactor": 1.0,
                    }
                )
    return rf, radar, ptz


# --------------------------------------------------------------------------- #
# Transports
# --------------------------------------------------------------------------- #


class Transport:
    """Minimal request interface so the demo runs in-process or over HTTP."""

    def call(self, method: str, path: str, payload: Any = None) -> tuple[int, Any]:
        raise NotImplementedError


class HTTPTransport(Transport):
    def __init__(self, base_url: str, timeout: float = 30.0) -> None:
        self.base_url = base_url.rstrip("/")
        self.timeout = timeout

    def call(self, method: str, path: str, payload: Any = None) -> tuple[int, Any]:
        data = json.dumps(payload).encode("utf-8") if payload is not None else None
        request = urllib.request.Request(
            f"{self.base_url}{path}",
            data=data,
            method=method,
            headers={"Content-Type": "application/json", "Accept": "application/json"},
        )
        try:
            with urllib.request.urlopen(request, timeout=self.timeout) as response:
                body = response.read()
                return response.status, json.loads(body) if body else {}
        except urllib.error.HTTPError as exc:
            body = exc.read()
            return exc.code, json.loads(body) if body else {}


class InProcessTransport(Transport):
    def __init__(self) -> None:
        from fastapi.testclient import TestClient

        from .app import Settings, create_app

        # A large stale window keeps a fixed --as-of usable for replay.
        self._client = TestClient(create_app(Settings(stale_after_s=1e9)))

    def call(self, method: str, path: str, payload: Any = None) -> tuple[int, Any]:
        response = self._client.request(method, path, json=payload)
        try:
            return response.status_code, response.json()
        except ValueError:
            return response.status_code, {}


# --------------------------------------------------------------------------- #
# Oracle
# --------------------------------------------------------------------------- #


@dataclass(slots=True)
class Finding:
    kind: str
    detail: str


@dataclass(slots=True)
class ScenarioResult:
    scenario: Scenario
    sent: dict[str, int]
    tracks: list[dict[str, Any]]
    c2_detections: int
    rejected: dict[str, int]
    rejection_reasons: dict[str, list[str]] = field(default_factory=dict)
    findings: list[Finding] = field(default_factory=list)

    @property
    def passed(self) -> bool:
        return not self.findings


def label_ownership(scenario: Scenario) -> dict[str, set[str]]:
    """Map every emitted label to the physical drones that produced it."""

    owners: dict[str, set[str]] = {}
    for drone in scenario.drones:
        for observation in drone.observations:
            owners.setdefault(observation.label, set()).add(drone.physical_id)
    return owners


def grade(scenario: Scenario, tracks: Sequence[dict[str, Any]]) -> list[Finding]:
    owners = label_ownership(scenario)
    findings: list[Finding] = []
    by_label = {str(track.get("trackId")): track for track in tracks}

    for label, track in by_label.items():
        drones = owners.get(label)
        if not drones:
            findings.append(Finding("ghost", f"track {label!r} maps to no physical drone"))
        elif len(drones) > 1:
            findings.append(
                Finding(
                    "merged",
                    f"track {label!r} covers {len(drones)} physical drones "
                    f"({', '.join(sorted(drones))}); expected one track each",
                )
            )

    for drone in scenario.drones:
        labels = sorted(
            label
            for label, drones in owners.items()
            if drone.physical_id in drones and label in by_label
        )
        if len(labels) < drone.expected_tracks:
            findings.append(
                Finding(
                    "missing",
                    f"drone {drone.physical_id} produced {len(labels)} track(s), "
                    f"expected {drone.expected_tracks}",
                )
            )
            continue
        if len(labels) > drone.expected_tracks:
            findings.append(
                Finding(
                    "duplicate",
                    f"drone {drone.physical_id} produced {len(labels)} tracks "
                    f"({', '.join(labels)}); expected {drone.expected_tracks}",
                )
            )
        tiers = {label: str(by_label[label].get("confirmationTier")) for label in labels}
        if drone.expected_tier not in tiers.values():
            findings.append(
                Finding(
                    "tier",
                    f"drone {drone.physical_id} expected tier {drone.expected_tier}, got "
                    + ", ".join(f"{label}={tier}" for label, tier in tiers.items()),
                )
            )
    return findings


def run_scenario(transport: Transport, scenario: Scenario, as_of: datetime) -> ScenarioResult:
    def expect(method: str, path: str, payload: Any = None) -> Any:
        status, body = transport.call(method, path, payload)
        if status != 200:
            raise SystemExit(f"{method} {path} failed with HTTP {status}: {body}")
        return body

    expect("DELETE", "/v1/observations")
    expect("POST", "/v1/observations/nodes", {"nodes": node_payloads()})
    expect("PUT", "/v1/environment", {"weather": CLEAR_WEATHER, "simulationPerimeter": PERIMETER})

    rf, radar, ptz = observation_payloads(scenario, as_of)
    if rf:
        expect("POST", "/v1/observations/rf", {"links": rf})
    if radar:
        expect("POST", "/v1/observations/search-radar", {"detections": radar})
    if ptz:
        expect("POST", "/v1/observations/ptz", {"confirmations": ptz})

    status, body = transport.call(
        "POST",
        f"/v1/fusion/run?as_of={as_of.isoformat().replace('+00:00', 'Z')}",
    )
    if status != 200:
        raise SystemExit(f"POST /v1/fusion/run failed with HTTP {status}: {body}")

    return ScenarioResult(
        scenario=scenario,
        sent={"rf": len(rf), "searchRadar": len(radar), "ptz": len(ptz)},
        tracks=list(body.get("tracks", [])),
        c2_detections=int(body.get("c2DetectionCount", 0)),
        rejected={key: len(value) for key, value in body.get("rejectedEvidence", {}).items()},
        rejection_reasons={
            key: sorted({str(row.get("reason")) for row in value})
            for key, value in body.get("rejectedEvidence", {}).items()
            if value
        },
        findings=grade(scenario, body.get("tracks", [])),
    )


# --------------------------------------------------------------------------- #
# Reporting
# --------------------------------------------------------------------------- #

_KIND_LABEL = {
    "duplicate": "DUPLICATE",
    "merged": "MERGED",
    "missing": "MISSING",
    "ghost": "GHOST",
    "tier": "TIER",
}


def render(result: ScenarioResult) -> None:
    scenario = result.scenario
    print(f"\n{'=' * 78}")
    print(f"scenario: {scenario.name}")
    print(f"  {scenario.description}")
    print(
        f"  physical drones: {len(scenario.drones)}   "
        f"observations sent: rf={result.sent['rf']} radar={result.sent['searchRadar']} ptz={result.sent['ptz']}"
    )
    print(f"{'-' * 78}")
    print(f"  {'emitted track':<16} {'tier':<12} {'score':>6}  {'fam':>3}  {'pos':<3} modalities")
    if not result.tracks:
        print("  (no tracks emitted)")
    for track in sorted(result.tracks, key=lambda item: str(item.get("trackId"))):
        score = track.get("fusedEvidenceScore")
        modalities = ",".join(track.get("activeModalities") or []) or "-"
        print(
            f"  {str(track.get('trackId')):<16} {str(track.get('confirmationTier')):<12} "
            f"{score if score is not None else 0.0:>6.3f}  "
            f"{track.get('activeModalityFamilyCount', 0):>3}  "
            f"{'yes' if track.get('positionAvailable') else 'no ':<3} {modalities}"
        )
    rejected_total = sum(result.rejected.values())
    print(
        f"  tracks={len(result.tracks)}  c2 detections={result.c2_detections}  "
        f"rejected evidence={rejected_total}"
    )
    for modality, reasons in result.rejection_reasons.items():
        for reason in reasons:
            print(f"    rejected {modality}: {reason}")
    print(f"{'-' * 78}")
    if result.passed:
        print("  VERDICT: CORRECT - one track per physical drone, tiers as expected")
    else:
        verdict = "INCORRECT (known gap)" if scenario.known_gap else "INCORRECT (regression)"
        print(f"  VERDICT: {verdict}")
        for finding in result.findings:
            print(f"    [{_KIND_LABEL.get(finding.kind, finding.kind.upper())}] {finding.detail}")
        if scenario.known_gap:
            print(f"    cause: {scenario.known_gap}")


def summarise(results: Iterable[ScenarioResult], *, strict: bool) -> int:
    rows = list(results)
    regressions = [item for item in rows if not item.passed and not item.scenario.known_gap]
    gap_failures = [item for item in rows if not item.passed and item.scenario.known_gap]
    closed_gaps = [item for item in rows if item.passed and item.scenario.known_gap]

    print(f"\n{'=' * 78}")
    print("summary")
    print(f"{'-' * 78}")
    for item in rows:
        if item.passed:
            state = "PASS (gap closed!)" if item.scenario.known_gap else "PASS"
        else:
            state = "FAIL (known gap)" if item.scenario.known_gap else "FAIL (regression)"
        print(f"  {item.scenario.name:<26} {state}")
    print(f"{'-' * 78}")
    print(
        f"  {len(rows) - len(regressions) - len(gap_failures)}/{len(rows)} scenarios correct   "
        f"regressions={len(regressions)}   known-gap failures={len(gap_failures)}"
    )
    if closed_gaps:
        print("  a known gap now passes; update its scenario if the fix is intentional:")
        for item in closed_gaps:
            print(f"    - {item.scenario.name}")
    if gap_failures and not strict:
        print("  known-gap failures do not affect the exit code; pass --strict to include them")
    failures = len(regressions) + (len(gap_failures) if strict else 0)
    return 1 if failures else 0


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "--base-url",
        default=None,
        help="Fusion API base URL. Omit to run the app in-process.",
    )
    parser.add_argument(
        "--as-of",
        default=None,
        help="Timezone-aware ISO-8601 sample time (default: now).",
    )
    parser.add_argument("--scenario", action="append", help="Run only the named scenario. Repeatable.")
    parser.add_argument("--strict", action="store_true", help="Count known-gap failures in the exit code.")
    parser.add_argument("--json", action="store_true", help="Emit machine-readable results.")
    parser.add_argument("--list", action="store_true", help="List scenario names and exit.")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    available = scenarios()
    if args.list:
        for scenario in available:
            marker = " (known gap)" if scenario.known_gap else ""
            print(f"{scenario.name}{marker}: {scenario.description}")
        return 0

    selected = available
    if args.scenario:
        wanted = set(args.scenario)
        unknown = wanted - {item.name for item in available}
        if unknown:
            raise SystemExit(f"unknown scenario(s): {', '.join(sorted(unknown))}")
        selected = tuple(item for item in available if item.name in wanted)

    if args.as_of:
        as_of = datetime.fromisoformat(args.as_of.replace("Z", "+00:00"))
        if as_of.tzinfo is None:
            raise SystemExit("--as-of must be timezone-aware")
    else:
        as_of = datetime.now(timezone.utc)

    transport = HTTPTransport(args.base_url) if args.base_url else InProcessTransport()
    if not args.json:
        target = args.base_url or "in-process ASGI app"
        print(f"fusion API demo -> {target}")
        print(f"sample time      -> {as_of.isoformat().replace('+00:00', 'Z')}")

    results = [run_scenario(transport, scenario, as_of) for scenario in selected]

    if args.json:
        print(
            json.dumps(
                {
                    "asOfUtc": as_of.isoformat().replace("+00:00", "Z"),
                    "scenarios": [
                        {
                            "name": item.scenario.name,
                            "knownGap": item.scenario.known_gap,
                            "physicalDrones": len(item.scenario.drones),
                            "observationsSent": item.sent,
                            "trackCount": len(item.tracks),
                            "c2DetectionCount": item.c2_detections,
                            "rejectedEvidence": item.rejected,
                            "rejectionReasons": item.rejection_reasons,
                            "correct": item.passed,
                            "findings": [
                                {"kind": finding.kind, "detail": finding.detail}
                                for finding in item.findings
                            ],
                            "tracks": [
                                {
                                    "trackId": track.get("trackId"),
                                    "confirmationTier": track.get("confirmationTier"),
                                    "fusedEvidenceScore": track.get("fusedEvidenceScore"),
                                    "activeModalityFamilyCount": track.get("activeModalityFamilyCount"),
                                    "activeModalities": track.get("activeModalities"),
                                    "positionAvailable": track.get("positionAvailable"),
                                }
                                for track in item.tracks
                            ],
                        }
                        for item in results
                    ],
                },
                indent=2,
            )
        )
        regressions = sum(
            1 for item in results if not item.passed and not item.scenario.known_gap
        )
        gaps = sum(1 for item in results if not item.passed and item.scenario.known_gap)
        return 1 if regressions + (gaps if args.strict else 0) else 0

    for result in results:
        render(result)
    return summarise(results, strict=args.strict)


if __name__ == "__main__":
    sys.exit(main())
