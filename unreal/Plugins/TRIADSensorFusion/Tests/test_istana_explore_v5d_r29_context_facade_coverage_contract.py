import copy
import hashlib
import importlib.util
import json
import platform
import shutil
import subprocess
import sys
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
SOURCE_ROOT = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Surroundings"
    / "R29ContextFacadeCoverage"
)
GENERATED = SOURCE_ROOT / "Generated"
CONTRACT = SOURCE_ROOT / "r29_context_facade_coverage.contract.json"
GENERATOR = SOURCE_ROOT / "build_r29_context_facade_coverage.py"
README = SOURCE_ROOT / "README.md"
MANIFEST = GENERATED / "IstanaPublicViewV5DR29ContextFacadeCoverage.manifest.json"
MESH = GENERATED / "SM_IPV5D_R29_ContextFacadeCoverage_Render.obj"
MTL = GENERATED / "IstanaPublicViewV5DR29ContextFacadeCoverage.mtl"
R28_MANIFEST = (
    SOURCE_ROOT.parent
    / "R28EnvironmentalDressing"
    / "Generated"
    / "IstanaPublicViewV5DR28EnvironmentalDressing.manifest.json"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def load_generator():
    module_name = "triad_r29_context_facade_coverage_contract_test_module"
    spec = importlib.util.spec_from_file_location(module_name, GENERATOR)
    if spec is None or spec.loader is None:
        raise AssertionError("could not load R29 facade generator")
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


def pinned_python_311_command():
    if platform.python_implementation() == "CPython" and sys.version_info[:2] == (3, 11):
        return [sys.executable]
    if sys.platform == "win32" and shutil.which("py"):
        probe = subprocess.run(
            ["py", "-3.11", "-B", "-c", "import sys; raise SystemExit(sys.version_info[:2] != (3, 11))"],
            text=True,
            capture_output=True,
            check=False,
            timeout=10,
        )
        if probe.returncode == 0:
            return ["py", "-3.11"]
    candidate = shutil.which("python3.11")
    if candidate:
        return [candidate]
    return None


class IstanaExploreV5DR29ContextFacadeCoverageContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for path in (CONTRACT, GENERATOR, README, MANIFEST, MESH, MTL, R28_MANIFEST):
            if not path.is_file():
                raise AssertionError(f"missing R29 facade coverage input: {path}")
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        cls.r28_manifest = json.loads(R28_MANIFEST.read_text(encoding="utf-8"))

    def test_contract_is_explicitly_render_only_and_not_live_integrated(self):
        self.assertEqual(
            self.contract["schema"],
            "triad.istana_explore_v5d.r29_context_facade_coverage.contract.v1",
        )
        self.assertEqual(
            self.manifest["schema"],
            "triad.istana_explore_v5d.r29_context_facade_coverage.manifest.v1",
        )
        self.assertEqual(
            self.manifest["status"], "SOURCE_GENERATED_NOT_LIVE_UNREAL_INTEGRATED"
        )
        self.assertIn("NOT_SURVEY", self.manifest["claimStatus"])
        self.assertIn("NOT_AS_BUILT", self.manifest["claimStatus"])
        self.assertTrue(self.manifest["negativeAuthority"])
        self.assertTrue(
            all(value is False for value in self.manifest["negativeAuthority"].values())
        )
        boundary = self.manifest["supersessionBoundary"]
        self.assertFalse(boundary["concurrentRenderingWithR28ArchitectureAllowed"])
        self.assertFalse(boundary["liveUnrealIntegrationProven"])
        self.assertFalse(boundary["existingCesiumActorOrProviderPolicyMutation"])
        self.assertFalse(boundary["existingCurrentSurroundingsMeshOrRfShellMutation"])

    def test_exact_six_source_bindings_are_byte_hash_pinned(self):
        expected = {
            "r28Contract": (9317, "0CA25D72C3FAAEA2AF445E74ED374AD9AAA149DDC4BDF0691DC235435D9C8DF8"),
            "r28Generator": (83872, "D60C6F26AB0CAFBB201C9F4BE9DC7BACB799D8538C82584DA15D0855CDBC6DF9"),
            "r28Manifest": (20596, "3366F0B1D6C7A5801538B897488DCF506D1A1014AFB4E8676B64223420C8C641"),
            "surroundingsFeatures": (3045768, "8CEDD488AC6E91C3335D779EA55FB590BCC679EFEF08CBEEB5B838C5A3B2E9F5"),
            "surroundingsGeometry": (4367992, "D34D752E4B6D7E4BC3FE4B44DF8EAEC5A824A04D83905DBA85169C31F5F2909A"),
            "surroundingsManifest": (4527, "8E46B7E9D95DFA2CE03EF3143382BA4CD6CFF091A363DE9D6898EF7624F72C78"),
        }
        self.assertEqual(set(self.contract["sourceBindings"]), set(expected))
        self.assertEqual(set(self.manifest["sourceBindings"]), set(expected))
        for key, (size, digest) in expected.items():
            self.assertEqual(self.contract["sourceBindings"][key]["bytes"], size)
            self.assertEqual(self.contract["sourceBindings"][key]["sha256"], digest)
            self.assertEqual(self.manifest["sourceBindings"][key]["bytes"], size)
            self.assertEqual(self.manifest["sourceBindings"][key]["sha256"], digest)

    def test_coverage_materially_exceeds_r28_without_geographic_holes(self):
        metrics = self.manifest["facadeCoverage"]
        self.assertEqual(metrics["sourcePartCount"], 1305)
        self.assertEqual(metrics["annulusPartCount"], 1302)
        self.assertEqual(metrics["facadeEligiblePartCount"], 1174)
        self.assertEqual(metrics["selectedPartCount"], 1174)
        self.assertEqual(metrics["r28SelectedPartBaseline"], 128)
        self.assertEqual(
            self.r28_manifest["architecturalDressing"]["selectedPartCount"], 128
        )
        self.assertEqual(metrics["selectedPartCoverageMultiplierOverR28"], 9.171875)
        self.assertGreaterEqual(metrics["selectedPartFractionOfAllSourceParts"], 0.88)
        self.assertGreaterEqual(metrics["selectedPartFractionOfAnnulusParts"], 0.90)
        audit = metrics["omnidirectionalCoverageAudit"]
        self.assertEqual(len(audit["cells"]), 24)
        for cell in audit["cells"]:
            if cell["annulusPartCount"]:
                self.assertGreaterEqual(cell["selectedPartFraction"], 0.65)

    def test_real_geometry_cues_and_semantic_proxies_have_nonzero_census(self):
        metrics = self.manifest["facadeCoverage"]
        self.assertEqual(metrics["selectedFacadeEdgeCount"], 4571)
        self.assertEqual(metrics["apertureGroupCount"], 20011)
        self.assertEqual(metrics["bottomFrameRailCount"], 20011)
        self.assertEqual(metrics["mullionCount"], 3270)
        self.assertEqual(metrics["continuousSillLedgeCount"], 7881)
        self.assertEqual(metrics["parapetCapEdgeCount"], 4479)
        self.assertEqual(metrics["awningProxyCount"], 46)
        self.assertEqual(metrics["residentialBalconyProxyCount"], 28)
        relief = metrics["physicalRelief"]
        self.assertEqual(relief["glassOutwardOffsetMeters"], 0.045)
        self.assertEqual(relief["frameOutwardOffsetMeters"], 0.12)
        self.assertEqual(relief["frameToGlassReliefMeters"], 0.075)
        self.assertEqual(relief["sillOutwardProjectionMeters"], 0.22)
        self.assertEqual(
            metrics["apertureFrameClosure"],
            {
                "bottomFrameRailCount": 20011,
                "bottomRailIntersectsContinuousSillTop": True,
                "everyGroupedApertureFourSided": True,
                "requiredBottomFrameRailCount": 20011,
            },
        )
        self.assertEqual(
            self.contract["facadeCoverage"]["geometryPolicy"][
                "bottomFrameRailCoverage"
            ],
            "EVERY_EMITTED_GROUPED_APERTURE",
        )
        self.assertFalse(metrics["trueFacadeOpeningsCutIntoSourceShell"])
        self.assertFalse(metrics["originalBuildingFootprintsHeightsAndRfShellModified"])

    def test_aperture_frame_and_glass_are_physically_non_coplanar(self):
        module = load_generator()
        primitives = module.load_r28_primitives()
        edge = primitives.WallEdge(
            primitives.Point(0.0, 0.0, 0.0),
            primitives.Point(10.0, 0.0, 0.0),
            primitives.Point(0.0, 0.0, 10.0),
            primitives.Point(10.0, 0.0, 10.0),
            primitives.Point(0.0, -1.0, 0.0),
        )
        mesh = module.MeshBuilder(primitives, module.MATERIALS)
        policy = self.contract["facadeCoverage"]["geometryPolicy"]
        module.add_recessed_aperture(
            mesh,
            edge,
            5.0,
            4.0,
            2.0,
            1.6,
            "MI_IPV5D_R29_GlassCool",
            "MI_IPV5D_R29_FrameDark",
            True,
            policy,
        )
        glass_y = {
            round(corner.point.y, 6)
            for triangle in mesh.triangles["MI_IPV5D_R29_GlassCool"]
            for corner in triangle.corners
        }
        frame_y = {
            round(corner.point.y, 6)
            for triangle in mesh.triangles["MI_IPV5D_R29_FrameDark"]
            for corner in triangle.corners
        }
        self.assertEqual(glass_y, {-0.045})
        self.assertEqual(frame_y, {-0.124, -0.12})
        self.assertEqual(round(abs(-0.12) - abs(-0.045), 3), 0.075)
        frame_triangles = mesh.triangles["MI_IPV5D_R29_FrameDark"]
        self.assertEqual(len(frame_triangles), 10)
        bottom_rail_triangles = [
            triangle
            for triangle in frame_triangles
            if round(min(corner.point.z for corner in triangle.corners), 6) == 3.2
            and round(max(corner.point.z for corner in triangle.corners), 6) == 3.34
        ]
        self.assertEqual(len(bottom_rail_triangles), 2)
        self.assertEqual(
            {
                round(corner.point.y, 6)
                for triangle in bottom_rail_triangles
                for corner in triangle.corners
            },
            {-0.12},
        )

    def test_performance_ceilings_and_all_eleven_materials_are_enforced(self):
        metrics = self.manifest["facadeCoverage"]
        ceilings = metrics["hardCeilings"]
        self.assertEqual(metrics["triangleCount"], 256850)
        self.assertEqual(metrics["sourceCornerCount"], 770550)
        self.assertLessEqual(metrics["triangleCount"], ceilings["maximumTriangleCount"])
        self.assertLessEqual(
            metrics["sourceCornerCount"], ceilings["maximumSourceCornerCount"]
        )
        self.assertEqual(ceilings["triangleHeadroom"], 3150)
        self.assertEqual(ceilings["sourceCornerHeadroom"], 9450)
        counts = metrics["materialTriangleCounts"]
        self.assertEqual(len(counts), 11)
        self.assertTrue(all(value > 0 for value in counts.values()))
        self.assertEqual(sum(counts.values()), metrics["triangleCount"])
        variation = metrics["materialVariation"]
        self.assertEqual(variation["glassVariantCount"], 3)
        self.assertEqual(variation["frameVariantCount"], 3)
        self.assertEqual(variation["sillVariantCount"], 2)
        self.assertEqual(variation["balconyRailVariantCount"], 1)

    def test_generated_mesh_receipts_and_split_corner_census_are_exact(self):
        expected = {
            MESH.name: (
                102051194,
                "8CE9659A991FC18369EC980FBCA8F9EFAF00B81784D9B2F423BEC935D13181C7",
            ),
            MTL.name: (
                1580,
                "4989AFC4478B48C55E038F1E85233C8FA1405360EA19C7E623FDB98FD3B59ADD",
            ),
        }
        receipts = {item["file"]: item for item in self.manifest["outputs"]}
        self.assertEqual(set(receipts), set(expected))
        for path in (MESH, MTL):
            size, digest = expected[path.name]
            self.assertEqual(path.stat().st_size, size)
            self.assertEqual(sha256(path), digest)
            self.assertEqual(receipts[path.name]["bytes"], size)
            self.assertEqual(receipts[path.name]["sha256"], digest)
        counts = {b"v ": 0, b"vt ": 0, b"vn ": 0, b"f ": 0}
        materials = set()
        with MESH.open("rb") as handle:
            for line in handle:
                for prefix in counts:
                    if line.startswith(prefix):
                        counts[prefix] += 1
                        break
                if line.startswith(b"usemtl "):
                    materials.add(line.split(maxsplit=1)[1].strip().decode("ascii"))
        triangles = self.manifest["facadeCoverage"]["triangleCount"]
        corners = self.manifest["facadeCoverage"]["sourceCornerCount"]
        self.assertEqual(counts[b"f "], triangles)
        self.assertEqual(counts[b"v "], corners)
        self.assertEqual(counts[b"vt "], corners)
        self.assertEqual(counts[b"vn "], corners)
        self.assertEqual(materials, set(self.manifest["facadeCoverage"]["materialTriangleCounts"]))

    def test_generated_outputs_do_not_emit_source_names_ids_or_raw_tags(self):
        forbidden = (
            b"OSM:",
            b"elementId",
            b"elementType",
            b"changeset",
            b"opening_hours",
            b"addr:",
            b"wikidata",
            b"wikipedia",
        )
        for path in (MESH, MTL, MANIFEST):
            payload = path.read_bytes()
            for token in forbidden:
                self.assertNotIn(token, payload, f"{token!r} leaked into {path.name}")
        self.assertFalse(
            self.manifest["facadeCoverage"][
                "sourceNamesIdsRawTagsOrSecurityMetadataEmitted"
            ]
        )

    def test_input_and_runtime_drift_fail_closed(self):
        module = load_generator()
        module.require_supported_python_runtime((3, 11, 0), "CPython")
        with self.assertRaisesRegex(RuntimeError, "requires CPython 3.11"):
            module.require_supported_python_runtime((3, 12, 0), "CPython")
        with self.assertRaisesRegex(RuntimeError, "requires CPython 3.11"):
            module.require_supported_python_runtime((3, 11, 0), "PyPy")
        drifted = copy.deepcopy(self.contract)
        drifted["sourceBindings"]["surroundingsGeometry"]["sha256"] = "0" * 64
        with self.assertRaisesRegex(RuntimeError, "Frozen R29 input drifted"):
            module.validate_inputs(drifted)

    def test_committed_outputs_are_byte_deterministic(self):
        python_command = pinned_python_311_command()
        if python_command is None:
            self.skipTest("CPython 3.11 is unavailable; static R29 receipt checks remain active")
        result = subprocess.run(
            [*python_command, "-B", str(GENERATOR), "--check"],
            cwd=REPO,
            text=True,
            capture_output=True,
            check=False,
            timeout=60,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("deterministic check: PASS", result.stdout)


if __name__ == "__main__":
    unittest.main()
