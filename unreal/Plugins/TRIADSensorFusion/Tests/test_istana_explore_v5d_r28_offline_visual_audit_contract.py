from __future__ import annotations

import hashlib
import json
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


REPO = Path(__file__).absolute().parents[4]
R28 = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Surroundings"
    / "R28EnvironmentalDressing"
)
GENERATED = R28 / "Generated"
AUDITOR = R28 / "render_r28_offline_visual_audit.py"
AUDIT = R28 / "OfflineVisualAudit"
REPORT = AUDIT / "r28_offline_visual_audit.json"
MANIFEST = GENERATED / "IstanaPublicViewV5DR28EnvironmentalDressing.manifest.json"
GUIDE = REPO / "docs" / "ISTANA_EXPLORE_V5D_R28_ENVIRONMENT_INTEGRATION.md"
VIEW_FILES = {
    "r28_full_coverage_plan.png",
    "r28_public_realm_drape_height.png",
    "r28_priority_sector_oblique.png",
    "r28_densest_facade_detail.png",
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


class IstanaExploreV5DR28OfflineVisualAuditContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for path in (AUDITOR, AUDIT, REPORT, MANIFEST, GUIDE):
            if not path.exists():
                raise AssertionError(f"missing R28 offline-audit artifact: {path}")
        cls.report_text = REPORT.read_text(encoding="utf-8")
        cls.report = json.loads(cls.report_text)
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def test_report_is_canonical_current_source_only_receipt(self):
        self.assertEqual(
            self.report_text,
            json.dumps(
                self.report, indent=2, sort_keys=True, ensure_ascii=True
            )
            + "\n",
        )
        self.assertEqual(self.report["schema"], "triad.r28.offline_visual_audit.v2")
        self.assertEqual(
            self.report["validationStatus"], "PASS_SOURCE_CONSISTENCY_ONLY"
        )
        self.assertEqual(
            self.report["fidelityEvidenceStatus"],
            "BLOCKED_ON_AUTHORITATIVE_FIDELITY_DATA",
        )
        self.assertEqual(
            self.report["scope"],
            "READ_ONLY_REPO_SOURCE_AUDIT_NO_NATIVE_UNREAL_OR_PROVIDER_ACCESS",
        )

    def test_current_architecture_realm_mtl_and_manifest_are_hash_bound(self):
        receipts = self.report["sourceAssetReceipts"]
        expected = {
            "architectureObj": GENERATED
            / "SM_IPV5D_R28_ContextArchitecturalDressing_Render.obj",
            "publicRealmObj": GENERATED
            / "SM_IPV5D_R28_ConnectivePublicRealm_Render.obj",
            "materialCatalogue": GENERATED
            / "IstanaPublicViewV5DR28EnvironmentalDressing.mtl",
            "manifest": MANIFEST,
        }
        for key, path in expected.items():
            with self.subTest(key=key):
                self.assertEqual(receipts[key]["bytes"], path.stat().st_size)
                self.assertEqual(receipts[key]["sha256"], sha256(path))

        output_receipts = {
            item["file"]: item for item in self.report["receipts"]
        }
        self.assertEqual(
            set(output_receipts),
            {item["file"] for item in self.manifest["outputs"]},
        )
        for expected_receipt in self.manifest["outputs"]:
            actual = output_receipts[expected_receipt["file"]]
            self.assertTrue(actual["matches"])
            self.assertEqual(actual["actualBytes"], expected_receipt["bytes"])
            self.assertEqual(actual["actualSha256"], expected_receipt["sha256"])

    def test_manifest_derived_counts_are_current_and_not_the_stale_85_part_pass(self):
        architecture = self.manifest["architecturalDressing"]
        realm = self.manifest["connectivePublicRealm"]
        expected = {
            "sourceBuildingFeatureCount": architecture["sourceFeatureCount"],
            "sourceBuildingPartCount": architecture["sourcePartCount"],
            "eligibleBuildingPartCount": architecture["eligibleAnnulusPartCount"],
            "selectedBuildingPartCount": architecture["selectedPartCount"],
            "undressedBuildingPartCount": architecture["sourcePartCount"]
            - architecture["selectedPartCount"],
            "selectedBuildingPartFraction": architecture["selectedPartCount"]
            / architecture["sourcePartCount"],
            "windowCount": architecture["windowCount"],
            "architecturalTriangleCount": architecture["triangleCount"],
            "architecturalSourceCornerCount": architecture["sourceCornerCount"],
            "parapetEdgeCount": architecture["parapetEdgeCount"],
            "awningCount": architecture["awningCount"],
            "acceptedPublicRealmSegmentCount": realm["acceptedSegmentCount"],
            "publicRealmTriangleCount": realm["triangleCount"],
            "terrainSampleRequestCount": realm["terrainSampleRequestCount"],
            "unresolvedTerrainSampleCount": realm["unresolvedTerrainSampleCount"],
        }
        self.assertEqual(self.report["derivedCounts"], expected)
        self.assertEqual(expected["selectedBuildingPartCount"], 128)
        self.assertEqual(expected["windowCount"], 14786)
        self.assertEqual(expected["architecturalTriangleCount"], 149758)
        self.assertEqual(expected["architecturalSourceCornerCount"], 449274)
        self.assertNotIn("Only 85", (AUDIT / "README.md").read_text(encoding="utf-8"))

    def test_exact_four_current_view_receipts_are_complete(self):
        views = self.report["previewViews"]
        self.assertEqual(len(views), 4)
        self.assertEqual({view["file"] for view in views}, VIEW_FILES)
        self.assertEqual(len({view["viewId"] for view in views}), 4)
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
                self.assertGreater(width, 1000)
                self.assertGreater(height, 1000)

    def test_actual_context_reference_coverage_is_honestly_zero_of_four(self):
        coverage = self.report["actualContextReferenceCoverage"]
        self.assertEqual(coverage["requiredViewCount"], 4)
        self.assertEqual(coverage["coveredViewCount"], 0)
        self.assertEqual(coverage["coverageFraction"], 0.0)
        self.assertFalse(coverage["comparisonPerformed"])
        self.assertEqual(
            coverage["status"], "BLOCKED_ON_AUTHORITATIVE_FIDELITY_DATA"
        )
        self.assertEqual(
            coverage["claim"], "NO_ACTUAL_SITE_REFERENCE_COMPARISON_WAS_PERFORMED"
        )
        self.assertEqual(len(coverage["views"]), 4)
        for view in coverage["views"]:
            self.assertFalse(view["referenceAvailable"])
            self.assertFalse(view["comparisonPerformed"])
            self.assertEqual(
                set(view["actualReference"]),
                {"source", "captureDate", "licence", "sha256", "cameraCorrespondence"},
            )
            self.assertTrue(
                all(value is None for value in view["actualReference"].values())
            )

    def test_operator_guide_keeps_offline_audit_outside_capture_authority(self):
        guide = GUIDE.read_text(encoding="utf-8")
        offline = guide.split("## Offline source audit (not proof capture)", 1)[1]
        offline = offline.split("## Proof capture", 1)[0]
        for token in (
            "OfflineVisualAudit/README.md",
            "actualContextReferenceCoverage=0/4",
            "comparisonPerformed=false",
            "BLOCKED_ON_AUTHORITATIVE_FIDELITY_DATA",
            "cannot satisfy any",
        ):
            self.assertIn(token, offline)

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
            timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn('"actualContextReferenceCoverage": "0/4"', result.stdout)
        self.assertIn(
            '"fidelityEvidenceStatus": "BLOCKED_ON_AUTHORITATIVE_FIDELITY_DATA"',
            result.stdout,
        )
        after = {
            path.name: (path.stat().st_mtime_ns, sha256(path))
            for path in AUDIT.iterdir()
            if path.is_file()
        }
        self.assertEqual(after, before)

    def test_check_rejects_a_stale_preview_receipt(self):
        with tempfile.TemporaryDirectory(prefix="triad-r28-audit-negative-") as root:
            candidate = Path(root) / "audit"
            shutil.copytree(AUDIT, candidate)
            stale_view = candidate / "r28_densest_facade_detail.png"
            stale_view.write_bytes(stale_view.read_bytes() + b"STALE")
            result = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(AUDITOR),
                    "--check",
                    "--output-dir",
                    str(candidate),
                ],
                cwd=REPO,
                text=True,
                capture_output=True,
                check=False,
                timeout=30,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("R28 preview receipt mismatch", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
