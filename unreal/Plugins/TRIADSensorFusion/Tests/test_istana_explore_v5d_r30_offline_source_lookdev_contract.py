from __future__ import annotations

import hashlib
import json
import struct
import subprocess
import sys
import unittest
from pathlib import Path


REPO = Path(__file__).absolute().parents[4]
R30 = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Surroundings"
    / "R30FacadeLookdev"
)
RENDERER = R30 / "render_r30_offline_source_lookdev.py"
AUDIT = R30 / "OfflineSourceLookdev"
PREVIEW = AUDIT / "r30_facade_cpu_source_lookdev.png"
REPORT = AUDIT / "r30_facade_cpu_source_lookdev.json"
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


class R30OfflineSourceLookdevContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        for path in (RENDERER, AUDIT, PREVIEW, REPORT, README):
            if not path.exists():
                raise AssertionError(f"missing R30 offline lookdev artifact: {path}")
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
            "triad.r30.facade_lookdev.offline_cpu_source_preview.v1",
        )
        self.assertEqual(self.report["validationStatus"], "PASS_SOURCE_LOOKDEV_ONLY")
        self.assertIs(self.report["visualAcceptance"], False)

    def test_exact_r29_geometry_and_r30_texture_inputs_are_hash_bound(self) -> None:
        receipts = self.report["sourceAssetReceipts"]
        self.assertIn("r29FacadeObj", receipts)
        self.assertIn("r30Contract", receipts)
        self.assertIn("pinnedR28RendererPrimitives", receipts)
        texture_keys = {key for key in receipts if key.startswith("texture") and key != "textureManifest"}
        self.assertEqual(12, len(texture_keys))
        for key, receipt in receipts.items():
            with self.subTest(key=key):
                path = REPO / receipt["file"]
                self.assertTrue(path.is_file())
                self.assertEqual(receipt["bytes"], path.stat().st_size)
                self.assertEqual(receipt["sha256"], sha256(path))

    def test_complete_mesh_was_validated_but_only_bounded_crop_was_retained(self) -> None:
        counts = self.report["derivedCounts"]
        self.assertEqual(counts["sourceFacadeTriangleCount"], 256_850)
        self.assertEqual(counts["sourceFacadeMaterialSectionCount"], 11)
        self.assertEqual(counts["texturedFacadeSectionCount"], 8)
        self.assertEqual(counts["proceduralGlassSectionCount"], 3)
        self.assertEqual(counts["texturedFacadeTriangleCount"], 216_828)
        self.assertAlmostEqual(counts["texturedFacadeTriangleFraction"], 216_828 / 256_850)
        self.assertLess(counts["retainedFacadeTriangleCount"], 3_000)
        self.assertLess(counts["retainedBaseTriangleCount"], 500)
        self.assertEqual(sum(counts["sourceMaterialTriangleCounts"].values()), 256_850)

    def test_preview_receipt_and_cpu_raster_contract_are_explicit(self) -> None:
        receipt = self.report["previewReceipt"]
        self.assertEqual((receipt["widthPixels"], receipt["heightPixels"]), (2200, 1320))
        self.assertEqual(png_dimensions(PREVIEW), (2200, 1320))
        self.assertEqual(receipt["bytes"], PREVIEW.stat().st_size)
        self.assertEqual(receipt["sha256"], sha256(PREVIEW))
        self.assertRegex(receipt["decodedRgbaSha256"], r"^[A-F0-9]{64}$")
        settings = self.report["renderSettings"]
        self.assertEqual(settings["uvInterpolation"], "PERSPECTIVE_CORRECT_RECIPROCAL_DEPTH")
        self.assertIn("Z_BUFFER", settings["visibility"])
        self.assertIn("BILINEAR_WRAP", settings["textureSampling"])
        self.assertEqual(settings["previewTextureMipPixels"], [512, 512])
        self.assertEqual(settings["minimumRenderFreeVirtualBytes"], 1_500_000_000)
        self.assertEqual(
            settings["opaqueGlassApproximation"],
            "TANGENT_CAMERA_PARALLAX_STOREY_BAND_PLUS_FRESNEL_SKY_TINT_NO_TRANSLUCENCY",
        )

    def test_opaque_glass_probe_proves_source_math_without_self_accepting_visuals(self) -> None:
        probe = self.report["opaqueGlassProbe"]
        self.assertEqual(probe["sampleViewAnglesDegrees"], [0.0, 55.0, 75.0])
        self.assertEqual(
            set(probe["samples"]),
            {
                "MI_IPV5D_R29_GlassCool",
                "MI_IPV5D_R29_GlassWarm",
                "MI_IPV5D_R29_GlassNeutral",
            },
        )
        for samples in probe["samples"].values():
            self.assertEqual(len(samples), 3)
            weights = [sample["reflectionWeight"] for sample in samples]
            self.assertLess(weights[0], weights[1])
            self.assertLess(weights[1], weights[2])
            for sample in samples:
                self.assertEqual(3, len(sample["linearBaseColor"]))
                self.assertGreaterEqual(sample["roughness"], 0.0)
                self.assertLessEqual(sample["roughness"], 1.0)
        checks = probe["mechanicalChecks"]
        self.assertIs(checks["reflectionWeightStrictlyIncreasesTowardGrazing"], True)
        self.assertGreater(checks["minimumFaceOnFamilyColourSeparation"], 0.02)
        self.assertGreater(checks["minimumFaceToGrazingLinearColourChange"], 0.10)
        self.assertIs(checks["allRoughnessSamplesClampedZeroToOne"], True)
        self.assertIs(probe["nativeVisualAcceptanceClaimed"], False)

    def test_fail_closed_labels_and_claim_boundary_are_complete(self) -> None:
        labels = set(self.report["prominentLabels"])
        for label in (
            "OFFLINE CPU SOURCE LOOKDEV",
            "NOT UNREAL",
            "NOT NATIVE R30",
            "NOT CESIUM OR PROVIDER COMPOSITION",
            "NOT ACTUAL-SITE OR PHYSICAL-MATERIAL TRUTH",
            "VISUAL ACCEPTANCE = FALSE",
        ):
            self.assertIn(label, labels)
        self.assertTrue(all(value is False for value in self.report["claimBoundary"].values()))
        readme = README.read_text(encoding="utf-8")
        for phrase in (
            "not an Unreal screenshot",
            "not a native R30 integration",
            "not Cesium/provider composition",
            "not an actual-site",
            "Visual acceptance remains **false**",
            "frequently synthetic-height geometry",
            "opaque glass Fresnel",
            "face-on/55-degree/75-degree glass probes",
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
            ("--self-test", '"perspectiveCorrectInterpolation": true'),
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
