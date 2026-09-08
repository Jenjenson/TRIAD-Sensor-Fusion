import hashlib
import importlib.util
import json
import math
import shutil
import sys
from collections import Counter, defaultdict
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[4]
PACK = ROOT / "unreal/SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/TropicalUmbrellaHeroCandidate"
BUILDER = PACK / "build_tropical_umbrella_hero_candidate.py"
CONTRACT = PACK / "tropical_umbrella_hero_candidate.contract.json"
AUDIT = PACK / "OfflineAudit/tropical_umbrella_hero_candidate.audit.json"
MTL = PACK / "Generated/M_IPV5D_TropicalUmbrellaHeroCandidate.mtl"
README = PACK / "README.md"
MATERIALS = ("Bark", "LeafLive", "LeafDry")


def load_builder():
    spec = importlib.util.spec_from_file_location("triad_tropical_umbrella_candidate_test", BUILDER)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def artifact_receipt(path: Path, root: Path) -> dict[str, object]:
    return {"path": path.relative_to(root).as_posix(), "bytes": path.stat().st_size, "sha256": sha256(path)}


def independent_obj_proof(path: Path) -> dict[str, object]:
    """Test-side parser intentionally independent of the production checker."""
    positions: list[tuple[float, float, float]] = []
    uv_count = 0
    normal_count = 0
    material_order: list[str] = []
    material_faces: Counter[str] = Counter()
    edge_count: Counter[tuple[str, int, int]] = Counter()
    edge_direction: Counter[tuple[str, int, int]] = Counter()
    component_volume: defaultdict[str, float] = defaultdict(float)
    components: set[str] = set()
    active_material = ""
    active_component = ""
    minimum = [math.inf, math.inf, math.inf]
    maximum = [-math.inf, -math.inf, -math.inf]
    mtllibs: list[str] = []

    for line_number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        fields = line.split()
        if fields[0] == "v":
            assert len(fields) == 4, line_number
            point = tuple(float(value) for value in fields[1:])
            assert all(math.isfinite(value) for value in point), line_number
            positions.append(point)
            for axis in range(3):
                minimum[axis] = min(minimum[axis], point[axis])
                maximum[axis] = max(maximum[axis], point[axis])
        elif fields[0] == "vt":
            assert len(fields) == 3 and all(math.isfinite(float(value)) for value in fields[1:]), line_number
            uv_count += 1
        elif fields[0] == "vn":
            assert len(fields) == 4
            normal = tuple(float(value) for value in fields[1:])
            assert all(math.isfinite(value) for value in normal), line_number
            assert abs(math.sqrt(sum(value * value for value in normal)) - 1.0) <= 1.0e-5, line_number
            normal_count += 1
        elif fields[0] == "mtllib":
            assert len(fields) == 2
            mtllibs.append(fields[1])
        elif fields[0] == "o":
            assert len(fields) == 2 and not fields[1].upper().startswith(("UCX_", "UBX_", "USP_", "UCP_"))
        elif fields[0] == "usemtl":
            assert len(fields) == 2 and fields[1] in MATERIALS
            active_material = fields[1]
            active_component = ""
            material_order.append(active_material)
        elif fields[0] == "g":
            assert len(fields) == 2 and active_material
            active_component = fields[1]
            assert active_component not in components
            assert not active_component.upper().startswith(("UCX_", "UBX_", "USP_", "UCP_"))
            components.add(active_component)
        elif fields[0] == "f":
            assert len(fields) == 4 and active_material and active_component, line_number
            indices = []
            for token in fields[1:]:
                parts = token.split("/")
                assert len(parts) == 3 and all(parts), line_number
                triplet = tuple(int(part) for part in parts)
                assert triplet[0] > 0 and triplet[0] == triplet[1] == triplet[2], line_number
                assert triplet[0] <= len(positions) and triplet[1] <= uv_count and triplet[2] <= normal_count, line_number
                indices.append(triplet[0] - 1)
            assert len(set(indices)) == 3, line_number
            a, b, c = (positions[index] for index in indices)
            ab = (b[0] - a[0], b[1] - a[1], b[2] - a[2])
            ac = (c[0] - a[0], c[1] - a[1], c[2] - a[2])
            cross = (
                ab[1] * ac[2] - ab[2] * ac[1],
                ab[2] * ac[0] - ab[0] * ac[2],
                ab[0] * ac[1] - ab[1] * ac[0],
            )
            assert math.sqrt(sum(value * value for value in cross)) > 2.0e-10, line_number
            material_faces[active_material] += 1
            component_volume[active_component] += (
                a[0] * (b[1] * c[2] - b[2] * c[1])
                + a[1] * (b[2] * c[0] - b[0] * c[2])
                + a[2] * (b[0] * c[1] - b[1] * c[0])
            ) / 6.0
            for first, second in ((indices[0], indices[1]), (indices[1], indices[2]), (indices[2], indices[0])):
                low, high = sorted((first, second))
                key = (active_component, low, high)
                edge_count[key] += 1
                edge_direction[key] += 1 if first == low else -1
        else:
            pytest.fail(f"Unsupported OBJ directive {fields[0]!r} at line {line_number}")

    assert positions and len(positions) == uv_count == normal_count
    assert mtllibs == ["M_IPV5D_TropicalUmbrellaHeroCandidate.mtl"]
    assert material_order == list(MATERIALS) and set(material_faces) == set(MATERIALS)
    assert components == set(component_volume)
    assert edge_count and all(value == 2 for value in edge_count.values())
    assert all(value == 0 for value in edge_direction.values())
    volumes = [abs(value) for value in component_volume.values()]
    assert volumes and all(math.isfinite(value) and value > 1.0e-12 for value in volumes)
    span = [maximum[axis] - minimum[axis] for axis in range(3)]
    live, dry = material_faces["LeafLive"], material_faces["LeafDry"]
    assert minimum[2] == 0.0
    assert 18.0 <= max(span[0], span[1]) <= 29.5 and 10.0 <= span[2] <= 16.5
    assert live > material_faces["Bark"] and 0.005 <= dry / (live + dry) <= 0.04
    return {
        "vertices": len(positions),
        "triangles": sum(material_faces.values()),
        "trianglesByMaterial": dict(sorted(material_faces.items())),
        "closedComponentGroups": len(components),
        "minimumPositiveComponentVolume": round(min(volumes), 12),
    }


def copy_pack(tmp_path: Path) -> Path:
    destination = tmp_path / "candidate"
    shutil.copytree(PACK, destination)
    return destination


def synchronize_obj_receipt(pack: Path, relative: str) -> None:
    contract_path = pack / CONTRACT.name
    contract = json.loads(contract_path.read_text(encoding="utf-8"))
    obj_path = pack / relative
    for row in contract["geometry"]["stats"]:
        if row["obj"]["path"] == relative:
            row["obj"] = artifact_receipt(obj_path, pack)
            break
    else:
        pytest.fail("Mutated OBJ was not present in the contract roster")
    contract_path.write_text(json.dumps(contract, indent=2, sort_keys=True, ensure_ascii=False) + "\n", encoding="utf-8")


def test_candidate_pack_passes_its_fail_closed_checker():
    assert load_builder().check(PACK)["status"] == "PASS"


def test_delivery_is_honestly_source_only_and_dormant():
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    assert contract["status"] == "UNADMITTED_SOURCE_ONLY_VISUAL_CANDIDATE"
    state = contract["deliveryState"]
    assert state["sourceImplemented"] is True
    assert all(state[key] is False for key in ("unrealPackagesWritten", "meshAssetsMaterialized", "mapModified", "runtimeIntegrated", "nativeCaptured", "humanAccepted"))
    gates = contract["futureIntegrationBoundary"]
    assert gates["compiledTrustAnchorsPopulated"] is False
    assert gates["materializationReachable"] is False
    assert gates["runtimeSelectionCompiledFalse"] is True
    assert gates["runtimeActivationCompiledFalse"] is True


def test_candidate_has_three_distinct_forms_and_real_geometric_lods():
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    stats = contract["geometry"]["stats"]
    assert len(stats) == 9
    assert len({row["obj"]["sha256"] for row in stats}) == 9
    for variant in "ABC":
        rows = [row for row in stats if row["variant"] == variant]
        assert [row["lod"] for row in rows] == ["LOD0", "LOD1", "LOD2"]
        assert rows[0]["triangles"] > rows[1]["triangles"] > rows[2]["triangles"]
        assert all(row["closedTwoManifoldIndexedEdges"] for row in rows)
        assert all(row["oppositeOrientationPerComponentEdge"] for row in rows)
        assert all(row["positiveFiniteVolumePerComponent"] for row in rows)
        assert all(row["oneToOnePositionUvNormalStreams"] for row in rows)
        assert all(row["identicalPositionUvNormalFaceIndices"] for row in rows)
        assert all(row["zeroCollapsedEdges"] for row in rows)
        assert all(row["zeroDegenerateTriangles"] for row in rows)


def test_test_side_parser_independently_proves_every_serialized_obj():
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    for row in contract["geometry"]["stats"]:
        proof = independent_obj_proof(PACK / row["obj"]["path"])
        assert proof["vertices"] == row["vertices"]
        assert proof["triangles"] == row["triangles"]
        assert proof["trianglesByMaterial"] == row["trianglesByMaterial"]
        assert proof["closedComponentGroups"] == row["closedComponentGroups"]
        assert proof["minimumPositiveComponentVolume"] == row["minimumPositiveComponentVolume"]


def test_dense_canopy_is_volumetric_not_a_full_tree_billboard():
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    volumetric = contract["geometry"]["volumetric"]
    assert volumetric == {
        "closedFiniteThicknessLeaflets": True,
        "closedTubeTrunksBranchesAndTwigs": True,
        "closedTwoManifoldIndexedEdgesEveryLod": True,
        "fullTreeBillboardsOrCrossPlaneImpostors": False,
        "oneToOnePositionUvNormalStreamsAndIndicesEveryLod": True,
        "oppositeOrientationPerComponentEdgeEveryLod": True,
        "positiveFiniteVolumePerComponentEveryLod": True,
        "rootPlaneExactlyZeroEveryLod": True,
        "zeroCollapsedEdgesEveryLod": True,
        "zeroDegenerateTrianglesEveryLod": True,
    }
    for row in contract["geometry"]["stats"]:
        faces = row["trianglesByMaterial"]
        assert faces["LeafLive"] > faces["Bark"]
        assert 0.005 <= row["dryLeafTriangleFraction"] <= 0.04
        assert row["spanXYZ"][0] > 4.0 and row["spanXYZ"][1] > 4.0 and row["spanXYZ"][2] > 4.0


def test_geography_transforms_and_simulation_authorities_are_immutable():
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    assert all(value is False for value in contract["preservation"].values())
    gates = contract["futureIntegrationBoundary"]
    assert gates["exactExistingSourceInstanceKeysAndWorldTransformsMustBeCopiedWithoutMutation"] is True
    assert gates["newOrDeletedTreePlacementsAllowed"] is False
    assert gates["sourceTreeSuppressionMustBeAtomicAndRollbackComplete"] is True
    assert gates["exactTreeRealismUmbrellaMeshMandatoryFallback"] == "/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Umbrella_NearLOD0.SM_IPV5D_Tree_Umbrella_NearLOD0"


def test_public_reference_is_form_only_not_species_or_inventory_truth():
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    boundary = contract["visualReferenceBoundary"]
    assert boundary["authority"] == "docs/ISTANA_PUBLIC_VEGETATION_REFERENCE.md"
    assert boundary["speciesIdentityClaimed"] is False
    assert boundary["currentIstanaTreeMatchClaimed"] is False
    assert boundary["placementOrInventoryClaimed"] is False
    joined = " ".join(contract["claimBoundaries"])
    assert "No species identity" in joined
    assert "not visible in the current R29 map" in joined


def test_every_artifact_receipt_and_offline_audit_are_exact():
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    audit = json.loads(AUDIT.read_text(encoding="utf-8"))
    assert audit["status"] == "DETERMINISTIC_OFFLINE_SOURCE_AUDIT_NOT_RENDERER_ACCEPTANCE"
    assert audit["contractPath"] == CONTRACT.name
    assert contract["offlineAudit"] == artifact_receipt(AUDIT, PACK)
    image = PACK / audit["image"]["path"]
    assert audit["image"]["bytes"] == image.stat().st_size
    assert audit["image"]["sha256"] == sha256(image)
    assert contract["offlineAuditImage"] == artifact_receipt(image, PACK)
    assert contract["materialLibrary"] == artifact_receipt(MTL, PACK)
    assert contract["readme"] == artifact_receipt(README, PACK)
    for row in contract["geometry"]["stats"]:
        path = PACK / row["obj"]["path"]
        assert row["obj"]["bytes"] == path.stat().st_size
        assert row["obj"]["sha256"] == sha256(path)


def test_appended_mtl_is_rejected_even_with_synchronized_receipt(tmp_path):
    pack = copy_pack(tmp_path)
    mtl = pack / "Generated/M_IPV5D_TropicalUmbrellaHeroCandidate.mtl"
    mtl.write_bytes(mtl.read_bytes() + b"\n# adversarial append\n")
    contract_path = pack / CONTRACT.name
    contract = json.loads(contract_path.read_text(encoding="utf-8"))
    contract["materialLibrary"] = artifact_receipt(mtl, pack)
    contract_path.write_text(json.dumps(contract, indent=2, sort_keys=True, ensure_ascii=False) + "\n", encoding="utf-8")
    with pytest.raises(AssertionError, match="Material library content"):
        load_builder().check(pack)


def test_synchronized_receipt_zero_area_obj_is_rejected(tmp_path):
    pack = copy_pack(tmp_path)
    relative = "Generated/SM_IPV5D_TropicalUmbrellaHero_A_LOD0.obj"
    obj = pack / relative
    lines = obj.read_text(encoding="utf-8").splitlines()
    first_face = next(line for line in lines if line.startswith("f "))
    first_index, second_index = [int(token.split("/")[0]) for token in first_face.split()[1:3]]
    vertex_line_indices = [index for index, line in enumerate(lines) if line.startswith("v ")]
    first_vertex_fields = lines[vertex_line_indices[first_index - 1]].split()
    lines[vertex_line_indices[second_index - 1]] = " ".join(["v", *first_vertex_fields[1:]])
    obj.write_text("\n".join(lines) + "\n", encoding="utf-8")
    synchronize_obj_receipt(pack, relative)
    with pytest.raises(AssertionError, match="zero or non-finite area"):
        load_builder().check(pack)


def test_synchronized_receipt_collapsed_edge_obj_is_rejected(tmp_path):
    pack = copy_pack(tmp_path)
    relative = "Generated/SM_IPV5D_TropicalUmbrellaHero_A_LOD0.obj"
    obj = pack / relative
    lines = obj.read_text(encoding="utf-8").splitlines()
    face_line_index = next(index for index, line in enumerate(lines) if line.startswith("f "))
    fields = lines[face_line_index].split()
    fields[2] = fields[1]
    lines[face_line_index] = " ".join(fields)
    obj.write_text("\n".join(lines) + "\n", encoding="utf-8")
    synchronize_obj_receipt(pack, relative)
    with pytest.raises(AssertionError, match="collapsed edge"):
        load_builder().check(pack)


def test_builder_dependency_and_builder_are_hash_pinned():
    contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
    assert contract["builder"]["sha256"] == sha256(BUILDER)
    dependency = ROOT / contract["pinnedMeshUtility"]["path"]
    assert dependency.is_file()
    assert contract["pinnedMeshUtility"]["sha256"] == sha256(dependency)
    for pin in contract["pinnedExistingVisualAuthority"].values():
        source = ROOT / pin["path"]
        assert source.is_file()
        assert pin["bytes"] == source.stat().st_size
        assert pin["sha256"] == sha256(source)


def test_candidate_remains_source_only_and_post_r33_integration_is_fail_closed():
    assert not list(PACK.rglob("*.uasset"))
    assert not list(PACK.rglob("*.umap"))
    needle = "TropicalUmbrellaHero"
    source_root = ROOT / "unreal/Plugins/TRIADSensorFusion/Source"
    scripts_root = ROOT / "scripts"
    allowed_source_receipts = {
        "TRIADSensorFusion/Public/TRIADIstanaExploreV5DTropicalUmbrellaHeroActor.h",
        "TRIADSensorFusion/Private/TRIADIstanaExploreV5DTropicalUmbrellaHeroActor.cpp",
        "TRIADSensorFusion/Private/Tests/TRIADIstanaExploreV5DTropicalUmbrellaHeroActorTests.cpp",
        "TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary.h",
        "TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary.cpp",
        "TRIADSensorFusionEditor/Private/Tests/TRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibraryTests.cpp",
    }
    source_receipts = set()
    searched = []
    for path in source_root.rglob("*"):
        if path.is_file() and path.suffix.lower() in {".cpp", ".h", ".inl"}:
            searched.append(path)
            if needle in path.read_text(encoding="utf-8", errors="ignore"):
                source_receipts.add(path.relative_to(source_root).as_posix())
    assert source_receipts == allowed_source_receipts
    for path in scripts_root.rglob("*"):
        if path.is_file() and path.suffix.lower() in {".ps1", ".json"}:
            searched.append(path)
            assert needle not in path.read_text(encoding="utf-8", errors="ignore"), path
    assert searched

    runtime_source = (
        source_root
        / "TRIADSensorFusion/Private/TRIADIstanaExploreV5DTropicalUmbrellaHeroActor.cpp"
    ).read_text(encoding="utf-8")
    editor_header = (
        source_root
        / "TRIADSensorFusionEditor/Public/TRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary.h"
    ).read_text(encoding="utf-8")
    editor_source = (
        source_root
        / "TRIADSensorFusionEditor/Private/TRIADIstanaExploreV5DTropicalUmbrellaHeroEditorLibrary.cpp"
    ).read_text(encoding="utf-8")
    assert "constexpr bool bRuntimeSelectionCompiled = false;" in runtime_source
    assert "constexpr bool bRuntimeActivationCompiled = false;" in runtime_source
    assert "constexpr bool bMaterializationAndSwapCompiled = false;" in editor_source
    materializer = "MaterializeAndStageTrustedTropicalUmbrellaHeroInternal"
    assert editor_header.count(materializer) + editor_source.count(materializer) == 2
    assert editor_header.count("UFUNCTION(") == 1
    assert "UFUNCTION(BlueprintCallable" in editor_header
    assert editor_header.index("InspectTropicalUmbrellaHeroReceipts") < editor_header.index(materializer)
