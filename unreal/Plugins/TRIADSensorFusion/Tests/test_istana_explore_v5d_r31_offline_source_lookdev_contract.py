from __future__ import annotations

import hashlib
import json
import struct
import subprocess
import sys
import unittest
from pathlib import Path


REPO = Path(__file__).absolute().parents[4]
R31 = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Surroundings"
    / "R31BroadShellLookdev"
)
RENDERER = R31 / "render_r31_offline_broad_shell_lookdev.py"
AUDIT = R31 / "OfflineSourceLookdev"
PREVIEW = AUDIT / "r31_broad_shell_cpu_source_lookdev.png"
REPORT = AUDIT / "r31_broad_shell_cpu_source_lookdev.json"
README = AUDIT / "README.md"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def png_dimensions(path: Path) -> tuple[int, int]:
    header = path.read_bytes()[:24]
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        raise AssertionError(f"not a PNG: {path}")
    return struct.unpack(">II", header[16:24])


class R31OfflineSourceLookdevContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        for path in (RENDERER, AUDIT, PREVIEW, REPORT, README):
            if not path.exists():
                raise AssertionError(f"missing R31 offline lookdev artifact: {path}")
        cls.report_text = REPORT.read_text(encoding="utf-8")
        cls.report = json.loads(cls.report_text)

    def test_canonical_source_only_receipt_and_single_preview(self) -> None:
        self.assertEqual(
            {path.name for path in AUDIT.iterdir() if path.is_file()},
            {"README.md", PREVIEW.name, REPORT.name},
        )
        self.assertEqual(
            self.report_text,
            json.dumps(self.report, indent=2, sort_keys=True, ensure_ascii=True) + "\n",
        )
        self.assertEqual(
            self.report["schema"],
            "triad.r31.broad_shell_lookdev.offline_cpu_source_preview.v1",
        )
        self.assertEqual(self.report["validationStatus"], "PASS_SOURCE_LOOKDEV_ONLY")
        self.assertIs(self.report["visualAcceptance"], False)

    def test_exact_broad_shell_and_material_inputs_are_hash_bound(self) -> None:
        receipts = self.report["sourceAssetReceipts"]
        self.assertEqual(len(receipts), 16)
        for key in (
            "renderer",
            "pinnedR30RendererPrimitives",
            "r31Factory",
            "r31Contract",
            "v2BroadShellObj",
            "v2BroadShellMtl",
            "textureManifest",
        ):
            self.assertIn(key, receipts)
        self.assertEqual(
            len([key for key in receipts if key.startswith("texture") and key != "textureManifest"]),
            9,
        )
        for key, receipt in receipts.items():
            with self.subTest(key=key):
                path = REPO / receipt["file"]
                self.assertTrue(path.is_file())
                self.assertEqual(receipt["bytes"], path.stat().st_size)
                self.assertEqual(receipt["sha256"], sha256(path))

    def test_complete_mesh_is_censused_and_render_is_a_bounded_crop(self) -> None:
        counts = self.report["derivedCounts"]
        self.assertEqual(counts["sourceTriangleCount"], 43_448)
        self.assertEqual(counts["sourceMaterialSectionCount"], 17)
        self.assertEqual(sum(counts["sourceMaterialTriangleCounts"].values()), 43_448)
        self.assertGreater(counts["retainedTriangleCount"], 1_000)
        self.assertLess(counts["retainedTriangleCount"], 2_000)
        self.assertLessEqual(
            set(counts["retainedMaterialTriangleCounts"]),
            set(counts["sourceMaterialTriangleCounts"]),
        )

    def test_preview_and_material_delta_are_explicit(self) -> None:
        receipt = self.report["previewReceipt"]
        self.assertEqual((receipt["widthPixels"], receipt["heightPixels"]), (2200, 1320))
        self.assertEqual(png_dimensions(PREVIEW), (2200, 1320))
        self.assertEqual(receipt["bytes"], PREVIEW.stat().st_size)
        self.assertEqual(receipt["sha256"], sha256(PREVIEW))
        self.assertRegex(receipt["decodedRgbaSha256"], r"^[A-F0-9]{64}$")
        settings = self.report["renderSettings"]
        self.assertEqual(
            settings["uvSemantics"],
            "WALL_SOURCE_U_PER_SEGMENT_AND_V_ABSOLUTE_Z; RUNTIME_TEXTURE_AND_CADENCE_U_VERTEX_TANGENT_WORLD_PROJECTED; PLAN_UV_HERO_LOCAL_XY",
        )
        self.assertIs(settings["shaderEquivalence"], False)
        self.assertEqual(settings["previewTextureMipPixels"], [512, 512])
        metrics = self.report["comparisonMetrics"]
        self.assertEqual(metrics["alignedPixelCount"], 1030 * 900)
        self.assertGreater(metrics["meanAbsoluteChannelDifference"], 1.0)
        self.assertGreater(metrics["pixelFractionMaximumChannelDifferenceAbove8"], 0.20)
        self.assertGreater(metrics["changedPixelCount"], 250_000)

    def test_fixed_crop_is_a_high_occupancy_generic_building_repetition_probe(self) -> None:
        probe = self.report["highOccupancyGenericBuildingProbe"]
        self.assertEqual(
            probe["purpose"],
            "HIGH_OCCUPANCY_GENERIC_BUILDING_REPETITION_DIAGNOSTIC",
        )
        self.assertTrue(probe["cropIsFixed"])
        self.assertEqual(
            probe["genericSlots"],
            ["MAT_GENERIC_BUILDING_HINT", "MAT_ROOF_GENERIC_BUILDING_HINT"],
        )
        self.assertGreaterEqual(probe["genericBuildingTriangleFraction"], 0.70)
        self.assertGreaterEqual(probe["shellFiniteDepthPixelFraction"], 0.30)
        self.assertGreater(probe["sampledFacadePlaneCount"], 4)
        self.assertEqual(
            probe["facadeArchetypeDominanceIndicesPresent"], [0, 1, 2, 3]
        )
        self.assertEqual(
            set(probe["facadeArchetypeDominanceTriangleCounts"]),
            {"0", "1", "2", "3"},
        )
        self.assertTrue(probe["sourceDiagnosticPassed"])
        self.assertFalse(probe["nativeVisualAcceptanceGranted"])

    def test_full_source_uv_and_coordinate_matched_seam_probe_is_green(self) -> None:
        probe = self.report["facadePlaneSeamProbe"]
        self.assertTrue(probe["sourceDiagnosticPassed"])
        self.assertFalse(probe["nativeVisualAcceptanceGranted"])
        self.assertEqual(probe["sourceTriangleCountProbed"], 43_448)
        uv = probe["sourceUvSemantics"]
        self.assertEqual(uv["wallTriangleCount"], 24_360)
        self.assertEqual(uv["roofOrHiddenBottomTriangleCount"], 19_088)
        self.assertFalse(uv["buildingLocalAboveGradeCoordinateAvailable"])
        self.assertTrue(uv["sourceDiagnosticPassed"])
        legacy = probe["legacyPerSegmentWallU"]["geometricCoordinateMatchedDomain"]
        self.assertGreater(legacy["uniqueNearCoplanarRawUvDiscontinuityEdgeCount"], 0)
        fixed = probe["seamSafeFacadePlane"]
        self.assertEqual(fixed["vertexTangentDominantComponentQuantizationLevels"], 4096)
        self.assertEqual(
            fixed["vertexTangentDominantComponentQuantizationPhase"],
            89.0 / 1048576.0,
        )
        self.assertGreater(
            fixed["coordinateMatchedSameFacingExactCoplanarWallEdgeCount"],
            10_000,
        )
        self.assertLessEqual(
            fixed["coordinateMatchedSameFacingExactCoplanarMetricUvMaximumJumpMetres"],
            probe["thresholds"]["maximumCoplanarMetricTextureUvJumpMetres"],
        )
        self.assertLessEqual(
            fixed["wallMetricScaleMaximumRelativeError"],
            probe["thresholds"]["maximumWallMetricScaleRelativeError"],
        )
        for field in (
            "withinTriangleFacadeSignalToleranceExceededCount",
            "coplanarSharedEdgeFacadeSignalToleranceExceededCount",
            "coplanarSharedEdgeMetricTextureUvToleranceExceededCount",
            "coplanarWallCanonicalFacadeUToleranceExceededCount",
            "wallCanonicalTangentAlignmentToleranceExceededCount",
            "wallMetricScaleRelativeErrorToleranceExceededCount",
            "coplanarWallTangentOrientationMismatchCount",
            "coordinateMatchedSameFacingExactCoplanarMetricUvToleranceExceededEdgeCount",
            "coordinateMatchedSameFacingExactCoplanarProjectionSelectorMismatchEdgeCount",
        ):
            self.assertEqual(fixed[field], 0, field)

    def test_architectural_grammar_is_distinct_continuous_and_non_authorizing(self) -> None:
        probe = self.report["architecturalGrammarProbe"]
        self.assertEqual(
            probe["purpose"],
            "SOURCE_ONLY_CONTINUOUS_ARCHITECTURAL_GRAMMAR_DIAGNOSTIC",
        )
        self.assertEqual(len(probe["familySignatures"]), 4)
        self.assertTrue(all(len(row) == 6 for row in probe["familySignatures"]))
        self.assertTrue(probe["allFourFamilySignaturesDistinct"])
        self.assertTrue(probe["finiteAndBounded"])
        self.assertTrue(probe["continuousTwoStoreyWave"])
        self.assertFalse(probe["hardFloorParitySelectorUsed"])
        self.assertFalse(probe["exactPerBuildingStyleClaimed"])
        self.assertFalse(probe["balconyGeometryClaimed"])
        self.assertFalse(probe["nativeVisualAcceptanceGranted"])
        self.assertGreaterEqual(
            probe["minimumPairwiseFamilySignatureDistance"],
            probe["thresholds"]["minimumPairwiseFamilySignatureDistance"],
        )
        self.assertLessEqual(
            probe["phaseWrapMaximumCueDelta"],
            probe["thresholds"]["maximumPhaseWrapCueDelta"],
        )
        self.assertTrue(probe["sourceDiagnosticPassed"])

    def test_fail_closed_claim_boundary_and_readme_are_complete(self) -> None:
        self.assertTrue(all(value is False for value in self.report["claimBoundary"].values()))
        parity = self.report["renderPathParityBoundary"]
        self.assertTrue(parity["withinModeledSourcePathContinuityProven"])
        self.assertFalse(parity["naniteRasterAppearanceParityProven"])
        self.assertTrue(parity["nativeSameCameraHighOccupancyPairRequired"])
        self.assertFalse(parity["sourceOrOfflineProbeMayAuthorizeParity"])
        limitations = " ".join(self.report["knownLimitations"])
        for phrase in (
            "not Unreal shader equivalence",
            "excludes the Istana hero",
            "frequently synthetic heights",
            "no OSM group identifier",
            "spandrel, pier and slab-edge cues are procedural shading only",
            "packed Nanite and raster-fallback tangent paths",
            "Lumen and Nanite",
        ):
            self.assertIn(phrase, limitations)
        readme = README.read_text(encoding="utf-8")
        for phrase in (
            "not an Unreal screenshot",
            "not a native R31 integration",
            "not Cesium/provider composition",
            "not an actual-site",
            "Visual acceptance remains **false**",
            "frequently synthetic heights",
            "high-occupancy generic-building probe",
            "four archetypes continuously",
            "0.002 m source-float tolerance",
            "native same-camera Nanite/raster pair remains mandatory",
            "Singapore-context/HDB-inspired macro grammar",
            "not balcony geometry",
        ):
            self.assertIn(phrase, readme)

    def test_check_and_math_self_test_are_read_only(self) -> None:
        before = {
            path.name: (path.stat().st_mtime_ns, sha256(path))
            for path in AUDIT.iterdir()
            if path.is_file()
        }
        for flag, token in (
            ("--check", '"nativeOrProviderClaimed": false'),
            ("--self-test", '"facadePlaneSignalsDeterministic": true'),
        ):
            result = subprocess.run(
                [sys.executable, "-B", str(RENDERER), flag],
                cwd=REPO,
                text=True,
                capture_output=True,
                check=False,
                timeout=45,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn(token, result.stdout)
        after = {
            path.name: (path.stat().st_mtime_ns, sha256(path))
            for path in AUDIT.iterdir()
            if path.is_file()
        }
        self.assertEqual(after, before)


if __name__ == "__main__":
    unittest.main()
