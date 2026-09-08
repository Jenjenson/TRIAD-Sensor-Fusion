from __future__ import annotations

import json
import math
import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
PLUGIN = REPO / "unreal" / "Plugins" / "TRIADSensorFusion"
ACTOR_H = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Public"
    / "TRIADIstanaExploreV5DLandmarkVegetationActor.h"
)
ACTOR_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusion"
    / "Private"
    / "TRIADIstanaExploreV5DLandmarkVegetationActor.cpp"
)
EDITOR_H = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Public"
    / "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h"
)
EDITOR_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp"
)
GROUND_EDITOR_H = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Public"
    / "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.h"
)
GROUND_EDITOR_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5DGroundVegetationEditorLibrary.cpp"
)
HYBRID_H = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Public"
    / "TRIADIstanaExploreV5DHybridEditorLibrary.h"
)
HYBRID_CPP = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5DHybridEditorLibrary.cpp"
)
TEMASEK_FACTORY = (
    PLUGIN
    / "Source"
    / "TRIADSensorFusionEditor"
    / "Private"
    / "TRIADIstanaExploreV5DTemasekShophouseAssetFactory.cpp"
)
SOURCE_ROOT = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Surroundings"
)
LEGACY_TEMASEK_MANIFEST = (
    SOURCE_ROOT
    / "R24TemasekShophouse"
    / "Generated"
    / "IstanaPublicViewV5DR24TemasekShophouse.manifest.json"
)
LEGACY_TEMASEK_MTL = (
    SOURCE_ROOT
    / "R24TemasekShophouse"
    / "Generated"
    / "SM_IPV5D_R24_TemasekShophouse_Render.mtl"
)
MACDONALD_MTL = (
    SOURCE_ROOT
    / "R24MacDonaldHouse"
    / "Generated"
    / "SM_IPV5D_R24_MacDonaldHouse_Render.mtl"
)
GRASS_MANIFEST = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5B"
    / "Grass"
    / "Generated"
    / "bermuda_turf_cluster.manifest.json"
)


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


def constexpr_number(text: str, name: str) -> float:
    match = re.search(
        rf"constexpr (?:double|float|int32) {re.escape(name)} = ([0-9.]+)",
        text,
    )
    if match is None:
        raise AssertionError(f"missing constexpr {name}")
    return float(match.group(1))


class LandmarkVegetationR27IntegrationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        paths = (
            ACTOR_H,
            ACTOR_CPP,
            EDITOR_H,
            EDITOR_CPP,
            GROUND_EDITOR_H,
            GROUND_EDITOR_CPP,
            HYBRID_H,
            HYBRID_CPP,
            TEMASEK_FACTORY,
            LEGACY_TEMASEK_MANIFEST,
            LEGACY_TEMASEK_MTL,
            MACDONALD_MTL,
            GRASS_MANIFEST,
        )
        for path in paths:
            if not path.is_file():
                raise AssertionError(f"missing R27 contract input: {path}")
        cls.actor_h = ACTOR_H.read_text(encoding="utf-8")
        cls.actor = ACTOR_CPP.read_text(encoding="utf-8")
        cls.editor_h = EDITOR_H.read_text(encoding="utf-8")
        cls.editor = EDITOR_CPP.read_text(encoding="utf-8")
        cls.ground_editor_h = GROUND_EDITOR_H.read_text(encoding="utf-8")
        cls.ground_editor = GROUND_EDITOR_CPP.read_text(encoding="utf-8")
        cls.hybrid_h = HYBRID_H.read_text(encoding="utf-8")
        cls.hybrid = HYBRID_CPP.read_text(encoding="utf-8")
        cls.temasek_factory = TEMASEK_FACTORY.read_text(encoding="utf-8")

    def test_r26_failure_is_corrected_by_isolated_65_to_90_m_materials(self):
        for token in (
            "TRIAD_EXPLORE_V5D_GRASS_R23B_STABLE_SPATIAL_VISIBILITY_20M_28M",
            "smoothstep(2000.0,2800.0,distanceCm)",
            "TRIAD_EXPLORE_V5D_LANDMARK_GRASS_R27_SINGLE_GATE_VISIBILITY_65M_90M",
            "smoothstep(6500.0,9000.0,distanceCm)",
            "effectiveVisibility=max(componentVisibility,calibratedVisibility)",
            "return stableGate*revisionGate*calibrationGate",
            "SourceStableVisibilityCode",
            "R27StableVisibilityCode",
            "TargetLegacyCount != 0 || TargetR27Count != 1",
            "SourceData->OpacityMask.Expression != SourceStable",
            "TargetData->OpacityMask.Expression != TargetStable",
            "SourceData->ExpressionCollection.Expressions.Num() != 32",
            "SourceExpression->GetClass() != TargetExpression->GetClass()",
            "ValidateExactR23BDerivativeGrassMaterial",
            "FPackageName::DoesPackageExist",
        ):
            self.assertIn(token, self.editor)
        r27_code = between(
            self.editor,
            "const FString R27StableVisibilityCode(",
            "const FString UmbrellaTreeMeshPath(",
        )
        self.assertEqual(1, len(re.findall(r"(?<!smooth)step\(", r27_code)))
        self.assertNotIn("instanceGate", r27_code)
        self.assertNotIn("distanceGate", r27_code)
        for token in (
            "ValidateExactR23BDerivativeGrassMaterial",
            "ValidateR23BGrassMaterial(Source, ProfileIndex",
            "ValidateCompiledMaterial(Material, OutError)",
            "clean disk-backed source and target packages",
        ):
            self.assertIn(token, self.ground_editor_h + self.ground_editor)
        target_root = (
            "/Game/TRIAD/IstanaPublicViewExploreV5D/"
            "LandmarkVegetationR27/Materials/"
        )
        self.assertEqual(4, self.actor.count(target_root))
        self.assertIn("StaticDuplicateObject", self.editor)
        self.assertIn("sourceMaterialsModified=false", self.editor)
        self.assertNotIn(
            "/GroundVegetation/Materials/M_IPV5D_Turf_Manicured",
            self.actor,
        )

    def test_far_camera_acceptance_math_is_measurable_and_conservative(self):
        cull_start = constexpr_number(self.actor, "GrassCullStartDistanceCm")
        cull_end = constexpr_number(self.actor, "GrassCullEndDistanceCm")
        distance = constexpr_number(self.actor, "EvidenceViewDistanceCm")
        height_scale = constexpr_number(
            self.actor, "GrassHeightScaleMinimum"
        )
        source_height = constexpr_number(
            self.actor, "GrassCarrierMaximumSourceHeightCm"
        )
        horizontal_fov = constexpr_number(
            self.actor, "EvidenceViewHorizontalFovDegrees"
        )
        aspect = 16.0 / 9.0
        image_height = constexpr_number(self.actor, "EvidenceViewHeightPixels")

        alpha = (distance - cull_start) / (cull_end - cull_start)
        visibility = 1.0 - alpha * alpha * (3.0 - 2.0 * alpha)
        vertical_half_fov = math.atan(
            math.tan(math.radians(horizontal_fov / 2.0)) / aspect
        )
        focal_pixels = image_height / (2.0 * math.tan(vertical_half_fov))
        projected_tip_pixels = (
            source_height * height_scale / distance * focal_pixels
        )

        self.assertAlmostEqual(72.8, distance / 100.0, places=6)
        self.assertGreaterEqual(visibility, 0.75)
        self.assertLess(visibility, 1.0)
        self.assertGreaterEqual(projected_tip_pixels, 1.5)
        self.assertLess(projected_tip_pixels, 2.0)
        self.assertIn(
            "ExpectedMaterialVisibilityAtEvidenceView() < 0.75",
            self.actor,
        )
        self.assertIn(
            "MinimumAcceptedTallestCarrierTipProjectionPixels = 1.50",
            self.actor,
        )
        for token in (
            "visibilityGateCount=1",
            "componentFadeIntegrated=true",
            "tallestCarrierTipProjectionPixelsAtEvidenceRange=",
            "projectionAssumption=perpendicularPinholeMaxSourceTip",
        ):
            self.assertIn(token, self.actor + self.editor + self.hybrid)
        self.assertIn("visualCaptureAccepted=false", self.actor + self.editor + self.hybrid)
        self.assertIn(
            "captureRevalidationRequired=true",
            self.actor + self.editor + self.hybrid,
        )
        self.assertNotIn("distanceReadableGrass=true", self.actor)

    def test_carrier_scale_is_visible_but_remains_lawn_scale_and_bounded(self):
        manifest = json.loads(GRASS_MANIFEST.read_text(encoding="utf-8"))
        manifest_text = json.dumps(manifest)
        self.assertIn("4.4", manifest_text)
        source_height = constexpr_number(
            self.actor, "GrassCarrierMaximumSourceHeightCm"
        )
        xy_min = constexpr_number(self.actor, "GrassCoverageScaleMinimum")
        xy_max = constexpr_number(self.actor, "GrassCoverageScaleMaximum")
        z_min = constexpr_number(self.actor, "GrassHeightScaleMinimum")
        z_max = constexpr_number(self.actor, "GrassHeightScaleMaximum")
        self.assertEqual((1.12, 1.38), (xy_min, xy_max))
        self.assertEqual((1.72, 2.12), (z_min, z_max))
        self.assertGreaterEqual(source_height * z_min, 7.5)
        self.assertLessEqual(source_height * z_max, 10.0)
        for token in (
            "FVector(CoverageScale, CoverageScale, HeightScale)",
            "Scale.X < GrassCoverageScaleMinimum",
            "Scale.X > GrassCoverageScaleMaximum",
            "FMath::IsNearlyEqual(Scale.X, Scale.Y, 0.0001)",
            "Scale.Z < GrassHeightScaleMinimum",
            "Scale.Z > GrassHeightScaleMaximum",
            "GrassCarrierMinimum.X, -78.05, -77.72",
            "GrassCarrierMaximum.Z, 4.33, 4.47",
            "exact R11 Bermuda carrier centimetre bounds",
        ):
            self.assertIn(token, self.actor)

    def test_performance_budget_and_wpo_boundary_are_unchanged(self):
        for token in (
            "MaximumGrassInstancesPerLandmark = 2048",
            "MacDonaldGrassCount = 1536",
            "TemasekGrassCount = 1536",
            "GrassWpoDisableDistanceCm = 2400",
            "static_assert(MacDonaldGrassCount <= MaximumGrassInstancesPerLandmark)",
            "static_assert(TemasekGrassCount <= MaximumGrassInstancesPerLandmark)",
            "grassInstances=3072",
            "maximumGrassPerSite=2048",
            "renderOnly=true",
            "sensorAuthority=false",
            "rfAuthority=false",
        ):
            self.assertIn(token, self.actor + self.hybrid)
        self.assertEqual(
            10,
            self.actor_h.count(
                "TObjectPtr<UHierarchicalInstancedStaticMeshComponent>"
            ),
        )

    def test_material_transaction_is_exact_four_saved_packages_and_idempotent(self):
        for endpoint in (
            "BuildOrValidateLandmarkGrassMaterialsR27",
            "ValidateLandmarkGrassMaterialsR27",
        ):
            self.assertIn(endpoint, self.editor_h)
            self.assertIn(endpoint, self.editor)
        for marker in (
            "BUILD_REFUSED_PARTIAL_NAMESPACE",
            "ExistingPackages != 0 && ExistingPackages != 4",
            "createdPackages=4",
            "exactSavedPackages=4",
            "IDEMPOTENT_PASS",
            "AssetsToSave.Num() != 4",
            "SaveLoadedAssets(AssetsToSave, false)",
            "ValidateR27GrassMaterialsInternal(true",
        ):
            self.assertIn(marker, self.editor)
        self.assertNotIn("SaveMap(", self.editor)
        self.assertNotIn("SpawnActor", self.editor)

    def test_map_endpoint_requires_pinned_predecessor_and_external_backup(self):
        for endpoint in (
            "ApplyIstanaExploreV5DLandmarkVegetationR27VisualCorrectionToLoadedHybridMap",
            "ValidateIstanaExploreV5DLandmarkVegetationR27SuccessorMap",
        ):
            self.assertIn(endpoint, self.hybrid_h)
            self.assertIn(endpoint, self.hybrid)
        apply = between(
            self.hybrid,
            "ApplyIstanaExploreV5DLandmarkVegetationR27VisualCorrectionToLoadedHybridMap(",
            "ValidateIstanaExploreV5DLandmarkVegetationR27SuccessorMap(",
        )
        for token in (
            "ExpectedPredecessorBytes",
            "ExpectedPredecessorSha256",
            "VerifiedExternalBackupFilename",
            "V5DLandmarkVegetationR27V1",
            "FPaths::IsUnderDirectory",
            "FPaths::IsSamePath",
            "BackupBytes != ExpectedPredecessorBytes",
            "BackupSha256 != ExpectedSha256",
            "rollbackOwnedByWrapper=true",
            "ValidateHybridWorld(",
            "ELandmarkPresencePolicy::Required,\n            false",
            "ConfigureLandmarkVegetationActor",
            "ValidateLandmarkVegetationR27World",
            "IDEMPOTENT_EXPLORE_V5D_LANDMARK_VEGETATION_R27_ALREADY_VALID",
            "APPLY_REFUSED_FINAL_MUTATION_GATE",
            "APPLY_FAILED_PRE_SAVE_PIN_GATE",
            "MutationGateBackupSha256",
            "PreSaveBackupSha256",
        ):
            self.assertIn(token, apply)
        self.assertEqual(1, apply.count("SaveMap("))
        self.assertLess(apply.index("BackupSha256 != ExpectedSha256"), apply.index("SaveMap("))
        self.assertLess(apply.index("ConfigureLandmarkVegetationActor"), apply.index("SaveMap("))
        self.assertLess(apply.index("SaveMap("), apply.index("LoadMap(DestinationFilename)"))

    def test_cartoon_tree_owner_is_legacy_temasek_not_macdonald(self):
        legacy = json.loads(LEGACY_TEMASEK_MANIFEST.read_text(encoding="utf-8"))
        counts = legacy["counts"]
        categories = counts["categories"]
        self.assertEqual(25_600, counts["triangles"])
        self.assertEqual(19, counts["materials"])
        self.assertEqual(3, categories["MATURE_TREE_TRUNK"])
        self.assertEqual(18, categories["MATURE_TREE_BRANCH"])
        self.assertEqual(27, categories["MATURE_TREE_CANOPY"])
        self.assertEqual(12, categories["ROOF_TERRACE_FOLIAGE"])
        temasek_mtl = LEGACY_TEMASEK_MTL.read_text(encoding="utf-8")
        for material in ("M_TSH_Bark", "M_TSH_LeafDeep", "M_TSH_LeafLight"):
            self.assertIn(f"newmtl {material}", temasek_mtl)
        macdonald_mtl = MACDONALD_MTL.read_text(encoding="utf-8")
        for token in ("Leaf", "Bark", "Tree", "Foliage"):
            self.assertNotIn(token, macdonald_mtl)

        for token in (
            "ExpectedTriangleCount = 15760",
            "ExpectedMaterialCount = 15",
            "bakedFoliageRenderComponents=0",
            "foliageOwner=ATRIADIstanaExploreV5DLandmarkVegetationActor",
            "foliageTreeAnchors=3",
        ):
            self.assertIn(token, self.temasek_factory)
        for marker in (
            "legacyTemasekBakedFoliageRemoved=true",
            "temasekMeshTriangles=15760",
            "temasekMaterialSlots=15",
            "bakedFoliageRenderComponents=0",
        ):
            self.assertIn(marker, self.hybrid)


if __name__ == "__main__":
    unittest.main()
