import math
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
PLUGIN = ROOT / "Plugins" / "TRIADSensorFusion"
RUNTIME_PUBLIC = PLUGIN / "Source" / "TRIADSensorFusion" / "Public"
RUNTIME_PRIVATE = PLUGIN / "Source" / "TRIADSensorFusion" / "Private"
EDITOR_PUBLIC = PLUGIN / "Source" / "TRIADSensorFusionEditor" / "Public"
EDITOR_PRIVATE = PLUGIN / "Source" / "TRIADSensorFusionEditor" / "Private"


def mix_bits(value: int) -> int:
    value &= 0xFFFFFFFF
    value ^= value >> 16
    value = (value * 0x7FEB352D) & 0xFFFFFFFF
    value ^= value >> 15
    value = (value * 0x846CA68B) & 0xFFFFFFFF
    value ^= value >> 16
    return value & 0xFFFFFFFF


def hash_unit(value: int) -> float:
    return (mix_bits(value) & 0x00FFFFFF) / float(0x01000000)


def radical_inverse(index: int, base: int) -> float:
    inverse_base = 1.0 / base
    fraction = inverse_base
    result = 0.0
    while index > 0:
        result += (index % base) * fraction
        index //= base
        fraction *= inverse_base
    return result


def analytic_height_cm(x_cm: float, y_cm: float) -> float:
    x_m = x_cm / 100.0
    y_m = y_cm / 100.0
    radius_m = math.hypot(x_m, y_m)
    return 100.0 * (
        0.72 * math.sin(x_m / 185.0)
        + 0.48 * math.cos(y_m / 230.0)
        + 0.22 * math.sin((x_m + y_m) / 97.0)
        + 0.0000011 * radius_m * radius_m
        - 0.48
    )


SITES = {
    "macdonald": {
        "anchor": (36320.390052826, 87155.365772797),
        "other_anchor": (40411.657951, 88424.311139),
        "yaw": -161.885822122792,
        "axes": (7200.0, 5000.0),
        "exclusion": (1800.0, 1100.0),
        "count": 1536,
        "seed": 0x4D414344,
    },
    "temasek": {
        "anchor": (40411.657951, 88424.311139),
        "other_anchor": (36320.390052826, 87155.365772797),
        "yaw": -162.5152283523459,
        "axes": (6900.0, 4700.0),
        "exclusion": (2300.0, 1550.0),
        "count": 1536,
        "seed": 0x54454D41,
    },
}


def local_to_world(site: dict, local_x: float, local_y: float) -> tuple[float, float]:
    yaw = math.radians(site["yaw"])
    cos_yaw = math.cos(yaw)
    sin_yaw = math.sin(yaw)
    anchor_x, anchor_y = site["anchor"]
    return (
        anchor_x + local_x * cos_yaw - local_y * sin_yaw,
        anchor_y + local_x * sin_yaw + local_y * cos_yaw,
    )


def accepted_grass(site: dict) -> list[tuple[int, int, float, float, float]]:
    accepted = []
    ceiling = site["count"] * 32
    for candidate in range(1, ceiling + 1):
        local_x = (2.0 * radical_inverse(candidate, 2) - 1.0) * site["axes"][0]
        local_y = (2.0 * radical_inverse(candidate, 3) - 1.0) * site["axes"][1]
        ellipse = (local_x / site["axes"][0]) ** 2 + (
            local_y / site["axes"][1]
        ) ** 2
        world_x, world_y = local_to_world(site, local_x, local_y)
        own_d2 = (world_x - site["anchor"][0]) ** 2 + (
            world_y - site["anchor"][1]
        ) ** 2
        other_d2 = (world_x - site["other_anchor"][0]) ** 2 + (
            world_y - site["other_anchor"][1]
        ) ** 2
        outside_hardscape = (
            abs(local_x) > site["exclusion"][0]
            or abs(local_y) > site["exclusion"][1]
        )
        if ellipse > 1.0 or own_d2 > other_d2 or not outside_hardscape:
            continue
        stable = mix_bits(candidate ^ site["seed"])
        bucket = mix_bits(stable ^ 0xA17C9E21) % 100
        profile = 0 if bucket < 55 else (1 if bucket < 75 else (2 if bucket < 90 else 3))
        gap_cm = 1.0 + 2.0 * hash_unit(stable ^ 0xC5EED123)
        accepted.append((candidate, profile, world_x, world_y, gap_cm))
        if len(accepted) == site["count"]:
            break
    return accepted


class LandmarkVegetationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.actor_h_path = (
            RUNTIME_PUBLIC / "TRIADIstanaExploreV5DLandmarkVegetationActor.h"
        )
        cls.actor_cpp_path = (
            RUNTIME_PRIVATE / "TRIADIstanaExploreV5DLandmarkVegetationActor.cpp"
        )
        cls.editor_h_path = (
            EDITOR_PUBLIC
            / "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.h"
        )
        cls.editor_cpp_path = (
            EDITOR_PRIVATE
            / "TRIADIstanaExploreV5DLandmarkVegetationEditorLibrary.cpp"
        )
        for path in (
            cls.actor_h_path,
            cls.actor_cpp_path,
            cls.editor_h_path,
            cls.editor_cpp_path,
        ):
            if not path.is_file():
                raise AssertionError(f"missing landmark vegetation file: {path}")
        cls.actor_h = cls.actor_h_path.read_text(encoding="utf-8")
        cls.actor_cpp = cls.actor_cpp_path.read_text(encoding="utf-8")
        cls.editor_h = cls.editor_h_path.read_text(encoding="utf-8")
        cls.editor_cpp = cls.editor_cpp_path.read_text(encoding="utf-8")

    def constexpr_int(self, name: str) -> int:
        match = re.search(rf"constexpr int32 {re.escape(name)} = (\d+);", self.actor_cpp)
        self.assertIsNotNone(match, name)
        return int(match.group(1))

    def test_r27_asset_boundary_is_isolated_from_map_and_facade_work(self):
        combined = self.actor_h + self.actor_cpp + self.editor_h + self.editor_cpp
        for forbidden in (
            "TRIADIstanaExploreV5DHybridEditorLibrary",
            "TRIADIstanaExploreV5DCurrentSurroundings",
            "TRIADIstanaExploreV5DContextPolicy",
            "TRIADIstanaExploreV5DTemasekShophouseActor",
        ):
            self.assertNotIn(forbidden, combined)
        for forbidden in (
            "SpawnActor",
            "SavePackage",
            "LoadMap(",
            "CreateFreshAssets",
        ):
            self.assertNotIn(forbidden, self.editor_cpp)
        self.assertIn("mapMutation=false", self.editor_cpp)
        self.assertIn("BuildOrValidateLandmarkGrassMaterialsR27", self.editor_cpp)
        self.assertIn("SaveLoadedAssets(AssetsToSave, false)", self.editor_cpp)
        self.assertIn("createdPackages=4", self.editor_cpp)
        self.assertIn("sourceMaterialsModified=false", self.editor_cpp)

    def test_exact_landmark_anchors_and_world_space_serialization(self):
        for token in (
            "FVector(36320.390052826, 87155.365772797, 0.0)",
            "-161.885822122792",
            "FVector(40411.657951, 88424.311139, 0.0)",
            "-162.5152283523459",
            "SavedLayout",
            "FTRIADIstanaExploreV5DLandmarkVegetationLayout",
            "GetActorTransform().Equals(FTransform::Identity, 0.0001)",
            "WorldToLocalXY",
            "LocalToWorldXY",
            "nearestLandmarkPartition=true",
        ):
            self.assertIn(token, self.actor_h + self.actor_cpp)
        self.assertRegex(
            self.actor_cpp,
            r"AddInstances\(\s*Combined,\s*false,\s*true,\s*false\s*\);",
        )
        self.assertRegex(
            self.actor_h,
            r"UPROPERTY\([^)]*\)\s*FTRIADIstanaExploreV5DLandmarkVegetationLayout SavedLayout;",
        )
        for site in SITES.values():
            self.assertLess(math.hypot(*site["anchor"]), 100000.0)

    def test_low_discrepancy_layout_is_bounded_exact_and_replayable(self):
        for token in (
            "RadicalInverse",
            "CandidateAttemptsPerGrassInstance = 32",
            "static_cast<uint32>(Candidate),\n                       2u",
            "static_cast<uint32>(Candidate),\n                       3u",
            "MixBits",
            "BuildDeterministicLayout",
            "OwnDistanceSquared <= OtherDistanceSquared",
            "AppendPlantingBeds",
            "GoldenAngleRadians = 2.39996322972865332",
            "ValidatePatchOwnedTransformArray",
        ):
            self.assertIn(token, self.actor_cpp)
        self.assertNotIn("AppendPlantingRing", self.actor_cpp)
        for forbidden in (
            "FMath::Rand",
            "FRandomStream",
            "LineTrace",
            "SweepSingle",
            "SweepMulti",
            "OverlapBlockingTest",
            "GetHitResultUnderCursor",
        ):
            self.assertNotIn(forbidden, self.actor_cpp)

        generated = {}
        for name, site in SITES.items():
            instances = accepted_grass(site)
            generated[name] = instances
            self.assertEqual(site["count"], len(instances), name)
            self.assertLessEqual(instances[-1][0], site["count"] * 32)
            profile_counts = [0, 0, 0, 0]
            for _, profile, world_x, world_y, gap_cm in instances:
                profile_counts[profile] += 1
                self.assertTrue(math.isfinite(analytic_height_cm(world_x, world_y)))
                self.assertGreaterEqual(gap_cm, 1.0)
                self.assertLessEqual(gap_cm, 3.0)
                own_d2 = (world_x - site["anchor"][0]) ** 2 + (
                    world_y - site["anchor"][1]
                ) ** 2
                other_d2 = (world_x - site["other_anchor"][0]) ** 2 + (
                    world_y - site["other_anchor"][1]
                ) ** 2
                self.assertLessEqual(own_d2, other_d2 + 1e-6)
            self.assertTrue(all(count > 0 for count in profile_counts), profile_counts)

        mac_points = {(round(row[2], 6), round(row[3], 6)) for row in generated["macdonald"]}
        temasek_points = {(round(row[2], 6), round(row[3], 6)) for row in generated["temasek"]}
        self.assertFalse(mac_points.intersection(temasek_points))

    def test_grass_and_tree_render_budgets_are_explicit(self):
        self.assertEqual(2048, self.constexpr_int("MaximumGrassInstancesPerLandmark"))
        self.assertEqual(1536, self.constexpr_int("MacDonaldGrassCount"))
        self.assertEqual(1536, self.constexpr_int("TemasekGrassCount"))
        self.assertLessEqual(
            self.constexpr_int("MacDonaldGrassCount"),
            self.constexpr_int("MaximumGrassInstancesPerLandmark"),
        )
        self.assertLessEqual(
            self.constexpr_int("TemasekGrassCount"),
            self.constexpr_int("MaximumGrassInstancesPerLandmark"),
        )
        self.assertEqual(6500, self.constexpr_int("GrassCullStartDistanceCm"))
        self.assertEqual(9000, self.constexpr_int("GrassCullEndDistanceCm"))
        self.assertEqual(2400, self.constexpr_int("GrassWpoDisableDistanceCm"))
        self.assertLess(
            self.constexpr_int("GrassWpoDisableDistanceCm"),
            self.constexpr_int("GrassCullStartDistanceCm"),
        )
        self.assertLess(7280, self.constexpr_int("GrassCullEndDistanceCm"))
        for token in (
            "GrassLodDistanceScale = 0.60f",
            "RuntimeTreeMinimumLod = 0",
            "RuntimeTreeForcedLodModel = 0",
            "RuntimeTreeLodDistanceScale = 1.8f",
            "Component->bOverrideMinLOD = true",
            "Component->MinLOD = RuntimeTreeMinimumLod",
            "Component->ForcedLodModel = RuntimeTreeForcedLodModel",
            "Component->WorldPositionOffsetDisableDistance = WpoDisableDistanceCm",
            "Component->bEnableDensityScaling = false",
            "Component->bAutoRebuildTreeOnInstanceChanges = false",
        ):
            self.assertIn(token, self.actor_cpp)
        self.assertEqual(
            10,
            self.actor_h.count(
                "TObjectPtr<UHierarchicalInstancedStaticMeshComponent>"
            ),
        )
        self.assertEqual(
            10,
            self.actor_cpp.count(
                "CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>"
            ),
        )

    def test_render_only_truth_boundary_is_fail_closed(self):
        for token in (
            "SetActorEnableCollision(false)",
            "SetCollisionEnabled(ECollisionEnabled::NoCollision)",
            "SetCollisionResponseToAllChannels(ECR_Ignore)",
            "SetGenerateOverlapEvents(false)",
            "SetCanEverAffectNavigation(false)",
            "bAppearanceOnly = true",
            "bSourceAssetPackagesModified = false",
            "bCollisionOrNavigationAuthority = false",
            "bSensorOrRfAuthority = false",
            "bBotanicalSurveyOrCurrentSeasonClaimed = false",
            "collision=false",
            "navigation=false",
            "sensorAuthority=false",
            "rfAuthority=false",
            "surveyClaim=false",
            "botanicalClaim=false",
            "seasonalClaim=false",
        ):
            self.assertIn(token, self.actor_h + self.actor_cpp + self.editor_cpp)
        self.assertIn("FCollisionResponseContainer(ECR_Ignore)", self.actor_cpp)
        self.assertIn("GetActorEnableCollision()", self.actor_cpp)
        self.assertNotIn("QueryAndPhysics", self.actor_cpp)
        self.assertNotIn("QueryOnly", self.actor_cpp)

    def test_analytic_surface_is_exact_and_does_not_trace(self):
        for token in (
            "0.72 * FMath::Sin(XMeters / 185.0)",
            "0.48 * FMath::Cos(YMeters / 230.0)",
            "0.22 * FMath::Sin((XMeters + YMeters) / 97.0)",
            "0.0000011 * RadiusMeters * RadiusMeters - 0.48",
            "GrassPlacementGapMinimumCm = 1.0",
            "GrassPlacementGapMaximumCm = 3.0",
        ):
            self.assertIn(token, self.actor_cpp)
        for x_cm, y_cm in (
            SITES["macdonald"]["anchor"],
            SITES["temasek"]["anchor"],
            (32000.0, 84000.0),
            (45000.0, 92000.0),
        ):
            self.assertTrue(math.isfinite(analytic_height_cm(x_cm, y_cm)))

    def test_exact_reusable_v4_v5b_v5d_asset_roster(self):
        expected_paths = (
            "/Game/TRIAD/IstanaPublicViewExploreV5B/Vegetation/Meshes/SM_IPV5B_BermudaTurfCluster.SM_IPV5B_BermudaTurfCluster",
            "/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR27/Materials/M_IPV5D_LandmarkTurf_R27_Manicured.M_IPV5D_LandmarkTurf_R27_Manicured",
            "/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR27/Materials/M_IPV5D_LandmarkTurf_R27_Humid.M_IPV5D_LandmarkTurf_R27_Humid",
            "/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR27/Materials/M_IPV5D_LandmarkTurf_R27_Shade.M_IPV5D_LandmarkTurf_R27_Shade",
            "/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR27/Materials/M_IPV5D_LandmarkTurf_R27_DryEdge.M_IPV5D_LandmarkTurf_R27_DryEdge",
            "/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Umbrella_NearLOD0.SM_IPV5D_Tree_Umbrella_NearLOD0",
            "/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_Dome_NearLOD0.SM_IPV5D_Tree_Dome_NearLOD0",
            "/Game/TRIAD/IstanaPublicViewExploreV5D/TreeRealism/Meshes/SM_IPV5D_Tree_HighForkRounded_NearLOD0.SM_IPV5D_Tree_HighForkRounded_NearLOD0",
            "/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Meshes/SM_IPV4_Shrub04_A.SM_IPV4_Shrub04_A",
            "/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_Shrub04_Wind.M_IPV4_Shrub04_Wind",
            "/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Meshes/SM_IPV4_Calathea_D.SM_IPV4_Calathea_D",
            "/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_Calathea_Wind.M_IPV4_Calathea_Wind",
            "/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Meshes/SM_IPV4_Periwinkle06_F.SM_IPV4_Periwinkle06_F",
            "/Game/TRIAD/IstanaPublicViewExploreV4/Vegetation/Materials/M_IPV4_Periwinkle_Wind.M_IPV4_Periwinkle_Wind",
        )
        for path in expected_paths:
            self.assertIn(path, self.actor_cpp)
        for path in expected_paths[:1] + expected_paths[5:]:
            self.assertIn(path, self.editor_cpp)
        self.assertIn(
            "/Game/TRIAD/IstanaPublicViewExploreV5D/LandmarkVegetationR27/Materials/",
            self.editor_cpp,
        )
        for name in (
            "M_IPV5D_LandmarkTurf_R27_Manicured",
            "M_IPV5D_LandmarkTurf_R27_Humid",
            "M_IPV5D_LandmarkTurf_R27_Shade",
            "M_IPV5D_LandmarkTurf_R27_DryEdge",
        ):
            self.assertIn(name, self.editor_cpp)
        self.assertIn("exactAssets=14", self.editor_cpp)
        self.assertIn("reusedUnmodifiedAssets=10", self.editor_cpp)
        self.assertIn("isolatedR27GrassMaterials=4", self.editor_cpp)
        for source_material in (
            "M_IPV5D_Turf_Manicured",
            "M_IPV5D_Turf_Humid",
            "M_IPV5D_Turf_Shade",
            "M_IPV5D_Turf_DryEdge",
        ):
            self.assertIn(source_material, self.editor_cpp)
        self.assertNotIn("Tree_Palm", self.actor_cpp + self.editor_cpp)
        self.assertNotIn("PalmTree", self.actor_cpp + self.editor_cpp)

    def test_temasek_tree_replacement_uses_the_exact_r24_foliage_handoff(self):
        for token in (
            "FVector(-18.5, -19.0, 0.0)",
            "FVector(18.5, -18.8, 0.0)",
            "FVector(-13.2, -16.2, 0.0)",
            "ExpectedTemasekFoliageLayoutSchema",
            "ExpectedTemasekSourcePlacementTransform",
            "MakeTemasekContractTreeWorldTransform",
            "translation/yaw/non-uniform plan scale",
            "actorMapIntegrationAuthority=false",
            "r27FarCameraCorrectionConfigured=true",
            "evidenceRangeMeters=72.8",
            "visualCaptureAccepted=false",
            "captureRevalidationRequired=true",
        ):
            self.assertIn(token, self.actor_cpp)
        self.assertNotIn("ApplyIstanaExploreV5DTemasek", self.editor_cpp)


if __name__ == "__main__":
    unittest.main()
