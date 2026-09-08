import fnmatch
import hashlib
import json
import re
import unittest
from decimal import Decimal
from pathlib import Path


UNREAL_ROOT = Path(__file__).absolute().parents[3]
REPO_ROOT = UNREAL_ROOT.parent
PLUGIN_ROOT = UNREAL_ROOT / "Plugins/TRIADSensorFusion"
CANDIDATE_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/Vegetation/"
    "OuterContextVegetationCandidate"
)
INTEGRATION_ROOT = (
    UNREAL_ROOT
    / "SourceAssets/IstanaPublicViewExploreV5D/Vegetation/"
    "OuterContextVegetationIntegration"
)
CONTRACT_PATH = (
    INTEGRATION_ROOT
    / "outer_context_vegetation_post_r33.source_contract.v1.json"
)
README_PATH = INTEGRATION_ROOT / "README.md"
PLACEMENTS_PATH = CANDIDATE_ROOT / "Generated/outer_context_tree_placements.json"
CANDIDATE_CONTRACT_PATH = (
    CANDIDATE_ROOT / "outer_context_vegetation_candidate.contract.json"
)
RUNTIME_HEADER = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Public/"
    "TRIADIstanaExploreV5DOuterContextVegetationActor.h"
)
RUNTIME_SOURCE = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DOuterContextVegetationActor.cpp"
)
PLACEMENT_MIRROR = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/"
    "TRIADIstanaExploreV5DOuterContextVegetationPlacements.inl"
)
RUNTIME_TEST = (
    PLUGIN_ROOT
    / "Source/TRIADSensorFusion/Private/Tests/"
    "TRIADIstanaExploreV5DOuterContextVegetationActorTests.cpp"
)
RELEASE_CONFIG = REPO_ROOT / "distribution/github-free-release-bundles.json"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


def load_decimal_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"), parse_float=Decimal)


class OuterContextVegetationPostR33SourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
        cls.candidate_contract = json.loads(
            CANDIDATE_CONTRACT_PATH.read_text(encoding="utf-8")
        )
        cls.placements = load_decimal_json(PLACEMENTS_PATH)
        cls.header = RUNTIME_HEADER.read_text(encoding="utf-8")
        cls.source = RUNTIME_SOURCE.read_text(encoding="utf-8")
        cls.mirror = PLACEMENT_MIRROR.read_text(encoding="utf-8")
        cls.runtime_test = RUNTIME_TEST.read_text(encoding="utf-8")
        cls.release_config = json.loads(RELEASE_CONFIG.read_text(encoding="utf-8"))

    def test_source_delivery_is_unnumbered_unexecuted_and_fail_closed(self):
        self.assertEqual(
            "triad.istana.outer_context_vegetation."
            "post_r33_source_integration.v1",
            self.contract["schema"],
        )
        self.assertEqual("SOURCE_SCAFFOLD_NOT_EXECUTED", self.contract["status"])
        truth = self.contract["truthBoundary"]
        for key, value in truth.items():
            self.assertFalse(value, key)
        admission = self.contract["admission"]
        self.assertEqual(4, admission["compiledTrustAnchorCount"])
        self.assertFalse(admission["compiledTrustAnchorsPopulated"])
        self.assertFalse(admission["candidateDistributionGatesSatisfied"])
        self.assertFalse(admission["callerValuesCanAuthorizeActivation"])
        self.assertFalse(admission["configurationOrVisibilityReachable"])
        self.assertTrue(admission["futureReviewedSourceChangeRequired"])

    def test_candidate_inputs_are_byte_and_hash_exact(self):
        for row in self.contract["sourcePins"].values():
            path = REPO_ROOT / row["file"]
            self.assertTrue(path.is_file(), path)
            self.assertEqual(row["bytes"], path.stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)
        self.assertEqual(145, self.candidate_contract["exactCensus"]["placements"])
        self.assertFalse(self.candidate_contract["publicDistributionReady"])
        self.assertFalse(self.candidate_contract["nativeProjectApplied"])
        self.assertEqual(145, len(self.placements["placements"]))

    def test_fixed_point_mirror_matches_all_145_source_rows_exactly(self):
        pattern = re.compile(
            r'^TRIAD_OUTER_CONTEXT_VEGETATION_PLACEMENT\('
            r'"([^"]+)",\s*(\d+),\s*(-?\d+)LL,\s*(-?\d+)LL,'
            r"\s*(-?\d+),\s*(\d+)\)$",
            re.MULTILINE,
        )
        actual = [match.groups() for match in pattern.finditer(self.mirror)]
        form_base = {
            "umbrella": 0,
            "dome": 3,
            "highForkRounded": 6,
            "columnar": 9,
        }
        expected = []
        for row in self.placements["placements"]:
            expected.append(
                (
                    row["placementKey"],
                    str(
                        form_base[row["formProxy"]]
                        + "ABC".index(row["variantSelector"])
                    ),
                    str(int(row["xEastMeters"] * Decimal(1_000_000))),
                    str(int(row["ySouthMeters"] * Decimal(1_000_000))),
                    str(int(row["yawDegrees"] * Decimal(1_000))),
                    str(int(row["uniformScale"] * Decimal(1_000_000))),
                )
            )
        self.assertEqual(145, len(actual))
        self.assertEqual(expected, actual)
        self.assertEqual(145, len({row[0] for row in actual}))
        self.assertEqual(list(range(12)), sorted({int(row[1]) for row in actual}))

    def test_public_surface_is_inspection_and_pure_layout_only(self):
        public_text, private_text = self.header.split("private:", 1)
        self.assertNotIn("ConfigureOuterContextVegetation", public_text)
        self.assertNotIn("ApplyPresentationState", public_text)
        self.assertIn("ConfigureOuterContextVegetation", private_text)
        self.assertIn("ApplyPresentationState", private_text)
        self.assertEqual(0, self.header.count("UFUNCTION(BlueprintCallable"))
        self.assertEqual(2, self.header.count("UFUNCTION(BlueprintPure"))
        self.assertIn("BuildDeterministicLayout", public_text)
        self.assertIn("ValidateAssetRoster", public_text)
        self.assertNotIn("friend ", self.header)

    def test_compiled_admission_and_distribution_gates_are_unset(self):
        for sentinel in (
            "UNSET_ACCEPTED_R33_RECEIPT_SHA256",
            "UNSET_FUTURE_ASSETS_ONLY_AUTHORIZATION_RECEIPT_SHA256",
            "UNSET_TERRAIN_CONTACT_RECEIPT_SHA256",
            "UNSET_PUBLIC_DISTRIBUTION_RECEIPT_SHA256",
        ):
            self.assertIn(sentinel, self.source)
        self.assertIn(
            "constexpr bool bCompiledCandidateDistributionGatesSatisfied = false;",
            self.source,
        )
        self.assertIn("!CompiledTrustAnchorsConfigured()", self.source)
        self.assertIn(
            "!CandidateDistributionGatesDeclaredSatisfied()", self.source
        )
        self.assertEqual(0, self.source.count("SpawnActor"))
        self.assertEqual(0, self.source.count("GetWorld()"))
        self.assertEqual(0, self.source.count("LineTrace"))

    def test_render_owner_is_exactly_twelve_hism_buckets_and_non_authoritative(self):
        implementation = self.contract["implementation"]
        self.assertEqual(12, implementation["hismBuckets"])
        self.assertEqual(4, implementation["broadleafForms"])
        self.assertEqual(3, implementation["variantsPerForm"])
        self.assertEqual(145, implementation["placements"])
        for token in (
            "UHierarchicalInstancedStaticMeshComponent",
            "SetCollisionEnabled(ECollisionEnabled::NoCollision)",
            "SetCollisionResponseToAllChannels(ECR_Ignore)",
            "SetGenerateOverlapEvents(false)",
            "SetCanEverAffectNavigation(false)",
            "SetVisibility(false, true)",
            "SetHiddenInGame(true, true)",
            "bCollisionNavigationLosRfSensorTerrainOrGeospatialAuthority = false",
            "bMapBindingImplemented = false",
            "bNativeVisualAcceptanceProvided = false",
        ):
            self.assertIn(token, self.header + self.source)

    def test_native_hism_readback_has_narrow_separate_tolerances(self):
        for token in (
            "NativeInstanceTranslationToleranceCm = 0.02",
            "NativeInstanceRotationTolerance = 0.00001",
            "NativeInstanceScaleTolerance = 0.00001",
            "NativeInstanceTransformValue(Actual, ExpectedTransforms[Index])",
            "NativeInstanceTransformMatches",
            "Probe->AddInstance(ExpectedRoundTrip, false)",
            "Probe->GetInstanceTransform(0, ActualRoundTrip, false)",
            "Meaningful translation drift is rejected",
            "Meaningful scale drift is rejected",
        ):
            self.assertIn(token, self.header + self.source + self.runtime_test)
        exact_layout_validation = self.source.split(
            "bool ValidateLayoutInternal", 1
        )[1].split("bool ComponentMatches", 1)[0]
        self.assertIn("ExactTransformValue", exact_layout_validation)
        component_validation = self.source.split("bool ComponentMatches", 1)[1]
        component_validation = component_validation.split("} // namespace", 1)[0]
        self.assertNotIn("ExactTransformValue", component_validation)

    def test_external_z_is_required_without_sampling_or_xy_override(self):
        for token in (
            "TOptional<double> ZMeters",
            "Contact.PlacementKey != Expected.PlacementKey",
            "!Contact.ZMeters.IsSet()",
            "!IsFiniteTerrainContactZ(Contact.ZMeters.GetValue())",
            "Expected.XEastMicrometers",
            "Expected.YSouthMicrometers",
            "Expected.YawMillidegrees",
            "Expected.UniformScaleMillionths",
        ):
            self.assertIn(token, self.header + self.source)
        for forbidden in (
            "SetActorLocation",
            "SetActorRotation",
            "SetActorTransform",
            "Landscape",
            "SampleHeight",
            "GetHeight",
            "LineTraceSingle",
        ):
            self.assertNotIn(forbidden, self.source)

    def test_provider_state_policy_prevents_google_double_vegetation(self):
        state_body = self.source.split("CanRenderInPresentationState", 1)[1]
        state_body = state_body.split("CompiledTrustAnchorsConfigured", 1)[0]
        self.assertIn("CwtPresented", state_body)
        self.assertIn("SafeLocal", state_body)
        self.assertNotIn("GooglePrimary", state_body)
        self.assertNotIn("CwtWarming", state_body)
        state_policy = self.contract["providerStatePolicy"]
        self.assertFalse(state_policy["googlePrimaryVisible"])
        self.assertFalse(state_policy["cwtWarmingVisible"])
        self.assertTrue(state_policy["cwtPresentedFutureEligible"])
        self.assertTrue(state_policy["safeLocalFutureEligible"])

    def test_unready_candidate_is_excluded_from_release_bundles(self):
        bundle = next(
            row
            for row in self.release_config["bundles"]
            if row["name"] == "source-assets"
        )
        required = {
            "SourceAssets/IstanaPublicViewExploreV5D/Vegetation/"
            "OuterContextVegetationCandidate/*",
            "SourceAssets/IstanaPublicViewExploreV5D/Vegetation/"
            "OuterContextVegetationIntegration/*",
        }
        self.assertTrue(required.issubset(set(bundle["excludes"])))
        for root in (CANDIDATE_ROOT, INTEGRATION_ROOT):
            for path in root.rglob("*"):
                if not path.is_file():
                    continue
                relative = path.relative_to(UNREAL_ROOT).as_posix()
                self.assertTrue(
                    any(
                        fnmatch.fnmatchcase(relative, pattern)
                        for pattern in bundle["excludes"]
                    ),
                    relative,
                )

    def test_artifact_roster_and_hashes_are_exact(self):
        expected_paths = {
            path.relative_to(REPO_ROOT).as_posix()
            for path in (
                RUNTIME_HEADER,
                RUNTIME_SOURCE,
                PLACEMENT_MIRROR,
                RUNTIME_TEST,
                README_PATH,
                Path(__file__).absolute(),
                RELEASE_CONFIG,
            )
        }
        rows = self.contract["implementation"]["artifacts"]
        self.assertEqual(expected_paths, {row["file"] for row in rows})
        for row in rows:
            path = REPO_ROOT / row["file"]
            self.assertEqual(row["bytes"], path.stat().st_size, path)
            self.assertEqual(row["sha256"], sha256(path), path)

    def test_scaffold_is_balanced_and_remaining_native_proof_is_explicit(self):
        for path, text in (
            (RUNTIME_HEADER, self.header),
            (RUNTIME_SOURCE, self.source),
            (RUNTIME_TEST, self.runtime_test),
        ):
            self.assertEqual(text.count("{"), text.count("}"), path)
            self.assertEqual(text.count("#if"), text.count("#endif"), path)
        remaining = set(self.contract["remainingProof"])
        for item in (
            "UE55_UBT_UHT_COMPILE_AND_LINK",
            "ACCEPTED_R33_NATIVE_CAPTURE_RECEIPT",
            "EXTERNALLY_VALIDATED_TERRAIN_CONTACT_Z",
            "OWNED_RUNTIME_VISIBLE_OSM_ATTRIBUTION_PRESENTER",
            "MAP_AND_PIE_INTEGRATION",
            "MATCHED_NATIVE_CAPTURES",
            "TEMPORAL_AND_PERFORMANCE_ACCEPTANCE",
            "EXPLICIT_HUMAN_VISUAL_ACCEPTANCE",
        ):
            self.assertIn(item, remaining)


if __name__ == "__main__":
    unittest.main()
