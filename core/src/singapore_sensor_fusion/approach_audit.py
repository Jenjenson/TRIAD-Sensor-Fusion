"""Read-only audit for the simulated outside-to-Singapore approach scenario.

The configured shape is an explicit simulation evaluation perimeter, not
a legal or national boundary.  The audit keeps authored scenario truth
separate from RF detections and never performs engagement actions.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import time
from typing import Any, Mapping, Sequence

from .geodesy import ENU, Geodetic, enu_to_geodetic, wgs84_surface_distance_m
from .oak_rgbd import write_json_atomic
from .paths import DEFAULT_TRIAD_CONFIG, DEFAULT_TRIAD_SENSOR_SAVED_DIR


WGS84_RADIUS_M = 6_378_137.0
DEFAULT_CONFIG = DEFAULT_TRIAD_CONFIG
DEFAULT_SNAPSHOT = DEFAULT_TRIAD_SENSOR_SAVED_DIR / "latest_rf_snapshot.json"
APPROACH_FIELDS = (
    "airspaceState",
    "distanceToPerimeterMeters",
    "approachRateMetersPerSecond",
    "headingDegrees",
    "speedMetersPerSecond",
    "outsideSimulationPerimeter",
    "ingressCorridorId",
)
SUPPORTED_SNAPSHOT_SCHEMAS = frozenset(
    ("triad.live_rf_snapshot.v1", "triad.live_rf_snapshot.v2", "triad.live_rf_snapshot.v3")
)


@dataclass(frozen=True, slots=True)
class Perimeter:
    min_lon: float
    max_lon: float
    min_lat: float
    max_lat: float
    deadband_mps: float = 0.25
    geometry_type: str = "axis_aligned_wgs84_rectangle"
    center_lon: float | None = None
    center_lat: float | None = None
    radius_m: float | None = None

    @property
    def is_circle(self) -> bool:
        return self.geometry_type.lower() in {"circle", "wgs84_geodesic_circle"}

    @property
    def mid_lat_radians(self) -> float:
        return math.radians((self.min_lat + self.max_lat) * 0.5)

    @property
    def meters_per_lon_degree(self) -> float:
        return math.pi * WGS84_RADIUS_M / 180.0 * max(abs(math.cos(self.mid_lat_radians)), 1e-6)

    @property
    def meters_per_lat_degree(self) -> float:
        return math.pi * WGS84_RADIUS_M / 180.0


def _circle_perimeter(
    center_lon: float,
    center_lat: float,
    radius_m: float,
    *,
    deadband_mps: float = 0.25,
) -> Perimeter:
    if not -180.0 <= center_lon <= 180.0 or not -90.0 <= center_lat <= 90.0:
        raise ValueError("circle center must be valid WGS84 longitude/latitude")
    if radius_m <= 0.0:
        raise ValueError("circle radius must be > 0")
    center = Geodetic(center_lat, center_lon, 0.0)
    east = enu_to_geodetic(ENU(radius_m, 0.0, 0.0), center)
    west = enu_to_geodetic(ENU(-radius_m, 0.0, 0.0), center)
    north = enu_to_geodetic(ENU(0.0, radius_m, 0.0), center)
    south = enu_to_geodetic(ENU(0.0, -radius_m, 0.0), center)
    return Perimeter(
        min_lon=west.longitude_deg,
        max_lon=east.longitude_deg,
        min_lat=south.latitude_deg,
        max_lat=north.latitude_deg,
        deadband_mps=deadband_mps,
        geometry_type="wgs84_geodesic_circle",
        center_lon=center_lon,
        center_lat=center_lat,
        radius_m=radius_m,
    )


def _finite(value: object) -> float | None:
    if isinstance(value, bool):
        return None
    try:
        result = float(value)
    except (TypeError, ValueError):
        return None
    return result if math.isfinite(result) else None


def _load_object(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError(f"JSON root is not an object: {path}")
    return value


def _perimeter_from_config(config: Mapping[str, Any]) -> Perimeter:
    raw = config.get("SimulationPerimeter")
    if not isinstance(raw, Mapping) or raw.get("bEnabled") is not True:
        raise ValueError("SimulationPerimeter must be present and enabled")
    deadband = max(_finite(raw.get("PhaseRateDeadbandMetersPerSecond")) or 0.0, 0.0)
    if str(raw.get("Shape") or "Rectangle").strip().lower() == "circle":
        center_lon = _finite(raw.get("CenterLongitudeDegrees"))
        center_lat = _finite(raw.get("CenterLatitudeDegrees"))
        radius_m = _finite(raw.get("RadiusMeters"))
        if center_lon is None or center_lat is None or radius_m is None:
            raise ValueError("circular SimulationPerimeter requires a finite center and radius")
        return _circle_perimeter(center_lon, center_lat, radius_m, deadband_mps=deadband)
    values = (
        _finite(raw.get("MinimumLongitudeDegrees")),
        _finite(raw.get("MaximumLongitudeDegrees")),
        _finite(raw.get("MinimumLatitudeDegrees")),
        _finite(raw.get("MaximumLatitudeDegrees")),
    )
    if any(item is None for item in values):
        raise ValueError("SimulationPerimeter bounds must be finite")
    min_lon, max_lon = sorted((float(values[0]), float(values[1])))
    min_lat, max_lat = sorted((float(values[2]), float(values[3])))
    if min_lon == max_lon or min_lat == max_lat:
        raise ValueError("SimulationPerimeter must have non-zero area")
    return Perimeter(
        min_lon=min_lon,
        max_lon=max_lon,
        min_lat=min_lat,
        max_lat=max_lat,
        deadband_mps=deadband,
    )


def _perimeter_from_snapshot(snapshot: Mapping[str, Any]) -> Perimeter:
    raw = snapshot.get("simulationPerimeter")
    if not isinstance(raw, Mapping):
        raise ValueError("snapshot simulationPerimeter is missing")
    geometry_type = str(raw.get("geometryType") or raw.get("shape") or "").strip().lower()
    if geometry_type in {"circle", "wgs84_geodesic_circle"}:
        center_lon = _finite(raw.get("centerLongitudeDegrees"))
        center_lat = _finite(raw.get("centerLatitudeDegrees"))
        radius_m = _finite(raw.get("radiusMeters"))
        if center_lon is None or center_lat is None or radius_m is None:
            raise ValueError("snapshot circular perimeter requires a finite center and radius")
        deadband = max(_finite(raw.get("phaseRateDeadbandMetersPerSecond")) or 0.0, 0.0)
        return _circle_perimeter(center_lon, center_lat, radius_m, deadband_mps=deadband)
    aliases = (
        ("minimumLongitudeDegrees", "minLongitudeDegrees", "minLon"),
        ("maximumLongitudeDegrees", "maxLongitudeDegrees", "maxLon"),
        ("minimumLatitudeDegrees", "minLatitudeDegrees", "minLat"),
        ("maximumLatitudeDegrees", "maxLatitudeDegrees", "maxLat"),
    )
    values: list[float] = []
    for names in aliases:
        value = next((_finite(raw.get(name)) for name in names if _finite(raw.get(name)) is not None), None)
        if value is None:
            raise ValueError(f"snapshot perimeter is missing {names[0]}")
        values.append(value)
    return Perimeter(
        min_lon=min(values[0], values[1]),
        max_lon=max(values[0], values[1]),
        min_lat=min(values[2], values[3]),
        max_lat=max(values[2], values[3]),
    )


def signed_distance_to_perimeter_m(lon: float, lat: float, perimeter: Perimeter) -> float:
    """Match the plugin's signed rectangle or WGS84 circle distance."""

    if perimeter.is_circle:
        assert perimeter.center_lon is not None
        assert perimeter.center_lat is not None
        assert perimeter.radius_m is not None
        center = Geodetic(perimeter.center_lat, perimeter.center_lon, 0.0)
        point = Geodetic(lat, lon, 0.0)
        return wgs84_surface_distance_m(center, point) - perimeter.radius_m

    x = lon * perimeter.meters_per_lon_degree
    west = perimeter.min_lon * perimeter.meters_per_lon_degree
    east = perimeter.max_lon * perimeter.meters_per_lon_degree
    y = lat * perimeter.meters_per_lat_degree
    south = perimeter.min_lat * perimeter.meters_per_lat_degree
    north = perimeter.max_lat * perimeter.meters_per_lat_degree
    dx = west - x if x < west else x - east if x > east else 0.0
    dy = south - y if y < south else y - north if y > north else 0.0
    if dx > 0.0 or dy > 0.0:
        return math.hypot(dx, dy)
    return -max(min(x - west, east - x, y - south, north - y), 0.0)


def _move_enu(lon: float, lat: float, east_m: float, north_m: float) -> tuple[float, float]:
    lat_radians = math.radians(lat)
    return (
        lon + math.degrees(east_m / (WGS84_RADIUS_M * max(abs(math.cos(lat_radians)), 1e-6))),
        lat + math.degrees(north_m / WGS84_RADIUS_M),
    )


def _slant_m(a: Mapping[str, Any], lon: float, lat: float, height_m: float) -> float:
    node_lon = float(a["LongitudeDegrees"])
    node_lat = float(a["LatitudeDegrees"])
    mean_lat = math.radians((node_lat + lat) * 0.5)
    east = math.radians(lon - node_lon) * WGS84_RADIUS_M * math.cos(mean_lat)
    north = math.radians(lat - node_lat) * WGS84_RADIUS_M
    up = height_m - float(a.get("HeightMeters", 0.0))
    return math.sqrt(east * east + north * north + up * up)


def _formation_starts(target: Mapping[str, Any]) -> list[tuple[float, float]]:
    count = max(int(target.get("SpawnCount", 1)), 1)
    columns = min(max(int(target.get("FormationColumns", 4)), 1), count)
    rows = math.ceil(count / columns)
    spacing = max(_finite(target.get("FormationSpacingMeters")) or 0.0, 0.0)
    base_lon = float(target["StartLongitudeDegrees"])
    base_lat = float(target["StartLatitudeDegrees"])
    result: list[tuple[float, float]] = []
    for index in range(count):
        row = index // columns
        column = index % columns
        in_row = min(columns, count - row * columns)
        east = (column - (in_row - 1) * 0.5) * spacing
        north = (row - (rows - 1) * 0.5) * spacing
        result.append(_move_enu(base_lon, base_lat, east, north))
    return result


def _check(check_id: str, status: str, message: str, **details: Any) -> dict[str, Any]:
    return {"id": check_id, "status": status, "message": message, "details": details}


def audit_config(config: Mapping[str, Any]) -> dict[str, Any]:
    checks: list[dict[str, Any]] = []
    try:
        perimeter = _perimeter_from_config(config)
    except ValueError as exc:
        return {"status": "fail", "checks": [_check("config.perimeter", "fail", str(exc))], "routes": []}

    checks.append(_check(
        "config.perimeter",
        "pass",
        "An explicit enabled simulation perimeter is configured and labelled separately from legal boundaries.",
        geometry_type=perimeter.geometry_type,
        min_lon=perimeter.min_lon,
        max_lon=perimeter.max_lon,
        min_lat=perimeter.min_lat,
        max_lat=perimeter.max_lat,
        center_lon=perimeter.center_lon,
        center_lat=perimeter.center_lat,
        radius_m=perimeter.radius_m,
    ))
    raw_targets = config.get("DemoTargets")
    targets = [item for item in raw_targets if isinstance(item, Mapping) and item.get("bEnabled") is not False] if isinstance(raw_targets, list) else []
    nodes = [item for item in config.get("SensorNodes", []) if isinstance(item, Mapping) and item.get("bEnabled") is not False]
    routes: list[dict[str, Any]] = []
    for target in targets:
        name = str(target.get("ActorName") or "unnamed")
        starts = _formation_starts(target)
        direction = target.get("LinearDirectionEnu") if isinstance(target.get("LinearDirectionEnu"), Mapping) else {}
        east = _finite(direction.get("X")) or 0.0
        north = _finite(direction.get("Y")) or 0.0
        norm = math.hypot(east, north)
        distance = max(_finite(target.get("LinearDistanceMeters")) or 0.0, 0.0)
        speed = max(_finite(target.get("LinearSpeedMetersPerSecond")) or 0.0, 0.0)
        unit_east, unit_north = ((east / norm, north / norm) if norm > 0.0 else (0.0, 0.0))
        start_distances = [signed_distance_to_perimeter_m(lon, lat, perimeter) for lon, lat in starts]
        sample_distances: list[float] = []
        for step in range(257):
            travel = distance * step / 256.0
            lon, lat = _move_enu(starts[0][0], starts[0][1], unit_east * travel, unit_north * travel)
            sample_distances.append(signed_distance_to_perimeter_m(lon, lat, perimeter))
        next_lon, next_lat = _move_enu(starts[0][0], starts[0][1], unit_east * speed, unit_north * speed)
        initial_rate = start_distances[0] - signed_distance_to_perimeter_m(next_lon, next_lat, perimeter)
        first_inside_step = next((index for index, value in enumerate(sample_distances) if value <= 0.0), None)
        entry_seconds = None
        if first_inside_step is not None and speed > 0.0:
            entry_seconds = round((distance * first_inside_step / 256.0) / speed, 1)
        nearest_start_slant = min(
            (_slant_m(node, lon, lat, float(target.get("StartHeightMeters", 0.0))) for lon, lat in starts for node in nodes),
            default=math.inf,
        )
        max_node_range = max((_finite(node.get("DetectionRangeMeters")) or 0.0 for node in nodes), default=0.0)
        route = {
            "name": name,
            "spawn_count": len(starts),
            "corridor": target.get("IngressCorridorId"),
            "hostile_scenario_truth": (target.get("RFEmitter") or {}).get("bHostileScenarioTruth") if isinstance(target.get("RFEmitter"), Mapping) else None,
            "all_formation_members_start_outside": all(value > 0.0 for value in start_distances),
            "minimum_start_distance_to_perimeter_m": round(min(start_distances), 1),
            "initial_approach_rate_mps": round(initial_rate, 3),
            "crosses_perimeter": min(sample_distances, default=math.inf) < 0.0,
            "minimum_route_signed_distance_m": round(min(sample_distances, default=math.inf), 1),
            "estimated_entry_seconds": entry_seconds,
            "one_way_duration_seconds": round(distance / speed, 1) if speed > 0.0 else None,
            "nearest_start_node_slant_range_m": round(nearest_start_slant, 1),
            "inside_configured_node_range_at_start": nearest_start_slant <= max_node_range,
            "ping_pong_route": target.get("bPingPongLinearPath") is True,
            "inbound_scenario_declared": target.get("bInboundApproachScenario") is True,
        }
        routes.append(route)

    inbound = [route for route in routes if route["inbound_scenario_declared"]]
    checks.extend([
        _check(
            "config.external_origins",
            "pass" if inbound and all(route["all_formation_members_start_outside"] for route in inbound) else "fail",
            f"{sum(route['all_formation_members_start_outside'] for route in inbound)}/{len(inbound)} inbound formations start wholly outside the simulation perimeter.",
        ),
        _check(
            "config.inbound_motion",
            "pass" if inbound and all(route["initial_approach_rate_mps"] > perimeter.deadband_mps for route in inbound) else "fail",
            f"{sum(route['initial_approach_rate_mps'] > perimeter.deadband_mps for route in inbound)}/{len(inbound)} inbound routes initially close on the perimeter.",
        ),
        _check(
            "config.boundary_crossing",
            "pass" if inbound and all(route["crosses_perimeter"] for route in inbound) else "fail",
            f"{sum(route['crosses_perimeter'] for route in inbound)}/{len(inbound)} inbound routes cross into the configured perimeter.",
        ),
        _check(
            "config.outside_detection_envelope",
            "pass" if any(route["all_formation_members_start_outside"] and route["inside_configured_node_range_at_start"] for route in routes) else "fail",
            "At least one outside formation begins inside a configured node's geometric range envelope; RF link-budget gates still apply.",
        ),
        _check(
            "config.route_continuity",
            "pass" if inbound and all(route["ping_pong_route"] for route in inbound) else "warn",
            "Ping-pong routing avoids the non-ping-pong linear path's endpoint-to-origin modulo reset.",
        ),
    ])
    return {
        "status": "fail" if any(item["status"] == "fail" for item in checks) else "pass",
        "checks": checks,
        "routes": routes,
        "total_spawn_count": sum(route["spawn_count"] for route in routes),
    }


def audit_snapshot_series(snapshots: Sequence[Mapping[str, Any]]) -> dict[str, Any]:
    checks: list[dict[str, Any]] = []
    if not snapshots:
        return {"status": "not_run", "checks": [_check("runtime.samples", "warn", "No runtime approach snapshot was supplied.")], "sample_count": 0}
    valid = [
        item
        for item in snapshots
        if item.get("schemaVersion") in SUPPORTED_SNAPSHOT_SCHEMAS
        and item.get("sampleComplete") is True
    ]
    checks.append(_check(
        "runtime.complete_samples",
        "pass" if len(valid) == len(snapshots) else "fail",
        f"{len(valid)}/{len(snapshots)} snapshots are complete supported v1/v2 Unreal samples.",
    ))
    if not valid:
        return {"status": "fail", "checks": checks, "sample_count": len(snapshots)}
    safety_ok = all(item.get("detectionOnly") is True and item.get("actionsTaken") == "none" for item in valid)
    checks.append(_check("runtime.detection_only", "pass" if safety_ok else "fail", "All samples remain detection-only with actionsTaken=none."))

    field_errors = 0
    sign_errors = 0
    state_errors = 0
    kinematic_errors = 0
    geometry_errors = 0
    outside_detected: set[str] = set()
    approaching_detected: set[str] = set()
    moving_inward: set[str] = set()
    transitions: set[str] = set()
    histories: dict[str, list[tuple[float, float, str]]] = {}
    corridor_milestones: dict[str, dict[str, Any]] = {}
    for snapshot in valid:
        try:
            perimeter = _perimeter_from_snapshot(snapshot)
        except ValueError:
            geometry_errors += 1
            continue
        sim_seconds = _finite(snapshot.get("simulationSeconds"))
        targets = snapshot.get("scenarioTargets") if isinstance(snapshot.get("scenarioTargets"), list) else []
        for raw in targets:
            if not isinstance(raw, Mapping):
                continue
            actor = raw.get("targetActor")
            if not isinstance(actor, str):
                field_errors += 1
                continue
            if any(field not in raw for field in APPROACH_FIELDS):
                field_errors += 1
            distance = _finite(raw.get("distanceToPerimeterMeters"))
            rate = _finite(raw.get("approachRateMetersPerSecond"))
            heading = _finite(raw.get("headingDegrees"))
            speed = _finite(raw.get("speedMetersPerSecond"))
            outside = raw.get("outsideSimulationPerimeter")
            state = raw.get("airspaceState")
            lon = _finite(raw.get("targetLongitudeDegrees"))
            lat = _finite(raw.get("targetLatitudeDegrees"))
            if distance is None or rate is None or heading is None or speed is None or not isinstance(outside, bool):
                field_errors += 1
                continue
            if outside != (distance > 0.0):
                sign_errors += 1
            expected_state = "INSIDE" if not outside else "APPROACHING" if rate > 0.25 else "DEPARTING" if rate < -0.25 else "OUTSIDE"
            if state != expected_state:
                state_errors += 1
            if not (0.0 <= heading < 360.0) or speed < 0.0 or abs(rate) > speed + 1.0:
                kinematic_errors += 1
            if lon is not None and lat is not None and abs(signed_distance_to_perimeter_m(lon, lat, perimeter) - distance) > 2.0:
                geometry_errors += 1
            if sim_seconds is not None:
                histories.setdefault(actor, []).append((sim_seconds, distance, str(state)))
            if raw.get("detectedThisSample") is True and outside:
                outside_detected.add(actor)
                if state == "APPROACHING":
                    approaching_detected.add(actor)
                corridor = raw.get("ingressCorridorId")
                if isinstance(corridor, str) and corridor:
                    milestone = corridor_milestones.setdefault(corridor, {})
                    milestone.setdefault("firstOutsideDetected", {
                        "targetActor": actor,
                        "timestampUtc": snapshot.get("timestampUtc"),
                        "simulationSeconds": sim_seconds,
                        "airspaceState": state,
                        "distanceToPerimeterMeters": round(distance, 3),
                        "approachRateMetersPerSecond": round(rate, 3),
                        "headingDegrees": round(heading, 3),
                        "speedMetersPerSecond": round(speed, 3),
                        "detectedLinkCount": raw.get("detectedLinkCount"),
                    })
            corridor = raw.get("ingressCorridorId")
            if isinstance(corridor, str) and corridor and state == "INSIDE":
                milestone = corridor_milestones.setdefault(corridor, {})
                if "firstOutsideDetected" in milestone:
                    milestone.setdefault("firstInsideAfterOutsideDetection", {
                        "targetActor": actor,
                        "timestampUtc": snapshot.get("timestampUtc"),
                        "simulationSeconds": sim_seconds,
                        "airspaceState": state,
                        "distanceToPerimeterMeters": round(distance, 3),
                        "detectedThisSample": raw.get("detectedThisSample") is True,
                    })

        ranges = {
            item.get("nodeId"): _finite(item.get("detectionRangeMeters"))
            for item in snapshot.get("sensorNodes", [])
            if isinstance(item, Mapping)
        }
        for link in snapshot.get("detectedRFLinks", []):
            if not isinstance(link, Mapping):
                continue
            slant = _finite(link.get("slantRangeMeters"))
            maximum = ranges.get(link.get("nodeId"))
            if slant is None or maximum is None or slant > maximum + 0.5:
                geometry_errors += 1

    for actor, samples in histories.items():
        samples.sort()
        for prior, current in zip(samples, samples[1:]):
            if current[1] < prior[1] - 1.0:
                moving_inward.add(actor)
            if prior[1] > 0.0 and current[1] <= 0.0:
                transitions.add(actor)

    checks.extend([
        _check("runtime.approach_fields", "pass" if field_errors == 0 else "fail", f"Approach-field errors: {field_errors}."),
        _check("runtime.signed_distance", "pass" if sign_errors == 0 and geometry_errors == 0 else "fail", f"Signed-distance/geometry errors: {sign_errors + geometry_errors}."),
        _check("runtime.state_semantics", "pass" if state_errors == 0 else "fail", f"Airspace-state errors: {state_errors}."),
        _check("runtime.kinematics", "pass" if kinematic_errors == 0 else "fail", f"Heading/speed/rate errors: {kinematic_errors}."),
        _check(
            "runtime.outside_rf_detection",
            "pass" if outside_detected else "fail",
            f"Detected RF tracks while outside the perimeter: {len(outside_detected)}.",
            actors=sorted(outside_detected),
        ),
        _check(
            "runtime.approaching_rf_detection",
            "pass" if approaching_detected else "fail",
            f"Detected RF tracks explicitly classified APPROACHING: {len(approaching_detected)}.",
            actors=sorted(approaching_detected),
        ),
        _check(
            "runtime.distance_decreases",
            "pass" if moving_inward else "warn",
            f"Targets observed with decreasing signed perimeter distance across samples: {len(moving_inward)}.",
            actors=sorted(moving_inward),
        ),
        _check(
            "runtime.outside_to_inside_transition",
            "pass" if transitions else "warn",
            f"Outside-to-inside crossings observed during this bounded capture: {len(transitions)}; a short capture may legitimately observe none.",
            actors=sorted(transitions),
        ),
        _check(
            "runtime.corridor_transition_evidence",
            "pass" if corridor_milestones and all(
                "firstOutsideDetected" in item and "firstInsideAfterOutsideDetection" in item
                for item in corridor_milestones.values()
            ) else "warn",
            "Per-corridor milestones preserve the first outside RF-detected sample and the first subsequent inside sample.",
            complete_corridors=sorted(
                corridor for corridor, item in corridor_milestones.items()
                if "firstOutsideDetected" in item and "firstInsideAfterOutsideDetection" in item
            ),
        ),
    ])
    for milestone in corridor_milestones.values():
        outside_sample = milestone.get("firstOutsideDetected")
        inside_sample = milestone.get("firstInsideAfterOutsideDetection")
        if isinstance(outside_sample, Mapping) and isinstance(inside_sample, Mapping):
            outside_seconds = _finite(outside_sample.get("simulationSeconds"))
            inside_seconds = _finite(inside_sample.get("simulationSeconds"))
            if outside_seconds is not None and inside_seconds is not None:
                milestone["secondsFromOutsideDetectionToInside"] = round(inside_seconds - outside_seconds, 3)
    return {
        "status": "fail" if any(item["status"] == "fail" for item in checks) else "pass",
        "checks": checks,
        "sample_count": len(snapshots),
        "complete_sample_count": len(valid),
        "corridor_milestones": corridor_milestones,
    }


def _corridor_transition_capture_complete(samples: Sequence[Mapping[str, Any]]) -> bool:
    expected: set[str] = set()
    outside_detected: set[str] = set()
    inside_after_outside: set[str] = set()
    for snapshot in samples:
        targets = snapshot.get("scenarioTargets") if isinstance(snapshot.get("scenarioTargets"), list) else []
        for raw in targets:
            if not isinstance(raw, Mapping):
                continue
            corridor = raw.get("ingressCorridorId")
            if not isinstance(corridor, str) or not corridor or raw.get("inboundApproachScenario") is not True:
                continue
            expected.add(corridor)
            if raw.get("detectedThisSample") is True and raw.get("outsideSimulationPerimeter") is True:
                outside_detected.add(corridor)
            if corridor in outside_detected and raw.get("airspaceState") == "INSIDE":
                inside_after_outside.add(corridor)
    return bool(expected) and expected <= outside_detected and expected <= inside_after_outside


def capture_snapshots(
    path: Path,
    duration_seconds: float,
    poll_seconds: float = 0.25,
    *,
    stop_after_corridor_transitions: bool = False,
    max_samples: int = 1_024,
) -> list[dict[str, Any]]:
    deadline = time.monotonic() + max(duration_seconds, 0.0)
    samples: list[dict[str, Any]] = []
    last_key: tuple[object, object] | None = None
    last_simulation_seconds: float | None = None
    while True:
        try:
            snapshot = _load_object(path)
        except (FileNotFoundError, OSError, UnicodeDecodeError, json.JSONDecodeError, ValueError):
            snapshot = None
        if snapshot is not None:
            key = (snapshot.get("timestampUtc"), snapshot.get("simulationSeconds"))
            if key != last_key:
                simulation_seconds = _finite(snapshot.get("simulationSeconds"))
                # A decreased simulation clock denotes a restarted session.
                # Discard pre-restart state so an old INSIDE sample cannot be
                # paired with a new session's OUTSIDE origin.
                if (
                    simulation_seconds is not None
                    and last_simulation_seconds is not None
                    and simulation_seconds < last_simulation_seconds - 1.0
                ):
                    samples.clear()
                samples.append(snapshot)
                if len(samples) > max(max_samples, 1):
                    samples.pop(0)
                last_key = key
                last_simulation_seconds = simulation_seconds
                if stop_after_corridor_transitions and _corridor_transition_capture_complete(samples):
                    return samples
        if time.monotonic() >= deadline:
            return samples
        time.sleep(max(min(poll_seconds, 5.0), 0.05))


def run_audit(config: Mapping[str, Any], snapshots: Sequence[Mapping[str, Any]]) -> dict[str, Any]:
    static = audit_config(config)
    runtime = audit_snapshot_series(snapshots)
    statuses = {static["status"], runtime["status"]}
    overall = "fail" if "fail" in statuses else "pass" if runtime["status"] == "pass" else "static_pass_runtime_not_run"
    return {
        "schema_version": "triad.approach_audit.v1",
        "generated_at_utc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "overall_status": overall,
        "scope": {
            "perimeter_is_legal_or_national_boundary": False,
            "scenario_truth_is_sensor_detection": False,
            "detection_only": True,
            "engagement_actions": "none",
        },
        "static_config": static,
        "runtime": runtime,
    }


def render_markdown(report: Mapping[str, Any]) -> str:
    lines = [
        "# Singapore inbound-approach scenario validation",
        "",
        f"**Result:** `{report['overall_status']}`  ",
        f"**Generated:** {report['generated_at_utc']}  ",
        "**Scope:** Detection-only simulation. The configured shape is an evaluation perimeter, not a legal or national boundary.",
        "",
        "## Static route checks",
        "",
    ]
    for check in report["static_config"]["checks"]:
        lines.append(f"- `{check['status'].upper()}` **{check['id']}** — {check['message']}")
    lines.extend(["", "## Configured ingress formations", ""])
    for route in report["static_config"]["routes"]:
        lines.append(
            f"- **{route['name']}** ({route['spawn_count']} targets, `{route['corridor']}`): "
            f"starts {route['minimum_start_distance_to_perimeter_m'] / 1000.0:.2f} km outside, "
            f"entry about {route['estimated_entry_seconds']} s, initial closing rate "
            f"{route['initial_approach_rate_mps']:.1f} m/s, nearest node slant "
            f"{route['nearest_start_node_slant_range_m'] / 1000.0:.2f} km."
        )
    lines.extend(["", "## Runtime checks", ""])
    for check in report["runtime"]["checks"]:
        lines.append(f"- `{check['status'].upper()}` **{check['id']}** — {check['message']}")
    lines.extend([
        "",
        "## Interpretation limits",
        "",
        "- `scenarioTargets` is authored/discovered simulation truth. Only `detectedThisSample=true`, `tracks`, and `detectedRFLinks` demonstrate RF detection.",
        "- A configured 20 km geometric range is a hard ceiling, not a promise of detection; sensitivity, frequency, weather, obstruction and link-budget gates can shorten onset range.",
        "- `APPROACHING` means positive closure on the documented simulation perimeter while still outside. It is not a threat classification or calibrated probability.",
        "- No engagement or effector behavior is present.",
        "",
    ])
    return "\n".join(lines)


def _write_text_atomic(path: Path, value: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_text(value, encoding="utf-8")
    temporary.replace(path)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Audit outside-to-Singapore approach geometry and live detection telemetry.")
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--snapshot", type=Path, default=DEFAULT_SNAPSHOT)
    parser.add_argument("--capture-seconds", type=float, default=0.0)
    parser.add_argument("--poll-seconds", type=float, default=0.25)
    parser.add_argument(
        "--stop-after-corridor-transitions",
        action="store_true",
        help="Stop once every authored inbound corridor has outside-detected and subsequent inside evidence.",
    )
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--markdown-output", type=Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    config = _load_object(args.config)
    if not 0.0 <= args.capture_seconds <= 300.0:
        raise SystemExit("--capture-seconds must be in [0, 300]")
    snapshots = capture_snapshots(
        args.snapshot,
        args.capture_seconds,
        args.poll_seconds,
        stop_after_corridor_transitions=args.stop_after_corridor_transitions,
    )
    report = run_audit(config, snapshots)
    if args.json_output is not None:
        write_json_atomic(report, args.json_output)
    if args.markdown_output is not None:
        _write_text_atomic(args.markdown_output, render_markdown(report))
    print(json.dumps({
        "overall_status": report["overall_status"],
        "static_status": report["static_config"]["status"],
        "runtime_status": report["runtime"]["status"],
        "samples": report["runtime"]["sample_count"],
    }))
    return 1 if report["overall_status"] == "fail" else 0


if __name__ == "__main__":
    raise SystemExit(main())
