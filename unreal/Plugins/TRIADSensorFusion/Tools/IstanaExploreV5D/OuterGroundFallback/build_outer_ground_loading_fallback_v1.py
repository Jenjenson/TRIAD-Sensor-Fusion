"""Build the bounded V5D visual-only outer-ground loading fallback.

The exact inherited synthetic terrain stops at 1,000 m while four of the six
frozen R24 landmark QA cameras cross that rim.  This source-only package copies
the exact seam and damps its last synthetic radial slope to zero at 1,250 m.
It grants no terrain, survey, collision, navigation, sensor, or RF authority.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import sys
import tempfile
from typing import Any, Iterable, Sequence


PACKAGE_ROOT = Path(__file__).absolute().parent
REPO_ROOT = PACKAGE_ROOT.parents[5]
CONTRACT_PATH = PACKAGE_ROOT / "outer_ground_loading_fallback_v1.contract.json"
SHARED_GEOMETRY_ROOT = (
    REPO_ROOT / "unreal" / "SourceAssets" / "IstanaPublicView"
)
sys.path.insert(0, str(SHARED_GEOMETRY_ROOT))
from public_view_geometry import ObjMesh, file_record, sha256_file, write_json  # noqa: E402


CONTRACT_SCHEMA = (
    "triad.istana_explore_v5d.outer_ground_loading_fallback_contract.v1"
)
MANIFEST_SCHEMA = (
    "triad.istana_explore_v5d.outer_ground_loading_fallback_manifest.v1"
)
AUDIT_SCHEMA = "triad.istana_explore_v5d.outer_ground_loading_fallback_audit.v1"
LOCK_SCHEMA = (
    "triad.istana_explore_v5d.outer_ground_loading_fallback_acceptance_lock.v1"
)
STATUS = "SOURCE_ONLY_RENDER_FALLBACK_READY_NOT_LIVE_UE_INTEGRATED"
CLAIM = (
    "SYNTHETIC_EDGE_CONTINUATION_FOR_PROVIDER_LOADING_ONLY_NOT_TERRAIN_"
    "NOT_SURVEY_NOT_AS_BUILT"
)
GROUP = "V5D_OUTER_GROUND_PROVIDER_LOADING_VISUAL_ONLY"
MATERIAL = "M_IPV5D_OuterGroundLoadingFallback"
SMOOTHING_GROUP = "V5DOuterGroundLoadingFallbackSurface"


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def _load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    _require(isinstance(value, dict), f"JSON root is not an object: {path}")
    return value


def _inside(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def _resolve_bound_path(relative_path: str) -> Path:
    path = Path(os.path.abspath(PACKAGE_ROOT / relative_path))
    _require(
        _inside(path, REPO_ROOT),
        f"Source binding escaped the repository: {relative_path}",
    )
    return path


def _verify_file(binding: dict[str, Any], label: str) -> Path:
    relative_path = binding.get("repositoryRelativePath")
    _require(isinstance(relative_path, str), f"{label} path is missing.")
    path = _resolve_bound_path(relative_path)
    _require(path.is_file(), f"{label} is absent: {path}")
    _require(
        path.stat().st_size == int(binding.get("bytes", -1)),
        f"{label} byte count changed.",
    )
    _require(
        sha256_file(path).upper() == str(binding.get("sha256", "")).upper(),
        f"{label} SHA-256 changed.",
    )
    return path


def _validate_contract(contract: dict[str, Any]) -> None:
    _require(contract.get("schema") == CONTRACT_SCHEMA, "Unexpected contract schema.")
    _require(
        contract.get("status") == STATUS and contract.get("claimStatus") == CLAIM,
        "Outer-ground status or claim boundary changed.",
    )
    for name in (
        "target",
        "sourceBindings",
        "captureGeometryAudit",
        "geometryPolicy",
        "frame",
        "materialPlan",
        "providerLifecycleIntegrationSpec",
        "renderPolicy",
        "expectedTopology",
        "output",
    ):
        _require(isinstance(contract.get(name), dict), f"Missing section: {name}")

    target = contract["target"]
    geometry = contract["geometryPolicy"]
    lifecycle = contract["providerLifecycleIntegrationSpec"]
    render = contract["renderPolicy"]
    materials = contract["materialPlan"]
    expected = contract["expectedTopology"]
    _require(
        target.get("innerRadiusMeters") == 1000.0
        and target.get("outerRadiusMeters") == 1250.0
        and target.get("qaCameraEnvelopeRadiusMeters") == 1200.0
        and target.get("minimumOuterGuardMeters") == 50.0,
        "The bounded 1000/1200/1250 m scope changed.",
    )
    _require(
        geometry.get("shape") == "EXACT_128_SECTOR_CONCENTRIC_ANNULUS"
        and geometry.get("innerSeamRadiusMeters") == 1000.0
        and geometry.get("innerSlopeSampleRadiusMeters") == 975.0
        and geometry.get("outerRadiusMeters") == 1250.0
        and geometry.get("radialBandCount") == 5
        and geometry.get("radialBandWidthMeters") == 50.0
        and geometry.get("angularSectorCount") == 128
        and geometry.get("innerAreaOverlapSquareMeters") == 0.0
        and geometry.get("sourceTerrainModified") is False
        and geometry.get("sourceTerrainSkirtModified") is False
        and geometry.get("buildingsRoadsWaterOrVegetationEncoded") is False
        and geometry.get("outerBoundarySkirtIncluded") is False,
        "Outer-ground geometry or no-overlap policy changed.",
    )
    _require(
        materials.get("orderedSlots") == [MATERIAL]
        and materials.get("previewOnly") is True
        and materials.get("textureCount") == 0
        and materials.get("physicalMaterialClaimed") is False,
        "Preview-material boundary changed.",
    )
    _require(
        lifecycle.get("liveIntegrationIncluded") is False
        and lifecycle.get("defaultBeforePolicyBegins") == "VISIBLE_FAIL_CLOSED"
        and lifecycle.get("providerFallbackVisible") == "VISIBLE"
        and lifecycle.get("providerLoadProgressAtOrAbovePercent") == 98.0
        and lifecycle.get("providerRestoreBelowPercent") == 90.0
        and lifecycle.get("globalProgressIsLandmarkSpecificReadinessProof") is False,
        "Provider-loading integration specification changed.",
    )
    required_false = (
        "collisionEnabled",
        "overlapEventsEnabled",
        "navigationRelevant",
        "castShadow",
        "affectDistanceFieldLighting",
        "affectDynamicIndirectLighting",
        "sensorOcclusionAuthority",
        "rfGeometryAuthority",
        "rfMaterialAuthority",
        "terrainAuthority",
        "demOrDtmAuthority",
        "surveyOrAsBuiltAuthority",
        "geographicSurfaceClassificationAuthority",
        "routeAccessOperationalOrSecurityAuthority",
        "liveUnrealIntegrationIncluded",
    )
    _require(render.get("renderOnly") is True, "Render-only flag changed.")
    _require(
        all(render.get(key) is False for key in required_false),
        "A forbidden authority or runtime side effect was enabled.",
    )
    _require(
        expected
        == {
            "triangleCount": 1280,
            "duplicatedCornerCount": 3840,
            "smoothTriangleCount": 1280,
            "hardTriangleCount": 0,
            "smoothingGroupCount": 1,
            "angularSectorCount": 128,
            "radialBandCount": 5,
            "uniqueInnerSeamPointCount": 128,
            "uniqueOuterBoundaryPointCount": 128,
            "materialTriangleCounts": {MATERIAL: 1280},
            "groupTriangleCounts": {GROUP: 1280},
        },
        "Expected outer-ground topology changed.",
    )


def _verify_sources(contract: dict[str, Any]) -> dict[str, Path]:
    bindings = contract["sourceBindings"]
    expected_keys = {
        "terrainMesh",
        "terrainContract",
        "sharedObjWriter",
        "macDonaldPlacement",
        "macDonaldCaptureHarness",
        "temasekPlacement",
        "temasekCaptureHarness",
    }
    _require(set(bindings) == expected_keys, "Source-binding roster changed.")
    sources = {
        key: _verify_file(value, key)
        for key, value in bindings.items()
    }
    terrain_contract = _load_json(sources["terrainContract"])
    _require(
        terrain_contract.get("schema") == "triad.istana_public_view_contract.v1"
        and terrain_contract.get("scope", {}).get("localContextRadiusMeters")
        == 1000.0
        and terrain_contract.get("contextRequirements", {}).get(
            "terrainBoundaryTreatment"
        )
        == "SYNTHETIC_DOWNWARD_VISUAL_ONLY_SKIRT_NO_COLLISION",
        "Inherited one-kilometre synthetic-terrain contract changed.",
    )
    for key in ("macDonaldCaptureHarness", "temasekCaptureHarness"):
        text = sources[key].read_text(encoding="utf-8")
        _require(
            "120000.0" in text and "New-TargetLockedPose" in text,
            f"{key} no longer exposes the pinned 1200 m QA envelope.",
        )
    return sources


def _pose_world_centimetres(
    placement: dict[str, Any], local_metres: Sequence[float]
) -> tuple[float, float, float]:
    transform = placement["unrealTransform"]
    translation = [float(value) for value in transform["translationCentimeters"]]
    rotation = [float(value) for value in transform["rotationDegrees"]]
    scale = [float(value) for value in transform["scale3D"]]
    _require(len(translation) == len(rotation) == len(scale) == 3, "Bad R24 transform.")
    yaw = math.radians(rotation[1])
    scaled_x = 100.0 * scale[0] * float(local_metres[0])
    scaled_y = 100.0 * scale[1] * float(local_metres[1])
    scaled_z = 100.0 * scale[2] * float(local_metres[2])
    return (
        translation[0] + math.cos(yaw) * scaled_x - math.sin(yaw) * scaled_y,
        translation[1] + math.sin(yaw) * scaled_x + math.cos(yaw) * scaled_y,
        translation[2] + scaled_z,
    )


def _validate_capture_audit(
    contract: dict[str, Any], sources: dict[str, Path]
) -> dict[str, Any]:
    audit = contract["captureGeometryAudit"]
    placements = {
        "MACDONALD": _load_json(sources["macDonaldPlacement"]),
        "TEMASEK": _load_json(sources["temasekPlacement"]),
    }
    rows: list[dict[str, Any]] = []
    for declaration in audit.get("poses", []):
        _require(isinstance(declaration, dict), "Capture pose is not an object.")
        pose_id = str(declaration.get("id", ""))
        placement_key = "MACDONALD" if pose_id.startswith("MACDONALD_") else "TEMASEK"
        actual = _pose_world_centimetres(
            placements[placement_key], declaration["cameraLocalMeters"]
        )
        expected_world = tuple(
            float(value) for value in declaration["cameraWorldCentimeters"]
        )
        _require(
            all(abs(actual[i] - expected_world[i]) <= 1.0e-6 for i in range(3)),
            f"Audited camera world transform changed: {pose_id}",
        )
        radius = math.hypot(actual[0], actual[1]) / 100.0
        _require(
            abs(radius - float(declaration["horizontalRadiusMeters"])) <= 5.1e-7,
            f"Audited camera radius changed: {pose_id}",
        )
        outside = radius > float(audit["sourceTerrainRadiusMeters"])
        _require(
            outside is declaration["outsideSourceTerrain"],
            f"Audited source-terrain containment changed: {pose_id}",
        )
        rows.append(
            {
                "id": pose_id,
                "horizontalRadiusMeters": round(radius, 6),
                "outsideSourceTerrain": outside,
            }
        )
    radii = [float(row["horizontalRadiusMeters"]) for row in rows]
    source_radius = float(audit["sourceTerrainRadiusMeters"])
    outer_radius = float(contract["target"]["outerRadiusMeters"])
    outside_count = sum(radius > source_radius for radius in radii)
    maximum = max(radii)
    _require(len(rows) == audit.get("poseCount") == 6, "Capture pose count changed.")
    _require(
        outside_count == audit.get("posesOutsideSourceTerrainCount") == 4,
        "Capture/source-terrain mismatch count changed.",
    )
    _require(
        abs(maximum - float(audit["maximumAuditedCameraRadiusMeters"])) <= 5.1e-7
        and abs(
            maximum
            - source_radius
            - float(audit["maximumAuditedCameraOverrunMeters"])
        )
        <= 5.1e-7
        and abs(
            outer_radius
            - maximum
            - float(audit["outerFallbackMarginBeyondFarthestAuditedCameraMeters"])
        )
        <= 5.1e-7,
        "Capture-audit maximum or margin changed.",
    )
    return {
        "poseCount": len(rows),
        "posesOutsideSourceTerrainCount": outside_count,
        "maximumAuditedCameraRadiusMeters": round(maximum, 6),
        "maximumAuditedCameraOverrunMeters": round(maximum - source_radius, 6),
        "outerFallbackMarginBeyondFarthestAuditedCameraMeters": round(
            outer_radius - maximum, 6
        ),
        "poses": rows,
    }


def _load_source_rings(
    terrain_path: Path, contract: dict[str, Any]
) -> dict[float, list[tuple[float, float, float]]]:
    geometry = contract["geometryPolicy"]
    sector_count = int(geometry["angularSectorCount"])
    target_radii = (
        float(geometry["innerSlopeSampleRadiusMeters"]),
        float(geometry["innerSeamRadiusMeters"]),
    )
    candidates: dict[float, dict[int, set[tuple[float, float, float]]]] = {
        radius: {sector: set() for sector in range(sector_count)}
        for radius in target_radii
    }
    face_count = 0
    with terrain_path.open("r", encoding="utf-8") as stream:
        for line in stream:
            if line.startswith("v "):
                _, encoded_x, encoded_y, encoded_z = line.split()
                point = (
                    float(encoded_x) / 100.0,
                    -float(encoded_y) / 100.0,
                    float(encoded_z) / 100.0,
                )
                radius = math.hypot(point[0], point[1])
                for target in target_radii:
                    if abs(radius - target) <= 1.0e-6:
                        angle = math.atan2(point[1], point[0]) % (2.0 * math.pi)
                        sector = (
                            int(round(angle * sector_count / (2.0 * math.pi)))
                            % sector_count
                        )
                        expected_angle = 2.0 * math.pi * sector / sector_count
                        _require(
                            abs(
                                math.remainder(
                                    angle - expected_angle, 2.0 * math.pi
                                )
                            )
                            <= 1.0e-9,
                            "Inherited terrain angular sector drifted.",
                        )
                        candidates[target][sector].add(
                            tuple(round(value, 10) for value in point)
                        )
            elif line.startswith("f "):
                face_count += 1
    _require(
        face_count == int(contract["sourceBindings"]["terrainMesh"]["triangleCount"]),
        "Inherited terrain triangle count changed.",
    )
    result: dict[float, list[tuple[float, float, float]]] = {}
    for radius in target_radii:
        ring: list[tuple[float, float, float]] = []
        for sector in range(sector_count):
            points = candidates[radius][sector]
            _require(
                len(points) == 1,
                f"Inherited ring {radius:g} m sector {sector} is not unique.",
            )
            ring.append(next(iter(points)))
        result[radius] = ring
    return result


def _ring_digest(points: Sequence[tuple[float, float, float]]) -> str:
    payload = json.dumps(
        points, separators=(",", ":"), allow_nan=False
    ).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def _build_mesh(
    source_rings: dict[float, list[tuple[float, float, float]]],
    contract: dict[str, Any],
) -> tuple[ObjMesh, dict[str, Any]]:
    geometry = contract["geometryPolicy"]
    inner_radius = float(geometry["innerSeamRadiusMeters"])
    sample_radius = float(geometry["innerSlopeSampleRadiusMeters"])
    outer_radius = float(geometry["outerRadiusMeters"])
    band_count = int(geometry["radialBandCount"])
    band_width = float(geometry["radialBandWidthMeters"])
    sector_count = int(geometry["angularSectorCount"])
    _require(
        math.isclose(inner_radius + band_count * band_width, outer_radius),
        "Radial bands do not exactly reach the outer radius.",
    )
    inner = source_rings[inner_radius]
    previous = source_rings[sample_radius]
    source_step = inner_radius - sample_radius
    slopes = [
        (inner[sector][2] - previous[sector][2]) / source_step
        for sector in range(sector_count)
    ]
    _require(
        max(abs(value) for value in slopes) < 0.05,
        "Inherited synthetic edge slope exceeded the continuation guard.",
    )
    rings: list[list[tuple[float, float, float]]] = []
    for band in range(band_count + 1):
        radius = inner_radius + band * band_width
        distance = radius - inner_radius
        ring: list[tuple[float, float, float]] = []
        for sector in range(sector_count):
            inner_point = inner[sector]
            if band == 0:
                point = inner_point
            else:
                damped_delta = slopes[sector] * distance * (
                    1.0 - 0.5 * distance / (outer_radius - inner_radius)
                )
                point = (
                    radius * inner_point[0] / inner_radius,
                    radius * inner_point[1] / inner_radius,
                    inner_point[2] + damped_delta,
                )
            ring.append(point)
        rings.append(ring)

    mesh = ObjMesh(
        "SM_IPV5D_OuterGroundLoadingFallback_Render",
        source_description=(
            "TRIAD Istana V5D bounded outer-ground provider-loading fallback"
        ),
        attribution_comments=(
            "copies only the exact inherited 1000 m synthetic-terrain seam",
            "damped synthetic visual continuation to 1250 m; not terrain, DEM, DTM, survey, or as-built data",
            "render-only source package; no live Unreal integration, collision, navigation, sensor, RF, route, access, operational, or security authority",
            "no buildings, roads, water, vegetation, markings, or geographic surface classes encoded",
        ),
        ue_legacy_obj_precondition=True,
    )
    for band in range(band_count):
        for sector in range(sector_count):
            nxt = (sector + 1) % sector_count
            a = rings[band][sector]
            b = rings[band + 1][sector]
            c = rings[band + 1][nxt]
            d = rings[band][nxt]
            mesh.add_quad(
                GROUP,
                MATERIAL,
                a,
                b,
                c,
                d,
                ((a[0], a[1]), (b[0], b[1]), (c[0], c[1]), (d[0], d[1])),
                SMOOTHING_GROUP,
            )
    topology = {
        "triangleCount": mesh.triangle_count,
        "duplicatedCornerCount": mesh.vertex_count,
        "smoothTriangleCount": mesh.smooth_triangle_count,
        "hardTriangleCount": mesh.hard_triangle_count,
        "smoothingGroupCount": mesh.smoothing_group_count,
        "angularSectorCount": sector_count,
        "radialBandCount": band_count,
        "uniqueInnerSeamPointCount": len(set(rings[0])),
        "uniqueOuterBoundaryPointCount": len(set(rings[-1])),
        "materialTriangleCounts": {MATERIAL: mesh.triangle_count},
        "groupTriangleCounts": {GROUP: mesh.triangle_count},
    }
    _require(
        topology == contract["expectedTopology"],
        "Generated topology differs from the contract.",
    )
    outer_deltas = [
        rings[-1][sector][2] - inner[sector][2]
        for sector in range(sector_count)
    ]
    plan_area = (
        0.5
        * sector_count
        * (outer_radius * outer_radius - inner_radius * inner_radius)
        * math.sin(2.0 * math.pi / sector_count)
    )
    metrics = {
        "sourceSlopeSampleDistanceMeters": source_step,
        "maximumAbsoluteInheritedRadialSlope": round(
            max(abs(value) for value in slopes), 9
        ),
        "minimumOuterHeightDeltaMeters": round(min(outer_deltas), 9),
        "maximumOuterHeightDeltaMeters": round(max(outer_deltas), 9),
        "planAreaSquareMeters": round(plan_area, 6),
        "innerSeamPointSetSha256": _ring_digest(rings[0]),
        "sourceInnerSeamPointSetSha256": _ring_digest(inner),
        "outerBoundaryPointSetSha256": _ring_digest(rings[-1]),
        "innerSeamExactSourceCopy": rings[0] == inner,
        "innerAreaOverlapSquareMeters": 0.0,
        "outerGuardBeyondQaCameraEnvelopeMeters": round(
            outer_radius
            - float(contract["target"]["qaCameraEnvelopeRadiusMeters"]),
            6,
        ),
    }
    _require(
        metrics["innerSeamExactSourceCopy"] is True
        and metrics["innerSeamPointSetSha256"]
        == metrics["sourceInnerSeamPointSetSha256"],
        "Generated inner seam is not an exact source copy.",
    )
    return mesh, {"topology": topology, "metrics": metrics}


def _write_material_library(path: Path, contract: dict[str, Any]) -> None:
    colour = [
        float(value)
        for value in contract["materialPlan"]["previewBaseColorLinear"]
    ]
    _require(
        len(colour) == 3 and all(0.0 <= value <= 1.0 for value in colour),
        "Bad preview colour.",
    )
    lines = [
        "# TRIAD Istana V5D outer-ground loading-fallback preview material",
        "# Synthetic visual edge continuation only; not terrain, survey, as-built, physical material, collision, navigation, sensor, or RF authority",
        "# Preview only; future Unreal integration requires the separately reviewed provider-loading visibility state machine",
        "",
        f"newmtl {MATERIAL}",
        f"Kd {colour[0]:.6f} {colour[1]:.6f} {colour[2]:.6f}",
        "Ka 0.000000 0.000000 0.000000",
        "Ks 0.010000 0.010000 0.010000",
        "Ns 5.000000",
        "d 1.000000",
        "illum 2",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8", newline="\n")


def _insert_mtl_binding(path: Path, mtl_name: str) -> None:
    lines = path.read_text(encoding="utf-8").splitlines()
    object_index = next(
        index for index, line in enumerate(lines) if line.startswith("o ")
    )
    lines.insert(object_index, f"mtllib {mtl_name}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def _set_sha256(paths: Sequence[Path]) -> str:
    digest = hashlib.sha256()
    for path in sorted(paths, key=lambda value: value.name):
        digest.update(path.name.encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def build(
    output_dir: Path | None = None,
    contract_path: Path | None = None,
) -> dict[str, Any]:
    contract_path = Path(os.path.abspath(contract_path or CONTRACT_PATH))
    _require(
        _inside(contract_path, PACKAGE_ROOT),
        "Outer-ground contract escaped its source package.",
    )
    contract = _load_json(contract_path)
    _validate_contract(contract)
    sources = _verify_sources(contract)
    capture_audit = _validate_capture_audit(contract, sources)
    source_rings = _load_source_rings(sources["terrainMesh"], contract)
    mesh, geometry_result = _build_mesh(source_rings, contract)

    output = contract["output"]
    output_dir = Path(
        os.path.abspath(output_dir or PACKAGE_ROOT / str(output["directory"]))
    )
    _require(
        output_dir != Path(output_dir.anchor),
        "Refusing a filesystem root output.",
    )
    output_dir.mkdir(parents=True, exist_ok=True)
    mesh_path = output_dir / str(output["renderMesh"])
    mtl_path = output_dir / str(output["materialLibrary"])
    audit_path = output_dir / str(output["audit"])
    manifest_path = output_dir / str(output["manifest"])
    lock_path = output_dir / str(output["acceptanceLock"])

    _write_material_library(mtl_path, contract)
    mesh.write(mesh_path)
    _insert_mtl_binding(mesh_path, mtl_path.name)
    audit_payload = {
        "schema": AUDIT_SCHEMA,
        "status": STATUS,
        "claimStatus": CLAIM,
        "diagnosis": {
            "sourceTerrainRadiusMeters": 1000.0,
            "auditedPoseCount": capture_audit["poseCount"],
            "posesOutsideSourceTerrainCount": capture_audit[
                "posesOutsideSourceTerrainCount"
            ],
            "maximumAuditedCameraRadiusMeters": capture_audit[
                "maximumAuditedCameraRadiusMeters"
            ],
            "maximumAuditedCameraOverrunMeters": capture_audit[
                "maximumAuditedCameraOverrunMeters"
            ],
            "diagnosticConclusion": (
                "The black/void foreground in the frozen TelemetryOnly R24 "
                "views is consistent with cameras or their near-ground footprint "
                "crossing the exact 1000 m synthetic-terrain rim; it is not "
                "evidence that provider terrain failed."
            ),
        },
        "captureGeometryAudit": capture_audit,
        "geometryPolicy": contract["geometryPolicy"],
        "geometryMetrics": geometry_result["metrics"],
        "providerLifecycleIntegrationSpec": contract[
            "providerLifecycleIntegrationSpec"
        ],
        "renderPolicy": contract["renderPolicy"],
        "integrationDisposition": {
            "liveUnrealIntegrated": False,
            "mapOrActorChanged": False,
            "sourceTerrainChanged": False,
            "providerPolicyChanged": False,
            "nativeAssetCreated": False,
            "visualImprovementClaimedInCurrentMap": False,
        },
    }
    write_json(audit_path, audit_payload)
    primary_paths = [mesh_path, mtl_path, audit_path]
    manifest: dict[str, Any] = {
        "schema": MANIFEST_SCHEMA,
        "status": STATUS,
        "claimStatus": CLAIM,
        "contract": {
            "path": contract_path.name,
            "bytes": contract_path.stat().st_size,
            "sha256": sha256_file(contract_path),
        },
        "generator": {
            "path": Path(__file__).name,
            "bytes": Path(__file__).stat().st_size,
            "sha256": sha256_file(Path(__file__)),
        },
        "sourceBindings": contract["sourceBindings"],
        "captureGeometryAudit": capture_audit,
        "geometryPolicy": contract["geometryPolicy"],
        "geometryMetrics": geometry_result["metrics"],
        "topology": geometry_result["topology"],
        "frame": contract["frame"],
        "materialPlan": contract["materialPlan"],
        "providerLifecycleIntegrationSpec": contract[
            "providerLifecycleIntegrationSpec"
        ],
        "renderPolicy": contract["renderPolicy"],
        "files": [
            file_record("V5D_OUTER_GROUND_LOADING_RENDER_ONLY", mesh_path, mesh),
            file_record(
                "V5D_OUTER_GROUND_LOADING_PREVIEW_MTL_ONLY", mtl_path
            ),
            file_record(
                "V5D_OUTER_GROUND_LOADING_CAPTURE_GEOMETRY_AUDIT",
                audit_path,
            ),
        ],
        "primaryOutputSetSha256": _set_sha256(primary_paths),
        "limitations": [
            "The mesh copies only the exact inherited 1000 m synthetic-terrain seam and damps its last 25 m radial slope; no measured elevation is consumed or inferred.",
            "The annulus encodes no buildings, roads, water, vegetation, markings, land cover, or geographic surface classes.",
            "The R24 diagnosis uses exact camera/terrain geometry plus visual observation of TelemetryOnly captures; it is not provider-ready evidence.",
            "No live Unreal asset, actor, component, map edit, visibility transition, or provider-policy change is included.",
            "Future integration must remain render-only and visible fail-closed while provider context is unavailable, then hide only under the existing separately validated provider-ready state.",
            "No collision, overlap, navigation, sensor-occlusion, RF, terrain, DEM/DTM, survey, as-built, route, access, operational, security, or physical-material authority is asserted.",
        ],
    }
    write_json(manifest_path, manifest)
    locked_paths = [
        contract_path,
        Path(__file__),
        *primary_paths,
        manifest_path,
    ]
    lock_payload = {
        "schema": LOCK_SCHEMA,
        "status": STATUS,
        "claimStatus": CLAIM,
        "files": [
            {
                "path": path.name,
                "scope": (
                    "PACKAGE"
                    if path.parent == PACKAGE_ROOT
                    else "GENERATED"
                ),
                "bytes": path.stat().st_size,
                "sha256": sha256_file(path),
            }
            for path in locked_paths
        ],
        "lockedSetSha256": _set_sha256(locked_paths),
        "topology": geometry_result["topology"],
        "geometryMetrics": geometry_result["metrics"],
        "renderPolicy": contract["renderPolicy"],
    }
    write_json(lock_path, lock_payload)

    expected_names = {
        str(output["renderMesh"]),
        str(output["materialLibrary"]),
        str(output["audit"]),
        str(output["manifest"]),
        str(output["acceptanceLock"]),
    }
    actual_names = {
        path.name for path in output_dir.iterdir() if path.is_file()
    }
    _require(
        actual_names == expected_names,
        "Output directory contains unexpected files: "
        f"{sorted(actual_names ^ expected_names)}",
    )
    return manifest


def check_committed_outputs(
    contract_path: Path | None = None,
) -> dict[str, Any]:
    contract_path = Path(os.path.abspath(contract_path or CONTRACT_PATH))
    contract = _load_json(contract_path)
    _validate_contract(contract)
    committed = Path(
        os.path.abspath(PACKAGE_ROOT / contract["output"]["directory"])
    )
    names = [
        str(contract["output"][key])
        for key in (
            "renderMesh",
            "materialLibrary",
            "audit",
            "manifest",
            "acceptanceLock",
        )
    ]
    with tempfile.TemporaryDirectory(
        prefix="triad_v5d_outer_ground_"
    ) as directory:
        candidate = Path(directory)
        manifest = build(candidate, contract_path)
        for name in names:
            expected_path = committed / name
            actual_path = candidate / name
            _require(
                expected_path.is_file()
                and expected_path.read_bytes() == actual_path.read_bytes(),
                f"Committed deterministic output differs: {name}",
            )
    return manifest


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--contract", type=Path)
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args(
        list(argv) if argv is not None else None
    )
    if arguments.check and arguments.output is not None:
        parser.error("--check and --output are mutually exclusive")
    manifest = (
        check_committed_outputs(arguments.contract)
        if arguments.check
        else build(arguments.output, arguments.contract)
    )
    print(
        json.dumps(
            {
                "status": manifest["status"],
                "triangles": manifest["topology"]["triangleCount"],
                "innerRadiusMeters": manifest["geometryPolicy"][
                    "innerSeamRadiusMeters"
                ],
                "outerRadiusMeters": manifest["geometryPolicy"][
                    "outerRadiusMeters"
                ],
                "posesOutsideSourceTerrain": manifest[
                    "captureGeometryAudit"
                ]["posesOutsideSourceTerrainCount"],
                "maximumAuditedCameraRadiusMeters": manifest[
                    "captureGeometryAudit"
                ]["maximumAuditedCameraRadiusMeters"],
                "collisionEnabled": manifest["renderPolicy"][
                    "collisionEnabled"
                ],
                "sensorOcclusionAuthority": manifest["renderPolicy"][
                    "sensorOcclusionAuthority"
                ],
                "rfGeometryAuthority": manifest["renderPolicy"][
                    "rfGeometryAuthority"
                ],
                "liveUnrealIntegrationIncluded": manifest["renderPolicy"][
                    "liveUnrealIntegrationIncluded"
                ],
            },
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
