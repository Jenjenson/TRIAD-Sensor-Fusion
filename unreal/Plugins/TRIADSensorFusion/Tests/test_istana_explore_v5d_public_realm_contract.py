from __future__ import annotations

import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[4]
PACKAGE = (
    REPO
    / "unreal/SourceAssets/IstanaPublicViewExploreV5D/PublicRealm"
)
GENERATED = PACKAGE / "Generated"
CONTRACT_PATH = PACKAGE / "istana_public_view_v5d_public_realm.contract.json"
BUILDER = PACKAGE / "build_public_realm_v5d.py"
CORE = GENERATED / "SM_IPV5D_PublicRealm_Core_Render.obj"
FALLBACK = GENERATED / "SM_IPV5D_PublicRealm_Fallback_Render.obj"
MTL = GENERATED / "SM_IPV5D_PublicRealm_Render.mtl"
FEATURES = GENERATED / "IstanaPublicViewV5DPublicRealm.features.json"
MANIFEST = GENERATED / "IstanaPublicViewV5DPublicRealm.manifest.json"
LOCK = GENERATED / "IstanaPublicViewV5DPublicRealm.acceptance.lock.json"
OUTPUT_NAMES = (
    CORE.name,
    FALLBACK.name,
    MTL.name,
    FEATURES.name,
    MANIFEST.name,
    LOCK.name,
)
CANONICAL_PROVIDER_CLIP_RELATIVE_PATH = (
    "../../../Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DContextPolicyActor.cpp"
)
HISTORICAL_PROVIDER_CLIP = (
    PACKAGE
    / "NativeSourceClosure/TRIADIstanaExploreV5DContextPolicyActor.cpp"
)
LIVE_CONTEXT_POLICY = (
    REPO
    / "unreal/Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DContextPolicyActor.cpp"
)
PROVIDER_CLIP_NUMERIC_LINES = (
    "constexpr int32 RequiredSiteClipSplinePoints = 64;",
    "constexpr double RequiredSiteClipCenterXMeters = 0.0;",
    "constexpr double RequiredSiteClipCenterYMeters = 55.0;",
    "constexpr double RequiredSiteClipSemiAxisXMeters = 185.0;",
    "constexpr double RequiredSiteClipSemiAxisYMeters = 245.0;",
    "constexpr double RequiredSiteClipRipple3Amplitude = 0.035;",
    "constexpr double RequiredSiteClipRipple3PhaseRadians = 0.43;",
    "constexpr double RequiredSiteClipRipple5Amplitude = 0.015;",
    "constexpr double RequiredSiteClipRipple5PhaseRadians = -0.91;",
    "constexpr double RequiredSiteClipRipple7Amplitude = 0.006;",
    "constexpr double RequiredSiteClipRipple7PhaseRadians = 1.37;",
    "constexpr double RequiredSiteClipSegmentParameterEpsilon = 1.0e-6;",
)
PROVIDER_CLIP_REQUIRED_SYMBOLS = (
    "ExpectedProviderSiteClipSplinePoints()",
    "ExpectedProviderSiteClipCenterCentimeters()",
    "ExpectedProviderSiteClipSemiAxesCentimeters()",
    "ExpectedProviderSiteClipRippleAmplitudes()",
    "ExpectedProviderSiteClipRipplePhasesRadians()",
    "ExpectedProviderSiteClipSegmentParameterEpsilon()",
    "ExpectedProviderSiteClipPointCentimeters(int32 Index)",
    "EvaluateProviderSiteClipSignedInwardDistanceCentimeters(",
)
EXPECTED_MATERIAL_ORDER = (
    "MI_IPV5C_OfficialPlanningRoadZone",
    "MI_IPV5C_OfficialPlanningRoadGraphic",
    "MI_IPV5D_PublicRealmConcrete",
)
EXPECTED_STATUS = (
    "VISUAL_ASSUMPTION_BOUND_OFFLINE_RENDER_ONLY_NOT_LIVE_UE_INTEGRATED"
)


def load_json(path: Path) -> dict[str, object]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise AssertionError(f"Expected a JSON object: {path}")
    return payload


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ordered_prefixed_values(path: Path, prefix: str) -> tuple[str, ...]:
    return tuple(
        line[len(prefix) :]
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.startswith(prefix)
    )


def logical_obj_triangles(
    path: Path,
) -> tuple[list[tuple[float, float, float]], list[tuple[int, int, int]]]:
    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[int, int, int]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("v "):
            _, x, encoded_y, z = line.split()
            vertices.append((float(x) / 100.0, -float(encoded_y) / 100.0, float(z) / 100.0))
        elif line.startswith("f "):
            indices = tuple(
                int(token.split("/", 1)[0]) - 1 for token in line.split()[1:]
            )
            if len(indices) != 3:
                raise AssertionError(f"Non-triangle OBJ face in {path}")
            faces.append(indices)
    return vertices, faces


def provider_polygon(contract: dict[str, object]) -> list[tuple[float, float]]:
    declaration = contract["providerPartition"]
    assert isinstance(declaration, dict)
    center_x, center_y = declaration["centerMeters"]
    semi_x, semi_y = declaration["semiAxesMeters"]
    ripple = declaration["radialRipple"]
    count = declaration["splinePoints"]
    assert isinstance(ripple, dict) and isinstance(count, int)
    points: list[tuple[float, float]] = []
    for index in range(count):
        theta = 2.0 * math.pi * index / count
        scale = 1.0 + sum(
            float(amplitude) * math.sin(int(harmonic) * theta + float(phase))
            for harmonic, amplitude, phase in zip(
                ripple["harmonics"],
                ripple["amplitudes"],
                ripple["phasesRadians"],
                strict=True,
            )
        )
        points.append(
            (
                float(center_x) + scale * float(semi_x) * math.cos(theta),
                float(center_y) + scale * float(semi_y) * math.sin(theta),
            )
        )
    return points


def point_segment_distance(
    point: tuple[float, float],
    first: tuple[float, float],
    second: tuple[float, float],
) -> float:
    dx = second[0] - first[0]
    dy = second[1] - first[1]
    length_squared = dx * dx + dy * dy
    if length_squared == 0.0:
        return math.dist(point, first)
    parameter = max(
        0.0,
        min(
            1.0,
            ((point[0] - first[0]) * dx + (point[1] - first[1]) * dy)
            / length_squared,
        ),
    )
    projection = (first[0] + parameter * dx, first[1] + parameter * dy)
    return math.dist(point, projection)


def point_in_or_on_polygon(
    point: tuple[float, float], polygon: list[tuple[float, float]]
) -> bool:
    inside = False
    for first, second in zip(polygon, polygon[1:] + polygon[:1]):
        if point_segment_distance(point, first, second) <= 1.0e-6:
            return True
        if (first[1] > point[1]) != (second[1] > point[1]):
            crossing_x = first[0] + (
                (point[1] - first[1])
                * (second[0] - first[0])
                / (second[1] - first[1])
            )
            if point[0] < crossing_x:
                inside = not inside
    return inside


def locked_set_sha256(paths: list[Path]) -> str:
    digest = hashlib.sha256()
    for path in sorted(paths, key=lambda item: item.name):
        digest.update(path.name.encode("utf-8"))
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def contract_source(
    section_name: str, declaration_name: str, declaration: dict[str, object]
) -> Path:
    """Resolve a frozen lineage byte without confusing it with its live successor."""
    if section_name == "algorithmLineage" and declaration_name == "v5dProviderClip":
        return HISTORICAL_PROVIDER_CLIP.resolve()
    return (PACKAGE / str(declaration["repositoryRelativePath"])).resolve()


def stage_historical_replay_tree(
    stage_root: Path, contract: dict[str, object]
) -> Path:
    """Stage only the files needed to replay the immutable public-realm builder."""
    staged_package = (
        stage_root
        / "unreal/SourceAssets/IstanaPublicViewExploreV5D/PublicRealm"
    )
    staged_package.mkdir(parents=True)
    shutil.copy2(CONTRACT_PATH, staged_package / CONTRACT_PATH.name)
    staged_builder = staged_package / BUILDER.name
    shutil.copy2(BUILDER, staged_builder)

    for section_name in ("sourceBindings", "algorithmLineage"):
        declarations = contract[section_name]
        assert isinstance(declarations, dict)
        for declaration_name, declaration in declarations.items():
            assert isinstance(declaration, dict)
            source = contract_source(section_name, declaration_name, declaration)
            destination = (
                staged_package / str(declaration["repositoryRelativePath"])
            ).resolve()
            try:
                destination.relative_to(stage_root.resolve())
            except ValueError as error:
                raise AssertionError(
                    f"Historical replay destination escaped its stage: {destination}"
                ) from error
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
    return staged_builder


class IstanaExploreV5DPublicRealmContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.contract = load_json(CONTRACT_PATH)
        cls.manifest = load_json(MANIFEST)
        cls.features = load_json(FEATURES)
        cls.lock = load_json(LOCK)
        cls.external_root = Path(
            str(cls.contract["externalOfficialReference"]["rootHintOnlyNotAuthority"])
        )

    def test_exact_output_roster_status_and_topology_are_locked(self) -> None:
        self.assertEqual(
            {path.name for path in GENERATED.iterdir() if path.is_file()},
            set(OUTPUT_NAMES),
        )
        self.assertEqual(self.contract["status"], EXPECTED_STATUS)
        self.assertEqual(self.manifest["status"], EXPECTED_STATUS)
        self.assertEqual(self.lock["status"], EXPECTED_STATUS)
        topology = self.manifest["topology"]
        self.assertEqual(topology, self.lock["topology"])
        self.assertEqual(
            self.contract["expectedTopology"],
            {
                key: topology[key]
                for key in (
                    "sourceMetrics",
                    "core",
                    "fallback",
                    "totalTriangleCount",
                    "totalDuplicatedCornerCount",
                )
            },
        )
        self.assertEqual(topology["core"]["triangleCount"], 444)
        self.assertEqual(topology["fallback"]["triangleCount"], 1101)
        self.assertEqual(topology["totalTriangleCount"], 1545)
        self.assertEqual(topology["totalDuplicatedCornerCount"], 4635)
        self.assertEqual(topology["laneOrCrossingPaintTriangles"], 0)
        self.assertEqual(topology["providerClipSplinePoints"], 64)
        self.assertEqual(topology["publicRealmMeshOverlapMeters"], 0.0)
        metrics = topology["sourceMetrics"]
        self.assertEqual(metrics["acceptedPublicWays"], 29)
        self.assertEqual(
            metrics["acceptedWaysByClass"],
            {"motorway": 6, "residential": 2, "secondary": 7, "tertiary": 14},
        )
        self.assertEqual(metrics["explicitSidewalkSides"], {"left": 2})
        self.assertEqual(metrics["nonPublicAccessWaysExcludedWithin300m"], 9)
        self.assertEqual(metrics["heroHardscapeRoadOverlapSquareMeters"], 0.0)
        self.assertEqual(metrics["heroBuildingRoadOverlapSquareMeters"], 0.0)

    def test_source_bytes_epochs_licences_and_dependencies_are_pinned(self) -> None:
        self.assertEqual(self.contract["target"]["currentAsOf20260831Claimed"], False)
        self.assertEqual(
            self.contract["sourceBindings"]["openStreetMapSnapshot"]["snapshotLocalDate"],
            "2026-08-25",
        )
        for section_name in ("sourceBindings", "algorithmLineage"):
            for declaration_name, declaration in self.contract[section_name].items():
                source = contract_source(section_name, declaration_name, declaration)
                self.assertTrue(source.is_file(), source)
                self.assertEqual(source.stat().st_size, declaration["bytes"])
                self.assertEqual(sha256(source).upper(), declaration["sha256"])

        provider_lineage = self.contract["algorithmLineage"]["v5dProviderClip"]
        self.assertEqual(
            provider_lineage["repositoryRelativePath"],
            CANONICAL_PROVIDER_CLIP_RELATIVE_PATH,
        )
        self.assertEqual(
            (PACKAGE / provider_lineage["repositoryRelativePath"]).resolve(),
            LIVE_CONTEXT_POLICY.resolve(),
        )
        self.assertTrue(provider_lineage["numericContractCopiedAndIndependentlyTested"])
        self.assertTrue(LIVE_CONTEXT_POLICY.is_file())
        self.assertNotEqual(
            sha256(LIVE_CONTEXT_POLICY).upper(), provider_lineage["sha256"]
        )
        historical_text = HISTORICAL_PROVIDER_CLIP.read_text(encoding="utf-8")
        live_text = LIVE_CONTEXT_POLICY.read_text(encoding="utf-8")
        for exact_line in PROVIDER_CLIP_NUMERIC_LINES:
            self.assertEqual(historical_text.count(exact_line), 1, exact_line)
            self.assertEqual(live_text.count(exact_line), 1, exact_line)
        for symbol in PROVIDER_CLIP_REQUIRED_SYMBOLS:
            self.assertIn(symbol, historical_text)
            self.assertIn(symbol, live_text)
        self.assertEqual(
            self.manifest["dependencyContract"],
            {
                "pythonMinimum": "3.11",
                "numpy": "2.4.6",
                "shapely": "2.1.2",
                "geos": "3.13.1",
                "pyproj": "3.7.2",
                "proj": "9.5.1",
            },
        )
        self.assertEqual(
            self.lock["licence"],
            "ODbL_1.0_AND_SINGAPORE_OPEN_DATA_LICENCE_1.0",
        )
        self.assertEqual(
            self.manifest["licence"]["officialPlanningDerivatives"]["licence"],
            "Singapore Open Data Licence 1.0",
        )
        self.assertEqual(
            self.manifest["licence"]["openStreetMap"]["licence"],
            "Open Data Commons Open Database License 1.0",
        )

    def test_acceptance_lock_pins_every_authored_and_generated_byte(self) -> None:
        locked_paths: list[Path] = []
        for record in self.lock["files"]:
            root = PACKAGE if record["scope"] == "PACKAGE" else GENERATED
            path = root / record["path"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(path.stat().st_size, record["bytes"])
            self.assertEqual(sha256(path), record["sha256"])
            locked_paths.append(path)
        self.assertEqual(locked_set_sha256(locked_paths), self.lock["lockedSetSha256"])
        manifest_files = {record["path"]: record for record in self.manifest["files"]}
        for path in (CORE, FALLBACK, MTL, FEATURES):
            self.assertEqual(manifest_files[path.name]["bytes"], path.stat().st_size)
            self.assertEqual(manifest_files[path.name]["sha256"], sha256(path))

    def test_obj_material_sections_and_legacy_import_contract_are_exact(self) -> None:
        self.assertEqual(tuple(self.contract["materialPlan"]["orderedSlots"]), EXPECTED_MATERIAL_ORDER)
        self.assertEqual(
            ordered_prefixed_values(MTL, "newmtl "), EXPECTED_MATERIAL_ORDER
        )
        self.assertEqual(
            ordered_prefixed_values(CORE, "usemtl "),
            ("MI_IPV5C_OfficialPlanningRoadZone",),
        )
        self.assertEqual(
            ordered_prefixed_values(FALLBACK, "usemtl "), EXPECTED_MATERIAL_ORDER
        )
        self.assertEqual(
            ordered_prefixed_values(CORE, "g "),
            ("V5D_PUBLIC_ROAD_BASE_VISUAL_ASSUMPTION",),
        )
        self.assertEqual(
            ordered_prefixed_values(FALLBACK, "g "),
            (
                "V5D_PUBLIC_ROAD_BASE_VISUAL_ASSUMPTION",
                "V5D_OFFICIAL_ROAD_GRAPHIC_VISUAL_REFERENCE",
                "V5D_EXPLICITLY_TAGGED_SIDEWALK_VISUAL_ASSUMPTION",
                "V5D_EXPLICITLY_TAGGED_LOW_KERB_TOP_VISUAL_ASSUMPTION",
                "V5D_PUBLIC_KERB_VISUAL_WALL",
            ),
        )
        for path in (CORE, FALLBACK):
            header = "\n".join(path.read_text(encoding="utf-8").splitlines()[:16])
            for marker in (
                "UE5.5 legacy OBJ preconditioned",
                "disk v=(x,-y,z), vn=(nx,-ny,nz)",
                "each face-token order is reversed",
                "disk vt=(u,1-v)",
                "positions: centimetres",
            ):
                self.assertIn(marker, header)
        frame = self.contract["frame"]
        self.assertEqual(frame["metricProjection"], "EPSG:3414 SVY21 / Singapore TM")
        self.assertEqual(frame["buildScale3D"], [1.0, 1.0, 1.0])
        self.assertTrue(frame["negativeBuildScaleForbidden"])

    def test_meshes_obey_exact_300m_and_provider_partition(self) -> None:
        polygon = provider_polygon(self.contract)
        self.assertEqual(len(polygon), 64)
        for path, expected_faces, should_be_core in (
            (CORE, 444, True),
            (FALLBACK, 1101, False),
        ):
            vertices, faces = logical_obj_triangles(path)
            self.assertEqual(len(vertices), expected_faces * 3)
            self.assertEqual(len(faces), expected_faces)
            self.assertLessEqual(
                max(math.hypot(vertex[0], vertex[1]) for vertex in vertices),
                300.000001,
            )
            for face in faces:
                centroid = (
                    sum(vertices[index][0] for index in face) / 3.0,
                    sum(vertices[index][1] for index in face) / 3.0,
                )
                inside = point_in_or_on_polygon(centroid, polygon)
                self.assertEqual(inside, should_be_core, (path.name, centroid))

    def test_sidecar_is_sanitised_and_all_deferred_truth_stays_absent(self) -> None:
        self.assertEqual(self.features["featureCount"], 29)
        rows = self.features["features"]
        self.assertEqual(len(rows), 29)
        self.assertEqual(len({row["sourceWayKeySha256"] for row in rows}), 29)
        expected_keys = {
            "collisionNavigationSensorRfAuthority",
            "explicitSidewalkSidesConsumed",
            "heroOffsetSidesAfterNorthReflection",
            "highwayClass",
            "numericLaneCountConsumed",
            "presentationRoadWidthMeters",
            "renderOnly",
            "roadWidthIsPresentationAssumption",
            "sidewalkAndKerbDimensionsArePresentationAssumptions",
            "sourceGeometryPointCount",
            "sourceWayKeySha256",
        }
        for row in rows:
            self.assertEqual(set(row), expected_keys)
            self.assertTrue(row["renderOnly"])
            self.assertFalse(row["collisionNavigationSensorRfAuthority"])
        sidewalk_rows = [row for row in rows if row["explicitSidewalkSidesConsumed"]]
        self.assertEqual(len(sidewalk_rows), 2)
        for row in sidewalk_rows:
            self.assertEqual(row["explicitSidewalkSidesConsumed"], ["left"])
            self.assertEqual(row["heroOffsetSidesAfterNorthReflection"], ["right"])
        self.assertEqual(
            self.features["sanitization"],
            {
                "accessValuesRetained": False,
                "barrierFenceGateOrSecurityGeometryProduced": False,
                "operationalOrRestrictedCirculationGeometryProduced": False,
                "rawOsmTagsRetained": False,
                "sourceRoadNamesRetained": False,
                "sourceWayIdsRetained": False,
            },
        )
        deferred = self.contract["deferredEvidence"]
        for key, value in deferred.items():
            if key != "reason":
                self.assertFalse(value, key)
        for key, value in self.contract["renderPolicy"].items():
            if key == "renderOnly":
                self.assertTrue(value)
            else:
                self.assertFalse(value, key)

    @unittest.skipUnless(
        importlib.util.find_spec("shapely") is not None
        and importlib.util.find_spec("pyproj") is not None,
        "Pinned geometry dependencies are not installed",
    )
    def test_mounted_official_sources_and_double_generation_are_byte_identical(self) -> None:
        if not self.external_root.is_dir():
            self.skipTest("Hash-pinned external official reference pack is not mounted")
        official = self.contract["externalOfficialReference"]
        acquisition = self.external_root / official["acquisitionManifest"]["relativePath"]
        derivatives_manifest = (
            self.external_root / official["derivativesManifest"]["relativePath"]
        )
        declarations = [
            (acquisition, official["acquisitionManifest"]),
            (derivatives_manifest, official["derivativesManifest"]),
        ]
        derivative_root = derivatives_manifest.parent
        declarations.extend(
            (derivative_root / record["relativePath"], record)
            for record in official["usedDerivatives"]
        )
        for path, declaration in declarations:
            self.assertTrue(path.is_file(), path)
            self.assertEqual(path.stat().st_size, declaration["bytes"])
            self.assertEqual(sha256(path).upper(), declaration["sha256"])

        environment = os.environ.copy()
        environment["PYTHONDONTWRITEBYTECODE"] = "1"
        with tempfile.TemporaryDirectory(prefix="triad_v5d_public_realm_test_") as root:
            staged_builder = stage_historical_replay_tree(
                Path(root) / "historical_source_tree", self.contract
            )
            first = Path(root) / "first"
            second = Path(root) / "second"
            for output in (first, second):
                completed = subprocess.run(
                    (
                        sys.executable,
                        str(staged_builder),
                        "--external-root",
                        str(self.external_root),
                        "--output",
                        str(output),
                    ),
                    cwd=REPO,
                    env=environment,
                    text=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    check=False,
                )
                self.assertEqual(completed.returncode, 0, completed.stderr)
            for name in OUTPUT_NAMES:
                self.assertEqual((first / name).read_bytes(), (second / name).read_bytes())
                self.assertEqual((first / name).read_bytes(), (GENERATED / name).read_bytes())


if __name__ == "__main__":
    unittest.main()
