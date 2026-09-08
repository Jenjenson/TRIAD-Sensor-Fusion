from hashlib import sha256
import json
from pathlib import Path
import unittest


REPO = Path(__file__).resolve().parents[4]
SOURCE_ROOT = (
    REPO
    / "unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R31BroadShellLookdev"
)
CONTRACT = SOURCE_ROOT / "r31_broad_shell_lookdev.contract.json"
README = SOURCE_ROOT / "README.md"


def file_sha256(path: Path) -> str:
    digest = sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest().upper()


class IstanaExploreV5DR31BroadShellContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))
        cls.readme = README.read_text(encoding="utf-8")

    def test_exact_v2_geometry_identity_is_retained(self):
        mesh = self.contract["sourceMesh"]
        self.assertEqual(
            self.contract["schema"],
            "triad.istana_explore_v5d.r31_broad_shell_lookdev.v1",
        )
        self.assertEqual(self.contract["revision"], "R31")
        source_obj = mesh["sourceObj"]
        imported = mesh["importedAsset"]
        self.assertEqual(source_obj["vertexRecordCount"], 24_522)
        self.assertEqual(source_obj["textureCoordinateRecordCount"], 130_632)
        self.assertEqual(source_obj["faceCount"], 43_448)
        self.assertEqual(imported["triangleCount"], 43_448)
        self.assertEqual(imported["materialSlotCount"], 17)
        self.assertEqual(imported["renderVertexCount"], 24_468)
        self.assertEqual(imported["meshDescriptionVertexInstanceCount"], 130_344)
        self.assertEqual(mesh["uv0Channels"], 1)
        self.assertTrue(mesh["fullPrecisionUv"])
        self.assertFalse(mesh["generatedLightmapUv"])
        self.assertEqual(mesh["canonicalSourceGroupCount"], 1_391)
        self.assertEqual(mesh["retainedSuppressionV2GroupCount"], 1_388)
        self.assertTrue(mesh["groupIdentifiersAndOrderPreserved"])
        self.assertEqual(mesh["relativeTransform"], "identity")
        self.assertEqual(mesh["naniteKeepPercentTriangles"], 1.0)
        self.assertEqual(mesh["naniteFallbackPercentTriangles"], 1.0)
        self.assertFalse(mesh["collision"])
        self.assertFalse(mesh["navigation"])

    def test_exact_v2_and_texture_sources_are_hash_admitted(self):
        pins = [self.contract["sourceMesh"]["sourceObj"]]
        pins.extend(self.contract["sourcePins"])
        pins.extend(self.contract["texturePins"])
        self.assertEqual(len(self.contract["sourcePins"]), 5)
        self.assertEqual(len(self.contract["texturePins"]), 9)
        for pin in pins:
            path = REPO / pin["file"]
            self.assertTrue(path.is_file(), pin["file"])
            self.assertEqual(path.stat().st_size, pin["bytes"], pin["file"])
            self.assertEqual(file_sha256(path), pin["sha256"], pin["file"])

    def test_native_source_closure_is_exact_pre_r33_and_hash_admitted(self):
        closure = self.contract["nativeSourceClosure"]
        self.assertEqual(
            closure["schema"],
            "triad.istana_explore_v5d.r31_context_policy_source_closure.v1",
        )
        self.assertEqual(closure["version"], "R31-pre-R33")
        self.assertTrue(closure["immutable"])
        self.assertTrue(closure["repositoryAndNativePathsAreExplicit"])
        self.assertTrue(closure["samePathFallbackRetainedForOtherPins"])
        self.assertFalse(closure["r33SourceOrDeclarationAllowed"])
        expected = (
            (
                "ContextPolicyPublicHeader",
                "unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R31BroadShellLookdev/NativeSourceClosure/TRIADIstanaExploreV5DContextPolicyActor.h",
                "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Public/TRIADIstanaExploreV5DContextPolicyActor.h",
                10_649,
                "9114F728E337523DF3AD0FE5021685683C41BB048CB0049713C2ECC9342D962B",
            ),
            (
                "ContextPolicyPrivateSource",
                "unreal/SourceAssets/IstanaPublicViewExploreV5D/Surroundings/R31BroadShellLookdev/NativeSourceClosure/TRIADIstanaExploreV5DContextPolicyActor.cpp",
                "Plugins/TRIADSensorFusion/Source/TRIADSensorFusion/Private/TRIADIstanaExploreV5DContextPolicyActor.cpp",
                134_398,
                "58074AC6449BD6FBF7D86DDB876B9CD3B7BEFF03162DAA9EFE10C0A08E0DF4C4",
            ),
        )
        self.assertEqual(len(closure["files"]), 2)
        for row, expected_row in zip(closure["files"], expected, strict=True):
            role, repository_file, native_path, size, digest = expected_row
            self.assertEqual(row["role"], role)
            self.assertEqual(row["repositoryFile"], repository_file)
            self.assertEqual(row["nativeRelativePath"], native_path)
            self.assertEqual(row["bytes"], size)
            self.assertEqual(row["sha256"], digest)
            path = REPO / repository_file
            self.assertTrue(path.is_file(), repository_file)
            self.assertEqual(path.stat().st_size, size, repository_file)
            self.assertEqual(file_sha256(path), digest, repository_file)
            text = path.read_text(encoding="utf-8")
            for forbidden in (
                "TRIADIstanaExploreV5DR33",
                "RegisterR33CesiumWorldTerrainController",
                "CesiumWorldTerrainReferenceActor",
            ):
                self.assertNotIn(forbidden, text, (repository_file, forbidden))

    def test_slot_roster_comes_from_exact_imported_v2_order(self):
        expected = [
            ("MAT_BOTTOM_HIDDEN", "FallbackRoof", 9436),
            ("MAT_COMMERCIAL_HINT", "FallbackWall", 2166),
            ("MAT_GENERIC_BUILDING_HINT", "FallbackWall", 14696),
            ("MAT_HEALTHCARE_HINT", "FallbackWall", 278),
            ("MAT_HOTEL_HINT", "OfficialWall", 414),
            ("MAT_INDUSTRIAL_HINT", "FallbackWall", 8),
            ("MAT_RELIGIOUS_HINT", "OfficialWall", 150),
            ("MAT_RESIDENTIAL_HINT", "FallbackWall", 6316),
            ("MAT_ROOF_COMMERCIAL_HINT", "FallbackRoof", 1029),
            ("MAT_ROOF_GENERIC_BUILDING_HINT", "FallbackRoof", 5452),
            ("MAT_ROOF_HEALTHCARE_HINT", "FallbackRoof", 131),
            ("MAT_ROOF_HOTEL_HINT", "OfficialRoof", 169),
            ("MAT_ROOF_INDUSTRIAL_HINT", "FallbackRoof", 2),
            ("MAT_ROOF_RELIGIOUS_HINT", "OfficialRoof", 53),
            ("MAT_ROOF_RESIDENTIAL_HINT", "FallbackRoof", 2684),
            ("MAT_ROOF_TRANSPORT_HINT", "FallbackRoof", 132),
            ("MAT_TRANSPORT_HINT", "FallbackWall", 332),
        ]
        actual = [tuple(row) for row in self.contract["semanticSlotRoles"]]
        self.assertEqual(actual, expected)
        self.assertEqual(sum(row[2] for row in actual), 43_448)
        roles = [row[1] for row in actual]
        self.assertEqual(roles.count("OfficialWall"), 2)
        self.assertEqual(roles.count("OfficialRoof"), 2)
        self.assertEqual(roles.count("FallbackWall"), 6)
        self.assertEqual(roles.count("FallbackRoof"), 7)

    def test_four_instances_use_metric_wrap_and_role_gated_vertical_cues(self):
        package = self.contract["assetPackage"]
        instances = package["instances"]
        self.assertEqual(
            package["root"],
            "/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsShellLookdevR31",
        )
        self.assertEqual(len(instances), 4)
        self.assertEqual(
            [row["role"] for row in instances],
            ["OfficialWall", "OfficialRoof", "FallbackWall", "FallbackRoof"],
        )
        for instance in instances:
            expected_mask = 1.0 if instance["role"].endswith("Wall") else 0.0
            self.assertEqual(instance["wallVerticalWeatherMask"], expected_mask)
            self.assertGreater(instance["metresPerTile"], 0.0)
        uv = self.contract["uvSemantics"]
        self.assertEqual(uv["unrealImport"], "UV0.y = 1 - source OBJ V")
        self.assertIn("sourceAbsoluteZMetres = 1 - UV0.y", uv["shaderRecovery"])
        self.assertFalse(uv["buildingLocalAboveGradeCoordinateAvailable"])
        self.assertFalse(uv["groundContactOrPlinthFromUvAllowed"])
        self.assertIn("reciprocal metres-per-tile", uv["textureTiling"])
        self.assertIn("seamless wrap", uv["textureTiling"])

    def test_instance_specs_have_exact_complete_22_scalar_3_vector_roster(self):
        scalar_fields = (
            "metresPerTile",
            "textureInfluence",
            "normalStrength",
            "surfaceRoughness",
            "roughnessTextureWeight",
            "metallic",
            "specular",
            "aoTextureWeight",
            "variationCellMeters",
            "weatheringStrength",
            "wallVerticalWeatherMask",
            "bayMeters",
            "storeyMeters",
            "apertureWidthFraction",
            "apertureHeightFraction",
            "apertureSillFraction",
            "apertureHintStrength",
            "apertureFadeStartCm",
            "apertureFadeEndCm",
            "atmosphereStartCm",
            "atmosphereEndCm",
            "atmosphereStrength",
        )
        vector_fields = ("tint", "apertureTint", "atmosphereTint")
        expected_fields = {"role", "name", "textureSet", *scalar_fields, *vector_fields}
        expected_scalars = {
            "OfficialWall": (1.8, 0.78, 0.48, 0.68, 0.72, 0.0, 0.29, 0.72, 46.0, 0.24, 1.0, 3.0, 3.2, 0.62, 0.5, 0.18, 0.72, 55000.0, 100000.0, 60000.0, 130000.0, 0.22),
            "OfficialRoof": (1.3, 0.9, 0.62, 0.76, 0.82, 0.0, 0.24, 0.8, 52.0, 0.2, 0.0, 3.0, 3.2, 0.62, 0.5, 0.18, 0.0, 55000.0, 100000.0, 60000.0, 130000.0, 0.18),
            "FallbackWall": (2.2, 0.72, 0.42, 0.72, 0.68, 0.0, 0.27, 0.68, 48.0, 0.3, 1.0, 3.15, 3.15, 0.6, 0.48, 0.2, 0.68, 55000.0, 95000.0, 55000.0, 125000.0, 0.25),
            "FallbackRoof": (1.6, 0.82, 0.5, 0.82, 0.76, 0.0, 0.22, 0.74, 54.0, 0.27, 0.0, 3.15, 3.15, 0.6, 0.48, 0.2, 0.0, 55000.0, 95000.0, 55000.0, 125000.0, 0.2),
        }
        for instance in self.contract["assetPackage"]["instances"]:
            self.assertEqual(set(instance), expected_fields, instance["role"])
            self.assertEqual(
                tuple(instance[name] for name in scalar_fields),
                expected_scalars[instance["role"]],
            )
            for name in vector_fields:
                self.assertEqual(len(instance[name]), 4, (instance["role"], name))

    def test_depth_cues_and_negative_authority_are_explicit(self):
        self.assertEqual(
            set(self.contract["retainedR25DepthCues"]),
            {
                "frameAndInsetGlass",
                "mullionAndTransom",
                "reveal",
                "perCellOccupancyAndTone",
                "glassFresnel",
                "spandrelPierAndSlabEdgeRelief",
                "spandrelPierAndSlabEdgeGeometry",
                "plinthContact",
                "atmosphericDistance",
                "wallRoleGated",
                "geometryDisplacement",
            },
        )
        cues = self.contract["retainedR25DepthCues"]
        self.assertTrue(
            all(
                cues[name]
                for name in cues
                if name
                not in (
                    "geometryDisplacement",
                    "plinthContact",
                    "spandrelPierAndSlabEdgeGeometry",
                )
            )
        )
        self.assertTrue(cues["spandrelPierAndSlabEdgeRelief"])
        self.assertFalse(cues["spandrelPierAndSlabEdgeGeometry"])
        self.assertFalse(cues["geometryDisplacement"])
        self.assertFalse(cues["plinthContact"])
        runtime = self.contract["runtimePolicy"]
        for name in (
            "meshPackageMutation",
            "topologyMutation",
            "transformMutation",
            "geographyMutation",
            "cesiumMutation",
            "vegetationMutation",
            "terrainMutation",
            "collisionAuthority",
            "navigationAuthority",
            "simulationAuthority",
            "sensorAuthority",
            "rfAuthority",
        ):
            self.assertFalse(runtime[name], name)
        authority = self.contract["authority"]
        for name in (
            "survey",
            "asBuilt",
            "currentComplete",
            "physicalMaterialTruth",
            "hyperrealClaim",
            "sensorPropagation",
            "rfMaterial",
            "rfOcclusion",
            "visualAcceptance",
        ):
            self.assertFalse(authority[name], name)
        self.assertTrue(authority["nativeCaptureRequired"])

    def test_building_scale_facade_variation_is_deterministic_and_geometry_safe(self):
        variation = self.contract["appearanceVariation"]
        self.assertEqual(variation["presentationRange"], "mid/far context only")
        self.assertEqual(variation["facadeFamilyCount"], 4)
        self.assertEqual(variation["familyBayScales"], [0.82, 1.02, 1.18, 0.92])
        self.assertEqual(variation["familyStoreyScales"], [0.96, 1.06, 0.92, 1.02])
        self.assertEqual(
            variation["familyApertureWidthScales"], [0.86, 1.04, 1.16, 0.94]
        )
        self.assertEqual(
            variation["familyApertureHeightScales"], [1.05, 0.91, 1.12, 0.97]
        )
        self.assertEqual(
            variation["familyColourMultipliers"],
            {
                "red": [0.96, 1.04, 1.02, 0.98],
                "green": [0.99, 1.01, 0.97, 1.03],
                "blue": [1.04, 0.96, 0.94, 1.01],
            },
        )
        rhythm = variation["architecturalRhythm"]
        self.assertEqual(
            rhythm["familySpandrelStrengths"], [0.34, 0.16, 0.52, 0.26]
        )
        self.assertEqual(
            rhythm["familyPierStrengths"], [0.18, 0.42, 0.22, 0.56]
        )
        self.assertEqual(
            rhythm["familySlabStrengths"], [0.1, 0.3, 0.58, 0.18]
        )
        for field in (
            "continuousTwoStoreyWave",
            "antialiasedFacadeEdgeMasks",
            "wallRoleGated",
            "distanceFadeGated",
            "microReadabilityGated",
            "renderOnlyDepthImpression",
            "sourceGrammarProbeRequired",
        ):
            self.assertTrue(rhythm[field], field)
        for field in (
            "hardFloorParitySelectorUsed",
            "geometryOrSilhouetteChanged",
            "balconyGeometryClaimed",
            "exactPerBuildingStyleClaimed",
        ):
            self.assertFalse(rhythm[field], field)
        self.assertEqual(rhythm["minimumPairwiseFamilySignatureDistance"], 0.05)
        self.assertEqual(rhythm["maximumPhaseWrapCueDelta"], 0.001)
        identity = variation["identitySource"]
        self.assertEqual(
            identity["method"],
            "continuous canonical-normal and signed-plane-distance signals with normalized four-archetype blending",
        )
        self.assertFalse(identity["arrayPositionUsed"])
        self.assertFalse(identity["shortRepeatingCycleUsed"])
        self.assertFalse(identity["exactOsmGroupIdVisibleToPixelShader"])
        self.assertFalse(identity["worldXyGridCellUsed"])
        self.assertFalse(identity["hardNormalQuantizationUsed"])
        self.assertFalse(identity["hardPlaneDistanceQuantizationUsed"])
        self.assertFalse(identity["hardFacadeFamilyThresholdUsed"])
        self.assertTrue(identity["normalizedFourArchetypeBlend"])
        self.assertIn("material-only", identity["reason"])
        cadence = variation["cadencePhase"]
        self.assertTrue(cadence["vertexTangentWorldPlaneFacadeU"])
        self.assertTrue(cadence["vertexTangentDominantAxisWorldCoordinate"])
        self.assertTrue(cadence["dominantComponentQuantizedMetricScaleRestoration"])
        self.assertFalse(cadence["orientationDependentFacadeStretchIntroduced"])
        self.assertFalse(cadence["sourcePerSegmentUUsedForWallCadence"])
        self.assertFalse(cadence["coplanarSegmentResetSeamsAllowed"])
        glazing = variation["glazing"]
        self.assertEqual(
            glazing["facadePlaneSignalOccupancyThresholdRange"], [0.13, 0.37]
        )
        self.assertEqual(glazing["occupancyThresholdTransitionHalfWidth"], 0.07)
        self.assertEqual(glazing["unoccupiedCueStrength"], 0.38)
        self.assertEqual(glazing["occupiedCueStrength"], 1.0)
        self.assertEqual(glazing["blindThresholdCentre"], 0.84)
        self.assertEqual(glazing["blindThresholdTransitionHalfWidth"], 0.05)
        self.assertTrue(glazing["fresnelRetained"])
        self.assertFalse(glazing["emissive"])
        weathering = variation["weathering"]
        for field in (
            "wallRunoff",
            "roofMottling",
            "facadePlaneSignalRoughnessVariation",
            "restrained",
        ):
            self.assertTrue(weathering[field], field)
        self.assertFalse(weathering["wallVerticalDirt"])
        self.assertFalse(weathering["plinthContactDarkening"])
        self.assertFalse(weathering["buildingLocalGradeVisibleToPixelShader"])
        interior = variation["shallowInteriorCue"]
        self.assertEqual(interior["nominalDepthMetres"], 0.115)
        self.assertTrue(interior["viewShiftedInFacadeUvBasis"])
        for field in (
            "geometryDisplacement",
            "worldPositionOffset",
            "pixelDepthOffset",
            "collisionOrOcclusionAuthority",
        ):
            self.assertFalse(interior[field], field)

    def test_high_occupancy_native_oblique_has_explicit_human_rejection_criteria(self):
        criterion = self.contract["nativeCaptureGate"]["humanVisualReviewCriteria"][
            "highOccupancySurroundingBuildingOblique"
        ]
        self.assertEqual(criterion["poseId"], "surroundings_oblique_macdonald")
        self.assertTrue(criterion["required"])
        self.assertGreaterEqual(len(criterion["reviewMustReject"]), 12)
        rejection_text = " ".join(criterion["reviewMustReject"])
        for phrase in (
            "pasted-on stripes",
            "patchwork",
            "aliasing",
            "hard odd/even-storey selector",
        ):
            self.assertIn(phrase, rejection_text)
        self.assertFalse(criterion["automaticVisualAcceptanceAllowed"])
        self.assertFalse(criterion["sourceOrOfflineProbeMayAuthorizeAcceptance"])
        self.assertEqual(
            criterion["reference"], "docs/ISTANA_VISUAL_FIDELITY_REFERENCE.md"
        )

    def test_cross_render_path_parity_is_not_claimed_and_requires_native_pair_review(self):
        seam = self.contract["seamSafety"]
        self.assertTrue(seam["sourceProbeProvesWithinRenderPathContinuity"])
        self.assertFalse(seam["sourceProbeProvesNaniteRasterAppearanceParity"])
        self.assertTrue(seam["nativeNaniteRasterHighOccupancyPairRequired"])
        self.assertTrue(
            seam["nativeReviewMustRejectNaniteRasterCadenceFamilyTextureOrNormalPop"]
        )
        capture = self.contract["nativeCaptureGate"]
        self.assertEqual(
            capture["schema"], "triad.istana_explore_v5d.r31_player0_capture.v2"
        )
        self.assertEqual(capture["exactPoseCount"], 5)
        self.assertEqual(capture["exactImageCount"], 6)
        self.assertEqual(
            capture["acceptanceMode"]["naniteRasterPairReviewConfirmationSwitch"],
            "ConfirmNaniteRasterPairReviewedAndAccepted",
        )
        self.assertTrue(
            capture["pendingReceipt"]["naniteRasterHighOccupancyComparisonRequired"]
        )
        self.assertFalse(
            capture["pendingReceipt"]["humanNaniteRasterComparisonAttested"]
        )
        self.assertFalse(
            capture["pendingReceipt"]["naniteRasterAppearanceParityAccepted"]
        )
        comparison = capture["humanVisualReviewCriteria"][
            "naniteRasterHighOccupancyComparison"
        ]
        self.assertEqual(comparison["poseId"], "surroundings_oblique_macdonald")
        self.assertTrue(comparison["sameCameraPoseAndGameRunRequired"])
        self.assertTrue(comparison["actualConsoleReadbackAndStableWindowRequired"])
        self.assertEqual(
            comparison["naniteConsoleValues"],
            {"naniteOn": 1, "rasterFallback": 0, "restoreAfterCapture": 1},
        )
        self.assertEqual(
            comparison["naniteProxyRenderModeValues"],
            {"naniteOn": 1, "rasterFallback": 0, "restoreAfterCapture": 1},
        )
        self.assertTrue(
            comparison[
                "unsupportedNaniteMustHideInsteadOfSilentlySubstitutingProxy"
            ]
        )
        self.assertTrue(
            comparison["baselineHighOccupancyShellVisibilityHumanCheckRequired"]
        )
        self.assertGreaterEqual(len(comparison["reviewMustReject"]), 5)
        self.assertFalse(comparison["automaticVisualAcceptanceAllowed"])
        self.assertFalse(comparison["sourceOrOfflineProbeMayAuthorizeAcceptance"])
        self.assertTrue(
            capture["image"]["baselineFiveDistinctFileAndDecodedPixelIdentityRequired"]
        )
        self.assertTrue(capture["image"]["naniteRasterPairDistinctFilesRequired"])
        self.assertFalse(
            capture["image"]["naniteRasterPairDecodedPixelDifferenceRequired"]
        )
        self.assertIn(
            "HumanNaniteRasterComparisonAttested",
            capture["downstreamReceiptFields"],
        )
        self.assertIn(
            "NaniteRasterAppearanceParityAccepted",
            capture["downstreamReceiptFields"],
        )

    def test_texture_asset_binding_is_practical_and_does_not_overclaim_source_id(self):
        binding = self.contract["textureAssetBinding"]
        self.assertTrue(binding["sourceFileSha256Pinned"])
        self.assertTrue(binding["singleSourceImportPathRequired"])
        self.assertTrue(binding["storedSourceMd5MustMatchCurrentFile"])
        self.assertTrue(binding["sourceIdRequiredNonEmpty"])
        self.assertFalse(binding["sourceIdClaimedAsPayloadDigest"])
        self.assertFalse(binding["embeddedPixelByteEqualityClaim"])
        self.assertTrue(binding["nativeTextureTreeImmutableDuringTransaction"])
        self.assertIn("one layer", binding["sourceShape"])
        self.assertIn("TSF_BGRA8", binding["sourceShape"])

    def test_native_order_and_fixed_memory_envelope_are_exact(self):
        native = self.contract["nativeTransaction"]
        self.assertEqual(
            native["promotionOrder"],
            "R30 commit and capture first; R31 successor commit and capture second",
        )
        self.assertEqual(native["preflightFreeVirtualGiB"], 10)
        self.assertEqual(native["buildFreeVirtualGiB"], 6)
        self.assertEqual(native["ownedProcessPrivateMemoryCeilingGiB"], 12)
        self.assertEqual(native["contractReferencedFileCount"], 15)
        self.assertEqual(native["promotedContractReferencedFileCount"], 4)
        self.assertEqual(native["retainedContractReferencedFileCount"], 11)
        self.assertEqual(
            native["unrealBuildToolArguments"],
            ["-NoUBA", "-NoUBALocal", "-MaxParallelActions=1"],
        )
        self.assertFalse(native["parallelBuild"])

    def test_readme_is_truthful_about_scope_and_native_acceptance(self):
        for text in (
            "material-only successor",
            "43,448 triangles",
            "sourceAbsoluteZMetres = 1 - UV0.y",
            "WallVerticalWeatherMask",
            "Four continuously blended facade archetypes",
            "VertexTangentWS",
            "0.002 m",
            "0.115 m",
            "mid/far Singapore context only",
            "high-occupancy surrounding-building review",
            "not establish survey",
            "Native visual acceptance remains false",
            "first commit and capture R30",
        ):
            self.assertIn(text, self.readme)


if __name__ == "__main__":
    unittest.main()
