from __future__ import annotations

import hashlib
import json
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[4]
SOURCE_ROOT = (
    REPO
    / "unreal"
    / "SourceAssets"
    / "IstanaPublicViewExploreV5D"
    / "Surroundings"
    / "R30FacadeLookdev"
)
CONTRACT = SOURCE_ROOT / "r30_facade_lookdev.contract.json"
README = SOURCE_ROOT / "README.md"
EXPECTED_CONTRACT_BYTES = 8_277
EXPECTED_CONTRACT_SHA256 = (
    "BD11A517E8AC3F6EDBE0F6FE0912409D11180AD4FFAACD91E684391CC2B19F03"
)

EXPECTED_SLOTS = (
    "MI_IPV5D_R29_GlassCool",
    "MI_IPV5D_R29_GlassWarm",
    "MI_IPV5D_R29_GlassNeutral",
    "MI_IPV5D_R29_FrameLight",
    "MI_IPV5D_R29_FrameDark",
    "MI_IPV5D_R29_FrameBronze",
    "MI_IPV5D_R29_SillLight",
    "MI_IPV5D_R29_SillDark",
    "MI_IPV5D_R29_RoofTrim",
    "MI_IPV5D_R29_Canopy",
    "MI_IPV5D_R29_BalconyRail",
)
EXPECTED_TEXTURE_ASSIGNMENTS = {
    "MI_IPV5D_R29_FrameLight": ("Shutter", 0.8),
    "MI_IPV5D_R29_FrameDark": ("Shutter", 0.8),
    "MI_IPV5D_R29_FrameBronze": ("Shutter", 0.8),
    "MI_IPV5D_R29_SillLight": ("Stone", 1.5),
    "MI_IPV5D_R29_SillDark": ("Stone", 1.5),
    "MI_IPV5D_R29_RoofTrim": ("Slate", 1.2),
    "MI_IPV5D_R29_Canopy": ("DarkTimber", 1.0),
    "MI_IPV5D_R29_BalconyRail": ("Shutter", 0.8),
}
GLASS_SLOTS = EXPECTED_SLOTS[:3]


class R30FacadeLookdevContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        for path in (CONTRACT, README):
            if not path.is_file():
                raise AssertionError(f"missing isolated R30 lookdev source: {path}")
        cls.contract = json.loads(CONTRACT.read_text(encoding="utf-8"))

    def test_contract_receipt_is_exactly_pinned(self) -> None:
        payload = CONTRACT.read_bytes()
        self.assertEqual(EXPECTED_CONTRACT_BYTES, len(payload))
        self.assertEqual(
            EXPECTED_CONTRACT_SHA256,
            hashlib.sha256(payload).hexdigest().upper(),
        )

    def test_contract_is_an_isolated_material_only_r29_successor(self) -> None:
        self.assertEqual(
            "triad.istana_explore_v5d.r30_facade_lookdev.v1",
            self.contract["schema"],
        )
        self.assertEqual(
            "/Game/TRIAD/IstanaPublicViewExploreV5D/SurroundingsLookdevR30",
            self.contract["assetRoot"],
        )
        predecessor = self.contract["predecessor"]
        self.assertEqual(
            "TRIADIstanaExploreV5DR29FacadeEnvironment",
            predecessor["actor"],
        )
        self.assertEqual(256_850, predecessor["triangleCount"])
        self.assertEqual(11, predecessor["materialSections"])
        self.assertIn("SurroundingsRealismR29/Meshes/", predecessor["meshObjectPath"])
        for claim in (
            "PUBLIC_REFERENCE_VISUAL_APPROXIMATION",
            "NOT_SURVEY",
            "AS_BUILT",
            "CURRENT_COMPLETE",
            "PHYSICAL_MATERIAL_TRUTH",
        ):
            self.assertIn(claim, self.contract["claimStatus"])

    def test_exact_eleven_semantic_slot_overrides_are_ordered_and_unique(self) -> None:
        overrides = self.contract["overrides"]
        self.assertEqual(11, len(overrides))
        self.assertEqual(EXPECTED_SLOTS, tuple(item["slot"] for item in overrides))
        expected_assets = tuple(slot.replace("R29", "R30") for slot in EXPECTED_SLOTS)
        self.assertEqual(expected_assets, tuple(item["asset"] for item in overrides))
        self.assertEqual(11, len({item["slot"] for item in overrides}))
        self.assertEqual(11, len({item["asset"] for item in overrides}))

    def test_true_texture_switch_is_exactly_eight_textured_and_three_glass(self) -> None:
        overrides = {item["slot"]: item for item in self.contract["overrides"]}
        textured = [item for item in overrides.values() if item["useTextureSet"]]
        procedural = [item for item in overrides.values() if not item["useTextureSet"]]
        self.assertEqual(8, len(textured))
        self.assertEqual(3, len(procedural))
        self.assertEqual(24, sum(3 for _ in textured))
        self.assertEqual(0, sum(3 for item in procedural if item["textureSet"] is not None))
        self.assertEqual(set(EXPECTED_TEXTURE_ASSIGNMENTS), {item["slot"] for item in textured})
        self.assertEqual(set(GLASS_SLOTS), {item["slot"] for item in procedural})

        for slot, (texture_set, metres_per_tile) in EXPECTED_TEXTURE_ASSIGNMENTS.items():
            item = overrides[slot]
            self.assertIs(item["useTextureSet"], True)
            self.assertEqual(texture_set, item["textureSet"])
            self.assertEqual(metres_per_tile, item["metresPerTile"])
            self.assertGreater(item["textureInfluence"], 0.0)
            self.assertGreater(item["normalStrength"], 0.0)

        for slot in GLASS_SLOTS:
            item = overrides[slot]
            self.assertIs(item["useTextureSet"], False)
            self.assertIsNone(item["textureSet"])
            self.assertEqual(0.0, item["metallic"])
            self.assertGreaterEqual(item["roughness"], 0.09)
            self.assertLessEqual(item["roughness"], 0.12)
            for field in (
                "textureInfluence",
                "normalStrength",
                "roughnessTextureWeight",
                "metallicTextureWeight",
                "aoTextureWeight",
            ):
                self.assertEqual(0.0, item[field], f"{slot}.{field}")

    def test_master_exposes_one_explicit_texture_branch_contract(self) -> None:
        master = self.contract["master"]
        self.assertEqual("M_IPV5D_R30_ContextFacadePBR_Master", master["name"])
        self.assertEqual("Surface", master["materialDomain"])
        self.assertEqual("Opaque", master["blendMode"])
        self.assertEqual("DefaultLit", master["shadingModel"])
        self.assertIs(master["nanite"], True)
        self.assertEqual(
            ["BaseColorTexture", "NormalTexture", "PackedORMTexture"],
            master["textureParameters"],
        )
        self.assertEqual(
            [
                "UvScale",
                "TextureInfluence",
                "NormalStrength",
                "Roughness",
                "RoughnessTextureWeight",
                "Metallic",
                "MetallicTextureWeight",
                "Specular",
                "AoTextureWeight",
            ],
            master["scalarParameters"],
        )
        self.assertEqual(["Tint"], master["vectorParameters"])
        self.assertEqual(["UseTextureSet"], master["staticSwitchParameters"])
        self.assertEqual(52, master["expressionCount"])
        self.assertEqual(
            {
                "namedStaticBoolParameters": 1,
                "plainStaticSwitches": 5,
                "duplicateProneStaticSwitchParameterNodes": 0,
                "controlledChannels": [
                    "BaseColor",
                    "Normal",
                    "Roughness",
                    "Metallic",
                    "AmbientOcclusion",
                ],
            },
            master["staticSwitchTopology"],
        )
        self.assertIn("physical metres", master["uvContract"])

    def test_opaque_glass_branch_is_view_dependent_subtle_and_sorting_safe(self) -> None:
        glass = self.contract["master"]["opaqueGlassApproximation"]
        self.assertEqual(
            "UseTextureSet=false for the three glass instances only",
            glass["activation"],
        )
        self.assertEqual("Opaque", glass["blendMode"])
        self.assertIs(glass["translucencyUsed"], False)
        self.assertIs(glass["sortingRiskIntroduced"], False)
        self.assertEqual(
            {
                "model": "Fresnel blend from tinted shallow-interior cue to restrained sky-reflection tint",
                "exponent": 4.0,
                "baseReflectance": 0.12,
                "reflectionStrength": 0.86,
                "reflectionTint": [0.20, 0.34, 0.48, 1.0],
            },
            glass["viewResponse"],
        )
        interior = glass["interiorCue"]
        self.assertEqual(interior["parallaxDepth"], 0.035)
        self.assertEqual(interior["bandPeriodScaledUv"], 0.75)
        self.assertEqual(interior["bandPeriodMetresAtGlassUvScale"], 3.0)
        self.assertEqual(interior["brightnessVariation"], 0.075)
        self.assertEqual(glass["roughnessVariation"]["amplitude"], 0.018)
        acceptance = glass["humanAcceptance"]
        self.assertEqual(["008m", "002m"], acceptance["requiredPlayer0PoseIds"])
        self.assertEqual(3, len(acceptance["008m"]))
        self.assertEqual(3, len(acceptance["002m"]))
        self.assertIs(acceptance["automaticAcceptanceAllowed"], False)
        self.assertIs(acceptance["nativePlayer0CaptureRequired"], True)

        readme = README.read_text(encoding="utf-8")
        for phrase in (
            "view-angle Fresnel",
            "world-to-tangent",
            "0.035 scaled UV units",
            "+/-7.5% interior brightness",
            "+/-0.018 roughness variation",
            "native `008m` and `002m`",
            "flat dark paint",
            "no translucent sorting path",
        ):
            self.assertIn(phrase, readme)

    def test_texture_authority_is_context_only_and_hero_dependencies_are_forbidden(self) -> None:
        self.assertEqual(
            "/Game/TRIAD/IstanaPublicView/Textures",
            self.contract["allowedTextureRoot"],
        )
        forbidden = self.contract["forbiddenDependencyPrefixes"]
        self.assertEqual(4, len(forbidden))
        self.assertEqual(
            {
                "/Game/TRIAD/IstanaPublicView/HeroMaterialsV2",
                "/Game/TRIAD/IstanaPublicView/HeroMaterialsV3",
                "/Game/TRIAD/IstanaPublicView/HeroMaterialsV4",
                "/Game/TRIAD/IstanaPublicView/HeroMaterialsV5",
            },
            set(forbidden),
        )
        encoded_overrides = json.dumps(self.contract["overrides"], sort_keys=True)
        self.assertNotIn("HeroMaterials", encoded_overrides)

    def test_all_topology_geography_and_authority_invariants_fail_closed(self) -> None:
        self.assertEqual(
            {
                "meshPackagesMutated": False,
                "meshTopologyModified": False,
                "meshTransformsModified": False,
                "geographyModified": False,
                "collisionModified": False,
                "navigationModified": False,
                "simulationAuthorityAdded": False,
                "sensorAuthorityAdded": False,
                "rfAuthorityAdded": False,
                "cesiumOrProviderPolicyModified": False,
                "vegetationModified": False,
                "terrainModified": False,
                "visualCaptureAccepted": False,
            },
            self.contract["invariants"],
        )


if __name__ == "__main__":
    unittest.main()
