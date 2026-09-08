from __future__ import annotations

import hashlib
import importlib.util
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib


REPO = Path(__file__).absolute().parents[4]
R32 = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Vegetation"
    / "R32MediumDistanceTurf"
)
RENDERER = R32 / "render_r32_offline_visual_audit.py"
AUDIT = R32 / "OfflineVisualAudit"
PREVIEW = AUDIT / "r32_turf_visibility_source_audit.png"
REPORT = AUDIT / "r32_turf_visibility_source_audit.json"
README = AUDIT / "README.md"
EXPECTED_INPUT_KEYS = {
    "renderer",
    "r32Contract",
    "r32RuntimeActor",
    "r32Player0CaptureLibrary",
    "r29MaterialFactory",
    "r32MaterialFactory",
    "r29SourceManifest",
    "r29FineClusterObj",
    "r29BroadClusterObj",
    "r29MixedClusterObj",
}
EXPECTED_CLAIM_KEYS = {
    "unrealRender",
    "nativeR32Integration",
    "player0Capture",
    "nativeCameraEquivalence",
    "shaderOrPbrEquivalence",
    "nativeVisualAcceptance",
    "performanceAcceptance",
    "actualSiteOrBotanicalTruth",
    "hyperrealism",
    "collisionAuthority",
    "navigationAuthority",
    "sensorAuthority",
    "rfAuthority",
    "geospatialAuthority",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def smoothstep(edge0: float, edge1: float, value: float) -> float:
    t = max(0.0, min(1.0, (value - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def decode_rgb_png(path: Path) -> tuple[int, int, bytes, tuple[bytes, ...]]:
    payload = path.read_bytes()
    if payload[:8] != b"\x89PNG\r\n\x1a\n":
        raise AssertionError(f"not a PNG: {path}")
    cursor = 8
    width = height = 0
    idat = bytearray()
    kinds: list[bytes] = []
    while cursor < len(payload):
        length = struct.unpack(">I", payload[cursor : cursor + 4])[0]
        kind = payload[cursor + 4 : cursor + 8]
        body = payload[cursor + 8 : cursor + 8 + length]
        crc = struct.unpack(
            ">I", payload[cursor + 8 + length : cursor + 12 + length]
        )[0]
        if zlib.crc32(kind + body) & 0xFFFFFFFF != crc:
            raise AssertionError(f"bad PNG CRC: {kind!r}")
        cursor += 12 + length
        kinds.append(kind)
        if kind == b"IHDR":
            width, height, depth, color, compression, filtering, interlace = (
                struct.unpack(">IIBBBBB", body)
            )
            if (depth, color, compression, filtering, interlace) != (
                8,
                2,
                0,
                0,
                0,
            ):
                raise AssertionError("unexpected PNG encoding")
        elif kind == b"IDAT":
            idat.extend(body)
        elif kind == b"IEND":
            break
    raw = zlib.decompress(bytes(idat))
    row_bytes = width * 3
    if len(raw) != height * (row_bytes + 1):
        raise AssertionError("unexpected decoded PNG length")
    decoded = bytearray()
    for row in range(height):
        start = row * (row_bytes + 1)
        if raw[start] != 0:
            raise AssertionError("unexpected PNG row filter")
        decoded.extend(raw[start + 1 : start + 1 + row_bytes])
    return width, height, bytes(decoded), tuple(kinds)


def import_renderer() -> object:
    name = "_triad_r32_offline_visual_audit_test_module"
    spec = importlib.util.spec_from_file_location(name, RENDERER)
    if spec is None or spec.loader is None:
        raise AssertionError("could not import R32 offline renderer")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


class R32OfflineVisualAuditContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        for path in (RENDERER, AUDIT, PREVIEW, REPORT, README):
            if not path.exists():
                raise AssertionError(f"missing R32 offline audit artifact: {path}")
        cls.report_text = REPORT.read_text(encoding="utf-8")
        cls.report = json.loads(cls.report_text)

    def test_exact_artifact_roster_and_canonical_source_only_receipt(self) -> None:
        self.assertEqual(
            {path.name for path in AUDIT.iterdir()},
            {PREVIEW.name, REPORT.name, README.name},
        )
        self.assertTrue(all(path.is_file() for path in AUDIT.iterdir()))
        self.assertEqual(
            self.report_text,
            json.dumps(
                self.report,
                indent=2,
                sort_keys=True,
                ensure_ascii=True,
            )
            + "\n",
        )
        self.assertEqual(
            self.report["schema"],
            "triad.istana_explore_v5d.r32_turf_visibility.offline_source_audit.v1",
        )
        self.assertEqual(
            self.report["validationStatus"],
            "PASS_SOURCE_DIAGNOSTIC_ONLY",
        )
        self.assertIs(self.report["visualAcceptance"], False)

    def test_exact_existing_r32_and_r29_sources_are_hash_bound(self) -> None:
        receipts = self.report["sourceAssetReceipts"]
        self.assertEqual(set(receipts), EXPECTED_INPUT_KEYS)
        for key, receipt in receipts.items():
            with self.subTest(key=key):
                path = REPO / receipt["file"]
                self.assertTrue(path.is_file())
                self.assertEqual(receipt["bytes"], path.stat().st_size)
                self.assertEqual(receipt["sha256"], sha256(path))
        binding = self.report["sourceBinding"]
        self.assertEqual(binding["r32SourceTransformCount"], 18_432)
        self.assertEqual(binding["r32SelectedTransformCount"], 4_608)
        self.assertEqual(binding["r32SelectedFraction"], 0.25)
        self.assertEqual(
            binding["r29ModeledBladeCounts"],
            [64, 56, 72],
        )
        self.assertEqual(
            binding["r29MeshMaximumHeightCentimeters"],
            [13.844719, 18.459288, 19.605375],
        )
        self.assertEqual(binding["targetTipHeightCentimeters"], [5.0, 9.0])
        self.assertEqual(binding["hismCullMeters"], [65.0, 90.0])
        self.assertEqual(binding["wpoDisableMeters"], 60.0)
        self.assertEqual(binding["r29RetainedFarNormalResponse"], 0.10)
        self.assertEqual(binding["r32MaterialDerivativeCount"], 4)
        self.assertEqual(
            binding["r32MediumResponseFullReadMeters"], [20.0, 65.0]
        )
        self.assertEqual(binding["r32ResponseFadeOutMeters"], [65.0, 76.0])
        self.assertEqual(len(binding["r32MaterialObjectPaths"]), 4)

    def test_exact_censuses_and_presented_budgets_are_recomputed(self) -> None:
        counts = self.report["derivedCounts"]
        self.assertEqual(counts["sourceModeledBladesPerCarrier"], [64, 56, 72])
        self.assertEqual(counts["sourceTrianglesPerCarrier"], [1024, 896, 1152])
        self.assertEqual(counts["sourceTransformCount"], 18_432)
        self.assertEqual(counts["selectedTransformCount"], 4_608)
        self.assertEqual(counts["profileCount"], 4)
        self.assertEqual(counts["meshVariantCount"], 3)
        self.assertEqual(counts["profileVariantBucketCount"], 12)
        self.assertTrue(counts["allProfileVariantBucketsNonEmpty"])
        expected_blades = 2310 * 64 + 1380 * 56 + 918 * 72
        expected_triangles = 2310 * 1024 + 1380 * 896 + 918 * 1152
        self.assertEqual(counts["presentedModeledBladeBudget"], expected_blades)
        self.assertEqual(expected_blades, 291_216)
        self.assertEqual(counts["worstCaseVisibleTriangleCount"], expected_triangles)
        self.assertEqual(expected_triangles, 4_659_456)

    def test_six_fixed_probes_expose_projected_size_and_cull_risk(self) -> None:
        rows = self.report["probeDiagnostics"]
        self.assertEqual(
            [row["id"] for row in rows],
            ["012m", "020m", "050m", "065m", "090m", "095m"],
        )
        self.assertEqual(
            [row["distanceMeters"] for row in rows],
            [12.0, 20.0, 50.0, 65.0, 90.0, 95.0],
        )
        self.assertEqual(
            [row["nativePosePitchDegrees"] for row in rows],
            [
                -7.782210724,
                -4.703535,
                -1.87862806,
                -1.445309952,
                -1.04394089,
                -0.989007849,
            ],
        )
        self.assertEqual(
            [row["exactZ0IntersectionProbe"] for row in rows],
            [True, False, True, True, True, True],
        )
        self.assertAlmostEqual(
            rows[1]["actualZ0IntersectionDistanceMeters"],
            19.932647,
            places=5,
        )
        for index in (0, 2, 3, 4, 5):
            self.assertAlmostEqual(
                rows[index]["actualZ0IntersectionDistanceMeters"],
                rows[index]["distanceMeters"],
                places=5,
            )
        self.assertEqual(
            [row["calibratedVisibility"] for row in rows],
            [1.0, 1.0, 1.0, 1.0, 0.0, 0.0],
        )
        self.assertEqual(
            [row["wpoExpectedActive"] for row in rows],
            [True, True, True, False, False, False],
        )
        for row in rows:
            distance = row["distanceMeters"]
            self.assertAlmostEqual(
                row["roughnessDistanceDetailTerm"],
                1.0 - smoothstep(18.0, 65.0, distance),
                places=8,
            )
            self.assertAlmostEqual(
                row["normalDistanceDetailTerm"],
                1.0 - smoothstep(18.0, 60.0, distance),
                places=8,
            )
            self.assertAlmostEqual(
                row[
                    "normalResponseCoefficientBeforeBladeHeightAndInstanceFade"
                ],
                0.10
                + 0.34 * (1.0 - smoothstep(18.0, 60.0, distance)),
                places=8,
            )
            self.assertFalse(row["nativeReadabilityAccepted"])
        self.assertTrue(rows[2]["belowFourPixelNominalTipRisk"])
        self.assertTrue(rows[3]["belowFourPixelNominalTipRisk"])
        self.assertTrue(rows[2]["mostlySubpixelRibbonWidthRisk"])
        self.assertEqual(rows[4]["postGateProjectedRibbonSegmentCount"], 0)
        self.assertEqual(rows[5]["postGateProjectedRibbonSegmentCount"], 0)
        self.assertGreater(rows[0]["rasterizedCentralStripSegmentCount"], 0)
        self.assertGreater(rows[1]["rasterizedCentralStripSegmentCount"], 0)
        self.assertGreater(rows[2]["rasterizedCentralStripSegmentCount"], 0)
        self.assertGreater(rows[3]["rasterizedCentralStripSegmentCount"], 0)

    def test_dense_fade_probe_is_monotonic_and_non_authorizing(self) -> None:
        fade = self.report["fadeDiagnostic"]
        self.assertEqual(fade["fadeStartsAtMeters"], 65.0)
        self.assertEqual(fade["cullEndsAtMeters"], 90.0)
        self.assertEqual(fade["wpoDisabledAtMeters"], 60.0)
        self.assertFalse(fade["nativeUnderlyingLawnContinuityKnown"])
        self.assertFalse(fade["nativeBaldBandOrHardCutoffAccepted"])
        dense = [
            row
            for row in fade["samples"]
            if 65.0 <= row["distanceMeters"] <= 90.0
        ]
        self.assertEqual(len(dense), 101)
        self.assertEqual(dense[0]["calibratedVisibility"], 1.0)
        self.assertEqual(dense[50]["distanceMeters"], 77.5)
        self.assertEqual(dense[50]["calibratedVisibility"], 0.5)
        self.assertEqual(dense[-1]["calibratedVisibility"], 0.0)
        self.assertTrue(
            all(
                left["calibratedVisibility"]
                >= right["calibratedVisibility"]
                for left, right in zip(dense, dense[1:])
            )
        )

    def test_continuous_material_response_source_intent_is_recomputed(self) -> None:
        diagnostic = self.report["materialResponseDiagnostic"]
        self.assertEqual(
            diagnostic["method"],
            "CPU_DOUBLE_PRECISION_SOURCE_MATH_MIRROR_ONLY",
        )
        self.assertEqual(diagnostic["broadScaleMeters"], 11.0)
        self.assertEqual(diagnostic["mesoScaleMeters"], 3.4)
        self.assertEqual(diagnostic["grid"]["dimensions"], [65, 41])
        self.assertEqual(diagnostic["grid"]["sampleCount"], 2665)
        self.assertEqual(
            [row["response"] for row in diagnostic["distanceGateSamples"]],
            [0.0, 0.0, 0.5, 1.0, 1.0, 1.0, 0.5, 0.0, 0.0],
        )
        self.assertEqual(
            [
                row["response"]
                for row in diagnostic["screenFootprintBandLimitSamples"]
            ],
            [1.0, 1.0, 0.5, 0.0, 0.0],
        )
        statistics = diagnostic["organicResponse"]
        self.assertGreaterEqual(statistics["minimum"], 0.0)
        self.assertLessEqual(statistics["maximum"], 1.0)
        self.assertGreater(statistics["standardDeviation"], 0.05)
        self.assertGreater(
            statistics["meanAbsoluteHorizontalNeighborDelta"], 0.0
        )
        self.assertGreater(
            statistics["meanAbsoluteVerticalNeighborDelta"], 0.0
        )
        self.assertFalse(diagnostic["hardStripeOrCheckerSelectorFound"])
        self.assertFalse(diagnostic["nativeShaderEvaluated"])
        self.assertFalse(diagnostic["physicalPbrTruthEvaluated"])
        self.assertFalse(diagnostic["siteMeasuredMaterialTruthEvaluated"])

    def test_preview_is_hash_pinned_decodable_and_metadata_free(self) -> None:
        width, height, decoded, kinds = decode_rgb_png(PREVIEW)
        self.assertEqual((width, height), (2560, 1440))
        self.assertEqual(kinds, (b"IHDR", b"IDAT", b"IEND"))
        receipt = self.report["previewReceipt"]
        self.assertEqual(receipt["bytes"], PREVIEW.stat().st_size)
        self.assertEqual(receipt["sha256"], sha256(PREVIEW))
        self.assertEqual(
            receipt["decodedRgbSha256"],
            hashlib.sha256(decoded).hexdigest().upper(),
        )
        self.assertGreater(len(set(decoded[::4096])), 4)

    def test_fail_closed_claim_boundary_and_readme_are_complete(self) -> None:
        boundary = self.report["claimBoundary"]
        self.assertEqual(set(boundary), EXPECTED_CLAIM_KEYS)
        self.assertTrue(all(value is False for value in boundary.values()))
        method = self.report["renderMethod"]
        self.assertTrue(method["actualR29ObjGeometryParsed"])
        self.assertFalse(method["unrealRendererUsed"])
        self.assertFalse(method["pbrMaterialEvaluation"])
        self.assertTrue(method["r32ContinuousMaterialSourceMathMirrored"])
        self.assertFalse(method["nativeShaderEvaluation"])
        self.assertFalse(method["actualR32WorldTransformsLoaded"])
        self.assertFalse(method["hismInstanceFadeEvaluated"])
        self.assertFalse(method["unrealPerInstanceRandomEvaluated"])
        self.assertFalse(method["stableGateProxyIsUnrealEquivalent"])
        preservation = self.report["preservation"]
        self.assertTrue(all(value is False for value in preservation.values()))
        limitations = " ".join(self.report["knownLimitations"])
        for phrase in (
            "not Unreal",
            "does not pin Player0 FOV",
            "not the exact R23/R32 world-transform field",
            "HISM fade ordering",
            "CPU source-math mirror",
            "90-95 metre blend continuity remain unknown",
            "target-hardware performance",
        ):
            self.assertIn(phrase, limitations)
        readme = README.read_text(encoding="utf-8")
        for phrase in (
            "not an Unreal screenshot",
            "not a native R32 integration",
            "not a Player0 capture",
            "not equivalent to their native ordering",
            "physical PBR or site truth",
            "not the native R23/R32 transform field",
            "Native visual acceptance remains **false**",
            "ordered R30-R33 transaction/capture/human-review chain",
            "check and self-test modes are read-only",
        ):
            self.assertIn(phrase, readme)

    def test_check_and_self_test_are_read_only_and_cli_is_fail_closed(self) -> None:
        audited = [RENDERER, PREVIEW, REPORT, README] + [
            REPO / receipt["file"]
            for receipt in self.report["sourceAssetReceipts"].values()
        ]
        before = {
            str(path): (path.stat().st_mtime_ns, sha256(path))
            for path in set(audited)
        }
        for flag, token in (
            ("--check", '"nativeOrUnrealClaimed": false'),
            ("--self-test", '"writesPerformed": false'),
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
            str(path): (path.stat().st_mtime_ns, sha256(path))
            for path in set(audited)
        }
        self.assertEqual(after, before)
        for arguments in ((), ("--check", "--self-test")):
            result = subprocess.run(
                [sys.executable, "-B", str(RENDERER), *arguments],
                cwd=REPO,
                text=True,
                capture_output=True,
                check=False,
                timeout=15,
            )
            self.assertEqual(result.returncode, 2)

    def test_render_replays_all_three_artifacts_bit_for_bit(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            destination = Path(temporary) / "audit"
            result = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(RENDERER),
                    "--render",
                    "--output",
                    str(destination),
                ],
                cwd=REPO,
                text=True,
                capture_output=True,
                check=False,
                timeout=45,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for expected in (PREVIEW, REPORT, README):
                with self.subTest(file=expected.name):
                    self.assertEqual(
                        expected.read_bytes(),
                        (destination / expected.name).read_bytes(),
                    )

    def test_memory_guard_refuses_before_first_output_write(self) -> None:
        renderer = import_renderer()
        original = renderer.current_free_virtual_bytes
        renderer.current_free_virtual_bytes = (
            lambda: renderer.MIN_RENDER_FREE_VIRTUAL_BYTES - 1
        )
        try:
            with tempfile.TemporaryDirectory() as temporary:
                destination = Path(temporary) / "must_not_exist"
                with self.assertRaisesRegex(
                    RuntimeError,
                    "memory admission guard",
                ):
                    renderer.render(destination)
                self.assertFalse(destination.exists())
        finally:
            renderer.current_free_virtual_bytes = original


if __name__ == "__main__":
    unittest.main()
