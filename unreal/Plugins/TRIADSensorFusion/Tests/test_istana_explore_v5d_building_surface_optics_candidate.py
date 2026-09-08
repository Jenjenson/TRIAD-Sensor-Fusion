from __future__ import annotations

from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import unittest


REPO = Path(__file__).absolute().parents[4]
CANDIDATE = (
    REPO
    / "unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings"
    / "BuildingSurfaceOpticsCandidate"
)
CONTRACT_PATH = CANDIDATE / "building_surface_optics_candidate.contract.json"
INTENT_PATH = CANDIDATE / "building_surface_optics.material_intent.hlsl"
BUILDER_PATH = CANDIDATE / "render_building_surface_optics_candidate.py"
AUDIT_PATH = CANDIDATE / "OfflineAudit/building_surface_optics_audit.json"
PREVIEW_PATH = CANDIDATE / "OfflineAudit/building_surface_optics_comparison.png"
README_PATH = CANDIDATE / "OfflineAudit/README.md"
SOURCE_OBJ = (
    REPO
    / "unreal/Generated/IstanaExploreV5D/LocalFallbackSuppressionV2"
    / "SM_IPV5D_OSMCurrentSurroundings_Render_LocalFallbackSuppressed_v2.obj"
)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def reject_duplicate_keys(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(path: Path) -> dict[str, object]:
    return json.loads(
        path.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicate_keys
    )


class BuildingSurfaceOpticsCandidateContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.contract = load_json(CONTRACT_PATH)
        cls.audit = load_json(AUDIT_PATH)
        cls.intent = INTENT_PATH.read_text(encoding="utf-8")
        cls.builder = BUILDER_PATH.read_text(encoding="utf-8")

    def test_source_artifact_roster_exists_and_contains_no_native_or_geometry_output(self) -> None:
        for path in (
            CONTRACT_PATH,
            INTENT_PATH,
            BUILDER_PATH,
            AUDIT_PATH,
            PREVIEW_PATH,
            README_PATH,
        ):
            self.assertTrue(path.is_file(), path)
        forbidden = {".uasset", ".umap", ".dll", ".obj", ".fbx", ".gltf", ".glb"}
        produced = [
            path
            for path in CANDIDATE.rglob("*")
            if path.is_file() and path.suffix.lower() in forbidden
        ]
        self.assertEqual([], produced)

    def test_contract_is_explicitly_unadmitted_and_does_not_claim_hyperreal_truth(self) -> None:
        self.assertEqual(
            "triad.istana_explore_v5d.building_surface_optics_candidate.v1",
            self.contract["schema"],
        )
        self.assertEqual("SOURCE_ONLY_UNADMITTED", self.contract["status"])
        boundary = self.contract["claimBoundary"]
        for key in (
            "unrealAsset",
            "nativeIntegrated",
            "nativeCompiled",
            "nativeCaptured",
            "humanVisualAcceptance",
            "hyperrealClaim",
            "actualSiteMaterialTruth",
            "surveyOrAsBuiltTruth",
            "currentComplete",
        ):
            self.assertFalse(boundary[key], key)
        self.assertTrue(boundary["sourceLookdevOnly"])

    def test_every_contract_input_is_byte_and_hash_pinned(self) -> None:
        roles = set()
        for pin in self.contract["inputs"]:
            self.assertNotIn(pin["role"], roles)
            roles.add(pin["role"])
            path = REPO / pin["file"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(pin["bytes"], path.stat().st_size, pin["role"])
            self.assertEqual(pin["sha256"], sha256(path), pin["role"])
        self.assertEqual(
            {
                "PinnedR31AppearanceDefinition",
                "OfflinePredecessorEmulator",
                "ImmutableBroadShellObj",
                "ImmutableBroadShellMtl",
                "MaterialIntent",
            },
            roles,
        )

    def test_independent_obj_census_preserves_all_geometry_and_slot_counts(self) -> None:
        triangles = 0
        groups: set[str] = set()
        slots: Counter[str] = Counter()
        group = ""
        material = ""
        with SOURCE_OBJ.open("r", encoding="utf-8") as handle:
            for raw in handle:
                if raw.startswith("g "):
                    group = raw[2:].strip()
                    groups.add(group)
                elif raw.startswith("usemtl "):
                    material = raw[7:].strip()
                elif raw.startswith("f "):
                    self.assertTrue(group)
                    self.assertTrue(material)
                    self.assertEqual(3, len(raw.split()) - 1)
                    triangles += 1
                    slots[material] += 1
        immutable = self.contract["immutableGeometry"]
        self.assertEqual(immutable["sourceObjTriangles"], triangles)
        self.assertEqual(43448, triangles)
        self.assertEqual(immutable["retainedSuppressionGroups"], len(groups))
        self.assertEqual(1388, len(groups))
        self.assertEqual(immutable["materialSlots"], len(slots))
        self.assertEqual(17, len(slots))
        self.assertEqual(dict(sorted(slots.items())), self.audit["geometryEvidence"]["materialTriangleCounts"])

    def test_geometry_and_authority_mutations_are_all_denied(self) -> None:
        immutable = self.contract["immutableGeometry"]
        for key in (
            "meshPackageMutation",
            "vertexMutation",
            "topologyMutation",
            "uvMutation",
            "materialSlotRosterOrOrderMutation",
            "footprintMutation",
            "heightMutation",
            "silhouetteMutation",
            "componentTransformMutation",
            "buildingAdditionOrRemoval",
        ):
            self.assertFalse(immutable[key], key)
        self.assertEqual("identity", immutable["componentTransform"])
        authority = self.contract["authority"]
        self.assertTrue(authority["renderOnly"])
        for key, value in authority.items():
            if key != "renderOnly":
                self.assertFalse(value, key)

    def test_material_intent_has_only_bounded_presentation_outputs(self) -> None:
        for output in (
            "CandidateBaseColor",
            "CandidateTangentNormal",
            "CandidateRoughness",
            "CandidateMetallic",
            "CandidateAmbientOcclusion",
            "CandidateClearCoat",
            "CandidateClearCoatRoughness",
        ):
            self.assertIn(f"out float{'' if output not in ('CandidateBaseColor', 'CandidateTangentNormal') else '3'} {output}", self.intent)
        for forbidden in (
            "WorldPositionOffset",
            "PixelDepthOffset",
            "OpacityMask",
            "SV_Position",
            "discard",
            "clip(",
            "buildingCell",
            "floor(WorldPosition",
        ):
            self.assertNotIn(forbidden, self.intent)
        self.assertIn("GlassMask * WallMask", self.intent)
        self.assertIn("(FrameMask + DividerMask) * WallMask", self.intent)
        self.assertIn("float2 R31PaneCellIndex", self.intent)
        self.assertIn(
            "R31PaneCellIndex, float2(37.11, 83.17)",
            self.intent,
        )
        self.assertNotIn("floor(FacadeUvMetres", self.intent)
        self.assertNotIn("FacadeUvMetres.x / 3.0", self.intent)
        self.assertNotIn("FacadeUvMetres.y / 3.2", self.intent)

    def test_intent_constants_match_contract_and_keep_roles_distinct(self) -> None:
        optics = self.contract["opticalResponse"]
        self.assertIn("lerp(0.055, 0.135, PaneSignal)", self.intent)
        self.assertIn("lerp(0.075, 0.165, PaneSignal)", self.intent)
        self.assertIn("lerp(0.27, 0.39, FacadeSignals.z)", self.intent)
        self.assertIn("R31Metallic, 0.54", self.intent)
        self.assertEqual([0.055, 0.16], optics["glazing"]["clearCoatRoughnessRange"])
        self.assertEqual([0.075, 0.2], optics["glazing"]["baseRoughnessRange"])
        self.assertEqual([0.27, 0.39], optics["frames"]["roughnessRange"])
        self.assertEqual(0.54, optics["frames"]["metallic"])
        self.assertEqual([0.37, 2.1, 13.0], optics["walls"]["metricResponseWavelengthsMetres"])
        self.assertEqual([0.23, 1.8, 9.0], optics["roofs"]["metricResponseWavelengthsMetres"])

    def test_pane_signal_uses_exact_r31_aperture_cell_identity(self) -> None:
        glazing = self.contract["opticalResponse"]["glazing"]
        self.assertTrue(glazing["paneIdentityConstantWithinExistingApertureCell"])
        self.assertFalse(glazing["independentFixedFacadeGridUsed"])
        self.assertIn("floor(float2(q_u,q_v))", glazing["paneIdentitySource"])
        self.assertIn(
            'pane_cell_index = np.floor(np.stack((q_u, q_v), axis=1))',
            self.builder,
        )
        self.assertIn('"paneCellIndex": pane_cell_index', self.builder)
        self.assertIn(
            'pane_signal = pane_signal_from_cell(np, pane_cell_index)',
            self.builder,
        )
        self.assertNotIn("np.floor(facade_uv[:, 0] / 3.0)", self.builder)
        self.assertNotIn("np.floor(facade_uv[:, 1] / 3.2)", self.builder)

        pane = self.audit["paneIdentityProbe"]
        thresholds = self.contract["offlineAudit"]
        self.assertTrue(pane["fixedThreeByThreePointTwoMetreFacadeGridRemoved"])
        self.assertTrue(
            pane["noPaneIdentityDiscontinuityInsideExistingApertureCell"]
        )
        self.assertEqual(0.0, pane["maximumWithinCellPaneSignalRange"])
        self.assertGreaterEqual(
            pane["qualifiedExistingApertureCellCount"],
            thresholds["minimumPaneIdentityQualifiedCellCount"],
        )
        self.assertGreaterEqual(
            pane["adjacentCellPairCount"],
            thresholds["minimumAdjacentPaneCellPairCount"],
        )
        self.assertEqual(
            pane["adjacentCellPairCount"],
            pane["adjacentCellPairsWithDifferentSignal"],
        )
        self.assertGreaterEqual(
            pane["minimumAdjacentCellPaneSignalDelta"],
            thresholds["minimumAdjacentPaneSignalDelta"],
        )

    def test_future_application_is_double_gated_after_human_accepted_r33(self) -> None:
        gate = self.contract["futureAdmission"]
        self.assertFalse(gate["allowedNow"])
        self.assertTrue(gate["acceptedR33ReceiptRequired"])
        self.assertEqual("COMMITTED", gate["acceptedR33ReceiptStatus"])
        self.assertTrue(gate["acceptedR33HumanReviewRequired"])
        self.assertTrue(gate["separateExplicitAuthorizationReceiptRequired"])
        self.assertGreaterEqual(len(gate["authorizationReceiptMustHashBind"]), 7)
        required = "\n".join(gate["requiredNativeProof"])
        for phrase in (
            "UE5.5 compile and link",
            "save and cold reload",
            "exact 43,448 triangle and 17 slot revalidation",
            "exact footprint, height, transform and silhouette comparison",
            "Nanite and raster same-camera parity",
            "target-hardware performance",
            "explicit human visual acceptance",
        ):
            self.assertIn(phrase, required)

    def test_audit_has_real_visual_delta_and_ordinary_distance_separation(self) -> None:
        self.assertEqual("PASS_SOURCE_LOOKDEV_ONLY", self.audit["status"])
        self.assertFalse(self.audit["nativeIntegrated"])
        self.assertFalse(self.audit["visualAcceptance"])
        comparison = self.audit["comparisonMetrics"]
        self.assertGreater(comparison["meanAbsoluteChannelDelta255"], 4.5)
        self.assertGreater(comparison["pixelFractionMaximumChannelDeltaAbove4"], 0.29)
        self.assertGreater(comparison["meanNeighbourGradientGainFraction"], 0.10)
        self.assertGreater(comparison["shellLuminanceStandardDeviationGainFraction"], 0.08)
        self.assertGreater(self.audit["renderMetrics"]["candidate"]["finiteDepthPixelFraction"], 0.82)
        rows = self.audit["controlledResponseProbe"]["wallDistances"]
        self.assertEqual([20.0, 50.0, 75.0], [row["distanceMetres"] for row in rows])
        for row in rows:
            self.assertEqual(0.0, row["opaqueBlackGlassFraction"])
            self.assertGreater(row["wallGlassMedianRoughnessSeparation"], 0.56)
            self.assertGreater(row["frameGlassMedianRoughnessSeparation"], 0.25)
            self.assertGreater(row["candidateMeanAbsoluteBaseColorDelta"], 0.020)
            self.assertGreater(row["wallGlassMedianLuminanceSeparationGain"], 0.0)
        explanation = self.audit["controlledResponseProbe"]["distanceResponseExplanation"]
        self.assertEqual(75.0, explanation["fullStrengthThroughMetres"])
        self.assertTrue(explanation["roughnessSeparationIntentionallyInvariantThroughProbeRange"])
        separations = [row["wallGlassMedianRoughnessSeparation"] for row in rows]
        self.assertLess(max(separations) - min(separations), 1e-7)
        roof = self.audit["controlledResponseProbe"]["roof"]
        self.assertGreater(roof["candidateRoughnessStandardDeviation"], 0.035)
        self.assertGreater(
            roof["candidateRoughnessStandardDeviation"],
            7.0 * roof["predecessorRoughnessStandardDeviation"],
        )

    def test_preview_receipt_and_decoded_pixels_are_bound(self) -> None:
        output = self.audit["output"]
        self.assertEqual([2000, 1160], output["pixels"])
        self.assertEqual(PREVIEW_PATH.stat().st_size, output["bytes"])
        self.assertEqual(sha256(PREVIEW_PATH), output["sha256"])
        with PREVIEW_PATH.open("rb") as handle:
            header = handle.read(24)
        self.assertEqual(b"\x89PNG\r\n\x1a\n", header[:8])
        self.assertEqual((2000, 1160), struct.unpack(">II", header[16:24]))
        self.assertEqual(64, len(output["decodedRgbaSha256"]))

    def test_builder_read_only_check_and_formula_self_test_pass(self) -> None:
        environment = os.environ.copy()
        environment["PYTHONDONTWRITEBYTECODE"] = "1"
        for switch, marker in (
            ("--check", "PASS_SOURCE_LOOKDEV_ONLY"),
            ("--self-test", "PASS_SOURCE_FORMULA_SELF_TEST"),
        ):
            completed = subprocess.run(
                [sys.executable, "-B", str(BUILDER_PATH), switch],
                cwd=REPO,
                env=environment,
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(0, completed.returncode, completed.stderr)
            self.assertIn(marker, completed.stdout)

if __name__ == "__main__":
    unittest.main()
