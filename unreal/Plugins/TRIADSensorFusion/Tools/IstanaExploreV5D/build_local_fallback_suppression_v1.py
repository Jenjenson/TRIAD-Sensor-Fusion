#!/usr/bin/env python3
"""Build the V5D local-fallback render suppression successor.

This tool removes two exact OBJ group blocks from a hash-pinned 2026-08-31
render derivative.  It never rewrites the canonical geometry, feature
metadata, canonical render source, manifest, or RF shell.  The result is demo
presentation evidence only: provider overlap remains explicitly unresolved.
"""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
from typing import Any, Iterable


ROOT = Path(__file__).resolve().parent
CONTRACT_PATH = ROOT / "local_fallback_suppression_v1.contract.json"
CONTRACT_SCHEMA = (
    "triad.istana_explore_v5d.local_fallback_suppression_contract.v1"
)
METADATA_SCHEMA = (
    "triad.istana_explore_v5d.local_fallback_suppression_metadata.v1"
)
MANIFEST_SCHEMA = (
    "triad.istana_explore_v5d.local_fallback_suppression_manifest.v1"
)
EXPECTED_SOURCE_KEYS = (
    "OSM:way:46521250",
    "OSM:way:1551538490",
)
EXPECTED_GROUPS = (
    "OSM_way_46521250_P00",
    "OSM_way_1551538490_P00",
)


def canonical_bytes(payload: Any) -> bytes:
    return (
        json.dumps(
            payload,
            ensure_ascii=False,
            indent=2,
            sort_keys=True,
        )
        + "\n"
    ).encode("utf-8")


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest().upper()


def file_record(path: Path, *, role: str | None = None) -> dict[str, Any]:
    payload = path.read_bytes()
    record: dict[str, Any] = {
        "file": path.name,
        "bytes": len(payload),
        "sha256": sha256_bytes(payload),
    }
    if role is not None:
        record["role"] = role
    return record


def write_atomic(path: Path, payload: bytes) -> None:
    temporary = path.with_name(f".{path.name}.tmp")
    temporary.write_bytes(payload)
    temporary.replace(path)


def load_contract(path: Path = CONTRACT_PATH) -> dict[str, Any]:
    contract = json.loads(path.read_text(encoding="utf-8"))
    if contract.get("schema") != CONTRACT_SCHEMA:
        raise ValueError("Unexpected local-fallback suppression contract schema.")
    entries = contract.get("suppression", {}).get("sourceKeys", [])
    if tuple(row.get("sourceKey") for row in entries) != EXPECTED_SOURCE_KEYS:
        raise ValueError("Suppression scope must remain the exact two admitted OSM keys.")
    if tuple(row.get("objGroup") for row in entries) != EXPECTED_GROUPS:
        raise ValueError("Suppression OBJ group roster changed.")
    policy = contract.get("authorityPolicy", {})
    if not policy.get("renderOnly") or not policy.get("localFallbackOnly"):
        raise ValueError("Suppression successor lost its render-only local-fallback scope.")
    forbidden_true = (
        "providerOverlapResolved",
        "collisionEnabled",
        "navigationAuthority",
        "sensorOcclusionAuthority",
        "rfGeometryAuthority",
        "rfMaterialAuthority",
        "surveyOrAsBuiltAuthority",
        "facadeOrApertureAuthority",
        "measuredHeightClaimed",
    )
    if any(policy.get(key) is not False for key in forbidden_true):
        raise ValueError("Suppression contract gained forbidden authority.")
    inputs = contract.get("sourceInputs", [])
    if len(inputs) != 7 or not all(
        row.get("mustRemainByteIdentical") is True for row in inputs
    ):
        raise ValueError("Every one of the seven canonical/RF inputs must be immutable.")
    output_names = contract.get("output", {})
    for key in ("filteredRenderObj", "renderMtl", "metadata", "manifest"):
        value = output_names.get(key)
        if not isinstance(value, str) or not value or Path(value).name != value:
            raise ValueError(f"Output '{key}' must be a plain file name.")
    return contract


def validate_inputs(
    input_dir: Path,
    contract: dict[str, Any],
) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    for expected in contract["sourceInputs"]:
        path = input_dir / expected["file"]
        if not path.is_file():
            raise ValueError(f"Missing admitted source input: {path}")
        actual = file_record(path, role=expected["role"])
        for key in ("file", "bytes", "sha256"):
            if actual[key] != expected[key]:
                raise ValueError(
                    f"Source input guard failed for {path.name} ({key}): "
                    f"expected={expected[key]!r} actual={actual[key]!r}"
                )
        records.append(actual)
    return records


def _material_for(surface: str, semantic: str) -> str:
    if surface == "BOTTOM":
        return "MAT_BOTTOM_HIDDEN"
    if surface == "ROOF":
        return f"MAT_ROOF_{semantic}"
    if surface == "WALL":
        return f"MAT_{semantic}"
    raise ValueError(f"Unexpected canonical surface code name: {surface!r}")


def validate_suppression_metadata(
    input_dir: Path,
    contract: dict[str, Any],
) -> list[dict[str, Any]]:
    by_role = {row["role"]: row for row in contract["sourceInputs"]}
    geometry = json.loads(
        (input_dir / by_role["canonicalGeometry"]["file"]).read_text(
            encoding="utf-8"
        )
    )
    feature_payload = json.loads(
        (input_dir / by_role["featureMetadata"]["file"]).read_text(
            encoding="utf-8"
        )
    )
    features = feature_payload.get("features", [])
    triangles = geometry.get("triangles", [])
    vertices = geometry.get("vertices", [])
    surface_codes = geometry.get("surfaceCodes", [])
    expected = contract["suppression"]["expectedCanonical"]
    if (
        len(features) != expected["features"]
        or len(triangles) != expected["triangles"]
        or len(vertices) != expected["vertexLines"]
        or sum(len(row.get("parts", [])) for row in features)
        != expected["polygonParts"]
    ):
        raise ValueError("Canonical geometry/feature census drifted.")

    admitted: list[dict[str, Any]] = []
    for specification in contract["suppression"]["sourceKeys"]:
        feature_index = specification["featureIndex"]
        if not 0 <= feature_index < len(features):
            raise ValueError("Suppression feature index is outside the source roster.")
        feature = features[feature_index]
        if (
            feature.get("sourceKey") != specification["sourceKey"]
            or feature.get("semanticVisualClass")
            != specification["semanticVisualClass"]
            or len(feature.get("parts", [])) != 1
        ):
            raise ValueError(
                f"Suppression feature identity drifted: {specification['sourceKey']}"
            )
        part = feature["parts"][0]
        for key in ("partId", "triangleRange", "vertexRange"):
            if part.get(key) != specification[key]:
                raise ValueError(
                    f"Suppression {key} drifted for {specification['sourceKey']}."
                )
        start, count = part["triangleRange"]
        surface_counts: Counter[str] = Counter()
        material_counts: Counter[str] = Counter()
        for triangle in triangles[start : start + count]:
            if triangle[3] != feature_index or triangle[4] != 0:
                raise ValueError("Suppression triangle range escaped its exact feature/part.")
            surface = surface_codes[triangle[5]]
            surface_counts[surface] += 1
            material_counts[_material_for(surface, feature["semanticVisualClass"])] += 1
        if dict(sorted(surface_counts.items())) != specification["surfaceTriangles"]:
            raise ValueError(
                f"Suppression surface census drifted for {specification['sourceKey']}."
            )
        admitted.append(
            {
                "sourceKey": specification["sourceKey"],
                "featureIndex": feature_index,
                "semanticVisualClass": feature["semanticVisualClass"],
                "partId": part["partId"],
                "objGroup": specification["objGroup"],
                "triangleRange": part["triangleRange"],
                "vertexRange": part["vertexRange"],
                "surfaceTriangles": dict(sorted(surface_counts.items())),
                "materialTriangles": dict(sorted(material_counts.items())),
            }
        )
    return admitted


def scan_obj(payload: bytes) -> dict[str, Any]:
    current_material: str | None = None
    material_triangles: Counter[str] = Counter()
    groups: list[str] = []
    vertex_lines = 0
    texture_coordinate_lines = 0
    triangles = 0
    for raw in payload.splitlines(keepends=True):
        line = raw.decode("utf-8")
        if line.startswith("v "):
            vertex_lines += 1
        elif line.startswith("vt "):
            texture_coordinate_lines += 1
        elif line.startswith("g "):
            groups.append(line[2:].strip())
        elif line.startswith("usemtl "):
            current_material = line[7:].strip()
        elif line.startswith("f "):
            if current_material is None:
                raise ValueError("OBJ face appeared before a material assignment.")
            corners = len(line.split()) - 1
            if corners != 3:
                raise ValueError("Only the canonical triangular OBJ is admitted.")
            triangles += 1
            material_triangles[current_material] += 1
    return {
        "vertexLines": vertex_lines,
        "textureCoordinateLines": texture_coordinate_lines,
        "triangles": triangles,
        "groups": groups,
        "materialTriangles": dict(sorted(material_triangles.items())),
    }


def _selected_lines(payload: bytes, prefix: bytes) -> list[bytes]:
    return [line for line in payload.splitlines(keepends=True) if line.startswith(prefix)]


def filter_render_obj(
    payload: bytes,
    group_names: Iterable[str],
) -> tuple[bytes, list[dict[str, Any]]]:
    targets = tuple(group_names)
    if targets != EXPECTED_GROUPS:
        raise ValueError("Filter scope must be the exact two admitted OBJ groups.")
    target_set = set(targets)
    seen: Counter[str] = Counter()
    omitted: dict[str, bytearray] = {name: bytearray() for name in targets}
    output = bytearray()
    retained_faces: list[bytes] = []
    suppress_group: str | None = None
    for raw in payload.splitlines(keepends=True):
        if raw.startswith(b"g "):
            group = raw[2:].decode("utf-8").strip()
            suppress_group = group if group in target_set else None
            if suppress_group is not None:
                seen[suppress_group] += 1
        if suppress_group is not None:
            omitted[suppress_group].extend(raw)
        else:
            output.extend(raw)
            if raw.startswith(b"f "):
                retained_faces.append(raw)
    if any(seen[name] != 1 for name in targets):
        raise ValueError(
            "Each admitted suppression OBJ group must occur exactly once: "
            + repr(dict(seen))
        )
    result = bytes(output)
    blocks: list[dict[str, Any]] = []
    for name in targets:
        block = bytes(omitted[name])
        stats = scan_obj(block)
        blocks.append(
            {
                "objGroup": name,
                "bytes": len(block),
                "sha256": sha256_bytes(block),
                "triangles": stats["triangles"],
                "materialTriangles": stats["materialTriangles"],
            }
        )

    # These comparisons prove that the successor is a byte-level deletion,
    # never a coordinate/UV rewrite or a surviving-face reorder.
    if _selected_lines(payload, b"v ") != _selected_lines(result, b"v "):
        raise ValueError("The filter changed a source vertex line.")
    if _selected_lines(payload, b"vt ") != _selected_lines(result, b"vt "):
        raise ValueError("The filter changed a source texture-coordinate line.")
    if retained_faces != _selected_lines(result, b"f "):
        raise ValueError("The filter changed or reordered a surviving OBJ face.")
    return result, blocks


def _assert_expected_stats(
    label: str,
    stats: dict[str, Any],
    expected: dict[str, Any],
) -> None:
    for key in (
        "vertexLines",
        "textureCoordinateLines",
        "triangles",
        "materialTriangles",
    ):
        if stats[key] != expected[key]:
            raise ValueError(
                f"{label} OBJ {key} drifted: "
                f"expected={expected[key]!r} actual={stats[key]!r}"
            )


def build(
    input_dir: Path,
    output_dir: Path,
    contract_path: Path = CONTRACT_PATH,
) -> dict[str, Any]:
    input_dir = input_dir.resolve()
    output_dir = output_dir.resolve()
    contract_path = contract_path.resolve()
    if input_dir == output_dir:
        raise ValueError("Output directory must not be the canonical input directory.")
    contract = load_contract(contract_path)
    inputs_before = validate_inputs(input_dir, contract)
    admitted = validate_suppression_metadata(input_dir, contract)
    by_role = {row["role"]: row for row in contract["sourceInputs"]}
    source_obj = input_dir / by_role["canonicalRenderObj"]["file"]
    source_mtl = input_dir / by_role["canonicalRenderMtl"]["file"]
    source_payload = source_obj.read_bytes()
    source_stats = scan_obj(source_payload)
    _assert_expected_stats(
        "Canonical",
        source_stats,
        contract["suppression"]["expectedCanonical"],
    )

    filtered_payload, omitted_blocks = filter_render_obj(
        source_payload,
        (row["objGroup"] for row in admitted),
    )
    filtered_stats = scan_obj(filtered_payload)
    expected_filtered = contract["suppression"]["expectedFiltered"]
    _assert_expected_stats("Filtered", filtered_stats, expected_filtered)
    if (
        len(filtered_payload) != expected_filtered["bytes"]
        or sha256_bytes(filtered_payload) != expected_filtered["sha256"]
        or source_stats["triangles"] - filtered_stats["triangles"]
        != expected_filtered["omittedTriangles"]
    ):
        raise ValueError("Filtered OBJ byte/hash/triangle delta guard failed.")
    by_group = {row["objGroup"]: row for row in omitted_blocks}
    for row in admitted:
        block = by_group[row["objGroup"]]
        if (
            block["triangles"] != row["triangleRange"][1]
            or block["materialTriangles"] != row["materialTriangles"]
        ):
            raise ValueError(
                f"OBJ omission disagrees with canonical metadata for {row['sourceKey']}."
            )

    output_dir.mkdir(parents=True, exist_ok=True)
    output = contract["output"]
    filtered_path = output_dir / output["filteredRenderObj"]
    mtl_path = output_dir / output["renderMtl"]
    metadata_path = output_dir / output["metadata"]
    manifest_path = output_dir / output["manifest"]
    write_atomic(filtered_path, filtered_payload)
    write_atomic(mtl_path, source_mtl.read_bytes())

    inputs_after = validate_inputs(input_dir, contract)
    if inputs_after != inputs_before:
        raise ValueError("A canonical or RF source changed during successor generation.")
    derivative_records = [file_record(filtered_path), file_record(mtl_path)]
    metadata = {
        "schema": METADATA_SCHEMA,
        "status": contract["status"],
        "claimStatus": contract["claimStatus"],
        "sourceEpoch": contract["sourceEpoch"],
        "contract": file_record(contract_path),
        "sourceInputs": inputs_before,
        "sourceInputSetSha256": sha256_bytes(canonical_bytes(inputs_before)),
        "suppression": {
            "mode": contract["suppression"]["mode"],
            "reason": contract["suppression"]["reason"],
            "sourceKeys": admitted,
            "omittedObjBlocks": omitted_blocks,
        },
        "canonicalCounts": source_stats,
        "filteredCounts": filtered_stats,
        "preservation": {
            **contract["preservationPolicy"],
            "sourceInputsMatchedBeforeAndAfter": True,
        },
        "authorityPolicy": contract["authorityPolicy"],
        "outputs": derivative_records,
        "derivativeSetSha256": sha256_bytes(canonical_bytes(derivative_records)),
    }
    write_atomic(metadata_path, canonical_bytes(metadata))
    output_records = [*derivative_records, file_record(metadata_path)]
    manifest = {
        "schema": MANIFEST_SCHEMA,
        "status": contract["status"],
        "claimStatus": contract["claimStatus"],
        "sourceEpoch": contract["sourceEpoch"],
        "contract": file_record(contract_path),
        "tool": file_record(Path(__file__).resolve()),
        "outputs": output_records,
        "outputSetSha256": sha256_bytes(canonical_bytes(output_records)),
        "authorityPolicy": contract["authorityPolicy"],
    }
    write_atomic(manifest_path, canonical_bytes(manifest))
    return manifest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input-dir",
        required=True,
        type=Path,
        help="Hash-pinned 2026-08-31 GeneratedCurrent directory.",
    )
    parser.add_argument(
        "--output-dir",
        required=True,
        type=Path,
        help="Separate directory for the additive filtered derivative.",
    )
    parser.add_argument("--contract", type=Path, default=CONTRACT_PATH)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    manifest = build(args.input_dir, args.output_dir, args.contract)
    print(json.dumps(manifest, ensure_ascii=False, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
