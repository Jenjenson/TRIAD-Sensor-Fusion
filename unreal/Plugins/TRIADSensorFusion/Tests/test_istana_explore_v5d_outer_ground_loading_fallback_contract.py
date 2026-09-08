from __future__ import annotations

import importlib.util
import json
import math
from pathlib import Path
import unittest


PACKAGE_ROOT = (
    Path(__file__).absolute().parents[1]
    / "Tools"
    / "IstanaExploreV5D"
    / "OuterGroundFallback"
)
CONTRACT_PATH = PACKAGE_ROOT / "outer_ground_loading_fallback_v1.contract.json"
GENERATED = PACKAGE_ROOT / "Generated"
BUILDER_PATH = PACKAGE_ROOT / "build_outer_ground_loading_fallback_v1.py"
MANIFEST_NAME = "IstanaPublicViewV5DOuterGroundLoadingFallback.manifest.json"
LOCK_NAME = "IstanaPublicViewV5DOuterGroundLoadingFallback.acceptance.lock.json"

SPEC = importlib.util.spec_from_file_location("outer_ground_builder", BUILDER_PATH)
assert SPEC is not None and SPEC.loader is not None
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)


class OuterGroundLoadingFallbackV1Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
        cls.manifest = json.loads(
            (GENERATED / MANIFEST_NAME).read_text(encoding="utf-8")
        )

    def test_01_scope_is_minimal_1000_to_1250_m_annulus(self) -> None:
        target = self.contract["target"]
        geometry = self.contract["geometryPolicy"]
        self.assertEqual(target["innerRadiusMeters"], 1000.0)
        self.assertEqual(target["qaCameraEnvelopeRadiusMeters"], 1200.0)
        self.assertEqual(target["outerRadiusMeters"], 1250.0)
        self.assertEqual(target["minimumOuterGuardMeters"], 50.0)
        self.assertEqual(geometry["radialBandCount"], 5)
        self.assertEqual(geometry["angularSectorCount"], 128)
        self.assertEqual(geometry["innerAreaOverlapSquareMeters"], 0.0)
        self.assertFalse(geometry["sourceTerrainModified"])
        self.assertFalse(geometry["sourceTerrainSkirtModified"])

    def test_02_capture_geometry_explains_void_without_provider_claim(self) -> None:
        audit = self.contract["captureGeometryAudit"]
        self.assertEqual(audit["poseCount"], 6)
        self.assertEqual(audit["posesOutsideSourceTerrainCount"], 4)
        self.assertAlmostEqual(
            audit["maximumAuditedCameraRadiusMeters"], 1135.190795, places=6
        )
        self.assertAlmostEqual(
            audit["maximumAuditedCameraOverrunMeters"], 135.190795, places=6
        )
        self.assertAlmostEqual(
            audit["outerFallbackMarginBeyondFarthestAuditedCameraMeters"],
            114.809205,
            places=6,
        )
        self.assertFalse(
            self.contract["providerLifecycleIntegrationSpec"][
                "globalProgressIsLandmarkSpecificReadinessProof"
            ]
        )

    def test_03_no_simulation_or_terrain_authority(self) -> None:
        render = self.contract["renderPolicy"]
        self.assertTrue(render["renderOnly"])
        for key, value in render.items():
            if key != "renderOnly":
                self.assertFalse(value, key)
        self.assertFalse(
            self.contract["providerLifecycleIntegrationSpec"][
                "liveIntegrationIncluded"
            ]
        )

    def test_04_acceptance_lock_pins_every_package_byte(self) -> None:
        lock = json.loads((GENERATED / LOCK_NAME).read_text(encoding="utf-8"))
        self.assertEqual(lock["schema"], builder.LOCK_SCHEMA)
        for record in lock["files"]:
            root = PACKAGE_ROOT if record["scope"] == "PACKAGE" else GENERATED
            path = root / record["path"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(path.stat().st_size, record["bytes"], path)
            self.assertEqual(builder.sha256_file(path), record["sha256"], path)

    def test_05_obj_topology_radius_normals_and_material_are_exact(self) -> None:
        path = GENERATED / "SM_IPV5D_OuterGroundLoadingFallback_Render.obj"
        positions: list[tuple[float, float, float]] = []
        normals: list[tuple[float, float, float]] = []
        faces: list[list[str]] = []
        groups: set[str] = set()
        materials: set[str] = set()
        for line in path.read_text(encoding="utf-8").splitlines():
            if line.startswith("v "):
                _, x, encoded_y, z = line.split()
                positions.append(
                    (
                        float(x) / 100.0,
                        -float(encoded_y) / 100.0,
                        float(z) / 100.0,
                    )
                )
            elif line.startswith("vn "):
                _, x, encoded_y, z = line.split()
                normals.append((float(x), -float(encoded_y), float(z)))
            elif line.startswith("f "):
                faces.append(line.split()[1:])
            elif line.startswith("g "):
                groups.add(line[2:])
            elif line.startswith("usemtl "):
                materials.add(line[7:])
        expected = self.contract["expectedTopology"]
        self.assertEqual(len(positions), expected["duplicatedCornerCount"])
        self.assertEqual(len(normals), expected["duplicatedCornerCount"])
        self.assertEqual(len(faces), expected["triangleCount"])
        self.assertEqual(groups, {builder.GROUP})
        self.assertEqual(materials, {builder.MATERIAL})
        radii = [math.hypot(point[0], point[1]) for point in positions]
        self.assertAlmostEqual(min(radii), 1000.0, places=6)
        self.assertAlmostEqual(max(radii), 1250.0, places=6)
        self.assertTrue(all(normal[2] > 0.99 for normal in normals))
        self.assertEqual(faces[0], ["1/1/1", "3/3/3", "2/2/2"])

    def test_06_manifest_proves_exact_seam_and_bounded_relief(self) -> None:
        metrics = self.manifest["geometryMetrics"]
        self.assertTrue(metrics["innerSeamExactSourceCopy"])
        self.assertEqual(
            metrics["innerSeamPointSetSha256"],
            metrics["sourceInnerSeamPointSetSha256"],
        )
        self.assertLess(metrics["maximumAbsoluteInheritedRadialSlope"], 0.01)
        self.assertLess(abs(metrics["minimumOuterHeightDeltaMeters"]), 1.0)
        self.assertLess(abs(metrics["maximumOuterHeightDeltaMeters"]), 1.0)
        self.assertEqual(
            metrics["outerGuardBeyondQaCameraEnvelopeMeters"], 50.0
        )

    def test_07_committed_outputs_are_byte_deterministic(self) -> None:
        manifest = builder.check_committed_outputs()
        self.assertEqual(manifest["topology"], self.contract["expectedTopology"])


if __name__ == "__main__":
    unittest.main()
