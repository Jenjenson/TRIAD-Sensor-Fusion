from __future__ import annotations

import math
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


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


def mix_bits(value: int) -> int:
    value &= 0xFFFFFFFF
    value ^= value >> 16
    value = (value * 0x7FEB352D) & 0xFFFFFFFF
    value ^= value >> 15
    value = (value * 0x846CA68B) & 0xFFFFFFFF
    value ^= value >> 16
    return value & 0xFFFFFFFF


def radical_inverse(index: int, base: int) -> float:
    inverse_base = 1.0 / base
    fraction = inverse_base
    result = 0.0
    while index > 0:
        result += (index % base) * fraction
        index //= base
        fraction *= inverse_base
    return result


SITES = {
    "macdonald": {
        "anchor": (36320.390052826, 87155.365772797),
        "other": (40411.657951, 88424.311139),
        "yaw": -161.885822122792,
        "axes": (7200.0, 5000.0),
        "exclusion": (1535.0, 835.0),
        "seed": 0x4D414344 ^ 0x1C,
    },
    "temasek": {
        "anchor": (40411.657951, 88424.311139),
        "other": (36320.390052826, 87155.365772797),
        "yaw": -162.5152283523459,
        "axes": (6900.0, 4700.0),
        "exclusion": (2035.0, 1285.0),
        "seed": 0x54454D41 ^ 0x1C,
    },
}


def accepted_r28_profiles(site: dict) -> list[int]:
    accepted: list[int] = []
    yaw = math.radians(site["yaw"])
    cos_yaw = math.cos(yaw)
    sin_yaw = math.sin(yaw)
    ax, ay = site["anchor"]
    ox, oy = site["other"]
    for candidate in range(1, 3072 * 32 + 1):
        sequence = candidate + 65537
        lx = (2.0 * radical_inverse(sequence, 2) - 1.0) * site["axes"][0]
        ly = (2.0 * radical_inverse(sequence, 3) - 1.0) * site["axes"][1]
        ellipse = (lx / site["axes"][0]) ** 2 + (
            ly / site["axes"][1]
        ) ** 2
        wx = ax + lx * cos_yaw - ly * sin_yaw
        wy = ay + lx * sin_yaw + ly * cos_yaw
        own_d2 = (wx - ax) ** 2 + (wy - ay) ** 2
        other_d2 = (wx - ox) ** 2 + (wy - oy) ** 2
        outside_hardscape = (
            abs(lx) > site["exclusion"][0]
            or abs(ly) > site["exclusion"][1]
        )
        if ellipse > 1.0 or own_d2 > other_d2 or not outside_hardscape:
            continue
        stable = mix_bits(sequence ^ site["seed"])
        bucket = mix_bits(stable ^ 0x528C91E7) % 100
        accepted.append(
            0 if bucket < 70 else (1 if bucket < 90 else (2 if bucket < 99 else 3))
        )
        if len(accepted) == 3072:
            break
    return accepted


class LandmarkVegetationR28IntegrationContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for path in (
            ACTOR_H,
            ACTOR_CPP,
            EDITOR_H,
            EDITOR_CPP,
            GROUND_EDITOR_H,
            GROUND_EDITOR_CPP,
        ):
            if not path.is_file():
                raise AssertionError(f"missing R28 contract input: {path}")
        cls.actor_h = ACTOR_H.read_text(encoding="utf-8")
        cls.actor = ACTOR_CPP.read_text(encoding="utf-8")
        cls.editor_h = EDITOR_H.read_text(encoding="utf-8")
        cls.editor = EDITOR_CPP.read_text(encoding="utf-8")
        cls.ground_h = GROUND_EDITOR_H.read_text(encoding="utf-8")
        cls.ground = GROUND_EDITOR_CPP.read_text(encoding="utf-8")

    def test_r28_is_additive_and_keeps_r27_contract_callable(self):
        combined = self.actor_h + self.actor + self.editor_h + self.editor
        for token in (
            "ConfigureLandmarkVegetation(",
            "ValidateLandmarkVegetation(",
            "BuildOrValidateLandmarkGrassMaterialsR27",
            "ValidateLandmarkGrassMaterialsR27",
            "ConfigureLandmarkVegetationR28(",
            "ValidateLandmarkVegetationR28(",
            "BuildOrValidateLandmarkGrassMaterialsR28",
            "ValidateLandmarkGrassMaterialsR28",
            "VISUAL_ASSUMPTION_BOUND_R28_DENSE_TURF_MATURE_TROPICAL_CANOPY",
        ):
            self.assertIn(token, combined)

    def test_dense_layout_is_replayable_bounded_and_all_profiles_survive(self):
        for token in (
            "MaximumGrassInstancesPerLandmarkR28 = 4096",
            "MacDonaldGrassCountR28 = 3072",
            "TemasekGrassCountR28 = 3072",
            "R28GrassSequenceOffset = 65537u",
            "R28GrassProfileSalt = 0x528C91E7u",
            "ProfileBucket < 70u",
            "ProfileBucket < 90u",
            "ProfileBucket < 99u",
            "Scale.Z > 1.98 + 0.0001",
        ):
            self.assertIn(token, self.actor)
        for site in SITES.values():
            profiles = accepted_r28_profiles(site)
            self.assertEqual(3072, len(profiles))
            self.assertEqual({0, 1, 2, 3}, set(profiles))
            self.assertGreaterEqual(profiles.count(0), 2000)
            self.assertLessEqual(profiles.count(3), 50)

    def test_bald_stripe_is_reduced_without_entering_building_footprints(self):
        r28_specs = between(
            self.actor,
            "FLandmarkSiteSpec GetR28SiteSpec(",
            "uint32 MixBits(",
        )
        for token in (
            "R28HardscapeExclusionClearanceCm = 35.0",
            "1500.0 + R28HardscapeExclusionClearanceCm",
            "800.0 + R28HardscapeExclusionClearanceCm",
            "2000.0 + R28HardscapeExclusionClearanceCm",
            "1250.0 + R28HardscapeExclusionClearanceCm",
            "no grass is admitted beneath building geometry",
        ):
            self.assertIn(token, self.actor)
        self.assertNotIn("300.0 +", r28_specs)
        carrier_area = 157.07 * 136.84 * 1.12**2
        for site in SITES.values():
            patch_area = math.pi * site["axes"][0] * site["axes"][1]
            exclusion_area = 4.0 * site["exclusion"][0] * site["exclusion"][1]
            coverage = 3072 * carrier_area / (patch_area - exclusion_area)
            self.assertGreaterEqual(coverage, 0.75)
        self.assertIn("R28MinimumNominalCarrierCoverage = 0.75", self.actor)
        self.assertIn("NominalR28CarrierCoverage(Spec)", self.actor)

    def test_temasek_canopy_is_mature_irregular_and_anchor_preserving(self):
        r28_trees = between(
            self.actor,
            "void BuildTreesR28(",
            "bool AppendPlantingBeds(",
        )
        for token in (
            "MakeTemasekContractTreeWorldTransformR28",
            "0, 8.0, FVector(2.05, 1.78, 1.72)",
            "1, -13.0, FVector(1.82, 2.00, 1.68)",
            "2, 29.0, FVector(1.92, 1.72, 1.58)",
            "umbrella/dome/umbrella roster",
            "sourceTreeAnchorTranslationsPreserved=true",
        ):
            self.assertIn(token, self.actor)
        self.assertEqual(2, r28_trees.count("OutLayout.TreesUmbrella.Add"))
        self.assertEqual(1, r28_trees.count("OutLayout.TreesDome.Add"))
        self.assertNotIn("OutLayout.TreesHighFork.Add", r28_trees)
        self.assertIn("!Layout.TreesHighFork.IsEmpty()", self.actor)

    def test_material_delta_is_exact_green_and_source_preserving(self):
        root = (
            "/Game/TRIAD/IstanaPublicViewExploreV5D/"
            "LandmarkVegetationR28/Materials/"
        )
        self.assertEqual(4, self.actor.count(root))
        for token in (
            "R28SourceGrassMaterialPaths",
            "R28GrassMaterialAssetNames",
            "R28GrassMaterialRoot",
            "ExistingPackages != 0 && ExistingPackages != 4",
            "AssetsToSave.Num() != 4",
            "ValidateExactR23BVisualDerivativeGrassMaterial",
            "allowedGraphDeltas=visibilityLabel,colorCode",
            "nearDetailedColor=lerp(nearDetailedColor,dryColor,0.26*dryNear",
            "nearDetailedColor=lerp(nearDetailedColor,thatchColor,0.46*thatchBand",
            "float3 farColor=lerp(body,dryColor,0.12*dryFraction)",
            "farColor=lerp(farColor,thatchColor,0.025*thatchFraction)",
            "float3(0.90,1.12,0.90)",
            "result=lerp(finalLuma.xxx,result,0.94)",
            "sourceMaterialsModified=false",
        ):
            self.assertIn(token, self.editor)
        for token in (
            "ValidateExactR23BVisualDerivativeGrassMaterial",
            "ColorDescriptionOverride",
            "ColorCodeOverride",
            "allowedNodeDeltas=visibility,color",
        ):
            self.assertIn(token, self.ground_h + self.ground)

    def test_editor_boundary_stays_asset_only_and_truth_safe(self):
        for forbidden in (
            "SpawnActor",
            "SaveMap(",
            "LoadMap(",
            "TRIADIstanaExploreV5DHybridEditorLibrary",
            "TRIADIstanaExploreV5DCurrentSurroundings",
        ):
            self.assertNotIn(forbidden, self.editor)
        for token in (
            "mapMutation=false",
            "renderOnly=true",
            "collision=false",
            "navigation=false",
            "sensorAuthority=false",
            "rfAuthority=false",
            "bSourceAssetPackagesModified = false",
            "bBotanicalSurveyOrCurrentSeasonClaimed = false",
            "visualCaptureAccepted=false",
            "captureRevalidationRequired=true",
        ):
            self.assertIn(token, self.actor + self.editor)


if __name__ == "__main__":
    unittest.main()
