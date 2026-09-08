from __future__ import annotations

import hashlib
import json
import struct
import subprocess
import sys
import unittest
from pathlib import Path


REPO = Path(__file__).absolute().parents[4]
R29 = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Surroundings"
    / "R29ContextFacadeCoverage"
)
GENERATED = R29 / "Generated"
AUDITOR = R29 / "render_r29_offline_visual_audit.py"
AUDIT = R29 / "OfflineVisualAudit"
REPORT = AUDIT / "r29_offline_visual_audit.json"
README = AUDIT / "README.md"
MANIFEST = GENERATED / "IstanaPublicViewV5DR29ContextFacadeCoverage.manifest.json"
VIEW_FILES = {
    "r29_full_2km_facade_coverage.png",
    "r29_priority_sector_oblique.png",
    "r29_close_facade_cue_gallery.png",
}


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


class IstanaExploreV5DR29OfflineVisualAuditContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for path in (AUDITOR, AUDIT, REPORT, README, MANIFEST):
            if not path.exists():
                raise AssertionError(f"missing R29 offline-audit artifact: {path}")
        cls.report_text = REPORT.read_text(encoding="utf-8")
        cls.report = json.loads(cls.report_text)
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def test_exact_canonical_file_roster_and_report(self):
        self.assertEqual(
            {path.name for path in AUDIT.iterdir() if path.is_file()},
            {"r29_offline_visual_audit.json", "README.md", *VIEW_FILES},
        )
        self.assertEqual(
            self.report_text,
            json.dumps(self.report, indent=2, sort_keys=True, ensure_ascii=True) + "\n",
        )
        self.assertEqual(
            self.report["schema"],
            "triad.r29.context_facade_coverage.offline_visual_audit.v1",
        )
        self.assertEqual(
            self.report["validationStatus"], "PASS_SOURCE_CONSISTENCY_ONLY"
        )
        self.assertEqual(
            self.report["fidelityEvidenceStatus"],
            "BLOCKED_ON_AUTHORITATIVE_FIDELITY_DATA",
        )

    def test_current_source_inputs_are_exactly_hash_bound(self):
        expected = {
            "generator": R29 / "build_r29_context_facade_coverage.py",
            "contract": R29 / "r29_context_facade_coverage.contract.json",
            "manifest": MANIFEST,
            "facadeObj": GENERATED / "SM_IPV5D_R29_ContextFacadeCoverage_Render.obj",
            "facadeMtl": GENERATED / "IstanaPublicViewV5DR29ContextFacadeCoverage.mtl",
            "contextShellForPreviewOnly": R29.parent
            / "GeneratedFrozenFallback"
            / "SM_IPV5D_OSMCurrentSurroundings_Render.obj",
            "contextShellMtlForPreviewOnly": R29.parent
            / "GeneratedFrozenFallback"
            / "SM_IPV5D_OSMCurrentSurroundings_Render.mtl",
            "cpuRendererPrimitives": R29.parent
            / "R28EnvironmentalDressing"
            / "render_r28_offline_visual_audit.py",
        }
        receipts = self.report["sourceAssetReceipts"]
        self.assertEqual(set(receipts), set(expected))
        for key, path in expected.items():
            with self.subTest(key=key):
                self.assertEqual(receipts[key]["bytes"], path.stat().st_size)
                self.assertEqual(receipts[key]["sha256"], sha256(path))

    def test_exact_three_preview_receipts_are_current(self):
        views = self.report["previewViews"]
        self.assertEqual(len(views), 3)
        self.assertEqual({view["file"] for view in views}, VIEW_FILES)
        self.assertEqual(len({view["viewId"] for view in views}), 3)
        for view in views:
            with self.subTest(view=view["viewId"]):
                path = AUDIT / view["file"]
                receipt = view["generatedPreviewReceipt"]
                width, height = png_dimensions(path)
                self.assertEqual(receipt["bytes"], path.stat().st_size)
                self.assertEqual(receipt["sha256"], sha256(path))
                self.assertEqual(receipt["widthPixels"], width)
                self.assertEqual(receipt["heightPixels"], height)
                self.assertRegex(receipt["decodedRgbaSha256"], r"^[A-F0-9]{64}$")
                self.assertGreaterEqual(width, 1700)
                self.assertGreaterEqual(height, 1300)

    def test_coverage_and_geometry_counts_match_current_manifest(self):
        counts = self.report["derivedCounts"]
        self.assertEqual(counts["sourceBuildingPartCount"], 1305)
        self.assertEqual(counts["annulusBuildingPartCount"], 1302)
        self.assertEqual(counts["selectedBuildingPartCount"], 1174)
        self.assertEqual(counts["coverageMultiplierOverR28"], 9.171875)
        self.assertEqual(counts["apertureGroupCount"], 20011)
        self.assertEqual(counts["bottomFrameRailCount"], 20011)
        self.assertEqual(counts["continuousSillLedgeCount"], 7881)
        self.assertEqual(counts["awningProxyCount"], 46)
        self.assertEqual(counts["residentialBalconyProxyCount"], 28)
        self.assertEqual(counts["triangleCount"], 256850)
        self.assertEqual(counts["materialSlotCount"], 11)
        self.assertEqual(
            counts["triangleCount"], self.manifest["facadeCoverage"]["triangleCount"]
        )

    def test_gallery_proves_isolated_source_geometry_cues(self):
        gallery = next(
            view
            for view in self.report["previewViews"]
            if view["viewId"] == "CLOSE_FACADE_CUE_GALLERY"
        )["metrics"]
        self.assertEqual(gallery["panelCount"], 4)
        self.assertEqual(gallery["actualObjCropPanelCount"], 3)
        awning = gallery["awningEvidence"]
        self.assertEqual((awning["proxyCount"], awning["trianglesPerProxy"]), (46, 8))
        self.assertEqual(awning["renderMetrics"]["triangleCountDrawn"], 8)
        self.assertTrue(awning["renderMetrics"]["isolatedFromFrozenContextShell"])
        balcony = gallery["balconyEvidence"]
        self.assertEqual((balcony["proxyCount"], balcony["trianglesPerProxy"]), (28, 6))
        self.assertEqual(balcony["renderMetrics"]["triangleCountDrawn"], 14)
        self.assertTrue(balcony["renderMetrics"]["isolatedFromFrozenContextShell"])
        self.assertEqual(
            gallery["physicalReliefMeters"],
            {
                "frameOutwardOffsetMeters": 0.12,
                "frameToGlassReliefMeters": 0.075,
                "glassOutwardOffsetMeters": 0.045,
                "parapetCapHeightMeters": 0.18,
                "sillOutwardProjectionMeters": 0.22,
            },
        )
        aperture = gallery["apertureEvidence"]
        self.assertEqual(aperture["bottomFrameRailCount"], 20011)
        self.assertTrue(aperture["everyGroupedApertureFourSided"])
        self.assertTrue(aperture["bottomRailIntersectsContinuousSillTop"])

    def test_reference_and_native_capture_boundary_is_fail_closed(self):
        coverage = self.report["actualContextReferenceCoverage"]
        self.assertEqual((coverage["coveredViewCount"], coverage["requiredViewCount"]), (0, 3))
        self.assertEqual(coverage["coverageFraction"], 0.0)
        self.assertFalse(coverage["comparisonPerformed"])
        self.assertEqual(
            coverage["status"], "BLOCKED_ON_AUTHORITATIVE_FIDELITY_DATA"
        )
        self.assertEqual(len(coverage["views"]), 3)
        for view in coverage["views"]:
            self.assertFalse(view["referenceAvailable"])
            self.assertFalse(view["comparisonPerformed"])
            self.assertTrue(all(value is None for value in view["actualReference"].values()))
        self.assertTrue(
            all(value is False for value in self.report["sourceOnlyBoundary"].values())
        )

    def test_readme_is_honest_and_render_labels_are_ascii(self):
        readme = README.read_text(encoding="utf-8")
        for token in (
            "Coverage: **0/3**",
            "BLOCKED_ON_AUTHORITATIVE_FIDELITY_DATA",
            "not Unreal screenshots",
            "not hyperrealistic at close range",
            "flat colours without texture/normal/weathering maps",
            "No comparison was performed or implied",
            "20,011 bottom frame rails",
        ):
            self.assertIn(token, readme)
        AUDITOR.read_text(encoding="ascii")

    def test_stdlib_check_mode_is_read_only_and_passes(self):
        before = {
            path.name: (path.stat().st_mtime_ns, sha256(path))
            for path in AUDIT.iterdir()
            if path.is_file()
        }
        result = subprocess.run(
            [sys.executable, "-B", str(AUDITOR), "--check"],
            cwd=REPO,
            text=True,
            capture_output=True,
            check=False,
            timeout=45,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('"actualContextReferenceCoverage": "0/3"', result.stdout)
        self.assertIn('"visualCaptureAccepted": false', result.stdout)
        after = {
            path.name: (path.stat().st_mtime_ns, sha256(path))
            for path in AUDIT.iterdir()
            if path.is_file()
        }
        self.assertEqual(after, before)


if __name__ == "__main__":
    unittest.main()
