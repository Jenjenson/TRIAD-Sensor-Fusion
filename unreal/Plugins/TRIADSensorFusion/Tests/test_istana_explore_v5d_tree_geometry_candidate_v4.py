import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


UNREAL_ROOT = Path(__file__).absolute().parents[3]
REPO_ROOT = UNREAL_ROOT.parent
CANDIDATE_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/TreeRealism/"
    "GeometryVariationCandidateV4"
)
BUILDER = CANDIDATE_ROOT / "build_tree_geometry_candidate_v4.py"
CONTRACT_PATH = CANDIDATE_ROOT / "tree_geometry_variation_candidate.v4.json"
MANIFEST_PATH = CANDIDATE_ROOT / "tree_geometry_instance_selector.v4.json"
AUDIT_ROOT = CANDIDATE_ROOT / "OfflineAudit"
AUDIT_PATH = AUDIT_ROOT / "tree_geometry_candidate_v4_audit.json"
PREVIEW_PATH = AUDIT_ROOT / "tree_geometry_candidate_v4_contact_sheet.png"
MATERIAL_V3_PATH = (
    CANDIDATE_ROOT.parent
    / "istana_public_view_v5d_tree_realism.material_response_amendment.v3.json"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


class ExploreV5DTreeGeometryCandidateV4Tests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
        cls.manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
        cls.audit = json.loads(AUDIT_PATH.read_text(encoding="utf-8"))
        cls.material_v3 = json.loads(MATERIAL_V3_PATH.read_text(encoding="utf-8"))

    def test_pack_is_explicitly_unadmitted_and_sequenced_after_r33(self):
        self.assertEqual(
            "triad.istana_public_view_explore_v5d_tree_realism."
            "geometry_variation_candidate.v4",
            self.contract["schema"],
        )
        self.assertEqual("UNADMITTED_VISUAL_REFERENCE_ONLY", self.contract["status"])
        state = self.contract["deliveryState"]
        self.assertEqual(15, state["candidateRecipeCount"])
        self.assertEqual(5, state["forms"])
        self.assertEqual(3, state["variantsPerForm"])
        for key in (
            "meshAssetsMaterialized",
            "unrealPackagesWritten",
            "mapModified",
            "nativeIntegrationAuthority",
            "nativeVisualAcceptanceProvided",
        ):
            self.assertFalse(state[key], key)
        sequencing = self.contract["sequencing"]
        self.assertEqual("R33", sequencing["predecessorAcceptanceRequired"])
        self.assertTrue(sequencing["futureSuccessorTransactionRequired"])
        self.assertFalse(sequencing["futureSuccessorIdentifierAssigned"])
        self.assertFalse(sequencing["queuedTransactionOrCaptureFilesModified"])

    def test_exact_five_cc0_source_mesh_receipts_and_decoded_geometry(self):
        sources = self.contract["sourceMeshes"]
        evidence = self.audit["sourceVertexEvidence"]
        self.assertEqual(5, len(sources))
        self.assertEqual(5, len(evidence))
        expected = {
            "umbrella": (818_396, "94B182081F7BA5D78DD5D8EC6CD55A9F57999AE19540E75629D2066E1FA5CC45"),
            "dome": (1_755_568, "110C7897AFEF4F9A5C7DD3131D10B221332932CCB089AF623F36B2A3D26A1CEE"),
            "highForkRounded": (1_218_741, "84990531ACD0CA2901F74435E198AD57C951A99EFD237B04A8B2676D3D6AF2AB"),
            "columnar": (3_055_019, "58F93F8D2022E6D72CEABAD5ABD13EFEC5036E1E07667E7E5C2767FAC607F41B"),
            "palm": (2_675, "F5178D5041F0BDD013D783B8458AA8F867E2CF0DE5C059E89830D05DA499FF8A"),
        }
        for source, probe in zip(sources, evidence):
            self.assertEqual("CC0-1.0", source["license"])
            path = REPO_ROOT / source["sourceFile"]["file"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(source["sourceFile"]["bytes"], path.stat().st_size)
            self.assertEqual(source["sourceFile"]["sha256"], sha256(path))
            count, decoded_hash = expected[source["form"]]
            self.assertEqual(count, source["expectedVertexCount"])
            self.assertEqual(count, probe["fullVertexCount"])
            self.assertEqual(decoded_hash, probe["decodedCoordinateBytesSha256"])
            self.assertEqual(source["selectedGeometry"], probe["selectedGeometry"])
            self.assertEqual("Z", probe["sourceBounds"]["upAxis"])
            self.assertGreaterEqual(probe["probe"]["vertexCount"], min(count, 32_768))

    def test_fifteen_unique_true_xyz_warp_recipes_are_bounded(self):
        candidates = self.contract["candidateVariants"]
        self.assertEqual(15, len(candidates))
        self.assertEqual(15, len({row["id"] for row in candidates}))
        self.assertEqual(15, len({row["candidateObjectPath"] for row in candidates}))
        self.assertEqual(15, len({row["candidatePackagePath"] for row in candidates}))
        forms = {source["form"] for source in self.contract["sourceMeshes"]}
        self.assertEqual(
            {"umbrella", "dome", "highForkRounded", "columnar", "palm"},
            forms,
        )
        for form in forms:
            rows = [row for row in candidates if row["form"] == form]
            self.assertEqual(["A", "B", "C"], [row["selector"] for row in rows])
            self.assertEqual(3, len({json.dumps(row["parameters"], sort_keys=True) for row in rows}))
        for row in candidates:
            params = row["parameters"]
            self.assertEqual(0.025, params["rootHoldNormalizedHeight"])
            self.assertGreater(params["crownFullNormalizedHeight"], params["crownStartNormalizedHeight"])
            self.assertTrue(0.88 <= min(params["crownScaleXY"]) <= 1.21)
            self.assertTrue(0.93 <= params["heightScale"] <= 1.07)
            self.assertLessEqual(max(abs(value) for value in params["bendBySourceHeightXY"]), 0.041)
            self.assertLessEqual(params["radialRippleFraction"], 0.028)

    def test_material_slot_semantics_are_exactly_bound_to_response_v3(self):
        alias = {"highFork": "highForkRounded"}
        expected = {}
        for row in self.material_v3["materials"]:
            form = alias.get(row["form"], row["form"])
            expected.setdefault(form, []).append(
                {
                    "slot": row["slot"],
                    "role": row["role"],
                    "responseMaterial": row["responseMaterial"],
                }
            )
        actual = {
            row["form"]: row["materialResponseV3Bindings"]
            for row in self.contract["sourceMeshes"]
        }
        self.assertEqual(expected, actual)
        warp = self.contract["vertexWarp"]
        self.assertFalse(warp["topologyOperationsAllowed"])
        self.assertTrue(warp["vertexPositionMutationRequired"])
        self.assertFalse(warp["uvVertexColourPolygonGroupAndMaterialSlotMutationAllowed"])
        self.assertTrue(warp["normalAndTangentRecomputeRequired"])

    def test_manifest_is_exact_deterministic_736_entry_source_selector(self):
        self.assertEqual(
            "triad.istana_public_view_explore_v5d_tree_realism."
            "geometry_variation_candidate.instance_selector.v4",
            self.manifest["schema"],
        )
        self.assertEqual(
            {"v4Main": 720, "v4Heritage": 9, "r29Landmark": 7, "combined": 736},
            self.manifest["sourceCensus"],
        )
        rows = self.manifest["rows"]
        self.assertEqual(736, len(rows))
        self.assertEqual(list(range(736)), [row["ordinal"] for row in rows])
        self.assertEqual(736, len({row["sourceInstanceKey"] for row in rows}))
        self.assertEqual(232, sum(row["sourceForm"] == "columnar" for row in rows[:720]))
        self.assertEqual(
            488,
            sum(
                row["sourceForm"] == "NATIVE_V4_RASTER_CLASSIFICATION_REQUIRED"
                for row in rows[:720]
            ),
        )
        lookup = self.manifest["candidateLookup"]
        for row in rows:
            self.assertIn(row["variantSelector"], "ABC")
            for form in row["allowedResolvedForms"]:
                self.assertIn(row["variantSelector"], lookup[form])
        selector_counts = {
            selector: sum(row["variantSelector"] == selector for row in rows)
            for selector in "ABC"
        }
        self.assertLessEqual(max(selector_counts.values()) - min(selector_counts.values()), 50)
        row_digest = hashlib.sha256(
            json.dumps(
                rows, sort_keys=True, separators=(",", ":"), ensure_ascii=True
            ).encode("ascii")
        ).hexdigest().upper()
        self.assertEqual(row_digest, self.manifest["rowsSha256"])
        self.assertEqual(sha256(CONTRACT_PATH), self.manifest["contract"]["sha256"])

    def test_transform_geography_and_simulation_authority_remain_fail_closed(self):
        transform = self.manifest["transformPreservation"]
        self.assertEqual(
            "IDENTITY_COPY_OF_SOURCE_WORLD_FTRANSFORM_VALUE",
            transform["operationForEveryRow"],
        )
        self.assertFalse(transform["manifestContainsInventedTransformNumbers"])
        self.assertFalse(transform["manifestChangesTranslationRotationOrScale"])
        self.assertTrue(transform["sourceKeyOrderPreserved"])
        self.assertTrue(transform["nativeBeforeAfterCanonicalTransformHashesRequired"])
        self.assertFalse(transform["nativeTransformReceiptPresent"])
        self.assertFalse(transform["nativeExactTransformEqualityProven"])
        preservation = self.contract["preservation"]
        self.assertTrue(all(value is False for value in preservation.values()))
        truth = dict(self.contract["truthBoundary"])
        self.assertTrue(truth.pop("appearanceCandidateOnly"))
        self.assertTrue(all(value is False for value in truth.values()))

    def test_source_probe_geometry_is_materially_distinct_and_nonfolding(self):
        evidence = self.audit["candidateGeometryEvidence"]
        self.assertEqual(15, len(evidence))
        self.assertEqual(15, len({row["warpedProbeCoordinateBytesSha256"] for row in evidence}))
        for row in evidence:
            self.assertEqual(0.0, row["rootPlaneMaximumAbsoluteDelta"])
            self.assertGreater(row["numericProbeMinimumJacobianDeterminant"], 0.50)
            self.assertGreater(row["normalizedDisplacement"]["rmsBySourceHeight"], 0.01)
            self.assertGreater(row["normalizedDisplacement"]["maximumBySourceHeight"], 0.02)
            self.assertGreater(row["warpedBounds"]["bboxVolumeRatioToSourceProbe"], 0.70)
        for row in self.audit["withinFormDistinctness"]:
            self.assertTrue(row["allWarpedProbeDigestsUnique"])
            self.assertGreaterEqual(row["minimumPairwiseRmsProbeSeparationBySourceHeight"], 0.015)
            self.assertGreaterEqual(row["minimumPairwiseMaximumProbeSeparationBySourceHeight"], 0.04)
            self.assertGreater(row["minimumPairwiseSilhouetteJaccardAcrossViews"], 0.25)
            self.assertEqual(3, len(row["pairwise"]))

    def test_fixed_view_audit_is_receipt_bound_but_never_claims_native_acceptance(self):
        self.assertEqual("PASS_SOURCE_CANDIDATE_ONLY", self.audit["validationStatus"])
        self.assertEqual("UNADMITTED_VISUAL_REFERENCE_ONLY", self.audit["admissionStatus"])
        self.assertTrue(all(self.audit["validationChecks"].values()))
        self.assertTrue(all(value is False for value in self.audit["claimBoundary"].values()))
        preview = self.audit["previewReceipt"]
        self.assertEqual(2560, preview["widthPixels"])
        self.assertEqual(2320, preview["heightPixels"])
        self.assertEqual(preview["bytes"], PREVIEW_PATH.stat().st_size)
        self.assertEqual(preview["sha256"], sha256(PREVIEW_PATH))
        fixed = self.audit["fixedViewAudit"]
        self.assertEqual(
            ["frontXZ", "sideYZ", "crownTopXY", "rootTrunkXZ", "lodAuthorityLadder"],
            fixed["viewsPerCandidate"],
        )
        self.assertTrue(fixed["actualPinnedSourceVerticesUsed"])
        self.assertFalse(fixed["sourceTrianglesRasterized"])
        self.assertFalse(fixed["nativeUnrealLodsRendered"])
        self.assertFalse(fixed["nativeVisualAcceptanceProvided"])

    def test_check_is_read_only(self):
        watched = [CONTRACT_PATH, MANIFEST_PATH, AUDIT_PATH, PREVIEW_PATH]
        before = {path: (sha256(path), path.stat().st_mtime_ns) for path in watched}
        checked = subprocess.run(
            [sys.executable, "-B", str(BUILDER), "--check"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=45,
            check=False,
        )
        self.assertEqual(0, checked.returncode, checked.stdout + checked.stderr)
        after = {path: (sha256(path), path.stat().st_mtime_ns) for path in watched}
        self.assertEqual(before, after)

    def test_numpy_self_test_passes_when_dependency_is_available(self):
        try:
            import numpy  # noqa: F401
        except ImportError:
            self.skipTest("The nonlinear geometry self-test requires NumPy.")
        internal = subprocess.run(
            [sys.executable, "-B", str(BUILDER), "--self-test"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
        self.assertEqual(0, internal.returncode, internal.stdout + internal.stderr)

    def test_isolated_rebuild_is_byte_deterministic(self):
        try:
            import PIL  # noqa: F401
            import numpy  # noqa: F401
        except ImportError:
            self.skipTest("Isolated contact-sheet rebuild requires Pillow and NumPy.")
        with tempfile.TemporaryDirectory() as temporary:
            rebuilt = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(BUILDER),
                    "--build",
                    "--output-dir",
                    temporary,
                ],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                timeout=60,
                check=False,
            )
            self.assertEqual(0, rebuilt.returncode, rebuilt.stdout + rebuilt.stderr)
            temporary_root = Path(temporary)
            pairs = (
                (CONTRACT_PATH, temporary_root / CONTRACT_PATH.name),
                (MANIFEST_PATH, temporary_root / MANIFEST_PATH.name),
                (AUDIT_PATH, temporary_root / "OfflineAudit" / AUDIT_PATH.name),
                (PREVIEW_PATH, temporary_root / "OfflineAudit" / PREVIEW_PATH.name),
            )
            for committed, generated in pairs:
                self.assertEqual(sha256(committed), sha256(generated), committed.name)

    def test_read_only_check_passes_with_site_packages_disabled(self):
        isolated = subprocess.run(
            [sys.executable, "-I", "-S", "-B", str(BUILDER), "--check"],
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
        self.assertEqual(0, isolated.returncode, isolated.stdout + isolated.stderr)

        launcher = shutil.which("py")
        if launcher is not None:
            system_python = subprocess.run(
                [launcher, "-3", "-I", "-S", "-B", str(BUILDER), "--check"],
                cwd=REPO_ROOT,
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )
            self.assertEqual(
                0,
                system_python.returncode,
                system_python.stdout + system_python.stderr,
            )

    def test_manifest_or_truth_mutation_is_rejected(self):
        sys.path.insert(0, str(CANDIDATE_ROOT))
        try:
            import build_tree_geometry_candidate_v4 as builder
        finally:
            sys.path.pop(0)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            shutil.copytree(CANDIDATE_ROOT, root / "candidate")
            copied = root / "candidate"
            manifest_path = copied / MANIFEST_PATH.name
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            manifest["rows"][0]["variantSelector"] = "D"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "selector roster drifted"):
                builder.verify_semantics(copied)


if __name__ == "__main__":
    unittest.main()
